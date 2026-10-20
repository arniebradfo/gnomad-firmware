/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kyle Palaniuk */
/* Gnomad (FORK.md, round EW): the Advanced pages' knobs on a Gnomad sound, for params.c page_desc
 * and ui.c page_visible (each one guarded line there) and the skin's words (gn_skin.c). Included at the end of
 * gn_core.c (after the descriptors); the main loop only: gn_core_adv.c says what each page does. */
static const engine_t *gna_desc_of(int32_t ce)
{
    return ce == GC_VA ? &ENG_GN_VA : ce == GC_FM ? &ENG_GN_FM : ce == GC_DRUMS ? &ENG_GN_DRUMS : ce == GC_GNOME ?
           &ENG_GN_GNOME : &ENG_GN_SAMPLE;
}

/* why knob `slot` of a page does nothing on the selected track's Gnomad sound: 0 = it does something; GNW_KNOB + k:
 * the sound's own knob k has it (the main loop's plan of its bind, gn_core_host); else a GNW_* reason (the skin's
 * words, gn_skin.c) */
enum { GNW_LIVE, GNW_DRUM_ENV, GNW_NONE, GNW_SINE, GNW_START, GNW_FADE, GNW_DRIVE, GNW_PAN, GNW_TONE, GNW_TYPE,
       GNW_DAMP, GNW_SLICES, GNW_KNOB };
#define GNW_NO_FLT GNW_NONE            /* (the engine has no such thing: "Not on this sound") */
#define GNW_NO_PIT GNW_NONE
#define GNW_SHAPE GNW_NONE
#define GNW_NO_LFO GNW_NONE
#define GNW_FIXED GNW_NONE
#define GNW_NO_AMP GNW_NONE
enum { GNK_TRACK, GNK_GLOBAL, GNK_EDIT1, GNK_EDIT2, GNK_NONE };
static uint8_t gn_why[4];              /* each card's reason as page_desc last said it (the skin draws it, gn_skin.c) */
static const uint8_t GNA_NONE[GNA_N] = {GNW_DRUM_ENV, GNW_DRUM_ENV, GNW_DRUM_ENV, GNW_DRUM_ENV, GNW_NO_FLT, GNW_NO_PIT,
                                        GNW_NO_LFO, GNW_NO_PIT, GNW_NO_AMP};
static const uint8_t GNA_DEAD[][2] = {{P_ED_SHP, GNW_SHAPE}, {P_LD_SHP, GNW_SHAPE}, {P_LD_FLT, GNW_NO_FLT},
                                      {P_LWAVE, GNW_SINE}, {P_LPHASE, GNW_START}, {P_LFADE, GNW_FADE},
                                      {P_DIST, GNW_DRIVE}, {P_PAN, GNW_PAN}, {P_GLIDE, GNW_FIXED}, {P_GLMODE, GNW_FIXED},
                                      {P_DETUNE, GNW_FIXED}, {G_DCOLOR, GNW_TONE}, {G_RTYPE, GNW_TYPE}, {G_RDAMP, GNW_DAMP}};
/* (round EW-FIXES: VOICE's Glide and its Mode, VOICE 2's Detune move Felucca's voice pitch, which the core never reads:
 * it gets the key alone, gnc_note_on) */
static uint32_t gn_page_why(uint32_t kind, uint32_t id)
{
    uint32_t ti = song.sel % NTRK, c, s;
    int32_t ce = gnc_engine(trk[ti].eng_req);
    uint64_t r;
    const uint8_t *kit;
    if (ce < 0 || kind == GNK_EDIT1)
        return GNW_LIVE;
    if (kind == GNK_EDIT2) {
        if (ce == GC_SAMPLE && id < 3u && gna_word(gnc_record(ce, trk[ti].preset, &kit), 13u) >= 2u)
            return GNW_SLICES;
        r = gna_reach2(ce, id);
    } else {
        for (c = 0; c < NELEM(GNA_DEAD); c++)           /* (the shared ids are below 32, the track's other: the */
            if (GNA_DEAD[c][0] == id && (c < 11u) == (kind == GNK_TRACK))  /* first eleven the track's) */
                return GNA_DEAD[c][1];
        for (c = 0; c < GNA_N && (GNA_ID[c] != id || kind != GNK_TRACK); c++)
            ;
        if (c == GNA_N)
            return GNW_LIVE;
        if (!GNA_P[ce][c])
            return GNA_NONE[c];
        r = gna_reach(ce, c);
    }
    if (!gnc.bound[ti] || gnh.plan[ti] != gnc.bseq[ti] || !(r & gnh.reach[ti]))
        return GNW_LIVE;
    for (s = 0; s < 3u && !(r & gnh.kreach[ti][s]); s++)
        ;
    return GNW_KNOB + s;
}

/* params.c page_desc: knob `slot` of a page (kind, its parameter id) on the selected track. 1 = decided here: *d the
 * descriptor (0: nothing to do, the card says why), *valp the setting; 0 = upstream's (a Felucca track, a live knob of
 * a track page). EDIT 1: the sound's four, each where its amount is kept (the plan's), named as the record names it */
static int gn_page_slot(uint32_t kind, uint32_t id, uint32_t slot, int16_t **valp, const param_desc_t **d)
{
    static param_desc_t e1[4];
    static const char *const DEF[4] = {"TONE", "MOTION", "ECHO", "SPACE"};
    static const uint8_t NATIVE[4] = {P_E0, P_E1, P_DLY, P_REV};
    uint32_t ti = song.sel % NTRK, store, w;
    track_t *t = &trk[ti];
    int32_t ce = gnc_engine(t->eng_req);
    slot &= 3u;
    gn_why[slot] = GNW_LIVE;
    if (ce < 0 || kind == GNK_NONE)
        return 0;
    if (kind == GNK_EDIT1) {
        const uint8_t *kit, *rec = gnc_record(ce, t->preset, &kit);
        static char nm[4][8];                       /* (the record's name in words: "GROWL" is Growl) */
        const char *l = ce == GC_DRUMS ? "" : gc_macro_label((const gc_patch *)rec, slot);
        uint32_t i;
        l = l[0] ? l : DEF[slot];
        for (i = 0; l[i] && i < 7u; i++)
            nm[slot][i] = (char)(i && l[i] >= 'A' && l[i] <= 'Z' ? l[i] + 32 : l[i]);
        nm[slot][i] = 0;
        store = gnc.bound[ti] && gnh.plan[ti] == gnc.bseq[ti] ? gnh.store[ti][slot] : NATIVE[slot];
        e1[slot] = (param_desc_t){nm[slot], F_PCT, 0, 127, (int16_t)(store == P_E0 ? 64 : 0), 0, 0};
        *valp = &t->p[store];
        *d = &e1[slot];
        return 1;
    }
    w = gn_page_why(kind, kind == GNK_EDIT2 ? slot : id);
    gn_why[slot] = (uint8_t)w;
    if (w == GNW_LIVE) {
        if (kind != GNK_EDIT2)
            return 0;
        *valp = &t->p[P_E4 + slot];
        *d = &gna_desc_of(ce)->edit[4u + slot];
        return 1;
    }
    *valp = 0;
    *d = 0;
    return 1;
}

/* ui.c page_visible: a page with nothing to do on the selected track's Gnomad engine is not offered (SLICER and MOD
 * never: the core has neither; ENV DEST and LFO DEST where none of their knobs does anything) */
static int gna_none(uint32_t why) { return why != GNW_LIVE && why < GNW_KNOB; }
static int gn_page_shown(uint32_t never, uint32_t id0)
{
    if (gnc_engine(TSEL->eng_req) < 0)
        return 1;
    if (never)
        return 0;
    if (id0 == P_ED_FLT || id0 == P_LD_PIT)
        return !(gna_none(gn_page_why(GNK_TRACK, id0)) &&
                 gna_none(gn_page_why(GNK_TRACK, id0 == P_ED_FLT ? P_ED_PIT : P_LD_AMP)));
    return 1;
}
