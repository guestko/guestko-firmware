#!/usr/bin/env python3
"""Generate additive 50 Hz + 850 Hz waveform CSV for FeelTech FY6300 arbitrary upload."""

import math
import os

F1, F2 = 50.0, 850.0
MIX_F1, MIX_F2 = 0.7, 0.3
FS = 100_000  # Hz
T = 1.0 / F1  # one period (20 ms); 850 = 17 × 50
DT = 1.0 / FS

samples = []
t = 0.0
while t < T:
    val = MIX_F1 * math.sin(2 * math.pi * F1 * t) + MIX_F2 * math.sin(
        2 * math.pi * F2 * t
    )
    samples.append(val)
    t += DT

peak = max(abs(v) for v in samples)
samples = [v / peak for v in samples]

here = os.path.dirname(os.path.abspath(__file__))
out = os.path.join(here, "urmet_50_850.csv")
with open(out, "w", encoding="ascii") as f:
    for v in samples:
        f.write(f"{v:.6f}\n")

print(f"Written {len(samples)} samples to {out}")
print(f"Period {T * 1000:.1f} ms, FS {FS} Hz, range [{min(samples):.3f}, {max(samples):.3f}]")
