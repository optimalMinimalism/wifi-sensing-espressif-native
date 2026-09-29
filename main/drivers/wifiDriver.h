#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "drivers/driverStatus.h"

DriverStatus wifiInit(void);
DriverStatus wifiConnect(void);
DriverStatus wifiDisconnect(void);
bool wifiIsConnected(void);
DriverStatus wifiGetBssid(uint8_t bssid[6]);
