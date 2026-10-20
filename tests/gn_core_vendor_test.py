#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Kyle Palaniuk
"""Gnomad (FORK.md, "Gnomad engines"): the vendored core (firmware/gnomad-core) is what CORE-VERSION.txt says:
every file it lists has that SHA-256, no other file is in include/ or src/, and the commit is named (40 hex
digits). The open-source code the core adapts (third_party/, CORE-NOTICE) is held to the same record. A hand edit of the core, or a re-vendor without its record, fails here; re-vendor with sync-core.sh."""
import hashlib
import re
import sys
from pathlib import Path

root = Path(__file__).resolve().parent.parent / "firmware" / "gnomad-core"
text = (root / "CORE-VERSION.txt").read_text()
bad = []
if not re.search(r"^commit   [0-9a-f]{40}$", text, re.M):
    bad.append("CORE-VERSION.txt names no commit")
listed = dict((p, h) for h, p in re.findall(r"^  ([0-9a-f]{64})  (\S+)$", text, re.M))
for p, h in sorted(listed.items()):
    f = root / p
    if not f.is_file():
        bad.append(f"{p}: listed, missing")
    elif hashlib.sha256(f.read_bytes()).hexdigest() != h:
        bad.append(f"{p}: not the vendored bytes")
for f in sorted([*root.glob("include/*"), *root.glob("src/*"), *(p for p in root.glob("third_party/**/*") if p.is_file())]):
    if str(f.relative_to(root)) not in listed:
        bad.append(f"{f.relative_to(root)}: not in CORE-VERSION.txt")
for p in ("gn_core_unity.h", "gn_core_records.h"):
    if p not in listed:
        bad.append(f"{p}: not in CORE-VERSION.txt")
# GC_DRUM_LAB is the core's lab-only drum build: its render path divides (code review, 2026-10-12). Nothing in the
# firmware's build (flags in tools/, build.sh, the workflows, tests/, firmware/: headers, sources, the unity header)
# may define it; gn_core_unity.h also stops the compile with #error if it ever is.
LAB = "GC_" + "DRUM_LAB"                              # split, so this file does not match itself
lab_def = re.compile(r"(-D\s*" + LAB + r"\b|#\s*define\s+" + LAB + r"\b|\b" + LAB + r"\s*=|[\"']" + LAB + r"[\"'])")
repo = root.parent.parent
lab_hits = []
for top in ("firmware", "tools", "tests", ".github", "build.sh"):
    base = repo / top
    for f in ([base] if base.is_file() else sorted(p for p in base.rglob("*") if p.is_file())):
        if f.suffix not in ("", ".c", ".h", ".py", ".sh", ".yml", ".yaml", ".ld", ".txt", ".mk", ".json") or f.name == "CORE-VERSION.txt":
            continue
        try:
            for n, line in enumerate(f.read_text(errors="replace").splitlines(), 1):
                if lab_def.search(line):
                    lab_hits.append(f"{f.relative_to(repo)}:{n}")
        except OSError:
            pass
if lab_hits:
    bad.append(f"{LAB} (the core's lab-only drum build, division in the render path) is defined for the firmware at "
               + ", ".join(lab_hits))
unity = (root / "gn_core_unity.h").read_text()
if "#ifdef " + LAB not in unity or "#error" not in unity:
    bad.append(f"gn_core_unity.h lacks the #error guard against {LAB} (re-vendor with sync-core.sh)")
print(f"vendored core: {len(listed)} files checked against CORE-VERSION.txt, {LAB} checked in the firmware's build"
      + ("" if not bad else ": " + "; ".join(bad)))
sys.exit(1 if bad else 0)
