#pragma once

#include <Arduino.h>

// Values collected during one wake cycle. Invalid values are skipped by the sink.
struct Measurement {
  float temperatureC;
  bool temperatureValid;
  float batteryVoltage;
  uint8_t batteryPercent;
  bool batteryValid;
};

// Backend-independent interface for publishing measurements (Blynk, MQTT, HTTP...).
// Implementations assume WiFi is already connected.
class DataSink {
public:
  virtual ~DataSink() = default;

  // Connects to the backend. Returns false on failure or timeout.
  virtual bool begin() = 0;

  // Publishes one measurement. Returns false if it could not be sent.
  virtual bool send(const Measurement &measurement) = 0;

  // Flushes pending data and disconnects. Called before going to sleep.
  virtual void end() = 0;
};
