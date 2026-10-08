#pragma once

#include <Arduino.h>

#define DEFAULT_INTERVAL_MIN 30
#define MIN_INTERVAL_MIN 1
#define MAX_INTERVAL_MIN 180  // ESP8266 deep sleep limit is ~3.5 h

// Per-device settings entered in the config portal and stored in LittleFS
struct DeviceConfig {
  char wifiSsid[33];
  char wifiPassword[65];
  char blynkToken[33];
  uint16_t intervalMin;
};

// Loads the config from flash. Returns false if it is missing or incomplete.
bool loadConfig(DeviceConfig &config);

// Writes the config to flash. Returns false on file system error.
bool saveConfig(const DeviceConfig &config);
