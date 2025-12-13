# Smart Intercom System (Telegram & OTA Edition)
### For Urmet CS1132 / 1131 / 1140 Series

This project upgrades a standard analog apartment intercom handset (**Urmet CS1132 Scaitel, 1131, 1140 series**) into an automated, smart entry system using an **ESP32-C3 SuperMini**.

## 📖 Overview
The device monitors the analog intercom line using a custom peak detector circuit. When a ring is detected, it notifies you via **Telegram**, allows remote door control, and supports wireless firmware updates.

## 🌟 Key Features
* **🔔 Instant Notifications:** Receive a Telegram message immediately when the doorbell rings.
* **🔓 Remote & Auto Control:** Open the door manually via Telegram (`/open`) or automatically upon ring detection.
* **☁️ Cloud Updates (OTA):** Update firmware wirelessly using direct Google Drive links (supports redirects).
* **🔇 Noise Reduction:** Uses a hardware Peak Detector and short relay pulse (500ms) to prevent audio interference/buzzing on the line.
* **🧠 Smart Status:** Visual connection status via LED and line noise diagnostics.
* **💾 Persistent Settings:** Sensitivity thresholds are saved in non-volatile memory.

---

## 🛠 Hardware Required

1.  **Microcontroller:** ESP32-C3 SuperMini (Black board recommended).
2.  **Relay Module:** 3.3V High-Level Trigger.
3.  **Components for Peak Detector (Signal Conditioning):**
    * **Diode:** 1N4007 (Essential for blocking negative voltage).
    * **Capacitor:** 4.7 µF (Electrolytic or Ceramic).
    * **Resistor:** 10 kΩ (Discharge resistor).

---

## 🔌 Wiring Diagram & Safety

**⚠️ SAFETY WARNING:** The Intercom Signal Ground is often shared with the Speaker Line.
**DO NOT connect the ESP32 to a computer via USB while it is connected to the intercom.** This can cause a ground loop and damage your USB port. Power the ESP32 only via an isolated USB wall charger or power bank during operation.

### 1. Signal Detection (Peak Detector Circuit)
This circuit converts the intercom's PWM ring signal (~800Hz, +/- 3.5V) into a stable DC voltage for the ESP32 to read safely.

* **Intercom CA (Call Line / Wywołanie)** → **Diode Anode (+)**
* **Diode Cathode (-)** → **GPIO 3** (ESP32 Analog Input)

* **At the GPIO 3 Node (Parallel connection):**
    1.  **Capacitor (+)** to GPIO 3, **Capacitor (-)** to GND.
    2.  **Resistor (10k)** to GPIO 3, **Resistor** to GND.

* **Intercom Pin 1 (Speaker Line / Słuchawka)** → **ESP32 GND**.

### 2. Door Control (Relay)
* **ESP32 GPIO 10** → **Relay IN**.
* **ESP32 3.3V/5V** → **Relay VCC**.
* **ESP32 GND** → **Relay GND**.
* **Intercom Pin 9 & 10** → Connect to Relay **COM** and **NO** (Normally Open) terminals.

### 3. Status LED
* **Built-in LED:** GPIO 8 (Active LOW logic).

---

## 💡 Status Indicator (LED)
Behavior of the built-in LED (GPIO 8):

| System State | LED Behavior | Meaning |
| :--- | :--- | :--- |
| **Startup** | ⚡️ 5 Rapid Blinks | Power ON, Hardware check passed. |
| **Connected** | 🔵 Solid ON | Wi-Fi connected, Bot active, System Ready. |
| **Disconnected** | ⚫️ OFF | No Wi-Fi connection or Network error. |

---

## ⚙️ Logic & Sensitivity
1.  **Signal Detection:** The code reads the analog voltage on **GPIO 3**.
2.  **Validation:**
    * Short glitches (< 500ms) are ignored.
    * Stuck buttons (> 5000ms) are ignored.
    * Only signals between **0.5s and 5.0s** trigger the notification/action.
3.  **Action:** The relay fires for exactly **500 ms**. This is sufficient to unlatch the mechanical lock without disturbing the audio line.

---

## 🛠 Admin Guide: Cloud Update (OTA)
The system supports remote updates via **Google Drive**.

### Step 1: Prepare the File
**⚠️ CRITICAL:** Do not upload the `.ino` source code. You must compile it first.
1.  In Arduino IDE: **Sketch -> Export Compiled Binary**.
2.  Locate the `.bin` file in the sketch folder (e.g., `intercom.ino.esp32c3.bin`).

### Step 2: Upload & Share
1.  Upload the `.bin` file to **Google Drive**.
2.  Right-click -> **Share** -> Change access to **"Anyone with the link"**.
3.  **Copy Link**.

### Step 3: Update via Telegram
Send the command to your bot:
```text
/update [https://drive.google.com/file/d/YOUR_FILE_ID/view?usp=sharing](https://drive.google.com/file/d/YOUR_FILE_ID/view?usp=sharing)
