#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Kyle Palaniuk
# Gnomad, a fork of Felucca (see FORK.md). Not part of upstream Felucca.
"""The skin's pictures for review (Pillow), from tests/gn_skin_render.c sheet SRC:
  gn_skin_sheet.py SRC OUT
  OUT/<name>.png          each screen at the synth's 240 x 240
  OUT/2x/<name>.png       the same, twice as large (nearest neighbour: every pixel as drawn)
  OUT/sheet.png           every screen at 240 x 240 with its page name under it (the index's order)
  OUT/index.txt           the pictures and their names
"""
import sys
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

src, out = Path(sys.argv[1]), Path(sys.argv[2])
(out / "2x").mkdir(parents=True, exist_ok=True)
# the labels are the firmware's 8-bit text (its titles: 0xB7 the middle dot, 0x85 the ellipsis): cp1252
rows = [line.split("\t", 1) for line in (src / "index.tsv").read_bytes().decode("cp1252").splitlines() if "\t" in line]
try:
    font = ImageFont.load_default(size=14)
except TypeError:                                   # (an older Pillow: its bitmap font)
    font = ImageFont.load_default()
shots = []
for f, label in rows:
    img = Image.open(src / f).convert("RGB")
    name = Path(f).stem
    img.save(out / f"{name}.png")
    img.resize((480, 480), Image.NEAREST).save(out / "2x" / f"{name}.png")
    shots.append((name, label, img))


def wrap(d, text, width):
    words, lines, cur = text.split(" "), [], ""
    for w in words:
        t = (cur + " " + w).strip()
        if d.textlength(t, font=font) <= width or not cur:
            cur = t
        else:
            lines.append(cur)
            cur = w
    return lines + [cur]


cols, cw, ch, pad = 8, 252, 292, 12
sheet = Image.new("RGB", (cols * cw + pad, ((len(shots) + cols - 1) // cols) * ch + pad + 30), (52, 53, 56))
d = ImageDraw.Draw(sheet)
d.text((pad, 8), f"Gnomad skin, phase 1: GNOMAD on, {len(shots)} screens at the synth's 240 px (page name, Felucca's "
       "code in brackets)", fill=(240, 240, 236), font=font)
for i, (name, label, img) in enumerate(shots):
    x, y = pad + (i % cols) * cw, 34 + (i // cols) * ch
    sheet.paste(img, (x, y))
    for k, line in enumerate(wrap(d, label, 240)[:2]):
        d.text((x, y + 244 + k * 17), line, fill=(240, 240, 236), font=font)
sheet.save(out / "sheet.png")
(out / "index.txt").write_text("".join(f"{n}.png\t{l}\n" for n, l, _ in shots))
print(f"gn_skin_sheet: {len(shots)} screens, their 2x copies and sheet.png in {out}")
