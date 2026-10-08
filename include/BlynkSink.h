#pragma once

#include "DataSink.h"

// Publishes measurements to Blynk IoT (blynk.cloud).
// Virtual pins: V0 temperature (C), V1 battery voltage (V), V2 battery charge (%).
class BlynkSink : public DataSink {
public:
  // authToken must stay valid for the lifetime of this object (Blynk keeps the pointer)
  BlynkSink(const char *authToken, uint32_t connectTimeoutMs);

  bool begin() override;
  bool send(const Measurement &measurement) override;
  void end() override;

private:
  const char *_authToken;
  uint32_t _connectTimeoutMs;
};
