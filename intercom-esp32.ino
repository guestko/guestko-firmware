// ---- Pins ----
const int PIN_LED = 5;        // MOSFET GATE
const int PIN_BUTTON = 4;     // Button to GND, INPUT_PULLUP

// ---- Logical state ----
bool isActive = false;        // false = breathing, true = LED fully on

// ---- Breathing parameters ----
int fadeValue = 0;
int fadeStep = 1;             // was 5 — make it about 2x smoother
unsigned long lastFadeUpdate = 0;

// ---- Debounce ----
unsigned long lastButtonTime = 0;
const int debounce = 180;

void setup() {
  pinMode(PIN_BUTTON, INPUT_PULLUP);
  pinMode(PIN_LED, OUTPUT);

  analogWrite(PIN_LED, 0);
}

void loop() {
  // ---- Button ----
  if (digitalRead(PIN_BUTTON) == LOW) {
    unsigned long now = millis();

    if (now - lastButtonTime > debounce) {
      lastButtonTime = now;

      // Toggle state
      isActive = !isActive;

      // If active now — LED to max
      if (isActive) {
        analogWrite(PIN_LED, 255);
      }
    }
  }

  // ---- Modes ----
  if (!isActive) {
    // ----------- Smooth breathing -----------
    unsigned long now = millis();

    if (now - lastFadeUpdate > 10) {  // breathing speed
      lastFadeUpdate = now;

      fadeValue += fadeStep;

      if (fadeValue >= 255 || fadeValue <= 0) {
        fadeStep = -fadeStep;
      }

      analogWrite(PIN_LED, fadeValue);
    }

  } else {
    // ----------- Solid LED -----------
    analogWrite(PIN_LED, 255);
  }
}
