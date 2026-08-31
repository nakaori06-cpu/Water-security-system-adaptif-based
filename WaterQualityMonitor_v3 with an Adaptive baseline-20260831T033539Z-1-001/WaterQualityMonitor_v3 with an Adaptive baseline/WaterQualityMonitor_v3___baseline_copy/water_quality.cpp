#include "water_quality.h"

#include <math.h>

#include "config.h"
#include "sensors.h"

// ==========================================================
// INTERNAL STATE
// ==========================================================

static WaterQuality water;

// ==========================================================
// SCORE WEIGHTS
// ==========================================================

static const float PH_WEIGHT =
    WATER_SCORE_PH_WEIGHT;

static const float TDS_WEIGHT =
    WATER_SCORE_TDS_WEIGHT;

static const float NTU_WEIGHT =
    WATER_SCORE_NTU_WEIGHT;

static const float TEMP_WEIGHT =
    WATER_SCORE_TEMP_WEIGHT;

// ==========================================================
// HELPERS
// ==========================================================

static float clampScore(float value)
{
    if (value < 0.0f)
    {
        return 0.0f;
    }

    if (value > 100.0f)
    {
        return 100.0f;
    }

    return value;
}

static bool isFiniteValue(float value)
{
    return !isnan(value) && !isinf(value);
}

// ==========================================================
// INDIVIDUAL SENSOR SCORES
// ==========================================================

static float calculatePHScore(float ph)
{
    if (!isFiniteValue(ph))
    {
        return 0.0f;
    }

    if (ph >= 6.8f && ph <= 7.4f)
    {
        return 100.0f;
    }

    if (ph >= 6.5f && ph < 6.8f)
    {
        return 90.0f;
    }

    if (ph > 7.4f && ph <= 8.0f)
    {
        return 90.0f;
    }

    if (ph >= 6.2f && ph < 6.5f)
    {
        return 70.0f;
    }

    if (ph > 8.0f && ph <= 8.5f)
    {
        return 70.0f;
    }

    if (ph >= 5.8f && ph < 6.2f)
    {
        return 40.0f;
    }

    if (ph > 8.5f && ph <= 9.0f)
    {
        return 40.0f;
    }

    return 0.0f;
}

static float calculateTDSScore(float tds)
{
    if (!isFiniteValue(tds) || tds < 0.0f)
    {
        return 0.0f;
    }

    if (tds <= 150.0f)
    {
        return 100.0f;
    }

    if (tds <= 300.0f)
    {
        return 90.0f;
    }

    if (tds <= 500.0f)
    {
        return 75.0f;
    }

    if (tds <= 700.0f)
    {
        return 50.0f;
    }

    return 0.0f;
}

static float calculateNTUScore(float ntu)
{
    if (!isFiniteValue(ntu) || ntu < 0.0f)
    {
        return 0.0f;
    }

    if (ntu <= 1.0f)
    {
        return 100.0f;
    }

    if (ntu <= 3.0f)
    {
        return 90.0f;
    }

    if (ntu <= 5.0f)
    {
        return 75.0f;
    }

    if (ntu <= 10.0f)
    {
        return 40.0f;
    }

    return 0.0f;
}

static float calculateTemperatureScore(float temperature)
{
    if (!isFiniteValue(temperature))
    {
        return 0.0f;
    }

    if (temperature >= 24.0f && temperature <= 30.0f)
    {
        return 100.0f;
    }

    if (temperature >= 22.0f && temperature < 24.0f)
    {
        return 90.0f;
    }

    if (temperature > 30.0f && temperature <= 32.0f)
    {
        return 90.0f;
    }

    if (temperature >= 20.0f && temperature < 22.0f)
    {
        return 70.0f;
    }

    if (temperature > 32.0f && temperature <= 35.0f)
    {
        return 70.0f;
    }

    return 40.0f;
}

// ==========================================================
// INITIALIZATION
// ==========================================================

void initWaterQuality()
{
    water.score = 0.0f;
    water.confidence = 0.0f;

    water.status = WATER_WARMUP;
    water.risk = RISK_UNKNOWN;

    water.valid = false;

    // Fail-safe during startup.
    water.relayEnable = false;
    water.buzzerEnable = false;
    water.alarm = false;
    water.filterMaintenance = false;

    water.safeForBathing = false;
    water.safeForLaundry = false;
    water.safeForCleaning = false;
    water.safeForDrinking = false;

    water.phValid = false;
    water.tdsValid = false;
    water.ntuValid = false;
    water.temperatureValid = false;

    water.statusText = "WARMUP";
    water.riskText = "UNKNOWN";
}

// ==========================================================
// UPDATE
// ==========================================================

void updateWaterQuality()
{
    const SensorData sensor =
        getSensorData();

    water.phValid =
        sensor.phValid;

    water.tdsValid =
        sensor.tdsValid;

    water.ntuValid =
        sensor.ntuValid;

    water.temperatureValid =
        sensor.temperatureValid;

    // ------------------------------------------------------
    // Sensor warm-up / invalid state
    // ------------------------------------------------------

    if (!sensor.allRequiredSensorsValid)
    {
        water.score = 0.0f;
        water.confidence = 0.0f;

        water.status = WATER_WARMUP;
        water.risk = RISK_UNKNOWN;
        water.valid = false;

        water.statusText = "WARMUP";
        water.riskText = "UNKNOWN";

        // Fail-safe relay while sensor data is unavailable.
        water.relayEnable = false;
        water.buzzerEnable = false;
        water.alarm = false;
        water.filterMaintenance = false;

        water.safeForBathing = false;
        water.safeForLaundry = false;
        water.safeForCleaning = false;
        water.safeForDrinking = false;

        return;
    }

    // ------------------------------------------------------
    // Calculate individual scores
    // ------------------------------------------------------

    const float phScore =
        calculatePHScore(sensor.ph);

    const float tdsScore =
        calculateTDSScore(sensor.tds);

    const float ntuScore =
        calculateNTUScore(sensor.ntu);

    const float temperatureScore =
        calculateTemperatureScore(
            sensor.temperature
        );

    // ------------------------------------------------------
    // Weighted score
    // ------------------------------------------------------

    water.score =
        (
            phScore * PH_WEIGHT +
            tdsScore * TDS_WEIGHT +
            ntuScore * NTU_WEIGHT +
            temperatureScore * TEMP_WEIGHT
        ) / 100.0f;

    water.score =
        clampScore(water.score);

    water.valid = true;

    // ------------------------------------------------------
    // Status classification
    // ------------------------------------------------------

    if (water.score >= 95.0f)
    {
        water.status = EXCELLENT;
        water.statusText = "EXCELLENT";
    }
    else if (water.score >= 85.0f)
    {
        water.status = GOOD;
        water.statusText = "GOOD";
    }
    else if (water.score >= 70.0f)
    {
        water.status = FAIR;
        water.statusText = "FAIR";
    }
    else if (water.score >= 50.0f)
    {
        water.status = POOR;
        water.statusText = "POOR";
    }
    else
    {
        water.status = UNSAFE;
        water.statusText = "UNSAFE";
    }

    // ------------------------------------------------------
    // Risk classification
    // ------------------------------------------------------

    switch (water.status)
    {
        case EXCELLENT:
        case GOOD:
            water.risk = SAFE;
            water.riskText = "SAFE";
            break;

        case FAIR:
            water.risk = WARNING;
            water.riskText = "WARNING";
            break;

        case POOR:
        case UNSAFE:
            water.risk = DANGER;
            water.riskText = "DANGER";
            break;

        case WATER_WARMUP:
        default:
            water.risk = RISK_UNKNOWN;
            water.riskText = "UNKNOWN";
            break;
    }

    // ------------------------------------------------------
    // Default automatic outputs
    // ------------------------------------------------------

    water.relayEnable = true;
    water.buzzerEnable = false;
    water.alarm = false;
    water.filterMaintenance = false;

    // ------------------------------------------------------
    // Water-quality recommendations
    // ------------------------------------------------------

    if (water.status == FAIR)
    {
        water.filterMaintenance = true;
    }
    else if (water.status == POOR)
    {
        water.buzzerEnable = true;
        water.filterMaintenance = true;
    }
    else if (water.status == UNSAFE)
    {
        water.buzzerEnable = true;
        water.alarm = true;
        water.filterMaintenance = true;

#if UNSAFE_WATER_STOPS_RELAY
        water.relayEnable = false;
#endif
    }

    // ------------------------------------------------------
    // Usage recommendations
    // ------------------------------------------------------
    //
    // These are conservative recommendations only.
    // The sensor set cannot certify drinking-water safety.
    //

    water.safeForDrinking = false;
    water.safeForBathing = false;
    water.safeForLaundry = false;
    water.safeForCleaning = false;

#if DRINKING_WATER_RECOMMENDATION_ENABLED
    if (water.status == EXCELLENT)
    {
        water.safeForDrinking = true;
    }
#endif

    if (water.status == EXCELLENT)
    {
        water.safeForBathing = true;
        water.safeForLaundry = true;
        water.safeForCleaning = true;
    }
    else if (water.status == GOOD)
    {
        water.safeForBathing = true;
        water.safeForLaundry = true;
        water.safeForCleaning = true;
    }
    else if (water.status == FAIR)
    {
        water.safeForLaundry = true;
        water.safeForCleaning = true;
    }

    // ------------------------------------------------------
    // Confidence
    // ------------------------------------------------------
    //
    // Required sensors are valid at this point.
    // DS18B20 failure is allowed because TDS uses 25 C fallback,
    // but confidence is reduced.
    //

    water.confidence = 100.0f;

    if (!sensor.ds18b20OK)
    {
        water.confidence -= 20.0f;
    }

    if (!sensor.temperatureValid)
    {
        water.confidence -= 10.0f;
    }

    if (!sensor.phValid)
    {
        water.confidence -= 30.0f;
    }

    if (!sensor.tdsValid)
    {
        water.confidence -= 30.0f;
    }

    if (!sensor.ntuValid)
    {
        water.confidence -= 20.0f;
    }

    water.confidence =
        clampScore(water.confidence);
}

// ==========================================================
// GETTERS
// ==========================================================

WaterQuality getWaterQuality()
{
    return water;
}

const char *getWaterStatusText()
{
    return water.statusText.c_str();
}

const char *getWaterRiskText()
{
    return water.riskText.c_str();
}