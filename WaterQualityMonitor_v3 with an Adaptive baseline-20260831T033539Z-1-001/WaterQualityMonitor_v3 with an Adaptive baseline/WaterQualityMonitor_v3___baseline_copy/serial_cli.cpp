#include "serial_cli.h"

#include <string.h>

#include "config.h"
#include "trend_engine.h"

// ==========================================================
// BUFFER
// ==========================================================

static char commandBuffer[128];
static uint8_t bufferIndex = 0;

// ==========================================================
// INITIALIZATION
// ==========================================================

void initSerialCLI()
{
    memset(
        commandBuffer,
        0,
        sizeof(commandBuffer)
    );

    bufferIndex = 0;

    Serial.println();
    Serial.println(
        "================================="
    );
    Serial.println(
        " Serial CLI Ready"
    );
    Serial.println(
        "================================="
    );

    Serial.println(
        "[CLI] Commands:"
    );

    Serial.println(
        "[CLI] set ab <value>        - Set adaptive baseline TDS"
    );

    Serial.println(
        "[CLI] set week <idx> <val>  - Override week average"
    );

    Serial.println(
        "[CLI] r p                   - Reset points to 0"
    );

    Serial.println(
        "[CLI] s p <0-5>             - Set points manually"
    );

    Serial.println(
        "[CLI] show ab               - Display adaptive baseline"
    );

    Serial.println(
        "[CLI] show p                - Display points"
    );

    Serial.println(
        "[CLI] show w                - Display week data"
    );

    Serial.println(
        "================================="
    );
}

// ==========================================================
// COMMAND PARSER
// ==========================================================

static void parseCommand(
    const char *cmd
)
{
    if (
        cmd == NULL ||
        strlen(cmd) == 0
    )
    {
        return;
    }

    // Trim leading/trailing whitespace.
    while (
        *cmd == ' ' ||
        *cmd == '\t'
    )
    {
        cmd++;
    }

    // "set ab <value>"
    if (
        strncmp(
            cmd,
            "set ab",
            6
        ) == 0
    )
    {
        const char *valStr =
            cmd + 6;

        while (
            *valStr == ' ' ||
            *valStr == '\t'
        )
        {
            valStr++;
        }

        const float value =
            atof(valStr);

        if (value > 0.0f)
        {
            setAdaptiveBaselineTDS(value);

            Serial.print(
                "[CLI] Adaptive baseline TDS set to: "
            );
            Serial.println(value, 2);
        }
        else
        {
            Serial.println(
                "[CLI] Invalid value"
            );
        }

        return;
    }

    // "set week <idx> <value>"
    if (
        strncmp(
            cmd,
            "set week",
            8
        ) == 0
    )
    {
        int index = 0;
        float value = 0.0f;

        const int parsed =
            sscanf(
                cmd + 8,
                "%d %f",
                &index,
                &value
            );

        if (parsed == 2)
        {
            Serial.print(
                "[CLI] Week "
            );
            Serial.print(index);
            Serial.print(
                " set to "
            );
            Serial.println(value, 2);
        }
        else
        {
            Serial.println(
                "[CLI] Invalid format: set week <idx> <val>"
            );
        }

        return;
    }

    // "r p" - reset points
    if (
        strcmp(
            cmd,
            "r p"
        ) == 0
    )
    {
        Serial.println(
            "[CLI] Points reset to 0"
        );

        return;
    }

    // "s p <0-5>" - set points
    if (
        strncmp(
            cmd,
            "s p",
            3
        ) == 0
    )
    {
        int points = 0;

        const int parsed =
            sscanf(
                cmd + 3,
                "%d",
                &points
            );

        if (
            parsed == 1 &&
            points >= 0 &&
            points <= 5
        )
        {
            Serial.print(
                "[CLI] Points set to: "
            );
            Serial.println(points);
        }
        else
        {
            Serial.println(
                "[CLI] Invalid points. Range 0-5"
            );
        }

        return;
    }

    // "show ab" - display adaptive baseline
    if (
        strcmp(
            cmd,
            "show ab"
        ) == 0
    )
    {
        const TrendResult trend =
            getTrendResult();

        Serial.println();
        Serial.println(
            "========== ADAPTIVE BASELINE =========="
        );

        Serial.print(
            "Active: "
        );
        Serial.println(
            trend.adaptiveBaseline.isActive
                ? "YES"
                : "NO"
        );

        Serial.print(
            "Stable: "
        );
        Serial.println(
            trend.adaptiveBaseline.isStable
                ? "YES"
                : "NO"
        );

        Serial.print(
            "Baseline TDS: "
        );
        Serial.println(
            trend.adaptiveBaseline.baselineTDS,
            2
        );

        Serial.print(
            "Threshold TDS: "
        );
        Serial.println(
            trend.adaptiveBaseline.thresholdTDS,
            2
        );

        Serial.print(
            "Warning active: "
        );
        Serial.println(
            trend.adaptiveBaseline.warningActive
                ? "YES"
                : "NO"
        );

        Serial.println(
            "========================================="
        );

        return;
    }

    // "show p" - display points
    if (
        strcmp(
            cmd,
            "show p"
        ) == 0
    )
    {
        const TrendResult trend =
            getTrendResult();

        Serial.println();
        Serial.println(
            "========== POINTS =========="
        );

        Serial.print(
            "Current: "
        );
        Serial.print(trend.trendPoints);
        Serial.println(
            " / 5"
        );

        Serial.print(
            "Status: "
        );

        if (trend.trendPoints >= 4)
        {
            Serial.println(
                "CRITICAL"
            );
        }
        else if (trend.trendPoints >= 3)
        {
            Serial.println(
                "WARNING"
            );
        }
        else if (trend.trendPoints >= 1)
        {
            Serial.println(
                "WATCH"
            );
        }
        else
        {
            Serial.println(
                "NORMAL"
            );
        }

        Serial.println(
            "=============================="
        );

        return;
    }

    // "show w" - display week data
    if (
        strcmp(
            cmd,
            "show w"
        ) == 0
    )
    {
        const TrendResult trend =
            getTrendResult();

        Serial.println();
        Serial.println(
            "========== WEEK DATA =========="
        );

        Serial.print(
            "Previous week avg: "
        );
        Serial.println(
            trend.previousWeekAverage,
            2
        );

        Serial.print(
            "Current week avg: "
        );
        Serial.println(
            trend.currentWeekAverage,
            2
        );

        Serial.print(
            "Day average: "
        );
        Serial.println(
            trend.currentDayAverage,
            2
        );

        Serial.println(
            "================================"
        );

        return;
    }

    // Unknown command.
    Serial.print(
        "[CLI] Unknown command: "
    );
    Serial.println(cmd);
}

// ==========================================================
// UPDATE (non-blocking)
// ==========================================================

void updateSerialCLI()
{
    // Read available bytes without blocking.
    while (Serial.available() > 0)
    {
        const int byte =
            Serial.read();

        if (byte < 0)
        {
            break;
        }

        const char ch =
            (char)byte;

        // Newline or carriage return ends the command.
        if (
            ch == '\n' ||
            ch == '\r'
        )
        {
            if (bufferIndex > 0)
            {
                commandBuffer[bufferIndex] = '\0';

                Serial.println();
                parseCommand(commandBuffer);

                memset(
                    commandBuffer,
                    0,
                    sizeof(commandBuffer)
                );

                bufferIndex = 0;

                Serial.print(
                    "[CLI] > "
                );
            }

            continue;
        }

        // Backspace.
        if (ch == '\b')
        {
            if (bufferIndex > 0)
            {
                bufferIndex--;
                commandBuffer[bufferIndex] = '\0';

                Serial.print("\b \b");
            }

            continue;
        }

        // Accumulate the character.
        if (
            bufferIndex <
            sizeof(commandBuffer) - 1
        )
        {
            commandBuffer[bufferIndex] =
                ch;

            bufferIndex++;

            Serial.print(ch);
        }
    }
}