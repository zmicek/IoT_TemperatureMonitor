# ESP32-C3 SuperMini modification for battery power

The ESP32-C3 chip draws about 5 µA in deep sleep, and the SuperMini has no USB-serial chip. Most of what is left in sleep is the power LED and the voltage regulator. This guide describes how to check and reduce both.

SuperMini boards differ between manufacturers. Figures below are typical or datasheet values, not measurements on this project. Measure your own board before and after each step (see [Measuring current](#measuring-current)).

## Where the current goes

| Component | Sleep current | Action |
|---|---|---|
| Power LED (red) | ~1-3 mA, always on | Remove |
| Voltage regulator | ~1-50 µA, depends on the part | Check the marking |
| ESP32-C3 | ~5 µA | Stays |
| Battery voltage divider | ~2 µA | Stays |
| DS18B20 in standby | ~1 µA | Negligible |

## Step 1: Remove the power LED

Most SuperMini boards have a red LED that lights whenever the board is powered. In sleep it draws more than everything else combined.

Desolder the LED or its series resistor, whichever is easier to reach. Removing either one is enough. The blue LED on GPIO8 is controlled by the firmware and stays off in sleep, so leave it.

## Step 2: Check the regulator

Look at the marking on the small 5-pin regulator near the USB connector and find its datasheet. What matters:

| Property | Why |
|---|---|
| Quiescent current | Adds directly to sleep current. Good parts are in the 1-50 µA range |
| Maximum output current | WiFi transmit peaks reach ~350 mA. A regulator rated for less (some are 200-250 mA) may let the voltage dip and cause brownout resets |
| Dropout voltage | Determines how low the battery can go. At 3.3 V battery and high load, the ESP gets battery voltage minus dropout |

If you see `brownout` as the wake-up reason, add a **470-1000 µF capacitor** between 3V3 and GND close to the board. It covers the short WiFi current peaks.

If the regulator has high quiescent current, you can replace it with a pin-compatible low-quiescent part, or remove it and feed 3V3 from an external regulator (for example HT7333 or MCP1700, both a few µA, but only 250 mA, so add the capacitor).

## Step 3: USB and battery at the same time

On many SuperMini boards the 5V pin is connected to USB VBUS. With the battery on the 5V pin, plugging in USB would push 5 V into the battery.

Check your board with a multimeter in diode mode between the USB connector VBUS and the 5V pin. If there is no diode, either:

- disconnect the battery whenever USB is plugged in, or
- add a Schottky diode (e.g. 1N5817) between battery + and the 5V pin, anode on the battery side. Keep the battery divider on the battery side of the diode.

## Measuring current

Put a multimeter in series with battery +.

- While the ESP is awake, use the **mA range**. WiFi current peaks reach ~350 mA.
- In deep sleep, switch to the **µA range** for accurate readings.

The µA range has a high internal resistance (burden voltage). If the ESP wakes up while the meter is on that range, the supply voltage drops and the ESP may reset. A practical approach:

1. Short the multimeter leads with a jumper wire.
2. Wait until the ESP goes to sleep (the USB port disappears, or the current drops).
3. Remove the jumper and read the sleep current on the µA range.
4. Put the jumper back before the next wake-up.

Measure with USB unplugged. USB powers the board through VBUS and the reading would be meaningless.

### Expected values

| Board state | Deep sleep current |
|---|---|
| Stock SuperMini | ~1-3 mA (mostly the power LED) |
| Power LED removed | ~10-60 µA, depending on the regulator |

If you measure much more after removing the LED, look for other parts on the 3.3 V rail, or a DS18B20 wired for parasite power.

## Charging and battery protection

The firmware stops waking up below 3.3 V, but in deep sleep it still draws current. Over months it can discharge the cell further. Use a **protected 18650 cell**, or a charger module with a protection circuit (for example TP4056 with DW01). The protection circuit itself draws a few µA.
