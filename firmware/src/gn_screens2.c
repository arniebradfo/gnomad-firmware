/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kyle Palaniuk */
/* Gnomad (FORK.md): the start-up (3i) and saved (3g) screens. Included by gn_screens.c. */

/* -------------------------------------------------------- start-up (3i) --- */
/* FM1-Boot, when Easy is entered (the synth starts in Felucca's pages), in 3 s (the board takes 6):
 * his hat (the mark) draws itself, he pops up under it, sings A E I O U while three notes float up,
 * the four knob dots pop in one by one, and he ends on a cheer with two hops. A key or a knob ends
 * it at once. Drawn at 15 frames a second (gs.boot): the box that moves is 122 x 150 px, at most two
 * calls' worth of pixels (GS_BUDGET_PX). */
static const gx_type_t GS_BRAND = {Q(24), Q(3.0), 20};
static const gx_type_t GS_MAKER = {Q(10), Q(1.4), 160};

static uint32_t gs_boot_vowel(uint32_t t)
{
    return t < 1000u ? 1u : t < 1250u ? 2u : t < 1500u ? 3u : t < 1750u ? 4u : t < 2000u ? 5u : 1u;
}

static void scene_boot(void)
{
    static const int32_t NX[3] = {Q(74), Q(166), Q(160)}, NY[3] = {Q(112), Q(104), Q(66)};
    uint32_t t = gs.boot, k;
    gx_xf_t g = GX_XF(73.2, 22.7, 0.78);
    gx_rect(0, 0, Q(240), Q(240), GX_BLUE);
    if (t >= 300u) {                                 /* he pops up from under the hat */
        int32_t sc = t >= 500u ? 4096 : t < 420u ? (int32_t)(t - 300u) * 4588 / 120 : 4588 - (int32_t)(t - 420u) * 492 / 80;
        int32_t hop = 0;
        gx_xf_t b;
        if (t >= 2250u && t < 2650u) {
            uint32_t h = (t - 2250u) % 200u;
            hop = (int32_t)(h < 100u ? h : 200u - h) * Q(4) / 100;
        }
        b.s = g.s * sc / 4096;
        b.tx = g.tx + (((Q(60) * g.s) >> 12) - ((Q(60) * b.s) >> 12));
        b.ty = g.ty + (((Q(156) * g.s) >> 12) - ((Q(156) * b.s) >> 12)) - hop;
        if (b.s > 0)
            gs_gnome(&b, gs_boot_vowel(t), t >= 2250u, Q(2));
    }
    {   /* the hat over him: its outline draws itself, then it fills */
        uint32_t fill = t >= 330u ? 255u : t < 200u ? 0u : (t - 200u) * 255u / 130u;
        uint32_t hat = t < 300u ? t * 1000u / 300u : 1000u, tip = t < 180u ? 0u : t < 330u ? (t - 180u) * 1000u / 150u : 1000u;
        int32_t hop = 0;
        gx_xf_t h = g;
        if (t >= 2250u && t < 2650u) {
            uint32_t q = (t - 2250u) % 200u;
            hop = (int32_t)(q < 100u ? q : 200u - q) * Q(4) / 100;
        }
        h.ty -= hop;
        if (fill)
            gx_path(&h, GA_G_HAT, gs_mix(GX_RED, GX_BLUE, fill), GX_NONE, 0);
        if (hat) {
            gx.trim = hat < 1000u ? hat : 0u;
            gx_path(&h, GA_G_HAT, GX_NONE, GX_INK, Q(2));
        }
        if (tip) {
            gx.trim = tip < 1000u ? tip : 0u;
            gx_path(&h, GA_G_TIP, GX_NONE, GX_INK, Q(2));
        }
        gx.trim = 0;
    }
    for (k = 0; k < 3u; k++) {                      /* three notes float up while he sings */
        uint32_t t0 = 700u + k * 300u;
        if (t >= t0 && t < t0 + 700u) {
            uint32_t u = t - t0, a = u < 100u ? u * 255u / 100u : u > 500u ? (700u - u) * 255u / 200u : 255u;
            gs_float_note(NX[k] + (int32_t)u * Q(3) / 700, NY[k] - (int32_t)u * Q(16) / 700, GX_BLUE, a);
        }
    }
    for (k = 0; k < 4u; k++) {                       /* the four knob dots pop in */
        uint32_t t0 = 2000u + k * 100u;
        if (t >= t0) {
            uint32_t u = t - t0;
            int32_t r = u >= 150u ? Q(6) : u < 90u ? (int32_t)u * Q(7.5) / 90 : Q(7.5) - (int32_t)(u - 90u) * Q(1.5) / 60;
            gx_circle_s(Q(91.5) + (int32_t)k * Q(19), Q(161), r, GS_FIELD[k], GX_PAPER, Q(2));
        }
    }
    gx_text(Q(120), Q(195), &GS_BRAND, 1, GN_NAME, GX_PAPER);
    gx_text(Q(120), Q(212), &GS_MAKER, 1, "BY " GN_MAKER, GX_PAPER);
}

static void parts_boot(void)
{
    uint32_t t = gs.boot;
    gs_part(60, 0, 182, 150, t < 2700u ? t + 1u : 0xFFFFu);      /* him, the hat, the notes: a frame each 66 ms */
    gs_part(80, 150, 160, 172, t < 2000u ? 1u : t < 2500u ? t + 2u : 0xFFFFu);   /* the knob dots */
    gs_part(0, 172, 240, 240, 1);                                 /* the name */
}

/* a part's sound in a few words: a SAMPLE track whose SET is not its preset's (Easy's kit: PERC)
 * by its SET, else as Felucca's pages name it (b holds 13) */
static void gs_sound_name(uint32_t p, char *b)
{
    const track_t *t = &trk[p];
    uint32_t set = (uint32_t)t->p[P_E0] % SMP_NALL;
    if (t->eng_req == GE_SAMPLE && user_of(t) >= UP_SLOTS && set != SMP_SET_ORIG[t->preset % SMP_NSETS])
        str_cpy(b, SMP_ALL_NAMES[set], 13);
    else
        trk_short_name(p, b);
}

/* --------------------------------------------- saved, safe to unplug (3g) --- */
/* FM1-Saved, after the app saved a project and the synth read it back from flash. The lines at the
 * bottom say what plays what with no phone attached: which MIDI channels (the 3.5 mm jack and USB
 * alike) play which track, read from seq.c's own routing (midi_track) for every channel, so they
 * hold whatever GLO > SYSTEM ROUT says (CH1-4: channels 1 to 4 play tracks 1 to 4 and every other
 * channel the selected track; SEL: every channel the selected track), and say DRUMS for a track
 * that holds a kit. The app's own names for the controllers need the editor protocol to carry them,
 * which this build does not add. */
typedef struct {
    uint8_t lo, hi, trk;                  /* channels lo..hi (0..15) play track trk */
} gs_route_t;

static int gs_is_kit(uint32_t p)                  /* a track that holds a kit: DRUM, or SAMPLE's PERC set */
{
    const track_t *t = &trk[p % NTRK];
    return t->eng_req == GE_DRUM || (t->eng_req == GE_SAMPLE && t->p[P_E0] == GN_KIT_SET);
}

/* the routing as at most three claims, read from midi_track for each of the 16 channels: a run of
 * channels that each play their own track (lo..hi play tracks lo..hi: trk 0xFF), or a run that plays
 * one track; then, while there is room, channel 4 alone when it plays a kit. 0: more than three
 * (no routing of seq.c's makes that: the screen then says nothing rather than something untrue) */
static uint32_t gs_routes(gs_route_t *r)
{
    uint32_t ch, n = 0, e, t[16];
    for (ch = 0; ch < 16u; ch++)
        t[ch] = (uint32_t)(midi_track(ch) - trk) % NTRK;
    for (ch = 0; ch < 16u; ch = e + 1u) {
        int own = t[ch] == ch && ch + 1u < 16u && t[ch + 1u] == ch + 1u;
        for (e = ch; e + 1u < 16u; e++) {
            int next_own = t[e + 1u] == e + 1u && e + 2u < 16u && t[e + 2u] == e + 2u;
            if (own ? t[e + 1u] != e + 1u : t[e + 1u] != t[ch] || next_own)
                break;
        }
        if (n == 3u)
            return 0;
        r[n].lo = (uint8_t)ch;
        r[n].hi = (uint8_t)e;
        r[n].trk = (uint8_t)(own ? 0xFFu : t[ch]);
        n++;
    }
    for (ch = 0; ch < n && n < 3u; ch++)
        if (r[ch].trk == 0xFFu && r[ch].lo <= 3u && r[ch].hi >= 3u && t[3] == 3u && gs_is_kit(3)) {
            r[n].lo = r[n].hi = r[n].trk = 3;
            n++;
            break;
        }
    return n;
}

static void gs_route_text(const gs_route_t *r, char *b)   /* b holds 24: "CH 1-4: TRACKS 1-4", "ALL CH: DRUMS 4" */
{
    if (r->lo == 0u && r->hi == 15u) {
        str_cpy(b, "ALL CH: ", 24);
    } else {
        str_cpy(b, "CH ", 24);
        fmt_int(b + str_len(b), r->lo + 1);
        if (r->hi != r->lo) {
            str_cpy(b + str_len(b), "-", 24 - str_len(b));
            fmt_int(b + str_len(b), r->hi + 1);
        }
        str_cpy(b + str_len(b), ": ", 24 - str_len(b));
    }
    if (r->trk == 0xFFu) {
        str_cpy(b + str_len(b), "TRACKS ", 24 - str_len(b));
        fmt_int(b + str_len(b), r->lo + 1);
        str_cpy(b + str_len(b), "-", 24 - str_len(b));
        fmt_int(b + str_len(b), r->hi + 1);
        return;
    }
    str_cpy(b + str_len(b), gs_is_kit(r->trk) ? "DRUMS " : "TRACK ", 24 - str_len(b));
    fmt_int(b + str_len(b), r->trk + 1);
}

/* the panel's SAVE (pick 2): the Gnome cheers SAVED once the song is read back from flash the same; FULL
 * when each of the four song slots holds a song of someone else's (nothing is written over); NOT SAVED when
 * the flash did not take it */
static void scene_saved_mine(void)
{
    static const char *const WORD[5] = {"", "SAVED", "FULL", "NOT SAVED", "SAVED"};
    static const char *const LINE[5] = {"", "SAFE TO UNPLUG", "4 SONGS: NO ROOM", "TRY AGAIN (SAVE)",
                                        "NOT KEPT AS EASY"};
    gx_xf_t g = GX_XF(22, 78, 0.74);
    gs_head();
    gs_knobs();
    gs_field(gs.saved == 1u ? GX_YELLOW : GX_PAPER);
    if (gx_visible(10, 70, 120, 210))
        gs_gnome(&g, gs.saved == 1u ? 1u : 0u, gs.saved == 1u, Q(2.4));
    {
        gx_type_t big = {Q(22), Q(2.8), 20};
        gx_text(Q(172), Q(132), &big, 1, WORD[gs.saved % 5u], GX_INK);
    }
    gx_text(Q(172), Q(156), gs.saved == 4u ? &GS_SMALL : &GS_LABEL, 1, LINE[gs.saved % 5u], GX_INK);
    if (gs.saved == 4u)                               /* (round 9: Easy's record of the slot not written) */
        gx_text(Q(172), Q(170), &GS_SMALL, 1, "NEXT SAVE: A NEW SLOT", GX_INK);
    gs_band("SAVED AS " GN_SONG);
}

static void scene_saved_app(void)
{
    static const gx_xf_t LINK = GX_XF(10, 8, 0.5833), MARK = GX_XF(193.82, 46.64, 0.23), CHECK = GX_XF(0, 0, 1);
    gs_route_t r[3];
    uint32_t i, n = gs_routes(r);
    gx_rect(0, 0, Q(240), Q(240), GX_GROUND);
    gx_rect(0, 0, Q(240), Q(30), GX_PAPER);
    gx_rect(0, Q(29.25), Q(240), Q(1.5), GX_INK);
    if (gs.linked) {
        gx_path(&LINK, GA_LINK, GX_NONE, GX_INK, Q(2.6));
        gx_text(Q(30), Q(18.5), &GS_LABEL, 0, "LINKED BY USB", GX_INK);
    }
    gx_rrect(Q(10), Q(38), Q(220), Q(124), Q(16), GX_YELLOW, GX_INK, Q(1.5));
    gx_circle(&MARK, Q(66), Q(62), Q(56), GX_NONE, GX_INK, Q(7.5));
    gx_path(&MARK, GA_MARK, GX_NONE, GX_INK, Q(7.5));
    for (i = 0; i < 4u; i++)
        gx_circle_s(Q(197.75) + (int32_t)i * Q(7.5), Q(81), Q(2.8), GS_FIELD[i], GX_INK, Q(1.5));
    gx_circle_s(Q(120), Q(82), Q(28), GX_NONE, GX_INK, Q(2.25));
    gx_path(&CHECK, "M106 83 L116 93 L135 72", GX_NONE, GX_INK, Q(3));
    {
        gx_type_t big = {Q(14), Q(1.9), 20};
        gx_text(Q(120), Q(136), &big, 1, "SAVED ON THIS FM-1", GX_INK);
    }
    gx_text(Q(120), Q(152), &GS_LABEL, 1, "SAFE TO UNPLUG", GX_INK);
    for (i = 0; i < n; i++) {
        char line[24];
        gx_xf_t ic = {Q(52), Q(173.5) + (int32_t)i * Q(18), GX_S14};
        gs_route_text(&r[i], line);
        gx_path(&ic, r[i].trk == 0xFFu ? GA_KEYTAR : gs_is_kit(r[i].trk) ? GA_PADS : GA_RIBBON, GX_NONE, GX_INK, Q(2.6));
        gx_text(Q(74), Q(184) + (int32_t)i * Q(18), &GS_LABEL, 0, line, GX_INK);
    }
}

static void scene_saved(void)
{
    if (gs.saved)
        scene_saved_mine();
    else
        scene_saved_app();
}

static void parts_saved(void)
{
    gs_route_t r[3];
    uint32_t i, n, sig;
    if (gs.saved) {
        parts_head();
        parts_knobs();
        gs_part(0, 70, 240, 240, gs.saved + 1u);
        return;
    }
    n = gs_routes(r);
    sig = n + 1u;
    for (i = 0; i < n; i++)
        sig = sig * 131u + r[i].lo * 17u + r[i].hi * 257u + r[i].trk * 4099u +
              (r[i].trk != 0xFFu && gs_is_kit(r[i].trk)) * 7u;
    gs_part(0, 0, 240, 31, gs.linked + 1u);
    gs_part(0, 31, 240, 240, sig);
}
