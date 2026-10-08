#include <ArduinoJson.h>
#include <LittleFS.h>

#include "DeviceConfig.h"

#define CONFIG_PATH "/config.json"

static uint16_t clampInterval(uint16_t intervalMin) {
  return constrain(intervalMin, MIN_INTERVAL_MIN, MAX_INTERVAL_MIN);
}

// Formats the partition on first use, when it doesn't hold a file system yet
static bool mountFileSystem() {
  return LittleFS.begin(true);
}

bool loadConfig(DeviceConfig &config) {
  config.wifiSsid[0] = '\0';
  config.wifiPassword[0] = '\0';
  config.blynkToken[0] = '\0';
  config.intervalMin = DEFAULT_INTERVAL_MIN;

  if (!mountFileSystem()) {
    Serial.println("Config: failed to mount LittleFS");
    return false;
  }

  File file = LittleFS.open(CONFIG_PATH, "r");
  if (!file) {
    Serial.println("Config: no saved config");
    return false;
  }

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, file);
  file.close();
  if (error) {
    Serial.printf("Config: invalid file (%s)\n", error.c_str());
    return false;
  }

  strlcpy(config.wifiSsid, doc["wifi_ssid"] | "", sizeof(config.wifiSsid));
  strlcpy(config.wifiPassword, doc["wifi_password"] | "", sizeof(config.wifiPassword));
  strlcpy(config.blynkToken, doc["blynk_token"] | "", sizeof(config.blynkToken));
  config.intervalMin = clampInterval(doc["interval_min"] | DEFAULT_INTERVAL_MIN);

  if (config.wifiSsid[0] == '\0' || config.blynkToken[0] == '\0') {
    Serial.println("Config: incomplete, WiFi SSID or Blynk token missing");
    return false;
  }

  Serial.printf("Config: loaded, SSID '%s', interval %u min\n", config.wifiSsid,
                config.intervalMin);
  return true;
}

bool saveConfig(const DeviceConfig &config) {
  if (!mountFileSystem()) {
    Serial.println("Config: failed to mount LittleFS");
    return false;
  }

  File file = LittleFS.open(CONFIG_PATH, "w");
  if (!file) {
    Serial.println("Config: failed to open file for writing");
    return false;
  }

  JsonDocument doc;
  doc["wifi_ssid"] = config.wifiSsid;
  doc["wifi_password"] = config.wifiPassword;
  doc["blynk_token"] = config.blynkToken;
  doc["interval_min"] = config.intervalMin;

  bool written = serializeJson(doc, file) > 0;
  file.close();
  Serial.println(written ? "Config: saved" : "Config: write failed");
  return written;
}
