#include <WiFiManager.h>

#include "ConfigPortal.h"

bool runConfigPortal(DeviceConfig &config, uint16_t timeoutSec) {
  char intervalText[6];  // fits any uint16_t
  snprintf(intervalText, sizeof(intervalText), "%u", config.intervalMin);

  WiFiManagerParameter blynkTokenParam("blynk_token", "Blynk auth token", config.blynkToken,
                                       sizeof(config.blynkToken) - 1);
  WiFiManagerParameter intervalParam("interval_min", "Measurement interval (minutes)",
                                     intervalText, 3);

  WiFiManager wifiManager;
  wifiManager.addParameter(&blynkTokenParam);
  wifiManager.addParameter(&intervalParam);
  wifiManager.setConfigPortalTimeout(timeoutSec);

  // Last 3 bytes of the MAC in the name tell devices apart when several are being set up
  uint64_t mac = ESP.getEfuseMac();
  const uint8_t *macBytes = (const uint8_t *)&mac;
  char apName[32];
  snprintf(apName, sizeof(apName), "TempMonitor-%02X%02X%02X", macBytes[3], macBytes[4],
           macBytes[5]);
  Serial.printf("Config portal: connect to WiFi '%s' (timeout %u s)\n", apName, timeoutSec);

  // Returns true only once the device has connected with the entered credentials
  if (!wifiManager.startConfigPortal(apName)) {
    Serial.println("Config portal: timeout, nothing saved");
    return false;
  }

  strlcpy(config.wifiSsid, WiFi.SSID().c_str(), sizeof(config.wifiSsid));
  strlcpy(config.wifiPassword, WiFi.psk().c_str(), sizeof(config.wifiPassword));
  strlcpy(config.blynkToken, blynkTokenParam.getValue(), sizeof(config.blynkToken));

  long interval = atol(intervalParam.getValue());
  config.intervalMin = constrain(interval, MIN_INTERVAL_MIN, MAX_INTERVAL_MIN);

  Serial.printf("Config portal: connected to '%s'\n", config.wifiSsid);
  return true;
}
