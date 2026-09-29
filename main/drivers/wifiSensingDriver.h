#pragma once

#include <stdbool.h>

#include "driverStatus.h"

typedef enum
{
    WIFI_SENSING_UNKNOWN = 0,
    WIFI_SENSING_CALIBRATING,
    WIFI_SENSING_EMPTY,
    WIFI_SENSING_PRESENCE,
    WIFI_SENSING_MOTION
} WifiSensingState;

typedef struct
{
    WifiSensingState state;
    bool motion;
    bool presence;
    bool calibrated;
    float jitter;
    float wander;
} WifiSensingMeasurement;

DriverStatus wifiSensingInit(void);
DriverStatus wifiSensingCalibrationStart(void);
DriverStatus wifiSensingCalibrationStop(void);
DriverStatus wifiSensingStart(void);
DriverStatus wifiSensingStop(void);
DriverStatus wifiSensingRead(WifiSensingMeasurement *measurement);
DriverStatus wifiSensingDeinit(void);
