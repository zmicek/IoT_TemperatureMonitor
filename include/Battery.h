#pragma once

#include <Arduino.h>

// 18650 Li-ion battery measured on A0.
// NodeMCU has an on-board 220k/100k divider on A0 (3.2 V full scale). An external
// 120k resistor in series between battery + and A0 extends full scale to ~4.4 V:
// (120k + 220k + 100k) / 100k = 4.4
#define BATTERY_DIVIDER_RATIO 4.4f

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
