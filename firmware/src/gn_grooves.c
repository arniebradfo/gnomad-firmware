/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kyle Palaniuk */
/* Gnomad (FORK.md, Easy: demo grooves; decided 2026-10-11): five grooves in flash, read only, so a
 * new player hears what the synth can do. Each is all four tracks (a lead, chords, a bass, Easy's DRUM MACHINE), each track
 * one of Easy's own sounds (gn_state.c GN_SOUNDS), with its knobs, its level, its steps, LEN DIV SWING GATE, and
 * the groove's tempo. The music is ours, written for these sounds.
 *
 * Where: the loop page (SEQ, the circle): PRESETS turns through the grooves, the list shows 2 s (the peek list's
 * place; gn_screens.c). A detent loads the next groove and plays it from the top. Elsewhere PRESETS is the sound
 * inside the picture, as before. (The other way considered: a tenth picture "GROOVES" on ALGORITHM. Not taken: a
 * picture is one track's sound, a groove is four tracks' music; it would need its own art and change the nine
 * pictures every screen and test counts.)
 *
 * Whose music: a groove loads only over music that is not lost by it: every track empty or still the steps a
 * groove (or Felucca's SEQ > PATTERNS) put there (ui.c pat_sig: upstream's own "replace without asking"), or
 * the four tracks' steps exactly as one of the four project slots holds them (saved). Otherwise nothing changes
 * and the band says SAVE FIRST (SAVE). Loading writes no project slot: the four slots stay the user's, and a
 * groove saved with SAVE goes where Easy's SAVE always goes (Easy's own slot, else the first empty one).
 * Loading is Easy's own work, by Easy's rules: each track's sound is an Easy pick (gn_sound_put: Felucca's sound
 * load, Easy owns it), the knobs are Easy's (gn_knob_write), the level a kept write (gn_set_kept, put back on
 * leaving); the steps are the groove's, like a pattern load (pat_sig, as ui.c pat_load); a recording, its arming
 * and Easy's undo copy end (the music was replaced). With an external clock the tempo and the transport are the
 * clock's: the groove loads and waits for it. */
#if GN_DEMO_GROOVES
/* a step's velocity, by its class (the top two bits of an event's first byte) */
static const uint8_t GN_GV_VEL[4] = {52, 76, 100, 122};
/* An event is a byte: the step (0..63) and the velocity class (bits 6, 7); then its notes, a byte each, bit 7 set
 * on every note but the last of the step; then, on a melodic track, how many steps it sounds (1: this one; more:
 * the steps after it are TIEs). Easy's kit has no length (a hit). The events go up the steps. */
#define GV(st, v) (uint8_t)((st) | (v) << 6)
#define D1(st, v, a) GV(st, v), a
#define D2(st, v, a, b) GV(st, v), (a) | 0x80, b
#define N1(st, v, n, len) GV(st, v), n, len
#define N3(st, v, a, b, c, len) GV(st, v), (a) | 0x80, (b) | 0x80, c, len
/* Easy's sounds the grooves use (GN_SOUNDS indices; tests/gn_easy_test.c checks each by its name) */
enum { GVS_VOX = 1, GVS_ORGAN = 4, GVS_GLASS = 10, GVS_KALIMBA = 15, GVS_WHALE = 23, GVS_SUB = 24, GVS_SQUARE = 25,
       GVS_FMBASS = 26, GVS_FAT = 27, GVS_CHOIR = 28, GVS_SHIMMER = 31, GVS_FLUTE = 37, GVS_PIANO = 38, GVS_PLUCK = 39,
       GVS_NYLON = 41, GVS_DRUMS = GN_SND_KIT + 1u };   /* (round CR: the drums after FUNNY's sounds) */
/* General MIDI notes of Easy's pads (gn_state.c GN_PADS, GN_BLACK_PADS: every one a key of the kit), as the DRUM
 * engine plays them: 69 a closed hat 3 semitones up (the shaker's note), 76 a clave, 40 the snare 2 up */
enum { GVK_KICK = 36, GVK_SN = 38, GVK_SN2 = 40, GVK_RIM = 37, GVK_CLAP = 39, GVK_CH = 42, GVK_OH = 46, GVK_SHAK = 69,
       GVK_WOOD = 76 };

/* ---- 1. HEAD NOD: hip-hop, 90 BPM, 2 bars of 1/16, swung. D minor: Dm9 (a bar), Bbmaj7 (a bar) */
static const uint8_t GV1_LEAD[] = {   /* GLASS BELL, an octave lower and darker (v2), a slow answer with echo */
    N1(2, 2, 69, 2), N1(6, 1, 65, 2), N1(8, 2, 64, 4),
    N1(18, 2, 69, 2), N1(22, 1, 65, 2), N1(24, 2, 62, 6)};
static const uint8_t GV1_KEYS[] = {   /* the Piano (an electric piano): Dm9, Bbmaj7, chopped */
    N3(0, 2, 65, 69, 76, 7), N3(10, 1, 65, 69, 76, 4),
    N3(16, 2, 62, 65, 69, 7), N3(26, 1, 62, 65, 69, 2), N3(29, 0, 62, 65, 69, 3)};
static const uint8_t GV1_BASS[] = {   /* FM BASS with the kick */
    N1(0, 3, 38, 4), N1(7, 1, 38, 2), N1(10, 2, 33, 3), N1(14, 1, 36, 2),
    N1(16, 3, 34, 5), N1(22, 1, 34, 2), N1(26, 2, 41, 2), N1(28, 2, 33, 4)};
static const uint8_t GV1_KIT[] = {    /* boom-bap: the snare on 2 and 4, a late kick, the open hat to turn around */
    D2(0, 3, GVK_KICK, GVK_CH), D1(2, 1, GVK_CH), D2(4, 3, GVK_SN, GVK_CH), D1(6, 1, GVK_CH), D1(7, 1, GVK_KICK), D1(8, 2, GVK_CH), D2(10, 2, GVK_KICK, GVK_CH),
    D2(12, 3, GVK_SN, GVK_CH), D1(14, 1, GVK_CH), D1(15, 0, GVK_CH),
    D2(16, 3, GVK_KICK, GVK_CH), D1(18, 1, GVK_CH), D2(20, 3, GVK_SN, GVK_CH), D2(22, 1, GVK_KICK, GVK_CH), D1(24, 2, GVK_CH), D2(26, 2, GVK_KICK, GVK_CH),
    D1(27, 1, GVK_KICK), D2(28, 3, GVK_SN, GVK_CH), D1(30, 1, GVK_OH), D1(31, 0, GVK_SN)};

/* ---- 2. FLOOR FILLER: house, 124 BPM, 1 bar of 1/16. A minor: Am7 stabs, G over A to turn */
static const uint8_t GV2_LEAD[] = {   /* PLUCK, a hook between the stabs */
    N1(3, 2, 76, 1), N1(4, 1, 72, 1), N1(7, 2, 74, 2), N1(11, 1, 72, 1), N1(12, 2, 69, 2), N1(15, 1, 67, 1)};
static const uint8_t GV2_KEYS[] = {   /* the Organ's stabs, voiced high, out of the bass's and the pluck's way */
    N3(2, 2, 72, 76, 79, 1), N3(5, 2, 72, 76, 79, 2), N3(10, 1, 72, 76, 79, 1), N3(13, 2, 74, 79, 83, 2)};
static const uint8_t GV2_BASS[] = {   /* SQUARE BASS on the off-beats */
    N1(2, 3, 33, 2), N1(6, 2, 33, 2), N1(9, 1, 36, 1), N1(10, 3, 33, 2), N1(13, 1, 31, 1), N1(14, 2, 40, 2)};
static const uint8_t GV2_KIT[] = {    /* four on the floor, the clap on 2 and 4, open hats off the beat */
    D1(0, 3, GVK_KICK), D1(1, 0, GVK_CH), D1(2, 2, GVK_OH), D1(3, 1, GVK_CH), D2(4, 3, GVK_KICK, GVK_CLAP), D1(5, 0, GVK_CH), D1(6, 2, GVK_OH),
    D1(7, 1, GVK_CH), D1(8, 3, GVK_KICK), D1(9, 0, GVK_CH), D1(10, 2, GVK_OH), D1(11, 0, GVK_CH), D2(12, 3, GVK_KICK, GVK_CLAP), D1(13, 1, GVK_CH),
    D1(14, 2, GVK_OH), D1(15, 1, GVK_CH)};

/* ---- 3. RAINY DESK: lo-fi study, 78 BPM, 4 bars of 1/16, swung hard. C major: Fmaj7 Em7 Dm7 Cmaj7, a bar each */
static const uint8_t GV3_LEAD[] = {   /* SOFT FLUTE, a lazy line on the chords' tones */
    N1(4, 1, 76, 2), N1(6, 1, 79, 2), N1(8, 2, 81, 6),
    N1(20, 1, 79, 2), N1(22, 2, 76, 6),
    N1(36, 1, 77, 2), N1(38, 1, 76, 2), N1(40, 2, 74, 6),
    N1(52, 1, 71, 2), N1(54, 1, 72, 2), N1(56, 2, 76, 7)};
#define GV3_BAR(b, x, y, z) N3(16 * (b), 2, x, y, z, 6), N3(16 * (b) + 7, 1, x, y, z, 3), N3(16 * (b) + 11, 0, x, y, z, 4)
static const uint8_t GV3_KEYS[] = {   /* NYLON GUITAR, voiced close so each chord leads into the next */
    GV3_BAR(0, 64, 69, 72), GV3_BAR(1, 62, 67, 71), GV3_BAR(2, 60, 65, 69), GV3_BAR(3, 59, 64, 67)};
static const uint8_t GV3_BASS[] = {   /* SUB BASS: the roots, a step into the next */
    N1(0, 2, 41, 7), N1(10, 1, 41, 2), N1(14, 0, 43, 2),
    N1(16, 2, 40, 7), N1(26, 1, 40, 2), N1(30, 0, 41, 2),
    N1(32, 2, 38, 7), N1(42, 1, 38, 2), N1(46, 0, 43, 2),
    N1(48, 2, 36, 7), N1(58, 1, 36, 2), N1(62, 0, 40, 2)};
static const uint8_t GV3_KIT[] = {    /* two bars, again: a soft snare, a lazy kick, a rim to close */
    D2(0, 2, GVK_KICK, GVK_CH), D1(2, 0, GVK_CH), D2(4, 2, GVK_SN2, GVK_CH), D1(6, 0, GVK_CH), D1(8, 1, GVK_CH), D1(9, 1, GVK_KICK), D2(10, 2, GVK_KICK, GVK_CH),
    D2(12, 2, GVK_SN2, GVK_CH), D1(14, 0, GVK_CH),
    D2(16, 2, GVK_KICK, GVK_CH), D1(18, 0, GVK_CH), D2(20, 2, GVK_SN2, GVK_CH), D1(22, 0, GVK_CH), D1(24, 1, GVK_CH), D2(26, 1, GVK_KICK, GVK_CH),
    D1(27, 1, GVK_KICK), D2(28, 2, GVK_SN2, GVK_CH), D1(30, 0, GVK_CH), D1(31, 0, GVK_RIM)};

/* ---- 4. NIGHT BUS: drum and bass played half-time, 170 BPM, 2 bars of 1/16. F minor: Fm9, Dbmaj7 */
static const uint8_t GV4_LEAD[] = {   /* VOX LEAD, sung chops with echo */
    N1(3, 2, 72, 1), N1(6, 2, 75, 2), N1(14, 1, 72, 1), N1(19, 2, 70, 1), N1(22, 2, 72, 3), N1(28, 1, 68, 2)};
static const uint8_t GV4_KEYS[] = {   /* SHIMMER, a bar each */
    N3(0, 1, 60, 63, 67, 16), N3(16, 1, 60, 65, 68, 16)};
static const uint8_t GV4_BASS[] = {   /* FAT BASS, long and low */
    N1(0, 3, 41, 8), N1(10, 2, 41, 2), N1(12, 2, 44, 4), N1(16, 3, 37, 8), N1(26, 2, 37, 2), N1(28, 2, 39, 4)};
static const uint8_t GV4_KIT[] = {    /* the snare on 3, ghosts, a roll home */
    D2(0, 3, GVK_KICK, GVK_CH), D1(2, 1, GVK_CH), D1(4, 2, GVK_CH), D1(6, 1, GVK_CH), D2(8, 3, GVK_SN, GVK_CH), D2(10, 2, GVK_KICK, GVK_CH), D1(11, 1, GVK_KICK),
    D1(12, 2, GVK_CH), D1(13, 0, GVK_SN), D1(14, 1, GVK_CH), D1(15, 0, GVK_CH),
    D2(16, 3, GVK_KICK, GVK_CH), D1(18, 1, GVK_CH), D1(20, 2, GVK_CH), D2(22, 1, GVK_KICK, GVK_CH), D2(24, 3, GVK_SN, GVK_CH), D2(26, 2, GVK_KICK, GVK_CH),
    D1(28, 2, GVK_CH), D1(29, 0, GVK_SN), D1(30, 1, GVK_OH), D1(31, 1, GVK_SN)};

/* ---- 5. SKY GARDEN: dreamy, 100 BPM, 8 bars of 1/8. C major: Cmaj7 Am7 Fmaj7 G, two bars each */
#define GV5_TWINKLE(s, a, b, c, d, e) N1(16 * (s) + 2, 1, a, 1), N1(16 * (s) + 3, 1, b, 1), N1(16 * (s) + 5, 2, c, 2), \
                                      N1(16 * (s) + 10, 0, d, 1), N1(16 * (s) + 11, 0, e, 2)
static const uint8_t GV5_LEAD[] = {   /* KALIMBA, far away (echo and space) */
    GV5_TWINKLE(0, 76, 79, 83, 79, 76), GV5_TWINKLE(1, 76, 79, 84, 81, 79), GV5_TWINKLE(2, 76, 81, 84, 79, 77),
    GV5_TWINKLE(3, 74, 79, 83, 86, 79)};
static const uint8_t GV5_KEYS[] = {   /* the CHOIR holds each chord its two bars */
    N3(0, 1, 64, 67, 71, 16), N3(16, 1, 64, 67, 72, 16), N3(32, 1, 64, 69, 72, 16), N3(48, 1, 62, 67, 71, 16)};
static const uint8_t GV5_BASS[] = {   /* the WHALE sings the roots, gliding */
    N1(0, 2, 36, 14), N1(16, 2, 33, 14), N1(32, 2, 41, 14), N1(48, 2, 43, 14)};
static const uint8_t GV5_KIT[] = {    /* two bars, again: a soft kick, a rim, the shaker, a wood block */
    D1(0, 1, GVK_KICK), D1(2, 0, GVK_SHAK), D2(4, 1, GVK_RIM, GVK_SHAK), D1(6, 0, GVK_SHAK), D1(7, 0, GVK_SHAK),
    D1(8, 0, GVK_KICK), D1(10, 0, GVK_SHAK), D2(12, 1, GVK_RIM, GVK_SHAK), D1(14, 0, GVK_SHAK), D1(15, 0, GVK_WOOD)};
#undef GV
#undef D1
#undef D2
#undef N1
#undef N3
#undef GV3_BAR
#undef GV5_TWINKLE

typedef struct {
    uint8_t snd;                          /* Easy's sound (GN_SOUNDS) */
    int8_t level;                         /* added to the sound's own level (0.5 dB a step) */
    uint16_t knobs;                       /* Bright Wobble Echo Space, a nibble each: an amount 0..14, 15 the rest */
    uint8_t gate, tile;                   /* GATE; the first `tile` steps again to LEN (0: none) */
    uint8_t n;                            /* the events' bytes */
    const uint8_t *ev;
} gn_gvtrk_t;
typedef struct {
    const char *name;
    uint8_t bpm, div, len, swing;         /* the tempo; every track's DIV, LEN and SWING */
    gn_gvtrk_t t[NTRK];                   /* the lead (track 1), chords, bass, the drum machine (track 4) */
    uint8_t kit[4];                       /* the drum machine's TONE, DECY, SNAP (the hats' sizzle) and KICK */
#if GN_ENGINES
    uint8_t gkit;                         /* round EW: its Gnomad kit (a place in the kits' list, gn_content.h) */
#endif
} gn_groove_t;
/* round EW: each groove's own kit on the Gnomad DRUMS (Easy's DRUM MACHINE plays House alone): the kit a groove was
 * written for, by name (gn_content.h; a name the banks do not have does not compile) */
#if GN_ENGINES
#define GN_GV_KIT(name) , GN_GV_KIT_(GN_CONTENT_KN_##name)
#define GN_GV_KIT_(...) GN_GV_KIT__(__VA_ARGS__)
#define GN_GV_KIT__(slot, k) (k)
#else
#define GN_GV_KIT(name)
#endif
/* the knobs a groove sets: Bright, Echo and Space (Wobble at the sound's rest); KR: that one at rest too */
#define KN(b, e, s) (uint16_t)((b) | 0xF0u | (e) << 8 | (s) << 12)
#define KR 15
#define EV(a) sizeof(a), a
/* v2 (a listening test, 2026-10-11): the drums on the DRUM engine (Easy's DRUM MACHINE: synthesized hats, short and
 * bright; 1.0.1's PERC samples are 22 kHz with 0.5 s cymbals), each groove's kit shaped; more bass; levels so the
 * master limiter (fx.c LIM_T) rarely acts. Levels: each track measured solo (FORK.md, grooves) */
static const gn_groove_t GN_GROOVES[] = {
    {"HEAD NOD", 90, GN_DIV16, 32, 30,
     {{GVS_GLASS, -6, KN(3, 9, 7), 64, 0, EV(GV1_LEAD)}, {GVS_PIANO, -6, KN(KR, KR, 6), 64, 0, EV(GV1_KEYS)},
      {GVS_FMBASS, 6, KN(KR, 0, 0), 96, 0, EV(GV1_BASS)}, {GVS_DRUMS, -6, KN(KR, KR, KR), 64, 0, EV(GV1_KIT)}},
     {50, 64, 48, 1} GN_GV_KIT(BOOM_BAP)},
    {"FLOOR FILLER", 124, GN_DIV16, 16, 10,
     {{GVS_PLUCK, -4, KN(KR, 8, 6), 64, 0, EV(GV2_LEAD)}, {GVS_ORGAN, -10, KN(KR, KR, 5), 64, 0, EV(GV2_KEYS)},
      {GVS_SQUARE, 10, KN(3, 0, 0), 80, 0, EV(GV2_BASS)}, {GVS_DRUMS, -6, KN(KR, KR, KR), 64, 0, EV(GV2_KIT)}},
     {64, 52, 52, 0} GN_GV_KIT(HOUSE)},
    {"RAINY DESK", 78, GN_DIV16, 64, 40,
     {{GVS_FLUTE, -2, KN(KR, 6, 8), 64, 0, EV(GV3_LEAD)}, {GVS_NYLON, 2, KN(KR, KR, 6), 64, 0, EV(GV3_KEYS)},
      {GVS_SUB, -6, KN(KR, 0, 0), 100, 0, EV(GV3_BASS)}, {GVS_DRUMS, -6, KN(KR, KR, KR), 64, 32, EV(GV3_KIT)}},
     {40, 56, 40, 1} GN_GV_KIT(HUSH)},
    {"NIGHT BUS", 170, GN_DIV16, 32, 0,
     {{GVS_VOX, -2, KN(KR, 9, 8), 64, 0, EV(GV4_LEAD)}, {GVS_SHIMMER, -8, KN(KR, KR, 10), 120, 0, EV(GV4_KEYS)},
      {GVS_FAT, 12, KN(KR, 0, 0), 110, 0, EV(GV4_BASS)}, {GVS_DRUMS, -6, KN(KR, KR, KR), 64, 0, EV(GV4_KIT)}},
     {58, 50, 48, 0} GN_GV_KIT(NIGHT_BUS)},
    {"SKY GARDEN", 100, GN_DIV8, 64, 0,
     {{GVS_KALIMBA, 6, KN(KR, 11, 11), 64, 0, EV(GV5_LEAD)}, {GVS_CHOIR, -6, KN(KR, KR, 10), 120, 0, EV(GV5_KEYS)},
      {GVS_WHALE, 4, KN(KR, 0, KR), 120, 0, EV(GV5_BASS)}, {GVS_DRUMS, -4, KN(KR, KR, KR), 64, 16, EV(GV5_KIT)}},
     {45, 72, 40, 1} GN_GV_KIT(PEBBLE)},
};
#undef KN
#undef KR
#undef EV
#define GN_NGROOVES (sizeof GN_GROOVES / sizeof GN_GROOVES[0])
_Static_assert(GN_NGROOVES >= 1u && GN_NGROOVES <= 7u, "gn.grv keeps a groove + 1 in three bits");
#define GN_GV_NUM 7u                      /* gn.grv: the groove last loaded + 1 (0: none) */
#define GN_GV_NO 0x40u                    /* .. the last turn was refused: SAVE FIRST */
#define GN_GV_LIST 0x80u                  /* .. the list is the one PRESETS shows (the loop page) */

#if GN_ENGINES
/* gn/ux catch-up (FW-P0): the grooves' drums are written in General MIDI notes for Felucca's DRUM engine (its lanes,
 * dsp drum_lane). With Felucca's engines retired Easy's DRUM MACHINE is a Gnomad kit (gn_state.c GN_KIT: the core's
 * DRUMS), whose pad i is MIDI 36 + i (gn_core.c gnc_drum_keys; gn_state.c GN_GC_PAD_NAME): each groove note goes to
 * the identity that plays its role (the snare 2 a snare, the wood block the clave, the shaker the shaker) */
static uint32_t gn_gv_pad(uint32_t n)
{
    switch (n) {
    case 36: return 36;                               /* KICK */
    case 38: case 40: return 37;                      /* SNARE (and the snare 2) */
    case 39: return 38;                               /* CLAP */
    case 42: case 44: return 39;                      /* CLOSED HAT (and the pedal hat) */
    case 46: return 40;                               /* OPEN HAT */
    case 37: return 44;                               /* RIM */
    case 56: return 45;                               /* COWBELL */
    case 75: case 76: case 77: return 46;             /* CLAVE (the claves, the wood blocks) */
    case 54: case 69: case 70: return 47;             /* SHAKER (the tambourine, the shaker, the maracas) */
    default: return n;
    }
}
#endif

/* groove g's steps of track p into st[NSTEP]: the events, then the first `tile` steps again up to LEN; the
 * rest empty, as a cleared step */
static void gn_gv_steps(const gn_groove_t *G, uint32_t p, step_t *st)
{
    const gn_gvtrk_t *T = &G->t[p % NTRK];
    uint32_t i = 0, j, len = G->len, tile = T->tile ? T->tile : len;
    int hit = (GN_SOUNDS[T->snd].flags & GNF_PADS) != 0;
#if GN_ENGINES
    int pad = hit && gn_gnomad_kit(p);                /* (the drum machine on the Gnomad kit: its identities) */
#endif
    memset(st, 0, NSTEP * sizeof *st);                /* (no byte of whatever was there before) */
    for (j = 0; j < NSTEP; j++)
        st[j].time = ST_REST;                         /* (as ui.c step_clear leaves a step) */
    while (i < T->n) {
        uint32_t a = T->ev[i++], at = a & 63u, hold = 1, b;
        step_t *s = &st[at];
        s->time = ST_NOTE;
        s->vel = GN_GV_VEL[a >> 6];
        do {
            b = i < T->n ? T->ev[i++] : 0u;
            if (s->n < 4u)
#if GN_ENGINES
                s->note[s->n++] = (uint8_t)(pad ? gn_gv_pad(b & 127u) : b & 127u);
#else
                s->note[s->n++] = (uint8_t)(b & 127u);
#endif
        } while (b & 0x80u);
        if (!hit && i < T->n)
            hold = T->ev[i++];
        for (j = 1; j < hold && at + j < NSTEP; j++)
            st[at + j].time = ST_TIE;
    }
    for (j = tile; j < len && j < NSTEP; j++)
        st[j] = st[j - tile];
}

/* a groove may load: nothing of the player's music is lost by it (see the top) */
static int gn_gv_free(void)
{
    uint32_t p, s;
    for (p = 0; p < NTRK && (seq_is_empty(&trk[p]) || steps_sig(&trk[p]) == pat_sig[p]); p++)
        ;
    if (p == NTRK)
        return 1;
    for (s = 0; s < 4u; s++) {                        /* saved: a slot holds these very steps */
        if (!proj_import(&proj_scratch, &proj_slot[s], sizeof(project_store_t)))
            continue;
        for (p = 0; p < NTRK && !memcmp(proj_scratch.t[p].step, trk[p].step, sizeof trk[p].step); p++)
            ;
        if (p == NTRK)
            return 1;
    }
    return 0;
}

/* groove g onto the four tracks, by Easy's own hand (see the top), and from the top */
static void gn_gv_load(uint32_t g)
{
    const gn_groove_t *G = &GN_GROOVES[g % GN_NGROOVES];
    uint32_t p, k;
    gn_rec_cancel((1u << NTRK) - 1u);                 /* (the music replaced: no arming, take or save waits) */
    gn_undo_drop();
    gn.auto_dirty = 0;                                /* (round 25: a groove is new music, not the saved song) */
#ifdef FELUCCA_FAVORITES
    if (GN_SONG_BYTE) {                               /* a groove is a new song: Easy's last one stays in its slot, */
        GN_SONG_BYTE = 0;                             /* the player's, and the next SAVE takes an empty one (the */
        settings_save();                              /* record written with the next settings save) */
    }
#endif
    for (p = 0; p < NTRK; p++) {
        const gn_gvtrk_t *T = &G->t[p];
        track_t *t = &trk[p];
        gn_sound_put(p, T->snd);
        for (k = 0; k < 4u; k++)
            if (((T->knobs >> (4u * k)) & 15u) != 15u) {
                gn.amount[p][k] = (uint8_t)((T->knobs >> (4u * k)) & 15u);
                gn_knob_write(p, k);
            }
        if (T->level)
            gn_set_kept(t, P_LEVEL, t->p[P_LEVEL] + T->level);
#if GN_ENGINES
        if (p == NTRK - 1u && gn_gnomad_kit(p)) {     /* round EW: the groove's own kit, and its shape on EDIT 2 (TONE */
            uint32_t j;                               /* DECAY SNAP from Felucca's 64-centred three, KICK as the kit's) */
            gn_preset_put(t, G->gkit);
            gn.own_pre[p] = t->preset;                /* (still Easy's DRUM MACHINE: its kit the groove's) */
            for (j = 0; j < 3u; j++)
                t->p[P_E4 + j] = (int16_t)clamp((int32_t)G->kit[j] - 64, -64, 63);
            t->p[P_E7] = 0;
        }
        if (p == NTRK - 1u && !gn_gnomad_kit(p)) {    /* the drum machine's shape (TONE: the Bright bubble follows); */
#else                                                 /* a Gnomad kit reads only BRIT and WOBL: its own shape, as is */
        if (p == NTRK - 1u) {                         /* the drum machine's shape (TONE: the Bright bubble follows) */
#endif
            static const uint8_t KIT_ID[4] = {P_E2, P_E3, P_E4, P_E6};
            for (k = 0; k < 4u; k++)
                gn_set(t, KIT_ID[k], G->kit[k]);
        }
        gn_gv_steps(G, p, proj_scratch.t[p].step);    /* (main-loop scratch, as project.c's) */
        fm1_irq_off();
        memcpy(t->step, proj_scratch.t[p].step, sizeof t->step);
        t->p[P_SLEN] = G->len;
        t->p[P_SDIV] = G->div;
        t->p[P_SSWING] = G->swing;
        t->p[P_SGATE] = T->gate;
        t->rh_n = t->rh_ties = t->rskip_n = 0;
        fm1_irq_on();
        pat_sig[p] = steps_sig(t);                    /* (a pattern load's: the next replaces it without asking) */
        pat_last[p] = 0;
    }
    if (!song.g[G_CLOCK]) {
        song.g[G_BPM] = (int16_t)clamp(G->bpm, GP[G_BPM].min, GP[G_BPM].max);
        transport_req = song.playing ? 3u : 1u;      /* (from the top: a restart, or a start) */
    }
}

/* PRESETS on the loop page: the next groove (or the one before), shown in the list for 2 s; refused (the band says
 * SAVE FIRST) while a song plays or while it would lose the player's music */
static void gn_gv_turn(int32_t s)
{
    uint32_t cur = gn.grv & GN_GV_NUM;
    int32_t base = cur ? (int32_t)cur - 1 : (s > 0 ? -1 : 0), n = (int32_t)GN_NGROOVES;
    gn.peek_t = fm1_ms | 1u;
    if (chain_busy() || !gn_gv_free()) {
        gn.grv = (uint8_t)(cur | GN_GV_LIST | GN_GV_NO);
        return;
    }
    base = ((base + s) % n + n) % n;
    gn_gv_load((uint32_t)base);
    gn.grv = (uint8_t)(((uint32_t)base + 1u) | GN_GV_LIST);
}

/* round FLOW-1, M2: the GROOVES page's nine tiles (spec 4.7, 6.4, 7): keys 1 to 5 (the first white keys) the factory
 * grooves, 6 to 9 the project slots, MINE 1 to 4. Playing, a key queues its tile for the next bar line: the loop stops
 * on it (gn.tq, the audio ISR) and the tile loads and plays from the top at once (gn_tile_load, the next input pass);
 * the same key again before the bar: now. Stopped: it loads and plays now. Never over music not kept (spec 180-182,
 * S-10): stopped, Easy keeps it first (its SAVE, silently: gn_save_now; FULL or NOT SAVED: it asks); playing, the tile
 * asks first (STOP, OR PRESS AGAIN), and a second press loads anyway. An empty MINE says so. A song playing: nothing */
#define GN_NTILES (GN_NGROOVES + 4u)
static void gn_tile_load(void)
{
    uint32_t i = gn.tile_q - 1u;
    gn.tile_q = 0;
    if (i < GN_NGROOVES) {
        gn_gv_load(i);                                /* (Easy's picks, steps, tempo; it starts the loop) */
        gn.grv = (uint8_t)(i + 1u);
    } else {
        gn_song_boot(i - GN_NGROOVES);                /* (project_load, each Easy sound Easy's again: round 25) */
        if (proj_cur != i - GN_NGROOVES)
            return;
        if (!song.g[G_CLOCK])
            transport_req = 1;
    }
    gn.tile = (uint8_t)(i + 1u);
}
static void gn_tile_press(uint32_t i)
{
    uint32_t q = i + 1u;
    if (chain_busy())
        return;
    if (i >= GN_NGROOVES && !project_used(i - GN_NGROOVES)) {
        gn.tile_ask = (uint8_t)(0x40u | q);           /* (EMPTY) */
        return;
    }
    if (song.playing && gn.tile_q == q) {             /* again before the bar: now */
        gn.tq = 0;
        transport_req = 2;
        return;
    }
    if (!gn_gv_free() && (song.playing || !((1u << gn_save_now(1)) & 0x12u)) && gn.tile_ask != q) {
        gn.tile_ask = (uint8_t)q;                     /* (not kept: asks; a second press loads anyway; kept: SAVED, 1, */
        return;                                       /* or 4, on flash without the power-on's record) */
    }
    gn.tile_ask = 0;
    gn.tile_q = (uint8_t)q;
    if (!song.playing || song.g[G_CLOCK])
        gn_tile_load();                               /* (stopped, or the clock's transport: at once) */
    else
        gn.tq = GN_TQ_STOP;                           /* (the bar line: the loop stops, then it loads) */
}
static void gn_tiles_mine(void)                   /* the slots that hold a song (the page's MINE tiles) */
{
    uint32_t k;
    gn.mine = 0;
    for (k = 0; k < 4u; k++)
        gn.mine |= (uint8_t)(project_used(k) << k);
}
static void gn_tiles_keys(uint32_t notes)
{
    uint32_t k;
    for (k = 0; notes && k < 27u; k++)
        if (((notes >> k) & 1u) && GN_WHITE[k] < GN_NTILES)
            gn_tile_press(GN_WHITE[k]);
}
#endif
