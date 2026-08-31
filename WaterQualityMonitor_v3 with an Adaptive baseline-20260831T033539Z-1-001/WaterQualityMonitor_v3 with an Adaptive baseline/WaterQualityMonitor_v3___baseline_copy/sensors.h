#ifndef SENSORS_H
#define SENSORS_H

#include <Arduino.h>

// ==========================================================
// SENSOR DATA
// ==========================================================

struct SensorData
{
    float temperature;
    float ph;
    float tds;
    float ntu;

    float tdsVoltage;
    float phVoltage;
    float ntuVoltage;

    float tdsTemperatureCoefficient;
    float tdsRawEC;

    int16_t rawTDS;
    int16_t rawPH;
    int16_t rawNTU;

    bool temperatureValid;
    bool phValid;
    bool tdsValid;
    bool ntuValid;

    bool ds18b20OK;
    bool ads1115OK;

    bool warmupComplete;
    bool allRequiredSensorsValid;

    uint8_t validSampleCount;

    unsigned long lastUpdate;
};

// ==========================================================
// LIFECYCLE
// ==========================================================

void initSensors();
void updateSensors();

// ==========================================================
// STATUS
// ==========================================================

bool sensorsReady();
bool sensorsWarmupComplete();

bool ads1115OK();
bool ds18b20OK();

bool temperatureValid();
bool phValid();
bool tdsValid();
bool ntuValid();

bool allRequiredSensorsValid();

// ==========================================================
// DATA
// ==========================================================

SensorData getSensorData();

float getTemperature();
float getPH();
float getTDS();
float getNTU();

float getTDSVoltage();
float getPHVoltage();
float getNTUVoltage();

float getTDSRawEC();
float getTDSTemperatureCoefficient();

int16_t getRawTDS();
int16_t getRawPH();
int16_t getRawNTU();

// ==========================================================
// DIAGNOSTICS
// ==========================================================

void printSensorDebug();

#endif