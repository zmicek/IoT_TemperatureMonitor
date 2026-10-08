# IoT_TemperatureMonitor
Simple IoT project using ESP controller to monitor temperature for later analysis

## Hardware

- ESP32-C3 SuperMini
- DS18B20 temperature sensor (3-wire version)
- 4.7 kΩ resistor
- 18650 Li-ion battery (protected cell, or a charger module with protection)
- 2 × 1 MΩ resistors and a 100 nF capacitor (battery measurement)

## Wiring

| Pin | Connected to |
|---|---|
| GPIO4 | DS18B20 DATA |
| GPIO3 | Battery voltage divider |
| GPIO8 | Blue on-board LED (status, active low) |
| GPIO9 | BOOT button (config portal, active low) |
| 5V | Battery + |
| 3V3 | DS18B20 VDD, DS18B20 pull-up |
| GND | Battery −, DS18B20 GND, divider |

### Temperature sensor

The sensor uses normal (external) power, not parasite power.

| Wire (usual color) | DS18B20 | ESP32-C3 SuperMini |
|---|---|---|
| red | VDD | 3V3 |
| black | GND | GND |
| yellow / white | DATA | GPIO4 |

A **4.7 kΩ pull-up resistor between DATA and 3V3 is required**. Without it the sensor will not be detected.

Wire colors can differ between manufacturers, so check the seller's description or the datasheet.

To use a different data pin, change `ONE_WIRE_PIN` in [src/main.cpp](src/main.cpp). Avoid GPIO2, GPIO8 and GPIO9: they are strapping pins that decide the boot mode.

### Normal vs. parasite power

In parasite power mode the DS18B20 is powered through the DATA line: VDD is tied to GND and only two wires are used. During temperature conversion the microcontroller has to drive the line HIGH directly ("strong pull-up"), because the pull-up resistor can't supply enough current.

Normal power, with VDD on 3.3V, is more reliable, especially with long cables or several sensors on one bus. It is what this project uses. The `DallasTemperature` library detects the mode automatically, so no code change is needed.

### Battery

Connect **battery + to the 5V pin** and battery − to GND. The on-board regulator turns it into 3.3 V.

**Don't connect USB while the battery is on the 5V pin**, unless you have checked that your board has a diode between USB and the 5V pin. Without one, USB 5 V goes straight into the 18650. Either disconnect the battery while using USB, or put a Schottky diode (e.g. 1N5817) between battery + and the 5V pin, anode on the battery side. The diode costs about 0.2-0.3 V of battery range.

Charge the battery with a separate charger module, for example TP4056 with DW01 protection.

### Battery measurement

The ESP32-C3 ADC reads up to about 2.5 V, so the battery voltage (up to 4.2 V) goes through a divider that halves it:

```
battery + ── 1M ──┬── 1M ── GND
                  │
                GPIO3
                  │
                100 nF
                  │
                 GND
```

- Measure at the battery terminal, before the regulator (and before the diode, if you use one).
- The capacitor is needed: with 1 MΩ resistors the divider can't charge the ADC sampling capacitor fast enough on its own, and readings would be too low and noisy.
- The divider draws about 2 µA (4.2 V / 2 MΩ).
- GPIO3 is on ADC1. Don't move it to GPIO5: that is ADC2, which doesn't work while WiFi is on.

**Calibration:** `analogReadMilliVolts()` uses the factory ADC calibration, but resistor tolerances still add error. Measure the battery with a multimeter, compare it with the voltage in the serial output, and set `BATTERY_CALIBRATION` in [include/Battery.h](include/Battery.h) to `measured / reported`.

## How it works

Every 30 minutes (configurable) the ESP wakes up from deep sleep and:

1. measures the battery voltage (before WiFi, because the ADC is noisy while the radio is on),
2. starts a DS18B20 temperature conversion,
3. connects to WiFi while the conversion runs,
4. sends temperature and battery data to Blynk,
5. goes back to deep sleep for the rest of the interval.

Each wake-up is a fresh boot, so the whole cycle runs in `setup()` and `loop()` is never reached. If WiFi or Blynk can't connect within the timeout, the measurement is skipped and the ESP goes back to sleep, so a network outage doesn't drain the battery.

The ESP32-C3 wakes itself from deep sleep with its internal RTC timer. No extra wiring is needed.

The interval is set in the config portal. Timeouts are set at the top of [src/main.cpp](src/main.cpp) (`WIFI_CONNECT_TIMEOUT_MS`, `BACKEND_CONNECT_TIMEOUT_MS`, `CONFIG_PORTAL_TIMEOUT_S`).

### Battery protection

If the battery is below **3.3 V** (`BATTERY_CUTOFF_V`), the ESP doesn't start WiFi and goes to deep sleep with no wake-up source. This protects the cell from over-discharge and avoids a reset loop caused by WiFi current peaks on a weak battery. After recharging, press RST.

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

1. The blue LED turns on.
2. On your phone, connect to the WiFi network `TempMonitor-XXXXXX`. The suffix is the end of the MAC address, so several devices can be told apart.
3. The setup page opens automatically. If it doesn't, open `192.168.4.1` in a browser.
4. Tap **Configure WiFi**, pick your network, and enter the password, the Blynk auth token and the measurement interval (1-180 minutes).
5. Save. Once the ESP connects to your network, it stores the settings and restarts into the normal cycle.

The portal closes after 5 minutes without saving. If the device was not configured before, it then sleeps until RST is pressed, so an unconfigured device doesn't drain the battery.

### Changing the settings later

Press **RST**, release it, then press and hold **BOOT** until the blue LED turns on (up to ~10 s, while the ESP is awake). The portal opens with the current token and interval filled in. If you leave without saving, the old settings are kept.

Don't hold BOOT while pressing RST: that starts the ESP bootloader instead. Just press RST again.

A lost WiFi connection, for example while the router is off, never opens the portal on its own. The ESP skips that measurement and tries again on the next wake-up.

## Build and upload

Requires [PlatformIO](https://platformio.org/). The libraries (`OneWire`, `DallasTemperature`, `Blynk`, `WiFiManager`, `ArduinoJson`) are downloaded automatically on first build.

```
pio run -t upload
pio device monitor
```

The ESP32-C3 has USB built into the chip, so the USB port only exists while the ESP is awake. To upload while it is sleeping:

1. Hold **BOOT**.
2. Press and release **RST**.
3. Release BOOT. The USB port appears and stays.
4. Run `pio run -t upload`.
5. Press RST to start the new firmware.

The serial monitor also disconnects every time the ESP goes to sleep. Lines printed right after wake-up can be missing, because the computer needs a moment to reconnect to the USB port.

If the build fails with `No module named 'intelhex'`, install it into the PlatformIO Python environment:

```
~/.platformio/penv/Scripts/python -m pip install intelhex
```

## Expected serial output

```
IoT Temperature Monitor - wake-up (timer)
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

Not measured yet. Figures are typical values for the ESP32-C3 SuperMini with the power LED removed.

| Phase | Current | Duration per cycle |
|---|---|---|
| Awake (boot, WiFi, Blynk) | ~80-100 mA average, peaks up to ~350 mA | ~3-6 s |
| Deep sleep | ~5 µA chip, plus regulator quiescent current (1-50 µA, depends on the board), ~2 µA divider, ~1 µA DS18B20 | rest of the 30 min |

That comes to roughly **4-8 mAh per day**, about 9-12 months on a 2000 mAh battery. Most of the energy goes to the WiFi connection, not to sleep.

**The power LED has to be removed.** It draws around 1-3 mA all the time, which is more than everything else in sleep combined and cuts battery life to a few weeks. See [docs/board-modification.md](docs/board-modification.md).

Deep sleep is used instead of light sleep, because light sleep keeps more of the chip powered and draws over 100 µA.

## Troubleshooting

| Symptom | Likely cause |
|---|---|
| `found 0 device(s)` | Missing pull-up resistor, or DATA not on GPIO4 |
| `parasite power ON` | VDD (red wire) not connected to 3V3, or wire colors differ on your sensor |
| USB port disappears | Normal: the ESP is in deep sleep. For upload, use BOOT + RST |
| `WiFi: connection timeout` | Router off, weak signal, or WiFi password changed (reopen the config portal). On some SuperMini boards, enabling `WIFI_TX_POWER` in [src/main.cpp](src/main.cpp) helps |
| `Backend: connection failed` | Wrong Blynk auth token, or template ID/name in `blynk_template.h` don't match the device |
| Wake-up reason `brownout` | Battery nearly empty, or the regulator can't supply WiFi current peaks. Add a 470-1000 µF capacitor between 3V3 and GND |
| Portal network doesn't appear | Device already configured: use RST + BOOT to reopen the portal |
| `Battery: not detected` while on battery | Divider not connected to GPIO3, or battery − not on GND |
| Battery voltage off by more than a few % | Missing 100 nF capacitor on GPIO3, or calibrate `BATTERY_CALIBRATION` |
| `Battery: below cutoff, sleeping until reset` | Battery empty: recharge it, then press RST |
| `Not configured, sleeping until reset` | Portal timed out on first setup: press RST to open it again |
