#!/usr/bin/env python3
# Gnomad, a fork of Felucca (see FORK.md). Not part of upstream Felucca.
"""The package to install: this branch's app with the update loader of upstream's released package.

  released_package.py RELEASED.fwsc SHA256 SIZE LOADER_SHA256 OUT.fwsc OUT_LOADER.bin

Installing a package runs that package's own update loader: the firmware on the synth stages the
new package's ota.bin (firmware/src/ota.c ota_stage) and that loader writes the app. So the package
to install carries upstream's released loader, never one built here.

RELEASED (the workflow downloads it from upstream's release) must have SIZE bytes and SHA256, the
values on upstream's release page. Read the way the device reads it (tools/gn_package_check.py) it
must pass every check, and the update loader the device would select from it must have
LOADER_SHA256, pinned in the workflow (when that is empty, the loader's SHA-256 is printed and the
step fails, so it can be pinned). That loader goes to OUT_LOADER and into OUT with build/felucca.bin,
packed exactly as tools/build.py packs the normal package:
fm1pkg_make.ufw(fm1pkg_make.flash_image(app, KEY), ota, identity), with the normal package's
identity. OUT is read back the same way: its app area must equal the normal package's, its loader
the pinned one. Run from the repo root after ./build.sh (needs AC79_SDK, as the build)."""
import hashlib
import sys
from pathlib import Path

ROOT = Path.cwd()
sys.path.insert(0, str(ROOT / "tools"))
import fm1_install as I  # noqa: E402
import fm1pkg_make as P  # noqa: E402
import gn_package_check as C  # noqa: E402


def sha(b):
    return hashlib.sha256(b).hexdigest()


def main(argv):
    released, want_pkg, want_size, want_ldr, out, out_ldr = argv
    raw = Path(released).read_bytes()
    print(f"released package: {released}: {len(raw)} B, SHA-256 {sha(raw)}, identity {I.product_of(raw)}")
    if len(raw) != int(want_size) or sha(raw) != want_pkg.lower():
        raise SystemExit(f"released_package: FAIL {released} is not the published package "
                         f"({want_size} B, SHA-256 {want_pkg})")
    problems, sel = C.check(raw)
    for p in problems:
        print(f"released package: FAIL {p}")
    if problems:
        raise SystemExit(1)
    ota = sel["ota"]
    print(f"released loader: {len(ota)} B, SHA-256 {sha(ota)} (the ota.bin the device selects from it)")
    if not want_ldr:
        raise SystemExit(f"released_package: FAIL the loader's SHA-256 is not pinned: set RELEASED_LOADER_SHA256 "
                         f"to {sha(ota)} in .github/workflows/firmware.yml")
    if sha(ota) != want_ldr.lower():
        raise SystemExit(f"released_package: FAIL the released loader has SHA-256 {sha(ota)}, pinned {want_ldr}")
    Path(out_ldr).write_bytes(ota)
    normal = (ROOT / "build" / "felucca.fwsc").read_bytes()
    nprob, nsel = C.check(normal)
    for p in nprob:
        print(f"normal package: FAIL {p}")
    if nprob:
        raise SystemExit(1)
    identity = I.product_of(normal)
    app = (ROOT / "build" / "felucca.bin").read_bytes()
    pkg = P.ufw(P.flash_image(app, P.KEY), ota, identity)          # as tools/build.py packs felucca.fwsc
    Path(out).write_bytes(pkg)
    problems, _ = C.check(Path(out).read_bytes(), nsel["flash"], want_ldr)
    if I.product_of(pkg) != identity:
        problems.append(f"its identity is {I.product_of(pkg)!r}, not {identity!r}")
    for p in problems:
        print(f"package to install: FAIL {p}")
    if problems:
        raise SystemExit(1)
    print(f"package to install: {out}: {len(pkg)} B, SHA-256 {sha(pkg)}, identity {identity}; read as the device "
          f"reads it: its app area is build/felucca.fwsc's, its loader the released one ({want_ldr})")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
