# Smart Intercom Controller (Urmet CS1132 / 1131)

Upgrade a standard analog apartment intercom handset into an automated entry system using **ESP32-C3**.

## Project Structure

```
Smart-Intercom-ESP32/
├── intercom-esp32-receiver.ino      # Legacy offline receiver (peak detector + relay)
├── intercom-signal-simulator.ino    # Legacy simple PWM generator
└── matter/
    ├── intercom-adapter.ino         # Production: Tuya/Matter adapter + OLED
    ├── intercom-signal-generator.ino # Bench calibrator with OLED
    ├── matter-esp32.ino             # Matter PoC switch demo
    ├── adapter technical specification
    ├── generator technical specification
    └── README.md
```

## Which Firmware to Use?

| Use case | Firmware |
|----------|----------|
| **Production install in Urmet handset (Tuya Smart Life)** | `matter/intercom-adapter.ino` |
| **Bench testing / calibrating receivers** | `matter/intercom-signal-generator.ino` |
| **Matter protocol experiments** | `matter/matter-esp32.ino` |
| **Simple offline auto-open (no Wi-Fi)** | `intercom-esp32-receiver.ino` |

---

## Production Adapter (Tuya / Matter)

**Core function:** Detect intercom ring via galvanically isolated PC817 optocoupler, report to Tuya over Matter, open door via PhotoMOS on Tuya automation command.

**Highlights:**
- Galvanic isolation (PC817 + PhotoMOS TLP172A)
- Always-on Wi-Fi + external PSU
- Scheduling & notifications in **Smart Life** app (no RTC module)
- 0.42" OLED status display (READY / RING / DOOR OPEN)

See [`matter/adapter technical specification`](matter/adapter%20technical%20specification) for full wiring diagram.

### Quick Wiring (Adapter)

**Ring detector:**
- CA → 1N4007 → 10 µF → PC817 IN+/IN−
- PC817 OUT → GPIO4
- Pin 6 (GND) → common ground

**Door release:**
- GPIO2 → 220 Ω → PhotoMOS IN+
- PhotoMOS OUT → Pin 9 ↔ Pin 6

**Display:** onboard OLED on GPIO5 (SDA) / GPIO6 (SCL)

---

## Legacy Offline Receiver

Standalone auto-open without Wi-Fi. Uses peak detector on GPIO3 and relay on GPIO10.

See wiring in the sections below (original v34 design).

### Peak Detector Wiring

⚠️ **Do not connect USB while wired to intercom.** Use isolated USB charger only.

- Intercom CA → Diode Anode → Diode Cathode → **GPIO 3**
- GPIO 3 → Capacitor (+) / Capacitor (−) → GND
- GPIO 3 → Resistor 10 kΩ → GND
- Intercom Pin 1 (Speaker) → GND

### Relay

- GPIO 10 → Relay IN
- Pin 9 & 10 → Relay COM / NO

### Logic

- ADC threshold > 1000 (~0.8 V)
- Valid ring: 0.5 – 5.0 s → relay pulse 500 ms

---

## Flashing

1. Open the desired `.ino` in Arduino IDE
2. Board: **ESP32C3 Dev Module**
3. Install **U8g2** (OLED sketches) and **Matter** library (adapter/PoC)
4. Flash via USB (disconnect from intercom first)
5. For adapter: commission in Smart Life, create ring → door automation

## License

Open Source DIY Project. Use at your own risk.
