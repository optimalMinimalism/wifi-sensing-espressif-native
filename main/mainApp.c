#include "config.h"
#include "drivers/wifiSensingDriver.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sensingTask.h"
#include "wifiTask.h"

static const char *tag = "APP";

void app_main(void)
{
    DriverStatus status = wifiTaskStart();
    if (status != DRIVER_OK) {
        ESP_LOGE(tag, "Wi-Fi startup failed (status %d)", status);
        return;
    }
    status = wifiSensingInit();
    if (status != DRIVER_OK) {
        ESP_LOGE(tag, "sensing initialization failed (status %d)", status);
        return;
    }

    ESP_LOGI(tag, "Keep monitored area empty");
    status = wifiSensingCalibrationStart();
    if (status != DRIVER_OK) {
        ESP_LOGE(tag, "calibration start failed (status %d)", status);
        return;
    }
    vTaskDelay(pdMS_TO_TICKS(SENSING_CALIBRATION_MS));
    status = wifiSensingCalibrationStop();
    if (status != DRIVER_OK) {
        ESP_LOGE(tag, "calibration stop failed (status %d)", status);
        return;
    }
    status = wifiSensingStart();
    if (status != DRIVER_OK) {
        ESP_LOGE(tag, "sensing start failed (status %d)", status);
        return;
    }
    status = sensingTaskStart();
    if (status != DRIVER_OK) {
        ESP_LOGE(tag, "sensing task start failed (status %d)", status);
        wifiSensingStop();
    }
}
