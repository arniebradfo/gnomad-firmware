#!/usr/bin/env python3
# Gnomad, a fork of Felucca (see FORK.md). Not part of upstream Felucca.
"""The build's options as the update path needs them, and the figures the linker reports.

  build_info.py        (repo root, after ./build.sh and objdump -t build/felucca.elf > build/felucca.syms)

Fails unless the options an install depends on are what the update needs: FELUCCA_OTA 1 (the update
entry), FELUCCA_OTA_DRYRUN 0 (an update commits: a dry run stages, asks "success" and undoes),
FELUCCA_OTA_RAMONLY 0 (the update record also goes to flash, so an interrupted install resumes at the next
power-on), FELUCCA_FLASH 1, the identity FM-1_900, and the name <GN_NAME> <milestone> <commit> (GN_VERSION,
which the workflow's Names step makes). The values are the ones the image was compiled with: no FELUCCA_* variable may be set (build.py
passes only those, so it passed none), and JieLi's clang preprocesses firmware/src/felucca.c with build.py's
include paths and FELUCCA_ID, printing the macros the one compilation unit ends with (FELUCCA_OTA_RAMONLY
comes from ota.c, the name from build/gen/gn_name.h). tests/ota_test.c and tests/ldr_test.c must define the
same dry-run value, so the update tests run the configuration the image has.

Also the Gnomad engine swap as compiled (GN_ENGINES 1 and the six engines it retires: firmware/src/gn_engines.h);
`build_info.py --engines` prints and checks only that (the Build step runs it right after build.sh, link or not);
`--engines-off` the engines-off build's (GN_ENGINES 0, nothing dropped: upstream's tests run against it).
Then the image size against the app slot, RAM .data+.bss against its 96 KiB, the pool against its region
and the 8 KiB build.py keeps spare, .noinit against its region, from the linker's symbols."""
import os
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path.cwd()
sys.path.insert(0, str(ROOT / "tools"))
import fm1_install as I  # noqa: E402
import fm1pkg_make as P  # noqa: E402

PRODUCT = "FM-1_900"                                               # tools/build.py: identity of non-release builds
RAM_ORG, RAM_LIMIT = 0x01C08000, 96 * 1024                         # firmware/app.ld
POOL_LIMIT, POOL_SPARE = 0x54000, 8192                             # app.ld, build.py check()
NOINIT_ORG, NOINIT_LEN = 0x01C7C000, 0x3D50                        # app.ld


def engines(macros, fail, off=False):
    """Gnomad engines (FORK.md): the shipping configuration as compiled (JieLi's preprocessor on felucca.c): the swap
    on, the six engines it retires out; the sample block and the kept engines as they are; printed on every run"""
    need = {"GN_ENGINES": "1", "GN_DROP_PHYS": "1", "GN_DROP_GRAIN": "1", "GN_DROP_WHEEL": "1", "GN_DROP_FM6": "1",
            "GN_DROP_SLICE": "1", "GN_DROP_DRUM": "1"}
    have = dict(re.findall(r"^#define (GN_ENGINES|GN_DROP_\w+|GN_CORE_LIBCALLS) (.*)$", macros, re.M))
    if off:                                      # the engines-off build (upstream's tests): no swap, nothing dropped
        need = {k: "0" for k in have if k.startswith(("GN_ENGINES", "GN_DROP_"))}
        need["GN_ENGINES"] = "0"
    for k, v in need.items():
        if have.get(k) != v:
            fail.append(f"{k} is {have.get(k)}: the {'engines-off build' if off else 'shipping configuration'} needs {v} "
                        "(firmware/src/gn_engines.h, build/gen/gn_engines_config.h)")
    print("engine " + " ".join(f"{k.lower()}={v}" for k, v in sorted(have.items())))
    # the build's own configuration, for the gates that read the image (tests/gn_target_budget.py)
    Path("build").mkdir(exist_ok=True)
    (Path("build") / "gn_engines.txt").write_text("".join(f"{k} {v}\n" for k, v in sorted(have.items())))


def preprocess():
    clang = Path(os.environ.get("JIELI_TOOLCHAIN", "/nonexistent")) / "pi32v2" / "bin" / "clang"
    return subprocess.run([str(clang), "-target", "pi32v2", "-Ifirmware/hal", "-Ifirmware/src", "-Ibuild/gen",
                           f'-DFELUCCA_ID="{PRODUCT}"', "-dM", "-E", "firmware/src/felucca.c"],
                          capture_output=True, text=True)


def main():
    fail = []
    env = sorted(k for k in os.environ if k.startswith("FELUCCA_"))
    if env:
        fail.append(f"build options set in the environment: {', '.join(env)}")
    version = os.environ.get("GN_VERSION", "")
    if not version:
        fail.append("no GN_VERSION (the workflow's Names step)")
    need = {"FELUCCA_OTA": "1", "FELUCCA_OTA_DRYRUN": "0", "FELUCCA_OTA_RAMONLY": "0", "FELUCCA_FLASH": "1",
            "FELUCCA_ID": f'"{PRODUCT}"', "FELUCCA_VERSION": f'"{version}"'}
    clang = Path(os.environ.get("JIELI_TOOLCHAIN", "/nonexistent")) / "pi32v2" / "bin" / "clang"
    r = subprocess.run([str(clang), "-target", "pi32v2", "-Ifirmware/hal", "-Ifirmware/src", "-Ibuild/gen",
                        f'-DFELUCCA_ID="{PRODUCT}"', "-dM", "-E", "firmware/src/felucca.c"],
                       capture_output=True, text=True)
    if r.returncode:
        fail.append(f"the preprocessor failed: {r.stderr.strip()[:300]}")
    have = dict(re.findall(r"^#define (FELUCCA_\w+) (.*)$", r.stdout, re.M))
    for k, v in need.items():
        if have.get(k) != v:
            fail.append(f"{k} is {have.get(k)}, the update needs {v}")
    engines(r.stdout, fail)
    for t in ("tests/ota_test.c", "tests/ldr_test.c"):
        m = re.search(r"#define\s+FELUCCA_OTA_DRYRUN\s+(\S+)", (ROOT / t).read_text())
        if not m or m.group(1) != need["FELUCCA_OTA_DRYRUN"]:
            fail.append(f"{t} does not test FELUCCA_OTA_DRYRUN {need['FELUCCA_OTA_DRYRUN']}, the image's value")
    product = I.product_of((ROOT / "build/felucca.fwsc").read_bytes())
    if product != PRODUCT:
        fail.append(f"the package identity is {product}, not {PRODUCT}")
    print("config " + " ".join(f"{k[8:].lower()}={have.get(k)}" for k in need) + f" package={product}")
    others = {k: v for k, v in sorted(have.items()) if k not in need}
    print("other  " + " ".join(f"{k[8:].lower()}={v}" for k, v in others.items()))

    syms = (ROOT / "build/felucca.syms").read_text()

    def sym(name):
        m = re.search(r"^([0-9a-f]+) .*\s" + re.escape(name) + r"$", syms, re.M)
        if not m:
            raise SystemExit(f"build_info: no symbol {name} in build/felucca.syms")
        return int(m.group(1), 16)
    img = len((ROOT / "build/felucca.bin").read_bytes())
    bss = sym("_bss_end") - RAM_ORG
    pool = sym("_pool_end") - sym("_pool_start")
    rt = sym("_rt_end") - sym("_rt_start")
    noinit_end, objs = NOINIT_ORG, []
    for line in syms.splitlines():
        m = re.match(r"^([0-9a-f]+)\s.*\s(\.noinit\S*)\s+([0-9a-f]+)\s+(\S+)\s*$", line)
        if m and int(m.group(3), 16):
            a, n = int(m.group(1), 16), int(m.group(3), 16)
            objs.append((a, n, m.group(4)))
            noinit_end = max(noinit_end, a + n)
    noinit = noinit_end - NOINIT_ORG
    print(f"image  {img} B of the {P.APP_SLOT} B app slot: {P.APP_SLOT - img} B free")
    data, bss_only = sym("_data_end") - sym("_data_start"), sym("_bss_end") - sym("_bss_start")
    print(f"ram    .data+.bss {bss} B of {RAM_LIMIT}: {RAM_LIMIT - bss} B free (.data {data} B, its initial "
          f"values in the image too; .bss {bss_only} B)")
    print(f"pool   {pool} B of {POOL_LIMIT}: {POOL_LIMIT - pool} B spare, {POOL_LIMIT - pool - POOL_SPARE} B usable "
          f"(build.py keeps {POOL_SPARE} B spare)")
    print(f"noinit {noinit} B of {NOINIT_LEN} (to the end of the last object): {NOINIT_LEN - noinit} B free; "
          + ", ".join(f"{name} {n} B at {a:#x}" for a, n, name in sorted(objs)))
    print(f"ramtxt .ram_text {rt} B of {0x6000}")
    for f in fail:
        print(f"FAIL {f}")
    return 1 if fail else 0


if __name__ == "__main__":
    if sys.argv[1:] in (["--engines"], ["--engines-off"]):   # (the Build steps, right after build.sh, link or not)
        fail = []
        r = preprocess()
        if r.returncode:
            fail.append(f"the preprocessor failed: {r.stderr.strip()[:300]}")
        engines(r.stdout, fail, off=sys.argv[1] == "--engines-off")
        for f in fail:
            print(f"FAIL {f}")
        sys.exit(1 if fail else 0)
    sys.exit(main())
