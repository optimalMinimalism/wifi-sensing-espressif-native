#pragma once

/* Wi-Fi credentials are generated from the local .env during the build. */
#include "wifi_credentials.h"
#define WIFI_CONNECT_TIMEOUT_MS 30000

#define WIFI_TASK_PRIORITY      5
#define WIFI_TASK_STACK_SIZE    4096

#define SENSING_TASK_PRIORITY   6      
#define SENSING_TASK_STACK_SIZE 4096  
#define SENSING_PERIOD_MS       500   // How often to read the sensors
#define SENSING_CALIBRATION_MS  15000 // Calibration time for the sensors
#define SENSING_CALIBRATION_MAX_MS 60000 // Fail if training has no usable data by this time

//provisory logging settings, can be changed in the future
#define LOGGING_TIME 1 // How often to log the sensors
#define LOGGING_TYPE 0 // 0 = seconds, 1 = minutes, 2 = hours, 3 = days, 4 = weeks