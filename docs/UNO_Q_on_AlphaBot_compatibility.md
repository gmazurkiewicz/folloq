# Arduino UNO Q (ABX00162) on the AlphaBot platform: compatibility report

Date: 2026-11-06

## Sources used

| Source | Notes |
|---|---|
| `alphabot/AlphaBot_Schematic.pdf` | 3 pages: power and Arduino/RPi sockets, modules and jumpers, L298P motor driver |
| `ABX00162-full-pinout.pdf` | Arduino pinout, last update 17 Feb 2026 |
| `alphabot/AlphaBot-User-Manual.pdf` | Battery type, board layout, jumper tables |
| `alphabot/AlphaBot_Config.h`, `AlphaBot_SelfTest.ino` | Pin assignments and battery monitor constants |
| UNO Q datasheet ABX00162 (downloaded, modified 03/11/2026) | Not in the repo. Used to settle power and tolerance questions the pinout PDF doesn't answer. Text extracted with PyMuPDF (`uv add pymupdf`). |

Nothing was measured on hardware. Everything below comes from documents, and the schematic was read from a rendered image. Items worth checking with a multimeter are flagged in section 8.

## 1. Verdict

| Question | Answer |
|---|---|
| Does the UNO Q plug into the AlphaBot Arduino socket? | **Yes.** It uses the UNO R3 header layout, hole pattern and board outline. Every AlphaBot signal lands on a pin that exists on the UNO Q. |
| Are the logic levels compatible? | **Mostly.** Two inputs are not safe as wired: **D3** (right wheel encoder) and **A4/A5** (analog obstacle outputs). Details in section 3. |
| Can the 5 V rails be connected together? | **Yes.** The UNO Q `+5V` pin is a diode-OR input/output (up to 3 A), so it tolerates the AlphaBot LM2596 rail. |
| Can the whole robot run from 7.5 V on VIN? | **Not reliably as the board is built.** The UNO Q needs 7.0 V minimum at VIN. A 7.5 V source passes through S1 and D9 (SS54), so the VIN pin sees only about 7.0–7.2 V. That is at or below the limit once you add load and battery sag. See section 4.3. |
| Can it run from 12 V at the battery connector? | **The UNO Q is happy, but two AlphaBot parts are not.** VIN of about 11.6 V is well inside the UNO Q's 7–24 V range. However, the battery-sense divider then puts about 5.8 V on a TLC1543 input that tolerates about 4.9 V, and the motors will see roughly twice the voltage they get from the stock 7.4 V pack. Fix both before using 12 V. See section 4.5. |

## 2. How the AlphaBot power tree is built (schematic p.1)

```
Battery(+) -> S1 switch -> D9 SS54 -> [VIN net] --+--> LM2596 (fixed 5 V) -> L2 -> D11 SS54 -> [5V rail]
                                                  |        (FBACK taken before D11)
                                                  +--> L298P Vs (motor supply) + 4x4 SS54 clamp diodes
                                                  +--> Arduino header pin 25 (VIN)
                                                  +--> R4/R5 10k+10k divider -> TA10 -> TLC1543 A10 (channel 10, pin 12)
[5V rail] -> Arduino header pin 28 (+5V), L298P VSS, TLC1543 VCC/REF+, sensors, servo, ultrasonic, IR, Bluetooth
Arduino 3V3 (pin 29) -> NRF24L01 VCC only
IOREF (30), RESET (31) and A7 (32) are crossed out (not connected). AREF is not connected.
```

- The `VIN` net is **after** D9. The AlphaBot VIN is therefore the battery voltage minus one Schottky drop. The Arduino VIN pin gets the same voltage.
- The `5V` rail is **after** D11. FBACK is taken from the inductor node, so the loaded rail is about 5.0 V minus the Vf of D11. That is roughly 4.5–4.7 V (read from the schematic, unmeasured). D11 stops an Arduino or USB-powered 5 V from back-feeding the LM2596.
- The manual (p.13) specifies 2×18650 cells (7.4 V nominal, 6.0–8.4 V range) and says an external supply must not exceed 12 V.
- C3 and C4 are 16 V parts, which is the practical VIN ceiling.

## 3. Logic-level analysis

UNO Q GPIOs are 3.3 V. The pinout PDF says all MCU pins are 5 V tolerant except A0/A1. **The newer datasheet is stricter** and contradicts the pinout PDF in two places:

- `~D3` (PB0) uses a TT-type pad. It is **3.6 V maximum in any mode** and is not 5 V tolerant.
- A0–A5 are not 5 V tolerant when used as ADC or any analog function (VDD + 0.3 V maximum). As plain digital inputs, A2–A5 are fine.

Use the datasheet values below.

| UNO Q pin | MCU pad | AlphaBot function | Signal level at the pin | Status |
|---|---|---|---|---|
| D0 / D1 | PB7 / PB6 | Bluetooth header RX/TX, UART switch to Pi | 5 V in, 3.3 V out | OK. FT pads, and a 3.3 V output is a valid high for 5 V receivers. |
| D2 | PB3 | CNTL, left wheel encoder | 5 V in | OK |
| **D3** | **PB0 (TT)** | **CNTR, right wheel encoder** | **about 4.6–5 V in** | **Not OK.** The speed module's SN74LVC1G126 runs from the 5 V rail, so the input exceeds the 3.6 V limit. |
| D4 | PA12 | IR receiver (via 1 kΩ R3), NRF IRQ | 5 V in | OK |
| D5 / D6 | PA11 / PB1 | ENA / ENB (L298P) | 3.3 V out | OK. The L298P VIH is about 2.3 V. Both are PWM capable. |
| D7 / D8 | PB2 / PB4 | DL / DR (digital obstacle outputs), NRF CSN/CE | 5 V in | OK |
| D9 / D10 | PB8 / PB9 | Servo S1 / S2 | 3.3 V out | OK for SG90. Both are PWM capable. |
| D11 | PB15 | Ultrasonic Trig out, TLC1543 DOUT in, NRF MOSI | 3.3 V out, 5 V in | OK |
| D12 | PB14 | Ultrasonic Echo in, TLC1543 ADDR, NRF MISO | 5 V in | OK |
| D13 | PB13 | TLC1543 IOCLK, NRF SCK | 3.3 V out | OK. The TLC1543 VIH is about 2 V. |
| A0, A1 | PA4, PA5 | L298P IN1, IN2 | outputs only | OK as outputs. The pads are not 5 V tolerant, but nothing on the AlphaBot drives them from 5 V. |
| A2, A3 | PA6, PA7 | L298P IN3, IN4 | outputs only | OK |
| **A4 / A5** | **PC1 / PC0** | **AL / AR, analog outputs of the obstacle modules** | **0 to about 5 V analog** | **Not OK as analog inputs.** An ADC pad limit of about 3.6 V clips the reading and injects current through the clamp diodes. Use the digital DL/DR outputs on D7/D8 instead (the manual's own setup), or add a divider. |
| 3V3 pin | PWR_3P3V | NRF24L01 VCC | output | OK for a plain NRF24L01 (about 12–14 mA). Don't use the PA/LNA variant (over 100 mA). |
| IOREF, RESET, AREF, A7 | not connected on the AlphaBot | none | OK. The datasheet says IOREF is output-only and must not be back-fed. |

### Fix for D3

Put a divider on the CNTR line, for example 4.7 kΩ in series and 10 kΩ to GND. That gives about 3.1–3.4 V at 4.6–5 V in. Alternatively, rewire CNTR with a jumper wire to a free 5 V-tolerant digital pin (for example D7 or D8, if the digital obstacle sensors are not used). The encoder pulses are slow (20 per revolution), so a divider is fine.

## 4. Power analysis

### 4.1 UNO Q input options (datasheet section 3)

| Input | Range | Notes |
|---|---|---|
| USB-C VBUS | 4.5–5.5 V, 5 V/3 A | Negotiates a 5 V/3 A contract only |
| VIN (DC_IN) | **7.0–24 V** | Feeds an on-board buck to 5 V. Reverse polarity is not protected in use. |
| `+5V` pin (5V_SYS) | 5 V, up to 3 A | Input or output. Diode-OR'd with the buck output and USB VBUS (Schottky). |

### 4.2 The 5 V rails: no conflict

The AlphaBot 5 V rail goes to the UNO Q `+5V` pin. Three sources then share one bus through diodes: the AlphaBot LM2596 (via D11), the UNO Q VIN buck and USB. Whichever is higher carries the load, and nothing back-feeds another source. This is the arrangement the datasheet describes as intended.

Consequences:

- The 5 V rail is a diode drop below nominal in every case. The UNO Q buck output goes through a Schottky (0.35–0.39 V at 1–2 A per the datasheet), so the bus will sit around 4.6–4.7 V. On an UNO R3 the rail was closer to 5.0 V.
- The TLC1543 minimum VCC is about 4.5 V, so it has very little margin.
- The SG90 servo (4.8–6 V nominal) will be slightly under-powered at full load.
- **`TLC1543_REFERENCE_VOLTAGE 5.0f` (Config.h) and the `5.0f` in `BatteryMonitor` (SelfTest.ino) will be wrong.** REF+ is tied to VCC, so the battery reading will come out about 8% high. Measure the rail and use that value.
- Even when VIN sags below 7 V and the UNO Q buck drops out, the LM2596 path (via D11 into `+5V`) can keep the UNO Q alive. This is a useful safety net. It is not a good primary design, because the rail is already near 4.5 V.

### 4.3 Powering from 7.5 V

Two ways to apply 7.5 V:

| Scenario | UNO Q VIN pin | Verdict |
|---|---|---|
| **A. 7.5 V at the AlphaBot battery connector or POW header** (through S1 and D9) | 7.5 V − Vf(SS54) − switch/wire drop ≈ **7.0–7.2 V** at light load, lower with load | **Marginal.** About 0–0.2 V above the 7.0 V limit. Datasheet: "brief dips can cause resets." Motor starts, battery sag and cable resistance will push it below 7 V. Not recommended. |
| **B. 7.5 V injected directly on header pin 25 / the VIN net** (bypassing S1 and D9) | ≈ 7.5 V | Voltage is in spec with only 0.5 V margin. **Not recommended**: it bypasses the power switch and reverse-polarity diode, and motor current (up to 2 A per channel from the L298P) would flow through a single header pin and the UNO Q's VIN trace. |

With the stock 2×18650 pack (8.4 V full, 7.4 V nominal) the VIN pin sees about 8.0 V when full, about 7.0 V at nominal, and drops below 7 V at roughly 25–30% remaining capacity. The UNO Q buck would then rely on the LM2596 path described in 4.2.

### 4.4 Recommended supply

Set the supply or battery voltage at the AlphaBot battery connector to **8.0–9.0 V** (after D9 the VIN pin sees 7.6–8.6 V).

- **Lower limit:** UNO Q VIN of at least 7.0 V, plus about 0.5 V for load sag.
- **Upper limit:** TA10 divider. At 2:1 the TLC1543 A10 (channel 10) input must stay below about 4.5 V (TLC1543 VCC is about 4.6 V, absolute maximum VCC + 0.3 V). That caps VIN at about 9 V, so about 9.4 V at the connector. Above this the battery monitor input is over-driven.
- Options if you only have 2S Li-ion: add a small boost converter to about 8.5 V, or fit a third cell and a regulator, or accept D9 being bridged with a wire (about 0.4 V gain, but it loses reverse-polarity protection).
- Alternative: power the UNO Q from a 5 V/3 A USB-C power bank and leave the battery for motors. The diode-OR makes this safe, and the UNO Q VIN pin is still connected to the AlphaBot VIN net, so it will be energized (and just idle if under 7 V).

### 4.5 Powering from 12 V

Assumes 12 V is applied at the AlphaBot battery connector (through S1 and D9). After D9 the VIN net is about **11.6 V** (about 12.2 V if the source is a fully charged 3S Li-ion at 12.6 V).

| Part | Effect of 12 V | Status |
|---|---|---|
| UNO Q VIN | 11.6 V is inside 7–24 V with 4.6 V of headroom. This also removes the sag problem from section 4.3. | OK |
| LM2596 (fixed 5 V) | Input limit is 40 V. Efficiency drops a little, so it runs warmer. | OK |
| L298P Vs | Rated far above 12 V. | OK |
| C3 / C4 (100 µF / 16 V) | About 72–76% of rating at 11.6–12.2 V, and motor switching spikes sit on top. The manual says an external supply must not exceed 12 V. A 12 V adapter that is unregulated can read 14–16 V with no load. | **Marginal.** Use a regulated source of 12.6 V or less, or replace C3 with a 25 V part. |
| Battery divider R4/R5 → TA10 → TLC1543 A10 | 11.6 V / 2 = **5.8 V**. The TLC1543 absolute maximum input is VCC + 0.3 V, about 4.9 V on a 4.6 V rail. | **Not OK.** Over-voltage on the ADC, and injection current can disturb the neighbouring line-sensor channels. |
| Motors | The L298P drops roughly 2 V, so full PWM gives about 9–9.5 V at the motor, against about 4.8 V with the stock 7.4 V pack. | **Not OK without a speed cap.** The motor rating is not in the supplied files. Check the label. |
| S1, D9 | Lower current for the same power. | OK |

#### Fixes

1. **Battery divider.** Replace R4 (the 10 kΩ on the `VIN` side, schematic p.2) with 22 kΩ. The ratio becomes (22 + 10) / 10 = 3.2, so TA10 is about 3.6 V at 11.6 V and about 4.2 V at a 14 V VIN. The designators R4/R5 are also used for the UART resistors on p.1, so confirm the right pair on the board. Then set `BATTERY_VOLTAGE_DIVIDER_RATIO` in `AlphaBot_Config.h` and the `2.0f` in `BatteryMonitor(...)` in the sketch to 3.2. If you don't need the battery reading, remove R4 instead.
2. **Motor voltage.** Cap the PWM so the average motor voltage stays near the motor rating. Rough guide: motor voltage ≈ duty × (VIN − 2 V). For a motor rated about 6 V at 11.6 V in, that is a duty of roughly 60%, so set `MAX_MOTOR_SPEED` to about 150 (of 255) until you know the real rating. Alternatively, drop the voltage with a buck converter to about 8 V before the AlphaBot battery connector.
3. **Source.** Use a regulated 12 V supply or pack of 12.6 V or less. Don't use an unregulated 12 V wall adapter.
4. **Battery thresholds.** The config values (6.0–8.4 V) assume 2S. For a 3S Li-ion pack use about 9.0 V (empty) to 12.6 V (full), with a low warning near 10 V.

D3 and A4/A5 findings (section 3), the 5 V rail behaviour (section 4.2) and the TLC1543 reference voltage point are unchanged by the supply voltage.

### 4.6 Current budget

| Item | Estimate |
|---|---|
| UNO Q | Datasheet gives no consumption figure. Size for a 5 V/3 A supply, so up to roughly 15 W, or ≈2.5 A at 7 V after buck losses, as a worst case. Typical use will be far lower. |
| L298P motors | Up to 2 A per channel |
| 5 V rail (LM2596 is rated 3 A) | Servo and ultrasonic add a few hundred mA at most. Shared with the UNO Q if it draws from `+5V`. |

- **S1** is a small 6-pin slide switch, which usually carries 0.3–1 A. Check its rating, because it carries the whole VIN net.
- **D9 / SS54** is rated 5 A, which is fine.
- **18650 holders and cells:** make sure the cells can deliver the combined peak.

## 5. Conflicts already present on the AlphaBot (independent of the UNO Q)

These are jumper-selected sharing in the AlphaBot design, not problems with the UNO Q:

| Pin | Shared between |
|---|---|
| D10 | Servo S2 and TLC1543 CS (default `ADC_CS_PIN`) |
| D11, D12 | Ultrasonic Trig/Echo, TLC1543 DOUT/ADDR, NRF24L01 MOSI/MISO |
| D13 | TLC1543 IOCLK, NRF24L01 SCK |
| D7, D8 | DL/DR digital obstacle outputs, NRF24L01 CSN/CE |
| D4 | IR receiver, NRF24L01 IRQ |

The manual says to remove the jumpers of unused modules. TLC1543 DOUT and ADDR are on D11 and D12, the reverse of the hardware SPI MOSI/MISO pins, so the TLC1543 needs bit-banged SPI.

## 6. Software and porting notes

- The UNO Q MCU (STM32U585) runs Zephyr under the Arduino core, not AVR. Anything using AVR registers or timers will need porting (check the missing `MotorControl.h`, `TLC1543_ADC.h`, `WheelEncoder.h`, `BatteryMonitor.h`, and any IR remote library, none of which are in the repo).
- `AlphaBot_SelfTest.ino` defines `BATTERY_ADC_PIN A6`. That pin doesn't exist on the AlphaBot header or on the UNO Q. `AlphaBot_Config.h` correctly says to read the battery through the TLC1543, but it gives the wrong channel: it sets `ADC_BATTERY_CHANNEL 5` and `TOTAL_ADC_CHANNELS 6`. The schematic puts net `TA10` on TLC1543 **A10** (pin 12), so the battery is channel **10**. Channel 5 (pin 6) is the unused net `TA5`. Fix the sketch and the config accordingly.
- `AlphaBot_Config.h` defines `LED_PIN 13` and `ADC_CLOCK_PIN 13`. These collide, and the UNO Q has no D13 LED (its LEDs are on PH10–PH15).
- The battery comments say 4× NiMH (6.0–8.4 V), but the manual says 2×18650 (7.4 V nominal). Update the thresholds to the real cell chemistry. For 2S Li-ion, 6.0 V empty is too low and would over-discharge the cells.
- On interrupts: STM32 EXTI lines are shared by pin number. D3 (PB0) and A5 (PC0) both use line 0, so don't use both as interrupts.
- Motor safety at boot: STM32 pins float before the sketch starts, so add 10 kΩ pull-downs on ENA and ENB (D5 and D6, or at the L298P side) so the motors stay off during reset and flashing. Linux boot takes 20–30 s on the UNO Q, though the MCU is independent of that.
- Wi-Fi/BT (2.4 GHz) on the UNO Q and an NRF24L01 will interfere with each other if used together.

## 7. Mechanical (not verifiable from the supplied files)

- The UNO Q matches the UNO outline and hole pattern, 68.58 × 53.34 mm. The two 60-pin JMISC and JMEDIA connectors are on the bottom side and kept under 2 mm.
- The manual shows the control board mounted under the AlphaBot mainboard. Dry-fit to check that the UNO Q's top-side parts (USB-C connector, heat spreader over the QRB2210, LED matrix, JCTL header, Qwiic) don't touch the AlphaBot underside, the Raspberry Pi socket or the chassis standoffs, and that the bottom connectors clear the chassis.
- The UNO Q has a Wi-Fi antenna along one edge. Keep it clear of metal where possible.

## 8. Checklist before powering up

1. Remove or leave unconnected the jumpers for modules you don't use (section 5).
2. Wire CNTR through a divider, or move it off D3 (section 3).
3. Use DL/DR (D7/D8) for obstacle detection, not AL/AR on A4/A5.
4. Add 10 kΩ pull-downs on ENA/ENB.
5. If running from 12 V: change R4 to 22 kΩ (or remove it), update the divider ratio in software, and cap `MAX_MOTOR_SPEED` (section 4.5). If running from 8–9 V, none of these changes are needed.
6. Power up the AlphaBot with the UNO Q removed. Measure at the UNO Q VIN pin (header pin 25): expect the supply voltage minus about 0.4 V (about 7.6–8.6 V for 8–9 V, about 11.6 V for 12 V). Measure the TA10 node (TLC1543 pin 12): it must stay below 4.5 V. Measure the 5 V rail at the `+5V` pin: expect about 4.5–5.0 V. Record it for the sketch's ADC reference.
7. Insert the UNO Q, switch on, and re-measure the VIN pin and 5 V rail at idle and with motors running. VIN should stay above 7.0 V at all times.
8. Check S1, D9 and the LM2596 temperature after a few minutes of motor load.
