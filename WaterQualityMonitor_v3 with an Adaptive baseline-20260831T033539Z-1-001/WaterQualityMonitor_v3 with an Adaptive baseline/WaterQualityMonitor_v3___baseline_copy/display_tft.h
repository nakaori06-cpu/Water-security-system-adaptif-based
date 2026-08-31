#ifndef DISPLAY_TFT_H
#define DISPLAY_TFT_H

#include <Arduino.h>

#include "sensors.h"
#include "water_quality.h"
#include "decision_engine.h"

void initTFT();

void updateTFT(
    const SensorData &sensor,
    const WaterQuality &water,
    const DecisionState &decision
);

void clearTFT();

#endif