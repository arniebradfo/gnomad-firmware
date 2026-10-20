/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kyle Palaniuk */
/* Gnomad (FORK.md, round SEQ-1: the sequencer's phase 1). Included by gn_easy.c.
 *
 * gn_seq_*: the sequencer as Gnomad's pages see it. Phase 1 keeps Felucca's data model (four tracks of 64 step_t,
 * the track's LEN and DIV, ties, the step's velocity and chance) and hides it here, so phase 2 can swap the storage
 * for the spec's 12-byte step and shared pool (specs/SEQUENCER-SPEC-v0.md, 3.1 and 3.2) without the pages changing:
 * the LOOP and STEPS pages (gn_screens.c) and the STEPS tap grid below read and write steps only through these.
 * Places: a bar is shown as 16 places (8 on the 1/8 grid); with SNAP off (1/32) a place is two steps.
 *
 * Also here: the page ring SELECT turns (one control changes the page; spec 1, S-1 and S-3), and the STEPS
 * page's tap grid (spec 4.3 and 6.2: the sixteen white keys are the sixteen places of the bar shown; tap = on, tap a
 * lit step = off, the sound plays as you tap; hold a key and turn K1..K4 = that step's NOTE, STRENGTH, LENGTH, WHEN).
 * Every write takes Easy's undo copy first (SAVE held undoes, as step entry). Nothing here writes flash. */
#define GN_SEQ_NONE 0xFFFFu

/* ------------------------------------------------------------ reading --- */
static uint32_t gn_seq_len(uint32_t p) { return (uint32_t)clamp(trk[p % NTRK].p[P_SLEN], 1, NSTEP); }
static uint32_t gn_seq_spb(uint32_t p) { return gn_grid_ok(p % NTRK) ? gn_spb((uint32_t)trk[p % NTRK].p[P_SDIV]) : 16u; }
static uint32_t gn_seq_bars(uint32_t p) { return (gn_seq_len(p) + gn_seq_spb(p) - 1u) / gn_seq_spb(p); }
static uint32_t gn_seq_cells(uint32_t p) { return gn_seq_spb(p) > 16u ? 16u : gn_seq_spb(p); }   /* places a bar */
static uint32_t gn_seq_fold(uint32_t p) { return gn_seq_spb(p) / gn_seq_cells(p); }                /* steps a place */
/* the step the playhead is in (playing), else GN_SEQ_NONE */
static uint32_t gn_seq_pos(uint32_t p) { return song.playing ? (uint32_t)trk[p % NTRK].seq_idx % gn_seq_len(p) : GN_SEQ_NONE; }
static const step_t *gn_seq_at(uint32_t p, uint32_t i) { return &trk[p % NTRK].step[i % NSTEP]; }
static int gn_seq_sounds(uint32_t p, uint32_t i)       /* step i begins something */
{
    const step_t *s = gn_seq_at(p, i);
    return i < gn_seq_len(p) && s->time == ST_NOTE && (s->n || s->hit);
}
static int gn_seq_tied(uint32_t p, uint32_t i) { return i < gn_seq_len(p) && gn_seq_at(p, i)->time == ST_TIE; }
static uint32_t gn_seq_pitch(uint32_t p, uint32_t i)    /* its first note, 0 none */
{
    const step_t *s = gn_seq_at(p, i);
    return gn_seq_sounds(p, i) && s->n ? s->note[0] : 0u;
}
static int gn_seq_drum(uint32_t p, uint32_t i, uint32_t d) { return i < gn_seq_len(p) && gn_kit_has(p, gn_seq_at(p, i), d); }
static uint32_t gn_seq_vel(uint32_t p, uint32_t i) { return gn_seq_at(p, i)->vel; }
static uint32_t gn_seq_when(uint32_t p, uint32_t i) { return step_chance(gn_seq_at(p, i)); }   /* 0..100 % */
static uint32_t gn_seq_glen(uint32_t p, uint32_t i)    /* the steps it holds: itself and the ties after it */
{
    uint32_t n = 1, len = gn_seq_len(p);
    while (n < 16u && i + n < len && gn_seq_tied(p, i + n))
        n++;
    return n;
}
/* place c of bar b: its first step that sounds, else its first step (two 32nds a place with SNAP off) */
static uint32_t gn_seq_place_step(uint32_t p, uint32_t b, uint32_t c)
{
    uint32_t f = gn_seq_fold(p), i = b * gn_seq_spb(p) + c * f, j;
    for (j = 0; j < f; j++)
        if (gn_seq_sounds(p, i + j))
            return i + j;
    return i;
}
/* bar b's places that begin something (bit c), held on (ties), and play drum d */
static uint32_t gn_seq_bar_mask(uint32_t p, uint32_t b, uint32_t what, uint32_t d)
{
    uint32_t c, j, m = 0, f = gn_seq_fold(p), i0 = b * gn_seq_spb(p);
    for (c = 0; c < gn_seq_cells(p); c++)
        for (j = 0; j < f; j++) {
            uint32_t i = i0 + c * f + j;
            if (what == 0u ? gn_seq_sounds(p, i) : what == 1u ? gn_seq_tied(p, i) : gn_seq_drum(p, i, d))
                m |= 1u << c;
        }
    return m;
}

/* ------------------------------------------------------------ writing --- */
/* a write: refused while a song plays or a take records (spec 7: edits while playing land at the next step, the audio
 * interrupt off for the bytes); the undo copy first, as step entry's */
static int gn_seq_begin(uint32_t p)
{
    if (chain_busy() || song.rec)
        return 0;
    gn_undo_take(p, GN_UK(GN_UK_STEPS));
    fm1_irq_off();
    return 1;
}
static void gn_seq_end(uint32_t p)
{
    fm1_irq_on();
    gn.undo_mine = gn_pat_sig(p);
}
static void gn_seq_fresh(step_t *s)                    /* a step that will begin something, empty */
{
    step_clear(s);
    s->time = ST_NOTE;
    s->vel = 100;
}
static void gn_seq_unhold(uint32_t p, uint32_t i)      /* the ties after step i: empty steps */
{
    uint32_t len = gn_seq_len(p);
    while (++i < len && trk[p].step[i].time == ST_TIE) {
        step_clear(&trk[p].step[i]);
        trk[p].step[i].time = ST_NOTE;
    }
}
/* drum d at step i: on or off (the DRUM engine's lane bit, else the drum's note among the step's four) */
static void gn_seq_drum_set(uint32_t p, uint32_t i, uint32_t d, int on)
{
    step_t *s = &trk[p].step[i % NSTEP];
    uint32_t note = gn_kit_note(p, d), k, n;
    if (i >= gn_seq_len(p) || on == gn_seq_drum(p, i, d) || !gn_seq_begin(p))
        return;
    if (on) {
        if (s->time != ST_NOTE || (!s->n && !s->hit))
            gn_seq_fresh(s);
        if (trk[p].eng_req == GE_DRUM && note == DRUM_LANE_NOTE[drum_lane(note)])
            s->hit |= (uint8_t)(1u << drum_lane(note));
        else if (s->n < 4u)
            s->note[s->n++] = (uint8_t)note;
        else
            s->note[3] = (uint8_t)note;
    } else {
        for (k = n = 0; k < s->n && k < 4u; k++)
            if (gn_kit_of(p, s->note[k]) != d)
                s->note[n++] = s->note[k];
        s->n = (uint8_t)n;
        for (k = 0; k < NLANE; k++)
            if (gn_kit_of(p, DRUM_LANE_NOTE[k]) == d)
                s->hit &= (uint8_t)~(1u << k);
        s->acc &= s->hit;
    }
    gn_seq_end(p);
}
/* a melody's step i: note (the pen note), or 0: empty, and what it held on with it */
static void gn_seq_note_set(uint32_t p, uint32_t i, uint32_t note)
{
    step_t *s = &trk[p].step[i % NSTEP];
    if (i >= gn_seq_len(p) || !gn_seq_begin(p))
        return;
    gn_seq_unhold(p, i);
    gn_seq_fresh(s);
    if (note) {
        s->n = 1;
        s->note[0] = (uint8_t)clamp((int32_t)note, 0, 127);
    }
    gn_seq_end(p);
}
/* the held step's four (spec 6.2: NOTE, STRENGTH, LENGTH, WHEN); a step that begins nothing is left as it is */
static const uint8_t GN_WHEN[5] = {0, 75, 50, 25, 10};  /* ALWAYS, OFTEN, SOMETIMES, RARELY, HARDLY (chance %) */
static void gn_seq_edit(uint32_t p, uint32_t i, uint32_t what, int32_t s)
{
    step_t *st = &trk[p].step[i % NSTEP];
    uint32_t k, len = gn_seq_len(p);
    if (!gn_seq_sounds(p, i) || !gn_seq_begin(p))
        return;
    if (what == 0u) {                                 /* NOTE: a semitone a detent (a melody's) */
        for (k = 0; k < st->n && k < 4u; k++)
            st->note[k] = (uint8_t)clamp((int32_t)st->note[k] + s, 0, 127);
    } else if (what == 1u) {                          /* STRENGTH: the velocity, 4 a detent */
        st->vel = (uint8_t)clamp((int32_t)(st->vel ? st->vel : 100u) + s * 4, 1, 127);
    } else if (what == 2u) {                          /* LENGTH: the ties after it, 1 to 16 steps, over rests only */
        int32_t d;
        for (d = s > 0 ? s : -s; d > 0; d--) {        /* (a detent a step) */
            uint32_t n = gn_seq_glen(p, i);
            if (s > 0 && n < 16u && i + n < len && !gn_seq_sounds(p, i + n) && !gn_seq_tied(p, i + n)) {
                step_clear(&trk[p].step[i + n]);
                trk[p].step[i + n].time = ST_TIE;
            } else if (s < 0 && n > 1u) {
                step_clear(&trk[p].step[i + n - 1u]);
                trk[p].step[i + n - 1u].time = ST_NOTE;
            }
        }
    } else {                                          /* WHEN: ALWAYS OFTEN SOMETIMES RARELY HARDLY, a detent each */
        uint32_t c = step_chance(st);
        for (k = 0; k < 4u && (k ? GN_WHEN[k] : 100u) > c; k++)
            ;
        k = (uint32_t)clamp((int32_t)k + clamp(s, -4, 4), 0, 4);
        st->probability = GN_WHEN[k];
    }
    gn_seq_end(p);
}
static const char *gn_seq_when_word(uint32_t c)
{
    return c >= 100u ? "ALWAYS" : c >= 75u ? "OFTEN" : c >= 50u ? "SOMETIMES" : c >= 25u ? "RARELY" : c ? "HARDLY" : "NEVER";
}

/* ------------------------------------------------------- the page ring --- */
/* SELECT: the next page, a detent a page, round: HOME SOUND GROOVES LOOP STEPS MIX (spec 1, S-3; round FLOW-1: SONG
 * hidden until it is real). Step entry (SEQ's third page) sits where STEPS does. HOME, SEQ and GLO still go where they
 * went */
static const uint8_t GN_RING[] = {GP_HOME, GP_PLAY, GP_GROOVES, GP_LOOP, GP_STEPS, GP_MIX};
#define GN_NRING NELEM(GN_RING)
static uint32_t gn_ring_of(uint32_t page)
{
    uint32_t i;
    if (page == GP_RECORD)
        page = GP_STEPS;
    for (i = 0; i < GN_NRING && GN_RING[i] != page; i++)
        ;
    return i < GN_NRING ? i : 0u;
}
static void gn_ring_turn(int32_t s)
{
    int32_t i = (int32_t)gn_ring_of(gn.page) + clamp(s, -3, 3);   /* (a fast turn: up to three pages a frame) */
    gn.page = GN_RING[(i + 2 * (int32_t)GN_NRING) % (int32_t)GN_NRING];
    gn.pick_t = gn.peek_t = gn.tap_t = 0;
    gn.hold_key = 0;
}

/* ------------------------------------------------- the STEPS tap grid --- */
/* live on the STEPS page of an Easy grid while nothing records, no song plays and SHIFT is up (SHIFT's step edits
 * keep UX-4's cursor); else the keys play as everywhere else (gn_key) */
static int gn_steps_live(void)
{
    uint32_t p = gn_part();
    return gn.page == GP_STEPS && gn_grid_ok(p) && !song.rec && !gn.arm && !gn.prov && !chain_busy() && !gn.shift;
}
static uint32_t gn_pen(void) { return gn.last_note ? gn.last_note : 60u; }   /* the melody's pen note (K2) */
/* once a frame, before the keys: what gn_key (the audio interrupt) gives a white key on STEPS */
static void gn_steps_arm(void)
{
    uint32_t p = gn_part();
    gn.steps_cells = (uint8_t)gn_seq_cells(p);
    gn.steps_note = (uint8_t)(gn_kit_n(p) ? gn_kit_note(p, gn.drum) : gn_pen());
    gn.steps_keys = (uint8_t)gn_steps_live();
    if (!gn.steps_keys)
        gn.hold_key = 0;
}
static int gn_white_down(uint32_t c)                  /* the white key of place c is down */
{
    uint32_t k;
    for (k = 0; k < 27u; k++)
        if (GN_WHITE[k] == c && ((fm1_in.notes >> k) & 1u))
            return 1;
    return 0;
}
static int gn_steps_on(uint32_t p, uint32_t i)        /* the step is lit: K2's drum (a kit), a note (a melody) */
{
    return gn_kit_n(p) ? gn_seq_drum(p, i, gn.drum) : gn_seq_sounds(p, i);
}
static void gn_steps_keys(uint32_t notes)
{
    uint32_t p = gn_part(), k;
    if (gn.hold_key && !gn_white_down(gn.hold_key - 1u)) {   /* let go: a lit step pressed and not turned goes off */
        if (gn.hold_off && !gn.hold_used && gn.steps_keys) {
            uint32_t i = gn.cursor;
            if (gn_kit_n(p))
                gn_seq_drum_set(p, i, gn.drum, 0);
            else
                gn_seq_note_set(p, i, 0);
        }
        gn.hold_key = 0;
    }
    if (!gn.steps_keys || !notes)
        return;
    for (k = 0; k < 27u; k++) {
        uint32_t c = GN_WHITE[k], b, i;
        if (!((notes >> k) & 1u) || c >= gn.steps_cells)
            continue;
        b = gn_view_bar(p);
        i = gn_seq_place_step(p, b, c);
        if (i >= gn_seq_len(p))
            continue;
        gn.cursor = (uint8_t)i;                       /* (the cursor: the last step tapped) */
        gn.vbar = (uint8_t)b;                         /* (the page stays on this bar 3 s: GN_VBAR_MS) */
        gn.vbar_t = fm1_ms | 1u;
        gn.hold_key = (uint8_t)(c + 1u);
        gn.hold_used = 0;
        gn.hold_off = (uint8_t)gn_steps_on(p, i);
        if (!gn.hold_off) {
            if (gn_kit_n(p))
                gn_seq_drum_set(p, i, gn.drum, 1);
            else
                gn_seq_note_set(p, i, gn_pen());
        }
    }
}
