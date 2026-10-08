# IoT_TemperatureMonitor
Simple IoT project using ESP controller to monitor temperature for later analysis

## Hardware

- ESP8266 ESP-12E (e.g. NodeMCU or Wemos D1 mini)
- DS18B20 temperature sensor (3-wire version)
- 4.7 kΩ resistor
- 18650 Li-ion battery
- 120 kΩ resistor (battery measurement)

## Wiring

The sensor uses normal (external) power, not parasite power.

| Wire (usual color) | DS18B20 | ESP-12E |
|---|---|---|
| red | VDD | 3.3V |
| black | GND | GND |
| yellow / white | DATA | GPIO4 (D2) |

A **4.7 kΩ pull-up resistor between DATA and 3.3V is required**. Without it the sensor will not be detected.

Wire colors can differ between manufacturers, so check the seller's description or the datasheet.

To use a different data pin, change `ONE_WIRE_PIN` in [src/main.cpp](src/main.cpp).

### Deep sleep wake-up

**Connect GPIO16 (D0) to RST.** The RTC timer wakes the chip by pulling GPIO16 low, which resets it. Without this wire the ESP never wakes up from deep sleep.

If uploading fails with the wire in place, disconnect it while flashing.

### Battery measurement

Connect **battery + through a 120 kΩ resistor to A0**. Battery − goes to GND.

NodeMCU already has a 220k/100k divider on A0, which gives 3.2 V full scale. That is less than a full 18650 (4.2 V). The external 120 kΩ in series extends full scale to ~4.4 V:

```
battery + ── 120k ── A0 ──[ on board: 220k ── ADC ── 100k ── GND ]
```

Measure at the battery terminal, before any regulator.

Check the divider on your board before relying on the readings: with the board unpowered, the resistance between A0 and GND should be about 320 kΩ. If your board uses different resistors, update `BATTERY_DIVIDER_RATIO` in [include/Battery.h](include/Battery.h).

The divider draws a constant ~10 µA from the battery (4.2 V / 440 kΩ).

**Calibration:** the ESP8266 ADC and resistor tolerances can be off by several percent. Measure the battery with a multimeter, compare it with the voltage in the serial output, and set `BATTERY_CALIBRATION` to `measured / reported`.

### Normal vs. parasite power

In parasite power mode the DS18B20 is powered through the DATA line: VDD is tied to GND and only two wires are used. During temperature conversion the microcontroller has to drive the line HIGH directly ("strong pull-up"), because the pull-up resistor can't supply enough current.

Normal power, with VDD on 3.3V, is more reliable, especially with long cables or several sensors on one bus. It is what this project uses. The `DallasTemperature` library detects the mode automatically, so no code change is needed.

## How it works

Every 30 minutes (configurable) the ESP wakes up from deep sleep and:

1. measures the battery voltage (before WiFi, because the ADC is noisy while the radio is on),
2. starts a DS18B20 temperature conversion,
3. connects to WiFi while the conversion runs,
4. sends temperature and battery data to Blynk,
5. goes back to deep sleep for the rest of the interval.

Each wake-up is a fresh boot, so the whole cycle runs in `setup()` and `loop()` is never reached. If WiFi or Blynk can't connect within the timeout, the measurement is skipped and the ESP goes back to sleep, so a network outage doesn't drain the battery.

The interval is set in the config portal. Timeouts are set at the top of [src/main.cpp](src/main.cpp) (`WIFI_CONNECT_TIMEOUT_MS`, `BACKEND_CONNECT_TIMEOUT_MS`, `CONFIG_PORTAL_TIMEOUT_S`).

### Battery protection

If the battery is below **3.3 V** (`BATTERY_CUTOFF_V`), the ESP doesn't start WiFi and goes to deep sleep without a wake-up timer. This protects the cell from over-discharge and avoids a reset loop caused by WiFi current peaks on a weak battery. After recharging, press RST.

Battery charge in percent comes from a typical Li-ion discharge curve, where 3.3 V is 0 %. It is an estimate: the curve differs between cells and with temperature.

If the measured voltage is below 2.5 V, the battery is treated as not connected, for example while running from USB. The device then works normally and doesn't send battery data.

To get an alert, add a Blynk automation or event on V1, for example when the voltage drops below 3.5 V.

### Data backend

Publishing goes through the `DataSink` interface in [include/DataSink.h](include/DataSink.h) (`begin()`, `send()`, `end()`). The current implementation is `BlynkSink`. To switch to another backend (MQTT, HTTP...), implement `DataSink` and change the `dataSink` reference in [src/main.cpp](src/main.cpp).

## Configuration

All devices run the same firmware. Per-device settings are entered over WiFi from a phone and stored in flash (LittleFS), so they survive power loss and reflashing of the same firmware.

### Blynk template (once, before building)

1. In the [Blynk console](https://blynk.cloud), create a template with these datastreams:

   | Pin | Name | Type | Unit | Range |
   |---|---|---|---|---|
   | V0 | Temperature | Double | °C | -55 to 125 |
   | V1 | Battery voltage | Double | V | 0 to 4.5 |
   | V2 | Battery charge | Integer | % | 0 to 100 |

2. Copy the template ID and template name into [include/blynk_template.h](include/blynk_template.h). They are the same for all devices.
3. For each physical device, create a device from that template and note its auth token.

### Device setup (config portal)

On first boot, with no saved config, the ESP opens a config portal:

1. The blue LED on the ESP module turns on.
2. On your phone, connect to the WiFi network `TempMonitor-XXXXXX`. The suffix is the chip ID, so several devices can be told apart.
3. The setup page opens automatically. If it doesn't, open `192.168.4.1` in a browser.
4. Tap **Configure WiFi**, pick your network, and enter the password, the Blynk auth token and the measurement interval (1-180 minutes).
5. Save. Once the ESP connects to your network, it stores the settings and restarts into the normal cycle.

The portal closes after 5 minutes without saving. If the device was not configured before, it then sleeps until RST is pressed, so an unconfigured device doesn't drain the battery.

### Changing the settings later

Press **RST**, release it, then press and hold **FLASH** until the blue LED turns on (up to ~10 s, while the ESP is awake). The portal opens with the current token and interval filled in. If you leave without saving, the old settings are kept.

Don't hold FLASH while pressing RST: that starts the ESP bootloader instead. Just press RST again.

A lost WiFi connection, for example while the router is off, never opens the portal on its own. The ESP skips that measurement and tries again on the next wake-up.

## Build and upload

Requires [PlatformIO](https://platformio.org/). The libraries (`OneWire`, `DallasTemperature`, `Blynk`, `WiFiManager`, `ArduinoJson`) are downloaded automatically on first build.

```
pio run -t upload
pio device monitor
```

The serial port runs at 115200 baud.

## Expected serial output

```
IoT Temperature Monitor - wake-up (Deep-Sleep Wake)
Battery: 3.95 V, 74 %
Config: loaded, SSID 'MyHomeWiFi', interval 30 min
DS18B20: found 1 device(s) on GPIO4
DS18B20: parasite power OFF
DS18B20: device 0 address 28XXXXXXXXXXXXXX, resolution 12 bit
WiFi: connected in 2800 ms, IP 192.168.1.50
DS18B20: temperature 23.44 C
Backend: measurement sent
Awake for 3900 ms, sleeping for 1796 s
```

A DS18B20 address always starts with `28`, the family code. Resolution is set to 12 bits (±0.0625 °C, about 750 ms per conversion).

## Power consumption (estimate)

Not measured yet. Figures are typical values for the bare ESP-12E module.

| Phase | Current | Duration per cycle |
|---|---|---|
| Awake (boot, WiFi, Blynk) | ~70-80 mA average, peaks up to ~170 mA | ~3-6 s |
| Deep sleep | ~20 µA, plus ~10 µA for the battery divider | rest of the 30 min |

That comes to roughly **3-6 mAh per day**, about 1 year on a 2000 mAh battery. Most of the energy goes to the WiFi connection, not to sleep.

Deep sleep is used instead of light sleep. ESP8266 light sleep draws ~0.5-1 mA (about 20-25 mAh per day) and its timer can't sleep for a full 30 minutes.

Development boards draw much more in sleep: the NodeMCU voltage regulator and USB-serial chip add roughly 5-15 mA. For battery use, run on a bare module or a board with a low-quiescent regulator (e.g. HT7333, MCP1700). See [docs/board-modification.md](docs/board-modification.md) for how to modify a Lolin NodeMCU.

## Troubleshooting

| Symptom | Likely cause |
|---|---|
| `found 0 device(s)` | Missing pull-up resistor, or DATA not on GPIO4 |
| `parasite power ON` | VDD (red wire) not connected to 3.3V, or wire colors differ on your sensor |
| ESP never wakes up | GPIO16 (D0) not connected to RST |
| `WiFi: connection timeout` | Router off, weak signal, or WiFi password changed (reopen the config portal) |
| `Backend: connection failed` | Wrong Blynk auth token, or template ID/name in `blynk_template.h` don't match the device |
| Portal network doesn't appear | Device already configured: use RST + FLASH to reopen the portal |
| `Battery: not detected` while on battery | 120 kΩ resistor not connected to A0, or battery − not on GND |
| Battery voltage off by more than a few % | Calibrate `BATTERY_CALIBRATION`, or check the A0 divider on your board |
| `Battery: below cutoff, sleeping until reset` | Battery empty: recharge it, then press RST |
| `Not configured, sleeping until reset` | Portal timed out on first setup: press RST to open it again |
