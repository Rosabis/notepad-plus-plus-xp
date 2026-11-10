#!/usr/bin/env python3
"""Convert PNG-compressed .ico files to BMP-format .ico files.

Windows XP's LoadImage and CreateIconFromResourceEx cannot decode PNG
entries inside an ICO file (PNG-in-ICO support was added in Windows Vista).
This script rewrites every .ico in the project to use BMP entries so the
icons can load on both XP and modern Windows.
"""

import os
import sys
import glob
from PIL import Image

ROOT = r"C:\Users\Arthur\Documents\GitHub\notepad-plus-plus-xp\PowerEditor\src\icons"

def is_png_ico(path):
    """Return True if the ICO file contains PNG-compressed entries."""
    try:
        with open(path, "rb") as f:
            data = f.read()
        if len(data) < 22:
            return False
        # ICONDIR.idType should be 1
        if int.from_bytes(data[2:4], "little") != 1:
            return False
        count = int.from_bytes(data[4:6], "little")
        png_magic = b"\x89PNG"
        for i in range(count):
            entry_offset = 6 + i * 16
            if entry_offset + 16 > len(data):
                return False
            # dwImageOffset is at entry_offset + 12, 4 bytes
            data_offset = int.from_bytes(data[entry_offset + 12:entry_offset + 16], "little")
            if data_offset + 4 > len(data):
                continue
            magic = data[data_offset:data_offset + 4]
            if magic == png_magic:
                return True
        return False
    except Exception as e:
        print(f"  ! read failed: {e}", file=sys.stderr)
        return False


def convert(path):
    """Rewrite a single .ico to BMP entries. Returns True if changed."""
    # Strip read-only attribute (git often stores these as +r)
    try:
        import stat
        current = os.stat(path).st_mode
        if not (current & stat.S_IWRITE):
            os.chmod(path, current | stat.S_IWRITE)
    except Exception:
        pass

    try:
        img = Image.open(path)
    except Exception as e:
        print(f"  ! open failed: {e}", file=sys.stderr)
        return False

    frames = []
    try:
        n = 0
        while True:
            img.seek(n)
            frames.append(img.copy())
            n += 1
    except EOFError:
        pass

    sizes = [(f.width, f.height) for f in frames]
    # Write directly to the target file (Pillow opens for write)
    try:
        # bitmap_format='bmp' forces BMP entries (instead of PNG)
        # PNG-in-ico is supported on Vista+ but NOT on XP -- this is the
        # whole reason we are doing this conversion.
        with open(path, "wb") as fh:
            img.save(fh, format="ICO", sizes=sizes, bitmap_format="bmp")
    except Exception as e:
        print(f"  ! save failed: {e}", file=sys.stderr)
        return False

    return True


def main():
    if not os.path.isdir(ROOT):
        print(f"Directory not found: {ROOT}", file=sys.stderr)
        sys.exit(1)

    png_count = 0
    converted = 0
    skipped = 0
    failed = 0
    total = 0

    for dirpath, _dirs, files in os.walk(ROOT):
        for name in files:
            if not name.lower().endswith(".ico"):
                continue
            full = os.path.join(dirpath, name)
            total += 1
            if is_png_ico(full):
                png_count += 1
                if convert(full):
                    converted += 1
                    print(f"  converted: {full}")
                else:
                    failed += 1
            else:
                skipped += 1

    print()
    print(f"Total .ico files scanned: {total}")
    print(f"PNG-compressed (need conversion): {png_count}")
    print(f"Already BMP (skipped):          {skipped}")
    print(f"Converted to BMP:                {converted}")
    print(f"Failed:                          {failed}")


if __name__ == "__main__":
    main()