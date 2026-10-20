#!/usr/bin/env python3
# Gnomad, a fork of Felucca (see FORK.md). Not part of upstream Felucca.
"""How the synth starts and installs is upstream's bytes, except the branding lines (FORK.md, Branding, rounds G and V1.6).

  gn_protected.py UPSTREAM_COMMIT PATH...

Every PATH (a file or a directory) must have the bytes it has at UPSTREAM_COMMIT, with no file added or
removed, except the lines in ALLOW: in main.c and usb.c the start-up splash, the crash page's title and
the app's USB strings (manufacturer and product, one line), each replaced as one whole line by exactly the line
written here. The file is
compared line by line, newlines included: the same number of lines, and every line either upstream's or,
where upstream has an approved old line, exactly its approved new line. Anything else (a byte elsewhere,
a byte inside an approved line, a line added, moved or removed) fails. The USB strings' line must stay in
the app's branch of `#ifdef FELUCCA_LOADER` (the update loader includes usb.c and keeps upstream's
"Hügelton Instruments", "Felucca Update" and every other byte). Upstream writes the manufacturer (STR1) once,
above that #ifdef, for both builds, and get_desc sends STR1: the app's felucca.c saves STR1 -> GN_STR1
(`#pragma push_macro("STR1")`) and renames upstream's array before it includes usb.c; the approved line pops
that name and defines GN_STR1 "Gnomad" beside STR2. Without felucca.c's lines (the loader, upstream's host
tests) there is nothing to pop and STR1 stays upstream's. tests/gn_protected_test.py compiles both ways.

Writes nothing; prints one line per path and exits 1 on any difference. check() is what
tests/gn_protected_test.py proves against altered copies."""
import subprocess
import sys

# file -> [(upstream's line, our line)]; whole lines, without the newline
ALLOW = {
    "firmware/src/main.c": [
        (b'    draw_text_line(0, 8, 240, &AF_M, "FELUCCA CRASH", UI_CRASH_INK, UI_CRASH_BG, 1);',
         b'    draw_text_line(0, 8, 240, &AF_M, "GNOMAD CRASH", UI_CRASH_INK, UI_CRASH_BG, 1);'),
        (b'    draw_text_box(0, 94, 240, &AF_L, "FELUCCA", T_THEME, 1);',
         b'    draw_text_box(0, 94, 240, &AF_L, "GNOMAD", T_THEME, 1);'),
        (b'    draw_text_box(0, 134, 240, &AF_S, "MULTI-ENGINE SYNTH", T_MID, 1);',
         b'    draw_text_box(0, 134, 240, &AF_S, "SYNTH V1", T_MID, 1);'),
    ],
    "firmware/src/usb.c": [         # the app's manufacturer "Gnomad" (round V1.6) and product "Gnomad Synth V1" (round G)
        (b"static const uint8_t STR2[] = {16, 3, 'F', 0, 'e', 0, 'l', 0, 'u', 0, 'c', 0, 'c', 0, 'a', 0};",
         b"_Pragma(\"pop_macro(\\\"STR1\\\")\") "
         b"static const uint8_t GN_STR1[] = {14, 3, 'G', 0, 'n', 0, 'o', 0, 'm', 0, 'a', 0, 'd', 0}, "
         b"STR2[] = {32, 3, 'G', 0, 'n', 0, 'o', 0, 'm', 0, 'a', 0, 'd', 0, ' ', 0, 'S', 0, "
         b"'y', 0, 'n', 0, 't', 0, 'h', 0, ' ', 0, 'V', 0, '1', 0};"),
    ],
}
APP_ONLY = {"firmware/src/usb.c"}          # its approved lines only in the app's branch of #ifdef FELUCCA_LOADER


def app_branch(lines, i):
    """line i is inside the #else of an `#ifdef FELUCCA_LOADER` (no other conditional between)"""
    depth, seen_else = 0, False
    for j in range(i - 1, -1, -1):
        t = lines[j].strip()
        if t.startswith(b"#endif"):
            depth += 1
        elif t.startswith(b"#if"):
            if depth:
                depth -= 1
                continue
            return seen_else and t.split() == [b"#ifdef", b"FELUCCA_LOADER"]
        elif t.startswith(b"#else") or t.startswith(b"#elif"):
            if depth:
                continue
            if seen_else or t.startswith(b"#elif"):
                return False
            seen_else = True
    return False


def check(path, up, head):
    """(ok, message) for one file's bytes: upstream's, or upstream's with approved lines exactly replaced"""
    if up == head:
        return True, "the same bytes"
    allow = ALLOW.get(path)
    if not allow:
        return False, "differs (no line of it is approved)"
    a, b = up.split(b"\n"), head.split(b"\n")
    if len(a) != len(b):
        return False, f"{len(b) - len(a):+d} lines (only approved lines may change, one for one)"
    pairs = dict(allow)
    changed = []
    for i, (x, y) in enumerate(zip(a, b)):
        if x == y:
            continue
        if pairs.get(x) != y:
            return False, f"line {i + 1} differs and is not an approved replacement: {y[:100]!r}"
        if path in APP_ONLY and not app_branch(b, i):
            return False, f"line {i + 1}: approved only in the app's branch of #ifdef FELUCCA_LOADER"
        changed.append(i + 1)
    return True, f"upstream's bytes except approved line(s) {', '.join(map(str, changed))}"


def git(*args):
    return subprocess.run(["git", *args], check=True, capture_output=True).stdout


def files(rev, path):
    out = git("ls-tree", "-r", "--name-only", rev, "--", path)
    return set(out.decode().split())


def main(argv):
    if len(argv) < 3:
        sys.exit(__doc__)
    rev, bad = argv[1], 0
    for p in argv[2:]:
        old, new = files(rev, p), files("HEAD", p)
        fails = [f"{f}: {'added' if f in new else 'removed'}" for f in sorted(old ^ new)]
        notes = []
        for f in sorted(old & new):
            ok, msg = check(f, git("show", f"{rev}:{f}"), git("show", f"HEAD:{f}"))
            if not ok:
                fails.append(f"{f}: {msg}")
            elif f in ALLOW:
                notes.append(f"{f}: {msg}")
        if not old and not new:
            fails.append(f"{p}: in neither tree")
        for m in fails:
            print(f"  FAIL  {m}")
        for m in notes:
            print(f"  ok    {m}")
        if not fails and not notes:
            print(f"  ok    {p}: the same bytes ({len(old)} file{'s' if len(old) != 1 else ''})")
        bad += len(fails)
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
