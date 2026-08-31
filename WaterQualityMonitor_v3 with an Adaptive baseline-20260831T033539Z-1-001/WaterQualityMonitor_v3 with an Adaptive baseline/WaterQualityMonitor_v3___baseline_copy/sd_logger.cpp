#include "sd_logger.h"

#include <SPI.h>
#include <SD.h>

#include "config.h"
#include "rtc_manager.h"
#include "sensors.h"
#include "water_quality.h"
#include "decision_engine.h"
#include "trend_engine.h"

// ==========================================================
// SD STATE
// ==========================================================

static SPIClass sdSPI(HSPI);

static bool sdReady = false;

static unsigned long lastLogMs = 0;

static const char *LOG_FILE_NAME =
    "/water_log.csv";

// ==========================================================
// CSV HEADER
// ==========================================================

static const char *CSV_HEADER =
    "date,time,"
    "temperature_c,ph,tds_ppm,ntu,"
    "tds_voltage,ph_voltage,ntu_voltage,"
    "tds_raw_ec,tds_temperature_coefficient,"
    "raw_tds,raw_ph,raw_ntu,"
    "water_score,water_status,water_risk,"
    "water_confidence,"
    "relay_state,buzzer_state,alarm,"
    "maintenance,"
    "ads1115_ok,ds18b20_ok,"
    "trend_status,trend_points";

// ==========================================================
// HELPERS
// ==========================================================

static void createHeaderIfRequired()
{
    if (SD.exists(LOG_FILE_NAME))
    {
        return;
    }

    File file =
        SD.open(
            LOG_FILE_NAME,
            FILE_WRITE
        );

    if (!file)
    {
        Serial.println(
            "[SD] Failed to create log file"
        );

        return;
    }

    file.println(CSV_HEADER);
    file.close();

    Serial.println(
        "[SD] CSV header created"
    );
}

static bool writeCSVRecord()
{
    const SensorData sensor =
        getSensorData();

    const WaterQuality water =
        getWaterQuality();

    const DecisionState decision =
        getDecisionState();

    const TrendResult trend =
        getTrendResult();

    File file =
        SD.open(
            LOG_FILE_NAME,
            FILE_APPEND
        );

    if (!file)
    {
        Serial.println(
            "[SD] Failed to open CSV file"
        );

        return false;
    }

    // ------------------------------------------------------
    // Date and time
    // ------------------------------------------------------

    file.print(getDateString());
    file.print(",");

    file.print(getTimeString());
    file.print(",");

    // ------------------------------------------------------
    // Sensor values
    // ------------------------------------------------------

    file.print(sensor.temperature, 2);
    file.print(",");

    file.print(sensor.ph, 3);
    file.print(",");

    file.print(sensor.tds, 3);
    file.print(",");

    file.print(sensor.ntu, 3);
    file.print(",");

    file.print(sensor.tdsVoltage, 6);
    file.print(",");

    file.print(sensor.phVoltage, 6);
    file.print(",");

    file.print(sensor.ntuVoltage, 6);
    file.print(",");

    file.print(sensor.tdsRawEC, 6);
    file.print(",");

    file.print(
        sensor.tdsTemperatureCoefficient,
        6
    );
    file.print(",");

    file.print(sensor.rawTDS);
    file.print(",");

    file.print(sensor.rawPH);
    file.print(",");

    file.print(sensor.rawNTU);
    file.print(",");

    // ------------------------------------------------------
    // Water-quality state
    // ------------------------------------------------------

    file.print(water.score, 2);
    file.print(",");

    file.print(water.statusText);
    file.print(",");

    file.print(water.riskText);
    file.print(",");

    file.print(water.confidence, 1);
    file.print(",");

    // ------------------------------------------------------
    // Output state
    // ------------------------------------------------------

    file.print(
        decision.relay == RELAY_ON
            ? "ON"
            : "OFF"
    );
    file.print(",");

    file.print(
        decision.buzzer == BUZZER_ON
            ? "ON"
            : "OFF"
    );
    file.print(",");

    file.print(
        decision.alarm
            ? "YES"
            : "NO"
    );
    file.print(",");

    file.print(
        decision.filterMaintenance
            ? "YES"
            : "NO"
    );
    file.print(",");

    // ------------------------------------------------------
    // Hardware state
    // ------------------------------------------------------

    file.print(
        sensor.ads1115OK
            ? "OK"
            : "FAIL"
    );
    file.print(",");

    file.print(
        sensor.ds18b20OK
            ? "OK"
            : "FAIL"
    );
    file.print(",");

    // ------------------------------------------------------
    // Trend state
    // ------------------------------------------------------

    file.print(getTrendStatusText());
    file.print(",");

    file.println(trend.trendPoints);

    file.close();

    return true;
}

// ==========================================================
// INITIALIZATION
// ==========================================================

void initSDLogger()
{
    sdReady = false;
    lastLogMs = millis();

    Serial.println();
    Serial.println("=================================");
    Serial.println(" Initializing SD Card Logger");
    Serial.println("=================================");

    // Make sure the chip-select line is inactive
    // before starting the SPI bus.
    pinMode(SD_CS_PIN, OUTPUT);
    digitalWrite(SD_CS_PIN, HIGH);

    // Separate HSPI bus:
    // SCK  = GPIO25
    // MISO = GPIO27
    // MOSI = GPIO26
    // CS   = GPIO13
    sdSPI.begin(
        SD_SCK_PIN,
        SD_MISO_PIN,
        SD_MOSI_PIN,
        SD_CS_PIN
    );

    if (
        !SD.begin(
            SD_CS_PIN,
            sdSPI
        )
    )
    {
        Serial.println(
            "[FAIL] SD card initialization failed"
        );

        Serial.println(
            "[INFO] Logging disabled"
        );

        return;
    }

    if (SD.cardType() == CARD_NONE)
    {
        Serial.println(
            "[FAIL] No SD card detected"
        );

        return;
    }

    sdReady = true;

    const uint64_t cardSizeMB =
        SD.cardSize() /
        (1024ULL * 1024ULL);

    Serial.print("[ OK ] SD card ready: ");
    Serial.print((unsigned long)cardSizeMB);
    Serial.println(" MB");

    createHeaderIfRequired();

    Serial.print("[SD] Log interval: ");
    Serial.print(SD_LOG_INTERVAL_MS / 1000UL);
    Serial.println(" seconds");

    Serial.println("=================================");
}

// ==========================================================
// UPDATE
// ==========================================================

void updateSDLogger()
{
    if (!sdReady)
    {
        return;
    }

    if (!rtcAvailable())
    {
        return;
    }

    const unsigned long now =
        millis();

    if (
        now - lastLogMs <
        SD_LOG_INTERVAL_MS
    )
    {
        return;
    }

    lastLogMs = now;

    const SensorData sensor =
        getSensorData();

    // Do not write startup or invalid data.
    if (
        !sensor.allRequiredSensorsValid
    )
    {
        Serial.println(
            "[SD] Skipped invalid sensor record"
        );

        return;
    }

    if (writeCSVRecord())
    {
        Serial.print("[SD] Saved: ");
        Serial.print(getDateString());
        Serial.print(" ");
        Serial.println(getTimeString());
    }
}

// ==========================================================
// STATUS
// ==========================================================

bool sdCardAvailable()
{
    return sdReady;
}