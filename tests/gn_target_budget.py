#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Kyle Palaniuk
"""Gnomad (FORK.md, "Gnomad engines"): what upstream's target cost check (tests/target_budget.py) needs to know about
the engine swap. target_budget.py calls it when this file is there; it changes nothing for a build without the swap.

1. Retired engines: the engines the build compiled out, read from the build's own configuration (build/gn_engines.txt,
   written by .github/ci/build_info.py --engines from JieLi's preprocessor on felucca.c), never from a list here. A
   compiled-out engine's functions are skipped and printed "retired"; one that is still in the disassembly fails (the
   configuration and the image disagree). A kept engine's function that is missing fails as before.
2. The core (with GN_ENGINES 1): every core function in build/felucca.dis is printed with its cost. The core works per
   sample through calls (render_common's frame loop -> gc_mix_frame -> each voice's engine render), so target_budget's
   loop-only cost of an engine render is 0; here a core function's cost is its whole body, weighted as target_budget
   weights a loop body (x4 a nesting level, divides x9): an upper bound per sample (every branch counted).
   - per slot: the Gnomad engine's functions (those its source file defines, gn_budget.core_names) against the sum of
     the retired engine's lines in tests/target_budget.txt (GNOME in 7: WHEEL's, VA in 9: PHYS's, DRUMS in 10: DRUM's,
     FM in 12: FM6's, SAMPLER in 13: SLICE's), with target_budget's own +10 %: no other headroom;
   - the audio interrupt: fm1_alnk0_irq's cost plus the core's per-block work (render_common, which the interrupt
     reaches through a call) against fm1_alnk0_irq's line, +10 %.
   Over a line fails; the numbers are printed so the line can be decided (FORK.md)."""
import os
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / ".github" / "ci"))
CONFIG = Path("build/gn_engines.txt")
# each engine flag -> the target_budget.py FUNCS entries of that engine (its own source files' functions)
ENGINE_FUNCS = {
    "GN_DROP_ANALOG": ["analog_render"], "GN_DROP_PHASE": ["phase_render"], "GN_DROP_LOFI": ["lofi_render"],
    "GN_DROP_SAMPLE": ["sample_render"], "GN_DROP_VOICE": ["formant_render"], "GN_DROP_TRIO": ["trio_render", "trio_pass"],
    "GN_DROP_WHEEL": ["wheel_render", "wheel_block"], "GN_DROP_GRAIN": ["grain_render", "grain_block"],
    "GN_DROP_PHYS": ["phys_render", "px_modal_block", "px_modal_run", "px_memb_block", "px_string_excite",
                     "px_string_run", "px_symp_run"],
    "GN_DROP_DRUM": ["drum_render", "dv_metal_run", "dv_kick_run", "dv_snare_run", "dv_clap_run", "dv_hat_run",
                     "dv_tom_run", "dv_rim_run", "dv_bell_run", "dv_cym_run", "dv_out"],
    "GN_DROP_NOISE": ["noise_render"], "GN_DROP_FM6": ["fm6_render", "fm6_op_run", "fm6_op_fb"],
    "GN_DROP_SLICE": ["slice_render", "slc_rev"],
}
# the Gnomad engine in each retired engine's slot (firmware/src/gn_core.c)
SLOTS = [("GNOME", "Gnome Voice", "GN_DROP_WHEEL"), ("VA", "VA", "GN_DROP_PHYS"), ("DRUMS", "Drums", "GN_DROP_DRUM"),
         ("FM", "FM", "GN_DROP_FM6"), ("SAMPLER", "Sample", "GN_DROP_SLICE")]


def config():
    """the build's flags as compiled; {} when build_info.py --engines did not run (no swap assumed: nothing skipped)"""
    if not CONFIG.exists():
        return {}
    return dict(ln.split()[:2] for ln in CONFIG.read_text().splitlines() if len(ln.split()) >= 2)


def retired(cfg):
    return {f for flag, fs in ENGINE_FUNCS.items() if cfg.get(flag) == "1" for f in fs}


def whole(tb, insns):
    """a per-sample function's cost: every instruction, x NEST per loop level inside it, divides x (1 + DIV_W)"""
    lo, heads, w = insns[0][0], {}, 0
    for a, t in insns:
        m = tb.TARGET.search(t)
        if m and lo <= int(m.group(1), 16) < a:
            h = int(m.group(1), 16)
            heads[h] = max(heads.get(h, a), a)
    for a, t in insns:
        k = tb.NEST ** min(sum(1 for h, e in heads.items() if h <= a <= e), tb.MAXD)
        if re.search(r"= r\d+ / r\d+", t):
            k *= 1 + tb.DIV_W
        w += k
    return w


def all_functions(tb, path):
    out, cur = {}, None
    with open(path) as f:
        for line in f:
            m = tb.LABEL.match(line.strip())
            if m:
                if not m.group(1).startswith("."):
                    cur = out.setdefault(m.group(1), [])
                continue
            m = tb.INSN.match(line)
            if cur is not None and m and not m.group(3).lstrip().startswith("<"):
                cur.append((int(m.group(1), 16), m.group(3).strip()))
    return out


def check(tb, dis, res, base, cfg):
    """after target_budget's own lines: the retired engines' functions, the core's; returns the failures"""
    fail = 0
    gone = retired(cfg)
    for n in sorted(gone & set(res)):
        print(f"target: FAIL {n} is compiled out by the build's configuration ({CONFIG}) but is in {dis}")
        fail += 1
    if cfg.get("GN_ENGINES") != "1":
        return fail
    import gn_budget as B
    old = os.getcwd()                                   # (gn_budget reads the tree relative to the root)
    try:
        os.chdir(ROOT)
        names = B.core_names(Path("firmware/src"))
    finally:
        os.chdir(old)
    fns = all_functions(tb, dis)
    core = {n: fns[n] for n in fns if n.split(".")[0] in names and fns[n]}
    per = {}
    for n in sorted(core):
        w = whole(tb, core[n])
        per.setdefault(names[n.split(".")[0]], 0)
        per[names[n.split(".")[0]]] += w
        print(f"target: core {n:20s} {len(core[n]):5d} instructions, {tb.cost(core[n])['cost']:6d} loop cost, "
              f"{w:6d} per call ({names[n.split('.')[0]]})")
    for label, engine, flag in SLOTS:
        b = sum(base.get(f, 0) for f in ENGINE_FUNCS[flag])
        got = per.get(engine, 0)
        ok = got <= b * (1 + tb.TOL)
        print(f"target: slot {label:8s} {got:6d} (its functions per sample) against {ENGINE_FUNCS[flag][0].split('_')[0].upper()}'s "
              f"lines {b:5d} (+{tb.TOL * 100:.0f} %): " + ("ok" if ok else
              f"FAIL: OVER THE SLOT'S BUDGET (x{got / max(b, 1):.1f}; a decision for the line, FORK.md)"))
        fail += not ok
    irq, rc = res.get("fm1_alnk0_irq"), fns.get("render_common")
    if irq and rc and "fm1_alnk0_irq" in base:
        total = irq["cost"] + tb.cost(rc)["cost"]
        ok = total <= base["fm1_alnk0_irq"] * (1 + tb.TOL)
        print(f"target: fm1_alnk0_irq with the core's per-block work (render_common) {total} against its line "
              f"{base['fm1_alnk0_irq']} (+{tb.TOL * 100:.0f} %): " + ("ok" if ok else
              f"FAIL: OVER (x{total / base['fm1_alnk0_irq']:.2f}; a decision for the line, FORK.md)"))
        fail += not ok
    return fail
