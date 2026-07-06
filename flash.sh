#!/usr/bin/env bash
# Flash helper for SolarInverterController (ESP32).
#
# Usage:
#   ./flash.sh                     build + flash firmware over USB
#   ./flash.sh all                 build + flash firmware AND web UI (USB)
#   ./flash.sh fs                  build + flash only the web UI (LittleFS)
#   ./flash.sh monitor             open the serial monitor (115200)
#   ./flash.sh erase               wipe ALL flash (config + fs), then reflash all
#   ./flash.sh test                run the host-side protocol tests
#
#   ./flash.sh ota <host> [fw|fs|all]   flash over the network, e.g.:
#       ./flash.sh ota ESP-solar.local
#       ./flash.sh ota 192.168.1.42 all
#
# Options via env:
#   PORT=/dev/ttyUSB1 ./flash.sh   force a specific USB port

set -euo pipefail
SELF="$(readlink -f "$0")"
cd "$(dirname "$SELF")/firmware"

PIO="$HOME/.platformio-venv/bin/pio"
if ! [ -x "$PIO" ]; then
    PIO="$(command -v pio || true)"
    [ -n "$PIO" ] || { echo "PlatformIO not found. Install: python3 -m venv ~/.platformio-venv && ~/.platformio-venv/bin/pip install platformio"; exit 1; }
fi

PORT_ARG=()
[ -n "${PORT:-}" ] && PORT_ARG=(--upload-port "$PORT")

usb_hint() {
    if ! ls /dev/ttyUSB* /dev/ttyACM* >/dev/null 2>&1; then
        echo "!! No /dev/ttyUSB*/ttyACM* device found — is the board plugged in?"
    elif [ ! -w "$(ls /dev/ttyUSB* /dev/ttyACM* 2>/dev/null | head -1)" ]; then
        echo "!! Serial port not writable. On Arch: sudo usermod -aG uucp $USER (then re-login)"
    fi
}

case "${1:-fw}" in
    fw)
        usb_hint
        "$PIO" run -e esp32 -t upload "${PORT_ARG[@]}"
        ;;
    fs)
        usb_hint
        "$PIO" run -e esp32 -t uploadfs "${PORT_ARG[@]}"
        ;;
    all)
        usb_hint
        "$PIO" run -e esp32 -t upload "${PORT_ARG[@]}"
        "$PIO" run -e esp32 -t uploadfs "${PORT_ARG[@]}"
        ;;
    ota)
        HOST="${2:?usage: ./flash.sh ota <host|ip> [fw|fs|all]}"
        WHAT="${3:-fw}"
        case "$WHAT" in
            fw)  "$PIO" run -e esp32 -t upload   --upload-port "$HOST" ;;
            fs)  "$PIO" run -e esp32 -t uploadfs --upload-port "$HOST" ;;
            all) "$PIO" run -e esp32 -t upload   --upload-port "$HOST"
                 "$PIO" run -e esp32 -t uploadfs --upload-port "$HOST" ;;
            *)   echo "unknown target '$WHAT' (fw|fs|all)"; exit 1 ;;
        esac
        ;;
    erase)
        usb_hint
        echo ">> erasing entire flash (config + filesystem)…"
        "$PIO" run -e esp32 -t erase "${PORT_ARG[@]}"
        echo ">> reflashing firmware + web UI…"
        "$PIO" run -e esp32 -t upload "${PORT_ARG[@]}"
        "$PIO" run -e esp32 -t uploadfs "${PORT_ARG[@]}"
        ;;
    monitor)
        "$PIO" device monitor -b 115200
        ;;
    test)
        "$PIO" test -e native
        ;;
    build)
        "$PIO" run -e esp32
        "$PIO" run -e esp32 -t buildfs
        echo "Binaries: firmware/.pio/build/esp32/firmware.bin + littlefs.bin (usable via browser OTA)"
        ;;
    help|-h|--help)
        awk 'NR>1 && /^#/ {sub(/^# ?/,""); print; next} NR>1 {exit}' "$SELF"
        ;;
    *)
        echo "unknown command '${1}' — try: ./flash.sh help"
        exit 1
        ;;
esac
