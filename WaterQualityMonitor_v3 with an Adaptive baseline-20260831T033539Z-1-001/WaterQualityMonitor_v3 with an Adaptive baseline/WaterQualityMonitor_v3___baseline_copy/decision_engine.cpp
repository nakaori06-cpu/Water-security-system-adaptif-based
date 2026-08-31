#include "decision_engine.h"

#include "config.h"
#include "water_quality.h"
#include "trend_engine.h"
#include "touch_manager.h"

// ==========================================================
// INTERNAL STATE
// ==========================================================

static DecisionState decision;

// ==========================================================
// INITIALIZATION
// ==========================================================

void initDecisionEngine()
{
    decision.mode = MODE_AUTO;

    decision.relay = RELAY_OFF;
    decision.buzzer = BUZZER_OFF;

    decision.alarm = false;
    decision.filterMaintenance = false;

    decision.relayLocked = false;
    decision.manualOverride = false;

    decision.absoluteSafetyLock = false;
    decision.externalOverride = false;
    decision.waterQualityLock = false;

    pinMode(RELAY_PIN, OUTPUT);
    pinMode(BUZZER_PIN, OUTPUT);

    digitalWrite(
        RELAY_PIN,
        RELAY_INACTIVE_LEVEL
    );

    digitalWrite(
        BUZZER_PIN,
        BUZZER_INACTIVE_LEVEL
    );
}

// ==========================================================
// OUTPUT HELPERS
// ==========================================================

static void applyRelayOutput()
{
    digitalWrite(
        RELAY_PIN,
        decision.relay == RELAY_ON
            ? RELAY_ACTIVE_LEVEL
            : RELAY_INACTIVE_LEVEL
    );
}

static void applyBuzzerOutput()
{
    digitalWrite(
        BUZZER_PIN,
        decision.buzzer == BUZZER_ON
            ? BUZZER_ACTIVE_LEVEL
            : BUZZER_INACTIVE_LEVEL
    );
}

// ==========================================================
// DECISION UPDATE
// ==========================================================
//
// Priority order:
//
// 1. Absolute TDS alarm
// 2. GPIO34 external relay override
// 3. Manual pump OFF
// 4. Manual pump ON
// 5. Automatic water-quality logic
//
// Trend warning and trend maintenance do not stop the relay.
// Only the absolute TDS alarm, external override, manual OFF,
// or configured UNSAFE water-quality lock can stop it.
//

void updateDecisionEngine()
{
    const WaterQuality water =
        getWaterQuality();

    const TrendResult trend =
        getTrendResult();

    const bool externalOverride =
        isRelayAutoModeDisabled();

    decision.filterMaintenance =
        water.filterMaintenance ||
        trend.maintenanceRequired;

    decision.absoluteSafetyLock =
        trend.absoluteAlarmActive;

    decision.externalOverride =
        externalOverride;

    decision.waterQualityLock =
        water.valid &&
        water.alarm &&
        !trend.absoluteAlarmActive;

    decision.relayLocked =
        decision.absoluteSafetyLock ||
        decision.externalOverride ||
        decision.waterQualityLock;

    // ------------------------------------------------------
    // Priority 1: absolute TDS safety
    // ------------------------------------------------------

    if (trend.absoluteAlarmActive)
    {
        decision.relay = RELAY_OFF;
        decision.buzzer = BUZZER_ON;
        decision.alarm = true;
        decision.manualOverride = false;

        applyRelayOutput();
        applyBuzzerOutput();

        return;
    }

    // ------------------------------------------------------
    // Priority 2: GPIO34 external override
    // ------------------------------------------------------

    if (externalOverride)
    {
        decision.relay = RELAY_OFF;
        decision.buzzer = BUZZER_OFF;
        decision.alarm = false;
        decision.manualOverride = true;

        applyRelayOutput();
        applyBuzzerOutput();

        return;
    }

    // ------------------------------------------------------
    // Priority 3: manual pump OFF
    // ------------------------------------------------------

    if (decision.mode == MODE_MANUAL_OFF)
    {
        decision.relay = RELAY_OFF;
        decision.buzzer = BUZZER_OFF;
        decision.alarm = false;
        decision.manualOverride = true;

        applyRelayOutput();
        applyBuzzerOutput();

        return;
    }

    // ------------------------------------------------------
    // Priority 4: manual pump ON
    // ------------------------------------------------------

    if (decision.mode == MODE_MANUAL_ON)
    {
        decision.relay =
            water.valid &&
            water.relayEnable
                ? RELAY_ON
                : RELAY_OFF;

        decision.buzzer = BUZZER_OFF;
        decision.alarm = false;
        decision.manualOverride = true;

        applyRelayOutput();
        applyBuzzerOutput();

        return;
    }

    // ------------------------------------------------------
    // Priority 5: automatic operation
    // ------------------------------------------------------

    decision.manualOverride = false;

    decision.relay =
        water.valid &&
        water.relayEnable
            ? RELAY_ON
            : RELAY_OFF;

    decision.buzzer =
        water.buzzerEnable
            ? BUZZER_ON
            : BUZZER_OFF;

    decision.alarm =
        water.alarm;

    applyRelayOutput();
    applyBuzzerOutput();
}

// ==========================================================
// GETTER
// ==========================================================

DecisionState getDecisionState()
{
    return decision;
}

// ==========================================================
// MODES
// ==========================================================

void setAutoMode()
{
    decision.mode = MODE_AUTO;
    decision.manualOverride = false;

    Serial.println(
        "[DECISION] Mode changed to AUTO"
    );
}

void setManualRelayON()
{
    decision.mode = MODE_MANUAL_ON;
    decision.manualOverride = true;

    Serial.println(
        "[DECISION] Mode changed to MANUAL ON"
    );
}

void setManualRelayOFF()
{
    decision.mode = MODE_MANUAL_OFF;
    decision.manualOverride = true;

    Serial.println(
        "[DECISION] Mode changed to MANUAL OFF"
    );
}

// ==========================================================
// DEBUG
// ==========================================================

void printDecisionDebug()
{
    Serial.println();
    Serial.println("========== DECISION DEBUG ==========");

    Serial.print("Mode: ");

    switch (decision.mode)
    {
        case MODE_AUTO:
            Serial.println("AUTO");
            break;

        case MODE_MANUAL_ON:
            Serial.println("MANUAL ON");
            break;

        case MODE_MANUAL_OFF:
            Serial.println("MANUAL OFF");
            break;
    }

    Serial.print("Relay: ");
    Serial.println(
        decision.relay == RELAY_ON
            ? "ON"
            : "OFF"
    );

    Serial.print("Buzzer: ");
    Serial.println(
        decision.buzzer == BUZZER_ON
            ? "ON"
            : "OFF"
    );

    Serial.print("Alarm: ");
    Serial.println(
        decision.alarm
            ? "YES"
            : "NO"
    );

    Serial.print("Maintenance: ");
    Serial.println(
        decision.filterMaintenance
            ? "YES"
            : "NO"
    );

    Serial.print("Absolute lock: ");
    Serial.println(
        decision.absoluteSafetyLock
            ? "YES"
            : "NO"
    );

    Serial.print("External override: ");
    Serial.println(
        decision.externalOverride
            ? "YES"
            : "NO"
    );

    Serial.print("Water-quality lock: ");
    Serial.println(
        decision.waterQualityLock
            ? "YES"
            : "NO"
    );

    Serial.println(
        "===================================="
    );
}