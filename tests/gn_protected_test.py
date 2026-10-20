#!/usr/bin/env python3
# Gnomad, a fork of Felucca (see FORK.md). Not part of upstream Felucca.
"""The gate "How the synth starts and installs is upstream's bytes" (.github/ci/gn_protected.py) passes exactly
the branding lines (FORK.md, Branding, rounds G and V1.6) and fails on any other byte.

  gn_protected_test.py UPSTREAM_COMMIT

Against upstream's main.c / usb.c / ota.c at UPSTREAM_COMMIT and this tree's (HEAD): HEAD and upstream pass;
a byte flipped anywhere else (a sweep over both files), a byte inside an approved line, a line added or removed,
a newline added, an approved line moved, the USB strings in the loader's branch, round G's USB line (upstream's
manufacturer), a byte of the manufacturer, and a byte in a file with no approved line all fail. Then usb.c's
string descriptors as compiled (cc, CC to override): the app (felucca.c's lines before usb.c, then usb.c's)
sends "Gnomad" and "Gnomad Synth V1"; the update loader (loader.c's #define) the same bytes as upstream's
usb.c gives it, "Hügelton Instruments" and "Felucca Update"; usb.c without felucca.c's lines (upstream's host
tests include it so) builds, its manufacturer upstream's."""
import os
import subprocess
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / ".github" / "ci"))
import gn_protected as G  # noqa: E402

MAIN, USB, OTA = "firmware/src/main.c", "firmware/src/usb.c", "firmware/src/ota.c"
FELUCCA = "firmware/src/felucca.c"
ROUND_G = (b"static const uint8_t STR2[] = {32, 3, 'G', 0, 'n', 0, 'o', 0, 'm', 0, 'a', 0, 'd', 0, ' ', 0, 'S', 0, "
           b"'y', 0, 'n', 0, 't', 0, 'h', 0, ' ', 0, 'V', 0, '1', 0};")   # round G's line: the product only


def show(rev, f):
    return subprocess.run(["git", "show", f"{rev}:{f}"], check=True, capture_output=True).stdout


def descriptors(usb):
    """usb.c's string descriptor lines: from STR0 to the #endif after the #ifdef FELUCCA_LOADER that names STR2"""
    ls = usb.split(b"\n")
    a = next(i for i, x in enumerate(ls) if x.startswith(b"static const uint8_t STR0"))
    b = next(i for i in range(a, len(ls)) if ls[i].strip() == b"#endif")
    return b"\n".join(ls[a:b + 1]) + b"\n"


def prelude(felucca):
    """felucca.c's preprocessor lines just before #include "usb.c" (b"" when there are none)"""
    ls = felucca.split(b"\n")
    end = start = ls.index(b'#include "usb.c"')
    while start and ls[start - 1].startswith(b"#"):
        start -= 1
    return b"\n".join(ls[start:end]) + b"\n" if start < end else b""


def strings(before, usb):
    """(STR1, STR2) as the compiler lays them out, or None when the lines do not build"""
    src = (b"#include <stdint.h>\n#include <stdio.h>\n" + before + descriptors(usb) +
           b'static void put(const uint8_t *s, unsigned n) { while (n--) printf("%02x", *s++); printf("\\n"); }\n'
           b"int main(void) { put(STR1, sizeof STR1); put(STR2, sizeof STR2); return 0; }\n")
    with tempfile.TemporaryDirectory() as d:
        c, exe = os.path.join(d, "usb_strings.c"), os.path.join(d, "usb_strings")
        Path(c).write_bytes(src)
        cc = os.environ.get("CC", "cc").split()
        if subprocess.run([*cc, "-std=gnu11", "-o", exe, c], capture_output=True).returncode:
            return None
        out = subprocess.run([exe], check=True, capture_output=True, text=True).stdout.split()
    return tuple(bytes.fromhex(x) for x in out)


def text(d):
    """a USB string descriptor's text, or None unless bLength is its size and bDescriptorType 3"""
    if len(d) < 2 or d[0] != len(d) or d[1] != 3 or len(d) % 2:
        return None
    return d[2:].decode("utf-16-le")


def main(rev):
    bad = 0

    def expect(name, path, up, head, want):
        nonlocal bad
        ok, msg = G.check(path, up, head)
        good = ok == want
        bad += not good
        print(f"  {'ok  ' if good else 'FAIL'}  {name}: {'passes' if ok else 'fails'} ({msg[:90]})")

    up = {f: show(rev, f) for f in (MAIN, USB, OTA)}
    head = {f: show("HEAD", f) for f in (MAIN, USB, OTA)}
    expect("main.c as branded", MAIN, up[MAIN], head[MAIN], True)
    expect("usb.c as branded", USB, up[USB], head[USB], True)
    expect("main.c as upstream", MAIN, up[MAIN], up[MAIN], True)
    expect("ota.c as upstream", OTA, up[OTA], up[OTA], True)

    # every approved line is really used by this tree (the allow-list holds nothing stale)
    for f in (MAIN, USB):
        for old, new in G.ALLOW[f]:
            used = new in head[f].split(b"\n") and old in up[f].split(b"\n")
            bad += not used
            print(f"  {'ok  ' if used else 'FAIL'}  {f}: approved line present here and upstream: {new[:60]!r}")

    # a sweep: one byte flipped anywhere in the branded file fails (newlines too)
    for f in (MAIN, USB):
        h, stride, missed, n = head[f], max(1, len(head[f]) // 3000), [], 0
        for i in range(0, len(h), stride):
            m = h[:i] + bytes([h[i] ^ 0x01]) + h[i + 1:]
            n += 1
            if G.check(f, up[f], m)[0]:
                missed.append(i)
        bad += bool(missed)
        print(f"  {'ok  ' if not missed else 'FAIL'}  {f}: {n} single-byte flips, {len(missed)} passed {missed[:5]}")

    crash = head[MAIN].replace(b'"GNOMAD CRASH"', b'"GNOMAD CRASX"')
    expect("a byte inside an approved line (GNOMAD CRASX)", MAIN, up[MAIN], crash, False)
    lines = head[MAIN].split(b"\n")
    expect("a line added (a blank line after the splash)", MAIN, up[MAIN],
           b"\n".join(lines[:137] + [b""] + lines[137:]), False)
    expect("a line removed", MAIN, up[MAIN], b"\n".join(lines[:10] + lines[11:]), False)
    expect("a newline added at the end", MAIN, up[MAIN], head[MAIN] + b"\n", False)
    expect("a space added at the end of the logic line after the splash", MAIN, up[MAIN],
           head[MAIN].replace(b"    if (felucca_dbg.magic != DBG_MAGIC) {", b"    if (felucca_dbg.magic != DBG_MAGIC) { ", 1),
           False)
    # an approved line moved: swap the splash's two lines
    i = next(k for k, x in enumerate(lines) if b'"GNOMAD", T_THEME' in x)
    sw = lines[:i] + [lines[i + 1], lines[i]] + lines[i + 2:]
    expect("the splash's two approved lines swapped", MAIN, up[MAIN], b"\n".join(sw), False)

    # usb.c: the new name in the update loader's branch too (or instead) fails
    ul = up[USB].split(b"\n")
    (old, new), = G.ALLOW[USB]
    k = ul.index(old)
    hl = head[USB].split(b"\n")
    j = max(x for x in range(k) if hl[x].startswith(b"static const uint8_t STR2[]"))   # the loader's (2 lines)
    loader_named = hl[:j] + [new, b""] + hl[j + 2:]
    expect("usb.c: the loader's name lines replaced", USB, up[USB], b"\n".join(loader_named), False)
    swapped = list(ul)
    swapped[k] = new
    expect("usb.c: approved line only (the minimal change)", USB, up[USB], b"\n".join(swapped), True)
    fake = list(ul)
    fake.insert(k, b"#endif")
    fake.insert(k, b"#else")
    fake.insert(k, b"#ifdef FELUCCA_LOADER")
    expect("usb.c: approved line with an extra conditional around it", USB, up[USB], b"\n".join(fake), False)
    expect("usb.c: upstream's manufacturer lines changed (the loader's too; not approved)", USB, up[USB],
           head[USB].replace(b"'H', 0, 0xFC, 0", b"'G', 0, 0xFC, 0", 1), False)
    # round V1.6: the approved line is the app's two strings; round G's line (upstream's manufacturer) no longer passes
    g = list(ul)
    g[k] = ROUND_G
    expect("usb.c: round G's line (the product only, upstream's manufacturer)", USB, up[USB], b"\n".join(g), False)
    for name, a, b in (("a byte of the manufacturer (Gnomae)", b"'d', 0}, STR2", b"'e', 0}, STR2"),
                       ("the manufacturer's length byte (14 -> 16)", b"STR1[] = {14,", b"STR1[] = {16,"),
                       ("the pop of STR1 left out", b'_Pragma("pop_macro(\\"STR1\\")") ', b""),
                       ("STR1 popped by another name", b'pop_macro(\\"STR1\\")', b'pop_macro(\\"STR2\\")')):
        m = head[USB].replace(a, b, 1)
        assert m != head[USB], name
        expect(f"usb.c: {name}", USB, up[USB], m, False)
    # the loader's branch (#ifdef FELUCCA_LOADER .. #else) and everything shared are upstream's lines, in place
    i0 = hl.index(b"#ifdef FELUCCA_LOADER")
    i1 = next(x for x in range(i0, len(hl)) if hl[x] == b"#else")
    same = hl[:i1 + 1] == ul[:i1 + 1] and hl[i1 + 2:] == ul[i1 + 2:] and len(hl) == len(ul)
    bad += not same
    print(f"  {'ok  ' if same else 'FAIL'}  usb.c: every line but the app's one approved line is upstream's (lines 1-{i1 + 1}, "
          f"{i1 + 3}-{len(hl)})")

    # a file with no approved line: any byte fails
    expect("ota.c with one byte changed", OTA, up[OTA], up[OTA][:100] + b"X" + up[OTA][101:], False)
    expect("main.c's approved line in ota.c", OTA, up[OTA], up[OTA] + G.ALLOW[MAIN][0][1], False)

    # app_branch itself
    for name, ls, i, want in (("#else of #ifdef FELUCCA_LOADER", [b"#ifdef FELUCCA_LOADER", b"a", b"#else", b"X", b"#endif"], 3, True),
                              ("#ifdef FELUCCA_LOADER's own branch", [b"#ifdef FELUCCA_LOADER", b"X", b"#else", b"b", b"#endif"], 1, False),
                              ("#else of another #if", [b"#if FELUCCA_CDC", b"a", b"#else", b"X", b"#endif"], 3, False),
                              ("outside any conditional", [b"a", b"X"], 1, False),
                              ("a nested #if closed before it", [b"#ifdef FELUCCA_LOADER", b"#else", b"#if 1", b"c", b"#endif", b"X", b"#endif"], 5, True)):
        got = G.app_branch(ls, i)
        bad += got != want
        print(f"  {'ok  ' if got == want else 'FAIL'}  app_branch: {name}: {got}")

    # end to end: the gate's main() over the real protected list, HEAD's main.c with one stray byte -> exit 1
    real = G.git

    def stray(*args):
        out = real(*args)
        if args[:2] == ("show", f"HEAD:{MAIN}"):
            out = out.replace(b"fm1_reboot();", b"fm1_reboot(); ", 1)
        return out
    G.git = stray
    rc = G.main(["gn_protected.py", rev, MAIN, USB, OTA, "firmware/loader"])
    G.git = real
    bad += rc != 1
    print(f"  {'ok  ' if rc == 1 else 'FAIL'}  the gate itself on a tree with one stray byte in main.c: exit {rc}")
    rc = G.main(["gn_protected.py", rev, MAIN, USB, OTA, "firmware/loader"])
    bad += rc != 0
    print(f"  {'ok  ' if rc == 0 else 'FAIL'}  the gate itself on this tree: exit {rc}")

    # usb.c's strings as compiled: the app (felucca.c's lines, then usb.c's), the loader (loader.c's #define)
    pre = prelude(show("HEAD", FELUCCA))
    app = strings(pre, head[USB])
    ldr = strings(b"#define FELUCCA_LOADER 1\n", head[USB])
    ldr_up = strings(b"#define FELUCCA_LOADER 1\n", up[USB])
    app_up = strings(b"", up[USB])
    app_plain = strings(b"", head[USB])
    for name, got, want in (
            ("the app sends manufacturer \"Gnomad\", product \"Gnomad Synth V1\" (felucca.c, then usb.c)",
             app and tuple(map(text, app)), ("Gnomad", "Gnomad Synth V1")),
            ("the loader's strings are upstream's bytes", ldr, ldr_up),
            ("the loader sends \"H\u00fcgelton Instruments\", \"Felucca Update\"",
             ldr and tuple(map(text, ldr)), ("H\u00fcgelton Instruments", "Felucca Update")),
            ("upstream's app sent \"H\u00fcgelton Instruments\", \"Felucca\" (what changed)",
             app_up and tuple(map(text, app_up)), ("H\u00fcgelton Instruments", "Felucca")),
            ("usb.c without felucca.c's lines (upstream's host tests) builds: manufacturer upstream's",
             app_plain and tuple(map(text, app_plain)), ("H\u00fcgelton Instruments", "Gnomad Synth V1")),
            ("felucca.c's lines before usb.c are the ones FORK.md names",
             pre.count(b"\n"), 4)):
        good = got == want
        bad += not good
        print(f"  {'ok  ' if good else 'FAIL'}  {name}{'' if good else f': {got!r}'}")

    print("ALL PROTECTED-GATE TESTS PASSED" if not bad else f"{bad} PROTECTED-GATE TEST(S) FAILED")
    return 1 if bad else 0


if __name__ == "__main__":
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    sys.exit(main(sys.argv[1]))
