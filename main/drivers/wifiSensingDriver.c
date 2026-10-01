#include "wifiSensingDriver.h"

#include <string.h>

#include "config.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_wifi_sensing.h"
#include "wifiDriver.h"

static const char *tag = "SENSING";
static esp_wifi_sensing_fsm_handle_t sensingHandle;
static uint8_t peerMac[6];
static bool calibrating;
static bool calibrated;
static bool running;
static bool pingRunning;

static DriverStatus statusFromEspErr(esp_err_t error)
{
    switch (error) {
    case ESP_OK: return DRIVER_OK;
    case ESP_ERR_INVALID_ARG: return DRIVER_INVALID_ARGUMENT;
    case ESP_ERR_INVALID_STATE: return DRIVER_NOT_READY;
    case ESP_ERR_NOT_FOUND:
    case ESP_ERR_NOT_SUPPORTED: return DRIVER_INVALID_DATA;
    case ESP_ERR_TIMEOUT: return DRIVER_TIMEOUT;
    case ESP_ERR_NO_MEM: return DRIVER_INTERNAL_ERROR;
    default: return DRIVER_COMMUNICATION_ERROR;
    }
}

static DriverStatus checkPeer(void)
{
#if SENSING_USE_DEDICATED_TX
    return wifiIsConnected() ? DRIVER_OK : DRIVER_NOT_READY;
#else
    uint8_t currentBssid[6];
    DriverStatus status = wifiGetBssid(currentBssid);
    if (status != DRIVER_OK) {
        return status;
    }
    return memcmp(currentBssid, peerMac, sizeof(peerMac)) == 0
        ? DRIVER_OK : DRIVER_INVALID_DATA;
#endif
}

DriverStatus wifiSensingInit(void)
{
    if (sensingHandle != NULL) {
        return DRIVER_OK;
    }
    DriverStatus status;
#if SENSING_USE_DEDICATED_TX
    const uint8_t configuredPeer[6] = SENSING_PEER_MAC_BYTES;
    memcpy(peerMac, configuredPeer, sizeof(peerMac));
    status = wifiIsConnected() ? DRIVER_OK : DRIVER_NOT_READY;
#else
    status = wifiGetBssid(peerMac);
#endif
    if (status != DRIVER_OK) {
        return status;
    }

    esp_wifi_sensing_fsm_config_t config = DEFAULT_ESP_WIFI_SENSING_FSM_CONFIG();
    config.max_channel_num = 1;
    esp_err_t error = esp_wifi_sensing_fsm_create(&config, &sensingHandle);
    if (error == ESP_OK) {
        error = esp_wifi_sensing_fsm_add_channel(sensingHandle, peerMac);
    }
#if !SENSING_USE_DEDICATED_TX
    if (error == ESP_OK) {
        /* Training also needs a steady stream of CSI samples. */
        error = esp_wifi_sensing_fsm_ping_router_start(sensingHandle);
        if (error == ESP_OK) {
            pingRunning = true;
        }
    }
#endif
    if (error != ESP_OK) {
        ESP_LOGE(tag, "initialization failed: %s", esp_err_to_name(error));
        if (sensingHandle != NULL) {
            esp_wifi_sensing_fsm_delete(sensingHandle);
            sensingHandle = NULL;
        }
        return statusFromEspErr(error);
    }
#if SENSING_USE_DEDICATED_TX
    ESP_LOGI(tag, "using dedicated transmitter " MACSTR " as sensing peer; router ping disabled",
             MAC2STR(peerMac));
#else
    ESP_LOGI(tag, "using connected AP BSSID " MACSTR " as sensing peer", MAC2STR(peerMac));
#endif
    return DRIVER_OK;
}

DriverStatus wifiSensingCalibrationStart(void)
{
    if (sensingHandle == NULL) {
        return DRIVER_NOT_INITIALIZED;
    }
    if (running || calibrating) {
        return DRIVER_NOT_READY;
    }
    DriverStatus status = checkPeer();
    if (status != DRIVER_OK) {
        return status;
    }
    esp_err_t error = esp_wifi_sensing_fsm_train_start(sensingHandle, peerMac);
    if (error != ESP_OK) {
        ESP_LOGE(tag, "calibration start failed: %s", esp_err_to_name(error));
        return statusFromEspErr(error);
    }
    calibrating = true;
    calibrated = false;
    ESP_LOGI(tag, "calibration started");
    return DRIVER_OK;
}

DriverStatus wifiSensingCalibrationGetProgress(WifiSensingCalibrationProgress *progress)
{
    if (progress == NULL) {
        return DRIVER_INVALID_ARGUMENT;
    }
    if (sensingHandle == NULL) {
        return DRIVER_NOT_INITIALIZED;
    }
    if (!calibrating) {
        return DRIVER_NOT_READY;
    }
    DriverStatus status = checkPeer();
    if (status != DRIVER_OK) {
        return status;
    }

    esp_wifi_sensing_fsm_channel_diag_t diagnostic = {0};
    esp_err_t error = esp_wifi_sensing_fsm_get_channel_diag(sensingHandle, peerMac, &diagnostic);
    if (error != ESP_OK) {
        return statusFromEspErr(error);
    }
    progress->sampleCount = diagnostic.train_sample_count;
    progress->backgroundCount = diagnostic.train_background_count;
    progress->trainStatus = diagnostic.train_status;
    progress->lastAction = diagnostic.train_last_action;
    return DRIVER_OK;
}

DriverStatus wifiSensingCalibrationStop(void)
{
    if (sensingHandle == NULL) {
        return DRIVER_NOT_INITIALIZED;
    }
    if (!calibrating) {
        return DRIVER_NOT_READY;
    }
    float wanderThreshold;
    float jitterThreshold;
    esp_err_t error = esp_wifi_sensing_fsm_train_stop(sensingHandle, peerMac,
                                                       &wanderThreshold, &jitterThreshold);
    calibrating = false;
    if (error != ESP_OK) {
        ESP_LOGE(tag, "calibration failed: %s", esp_err_to_name(error));
        return statusFromEspErr(error);
    }
    calibrated = true;
    ESP_LOGI(tag, "calibration completed (wander %.3f, jitter %.3f)",
             wanderThreshold, jitterThreshold);
    return DRIVER_OK;
}

DriverStatus wifiSensingStart(void)
{
    if (sensingHandle == NULL) {
        return DRIVER_NOT_INITIALIZED;
    }
    if (!calibrated || calibrating) {
        return DRIVER_NOT_READY;
    }
    if (running) {
        return DRIVER_OK;
    }
    DriverStatus status = checkPeer();
    if (status != DRIVER_OK) {
        return status;
    }
    esp_err_t error = esp_wifi_sensing_fsm_control(sensingHandle,
                                                     ESP_WIFI_SENSING_FSM_CTRL_START,
                                                     NULL);
    if (error != ESP_OK) {
        ESP_LOGE(tag, "start failed: %s", esp_err_to_name(error));
        return statusFromEspErr(error);
    }
    running = true;
    ESP_LOGI(tag, "sensing started");
    return DRIVER_OK;
}

DriverStatus wifiSensingStop(void)
{
    if (sensingHandle == NULL) {
        return DRIVER_NOT_INITIALIZED;
    }
    if (!running) {
        return DRIVER_OK;
    }
    esp_err_t error = esp_wifi_sensing_fsm_control(sensingHandle,
                                                     ESP_WIFI_SENSING_FSM_CTRL_STOP,
                                                     NULL);
    if (error == ESP_OK) {
        running = false;
    }
    return statusFromEspErr(error);
}

DriverStatus wifiSensingRead(WifiSensingMeasurement *measurement)
{
    if (measurement == NULL) {
        return DRIVER_INVALID_ARGUMENT;
    }
    if (sensingHandle == NULL) {
        return DRIVER_NOT_INITIALIZED;
    }
    if (!running) {
        return DRIVER_NOT_READY;
    }
    DriverStatus status = checkPeer();
    if (status != DRIVER_OK) {
        return status;
    }

    esp_wifi_sensing_fsm_channel_diag_t diagnostic = {0};
    esp_wifi_sensing_fsm_state_t channelState;
    esp_err_t error = esp_wifi_sensing_fsm_get_channel_diag(sensingHandle,
                                                              peerMac, &diagnostic);
    if (error == ESP_OK) {
        error = esp_wifi_sensing_fsm_get_state(sensingHandle,
                                                peerMac, &channelState);
    }
    if (error != ESP_OK) {
        return statusFromEspErr(error);
    }

    measurement->calibrated = calibrated && diagnostic.train_thresholds_valid;
    measurement->jitter = diagnostic.jitter_value;
    measurement->wander = diagnostic.wander_value;
    measurement->presenceReady = diagnostic.presence_ready;
    measurement->presenceWanderAverage = diagnostic.presence_wander_average;
    measurement->presenceSomeoneThreshold = diagnostic.presence_someone_threshold;
    measurement->motion = channelState == ESP_WIFI_SENSING_FSM_STATE_ACTIVE;
    measurement->nativePresence = diagnostic.presence_ready &&
                                  diagnostic.presence_someone_status;
    measurement->appliedPresenceThreshold =
        diagnostic.train_wander_threshold * SENSING_PRESENCE_THRESHOLD_MULTIPLIER;
    measurement->presence = diagnostic.presence_ready && diagnostic.train_thresholds_valid &&
                            measurement->appliedPresenceThreshold > 0.0f &&
                            diagnostic.presence_wander_average > measurement->appliedPresenceThreshold;
    if (!measurement->calibrated ||
        diagnostic.init_stage != ESP_WIFI_SENSING_FSM_INIT_STAGE_STABLE) {
        measurement->state = WIFI_SENSING_UNKNOWN;
    } else if (measurement->motion) {
        measurement->state = WIFI_SENSING_MOTION;
    } else if (measurement->presence) {
        measurement->state = WIFI_SENSING_PRESENCE;
    } else {
        measurement->state = WIFI_SENSING_EMPTY;
    }
    return DRIVER_OK;
}

DriverStatus wifiSensingDeinit(void)
{
    if (sensingHandle == NULL) {
        return DRIVER_NOT_INITIALIZED;
    }
    DriverStatus status = wifiSensingStop();
    if (status != DRIVER_OK) {
        return status;
    }
    if (pingRunning) {
        esp_err_t error = esp_wifi_sensing_fsm_ping_router_stop(sensingHandle);
        if (error != ESP_OK) {
            return statusFromEspErr(error);
        }
        pingRunning = false;
    }
    esp_err_t error = esp_wifi_sensing_fsm_delete(sensingHandle);
    if (error != ESP_OK) {
        return statusFromEspErr(error);
    }
    sensingHandle = NULL;
    calibrating = false;
    calibrated = false;
    memset(peerMac, 0, sizeof(peerMac));
    return DRIVER_OK;
}
