#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Kyle Palaniuk
"""Make the core's generated tables.h safe for a single translation unit.

The core's generator writes each table inside a per-file block (#ifdef GC_TABLES_DEFINE_<FILE>) under one
include guard, so a unity build that includes several core files sees only the first file's block. This
rewrites every `static const T name[...] = {...};` definition with its own guard (GC_TBL_<name>), so the
firmware can define every GC_TABLES_DEFINE_* once and get each table exactly once. A table defined in two
blocks must be byte-identical there; otherwise this fails. Everything else is copied unchanged.
usage: unity_tables.py IN OUT"""
import re, sys
src = open(sys.argv[1]).read()
rx = re.compile(r'^static const [A-Za-z0-9_ ]+? (\w+)\[[^\]]*\](?:\[[^\]]*\])* = \{.*?^\};\n', re.S | re.M)
seen, out, pos = {}, [], 0
for m in rx.finditer(src):
    name, body = m.group(1), m.group(0)
    if name in seen and seen[name] != body:
        sys.exit(f"unity_tables.py: table {name} is defined twice with different contents")
    seen[name] = body
    out.append(src[pos:m.start()])
    out.append(f"#ifndef GC_TBL_{name}\n#define GC_TBL_{name}\n{body}#endif\n")
    pos = m.end()
out.append(src[pos:])
res = "".join(out)
if not seen:
    sys.exit("unity_tables.py: no table definitions found (the generator's format changed)")
open(sys.argv[2], "w").write("/* firmware/gnomad-core/sync-core.sh: unity-safe copy of the core's generated tables.h */\n" + res)
print(f"unity_tables.py: {len(seen)} tables guarded")
