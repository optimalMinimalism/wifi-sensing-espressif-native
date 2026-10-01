set(AP_ENV_FILE "${PROJECT_DIR}/.env")
if(NOT EXISTS "${AP_ENV_FILE}")
    message(FATAL_ERROR "Missing transmitter/option-a-softap/.env: copy .env.example and set AP_SSID/AP_PASSWORD/CONTROL_TOKEN/AP_CHANNEL")
endif()
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${AP_ENV_FILE}")
execute_process(
    COMMAND "${PYTHON}" "${PROJECT_DIR}/tools/generate_config.py"
            "${AP_ENV_FILE}" "${CMAKE_CURRENT_BINARY_DIR}/softap_config.h"
    RESULT_VARIABLE AP_CONFIG_RESULT
    ERROR_VARIABLE AP_CONFIG_ERROR
)
if(NOT AP_CONFIG_RESULT EQUAL 0)
    message(FATAL_ERROR "Cannot generate SoftAP settings: ${AP_CONFIG_ERROR}")
endif()

idf_component_register(
    SRCS "main.c" "control_http.c"
    INCLUDE_DIRS "." "${CMAKE_CURRENT_BINARY_DIR}"
    EMBED_TXTFILES "index.html"
    REQUIRES esp_http_server esp_timer esp_wifi esp_event esp_netif nvs_flash
)
