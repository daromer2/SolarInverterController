# SolarInverterController

ESP32 WiFi controller for **MPP Solar / Voltronic inverters** — the full rewrite
and successor of [InverterOfThings](old_inverterofthings/) (ESP8266).

Out of the box it supports:

| Device | Protocol | Notes |
|---|---|---|
| **MPI Hybrid 10KW** (3-phase) | PI17 (`^P003GS`…) | incl. feed-to-grid control (GPMP) |
| **PIP / Axpert series** | PI30 (`QPIGS`…) | full settings control |
| **PCM60X** MPPT charger | PI30 subset | monitoring + charge settings |

New inverters are added as **pure data files** (command tables) — see
`firmware/src/drivers/profiles/`.

## Features

- **Live SVG dashboard** per device type: animated power flow (PV → inverter →
  battery/load/grid) over WebSocket, warnings, mode badge
- **Full control** from web + MQTT: output/charger priority, battery voltages
  and currents, battery type, flags, feed-to-grid power, … all validated
  against min/max before hitting the wire, every command ACK/NAK-confirmed
- **MQTT**: structured topic schema + JSON state, command topics with results,
  availability (LWT), **Home Assistant auto-discovery** (sensors, switches,
  selects, numbers), and a **legacy compatibility toggle** publishing the exact
  old InverterOfThings topics (incl. the `/code` raw passthrough) so existing
  dashboards keep working
- **SDM630 integration**: a metering ESP32 can steer feed-in via
  `<base>/<name>/cmd/feed_power` (watts). A configurable **failsafe** reverts
  feed-in to a safe value if the controller goes silent
- **WiFi manager**: captive-portal AP (`SolarSetup-xxxx`) when unconfigured,
  automatic AP fallback (without losing settings) when the network is down
- **OTA**: browser upload (firmware + web assets) and ArduinoOTA, dual app
  partitions
- Settings in NVS with JSON export/import; raw command console; status LEDs
  (red = inverter link fault, green = MQTT) with configurable pins

## Hardware

- **ESP32 DevKit** (classic WROOM-32, 4MB flash)
- MAX3232 level shifter to the inverter's RS232 port
- Inverter TX/RX ↔ MAX3232 ↔ **GPIO16 (RX2) / GPIO17 (TX2)** — configurable
- LEDs on **GPIO25 (red) / GPIO26 (green)** — configurable
  (the legacy GPIO5 is a strapping pin on ESP32, avoid it)
- USB stays free for flashing/debug — no more "disconnect RX to flash"

## Build & flash

```bash
cd firmware
pio run -e esp32 -t upload      # firmware
pio run -e esp32 -t uploadfs    # web UI (LittleFS)
pio test -e native              # host-side protocol tests
```

First boot: join the `SolarSetup-xxxx` WiFi, the setup page opens (or browse
http://192.168.4.1), configure WiFi/type/MQTT, reboot. Afterwards the device
is at `http://ESP-<name>.local`.

## MQTT topics (base = `solar/<deviceName>`)

Published: `availability` (LWT), `state` (full JSON), `sensor/<key>` (per
datapoint), `mode`, `warnings`, `device`, `cmd/result`, `event`.
Subscribed: `cmd/<setKey>` (value), `cmd/raw`, `cmd/feed_power` (watts,
MPI fast path), `heartbeat`, legacy `code` when compat mode is on.

Example — set output priority to SBU:
```
mosquitto_pub -t solar/inverter1/cmd/output_priority -m "SBU (Solar-Battery-Utility)"
mosquitto_sub -t solar/inverter1/cmd/result
```

## Bench testing without an inverter

```bash
pip install pyserial
python3 firmware/tools/inverter_sim.py /dev/ttyUSB0 --type mpi
```
with a USB-serial adapter crossed into GPIO16/17.

## Hardware verification notes (test on first real run)

- **MPI PS power scaling**: this firmware uses the legacy `/10` scaling that
  was validated on the original MPI 10KW; jblance/mpp-solar treats the same
  fields as plain watts. If dashboard powers look 10× off, adjust the scales
  in `ProfileMPI10K.cpp` (MPI_PS_F).
- **MPI setter format**: PI17 setters are framed `^Snnn<CMD>` without TX CRC
  (matches legacy + jblance). Verify one harmless setter (e.g. `mute_buzzer`)
  before relying on GPMP feed control.
- **PCM60X QPIRI/QPIWS field layouts** are best-effort from the manual; the
  QPIGS layout is the legacy-proven one.

## Repository layout

```
firmware/              PlatformIO project (see src/ module layout)
old_inverterofthings/  legacy ESP8266 version (reference only)
reference/mpp-solar/   jblance protocol reference (git clone, not vendored)
```
