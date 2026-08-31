#include "sensors.h"

#include <math.h>
#include <string.h>
#include <Wire.h>
#include <Adafruit_ADS1X15.h>
#include <OneWire.h>
#include <DallasTemperature.h>

#include "config.h"

// ==========================================================
// HARDWARE OBJECTS
// ==========================================================

static Adafruit_ADS1115 ads;
static OneWire oneWire(ONE_WIRE_BUS);
static DallasTemperature ds18b20(&oneWire);

// ==========================================================
// STATE
// ==========================================================

static SensorData sensor;

static bool sensorSystemReady = false;
static bool ds18b20ConversionPending = false;

static unsigned long lastSensorUpdateMs = 0;
static unsigned long ds18b20ConversionStartMs = 0;

// ==========================================================
// HELPERS
// ==========================================================

static float clampFloat(
    float value,
    float minimum,
    float maximum
)
{
    if (value < minimum)
    {
        return minimum;
    }

    if (value > maximum)
    {
        return maximum;
    }

    return value;
}

static bool isFiniteFloat(float value)
{
    return !isnan(value) && !isinf(value);
}

// ==========================================================
// MEDIAN FILTER
// ==========================================================
//
// The array is populated completely before sorting.
// There are no zero-filled startup values.
//

static int16_t readADSRawMedian(
    uint8_t channel,
    uint8_t sampleCount
)
{
    if (sampleCount == 0)
    {
        sampleCount = 1;
    }

    if (sampleCount > ADC_SAMPLE_COUNT)
    {
        sampleCount = ADC_SAMPLE_COUNT;
    }

    int16_t samples[ADC_SAMPLE_COUNT];

    for (uint8_t i = 0; i < sampleCount; i++)
    {
        samples[i] =
            ads.readADC_SingleEnded(channel);

#if ADC_SAMPLE_DELAY_MS > 0
        delay(ADC_SAMPLE_DELAY_MS);
#endif
    }

    for (uint8_t i = 0; i < sampleCount; i++)
    {
        for (uint8_t j = i + 1; j < sampleCount; j++)
        {
            if (samples[j] < samples[i])
            {
                const int16_t temporary = samples[i];

                samples[i] = samples[j];
                samples[j] = temporary;
            }
        }
    }

    if ((sampleCount % 2U) == 1U)
    {
        return samples[sampleCount / 2U];
    }

    const uint8_t upperIndex = sampleCount / 2U;
    const uint8_t lowerIndex = upperIndex - 1U;

    return (int16_t)(
        (
            (int32_t)samples[lowerIndex] +
            (int32_t)samples[upperIndex]
        ) / 2
    );
}

static float rawToVoltage(int16_t raw)
{
    if (raw < 0)
    {
        raw = 0;
    }

    return clampFloat(
        raw * ADS1115_LSB_VOLTS,
        0.0f,
        4.096f
    );
}

// ==========================================================
// TDS
// ==========================================================
//
// DFRobot-compatible calculation:
//
// 1. Read voltage.
// 2. Apply temperature compensation.
// 3. Calculate EC using the DFRobot polynomial.
// 4. Apply calibrated K-value.
// 5. Convert EC to TDS using 0.5.
//

static float calculateTDS(
    float voltage,
    float temperatureC,
    float &temperatureCoefficient,
    float &rawEC
)
{
    // Original calibrated formula.
    temperatureCoefficient =
        1.0f +
        0.02f *
        (temperatureC - 25.0f);

    if (
        !isFiniteFloat(temperatureCoefficient) ||
        temperatureCoefficient <= 0.0f
    )
    {
        temperatureCoefficient = 1.0f;
    }

    const float compensatedVoltage =
        voltage /
        temperatureCoefficient;

    // Original DFRobot-style EC polynomial.
    rawEC =
        133.42f *
        compensatedVoltage *
        compensatedVoltage *
        compensatedVoltage
        -
        255.86f *
        compensatedVoltage *
        compensatedVoltage
        +
        857.39f *
        compensatedVoltage;

    if (
        !isFiniteFloat(rawEC) ||
        rawEC < 0.0f
    )
    {
        rawEC = 0.0f;
    }

    // K-value is the calibrated probe factor.
    // TDS conversion factor remains 0.5.
    const float calibratedEC =
        rawEC * TDS_K_VALUE;

    const float finalTDS =
        calibratedEC * 0.5f;

    if (!isFiniteFloat(finalTDS))
    {
        return 0.0f;
    }

    return clampFloat(
        finalTDS,
        0.0f,
        TDS_MAX_PPM
    );
}
// ==========================================================
// pH
// ==========================================================

static float calculatePH(float voltage)
{
    const float ph =
        PH_CAL_SLOPE *
        voltage +
        PH_CAL_OFFSET;

    return clampFloat(
        ph,
        PH_MIN_VALUE,
        PH_MAX_VALUE
    );
}

// ==========================================================
// TURBIDITY
// ==========================================================

static float calculateNTU(float voltage)
{
    const float sensorVoltage =
        voltage *
        NTU_ADC_TO_SENSOR_SCALE +
        NTU_VOLTAGE_OFFSET;

    if (sensorVoltage >= 4.2f)
    {
        return 0.0f;
    }

    if (sensorVoltage < 2.5f)
    {
        return NTU_MAX_VALUE;
    }

    float ntu =
        -1120.4f *
        sensorVoltage *
        sensorVoltage
        +
        5742.3f *
        sensorVoltage
        -
        4352.9f;

    if (!isFiniteFloat(ntu) || ntu < 0.0f)
    {
        ntu = 0.0f;
    }

    return clampFloat(
        ntu,
        0.0f,
        NTU_MAX_VALUE
    );
}

// ==========================================================
// DS18B20
// ==========================================================

static void beginTemperatureConversion()
{
    if (!sensor.ds18b20OK)
    {
        sensor.temperatureValid = false;
        return;
    }

    ds18b20.requestTemperatures();

    ds18b20ConversionPending = true;
    ds18b20ConversionStartMs = millis();
}

static void finishTemperatureConversionIfReady()
{
    if (!ds18b20ConversionPending)
    {
        return;
    }

    if (
        !ds18b20.isConversionComplete() &&
        millis() - ds18b20ConversionStartMs <
        DS18B20_CONVERSION_TIME_MS
    )
    {
        return;
    }

    const float value =
        ds18b20.getTempCByIndex(0);

    ds18b20ConversionPending = false;

    if (
        value == DEVICE_DISCONNECTED_C ||
        !isFiniteFloat(value) ||
        value < -50.0f ||
        value > 125.0f
    )
    {
        sensor.ds18b20OK = false;
        sensor.temperatureValid = false;

        Serial.println(
            "[SENSOR] DS18B20 reading invalid"
        );

        return;
    }

    sensor.temperature = value;
    sensor.temperatureValid = true;
}

// ==========================================================
// INITIALIZATION
// ==========================================================

void initSensors()
{
    memset(&sensor, 0, sizeof(sensor));

    sensor.temperature = 25.0f;
    sensor.ph = 0.0f;
    sensor.tds = 0.0f;
    sensor.ntu = 0.0f;

    sensor.tdsVoltage = 0.0f;
    sensor.phVoltage = 0.0f;
    sensor.ntuVoltage = 0.0f;

    sensor.tdsTemperatureCoefficient = 1.0f;
    sensor.tdsRawEC = 0.0f;

    sensor.temperatureValid = false;
    sensor.phValid = false;
    sensor.tdsValid = false;
    sensor.ntuValid = false;

    sensor.ds18b20OK = false;
    sensor.ads1115OK = false;

    sensor.warmupComplete = false;
    sensor.allRequiredSensorsValid = false;

    sensor.validSampleCount = 0;
    sensor.lastUpdate = millis();

    sensorSystemReady = false;
    ds18b20ConversionPending = false;

    Serial.println();
    Serial.println("========================================");
    Serial.println(" Initializing Sensors");
    Serial.println("========================================");

    // ------------------------------------------------------
    // ADS1115
    // ------------------------------------------------------

    if (
        ads.begin(
            ADS1115_ADDRESS,
            &Wire
        )
    )
    {
        ads.setGain(ADS_GAIN);
        ads.setDataRate(RATE_ADS1115_64SPS);

        sensor.ads1115OK = true;

        Serial.println("[ OK ] ADS1115 detected");
        Serial.print("[ OK ] ADS1115 address: 0x");
        Serial.println(ADS1115_ADDRESS, HEX);

        Serial.print("[ OK ] TDS channel: A");
        Serial.println(TDS_CHANNEL);

        Serial.print("[ OK ] pH channel: A");
        Serial.println(PH_CHANNEL);

        Serial.print("[ OK ] Turbidity channel: A");
        Serial.println(TURBIDITY_CHANNEL);

        Serial.print("[ OK ] ADC median samples: ");
        Serial.println(ADC_SAMPLE_COUNT);
    }
    else
    {
        sensor.ads1115OK = false;

        Serial.print("[FAIL] ADS1115 not found at 0x");
        Serial.println(ADS1115_ADDRESS, HEX);
    }

    // ------------------------------------------------------
    // DS18B20
    // ------------------------------------------------------

    ds18b20.begin();

    const uint8_t deviceCount =
        ds18b20.getDeviceCount();

    if (deviceCount > 0)
    {
        sensor.ds18b20OK = true;

        ds18b20.setResolution(
            DS18B20_RESOLUTION_BITS
        );

        ds18b20.setWaitForConversion(false);

        Serial.print("[ OK ] DS18B20 devices: ");
        Serial.println(deviceCount);

        Serial.print("[ OK ] DS18B20 resolution: ");
        Serial.print(DS18B20_RESOLUTION_BITS);
        Serial.println(" bit");
    }
    else
    {
        sensor.ds18b20OK = false;

        Serial.println("[WARN] DS18B20 not found");
        Serial.println("[INFO] TDS will use 25 C fallback");
    }

    sensorSystemReady = true;
    lastSensorUpdateMs = millis();

    Serial.println("[ OK ] Sensor manager ready");
    Serial.println("========================================");
}

// ==========================================================
// UPDATE
// ==========================================================

void updateSensors()
{
    if (!sensorSystemReady)
    {
        return;
    }

    finishTemperatureConversionIfReady();

    const unsigned long now = millis();

    if (
        now - lastSensorUpdateMs <
        SENSOR_UPDATE_INTERVAL_MS
    )
    {
        return;
    }

    lastSensorUpdateMs = now;

    // Start the next temperature conversion.
    beginTemperatureConversion();

    if (!sensor.ads1115OK)
    {
        sensor.phValid = false;
        sensor.tdsValid = false;
        sensor.ntuValid = false;
        sensor.allRequiredSensorsValid = false;

        return;
    }

    // ------------------------------------------------------
    // Read ADS1115 channels
    // ------------------------------------------------------

    sensor.rawTDS =
        readADSRawMedian(
            TDS_CHANNEL,
            ADC_SAMPLE_COUNT
        );

    sensor.rawPH =
        readADSRawMedian(
            PH_CHANNEL,
            ADC_SAMPLE_COUNT
        );

    sensor.rawNTU =
        readADSRawMedian(
            TURBIDITY_CHANNEL,
            ADC_SAMPLE_COUNT
        );

    sensor.tdsVoltage =
        rawToVoltage(sensor.rawTDS);

    sensor.phVoltage =
        rawToVoltage(sensor.rawPH);

    sensor.ntuVoltage =
        rawToVoltage(sensor.rawNTU);

    // ------------------------------------------------------
    // TDS
    // ------------------------------------------------------

    float temperatureForTDS =
        sensor.temperatureValid
            ? sensor.temperature
            : TDS_REFERENCE_TEMPERATURE_C;

    sensor.tds =
        calculateTDS(
            sensor.tdsVoltage,
            temperatureForTDS,
            sensor.tdsTemperatureCoefficient,
            sensor.tdsRawEC
        );

    sensor.tdsValid =
        sensor.tdsVoltage >= TDS_MIN_VALID_VOLTAGE &&
        sensor.tdsVoltage <= TDS_MAX_VALID_VOLTAGE &&
        isFiniteFloat(sensor.tds);

    // ------------------------------------------------------
    // pH
    // ------------------------------------------------------

    sensor.ph =
        calculatePH(sensor.phVoltage);

    sensor.phValid =
        sensor.phVoltage >= 0.0f &&
        sensor.phVoltage <= 4.096f &&
        isFiniteFloat(sensor.ph);

    // ------------------------------------------------------
    // Turbidity
    // ------------------------------------------------------

    sensor.ntu =
        calculateNTU(sensor.ntuVoltage);

    sensor.ntuValid =
        sensor.ntuVoltage >= 0.0f &&
        sensor.ntuVoltage <= 4.096f &&
        isFiniteFloat(sensor.ntu);

    // ------------------------------------------------------
    // Warm-up state
    // ------------------------------------------------------

    if (
        sensor.ads1115OK &&
        sensor.tdsValid &&
        sensor.phValid &&
        sensor.ntuValid
    )
    {
        if (
            sensor.validSampleCount <
            SENSOR_WARMUP_VALID_SAMPLES
        )
        {
            sensor.validSampleCount++;
        }
    }
    else
    {
        sensor.validSampleCount = 0;
    }

    sensor.warmupComplete =
        sensor.validSampleCount >=
        SENSOR_WARMUP_VALID_SAMPLES;

    // Temperature is not required for validity because
    // the TDS algorithm safely falls back to 25 C.
    sensor.allRequiredSensorsValid =
        sensor.ads1115OK &&
        sensor.tdsValid &&
        sensor.phValid &&
        sensor.ntuValid &&
        sensor.warmupComplete;

    sensor.lastUpdate = now;
}

// ==========================================================
// STATUS
// ==========================================================

bool sensorsReady()
{
    return sensorSystemReady;
}

bool sensorsWarmupComplete()
{
    return sensor.warmupComplete;
}

bool ads1115OK()
{
    return sensor.ads1115OK;
}

bool ds18b20OK()
{
    return sensor.ds18b20OK;
}

bool temperatureValid()
{
    return sensor.temperatureValid;
}

bool phValid()
{
    return sensor.phValid;
}

bool tdsValid()
{
    return sensor.tdsValid;
}

bool ntuValid()
{
    return sensor.ntuValid;
}

bool allRequiredSensorsValid()
{
    return sensor.allRequiredSensorsValid;
}

// ==========================================================
// GETTERS
// ==========================================================

SensorData getSensorData()
{
    return sensor;
}

float getTemperature()
{
    return sensor.temperature;
}

float getPH()
{
    return sensor.ph;
}

float getTDS()
{
    return sensor.tds;
}

float getNTU()
{
    return sensor.ntu;
}

float getTDSVoltage()
{
    return sensor.tdsVoltage;
}

float getPHVoltage()
{
    return sensor.phVoltage;
}

float getNTUVoltage()
{
    return sensor.ntuVoltage;
}

float getTDSRawEC()
{
    return sensor.tdsRawEC;
}

float getTDSTemperatureCoefficient()
{
    return sensor.tdsTemperatureCoefficient;
}

int16_t getRawTDS()
{
    return sensor.rawTDS;
}

int16_t getRawPH()
{
    return sensor.rawPH;
}

int16_t getRawNTU()
{
    return sensor.rawNTU;
}

// ==========================================================
// DEBUG
// ==========================================================

void printSensorDebug()
{
    Serial.println();
    Serial.println("========== SENSOR DEBUG ==========");

    Serial.print("ADS1115: ");
    Serial.println(sensor.ads1115OK ? "OK" : "FAIL");

    Serial.print("DS18B20: ");
    Serial.println(sensor.ds18b20OK ? "OK" : "FAIL");

    Serial.print("Warmup: ");
    Serial.println(sensor.warmupComplete ? "DONE" : "ACTIVE");

    Serial.print("Valid samples: ");
    Serial.println(sensor.validSampleCount);

    Serial.println();

    Serial.print("Temperature: ");
    Serial.print(sensor.temperature, 2);
    Serial.print(" C  [");
    Serial.print(sensor.temperatureValid ? "VALID" : "FALLBACK/INVALID");
    Serial.println("]");

    Serial.print("pH: ");
    Serial.print(sensor.ph, 2);
    Serial.print("  voltage=");
    Serial.print(sensor.phVoltage, 5);
    Serial.print("  raw=");
    Serial.print(sensor.rawPH);
    Serial.print("  [");
    Serial.print(sensor.phValid ? "VALID" : "INVALID");
    Serial.println("]");

    Serial.print("TDS: ");
    Serial.print(sensor.tds, 2);
    Serial.print(" ppm  voltage=");
    Serial.print(sensor.tdsVoltage, 5);
    Serial.print("  raw=");
    Serial.print(sensor.rawTDS);
    Serial.print("  [");
    Serial.print(sensor.tdsValid ? "VALID" : "INVALID");
    Serial.println("]");

    Serial.print("TDS temperature: ");
    Serial.print(sensor.temperature, 2);
    Serial.println(" C");

    Serial.print("TDS compensation coefficient: ");
    Serial.println(
        sensor.tdsTemperatureCoefficient,
        6
    );

    Serial.print("TDS raw EC: ");
    Serial.println(sensor.tdsRawEC, 4);

    Serial.print("TDS K-value: ");
    Serial.println(TDS_K_VALUE, 6);

    Serial.print("TDS final: ");
    Serial.println(sensor.tds, 2);

    Serial.print("Turbidity: ");
    Serial.print(sensor.ntu, 2);
    Serial.print(" NTU  voltage=");
    Serial.print(sensor.ntuVoltage, 5);
    Serial.print("  raw=");
    Serial.print(sensor.rawNTU);
    Serial.print("  [");
    Serial.print(sensor.ntuValid ? "VALID" : "INVALID");
    Serial.println("]");

    Serial.println(
        "=================================="
    );
}