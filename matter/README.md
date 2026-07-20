# Matter / Tuya Intercom Modules

ESP32-C3 firmware for the Smart Intercom project: Matter PoC switch and production intercom adapter for Tuya Smart Life.

## Firmware Files

| Sketch | Purpose |
|--------|---------|
| [`intercom-adapter.ino`](intercom-adapter.ino) | **Production adapter** — ring detection (PC817), door control (PhotoMOS), Matter → Tuya, OLED UI |
| [`matter-esp32.ino`](matter-esp32.ino) | **Matter PoC** — OnOff light demo with OLED and activation counter |

## Technical Specifications

| Document | Description |
|----------|-------------|
| [`adapter technical specification`](adapter%20technical%20specification) | Adapter wiring, PhotoMOS, optocoupler, Tuya/Matter architecture |
| [`../docs/signal-generator-wiring.md`](../docs/signal-generator-wiring.md) | **Bench test:** FeelTech FY6300 → optocoupler input (verified nominals) |

## Hardware: ESP32-C3 0.42" OLED Board

| Component | GPIO | Notes |
|-----------|------|-------|
| OLED SDA | 5 | Software I2C (adapter) or HW I2C (PoC) |
| OLED SCL | 6 | |
| Blue LED | 8 | Active LOW; 3 blinks on valid ring, solid ON on door pulse |
| BOOT button | 9 | PoC only |
| Ring input (adapter) | 4 | PC817, active LOW |
| Door output (adapter) | 2 | PhotoMOS TLP172A (HIGH 2 s; bench OK without load) |

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
- **Blue LED:** 3 blinks on valid ring, solid ON during 2 s door pulse
- **Wi-Fi:** Matter commissioning only (Smart Life QR)

## Bench Testing

Use **FeelTech FY6300-60M** (50 Hz AM / 850 Hz) instead of ESP-based generators. See [`../docs/signal-generator-wiring.md`](../docs/signal-generator-wiring.md).

Optocoupler input nominals (same as production adapter):

- **1N4007** + **10 µF (106)** → PC817 pin 1–2

## Required Libraries (Arduino IDE)

- [U8g2](https://github.com/olikraus/U8g2_Arduino) — OLED display
- [ESP32 Arduino Matter](https://github.com/espressif/arduino-esp32) — adapter & PoC (enable Matter in board support)

## Flashing

1. Board: **ESP32C3 Dev Module**
2. **Partition Scheme:** **Huge APP (3 MB No OTA / 1 MB SPIFFS)** — required (~2 MB Matter firmware)
3. **Erase All Flash Before Sketch Upload:** **Enabled** (first flash and after partition change)
4. **Flash Size:** **4 MB** (default on most C3 dev boards)
5. Open `intercom-adapter.ino` from this repo (`matter/` folder includes `partitions.csv`)
6. Disconnect adapter from intercom wiring before USB flash
7. Commission adapter in **Smart Life** (Matter pairing via QR in Serial monitor)
8. Rename endpoints: **Intercom Ring**, **Intercom Door**
9. Create automations:
   - Auto-open: ring → door ON, time 16:00–20:00
   - Notify: ring outside 16:00–20:00 → *Someone's at the door!*
   - Manual: tap Intercom Door anytime
