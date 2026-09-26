#!/usr/bin/env python3
"""Convert the preview tool's 1-bpp BMPs to PNGs (same names, .png).

BMPs stay universal but awkward; PNGs open everywhere (browsers, editors,
AI tooling). Run from the repo root: python tools/preview/to_png.py
"""
import sys
from pathlib import Path

from PIL import Image


def main() -> int:
    root = Path(__file__).resolve().parent.parent.parent / "out" / "preview"
    if not root.is_dir():
        print(f"no preview dir: {root}", file=sys.stderr)
        return 1
    n = 0
    for bmp in sorted(root.glob("*.bmp")):
        png = bmp.with_suffix(".png")
        Image.open(bmp).save(png)
        bmp.unlink()
        n += 1
    print(f"converted {n} bmp -> png in {root}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
