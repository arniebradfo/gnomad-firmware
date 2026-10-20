/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kyle Palaniuk */
/* Gnomad, a fork of Felucca (FORK.md): Easy mode on upstream's v1.0.1. This file holds the
 * fork's build flag, the product's name, and what the hooks in upstream files need before those
 * files are compiled. felucca.c includes it just before seq.c (the first file with a hook), and
 * gn_fork.c (the fork's code) just before main.c. Each hook is an `#if FELUCCA_GN` line or two in an
 * upstream file (FORK.md, touch list). The update loader and upstream's own tests do not include
 * this file, so for them every hook compiles out; tests/gn_upstream.c builds upstream's tests with
 * it, to show the hooks change nothing while Easy is off. */
#pragma once
#include <stdint.h>
#ifndef FELUCCA_GN
#define FELUCCA_GN 1             /* the fork's additions; 0 = upstream's behaviour */
#endif
#if FELUCCA_GN
#ifdef KB_SILENT                 /* seq.c's: it came first, and its hook would compile out unseen */
#error "gn_hooks.h must be included before seq.c (FORK.md)"
#endif

/* The product's name, wherever the synth shows it (Easy's start-up screen, gn_screens2.c). The
 * only place it is written in the firmware; the workflow reads it from this line for the build's
 * name (FELUCCA_VERSION, build/gen/gn_name.h). */
#define GN_NAME "GNOMAD"

/* Round 7, pick 1: the first power-on (no settings record of ours) is Easy, in GNOMAD's colours where the
 * Advanced skin is built in (GSK_PAL); after that the synth starts in the mode it was in when it was turned
 * off (gn_easy.c gn_boot, at the first main-loop frame: nothing of ours runs earlier). 0 = upstream's start,
 * Advanced, upstream's palette: tests/gn_upstream.c builds upstream's own tests so, and none of them ever
 * enters Easy; the host tests of Easy choose it per case (tests/gn_host.h). */
#ifndef GN_START_EASY
#define GN_START_EASY 1
#endif

/* Decided 2026-10-11: Easy's five demo grooves (gn_grooves.c), read only in flash, loaded by PRESETS
 * on the loop page. 0: none (PRESETS there is the sound inside the picture, as everywhere else) */
#ifndef GN_DEMO_GROOVES
#define GN_DEMO_GROOVES 1
#endif

/* seq.c keyboard_block: Easy's keys. Out of line: in the audio ISR's per-key loop it is one call,
 * made only when a key goes down. A key that rests returns GN_KEY_REST, seq.c's KB_SILENT (the
 * same value, gn_fork.c checks it): keyboard_block then sends nothing for it, down or up. */
#define GN_KEY_REST 255u
struct track;
static uint32_t gn_key(struct track *t, uint32_t k, uint32_t note) __attribute__((noinline));
/* Cold MIDI / panic work must stay out of the IRQ's render and queue loops.
 * Match upstream midi_event's noinline boundary; the block-rate watch stays small. */
static void gn_stop_notes(uint32_t usb_only) __attribute__((noinline));
static void gn_usb_watch(void);
static void gn_usb_notes_off(void) __attribute__((noinline));
#define GN_MIDI_TRACK_MASK 7u
static void gn_midi_event(uint32_t st, uint32_t ch, uint32_t d1, uint32_t d2, uint32_t source) __attribute__((noinline));
static int gn_panic_track(uint32_t track) __attribute__((noinline));
/* audio.c audio_block, after the USB audio tap (round 26): the metronome's click into the codec's output
 * only, never the USB audio, a pattern or MIDI. Out of line: one call a block, the ISR's loops as they were */
static void gn_click(int32_t *out, uint32_t n) __attribute__((noinline));
static int gn_rec_hit(const struct track *t);        /* seq.c rec_note: 1 = a hit, not held (Easy's kit) */
static int gn_input(void);                           /* ui_input.c ui_input: 1 = Easy took the input */
static int gn_leds(void);                            /* ui_input.c ui_leds: 1 = Easy set the LEDs */
static int gn_draw(void);                            /* ui_draw.c ui_draw: 1 = Easy drew the screen */
static void gn_project(uint32_t op, uint32_t slot);  /* editor.c ED_PROJECT, after it: a save the app asked for */
static void gn_project_saved(uint32_t slot);        /* project.c project_save_as, a save on flash (round 27) */
static void gn_editor(uint32_t cmd, const uint8_t *a, uint32_t na);   /* editor.c ed_handle, before any command */

#endif
