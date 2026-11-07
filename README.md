# 🛼 folloq

UNO Q on the Waveshare AlphaBot: line follower plus hardware test.
https://www.waveshare.com/wiki/AlphaBot

## Functionality

- **Line follower** runs by default at boot.
- **Motor test** starts on demand from the web GUI.
- **Web GUI** at `http://<board-ip>:7000`.

### Architecture

| Part | File | Role |
|---|---|---|
| MCU sketch | `sketch/sketch.ino` | TLC1543 read, line algorithm, motors, test, watchdog |
| Python | `python/main.py` | Polls MCU over Bridge, serves REST API |
| Web page | `assets/index.html` | Visualization and controls |

### Line follower

- Line is dark: low sensor value = on the line.
- Each sensor is scaled 0..1 from its own min/max.
  - Fallback limits until calibrated: 250 (line), 900 (floor).
- Position = strength-weighted average, -2 (IR1, left) to +2 (IR5, right).
- PD steering: `wheel = base * (1 ± (KP*err + KD*Δerr))`.
- Line lost: spin toward the last side for 2 s, then stop.
- Never seen a line since boot: stays still.
- Default speed 35%; set 10-80%, or up to 100% with Boost.

### Motor test

- 10 steps, 1.5 s each, 0.5 s pause between:
  - single wheels fwd/back
  - both fwd/back
  - spins
  - ramp
  - 100% step (Boost only)
- Operator watches the wheels (no encoders).
- Lift the robot first.
- Ends in idle.

### Safety

- Power cap 80% (PWM 204); 100% only via Boost.
- Motors off if Python stops polling for 2 s.
- STOP button sets idle.
- Motor pins low at boot.

### Web GUI

- Live view: five sensor circles, raw values, strength bars, line marker, position pointer.
- Also shows motor outputs, battery voltage and a log.
- Controls: speed, Boost, Start follower, Start motor test, Reset calibration, STOP.

### REST API

| Endpoint | Method | Purpose |
|---|---|---|
| `/state` | GET | Latest state (JSON) |
| `/settings?speed=40&boost=false` | GET | Set speed and boost |
| `/follow/start` | POST | Start follower |
| `/test/start` | POST | Start motor test |
| `/stop` | POST | Idle |
| `/cal/reset` | POST | Reset calibration |

### Bridge (MCU)

`get_state`, `follow_start(speed, boost)`, `test_start(boost)`, `stop_all`, `cal_reset`.

## Pins (sketch)

| Function | Pin |
|---|---|
| Left motor ENA / IN1 / IN2 | D5 / A0 / A1 |
| Right motor ENB / IN3 / IN4 | D6 / A2 / A3 |
| TLC1543 CS / CLK / ADDR / DOUT | D10 / D13 / D12 / D11 |
| Line sensors IR1-IR5 | TLC1543 ch 0-4 |
| Battery | TLC1543 ch 10, 2:1 divider |

## Learnings

- **Battery channel is 10**, not 5 as in `alphabot/AlphaBot_Config.h` (see `docs/AlphaBot_Pinout.md`).
- **`ADC_VREF` = 4.87 V**, measured on TLC1543 (pin 20 vs pin 10).
  - Pin 12 (battery, TA10) vs GND: 4.22 V, so VIN is about 8.44 V.
  - Safe margin: input is below VCC + 0.3 V.
  - Earlier assumed 4.6 V read the battery about 5% low.
- **Encoders unused.** D3 is not 5 V safe (see `docs/UNO_Q_on_AlphaBot_compatibility.md`).
- **Serial Monitor needs `Monitor`**, not `Serial`.
  - Call `Monitor.begin()` before any `Monitor.print`.
  - Printing earlier hung the MCU: `get_state` timed out and the page showed "no data".
- **Keep the `get_state` reply short.** Use numeric codes, not names. The long string version stopped answering.
- **WebUI brick does not serve `/socket.io/socket.io.js`.**
  - The page stayed on "connecting" with dead buttons.
  - Now uses REST polling (`fetch`) instead.
- **`expose_api` handlers take query parameters**, e.g. `/settings?speed=40`.
- **Adding a brick reverts the checkout.** App Lab refreshed it from the board and overwrote local `main.py` and `sketch.ino`. Re-check files after `app_bricks_add`.
- **After restart the MCU can take 30-60 s** to answer. "no data from MCU" in that window is normal. If it persists, check the Serial Monitor.
- **Sensors on a bright floor read about 960-990** (no line, or sensors lifted).

## Not yet verified on hardware

- Motor direction and left/right wiring.
- Sensor order (IR1 = left assumed).
- Follower behavior on a real line; gains `KP`, `KD` and speed need tuning.
- Motor test run and the web buttons in a browser.

## Next steps

- Run the motor test with wheels lifted.
- Check that the sensors react to a dark line.
- Tune `KP`, `KD` and speed.
- Check the battery readout against 8.44 V.
