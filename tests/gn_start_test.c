/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kyle Palaniuk */
/* Gnomad (FORK.md) host test, round 7 pick 1: how the synth starts. The first power-on (no settings
 * record of ours) is Easy, in GNOMAD's colours where the Advanced skin is built in; after that the synth
 * starts in the mode it was left in. The mode rides in favourites' row 15, byte 30 (gn_easy.c, the mode):
 * a byte unmodified 1.0.1 never reads or checks and keeps as saved. Through the firmware's own code
 * (tests/gn_host.h) with GN_START_EASY on, as the firmware has it:
 *   - no record, and a record of unmodified 1.0.1 (no marker): Easy at the first frame, GNOMAD, nothing
 *     written to flash for it;
 *   - Easy -> Advanced -> power cycle -> Advanced, and back -> Easy (the mode written by the normal
 *     settings save, while stopped; queued while playing, written after the stop);
 *   - 1.0.1's own code paths, which this tree keeps as upstream's bytes (settings_persist.c, favorites.c,
 *     panel.c, editor_backup.c; FORK.md, touch list): its import and its backup restore accept a record
 *     we wrote, everything 1.0.1 reads from it is what it would read without our byte, and its save keeps
 *     the byte as it was.
 *   tests/run_fork_tests.sh builds it: build/host-gn/gn_start_test */
/* Easy-only build has no skin. Exercise its migration branch with a valid palette
 * index as a test stand-in; merged trees use gn_skin.h's real GNOMAD index. */
#if defined(__has_include)
#if !__has_include("../firmware/src/gn_skin.h")
#define GSK_PAL 7u
#endif
#endif
#include "gn_host.h"
#include <sys/wait.h>
#include <unistd.h>

static int fails;
#define CHECK(c, what)                                                                   \
    do {                                                                                 \
        if (!(c)) {                                                                      \
            fails++;                                                                     \
            printf("    FAIL line %d: %s\n", __LINE__, what);                            \
        }                                                                                \
    } while (0)

static uint8_t nor_kept[sizeof host_nor];
static uint32_t erases;                           /* flash erases of the settings' sectors since power-on */

/* the synth switched off and on: the flash kept, the RAM as at power-on, then project.c persist_boot's
 * import of the settings record (the host has no flash ID to read, so it is done here as persist_boot
 * does it) and main.c's settings_init and palette */
static void power_cycle(void)
{
    persist_t p;
    int n;
    memcpy(nor_kept, host_nor, sizeof nor_kept);
    host_power_on();
    memcpy(host_nor, nor_kept, sizeof nor_kept);
    memset(&persist_saved, 0, sizeof persist_saved);
    persist_pending = 0;
    n = st_load(OBJ_SETTINGS, &p, sizeof p);
    if (settings_import(&p, n))
        persist_saved = p;
    for (n = 0; n < 4; n++)                       /* (persist_boot: the project slots from flash) */
        if (!proj_import(&proj_scratch, &proj_slot[n], sizeof(project_store_t)))
            proj_fetch((uint32_t)n);
    settings_init();
    panel_init();
    palette_set(settings.palette);
    erases = 0;
}

static int flash_record(persist_t *p) { return st_load(OBJ_SETTINGS, p, sizeof *p) == (int)sizeof *p; }

static void hold_switch(void)                     /* HOME + SAVE held 2 s: the other mode */
{
    host_hold_switch(2100);
    host_wait_ms(200);
}

/* a settings record as unmodified 1.0.1 writes it: settings_export of its own state (row 15's byte 30
 * never written: 0), into flash as its settings save does */
static void write_upstream_record(uint32_t palette, uint32_t hold, uint32_t leds)
{
    persist_t p;
    memset(&p, 0, sizeof p);
    settings.palette = palette;
    settings_hold = (uint8_t)hold;
    settings_leds = (uint8_t)leds;
    favorites.factory[5][0] = 0x05;               /* a starred preset or two, a user slot */
    favorites.user = 1u << 3;
    settings_export(&p);
    CHECK(p.favorites.factory[15][30] == 0, "(1.0.1's record: byte 30 of row 15 is 0)");
    st_save(OBJ_SETTINGS, &p, sizeof p);
}

/* ---------------------------------------------------------------- cases --- */
/* Independent Felucca 1.0.1 PER4 layout, including its PAN5 calibration. Do not
 * generate the source image with Gnomad's settings_export: that could hide writes. */
typedef struct {
    uint32_t magic, palette, lowcut, zoom;
    struct { uint32_t magic; uint8_t btn[14], enc[7]; int8_t dir[7]; } panel;
    uint32_t bold;
    struct { uint8_t factory[16][32]; uint32_t user, filter; } favorites;
} felucca101_settings_t;
_Static_assert(sizeof(felucca101_settings_t) == 572, "1.0.1 PER4 size");
_Static_assert(sizeof(persist_t) == sizeof(felucca101_settings_t), "PER4 unchanged");
_Static_assert(offsetof(felucca101_settings_t, favorites.factory[15][30]) == 562, "mode offset");

static felucca101_settings_t upstream101_image(void)
{
    felucca101_settings_t p = {0};
    uint32_t r, b;
    p.magic = 0x50455234u;
    p.palette = 64u + 2u;                         /* tagged 1.0.1 palette */
    p.lowcut = 2;
    p.zoom = 0x4C454401u;                         /* LEDS INV */
    p.panel.magic = 0x50414E35u;
    for (b = 0; b < 14; b++) p.panel.btn[b] = (uint8_t)(13u - b);
    for (b = 0; b < 7; b++) {
        p.panel.enc[b] = (uint8_t)(6u - b);
        p.panel.dir[b] = b & 1u ? -1 : 1;
    }
    p.bold = 0x484C4403u;                         /* HOLD 600 ms */
    for (r = 0; r < 16; r++)
        for (b = 0; b < 32; b++)
            p.favorites.factory[r][b] = (uint8_t)(17u * r + 3u * b + 1u);
    /* 1.0.1 clears retired DIGITAL on import; its own writer therefore saves 0.
     * Unused rows otherwise survive imports, including layer_seen at [15][31]. */
    memset(p.favorites.factory[1], 0, 32);
    p.favorites.factory[15][30] = 0;              /* never set by Gnomad */
    p.favorites.user = 0xA55A6996u;
    p.favorites.filter = 1;
    return p;
}

static void t_r18_felucca101_migration(void)
{
    felucca101_settings_t source = upstream101_image(), expected = source;
    persist_t actual;
    host_start_easy = 1;
    host_power_on();
    CHECK(st_save(OBJ_SETTINGS, &source, sizeof source) == 0, "write independent 1.0.1 image");
    power_cycle();
    host_frame();
    CHECK(gn.on && settings.palette == GSK_PAL, "migration boots Easy in GNOMAD");
    CHECK(GN_MODE_BYTE == 0xA1u, "migration sets first-boot Easy, count-in");
    CHECK(flash_record(&actual) && !memcmp(&actual, &source, sizeof source), "no startup flash write");
    expected.palette = palette_to_stored(GSK_PAL);
    expected.favorites.factory[15][30] = 0xA1u;
    actual = persist_saved;
    settings_export(&actual);
    CHECK(!memcmp(&actual, &expected, sizeof actual), "only palette and mode byte changed, all 572 bytes checked");
    settings_save();
    host_frame();
    CHECK(flash_record(&actual) && !memcmp(&actual, &expected, sizeof actual), "normal save writes exactly the migrated image");
    power_cycle();
    host_frame();
    actual = persist_saved;
    settings_export(&actual);
    CHECK(!memcmp(&actual, &expected, sizeof actual), "migrated image unchanged on next boot");
}

static void t_r18_gnomad_palette_kept(void)
{
    uint32_t easy;
    for (easy = 0; easy <= 1; easy++) {
        felucca101_settings_t source = upstream101_image();
        persist_t before, after;
        host_start_easy = 1;
        host_power_on();
        source.favorites.factory[15][30] = (uint8_t)(0xA0u | easy);
        CHECK(st_save(OBJ_SETTINGS, &source, sizeof source) == 0, "seed marked image");
        power_cycle();
        host_frame();
        settings.palette = 3;                    /* user's later choice, saved by Gnomad */
        palette_set(3);
        settings_save();
        host_frame();
        CHECK(flash_record(&before), "Gnomad writes user's colour");
        power_cycle();
        host_frame();
        CHECK(gn.on == easy && settings.palette == 3, "marked Easy/Advanced keeps user's colour and mode");
        after = persist_saved;
        settings_export(&after);
        CHECK(!memcmp(&before, &after, sizeof after), "later boot changes no settings byte");
        CHECK(flash_record(&after) && !memcmp(&before, &after, sizeof after), "later boot leaves flash untouched");
    }
}

static void t_first_power_on(void)
{
    persist_t p;
    host_start_easy = 1;
    host_power_on();                              /* (no record at all: an erased flash) */
    erases = 0;
    CHECK(!gn.on, "before the main loop: nothing of Easy's");
    host_frame();
    CHECK(gn.on && gn.page == GP_HOME, "the first frame: Easy, Home (round UX-1)");
    CHECK(favorites.factory[15][30] == (GN_MODE_MARK | GN_MODE_EASY), "the mode byte: our marker, Easy (in RAM)");
#if GSK_BUILD
    CHECK(settings.palette == GSK_PAL, "GNOMAD's colours");
#endif
    host_wait_ms(3100);
    CHECK(!flash_record(&p), "nothing saved at start-up (no record written)");
    CHECK(gn_view() == GV_HOME, "after the start-up picture: Home (round UX-1)");
}

static void t_upstream_record(void)
{
    persist_t before, after;
    host_start_easy = 1;
    host_power_on();
    write_upstream_record(2, 3, LEDS_INV);        /* 1.0.1's: palette 2, HOLD 600 ms, LEDS INV, favourites */
    power_cycle();
    CHECK(flash_record(&before), "(the record of 1.0.1 in flash)");
    CHECK(settings.palette == 2u && settings_hold == 3u && settings_leds == LEDS_INV, "its settings imported");
    host_frame();
    CHECK(gn.on, "a record of unmodified 1.0.1 (no marker): Easy, the first time");
#if GSK_BUILD
    CHECK(settings.palette == GSK_PAL, "..and GNOMAD's colours");
#endif
    CHECK(favorites.factory[5][0] == 0x05 && favorites.user == 1u << 3 && settings_hold == 3u &&
          settings_leds == LEDS_INV, "..its other settings as they were");
    host_wait_ms(1000);
    CHECK(flash_record(&after) && !memcmp(&before, &after, sizeof before), "..and nothing written at start-up");
    /* the next settings save (a favourite, say) writes the mode with them; then it is not the first */
    favorite_set(ENGI_FM6, 2, 1);
    settings_save();
    host_frame();
    CHECK(flash_record(&after) && after.favorites.factory[15][30] == (GN_MODE_MARK | GN_MODE_EASY),
          "the next settings save writes the mode byte");
#if GSK_BUILD
    CHECK(palette_from_stored(after.palette) == GSK_PAL, "..and GNOMAD as the palette");
#endif
    power_cycle();
    host_frame();
    CHECK(gn.on, "power cycle: Easy (the mode kept)");
}

static void t_power_cycles(void)
{
    persist_t p;
    host_start_easy = 1;
    host_power_on();
    host_frame();
    host_wait_ms(3000);
    CHECK(gn.on, "the first power-on: Easy");
    hold_switch();
    CHECK(!gn.on, "HOME + SAVE held 2 s: Advanced");
    CHECK(flash_record(&p) && p.favorites.factory[15][30] == GN_MODE_MARK, "the mode written: Advanced (stopped)");
    power_cycle();
    host_wait_ms(500);
    CHECK(!gn.on, "power cycle: Advanced, where it was left");
    hold_switch();
    CHECK(gn.on, "HOME + SAVE held 2 s: Easy");
    CHECK(flash_record(&p) && p.favorites.factory[15][30] == (GN_MODE_MARK | GN_MODE_EASY), "the mode written: Easy");
    power_cycle();
    host_frame();
    CHECK(gn.on, "power cycle: Easy, where it was left");
    /* playing: the save waits for the stop (settings_poll: no erase while the transport runs) */
    transport_req = 1;
    host_wait_ms(100);
    CHECK(song.playing, "(the loop plays)");
    hold_switch();
    CHECK(!gn.on && persist_pending == 1u, "leaving while playing: the save queued");
    CHECK(flash_record(&p) && p.favorites.factory[15][30] == (GN_MODE_MARK | GN_MODE_EASY), "..not yet written");
    transport_req = 2;
    host_wait_ms(100);
    CHECK(!persist_pending && flash_record(&p) && p.favorites.factory[15][30] == GN_MODE_MARK, "..written after the stop");
    power_cycle();
    host_wait_ms(300);
    CHECK(!gn.on, "power cycle: Advanced");
    /* no switch, no write: a power cycle in the same mode writes nothing */
    {
        persist_t a, b;
        CHECK(flash_record(&a), "(the record)");
        host_wait_ms(500);
        power_cycle();
        host_wait_ms(500);
        CHECK(flash_record(&b) && !memcmp(&a, &b, sizeof a), "a start in the kept mode writes nothing");
    }
}

/* 1.0.1's code with a record we wrote: settings_import, settings_export and the backup restore's
 * validation and import are upstream's bytes in this tree (FORK.md, touch list: none of them is edited) */
static uint32_t backup_put_settings(const persist_t *rec)
{
    static uint8_t a[12 + 320];
    const uint8_t *src = (const uint8_t *)rec;
    uint32_t len = sizeof *rec, off, i;
    a[0] = 0;
    a[1] = 1;
    for (i = 0; i < 5u; i++) {
        a[2 + i] = (uint8_t)((len >> (7u * i)) & 127u);
        a[7 + i] = (uint8_t)((st_crc32(src, len) >> (7u * i)) & 127u);
    }
    host_request(ED_BACKUP_PUT, a, 12);
    if (host_wire_n <= 7u || host_wire[7])
        return host_wire_n > 7u ? host_wire[7] : 0xFFu;
    for (off = 0; off < len; off += 256u) {
        uint32_t n = len - off > 256u ? 256u : len - off, k = 7;
        const uint8_t *q = src + off;
        a[0] = 1;
        a[1] = 1;
        for (i = 0; i < 5u; i++)
            a[2 + i] = (uint8_t)((off >> (7u * i)) & 127u);
        while (n) {
            uint32_t g = n > 7u ? 7u : n, m = 0;
            for (i = 0; i < g; i++)
                m |= (uint32_t)(q[i] >> 7) << i;
            a[k++] = (uint8_t)m;
            for (i = 0; i < g; i++)
                a[k++] = q[i] & 127u;
            q += g;
            n -= g;
        }
        host_request(ED_BACKUP_PUT, a, k);
        if (host_wire_n <= 7u || host_wire[7])
            return host_wire_n > 7u ? host_wire[7] : 0xFFu;
    }
    a[0] = 2;
    a[1] = 1;
    host_request(ED_BACKUP_PUT, a, 2);
    return host_wire_n > 7u ? host_wire[7] : 0xFFu;
}

static void t_upstream_accepts(void)
{
    persist_t ours, theirs, back;
    uint32_t e, k, same = 1;
    host_start_easy = 1;
    host_power_on();
    host_frame();                                 /* Easy, the byte ours */
    settings_hold = 2;
    settings_leds = LEDS_INV;
    favorites.factory[0][1] = 0x80;
    favorites.user = 0x81;
    favorites.filter = 1;
    {
        uint32_t i;                               /* (round 9: Easy's song record populated too) */
        for (i = 17; i < 29u; i++)
            favorites.factory[15][i] = (uint8_t)(0x5A ^ (i * 37u));
        favorites.factory[15][29] = 0x82;
    }
    memset(&ours, 0, sizeof ours);
    settings_export(&ours);                       /* the record our firmware saves */
    CHECK(ours.favorites.factory[15][30] == (GN_MODE_MARK | GN_MODE_EASY), "(our record carries the byte)");
    theirs = ours;
    theirs.favorites.factory[15][30] = 0;         /* the same record without it */
    /* 1.0.1's import of each: everything it reads is the same */
    {
        persist_t a = ours, b = theirs;
        favorites_t fa, fb;
        uint32_t pa, ha, la;
        CHECK(settings_import(&a, sizeof a) == 1, "1.0.1's import accepts our record (current, not migrated)");
        fa = favorites;
        pa = settings.palette;
        ha = settings_hold;
        la = settings_leds;
        CHECK(settings_import(&b, sizeof b) == 1, "(and the record without the byte)");
        fb = favorites;
        for (e = 0; e < NENGINES; e++)
            for (k = 0; k < 256u; k++)
                same &= favorite_has(e, k) == ((fa.factory[e][k / 8u] >> (k % 8u)) & 1u ? 1 : 0) &&
                        ((fa.factory[e][k / 8u] >> (k % 8u)) & 1u) == ((fb.factory[e][k / 8u] >> (k % 8u)) & 1u);
        for (k = 0; k < UP_SLOTS; k++)
            same &= ((fa.user >> k) & 1u) == ((fb.user >> k) & 1u);
        same &= fa.filter == fb.filter && pa == settings.palette && ha == settings_hold && la == settings_leds &&
                fa.factory[15][31] == fb.factory[15][31];
        CHECK(same, "..everything 1.0.1 reads from it (every favourite, the filter, palette, HOLD, LEDS, its own "
                    "layer byte) is what it reads without our byte");
        favorites = fa;
        memset(&back, 0, sizeof back);
        back = a;
        settings_export(&back);
        CHECK(back.favorites.factory[15][30] == (GN_MODE_MARK | GN_MODE_EASY), "..and 1.0.1's save keeps the byte as saved");
        CHECK(!memcmp(&back.favorites.factory[15][17], &ours.favorites.factory[15][17], 14),
              "..and Easy's song record (bytes 17 to 30) as saved");
    }
    /* 1.0.1's backup restore (BACKUP_PUT of the settings, object 1): validated, written, imported */
    CHECK(backup_put_settings(&ours) == 0u, "1.0.1's backup restore accepts our record (rc 0)");
    CHECK(flash_record(&back) && !memcmp(&back, &ours, sizeof back), "..and writes it as it came");
    CHECK(favorites.factory[15][30] == (GN_MODE_MARK | GN_MODE_EASY) && settings_hold == 2u &&
          settings_leds == LEDS_INV && favorites.user == 0x81, "..and imports it: the byte, as saved; the rest read");
    CHECK(!memcmp(&favorites.factory[15][17], &ours.favorites.factory[15][17], 14), "..Easy's song record as saved");
    {
        uint8_t a[4] = {0, 0, 0, 1};               /* the preferences' saves: UI_SET, FAV_SET */
        persist_t after;
        a[1] = (uint8_t)settings.palette;
        host_request(ED_UI_SET, a, 2);
        a[0] = 2;
        a[1] = 3;
        a[2] = 0;
        a[3] = 1;
        host_request(ED_FAV_SET, a, 4);
        host_frame();
        CHECK(flash_record(&after) && !memcmp(&after.favorites.factory[15][17], &ours.favorites.factory[15][17], 14),
              "..the preferences' own saves (UI_SET, FAV_SET) keep Easy's bytes 17 to 30");
    }
    power_cycle();
    host_frame();
    CHECK(gn.on, "the restored record: the synth starts in its mode (Easy)");
}

/* the editor's UI save path (editor_preferences.c ed_ui_save, after UI_SET): 0 saved, 3 not saved, 4 queued;
 * its check compares the saved record's favourites with RAM's, the mode byte among them */
static uint32_t ui_set_rc(void)
{
    uint8_t a[2] = {0, (uint8_t)settings.palette};
    host_request(ED_UI_SET, a, 2);
    return host_wire_n > 5u ? host_wire[5] : 0xFFu;
}

static void t_editor_ui_save(void)
{
    host_start_easy = 1;
    host_power_on();
    host_frame();                                 /* first power-on: the byte in RAM, not saved */
    CHECK(gn.on && ui_set_rc() == 0u, "first power-on, then the app's UI save: 0 (its own save writes the byte too)");
    hold_switch();                                /* a switch while stopped: saved at once */
    CHECK(!gn.on && !persist_pending, "(Advanced, the switch saved)");
    CHECK(ui_set_rc() == 0u, "a switch while stopped, then the app's UI save: 0");
    transport_req = 1;
    host_wait_ms(100);
    hold_switch();                                /* while playing: queued */
    CHECK(gn.on && persist_pending == 1u, "(Easy, the save queued while playing)");
    CHECK(ui_set_rc() == 4u, "..the app's UI save meanwhile: 4, queued until STOP (as for any setting)");
    transport_req = 2;
    host_wait_ms(100);
    CHECK(!persist_pending, "(the stop: written)");
    CHECK(ui_set_rc() == 0u, "..after the deferred save, the app's UI save: 0 (no false \"not saved\")");
}

/* (9, round 8): settings restored under the running synth are made to agree with it: the mode it runs in (both
 * ways), the marker on a record of other firmware (its palette then the person's: a later pick is kept over
 * GNOMAD at the next start); written by the normal settings save, never at start-up */
static void restore_settings_with(uint8_t mode_byte, uint32_t palette)
{
    persist_t rec;
    memset(&rec, 0, sizeof rec);
    settings_export(&rec);
    rec.favorites.factory[15][30] = mode_byte;
    rec.favorites.factory[15][29] = 0;
    rec.palette = palette_to_stored(palette);
    CHECK(backup_put_settings(&rec) == 0u, "(the app restores a settings record)");
}

static void t_restore_reconcile(void)
{
    persist_t p;
    host_start_easy = 1;
    host_power_on();
    host_frame();
    host_wait_ms(3100);
    CHECK(gn.on, "(Easy)");
    restore_settings_with(GN_MODE_MARK, 2);         /* a record that says Advanced, while Easy runs */
    host_wait_ms(100);
    CHECK(gn.on && favorites.factory[15][30] == (GN_MODE_MARK | GN_MODE_EASY), "restored Advanced under Easy: the byte says Easy");
    CHECK(flash_record(&p) && p.favorites.factory[15][30] == (GN_MODE_MARK | GN_MODE_EASY), "..and is saved so");
    power_cycle();
    host_frame();
    CHECK(gn.on, "..power cycle: Easy, the mode it ran in");
    hold_switch();
    CHECK(!gn.on, "(Advanced)");
    restore_settings_with(GN_MODE_MARK | GN_MODE_EASY, 2);   /* a record that says Easy, under Advanced */
    host_wait_ms(100);
    CHECK(!gn.on && flash_record(&p) && p.favorites.factory[15][30] == GN_MODE_MARK, "restored Easy under Advanced: saved Advanced");
    power_cycle();
    host_wait_ms(300);
    CHECK(!gn.on, "..power cycle: Advanced");
    restore_settings_with(0, 2);                    /* a record of other firmware (no marker), palette 2 */
    host_wait_ms(100);
    CHECK(flash_record(&p) && p.favorites.factory[15][30] == GN_MODE_MARK && palette_from_stored(p.palette) == 2u,
          "restored without the marker: the marker set, the restored palette kept, saved");
    {
        uint8_t a[2] = {0, 3};                      /* the person picks palette 3 */
        host_request(ED_UI_SET, a, 2);
        host_wait_ms(60);
    }
    power_cycle();
    {
        persist_t before, after;
        CHECK(flash_record(&before), "(the record)");
        host_wait_ms(500);
        CHECK(!gn.on && settings.palette == 3u, "..power cycle: Advanced, the picked palette (not GNOMAD)");
        CHECK(flash_record(&after) && !memcmp(&before, &after, sizeof before), "..and nothing written at start-up");
    }
}

/* (round 9, 1 and 2): the song's slot is Easy's only while the flash's copy of it is Easy's own last write
 * (its commit sequence and payload, in the settings record): any later write of it ends that, the same bytes or
 * not, in Easy or Advanced, and restoring the old bytes does not bring it back; SAFE TO UNPLUG only once Easy's
 * record is on flash too; when it cannot be written, a plain warning and no claim (the next SAVE a new slot) */
static void easy_save(void)
{
    host_tap(B_SAVE);
    host_wait_ms(100);
}

static uint32_t first_note_slot(uint32_t slot)    /* (a slot's decoded first note of track 1, to tell them) */
{
    project_name(slot, (char[PROJ_NAME_LEN + 1u]){0});
    return proj_scratch.t[0].step[0].note[0];
}

static uint32_t backup_put_slot(uint32_t slot, const project_store_t *obj)
{
    static uint8_t a[12 + 320];
    const uint8_t *src = (const uint8_t *)obj;
    uint32_t len = sizeof *obj, off, i;
    a[0] = 0;
    a[1] = (uint8_t)(2u + slot);
    for (i = 0; i < 5u; i++) {
        a[2 + i] = (uint8_t)((len >> (7u * i)) & 127u);
        a[7 + i] = (uint8_t)((st_crc32(src, len) >> (7u * i)) & 127u);
    }
    host_request(ED_BACKUP_PUT, a, 12);
    for (off = 0; off < len; off += 256u) {
        uint32_t n = len - off > 256u ? 256u : len - off, k = 7;
        const uint8_t *q = src + off;
        a[0] = 1;
        for (i = 0; i < 5u; i++)
            a[2 + i] = (uint8_t)((off >> (7u * i)) & 127u);
        while (n) {
            uint32_t g = n > 7u ? 7u : n, m = 0;
            for (i = 0; i < g; i++)
                m |= (uint32_t)(q[i] >> 7) << i;
            a[k++] = (uint8_t)m;
            for (i = 0; i < g; i++)
                a[k++] = q[i] & 127u;
            q += g;
            n -= g;
        }
        host_request(ED_BACKUP_PUT, a, k);
    }
    a[0] = 2;
    host_request(ED_BACKUP_PUT, a, 2);
    return host_wire_n > 7u ? host_wire[7] : 0xFFu;
}

static void t_song_ownership(void)
{
    project_store_t mine;
    persist_t p;
    host_start_easy = 1;
    host_power_on();
    host_frame();
    host_wait_ms(3100);
    trk[0].step[0].note[0] = 61;
    trk[0].step[0].n = 1;
    trk[0].step[0].time = ST_NOTE;
    easy_save();
    CHECK(gn.saved_mine == 1u && gn_song_owned() == 0u, "SAVE: slot 1, Easy's, SAFE TO UNPLUG");
    CHECK(flash_record(&p) && p.favorites.factory[15][29] == 0x90u, "..Easy's record of it on flash (round 27: 0x80 | bit 4 | slot)");
    memcpy(&mine, &proj_slot[0], sizeof mine);
    power_cycle();
    host_frame();
    CHECK(gn.on && gn_song_owned() == 0u, "power cycle: the slot still Easy's");
    CHECK(backup_put_slot(0, &mine) == 0u && !memcmp(&proj_slot[0], &mine, sizeof mine), "(the app restores the very same bytes)");
    CHECK(gn_song_owned() == GN_NO_SLOT, "..the same bytes written again by someone else: no longer Easy's");
    easy_save();
    CHECK(gn_song_owned() == 1u && !memcmp(&proj_slot[0], &mine, sizeof mine), "..SAVE: a new slot (2), slot 1 as restored");
    host_wait_ms(GN_SAVED_MS + 100u);
    /* Advanced writes Easy's slot 2 (the editor's save there), then Easy's bytes come back: still not Easy's */
    memcpy(&mine, &proj_slot[1], sizeof mine);
    hold_switch();
    CHECK(!gn.on, "(Advanced)");
    {
        uint8_t a[2] = {1, 1};
        host_request(ED_PROJECT, a, 2);
    }
    CHECK(backup_put_slot(1, &mine) == 0u, "(..and its old bytes restored over it)");
    hold_switch();
    CHECK(gn.on && gn_song_owned() == GN_NO_SLOT, "a save in Advanced, then the old bytes back: not Easy's (no resurrection)");
    host_wait_ms(GN_SAVED_MS + 100u);
    /* the settings cannot be written: the song saved, a plain warning, nothing claimed */
    host_fail_settings = 1;
    easy_save();
    CHECK(gn.saved_mine == 4u && gn_song_owned() == GN_NO_SLOT && project_used(2), "settings not writable: saved, the warning, no claim");
    CHECK(gn_view() == GV_SAVED, "..(the warning shown)");
    host_fail_settings = 0;
    host_wait_ms(GN_SAVED_MS + 100u);
    power_cycle();
    host_frame();
    host_wait_ms(1200);
    CHECK(gn_song_owned() == GN_NO_SLOT, "..a power cycle: slot 3 occupied, not Easy's");
    easy_save();
    CHECK(gn.saved_mine == 1u && gn_song_owned() == 3u, "..SAVE: the last empty slot (4)");
    host_wait_ms(GN_SAVED_MS + 100u);
    easy_save();
    CHECK(gn.saved_mine == 1u && gn_song_owned() == 3u, "..and again into slot 4, Easy's");
    (void)first_note_slot;
}

/* round 27 (F15, a report on V1.5): power-on restores the last save, whichever mode made it, in that mode; a mode switch
 * alone changes nothing of it; the start writes nothing (the mode byte follows the record in RAM only) */
static void t_r27_last_save(void)
{
    persist_t p, before, after;
    uint32_t easy_slot;
    host_start_easy = 1;
    host_power_on();
    host_frame();
    host_wait_ms(3000);
    trk[0].step[0].note[0] = 61;
    trk[0].step[0].n = 1;
    trk[0].step[0].time = ST_NOTE;
    easy_save();
    CHECK(gn.saved_mine == 1u && gn_song_owned() == 0u, "(Easy's SAVE: slot A)");
    host_wait_ms(GN_SAVED_MS + 100u);
    hold_switch();
    CHECK(!gn.on, "(Advanced)");
    trk[2].step[4].note[0] = 48;
    trk[2].step[4].n = 1;
    trk[2].step[4].time = ST_NOTE;
    name_open(NK_PROJ_SAVE, 1);                   /* Felucca's SAVE into slot B (ui_name.c name_ok) */
    name_ok();
    host_wait_ms(100);
    CHECK(flash_record(&p) && (p.favorites.factory[15][29] & 0xF3u) == 0xF1u,
          "Advanced's SAVE: the record on flash: slot B, round 27's, saved in Advanced, not Easy's own");
    CHECK(gn_song_owned() == GN_NO_SLOT, "..Easy owns no slot now (the record moved on)");
    hold_switch();
    CHECK(gn.on, "(left in Easy)");
    settings_poll();
    CHECK(flash_record(&before) && (before.favorites.factory[15][30] & GN_MODE_EASY), "(the mode on flash: Easy)");
    power_cycle();
    host_frame();
    host_wait_ms(1000);
    CHECK(!gn.on && proj_cur == 1u && trk[2].step[4].note[0] == 48u && !song.playing,
          "power cycle: Advanced (the last save's mode), slot B loaded, stopped");
    CHECK(flash_record(&after) && !memcmp(&before, &after, sizeof before), "..nothing written at start-up");
    power_cycle();
    host_frame();
    host_wait_ms(500);
    CHECK(!gn.on && proj_cur == 1u, "again: the same");
    /* Easy saves last, the synth left in Advanced: Easy */
    hold_switch();
    trk[0].step[2].note[0] = 64;
    trk[0].step[2].n = 1;
    trk[0].step[2].time = ST_NOTE;
    easy_save();
    easy_slot = gn_song_owned();
    CHECK(gn.saved_mine == 1u && easy_slot == 2u, "Easy's SAVE: neither A (Easy's older song) nor B (Advanced's): slot C");
    host_wait_ms(GN_SAVED_MS + 100u);
    hold_switch();
    CHECK(!gn.on, "(left in Advanced)");
    power_cycle();
    host_frame();
    host_wait_ms(1000);
    CHECK(gn.on && proj_cur == easy_slot && gn_song_owned() == easy_slot && trk[0].step[2].note[0] == 64u,
          "power cycle: Easy (the last save's mode), its song loaded");
    host_start_easy = 0;
}

static const struct {
    const char *name;
    void (*run)(void);
} CASES[] = {
    {"start (round 27): the last save picks the mode and the project; nothing written at start-up", t_r27_last_save},
    {"round 18: Felucca 1.0.1 migration changes exactly palette and mode", t_r18_felucca101_migration},
    {"round 18: Gnomad-written user palette survives later Easy and Advanced boots", t_r18_gnomad_palette_kept},
    {"start: no record: Easy at the first frame, GNOMAD, nothing written", t_first_power_on},
    {"start: a record of unmodified 1.0.1: Easy + GNOMAD the first time, nothing written; kept after", t_upstream_record},
    {"start: the mode survives power cycles both ways; written while stopped, after the stop if playing", t_power_cycles},
    {"start: 1.0.1's import, save and backup restore accept our record and ignore the byte", t_upstream_accepts},
    {"start: the song's slot: Easy's own last write only (sequence, payload); never inferred; warned (round 9)", t_song_ownership},
    {"start: settings restored under the running synth: the mode it runs, the marker, no start-up write (round 8)", t_restore_reconcile},
    {"start: the editor's UI save after mode switches (stopped, playing) answers 0, never a false 3", t_editor_ui_save},
};

int main(int argc, char **argv)
{
    uint32_t i, failed = 0;
    for (i = 0; i < sizeof CASES / sizeof CASES[0]; i++) {
        int st = 0;
        pid_t pid;
        if (argc > 1 && !strstr(CASES[i].name, argv[1]))
            continue;
        fflush(stdout);
        pid = fork();
        if (pid == 0) {
            CASES[i].run();
            fflush(stdout);
            _exit(fails ? 1 : 0);
        }
        if (pid < 0 || waitpid(pid, &st, 0) != pid || !WIFEXITED(st) || WEXITSTATUS(st)) {
            printf("%-96s FAILED\n", CASES[i].name);
            failed++;
        } else {
            printf("%-96s ok\n", CASES[i].name);
        }
    }
    printf(failed ? "gn start tests FAILED\n" : "gn start tests passed\n");
    return failed ? 1 : 0;
}
