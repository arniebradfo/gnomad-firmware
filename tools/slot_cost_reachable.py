#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Kyle Palaniuk
"""Gnomad (FORK.md, "The target cost lines on the shipping build"): a second target cost scorer, beside
tests/target_budget.py + tests/gn_target_budget.py, to compare against the lines. It reads the same pi32v2
listing (build/felucca.dis). Since its lines were adopted (2026-10-12, tests/gn_target_lines.txt) it is the shipping
build's target cost gate in CI (--lines); the old scorer is printed beside it, not a gate.

What it counts: only code the audio interrupt can run, found by following the calls (as gnomad-core's
tools/divcheck.py walks the render path from gc_mix_frame), and on each function's control-flow graph the WORST
PATH, not the whole body:
  - a branch (if / switch jump table) costs its most expensive arm only, not every arm added up;
  - a loop costs its trip count x its worst iteration; the trip count is read from the code where the compiler
    left it (a counter set to a constant before the loop and compared with a constant at its end), else from
    TRIPS below (each with the source line it comes from), else UNKNOWN_TRIP, and every such loop is listed;
  - a call costs the callee's worst path (recursively); an indirect call costs the most expensive candidate;
  - the core's control tick (gc_ktick: one sample in GC_KRATE = 16 runs the LFO / pitch / coefficient update;
    FM's block of GC_FM_BLOCK = 16 samples in the new core) is counted 1/16 per sample in the "amortised" column
    (exact over a 128-frame half: every voice ticks 8 times) and fully in the "tick sample" column (the one
    sample in 16 that runs it), whichever way round the compiler put the branch;
  - an instruction is 1, the second of a dual-issue pair ("a  #" then an indented partner) 0, a hardware
    divide 1 + 8 (target_budget's DIV_W). Not modelled: multiply latency, cache misses on flash, wait states,
    the clock. So it estimates cycles, it does not measure them (FORK.md: on the synth, the SYSTEM page's
    CPU % and the console's audio_max_us do);
  - Felucca's engines (1.0.1 base, or the ones a build keeps) are scored from the listing only (clean room):
    their outermost loop with no count in the code is the CTL frames (voice.c e->render(t, v, out, n, m)),
    inner ones UNKNOWN_TRIP; the reports name every such loop.
Reports, per slot: one voice's render for one sample (gc_<engine>_render and what it calls) beside the old scorer
and beside 1.0.1's engine of the slot (--base); per interrupt (one 128-frame half): Felucca's frame with every
engine at 0, and 8 voices of the heaviest engine (the core: its four render_common calls), each as a share of the
half's 2.901 ms at an assumed clock (--mhz). Note-on, MIDI and sequencer events are listed apart (EVENT); buffer
clears (silence, PANIC) once, not per sample (BULK).
  tools/slot_cost_reachable.py [DIS] [--base BASE_DIS] [--mhz 320] [--explain FUNCTION] [--lines LINES]
With --lines (CI: tests/gn_target_lines.txt, the lines adopted 2026-10-12) it is the shipping build's target
cost gate: every figure against its line, exit 1 when one is over."""
import argparse
import os
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
LABEL = re.compile(r"^([A-Za-z_.][\w.]*):$")
LINE = re.compile(r"^\s*([0-9a-f]+):\s+((?:[0-9a-f]{2} )+)\s*(.*)$")
DATA = re.compile(r"^<\s*(-?\d+) : 0x[0-9A-Fa-f]+ >")
TGT = re.compile(r"(?:goto|call) (-?\d+)(?: <[^>]*: ([0-9a-f]+) >)?")
RET = re.compile(r"^(\{pc\b.*\} = \[sp\+\+\]|pc = \[sp\+\+\]|rti\b|rts\b)")
TB = re.compile(r"^tb([bh]) \[r\d+\]")
CALLR = re.compile(r"^call r\d+$")
DIV = re.compile(r"= r\d+ / r\d+")

DIV_W = 8                       # tests/target_budget.py's: a divide weighs 1 + 8
KRATE = 16                      # gnomad-core gc_internal.h GC_KRATE: the control tick, one sample in 16
CTL = 32                        # firmware/src/gn_core.c: the core renders Felucca's CTL (32) frames a call
HALF = 128                      # firmware/src/core.h HALF_FRAMES: one audio interrupt renders one half
FS = 44117.6                    # firmware/src/audio.c DAC_TICKS: 24 MHz / 544
BULK = 512                      # a loop of >= 512 trips of <= 6 instructions is a clear (silence, PANIC), counted
                                # once per interrupt half, not per sample (gnomad-core fx.c clear_echo/fdn/chorus)
UNKNOWN_TRIP = 4                # a loop whose count neither the code nor TRIPS gives (target_budget's NEST)
# loops whose bound is a variable, by function: (the n-th such loop in address order, or "*" for all) -> trips.
# Each from the source, for the worst case the firmware can produce.
TRIPS = {
    # gnomad-core core.c render_common: for (n = 0; n < frames; n++) with frames = CTL (gn_core.c gn_core_mix);
    # its other loops walk the block's events (0 in a steady block: the events are costed apart, EVENT)
    "render_common": {"calls gc_mix_frame": CTL, "*": 1},
    "gc_render_mono": {"calls gc_mix_frame": CTL, "*": 1},
    # audio.c fm1_alnk0_irq: for (b = 0; b < HALF_FRAMES; b += CTL) audio_block(...): 4 blocks a half; the
    # loop that reaches the core (gn_core_mix inlined) calls render_common
    "fm1_alnk0_irq": {"calls render_common": HALF // CTL},
    # prims.c gc_udiv_nr: "at most 6 halvings", then correction loops of "a few steps": 6 taken for each
    "gc_udiv_nr": {"*": 6},
    # prims.c gc_udivmod64: shift-subtract, at most 64 steps a loop
    "gc_udivmod64": {"*": 64},
    # gc_internal.h gc_fdivk (inlined everywhere): while (y >> 32) folds the high word by 2^32 mod d; for the
    # core's operands (< 2^40, d >= 2^15) it ends in 2 passes; 3 taken where d is a variable (dr_wmean)
    # fx.c gc_fx_process: the rest are the FDN's for (i = 0; i < 4u; i++) and one gc_fdivk fold: 4 each
    "gc_fx_process": {"*": 4},
    "dr_wmean": {"*": 3},
    # drums.c dr_T3: "q0 <= 3 by subtraction", then gc_fdivk folds; gc_drums_render inlines the same
    "dr_T3": {"*": 3},
    "gc_drums_render": {"*": 3},
    # fx.c echo_encode: binary search over 128 codes, 7 steps
    "echo_encode": {"*": 7},
    # gnome_voice.c: three formants (for f < 3u, for i < 3u)
    "gc_gnome_coeffs": {"*": 3},
    "gc_gnome_render": {"*": 3},
    # sample.c loop wrap: "at most 8 passes: step <= 16 frames, length >= 2"
    "gc_sample_render": {"*": 8},
    # va.c gc_saw_bank: i from 6 down, at most 6 steps (gc_va_render inlines it)
    "gc_saw_bank": {"*": 6},
    "gc_va_render": {"*": 6},
}
# event-rate work the interrupt reaches (a note, a MIDI message, a sequencer step, a sound change, the first
# block's core start): not per sample. The "steady" interrupt leaves these calls out; each is listed with its
# own cost (one call), so the owner can add the events a block can carry.
EVENT = re.compile(
    r"^(gc_(ev_\w+|bind_track|detach_track|init|set_initial|\w+_note_on|\w+_note_off|record_\w+|crc32c|"
    r"sample_assets|sample_group_check|validate\w*|default_\w+|fm_word_ok|\w+_param_range|svf_f|gnome_shift)|"
    r"exec_event|validate_events|rep_core|"
    r"seq_\w+|midi_\w+|key_on|key_off|key_place|chain_\w+|voice_start|voice_kill|mono_\w+|rec_\w+|chord_\w+|"
    r"motion_step|input_on|input_off|gn_panic_track|gn_key|gn_stop_notes|gn_usb_notes_off|gn_midi_event|"
    r"shed_voice|kb_map|gnc_note_on|gnc_push|gnc_add|gnc_record|gnc_preset_rec|gnc_knob|gnc_drum_keys|gn_core_boot|"
    r"engine_range|check_flags_name|trk_note_on|trk_note_off|arp_\w+|"
    # envelope stage entry: once a stage, not per sample (gnomad-core tools/divcheck.py STAGE_ENTRY; FM's stage
    # length fm_stage_n is called only there)
    r"gc_env_segment|gc_ms_frames|gc_env_next|fm_stage_n)$")
ENGINES = [("FM", "gc_fm_render", ["fm6_render", "fm6_op_run", "fm6_op_fb"]),
           ("GNOME", "gc_gnome_render", ["wheel_render", "wheel_block"]),
           ("DRUMS", "gc_drums_render", ["drum_render", "dv_metal_run", "dv_kick_run", "dv_snare_run", "dv_clap_run",
                                         "dv_hat_run", "dv_tom_run", "dv_rim_run", "dv_bell_run", "dv_cym_run",
                                         "dv_out"]),
           ("SAMPLER", "gc_sample_render", ["slice_render", "slc_rev"]),
           ("VA", "gc_va_render", ["phys_render", "px_modal_block", "px_modal_run", "px_memb_block",
                                   "px_string_excite", "px_string_run", "px_symp_run"])]
IRQ = "fm1_alnk0_irq"


class Insn:
    __slots__ = ("addr", "size", "text", "pred", "kind", "tgts", "callee", "paired")

    def __init__(self, addr, size, text, pred):
        self.addr, self.size, self.text, self.pred = addr, size, text, pred
        self.kind, self.tgts, self.callee, self.paired = "op", [], None, False


def parse(path):
    """{function: [Insn]}, {address: function}; jump tables resolved (an entry v -> table start + 2 v)"""
    funcs, cur, pend, depth, bundle = {}, None, None, 0, False
    with open(path, errors="replace") as f:
        for raw in f:
            s = raw.strip()
            m = LABEL.match(s)
            if m:
                name = m.group(1)
                if not name.startswith("."):
                    cur = funcs.setdefault(name, [])
                    depth = 0
                elif pend is not None and name.startswith((".GJTIS", ".GJTIL")):
                    pend[1] = None                       # the table starts at the next data line
                continue
            if s == "}":
                depth = max(0, depth - 1)
                continue
            m = LINE.match(raw)
            if not m or cur is None:
                continue
            addr, size, text = int(m.group(1), 16), len(m.group(2).split()), m.group(3).strip()
            d = DATA.match(text)
            if d:
                if pend is not None:
                    if pend[1] is None:
                        pend[1] = addr
                    pend[0].tgts.append(pend[1] + 2 * (int(d.group(1)) & (0xFF if pend[2] == "b" else 0xFFFF)))
                continue
            if not re.match(r"^[a-z\[{(]", text) and not text.startswith(("r", "if", "goto", "call", "tb")):
                continue                                 # (string data in .text)
            pend = None
            i = Insn(addr, size, text.split("  #")[0].strip(), depth > 0)
            i.paired = bool(cur) and bundle                  # the second of a dual-issue pair: no cycle of its own
            bundle = "  #" in text and not i.paired
            if text.endswith("{") and text.startswith(("if", "ifs")):
                depth += 1
            t = TB.match(i.text)
            if RET.match(i.text):
                i.kind = "ret"
            elif t:
                i.kind = "table"
                pend = [i, None, t.group(1)]
            elif CALLR.match(i.text):
                i.kind = "icall"
            elif i.text.startswith(("goto", "call")) or re.match(r"^ifs? \(.*\) goto ", i.text):
                g = TGT.search(i.text)
                if g:
                    i.tgts = [int(g.group(2), 16) if g.group(2) else addr + size + int(g.group(1))]
                    i.kind = ("call" if i.text.startswith("call") else
                              "jump" if i.text.startswith("goto") and not i.pred else "cond")
            cur.append(i)
    funcs = {n: v for n, v in funcs.items() if v}
    owner = {}
    for n, v in funcs.items():
        for i in v:
            owner[i.addr] = n
    starts = {v[0].addr: n for n, v in funcs.items()}
    for v in funcs.values():
        for i in v:
            if i.kind == "call":
                i.callee = starts.get(i.tgts[0]) or owner.get(i.tgts[0])
    return funcs, starts


class Scorer:
    def __init__(self, funcs, starts, amortise, icall=None, steady=True, skip=(), frames=()):
        self.f, self.starts, self.am = funcs, starts, amortise
        self.skip = set(skip)                            # calls charged 0 (Felucca's frame without the engines)
        self.frames = set(frames)                        # engine renders: the outermost unpinned loop is the CTL frames
        self.icall = icall or []                         # candidates for an indirect call
        self.event = steady                              # EVENT calls left out (listed in self.events)
        self.memo, self.busy = {}, set()
        self.unknown, self.ticks, self.notes, self.events, self.framed = {}, {}, [], {}, set()
        self.once = {}                                   # bulk clears: their cost, counted once a half
        self.paths, self.loops = {}, {}

    # --- a function's worst path --------------------------------------------------------------------------
    def cost(self, name):
        if name in self.memo:
            return self.memo[name]
        if name in self.busy or name not in self.f:
            if name in self.busy:
                self.notes.append(f"recursion through {name}: counted once")
            return 0.0
        self.busy.add(name)
        c = self._cost(name)
        self.busy.discard(name)
        self.memo[name] = c
        return c

    def _cost(self, name):
        ins = self.f[name]
        by = {i.addr: i for i in ins}
        order = [i.addr for i in ins]
        lo, hi = order[0], order[-1]
        fac = {}

        def succ(i):
            if i.kind == "ret":
                return []
            if i.kind == "table":
                return [t for t in i.tgts if lo <= t <= hi and t in by]
            out = []
            if i.kind in ("jump", "cond"):
                t = i.tgts[0]
                if lo <= t <= hi and t in by:
                    out.append(t)
                elif i.kind == "jump":                   # a tail call: its cost, then this function ends
                    return out
            if i.kind != "jump" and i.addr != order[-1]:
                out.append(nxt[i.addr])
            return out

        def own(i):
            w = (0.0 if i.paired else 1.0) + (DIV_W if DIV.search(i.text) else 0)
            callee = None
            if i.kind == "call" and i.callee and i.callee != name:
                callee = i.callee
            elif i.kind == "jump" and i.tgts and not (lo <= i.tgts[0] <= hi):
                callee = self.starts.get(i.tgts[0])
            if callee and callee != name:
                if callee in self.skip:
                    pass
                elif self.event and EVENT.match(callee):
                    self.events[callee] = self.events.get(callee, 0)   # costed apart (report)
                else:
                    w += self.cost(callee)
            elif i.kind == "icall":
                w += max((self.cost(c) for c in self.icall if not (self.event and EVENT.match(c))), default=0.0)
            return w * fac.get(i.addr, 1.0)

        nxt = {a: order[k + 1] for k, a in enumerate(order[:-1])}
        # basic blocks
        leaders = {lo}
        for i in ins:
            if i.kind in ("cond", "jump", "table", "ret"):
                if i.addr in nxt:
                    leaders.add(nxt[i.addr])
                leaders.update(succ(i))
        blocks, cur = {}, None
        for a in order:
            if a in leaders:
                cur = a
                blocks[cur] = []
            blocks[cur].append(by[a])
        bsucc = {b: succ(v[-1]) for b, v in blocks.items()}
        fac.update(self._tick_factor(name, ins, blocks, bsucc))
        bcost = {b: sum(own(i) for i in v) for b, v in blocks.items()}
        # back edges by depth-first search from the entry; natural loops
        color, back, stack = {lo: 1}, {}, [(lo, iter(bsucc[lo]))]
        while stack:
            b, it = stack[-1]
            t = next(it, None)
            if t is None:
                color[b] = 2
                stack.pop()
            elif color.get(t) == 1:
                back.setdefault(t, set()).add(b)
            elif t not in color:
                color[t] = 1
                stack.append((t, iter(bsucc[t])))
        pred = {}
        for b, ss in bsucc.items():
            if b in color:
                for t in ss:
                    pred.setdefault(t, set()).add(b)
        dom = self._dominators(lo, [b for b in blocks if b in color], pred)
        loops = []
        for h, srcs in back.items():
            srcs = {s for s in srcs if h in dom[s]}     # a loop's head dominates its latches
            if not srcs:
                self.notes.append(f"{name}: an irreducible cycle at {hex(h)} (no loop weight; its arms once)")
                continue
            body, work = {h} | srcs, [s for s in srcs if s != h]
            while work:
                x = work.pop()
                for p in pred.get(x, ()):
                    if p not in body and h in dom[p]:
                        body.add(p)
                        work.append(p)
            loops.append((h, body, srcs))
        loops.sort(key=lambda l: len(l[1]))
        parent = {b: b for b in blocks}

        def find(x):
            while parent[x] != x:
                x = parent[x]
            return x

        node_cost = dict(bcost)
        node_succ = {b: list(v) for b, v in bsucc.items()}
        outer = {h for h, body, _ in loops if not any(body < b2 for _, b2, _ in loops)}
        done = []
        for h, body, srcs in loops:
            head = ("L", h)
            inner = set().union(*[d for d in done if d < body]) if done else set()
            done.append(body)
            members = {find(b) for b in body}
            start = find(h)
            memo = {}

            def walk(n, stack=()):
                if n in memo:
                    return memo[n]
                if n in stack:
                    return 0.0
                best = 0.0
                for t in node_succ[n]:
                    rt = find(t)
                    if rt == start or rt not in members or rt == n:
                        continue                          # next iteration, or out of the loop
                    best = max(best, walk(rt, stack + (n,)))
                memo[n] = node_cost[n] + best
                return memo[n]

            it = walk(start)
            trip, how = self._trip(name, blocks, body, srcs, h, order, by, dom, bsucc, inner, h in outer)
            outs = [t for n in members for t in node_succ[n] if find(t) not in members]
            parent[head] = head
            for n in members:
                parent[n] = head
            self.loops.setdefault(name, []).append((h, trip, how, it, set(body)))
            if trip >= BULK and it <= 6:                 # a buffer clear (gc_zero inlined): once, not per call
                self.once[f"{name} {hex(h)}"] = trip * it
                node_cost[head] = 0.0
                how = "code"
            else:
                node_cost[head] = trip * it
            node_succ[head] = outs
            if how != "code":
                self.unknown.setdefault(name, []).append(
                    (hex(h), sum(len(blocks[b]) for b in body), trip, how, round(it, 1)))
        memo = {}

        def top(n, stack=()):
            if n in memo:
                return memo[n]
            if n in stack:
                return 0.0
            best = 0.0
            for t in node_succ.get(n, []):
                rt = find(t)
                if rt != n:
                    best = max(best, top(rt, stack + (n,)))
            memo[n] = node_cost.get(n, 0.0) + best
            return memo[n]

        sys.setrecursionlimit(max(sys.getrecursionlimit(), 50000))
        total = top(find(lo))
        path, n = [], find(lo)
        while n is not None:                             # the worst path, for --explain
            path.append((n, node_cost.get(n, 0.0)))
            nxt_n, best = None, -1.0
            for t in node_succ.get(n, []):
                rt = find(t)
                if rt != n and rt in memo and memo[rt] > best and rt not in [p for p, _ in path]:
                    nxt_n, best = rt, memo[rt]
            n = nxt_n
        self.paths[name] = (path, blocks)
        return total

    @staticmethod
    def _dominators(entry, nodes, pred):
        """{block: the set of blocks that dominate it} (iterative data flow, reverse post-order not needed here)"""
        allset = set(nodes)
        dom = {n: set(allset) for n in nodes}
        dom[entry] = {entry}
        changed = True
        while changed:
            changed = False
            for n in nodes:
                if n == entry:
                    continue
                ps = [dom[p] for p in pred.get(n, ()) if p in dom]
                new = (set.intersection(*ps) if ps else set()) | {n}
                if new != dom[n]:
                    dom[n] = new
                    changed = True
        return dom

    # --- loop trip counts ---------------------------------------------------------------------------------
    def _trip(self, name, blocks, body, srcs, h, order, by, dom, bsucc, inner, outermost):
        insns = [i for b in body for i in blocks[b]]
        own_ins = [i for b in body - inner for i in blocks[b]]       # not in a loop nested in this one
        control = {b for b in body - inner if b in srcs or any(t not in body for t in bsucc[b])}
        bof = {i.addr: b for b in body for i in blocks[b]}
        first = min(i.addr for i in insns)
        before = [by[a] for a in order if a < first][::-1][:40]

        def init(reg):
            for i in before:
                m = re.match(rf"^{reg} = (-?\d+)(?: <.*)?$", i.text)
                if m:
                    v = int(m.group(1))
                    return v - (1 << 32) if v >= 1 << 31 else v    # a 32-bit register: 4294923196 is -44100
                pair = re.match(r"^(r\d+)_(r\d+) = 0$", i.text)
                if pair and reg in pair.groups():
                    return 0
                if re.match(rf"^({reg}\b[^=]*=|r\d+_{reg} |{reg}_r\d+ )", i.text):
                    return None
            return None

        def count(op, c, lim, s, step_first):
            """iterations until the continue condition (r op lim) fails, r = c at entry, + s an iteration"""
            if op == "!=":
                n = (lim - c) / s
                n = n if float(n).is_integer() else -1
            elif op in ("<", "<=") and s > 0:
                n = -(-(lim - c) // s) if op == "<" else (lim - c) // s + 1
            elif op in (">", ">=") and s < 0:
                n = -(-(c - lim) // -s) if op == ">" else (c - lim) // -s + 1
            else:
                return None
            n = int(n) + (0 if step_first else 1)
            return n if 0 < n < 100000 else None

        for i in (i for b in control for i in blocks[b][-1:]):
            m = re.match(r"^if \(--(r\d+) != 0\) goto", i.text)
            if m and i.tgts and bof.get(i.tgts[0]) in body:
                c = init(m.group(1))
                if c is not None and c > 0:
                    return c, "code"
            m = re.match(r"^ifs? \((r\d+) (!=|==|<|<=|>|>=) (-?\d+|r\d+)\) goto", i.text)
            if not m or i.kind != "cond":
                continue
            reg, op, lim = m.groups()
            inside = bof.get(i.tgts[0]) in body
            if op == "==" and not inside:
                op = "!="                                # exit when equal: continue while not equal
            elif not inside:
                continue
            elif op == "==":
                continue
            if lim.startswith("r"):
                lim = None if any(re.match(rf"^{lim}\b[^=]*=", j.text) for j in insns) else init(lim)
            else:
                lim = int(lim)
            stp = re.compile(rf"^{reg} (?:\+= (-?\d+)|= {reg} \+ (-?\d+))$")
            steps = [j for j in own_ins if stp.match(j.text)]
            c = init(reg)
            if lim is None or c is None or len(steps) != 1:
                continue
            st = steps[0]
            g = stp.match(st.text)
            sv = int(g.group(1) or g.group(2))
            if not sv:
                continue
            sb, tb = bof[st.addr], bof[i.addr]
            step_first = st.addr < i.addr if sb == tb else sb in dom.get(tb, ())
            n = count(op, c, lim, sv, step_first)
            if n:
                return n, "code"
        if outermost and name in self.frames:
            return CTL, "frames: an engine render's outermost loop, n = CTL (voice.c e->render(t, v, out, n, m))"
        tr = TRIPS.get(name)
        if tr:
            for key, n in tr.items():
                if key.startswith("calls ") and (name, key) not in self.framed and any(
                        i.kind == "call" and i.callee == key[6:] for i in insns):
                    self.framed.add((name, key))         # the innermost loop around that call only
                    return n, f"TRIPS {key}"
            if "*" in tr:
                return tr["*"], "TRIPS *"
        return UNKNOWN_TRIP, "unknown"

    # --- the control tick (gc_ktick) ----------------------------------------------------------------------
    def _tick_factor(self, name, ins, blocks, bsucc):
        """{address: 1/(MASK+1)} for the control tick's arm (k = b[..]; k' = (k + 1) & MASK; then the branch on
        k == 0, either way round: "if (k != 0) goto JOIN" with the tick arm falling through (the old core), or
        "if (k == 0) goto TICK" with the other arm falling through to a goto JOIN (the new core 6b11ee7: gc_ktick in
        gc_drums_render, FM's block of GC_FM_BLOCK 16 in gc_fm_render)). The tick arm is every block reached from
        its entry and not from the other arm's, wherever the compiler put it. MASK is 2^n - 1 as in the code (15 for
        GC_KRATE 16 and GC_FM_BLOCK 16). Empty in the "tick sample" mode."""
        if not self.am:
            return {}
        fac = {}
        bof = {}
        for b, v in blocks.items():
            for i in v:
                bof[i.addr] = b

        def reach(b0):
            seen, work = set(), [b0]
            while work:
                b = work.pop()
                if b in seen:
                    continue
                seen.add(b)
                work.extend(bsucc[b])
            return seen

        for k, i in enumerate(ins):
            m = re.match(r"^if \((r\d+) (!=|==) 0\) goto", i.text)
            if not m or i.kind != "cond" or i.tgts[0] <= i.addr or k + 1 >= len(ins):
                continue
            r, op = m.groups()
            window = ins[max(0, k - 24):k]
            inc = [w for w in window if re.match(rf"^(r\d+) = {r} \+ 1$", w.text)]
            if not inc:
                continue
            q = re.match(r"^(r\d+)", inc[-1].text).group(1)
            mask = None
            for w in window:
                mm = re.match(rf"^{q} = {q} & 0x([0-9A-F]+)$|^{q} &= (\d+)$", w.text)
                if mm:
                    v = int(mm.group(1), 16) if mm.group(1) else int(mm.group(2))
                    if v > 0 and v & (v + 1) == 0:       # 2^n - 1
                        mask = v
            if mask is None or bof.get(i.tgts[0]) is None:
                continue
            taken, fall = bof[i.tgts[0]], bof[ins[k + 1].addr]
            if op == "!=":                               # the tick arm falls through; JOIN is the branch's target
                tick, join = fall, taken
            else:                                        # the tick arm is the target; JOIN is where the other arm,
                tick, join, ft = taken, None, reach(taken)   # walked in order, first meets code the tick arm reaches
                order, k2 = [fall], 0
                while k2 < len(order) and join is None:
                    b = order[k2]
                    k2 += 1
                    if b != tick and b in ft:
                        join = b
                    order.extend(t for t in bsucc[b] if t not in order)
            seen, work = set(), [tick]
            while work:
                b = work.pop()
                if b in seen or b == join:
                    continue
                seen.add(b)
                work.extend(bsucc[b])
            n = 0
            for b in seen:
                for j in blocks[b]:
                    fac[j.addr] = 1.0 / (mask + 1)
                    n += 1
            self.ticks.setdefault(name, []).append((hex(i.addr), n))
        return fac


def reachable(funcs, root, icall):
    seen, stack = set(), [root]
    while stack:
        n = stack.pop()
        if n in seen or n not in funcs:
            continue
        seen.add(n)
        for i in funcs[n]:
            if i.kind == "call" and i.callee:
                stack.append(i.callee)
            elif i.kind == "icall":
                stack.extend(icall)
    return seen


def icall_candidates(funcs):
    """an indirect call on the interrupt path is an engine's render / done / note_on (voice.c's engine_t): the
    engines in the image (Felucca's *_render and the Gnomad shim's gnc_*), the most expensive taken"""
    skip = {"render_common", "gc_render_mono", "gc_render_stereo", "uac_render_start", "project_restore_runtime",
            "gn_size_render"}
    return sorted(n for n in funcs if (n.endswith("_render") and not n.startswith("gc_") and n not in skip)
                  or n.startswith("gnc_"))


def old_scores(dis):
    """the old scorer's figures for the same listing (tests/target_budget.py + tests/gn_target_budget.py),
    computed with their own functions"""
    import importlib.util
    argv, sys.argv = sys.argv, [sys.argv[0], dis]
    try:
        mods = {}
        for nm in ("target_budget", "gn_target_budget"):
            spec = importlib.util.spec_from_file_location(nm, ROOT / "tests" / f"{nm}.py")
            mods[nm] = importlib.util.module_from_spec(spec)
            spec.loader.exec_module(mods[nm])
    finally:
        sys.argv = argv
    tb, gn = mods["target_budget"], mods["gn_target_budget"]
    sys.path.insert(0, str(ROOT / ".github" / "ci"))
    import gn_budget as B
    old = os.getcwd()
    try:
        os.chdir(ROOT)
        names = B.core_names(Path("firmware/src"))
    finally:
        os.chdir(old)
    fns = gn.all_functions(tb, dis)
    base = {}
    for line in open(ROOT / "tests" / "target_budget.txt"):
        p = line.split()
        if len(p) >= 2 and not line.startswith("#"):
            base[p[0]] = int(p[1])
    per = {}
    for n in fns:
        if n.split(".")[0] in names and fns[n]:
            e = names[n.split(".")[0]]
            per[e] = per.get(e, 0) + gn.whole(tb, fns[n])
    eng = {"FM": "FM", "GNOME": "Gnome Voice", "DRUMS": "Drums", "SAMPLER": "Sample", "VA": "VA"}
    out = {}
    for label, _, fl in ENGINES:
        out[label] = (per.get(eng[label], 0), sum(base.get(f, 0) for f in fl))
    irq = tb.cost(fns[IRQ])["cost"] if fns.get(IRQ) else 0
    rc = tb.cost(fns["render_common"])["cost"] if fns.get("render_common") else 0
    out["IRQ"] = (irq + rc, base.get(IRQ, 0))
    return out


def score(dis, mhz):
    funcs, starts = parse(dis)
    ic = icall_candidates(funcs)
    fel = [n for n in ic if not n.startswith("gnc_")]     # Felucca's engine renders in this image
    res = {"dis": dis, "icall": ic, "half_cycles": HALF / FS * mhz * 1e6}
    for mode in ("amortised", "tick"):
        s = Scorer(funcs, starts, mode == "amortised", ic, frames=fel)
        r = {}
        for label, fn, old in ENGINES:
            if fn in funcs:
                r[label] = s.cost(fn)
            if old[0] in funcs:                          # the 1.0.1 engine of the slot: one voice, one sample
                r["felucca " + label] = s.cost(old[0]) / CTL
        r["felucca max"] = max((s.cost(n) / CTL for n in fel), default=0.0)
        for fn in ("gc_mix_frame", "render_common", IRQ):
            r[fn] = s.cost(fn) if fn in funcs else None
        res[mode] = r
        res[mode + "_scorer"] = s
    frame = Scorer(funcs, starts, True, [], skip={"render_common"}, frames=fel)
    res["frame"] = frame.cost(IRQ) if IRQ in funcs else None   # Felucca's half without any engine or the core
    res["frame_scorer"] = frame
    seen = reachable(funcs, IRQ, ic)
    res["reached"] = sorted(seen)
    full = Scorer(funcs, starts, True, ic, steady=False, frames=fel)
    res["events"] = {n: full.cost(n) for n in seen if EVENT.match(n)}
    return res


def half_engines(r):
    """one half's engine work at the worst: the core's four render_common calls, or Felucca's 8 voices of its
    heaviest engine (voice.c: one render a voice a CTL block)"""
    fel = 8 * HALF * r["felucca max"]
    if r.get("render_common") is not None:              # (gn/engines: Felucca's kept engines share the 8 voices)
        return max(HALF // CTL * r["render_common"], fel)
    return fel


def report(res, old, mhz, base=None):
    am, tk, hc = res["amortised"], res["tick"], res["half_cycles"]
    bam = base["amortised"] if base else {}
    p = print
    p(f"## slot_cost_reachable: {res['dis']}" + (f" (1.0.1 base: {base['dis']})" if base else ""))
    p(f"\nPer voice, per sample: the worst path of the engine's render and its callees (1 an instruction, a dual-issue "
      f"pair 1, a divide {1 + DIV_W}); the control tick 1/{KRATE} (amortised) or whole (the tick sample). "
      f"1.0.1's engine in the slot: its render for one voice, one CTL block, / {CTL}, by the same rules.\n")
    p(f"| Slot | old scorer | old line | old x | reachable, amortised | reachable, tick sample | 1.0.1 engine, "
      f"same rules | x 1.0.1 | 8 voices, a half @ {mhz:g} MHz |")
    p("| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |")
    for label, _, fl in ENGINES:
        if label not in am:
            continue
        og, ol = old.get(label, (0, 0))
        f = bam.get("felucca " + label)
        p(f"| {label} ({fl[0].split('_')[0].upper()}'s slot) | {og:,} | {ol:,} | {og / max(ol, 1):.1f} | {am[label]:,.0f} | "
          f"{tk[label]:,.0f} | " + (f"{f:,.0f} | {am[label] / f:.2f}" if f else "- | -") +
          f" | {8 * HALF * am[label] / hc * 100:.0f} % |")
    p(f"\nOne audio interrupt (one {HALF}-frame half = {HALF / FS * 1000:.3f} ms = {hc:,.0f} cycles at an assumed "
      f"{mhz:g} MHz; the shed threshold is 85 %), steady (no note, MIDI or step event in the half):\n")
    p("| Part of the half | this build | % of the half | 1.0.1 base | % |")
    p("| --- | ---: | ---: | ---: | ---: |")
    rows = [("Felucca's frame: sequencer, FX layer, reverb, master (engines and core 0)", res["frame"],
             base["frame"] if base else None),
            ("engines at the worst: 8 voices of the heaviest engine (core: 4 x render_common)", half_engines(am),
             half_engines(bam) if base else None)]
    rows.append(("total", rows[0][1] + rows[1][1], (rows[0][2] + rows[1][2]) if base else None))
    for name, v, bv in rows:
        p(f"| {name} | {v:,.0f} | {v / hc * 100:.0f} % | " + (f"{bv:,.0f} | {bv / hc * 100:.0f} %" if bv else "- | -") + " |")
    og, ol = old.get("IRQ", (0, 0))
    p(f"\nOld scorer, the same interrupt: {og:,} against the line {ol:,} (x{og / max(ol, 1):.2f}). "
      f"gc_mix_frame (one frame, 8 voices on the heaviest engine, FX, master): {am['gc_mix_frame'] or 0:,.0f} "
      f"amortised, {tk['gc_mix_frame'] or 0:,.0f} on a tick sample.")
    s = res["amortised_scorer"]
    once = {**s.once, **res["frame_scorer"].once}
    if once:
        p("\nBuffer clears (silence, PANIC), counted once, not per sample: " +
          "; ".join(f"{k} {v:,.0f}" for k, v in sorted(once.items())))
    ev = sorted(((n, c) for n, c in res["events"].items() if n.startswith(("gc_", "gnc_"))), key=lambda x: -x[1])
    p("\nThe core's event work, left out of the steady half (one call each, worst path; Felucca's own event paths, "
      "the same code as 1.0.1's, are not scored): " + "; ".join(f"{n} {c:,.0f}" for n, c in ev[:12]))
    p("\nControl ticks (function: branch, instructions at 1/16): " +
      "; ".join(f"{n} {v}" for n, v in sorted(s.ticks.items())))
    p("\nLoops whose count is not in the code (function: head, instructions, trips used, from where, worst iteration):")
    for n, v in sorted(s.unknown.items()):
        if n in res["reached"]:
            p(f"- {n}: " + "; ".join(f"{h} {sp} x{tr} ({how}, {it:,})" for h, sp, tr, how, it in v))
    irr = sorted({n.split(":")[0] for n in s.notes if "irreducible" in n})
    if irr:
        p(f"\nIrreducible cycles (a backward jump into shared code, e.g. a switch's impossible default; each arm "
          f"counted once, no loop weight) in: {', '.join(irr)}")


def figures(res):
    """the figures the lines gate: each slot's amortised voice-sample, and the steady interrupt half (Felucca's frame
    + 8 voices of the dearest engine), as the report's tables print them"""
    am = res["amortised"]
    out = {label: am[label] for label, _, _ in ENGINES if label in am}
    if res.get("frame") is not None:
        out["IRQ"] = res["frame"] + half_engines(am)
    return out


def read_lines(path):
    """{name: line} from a lines file (tests/gn_target_lines.txt): 'NAME VALUE  # source', '#' comments"""
    lines = {}
    for ln in Path(path).read_text().splitlines():
        p = ln.split("#", 1)[0].split()
        if not p:
            continue
        if len(p) != 2 or not p[1].isdigit():
            raise ValueError(f"{path}: not 'NAME VALUE': {ln!r}")
        lines[p[0]] = int(p[1])
    return lines


def check_lines(figs, lines, path="lines"):
    """print each figure against its line; returns the failures (over a line, a line with no figure, a figure with
    no line). A figure over its line fails as it is: the line is the gate, no tolerance on top"""
    fail = 0
    p = print
    p(f"\nThe lines ({path}): a figure over its line fails.\n")
    p("| Line | figure | line | figure / line | |")
    p("| --- | ---: | ---: | ---: | --- |")
    for name in list(lines) + [n for n in figs if n not in lines]:
        got, line = figs.get(name), lines.get(name)
        if got is None or line is None:
            p(f"| {name} | {'-' if got is None else f'{got:,.0f}'} | {'-' if line is None else f'{line:,}'} | - | "
              f"FAIL: {'no figure for this line' if got is None else 'no line for this figure'} |")
            fail += 1
            continue
        ok = round(got) <= line
        p(f"| {name} | {got:,.0f} | {line:,} | {got / line:.3f} | " +
          ("ok" if ok else f"FAIL: OVER by {round(got) - line:,} ({(got / line - 1) * 100:+.1f} %; a decision for the line)") + " |")
        fail += not ok
    p(f"\ntarget cost lines: {fail} failing of {len(lines)} (each figure rounded to a whole cycle, as printed)")
    return fail


def explain(s, name, depth=0, floor=0.02, seen=None):
    """the worst path of a function: each node worth >= floor of it (a loop: trips, its calls; a block: its calls)"""
    seen = seen if seen is not None else set()
    total = s.cost(name)
    if name in seen or name not in s.paths:
        return
    seen.add(name)
    path, blocks = s.paths[name]
    print(f"{'  ' * depth}{name}: {total:,.0f}")
    for n, c in path:
        if c < floor * total or c < 20:
            continue
        if isinstance(n, tuple):
            explain_loop(s, name, n[1], c, blocks, depth + 1, floor * total)
        else:
            calls = [i.callee for i in blocks[n] if i.kind == "call" and i.callee]
            print(f"{'  ' * depth}  block {hex(n)}: {c:,.0f}" + (f" calls {', '.join(calls)}" if calls else ""))
            for cl in calls:
                if s.cost(cl) >= floor * total and depth < 3:
                    explain(s, cl, depth + 2, floor, seen)


def explain_loop(s, name, h, c, blocks, depth, floor):
    lp = {l[0]: l for l in s.loops.get(name, [])}
    _, trip, how, it, body = lp[h]
    print(f"{'  ' * depth}loop at {hex(h)}: {c:,.0f} = {trip} trips ({how}) x worst iteration {it:,.0f}")
    inner = sorted((l for l in s.loops.get(name, []) if l[0] != h and l[0] in body and l[4] < body),
                   key=lambda l: -l[1] * l[3])
    for l in inner[:6]:
        if l[1] * l[3] >= floor / max(trip, 1):
            print(f"{'  ' * depth}  inner loop at {hex(l[0])}: {l[1]} trips ({l[2]}) x {l[3]:,.0f}")
    calls = {}
    for b in body:
        for i in blocks[b]:
            if i.kind == "call" and i.callee and not (s.event and EVENT.match(i.callee)):
                calls[i.callee] = s.cost(i.callee)
    for cl, cc in sorted(calls.items(), key=lambda x: -x[1])[:12]:
        if cc * trip >= floor:
            print(f"{'  ' * depth}  call {cl}: {cc:,.0f} a call")


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("dis", nargs="?", default="build/felucca.dis")
    ap.add_argument("--mhz", type=float, default=320.0, help="assumed CPU clock (AC79: up to 320 MHz)")
    ap.add_argument("--base", metavar="BASE_DIS", help="upstream 1.0.1's listing, scored by the same rules")
    ap.add_argument("--explain", metavar="FUNCTION", help="print FUNCTION's worst path (amortised, steady)")
    ap.add_argument("--lines", metavar="LINES", help="gate the figures on these lines (tests/gn_target_lines.txt): "
                    "exit 1 when one is over its line or the listing is missing")
    a = ap.parse_args()
    if not os.path.exists(a.dis):
        print(f"slot_cost_reachable: {'FAIL' if a.lines else 'skip'} ({a.dis} missing)")
        return 1 if a.lines else 0
    res = score(a.dis, a.mhz)
    if a.explain:
        explain(res["amortised_scorer"], a.explain)
        return 0
    base = a.base if a.base and os.path.exists(a.base) else None
    if a.base and not base:
        print(f"slot_cost_reachable: the 1.0.1 base listing {a.base} is missing: its columns are left out")
    report(res, old_scores(a.dis), a.mhz, score(base, a.mhz) if base else None)
    if a.lines:
        return 1 if check_lines(figures(res), read_lines(a.lines), a.lines) else 0
    return 0


if __name__ == "__main__":
    sys.exit(main())
