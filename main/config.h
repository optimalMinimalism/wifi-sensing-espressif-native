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

#define LOGGING_TIME 1 // How often to log the sensors
#define LOGGING_TYPE "sec" // "sec" , "min", "hour", "day" or "week" for logging every x seconds, minutes, hours, days or weeks. 
                            