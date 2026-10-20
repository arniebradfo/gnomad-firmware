#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Kyle Palaniuk
"""Gnomad (FORK.md, "Gnomad engines", round FW-CONTENT): the sound banks as const arrays in the firmware's flash.

  tools/gn_gen_content.py SRC OUT.h          (SRC: firmware/gnomad-content, the vendored files)
  tools/gn_gen_content.py SRC --list         (the census only: every authored slot, every skipped one)

SRC holds pitched.gcpbank (384 x 128 B, headerless, offset id*128), kits.gckbank (32 x 416 B, offset id*416) and the
factory PCM: factory.manifest ("<asset id> <name> frames <n> root <r> crc32c <c> sha256 <h>" lines, "total_bytes N")
and one <name>.pcm a line (PCM16 little endian, mono, 44100 Hz). Record formats: the core's SPEC/01 (gnomad-core),
bank layout SPEC/30, assets SPEC/14.

OUT.h (firmware/gnomad-content/gn_content.h, committed; sync-content.sh writes it, tests/gn_content_test.py checks it
is this script's output byte for byte) declares:
  GN_CONTENT_PITCHED[49152], GN_CONTENT_KITS[13312]   the banks as they are, every slot (unauthored ones are zero)
  GN_CONTENT_PCM[131072]                              the three assets one after another, in the manifest's order
  GN_CONTENT_ASSETS(X)    X(id, byte offset, frames, crc32c) for each asset
  GN_CONTENT_VA(X), _FM, _GNOME, _SAMPLE, _KITS
                          the authored slots of each engine in bank order (zero and malformed records skipped):
                          X(bank id, BRIT, WOBL, CHOR, DLY, REV): the record's stored Bright, Wobble, Chorus, Echo and
                          Space as the firmware's knob values (0..127) the shim (gn_core.c gnc_ctl) turns back into
                          the nearest values it can send; a kit's are the DRUMS carrier patch's (Bright 32768,
                          Wobble 0, the sends 16384: gn_core_records.h "Init Drums", which tests/gn_core_test.c checks).
                          The firmware's preset lists are the bank ids alone (gn_core.c, a uint16_t a preset, round
                          FW-CONTENT-7): it reads a preset's name and knobs from the record when asked
                          (gn_preset_get, the same nearest-knob search as knobs() below), and tests/gn_core_test.c
                          checks every preset's against the knob values listed here
  GN_CONTENT_P<id> / GN_CONTENT_K<id>
                          a bank sound's Felucca engine slot and preset index ("GN_ENGI_FM, 3"), for Easy's
                          sound list (gn_state.c): an id that is not authored does not compile.
  GN_CONTENT_PN_<NAME> / GN_CONTENT_KN_<NAME>, GN_CONTENT_ID_<NAME> / GN_CONTENT_KID_<NAME>
                          (round CR) the same by the sound's NAME (upper case, every other character "_": "Rain Chime"
                          is RAIN_CHIME), and its bank id: Easy's sounds name bank sounds by name, so a renumbered
                          bank keeps them right and a removed name does not compile. A name two records share gets no
                          macro (refused for a kit or a sound Easy names: it does not compile).
A record counts as authored when its magic, version, length, flags, CRC32C and name are well formed and its engine
fits the bank (pitched: VA FM GNOME SAMPLE; kits: pad count 16, kit ID = slot); the core's own validator decides the
rest at bind time (tests/gn_core_test.c runs gc_validate_patch / gc_validate_kit on every listed record and on every
skipped non-zero one). An engine with no authored slot is refused (a preset table cannot be empty)."""
import hashlib
import sys
from pathlib import Path

PATCH, KIT, NPITCHED, NKITS, PCM_TOTAL = 128, 416, 384, 32, 131072
ENGINES = {0: "VA", 1: "FM", 3: "GNOME", 4: "SAMPLE"}          # the core's gc_engine numbers in the pitched bank
ORDER = ("VA", "FM", "GNOME", "SAMPLE")
SLOT = {"VA": "GN_ENGI_VA", "FM": "GN_ENGI_FM", "GNOME": "GN_ENGI_GNOME", "SAMPLE": "GN_ENGI_SAMPLE",
        "KITS": "GN_ENGI_DRUMS"}                                # gn_core.c's slot names
GROUPS = ((0, 59), (60, 131), (132, 173), (174, 213), (214, 261), (262, 325), (326, 357), (358, 383))   # SPEC/30
# (gnomad-core 1.1.6, SPEC/CHANGES-1.1.6 C1 + FUNNY v2: Bass 60, Keys 72, Leads 42, Pads 40, Plucks 48, Gnome 64,
# Bells 32, Play 26; before it 64/72/48/40/48/64/32/16)
KIT_MACROS = (32768, 0, 16384, 16384, 16384)                    # the DRUMS carrier: Bright, Wobble, Echo, Space, Chorus


def crc32c(data):
    c = 0xFFFFFFFF
    for b in data:
        c ^= b
        for _ in range(8):
            c = (c >> 1) ^ (0x82F63B78 if c & 1 else 0)
    return c ^ 0xFFFFFFFF


def rd16(b, o):
    return b[o] | b[o + 1] << 8


def name_of(rec):
    """the record's name (bytes 16..31: 1..15 printable ASCII, then NUL and zero fill), or None"""
    f = rec[16:32]
    n = f.find(0)
    if n < 1 or n > 15 or any(c < 0x20 or c > 0x7E for c in f[:n]) or any(f[n:]):
        return None
    return f[:n].decode()


def sealed(rec, magic, length):
    """why rec is not a well-formed sealed record of its kind, or None"""
    if rec[0:4] != magic:
        return "magic"
    if rd16(rec, 4) != 2:
        return "version"
    if rd16(rec, 6) != length:
        return "length"
    if magic != b"GCP1" and (rec[10] or rec[11]):
        return "flags"                    # (a patch's bytes 10, 11 are its macro map, profile 0x00010300: core SPEC/CHANGES-1.3.0)
    if crc32c(rec[:12] + b"\0\0\0\0" + rec[16:]) != int.from_bytes(rec[12:16], "little"):
        return "crc"
    if name_of(rec) is None:
        return "name"
    return None


# the shim's knob laws (firmware/src/gn_core.c gnc_macro, gnc_ctl), and the knob value nearest a stored macro
def macro(v, centred):
    v = max(0, min(127, v))
    if centred:
        return v * 512 if v <= 64 else 32768 + (v - 64) * 32767 // 63
    return (v * 65535 + 63) // 127


def knob(target, law):
    return min(range(128), key=lambda v: (abs(law(v) - target), v))


def knobs(bright, wobble, echo, space, chorus):
    """-> BRIT, WOBL, CHOR, DLY, REV: the knob values whose sent values come nearest the stored macros
    (Space: the shim doubles the REV send, GN_CORE_SPACE_X 2, saturating)"""
    return (knob(bright, lambda v: macro(v, 1)), knob(wobble, lambda v: macro(v, 0)),
            knob(chorus, lambda v: macro(v, 0)), knob(echo, lambda v: macro(v, 0)),
            knob(space, lambda v: min(65535, macro(v, 0) * 2)))


def ident(name):
    """a bank name as a C identifier: upper case, every other character '_'"""
    return "".join(c if c.isalnum() else "_" for c in name.upper())


def census(src):
    pitched = (src / "pitched.gcpbank").read_bytes()
    kits = (src / "kits.gckbank").read_bytes()
    if len(pitched) != NPITCHED * PATCH or len(kits) != NKITS * KIT:
        sys.exit(f"gn_gen_content: pitched {len(pitched)} B, kits {len(kits)} B: not {NPITCHED * PATCH} and {NKITS * KIT}")
    lists = {e: [] for e in ORDER + ("KITS",)}
    skipped = []
    for i in range(NPITCHED):
        rec = pitched[i * PATCH:(i + 1) * PATCH]
        if not any(rec):
            continue
        why = sealed(rec, b"GCP1", PATCH)
        eng = ENGINES.get(rec[8])
        group = next(g for g, (lo, hi) in enumerate(GROUPS) if lo <= i <= hi)
        if why is None and eng is None:
            why = f"engine {rec[8]}"
        if why is None and rec[9] != group:
            why = f"group {rec[9]}, the slot's is {group}"
        if why:
            skipped.append(("pitched", i, why))
            continue
        p = [rd16(rec, 32 + 2 * k) for k in range(10)]
        lists[eng].append((i, name_of(rec), knobs(p[5], p[6], p[7], p[8], p[9])))
    for i in range(NKITS):
        rec = kits[i * KIT:(i + 1) * KIT]
        if not any(rec):
            continue
        why = sealed(rec, b"GCK1", KIT)
        if why is None and (rec[8] != 16 or rec[9] != i):
            why = f"pad count {rec[8]}, kit ID {rec[9]}"
        if why:
            skipped.append(("kits", i, why))
            continue
        lists["KITS"].append((i, name_of(rec), knobs(*KIT_MACROS)))
    return pitched, kits, lists, skipped


def assets(src):
    """the manifest's assets in order: (id, name, frames, crc32c, bytes); every check the manifest states"""
    out, total = [], None
    for line in (src / "factory.manifest").read_text().splitlines():
        t = line.split()
        if not t:
            continue
        if t[0] == "total_bytes":
            total = int(t[1])
            continue
        kv = dict(zip(t[2::2], t[3::2]))
        aid, name, frames, crc, sha = int(t[0]), t[1], int(kv["frames"]), int(kv["crc32c"]), kv["sha256"]
        data = (src / f"{name}.pcm").read_bytes()
        if len(data) != 2 * frames or crc32c(data) != crc or hashlib.sha256(data).hexdigest() != sha:
            sys.exit(f"gn_gen_content: {name}.pcm does not match factory.manifest (size, CRC32C or SHA-256)")
        if not 1 <= aid <= 65535 or not 1 <= frames <= 65535:
            sys.exit(f"gn_gen_content: asset {aid} ({name}): ID or frames out of range")
        out.append((aid, name, frames, crc, data))
    size = sum(len(a[4]) for a in out)
    if total != size or size != PCM_TOTAL or not 1 <= len(out) <= 3 or len({a[0] for a in out}) != len(out):
        sys.exit(f"gn_gen_content: the PCM is {size} B in {len(out)} assets (manifest total {total}); SPEC/30: "
                 f"{PCM_TOTAL} B in at most three, IDs unique")
    return out


def array(name, data, comment):
    rows = [",".join(str(b) for b in data[i:i + 32]) for i in range(0, len(data), 32)]
    return f"/* {comment} */\nstatic const uint8_t {name}[{len(data)}u] = {{\n" + ",\n".join(rows) + "};\n"


def header(src):
    pitched, kits, lists, skipped = census(src)
    pcm = assets(src)
    for e, l in lists.items():
        if not l:
            sys.exit(f"gn_gen_content: no authored {e} sound in the banks (a preset table cannot be empty)")
    h = ["/* SPDX-License-Identifier: GPL-3.0-only\n"
         " * Generated by tools/gn_gen_content.py from firmware/gnomad-content (sync-content.sh): do not edit. */\n",
         "/* Gnomad (FORK.md, \"Gnomad engines\", round FW-CONTENT): the factory sound banks in flash, as the core's\n"
         " * content (gnomad-core SPEC/30) has them; firmware/src/gn_core.c binds them (presets of the Gnomad engines). */\n",
         "#pragma once\n"]
    h.append(f"#define GN_CONTENT_PITCHED_BYTES {len(pitched)}u\n#define GN_CONTENT_KITS_BYTES {len(kits)}u\n"
             f"#define GN_CONTENT_PCM_BYTES {PCM_TOTAL}u\n")
    h.append("\n/* the authored slots of each engine, bank order: X(bank id, BRIT, WOBL, CHOR, DLY, REV). The firmware keeps\n"
             " * the ids (gn_core.c, a uint16_t a preset) and reads the name and knobs from the record; these knob values\n"
             " * are tests/gn_core_test.c's check of that */\n")
    for e, l in lists.items():
        names = ", ".join(f"{i} {n}" for i, n, _ in l)
        h.append(f"/* {e} ({len(l)}): {names} */\n#define GN_CONTENT_{e}_COUNT {len(l)}u\n")
        h.append(f"#define GN_CONTENT_{e}(X) " + " ".join(f"X({i}, {k[0]}, {k[1]}, {k[2]}, {k[3]}, {k[4]})"
                                                          for i, _, k in l) + "\n")
    h.append("\n/* a bank sound's slot and preset index, by bank id (gn_state.c, Easy's sounds) */\n")
    for e, l in lists.items():
        pre = "K" if e == "KITS" else "P"
        for n, (i, _, _) in enumerate(l):
            h.append(f"#define GN_CONTENT_{pre}{i} {SLOT[e]}, {n}\n")
    h.append("\n/* the same by name (round CR): GN_CONTENT_PN_<NAME> / KN_<NAME> \"slot, place\", GN_CONTENT_ID_<NAME> / KID_<NAME>\n"
             " * the bank id */\n")
    seen = {}
    for e, l in lists.items():
        for n, (i, nm, _) in enumerate(l):
            key = ("K" if e == "KITS" else "P", ident(nm))
            seen.setdefault(key, []).append((e, n, i))
    for (pre, nm), at in sorted(seen.items(), key=lambda kv: (kv[0][0] == "K", kv[1][0][2])):
        if len(at) != 1:
            h.append(f"/* {pre} {nm}: {len(at)} records share the name, no macro */\n")
            continue
        e, n, i = at[0]
        h.append(f"#define GN_CONTENT_{pre}N_{nm} {SLOT[e]}, {n}\n#define GN_CONTENT_{'K' if pre == 'K' else ''}ID_{nm} {i}u\n")
    h.append("\n/* the factory PCM: X(asset id, byte offset in GN_CONTENT_PCM, frames, crc32c) */\n#define GN_CONTENT_ASSETS(X)")
    off = 0
    for aid, name, frames, crc, data in pcm:
        h.append(f" X({aid}u, {off}u, {frames}u, 0x{crc:08x}u)")
        off += len(data)
    h.append("\n#define GN_CONTENT_NASSETS %du\n\n" % len(pcm))
    if skipped:
        h.append("/* skipped non-zero records: " + "; ".join(f"{b} {i} ({w})" for b, i, w in skipped) + " */\n")
    h.append(array("GN_CONTENT_PITCHED", pitched, "pitched.gcpbank: 384 records of 128 B, offset id * 128"))
    h.append(array("GN_CONTENT_KITS", kits, "kits.gckbank: 32 records of 416 B, offset id * 416"))
    h.append(array("GN_CONTENT_PCM", b"".join(a[4] for a in pcm),
                   "the factory PCM: " + ", ".join(f"asset {a[0]} {a[1]} {a[2]} frames" for a in pcm)))
    return "".join(h), lists, skipped


def main():
    if len(sys.argv) != 3:
        sys.exit("usage: tools/gn_gen_content.py SRC OUT.h | tools/gn_gen_content.py SRC --list")
    src = Path(sys.argv[1])
    text, lists, skipped = header(src)
    if sys.argv[2] == "--list":
        for e, l in lists.items():
            print(f"{e:<6} {len(l):>3}: " + ", ".join(f"{i} {n}" for i, n, _ in l))
        for b, i, w in skipped:
            print(f"skipped {b} {i}: {w}")
        return 0
    Path(sys.argv[2]).write_text(text)
    print(f"gn_gen_content: {sys.argv[2]}: " + ", ".join(f"{e} {len(l)}" for e, l in lists.items()) +
          f"; {len(skipped)} non-zero record(s) skipped")
    return 0


if __name__ == "__main__":
    sys.exit(main())
