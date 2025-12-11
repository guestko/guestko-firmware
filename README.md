# Offline Smart Intercom Controller (for Urmet CS1132 / 1131)

## 📖 Overview
This project upgrades a standard analog apartment intercom handset (**Urmet CS1132 Scaitel, 1131, 1140 series**) into an automated entry system using an **ESP32-C3 SuperMini**.

**Core Function:**
The device monitors the intercom line for a ringing signal. If a valid ring is detected (duration between 0.5s and 5s), it automatically triggers the relay to open the door.

**Highlights:**
* **100% Offline:** No WiFi, no Cloud, no delays. Instant boot.
* **Auto-Open:** Automatically unlocks the door when rung.
* **Noise Reduction:** Uses a Peak Detector circuit and short relay pulse (0.5s) to prevent audio interference.
* **Compatibility:** Designed for analog "4+n" systems (Urmet CS1132 PCB, 5025/2D panels).

---

## 🛠 Hardware Required

1.  **Microcontroller:** ESP32-C3 SuperMini.
2.  **Relay Module:** 3.3V High-Level Trigger.
3.  **Components for Peak Detector:**
    * **Diode:** 1N4007 (Essential for blocking negative voltage).
    * **Capacitor:** 4.7 µF (Electrolytic or Ceramic).
    * **Resistor:** 10 kΩ (Discharge resistor).

---

### 🔌 Wiring Diagram

**⚠️ SAFETY WARNING:** The Signal Ground is connected to the Intercom Speaker Line (Pin 1). 
**DO NOT connect the ESP32 to a computer via USB while it is connected to the intercom.** Power the ESP32 only via an isolated USB wall charger or power bank.

#### 1. Signal Detection (Peak Detector)
This circuit converts the intercom's PWM ring signal (~800Hz, +/- 3.5V) into a stable DC voltage for the ESP32.

* **Intercom CA (Call Line / Wywołanie)** -> **Diode Anode (+)**
* **Diode Cathode (-)** -> **GPIO 3** (ESP32 Input)

* **At the GPIO 3 Node (Parallel connection):**
    1.  **Capacitor (+)** to GPIO 3, **Capacitor (-)** to GND.
    2.  **Resistor (10k)** to GPIO 3, **Resistor** to GND.

* **Intercom Pin 1 (Speaker Line / Słuchawka)** -> **GND** (ESP32).

#### 2. Door Control (Relay)
* **ESP32 GPIO 10** -> **Relay IN**.
* **ESP32 5V/3.3V** -> **Relay VCC**.
* **ESP32 GND** -> **Relay GND**.
* **Intercom Pin 9 & 10** -> Connected to Relay **COM** and **NO** (Normally Open) terminals.

---

## ⚙️ Logic & Timing

1.  **Signal Detection:**
    * The code reads the analog voltage on **GPIO 3**.
    * Threshold: **ADC > 1000** (approx 0.8V). 
    * Due to the Peak Detector circuit, a valid ring produces ~2.7V DC.

2.  **Validation:**
    * Short glitches (< 500ms) are ignored.
    * Stuck buttons (> 5000ms) are ignored.
    * Only signals between **0.5s and 5.0s** trigger the door.

3.  **Action:**
    * The relay fires for exactly **500 ms**.
    * This short pulse is sufficient to unlatch the mechanical lock without causing buzzing on the handset audio line.

---

## 🚀 Installation

1.  Connect the ESP32 to your PC (Disconnect from intercom first!).
2.  Flash `intercom-esp32.ino` using Arduino IDE (Board: ESP32C3 Dev Module).
3.  Disconnect from PC.
4.  Wire up to the intercom system.
5.  Power up via external USB charger.
6.  System is live immediately.

## 📝 License
Open Source DIY Project. Use at your own risk.
