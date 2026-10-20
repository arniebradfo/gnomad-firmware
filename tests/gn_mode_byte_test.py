#!/usr/bin/env python3
# Gnomad, a fork of Felucca (see FORK.md). Not part of upstream Felucca.
"""Easy's bytes in the settings record (gn_easy.c: favourites' row 15, byte 30 the mode, byte 29 the song's slot,
bytes 17 to 28 its sequence and fingerprint) are ours only while
upstream never uses it. Upstream keeps its own byte beside it (ui_layer.c layer_seen, row 15 byte 31), so a later
upstream tag could take ours. This test reads upstream's sources (firmware/src, every file but the fork's gn_*) and
fails when:
  - anything indexes favourites' factory rows other than the ways 1.0.1 does: by an engine variable
    (favorites.c), the retired DIGITAL row and FM6's (settings_persist.c), and layer_seen's [15][31] (ui_layer.c);
  - anything names row 15 or bytes 17 to 30 of it ([15], [15][25] .. [15][30], layer_seen moved or a second byte
    beside it);
  - favourites' layout changes (factory[16][32] in favorites.c and in settings_persist.c's persist_t);
  - NENGINES can reach 15 (core.h: 13 + FELUCCA_SLICE): row 15 would then be an engine's or the user row's
    neighbour (favorite_set: engine == NENGINES is the user bank). The firmware asserts the same at compile time.
tests/run_fork_tests.sh runs it, so an upstream-sync pull request that brings any of these goes red.
  python3 tests/gn_mode_byte_test.py [firmware/src]"""
import re
import sys
from pathlib import Path

src = Path(sys.argv[1] if len(sys.argv) > 1 else "firmware/src")
ALLOWED = {                                   # (file, the subscript text after "factory[") as 1.0.1 has them
    ("favorites.c", "engine]"),
    ("settings_persist.c", "ENGI_DIGITAL]"),
    ("settings_persist.c", "ENGI_FM6]"),
    ("ui_layer.c", "15][31]"),
}
fails = []
files = sorted(p for p in src.iterdir() if p.suffix in (".c", ".h") and not p.name.startswith("gn_"))
for p in files:
    text = p.read_text(errors="replace")
    for m in re.finditer(r"factory\s*\[\s*([^\]]*)\]\s*(\[\s*[^\]]*\])?", text):
        sub = re.sub(r"\s+", "", m.group(1)) + "]" + (re.sub(r"\s+", "", m.group(2)) if m.group(2) else "")
        line = text.count("\n", 0, m.start()) + 1
        if "[16][32]" in m.group(0) or sub.startswith("16]"):
            continue                          # the declarations, checked below
        key = (p.name, sub if sub.startswith("15]") else sub.split("[")[0])
        if key not in ALLOWED:
            fails.append(f"{p.name}:{line}: favourites' factory rows indexed in a way 1.0.1 does not: {m.group(0)!r}")
    for m in re.finditer(r"\[\s*15\s*\]\s*\[\s*(?:1[7-9]|2[0-9]|30)\s*\]|layer_seen\s*\(|#define\s+layer_seen\b.*", text):
        line = text.count("\n", 0, m.start()) + 1
        if m.group(0).startswith("#define") and "favorites.factory[15][31]" in m.group(0):
            continue
        fails.append(f"{p.name}:{line}: row 15's bytes 17 to 30 or layer_seen's place: {m.group(0)!r}")
for name in ("favorites.c", "settings_persist.c"):
    t = (src / name).read_text()
    if not re.search(r"uint8_t\s+factory\s*\[\s*16\s*\]\s*\[\s*32\s*\]", t):
        fails.append(f"{name}: favourites' factory rows are no longer [16][32]")
core = (src / "core.h").read_text()
m = re.search(r"#define\s+NENGINES\s+\(\s*(\d+)\s*\+\s*FELUCCA_SLICE\s*\)", core)
if not m:
    fails.append("core.h: NENGINES is no longer (N + FELUCCA_SLICE): read it again by hand")
elif int(m.group(1)) + 1 >= 15:
    fails.append(f"core.h: NENGINES can reach {int(m.group(1)) + 1}: row 15 is no longer no engine's")
for f in fails:
    print("FAIL", f)
print(f"Easy's bytes (favourites' row 15, bytes 17 to 30): {len(files)} upstream sources read, "
      + ("upstream leaves it alone: ok" if not fails else f"{len(fails)} problem(s)"))
sys.exit(1 if fails else 0)
