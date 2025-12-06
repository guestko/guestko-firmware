// --- PIN DEFINITIONS ---
const int PIN_SIGNAL_INPUT  = 3;  // ADC Input for 800Hz signal (0.8V)
const int PIN_BUTTON        = 4;  // Latching button (to GND)
const int PIN_LED_BREATHE   = 5;  // External LED/MOSFET (Controlled by button)
const int PIN_LED_INDICATOR = 8;  // Built-in LED/Relay (Indicates 800Hz detection)

// --- LED CONFIGURATION (Indicator) ---
// For ESP32-C3 SuperMini: LOW usually turns LED ON. 
// Change to HIGH if your relay/LED works differently.
const int INDICATOR_ON  = LOW;
const int INDICATOR_OFF = HIGH;

// --- BREATHING SETTINGS ---
int fadeValue = 0;
int fadeStep = 2;              // Breathing speed
unsigned long lastFadeUpdate = 0;
const int FADE_INTERVAL = 10;  // Update LED every 10ms

// --- DETECTION SETTINGS ---
// 0.8V is approx 990 on ADC. Threshold set lower to catch edges reliably.
const int SIGNAL_THRESHOLD = 300; 
const int SAMPLE_WINDOW = 50;     // Listen for 50ms
const int MIN_PULSES = 25;        // Min pulses for 800Hz in 50ms
const int MAX_PULSES = 60;        // Max pulses

void setup() {
  Serial.begin(115200);
  
  // Setup Pins
  pinMode(PIN_SIGNAL_INPUT, INPUT);
  pinMode(PIN_BUTTON, INPUT_PULLUP);
  pinMode(PIN_LED_BREATHE, OUTPUT);
  pinMode(PIN_LED_INDICATOR, OUTPUT);
  
  // Start with indicator OFF
  digitalWrite(PIN_LED_INDICATOR, INDICATOR_OFF);
  
  // Start External LED OFF
  analogWrite(PIN_LED_BREATHE, 0);
}

// Helper function to handle the Button and Breathing LED
// We extract this so we can call it frequently inside the listening loop
void updateBreathingSystem() {
  // Check button state: LOW = Closed (GND), HIGH = Open
  bool isSwitchClosed = (digitalRead(PIN_BUTTON) == LOW);

  if (isSwitchClosed) {
    // --- MODE: SOLID ON ---
    analogWrite(PIN_LED_BREATHE, 255);
    fadeValue = 255; // Reset fade so it starts smoothly when opened

  } else {
    // --- MODE: BREATHING ---
    unsigned long now = millis();
    
    // Non-blocking timer for smooth animation
    if (now - lastFadeUpdate > FADE_INTERVAL) {
      lastFadeUpdate = now;
      
      fadeValue += fadeStep;

      // Reverse direction at limits
      if (fadeValue >= 255) {
        fadeValue = 255;
        fadeStep = -fadeStep;
      } else if (fadeValue <= 0) {
        fadeValue = 0;
        fadeStep = -fadeStep;
      }
      
      analogWrite(PIN_LED_BREATHE, fadeValue);
    }
  }
}

void loop() {
  unsigned long startMillis = millis();
  int pulseCount = 0;
  bool signalWasHigh = false;

  // --- SAMPLING LOOP (50ms duration) ---
  // We stay in this loop for 50ms to count pulses.
  // Crucially, we call updateBreathingSystem() INSIDE here so the LED doesn't freeze.
  while (millis() - startMillis < SAMPLE_WINDOW) {
    
    // 1. Update the Breathing LED (Priority Task)
    updateBreathingSystem();

    // 2. Read Signal (Background Task)
    int analogValue = analogRead(PIN_SIGNAL_INPUT);

    // Schmitt trigger logic to count pulses
    if (!signalWasHigh && analogValue > SIGNAL_THRESHOLD) {
      signalWasHigh = true;
      pulseCount++; 
    } 
    else if (signalWasHigh && analogValue < (SIGNAL_THRESHOLD - 50)) {
      signalWasHigh = false;
    }
  }

  // --- ANALYSIS (Runs every 50ms) ---
  // Check if we found ~40 pulses (800Hz)
  if (pulseCount >= MIN_PULSES && pulseCount <= MAX_PULSES) {
    // Signal Detected -> Turn ON Indicator
    digitalWrite(PIN_LED_INDICATOR, INDICATOR_ON);
    // Serial.printf("Signal! Pulses: %d\n", pulseCount); // Uncomment for debug
  } else {
    // No Signal -> Turn OFF Indicator
    digitalWrite(PIN_LED_INDICATOR, INDICATOR_OFF);
  }
}
