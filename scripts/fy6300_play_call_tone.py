#!/usr/bin/env python3
"""Generate and play intercom call tones on FeelTech FY6300 via fygen."""

from __future__ import annotations

import argparse
import glob
import math
import os
import sys
from dataclasses import dataclass

FYGEN_DIR = os.path.expanduser("~/WebstormProjects/fygen")
DEVICE_SAMPLES = 8192
WAVEFORM_INDEX = 1
FS = 100_000  # sample rate for one sweep period


@dataclass(frozen=True)
class CallToneProfile:
    id: str
    label: str
    f1_hz: float
    f2_hz: float
    sweep_hz: float
    volts: float = 8.0
    source: str = ""

    @property
    def playback_hz(self) -> float:
        return self.sweep_hz


PROFILES: dict[str, CallToneProfile] = {
    "a": CallToneProfile(
        id="787_1",
        label="Urmet 787/1",
        f1_hz=500.0,
        f2_hz=800.0,
        sweep_hz=11.0,
        source="787/1 PDF: F1=500, F2=800, sweep 10-12 Hz",
    ),
    "e3": CallToneProfile(
        id="787_1_f850",
        label="787/1 + F2=850 (user tune)",
        f1_hz=500.0,
        f2_hz=850.0,
        sweep_hz=11.0,
        source="After E: similar character, user says real ring brighter ~850 Hz",
    ),
    "e": CallToneProfile(
        id="787_2",
        label="Urmet 787/2",
        f1_hz=500.0,
        f2_hz=820.0,
        sweep_hz=12.0,
        source="787/1 PDF (787/2 variant): F1=500, F2=820, sweep 12 Hz",
    ),
    "v2": CallToneProfile(
        id="753_841",
        label="Video-measured 753/841",
        f1_hz=753.0,
        f2_hz=841.0,
        sweep_hz=12.0,
        source="Ring video peak measurement: F1=753, F2=841, sweep 12 Hz",
    ),
}


def get_profile(key: str) -> CallToneProfile:
    k = key.lower().strip()
    if k not in PROFILES:
        keys = ", ".join(sorted(PROFILES))
        raise KeyError(f"Unknown profile {key!r}. Choose one of: {keys}")
    return PROFILES[k]


def _triangle(t: float, sweep_hz: float) -> float:
    x = (t * sweep_hz) % 1.0
    if x < 0.5:
        return 4.0 * x - 1.0
    return 3.0 - 4.0 * x


def generate_sweep(f1: float, f2: float, sweep_hz: float) -> list[float]:
    period = 1.0 / sweep_hz
    dt = 1.0 / FS
    samples: list[float] = []
    phase = 0.0
    t = 0.0
    while t < period:
        tri = _triangle(t + dt * 0.5, sweep_hz)
        f_inst = (f1 + f2) * 0.5 + tri * (f2 - f1) * 0.5
        phase += 2.0 * math.pi * f_inst * dt
        samples.append(math.sin(phase))
        t += dt
    return samples


def generate_am(f_hz: float, mod_hz: float, sweep_hz: float) -> list[float]:
    period = 1.0 / sweep_hz
    dt = 1.0 / FS
    samples: list[float] = []
    t = 0.0
    while t < period:
        carrier = math.sin(2.0 * math.pi * f_hz * t)
        am = 0.35 + 0.65 * (0.5 + 0.5 * math.sin(2.0 * math.pi * mod_hz * t))
        samples.append(carrier * am)
        t += dt
    return samples


def normalize(values: list[float]) -> list[float]:
    peak = max(abs(v) for v in values) or 1.0
    return [v / peak for v in values]


def generate_waveform(profile: CallToneProfile) -> list[float]:
    if profile.f1_hz == profile.f2_hz:
        values = generate_am(profile.f1_hz, profile.sweep_hz, profile.sweep_hz)
    else:
        values = generate_sweep(profile.f1_hz, profile.f2_hz, profile.sweep_hz)
    return normalize(values)


def resample_period(values: list[float], target: int) -> list[float]:
    src = len(values)
    if target == src:
        return values
    out: list[float] = []
    for i in range(target):
        pos = i * src / target
        idx = int(pos) % src
        frac = pos - int(pos)
        nxt = (idx + 1) % src
        out.append(values[idx] * (1.0 - frac) + values[nxt] * frac)
    return out


def _ensure_fygen() -> None:
    if not os.path.isdir(FYGEN_DIR):
        sys.exit(
            f"fygen not found at {FYGEN_DIR}.\n"
            "Clone it: git clone https://github.com/mattwach/fygen.git "
            f"{FYGEN_DIR}"
        )
    if FYGEN_DIR not in sys.path:
        sys.path.insert(0, FYGEN_DIR)


def guess_port() -> str | None:
    for pattern in ("/dev/cu.wchusbserial*", "/dev/cu.usbserial*", "/dev/cu.SLAB_USBtoUART*"):
        matches = sorted(glob.glob(pattern))
        if matches:
            return matches[0]
    return None


def resolve_port(explicit: str | None) -> str:
    if explicit:
        return explicit
    env = os.environ.get("FYSERIAL")
    if env:
        return env
    guessed = guess_port()
    if guessed:
        return guessed
    sys.exit(
        "Serial port not specified and FY6300 not detected.\n"
        "Connect FY6300 via USB, then run: ls /dev/cu.*\n"
        "Retry with: --port /dev/cu.usbserialXXXX"
    )


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Generate and play intercom call tone on FeelTech FY6300 (CH1)."
    )
    parser.add_argument(
        "--profile",
        default="e3",
        choices=sorted(PROFILES),
        help="Tone profile (default: e3 = 787/1 + F2=850)",
    )
    parser.add_argument("--port", help="USB serial port")
    parser.add_argument("--volts", type=float, help="Override Vpp (default from profile)")
    parser.add_argument("--debug", action="store_true")
    parser.add_argument("--dry-run", action="store_true")
    args = parser.parse_args()

    profile = get_profile(args.profile)
    volts = args.volts if args.volts is not None else profile.volts
    playback_hz = profile.playback_hz

    values = generate_waveform(profile)
    upload = resample_period(values, DEVICE_SAMPLES)
    print(f"Profile [{args.profile}] {profile.label}")
    print(f"  {profile.f1_hz:g} / {profile.f2_hz:g} Hz, sweep {profile.sweep_hz:g} Hz")
    print(f"  Source: {profile.source}")
    print(f"Generated {len(values)} samples, period {1000 / profile.sweep_hz:.1f} ms")
    print(f"Resampled to {len(upload)} points, range [{min(upload):.3f}, {max(upload):.3f}]")

    if args.dry_run:
        print("Dry run OK.")
        return

    _ensure_fygen()
    import fygen  # noqa: WPS433

    port = resolve_port(args.port)
    print(f"Connecting to {port} ...")
    fy = fygen.FYGen(port, device_name="fy6300", debug_level=1 if args.debug else 0, timeout=30)
    try:
        fy.set(channel=fygen.CH1, enable=False)
        print(f"Uploading arb{WAVEFORM_INDEX} ({len(upload)} points) ...")
        fy.set_waveform(WAVEFORM_INDEX, values=upload, value_count=DEVICE_SAMPLES)
        fy.set(
            channel=fygen.CH1,
            wave=f"arb{WAVEFORM_INDEX}",
            freq_hz=playback_hz,
            volts=volts,
            offset_volts=0,
            enable=True,
        )
        print(
            f"OK: CH1 arb{WAVEFORM_INDEX}, playback {playback_hz:g} Hz, "
            f"{volts:g} Vpp, offset 0 V, output ON"
        )
    finally:
        fy.close()


if __name__ == "__main__":
    main()
