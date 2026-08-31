#ifndef TOUCH_MANAGER_H
#define TOUCH_MANAGER_H

#include <Arduino.h>

// ==========================================================
// TOUCH INPUTS
// ==========================================================
//
// GPIO34:
//     Relay automatic-control override.
//
// GPIO35:
//     Reserved for TFT page navigation and the
//     10-second Trend Details hold.
//
// Both pins are input-only ESP32 pins and require
// external electrical biasing.
//

void initTouchManager();
void updateTouchManager();

bool isManualOverrideActive();
bool isRelayAutoModeDisabled();

void setManualOverride(bool state);
void setRelayAutoModeDisabled(bool state);

#endif