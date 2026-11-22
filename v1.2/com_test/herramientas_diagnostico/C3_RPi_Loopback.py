#!/usr/bin/env python3
"""
C3_RPi_Loopback.py

Prueba SIMPLE de puente UART en Raspberry Pi:
  - hacer puente fisico pin 8 (TXD) <-> pin 10 (RXD)
  - ejecutar este script
  - el script envia lineas y espera recibir exactamente lo mismo

Uso:
  python3 C3_RPi_Loopback.py
  python3 C3_RPi_Loopback.py --seconds 8 --device /dev/ttyAMA0 --baud 9600
"""

from __future__ import annotations

import argparse
import errno
import os
import select
import termios
import time


def resolve_device(cli_device: str | None) -> str:
    if cli_device:
        return cli_device
    for path in ("/dev/serial0", "/dev/ttyAMA0", "/dev/ttyS0"):
        if os.path.exists(path):
            return path
    return "/dev/serial0"


BAUD_MAP = {
    9600: termios.B9600,
    19200: termios.B19200,
    38400: termios.B38400,
    57600: termios.B57600,
    115200: termios.B115200,
}


def open_uart(path: str, baud: int) -> int:
    fd = os.open(path, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
    attrs = termios.tcgetattr(fd)
    iflag, oflag, cflag, lflag, ispeed, ospeed, cc = attrs

    iflag &= ~(
        termios.IGNBRK
        | termios.BRKINT
        | termios.PARMRK
        | termios.ISTRIP
        | termios.INLCR
        | termios.IGNCR
        | termios.ICRNL
        | termios.IXON
        | termios.IXOFF
    )
    oflag &= ~termios.OPOST
    lflag &= ~(termios.ECHO | termios.ECHONL | termios.ICANON | termios.ISIG | termios.IEXTEN)
    cflag &= ~(termios.CSIZE | termios.PARENB | termios.CSTOPB | termios.CRTSCTS)
    cflag |= termios.CS8 | termios.CLOCAL | termios.CREAD

    speed = BAUD_MAP[baud]
    ispeed = ospeed = speed
    cc = list(cc)
    cc[termios.VMIN] = 0
    cc[termios.VTIME] = 0
    termios.tcsetattr(fd, termios.TCSANOW, [iflag, oflag, cflag, lflag, ispeed, ospeed, cc])
    termios.tcflush(fd, termios.TCIOFLUSH)
    return fd


def write_all(fd: int, payload: bytes) -> None:
    sent = 0
    while sent < len(payload):
        try:
            n = os.write(fd, payload[sent:])
            if n <= 0:
                return
            sent += n
        except OSError as exc:
            if exc.errno in (errno.EAGAIN, errno.EWOULDBLOCK):
                _, w, _ = select.select([], [fd], [], 0.3)
                if not w:
                    return
                continue
            raise


def read_lines(fd: int, buf: bytearray) -> list[str]:
    try:
        chunk = os.read(fd, 256)
    except OSError as exc:
        if exc.errno in (errno.EAGAIN, errno.EWOULDBLOCK):
            return []
        raise
    if not chunk:
        return []
    buf.extend(chunk)
    out: list[str] = []
    while True:
        idx = buf.find(b"\n")
        if idx < 0:
            break
        raw = bytes(buf[:idx])
        del buf[: idx + 1]
        line = raw.replace(b"\r", b"").decode("ascii", errors="replace").strip()
        if line:
            out.append(line)
    if len(buf) > 1024:
        del buf[:]
    return out


def stamp() -> str:
    return time.strftime("%H:%M:%S")


def main() -> int:
    parser = argparse.ArgumentParser(description="Prueba de loopback UART (puente TX-RX).")
    parser.add_argument("--device", default=None, help="Dispositivo serie, p.ej. /dev/ttyAMA0")
    parser.add_argument("--baud", type=int, default=9600, choices=sorted(BAUD_MAP.keys()))
    parser.add_argument("--seconds", type=float, default=10.0, help="Duracion de prueba")
    parser.add_argument("--period", type=float, default=0.5, help="Periodo entre envios")
    args = parser.parse_args()

    dev = resolve_device(args.device)
    print(f"[{stamp()}] loopback start dev={dev} baud={args.baud} seconds={args.seconds}")

    try:
        fd = open_uart(dev, args.baud)
    except OSError as exc:
        print(f"[{stamp()}] ERROR abriendo UART: {exc}")
        return 2

    tx_total = 0
    rx_match = 0
    rx_other = 0
    expected: list[str] = []
    buf = bytearray()
    t0 = time.monotonic()
    next_tx = t0
    seq = 0

    try:
        while True:
            now = time.monotonic()
            if now - t0 >= args.seconds:
                break

            if now >= next_tx:
                seq += 1
                msg = f"LB{seq:03d}"
                expected.append(msg)
                write_all(fd, (msg + "\n").encode("ascii"))
                tx_total += 1
                print(f"[{stamp()}] TX {msg}")
                next_tx = now + args.period

            r, _, _ = select.select([fd], [], [], 0.1)
            if fd in r:
                for line in read_lines(fd, buf):
                    if expected and line == expected[0]:
                        expected.pop(0)
                        rx_match += 1
                        print(f"[{stamp()}] RX {line} (ok)")
                    else:
                        rx_other += 1
                        print(f"[{stamp()}] RX {line} (unexpected)")
    finally:
        try:
            termios.tcdrain(fd)
        except Exception:
            pass
        try:
            os.close(fd)
        except Exception:
            pass

    print(
        f"[{stamp()}] resumen tx={tx_total} rx_ok={rx_match} "
        f"rx_unexpected={rx_other} pendientes={len(expected)}"
    )

    if tx_total > 0 and rx_match == tx_total and rx_other == 0:
        print(f"[{stamp()}] RESULTADO: PASS (loopback OK)")
        return 0

    print(f"[{stamp()}] RESULTADO: FAIL (RX no responde como loopback)")
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
