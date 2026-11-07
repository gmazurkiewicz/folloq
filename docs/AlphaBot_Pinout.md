# AlphaBot pinout

Source: `alphabot/AlphaBot_Schematic.pdf` (3 pages), cross-checked against `alphabot/AlphaBot-User-Manual.pdf` and `alphabot/AlphaBot_Config.h`.

Notation: `Dn`/`An` are Arduino UNO header pins. `Pn` are Raspberry Pi **BCM** GPIO numbers (as labelled on the schematic). Each module signal has a 3x2 or 3-pin jumper that selects the Arduino pin or the Pi pin.

## 1. Power nets

| Net | Source | Feeds |
|---|---|---|
| `VIN` | Battery + → S1 switch → D9 (SS54) | LM2596 input, L298P motor supply (Vs) and clamp diodes, Arduino header pin 25, R4/R5 battery divider |
| `5V` | LM2596 → 33 µH L2 → D11 (SS54) | Arduino header pin 28, Pi header pins 2 and 4, L298P VSS, TLC1543 VCC/REF+, all sensor/servo/ultrasonic/IR/Bluetooth headers |
| `3V3` | Arduino header pin 29 (or Pi header pins 1 and 17) | NRF24L01 VCC only |
| `GND` | common | everything |

Notes: the LM2596 feedback is taken before D11, so the loaded 5 V rail is about 5.0 V minus one Schottky drop. Reserved external power header `POW` (unsoldered) is in parallel with the battery connector, before S1 and D9. LED1 with R1 (1 kΩ) is the 5 V power indicator.

## 2. Arduino header (schematic p.1, pin numbers 1–32)

| Hdr pin | Name | Connected to | Alternate Pi pin (BCM) |
|---|---|---|---|
| 1 | D0 / RX | Bluetooth header pin 3; UART switch (via 1 kΩ) | P_RX / P_TX (UART switch) |
| 2 | D1 / TX | Bluetooth header pin 4; UART switch (via 1 kΩ) | P_TX / P_RX (UART switch) |
| 3 | D2 | Speed JMP: CNTL (left encoder) | P_CE1 (7) |
| 4 | D3 (PWM) | Speed JMP: CNTR (right encoder) | P_CE0 (8) |
| 5 | D4 | IRM JMP: IR receiver output (also NRF24L01 IRQ) | P18 |
| 6 | D5 (PWM) | Motor JMP2: ENA | P6 |
| 7 | D6 (PWM) | Motor JMP2: ENB | P26 |
| 8 | D7 | Obstacle 3x2 JMP: DL (also NRF24L01 CSN) | P19 |
| 9 | D8 | Obstacle 3x2 JMP: DR (also NRF24L01 CE) | P16 |
| 10 | D9 (PWM) | Servo JMP: S1 | P27 |
| 11 | D10 (PWM) | Servo JMP: S2; Tracing JMP4: CS | P22 / P5 |
| 12 | D11 (PWM/MOSI) | Ultrasonic JMP: Trig; Tracing JMP3: DOUT; NRF24L01 MOSI | P17 / P23 |
| 13 | D12 (MISO) | Ultrasonic JMP: Echo; Tracing JMP2: ADDR; NRF24L01 MISO | P5 / P24 |
| 14 | D13 (SCK) | Tracing JMP1: IOCLK; NRF24L01 SCK | P25 |
| 15 | GND | | |
| 16 | AREF | not connected (crossed out) | |
| 17 | SDA | net label only, no module | |
| 18 | SCL | net label only, no module | |
| 19 | A5 | Obstacle JMP: AR (analog right) | TA9 (TLC1543) |
| 20 | A4 | Obstacle JMP: AL (analog left) | TA8 (TLC1543) |
| 21 | A3 | Motor JMP3: IN4 | P21 |
| 22 | A2 | Motor JMP3: IN3 | P20 |
| 23 | A1 | Motor JMP1: IN2 | P13 |
| 24 | A0 | Motor JMP1: IN1 | P12 |
| 25 | VIN | `VIN` net (after D9) | |
| 26, 27 | GND | | |
| 28 | +5V | `5V` rail (after D11) | |
| 29 | 3V3 | NRF24L01 VCC | |
| 30 | RESET | not connected (crossed out) | |
| 31 | IOREF | not connected (crossed out) | |
| 32 | A7 | not connected (crossed out) | |

Pins that share a jumper position (use only one module per pin): D4, D7, D8, D10, D11, D12, D13.

## 3. Jumper blocks (3x2, odd pins = signal rows)

Each jumper bank has 6 pins in 3 rows: pin pair `1-2` is the Arduino side, `3-4` is the module side, `5-6` is the Pi side. Bridge `3-1` and `4-2` for Arduino, or `3-5` and `4-6` for Pi.

| Jumper | Pins 1 / 2 (Arduino) | Pins 3 / 4 (module) | Pins 5 / 6 (Pi) |
|---|---|---|---|
| Motor JMP1 | A0 / A1 | IN1 / IN2 | P12 / P13 |
| Motor JMP2 | D5 / D6 | ENA / ENB | P6 / P26 |
| Motor JMP3 | A2 / A3 | IN3 / IN4 | P20 / P21 |
| Speed JMP | D2 / D3 | CNTL / CNTR | P_CE1 / P_CE0 |
| Ultrasonic JMP | D11 / D12 | Trig / Echo | P17 / P5 |
| Obstacle JMP | A4 / A5 | AL / AR | TA8 / TA9 (TLC1543) |
| Obstacle "Header3X2" | D7 / D8 | DL / DR | P19 / P16 |
| Servo JMP | D9 / D10 | S1 / S2 | P27 / P22 |
| Tracing JMP1 (3-pin) | D13 | IOCLK | P25 |
| Tracing JMP2 (3-pin) | D12 | ADDR | P24 |
| Tracing JMP3 (3-pin) | D11 | DOUT | P23 |
| Tracing JMP4 (3-pin) | D10 | CS | P5 |
| IRM JMP (3-pin) | D4 | IRM | P18 |

## 4. Module headers

| Header | Pins |
|---|---|
| H1 (left speed) | 1 = 5V, 2 = GND, 3 = CNTL |
| H2 (right speed) | 1 = 5V, 2 = GND, 3 = CNTR |
| H3 (ultrasonic) | 1 = 5V, 2 = Trig, 3 = Echo, 4 = GND |
| H4 (left obstacle) | 1 = 5V, 2 = GND, 3 = AL, 4 = DL |
| H5 (right obstacle) | 1 = 5V, 2 = GND, 3 = AR, 4 = DR |
| H6 (servo, 3x2) | 1 = GND, 2 = GND, 3 = 5V, 4 = 5V, 5 = S1, 6 = S2 |
| P1 (tracking sensor, 7-pin) | 1 = 5V, 2 = GND, 3 = IR1, 4 = IR2, 5 = IR3, 6 = IR4, 7 = IR5 |
| Bluetooth (UART) | 1 = 5V, 2 = GND, 3 = D0/RX, 4 = D1/TX |
| NRF24L01 (SPI) | 1 = GND, 2 = VCC (3V3), 3 = CE (D8), 4 = CSN (D7), 5 = SCK (D13), 6 = MOSI (D11), 7 = MISO (D12), 8 = IRQ (D4) |
| IRM1 (IR receiver) | 1 = VCC (5V via R2 100 Ω, C6 100 nF to GND), 2 = GND, 3 = OUT (via R3 1 kΩ to `IRM`) |
| MotoA / MotoB | MotoA 2 = OUT1, 1 = OUT2; MotoB 2 = OUT3, 1 = OUT4 |

## 5. L298P motor driver (U: L1)

| Pin | Name | Net |
|---|---|---|
| 1, 10, 11, 20 | GND | GND |
| 2 | SENSEA | GND |
| 3, 18 | NC | not connected |
| 4 / 5 | OUT1 / OUT2 | MotoA |
| 6 | Vs | `VIN` |
| 7 | IN1 | A0 (Pi P12) |
| 8 | ENA | D5 (Pi P6) |
| 9 | IN2 | A1 (Pi P13) |
| 12 | VSS | `5V` (C1 10 µF, C2 100 nF to GND) |
| 13 | IN3 | A2 (Pi P20) |
| 14 | ENB | D6 (Pi P26) |
| 15 | IN4 | A3 (Pi P21) |
| 16 / 17 | OUT3 / OUT4 | MotoB |
| 19 | SENSEB | GND |

Eight SS54 diodes (D1–D8) clamp OUT1–OUT4 to `VIN` and GND. The manual (p.12) says IN1/IN2 drive the left motor and IN3/IN4 the right, with forward = IN4 on the right. **The robot does not behave that way** (see below).

**Verified on the robot** (UNO Q sketch, `sketch/sketch.ino`):

| Wheel | Channel | PWM | Forward | Backward |
|---|---|---|---|---|
| Right | A (MotoA, OUT1/OUT2) | ENA = D5 | IN2 = A1 high, IN1 low | IN1 = A0 high, IN2 low |
| Left | B (MotoB, OUT3/OUT4) | ENB = D6 | IN3 = A2 high, IN4 low | IN4 = A3 high, IN3 low |

So channel A is the right wheel and channel B the left wheel, the opposite of the manual. Motor wiring polarity on this unit also makes IN2 / IN3 the forward inputs.

## 6. TLC1543 10-bit ADC (U: T1)

| Pin | Name | Net |
|---|---|---|
| 1 | A0 | IR1 |
| 2 | A1 | IR2 |
| 3 | A2 | IR3 |
| 4 | A3 | IR4 |
| 5 | A4 | IR5 |
| 6 | A5 | TA5 (unused) |
| 7 | A6 | TA6 (unused) |
| 8 | A7 | TA7 (unused) |
| 9 | A8 | TA8 (alternate AL) |
| 10 | GND | GND |
| 11 | A9 | TA9 (alternate AR) |
| 12 | A10 | **TA10: battery voltage** |
| 13 | REF− | GND |
| 14 | REF+ | `5V` |
| 15 | CS | Tracing JMP4 (D10 / P5) |
| 16 | DOUT | Tracing JMP3 (D11 / P23) |
| 17 | ADDR | Tracing JMP2 (D12 / P24) |
| 18 | I/O CLK | Tracing JMP1 (D13 / P25) |
| 19 | EOC | not connected on the schematic |
| 20 | VCC | `5V` (C9 10 µF, C10 100 nF to GND) |

Battery measurement: `VIN` → R4 10 kΩ → `TA10` → R5 10 kΩ → GND, so `TA10 = VIN / 2`. The input must stay below about VCC + 0.3 V (about 4.9 V), which limits VIN to roughly 9 V with the stock resistors. Above that (for example a 12 V supply) increase R4, for example to 22 kΩ. The battery is on **channel 10** (pin 12). `AlphaBot_Config.h` says `ADC_BATTERY_CHANNEL 5`, which doesn't match the schematic.

## 7. UART switch

`D1/TX` and `D0/RX` connect through 1 kΩ resistors (R4, R5 on schematic p.1; the same designators are reused for the battery divider on p.2) to a 6-pin switch that links them to the Pi header `P_RX` / `P_TX`. Bluetooth shares D0/D1 on its own header, so turn the switch off when a Bluetooth module is fitted.

## 8. Raspberry Pi 40-pin header

| Pin | Signal | | Pin | Signal |
|---|---|---|---|---|
| 1 | 3.3 V | | 2 | 5 V |
| 3 | P_SDA (BCM2) | | 4 | 5 V |
| 5 | P_SCL (BCM3) | | 6 | GND |
| 7 | P4 | | 8 | P_TX (BCM14) |
| 9 | GND | | 10 | P_RX (BCM15) |
| 11 | P17 | | 12 | P18 |
| 13 | P27 | | 14 | GND |
| 15 | P22 | | 16 | P23 |
| 17 | 3.3 V | | 18 | P24 |
| 19 | P_MOSI (BCM10) | | 20 | GND |
| 21 | P_MISO (BCM9) | | 22 | P25 |
| 23 | P_SCK (BCM11) | | 24 | P_CE0 (BCM8) |
| 25 | GND | | 26 | P_CE1 (BCM7) |
| 27 | ID_SD | | 28 | ID_SC |
| 29 | P5 | | 30 | GND |
| 31 | P6 | | 32 | P12 |
| 33 | P13 | | 34 | GND |
| 35 | P19 | | 36 | P16 |
| 37 | P26 | | 38 | P20 |
| 39 | GND | | 40 | P21 |

## 9. Pin map used by `AlphaBot_Config.h`

| Function | Arduino pin | Matches schematic |
|---|---|---|
| Left motor PWM (ENA) | 5 | pin yes, but on the robot ENA drives the **right** wheel |
| Left motor IN1 / IN2 | A0 / A1 | pins yes, but they drive the **right** wheel; forward = IN2 |
| Right motor PWM (ENB) | 6 | pin yes, but on the robot ENB drives the **left** wheel |
| Right motor forward / backward | A3 / A2 | the config's forward/backward are wrong for this robot: the wheel (left) goes forward on IN3 = A2, backward on IN4 = A3. The config comments also call them IN3 / IN4, and the schematic has IN3 = A2, IN4 = A3 |
| Left / right encoder | 2 / 3 | yes (CNTL / CNTR) |
| TLC1543 CS / CLK / ADDR / DOUT | 10 / 13 / 12 / 11 | yes |
| Battery ADC channel | 5 | **no: schematic says channel 10 (A10)** |
| `LED_PIN` | 13 | same pin as `ADC_CLOCK_PIN` |
