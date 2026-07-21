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
#include <Wire.h>

// --- GPIO (ESP32-C3 0.42" OLED board) ---
#define PIN_RING_INPUT 4   // PC817 output, active LOW (INPUT_PULLUP)
#define PIN_DOOR_OUTPUT 2  // PhotoMOS TLP172A (or bench load); HIGH = active
#define PIN_STATUS_LED 8   // Onboard blue LED, active LOW
#define PIN_BOOT_BUTTON 9  // Hold 5 s to decommission Matter

#define OLED_SDA 5
#define OLED_SCL 6

// --- Ring validation (noise filter, on-device only) ---
const uint32_t MIN_RING_MS = 1000;
const uint32_t MAX_RING_MS = 3000;

// --- Door latch pulse ---
const uint32_t DOOR_PULSE_MS = 2000;

// --- Status LED (blink on valid ring, solid on door open) ---
const uint32_t LED_BLINK_INTERVAL_MS = 150;
const uint8_t RING_BLINK_COUNT = 3;

// --- BOOT button decommission ---
const uint32_t DECOMMISSION_MS = 5000;
unsigned long bootPressMs = 0;
bool bootPressed = false;

// --- Display (SSD1306 72x40, hardware I2C — same as matter-esp32.ino) ---
U8G2_SSD1306_72X40_ER_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

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
bool doorUiDirty = false;
bool ringReportPending = false;
unsigned long ringReportMs = 0;

bool ledBlinkActive = false;
uint8_t ledBlinksLeft = 0;
bool ledBlinkOn = false;
unsigned long ledBlinkMs = 0;

unsigned long lastDisplayUpdate = 0;
const unsigned long DISPLAY_INTERVAL_MS = 200;

unsigned long lastWifiDebugMs = 0;
const unsigned long WIFI_DEBUG_INTERVAL_MS = 10000;
wl_status_t lastWifiStatus = WL_NO_SHIELD;
int lastWifiDisconnectReason = -1;

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
    u8g2.drawStr(0, 25, "BOOT 5s=reset");
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

void startRingBlink() {
  if (doorActive) {
    return;
  }
  ledBlinkActive = true;
  ledBlinksLeft = RING_BLINK_COUNT;
  ledBlinkOn = true;
  ledBlinkMs = millis();
  digitalWrite(PIN_STATUS_LED, LOW);
}

void updateStatusLed() {
  if (doorActive) {
    digitalWrite(PIN_STATUS_LED, LOW);
    return;
  }

  if (!ledBlinkActive) {
    digitalWrite(PIN_STATUS_LED, HIGH);
    return;
  }

  if (millis() - ledBlinkMs < LED_BLINK_INTERVAL_MS) {
    return;
  }

  ledBlinkMs = millis();
  ledBlinkOn = !ledBlinkOn;
  digitalWrite(PIN_STATUS_LED, ledBlinkOn ? LOW : HIGH);

  if (!ledBlinkOn) {
    ledBlinksLeft--;
    if (ledBlinksLeft == 0) {
      ledBlinkActive = false;
    }
  }
}

void reportRingEvent() {
  if (Matter.isDeviceCommissioned()) {
    ringSensor.setContact(false);
    ringReportPending = true;
    ringReportMs = millis();
    Serial.println("[RING] Valid call — reporting to Matter");
  } else {
    Serial.println("[RING] Valid call (local only — not paired yet)");
    startRingBlink();
    uiState = UI_SENT;
    uiSentUntil = millis() + 2000;
    doorUiDirty = true;
  }
}

void updateRingReport() {
  if (!ringReportPending) {
    return;
  }
  if (millis() - ringReportMs < 100) {
    return;
  }
  ringSensor.setContact(true);
  ringReportPending = false;
  Serial.println("[RING] Matter contact restored");

  startRingBlink();
  uiState = UI_SENT;
  uiSentUntil = millis() + 2000;
  doorUiDirty = true;
}

bool onDoorChange(bool state) {
  // Called from Matter task — no I2C/Serial/display here.
  if (state) {
    ledBlinkActive = false;
    digitalWrite(PIN_DOOR_OUTPUT, HIGH);
    doorActive = true;
    doorOnMs = millis();
    uiState = UI_DOOR_OPEN;
    digitalWrite(PIN_STATUS_LED, LOW);
  } else {
    digitalWrite(PIN_DOOR_OUTPUT, LOW);
    doorActive = false;
    uiState = UI_READY;
    digitalWrite(PIN_STATUS_LED, HIGH);
  }
  doorUiDirty = true;
  return true;
}

void finishDoorPulse() {
  digitalWrite(PIN_DOOR_OUTPUT, LOW);
  doorActive = false;
  uiState = UI_READY;
  digitalWrite(PIN_STATUS_LED, HIGH);
  doorUiDirty = true;

  if (doorSwitch.setOnOff(false)) {
    Serial.println("[DOOR] Pulse complete — Matter synced OFF");
  } else {
    Serial.println("[DOOR] Pulse complete — Matter sync OFF failed");
  }
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
    finishDoorPulse();
  }
}

void checkDecommissionButton() {
  if (digitalRead(PIN_BOOT_BUTTON) == LOW) {
    if (!bootPressed) {
      bootPressed = true;
      bootPressMs = millis();
    } else if (millis() - bootPressMs >= DECOMMISSION_MS) {
      Serial.println("[MATTER] Decommissioning — hold released, device will reboot...");
      doorSwitch.setOnOff(false);
      digitalWrite(PIN_DOOR_OUTPUT, LOW);
      Matter.decommission();
      bootPressMs = millis();  // avoid repeat until reboot
    }
  } else {
    bootPressed = false;
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

const char *wifiStatusName(wl_status_t status) {
  switch (status) {
  case WL_NO_SHIELD: return "NO_SHIELD";
  case WL_IDLE_STATUS: return "IDLE";
  case WL_NO_SSID_AVAIL: return "NO_SSID_AVAIL";
  case WL_SCAN_COMPLETED: return "SCAN_COMPLETED";
  case WL_CONNECTED: return "CONNECTED";
  case WL_CONNECT_FAILED: return "CONNECT_FAILED";
  case WL_CONNECTION_LOST: return "CONNECTION_LOST";
  case WL_DISCONNECTED: return "DISCONNECTED";
  default: return "UNKNOWN";
  }
}

const char *wifiEventName(WiFiEvent_t event) {
  switch (event) {
  case ARDUINO_EVENT_WIFI_READY: return "WIFI_READY";
  case ARDUINO_EVENT_WIFI_STA_START: return "STA_START";
  case ARDUINO_EVENT_WIFI_STA_STOP: return "STA_STOP";
  case ARDUINO_EVENT_WIFI_STA_CONNECTED: return "STA_CONNECTED";
  case ARDUINO_EVENT_WIFI_STA_DISCONNECTED: return "STA_DISCONNECTED";
  case ARDUINO_EVENT_WIFI_STA_AUTHMODE_CHANGE: return "AUTHMODE_CHANGE";
  case ARDUINO_EVENT_WIFI_STA_GOT_IP: return "GOT_IP";
  case ARDUINO_EVENT_WIFI_STA_LOST_IP: return "LOST_IP";
  default: return "OTHER";
  }
}

void onWifiEvent(WiFiEvent_t event, WiFiEventInfo_t info) {
  Serial.printf("[WIFI EVT] %s (%d)\n", wifiEventName(event), event);

  if (event == ARDUINO_EVENT_WIFI_STA_DISCONNECTED) {
    lastWifiDisconnectReason = info.wifi_sta_disconnected.reason;
    Serial.printf("[WIFI EVT] disconnect reason=%d\n", lastWifiDisconnectReason);
  }
}

void printWifiDiagnostics(const char *tag) {
  wl_status_t status = WiFi.status();

  Serial.println();
  Serial.printf("=== WiFi debug [%s] ===\n", tag);
  Serial.printf("Matter commissioned: %s\n",
                Matter.isDeviceCommissioned() ? "YES" : "NO");
  Serial.printf("WiFi status: %s (%d)\n", wifiStatusName(status), status);
  Serial.printf("WiFi mode: %d\n", WiFi.getMode());

  if (status == WL_CONNECTED) {
    Serial.printf("SSID: %s\n", WiFi.SSID().c_str());
    Serial.printf("IP: %s\n", WiFi.localIP().toString().c_str());
    Serial.printf("Gateway: %s\n", WiFi.gatewayIP().toString().c_str());
    Serial.printf("RSSI: %d dBm\n", WiFi.RSSI());
    Serial.printf("Channel: %d\n", WiFi.channel());
  } else {
    Serial.println("SSID: (not connected)");
    if (lastWifiDisconnectReason >= 0) {
      Serial.printf("Last disconnect reason: %d\n", lastWifiDisconnectReason);
    }
  }

  Serial.printf("GPIO4 ring input: %s\n",
                digitalRead(PIN_RING_INPUT) == LOW ? "LOW (active)" : "HIGH (idle)");
  Serial.printf("Free heap: %u bytes\n", ESP.getFreeHeap());
  Serial.println("========================");
  Serial.println();
}

void updateWifiDebug() {
  wl_status_t status = WiFi.status();

  if (status != lastWifiStatus) {
    Serial.printf("[WIFI] status change: %s -> %s\n",
                  wifiStatusName(lastWifiStatus), wifiStatusName(status));
    lastWifiStatus = status;
    printWifiDiagnostics("status-change");
  }

  unsigned long now = millis();
  if (now - lastWifiDebugMs >= WIFI_DEBUG_INTERVAL_MS) {
    lastWifiDebugMs = now;
    printWifiDiagnostics("periodic");
  }
}

void printCommissioningInfo() {
  Serial.println();
  Serial.println("=== Matter commissioning ===");
  Serial.println("Smart Life: Add device -> Matter -> scan QR or enter code");
  Serial.printf("Manual code: %s\n", Matter.getManualPairingCode().c_str());
  Serial.printf("QR URL: %s\n", Matter.getOnboardingQRCodeUrl().c_str());
  Serial.println("Open QR URL in a browser to display the code.");
  Serial.println("============================");
  Serial.println();
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println();
  Serial.println(">>> Intercom Adapter (WiFi debug build) <<<");

  WiFi.onEvent(onWifiEvent);
  WiFi.setAutoReconnect(true);
  lastWifiStatus = WiFi.status();

  pinMode(PIN_RING_INPUT, INPUT_PULLUP);
  pinMode(PIN_DOOR_OUTPUT, OUTPUT);
  digitalWrite(PIN_DOOR_OUTPUT, LOW);

  pinMode(PIN_STATUS_LED, OUTPUT);
  digitalWrite(PIN_STATUS_LED, HIGH);

  pinMode(PIN_BOOT_BUTTON, INPUT_PULLUP);

  Wire.begin(OLED_SDA, OLED_SCL);
  u8g2.begin();
  u8g2.setContrast(255);
  u8g2.setPowerSave(0);
  updateDisplay();
  Serial.println("[OLED] init OK (72x40 HW I2C, SDA=5 SCL=6)");

  ringSensor.begin(true);  // idle = closed (no ring)
  doorSwitch.begin(false);
  doorSwitch.onChange(onDoorChange);

  Matter.begin();

  if (Matter.isDeviceCommissioned()) {
    // Reset stale ON state saved in Matter NVS (pulse ends with OFF sync).
    digitalWrite(PIN_DOOR_OUTPUT, LOW);
    doorActive = false;
    doorSwitch.setOnOff(false);
    uiState = UI_READY;
    doorUiDirty = true;
    Serial.println("Matter commissioned — ready.");
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("[WIFI] commissioned but not connected — hold BOOT 5s to reset pairing");
    }
  } else {
    uiState = UI_PAIRING;
    updateDisplay();
    printCommissioningInfo();
  }

  printWifiDiagnostics("boot");
}

void loop() {
  processRingInput();
  updateRingReport();
  updateDoorPulse();
  updateStatusLed();
  updateUiTimeouts();
  checkDecommissionButton();
  updateWifiDebug();

  if (doorUiDirty || millis() - lastDisplayUpdate >= DISPLAY_INTERVAL_MS) {
    lastDisplayUpdate = millis();
    doorUiDirty = false;
    updateDisplay();
  }

  if (!Matter.isDeviceCommissioned()) {
    if (ringActive) {
      uiState = UI_RINGING;
    } else if (uiState != UI_SENT && uiState != UI_DOOR_OPEN) {
      uiState = UI_PAIRING;
    }
    static unsigned long lastQrPrint = 0;
    if (lastQrPrint == 0 || millis() - lastQrPrint >= 30000) {
      printCommissioningInfo();
      lastQrPrint = millis();
    }
    delay(100);
    return;
  }

  delay(10);
}
