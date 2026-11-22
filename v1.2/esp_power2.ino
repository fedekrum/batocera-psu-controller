/*
 * esp_power2.ino - Clon idéntico de la lógica Teensy con soporte para Switch Permanente
 */

#include <Arduino.h>

// --- CONFIGURACIÓN (TWEAKABLE) ---
#define MOMENTARY_BUTTON false   // Cambiar a true si usas un botón momentáneo

static const uint32_t HOLD_TO_ON_MS = 500; 
static const uint32_t FORCED_OFF_MS = 6000; 
static const uint32_t SHUTDOWN_PULSE_MS = 2000; 
static const uint32_t RPI_BOOT_GRACE_MS = 10000; 
static const uint32_t CONFIRM_DEATH_MS = 3000; 
static const uint32_t POST_SHUTDOWN_DELAY_MS = 5000; 

// PINES LOLIN V3
static const uint8_t PIN_PSU_ON = D2;       
static const uint8_t PIN_BUTTON = D1;       
static const uint8_t PIN_RPI_SHUTDOWN = D5; 
static const uint8_t PIN_RPI_ALIVE = D7;    
static const uint8_t PIN_LED = D4;          

// --- VARIABLES ---
bool psuState = false;
bool rpiHasBooted = false;
uint32_t lastPowerOnMs = 0;
uint32_t lastAliveTimeMs = 0;
uint32_t lastHeartbeatLowMs = 0;
bool lastPhysicalButtonState = HIGH; // Para detectar cambios en switch permanente

// Helpers
bool isRPiAlive() { return digitalRead(PIN_RPI_ALIVE) == HIGH; }
bool isPsuOn() { return psuState; }
bool hasPassedBootGrace() { return (millis() - lastPowerOnMs) > RPI_BOOT_GRACE_MS; }

void setPsu(bool on) {
  psuState = on;
  digitalWrite(PIN_PSU_ON, on ? HIGH : LOW);
  if (!on) {
    rpiHasBooted = false;
    // Delay de seguridad para evitar re-encendidos por rebote
  }
}

void startLEDSequence() {
  for(int i=0; i<3; i++) {
    digitalWrite(PIN_LED, LOW); delay(100); 
    digitalWrite(PIN_LED, HIGH); delay(100);
  }
}

// OBJETO BUTTON (LÓGICA TEENSY)
struct Button {
  struct TimeEvent { uint32_t ms; uint8_t message; };
  const uint8_t pin;
  bool state; 
  bool lastReading;
  uint32_t lastChangeTime;
  uint32_t pressStartTime;
  TimeEvent events[4];
  uint8_t eventCount;
  uint8_t currentMessage;

  Button(uint8_t p) : pin(p) {
    state = false; lastReading = HIGH; lastChangeTime = 0;
    pressStartTime = 0; eventCount = 0; currentMessage = 0;
  }

  void begin() { pinMode(pin, INPUT_PULLUP); }

  void setTimeAndMessage(uint32_t ms, uint8_t msg) {
    if (eventCount >= 4) return;
    int i = eventCount - 1;
    while (i >= 0 && events[i].ms > ms) { events[i + 1] = events[i]; i--; }
    events[i + 1] = {ms, msg};
    eventCount++;
  }

  void update() {
    bool reading = digitalRead(pin);
    if (reading != lastReading) { lastChangeTime = millis(); }
    lastReading = reading;
    if ((millis() - lastChangeTime) > 20) {
      bool stableState = (reading == LOW); 
      if (stableState != state) {
        state = stableState;
        if (state == true) { pressStartTime = millis(); currentMessage = 0; }
        else { currentMessage = getMessageForDuration(millis() - pressStartTime); }
      }
    }
  }

  bool isPressed() { return state; }
  uint8_t getMessage() { uint8_t msg = currentMessage; currentMessage = 0; return msg; }
  uint8_t getMessageForDuration(uint32_t duration) {
    for (int i = eventCount - 1; i >= 0; i--) { if (duration >= events[i].ms) return events[i].message; }
    return 0;
  }
};

Button powerBtn(PIN_BUTTON);

// -----------------------------------------------------------------------------

void setup() {
  pinMode(PIN_PSU_ON, OUTPUT);
  pinMode(PIN_RPI_SHUTDOWN, OUTPUT);
  pinMode(PIN_RPI_ALIVE, INPUT); 
  pinMode(PIN_LED, OUTPUT);

  digitalWrite(PIN_RPI_SHUTDOWN, LOW);
  
  // INICIO SEGURO: Si el switch permanente está en ON al arrancar, prendemos sin dudar
  powerBtn.begin();
  if (!MOMENTARY_BUTTON && digitalRead(PIN_BUTTON) == LOW) {
      setPsu(true);
      lastPowerOnMs = millis();
  } else {
      setPsu(false);
  }

  powerBtn.setTimeAndMessage(HOLD_TO_ON_MS, 1);
  powerBtn.setTimeAndMessage(FORCED_OFF_MS, 2);

  startLEDSequence();
}

void loop() {
  uint32_t now = millis();
  bool rpiAlive = isRPiAlive();

  if (MOMENTARY_BUTTON) {
      // --- LÓGICA BOTÓN MOMENTÁNEO (TEENSY ORIGINAL) ---
      powerBtn.update();
      uint8_t msg = powerBtn.getMessage();
      if (msg > 0) {
        if (msg == 1) {
          if (!isPsuOn()) { setPsu(true); lastPowerOnMs = now; rpiHasBooted = false; }
          else if (hasPassedBootGrace()) {
              digitalWrite(PIN_RPI_SHUTDOWN, HIGH); delay(SHUTDOWN_PULSE_MS); digitalWrite(PIN_RPI_SHUTDOWN, LOW);
          }
        } else if (msg == 2 && isPsuOn()) setPsu(false);
      }
  } else {
      // --- LÓGICA SWITCH PERMANENTE ---
      bool currentPhysical = digitalRead(PIN_BUTTON);
      if (currentPhysical != lastPhysicalButtonState) {
          delay(50); // Debounce físico
          if (digitalRead(PIN_BUTTON) == currentPhysical) {
              if (currentPhysical == LOW) { // Switch movido a ON
                  if (!isPsuOn()) { setPsu(true); lastPowerOnMs = now; rpiHasBooted = false; }
              } else { // Switch movido a OFF
                  if (isPsuOn()) {
                      if (hasPassedBootGrace()) {
                          digitalWrite(PIN_RPI_SHUTDOWN, HIGH); delay(SHUTDOWN_PULSE_MS); digitalWrite(PIN_RPI_SHUTDOWN, LOW);
                      } else { setPsu(false); }
                  }
              }
              lastPhysicalButtonState = currentPhysical;
          }
      }
  }

  // --- MONITOREO COMÚN (Misión Critica) ---
  if (isPsuOn()) {
    if (!rpiHasBooted) {
      if (rpiAlive) {
        if (now - lastHeartbeatLowMs > 5000) { rpiHasBooted = true; lastAliveTimeMs = now; }
      } else { lastHeartbeatLowMs = now; }
      if (now - lastPowerOnMs > 60000) setPsu(false); // Timeout 60s
    } else {
      if (rpiAlive) lastAliveTimeMs = now;
      else if (now - lastAliveTimeMs > 15000) setPsu(false); // Confirmación muerte
    }
  }

  // Feedback LED (LOW es ON)
  digitalWrite(PIN_LED, isPsuOn() ? LOW : HIGH);
  delay(20);
}
