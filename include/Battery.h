#pragma once

#include <Arduino.h>

// 18650 Li-ion battery measured on GPIO3 (ADC1; ADC2 doesn't work while WiFi is on).
// External divider: battery + -- 1M -- GPIO3 -- 1M -- GND, with 100 nF from GPIO3 to GND.
// A full battery (4.2 V) gives 2.1 V on the pin, within the ADC range (~2.5 V).
#define BATTERY_ADC_PIN 3
#define BATTERY_DIVIDER_RATIO 2.0f  // (1M + 1M) / 1M

// Correction for resistor tolerance and ADC error. Set to
// (voltage measured with a multimeter) / (voltage reported by the device).
#define BATTERY_CALIBRATION 1.0f

// Below this the battery is considered not connected (e.g. running from USB)
#define BATTERY_PRESENT_MIN_V 2.5f

// Below this the device stops waking up to protect the cell from over-discharge
#define BATTERY_CUTOFF_V 3.3f

// Reads battery voltage, averaged over several samples.
// Call before WiFi is started: the ADC is noisy while the radio is active.
float readBatteryVoltage();

bool isBatteryPresent(float voltage);

// Approximate state of charge for a Li-ion cell at rest, 0-100 %
uint8_t batteryPercent(float voltage);
