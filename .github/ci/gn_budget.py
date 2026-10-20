#!/usr/bin/env python3
# Gnomad, a fork of Felucca (see FORK.md). Not part of upstream Felucca.
"""Easy's budgets from the linker, against the base (gn/base-1.0.1) built by the same toolchain in the
same job.

  gn_budget.py OURS BASE        (each a build/ folder: felucca.bin, felucca.syms, felucca.dis; or the
                                 CI's <tag>-budget-inputs ours/ and base/: .syms, .dis.gz, .bin.size)

1. Flash and RAM: the image (what is installed: .text, the .ram_text and .data images after it), .text
   (code and constants: the image up to .ram_text's load address), .ram_text, RAM .data and .bss apart (.data's initial values are also in the
   image), the pool, .noinit; each with the delta against the base.
2. The fork's own share: the symbols named by firmware/src/gn_*.c (functions, tables, state; a static
   function the compiler inlined is in its caller's code instead), by section.
3. A static stack estimate from each disassembly: each function's frame read off its prologue (pushes
   and stack-pointer moves), the deepest call path from each root (the main loop's fm1_cstart, the audio
   interrupt fm1_alnk0_irq, the key-scan interrupt fm1_timer5_irq, which nests into the audio one, and
   ui_draw, ui_input, ed_service); recursion counted once; Felucca's own calls through pointers not
   followed (stack() says which are). An estimate, not a measurement: the same method for both builds,
   so the difference is what Easy adds.
4. With GN_MILESTONE set (CI): each figure against the allowance FORK.md states for the milestone (a line
   "gn-budget <MILESTONE> key value ..."; GN_BUDGET_LINE names another line than the milestone's); any over it,
   or no line, and the script exits 1 (the run fails).
5. The Gnomad engines (FORK.md, "Gnomad engines"), on every run: the sound core's share of the image and RAM, by the
   names its vendored sources define (firmware/gnomad-core/src: functions, tables, statics; a table by the
   GC_TABLES_DEFINE_* block it is generated in, a function by its file), with the adapter's arena (gnc_state) as
   the core's RAM and its effect pool (gnc_fx) apart; the content (D20, round FW-CONTENT: the factory banks and PCM
   in the image, the arrays firmware/gnomad-content/gn_content.h defines, by name) and the RAM left for
   sequencer/UI growth; each with its headroom against engine-swap-budgets.md's hard line, and each engine against
   its estimate (REVIEW over 125 %: reported, never a failure); the rest (the image less the core, the content and
   the measured floor of the size measurements: Felucca's kept engines, the UI, Easy, the skin, the
   preset tables) is printed, held by max:image. The hard lines are the line's max:<figure> keys.
6. The content's own line (FORK.md "gn-budget CONTENT pitched N kits N pcm N max:content N"): the vendored files'
   sizes (firmware/gnomad-content) are the line's and their sum is within its cap (tests/gn_budget_test.py checks
   it on every run); with a build, the linked content arrays are that sum."""
import gzip
import os
import re
import sys
from pathlib import Path

XIP_ORG, RAM_ORG = 0x02000120, 0x01C08000
RAM_LIMIT, POOL_LIMIT, NOINIT_LEN = 96 * 1024, 0x54000, 0x3D50
LABEL = re.compile(r"^([A-Za-z_][A-Za-z_0-9.$]*):$")
INSN = re.compile(r"^\s*([0-9a-f]+):\s+((?:[0-9a-f]{2} )+)\s*(.*)$")
ROOTS = ["fm1_cstart", "fm1_alnk0_irq", "fm1_timer5_irq", "ui_draw", "ui_input", "ed_service",
         "gn_draw", "gn_input", "gn_key", "gn_editor", "gn_leds"]
INDIRECT = re.compile(r"^(scene_|parts_)|(_render|_block)$")


def num(s):
    return int(s, 16) if s.lower().startswith(("0x", "-0x")) else int(s)


def symbols(build):
    """objdump -t: address, flags, section, size, name (split on blanks: the flags column may be empty
    or several letters apart)"""
    out = []
    for line in (build / "felucca.syms").read_text().splitlines():
        t = line.split()
        if len(t) < 4 or not re.fullmatch(r"[0-9a-f]{8,16}", t[0]) or not re.fullmatch(r"[0-9a-f]{8,16}", t[-2]):
            continue
        out.append((int(t[0], 16), " ".join(t[1:-3]), t[-3], int(t[-2], 16), t[-1]))
    return out


def sym(syms, name, build=None):
    for a, _, _, _, n in syms:
        if n == name:
            return a
    if build:
        lines = [ln for ln in (build / "felucca.syms").read_text().splitlines() if name in ln or "_bss_end" in ln]
        print(f"gn_budget: no symbol {name} in {build}/felucca.syms; lines like it: {lines[:4]}")
    raise SystemExit(f"gn_budget: no symbol {name}")


def sizes(build):
    s = symbols(build)
    if not s:
        raise SystemExit(f"gn_budget: no symbols read from {build}/felucca.syms: "
                         f"{(build / 'felucca.syms').read_text().splitlines()[:6]}")
    binf = build / "felucca.bin"
    d = {"image": len(binf.read_bytes()) if binf.exists() else int((build / "felucca.bin.size").read_text()),
         ".text": sym(s, "_rt_load", build) - XIP_ORG,
         ".ram_text": sym(s, "_rt_end", build) - sym(s, "_rt_start", build),
         ".data": sym(s, "_data_end", build) - sym(s, "_data_start", build),
         ".bss": sym(s, "_bss_end", build) - sym(s, "_bss_start", build),
         "pool": sym(s, "_pool_end", build) - sym(s, "_pool_start", build)}
    d[".data+.bss"] = sym(s, "_bss_end", build) - RAM_ORG
    noinit = 0
    for a, _, sect, n, _ in s:
        if sect.startswith(".noinit") and n:
            noinit = max(noinit, a + n - 0x01C7C000)
    d[".noinit"] = noinit
    return d, s


def defined_names(files):
    names = set()
    for f in files:
        text = re.sub(r"/\*.*?\*/", " ", f.read_text(), flags=re.S)
        for m in re.finditer(r"^[A-Za-z_][\w \t\*]*?\b([A-Za-z_]\w*)\s*\([^;{]*\)\s*\{", text, re.M):
            names.add(m.group(1))
        for m in re.finditer(r"^\s*static\s+(?:const\s+)?(?:struct\s*\{|[\w \t\*]+?\b([A-Za-z_]\w*)\s*[\[=;])", text, re.M):
            if m.group(1):
                names.add(m.group(1))
        for m in re.finditer(r"^\}\s*([A-Za-z_]\w*)\s*(?:\[[^\]]*\])*\s*[=;]", text, re.M):
            names.add(m.group(1))
    return names - {"static", "const", "uint8_t", "uint16_t", "uint32_t", "int"}


CORE = Path("firmware/gnomad-core")
FLOOR_IMAGE, FLOOR_RAM = 287108, 69396      # size-measurements.md, CI run 37685583682: every Felucca engine and the samples out
CORE_ARENA, CORE_POOL = "gnc_state", "gnc_fx"   # firmware/src/gn_core.c: the core's state arena, its effect pool
HARD = {"core:image": 81920, "core:ram": 7168, "content": 196152, "ram:rest": 17644}   # engine-swap-budgets.md (D20)
SOFT = {"VA": 8192, "FM": 20480, "Drums": 12288, "Gnome Voice": 6144, "Sample": 10240, "Shared DSP/FX": 24576}
CONTENT_DIR = Path("firmware/gnomad-content")
CONTENT_ARRAYS = {"GN_CONTENT_PITCHED": "pitched", "GN_CONTENT_KITS": "kits", "GN_CONTENT_PCM": "pcm"}   # gn_content.h
CORE_FILE = {"va.c": "VA", "fm.c": "FM", "drums.c": "Drums", "gnome_voice.c": "Gnome Voice", "sample.c": "Sample"}
CORE_BLOCK = {"VA": "VA", "FM": "FM", "DRUMS": "Drums", "GNOME": "Gnome Voice", "SAMPLE": "Sample"}


def core_names(src):
    """name -> the engine (or Shared DSP/FX) of every identifier the vendored core defines, less any name the
    firmware's own sources define too; {} for a tree without the core"""
    if not (CORE / "src").is_dir():
        return {}
    out = {}
    for f in sorted((CORE / "src").glob("*.c")) + sorted(p for p in (CORE / "src").glob("*.h") if p.name != "tables.h"):
        for n in defined_names([f]):
            out[n] = CORE_FILE.get(f.name, "Shared DSP/FX")
    tables = CORE / "src" / "tables.h"
    if tables.exists():
        block = None
        for line in tables.read_text().splitlines():
            m = re.match(r"#ifdef GC_TABLES_DEFINE_(\w+)", line)
            if m:
                block = m.group(1)
                continue
            m = re.match(r"static const [\w ]+? (\w+)\[", line)
            if m and m.group(1) not in out:
                out[m.group(1)] = CORE_BLOCK.get(block, "Shared DSP/FX")
    theirs = defined_names(sorted(src.glob("*.c")))
    return {n: e for n, e in out.items() if n not in theirs}


def core_share(syms, names):
    """the core's figures: image (code and constants in .text), RAM (its .data/.bss and the arena), the pool"""
    d = {"code": 0, "constants": 0, "ram": 0, "pool": 0}
    per = {k: 0 for k in SOFT}
    for _, flags, sect, n, name in syms:
        base = name.split(".")[0]
        if not n:
            continue
        if base == CORE_ARENA:
            d["ram"] += n
        elif base == CORE_POOL:
            d["pool"] += n
        elif base in names:
            if sect == ".text":
                d["code" if "F" in flags else "constants"] += n
                per[names[base]] += n
            elif sect.startswith((".data", ".bss", ".sdata", ".sbss")):
                d["ram"] += n
    return d, per


def line_max(key, default):
    """the line's own max:<figure> (FORK.md's gn-budget line GN_BUDGET_LINE / GN_MILESTONE names), for the printed
    headroom (round MF: the floor is the line's, never a number here); default without a line or the key"""
    milestone = os.environ.get("GN_BUDGET_LINE") or os.environ.get("GN_MILESTONE")
    try:
        allow = allowances(milestone)[0] if milestone else None
    except OSError:
        allow = None
    return int(allow[key].replace(",", "")) if allow and key in allow else default


def engine_figures(so, syms, src):
    """the hard lines' figures into so (core:image, core:ram, content, ram:rest), printed with their headroom"""
    names = core_names(src)
    d, per = core_share(syms, names)
    so["core:image"] = d["code"] + d["constants"]
    so["core:ram"] = d["ram"]
    linked = content_linked(syms)
    so["content"] = sum(linked.values())
    so["rest"] = so["image"] - so["core:image"] - so["content"] - FLOOR_IMAGE
    so["ram:rest"] = so[".data+.bss"] - so["core:ram"] - FLOOR_RAM
    print(f"engine the Gnomad core ({len(names)} names from {CORE}/src): image {so['core:image']} B (code {d['code']}, "
          f"constants {d['constants']}), RAM {d['ram']} B ({CORE_ARENA} the arena), pool {d['pool']} B ({CORE_POOL})")
    print("engine the content linked: " + (", ".join(f"{k} {v} B" for k, v in linked.items()) or "none") +
          f"; the rest (image - core - content - floor {FLOOR_IMAGE}): {so['rest']} B, held by max:image")
    lines = (("total image", so["image"], line_max("max:image", 581564 - 16384)),
             (".data+.bss", so[".data+.bss"], line_max("max:.data+.bss", RAM_LIMIT - 4096)),
             ("engine core image", so["core:image"], HARD["core:image"]), ("engine core RAM", so["core:ram"], HARD["core:ram"]),
             ("content (the banks and PCM linked, gn_content.h)", so["content"], HARD["content"]),
             (f"RAM for sequencer/UI growth (.data+.bss - core - floor {FLOOR_RAM})", so["ram:rest"], HARD["ram:rest"]))
    for what, v, cap in lines:
        print(f"engine {what:<62} {v:>7} B, at most {cap:>7} B: {cap - v:+d} B headroom")
    for e, est in SOFT.items():
        v = per[e]
        flag = "REVIEW" if v > est * 5 // 4 else "ok    "
        print(f"engine {flag} {e:<14} {v:>6} B of code and constants, estimate {est} B (review over {est * 5 // 4} B)")
    return so


def content_linked(syms):
    """the content arrays in the image (.text), by name: {pitched, kits, pcm: bytes}"""
    out = {}
    for _, _, sect, n, name in syms:
        k = CONTENT_ARRAYS.get(name.split(".")[0])
        if k and n and sect == ".text":
            out[k] = out.get(k, 0) + n
    return out


def content_sizes(folder=CONTENT_DIR):
    """the vendored content's sizes: {pitched, kits, pcm (the assets factory.manifest lists)}"""
    pcm = 0
    for line in (folder / "factory.manifest").read_text().splitlines():
        t = line.split()
        if t and t[0].isdigit():
            pcm += (folder / f"{t[1]}.pcm").stat().st_size
    return {"pitched": (folder / "pitched.gcpbank").stat().st_size, "kits": (folder / "kits.gckbank").stat().st_size,
            "pcm": pcm}


CONTENT_KEYS = ("pitched", "kits", "pcm", "max:content")


def content_line(text=None):
    """FORK.md's content line: "gn-budget CONTENT pitched N kits N pcm N max:content N", each key once, plain numbers
    (or grouped by commas). -> (dict of ints, None), or (None, why)"""
    text = Path("FORK.md").read_text() if text is None else text
    lines = [ln.split() for ln in text.splitlines() if ln.split()[:2] == ["gn-budget", "CONTENT"]]
    if len(lines) != 1:
        return None, f"{len(lines)} lines \"gn-budget CONTENT ...\" (one wanted)"
    t = lines[0][2:]
    if len(t) % 2:
        return None, "the CONTENT line has a key without a value"
    out = {}
    for k, v in zip(t[0::2], t[1::2]):
        if k not in CONTENT_KEYS:
            return None, f"{k}: not a figure of the CONTENT line"
        if k in out:
            return None, f"{k} twice"
        if not re.fullmatch(r"(?:\d+|\d{1,3}(?:,\d{3})+)", v):
            return None, f"{k} {v}: takes a plain number"
        out[k] = int(v.replace(",", ""))
    missing = [k for k in CONTENT_KEYS if k not in out]
    if missing:
        return None, "the CONTENT line lacks " + ", ".join(missing)
    return out, None


def content_check(have, text=None, linked=None):
    """1 if the vendored sizes are not the line's, their sum is over its cap, or the linked arrays differ from them"""
    line, why = content_line(text)
    if line is None:
        print(f"FAIL budget CONTENT: FORK.md's line: {why}")
        return 1
    bad = 0
    for k in ("pitched", "kits", "pcm"):
        ok = have.get(k) == line[k]
        print(f"{'ok  ' if ok else 'FAIL'} budget CONTENT: {k} {have.get(k)} B vendored, the line says {line[k]} B")
        bad |= not ok
    total = sum(have.get(k, 0) for k in ("pitched", "kits", "pcm"))
    ok = total <= line["max:content"]
    print(f"{'ok  ' if ok else 'FAIL'} budget CONTENT: {total} B, at most {line['max:content']} B "
          f"({line['max:content'] - total:+d} B headroom)")
    bad |= not ok
    if linked is not None:
        ok = linked == {k: have[k] for k in ("pitched", "kits", "pcm")}
        print(f"{'ok  ' if ok else 'FAIL'} budget CONTENT: linked {linked}, vendored the same")
        bad |= not ok
    return 1 if bad else 0


def fork_names(src):
    """identifiers defined at any level in firmware/src/gn_*.c (functions, objects, static locals), less
    any name upstream's sources define too (a symbol of that name could be either's)"""
    ours = defined_names(sorted(src.glob("gn_*.c")))
    theirs = defined_names(sorted(f for f in src.glob("*.c") if not f.name.startswith("gn_")))
    return ours - theirs


def fork_share(syms, names):
    share = {}
    for _, flags, sect, n, name in syms:
        base = name.split(".")[0]
        if not n or (base not in names and name not in names):
            continue
        kind = sect
        if sect == ".text":
            kind = "code" if "F" in flags else "constants"
        share[kind] = share.get(kind, 0) + n
    return share


def functions(build):
    funcs, cur = {}, None
    dis = build / "felucca.dis"
    text = dis.read_text(errors="replace") if dis.exists() else \
        gzip.decompress((build / "felucca.dis.gz").read_bytes()).decode(errors="replace")
    for line in text.splitlines():
        m = LABEL.match(line.strip())
        if m:
            if not m.group(1).startswith("."):
                cur = funcs.setdefault(m.group(1), [])
            continue
        m = INSN.match(line)
        if cur is not None and m and not m.group(3).lstrip().startswith("<"):
            cur.append(m.group(3).strip())
    return funcs


def frame(insns):
    """bytes the prologue takes: pushes ([--sp] = {..} or a register) and sp moves, up to the first
    branch or call of the function"""
    n = 0
    for t in insns[:24]:
        if re.search(r"\b(call|goto|if|rts|rti)\b", t):
            break
        m = re.search(r"\[--sp\]\s*=\s*\{([^}]*)\}", t)
        if m:
            for r in m.group(1).split(","):
                r = r.strip()
                mm = re.match(r"[a-z]+(\d+)\s*-\s*[a-z]+(\d+)$", r)
                n += 4 * (abs(int(mm.group(2)) - int(mm.group(1))) + 1) if mm else 4
            continue
        if re.search(r"\[--sp\]\s*=", t):
            n += 4
            continue
        m = re.search(r"\bsp\s*\+=\s*(-?(?:0x[0-9a-f]+|\d+))\b", t)
        if m and num(m.group(1)) < 0:
            n -= num(m.group(1))
            continue
        m = re.search(r"\bsp\s*-=\s*(0x[0-9a-f]+|\d+)\b", t) or re.search(r"\bsp\s*=\s*sp\s*-\s*(0x[0-9a-f]+|\d+)\b", t)
        if m:
            n += num(m.group(1))
            continue
        m = re.search(r"\bsp\s*=\s*sp\s*\+\s*(-(?:0x[0-9a-f]+|\d+))\b", t)
        if m:
            n -= num(m.group(1))
    return n


def stack(build, names):
    """the deepest call path from each root over direct calls (and tail jumps); an indirect call counts
    only where Easy's draw dispatches its scenes (a function of the fork's, or one that calls Easy's
    drawing, gs_* or gx_*, and calls through a pointer: gn_draw's SCENE[] and PARTS[], inlined into
    ui_draw), as the deepest
    of scene_* and parts_*; Felucca's own indirect calls (the engines' renders, callbacks) are not
    followed, in either build: a lower bound, the same for both"""
    funcs = functions(build)
    fr = {f: frame(i) for f, i in funcs.items()}
    calls, indirect = {}, {}
    for f, insns in funcs.items():
        c, ind = set(), 0
        for t in insns:
            m = re.search(r"\bcall\s+-?\d+\s+<([^:>+ ]+)", t)
            if m:
                c.add(m.group(1))
            elif re.search(r"\bcall\b", t):
                ind += 1
            m = re.search(r"\bgoto\s+-?\d+\s+<([^:>+ ]+)", t)
            if m and m.group(1) != f and m.group(1) in funcs:
                c.add(m.group(1))
        calls[f], indirect[f] = c, ind
    fork = {f for f in funcs if f.split(".")[0] in names}
    scenes = sorted(f for f in fork if re.match(r"(scene|parts)_", f))
    draw = {f for f in fork if re.match(r"(gs|gx)_", f)}    # (Easy's drawing: what gn_draw calls)
    memo = {}

    def depth(f, onpath):
        if f in memo:
            return memo[f]
        if f in onpath or f not in funcs:
            return (0, [])
        onpath = onpath | {f}
        best = (0, [])
        targets = set(calls[f])
        if indirect[f] and (f in fork or calls[f] & draw):
            targets |= set(scenes)
        for c in sorted(targets):
            d = depth(c, onpath)
            if d[0] > best[0]:
                best = d
        memo[f] = (fr[f] + best[0], [f] + best[1])
        return memo[f]
    roots = {r: depth(r, frozenset()) for r in ROOTS if r in funcs}
    for r in ("ui_draw", "ui_input", "ed_service"):         # each hook's caller, through the fork only
        if r not in funcs or not fork:
            continue
        via = {c for c in calls[r] if c in fork}
        if indirect[r] and calls[r] & draw:
            via |= set(scenes)
        if via:
            d = max((depth(c, frozenset({r})) for c in sorted(via)), key=lambda x: x[0])
            roots[f"{r} via Easy"] = (fr[r] + d[0], [r] + d[1])
    if fork:
        top = max(fork, key=lambda f: depth(f, frozenset())[0])
        roots["the fork's deepest"] = depth(top, frozenset())
    known = sum(1 for v in fr.values() if v)
    sample = {r: funcs[r][:6] for r in ("fm1_alnk0_irq", "ui_draw") if r in funcs}
    return roots, known, len(funcs), sample, {f: fr[f] for f in ("ui_draw", "ui_input", "gn_key") if f in fr}


REQUIRED = ("image", ".data", ".bss", ".data+.bss", "pool", ".noinit", ".ram_text", "free:image",
            "free:.data+.bss", "stack:fm1_cstart", "stack:fm1_alnk0_irq", "stack:fm1_timer5_irq")
# optional absolute ceilings (the Gnomad engines' hard lines): max:<figure> N, the most the figure may be
MAXIMA = ("max:image", "max:.data+.bss", "max:core:image", "max:core:ram", "max:content", "max:ram:rest")


def allowances(milestone, text=None):
    """FORK.md's allowance line for the milestone: "gn-budget <MILESTONE> key value ...". A value +N is
    the most the figure may grow against the base, a plain N the least that must stay free (the keys
    free:<figure>); stack:<root> +N the most a root's static depth may grow. A number is plain decimal
    digits, or thousands grouped by commas (40,960). -> (dict, None), or (None, why) for no line, two
    lines, an empty or incomplete one (every REQUIRED key, each once), a key not known, a value not of
    its key's form"""
    text = Path("FORK.md").read_text() if text is None else text
    lines = [ln.split() for ln in text.splitlines() if ln.split()[:2] == ["gn-budget", milestone]]
    if not lines:
        return None, f"no line \"gn-budget {milestone} ...\""
    if len(lines) > 1:
        return None, f"{len(lines)} lines for {milestone}"
    t = lines[0][2:]
    if not t:
        return None, f"the line for {milestone} states nothing"
    if len(t) % 2:
        return None, f"the line for {milestone} has a key without a value"
    allow = {}
    for k, v in zip(t[0::2], t[1::2]):
        known = k in REQUIRED or k.startswith("stack:") and len(k) > 6 or k in MAXIMA
        if not known:
            return None, f"{k}: not a figure the budget measures"
        if k in allow:
            return None, f"{k} twice"
        digits = r"(?:\d+|\d{1,3}(?:,\d{3})+)"        # plain digits, or thousands grouped by commas
        plain = k.startswith(("free:", "max:"))
        form = digits if plain else r"[+-]" + digits
        if not re.fullmatch(form, v):
            return None, f"{k} {v}: " + (f"{k.split(':')[0]}:<figure> takes a plain number" if plain else "takes +N")
        allow[k] = v
    missing = [k for k in REQUIRED if k not in allow]
    if missing:
        return None, f"the line for {milestone} lacks " + ", ".join(missing)
    return allow, None


def check(milestone, so, sb, limit, stack_ours, stack_base, text=None):
    """1 if a figure is over its allowance, or FORK.md's line for the milestone is missing or not whole"""
    allow, why = allowances(milestone, text)
    if allow is None:
        print(f"FAIL budget: FORK.md's allowance for {milestone}: {why}")
        return 1
    bad = 0
    for k, v in allow.items():
        n = int(v.replace(",", ""))
        if k.startswith("max:"):
            f = k[4:]
            got = so.get(f)
            ok = got is not None and got <= n
            what = f"{f} {got} B, at most {n} B" if got is not None else f"{f}: no figure"
        elif k.startswith("free:"):
            f = k[5:]
            got = limit[f] - so[f] if f in limit and f in so else None
            ok = got is not None and got >= n
            what = f"{f} free {got} B, at least {n} B"
        elif k.startswith("stack:"):
            r = k[6:]
            got = stack_ours[r][0] - stack_base[r][0] if r in stack_ours and r in stack_base else None
            ok = got is not None and got <= n
            what = f"stack {r} {got:+d} B against the base, at most {n:+d} B" if got is not None else f"stack {r}: not in both builds"
        else:
            got = so[k] - sb[k] if k in so and k in sb else None
            ok = got is not None and got <= n
            what = f"{k} {got:+d} B against the base, at most {n:+d} B" if got is not None else f"{k}: no figure"
        print(f"{'ok  ' if ok else 'FAIL'} budget {milestone}: {what}")
        bad |= not ok
    return 1 if bad else 0


def main():
    ours, base = Path(sys.argv[1]), Path(sys.argv[2]) if len(sys.argv) > 2 else None
    so, syms = sizes(ours)
    sb = sizes(base)[0] if base else {}
    limit = {"image": 581564, ".data+.bss": RAM_LIMIT, "pool": POOL_LIMIT, ".noinit": NOINIT_LEN}
    for k, v in so.items():
        line = f"size   {k:<11} {v:>7} B"
        if k in sb:
            line += f"   base {sb[k]:>7} B   delta {v - sb[k]:+d} B"
        if k in limit:
            line += f"   of {limit[k]} ({limit[k] - v} free)"
        print(line)
    engine_figures(so, syms, Path("firmware/src"))             # (the core's names: this checkout's)
    share = fork_share(syms, fork_names(Path("firmware/src")))   # (the fork's names: this checkout's)
    print("fork   the symbols of firmware/src/gn_*.c: " +
          ", ".join(f"{k} {v} B" for k, v in sorted(share.items())) +
          " (a static function inlined into its caller is counted there instead)")
    names = fork_names(Path("firmware/src"))
    depths = {}
    for build, label in ((ours, "ours"), (base, "base")):
        if not build:
            continue
        roots, known, total, sample, frames = stack(build, names)
        depths[label] = roots
        print(f"stack  {label}: frames read off {known} of {total} functions' prologues; own frames " +
              ", ".join(f"{f} {n} B" for f, n in frames.items()))
        for r, (d, path) in roots.items():
            print(f"stack  {label}: {r:<18} {d:>6} B deepest  " + " > ".join(path[:14]) + (" .." if len(path) > 14 else ""))
        for r, ins in sample.items():
            print(f"stack  {label}: {r} begins: " + " | ".join(ins))
    content = 0
    if CONTENT_DIR.is_dir():                        # (round FW-CONTENT: the content's own line, and what was linked)
        content = content_check(content_sizes(), linked=content_linked(syms))
    milestone = os.environ.get("GN_BUDGET_LINE") or os.environ.get("GN_MILESTONE")
    if milestone and base:                          # (CI: the allowances FORK.md states, a failure fails the run)
        return check(milestone, so, sb, limit, depths["ours"], depths["base"]) | content
    return content


if __name__ == "__main__":
    sys.exit(main())
