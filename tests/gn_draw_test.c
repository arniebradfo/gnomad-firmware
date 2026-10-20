/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kyle Palaniuk */
/* Gnomad (FORK.md) host test: every Easy screen drawn whole, for every picture and through the states
 * each screen shows (the bubbles at every size and resting, the Gnome's mouths and the floating note,
 * the start-up's every frame, the loop's steps, the pads, the hold's ring, the volumes, the tap tempo,
 * saved with each routing and the panel's three), and the type's every glyph half off each edge of the screen. No audio runs, so
 * only the drawing code is exercised. tests/run_fork_tests.sh builds it with -fsanitize=undefined
 * (and -fno-sanitize-recover): a left shift of a negative value, an overflowing signed product, an
 * index out of its array or a misaligned access in the drawing code stops it (the review's finding 8).
 * Also: the scratch Easy keeps in the canvas's tail never reaches past it.
 *   tests/run_fork_tests.sh builds it: build/host-gn/gn_draw_test */
#include "gn_host.h"
#ifdef __APPLE__
#include <libproc.h>
#include <sys/resource.h>
static uint64_t insns(void)                       /* instructions retired by this process (macOS) */
{
    struct rusage_info_v4 ri;
    return proc_pid_rusage(getpid(), RUSAGE_INFO_V4, (rusage_info_t *)&ri) ? 0 : ri.ri_instructions;
}
#else
static uint64_t insns(void) { return 0; }
#endif

static uint32_t n_drawn;

static void whole(void (*scene)(void))
{
    int32_t y;
    for (y = 0; y < 240; y += (int32_t)GX_MH) {
        gx_begin(0, y, 240, 64, GX_GROUND);
        scene();
        gx_end();
    }
    n_drawn++;
}

int main(void)
{
    static const char ALL[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ 0123456789 .,:+-=/()%#!?";
    static uint16_t tail[CV_MAX - GX_TAIL_END];
    uint32_t s, k, r;
    host_power_on();
    memcpy(tail, &cv_px[GX_TAIL_END], sizeof tail);
    for (k = 0; k < GV_NONE; k++)                    /* every view has its scene and its parts */
        if (!GS_SCENE[k] || !GS_PARTS[k]) {
            printf("gn draw tests: view %u has no scene or no parts\n", k);
            return 1;
        }
    for (s = 0; s <= GN_OTHER; s++) {               /* play and pick: every picture and another sound */
        memset(&gs, 0, sizeof gs);
        gs.pic = (uint8_t)s;
        gs.field = (uint8_t)gs_pic_field(s);
        gs.ink = s >= GN_OTHER || gs.field >= 2u;
        gs.idle = s >= GN_OTHER;
        str_cpy(gs.name, s & 1u ? "E.PIANO" : "WWWWWWWWWWWW", sizeof gs.name);
        for (k = 0; k < 4u; k++)
            gs.r[k] = (uint8_t)(k * 80u);
        for (gs.vowel = 0; gs.vowel <= 5u; gs.vowel++) {
            gs.note = (uint8_t)(gs.vowel * 3u);
            gs.play = (uint8_t)(gs.vowel % 3u);
            gs.bpm = (int16_t)(40 + gs.vowel * 40u);
            gs.clock = (uint8_t)(gs.vowel & 1u);
            gs.trk = (uint8_t)(gs.vowel % 4u);
            str_cpy(gs.head, gs.vowel & 1u ? "WWWWWWWWWWWWWWWWWWWWWWW" : "BELLS * GLASS BELL", sizeof gs.head);
            gs.linked = (uint8_t)(gs.vowel & 1u);
            whole(scene_play);
        }
        for (gs.note = 0; gs.note <= 16u; gs.note++)
            whole(scene_play);
        whole(scene_pick);
        if (s < GN_NPICS) {                         /* the peek list: each sound of the picture the current one */
            uint32_t i;
            for (i = GN_GROUP[s]; i < GN_GROUP[s + 1u]; i++) {
                gs.snd = (uint8_t)i;
                whole(scene_peek);
            }
        }
#if GN_DEMO_GROOVES
        {                                           /* the grooves' list: none picked, each, SAVE FIRST */
            uint32_t i;
            for (i = 0; i <= GN_NGROOVES; i++) {
                gs.grv = (uint8_t)(GN_GV_LIST | i);
                whole(scene_peek);
                gs.grv |= GN_GV_NO;
                whole(scene_peek);
            }
            gs.grv = 0;
        }
#endif
    }
    memset(&gs, 0, sizeof gs);
    for (gs.boot = 0; gs.boot <= 3600u; gs.boot = (uint16_t)(gs.boot + 33u))
        whole(scene_boot);
    for (r = 0; r < 4u; r++)                         /* the loop (round UX-2): playhead, places, 1 to 8 bars */
        for (gs.step = 0; gs.step < 17u; gs.step++) {
            uint32_t i;
            for (i = 0; i < 8u; i++)
                gs.occ[i] = (uint16_t)(r == 3u ? 0xFFFFu : 0x1111u << ((gs.step + r + i) % 4u));
            gs.bars = (uint8_t)(1u << ((gs.step + r) % 4u));
            gs.bar = (uint8_t)(gs.step % gs.bars);
            gs.playing = (uint8_t)(gs.step & 1u);
            gs.rec = (uint8_t)(r == 2u ? 3u : 0u);
            gs.grid = (uint8_t)(r != 1u);
            gs.blink = (uint8_t)(gs.step & 1u);
            if (gs.step == 16u)
                gs.step = 0xFFu;
            whole(scene_loop);
            if (gs.step == 0xFFu)
                break;
        }
    memset(&gs, 0, sizeof gs);
    for (k = 0; k < 3u * 2u * 2u * 3u; k++) {         /* the loop pages: each grid, drums or not, playing or not */
        static const uint8_t SPB[3] = {8, 16, 32};
        uint32_t i;
        memset(&gs, 0, sizeof gs);
        gs.grid = (uint8_t)(k % 7u != 6u);
        gs.spb = SPB[k % 3u];
        gs.drums = (uint8_t)((k / 3u) & 1u);
        gs.playing = (uint8_t)((k / 6u) & 1u);
        gs.rec = (uint8_t)(k % 4u);
        gs.punch = (uint8_t)(k & 1u);
        gs.replace = (uint8_t)((k >> 1) & 1u);
        gs.bars = (uint8_t)(1u << (k % 4u));
        gs.bar = (uint8_t)(k % (gs.bars ? gs.bars : 1u));
        gs.col = gs.playing ? (uint8_t)(k % gs.spb) : 0xFFu;
        gs.cur = (uint8_t)((k * 5u) % gs.spb);
        gs.swing = (uint8_t)(k * 5u % 65u);
        gs.vol = (uint8_t)(k * 9u % 101u);
        gs.lanes = (uint8_t)(k * 37u);
        gs.trk = (uint8_t)(k % 4u);
        gs.bpm = 120;
        for (i = 0; i < 32u; i++) {
            gs.pitch[i] = (uint8_t)(i % 3u ? 40u + (i * 7u + k) % 50u : i % 2u);
            gs.lane[i] = (uint8_t)(i * 29u + k);
        }
        for (i = 0; i < 8u; i++)
            gs.occ[i] = (uint16_t)(0x1111u << (i % 4u));
        gs.cells = (uint8_t)(gs.spb > 16u ? 16u : gs.spb);   /* round UX-2: the steps' row, the drum list */
        gs.ccol = gs.playing ? (uint8_t)(k % gs.cells) : 0xFFu;
        for (i = 0; i < 16u; i++)
            gs.cnote[i] = (uint8_t)(i % 3u ? 30u + (i * 11u + k) % 70u : i % 2u);
        gs.ndrum = (uint8_t)(k % 3u == 0u ? 8u : 16u);
        gs.drum = (uint8_t)(k % gs.ndrum);
        gs.dbank = (uint8_t)(gs.drum / 8u);
        gs.dname = k & 1u ? "CLOSED HAT" : "TAMBOURINE";
        for (i = 0; i < 8u; i++) {
            gs.drn[i] = i & 1u ? "HIGH WOOD" : "KICK";
            gs.dnote[i] = (uint8_t)(35u + i * 3u);
            gs.drow[i] = (uint16_t)(0x5555u << (i & 1u));
        }
        gs.curnote = (uint8_t)(k & 1u);
        gs.lnote = (uint8_t)(48u + k);
        gs.shift = (uint8_t)((k >> 2) & 1u);          /* round UX-4: SHIFT, the cursor's place and step */
        gs.ccur = (uint8_t)(k % 5u ? k % gs.cells : 0xFFu);
        gs.son = (uint8_t)(k & 1u);
        gs.snote = (uint8_t)(36u + k);
        gs.svel = (uint8_t)(k * 9u % 128u);
        gs.role[2] = (uint8_t)(gs.shift ? KR_VEL : KR_LOOP);
        gs.role[0] = (uint8_t)(k & 1u ? KR_BAR : KR_MOVE);
        gs.role[1] = (uint8_t)(k & 2u ? KR_DRUM : KR_MOVE);
        gs.view = GV_LOOP;
        whole(scene_loop);
        gs.view = GV_STEPS;
        whole(scene_steps);
        gs.view = GV_RECORD;
        whole(scene_record);
        gs.view = GV_COUNT;
        gs.count = (uint8_t)(1u + k % 4u);
        whole(scene_count);
    }
    memset(&gs, 0, sizeof gs);
    for (k = 0; k < 2u * 18u; k++) {                 /* the kit (round UX-3): each pad lit, none, all; A, B, 8 pads */
        uint32_t i;
        gs.pads = (uint16_t)(k % 18u < 16u ? 1u << (k % 18u) : k % 18u == 16u ? 0u : 0xFFFFu);
        gs.kn = (uint8_t)(k % 3u ? 16u : 8u);
        gs.bank = (uint8_t)(gs.kn > 8u && k >= 18u);
        for (i = 0; i < 8u; i++)
            gs.kname[i] = i & 1u ? "TAMBOURINE" : "CLOSED HAT";
        whole(scene_drums);
    }
    memset(&gs, 0, sizeof gs);
    for (k = 0; k < 4u * 10u * 2u; k++) {            /* round FLOW-1, M2: GROOVES's tiles: each loaded, queued, asking */
        gs.tile = (uint8_t)(k % 10u);
        gs.tile_q = (uint8_t)((k / 10u) % 4u ? (k * 7u) % 10u : 0u);
        gs.tile_ask = (uint8_t)(k & 1u ? (k & 2u ? 0x40u | (k % 10u) : k % 10u) : 0u);
        gs.mine = (uint8_t)(k * 5u);
        gs.playing = (uint8_t)(k & 4u);
        gs.prog = (uint8_t)(k * 13u);
        gs.blink = (uint8_t)(k >= 40u);
        gs.trk = (uint8_t)(k % NTRK);
        whole(scene_tiles);
    }
    memset(&gs, 0, sizeof gs);
    for (k = 0; k <= GN_STEPS; k++) {                /* the sound: every amount of each knob, resting or not */
        uint32_t i;
        for (i = 0; i < 4u; i++)
            gs.amt[i] = (uint8_t)((k + i * 5u) % (GN_STEPS + 1u));
        gs.idle = (uint8_t)(k & 1u);
        gs.pic = (uint8_t)(k % (GN_OTHER + 1u));
        gs.field = (uint8_t)gs_pic_field(gs.pic);
        whole(scene_play);
    }
    for (gs.sweep = 0; gs.sweep <= 1000u; gs.sweep = (uint16_t)(gs.sweep + 25u)) {
        gs.to_easy = (uint8_t)(gs.sweep / 25u & 1u);
        whole(scene_switch);
    }
    for (k = 0; k < 2u * 128u; k++) {                 /* the volumes: every level on each track, each selected */
        uint32_t t;
        for (t = 0; t < NTRK; t++)
            gs.lvl[t] = (uint8_t)((k + t * 37u) % 128u);
        gs.trk = (uint8_t)(k % NTRK);
        gs.shift = (uint8_t)(k & 1u);                /* (round UX-4: SHIFT, pan; MUTE) */
        gs.mute = (uint8_t)(k * 7u);
        for (t = 0; t < NTRK; t++)
            gs.pan[t] = (int8_t)((int32_t)((k * 13u + t * 29u) % 128u) - 64);
        gs.view = GV_MIX;
        whole(scene_mix);
    }
    for (k = 0; k < 4u * 4u * 3u; k++) {             /* Home (round UX-1): levels, mute, sounding, armed, the hat */
        uint32_t t;
        memset(&gs, 0, sizeof gs);
        for (t = 0; t < NTRK; t++) {
            gs.lvl[t] = (uint8_t)((k * 29u + t * 41u) % 128u);
            str_cpy(gs.row[t], (k + t) & 1u ? "WWWWWWWW" : "GNOME", sizeof gs.row[t]);
        }
        gs.trk = (uint8_t)(k % NTRK);
        gs.mute = (uint8_t)(k * 5u);
        gs.live = (uint8_t)(k * 3u);
        gs.armed = (uint8_t)(k % 3u == 1u ? 1u << gs.trk : 0u);
        gs.rec = (uint8_t)(k % 3u == 2u ? 3u : k % 3u);
        gs.hat = (uint8_t)(k % 5u < 4u ? k % 5u : 0xFFu);
        gs.bpm = (int16_t)(40 + k * 4u);
        str_cpy(gs.head, "BARS 8 * SWING 60%", sizeof gs.head);
        gs.shift = (uint8_t)(k & 1u);
        gs.stored = (uint8_t)((k >> 1) & 1u);       /* (round UX-5: the strip's SAVED and ON SYNTH) */
        gs.play = (uint8_t)(k % 3u);
        gs.view = GV_HOME;
        whole(scene_home);
    }
    memset(&gs, 0, sizeof gs);
    for (k = 0; k < 2u * 6u; k++) {                  /* tap tempo: 40 .. 240, taps 0..4, a clock or not */
        gs.bpm = (int16_t)(40 + k * 18u);
        gs.taps = (uint8_t)(k % 5u);
        gs.clock = (uint8_t)(k >= 6u);
        whole(scene_tap);
    }
    gs.clock = 0;
    for (k = 1; k < 4u; k++) {                       /* the panel's SAVE: SAVED, FULL, NOT SAVED */
        gs.saved = (uint8_t)k;
        whole(scene_saved);
    }
    gs.saved = 0;
    for (k = 0; k < 2u * NTRK * 2u; k++) {           /* saved: both routings, each track selected, kits */
        song.g[G_ROUTE] = (int16_t)(k & 1u);
        song.sel = (uint8_t)((k >> 1) % NTRK);
        set_engine_of(&trk[3], k & 8u ? GE_DRUM : GE_ANALOG);
        gs.linked = (uint8_t)((k >> 2) & 1u);
        whole(scene_saved);
    }
    for (k = 0; k < 9u; k++) {                       /* the type, half off every edge */
        static const int32_t X[3] = {-120, 120, 360}, Y[3] = {-4, 30, 70};
        gx_begin(0, 0, 240, 64, GX_GROUND);
        gx_text(Q(X[k % 3u]), Q(Y[k / 3u]), &GS_NAME, 1, ALL, GX_INK);
        gx_text(Q(X[k % 3u]), Q(Y[k / 3u]), &GS_LABEL, (int)(k % 3u), ALL, GX_INK);
        gx_end();
        n_drawn++;
    }
    for (k = 0; k < 4u; k++) {                       /* strips of every height and width at the edges */
        static const int32_t B[4][4] = {{0, 0, 1, 1}, {239, 239, 1, 1}, {0, 176, 240, 64}, {100, 200, 140, 40}};
        gx_begin(B[k][0], B[k][1], B[k][2], B[k][3], GX_GROUND);
        scene_play();
        gx_end();
        n_drawn++;
    }
    {   /* the cost of each view drawn whole and of its dearest strip (host instructions; the target's
         * pi32v2 runs a similar count per strip, more for the 64-bit products) */
        void (*const *SC)(void) = GS_SCENE;          /* (gn_screens.c: every view's scene) */
        static const char *const NM[GV_NONE] = {"start-up", "play", "pick", "peek", "loop", "steps", "record", "drums",
                                                "volumes", "tap", "count-in", "saved", "switch", "home", "grooves"};
        uint32_t v;
        memset(&gs, 0, sizeof gs);
        gs.pic = 0;
        gs.field = 1;
        gs.boot = 1500;
        gs.vowel = 2;
        gs.note = 5;
        gs.sweep = 500;
        gs.occ[0] = 0x5555;
        gs.step = 3;
        gs.bars = 2;
        for (k = 0; k < 4u; k++)
            gs.r[k] = 200;
        song.sel = 0;
        if (insns())
            for (v = 0; v < GV_NONE; v++) {
                uint64_t all, worst = 0, t0 = insns(), t1;
                int32_t y;
                for (y = 0; y < 240; y += 64) {
                    t1 = insns();
                    gx_begin(0, y, 240, 64, GX_GROUND);
                    SC[v]();
                    gx_end();
                    t1 = insns() - t1;
                    worst = t1 > worst ? t1 : worst;
                }
                all = insns() - t0;
                printf("  %-9s whole %6.2f M instructions, its dearest strip (240 x 64) %5.2f M\n", NM[v], all / 1e6,
                       worst / 1e6);
            }
    }
    if (memcmp(tail, &cv_px[GX_TAIL_END], sizeof tail)) {
        printf("gn draw tests: the canvas past Easy's scratch was written\n");
        return 1;
    }
    printf("%u screens and strips drawn, the canvas's last %u bytes untouched: gn draw tests passed\n", n_drawn,
           (unsigned)sizeof tail);
    return 0;
}
