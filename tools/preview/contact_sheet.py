#!/usr/bin/env python3
"""Assemble the preview PNGs into labeled contact sheets (one per
orientation) so a whole design iteration can be reviewed at a glance.

Usage: python tools/preview/contact_sheet.py [orientation]
"""
import sys
from pathlib import Path

from PIL import Image, ImageDraw

PREVIEW = Path(__file__).resolve().parent.parent.parent / "out" / "preview"
SCALES = {0: 0.5, 1: 0.35, 2: 0.5, 3: 0.35}
COLS = 3
LABEL_H = 26


def main() -> int:
    orient = int(sys.argv[1]) if len(sys.argv) > 1 else 0
    pngs = sorted(p for p in PREVIEW.glob(f"*_o{orient}.png")
                  if not p.stem.startswith(("sheet", "_")))
    if not pngs:
        print(f"no *_o{orient}.png in {PREVIEW}", file=sys.stderr)
        return 1

    scale = SCALES[orient]
    first = Image.open(pngs[0])
    tw = int(first.width * scale)
    th = int(first.height * scale)
    rows = (len(pngs) + COLS - 1) // COLS
    sheet = Image.new("L", (COLS * (tw + 8) + 8, rows * (th + LABEL_H + 8) + 8), 255)
    d = ImageDraw.Draw(sheet)

    for i, p in enumerate(pngs):
        tile = Image.open(p).resize((tw, th))
        x = 8 + (i % COLS) * (tw + 8)
        y = 8 + (i // COLS) * (th + LABEL_H + 8)
        sheet.paste(tile, (x, y))
        d.rectangle([x, y, x + tw - 1, y + th - 1], outline=0)
        d.text((x + 2, y + th + 5), p.stem, fill=0)

    out = PREVIEW / f"sheet_o{orient}.png"
    sheet.save(out)
    print(f"{out}: {len(pngs)} tiles, {sheet.width}x{sheet.height}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
