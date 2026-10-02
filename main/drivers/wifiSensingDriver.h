#pragma once

#include <stdbool.h>
#include <stdint.h>

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
    bool presenceReady;
    float jitter;
    float wander;
    float presenceWanderAverage;
    float presenceSomeoneThreshold;
    float appliedPresenceThreshold;
    bool nativePresence;
    uint32_t motionSmoothScaled;
    uint32_t motionEnterLevelScaled;
    uint32_t motionExitLevelScaled;
    int motionProcessState;
    int motionInitStage;
    float trainWanderThreshold;
    float trainJitterThreshold;
    bool trainThresholdsValid;
    int trainStatus;
    int trainLastAction;
    uint32_t trainSampleCount;
    uint32_t trainBackgroundCount;
    float trainBackgroundAverage;
    float trainLastBasisWander;
    bool apInfoValid;
    int apRssi;
    int apChannel;
} WifiSensingMeasurement;

typedef struct
{
    uint32_t sampleCount;
    uint32_t backgroundCount;
    int trainStatus;
    int lastAction;
    float backgroundAverage;
    float lastBasisWander;
    float wander;
    float jitter;
} WifiSensingCalibrationProgress;

DriverStatus wifiSensingInit(void);
DriverStatus wifiSensingCalibrationStart(void);
DriverStatus wifiSensingCalibrationGetProgress(WifiSensingCalibrationProgress *progress);
DriverStatus wifiSensingCalibrationStop(void);
DriverStatus wifiSensingStart(void);
DriverStatus wifiSensingStop(void);
DriverStatus wifiSensingRead(WifiSensingMeasurement *measurement);
DriverStatus wifiSensingDeinit(void);
