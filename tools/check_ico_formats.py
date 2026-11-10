#!/usr/bin/env python3
"""Count ICO files by their first entry's image format."""
import os
ROOT = r"C:\Users\Arthur\Documents\GitHub\notepad-plus-plus-xp\PowerEditor\src\icons"

png = b"\x89PNG"
bmp_magic = b"\x28\x00\x00\x00"

png_n = bmp_n = other_n = 0
samples = []
for d, _, files in os.walk(ROOT):
    for f in files:
        if not f.lower().endswith(".ico"):
            continue
        p = os.path.join(d, f)
        with open(p, "rb") as fh:
            data = fh.read()
        if len(data) < 22:
            continue
        count = int.from_bytes(data[4:6], "little")
        if count == 0:
            continue
        # ICONDIRENTRY at offset 6 + i*16
        # dwBytesInRes at +8 (4 bytes), dwImageOffset at +12 (4 bytes)
        first_off = int.from_bytes(data[18:22], "little")
        if first_off + 4 > len(data):
            continue
        magic = data[first_off:first_off + 4]
        if magic == png:
            png_n += 1
            if len(samples) < 5:
                samples.append(("PNG", p))
        elif magic[:2] == bmp_magic[:2]:
            bmp_n += 1
            if len(samples) < 5:
                samples.append(("BMP", p))
        else:
            other_n += 1
            if len(samples) < 5:
                samples.append((f"OTHER({magic.hex()})", p))

print(f"Total scanned (with entries): {png_n + bmp_n + other_n}")
print(f"  PNG-compressed entries:     {png_n}")
print(f"  BMP-compressed entries:      {bmp_n}")
print(f"  Other format:                {other_n}")
print()
print("First few samples:")
for kind, p in samples:
    print(f"  [{kind}] {p}")