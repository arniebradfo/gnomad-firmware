/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kyle Palaniuk */
/* Gnomad (FORK.md): the Advanced skin, phase 1 (docs: option C, sections 1, 2, 3, 7, 8). MENU > COLOR > GNOMAD,
 * the ninth palette, draws Felucca's own pages in our paper and ink, one colour per knob, with plain words; any
 * other palette draws them exactly as upstream does (every hook returns at once unless GNOMAD is on).
 *
 * felucca.c includes this before gfx.c and gn_skin.c after the UI files (one compilation unit): the guarded
 * lines in upstream's files call the functions declared here, and only when GSK_BUILD is 1. A host test that
 * includes upstream's sources one by one (tests/ui_test.c and the others) has no GSK_BUILD: it compiles them as
 * upstream does, with eight palettes. Every name a person sees comes from the word tables (gn_skin_words.c);
 * the product's own name comes from GN_NAME alone: phase 1 shows it as the palette's name (MENU > COLOR and
 * the editor's palette list read it from the GNOMAD record, tools/gn_ui_palette.py). */
#include <stdint.h>

#if defined(__clang__)
#define GSK_MIN __attribute__((minsize))   /* main-loop code, built for size as upstream's UI files are (tools/size_fns.py) */
#else
#define GSK_MIN
#endif

#ifndef GN_NAME
#define GN_NAME "GNOMAD"             /* the product's name as the synth shows it (the build's name: FELUCCA_VERSION) */
#endif
#define GSK_PAL 8u                    /* GNOMAD's runtime palette index: settings.palette, MENU > COLOR's ninth */
#define GSK_STORED 13u                /* .. as the settings record stores it: the legacy PAPER id (below 20). Gnomad
                                      * reads 13 as GNOMAD; unmodified 1.0.1 reads it as PAPER and accepts it in its
                                      * settings import and backup validation, where UI_PAL_TAG + 8 = 72 is refused */

static struct {
    uint8_t on;                      /* GNOMAD selected (palette_set); Easy's entry and exit never touch it */
    char made[40];                   /* a phrase made from a wildcard (gn_skin_words.c gsk_word), drawn at once */
} gsk;                               /* the skin's resident RAM (option C: 64 B for phase 1) */

/* the sites whose fixed upstream wording the skin translates (gn_skin_words.c GSKW_PH): each its own table */
enum { GSKC_MSG, GSKC_FOOT, GSKC_LAYER, GSKC_NOTE, GSKC_MENU, GSKC_NAME, GSKC_SETUP, GSKC_CONFIRM, GSKC_DCY,
       GSKC_PATCH, GSKC_N };   /* (DCY, PATCH: two numbered choice lists, read by rule) */

struct track;
static void gsk_palette(uint32_t i);                                 /* gfx.c palette_set: GNOMAD's exact roles */
static uint32_t gsk_palette_to_stored(uint32_t i);                   /* gfx.c palette_to_stored */
static int gsk_palette_stored_ok(uint32_t v);                        /* gfx.c palette_stored_ok: never 72 */
static void gsk_keycap(uint32_t id, uint16_t *fill, uint16_t *ink);  /* gfx.c cv_keycap: badge paint */
static int gsk_column(uint32_t c, const char *label, const char *val, const char *unit, uint16_t vc,
                      int32_t ratio);                                /* ui_draw.c draw_column: the card */
static int gsk_hint(const char *s);                                  /* ui_draw.c draw_head: a message in words */
static int gsk_foot(void);                                           /* ui_draw.c draw_foot */
static int gsk_confirm(void);                                        /* ui_draw.c draw_confirm */
static int gsk_menu(void);                                           /* ui_menu.c draw_menu */
static void gsk_nm_title(char *b);                                   /* ui_name.c nm_title: by NAME's kind */
static int gsk_nm_panel(void);                                       /* ui_name.c nm_draw_panel */
static int gsk_nm_foot(void);                                        /* ui_name.c nm_draw_foot */
static int gsk_setup_title(void);                                    /* ui_input.c setup_title */
static int gsk_song(void);                                           /* ui_graph.c graph_song */
static int gsk_grid(const struct track *t, uint16_t c);              /* ui_graph.c graph_grid */
static int gsk_mod(const struct track *t, uint16_t c);               /* ui_graph.c graph_mod */
static void gsk_slen(char *t, const char *u);                        /* ui_graph.c graph_slices: the length */
static void gsk_note3(const char **a, const char **b, const char **c);   /* ui_graph.c panel_note */
static int gsk_scl(void);                                            /* ui_layer.c layer_scl */
static void gsk_lcell(uint32_t st, uint32_t icon, uint16_t *fill, uint16_t *ink, const char **name);   /* lcell */
static int gsk_keyrow(int32_t x0, int32_t x1, int32_t y, const void *h, uint32_t n, uint32_t on,
                      uint16_t under);                               /* a row of key hints (khint_t) in words */
static const char *gsk_text(uint32_t site, const char *s);           /* upstream's fixed wording at a site (GSKC_*) */
