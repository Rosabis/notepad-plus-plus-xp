#!/usr/bin/env python3
"""Count remaining PNG icons after conversion."""
import os
ROOT = r"C:\Users\Arthur\Documents\GitHub\notepad-plus-plus-xp\PowerEditor\src\icons"
png_magic = b"\x89PNG"
remaining = 0
total = 0
for d, _, files in os.walk(ROOT):
    for f in files:
        if not f.lower().endswith(".ico"):
            continue
        total += 1
        p = os.path.join(d, f)
        with open(p, "rb") as fh:
            data = fh.read()
        if len(data) < 22:
            continue
        first_off = int.from_bytes(data[18:22], "little")
        if first_off + 4 > len(data):
            continue
        magic = data[first_off:first_off + 4]
        if magic == png_magic:
            remaining += 1
print(f"Total: {total}, Still PNG: {remaining}")