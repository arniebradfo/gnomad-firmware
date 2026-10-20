#!/usr/bin/env python3
# Gnomad, a fork of Felucca (see FORK.md). Not part of upstream Felucca.
"""The budget gate's own test (.github/ci/gn_budget.py). Two kinds of case, asserted apart:
- a declaration the parser must refuse (missing, doubled, empty, incomplete, an unknown key, a key twice,
  a key without a value, a value of the wrong form or a malformed number): allowances() returns no
  allowance, and the check fails;
- a whole, well-formed declaration: allowances() returns it, and the check passes or fails on the
  figures alone (one inside every allowance passes; one over an allowance fails).
  python3 tests/gn_budget_test.py      (the repo root; CI runs it before the budget check)"""
import contextlib
import io
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / ".github" / "ci"))
import gn_budget as B  # noqa: E402

WHOLE = ("gn-budget T image +100 .data +10 .bss +10 .data+.bss +10 pool +0 .noinit +0 .ram_text +0 "
         "free:image 50 free:.data+.bss 50 stack:fm1_cstart +8 stack:fm1_alnk0_irq +8 stack:fm1_timer5_irq +8")
SO = {"image": 1100, ".data": 5, ".bss": 105, ".data+.bss": 110, "pool": 7, ".noinit": 3, ".ram_text": 2}
SB = {"image": 1000, ".data": 5, ".bss": 100, ".data+.bss": 105, "pool": 7, ".noinit": 3, ".ram_text": 2}
LIMIT = {"image": 2000, ".data+.bss": 200, "pool": 10, ".noinit": 4}
ST = {"fm1_cstart": (100, []), "fm1_alnk0_irq": (50, []), "fm1_timer5_irq": (10, [])}
fails = 0


def run_check(text, so=SO, st=ST):
    with contextlib.redirect_stdout(io.StringIO()):     # (the check's own lines: not this test's)
        return B.check("T", so, SB, LIMIT, st, ST, text)


def report(what, good, why=""):
    global fails
    print(f"gn budget test: {what:<62} {'ok' if good else 'FAIL'}" + (f" ({why})" if why else ""))
    fails += not good


def refused(what, text):
    """the parser refuses the declaration, and the check fails on it"""
    a, why = B.allowances("T", text)
    report("refused: " + what, a is None and why is not None and run_check(text) == 1, why or "accepted")


def judged(what, text, passes, so=SO, st=ST):
    """a well-formed declaration: accepted, and the check's verdict is the figures' alone"""
    a, why = B.allowances("T", text)
    report(("passes: " if passes else "fails on its figures: ") + what,
           a is not None and (run_check(text, so, st) == 0) == passes, why or "")


judged("a whole line, every figure inside it", WHOLE, True)
judged("thousands grouped by commas (+1,000)", WHOLE.replace("image +100", "image +1,000"), True)
judged("a free figure grouped (1,000), over what is free", WHOLE.replace("free:image 50", "free:image 1,000"), False)
judged("the image over its allowance", WHOLE, False, so={**SO, "image": 1101})
judged("a stack root over its allowance", WHOLE, False, st={**ST, "fm1_cstart": (109, [])})
judged("too little of the slot free", WHOLE.replace("free:image 50", "free:image 901"), False)
MAXW = WHOLE + " max:image 1500 max:core:image 300 max:core:ram 20 max:content 700 max:ram:rest 40"
SOX = {**SO, "core:image": 250, "core:ram": 20, "content": 600, "ram:rest": 30}
judged("the Gnomad engines' hard lines (max:), every figure inside", MAXW, True, so=SOX)
judged("the engine core's image over its max:", MAXW, False, so={**SOX, "core:image": 301})
judged("the core's RAM over its max: (by one byte)", MAXW, False, so={**SOX, "core:ram": 21})
judged("a max: figure the build does not have", MAXW, False, so=SO)
refused("a max: figure with a sign", MAXW.replace("max:content 700", "max:content +700"))
refused("a max: key not measured", WHOLE + " max:flash 5")
refused("a max: key twice", MAXW + " max:core:ram 20")
refused("no line for the milestone", "gn-budget OTHER image +1")
refused("two lines for it", WHOLE + "\n" + WHOLE)
refused("an empty line (gn-budget T)", "gn-budget T")
refused("a line without .bss", WHOLE.replace(" .bss +10", ""))
refused("a line without stack:fm1_alnk0_irq", WHOLE.replace(" stack:fm1_alnk0_irq +8", ""))
refused("a key without a value", WHOLE + " pool")
refused("a key not measured", WHOLE + " flash +5")
refused("a key twice", WHOLE + " image +100")
refused("a growth without its sign", WHOLE.replace("image +100", "image 100"))
refused("a free figure with a sign", WHOLE.replace("free:image 50", "free:image +50"))
refused("doubled and trailing commas (+1,,00,)", WHOLE.replace("image +100", "image +1,,00,"))
refused("a trailing comma (+100,)", WHOLE.replace("image +100", "image +100,"))
refused("a doubled comma (+1,,000)", WHOLE.replace("image +100", "image +1,,000"))
refused("a group of two (+1,00)", WHOLE.replace("image +100", "image +1,00"))
refused("a group of four (+1,0000)", WHOLE.replace("image +100", "image +1,0000"))
refused("a leading comma (+,100)", WHOLE.replace("image +100", "image +,100"))
refused("a free figure badly grouped (1,000,0)", WHOLE.replace("free:image 50", "free:image 1,000,0"))

# round FW-CONTENT: the content's own line (FORK.md "gn-budget CONTENT ..."), the vendored files' sizes against it
CL = "gn-budget CONTENT pitched 49152 kits 13312 pcm 131072 max:content 196152"
HAVE = {"pitched": 49152, "kits": 13312, "pcm": 131072}


def content(what, text, passes, have=HAVE, linked=None, parses=True):
    a, why = B.content_line(text)
    with contextlib.redirect_stdout(io.StringIO()):
        got = B.content_check(have, text, linked)
    report(what, (a is not None) == parses and (got == 0) == passes, why or "")


root = Path(__file__).resolve().parent.parent
real, why = B.content_line((root / "FORK.md").read_text())
with contextlib.redirect_stdout(io.StringIO()):
    sizes = B.content_sizes(root / "firmware" / "gnomad-content")
    verdict = B.content_check(sizes, (root / "FORK.md").read_text())
report("CONTENT: FORK.md's line holds the vendored content", real is not None and verdict == 0,
       why or f"vendored {sizes}, line {real}")
content("CONTENT: the line, the sizes it states", CL, True)
content("CONTENT: grouped by commas (196,152)", CL.replace("196152", "196,152"), True)
content("CONTENT: linked arrays the vendored sizes", CL, True, linked=dict(HAVE))
content("CONTENT: fails on its figures: linked arrays not the vendored ones", CL, False, linked={**HAVE, "pcm": 4000})
content("CONTENT: fails on its figures: a size not the vendored one", CL, False, have={**HAVE, "kits": 13728})
content("CONTENT: fails on its figures: the sum over the cap", CL.replace("196152", "193535"), False)
content("CONTENT: refused: no line", "gn-budget ENGINES image +1", False, parses=False)
content("CONTENT: refused: two lines", CL + "\n" + CL, False, parses=False)
content("CONTENT: refused: a key missing", CL.replace(" kits 13312", ""), False, parses=False)
content("CONTENT: refused: a key not of the line", CL + " wavetables 0", False, parses=False)
content("CONTENT: refused: a key twice", CL + " pcm 131072", False, parses=False)
content("CONTENT: refused: a signed value", CL.replace("pcm 131072", "pcm +131072"), False, parses=False)
content("CONTENT: refused: a key without a value", CL + " pcm", False, parses=False)
print("gn budget tests " + ("FAILED" if fails else "passed"))
sys.exit(1 if fails else 0)
