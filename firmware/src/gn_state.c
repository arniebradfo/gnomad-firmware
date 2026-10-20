/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kyle Palaniuk */
/* Gnomad (FORK.md): Easy mode's state, its eight sounds, its drum kit and sixteen pads, and the two
 * hooks that need no more than the sound and sequencer sources: the keys (seq.c keyboard_block, in the
 * audio ISR) and the editor's project command (editor.c). Included by gn_fork.c; Easy's panel, LEDs
 * and screens are gn_easy.c. Until HOME + SAVE held 2 s have entered Easy (gn.on), both hooks return
 * at once and change nothing. */
#define GN_STEPS 16                       /* a knob's amounts: 0 .. GN_STEPS */
/* the pages: the sound (GP_PLAY: the selected track's picture and its four knobs; HOME again from Home); the
 * loop's three (SEQ: the loop, the steps, the record screen, round and round); the four volumes (GLO); Home
 * (round UX-1: HOME, the four tracks at a glance, board GN-03 A); round SEQ-1 (phase 1): the grooves shelf
 * and the song, both "soon" in phase 1. SELECT turns the page ring HOME SOUND GROOVES LOOP STEPS SONG MIX */
enum { GP_PLAY, GP_LOOP, GP_STEPS, GP_RECORD, GP_MIX, GP_HOME, GP_GROOVES };   /* (round FLOW-1: no SONG page until real) */
#define GN_FLOW1 3                        /* round FLOW-1's milestone in this build (the simulator reads what is there) */
#define GN_LOOP_PAGE(pg) ((pg) >= GP_LOOP && (pg) <= GP_RECORD)
#define GN_TAP_MS 2000u                   /* the tempo's big number stays after the last EDIT tap */
#define GN_HOLD_MS 2000u                  /* HOME + SAVE */
#define GN_PICK_MS 1600u                  /* the pick screen stays after the last ALGORITHM turn */
#define GN_PEEK_MS 2000u                  /* the peek list stays after the last PRESETS turn */
#define GN_SAVED_MS 6000u
#define GN_BOOT_MS 3000u                  /* the start-up picture when Easy is entered */
#define GN_LOOP 16                        /* REC into an empty pattern: one bar of 1/16 (SNAP on: the drums; round FLOW-1) */
enum { GN_DIV8 = 1, GN_DIV16 = 2, GN_DIV32 = 3 }; /* Easy's grids (params.c N_DIV): 1/8, 1/16, 1/32 a step */
#define GN_MELODY 0u                      /* the track Easy's eight sounds go on: track 1 */
#define GN_DRUMS (NTRK - 1u)              /* the track Easy's kit goes on: track 4 */

#include "../gnomad-content/gn_easy_set.h"      /* easy.toml's pictures by bank name (generated, round CR) */

/* ------------------------------------------------------------ engines --- */
/* Felucca's engine numbers (engines.c ENGINES[]: the stores and the editor protocol hold them, so
 * they never move; tests/gn_easy_test.c checks each name) */
enum {
    GE_ANALOG, GE_RSV1, GE_PHASE, GE_LOFI, GE_SAMPLE, GE_VOICE, GE_TRIO, GE_WHEEL, GE_GRAIN, GE_PHYS, GE_DRUM,
    GE_NOISE, GE_FM6, GE_SLICE
};
_Static_assert(GE_RSV1 == ENGI_DIGITAL && GE_PHYS == ENGI_PHYS && GE_DRUM == ENGI_DRUM, "engines.c's engine numbers");

/* --------------------------------------------------------- the sounds --- */
/* Round 7, pick 3: the nine pictures are groups. ALGORITHM picks the picture, PRESETS the sound inside it;
 * a picture's first sound is the picture's sound of rounds 1 to 6. Each sound is one of 1.0.1's factory
 * presets (by name, FACTS-SYNTH-1.0.md; FORK.md lists the substitutions) plus a few settings, with its own
 * Bright, Wobble, Echo and Space (const tables, keyed by the sound Easy remembers on the track: gn.own) */
enum { GK_BRIGHT, GK_WOBBLE, GK_ECHO, GK_SPACE };
typedef struct {
    uint8_t id;                           /* P_* */
    int8_t v;
} gn_set_t;
/* a knob's travel over amounts 0 .. GN_STEPS: lo at 0, mid at the sound's rest (at the middle, 8, when it
 * rests at 0), hi at GN_STEPS: two straight pieces; and a second setting moved with it the same way */
typedef struct {
    uint8_t id;
    int8_t lo, mid, hi;
    uint8_t rest;                         /* the amount the sound starts with */
    uint8_t id2;                          /* GN_NONE: none */
    int8_t lo2, mid2, hi2;
} gn_knob_t;
enum { GNF_SYLL = 1, GNF_PADS = 2 };      /* the keys: the Gnome's syllables; pads (the GM kit, GN_PADS) */
typedef struct {
    const char *name;
    uint8_t engine, preset;               /* Felucca's engine (ENGINES[]) and factory preset */
    uint8_t pic;                          /* its picture, 0 .. GN_NPICS - 1 */
    int8_t octave;                        /* the keys' octave */
    uint8_t flags;                        /* GNF_* */
    uint8_t nset;
    const gn_set_t *set;                  /* after the preset and GN_COMMON: speak at once, ring, level */
    struct {
        gn_knob_t bright;                 /* Bright */
        uint8_t tail;                     /* Wobble, Echo, Space: GN_TAILS[tail] (a handful of kinds: kept once) */
    } kn;                                 /* (gn_snd_knob) */
} gn_sound_t;
#define GN_NONE 0xFFu

/* Every sound Easy puts on a track first gets these: the LFO Wobble moves (5.4 Hz, the first wave, no
 * fade), and none of the track's own arpeggio, chord keys, scale snap or transpose, which a sound load
 * keeps (ui.c param_kept) and which would make a key play or record other than its own note. */
static const gn_set_t GN_COMMON[7] = {{P_LRATE, 89}, {P_LWAVE, 0}, {P_LFADE, 0}, {P_AMODE, 0}, {P_CHRD, 0},
                                      {P_QUANT, 0}, {P_TRANS, 0}};

/* Bright: the engine's own brightness. Wobble: the LFO's amount to pitch (P_LD_PIT, the LFO at 5.4 Hz),
 * from none to +-1.9 semitones (LD_PIT 10: voice.c, 3/16 of a semitone a step). Echo: the delay send.
 * Space: the reverb send. Each sound's level (P_LEVEL, 0.5 dB a step; a sound load keeps the track's, so
 * Easy sets it) evens them out, quieter rather than louder. tests/gn_sound_test.c measures every one: each
 * speaks within 25 ms, and each knob makes a clear change on each across its travel (FORK.md, Easy:
 * sounds). GN_LIN: one straight travel lo .. hi with the rest on it (rounds 1 to 6's maps, unchanged) */
#define GN_MID(lo, hi, rest) ((lo) + (((hi) - (lo)) * ((rest) ? (rest) : GN_STEPS / 2) * 2 + GN_STEPS) / (2 * GN_STEPS))
#define GN_LIN(id, lo, hi, rest) {id, lo, GN_MID(lo, hi, rest), hi, rest, GN_NONE, 0, 0, 0}
#define GN_AT(id, lo, mid, hi, rest) {id, lo, mid, hi, rest, GN_NONE, 0, 0, 0}   /* the preset's mid at rest */
#define GN_WOB GN_LIN(P_LD_PIT, 0, 10, 0)
#define GN_ECHO GN_LIN(P_DLY, 0, 127, 3)
#define GN_SPACE GN_LIN(P_REV, 0, 127, 4)
/* Space for a short sound: the reverb send, and a second setting that lets it ring on into the room above rest */
#define GN_SPACE_RING(id, v, top) {P_REV, 0, GN_MID(0, 127, 4), 127, 4, id, v, v, top}
#define GN_WOB_AMP GN_LIN(P_LD_AMP, 0, 127, 0)         /* engines the LFO's pitch does not move: a tremolo */
#define GN_SET(...) (sizeof((const gn_set_t[]){__VA_ARGS__}) / sizeof(gn_set_t)), (const gn_set_t[]){__VA_ARGS__}
/* each engine's Bright (FACTS-SYNTH-1.0.md): VOICE SHIFT and BUZZ, WHEEL TOP, FM6 MLVL, LOFI TONE, GRAIN TONE
 * then SRC, PHASE DCW, TRIO CUT, PHYS BRIT, NOISE FREQ, ANALOG CUT, SAMPLE CUT, DRUM TONE, SLICE TONE: one straight
 * travel over the setting's range with the sound's own value at rest (GN_REST), so each half of the knob moves
 * half of the setting; where the setting runs out in one half, a second setting carries on (tuned by
 * tests/gn_sound_test.c) */
#define GN_REST(v, lo, hi) ((((v) - (lo)) * 2 * GN_STEPS + ((hi) - (lo))) / (2 * ((hi) - (lo))))
#define GN_LINAT(id, lo, hi, v) GN_LIN(id, lo, hi, GN_REST(v, lo, hi))
#define GB_FM6 GN_LIN(P_E2, -64, 63, 8)
#define GB_TONE(v) GN_LINAT(P_E7, 0, 127, v)          /* LOFI, SLICE TONE */
#define GB_E2(v) GN_LINAT(P_E2, 0, 127, v)            /* PHASE DCW, PHYS BRIT, NOISE FREQ, DRUM TONE */
#define GB_TRIO(v) GN_LINAT(P_E5, 0, 127, v)
#define GB_CUT(v) GN_LINAT(P_E4, 0, 127, v)           /* ANALOG and SAMPLE CUT */
/* VOICE: the formants' SHIFT (8 semitones either way of the sound's own) with the BUZZ */
#define GB_VOX(shift, buzz, rest) {P_E3, (shift) - 8, shift, (shift) + 8, rest, P_E4, 0, buzz, 127}
/* WHEEL with its upper bars full (the Organ's REG): TOP down below rest, the DRIVE up above it */
#define GB_WHEEL_FULL(drv) {P_E3, -8, 0, 0, 8, P_E6, drv, drv, 127}
#define GB_WHEEL_TOP GN_LIN(P_E3, 0, 8, 0)             /* WHEEL with its upper bars off: TOP up */
/* GRAIN: TONE up to the sound's own at rest, then a brighter source (SRC: piano, flute, sax) */
#define GB_GRAIN(src, tone) {P_E7, 0, tone, tone, 8, P_E0, src, src, 3}
#define GN_SPEAK 15                                    /* an attack that speaks at once (a pad's: ATK) */
/* a sound's Wobble, Echo and Space (gn_sound_t kn.tail): most are GN_FX; each kind is kept once */
enum { GN_FX, GN_FX_RING_50, GN_FX_RING_58, GN_FX_KIT, GN_FX_MACHINE, GN_FX_AMP, GN_NTAILS };
static const gn_knob_t GN_TAILS[GN_NTAILS][3] = {
    [GN_FX] = {GN_WOB, GN_ECHO, GN_SPACE},
    [GN_FX_RING_50] = {GN_WOB, GN_ECHO, GN_SPACE_RING(P_REL, 50, 110)},     /* ARCADE, FUNNY */
    [GN_FX_RING_58] = {GN_WOB, GN_ECHO, GN_SPACE_RING(P_REL, 58, 127)},     /* PLUCK */
    /* KIT: Wobble swings three times as far, Space adds the chorus send above rest; Echo's send is already at its top
     * (a track's send cannot go past it) */
    [GN_FX_KIT] = {GN_LIN(P_LD_PIT, 0, 30, 0), GN_ECHO, {P_REV, 0, GN_MID(0, 127, 4), 127, 4, P_CHOR, 0, 0, 127}},
    [GN_FX_MACHINE] = {GN_WOB_AMP, GN_ECHO, GN_SPACE_RING(P_E3, 64, 127)},  /* DRUM MACHINE */
    [GN_FX_AMP] = {GN_WOB_AMP, GN_ECHO, GN_SPACE},                          /* CHOPPED BREAK */
};

/* Gnomad engines (FORK.md, round FW-CONTENT-3; round CR: by name): a sound on a Gnomad engine names the bank sound it
 * plays by its bank NAME (firmware/gnomad-content/gn_content.h: GN_CONTENT_PN_<NAME>, a kit GN_CONTENT_KN_<NAME>, "slot,
 * preset index", generated from the vendored banks; a name the banks do not have, or have twice, does not compile, and
 * a renumbered bank keeps every sound right); without the swap it is the Felucca engine and preset it names first.
 * Easy's names stay Easy's; FORK.md lists which bank sound each plays. GN_EO(on, off): a value of the swap build only */
/* GN_GB: a Bright whose rest would write the core's own BRIT or WOBL (P_E0, P_E1 on a Gnomad slot) is, with the swap,
 * the core's Bright at its centre (gn_easy.c GN_CORE_KNOBS[0]: knob 64, the stored Bright of every authored sound
 * today), so the bank sound plays as authored */
#if GN_ENGINES
#define GN_EO(on, off) on                 /* (round CR: on the core P_LEVEL is a square law, 40 log10(L / 127) dB:
                                           * gn_core.c gnc_ctl; Felucca's is 0.5 dB a step) */
#define GN_BANK(e, p, name) GN_AUTHORED(GN_CONTENT_PN_##name)
#define GN_KIT(e, p, name) GN_AUTHORED(GN_CONTENT_KN_##name)
/* (round FW-CONTENT-6: the preset is the bank sound's place in its engine's whole authored list, which may be past
 * the first GN_PRESETS_MAX that engine offers over NAMES and PRESET, gn_core.c round FW-CONTENT-5: Easy still plays
 * it, gn_easy.c gn_preset_put. The id is an authored record of that engine, or GN_CONTENT_P<id> / K<id> does not
 * exist; its place is inside that engine's list and fits the track's preset byte, or it does not compile) */
#define GN_AUTHORED(...) GN_AUTHORED_(__VA_ARGS__)
#define GN_AUTHORED_(slot, k) slot, (k) + 0u * sizeof(char[(k) < GN_NAUTH(slot) && (k) < 255u ? 1 : -1])
#define GN_NAUTH(slot) ((slot) == GN_ENGI_VA ? GN_CONTENT_VA_COUNT : (slot) == GN_ENGI_FM ? GN_CONTENT_FM_COUNT : \
    (slot) == GN_ENGI_DRUMS ? GN_CONTENT_KITS_COUNT : (slot) == GN_ENGI_GNOME ? GN_CONTENT_GNOME_COUNT : \
    (slot) == GN_ENGI_SAMPLE ? GN_CONTENT_SAMPLE_COUNT : 0u)
#define GN_GB(...) GN_LIN(P_E0, 0, 127, 8)
/* GN_FEL: a sound on one of Felucca's seven engines the swap kept until 2026-10-11 (ANALOG PHASE LOFI SAMPLE VOICE
 * TRIO NOISE). The decision (gn_engines.h: "Felucca's engines retire in the shipping build") compiles
 * them out too (a silent descriptor in each slot, gn_size_stub.h), so with its engine dropped such a sound plays the
 * bank sound it names (FORK.md, round UX-R: the 21 picks, some "loose"); a comparison build that keeps
 * the engine (GN_DROP_<ENGINE> 0) plays Felucca's engine and preset as before */
#define GN_DROPPED(e) ((e) == GE_ANALOG ? GN_DROP_ANALOG : (e) == GE_PHASE ? GN_DROP_PHASE : (e) == GE_LOFI ? \
    GN_DROP_LOFI : (e) == GE_SAMPLE ? GN_DROP_SAMPLE : (e) == GE_VOICE ? GN_DROP_VOICE : (e) == GE_TRIO ? \
    GN_DROP_TRIO : (e) == GE_NOISE ? GN_DROP_NOISE : 0)
#define GN_FEL(e, p, name) GN_FEL_(e, p, GN_AUTHORED(GN_CONTENT_PN_##name))
#define GN_FEL_(...) GN_FEL__(__VA_ARGS__)
#define GN_FEL__(e, p, slot, k) (GN_DROPPED(e) ? (slot) : (e)), (GN_DROPPED(e) ? (k) : (p))
#else
#define GN_EO(on, off) off
#define GN_BANK(e, p, name) e, p
#define GN_KIT(e, p, name) e, p
#define GN_FEL(e, p, name) e, p
#define GN_GB(...) __VA_ARGS__
#endif

/* Round CR (FORK.md): the FUNNY picture is gnomad-core's content/easy.toml FUNNY (FUNNY v2), generated
 * (firmware/gnomad-content/gn_easy_set.h, tools/gn_gen_easy.py from easy.tsv): its names, its bank sounds by name, its
 * octaves, none typed here. Each plays its bank sound as authored (the VA pitch envelope is the joke) at one Easy level
 * (GN_FUNNY_LEVEL: the top, 127, +4 dB over 101 on the core's square law: at 101 the simulator measured their peaks
 * ~6 dB under the other pictures' sounds; the rest is the content's); without the swap each is ARCADE's Felucca
 * recipe (NOISE ZAP),
 * the engines-off build's stand-in for the host tests */
#define GN_FUNNY_LEVEL 127                /* (the engines-off stand-in at ARCADE's 106) */
#define GN_FUNNY_PIC 8                    /* (GN_FUNNY, below) */
#define GN_EZ_P(name) GN_FEL(GE_NOISE, 2, name)
#define GN_FUNNY_ROW(slot, name, bank, kind, oct) \
    {name, GN_EZ_##kind(bank), GN_FUNNY_PIC, oct, 0, GN_SET({P_LEVEL, GN_EO(GN_FUNNY_LEVEL, 106)}), \
     {GB_E2(96), GN_FX_RING_50}},
static const gn_sound_t GN_SOUNDS[] = {
    /* -------------------------------------------------------- GNOME (0) --- */
    /* the Gnome: VOX LEAD, one voice, each key a syllable (gn_key), a little higher than a man */
    {"GNOME", GN_FEL(GE_VOICE, 1, SING_AH), 0, 0, GNF_SYLL, GN_SET({P_VOICE, V_MONO}, {P_E2, 70}, {P_E3, 3}, {P_REL, 60}, {P_LEVEL, 96}),
     {GN_LIN(P_E4, 0, 127, 8), GN_FX}},
    {"VOX LEAD", GN_FEL(GE_VOICE, 1, SING_OO), 0, 0, 0, GN_SET({P_LEVEL, 96}), {GB_VOX(2, 90, 8), GN_FX}},
    {"WHISPER", GN_FEL(GE_VOICE, 3, WHISPER_OO), 0, 0, 0, GN_SET({P_ATK, GN_SPEAK}, {P_LEVEL, 100}), {GB_VOX(3, 50, 8), GN_FX}},
    {"WOW BASS", GN_FEL(GE_VOICE, 2, RUMBLE_AH), 0, -1, 0, GN_SET({P_LEVEL, GN_EO(127, 101)}), {GB_VOX(0, 100, 8), GN_FX}},
    /* -------------------------------------------------------- ORGAN (1) --- */
    {"ORGAN", GN_BANK(GE_WHEEL, 0, PARLOR_ORGAN), 1, 0, 0, GN_SET({P_LEVEL, 105}), {GB_WHEEL_FULL(20), GN_FX}},
    /* (WHEEL FULL ORGAN is the Organ itself: FM6 ORGAN, its patch DRAWBARS) */
    {"FULL ORGAN", GN_BANK(GE_FM6, 6, TIN_HARMONIUM), 1, 0, 0, GN_SET({P_LEVEL, 112}), {GN_AT(P_DIST, 0, 80, 127, 0), GN_FX}},
    {"GOSPEL", GN_BANK(GE_WHEEL, 2, STEADY_TINE), 1, 0, 0, GN_SET({P_LEVEL, 102}), {GB_WHEEL_TOP, GN_FX}},
    {"JAZZ", GN_BANK(GE_WHEEL, 1, REED_KEYS), 1, 0, 0, GN_SET({P_LEVEL, 105}), {GB_WHEEL_TOP, GN_FX}},
    {"ROCK DRIVE", GN_BANK(GE_WHEEL, 4, COPPER_CLAV), 1, 0, 0, GN_SET({P_LEVEL, 98}), {GN_GB({P_E1, 8, 0, -8, 8, P_E6, 0, 100, 127}), GN_FX}},
    {"TINE PIANO", GN_BANK(GE_FM6, 0, LAMPLIGHT), 1, 0, 0, GN_SET({P_LEVEL, 100}), {GB_FM6, GN_FX}},
    /* -------------------------------------------------------- BELLS (2) --- */
    /* the Bells: FM6 BELL (its patch GLASS BELL; 1.0 retired 0.9's DIGITAL BELL) */
    {"GLASS BELL", GN_BANK(GE_FM6, 1, ICE_CHIMES), 2, 0, 0, GN_SET({P_LEVEL, 108}), {GN_LIN(P_E2, -64, 63, 8), GN_FX}},
    {"PHASE BELL", GN_FEL(GE_PHASE, 4, TOWER_BELL), 2, GN_EO(1, 0), 0, GN_SET({P_LEVEL, GN_EO(83, 104)}), {GB_E2(80), GN_FX}},
    {"RING BELL", GN_FEL(GE_TRIO, 3, HANDBELL), 2, 0, 0, GN_SET({P_LEVEL, GN_EO(127, 114)}), {GB_TRIO(96), GN_FX}},
    {"BELL TREE", GN_BANK(GE_PHYS, 0, STARLIGHT_GLOCK), 2, 0, 0, GN_SET({P_LEVEL, 104}), {GB_E2(100), GN_FX}},
    {"MARIMBA", GN_BANK(GE_FM6, 5, TICK_TOCK), 2, 0, 0, GN_SET({P_LEVEL, 104}), {GB_FM6, GN_FX}},
    {"KALIMBA", GN_BANK(GE_PHYS, 4, KALIMBA_SEED), 2, 0, 0, GN_SET({P_LEVEL, 114}), {GB_E2(16), GN_FX}},
    /* -------------------------------------------------------- LASER (3) --- */
    /* the Laser: ARP 8BIT with the arpeggio its preset switched on in 0.9 (1.0 presets leave the
     * track's arpeggio alone): up, 1/16, two octaves, gate 40 */
    {"LASER", GN_FEL(GE_LOFI, 2, LASER_LINE), 3, 0, 0, GN_SET({P_AMODE, 1}, {P_ARATE, 2}, {P_AOCT, 2}, {P_AGATE, 40}, {P_LEVEL, 101}),
     {GN_LIN(P_E7, 20, 127, 13), GN_FX}},
    {"PULSE", GN_FEL(GE_LOFI, 0, PURE_SQUARE), 3, 0, 0, GN_SET({P_LEVEL, 101}), {GB_TONE(100), GN_FX}},
    /* (LOFI ARP 8BIT is the Laser itself: TRIO ARP LEAD, with the arpeggio as the Laser's) */
    {"8-BIT ARP", GN_FEL(GE_TRIO, 1, GLASS_SQUARE), 3, 0, 0, GN_SET({P_AMODE, 1}, {P_ARATE, 2}, {P_AOCT, 2}, {P_AGATE, 40}, {P_LEVEL, 100}),
     {GB_TRIO(88), GN_FX}},
    {"STEP LEAD", GN_FEL(GE_LOFI, 4, HOLLOW_SQUARE), 3, 0, 0, GN_SET({P_LEVEL, 97}), {GB_TONE(100), GN_FX}},
    {"CHIP CHOIR", GN_FEL(GE_TRIO, 4, VELVET_PULSE), 3, 0, 0, GN_SET({P_ATK, GN_SPEAK}, {P_LEVEL, 100}), {GB_TRIO(62), GN_FX}},
    {"ARCADE", GN_FEL(GE_NOISE, 2, SPACE_ZAP), 3, 0, 0, GN_SET({P_LEVEL, 106}), {GB_E2(96), GN_FX_RING_50}},
    {"GLITCH", GN_BANK(GE_GRAIN, 1, ROBOT_HICCUP), 3, 0, 0, GN_SET({P_LEVEL, 108}), {{P_E7, 0, 127, 127, 8, P_E4, 0, 0, 12}, GN_FX}},
    /* -------------------------------------------------------- WHALE (4) --- */
    /* the Whale: WOW BASS an octave down, a bigger singer, gliding from note to note. Round 24: every Whale
     * sound is a bass, its lowest key (F3 on the panel) a note at or below A2: SUB, SQUARE and FM BASS an octave
     * down (FAT BASS's preset is already: its third oscillator); tests/gn_sound_test.c t_whale_bass measures it.
     * (gn/ux catch-up, FW-P0: the octaves and FM BASS's Bright are round 24's, the bank ids gn/ux's) */
    {"WHALE", GN_FEL(GE_VOICE, 2, GIANT_AH), 4, -1, 0, GN_SET({P_E3, -6}, {P_GLIDE, 40}, {P_REL, 70}, {P_LEVEL, 101}),
     {{P_E3, -12, -6, 2, 11, P_E4, 0, 87, 127}, GN_FX}},
    {"SUB BASS", GN_FEL(GE_ANALOG, 7, MOSS_SUB), 4, -1, 0, GN_SET({P_LEVEL, 104}), {GN_AT(P_DIST, 0, 60, 127, 0), GN_FX}},
    {"SQUARE BASS", GN_FEL(GE_ANALOG, 2, SQUARE_ROOT), 4, -1, 0, GN_SET({P_LEVEL, 101}), {GB_CUT(50), GN_FX}},
    {"FM BASS", GN_BANK(GE_FM6, 2, DEEP_CURRENT), 4, -1, 0, GN_SET({P_LEVEL, 104}), {GN_AT(P_E2, -64, 0, 63, 4), GN_FX}},
    {"FAT BASS", GN_FEL(GE_TRIO, 0, THICK_FELT), 4, 0, 0, GN_SET({P_LEVEL, 101}), {GB_TRIO(62), GN_FX}},
    /* -------------------------------------------------------- CHOIR (5) --- */
    /* the Choir and the Cloud speak at once (their presets fade in slowly); the Cloud's grains are
     * shorter and denser, so the first one comes sooner */
    {"CHOIR", GN_FEL(GE_VOICE, 0, MUSHROOM_CHOIR), 5, 0, 0, GN_SET({P_ATK, 20}, {P_REL, 45}, {P_LEVEL, 87}), {GN_LIN(P_E4, 0, 127, 5), GN_FX}},
    {"SOFT PAD", GN_FEL(GE_ANALOG, 1, WOOL_PAD), 5, 0, 0, GN_SET({P_ATK, 20}, {P_LEVEL, 101}), {GB_CUT(60), GN_FX}},
    {"STRINGS", GN_FEL(GE_ANALOG, 11, SOFT_STRINGS), 5, 0, 0, GN_SET({P_ATK, 20}, {P_LEVEL, 101}), {GB_CUT(70), GN_FX}},
    {"SHIMMER", GN_BANK(GE_GRAIN, 3, GLASS_HARMONICS), 5, 0, 0, GN_SET({P_ATK, GN_SPEAK}, {P_LEVEL, 112}), {GN_GB(GB_GRAIN(0, 110)), GN_FX}},
    {"DRONE STRING", GN_BANK(GE_PHYS, 7, WARM_BLANKET), 5, 0, 0, GN_SET({P_LEVEL, 104}), {GB_E2(80), GN_FX}},
    /* -------------------------------------------------------- CLOUD (6) --- */
    {"CLOUD", GN_BANK(GE_GRAIN, 0, MORNING_FOG), 6, 0, 0, GN_SET({P_ATK, 15}, {P_E2, 76}, {P_E3, 100}, {P_LEVEL, 105}),
     {GN_GB({P_E7, 0, 87, 127, 11, P_E0, 2, 2, 3}), GN_FX}},
    {"FROZEN", GN_BANK(GE_GRAIN, 2, NORTHERN_AIR), 6, 0, 0, GN_SET({P_ATK, GN_SPEAK}, {P_E2, 76}, {P_E3, 100}, {P_LEVEL, 105}),
     {GN_GB(GB_GRAIN(0, 92)), GN_FX}},
    /* round 24: NOISE WIND and RAIN were "just noise" (the owner): PHASE STRING and ANALOG PWM STR, by
     * name (FACTS-SYNTH-1.0.md), as STRING PAD and WARM STRINGS; STRING's DCW below 50 is no darker, so its Bright
     * runs 50 .. 70 (rest) .. 127. With the engines retired they play bank sounds, and round CR (the rebind
     * picks, 2026-10-12, round UX-R's FIT) gives the two their own again: WIND is Paper Lantern (an airy triangle
     * pad with a breath of noise), RAIN is Rain Chime (a light chime whose glint and echo are the drops); the
     * engines-off build keeps round 24's Felucca strings */
    {"WIND", GN_FEL(GE_PHASE, 2, PAPER_LANTERN), 6, 0, 0, GN_SET({P_ATK, GN_SPEAK}, {P_LEVEL, 98}), {GN_AT(P_E2, 50, 70, 127, 8), GN_FX}},
    {"RAIN", GN_FEL(GE_ANALOG, 3, RAIN_CHIME), 6, 0, 0, GN_SET({P_ATK, 20}, {P_LEVEL, 101}), {GB_CUT(75), GN_FX}},
    {"SOFT FLUTE", GN_BANK(GE_WHEEL, 3, GLASS_WHISTLE), 6, 0, 0, GN_SET({P_LEVEL, 105}), {GB_WHEEL_TOP, GN_FX}},
    /* -------------------------------------------------------- PIANO (7) --- */
    /* the Piano: FM6 TINE EP, an electric piano. 1.0.1 cut SAMPLE's PIANO to 0.75 s a note, and its
     * tone control (CUT: a 6 dB slope from 900 Hz up to open, on a sample with little above 2 kHz)
     * moves it too little for a Bright a child hears (gn_sound_test, the PIANO set: +4.8 dB above
     * 2 kHz, the centroid 1.47 x); TINE EP rings, and its MLVL is a Bright no one misses */
    {"PIANO", GN_BANK(GE_FM6, 0, FELT_HAMMER), 7, 0, 0, GN_SET({P_LEVEL, 100}), {GN_LIN(P_E2, -64, 63, 8), GN_FX}},
    {"PLUCK", GN_BANK(GE_PHYS, 2, SNAP_PEA), 7, 0, 0, GN_SET({P_LEVEL, 127}), {GB_E2(72), GN_FX_RING_58}},
    {"HARP", GN_BANK(GE_PHYS, 8, HARP_DROPS), 7, 0, 0, GN_SET({P_LEVEL, 112}), {GB_E2(62), GN_FX}},
    /* (no nylon guitar among 1.0.1's presets: FM6 PLUCK, its patch NYLON PICK) */
    {"NYLON GUITAR", GN_BANK(GE_FM6, 7, SILK_PLUCK), 7, 0, 0, GN_SET({P_LEVEL, 104}), {GB_FM6, GN_FX}},
#if GN_ENGINES && GN_DROP_SAMPLES                    /* Gnomad (FORK.md): no sample block: on the Gnomad VA until re-voiced */
    {"SAX", GN_BANK(GE_PHYS, 0, REED_WHISTLE), 7, 0, 0, GN_SET({P_LEVEL, 120}), {GB_CUT(90), GN_FX}},
#else
    {"SAX", GE_SAMPLE, 3, 7, 0, 0, GN_SET({P_LEVEL, 120}), {GB_CUT(90), GN_FX}},
#endif
    {"BRASS", GN_BANK(GE_FM6, 3, BRASS_KITE), 7, 0, 0, GN_SET({P_LEVEL, 100}), {GB_FM6, GN_FX}},
    /* -------------------------------------------------------- FUNNY (8) --- */
    /* round CR: the ninth picture of ten, before the drums: the drums stay the last, one turn of
     * ALGORITHM back from the Gnome */
    GN_EASY_PIC_FUNNY(GN_FUNNY_ROW)
    /* -------------------------------------------------------- DRUMS (9) --- */
    /* Easy's drums: SAMPLE's PERC set (the General MIDI kit), upstream's own kit for a 0.9 project's drum
     * part (project.c proj_legacy_perc: SET PERC, LOOP off, CUT open, attack 0, decay and sustain full,
     * POLY) with a long release, Easy's level and sends, and no chorus. SAMPLE is not an engine of hits: a
     * released key would end a hit through the envelope, so it sustains in full and releases slower than
     * any of the kit's sounds lasts (each ends by itself, sample_render: a tap sounds as long as a held
     * key, and a voice is free when its sound ends). FORK.md gives the phone app the same recipe. */
#if GN_ENGINES && GN_DROP_SAMPLES                    /* Gnomad (FORK.md): no sample block: the kit on the Gnomad DRUMS */
    {"KIT", GN_KIT(GE_DRUM, 0, BIG_STEP), 9, 0, GNF_PADS, GN_SET({P_VOICE, V_POLY}, {P_CHOR, 0}, {P_LEVEL, 104}),
#else
    {"KIT", GE_SAMPLE, 0, 9, 0, GNF_PADS,
     GN_SET({P_E0, 4 /* GN_KIT_SET */}, {P_E3, 0}, {P_E4, 127}, {P_ATK, 0}, {P_DEC, 127}, {P_SUS, 127}, {P_REL, 120},
            {P_VOICE, V_POLY}, {P_CHOR, 0}, {P_LEVEL, 104}),
#endif
     /* pick 10 (the owner: "range higher in expression"): Bright opens the CUT up to rest and then drives the
      * sample's BITS (a crunch), Wobble swings three times as far, Space adds the chorus send above rest; Echo's send is
      * already at its top (a track's send cannot go past it) */
     {{P_E4, 0, 127, 127, 10, P_E2, 0, 0, 100}, GN_FX_KIT}},
    {"DRUM MACHINE", GN_KIT(GE_DRUM, 0, HOUSE), 9, 0, GNF_PADS, GN_SET({P_LEVEL, 101}),
     {GB_E2(70), GN_FX_MACHINE}},
    /* round EW: on the Gnomad DRUMS these two are kits too: their keys the kit's pads, as KIT's and DRUM MACHINE's */
    {"HAND DRUM", GN_KIT(GE_PHYS, 5, LATIN), 9, 0, GN_EO(GNF_PADS, 0), GN_SET({P_LEVEL, 104}), {GB_E2(76), GN_FX}},
    {"TOMS", GN_KIT(GE_PHYS, 6, WOODSHED), 9, 0, GN_EO(GNF_PADS, 0), GN_SET({P_LEVEL, 104}), {GB_E2(52), GN_FX}},
    /* (SLICE CHOP, the break cut into slices) */
    {"CHOPPED BREAK", GN_BANK(GE_SLICE, 0, WOODPILE), 9, 0, 0, GN_SET({P_LEVEL, 101}), {GB_TONE(90), GN_FX_AMP}},
};
#define GN_NSOUNDS (sizeof GN_SOUNDS / sizeof GN_SOUNDS[0])
#define GN_KIT_SET 4                      /* SAMPLE's SET: PERC (tools/gen_samples.py; gn_easy_test checks it) */
#define GN_NPICS 10u                      /* ALGORITHM turns through the ten pictures (round CR: FUNNY the tenth) */
#define GN_FUNNY 8u                       /* the ninth: FUNNY (round CR) */
#define GN_KIT 9u                         /* the tenth picture: the drums */
_Static_assert(GN_FUNNY == GN_FUNNY_PIC, "FUNNY's rows name their picture");
#define GN_OTHER 10u                      /* the picture of a track that holds none of Easy's */
#define GN_NOSND 0xFFu                    /* gn_easy_of: none of Easy's sounds */
/* each picture's first sound in GN_SOUNDS (the sounds are in their pictures' order), and the end */
static const uint8_t GN_GROUP[GN_NPICS + 1u] = {0, 4, 10, 16, 23, 28, 33, 38, 44, 44 + GN_EASY_PIC_FUNNY_COUNT,
                                                49 + GN_EASY_PIC_FUNNY_COUNT};
_Static_assert(GN_NSOUNDS == 49u + GN_EASY_PIC_FUNNY_COUNT && GN_NSOUNDS == 57u, "GN_GROUP: the sounds of each picture");
#define GN_SND_KIT 52u                    /* Easy's kit: the Drums picture's first sound (round CR: after FUNNY's 8) */
_Static_assert(GN_SND_KIT == 44u + GN_EASY_PIC_FUNNY_COUNT && GN_KIT_SET == 4, "the kit: the Drums picture's first sound, SET PERC");
#define GN_PIC_NAME_OF(p) (GN_SOUNDS[GN_GROUP[(p) % GN_NPICS]].name)
static const char *const GN_PIC_NAME[GN_NPICS] = {"GNOME", "ORGAN", "BELLS", "LASER", "WHALE", "CHOIR", "CLOUD",
                                                  "PIANO", "FUNNY", "DRUMS"};
static const uint8_t GN_PIC_FIELD[GN_NPICS] = {1, 1, 2, 3, 0, 1, 2, 3, 2, 0};   /* each picture's colour */

#if GN_ENGINES
/* Round FW-CONTENT-8 (FORK.md): the four tracks' sounds at power-on, chosen by bank id (gn_content.h GN_CONTENT_P<id>,
 * a kit K<id>: "slot, place in the engine's whole list"; an id the banks do not have authored does not compile), in
 * place of what TRK_DEF's preset positions find on the Gnomad slots (FM's preset 4 is whatever FM sound sits fifth in
 * bank order: bank 36). Put once a power-on, before anything can load a sound (gn_easy.c gn_power_on_sounds); every
 * other setting is felucca_init's. FORK.md gives each one's reason */
static const uint8_t GN_POWER_ON[NTRK][2] = {   /* (round CR: by name; ids at gnomad-core f16ae35 in the comments) */
    {GN_AUTHORED(GN_CONTENT_PN_ACID_LEAD)},       /* track 1: VA, Acid Lead (bank 141; 145 before 1.1.6) */
    {GN_AUTHORED(GN_CONTENT_PN_LAMPLIGHT)},       /* track 2: FM, Lamplight (bank 80; 72), Keys */
    {GN_AUTHORED(GN_CONTENT_PN_SING_AH)},         /* track 3: GNOME, Sing Ah (bank 272; 282), a lesson vowel */
    {GN_AUTHORED(GN_CONTENT_KN_BIG_STEP)},        /* track 4: DRUMS, kit Big Step (kit 0) */
};
_Static_assert(NTRK == 4, "GN_POWER_ON: a sound a track");
#define GN_BOOT_SOUNDS 32u                        /* gn.booted: the power-on sounds are put */
#endif

static const gn_sound_t *gn_sound_def(uint32_t i) { return i < GN_NSOUNDS ? &GN_SOUNDS[i] : 0; }
/* sound s's knob k (GK_*): its Bright, or its kind's Wobble, Echo or Space */
static const gn_knob_t *gn_snd_knob(const gn_sound_t *s, uint32_t k)
{
    k &= 3u;
    return k ? &GN_TAILS[s->kn.tail % GN_NTAILS][k - 1u] : &s->kn.bright;
}
static uint32_t gn_pic_of(uint32_t i) { return i < GN_NSOUNDS ? GN_SOUNDS[i].pic : GN_OTHER; }

/* the Gnome's syllables, one per white key in turn (VOWL to VOWL2 over TALK): wa, yo, oi, we,
 * ow, ya, ay, wo; the vowel positions A 0, E 32, I 64, O 95, U 127 */
static const uint8_t GN_SYLLABLE[8][2] = {{127, 0}, {64, 95}, {95, 64}, {127, 32}, {0, 127}, {64, 0}, {32, 64}, {127, 95}};
/* Easy's sixteen pads, pad 1 on the lowest white key: GM notes of the PERC set, only sounds a child
 * hears on the synth's own speaker (above 500 Hz a tom or a conga is far below the quietest pad, so
 * they have no pad; the kick stays, the beat's anchor). The snare and the closed hat get a second pad
 * (the same sound, for rolls with two fingers) and the wood block a higher one. */
static const uint8_t GN_PADS[16] = {
    36, 54, 38, 40,                       /* kick, tambourine, snare, snare 2 */
    37, 39, 42, 44,                       /* rim, clap, closed hat, pedal hat */
    46, 51, 56, 70,                       /* open hat, ride, cowbell, maracas */
    75, 76, 77, 49,                       /* claves, wood block, wood block (high), crash */
};
/* round UX-2: the pads' names, plain words, for the drum list, step entry and the kit (UX-3): Easy's sixteen
 * (GN_PADS, the PERC set's General MIDI sounds), and the Gnomad kit's sixteen stable identities (its pad i is MIDI
 * 36 + i: gn_core.c gnc_drum_keys; their roles: the core's SPEC/12 identity table) */
static const char *const GN_PAD_NAME[16] = {"KICK", "TAMBOURINE", "SNARE", "SNARE 2", "RIM", "CLAP", "CLOSED HAT",
                                            "PEDAL HAT", "OPEN HAT", "RIDE", "COWBELL", "MARACAS", "CLAVES", "WOOD",
                                            "HIGH WOOD", "CRASH"};
static const char *const GN_GC_PAD_NAME[16] = {"KICK", "SNARE", "CLAP", "CLOSED HAT", "OPEN HAT", "LOW TOM",
                                               "MID TOM", "HIGH TOM", "RIM", "COWBELL", "CLAVE", "SHAKER", "CONGA",
                                               "BONGO", "RIDE", "CRASH"};
/* round 13: the kit's eleven black keys, the upper row (F#3 .. F#5): the PERC set's sounds the pads leave out,
 * low to high (tools/gen_samples.py GM_KIT): the low toms (41 43 45), the high toms (47 48 50), the ride's bell
 * (53), the congas (62 63 64) and the shaker (69) */
static const uint8_t GN_BLACK_PADS[11] = {41, 43, 45, 47, 48, 50, 53, 62, 63, 64, 69};
#if GN_ENGINES
/* round EW-FIXES: a Gnomad kit has sixteen pads, all of them on the white keys, and the core plays a pad by its number
 * and the velocity only (gn_core.c gnc_key): no twelfth-to-27th sound to give the black keys. Each black key is a soft
 * hit (a ghost note: quieter, darker, shorter, the core's velocity law) of a drum beside it, chosen for grooves: KICK
 * SNARE CLOSED HAT OPEN HAT LOW TOM RIM COWBELL CLAVE SHAKER CONGA RIDE. Its note is the pad's in the ghost block
 * (gn_core.c GNC_GHOST: MIDI 4..19, below every General MIDI drum), so it never shares a note with a white key: it
 * records, plays back, rolls and leaves MIDI OUT as itself, and MIDI IN reaches it the same way */
static const uint8_t GN_GC_GHOST[11] = {4 + 0, 4 + 1, 4 + 3, 4 + 4, 4 + 5, 4 + 8, 4 + 9, 4 + 10, 4 + 11, 4 + 12, 4 + 14};
#endif
/* the place of each key among the sixteen white keys, the lower row (0 = F3 .. 15 = G5), 0xFF for a
 * black key of the upper row: seq.c's key_place of a white key (gn_easy_test checks all 27) */
static const uint8_t GN_WHITE[27] = {0, 0xFF, 1, 0xFF, 2, 0xFF, 3, 4, 0xFF, 5, 0xFF, 6, 7, 0xFF, 8, 0xFF, 9, 0xFF,
                                     10, 11, 0xFF, 12, 0xFF, 13, 14, 0xFF, 15};   /* ... D5 D#5 E5 F5 F#5 G5 */

/* the track settings a sound load keeps (ui.c param_kept) that Easy's sounds set: Felucca's SAVE-held
 * undo of a pick cannot put them back, so Easy keeps what each had just before Easy first wrote it and
 * puts that back on leaving (gn_easy.c gn_set_kept, gn_leave), unless anyone else has written it since */
static const uint8_t GN_KEPT[8] = {P_LEVEL, P_AMODE, P_ARATE, P_AOCT, P_AGATE, P_CHRD, P_QUANT, P_TRANS};

/* Easy's state. All of it starts at zero (power-on: Easy off, nothing Easy's), so it costs no flash
 * image (.bss, not .data). */
static struct {
    uint8_t on;                           /* Easy has the panel and the screen */
    uint8_t page;                         /* GP_*: the page the panel's buttons opened (HOME: play) */
    /* Easy's own sound on each track: GN_SOUNDS index or GN_KIT, + 1; 0 = none. Published only after
     * Easy's own load is complete, with the track's engine, preset and user slot right after it (own_*),
     * and cleared at every load of anyone else's (the app's sound, preset, project or backup: gn_editor;
     * Play's PRESETS; leaving), and for good at the first frame those three differ (gn_owner_check), or
     * the setting that picks the sound within its engine (gn_id_param: the kit's SET, an FM6 track's
     * PTCH) is no longer what Easy left there (own_id) */
    uint8_t own[NTRK], own_eng[NTRK], own_pre[NTRK], own_usr[NTRK];
    int16_t own_id[NTRK];
    uint8_t own_gen[NTRK];                /* an FM6 track's patch generation then (eng_fm6.c fm6_pgen): any patch
                                           * loaded since, even back to the same PTCH, is someone else's */
    uint8_t amount[NTRK][4];              /* Bright, Wobble, Echo, Space of each track: 0 .. GN_STEPS */
    int16_t wrote[NTRK][4];               /* what Easy last gave those settings (another writer: re-read) */
    /* Easy's claims to put a GN_KEPT setting back on leaving: bit k of kept_mask[t], with the value the
     * track had just before Easy first wrote it (kept_was: the baseline, read then, never at entering)
     * and what Easy last wrote (kept_wrote). A claim ends with any outside write of that setting, the
     * same value or not (the editor's, Play's knobs: gn_kept_retire), and every claim with a runtime
     * replacement (a project load, a backup put back: gn_editor) and with a project save (gn_project:
     * what was saved is what is kept); the next write of Easy's takes a new baseline */
    int16_t kept_was[NTRK][8];
    int16_t kept_wrote[NTRK][8];
    uint8_t kept_mask[NTRK];
    uint8_t last_sel;                     /* the track selected as Easy last saw it */
    uint8_t grv;                          /* the demo grooves (gn_grooves.c): the last loaded + 1, SAVE FIRST, the
                                           * list shown (a byte the struct's alignment left free) */
    uint32_t boot_t0, pick_t, saved_t;    /* ms | 1: entered, last ALGORITHM turn, a verified save */
    uint32_t peek_t;                      /* ms | 1: the last PRESETS turn (the peek list, 2 s) */
    uint32_t tap_t;                       /* ms | 1: the last EDIT tap (the tempo's big number, 2 s) */
    uint32_t save_t0;                     /* SAVE pressed at (ms | 1); bit 1: HOME joined, or the chord had it */
    uint8_t save_pend;                    /* SAVE asked for the song to be saved: done once the transport stops */
    uint8_t arm, arm_idx;                 /* REC: GA_* (gn_easy.c); the playhead's step at the last frame */
    uint8_t replace, punch;               /* K4: REPLACE (1) or KEEP; REC held: punching in */
    uint8_t take_on, take_trk;            /* a take is going on (its snapshot: the undo copy), on track take_trk */
    uint8_t take_idx;                     /* .. the step the playhead was in at the last frame (0xFF: none yet) */
    uint8_t crossed[NSTEP / 8];           /* .. the steps the playhead crossed while it recorded */
    uint8_t ttie[NSTEP / 8];              /* .. the steps it crossed while a key the take played was held */
    uint32_t take_tick;                   /* .. song.tick then (a START sets it back: a restart, not a wrap) */
    uint32_t take_keys;                   /* .. the keys it played (bit k) */
    uint8_t tlog_n, tlog_over;            /* .. what the keys played: the step each recorded into, its note (64 a
                                           * take; past that, tlog_over: a crossed step keeps every note) */
    uint8_t tlog[64][2];
    uint8_t prov_at, prov_div;            /* a provisional take's own snapshot (round 10: the undo copy stays the
                                           * last recording's): GN_PROV steps from step prov_at, DIV and LEN then */
    int16_t prov_len;
    step_t prov_step[18];                 /* (240 BPM, 1/32, the most swing: 400 ms and a frame enter 15) */
    uint8_t prov;                         /* REC down while playing, under GN_PUNCH_MS: a take that is a tap
                                           * until it is a punch (a tap puts the pattern back) */
    uint8_t gest_lock;                    /* the music was replaced mid-gesture: buttons do nothing till all up */
    uint8_t entry_put, entry_sel;         /* round 14: the entry's Gnome shown on track 1, not yet put there; the
                                           * track selected at the entry */
    uint8_t wob_mask;                     /* bit t: a claim to put back another sound's LFO rate and depth (the
                                           * fallback Wobble's), with the knob's place, the rate and depth then */
    uint8_t wob_start[NTRK];
    int16_t wob_rate[NTRK], wob_depth[NTRK];
    uint32_t entry_keys;                  /* .. the keys pressed for him (they sound once he is there) */
    uint32_t entry_sig;                   /* .. track 1 as the entry found it (ui.c track_sig) */
    uint8_t grid_set;                     /* bit t: the child set track t's BARS or SNAP (its grid is chosen) */
    uint16_t usb_suspend_ms;              /* low 16 bits of suspend onset; fits grid_set's existing padding */
    uint32_t mgen, undo_mgen;             /* the motion's generation (gn_editor: any command that may write it), and
                                           * the undo copy's (round 9) */
    uint32_t undo_msig;                   /* .. and a signature of the store as Easy left it */
    uint32_t cnt_t0, cnt_ms;              /* the count-in: since (ms | 1), how long */
    uint8_t cursor;                       /* the record screen, stopped: the step a key writes */
    uint8_t last_note;                    /* .. the note K2 puts back (the last a key wrote) */
    uint8_t undo_div;                     /* the recorded track's DIV before Easy's recording */
    motion_store_t undo_motion;           /* .. and the motion store (BARS and SNAP move motion: restored whole) */
    uint8_t saved_mine;                   /* the saved screen is the panel's SAVE (the Gnome says SAVED) */
    uint8_t auto_dirty;                   /* round 24: Easy wrote the loop since it was last saved (gn_dirty: 1);
                                           * round 25: 2, the tracks hold Easy's saved song (the header's mark) */
    uint16_t auto_t;                      /* .. the autosave not before then (fm1_ms / 16; saved_mine's padding) */
    uint32_t sw_t0;                       /* HOME + SAVE held since (ms | 1), 0 = not counting */
    uint8_t sw_done, sw_to_easy;          /* the hold switched; which way it goes */
    uint8_t sw_mark;                      /* bit 0 HOME, bit 1 SAVE: Easy marked this press (Felucca's t0 bit 1) */
    uint8_t adopt;                        /* round 24: the app's PRESET to adopt as Easy's: track << 6 | sound + 1
                                           * (gn_easy.c gn_adopt; sw_mark's padding byte: no RAM) */
    uint32_t rec_t0, home_t0;             /* REC and HOME presses in Easy (ui_input.c btn_hold) */
    uint32_t frame;                       /* ui.frame of the last per-frame upkeep */
    uint8_t undo_of;                      /* the track the undo copy is of, + 1; 0 = none */
    uint8_t click;                        /* round 26: the metronome's click, the audio ISR's own (gn_click,
                                           * below): blocks since it began (0 none), bit 7 the accent (undo_of's
                                           * padding byte: no RAM) */
    int16_t undo_len;
    uint32_t undo_mine, undo_rest;        /* the patterns as Easy's own recording left them: the recorded
                                           * track's, the other tracks' (a backstop: gn_editor drops the
                                           * copy before the app writes any step) */
    step_t undo[NSTEP];
    uint16_t pad_ms[16];                  /* drums: when each pad last sounded (ms | 1, 16 bits: 0 once 200 ms old) */
    uint32_t drum_age;                    /* drums: the last drum voice seen */
    uint8_t drawn;                        /* the screen is Easy's (also while it clears it on leaving) */
    uint8_t exit_y;                       /* leaving: the next row Easy clears to Felucca's ground */
    uint8_t booted;                       /* bits: boot (1), panic enabled (2), USB seen (4), timing (8), expired (16),
                                           * the power-on sounds put (32: GN_BOOT_SOUNDS) */
    /* ---- the UX rounds (FORK.md, "Round UX-1" on): appended, so the layout of rounds 7 to 23 stays byte for byte */
    uint16_t live_ms[NTRK];               /* Home: when each track last began a note (ms | 1, 16 bits) */
    uint32_t live_age[NTRK];              /* .. the newest voice seen on it */
    uint8_t drum;                         /* UX-2: the drum K2 picks (the drum list, step entry): its identity */
    uint8_t vbar;                         /* .. the steps page's bar, K1 (playing: the playhead's, 3 s after a turn) */
    uint32_t vbar_t;                      /* .. K1's last turn (ms | 1) */
    uint8_t bank;                         /* UX-3: the kit's pads shown, A (1-8) or B (9-16): OCT- / OCT+ on a kit */
    uint8_t shift;                        /* .. SHIFT: EDIT held GN_SHIFT_MS (a tap is still tap tempo) */
    uint8_t op;                           /* .. the FM page's operator (K1 under SHIFT), 0..5 */
    uint8_t tap_n;                        /* .. the tap tempo before EDIT's press, put back when it becomes SHIFT: */
    int16_t tap_bpm;                      /*    the taps counted, the tempo, the taps' times */
    uint32_t tap_was[4];
    uint32_t edit_t0;                     /* .. EDIT pressed at (ms | 1); 0: up */
    uint32_t music_sig;                   /* UX-5: the music as a saved project holds it, a fingerprint (gn_music_sig) */
    uint32_t saved_sig;                   /* .. as the last save read back from flash held it */
    uint8_t save_ok;                      /* .. there was one (else the music is only on the synth) */
    uint8_t save_note;                    /* round 27: a save to note as the last (GN_NOTE_*: gn_project_saved; save_ok's
                                           * padding byte: no RAM) */
    /* ---- round SEQ-1 (phase 1, FORK.md): appended */
    uint32_t klit_t[4];                   /* when knob k last turned (ms | 1): its card and what it changes lit */
    uint8_t steps_keys;                   /* the STEPS page's tap grid is live: a white key is a step (gn_key, ISR) */
    uint8_t steps_note;                   /* .. the note a white key sounds there (K2's drum, or the pen note) */
    uint8_t steps_cells;                  /* .. the places in the bar shown (8 or 16): keys past them rest */
    uint8_t hold_key;                     /* the white key held on STEPS: its place + 1 (0: none) */
    uint8_t hold_used;                    /* .. a knob turned while it was held (let go: the step stays) */
    uint8_t hold_off;                     /* .. it was pressed on a lit step: let go unturned, the step goes off */
    uint8_t tempo_hold;                   /* SELECT turned while EDIT is held: the tempo's big number, till EDIT is up */
    uint8_t lit_page;                     /* the page the lit knobs were turned on (another page: none lit) */
    /* ---- round FLOW-1 (the performance flow, FORK.md "Round FLOW-1"): appended */
    uint8_t tq;                           /* the transport queued for the next bar line (the audio ISR's, gn_click):
                                           * GN_TQ_RESTART (seq.c's transport_req 3: from the top) or GN_TQ_STOP (2) */
    uint8_t perf_n, perf_trk;             /* GLO held + keys: a fill (a roll); the keys rolling, and on which track */
    uint8_t perf_k[4], perf_note[4];      /* .. each key, the note it rolls */
    uint32_t perf_age;                    /* .. samples since the last hit (GN_PERF_NOW: hit now) */
    uint32_t play_t0, glo_t0;             /* PLAY (a tap stops when let go, held restarts), GLO (a tap: the volumes,
                                           * held: fills, + PLAY: stop at the bar's end): pressed at (btn_hold) */
    /* ---- round FLOW-1, M2 */
    uint32_t fx_t0;                       /* FX (a tap: track 1, held: an audition): pressed at (btn_hold) */
    uint8_t perf_roll, perf_new;          /* the keys held with GLO roll (a fill), with FX not (an audition); the keys
                                           * just added (bit i: perf_k[i]), each hit at once */
    uint8_t tile, tile_q, tile_ask;       /* GROOVES: the tile last loaded + 1; the tile queued (+ 1: it loads once the
                                           * loop stops on the bar line, gn.tq); the tile that asked PRESS AGAIN + 1 */
    uint8_t mine;                         /* .. the project slots that hold a song (bit s: MINE s + 1) */
    /* ---- round FLOW-1, M3 */
    uint8_t undo_kind, undid;             /* what the undo copy is for (GN_UK_*: its latest edit), and what SAVE held
                                           * last undid (GN_UK_NONE: nothing to undo), said in the band 2 s */
    uint32_t undid_t;                     /* .. since (ms | 1) */
} gn;
enum { GN_UK_NONE, GN_UK_TAKE, GN_UK_STEP, GN_UK_BARS, GN_UK_SNAP, GN_UK_STEPS };   /* gn.undo_kind (round FLOW-1, M3) */
#define GN_UK(k) ((k) << 4)                       /* gn_undo_take's force argument: bit 0 afresh, the kind above it */
#define GN_TQ_STOP 2u                             /* gn.tq: seq.c transport_req's stop */
#define GN_TQ_RESTART 3u                          /* .. and its restart from the top (GLO + PLAY in Advanced) */

#define GN_LOOP_DIRTY 1u                          /* gn.auto_dirty: Easy wrote the loop since it was saved */
#define GN_LOOP_SAVED 2u                          /* .. round 25: the tracks hold what Easy's slot holds (the header's
                                                   * mark, gn_screens.c), since a save or the power-on's load */
static void gn_undo_drop(void) { gn.undo_of = 0; }

/* the music was replaced or written by someone else (a project or the runtime backup loaded, the settings
 * restored, the editor's pattern writes): nothing of Easy's recording goes on into it. Armed, counting in,
 * recording, punching in, a SAVE waiting for the stop, a recording's held notes, the buttons under way (round
 * 9: they do nothing more until every button is up): all of it ends; the tracks it changed (bits) lose the
 * child's grid choice */
static void gn_rec_cancel(uint32_t tracks)
{
    uint32_t t;
    gn.arm = 0;
    gn.punch = 0;
    gn.prov = 0;                                      /* (a provisional take's music is gone too) */
    gn.take_on = 0;                                   /* (the take's music is gone: nothing to apply) */
    gn.save_pend = 0;
    gn.tq = gn.tile_q = 0;                            /* (round FLOW-1: no restart, stop or tile queued over it) */
    gn.grid_set &= (uint8_t)~tracks;                  /* (round 9: only the tracks it changed) */
    gn.gest_lock = (fm1_in.buttons & 0x3FFFu) != 0;   /* every button under way: nothing till all are up */
    gn.sw_t0 = 0;                                     /* (the HOME + SAVE hold too: no switch from it) */
    gn.rec_t0 = gn.rec_t0 ? gn.rec_t0 | 3u : 0u;      /* (and now: a release before the next frame acts not) */
    gn.save_t0 = gn.save_t0 ? gn.save_t0 | 3u : 0u;
    gn.home_t0 = gn.home_t0 ? gn.home_t0 | 3u : 0u;
    song.rec = 0;
#ifdef LED_PLAY_GREEN                                 /* (the UI's sources: Easy can be on; the firmware has them) */
    fm1_irq_off();
#endif
    for (t = 0; t < NTRK; t++)                        /* (no held note of a recording writes on release) */
        trk[t].rh_n = trk[t].rh_ties = trk[t].rskip_n = 0;
#ifdef LED_PLAY_GREEN
    fm1_irq_on();
#endif
}static void gn_disown(uint32_t t)
{
    gn.own[t % NTRK] = 0;
    gn.wob_mask &= (uint8_t)~(1u << (t % NTRK));     /* (a sound loaded: no claim to its LFO left) */
}
static void gn_disown_all(void)
{
    uint32_t t;
    for (t = 0; t < NTRK; t++)
        gn.own[t] = 0;
}

/* an outside write of setting id on track t (the same value or not): Easy no longer puts it back */
static void gn_kept_retire(uint32_t t, uint32_t id)
{
    uint32_t k;
    if (t >= NTRK)
        return;
    for (k = 0; k < NELEM(GN_KEPT); k++)
        if (GN_KEPT[k] == id)
            gn.kept_mask[t] &= (uint8_t)~(1u << k);
}

/* the GN_KEPT settings a track's motion can write (motion.c motion_param: of them, the level only): no
 * claim to them while a song plays (its projects' motion moves them, motion_step), and none is taken */
static int gn_kept_motion(uint32_t id) { return id == P_LEVEL; }
static void gn_kept_retire_motion(void)
{
    uint32_t t;
    for (t = 0; t < NTRK; t++)
        gn_kept_retire(t, P_LEVEL);
}

static void gn_kept_retire_all(void)              /* the tracks replaced (a project, a backup): no claim left */
{
    uint32_t t;
    for (t = 0; t < NTRK; t++)
        gn.kept_mask[t] = 0;
}

/* the setting of engine e that chooses a whole other sound, not a shade of it: SAMPLE's SET (the kit is
 * PERC), FM6's PTCH (the patch: a factory one, or a bank slot the app can rewrite); P_COUNT: none */
static uint32_t gn_id_param(uint32_t e)
{
    return e == GE_SAMPLE ? P_E0 : e == GE_FM6 ? P_E7 : P_COUNT;
}

/* Easy's own sound on track t, if t still holds it: its GN_SOUNDS index, else GN_NOSND. Read in the
 * audio ISR too (gn_key), so it writes nothing: a track whose engine, preset or user slot is not what
 * Easy left, or whose SET or PTCH (gn_id_param) is not, or (FM6) that has loaded a patch since, is
 * another sound at once, before the main loop clears its ownership for good */
static uint32_t gn_easy_of(const track_t *t)
{
    uint32_t i = (uint32_t)(t - trk) % NTRK, s = gn.own[i], id;
    if (!s || s > GN_NSOUNDS || t->eng_req != gn.own_eng[i] || t->preset != gn.own_pre[i] || t->user != gn.own_usr[i])
        return GN_NOSND;
    id = gn_id_param(t->eng_req);
    if (id < P_COUNT && t->p[id] != gn.own_id[i])
        return GN_NOSND;
    if (t->eng_req == GE_FM6 && fm6_pgen[i] != gn.own_gen[i])
        return GN_NOSND;
    if (s - 1u == GN_SND_KIT && GN_SOUNDS[GN_SND_KIT].engine == GE_SAMPLE && t->p[P_E0] != GN_KIT_SET)
        return GN_NOSND;
    return s - 1u;
}

/* round 14: what track p shows: the entry's Gnome on track 1 until he is used (gn_easy.c gn_entry_load), else
 * gn_easy_of */
static uint32_t gn_shown(uint32_t p)
{
    return gn.entry_put && p % NTRK == GN_MELODY ? GN_GROUP[0] : gn_easy_of(&trk[p % NTRK]);
}

/* seq.c rec_note: a note recorded on track t is a hit, never held (no tie into the steps after it), while t holds
 * Easy's kit (round 14: the hold stopped where the recording makes it, whatever the panel's frames do) */
static int gn_rec_hit(const struct track *t)
{
    uint32_t s;
    if (!gn.on)
        return 0;
    s = gn_easy_of(t);
    return s < GN_NSOUNDS && (GN_SOUNDS[s].flags & GNF_PADS);
}

/* ----------------------------------------------------------- the keys --- */
/* audio ISR, keyboard_block (seq.c): key k (0 = F3 .. 26 = G5) goes down on track t; note is what
 * Felucca's keys would play (kb_map). Returns the note to play, or GN_KEY_REST (seq.c KB_SILENT: the
 * key sends nothing, down or up). In Easy, a track that holds one of Easy's sounds: every key plays its own
 * note, chromatic (round 13: the black keys of the upper row too); the Gnome sets his syllable for the key
 * just before the note; a kit (Easy's, the drum machine: GNF_PADS): the n-th white key is pad n (GN_PADS),
 * the black keys the kit's other sounds (GN_BLACK_PADS). Any other sound: every key, chromatic. */
static uint32_t gn_black_pad(const track_t *t, uint32_t k)   /* black key k: its place */
{
#if GN_ENGINES
    if (gnc_engine(t->eng_req) == GC_DRUMS)           /* round EW-FIXES: on a Gnomad kit a ghost of a drum beside it */
        return GN_GC_GHOST[key_place(k) % 11u];       /* (round EW's LOW TOM .. CRASH were white keys 6 to 16 again) */
#endif
    (void)t;
    return GN_BLACK_PADS[key_place(k) % 11u];
}

/* round 13: Easy's keys are chromatic on every melodic track: another sound's keys as Felucca's with its
 * scale snap off (an engine's own key map, a GM kit or slices, as it is); its settings are not touched */
static uint32_t gn_chromatic(const track_t *t, uint32_t k)
{
    const engine_t *e = ENGINES[eng_idx(t->eng_req)];
    int32_t n = -1;
    if (e == &ENG_SAMPLE)                             /* (each called by name: the audio interrupt's stack) */
        n = sample_keys(t, k);
#if FELUCCA_SLICE
    else if (e == &ENG_SLICE)
        n = slice_keys(t, k);
#endif
    else if (e == &ENG_DRUM)
        n = drum_keys(t, k);
    if (n >= 0)
        return (uint32_t)n;
    return (uint32_t)clamp(53 + (int32_t)k + 12 * song.octave + t->p[P_TRANS], 0, 127);
}

/* round UX-3: white key w on a kit of Easy's: pad w. On the Gnomad DRUMS that is the kit's pad w itself (MIDI 36 + w:
 * gn_core.c gnc_drum_keys), so the sixteen white keys are its sixteen identities in order (banks A and B, the kit
 * view); on Felucca's engines Easy's sixteen pads (GN_PADS) */
static uint32_t gn_pad_note(const track_t *t, uint32_t w)
{
#if GN_ENGINES
    if (gnc_engine(t->eng_req) == GC_DRUMS)
        return 36u + (w & 15u);
#endif
    (void)t;
    return GN_PADS[w & 15u];
}

/* round FLOW-1: GLO held, a key is a fill: its note rolls on the track, a sixteenth at a time on the beat while both are
 * held (gn_perf, the audio ISR); FX held, it is an audition (M2): the note sounds as long as both are held. Either way
 * the key itself rests (KB_SILENT: seq.c records nothing, sends no MIDI, writes no step) and nothing is written to the
 * pattern: the notes go to the track's voices only (trk_note_on). Where the UI is not built in: none */
#ifdef LED_PLAY_GREEN
#define GN_HELD(label) ((fm1_in.buttons >> panel.btn[label]) & 1u)
#else
#define GN_HELD(label) 0u
#endif
#define GN_PERF_HELD() ((GN_HELD(B_GLO) || GN_HELD(B_FX)) && !gn.shift)
static uint32_t gn_perf_add(const track_t *t, uint32_t k, uint32_t note)
{
    uint32_t n = gn.perf_n, p = (uint32_t)(t - trk) % NTRK;
    if (note != GN_KEY_REST && n < 4u && (!n || gn.perf_trk == p)) {
        if (!n)
            gn.perf_roll = (uint8_t)GN_HELD(B_GLO);   /* (GLO: a fill; else FX: an audition) */
        gn.perf_trk = (uint8_t)p;
        gn.perf_k[n] = (uint8_t)k;
        gn.perf_note[n] = (uint8_t)note;
        gn.perf_new |= (uint8_t)(1u << n);
        gn.perf_n = (uint8_t)(n + 1u);
    }
    return GN_KEY_REST;
}

static uint32_t gn_key(track_t *t, uint32_t k, uint32_t note)
{
    uint32_t w, s, perf;
    if (!gn.on || k >= 27u)
        return note;
    gn.entry_keys &= ~(1u << k);                     /* round 16: a fresh press supersedes its queued entry press */
    if (gn.entry_put && t == &trk[GN_MELODY]) {       /* round 14: the entry's Gnome asked for: he sounds once */
        gn.entry_keys |= 1u << k;                     /* he is there (gn_entry_load, at once) */
        return GN_KEY_REST;
    }
    perf = GN_PERF_HELD();                            /* (round FLOW-1: a fill's key plays its sound, never a step) */
    if (!perf && gn.steps_keys && t == &trk[song.sel % NTRK])   /* round SEQ-1: STEPS, a white key is a step: it sounds */
        return GN_WHITE[k] < gn.steps_cells ? gn.steps_note : GN_KEY_REST;   /* K2's drum or the pen note */
    if (!perf && gn.page == GP_GROOVES && GN_WHITE[k] < 9u)
        return GN_KEY_REST;                           /* (round FLOW-1, M2: GROOVES, white keys 1 to 9 are the tiles) */
    if ((s = gn_easy_of(t)) >= GN_NSOUNDS)
        note = gn_chromatic(t, k);
    else if (w = GN_WHITE[k], GN_SOUNDS[s].flags & GNF_PADS)
        note = w == 0xFFu ? gn_black_pad(t, k) : gn_pad_note(t, w);
    else {
        if (w == 0xFFu)
            w = GN_WHITE[k - 1u];                      /* (a black key: its white neighbour's syllable) */
        /* the Gnome: VOWL, VOWL2 (0..127) on Felucca's VOICE. On the Gnomad GNOME (round EW) the core sings the key's
         * own syllable itself (P20: key mod the cycle), the note as it is: Easy's Gnome loads with TALK 12 (gn_sound_put) */
        if (!GN_DROP_VOICE && (GN_SOUNDS[s].flags & GNF_SYLL) && t->eng_req == GE_VOICE) {
            t->p[P_E0] = GN_SYLLABLE[w & 7u][0];
            t->p[P_E1] = GN_SYLLABLE[w & 7u][1];
        }
        note = (uint32_t)clamp(53 + (int32_t)k + 12 * (song.octave + GN_SOUNDS[s].octave), 0, 127);
    }
    return perf ? gn_perf_add(t, k, note) : note;
}

/* the audio ISR, once a block (gn_click): the keys of a fill or an audition. A key let go, or its button (GLO, FX), ends
 * its note; a key just added hits at once; a fill hits every key again each sixteenth of the beat (seq.c's beat, from
 * the loop's start: on the grid; a grid hit closer than half a sixteenth after the last waits) and ends each hit half a
 * sixteenth later (a melody's note rolls as a drum does); an audition holds its notes */
static void gn_perf(uint32_t n)
{
    track_t *t = &trk[gn.perf_trk % NTRK];
    uint32_t i, j = 0, q = beat_samples() / 4u, half = q / 2u, roll = gn.perf_roll, age = gn.perf_age, grid, fresh = 0;
    uint32_t held = (roll ? GN_HELD(B_GLO) : GN_HELD(B_FX)) && !gn.shift;
    for (i = 0; i < gn.perf_n; i++)
        if (held && ((fm1_in.notes >> gn.perf_k[i]) & 1u)) {
            fresh |= ((gn.perf_new >> i) & 1u) << j;
            gn.perf_k[j] = gn.perf_k[i];
            gn.perf_note[j++] = gn.perf_note[i];
        } else
            trk_note_off(t, gn.perf_note[i]);
    gn.perf_n = (uint8_t)j;
    gn.perf_new = 0;
    grid = roll && age >= half && beat_pos % (q ? q : 1u) < n;
    for (i = 0; i < j; i++)
        if (grid || ((fresh >> i) & 1u) || (roll && age < half && age + n >= half))
            trk_note_off(t, gn.perf_note[i]);
    for (i = 0; i < j; i++)
        if (grid || ((fresh >> i) & 1u))
            trk_note_on(t, gn.perf_note[i], 100);
    gn.perf_age = grid || fresh ? n : age + n;
}

/* ------------------------------------------------ the music, saved or not (round UX-5) --- */
#ifdef PROJ_NO_SLOT
/* a fingerprint of the music as a saved project holds it (project.c project_capture): the globals, each track's
 * settings as motion's base keeps them, its engine, preset, steps and FM6 patch, the motion store and the song's
 * chain; not the selected track nor the project's name (choosing a track changes no music). The status strip says
 * SAVED only while this is what the last save read back from flash held (gn_saved_now), else ON SYNTH */
static uint32_t gn_fnv(uint32_t h, const void *v, uint32_t n)
{
    const uint8_t *b = v;
    while (n--)
        h = (h ^ *b++) * 16777619u;
    return h;
}

static uint32_t gn_music_sig(void)
{
    uint32_t h = 2166136261u, t, id, f = motion_guard();
    h = gn_fnv(h, song.g, sizeof song.g);
    for (t = 0; t < NTRK; t++) {
        for (id = 0; id < P_COUNT; id++) {
            int16_t v = motion_base_value(&trk[t], id);
            h = gn_fnv(h, &v, sizeof v);
        }
        h = gn_fnv(h, &trk[t].eng_req, sizeof trk[t].eng_req);
        h = gn_fnv(h, &trk[t].preset, sizeof trk[t].preset);
        h = gn_fnv(h, trk[t].step, sizeof trk[t].step);
        h = gn_fnv(h, fm6_patch[t], sizeof fm6_patch[t]);
    }
    h = gn_fnv(h, &motion, sizeof motion);
    h = gn_fnv(h, &chain_config, sizeof chain_config);
    motion_unguard(f);
    return h;
}

/* a save was read back from flash the same (the panel's SAVE, the app's PROJECT save): the music now is saved */
static void gn_saved_now(void)
{
    gn.music_sig = gn.saved_sig = gn_music_sig();
    gn.save_ok = 1;
}
#endif

/* ------------------------------------------------ a save, verified --- */
#ifdef PROJ_NO_SLOT
/* project slot `slot` read back from flash is what the save left in RAM: it is on the synth and nothing
 * is being written (safe to unplug, 3g). Without flash nothing is verified */
static int gn_saved_verified(uint32_t slot)
{
#if FELUCCA_FLASH
    st_hdr_t h;
    return flash_ok && st_current(OBJ_PROJECT0 + (slot & 3u), &h) >= 0 && h.len == sizeof(project_store_t) &&
           !memcmp(st_buf, &proj_slot[slot & 3u], sizeof(project_store_t));
#else
    (void)slot;
    return 0;
#endif
}

/* Round 27 (F15, a report on V1.5): every project save on flash (project.c project_save_as: Advanced's SAVE, the app's,
 * Easy's own) is the last save; power-on restores it, in the mode it was saved from (gn_easy.c gn_boot). Easy's own
 * save writes its record itself (gn_save_now, gn_song_keep: GN_NOTE_EASY while it runs); any other is noted here and
 * its record written at the next input call, in either mode (gn_easy.c gn_last_keep: the record, the normal settings
 * save). GN_START_EASY 0 (upstream's tests): nothing */
#define GN_NOTE_SAVE 0x80u                        /* gn.save_note: a save to record, its slot in bits 0, 1 */
#define GN_NOTE_EASY 0x40u                        /* .. Easy's own save under way: its own record (gn_song_keep) */
#define GN_NOTE_ADV 0x20u                         /* .. saved in Advanced */
static void gn_project_saved(uint32_t slot)
{
    if (!GN_START_EASY || (gn.save_note & GN_NOTE_EASY))
        return;
    gn.save_note = (uint8_t)(GN_NOTE_SAVE | (gn.on ? 0u : GN_NOTE_ADV) | (slot & 3u));
}
#endif

/* ------------------------------------------------------- the editor --- */
#ifdef ED_HDR0
/* the editor's command cmd (editor.c ed_handle), before anything of it runs: what it may change is no
 * longer Easy's, by what the command does (editor.c, editor_backup.c, editor_fm6.c, editor_preferences.c):
 * - pat: a write of steps or of the pattern settings (STEP_SET, TRACK_STEP with a step, LEN DIV SWING
 *   GATE by SET or TRACK_PARAM), a track's motion, a song asked to play, a project loaded, the runtime
 *   backup put back (BACKUP_PUT's commit of object 0: project_restore_runtime) drops the undo copy,
 *   armed recording or not, held recorded note or not: "recording" is no proof of who wrote;
 * - snd: a sound load (PRESET, UP_LOAD, the engine by SET, the kit's SET or an FM6 track's PTCH by SET
 *   or TRACK_PARAM, a track's FM6 patch, an FM6 bank slot put or erased (the tracks on it reload it),
 *   a project, the runtime backup, the FM6 bank put back) ends Easy's ownership of the sound it
 *   reaches;
 * - kept: a write of one of the settings Easy puts back on leaving (GN_KEPT: by SET or TRACK_PARAM,
 *   the level by TRACK_MIX, or by a track's motion), the same value or not, ends Easy's claim to it on
 *   that track; a project or the runtime backup ends every claim, and so does a project save, once it
 *   has succeeded (gn_project, after the command).
 * Reads change none of them: INFO GET DUMP DESC STEP_GET NAMES SMP_INFO UP_LIST UP_GET WATCH PING TRACK
 * (a selection: gn_input) TRACK_DUMP UI_STATE UI_PALETTES FAV_GET BACKUP_LIST BACKUP_GET FM6_GET FM6_LIST,
 * TRACK_STEP TRACK_PARAM TRACK_MIX MOTION without a value, PROJECT's query, SONG's read (the editor sends
 * it on connecting and after every project save or load), the synth's own pushes (CHANGED RELOAD
 * STEP_CHANGED TRACK_CHANGED: ed_handle ignores them). Nor do the writes that touch no track: UI_SET
 * FAV_SET (preferences), UP_PUT UP_STORE UP_ERASE (the user bank), SMP_* (user samples), FM6_PUT and
 * FM6_ERASE of a bank slot no track plays, PROJECT's save, SONG's chain and stop, BACKUP_PUT's begin, chunks, abort and the commit of
 * every other object (settings, project slots, user banks). Any other command is taken as all three, on
 * every track. Easy off: nothing. */
/* the FM6 tracks whose patch is PTCH slot k (as PTCH has it, or as loaded: fm6_slot), as track bits */
static uint32_t gn_fm6_on_slot(uint32_t k)
{
    uint32_t t, m = 0;
    for (t = 0; t < NTRK; t++)
        if (trk[t].eng_req == GE_FM6 && ((uint32_t)trk[t].p[P_E7] == k || fm6_slot[t] == k))
            m |= 1u << t;
    return m;
}

#ifdef LED_PLAY_GREEN
static void gn_wob_retire(uint32_t t, uint32_t id);   /* gn_easy.c: the fallback Wobble's claim */
static uint32_t gn_adopt_of(uint32_t p, const uint8_t *a, uint32_t na);   /* gn_easy.c: round 24 */
static void gn_adopt(void);
#endif
#if GN_ENGINES
static void gn_power_on_sounds(void);                 /* gn_easy.c: round FW-CONTENT-8 */
#endif
static void gn_editor(uint32_t cmd, const uint8_t *a, uint32_t na)
{
    uint32_t pat = 1, snd = (1u << NTRK) - 1u, sel = 1u << (song.sel % NTRK), kept = 1, kt = 0, kid = P_COUNT;
    uint32_t settings = 0;                            /* (BACKUP_PUT's commit of the settings) */
    uint32_t ptrk = (1u << NTRK) - 1u;                /* the tracks a pattern write reaches */
#if GN_ENGINES
    gn_power_on_sounds();                             /* (a command before the first frame: they are there first) */
#endif
    if (!gn.on)
        return;
#ifdef LED_PLAY_GREEN
    if (gn.adopt)                                     /* round 24: the last PRESET adopted as Easy's pick, */
        gn_adopt();                                   /* before anything else of the app's runs */
#endif
    switch (cmd) {
    case ED_INFO: case ED_GET: case ED_DUMP: case ED_DESC: case ED_STEP_GET: case ED_NAMES: case ED_SMP_INFO:
    case ED_UP_LIST: case ED_UP_GET: case ED_WATCH: case ED_PING: case ED_TRACK_DUMP: case ED_TRACK:
    case ED_UI_STATE: case ED_UI_SET: case ED_UI_PALETTES: case ED_FAV_GET: case ED_FAV_SET:
    case ED_BACKUP_LIST: case ED_BACKUP_GET: case ED_UP_PUT: case ED_UP_STORE: case ED_UP_ERASE:
    case ED_SMP_BEGIN: case ED_SMP_WRITE: case ED_SMP_END: case ED_SMP_ERASE:   /* (user samples: no sound of Easy's) */
    case ED_CHANGED: case ED_RELOAD: case ED_STEP_CHANGED: case ED_TRACK_CHANGED:
    case ED_FM6_GET: case ED_FM6_LIST:
        pat = snd = kept = 0;
        RING_PUBLISH();
        return;                                       /* (reads: not even the motion's generation) */
    case ED_FM6_ERASE:                                /* bank slot: the FM6 tracks playing it hear it go */
        pat = kept = 0;
        snd = na >= 1u ? gn_fm6_on_slot(FM6_NFACTORY + a[0]) : 0u;
        break;
    case ED_SET:                                      /* scope, id, value: an engine, or SET / PTCH, is a sound */
        if (na < 2u || a[0] > 1u || a[1] >= (a[0] ? G_COUNT : P_COUNT))
            return;                                  /* round 17: rejected indices change no ownership */
        pat = na >= 2u && a[0] == 0u && a[1] >= P_SLEN && a[1] <= P_SGATE;
        ptrk = sel;
        snd = (na >= 2u && a[0] == 1u && a[1] == G_ENGSEL) ||
              (na >= 2u && a[0] == 0u && a[1] == gn_id_param(trk[song.sel % NTRK].eng_req)) ? sel : 0u;
        kept = 0;
        if (na >= 2u && a[0] == 0u) {
            kt = song.sel;
            kid = a[1];
        }
        break;
    case ED_TRACK_PARAM:                              /* track, id [, value] */
        if (na < 2u || a[0] >= NTRK || a[1] >= P_COUNT)
            return;
        pat = na > 2u && a[1] >= P_SLEN && a[1] <= P_SGATE;
        ptrk = 1u << a[0];
        snd = na > 2u && a[0] < NTRK && a[1] == gn_id_param(trk[a[0]].eng_req) ? 1u << a[0] : 0u;
        kept = 0;
        if (na > 2u) {
            kt = a[0];
            kid = a[1];
        }
        break;
    case ED_TRACK_MIX:                                /* track [, level, mute] */
        if (!na || a[0] >= NTRK)
            return;
        pat = snd = kept = 0;
        if (na >= 4u) {
            kt = a[0];
            kid = P_LEVEL;
        }
        break;
    case ED_TRACK_STEP:                               /* track, index [, step] */
        if (na < 2u || a[0] >= NTRK || a[1] >= NSTEP)
            return;
        pat = na > 2u;
        ptrk = 1u << a[0];
        snd = kept = 0;
        break;
    case ED_STEP_SET:
        if (!na || a[0] >= NSTEP)
            return;
        ptrk = sel;
        snd = kept = 0;
        break;
    case ED_MOTION:                                   /* track [, what ..]: a track's motion is its pattern's, */
        if (!na || a[0] >= NTRK)
            return;                                  /* ed_args_ok also checks the track before this hook */
        if (na > 1u && (a[1] == 3u || a[1] == 4u) &&
            (na < 4u || a[2] >= NSTEP || !motion_param(a[3])))
            return;                                  /* a rejected event cannot end the level or undo claim */
        pat = na > 1u;                                /* and it may move the level (motion.c motion_param) */
        ptrk = 1u << a[0];
        snd = kept = 0;
        if (na > 1u) {
            kt = a[0];
            kid = P_LEVEL;
        }
        break;
    case ED_PRESET:                                   /* (a sound load keeps the GN_KEPT settings: param_kept) */
    case ED_UP_LOAD:
        pat = kept = 0;
        snd = sel;
#ifdef LED_PLAY_GREEN
        if (cmd == ED_PRESET)                         /* round 24: one of Easy's 49 by engine and preset: */
            gn.adopt = (uint8_t)gn_adopt_of(song.sel % NTRK, a, na);   /* Easy's pick once it has loaded */
#endif
        break;
    case ED_PROJECT:                                  /* 0 load (every track), 1 save, 2 query */
        if (!na || a[0] != 0u)
            pat = snd = kept = 0;
        break;
    case ED_FM6_PUT:                                  /* a track's own patch, or a bank slot: the tracks on it */
        if (na >= 2u && a[0] == ED_FM6_TRACK && a[1] >= NTRK)
            return;                                  /* keep undo's motion generation too */
        pat = kept = 0;
        snd = na >= 2u && a[0] == ED_FM6_TRACK && a[1] < NTRK ? 1u << a[1] :
              na >= 2u && a[0] == ED_FM6_BANK ? gn_fm6_on_slot(FM6_NFACTORY + a[1]) : 0u;
        break;
    case ED_SONG:                                     /* 0 read, 1 the chain, 2 prepare and play, 3 stop */
        pat = na && a[0] == 2u;                       /* (a song plays its projects' patterns; their motion */
        snd = kept = 0;                               /* moves the level: once the song is prepared, chain_busy, */
        break;                                        /* gn_input ends the claims to it, before anything else) */
    case ED_BACKUP_PUT:                               /* op (0 begin, 1 chunk, 2 commit, 3 abort), object */
        settings = na >= 2u && a[0] == 2u && a[1] == 1u;
        if (na < 2u || a[0] != 2u || (a[1] != 0u && a[1] != 8u))
            pat = snd = kept = 0;
        else if (a[1] == 8u)                          /* the FM6 bank: the tracks on its patches reload */
            pat = kept = 0;
        break;                                        /* (object 0: the runtime replaced, all three) */
    default:                                          /* anything else: all three, on every track */
        break;
    }
    gn.mgen++;                                        /* (round 9: any of these may write or reset motion) */
    if (pat || kept || settings)
        gn_rec_cancel(kept || !pat ? 0u : ptrk);
    if (kept)
        gn.grid_set = 0;                              /* (every track replaced: no choice left) */
    if (pat) {
        gn_undo_drop();
        if (gn.auto_dirty == GN_LOOP_SAVED)           /* (round 25: the app writes the music: no longer the saved song) */
            gn.auto_dirty = 0;
    }
    for (cmd = 0; cmd < NTRK; cmd++)
        if ((snd >> cmd) & 1u)
            gn_disown(cmd);
    if (kept) {
        gn_kept_retire_all();
        gn.auto_dirty = 0;                            /* (round 24: the music replaced: nothing of Easy's to save) */
    } else if (kt < NTRK && kid < P_COUNT)
        gn_kept_retire(kt, kid);
#ifdef LED_PLAY_GREEN
    if (kt < NTRK && kid < P_COUNT)                   /* round 16: the editor rejects an out-of-range track */
        gn_wob_retire(kt, kid);                       /* (round 15: the app's LFO rate or depth is the app's) */
#endif
    RING_PUBLISH();                                   /* (stored before the command's own writes: the key hook) */
}

/* the editor's project command (editor.c ED_PROJECT), after it ran (a save that failed returned
 * before it). A save keeps what the person saved: every claim of Easy's to put a kept setting back ends
 * (the live tracks stay as saved when Easy is left; Easy's next write takes a new baseline). It is read
 * back from flash, and equal to what was saved means it is on the synth and nothing is being written:
 * safe to unplug (3g). Easy off: nothing. (A load: gn_editor, before.) */
static void gn_project(uint32_t op, uint32_t slot)
{
    if (!gn.on || op != 1u)
        return;
    gn_kept_retire_all();
    gn.auto_dirty = 0;                                /* (round 24: the app saved the music: not Easy's to save) */
    if (gn_saved_verified(slot)) {
        gn.saved_t = fm1_ms | 1u;
        gn.saved_mine = 0;
        gn_saved_now();                               /* (round UX-5: the status strip says SAVED) */
    }
}
#endif

/* Round 19: block-boundary panic in both product modes. booted's spare bits
 * retain activation (2) and the last USB configuration (4), with no RAM growth.
 * GN_START_EASY=0 keeps the upstream comparison build inert until Easy is entered. */
static int gn_panic_track(uint32_t p)
{
    uint32_t ch, note, k;
    if (!GN_START_EASY && !(gn.booted & 2u))
        return 0;
    for (ch = 0; ch < 16u; ch++)
        for (note = 0; note < 128u; note++)
            if ((midi_notes[ch][note] & GN_MIDI_TRACK_MASK) == p + 1u) {
                mchord_t *m = mchord_of(ch, note, p + 1u);
                uint32_t j, n = m ? m->n : 1u;
                for (j = 0; j < n; j++)
                    midi_out_event(0x08u | (0x80u | ch) << 8 |
                                   (uint32_t)(m ? m->note[j] : note) << 16);
            }
    for (k = 0; k < 27u; k++)
        if (kb_trk[k] == p && kb_chn[k]) {
            key_off(k, &trk[p]);
            kb_note[k] = KB_SILENT;
        }
    midi_silence_track(p);                 /* ownership, arp, mono, pending engine notes; one-block fade */
    trk[p].rskip_n = trk[p].rh_ties = 0;
    return 1;
}
/* One block-rate cleanup entry: the watch selects a scope without adding a
 * second call site to the IRQ loop. STOP always passes 0 (all tracks). */
static void gn_stop_notes(uint32_t usb_only)
{
    uint32_t p;
    if (usb_only) {
        gn_usb_notes_off();
        return;
    }
    for (p = 0; p < NTRK; p++) gn_panic_track(p);
}
/* Runtime ownership only: low three bits stay track+1; bits 3/4 record USB/TRS,
 * bits 5/6 record each source's pedal-held key-up; bit 7 remains upstream's pedal.
 * No added allocation. A same-channel/pitch USB and TRS pair shares one slot. */
#define GN_MIDI_USB 8u
#define GN_MIDI_TRS 16u
#define GN_MIDI_ORIGINS (GN_MIDI_USB | GN_MIDI_TRS)
static uint8_t gn_midi_pedal_bits(uint32_t value)
{
    uint32_t origins = value & GN_MIDI_ORIGINS;
    value &= ~MIDI_PEDAL_NOTE;
    if (origins && (value & (GN_MIDI_ORIGINS << 2)) == (origins << 2))
        value |= MIDI_PEDAL_NOTE;
    return (uint8_t)value;
}
static void gn_midi_event(uint32_t st, uint32_t ch, uint32_t d1, uint32_t d2, uint32_t source)
{
    uint32_t origin = source == 1u ? GN_MIDI_USB : GN_MIDI_TRS;
    uint32_t value = midi_notes[ch][d1], id = value & GN_MIDI_TRACK_MASK;
    if (!GN_START_EASY && !(gn.booted & 2u)) {
        midi_event(st, ch, d1, d2);
        return;
    }
    if (st == 0x90u && d2) {
        if (id == trk_index(midi_track(ch)) + 1u &&
            (value & GN_MIDI_ORIGINS & ~origin)) {
            midi_notes[ch][d1] = gn_midi_pedal_bits((value | origin) & ~(origin << 2));
            return;
        }
        midi_event(st, ch, d1, d2);
        if (midi_notes[ch][d1]) midi_notes[ch][d1] |= (uint8_t)origin;
        return;
    }
    if ((st == 0x80u || (st == 0x90u && !d2)) && (value & GN_MIDI_ORIGINS)) {
        if (!(value & origin)) return;               /* stale note-off from the other input */
        if (midi_channel(ch)->pedal && !drum_track(&trk[id - 1u]))
            value |= origin << 2;
        else
            value &= ~(origin | (origin << 2));
        if (value & GN_MIDI_ORIGINS)
            midi_notes[ch][d1] = gn_midi_pedal_bits(value);
        else
            midi_release(ch, d1);
        return;
    }
    if (st == 0xB0u && ((d1 == 64u && d2 < 64u) || d1 == 121u)) {
        uint32_t note;
        for (note = 0; note < 128u; note++) {
            value = midi_notes[ch][note];
            /* Physically held input keeps the shared slot on pedal-up. */
            if (!(value & MIDI_PEDAL_NOTE) && (value & (GN_MIDI_ORIGINS << 2)))
                midi_notes[ch][note] = (uint8_t)(value & ~((value & (GN_MIDI_ORIGINS << 2)) >> 2) &
                                                       ~(GN_MIDI_ORIGINS << 2));
        }
    }
    midi_event(st, ch, d1, d2);
}
static int gn_seq_held(const track_t *t, uint32_t note)
{
    uint32_t i;
    for (i = 0; i < t->seq_n; i++)
        if (t->seq_notes[i] == note) return 1;
    return 0;
}
/* Suspension cannot prove detach: retire only outstanding USB ownership.
 * Keep this cold channel/note/chord/voice scan out of the audio IRQ's block loop.
 * target_budget.py weights nested backward-branch spans, even on this rare path. */
static void gn_usb_notes_off(void)
{
    uint32_t ch, note;
    if (!GN_START_EASY && !(gn.booted & 2u)) return;
    for (ch = 0; ch < 16u; ch++)
        for (note = 0; note < 128u; note++) {
            uint32_t value = midi_notes[ch][note], id = value & GN_MIDI_TRACK_MASK;
            track_t *t;
            mchord_t *m;
            uint8_t nn[CHORD_MAX];
            uint32_t n, j, i;
            if (!(value & GN_MIDI_USB) || !id) continue;
            if (value & GN_MIDI_TRS) {
                midi_notes[ch][note] = gn_midi_pedal_bits(value & ~(GN_MIDI_USB | (GN_MIDI_USB << 2)));
                continue;
            }
            t = &trk[id - 1u];
            m = mchord_of(ch, note, id);
            n = m ? m->n : 1u;
            for (j = 0; j < n; j++) nn[j] = m ? m->note[j] : (uint8_t)note;
            if (m) m->id = 0;
            midi_notes[ch][note] = 0;
            midi_owners[id - 1u]--;
            if (!--midi_ch[ch].owned[id - 1u]) midi_ch[ch].targets &= (uint8_t)~(1u << (id - 1u));
            for (j = 0; j < n; j++) {
                uint32_t k = 0, arp = t->arp_note;
                if (midi_note_held(t, nn[j]) || midi_local_held(t, nn[j])) continue;
                /* Remove this root even with ARP HOLD, without dropping local roots. */
                rec_release(t, nn[j]);
                for (i = 0; i < t->nheld; i++)
                    if (t->held[i] != nn[j]) t->held[k++] = t->held[i];
                if (k != t->nheld && t->arp_phys) t->arp_phys--;
                t->nheld = (uint8_t)k;
                if (!gn_seq_held(t, nn[j])) {
                    trk_note_off(t, nn[j]);
                    for (i = 0; i < NVOICE; i++)
                        if (t->v[i].active && t->v[i].note == nn[j] && !t->v[i].gate) voice_kill(&t->v[i]);
                }
                if (arp && !t->nheld && !gn_seq_held(t, arp) && !midi_local_held(t, arp) && !midi_note_held(t, arp)) {
                    arp_off(t);
                    for (i = 0; i < NVOICE; i++)
                        if (t->v[i].active && t->v[i].note == arp && !t->v[i].gate) voice_kill(&t->v[i]);
                }
                midi_out_event(0x08u | (0x80u | ch) << 8 | (uint32_t)nn[j] << 16);
            }
        }
}
/* usb_poll leaves config set on unplug / frozen frames, but raises suspended
 * after about 100 ms. Qualify another 500 ms continuously suspended: short stalls
 * and suspend/resume cycles must not cut notes. This audio-block watcher runs far
 * more often than the 16-bit millisecond wrap; expired is latched until resume.
 * No driver changes, and no repeated panic while the bus remains suspended. */
#define GN_USB_SUSPEND_MS 500u
static void gn_usb_watch(void)
{
    uint32_t release = 0;                 /* stack/register only: 1 all, 2 USB only */
    if (!usb.config) {
        release = (gn.booted & 4u) ? 1u : 0u;
        gn.booted &= (uint8_t)~(4u | 8u | 16u);
    } else {
        gn.booted |= 4u;
        if (!usb.suspended) {
            gn.booted &= (uint8_t)~(8u | 16u);
        } else if (!(gn.booted & 8u)) {
            gn.usb_suspend_ms = (uint16_t)fm1_ms;
            gn.booted |= 8u;
        } else if (!(gn.booted & 16u) && (uint16_t)(fm1_ms - gn.usb_suspend_ms) > GN_USB_SUSPEND_MS) {
            gn.booted |= 16u;
            release = 2u;
        }
    }
    /* Same single call site as round 20, after the scope-selection branches.
     * No source-aware scan call in the timeout branch; no MIDI event deferral. */
    if (release)
        gn_stop_notes(release == 2u);
}

/* ------------------------------------------- the metronome (round 26) --- */
enum { GA_OFF, GA_READY, GA_COUNT };              /* gn.arm: not armed; armed (bar 1 or PLAY); counting in */
/* The mode byte (gn_easy.c, the mode, kept: pick 1) rides in favourites' row 15, byte 30; bits 7..4 Gnomad's marker,
 * bit 2 the metronome OFF (0, as every record before round 26 has it: ON). Here, as the audio ISR reads it; where
 * the UI (and so favourites) is not built in, no click */
#ifdef FELUCCA_FAVORITES
#define GN_MODE_BYTE (favorites.factory[15][30])
#define GN_MODE_MARK 0xA0u
#define GN_MODE_NOCLICK 4u                /* the recording's metronome off (0: on) */
#define GN_MODE_MARKED() ((GN_MODE_BYTE & 0xF0u) == GN_MODE_MARK)
static int gn_click_on(void) { return GN_MODE_MARKED() && !(GN_MODE_BYTE & GN_MODE_NOCLICK); }
#else
static int gn_click_on(void) { return 0; }
#endif

/* A click on each beat, the first of the bar accented, while the loop records (Easy's take, a punch, Felucca's own
 * recording in Advanced: song.rec), while Easy is armed and the loop plays (waiting for bar 1), and on each of
 * the count-in's four beats. The beat is the sequencer's own (seq.c beat_pos, beat_n: from the loop's start, the
 * tempo's or the external clock's), seen once a block in the audio ISR, so the click is on the beat to a block
 * (0.7 ms), not to a frame. It is synthesised (a 1,024-sample sine blip with a falling square envelope, a
 * parabola for the sine: no table, no sample, no voice of the four tracks') and added to the master output after
 * the USB audio tap: the codec hears it, the USB audio, the patterns and MIDI never do. One byte of state
 * (gn.click, padding). */
#define GN_CLICK_LEN 1024u                         /* samples (23 ms) */
#define GN_CLICK_HOLD 120u                         /* blocks before another may begin (87 ms; a beat at 240 BPM is
                                                    * 250 ms): a beat seen twice (an external clock's uneven
                                                    * advance) clicks once */
#define GN_CLICK_INC(hz) ((uint32_t)((hz) * 4294967296.0 / FS))
static int gn_click_due(uint32_t n)                /* a beat begins in this block */
{
    if (song.playing)
        return (song.rec || (gn.on && gn.arm)) && (beat_pos < n || song.tick == 1u);
    if (gn.on && gn.arm == GA_COUNT) {             /* (the count-in: beat_pos from its start, gn_play_btn) */
        uint32_t e = fm1_ms - (gn.cnt_t0 & ~1u);
        return beat_pos < n && e < gn.cnt_ms - gn.cnt_ms / 8u;   /* (its four beats, never a fifth) */
    }
    return 0;
}
/* round FLOW-1: the selected track enters a bar's first step in the next block (its own steps, swing and all: the
 * sequencer's tick, seq.c seq_tick): the queued restart or stop is asked for now, so the next block's events_block does
 * it before the tick, on the bar line itself: the bar's first step from the top, or none. The bar: four beats of the
 * track's DIV */
static void gn_tq_due(uint32_t n)
{
    const track_t *t = &trk[song.sel % NTRK];
    uint32_t per = div_samples((uint32_t)t->p[P_SDIV]), len = t->p[P_SLEN] > 0 ? (uint32_t)t->p[P_SLEN] : 1u;
    uint32_t spb = (4u * beat_samples() + per / 2u) / (per ? per : 1u);
    if (t->seq_pos < 0x7FFFFFFFu && t->seq_pos + n >= step_samples(t, per, t->seq_idx) &&
        !(((t->seq_idx + 1u) % len) % (spb ? spb : 1u))) {
        transport_req = gn.tq;
        gn.tq = 0;
    }
}
static void gn_click(int32_t *out, uint32_t n)
{
    uint32_t c = gn.click, i, e, inc, amp;
    if (gn.tq && song.playing && !transport_req)
        gn_tq_due(n);                              /* (round FLOW-1: a restart or a stop queued for the bar line) */
    if (gn.perf_n)
        gn_perf(n);                                /* (round FLOW-1: a fill's keys) */
    if (!c && gn_click_on() && gn_click_due(n))
        c = 1u | (beat_n == 0u ? 0x80u : 0u);
    if (!c)
        return;
    e = ((c & 0x7Fu) - 1u) * n;                    /* the click's sample at this block's first */
    gn.click = (uint8_t)((c & 0x7Fu) >= GN_CLICK_HOLD ? 0u : c + 1u);
    if (e >= GN_CLICK_LEN)
        return;
    inc = c & 0x80u ? GN_CLICK_INC(1568) : GN_CLICK_INC(1047);   /* G6 on the bar's 1, C6 on the others */
    amp = c & 0x80u ? 7000u : 4500u;
    for (i = 0; i < n && e + i < GN_CLICK_LEN; i++) {
        int32_t x = (int32_t)((e + i) * inc) >> 16;  /* the phase, Q15 of half a turn (-32768 .. 32767) */
        int32_t ax = x < 0 ? -x : x, d = (int32_t)(GN_CLICK_LEN - e - i);
        int32_t y = (x * (32768 - ax)) >> 13;      /* the sine's parabola, Q15 */
        int32_t v = ((y * (int32_t)amp >> 15) * d >> 10) * d >> 10;   /* (32-bit: no 64-bit helper on the chip) */
        out[2u * i] = clamp(out[2u * i] + v, -32768, 32767);
        out[2u * i + 1u] = clamp(out[2u * i + 1u] + v, -32768, 32767);
    }
}
