#!/usr/bin/env python3
"""
B1_RPi_Emisor.py — alterna BCM4 y BCM14 para prueba de cable con B2.

Mismo header que comunicacion/esp.ino:
  - Pin fisico 7 = BCM 4  -> cable ESP D5
  - Pin fisico 8 = BCM 14 -> cable ESP D7

En produccion pi.py deja BCM4 en HIGH fijo; aqui se alternan 4 y 14
solo para ver llegada al ESP (B2). No usar mientras corre pi.py.
"""
import lgpio
import sys
import time

BCM_FOR_HEADER_PIN7_ESP_D5 = 4
BCM_FOR_HEADER_PIN8_ESP_D7 = 14

HALF_PERIOD_S = 0.5
NUM_STEPS = 60


def main():
    h = None
    try:
        h = lgpio.gpiochip_open(0)
        lgpio.gpio_claim_output(h, BCM_FOR_HEADER_PIN7_ESP_D5)
        lgpio.gpio_claim_output(h, BCM_FOR_HEADER_PIN8_ESP_D7)

        print("--- B1 RPi EMISOR (~60 s, alterna pin7/pin8) ---")
        print(
            f"BCM4 (header 7, D5) y BCM14 (header 8, D7) "
            f"cada {HALF_PERIOD_S}s..."
        )

        for i in range(NUM_STEPS):
            lgpio.gpio_write(h, BCM_FOR_HEADER_PIN7_ESP_D5, 1)
            lgpio.gpio_write(h, BCM_FOR_HEADER_PIN8_ESP_D7, 0)
            time.sleep(HALF_PERIOD_S)
            lgpio.gpio_write(h, BCM_FOR_HEADER_PIN7_ESP_D5, 0)
            lgpio.gpio_write(h, BCM_FOR_HEADER_PIN8_ESP_D7, 1)
            time.sleep(HALF_PERIOD_S)
            sys.stdout.write(f"\rPasos restantes: {NUM_STEPS - i}  ")
            sys.stdout.flush()

        print("\nTest finalizado.")

    except OSError as e:
        print(f"\nError (lgpio): {e}")
    except Exception as e:
        print(f"\nError: {e}")
    finally:
        if h is not None:
            lgpio.gpio_write(h, BCM_FOR_HEADER_PIN7_ESP_D5, 0)
            lgpio.gpio_write(h, BCM_FOR_HEADER_PIN8_ESP_D7, 0)
            lgpio.gpiochip_close(h)


if __name__ == "__main__":
    main()
