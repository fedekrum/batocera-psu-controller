/*
 * esp_power.ino - VERSIÓN FINAL HÍBRIDA
 * - MOMENTARY_BUTTON true: Lógica completa Teensy (Seguridad y Forzado activo).
 * - MOMENTARY_BUTTON false: Lógica Manual Pura (Solo switch ON/OFF, sin
 * timeouts).
 */
#include <Arduino.h>

// --- CONFIGURACIÓN ---
#define MOMENTARY_BUTTON false

// Tiempos (Milisegundos)
static const uint32_t HOLD_TO_ON_MS = 500;
static const uint32_t FORCED_OFF_MS = 6000;
static const uint32_t SHUTDOWN_PULSE_MS = 2000;
static const uint32_t RPI_BOOT_GRACE_MS = 10000;

// PINES LOLIN V3
static const uint8_t PIN_BUTTON = D1;
static const uint8_t PIN_PSU_ON = D2;
static const uint8_t PIN_RPI_SHUTDOWN = D5;
static const uint8_t PIN_RPI_ALIVE = D7;
static const uint8_t PIN_LED = D4;

// --- VARIABLES DE ESTADO ---
bool psuState = false;
bool rpiHasBooted = false;
uint32_t lastPowerOnMs = 0;
uint32_t lastAliveTimeMs = 0;
uint32_t lastHeartbeatLowMs = 0;
bool lastPhysicalButtonState = HIGH;

// --- GESTIÓN DE LED (Blinks) ---
uint8_t ledStatusCode = 0; // 0=Off, 1=Boot, 2=Error, 3=Shutdown, 255=Solid
uint32_t lastLedToggleMs = 0;
uint8_t currentBlinkCount = 0;
bool ledPinState = HIGH; // HIGH = LED OFF (Lolin v1.3)

void setLedCode(uint8_t code) {
  if (ledStatusCode != code) {
    ledStatusCode = code;
    currentBlinkCount = 0;
    lastLedToggleMs = millis();
  }
}

void updateLed() {
  uint32_t now = millis();
  
  // Caso 0: PSU OFF
  if (!psuState) {
    digitalWrite(PIN_LED, HIGH); // Apagado
    return;
  }

  // Caso 255: Sólido (Todo OK)
  if (ledStatusCode == 255) {
    digitalWrite(PIN_LED, LOW); // Encendido (LOW es ON en Lolin)
    return;
  }

  // Lógica de parpadeo (parpadeos cortos de 200ms)
  uint32_t elapsed = now - lastLedToggleMs;

  if (currentBlinkCount < (ledStatusCode * 2)) {
    // Estamos en la fase de ráfaga de pulsos
    if (elapsed >= 200) {
      ledPinState = !ledPinState;
      digitalWrite(PIN_LED, ledPinState);
      currentBlinkCount++;
      lastLedToggleMs = now;
    }
  } else {
    // Estamos en la fase de pausa (1 segundo)
    digitalWrite(PIN_LED, HIGH); // Aseguramos apagado en pausa
    if (elapsed >= 1000) {
      currentBlinkCount = 0;
      lastLedToggleMs = now;
    }
  }
}

// Helpers
bool isRPiAlive() { return digitalRead(PIN_RPI_ALIVE) == HIGH; }
bool isPsuOn() { return psuState; }
bool hasPassedBootGrace() {
  return (millis() - lastPowerOnMs) > RPI_BOOT_GRACE_MS;
}

void setPsu(bool on) {
  psuState = on;
  digitalWrite(PIN_PSU_ON, on ? HIGH : LOW);
  if (!on) {
    rpiHasBooted = false;
    setLedCode(0);
  } else {
    setLedCode(1); // Empezamos booteando
  }
}

// OBJETO BUTTON (Clase Teensy)
// ... (mismo código de la clase Button)
struct Button {
  struct TimeEvent {
    uint32_t ms;
    uint8_t message;
  };
  const uint8_t pin;
  bool state;
  bool lastReading;
  uint32_t lastChangeTime;
  uint32_t pressStartTime;
  TimeEvent events[4];
  uint8_t eventCount;
  uint8_t currentMessage;

  Button(uint8_t p) : pin(p) {
    state = false;
    lastReading = HIGH;
    lastChangeTime = 0;
    pressStartTime = 0;
    eventCount = 0;
    currentMessage = 0;
  }
  void begin() { pinMode(pin, INPUT_PULLUP); }
  void setTimeAndMessage(uint32_t ms, uint8_t msg) {
    if (eventCount >= 4)
      return;
    int i = eventCount - 1;
    while (i >= 0 && events[i].ms > ms) {
      events[i + 1] = events[i];
      i--;
    }
    events[i + 1] = {ms, msg};
    eventCount++;
  }
  void update() {
    bool reading = digitalRead(pin);
    if (reading != lastReading) {
      lastChangeTime = millis();
    }
    lastReading = reading;
    if ((millis() - lastChangeTime) > 20) {
      bool stableState = (reading == LOW);
      if (stableState != state) {
        state = stableState;
        if (state == true) {
          pressStartTime = millis();
          currentMessage = 0;
        } else {
          currentMessage = getMessageForDuration(millis() - pressStartTime);
        }
      }
    }
  }
  bool isPressed() { return state; }
  uint8_t getMessage() {
    uint8_t msg = currentMessage;
    currentMessage = 0;
    return msg;
  }
  uint8_t getMessageForDuration(uint32_t duration) {
    for (int i = eventCount - 1; i >= 0; i--) {
      if (duration >= events[i].ms)
        return events[i].message;
    }
    return 0;
  }
};

Button powerBtn(PIN_BUTTON);

void setup() {
  pinMode(PIN_BUTTON, INPUT_PULLUP);
  delay(100);

  pinMode(PIN_PSU_ON, OUTPUT);
  pinMode(PIN_LED, OUTPUT);
  digitalWrite(PIN_LED, HIGH); // Apagado al inicio

  if (!MOMENTARY_BUTTON && digitalRead(PIN_BUTTON) == LOW) {
    psuState = true;
    digitalWrite(PIN_PSU_ON, HIGH);
    lastPowerOnMs = millis();
    lastPhysicalButtonState = LOW;
    setLedCode(1); // Booteando
  } else {
    psuState = false;
    digitalWrite(PIN_PSU_ON, LOW);
    lastPhysicalButtonState = HIGH;
    setLedCode(0);
  }

  pinMode(PIN_RPI_SHUTDOWN, OUTPUT);
  digitalWrite(PIN_RPI_SHUTDOWN, LOW);
  pinMode(PIN_RPI_ALIVE, INPUT);

  powerBtn.begin();
  powerBtn.setTimeAndMessage(HOLD_TO_ON_MS, 1);
  powerBtn.setTimeAndMessage(FORCED_OFF_MS, 2);
}

void loop() {
  uint32_t now = millis();
  bool rpiAlive = isRPiAlive();

  if (MOMENTARY_BUTTON) {
    // --- MODO BOTÓN MOMENTÁNEO (Lógica Teensy Completa) ---
    powerBtn.update();
    uint8_t msg = powerBtn.getMessage();
    if (msg > 0) {
      if (msg == 1) {
        if (!isPsuOn()) {
          setPsu(true);
          lastPowerOnMs = now;
        } else if (hasPassedBootGrace()) {
          setLedCode(3); // Apagando
          digitalWrite(PIN_RPI_SHUTDOWN, HIGH);
          delay(SHUTDOWN_PULSE_MS);
          digitalWrite(PIN_RPI_SHUTDOWN, LOW);
        }
      } else if (msg == 2 && isPsuOn())
        setPsu(false); 
    }

    if (isPsuOn()) {
      if (rpiAlive) {
        rpiHasBooted = true;
        lastAliveTimeMs = now;
        setLedCode(255); // Sólido = OK
      }
      if (rpiHasBooted && (now - lastAliveTimeMs > 15000)) {
        setLedCode(2); // Error de vida
        setPsu(false);
      }
      if (!rpiHasBooted && (now - lastPowerOnMs > 60000)) {
        setLedCode(2);
        setPsu(false);
      }
    }
  } else {
    // --- MODO SWITCH PERMANENTE (Lógica Manual Pura) ---
    bool currentPhysical = digitalRead(PIN_BUTTON);
    if (currentPhysical != lastPhysicalButtonState) {
      delay(50);
      if (digitalRead(PIN_BUTTON) == currentPhysical) {
        if (currentPhysical == LOW) { // Switch ON
          if (!isPsuOn()) {
            setPsu(true);
            lastPowerOnMs = now;
          }
        } else { // Switch OFF
          if (isPsuOn()) {
            if (hasPassedBootGrace()) {
              setLedCode(3); // Apagando
              digitalWrite(PIN_RPI_SHUTDOWN, HIGH);
              delay(SHUTDOWN_PULSE_MS);
              digitalWrite(PIN_RPI_SHUTDOWN, LOW);
            } else {
              setPsu(false);
            } 
          }
        }
        lastPhysicalButtonState = currentPhysical;
      }
    }

    // Monitoreo para Switch
    if (isPsuOn()) {
      if (rpiAlive) {
        rpiHasBooted = true;
        lastAliveTimeMs = now;
        setLedCode(255); // Sólido = OK
      } else if (rpiHasBooted) {
        // Si booteó y desapareció la vida (tras el pulso o crash)
        if (now - lastAliveTimeMs > 4000) {
          setPsu(false);
        }
      }
    }
  }

  updateLed(); // Gestión de parpadeos no bloqueante
  delay(1);
}
