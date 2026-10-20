#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Kyle Palaniuk
"""Gnomad (FORK.md): an FM-1 package read the way the device reads it.

  tools/gn_package_check.py PACKAGE.fwsc [--flash-like OTHER.fwsc] [--loader-sha256 HEX]

The installer serves the package's logical image (the .fwsc without its 20 marker bytes). The
firmware on the synth reads its UFW header and entry list and takes the LAST entry of type 0 as
flash.bin and the LAST of type 100 as ota.bin (firmware/src/ota.c ota_ufw), checks the update
loader's header (ota_stage, step 2: its CRCs, its load address, upstream's official loader or
Felucca's own) and the loader body's CRC, and stages it; that loader then takes flash.bin the same
way and writes its app area (firmware/loader/ldr_core.c, which also needs flash.bin to reach
0x93000). Nothing on the device checks an entry's name or its data CRC.

check() fails unless all of these hold, which is stricter than the device:
  - header CRC, entry-list CRC, 1..11 entries, the total equal to the logical image's length;
  - exactly two entries, (type 0, "flash.bin") then (type 100, "ota.bin"): no other type or name,
    no duplicate, so the device's last-wins choice cannot pick another entry than the named one;
  - every entry after the 1 KiB header the device reads, inside the image, the two not
    overlapping, each entry's data CRC right;
  - the update loader passes ota.c's checks, its body CRC matches, and flash.bin reaches 0x93000;
and then compares the bytes the device would select: flash.bin with another package's (as the
device selects them there too), ota.bin with an expected SHA-256. Pure Python; nothing is sent
anywhere. .github/ci/released_package.py uses it on the package to install; tests/gn_package_test.py
feeds it malformed packages."""
import hashlib
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import fm1_install as I  # noqa: E402  (package parsing only)
import fm1pkg_make as P  # noqa: E402

LOADER_LOAD = 0x01C0A800            # ota.c: the loader must load here
OTA_AREA_LEN = 0x5000               # ota.c: the staging area
APP_HI = 0x93000                    # ldr_core.c: flash.bin must reach the end of the app area
EXPECTED = [(0, "flash.bin"), (100, "ota.bin")]


def u16(b, o):
    return struct.unpack_from("<H", b, o)[0]


def u32(b, o):
    return struct.unpack_from("<I", b, o)[0]


def entries_of(logical):
    """(problems, header fields, [entry dicts]) of a logical image, as ota_ufw decodes them"""
    problems = []
    if len(logical) < 0x400:
        return [f"the image is {len(logical)} B, shorter than the 1 KiB header the device reads"], {}, []
    hdr = bytearray(logical[:0x40])
    P.enc(hdr, 0, 0x40)
    nent = u16(hdr, 8)
    head = {"nent": nent, "total": u32(hdr, 4)}
    if P.crc16(hdr[2:0x40]) != u16(hdr, 0):
        problems.append("the UFW header's CRC is wrong (the device refuses: -2)")
    if not 1 <= nent <= 11:
        problems.append(f"{nent} entries; the device takes 1..11 (-2)")
        return problems, head, []
    lst = logical[0x40:0x40 + nent * 0x50]
    if P.crc16(lst) != u16(hdr, 2):
        problems.append("the entry list's CRC is wrong (the device refuses: -2)")
    ents = []
    for i in range(nent):
        e = bytearray(lst[i * 0x50:(i + 1) * 0x50])
        P.enc(e, 0, 0x50)
        typ, idx, dcrc, _, off, size, _asize = struct.unpack_from("<HHHHIII", e, 0)
        name = bytes(e[0x40:]).split(b"\0")[0].decode("latin-1")
        ents.append({"type": typ, "index": idx, "dcrc": dcrc, "off": off, "size": size, "name": name})
    return problems, head, ents


def loader_problems(ota):
    """ota.c ota_stage step 2 and the body check of step 4, on the selected ota.bin"""
    p = []
    if len(ota) < 0x200:
        return [f"ota.bin is {len(ota)} B: too short for its header"]
    b = ota[:512]
    n = u32(b, 8)
    if P.crc16(b[2:32]) != u16(b, 0):
        p.append("ota.bin: header CRC wrong (-7)")
    if u32(b, 4) != 0x20 or b[12] != 0x41:
        p.append("ota.bin: not a loader header (-7)")
    if len(ota) != n + 0x20:
        p.append(f"ota.bin: {len(ota)} B, its header says {n + 0x20} (-7)")
    if n > OTA_AREA_LEN - 0x200:
        p.append(f"ota.bin: body {n} B does not fit the staging area (-7)")
    if P.crc16(b[0x22:0x40]) != u16(b, 0x20):
        p.append("ota.bin: inner header CRC wrong (-7)")
    if u32(b, 0x28) != LOADER_LOAD:
        p.append(f"ota.bin: load address {u32(b, 0x28):#x}, not {LOADER_LOAD:#x} (-7)")
    official = u16(b, 0) == 0xEBAA and u16(b, 2) == 0x5881 and n == 0x4DE1
    own = any(b[i:i + 16] == I.LOADER_MARK for i in range(0x48, 0x48 + 64 - 16 + 1)) and u32(b, 0x24) <= 0x14000
    if not official and not own:
        p.append("ota.bin: neither upstream's official loader nor Felucca's own (-7)")
    if P.crc16(ota[0x20:0x20 + n]) != u16(b, 2):
        p.append("ota.bin: body CRC wrong (-11)")
    return p


def check(raw, flash_like=None, loader_sha256=None):
    """(problems, {"flash": bytes, "ota": bytes}) for a package; no problems = it passes"""
    logical = I.logical_image(raw)
    problems, head, ents = entries_of(logical)
    if head and head["total"] != len(logical):
        problems.append(f"the header's total is {head['total']} B, the image {len(logical)} B")
    pairs = [(e["type"], e["name"]) for e in ents]
    if ents and pairs != EXPECTED:
        problems.append(f"entries {pairs}, not {EXPECTED}")
    for key in ("type", "name"):
        seen = [e[key] for e in ents]
        dup = sorted({str(v) for v in seen if seen.count(v) > 1})
        if dup:
            problems.append(f"duplicate entry {key}s: {', '.join(dup)}")
    spans = []
    for e in ents:
        end = e["off"] + e["size"]
        if e["off"] < 0x400 or end > len(logical) or (head and end > head["total"]):
            problems.append(f"{e['name']!r} [{e['off']:#x}, {end:#x}) is outside the image after its header")
            continue
        if P.crc16(logical[e["off"]:end]) != e["dcrc"]:
            problems.append(f"{e['name']!r}: data CRC wrong")
        spans.append((e["off"], end, e["name"]))
    spans.sort()
    for (a0, a1, an), (b0, b1, bn) in zip(spans, spans[1:]):
        if b0 < a1:
            problems.append(f"{an!r} and {bn!r} overlap")
    chosen = {}
    for e in ents:                              # ota_ufw: the last entry of each type wins
        if e["type"] == 0:
            chosen["flash"] = e
        if e["type"] == 100:
            chosen["ota"] = e
    sel = {k: bytes(logical[e["off"]:e["off"] + e["size"]]) for k, e in chosen.items()}
    if "flash" not in sel or "ota" not in sel:
        problems.append("the device finds no flash.bin (type 0) or no ota.bin (type 100) (-3)")
    if "ota" in sel:
        problems += loader_problems(sel["ota"])
        if loader_sha256 and hashlib.sha256(sel["ota"]).hexdigest() != loader_sha256.lower():
            problems.append(f"the loader the device selects has SHA-256 {hashlib.sha256(sel['ota']).hexdigest()}, "
                            f"not {loader_sha256.lower()}")
    if "flash" in sel:
        if len(sel["flash"]) < APP_HI:
            problems.append(f"flash.bin is {len(sel['flash'])} B; the loader needs {APP_HI} (-3)")
        if flash_like is not None and sel["flash"] != flash_like:
            problems.append("the flash.bin the device selects differs from the reference package's")
    return problems, sel


def main(argv):
    import argparse
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("package", type=Path)
    ap.add_argument("--flash-like", type=Path, help="a package whose device-selected flash.bin must be equal")
    ap.add_argument("--loader-sha256", help="the SHA-256 the device-selected ota.bin must have")
    a = ap.parse_args(argv)
    ref = None
    if a.flash_like:
        rp, rsel = check(a.flash_like.read_bytes())
        if rp:
            print(f"gn_package_check: FAIL the reference {a.flash_like}: " + "; ".join(rp))
            return 1
        ref = rsel["flash"]
    problems, sel = check(a.package.read_bytes(), ref, a.loader_sha256)
    for p in problems:
        print(f"gn_package_check: FAIL {a.package}: {p}")
    if not problems:
        print(f"gn_package_check: {a.package}: ok (flash.bin {len(sel['flash'])} B, ota.bin {len(sel['ota'])} B, "
              f"SHA-256 {hashlib.sha256(sel['ota']).hexdigest()})")
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
