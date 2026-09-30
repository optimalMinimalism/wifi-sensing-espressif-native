#include "wifiDriver.h"

#include <string.h>

#include "config.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "nvs_flash.h"

#define WIFI_CONNECTED_BIT BIT0

static const char *tag = "WIFI";
static EventGroupHandle_t connectionEvents;
static bool initialized;
static bool started;
static bool reconnectEnabled;

static const char *disconnectReasonName(uint8_t reason)
{
    switch (reason) {
    case WIFI_REASON_NO_AP_FOUND: return "network not found (check SSID and 2.4 GHz AP)";
    case WIFI_REASON_AUTH_FAIL: return "authentication failed (check password)";
    case WIFI_REASON_ASSOC_FAIL: return "association failed";
    case WIFI_REASON_HANDSHAKE_TIMEOUT:
    case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT: return "security handshake timed out";
    case WIFI_REASON_AUTH_EXPIRE: return "authentication timed out";
    default: return "see ESP-IDF Wi-Fi disconnect reason code";
    }
}

static DriverStatus statusFromEspErr(esp_err_t error)
{
    switch (error) {
    case ESP_OK: return DRIVER_OK;
    case ESP_ERR_INVALID_ARG: return DRIVER_INVALID_ARGUMENT;
    case ESP_ERR_INVALID_STATE: return DRIVER_NOT_READY;
    case ESP_ERR_TIMEOUT: return DRIVER_TIMEOUT;
    default: return DRIVER_COMMUNICATION_ERROR;
    }
}

static void wifiEventHandler(void *argument, esp_event_base_t eventBase,
                             int32_t eventId, void *eventData)
{
    (void)argument;

    if (eventBase == WIFI_EVENT && eventId == WIFI_EVENT_STA_START) {
        if (reconnectEnabled) {
            esp_err_t error = esp_wifi_connect();
            if (error != ESP_OK) {
                ESP_LOGE(tag, "initial connect failed: %s", esp_err_to_name(error));
            }
        }
    } else if (eventBase == WIFI_EVENT && eventId == WIFI_EVENT_STA_DISCONNECTED) {
        xEventGroupClearBits(connectionEvents, WIFI_CONNECTED_BIT);
        const wifi_event_sta_disconnected_t *disconnected = eventData;
        if (disconnected != NULL) {
            ESP_LOGW(tag, "disconnected: %s (reason %u, RSSI %d dBm)",
                     disconnectReasonName(disconnected->reason),
                     disconnected->reason, disconnected->rssi);
        } else {
            ESP_LOGW(tag, "disconnected (reason unavailable)");
        }
        if (reconnectEnabled) {
            esp_err_t error = esp_wifi_connect();
            if (error != ESP_OK) {
                ESP_LOGE(tag, "reconnect failed: %s", esp_err_to_name(error));
            }
        }
    } else if (eventBase == IP_EVENT && eventId == IP_EVENT_STA_GOT_IP) {
        xEventGroupSetBits(connectionEvents, WIFI_CONNECTED_BIT);
        ESP_LOGI(tag, "connected");
    }
}

DriverStatus wifiInit(void)
{
    if (initialized) {
        return DRIVER_OK;
    }
    if (strlen(WIFI_SSID) == 0 || strlen(WIFI_SSID) > 32 ||
        strlen(WIFI_PASSWORD) > 64 ||
        strcmp(WIFI_SSID, "YOUR_WIFI_SSID") == 0 ||
        strcmp(WIFI_PASSWORD, "YOUR_WIFI_PASSWORD") == 0) {
        ESP_LOGE(tag, "set valid WIFI_SSID and WIFI_PASSWORD in .env (use <value> delimiters)");
        return DRIVER_INVALID_ARGUMENT;
    }

    esp_err_t error = nvs_flash_init();
    if (error == ESP_ERR_NVS_NO_FREE_PAGES || error == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        error = nvs_flash_erase();
        if (error == ESP_OK) {
            error = nvs_flash_init();
        }
    }
    if (error != ESP_OK) {
        ESP_LOGE(tag, "NVS init failed: %s", esp_err_to_name(error));
        return statusFromEspErr(error);
    }

    error = esp_netif_init();
    if (error != ESP_OK && error != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(tag, "netif init failed: %s", esp_err_to_name(error));
        return statusFromEspErr(error);
    }
    error = esp_event_loop_create_default();
    if (error != ESP_OK && error != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(tag, "event loop init failed: %s", esp_err_to_name(error));
        return statusFromEspErr(error);
    }
    if (esp_netif_create_default_wifi_sta() == NULL) {
        ESP_LOGE(tag, "STA netif creation failed");
        return DRIVER_INTERNAL_ERROR;
    }

    connectionEvents = xEventGroupCreate();
    if (connectionEvents == NULL) {
        return DRIVER_INTERNAL_ERROR;
    }
    wifi_init_config_t initConfig = WIFI_INIT_CONFIG_DEFAULT();
    error = esp_wifi_init(&initConfig);
    if (error == ESP_OK) {
        error = esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                           wifiEventHandler, NULL);
    }
    if (error == ESP_OK) {
        error = esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                           wifiEventHandler, NULL);
    }
    if (error == ESP_OK) {
        error = esp_wifi_set_storage(WIFI_STORAGE_RAM);
    }
    if (error == ESP_OK) {
        error = esp_wifi_set_mode(WIFI_MODE_STA);
    }
    if (error == ESP_OK) {
        wifi_config_t wifiConfig = {0};
        memcpy(wifiConfig.sta.ssid, WIFI_SSID, strlen(WIFI_SSID));
        memcpy(wifiConfig.sta.password, WIFI_PASSWORD, strlen(WIFI_PASSWORD));
        error = esp_wifi_set_config(WIFI_IF_STA, &wifiConfig);
    }
    if (error == ESP_OK) {
        error = esp_wifi_set_ps(WIFI_PS_NONE);
    }
    if (error != ESP_OK) {
        ESP_LOGE(tag, "Wi-Fi init failed: %s", esp_err_to_name(error));
        return statusFromEspErr(error);
    }

    initialized = true;
    return DRIVER_OK;
}

DriverStatus wifiConnect(void)
{
    if (!initialized) {
        return DRIVER_NOT_INITIALIZED;
    }
    if (wifiIsConnected()) {
        return DRIVER_OK;
    }

    reconnectEnabled = true;
    esp_err_t error;
    if (!started) {
        error = esp_wifi_start();
        if (error == ESP_OK) {
            started = true;
        }
    } else {
        error = esp_wifi_connect();
    }
    if (error != ESP_OK) {
        ESP_LOGE(tag, "Wi-Fi start/connect failed: %s", esp_err_to_name(error));
        return statusFromEspErr(error);
    }

    EventBits_t bits = xEventGroupWaitBits(connectionEvents, WIFI_CONNECTED_BIT,
                                           pdFALSE, pdTRUE,
                                           pdMS_TO_TICKS(WIFI_CONNECT_TIMEOUT_MS));
    if (bits & WIFI_CONNECTED_BIT) {
        return DRIVER_OK;
    }
    reconnectEnabled = false;
    ESP_LOGE(tag, "connection timed out after %d ms", WIFI_CONNECT_TIMEOUT_MS);
    if (esp_wifi_stop() == ESP_OK) {
        started = false;
    }
    return DRIVER_TIMEOUT;
}

DriverStatus wifiDisconnect(void)
{
    if (!initialized) {
        return DRIVER_NOT_INITIALIZED;
    }
    reconnectEnabled = false;
    xEventGroupClearBits(connectionEvents, WIFI_CONNECTED_BIT);
    if (!started) {
        return DRIVER_OK;
    }
    return statusFromEspErr(esp_wifi_disconnect());
}

bool wifiIsConnected(void)
{
    return connectionEvents != NULL &&
           (xEventGroupGetBits(connectionEvents) & WIFI_CONNECTED_BIT) != 0;
}

DriverStatus wifiGetBssid(uint8_t bssid[6])
{
    if (bssid == NULL) {
        return DRIVER_INVALID_ARGUMENT;
    }
    if (!initialized) {
        return DRIVER_NOT_INITIALIZED;
    }
    if (!wifiIsConnected()) {
        return DRIVER_NOT_READY;
    }
    wifi_ap_record_t accessPoint = {0};
    esp_err_t error = esp_wifi_sta_get_ap_info(&accessPoint);
    if (error != ESP_OK) {
        return statusFromEspErr(error);
    }
    memcpy(bssid, accessPoint.bssid, 6);
    return DRIVER_OK;
}
