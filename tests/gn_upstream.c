/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kyle Palaniuk */
/* Gnomad (FORK.md): one of upstream's host tests, built with the fork's hooks in, as the firmware has
 * them: gn_hooks.h before the test's sources (FELUCCA_GN 1: every `#if FELUCCA_GN` line in seq.c,
 * ui_input.c, ui_draw.c and editor.c is compiled), the test itself unchanged, then gn_fork.c, the
 * fork's code for the sources the test has (gn_fork.c: Easy's keys with seq.c; its panel, LEDs and
 * screens with the UI; its project hook with the editor). Easy is never entered (only HOME + SAVE held
 * 2 s enter it, and no upstream test holds both), so each test must pass as it does without the fork,
 * and what it writes must be the same bytes: the proof that the hooks change nothing while Easy is
 * off. Built with -DFELUCCA_GN=0 it is the test as upstream builds it (every hook compiled out), the
 * reference the outputs are compared with. tests/run_fork_tests.sh builds it for each test, with the
 * test's own flags and arguments:
 *   cc ... '-DGN_UPSTREAM_TEST="regress.c"' -Itests -o build/host-gn/up_regress tests/gn_upstream.c -lm */
#ifndef GN_UPSTREAM_TEST
#error "GN_UPSTREAM_TEST: the upstream test's source, e.g. \"regress.c\""
#endif
#define GN_START_EASY 0          /* upstream's start: Advanced, upstream's palette (gn_hooks.h) */
#include "../firmware/src/gn_hooks.h"
#include GN_UPSTREAM_TEST
#if FELUCCA_GN
#include "../firmware/src/gn_fork.c"
#ifndef KB_SILENT
#error "the test has no seq.c: no hook to test"
#endif
#endif
