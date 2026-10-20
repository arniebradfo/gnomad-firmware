/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kyle Palaniuk */
/* Gnomad (FORK.md) host program: the synth's screen without a synth. The firmware's own drawing code
 * (tests/gn_host.h: everything from the sound to the editor, the screen as the panel would hold it)
 * is driven as a person drives the panel, into each of Easy's screens, and each is written as a PNG
 * (240 x 240, and 2x/ at 480 x 480). Felucca's own HOME page comes first, to show the renderer
 * shows the truth. With a second folder, every picture must be the same bytes as the one approved
 * there (tests/gn_screens): a change of the drawing shows up as a difference.
 *   gn_screen OUT [APPROVED]          (tests/run_fork_tests.sh: build/gn_screens, tests/gn_screens) */
#ifdef GN_UX_ENGINES                                   /* (round UX-3: the engines build's screens, no approved copy) */
#define GN_ENGINES 1
#include "../firmware/src/gn_engines.h"
#endif
#include "gn_host.h"
#include "gn_png.h"
#include <sys/stat.h>

static const char *out_dir, *ok_dir;
static int fails, shots;

/* the screen as it would be once Easy has drawn all it wants to (time stands still meanwhile) */
static void settle(void)
{
    uint32_t i;
    for (i = 0; i < 200u; i++) {
        ui_draw();
        if (!gn.on || (!gs_job.on && !gs_px_call))
            break;
    }
}

static int write_file(const char *path, const uint8_t *b, size_t n)
{
    FILE *f = fopen(path, "wb");
    if (!f || fwrite(b, 1, n, f) != n) {
        fprintf(stderr, "gn_screen: cannot write %s\n", path);
        if (f)
            fclose(f);
        return 1;
    }
    return fclose(f) != 0;
}

static int same_as_approved(const char *name, const uint8_t *b, size_t n)
{
    char path[512];
    FILE *f;
    uint8_t *a;
    long m;
    int same;
    snprintf(path, sizeof path, "%s/%s.png", ok_dir, name);
    if (!(f = fopen(path, "rb")))
        return 0;
    fseek(f, 0, SEEK_END);
    m = ftell(f);
    fseek(f, 0, SEEK_SET);
    a = malloc((size_t)m + 1u);
    same = a && fread(a, 1, (size_t)m, f) == (size_t)m && (size_t)m == n && !memcmp(a, b, n);
    free(a);
    fclose(f);
    return same;
}

static void shot(const char *name)
{
    static uint8_t rgb[240 * 240 * 3], big[480 * 480 * 3];
    char path[512];
    uint32_t x, y;
    size_t n;
    uint8_t *png;
    settle();
    for (y = 0; y < 240u; y++)
        for (x = 0; x < 240u; x++) {
            uint16_t c = host_screen[y * 240u + x];
            c = (uint16_t)((c >> 8) | (c << 8));        /* the canvas holds its pixels byte-swapped */
            rgb[(y * 240u + x) * 3u] = (uint8_t)(((c >> 11) & 31u) * 255u / 31u);
            rgb[(y * 240u + x) * 3u + 1u] = (uint8_t)(((c >> 5) & 63u) * 255u / 63u);
            rgb[(y * 240u + x) * 3u + 2u] = (uint8_t)((c & 31u) * 255u / 31u);
        }
    for (y = 0; y < 480u; y++)
        for (x = 0; x < 480u; x++)
            memcpy(&big[(y * 480u + x) * 3u], &rgb[((y / 2u) * 240u + x / 2u) * 3u], 3);
    png = gn_png(rgb, 240, 240, &n);
    snprintf(path, sizeof path, "%s/%s.png", out_dir, name);
    fails += write_file(path, png, n);
    if (ok_dir && !same_as_approved(name, png, n)) {
        printf("    %-28s differs from %s/%s.png\n", name, ok_dir, name);
        fails++;
    }
    free(png);
    png = gn_png(big, 480, 480, &n);
    snprintf(path, sizeof path, "%s/2x/%s.png", out_dir, name);
    fails += write_file(path, png, n);
    free(png);
    shots++;
}

static void key_tap(uint32_t k, uint32_t ms)
{
    host_key(k, 1);
    host_wait_ms(ms);
    host_key(k, 0);
    host_frame();
}

static uint32_t gn_wkey(uint32_t c)               /* the white key of place c (0 .. 15) */
{
    uint32_t k;
    for (k = 0; k < 27u; k++)
        if (GN_WHITE[k] == c)
            return k;
    return 0;
}

static void pick(int32_t steps)                  /* ALGORITHM, then the pick screen over */
{
    host_turn(EN_ALGO, steps);
    host_wait_ms(100u);
    host_tap(B_HOME);                            /* (round FLOW-1, M2: the wheel stays till HOME) */
    host_wait_ms(GN_PICK_MS);
}

int main(int argc, char **argv)
{
    static const char *const SOUND[8] = {"gnome", "organ", "bells", "laser", "whale", "choir", "cloud", "piano"};
    char path[512];
    uint32_t i;
    if (argc < 2) {
        fprintf(stderr, "usage: gn_screen OUT [APPROVED]\n");
        return 2;
    }
    out_dir = argv[1];
    ok_dir = argc > 2 ? argv[2] : 0;
    mkdir(out_dir, 0755);
    snprintf(path, sizeof path, "%s/2x", out_dir);
    mkdir(path, 0755);

    /* Felucca's own HOME page at power-on, Easy off: the renderer shows the truth */
    host_power_on();
    host_wait_ms(100);
    shot("felucca-home");
    key_tap(7, 0);
    host_key(7, 1);
    host_wait_ms(60);
    shot("felucca-home-note");
    host_key(7, 0);
    host_wait_ms(300);

    /* into Easy: the start-up (3i) as it plays */
    host_btn(B_HOME, 1);
    host_btn(B_SAVE, 1);
    host_wait_ms(1000);
    shot("easy-3h-enter-held-felucca");          /* the hold that enters: Felucca keeps its screen */
    host_wait_ms(1100);
    host_btn(B_HOME, 0);
    host_btn(B_SAVE, 0);
    host_wait_ms(400);
    shot("easy-3i-start-1");
    host_wait_ms(800);
    shot("easy-3i-start-2");
    host_wait_ms(1300);
    shot("easy-3i-start-3");
    host_wait_ms(800);
    shot("easy-ux1-home");                       /* round UX-1: Easy opens on Home, the four tracks */
    host_tap(B_HOME);                            /* (HOME on Home: the selected track's sound) */
    host_tap(B_SCL);                             /* (round 13: Easy starts on the Gnome, track 1) */
    host_wait_ms(300);
    shot("easy-3b-other");                       /* track 2 holds Felucca's own sound: its engine's map */
    host_tap(B_FX);
    host_wait_ms(300);

    /* the eight sounds, each a picture (3c while ALGORITHM turns, then 3b) */
    host_turn(EN_ALGO, (int32_t)GN_NPICS);       /* (round and round, to the Gnome again) */
    host_wait_ms(200);
    shot("easy-3c-pick");
    host_tap(B_HOME);                            /* (round FLOW-1, M2: HOME closes the wheel) */
    host_wait_ms(GN_PICK_MS - 30u);
    shot("easy-9b-gnome");
    host_key(7, 1);                              /* the Gnome sings */
    host_wait_ms(250);
    shot("easy-9b-gnome-sings");
    host_key(7, 0);
    host_key(11, 1);
    host_wait_ms(400);
    shot("easy-9b-gnome-sings-2");
    host_key(11, 0);
    host_wait_ms(600);
    for (i = 1; i < 8u; i++) {
        char nm[40];
        pick(1);
        snprintf(nm, sizeof nm, "easy-3b-%s", SOUND[i]);
        shot(nm);
        if (i == 2u || i == 3u) {                /* the peek list (PRESETS, pick 3): Bells, Laser */
            host_turn(EN_PRESET, 2);
            host_wait_ms(200);
            snprintf(nm, sizeof nm, "easy-11-peek-%s", SOUND[i]);
            shot(nm);
            host_tap(B_HOME);                    /* (round FLOW-1, M2: HOME closes the list) */
            host_wait_ms(GN_PEEK_MS + 70u);
            snprintf(nm, sizeof nm, "easy-11-play-%s-3rd", SOUND[i]);
            shot(nm);
            host_turn(EN_PRESET, -2);
            host_tap(B_HOME);
            host_wait_ms(GN_PEEK_MS + 70u);
        }
    }
    for (i = 0; i < 4u; i++)                     /* the knobs: each bubble its own size */
        host_turn(EN_K1 + i, (int32_t)(i * 4u) - 6);
    host_wait_ms(200);
    shot("easy-3b-knobs");
    host_tap(B_REC);                             /* a melody recorded on the Piano, then its steps (pick 8) */
    host_tap(B_PLAY);
    while (!song.rec)
        host_frame();
    for (i = 0; i < 6u; i++)
        key_tap(i * 2u + 7u, 160);
    host_tap(B_REC);
    host_tap(B_PLAY);
    host_tap(B_SEQ);
    host_tap(B_SEQ);
    host_wait_ms(300);
    shot("easy-11-steps-melody");
    host_btn(B_EDIT, 1);                         /* round UX-4: SHIFT on the steps: a step's note and strength */
    host_wait_ms(GN_SHIFT_MS + 100u);
    host_turn(EN_K1, 2);
    host_wait_ms(100);
    shot("easy-ux4-steps-shift");
    host_btn(B_EDIT, 0);
    host_wait_ms(100);
    host_tap(B_HOME);
    host_wait_ms(300);
    shot("easy-ux1-home-melody");                /* Home: the melody's track, the others as they are */
    host_tap(B_HOME);                            /* (the sound page again) */
    host_wait_ms(300);

    /* the drums (the tenth picture, round CR: after FUNNY) and the pads */
    pick(2);
    shot("easy-3e-drums");
    host_key(9, 1);                              /* the sixth white key: pad 6, the clap */
    host_wait_ms(60);
    shot("easy-3e-drums-pad");
    host_key(9, 0);
    host_wait_ms(400);
    host_tap(B_OCTUP);                           /* round UX-3: the kit's bank B */
    host_wait_ms(100);
    shot("easy-ux3-kit-bank-b");
    host_tap(B_OCTDN);
    host_btn(B_EDIT, 1);                         /* SHIFT: round FLOW-1, M2: no deep page in Easy */
    host_wait_ms(GN_SHIFT_MS + 100u);
    shot("easy-flow1-shift-no-deep");
    host_btn(B_EDIT, 0);
    host_wait_ms(300);

    /* recording (pick 5): REC arms, PLAY counts in, then the drums record; the loop page (3d) */
    host_tap(B_REC);
    host_wait_ms(300);
    shot("easy-11-armed");
    host_tap(B_PLAY);
    host_wait_ms(700);
    shot("easy-11-count-in");
    while (!song.rec)
        host_frame();
    for (i = 0; i < 6u; i++)
        key_tap(i % 2u ? 7u : 0u, 120);
    host_turn(EN_K4, 1);                         /* K4 while recording: REPLACE */
    host_wait_ms(100);
    shot("easy-11-recording-replace");
    host_turn(EN_K4, -1);
    host_tap(B_SEQ);
    host_wait_ms(300);
    shot("easy-3d-loop");
    host_tap(B_REC);
    host_wait_ms(300);
    shot("easy-11-loop-cards");                  /* the circle: BARS SWING SNAP VOLUME (pick 7) */
    host_tap(B_SEQ);
    host_wait_ms(300);
    shot("easy-11-steps-drums");                 /* the steps: the drums one row per sound (pick 8) */
    host_tap(B_SEQ);
    host_btn(B_REC, 1);                          /* the record screen, playing: REC held punches in */
    host_wait_ms(600);
    shot("easy-11-record-punch");
    host_btn(B_REC, 0);
    host_tap(B_PLAY);
    host_wait_ms(300);
    shot("easy-11-record-stopped");              /* stopped: one note at a time */
    {                                            /* round 26: EDIT held + K3, the metronome: its card */
        uint8_t was = favorites.factory[15][30];  /* (the firmware's record: Gnomad's marker, ON) */
        favorites.factory[15][30] = GN_MODE_MARK | GN_MODE_EASY;
        host_btn(B_EDIT, 1);
        host_frame();
        host_turn(EN_K3, 1);
        shot("easy-26-record-click");
        host_btn(B_EDIT, 0);
        host_wait_ms(GN_TAP_MS + 100u);
        favorites.factory[15][30] = was;
    }
    host_tap(B_HOME);
    host_wait_ms(300);

    /* the volumes (GLO), tap tempo (EDIT), the panel's SAVE (round 7, pick 2) */
    host_tap(B_GLO);
    host_turn(EN_K2, -6);
    host_wait_ms(200);
    shot("easy-11-volumes");
    host_btn(B_EDIT, 1);                         /* round UX-4: SHIFT on the volumes: the pan */
    host_wait_ms(GN_SHIFT_MS + 100u);
    host_turn(EN_K3, 5);
    host_wait_ms(100);
    shot("easy-ux4-volumes-pan");
    host_btn(B_EDIT, 0);
    host_wait_ms(100);
    host_tap(B_HOME);
    host_tap(B_EDIT);
    host_wait_ms(470);
    host_tap(B_EDIT);
    host_wait_ms(470);
    host_tap(B_EDIT);
    host_wait_ms(100);
    shot("easy-11-tap");
    host_wait_ms(GN_TAP_MS + 100u);
    host_start_easy = 1;                         /* (the panel's SAVE keeps Easy's record: the firmware's start) */
    host_tap(B_SAVE);
    host_wait_ms(200);
    shot("easy-11-saved-gnome");
    host_wait_ms(GN_SAVED_MS + 100u);
    host_fail_settings = 1;                      /* (round 9: Easy's record of the slot cannot be written) */
    host_tap(B_SAVE);
    host_wait_ms(200);
    shot("easy-11-saved-not-kept");
    host_fail_settings = 0;
    host_wait_ms(GN_SAVED_MS + 100u);

    /* saved, safe to unplug (3g): the app saves a project */
    {
        uint8_t a[2] = {1, 0};
        host_request(ED_PROJECT, a, 2);
        host_wait_ms(100);
        shot("easy-3g-saved");
    }
    host_wait_ms(GN_SAVED_MS + 100u);

    /* leaving (3h): the ring fills as HOME + SAVE are held */
    host_btn(B_HOME, 1);
    host_btn(B_SAVE, 1);
    host_wait_ms(1000);
    shot("easy-3h-switch");
    host_wait_ms(1100);
    host_btn(B_HOME, 0);
    host_btn(B_SAVE, 0);
    host_wait_ms(200);
    shot("easy-3h-back-to-felucca");
#if GN_DEMO_GROOVES
    /* the demo grooves: back into Easy, the loop page, PRESETS; a step of the player's, PRESETS: SAVE FIRST */
    host_hold_switch(2100);
    host_wait_ms(GN_BOOT_MS + 100u);
    host_tap(B_SEQ);
    host_wait_ms(300);
    host_turn(EN_PRESET, 1);
    host_wait_ms(200);
    shot("easy-12-grooves");
    host_wait_ms(GN_PEEK_MS + 100u);
    trk[0].step[5].time = ST_NOTE;               /* (the app writes a step: the player's music now) */
    trk[0].step[5].n = 1;
    trk[0].step[5].note[0] = 70;
    trk[0].step[5].vel = 100;
    host_turn(EN_PRESET, 1);
    host_wait_ms(200);
    shot("easy-12-grooves-save-first");
#endif

    /* round SEQ-1 (phase 1, layout A): a beat on all four tracks made with the STEPS tap grid (SELECT to the page,
     * the white keys the bar's places), then the LOOP page with its one playhead at three places, STEPS on a kit and a
     * melody, a held step, the lit knob, the tempo on EDIT held + SELECT, and the ring's two pages still to come */
    host_start_easy = 0;
    host_easy();
    {
        static const int8_t PICK[4] = {2, 4, 3, -1};            /* organ, whale, bells; the kit (the last) */
        static const uint16_t TAPS[4] = {0x0911, 0x4141, 0x2222, 0};
        static const uint8_t DRUM[3][2] = {{0, 0x11}, {1, 0x44}, {3, 0xFF}};   /* kick, snare, closed hat: places (x2) */
        uint32_t t, c, d;
        for (t = 0; t < 4u; t++) {
            host_tap(B_FX + t);
            pick(PICK[t]);
            while (gn.page != GP_LOOP)
                host_turn(EN_SELECT, 1);
            host_turn(EN_K1, -3);                    /* BARS 1, the grid Easy's */
            host_turn(EN_SELECT, 1);                 /* STEPS */
            if (t < 3u) {
                host_turn(EN_K2, (int32_t)t * 2);    /* the pen note */
                for (c = 0; c < 16u; c++)
                    if ((TAPS[t] >> c) & 1u)
                        key_tap(gn_wkey(c), 40);
            } else
                for (d = 0; d < 3u; d++) {
                    while (gn.drum != DRUM[d][0])
                        host_turn(EN_K2, 1);
                    for (c = 0; c < 16u; c++)
                        if ((DRUM[d][1] >> (c / 2u)) & 1u && (d < 2u ? !(c & 1u) : 1))
                            key_tap(gn_wkey(c), 40);
                }
        }
        host_wait_ms(1400);
        shot("easy-seq1-steps-kit");                 /* track 4's kit: K2's drum (the closed hat) as blocks */
        host_turn(EN_SELECT, -1);
        host_wait_ms(1400);
        shot("easy-seq1-loop-stopped");              /* LOOP, layout A: four lanes, the kit's lane tall */
        host_tap(B_PLAY);
        host_wait_ms(130);
        shot("easy-seq1-loop-playhead-1");
        host_wait_ms(700);
        shot("easy-seq1-loop-playhead-2");
        host_tap(B_FX);                              /* track 1 tall now */
        host_wait_ms(800);
        shot("easy-seq1-loop-playhead-3");
        host_turn(EN_K2, 2);                         /* SWING turning lights its card and the off-beats */
        host_wait_ms(100);
        shot("easy-seq1-loop-swing-lit");
        host_turn(EN_K2, -2);
        host_wait_ms(1400);
        host_turn(EN_SELECT, 1);                     /* STEPS on the melody, playing */
        host_wait_ms(500);
        shot("easy-seq1-steps-melody");
        host_tap(B_PLAY);
        host_key(gn_wkey(4), 1);                     /* hold place 5 and turn STRENGTH and WHEN */
        host_frame();
        host_turn(EN_K2, -6);
        host_turn(EN_K4, 2);
        host_wait_ms(100);
        shot("easy-seq1-steps-held");
        host_key(gn_wkey(4), 0);
        host_wait_ms(1400);
        host_btn(B_EDIT, 1);                         /* the tempo: EDIT held + SELECT */
        host_frame();
        host_turn(EN_SELECT, 4);
        host_wait_ms(100);
        shot("easy-seq1-tempo");
        host_btn(B_EDIT, 0);
        host_wait_ms(100);
        host_turn(EN_SELECT, -2);                    /* round FLOW-1, M2: GROOVES, the shelf (SONG hidden) */
        host_wait_ms(100);
        shot("easy-flow1-grooves");
    }
    /* round CR: FUNNY (the ninth picture of ten) on a fresh Easy (so no autosave above moves), its play page and its
     * eight sounds in the peek list, the eighth row above the band */
    host_start_easy = 0;
    host_easy();
    host_tap(B_HOME);                            /* (HOME on Home: the selected track's sound) */
    host_wait_ms(300);
    pick(-2);                                    /* (from another sound: back past the drums to FUNNY) */
    shot("easy-cr-funny");
    host_turn(EN_PRESET, 7);
    host_wait_ms(200);
    shot("easy-cr-peek-funny");
    host_wait_ms(GN_PEEK_MS + 100u);
    /* round FLOW-1: the performance transport's words, GLO held (fills), the record words with the
     * click's mark, EDIT held saying the click, on a fresh Easy */
    host_start_easy = 0;
    host_easy();
    favorites.factory[15][30] = GN_MODE_MARK | GN_MODE_EASY;   /* (the firmware's record: the click on) */
    host_tap(B_SEQ);
    host_tap(B_PLAY);
    host_btn(B_PLAY, 1);
    host_wait_ms(GN_RESTART_MS + 60u);
    shot("easy-flow1-restart-next-bar");         /* PLAY held: RESTART NEXT BAR in the band */
    host_btn(B_PLAY, 0);
    host_wait_ms(2100);                          /* (the restart done) */
    host_btn(B_GLO, 1);
    host_wait_ms(100);
    shot("easy-flow1-glo-held");                 /* GLO held: FILL (KEYS)  STOP AT BAR (PLAY) */
    host_tap(B_PLAY);
    host_btn(B_GLO, 0);
    host_wait_ms(100);
    shot("easy-flow1-stop-at-bar-end");          /* GLO + PLAY: STOP AT BAR END */
    while (song.playing)
        host_frame();
    host_tap(B_REC);
    host_tap(B_PLAY);
    host_wait_ms(700);
    shot("easy-flow1-count-in-click");           /* COUNT-IN 3 in the header, the click's mark */
    while (!song.rec)
        host_frame();
    host_wait_ms(300);
    shot("easy-flow1-recording-click");          /* RECORDING T1, the click's mark, KEEP in the band */
    host_btn(B_EDIT, 1);
    host_wait_ms(100);
    shot("easy-flow1-edit-click");               /* EDIT held: CLICK ON in the band */
    host_btn(B_EDIT, 0);
    host_tap(B_REC);
    host_tap(B_PLAY);
    host_wait_ms(GN_TAP_MS + 100u);
    /* M2: FX held + a key, an audition (the band says so); GROOVES's tiles: one loaded, one queued, one asking */
    host_btn(B_FX, 1);
    host_frame();
    host_key(gn_wkey(2), 1);
    host_wait_ms(150);
    shot("easy-flow1-audition");
    host_key(gn_wkey(2), 0);
    host_btn(B_FX, 0);
    host_wait_ms(100);
    host_turn(EN_SELECT, -1);                    /* (the loop page: GROOVES is one back) */
    host_wait_ms(100);
    shot("easy-flow1-grooves-shelf");
    key_tap(gn_wkey(0), 60);                     /* key 1: HEAD NOD loads and plays */
    host_wait_ms(700);
    key_tap(gn_wkey(1), 60);                     /* key 2: FLOOR FILLER at the next bar */
    while (fm1_ms % 1000u >= 500u)               /* (its tile's blink on) */
        host_frame();
    shot("easy-flow1-grooves-next-bar");
    while (gn.tile != 2u)
        host_frame();
    host_wait_ms(400);
    trk[0].step[3].time = ST_NOTE;               /* (a step of the player's: not kept) */
    trk[0].step[3].n = 1;
    trk[0].step[3].note[0] = 64;
    key_tap(gn_wkey(2), 60);
    host_wait_ms(40);
    shot("easy-flow1-grooves-ask");              /* NOT KEPT YET: STOP IT OR PRESS AGAIN */
    /* M3: a sound changed while playing (the wheel stays, the band names the track), a named undo */
    host_turn(EN_ALGO, 1);
    host_wait_ms(100);
    shot("easy-flow1-sound-changed");            /* TRACK 1 * SOUND CHANGED, the loop playing on */
    host_tap(B_HOME);
    host_tap(B_PLAY);
    host_wait_ms(200);
    host_tap(B_SEQ);                             /* the loop page */
    host_tap(B_REC);
    host_tap(B_PLAY);
    while (!song.rec)
        host_frame();
    key_tap(gn_wkey(4), 120);
    host_tap(B_REC);
    host_tap(B_PLAY);
    host_wait_ms(200);
    host_btn(B_SAVE, 1);
    host_wait_ms(800);
    host_btn(B_SAVE, 0);
    host_wait_ms(100);
    shot("easy-flow1-undid-take");               /* UNDID: LAST TAKE */
    host_wait_ms(2100);
    printf("%d screens in %s%s\n", shots, out_dir, ok_dir && !fails ? ", each the same bytes as the approved" : "");
    return fails ? 1 : 0;
}
