#include "display_oled.h"

#include <Wire.h>

#include "config.h"
#include "rtc_manager.h"
#include "trend_engine.h"

// ==========================================================
// OLED OBJECT
// ==========================================================

static Adafruit_SSD1306 display(
    OLED_WIDTH,
    OLED_HEIGHT,
    &Wire,
    OLED_RESET
);

static DisplayOLED oledDisplay;

static bool oledReady = false;

// ==========================================================
// INITIALIZATION
// ==========================================================

bool DisplayOLED::initialize()
{
    if (
        !display.begin(
            SSD1306_SWITCHCAPVCC,
            OLED_I2C_ADDRESS
        )
    )
    {
        Serial.println(
            "[OLED] Initialization failed"
        );

        oledReady = false;

        return false;
    }

    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println("Water Monitor");
    display.println("Initializing...");
    display.display();

    oledReady = true;

    Serial.println(
        "[OLED] Initialized"
    );

    return true;
}

// ==========================================================
// STATUS PAGE
// ==========================================================

void DisplayOLED::displayStatus(
    const SensorData &sensor,
    const WaterQuality &water,
    const DecisionState &decision
)
{
    if (!oledReady)
    {
        return;
    }

    char line[32];

    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);

    display.println("WATER MONITOR");

    // ------------------------------------------------------
    // Warm-up state
    // ------------------------------------------------------

    if (
        water.status == WATER_WARMUP ||
        !sensor.allRequiredSensorsValid
    )
    {
        display.println("STATUS: WARMUP");

        display.print("Valid: ");
        display.print(sensor.validSampleCount);
        display.print("/");
        display.println(SENSOR_WARMUP_VALID_SAMPLES);

        display.print("Relay: ");
        display.println(
            decision.relay == RELAY_ON
                ? "ON"
                : "OFF"
        );

        display.println("");
        display.println("Waiting for");
        display.println("valid sensors");

        display.display();

        return;
    }

    // ------------------------------------------------------
    // Normal status
    // ------------------------------------------------------

    snprintf(
        line,
        sizeof(line),
        "STATUS: %s",
        water.statusText.c_str()
    );

    display.println(line);

    snprintf(
        line,
        sizeof(line),
        "Score: %.0f%%  C:%0.f%%",
        water.score,
        water.confidence
    );

    display.println(line);

    display.println("");

    snprintf(
        line,
        sizeof(line),
        "pH: %.2f",
        sensor.ph
    );

    display.println(line);

    snprintf(
        line,
        sizeof(line),
        "TDS: %.0f ppm",
        sensor.tds
    );

    display.println(line);

    snprintf(
        line,
        sizeof(line),
        "NTU: %.1f",
        sensor.ntu
    );

    display.println(line);

    snprintf(
        line,
        sizeof(line),
        "Temp: %.1f C",
        sensor.temperature
    );

    display.println(line);

    display.display();
}

// ==========================================================
// INDICATOR PAGE
// ==========================================================

void DisplayOLED::displayIndicators(
    const SensorData &sensor,
    const WaterQuality &water,
    const DecisionState &decision
)
{
    if (!oledReady)
    {
        return;
    }

    const TrendResult trend =
        getTrendResult();

    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);

    display.println("SYSTEM STATUS");

    display.print("ADS1115: ");
    display.println(
        sensor.ads1115OK
            ? "OK"
            : "FAIL"
    );

    display.print("DS18B20: ");
    display.println(
        sensor.ds18b20OK
            ? "OK"
            : "FAIL"
    );

    display.print("RTC: ");
    display.println(
        rtcAvailable()
            ? "OK"
            : "FAIL"
    );

    display.print("OLED: ");
    display.println("OK");

    display.print("TFT: ");
    display.println("OK");

    display.print("Relay: ");
    display.println(
        decision.relay == RELAY_ON
            ? "ON"
            : "OFF"
    );

    display.print("Buzzer: ");
    display.println(
        decision.buzzer == BUZZER_ON
            ? "ON"
            : "OFF"
    );

    display.print("Trend: ");
    display.println(
        getTrendStatusText()
    );

    display.print("Alarm: ");
    display.println(
        trend.absoluteAlarmActive ||
        water.alarm
            ? "YES"
            : "NO"
    );

    display.display();
}

// ==========================================================
// CLEAR
// ==========================================================

void DisplayOLED::clear()
{
    if (!oledReady)
    {
        return;
    }

    display.clearDisplay();
    display.display();
}

// ==========================================================
// STATUS SYMBOL
// ==========================================================

const char *DisplayOLED::getStatusSymbol(
    WaterStatus status
)
{
    switch (status)
    {
        case EXCELLENT:
        case GOOD:
            return "OK";

        case FAIR:
            return "!";

        case POOR:
        case UNSAFE:
            return "!!";

        case WATER_WARMUP:
        default:
            return "?";
    }
}

// ==========================================================
// PUBLIC INITIALIZATION
// ==========================================================

void initOLED()
{
    oledDisplay.initialize();
}

// ==========================================================
// PUBLIC UPDATE
// ==========================================================

void updateOLED(
    const SensorData &sensor,
    const WaterQuality &water,
    const DecisionState &decision
)
{
    static unsigned long lastPageSwitchMs = 0;
    static bool showIndicators = false;

    const unsigned long now =
        millis();

    if (
        now - lastPageSwitchMs >=
        OLED_PAGE_INTERVAL_MS
    )
    {
        lastPageSwitchMs = now;
        showIndicators = !showIndicators;
    }

    if (showIndicators)
    {
        oledDisplay.displayIndicators(
            sensor,
            water,
            decision
        );
    }
    else
    {
        oledDisplay.displayStatus(
            sensor,
            water,
            decision
        );
    }
}