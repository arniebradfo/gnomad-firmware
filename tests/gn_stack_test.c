/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kyle Palaniuk */
/* Gnomad (FORK.md) host test: the stack the firmware's code takes on the host, with Easy and without it
 * (review 2's budgets: stack high-water as far as a harness can give it). Each walk runs on a thread of
 * its own whose stack is painted first; afterwards the deepest byte no longer the paint is the high-water.
 * Felucca's walk never enters Easy (its pages, its keys and knobs, the app's commands); Easy's walk is the
 * same in Easy, plus every picture, the loop and its undo, Play, the saved screen, the leaving and every
 * view drawn whole. Both run the same harness (tests/gn_host.h: the audio renders inside the same frames),
 * so the difference is what Easy adds; the host's ABI and compiler are not the synth's, so the numbers are
 * the host's (the target's estimate: .github/ci/gn_budget.py).
 *   tests/run_fork_tests.sh builds it: build/host-gn/gn_stack_test */
#include "gn_host.h"
#include <pthread.h>
#include <sys/mman.h>

#define STK (4u << 20)
static uint8_t *stk;
static uint32_t rnd_state = 4242u;
static uint32_t rnd(uint32_t n)
{
    rnd_state = rnd_state * 1103515245u + 12345u;
    return (rnd_state >> 8) % n;
}

static void ed_val14(uint8_t *b, int32_t v)
{
    b[0] = (uint8_t)((v + 8192) & 127);
    b[1] = (uint8_t)(((v + 8192) >> 7) & 127);
}

/* the app's commands, any of them: sounds, settings, steps, projects, user sounds, a backup put back */
static void app_any(void)
{
    uint8_t a[16] = {0};
    uint32_t c = rnd(8), e = rnd(NENGINES);
    switch (c) {
    case 0:
        a[0] = (uint8_t)e;
        a[1] = (uint8_t)rnd(ENGINES[e]->npresets ? ENGINES[e]->npresets : 1u);
        host_request(ED_PRESET, a, 2);
        break;
    case 1:
        a[1] = (uint8_t)rnd(P_COUNT);
        ed_val14(a + 2, (int32_t)rnd(200) - 60);
        host_request(ED_SET, a, 4);
        break;
    case 2: {
        uint8_t s[10] = {(uint8_t)rnd(NTRK), (uint8_t)rnd(NSTEP), 1, (uint8_t)(30u + rnd(60)), 0, 0, 0, ST_NOTE, 0, 100};
        host_request(ED_TRACK_STEP, s, 10);
        break;
    }
    case 3:
        a[0] = (uint8_t)rnd(2);
        a[1] = (uint8_t)rnd(4);
        host_request(ED_PROJECT, a, 2);
        break;
    case 4:
        a[0] = (uint8_t)rnd(16);
        host_request(ED_UP_STORE, a, 2);
        host_request(ED_UP_LOAD, a, 1);
        break;
    case 5:
        host_request(ED_BACKUP_LIST, 0, 0);
        host_request(ED_DUMP, 0, 0);
        break;
    case 6:
        a[0] = (uint8_t)rnd(NTRK);
        host_request(ED_TRACK, a, 1);
        break;
    default:
        host_request(ED_INFO, 0, 0);
        break;
    }
}

static void panel_any(int easy)
{
    static const uint32_t BTN[14] = {B_HOME, B_SAVE, B_REC, B_PLAY, B_OCTDN, B_OCTUP, B_FX, B_SEQ, B_EDIT, B_SCL, B_ENV,
                                     B_LFO, B_GLO, B_ARP};
    uint32_t a = rnd(100);
    if (a < 30u) {
        host_key(rnd(27), (int)rnd(2));
    } else if (a < 55u) {
        host_turn(EN_SELECT + rnd(NE), (int32_t)rnd(9) - 4);
    } else if (a < 75u) {
        uint32_t b = BTN[rnd(easy ? 14 : 9)];
        if (easy && b == B_HOME && rnd(2))
            b = B_REC;
        host_btn(b, 1);
        host_wait_ms(rnd(2) ? 15u : 100u + rnd(b == B_SAVE ? 500 : 1100));   /* (SAVE: no undo hold, FORK.md) */
        host_btn(b, 0);
    } else if (a < 90u) {
        app_any();
    }
    host_frame();
}

static void walk_felucca(void)
{
    uint32_t i;
    host_power_on();
    host_wait_ms(60);
    for (i = 0; i < 4000u; i++) {
        panel_any(0);
        if (gn.on)                                  /* (never: the chord is not in the walk) */
            break;
        if (i % 97u == 0u) {
            ui.force = 1;
            host_frame();
        }
    }
}

static void walk_easy(void)
{
    uint32_t i, v;
    host_start_easy = 1;                            /* (round 9: as the firmware starts: Easy, its records kept) */
    host_power_on();
    host_frame();
    host_wait_ms(3100);
    for (i = 0; i < GN_NSOUNDS; i++) {              /* round 7: every sound (ALGORITHM a picture, PRESETS a sound) */
        if (i && GN_SOUNDS[i].pic != GN_SOUNDS[i - 1u].pic)
            host_turn(EN_ALGO, 1);
        else if (i)
            host_turn(EN_PRESET, 1);
        else
            host_turn(EN_ALGO, 1);
        for (v = 0; v < 27u; v++)
            host_key(v, 1);
        host_wait_ms(60);
        for (v = 0; v < 27u; v++)
            host_key(v, 0);
        for (v = 0; v < 4u; v++)
            host_turn(EN_K1 + v, 9);
    }
    host_tap(B_REC);                                /* armed, the count-in, a take, undo on SAVE held */
    host_tap(B_PLAY);
    for (i = 0; i < 300u && !song.rec; i++)
        host_frame();
    host_key(7, 1);
    host_wait_ms(200);
    host_key(7, 0);
    host_turn(EN_K4, 1);                            /* (REPLACE) */
    host_wait_ms(600);
    host_tap(B_REC);
    host_btn(B_REC, 1);                             /* a punch, a key in its hold */
    host_key(9, 1);
    host_wait_ms(500);
    host_key(9, 0);
    host_btn(B_REC, 0);
    host_btn(B_SAVE, 1);
    host_wait_ms(800);
    host_btn(B_SAVE, 0);
    host_tap(B_PLAY);
    for (v = 0; v < 3u; v++) {                      /* the loop's pages: BARS, SNAP, step entry */
        host_tap(B_SEQ);
        host_turn(EN_K1, 1);
        host_turn(EN_K3, v & 1u ? 1 : -1);
        host_key(7, 1);
        host_wait_ms(40);
        host_key(7, 0);
    }
    host_tap(B_GLO);                                /* the volumes, tap tempo, a song saved */
    host_turn(EN_K2, -3);
    host_tap(B_EDIT);
    host_tap(B_EDIT);
    host_tap(B_SAVE);
    host_wait_ms(200);
    host_tap(B_HOME);
    {
        uint8_t a[2] = {1, 1};                      /* saved */
        host_request(ED_PROJECT, a, 2);
    }
    {   /* every view drawn whole, view v itself (its snapshot and its scene, a strip at a time) */
        void (*const *SC)(void) = GS_SCENE;          /* (gn_screens.c: every view's scene) */
        for (v = 0; v < GV_NONE; v++) {
            int32_t y;
            gs_snapshot(v);
            for (y = 0; y < 240; y += 32) {
                gx_begin(0, y, 240, 32, GX_GROUND);
                SC[v]();
                gx_end();
            }
        }
        gs_shown_view = GV_NONE;
        for (i = 0; i < 40u; i++)
            ui_draw();
    }
    for (i = 0; i < 4000u && gn.on; i++) {
        panel_any(1);
        if (!gn.on) {
            fm1_in.buttons = 0;
            host_hold_switch(2100);
        }
    }
    host_hold_switch(2100);                         /* the leaving */
    host_wait_ms(300);
}

static void *run(void *f)
{
    ((void (*)(void))f)();
    return 0;
}

static uint32_t high_water(void (*f)(void))
{
    pthread_attr_t at;
    pthread_t th;
    uint32_t i;
    memset(stk, 0xA5, STK);
    pthread_attr_init(&at);
    pthread_attr_setstack(&at, stk, STK);
    if (pthread_create(&th, &at, run, (void *)f) || pthread_join(th, 0)) {
        printf("gn stack: no thread\n");
        exit(1);
    }
    for (i = 0; i < STK && stk[i] == 0xA5u; i++)
        ;
    return STK - i;
}

int main(void)
{
    uint32_t felucca, easy;
    stk = mmap(0, STK, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
    if (stk == MAP_FAILED)
        return 1;
    felucca = high_water(walk_felucca);
    easy = high_water(walk_easy);
    printf("gn stack (host, cc -O2): Felucca's walk %u B, Easy's %u B: Easy adds %d B\n", felucca, easy,
           (int)easy - (int)felucca);
    if (easy > felucca + 2048u) {
        printf("gn stack: Easy takes over 2 KiB more than Felucca alone: FAILED\n");
        return 1;
    }
    printf("gn stack tests passed\n");
    return 0;
}
