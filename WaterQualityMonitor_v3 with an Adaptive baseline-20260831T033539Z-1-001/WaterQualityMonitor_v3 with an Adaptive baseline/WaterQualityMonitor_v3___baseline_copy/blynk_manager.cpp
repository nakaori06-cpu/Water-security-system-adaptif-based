#include "blynk_manager.h"

// Blynk configuration macros must be visible before
// including the Blynk library.
#include "secrets.h"

#define BLYNK_PRINT Serial

#include <WiFi.h>
#include <BlynkSimpleEsp32.h>

#include "config.h"
#include "sensors.h"
#include "water_quality.h"
#include "trend_engine.h"
#include "decision_engine.h"
#include "touch_manager.h"

// ==========================================================
// BLYNK STATE
// ==========================================================

static BlynkTimer blynkTimer;

static bool blynkConfigured = false;

static unsigned long lastBlynkConnectAttemptMs = 0;
static unsigned long lastBlynkMessageMs = 0;

// A short timeout prevents long blocking periods in loop().
static const unsigned long BLYNK_CONNECT_TIMEOUT_MS =
    100UL;

// ==========================================================
// STATUS CODE
// ==========================================================
//
// 0 = safe
// 1 = maintenance, warm-up, or trend collection
// 2 = alarm, warning, or dangerous water
//

static uint8_t getBlynkStatusCode()
{
    const SensorData sensor =
        getSensorData();

    const WaterQuality water =
        getWaterQuality();

    const TrendResult trend =
        getTrendResult();

    const DecisionState decision =
        getDecisionState();

    if (
        trend.absoluteAlarmActive ||
        trend.warningActive ||
        water.alarm ||
        decision.alarm ||
        water.risk == DANGER
    )
    {
        return 2;
    }

    if (
        !sensor.allRequiredSensorsValid ||
        trend.maintenanceRequired ||
        water.filterMaintenance ||
        water.status == WATER_WARMUP ||
        water.risk == WARNING
    )
    {
        return 1;
    }

    return 0;
}

// ==========================================================
// SEND TELEMETRY
// ==========================================================
//
// V5 and V6 are command/control pins.
// They are not overwritten with telemetry.
//
// V5:
//     pump command: OFF or AUTO
//
// V6:
//     relay automatic-control override
//
// V7:
//     status code
//
// V8:
//     trend points
//

static void sendBlynkData()
{
    if (
        !blynkConfigured ||
        !Blynk.connected()
    )
    {
        return;
    }

    const SensorData sensor =
        getSensorData();

    const WaterQuality water =
        getWaterQuality();

    const TrendResult trend =
        getTrendResult();

    const DecisionState decision =
        getDecisionState();

    // V0: pH
    Blynk.virtualWrite(
        V0,
        sensor.phValid
            ? sensor.ph
            : 0
    );

    // V1: TDS
    Blynk.virtualWrite(
        V1,
        sensor.tdsValid
            ? sensor.tds
            : 0
    );

    // V2: temperature
    Blynk.virtualWrite(
        V2,
        sensor.temperatureValid
            ? sensor.temperature
            : 0
    );

    // V3: turbidity / NTU
    Blynk.virtualWrite(
        V3,
        sensor.ntuValid
            ? sensor.ntu
            : 0
    );

    // V4: water-quality score
    Blynk.virtualWrite(
        V4,
        water.valid
            ? water.score
            : 0
    );

    // Do not write V5 here.
    // V5 is a control command input.

    // V6: GPIO34 relay override state
    Blynk.virtualWrite(
        V6,
        isRelayAutoModeDisabled()
            ? 1
            : 0
    );

    // V7: status code
    Blynk.virtualWrite(
        V7,
        getBlynkStatusCode()
    );

    // V8: trend points
    Blynk.virtualWrite(
        V8,
        trend.trendPoints
    );

    if (
        millis() - lastBlynkMessageMs >=
        10000UL
    )
    {
        lastBlynkMessageMs = millis();

        Serial.println(
            "[Blynk] Telemetry sent"
        );

        Serial.print(
            "[Blynk] Relay: "
        );
        Serial.println(
            decision.relay == RELAY_ON
                ? "ON"
                : "OFF"
        );
    }
}

// ==========================================================
// CONNECTION CALLBACK
// ==========================================================

BLYNK_CONNECTED()
{
    Serial.println();
    Serial.println(
        "[Blynk] Cloud connected"
    );

    // Restore the two control widgets.
    Blynk.syncVirtual(V5);
    Blynk.syncVirtual(V6);

    // Send one immediate telemetry update.
    sendBlynkData();
}

// ==========================================================
// V5: PUMP CONTROL
// ==========================================================
//
// 0 = manual pump OFF
// 1 = AUTO mode
//
// Absolute TDS safety still has higher priority than AUTO
// and MANUAL ON behavior.
//

BLYNK_WRITE(V5)
{
    const int value =
        param.asInt();

    if (value == 0)
    {
        Serial.println(
            "[Blynk] V5 command: MANUAL OFF"
        );

        setManualRelayOFF();
    }
    else
    {
        Serial.println(
            "[Blynk] V5 command: AUTO"
        );

        setAutoMode();
    }
}

// ==========================================================
// V6: RELAY AUTO OVERRIDE
// ==========================================================
//
// 0 = automatic relay control enabled
// 1 = relay automatic control disabled and relay OFF
//

BLYNK_WRITE(V6)
{
    const bool disabled =
        param.asInt() == 1;

    setRelayAutoModeDisabled(
        disabled
    );

    Serial.print(
        "[Blynk] V6 relay override: "
    );

    Serial.println(
        disabled
            ? "DISABLED / RELAY OFF"
            : "ENABLED / AUTOMATIC"
    );
}

// ==========================================================
// INITIALIZATION
// ==========================================================

void initBlynk()
{
    Serial.println();
    Serial.println(
        "================================="
    );
    Serial.println(
        " Initializing Blynk"
    );
    Serial.println(
        "================================="
    );

    Blynk.config(
        BLYNK_AUTH_TOKEN
    );

    blynkConfigured = true;

    blynkTimer.setInterval(
        BLYNK_UPDATE_INTERVAL_MS,
        sendBlynkData
    );

    Serial.println(
        "[ OK ] Blynk configured"
    );

    Serial.println(
        "[Blynk] Waiting for Wi-Fi"
    );

    Serial.println(
        "================================="
    );
}

// ==========================================================
// UPDATE
// ==========================================================

void updateBlynk()
{
    if (!blynkConfigured)
    {
        return;
    }

    const unsigned long now =
        millis();

    // Service the cloud on every loop while connected.
    if (Blynk.connected())
    {
        Blynk.run();
    }
    else if (WiFi.status() == WL_CONNECTED)
    {
        if (
            lastBlynkConnectAttemptMs == 0 ||
            now - lastBlynkConnectAttemptMs >=
            BLYNK_RECONNECT_INTERVAL_MS
        )
        {
            lastBlynkConnectAttemptMs =
                now;

            Serial.println(
                "[Blynk] Connection attempt"
            );

            // Keep the timeout short so the main loop remains
            // responsive to sensors and safety logic.
            const bool connected =
                Blynk.connect(
                    BLYNK_CONNECT_TIMEOUT_MS
                );

            Serial.println(
                connected
                    ? "[Blynk] Connected"
                    : "[Blynk] Not connected"
            );
        }
    }

    // Timed virtualWrite operations are handled here.
    blynkTimer.run();
}

// ==========================================================
// STATUS
// ==========================================================

bool isBlynkConnected()
{
    return
        blynkConfigured &&
        Blynk.connected();
}