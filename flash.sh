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
#   ./flash.sh --ip <host> [fw|fs|all]  flash over the network (OTA), e.g.:
#   ./flash.sh ota <host> [fw|fs|all]       ./flash.sh --ip 192.168.1.42 all
#       ./flash.sh --ip ESP-solar.local     ./flash.sh --ip solar.local fs
#
# Options via env:
#   PORT=/dev/ttyUSB1 ./flash.sh   force a specific USB port
#   OTA_PASS=secret ./flash.sh --ip <host>   OTA password (if one is set)

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

# Network (OTA) flash of one or more targets to a host/IP.
ota_flash() {
    local host="$1"; shift
    local what="${1:-fw}"
    local flags=(--upload-port "$host")
    [ -n "${OTA_PASS:-}" ] && flags+=(--upload-flags "--auth=$OTA_PASS")
    case "$what" in
        fw)  "$PIO" run -e esp32 -t upload   "${flags[@]}" ;;
        fs)  "$PIO" run -e esp32 -t uploadfs "${flags[@]}" ;;
        all) "$PIO" run -e esp32 -t upload   "${flags[@]}"
             "$PIO" run -e esp32 -t uploadfs "${flags[@]}" ;;
        *)   echo "unknown target '$what' (fw|fs|all)"; exit 1 ;;
    esac
}

# --ip <host> [fw|fs|all]  — network flash (alias for `ota`)
if [ "${1:-}" = "--ip" ]; then
    ota_flash "${2:?usage: ./flash.sh --ip <host|ip> [fw|fs|all]}" "${3:-fw}"
    exit $?
fi

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
        ota_flash "${2:?usage: ./flash.sh ota <host|ip> [fw|fs|all]}" "${3:-fw}"
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
