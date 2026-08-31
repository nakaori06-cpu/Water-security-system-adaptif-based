#include "display_tft.h"

#include <math.h>
#include <SPI.h>
#include <TFT_eSPI.h>

#include "config.h"
#include "rtc_manager.h"
#include "wifi_manager.h"
#include "trend_engine.h"
#include "touch_manager.h"

// ==========================================================
// TFT OBJECT
// ==========================================================
//
// TFT_eSPI must be configured through its selected
// User_Setup.h file:
//
// ST7789_DRIVER
// TFT_WIDTH  240
// TFT_HEIGHT 240
// TFT_CS     -1
// TFT_DC     16
// TFT_RST    17
// TFT_SCLK   18
// TFT_MOSI   23
// TFT_ROTATION 0
//

static TFT_eSPI tft;

static bool tftReady = false;
static bool tftSleeping = false;

// ==========================================================
// PAGES
// ==========================================================

enum TFTPage
{
    TFT_PAGE_DASHBOARD = 0,
    TFT_PAGE_DIAGNOSTIC,
    TFT_PAGE_WIFI,
    TFT_PAGE_TREND_GRAPH,
    TFT_PAGE_SECRET_TREND,
    TFT_PAGE_ADAPTIVE_BASELINE
};

static TFTPage currentPage =
    TFT_PAGE_DASHBOARD;

// ==========================================================
// TOUCH STATE
// ==========================================================

static bool lastTouchState = false;
static bool longHoldTriggered = false;

static unsigned long touchStartedMs = 0;
static unsigned long lastTouchActivityMs = 0;
static unsigned long lastTouchReleaseMs = 0;

static const unsigned long TFT_TOUCH_DEBOUNCE_MS =
    TOUCH_DEBOUNCE_MS;

// ==========================================================
// DRAWING
// ==========================================================

static const uint16_t TITLE_HEIGHT = 28;
static const uint16_t ROW_HEIGHT = 14;

static float waveOffset = 0.0f;

static unsigned long lastDashboardFrameMs = 0;

static const unsigned long DASHBOARD_FRAME_INTERVAL_MS =
    80UL;

static unsigned long lastStaticPageMs = 0;

static const unsigned long STATIC_PAGE_INTERVAL_MS =
    500UL;

// ==========================================================
// COLORS
// ==========================================================

static uint16_t waterColor(
    WaterStatus status
)
{
    switch (status)
    {
        case EXCELLENT:
        case GOOD:
            return TFT_GREEN;

        case FAIR:
            return TFT_YELLOW;

        case POOR:
        case UNSAFE:
            return TFT_RED;

        case WATER_WARMUP:
        default:
            return TFT_LIGHTGREY;
    }
}

static uint16_t trendColor(
    TrendStatus status
)
{
    switch (status)
    {
        case TREND_NORMAL:
            return TFT_GREEN;

        case TREND_WATCH:
            return TFT_YELLOW;

        case TREND_NEED_MAINTENANCE:
            return TFT_ORANGE;

        case TREND_WARNING:
            return TFT_ORANGE;

        case TREND_ABSOLUTE_ALARM:
            return TFT_RED;

        case TREND_DISABLED:
        case TREND_COLLECTING:
        default:
            return TFT_LIGHTGREY;
    }
}

static uint16_t pointsColor(
    uint8_t points
)
{
    if (points >= 4)
    {
        return TFT_RED;
    }
    else if (points >= 3)
    {
        return TFT_ORANGE;
    }
    else if (points >= 1)
    {
        return TFT_YELLOW;
    }
    else
    {
        return TFT_GREEN;
    }
}

// ==========================================================
// DRAWING HELPERS
// ==========================================================

static void drawTitle(
    const char *title
)
{
    tft.fillRect(
        0,
        0,
        TFT_WIDTH,
        TITLE_HEIGHT,
        TFT_DARKGREEN
    );

    tft.setTextSize(2);
    tft.setTextColor(
        TFT_WHITE,
        TFT_DARKGREEN
    );

    tft.setCursor(8, 6);
    tft.println(title);
}

static void printRow(
    uint16_t y,
    const char *label,
    const String &value,
    uint16_t color = TFT_WHITE
)
{
    tft.setTextSize(1);

    tft.setTextColor(
        TFT_LIGHTGREY,
        TFT_BLACK
    );

    tft.setCursor(8, y);
    tft.print(label);

    tft.setTextColor(
        color,
        TFT_BLACK
    );

    tft.println(value);
}

static String modeText(
    SystemMode mode
)
{
    switch (mode)
    {
        case MODE_AUTO:
            return "AUTO";

        case MODE_MANUAL_ON:
            return "MAN-ON";

        case MODE_MANUAL_OFF:
            return "MAN-OFF";
    }

    return "UNKNOWN";
}

// ==========================================================
// DASHBOARD
// ==========================================================

static void drawDashboard(
    const SensorData &sensor,
    const WaterQuality &water
)
{
    tft.fillScreen(TFT_BLACK);

    drawTitle("DASHBOARD");

    // ------------------------------------------------------
    // Water status
    // ------------------------------------------------------

    tft.fillRect(
        0,
        TITLE_HEIGHT,
        TFT_WIDTH,
        32,
        TFT_BLACK
    );

    tft.setCursor(
        8,
        TITLE_HEIGHT + 4
    );

    tft.setTextSize(2);
    tft.setTextColor(
        waterColor(water.status),
        TFT_BLACK
    );

    if (
        water.status == WATER_WARMUP ||
        !sensor.allRequiredSensorsValid
    )
    {
        tft.println("WARMUP");
    }
    else
    {
        tft.print(water.statusText);
        tft.print(" ");
        tft.print((int)water.score);
        tft.println("%");
    }

    // Display points indicator
    const TrendResult trend =
        getTrendResult();

    tft.setTextSize(1);
    tft.setTextColor(
        pointsColor(trend.trendPoints),
        TFT_BLACK
    );

    tft.setCursor(200, TITLE_HEIGHT + 6);
    tft.print("P:");
    tft.println(trend.trendPoints);

    // Display adaptive baseline warning if active
    if (trend.adaptiveBaseline.warningActive)
    {
        tft.setTextSize(1);
        tft.setTextColor(TFT_RED, TFT_BLACK);
        tft.setCursor(200, TITLE_HEIGHT + 16);
        tft.println("ABW!");
    }

    // ------------------------------------------------------
    // Wave graph
    // ------------------------------------------------------

    const int graphTop =
        TITLE_HEIGHT + 38;

    const int graphBottom =
        210;

    const int graphHeight =
        graphBottom - graphTop;

    const float normalizedTDS =
        constrain(
            sensor.tds / TDS_MAX_PPM,
            0.0f,
            1.0f
        );

    const float amplitude =
        8.0f +
        normalizedTDS * 35.0f;

    for (
        int x = 0;
        x < TFT_WIDTH;
        x++
    )
    {
        float wave =
            sinf(
                x * 0.06f +
                waveOffset
            ) *
            amplitude;

        wave +=
            sinf(
                x * 0.12f +
                waveOffset
            ) *
            amplitude *
            0.35f;

        int y =
            graphTop +
            graphHeight / 2 -
            (int)wave;

        y = constrain(
            y,
            graphTop,
            graphBottom
        );

        tft.drawFastVLine(
            x,
            y,
            graphBottom - y,
            TFT_CYAN
        );
    }

    waveOffset += 0.15f;

    // ------------------------------------------------------
    // Bottom values
    // ------------------------------------------------------

    tft.setTextSize(1);
    tft.setTextColor(
        TFT_WHITE,
        TFT_BLACK
    );

    tft.setCursor(8, 218);

    if (sensor.phValid)
    {
        tft.print("pH:");
        tft.print(sensor.ph, 2);
    }
    else
    {
        tft.print("pH:--");
    }

    tft.print(" ");

    if (sensor.tdsValid)
    {
        tft.print("TDS:");
        tft.print(sensor.tds, 0);
    }
    else
    {
        tft.print("TDS:--");
    }

    tft.print(" ");

    if (sensor.temperatureValid)
    {
        tft.print("T:");
        tft.print(sensor.temperature, 1);
        tft.print("C");
    }
    else
    {
        tft.print("T:--");
    }
}

// ==========================================================
// DIAGNOSTIC PAGE
// ==========================================================

static void drawDiagnostic(
    const SensorData &sensor,
    const WaterQuality &water,
    const DecisionState &decision
)
{
    const TrendResult trend =
        getTrendResult();

    tft.fillScreen(TFT_BLACK);

    drawTitle("DIAGNOSTIC");

    uint16_t y =
        TITLE_HEIGHT + 6;

    tft.setTextSize(1);
    tft.setTextColor(
        TFT_LIGHTGREY,
        TFT_BLACK
    );

    tft.setCursor(8, y);
    tft.println("=== HARDWARE ===");

    y += ROW_HEIGHT;

    printRow(
        y,
        "ADS1115: ",
        sensor.ads1115OK
            ? "OK"
            : "FAIL",
        sensor.ads1115OK
            ? TFT_GREEN
            : TFT_RED
    );

    y += ROW_HEIGHT;

    printRow(
        y,
        "DS18B20: ",
        sensor.ds18b20OK
            ? "OK"
            : "FAIL",
        sensor.ds18b20OK
            ? TFT_GREEN
            : TFT_YELLOW
    );

    y += ROW_HEIGHT;

    printRow(
        y,
        "RTC: ",
        rtcAvailable()
            ? "OK"
            : "FAIL",
        rtcAvailable()
            ? TFT_GREEN
            : TFT_YELLOW
    );

    y += ROW_HEIGHT;

    printRow(
        y,
        "Sensor warmup: ",
        sensor.warmupComplete
            ? "DONE"
            : "ACTIVE",
        sensor.warmupComplete
            ? TFT_GREEN
            : TFT_YELLOW
    );

    y += ROW_HEIGHT + 2;

    tft.setTextColor(
        TFT_LIGHTGREY,
        TFT_BLACK
    );

    tft.setCursor(8, y);
    tft.println("=== CONTROL ===");

    y += ROW_HEIGHT;

    printRow(
        y,
        "Relay: ",
        decision.relay == RELAY_ON
            ? "ON"
            : "OFF",
        decision.relay == RELAY_ON
            ? TFT_GREEN
            : TFT_RED
    );

    y += ROW_HEIGHT;

    printRow(
        y,
        "Buzzer: ",
        decision.buzzer == BUZZER_ON
            ? "ON"
            : "OFF",
        decision.buzzer == BUZZER_ON
            ? TFT_RED
            : TFT_GREEN
    );

    y += ROW_HEIGHT;

    printRow(
        y,
        "Mode: ",
        modeText(decision.mode),
        TFT_WHITE
    );

    y += ROW_HEIGHT;

    printRow(
        y,
        "Maintenance: ",
        decision.filterMaintenance
            ? "YES"
            : "NO",
        decision.filterMaintenance
            ? TFT_YELLOW
            : TFT_GREEN
    );

    y += ROW_HEIGHT + 2;

    tft.setTextColor(
        TFT_LIGHTGREY,
        TFT_BLACK
    );

    tft.setCursor(8, y);
    tft.println("=== POINTS ===");

    y += ROW_HEIGHT;

    printRow(
        y,
        "Score: ",
        String(trend.trendPoints) +
        " / " +
        String(TREND_MAX_POINTS),
        pointsColor(trend.trendPoints)
    );

    y += ROW_HEIGHT;

    printRow(
        y,
        "Status: ",
        getTrendStatusText(),
        trendColor(trend.status)
    );
}

// ==========================================================
// WIFI PAGE
// ==========================================================

static void drawWiFiPage()
{
    tft.fillScreen(TFT_BLACK);

    drawTitle("NETWORK");

    uint16_t y =
        TITLE_HEIGHT + 8;

    const bool connected =
        isWiFiConnected();

    printRow(
        y,
        "WiFi: ",
        connected
            ? "CONNECTED"
            : "DISCONNECTED",
        connected
            ? TFT_GREEN
            : TFT_RED
    );

    y += ROW_HEIGHT + 6;

    printRow(
        y,
        "SSID: ",
        getWiFiSSID(),
        TFT_WHITE
    );

    y += ROW_HEIGHT;

    printRow(
        y,
        "IP: ",
        getWiFiIP(),
        TFT_WHITE
    );

    y += ROW_HEIGHT + 6;

    if (connected)
    {
        printRow(
            y,
            "Online: ",
            String(
                getWiFiConnectedTime()
            ) + " sec",
            TFT_CYAN
        );
    }
    else
    {
        printRow(
            y,
            "Date: ",
            getDateString(),
            TFT_YELLOW
        );

        y += ROW_HEIGHT;

        printRow(
            y,
            "Time: ",
            getTimeString(),
            TFT_YELLOW
        );
    }
}

// ==========================================================
// TREND GRAPH
// ==========================================================

static void drawTrendGraph()
{
    const TrendChartData chart =
        getTrendChartData();

    const TrendResult trend =
        getTrendResult();

    tft.fillScreen(TFT_BLACK);

    drawTitle("TDS GRAPH");

    const int graphLeft = 10;
    const int graphRight = 230;
    const int graphTop = 45;
    const int graphBottom = 155;

    const int graphWidth =
        graphRight - graphLeft;

    const int graphHeight =
        graphBottom - graphTop;

    tft.drawRect(
        graphLeft,
        graphTop,
        graphWidth,
        graphHeight,
        TFT_DARKGREY
    );

    tft.setTextSize(1);
    tft.setTextColor(
        TFT_LIGHTGREY,
        TFT_BLACK
    );

    tft.setCursor(10, 32);
    tft.print("Saved hours: ");
    tft.println(chart.hourlyCount);

    float highest =
        trend.absoluteSafetyLimit;

    if (highest < 1.0f)
    {
        highest = 1.0f;
    }

    for (
        uint8_t i = 0;
        i < chart.hourlyCount;
        i++
    )
    {
        if (
            chart.hourlyValues[i] >
            highest
        )
        {
            highest =
                chart.hourlyValues[i];
        }
    }

    // Safety threshold line (red).
    const int safetyY =
        graphBottom -
        (int)(
            (
                trend.absoluteSafetyLimit /
                highest
            ) *
            graphHeight
        );

    if (
        safetyY >= graphTop &&
        safetyY <= graphBottom
    )
    {
        tft.drawFastHLine(
            graphLeft,
            safetyY,
            graphWidth,
            TFT_RED
        );
    }

    // Adaptive baseline threshold line (orange).
    if (trend.adaptiveBaseline.isActive)
    {
        const int adaptiveY =
            graphBottom -
            (int)(
                (
                    trend.adaptiveBaseline.thresholdTDS /
                    highest
                ) *
                graphHeight
            );

        if (
            adaptiveY >= graphTop &&
            adaptiveY <= graphBottom
        )
        {
            tft.drawFastHLine(
                graphLeft,
                adaptiveY,
                graphWidth,
                TFT_ORANGE
            );
        }
    }

    // Trend line.
    if (chart.hourlyCount >= 2)
    {
        const int denominator =
            chart.hourlyCount - 1;

        for (
            uint8_t i = 1;
            i < chart.hourlyCount;
            i++
        )
        {
            const int x1 =
                graphLeft +
                (
                    (int)(i - 1) *
                    graphWidth
                ) /
                denominator;

            const int x2 =
                graphLeft +
                (
                    (int)i *
                    graphWidth
                ) /
                denominator;

            int y1 =
                graphBottom -
                (int)(
                    (
                        chart.hourlyValues[i - 1] /
                        highest
                    ) *
                    graphHeight
                );

            int y2 =
                graphBottom -
                (int)(
                    (
                        chart.hourlyValues[i] /
                        highest
                    ) *
                    graphHeight
                );

            y1 = constrain(
                y1,
                graphTop,
                graphBottom
            );

            y2 = constrain(
                y2,
                graphTop,
                graphBottom
            );

            tft.drawLine(
                x1,
                y1,
                x2,
                y2,
                TFT_CYAN
            );
        }
    }

    uint16_t y = 165;

    String row1 =
        String("1H:") +
        String(chart.average1Hour, 0) +
        " 3H:" +
        String(chart.average3Hour, 0);

    printRow(
        y,
        "",
        row1,
        TFT_WHITE
    );

    y += ROW_HEIGHT;

    String row2 =
        String("6H:") +
        String(chart.average6Hour, 0) +
        " 12H:" +
        String(chart.average12Hour, 0);

    printRow(
        y,
        "",
        row2,
        TFT_WHITE
    );

    y += ROW_HEIGHT;

    String row3 =
        String("24H:") +
        String(chart.average24Hour, 0) +
        " ppm";

    printRow(
        y,
        "",
        row3,
        TFT_WHITE
    );

    y += ROW_HEIGHT;

    printRow(
        y,
        "Status: ",
        getTrendStatusText(),
        trendColor(trend.status)
    );
}

// ==========================================================
// SECRET TREND DETAILS
// ==========================================================

static void drawSecretTrendPage()
{
    const TrendResult trend =
        getTrendResult();

    tft.fillScreen(TFT_BLACK);

    drawTitle("TREND DETAILS");

    uint16_t y =
        TITLE_HEIGHT + 5;

    printRow(
        y,
        "Point Score: ",
        String(trend.trendPoints) +
        " / " +
        String(TREND_MAX_POINTS),
        pointsColor(trend.trendPoints)
    );

    y += ROW_HEIGHT;

    printRow(
        y,
        "Status: ",
        getTrendStatusText(),
        trendColor(trend.status)
    );

    y += ROW_HEIGHT;

    printRow(
        y,
        "Week-0: ",
        String(trend.week0Baseline, 1) +
        " ppm"
    );

    y += ROW_HEIGHT;

    printRow(
        y,
        "Prev Week: ",
        String(trend.previousWeekAverage, 1) +
        " ppm"
    );

    y += ROW_HEIGHT;

    printRow(
        y,
        "Current Week: ",
        String(trend.currentWeekAverage, 1) +
        " ppm"
    );

    y += ROW_HEIGHT;

    printRow(
        y,
        "+50% Gate: ",
        String(trend.baselineWarningGate, 1) +
        " ppm",
        TFT_YELLOW
    );

    y += ROW_HEIGHT;

    printRow(
        y,
        "80% Gate: ",
        String(trend.eightyPercentWarningGate, 1) +
        " ppm",
        TFT_ORANGE
    );

    y += ROW_HEIGHT;

    printRow(
        y,
        "ABS STOP: ",
        String(trend.absoluteSafetyLimit, 1) +
        " ppm",
        TFT_RED
    );

    y += ROW_HEIGHT;

    printRow(
        y,
        "Live TDS: ",
        String(trend.currentTDS, 1) +
        " ppm",
        TFT_CYAN
    );

    y += ROW_HEIGHT;

    printRow(
        y,
        "Records: ",
        String("H:") +
        String(trend.hourlyRecordCount) +
        "/24 D:" +
        String(trend.dailyRecordCount) +
        "/7"
    );

    y += ROW_HEIGHT;

    printRow(
        y,
        "Last Save: ",
        trend.lastSaveDate
    );

    y += ROW_HEIGHT;

    printRow(
        y,
        "Time: ",
        trend.lastSaveTime
    );
}

// ==========================================================
// ADAPTIVE BASELINE PAGE
// ==========================================================

static void drawAdaptiveBaselinePage()
{
    const TrendResult trend =
        getTrendResult();

    tft.fillScreen(TFT_BLACK);

    drawTitle("ADAPTIVE BASE");

    uint16_t y =
        TITLE_HEIGHT + 6;

    tft.setTextSize(1);
    tft.setTextColor(
        TFT_LIGHTGREY,
        TFT_BLACK
    );

    tft.setCursor(8, y);
    tft.println("=== STATUS ===");

    y += ROW_HEIGHT;

    printRow(
        y,
        "Active: ",
        trend.adaptiveBaseline.isActive
            ? "YES"
            : "NO",
        trend.adaptiveBaseline.isActive
            ? TFT_GREEN
            : TFT_LIGHTGREY
    );

    y += ROW_HEIGHT;

    printRow(
        y,
        "Stable: ",
        trend.adaptiveBaseline.isStable
            ? "YES"
            : "NO",
        trend.adaptiveBaseline.isStable
            ? TFT_GREEN
            : TFT_YELLOW
    );

    y += ROW_HEIGHT;

    printRow(
        y,
        "Warning: ",
        trend.adaptiveBaseline.warningActive
            ? "YES"
            : "NO",
        trend.adaptiveBaseline.warningActive
            ? TFT_RED
            : TFT_GREEN
    );

    y += ROW_HEIGHT + 2;

    tft.setTextColor(
        TFT_LIGHTGREY,
        TFT_BLACK
    );

    tft.setCursor(8, y);
    tft.println("=== VALUES ===");

    y += ROW_HEIGHT;

    printRow(
        y,
        "Baseline TDS: ",
        String(
            trend.adaptiveBaseline.baselineTDS,
            1
        ) + " ppm",
        TFT_CYAN
    );

    y += ROW_HEIGHT;

    printRow(
        y,
        "Threshold TDS: ",
        String(
            trend.adaptiveBaseline.thresholdTDS,
            1
        ) + " ppm",
        TFT_YELLOW
    );

    y += ROW_HEIGHT;

    printRow(
        y,
        "Current TDS: ",
        String(trend.currentTDS, 1) + " ppm",
        trend.currentTDS >=
        trend.adaptiveBaseline.thresholdTDS
            ? TFT_RED
            : TFT_WHITE
    );

    y += ROW_HEIGHT + 2;

    tft.setTextColor(
        TFT_LIGHTGREY,
        TFT_BLACK
    );

    tft.setCursor(8, y);
    tft.println("=== pH RANGE ===");

    y += ROW_HEIGHT;

    printRow(
        y,
        "Low: ",
        String(
            trend.adaptiveBaseline.thresholdPHLow,
            2
        ),
        TFT_WHITE
    );

    y += ROW_HEIGHT;

    printRow(
        y,
        "High: ",
        String(
            trend.adaptiveBaseline.thresholdPHHigh,
            2
        ),
        TFT_WHITE
    );

    y += ROW_HEIGHT;

    printRow(
        y,
        "Current: ",
        String(trend.currentPH, 2),
        (
            trend.currentPH <
            trend.adaptiveBaseline.thresholdPHLow ||
            trend.currentPH >
            trend.adaptiveBaseline.thresholdPHHigh
        )
            ? TFT_RED
            : TFT_GREEN
    );

    y += ROW_HEIGHT + 2;

    tft.setTextColor(
        TFT_LIGHTGREY,
        TFT_BLACK
    );

    tft.setCursor(8, y);
    tft.println("=== NTU THRES ===");

    y += ROW_HEIGHT;

    printRow(
        y,
        "Threshold: ",
        String(
            trend.adaptiveBaseline.thresholdNTU,
            1
        ),
        TFT_WHITE
    );

    y += ROW_HEIGHT;

    printRow(
        y,
        "Current: ",
        String(trend.currentNTU, 1),
        trend.currentNTU >=
        trend.adaptiveBaseline.thresholdNTU
            ? TFT_RED
            : TFT_GREEN
    );
}

// ==========================================================
// SLEEP
// ==========================================================

static void drawSleepScreen()
{
    tft.fillScreen(TFT_BLACK);

    tft.setTextColor(
        TFT_DARKGREY,
        TFT_BLACK
    );

    tft.setTextSize(2);
    tft.setCursor(55, 110);
    tft.println("SLEEP");
}

static void enterSleep()
{
    if (tftSleeping)
    {
        return;
    }

    tftSleeping = true;

    drawSleepScreen();

    Serial.println(
        "[TFT] Entered sleep screen"
    );
}

static void wakeUp()
{
    if (!tftSleeping)
    {
        return;
    }

    tftSleeping = false;
    lastTouchActivityMs = millis();

    // Force a redraw immediately after wake.
    lastStaticPageMs = 0;
    lastDashboardFrameMs = 0;

    Serial.println(
        "[TFT] Woke up"
    );
}

// ==========================================================
// PAGE NAVIGATION
// ==========================================================

static void nextNormalPage()
{
    if (
        currentPage ==
        TFT_PAGE_SECRET_TREND
    )
    {
        currentPage =
            TFT_PAGE_DASHBOARD;

        return;
    }

    const uint8_t next =
        (
            (uint8_t)currentPage + 1U
        ) %
        5U;

    currentPage =
        (TFTPage)next;
}

// ==========================================================
// TOUCH HANDLER
// ==========================================================

static void handleTFTTouch()
{
    const unsigned long now =
        millis();

    const bool touched =
        digitalRead(TOUCH_TFT_PIN) ==
        TOUCH_INPUT_ACTIVE_LEVEL;

    if (touched)
    {
        lastTouchActivityMs = now;

        if (!lastTouchState)
        {
            touchStartedMs = now;
            longHoldTriggered = false;
        }

        if (
            !tftSleeping &&
            !longHoldTriggered &&
            now - touchStartedMs >=
            TFT_SECRET_PAGE_HOLD_MS
        )
        {
            currentPage =
                TFT_PAGE_SECRET_TREND;

            longHoldTriggered = true;

            Serial.println(
                "[TFT] Secret trend page opened"
            );
        }
    }
    else if (lastTouchState)
    {
        // Input transitioned from HIGH to LOW.
        if (tftSleeping)
        {
            wakeUp();
        }
        else if (
            !longHoldTriggered &&
            now - lastTouchReleaseMs >=
            TFT_TOUCH_DEBOUNCE_MS
        )
        {
            lastTouchReleaseMs = now;

            nextNormalPage();

            Serial.print(
                "[TFT] Page: "
            );

            Serial.println(
                (uint8_t)currentPage
            );
        }
    }

    lastTouchState = touched;

    if (
        !tftSleeping &&
        now - lastTouchActivityMs >=
        TFT_SLEEP_TIMEOUT_MS
    )
    {
        enterSleep();
    }
}

// ==========================================================
// INITIALIZATION
// ==========================================================

void initTFT()
{
    Serial.println(
        "[TFT] Initializing ST7789"
    );

    // Preserve the existing TFT SPI pins.
    SPI.begin(
        TFT_SCLK,
        TFT_MISO,
        TFT_MOSI
    );

    tft.init();

    // Preserve rotation 0.
    tft.setRotation(
        TFT_ROTATION
    );

    tft.fillScreen(
        TFT_BLACK
    );

    tftReady = true;
    tftSleeping = false;

    lastTouchActivityMs = millis();

    // GPIO35 is input-only and requires
    // an external electrical bias.
    pinMode(
        TOUCH_TFT_PIN,
        INPUT
    );

    Serial.println(
        "[TFT] Ready"
    );

    Serial.println(
        "[TFT] Pages: Dashboard, Diagnostic, Network, Graph, Trend, Adaptive Base"
    );

    Serial.println(
        "[TFT] GPIO35 short press: next page"
    );

    Serial.println(
        "[TFT] GPIO35 hold 10 seconds: Trend Details"
    );
}

// ==========================================================
// UPDATE
// ==========================================================

void updateTFT(
    const SensorData &sensor,
    const WaterQuality &water,
    const DecisionState &decision
)
{
    if (!tftReady)
    {
        return;
    }

    handleTFTTouch();

    if (tftSleeping)
    {
        return;
    }

    const unsigned long now =
        millis();

    // Dashboard has a gentle animation.
    if (
        currentPage ==
        TFT_PAGE_DASHBOARD
    )
    {
        if (
            now - lastDashboardFrameMs >=
            DASHBOARD_FRAME_INTERVAL_MS
        )
        {
            lastDashboardFrameMs = now;

            drawDashboard(
                sensor,
                water
            );
        }

        return;
    }

    // Other pages are redrawn less frequently.
    if (
        now - lastStaticPageMs <
        STATIC_PAGE_INTERVAL_MS
    )
    {
        return;
    }

    lastStaticPageMs = now;

    switch (currentPage)
    {
        case TFT_PAGE_DIAGNOSTIC:
            drawDiagnostic(
                sensor,
                water,
                decision
            );
            break;

        case TFT_PAGE_WIFI:
            drawWiFiPage();
            break;

        case TFT_PAGE_TREND_GRAPH:
            drawTrendGraph();
            break;

        case TFT_PAGE_SECRET_TREND:
            drawSecretTrendPage();
            break;

        case TFT_PAGE_ADAPTIVE_BASELINE:
            drawAdaptiveBaselinePage();
            break;

        case TFT_PAGE_DASHBOARD:
        default:
            drawDashboard(
                sensor,
                water
            );
            break;
    }
}

// ==========================================================
// CLEAR
// ==========================================================

void clearTFT()
{
    if (tftReady)
    {
        tft.fillScreen(
            TFT_BLACK
        );
    }
}