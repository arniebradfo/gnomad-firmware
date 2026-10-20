#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Kyle Palaniuk
# Gnomad (FORK.md, "Gnomad engines", round FW-CONTENT): vendor (or re-vendor) the factory sound banks into
# firmware/gnomad-content.
#
#   firmware/gnomad-content/sync-content.sh --core [REF]     (round CR: the one-command recipe)
#     REF   a commit, tag or branch of gnomad-core (default: main); GNOMAD_CORE names the clone (~/dev/gnomad-core).
#           Exports REF with `git archive` (committed bytes only), builds its banks there as the core does
#           (`make banks`: scripts/gen_factory_pcm.py --check, tools/author/build_banks.py --check against the
#           commit's content/BANKS.sha256, tools/author/build_easy.py --check), and vendors content/build's
#           pitched.gcpbank, kits.gckbank and easy.tsv with content/pcm. Every field it writes comes from the commit,
#           so a re-run at the same commit leaves git clean.
#   firmware/gnomad-content/sync-content.sh SRC [PCM]        (a folder of built banks, e.g. a snapshot)
#     SRC   a folder with pitched.gcpbank, kits.gckbank and easy.tsv (gnomad-core's content/build)
#     PCM   a folder with factory.manifest and its .pcm assets (default: SRC if it has factory.manifest, else
#           SRC/../pcm, which is gnomad-core's content/pcm beside content/build)
#   SOURCE="gnomad-core <sha> ..."   what the bytes are (CONTENT-VERSION.txt); default: SRC's git commit, if any
#
# One command, nothing else to edit: it copies the banks and the PCM here as they are (bytes only: the catalogue fills
# later and the files are simply replaced), writes
#   easy.tsv              Easy's sound set by bank id (gnomad-core content/easy.toml, names resolved by the core's
#                         build_easy.py); tools/gn_gen_easy.py turns it into gn_easy_set.h
#   gn_easy_set.h         Easy's generated rows (tools/gn_gen_easy.py; checks every id's record against its name)
#   gn_content.h          the const arrays and the per-engine lists the firmware compiles (tools/gn_gen_content.py:
#                         it refuses malformed PCM, a bank of the wrong size or an engine with no authored sound)
#   CONTENT-VERSION.txt   the source, the authored counts, every file's SHA-256
# then prints `git status` of this folder. Commit the result ("engines: re-vendor content <source>").
# tests/gn_content_test.py (run_fork_tests.sh, CI) fails on any other byte, and regenerates gn_content.h to check it.
set -eu
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../.." && pwd)
tmp=$(mktemp -d "${TMPDIR:-/tmp}/gn-content.XXXXXX")
trap 'rm -rf "$tmp"' EXIT
if [ "${1:-}" = "--core" ]; then
    repo=${GNOMAD_CORE:-$HOME/dev/gnomad-core}
    sha=$(git -C "$repo" rev-parse --verify "${2:-main}^{commit}")
    mkdir "$tmp/core"
    git -C "$repo" archive "$sha" | tar -x -C "$tmp/core"
    (cd "$tmp/core" && python3 -I scripts/gen_factory_pcm.py --check >/dev/null &&
        python3 -I tools/author/build_banks.py --check >/dev/null && python3 -I tools/author/build_easy.py --check >/dev/null) ||
        { echo "sync-content.sh: gnomad-core $sha does not build its banks to its own BANKS.sha256" >&2; exit 1; }
    src=$tmp/core/content/build
    pcm=$tmp/core/content/pcm
    source="gnomad-core $sha ($(git -C "$repo" log -1 --format='%s, %cI' "$sha")): banks built from the commit by its own build_banks.py, equal to its content/BANKS.sha256"
    srcl="gnomad-core $(echo "$sha" | cut -c1-7):content/build (git archive; make banks)"
    pcml="gnomad-core $(echo "$sha" | cut -c1-7):content/pcm"
else
    src=$(cd "${1:?usage: sync-content.sh --core [REF] | sync-content.sh SRC [PCM]}" && pwd)
    if [ $# -ge 2 ]; then pcm=$(cd "$2" && pwd)
    elif [ -f "$src/factory.manifest" ]; then pcm=$src
    else pcm=$(cd "$src/../pcm" && pwd)
    fi
    source=${SOURCE:-$(git -C "$src" log -1 --format='%h %s' 2>/dev/null || echo "unknown (set SOURCE=)")}
    srcl=$src
    pcml=$pcm
fi
names=$(awk '$1 ~ /^[0-9]+$/ {print $2 ".pcm"}' "$pcm/factory.manifest")
[ -n "$names" ] || { echo "sync-content.sh: $pcm/factory.manifest lists no asset" >&2; exit 1; }

mkdir "$tmp/out"
cp "$src/pitched.gcpbank" "$src/kits.gckbank" "$src/easy.tsv" "$pcm/factory.manifest" "$tmp/out/"
for f in $names; do cp "$pcm/$f" "$tmp/out/"; done
python3 -I "$root/tools/gn_gen_content.py" "$tmp/out" "$tmp/out/gn_content.h" >/dev/null
python3 -I "$root/tools/gn_gen_easy.py" "$tmp/out" "$tmp/out/gn_easy_set.h" >/dev/null
census=$(python3 -I "$root/tools/gn_gen_content.py" "$tmp/out" --list | sed 's/^/  /')

# replace the vendored copy
rm -f "$here"/*.gcpbank "$here"/*.gckbank "$here"/*.pcm "$here/factory.manifest" "$here/gn_content.h" "$here/easy.tsv" \
    "$here/gn_easy_set.h"
for f in pitched.gcpbank kits.gckbank easy.tsv factory.manifest $names gn_content.h gn_easy_set.h; do cp "$tmp/out/$f" "$here/"; done
{
    echo "Gnomad factory content vendored into the Gnomad firmware (FORK.md, \"Gnomad engines\", round FW-CONTENT)"
    echo
    echo "source   $source"
    echo "banks    $srcl"
    echo "pcm      $pcml"
    echo "vendored by firmware/gnomad-content/sync-content.sh (no time, no local path with --core: a re-run at the same"
    echo "         commit writes the same bytes)"
    echo
    echo "Licence: the content is the owner's (Kyle Palaniuk), as the core is (gnomad-core SPEC/30, D01); inside this"
    echo "firmware it is granted under GPL-3.0-only as part of the corresponding source (../gnomad-core/NOTICE.txt)."
    echo
    echo "authored (tools/gn_gen_content.py: every authored slot, in bank order; NAMES and PRESET offer an engine's first 127,"
    echo "FORK.md round FW-CONTENT-5; Easy binds by bank ID, round FW-CONTENT-6):"
    echo "$census"
    echo
    echo "files (SHA-256):"
    for f in pitched.gcpbank kits.gckbank easy.tsv factory.manifest $names gn_content.h gn_easy_set.h; do
        printf '  %s  %s\n' "$(shasum -a 256 "$here/$f" | cut -d' ' -f1)" "$f"
    done
} >"$here/CONTENT-VERSION.txt"
cat "$here/CONTENT-VERSION.txt"
git -C "$here" status --short -- .
