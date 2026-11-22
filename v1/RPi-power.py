#!/usr/bin/env python3
"""
Teensy Power Management for Batocera (Raspberry Pi 5)
Uses lgpio library (compatible with Batocera)
"""

import argparse
import lgpio
import subprocess
import time

GPIO_HEARTBEAT = 18   # Output: tells Teensy "I'm alive"
GPIO_SHUTDOWN = 17    # Input: receives shutdown signal from Teensy
LOOP_SLEEP_S = 0.5

def is_shutdown_requested(h, gpio) -> bool:
    """Check if shutdown signal is HIGH"""
    return lgpio.gpio_read(h, gpio) == 1

def shutdown_system(h, gpio_heartbeat, debug=False) -> None:
    """Execute system shutdown with Batocera-specific handling"""
    if debug:
        print("Turning off heartbeat before shutdown...")
    
    # CRITICAL: Turn off heartbeat FIRST so Teensy knows we're shutting down
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
    parser = argparse.ArgumentParser()
    parser.add_argument("-d", "--debug", action="store_true", help="Print debug messages")
    args = parser.parse_args()
    debug = args.debug

    # Open GPIO chip
    h = lgpio.gpiochip_open(0)
    
    # Configure GPIO18 as output (heartbeat)
    lgpio.gpio_claim_output(h, GPIO_HEARTBEAT)
    lgpio.gpio_write(h, GPIO_HEARTBEAT, 1)  # Set HIGH immediately
    
    # Configure GPIO17 as input (shutdown signal)
    lgpio.gpio_claim_input(h, GPIO_SHUTDOWN)

    if debug:
        print("Teensy power management started (Batocera Mode)")
        print(f"GPIO{GPIO_HEARTBEAT} = HIGH (heartbeat)")
        print(f"Monitoring GPIO{GPIO_SHUTDOWN} for shutdown signal...")

    try:
        while True:
            if is_shutdown_requested(h, GPIO_SHUTDOWN):
                if debug:
                    print("Shutdown signal received from Teensy!")
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
