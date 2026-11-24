# Working on folloq with an AI agent

How an AI agent (Claude) writes code for this app, runs it on the UNO Q, reads logs and debugs. Status: plan. Nothing in this file has been tested on the board yet unless marked **verified**.

## 1. Board architecture (what the agent has to keep in mind)

| Side | Chip | Runs | Code here |
|---|---|---|---|
| MPU | Qualcomm QRB2210 | Debian Linux, Docker, `arduino-app-cli`, App Lab | `python/main.py`, bricks (`app.yaml`), `assets/` |
| MCU | STM32U585 (Cortex-M33) | Arduino core on Zephyr OS | `sketch/sketch.ino` |
| Link | Bridge (RPC, `Arduino_RouterBridge`) | `Bridge.provide()` on the MCU, `Bridge.call()` in Python | |

A bug can be in three places: the Python side, the sketch, or the Bridge between them. Check which side fails before changing code.

## 2. Access to the board

Three ways into the Linux shell:

| Method | Command | Notes |
|---|---|---|
| SSH over Wi-Fi | `ssh arduino@<board-name>.local` | User is always `arduino`. Preferred for the agent. Set up key-based login so no password prompt blocks it. |
| ADB over USB-C | `adb shell` | No network needed and no password. Fallback when Wi-Fi is down. Needs a data cable. |
| App Lab terminal | built in | For manual use only. |

Key setup (once, on the PC):

```sh
ssh-keygen -t ed25519 -f ~/.ssh/id_ed25519_unoq
ssh-copy-id -i ~/.ssh/id_ed25519_unoq.pub arduino@<board-name>.local   # or paste into ~/.ssh/authorized_keys on the board
# ~/.ssh/config
Host unoq
    HostName <board-name>.local
    User arduino
    IdentityFile ~/.ssh/id_ed25519_unoq
```

Board name / IP: _to fill in_.

## 3. Deploy and run loop

Single source of truth is the git checkout on the PC (`C:\workspace\folloq`). The board copy is a deploy target only.

Reason: App Lab refreshes the app from the board and overwrote local `main.py` and `sketch.ino` after adding a brick (see README, Learnings).

```sh
# 1. copy the app to the board (rsync may need: sudo apt-get install -y rsync on the board)
rsync -av --delete --exclude .git --exclude .cache ./ unoq:/home/arduino/ArduinoApps/folloq/
# 2. (re)start it
ssh unoq 'arduino-app-cli app stop /home/arduino/ArduinoApps/folloq; arduino-app-cli app start /home/arduino/ArduinoApps/folloq'
# 3. read logs (section 4)
# 4. fix, commit, repeat
```

`arduino-app-cli app start` compiles and flashes the sketch and starts the Python container. Check `arduino-app-cli --help` on the board for the exact subcommands of the installed version (the tool changes between releases).

## 4. Where the logs are

| Log | How to read it | Status |
|---|---|---|
| App start-up (sketch compile, flash, Python deploy) | output of `arduino-app-cli app start` | works |
| Python (`logger.info` in `main.py`) | `docker ps -a`, then `docker logs <container-id>` on the board; App Lab also shows it | to verify |
| MCU (`Monitor.print` in the sketch) | **only the App Lab Serial Monitor** | gap, see section 5 |
| System (Docker, `arduino-app-cli` service, SSH) | `journalctl -u <service>`, `journalctl -b` | standard Debian |

## 5. Next step idea: MCU logs without the App Lab Serial Monitor

Today the sketch's debug output (`[BOOT]`, `[SENS]`, `[FOLLOW] line lost`, `[WDG]`, `[TEST]`) is visible only in the App Lab Serial Monitor. An agent working over SSH can't read it, and neither can a script that records a test run. Community reports say `arduino-cli monitor` with the `arduino:zephyr` core answers "no monitor available".

Options, in order of preference:

1. **Forward MCU log lines over the Bridge.** Add a small `mcuLog()` helper in the sketch that both prints to `Monitor` and sends the line to Python (for example `Bridge.notify("mcu_log", line)`, or a queued buffer that Python drains with a `get_logs` call next to `get_state`). Python writes it with `logger.info("[MCU] ...")`, so it ends up in the container log and optionally in a file. Keep lines short (the long `get_state` reply stopped answering). Rate-limit the `[SENS]` line.
2. **Find how App Lab reads `Monitor`.** App Lab gets the stream from the board somehow (likely through the router / `arduino-app-cli`). If there is a socket or API for it, read it directly over SSH. To investigate on the board: `arduino-app-cli --help`, running services (`systemctl list-units | grep -i arduino`), open sockets (`ss -lx`, `ss -ltn`).
3. **Hardware UART to a USB-serial adapter** on D0/D1 (3.3 V adapter). Fully independent of Linux, useful when the MPU side is down. On the AlphaBot, D0/D1 are shared with the Bluetooth header and the UART switch to the Pi header, so turn the switch off and leave Bluetooth unfitted. Needs a check of which Zephyr serial object maps to D0/D1 on the UNO Q.

Also keep from the README: call `Monitor.begin()` before any `Monitor.print`, printing earlier hung the MCU.

## 6. Next step idea: data-driven PD tuning

Goal: tune `KP`, `KD`, speed and `FOUND_THRESHOLD` from recorded runs instead of guessing.

1. Make the tuning constants runtime parameters: a Bridge call such as `set_tuning(kp_milli, kd_milli, found_milli)` and a REST endpoint `/tuning?kp=...&kd=...`. No rebuild and reflash per change.
2. Record each run in Python to a CSV on the board: time, mode, position, error, strengths, `cmdL`, `cmdR`, battery. The current 50 ms `get_state` poll is enough for a first version.
3. The agent pulls the CSV over SSH, looks at oscillation, overshoot and line-lost events, and proposes the next parameter set.

## 7. Things to watch

- **Hardware calls inside Bridge handlers.** `stop_all`, `follow_start`, `test_start` and `manual_drive` call `motorsStop()` (`digitalWrite` / `analogWrite`) from the Bridge RPC thread. It passes the hardware tests so far. One community source warns that hardware APIs inside `Bridge.provide()` handlers can fail because they run in a background thread. If STOP is ever late or ignored, change the handlers to only set a flag and let `loop()` touch the pins.
- MCU can take 30-60 s to answer after a restart; "no data from MCU" in that window is normal.
- The ecosystem moves fast (App Lab 0.7 added Custom Bricks). Note the installed versions here when the board is first checked: `arduino-app-cli version`, App Lab version, Debian image.

## 8. What the agent needs from the user

- Board name or IP, and confirmation that `ssh unoq` works with a key (or that `adb` is installed).
- The `alphabot/` folder referenced by the docs (schematic, user manual, `AlphaBot_Config.h`), copied into this repo or a connected folder.
- For new modules: exact part, datasheet link, and which header / jumper it uses.

## 9. Knowledge sources

| Source | Use |
|---|---|
| docs.arduino.cc: UNO Q user manual, datasheet ABX00162, full pinout | Hardware facts |
| docs.arduino.cc/software/app-lab: App Lab docs, "Arduino App CLI" tutorial, Bridge API reference | App structure, CLI, Bridge |
| `github.com/arduino/arduino-app-cli` | CLI source and behaviour |
| Example apps installed on the board | Reference for `app.yaml`, `sketch.yaml`, Bridge use, bricks |
| forum.arduino.cc, UNO Q category | Real problems and workarounds |
| Community knowledge bases for LLM agents (e.g. `CWTI-Ltd/arduino_uno_q_knowledge_base_and_playground`, `mpous/ai-skills` `arduino-unoq-applab`) | Unofficial; check against official docs |
| This repo: README Learnings, `docs/` | Project-specific facts verified on the robot. Add new learnings there. |
