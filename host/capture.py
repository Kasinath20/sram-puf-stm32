#!/usr/bin/env python3
"""Collect SRAM power-up dumps from the board (PUF_MODE_CAPTURE firmware)."""
import argparse, csv, os, struct, time, zlib
import serial
from serial.tools import list_ports

HDR = struct.Struct("<4sIII")
MAGIC = b"PUF1"
REGIONS = 2

def find_port():
    for p in list_ports.comports():
        if p.vid == 0x0483:                      # STMicroelectronics ST-LINK
            return p.device
    return None

def read_frame(ser):
    buf = b""
    while buf != MAGIC:                          # resync on magic
        c = ser.read(1)
        if not c:
            raise TimeoutError("no data")
        buf = (buf + c)[-4:]
    rest = ser.read(HDR.size - 4)
    _, rsr, addr, n = HDR.unpack(MAGIC + rest)
    data = ser.read(n)
    crc = struct.unpack("<I", ser.read(4))[0]
    if len(data) != n:
        raise TimeoutError("short frame")
    return rsr, addr, data, (zlib.crc32(data) & 0xFFFFFFFF) == crc

def capture(port, baud):
    with serial.Serial(port, baud, timeout=0.5) as ser:
        deadline = time.time() + 10
        got = []
        while time.time() < deadline:
            ser.write(b"G")
            try:
                got.append(read_frame(ser))
                break
            except TimeoutError:
                continue
        else:
            raise TimeoutError("board never answered")
        ser.timeout = 3
        for _ in range(REGIONS - 1):
            got.append(read_frame(ser))
        return got

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--out", default="captures")
    ap.add_argument("--n", type=int, default=50)
    a = ap.parse_args()

    os.makedirs(a.out, exist_ok=True)
    log_path = os.path.join(a.out, "log.csv")
    new = not os.path.exists(log_path)
    log = open(log_path, "a", newline="")
    w = csv.writer(log)
    if new:
        w.writerow(["idx", "time", "addr", "rsr_hex", "crc_ok"])
    idx = len([f for f in os.listdir(a.out) if f.startswith("dump_")]) // REGIONS

    while idx < a.n:
        print(f"\n[{idx+1}/{a.n}] UNPLUG the USB cable, wait 5 s, plug it back in...")
        while find_port():
            time.sleep(0.2)
        port = None
        while not port:
            port = find_port()
            time.sleep(0.1)
        time.sleep(0.3)
        try:
            frames = capture(port, a.baud)
        except Exception as e:
            print("  capture failed:", e)
            continue
        if not all(ok for *_, ok in frames):
            print("  CRC error, discarding this capture")
            continue
        for rsr, addr, data, ok in frames:
            fn = f"dump_{addr:08x}_{idx:04d}.bin"
            with open(os.path.join(a.out, fn), "wb") as f:
                f.write(data)
            w.writerow([idx, time.time(), hex(addr), hex(rsr), ok])
            print(f"  saved {fn}  rsr=0x{rsr:08x}")
        log.flush()
        idx += 1

if __name__ == "__main__":
    main()
