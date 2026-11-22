#!/usr/bin/env python3
"""
Daemon serial heartbeat + shutdown para el controlador ATX/Teensy 3.0.

CONTROLADOR DE PSU - VERSION 1.1.1-beta (mismo protocolo que v1.4).

Contrato (ver PROTOCOLO.md):
  - UART principal @ 9600 8N1, raw, \\n por linea
    ( /dev/ttyAMA4 por default; override PI_SERIAL_DEVICE )
  - Pi TX (BCM4, pin 7, UART3 TX)  -> Teensy pin 0 (Serial1 RX)
  - Pi RX (BCM5, pin 29, UART3 RX) <- Teensy pin 1 (Serial1 TX)

Mensajes Pi -> controlador: BOOT_OK, ALIVE seq=.. uptime=.., BYE reason=..,
PONG.
Mensajes controlador -> Pi: POWEROFF_REQ, PING, HELLO fw=..,
MODE BYPASS, MODE NORMAL.

Comportamiento:
  1. Abre el dispositivo serie resuelto con termios (stdlib).
  2. Emite BOOT_OK una vez. Envia ALIVE cada ALIVE_PERIOD_MS.
  3. Escucha. Ante POWEROFF_REQ: BYE reason=shutdown_req + apagado.
  4. Ante SIGTERM/SIGINT: BYE reason=poweroff y cierre.

Niveles de log (ver LOG_LEVEL_DEFAULT en configuracion y _resolve_log_level):
  - --debug / -d / DEBUG=1 en linea de comandos fuerzan DEBUG.
  - En otro caso se usa LOG_LEVEL_DEFAULT (constante en este archivo).
"""
from __future__ import annotations

import errno
import logging
import os
import select
import signal
import subprocess
import sys
import termios
import time
from typing import Optional


# --- Configuracion ---------------------------------------------------------

SERIAL_BAUD = 9600

SERIAL_OPEN_RETRIES = 10
SERIAL_OPEN_SLEEP_S = 1.0

ALIVE_PERIOD_MS = 500
LOOP_SLEEP_S = 0.05
PSU_CONTROLLER_VERSION = "1.1.1-beta"  # CONTROLADOR DE PSU v1.1.1-beta (Teensy 3.0)


# Al detectar reinicio (SIGTERM con reboot.target en marcha, etc.)
# enviar QUIET seconds=<QUIET_SECONDS_REBOOT> para que el controlador no
# corte la PSU durante la ventana. El controlador aplica su propio tope
# duro (QUIET_MAX_MS).
#
# 180 s cubre el caso comun (boot ~20-30 s) con holgura generosa
# para fsck/reparacion de la SD, journal recovery, o arranque lento
# despues de apagar sucio. Si la Pi no volvio en ese tiempo, el controlador
# corta la PSU por watchdog (estado fail-safe).
QUIET_SECONDS_REBOOT = 180

# Nivel de log del daemon (y el mismo nombre se envia al controlador en LOG_LEVEL).
# Valores: OFF, ERROR, WARNING, INFO, DEBUG (mayusculas o minusculas).
# DEBUG = maximo detalle (cada TX/RX). Produccion: ERROR.
LOG_LEVEL_DEFAULT = "DEBUG"


# --- Logging ---------------------------------------------------------------

_LOG_OFF = 100

_LEVEL_FROM_NAME: dict[str, int] = {
    "OFF": _LOG_OFF,
    "NONE": _LOG_OFF,
    "0": _LOG_OFF,
    "ERROR": logging.ERROR,
    "WARNING": logging.WARNING,
    "WARN": logging.WARNING,
    "INFO": logging.INFO,
    "DEBUG": logging.DEBUG,
}

# Nombre canonico a enviar al controlador. Debe ser uno de los que el controlador
# entiende en parseLogLevel() (OFF, ERROR, WARNING, INFO, DEBUG).
_LEVEL_TO_CANONICAL_NAME: dict[int, str] = {
    _LOG_OFF: "OFF",
    logging.ERROR: "ERROR",
    logging.WARNING: "WARNING",
    logging.INFO: "INFO",
    logging.DEBUG: "DEBUG",
}


def _force_debug_from_env_or_argv(argv: list[str]) -> bool:
    if os.environ.get("DEBUG") == "1":
        return True
    return any(a in ("-d", "--debug") for a in argv[1:])


def _resolve_log_level(argv: list[str]) -> int:
    if _force_debug_from_env_or_argv(argv):
        return logging.DEBUG
    raw = str(LOG_LEVEL_DEFAULT).strip().upper()
    return _LEVEL_FROM_NAME.get(raw, logging.INFO)


def _build_logger(level: int) -> logging.Logger:
    logger = logging.getLogger("pi_power")
    logger.setLevel(level)
    if logger.handlers:
        return logger
    fmt = logging.Formatter(
        "%(asctime)s %(levelname)s %(message)s",
        datefmt="%Y-%m-%d %H:%M:%S",
    )
    if level >= _LOG_OFF:
        logger.addHandler(logging.NullHandler())
        logger.propagate = False
    else:
        class _FlushStreamHandler(logging.StreamHandler):
            def emit(self, record: logging.LogRecord) -> None:
                super().emit(record)
                self.flush()

        stream = _FlushStreamHandler(sys.stderr)
        stream.setFormatter(fmt)
        logger.addHandler(stream)
    return logger


# --- Serial (termios, sin dependencias externas) ---------------------------

_BAUD_TO_TERMIOS: dict[int, int] = {
    9600: termios.B9600,
    19200: termios.B19200,
    38400: termios.B38400,
    57600: termios.B57600,
    115200: termios.B115200,
}


def _configure_tty(fd: int, baud: int) -> None:
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

    b = _BAUD_TO_TERMIOS.get(baud)
    if b is None:
        raise ValueError(f"Baud no soportado: {baud}")
    ispeed = ospeed = b

    cc = list(cc)
    cc[termios.VMIN] = 0
    cc[termios.VTIME] = 0

    termios.tcsetattr(
        fd, termios.TCSANOW,
        [iflag, oflag, cflag, lflag, ispeed, ospeed, cc],
    )
    termios.tcflush(fd, termios.TCIOFLUSH)


def _resolve_serial_device() -> str:
    """
    Ruta del UART principal. En este proyecto usamos UART3
    (GPIO4/5), que suele aparecer como /dev/ttyAMA4 en Pi 4.
    Override: PI_SERIAL_DEVICE.
    """
    env = os.environ.get("PI_SERIAL_DEVICE", "").strip()
    if env and os.path.exists(env):
        return env
    for path in ("/dev/ttyAMA4", "/dev/serial0", "/dev/ttyAMA0", "/dev/ttyS0"):
        if os.path.exists(path):
            return path
    return "/dev/ttyAMA4"


def _open_serial_with_retry(device_path: str, logger: logging.Logger) -> int:
    last_exc: Optional[Exception] = None
    for attempt in range(1, SERIAL_OPEN_RETRIES + 1):
        try:
            fd = os.open(
                device_path,
                os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK,
            )
            _configure_tty(fd, SERIAL_BAUD)
            return fd
        except Exception as exc:
            last_exc = exc
            logger.warning(
                "open(%s) intento %d/%d fallo: %s",
                device_path, attempt, SERIAL_OPEN_RETRIES, exc,
            )
            time.sleep(SERIAL_OPEN_SLEEP_S)
    raise RuntimeError(
        f"No pude abrir {device_path}: {last_exc}"
    )


def _write_line(fd: int, line: str, logger: logging.Logger) -> None:
    data = (line + "\n").encode("ascii", errors="replace")
    total = 0
    while total < len(data):
        try:
            n = os.write(fd, data[total:])
            if n <= 0:
                break
            total += n
        except OSError as exc:
            if exc.errno in (errno.EAGAIN, errno.EWOULDBLOCK):
                r, w, _ = select.select([], [fd], [], 0.5)
                if not w:
                    logger.error("TX bloqueado; descarto: %r", line)
                    return
                continue
            logger.error("Fallo TX: %s", exc)
            return
    if logger.isEnabledFor(logging.DEBUG):
        logger.debug("TX %s", line)


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
    if len(buf) > 512:
        del buf[:]
    return lines


# --- Acciones del sistema --------------------------------------------------

def _execute_shutdown(logger: logging.Logger) -> None:
    logger.warning("Ejecutando apagado ordenado...")
    try:
        subprocess.run(
            ["batocera-es-swissknife", "--shutdown"],
            check=True,
        )
        return
    except (FileNotFoundError, subprocess.CalledProcessError) as exc:
        logger.warning(
            "batocera-es-swissknife fallo (%s). shutdown -h now.", exc
        )
    try:
        subprocess.run(["shutdown", "-h", "now"], check=False)
    except FileNotFoundError:
        logger.error("No hay shutdown(8); halt manual")
        subprocess.run(["halt"], check=False)


def _systemctl_list_jobs_suggests_reboot() -> bool:
    for bin_path in ("systemctl", "/usr/bin/systemctl", "/bin/systemctl"):
        try:
            p = subprocess.run(
                [bin_path, "list-jobs", "--no-pager"],
                capture_output=True,
                text=True,
                timeout=1.0,
                check=False,
            )
        except (OSError, subprocess.TimeoutExpired):
            continue
        blob = ((p.stdout or "") + "\n" + (p.stderr or "")).lower()
        if "reboot.target" in blob and "start" in blob:
            return True
    return False


def _run_systemd_shutdown_dir_suggests_reboot() -> bool:
    base = "/run/systemd/shutdown"
    try:
        names = os.listdir(base)
    except OSError:
        return False
    for name in names:
        path = os.path.join(base, name)
        try:
            with open(path, "r", encoding="utf-8", errors="ignore") as fh:
                body = fh.read().upper()
        except OSError:
            continue
        if "MODE=REBOOT" in body:
            return True
        if "REBOOT" in body and "POWEROFF" not in body and "HALT" not in body:
            return True
    return False


def _proc1_cmdline() -> str:
    """Contenido crudo de /proc/1/cmdline, separado por espacios."""
    try:
        with open("/proc/1/cmdline", "rb") as fh:
            raw = fh.read()
    except OSError:
        return ""
    return raw.replace(b"\x00", b" ").decode("ascii", errors="replace").strip()


SHUTDOWN_TRACE_PATH = "/userdata/system/pi_power.shutdown.log"


def _write_shutdown_trace(reason: str, init_cmd: str, is_reboot: bool) -> None:
    """
    Escribe una traza durable del shutdown. Usa O_APPEND + fsync para
    sobrevivir a un unmount sucio. Se separa del log principal porque
    ese pasa por el pipe de la shell y puede perderse en caida.
    """
    line = (
        f"{time.strftime('%Y-%m-%d %H:%M:%S')} "
        f"reason={reason} is_reboot={is_reboot} init={init_cmd!r}\n"
    )
    try:
        fd = os.open(
            SHUTDOWN_TRACE_PATH,
            os.O_WRONLY | os.O_APPEND | os.O_CREAT,
            0o644,
        )
    except OSError:
        return
    try:
        os.write(fd, line.encode("utf-8", errors="replace"))
        try:
            os.fsync(fd)
        except OSError:
            pass
    finally:
        try:
            os.close(fd)
        except OSError:
            pass


def _busybox_init_runlevel_is_reboot() -> bool:
    """
    En SysV/BusyBox init (Batocera), el runlevel actual se refleja en
    /proc/1/cmdline como 'init [N]'. Durante un reboot N=6, durante un
    poweroff/halt N=0, en operacion normal N=3. rcK (que mata a pi.py)
    corre DESPUES de que init ya cambio el runlevel, asi que en el
    momento del SIGTERM el cmdline ya dice [6] o [0].
    """
    cmd = _proc1_cmdline()
    # Buscamos '[6]' explicito. '[06]' no se usa en inittab aunque la
    # accion shd0 lista '06' como mascara.
    if "[6]" in cmd:
        return True
    return False


def _shutdown_is_reboot() -> bool:
    """
    SIGTERM vale para apagado y para reinicio. Consultamos varias
    fuentes (systemd si esta, BusyBox init via /proc/1/cmdline).
    """
    if _systemctl_list_jobs_suggests_reboot():
        return True
    if _run_systemd_shutdown_dir_suggests_reboot():
        return True
    if _busybox_init_runlevel_is_reboot():
        return True
    return False


# --- Senales ---------------------------------------------------------------

_terminate_requested = False
_terminate_reason = "poweroff"


def _install_signal_handlers() -> None:
    def _handler(signum, _frame):
        global _terminate_requested, _terminate_reason
        _terminate_requested = True
        _terminate_reason = "sigint" if signum == signal.SIGINT else "poweroff"

    signal.signal(signal.SIGTERM, _handler)
    signal.signal(signal.SIGINT, _handler)


# --- Bucle principal -------------------------------------------------------

def _now_ms() -> int:
    return int(time.monotonic() * 1000)


def _uptime_s() -> int:
    try:
        with open("/proc/uptime", "r", encoding="ascii") as fh:
            return int(float(fh.read().split()[0]))
    except OSError:
        return 0


def _canonical_level_name(level: int) -> str:
    return _LEVEL_TO_CANONICAL_NAME.get(level, "INFO")


def _send_log_level(fd: int, level: int, logger: logging.Logger) -> None:
    name = _canonical_level_name(level)
    _write_line(fd, f"LOG_LEVEL {name}", logger)


def _reemit_esp_log(
    line: str,
    logger: logging.Logger,
) -> None:
    # line == "LOG <LVL> <texto...>"
    parts = line.split(" ", 2)
    if len(parts) < 3:
        logger.debug("LOG malformado: %r", line)
        return
    lvl_name = parts[1].strip().upper()
    text = parts[2]
    level = _LEVEL_FROM_NAME.get(lvl_name, logging.INFO)
    if level >= _LOG_OFF:
        return
    logger.log(level, "[controlador] %s", text)


def _handle_incoming(
    line: str,
    logger: logging.Logger,
    state: dict,
    fd: int,
    resolved_level: int,
) -> None:
    if logger.isEnabledFor(logging.DEBUG):
        logger.debug("RX %s", line)
    if line == "MODE BYPASS":
        state["bypass_active"] = True
        logger.warning("controlador entro en modo BYPASS: POWEROFF_REQ se ignorara")
        return
    if line == "MODE NORMAL":
        state["bypass_active"] = False
        logger.warning("controlador salio de modo BYPASS: link de vuelta en pines")
        return
    if line == "POWEROFF_REQ":
        if state.get("bypass_active"):
            logger.warning("POWEROFF_REQ ignorado: controlador en modo BYPASS")
            return
        logger.warning("POWEROFF_REQ recibido desde controlador")
        state["poweroff_req"] = True
        return
    if line == "PING":
        state["pong_due"] = True
        return
    if line.startswith("LOG "):
        _reemit_esp_log(line, logger)
        return
    if line.startswith("LOG_LEVEL_ACK"):
        logger.debug("controlador aplico log_level: %s", line)
        return
    if line.startswith("HELLO"):
        logger.info("controlador dice: %s", line)
        # Si el controlador reinicio, reenviar el nivel.
        _send_log_level(fd, resolved_level, logger)
        return
    logger.debug("RX desconocido: %r", line)


def main() -> int:
    level = _resolve_log_level(sys.argv)
    logger = _build_logger(level)
    _install_signal_handlers()

    device_path = _resolve_serial_device()
    logger.debug(
        "Arrancando pi.py serial (%s @ %d)",
        device_path, SERIAL_BAUD,
    )

    try:
        fd = _open_serial_with_retry(device_path, logger)
    except RuntimeError as exc:
        logger.error("%s", exc)
        return 1

    rx_buf = bytearray()
    state: dict = {
        "poweroff_req": False,
        "pong_due": False,
        "bypass_active": False,
    }

    _send_log_level(fd, level, logger)
    _write_line(fd, "BOOT_OK", logger)
    logger.info(
        "UART %s @ %d — BOOT_OK (log Pi=%s, controlador recibe LOG_LEVEL igual)",
        device_path,
        SERIAL_BAUD,
        _canonical_level_name(level),
    )

    seq = 0
    next_alive_at_ms = _now_ms()

    try:
        while not _terminate_requested and not state["poweroff_req"]:
            now_ms = _now_ms()

            if now_ms >= next_alive_at_ms:
                seq += 1
                _write_line(
                    fd,
                    f"ALIVE seq={seq} uptime={_uptime_s()}",
                    logger,
                )
                next_alive_at_ms = now_ms + ALIVE_PERIOD_MS

            r, _, _ = select.select([fd], [], [], LOOP_SLEEP_S)
            if fd in r:
                for line in _read_lines(fd, rx_buf):
                    _handle_incoming(line, logger, state, fd, level)

            if state["pong_due"]:
                _write_line(fd, "PONG", logger)
                state["pong_due"] = False

        if state["poweroff_req"]:
            _write_line(fd, "BYE reason=shutdown_req", logger)
            _drain_tx(fd)
            _execute_shutdown(logger)
            return 0

        reason = _terminate_reason
        init_cmd = _proc1_cmdline()
        is_reboot = reason == "reboot" or _shutdown_is_reboot()
        # Traza DURABLE (fsync) en archivo aparte. El log normal puede
        # perderse en un unmount sucio; este archivo queda aunque el
        # sistema se caiga feo.
        _write_shutdown_trace(reason, init_cmd, is_reboot)
        logger.info(
            "shutdown detectado: signal_reason=%s is_reboot=%s init=%r",
            reason, is_reboot, init_cmd,
        )
        if is_reboot:
            reason = "reboot"
            _write_line(
                fd,
                f"QUIET seconds={QUIET_SECONDS_REBOOT} reason=reboot",
                logger,
            )
        logger.info("SIGTERM/SIGINT -> BYE reason=%s", reason)
        _write_line(fd, f"BYE reason={reason}", logger)
        _drain_tx(fd)
    finally:
        try:
            termios.tcdrain(fd)
        except Exception:
            pass
        try:
            os.close(fd)
        except Exception:
            pass

    return 0


def _drain_tx(fd: int) -> None:
    try:
        termios.tcdrain(fd)
    except Exception:
        pass


if __name__ == "__main__":
    sys.exit(main())
