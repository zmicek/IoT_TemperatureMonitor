// Must come before the Blynk header: it defines BLYNK_TEMPLATE_ID/NAME
#include "blynk_template.h"

#include <BlynkSimpleEsp8266.h>

#include "BlynkSink.h"

#define BLYNK_PIN_TEMPERATURE V0
#define BLYNK_PIN_BATTERY_VOLTAGE V1
#define BLYNK_PIN_BATTERY_PERCENT V2

BlynkSink::BlynkSink(const char *authToken, uint32_t connectTimeoutMs)
    : _authToken(authToken), _connectTimeoutMs(connectTimeoutMs) {}

bool BlynkSink::begin() {
  Blynk.config(_authToken);
  return Blynk.connect(_connectTimeoutMs);
}

bool BlynkSink::send(const Measurement &measurement) {
  if (!Blynk.connected()) {
    return false;
  }
  if (measurement.temperatureValid) {
    Blynk.virtualWrite(BLYNK_PIN_TEMPERATURE, measurement.temperatureC);
  }
  if (measurement.batteryValid) {
    Blynk.virtualWrite(BLYNK_PIN_BATTERY_VOLTAGE, measurement.batteryVoltage);
    Blynk.virtualWrite(BLYNK_PIN_BATTERY_PERCENT, measurement.batteryPercent);
  }
  Blynk.run();
  return true;
}

void BlynkSink::end() {
  Blynk.run();
  Blynk.disconnect();
}
