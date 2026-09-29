#include "wifiTask.h"

#include "drivers/wifiDriver.h"

DriverStatus wifiTaskStart(void)
{
    DriverStatus status = wifiInit();
    return status == DRIVER_OK ? wifiConnect() : status;
}
