#ifndef DISPLAY_OLED_H
#define DISPLAY_OLED_H

#include <Arduino.h>
#include <Adafruit_SSD1306.h>

#include "sensors.h"
#include "water_quality.h"
#include "decision_engine.h"

class DisplayOLED
{
public:
    bool initialize();

    void displayStatus(
        const SensorData &sensor,
        const WaterQuality &water,
        const DecisionState &decision
    );

    void displayIndicators(
        const SensorData &sensor,
        const WaterQuality &water,
        const DecisionState &decision
    );

    void clear();

    const char *getStatusSymbol(
        WaterStatus status
    );
};

void initOLED();

void updateOLED(
    const SensorData &sensor,
    const WaterQuality &water,
    const DecisionState &decision
);

#endif