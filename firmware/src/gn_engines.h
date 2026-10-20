/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kyle Palaniuk */
/* Gnomad (FORK.md, "Gnomad engines"): the engine swap's build options, the shipping configuration.
 * felucca.c includes this first, before core.h; nothing else does, so upstream's host tests (tests/hostsim.c)
 * and the fork's existing tests build Felucca's engines as before (GN_ENGINES 0 from gn_size_flags.h), and a
 * test of the swap sets GN_ENGINES 1 and includes this before hostsim.c (tests/gn_core_test.c).
 *
 * GN_ENGINES 1: the Gnomad sound core (firmware/gnomad-core, adapter gn_core.c) renders the Gnomad engines
 * VA, FM, DRUMS, GNOME and SAMPLER in the slots of the six Felucca engines it retires (the replacement
 * list: PHYS, GRAIN, WHEEL, FM6, SLICE, DRUM, compiled out with the size-plan flags), and the bundled factory
 * sample block is left out. ANALOG, PHASE, LOFI, VOICE, TRIO, NOISE and SAMPLE stay; each can be compiled out
 * by its own flag (GN_DROP_<ENGINE> 1: a silent descriptor in its slot, gn_size_stub.h) for a
 * later decision. FELUCCA_GN 0 (every fork hook out) builds no swap either.
 *
 * Overrides without touching the build interface (tools/build.py is upstream's): build/gen/gn_engines_config.h
 * (e.g. "#define GN_ENGINES 0", or "#define GN_DROP_NOISE 1"), read first. */
#pragma once
#if defined(__has_include)
#if __has_include("gn_engines_config.h")
#include "gn_engines_config.h"
#endif
#endif
#ifndef GN_ENGINES
#if defined(FELUCCA_GN) && !FELUCCA_GN
#define GN_ENGINES 0
#else
#define GN_ENGINES 1
#endif
#endif

#if GN_ENGINES
#if defined(FELUCCA_FM4) && FELUCCA_FM4
#error "GN_ENGINES: FELUCCA_FM4 (DIGITAL) converts its sounds to FM6, which the swap retires (engines.c asserts the order too)"
#endif
/* the six engines whose slots the Gnomad engines take: always out with the swap */
#if (defined(GN_DROP_PHYS) && !GN_DROP_PHYS) || (defined(GN_DROP_GRAIN) && !GN_DROP_GRAIN) || \
    (defined(GN_DROP_WHEEL) && !GN_DROP_WHEEL) || (defined(GN_DROP_FM6) && !GN_DROP_FM6) || \
    (defined(GN_DROP_SLICE) && !GN_DROP_SLICE) || (defined(GN_DROP_DRUM) && !GN_DROP_DRUM)
#error "GN_ENGINES: PHYS, GRAIN, WHEEL, FM6, SLICE and DRUM are retired (their slots are the Gnomad engines')"
#endif
#define GN_DROP_PHYS 1
#define GN_DROP_GRAIN 1
#define GN_DROP_WHEEL 1
#define GN_DROP_FM6 1
#define GN_DROP_SLICE 1
#define GN_DROP_DRUM 1
#ifndef GN_DROP_SAMPLES
#define GN_DROP_SAMPLES 1      /* the bundled factory sample block (SAMPLE's built-in sets: empty, indices kept) */
#endif
/* The decision (2026-10-07 "Felucca's engines retire", confirmed 2026-10-11 when gn/ux failed to
 * link with them in): the seven remaining Felucca engines are out of the shipping build too. Each stays overridable
 * from gn_engines_config.h (GN_DROP_<ENGINE> 0) for a comparison build. */
#ifndef GN_DROP_ANALOG
#define GN_DROP_ANALOG 1
#endif
#ifndef GN_DROP_PHASE
#define GN_DROP_PHASE 1
#endif
#ifndef GN_DROP_LOFI
#define GN_DROP_LOFI 1
#endif
#ifndef GN_DROP_VOICE
#define GN_DROP_VOICE 1
#endif
#ifndef GN_DROP_TRIO
#define GN_DROP_TRIO 1
#endif
#ifndef GN_DROP_NOISE
#define GN_DROP_NOISE 1
#endif
#ifndef GN_DROP_SAMPLE
#define GN_DROP_SAMPLE 1
#endif
#endif
