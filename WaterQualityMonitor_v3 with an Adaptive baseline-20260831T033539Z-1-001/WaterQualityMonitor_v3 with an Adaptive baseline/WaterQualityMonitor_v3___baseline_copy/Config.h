#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// ==========================================================
// FIRMWARE
// ==========================================================

#define FW_VERSION "4.0.0"

// ==========================================================
// SERIAL
// ==========================================================

#define SERIAL_BAUD_RATE 115200UL
#define ENABLE_SERIAL_DEBUG 1

// ==========================================================
// I2C
// ==========================================================

#define I2C_SDA 21
#define I2C_SCL 22

#define ADS1115_ADDRESS 0x48
#define RTC_I2C_ADDRESS 0x68
#define OLED_I2C_ADDRESS 0x3C

// ==========================================================
// ADS1115
// ==========================================================
//
// Confirmed wiring:
//
// TDS       -> A1
// Turbidity -> A0
// pH        -> A2
//
// ADS1115 GAIN_ONE range is ±4.096 V.
// The ADS1115 LSB at GAIN_ONE is 0.125 mV.
//

// ADS1115 channel mapping matching the calibrated sensor sketch
#define TDS_CHANNEL       0   // A0
#define PH_CHANNEL        2   // A2
#define TURBIDITY_CHANNEL 3   // A3, if turbidity is connected there

#define ADS_GAIN GAIN_ONE
#define ADS1115_LSB_VOLTS 0.000125f

#define ADC_SAMPLE_COUNT 20
#define ADC_SAMPLE_DELAY_MS 10UL

// Keep the calibrated sensor formula.
#define TDS_K_VALUE 1.000000f
#define TDS_MAX_PPM 1000.0f

#define TDS_REFERENCE_TEMPERATURE_C 25.0f
#define TDS_TEMPERATURE_COEFFICIENT 0.02f
#define TDS_TO_EC_FACTOR 0.5f


float PH_SLOPE  = -6.463;   // Changed from -5.70
float PH_OFFSET = 23.07;     // Changed from 22.84

// ==========================================================
// SENSOR UPDATE TIMING
// ==========================================================

#define SENSOR_UPDATE_INTERVAL_MS 1000UL

// DS18B20 asynchronous conversion.
// 10-bit resolution requires approximately 188 ms.
#define DS18B20_RESOLUTION_BITS 10
#define DS18B20_CONVERSION_TIME_MS 200UL

#define SENSOR_WARMUP_VALID_SAMPLES 3

// ==========================================================
// TDS CALCULATION
// ==========================================================
//
// DFRobot-style calculation:
//
// compensationCoefficient =
//     1.0 + 0.02 * (temperature - 25.0)
//
// compensatedVoltage = voltage / compensationCoefficient
//
// EC =
//     133.42 * V^3
//   - 255.86 * V^2
//   + 857.39 * V
//
// EC is multiplied by the probe K-value.
// TDS = EC * 0.5
//
// TDS_K_VALUE must come from calibration.
// It is not a generic display multiplier.
//

#define TDS_MIN_VALID_VOLTAGE 0.0f
#define TDS_MAX_VALID_VOLTAGE 4.096f

// ==========================================================
// pH CALIBRATION
// ==========================================================

#define PH_MIN_VALUE 0.0f
#define PH_MAX_VALUE 14.0f

// ==========================================================
// TURBIDITY
// ==========================================================

#define NTU_ADC_TO_SENSOR_SCALE 1.0f
#define NTU_MAX_VALUE 3000.0f

// Per-unit calibration: measured sensor voltage in distilled
// water (0 NTU reference). Re-measure and update this if the
// sensor is ever replaced.
#define NTU_CLEAR_WATER_VOLTAGE 3.81325f

// The DFRobot-style curve below assumes clear water reads
// 4.2 V. Real units vary, so this offset shifts our measured
// voltage into that curve's expected input range.
#define NTU_VOLTAGE_OFFSET (4.2f - NTU_CLEAR_WATER_VOLTAGE)

// ==========================================================
// GPIO
// ==========================================================

#define RELAY_PIN 5
#define BUZZER_PIN 14

#define RELAY_ACTIVE_LEVEL HIGH
#define RELAY_INACTIVE_LEVEL LOW

#define BUZZER_ACTIVE_LEVEL HIGH
#define BUZZER_INACTIVE_LEVEL LOW

#define ONE_WIRE_BUS 4

#define TOUCH_TFT_PIN 35
#define TOUCH_RELAY_OVERRIDE_PIN 34

// ==========================================================
// STATUS LEDS
// ==========================================================

#define LED_RED_PIN 32
#define LED_YELLOW_PIN 33
#define LED_GREEN_PIN 2

#define LED_ACTIVE_LEVEL HIGH
#define LED_INACTIVE_LEVEL LOW

// ==========================================================
// TFT
// ==========================================================

#define TFT_SCLK 18
#define TFT_MOSI 23
#define TFT_MISO 19
#define TFT_DC 16
#define TFT_RST 17
#define TFT_CS -1

#define TFT_WIDTH 240
#define TFT_HEIGHT 240
#define TFT_ROTATION 0

#define TFT_UPDATE_INTERVAL_MS 500UL
#define TFT_SLEEP_TIMEOUT_MS 300000UL
#define TFT_SECRET_PAGE_HOLD_MS 10000UL

// ==========================================================
// OLED
// ==========================================================

#define OLED_WIDTH 128
#define OLED_HEIGHT 64
#define OLED_RESET -1
#define OLED_UPDATE_INTERVAL_MS 1000UL
#define OLED_PAGE_INTERVAL_MS 5000UL

// ==========================================================
// SD CARD
// ==========================================================

#define SD_CS_PIN 13
#define SD_SCK_PIN 25
#define SD_MOSI_PIN 26
#define SD_MISO_PIN 27

#define SD_LOG_INTERVAL_MS 60000UL

// ==========================================================
// ABSOLUTE SAFETY
// ==========================================================
//
// This rule remains active even when trend history is disabled.
//

#define ABSOLUTE_TDS_RELAY_STOP_PPM 500.0f

// ==========================================================
// TREND ENGINE
// ==========================================================

#define TREND_ENGINE_ENABLED 1

#define TREND_SAMPLE_INTERVAL_MS 60000UL

#define TREND_HOURS_PER_DAY 24
#define TREND_DAYS_PER_WEEK 7

#define TREND_WEEK0_TDS_BASELINE_PPM 200.0f
#define TREND_BASELINE_RISE_PERCENT 0.50f
#define TREND_WARNING_PERCENT_OF_ABSOLUTE 0.80f

#define TREND_MAX_POINTS 5
#define TREND_WATCH_POINTS 3
#define TREND_MAINTENANCE_POINTS 4
#define TREND_WARNING_POINTS 5

#define TREND_WEEKLY_TDS_DEADBAND_PPM 5.0f
#define TREND_HOUR_MS (60UL * 60UL)
#define TREND_HOUR_SECONDS (60UL * 60UL)

// ==========================================================
// WATER QUALITY
// ==========================================================

#define WATER_SCORE_PH_WEIGHT 35.0f
#define WATER_SCORE_TDS_WEIGHT 25.0f
#define WATER_SCORE_NTU_WEIGHT 30.0f
#define WATER_SCORE_TEMP_WEIGHT 10.0f

// If true, UNSAFE water quality turns relay off.
// Absolute TDS safety always turns relay off regardless.
#define UNSAFE_WATER_STOPS_RELAY 1

// Drinking-water certification is not inferred from these sensors.
#define DRINKING_WATER_RECOMMENDATION_ENABLED 0

// ==========================================================
// TOUCH
// ==========================================================

#define TOUCH_DEBOUNCE_MS 300UL

// GPIO34 and GPIO35 are input-only pins.
// External pull-up/pull-down resistors are required.
#define TOUCH_INPUT_ACTIVE_LEVEL HIGH

// ==========================================================
// WIFI / BLYNK TIMING
// ==========================================================

#define WIFI_RECONNECT_INTERVAL_MS 60000UL
#define WIFI_STATUS_PRINT_INTERVAL_MS 10000UL

#define BLYNK_UPDATE_INTERVAL_MS 2000UL
#define BLYNK_RECONNECT_INTERVAL_MS 10000UL

#endif