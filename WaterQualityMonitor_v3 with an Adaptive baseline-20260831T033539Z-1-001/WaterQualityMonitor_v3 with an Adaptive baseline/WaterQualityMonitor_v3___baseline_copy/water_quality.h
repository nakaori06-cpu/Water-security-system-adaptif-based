#ifndef WATER_QUALITY_H
#define WATER_QUALITY_H

#include <Arduino.h>

// ==========================================================
// WATER STATUS
// ==========================================================

enum WaterStatus
{
    WATER_WARMUP,
    EXCELLENT,
    GOOD,
    FAIR,
    POOR,
    UNSAFE
};

// ==========================================================
// WATER RISK
// ==========================================================

enum WaterRisk
{
    RISK_UNKNOWN,
    SAFE,
    WARNING,
    DANGER
};

// ==========================================================
// WATER QUALITY RESULT
// ==========================================================

struct WaterQuality
{
    float score;
    float confidence;

    WaterStatus status;
    WaterRisk risk;

    bool valid;
    bool relayEnable;
    bool buzzerEnable;
    bool alarm;
    bool filterMaintenance;

    bool safeForBathing;
    bool safeForLaundry;
    bool safeForCleaning;
    bool safeForDrinking;

    bool phValid;
    bool tdsValid;
    bool ntuValid;
    bool temperatureValid;

    String statusText;
    String riskText;
};

// ==========================================================
// PUBLIC API
// ==========================================================

void initWaterQuality();
void updateWaterQuality();

WaterQuality getWaterQuality();

const char *getWaterStatusText();
const char *getWaterRiskText();

#endif