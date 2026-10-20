#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Kyle Palaniuk
"""Gnomad (FORK.md, "Gnomad engines", round FW-CONTENT): the vendored content (firmware/gnomad-content) is what
CONTENT-VERSION.txt says: every file it lists has that SHA-256, no other file is in the folder (but the record and the
sync script), a source is named; and gn_content.h, the arrays the firmware compiles, is tools/gn_gen_content.py's
output for these files byte for byte (CI regenerates it here, as sync-content.sh made it); round CR: so is
gn_easy_set.h, tools/gn_gen_easy.py's output for easy.tsv (each row's id checked against the bank by its name). A hand edit of a bank, of
the PCM or of the header, or a re-vendor without its record, fails here; re-vendor with sync-content.sh.
  python3 tests/gn_content_test.py      (the repo root; run_fork_tests.sh)"""
import hashlib
import re
import subprocess
import sys
import tempfile
from pathlib import Path

repo = Path(__file__).resolve().parent.parent
root = repo / "firmware" / "gnomad-content"
text = (root / "CONTENT-VERSION.txt").read_text()
bad = []
if not re.search(r"^source   \S", text, re.M) or "unknown (set SOURCE=)" in text:
    bad.append("CONTENT-VERSION.txt names no source")
listed = dict((p, h) for h, p in re.findall(r"^  ([0-9a-f]{64})  (\S+)$", text, re.M))
for p, h in sorted(listed.items()):
    f = root / p
    if not f.is_file():
        bad.append(f"{p}: listed, missing")
    elif hashlib.sha256(f.read_bytes()).hexdigest() != h:
        bad.append(f"{p}: not the vendored bytes")
for f in sorted(root.iterdir()):
    if f.name not in listed and f.name not in ("CONTENT-VERSION.txt", "sync-content.sh"):
        bad.append(f"{f.name}: not in CONTENT-VERSION.txt")
for p in ("pitched.gcpbank", "kits.gckbank", "factory.manifest", "gn_content.h", "easy.tsv", "gn_easy_set.h"):
    if p not in listed:
        bad.append(f"{p}: not in CONTENT-VERSION.txt")
with tempfile.TemporaryDirectory() as tmp:
    out = Path(tmp) / "gn_content.h"
    r = subprocess.run([sys.executable, "-I", str(repo / "tools" / "gn_gen_content.py"), str(root), str(out)],
                       capture_output=True, text=True)
    if r.returncode or not out.is_file():
        bad.append("tools/gn_gen_content.py refuses the vendored files: " + (r.stderr or r.stdout).strip())
    elif out.read_bytes() != (root / "gn_content.h").read_bytes():
        bad.append("gn_content.h is not tools/gn_gen_content.py's output for these files (run sync-content.sh)")
    out = Path(tmp) / "gn_easy_set.h"
    r = subprocess.run([sys.executable, "-I", str(repo / "tools" / "gn_gen_easy.py"), str(root), str(out)],
                       capture_output=True, text=True)
    if r.returncode or not out.is_file():
        bad.append("tools/gn_gen_easy.py refuses the vendored easy.tsv: " + (r.stderr or r.stdout).strip())
    elif out.read_bytes() != (root / "gn_easy_set.h").read_bytes():
        bad.append("gn_easy_set.h is not tools/gn_gen_easy.py's output for easy.tsv (run sync-content.sh)")
print(f"vendored content: {len(listed)} files checked against CONTENT-VERSION.txt, gn_content.h and gn_easy_set.h regenerated" +
      (" (the same bytes)" if not bad else ": " + "; ".join(bad)))
sys.exit(1 if bad else 0)
