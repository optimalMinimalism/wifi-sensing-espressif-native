#include "sensingTask.h"

#include <stdint.h>

#include "config.h"
#include "drivers/wifiSensingDriver.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *tag = "APP";
static TaskHandle_t sensingTaskHandle;
static const int64_t secondsPerUnit[] = {1, 60, 3600, 86400, 604800};
static const char *unitName[] = {"s", "m", "h", "d", "w"};

#if LOGGING_TYPE < 0 || LOGGING_TYPE > 4
#error "LOGGING_TYPE must be between 0 (seconds) and 4 (weeks)"
#endif
#if LOGGING_TIME <= 0
#error "LOGGING_TIME must be positive"
#endif

static const char *stateName(WifiSensingState state)
{
    switch (state) {
    case WIFI_SENSING_CALIBRATING: return "CALIBRATING";
    case WIFI_SENSING_EMPTY: return "EMPTY";
    case WIFI_SENSING_PRESENCE: return "PRESENCE";
    case WIFI_SENSING_MOTION: return "MOTION";
    default: return "UNKNOWN";
    }
}

static void logDiagnostics(const WifiSensingMeasurement *measurement)
{
    ESP_LOGI(
        tag,
        "diag state=%s calibrated=%d presence_ready=%d wander_raw=%.9f presence_avg=%.6f presence_threshold=%.6f applied_threshold=%.6f native_presence=%d",
        stateName(measurement->state), measurement->calibrated, measurement->presenceReady,
        measurement->wander, measurement->presenceWanderAverage,
        measurement->presenceSomeoneThreshold, measurement->appliedPresenceThreshold,
        measurement->nativePresence);
}

static void sensingTaskRun(void *argument)
{
    (void)argument;
    WifiSensingState previousState = WIFI_SENSING_UNKNOWN;
    bool havePreviousState = false;
    bool readFailed = false;
    TickType_t lastWake = xTaskGetTickCount();
    int64_t stateSinceUs = esp_timer_get_time();
    int64_t lastLogUs = stateSinceUs;
    const int64_t unitUs = secondsPerUnit[LOGGING_TYPE] * 1000000LL;
    const int64_t logIntervalUs = (int64_t)LOGGING_TIME * unitUs;

    for (;;) {
        WifiSensingMeasurement measurement;
        DriverStatus status = wifiSensingRead(&measurement);
        if (status == DRIVER_OK) {
            int64_t nowUs = esp_timer_get_time();
            if (!havePreviousState || measurement.state != previousState) {
                if (havePreviousState) {
                    double elapsedUnits = (double)(nowUs - stateSinceUs) / (double)unitUs;
                    ESP_LOGI(tag, "previous state: %s lasted %.2f %s", stateName(previousState),
                             elapsedUnits, unitName[LOGGING_TYPE]);
                }
                ESP_LOGI(tag, "%s", stateName(measurement.state));
                logDiagnostics(&measurement);
                previousState = measurement.state;
                havePreviousState = true;
                stateSinceUs = nowUs;
                lastLogUs = nowUs;
            } else if (nowUs - lastLogUs >= logIntervalUs) {
                logDiagnostics(&measurement);
                lastLogUs = nowUs;
            }
            readFailed = false;
        } else if (!readFailed) {
            ESP_LOGW(tag, "sensing read unavailable (status %d)", status);
            readFailed = true;
            havePreviousState = false;
        }
        vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(SENSING_PERIOD_MS));
    }
}

DriverStatus sensingTaskStart(void)
{
    if (sensingTaskHandle != NULL) {
        return DRIVER_OK;
    }
    BaseType_t result = xTaskCreate(sensingTaskRun, "sensingTask", SENSING_TASK_STACK_SIZE, NULL,
                                    SENSING_TASK_PRIORITY, &sensingTaskHandle);
    return result == pdPASS ? DRIVER_OK : DRIVER_INTERNAL_ERROR;
}
