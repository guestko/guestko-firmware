/**
 * Smart Intercom Controller v34.0 (Offline / Standalone)
 * * Logic:
 * 1. Monitors analog input for intercom ring signal (Peak Detector).
 * 2. If signal duration is valid (0.5s - 5.0s), triggers relay.
 * 3. Relay pulse is short (0.5s) to avoid line noise.
 * * Hardware Scheme (Peak Detector):
 * - Intercom CA -> Diode Anode -> Diode Cathode -> GPIO 3
 * - GPIO 3 -> Capacitor (+) -> Capacitor (-) -> GND
 * - GPIO 3 -> Resistor (10k) -> GND (Parallel to Capacitor)
 * - Intercom Pin 1 (Speaker) -> GND (Virtual Ground)
 */

#include <Arduino.h>

// ==========================================
// --- PINS CONFIGURATION ---
// ==========================================
const int PIN_SIGNAL_INPUT  = 3;   // Analog Input (from Peak Detector)
const int PIN_RELAY         = 10;  // Door Open Relay (Active High)

// ==========================================
// --- SETTINGS ---
// ==========================================
// ADC Threshold (0-4095). 1000 is approx 0.8V.
// Signal peak is ~2.8V, so 1000 is a safe margin.
const int SIGNAL_THRESHOLD  = 1000; 

// Signal Duration Logic (Anti-bounce)
const int MIN_PRESS_TIME    = 500;  // Min duration to trigger (0.5 sec)
const int MAX_PRESS_TIME    = 5000; // Max duration to trigger (5.0 sec)

// Relay Logic
const int RELAY_PULSE_MS    = 500;  // Relay click duration (0.5 sec)

// ==========================================
// --- VARIABLES ---
// ==========================================
// Relay Timer
unsigned long relayOnTime = 0;
bool isRelayActive = false;

// Signal State Machine
bool isReceivingSignal = false;
unsigned long signalStartTime = 0;

// ==========================================
// --- HARDWARE CONTROL ---
// ==========================================

void openDoor() {
  // Trigger the relay
  digitalWrite(PIN_RELAY, HIGH);
  isRelayActive = true;
  relayOnTime = millis();
  Serial.println("[ACTION] Relay ON (Opening Door)");
}

void updateRelay() {
  // Turn off relay after defined pulse duration
  if (isRelayActive && (millis() - relayOnTime > RELAY_PULSE_MS)) {
    digitalWrite(PIN_RELAY, LOW);
    isRelayActive = false;
    Serial.println("[ACTION] Relay OFF");
  }
}

// ==========================================
// --- SETUP ---
// ==========================================
void setup() {
  Serial.begin(115200);
  Serial.println(">>> Intercom Controller v34.0 Started (Offline Mode)");
  
  // Configure Pins
  pinMode(PIN_SIGNAL_INPUT, INPUT);
  pinMode(PIN_RELAY, OUTPUT);
  
  // Initial State
  digitalWrite(PIN_RELAY, LOW);
}

// ==========================================
// --- MAIN LOOP ---
// ==========================================
void loop() {
  // 1. Manage Relay State (Non-blocking)
  updateRelay();

  // 2. Read Analog Signal
  int adcVal = analogRead(PIN_SIGNAL_INPUT);
  bool signalActive = (adcVal > SIGNAL_THRESHOLD);

  // 3. Signal Processing Logic
  if (signalActive) {
    // --- SIGNAL DETECTED ---
    
    if (!isReceivingSignal) {
      // Rising Edge: Signal just started
      isReceivingSignal = true;
      signalStartTime = millis();
      Serial.println(">> Signal DETECTED");
    }
    
    // While signal is active, we just wait and let millis() count
    
  } else {
    // --- NO SIGNAL (IDLE) ---
    
    if (isReceivingSignal) {
      // Falling Edge: Signal just ended
      unsigned long duration = millis() - signalStartTime;
      isReceivingSignal = false;
      
      Serial.print("<< Signal ENDED. Duration: ");
      Serial.print(duration);
      Serial.println(" ms");
      
      // Analyze Duration
      if (duration >= MIN_PRESS_TIME && duration <= MAX_PRESS_TIME) {
        Serial.println("   [v] Valid Logic -> Unlocking...");
        openDoor();
      } else {
        Serial.println("   [x] Invalid Duration -> Ignored");
      }
    }
  }
  
  // Small delay to stabilize ADC readings
  delay(10);
}
