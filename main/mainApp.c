#include "config.h"
#include "drivers/wifiSensingDriver.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sensingTask.h"
#include "wifiTask.h"

static const char *tag = "APP";

#if SENSING_CALIBRATION_MAX_MS < SENSING_CALIBRATION_MS
#error "SENSING_CALIBRATION_MAX_MS must be at least SENSING_CALIBRATION_MS"
#endif

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
    TickType_t calibrationStarted = xTaskGetTickCount();
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        WifiSensingCalibrationProgress progress;
        status = wifiSensingCalibrationGetProgress(&progress);
        if (status != DRIVER_OK) {
            ESP_LOGE(tag, "calibration progress unavailable (status %d)", status);
            wifiSensingDeinit();
            return;
        }

        TickType_t elapsed = xTaskGetTickCount() - calibrationStarted;
        ESP_LOGI(tag,
                 "calibration elapsed_ms=%u samples=%u background=%u status=%d action=%d background_avg=%.9f basis_wander=%.9f wander_raw=%.9f jitter_raw=%.9f",
                 (unsigned)pdTICKS_TO_MS(elapsed), (unsigned)progress.sampleCount,
                 (unsigned)progress.backgroundCount, progress.trainStatus, progress.lastAction,
                 progress.backgroundAverage, progress.lastBasisWander, progress.wander,
                 progress.jitter);
        if (elapsed >= pdMS_TO_TICKS(SENSING_CALIBRATION_MS) && progress.sampleCount > 0 &&
            progress.backgroundCount > 0) {
            ESP_LOGI(tag, "calibration data ready: samples %u, background %u",
                     (unsigned)progress.sampleCount, (unsigned)progress.backgroundCount);
            break;
        }
        if (elapsed >= pdMS_TO_TICKS(SENSING_CALIBRATION_MS)) {
            ESP_LOGW(tag, "calibration waiting: samples %u, background %u, status %d, action %d",
                     (unsigned)progress.sampleCount, (unsigned)progress.backgroundCount,
                     progress.trainStatus, progress.lastAction);
        }
        if (elapsed >= pdMS_TO_TICKS(SENSING_CALIBRATION_MAX_MS)) {
            ESP_LOGE(tag, "calibration timed out without usable training data");
            wifiSensingDeinit();
            return;
        }
    }
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
