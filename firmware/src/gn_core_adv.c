/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kyle Palaniuk */
/* Gnomad (FORK.md, round EW): the Advanced pages on the Gnomad engines. Included by gn_core.c
 * after the main loop's plan (gnh); built only with GN_ENGINES.
 *
 * Until this round the core heard only Bright, Wobble, Echo, Space, Chorus, the level and MUTE from a track (and a
 * sound's own knobs, round MF): every other Advanced page was drawn and changed nothing. Now each page's knob reaches
 * the core parameter that does what the page says, or the page says in plain words that this engine has none (the
 * skin's card, gn_skin.c; params.c page_desc gives such a knob no descriptor, so it is inert), and a page with nothing
 * at all to do on the track's engine is not offered (ui.c page_visible: SLICER, MOD; ENV DEST, LFO DEST where the
 * engine has none of theirs). Nothing new is stored: every setting is the track's or the song's own, as before, in the
 * same project bytes; no editor-protocol command is added.
 *
 *   ENV        Attack, Decay, Sustain, Release: the core's P1..P4 (VA, GNOME, SAMPLER); FM: its six operators'
 *              envelopes, attacks moved by the knob's travel, decays, sustains and releases scaled by it; DRUMS: none
 *              (each drum fades by itself: EDIT 2's DECAY)
 *   ENV DEST   Filter: VA's filter envelope (P20); Pitch: VA's pitch envelope (P22), the Gnome's scoop (P18)
 *   LFO        Speed: the engine's motion rate (VA P21, FM P15, GNOME P16); the core's motion is a sine from each
 *              note's start, never faded: WAVE, START and FADE say so
 *   LFO DEST   Pitch: how far Wobble bends the pitch (VA, GNOME P23); Volume: the Gnome's growl (P15)
 *   FX         Chorus, Delay, Reverb: the sends, as before; Drive: no drive per part on the core (said)
 *   DLY        Time: the echo's time (0x304) on the song's tempo, the division as Felucca's (halved until it fits the
 *              core's 1 s); Repeat: its feedback (0x305); Mix: how much of every part's Delay send reaches the echo
 *              (90 = as sent); Colour: the core's echo has no tone (said)
 *   REVERB     Size: the plate's size and damping (0x307, CHANGES-1.2.1-fx's law; 90 = the core's 2.17 s); Type and
 *              Damping: one plate, its damping follows the size (said)
 *   CHORUS     Speed (0x309) and Depth (0x306)
 *   PAN        the core is mono (SPEC/01 D03): no pan anywhere on this build (VOICE 2, MIXER, Easy's SHIFT)
 *   EDIT 1     the sound's four knobs as Home and Easy have them, named as its record names them
 *   EDIT 2     four more of the engine's own, each a move from the sound as authored (0 = the sound itself):
 *              VA cutoff, resonance, detune, sub; FM index, feedback, the modulators' ratio, touch; GNOME vowel,
 *              size, breath, talk (its syllables, voice 2's P20); DRUMS tone, decay, snap (snare, clap, hats, rim)
 *              and the kick's pitch; SAMPLER start, end, loop, pitch
 * A control a knob of the sound's own already moves (round MF: its writes reach the same core parameter) is that
 * knob's: the card says "Knob n has it" and nothing is sent from the page.
 *
 * Rest (the absolute pages: ENV, ENV DEST, LFO, LFO DEST): at its rest place a knob sends nothing, so the core plays
 * the record's own value exactly; elsewhere it sends the page's own law (the value the card shows: Felucca's tables).
 * The rest place is the place whose value comes nearest the record's. A track's settings get those places once, when
 * they are not yet this firmware's (P_ED_FX, a setting nothing else reads, is the mark: GNA_MARK), on the main loop:
 * after any sound load (apply_preset_to puts every setting at its default, the mark among them) and on a project
 * saved before this round, which therefore keeps its sound. EDIT 2 needs no rest: 0, its default, is the sound.
 *
 * Where it runs: the main loop works out each track's wanted record (the bound record with the pages' changes) and
 * posts the words that differ from what was sent, one batch a track at a time (a mailbox, as round MF's moves); the
 * interrupt only copies a batch into the block, its record words as one group at one offset (SPEC/01: one patch group
 * and one kit group a track a block), after the track's macro move. The master controls likewise. Nothing here runs
 * in the interrupt but the copy. */

/* the absolute controls (track settings, their rest places managed) */
enum { GNA_ATK, GNA_DEC, GNA_SUS, GNA_REL, GNA_FENV, GNA_PENV, GNA_LRATE, GNA_LPIT, GNA_LAMP, GNA_N };
static const uint8_t GNA_ID[GNA_N] = {P_ATK, P_DEC, P_SUS, P_REL, P_ED_FLT, P_ED_PIT, P_LRATE, P_LD_PIT, P_LD_AMP};
static const int8_t GNA_LO[GNA_N] = {0, 0, 0, 0, -64, -64, 0, -64, 0};
static const int8_t GNA_HI[GNA_N] = {127, 127, 127, 127, 63, 63, 127, 63, 127};
#define GNA_OPS 0xFFu                  /* FM: its six operators' envelopes */
/* the core parameter control c reaches on each engine (gc_engine order), 0 = none */
static const uint8_t GNA_P[5][GNA_N] = {
    {1, 2, 3, 4, 20, 22, 21, 23, 0},                    /* VA */
    {GNA_OPS, GNA_OPS, GNA_OPS, GNA_OPS, 0, 0, 15, 0, 0},   /* FM */
    {0, 0, 0, 0, 0, 0, 0, 0, 0},                        /* DRUMS */
    {1, 2, 3, 4, 0, 18, 16, 23, 15},                    /* GNOME */
    {1, 2, 3, 4, 0, 0, 0, 0, 0},                        /* SAMPLER */
};
#define GNA_MARK 1                     /* P_ED_FX: the track's Advanced settings are at this firmware's rest places */
/* round EW-FIXES: the mark names the record its rest places are for (preset p: p + 1, wrapped into P_ED_FX's -64..63;
 * preset 0 is GNA_MARK itself), so a user preset (upreset.c up_load: it stores every setting, the mark among them)
 * binds the record it was saved from again; GNA_MARK on another preset is a track settled before this round's (its
 * record is the track's own preset: a project keeps it) */
#define GNA_MARK_OF(p) ((int16_t)((((p) + 65u) & 127u) - 64))
#define GNA_PG 8u                      /* a batch: at most 8 record words (a move's group and this one share an offset) and */
#define GNA_POST 16u                   /* P1..P4 streaming; or a kit's 16 pads' field (one EDIT 2 knob of DRUMS) */
enum { GNM_ETIME, GNM_EFB, GNM_CDEPTH, GNM_RSIZE, GNM_CRATE, GNM_N };
static const uint16_t GNM_ID[GNM_N] = {0x304, 0x305, 0x306, 0x307, 0x309};
static const int32_t GNM_BOOT[GNM_N] = {16538, 24576, 88, 49152, 35};   /* gn_core_boot's (SPEC/20 defaults) */
static struct {
    uint32_t bfor[NTRK];               /* the bind the caches below are for */
    int8_t rest[NTRK][GNA_N];          /* each absolute control's rest place */
    uint16_t w[NTRK][48];              /* the record's words as the core has them (the bind's, then what was sent) */
    int8_t ke[NTRK][4];                /* a kit's EDIT 2 as the core has it (its pads' fields from the kit's own) */
    /* the mailbox (one writer each way, as gnh's): the main loop writes m, gp, gk, mfor, then pend; the interrupt
     * copies them and clears pend */
    volatile uint8_t pend[NTRK];
    uint8_t gp[NTRK], gk[NTRK];        /* the batch's record group (first), its kit group (next); the rest stream */
    uint32_t mfor[NTRK];
    gc_macro_write m[NTRK][GNA_POST];
    /* the master controls: as sent (the main loop's), the batch, and DLY's MIX for the interrupt's Echo sends */
    int32_t ms[GNM_N];                 /* (as sent less the boot's: 0 at power-on) */
    volatile uint8_t mpend;
    gc_macro_write mm[GNM_N];
} gna;

static const uint32_t GNA_POW16[16] = {65536, 68438, 71468, 74632, 77936, 81386, 84990, 88752,
                                       92682, 96785, 101070, 105545, 110218, 115098, 120194, 125515};
/* x * 2^(e / 16), rounded (e -64..63: four octaves down to nearly four up) */
__attribute__((noinline)) static uint32_t gna_oct16(uint32_t x, int32_t e)
{
    int32_t q = e >= 0 ? e / 16 : -((15 - e) / 16);
    uint64_t y = (uint64_t)x * GNA_POW16[(uint32_t)(e - 16 * q)];
    return q >= 0 ? (uint32_t)(((y << q) + 32768u) >> 16) : (uint32_t)(((y >> -q) + 32768u) >> 16);
}
static int32_t gna_clamp(int32_t v, int32_t lo, int32_t hi) { return v < lo ? lo : v > hi ? hi : v; }

/* the signed parameters of an engine (their wire words two's complement) */
static int gna_signed(int32_t ce, uint32_t p)
{
    return p == 10u || (ce == GC_VA && (p == 20u || p == 22u)) ||
           (ce == GC_GNOME && (p == 13u || p == 18u || p == 22u));
}
__attribute__((noinline)) static int32_t gna_dec(int32_t ce, uint32_t p, uint32_t w) { return gna_signed(ce, p) ? (int32_t)(int16_t)w : (int32_t)w; }

/* control c's law: place v -> the core's semantic value (the value the page's card shows) */
static int32_t gna_ms(int32_t v, int32_t max)
{
    int32_t m = (int32_t)((TIME_MS_X10[v & 127] + 5u) / 10u);
    return m > max ? max : m;
}
__attribute__((noinline)) static int32_t gna_law(int32_t ce, uint32_t c, int32_t v)
{
    switch (c) {
    case GNA_ATK: return gna_ms(v, 5000);
    case GNA_DEC: case GNA_REL: return gna_ms(v, 10000);
    case GNA_SUS: case GNA_LAMP: return (v * 65535 + 63) / 127;
    case GNA_FENV: return v * 75;                       /* cents: 16 places an octave, four octaves */
    case GNA_PENV: return ce == GC_GNOME ? v * 25 / 8 : v * 75;   /* (the Gnome's scoop: +-200 cents) */
    case GNA_LRATE: return gna_clamp((int32_t)LFO_HZ_X100[v & 127], 10, ce == GC_GNOME ? 800 : 1200);
    default: return v <= 0 ? 1 : (v * 1200 + 31) / 63; /* GNA_LPIT: cents at full Wobble (1: none) */
    }
}

/* the record's own value of control c (the longest of FM's operators; a depth of 0 is the engine's own law) */
static uint32_t gna_word(const uint8_t *rec, uint32_t p) { return rd16(&rec[32u + 2u * p]); }
static int32_t gna_stored(int32_t ce, uint32_t c, const uint8_t *rec)
{
    uint32_t p = GNA_P[ce][c], j, m = 0, w;
    if (p != GNA_OPS) {
        int32_t s = gna_dec(ce, p, gna_word(rec, p));
        return c == GNA_LPIT && !s ? (ce == GC_GNOME ? 20 : 25) : s;
    }
    for (j = 0; j < 6u; j++) {
        uint32_t b = 18u + 5u * j;
        w = c == GNA_ATK ? gna_word(rec, b + 2u) & 0x1FFFu : c == GNA_DEC ? gna_word(rec, b + 3u) & 0x3FFFu :
            c == GNA_SUS ? (gna_word(rec, b + 4u) & 255u) * 257u : gna_word(rec, b + 4u) >> 8;
        if (c == GNA_REL)
            w = w ? w * 40u : gna_word(rec, 4u);
        m = w > m ? w : m;
    }
    return (int32_t)m;
}
/* the place whose law comes nearest the record's own (the lower of two as near) */
static int32_t gna_rest(int32_t ce, uint32_t c, const uint8_t *rec)
{
    int32_t v, best = GNA_LO[c], s = gna_stored(ce, c, rec);
    uint32_t near = 0xFFFFFFFFu;
    for (v = GNA_LO[c]; v <= GNA_HI[c]; v++) {
        int32_t d = gna_law(ce, c, v) - s;
        uint32_t a = (uint32_t)(d < 0 ? -d : d);
        if (a < near) {
            near = a;
            best = v;
        }
    }
    return best;
}

/* the core parameters (bits) control c reaches on engine ce: a knob of the sound's own reaching one owns it */
#define GNA_B(p) ((uint64_t)1 << (p))
static uint64_t gna_reach(int32_t ce, uint32_t c)
{
    uint32_t p = GNA_P[ce][c], j;
    uint64_t m = 0;
    if (p != GNA_OPS)
        return p ? GNA_B(p) : 0;
    for (j = 0; j < 6u; j++)
        m |= GNA_B(18u + 5u * j + (c == GNA_ATK ? 2u : c == GNA_DEC ? 3u : 4u));
    return c == GNA_REL ? m | GNA_B(4) : m;
}
/* EDIT 2's four on each engine (DRUMS: the kit's fields, gna_want): the parameter each moves; a place's step (the step
 * x mul >> sh; GNA_OCT: 16 places an octave; GNA_SET: the place itself; GNA_OWN: worked out in gna_want) and its range */
enum { GNA_OCT = 0, GNA_SET = -1, GNA_OWN = -2 };
static const struct gna_e2 { uint8_t p, sh; int16_t mul, lo; uint16_t hi; } GNA_E2[5][4] = {
    {{18, 0, GNA_OCT, 20, 11000}, {19, 0, 1024, 0, 65535}, {14, 6, 50, 0, 50}, {16, 0, 1024, 0, 65535}},   /* VA */
    {{14, 0, 32, 0, 2048}, {13, 0, 1024, 0, 65535}, {18, 0, GNA_OWN, 0, 0}, {16, 0, 1024, 0, 65535}},     /* FM */
    {{0, 0, 0, 0, 0}, {0, 0, 0, 0, 0}, {0, 0, 0, 0, 0}, {0, 0, 0, 0, 0}},                                 /* DRUMS */
    {{12, 0, 16, 0, 1024}, {13, 0, 19, -1200, 1200}, {14, 0, 512, 0, 32768}, {20, 0, GNA_SET, 0, 12}},     /* GNOME */
    {{16, 0, GNA_OWN, 0, 0}, {17, 0, GNA_OWN, 0, 0}, {13, 0, GNA_OWN, 0, 0}, {10, 0, 100, -2400, 2400}}}; /* SAMPLER */
/* the parameters EDIT 2's knob s reaches (FM's ratio: every operator's pitch word; START, END: both) */
static uint64_t gna_reach2(int32_t ce, uint32_t s)
{
    if (ce == GC_FM && s == 2u)
        return GNA_B(18) | GNA_B(23) | GNA_B(28) | GNA_B(33) | GNA_B(38) | GNA_B(43);
    if (ce == GC_SAMPLE && s < 2u)
        return GNA_B(16) | GNA_B(17);
    return ce == GC_DRUMS ? 0 : GNA_B(GNA_E2[ce][s & 3u].p);
}
/* FM's operator words the ENV page moves (SPEC/11, base 18 + 5j): attack +2 bits 0..12, decay +3 bits 0..13, sustain
 * +4 bits 0..7, release code +4 bits 8..15 (x 40 ms; 0: P4's) */
static const struct { uint8_t off, sh; uint16_t mask, max; } GNA_OPF[4] = {
    {2, 0, 0x1FFF, 5000}, {3, 0, 0x3FFF, 10000}, {4, 0, 0x00FF, 255}, {4, 8, 0xFF00, 255}};
/* the FM algorithm's carriers (SPEC/11, our G6 graphs: bit j = operator A..F) */
static const uint8_t GNA_CARR[8] = {0x20, 0x20, 0x30, 0x20, 0x30, 0x30, 0x30, 0x38};
#define GNA_FRAMES(id, off, frames, crc) a == (id) ? (frames) :
static uint32_t gna_asset_frames(uint32_t a) { return GN_CONTENT_ASSETS(GNA_FRAMES) 0u; }
#undef GNA_FRAMES

/* the record words w (from the record) track t's pages make, on engine ce (not DRUMS: gna_kit); owned: the parameters
 * the sound's own knobs reach (left as recorded) */
__attribute__((noinline)) static void gna_want(const track_t *t, int32_t ce, const uint8_t *rec, uint32_t ti, uint64_t owned,
                                               uint16_t w[48])
{
    uint32_t c, j, p;
    int32_t e[4];
    const struct gna_e2 *x;
    for (p = 0; p < 48u; p++)
        w[p] = (uint16_t)gna_word(rec, p);
    for (j = 0; j < 4u; j++)
        e[j] = t->p[P_E4 + j];
    for (c = 0; c < GNA_N; c++) {                       /* the absolute pages */
        int32_t v = t->p[GNA_ID[c]], r = gna.rest[ti][c], L, L0;
        p = GNA_P[ce][c];
        if (!p || v == r || (gna_reach(ce, c) & owned))
            continue;
        L = gna_law(ce, c, v);
        if (p != GNA_OPS) {
            w[p] = (uint16_t)L;
            continue;
        }
        L0 = gna_law(ce, c, r);                         /* FM: the operators moved by the knob's travel */
        for (j = 0; j < 6u; j++) {
            uint16_t *wd = &w[18u + 5u * j + GNA_OPF[c].off];
            int32_t x = (*wd & GNA_OPF[c].mask) >> GNA_OPF[c].sh, lo = 0;
            if (c == GNA_ATK)                           /* (an attack moved, the rest scaled; a sustain of 0 set) */
                x = x + L - L0;
            else if (x || c == GNA_SUS) {
                lo = c == GNA_REL;                      /* (a release code stays a code: 0 is P4's) */
                x = L0 ? (int32_t)((uint32_t)x * (uint32_t)L / (uint32_t)L0) : L * 255 / 65535;
            }
            *wd = (uint16_t)((*wd & ~GNA_OPF[c].mask) | (uint32_t)gna_clamp(x, lo, GNA_OPF[c].max) << GNA_OPF[c].sh);
        }
        if (c == GNA_REL)
            w[4] = (uint16_t)gna_clamp((int32_t)(gna_word(rec, 4u) * (uint32_t)L / (uint32_t)L0), 0, 10000);
    }
    for (j = 0; j < 4u; j++) {                          /* EDIT 2: moves from the sound as authored */
        if (!e[j] || (gna_reach2(ce, j) & owned))
            continue;
        x = &GNA_E2[ce][j];
        if (x->mul != GNA_OWN) {
            int32_t v = gna_dec(ce, x->p, w[x->p]);
            v = x->mul == GNA_OCT ? (int32_t)gna_oct16((uint32_t)v, e[j]) : x->mul == GNA_SET ? e[j]
                                                                         : v + ((e[j] * x->mul) >> x->sh);
            w[x->p] = (uint16_t)gna_clamp(v, x->lo, x->hi);
        } else if (ce == GC_FM) {                       /* the modulators' ratio (ratio mode), 16 places an octave */
            uint32_t carr = GNA_CARR[w[12] & 7u];
            for (p = 0; p < 6u; p++) {
                uint32_t b = 18u + 5u * p, code = w[b] & 0x1FFFu;
                if ((carr >> p) & 1u || (w[b + 2u] & 0x8000u))
                    continue;
                code = (uint32_t)gna_clamp((int32_t)gna_oct16(code ? code : 8192u, e[j]), 1, 8192);
                w[b] = (uint16_t)((w[b] & 0xE000u) | (code & 0x1FFFu));   /* (8192 is code 0) */
            }
        } else if (w[13] < 2u) {                        /* SAMPLER START, END, LOOP: a one-shot or a loop, not slices */
            int32_t s0 = w[16], e0 = w[17], len = e0 - s0, fr = (int32_t)gna_asset_frames(w[12]);
            if (j == 0u)
                w[16] = (uint16_t)gna_clamp(s0 + e[0] * len / 128, 0, e0 - 2);
            else if (j == 1u)
                w[17] = (uint16_t)gna_clamp(e0 + e[1] * len / 128, (int32_t)w[16] + 2, fr < 65535 ? fr : 65535);
            else
                w[13] = (uint16_t)(e[2] == 2 ? 1u : 0u);
        }
    }
}

/* a kit's EDIT 2 (DRUMS): a knob moved since the core last had it -> its pads' field from the kit's own, one knob a
 * batch: TONE every pad's tone, DECAY every pad's decay (x 2^(e/32)), SNAP the character of the snare, clap, the hats
 * and the rim, KICK the kick's tune; n the batch's fields */
__attribute__((noinline)) static uint32_t gna_kit(uint32_t ti, const track_t *t, const uint8_t *kit)
{
    uint32_t j, pd, n = 0;
    for (j = 0; j < 4u; j++) {
        int32_t e = t->p[P_E4 + j];
        if (e == gna.ke[ti][j])
            continue;
        for (pd = 0; pd < 16u; pd++) {                  /* (TONE: pad +8, field 3; DECAY: +4, 1; SNAP: +10, 4; KICK: +2, 0) */
            int32_t v = (int32_t)rd16(&kit[32u + 24u * pd + "\10\4\12\2"[j]]);
            if ((j == 2u && !((0x11Eu >> pd) & 1u)) || (j == 3u && pd))
                continue;
            v = j == 1u ? gna_clamp((int32_t)gna_oct16((uint32_t)v, e >> 1), 10, 4000) :
                j == 3u ? gna_clamp((int16_t)v + e * 19, -2400, 2400) : gna_clamp(v + e * 512, 0, 65535);
            gna.m[ti][n].parameter = (uint16_t)(0x200u + 16u * pd + (uint32_t)"\3\1\4\0"[j]);
            gna.m[ti][n++].value = v;
        }
        gna.ke[ti][j] = (int8_t)e;
        break;
    }
    return n;
}

/* the main loop: track t's absolute settings at its sound's rest places, unless they are already this firmware's (the
 * mark): after a sound load (apply_preset_to puts the mark's setting at 0, its default; Easy's loads call this at once,
 * gn_easy.c, so Easy's own record of the track is taken after it) and on a project saved before round EW */
/* every absolute control's rest place on record rec (into r, 0 where the engine has none) */
static void gna_rests(int32_t ce, const uint8_t *rec, int8_t *r)
{
    uint32_t c;
    for (c = 0; c < GNA_N; c++)
        r[c] = (int8_t)(GNA_P[ce][c] ? gna_rest(ce, c, rec) : 0);
}
static void gna_settle(track_t *t)
{
    const uint8_t *kit;
    int32_t ce = gnc_engine(t->eng_req);
    int8_t r[GNA_N];
    uint32_t c;
    if (ce < 0 || t->p[P_ED_FX] == GNA_MARK || t->p[P_ED_FX] == GNA_MARK_OF(t->preset))
        return;
    gna_rests(ce, gnc_record(ce, t->preset, &kit), r);
    for (c = 0; c < GNA_N; c++)
        if (GNA_P[ce][c])
            t->p[GNA_ID[c]] = r[c];
    t->p[P_ED_FX] = GNA_MARK_OF(t->preset);
}

/* upreset.c up_load (round EW-FIXES): the preset a user preset of engine e binds, from its stored mark fx: the record it
 * was saved from; 0 for a mark of no preset of that engine (0: saved before round EW, its pages settle on preset 0) */
static uint8_t gna_up_preset(uint32_t e, int32_t fx)
{
    int32_t ce = gnc_engine(e);
    uint32_t p = (uint32_t)(fx - 1) & 127u;
    return (uint8_t)(ce >= 0 && p < GNC_LIST[ce].all ? p : 0u);
}

/* the main loop (gn_core_host, after the moves): each planned track's pages -> a batch of what differs */
__attribute__((noinline)) static void gna_host_track(uint32_t ti, uint32_t b, int32_t ce, const uint8_t *rec, uint64_t owned)
{
    track_t *t = &trk[ti];
    const uint8_t *kit;
    uint16_t w[48];
    uint32_t c, p, n = 0, gp = 0;
    if (ce < 0 || !rec || gnc_engine(t->eng_req) != ce || t->engine + 1u != gnc.bound[ti] || t->preset != gnc.preset[ti])
        return;                                         /* (another sound on its way: its bind first) */
    if (gna.bfor[ti] != b) {                            /* a new bind: the core has the record (and kit) as stored */
        for (p = 0; p < 48u; p++)
            gna.w[ti][p] = (uint16_t)gna_word(rec, p);
        gna_rests(ce, rec, gna.rest[ti]);
        for (c = 0; c < 4u; c++)
            gna.ke[ti][c] = 0;
        gna.bfor[ti] = b;
    }
    if (gna.pend[ti])
        return;                                         /* (the interrupt has not taken the last batch yet) */
    if (ce == GC_DRUMS) {
        gnc_record(ce, gnc.preset[ti], &kit);
        n = gna_kit(ti, t, kit);
    } else {
        gna_want(t, ce, rec, ti, owned, w);
        for (p = 10; p < 52u && n < GNA_POST; p++) {    /* the record group P10..P47 (8 a batch), then P1..P4 */
            uint32_t q = p < 48u ? p : p - 47u;
            if (w[q] == gna.w[ti][q] || (p < 48u && gp == GNA_PG))
                continue;
            gna.m[ti][n].parameter = (uint16_t)q;
            gna.m[ti][n++].value = gna_dec(ce, q, w[q]);
            gna.w[ti][q] = w[q];
            gp += p < 48u;
        }
    }
    if (!n)
        return;
    gna.gp[ti] = (uint8_t)gp;
    gna.gk[ti] = (uint8_t)(n - gp) * (ce == GC_DRUMS);
    gna.mfor[ti] = b;
    RING_PUBLISH();
    gna.pend[ti] = (uint8_t)n;
}

/* piecewise straight, 0 at 0, `at` at place knee, `top` at 127 */
static int32_t gna_pw(int32_t v, int32_t knee, int32_t at, int32_t top)
{
    return v <= knee ? v * at / knee : at + (v - knee) * (top - at) / (127 - knee);
}
/* the master controls the song's DLY, REVERB and CHORUS pages set (main loop), and DLY's MIX */
__attribute__((noinline)) static void gna_host_master(void)
{
    static const uint16_t DIV[10] = {48, 24, 12, 6, 16, 8, 96, 192, 384, 768};   /* (params.c N_DIV, 48 a beat) */
    const int16_t *g = song.g;                          /* (each setting within its descriptor's range, as stored) */
    int32_t v[GNM_N];
    uint32_t f = 55125u * DIV[(uint16_t)g[G_DTIME] % 10u] / (uint32_t)(g[G_BPM] < 40 ? 40 : g[G_BPM]), i, n = 0;
    while (f > 44100u)                                  /* (44,100 x 60 / 48: a 48th of a beat's frames; the core's */
        f /= 2u;                                        /* echo is a second: the same division, halved) */
    v[GNM_ETIME] = (int32_t)f;
    v[GNM_EFB] = (g[G_DFDBK] & 127) * 2048 / 5;         /* (60, the default: the core's 24,576; 120: 49,152) */
    v[GNM_CDEPTH] = gna_pw(g[G_CDEPTH] & 127, 60, 88, 192);   /* (60: the core's 88) */
    v[GNM_RSIZE] = gna_pw(g[G_RSIZE] & 127, 90, 49152, 65535);   /* (90: the core's 2.17 s) */
    v[GNM_CRATE] = gna_clamp((int32_t)LFO_HZ_X100[g[G_CRATE] & 127], 10, 200);
    gnc.emix = (int16_t)((g[G_DMIX] & 127) * 256 / 90 - 256);   /* (the interrupt's gnc_ctl reads it) */
    if (gna.mpend)
        return;
    for (i = 0; i < GNM_N; i++)
        if (v[i] - GNM_BOOT[i] != gna.ms[i]) {
            gna.mm[n].parameter = GNM_ID[i];
            gna.mm[n++].value = v[i];
            gna.ms[i] = v[i] - GNM_BOOT[i];
        }
    if (n) {
        RING_PUBLISH();
        gna.mpend = (uint8_t)n;
    }
}

