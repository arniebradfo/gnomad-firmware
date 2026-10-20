#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Kyle Palaniuk
"""The retired-engine rule of tests/gn_target_budget.py, through upstream's tests/target_budget.py on a made-up listing:
an engine the build's configuration (build/gn_engines.txt) compiles out is "retired" when absent and a failure when
present; a kept engine's missing function fails as before; with no configuration nothing is skipped."""
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
FN = "{n}:\n 2000000:    01 02             \tr0 = r1\n 2000002:    03 04             \tgoto -4 <{n} : 2000000 >\n"
fails = 0


def run(dis_funcs, cfg):
    with tempfile.TemporaryDirectory() as d:
        d = Path(d)
        (d / "felucca.dis").write_text("".join(FN.format(n=n) for n in dis_funcs))
        (d / "budget.txt").write_text("")
        if cfg is not None:
            (d / "build").mkdir()
            (d / "build" / "gn_engines.txt").write_text("".join(f"{k} {v}\n" for k, v in cfg.items()))
        r = subprocess.run([sys.executable, str(ROOT / "tests" / "target_budget.py"), "felucca.dis", "budget.txt"],
                           cwd=d, capture_output=True, text=True)
        return r.stdout


def report(what, ok):
    global fails
    print(f"gn target budget test: {what:<72} {'ok' if ok else 'FAIL'}")
    fails += not ok


out = run(["analog_render"], {"GN_ENGINES": "0", "GN_DROP_WHEEL": "1"})
report("a compiled-out engine (WHEEL) absent: retired, no failure", "wheel_render    retired" in out and
       "FAIL wheel_render" not in out and "FAIL wheel_block" not in out)
report("a kept engine (PHASE) absent: fails as upstream's check does", "FAIL phase_render not found" in out)
out = run(["wheel_render"], {"GN_ENGINES": "0", "GN_DROP_WHEEL": "1"})
report("a compiled-out engine still in the listing: fails", "FAIL wheel_render is compiled out" in out)
out = run([], None)
report("no configuration: nothing skipped (WHEEL absent fails)", "FAIL wheel_render not found" in out and "retired" not in out)
print("gn target budget tests " + ("FAILED" if fails else "passed"))
sys.exit(1 if fails else 0)
