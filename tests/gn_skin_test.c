/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kyle Palaniuk */
/* Gnomad, a fork of Felucca (see FORK.md). Not part of upstream Felucca.
 * The Advanced skin, phase 1 (firmware/src/gn_skin.c, gn_skin_words.c; option C), on tests/ui_test.c's stubs with
 * the real UI sources, built with GSK_BUILD by tests/gn_skin_tests.sh:
 *   palette     GNOMAD's sixteen roles exactly as section 2 gives them (RGB888 through RGB565), light, not MONO;
 *               any other palette after GNOMAD exactly as before it; the contrast of GNOMAD's pairings (upstream's
 *               minimums where its roles meet them; the ones the specification's values miss are reported)
 *   menu        MENU > COLOR: HI-CON, then GNOMAD (its name GN_NAME), then MONO; OCT+ steps there too
 *   words       every entry still says what upstream says (a label's code is upstream's label for that parameter
 *               in some mode of its engine; a title's code is upstream's title), so a stale entry fails here at a
 *               merge instead of showing a wrong word; every card of every page of every engine (every descriptor
 *               variant), of HOME and of the hold layers has a word; every page title; every message, hint, menu
 *               item, confirmation and note upstream draws in the default build; every label fits Leo's card
 *               (50 px at his 12 px face), every title its footer, every message the header
 *   values      every choice list matches upstream's (its length, by the descriptors that use it), every word fits
 *               Leo's card (50 px) and the scale layer's cell; every value every card of the default build can show
 *               (every value of every card of every page, engine, variant, HOME, hold layer) is in words but for the
 *               deliberate list GSKT_KEEP (FORK.md: data, notation, and tokens with no word that fits)
 *   names       user content is drawn as it is: sounds, projects and patterns named like every phrase and pattern
 *               of the tables (NAME, INIT, SAVE ERROR, SONG PATTERN CHANGES, ...) through the erase, replace and
 *               load dialogs, the load message and the footer; their stored bytes unchanged
 * Exit 1 on a failure. */
#include "../firmware/src/gn_skin.h"
static void hk_label(uint32_t key, const char *up, const char *word);
static void hk_value(uint32_t key, const char *up, const char *word);
static void hk_text(const char *s);
#define GSK_HOOK_LABEL(key, up, word) hk_label(key, up, word)
#define GSK_HOOK_VALUE(key, up, word) hk_value(key, up, word)
#define GFX_HOOK_TEXT(x0, y0, x1, y1, s, flags) hk_text(s)
#define GFX_HOOK_BLIT(x, y, r0) ((void)0)
#define GFX_HOOK_BEGIN() ((void)0)
#define GFX_HOOK_PIXELS(n) ((void)0)
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
#include "../firmware/src/gn_skin.c"

static int bad;
static void ok(const char *what, int good)
{
    printf("skin: %-88s %s\n", what, good ? "ok" : "FAIL");
    bad += !good;
}
static double lin(uint32_t v, uint32_t max)
{
    double c = v / (double)max;
    return c <= 0.04045 ? c / 12.92 : pow((c + 0.055) / 1.055, 2.4);
}
static double lum(uint16_t c) { return 0.2126 * lin(c >> 11, 31) + 0.7152 * lin((c >> 5) & 63u, 63) + 0.0722 * lin(c & 31u, 31); }
static double contrast(uint16_t a, uint16_t b)
{
    double x = lum(a), y = lum(b);
    return x > y ? (x + 0.05) / (y + 0.05) : (y + 0.05) / (x + 0.05);
}
static uint16_t c565(uint32_t rgb) { return (uint16_t)(((rgb >> 19) & 31u) << 11 | ((rgb >> 10) & 63u) << 5 | ((rgb >> 3) & 31u)); }

/* the cards' lookups (gn_skin.c GSK_HOOK_LABEL): a label with no entry is a miss */
static char miss[64][48];
static uint32_t nmiss, nlook;
static int has_label(uint32_t key, const char *code)
{
    uint32_t i;
    for (i = 0; i < NELEM(GSKW_LABELS); i++)
        if (GSKW_LABELS[i].key == key && str_eq(GSKW_LABELS[i].code, code)) return 1;
    return 0;
}
static const char *cur_where = "";
static void hk_label(uint32_t key, const char *up, const char *word)
{
    uint32_t i;
    (void)word;
    nlook++;
    if (!up[0] || has_label(key, up)) return;
    for (i = 0; i < nmiss; i++)
        if (!strncmp(miss[i], up, strlen(up)) && miss[i][strlen(up)] == ' ') return;
    if (nmiss < 64u) snprintf(miss[nmiss++], sizeof miss[0], "%s (%s, key %03x)", up, cur_where, key);
}
static void draw_all(void) { ui.force = 1; ui_draw(); }

static void test_palette(void)
{
    static const uint32_t ROLE[16] = {0xF9F8F4, 0xEEEDE8, 0x17181B, 0x17181B, 0x17181B,   /* bg surf text theme accent */
                                      0x5B5D62, 0x5B5D62, 0xE3E2DC, 0x17181B, 0xE3E2DC,   /* mid dim line sel tint */
                                      0xF9F8F4, 0xF2622A, 0xE3E2DC, 0x17181B, 0xE3E2DC,   /* ink rec raise key lane */
                                      0xEEEDE8};                                        /* grid */
    const uint16_t *tok = &ux.bg;
    uint32_t i, p, same = 1;
    double worst = 99;
    palette_set(GSK_PAL);
    for (i = 0; i < 16u; i++)
        if (tok[i] != c565(ROLE[i])) { printf("  role %u: %04x, want %04x\n", i, tok[i], c565(ROLE[i])); same = 0; }
    ok("GNOMAD: the sixteen roles exactly (section 2), light, not MONO, the skin on",
       same && ux.light == 1 && ux.mono == 0 && gsk.on);
    for (p = 0; p < 8u; p++) {                       /* another palette after GNOMAD: exactly as before it */
        uint16_t before[16];
        uint32_t g;
        palette_set(p);
        memcpy(before, tok, sizeof before);
        g = ux.light | ux.mono << 1;
        palette_set(GSK_PAL);
        palette_set(p);
        if (memcmp(before, tok, sizeof before) || g != (ux.light | ux.mono << 1u) || gsk.on) same = 0;
    }
    ok("each of upstream's eight palettes after GNOMAD: every role as before, the skin off", same);
    palette_set(GSK_PAL);
    {
        struct { uint16_t fg, bg; double min; const char *what; } c[] = {
            {T_TEXT, T_BG, 12.0, "ink on paper"}, {T_TEXT, T_SURF, 9.0, "ink on ground"},
            {T_MID, T_BG, 5.0, "second ink on paper"}, {T_MID, T_SURF, 4.0, "second ink on ground"},
            {T_THEME, T_SURF, 4.5, "values on a card"}, {T_INK, T_SEL, 4.5, "paper on a selection"},
            {T_INK, T_KEY, 4.5, "a keycap's label"}, {T_KEY, T_SURF, 2.5, "a keycap on ground"},
            {T_REC, T_BG, 3.0, "REC (vermilion) on paper"}, {T_INK, T_DIM, 1.8, "an unavailable keycap"},
            {c565(0xF9F8F4), c565(0x2352E6), 4.5, "paper on knob 1 blue"}, {c565(0xF9F8F4), c565(0x0B7D4F), 4.5, "paper on knob 2 green"},
            {c565(0x17181B), c565(0xF4B400), 4.5, "ink on knob 3 yellow"}, {c565(0x17181B), c565(0xF2622A), 4.5, "ink on knob 4 vermilion"},
        };
        uint32_t good = 1;
        for (i = 0; i < NELEM(c); i++) {
            double r = contrast(c[i].fg, c[i].bg);
            if (r < c[i].min) { printf("  %s: %.2f < %.2f\n", c[i].what, r, c[i].min); good = 0; }
            if (r / c[i].min < worst) worst = r / c[i].min;
        }
        ok("GNOMAD's text pairings meet upstream's minimums (and the knob colours their foregrounds)", good);
        printf("  (worst margin x%.2f; for the report, upstream's checks GNOMAD's specified values miss: dividers"
               " (wells on paper) %.2f < 1.25, accent on paper %.2f is ink; the spines on ground: blue %.2f, green %.2f,"
               " yellow %.2f, vermilion %.2f, each inside an ink keyline)\n", worst, contrast(T_LINE, T_BG),
               contrast(T_ACCENT, T_BG), contrast(c565(0x2352E6), T_SURF), contrast(c565(0x0B7D4F), T_SURF),
               contrast(c565(0xF4B400), T_SURF), contrast(c565(0xF2622A), T_SURF));
    }
}

static void test_menu(void)
{
    ui_power_on();
    hold(B_HOME);
    ui.menu_sel = MI_COLOR;
    settings.palette = 7; palette_set(7);
    turn(EN_K1, 1);
    ok("MENU > COLOR: KNOB 1 steps from HI-CON to GNOMAD, the ninth, named GN_NAME; the skin on",
       settings.palette == GSK_PAL && gsk.on && !strcmp(UI_PALETTES[settings.palette].name, GN_NAME) && T_BG == c565(0xF9F8F4));
    draw_all();
    turn(EN_K1, 1);
    ok(".. and on to MONO: the skin off", settings.palette == UI_MONO_INDEX && !gsk.on);
    turn(EN_K1, -1);
    ok(".. and back to GNOMAD", settings.palette == GSK_PAL && gsk.on);
    settings.palette = 7; palette_set(7);
    press(B_OCTUP);
    ok("OCT+ steps to GNOMAD too", settings.palette == GSK_PAL && gsk.on);
    press(B_OCTDN);
    ok("OCT- closes the menu with GNOMAD kept, saved as 13", !ui.menu && settings.palette == GSK_PAL && palette_to_stored(settings.palette) == 13u);
}

/* every label entry's code is what upstream labels that parameter in some mode of its engine; every title entry's
 * code upstream's title */
static void test_tables(void)
{
    uint32_t i, stale = 0;
    for (i = 0; i < NELEM(GSKW_LABELS); i++) {
        const gsk_label_t *l = &GSKW_LABELS[i];
        uint32_t kind = l->key >> 8, a = l->key & 255u, found = 0;
        if (kind == 0u) found = a < P_COUNT && str_eq(TP[a].label, l->code);
        else if (kind == 1u) found = a < G_COUNT && (str_eq(GP[a].label, l->code) || (a == G_MIDI && str_eq(l->code, "USB")));
        else if (kind == 2u) {
            uint32_t e = a >> 3, k = a & 7u;
            int32_t m0, m1;
            track_t t;
            if (e >= NENGINES) { found = 0; }
            else for (m0 = ENGINES[e]->edit[0].min; m0 <= ENGINES[e]->edit[0].max && !found; m0++)
                for (m1 = ENGINES[e]->edit[1].min; m1 <= ENGINES[e]->edit[1].max && !found; m1++) {
                    memset(&t, 0, sizeof t);
                    t.eng_req = (uint8_t)e;
                    t.p[P_E0] = (int16_t)m0;
                    t.p[P_E1] = (int16_t)m1;
                    found = str_eq(track_desc(&t, P_E0 + k)->label, l->code);
                }
        } else found = 1;                            /* (the cards pages assemble: seen drawn, test_cover) */
        if (!found) { printf("  stale label entry: key %03x code %s (%s)\n", l->key, l->code, l->word); stale++; }
    }
    for (i = 0; i < NELEM(GSKW_TITLES); i++) {
        const gsk_title_t *t = &GSKW_TITLES[i];
        uint32_t found = 0, k;
        if (t->a >= GSKT_EDIT1)
            found = t->b < NENGINES && (t->a == GSKT_HOME ? str_eq(ENGINES[t->b]->name, t->code) :
                                        str_eq(ENGINES[t->b]->page_title[t->a - GSKT_EDIT1], t->code));
        else
            for (k = 0; k < NPAGES; k++)
                if (PAGES[k].fam == t->a && PAGES[k].graph == t->b && PAGES[k].id[0] == t->c &&
                    (str_eq(PAGES[k].title, t->code) || (t->b == GR_ROLL && str_eq(t->code, "GRID"))))
                    found = 1;
        if (!found) { printf("  stale title entry: %s (%s)\n", t->code, t->word); stale++; }
    }
    ok("every word entry still matches upstream's labels and titles (none stale)", !stale);
    for (i = 0, stale = 0; i < NELEM(GSKW_LABELS); i++)
        if (text_w(&AF_S, GSKW_LABELS[i].word) > COL_W - 2 - 5) { printf("  %s: %d px\n", GSKW_LABELS[i].word, text_w(&AF_S, GSKW_LABELS[i].word)); stale++; }
    ok("every card label fits Leo's card: at most 50 px at his 12 px face", !stale);
}

/* every card the default build draws, on every page of every engine (and every descriptor variant), HOME, the
 * hold layers: a word for each; every page title */
static void test_cover(void)
{
    uint32_t e, i, k, titles = 0, tmiss = 0, l;
    char ti[32], where[48];
    static const struct { uint32_t e, id; int16_t v; } VAR[] = {{3, P_E1, RW_WRAM}, {3, P_E0, CHIP_STEP},
        {ENGI_PHYS, P_E0, PM_MEMB}, {ENGI_PHYS, P_E0, PM_SYMP}, {11, P_E0, NZ_LFSR}, {11, P_E0, NZ_META}};
    for (e = 0; e < NENGINES; e++) {
        if (!eng_ok(e)) continue;
        for (k = 0; k <= NELEM(VAR); k++) {
            if (k && VAR[k - 1u].e != e) continue;
            for (i = 0; i < NPAGES + 1u; i++) {
                ui_power_on(); settings.palette = GSK_PAL; palette_set(GSK_PAL);
                set_engine_of(TSEL, e);
                if (k) TSEL->p[VAR[k - 1u].id] = VAR[k - 1u].v;
                if (i == NPAGES) go_home();
                else { ui.home = 0; ui.page = (uint8_t)i; page_entered(); if (!page_visible(i)) continue; }
                snprintf(where, sizeof where, "%s/%s", ENGINES[e]->name, i == NPAGES ? "HOME" : PAGES[i].title);
                cur_where = where;
                draw_all();
                gsk_title(ti, sizeof ti);
                titles++;
                if (i < NPAGES && PAGES[i].graph != GR_ROLL &&
                    (!strncmp(ti, PAGES[i].title, strlen(PAGES[i].title)) || (PAGES[i].scope == SC_ENGINE &&
                     !strncmp(ti, ENGINES[e]->page_title[PAGES[i].id[0] != P_E0], 4)))) { printf("  title not in words: %s (%s)\n", ti, where); tmiss++; }
                if (i == NPAGES && !strcmp(ti, "HOME")) { printf("  title not in words: HOME (%s)\n", where); tmiss++; }
            }
        }
        for (l = LAYER_FX; l < LAYER_N; l++) {
            ui_power_on(); settings.palette = GSK_PAL; palette_set(GSK_PAL); set_engine_of(TSEL, e); go_home();
            ui.layer = (uint8_t)l;
            snprintf(where, sizeof where, "%s/layer %u", ENGINES[e]->name, l);
            cur_where = where;
            draw_all();
            if (l == LAYER_FX) { perf_held = perf_act = PF_BIT(PF_OUP); draw_all(); perf_held = perf_act = 0; }
        }
    }
    for (i = 0; i < nmiss; i++) printf("  no word: %s\n", miss[i]);
    ok("every card of every page, engine, descriptor variant, HOME and hold layer has a word", !nmiss && nlook > 1000u);
    ok("every page title is in words (the piano roll keeps upstream's key, C MIN)", !tmiss && titles > 400u);
    {
        int32_t w = 0;
        for (i = 0; i < NELEM(GSKW_TITLES); i++)
            if (text_w(&AF_S, GSKW_TITLES[i].word) > w) w = text_w(&AF_S, GSKW_TITLES[i].word);
        printf("  (the widest title: %d px; the footer gives the sound name at least 64 px beside it)\n", w);
    }
}

/* every fixed phrase upstream draws in the default build, at its own site */
static void test_phrases(void)
{
    static const struct { uint32_t site; const char *up; } UP[] = {
        {GSKC_MSG, "NOTHING TO UNDO"}, {GSKC_MSG, "NOTHING TO DELETE"}, {GSKC_MSG, "NOTHING TO CLEAR"},
        {GSKC_MSG, "MOTION FULL"}, {GSKC_MSG, "NO FAVORITES"}, {GSKC_MSG, "ADD A SONG ROW"}, {GSKC_MSG, "STOP FIRST"},
        {GSKC_MSG, "STOP TO SAVE"}, {GSKC_MSG, "STOP TO EDIT"}, {GSKC_MSG, "STOP TO RECORD"}, {GSKC_MSG, "STOP TO UNDO"},
        {GSKC_MSG, "UPDATE CANCELLED"}, {GSKC_MSG, "SETUP CANCELLED"}, {GSKC_MSG, "EMPTY SLOT"}, {GSKC_MSG, "LOADED"},
        {GSKC_MSG, "SAVED"}, {GSKC_MSG, "RENAMED"}, {GSKC_MSG, "SAVED (RAM)"}, {GSKC_MSG, "ERASED (RAM)"},
        {GSKC_MSG, "RENAMED (RAM)"}, {GSKC_MSG, "SAVE ERROR"}, {GSKC_MSG, "SAVE FORMAT ERROR"}, {GSKC_MSG, "ERASE ERROR"},
        {GSKC_MSG, "SLICES SAVED"}, {GSKC_MSG, "TOO LONG TO SAVE"}, {GSKC_MSG, "KEYS RECORD LIVE"},
        {GSKC_MSG, "LANE KEYS RECORD"}, {GSKC_MSG, "MOTION CLEARED"}, {GSKC_MSG, "PATTERN CLEARED"},
        {GSKC_MSG, "STEP CLEARED"}, {GSKC_MSG, "ROW DELETED"}, {GSKC_MSG, "SONG CLEARED"}, {GSKC_MSG, "SOUND INIT"},
        {GSKC_MSG, "[SEQ] TO RECORD"}, {GSKC_MSG, "MIX PUT BACK"}, {GSKC_MSG, "SCALE PUT BACK"},
        {GSKC_MSG, "TAP: CLK IS EXT"}, {GSKC_MSG, "RESTART: CLK IS EXT"}, {GSKC_MSG, "NAME FULL"}, {GSKC_MSG, "DIV MAN"},
        {GSKC_MSG, "FIRST SLICE"}, {GSKC_MSG, "PICK A SLICE"}, {GSKC_MSG, "SAMPLE TOO SHORT"},
        {GSKC_MSG, "32 SLICES AT MOST"}, {GSKC_MSG, "SLICE TOO SHORT"}, {GSKC_MSG, "SRC USR1-3 TO EDIT"},
        {GSKC_MSG, "USR2 EMPTY"}, {GSKC_MSG, "MIDI IN -> T3"}, {GSKC_MSG, "UNDO/REDO T2"}, {GSKC_MSG, "TRACK 4 CLEARED"},
        {GSKC_MSG, "PATTERN B EMPTY"}, {GSKC_MSG, "HOLD [GLO] QUICK"}, {GSKC_MSG, "LOADED 07 A VERY LONG"},
        {GSKC_MSG, "SAVED U07"}, {GSKC_MSG, "ERASED U07"}, {GSKC_MSG, "RENAMED U07"}, {GSKC_MSG, "ENGINE ANALOG"},
        {GSKC_MSG, "[FX] HOLD"}, {GSKC_MSG, "[GLO] SET"}, {GSKC_MSG, "[SCL] SET"}, {GSKC_MSG, "[EDIT] SET"},
        {GSKC_CONFIRM, "SONG PATTERN CHANGES"}, {GSKC_CONFIRM, "NO"}, {GSKC_CONFIRM, "YES"},
        {GSKC_NOTE, "NO FAVORITES"}, {GSKC_NOTE, "LIST ALL TO ADD SOUNDS"}, {GSKC_NOTE, "TURN TO PICK"},
        {GSKC_NOTE, "[OCT+] CONFIRM"}, {GSKC_NOTE, "NO SAMPLE"}, {GSKC_NOTE, "SRC: BREAK OR USR1-3"},
        {GSKC_NOTE, "CHORD KEYS OFF"}, {GSKC_NOTE, "[K1] DIA3: IN-KEY CHORDS"}, {GSKC_NOTE, "NO CHORDS ON KITS"},
        {GSKC_NOTE, "[K2] ADD PATTERN"}, {GSKC_NOTE, "[SAVE] PROJECT A-D"}, {GSKC_NOTE, "LAST"}, {GSKC_NOTE, "ON ROOT"},
        {GSKC_NOTE, "ROOT ONLY"}, {GSKC_NOTE, "+ ADD PATTERN"}, {GSKC_NOTE, "NOT SAVED"}, {GSKC_NOTE, "LEFT"},
        {GSKC_NOTE, "LEN"}, {GSKC_SETUP, "HARDWARE CALIBRATION"}, {GSKC_SETUP, "TEACH EACH BUTTON AND KNOB"},
        {GSKC_SETUP, "PRESS"}, {GSKC_SETUP, "TURN RIGHT"}, {GSKC_NAME, "TAP AGAIN: NEXT"},
        {GSKC_NAME, "TAP AGAIN: NEXT LETTER"}, {GSKC_NAME, "ONE TAP, ONE CHARACTER"}, {GSKC_NAME, "SPACE"},
        {GSKC_NAME, "DEL"}, {GSKC_NAME, "SAVE"}, {GSKC_NAME, "RENAME"}, {GSKC_NAME, "CANCEL"}, {GSKC_NAME, "MOVE"},
        {GSKC_NAME, "CHAR"}, {GSKC_NAME, "TYPE"}, {GSKC_LAYER, "SOLO"}, {GSKC_LAYER, "ALL"}, {GSKC_LAYER, "TAP"},
        {GSKC_LAYER, "INIT"}};
    uint32_t i, miss_n = 0, wide = 0;
    palette_set(GSK_PAL);
    for (i = 0; i < NELEM(UP); i++) {
        const char *w = gsk_text(UP[i].site, UP[i].up);
        if (w == UP[i].up || !strcmp(w, UP[i].up)) { printf("  no words for \"%s\" (site %u)\n", UP[i].up, UP[i].site); miss_n++; }
        else if (UP[i].site == GSKC_MSG && text_w(&AF_S, w) > 236 - 106 && strncmp(UP[i].up, "LOADED 07", 9)) {
            printf("  \"%s\" is %d px, the header has 130\n", w, text_w(&AF_S, w)); wide++;
        }
    }
    ok("every fixed phrase upstream draws has words at its own site", !miss_n);
    ok("every message fits the header's 130 px", !wide);
    ok("a wildcard carries the data: LOADED 07 NAME, HOLD [GLO] QUICK, MIDI IN -> T3",
       !strcmp(gsk_text(GSKC_MSG, "LOADED 07 NAME"), "Loaded 07 NAME") &&
       !strcmp(gsk_text(GSKC_MSG, "HOLD [GLO] QUICK"), "Hold [GLO] for shortcuts") &&
       !strcmp(gsk_text(GSKC_MSG, "MIDI IN -> T3"), "MIDI to part 3"));
    ok("a site's phrase is not looked up at another site (NAME's MOVE is the menu's Move, not Cursor)",
       !strcmp(gsk_text(GSKC_MENU, "MOVE"), "Move") && !strcmp(gsk_text(GSKC_NAME, "MOVE"), "Cursor") &&
       !strcmp(gsk_text(GSKC_NOTE, "SAVE ERROR"), "SAVE ERROR") && !strcmp(gsk_text(GSKC_FOOT, "NO"), "NO"));
    ok("unknown words, labels and pages fall back to upstream's text",
       !strcmp(gsk_text(GSKC_MSG, "SOMETHING NEW"), "SOMETHING NEW") &&
       !strcmp(gsk_word(GSKW_LABEL, GSKK_TRK(P_ATK), GSKW_CELL, "NEW"), "NEW") &&
       !strcmp(gsk_word(GSKW_TITLE, GSKW_TKEY(FAM_ENV, GR_ADSR, P_ATK), GSKW_CELL, "ENVELOPE"), "ENVELOPE"));
    palette_set(0);
    ok("with another palette every phrase is upstream's", !strcmp(gsk_text(GSKC_MSG, "STOP TO SAVE"), "STOP TO SAVE") &&
       !strcmp(gsk_text(GSKC_MSG, "[FX] HOLD"), "[FX] HOLD"));
}

/* the choice lists: each the length of upstream's (by every descriptor that uses it), each word within Leo's card */
static void test_choices(void)
{
    uint32_t i, e, k, mism = 0, wide = 0, used, nw;
    for (i = 0; i < NELEM(GSKW_CHOICES); i++) {
        const gsk_choice_t *c = &GSKW_CHOICES[i];
        const param_desc_t *ds[64];
        uint32_t nd = 0, j;
        for (k = 0; k < P_COUNT; k++) if (TP[k].names == c->up && nd < 64u) ds[nd++] = &TP[k];
        for (k = 0; k < G_COUNT; k++) if (GP[k].names == c->up && nd < 64u) ds[nd++] = &GP[k];
        for (e = 0; e < NENGINES; e++) {
            int32_t m0, m1;
            track_t t;
            for (k = 0; k < 8u; k++) {
                if (ENGINES[e]->edit[k].names == c->up && nd < 64u) ds[nd++] = &ENGINES[e]->edit[k];
                for (m0 = 0; m0 < 5 && ENGINES[e]->desc; m0++)      /* (the variants' descriptors) */
                    for (m1 = 0; m1 < 5; m1++) {
                        const param_desc_t *d;
                        memset(&t, 0, sizeof t); t.eng_req = (uint8_t)e; t.p[P_E0] = (int16_t)m0; t.p[P_E1] = (int16_t)m1;
                        d = ENGINES[e]->desc(&t, k);
                        if (d && d->names == c->up && nd < 64u) ds[nd++] = d;
                    }
            }
        }
        used = nd > 0 || c->up == N_MSRC || c->up == N_MDST;
        for (j = 0; j < nd; j++) {
            uint32_t n = 0;
            if (ds[j]->fmt == F_ENUM) n = (uint32_t)(ds[j]->max - ds[j]->min + 1);
            else while (ds[j]->names[n]) n++;
            if (n != c->n && !(c->up == N_FM6_ALG && n > c->n)) { printf("  choice list %u: %u words, upstream %u\n", i, c->n, n); mism++; }
        }
        if (!used) { printf("  choice list %u is used by no descriptor\n", i); mism++; }
        for (k = 0, nw = 0; k < c->n; k++) {
            const char *w = gsk_nth(c->w, c->n, k);
            if (w[0] && text_w(&AF_S, w) > COL_W - 7) { printf("  \"%s\": %d px\n", w, text_w(&AF_S, w)); wide++; }
            nw += w[0] != 0;
        }
    }
    ok("every choice list has upstream's length and is used; MOD's lists match its sources and targets",
       !mism && NELEM(N_MSRC) == GSKW_CHOICES[gsk_choices_of(N_MSRC)].n && NELEM(N_MDST) == GSKW_CHOICES[gsk_choices_of(N_MDST)].n);
    ok("every choice word fits Leo's card (50 px) and the scale layer's cell", !wide);
}

/* every value the cards can show (GSK_HOOK_VALUE), kept as upstream's token only if listed here (FORK.md) */
static const char *const GSKT_KEEP[] = {
    /* notation and data: note names, engine names, the user slots, sample sets, the FM6 algorithm's own name */
    "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B", "ANALOG", "PHASE", "LOFI", "SAMPLE", "VOICE", "TRIO",
    "WHEEL", "GRAIN", "PHYS", "DRUM", "NOISE", "FM6", "SLICE", "U01", "PIANO", "FLUTE", "SAX", "PERC", "USR1", "USR2", "USR3",
    "[OCT+]", "E6", "E8",
    /* noise registers (option C: their numeric tokens stay) */
    "L23", "L20", "L17", "L15", "L13", "L11", "L9", "L7", "127B", "63B",
    /* no plain word fits Leo's card (phase 2's rows say them in full) */
    "SPLS", "SQ50", "SQ25", "SQ12", "P&S", "P+N", "SYNCP", "R+S", "R+SP", "MEMB", "H+CYM"};
static char vkept[96][16];
static uint32_t nvkept, nvalues;
static void hk_value(uint32_t key, const char *up, const char *word)
{
    uint32_t i;
    const char *p;
    int alpha = 0;
    (void)key;
    nvalues++;
    if (word != up) return;
    for (p = up; *p; p++) alpha |= (*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z');
    if (!alpha) return;
    for (i = 0; i < nvkept; i++) if (!strcmp(vkept[i], up)) return;
    if (nvkept < 96u) snprintf(vkept[nvkept++], sizeof vkept[0], "%s", up);
}
static void test_values(void)
{
    uint32_t e, i, c, k, l, unlisted = 0;
    static const struct { uint32_t e, id; int16_t v; } VAR[] = {{3, P_E1, RW_WRAM}, {3, P_E0, CHIP_STEP},
        {ENGI_PHYS, P_E0, PM_MEMB}, {ENGI_PHYS, P_E0, PM_SYMP}, {11, P_E0, NZ_LFSR}, {11, P_E0, NZ_META}};
    for (e = 0; e < NENGINES; e++) {
        if (!eng_ok(e)) continue;
        for (k = 0; k <= NELEM(VAR); k++) {
            if (k && VAR[k - 1u].e != e) continue;
            for (i = 0; i <= NPAGES; i++) {
                ui_power_on(); settings.palette = GSK_PAL; palette_set(GSK_PAL); set_engine_of(TSEL, e);
                if (k) TSEL->p[VAR[k - 1u].id] = VAR[k - 1u].v;
                if (i == NPAGES) go_home();
                else { ui.home = 0; ui.page = (uint8_t)i; page_entered(); if (!page_visible(i)) continue; }
                for (c = 0; c < 4u; c++) {
                    int16_t *vp;
                    const param_desc_t *d = i == NPAGES ? home_param(c, &vp) : PAGES[i].scope == SC_GLOBAL ||
                        PAGES[i].scope == SC_TRACK || PAGES[i].scope == SC_ENGINE ? page_desc(cur_page(), c, &vp) : 0;
                    int32_t v, v0;
                    if (!d || !vp) { ui.force = 1; draw_columns(); continue; }
                    v0 = *vp;
                    for (v = d->min; v <= d->max; v++) {
                        *vp = (int16_t)v; ui.force = 1; draw_columns();
                        if (v - d->min > 300) v = d->max - 1;
                    }
                    *vp = (int16_t)v0;
                }
            }
        }
        for (l = LAYER_FX; l < LAYER_N; l++) {
            ui_power_on(); palette_set(GSK_PAL); set_engine_of(TSEL, e); go_home(); ui.layer = (uint8_t)l; draw_all();
        }
    }
    for (i = 0; i < nvkept; i++) {
        for (k = 0; k < NELEM(GSKT_KEEP) && strcmp(GSKT_KEEP[k], vkept[i]); k++) ;
        if (k == NELEM(GSKT_KEEP)) { printf("  a card shows upstream's \"%s\", not on the deliberate list\n", vkept[i]); unlisted++; }
    }
    ok("every value a card shows is in words, but for the deliberate list (FORK.md)", !unlisted && nvalues > 100000u);
    printf("  (%u card values drawn; %u distinct tokens kept, all listed)\n", nvalues, nvkept);
}

/* user content stays as it is: names equal to every phrase, wildcard and word of the tables */
static char drawn[128][48];
static uint32_t ndrawn;
static void hk_text(const char *s) { if (ndrawn < 128u) snprintf(drawn[ndrawn++], sizeof drawn[0], "%s", s); }
static int drawn_has(const char *s)
{
    uint32_t i;
    for (i = 0; i < ndrawn; i++) if (!strcmp(drawn[i], s)) return 1;
    return 0;
}
static void test_names(void)
{
    static const char *const EXTRA[] = {"NAME", "INIT", "SAVE ERROR", "SONG PATTERN", "NO", "YES", "(RAM)", "SAVE U07",
                                        "LOADED", "ERASE U07?", "A EMPTY", "T1", "MOVE", "PAGE 1", "CONST", "D3", "F1",
                                        "B12", "LEFT", "LEN", "OFF", "ON", "HOLD", "DIM", "STEP", "S"};
    const char *names[400];
    uint32_t n = 0, i, k, badd = 0, badst = 0, badmsg = 0, badfoot = 0;
    for (k = 0; k < GSKC_N; k++)                      /* every phrase's upstream side and words, wildcards as they are */
        for (i = 0; i < GSKW_PH[k].n && n < 380u; i++) {
            names[n++] = GSKW_PH[k].t[i].up;
            names[n++] = GSKW_PH[k].t[i].word;
        }
    for (i = 0; i < NELEM(EXTRA); i++) names[n++] = EXTRA[i];
    for (i = 0; i < n; i++) {
        char nm[13], got[16];
        uint32_t j, kind;
        uint8_t bank[sizeof up_bank];
        for (j = 0; j < 12u && names[i][j]; j++)      /* (the device's names: upper case, at most 12) */
            nm[j] = (char)(names[i][j] >= 'a' && names[i][j] <= 'z' ? names[i][j] - 32 : names[i][j]);
        nm[j] = 0;
        ui_power_on(); song.playing = 0; settings.palette = GSK_PAL; palette_set(GSK_PAL);
        load_pat16(TSEL, PATTERNS[0].note, PATTERNS[0].flags);   /* (so U07 holds a pattern too) */
        up_store(6, nm);
        up_name(6, got);
        project_save_as(0, nm);
        memcpy(bank, up_bank, sizeof bank);
        for (kind = CF_CLEAR_SEQ; kind <= CF_ERASE_USER; kind++) {
            char want[16];
            if (kind != CF_OVR_USER && kind != CF_ERASE_USER && kind != CF_OVR_PROJ && kind != CF_LOAD_PAT) continue;
            if (kind == CF_LOAD_PAT) {                /* a user sound's pattern over the steps: its name */
                if (!up_has_pat(6)) { printf("  (no pattern in U07)\n"); badd++; continue; }
                ui.ppick = (uint8_t)(NPATTERNS + up_pat_rank(6));
            }
            ui.confirm = (uint8_t)kind; ui.confirm_trk = kind == CF_OVR_PROJ || kind == CF_LOAD_PAT ? 0u : 6u; ui.msg_t = 0;
            if (kind == CF_OVR_PROJ) project_name(0, want); else str_cpy(want, got, sizeof want);
            ndrawn = 0;
            ui.force = 1; ui_draw();
            if (want[0] && !drawn_has(want)) { printf("  dialog %u: the target \"%s\" not drawn as it is\n", kind, want); badd++; }
        }
        ui.confirm = 0;
        if (memcmp(bank, up_bank, sizeof bank)) badst++;
        up_name(6, nm); if (strcmp(nm, got)) badst++;
        {   /* the load message, "LOADED U07 <name>" (upreset.c, ui.c pat_load_ui): what the header is given to draw */
            char m[24], want[24];
            str_cpy(m, "LOADED U07 ", sizeof m); str_cpy(m + 11, got, sizeof m - 11);
            str_cpy(want, "Loaded U07 ", sizeof want); str_cpy(want + 11, got, sizeof want - 11);
            if (strcmp(gsk_text(GSKC_MSG, m), want)) { printf("  \"%s\" became \"%s\"\n", m, gsk_text(GSKC_MSG, m)); badmsg++; }
        }
        {   /* the footer: the sound's name as it is */
            ui.msg_t = 0; up_load(6); go_home(); ndrawn = 0; ui.force = 1; ui_draw();
            for (j = 0, k = 0; j < ndrawn; j++) k |= strncmp(drawn[j], got, str_len(got) > 4 ? 4 : str_len(got)) == 0;
            if (got[0] && !k) { printf("  the footer does not show \"%s\"\n", got); badfoot++; }
        }
    }
    ok("dialogs (erase, replace a sound, a project, a pattern) draw the target's name as it is, every phrase as a name", !badd);
    ok(".. and the names stored are unchanged by drawing", !badst);
    ok("the load message carries the name as it is", !badmsg);
    ok("the footer shows the sound's name as it is", !badfoot);
    printf("  (%u names: every phrase and word of every site's table, and %u more)\n", n, (unsigned)NELEM(EXTRA));
}

int main(void)
{
    test_palette();
    test_menu();
    test_tables();
    test_cover();
    test_phrases();
    test_choices();
    test_values();
    test_names();
    printf(bad ? "SKIN TESTS FAILED\n" : "skin: all passed\n");
    return bad ? 1 : 0;
}
