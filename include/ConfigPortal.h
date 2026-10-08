#pragma once

#include "DeviceConfig.h"

// Starts a WiFi access point with a captive portal where WiFi credentials,
// Blynk token and measurement interval are entered. Blocks until the device
// connects to the entered network or the timeout expires.
// On success fills in config and returns true. Does not save it to flash.
bool runConfigPortal(DeviceConfig &config, uint16_t timeoutSec);
