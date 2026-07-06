#!/usr/bin/env python3
"""Serial inverter simulator for bench-testing the ESP32 without hardware.

Wire a USB-serial adapter to the ESP32's UART2 (adapter RX -> GPIO17,
adapter TX -> GPIO16, common GND) and run:

    python3 inverter_sim.py /dev/ttyUSB0 --type pip
    python3 inverter_sim.py /dev/ttyUSB0 --type mpi
    python3 inverter_sim.py /dev/ttyUSB0 --type pcm

It answers the poll queries with canned-but-jittered data, ACKs setter
commands, and logs every frame with CRC validation, so the full chain
(engine -> parse -> MQTT/WS -> dashboard animation) can be exercised.
"""
import argparse
import random
import sys
import time

try:
    import serial  # pyserial
except ImportError:
    sys.exit("pip install pyserial")


def crc16(data: bytes) -> int:
    """CRC16-XMODEM with the MPP 0x28/0x0D/0x0A byte bump."""
    tbl = [0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50A5, 0x60C6, 0x70E7,
           0x8108, 0x9129, 0xA14A, 0xB16B, 0xC18C, 0xD1AD, 0xE1CE, 0xF1EF]
    c = 0
    for b in data:
        da = (c >> 8) >> 4
        c = ((c << 4) & 0xFFFF) ^ tbl[(da ^ (b >> 4)) & 0xF]
        da = (c >> 8) >> 4
        c = ((c << 4) & 0xFFFF) ^ tbl[(da ^ (b & 0xF)) & 0xF]
    lo, hi = c & 0xFF, (c >> 8) & 0xFF
    if lo in (0x28, 0x0D, 0x0A):
        lo += 1
    if hi in (0x28, 0x0D, 0x0A):
        hi += 1
    return (hi << 8) | lo


def frame_pi30(payload: str) -> bytes:
    body = ("(" + payload).encode()
    c = crc16(body)
    return body + bytes([c >> 8, c & 0xFF, 13])


def frame_pi17(payload: str) -> bytes:
    body = f"^D{len(payload) + 3:03d}{payload}".encode()
    c = crc16(body)
    return body + bytes([c >> 8, c & 0xFF, 13])


def ack_pi30(ok=True) -> bytes:
    return frame_pi30("ACK" if ok else "NAK")


def ack_pi17(ok=True) -> bytes:
    body = ("^1" if ok else "^0").encode()
    c = crc16(body)
    return body + bytes([c >> 8, c & 0xFF, 13])


def j(v, spread=0.03):
    """jitter a number a few percent so the dashboard visibly updates"""
    return v * (1 + random.uniform(-spread, spread))


# ---------------------------------------------------------------- responses
def pip_responses(cmd):
    if cmd == "QPIGS":
        return frame_pi30(
            f"{j(230.2):05.1f} {j(50.0, .001):04.1f} {j(230.0):05.1f} {j(50.0,.001):04.1f} "
            f"{int(j(1450)):04d} {int(j(1370)):04d} {int(j(28)):03d} {int(j(400)):03d} "
            f"{j(52.3):05.2f} {int(j(12)):03d} {int(j(78)):03d} {int(j(41)):04d} "
            f"{j(6.2):04.1f} {j(310.5):05.1f} {j(52.2):05.2f} {int(0):05d} "
            f"00110110 00 00 {int(j(1900)):05d} 010")
    if cmd == "QMOD":
        return frame_pi30("B")
    if cmd == "QPIWS":
        return frame_pi30("00000000000000000000000000000000")
    if cmd == "QFLAG":
        return frame_pi30("EakxyDbjuvz")
    if cmd == "QPIRI":
        return frame_pi30("230.0 21.7 230.0 50.0 21.7 5000 4000 48.0 46.0 42.0 "
                          "56.4 54.0 0 10 010 1 0 0 6 01 0 0 54.0 0 1")
    if cmd == "QID":
        return frame_pi30("92931904100005")
    if cmd == "QVFW":
        return frame_pi30("VERFW:00072.70")
    return None


def pcm_responses(cmd):
    if cmd == "QPIGS":
        return frame_pi30(f"{j(87.6):05.1f} {j(51.2):05.2f} {j(8.8):05.2f} "
                          f"{j(4.7):05.2f} {j(4.1):05.2f} {int(j(450)):04d}")
    if cmd == "QPIRI":
        return frame_pi30("0060 48 60.0 56.4 54.0 0 1 0.0 1 48 0 46.0 1")
    if cmd == "QID":
        return frame_pi30("PCM60X0000001")
    if cmd == "QVFW":
        return frame_pi30("VERFW:00001.10")
    return None


def mpi_responses(cmd):
    if cmd == "GS":
        return frame_pi17(
            f"{int(j(3462)):04d},{int(j(3468)):04d},{int(j(40)):04d},{int(j(35)):04d},"
            f"{int(j(503)):04d},{int(j(71)):03d},+{int(j(120)):05d},"
            f"{int(j(2369)):04d},{int(j(2367)):04d},{int(j(2350)):04d},{int(j(5000,.0002)):04d},"
            f"0012,0011,0010,{int(j(2371)):04d},{int(j(2365)):04d},{int(j(2352)):04d},"
            f"{int(j(5001,.0002)):04d},0021,0020,0019,{int(j(31)):03d},{int(j(34)):03d},000,0")
    if cmd == "PS":
        return frame_pi17(
            f"{int(j(1390)):05d},{int(j(1220)):05d},{int(j(150)):+06d},,,,,"
            f"{int(j(502)):04d},{int(j(973)):04d},{int(j(385)):04d},{int(j(1860)):05d},"
            f"{int(j(593)):04d},{int(j(1040)):04d},{int(j(399)):04d},{int(j(2032)):05d},"
            f"{int(j(31)):03d},1,1,1,1,2,1")
    if cmd == "MOD":
        return frame_pi17("05")
    if cmd == "WS":
        return frame_pi17(",".join(["0"] * 18))
    if cmd == "FLAG":
        return frame_pi17("0,0,1,0,1,0")
    if cmd == "HECS":
        return frame_pi17("01,1,0,1,1,1,0,0,0")
    if cmd == "BATS":
        return frame_pi17("1200,0576,0546,0060,060,0520,0420,0480,0460,0500,1,,,0,0540,000,0300,0250")
    if cmd == "FPADJ":
        return frame_pi17("1,0000,1,0099,1,0109,1,0112")
    if cmd == "GPMP":
        return frame_pi17("10000")
    if cmd == "ET":
        return frame_pi17("00534267")
    if cmd == "ID":
        return frame_pi17("96332309100452")
    if cmd == "VFW":
        return frame_pi17("VERFW:00052.30")
    return None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("port")
    ap.add_argument("--type", choices=["pip", "pcm", "mpi"], default="pip")
    ap.add_argument("--baud", type=int, default=2400)
    ap.add_argument("--nak", action="store_true", help="NAK all setters")
    args = ap.parse_args()

    pi17 = args.type == "mpi"
    tbl = {"pip": pip_responses, "pcm": pcm_responses, "mpi": mpi_responses}[args.type]

    s = serial.Serial(args.port, args.baud, timeout=0.1)
    print(f"simulating {args.type.upper()} on {args.port} @ {args.baud}")
    buf = b""
    while True:
        buf += s.read(64)
        if b"\r" not in buf:
            continue
        line, _, buf = buf.partition(b"\r")
        if not line:
            continue

        if pi17:
            # ^Pnnn<CMD> or ^Snnn<CMD>, no TX CRC
            if len(line) < 6 or line[0:1] != b"^":
                print(f"?? {line!r}")
                continue
            kind, cmd = chr(line[1]), line[5:].decode(errors="replace")
            print(f"<- {kind} {cmd}")
            if kind == "S":
                s.write(ack_pi17(not args.nak))
                continue
            resp = tbl(cmd)
        else:
            # <CMD><crcHi><crcLo>
            body, rx = line[:-2], line[-2:]
            c = crc16(body)
            if bytes([c >> 8, c & 0xFF]) != rx:
                print(f"!! bad CRC from device: {line!r}")
                continue
            cmd = body.decode(errors="replace")
            print(f"<- {cmd}")
            if not cmd.startswith("Q"):
                s.write(ack_pi30(not args.nak))
                continue
            resp = tbl(cmd)

        if resp:
            time.sleep(0.15)  # a real inverter thinks about it too
            s.write(resp)
            print(f"-> {resp[:70]!r}{'...' if len(resp) > 70 else ''}")
        else:
            print(f"   (no canned response for {cmd})")


if __name__ == "__main__":
    main()
