#!/usr/bin/env python3
"""Test Pillow ICO save with bitmap_format='bmp' option."""
from PIL import Image
import os
src = r"C:\Users\Arthur\Documents\GitHub\notepad-plus-plus-xp\PowerEditor\src\icons\moreOnTooltip.ico"
img = Image.open(src)
frames = []
n = 0
try:
    while True:
        img.seek(n)
        frames.append(img.copy())
        n += 1
except EOFError:
    pass
sizes = [(f.width, f.height) for f in frames]

for method, kw in [("default", {}), ("bmp_format", {"bitmap_format": "bmp"})]:
    out = rf"C:\Users\Arthur\tmp_test_{method}.ico"
    try:
        img.save(out, format="ICO", sizes=sizes, **kw)
        sz = os.path.getsize(out)
        with open(out, "rb") as f:
            d = f.read()
        off = int.from_bytes(d[18:22], "little")
        magic = d[off:off + 4].hex()
        is_png = magic == "89504e47"
        is_bmp = magic[:2] == "2800"
        print(f"{method}: size={sz}, magic@22={magic} (PNG={is_png} BMP={is_bmp})")
    except Exception as e:
        print(f"{method}: ERROR {e}")