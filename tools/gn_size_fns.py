#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Kyle Palaniuk
"""Gnomad (FORK.md, "Size diet"): the shipping build's main-loop code built for size beyond upstream's SIZE_FILES.

tools/size_fns.py (upstream's) gives LLVM's minsize to the functions its SIZE_FILES define (the UI, the stores, the
editor, the console) and leaves everything else at -Os. The fork's own screens and Easy (gn_*.c) and a few of
upstream's later UI files (ui_layer.c, ui_name.c, editor_fm6.c, ...) are main-loop code too but are not in that
list. With the engine swap (the IR defines gn_core.c's gnc_render) size_fns.py calls gn_minsize() here and marks
every function minsize except:
  - what the audio and timer interrupts can run: everything fm1_alnk0_irq or fm1_timer5_irq (hal/fm1_isr.S's two
    bodies) names, transitively, in the IR before inlining (a direct call or any other mention), and for an indirect
    call there every function whose address is taken and whose parameter types are the call's (an engine's
    render / note_on / keys / done through engine_t). So the interrupts' code is compiled as before, and the target
    cost lines (tools/slot_cost_reachable.py, tests/target_budget.py) read the same code;
  - the protected files (how the synth starts, installs and updates is upstream's bytes: main.c, ota.c, ota_hw.c,
    usb.c, libc.c, firmware/hal), and any function placed in a section of its own (.ram_text);
  - the Gnomad core (firmware/gnomad-core/src) and its adapter gn_core.c (the core's own figures stay its own).
Nothing else changes: the same functions, the same calls, the same data; host builds (tests, the simulator) never
run this. Without the swap (the engines-off build for upstream's tests, upstream's own tree) it returns nothing.
  gn_size_fns.py IN.ll    prints how many functions it would mark and the interrupts' reach"""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
FW = ROOT / "firmware"
ROOTS = ("fm1_alnk0_irq", "fm1_timer5_irq")         # hal/fm1_isr.S: FM1_ISR isr_timer5 / isr_alnk0
PROTECTED = ["src/main.c", "src/ota.c", "src/ota_hw.c", "src/usb.c", "src/libc.c", "hal/*.h", "hal/*.c"]
CORE = ["gnomad-core/src/*.c", "gnomad-core/src/*.h", "src/gn_core.c"]
SWAP = "gnc_render"                                 # defined only with the engine swap (gn_core.c)

DEFINE = re.compile(r"^define [^\n]*?@\"?([\w.]+)\"?\(([^\n]*?)\)[^\n]*\{$", re.M)
REF = re.compile(r"@\"?([A-Za-z_][\w.]*)\"?")
ICALL = re.compile(r"\bcall [^@\n]*?%[\w.]+\(([^\n]*)\)")


def c_definitions(text):
    """every function a C file defines: a name and its parameter list followed by '{' (at brace depth 0, or a signature
    at column 0), whatever precedes it (static, RAMFN, __attribute__, a macro). Over-finding keeps more code as it was"""
    text = re.sub(r"/\*.*?\*/|//[^\n]*", " ", text, flags=re.S)
    text = re.sub(r"^[ \t]*#(?:[^\n]*\\\n)*[^\n]*", " ", text, flags=re.M)    # preprocessor lines (a macro's braces)
    text = re.sub(r"\"(?:\\.|[^\"\\\n])*\"|'(?:\\.|[^'\\\n])*'", "\"\"", text)
    while True:                                       # __attribute__((...)) before a name: out, nested parentheses too
        m = re.search(r"\b__attribute__\s*\(", text)
        if not m:
            break
        i, depth = m.end() - 1, 0
        while i < len(text):
            depth += {"(": 1, ")": -1}.get(text[i], 0)
            i += 1
            if depth == 0:
                break
        text = text[:m.start()] + " " + text[i:]
    flat, depth = [], 0
    for ch in text:
        if ch == "}":
            depth -= 1
        flat.append(ch if depth == 0 else " ")
        if ch == "{":
            depth += 1
    flat = "".join(flat)
    found = {m.group(1) for m in re.finditer(r"\b([A-Za-z_]\w*)\s*\(([^;{}]*)\)\s*\{", flat, re.S)}
    # and every signature at column 0 whose ')' is followed by '{' (size_fns.py's rule with any first word): the
    # depth walk above loses its way where #if branches open a brace each
    for m in re.finditer(r"^[A-Za-z_][^;=(\n]*?\b([A-Za-z_]\w*)\s*\(", text, re.M):
        i, depth = m.end() - 1, 0
        while i < len(text):
            depth += {"(": 1, ")": -1}.get(text[i], 0)
            i += 1
            if depth == 0:
                break
        if re.match(r"\s*\{", text[i:i + 200]):
            found.add(m.group(1))
    return found - {"if", "for", "while", "switch", "return", "sizeof"}


def names_in(patterns):
    out = set()
    for p in patterns:
        for f in sorted(FW.glob(p)):
            out |= c_definitions(f.read_text(errors="replace"))
    return out


def param_types(s):
    """an IR argument or parameter list -> its types ("%struct.track* %5, i32 0" and "%struct.track*, i32" alike)"""
    parts, depth, cur = [], 0, ""
    for ch in s:
        depth += ch in "([{<"
        depth -= ch in ")]}>"
        if ch == "," and depth == 0:
            parts.append(cur)
            cur = ""
        else:
            cur += ch
    if cur.strip():
        parts.append(cur)
    out = []
    for a in parts:
        a = re.sub(r" (?:nonnull|noalias|readonly|readnone|nocapture|signext|zeroext|returned|byval|sret|inreg|"
                   r"dereferenceable(?:_or_null)?\(\d+\)|align \d+)\b", "", " " + a.strip())
        a = re.sub(r" (?:%[\w.]+|-?\d+|null|undef|true|false|@[\w.]+)$", "", a)
        out.append(a.strip())
    return tuple(out)


def parse(ir):
    """{function: (its parameter types, its body, its define line)}"""
    fns = {}
    for m in DEFINE.finditer(ir):
        end = ir.find("\n}\n", m.end())
        fns[m.group(1)] = (param_types(m.group(2)), ir[m.end():end], m.group(0))
    return fns


def interrupt_reach(ir, fns):
    """the functions the interrupts can run (see the docstring), and the address-taken ones"""
    refs, taken = {}, set()
    for n, (_, body, _) in fns.items():
        r = set()
        for line in body.split("\n"):
            for m in REF.finditer(line):
                g = m.group(1)
                if g in fns:
                    r.add(g)
                    if not re.search(r"\bcall [^@\n]*@\"?" + re.escape(g) + r"\"?\(", line):
                        taken.add(g)
        refs[n] = r
    for line in ir.split("\n"):                       # function addresses in data (engine_t, callback tables)
        if line.startswith("@"):
            taken |= {m.group(1) for m in REF.finditer(line[1:]) if m.group(1) in fns}
    seen, todo = set(), list(ROOTS)
    while todo:
        n = todo.pop()
        if n in seen or n not in fns:
            continue
        seen.add(n)
        todo.extend(refs[n])
        for m in ICALL.finditer(fns[n][1]):
            want = param_types(m.group(1))
            todo.extend(t for t in taken if fns[t][0] == want)
    return seen, taken


def gn_minsize(ir):
    """the functions to mark minsize besides SIZE_FILES' (empty without the engine swap)"""
    fns = parse(ir)
    if SWAP not in fns:
        return set()
    reach, _ = interrupt_reach(ir, fns)
    keep = reach | names_in(PROTECTED) | names_in(CORE)
    keep |= {n for n, (_, _, line) in fns.items() if " section " in line}
    return set(fns) - keep


def main(path):
    ir = Path(path).read_text()
    fns = parse(ir)
    reach, taken = interrupt_reach(ir, fns)
    marked = gn_minsize(ir)
    print(f"gn_size_fns: {len(fns)} functions, {len(reach)} the interrupts reach ({len(taken & reach)} through an "
          f"indirect call: {', '.join(sorted(taken & reach))}), {len(marked)} built for size beyond SIZE_FILES")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1]))
