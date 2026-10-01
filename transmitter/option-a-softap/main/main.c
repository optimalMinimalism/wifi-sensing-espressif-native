#include <stdio.h>
#include <string.h>

#include "control_http.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "softap_config.h"

static const char *tag = "SOFTAP";
static esp_netif_t *ap_netif;

static void wifi_event(void *argument, esp_event_base_t base, int32_t id, void *data)
{
    (void)argument;
    (void)base;
    (void)data;
    if (id == WIFI_EVENT_AP_STACONNECTED) {
        ESP_LOGI(tag, "station connected");
        control_http_publish("wifi", "station connected");
    } else if (id == WIFI_EVENT_AP_STADISCONNECTED) {
        ESP_LOGI(tag, "station disconnected");
        control_http_publish("wifi", "station disconnected");
    }
}

static void snapshot(control_http_snapshot_t *status, void *context)
{
    (void)context;
    snprintf(status->wifi, sizeof(status->wifi), "softap");
    esp_netif_ip_info_t ip_info;
    if (esp_netif_get_ip_info(ap_netif, &ip_info) == ESP_OK) {
        esp_ip4addr_ntoa(&ip_info.ip, status->ip, sizeof(status->ip));
    }
    wifi_sta_list_t stations = {0};
    if (esp_wifi_ap_get_sta_list(&stations) == ESP_OK) {
        status->clients = stations.num;
    }
}

void app_main(void)
{
    esp_err_t error = nvs_flash_init();
    if (error == ESP_ERR_NVS_NO_FREE_PAGES || error == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        error = nvs_flash_init();
    }
    ESP_ERROR_CHECK(error);
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ap_netif = esp_netif_create_default_wifi_ap();
    if (ap_netif == NULL) {
        ESP_LOGE(tag, "failed to create AP network interface");
        return;
    }
    wifi_init_config_t init = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&init));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                                wifi_event, NULL));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    wifi_config_t config = {0};
    memcpy(config.ap.ssid, AP_SSID, strlen(AP_SSID));
    config.ap.ssid_len = strlen(AP_SSID);
    memcpy(config.ap.password, AP_PASSWORD, strlen(AP_PASSWORD));
    config.ap.channel = AP_CHANNEL;
    config.ap.authmode = WIFI_AUTH_WPA2_PSK;
    config.ap.max_connection = 4;
    config.ap.beacon_interval = 100;
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &config));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_set_bandwidth(WIFI_IF_AP, WIFI_BW20));

    control_http_config_t http = {
        .role = "softap",
        .token = CONTROL_TOKEN,
        .port = 80,
        .snapshot = snapshot,
    };
    ESP_ERROR_CHECK(control_http_start(&http));
    esp_netif_ip_info_t ip_info;
    ESP_ERROR_CHECK(esp_netif_get_ip_info(ap_netif, &ip_info));
    ESP_LOGI(tag, "SSID %s channel %d IP " IPSTR, AP_SSID, AP_CHANNEL,
             IP2STR(&ip_info.ip));
    control_http_publish("wifi", "SoftAP ready");
}
