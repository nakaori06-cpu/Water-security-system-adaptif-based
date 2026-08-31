#include "display_engine.h"

#include "config.h"
#include "sensors.h"
#include "water_quality.h"
#include "decision_engine.h"
#include "display_oled.h"
#include "display_tft.h"

// ==========================================================
// DISPLAY TIMERS
// ==========================================================

static unsigned long lastOLEDUpdateMs = 0;
static unsigned long lastTFTUpdateMs = 0;

// ==========================================================
// INITIALIZATION
// ==========================================================

void initDisplayEngine()
{
    Serial.println();
    Serial.println(
        "================================="
    );
    Serial.println(
        " Initializing Display Engine"
    );
    Serial.println(
        "================================="
    );

    initOLED();
    initTFT();

    lastOLEDUpdateMs = millis();
    lastTFTUpdateMs = millis();

    Serial.println(
        "[ OK ] Display engine initialized"
    );
}

// ==========================================================
// UPDATE
// ==========================================================

void updateDisplayEngine()
{
    const unsigned long now =
        millis();

    const SensorData sensor =
        getSensorData();

    const WaterQuality water =
        getWaterQuality();

    const DecisionState decision =
        getDecisionState();

    if (
        now - lastOLEDUpdateMs >=
        OLED_UPDATE_INTERVAL_MS
    )
    {
        lastOLEDUpdateMs = now;

        updateOLED(
            sensor,
            water,
            decision
        );
    }

    if (
        now - lastTFTUpdateMs >=
        TFT_UPDATE_INTERVAL_MS
    )
    {
        lastTFTUpdateMs = now;

        updateTFT(
            sensor,
            water,
            decision
        );
    }
}