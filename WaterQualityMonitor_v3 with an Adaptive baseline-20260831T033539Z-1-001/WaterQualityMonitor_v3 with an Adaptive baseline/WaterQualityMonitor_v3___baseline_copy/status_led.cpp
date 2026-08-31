#include "status_led.h"

#include "config.h"
#include "sensors.h"
#include "water_quality.h"
#include "trend_engine.h"
#include "decision_engine.h"

// ==========================================================
// LED HELPERS
// ==========================================================

static void setLED(
    uint8_t pin,
    bool enabled
)
{
    digitalWrite(
        pin,
        enabled
            ? LED_ACTIVE_LEVEL
            : LED_INACTIVE_LEVEL
    );
}

static void turnOffAllLEDs()
{
    setLED(LED_RED_PIN, false);
    setLED(LED_YELLOW_PIN, false);
    setLED(LED_GREEN_PIN, false);
}

// ==========================================================
// INITIALIZATION
// ==========================================================

void initStatusLEDs()
{
    pinMode(LED_RED_PIN, OUTPUT);
    pinMode(LED_YELLOW_PIN, OUTPUT);
    pinMode(LED_GREEN_PIN, OUTPUT);

    turnOffAllLEDs();

    Serial.println(
        "[LED] Status LEDs initialized"
    );
}

// ==========================================================
// UPDATE
// ==========================================================
//
// Priority:
//
// 1. Absolute TDS alarm / active water alarm -> RED
// 2. Warning / danger trend / maintenance    -> YELLOW
// 3. Valid safe water                      -> GREEN
// 4. Warm-up or unavailable readings         -> YELLOW
//
// Warm-up is never shown as green because green would
// incorrectly imply that the water was verified safe.
//

void updateStatusLEDs()
{
    const SensorData sensor =
        getSensorData();

    const WaterQuality water =
        getWaterQuality();

    const TrendResult trend =
        getTrendResult();

    const DecisionState decision =
        getDecisionState();

    turnOffAllLEDs();

    // ------------------------------------------------------
    // RED: alarm or dangerous water
    // ------------------------------------------------------

    if (
        trend.absoluteAlarmActive ||
        water.alarm ||
        decision.alarm ||
        water.risk == DANGER
    )
    {
        setLED(
            LED_RED_PIN,
            true
        );

        return;
    }

    // ------------------------------------------------------
    // YELLOW: warning, maintenance, or warm-up
    // ------------------------------------------------------

    if (
        trend.warningActive ||
        trend.maintenanceRequired ||
        water.filterMaintenance ||
        water.risk == WARNING ||
        water.status == WATER_WARMUP ||
        !sensor.allRequiredSensorsValid
    )
    {
        setLED(
            LED_YELLOW_PIN,
            true
        );

        return;
    }

    // ------------------------------------------------------
    // GREEN: valid non-dangerous result
    // ------------------------------------------------------

    if (
        water.valid &&
        (
            water.status == EXCELLENT ||
            water.status == GOOD
        )
    )
    {
        setLED(
            LED_GREEN_PIN,
            true
        );

        return;
    }

    // Unknown fallback: yellow.
    setLED(
        LED_YELLOW_PIN,
        true
    );
}