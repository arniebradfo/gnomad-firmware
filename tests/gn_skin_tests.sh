#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Kyle Palaniuk
# Gnomad, a fork of Felucca (see FORK.md). Not part of upstream Felucca.
# The Advanced skin's host tests (option C, phase 1: firmware/src/gn_skin*.c), from the repo root after ./build.sh
# (build/gen); the workflow runs them after upstream's tests/run_tests.sh and tests/run_fork_tests.sh:
#   tests/gn_skin_tests.sh BASE SCREENS
# BASE: the sources of the base this branch is built on (gn/base-1.0.1: its firmware/ and tests/); the same test
# programs are built against them, without GSK_BUILD. SCREENS: where GNOMAD's screens go (tests/gn_skin_sheet.py).
#
# skin        tests/gn_skin_test.c: GNOMAD's roles exactly, every other palette after it as before; MENU > COLOR's
#             ninth; every word entry still matching upstream's labels and titles; every card, title and phrase
#             the default build draws in words; every label within Leo's card, every message within the header.
# stored id   tests/gn_skin_persist_test.c, built twice: 13 is GNOMAD in Gnomad (import, export, boot, the
#             retained SET3 record, the backup's settings object, the record upstream's deferred save writes); in
#             the base (1.0.1) 13 stays PAPER, and the record Gnomad wrote passes 1.0.1's backup validation and import.
# inert       tests/gn_skin_render.c inert, built against the base and against this tree: every scene of upstream's
#             host renderer (tests/ui_render.c), every page of every engine, HOME, every hold layer and every value
#             of every card in each of the eight palettes: the two lists of hashes must be equal (byte-identical
#             renders); and in this tree, drawing each scene in GNOMAD first leaves no pixel behind.
# screens     tests/gn_skin_render.c sheet: GNOMAD's screens (option C's catalogue, the descriptor variants, the hold
#             layers, the menu, NAME, every confirmation, calibration, messages) and every value of every card,
#             linted by upstream's lint; the PNGs, their 2x copies and the labelled sheet in SCREENS.
set -e
cd "$(dirname "$0")/.."
[ $# -eq 2 ] || { echo "usage: tests/gn_skin_tests.sh BASE SCREENS"; exit 2; }
BASE=$1
SCREENS=$2
[ -f build/gen/ui_palettes.h ] || { echo "run ./build.sh first (build/gen)"; exit 1; }
[ -f "$BASE/tests/ui_render.c" ] || { echo "no base sources in $BASE"; exit 1; }
OUT=build/host-gn-skin
mkdir -p "$OUT"
CC="${CC:-cc} -O1 -w"
fail=0
run() { echo "== $1"; shift; "$@" || fail=1; }

$CC -DGSK_BUILD=1 -Ibuild/gen -Ifirmware/src -Itests -o "$OUT/gn_skin_test" tests/gn_skin_test.c -lm
run "skin: the palette, MENU > COLOR, the words (matching upstream, complete, fitting)" "$OUT/gn_skin_test"

# the base's build of the same programs (its generated headers are this build's: only ui_palettes.h differs, and
# only under GSK_BUILD)
cp tests/gn_skin_persist_test.c tests/gn_skin_render.c "$BASE/tests/"
$CC -DGSK_BUILD=1 -Ibuild/gen -Ifirmware/src -o "$OUT/gn_skin_persist" tests/gn_skin_persist_test.c -lm
$CC -Ibuild/gen -o "$OUT/gn_skin_persist_base" "$BASE/tests/gn_skin_persist_test.c" -lm
run "stored ids 13, 70, 72 through Gnomad: import, export, backup, preferences commands, a backup round trip" \
    "$OUT/gn_skin_persist" "$OUT/settings-gnomad.bin"
run ".. through the base (1.0.1); then Gnomad's settings object restored there and read back" \
    "$OUT/gn_skin_persist_base" "$OUT/settings-via-1.0.1.bin" "$OUT/settings-gnomad.bin"
run ".. and that object, back in Gnomad: PAPER, every other setting as Gnomad had it" \
    "$OUT/gn_skin_persist" "$OUT/settings-back.bin" "$OUT/settings-via-1.0.1.bin"

$CC -DGSK_BUILD=1 -Ibuild/gen -Ifirmware/src -Itests -o "$OUT/gn_skin_render" tests/gn_skin_render.c -lm
$CC -Ibuild/gen -o "$OUT/gn_skin_render_base" "$BASE/tests/gn_skin_render.c" -lm
run "eight palettes, the base's renders" "$OUT/gn_skin_render_base" inert "$OUT/inert-base.txt"
run "eight palettes, this tree's renders (and after GNOMAD)" "$OUT/gn_skin_render" inert "$OUT/inert-skin.txt"
# Branding, round G (FORK.md): the ABOUT document is Gnomad's (ui_menu.c), so its four scenes are left out of the
# comparison, and only they: the same scene list on both sides, and every other render byte-identical
ABOUT='^[0-9]+ about(_rec|_credits|_end)? '
grep -Ev "$ABOUT" "$OUT/inert-base.txt" > "$OUT/inert-base-noabout.txt"
grep -Ev "$ABOUT" "$OUT/inert-skin.txt" > "$OUT/inert-skin-noabout.txt"
same_scenes() {                            # the same scene list, and ABOUT's four scenes in each of the eight palettes
    cut -d' ' -f1,2 "$OUT/inert-base.txt" > "$OUT/inert-base.scenes"
    cut -d' ' -f1,2 "$OUT/inert-skin.txt" > "$OUT/inert-skin.scenes"
    cmp "$OUT/inert-base.scenes" "$OUT/inert-skin.scenes" && [ "$(grep -Ec "$ABOUT" "$OUT/inert-skin.txt")" -eq 32 ]
}
run "eight palettes: the same scenes as the base, ABOUT's 4 x 8 among them" same_scenes
run "eight palettes: byte-identical to the base but ABOUT ($(wc -l < "$OUT/inert-base-noabout.txt" | tr -d ' ') screens, pages and card sweeps)" \
    cmp "$OUT/inert-base-noabout.txt" "$OUT/inert-skin-noabout.txt"

rm -rf "$OUT/sheet" "$SCREENS"
mkdir -p "$OUT/sheet"
run "GNOMAD: the catalogue's screens and every card value, upstream's lint" "$OUT/gn_skin_render" sheet "$OUT/sheet"
$CC -DGSK_BUILD=1 -DFELUCCA_FM4=1 -Ibuild/gen -Ifirmware/src -Itests -o "$OUT/gn_skin_render_fm4" tests/gn_skin_render.c -lm
run "GNOMAD: the five operator pages a FELUCCA_FM4=1 build shows, upstream's lint" "$OUT/gn_skin_render_fm4" ops "$OUT/sheet"
if python3 -c "import PIL" 2>/dev/null; then
    run "GNOMAD: the PNGs, their 2x copies, the labelled sheet" python3 tests/gn_skin_sheet.py "$OUT/sheet" "$SCREENS"
    cp "$OUT/sheet/report.txt" "$SCREENS/lint-report.txt"
fi

[ $fail -eq 0 ] && echo "ALL SKIN TESTS PASSED" || { echo "SKIN TESTS FAILED"; exit 1; }
