# NodeMCU board modification for battery power

The ESP-12E module draws about 20 µA in deep sleep, but a stock NodeMCU board draws milliamps. Almost all of it goes to the voltage regulator and the USB-serial chip. This guide describes how to reduce sleep current on a **Lolin NodeMCU V3** (CH340G) for running from an 18650 battery.

Figures below are typical or datasheet values, not measurements on this project. Measure your own board before and after each step (see [Measuring current](#measuring-current)).

## Where the current goes

| Component | Sleep current | Action |
|---|---|---|
| AMS1117-3.3 regulator | ~5 mA typical, up to 10 mA | Remove and replace |
| CH340G USB-serial chip | from tens of µA to several mA, depends on how it is powered | Check its supply, cut or remove if needed |
| ESP-12E module | ~20 µA | Stays |
| Battery voltage divider (A0) | ~10 µA | Stays (see [Battery measurement](../README.md#battery-measurement)) |
| DS18B20 in standby | ~1 µA | Negligible |

A stock board is commonly reported at 10-20 mA total in deep sleep. That empties a 2000 mAh battery in about a week. After the modifications below, expect roughly 30-100 µA, which with the WiFi wake-ups gives about 9-12 months.

## Step 1: Replace the AMS1117 regulator

The AMS1117 has high quiescent current and a large dropout voltage (around 1 V). From an 18650 it can't keep the ESP at 3.3 V once the battery drops below ~4.3 V, which is almost immediately. It has to go.

**Remove it.** Powering the board through the 3V3 pin with the AMS1117 still in place is not enough: current can flow back through it. The AMS1117 is the SOT-223 part (3 pins plus a large tab) near the USB connector. Remove it with hot air, or cut its legs and then desolder the tab.

**Power the 3V3 rail with one of these:**

| Option | Wiring | Notes |
|---|---|---|
| 18650 + low-quiescent LDO | battery + → LDO IN, LDO OUT → NodeMCU 3V3, GND common | HT7333 or MCP1700 (a few µA quiescent current, 250 mA output). Add a **470-1000 µF capacitor** on the 3.3 V output, otherwise WiFi current peaks can reset the ESP |
| LiFePO4 cell (3.2 V) | battery + → NodeMCU 3V3 directly | No regulator needed: the cell voltage stays within the ESP range (about 2.5-3.6 V). Simplest and most efficient, but the battery cutoff and charge curve in [include/Battery.h](../include/Battery.h) are for Li-ion and would need changing |

Check the regulator datasheet for dropout voltage at your load. Near the end of discharge the ESP gets battery voltage minus dropout.

**After this step USB no longer powers the ESP.** USB 5V only reaches VIN and the CH340G. During development, connect the battery or a 3.3 V bench supply as well as USB.

## Step 2: Check how the CH340G is powered

On Lolin boards the CH340G supply differs between manufacturers. Find out which rail it is on before deciding what to do.

CH340G pins (SOP-16):

| Pin | Function |
|---|---|
| 1 | GND |
| 4 | V3 |
| 16 | VCC |

With the board unpowered, use the continuity tester of a multimeter between **pin 16** and the **3V3** pin, then between **pin 16** and the **VIN/VU** pin.

| Pin 16 connected to | What happens on battery, USB unplugged | Action |
|---|---|---|
| VIN / VUSB (5 V) | CH340G is unpowered and draws nothing from its supply | Nothing, but check [TX line leakage](#tx-line-leakage) |
| 3V3 | CH340G is powered from the battery all the time | Lift pin 16 or cut the trace, or remove the chip |

To confirm, measure sleep current before and after lifting pin 16. The difference is what the CH340G draws.

### TX line leakage

When the CH340G is unpowered, the ESP TX pin (driven HIGH while awake) can leak current through the CH340G input protection diodes into its supply. This only happens while the ESP is awake, and the amount depends on the board, from tens of µA to around a milliamp.

Removing the CH340G, or cutting the trace between the ESP TX and the CH340G RXD, eliminates it.

## Step 3: Flashing without the CH340G

If you removed the CH340G or cut its lines, flash through an external **3.3 V** USB-TTL adapter:

| Adapter | NodeMCU |
|---|---|
| TX | RX |
| RX | TX |
| GND | GND |

The board also needs power (battery or 3.3 V supply on 3V3). Do not power it from the adapter's 5 V pin.

To enter the bootloader:

1. Disconnect the D0-RST wire.
2. Hold **FLASH**, press and release **RST**, then release FLASH.
3. Run `pio run -t upload`.
4. Reconnect the D0-RST wire and press RST.

## Measuring current

Put a multimeter in series with battery +.

- While the ESP is awake, use the **mA range**. WiFi current peaks reach 170 mA or more.
- In deep sleep, switch to the **µA range** for accurate readings.

The µA range has a high internal resistance (burden voltage). If the ESP wakes up while the meter is on that range, the supply voltage drops and the ESP may reset or fail to connect. A practical approach:

1. Short the multimeter leads with a jumper wire.
2. Wait until the ESP goes to sleep (`sleeping for ... s` in the serial output, or the WiFi current drops).
3. Remove the jumper and read the sleep current on the µA range.
4. Put the jumper back before the next wake-up.

### Expected values

| Board state | Deep sleep current |
|---|---|
| Stock NodeMCU | ~10-20 mA |
| AMS1117 removed, CH340G on 3V3 | depends on the CH340G, from ~100 µA to several mA |
| AMS1117 removed, CH340G unpowered or removed | ~30-100 µA |

The last row includes the ESP (~20 µA), battery divider (~10 µA), LDO quiescent current (a few µA) and the DS18B20 (~1 µA). If you measure much more, something else is still powered. Look for LEDs or other parts on the 3.3 V rail.

## Charging and battery protection

The firmware stops waking up below 3.3 V, but in deep sleep it still draws current. Over months it can discharge the cell further. Use a **protected 18650 cell**, or a charger module with a protection circuit (for example TP4056 with DW01). The protection circuit itself draws a few µA.
