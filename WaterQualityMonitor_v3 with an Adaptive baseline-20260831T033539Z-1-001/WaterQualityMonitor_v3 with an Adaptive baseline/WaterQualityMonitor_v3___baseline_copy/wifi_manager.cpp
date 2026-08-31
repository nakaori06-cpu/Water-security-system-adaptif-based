#include "wifi_manager.h"

#include <WiFi.h>

#include "config.h"
#include "secrets.h"

// ==========================================================
// WIFI STATE
// ==========================================================

static bool wifiWasConnected = false;

static unsigned long wifiConnectedSinceMs = 0;
static unsigned long lastWiFiRetryMs = 0;
static unsigned long lastWiFiStatusMessageMs = 0;

// ==========================================================
// INITIALIZATION
// ==========================================================

void initWiFi()
{
    Serial.println();
    Serial.println(
        "================================="
    );
    Serial.println(
        " Initializing Wi-Fi"
    );
    Serial.println(
        "================================="
    );

    WiFi.mode(WIFI_STA);

    // Allow the ESP32 Wi-Fi stack to reconnect automatically.
    WiFi.setAutoReconnect(true);

    // Prevent credentials from being stored persistently
    // in flash by the Wi-Fi library.
    WiFi.persistent(false);

    WiFi.begin(
        WIFI_SSID,
        WIFI_PASSWORD
    );

    lastWiFiRetryMs = millis();
    lastWiFiStatusMessageMs = millis();

    Serial.print(
        "[WiFi] Connecting to: "
    );
    Serial.println(WIFI_SSID);

    Serial.println(
        "[WiFi] Connection is handled asynchronously"
    );
}

// ==========================================================
// UPDATE
// ==========================================================

void updateWiFi()
{
    const unsigned long now =
        millis();

    const wl_status_t status =
        WiFi.status();

    if (status == WL_CONNECTED)
    {
        if (!wifiWasConnected)
        {
            wifiWasConnected = true;
            wifiConnectedSinceMs = now;

            Serial.println();
            Serial.println(
                "================================="
            );
            Serial.println(
                "[WiFi] Connected"
            );

            Serial.print(
                "[WiFi] SSID: "
            );
            Serial.println(
                WiFi.SSID()
            );

            Serial.print(
                "[WiFi] IP: "
            );
            Serial.println(
                WiFi.localIP()
            );

            Serial.print(
                "[WiFi] RSSI: "
            );
            Serial.print(
                WiFi.RSSI()
            );
            Serial.println(
                " dBm"
            );

            Serial.println(
                "================================="
            );
        }

        return;
    }

    if (wifiWasConnected)
    {
        wifiWasConnected = false;
        wifiConnectedSinceMs = 0;

        Serial.println();
        Serial.println(
            "[WiFi] Disconnected"
        );
    }

    // Retry without blocking the main loop.
    if (
        now - lastWiFiRetryMs >=
        WIFI_RECONNECT_INTERVAL_MS
    )
    {
        lastWiFiRetryMs = now;

        Serial.println(
            "[WiFi] Reconnect requested"
        );

        WiFi.reconnect();
    }

    if (
        now - lastWiFiStatusMessageMs >=
        WIFI_STATUS_PRINT_INTERVAL_MS
    )
    {
        lastWiFiStatusMessageMs = now;

        Serial.print(
            "[WiFi] Waiting. Status code: "
        );
        Serial.println(
            (int)status
        );
    }
}

// ==========================================================
// STATUS
// ==========================================================

bool isWiFiConnected()
{
    return
        wifiWasConnected &&
        WiFi.status() == WL_CONNECTED;
}

String getWiFiSSID()
{
    if (!isWiFiConnected())
    {
        return "Not Connected";
    }

    return WiFi.SSID();
}

String getWiFiIP()
{
    if (!isWiFiConnected())
    {
        return "N/A";
    }

    return WiFi.localIP().toString();
}

unsigned long getWiFiConnectedTime()
{
    if (!isWiFiConnected())
    {
        return 0;
    }

    return
        (
            millis() -
            wifiConnectedSinceMs
        ) /
        1000UL;
}