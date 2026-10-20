/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kyle Palaniuk */
/* Gnomad (FORK.md): the Advanced skin, phase 1 (option C: sections 1, 2, 3, 7 and 8). Included by felucca.c after
 * the UI files (one compilation unit); the declarations are in gn_skin.h.
 *
 * MENU > COLOR > GNOMAD, the ninth palette (runtime index 8, stored as the legacy PAPER id 13), turns it on; any
 * other palette turns it off, and then every hook below returns at once and upstream draws as it always does.
 * With it on:
 *   the palette    GNOMAD's sixteen roles exactly (section 2), after upstream's derivation, light, not MONO;
 *   the paint      one colour per knob (gsk_paint): knob 1 blue, 2 green, 3 yellow, 4 vermilion, the text on
 *                  them paper, paper, ink, ink; Leo's four cards keep his place and size, ground-filled with an
 *                  ink keyline (2 px while the knob was just turned) and a 3 px spine in the knob's colour inside
 *                  it, the gauge in the knob's colour (a knob with nothing to do: a dash); the K1..K4 badges in
 *                  their knob's colour; a REC badge (vermilion) with ink on it; on the drum grid the cursor (knob 1),
 *                  the selected lane (knob 2) and accented hits (knob 4) in those knobs' colours; parts 1..4 are
 *                  never painted as knobs;
 *   the words      every card label, page title, footer hint, layer name, menu item, confirmation, message and
 *                  note in plain words (gn_skin_words.c), Felucca's code beside the name where the card has room;
 *                  the values stay exactly as upstream formats them (phase 2's rows bring the choice words); the
 *                  footer keeps the engine's icon but not its name on HOME (whose title names the engine) and
 *                  beside a long title, so the sound's name keeps 64 px.
 * Some of upstream's drawing functions are drawn here instead, line for line as upstream draws them but for the
 * words and the paint (the footer, the menu, the dialog, NAME's panel and footer, the calibration title, the
 * song list, the drum grid): a guarded line at the top of upstream's function calls ours. Nothing here runs from
 * the audio code, the start-up path (beyond palette_set's few assignments when GNOMAD was saved) or the update
 * path, and nothing is stored but the palette's id in the settings record upstream already writes. */

/* -------------------------------------------------------------- palette --- */
#define GSK_RGB(v) RGB(((v) >> 16) & 255u, ((v) >> 8) & 255u, (v) & 255u)
#define GSK_PAPER GSK_RGB(0xF9F8F4u)
#define GSK_GROUND GSK_RGB(0xEEEDE8u)
#define GSK_INK GSK_RGB(0x17181Bu)
#define GSK_INK2 GSK_RGB(0x5B5D62u)           /* second ink */
#define GSK_WELLS GSK_RGB(0xE3E2DCu)
#define GSK_BLUE GSK_RGB(0x2352E6u)           /* knob 1 */
#define GSK_GREEN GSK_RGB(0x0B7D4Fu)          /* knob 2 */
#define GSK_YELLOW GSK_RGB(0xF4B400u)         /* knob 3 */
#define GSK_VERMILION GSK_RGB(0xF2622Au)      /* knob 4 */
static const uint16_t GSK_KNOB[4] = {GSK_BLUE, GSK_GREEN, GSK_YELLOW, GSK_VERMILION};

/* palette_set(i), after upstream derived palette i: GNOMAD's exact roles (option C, section 2) */
GSK_MIN static void gsk_palette(uint32_t i)
{
    gsk.on = i % NPALETTES == GSK_PAL;
    if (!gsk.on)
        return;
    ux.bg = GSK_PAPER;
    ux.surf = GSK_GROUND;
    ux.text = ux.theme = ux.accent = ux.sel = ux.key = GSK_INK;
    ux.mid = ux.dim = GSK_INK2;
    ux.line = ux.tint = ux.raise = ux.lane = GSK_WELLS;
    ux.ink = GSK_PAPER;
    ux.rec = GSK_VERMILION;
    ux.grid = GSK_GROUND;
    ux.light = 1;
    ux.mono = 0;
}
/* the settings record's id: GNOMAD as the legacy PAPER id (palette_from_stored reads it back), the rest as upstream */
GSK_MIN static uint32_t gsk_palette_to_stored(uint32_t i)
{
    return i % NPALETTES == GSK_PAL ? GSK_STORED : UI_PAL_TAG + i % NPALETTES;
}
/* a stored id the backup may restore: an old one (below 20) or one of upstream's eight tagged ones; never a tagged
 * GNOMAD (UI_PAL_TAG + 8 = 72), which nothing writes and unmodified 1.0.1 refuses (13 is GNOMAD's id) */
GSK_MIN static int gsk_palette_stored_ok(uint32_t v)
{
    return v < 20u || (v >= UI_PAL_TAG && v - UI_PAL_TAG < GSK_PAL);
}

/* ---------------------------------------------------------------- paint --- */
/* gsk_paint(role, knob, state, fallback): the skin's colour for a role of knob 0..3 (GSK_NO_KNOB: none), in a
 * state; fallback (upstream's colour) when the skin is off or has nothing to say */
enum { GSKP_FILL, GSKP_ON_FILL, GSKP_GAUGE, GSKP_SPINE };
enum { GSKS_NORMAL, GSKS_HOT, GSKS_DIM };
#define GSK_NO_KNOB 4u
GSK_MIN static uint16_t gsk_paint(uint32_t role, uint32_t knob, uint32_t state, uint16_t fallback)
{
    if (!gsk.on || knob > 3u)
        return fallback;
    switch (role) {
    case GSKP_ON_FILL:                                  /* text on the knob's colour: paper, paper, ink, ink */
        return knob < 2u ? GSK_PAPER : GSK_INK;
    case GSKP_GAUGE:
        return state == GSKS_DIM ? T_DIM : GSK_KNOB[knob];
    default:                                          /* GSKP_FILL, GSKP_SPINE */
        return GSK_KNOB[knob];
    }
}
/* cv_keycap, by the badge it is: a knob's (K1..K4) in its colour with its text's colour (unavailable: as upstream);
 * the REC badge (upstream fills it with T_REC, vermilion here) with ink on it; every other badge as upstream */
GSK_MIN static void gsk_keycap(uint32_t id, uint16_t *fill, uint16_t *ink)
{
    if (!gsk.on)
        return;
    if (id >= KC_K1 && id <= KC_K4 && *fill != T_DIM) {
        *fill = gsk_paint(GSKP_FILL, id - KC_K1, GSKS_NORMAL, *fill);
        *ink = gsk_paint(GSKP_ON_FILL, id - KC_K1, GSKS_NORMAL, *ink);
    } else if (id == KC_REC && *fill == T_REC) {
        *ink = GSK_INK;
    }
}

#ifndef GSK_CORE_ONLY                             /* (tests/gn_skin_persist_test.c: the palette alone) */
#include "gn_skin_words.c"
#ifndef GSK_HOOK_LABEL
#define GSK_HOOK_LABEL(key, upstream, word) ((void)0)   /* host tests (tests/gn_skin_test.c): every card's lookup */
#define GSK_HOOK_VALUE(key, upstream, word) ((void)0)   /* .. and every card value's */
#endif

/* upstream's own fixed wording s, drawn at site (GSKC_*), in words; never a name, never user content */
GSK_MIN static const char *gsk_text(uint32_t site, const char *s) { return gsk.on ? gsk_word(GSKW_PHRASE, site, GSKW_CELL, s) : s; }

/* ---------------------------------------------------------------- cards --- */
/* what card c is of (gn_skin_words.c GSKK_*), as ui_draw.c draw_columns and ui_layer.c layer_cards decide it; for a
 * card of a descriptor (HOME, an ordinary page, the SCL layer's) also the descriptor and its value */
GSK_MIN static uint32_t gsk_card(uint32_t c, const param_desc_t **d, int16_t **vp)
{
    const page_t *pg = cur_page();
    uint32_t e = TSEL->eng_req % NENGINES, id;
    c &= 3u;
    *d = 0;
    *vp = 0;
    if (ui.layer == LAYER_FX || ui.layer == LAYER_GLO || ui.layer == LAYER_EDIT)
        return GSKK_GR(GSKG_LAYER(ui.layer), c);
    if (ui.home) {
        id = ENGINES[e]->knob[c];
        *d = home_param(c, vp);
        return id >= P_E0 ? GSKK_ENG(e, id - P_E0) : GSKK_TRK(id);
    }
    switch (pg->graph) {
    case GR_SONG: case GR_CHANCE: case GR_MOTION: case GR_TOOLS: case GR_TRK: case GR_BROWSE: case GR_PATS:
    case GR_USER: case GR_SLICES: case GR_MOD:
        return GSKK_GR(pg->graph, c);
    default:
        break;
    }
    if (pg->scope == SC_TRK)
        return GSKK_GR(GR_TRK, c);
    if (pg->scope == SC_STEP)
        return GSKK_GR(drum_track(TSEL) ? GSKG_DRUM : GR_ROLL, c);
    id = pg->id[c];
    if (!(pg->scope == SC_GLOBAL && id == G_MIDI))      /* (SYSTEM's knob 1: its card shows USB's status) */
        *d = page_desc(pg, c, vp);
    if (pg->scope == SC_GLOBAL)
        return GSKK_GLB(id);
    if (pg->scope == SC_ENGINE)
        return GSKK_ENG(e, id - P_E0);
    return GSKK_TRK(id);
}
GSK_MIN static int gsk_ieq(const char *a, const char *b, int prefix)     /* a == b ignoring case (prefix: b starts a) */
{
    for (; *a && *b; a++, b++)
        if ((*a | 32) != (*b | 32))
            return 0;
    return !*b && (prefix || !*a);
}
/* the choice upstream drew as val for value v of descriptor d (params.c param_format picks it the same way), in words;
 * else val. Two lists of numbered tokens read by rule (LOFI's decay steps, FM6's patches) */
GSK_MIN static const char *gsk_choice(const param_desc_t *d, int32_t v, const char *val)
{
    uint32_t k, n = 0;
    if (!d || !d->names) {
        if (d && (d->fmt == F_DB || d->fmt == F_ONOFF) && (str_eq(val, "OFF") || str_eq(val, "ON")))
            return gsk_word(GSKW_CHOICE, gsk_choices_of(N_ONOFF) << 8 | (uint32_t)str_eq(val, "ON"), GSKW_CELL, val);
        return val;
    }
    v = clamp(v, d->min, d->max);
    if (d->fmt == F_ENUM) {
        k = (uint32_t)v;
    } else {                                          /* F_INT with a 0-terminated list: the range split evenly */
        while (d->names[n])
            n++;
        k = (uint32_t)(v - d->min) * n / (uint32_t)(d->max - d->min + 1);
    }
#if !GN_DROP_LOFI
    if (d->names == N_DCY)
        return gsk_word(GSKW_PHRASE, GSKC_DCY, GSKW_CELL, val);
#endif
#if !GN_DROP_FM6
    if (d->names == N_FM6_PATCH)
        return gsk_word(GSKW_PHRASE, GSKC_PATCH, GSKW_CELL, val);
#endif
    n = gsk_choices_of(d->names);
    return n < 255u && k < 256u ? gsk_word(GSKW_CHOICE, n << 8 | k, GSKW_CELL, val) : val;
}
/* MOD's source (src 1) or target (0) v, shown upstream as val, in words: a target of the engine's own settings
 * (E1..E8) by its card label */
GSK_MIN static const char *gsk_modword(int src, int32_t v, const char *val)
{
    if (src)
        return gsk_word(GSKW_CHOICE, gsk_choices_of(N_MSRC) << 8 | (uint32_t)clamp(v, 0, MS_N - 1), GSKW_CELL, val);
    v = clamp(v, 0, MD_N - 1);
    if (v >= MD_E1)
        return gsk_word(GSKW_LABEL, GSKK_ENG(TSEL->eng_req % NENGINES, (uint32_t)(v - MD_E1)), GSKW_CELL, val);
    return gsk_word(GSKW_CHOICE, gsk_choices_of(N_MDST) << 8 | (uint32_t)v, GSKW_CELL, val);
}
/* the value card c shows, in words: by its descriptor and value, MOD's source and target, or the card's own tokens */
GSK_MIN static const char *gsk_value(uint32_t c, uint32_t key, const param_desc_t *d, const int16_t *vp, const char *val)
{
    const char *w;
    if (!ui.home && !ui.layer && cur_page()->graph == GR_MOD && (c == 1u || c == 2u))
        return gsk_modword(c == 1u, TSEL->p[P_M1SRC + 3u * mod_ui_slot + c - 1u], val);
    if (d && vp && (w = gsk_choice(d, *vp, val)) != val)
        return w;
    return gsk_word(GSKW_CARD, key, GSKW_CELL, val);
}

#if GN_ENGINES
/* round EW (gn_core_pages.c): why card c does nothing on the selected track's Gnomad sound, two short lines (the
 * second after the first's NUL); 0: it does something (or the track is not on a Gnomad engine) */
GSK_MIN static const char *gsk_gn_why(uint32_t c)
{
    static const char WHY[] = "Drums\0fade out\0Not on\0this sound\0Always\0a sine\0Starts on\0the note\0No fade\0here\0"
        "No drive\0per part\0Mono:\0no pan\0Echo has\0no tone\0A plate\0reverb\0Size sets\0it\0Slices\0play whole";
    static char knob[] = "Knob 1\0has it";
    const char *t = WHY;
    uint32_t w = gn_why[c & 3u];
    if (ui.home || ui.layer || !w)
        return 0;
    if (w >= GNW_KNOB) {
        knob[5] = (char)('1' + (w - GNW_KNOB));
        return knob;
    }
    while (--w)                                       /* (reason w: the (2w - 1)-th line) */
        t += str_len(t) + 1u, t += str_len(t) + 1u;
    return t;
}
#endif

/* ui_draw.c draw_column(c, ...): knob c's card, Leo's 57 x 44 at his place, in the skin; 0 = upstream draws it.
 * Ground in an ink keyline with a 3 px spine (x 1..3) in the knob's colour; just turned, the keyline is 2 px on the
 * other three sides. The label in words (ink), upstream's code after it (second ink) when both fit Leo's 50 px
 * and the code is not the word itself; the value in words, measured first: upstream's token when the word would
 * not fit (M, else S, as upstream); the unit; the gauge in the knob's colour. Drawn when something changed. */
GSK_MIN static int gsk_column(uint32_t c, const char *label, const char *val, const char *unit, uint16_t vc, int32_t ratio)
{
    char key[12];
    const char *name, *code = 0, *word;
    const char *why = 0;
    const param_desc_t *d;
    int16_t *vp;
    int hot = c == ui.hot_col && ui.hot_t, i;
    int32_t room = COL_W - 7, uw, x;                  /* (50 px, as the label's line: upstream's value has 49) */
    uint32_t kn, h, ck;
    int32_t kid;
    const aafont_t *vf = &AF_M;
    uint16_t kc = gsk_paint(GSKP_SPINE, c, GSKS_NORMAL, T_THEME);
    uint16_t vcol = vc == T_DIM || vc == T_MID ? vc : T_TEXT;
    if (!gsk.on)
        return 0;
    fmt_named = 0;                                    /* (params.c: a name, for this card only) */
    if (str_eq(unit, label))
        unit = "";                                    /* "BPM 124 BPM", "USB OFF USB": the label says it */
    ck = gsk_card(c, &d, &vp);
    name = gsk_word(GSKW_LABEL, ck, GSKW_CELL, label);
#if GN_ENGINES
    why = !label[0] && !val[0] ? gsk_gn_why(c) : 0;   /* .. a knob with nothing to do there: why, in words */
#endif
    GSK_HOOK_LABEL(ck, label, name);
    unit = gsk_word(GSKW_UNIT, 0, GSKW_CELL, unit);
    if (name != label && !gsk_ieq(name, label, 1) &&
        text_w(&AF_S, name) + 4 + text_w(&AF_S, label) <= COL_W - 2 - 5)
        code = label;
    kid = kc_tag(val, &kn);
    uw = unit[0] ? text_w(&AF_S, unit) + 3 : 0;
    word = kid >= 0 ? val : gsk_value(c, ck, d, vp, val);
    if (word != val && text_w(&AF_S, word) + uw > room)
        word = val;                                   /* (measured: no room for the word, upstream's token) */
    GSK_HOOK_VALUE(ck, val, word);
    if (text_w(vf, word) + uw > COL_W - 8)            /* (M as upstream decides it, 49 px: its ink overhangs) */
        vf = &AF_S;
    h = str_hash(str_hash(str_hash(str_hash(2166136261u + c * 131u + ux.gen * 7919u, name), word), unit), code ? code : "|") +
        vc * 31u + (uint32_t)(ratio < 0 ? 0 : 1 + ratio / 20) * 104729u + (uint32_t)hot * 1299709u +
        (why ? str_hash(7u, why) : 0u);
    key[0] = 1;                                       /* (upstream's keys never start so: its next draw is whole) */
    for (i = 0; i < 8; i++)
        key[1 + i] = "0123456789abcdef"[(h >> (28 - 4 * i)) & 15u];
    key[9] = 0;
    if (!ui.force && str_eq(key, ui.col[c]))
        return 1;
    str_cpy(ui.col[c], key, sizeof ui.col[c]);
    cv_begin(COL_W, COL_H, T_BG);
    cv_rrect(0, 0, COL_W, COL_H, 4, T_TEXT, T_BG);   /* the ink keyline (just turned: 2 px but on the spine's side) */
    cv_rrect(1, hot ? 2 : 1, COL_W - (hot ? 3 : 2), COL_H - (hot ? 4 : 2), 3, T_SURF, T_TEXT);
    cv_rect(1, 4, 3, COL_H - 8, kc);                  /* the spine, x 1..3 */
    if (why) {                                        /* round EW: nothing to do on this sound: why, two lines */
        cv_text_fit(7, 9, &AF_S, why, T_MID, T_SURF, COL_W - 10);
        cv_text_fit(7, 23, &AF_S, why + str_len(why) + 1u, T_MID, T_SURF, COL_W - 10);
    } else if (!label[0] && !val[0])
        cv_rect(8, 26, 12, 2, T_DIM);                 /* a knob that does nothing here: a dash */
    if (label[0] || val[0]) {
        if (label[0]) {
            x = cv_text_fit(5, 3, &AF_S, name, T_TEXT, T_SURF, COL_W - 2 - 5);
            if (code)
                cv_text_on(x + 4, 3, &AF_S, code, T_MID, T_SURF);
        }
        if (kid >= 0)                                 /* "[OCT+]": the keycap (ink: it would act; second ink: not) */
            x = cv_keycap(5, 20, (uint32_t)kid, vc == T_ACCENT || vc == T_DIM ? vc : T_KEY, T_INK, T_SURF);
        else if (vf == &AF_M)
            x = cv_text_on(5, 17, &AF_M, word, vcol, T_SURF);
        else
            x = cv_text_fit(5, 20, vf, word, vcol, T_SURF, room - uw);
        if (unit[0])
            cv_text_on(x + 3, 20, &AF_S, unit, T_MID, T_SURF);
        if (ratio >= 0) {
            int32_t gw = COL_W - 10, fx = ratio * gw / 1000;
            cv_rrect(5, 38, gw, 3, 1, T_LINE, T_SURF);
            cv_rrect(5, 38, fx < 3 ? 3 : fx, 3, 1, gsk_paint(GSKP_GAUGE, c, vc == T_DIM ? GSKS_DIM : GSKS_NORMAL, T_THEME),
                     T_LINE);
        }
    }
    cv_blit((uint32_t)CARD_X(c), Y_LABEL);
    return 1;
}

/* --------------------------------------------------- header, key hints --- */
/* ui_draw.c draw_head: a message or the layer's name in words (a keycap in it stays a keycap) */
GSK_MIN static int gsk_hint(const char *s)
{
    if (!gsk.on)
        return 0;
    cv_free_hint(106, 6, gsk_text(GSKC_MSG, s), T_TEXT, T_BG, 236 - 106);
    return 1;
}
/* a row of key hints (gfx.c cv_key_row), their words (upstream's literals) looked up at site ctx (GSKC_*) */
GSK_MIN static void gsk_keyrow_ctx(int32_t x0, int32_t x1, int32_t y, const khint_t *h, uint32_t n, uint32_t on, uint16_t under,
                      uint32_t ctx)
{
    khint_t k[3];
    char w[3][24];
    uint32_t i;
    for (i = 0; i < n && i < 3u; i++) {
        k[i] = h[i];
        if (h[i].act) {
            str_cpy(w[i], gsk_word(GSKW_PHRASE, ctx, GSKW_CELL, h[i].act), sizeof w[i]);
            k[i].act = w[i];
        }
    }
    cv_key_row(x0, x1, y, k, i, on, under);
}
GSK_MIN static int gsk_keyrow(int32_t x0, int32_t x1, int32_t y, const void *h, uint32_t n, uint32_t on, uint16_t under)
{
    if (!gsk.on)
        return 0;
    gsk_keyrow_ctx(x0, x1, y, (const khint_t *)h, n, on, under, GSKC_FOOT);
    return 1;
}

/* --------------------------------------------------------------- footer --- */
/* the page's title in words and its place in its family ("Envelope 1/2"); HOME: the engine's ("Analog") */
GSK_MIN static void gsk_title(char *ti, uint32_t n)
{
    const page_t *pg = cur_page();
    uint32_t e = TSEL->eng_req % NENGINES, i, cnt = 0, k = 0;
    const char *code, *w;
    if (ui.home) {
        w = gsk_word(GSKW_TITLE, GSKW_TKEY(GSKT_HOME, e, 0), GSKW_CELL, ENGINES[e]->name);
        str_cpy(ti, w == ENGINES[e]->name ? "HOME" : w, n);
        return;
    }
    for (i = 0; i < NPAGES; i++)
        if (PAGES[i].fam == pg->fam && page_visible(i)) {
            cnt++;
            if (i == ui.page)
                k = cnt;
        }
    if (pg->scope == SC_ENGINE) {                     /* EDIT: the engine's */
        code = ENGINES[e]->page_title[pg->id[0] != P_E0];
        w = gsk_word(GSKW_TITLE, GSKW_TKEY(pg->id[0] != P_E0 ? GSKT_EDIT2 : GSKT_EDIT1, e, 0), GSKW_CELL, code);
    } else {
        code = grid_on() ? "GRID" : pg->title;
        w = gsk_word(GSKW_TITLE, GSKW_TKEY(pg->fam, pg->graph, pg->id[0]), GSKW_CELL, code);
    }
    str_cpy(ti, w, n);
    if (cnt > 1) {
        str_cpy(ti + str_len(ti), " ", n - str_len(ti));
        fmt_int(ti + str_len(ti), (int32_t)k);
        str_cpy(ti + str_len(ti), "/", n - str_len(ti));
        fmt_int(ti + str_len(ti), (int32_t)cnt);
    }
    if (pg->graph == GR_ROLL && !drum_track(TSEL)) {  /* the piano roll: then its tinted rows' key ("Notes 1/6 · C Minor",
                                                       * upstream shows the key alone), when it leaves the sound 64 px */
        char k[20];
        int32_t sc = clamp(TSEL->p[P_SCALE], 0, (int32_t)NELEM(N_SCALE) - 1);
        str_cpy(k, " \xB7 ", sizeof k);
        str_cpy(k + str_len(k), N_NOTE[(uint32_t)TSEL->p[P_ROOT] % 12u], 4);
        str_cpy(k + str_len(k), " ", 2);
        str_cpy(k + str_len(k), gsk_word(GSKW_CHOICE, gsk_choices_of(N_SCALE) << 8 | (uint32_t)sc, GSKW_CELL, N_SCALE[sc]),
                sizeof k - str_len(k));
        if (text_w(&AF_S, ti) + text_w(&AF_S, k) <= 232 - 12 - 64 - 25 && str_len(ti) + str_len(k) < n)
            str_cpy(ti + str_len(ti), k, n - str_len(ti));
    }
}
/* ui_draw.c draw_footer, as upstream's: row 1 the steps (or the action keys, the grid's page), row 2 the engine,
 * the sound and the page, in words */
GSK_MIN static int gsk_foot(void)
{
    char s[64], pn[16], ti[32];
    const track_t *t = TSEL;
    uint32_t sig;
    const page_t *pg = cur_page();
    const char *ename = ENGINES[TSEL->eng_req % NENGINES]->name;
    int32_t x;
    if (!gsk.on)
        return 0;
    sound_name(t, pn);
    gsk_title(ti, sizeof ti);
    str_cpy(s, ename, sizeof s);
    s[str_len(s) + 1u] = 0;
    s[str_len(s)] = (char)('1' + song.sel);
    str_cpy(s + str_len(s), pn, sizeof s - str_len(s));
    str_cpy(s + str_len(s), ti, sizeof s - str_len(s));
    {   /* step markers: the playhead only when it is in the shown bank, the cursor only in SEQ */
        uint32_t ph = song.playing && t->seq_idx / 16u == ui.bank ? t->seq_idx : 0xFFu;
        sig = str_hash(0x9E3779B9u, s) + ph * 97u + (song.seq_mode ? ui.cursor : 0xFFu) * 3001u + steps_hash(t) +
              (ui.home ? 0u : page_icon(pg)) * 7121u + ui.bank * 7u + (uint32_t)t->p[P_SLEN] * 13u + 0x6E6Fu;
    }
    if (act_cols()) {
        char ha[16], hb[16];
        foot_hint(ha, hb);
        sig += str_hash(str_hash(act_ready() ? 7u : 3u, ha), hb) + foot_rename() * 7717u;
    }
    if (grid_on())
        sig += 0x51EDu + (uint32_t)black_held(GK_ACC) * 977u;
    if (!ui.force && sig == ui.foot_sig)
        return 1;
    ui.foot_sig = sig;
    cv_begin(240, H_FOOT, T_BG);
    if (act_cols()) {                                 /* row 1: what OCT+ / OCT- do */
        char ha[16], hb[16];
        khint_t kh[3];
        uint32_t rn = foot_rename();
        foot_hint(ha, hb);
        kh[0] = (khint_t){KC_OCTUP, ha + 5};
        kh[1] = (khint_t){KC_OCTDN, hb + 5};
        if (rn) {                                     /* USER / PROJECT: EDIT renames, between them */
            kh[2] = kh[1];
            kh[1] = (khint_t){KC_EDIT, "NAME"};
            gsk_keyrow_ctx(8, 232, 2, kh, 3, (act_ready() ? 1u : 0u) | (rn == 2u ? 2u : 0u) | 4u, T_BG, GSKC_FOOT);
        } else {
            gsk_keyrow_ctx(8, 232, 2, kh, 2, act_ready() ? 3u : 2u, T_BG, GSKC_FOOT);
        }
    } else if (grid_on()) {                           /* row 1: the page, and what the keys do */
        char b[16];
        uint32_t len = (uint32_t)t->p[P_SLEN];
        str_cpy(b, "PAGE ", sizeof b);
        fmt_int(b + 5, (int32_t)ui.bank + 1);
        str_cpy(b + str_len(b), "/", 4);
        fmt_int(b + str_len(b), (int32_t)((len + 15u) / 16u));
        cv_text(8, 2, &AF_S, gsk_text(GSKC_FOOT, b), T_THEME);
        if (black_held(GK_ACC)) {
            cv_text_r(232, 2, &AF_S, gsk_text(GSKC_FOOT, "ACCENT"), T_ACCENT, T_BG);
        } else {
            const char *w = gsk_text(GSKC_FOOT, "STEPS");
            cv_key_hint(232 - kh_w(KC_KEYS, w), 2, KC_KEYS, w, 1, T_BG);
        }
    } else {
        uint32_t i;
        for (i = 0; i < 16u; i++) {                   /* row 1: the cursor's bank, 16 bars in 4 groups */
            uint32_t si = ui.bank * 16u + i;
            int32_t sx = 10 + (int32_t)i * 13 + (int32_t)(i / 4u) * 4;
            const step_t *st = &seq_steps(t)[si];
            if (si >= (uint32_t)t->p[P_SLEN])
                continue;
            if (step_on(st))
                cv_rrect(sx, 1, 9, 11, 2, (st->flags & SF_ACCENT) ? T_ACCENT : T_THEME, T_BG);
            else
                cv_rrect(sx, 9, 9, 3, 1, st->time == ST_TIE ? T_MID : T_RAISE, T_BG);
            if (song.seq_mode && si == ui.cursor)
                cv_rect(sx, 14, 9, 2, T_ACCENT);
            else if (song.playing && si == t->seq_idx)
                cv_rect(sx, 14, 9, 2, T_TEXT);
        }
    }
    x = 8;
    if (FELUCCA_ICONS)                                /* row 2: engine icon + name, sound, page */
        x += cv_icon_on(x, 20, 12, engine_icon(ename), T_MID, T_BG) + 5;
    {
        uint32_t pi = ui.home ? ICON_NONE : page_icon(pg);
        int32_t tx = 232 - text_w(&AF_S, ti) - (pi != ICON_NONE ? 16 : 0), ew = text_w(&AF_S, ename);
        if (!FELUCCA_ICONS || (!ui.home && tx - 12 - (x + (ew < 80 ? ew : 80) + 10) >= 64))   /* (HOME, whose title names
                                                                                                 * the engine, or a long title: the icon alone) */
            x = cv_text_fit(x, 19, &AF_S, ename, T_THEME, T_BG, 80);
        else
            x -= 10;
        cv_free_text(x + 10, 19, &AF_S, pn, T_TEXT, T_BG, tx - 12 - (x + 10));
        if (pi != ICON_NONE)
            cv_icon_on(tx, 20, 12, pi, T_MID, T_BG);
        cv_text_r(232, 19, &AF_S, ti, T_MID, T_BG);
    }
    cv_blit(0, Y_FOOT);
    return 1;
}

/* --------------------------------------------------------------- dialog --- */
/* ui_draw.c draw_confirm, as upstream's, in words. The question is made from the dialog's kind (ui.confirm) and its
 * part, project or slot, never from upstream's text; the second line is upstream's as it is (the sound's, project's
 * or pattern's name), but for the one explanation upstream puts there itself, which this finds as upstream does
 * (a project without a name). Then No and Yes */
GSK_MIN static int gsk_confirm(void)
{
    char a[24], b[24], wa[40], d[8];
    const aafont_t *tf = &AF_M;
    const char *no, *yes, *q, *wb = b;
    uint32_t k = ui.confirm_trk, kind = ui.confirm;
    if (!gsk.on)
        return 0;
    confirm_text(a, b);
    d[0] = (char)('1' + k % NTRK);                     /* the part (CLEAR TRACK, the SEQUENCE ones, MOTION) */
    d[1] = 0;
    if (kind == CF_OVR_PROJ)
        d[0] = (char)('A' + (k & 3u));                /* the project */
    else if (kind == CF_OVR_USER || kind == CF_ERASE_USER)
        up_slot_label(d, k);                          /* the user sound's slot, "U07" */
    q = gsk_word(GSKW_CONFIRM, kind, GSKW_CELL, a);
    if (q == a)
        str_cpy(wa, a, sizeof wa);                    /* (a kind this does not know: upstream's question) */
    else
        gsk_fill(q, d, sizeof d, wa, sizeof wa);
    if (kind == CF_OVR_PROJ) {
        char t[16];
        if (!project_name(k & 3u, t) || !t[0])
            wb = gsk_text(GSKC_CONFIRM, b);           /* upstream's "SONG PATTERN CHANGES" */
    }
    no = gsk_text(GSKC_CONFIRM, "NO");
    yes = gsk_text(GSKC_CONFIRM, "YES");
    lcd_fill(0, H_HEAD, 240, 240 - H_HEAD, T_BG);
    ui.head_sig = ~0u;
    cv_begin(DLG_W, DLG_H, T_BG);
    cv_rrect(0, 0, DLG_W, DLG_H, 8, T_SURF, T_BG);
    cv_icon_on(DLG_W / 2 - 8, 12, 16, ui.confirm == CF_CLEAR_MOTION ? ICON_X_MOTION_DEL : ICON_X_WARN, T_ACCENT, T_SURF);
    if (text_w(tf, wa) > DLG_W - 16)
        tf = &AF_S;
    cv_text_c(DLG_W / 2, 34, tf, wa, T_TEXT, T_SURF);
    if (wb[0]) {
        char f[32];
        text_fit(f, sizeof f, wb, &AF_S, DLG_W - 16);  /* a name: free text */
        cv_text_flags(DLG_W / 2 - text_w(&AF_S, f) / 2, 56, &AF_S, f, T_MID, T_SURF, 8u | (f[str_len(f) - 1u] == ELLIPSIS));
    }
    cv_rrect(10, 80, 90, 26, 6, T_RAISE, T_SURF);     /* OCT-: No */
    cv_key_hint(55 - kh_w(KC_OCTDN, no) / 2, 87, KC_OCTDN, no, 1, T_RAISE);
    cv_rrect(108, 80, 90, 26, 6, T_THEME, T_SURF);    /* OCT+: Yes (on the ink fill: a paper keycap, ink label) */
    {
        int32_t x = 153 - kh_w(KC_OCTUP, yes) / 2;
        x = cv_keycap(x, 87, KC_OCTUP, T_INK, T_THEME, T_THEME);
        cv_text_on(x + KH_GAP, 86, &AF_S, yes, T_INK, T_THEME);
    }
    cv_blit(DLG_X, DLG_Y);
    return 1;
}

/* ----------------------------------------------------------------- menu --- */
/* ui_menu.c draw_menu (and its menu_head), as upstream's, in words; GNOMAD's swatches are the four knobs' */
GSK_MIN static void gsk_menu_head(void)
{
    cv_begin(240, H_HEAD, T_BG);
    cv_icon_on(8, 4, 16, ui.menu == 2 ? ICON_X_INFO : ICON_X_COG, T_THEME, T_BG);
    cv_text(30, 3, &AF_M, gsk_text(GSKC_MENU, ui.menu == 2 ? "ABOUT / CREDITS" : "MENU"), T_TEXT);
    if (ui.menu >= 2) {
        const char *w = song.rec ? 0 : gsk_text(GSKC_MENU, "BACK");
        int32_t x = 232 - kh_w(KC_OCTDN, w);
        cv_key_hint(x, 6, KC_OCTDN, w, 1, T_BG);
        draw_rec_mark(x - 20, T_BG);
    } else {
        draw_rec_mark(178, T_BG);
    }
    cv_blit(0, Y_HEAD);
}
GSK_MIN static int gsk_menu(void)
{
    static const uint16_t ICO[MI_COUNT] = {ICON_X_PALETTE, ICON_X_SPEAKER, ICON_X_TIMER, ICON_X_STAR_O, ICON_X_DOCTOR,
                                           ICON_X_INFO, ICON_X_BACK};
    static const char *const LOWCUT[3] = {"OFF", "LOWCUT", "BASS+"};
    uint32_t i, pass, sig;
    if (!gsk.on)
        return 0;
    sig = ui.menu * 7u + ui.menu_sel * 131u + settings.palette * 1009u + settings.lowcut * 7919u +
          settings_hold * 3511u + settings_leds * 6151u + song.rec * 65537u + song.sel * 13u +
          (ui.menu == 2 ? ui.menu_scroll * 48611u : 0u);
    if (!ui.force && sig == ui.menu_sig)
        return 1;
    ui.menu_sig = sig;
    gsk_menu_head();
    if (ui.menu >= 2) {                               /* ABOUT and the credits: upstream's document, as it is */
        int32_t scroll = ui.menu_scroll, max = menu_scroll_max();
        int32_t thumb = MENU_DOC_H * MENU_DOC_H / (max + MENU_DOC_H);
        if (thumb < 12) thumb = 12;
        for (pass = 0; pass < 2u; pass++) {
            cv_begin(240, pass ? MENU_DOC_H - 124u : 124u, T_BG);
            cv_oy = pass ? -124 : 0;
            cv_scroll = 1;
            menu_document(-scroll, 1);
            cv_scroll = 0;
            cv_rrect(235, 2, 3, MENU_DOC_H - 4, 1, T_LINE, T_BG);
            cv_rrect(235, 2 + (max ? scroll * (MENU_DOC_H - 4 - thumb) / max : 0), 3, thumb, 1, T_MID, T_LINE);
            cv_oy = 0;
            cv_blit(0, H_HEAD + pass * 124u);
        }
        cv_begin(240, 240 - (H_HEAD + MENU_DOC_H), T_BG);
        cv_key_hint(8, 4, KC_PRESETS, gsk_text(GSKC_MENU, "SCROLL"), 1, T_BG);
        cv_blit(0, H_HEAD + MENU_DOC_H);
        return 1;
    }
    for (pass = 0; pass < 2u; pass++) {
        int32_t top = pass ? MENU_SPLIT : H_HEAD;
        cv_begin(240, (uint32_t)(pass ? 240 - MENU_SPLIT : MENU_SPLIT - H_HEAD), T_BG);
        cv_oy = -top;
        for (i = 0; i < MI_COUNT; i++) {
            int32_t y = MENU_Y0 + (int32_t)i * MENU_ROW;
            int sel = i == ui.menu_sel;
            uint16_t bg = sel ? T_THEME : T_SURF, fg = sel ? T_INK : T_TEXT, val = sel ? T_INK : T_THEME;
            if (y + 24 <= top || y >= top + (int32_t)cv_h)
                continue;
            cv_rrect(4, y, 232, 24, 6, bg, T_BG);
            cv_icon_on(12, y + 4, 16, ICO[i], sel ? T_INK : T_MID, bg);
            cv_text_on(36, y + 5, &AF_S, gsk_text(GSKC_MENU, MI_NAME[i]), fg, bg);
            if (i == MI_LOWCUT)
                cv_text_r(228, y + 3, &AF_M, gsk_text(GSKC_MENU, LOWCUT[settings.lowcut % 3u]), val, bg);
            if (i == MI_HOLD) {
                char b[8] = "0.4";
                b[2] = (char)('0' + HOLD_MS[settings_hold % 4u] / 100u);
                cv_text_r(cv_text_r(228, y + 6, &AF_S, "s", val == T_INK ? T_INK : T_MID, bg) - 3, y + 3, &AF_M, b, val, bg);
            }
            if (i == MI_LEDS)
                cv_text_r(228, y + 3, &AF_M, gsk_text(GSKC_MENU, settings_leds == LEDS_INV ? "INV" : "DIM"), val, bg);
            if (i == MI_COLOR) {
                uint32_t k;
                const uint16_t *tok = &T_BG;            /* BG SURF TEXT THEME ACCENT; GNOMAD: paper and the four knobs */
                cv_text_r(146, y + 3, &AF_M, UI_PALETTES[settings.palette % NPALETTES].name, val, bg);
                for (k = 0; k < 5u; k++) {             /* (GNOMAD's: each colour in an ink rim) */
                    int gp = settings.palette % NPALETTES == GSK_PAL;
                    uint16_t sw = gp ? (k ? GSK_KNOB[k - 1u] : GSK_PAPER) : tok[k], rim = gp ? GSK_INK : T_RAISE;
                    cv_rrect(152 + (int32_t)k * 15, y + 6, 12, 12, 3, rim, bg);
                    cv_rrect(153 + (int32_t)k * 15, y + 7, 10, 10, 2, sw, rim);
                }
            }
        }
        gsk_keyrow_ctx(8, 232, 207, MENU_KEYS, 3, 7u, T_BG, GSKC_MENU);
        cv_oy = 0;
        cv_blit(0, (uint32_t)top);
    }
    return 1;
}

/* ----------------------------------------------------------------- NAME --- */
/* ui_name.c nm_title: its title ("Save U07", "Rename project A") made from NAME's kind and slot, in place of
 * upstream's (b holds 24); the name being typed is never looked up */
GSK_MIN static void gsk_nm_title(char *b)
{
    char d[8];
    const char *t;
    if (!gsk.on)
        return;
    t = gsk_word(GSKW_NAMETITLE, nm.kind, GSKW_CELL, b);
    if (t == b)
        return;
    if (nm.kind <= NK_USER_RENAME) {
        up_slot_label(d, nm.slot);
    } else {
        d[0] = (char)('A' + (nm.slot & 3u));
        d[1] = 0;
    }
    gsk_fill(t, d, sizeof d, b, 24);
}
/* ui_name.c nm_draw_panel, as upstream's, in words */
GSK_MIN static int gsk_nm_panel(void)
{
    static const char *const FN[5] = {"F#", "G#", "A#", "C#", "D#"};
    uint32_t i;
    if (!gsk.on)
        return 0;
    cv_begin(240, H_GRAPH, T_BG);
    cv_rrect(3, 0, 234, H_GRAPH, 5, T_SURF, T_BG);
    cv_bg = T_SURF;
    if (nm.key) {
        const char *g = nm_group(nm.key - 1u);
        uint32_t n = str_len(g);
        for (i = 0; i < n; i++) {
            int32_t x = 10 + 26 * (int32_t)i;
            char c[2] = {g[i], 0};
            int typed = i == nm.tap, next = i == (nm.tap + 1u) % n;
            uint16_t f = typed ? T_THEME : T_RAISE;
            cv_rrect(x, 4, 22, 22, 4, f, T_SURF);
            cv_text_on(x + 11 - text_w(&AF_M, c) / 2, 5, &AF_M, c, typed ? T_INK : next ? T_ACCENT : T_TEXT, f);
        }
        cv_text_on(14 + 26 * (int32_t)n, 8, &AF_S, gsk_word(GSKW_PHRASE, GSKC_NAME, GSKW_CELL, "TAP AGAIN: NEXT"), T_MID, T_SURF);
    } else {
        cv_text_on(10, 8, &AF_S, gsk_word(GSKW_PHRASE, GSKC_NAME, GSKW_CELL, nm.num ? "ONE TAP, ONE CHARACTER" : "TAP AGAIN: NEXT LETTER"),
                   T_MID, T_SURF);
    }
    cv_text_r(230, 8, &AF_S, nm.num ? "123" : "ABC", T_THEME, T_SURF);
    for (i = 0; i < 16u; i++) {
        int32_t x = 8 + 28 * (int32_t)(i % 8u), y = i < 8u ? 31 : 54;
        int act = nm.key == i + 1u;
        uint16_t f = act ? T_THEME : T_RAISE;
        const char *g = nm_group(i);
        cv_rrect(x, y, 26, 20, 4, f, T_SURF);
        cv_text_on(x + 13 - text_w(&AF_S, g) / 2, y + 3, &AF_S, g, act ? T_INK : T_TEXT, f);
    }
    for (i = 0; i < 5u; i++) {
        int32_t x = 8 + 45 * (int32_t)i, y = 80;
        cv_rrect(x, y, 42, 36, 4, T_KEY, T_SURF);
        cv_text_c(x + 21, y + 2, &AF_S, FN[i], T_INK, T_KEY);
        if (i == NB_LEFT || i == NB_RIGHT)
            cv_icon_on(x + 15, y + 20, 12, i == NB_LEFT ? ICON_X_LEFT : ICON_X_RIGHT, T_INK, T_KEY);
        else
            cv_text_c(x + 21, y + 18, &AF_S, gsk_word(GSKW_PHRASE, GSKC_NAME, GSKW_CELL, i == NB_SPACE ? "SPACE" : i == NB_DEL ?
                      "DEL" : nm.num ? "ABC" : "123"), T_INK, T_KEY);
    }
    cv_blit(0, Y_GRAPH);
    return 1;
}
/* ui_name.c nm_draw_foot, as upstream's, in words */
GSK_MIN static int gsk_nm_foot(void)
{
    khint_t a[2], b[3];
    int ok = !transport_busy();
    if (!gsk.on)
        return 0;
    a[0] = (khint_t){KC_OCTUP, nm.kind == NK_USER_SAVE || nm.kind == NK_PROJ_SAVE ? "SAVE" : "RENAME"};
    a[1] = (khint_t){KC_OCTDN, "CANCEL"};
    b[0] = (khint_t){KC_K1, "MOVE"};
    b[1] = (khint_t){KC_K2, "CHAR"};
    b[2] = (khint_t){KC_KEYS, "TYPE"};
    cv_begin(240, H_FOOT, T_BG);
    gsk_keyrow_ctx(8, 232, 2, a, 2, ok ? 3u : 2u, T_BG, GSKC_NAME);
    gsk_keyrow_ctx(8, 232, 21, b, 3, 7u, T_BG, GSKC_NAME);
    cv_blit(0, Y_FOOT);
    return 1;
}

/* ---------------------------------------------------------- calibration --- */
/* ui_input.c setup_title, as upstream's, in words */
GSK_MIN static int gsk_setup_title(void)
{
    const char *t;
    int32_t x;
    if (!gsk.on)
        return 0;
    lcd_fill(0, 0, 240, 240, T_BG);
    t = gsk_text(GSKC_SETUP, "HARDWARE CALIBRATION");
    x = (240 - (16 + 6 + text_w(&AF_M, t))) / 2;
    cv_begin(240, 24, T_BG);
    cv_icon_on(x, 4, 16, ICON_X_DOCTOR, T_THEME, T_BG);
    cv_text(x + 22, 3, &AF_M, t, T_TEXT);
    cv_blit(0, 5);
    draw_text_box(0, 32, 240, &AF_S, gsk_text(GSKC_SETUP, "TEACH EACH BUTTON AND KNOB"), T_MID, 1);
    lcd_fill(16, 56, 208, 1, T_LINE);
    return 1;
}

/* ---------------------------------------------------------------- panel --- */
/* ui_graph.c panel_note: its three lines in words (fixed phrases: the table's own words, no buffer) */
GSK_MIN static void gsk_note3(const char **a, const char **b, const char **c)
{
    if (!gsk.on)
        return;
    *a = gsk_text(GSKC_NOTE, *a);
    if (*b)
        *b = gsk_text(GSKC_NOTE, *b);
    if (*c)
        *c = gsk_text(GSKC_NOTE, *c);
}
/* ui_graph.c graph_slices: the selected slice's length, "Length 0.25 s" */
GSK_MIN static void gsk_slen(char *t, const char *u)
{
    if (!gsk.on)
        return;
    str_cpy(t, gsk_text(GSKC_NOTE, "LEN"), 20);
    str_cpy(t + str_len(t), " ", 20 - str_len(t));
    str_cpy(t + str_len(t), u, 20 - str_len(t));
    str_cpy(t + str_len(t), " ", 20 - str_len(t));
    str_cpy(t + str_len(t), gsk_word(GSKW_UNIT, 0, GSKW_CELL, "S"), 20 - str_len(t));
}
/* ui_layer.c lcell(..., icon, name, st): a cell's name in words, where upstream wrote it as its own explanation:
 * every cell of GLO's (SOLO, ALL, TAP) and EDIT's warning cell (INIT); EDIT's engine names stay as they are. A
 * REPEAT waiting for its 1/16 in second ink (upstream: the same THEME as a held one, one ink in GNOMAD) */
GSK_MIN static void gsk_lcell(uint32_t st, uint32_t icon, uint16_t *fill, uint16_t *ink, const char **name)
{
    if (!gsk.on)
        return;
    if (st == LS_WAIT) {
        *fill = T_MID;
        *ink = T_INK;
    }
    if (*name && (ui.layer == LAYER_GLO || (ui.layer == LAYER_EDIT && icon == ICON_X_WARN)))
        *name = gsk_text(GSKC_LAYER, *name);
}

/* ui_graph.c graph_song, as upstream's, in words */
GSK_MIN static int gsk_song(void)
{
    uint32_t first = ui.song_row > 2u ? ui.song_row - 2u : 0u, i;
    if (!gsk.on)
        return 0;
    if (!chain_config.count) {
        panel_note("PAT A x4 > B x1", "[K2] ADD PATTERN", "[SAVE] PROJECT A-D");
        return 1;
    }
    for (i = first; i < CHAIN_ROWS && i < first + 7u; i++) {
        char b[16];
        int32_t y = LIST_Y(i - first);
        int sel = i == ui.song_row;
        uint16_t bg = sel ? T_THEME : T_SURF, col = sel ? T_INK : T_TEXT, dim = sel ? T_INK : T_DIM;
        if (i > chain_config.count) break;
        if (sel) cv_rrect(6, y, 228, 16, 4, T_THEME, T_SURF);
        if (chain.running && i == chain.row) cv_icon_on(9, y + 2, 12, ICON_X_RIGHT, sel ? T_INK : T_ACCENT, bg);
        fmt_int(b, (int32_t)i + 1); cv_text_on(24, y + 1, &AF_S, b, sel ? T_INK : T_MID, bg);
        if (i == chain_config.count) { cv_text_on(54, y + 1, &AF_S, gsk_text(GSKC_NOTE, "+ ADD PATTERN"), dim, bg); break; }
        b[0] = (char)('A' + chain_config.row[i].slot); b[1] = 0;
        cv_text_on(54, y + 1, &AF_S, b, col, bg);
        b[0] = 'x'; fmt_int(b + 1, chain_config.row[i].repeat);
        cv_text_on(82, y + 1, &AF_S, b, col, bg);
        if (i + 1u < chain_config.count) cv_text_on(122, y + 1, &AF_S, ">", dim, bg);
        if (!graph_project_used(chain_config.row[i].slot)) cv_text_on(142, y + 1, &AF_S, gsk_text(GSKC_NOTE, "NOT SAVED"), dim, bg);
        else if (chain.running && i == chain.row) {
            fmt_int(b, chain.remaining); str_cpy(b + str_len(b), " ", 2);
            str_cpy(b + str_len(b), gsk_text(GSKC_NOTE, "LEFT"), sizeof b - str_len(b));
            cv_text_on(142, y + 1, &AF_S, b, sel ? T_INK : T_THEME, bg);
        } else if (graph_project_name(chain_config.row[i].slot)[0]) {
            cv_free_text(142, y + 1, &AF_S, graph_project_name(chain_config.row[i].slot), sel ? T_INK : T_MID, bg, 232 - 142);
        }
    }
    return 1;
}
/* ui_graph.c graph_grid, as upstream's, with each knob's thing in its colour and an ink boundary: the cursor (knob 1,
 * STEP) a blue frame in an ink one, the selected lane (knob 2, LANE) its name in ink over a green underline (a
 * thin bar: no boundary), an accented hit (knob 4, ACC) vermilion in ink; hits ink */
GSK_MIN static int gsk_grid(const struct track *tp, uint16_t c)
{
    const track_t *t = (const track_t *)tp;
    uint32_t l, i, len = (uint32_t)t->p[P_SLEN], base = ui.bank * 16u;
    int32_t y0 = 6;
    if (!gsk.on)
        return 0;
    for (l = 0; l < NLANE; l++) {
        int32_t y = y0 + 4 + (int32_t)l * 14;
        int sel = l == ui.lane;
        uint16_t row = sel ? T_RAISE : T_SURF;
        const char *ab = drum_lane_abbr(t, l);
        if (sel) {
            cv_rrect(6, y, 226, 14, 3, T_RAISE, T_SURF);
            cv_rect(10, y + 12, text_w(&AF_S, ab), 2, gsk_paint(GSKP_FILL, 1, GSKS_NORMAL, T_ACCENT));
        }
        cv_text_on(10, y - 1, &AF_S, ab, sel ? T_TEXT : T_MID, row);
        for (i = 0; i < 16u && base + i < len; i++) {
            const step_t *st = &seq_steps(t)[base + i];
            uint32_t b = 1u << l;
            int32_t x = 44 + (int32_t)i * 12;
            if (step_lanes(st) & b) {
                cv_rrect(x, y + 2, 10, 10, 2, c, row);
                if (step_accents(st) & b)
                    cv_rect(x + 2, y + 4, 6, 6, gsk_paint(GSKP_FILL, 3, GSKS_NORMAL, T_ACCENT));
            } else {
                cv_rect(x + 4, y + 6, 2, 2, i % 4u == 0u || sel ? T_MID : T_DIM);
            }
            if (sel && base + i == ui.cursor) {
                cv_frame(x - 2, y, 14, 14, T_TEXT);
                cv_frame(x - 1, y + 1, 12, 12, gsk_paint(GSKP_FILL, 0, GSKS_NORMAL, T_TEXT));
            }
        }
    }
    if (song.playing && t->seq_idx < len && t->seq_idx / 16u == ui.bank)
        cv_rect(44 + (int32_t)(t->seq_idx % 16u) * 12, y0 - 1, 10, 3, T_TEXT);
    return 1;
}
/* ui_graph.c graph_mod, as upstream's, in words: each slot's source, arrow and target placed by their measured widths
 * (upstream's tokens where the words would not fit before the amount at x 170) */
GSK_MIN static int gsk_mod(const struct track *tp, uint16_t c)
{
    const track_t *t = (const track_t *)tp;
    uint32_t k;
    if (!gsk.on)
        return 0;
    for (k = 0; k < NMSLOT; k++) {
        const int16_t *p = &t->p[P_M1SRC + 3u * k];
        int32_t y = 14 + (int32_t)k * 24, on = p[0] && p[1] && p[2], sel = k == mod_ui_slot, ax, dx;
        uint16_t bg = sel ? T_THEME : T_SURF, col = sel ? T_INK : on ? c : T_DIM;
        const char *su = N_MSRC[clamp(p[0], 0, MS_N - 1)], *du = mod_dst_name(t, p[1]);
        const char *sw = gsk_modword(1, p[0], su), *dw = gsk_modword(0, p[1], du), *unit;
        char b[8];
        ax = 34 + text_w(&AF_S, sw) + 8;
        if (ax < 78) ax = 78;
        dx = ax + 24;
        if (dx + text_w(&AF_S, dw) > 166) {           /* (measured: too wide, upstream's tokens) */
            sw = su;
            dw = du;
            ax = 78;
            dx = 102;
        }
        if (sel)
            cv_rrect(6, y, 228, 17, 4, T_THEME, T_SURF);
        b[0] = (char)('1' + k);
        b[1] = 0;
        cv_text_on(14, y + 1, &AF_S, b, sel ? T_INK : T_MID, bg);
        cv_text_on(34, y + 1, &AF_S, sw, col, bg);
        cv_line(ax, y + 8, ax + 14, y + 8, col);
        cv_line(ax + 10, y + 4, ax + 14, y + 8, col);
        cv_line(ax + 10, y + 12, ax + 14, y + 8, col);
        cv_text_on(dx, y + 1, &AF_S, dw, col, bg);
        param_format(&TP[P_M1AMT], p[2], b, &unit);
        cv_text_on(cv_text_on(170, y + 1, &AF_S, b, col, bg) + 2, y + 1, &AF_S, unit, sel ? T_INK : T_DIM, bg);
    }
    return 1;
}

/* ui_layer.c layer_scl, as upstream's: KNOB 2's scales, 4 x 4, in words (measured: upstream's token where the word
 * would not fit its cell) */
GSK_MIN static int gsk_scl(void)
{
    uint32_t i, sc = (uint32_t)TSEL->p[P_SCALE], set = gsk_choices_of(N_SCALE);
    if (!gsk.on)
        return 0;
    for (i = 0; i < 16u && i <= (uint32_t)TP[P_SCALE].max; i++) {
        int32_t x = LC_X(i % 4u), y = 4 + 29 * (int32_t)(i / 4u);
        uint16_t ink, fill = lc_fill(i == sc ? LS_SEL : LS_OFF, &ink);
        const char *w = gsk_word(GSKW_CHOICE, set << 8 | i, GSKW_CELL, TP[P_SCALE].names[i]);
        if (text_w(&AF_S, w) > LC_W - 4)
            w = TP[P_SCALE].names[i];
        cv_rrect(x, y, LC_W, 25, 4, fill, T_SURF);
        cv_text_c(x + LC_W / 2, y + 7, &AF_S, w, ink, fill);
    }
    return 1;
}
#endif /* GSK_CORE_ONLY */
