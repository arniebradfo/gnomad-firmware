#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Kyle Palaniuk
"""The shipping build's target cost gate (tools/slot_cost_reachable.py --lines, FORK.md "The target cost lines,
adopted") on made-up listings and figures: the control tick is found both ways round the compiler puts its branch and
counted 1/16 amortised, whole on the tick sample; a figure over its line fails, at its line passes; a line with no
figure and a figure with no line fail; the lines file parses as tests/gn_target_lines.txt is written."""
import contextlib
import io
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))
import slot_cost_reachable as S  # noqa: E402

fails = 0


def report(what, ok):
    global fails
    print(f"gn slot cost test: {what:<88} {'ok' if ok else 'FAIL'}")
    fails += not ok


def listing(name, body):
    """a function in the pi32v2 listing's form: two-byte instructions from 0x2000000; 'goto @L' jumps to label L"""
    lines, addr, labels, out = [], 0x2000000, {}, [f"{name}:"]
    for t in body:
        if t.endswith(":"):
            labels[t[:-1]] = addr
        else:
            lines.append((addr, t))
            addr += 2
    for a, t in lines:
        if "@" in t:
            head, lab = t.split("@")
            t = f"{head}{labels[lab] - a - 2} <{name}+0x{labels[lab] - 0x2000000:X} : {labels[lab]:x} >"
        out.append(f" {a:x}:    00 00             \t{t}")
    return "\n".join(out) + "\n"


PRE = ["[--sp] = {rets, r15-r4}", "r7 = b[r6+15] (u)", "r0 = r7 + 1", "r0 = r0 & 0xF", "b[r6+15] = r0"]
TICK = [f"r{n} = r{n} + 3" for n in range(1, 9)]                  # the tick arm: 8 instructions
TAIL = ["join:", "r0 = r1", "pc = [sp++]"]
FORMS = {
    # the old core's shape: if (k != 0) goto JOIN, the tick arm falls through
    "if (k != 0) goto JOIN": PRE + ["if (r7 != 0) goto @join"] + TICK + TAIL,
    # the new core's (gc_fm_render, gc_drums_render at 6b11ee7): if (k == 0) goto TICK, the other arm goto JOIN
    "if (k == 0) goto TICK": PRE + ["if (r7 == 0) goto @tick", "r1 = r2", "goto @join", "tick:"] + TICK + TAIL,
}
WANT = {"if (k != 0) goto JOIN": (6 + 8 / 16 + 2, 6 + 8 + 2), "if (k == 0) goto TICK": (6 + 2 + 2, 6 + 8 + 2)}
with tempfile.TemporaryDirectory() as d:
    for form, body in FORMS.items():
        p = Path(d) / "felucca.dis"
        p.write_text(listing("gc_x_render", body))
        funcs, starts = S.parse(str(p))
        am = S.Scorer(funcs, starts, True).cost("gc_x_render")
        tk = S.Scorer(funcs, starts, False).cost("gc_x_render")
        report(f"{form}: amortised {am:g} (want {WANT[form][0]:g}), tick sample {tk:g} (want {WANT[form][1]:g})",
               (am, tk) == WANT[form])
    # a mask that is not 2^n - 1 is not a control tick: the arm counts whole
    p.write_text(listing("gc_x_render", [t.replace("0xF", "0xE") for t in FORMS["if (k == 0) goto TICK"]]))
    funcs, starts = S.parse(str(p))
    report("& 0xE (not 2^n - 1): no tick, the arm counted whole", S.Scorer(funcs, starts, True).cost("gc_x_render") == 16)

    lf = Path(d) / "lines.txt"
    lf.write_text("# a comment\nFM 901   # source\n\nIRQ 2145875 # 1,950,795 x 1.10\n")
    report("the lines file: NAME VALUE, '#' comments, blank lines", S.read_lines(lf) == {"FM": 901, "IRQ": 2145875})
    lf.write_text("FM 901.5\n")
    try:
        S.read_lines(lf)
        report("a malformed line is refused", False)
    except ValueError:
        report("a malformed line is refused", True)
    report("the repository's lines file parses, six lines",
           list(S.read_lines(ROOT / "tests" / "gn_target_lines.txt")) == ["FM", "GNOME", "DRUMS", "SAMPLER", "VA", "IRQ"])


def gate(figs, lines):
    with contextlib.redirect_stdout(io.StringIO()) as o:
        n = S.check_lines(figs, lines)
    return n, o.getvalue()


n, out = gate({"FM": 901, "IRQ": 2145875.0}, {"FM": 901, "IRQ": 2145875})
report("figures at their lines pass", n == 0 and "FAIL" not in out)
n, out = gate({"FM": 902, "IRQ": 1.0}, {"FM": 901, "IRQ": 2145875})
report("a figure over its line fails, with the delta", n == 1 and "OVER by 1" in out)
n, out = gate({"FM": 1.0}, {"FM": 901, "IRQ": 2145875})
report("a line with no figure fails", n == 1 and "no figure for this line" in out)
n, out = gate({"FM": 1.0, "VA": 1.0}, {"FM": 901})
report("a figure with no line fails", n == 1 and "no line for this figure" in out)
print("gn slot cost tests " + ("FAILED" if fails else "passed"))
sys.exit(1 if fails else 0)
