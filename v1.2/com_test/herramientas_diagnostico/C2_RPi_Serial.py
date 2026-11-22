#!/usr/bin/env python3
"""
C2_RPi_Serial.py — pareja de C1_ESP_Serial.ino.

Pi: abre el UART ( /dev/serial0 o /dev/ttyAMA0 en Batocera ) @ 9600 8N1
(raw), envia "yyy\\n" cada 1 s y
loguea cada linea RX y cada TX a stdout y a un archivo.

Cableado (comunicacion serial/):
  - Pi pin 8  (BCM14 TXD) -> ESP D5
  - Pi pin 10 (BCM15 RXD) <- ESP D7

Requiere:
  - enable_uart=1 en /boot/config.txt
  - dtoverlay=miniuart-bt (para no perder Bluetooth) o disable-bt
  - /boot/cmdline.txt sin console=serial0|ttyAMA0|ttyS0
  - reboot despues del cambio

Uso:
  python3 /userdata/system/C2_RPi_Serial.py
"""
from __future__ import annotations

import errno
import os
import select
import sys
import termios
import time

SERIAL_BAUD = 9600
TX_PERIOD_S = 1.0
LOG_FILE = "/userdata/system/serial_test.log"


def _resolve_serial_device() -> str:
    """Batocera a veces no crea /dev/serial0; el PL011 suele ser ttyAMA0."""
    env = os.environ.get("PI_SERIAL_DEVICE", "").strip()
    if env and os.path.exists(env):
        return env
    for path in ("/dev/serial0", "/dev/ttyAMA0", "/dev/ttyS0"):
        if os.path.exists(path):
            return path
    return "/dev/serial0"

_BAUD_MAP = {
    9600: termios.B9600,
    19200: termios.B19200,
    38400: termios.B38400,
    57600: termios.B57600,
    115200: termios.B115200,
}


def _stamp() -> str:
    return time.strftime("%H:%M:%S")


def _open_tty(path: str, baud: int) -> int:
    fd = os.open(path, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
    attrs = termios.tcgetattr(fd)
    iflag, oflag, cflag, lflag, ispeed, ospeed, cc = attrs
    iflag &= ~(
        termios.IGNBRK | termios.BRKINT | termios.PARMRK
        | termios.ISTRIP | termios.INLCR | termios.IGNCR
        | termios.ICRNL | termios.IXON | termios.IXOFF
    )
    oflag &= ~termios.OPOST
    lflag &= ~(
        termios.ECHO | termios.ECHONL | termios.ICANON
        | termios.ISIG | termios.IEXTEN
    )
    cflag &= ~(termios.CSIZE | termios.PARENB | termios.CSTOPB | termios.CRTSCTS)
    cflag |= termios.CS8 | termios.CLOCAL | termios.CREAD
    b = _BAUD_MAP[baud]
    ispeed = ospeed = b
    cc = list(cc)
    cc[termios.VMIN] = 0
    cc[termios.VTIME] = 0
    termios.tcsetattr(
        fd, termios.TCSANOW,
        [iflag, oflag, cflag, lflag, ispeed, ospeed, cc],
    )
    termios.tcflush(fd, termios.TCIOFLUSH)
    return fd


def _write_all(fd: int, data: bytes) -> None:
    total = 0
    while total < len(data):
        try:
            n = os.write(fd, data[total:])
            if n <= 0:
                break
            total += n
        except OSError as exc:
            if exc.errno in (errno.EAGAIN, errno.EWOULDBLOCK):
                _, w, _ = select.select([], [fd], [], 0.5)
                if not w:
                    return
                continue
            raise


def _read_lines(fd: int, buf: bytearray) -> list[str]:
    try:
        chunk = os.read(fd, 256)
    except OSError as exc:
        if exc.errno in (errno.EAGAIN, errno.EWOULDBLOCK):
            return []
        raise
    if not chunk:
        return []
    buf.extend(chunk)
    lines: list[str] = []
    while True:
        idx = buf.find(b"\n")
        if idx < 0:
            break
        raw = bytes(buf[:idx])
        del buf[: idx + 1]
        line = raw.replace(b"\r", b"").decode("ascii", errors="replace").strip()
        if line:
            lines.append(line)
    if len(buf) > 1024:
        del buf[:]
    return lines


def main() -> int:
    try:
        log = open(LOG_FILE, "w", encoding="utf-8", buffering=1)
    except OSError as exc:
        print(f"No pude abrir {LOG_FILE}: {exc}", file=sys.stderr)
        return 1

    def emit(msg: str) -> None:
        line = f"[{_stamp()}] {msg}"
        print(line, flush=True)
        try:
            log.write(line + "\n")
        except OSError:
            pass

    dev = _resolve_serial_device()
    emit(f"--- C2 RPi SERIAL en {dev} @ {SERIAL_BAUD} ---")

    try:
        fd = _open_tty(dev, SERIAL_BAUD)
    except OSError as exc:
        emit(f"ERROR abriendo puerto: {exc}")
        return 1
    emit("puerto abierto")

    rx_buf = bytearray()
    next_tx_at = 0.0

    try:
        while True:
            now = time.monotonic()
            if now >= next_tx_at:
                _write_all(fd, b"yyy\n")
                emit("TX yyy")
                next_tx_at = now + TX_PERIOD_S

            r, _, _ = select.select([fd], [], [], 0.1)
            if fd in r:
                for line in _read_lines(fd, rx_buf):
                    emit(f"RX {line}")
    except KeyboardInterrupt:
        emit("interrumpido por usuario")
    finally:
        try:
            termios.tcdrain(fd)
        except Exception:
            pass
        try:
            os.close(fd)
        except Exception:
            pass
        log.close()

    return 0


if __name__ == "__main__":
    sys.exit(main())
