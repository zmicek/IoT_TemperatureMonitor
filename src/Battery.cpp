#include "Battery.h"

#define BATTERY_ADC_PIN A0
#define BATTERY_SAMPLES 16
#define ADC_FULL_SCALE 1024.0f  // ESP8266 analogRead returns 0-1024 for 0-1.0 V

struct VoltagePoint {
  float voltage;
  uint8_t percent;
};

// Typical Li-ion discharge curve at rest. 0 % is set to BATTERY_CUTOFF_V, not to the
// cell's absolute minimum, because the regulator and ESP stop working before that.
static const VoltagePoint DISCHARGE_CURVE[] = {
    {4.20f, 100}, {4.10f, 90}, {4.00f, 80}, {3.92f, 70}, {3.86f, 60}, {3.82f, 50},
    {3.79f, 40},  {3.75f, 30}, {3.70f, 20}, {3.62f, 10}, {3.50f, 5},  {3.30f, 0},
};
static const size_t DISCHARGE_CURVE_SIZE = sizeof(DISCHARGE_CURVE) / sizeof(DISCHARGE_CURVE[0]);

float readBatteryVoltage() {
  uint32_t sum = 0;
  for (uint8_t i = 0; i < BATTERY_SAMPLES; i++) {
    sum += analogRead(BATTERY_ADC_PIN);
    delay(2);
  }
  float adcVoltage = (sum / (float)BATTERY_SAMPLES) / ADC_FULL_SCALE;
  return adcVoltage * BATTERY_DIVIDER_RATIO * BATTERY_CALIBRATION;
}

bool isBatteryPresent(float voltage) {
  return voltage >= BATTERY_PRESENT_MIN_V;
}

// Linear interpolation between the points of the discharge curve
uint8_t batteryPercent(float voltage) {
  if (voltage >= DISCHARGE_CURVE[0].voltage) {
    return 100;
  }
  for (size_t i = 1; i < DISCHARGE_CURVE_SIZE; i++) {
    const VoltagePoint &upper = DISCHARGE_CURVE[i - 1];
    const VoltagePoint &lower = DISCHARGE_CURVE[i];
    if (voltage >= lower.voltage) {
      float fraction = (voltage - lower.voltage) / (upper.voltage - lower.voltage);
      return lower.percent + (uint8_t)(fraction * (upper.percent - lower.percent) + 0.5f);
    }
  }
  return 0;
}
