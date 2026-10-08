#include <Arduino.h>
#include <WiFi.h>
#include <esp_sleep.h>
#include <OneWire.h>
#include <DallasTemperature.h>

#include "Battery.h"
#include "DeviceConfig.h"
#include "ConfigPortal.h"
#include "DataSink.h"
#include "BlynkSink.h"

#define SERIAL_BAUD 115200

// DS18B20 data pin. Needs a 4.7k pull-up to 3.3V.
#define ONE_WIRE_PIN 4
#define DS18B20_RESOLUTION 12  // 9-12 bits
#define DS18B20_CONVERSION_TIMEOUT_MS 1000

#define WIFI_CONNECT_TIMEOUT_MS 10000
#define BACKEND_CONNECT_TIMEOUT_MS 5000

// Some SuperMini batches have a poorly tuned antenna and connect more reliably with
// lower transmit power, which also lowers current peaks. Uncomment if WiFi is unstable.
// #define WIFI_TX_POWER WIFI_POWER_8_5dBm

// Config portal is opened on first boot, or when the BOOT button is held while awake
#define CONFIG_BUTTON_PIN 9  // BOOT button on SuperMini, active low
#define STATUS_LED_PIN 8     // SuperMini blue LED, active low, lit while the portal is open
#define CONFIG_PORTAL_TIMEOUT_S 300

OneWire oneWire(ONE_WIRE_PIN);
DallasTemperature sensors(&oneWire);

DeviceConfig config;

BlynkSink blynkSink(config.blynkToken, BACKEND_CONNECT_TIMEOUT_MS);
DataSink &dataSink = blynkSink;

void initDS18B20();
void printAddress(const DeviceAddress address);
bool readTemperature(float &temperatureC);
bool connectWiFi(uint32_t timeoutMs);
bool publish(DataSink &sink, const Measurement &measurement);
bool configButtonPressed();
void enterConfigMode(bool haveConfig);
void readBattery(Measurement &measurement);
const char *wakeReason();
void goToSleep();
void sleepForever();

// The whole measure-send-sleep cycle runs here; each wake-up is a fresh boot.
void setup() {
  // Serial runs over the built-in USB. Without a USB host the output is dropped without
  // blocking, so it costs no awake time on battery.
  Serial.begin(SERIAL_BAUD);
  Serial.println();
  Serial.printf("IoT Temperature Monitor - wake-up (%s)\n", wakeReason());

  pinMode(CONFIG_BUTTON_PIN, INPUT_PULLUP);

  // Battery first: the ADC must be read before WiFi starts
  Measurement measurement;
  readBattery(measurement);

  bool haveConfig = loadConfig(config);
  if (!haveConfig || configButtonPressed()) {
    enterConfigMode(haveConfig);
  }

  initDS18B20();

  // Start the conversion without blocking, so it runs while WiFi connects
  sensors.setWaitForConversion(false);
  sensors.requestTemperatures();

  bool wifiConnected = connectWiFi(WIFI_CONNECT_TIMEOUT_MS);

  measurement.temperatureValid = readTemperature(measurement.temperatureC);

  if (wifiConnected && (measurement.temperatureValid || measurement.batteryValid)) {
    publish(dataSink, measurement);
  }

  // Second check, so the button can be pressed any time during the awake period
  if (configButtonPressed()) {
    enterConfigMode(true);
  }

  goToSleep();
}

void loop() {
  // Never reached: setup() always ends in deep sleep or restart
}

// Initializes the OneWire bus and reports every DS18B20 found on it
void initDS18B20() {
  sensors.begin();

  uint8_t count = sensors.getDeviceCount();
  Serial.printf("DS18B20: found %u device(s) on GPIO%d\n", count, ONE_WIRE_PIN);
  if (count == 0) {
    Serial.println("DS18B20: no sensor found - check wiring and pull-up resistor");
    return;
  }

  Serial.printf("DS18B20: parasite power %s\n", sensors.isParasitePowerMode() ? "ON" : "OFF");

  DeviceAddress address;
  for (uint8_t i = 0; i < count; i++) {
    if (!sensors.getAddress(address, i)) {
      Serial.printf("DS18B20: unable to read address of device %u\n", i);
      continue;
    }
    sensors.setResolution(address, DS18B20_RESOLUTION);
    Serial.printf("DS18B20: device %u address ", i);
    printAddress(address);
    Serial.printf(", resolution %u bit\n", sensors.getResolution(address));
  }
}

void printAddress(const DeviceAddress address) {
  for (uint8_t i = 0; i < 8; i++) {
    Serial.printf("%02X", address[i]);
  }
}

// Waits for the conversion started in setup() and reads the first sensor
bool readTemperature(float &temperatureC) {
  uint32_t start = millis();
  while (!sensors.isConversionComplete()) {
    if (millis() - start > DS18B20_CONVERSION_TIMEOUT_MS) {
      Serial.println("DS18B20: conversion timeout");
      return false;
    }
    delay(10);
  }

  temperatureC = sensors.getTempCByIndex(0);
  if (temperatureC == DEVICE_DISCONNECTED_C) {
    Serial.println("DS18B20: read failed");
    return false;
  }
  Serial.printf("DS18B20: temperature %.2f C\n", temperatureC);
  return true;
}

// Reads the battery and stops the device for good if the cell is nearly empty
void readBattery(Measurement &measurement) {
  measurement.batteryVoltage = readBatteryVoltage();
  measurement.batteryValid = isBatteryPresent(measurement.batteryVoltage);

  if (!measurement.batteryValid) {
    Serial.printf("Battery: not detected (%.2f V), powered from USB?\n",
                  measurement.batteryVoltage);
    return;
  }

  measurement.batteryPercent = batteryPercent(measurement.batteryVoltage);
  Serial.printf("Battery: %.2f V, %u %%\n", measurement.batteryVoltage,
                measurement.batteryPercent);

  if (measurement.batteryVoltage < BATTERY_CUTOFF_V) {
    // No WiFi attempt: the current peaks could brown out the regulator and cause
    // a reset loop. Sleeping without a timer wakes only on RST, after recharging.
    Serial.println("Battery: below cutoff, sleeping until reset");
    sleepForever();
  }
}

bool connectWiFi(uint32_t timeoutMs) {
  WiFi.persistent(false);  // don't rewrite WiFi config in flash on every wake-up
  WiFi.mode(WIFI_STA);
  WiFi.begin(config.wifiSsid, config.wifiPassword);
#ifdef WIFI_TX_POWER
  WiFi.setTxPower(WIFI_TX_POWER);
#endif

  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - start > timeoutMs) {
      Serial.println("WiFi: connection timeout");
      return false;
    }
    delay(50);
  }
  Serial.printf("WiFi: connected in %lu ms, IP %s\n", millis() - start,
                WiFi.localIP().toString().c_str());
  return true;
}

bool publish(DataSink &sink, const Measurement &measurement) {
  if (!sink.begin()) {
    Serial.println("Backend: connection failed");
    return false;
  }
  bool sent = sink.send(measurement);
  sink.end();
  Serial.println(sent ? "Backend: measurement sent" : "Backend: send failed");
  return sent;
}

bool configButtonPressed() {
  return digitalRead(CONFIG_BUTTON_PIN) == LOW;
}

// Runs the config portal. Never returns: restarts after saving, otherwise sleeps.
void enterConfigMode(bool haveConfig) {
  pinMode(STATUS_LED_PIN, OUTPUT);
  digitalWrite(STATUS_LED_PIN, LOW);

  if (runConfigPortal(config, CONFIG_PORTAL_TIMEOUT_S) && saveConfig(config)) {
    Serial.println("Restarting with new config");
    Serial.flush();
    ESP.restart();
  }

  digitalWrite(STATUS_LED_PIN, HIGH);
  if (haveConfig) {
    // Portal opened by the button and abandoned: keep the old config
    goToSleep();
  }

  // Not configured yet: sleep until RST is pressed instead of reopening the portal
  // every interval and draining the battery
  Serial.println("Not configured, sleeping until reset");
  sleepForever();
}

const char *wakeReason() {
  if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_TIMER) {
    return "timer";
  }
  switch (esp_reset_reason()) {
    case ESP_RST_POWERON:
      return "power-on or RST button";
    case ESP_RST_DEEPSLEEP:
      return "deep sleep";
    case ESP_RST_SW:
      return "restart";
    case ESP_RST_BROWNOUT:
      return "brownout";
    case ESP_RST_PANIC:
    case ESP_RST_INT_WDT:
    case ESP_RST_TASK_WDT:
    case ESP_RST_WDT:
      return "crash";
    default:
      return "other";
  }
}

// Sleeps for the rest of the interval, so wake-ups stay ~config.intervalMin apart
void goToSleep() {
  const uint64_t intervalUs = config.intervalMin * 60ULL * 1000000ULL;
  uint64_t awakeUs = (uint64_t)millis() * 1000ULL;
  uint64_t sleepUs = awakeUs < intervalUs ? intervalUs - awakeUs : intervalUs;

  Serial.printf("Awake for %lu ms, sleeping for %lu s\n", millis(),
                (unsigned long)(sleepUs / 1000000ULL));
  Serial.flush();
  esp_sleep_enable_timer_wakeup(sleepUs);
  esp_deep_sleep_start();
}

// Deep sleep with no wake-up source: only RST (or power cycle) starts the device again
void sleepForever() {
  Serial.flush();
  esp_deep_sleep_start();
}
