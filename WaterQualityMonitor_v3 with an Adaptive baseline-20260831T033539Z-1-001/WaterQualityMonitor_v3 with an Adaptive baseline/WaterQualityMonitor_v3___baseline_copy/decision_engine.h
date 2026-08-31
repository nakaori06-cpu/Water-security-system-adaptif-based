#ifndef DECISION_ENGINE_H
#define DECISION_ENGINE_H

#include <Arduino.h>

// ==========================================================
// OPERATING MODE
// ==========================================================

enum SystemMode
{
    MODE_AUTO,
    MODE_MANUAL_ON,
    MODE_MANUAL_OFF
};

// ==========================================================
// OUTPUT STATES
// ==========================================================

enum RelayState
{
    RELAY_OFF,
    RELAY_ON
};

enum BuzzerState
{
    BUZZER_OFF,
    BUZZER_ON
};

// ==========================================================
// DECISION RESULT
// ==========================================================

struct DecisionState
{
    SystemMode mode;

    RelayState relay;
    BuzzerState buzzer;

    bool alarm;
    bool filterMaintenance;

    bool relayLocked;
    bool manualOverride;

    bool absoluteSafetyLock;
    bool externalOverride;
    bool waterQualityLock;
};

// ==========================================================
// PUBLIC API
// ==========================================================

void initDecisionEngine();
void updateDecisionEngine();

DecisionState getDecisionState();

// ==========================================================
// MANUAL CONTROL
// ==========================================================

void setAutoMode();
void setManualRelayON();
void setManualRelayOFF();

// ==========================================================
// DIAGNOSTICS
// ==========================================================

void printDecisionDebug();

#endif