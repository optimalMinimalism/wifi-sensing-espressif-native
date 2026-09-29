#pragma once

/* Set these to the credentials of a 2.4 GHz access point before flashing. */
#define WIFI_SSID               "name-of-your-access-point"
#define WIFI_PASSWORD           "pass"
#define WIFI_CONNECT_TIMEOUT_MS 30000

#define WIFI_TASK_PRIORITY      5
#define WIFI_TASK_STACK_SIZE    4096

#define SENSING_TASK_PRIORITY   6
#define SENSING_TASK_STACK_SIZE 4096
#define SENSING_PERIOD_MS       500
#define SENSING_CALIBRATION_MS  15000
