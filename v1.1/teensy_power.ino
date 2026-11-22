/*
 * Teensy Power controller for PC ATX PSU and Raspberry Pi 5
 */

#include <Arduino.h>

// -----------------------------------------------------------------------------
//  CONFIGURACIÓN Y AJUSTES (TWEAKABLE VARIABLES)
// -----------------------------------------------------------------------------

// Tiempos de Botón (Milisegundos)
static const uint32_t HOLD_TO_ON_MS =
    500; // Mantener presionado para ENCENDER/APAGAR (0.5s)
static const uint32_t FORCED_OFF_MS =
    6000; // Mantener para CORTE DE LUZ FORZADO (6s)

// Tiempos de Sistema
static const uint32_t SHUTDOWN_PULSE_MS =
    2000; // Duración del pulso de apagado a la RPi (2s)
static const uint32_t RPI_BOOT_GRACE_MS =
    10000; // Tiempo de gracia al arrancar antes de permitir apagar con botón
static const uint32_t CONFIRM_DEATH_MS =
    3000; // Espera para confirmar que RPi murió antes de cortar PSU
static const uint32_t POST_SHUTDOWN_DELAY_MS =
    5000; // Tiempo muerto tras apagar (ignora botones)

// Pines de Hardware
static const uint8_t PIN_PSU_ON = 2; // Salida: Transistor -> Fuente ATX (PS_ON)
static const uint8_t PIN_BUTTON = 3; // Entrada: Botón Momentáneo (INPUT_PULLUP)
static const uint8_t PIN_RPI_SHUTDOWN =
    4; // Salida: Señal a RPi (GPIO 17) -> "Apagate"
static const uint8_t PIN_RPI_ALIVE =
    5; // Entrada: Señal de RPi (GPIO 18) -> "Estoy viva"
static const uint8_t PIN_LED = LED_BUILTIN; // LED Interno (D13)

// -----------------------------------------------------------------------------

// Variables Globales (Sistema)
bool rpiHasBooted = false;
uint32_t lastPowerOnMs = 0;
uint32_t lastHeartbeatLowMs = 0; // Para filtrar ruido al bootear
uint32_t lastAliveTimeMs = 0;    // Para confirmar muerte segura

// -----------------------------------------------------------------------------

// Helpers
bool isRPiAlive() { return digitalRead(PIN_RPI_ALIVE) == HIGH; }

bool isPsuOn() { return digitalRead(PIN_PSU_ON) == HIGH; }

bool hasPassedBootGrace() {
  return (millis() - lastPowerOnMs) > RPI_BOOT_GRACE_MS;
}

void setPsu(bool on) {
  digitalWrite(PIN_PSU_ON, on ? HIGH : LOW);
  if (!on) {
    delay(POST_SHUTDOWN_DELAY_MS);
  }
}

void startLEDSequence() {
  digitalWrite(PIN_LED, HIGH);
  delay(100);
  digitalWrite(PIN_LED, LOW);
  delay(100);
  digitalWrite(PIN_LED, HIGH);
  delay(100);
  digitalWrite(PIN_LED, LOW);
}

// objeto que define que dice el boton
struct Button {
  // Estructura interna para guardar (tiempo -> mensaje)
  struct TimeEvent {
    uint32_t ms;
    uint8_t message;
  };

  const uint8_t pin;

  // Estado interno
  bool state; // true = APRETADO
  bool lastReading;
  uint32_t lastChangeTime;
  uint32_t pressStartTime;

  // Array fijo de eventos (max 4 por simplicidad)
  TimeEvent events[4];
  uint8_t eventCount;

  // Mensaje generado al soltar (0 = ninguno)
  uint8_t currentMessage;

  // Constructor
  Button(uint8_t p) : pin(p) {
    state = false;
    lastReading = HIGH;
    lastChangeTime = 0;
    pressStartTime = 0;
    eventCount = 0;
    currentMessage = 0;
  }

  // Configura hardware
  void begin() { pinMode(pin, INPUT_PULLUP); }

  // Agrega una definicion de tiempo -> mensaje.
  // Mantiene el array ordenado por tiempo (menor a mayor).
  void setTimeAndMessage(uint32_t ms, uint8_t msg) {
    if (eventCount >= 4)
      return; // Limite alcanzado

    // 1. Encontrar la posicion donde insertar
    int i = eventCount - 1;
    while (i >= 0 && events[i].ms > ms) {
      events[i + 1] = events[i]; // Mover elementos mayores a la derecha
      i--;
    }

    // 2. Insertar el nuevo evento
    events[i + 1] = {ms, msg};
    eventCount++;
  }

  // Actualiza estado (llamar en loop)
  void update() {
    bool reading = digitalRead(pin);

    // Debounce basico
    if (reading != lastReading) {
      lastChangeTime = millis();
    }
    lastReading = reading;

    if ((millis() - lastChangeTime) > 20) {
      // Lectura estable
      bool stableState = (reading == LOW); // LOW = Apretado

      if (stableState != state) {
        state = stableState;

        if (state == true) {
          // ACABA DE APRETARSE
          pressStartTime = millis();
          currentMessage = 0; // Limpiar mensaje anterior
        } else {
          // ACABA DE SOLTARSE - Calcular mensaje
          uint32_t duration = millis() - pressStartTime;
          currentMessage = getMessageForDuration(duration);
        }
      }
    }
  }

  // Devuelve true si el boton esta fisicamente apretado
  bool isPressed() { return state; }

  // Devuelve el mensaje generado (lo consume, vuelve a 0)
  uint8_t getMessage() {
    uint8_t msg = currentMessage;
    currentMessage = 0;
    return msg;
  }

  // Devuelve el mensaje correspondiente a la duracion pasada (uso interno)
  uint8_t getMessageForDuration(uint32_t duration) {
    // Recorremos de atras para adelante (del tiempo mas largo al mas corto)
    for (int i = eventCount - 1; i >= 0; i--) {
      if (duration >= events[i].ms) {
        return events[i].message;
      }
    }
    return 0; // 0 = Sin mensaje (cortito)
  }
};

// Instancia Global
Button powerBtn(PIN_BUTTON);

// -----------------------------------------------------------------------------

// Setup
void setup() {
  // Configuración de pines del sistema
  pinMode(PIN_PSU_ON, OUTPUT);
  pinMode(PIN_RPI_SHUTDOWN, OUTPUT);
  pinMode(PIN_RPI_ALIVE, INPUT_PULLDOWN);
  pinMode(PIN_LED, OUTPUT);

  // Estado inicial
  digitalWrite(PIN_RPI_SHUTDOWN, LOW);
  setPsu(false);

  // Configuración del Botón
  powerBtn.begin();

  // Definimos los mensajes:
  // 1 = ACCION NORMAL (500ms) -> Encender / Apagar suave
  // 2 = FORZADO (6000ms) -> Apagar a lo bruto
  powerBtn.setTimeAndMessage(HOLD_TO_ON_MS, 1);
  powerBtn.setTimeAndMessage(FORCED_OFF_MS, 2);

  // Feedback de arranque
  startLEDSequence();
}

// -----------------------------------------------------------------------------

// Loop
void loop() {
  uint32_t now = millis();
  bool rpiAlive = isRPiAlive();

  // 1. Actualizar botón y obtener mensaje
  powerBtn.update();
  uint8_t msg = powerBtn.getMessage();

  // 2. Procesar mensaje (si hay)
  if (msg > 0) {
    if (msg == 1) {
      // --- MENSAJE 1: TOGGLE ---
      if (!isPsuOn()) {
        // Encender
        setPsu(true);
        lastPowerOnMs = now;
        rpiHasBooted = false;
      } else {
        // Apagar suave (solo si ya pasó el tiempo de gracia)
        if (hasPassedBootGrace()) {
          digitalWrite(PIN_RPI_SHUTDOWN, HIGH);
          delay(SHUTDOWN_PULSE_MS);
          digitalWrite(PIN_RPI_SHUTDOWN, LOW);
        }
      }
    } else if (msg == 2) {
      // --- MENSAJE 2: FORZADO ---
      if (isPsuOn()) {
        setPsu(false);
      }
    }
  }

  // 3. Monitoreo automático de RPi (Misión Punto 3)
  if (isPsuOn()) {
    if (!rpiHasBooted) {
      // A. Confirmar arranque estable (Anti-Ruido: 5s de HIGH continuo)
      if (rpiAlive) {
        if (now - lastHeartbeatLowMs > 5000) {
          rpiHasBooted = true;
          lastAliveTimeMs = now;
        }
      } else {
        lastHeartbeatLowMs = now;
      }

      // B. Timeout de seguridad (30 segundos si nunca bootea)
      if (now - lastPowerOnMs > 30000) {
        setPsu(false);
      }
    } else {
      // C. Monitoreo de muerte (Confirmación: 15s de silencio total)
      if (rpiAlive) {
        lastAliveTimeMs = now;
      } else {
        if (now - lastAliveTimeMs > 15000) {
          setPsu(false);
        }
      }
    }
  }

  // 4. Feedback Visual LED
  if (powerBtn.isPressed()) {
    // Parpadeo rápido mientras está apretado
    if ((now / 100) % 2 == 0)
      digitalWrite(PIN_LED, HIGH);
    else
      digitalWrite(PIN_LED, LOW);
  } else {
    // LED refleja estado de la PSU
    digitalWrite(PIN_LED, isPsuOn() ? HIGH : LOW);
  }
}

// -----------------------------------------------------------------------------

// Main
#ifndef ARDUINO
int main() {
  setup();
  while (1) {
    loop();
  }
  return 0;
}
#endif
