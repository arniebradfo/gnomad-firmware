#!/usr/bin/env python3
# Gnomad, a fork of Felucca (see FORK.md). Not part of upstream Felucca.
"""Compare a Felucca .fwsc package built here with a released one; a Markdown report on stdout.

  compare_fwsc.py SRC OURS THEIRS [--digest sha256:...]

SRC is the source tree OURS was built from: its tools/ give the package format (fm1pkg_make.py,
fm1_install.py), its build/ the app, the loader and the ELF. If the only difference in the app is
the build date (__DATE__, on the ABOUT page), the app is repackaged with the released package's
date to show whether everything else is identical (needs AC79_SDK, as the build does). A report:
the exit status is 0 whether or not the packages match."""
import argparse
import hashlib
import os
import re
import struct
import subprocess
import sys
from pathlib import Path

ap = argparse.ArgumentParser()
ap.add_argument("src", type=Path)
ap.add_argument("ours", type=Path)
ap.add_argument("theirs", type=Path)
ap.add_argument("--digest", default="", help="the digest GitHub recorded for THEIRS")
a = ap.parse_args()
sys.path.insert(0, str(a.src / "tools"))
import fm1_install as I  # noqa: E402
import fm1pkg_make as P  # noqa: E402

APP_XIP = 0x02000120                                     # tools/build.py: app offset 0 in the XIP map
DATE = re.compile(rb"(?:Jan|Feb|Mar|Apr|May|Jun|Jul|Aug|Sep|Oct|Nov|Dec) [ 123]\d \d{4}")   # __DATE__
APP = "app (felucca.bin in its slot)"


def parts(raw):
    """the package as the device reads it, split by the format (tools/fm1pkg_make.py ufw, flash_image)"""
    logical = I.logical_image(raw)
    hdr = bytearray(logical[:0x40])
    P.enc(hdr, 0, 0x40)
    nfiles = struct.unpack_from("<H", hdr, 8)[0]
    out, files = {"UFW header + file list": logical[:0x40 + 0x50 * nfiles]}, {}
    for i in range(nfiles):
        e = bytearray(logical[0x40 + 0x50 * i:0x40 + 0x50 * (i + 1)])
        P.enc(e, 0, 0x50)
        _, _, _, _, off, size, _ = struct.unpack_from("<HHHHIII", e, 0)
        files[bytes(e[0x40:]).split(b"\0")[0].decode(errors="replace")] = logical[off:off + size]
    flash = files.get("flash.bin", b"")
    area = bytearray(flash[0x4000:])
    P.sfc(area, 0, len(area), 0, P.KEY)                   # the app area is SFC-encrypted with the chip key
    out["flash head (SDK SPL, isd_config, JLFS)"] = flash[:0x4000]
    out["app area directory (CRCs)"] = bytes(area[:0x120])
    out[APP] = bytes(area[0x120:0x120 + P.APP_SLOT])
    out["SDK cfg_tool.bin, EQ table, region descriptors"] = bytes(area[0x120 + P.APP_SLOT:])
    out["update loader (ota.bin)"] = files.get("ota.bin", b"")
    return I.product_of(raw), out


def ranges(x, y, gap=16):
    """[[start, end)] of the differing bytes (a run ends after `gap` equal bytes)"""
    out = []
    for i in range(max(len(x), len(y))):
        if i >= len(x) or i >= len(y) or x[i] != y[i]:
            if out and i - out[-1][1] <= gap:
                out[-1][1] = i + 1
            else:
                out.append([i, i + 1])
    return out


def ndiff(x, y):
    return sum(e - s for s, e in ranges(x, y, 0))


def symbols():
    """[(start, end, name)] of the sized .text symbols in SRC/build/felucca.elf (JieLi objdump -t)"""
    objdump = Path(os.environ.get("JIELI_TOOLCHAIN", "/nonexistent")) / "common" / "bin" / "objdump"
    try:
        r = subprocess.run([str(objdump), "-t", str(a.src / "build" / "felucca.elf")], capture_output=True,
                           text=True, timeout=60)
    except OSError:
        return []
    out = []
    for ln in r.stdout.splitlines():
        m = re.match(r"^([0-9a-f]{8})\s.{7}\s(\.text)\s+([0-9a-f]+)\s+(\S+)$", ln)
        if m and int(m.group(3), 16):
            out.append((int(m.group(1), 16), int(m.group(1), 16) + int(m.group(3), 16), m.group(4)))
    return out


def where(off, syms):
    addr = APP_XIP + off
    hit = [f"{n}+{addr - s:#x}" for s, e, n in syms if s <= addr < e]
    return hit[0] if hit else "no symbol (an unnamed constant, such as a string literal)"


def show(b, s, e):
    """hex and printable text of b[s:e]"""
    chunk = b[s:e]
    return chunk.hex(" ") + "  |" + "".join(chr(c) if 32 <= c < 127 else "." for c in chunk) + "|"


ours, theirs = a.ours.read_bytes(), a.theirs.read_bytes()
h_ours, h_theirs = hashlib.sha256(ours).hexdigest(), hashlib.sha256(theirs).hexdigest()
out = [f"## Reproducibility: {a.ours.name} against upstream's release", ""]
out += ["| | Size (bytes) | SHA-256 |", "| --- | ---: | --- |",
        f"| ours (rebuilt here) | {len(ours)} | `{h_ours}` |",
        f"| upstream release | {len(theirs)} | `{h_theirs}` |", ""]
if a.digest.startswith("sha256:"):
    match = "matches" if a.digest[7:] == h_theirs else "DOES NOT MATCH"
    out += [f"The downloaded release asset {match} the digest GitHub recorded for it (`{a.digest}`).", ""]
if ours == theirs:
    out += ["**Byte-identical.** The package built here from the tag is the package upstream released."]
    print("\n".join(out))
    sys.exit(0)

out += [f"**Not byte-identical:** {ndiff(ours, theirs)} bytes differ.", ""]
try:
    id_o, p_o = parts(ours)
    id_t, p_t = parts(theirs)
except Exception as e:                                   # a report: say why and stop
    out += [f"Could not split the packages by the format ({type(e).__name__}: {e})."]
    print("\n".join(out))
    sys.exit(0)
out += [f"Identity: ours `{id_o}`, upstream `{id_t}`.", "",
        "| Part | Ours (bytes) | Upstream (bytes) | Same | Differing bytes |", "| --- | ---: | ---: | --- | ---: |"]
for name in p_o:
    x, y = p_o[name], p_t.get(name, b"")
    out.append(f"| {name} | {len(x)} | {len(y)} | {'yes' if x == y else 'no'} | {ndiff(x, y)} |")
out.append("")

app_o, app_t = p_o[APP], p_t.get(APP, b"")
rs = ranges(app_o, app_t) if app_o != app_t else []
if rs:
    syms = symbols()
    used_o, used_t = len(app_o.rstrip(b"\xff")), len(app_t.rstrip(b"\xff"))
    out += [f"### Where the app differs ({len(rs)} region(s))", "",
            f"App image without the 0xFF fill: ours {used_o} B, upstream {used_t} B.", "", "```text"]
    for s, e in rs[:20]:
        c0, c1 = max(0, s - 8), min(max(len(app_o), len(app_t)), e + 8)
        out += [f"app offset {s:#08x}, {e - s} B, in {where(s, syms)}",
                f"  ours      {show(app_o, c0, c1)}", f"  upstream  {show(app_t, c0, c1)}"]
    out += ["```", ""]
    # a different size: where upstream's bytes become ours shifted by the difference (code before it differs
    # only in the addresses it loads, which moved)
    d = used_t - used_o
    if d:
        at = next((i for i in range(0, min(used_o, used_t - d) - 64, 4)
                   if app_o[i:i + 64] != app_t[i:i + 64] and (app_o[i:i + 64] == app_t[i + d:i + d + 64] if d > 0
                                                               else app_o[i - d:i - d + 64] == app_t[i:i + 64])), None)
        if at is None:
            out += [f"Upstream's app is {d:+d} B against ours; no single place where one becomes the other shifted.", ""]
        else:
            lo = next((i for i in range(at - 4, -1, -4) if app_o[i:i + 64] == app_t[i:i + 64]), 0)
            out += [f"Upstream's app is {d:+d} B against ours. From app offset {at:#08x} ({where(at, syms)} in ours) "
                    f"on, its bytes are ours shifted by {d:+d} B; up to {lo + 64:#08x} ({where(lo + 64, syms)}) the two "
                    "line up unshifted. The difference in size lies between those two offsets; elsewhere before them "
                    "the code differs in the addresses it loads, which moved.", ""]

# the build date: every app difference inside a __DATE__ string at the same place in both images
dates = [(m.start(), m.end()) for m in DATE.finditer(app_o) if DATE.fullmatch(app_t[m.start():m.end()])]
if rs and all(any(ds <= s and e <= de for ds, de in dates) for s, e in rs):
    ds, de = next((ds, de) for ds, de in dates if ds <= rs[0][0] < de)
    d_o, d_t = app_o[ds:de].decode(), app_t[ds:de].decode()
    out += [f"Every difference in the app is the **build date** (`__DATE__` on the ABOUT page, "
            f"`firmware/src/ui_menu.c`): ours `{d_o}`, upstream `{d_t}`. The other differing parts hold CRCs "
            "over the app.", ""]
    try:
        app_bin = bytearray((a.src / "build" / "felucca.bin").read_bytes())
        ota = (a.src / "build" / "loader" / "ota.bin").read_bytes()
        for s, e in dates:
            if any(s <= r0 < e for r0, _ in rs):
                app_bin[s:e] = app_t[s:e]
        again = P.ufw(P.flash_image(bytes(app_bin), P.KEY), ota, id_o)
        if again == theirs:
            out += [f"**Byte-identical with upstream's build date:** this app and loader, packaged again with "
                    f"`{d_t}` in place of `{d_o}`, give exactly upstream's package (SHA-256 `{h_theirs}`). "
                    "Toolchain, SDK files and sources reproduce the release.", ""]
        else:
            out += [f"With upstream's build date the package still differs in {ndiff(again, theirs)} bytes.", ""]
    except (OSError, SystemExit) as e:
        out += [f"Could not repackage with upstream's date ({e}).", ""]
print("\n".join(out))
