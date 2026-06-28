## Project Summary: Universal Matter-Based Smart Switch PoC

This project serves as a **Proof of Concept (PoC)** for a universal, commercial-grade smart switch/intercom controller built on the open-source **Matter** protocol using an ultra-compact **ESP32-C3 OLED Development Board**. 

By bypassing proprietary ecosystems and legacy local servers, this solution embraces the modern Matter standard. It provides a decentralized, secure, and cross-platform hardware accessory capable of operating simultaneously across all major smart home fabrics (Apple Home, Google Home, Smart Life/Tuya) entirely locally.

### 🚀 Key Features

* **Universal Ecosystem Integration:** Powered by the Matter protocol for seamless, ultra-low latency, and local multi-fabric control across Apple Home, Google Home, and Tuya apps.
* **Zero-Config Wi-Fi Provisioning:** Employs Bluetooth Low Energy (BLE) commissioning. Network credentials are securely transmitted directly from the user's smartphone during initial setup—eliminating the need for hardcoded Wi-Fi SSIDs or passwords in the source code.
* **Persistent Metrics Tracking:** Automatically logs and tracks device usage via an activation counter. Utilizing the embedded `Preferences` library, metric data is written to the ESP32's non-volatile storage (NVS) using safe key-value journaling to prevent flash memory wear while surviving unexpected power cuts.
* **Real-Time Micro-OLED Interface:** Integrates a 0.42" OLED screen driven by the lightweight `U8g2` library. The display dynamically visualizes pairing states (`PAIRING...`), real-time operations (`STATUS: ON/OFF`), and total telemetry data (`COUNT`).
* **Hardware-Level Overrides:** Programmed debounced routing via the physical BOOT button for immediate local control, paired with a 5-second long-press trigger for localized device decommissioning and factory resets.

### 🛠 Hardware Specification & Pinout

The software architecture is specifically tuned to match the hardware-level quirks of the specialized **ESP32-C3 0.42" OLED** board variants:

| Component | Pin (GPIO) | Configuration Notes |
| :--- | :--- | :--- |
| **Onboard Blue LED** | `GPIO 8` | Set up with inverted (**Active-LOW**) logic |
| **BOOT Button** | `GPIO 9` | Configured with internal `INPUT_PULLUP` for toggling/resetting |
| **OLED Display SDA** | `GPIO 5` | Hardware I2C Data Line (SSD1306 72x40 matrix) |
| **OLED Display SCL** | `GPIO 6` | Hardware I2C Clock Line (SSD1306 72x40 matrix) |