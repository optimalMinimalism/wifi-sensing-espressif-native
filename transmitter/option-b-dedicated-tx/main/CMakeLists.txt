set(TX_ENV_FILE "${PROJECT_DIR}/.env")
if(NOT EXISTS "${TX_ENV_FILE}")
    message(FATAL_ERROR "Missing transmitter/option-b-dedicated-tx/.env: copy .env.example and set TX_CHANNEL/TX_FREQUENCY_HZ/TX_MAC")
endif()
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${TX_ENV_FILE}")
execute_process(
    COMMAND "${PYTHON}" "${PROJECT_DIR}/tools/generate_config.py"
            "${TX_ENV_FILE}" "${CMAKE_CURRENT_BINARY_DIR}/dedicated_tx_config.h"
    RESULT_VARIABLE TX_CONFIG_RESULT
    ERROR_VARIABLE TX_CONFIG_ERROR
)
if(NOT TX_CONFIG_RESULT EQUAL 0)
    message(FATAL_ERROR "Cannot generate dedicated transmitter settings: ${TX_CONFIG_ERROR}")
endif()

idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "." "${CMAKE_CURRENT_BINARY_DIR}"
    REQUIRES esp_wifi esp_event esp_netif esp_timer nvs_flash
)
