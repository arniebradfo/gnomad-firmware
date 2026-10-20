/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kyle Palaniuk */
/* Gnomad (FORK.md): Easy mode on the synth's own panel and screen, for a child (design boards
 * FM1-Panel 3a to FM1-Boot 3i, FM1-Gnome 9b). Felucca's own pages are the other mode, Advanced,
 * untouched. The first power-on is Easy; after that the synth starts in the mode it was left in (round 7,
 * pick 1: gn_boot, at the first main-loop frame), or, once a project has been saved, in the mode of the last
 * save with that project loaded (round 27, F15). HOME and SAVE held together for 2 s without a break
 * switch between the two. The switch changes nothing musical
 * (gn_enter, gn_leave): only an ALGORITHM or PRESETS turn puts one of Easy's sounds on the selected
 * track.
 *
 * In Easy (3a; round 7's panel, the choices of 2026-10-06):
 *   ALGORITHM     ten pictures, nine of sounds (FUNNY the ninth, round CR) and the drums (the Gnome sings,
 *                 9b; the drums' pads, 3e): a detent puts the next picture's first sound on the selected track
 *   PRESETS       the sound inside the picture (57 in all: gn_state.c GN_SOUNDS), the peek list
 *                 of the picture's sounds for 2 s; on the loop page (LOOP: a block a bar, round UX-2) the demo
 *                 grooves (gn_grooves.c): a detent loads the next on all four tracks and plays it
 *   SELECT        the page ring, HOME SOUND GROOVES LOOP STEPS SONG MIX (round SEQ-1, gn_seq.c GN_RING); EDIT
 *                 held + SELECT the tempo itself, 40 to 240 BPM (an external clock: nothing); every header
 *                 shows the tempo
 *   KNOB 1..4     Bright, Wobble, Echo, Space of the track the keys play, while it holds one of
 *                 Easy's sounds: a bubble each, bigger is more (another sound: the bubbles rest);
 *                 on GLO's page the four tracks' volumes
 *   FX SCL ENV LFO  tracks 1 to 4, the selected one lit
 *   SEQ           the loop's pages (round UX-2, board GN-02 A, straight rows): the loop (a block a bar), the
 *                 steps (one bar up close: K1 the bar switch; a melody's note names, a kit's drum list with
 *                 K2 the drum), the record screen (playing: REC held records; stopped: a key writes at the
 *                 cursor and moves it on, K2 writes in place, K1 alone moves it (round 24),
 *                 PLAY opens the loop), round and round; K1-K4 are BARS (1 2 4 8), SWING, SNAP,
 *                 VOLUME (the record screen: MOVE, NOTE/REST, SNAP, KEEP/REPLACE; a kit's: MOVE, DRUM,
 *                 NOTE/REST, KEEP/REPLACE)
 *   GLO           the four volumes on the four knobs (and back)
 *   ARP           the track's arpeggio on and off (Easy's sound)
 *   EDIT          tap tempo (Felucca's own: the last four taps), the big number while tapping; held 0.5 s
 *                 SHIFT (round UX-3, UX-4): the page's Advanced depth at the same place (the sound's deep page,
 *                 a step's note and strength, pan, MUTE on the track buttons), said before it acts
 *   SAVE          tap: the song saved, never over a song of someone else's (the Gnome says SAVED)
 *                 (when let go, and never when HOME joined: HOME + SAVE 2 s is the switch); round 24: the
 *                 loop Easy wrote is saved by itself too, once stopped and settled (gn_autosave)
 *   HOME          Home: the four tracks at a glance (round UX-1); on Home, the selected track's sound
 *   OCT- / OCT+   octave, two up or down at most (held together for 5 s they are Felucca's way
 *                 into its update mode, main.c: from 2 s Felucca's countdown has the screen)
 *   PLAY/STOP     the tracks' patterns on Felucca's own sequencer, from any page
 *   REC           tap: armed (playing: from bar 1; stopped: PLAY counts in 4 3 2 1), tap again: off;
 *                 held 0.4 s while the loop plays: punch in until let go; K4 KEEP / REPLACE meanwhile
 *   SAVE held     undo of Easy's last recording (when let go; never when HOME joined)
 *   keys          on one of Easy's sounds every key its own note, chromatic, the black keys of the upper
 *                 row too (round 13); on a kit (KIT, DRUM MACHINE, HAND DRUM, TOMS) the sixteen white keys of
 *                 the lower row pads 1 to 16, F3 to G5, and the eleven black keys the kit's eleven others
 *                 (on a Gnomad kit, round EW-FIXES: a ghost of a drum beside it); the Gnome sings a syllable a key
 *                 (on the Gnomad GNOME the core's own, P20: gn_sound_put sets TALK 12); on another sound every
 *                 key plays as in Felucca
 * The screens: entering (3i), play (3b; 9b for the Gnome), pick a picture (3c, while ALGORITHM
 * turns), the peek list (while PRESETS turns), loop (3d, SEQ, and while recording), drums (3e), the volumes (GLO), tap tempo (EDIT), saved
 * and safe to unplug (3g: after the app saves a project, or SAVE, read back from flash), and leaving
 * (3h). Each but 3g, 3h and 3i has the header, the knob row, the field and the band (gn_screens.c).
 *
 * While Easy is off every hook returns at once: gn_input only watches for the hold that enters
 * (gn_hold_watch) and leaves the input to Felucca, gn_leds and gn_draw leave the LEDs and the screen
 * to Felucca (gn_draw: but for the strips that clear Easy's screen after leaving), gn_key passes every
 * note through, gn_editor and gn_project do nothing. Of these only gn_key can run before the main loop
 * (the audio interrupt, as soon as audio runs): it returns the note. */
#define GN_MAKER "STRONGNOME"

static int gn_btn(uint32_t label) { return (int)((fm1_in.buttons >> panel.btn[label]) & 1u); }

/* ms since a moment kept as fm1_ms | 1 (bit 0 set: 0 means "not set") */
static uint32_t gn_since(uint32_t t) { return fm1_ms - (t & ~1u); }

/* the track Easy's keys, knobs and REC act on: the selected one */
static uint32_t gn_part(void) { return song.sel % NTRK; }

/* --------------------------------------------- the knobs and the sounds --- */
/* the setting knob k moves on track p now: Easy's own sound's, while p holds it (gn_easy_of: the
 * engine p really has is the sound's), else none (the bubbles rest: a knob writes nothing to a
 * sound Easy did not make) */
/* round 13: another sound's four knobs, by its engine: the engine's Bright (as Easy's own sounds of it have it,
 * FACTS-SYNTH-1.0.md), Wobble (the LFO's pitch, or its level where the pitch does not move it) with the LFO's
 * rate brought to Easy's 5.4 Hz while it is off its starting place (round 14: a claim, gn_wob_claim), Echo and
 * Space: one straight travel over each, the knob's place read from the sound (gn_knob_follow) */
#define GN_FB_WOB GN_LIN(P_LD_PIT, 0, 10, 8)          /* (the LFO's rate: gn_wob_claim) */
#define GN_FB_WOB_AMP GN_LIN(P_LD_AMP, 0, 127, 8)
#define GN_FB_FX GN_LIN(P_DLY, 0, 127, 8), GN_LIN(P_REV, 0, 127, 8)
/* (each engine's Bright, and whether its Wobble is the LFO's level: GN_FB_AMP; Wobble, Echo and Space are the same
 * three maps on every engine, GN_FB_REST, so they are kept once) */
#define GN_FB(id, lo, hi) GN_LIN(id, lo, hi, 8)
#define GN_FB_AMP(id, lo, hi) GN_LIN(id, lo, hi, 8)
#define GN_FB_AMPS (1u << GE_DRUM | 1u << GE_NOISE | 1u << GE_SLICE)
static const gn_knob_t GN_FALLBACK[14] = {
    [GE_ANALOG] = GN_FB(P_E4, 0, 127),    /* CUT */
    [GE_RSV1] = GN_FB(P_E2, 0, 127),
    [GE_PHASE] = GN_FB(P_E2, 0, 127),     /* DCW */
    [GE_LOFI] = GN_FB(P_E7, 0, 127),      /* TONE */
    [GE_SAMPLE] = GN_FB(P_E4, 0, 127),    /* CUT */
    [GE_VOICE] = GN_FB(P_E4, 0, 127),     /* BUZZ */
    [GE_TRIO] = GN_FB(P_E5, 0, 127),      /* CUT */
    [GE_WHEEL] = GN_FB(P_E3, 0, 8),       /* TOP */
    [GE_GRAIN] = GN_FB(P_E7, 0, 127),     /* TONE */
    [GE_PHYS] = GN_FB(P_E2, 0, 127),      /* BRIT */
    [GE_DRUM] = GN_FB_AMP(P_E2, 0, 127),  /* TONE */
    [GE_NOISE] = GN_FB_AMP(P_E2, 0, 127), /* FREQ */
    [GE_FM6] = GN_FB(P_E2, -64, 63),      /* MLVL */
    [GE_SLICE] = GN_FB_AMP(P_E7, 0, 127), /* TONE */
};
static const gn_knob_t GN_FB_REST[2][3] = {{GN_FB_WOB, GN_FB_FX}, {GN_FB_WOB_AMP, GN_FB_FX}};
/* the fallback map of engine e's knob k (e < 14) */
static const gn_knob_t *gn_fb_map(uint32_t e, uint32_t k)
{
    return k ? &GN_FB_REST[(GN_FB_AMPS >> e) & 1u][k - 1u] : &GN_FALLBACK[e];
}
_Static_assert(GE_SLICE == 13 && NENGINES <= 14, "one fallback map an engine");

/* the setting knob k moves on track p now: Easy's own sound's, while p holds it (gn_easy_of: the engine p really
 * has is the sound's), else (round 13) the fallback map of the engine p has */
#if GN_ENGINES
/* Gnomad (FORK.md, "Gnomad engines"): on a Gnomad engine (gn_core.c) every sound's four knobs are the core's four
 * macros: BRIT and WOBL, and the track's delay and reverb sends (the core's Echo and Space) */
static const gn_knob_t GN_CORE_KNOBS[4] = {GN_LIN(P_E0, 0, 127, 8), GN_LIN(P_E1, 0, 127, 0), GN_ECHO, GN_SPACE};
/* round MF: a bank sound's own four knobs (gn_core.c gnc_slot, its record's macro map). A knob that
 * is one of the controls is GN_CORE_KNOBS' for that control (the sends stay the sends, in whatever place); a knob of
 * the sound's own keeps its amount in K1..K4 (P_E0 + its place), one straight travel 0 .. 127 with the sound's rest
 * place (gnc_kr) at Easy's step nearest it, exactly, so a sound load starts where the sound was authored */
static struct {
    uint16_t key[NTRK];                   /* the track's engine and preset the knobs were read for, + 1 (0: none) */
    uint8_t own[NTRK];                    /* the places that are the sound's own (bits) */
    gn_knob_t own_knob[NTRK][4];
    uint8_t fixed[NTRK][4];               /* the others: GN_CORE_KNOBS' index */
} gn_cm;
static void gn_cmap(uint32_t p)
{
    const track_t *t = &trk[p % NTRK];
    int32_t ce = gnc_engine(t->eng_req);
    uint16_t key = (uint16_t)(((uint32_t)t->eng_req << 8 | t->preset) + 1u);
    const uint8_t *kit, *rec;
    uint32_t k;
    p %= NTRK;
    if (gn_cm.key[p] == key)
        return;
    gn_cm.key[p] = key;
    gn_cm.own[p] = 0;
    rec = ce >= 0 ? gnc_record(ce, t->preset, &kit) : 0;
    for (k = 0; k < 4u; k++) {
        uint32_t store, rest, kr, er;
        uint64_t par;
        if (!gnc_slot(rec, ce, k, &store, &par, &rest)) {
            gn_cm.fixed[p][k] = (uint8_t)(store == P_E0 ? 0u : store == P_E1 ? 1u : store == P_DLY ? 2u : 3u);
            continue;
        }
        kr = gnc_kr(rest);
        er = kr ? (kr * GN_STEPS + 63u) / 127u : 0u;  /* (resting at 0: the middle is half the travel, GN_MID) */
        er = kr && er < 1u ? 1u : kr < 127u && er > GN_STEPS - 1u ? GN_STEPS - 1u : er;
        gn_cm.own[p] |= (uint8_t)(1u << k);
        gn_cm.own_knob[p][k] = (gn_knob_t){(uint8_t)store, 0, (int8_t)(kr ? kr : 64u), 127, (uint8_t)er, GN_NONE, 0, 0, 0};
    }
}
/* the label of knob k on track p: a Gnomad sound's from its record (gc_macro_label), else 0 (TONE MOTION ECHO SPACE) */
static const char *gn_knob_label(uint32_t p, uint32_t k)
{
    const track_t *t = &trk[p % NTRK];
    int32_t ce = gnc_engine(t->eng_req);
    const uint8_t *kit, *rec;
    if (ce < 0 || ce == GC_DRUMS || (gn.entry_put && p % NTRK == GN_MELODY))
        return 0;
    rec = gnc_record(ce, t->preset, &kit);
    return gc_macro_label((const gc_patch *)rec, k & 3u);
}
#endif
static const gn_knob_t *gn_map(uint32_t p, uint32_t k)
{
    const gn_sound_t *s = gn_sound_def(gn_shown(p % NTRK));
#if GN_ENGINES
    if (gnc_engine(trk[p % NTRK].eng_req) >= 0 && !(s && gn.entry_put && p % NTRK == GN_MELODY)) {
        gn_cmap(p);
        return (gn_cm.own[p % NTRK] >> (k & 3u)) & 1u ? &gn_cm.own_knob[p % NTRK][k & 3u]
                                                       : &GN_CORE_KNOBS[gn_cm.fixed[p % NTRK][k & 3u]];
    }
#endif
    if (s && gn.entry_put && p % NTRK == GN_MELODY)
        return gn_snd_knob(s, k);                      /* (the entry's Gnome, shown: his map) */
    if (!s || s->engine != trk[p % NTRK].eng_req)
        return trk[p % NTRK].eng_req < 14u ? gn_fb_map(trk[p % NTRK].eng_req, k & 3u) : 0;
    return gn_snd_knob(s, k);
}

/* round 14: the fallback Wobble's claim. Another sound's Wobble moved from where Easy found it brings the LFO's
 * rate to Easy's 5.4 Hz (else a slow or stopped LFO would not wobble); the rate and the depth as they were, and the
 * knob's place then, are kept, and put back when the knob comes back to that place and when Easy is left, each
 * only while it still holds what Easy wrote (anyone else's write since is theirs); a sound load ends the claim.
 * Round 15: anyone else's write of the rate or the depth ends that half of the claim (GN_WOB_GONE in its baseline),
 * the same value or not: the editor's (gn_editor, gn_wob_retire) and any other the knob's bubble follows
 * (gn_knob_follow, which takes the new value as the knob's: never a proof that Easy wrote it); Easy's next move of
 * Wobble starts that half again from the track's value then */
#define GN_WOB_RATE 89
#define GN_WOB_GONE (-32767 - 1)                      /* wob_rate, wob_depth: that half's claim ended */
static int gn_fallback(uint32_t p)                    /* track p's knobs are its engine's fallback map */
{
    return !(gn.entry_put && p == GN_MELODY) && !gn_sound_def(gn_easy_of(&trk[p])) && trk[p].eng_req < 14u
#if GN_ENGINES
           && gnc_engine(trk[p].eng_req) < 0          /* (Gnomad: no LFO claim, its Wobble is the core's) */
#endif
        ;
}
static void gn_wob_drop(uint32_t p) { gn.wob_mask &= (uint8_t)~(1u << (p % NTRK)); }
static void gn_wob_back(uint32_t p)
{
    const gn_knob_t *m;
    p %= NTRK;
    if (!((gn.wob_mask >> p) & 1u))
        return;
    gn_wob_drop(p);
    if (trk[p].eng_req >= 14u)
        return;
    m = gn_fb_map(trk[p].eng_req, 1u);
    if (gn.wob_rate[p] != GN_WOB_GONE && trk[p].p[P_LRATE] == GN_WOB_RATE)
        trk[p].p[P_LRATE] = gn.wob_rate[p];
    if (gn.wob_depth[p] != GN_WOB_GONE && trk[p].p[m->id] == gn.wrote[p][1]) {
        trk[p].p[m->id] = gn.wob_depth[p];
        gn.wrote[p][1] = gn.wob_depth[p];
    }
}
static void gn_wob_back_all(void)
{
    uint32_t p;
    for (p = 0; p < NTRK; p++)
        gn_wob_back(p);
}
static void gn_wob_claim(uint32_t p, uint32_t k)      /* before knob k of track p moves */
{
    if (k != 1u || !gn_fallback(p))
        return;
    if (!((gn.wob_mask >> p) & 1u))
        gn.wob_rate[p] = gn.wob_depth[p] = GN_WOB_GONE;
    gn.wob_mask |= (uint8_t)(1u << p);
    if (gn.wob_rate[p] == GN_WOB_GONE)
        gn.wob_rate[p] = trk[p].p[P_LRATE];
    if (gn.wob_depth[p] == GN_WOB_GONE) {             /* (the knob's place: where this half starts) */
        gn.wob_start[p] = gn.amount[p][1];
        gn.wob_depth[p] = trk[p].p[gn_fb_map(trk[p].eng_req, 1u)->id];
    }
}
/* gn_editor: the editor writes valid track t's setting id (before it runs) */
static void gn_wob_retire(uint32_t t, uint32_t id)
{
    if (!((gn.wob_mask >> t) & 1u) || trk[t].eng_req >= 14u)
        return;
    if (id == P_LRATE)
        gn.wob_rate[t] = GN_WOB_GONE;
    if (id == gn_fb_map(trk[t].eng_req, 1u)->id)
        gn.wob_depth[t] = GN_WOB_GONE;
}
static int16_t gn_set(track_t *t, uint32_t id, int32_t v);
static void gn_wob_moved(uint32_t p, uint32_t k)      /* after it moved */
{
    if (k != 1u || !((gn.wob_mask >> p) & 1u))
        return;
    if (gn.amount[p][1] == gn.wob_start[p])
        gn_wob_back(p);                               /* (back at its place: the sound's own again) */
    else
        gn_set(&trk[p], P_LRATE, GN_WOB_RATE);
}

/* a write of Easy's to track t: inside the setting's own range for the engine t has (params.c
 * track_desc, and the editor's static range, editor.c ed_tdesc), a setting with no range (an
 * engine's unused slot) left as it is; returns what the setting holds */
static int16_t gn_set(track_t *t, uint32_t id, int32_t v)
{
    const param_desc_t *d;
    int32_t lo, hi;
    if (id >= P_COUNT)
        return 0;
    d = track_desc(t, id);
    lo = d->min;
    hi = d->max;
    if (id >= P_E0 && id <= P_E7) {
        const param_desc_t *s = &ENGINES[t->eng_req % NENGINES]->edit[id - P_E0];
        lo = s->min > lo ? s->min : lo;
        hi = s->max < hi ? s->max : hi;
    }
    if (hi <= lo)
        return t->p[id];
    t->p[id] = (int16_t)clamp(v, lo, hi);
    return t->p[id];
}

/* amount 0 .. GN_STEPS along a knob's two pieces: lo at 0, mid at rest, hi at GN_STEPS */
static int32_t gn_line(int32_t lo, int32_t hi, int32_t a, int32_t n)   /* a of n along lo..hi, rounded */
{
    return n > 0 ? lo + ((hi - lo) * a * 2 + n) / (2 * n) : hi;
}
static int32_t gn_travel(int32_t lo, int32_t mid, int32_t hi, uint32_t rest, uint32_t amount)
{
    uint32_t bend = rest ? rest : GN_STEPS / 2u;     /* (resting at 0: mid at the middle) */
    return amount <= bend ? gn_line(lo, mid, (int32_t)amount, (int32_t)bend)
                          : gn_line(mid, hi, (int32_t)(amount - bend), (int32_t)(GN_STEPS - bend));
}

/* knob k's settings for its amount on track p */
static void gn_knob_write(uint32_t p, uint32_t k)
{
    const gn_knob_t *m = gn_map(p, k);
    uint32_t a = gn.amount[p][k];
    if (!m)
        return;
    gn.wrote[p][k] = gn_set(&trk[p], m->id, gn_travel(m->lo, m->mid, m->hi, m->rest, a));
    if (m->id2 != GN_NONE)
        gn_set(&trk[p], m->id2, gn_travel(m->lo2, m->mid2, m->hi2, m->rest, a));
}

/* the amount (0 .. GN_STEPS) of knob m whose setting comes nearest v, the lower of two as near */
static uint32_t gn_knob_near(const gn_knob_t *m, int32_t v)
{
    uint32_t a, best = 0;
    int32_t d, bd = 0x7FFFFFFF;
    for (a = 0; a <= GN_STEPS; a++) {
        d = gn_travel(m->lo, m->mid, m->hi, m->rest, a) - v;
        d = d < 0 ? -d : d;
        if (d < bd) {
            bd = d;
            best = a;
        }
    }
    return best;
}

/* another writer (the app, the editor) moved a knob's setting: the bubble follows it (the nearest amount) */
static void gn_knob_follow(uint32_t p)
{
    uint32_t k;
    if (gn.entry_put && p == GN_MELODY)
        return;                                       /* (the entry's Gnome, shown: his bubbles at rest) */
    for (k = 0; k < 4u; k++) {
        const gn_knob_t *m = gn_map(p, k);
        int32_t v;
        if (!m || m->hi == m->lo)
            continue;
        v = trk[p].p[m->id];
        if (v == gn.wrote[p][k])
            continue;
        gn.amount[p][k] = (uint8_t)gn_knob_near(m, v);
        gn.wrote[p][k] = (int16_t)v;
        if (k == 1u && ((gn.wob_mask >> p) & 1u))
            gn.wob_depth[p] = GN_WOB_GONE;            /* (round 15: someone else's depth, not Easy's claim) */
    }
}

/* a write of Easy's sound to a track setting a sound load keeps (GN_KEPT): Easy's first write of it
 * (since entering, since anyone else wrote it, or since the project was replaced) takes what the track
 * has then as the value to put back on leaving (the baseline: this track's, now, never the one of the
 * project Easy was entered with); Easy's later writes keep that baseline (gn_leave). While a song is
 * prepared or plays (chain_busy), none is taken of a setting its motion can move (the level) */
/* track p's own motion moves its level as the loop plays (motion.c motion_step: an event of P_LEVEL, motion on
 * for the track) */
static int gn_level_motion(uint32_t p)
{
    uint32_t i;
    if (!((motion.on >> (p % NTRK)) & 1u))
        return 0;
    for (i = 0; i < motion.count && i < MOTION_MAX; i++)
        if ((motion.event[i].place >> 6) == p % NTRK && motion.event[i].param == P_LEVEL)
            return 1;
    return 0;
}

static void gn_set_kept(track_t *t, uint32_t id, int32_t v)
{
    uint32_t p = (uint32_t)(t - trk) % NTRK, k;
    for (k = 0; k < NELEM(GN_KEPT) && GN_KEPT[k] != id; k++)
        ;
    if (k < NELEM(GN_KEPT) && gn_kept_motion(id) && (chain_busy() || (song.playing && gn_level_motion(p))))
        k = NELEM(GN_KEPT);                           /* a song, or the loop with the track's level motion: */
                                                      /* its motion moves it, no claim (round 8) */
    if (k < NELEM(GN_KEPT) && !((gn.kept_mask[p] >> k) & 1u)) {
        gn.kept_was[p][k] = t->p[id];
        gn.kept_mask[p] |= (uint8_t)(1u << k);
    }
    v = gn_set(t, id, v);
    if (k < NELEM(GN_KEPT))
        gn.kept_wrote[p][k] = (int16_t)v;
}

#if GN_ENGINES
/* (FORK.md, round FW-CONTENT-6) Easy's preset pi on track t: as ui.c's apply_preset_to, but a Gnomad bank sound past
 * its engine's offered list (gn_core.c GN_PRESETS_MAX) is bound by bank ID, not wrapped: pi is its place in the
 * engine's whole authored list (gn_state.c GN_BANK, from the ID; GN_PRESET reads every authored sound's record), the
 * track's preset is set to it and gn_core.c's bind reads that list. apply_preset_to loads the rest of the sound as
 * for any preset (defaults, user slot, release), then the record's own entry is written over it, the audio
 * interrupt off between the two (it binds the track's preset: it sees one sound, never the wrapped one) */
static void gn_preset_put(track_t *t, uint32_t pi)
{
    const engine_t *e = ENGINES[t->eng_req % NENGINES];
    int32_t ce = gnc_engine(t->eng_req);
    const preset_t *pr;
    uint32_t i;
    if (ce < 0 || pi < e->npresets || pi >= GNC_LIST[ce].all) {
        apply_preset_to(t, pi);
        gna_settle(t);                                /* (round EW: the pages' settings at the sound's own) */
        return;
    }
    pr = GN_PRESET(e, pi);                            /* (read from the record before the interrupt is off) */
    fm1_irq_off();
    apply_preset_to(t, pi);
    t->preset = (uint8_t)pi;
    for (i = 0; i < 8u; i++)
        t->p[P_E0 + i] = (int16_t)pr->e[i];
    t->p[P_ATK] = pr->env[0];
    t->p[P_DEC] = pr->env[1];
    t->p[P_SUS] = pr->env[2];
    t->p[P_REL] = pr->env[3];
    t->p[P_ED_FLT] = pr->fenv;
    t->p[P_VOICE] = pr->mono ? V_LEGATO : V_POLY;
    for (i = 0; i < 4u; i++)                          /* the sends (a Gnomad preset states all four: FX) */
        if (pr->fx[i])
            t->p[P_DIST + i] = (int16_t)(pr->fx[i] - 1);
    fm1_irq_on();
    gna_settle(t);                                    /* (round EW: the pages' settings at the sound's own) */
}

/* Round FW-CONTENT-8 (FORK.md): the power-on sounds (gn_state.c GN_POWER_ON) on the four tracks, once a power-on: at
 * the first main-loop frame (gn_boot), or before an editor command that comes earlier (gn_editor), so nothing loaded
 * after power-on (a project, a preset, a backup, Easy's song) is ever touched; main.c's felucca_init has put TRK_DEF's
 * sounds and nothing else has run. Loaded as felucca_init loads TRK_DEF's: the engine, then the preset with its sends,
 * no undo copy; the steps, the patterns and the kept settings stay */
static void gn_power_on_sounds(void)
{
    uint32_t i;
    if (gn.booted & GN_BOOT_SOUNDS)
        return;
    gn.booted |= GN_BOOT_SOUNDS;
    undo_depth++;                                     /* (no undo copy of the power-on loads, as felucca_init) */
    for (i = 0; i < NTRK; i++) {
        track_t *t = &trk[i];
        if (t->eng_req != GN_POWER_ON[i][0])
            set_engine_of(t, GN_POWER_ON[i][0]);
        gn_preset_put(t, GN_POWER_ON[i][1]);
    }
    undo_depth--;
    ui.force = 1;
}
#else
#define gn_preset_put apply_preset_to
#endif

/* Easy's sound i (GN_SOUNDS index) on track p: its engine and preset (ui.c), then
 * GN_COMMON, its own settings and its knobs at rest, all inside one sound load for Felucca's undo
 * (load_begin .. load_end: SAVE held in Advanced swaps back the sound from before it, or from before
 * the first of several picks in a row, since the track is just as the last pick left it). The track
 * is no one's while its sound is made (own 0: the key hook plays it as Felucca would); Easy owns it
 * from the moment the whole sound is there, with the engine, preset and user slot it ended with. */
/* round MF: a write of a sound's own setting a sound load makes: on a Gnomad engine its engine settings past K1, K2
 * (an Easy sound's Felucca settings, which a Gnomad engine never had: K3 and K4 keep its own knobs now) are left */
static int gn_put_ok(const track_t *t, uint32_t id)
{
#if GN_ENGINES
    if (gnc_engine(t->eng_req) >= 0 && id >= P_E2 && id <= P_E7)
        return 0;
#endif
    (void)t;
    (void)id;
    return 1;
}

static void gn_dirty(void);
static void gn_sound_put(uint32_t p, uint32_t i)
{
    const gn_sound_t *s = gn_sound_def(i);
    track_t *t = &trk[p % NTRK];
    uint32_t k;
    p %= NTRK;
    if (!s)
        return;
    if (p == GN_MELODY && gn.entry_put) {             /* round 14: the entry's Gnome's place: a fresh copy */
        gn.entry_put = 0;                             /* (Felucca's undo: track 1 as before Easy; the keys */
        undo.keep = 0;                                /* that asked for him sound on what is put: gn_entry_keys) */
    }
    gn_disown(p);
    gn_wob_drop(p);
    RING_PUBLISH();
    gn.mgen++;                                        /* (round 10: a sound load resets the track's motion) */
    load_begin(t, UNDO_SOUND);
    if (t->eng_req != s->engine)
        set_engine_of(t, s->engine);
    gn_preset_put(t, s->preset);
    for (k = 0; k < NELEM(GN_COMMON); k++)
        gn_set_kept(t, GN_COMMON[k].id, GN_COMMON[k].v);
    for (k = 0; k < s->nset; k++)
        if (gn_put_ok(t, s->set[k].id))
            gn_set_kept(t, s->set[k].id, s->set[k].v);
    for (k = 0; k < 4u; k++) {                        /* the knobs at rest (as gn_knob_write) */
        const gn_knob_t *m = gn_snd_knob(s, k);
        gn.amount[p][k] = m->rest;
        gn.wrote[p][k] = gn_put_ok(t, m->id) ? gn_set(t, m->id, gn_travel(m->lo, m->mid, m->hi, m->rest, m->rest))
                                             : t->p[m->id];
        if (m->id2 != GN_NONE && gn_put_ok(t, m->id2))
            gn_set(t, m->id2, gn_travel(m->lo2, m->mid2, m->hi2, m->rest, m->rest));
    }
#if GN_ENGINES
    gn_cmap(p);                                       /* round MF: a bank sound's own knobs at their rest places */
    if (gnc_engine(t->eng_req) >= 0 && gn_cm.own[p]) {
        for (k = 0; k < 4u; k++) {
            const gn_knob_t *m = gn_map(p, k);
            if ((gn_cm.own[p] >> k) & 1u) {
                gn.amount[p][k] = m->rest;
                gn.wrote[p][k] = gn_set(t, m->id, gn_travel(m->lo, m->mid, m->hi, m->rest, m->rest));
            } else {                                  /* (a control: its bubble where its setting is) */
                gn.amount[p][k] = (uint8_t)gn_knob_near(m, t->p[m->id]);
                gn.wrote[p][k] = t->p[m->id];
            }
        }
    }
    if ((s->flags & GNF_SYLL) && gnc_engine(t->eng_req) == GC_GNOME)
        t->p[P_E7] = 12;                              /* round EW: the Gnome's syllables, a key each (EDIT 2's TALK) */
    t->p[P_ED_FX] = 0;                                /* round EW: Easy's sets above are Felucca's recipes, the Gnomad */
    gna_settle(t);                                    /* sound plays as authored: the pages' settings at its own */
#endif
    load_end(t);
    gn.own_eng[p] = t->eng_req;
    gn.own_pre[p] = t->preset;
    gn.own_usr[p] = t->user;
    gn.own_id[p] = gn_id_param(t->eng_req) < P_COUNT ? t->p[gn_id_param(t->eng_req)] : 0;
    gn.own_gen[p] = fm6_pgen[p];                     /* (FM6: the patch the pick loaded) */
    RING_PUBLISH();
    gn.own[p] = (uint8_t)(i + 1u);
    if (gn.auto_dirty == GN_LOOP_SAVED)              /* round 25: the song Easy saved (or loaded) with another sound: */
        gn_dirty();                                   /* saved again once settled, so the next power-on has it */
}

/* every input call, the chord counting or not: a track that no longer holds what Easy put there is no
 * longer Easy's, for good (an engine switched away and back, the same preset loaded again by someone
 * else, a song's motion moving PTCH or SET away and back while HOME + SAVE are held, are someone else's) */
static void gn_owner_check(void)
{
    uint32_t t;
    for (t = 0; t < NTRK; t++)
        if (gn.own[t] && gn_easy_of(&trk[t]) == GN_NOSND)
            gn_disown(t);
}

/* round 24 (finding F6): the phone app's PRESET (engine, preset) on track p while Easy runs, when it is one of
 * Easy's 49 sounds, lands as Easy's pick of it: the app picks a sound by its preset, so the track gets the sound
 * whole (Easy's settings, the GN_KEPT claims, the knobs at rest), Easy owns it and Easy's screen shows the
 * picture, as a turn of ALGORITHM or PRESETS would; no new command. Which of Easy's sounds: the one the track shows
 * already when it is that preset (the Gnome stays the Gnome); else the one named for the preset (a picture's
 * first sound is named for its picture: VOICE VOX LEAD is Vox lead, not the Gnome; FM6 TINE EP is Tine piano);
 * else the picture's own. Never the kit (SAMPLE preset 0 is PIANO: the kit is its SET, PERC). Returns track << 6
 * | sound + 1 for gn_adopt, 0 for none. Read by gn_editor before the command runs (the track's sound as it was) */
static uint32_t gn_adopt_of(uint32_t p, const uint8_t *a, uint32_t na)
{
    const engine_t *e;
    uint32_t want, cur, i, first = GN_NOSND, named = GN_NOSND;
    p %= NTRK;
    if (na < 2u || a[0] >= NENGINES || a[0] == ENGI_DIGITAL || !(e = ENGINES[a[0]])->npresets)
        return 0;
    want = preset_orig(e, a[1] % e->npresets);
    cur = gn_shown(p);
    for (i = 0; i < GN_NSOUNDS; i++) {
        if (i == GN_SND_KIT || GN_SOUNDS[i].engine != a[0] || preset_orig(e, GN_SOUNDS[i].preset % e->npresets) != want)
            continue;
        if (i == cur)
            return p << 6 | (i + 1u);
        if (first == GN_NOSND)
            first = i;
        if (named == GN_NOSND && i != GN_GROUP[GN_SOUNDS[i].pic])
            named = i;
    }
    i = named != GN_NOSND ? named : first;
    return i < GN_NSOUNDS ? p << 6 | (i + 1u) : 0u;
}

/* the adoption, before anything else of the app's or the panel's (gn_editor at its next command, gn_input at its
 * next call): only if the app's load landed and nothing loaded since (the track's engine and preset the sound's,
 * no user slot); track 1's entry Gnome, if he was only shown, is gone (track 1 is the app's load now). One of
 * Felucca's loads in a row with the app's, so SAVE held in Advanced gives back the sound from before the app's */
static void gn_adopt(void)
{
    uint32_t p = (gn.adopt >> 6) % NTRK, i = (gn.adopt & 63u) - 1u;
    const gn_sound_t *s = gn_sound_def(i);
    const engine_t *e;
    gn.adopt = 0;
    if (!gn.on || !s)
        return;
    e = ENGINES[s->engine % NENGINES];
    if (trk[p].eng_req != s->engine || trk[p].user || !e->npresets ||
        trk[p].preset != preset_orig(e, s->preset % e->npresets))
        return;
    if (p == GN_MELODY)
        gn.entry_put = 0;
    gn_sound_put(p, i);
    gn.boot_t0 = gn.tap_t = 0;
    gn.pick_t = gn.peek_t = 0;
    if (p == gn_part()) {                             /* (the screen: the pick's, or the peek list's) */
        if (i == GN_GROUP[s->pic])
            gn.pick_t = fm1_ms | 1u;
        else
            gn.peek_t = fm1_ms | 1u;
    }
}

/* the picture the selected track shows: the picture of Easy's sound on it, GN_OTHER for another sound */
static uint32_t gn_picture(void) { return gn_pic_of(gn_shown(gn_part())); }

/* ALGORITHM (pick 3): a picture a detent, on the selected track, its first sound (a picture's first sound
 * is its sound of rounds 1 to 6); from another sound, the first picture or the last */
static void gn_pick(int32_t steps)
{
    uint32_t cur = gn_picture(), n;
    int32_t base = cur < GN_NPICS ? (int32_t)cur : (steps > 0 ? -1 : 0);
    n = (uint32_t)(((base + steps) % (int32_t)GN_NPICS + (int32_t)GN_NPICS) % (int32_t)GN_NPICS);
    gn_sound_put(gn_part(), GN_GROUP[n]);
}

/* PRESETS (pick 3): the sound inside the selected track's picture, a detent a sound, round and round; the
 * peek list shows meanwhile (gn.peek_t). Another sound: PRESETS rests (ALGORITHM picks a picture first) */
static void gn_entry_load(void);
static void gn_peek(int32_t steps)
{
    uint32_t s, pic, n0, n;
    if (gn.entry_put && gn_part() == GN_MELODY)
        gn_entry_load();                              /* (round 14: the entry's Gnome, now he is used) */
    s = gn_easy_of(&trk[gn_part()]);
    if (s == GN_NOSND)
        return;
    pic = gn_pic_of(s);
    n0 = GN_GROUP[pic];
    n = GN_GROUP[pic + 1u] - n0;
    gn_sound_put(gn_part(), n0 + (uint32_t)((((int32_t)(s - n0) + steps) % (int32_t)n + (int32_t)n) % (int32_t)n));
    gn.peek_t = fm1_ms | 1u;
    gn.grv &= 0x7Fu;                                  /* (the peek list, not the grooves': gn_grooves.c GN_GV_LIST) */
}

/* ---------------------------------------------------------- recording --- */
/* REC: the track being recorded is copied first, so a hold can put it back. The copy belongs to Easy's
 * own last recording only. The app's writes are the editor's commands: gn_editor (gn_state.c) drops
 * the copy before any command that writes a step, the pattern settings, a song, a project or a backup
 * runs, armed recording or not. What records into the armed track meanwhile (the keys, and MIDI notes,
 * which seq.c records as it records the keys) is that recording. As a backstop the copy is also tied
 * to the patterns as Easy's recording left them (undo_mine: the recorded track, undo_rest: the other
 * three): a change of another track's, or of this one's while nothing records, drops it
 * (gn_undo_watch); so do a song, another track and a mode switch. */
/* round 24: the loop Easy wrote is saved by itself. Project RAM does not survive a power-off, so an unsaved
 * loop was lost with it; now each of Easy's own writes of a pattern (a recording that wrote, REPLACE, step entry,
 * BARS, SNAP, an undo) marks the loop unsaved (gn_dirty), and once the loop is stopped and settled, Easy saves it
 * as SAVE does (gn_autosave): into Easy's own slot, else the first empty one, never over a song of someone
 * else's. Settled: nothing records, armed or counts in, no key or button is down, and 4 s since the last write,
 * key or button (a flash erase silences the audio: never under the child's fingers); and at least 10 s after the
 * last autosave, so the flash is not hammered (a sector pair a slot, each erase one of ~100,000). Times in 16 ms
 * ticks of 16 bits (auto_t: the autosave not before then), kept within the gap ahead so they never wrap */
#define GN_AUTO_SETTLE 250u                           /* 4 s */
#define GN_AUTO_GAP 625u                              /* 10 s */
static uint16_t gn_t16(void) { return (uint16_t)(fm1_ms >> 4); }
static void gn_auto_push(uint32_t ticks)          /* the autosave not before `ticks` from now */
{
    int32_t d = (int16_t)(uint16_t)(gn.auto_t - gn_t16());
    if (d < (int32_t)ticks || d > (int32_t)GN_AUTO_GAP)
        gn.auto_t = (uint16_t)(gn_t16() + ticks);
}
static void gn_dirty(void)
{
    gn.auto_dirty = GN_LOOP_DIRTY;
    gn_auto_push(GN_AUTO_SETTLE);
}

static uint32_t gn_pat_sig(uint32_t t)            /* a track's steps and LEN */
{
    const uint8_t *b = (const uint8_t *)trk[t].step;
    uint32_t h = 2166136261u, i;
    for (i = 0; i < sizeof trk[t].step; i++)
        h = (h ^ b[i]) * 16777619u;
    return (h ^ (uint16_t)trk[t].p[P_SLEN]) * 16777619u;
}

static uint32_t gn_pat_rest(uint32_t p)           /* the other tracks' */
{
    uint32_t t, h = 0;
    for (t = 0; t < NTRK; t++)
        if (t != p)
            h = h * 31u + gn_pat_sig(t);
    return h;
}

/* once a frame in Easy: while Easy records track p (or a key it recorded there is still held, its
 * release still to come), p's pattern moves by Easy's own hand, and nothing else may; otherwise
 * nothing may */
static void gn_undo_watch(void)
{
    uint32_t p = gn.undo_of - 1u;
    if (!gn.undo_of || p >= NTRK || gn.prov)          /* (a tap's take: put back exactly, or kept as a punch) */
        return;
    if (chain_busy() || gn_pat_rest(p) != gn.undo_rest) {
        gn_undo_drop();
        return;
    }
    if (((song.rec >> p) & 1u) || trk[p].rh_n) {
        uint32_t sig = gn_pat_sig(p);
        if (sig != gn.undo_mine)
            gn_dirty();                               /* (round 24: the recording wrote) */
        gn.undo_mine = sig;
    } else if (gn_pat_sig(p) != gn.undo_mine)
        gn_undo_drop();
}

/* Recording (round 7, pick 5; round 8). REC only arms (gn.arm) and never leaves the page; the recording itself
 * starts at the next bar 1 when the loop plays, or after PLAY's count-in (4, 3, 2, 1) when it is stopped; it
 * goes into the selected track (another track while armed: that one). KEEP adds notes (Felucca's overdub);
 * REPLACE (K4 while armed or recording) takes out of each step the notes it held before the recording began,
 * as the playhead enters it while recording, once a take (gn_rec_tick: what the take records stays). REC held
 * while the loop plays punches in until it is let go, the keys of the hold included. Undo is SAVE held. */
static int gn_is_drums(uint32_t p);
static uint32_t gn_spb(uint32_t div);

/* track p's motion events onto the grid one finer or one coarser (their places are steps), the store kept
 * valid (motion.c motion_valid): coarser, an event of the pair's first fine step wins over the same setting's
 * on the second (the second is dropped); finer, an event past the 64 steps (a coarse step from 32 on) is
 * dropped with the steps that hold it */
static void gn_motion_grid(uint32_t p, int finer)
{
    uint32_t i, j, n = 0, f = motion_guard();
    uint64_t drop = 0;                                /* bit i: event i goes (decided on the places as they are) */
    for (i = 0; i < motion.count && i < MOTION_MAX; i++) {
        const motion_event_t *e = &motion.event[i];
        uint32_t st = e->place & 63u;
        if ((e->place >> 6) != p)
            continue;
        if (finer) {
            if (st >= NSTEP / 2u)
                drop |= 1ull << i;
            continue;
        }
        for (j = 0; j < motion.count && j < MOTION_MAX; j++) {   /* the pair's first fine step wins */
            const motion_event_t *o = &motion.event[j];
            if ((o->place >> 6) == p && o->param == e->param && (o->place & 63u) / 2u == st / 2u && (o->place & 63u) < st)
                drop |= 1ull << i;
        }
    }
    for (i = 0; i < motion.count && i < MOTION_MAX; i++) {
        motion_event_t e = motion.event[i];
        if ((drop >> i) & 1u)
            continue;
        if ((e.place >> 6) == p)
            e.place = (uint8_t)(p << 6 | (finer ? (e.place & 63u) * 2u : (e.place & 63u) / 2u));
        motion.event[n++] = e;
    }
    memset(motion.event + n, 0, (MOTION_MAX - n) * sizeof motion.event[0]);
    motion.count = (uint8_t)n;
    motion_unguard(f);
}
static int gn_grid_ok_div(uint32_t div) { return div >= GN_DIV8 && div <= GN_DIV32; }
#define GN_PROV NELEM(gn.prov_step)                 /* a tap's window: the steps it may record into */
#define GN_PUNCH_MS 400u                          /* REC held this long while playing: punch in */
/* (gn.arm's GA_OFF, GA_READY, GA_COUNT: gn_state.c, round 26: the metronome's ISR reads it) */

/* the step a note played now on track p records into (seq.c rec_note: the one playing, or the next past
 * its middle), and whether the playhead is in the first half of its step */
static uint32_t gn_rec_target(uint32_t p, int *first_half)
{
    const track_t *t = &trk[p];
    uint32_t len = t->p[P_SLEN] > 0 ? (uint32_t)t->p[P_SLEN] : 1u, idx = t->seq_idx % len;
    uint32_t half = step_samples(t, div_samples((uint32_t)t->p[P_SDIV]), t->seq_idx) / 2u;
    if (!song.playing || t->seq_pos >= 0x7FFFFFFFu) {  /* stopped, or about to start: step 0 first */
        *first_half = 1;
        return 0;
    }
    *first_half = t->seq_pos <= half;
    return *first_half ? idx : (idx + 1u) % len;
}

static uint32_t gn_motion_sig(void)               /* the motion store's bytes */
{
    const uint8_t *b = (const uint8_t *)&motion;
    uint32_t h = 2166136261u, i;
    for (i = 0; i < sizeof motion; i++)
        h = (h ^ b[i]) * 16777619u;
    return h;
}

/* an undo copy of track p (steps, LEN, DIV, and the motion store with its generation: gn.mgen, which every
 * editor command that may write motion moves) for what Easy is about to do to it: a recording, step entry,
 * BARS or SNAP (each SAVE held undoes); force: afresh (a recording's take), else a copy already of this
 * track's (the same session of edits) stays */
static void gn_undo_take(uint32_t p, int force)
{
    uint32_t f;
    gn.undo_kind = (uint8_t)(force >> 4);             /* (round FLOW-1, M3: named by its latest edit) */
    if (gn.undo_of == p + 1u && !(force & 1))
        return;
    memcpy(gn.undo, trk[p].step, sizeof gn.undo);
    gn.undo_len = trk[p].p[P_SLEN];
    gn.undo_div = (uint8_t)trk[p].p[P_SDIV];
    f = motion_guard();
    gn.undo_motion = motion;
    motion_unguard(f);
    gn.undo_mgen = gn.mgen;
    gn.undo_msig = gn_motion_sig();
    gn.undo_of = (uint8_t)(p + 1u);
    gn.undo_rest = gn_pat_rest(p);
    gn.undo_mine = gn_pat_sig(p);
}

/* ---- the take (round 9). A take is one recording into one track: its snapshot is the undo copy, taken when it
 * begins; it records into the live pattern as Felucca records (seq.c rec_note, rec_hold, rec_release:
 * overdub, retriggers, chords, ties) and deletes nothing while it goes. Easy notes the steps the playhead
 * crosses while it records (the sequencer's own step, and its tick: a START, STOP or CONTINUE is a new
 * start, not a wrap) and what the panel's keys played into which step. When the take ends (REC, a punch let
 * go, a STOP, a START, another track) with REPLACE: each step it crossed keeps only what the take put there,
 * every other step is as the snapshot had it (with any note the take recorded ahead into it). KEEP: the
 * take as it recorded (overdub). The music replaced (gn_rec_cancel) ends a take with nothing applied. */
static void gn_take_begin(uint32_t p)
{
    gn.take_on = 1;
    gn.take_trk = (uint8_t)p;
    gn.take_idx = 0xFFu;
    gn.take_tick = song.playing ? song.tick : 0u;     /* round 12: begun stopped (a count-in's end), the loop
                                                       * starts from tick 0: no START to see in its first frame */
    gn.take_keys = 0;
    gn.tlog_n = gn.tlog_over = 0;
    memset(gn.crossed, 0, sizeof gn.crossed);
    memset(gn.ttie, 0, sizeof gn.ttie);
}

static int gn_bit(const uint8_t *m, uint32_t j) { return (m[(j % NSTEP) >> 3] >> (j & 7u)) & 1u; }
static void gn_bit_set(uint8_t *m, uint32_t j) { m[(j % NSTEP) >> 3] |= (uint8_t)(1u << (j & 7u)); }

/* the take's own notes at step j: the keys' (logged), and any the step holds that the snapshot's did not
 * (MIDI in, the arpeggio: whatever the panel did not see) */
static int gn_take_has(uint32_t j, uint32_t note)
{
    uint32_t i;
    for (i = 0; i < gn.tlog_n; i++)
        if (gn.tlog[i][0] == j && gn.tlog[i][1] == note)
            return 1;
    return 0;
}

static void gn_take_end(int apply)
{
    uint32_t p = gn.take_trk % NTRK, len, j, i, k;
    track_t *t = &trk[p];
    if (!gn.take_on)
        return;
    gn.take_on = 0;
    if (gn.undo_of == p + 1u && gn_pat_sig(p) != gn.undo_mine)
        gn_dirty();                                   /* (round 24: the take's last frame wrote) */
    if (!apply || !gn.replace || gn.undo_of != p + 1u)
        return;
    len = (uint32_t)clamp(t->p[P_SLEN], 1, NSTEP);
    fm1_irq_off();
    for (j = 0; j < len; j++) {
        step_t *s = &t->step[j];
        const step_t *u = &gn.undo[j];
        step_t keep = *s;
        uint32_t n = 0;
        uint8_t hit;
        if (!gn_bit(gn.crossed, j)) {                 /* not crossed: the snapshot, and the take's notes and */
            *s = *u;                                  /* hits ahead (round 10: the drums' as the notes) */
            for (i = 0; drum_track(t) && i < gn.tlog_n; i++) {
                uint32_t l = drum_lane(gn.tlog[i][1]);
                if (gn.tlog[i][0] == j && gn.tlog[i][1] == DRUM_LANE_NOTE[l] && ((keep.hit >> l) & 1u)) {
                    if (s->time != ST_NOTE)
                        s->n = 0;
                    s->time = ST_NOTE;
                    s->hit |= (uint8_t)(1u << l);
                    s->acc |= (uint8_t)(keep.acc & (1u << l));
                    if (s->vel < keep.vel)
                        s->vel = keep.vel;
                }
            }
            for (i = 0; i < keep.n && i < 4u; i++)
                if (gn_take_has(j, keep.note[i])) {
                    for (k = 0; k < s->n && s->note[k] != keep.note[i]; k++)
                        ;
                    if (s->time != ST_NOTE)
                        s->n = 0;
                    s->time = ST_NOTE;
                    if (k == s->n && s->n < 4u)
                        s->note[s->n++] = keep.note[i];
                }
            continue;
        }
        if (keep.time == ST_TIE) {                    /* crossed: a tie the take held stays, an old one ends */
            if (!gn_bit(gn.ttie, j))
                step_clear(s);
            continue;
        }
        if (keep.time != ST_NOTE) {
            step_clear(s);
            continue;
        }
        for (i = 0; i < keep.n && i < 4u; i++) {      /* the take's: logged, or not in the snapshot */
            int old = 0;
            for (k = 0; u->time == ST_NOTE && k < u->n && k < 4u; k++)
                old |= u->note[k] == keep.note[i];
            if (gn_take_has(j, keep.note[i]) || !old || gn.tlog_over)   /* (past the log: both kept) */
                s->note[n++] = keep.note[i];
        }
        hit = gn.tlog_over ? keep.hit : (uint8_t)(keep.hit & ~(u->time == ST_NOTE ? u->hit : 0u));
        for (i = 0; drum_track(t) && i < gn.tlog_n; i++)
            if (gn.tlog[i][0] == j && gn.tlog[i][1] == DRUM_LANE_NOTE[drum_lane(gn.tlog[i][1])])
                hit |= (uint8_t)(keep.hit & (1u << drum_lane(gn.tlog[i][1])));
        s->n = (uint8_t)n;
        s->hit = hit;
        s->acc &= hit;
        if (!s->n && !s->hit)
            step_clear(s);
    }
    t->rh_n = t->rh_ties = t->rskip_n = 0;
    fm1_irq_on();
    gn.undo_mine = gn_pat_sig(p);
    gn_dirty();                                       /* (round 24: REPLACE wrote) */
}

static void gn_undo_track(uint32_t p);
/* once a frame while a take goes on, and for each key the panel plays into it (notes: their edges) */
static uint32_t gn_key_note(uint32_t p, uint32_t k);
static void gn_take_tick(void)                    /* (playing or stopped: up to the step the playhead is in) */
{
    uint32_t p = gn.take_trk % NTRK, len = (uint32_t)clamp(trk[p].p[P_SLEN], 1, NSTEP), idx, g;
    if (!gn.take_on || trk[p].seq_pos >= 0x7FFFFFFFu || (!song.playing && gn.take_idx == 0xFFu))
        return;                                       /* (round 12: not yet playing: the old playhead is no step) */
    idx = trk[p].seq_idx % len;
    if (gn.take_idx == 0xFFu) {
        gn_bit_set(gn.crossed, idx);
    } else {
        for (g = 0; gn.take_idx != idx && g < len; g++) {   /* every step entered since the last frame */
            gn.take_idx = (uint8_t)((gn.take_idx + 1u) % len);
            gn_bit_set(gn.crossed, gn.take_idx);
            if (fm1_in.notes & gn.take_keys)
                gn_bit_set(gn.ttie, gn.take_idx);    /* (a key of the take's held into it) */
        }
    }
    gn.take_idx = (uint8_t)idx;
    if (song.playing)
        gn.take_tick = song.tick;
}

/* the keys the panel plays into a take (notes: their edges): each the step it records into, its note */
static void gn_take_log(uint32_t notes)
{
    uint32_t p = gn.take_trk % NTRK, k;
    int fh;
    if (!gn.take_on || !song.playing)
        return;
    for (k = 0; k < 27u && notes; k++)
        if ((notes >> k) & 1u) {
            uint32_t n = gn_key_note(p, k);
            gn.take_keys |= 1u << k;
            if (n <= 127u && gn.tlog_n < NELEM(gn.tlog)) {
                gn.tlog[gn.tlog_n][0] = (uint8_t)gn_rec_target(p, &fh);
                gn.tlog[gn.tlog_n][1] = (uint8_t)n;
                gn.tlog_n++;
            } else if (n <= 127u) {
                gn.tlog_over = 1;
            }
        }
}

/* a punch begins with keys held (round 10): their notes recorded now, as when pressed (seq.c key_on, input_on) */
static void gn_take_held(uint32_t p)
{
    uint32_t k, i, held = 0;
    if (!rec_on(&trk[p]) || trk[p].p[P_AMODE])
        return;
    fm1_irq_off();
    for (k = 0; k < 27u; k++)
        if (((fm1_in.notes >> k) & 1u) && kb_trk[k] == p && kb_note[k] != KB_SILENT && kb_chn[k]) {
            held |= 1u << k;
            for (i = 0; i < kb_chn[k]; i++)
                rec_note(&trk[p], kb_chord[k][i], 100);
        }
    fm1_irq_on();
    gn_take_log(held);
}

/* the recording begins on track p: a fresh undo copy (Easy's own last recording only, and the take's snapshot);
 * an untouched empty pattern (no notes, and no BARS or SNAP of the child's on it: gn.grid_set) gets the default
 * grid, two bars; then the track records */
static void gn_rec_go(uint32_t p, int prov)
{
    uint8_t bit = (uint8_t)(1u << p);
    uint32_t i, len;
    if (chain_busy())                                 /* a song plays (SEQ > SONG): its patterns, not ours */
        return;
    if (!prov) {
        gn_undo_take(p, 1 | GN_UK(GN_UK_TAKE));
    } else {                                          /* round 10: a tap's own snapshot, the undo copy kept */
        len = (uint32_t)clamp(trk[p].p[P_SLEN], 1, NSTEP);
        gn.prov_at = (uint8_t)(trk[p].seq_idx % len);
        gn.prov_len = trk[p].p[P_SLEN];
        gn.prov_div = (uint8_t)trk[p].p[P_SDIV];
    }
    if (seq_is_empty(&trk[p]) && !((gn.grid_set >> p) & 1u)) {
        if (!song.playing) {                          /* the default grid (pick 9: the drums snapped, a melody not) */
            fm1_irq_off();
            trk[p].p[P_SDIV] = gn_is_drums(p) ? GN_DIV16 : GN_DIV32;
            trk[p].p[P_SLEN] = (int16_t)gn_spb((uint32_t)trk[p].p[P_SDIV]);   /* (round FLOW-1: one bar, spec S-5) */
            fm1_irq_on();
        } else if (gn_grid_ok_div((uint32_t)trk[p].p[P_SDIV])) {
            trk[p].p[P_SLEN] = (int16_t)gn_spb((uint32_t)trk[p].p[P_SDIV]);   /* (playing: its grid, one bar) */
        }
        if (!prov)
            gn.undo_mine = gn_pat_sig(p);
    }
    len = (uint32_t)clamp(trk[p].p[P_SLEN], 1, NSTEP);
    for (i = 0; prov && i < GN_PROV; i++)            /* (the window on the grid it records on) */
        gn.prov_step[i] = trk[p].step[(gn.prov_at + i) % len];
    gn_take_begin(p);
    gn.prov = (uint8_t)prov;
    gn.arm = GA_OFF;
    song.rec = bit;
    if (prov)
        gn_take_held(p);
}
static void gn_rec_begin(uint32_t p) { gn_rec_go(p, 0); }

/* a tap's take put back exactly (round 10): its window as it was, LEN and DIV, the hold state cleared, all
 * with the audio IRQ off; the undo copy (the last recording's) untouched; REC armed, its release no tap */
static void gn_prov_back(void)
{
    uint32_t p = gn.take_trk % NTRK, len, i;
    if (!gn.prov)
        return;
    gn.prov = gn.punch = gn.take_on = 0;
    fm1_irq_off();
    song.rec = 0;
    len = (uint32_t)clamp(trk[p].p[P_SLEN], 1, NSTEP);
    for (i = 0; i < GN_PROV && i < len; i++)
        trk[p].step[(gn.prov_at + i) % len] = gn.prov_step[i];
    trk[p].p[P_SLEN] = gn.prov_len;
    trk[p].p[P_SDIV] = gn.prov_div;
    trk[p].rh_n = trk[p].rh_ties = trk[p].rskip_n = 0;
    fm1_irq_on();
    gn.arm = GA_READY;
    gn.rec_t0 |= 2u;
}

/* the tap's take would record past its window: the steps it crossed (from prov_at, on), with the one it
 * records ahead into and one more it may enter before the next frame, more than the window holds; a pattern
 * no longer than the window never is */
static int gn_prov_full(void)
{
    uint32_t p = gn.take_trk % NTRK, len = (uint32_t)clamp(trk[p].p[P_SLEN], 1, NSTEP), i, c = 0;
    if (len <= GN_PROV)
        return 0;
    for (i = 0; i < sizeof gn.crossed; i++)
        c += (uint32_t)__builtin_popcount(gn.crossed[i]);
    return c + 2u > GN_PROV;
}

/* a tap held on into a punch: the undo copy now (the pattern before the take: the window from the tap's
 * snapshot, LEN and DIV then; the rest as the take found it) */
static void gn_prov_keep(void)
{
    uint32_t p = gn.take_trk % NTRK, len, i;
    int16_t l2 = trk[p].p[P_SLEN];
    if (!gn.prov)
        return;
    gn.prov = 0;
    gn_undo_take(p, 1 | GN_UK(GN_UK_TAKE));
    len = (uint32_t)clamp(l2, 1, NSTEP);
    for (i = 0; i < GN_PROV && i < len; i++)
        gn.undo[(gn.prov_at + i) % len] = gn.prov_step[i];
    gn.undo_len = gn.prov_len;
    gn.undo_div = gn.prov_div;
}

/* once a frame: the take's end when the track stops recording (REC, a punch let go, a STOP: a punch's ends
 * its arming too, any other's is ready again for PLAY's count-in) or the transport starts again (its tick
 * set back: the take ends there and the arming takes the next one from bar 1); an armed track waits for its
 * bar 1 (the playhead in the second half of the last step, or past the loop's end since the last frame), the
 * count-in for its four beats */
static void gn_rec_tick(void)
{
    uint32_t p = gn_part(), len = trk[p].p[P_SLEN] > 0 ? (uint32_t)trk[p].p[P_SLEN] : 1u, idx;
    uint8_t bit = (uint8_t)(1u << p);
    int fh;
    if (gn.prov && (!song.playing || song.tick < gn.take_tick)) {   /* (a tap under way, stopped or started */
        gn_prov_back();                               /* again: put back, as a tap's) */
    }
    if (gn.take_on && song.playing && song.tick < gn.take_tick) {   /* START or CONTINUE: a new start */
        gn_take_end(1);
        song.rec = 0;
        gn.arm = gn.punch ? GA_OFF : GA_READY;
        gn.arm_idx = 0xFFu;
        gn.punch = 0;
    }
    if (gn.take_on && !song.playing && !transport_req) {   /* round 10: STOPPED, at any observation: */
        gn_take_tick();                               /* every step up to the playhead's, then the take ends */
        gn_take_end(1);
        song.rec = 0;                                 /* the loop stopped: ready again (PLAY counts in), */
        gn.arm = gn.punch ? GA_OFF : GA_READY;        /* a punch's arming gone with it */
        gn.punch = 0;
    }
    if (gn.take_on && !(song.rec & (1u << gn.take_trk))) {
        gn_take_tick();
        gn_take_end(1);                               /* (REC, a punch let go) */
    }
    gn_take_tick();
    if (gn.prov && gn_prov_full()) {                 /* a tap's window about to overflow (a stalled panel: */
        gn_prov_back();                               /* it is never a punch before 400 ms): put back, armed */
    }
    if (gn.arm == GA_COUNT) {
        if (song.playing)
            gn.arm = GA_READY;
        else if (fm1_ms - (gn.cnt_t0 & ~1u) >= gn.cnt_ms) {
            gn_rec_begin(p);
            transport_req = 1;
        }
    }
    idx = trk[p].seq_idx % len;
    if (gn.arm == GA_READY && song.playing && !chain_busy() && trk[p].seq_pos < 0x7FFFFFFFu) {
        gn_rec_target(p, &fh);
        if ((idx + 1u == len && !fh) || idx < gn.arm_idx)
            gn_rec_begin(p);
    }
    gn.arm_idx = (uint8_t)idx;
}

/* another track selected: what was armed or recording is armed on it now (bar 1 again; a count-in counts on);
 * the take ends on its own track; the undo copy was the other track's */
static void gn_retarget(void)
{
    uint32_t p = gn_part();
    gn_prov_back();                                   /* (a tap under way: its take put back, as a tap's) */
    gn_take_end(1);
    gn.arm_idx = (uint8_t)(trk[p].seq_idx % (uint32_t)clamp(trk[p].p[P_SLEN], 1, NSTEP));   /* (its bar 1 to wait for) */
    if (song.rec || gn.punch || gn.prov)
        gn.arm = GA_READY;
    song.rec = 0;
    gn.punch = gn.prov = 0;
    gn_undo_drop();
}

/* REC tapped: armed, or (armed, counting in or recording) not any more */
static void gn_rec_tap(void)
{
    if (gn.arm || song.rec) {
        song.rec = 0;
        gn_take_end(1);
        gn.arm = GA_OFF;
        return;
    }
    if (!chain_busy())
        gn.arm = GA_READY;
}

/* REC on the panel (round 9). Down while the loop plays and nothing is armed or recording: a take begins at
 * once (Felucca's own recording: every key, its hold and its release, as any recording), provisional; held
 * GN_PUNCH_MS it is a punch until let go; let go sooner it was a tap: the pattern back as it was (the keys
 * played, not recorded) and REC armed. Otherwise a tap arms (when let go) */
static void gn_rec_btn(void)
{
    uint32_t p = gn_part();
    if (gn_btn(B_REC)) {
        if (!gn.rec_t0) {
            if (gn.entry_put && p == GN_MELODY)
                gn_entry_load();                      /* (round 14: recording him: the Gnome there first) */
            gn.rec_t0 = (fm1_ms | 1u) & ~2u;
            if (song.playing && !chain_busy() && !gn.arm && !song.rec)
                gn_rec_go(p, 1);
        } else if (gn.prov && !(gn.rec_t0 & 2u) && fm1_ms - (gn.rec_t0 & ~3u) >= GN_PUNCH_MS) {
            gn.rec_t0 |= 2u;                          /* (no tap when let go) */
            gn_prov_keep();
            gn.punch = 1;
        }
        return;
    }
    if (gn.prov) {                                    /* a tap: the take put back, then armed */
        gn_prov_back();
        gn.rec_t0 = 0;
        return;
    }
    if (gn.punch) {
        gn.punch = 0;
        song.rec = 0;
        gn_take_end(1);
    }
    if (gn.rec_t0 && !(gn.rec_t0 & 2u))
        gn_rec_tap();
    gn.rec_t0 = 0;
}

/* PLAY: start and stop, from any page; armed and stopped, the count-in first (an external clock starts the
 * loop itself: no count-in, the recording waits for bar 1); during the count-in, PLAY calls it off */
static void gn_play_btn(void)
{
    if (gn.arm == GA_COUNT) {
        gn.arm = GA_READY;
        return;
    }
    if (gn.page == GP_RECORD && !song.playing && !chain_busy())
        gn.page = GP_LOOP;                            /* round UX-2 (board GN-02 A): from step entry PLAY opens the loop */
    if (gn.arm == GA_READY && !song.playing && !chain_busy() && !song.g[G_CLOCK]) {
        gn.arm = GA_COUNT;
        gn.cnt_t0 = fm1_ms | 1u;
        gn.cnt_ms = 240000u / (uint32_t)(song.g[G_BPM] > 0 ? song.g[G_BPM] : 120);   /* four beats */
        fm1_irq_off();                                /* round 26: the beat from the count-in's start (the */
        beat_pos = beat_samples() - 1u;               /* metronome's: its next block is the 1; seq_start */
        beat_n = 3u;                                  /* sets both again for the loop) */
        fm1_irq_on();
        return;
    }
    transport_req = song.playing || chain_busy() ? 2 : 1;
}

/* a track button (FX SCL ENV LFO: tracks 1 to 4): the track; SHIFT held, its MUTE on and off (round UX-4) */
static void gn_track_btn(uint32_t t)
{
    if (gn.shift)
        trk[t].p[P_MUTE] = (int16_t)!trk[t].p[P_MUTE];
    else
        track_select(t);
}

/* round FLOW-1 (a player's request: "start and stop looping from the head of a section from a button"): while the loop plays (no
 * song, no external clock: the clock owns the transport), PLAY acts when let go: a tap stops at once, as before; held
 * GN_RESTART_MS it restarts from the top at the next bar line (the band: RESTART NEXT BAR), held again before the bar,
 * now. GLO held + PLAY: stop at the bar's end (STOP AT BAR END), again: now. The audio ISR does the queued one on the
 * bar line itself (gn_state.c gn_click); nothing of the music changes. Stopped, PLAY starts on the press, as before */
#define GN_RESTART_MS 400u
static void gn_tq_set(uint32_t q)                 /* queue q for the bar line; queued already: now */
{
    if (gn.tq == q) {
        gn.tq = 0;
        transport_req = q;
    } else
        gn.tq = (uint8_t)q;
}
static void gn_play_watch(uint32_t pressed)
{
    int live = song.playing && !chain_busy() && !song.g[G_CLOCK] && gn.arm != GA_COUNT;
    if (!song.playing)
        gn.tq = 0;                                    /* (stopped, whoever stopped it: nothing queued) */
    if (pressed) {
        gn.pick_t = gn.peek_t = 0;                    /* (M2: PLAY ends the picture wheel and the sound list) */
        gn.tile_q = 0;                                /* (and a tile queued: PLAY decides now) */
        if (gn_btn(B_GLO))
            gn.glo_t0 |= 2u;                          /* (GLO used: its release is no tap) */
        gn.play_t0 = 0;
        if (live && gn_btn(B_GLO))
            gn_tq_set(GN_TQ_STOP);
        else if (live)
            gn.play_t0 = (fm1_ms | 1u) & ~2u;        /* (a tap or a hold: seen as it goes on) */
        else
            gn_play_btn();
        return;
    }
    if (!gn.play_t0)
        return;
    if (!gn_btn(B_PLAY)) {
        if (!(gn.play_t0 & 2u)) {                     /* a tap: stop now (anything queued with it) */
            gn.tq = 0;
            gn_play_btn();
        }
        gn.play_t0 = 0;
    } else if (!(gn.play_t0 & 2u) && fm1_ms - (gn.play_t0 & ~3u) >= GN_RESTART_MS) {
        gn.play_t0 |= 2u;
        gn_tq_set(GN_TQ_RESTART);
    }
}

/* SAVE held (round 7: it was REC held), and a tap of REC that began a take: the recorded track as it was
 * before Easy's recording, with the audio IRQ off (the sequencer never sees half a pattern) and the
 * recording's hold state cleared there too (seq.c rec_release, rec_hold). The motion store's part of that
 * track only while no one else may have written motion since (round 9: the generation, gn.mgen, and the store
 * as Easy left it); the other tracks' motion is never touched */
static void gn_undo_track(uint32_t p)
{
    p %= NTRK;
    gn.undid = GN_UK_NONE;                            /* (round FLOW-1, M3: what it undid, said in the band) */
    gn.undid_t = fm1_ms | 1u;
    gn_prov_back();
    song.rec = 0;
    gn.arm = GA_OFF;
    gn.punch = 0;
    gn.take_on = 0;
    if (gn.undo_of != p + 1u || chain_busy())         /* (a song's patterns are its own: STOP TO EDIT) */
        return;
    gn_undo_watch();                                  /* (anything since the last frame) */
    if (gn.undo_of != p + 1u)
        return;
    if (gn.mgen == gn.undo_mgen && gn_motion_sig() == gn.undo_msig) {
        uint32_t f = motion_guard(), i, n = 0;
        motion_restore(&trk[p]);
        for (i = 0; i < motion.count && i < MOTION_MAX; i++)   /* the other tracks' as they are, */
            if ((motion.event[i].place >> 6) != p)
                motion.event[n++] = motion.event[i];
        for (i = 0; i < gn.undo_motion.count && i < MOTION_MAX && n < MOTION_MAX; i++)   /* this one's as it was */
            if ((gn.undo_motion.event[i].place >> 6) == p)
                motion.event[n++] = gn.undo_motion.event[i];
        memset(motion.event + n, 0, (MOTION_MAX - n) * sizeof motion.event[0]);
        motion.count = (uint8_t)n;
        motion.on = (uint8_t)((motion.on & ~(1u << p)) | (gn.undo_motion.on & (1u << p)));
        motion_unguard(f);
    }
    fm1_irq_off();
    memcpy(trk[p].step, gn.undo, sizeof gn.undo);
    trk[p].p[P_SLEN] = gn.undo_len;
    trk[p].p[P_SDIV] = gn.undo_div;
    trk[p].rh_n = 0;
    trk[p].rh_ties = 0;
    trk[p].rskip_n = 0;
    fm1_irq_on();
    gn_undo_drop();
    gn.undid = gn.undo_kind ? gn.undo_kind : GN_UK_TAKE;
    gn_dirty();                                       /* (round 24: the loop as before, unsaved) */
}
static void gn_undo(void) { gn_undo_track(gn_part()); }

/* ------------------------------------------- the loop page (picks 6 to 9) --- */
/* A track's grid is its DIV: SNAP on records onto 1/16 steps (with the track's swing), SNAP off onto 1/32
 * (the finest the sequencer has: notes land within a 32nd); 8 bars only fit 64 steps at 1/8. So a track's
 * SNAP is its DIV (1/32: off) and its BARS its LEN over a bar's steps; a DIV that is none of the three (a
 * grown-up's 1/4, triplets) is not Easy's: BARS and SNAP rest. BARS and SNAP change the grid only when the
 * child turns them, never on their own: SNAP off spreads every step onto every other 32nd (nothing lost), SNAP
 * on keeps each pair's step that sounds, the first when both do (a note on an odd 32nd moves to the 16th
 * before it); the track's motion moves with its steps. A first recording into an empty pattern starts with
 * the default grid: the drums snapped (1/16, two bars), a melody not (1/32, two bars). */
static const uint8_t GN_BARS[4] = {1, 2, 4, 8};
/* what knob k does now (round 8: one map for the input and the screen, gn_knob_role) */
enum { KR_SOUND, KR_MIX, KR_LOOP, KR_MOVE, KR_KEEP, KR_BAR, KR_DRUM, KR_REST, KR_PITCH, KR_VEL, KR_PAN,
       KR_PEN, KR_HNOTE, KR_HVEL, KR_HLEN, KR_HWHEN,     /* round SEQ-1: STEPS' pen note; a held step's four */
       KR_CLICK_ON, KR_CLICK_OFF };                  /* round 26: EDIT held + K3, the metronome */

static uint32_t gn_spb(uint32_t div) { return div == GN_DIV32 ? 32u : div == GN_DIV16 ? 16u : 8u; }   /* a bar's steps */
static int gn_grid_ok(uint32_t p) { return trk[p].p[P_SDIV] >= GN_DIV8 && trk[p].p[P_SDIV] <= GN_DIV32; }
static int gn_snap(uint32_t p) { return trk[p].p[P_SDIV] != GN_DIV32; }
static uint32_t gn_bars(uint32_t p)                   /* the loop's bars on Easy's grid, rounded up */
{
    uint32_t spb = gn_spb((uint32_t)trk[p].p[P_SDIV]);
    return ((uint32_t)clamp(trk[p].p[P_SLEN], 1, NSTEP) + spb - 1u) / spb;
}

/* the drums: Easy's DRUMS picture, the DRUM engine, or SAMPLE's GM kit */
static int gn_is_drums(uint32_t p)
{
    const track_t *t = &trk[p % NTRK];
    return gn_pic_of(gn_easy_of(t)) == GN_KIT || t->eng_req == GE_DRUM ||
           (t->eng_req == GE_SAMPLE && t->p[P_E0] == GN_KIT_SET);
}

/* round UX-2: a kit's drums, each with its name and the note it plays (the drum list, step entry, the kit): the
 * Gnomad kit's sixteen identities (pad i: MIDI 36 + i), Felucca's DRUM engine's eight lanes, else Easy's sixteen pads
 * (SAMPLE's PERC set: the white keys' notes, GN_PADS) */
/* Felucca's DRUM engine on track p (with the swap its slot is the Gnomad DRUMS, gn_gnomad_kit: never) */
#define GN_FEL_DRUM(p) (!GN_ENGINES && trk[(p) % NTRK].eng_req == GE_DRUM)
static int gn_gnomad_kit(uint32_t p)
{
#if GN_ENGINES
    return gnc_engine(trk[p % NTRK].eng_req) == GC_DRUMS;
#else
    (void)p;
    return 0;
#endif
}
static uint32_t gn_kit_n(uint32_t p)
{
    if (!gn_is_drums(p))
        return 0;
    return gn_gnomad_kit(p) ? 16u : GN_FEL_DRUM(p) ? NLANE : 16u;
}
static uint32_t gn_kit_note(uint32_t p, uint32_t i)
{
    i %= gn_kit_n(p) ? gn_kit_n(p) : 1u;
    return gn_gnomad_kit(p) ? 36u + i : GN_FEL_DRUM(p) ? DRUM_LANE_NOTE[i % NLANE] : GN_PADS[i % 16u];
}
static const char *gn_kit_name(uint32_t p, uint32_t i)
{
    static const char *const LANE[NLANE] = {"KICK", "SNARE", "CLAP", "HAT", "OPEN", "TOM", "RIM", "BELL"};
    i %= gn_kit_n(p) ? gn_kit_n(p) : 1u;
    return gn_gnomad_kit(p) ? GN_GC_PAD_NAME[i % 16u] : GN_FEL_DRUM(p) ? LANE[i % NLANE] : GN_PAD_NAME[i % 16u];
}
/* the drum a note plays on track p's kit: its identity, or 0xFF none */
static uint32_t gn_kit_of(uint32_t p, uint32_t note)
{
    uint32_t i, n = gn_kit_n(p);
    if (gn_gnomad_kit(p))
        return (note + 128u - 36u) % 16u;            /* (gn_core.c: pad = (note - 36) mod 16) */
    if (GN_FEL_DRUM(p))
        return drum_lane(note);
    for (i = 0; i < n; i++)
        if (GN_PADS[i] == note)
            return i;
    return 0xFFu;
}
/* step s of track p plays drum i (its notes, and the DRUM engine's hits) */
static int gn_kit_has(uint32_t p, const step_t *s, uint32_t i)
{
    uint32_t k;
    if (s->time != ST_NOTE)
        return 0;
    for (k = 0; k < s->n && k < 4u; k++)
        if (gn_kit_of(p, s->note[k]) == i)
            return 1;
    for (k = 0; k < NLANE; k++)
        if (((s->hit >> k) & 1u) && gn_kit_of(p, DRUM_LANE_NOTE[k]) == i)
            return 1;
    return 0;
}

static int gn_step_used(const step_t *s) { return s->time == ST_TIE || (s->time == ST_NOTE && (s->n || s->hit)); }


/* track p onto the grid one finer (spread) or one coarser (merge), with the audio IRQ off; the motion of its
 * steps with it (gn_motion_grid) */
static void gn_grid_step(uint32_t p, int finer)
{
    track_t *t = &trk[p];
    uint32_t i;
    fm1_irq_off();
    if (finer) {
        for (i = NSTEP / 2u; i-- > 0;) {
            t->step[2u * i] = t->step[i];
            step_clear(&t->step[2u * i + 1u]);
            t->step[2u * i + 1u].time = ST_NOTE;       /* (an empty step: what was held goes on) */
        }
        t->p[P_SDIV]++;
        t->p[P_SLEN] = (int16_t)clamp(t->p[P_SLEN] * 2, 1, NSTEP);
        t->seq_idx = (uint16_t)((t->seq_idx * 2u) % NSTEP);
    } else {
        for (i = 0; i < NSTEP / 2u; i++)
            t->step[i] = gn_step_used(&t->step[2u * i]) || !gn_step_used(&t->step[2u * i + 1u]) ? t->step[2u * i]
                                                                                              : t->step[2u * i + 1u];
        for (i = NSTEP / 2u; i < NSTEP; i++) {
            step_clear(&t->step[i]);
            t->step[i].time = ST_NOTE;
        }
        t->p[P_SDIV]--;
        t->p[P_SLEN] = (int16_t)clamp((t->p[P_SLEN] + 1) / 2, 1, NSTEP);
        t->seq_idx = (uint16_t)(t->seq_idx / 2u);
    }
    t->rh_n = t->rh_ties = t->rskip_n = 0;
    fm1_irq_on();
    gn_motion_grid(p, finer);
    if (gn.undo_of == p + 1u)
        gn.undo_msig = gn_motion_sig();               /* (Easy's own move of the motion) */
}

/* BARS (K1 on the loop pages): 1, 2, 4 or 8, on the grid SNAP has where it fits (SNAP off: 1/32 for 1 or 2
 * bars, else the finest that fits) */
static void gn_bars_set(uint32_t p, uint32_t bars)
{
    uint32_t to = gn_snap(p) ? (bars >= 8u ? GN_DIV8 : GN_DIV16) : bars <= 2u ? GN_DIV32 : bars == 4u ? GN_DIV16 : GN_DIV8;
    while ((uint32_t)trk[p].p[P_SDIV] < to) {
        uint32_t i;
        /* Refining would discard the hidden upper half. Keep this grid until
         * it fits losslessly; shortening must never erase notes or motion. */
        for (i = NSTEP / 2u; i < NSTEP; i++)
            if (gn_step_used(&trk[p].step[i])) break;
        if (i < NSTEP) break;
        for (i = 0; i < motion.count; i++)
            if ((motion.event[i].place >> 6) == p && (motion.event[i].place & 63u) >= NSTEP / 2u) break;
        if (i < motion.count) break;
        gn_grid_step(p, 1);
    }
    while ((uint32_t)trk[p].p[P_SDIV] > to)
        gn_grid_step(p, 0);
    trk[p].p[P_SLEN] = (int16_t)clamp((int32_t)(bars * gn_spb((uint32_t)trk[p].p[P_SDIV])), 1, NSTEP);
}

/* SNAP (K3): on, the grid 1/16; off, 1/32, while the loop is 2 bars or fewer (at 1/32 64 steps are 2 bars);
 * 0: refused (more bars) */
static int gn_snap_set(uint32_t p, int on)
{
    if (on == gn_snap(p))
        return 1;
    if (on) {
        gn_grid_step(p, 0);
        return 1;
    }
    if (gn_bars(p) > 2u)
        return 0;
    while (trk[p].p[P_SDIV] < GN_DIV32)
        gn_grid_step(p, 1);
    return 1;
}

/* the loop pages' knobs (pick 7): BARS, SWING, SNAP, VOLUME of the selected track; the record screen's: MOVE
 * and NOTE / REST (stopped), SNAP, and K4 (or anywhere armed or recording) KEEP / REPLACE. BARS and SNAP
 * rest while recording or counting in, during a song, and on a grid that is not Easy's. BARS and SNAP keep
 * an undo copy as a recording does (SAVE held puts the steps, LEN, DIV and the motion back) */
static void gn_cursor_note(uint32_t p, int note);
static uint32_t gn_knob_role(uint32_t k);
#define GN_VBAR_MS 3000u                          /* the steps page follows the playhead again 3 s after K1 */
/* the bar the steps page shows (round UX-2): K1's, or while playing the playhead's (3 s after K1's last turn) */
static uint32_t gn_view_bar(uint32_t p)
{
    uint32_t spb = gn_grid_ok(p) ? gn_spb((uint32_t)trk[p].p[P_SDIV]) : 16u;
    uint32_t len = (uint32_t)clamp(trk[p].p[P_SLEN], 1, NSTEP), bars = (len + spb - 1u) / spb;
    if (song.playing && (!gn.vbar_t || gn_since(gn.vbar_t) >= GN_VBAR_MS))
        return (trk[p].seq_idx % len) / spb;
    return gn.vbar < bars ? gn.vbar : bars - 1u;
}
/* round UX-4: SHIFT on the steps and on step entry: the cursor's step, its notes a semitone up or down (pitch) or its
 * strength (the step's velocity, 4 a detent): with the session's undo copy, as step entry (SAVE held undoes); a step
 * that plays nothing is left as it is */
/* the step SHIFT's edits reach: the cursor's; on the steps page with SNAP off (two 32nds to a place) the first of the
 * cursor's place's two that plays */
static uint32_t gn_cursor_step(uint32_t p)
{
    const track_t *t = &trk[p];
    uint32_t len = (uint32_t)clamp(t->p[P_SLEN], 1, NSTEP), i = gn.cursor % len;
    if (gn.page == GP_STEPS && gn_grid_ok(p) && gn_spb((uint32_t)t->p[P_SDIV]) > 16u) {
        i &= ~1u;
        if (!(t->step[i].time == ST_NOTE && (t->step[i].n || t->step[i].hit)) && i + 1u < len)
            i++;
    }
    return i;
}

static void gn_step_edit(uint32_t p, int pitch, int32_t s)
{
    track_t *t = &trk[p];
    uint32_t k;
    step_t *st = &t->step[gn_cursor_step(p)];
    if (chain_busy() || song.rec || st->time != ST_NOTE || (!st->n && !st->hit))
        return;
    gn_undo_take(p, GN_UK(GN_UK_STEP));
    fm1_irq_off();
    if (pitch)
        for (k = 0; k < st->n && k < 4u; k++)
            st->note[k] = (uint8_t)clamp((int32_t)st->note[k] + s, 0, 127);
    else
        st->vel = (uint8_t)clamp((int32_t)st->vel + s * 4, 1, 127);
    fm1_irq_on();
    gn.undo_mine = gn_pat_sig(p);
}

#include "gn_seq.c"                               /* round SEQ-1: the sequencer's API, the page ring, the tap grid */

static void gn_loop_knob(uint32_t k, int32_t s)
{
    uint32_t p = gn_part(), r = gn_knob_role(k);
    int busy = song.rec || gn.arm || chain_busy() || !gn_grid_ok(p);
    if (r == KR_MOVE) {                               /* the record screen, stopped: MOVE, NOTE / REST */
        uint32_t len = (uint32_t)clamp(trk[p].p[P_SLEN], 1, NSTEP);
        if (k == 0u) {
            if (gn.shift && gn.page == GP_STEPS && gn_grid_ok(p) && gn_spb((uint32_t)trk[p].p[P_SDIV]) > 16u)
                s *= 2;                               /* (round UX-4: the steps page's places, two 32nds each) */
            gn.cursor = (uint8_t)(((int32_t)gn.cursor + s % (int32_t)len + (int32_t)len) % (int32_t)len);
            if (gn.shift && gn_grid_ok(p)) {         /* (round UX-4: the steps page shows the cursor's bar) */
                gn.vbar = (uint8_t)(gn.cursor / gn_spb((uint32_t)trk[p].p[P_SDIV]));
                gn.vbar_t = fm1_ms | 1u;
            }
        } else {
            gn_cursor_note(p, s > 0);
        }
        return;
    }
    if (r == KR_PITCH || r == KR_VEL) {               /* round UX-4: SHIFT, the cursor's step */
        gn_step_edit(p, r == KR_PITCH, s);
        return;
    }
    if (r == KR_PEN) {                                /* round SEQ-1: STEPS, a melody's pen note (what a key taps) */
        gn.last_note = (uint8_t)clamp((int32_t)gn_pen() + s, 24, 108);
        return;
    }
    if (r >= KR_HNOTE && r <= KR_HWHEN) {             /* .. a held step's NOTE STRENGTH LENGTH WHEN */
        gn_seq_edit(p, gn.cursor, r - KR_HNOTE, s);
        gn.hold_used = 1;
        return;
    }
    if (r == KR_DRUM) {                               /* round UX-2: K2 picks the drum, round and round */
        int32_t n = (int32_t)gn_kit_n(p);
        gn.drum = (uint8_t)(n ? (((int32_t)gn.drum + s) % n + n) % n : 0);
        return;
    }
    if (r == KR_BAR) {                                /* round UX-2: the steps page's bar switch */
        uint32_t spb = gn_grid_ok(p) ? gn_spb((uint32_t)trk[p].p[P_SDIV]) : 16u;
        int32_t bars = (int32_t)(((uint32_t)clamp(trk[p].p[P_SLEN], 1, NSTEP) + spb - 1u) / spb);
        gn.vbar = (uint8_t)clamp((int32_t)gn_view_bar(p) + s, 0, bars - 1);
        gn.vbar_t = fm1_ms | 1u;
        return;
    }
    switch (k) {
    case 0: {
        uint32_t b = gn_bars(p), i;
        for (i = 0; i < 3u && GN_BARS[i] < b; i++)
            ;
        i = (uint32_t)clamp((int32_t)i + (s > 0 ? 1 : -1), 0, 3);
        if (!busy) {
            gn.grid_set |= (uint8_t)(1u << p);        /* (the child's choice, a change or not) */
            if (GN_BARS[i] != b) {
                gn_undo_take(p, GN_UK(GN_UK_BARS));
                gn_bars_set(p, GN_BARS[i]);
                gn.undo_mine = gn_pat_sig(p);
                gn_dirty();
            }
        }
        break;
    }
    case 1:
        trk[p].p[P_SSWING] = (int16_t)clamp(trk[p].p[P_SSWING] + s * 5, 0, 60);   /* a detent 5 %, up to 60 % */
        break;
    case 2:
        if (!busy)
            gn.grid_set |= (uint8_t)(1u << p);
        if (!busy && (s > 0) != gn_snap(p)) {
            gn_undo_take(p, GN_UK(GN_UK_SNAP));
            gn_snap_set(p, s > 0);
            gn.undo_mine = gn_pat_sig(p);
            gn_dirty();
        }
        break;
    default:
        if (gn.page == GP_RECORD)
            gn.replace = s > 0;
        else
            gn_set_kept(&trk[p], P_LEVEL, trk[p].p[P_LEVEL] + s * 4);   /* VOLUME: as GLO's (put back on leaving) */
        break;
    }
}

/* the record screen, stopped (choice 10, option C): the step at the cursor holds note (a key's, or K2's
 * last); K2 down: a rest there. A key moves the cursor on; K2 writes in place (round 24: only K1, MOVE,
 * moves the cursor, so K2 turned to and fro sets one step). One undo copy for the session (the first write),
 * as for a recording */
static void gn_step_write(uint32_t p, uint32_t note, int rest, int advance)
{
    track_t *t = &trk[p];
    uint32_t len = (uint32_t)clamp(t->p[P_SLEN], 1, NSTEP), i = gn.cursor % len;
    step_t *st = &t->step[i];
    if (chain_busy())
        return;
    gn_undo_take(p, GN_UK(GN_UK_STEP));               /* (the session's copy) */
    fm1_irq_off();
    step_clear(st);
    if (!rest) {
        st->time = ST_NOTE;
        st->vel = 100;
        if (t->eng_req == GE_DRUM) {
            uint32_t l = drum_lane(note);
            if (note == DRUM_LANE_NOTE[l])
                st->hit = (uint8_t)(1u << l);
            else {
                st->n = 1;
                st->note[0] = (uint8_t)note;
            }
        } else {
            st->n = 1;
            st->note[0] = (uint8_t)note;
        }
    }
    fm1_irq_on();
    gn.undo_mine = gn_pat_sig(p);
    gn_dirty();                                       /* (round 24: step entry wrote) */
    if (advance)
        gn.cursor = (uint8_t)((i + 1u) % len);
}

static void gn_cursor_note(uint32_t p, int note)  /* K2: in place */
{
    if (gn_kit_n(p))                                  /* round UX-2: a kit: the drum K2 picked */
        gn_step_write(p, gn_kit_note(p, gn.drum), !note, 0);
    else                                              /* (round 24: K2 writes in place) */
        gn_step_write(p, gn.last_note ? gn.last_note : 60u, !note, 0);
}

/* the note key k plays on track p in Easy (gn_key's, without the Gnome's syllables); GN_KEY_REST: none */
static uint32_t gn_key_note(uint32_t p, uint32_t k)
{
    uint32_t s = gn_shown(p), w = k < 27u ? GN_WHITE[k] : 0xFFu;
    if (k >= 27u)
        return GN_KEY_REST;
    if (s >= GN_NSOUNDS)
        return gn_chromatic(&trk[p], k);
    if (GN_SOUNDS[s].flags & GNF_PADS)
        return w == 0xFFu ? gn_black_pad(&trk[p % NTRK], k) : gn_pad_note(&trk[p % NTRK], w);
    return (uint32_t)clamp(53 + (int32_t)k + 12 * (song.octave + GN_SOUNDS[s].octave), 0, 127);
}

/* keys pressed on the record screen while stopped: each written at the cursor */
static void gn_step_keys(uint32_t notes)
{
    uint32_t p = gn_part(), k;
    if (gn.page != GP_RECORD || song.playing || !notes)
        return;
    for (k = 0; k < 27u; k++)
        if ((notes >> k) & 1u) {
            uint32_t n = gn_key_note(p, k);
            if (n == GN_KEY_REST || n > 127u)
                continue;
            gn.last_note = (uint8_t)n;
            if (gn_kit_n(p) && gn_kit_of(p, n) < gn_kit_n(p))
                gn.drum = (uint8_t)gn_kit_of(p, n);  /* (round UX-2: the drum a key wrote is K2's then) */
            gn_step_write(p, n, 0, 1);
        }
}

/* ------------------------------------------------ the mode, kept (pick 1) --- */
/* Which mode the synth was left in rides in the settings record (settings_persist.c persist_t), in byte 30
 * of favourites' row 15: no engine has the number 15 (favorites.c: factory rows 0..15, NENGINES 14), so
 * unmodified 1.0.1 never reads, writes or checks it, keeps it as it was saved (settings_import and
 * settings_export copy the row whole, and its backup restore validates nothing in it), and upstream itself
 * keeps a byte of its own beside it the same way (ui_layer.c layer_seen, byte 31). The retired bold and zoom
 * fields cannot carry it: 1.0.1's backup restore refuses any value there but 0, 1 and its own tag
 * (panel.c hold_stored_ok, leds_stored_ok), and its save writes 0 over them when HOLD or LEDS is set.
 * Bits 7..4 are the marker (a record without it has not been marked by Gnomad: gn_settings_migrate),
 * bit 0 the mode (1 Easy), bit 1 reserved for the recording style "the first key starts it"
 * (pick 5, option B: the phone app's switch, which needs a protocol command; always 0 here: count-in),
 * bit 2 the metronome OFF (round 26: 0, as every record before it has it, is ON: the click while recording),
 * bit 3 zero. Nothing else is stored, and nothing is written but on the normal settings save
 * (project.c settings_save: queued, written while stopped, and only when the record changed). */
#ifdef FELUCCA_FAVORITES
#define GN_MODE_EASY 1u                   /* (GN_MODE_BYTE, GN_MODE_MARK, GN_MODE_NOCLICK: gn_state.c, round 26) */
#define GN_MODE_FIRSTKEY 2u               /* reserved (always 0: count-in) */
_Static_assert(NENGINES < 15, "favourites' row 15 is no engine's (gn_easy.c, the mode)");

static int gn_mode_marked(void) { return GN_MODE_MARKED(); }

/* Round 18: settings migration from Felucca 1.0.1. Its unused mode byte has no
 * Gnomad marker. Only the palette (where the skin exists) and this byte change;
 * all other imported preferences, including the neighbouring song bytes, stay.
 * Called once by gn_boot, never by a live settings restore. The marker and colour
 * reach flash together on the next normal settings save, not during start-up. */
static void gn_settings_migrate(void)
{
    if (gn_mode_marked())
        return;
#ifdef GSK_PAL
    settings.palette = GSK_PAL;
    palette_set(GSK_PAL);
#endif
    GN_MODE_BYTE = (uint8_t)(GN_MODE_MARK | GN_MODE_EASY);
}

/* the mode now into the record's byte (the reserved style bit and the metronome's as they are); a change of it
 * queues the normal settings save */
static void gn_mode_keep(uint32_t easy)
{
    uint8_t b = (uint8_t)(GN_MODE_MARK | (gn_mode_marked() ? GN_MODE_BYTE & (GN_MODE_FIRSTKEY | GN_MODE_NOCLICK) : 0u) |
                          (easy ? GN_MODE_EASY : 0u));
    if (!GN_START_EASY || b == GN_MODE_BYTE)
        return;
    GN_MODE_BYTE = b;
    settings_save();
}
/* every input call after the first (round 8): settings restored under the running synth (the editor's backup,
 * BACKUP_PUT of the settings) can bring another mode, or a record of other firmware with no marker; the
 * byte is made the running mode's again, the marker set, and the normal settings save queued, so the next
 * start is the mode the synth is in and the restored palette (a record without the marker: the person's
 * own) stays. A record without the marker carried no song slot of Easy's either. Nothing at start-up */
static void gn_mode_reconcile(void)
{
    if (!GN_START_EASY || !gn.booted)
        return;
    if (!gn_mode_marked()) {
        favorites.factory[15][29] = 0;                /* (no slot of Easy's: GN_SONG_BYTE) */
        GN_MODE_BYTE = (uint8_t)(GN_MODE_MARK | (gn.on ? GN_MODE_EASY : 0u));
        settings_save();
        return;
    }
    gn_mode_keep(gn.on);
}

/* Round 26: the recording's metronome (gn_state.c gn_click_on: ON unless the record says OFF).
 * Its switch (EDIT held + K3) writes the bit and queues the normal settings save, as a mode switch does: it
 * survives a power cycle */
static void gn_click_set(int on)
{
    uint8_t b;
    if (!gn_mode_marked())
        return;
    b = (uint8_t)(on ? GN_MODE_BYTE & ~GN_MODE_NOCLICK : GN_MODE_BYTE | GN_MODE_NOCLICK);
    if (b == GN_MODE_BYTE)
        return;
    GN_MODE_BYTE = b;
    settings_save();
}
#else
static int gn_mode_marked(void) { return 0; }
static void gn_mode_keep(uint32_t easy) { (void)easy; }
static void gn_mode_reconcile(void) {}
static void gn_click_set(int on) { (void)on; }
#endif

/* ------------------------------------------------- in and out of Easy --- */
/* a press under way of a button whose tap or hold would act on release (btn_hold's t0: press time
 * | 1, bit 1 = acted on): none any more. A button still down stays acted on until let go, so no tap
 * comes of it; one up starts afresh. Felucca's and Easy's, at each crossing of the mode boundary and
 * when the HOME + SAVE chord takes the panel (a REC pressed before the chord and let go during it
 * would otherwise be a tap at the next frame either side sees: review 2) */
static void gn_cancel(uint32_t *t0, uint32_t label) { *t0 = gn_btn(label) ? *t0 | 2u : 0u; }
static void gn_cancel_gestures(void)
{
    gn_cancel(&gn.rec_t0, B_REC);
    gn_cancel(&gn.home_t0, B_HOME);
    gn_cancel(&gn.glo_t0, B_GLO);
    gn_cancel(&gn.play_t0, B_PLAY);
    gn_cancel(&gn.fx_t0, B_FX);
    gn.save_t0 = gn_btn(B_SAVE) ? gn.save_t0 | 3u : 0u;
    gn_cancel(&ui.rec_t0, B_REC);
    gn_cancel(&ui.seq_t0, B_SEQ);
    gn_cancel(&ui.home_t0, B_HOME);
    gn_cancel(&ui.save_t0, B_SAVE);
    ui.pg_down = 0;                                   /* (page buttons act when let go) */
}

/* A switch from Advanced (round 13): Easy begins on the Gnome, track 1 selected, his sound put there as
 * an ALGORITHM pick puts it (Felucca's own sound load, its undo copy taken fresh; the kept settings
 * claimed by gn_set_kept); leaving with track 1 just as the entry left it (the keys' syllables aside)
 * puts track 1's own sound back (Felucca's undo swap) and the selection. Every other track keeps its
 * sound, settings and pattern, the tempo stays; Easy owns no other sound (its engine's knob map
 * moves it: gn_map). Felucca's menu, a dialog,
 * NAME and a layer close; live recording stops; the keys are Easy's (no DRUM grid, no layer's keys, no
 * FX layer effects: gn_upkeep). */
/* round UX-4: one page identity in both depths. Felucca's page as Easy is entered says where Easy opens (its STEP
 * page: the steps, at its cursor; PATTERN or PHRASES: the loop; the mixer or GLO: the volumes; a sound's pages: the
 * sound; anything else: Home), and Easy's page as it is left says which of Felucca's opens (the steps and step entry:
 * STEP at Easy's cursor; the loop: PATTERN; the volumes: the mixer; Home and the sound: HOME). The selected track is
 * the same object in both (song.sel); only an entry that lands on Home shows the Gnome on track 1 (round 13) */
static uint32_t gn_page_from_felucca(void)
{
    const page_t *pg;
    if (ui.home || ui.menu || ui.confirm)
        return GP_HOME;
    pg = cur_page();
    switch (pg->fam) {
    case FAM_SEQ:
        return pg->graph == GR_ROLL ? GP_STEPS : GP_LOOP;
    case FAM_TRK: case FAM_GLO:
        return GP_MIX;
    case FAM_ENV: case FAM_LFO: case FAM_FX: case FAM_EDIT: case FAM_ARP: case FAM_SCL:
        return GP_PLAY;
    default:
        return GP_HOME;
    }
}

static void gn_page_to_felucca(uint32_t page)
{
    uint32_t i;
    if (page == GP_STEPS || page == GP_RECORD || page == GP_LOOP) {
        for (i = page_first(FAM_SEQ); i < NPAGES && PAGES[i].fam == FAM_SEQ &&
                                      (PAGES[i].graph == GR_ROLL) != (page != GP_LOOP); i++)
            ;
        ui.page = (uint8_t)(i < NPAGES && PAGES[i].fam == FAM_SEQ ? i : page_first(FAM_SEQ));
        ui.home = 0;
        if (page != GP_LOOP)
            cursor_set((int32_t)gn.cursor);
        page_entered();
    } else if (page == GP_MIX) {
        ui.page = (uint8_t)page_first(FAM_TRK);
        ui.home = 0;
        page_entered();
    } else {
        go_home();
    }
}

static void gn_enter(void)
{
    uint32_t from = gn_page_from_felucca();           /* (round UX-4: before anything of Felucca's closes) */
    gn.booted |= 2u;
    panic_req |= (1u << NTRK) - 1u;
    gn_mode_keep(1);
    gn.on = 1;
    gn.adopt = 0;
    gn.auto_dirty = 0;
    gn_undo_drop();
    gn_disown_all();
    RING_PUBLISH();
    gn_kept_retire_all();
    gn_cancel_gestures();
    song.rec = 0;
    song.seq_mode = 0;
    if (ui.menu)
        menu_close();
    ui.menu = 0;
    ui.confirm = 0;
    name_close();
    ui.ly = 0;                                        /* ui_layer.c: no layer armed or shown */
    ui.ly_t0 = 0;
    ui.layer = 0;
    ui.msg_t = 0;                                     /* (no message of Felucca's left to show later) */
    ui.msg2[0] = 0;
    gn.last_sel = song.sel;
    gn.page = (uint8_t)from;                          /* (round UX-1: Home; UX-4: Felucca's page's own) */
    if (from == GP_STEPS) {                           /* (its step: Felucca's STEP page's cursor) */
        gn.cursor = (uint8_t)(ui.cursor % NSTEP);
        gn.vbar = (uint8_t)(gn_grid_ok(song.sel % NTRK) ? gn.cursor / gn_spb((uint32_t)TSEL->p[P_SDIV]) : 0u);
        gn.vbar_t = fm1_ms | 1u;
    }
    gn.tap_t = gn.save_pend = 0;
    gn.arm = GA_OFF;
    gn.punch = 0;
    gn.grid_set = 0;
    gn.prov = 0;
    gn.take_on = 0;
    gn.shift = 0;                                     /* (round UX-3: no SHIFT carried over) */
    gn.edit_t0 = 0;
    gn.boot_t0 = fm1_ms | 1u;                         /* the start-up picture (3i) until a key or 3 s */
    gn.pick_t = gn.saved_t = 0;
    gn.exit_y = 0;
    gn.replace = 0;                                   /* (round 13: every session's recording starts KEEP) */
    gn.entry_sel = song.sel;                          /* round 14: Easy begins on the Gnome, track 1, shown */
    gn.entry_put = 0;                                 /* (gn_shown) and put there only when he is first */
    gn.entry_keys = 0;                                /* used: a key, a knob, PRESETS, ARP, REC on track 1 */
    if (!chain_busy() && from == GP_HOME) {           /* (gn_entry_load). Until then track 1, its motion */
        uint32_t k;                                   /* and Felucca's undo copy are not touched at all */
        track_select(GN_MELODY);
        gn.last_sel = song.sel;
        gn.entry_put = 1;
        gn.entry_sig = track_sig(&trk[GN_MELODY]);
        for (k = 0; k < 4u; k++)                      /* (his bubbles at rest meanwhile) */
            gn.amount[GN_MELODY][k] = gn_snd_knob(&GN_SOUNDS[GN_GROUP[0]], k)->rest;
    }
}

/* round 14: the entry's Gnome, put on track 1 now that he is used (Felucca's own sound load: its undo copy, taken
 * fresh, is track 1 as it was before Easy, so SAVE held in Advanced gives that back); the keys that asked for him
 * sound once he is there (gn_entry_keys). Track 1 changed meanwhile (the app, a project, a song): no Gnome, the
 * track as it is, and (round 15) the keys sound on it all the same */
static void gn_entry_load(void)
{
    if (!gn.entry_put)
        return;
    if (chain_busy() || track_sig(&trk[GN_MELODY]) != gn.entry_sig) {
        gn.entry_put = 0;
        return;
    }
    gn_sound_put(GN_MELODY, GN_GROUP[0]);             /* (clears entry_put, a fresh undo copy) */
}

/* the keys that asked for the entry's Gnome, as pressed (seq.c key_on, the audio IRQ off), once track 1's sound is
 * there: no engine switch or fade under way (voice.c), and (round 15) the release a sound load asks of the audio
 * (seq.c panic_req, the load's own or the app's) done, so it cannot end the key it follows */
static void gn_entry_keys(void)
{
    track_t *t = &trk[GN_MELODY];
    uint32_t k, keys = gn.entry_keys;
    if (gn.entry_put || !keys || t->xf_on || t->eng_req != t->engine || ((panic_req >> GN_MELODY) & 1u))
        return;
    gn.entry_keys = 0;
    for (k = 0; k < 27u; k++)
        if (((keys >> k) & 1u) && ((fm1_in.notes >> k) & 1u) && kb_trk[k] == GN_MELODY) {
            fm1_irq_off();
            kb_note[k] = (uint8_t)gn_key(t, k, kb_map(t, k));
            if (kb_note[k] != KB_SILENT)
                key_on(k, t);
            fm1_irq_on();
        }
}

/* back to Felucca's pages, on HOME. The sounds Easy put stay where they are, but are no longer Easy's
 * (Advanced may change them in any way: the next entry starts with none). The track settings a sound
 * load keeps that Easy's sounds set (GN_KEPT: level, the arpeggiator, chord keys, scale snap,
 * transpose) go back to their baselines, each one Easy still has a claim to (no one else wrote it
 * since Easy did, the same value or not; no project or backup replaced the tracks: gn_editor) and that
 * still holds what Easy wrote: so Felucca's SAVE-held undo of the last pick, which puts back the sound
 * but never these, gives the track back as it was. The screen is cleared to Felucca's ground a strip a
 * frame (gn_draw), then Felucca draws all of it. */
static void gn_leave(void)
{
    uint32_t t, k;
    int entry;
    panic_req |= (1u << NTRK) - 1u;
    gn_prov_back();                                   /* (a tap's take put back) */
    entry = gn.entry_put;                             /* (the Gnome never used: nothing of track 1 to put back) */
    gn.entry_put = 0;
    gn.entry_keys = 0;
    gn_wob_back_all();
    gn_mode_keep(0);
    gn.on = 0;
    gn.shift = 0;
    gn.edit_t0 = 0;
    gn.tq = gn.tile_q = 0;                            /* (round FLOW-1: Advanced's transport is Felucca's own) */
    gn.adopt = 0;
    gn.auto_dirty = 0;                                /* (round 24: Advanced's music is Advanced's to save) */
    gn.save_pend = 0;
    gn.arm = GA_OFF;
    gn.punch = 0;
    gn.grid_set = 0;
    gn.prov = 0;
    gn.take_on = 0;
    gn_undo_drop();
    gn_disown_all();
    RING_PUBLISH();
    for (t = 0; t < NTRK; t++) {
        for (k = 0; k < NELEM(GN_KEPT); k++)
            if (((gn.kept_mask[t] >> k) & 1u) && trk[t].p[GN_KEPT[k]] == gn.kept_wrote[t][k])
                trk[t].p[GN_KEPT[k]] = gn.kept_was[t][k];
        gn.kept_mask[t] = 0;
    }
    if (entry && song.sel == GN_MELODY && gn.entry_sel < NTRK)
        song.sel = gn.entry_sel;                      /* (the selection, as Easy found it) */
    gn_cancel_gestures();
    song.rec = 0;
    ui.msg_t = 0;                                     /* (what Felucca said while Easy had the screen is old, */
    ui.msg2[0] = 0;                                   /* and so is what ui_notices would say now) */
    midi_hint = 0;
    motion_full = 0;
    gn.exit_y = 0;
    gn_page_to_felucca(gn.page);                      /* (round UX-4: the page's own; Home and the sound: HOME) */
}

/* every frame Easy has the panel: what Felucca's ui_input does first (FM6's patch after PTCH, a
 * DIGITAL sound to FM6), and the keys kept Easy's */
static void gn_upkeep(void)
{
    uint32_t k;
    for (k = 0; k < NELEM(gn.pad_ms); k++)             /* (a pad's light over: its time cleared, before 16 bits wrap) */
        if (gn.pad_ms[k] && (uint16_t)(fm1_ms - gn.pad_ms[k]) >= 200u)
            gn.pad_ms[k] = 0;
    for (k = 0; k < NTRK; k++)                         /* (Home's rows: the same) */
        if (gn.live_ms[k] && (uint16_t)(fm1_ms - gn.live_ms[k]) >= 200u)
            gn.live_ms[k] = 0;
    fm6_poll();
#if !FELUCCA_FM4
    for (k = 0; k < NTRK; k++)
        if (trk[k].eng_req == ENGI_DIGITAL)
            fm4_track(&trk[k]);
#endif
    song.grid = 0;                                    /* seq.c: no DRUM grid, no NAME */
    kb_mask = 0;                                      /* seq.c: no button makes the keys a layer's */
    perf_mask = 0;
    perf_kill = 1;                                    /* perform.c: no FX layer effect */
    (void)k;
}

/* ------------------------------------------------------------- the hold --- */
/* HOME + SAVE held together for 2 s without a break switch Easy and Advanced. Either button up, or
 * another button down, ends the hold (the count starts again with the next chord of the two alone).
 * It counts only on presses Felucca has not acted on: Felucca's own holds of these buttons (HOME
 * held 0.7 s: the menu; SAVE held 0.7 s: undo of the last sound load) and its taps (HOME: the HOME
 * page; SAVE: the SAVE pages) act through btn_hold (ui_input.c), which marks a press it acted on with
 * bit 1 of its t0 and never acts on a press so marked. When the second of the two goes down before
 * either has fired, both presses are marked at once, before Felucca's ui_input sees that frame: then
 * neither is a tap or a hold for Felucca, the hold runs, and Felucca keeps the rest of the panel. A
 * press already acted on (the menu is open, the undo happened) never starts it: let go and press again.
 * The other held buttons of Felucca (SEQ: SONG; FX GLO SCL EDIT: the layers; OCT- + OCT+: the update
 * mode) are not these two, and with any of them down the hold does not count. */
static void gn_mark(void)
{
    if (gn_btn(B_HOME) && !(gn.sw_mark & 1u)) {
        ui.home_t0 |= 2u;
        gn.sw_mark |= 1u;
    }
    if (gn_btn(B_SAVE) && !(gn.sw_mark & 2u)) {
        ui.save_t0 |= 2u;
        gn.sw_mark |= 2u;
    }
    gn.home_t0 |= 2u;                                 /* nor a tap for Easy's HOME (back to playing) */
    gn.save_t0 |= 3u;                                 /* nor for Easy's SAVE (the song, undo) */
    gn_cancel(&gn.rec_t0, B_REC);                     /* the chord has the panel: no REC, SEQ or page */
    gn_cancel(&ui.rec_t0, B_REC);                     /* press under way becomes a tap later (review 2) */
    gn_cancel(&ui.seq_t0, B_SEQ);
    ui.pg_down = 0;
}

static void gn_unmark(void)                       /* a press let go is no longer the one Easy marked */
{
    if (!gn_btn(B_HOME))
        gn.sw_mark &= (uint8_t)~1u;
    if (!gn_btn(B_SAVE))
        gn.sw_mark &= (uint8_t)~2u;
}

static int gn_chord(void)                         /* HOME and SAVE down, nothing else */
{
    uint32_t both = (1u << panel.btn[B_HOME]) | (1u << panel.btn[B_SAVE]);
    return (fm1_in.buttons & 0x3FFFu) == both;
}

/* Easy off: the hold that enters it, while Felucca keeps the panel and the screen. 1 = Easy has
 * just been entered */
static int gn_hold_watch(void)
{
    gn_unmark();
    if (!gn_btn(B_HOME) || !gn_btn(B_SAVE) || !gn_chord()) {
        gn.sw_t0 = 0;                                 /* either up, or another button: the hold breaks */
        return 0;
    }
    if (!gn.sw_t0) {
        if (!gn_chord() || ((ui.home_t0 & 2u) && !(gn.sw_mark & 1u)) || ((ui.save_t0 & 2u) && !(gn.sw_mark & 2u)))
            return 0;                                 /* another button, or a press Felucca has acted on */
        gn_mark();
        gn.sw_t0 = fm1_ms | 1u;
        return 0;
    }
    gn_mark();
    if (gn_since(gn.sw_t0) < GN_HOLD_MS)
        return 0;
    gn.sw_done = 1;
    gn.sw_to_easy = 1;
    gn_enter();
    return 1;
}

/* Easy on: the hold that leaves (3h shows it filling), and the rest of the one that entered. While
 * the counting chord is down nothing else reacts. 1 = the panel is the hold's this frame */
static int gn_switch(void)
{
    gn_unmark();
    if (gn.gest_lock) {                               /* round 10: the music replaced during the hold: no */
        gn.sw_t0 = 0;                                 /* switch until every button is up (gn_input) */
        gn.sw_done = 0;
        return 0;
    }
    if (!gn_btn(B_HOME) || !gn_btn(B_SAVE)) {
        gn.sw_t0 = 0;
        gn.sw_done = 0;
        return 0;
    }
    if (!gn_chord()) {
        gn.sw_t0 = 0;                                 /* another button: the hold breaks (that button acts) */
        return 0;
    }
    if (!gn.sw_t0) {
        gn_mark();
        gn.sw_t0 = fm1_ms | 1u;
        gn.sw_done = 0;
        gn.sw_to_easy = !gn.on;
    } else if (!gn.sw_done && gn_since(gn.sw_t0) >= GN_HOLD_MS) {
        gn.sw_done = 1;
        if (gn.on)
            gn_leave();
    }
    gn_mark();
    fm1_input_edges(0);                               /* while both are down, the presses are the hold's */
    fm1_input_note_edges();
    enc_drop();
    return 1;
}

/* the first main-loop frame (pick 1): the first power-on (no record of ours: gn_mode_marked) starts in
 * Easy, and GNOMAD is the palette until someone picks another (where the skin is built in: GSK_PAL); after
 * that, the mode the record keeps. Nothing is saved here: the first power-on's byte is in RAM only, written
 * with the next settings save (a mode switch, a palette, a favourite). GN_START_EASY 0: nothing at all.
 * Round 27 (F15): the last save, while flash still holds that write (gn_rec_slot), decides: the mode it was saved
 * from (the byte in RAM only) and that project loaded, stopped. No such record: the mode left in, as before, and in
 * Easy its own song where a round 25/26 record holds one */
static void gn_boot_mode(void);
static void gn_slots_from_flash(void);
static void gn_boot(void)
{
    gn.booted |= 1u;
#if GN_ENGINES
    gn_power_on_sounds();                             /* round FW-CONTENT-8: before the entry's Gnome is decided */
#endif
    if (!GN_START_EASY)
        return;
    gn_slots_from_flash();                            /* round 25: the slot list is the flash's */
    gn_boot_mode();                                   /* the mode, and the last save loaded (round 27) */
}

/* ------------------------------------------------------- the panel's buttons (pick 2) --- */
/* ARP: the selected track's arpeggio on or off, while it holds Easy's sound (another sound's arpeggiator is
 * its own: ARP rests). On: the sound's own (the Laser's: up, 1/16, two octaves), else up, 1/16, one octave;
 * each a write of Easy's to a setting a sound load keeps (gn_set_kept: put back on leaving) */
static void gn_arp_toggle(void)
{
    track_t *t = TSEL;
    const gn_sound_t *d;
    if (gn.entry_put && gn_part() == GN_MELODY)
        gn_entry_load();
    d = gn_sound_def(gn_easy_of(t));
    int32_t v[4] = {1, 2, 1, 64};
    uint32_t k, i;
    if (!d)
        return;
    if (t->p[P_AMODE]) {
        gn_set_kept(t, P_AMODE, 0);
        return;
    }
    for (k = 0; k < d->nset; k++)
        for (i = 0; i < 4u; i++)
            if (d->set[k].id == P_AMODE + i)
                v[i] = d->set[k].v;
    for (i = 0; i < 4u; i++)
        gn_set_kept(t, P_AMODE + i, i == 0 && !v[0] ? 1 : v[i]);
}

/* SAVE: acts when let go: a tap saves the song; a hold of 0.7 s or more is Easy's undo (gn_undo, from round
 * 7: it was REC held). Never when HOME was down at any moment of the press (the HOME + SAVE chord's: gn_mark
 * and gn_cancel_gestures spoil it too, bit 1), so the hold to Advanced saves and undoes nothing */
static uint32_t gn_save_btn(void)
{
    uint32_t r;
    if (gn_btn(B_SAVE)) {
        if (!gn.save_t0)
            gn.save_t0 = (fm1_ms | 1u) & ~2u;
        if (gn_btn(B_HOME))
            gn.save_t0 |= 2u;
        return BT_NONE;
    }
    r = !gn.save_t0 || (gn.save_t0 & 2u) ? BT_NONE : fm1_ms - (gn.save_t0 & ~3u) >= 700u ? BT_HOLD : BT_TAP;
    gn.save_t0 = 0;
    return r;
}

/* the song's slot (round 9: owned only by Easy's own last write of it, never inferred). Easy keeps, beside the
 * mode byte in favourites' row 15 (gn_easy.c, the mode), the slot it last saved its song to (byte 29:
 * 0x80 | slot), the storage's commit sequence of that write (bytes 25 to 28: storage.c st_hdr_t seq, which
 * every later write of the slot moves, by anyone, in Easy or in Advanced, the same bytes or not) and a 64-bit
 * fingerprint of the project's payload as written (bytes 17 to 24: FNV-1a over all but its own trailing
 * checksum). The slot is Easy's while the flash's current copy has that sequence and that payload: any save,
 * load-and-save, rename or restore of it ends it. Easy never infers ownership otherwise: unsure, SAVE takes an
 * empty slot, else says FULL.
 * Round 27 (F15): the record is of the LAST save, whoever made it (gn_project_saved: Advanced's SAVE, the app's),
 * and power-on restores it in the mode it was saved from (gn_boot). Byte 29's bit 4 says round 27 wrote it, bit 5
 * that it was saved in Advanced, bit 6 that it is not Easy's own song (Advanced's or the app's): Easy owns the slot
 * only while bits 5 and 6 are clear, so its SAVE and autosave never write a save of someone else's, nor an older
 * song of its own once another save came after it (the record moved on). A round 25/26 record (bit 4 clear: 0x80 |
 * slot) starts as it did: in the mode the synth was left in, Easy's song loaded only in Easy */
#define GN_SONG GN_NAME " SONG"
#define GN_NO_SLOT 0xFFu
#define GN_SONG_R27 0x10u                         /* byte 29: round 27's record of the last save */
#define GN_SONG_ADV 0x20u                         /* .. saved in Advanced: power-on starts there */
#define GN_SONG_OTHER 0x40u                       /* .. not Easy's own song: Easy never writes it */
#ifdef FELUCCA_FAVORITES
#define GN_SONG_BYTE (favorites.factory[15][29])
#define GN_SONG_SEQ (&favorites.factory[15][25])
#define GN_SONG_FP (&favorites.factory[15][17])
#endif
static uint64_t gn_fp64(const uint8_t *b)         /* a project's payload: all but the trailing 4-byte sum */
{
    uint64_t h = 14695981039346656037ull;
    uint32_t i;
    for (i = 0; i + 4u < sizeof(project_store_t); i++)
        h = (h ^ b[i]) * 1099511628211ull;
    return h;
}

/* slot's current flash copy: its commit sequence and fingerprint; 0: none readable */
static int gn_slot_now(uint32_t slot, uint32_t *seq, uint64_t *fp)
{
#if FELUCCA_FLASH
    st_hdr_t h;
    if (!flash_ok || st_current(OBJ_PROJECT0 + (slot & 3u), &h) < 0 || h.len != sizeof(project_store_t))
        return 0;
    *seq = h.seq;
    *fp = gn_fp64(st_buf);
    return 1;
#else
    (void)slot;
    (void)seq;
    (void)fp;
    return 0;
#endif
}

static uint32_t gn_rd32(const uint8_t *b) { return (uint32_t)b[0] | (uint32_t)b[1] << 8 | (uint32_t)b[2] << 16 | (uint32_t)b[3] << 24; }

static uint32_t gn_rec_slot(void)                 /* the record's slot, still that write on flash; GN_NO_SLOT: none */
{
#ifdef FELUCCA_FAVORITES
    uint32_t slot = GN_SONG_BYTE & 3u, seq;
    uint64_t fp;
    if (!gn_mode_marked() || (GN_SONG_BYTE & 0x8Cu) != 0x80u || !project_used(slot) || !gn_slot_now(slot, &seq, &fp))
        return GN_NO_SLOT;
    return seq == gn_rd32(GN_SONG_SEQ) && (uint32_t)fp == gn_rd32(GN_SONG_FP) &&
           (uint32_t)(fp >> 32) == gn_rd32(GN_SONG_FP + 4) ? slot : GN_NO_SLOT;
#else
    return GN_NO_SLOT;
#endif
}

static uint32_t gn_song_owned(void)               /* Easy's slot, still its own last write; GN_NO_SLOT: none */
{
#ifdef FELUCCA_FAVORITES
    return GN_SONG_BYTE & (GN_SONG_ADV | GN_SONG_OTHER) ? GN_NO_SLOT : gn_rec_slot();
#else
    return GN_NO_SLOT;
#endif
}

static uint32_t gn_song_slot(void)
{
    uint32_t i = gn_song_owned();
    if (i != GN_NO_SLOT)
        return i;
    for (i = 0; i < 4u; i++)
        if (!project_used(i))
            return i;
    return GN_NO_SLOT;
}

/* the slot Easy has just written: its number, sequence and fingerprint into the record's bytes and the record
 * saved (SAVE runs only while stopped: written now) and read back. 1: the record on flash holds them; 0: it
 * does not (the bytes then say no slot, so nothing is claimed; the retry, if any, writes that) */
#ifdef FELUCCA_FAVORITES
static int gn_rec_write(uint32_t slot, uint32_t bits)   /* the record: slot's write now, bits 5 6; 0: unreadable */
{
    uint32_t seq, i;
    uint64_t fp;
    if (!gn_slot_now(slot, &seq, &fp))
        return 0;
    for (i = 0; i < 4u; i++) {
        GN_SONG_SEQ[i] = (uint8_t)(seq >> (8u * i));
        GN_SONG_FP[i] = (uint8_t)(fp >> (8u * i));
        GN_SONG_FP[4u + i] = (uint8_t)(fp >> (32u + 8u * i));
    }
    GN_SONG_BYTE = (uint8_t)(0x80u | GN_SONG_R27 | bits | (slot & 3u));
    settings_save();
    return 1;
}
#endif

static int gn_song_keep(uint32_t slot)
{
#ifdef FELUCCA_FAVORITES
    if (!GN_START_EASY || !gn_rec_write(slot, 0))
        return 0;
#if FELUCCA_FLASH
    {   /* the record read back from flash (into st_buf: no copy of it on the stack) */
        st_hdr_t h;
        if (!persist_pending && flash_ok && st_current(OBJ_SETTINGS, &h) >= 0 && h.len == sizeof(persist_t) &&
            !memcmp(st_buf + __builtin_offsetof(persist_t, favorites.factory[15][17]), &favorites.factory[15][17], 13))
            return 1;
    }
#endif
    GN_SONG_BYTE = 0;                                 /* (not on flash: not Easy's) */
    return 0;
#else
    (void)slot;
    return 0;
#endif
}

/* the song as it is now into its slot, once the transport has stopped (a flash erase silences the audio:
 * project_save_as saves only while stopped; SAVE stops the loop first). What was saved is what is kept:
 * every claim of Easy's to put a setting back ends (as an editor save, gn_project). The screen says SAVED
 * (the Gnome) once the save is read back from flash the same; FULL when no slot is free */
/* the song as it is now into Easy's slot (the transport stopped): 1 SAVED (read back, and Easy's record of it),
 * 2 FULL (no slot Easy may write), 3 NOT SAVED, 4 saved but Easy's record of it not. SAVE's (retire: every claim
 * ends, what the child saved is kept) and the autosave's (round 24: claims stay, leaving still puts Easy's settings
 * back; the slot holds the music as Easy plays it) */
static uint32_t gn_save_now(int retire)
{
    uint32_t slot = gn_song_slot(), r;
    if (slot == GN_NO_SLOT)
        return 2;
    gn.save_note = GN_NOTE_EASY;                      /* (round 27: its own record, below, not the hook's) */
    r = (uint32_t)project_save_as(slot, GN_SONG);
    gn.save_note = 0;
    if (r)
        return 3;
    gn.auto_dirty = 0;                                /* (the loop is in a slot now) */
    if (retire)
        gn_kept_retire_all();
    if (!gn_saved_verified(slot))
        return 3;                                     /* (NOT SAVED: the flash does not hold it) */
    gn_saved_now();                                   /* (round UX-5: read back the same: the strip says SAVED) */
    if (!gn_song_keep(slot))
        return 4;                                     /* (a plain warning: the next power-on will not find it) */
    gn.auto_dirty = GN_LOOP_SAVED;                    /* SAVED, SAFE TO UNPLUG: and the header's mark (round 25) */
    return 1;
}

static void gn_save_song(void)
{
    if (transport_busy()) {
        if (song.playing && !chain_busy())
            transport_req = 2;
        return;                                       /* (save_pend: again next frame) */
    }
    gn.save_pend = 0;
    gn.saved_mine = (uint8_t)gn_save_now(1);
    gn.saved_t = fm1_ms | 1u;
    if (gn.auto_dirty == GN_LOOP_DIRTY)
        gn.auto_dirty = 0;                            /* (round 24: SAVE said what it said; no autosave to say it again) */
}

/* round 27: a save not Easy's own (gn_state.c gn_project_saved: Advanced's SAVE, the app's) is the last save now: its
 * record written (its slot, sequence and payload; saved in Advanced or not; not Easy's own) with the normal settings
 * save, at the next input call of either mode. A slot that does not read back: no record (power-on then starts in
 * the mode the synth was left in, nothing loaded) */
static void gn_last_keep(void)
{
    uint32_t n = gn.save_note;
    gn.save_note = 0;
#ifdef FELUCCA_FAVORITES
    if (!gn_mode_marked())
        return;
    if (!gn_rec_write(n & 3u, GN_SONG_OTHER | (n & GN_NOTE_ADV ? GN_SONG_ADV : 0u))) {
        GN_SONG_BYTE = 0;
        settings_save();
    }
#else
    (void)n;
#endif
}

/* round 24: once a frame, the loop Easy wrote saved once it is stopped and settled (gn_dirty). A save
 * that worked says nothing (the child goes on); FULL, NOT SAVED or Easy's record not kept show as SAVE's do,
 * once: the loop is then no longer marked until Easy writes it again (no retry every 10 s) */
static void gn_autosave(void)
{
    uint32_t r;
    if (gn.auto_dirty != GN_LOOP_DIRTY)
        return;
    if (transport_busy() || chain_busy() || song.rec || gn.arm || gn.take_on || gn.prov || gn.save_pend ||
        gn.sw_t0 || fm1_in.notes || (fm1_in.buttons & 0x3FFFu)) {
        gn_auto_push(GN_AUTO_SETTLE);                 /* (playing, recording, held: 4 s after it ends) */
        return;
    }
    if ((int16_t)(uint16_t)(gn_t16() - gn.auto_t) < 0)
        return;
    gn.auto_dirty = 0;
    gn.auto_t = (uint16_t)(gn_t16() + GN_AUTO_GAP);
    r = gn_save_now(0);
    if (r != 1u) {
        gn.saved_mine = (uint8_t)r;
        gn.saved_t = fm1_ms | 1u;
    }
}

/* round 25 (a report on V1.1): the power-on in Easy starts with Easy's last saved song. Felucca's power-on
 * (main.c felucca_init) puts its default tracks up and never loads a project: a song SAVE wrote and read back was
 * on flash but no Easy player could find it. Now, at the first main-loop frame (gn_boot, nothing earlier), when
 * the synth starts in Easy and Easy's record of its slot holds (gn_song_owned: the slot, its commit sequence and
 * the fingerprint of the project as Easy wrote it, all checked against flash), that slot is loaded (project.c
 * project_load: the transport stopped, no autoplay) and each track's sound, if it is one of Easy's 49, is Easy's
 * again with its knobs where the song left them (gn_reclaim). Nothing is written. A synth left in Advanced starts
 * as Felucca does; so does a slot someone else has written since (the record no longer holds).
 * Round 27 (F15): the slot is the last save's, whoever made it (gn_boot_mode); in Advanced it is Felucca's own load
 * (its LOADED shown); in Easy the mark only for Easy's own song */
static void gn_reclaim(uint32_t p)
{
    track_t *t = &trk[p % NTRK];
    uint32_t i, k, best = GN_NOSND, bs = 0;
    p %= NTRK;
    for (i = 0; i < GN_NSOUNDS; i++) {               /* the sound of its engine and preset whose own settings it */
        const gn_sound_t *s = &GN_SOUNDS[i];          /* holds most (the Gnome against Vox lead: his MONO, his TALK) */
        const engine_t *e = ENGINES[s->engine % NENGINES];
        uint32_t sc = 1;
        if (s->engine != t->eng_req || t->user || !e->npresets || t->preset != preset_orig(e, s->preset % e->npresets))
            continue;
        if (t->eng_req == GE_SAMPLE && (i == GN_SND_KIT) != (t->p[P_E0] == GN_KIT_SET))
            continue;                                 /* (the kit is SAMPLE's PERC set; another SAMPLE sound is not) */
        for (k = 0; k < s->nset; k++)
            sc += (uint32_t)(t->p[s->set[k].id] == s->set[k].v);
        if (sc > bs) {
            bs = sc;
            best = i;
        }
    }
    if (best == GN_NOSND)
        return;
    gn.own_eng[p] = t->eng_req;
    gn.own_pre[p] = t->preset;
    gn.own_usr[p] = t->user;
    gn.own_id[p] = gn_id_param(t->eng_req) < P_COUNT ? t->p[gn_id_param(t->eng_req)] : 0;
    gn.own_gen[p] = fm6_pgen[p];
    for (k = 0; k < 4u; k++)
        gn.wrote[p][k] = -32767 - 1;                  /* (the bubbles: where the song's values are, gn_knob_follow) */
    RING_PUBLISH();
    gn.own[p] = (uint8_t)(best + 1u);
    gn_knob_follow(p);
}

/* round 25: the four project slots as flash holds them. persist_boot (project.c) keeps a .noinit RAM copy of a slot
 * whenever it still reads as a valid project, and fetches from flash only the ones that do not: its own comment says
 * "the flash copy wins at boot", and it does only if RAM is garbage. A RAM copy that survived the power-off (or a
 * reset that never cut it) but is older than the slot on flash would hide the newer song from the slot list, from
 * SAVE's whose-slot rules and from the power-on load. At the first main-loop frame, before anything reads a slot,
 * each slot whose flash copy differs from RAM is fetched again; a slot flash does not hold is left as RAM has it */
static void gn_slots_from_flash(void)
{
#if FELUCCA_FLASH
    uint32_t i;
    st_hdr_t h;
    if (!flash_ok)
        return;
    for (i = 0; i < 4u; i++)
        if (st_current(OBJ_PROJECT0 + i, &h) >= 0 && h.len == sizeof(project_store_t) &&
            memcmp(st_buf, &proj_slot[i], sizeof(project_store_t)))
            proj_fetch(i);
#endif
}

static void gn_song_boot(uint32_t slot)
{
    uint32_t p;
    if (slot == GN_NO_SLOT || chain_busy())
        return;
    project_load(slot);
    if (proj_cur != slot || !gn.on)
        return;                                       /* (not loaded: Felucca's power-on tracks stay; Advanced: Felucca's
                                                       * own load, its LOADED shown) */
    gn.entry_put = 0;                                 /* (no entry Gnome over the song's track 1) */
    gn.entry_keys = 0;
    gn.last_sel = song.sel;
    for (p = 0; p < NTRK; p++)
        gn_reclaim(p);
    ui.msg_t = 0;                                     /* (project_load's LOADED: Felucca's message, not shown) */
    ui.msg2[0] = 0;
    gn.auto_dirty = gn_song_owned() == slot ? GN_LOOP_SAVED : 0u;   /* (the mark: Easy's own song only, round 27) */
}

/* gn_boot's mode and song (above) */
static void gn_boot_mode(void)
{
#ifdef FELUCCA_FAVORITES
    uint32_t last = gn_mode_marked() && (GN_SONG_BYTE & GN_SONG_R27) ? gn_rec_slot() : GN_NO_SLOT;
    gn_settings_migrate();
    if (last != GN_NO_SLOT)                           /* round 27: the last save's mode */
        GN_MODE_BYTE = (uint8_t)((GN_MODE_BYTE & ~GN_MODE_EASY) | (GN_SONG_BYTE & GN_SONG_ADV ? 0u : GN_MODE_EASY));
    else if (GN_MODE_BYTE & GN_MODE_EASY)
        last = gn_song_owned();                       /* (round 25/26's record: Easy's song, in Easy only) */
    if (GN_MODE_BYTE & GN_MODE_EASY)
        gn_enter();
    gn_song_boot(last);                               /* round 25, round 27: the last save, stopped */
#endif
}

/* GLO's page: the four tracks' volumes on the four knobs (a write of Easy's: put back on leaving) */
static void gn_mix_knob(uint32_t k, int32_t s)
{
    track_t *t = &trk[k % NTRK];
    gn_set_kept(t, P_LEVEL, t->p[P_LEVEL] + s * 4);
}

/* SELECT: the tempo itself, 40 to 240 BPM (Felucca's range), a detent a beat; with an external clock the
 * tempo is the clock's (the header shows it) and SELECT does nothing */
static void gn_tempo_turn(int32_t s)
{
    if (song.g[G_CLOCK])
        return;
    song.g[G_BPM] = (int16_t)clamp(song.g[G_BPM] + accel(EN_SELECT, s, 200), GP[G_BPM].min, GP[G_BPM].max);
}

/* what knob k does now (round 8: the one map both the input and the screen read): K3 the metronome while EDIT is
 * held (round 26: turned right ON, left OFF; the record screen's SNAP too); K4 KEEP or REPLACE while armed
 * or recording, and on the record screen; GLO's volumes; the loop pages' BARS SWING SNAP VOLUME (the record
 * screen stopped: MOVE, NOTE / REST); else the sound's Bright to Space. Whatever screen is up (the peek list,
 * tap tempo, the count-in, saved) shows these */
/* round UX-2: on the steps page K1 is the bar shown (the bar switch) and, on a kit, K2 the drum (the list's); on the
 * record screen stopped, a kit's K2 picks the drum NOTE writes and K3 is NOTE / REST (a melody's: K2, as before) */
/* gn/ux catch-up (FW-P0): EDIT held is both round 26's metronome switch (K3) and round UX-3's SHIFT (after
 * GN_SHIFT_MS). K3 is the metronome from EDIT's press on, on every page, except where SHIFT, once on, gives K3 a
 * job of its own: a step's strength (the steps page, the record screen stopped) and track 3's pan (the volumes) */
static int gn_k3_shift_job(void)
{
    return gn.shift && (gn.page == GP_STEPS || (gn.page == GP_RECORD && !song.playing) || (!GN_ENGINES && gn.page == GP_MIX));
}
static uint32_t gn_knob_role(uint32_t k)
{
    if (k == 2u && gn_btn(B_EDIT) && !gn_k3_shift_job())
        return gn_click_on() ? KR_CLICK_ON : KR_CLICK_OFF;
    if (gn_btn(B_FX) && !gn.shift)                    /* round FLOW-1, M2: FX held, an audition: the sound's own four */
        return KR_SOUND;                              /* (and its ECHO, SPACE) on every page; (the deep page: gone) */
    if (gn.shift && (gn.page == GP_STEPS || (gn.page == GP_RECORD && !song.playing)))   /* round UX-4: a step's */
        return k == 0u ? KR_MOVE : k == 1u ? (gn_kit_n(gn_part()) ? KR_DRUM : KR_PITCH) : k == 2u ? KR_VEL : KR_REST;
    if (gn.shift && gn.page == GP_MIX)                /* .. the four tracks' pan (round EW: none on the mono core) */
        return GN_EO(KR_MIX, KR_PAN);
    if (gn.shift)                                     /* .. nothing deeper here: the knobs as they are */
        return gn.page == GP_LOOP ? KR_LOOP : KR_SOUND;
    if (k == 3u && (gn.arm || song.rec || gn.page == GP_RECORD))
        return KR_KEEP;
    if (gn.page == GP_STEPS && gn.steps_keys && gn.hold_key)   /* round SEQ-1: a held step's four (a kit: no NOTE) */
        return k == 0u ? (gn_kit_n(gn_part()) ? KR_REST : KR_HNOTE) : k == 1u ? KR_HVEL : k == 2u ? KR_HLEN : KR_HWHEN;
    if (gn.page == GP_MIX)
        return KR_MIX;
    if (GN_LOOP_PAGE(gn.page)) {
        int kit = gn_kit_n(gn_part()) != 0;
        if (gn.page == GP_RECORD && !song.playing && k < 3u)
            return k == 0u ? KR_MOVE : kit ? (k == 1u ? KR_DRUM : KR_MOVE) : (k == 1u ? KR_MOVE : KR_LOOP);
        if (gn.page == GP_STEPS && k == 0u)
            return KR_BAR;
        if (gn.page == GP_STEPS && k == 1u)
            return kit ? KR_DRUM : KR_PEN;            /* (round SEQ-1: a melody's K2, the pen note) */
        return KR_LOOP;
    }
    return KR_SOUND;
}

/* round UX-3: SHIFT is EDIT held GN_SHIFT_MS. EDIT's press is a tap of the tap tempo, as before (pick 4: it acts on
 * the press, the beat's own moment); held on, it is SHIFT and that tap is taken back (the taps counted, their times
 * and the tempo as they were before it), so SHIFT never moves the tempo. Let go: SHIFT ends. While it is held the
 * page shows its Advanced depth at the same place, with the same track (gn_view: the sound's deep page) */
#define GN_SHIFT_MS 500u
static void gn_shift_on(void)                     /* EDIT's press was no tap: put the tap tempo back as it was */
{
    gn.shift = 1;
    lys.ntap = gn.tap_n;
    memcpy(lys.tap, gn.tap_was, sizeof gn.tap_was);
    song.g[G_BPM] = gn.tap_bpm;
    gn.tap_t = 0;
    ui.bpm_t = 0;
}
static void gn_shift_watch(void)
{
    if (!gn_btn(B_EDIT)) {
        gn.shift = 0;
        gn.edit_t0 = 0;
        gn.tempo_hold = 0;                            /* (round SEQ-1: the tempo's big number goes with EDIT) */
        return;
    }
    if (!gn.shift && gn.edit_t0 && gn_since(gn.edit_t0) >= GN_SHIFT_MS)
        gn_shift_on();
}

#include "gn_grooves.c"                            /* the demo grooves: PRESETS on the loop page */

/* ------------------------------------------------------------- input --- */
static int gn_input(void)
{
    uint32_t pressed, notes, id, k, now = fm1_ticks();
    int32_t s;
#if GN_ENGINES
    gn_core_host();                                   /* round MF: the sounds' own knobs, planned here, never in the */
#endif                                                /* audio interrupt (gn_core.c), Easy on or off */
    if (!(gn.booted & 1u))
        gn_boot();
    else
        gn_mode_reconcile();
    if (gn.save_note & GN_NOTE_SAVE)                  /* round 27: a save of Advanced's or the app's, the last now */
        gn_last_keep();
    if (!gn.on && !gn.sw_done)                        /* Easy off: only the hold that enters it (a hold */
        return gn_hold_watch();                       /* that has just left Easy is gn_switch's until let go) */
    if (gn.on) {                                      /* every call, the chord counting or not (round 6): */
        if (gn.adopt)                                 /* (round 24: the app's PRESET as Easy's pick, first) */
            gn_adopt();
        gn_upkeep();
        gn_owner_check();                             /* a load Easy did not make: its sound for good */
        if (chain_busy())                             /* a song prepared or playing (however started, once */
            gn_kept_retire_motion();                  /* it is): its motion moves the level, no claim to it */
        else if (song.playing) {                      /* round 8: the loop plays a track's own level motion */
            uint32_t t;                               /* (its writes, the same value or not, are motion's) */
            for (t = 0; t < NTRK; t++)
                if (gn_level_motion(t))
                    gn_kept_retire(t, P_LEVEL);
        }
    }
    if (gn_switch())
        return 1;
    if (!gn.on)
        return 0;
    if (gn.entry_keys) {                              /* round 14: keys asked for the entry's Gnome */
        gn_entry_load();
        gn_entry_keys();
    }
    if (gn.frame != ui.frame) {                       /* once a frame (ui_input runs many times in one) */
        gn.frame = ui.frame;
        if (!(gn.frame & 3u))                         /* (round UX-5: the music's fingerprint, every 4th frame) */
            gn.music_sig = gn_music_sig();
        if (gn.entry_put && track_sig(&trk[GN_MELODY]) != gn.entry_sig)
            gn.entry_put = 0;                         /* (track 1 changed by someone else: no Gnome) */
        gn_knob_follow(gn_part());
        gn_undo_watch();
        if (gn.save_pend)
            gn_save_song();
        gn_rec_tick();
        gn_autosave();                                /* (round 24) */
#if GN_DEMO_GROOVES
        if (gn.page == GP_GROOVES && !(gn.frame & 3u))
            gn_tiles_mine();                          /* (round FLOW-1, M2: GROOVES's MINE tiles) */
#endif
    }
    if (song.sel != gn.last_sel)                      /* another track (the editor, a load): the keys, the */
        gn_retarget();                                /* knobs and REC are on it now */
    gn.last_sel = song.sel;
    pressed = fm1_input_edges(0);
    notes = fm1_input_note_edges();
    if (gn.gest_lock) {                               /* round 9: the music was replaced mid-gesture: the */
        if (!(fm1_in.buttons & 0x3FFFu))              /* buttons do nothing until every one is up */
            gn.gest_lock = 0;
        else {
            pressed = 0;
            gn_cancel(&gn.rec_t0, B_REC);
            gn_cancel(&gn.home_t0, B_HOME);
            gn_cancel(&gn.glo_t0, B_GLO);
            gn_cancel(&gn.play_t0, B_PLAY);
            gn_cancel(&gn.fx_t0, B_FX);
            gn.save_t0 = gn_btn(B_SAVE) ? gn.save_t0 | 3u : 0u;
        }
    }
    if (gn.boot_t0 && (pressed || notes || fm1_in.notes))
        gn.boot_t0 = 0;                               /* a key or button ends the start-up picture */
    if (pressed || notes) {
        gn.saved_t = 0;
        if (gn.auto_dirty)
            gn_auto_push(GN_AUTO_SETTLE);             /* (round 24: the autosave waits for the child) */
    }
    if (notes)                                        /* (round FLOW-1, M2: a key ends the picture wheel and the sound */
        gn.pick_t = gn.peek_t = 0;                    /* list, which stay till then) */
    if (gn_btn(B_GLO) || gn_btn(B_FX))
        notes = 0;                                    /* (round FLOW-1: GLO or FX held, a fill's or an audition's keys: */
#if GN_DEMO_GROOVES                                   /* no step written) */
    if (gn.page == GP_GROOVES)
        gn_tiles_keys(notes);                         /* (M2: GROOVES, white keys 1 to 9 load the tiles) */
#endif
    gn_step_keys(notes);                              /* the record screen, stopped: keys into steps */
    gn_steps_keys(notes);                             /* round SEQ-1: STEPS, the white keys are the bar's places */
    gn_take_log(notes);                               /* (a take: what the panel's keys played) */
    {
        uint32_t home = btn_hold(&gn.home_t0, B_HOME, now, 0);
        uint32_t save = gn_save_btn();
        if (home == BT_TAP) {                         /* HOME: Home (round UX-1); on Home, the sound; (round FLOW-1, */
            if (!gn.pick_t && (!gn.peek_t || (gn.grv & 0x80u)))   /* M2: the wheel or the sound list up (not the */
                gn.page = gn.page == GP_HOME ? GP_PLAY : GP_HOME;   /* grooves' 2 s list, GN_GV_LIST): it closes
                                                                 * them first, the page under them stays) */
            gn.pick_t = gn.peek_t = gn.tap_t = gn.saved_t = 0;
        }
        uint32_t glo = btn_hold(&gn.glo_t0, B_GLO, now, 0), fx = btn_hold(&gn.fx_t0, B_FX, now, 0);
        if (gn.perf_n)                                /* (round FLOW-1: GLO held + keys, a fill; FX held + keys, an */
            *(gn.perf_roll ? &gn.glo_t0 : &gn.fx_t0) |= 2u;   /* audition: no tap) */
        if (fx == BT_TAP)                             /* FX: track 1 (M2: when let go, as it may be held) */
            gn_track_btn(0);
        if (glo == BT_TAP) {                          /* GLO: the four volumes and back (to Home: round UX-1); */
            gn.page = gn.page == GP_MIX ? GP_HOME : GP_MIX;   /* (round FLOW-1: when let go, as it may be held) */
            gn.pick_t = gn.peek_t = gn.tap_t = 0;
        }
        gn_play_watch((pressed >> panel.btn[B_PLAY]) & 1u);   /* PLAY, from any page */
#if GN_DEMO_GROOVES
        if (gn.tile_q && !song.playing && !transport_req)
            gn_tile_load();                           /* (M2: a tile queued, the loop stopped on the bar line) */
#endif
        gn_rec_btn();
        if (save == BT_TAP)
            gn.save_pend = 1;
        else if (save == BT_HOLD)
            gn_undo();
    }
    gn_shift_watch();                                 /* (round UX-3: EDIT held is SHIFT; let go, not, before */
    for (id = 0; id < 14u; id++) {                    /* this frame's buttons act) */
        uint32_t b;
        if (!((pressed >> id) & 1u))
            continue;
        b = panel_btn_of(id);
        switch (b) {
        case B_SCL: case B_ENV: case B_LFO:           /* the four tracks (panel.c: B_FX .. B_LFO = 0 .. 3; FX's below) */
            gn_track_btn(b - B_FX);
            break;
        case B_SEQ:                                   /* the loop's pages: loop, steps, record, loop .. */
            gn.page = gn.page == GP_LOOP ? GP_STEPS : gn.page == GP_STEPS ? GP_RECORD : GP_LOOP;
            gn.pick_t = gn.peek_t = gn.tap_t = 0;
            if (gn.page == GP_RECORD)
                gn.cursor = 0;
            break;
        case B_ARP:
            gn_arp_toggle();
            break;
        case B_EDIT:                                  /* tap tempo: Felucca's own (ui_layer.c, the last taps) */
            gn.tap_n = lys.ntap;                      /* (round UX-3: held on, it is SHIFT and this tap */
            gn.tap_bpm = song.g[G_BPM];               /* is taken back: gn_shift_watch) */
            memcpy(gn.tap_was, lys.tap, sizeof gn.tap_was);
            gn.edit_t0 = fm1_ms | 1u;
            glo_tap();
            ui.msg_t = 0;                             /* (its message, an external clock: Easy's screen says it) */
            gn.tap_t = fm1_ms | 1u;
            break;
        case B_OCTDN: case B_OCTUP:
            if (gn_kit_n(gn_part()) > 8u)             /* round UX-3: a kit's bank (its pads shown: A 1-8, B 9-16) */
                gn.bank = b == B_OCTUP;
            else
                song.octave += b == B_OCTDN ? (song.octave > -2 ? -1 : 0) : (song.octave < 2 ? 1 : 0);
            break;
        default:
            break;
        }
    }
    if ((s = panel_enc(EN_ALGO)) != 0) {              /* ALGORITHM: the picture */
        gn.pick_t = fm1_ms | 1u;
        gn.peek_t = gn.tap_t = 0;
        gn.boot_t0 = 0;
        gn_pick(s);
    }
    if ((s = panel_enc(EN_PRESET)) != 0) {            /* PRESETS: the sound inside it */
        gn.pick_t = gn.tap_t = 0;
        gn.boot_t0 = 0;
#if GN_DEMO_GROOVES
        if (gn.page == GP_LOOP)                       /* (the loop page: the demo grooves) */
            gn_gv_turn(s);
        else
#endif
            gn_peek(s);
    }
    if (song.sel != gn.last_sel) {                    /* (a track button) */
        gn_retarget();
        gn.last_sel = song.sel;
    }
    if ((s = panel_enc(EN_SELECT)) != 0) {            /* round SEQ-1 (spec S-1, S-2): SELECT turns the page */
        if (gn_btn(B_EDIT)) {                         /* ring; EDIT held + SELECT, the tempo (its big number while */
            if (!gn.shift && gn.edit_t0)              /* EDIT is held; EDIT's press was no tap then) */
                gn_shift_on();
            gn.tempo_hold = 1;
            gn_tempo_turn(s);
        } else
            gn_ring_turn(s);
        gn.boot_t0 = 0;
    }
    for (k = 0; k < 4u; k++)
        if ((s = panel_enc(EN_K1 + k)) != 0) {
            uint32_t p = gn_part(), r = gn_knob_role(k);
            gn.boot_t0 = 0;
            if (r == KR_REST)
                continue;                             /* (round UX-3: SHIFT's deep page reads, it writes nothing) */
            gn.klit_t[k] = fm1_ms | 1u;               /* (round SEQ-1: what turns lights up, in line) */
            if (gn_btn(B_FX))
                gn.fx_t0 |= 2u;                       /* (round FLOW-1: FX used for an audition: no tap) */
            if (r == KR_KEEP)
                gn.replace = s > 0;
            else if (r == KR_CLICK_ON || r == KR_CLICK_OFF) {
                gn_click_set(s > 0);                  /* (round 26: kept in the settings record) */
                gn.tap_t = 0;                         /* (EDIT's press was no tap: the page and its CLICK card) */
            }
            else if (r == KR_MIX)
                gn_mix_knob(k, s);
            else if (r == KR_LOOP || r == KR_MOVE || r == KR_BAR || r == KR_DRUM || r == KR_PITCH || r == KR_VEL ||
                     r == KR_PEN || (r >= KR_HNOTE && r <= KR_HWHEN))
                gn_loop_knob(k, s);
            else if (r == KR_PAN)                     /* round UX-4: SHIFT on the volumes: track k's pan */
                gn_set(&trk[k % NTRK], P_PAN, trk[k % NTRK].p[P_PAN] + s * 4);
            else if (gn_map(p, k)) {
                if (gn.entry_put && p == GN_MELODY)
                    gn_entry_load();                  /* (round 14: the entry's Gnome, now he is used) */
                gn_wob_claim(p, k);
                gn.amount[p][k] = (uint8_t)clamp((int32_t)gn.amount[p][k] + s, 0, GN_STEPS);
                gn_knob_write(p, k);
                gn_wob_moved(p, k);
            }
        }
    if (gn.lit_page != gn.page) {                     /* (round SEQ-1: another page, another knob map: none lit) */
        memset(gn.klit_t, 0, sizeof gn.klit_t);
        gn.lit_page = gn.page;
    }
    gn_steps_arm();                                   /* (round SEQ-1: what the keys do on STEPS from now on) */
    return 1;
}

/* ---------------------------------------------------------- the LEDs --- */
/* Lit: the selected track's button (FX SCL ENV LFO: tracks 1 to 4; the LEDs are one colour, the screen's
 * badge says the track's), SEQ on the loop page, GLO on the volumes, ARP while the track's arpeggio is on,
 * PLAY's green while the loop plays, REC while recording, OCT- / OCT+ off the middle octave, the keys held
 * that sound. Glowing dim (MENU > LEDS DIM, Felucca's default): every button (each does something in Easy)
 * and the keys that play; LEDS INV turns it around as Felucca's own LEDs do. Easy off: Felucca's LEDs,
 * during the hold that enters too. */
static int gn_leds(void)
{
    uint8_t nl[FM1_NCOL] = {0}, nd[FM1_NCOL] = {0};
    uint32_t k, c, all;
    if (!gn.on)
        return 0;
    all = 1;                                          /* (round 13: every key plays, black keys too) */
    led_put(nl, panel.btn[B_FX + gn_part()], 1);
    led_put(nl, panel.btn[B_SEQ], GN_LOOP_PAGE(gn.page));
    led_put(nl, panel.btn[B_GLO], gn.page == GP_MIX);
    led_put(nl, panel.btn[B_ARP], TSEL->p[P_AMODE] != 0);
    led_put(nl, panel.btn[B_REC], song.rec != 0u || (gn.arm == GA_READY && ((fm1_ms / 250u) & 1u) == 0u) ||
                                      (gn.arm == GA_COUNT && ((fm1_ms / 100u) & 1u) == 0u));   /* armed: blinking */
    if (gn_kit_n(gn_part()) > 8u) {                   /* round UX-3: a kit: the bank shown */
        led_put(nl, panel.btn[B_OCTDN], !gn.bank);
        led_put(nl, panel.btn[B_OCTUP], gn.bank);
    } else {
        led_put(nl, panel.btn[B_OCTDN], song.octave < 0);
        led_put(nl, panel.btn[B_OCTUP], song.octave > 0);
    }
    led_put(nl, panel.btn[B_EDIT], gn.shift);         /* (SHIFT held: EDIT lit) */
    for (k = 0; k < 27u; k++) {
        int sounds = all;
        led_put(nl, 14u + k, sounds && ((fm1_in.notes >> k) & 1u));
        if (sounds)
            led_put(nd, 14u + k, 1);
    }
    for (k = 0; k < NB; k++)
        led_put(nd, panel.btn[k], 1);
    if (song.playing)
        led_clear(nd, panel.btn[B_PLAY]);             /* playing: PLAY's green, its own LED dark */
    for (c = 0; c < FM1_NCOL; c++)
        if (settings_leds == LEDS_INV) {
            nl[c] = (uint8_t)(nd[c] & ~nl[c]);
            nd[c] = 0;
        }
    if (song.playing)
        nl[LED_PLAY_GREEN >> 3] |= (uint8_t)(1u << (LED_PLAY_GREEN & 7u));
    for (c = 0; c < FM1_NCOL; c++)
        fm1_led_dim[c] = nd[c];
    for (c = 0; c < FM1_NCOL; c++)
        fm1_led[c] = nl[c];
    return 1;
}

#include "gn_screens.c"
