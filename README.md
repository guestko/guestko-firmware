# Smart Intercom ESP32 (Telegram & OTA Edition)

A smart intercom system based on the **ESP32-C3 SuperMini**. It features Telegram notifications, remote door control, and wireless cloud updates (OTA) directly from Google Drive.

## 🌟 Key Features
* **🔔 Instant Notifications:** Receive a message in Telegram when the doorbell rings.
* **🔓 Remote Control:** Open the door using the `/open` command or the chat button.
* **☁️ Cloud Updates (OTA):** Update firmware wirelessly using direct Google Drive links (supports redirects).
* **🧠 Smart Status:** Visual connection status via LED and line noise diagnostics.
* **💾 Persistent Settings:** Sensitivity thresholds are saved in non-volatile memory.

---

## 💡 Status Indicator (LED)
Behavior of the built-in LED (GPIO 8, Active LOW):

| System State | LED Behavior | Meaning |
| :--- | :--- | :--- |
| **Startup** | ⚡️ 5 Rapid Blinks | Power ON, Hardware check passed. |
| **Connected** | 🔵 Solid ON | Wi-Fi connected, Bot active, System Ready. |
| **Disconnected** | ⚫️ OFF | No Wi-Fi connection or Network error. |

---

## 🛠 Admin Guide: How to Perform a Cloud Update (OTA)

This system allows you to update the firmware remotely without a USB cable. The bot automatically handles Google Drive "Share" links.

### Step 1: Prepare the Firmware File
**⚠️ CRITICAL:** Do not upload the source code (`.ino`). The microcontroller requires a compiled binary file (`.bin`).

1.  Open your sketch in **Arduino IDE**.
2.  Go to the menu: **Sketch -> Export Compiled Binary**.
3.  Open the sketch folder (**Sketch -> Show Sketch Folder**).
4.  Locate the file ending in `.bin` (e.g., `intercom.ino.esp32c3.bin`).
    * *Note: Ignore files containing `bootloader` or `partitions`.*

### Step 2: Upload to Cloud
1.  Upload the `.bin` file to **Google Drive**.
2.  Right-click the file -> **Share**.
3.  Under "General Access", select: **"Anyone with the link"**.
4.  Click **Copy Link**.

### Step 3: Trigger the Update
1.  Open your Telegram chat with the Intercom Bot.
2.  Send the update command with your link:
    ```text
    /update [https://drive.google.com/file/d/YOUR_FILE_ID/view?usp=sharing](https://drive.google.com/file/d/YOUR_FILE_ID/view?usp=sharing)
    ```
3.  **Wait for the process:**
    * Bot replies: `⏳ OTA Starting...`
    * If successful: `✅ OK! Rebooting...`
    * After ~15 seconds, you will receive a `✅ SYSTEM READY` message with the new build timestamp.

---

## ⚙️ Bot Commands
* `/start` — Main Menu.
* `/open` — Open the door (triggers relay).
* `/status` — Show signal noise level, current threshold, and Firmware Build Date.
* `/update [url]` — Start the OTA update process.
* **Settings Menu:** Adjust the signal sensitivity threshold.

---

## 🔌 Hardware Pinout
* **Board:** ESP32-C3 SuperMini (Black).
* **Signal Input (Audio):** GPIO 3 (Analog).
* **Relay (Door):** GPIO 10.
* **Status LED:** GPIO 8.

## ⚠️ Initial Setup
For the very first installation (or if the device is "bricked" and cannot connect to Wi-Fi), you must flash the firmware via **USB Cable** using Arduino IDE. All subsequent updates can be done via Telegram.
