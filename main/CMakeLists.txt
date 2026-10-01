if(NOT WIFI_ENV_FILE)
    set(WIFI_ENV_FILE "${PROJECT_DIR}/.env")
endif()
if(NOT EXISTS "${WIFI_ENV_FILE}")
    message(FATAL_ERROR "Missing .env: copy .env.example to .env and set WIFI_SSID/WIFI_PASSWORD")
endif()
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${WIFI_ENV_FILE}")
execute_process(
    COMMAND "${PYTHON}" "${PROJECT_DIR}/tools/generate_wifi_credentials.py"
            "${WIFI_ENV_FILE}" "${CMAKE_CURRENT_BINARY_DIR}/wifi_credentials.h"
    RESULT_VARIABLE WIFI_CREDENTIALS_RESULT
    ERROR_VARIABLE WIFI_CREDENTIALS_ERROR
)
if(NOT WIFI_CREDENTIALS_RESULT EQUAL 0)
    message(FATAL_ERROR "Cannot generate Wi-Fi credentials: ${WIFI_CREDENTIALS_ERROR}")
endif()

idf_component_register(
    SRCS
        "mainApp.c"
        "drivers/wifiDriver.c"
        "wifiTask.c"
        "sensingTask.c"
        "drivers/wifiSensingDriver.c"
    INCLUDE_DIRS "." "${CMAKE_CURRENT_BINARY_DIR}"
    REQUIRES esp_wifi esp_event esp_netif esp_timer nvs_flash esp_wifi_sensing
)
