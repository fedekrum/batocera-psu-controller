#!/usr/bin/env python3
"""
A2_RPi_Receptor.py — lee BCM4 y BCM14 mientras A1 pulsa D5 y D7.

Mismo header que comunicacion/esp.ino:
  - Pin fisico 7 = BCM 4  (linea del ESP D5)
  - Pin fisico 8 = BCM 14 (linea del ESP D7)

En este test la Pi no debe escribir esos GPIO (solo A1 en el ESP).
"""
import lgpio
import time

# BCM del header 40 pines (no confundir con nombres “heartbeat” del
# firmware de produccion: aqui solo importa el numero de GPIO).
BCM_FOR_HEADER_PIN7_ESP_D5 = 4
BCM_FOR_HEADER_PIN8_ESP_D7 = 14

LOG_FILE = "/userdata/system/debug_final.log"
POLL_S = 0.01


def main():
    h = None
    try:
        with open(LOG_FILE, "w", encoding="utf-8") as f:
            f.write(f"--- A2 RPi RECEPTOR {time.ctime()} ---\n")
            f.write(
                "BCM4 (header 7) = cable D5 ESP | "
                "BCM14 (header 8) = cable D7 ESP\n"
            )

        h = lgpio.gpiochip_open(0)
        lgpio.gpio_claim_input(h, BCM_FOR_HEADER_PIN7_ESP_D5)
        lgpio.gpio_claim_input(h, BCM_FOR_HEADER_PIN8_ESP_D7)

        print(f"Log: {LOG_FILE} (tail -f)")

        last_pin7 = -1
        last_pin8 = -1

        while True:
            pin7 = lgpio.gpio_read(h, BCM_FOR_HEADER_PIN7_ESP_D5)
            pin8 = lgpio.gpio_read(h, BCM_FOR_HEADER_PIN8_ESP_D7)

            if pin7 != last_pin7 or pin8 != last_pin8:
                t = time.strftime("%H:%M:%S")
                log = (
                    f"[{t}] header7/BCM4(D5):{pin7} | "
                    f"header8/BCM14(D7):{pin8}"
                )
                with open(LOG_FILE, "a", encoding="utf-8") as f:
                    f.write(log + "\n")
                print(log)
                last_pin7 = pin7
                last_pin8 = pin8

            time.sleep(POLL_S)

    except OSError as e:
        print(f"Error (lgpio/archivo): {e}")
    except Exception as e:
        print(f"Error: {e}")
    finally:
        if h is not None:
            lgpio.gpiochip_close(h)


if __name__ == "__main__":
    main()
