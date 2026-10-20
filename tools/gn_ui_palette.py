# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Kyle Palaniuk
# Gnomad, a fork of Felucca (see FORK.md). Not part of upstream Felucca.
"""Gnomad: the ninth palette, GNOMAD, for tools/gen_ui_palettes.py (two lines there call emit()).

The record's name is the macro GN_NAME (firmware/src/gn_skin.h), the one constant the product's name on the synth
comes from. Only a build with GSK_BUILD (firmware/src/felucca.c) has it: build/gen/ui_palettes.h holds the record and the
count under #if GSK_BUILD, so every other compile (upstream's host tests include the sources without GSK_BUILD)
sees upstream's eight palettes unchanged, byte for byte. The record's five colours are only the base of
gfx.c's derivation: firmware/src/gn_skin.c then sets all sixteen roles exactly (option C, section 2), so the
generator's contrast checks, which are about derived palettes, do not apply to it; tests/gn_skin_test.c checks
GNOMAD's own pairings instead. Saved settings store GNOMAD as the legacy PAPER id 13 (gn_skin.c), never as
UI_PAL_TAG + 8, which unmodified 1.0.1 would refuse.
"""

# name (a C expression), bg (paper), surf (ground), text (ink), theme (ink), accent (ink): 8-bit RGB
GNOMAD = ("GN_NAME", (0xF9, 0xF8, 0xF4), (0xEE, 0xED, 0xE8), (0x17, 0x18, 0x1B), (0x17, 0x18, 0x1B),
          (0x17, 0x18, 0x1B))


def emit(out, to565):
    """out: the header's lines as gen_ui_palettes.py made them; returns them with the GNOMAD record before the
    end of UI_PALETTES and UI_NPALETTES one more, both under #if GSK_BUILD; nothing else changes"""
    end = out.index("};")                                   # the end of UI_PALETTES, the header's first table
    rec = f"    {{{GNOMAD[0]}, " + ", ".join(f"0x{to565(c):04x}" for c in GNOMAD[1:]) + "},"
    out = out[:end] + ["#if GSK_BUILD", rec, "#endif"] + out[end:]
    k = next(i for i, line in enumerate(out) if line.startswith("#define UI_NPALETTES "))
    n = int(out[k].split()[2].rstrip("u"))
    out[k:k + 1] = ["#if GSK_BUILD", f"#define UI_NPALETTES {n + 1}u          /* Gnomad (FORK.md): GNOMAD, the ninth */",
                    "#else", out[k], "#endif"]
    return out
