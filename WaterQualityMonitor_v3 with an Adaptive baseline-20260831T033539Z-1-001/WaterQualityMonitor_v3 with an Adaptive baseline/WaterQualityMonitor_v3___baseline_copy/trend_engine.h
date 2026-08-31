#ifndef TREND_ENGINE_H
#define TREND_ENGINE_H

#include <Arduino.h>

// ==========================================================
// TREND STATUS
// ==========================================================

enum TrendStatus
{
    TREND_DISABLED,
    TREND_COLLECTING,
    TREND_NORMAL,
    TREND_WATCH,
    TREND_NEED_MAINTENANCE,
    TREND_WARNING,
    TREND_ABSOLUTE_ALARM
};

// ==========================================================
// ADAPTIVE BASELINE STATE
// ==========================================================

struct AdaptiveBaseline
{
    bool isActive;
    bool isStable;

    float baselinePH;
    float baselineTDS;
    float baselineNTU;

    float thresholdPHLow;
    float thresholdPHHigh;

    float thresholdTDS;
    float thresholdNTU;

    uint32_t stableStartEpoch;
    uint32_t warningStartEpoch;

    bool warningActive;
    bool warningTriggeredPH;
    bool warningTriggeredTDS;
    bool warningTriggeredNTU;
};

// ==========================================================
// GRAPH DATA
// ==========================================================

struct TrendChartData
{
    float hourlyValues[24];

    uint8_t hourlyCount;

    float average1Hour;
    float average3Hour;
    float average6Hour;
    float average12Hour;
    float average24Hour;
};

// ==========================================================
// TREND RESULT
// ==========================================================

struct TrendResult
{
    TrendStatus status;

    bool maintenanceRequired;
    bool warningActive;
    bool absoluteAlarmActive;

    uint8_t trendPoints;

    uint8_t hourlyRecordCount;
    uint8_t dailyRecordCount;

    float currentTDS;
    float currentPH;
    float currentNTU;

    float currentDayAverage;
    float previousWeekAverage;
    float currentWeekAverage;

    float week0Baseline;
    float baselineWarningGate;
    float eightyPercentWarningGate;
    float absoluteSafetyLimit;

    AdaptiveBaseline adaptiveBaseline;

    String lastSaveDate;
    String lastSaveTime;
};

// ==========================================================
// PUBLIC API
// ==========================================================

void initTrendEngine();
void updateTrendEngine();

TrendResult getTrendResult();
TrendChartData getTrendChartData();

const char *getTrendStatusText();

// ==========================================================
// ADAPTIVE BASELINE CONTROL (CLI)
// ==========================================================

void setAdaptiveBaselinePH(float value);
void setAdaptiveBaselineTDS(float value);
void setAdaptiveBaselineNTU(float value);

void resetAdaptiveBaseline();

#endif