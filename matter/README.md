# Matter / Tuya Intercom Modules

ESP32-C3 firmware for the Smart Intercom project: bench signal generator, Matter PoC switch, and production intercom adapter for Tuya Smart Life.

## Firmware Files

| Sketch | Purpose |
|--------|---------|
| [`intercom-adapter.ino`](intercom-adapter.ino) | **Production adapter** — ring detection (PC817), door control (PhotoMOS), Matter → Tuya, OLED UI |
| [`intercom-signal-generator.ino`](intercom-signal-generator.ino) | **Bench calibrator** — ±4 V AC signal generator with OLED |
| [`matter-esp32.ino`](matter-esp32.ino) | **Matter PoC** — OnOff light demo with OLED and activation counter |

## Technical Specifications

| Document | Description |
|----------|-------------|
| [`adapter technical specification`](adapter%20technical%20specification) | Adapter wiring, PhotoMOS, optocoupler, Tuya/Matter architecture |
| [`generator technical specification`](generator%20technical%20specification) | Signal generator schematic and OLED notes |

## Hardware: ESP32-C3 0.42" OLED Board

| Component | GPIO | Notes |
|-----------|------|-------|
| OLED SDA | 5 | Software I2C (adapter/generator) or HW I2C (PoC) |
| OLED SCL | 6 | |
| Blue LED | 8 | Active LOW |
| BOOT button | 9 | PoC only |
| Ring input (adapter) | 4 | PC817, active LOW |
| Door output (adapter) | 2 | PhotoMOS TLP172A |
| Signal PWM (generator) | 1 | IRLB8743 MOSFET gate |
| Freq button (generator) | 3 | |

## Adapter Architecture (Tuya)

```
Ring (CA) → PC817 → GPIO4 → duration filter (1–3 s)
                                    ↓
                          Matter Contact Sensor → Tuya
                                    ↓
                    Tuya automation (schedule, notify, …)
                                    ↓
                          Matter OnOff Switch → GPIO2 → PhotoMOS → Door
```

- **Always-on** Wi-Fi + external PSU
- **No RTC / DS3231** — scheduling in Smart Life app
- **OLED states:** READY, RING, EVENT SENT, DOOR OPEN, PAIRING, NO WiFi

## Required Libraries (Arduino IDE)

- [U8g2](https://github.com/olikraus/U8g2_Arduino) — OLED display
- [ESP32 Arduino Matter](https://github.com/espressif/arduino-esp32) — adapter & PoC (enable Matter in board support)

## Flashing

1. Board: **ESP32C3 Dev Module**
2. Disconnect adapter from intercom wiring before USB flash
3. Commission adapter in **Smart Life** (Matter pairing via QR in Serial monitor)
4. Create automation: *When ring sensor opens → turn door switch ON*
