#!/usr/bin/env python3
# Gnomad, a fork of Felucca (see FORK.md). Not part of upstream Felucca.
"""Job summary (Markdown on stdout) for the firmware workflow: the commit and the build's name, the files
uploaded (size, SHA-256), the build's options and sizes, our changes to firmware/ against the base tag,
where the toolchain and the SDK files came from, tool versions, upstream's host tests and ours, and the
package to install. Run from the repo root after the build (any later step may have failed)."""
import hashlib
import json
import os
import subprocess
from pathlib import Path

BUILD = Path("build")
TAG = os.environ.get("GN_TAG", "")
UP = os.environ.get("UPSTREAM_TAG", "")


def run(*cmd):
    try:
        r = subprocess.run(cmd, capture_output=True, text=True, timeout=30)
        return (r.stdout or r.stderr).strip().splitlines()[0]
    except (OSError, IndexError, subprocess.SubprocessError):
        return "not available"


def lines(path):
    try:
        return path.read_text(errors="replace").splitlines()
    except OSError:
        return []


def block(title, body, empty="not run"):
    return ["", f"### {title}", ""] + (["```text", *body, "```"] if body else [empty])


out = ["## Firmware build", ""]
built = os.environ.get("GN_COMMIT", "") or run("git", "rev-parse", "HEAD")
try:
    pr = json.loads(Path(os.environ["GITHUB_EVENT_PATH"]).read_text()).get("pull_request") or {}
except (KeyError, OSError, ValueError):
    pr = {}
if pr:
    out += [f"Pull request #{pr.get('number')}: built its head `{built}` itself (not a merge with "
            f"`{pr.get('base', {}).get('ref', '?')}`); every name below is that commit's.", ""]
else:
    out += [f"Commit `{built}` ({os.environ.get('GITHUB_EVENT_NAME', '?')} on "
            f"`{os.environ.get('GITHUB_REF_NAME', '?')}`)", ""]
out += [f"Base: upstream `{os.environ.get('UPSTREAM_REPO', '?')}` `{UP}` "
        f"(`{os.environ.get('UPSTREAM_COMMIT', '?')}`). The build's name (`INFO`): "
        f"`FELUCCA {os.environ.get('GN_VERSION', '?')}`.", ""]

files = [(f"{TAG}-released-loader.fwsc", "**the package to install**: this app with upstream's released update loader"),
         (f"{TAG}-app.bin", "the firmware app (`build/felucca.bin`)"),
         (f"{TAG}-upstream-{UP}-loader-ota.bin", f"upstream's {UP} update loader, as pinned (from the published package)"),
         (f"{TAG}-manifest.txt", "commit, base, name, options, the files' SHA-256")]
out += ["| Uploaded file | What | Size (bytes) | SHA-256 |", "| --- | --- | ---: | --- |"]
for name, what in files:
    p = BUILD / name
    if p.is_file():
        data = p.read_bytes()
        out.append(f"| `{name}` | {what} | {len(data)} | `{hashlib.sha256(data).hexdigest()}` |")
    else:
        out.append(f"| `{name}` | {what} | missing | |")
screens = BUILD / f"{TAG}-screens"
pngs = sorted(screens.rglob("*.png")) if screens.is_dir() else []
out += ["", f"Folders, zipped: `{TAG}-installer-site` (the installer page for the package to install), "
        f"`{TAG}-screens` ({len(pngs)} PNGs: upstream's renders of every screen, and the ABOUT pages with this "
        f"build's name), `{TAG}-generated-headers` (`build/gen`), `{TAG}-logs`. Not uploaded: `build/felucca.fwsc` "
        "and `build/loader/ota.bin`, whose update loader was built here.", ""]

info = lines(BUILD / "build-info.txt")
out += block("Build options and sizes (`.github/ci/build_info.py`, from the linker's symbols)", info)
diff = lines(BUILD / "base-diff.txt")
out += block(f"Our changes to `firmware/` against {UP} (FORK.md: the touch list)", diff, "none")

build_log = lines(BUILD / "build.log")
picked = [ln.strip() for ln in build_log if ln.startswith(("app ", "loader", "package", "  ok", "  FAIL", "warning"))]
out += block("Build output", picked)

out += ["", "### Toolchain, SDK files, tools", ""]
for title, prov in (("JieLi toolchain", Path.home() / ".jieli" / "PROVENANCE"),
                    ("AC79 SDK files", Path.home() / "fw-AC79_AIoT_SDK" / "PROVENANCE")):
    kv = dict(ln.split(" ", 1) for ln in lines(prov) if " " in ln)
    out.append(f"- **{title}:** " + (", ".join(f"{k} `{v}`" for k, v in kv.items()) or "no provenance file"))
tc = os.environ.get("JIELI_TOOLCHAIN", "")
out += [f"- JieLi clang: `{run(f'{tc}/pi32v2/bin/clang', '--version')}`",
        f"- Python `{run('python3', '--version')}`, Pillow `{run('python3', '-c', 'import PIL; print(PIL.__version__)')}`, "
        f"fontTools `{run('python3', '-c', 'import fontTools; print(fontTools.version)')}`",
        f"- host cc: `{run('cc', '--version')}`; node: `{run('node', '--version')}`"]

tests = lines(BUILD / "host-tests.log")
out += ["", "### Upstream's host tests (`tests/run_tests.sh`, unchanged)", ""]
if tests:
    verdict = next((ln for ln in reversed(tests) if "HOST TESTS" in ln), "no verdict (the run stopped early)")
    out += [f"**{verdict}**", ""] + [f"- {ln[3:]}" for ln in tests if ln.startswith("== ")]
    notes = [ln.strip() for ln in tests if "no instruction counter" in ln or "skip" in ln.lower()]
    fails = [ln.strip() for ln in tests if "FAIL" in ln and "HOST TESTS" not in ln]
    if notes:
        out += ["", "Not checked on this runner:", "", *[f"- {n}" for n in notes]]
    if fails:
        out += ["", "Failures:", "", "```text", *fails[:60], "```"]
else:
    out += ["not run"]

fork = lines(BUILD / "fork-tests.log")
out += ["", "### Our host tests (`tests/run_fork_tests.sh`)", ""]
if fork:
    verdict = next((ln for ln in reversed(fork) if "FORK TESTS" in ln), "no verdict (the run stopped early)")
    out += [f"**{verdict}**", ""] + [f"- {ln[3:]}" for ln in fork if ln.startswith("== ")]
    fails = [ln.strip() for ln in fork if "FAIL" in ln and "FORK TESTS" not in ln]
    if fails:
        out += ["", "Failures:", "", "```text", *fails[:60], "```"]
else:
    out += ["not run"]

skin = lines(BUILD / "skin-tests.log")
out += ["", "### The Advanced skin's tests (`tests/gn_skin_tests.sh`)", ""]
if skin:
    verdict = next((ln for ln in reversed(skin) if "SKIN TESTS" in ln), "no verdict (the run stopped early)")
    out += [f"**{verdict}**", ""] + [f"- {ln[3:]}" for ln in skin if ln.startswith("== ")]
    fails = [ln.strip() for ln in skin if "FAIL" in ln and "SKIN TESTS" not in ln]
    if fails:
        out += ["", "Failures:", "", "```text", *fails[:60], "```"]
    out += ["", f"GNOMAD's screens, their 2x copies and the labelled sheet: `{TAG}-skin-screens`."]
else:
    out += ["not run"]

out += block("The package to install (`.github/ci/released_package.py`, `web/make_site.py`)",
             lines(BUILD / "released-package.log") + lines(BUILD / "site.log"), "not made")
print("\n".join(out))
