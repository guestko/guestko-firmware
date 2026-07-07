# Custom Audio Branch (Future Work)

This branch explores **optional custom ringback melodies** for the intercom adapter.
Production v1 development continues on the `matter` branch (minimal invasive: listen-only tap, no CA cut).

## Problem

Cutting the CA line and injecting audio via BC547 breaks normal intercom operation outside
smart mode. A bypass mechanism is required so the handset can fall back to the factory buzzer.

## Switching Options (TBD)

### Option A — Mechanical toggle (2 positions)

```
Position 1 (STANDARD):  CA ── factory buzzer path (original PCB)
Position 2 (CUSTOM):    CA ── ESP32 audio injector (BC547 / DAC)
```

- Pros: works without Wi-Fi/ESP; user-visible mode; failsafe
- Cons: manual switch; mounting inside handset

### Option B — Relay / PhotoMOS bypass (software-controlled)

```
ESP OFF / schedule inactive:  relay ──> standard path
ESP active / custom mode:     relay ──> injector path
```

- Pros: automatic; integrates with Tuya schedule
- Cons: no ring if ESP/relay fails in custom mode; more parts

### Option C — Parallel tap only (v1 on `matter` branch)

Optocoupler listens in parallel; CA audio path untouched. No custom melody.

## Hardware Reference (from adapter spec §2.3)

```
GPIO18 ──[ 1 kΩ ]──> BC547 base
BC547 emitter ──> CA terminal (when switched to custom path)
```

## Open Questions

- [ ] Urmet CS1132 PCB: exact CA trace tap points for bypass
- [ ] Match original buzzer amplitude/frequency or accept different sound
- [ ] RTTTL playback library vs PWM tone generator
- [ ] Sync custom mode with Tuya schedule vs physical switch

## Related

- [`adapter technical specification`](adapter%20technical%20specification) — full adapter wiring
- `matter` branch — v1 firmware without custom audio
