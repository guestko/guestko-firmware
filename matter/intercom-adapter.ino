/**
 * Intercom Smart Adapter (Tuya / Matter)
 *
 * Detects intercom ring via galvanically isolated PC817 optocoupler,
 * reports events to Matter (Smart Life / Tuya), opens door via PhotoMOS
 * on command from Tuya automations.
 *
 * Always-on Wi-Fi + external PSU. No RTC / DS3231 — scheduling in Tuya app.
 *
 * See: matter/adapter technical specification
 */

#include <Arduino.h>
#include <Matter.h>
#include <U8g2lib.h>
#include <WiFi.h>

// --- GPIO (ESP32-C3 0.42" OLED board) ---
#define PIN_RING_INPUT 4   // PC817 output, active LOW (INPUT_PULLUP)
#define PIN_DOOR_OUTPUT 2  // PhotoMOS TLP172A (GPIO5 = OLED SDA)

#define OLED_SDA 5
#define OLED_SCL 6

// --- Ring validation (noise filter, on-device only) ---
const uint32_t MIN_RING_MS = 1000;
const uint32_t MAX_RING_MS = 3000;

// --- Door latch pulse ---
const uint32_t DOOR_PULSE_MS = 2000;

// --- Display ---
U8G2_SSD1306_72X40_ER_F_SW_I2C u8g2(U8G2_R0, OLED_SCL, OLED_SDA, U8X8_PIN_NONE);

enum UiState {
  UI_READY,
  UI_RINGING,
  UI_SENT,
  UI_DOOR_OPEN,
  UI_PAIRING,
  UI_NO_WIFI
};

UiState uiState = UI_READY;
unsigned long uiSentUntil = 0;
unsigned long ringStartMs = 0;
bool ringActive = false;

unsigned long doorOnMs = 0;
bool doorActive = false;

unsigned long lastDisplayUpdate = 0;
const unsigned long DISPLAY_INTERVAL_MS = 200;

// --- Matter endpoints ---
MatterContactSensor ringSensor;
MatterOnOffPlugin doorSwitch;

void updateDisplay() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);

  switch (uiState) {
  case UI_PAIRING:
    u8g2.drawStr(0, 12, "PAIRING...");
    u8g2.drawStr(0, 25, "Smart Life");
    u8g2.drawStr(0, 38, "Scan QR");
    break;

  case UI_NO_WIFI:
    u8g2.drawStr(0, 12, "NO WiFi");
    u8g2.drawStr(0, 25, "Check router");
    break;

  case UI_RINGING: {
    u8g2.drawStr(0, 12, ">>> RING <<<");
    uint32_t dur = millis() - ringStartMs;
    char buf[16];
    snprintf(buf, sizeof(buf), "%lu.%lus", dur / 1000, (dur / 100) % 10);
    u8g2.drawStr(0, 28, buf);
    u8g2.drawStr(0, 38, WiFi.status() == WL_CONNECTED ? "WiFi OK" : "NO WiFi");
    break;
  }

  case UI_SENT:
    u8g2.drawStr(0, 12, "EVENT SENT");
    u8g2.drawStr(0, 28, "-> Tuya");
    u8g2.drawStr(0, 38, WiFi.status() == WL_CONNECTED ? "WiFi OK" : "NO WiFi");
    break;

  case UI_DOOR_OPEN: {
    u8g2.drawStr(0, 12, "DOOR OPEN");
    uint32_t elapsed = millis() - doorOnMs;
    int bar = map(elapsed, 0, DOOR_PULSE_MS, 0, 56);
    if (bar > 56) bar = 56;
    u8g2.drawFrame(0, 24, 58, 8);
    u8g2.drawBox(1, 25, bar, 6);
    break;
  }

  case UI_READY:
  default:
    u8g2.drawStr(0, 12, "READY");
    u8g2.drawStr(0, 25, Matter.isDeviceCommissioned() ? "TUYA OK" : "NOT PAIRED");
    u8g2.drawStr(0, 38, WiFi.status() == WL_CONNECTED ? "WiFi OK" : "NO WiFi");
    break;
  }

  u8g2.sendBuffer();
}

void reportRingEvent() {
  ringSensor.setContact(true);
  ringSensor.updateAccessory();
  delay(100);
  ringSensor.setContact(false);
  ringSensor.updateAccessory();

  uiState = UI_SENT;
  uiSentUntil = millis() + 2000;
  Serial.println("[RING] Valid call reported to Matter");
}

bool onDoorChange(bool state) {
  if (state) {
    digitalWrite(PIN_DOOR_OUTPUT, HIGH);
    doorActive = true;
    doorOnMs = millis();
    uiState = UI_DOOR_OPEN;
    Serial.println("[DOOR] PhotoMOS ON");
  } else {
    digitalWrite(PIN_DOOR_OUTPUT, LOW);
    doorActive = false;
    uiState = UI_READY;
    Serial.println("[DOOR] PhotoMOS OFF");
  }
  updateDisplay();
  return true;
}

void processRingInput() {
  bool signalLow = (digitalRead(PIN_RING_INPUT) == LOW);

  if (signalLow) {
    if (!ringActive) {
      ringActive = true;
      ringStartMs = millis();
      uiState = UI_RINGING;
      Serial.println("[RING] Signal detected");
    }
  } else if (ringActive) {
    ringActive = false;
    uint32_t duration = millis() - ringStartMs;

    Serial.printf("[RING] Ended, duration: %u ms\n", duration);

    if (duration >= MIN_RING_MS && duration <= MAX_RING_MS) {
      reportRingEvent();
    } else {
      Serial.println("[RING] Discarded (noise / stuck button)");
      uiState = UI_READY;
    }
  }
}

void updateDoorPulse() {
  if (doorActive && (millis() - doorOnMs >= DOOR_PULSE_MS)) {
    digitalWrite(PIN_DOOR_OUTPUT, LOW);
    doorActive = false;
    doorSwitch.setOnOff(false);
    uiState = UI_READY;
    Serial.println("[DOOR] Pulse complete");
  }
}

void updateUiTimeouts() {
  if (uiState == UI_SENT && millis() >= uiSentUntil) {
    uiState = UI_READY;
  }

  if (WiFi.status() != WL_CONNECTED && Matter.isDeviceCommissioned()) {
    if (uiState != UI_RINGING && uiState != UI_DOOR_OPEN) {
      uiState = UI_NO_WIFI;
    }
  } else if (uiState == UI_NO_WIFI) {
    uiState = UI_READY;
  }
}

void setup() {
  Serial.begin(115200);

  pinMode(PIN_RING_INPUT, INPUT_PULLUP);
  pinMode(PIN_DOOR_OUTPUT, OUTPUT);
  digitalWrite(PIN_DOOR_OUTPUT, LOW);

  {
    U8G2_SSD1306_128X64_NONAME_F_SW_I2C clearer(U8G2_R0, OLED_SCL, OLED_SDA,
                                                  U8X8_PIN_NONE);
    clearer.begin();
    clearer.clearBuffer();
    clearer.sendBuffer();
  }

  u8g2.begin();
  u8g2.setContrast(255);
  updateDisplay();

  ringSensor.begin(false);
  doorSwitch.begin(false);
  doorSwitch.onChange(onDoorChange);

  Matter.begin();

  if (Matter.isDeviceCommissioned()) {
    ringSensor.updateAccessory();
    doorSwitch.updateAccessory();
    Serial.println("Matter commissioned — ready.");
  } else {
    uiState = UI_PAIRING;
    updateDisplay();
    Serial.println("Waiting for Matter commissioning (Smart Life)...");
  }
}

void loop() {
  if (!Matter.isDeviceCommissioned()) {
    uiState = UI_PAIRING;
    if (millis() - lastDisplayUpdate >= DISPLAY_INTERVAL_MS) {
      lastDisplayUpdate = millis();
      updateDisplay();
    }
    delay(100);
    return;
  }

  processRingInput();
  updateDoorPulse();
  updateUiTimeouts();

  if (millis() - lastDisplayUpdate >= DISPLAY_INTERVAL_MS) {
    lastDisplayUpdate = millis();
    updateDisplay();
  }

  delay(10);
}
