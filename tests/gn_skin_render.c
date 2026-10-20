/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kyle Palaniuk */
/* Gnomad, a fork of Felucca (see FORK.md). Not part of upstream Felucca.
 * The Advanced skin's screens (firmware/src/gn_skin.c, option C phase 1), drawn by upstream's own host renderer:
 * tests/ui_render.c is included whole (its scenes, its layout lint, upstream's drawing code on tests/ui_test.c's
 * stubs); its main is never run, ours runs first. Built by tests/gn_skin_tests.sh:
 *
 *   gn_skin_render inert OUT.txt     every scene of tests/ui_render.c, every page of every engine (and each one's
 *                                    HOME), every hold layer, and every value of every card, in each of upstream's
 *                                    eight palettes: one hash per screen (FNV-1a 64 of the 240 x 240 RGB565 pixels,
 *                                    or of the card band for a value). Built with GSK_BUILD against this tree and
 *                                    without it against the base's sources (gn/base-1.0.1), the two files must be
 *                                    equal: with any of the eight palettes, the skin changes no pixel. The GSK_BUILD
 *                                    build also draws each scene in GNOMAD first and then in the palette, and that
 *                                    must give the same hash as drawing it fresh (nothing of the skin stays behind).
 *   gn_skin_render sheet OUTDIR      (GSK_BUILD only) GNOMAD's screens: every entry of option C's catalogue (the pages
 *                                    of every engine, HOME and both EDIT pages each, the descriptor variants), the
 *                                    four hold layers, the menu, NAME, every confirmation, calibration and messages:
 *                                    OUTDIR/NNN_name.ppm and OUTDIR/index.tsv (file, page name); every screen and
 *                                    every value of every card linted by upstream's lint (tests/ui_render.c lint:
 *                                    nothing off the screen, cut, overlapping, spilling out of its card, or
 *                                    ellipsised but free text). tests/gn_skin_sheet.py makes the PNGs and the sheet.
 *   gn_skin_render ops OUTDIR        (GSK_BUILD, FELUCCA_FM4=1) the five operator pages the default build hides, appended
 * Exit 1 on a finding. */
#include <stdlib.h>
#ifndef GSK_BUILD
#define GSK_BUILD 0                                        /* (the base's sources: upstream's eight palettes) */
#endif
#if GSK_BUILD
#include "../firmware/src/gn_skin.h"
#endif
#include "ui_render.c"
#if GSK_BUILD
#include "../firmware/src/gn_skin.c"
#endif
#undef __attribute__                                     /* (hostsim.c defines it away) */

static uint64_t gr_hash(uint32_t y0, uint32_t y1)        /* FNV-1a 64 over screen rows y0 .. y1 - 1 */
{
    uint64_t h = 14695981039346656037ull;
    uint32_t i;
    for (i = y0 * 240u; i < y1 * 240u; i++) {
        h = (h ^ (host_screen[i] & 255u)) * 1099511628211ull;
        h = (h ^ (host_screen[i] >> 8)) * 1099511628211ull;
    }
    return h;
}

/* the descriptor variants (option C, section 3): the selectors that change what an engine's cards are of, each
 * combination: LOFI's wave (WRAM: DUTY is WAV#) and chip (STEP: CRSH is DCY), PHYS's model, NOISE's mode */
static const struct { uint8_t e, n; uint8_t id[2]; int16_t v[4][2]; const char *what[4]; } GR_VARIANTS[] = {
    {3, 4, {P_E1, P_E0}, {{0, 0}, {4, 0}, {0, 4}, {4, 4}}, {"plain", "wave bank", "stepped", "wave bank, stepped"}},
    {9, 4, {P_E0, P_E0}, {{0, 0}, {1, 1}, {2, 2}, {3, 3}}, {"modal", "string", "membrane", "sympathetic"}},
    {11, 4, {P_E0, P_E0}, {{0, 0}, {1, 1}, {2, 2}, {3, 3}}, {"analog", "dust", "register", "metal"}}};
static uint32_t gr_variants(uint32_t e)                  /* the combinations engine e has (1: none) */
{
    uint32_t i;
    for (i = 0; i < NELEM(GR_VARIANTS); i++) if (GR_VARIANTS[i].e == e) return GR_VARIANTS[i].n;
    return 1;
}
static void gr_variant(uint32_t e, uint32_t k)           /* combination k of engine e on the selected track */
{
    uint32_t i;
    for (i = 0; i < NELEM(GR_VARIANTS); i++)
        if (GR_VARIANTS[i].e == e) {
            TSEL->p[GR_VARIANTS[i].id[0]] = GR_VARIANTS[i].v[k][0];
            TSEL->p[GR_VARIANTS[i].id[1]] = GR_VARIANTS[i].v[k][1];
        }
}

/* ------------------------------------------------------------------------------------------------ inert --- */
static FILE *gr_out;
static uint32_t gr_n, gr_bad;
static void gr_put(const char *what, uint32_t p, uint64_t h) { fprintf(gr_out, "%u %s %016llx\n", p, what, (unsigned long long)h); gr_n++; }
static int gr_scene_ok(uint32_t s)
{
    return FELUCCA_FM4 || !(s == S_OP_ENV || (s >= S_ALG1 && s <= S_OP_LEVEL));   /* (as ui_render.c main) */
}
/* successive frames as the device draws them: input, then ui_draw, the framebuffer kept (never cleared), unforced
 * but for what upstream forces itself (a page change); a hash after every frame. One script: idle frames (the
 * scope, the meters), each knob turned (its card hot, its digits rolling, the timer running out), SELECT (the BPM's
 * roll), a message expiring, the page buttons, FX held (its layer), let go */
static const struct { uint8_t kind, arg, frames; } GR_SCRIPT[] = {
    {0, 0, 8}, {1, EN_K1, 12}, {1, EN_K2, 12}, {1, EN_K3, 12}, {1, EN_K4, 12}, {2, EN_SELECT, 14}, {3, 0, 46},
    {4, B_ENV, 6}, {4, B_ENV, 6}, {4, B_HOME, 6}, {5, B_FX, 4}, {0, 0, 10}};
#define GR_HALF 6u                                       /* (GR_SCRIPT[0 .. GR_HALF-1]: the first half) */
static void gr_step(uint32_t k)
{
    switch (GR_SCRIPT[k].kind) {
    case 1: turn(GR_SCRIPT[k].arg, 1); break;
    case 2: turn(GR_SCRIPT[k].arg, 1); turn(GR_SCRIPT[k].arg, 1); break;
    case 3: ui_message("STOP TO SAVE"); break;
    case 4: press(GR_SCRIPT[k].arg); break;
    case 5: hold(GR_SCRIPT[k].arg); break;
    default: break;
    }
}
static void gr_frames(uint32_t p, uint32_t s)
{
    uint32_t k, f;
    char name[64];
    setup((int)s); pal(p); draw((int)s);
    for (k = 0; k < NELEM(GR_SCRIPT); k++) {
        gr_step(k);
        for (f = 0; f < GR_SCRIPT[k].frames; f++) {
            frame();
            snprintf(name, sizeof name, "frames/%s/%u.%u", S_NAME[s], k, f);
            gr_put(name, p, gr_hash(0, 240));
        }
    }
}
static const int GR_FRAME_SCENES[] = {S_HOME, S_ENV, S_GLOBAL, S_MIXER, S_STEP, S_DRUM, S_PRESETS, S_SONG, S_CHORD,
                                      S_EDIT_WHEEL, S_MOD, S_SYSTEM};
#if GSK_BUILD
/* the same script, its first half drawn in palette q (GSK_PAL: GNOMAD), then palette p chosen as MENU > COLOR and the
 * editor choose it (palette_set, then upstream's forced redraw, no clearing), the second half's frames hashed into h;
 * in a child of the same parent, so both runs start alike */
static void gr_frames_after(uint32_t s, uint32_t q, uint32_t p, uint64_t *h, uint32_t nh)
{
    int fd[2];
    pid_t pid;
    if (pipe(fd)) { perror("pipe"); exit(1); }
    pid = fork();
    if (pid < 0) { perror("fork"); exit(1); }
    if (!pid) {
        uint32_t k, f, n = 0;
        close(fd[0]);
        setup((int)s); pal(q); draw((int)s);
        for (k = 0; k < NELEM(GR_SCRIPT); k++) {
            if (k == GR_HALF) { settings.palette = p; palette_set(p); ui.force = 1; }
            gr_step(k);
            for (f = 0; f < GR_SCRIPT[k].frames; f++) {
                frame();
                if (k >= GR_HALF && n < nh) h[n++] = gr_hash(0, 240);
            }
        }
        if (write(fd[1], h, nh * sizeof *h) != (ssize_t)(nh * sizeof *h)) _exit(1);
        _exit(0);
    }
    close(fd[1]);
    if (read(fd[0], h, nh * sizeof *h) != (ssize_t)(nh * sizeof *h)) { fprintf(stderr, "inert: a child failed\n"); exit(1); }
    close(fd[0]);
    waitpid(pid, 0, 0);
}
static uint32_t gr_frame_pairs;
static void gr_frames_gnomad(void)
{
    static uint64_t a[256], b[256], c[256];
    uint32_t p, i, k, nh = 0, j;
    for (k = GR_HALF; k < NELEM(GR_SCRIPT); k++) nh += GR_SCRIPT[k].frames;
    for (p = 0; p < 8u; p++)
        for (i = 0; i < NELEM(GR_FRAME_SCENES); i++) {
            uint32_t s = (uint32_t)GR_FRAME_SCENES[i];
            gr_frames_after(s, p, p, a, nh);              /* p throughout */
            gr_frames_after(s, GSK_PAL, p, b, nh);        /* GNOMAD, then p */
            gr_frames_after(s, (p + 1u) % 8u, p, c, nh);  /* another of upstream's, then p (for the record) */
            gr_frame_pairs += nh;
            for (j = 0; j < nh && a[j] == b[j]; j++) ;
            if (j < nh) {
                fprintf(stderr, "inert: %s in %s: frame %u after leaving GNOMAD differs\n", S_NAME[s], UI_PALETTES[p].name, j);
                gr_bad++;
            }
            for (j = 0; j < nh && a[j] == c[j]; j++) ;
            if (j < nh)
                printf("inert: (%s in %s: frame %u after leaving %s differs too: upstream's own)\n", S_NAME[s],
                       UI_PALETTES[p].name, j, UI_PALETTES[(p + 1u) % 8u].name);
        }
}
#endif
#if GSK_BUILD
/* scene s drawn twice, the second time in palette p: way 0 in p both times, way 1 in GNOMAD first (a palette switch, as
 * MENU > COLOR makes it, then upstream's full redraw); ways 2 and 3 the same with the scene set up again in between.
 * Each way runs in a child of the same parent, so the views that move from frame to frame (the piano roll's, the
 * meters, the save-slot cache) start alike: the hash of the last draw */
static uint64_t gr_way(uint32_t s, uint32_t p, uint32_t way)
{
    int fd[2];
    pid_t pid;
    uint64_t h = 0;
    if (pipe(fd)) { perror("pipe"); exit(1); }
    pid = fork();
    if (pid < 0) { perror("fork"); exit(1); }
    if (!pid) {
        close(fd[0]);
        setup((int)s); pal(way & 1u ? GSK_PAL : p); draw((int)s);
        if (way & 2u) setup((int)s);
        pal(p); draw((int)s);
        h = gr_hash(0, 240);
        if (write(fd[1], &h, sizeof h) != (ssize_t)sizeof h) _exit(1);
        _exit(0);
    }
    close(fd[1]);
    if (read(fd[0], &h, sizeof h) != (ssize_t)sizeof h) { fprintf(stderr, "inert: a child failed\n"); exit(1); }
    close(fd[0]);
    waitpid(pid, 0, 0);
    return h;
}
static uint32_t gr_pairs;
static void gr_after(void)
{
    uint32_t p, s;
    for (p = 0; p < 8u; p++)
        for (s = 0; s < S_COUNT; s++) {
            uint64_t h0;
            if (!gr_scene_ok(s) || s == S_CALIBRATION) continue;   /* (calibration: two fixed draws, no state) */
            h0 = gr_way(s, p, 0);
            gr_pairs += 2;
            if (gr_way(s, p, 1) != h0) {
                fprintf(stderr, "inert: %s in %s differs after a switch from GNOMAD\n", S_NAME[s], UI_PALETTES[p].name);
                gr_bad++;
            }
            if (gr_way(s, p, 3) != gr_way(s, p, 2)) {
                fprintf(stderr, "inert: %s in %s differs after GNOMAD, set up again\n", S_NAME[s], UI_PALETTES[p].name);
                gr_bad++;
            }
        }
}
#else
static uint32_t gr_pairs;
#endif
static int gr_inert(const char *path)
{
    uint32_t p, s, e, i, c;
    char name[96];
    gr_out = fopen(path, "w");
    if (!gr_out) { fprintf(stderr, "cannot write %s\n", path); return 1; }
    rep = fopen("/dev/null", "w");
    for (p = 0; p < 8u; p++) {
        for (s = 0; s < S_COUNT; s++) {                  /* upstream's scenes */
            uint64_t h;
            if (!gr_scene_ok(s)) continue;
            setup((int)s); pal(p); draw((int)s);
            h = gr_hash(0, 240);
            gr_put(S_NAME[s], p, h);
        }
        for (e = 0; e < NENGINES; e++) {                 /* every page of every engine (every variant), HOME, layers */
            uint32_t var;
            if (!eng_ok(e)) continue;
            for (var = 0; var < gr_variants(e); var++)
            for (i = 0; i < NPAGES; i++) {
                state(); pal(p); eng(e); gr_variant(e, var);
                ui.home = 0; ui.page = (uint8_t)i; page_entered();
                if (!page_visible(i)) continue;
                draw(-1);
                snprintf(name, sizeof name, "%s.%u/%s", ENGINES[e]->name, var, PAGES[i].title);
                gr_put(name, p, gr_hash(0, 240));
                for (c = 0; c < 4u; c++) {               /* every value of every card (the card band) */
                    int16_t *vp;
                    const param_desc_t *d = PAGES[i].scope == SC_GLOBAL || PAGES[i].scope == SC_TRACK ||
                                            PAGES[i].scope == SC_ENGINE ? page_desc(cur_page(), c, &vp) : 0;
                    int32_t v, v0;
                    uint64_t hh = 14695981039346656037ull;
                    if (!d || !d->label || d->label[0] == '-' || PAGES[i].graph == GR_MOD) continue;
                    v0 = *vp;
                    for (v = d->min; v <= d->max; v++) {
                        *vp = (int16_t)v;
                        ui.force = 1;
                        draw_columns();
                        hh = (hh ^ gr_hash(Y_LABEL, Y_LABEL + CARD_H)) * 1099511628211ull;
                        if (v - d->min > 300) v = d->max - 1;
                    }
                    *vp = (int16_t)v0;
                    snprintf(name, sizeof name, "%s.%u/%s/card%u", ENGINES[e]->name, var, PAGES[i].title, c + 1u);
                    gr_put(name, p, hh);
                }
            }
            for (var = 0; var < gr_variants(e); var++) {
                uint32_t cc;
                state(); pal(p); eng(e); gr_variant(e, var); go_home(); draw(-1);
                snprintf(name, sizeof name, "%s.%u/HOME", ENGINES[e]->name, var);
                gr_put(name, p, gr_hash(0, 240));
                for (cc = 0; cc < 4u; cc++) {            /* HOME's every value */
                    int16_t *vp;
                    const param_desc_t *d = home_param(cc, &vp);
                    int32_t v, v0 = *vp;
                    uint64_t hh = 14695981039346656037ull;
                    for (v = d->min; v <= d->max; v++) {
                        *vp = (int16_t)v; ui.force = 1; draw_columns();
                        hh = (hh ^ gr_hash(Y_LABEL, Y_LABEL + CARD_H)) * 1099511628211ull;
                        if (v - d->min > 300) v = d->max - 1;
                    }
                    *vp = (int16_t)v0;
                    snprintf(name, sizeof name, "%s.%u/HOME/card%u", ENGINES[e]->name, var, cc + 1u);
                    gr_put(name, p, hh);
                }
            }
            for (i = LAYER_FX; i < LAYER_N; i++) {       /* the hold layers over HOME */
                state(); pal(p); eng(e); go_home(); ui.layer = (uint8_t)i; draw(-1);
                snprintf(name, sizeof name, "%s/layer%u", ENGINES[e]->name, i);
                gr_put(name, p, gr_hash(0, 240));
            }
        }
    }
    for (p = 0; p < 8u; p++)                             /* successive frames, the framebuffer kept */
        for (i = 0; i < NELEM(GR_FRAME_SCENES); i++)
            gr_frames(p, (uint32_t)GR_FRAME_SCENES[i]);
    fclose(gr_out);
#if GSK_BUILD
    gr_after();
    gr_frames_gnomad();
    printf("inert: %u frames compared after leaving GNOMAD, from %u scenes x 8 palettes\n", gr_frame_pairs,
           (unsigned)NELEM(GR_FRAME_SCENES));
#endif
    printf("inert: %u screens hashed in the eight palettes%s; %u differ after GNOMAD (of %u)\n", gr_n,
           GSK_BUILD ? " (GSK_BUILD)" : " (no GSK_BUILD)", gr_bad, gr_pairs);
    return gr_bad ? 1 : 0;
}

#if GSK_BUILD
/* ------------------------------------------------------------------------------------------------ sheet --- */
static const char *gs_dir;
static FILE *gs_index;
static uint32_t gs_n;
/* the screen as it is: linted (upstream's lint), OUTDIR/NNN_file.ppm, its name in the index */
static void gs_snap(const char *file, const char *label)
{
    char path[600];
    uint32_t i;
    FILE *f;
    cur_name = file;
    lint();
    snprintf(path, sizeof path, "%s/%03u_%s.ppm", gs_dir, gs_n, file);
    f = fopen(path, "wb");
    if (!f) { fprintf(stderr, "cannot write %s\n", path); exit(1); }
    fprintf(f, "P6\n240 240\n255\n");
    for (i = 0; i < 240u * 240u; i++) {
        uint16_t c = swap16(host_screen[i]);
        uint8_t b[3] = {(uint8_t)((c >> 11) * 255u / 31u), (uint8_t)(((c >> 5) & 63u) * 255u / 63u), (uint8_t)((c & 31u) * 255u / 31u)};
        fwrite(b, 1, 3, f);
    }
    fclose(f);
    fprintf(gs_index, "%03u_%s.ppm\t%s\n", gs_n, file, label);
    gs_n++;
}
static void gs_begin(void) { state(); pal(GSK_PAL); }
static void gs_scene(int s, const char *file, const char *label)
{
    setup(s);
    if (s != S_MESSAGE && s != S_MESSAGE_KEY && s != S_NAME_PLAYING && s != S_LAYER_HINT)
        ui.msg_t = 0;                                    /* (no flash here: the stores say "SAVED (RAM)") */
    pal(GSK_PAL);
    draw(s);
    gs_snap(file, label);
}
/* the page's own title in words with upstream's code ("Envelope (ENV)") */
static void gs_title(char *b, uint32_t n)
{
    char t[32];
    const page_t *pg = cur_page();
    uint32_t e = TSEL->eng_req % NENGINES;
    const char *code = ui.home ? "HOME" : pg->scope == SC_ENGINE ? ENGINES[e]->page_title[pg->id[0] != P_E0] :
                       grid_on() ? "GRID" : pg->title;
    gsk_title(t, sizeof t);
    snprintf(b, n, "%s (%s%s%s)", t, ui.home || pg->scope == SC_ENGINE ? ENGINES[e]->name : "",
             ui.home || pg->scope == SC_ENGINE ? " " : "", code);
}
static void gs_sheet(void)
{
    char file[64], label[96];
    uint32_t e, i, k;
    static const char *const PAGE_ORDER[] = {"ENV", "ENV DEST", "LFO", "LFO DEST", "MOD", "FX", "SLICER", "DLY", "REVERB",
        "CHORUS", "SCL", "CHORD", "VOICE", "VOICE 2", "GLOBAL", "SYSTEM", "PRESETS", "USER", "PROJECT", "TOOLS", "ARP",
        "ARP 2", "STEP", "PATTERN", "PHRASES", "MIXER", "SONG", "CHANCE", "MOTION"};
    /* the engines: HOME, EDIT 1, EDIT 2 (SLICE: and SLICES) */
    for (e = 0; e < NENGINES; e++) {
        if (!eng_ok(e)) continue;
        for (k = 0; k < 3u; k++) {
            gs_begin(); eng(e);
            if (k == 0u) go_home(); else go_title(k == 1u ? "EDIT 1" : "EDIT 2");
            if (k == 0u) { ui.hot_col = 1; ui.hot_t = 30; }   /* knob 2 just turned on every HOME */
            draw(-1);
            gs_title(label, sizeof label);
            snprintf(file, sizeof file, "%s_%s", ENGINES[e]->name, k == 0u ? "home" : k == 1u ? "edit1" : "edit2");
            gs_snap(file, label);
        }
    }
    /* the descriptor variants, every combination of their selectors on each page that shows them */
    for (k = 0; k < NELEM(GR_VARIANTS); k++) {
        uint32_t var, pg;
        for (var = 1; var < GR_VARIANTS[k].n; var++)
            for (pg = 0; pg < 3u; pg++) {
                if (GR_VARIANTS[k].e == 3u && pg == 2u) continue;   /* (LOFI's selectors change EDIT 1 and HOME) */
                gs_begin(); eng(GR_VARIANTS[k].e); gr_variant(GR_VARIANTS[k].e, var);
                if (pg == 0u) go_home(); else go_title(pg == 1u ? "EDIT 1" : "EDIT 2");
                draw(-1);
                gs_title(label, sizeof label);
                snprintf(label + strlen(label), sizeof label - strlen(label), ", %s", GR_VARIANTS[k].what[var]);
                snprintf(file, sizeof file, "%s_%s_v%u", ENGINES[GR_VARIANTS[k].e]->name, pg == 0u ? "home" : pg == 1u ?
                         "edit1" : "edit2", var);
                gs_snap(file, label);
            }
    }
#if FELUCCA_SLICE
    gs_begin(); eng(13u); go_page(GR_SLICES); sp.sel = 5; draw(-1);
    gs_title(label, sizeof label); gs_snap("SLICE_slices", label);
#endif
    /* the shared pages, as ANALOG's part 1 */
    for (i = 0; i < NELEM(PAGE_ORDER); i++) {
        gs_begin(); go_title(PAGE_ORDER[i]);
        if (!strcmp(PAGE_ORDER[i], "ENV")) { ui.hot_col = 2; ui.hot_t = 30; }
        if (!strcmp(PAGE_ORDER[i], "MOD")) { TSEL->p[P_M1SRC] = 1; TSEL->p[P_M1DST] = 2; TSEL->p[P_M1AMT] = 40; mod_ui_slot = 0; }
        if (!strcmp(PAGE_ORDER[i], "SLICER")) TSEL->p[P_SLCR] = 1;
        if (!strcmp(PAGE_ORDER[i], "TOOLS")) ui.act = 1;
        if (!strcmp(PAGE_ORDER[i], "STEP")) { song.rec = 1; ui.cursor = 6; }
        draw(-1);
        gs_title(label, sizeof label);
        snprintf(file, sizeof file, "page_%s", PAGE_ORDER[i]);
        for (k = 0; file[k]; k++) if (file[k] == ' ') file[k] = '_';
        gs_snap(file, label);
    }
    gs_scene(S_DRUM, "page_STEP_drum", "Drum grid (STEP, drum part: GRID)");
    gs_scene(S_MIXER_PAN, "page_MIXER_pan", "Mixer, Pan just turned (MIXER)");
    gs_scene(S_SONG, "page_SONG_rows", "Song, three rows (SONG)");
    gs_scene(S_USER, "page_USER_slot", "User sounds, a used slot (USER)");
    gs_scene(S_PROJECT, "page_PROJECT_save", "Projects, Save picked (PROJECT)");
    gs_scene(S_REVERB, "page_REVERB_spring", "Reverb, spring (REVERB)");
    gs_scene(S_CHORD_OFF, "page_CHORD_off", "Chord keys off (CHORD)");
    gs_scene(S_PRESETS_NOFAV, "page_PRESETS_nofav", "Sounds, no favourites (PRESETS)");
    /* the hold layers */
    gs_scene(S_FX_HELD, "layer_FX", "Live effects (FX held)");
    gs_scene(S_FX_WAIT, "layer_FX_wait", "Live effects, a repeat waiting (FX held)");
    gs_scene(S_FX_HARM, "layer_FX_shimmer", "Live effects, octave: Shimmer (FX held)");
    gs_scene(S_GLO_ACTIVE, "layer_GLO", "Quick mix (GLO held)");
    gs_scene(S_SCL_ACTIVE, "layer_SCL", "Key and chord (SCL held)");
    gs_scene(S_EDIT_ACTIVE, "layer_EDIT", "Choose engine (EDIT held)");
    gs_scene(S_LAYER_HINT, "layer_hint", "Hold GLO for shortcuts (a tap)");
    /* the menu */
    gs_scene(S_MENU, "menu", "Menu, Colour: GNOMAD (HOME held)");
    gs_scene(S_MENU_SPEAKER, "menu_speaker", "Menu, Speaker (HOME held)");
    gs_scene(S_MENU_HOLD, "menu_hold", "Menu, Hold time (HOME held)");
    gs_scene(S_MENU_LEDS, "menu_lights", "Menu, Lights (HOME held)");
    gs_scene(S_ABOUT, "menu_about", "About / Credits (MENU > ABOUT)");
    gs_scene(S_ABOUT_CREDITS, "menu_credits", "About / Credits, the credits (MENU > ABOUT)");
    /* NAME */
    gs_scene(S_NAME_USER, "name_save", "Name: Save U07 (SAVE > USER)");
    gs_scene(S_NAME_TYPING, "name_typing", "Name: a letter cycling (SAVE > USER)");
    gs_scene(S_NAME_123, "name_123", "Name: 123, a project (SAVE > PROJECT)");
    gs_scene(S_NAME_EMPTY, "name_empty", "Name: empty, the placeholder (SAVE > PROJECT)");
    gs_scene(S_NAME_FULL, "name_full", "Name: full, Rename (EDIT on USER)");
    gs_scene(S_NAME_PLAYING, "name_playing", "Name: playing, Stop to save");
    /* the confirmations */
    for (k = CF_CLEAR_SEQ; k <= CF_ERASE_USER; k++) {
        static const char *const CF[] = {"", "clear_pattern", "clear_part", "replace_project", "replace_user",
                                         "replace_pattern", "delete_row", "clear_song", "reset_sound", "clear_motion",
                                         "erase_user"};
        gs_begin(); song.playing = 0; up_store(6, "MY LONG BASS"); project_save_as(0, "LOFI JAM");
        ui.msg_t = 0;                                    /* (the stores' own messages) */
        ui.confirm = (uint8_t)k; ui.confirm_trk = k == CF_OVR_USER || k == CF_ERASE_USER ? 6u : k == CF_OVR_PROJ ? 0u : 2u;
        draw(-1);
        snprintf(file, sizeof file, "confirm_%s", CF[k]);
        snprintf(label, sizeof label, "Confirmation: %s", CF[k]);
        for (i = 0; label[i]; i++) if (label[i] == '_') label[i] = ' ';
        gs_snap(file, label);
    }
    /* calibration, messages, the header's states */
    gs_scene(S_CALIBRATION, "calibration", "Teach controls (MENU > CALIBRATION)");
    gs_scene(S_MESSAGE, "message_loaded", "A message: Loaded (a pattern)");
    gs_scene(S_MESSAGE_KEY, "message_key", "A message with a keycap");
    gs_begin(); go_home(); ui_message("STOP TO SAVE"); draw(-1); gs_snap("message_stop", "A message: Stop to save");
    gs_begin(); go_home(); ui_say("UNDO/REDO ", "T2"); draw(-1); gs_snap("message_undo", "A message: Undo/redo part 2");
    gs_scene(S_MOTION_REC, "page_MOTION_rec", "Knob recording, recording (MOTION)");
    gs_scene(S_BATT_USB, "home_usb", "Analog, on USB power (HOME)");
}
/* every value of every card on every page of every engine, and every page whole, in GNOMAD: linted */
static uint32_t gs_nsweep;
static void gs_sweep(void)
{
    uint32_t e, i, c;
    char name[64];
    for (e = 0; e < NENGINES; e++) {
        if (!eng_ok(e)) continue;
        for (i = 0; i < NPAGES; i++) {
            state(); pal(GSK_PAL); eng(e);
            ui.home = 0; ui.page = (uint8_t)i; page_entered();
            if (!page_visible(i)) continue;
            snprintf(name, sizeof name, "GNOMAD %s/%s", ENGINES[e]->name, PAGES[i].title);
            cur_name = name;
            for (c = 0; c < 4u; c++) {
                int16_t *vp;
                const param_desc_t *d = PAGES[i].scope == SC_GLOBAL || PAGES[i].scope == SC_TRACK || PAGES[i].scope == SC_ENGINE
                                        ? page_desc(cur_page(), c, &vp) : 0;
                int32_t v, v0;
                if (!d || !d->label || d->label[0] == '-' || PAGES[i].graph == GR_MOD) continue;
                v0 = *vp;
                for (v = d->min; v <= d->max; v++) {
                    uint32_t k;
                    *vp = (int16_t)v;
                    for (k = 0; k < 2u; k++) {           /* idle, and the knob just turned */
                        nscr = npend = 0;
                        ui.hot_col = (uint8_t)c; ui.hot_t = (uint8_t)(k ? 30 : 0);
                        ui.force = 1;
                        draw_columns();
                        lint();
                        gs_nsweep++;
                    }
                    if (v - d->min > 300) v = d->max - 1;
                }
                *vp = (int16_t)v0;
                ui.hot_t = 0;
            }
            draw(-1);
            lint();
        }
        state(); pal(GSK_PAL); eng(e); go_home();
        snprintf(name, sizeof name, "GNOMAD %s/HOME", ENGINES[e]->name);
        cur_name = name;
        for (c = 0; c < 4u; c++) {                       /* HOME's knobs: every value */
            int16_t *vp;
            const param_desc_t *d = home_param(c, &vp);
            int32_t v, v0 = *vp;
            for (v = d->min; v <= d->max; v++) {
                *vp = (int16_t)v;
                nscr = npend = 0;
                ui.force = 1;
                draw_columns();
                lint();
                gs_nsweep++;
                if (v - d->min > 300) v = d->max - 1;
            }
            *vp = (int16_t)v0;
        }
        draw(-1);
        lint();
    }
    {   /* upstream's scenes, all of them, in GNOMAD */
        uint32_t s;
        for (s = 0; s < S_COUNT; s++) {
            char n[64];
            if (!gr_scene_ok(s)) continue;
            snprintf(n, sizeof n, "GNOMAD/%s", S_NAME[s]);
            cur_name = n;
            setup((int)s); pal(GSK_PAL); draw((int)s);
            lint();
        }
    }
}
#if FELUCCA_FM4
/* (a FELUCCA_FM4=1 build) the five operator pages of the retired DIGITAL engine, dormant in the default build:
 * appended to the sheet's index */
static int gs_ops(const char *dir)
{
    static const char *const OPS[] = {"OP1 ENV", "OP2 ENV", "OP3 ENV", "OP4 ENV", "OP LEVEL"};
    char path[600], label[96], file[32], line[256];
    uint32_t k;
    FILE *f;
    gs_dir = dir;
    snprintf(path, sizeof path, "%s/index.tsv", dir);
    if ((f = fopen(path, "r"))) { while (fgets(line, sizeof line, f)) gs_n++; fclose(f); }
    gs_index = fopen(path, "a");
    snprintf(path, sizeof path, "%s/report-ops.txt", dir);
    rep = fopen(path, "w");
    if (!rep || !gs_index) { fprintf(stderr, "cannot write in %s\n", dir); return 1; }
    nfind = 0;
    for (k = 0; k < NELEM(OPS); k++) {
        gs_begin(); eng(ENGI_DIGITAL); go_title(OPS[k]);
        if (k == 4u) { ui.hot_col = 2; ui.hot_t = 30; }
        draw(-1);
        gs_title(label, sizeof label);
        snprintf(file, sizeof file, "op_%u", k + 1u);
        snprintf(label + strlen(label), sizeof label - strlen(label), ", FELUCCA_FM4 only");
        gs_snap(file, label);
    }
    fclose(rep);
    fclose(gs_index);
    printf("sheet: the %u dormant DIGITAL operator pages (FELUCCA_FM4=1) added; %u lint findings\n", (unsigned)NELEM(OPS), nfind);
    return nfind ? 1 : 0;
}
#endif
static int gs_run(const char *dir)
{
    char path[600];
    gs_dir = dir;
    snprintf(path, sizeof path, "%s/report.txt", dir);
    rep = fopen(path, "w");
    snprintf(path, sizeof path, "%s/index.tsv", dir);
    gs_index = fopen(path, "w");
    if (!rep || !gs_index) { fprintf(stderr, "cannot write in %s\n", dir); return 1; }
    nfind = 0;
    gs_sheet();
    gs_sweep();
    fprintf(rep, "\n%u screens, %u card values (idle and just turned); %u lint findings, %u ellipsised free texts\n",
            gs_n, gs_nsweep, nfind, nfree);
    fclose(rep);
    fclose(gs_index);
    printf("sheet: %u GNOMAD screens in %s, %u card values swept; %u lint findings (%s/report.txt)\n", gs_n, dir,
           gs_nsweep, nfind, dir);
    return nfind ? 1 : 0;
}
#endif

static int gn_main(int argc, char **argv)
{
    if (argc == 3 && !strcmp(argv[1], "inert"))
        return gr_inert(argv[2]);
#if GSK_BUILD
    if (argc == 3 && !strcmp(argv[1], "sheet"))
        return gs_run(argv[2]);
#if FELUCCA_FM4
    if (argc == 3 && !strcmp(argv[1], "ops"))
        return gs_ops(argv[2]);
#endif
#endif
    fprintf(stderr, "usage: gn_skin_render inert OUT.txt | sheet OUTDIR (GSK_BUILD)\n");
    return 2;
}
/* runs before upstream's main (tests/ui_render.c), which is never reached */
__attribute__((constructor)) static void gn_first(int argc, char **argv, char **envp)
{
    (void)envp;
    exit(gn_main(argc, argv));
}
