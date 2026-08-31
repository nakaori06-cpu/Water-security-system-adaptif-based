#include "trend_engine.h"

#include <SD.h>
#include <RTClib.h>

#include "config.h"
#include "rtc_manager.h"
#include "sd_logger.h"
#include "sensors.h"

// Forward declarations
static void updateChartData();
static void evaluateTrend();
static void updateAdaptiveBaseline();
static void resetTrendResult();

// File names
static const char *HOURLY_FILE = "/trend_hourly.csv";
static const char *DAILY_FILE = "/trend_daily.csv";
static const char *WEEKLY_FILE = "/trend_weekly.csv";
static const char *STATE_FILE = "/trend_state.csv";

// Data structures
struct HourRecord { uint32_t startEpoch; float averageTDS; };
struct DayRecord { uint32_t startEpoch; float averageTDS; };
struct WeekRecord { uint32_t startEpoch; uint32_t endEpoch; float averageTDS; };
struct TrendState { uint8_t points; float previousWeekAverage; bool hasRealWeek; uint32_t lastSaveEpoch; };

// Memory
static HourRecord hourlyRecords[TREND_HOURS_PER_DAY];
static uint8_t hourlyCount = 0;
static DayRecord dailyRecords[TREND_DAYS_PER_WEEK];
static uint8_t dailyCount = 0;
static TrendState trendState;
static TrendResult trendResult;
static TrendChartData chartData;

// Current hour
static uint32_t currentHourStartEpoch = 0;
static float currentHourTDSSum = 0.0f;
static uint32_t currentHourSampleCount = 0;
static unsigned long lastSampleMs = 0;
static bool trendReady = false;

// CSV helpers
static String getCSVField(const String &line, uint8_t fieldIndex) {
    uint8_t currentField = 0;
    int fieldStart = 0;
    for (int i = 0; i <= line.length(); i++) {
        const bool fieldEnded = (i == line.length()) || (line.charAt(i) == ',');
        if (fieldEnded) {
            if (currentField == fieldIndex) return line.substring(fieldStart, i);
            currentField++;
            fieldStart = i + 1;
        }
    }
    return "";
}

static float getCSVFloat(const String &line, uint8_t fieldIndex) {
    return getCSVField(line, fieldIndex).toFloat();
}

static uint32_t getCSVUInt32(const String &line, uint8_t fieldIndex) {
    return (uint32_t)getCSVField(line, fieldIndex).toInt();
}

// Time helpers
static String formatDateFromEpoch(uint32_t epoch) {
    if (epoch == 0) return "N/A";
    DateTime time(epoch);
    char buffer[12];
    snprintf(buffer, sizeof(buffer), "%02d/%02d/%04d", time.day(), time.month(), time.year());
    return String(buffer);
}

static String formatTimeFromEpoch(uint32_t epoch) {
    if (epoch == 0) return "N/A";
    DateTime time(epoch);
    char buffer[10];
    snprintf(buffer, sizeof(buffer), "%02d:%02d:%02d", time.hour(), time.minute(), time.second());
    return String(buffer);
}

// File I/O
static void writeHourlyFile() {
    if (SD.exists(HOURLY_FILE)) SD.remove(HOURLY_FILE);
    File file = SD.open(HOURLY_FILE, FILE_WRITE);
    if (!file) { Serial.println("[TREND] Cannot write hourly file"); return; }
    file.println("start_epoch,tds_average");
    for (uint8_t i = 0; i < hourlyCount; i++) {
        file.print(hourlyRecords[i].startEpoch);
        file.print(",");
        file.println(hourlyRecords[i].averageTDS, 3);
    }
    file.close();
}

static void writeDailyFile() {
    if (SD.exists(DAILY_FILE)) SD.remove(DAILY_FILE);
    File file = SD.open(DAILY_FILE, FILE_WRITE);
    if (!file) { Serial.println("[TREND] Cannot write daily file"); return; }
    file.println("start_epoch,tds_average");
    for (uint8_t i = 0; i < dailyCount; i++) {
        file.print(dailyRecords[i].startEpoch);
        file.print(",");
        file.println(dailyRecords[i].averageTDS, 3);
    }
    file.close();
}

static void appendWeeklyRecord(const WeekRecord &record) {
    const bool fileExists = SD.exists(WEEKLY_FILE);
    File file = SD.open(WEEKLY_FILE, FILE_APPEND);
    if (!file) { Serial.println("[TREND] Cannot append weekly record"); return; }
    if (!fileExists) file.println("start_epoch,end_epoch,tds_average");
    file.print(record.startEpoch);
    file.print(",");
    file.print(record.endEpoch);
    file.print(",");
    file.println(record.averageTDS, 3);
    file.close();
}

static void writeTrendState() {
    if (SD.exists(STATE_FILE)) SD.remove(STATE_FILE);
    File file = SD.open(STATE_FILE, FILE_WRITE);
    if (!file) { Serial.println("[TREND] Cannot save trend state"); return; }
    file.println("points,previous_week_average,has_real_week,last_save_epoch");
    file.print(trendState.points);
    file.print(",");
    file.print(trendState.previousWeekAverage, 3);
    file.print(",");
    file.print(trendState.hasRealWeek ? 1 : 0);
    file.print(",");
    file.println(trendState.lastSaveEpoch);
    file.close();
}

static void loadHourlyFile() {
    hourlyCount = 0;
    if (!SD.exists(HOURLY_FILE)) return;
    File file = SD.open(HOURLY_FILE, FILE_READ);
    if (!file) return;
    while (file.available() && hourlyCount < TREND_HOURS_PER_DAY) {
        String line = file.readStringUntil('\n');
        line.trim();
        if (line.length() == 0 || line.startsWith("start_epoch")) continue;
        hourlyRecords[hourlyCount].startEpoch = getCSVUInt32(line, 0);
        hourlyRecords[hourlyCount].averageTDS = getCSVFloat(line, 1);
        hourlyCount++;
    }
    file.close();
}

static void loadDailyFile() {
    dailyCount = 0;
    if (!SD.exists(DAILY_FILE)) return;
    File file = SD.open(DAILY_FILE, FILE_READ);
    if (!file) return;
    while (file.available() && dailyCount < TREND_DAYS_PER_WEEK) {
        String line = file.readStringUntil('\n');
        line.trim();
        if (line.length() == 0 || line.startsWith("start_epoch")) continue;
        dailyRecords[dailyCount].startEpoch = getCSVUInt32(line, 0);
        dailyRecords[dailyCount].averageTDS = getCSVFloat(line, 1);
        dailyCount++;
    }
    file.close();
}

static void loadTrendState() {
    trendState.points = 0;
    trendState.previousWeekAverage = TREND_WEEK0_TDS_BASELINE_PPM;
    trendState.hasRealWeek = false;
    trendState.lastSaveEpoch = 0;
    if (!SD.exists(STATE_FILE)) { writeTrendState(); return; }
    File file = SD.open(STATE_FILE, FILE_READ);
    if (!file) return;
    while (file.available()) {
        String line = file.readStringUntil('\n');
        line.trim();
        if (line.length() == 0 || line.startsWith("points")) continue;
        trendState.points = (uint8_t)getCSVUInt32(line, 0);
        trendState.previousWeekAverage = getCSVFloat(line, 1);
        trendState.hasRealWeek = getCSVUInt32(line, 2) == 1;
        trendState.lastSaveEpoch = getCSVUInt32(line, 3);
        break;
    }
    file.close();
}

// Averages
static float getCurrentHourAverage() {
    if (currentHourSampleCount == 0) return 0.0f;
    return currentHourTDSSum / currentHourSampleCount;
}

static float getWindowAverage(uint8_t requestedHours) {
    float total = 0.0f;
    uint8_t count = 0;
    if (currentHourSampleCount > 0) { total += getCurrentHourAverage(); count++; }
    for (int i = hourlyCount - 1; i >= 0; i--) {
        if (count >= requestedHours) break;
        total += hourlyRecords[i].averageTDS;
        count++;
    }
    if (count == 0) return 0.0f;
    return total / count;
}

static float getCurrentDayAverage() { return getWindowAverage(TREND_HOURS_PER_DAY); }
static float getCurrentWeekAverage() {
    if (dailyCount == 0) return 0.0f;
    float total = 0.0f;
    for (uint8_t i = 0; i < dailyCount; i++) total += dailyRecords[i].averageTDS;
    return total / dailyCount;
}

// Hour management
static void resetCurrentHour(uint32_t nowEpoch) {
    currentHourStartEpoch = nowEpoch;
    currentHourTDSSum = 0.0f;
    currentHourSampleCount = 0;
}

static void saveCompletedHour(uint32_t nowEpoch) {
    if (currentHourSampleCount == 0) { resetCurrentHour(nowEpoch); return; }
    if (hourlyCount >= TREND_HOURS_PER_DAY) {
        for (uint8_t i = 1; i < hourlyCount; i++) hourlyRecords[i - 1] = hourlyRecords[i];
        hourlyCount--;
    }
    hourlyRecords[hourlyCount].startEpoch = currentHourStartEpoch;
    hourlyRecords[hourlyCount].averageTDS = getCurrentHourAverage();
    hourlyCount++;
    trendState.lastSaveEpoch = nowEpoch;
    writeHourlyFile();
    writeTrendState();
    Serial.print("[TREND] Hour saved: ");
    Serial.println(hourlyRecords[hourlyCount - 1].averageTDS, 2);
    resetCurrentHour(nowEpoch);

    if (hourlyCount < TREND_HOURS_PER_DAY) return;
    float dailyAverage = 0.0f;
    for (uint8_t i = 0; i < hourlyCount; i++) dailyAverage += hourlyRecords[i].averageTDS;
    dailyAverage /= hourlyCount;
    if (dailyCount >= TREND_DAYS_PER_WEEK) {
        for (uint8_t i = 1; i < dailyCount; i++) dailyRecords[i - 1] = dailyRecords[i];
        dailyCount--;
    }
    dailyRecords[dailyCount].startEpoch = hourlyRecords[0].startEpoch;
    dailyRecords[dailyCount].averageTDS = dailyAverage;
    dailyCount++;
    trendState.lastSaveEpoch = nowEpoch;
    writeDailyFile();
    hourlyCount = 0;
    writeHourlyFile();
    writeTrendState();
    Serial.print("[TREND] Day saved: ");
    Serial.println(dailyAverage, 2);

    if (dailyCount < TREND_DAYS_PER_WEEK) return;
    float weeklyAverage = 0.0f;
    for (uint8_t i = 0; i < dailyCount; i++) weeklyAverage += dailyRecords[i].averageTDS;
    weeklyAverage /= dailyCount;
    WeekRecord weeklyRecord;
    weeklyRecord.startEpoch = dailyRecords[0].startEpoch;
    weeklyRecord.endEpoch = nowEpoch;
    weeklyRecord.averageTDS = weeklyAverage;
    appendWeeklyRecord(weeklyRecord);
    
    const float difference = weeklyAverage - trendState.previousWeekAverage;
    if (difference > TREND_WEEKLY_TDS_DEADBAND_PPM) {
        if (trendState.points < TREND_MAX_POINTS) trendState.points++;
    } else if (difference < -TREND_WEEKLY_TDS_DEADBAND_PPM) {
        if (trendState.points > 0) trendState.points--;
    }
    
    trendState.previousWeekAverage = weeklyAverage;
    trendState.hasRealWeek = true;
    trendState.lastSaveEpoch = nowEpoch;
    dailyCount = 0;
    writeDailyFile();
    writeTrendState();
    Serial.println("[TREND] WEEK COMPLETED");
}

// Adaptive baseline
static void updateAdaptiveBaseline() {
    const SensorData sensor = getSensorData();
    AdaptiveBaseline &ab = trendResult.adaptiveBaseline;
    
    if (!ab.isActive) {
        if (trendState.points < 3) {
            ab.isActive = true;
            ab.isStable = true;
            ab.baselinePH = sensor.ph;
            ab.baselineTDS = sensor.tds;
            ab.baselineNTU = sensor.ntu;
            ab.thresholdPHLow = ab.baselinePH - 0.35f;
            ab.thresholdPHHigh = ab.baselinePH + 0.35f;
            ab.thresholdTDS = ab.baselineTDS * 1.15f;
            ab.thresholdNTU = ab.baselineNTU * 1.20f;
            ab.stableStartEpoch = getRTCEpoch();
            ab.warningActive = false;
            ab.warningStartEpoch = 0;
            Serial.print("[TREND] Adaptive baseline activated. TDS: ");
            Serial.println(ab.baselineTDS, 2);
        }
        return;
    }

    if (ab.isActive && ab.isStable) {
        const bool phExceeded = sensor.ph < ab.thresholdPHLow || sensor.ph > ab.thresholdPHHigh;
        const bool tdsExceeded = sensor.tdsValid && sensor.tds >= ab.thresholdTDS;
        const bool ntuExceeded = sensor.ntuValid && sensor.ntu >= ab.thresholdNTU;
        const bool anyExceeded = phExceeded || tdsExceeded || ntuExceeded;

        if (anyExceeded) {
            if (ab.warningStartEpoch == 0) {
                ab.warningStartEpoch = getRTCEpoch();
                ab.warningTriggeredPH = phExceeded;
                ab.warningTriggeredTDS = tdsExceeded;
                ab.warningTriggeredNTU = ntuExceeded;
                Serial.println("[TREND] Adaptive baseline warning started");
            } else {
                const uint32_t nowEpoch = getRTCEpoch();
                const uint32_t secondsElapsed = nowEpoch - ab.warningStartEpoch;
                if (secondsElapsed >= 86400UL) {
                    ab.warningActive = true;
                    Serial.println("[TREND] Adaptive baseline WARNING ACTIVE");
                }
            }
        } else {
            if (ab.warningStartEpoch > 0) {
                ab.warningStartEpoch = 0;
                ab.warningTriggeredPH = false;
                ab.warningTriggeredTDS = false;
                ab.warningTriggeredNTU = false;
                Serial.println("[TREND] Adaptive baseline warning cleared");
            }
        }
    }
}

// Results
static void resetTrendResult() {
    AdaptiveBaseline savedAB = trendResult.adaptiveBaseline;
    
    trendResult.status = TREND_DISABLED;
    trendResult.maintenanceRequired = false;
    trendResult.warningActive = false;
    trendResult.absoluteAlarmActive = false;
    trendResult.trendPoints = 0;
    trendResult.hourlyRecordCount = 0;
    trendResult.dailyRecordCount = 0;
    trendResult.currentTDS = 0.0f;
    trendResult.currentPH = 0.0f;
    trendResult.currentNTU = 0.0f;
    trendResult.currentDayAverage = 0.0f;
    trendResult.previousWeekAverage = 0.0f;
    trendResult.currentWeekAverage = 0.0f;
    trendResult.week0Baseline = TREND_WEEK0_TDS_BASELINE_PPM;
    trendResult.baselineWarningGate = 0.0f;
    trendResult.eightyPercentWarningGate = 0.0f;
    trendResult.absoluteSafetyLimit = ABSOLUTE_TDS_RELAY_STOP_PPM;
    trendResult.lastSaveDate = "N/A";
    trendResult.lastSaveTime = "N/A";
    
    trendResult.adaptiveBaseline = savedAB;
}

static void updateChartData() {
    chartData.hourlyCount = hourlyCount;
    for (uint8_t i = 0; i < 24; i++) chartData.hourlyValues[i] = 0.0f;
    for (uint8_t i = 0; i < hourlyCount; i++) chartData.hourlyValues[i] = hourlyRecords[i].averageTDS;
    chartData.average1Hour = getWindowAverage(1);
    chartData.average3Hour = getWindowAverage(3);
    chartData.average6Hour = getWindowAverage(6);
    chartData.average12Hour = getWindowAverage(12);
    chartData.average24Hour = getWindowAverage(24);
}

static void evaluateTrend() {
    resetTrendResult();
    const SensorData sensor = getSensorData();
    const float baselineGate = TREND_WEEK0_TDS_BASELINE_PPM * (1.0f + TREND_BASELINE_RISE_PERCENT);
    const float eightyPercentGate = ABSOLUTE_TDS_RELAY_STOP_PPM * TREND_WARNING_PERCENT_OF_ABSOLUTE;
    
    trendResult.currentTDS = sensor.tds;
    trendResult.currentPH = sensor.ph;
    trendResult.currentNTU = sensor.ntu;
    trendResult.trendPoints = trendState.points;
    trendResult.hourlyRecordCount = hourlyCount;
    trendResult.dailyRecordCount = dailyCount;
    trendResult.currentDayAverage = getCurrentDayAverage();
    trendResult.previousWeekAverage = trendState.previousWeekAverage;
    trendResult.currentWeekAverage = getCurrentWeekAverage();
    trendResult.week0Baseline = TREND_WEEK0_TDS_BASELINE_PPM;
    trendResult.baselineWarningGate = baselineGate;
    trendResult.eightyPercentWarningGate = eightyPercentGate;
    trendResult.absoluteSafetyLimit = ABSOLUTE_TDS_RELAY_STOP_PPM;
    trendResult.lastSaveDate = formatDateFromEpoch(trendState.lastSaveEpoch);
    trendResult.lastSaveTime = formatTimeFromEpoch(trendState.lastSaveEpoch);

    updateAdaptiveBaseline();

    if (sensor.tdsValid && sensor.tds >= ABSOLUTE_TDS_RELAY_STOP_PPM) {
        trendResult.status = TREND_ABSOLUTE_ALARM;
        trendResult.absoluteAlarmActive = true;
        trendResult.warningActive = true;
        trendResult.maintenanceRequired = true;
        return;
    }

    if (!TREND_ENGINE_ENABLED) { trendResult.status = TREND_DISABLED; return; }
    if (!trendReady) { trendResult.status = TREND_COLLECTING; return; }
    if (trendResult.adaptiveBaseline.warningActive) {
        trendResult.status = TREND_WARNING;
        trendResult.warningActive = true;
        trendResult.maintenanceRequired = true;
        return;
    }

    const bool passedBaselineGate = sensor.tdsValid && sensor.tds >= baselineGate;
    const bool passedWarningGate = sensor.tdsValid && sensor.tds >= eightyPercentGate;

    if (trendState.points >= TREND_WARNING_POINTS && passedBaselineGate && passedWarningGate) {
        trendResult.status = TREND_WARNING;
        trendResult.warningActive = true;
        trendResult.maintenanceRequired = true;
        return;
    }

    if (trendState.points >= TREND_MAINTENANCE_POINTS && passedBaselineGate) {
        trendResult.status = TREND_NEED_MAINTENANCE;
        trendResult.maintenanceRequired = true;
        return;
    }

    if (trendState.points >= TREND_WATCH_POINTS && passedBaselineGate) {
        trendResult.status = TREND_WATCH;
        return;
    }

    trendResult.status = TREND_NORMAL;
}

// Public API
void initTrendEngine() {
    resetTrendResult();
    hourlyCount = 0;
    dailyCount = 0;
    currentHourStartEpoch = 0;
    currentHourTDSSum = 0.0f;
    currentHourSampleCount = 0;
    lastSampleMs = 0;
    trendReady = false;

    Serial.println("[TREND] Initializing Trend Engine");

    if (!TREND_ENGINE_ENABLED) {
        Serial.println("[TREND] Trend history disabled");
        return;
    }

    if (!rtcAvailable()) {
        Serial.println("[TREND] RTC unavailable");
        return;
    }

    if (!sdCardAvailable()) {
        Serial.println("[TREND] SD card unavailable");
        return;
    }

    loadHourlyFile();
    loadDailyFile();
    loadTrendState();
    resetCurrentHour(getRTCEpoch());
    trendReady = true;

    Serial.print("[TREND] Ready. Baseline: ");
    Serial.println(TREND_WEEK0_TDS_BASELINE_PPM, 2);
}

void updateTrendEngine() {
    if (!TREND_ENGINE_ENABLED) { updateChartData(); evaluateTrend(); return; }
    if (!trendReady) { updateChartData(); evaluateTrend(); return; }
    if (!rtcAvailable() || !sdCardAvailable()) { trendReady = false; updateChartData(); evaluateTrend(); return; }

    const uint32_t nowEpoch = getRTCEpoch();
    const unsigned long nowMs = millis();

    if (lastSampleMs == 0 || nowMs - lastSampleMs >= TREND_SAMPLE_INTERVAL_MS) {
        currentHourTDSSum += getTDS();
        currentHourSampleCount++;
        lastSampleMs = nowMs;
    }

    if (nowEpoch - currentHourStartEpoch >= TREND_HOUR_SECONDS) {
        saveCompletedHour(nowEpoch);
    }

    updateChartData();
    evaluateTrend();
}

TrendResult getTrendResult() { return trendResult; }
TrendChartData getTrendChartData() { return chartData; }

const char *getTrendStatusText() {
    switch (trendResult.status) {
        case TREND_DISABLED: return "TREND DISABLED";
        case TREND_COLLECTING: return "COLLECTING";
        case TREND_NORMAL: return "NORMAL";
        case TREND_WATCH: return "TREND WATCH";
        case TREND_NEED_MAINTENANCE: return "NEED MAINTENANCE";
        case TREND_WARNING: return "WARNING";
        case TREND_ABSOLUTE_ALARM: return "ABSOLUTE ALARM";
    }
    return "UNKNOWN";
}

void setAdaptiveBaselinePH(float value) {
    if (value > 0.0f && value < 14.0f) {
        trendResult.adaptiveBaseline.baselinePH = value;
        trendResult.adaptiveBaseline.thresholdPHLow = value - 0.35f;
        trendResult.adaptiveBaseline.thresholdPHHigh = value + 0.35f;
        Serial.print("[TREND] Adaptive baseline pH: ");
        Serial.println(value, 2);
    }
}

void setAdaptiveBaselineTDS(float value) {
    if (value > 0.0f && value < 2000.0f) {
        trendResult.adaptiveBaseline.baselineTDS = value;
        trendResult.adaptiveBaseline.thresholdTDS = value * 1.15f;
        Serial.print("[TREND] Adaptive baseline TDS: ");
        Serial.println(value, 2);
    }
}

void setAdaptiveBaselineNTU(float value) {
    if (value >= 0.0f && value < 3000.0f) {
        trendResult.adaptiveBaseline.baselineNTU = value;
        trendResult.adaptiveBaseline.thresholdNTU = value * 1.20f;
        Serial.print("[TREND] Adaptive baseline NTU: ");
        Serial.println(value, 2);
    }
}

void resetAdaptiveBaseline() {
    memset(&trendResult.adaptiveBaseline, 0, sizeof(AdaptiveBaseline));
    Serial.println("[TREND] Adaptive baseline reset");
}