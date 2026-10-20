#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Kyle Palaniuk
"""Gnomad (FORK.md) test of tools/gn_package_check.py: genuine packages pass, malformed ones fail.

  tests/gn_package_test.py BUILT.fwsc [TO_INSTALL.fwsc LOADER_SHA256]

BUILT is a package this tree's packer made (build/felucca.fwsc). Its flash.bin and ota.bin, as the
device selects them, are packed again by test_ufw(), which first has to give exactly the bytes of
tools/fm1pkg_make.py ufw() for the same files; then it makes the malformed packages below, each of
which the check must refuse: swapped types, a duplicate or extra entry, a name twice, an entry
outside the image, a wrong header, list or data CRC, overlapping entries, a wrong total, twelve
entries, and two the device itself would take: another loader (all its CRCs mended) and another
app area. With TO_INSTALL and LOADER_SHA256 it also checks the package to install: its app area
as BUILT's, its loader the pinned one, and (with node) that web/fm1pkg.js, which the installer page
uses, reads it as tools/fm1_install.py does. Nothing is sent anywhere."""
import hashlib
import shutil
import struct
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import fm1_install as I  # noqa: E402
import fm1pkg_make as P  # noqa: E402
import gn_package_check as C  # noqa: E402

bad = 0


def report(what, ok):
    global bad
    print(f"{what:<78} {'ok' if ok else 'FAIL'}")
    bad += not ok


def with_markers(logical, product):
    """the .fwsc around a logical image: one marker byte after each of the first 20 blocks (as P.ufw)"""
    raw = bytearray()
    for i in range(20):
        ch = product[i] if i < len(product) else None
        raw += logical[i * 0x2F:(i + 1) * 0x2F] + bytes([(ord(ch) + i + 1) & 0xFF if ch else 0x7D])
    return bytes(raw + logical[20 * 0x2F:])


def test_ufw(files, product, edit=None, recrc=False, total_delta=0):
    """P.ufw's layout for (type, index, name, data) files; edit(entries) may change the entry fields
    before they are encoded; recrc: data CRCs taken from the image as finally laid out"""
    off, ents = 0x400, []
    for typ, idx, name, d in files:
        off = (off + 0xFF) & ~0xFF
        ents.append({"type": typ, "index": idx, "dcrc": P.crc16(d), "off": off, "size": len(d), "name": name, "d": d})
        off += len(d)
    total = off
    if edit:
        edit(ents)
    end = max([total] + [e["off"] + len(e["d"]) for e in ents])
    logical = bytearray(b"\xFF" * end)
    for e in ents:
        logical[e["off"]:e["off"] + len(e["d"])] = e["d"]
    if recrc:
        for e in ents:
            e["dcrc"] = P.crc16(logical[e["off"]:e["off"] + e["size"]])
    lst = bytearray()
    for e in ents:
        x = bytearray(0x50)
        struct.pack_into("<HHHHIII", x, 0, e["type"], e["index"], e["dcrc"], 0, e["off"], e["size"],
                         (e["size"] + 31) & ~31)
        x[0x40:0x40 + len(e["name"])] = e["name"].encode()
        P.enc(x, 0, 0x50)
        lst += x
    hdr = bytearray(0x40)
    struct.pack_into("<IH", hdr, 4, total + total_delta, len(ents))
    struct.pack_into("<HHH", hdr, 10, 0x0004, 0x0200, 0)
    hdr[16:16 + len(P.UFW_CHIP)] = P.UFW_CHIP
    struct.pack_into("<H", hdr, 2, P.crc16(lst))
    struct.pack_into("<H", hdr, 0, P.crc16(hdr[2:0x40]))
    P.enc(hdr, 0, 0x40)
    logical[0:0x40] = hdr
    logical[0x40:0x40 + len(lst)] = lst
    return with_markers(bytes(logical[:max(end, 0x400)]), product)


def flip_logical(raw, at):
    """the package with one byte of its logical image changed"""
    lg = bytearray(I.logical_image(raw))
    lg[at] ^= 0x01
    return with_markers(bytes(lg), I.product_of(raw))


def mended_loader(ota):
    """ota.bin with one byte of its compressed body changed and both CRCs that cover it mended:
    ota.c would stage it (a different loader the device accepts)"""
    o = bytearray(ota)
    o[0x100] ^= 0x01
    n = struct.unpack_from("<I", o, 8)[0]
    struct.pack_into("<H", o, 2, P.crc16(o[0x20:0x20 + n]))
    struct.pack_into("<H", o, 0, P.crc16(o[2:32]))
    return bytes(o)


def main(argv):
    built = Path(argv[0]).read_bytes()
    product = I.product_of(built)
    problems, sel = C.check(built)
    report(f"{argv[0]} ({product}): passes the check", not problems)
    for p in problems:
        print(f"    {p}")
    F, O = sel["flash"], sel["ota"]
    genuine = [(0, 0, "flash.bin", F), (100, 1, "ota.bin", O)]
    same = test_ufw(genuine, product)
    report("test_ufw() gives fm1pkg_make.ufw()'s bytes for the same files", same == P.ufw(F, O, product))
    report("... and that package passes, compared with itself and the loader's SHA-256",
           not C.check(same, F, hashlib.sha256(O).hexdigest())[0])

    F2 = bytearray(F)
    F2[-1] ^= 0x01
    O2 = mended_loader(O)
    report("the mended loader passes ota.c's own checks (the device would stage it)", not C.loader_problems(O2))
    cases = [
        ("types swapped (flash.bin type 100, ota.bin type 0)",
         test_ufw([(100, 0, "flash.bin", F), (0, 1, "ota.bin", O)], product)),
        ("a second type-0 flash.bin after ota.bin (the device takes the last)",
         test_ufw(genuine + [(0, 2, "flash.bin", bytes(F2))], product)),
        ("one name twice (ota.bin's entry named flash.bin)",
         test_ufw([(0, 0, "flash.bin", F), (100, 1, "flash.bin", O)], product)),
        ("an extra entry", test_ufw(genuine + [(7, 2, "extra.bin", b"\x00" * 64)], product)),
        ("ota.bin's entry reaches 4 KiB past the image",
         test_ufw(genuine, product, edit=lambda es: es[1].update(size=es[1]["size"] + 0x1000))),
        ("the header's CRC wrong", flip_logical(same, 0x30)),
        ("the entry list's CRC wrong", flip_logical(same, 0x40 + 0x50 + 0x48)),
        ("flash.bin's data CRC wrong", test_ufw(genuine, product, edit=lambda es: es[0].update(dcrc=es[0]["dcrc"] ^ 1))),
        ("ota.bin laid over flash.bin (both CRCs mended)",
         test_ufw(genuine, product, edit=lambda es: es[1].update(off=es[0]["off"] + 0x1000), recrc=True)),
        ("the header's total 256 B too large", test_ufw(genuine, product, total_delta=0x100)),
        ("twelve entries", test_ufw(genuine + [(7, 2 + k, f"x{k}.bin", b"\x00" * 16) for k in range(10)], product)),
        ("another loader, every CRC right (only the SHA-256 tells)",
         test_ufw([(0, 0, "flash.bin", F), (100, 1, "ota.bin", O2)], product)),
        ("another app area, every CRC right (only the comparison tells)",
         test_ufw([(0, 0, "flash.bin", bytes(F2)), (100, 1, "ota.bin", O)], product)),
    ]
    for what, raw in cases:
        problems, _ = C.check(raw, F, hashlib.sha256(O).hexdigest())
        report(f"refused: {what}", bool(problems))
        if problems:
            print(f"    {problems[0]}")

    if len(argv) >= 3:
        pkg = Path(argv[1])
        raw = pkg.read_bytes()
        problems, sel = C.check(raw, F, argv[2])
        report(f"{pkg.name}: the package to install passes: app area as {Path(argv[0]).name}'s, the pinned loader",
               not problems)
        for p in problems:
            print(f"    {p}")
        if shutil.which("node"):            # as tests/install_test.py against_js(): what the installer page reads
            js = ("import { logicalImage, productOf } from %r; import { readFileSync } from 'node:fs';"
                  "const p = readFileSync(process.argv[1]); process.stderr.write(productOf(p));"
                  "process.stdout.write(logicalImage(p));") % str(ROOT / "web/fm1pkg.js")
            r = subprocess.run(["node", "--input-type=module", "-e", js, str(pkg)], capture_output=True)
            report(f"web/fm1pkg.js (the installer page) reads it as tools/fm1_install.py does ({I.product_of(raw)})",
                   r.returncode == 0 and r.stdout == I.logical_image(raw) and r.stderr.decode() == I.product_of(raw))
        else:
            print("web/fm1pkg.js against the package to install: skipped (no node)")
    print("gn package tests passed" if not bad else "GN PACKAGE TESTS FAILED")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
