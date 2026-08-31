#include "touch_manager.h"

#include "config.h"

// ==========================================================
// INTERNAL STATE
// ==========================================================

static bool manualOverrideActive = false;
static bool relayAutoModeDisabled = false;

static bool lastRelayTouchState = false;

static unsigned long lastRelayTouchChangeMs = 0;

// ==========================================================
// INITIALIZATION
// ==========================================================

void initTouchManager()
{
    Serial.println();
    Serial.println(
        "================================="
    );
    Serial.println(
        " Initializing Touch Manager"
    );
    Serial.println(
        "================================="
    );

    // GPIO34 has no reliable internal pull-up/down.
    pinMode(
        TOUCH_RELAY_OVERRIDE_PIN,
        INPUT
    );

    manualOverrideActive = false;
    relayAutoModeDisabled = false;

    lastRelayTouchState =
        digitalRead(
            TOUCH_RELAY_OVERRIDE_PIN
        ) == TOUCH_INPUT_ACTIVE_LEVEL;

    lastRelayTouchChangeMs =
        millis();

    Serial.println(
        "[ OK ] GPIO34 relay override input"
    );

    Serial.println(
        "[ OK ] GPIO35 reserved for TFT pages"
    );

    Serial.println(
        "[INFO] GPIO34/GPIO35 require external biasing"
    );

    Serial.println(
        "================================="
    );
}

// ==========================================================
// UPDATE
// ==========================================================

void updateTouchManager()
{
    const unsigned long now =
        millis();

    const bool currentState =
        digitalRead(
            TOUCH_RELAY_OVERRIDE_PIN
        ) == TOUCH_INPUT_ACTIVE_LEVEL;

    // Detect only a new press, not a continuously held input.
    const bool newPress =
        currentState &&
        !lastRelayTouchState;

    if (
        newPress &&
        now - lastRelayTouchChangeMs >=
        TOUCH_DEBOUNCE_MS
    )
    {
        lastRelayTouchChangeMs =
            now;

        relayAutoModeDisabled =
            !relayAutoModeDisabled;

        Serial.println();
        Serial.println(
            "========== RELAY OVERRIDE =========="
        );

        Serial.print(
            "Automatic relay control: "
        );

        Serial.println(
            relayAutoModeDisabled
                ? "DISABLED"
                : "ENABLED"
        );

        Serial.print(
            "Relay behavior: "
        );

        Serial.println(
            relayAutoModeDisabled
                ? "FORCED OFF"
                : "AUTOMATIC"
        );

        Serial.println(
            "====================================="
        );
    }

    lastRelayTouchState =
        currentState;
}

// ==========================================================
// STATUS
// ==========================================================

bool isManualOverrideActive()
{
    return manualOverrideActive;
}

bool isRelayAutoModeDisabled()
{
    return relayAutoModeDisabled;
}

// ==========================================================
// EXTERNAL CONTROL
// ==========================================================

void setManualOverride(
    bool state
)
{
    manualOverrideActive =
        state;
}

void setRelayAutoModeDisabled(
    bool state
)
{
    relayAutoModeDisabled =
        state;
}