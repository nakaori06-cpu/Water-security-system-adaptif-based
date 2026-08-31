#include <Arduino.h>
#include <Wire.h>

#include "config.h"

#include "rtc_manager.h"
#include "sensors.h"
#include "water_quality.h"
#include "sd_logger.h"
#include "trend_engine.h"
#include "decision_engine.h"
#include "display_engine.h"
#include "touch_manager.h"
#include "wifi_manager.h"
#include "blynk_manager.h"
#include "status_led.h"
#include "serial_cli.h"

// ==========================================================
// DEBUG TIMERS
// ==========================================================

static unsigned long lastDebugMs = 0;

static const unsigned long DEBUG_INTERVAL_MS =
    30000UL;

// ==========================================================
// SETUP
// ==========================================================

void setup()
{
    Serial.begin(
        SERIAL_BAUD_RATE
    );

    delay(1000);

    Serial.println();
    Serial.println(
        "========================================"
    );
    Serial.println(
        " ESP32 WATER QUALITY MONITOR"
    );
    Serial.print(
        " Firmware: "
    );
    Serial.println(
        FW_VERSION
    );
    Serial.println(
        "========================================"
    );

    // ------------------------------------------------------
    // I2C MUST BE INITIALIZED FIRST
    // ------------------------------------------------------

    Wire.begin(
        I2C_SDA,
        I2C_SCL
    );

    Wire.setClock(
        100000UL
    );

    Serial.print(
        "[I2C] SDA: GPIO"
    );
    Serial.println(I2C_SDA);

    Serial.print(
        "[I2C] SCL: GPIO"
    );
    Serial.println(I2C_SCL);

    // ------------------------------------------------------
    // HARDWARE INITIALIZATION
    // ------------------------------------------------------

    initRTC();

    initSensors();

    initWaterQuality();

    initSDLogger();

    initTrendEngine();

    initDecisionEngine();

    initTouchManager();

    initStatusLEDs();

    initDisplayEngine();

    // ------------------------------------------------------
    // SERIAL CLI
    // ------------------------------------------------------

    initSerialCLI();

    // ------------------------------------------------------
    // NETWORK INITIALIZATION
    // ------------------------------------------------------

    initWiFi();

    initBlynk();

    Serial.println();
    Serial.println(
        "========================================"
    );
    Serial.println(
        " SYSTEM READY"
    );
    Serial.println(
        "========================================"
    );

    Serial.println();
    Serial.print(
        "[CLI] > "
    );
}

// ==========================================================
// MAIN LOOP
// ==========================================================

void loop()
{
    // ------------------------------------------------------
    // 0. Serial CLI (non-blocking)
    // ------------------------------------------------------

    updateSerialCLI();

    // ------------------------------------------------------
    // 1. Time
    // ------------------------------------------------------

    updateRTC();

    // ------------------------------------------------------
    // 2. Sensors
    // ------------------------------------------------------

    updateSensors();

    // ------------------------------------------------------
    // 3. Water-quality classification
    // ------------------------------------------------------

    updateWaterQuality();

    // ------------------------------------------------------
    // 4. Local input handling
    // ------------------------------------------------------

    updateTouchManager();

    // ------------------------------------------------------
    // 5. Trend and absolute TDS safety evaluation
    // ------------------------------------------------------

    updateTrendEngine();

    // ------------------------------------------------------
    // 6. Relay and buzzer decision
    // ------------------------------------------------------

    updateDecisionEngine();

    // ------------------------------------------------------
    // 7. Status LEDs
    // ------------------------------------------------------

    updateStatusLEDs();

    // ------------------------------------------------------
    // 8. Displays
    // ------------------------------------------------------

    updateDisplayEngine();

    // ------------------------------------------------------
    // 9. Wi-Fi and Blynk
    // ------------------------------------------------------

    updateWiFi();

    updateBlynk();

    // ------------------------------------------------------
    // 10. SD logging
    // ------------------------------------------------------

    updateSDLogger();

    // ------------------------------------------------------
    // 11. Periodic diagnostics
    // ------------------------------------------------------

    const unsigned long now =
        millis();

    if (
        now - lastDebugMs >=
        DEBUG_INTERVAL_MS
    )
    {
        lastDebugMs = now;

        printSensorDebug();
        printDecisionDebug();

        const TrendResult trend =
            getTrendResult();

        Serial.println();
        Serial.println(
            "========== TREND DEBUG =========="
        );

        Serial.print(
            "Status: "
        );
        Serial.println(
            getTrendStatusText()
        );

        Serial.print(
            "Current TDS: "
        );
        Serial.println(
            trend.currentTDS,
            2
        );

        Serial.print(
            "Current pH: "
        );
        Serial.println(
            trend.currentPH,
            2
        );

        Serial.print(
            "Current NTU: "
        );
        Serial.println(
            trend.currentNTU,
            2
        );

        Serial.print(
            "Trend points: "
        );
        Serial.println(
            trend.trendPoints
        );

        Serial.print(
            "Hourly records: "
        );
        Serial.println(
            trend.hourlyRecordCount
        );

        Serial.print(
            "Daily records: "
        );
        Serial.println(
            trend.dailyRecordCount
        );

        Serial.print(
            "Previous week average: "
        );
        Serial.println(
            trend.previousWeekAverage,
            2
        );

        Serial.print(
            "Current week average: "
        );
        Serial.println(
            trend.currentWeekAverage,
            2
        );

        if (trend.adaptiveBaseline.isActive)
        {
            Serial.println(
                "[TREND] Adaptive baseline ACTIVE"
            );

            Serial.print(
                "  Baseline TDS: "
            );
            Serial.println(
                trend.adaptiveBaseline.baselineTDS,
                2
            );

            Serial.print(
                "  Threshold TDS: "
            );
            Serial.println(
                trend.adaptiveBaseline.thresholdTDS,
                2
            );

            Serial.print(
                "  Warning active: "
            );
            Serial.println(
                trend.adaptiveBaseline.warningActive
                    ? "YES"
                    : "NO"
            );
        }

        Serial.print(
            "Maintenance: "
        );
        Serial.println(
            trend.maintenanceRequired
                ? "YES"
                : "NO"
        );

        Serial.print(
            "Warning: "
        );
        Serial.println(
            trend.warningActive
                ? "YES"
                : "NO"
        );

        Serial.print(
            "Absolute alarm: "
        );
        Serial.println(
            trend.absoluteAlarmActive
                ? "YES"
                : "NO"
        );

        Serial.println(
            "================================="
        );
    }

    // Give background tasks a small opportunity to run.
    delay(1);
}