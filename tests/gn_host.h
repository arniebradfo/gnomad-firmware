/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kyle Palaniuk */
/* Gnomad (FORK.md): the firmware on the host, for the fork's tests of Easy mode. Upstream's own host
 * harness does the sound and the sequencer (tests/hostsim.c: the engines, voices, effects, USB and
 * TRS MIDI in, the sequencer, as the synth's one compilation unit has them); this adds what Easy
 * needs around it, in felucca.c's order: the UI (gfx.c to ui_layer.c, as tests/ui_test.c builds
 * it), storage on a RAM NOR flash (as tests/persistence_test.c), user presets, projects, the editor
 * (editor.c with its USB SysEx path), and the fork's code (gn_hooks.h first, gn_fork.c last). Only
 * the hardware layer is the host's:
 *   time      a microsecond clock the harness moves (fm1_ticks: 1 tick a microsecond; fm1_ms follows)
 *   input     the key, button and knob state of hal/fm1_input.h, set by host_key, host_btn, host_turn
 *   audio     host_frame renders the audio ISR's share of a 15 ms main-loop frame in blocks of CTL
 *             (fx.c mix_block, then the metronome's gn_click, as audio.c audio_block), optionally into a
 *             capture buffer (and the USB audio's, before the click), then runs the
 *             main loop's body: ui_input, settings_poll, ui_leds, ui_draw, ed_service
 *   screen    lcd_fill / lcd_blit write host_screen (240 x 240, the canvas's byte-swapped RGB565) and
 *             count the pixels sent (at 12 MHz SPI a pixel takes 1.33 us)
 *   LEDs      fm1_led / fm1_led_dim as ui_leds leaves them, with the synth's key map (led_pos)
 * Build from the repo root, with the generated headers of a CI run in build/gen:
 *   cc -O1 -w -Ibuild/gen -Ifirmware/src -Itests -o build/host-gn/gn_easy_test tests/gn_easy_test.c -lm */
#pragma once
#include <stddef.h>
#include <stdint.h>
static uint32_t host_slots[3u * 0x14000u / 4u];          /* USR1..3 (zero: empty), as the flash at 0xA0000 */
#define SMP_USER_XIP(k) ((const uint8_t *)host_slots + (k) * SMP_USER_SIZE)
#define FELUCCA_OTA 1
#define FELUCCA_VERSION "TEST"
/* the Advanced skin where the tree has it (gn/gnomad-1.0: felucca.c builds it, GSK_BUILD 1), so Easy is
 * tested with it in, as the firmware has them together; a tree without it: upstream's eight palettes */
#if !defined(GSK_BUILD) && defined(__has_include)
#if __has_include("../firmware/src/gn_skin.h")
#define GSK_BUILD 1
#endif
#endif
#ifndef GSK_BUILD
#define GSK_BUILD 0
#endif
/* the start in Easy (gn_hooks.h GN_START_EASY, 1 in the firmware): per case here, 0 unless a case sets it
 * before its power-on, so a case that enters Easy by the hold starts in Advanced as before */
static int host_start_easy;
#define GN_START_EASY host_start_easy
#include "../firmware/src/gn_hooks.h"                   /* the fork's flag and hook declarations: before seq.c */
#define main hostsim_main
#include "hostsim.c"
#undef main

/* ------------------------------------------------------------ HAL --- */
#define FM1_NCOL 11u
static const int8_t FM1_KEYMAP[6][FM1_NCOL] = {         /* hal/fm1_input.h */
    {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {5, 11, 4, 10, 3, 9, 2, 8, -1, -1, -1},
    {34, 35, 36, 37, 38, 40, 39, 13, 7, 6, 12},
    {23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33},
    {0, 1, 15, 14, 17, 16, 19, 18, 20, 21, 22},
    {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
};
static uint8_t fm1_led[FM1_NCOL], fm1_led_dim[FM1_NCOL];
#define FM1_TICKS_PER_US 1u
static uint32_t host_ticks, host_pressed, host_notes_pressed;
static uint64_t host_us;                                  /* since power-on */
static int32_t host_enc[7];
static uint32_t fm1_ticks(void) { return host_ticks; }
static uint32_t fm1_input_edges(int x)
{
    uint32_t p = host_pressed;
    (void)x;
    host_pressed = 0;
    return p;
}
static uint32_t fm1_input_note_edges(void)
{
    uint32_t n = host_notes_pressed;
    host_notes_pressed = 0;
    return n;
}
static int32_t fm1_enc_take(uint32_t e)
{
    int32_t s = host_enc[e % 7u];
    host_enc[e % 7u] = 0;
    return s;
}
static void host_audio_us(uint64_t us);
static void fm1_wdt_feed(void) { host_audio_us(1000u); }   /* (a main-loop wait: the audio ISR runs meanwhile) */
static uint32_t host_irq_off;                             /* fm1_irq_off without its fm1_irq_on: a bug */
static void fm1_irq_off(void) { host_irq_off++; }
static void fm1_irq_on(void) { host_irq_off--; }

/* the panel: host_screen as the ST7789 would hold it, and what was sent */
static uint16_t host_screen[240 * 240];
static uint64_t host_lcd_px;                              /* pixels sent since power-on */
static uint32_t host_lcd_blits;
static void lcd_fill(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint16_t c)
{
    uint32_t i, j;
#ifdef UI_FILL_HOOK
    UI_FILL_HOOK(x, y, w, h);
#endif
    for (j = 0; j < h && y + j < 240u; j++)
        for (i = 0; i < w && x + i < 240u; i++)
            host_screen[(y + j) * 240u + x + i] = (uint16_t)((c >> 8) | (c << 8));
    host_lcd_px += (uint64_t)w * h;
}
static void lcd_sync(void) {}
#define SCOPE_N 512u                                      /* audio.c: the HOME oscilloscope */
static int16_t scope_buf[SCOPE_N];
static uint32_t scope_w;
static void lcd_blit(uint32_t x, uint32_t y, uint32_t w, uint32_t h, const uint16_t *p)
{
    uint32_t i, j;
    for (j = 0; j < h && y + j < 240u; j++)
        for (i = 0; i < w && x + i < 240u; i++)
            host_screen[(y + j) * 240u + x + i] = p[j * w + i];
    host_lcd_px += (uint64_t)w * h;
    host_lcd_blits++;
}
static struct { uint32_t stage; } felucca_dbg;

/* the editor's SysEx path (tests/editor_test.c) */
static uint8_t host_wire[4096];
static uint32_t host_wire_n;
static void host_drain(void)
{
    while (so_r != so_w) {
        uint32_t p = sx_out_q[so_r++ % SXQ], cin = p & 15u, i;
        uint32_t n = cin == 4u || cin == 7u ? 3u : cin == 6u ? 2u : 1u;
        for (i = 0; i < n && host_wire_n < sizeof host_wire; i++)
            host_wire[host_wire_n++] = (uint8_t)(p >> (8u * (i + 1u)));
    }
}
static uint32_t ota_now_ms(void) { return fm1_ms; }
static void ota_idle(void) { host_drain(); }

#define FELUCCA_FLASH 1
#if GSK_BUILD
#include "../firmware/src/gn_skin.h"                    /* (felucca.c: before gfx.c) */
#endif
#include "../firmware/src/gfx.c"
#include "../firmware/src/panel.c"
#include "../firmware/src/ui.c"
#include "../firmware/src/icons.c"
#include "../firmware/src/ui_graph.c"
#include "../firmware/src/ui_draw.c"
#include "../firmware/src/ui_menu.c"
#include "../firmware/src/ui_input.c"
#include "../firmware/src/ui_layer.c"
#if GSK_BUILD
#include "../firmware/src/gn_skin.c"                    /* (felucca.c: after the UI files) */
#endif

/* storage on a RAM NOR flash (tests/persistence_test.c) */
static uint8_t host_nor[0x100000], flash_ok = 1;
static int st_read(uint32_t off, void *dst, uint32_t n) { memcpy(dst, host_nor + off, n); return 0; }
static int host_fail_settings;                           /* the settings' sectors refuse to erase (a test) */
static int st_erase(uint32_t off)
{
    if (host_fail_settings && off >= 0xFC000u && off < 0xFE000u)
        return -1;
    memset(host_nor + off, 0xFF, 4096);
    return 0;
}
static int st_prog(uint32_t off, const void *src, uint32_t n)
{
    const uint8_t *s = src;
    uint32_t i;
    for (i = 0; i < n; i++)
        host_nor[off + i] &= s[i];
    return 0;
}
static uint32_t irq_save(void) { return 0; }
static void irq_restore(uint32_t f) { (void)f; }
static uint32_t fl_jedec_ram(void) { return 0; }
static void fl_plain_window_init(void) {}
#define FL_FAR(fn) (fn)
static void audio_silence(void) {}
static void fl_inval(uint32_t off, uint32_t n) { (void)off; (void)n; }
static int fl_erase4k(uint32_t off, uint32_t *took)
{
    *took = 0;
    return st_erase(off);
}
static int fl_write(uint32_t off, const void *p, uint32_t n) { return st_prog(off, p, n); }
#include "../firmware/src/storage.c"
#include "../firmware/src/upreset.c"
#include "../firmware/src/project.c"
#include "../firmware/src/editor.c"
#include "../firmware/src/gn_fork.c"                    /* the fork's code: after what it hooks into */

/* ----------------------------------------------------------- driving --- */
/* main.c felucca_init, and the host's own state cleared: a synth just switched on */
static void host_power_on(void)
{
    uint32_t i;
    memset(trk, 0, sizeof trk);
    memset(&song, 0, sizeof song);
    memset(&chain, 0, sizeof chain);
    memset(&ui, 0, sizeof ui);
    memset(&nm, 0, sizeof nm);
    memset(&undo, 0, sizeof undo);
    memset(&gn, 0, sizeof gn);                            /* (.bss at power-on: Easy off, owning nothing) */
    memset(pat_last, 0, sizeof pat_last);
    memset(proj_slot, 0, sizeof proj_slot);
    memset(up_bank, 0, sizeof up_bank);
    memset(host_nor, 0xFF, sizeof host_nor);
    memset(&favorites, 0, sizeof favorites);
    memset(&settings, 0, sizeof settings);
    settings_init();
    proj_name[0] = 0;
    proj_cur = PROJ_NO_SLOT;
    panel = PANEL_DEFAULT;
    host_tracks_init();
    chain_defaults(&chain_config);
    for (i = 0; i < G_COUNT; i++)
        song.g[i] = GP[i].def;
    undo_depth++;
    fm6_init();
    for (i = 0; i < NTRK; i++) {
        track_t *t = &trk[i];
        track_defaults(t);
        set_engine_of(t, TRK_DEF[i][0]);
        apply_preset_to(t, TRK_DEF[i][1]);
        t->engine = t->eng_req;
        track_defaults_steps(t);
        if (TRK_DEF[i][2])
            load_pat16(t, PATTERNS[TRK_DEF[i][2] - 1u].note, PATTERNS[TRK_DEF[i][2] - 1u].flags);
        pat_sig[i] = steps_sig(t);
        pat_last[i] = TRK_DEF[i][2];
    }
    undo_depth--;
    song.sel = 0;
    song.master_q12 = 2048;
    ui.home = 1;
    ui.force = 1;
    palette_set(settings.palette);
    transport_req = 0;
    panic_req = 0;
    fm1_in.buttons = fm1_in.notes = 0;
    lcd_fill(0, 0, 240, 240, T_BG);
}

static int32_t *host_cap;                                 /* audio capture: the left channel (Q15), or 0 */
static uint32_t host_cap_n, host_cap_max;
static int32_t *host_usb_cap;                             /* .. and the USB audio's (audio.c uac_tap: before the */
static uint32_t host_usb_n;                               /* metronome's click, round 26), or 0; host_cap_max */

/* the audio ISR for us microseconds (blocks of CTL samples), the clocks with it; also what a main-loop
 * wait (fm1_wdt_feed: the editor's wait for the transport to stop, say) lets it do */
static void host_audio_us(uint64_t us)
{
    static double owed;
    owed += (double)us * FS / 1e6;
    while (owed >= CTL) {
        int32_t out[2 * CTL];
        uint32_t i;
        owed -= CTL;
        mix_block(out, CTL);
        for (i = 0; host_usb_cap && i < CTL && host_usb_n < host_cap_max; i++)
            host_usb_cap[host_usb_n++] = out[2u * i];    /* (audio.c audio_block: uac_tap, then gn_click) */
        gn_click(out, CTL);
        for (i = 0; host_cap && i < CTL && host_cap_n < host_cap_max; i++)
            host_cap[host_cap_n++] = out[2u * i];
        host_us += (uint64_t)CTL * 1000000u / FS;
        host_ticks = (uint32_t)host_us;
        fm1_ms = (uint32_t)(host_us / 1000u);
    }
}

/* one main-loop frame: 15 ms of audio, then main.c's frame body */
static void host_frame(void)
{
    host_audio_us(15000u);
    ui_input();
    settings_poll();
    ui_leds();
    ui_draw();
    ed_service();
    host_drain();
}

static void host_wait_ms(uint32_t ms)
{
    uint32_t n = (ms + 14u) / 15u;
    while (n--)
        host_frame();
}

static void host_btn(uint32_t label, int down)
{
    uint32_t bit = 1u << panel.btn[label];
    if (down && !(fm1_in.buttons & bit))
        host_pressed |= bit;
    fm1_in.buttons = down ? fm1_in.buttons | bit : fm1_in.buttons & ~bit;
}

static void host_key(uint32_t k, int down)
{
    uint32_t bit = 1u << k;
    if (down && !(fm1_in.notes & bit))
        host_notes_pressed |= bit;
    fm1_in.notes = down ? fm1_in.notes | bit : fm1_in.notes & ~bit;
}

static void host_turn(uint32_t role, int32_t steps)
{
    host_enc[panel.enc[role]] += steps * panel.dir[role];
    host_frame();
}

/* round SEQ-1 (spec S-2): the tempo is EDIT held + SELECT, a beat a detent (SELECT alone turns the page ring) */
static void host_tempo(int32_t steps)
{
    host_btn(B_EDIT, 1);
    host_enc[panel.enc[EN_SELECT]] += steps * panel.dir[EN_SELECT];
    host_frame();
    host_btn(B_EDIT, 0);
    host_frame();
}

static void host_tap(uint32_t label)                      /* a tap: down a frame, up */
{
    host_btn(label, 1);
    host_frame();
    host_btn(label, 0);
    host_frame();
}

/* HOME + SAVE held ms, then let go: into Easy and out of it (2 s) */
static void host_hold_switch(uint32_t ms)
{
    host_btn(B_HOME, 1);
    host_btn(B_SAVE, 1);
    host_wait_ms(ms);
    host_btn(B_HOME, 0);
    host_btn(B_SAVE, 0);
    host_frame();
}

static void host_easy_gnome(void)                         /* power-on, then into Easy (round 13: the Gnome on */
{                                                         /* track 1), start-up over */
    host_power_on();
    host_frame();
    host_hold_switch(2100);
    host_key(7, 1);                                       /* (a key ends the start-up picture) */
    host_frame();
    host_key(7, 0);
    host_wait_ms(300);
}

/* the tests written before round 13 start from Easy with track 1 still on Felucca's own sound: the entry's Gnome
 * (round 14: shown, put there only when used) dropped before anything uses him */
static void host_easy(void)
{
    host_power_on();
    host_frame();
    host_hold_switch(2100);
    gn.entry_put = 0;
    gn.entry_keys = 0;
    host_key(7, 1);                                       /* (a key ends the start-up picture) */
    host_frame();
    host_key(7, 0);
    host_wait_ms(300);
}

/* the editor as the app drives it (tests/editor_test.c request) */
static uint32_t host_request(uint32_t cmd, const uint8_t *a, uint32_t n)
{
    uint32_t i;
    host_wire_n = 0;
    ota_frame_done();
    sysex_byte(0xF0);
    sysex_byte(ED_HDR0);
    sysex_byte(ED_HDR1);
    sysex_byte(ED_HDR2);
    sysex_byte((uint8_t)cmd);
    for (i = 0; i < n; i++)
        sysex_byte(a[i]);
    sysex_byte(0xF7);
    ed_service();
    host_drain();
    return host_wire_n;
}

static int host_led(uint32_t id)                          /* LED of key id lit (button 0..13, note key 14..40) */
{
    uint8_t q = led_pos[id];
    return q != 0xFF && ((fm1_led[q >> 3] >> (q & 7u)) & 1u);
}
static int host_led_dim(uint32_t id)
{
    uint8_t q = led_pos[id];
    return q != 0xFF && ((fm1_led_dim[q >> 3] >> (q & 7u)) & 1u);
}
