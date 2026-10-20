/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kyle Palaniuk */
/* Gnomad, a fork of Felucca (see FORK.md). Not part of upstream Felucca.
 * GNOMAD's stored id, 13 (the legacy PAPER id; option C, section 1), through Gnomad and through unmodified 1.0.1:
 * the settings import and export (settings_persist.c), the boot (persist_boot, settings_init: also the retained
 * SET3 record), the editor's full backup of the settings object (editor_backup.c: its validation, then the
 * restore), the editor's preferences commands (editor_preferences.c: UI_PALETTES, UI_STATE, UI_SET, the deferred
 * save), on simulated NOR flash. The same source is built twice by tests/gn_skin_tests.sh:
 *   with GSK_BUILD, against this tree:   13 is GNOMAD and GNOMAD is saved as 13; tagged PAPER (70) stays PAPER;
 *                                      every other old id and the eight tagged ones as upstream; the record
 *                                      Gnomad writes with GNOMAD on goes to FILE
 *   without, against the base's sources (gn/base-1.0.1, upstream 1.0.1 as released for this purpose):
 *                                      13 is PAPER, as always; the record in FILE, written by Gnomad, is accepted
 *                                      by the backup's validation and restores as PAPER, and the import reads it.
 *   the stored ids 13, 70 and 72 through import, export and the backup's validation in both builds (72, a
 *   tagged GNOMAD nothing writes: refused by the backup in both, canonicalised to 13 by Gnomad's import); a full
 *   BACKUP_LIST / GET / PUT round trip of the settings object and a power-on after it, every other setting kept.
 *   gn_skin_persist_test OUT [IN]   OUT: this build's settings object as its BACKUP GET gives it (Gnomad: with
 *                                   GNOMAD on); IN: the other build's, restored here by BACKUP PUT and read back
 *   tests/gn_skin_tests.sh runs Gnomad (OUT), then 1.0.1 (OUT, IN = Gnomad's), then Gnomad again (IN = 1.0.1's).
 * Harness: tests/backup_test.c's and tests/persistence_test.c's (upstream's), the same includes and stubs. */
#ifndef GSK_BUILD
#define GSK_BUILD 0
#endif
#if GSK_BUILD
#include "../firmware/src/gn_skin.h"
#endif
static unsigned char host_samples[3][0x14000];
#define SMP_USER_XIP(k) host_samples[k]
#define main hostsim_main
#include "hostsim.c"
#undef main

static int32_t fm1_enc_take(uint32_t e) { (void)e; return 0; }
static void fm1_irq_off(void) {}
static void fm1_irq_on(void) {}
static void lcd_sync(void) {}
static void lcd_blit(uint32_t x, uint32_t y, uint32_t w, uint32_t h, const uint16_t *p)
{ (void)x; (void)y; (void)w; (void)h; (void)p; }
#define FELUCCA_FLASH 1
#include "../firmware/src/gfx.c"
#include "../firmware/src/panel.c"
#include "../firmware/src/ui.c"
static void panel_setup(void) {}

static uint8_t nor[0x100000], flash_ok = 1;
static int erase_error;
static uint32_t erases;
static int st_read(uint32_t off, void *dst, uint32_t n) { memcpy(dst, nor + off, n); return 0; }
static int st_erase(uint32_t off) { if (erase_error) return -8; memset(nor + off, 0xFF, 4096); erases++; return 0; }
static int st_prog(uint32_t off, const void *src, uint32_t n) { memcpy(nor + off, src, n); return 0; }
static uint32_t irq_save(void) { return 0; }
static void irq_restore(uint32_t f) { (void)f; }
static uint32_t fl_jedec_ram(void) { return 0x856014u; }
static void fl_plain_window_init(void) {}
#define FL_FAR(fn) (fn)
#include "../firmware/src/storage.c"
#include "../firmware/src/upreset.c"
#include "../firmware/src/project.c"

enum { ED_BACKUP_LIST = 65, ED_BACKUP_GET, ED_BACKUP_PUT };
static uint8_t rep[4096];
static uint32_t rep_n;
static void ed_b(uint32_t v) { if (rep_n < sizeof rep) rep[rep_n++] = (uint8_t)(v & 127u); }
static uint32_t ed_unpack7(const uint8_t *a, uint32_t na, uint8_t *out, uint32_t max)
{
    uint32_t n = 0;
    while (na && n < max) {
        uint32_t m = *a++, j;
        na--;
        uint32_t k = na > 7u ? 7u : na;
        if (!k || n + k > max || (m >> k))
            return 0;
        for (j = 0; j < k; j++, na--)
            out[n++] = (uint8_t)(*a++ | ((m >> j) & 1u) << 7);
    }
    return na ? 0u : n;
}
static uint8_t ed_smp_buf[512] __attribute__((aligned(4)));
static void fm1_wdt_feed(void) {}
static int ed_flash_stop(void) { return transport_busy(); }
#include "../firmware/src/editor_backup.c"
enum { ED_UI_STATE = 34, ED_UI_SET, ED_UI_PALETTES, ED_FAV_GET, ED_FAV_SET };
static char pal_names[16][16];
static uint32_t npal_names;
static void ed_v(int32_t n) { (void)n; }
static int32_t ed_rv(const uint8_t *a) { return (int32_t)(a[0] | (uint32_t)a[1] << 7) - 8192; }
static void ed_str(const char *s, uint32_t n) { if (npal_names < 16u) snprintf(pal_names[npal_names++], sizeof pal_names[0], "%.*s", (int)n, s); }
#include "../firmware/src/editor_preferences.c"
#if GSK_BUILD
#define GSK_CORE_ONLY 1                    /* (the palette alone: no UI here) */
#include "../firmware/src/gn_skin.c"
#endif

static int bad;
static void check(const char *what, int ok)
{
    printf("persist (%s): %-74s %s\n", GSK_BUILD ? "Gnomad" : "1.0.1", what, ok ? "ok" : "FAIL");
    bad += !ok;
}
static void reset(void)
{
    memset(nor, 0xFF, sizeof nor);
    memset(&song, 0, sizeof song);
    memset(trk, 0, sizeof trk);
    memset(&persist_saved, 0, sizeof persist_saved);
    memset(&settings, 0, sizeof settings);
    memset(&ui, 0, sizeof ui);
    panel = PANEL_DEFAULT;
    settings_init();
    host_tracks_init();
    fm1_ms = 0;
    transport_req = 0;
    usb.resets = 0;
    erase_error = 0;
    erases = 0;
    ed_bk_valid = ed_bk_put = 0;
}
static uint32_t call(uint32_t cmd, const uint8_t *a, uint32_t n)
{
    rep_n = 0;
    ed_backup_handle(cmd, a, n);
    return rep_n;
}
static void put32(uint8_t *a, uint32_t v) { for (uint32_t i = 0; i < 5u; i++) a[i] = (uint8_t)((v >> (7u * i)) & 127u); }
static uint32_t put_all(uint32_t id, const void *v, uint32_t len, uint32_t crc)   /* (tests/backup_test.c's) */
{
    const uint8_t *p = v;
    uint8_t a[16 + 300];
    uint32_t off, rc;
    a[0] = 0; a[1] = (uint8_t)id; put32(a + 2, len); put32(a + 7, crc);
    call(ED_BACKUP_PUT, a, 12);
    rc = rep[2];
    for (off = 0; !rc && off < len; off += 256u) {
        uint32_t n = len - off > 256u ? 256u : len - off, k = 7;
        const uint8_t *q = p + off;
        a[0] = 1; a[1] = (uint8_t)id; put32(a + 2, off);
        while (n) {
            uint32_t g = n > 7u ? 7u : n, m = 0, i;
            for (i = 0; i < g; i++) m |= (uint32_t)(q[i] >> 7) << i;
            a[k++] = (uint8_t)m;
            for (i = 0; i < g; i++) a[k++] = q[i] & 127u;
            q += g;
            n -= g;
        }
        call(ED_BACKUP_PUT, a, k);
        rc = rep[2];
    }
    if (rc) return rc;
    a[0] = 2; a[1] = (uint8_t)id;
    call(ED_BACKUP_PUT, a, 2);
    return rep[2];
}

/* BACKUP_LIST then BACKUP_GET of the settings object (id 1) into p: 1 = read */
static int bk_get_settings(persist_t *p)
{
    uint8_t a[8] = {1};
    uint32_t len = 0, crc = 0, i;
    call(ED_BACKUP_LIST, 0, 0);
    if (rep[1]) return 0;
    for (i = 0; i < rep[2]; i++) {
        const uint8_t *o = rep + 3 + i * 11u;
        if (o[0] == 1u) { len = ed_bk_r32(o + 1); crc = ed_bk_r32(o + 6); }
    }
    if (len != sizeof *p) return 0;
    for (i = 0; i < len; i += 256u) {                 /* (a GET returns at most 256 bytes) */
        uint32_t n = len - i > 256u ? 256u : len - i;
        put32(a + 1, i);
        a[6] = (uint8_t)(n & 127u);
        a[7] = (uint8_t)(n >> 7);
        call(ED_BACKUP_GET, a, sizeof a);
        if (rep[1] || ed_unpack7(rep + 9, rep_n - 9u, (uint8_t *)p + i, n) != n) return 0;
    }
    return st_crc32(p, sizeof *p) == crc;
}
/* every field of two settings objects but the palette */
static int same_but_palette(const persist_t *a, const persist_t *b)
{
    persist_t x = *a, y = *b;
    x.palette = y.palette = 0;
    return !memcmp(&x, &y, sizeof x);
}
static uint32_t ui_set_palette(uint32_t v)            /* the editor's UI_SET of the palette: its rc */
{
    uint8_t a[2] = {0, (uint8_t)v};
    rep_n = 0;
    ed_ui_handle(ED_UI_SET, a, 2);
    return rep[0];
}
/* the settings a person might have changed, so a round trip shows they are all kept */
static void some_settings(void)
{
    settings.lowcut = 2; fx_lowcut = 2;
    settings_leds = LEDS_INV;
    settings_hold = 3;
    favorite_set(0, 3, 1);
    favorite_set(5, 1, 1);
    {   /* two knobs taught the other way round (a valid table) */
        uint8_t t = panel.enc[0];
        panel.enc[0] = panel.enc[2];
        panel.enc[2] = t;
    }
}

int main(int argc, char **argv)
{
    persist_t p, q, g;
    FILE *f;
    uint32_t i, ok;
    const uint32_t want13 = GSK_BUILD ? 8u : 6u;        /* GNOMAD here, PAPER in 1.0.1 */
    if (argc != 2 && argc != 3) { fprintf(stderr, "usage: gn_skin_persist_test OUT [IN]\n"); return 2; }

    /* the ids: 13 GNOMAD (Gnomad) / PAPER (1.0.1); the other old ids, the tagged ones as upstream */
    ok = palette_from_stored(13) == want13 && palette_stored_ok(13) && palette_from_stored(70) == 6u;
    for (i = 0; i < 20u; i++)
        if (i != 13u && palette_from_stored(i) != UI_PALETTE_MIGRATE[i]) ok = 0;
    for (i = 0; i < 8u; i++)
        if (palette_to_stored(i) != UI_PAL_TAG + i || palette_from_stored(UI_PAL_TAG + i) != i) ok = 0;
    check("13 reads as the palette it names here; old ids and tagged ids as upstream", ok);
#if GSK_BUILD
    check("GNOMAD (8) is stored as 13 and read back as GNOMAD, never as 72",
          palette_to_stored(GSK_PAL) == 13u && palette_from_stored(palette_to_stored(GSK_PAL)) == GSK_PAL &&
          NPALETTES == 9u && !strcmp(UI_PALETTES[GSK_PAL].name, GN_NAME));
#endif

    /* the import and export of the settings record */
    reset();
    memset(&p, 0, sizeof p);
    settings_export(&p);
    p.palette = 13;
    check("a settings record holding 13 imports (current format), as the palette 13 names here",
          settings_import(&p, sizeof p) == 1 && settings.palette == want13 && p.palette == (GSK_BUILD ? 13u : 70u));
    settings_export(&p);
    check("exported again: 13 (Gnomad: GNOMAD) / 70 (1.0.1: PAPER, tagged)", p.palette == (GSK_BUILD ? 13u : 70u));

    /* the boot: the record in flash, the retained settings (noinit) as they come */
    reset();
    settings_export(&p);
    p.palette = 13;
    check("the record with 13 is written to flash", st_save(OBJ_SETTINGS, &p, sizeof p) == 0);
    memset(&settings, 0xA5, sizeof settings);        /* (noinit after a power cut) */
    persist_boot();
    settings_init();
    check("power-on: the palette 13 names, and settings_init keeps it", settings.palette == want13 &&
          settings.magic == SETTINGS_MAGIC && persist_saved.palette == (GSK_BUILD ? 13u : 70u));
#if GSK_BUILD
    check("power-on with GNOMAD: the skin is on (palette_set ran its roles)", gsk.on && T_BG == RGB(0xF9, 0xF8, 0xF4));
#endif
    reset();
    settings.magic = SETTINGS_MAGIC_OLD;              /* a retained SET3 record of earlier firmware */
    settings.palette = 13;
    settings_init();
    check("a retained SET3 record with 13 migrates to the palette 13 names", settings.palette == want13);

    /* the editor's full backup: the settings object (id 1) with 13 */
    reset();
    settings_export(&p);
    p.palette = 13;
    check("backup PUT of settings with 13: accepted, restored as the palette 13 names",
          put_all(1, &p, sizeof p, st_crc32(&p, sizeof p)) == 0 && settings.palette == want13);
    p.palette = 70;
    check("backup PUT of settings with tagged PAPER (70): PAPER",
          put_all(1, &p, sizeof p, st_crc32(&p, sizeof p)) == 0 && settings.palette == 6u);

    /* 72: a tagged GNOMAD, which nothing writes; the backup refuses it in both builds (Gnomad: gn_skin.c
     * gsk_palette_stored_ok), Gnomad's import makes it 13, 1.0.1's reads it as MONO */
    reset();
    settings_export(&p);
    p.palette = 72;
    {
        uint32_t before = erases;
        check("backup PUT of settings with 72: refused (rc 2), nothing written",
              put_all(1, &p, sizeof p, st_crc32(&p, sizeof p)) == 2u && erases == before && !palette_stored_ok(72));
    }
    p.palette = 72;
    check("a flash record holding 72 imports as GNOMAD, stored 13 (Gnomad) / as MONO, 64 (1.0.1)",
          settings_import(&p, sizeof p) == 1 && settings.palette == (GSK_BUILD ? 8u : 0u) && p.palette == (GSK_BUILD ? 13u : 64u));

    /* the editor's preferences commands, the real handler */
    reset();
    settings_init();
    npal_names = 0;
    rep_n = 0;
    ed_ui_handle(ED_UI_PALETTES, 0, 0);
    check("UI_PALETTES: 9 names, the ninth GN_NAME (Gnomad) / upstream's 8 (1.0.1)",
          rep[0] == (GSK_BUILD ? 9u : 8u) && npal_names == rep[0] && !strcmp(pal_names[6], "PAPER") &&
          (!GSK_BUILD || !strcmp(pal_names[8], "GNOMAD")));
    ok = ui_set_palette(8);
    check("UI_SET palette 8: GNOMAD, saved at once as 13 (Gnomad) / refused, rc 1 (1.0.1)", GSK_BUILD ?
          ok == 0u && settings.palette == 8u && persist_saved.palette == 13u && !persist_pending :
          ok == 1u && settings.palette == UI_MONO_INDEX);
    rep_n = 0;
    ed_ui_handle(ED_UI_STATE, 0, 0);
    check("UI_STATE reports the palette's index", rep[1] == (GSK_BUILD ? 8u : UI_MONO_INDEX));
    song.playing = 1;
    ok = ui_set_palette(6);
    check("UI_SET while playing: applied, the save deferred (rc 4)", ok == 4u && settings.palette == 6u && persist_pending == 1u);
    song.playing = 0;
    for (i = 0; i < 200u && persist_pending; i++) { fm1_ms += 100u; settings_poll(); }
    check(".. and saved once stopped: PAPER as 70", !persist_pending && persist_saved.palette == 70u);

    /* the full backup round trip: LIST, GET, PUT, a power-on; every setting kept */
    reset();
    settings_init();
    some_settings();
    settings.palette = GSK_BUILD ? 8u : 6u;               /* (GNOMAD here, PAPER in 1.0.1) */
    palette_set(settings.palette);
    settings_save();
    for (i = 0; i < 200u && persist_pending; i++) { fm1_ms += 100u; settings_poll(); }
    check("BACKUP_LIST and GET of the settings object (Gnomad: GNOMAD on, 13; 1.0.1: PAPER, 70)",
          bk_get_settings(&g) && g.palette == (GSK_BUILD ? 13u : 70u) && g.lowcut == 2u);
    reset();
    settings_init();
    check("BACKUP PUT of it into a fresh device", put_all(1, &g, sizeof g, st_crc32(&g, sizeof g)) == 0u);
    memset(&settings, 0xA5, sizeof settings);
    persist_boot();
    settings_init();
    check(".. a power-on: the palette and every other setting as before (GET equal byte for byte)",
          settings.palette == (GSK_BUILD ? 8u : 6u) && settings.lowcut == 2u && settings_leds == LEDS_INV &&
          HOLD_MS[settings_hold % 4u] == 600u && bk_get_settings(&q) && !memcmp(&q, &g, sizeof q));
    if (argc == 3) {                                  /* the other build's object, restored here */
        persist_t o;
        f = fopen(argv[2], "rb");
        check("the other build's settings object read", f && fread(&o, sizeof o, 1, f) == 1);
        if (f) fclose(f);
        reset();
        settings_init();
        check("its BACKUP PUT here is accepted", put_all(1, &o, sizeof o, st_crc32(&o, sizeof o)) == 0u);
        memset(&settings, 0xA5, sizeof settings);
        persist_boot();
        settings_init();
        check(GSK_BUILD ? ".. 1.0.1's PAPER (70) is PAPER here; every other setting kept (GET: only the palette as it was)" :
                          ".. Gnomad's GNOMAD (13) is PAPER here, kept as 70; every other setting kept",
              settings.palette == 6u && bk_get_settings(&q) && q.palette == 70u && same_but_palette(&q, &o) &&
              settings.lowcut == 2u && settings_leds == LEDS_INV);
        g = q;                                        /* (what this build now holds goes on: a full circle) */
    }
    f = fopen(argv[1], "wb");
    check("this build's settings object written for the other build", f && fwrite(&g, sizeof g, 1, f) == 1);
    if (f) fclose(f);
    printf(bad ? "PERSIST TESTS FAILED\n" : "persist (%s): all passed\n", GSK_BUILD ? "Gnomad" : "1.0.1");
    return bad ? 1 : 0;
}
