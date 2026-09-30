#!/usr/bin/env python3
"""PUF quality metrics for one SRAM bank.   usage: analyze.py 20020000 [dir]"""
import glob, sys
import numpy as np

addr = sys.argv[1]
d = sys.argv[2] if len(sys.argv) > 2 else "captures"
files = sorted(glob.glob(f"{d}/dump_{addr}_*.bin"))
if not files:
    sys.exit("no dumps found")
D = np.array([np.unpackbits(np.fromfile(f, dtype=np.uint8), bitorder="little")
              for f in files])
n, nb = D.shape
print(f"{n} captures, {nb} bits each")

if (D == 0).all() or (D == 1).all():
    sys.exit("ALL ZERO/ONE -> bank erased or ECC-initialised. Not usable as a PUF.")

p1 = D.mean(axis=0)
ref = (p1 > 0.5).astype(np.uint8)
flip = np.minimum(p1, 1 - p1)
print(f"Uniformity (fraction of 1s):     {D.mean():.3f}   (ideal ~0.5)")
print(f"Always-stable bits:              {((p1 == 0) | (p1 == 1)).mean():.3f}")
print(f"Bits with flip prob > 10%:       {(flip > 0.10).mean():.3f}")
ber = (D != ref).mean(axis=1)
print(f"BER vs majority: mean {ber.mean():.4f}, worst {ber.max():.4f}")
hd = [(D[i] != D[j]).mean() for i in range(n) for j in range(i + 1, n)]
print(f"Intra-chip Hamming distance:     {np.mean(hd):.4f}")
print(f"Min-entropy estimate / bit:      "
      f"{-np.log2(np.maximum(p1, 1 - p1).clip(max=0.999999)).mean():.3f} "
      f"(over the per-bit majority probability)")
print("Note: uniqueness (inter-chip HD) needs several boards; not measurable here.")
