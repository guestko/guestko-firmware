# Offline Smart Intercom Controller

[English](README.md) | [Polski](README.pl.md)

## Project goal

This project explores a low-cost, offline way to automate entry through an existing analog apartment intercom. The goal is to let a visitor ring the apartment and have the controller operate the door-release contact automatically, without a phone app, Wi-Fi, or a cloud service.

The prototype uses an **ESP32-C3 SuperMini**, a ring-detection circuit, and a relay connected to the handset's door-release contacts. The target hardware is an **Urmet CS1132 / 1131-style analog 4+n installation**. Wiring and signal characteristics must be checked for the specific installation; this repository does not establish compatibility with every handset in those families.

For clients and reviewers, the repository contains the receiver firmware and the earlier bench signal simulator. It documents an iterative hardware prototype, rather than a universally validated intercom product. The current firmware opens the door after any ring that meets its duration criteria; it does not identify or authenticate the visitor.

## Development history and project materials

Initially, I used an ESP mini board to simulate the intercom ringing signal on the bench. This proved unsuitable as a substitute for the real installation: what worked on the table did not work in the field. The simulated signal did not provide a sufficient basis for validating operation with the actual intercom.

In later iterations, I used a purchased signal generator for testing. It is visible in some of the project photos. The original simulator sketch is retained here as part of the development history.

[Project photos and related materials (iCloud shared album)](https://www.icloud.com/sharedalbum/#D1gv3LkAJSC1a3V6JjvIoOCJMTwCAEQARogahy9vvzygcSkKMiSHnLvQaCZdfpO5kJrTrVS_D_g5ss)

## Repository contents

| File | Purpose |
| --- | --- |
| [`intercom-esp32-receiver.ino`](intercom-esp32-receiver.ino) | Standalone receiver: measures ring duration and controls the relay. |
| [`intercom-signal-simulator.ino`](intercom-signal-simulator.ino) | Early ESP bench simulator: generates 800 Hz, 50% duty-cycle PWM. GPIO 5 is active while a button on GPIO 4 is held to ground; GPIO 6 outputs continuously. |
| [`1132_INSTRUKCJA.pdf`](1132_INSTRUKCJA.pdf), [`ds.pdf`](ds.pdf) | Included hardware reference PDFs. |

## Receiver hardware and wiring

Required components:

- ESP32-C3 SuperMini.
- Relay module with a 3.3 V-compatible, active-high control input. Supply the module at its rated voltage.
- Peak detector: 1N4007 diode, 4.7 µF capacitor, and 10 kΩ discharge resistor.
- Isolated USB wall supply or power bank.

**Disconnect the intercom before connecting the ESP32 to a computer by USB.** In this prototype, ESP32 ground is connected to the handset's speaker line (pin 1), rather than an assumed universal ground. Check the installation's wiring and detector output voltage before connecting the ESP32; the detector does not regulate or clamp the input voltage.

### Ring detection

The peak detector rectifies and smooths the ring waveform for the analog input. The prototype's expected ring waveform is approximately 800 Hz and ±3.5 V relative to the selected reference; actual installations may differ.

- Intercom **CA (call line / wywołanie)** → diode anode.
- Diode cathode → **GPIO 3**.
- At the GPIO 3 node, connect the **4.7 µF capacitor** and **10 kΩ resistor** in parallel to ESP32 GND. For an electrolytic capacitor, connect its positive terminal to GPIO 3.
- Intercom **pin 1 (speaker line / słuchawka)** → ESP32 GND.

### Door release

- ESP32 **GPIO 10** → relay IN.
- Relay VCC → supply matching the module's rating.
- Relay GND → ESP32 GND.
- Handset door-release contacts, **pins 9 and 10 in the documented prototype wiring**, → relay COM and NO (normally open). Verify these contacts on the actual handset before wiring.

## Receiver behavior

The implementation in `intercom-esp32-receiver.ino` uses the following settings:

| Setting | Value |
| --- | --- |
| Analog signal input | GPIO 3 |
| Active-high relay output | GPIO 10 |
| Signal threshold | ADC reading strictly greater than 1000 |
| Accepted detected duration | 500–5000 ms, inclusive |
| Nominal relay pulse | 500 ms |
| Loop delay | 10 ms |
| Serial baud rate | 115200 |

1. On startup, the firmware sets the relay output LOW. It does not initialize Wi-Fi or any cloud connection.
2. An ADC reading above 1000 starts timing a signal. The threshold is a raw ADC count, not a calibrated voltage measurement.
3. When the reading drops to 1000 or below, the firmware checks the elapsed duration. It activates the relay only if that duration is between 500 and 5000 ms, inclusive. **Unlocking happens after the detected ring ends.** Shorter or longer signals are ignored; a continuously active signal does not trigger an unlock.
4. The relay returns LOW once more than 500 ms have elapsed, as checked by the main loop. The pulse is therefore approximately 500 ms, with timing affected by the loop delay and processing.

The duration check applies to each uninterrupted above-threshold interval. There is no separate software debounce, gap tolerance, or cooldown between accepted rings. The peak detector is intended to smooth the waveform, and the short relay pulse is intended to reduce interference; field behavior depends on the hardware and installation.

## Flashing and setup

1. Disconnect the ESP32 from the intercom before connecting it to a computer.
2. Use the Arduino IDE with ESP32 board support and select **ESP32C3 Dev Module** for the receiver board.
3. Put `intercom-esp32-receiver.ino` in its own sketch folder named `intercom-esp32-receiver`, then open and upload it. Keep the simulator in a separate sketch folder: both files define `setup()` and `loop()` and must not be compiled together.
4. Disconnect the computer, wire the receiver to the verified intercom connections, and power it from the isolated supply or power bank.
5. Verify ring detection and door-release behavior on the actual installation, including rejection of short and overlong signals.

The simulator is a separate test sketch, not part of the deployed receiver. Its GPIO assignments and PWM configuration are defined in `intercom-signal-simulator.ino`; its bench results do not demonstrate compatibility with a real intercom.

## License

No license file is currently included in this repository.
