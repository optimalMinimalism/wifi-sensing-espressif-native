#include <inttypes.h>
#include <string.h>
#include <unistd.h>

#include "dedicated_tx_config.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_now.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "nvs_flash.h"

static const char *tag = "CSI_TX";
static const uint8_t broadcastMac[ESP_NOW_ETH_ALEN] = {
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
};
static const uint8_t transmitterMac[ESP_NOW_ETH_ALEN] = TX_MAC_BYTES;

typedef struct {
    uint32_t magic;
    uint32_t sequence;
    int64_t timestampUs;
} __attribute__((packed)) sensing_packet_t;

static void initNvs(void)
{
    esp_err_t error = nvs_flash_init();
    if (error == ESP_ERR_NVS_NO_FREE_PAGES || error == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        error = nvs_flash_init();
    }
    ESP_ERROR_CHECK(error);
}

static void initRadio(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    wifi_init_config_t init = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&init));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));
    ESP_ERROR_CHECK(esp_wifi_set_bandwidth(WIFI_IF_STA, WIFI_BW20));
    ESP_ERROR_CHECK(esp_wifi_set_channel(TX_CHANNEL, WIFI_SECOND_CHAN_NONE));
    ESP_ERROR_CHECK(esp_wifi_set_mac(WIFI_IF_STA, transmitterMac));

    ESP_ERROR_CHECK(esp_now_init());
    esp_now_peer_info_t peer = {
        .channel = TX_CHANNEL,
        .ifidx = WIFI_IF_STA,
        .encrypt = false,
    };
    memcpy(peer.peer_addr, broadcastMac, sizeof(peer.peer_addr));
    ESP_ERROR_CHECK(esp_now_add_peer(&peer));
    esp_now_rate_config_t rate = {
        .phymode = WIFI_PHY_MODE_HT20,
        .rate = WIFI_PHY_RATE_MCS0_LGI,
        .ersu = false,
        .dcm = false,
    };
    ESP_ERROR_CHECK(esp_now_set_peer_rate_config(broadcastMac, &rate));
}

void app_main(void)
{
    initNvs();
    initRadio();

    ESP_LOGI(tag, "dedicated CSI transmitter ready: mac=%s channel=%d bandwidth=HT20 rate=%d Hz",
             TX_MAC_STRING, TX_CHANNEL, TX_FREQUENCY_HZ);

    const useconds_t periodUs = 1000000U / TX_FREQUENCY_HZ;
    uint32_t sent = 0;
    uint32_t failed = 0;
    int64_t lastReportUs = esp_timer_get_time();
    sensing_packet_t packet = {.magic = 0x42534943}; /* "CISB" on the wire. */

    for (;;) {
        packet.timestampUs = esp_timer_get_time();
        esp_err_t error = esp_now_send(broadcastMac, (const uint8_t *)&packet,
                                       sizeof(packet));
        if (error == ESP_OK) {
            sent++;
        } else {
            failed++;
            if (failed == 1) {
                ESP_LOGW(tag, "ESP-NOW queue rejected a packet: %s", esp_err_to_name(error));
            }
        }
        packet.sequence++;

        int64_t nowUs = esp_timer_get_time();
        if (nowUs - lastReportUs >= 1000000) {
            ESP_LOGI(tag, "tx total=%" PRIu32 " accepted=%" PRIu32 " rejected=%" PRIu32,
                     packet.sequence, sent, failed);
            sent = 0;
            failed = 0;
            lastReportUs = nowUs;
        }
        usleep(periodUs);
    }
}
