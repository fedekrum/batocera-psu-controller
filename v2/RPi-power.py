"""
Microcontroller Power Management for Batocera (Raspberry Pi 5)
Uses lgpio library (compatible with Batocera)
"""

import argparse
import lgpio
import subprocess
import time

GPIO_HEARTBEAT = 14   # Output: tells MCU "I'm alive"
GPIO_SHUTDOWN = 4     # Input: receives shutdown signal from MCU
LOOP_SLEEP_S = 0.5

# Archivo de log para auditoría
LOG_FILE = "/userdata/system/rpi_power.log"

def log_event(message):
    try:
        t = time.strftime("%Y-%m-%d %H:%M:%S")
        with open(LOG_FILE, "a") as f:
            f.write(f"[{t}] {message}\n")
    except:
        pass

def is_shutdown_requested(h, gpio) -> bool:
    """Check if shutdown signal is HIGH"""
    return lgpio.gpio_read(h, gpio) == 1

def shutdown_system(h, gpio_heartbeat, debug=False) -> None:
    """Execute system shutdown with Batocera-specific handling"""
    if debug:
        print("Turning off heartbeat before shutdown...")
    
    # CRITICAL: Turn off heartbeat FIRST so MCU knows we're shutting down
    lgpio.gpio_write(h, gpio_heartbeat, 0)
    
    if debug:
        print("Executing Batocera safe shutdown...")
    
    # Try Batocera's native shutdown command (saves metadata)
    try:
        subprocess.run(["batocera-es-swissknife", "--shutdown"], check=True)
    except (FileNotFoundError, subprocess.CalledProcessError):
        # Fallback to generic shutdown
        if debug:
            print("batocera-es-swissknife not found, using generic shutdown.")
        subprocess.run(["shutdown", "-h", "now"])

def main():
    # Registrar inicio
    log_event("--- SCRIPT POWER INICIADO ---")
    
    parser = argparse.ArgumentParser()
    parser.add_argument("-d", "--debug", action="store_true", help="Print debug messages")
    args = parser.parse_args()
    debug = args.debug

    # Open GPIO chip
    h = lgpio.gpiochip_open(0)
    
    # Configure GPIO14 as output (heartbeat)
    lgpio.gpio_claim_output(h, GPIO_HEARTBEAT)
    lgpio.gpio_write(h, GPIO_HEARTBEAT, 1)  # Set HIGH immediately
    log_event("Heartbeat ACTIVADO en GPIO 14")
    
    # Configure GPIO4 as input (shutdown signal) with PULL_DOWN
    # Esto evita que el ruido electrico dispare apagados falsos
    lgpio.gpio_claim_input(h, GPIO_SHUTDOWN, lgpio.SET_PULL_DOWN)

    if debug:
        print("MCU power management started (Batocera Mode)")
        print(f"GPIO{GPIO_HEARTBEAT} = HIGH (heartbeat)")
        print(f"Monitoring GPIO{GPIO_SHUTDOWN} for shutdown signal...")

    try:
        while True:
            if is_shutdown_requested(h, GPIO_SHUTDOWN):
                if debug:
                    print("Shutdown signal received from MCU!")
                shutdown_system(h, GPIO_HEARTBEAT, debug)
                break
            time.sleep(LOOP_SLEEP_S)
    except KeyboardInterrupt:
        if debug:
            print("\nScript stopped by user.")
    finally:
        lgpio.gpio_write(h, GPIO_HEARTBEAT, 0)  # Turn off heartbeat
        lgpio.gpiochip_close(h)

if __name__ == "__main__":
    main()
