/* SPDX-License-Identifier: GPL-3.0-only
 * Build-time exclusion of Felucca's engines and the bundled sample block (gn/size-plan's flags; Gnomad's engine
 * swap, gn_engines.h, sets the retired ones). Slots and wire/disk layouts never change. */
#pragma once
#if __has_include("gn_size_config.h")
#include "gn_size_config.h"
#endif
#ifndef GN_ENGINES
#define GN_ENGINES 0             /* Gnomad's engine swap (gn_engines.h: felucca.c sets it; host tests build without) */
#endif
#ifndef GN_DROP_ANALOG
#define GN_DROP_ANALOG 0
#endif
#ifndef GN_DROP_PHASE
#define GN_DROP_PHASE 0
#endif
#ifndef GN_DROP_LOFI
#define GN_DROP_LOFI 0
#endif
#ifndef GN_DROP_VOICE
#define GN_DROP_VOICE 0
#endif
#ifndef GN_DROP_TRIO
#define GN_DROP_TRIO 0
#endif
#ifndef GN_DROP_WHEEL
#define GN_DROP_WHEEL 0
#endif
#ifndef GN_DROP_GRAIN
#define GN_DROP_GRAIN 0
#endif
#ifndef GN_DROP_PHYS
#define GN_DROP_PHYS 0
#endif
#ifndef GN_DROP_SLICE
#define GN_DROP_SLICE 0
#endif
#ifndef GN_DROP_SAMPLE
#define GN_DROP_SAMPLE 0
#endif
#ifndef GN_DROP_DRUM
#define GN_DROP_DRUM 0
#endif
#ifndef GN_DROP_FM6
#define GN_DROP_FM6 0
#endif
#ifndef GN_DROP_SAMPLES
#define GN_DROP_SAMPLES 0
#endif
#define GN_SAMPLE_USERS (!GN_DROP_SAMPLE || !GN_DROP_GRAIN || (FELUCCA_SLICE && !GN_DROP_SLICE))

#ifndef GN_DROP_NOISE
#define GN_DROP_NOISE 0
#endif

/* Gnomad (FORK.md, round UX-L): with the swap, Felucca's engines it compiled out beyond the six it replaces (ANALOG
 * PHASE LOFI SAMPLE VOICE TRIO NOISE, gn_engines.h): their slots keep their numbers (the stores, the protocol) and
 * nothing on the synth lists them (engines.c eng_gone, ENGINE_ORDER; core.h NENG_SHOWN) */
#define GN_GONE_N (GN_ENGINES ? GN_DROP_ANALOG + GN_DROP_PHASE + GN_DROP_LOFI + GN_DROP_SAMPLE + GN_DROP_VOICE + \
                                GN_DROP_TRIO + GN_DROP_NOISE : 0)
/* ... and the editor: 1 = such a slot answers with an empty descriptor (INFO and DESC's ENG name "", NAMES 0 presets).
 * Off until the app reads it: the app's FirmwareShape.gnomad10 asks INFO for the seven names exactly */
#ifndef GN_EDITOR_HIDES_GONE
#define GN_EDITOR_HIDES_GONE 0
#endif

/* Gnomad engines (FORK.md, round FW-CONTENT-7): a preset of engine e, wherever a preset is read (the screens, NAMES,
 * a sound load, Easy, the console). Felucca's engines: their table, e->presets[k], as upstream reads it. The Gnomad
 * engines (GN_ENGINES) keep no preset_t a sound (.presets 0): a preset is a bank ID, its name and its knobs read from
 * the record (gn_core.c gn_preset_name, gn_preset_get; GN_PRESET's preset_t is a compound literal of the caller's
 * block, so no RAM of its own). k: a preset as the track or the list holds it (0..npresets-1; Easy's place in the
 * whole list on a Gnomad engine) */
#if GN_ENGINES
#define GN_PRESET(e, k) gn_preset_get((e), (k), &(preset_t){0})
#define GN_PRESET_NAME(e, k) gn_preset_name((e), (k))
#define GN_PRESET_PAT(e, k) gn_preset_pat((e), (k))   /* (a Gnomad preset suggests no pattern: 0) */
#else
#define GN_PRESET(e, k) (&(e)->presets[k])
#define GN_PRESET_NAME(e, k) ((e)->presets[k].name)
#define GN_PRESET_PAT(e, k) ((e)->presets[k].pat)
#endif
