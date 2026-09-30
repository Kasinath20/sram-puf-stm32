#!/usr/bin/env python3
"""Simulated SRAM PUF for testing the pipeline without hardware."""
import argparse, os
import numpy as np

ap = argparse.ArgumentParser()
ap.add_argument("--out", default="captures_sim")
ap.add_argument("--addr", default="20020000")
ap.add_argument("--n", type=int, default=60)
ap.add_argument("--chip-seed", type=int, default=1)
ap.add_argument("--noise", type=float, default=0.2, help="noise sigma relative to mismatch sigma=1")
ap.add_argument("--bits", type=int, default=32768)
a = ap.parse_args()

os.makedirs(a.out, exist_ok=True)
chip = np.random.default_rng(a.chip_seed)
mismatch = chip.normal(0, 1, a.bits)               # per-cell manufacturing offset
noise = np.random.default_rng()                    # fresh noise every boot
for k in range(a.n):
    bits = ((mismatch + noise.normal(0, a.noise, a.bits)) > 0).astype(np.uint8)
    np.packbits(bits, bitorder="little").tofile(f"{a.out}/dump_{a.addr}_{k:04d}.bin")
print(f"wrote {a.n} simulated dumps to {a.out}/")
