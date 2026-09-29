idf_component_register(
    SRCS
        "mainApp.c"
        "drivers/wifiDriver.c"
        "wifiTask.c"
        "sensingTask.c"
        "drivers/wifiSensingDriver.c"
    INCLUDE_DIRS "."
    REQUIRES esp_wifi esp_event esp_netif nvs_flash esp_wifi_sensing
)
