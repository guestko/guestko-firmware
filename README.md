# Smart Intercom DIY Project with ESP32

## 📖 Overview
This project upgrades a standard analog apartment intercom handset with smart features using an **ESP32-C3 SuperMini** microcontroller.

**Key Features:**
* **Secret Knock:** Open the door by tapping a rhythmic code on the intercom button (e.g., 3 short clicks).
* **Telegram Bot:** Full control via smartphone (Open door, Manage users, Configure settings).
* **WiFi Manager:** No hardcoded credentials. Connect to the device's hotspot to set up Wi-Fi.
* **User Management:** Invite family members via temporary codes.
* **Physical Safety Switch:** Toggle a physical switch to completely disable the "listening" mode for security.

---

## 🛠 Hardware Required

1.  **Microcontroller:** ESP32-C3 SuperMini (or any ESP32 board).
2.  **Relay:** 3.3V High-Level Trigger Relay (to simulate button press).
3.  **Passive Components (RC Filter):**
    * Resistor: **4.7 kΩ**
    * Capacitor: **4.7 µF** (Ceramic or Electrolytic)
    * *This filter smooths the intercom's PWM signal into DC voltage for the ESP32.*
4.  **Switch/Button:**
    * **Type:** Latching Illuminated Push Button (Internal resistor, up to 12V, polarity insensitive).
    * **Function:** Used for Arm/Disarm mode and visual status.
5.  **LED Driver (MOSFET Control):**
    * **MOSFET:** **IRLB8743** (N-Channel Logic Level).
    * **Gate Resistor:** **100 Ω** (Protects ESP32 GPIO).
    * **Pull-down Resistor:** **100 kΩ** (Keeps Gate stable/closed).
    * **Wiring:** Gate to GPIO 5, Source to GND, Drain to Button LED cathode (-).

### 🔌 Wiring Diagram

#### 1. Signal Input ("The Ear")
* **Source:** Intercom Signal Line (+)
* **Connection:** Signal Line -> Resistor (4.7k) -> **GPIO 3** (ESP32)
* **Filtering:** **GPIO 3** -> Capacitor (4.7uF) -> **GND**

#### 2. Door Control
* **Relay IN:** **GPIO 10** (Active HIGH)

#### 3. Safety Switch (Button Input)
* **Pin:** **GPIO 4**
* **Connection:** One side to GPIO 4, other side to **GND**.
* *Note: Uses internal INPUT_PULLUP.*

#### 4. Status LED (Button Backlight)
Controlled via MOSFET to handle the button's built-in LED (up to 12V).

---

## 🚀 Installation & Setup

### 1. Flash the Firmware
1.  Open `intercom-esp32.ino` in Arduino IDE.
2.  Install required libraries:
    * `UniversalTelegramBot` by Brian Lough
    * `ArduinoJson` by Benoit Blanchon
    * `WiFiManager` by tzapu
3.  **CRITICAL:** Edit the `HARDCODED CONFIG` section at the top of the sketch:
    ```cpp
    const String ROOT_ADMIN_ID = "YOUR_TELEGRAM_ID"; // Get from @userinfobot
    const String BOT_TOKEN     = "YOUR_BOT_TOKEN";   // Get from @BotFather
    ```
4.  Select board **"ESP32C3 Dev Module"** and upload.

### 2. First Run (Wi-Fi Setup)
1.  Power up the ESP32.
2.  Connect your phone to the Wi-Fi network named **`Intercom-Setup`**.
3.  A portal will open automatically (or go to `192.168.4.1`).
4.  Select your home Wi-Fi and enter the password.
5.  The device will reboot and connect to Telegram.

### 3. Telegram Bot Usage
* `/start` - Open the main menu.
* **Open 🔓** - Trigger the relay to open the door immediately.
* **Status 📊** - Check connection, active code, and signal levels.
* **⚙️ Setup Code** - Change the secret rhythmic pattern (Short/Long presses).
* **🛠 Settings** - Tune signal sensitivity (Debounce Gap, Long Press threshold).
* **👥 Users** - Generate invite codes for family members.

---

## 🧠 How it Works

### Signal Detection Logic
The intercom uses a PWM-like signal. The RC circuit converts this to a DC voltage level.
* **Idle (Standby):** Line is at **0V**. ADC reads ~0.
* **Active (Button Pressed):** Signal appears, voltage rises to **~0.3V - 0.4V**.
* **Logic:** The firmware detects a press when `ADC > 200` (Threshold ~0.15V).

### Security Features
1.  **Root Admin:** Your ID is hardcoded. You cannot be locked out.
2.  **Physical Switch:**
    * **OFF:** The ESP32 physically ignores the input line. "Safe Mode". The Button LED breathes.
    * **ON:** The ESP32 listens for the secret code. The Button LED is solid ON.
3.  **Watchdog:** The code uses non-blocking logic to prevent crashes during Wi-Fi activity.

### Factory Reset
If you change your Wi-Fi router:
1.  Toggle the physical switch **5 times within 3 seconds**.
2.  The LED will flash rapidly (Strobe effect), and settings will be wiped.
3.  Connect to `Intercom-Setup` again.

---

## 📝 License
This is an open-source DIY project. Use at your own risk.
