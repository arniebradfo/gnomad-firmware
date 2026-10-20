/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kyle Palaniuk */
/* Gnomad (FORK.md): the fork's code, Easy mode. felucca.c includes this file just before main.c,
 * after everything it hooks into; gn_hooks.h holds the flag and the declarations the hooks need
 * earlier. Each part below is compiled when the sources it hooks into are (the firmware has them
 * all; a host program may have only some: tests/gn_upstream.c builds upstream's own tests with the
 * fork's flag on, and each of them takes only the sources it tests). */
#if FELUCCA_GN
#ifdef KB_SILENT                 /* seq.c: Easy's state, its sounds and pads, the keys; the editor's hook */
_Static_assert(GN_KEY_REST == KB_SILENT, "gn_hooks.h GN_KEY_REST is seq.c's KB_SILENT");
#include "gn_state.c"
#endif
#ifdef LED_PLAY_GREEN            /* ui_input.c, with the rest of the UI: Easy's panel, LEDs and screens */
#if !defined(KB_SILENT) || !defined(COL_W)
#error "Easy needs seq.c and ui_draw.c with ui_input.c"
#endif
#include "gn_gfx.c"              /* anti-aliased drawing in the design boards' terms, in the canvas's tail */
#include "gn_type.c"             /* the fork's stroke type */
#include "gn_art.c"              /* the Easy screens' pictures */
#include "gn_easy.c"             /* Easy mode: panel, LEDs, screens (gn_screens.c) */
#endif
#endif
