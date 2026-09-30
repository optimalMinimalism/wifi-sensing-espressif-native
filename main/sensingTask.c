#include "sensingTask.h"

#include "config.h"
#include "drivers/wifiSensingDriver.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *tag = "APP";
static TaskHandle_t sensingTaskHandle;
static int timeWithoutChangeMS = 0;
static int timeWithoutChangeSEC = 0;

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

static void sensingTaskRun(void *argument)
{
    (void)argument;
    WifiSensingState previousState = WIFI_SENSING_UNKNOWN;
    bool havePreviousState = false;
    bool readFailed = false;
    TickType_t lastWake = xTaskGetTickCount();

    for (;;) {
        WifiSensingMeasurement measurement;
        DriverStatus status = wifiSensingRead(&measurement);
        if (status == DRIVER_OK) {
            if (!havePreviousState || measurement.state != previousState) {
                ESP_LOGI(tag, "%s", stateName(measurement.state));
                previousState = measurement.state;
                havePreviousState = true;
            } else {
                timeWithoutChangeMS += SENSING_PERIOD_MS;
                if (timeWithoutChange >= 1000) {
                    timeWithoutChangeSEC++;
                    ESP_LOGI(tag, "no change for %d seconds", timeWithoutChangeSEC);
                    timeWithoutChangeMS = 0;
                }

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
    BaseType_t result = xTaskCreate(sensingTaskRun, "sensingTask",
                                     SENSING_TASK_STACK_SIZE, NULL,
                                     SENSING_TASK_PRIORITY, &sensingTaskHandle);
    return result == pdPASS ? DRIVER_OK : DRIVER_INTERNAL_ERROR;
}
