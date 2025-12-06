# Button with LED and MOSFET Driver (ESP32)

## Overview
This setup uses a push button with an integrated LED.  
The LED is driven through an N‑channel MOSFET **IRLB8743**.  
The ESP32 controls the MOSFET gate to enable smooth "breathing" animation or full‑brightness mode.

## Wiring

### Button
- One side of the button is connected to **GND**.
- The other side goes to **GPIO 4** on the ESP32.
- The pin is configured as **INPUT_PULLUP**, so the button pulls the pin LOW when pressed.

### LED + MOSFET
- ESP32 pin **GPIO 5** drives the **gate** of the MOSFET.
- Between ESP32 pin and MOSFET gate there is a **100 Ω resistor** (gate current limiter).
- Between **gate** and **source** of MOSFET there is a **100 kΩ resistor** (pull‑down to keep MOSFET off when pin is floating).
- MOSFET used: **IRLB8743** (logic‑level, suitable for LED loads).
- LED (or LED assembly) is connected between **VCC** and **MOSFET drain**.
- MOSFET **source** goes to **GND**.

### Behavior
- When the button is pressed, the mode toggles between:
  - **Breathing LED** (PWM fade in/out)
  - **Solid LED** (full brightness)

## Sketch

```cpp
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
```
