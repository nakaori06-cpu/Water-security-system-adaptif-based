#include "rtc_manager.h"

#include <Wire.h>
#include <RTClib.h>

#include "config.h"

// ==========================================================
// RTC STATE
// ==========================================================

static RTC_DS3231 rtc;

static DateTime cachedDateTime(
    2000,
    1,
    1,
    0,
    0,
    0
);

static bool rtcReady = false;

static unsigned long lastRTCUpdateMs = 0;

static const unsigned long RTC_UPDATE_INTERVAL_MS =
    1000UL;

// ==========================================================
// HELPERS
// ==========================================================

static bool probeRTC()
{
    Wire.beginTransmission(RTC_I2C_ADDRESS);

    return Wire.endTransmission() == 0;
}

// ==========================================================
// INITIALIZATION
// ==========================================================

void initRTC()
{
    rtcReady = false;

    Serial.println();
    Serial.println("=================================");
    Serial.println(" Initializing DS3231 RTC");
    Serial.println("=================================");

    if (!probeRTC())
    {
        Serial.print("[FAIL] RTC not found at address 0x");
        Serial.println(RTC_I2C_ADDRESS, HEX);

        Serial.println(
            "[INFO] System will continue without RTC"
        );

        return;
    }

    if (!rtc.begin(&Wire))
    {
        Serial.println(
            "[FAIL] RTClib could not initialize DS3231"
        );

        return;
    }

    rtcReady = true;

    if (rtc.lostPower())
    {
        Serial.println("[WARN] RTC lost power");

        Serial.println(
            "[INFO] Setting RTC to firmware compile time"
        );

        rtc.adjust(
            DateTime(
                F(__DATE__),
                F(__TIME__)
            )
        );
    }

    cachedDateTime = rtc.now();
    lastRTCUpdateMs = millis();

    Serial.println("[ OK ] DS3231 initialized");

    Serial.print("[RTC] Date: ");
    Serial.println(getDateString());

    Serial.print("[RTC] Time: ");
    Serial.println(getTimeString());

    Serial.print("[RTC] Temperature: ");
    Serial.print(rtc.getTemperature(), 2);
    Serial.println(" C");

    Serial.println("=================================");
}

// ==========================================================
// UPDATE
// ==========================================================

void updateRTC()
{
    if (!rtcReady)
    {
        return;
    }

    const unsigned long now =
        millis();

    if (
        now - lastRTCUpdateMs <
        RTC_UPDATE_INTERVAL_MS
    )
    {
        return;
    }

    lastRTCUpdateMs = now;

    cachedDateTime = rtc.now();
}

// ==========================================================
// STATUS
// ==========================================================

bool rtcAvailable()
{
    return rtcReady;
}

// ==========================================================
// FORMATTED DATE
// ==========================================================

String getDateString()
{
    if (!rtcReady)
    {
        return "RTC N/A";
    }

    char buffer[16];

    snprintf(
        buffer,
        sizeof(buffer),
        "%02d/%02d/%04d",
        cachedDateTime.day(),
        cachedDateTime.month(),
        cachedDateTime.year()
    );

    return String(buffer);
}

// ==========================================================
// FORMATTED TIME
// ==========================================================

String getTimeString()
{
    if (!rtcReady)
    {
        return "RTC N/A";
    }

    char buffer[16];

    snprintf(
        buffer,
        sizeof(buffer),
        "%02d:%02d:%02d",
        cachedDateTime.hour(),
        cachedDateTime.minute(),
        cachedDateTime.second()
    );

    return String(buffer);
}

// ==========================================================
// EPOCH
// ==========================================================

uint32_t getRTCEpoch()
{
    if (!rtcReady)
    {
        return 0;
    }

    return cachedDateTime.unixtime();
}