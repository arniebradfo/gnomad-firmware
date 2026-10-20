/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kyle Palaniuk */
/* Gnomad (FORK.md, "The UX rounds") host test: Gnomad's own screens on the synth, one coherent Easy and Advanced,
 * through the firmware's own code (tests/gn_host.h), driven as a person drives the panel and as the app drives the
 * editor. Each case starts from power-on in a process of its own. Built twice by tests/run_fork_tests.sh: as the
 * fork's other host tests build (Felucca's engines), and with the engine swap (GN_ENGINES 1: the Gnomad engines in
 * their slots, gn_engines.h first), for the pages that read the Gnomad engines (the kit's sixteen pads, the FM
 * page). Both with -fsanitize=address,undefined.
 *   UX-1  Home: the four tracks at a glance (board GN-03 A)
 *   UX-2  the loop's pages (board GN-02 A): bar rows, the bar switch, note names, the drum list, step entry
 *   UX-3  the sound browser (board GN-05 A): the picture wheel, the peek list, the named knobs, the kit's banks,
 *         SHIFT (EDIT held) and the deep page (the Gnomad FM's operators, read-only)
 *   UX-4  one Easy and Advanced: page identity and the selected track across the switch, a project never converted,
 *         SHIFT's depth on every page (MUTE, a step's note and strength, pan)
 *   UX-5  the status strip on every page, and the save words: SAVED only when a save was read back the same
 *   UX-6  Home: a selection move redraws both rows whole, the outline's right end too (the simulator's tour)
 *   SEQ-1 phase 1: SELECT's page ring, the tempo on EDIT + SELECT, LOOP layout A, the STEPS tap grid, lit knobs
 *   tests/run_fork_tests.sh builds it: build/host-gn/gn_ux_test, build/host-gn/gn_ux_eng_test */
#ifdef GN_UX_ENGINES
#define GN_ENGINES 1
#include "../firmware/src/gn_engines.h"
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

static void key(uint32_t k, uint32_t ms)            /* a key held ms, then let go */
{
    host_key(k, 1);
    host_wait_ms(ms);
    host_key(k, 0);
    host_wait_ms(30);
}

static void host_ed_val(uint8_t *b, int32_t v)      /* an editor value: 14 bits around 8192 */
{
    b[0] = (uint8_t)((v + 8192) & 127);
    b[1] = (uint8_t)(((v + 8192) >> 7) & 127);
}

static void app_mix(uint32_t t, int32_t level, int32_t mute)   /* the app: TRACK_MIX track, level, mute */
{
    uint8_t a[4] = {(uint8_t)t, 0, 0, (uint8_t)(mute != 0)};
    host_ed_val(a + 1, level);
    host_request(ED_TRACK_MIX, a, 4);
}

static void settle(void)                            /* Easy's screen drawn all it wants (time stands still) */
{
    uint32_t i;
    for (i = 0; i < 200u; i++) {
        ui_draw();
        if (!gs_job.on && !gs_px_call)
            break;
    }
}

/* ------------------------------------------------------------- UX-1 --- */
static void t_ux1_home(void)
{
    uint32_t k;
    host_easy_gnome();
    CHECK(gn.on && gn.page == GP_HOME && gn_view() == GV_HOME, "into Easy: Home, the four tracks");
    ui_draw();
    CHECK(str_eq(gs.row[0], "GNOME"), "..track 1's row: the entry's Gnome (shown there, round 14)");
    for (k = 0; k < NTRK; k++)
        CHECK(gs.row[k][0] && gx_text_w(&GS_LABEL, gs.row[k]) <= Q(100), "..every row names its sound, and it fits");
    CHECK(gs.trk == 0u && !gs.mute, "..track 1 selected, nothing muted");
    CHECK(str_eq(gs.head, "BARS 4 * SWING 0%") || (gs.head[0] == 'B' && gs.head[1] == 'A'),
          "..the header: the selected track's BARS and SWING beside the tempo");
    host_tap(B_HOME);
    CHECK(gn.page == GP_PLAY && gn_view() == GV_PLAY, "HOME on Home: the selected track's sound");
    host_tap(B_HOME);
    CHECK(gn.page == GP_HOME && gn_view() == GV_HOME, "HOME again: Home");
    host_tap(B_GLO);
    CHECK(gn_view() == GV_MIX, "GLO: the volumes");
    host_tap(B_GLO);
    CHECK(gn_view() == GV_HOME, "GLO again: back to Home");
    host_tap(B_SEQ);
    CHECK(gn_view() == GV_LOOP, "SEQ: the loop");
    host_tap(B_HOME);
    CHECK(gn_view() == GV_HOME, "HOME from the loop: Home");
}

static void t_ux1_rows(void)
{
    int32_t a0;
    host_easy();
    app_mix(1, 40, 1);                               /* the app: track 2 at 40, muted */
    host_frame();
    ui_draw();
    CHECK(gs.lvl[1] == 40u && (gs.mute & 2u), "the app's level and MUTE show on track 2's row");
    app_mix(1, 40, 0);
    host_frame();
    ui_draw();
    CHECK(!(gs.mute & 2u), "..unmuted");
    host_tap(B_ENV);                                 /* track 3 selected */
    ui_draw();
    CHECK(gs.trk == 2u, "a track button: its row outlined (selected)");
    host_turn(EN_ALGO, 1);                           /* the Gnome on track 3 */
    host_wait_ms(GN_PICK_MS + 100u);
    a0 = gn.amount[2][0];
    host_turn(EN_K1, 2);
    CHECK(gn_view() == GV_PICK && gn.amount[2][0] == a0 + 2, "Home's knobs: the selected sound's four (K1 Bright; the wheel stays: round FLOW-1)");
    host_tap(B_HOME);
    CHECK(gn_view() == GV_HOME, "..HOME ends the wheel, Home under it stays");
    host_key(7, 1);
    host_wait_ms(60);
    ui_draw();
    CHECK((gs.live >> 2) & 1u, "a key held: the row sounds (the triangle)");
    host_key(7, 0);
    host_wait_ms(2500);                              /* (the release's tail and the 150 ms after a note's start) */
    ui_draw();
    CHECK(!((gs.live >> 2) & 1u), "..let go and over: quiet");
    settle();
    app_mix(0, 70, 0);                               /* a change on one row redraws that row, not the screen */
    host_audio_us(15000u);
    ui_input();
    ui_draw();
    CHECK(gs_px_call > 0u && gs_px_call <= 240u * 36u, "a level change: that row redrawn, nothing else");
}

static void t_ux1_hat(void)
{
    uint32_t i;
    host_easy();
    host_tap(B_LFO);                                 /* track 4: Easy's kit, its pattern empty */
    host_turn(EN_ALGO, -1);
    host_wait_ms(GN_PICK_MS + 100u);
    for (i = 0; i < NSTEP; i++)
        step_clear(&trk[3].step[i]);
    host_tap(B_HOME);
    if (gn.page != GP_HOME)
        host_tap(B_HOME);
    ui_draw();
    CHECK(gs.hat == 3u, "an empty kit: its row wears the Gnome's hat");
    trk[3].step[0].time = ST_NOTE;
    trk[3].step[0].n = 1;
    trk[3].step[0].note[0] = 36;
    host_frame();
    ui_draw();
    CHECK(gs.hat == 0xFFu, "..a hit dropped in: no hat");
}

/* ------------------------------------------------------------- UX-2 --- */
static void seq_to(uint32_t page)                    /* SEQ until that page of the loop's (loop, steps, record) */
{
    uint32_t i;
    for (i = 0; i < 4u && gn.page != page; i++)
        host_tap(B_SEQ);
}

static void t_ux2_loop(void)
{
    uint32_t k;
    host_easy();
    host_turn(EN_ALGO, 2);                           /* the Organ on track 1 */
    host_wait_ms(GN_PICK_MS + 100u);
    seq_to(GP_LOOP);
    host_turn(EN_K1, -3);                            /* BARS 1, then 2 */
    host_turn(EN_K1, 1);
    seq_to(GP_RECORD);
    for (k = 0; k < 3u; k++)                         /* three notes written, a rest between each */
        key(7u + 2u * k, 40), host_turn(EN_K1, 1);
    seq_to(GP_LOOP);
    ui_draw();
    CHECK(gn_view() == GV_LOOP && gs.bars == 2u && gs_nbars() == 2u, "the loop: a block a bar, two bars");
    CHECK(gs.occ[0] && !gs.occ[1], "..bar 1 marks where it sounds, bar 2 nothing");
    CHECK(!gs.playing && gs.step == 0xFFu, "..stopped: no playhead");
    host_tap(B_PLAY);
    host_wait_ms(200);
    ui_draw();
    CHECK(gs.playing && gs.step != 0xFFu && gs.bar == 0u, "PLAY: the playhead in bar 1");
    for (k = 0; k < 4u; k++)
        CHECK(gn_knob_role(k) == KR_LOOP, "..its knobs: BARS SWING SNAP VOLUME (pick 7)");
    host_tap(B_PLAY);
}

static void t_ux2_steps(void)
{
    char l[12], v[12], nm[8];
    host_easy();
    host_turn(EN_ALGO, 2);
    host_wait_ms(GN_PICK_MS + 100u);
    seq_to(GP_LOOP);
    host_turn(EN_K1, -3);
    host_turn(EN_K1, 1);                             /* two bars */
    seq_to(GP_RECORD);
    key(8, 40);                                      /* C#4 at step 1 */
    seq_to(GP_STEPS);
    ui_draw();
    CHECK(gn_view() == GV_STEPS && gn_knob_role(0) == KR_BAR && gs.bar == 0u, "the steps: K1 is the bar switch, bar 1");
    CHECK(gs.cnote[0] == 61u, "..the place holds its note (C#4)");
    note_name(nm, gs.cnote[0]);
    CHECK(str_eq(nm, "C#4") && gx_glyph('#')->c == '#', "..its name over it, the sharp drawn (the type has #)");
    host_turn(EN_K1, 1);
    ui_draw();
    gs_knob_text(0, l, v);
    CHECK(gs.bar == 1u && str_eq(l, "BAR") && str_eq(v, "2 OF 2"), "K1: bar 2 of 2 (its card says so)");
    host_turn(EN_K1, 5);
    CHECK(gn_view_bar(0) == 1u, "..no further than the last");
    host_turn(EN_K1, -1);
    ui_draw();
    CHECK(gs.bar == 0u && gs.cnote[0] == 61u, "K1 back: bar 1");
    host_turn(EN_K1, 1);
    host_tap(B_PLAY);
    host_wait_ms(500);
    CHECK(gn_view_bar(0) == 1u, "playing: the bar K1 chose, for 3 s");
    host_wait_ms(GN_VBAR_MS);
    CHECK(gn_view_bar(0) == (trk[0].seq_idx % (uint32_t)trk[0].p[P_SLEN]) / gn_spb((uint32_t)trk[0].p[P_SDIV]),
          "..then the playhead's");
    host_tap(B_PLAY);
}

static void t_ux2_drums(void)
{
    char l[12], v[12];
    uint32_t n0;
    host_easy();
    host_tap(B_LFO);                                 /* track 4: Easy's kit */
    host_turn(EN_ALGO, -1);
    host_wait_ms(GN_PICK_MS + 100u);
    seq_to(GP_RECORD);
    CHECK(gn_view() == GV_RECORD && gn_knob_role(0) == KR_MOVE && gn_knob_role(1) == KR_DRUM && gn_knob_role(2) == KR_MOVE,
          "a kit's step entry: K1 MOVE, K2 the drum, K3 NOTE / REST");
    host_turn(EN_K2, 2);
    ui_draw();
    gs_knob_text(1, l, v);
    CHECK(gn.drum == 2u && str_eq(v, gn_kit_name(3, 2)) && gs.dname && str_eq(gs.dname, gn_kit_name(3, 2)),
          "K2: the third drum, named on its card and on the page");
    n0 = gn.cursor;
    host_turn(EN_K3, 1);                             /* (round 24, gn/ux catch-up: NOTE / REST writes in place) */
    CHECK(gn_kit_has(3, &trk[3].step[n0], 2) && gn.cursor == n0 && gn.undo_of == 4u,
          "K3 up: that drum at the cursor, the cursor stays, an undo copy");
    host_turn(EN_K1, 1);
    CHECK(gn.cursor == n0 + 1u, "K1: the cursor on");
    host_turn(EN_K3, -1);
    CHECK(gn.cursor == n0 + 1u &&
          (trk[3].step[n0 + 1u].time != ST_NOTE || (!trk[3].step[n0 + 1u].n && !trk[3].step[n0 + 1u].hit)),
          "K3 down: a rest, in place");
    key(0, 40);                                      /* the first white key: pad 1 at the cursor, K2's drum now */
    CHECK(gn.drum == 0u, "a key writes its drum, and K2's drum is that one");
    seq_to(GP_STEPS);
    host_turn(EN_K2, 2);
    ui_draw();
    CHECK(gn_view() == GV_STEPS && gs.ndrum == gn_kit_n(3) && gs.ndrum >= 8u && gs.drn[2] && (gs.drow[2] & 1u),
          "the drum list: every drum with its row, the third's hit in place 1");
    CHECK(gs.dnote[2] == gn_kit_note(3, 2), "..each with its note");
    host_turn(EN_K2, 8);
    ui_draw();
    CHECK(gs.ndrum < 16u || (gs.drum == 10u && gs.dbank == 1u), "K2 past the eighth: the list's second half (9 to 16)");
#if GN_ENGINES
    CHECK(gn_kit_n(3) == 16u && str_eq(gn_kit_name(3, 3), "CLOSED HAT") && gn_kit_note(3, 3) == 39u,
          "the Gnomad kit: sixteen drums, pad i on note 36 + i (its stable identities)");
#else
    CHECK(gn_kit_n(3) == 16u && gn_kit_note(3, 2) == GN_PADS[2], "Easy's kit: its sixteen pads");
#endif
}

static void t_ux2_play_opens(void)
{
    host_easy();
    seq_to(GP_RECORD);
    host_tap(B_REC);
    CHECK(gn.page == GP_RECORD && gn.arm == GA_READY, "step entry: REC arms, the page stays (REC never leaves it)");
    host_tap(B_PLAY);
    CHECK(gn.page == GP_LOOP && gn_view() == GV_COUNT, "..PLAY opens the loop: the count-in, then the loop");
    host_wait_ms(2200);
    CHECK(song.rec == 1u && gn_view() == GV_LOOP, "..recording, on the loop page");
    host_tap(B_REC);
    host_tap(B_PLAY);
    seq_to(GP_RECORD);
    host_tap(B_PLAY);
    host_wait_ms(60);
    CHECK(song.playing && gn.page == GP_LOOP, "not armed: PLAY from step entry opens the loop too");
    host_tap(B_PLAY);
    host_tap(B_HOME);
    host_tap(B_PLAY);
    host_wait_ms(60);
    CHECK(song.playing && gn.page == GP_HOME, "from any other page PLAY leaves the page as it is");
    host_tap(B_PLAY);
}

/* ------------------------------------------------------------- UX-3 --- */
static void to_sound(void)                           /* the sound page: HOME, then HOME on Home */
{
    host_tap(B_HOME);
    if (gn.page != GP_PLAY)
        host_tap(B_HOME);
}

static void t_ux3_browse(void)
{
    uint32_t k;
    host_easy();
    host_turn(EN_ALGO, 3);                           /* the third picture: the Bells */
    CHECK(gn_view() == GV_PICK && gn_picture() == 2u, "ALGORITHM: the wheel, the Bells in the middle");
    host_wait_ms(GN_PICK_MS + 2000u);
    ui_draw();
    CHECK(gn_view() == GV_PICK && str_eq(GS_HINT_PICK, "TURN ALGORITHM: PICTURE"),
          "..round FLOW-1, M2: the wheel stays (no time-out), and says TURN ALGORITHM: PICTURE");
    host_turn(EN_PRESET, 1);
    CHECK(gn_view() == GV_PEEK, "PRESETS (the other encoder): the wheel gone, the peek list");
    host_wait_ms(GN_PEEK_MS + 2000u);
    ui_draw();
    CHECK(gn_view() == GV_PEEK && str_eq(GS_HINT_PEEK, "TURN PRESETS: SOUND"), "..it stays too: TURN PRESETS: SOUND");
    host_key(7, 1);
    host_frame();
    host_key(7, 0);
    host_frame();
    CHECK(gn_view() == GV_HOME && gn_easy_of(&trk[0]) == GN_GROUP[2] + 1u, "..a key: the page again, the sound kept");
    host_turn(EN_ALGO, 1);
    host_turn(EN_ALGO, -1);
    host_tap(B_PLAY);
    CHECK(gn_view() == GV_HOME, "..PLAY ends the wheel too");
    host_tap(B_PLAY);
    to_sound();
    ui_draw();
    CHECK(gn_view() == GV_PLAY, "HOME on Home: the sound page");
    for (k = 0; k < 4u; k++)
        CHECK(gs.amt[k] == gn.amount[0][k], "..its four knobs' bars as far as each is turned");
    CHECK(str_eq(GS_MACRO[0], "TONE") && str_eq(GS_MACRO[1], "MOTION") && str_eq(GS_MACRO[2], "ECHO") &&
          str_eq(GS_MACRO[3], "SPACE"), "..named TONE MOTION ECHO SPACE");
    host_turn(EN_K2, 3);
    ui_draw();
    CHECK(gs.amt[1] == gn.amount[0][1] && gn_view() == GV_PLAY, "K2 turned: its bar follows");
}

static void t_ux3_kit(void)
{
    uint32_t k, w9 = 0;
    int8_t oct;
    host_easy();
    host_tap(B_LFO);
    host_turn(EN_ALGO, -1);                          /* Easy's kit on track 4 */
    host_wait_ms(GN_PICK_MS + 100u);
    to_sound();
    ui_draw();
    CHECK(gn_view() == GV_DRUMS && gs.kn == 16u && gs.bank == 0u && str_eq(gs.kname[0], gn_kit_name(3, 0)),
          "the kit: bank A, pads 1 to 8 named");
    oct = song.octave;
    host_tap(B_OCTUP);
    ui_draw();
    CHECK(gn.bank == 1u && gs.bank == 1u && str_eq(gs.kname[0], gn_kit_name(3, 8)) && song.octave == oct,
          "OCT+: bank B (pads 9 to 16), the octave untouched");
    CHECK(host_led(panel.btn[B_OCTUP]) && !host_led(panel.btn[B_OCTDN]), "..OCT+ lit: B");
    for (k = 0; k < 27u; k++)                        /* the tenth white key: pad 10 */
        if (GN_WHITE[k] == 9u)
            w9 = k;
    host_key(w9, 1);
    host_frame();
    ui_draw();
    CHECK((gs.pads >> 9) & 1u, "the tenth white key: pad 10 lit, in bank B");
    host_key(w9, 0);
    host_tap(B_OCTDN);
    CHECK(gn.bank == 0u && song.octave == oct, "OCT-: bank A");
#if GN_ENGINES
    for (k = 0; k < 27u; k++)
        if (GN_WHITE[k] != 0xFFu)
            CHECK(gn_key(&trk[3], k, 0) == 36u + GN_WHITE[k], "the Gnomad kit: white key n plays its pad n (MIDI 36 + n)");
    key(w9, 80);
    CHECK(gn_kit_of(3, 45) == 9u && str_eq(gn_kit_name(3, 9), "COWBELL"), "..pad 10 is the cowbell");
#endif
}

static void t_ux3_shift(void)
{
    uint32_t i;
    int16_t bpm;
    host_easy();
    to_sound();
    for (i = 0; i < 3u; i++) {                       /* tap tempo: 120 */
        host_tap(B_EDIT);
        host_wait_ms(500 - 45);                     /* (a tap is two frames: 495 ms apart) */
    }
    bpm = song.g[G_BPM];
    CHECK(bpm >= 118 && bpm <= 122 && !gn.shift, "EDIT taps: tap tempo, as before");
    host_wait_ms(200);
    host_btn(B_EDIT, 1);                             /* a press 0.2 s after the last tap: held on */
    host_frame();
    CHECK(song.g[G_BPM] != bpm || gn_view() == GV_TAP, "..its press is a tap first");
    host_wait_ms(GN_SHIFT_MS + 50u);
    CHECK(gn.shift && song.g[G_BPM] == bpm, "held 0.5 s: SHIFT, and that tap taken back (the tempo as it was)");
    CHECK(gn_view() == GV_PLAY && host_led(panel.btn[B_EDIT]), "..round FLOW-1: no deep page in Easy, the sound page stays, EDIT lit");
    CHECK(gn_knob_role(0) == KR_SOUND && gn_knob_role(1) == KR_SOUND && gn_knob_role(3) == KR_SOUND,
          "..the sound's own knobs, as without SHIFT");
    host_btn(B_EDIT, 0);
    host_frame();
    CHECK(!gn.shift && gn_view() == GV_PLAY && song.g[G_BPM] == bpm, "EDIT let go: the sound page, the tempo kept");
}

#if GN_ENGINES
static void t_ux3_fm(void)
{
    uint8_t a[2] = {12, 0};                          /* the app: the Gnomad FM's sound on track 1 */
    const uint8_t *b = GN_CORE_PATCH[GC_FM];
    uint32_t alg = (uint32_t)(b[32 + 24] | b[33 + 24] << 8) & 7u, j;
    host_easy();
    host_request(ED_PRESET, a, 2);
    host_wait_ms(100);
    to_sound();
    host_btn(B_EDIT, 1);
    host_wait_ms(GN_SHIFT_MS + 100u);
    ui_draw();
    (void)b;
    (void)alg;
    (void)j;
    CHECK(gn_view() == GV_PLAY, "SHIFT on the Gnomad FM: round FLOW-1, M2: the read-only deep page is hidden from Easy");
    host_btn(B_EDIT, 0);
    host_frame();
}
#endif

/* ------------------------------------------------------------- UX-4 --- */
static void felucca_page(uint32_t fam, uint32_t graph)   /* Advanced: Felucca on that page (the first of its kind) */
{
    uint32_t i;
    for (i = 0; i < NPAGES && !(PAGES[i].fam == fam && (graph == 0xFFu || PAGES[i].graph == graph)); i++)
        ;
    ui.home = 0;
    ui.page = (uint8_t)i;
    page_entered();
    host_frame();
}

typedef struct {
    step_t step[NTRK][NSTEP];
    int16_t p[NTRK][P_COUNT];
    uint8_t eng[NTRK], pre[NTRK];
    int16_t g[G_COUNT];
    motion_store_t motion;
} music_t;

static void music_get(music_t *m)
{
    uint32_t t;
    memset(m, 0, sizeof *m);
    for (t = 0; t < NTRK; t++) {
        memcpy(m->step[t], trk[t].step, sizeof trk[t].step);
        memcpy(m->p[t], trk[t].p, sizeof trk[t].p);
        m->eng[t] = trk[t].eng_req;
        m->pre[t] = trk[t].preset;
    }
    memcpy(m->g, song.g, sizeof song.g);
    m->motion = motion;
}

static void t_ux4_identity(void)
{
    host_power_on();
    host_frame();
    track_select(2);
    felucca_page(FAM_SEQ, GR_ROLL);                  /* Advanced: STEP, track 3, the cursor on step 6 */
    cursor_set(5);
    host_hold_switch(2100);
    CHECK(gn.on && gn.page == GP_STEPS && song.sel == 2u && gn.cursor == 5u,
          "from Felucca's STEP: Easy's steps, the same track, the same step");
    CHECK(!gn.entry_put, "..no Gnome put in the way (he greets an entry on Home)");
    host_key(7, 1);                                  /* (the start-up picture over) */
    host_frame();
    host_key(7, 0);
    host_wait_ms(200);
    seq_to(GP_RECORD);                               /* (step entry opens at step 1: round 7) */
    host_turn(EN_K1, 3);
    host_hold_switch(2100);
    CHECK(!gn.on && !ui.home && cur_page()->fam == FAM_SEQ && cur_page()->graph == GR_ROLL && ui.cursor == 3u &&
          song.sel == 2u, "leaving step entry: Felucca's STEP, Easy's step (4), the same track");
    felucca_page(FAM_SEQ, GR_STEPS);                 /* PATTERN */
    host_hold_switch(2100);
    CHECK(gn.on && gn.page == GP_LOOP, "from Felucca's PATTERN: the loop");
    host_hold_switch(2100);
    CHECK(!gn.on && !ui.home && cur_page()->fam == FAM_SEQ && cur_page()->graph != GR_ROLL, "..and back to PATTERN");
    felucca_page(FAM_TRK, 0xFFu);                    /* the mixer */
    host_hold_switch(2100);
    CHECK(gn.on && gn.page == GP_MIX, "from the mixer: the volumes");
    host_hold_switch(2100);
    CHECK(!gn.on && !ui.home && cur_page()->fam == FAM_TRK, "..and back to the mixer");
    felucca_page(FAM_ENV, 0xFFu);                    /* a sound's page */
    host_hold_switch(2100);
    CHECK(gn.on && gn.page == GP_PLAY && song.sel == 2u, "from a sound's page: the sound, the same track");
    host_hold_switch(2100);
    CHECK(!gn.on && ui.home, "..the sound back to HOME");
    host_hold_switch(2100);
    CHECK(gn.on && gn.page == GP_HOME && song.sel == 0u && gn.entry_put, "from HOME: Home, the Gnome shown on track 1 (round 13)");
}

static void t_ux4_never_converts(void)
{
    music_t a, b;
    uint32_t i, from;
    host_power_on();
    host_frame();
    trk[1].p[P_SDIV] = 5;                            /* a grown-up's triplets: not Easy's grid */
    trk[1].p[P_SLEN] = 48;
    trk[1].p[P_SSWING] = 37;
    for (i = 0; i < 48u; i += 5u) {
        trk[1].step[i].time = ST_NOTE;
        trk[1].step[i].n = 1;
        trk[1].step[i].note[0] = (uint8_t)(50u + i);
        trk[1].step[i].vel = (uint8_t)(20u + i);
    }
    trk[1].step[1].time = ST_TIE;
    track_select(1);
    for (from = 0; from < 3u; from++) {
        if (from == 1u)
            felucca_page(FAM_SEQ, GR_ROLL);
        else if (from == 2u)
            felucca_page(FAM_TRK, 0xFFu);
        else
            go_home();
        host_frame();
        music_get(&a);
        host_hold_switch(2100);
        host_wait_ms(3200);
        host_tap(B_HOME);                            /* every page, looked at, nothing turned */
        host_tap(B_HOME);
        seq_to(GP_LOOP);
        seq_to(GP_STEPS);
        seq_to(GP_RECORD);
        host_tap(B_GLO);
        host_btn(B_EDIT, 1);                         /* SHIFT held on a page and let go */
        host_wait_ms(GN_SHIFT_MS + 100u);
        host_btn(B_EDIT, 0);
        host_tap(B_HOME);
        host_hold_switch(2100);
        music_get(&b);
        CHECK(!gn.on && !memcmp(&a, &b, sizeof a), "a mode switch and back, every page visited: the music byte for byte");
        CHECK(trk[1].p[P_SDIV] == 5 && trk[1].p[P_SLEN] == 48, "..the triplet grid kept (never converted)");
    }
}

static void t_ux4_shift(void)
{
    uint32_t n0;
    char l[12], v[12];
    host_easy();
    host_btn(B_EDIT, 1);
    host_wait_ms(GN_SHIFT_MS + 100u);
    ui_draw();
    CHECK(gn.shift && gn_view() == GV_HOME && str_eq(gs_hint("x"), gn_click_on() ? "CLICK ON (K3)  MUTE: FX..LFO" :
                                                                       "CLICK OFF (K3)  MUTE: FX..LFO"),
          "SHIFT on Home: it says what it does first (MUTE on the track buttons; round FLOW-1: and the click, K3)");
    host_tap(B_ENV);
    CHECK(trk[2].p[P_MUTE] && song.sel == 0u, "..SHIFT + ENV: track 3 muted, the selection kept");
    host_tap(B_ENV);
    CHECK(!trk[2].p[P_MUTE], "..again: unmuted");
    host_btn(B_EDIT, 0);
    host_tap(B_ENV);
    CHECK(song.sel == 2u && !trk[2].p[P_MUTE], "let go: ENV selects track 3, as before");
    host_tap(B_FX);
    seq_to(GP_RECORD);
    key(7, 40);                                      /* C4 at step 1 */
    host_turn(EN_K1, -1);
    n0 = gn.cursor;
    seq_to(GP_STEPS);
    host_btn(B_EDIT, 1);
    host_wait_ms(GN_SHIFT_MS + 100u);
    CHECK(gn_knob_role(0) == KR_MOVE && gn_knob_role(1) == KR_PITCH && gn_knob_role(2) == KR_VEL,
          "SHIFT on the steps: K1 the step, K2 its note, K3 its strength");
    host_turn(EN_K2, 1);
    host_turn(EN_K3, 2);
    ui_draw();
    gs_knob_text(1, l, v);
    CHECK(trk[0].step[n0].note[0] == 61u && trk[0].step[n0].vel == 108u && str_eq(v, "C#4"),
          "..C4 up to C#4, strength 100 to 108 (its cards say so)");
    CHECK(gs.ccur != 0xFFu && gs.son, "..the cursor's place outlined");
    host_btn(B_EDIT, 0);
    host_frame();
    host_btn(B_SAVE, 1);                             /* SAVE held: undone */
    host_wait_ms(800);
    host_btn(B_SAVE, 0);
    host_wait_ms(60);
    CHECK(trk[0].step[n0].time != ST_NOTE || !trk[0].step[n0].n,
          "SAVE held: the session undone, step entry and SHIFT's edits together (one undo copy, as step entry's)");
    host_tap(B_GLO);
    host_btn(B_EDIT, 1);
    host_wait_ms(GN_SHIFT_MS + 100u);
    {
        int16_t pan = trk[1].p[P_PAN];
        host_turn(EN_K2, 3);
        ui_draw();
        gs_knob_text(1, l, v);
#if GN_ENGINES                                       /* round EW: the core is mono: SHIFT leaves the volumes as they are */
        CHECK(trk[1].p[P_PAN] == pan && gn_knob_role(1) == KR_MIX && !str_eq(l, "2 PAN"),
              "SHIFT on the volumes: no pan on the Gnomad engines (mono core), K2 still track 2's volume");
#else
        CHECK(trk[1].p[P_PAN] == pan + 12 && str_eq(l, "2 PAN"), "SHIFT on the volumes: K2 track 2's pan");
#endif
    }
    host_btn(B_EDIT, 0);
    seq_to(GP_LOOP);
    host_btn(B_EDIT, 1);
    host_wait_ms(GN_SHIFT_MS + 100u);
    CHECK(gn_knob_role(0) == KR_LOOP && gn_knob_role(3) == KR_LOOP, "SHIFT on the loop: its knobs as they are");
    host_btn(B_EDIT, 0);
}

/* ------------------------------------------------------------- UX-5 --- */
static int stored_now(void)                          /* what the strip says after a few frames: 1 SAVED, 0 ON SYNTH */
{
    host_wait_ms(80);
    ui_draw();
    return gs.stored;
}

static void t_ux5_words(void)
{
    uint8_t a[2] = {1, 2};                           /* the app: PROJECT save, slot 3 */
    host_start_easy = 1;                             /* (the firmware's start, in Easy: it keeps its record of the */
    host_power_on();                                 /* slot) */
    host_frame();
    key(7, 40);                                      /* (the start-up picture over) */
    CHECK(gn.on && !stored_now(), "the first power-on, nothing saved yet: ON SYNTH");
    host_tap(B_SAVE);
    host_wait_ms(200);
    CHECK((gn.saved_mine == 1u || gn.saved_mine == 4u) && stored_now(), "SAVE, read back from flash the same: SAVED");
    host_turn(EN_K1, 2);
    CHECK(!stored_now(), "a knob moves the sound: ON SYNTH");
    host_turn(EN_K1, -2);
    CHECK(stored_now(), "..back where it was saved: SAVED again (the music is what the flash holds)");
    host_tap(B_SCL);
    CHECK(stored_now(), "another track selected: still SAVED (choosing is not music)");
    host_tap(B_FX);
    host_tempo(3);
    CHECK(!stored_now(), "the tempo: ON SYNTH");
    host_request(ED_PROJECT, a, 2);
    host_wait_ms(100);
    CHECK(stored_now(), "the app saves, read back: SAVED");
    host_wait_ms(GN_SAVED_MS + 100u);
    host_tempo(-3);
    flash_ok = 0;                                    /* the flash takes nothing: RAM only */
    host_tap(B_SAVE);
    host_wait_ms(200);
    CHECK(gn.saved_mine == 3u && !stored_now(), "a save the flash does not hold: NOT SAVED, and the strip ON SYNTH");
    flash_ok = 1;
    host_start_easy = 0;
}

static void t_ux5_strip(void)
{
    char b[32];
    host_easy();
    ui_draw();
    gs_state_words(b);
    CHECK(str_eq(b, "STOPPED * HOME"), "the strip on Home: STOPPED * HOME");
    host_tap(B_SEQ);
    ui_draw();
    gs_state_words(b);
    CHECK(str_eq(b, "STOPPED * NOTES"), "..on the loop: NOTES");
    host_tap(B_REC);
    ui_draw();
    gs_state_words(b);
    CHECK(str_eq(b, "ARMED T1 * KEEP"), "..REC: ARMED, the track and KEEP (round FLOW-1)");
    host_tap(B_PLAY);
    host_wait_ms(2200);
    ui_draw();
    gs_state_words(b);
    CHECK(str_eq(b, "RECORDING T1 * KEEP"), "..the count-in over: RECORDING, the track, KEEP");
    host_tap(B_REC);
    ui_draw();
    gs_state_words(b);
    CHECK(str_eq(b, "PLAYING * NOTES"), "..REC again: PLAYING");
    host_tap(B_GLO);
    ui_draw();
    gs_state_words(b);
    CHECK(str_eq(b, "PLAYING * MIX"), "..GLO: MIX");
    host_tap(B_PLAY);
    host_tap(B_HOME);
    host_tap(B_HOME);
    ui_draw();
    gs_state_words(b);
    CHECK(str_eq(b, "STOPPED * SOUND"), "..the sound page: SOUND");
}

/* ------------------------------------------------------------- UX-6 --- */
/* Home's selection moved by a track button (the simulator's tour): the outline runs x 10..236, so the
 * screen the parts leave must be the screen drawn whole, on every move, and nothing outside the two rows changes
 * in the field */
static uint16_t ux6_was[240 * 240], ux6_now[240 * 240];

static void t_ux6_select(void)
{
    static const uint32_t TB[5] = {B_SCL, B_ENV, B_LFO, B_FX, B_ENV};   /* tracks 2, 3, 4, 1, 3 */
    uint32_t m, i, out, bad;
    host_easy();
    ui_draw();
    settle();
    CHECK(gn_view() == GV_HOME && gs.trk == 0u, "on Home, track 1 selected");
    for (m = 0; m < 5u; m++) {
        uint32_t from = gs.trk, to = TB[m] - B_FX;
        int32_t f0 = GS_ROW_Y(from) - 1, t0 = GS_ROW_Y(to) - 1;
        memcpy(ux6_was, host_screen, sizeof ux6_was);
        host_tap(TB[m]);
        settle();
        CHECK(gn_view() == GV_HOME && gs.trk == to, "a track button on Home: that track selected");
        memcpy(ux6_now, host_screen, sizeof ux6_now);
        for (i = 0, out = 0; i < 240u * 240u; i++) {
            int32_t y = (int32_t)(i / 240u);
            if (y >= 70 && y < 210 && ux6_now[i] != ux6_was[i] && !(y >= f0 && y < f0 + 35) && !(y >= t0 && y < t0 + 35))
                out++;
        }
        CHECK(out == 0u, "..in the field, only the row it left and the row it took changed");
        gs_shown_view = GV_NONE;                      /* the same screen drawn whole */
        settle();
        for (i = 0, bad = 0; i < 240u * 240u; i++)
            bad += host_screen[i] != ux6_now[i];
        if (bad)
            printf("    move %u (track %u -> %u): %u pixels differ from the screen drawn whole\n", m, from + 1u, to + 1u, bad);
        CHECK(bad == 0u, "..the selection redrawn whole: the screen as if drawn whole (no outline left behind)");
    }
}

#if GN_ENGINES
/* ------------------------------------------------------------- UX-7 --- */
/* the editor's reply after the header (F0 7D h1 h2 cmd): its strings, 0-terminated, from byte p; the next p */
static uint32_t ux7_str(uint32_t p, uint32_t len, char *s, uint32_t n)
{
    uint32_t j = 0;
    while (p < len - 1u && host_wire[p] && j < n - 1u)
        s[j++] = (char)host_wire[p++];
    s[j] = 0;
    return p + 1u;
}

static void t_ux7_engines(void)
{
    /* the app's FirmwareShape.gnomad10 (the companion app's EngineTable.gnomad10): INFO's names, exactly */
    static const char *const APP[NENGINES] = {"ANALOG", "-", "PHASE", "LOFI", "SAMPLE", "VOICE", "TRIO", "GNOME", "-",
                                              "VA", "DRUMS", "NOISE", "FM", "SAMPLER"};
    static const uint32_t PLAY[5] = {9, 12, 10, 7, 13}, GONE[7] = {0, 2, 3, 4, 5, 6, 11};
    char info[NENGINES][16], s[16];
    uint32_t i, k, n, p, len, total, bad, page, seen[NENGINES + 1u] = {0};
    host_power_on();
    CHECK(!gn.on, "Advanced (Felucca's pages)");

    /* the lists: the order, the PRESETS list, the EDIT layer's keys */
    for (i = 0, bad = 0; i < 5u; i++)
        bad += eng_vis(i) != PLAY[i];
    CHECK(NENG_SHOWN == 5 && !bad, "the order: VA FM DRUMS GNOME SAMPLER, nothing else");
    for (i = 0, bad = 0; i < 7u; i++)
        bad += !eng_gone(GONE[i]) || eng_ok(GONE[i]) || eng_rank(GONE[i]) != eng_rank(12);
    CHECK(!bad, "..the seven retired slots: never offered, shown where FM is");
    (void)preset_all_pos(&total);
    for (n = 0, bad = 0; n < total; n++) {
        uint32_t e = preset_all_at(n, &k);
        seen[e]++;
        bad += e < NENGINES && !eng_ok(e);
    }
    for (i = 0; i < 5u; i++)
        bad += !seen[PLAY[i]];
    CHECK(!bad && total > 0u, "PRESETS: every sound of the five, none of a retired engine");
    CHECK(LY_INIT == 5u, "the EDIT layer: a key for each of the five, INIT on the sixth");

    /* ENG (the PRESETS page's KNOB 2): five detents round from VA, each a Gnomad engine */
    for (page = 0; page < NPAGES && PAGES[page].graph != GR_BROWSE; page++)
        ;
    ui.home = 0;
    ui.page = (uint8_t)page;
    select_engine(9);
    for (i = 0, bad = 0; i < 5u; i++) {
        host_turn(EN_K2, 1);
        host_wait_ms(300);
        bad += TSEL->eng_req != PLAY[(i + 1u) % 5u];
    }
    for (i = 0; i < 5u; i++) {
        host_turn(EN_K2, -1);
        host_wait_ms(300);
        bad += !eng_ok(TSEL->eng_req);
    }
    CHECK(page < NPAGES && !bad && TSEL->eng_req == 9u, "ENG turned: VA FM DRUMS GNOME SAMPLER and round, both ways");

    /* the editor: INFO, DESC's ENG names, NAMES */
    len = host_request(ED_INFO, 0, 0);
    p = ux7_str(5, len, s, sizeof s) + 5u;
    CHECK(len > 7u && host_wire[4] == ED_INFO && host_wire[p - 5u] == NENGINES, "INFO: fourteen slots");
    for (i = 0; i < NENGINES; i++)
        p = ux7_str(p, len, info[i], sizeof info[i]);
    for (i = 0, bad = 0; i < NENGINES; i++)
        bad += !str_eq(info[i], GN_EDITOR_HIDES_GONE && eng_gone(i) ? "" : APP[i]);
    CHECK(!bad, GN_EDITOR_HIDES_GONE ? "..a retired slot named \"\", the others as the app's table"
                                     : "..the app's table exactly (FirmwareShape.gnomad10: the switch is off)");
    {
        uint8_t a[2] = {1, G_ENGSEL};
        len = host_request(ED_DESC, a, 2);
        p = 14;                                          /* the header, scope, id, fmt, min, max, def */
        p = ux7_str(p, len, s, sizeof s);                /* label */
        p = ux7_str(p, len, s, sizeof s);                /* unit */
        for (i = 0, bad = 0; i < NENGINES; i++) {
            p = ux7_str(p, len, s, sizeof s);
            bad += !str_eq(s, info[i]);
        }
        CHECK(host_wire[4] == ED_DESC && !bad && p == len - 1u, "DESC ENG (global 20): INFO's names, slot for slot");
    }
    for (i = 0, bad = 0; i < 7u; i++) {
        uint8_t a[1] = {(uint8_t)GONE[i]};
        len = host_request(ED_NAMES, a, 1);
        bad += len < 8u || host_wire[5] != GONE[i] ||
               host_wire[6] != (GN_EDITOR_HIDES_GONE ? 0u : ENGINES[GONE[i]]->npresets);
    }
    CHECK(!bad, GN_EDITOR_HIDES_GONE ? "NAMES: a retired slot, 0 presets" : "NAMES: a retired slot as before (switch off)");
    {
        uint8_t a[1] = {12};
        len = host_request(ED_NAMES, a, 1);
        CHECK(len > 8u && host_wire[6] > 0u, "NAMES: FM's presets, as before");
    }
}
#endif

/* ------------------------------------------------------------- SEQ-1 --- */
/* round SEQ-1 (phase 1): the page ring on SELECT, the tempo on EDIT held + SELECT, layout A's LOOP, the STEPS tap
 * grid (tap on, tap off, hold + K1..K4), the lit knobs, gn_seq_* as the pages' only way to the steps */
static uint32_t wkey(uint32_t c)                    /* the white key of place c */
{
    uint32_t k;
    for (k = 0; k < 27u; k++)
        if (GN_WHITE[k] == c)
            return k;
    return 0;
}

static void t_seq1_ring(void)
{
    static const uint8_t RING[6] = {GP_PLAY, GP_GROOVES, GP_LOOP, GP_STEPS, GP_MIX, GP_HOME};
    uint32_t i;
    int16_t bpm;
    host_easy();
    bpm = song.g[G_BPM];
    CHECK(gn.page == GP_HOME, "(on Home)");
    for (i = 0; i < 6u; i++) {
        host_turn(EN_SELECT, 1);
        ui_draw();
        CHECK(gn.page == RING[i], "SELECT, a detent: the next page of the ring (HOME SOUND GROOVES LOOP STEPS MIX; SONG hidden: round FLOW-1)");
        CHECK(gs.ring == (i + 1u) % 6u + 1u, "..its dot filled in the header");
    }
    CHECK(song.g[G_BPM] == bpm, "..SELECT alone never moves the tempo");
    host_turn(EN_SELECT, -1);
    CHECK(gn.page == GP_MIX, "back a detent from Home: MIX (round)");
    host_turn(EN_SELECT, -3);
    CHECK(gn.page == GP_GROOVES, "a fast turn, three detents in a frame: three pages");
    CHECK(gn_view() == GV_GROOVES, "GROOVES: its page (the shelf: round FLOW-1, M2)");
    host_turn(EN_SELECT, 3);
    CHECK(gn.page == GP_MIX, "..three on: MIX (no SONG stop)");
    seq_to(GP_RECORD);
    host_turn(EN_SELECT, 1);
    CHECK(gn.page == GP_MIX, "step entry sits where STEPS does: one on is MIX");
    host_tap(B_HOME);
    CHECK(gn.page == GP_HOME, "HOME still goes Home");
    host_tap(B_SEQ);
    CHECK(gn.page == GP_LOOP, "SEQ still opens the loop");
}

static void t_seq1_tempo(void)
{
    int16_t bpm;
    uint32_t n0;
    host_easy();
    bpm = song.g[G_BPM];
    n0 = lys.ntap;
    host_btn(B_EDIT, 1);
    host_frame();
    host_turn(EN_SELECT, 5);
    ui_draw();
    CHECK(song.g[G_BPM] == bpm + 5 && gn_view() == GV_TAP && gs.tempo && gs.bpm == bpm + 5,
          "EDIT held + SELECT: the tempo a beat a detent, its big number");
    CHECK(gn.page == GP_HOME, "..the page stays");
    host_wait_ms(GN_TAP_MS + 500u);
    CHECK(gn_view() == GV_TAP, "..the number stays while EDIT is held");
    host_turn(EN_SELECT, -2);
    CHECK(song.g[G_BPM] == bpm + 3, "..and down");
    host_btn(B_EDIT, 0);
    host_frame();
    CHECK(gn_view() == GV_HOME && song.g[G_BPM] == bpm + 3 && lys.ntap == n0, "let go: the page; EDIT's press counted no tap");
    host_tap(B_EDIT);
    CHECK(gn_view() == GV_TAP && !gn.tempo_hold, "EDIT tapped: tap tempo, as before");
}

static void t_seq1_loop(void)
{
    uint32_t t;
    host_easy();
    host_turn(EN_ALGO, 2);                           /* track 1: the Organ, a note at places 1 and 9 */
    host_wait_ms(GN_PICK_MS + 100u);
    seq_to(GP_LOOP);
    host_turn(EN_K1, -3);
    host_turn(EN_SELECT, 1);
    key(wkey(0), 40);
    key(wkey(8), 40);
    host_tap(B_LFO);                                 /* track 4: the kit, its first drum at 1 5 9 13 */
    host_turn(EN_ALGO, -1);
    host_wait_ms(GN_PICK_MS + 100u);
    host_turn(EN_SELECT, -1);
    host_turn(EN_K1, -3);
    host_turn(EN_SELECT, 1);
    for (t = 0; t < 16u; t += 4u)
        key(wkey(t), 40);
    host_turn(EN_SELECT, -1);
    ui_draw();
    CHECK(gn_view() == GV_LOOP && gs.trk == 3u, "LOOP: track 4 selected");
    CHECK(gs.ln_occ[0] == 0x0101u && gs.ln_occ[3] == 0x1111u && !gs.ln_occ[1] && !gs.ln_occ[2],
          "..every track's lane: where each sounds (track 1 at 1 and 9, the kit at 1 5 9 13)");
    CHECK(gs.drums && gs.trows == 1u && gs.trow[0] == 0x1111u, "..the tall kit lane: its drums as rows");
    CHECK(gs_lane_h(3) == 44 && gs_lane_h(0) == 18 && gs_lane_y(3) + 44 <= 207, "..the selected lane tall, the others slim, all in the field");
    for (t = 0; t < NTRK; t++)
        CHECK(gs.ln_col[t] == 0xFFu, "..stopped: no playhead");
    host_tap(B_PLAY);
    host_wait_ms(400);
    ui_draw();
    CHECK(gs.ln_col[3] != 0xFFu && gs.ln_col[0] == gs.ln_col[3], "PLAY: one playhead, the same place in every lane");
    CHECK(gs.ln_col[3] == ((trk[3].seq_idx % (uint32_t)trk[3].p[P_SLEN]) % 16u), "..where the track plays");
    host_tap(B_FX);
    ui_draw();
    CHECK(gs.trk == 0u && !gs.drums && gs.cnote[0] > 1u && gs.cnote[8] > 1u, "track 1 selected: its lane tall, its notes");
    host_tap(B_PLAY);
}

static void t_seq1_steps(void)
{
    char l[12], v[12];
    uint32_t p0, i;
    host_easy();
    host_turn(EN_ALGO, 2);
    host_wait_ms(GN_PICK_MS + 100u);
    seq_to(GP_LOOP);
    host_turn(EN_K1, -3);
    host_turn(EN_K1, 1);                             /* two bars */
    host_turn(EN_SELECT, 1);
    CHECK(gn.page == GP_STEPS && gn.steps_keys && gn.steps_cells == 16u, "SELECT from LOOP: STEPS, the tap grid live");
    CHECK(gn_knob_role(0) == KR_BAR && gn_knob_role(1) == KR_PEN, "..K1 the bar, K2 the pen note (a melody)");
    p0 = gn_pen();
    host_turn(EN_K2, 2);
    CHECK(gn_pen() == p0 + 2u && gn_key(&trk[0], wkey(3), 99) == p0 + 2u, "K2: the pen note, what a white key sounds");
    CHECK(gn_key(&trk[0], 1, 99) == GN_KEY_REST, "..a black key rests on STEPS");
    key(wkey(4), 40);
    CHECK(gn_seq_pitch(0, 4) == p0 + 2u && gn.undo_of == 1u, "tap the fifth key: place 5 on, the pen note, an undo copy");
    key(wkey(4), 40);
    CHECK(!gn_seq_sounds(0, 4), "..tap it again: off");
    key(wkey(4), 40);
    host_key(wkey(4), 1);                            /* hold it: the step's four on K1..K4 */
    host_frame();
    CHECK(gn_knob_role(0) == KR_HNOTE && gn_knob_role(1) == KR_HVEL && gn_knob_role(2) == KR_HLEN && gn_knob_role(3) == KR_HWHEN,
          "hold a key: K1..K4 are that step's NOTE STRENGTH LENGTH WHEN");
    host_turn(EN_K1, 1);
    host_turn(EN_K2, -5);
    host_turn(EN_K3, 2);
    host_turn(EN_K4, 2);
    ui_draw();
    CHECK(gn_seq_pitch(0, 4) == p0 + 3u && gn_seq_vel(0, 4) == 80u && gn_seq_glen(0, 4) == 3u && gn_seq_when(0, 4) == 50u,
          "..a semitone up, strength 80, three steps long (ties), SOMETIMES");
    gs_knob_text(3, l, v);
    CHECK(str_eq(l, "WHEN") && str_eq(v, "SOMETIMES") && gs.hold == 5u && gs.lit == 15u && gs.hlit == 3u,
          "..the cards say so, all four lit; the held place outlined in K4's colour (the last turned)");
    gs_knob_text(2, l, v);
    CHECK(str_eq(l, "LENGTH") && str_eq(v, "3 STEPS"), "..LENGTH 3 STEPS");
    host_key(wkey(4), 0);
    host_wait_ms(30);
    CHECK(gn_seq_sounds(0, 4) && gn_knob_role(1) == KR_PEN, "let go after turning: the step stays; the cards are the page's");
    host_turn(EN_K1, 1);                             /* bar 2 */
    key(wkey(0), 40);
    CHECK(gn_seq_sounds(0, 16), "K1 to bar 2: the first key is step 17");
    host_btn(B_SAVE, 1);
    host_wait_ms(900);
    host_btn(B_SAVE, 0);
    host_wait_ms(30);
    CHECK(!gn_seq_sounds(0, 4) && !gn_seq_sounds(0, 16) && !gn_seq_tied(0, 5), "SAVE held: the taps undone");
    (void)i;
    host_tap(B_REC);                                 /* armed: the keys play, they do not tap */
    CHECK(!gn.steps_keys && gn_key(&trk[0], wkey(3), 99) != p0 + 3u, "REC armed: the keys are notes again");
}

static void t_seq1_steps_kit(void)
{
    host_easy();
    host_tap(B_LFO);
    host_turn(EN_ALGO, -1);
    host_wait_ms(GN_PICK_MS + 100u);
    seq_to(GP_LOOP);
    host_turn(EN_K1, -3);
    host_turn(EN_SELECT, 1);
    CHECK(gn_knob_role(1) == KR_DRUM, "a kit's STEPS: K2 the drum");
    host_turn(EN_K2, 2);
    CHECK(gn_key(&trk[3], wkey(5), 99) == gn_kit_note(3, 2), "..a white key sounds K2's drum");
    key(wkey(0), 40);
    key(wkey(8), 40);
    CHECK(gn_seq_drum(3, 0, 2) && gn_seq_drum(3, 8, 2), "tap: K2's drum at places 1 and 9");
    host_turn(EN_K2, -2);
    key(wkey(0), 40);
    CHECK(gn_seq_drum(3, 0, 0) && gn_seq_drum(3, 0, 2), "another drum on the same step: both play");
    ui_draw();
    CHECK((gs.pon & 0x0101u) == 0x0001u && gs.podr[0] == 1u && gs.podr[8] == 1u, "..the page: K2's drum lit, a dot for the other");
    key(wkey(0), 40);
    CHECK(!gn_seq_drum(3, 0, 0) && gn_seq_drum(3, 0, 2), "tap it again: that drum off, the other kept");
    host_key(wkey(8), 1);
    host_frame();
    CHECK(gn_knob_role(0) == KR_REST && gn_knob_role(1) == KR_HVEL, "a kit's held step: no NOTE; STRENGTH on K2");
    host_key(wkey(8), 0);
    host_wait_ms(30);
    CHECK(gn_seq_drum(3, 8, 0) && gn_seq_drum(3, 8, 2), "..pressed where K2's drum was off: on, and stays on let go");
}

static uint16_t seq1_was[240 * 240];
static int seq1_green(uint16_t c)                   /* a pixel of the canvas (byte-swapped): green leads */
{
    uint32_t r, g, b;
    c = (uint16_t)((c >> 8) | (c << 8));
    r = ((c >> 11) & 31u) * 2u;
    g = (c >> 5) & 63u;
    b = (c & 31u) * 2u;
    return g > r && g >= b;
}
static void t_seq1_lit(void)
{
    uint32_t i, changed, green, cards;
    host_easy();
    host_turn(EN_ALGO, 2);
    host_wait_ms(GN_PICK_MS + 100u);
    seq_to(GP_LOOP);
    host_turn(EN_K1, -3);
    host_wait_ms(GN_LIT_MS + 100u);
    settle();
    CHECK(gn_view() == GV_LOOP && !gs.lit, "(LOOP, nothing turning)");
    memcpy(seq1_was, host_screen, sizeof seq1_was);
    host_turn(EN_K2, 1);
    settle();
    CHECK(gs.lit == 2u, "K2 turned: its card lit");
    for (i = 0, changed = green = cards = 0; i < 240u * 240u; i++) {
        uint32_t x = i % 240u, y = i / 240u;
        if (host_screen[i] == seq1_was[i])
            continue;
        if (y >= 31u && y < 69u && !(x >= 61u && x < 119u))
            cards++;
        if (y >= 93u && y < 208u && x >= 26u) {
            changed++;
            green += (uint32_t)seq1_green(host_screen[i]);
        }
    }
    CHECK(cards == 0u, "..no other card changed");
    CHECK(changed > 1000u && green * 10u >= changed * 9u, "..what SWING changes lit in the field in its colour (the off-beats green)");
    host_wait_ms(GN_LIT_MS + 100u);
    settle();
    for (i = 0, changed = 0; i < 240u * 240u; i++)
        changed += i / 240u >= 70u && i / 240u < 208u && host_screen[i] != seq1_was[i];
    CHECK(!gs.lit && changed == 0u, "1.2 s later: unlit, the field as it was");
}


/* ------------------------------------------------------------ FLOW-1 --- */
/* round FLOW-1: the performance transport, fills, the record words, the click's mark, a blank loop of
 * one bar */
static void flow_block(void) { host_audio_us((uint64_t)CTL * 1000000u / FS + 1u); }   /* one audio block */
static uint32_t flow_pat_sig(void)                   /* every track's steps, LEN and DIV */
{
    uint32_t t, i, h = 2166136261u;
    for (t = 0; t < NTRK; t++) {
        const uint8_t *b = (const uint8_t *)trk[t].step;
        for (i = 0; i < sizeof trk[t].step; i++)
            h = (h ^ b[i]) * 16777619u;
        h = (h ^ (uint16_t)trk[t].p[P_SLEN]) * 16777619u;
        h = (h ^ (uint16_t)trk[t].p[P_SDIV]) * 16777619u;
    }
    return h;
}
static void flow_two_bars(void)                       /* every track two bars of 1/16, a note on each step of track 1 */
{
    uint32_t t, i;
    for (t = 0; t < NTRK; t++) {
        trk[t].p[P_SDIV] = GN_DIV16;
        trk[t].p[P_SLEN] = 32;
    }
    for (i = 0; i < 32u; i++) {
        step_clear(&trk[0].step[i]);
        trk[0].step[i].time = ST_NOTE;
        trk[0].step[i].n = 1;
        trk[0].step[i].note[0] = 60;
    }
}
static uint32_t flow_bar_blocks(void) { return 4u * beat_samples() / CTL; }   /* a bar, in audio blocks */
/* blocks until song.tick goes back (a restart) or the loop stops; the tick it had then */
static uint32_t flow_until(int stop, uint32_t max, uint32_t *at)
{
    uint32_t b, last = song.tick;
    for (b = 0; b < max; b++) {
        flow_block();
        if (stop ? !song.playing : song.tick < last) {
            *at = last;
            return b;
        }
        last = song.tick;
    }
    *at = last;
    return max;
}
static void flow_words(char *b) { ui_draw(); gs_state_words(b); }

static void t_flow1_transport(void)
{
    char b[32];
    uint32_t sig, at, bar, idx;
    host_easy();
    flow_two_bars();
    sig = flow_pat_sig();
    host_tap(B_PLAY);                                /* stopped: PLAY starts on the press, as before */
    CHECK(song.playing, "stopped, PLAY: the loop plays");
    bar = flow_bar_blocks();
    while (song.tick < bar / 3u)
        host_frame();
    host_btn(B_PLAY, 1);                             /* held 0.45 s in the first bar: RESTART NEXT BAR */
    host_wait_ms(GN_RESTART_MS + 50u);
    CHECK(gn.tq == GN_TQ_RESTART && song.playing, "PLAY held 0.4 s while playing: a restart queued, nothing stopped");
    flow_words(b);
    CHECK(str_eq(b, "RESTART NEXT BAR * HOME"), "..the band: RESTART NEXT BAR (only the transport word)");
    host_btn(B_PLAY, 0);
    host_frame();
    CHECK(song.playing && gn.tq == GN_TQ_RESTART, "..let go: no stop, still queued");
    flow_until(0, 4u * bar, &at);
    idx = trk[0].seq_idx;
    CHECK(at + 2u >= bar && at <= bar + 1u, "..the restart lands on the bar line (the first bar's end, within a block)");
    CHECK(idx == 0u && song.playing && !gn.tq, "..from the top: step 0 again, playing, nothing queued");
    CHECK(flow_pat_sig() == sig, "..no step, LEN or DIV changed");
    host_wait_ms(300);
    host_btn(B_PLAY, 1);                             /* held, and held again before the bar: NOW */
    host_wait_ms(GN_RESTART_MS + 50u);
    host_btn(B_PLAY, 0);
    host_frame();
    host_btn(B_PLAY, 1);
    host_wait_ms(GN_RESTART_MS + 30u);
    CHECK(!gn.tq && song.tick < 60u, "PLAY held again before the bar: the restart now (the NOW escape)");
    host_btn(B_PLAY, 0);
    host_wait_ms(300);
    host_btn(B_GLO, 1);                              /* GLO held + PLAY: STOP AT BAR END */
    host_frame();
    host_tap(B_PLAY);
    CHECK(gn.tq == GN_TQ_STOP && song.playing, "GLO held + PLAY: a stop queued for the bar's end, still playing");
    flow_words(b);
    CHECK(str_eq(b, "STOP AT BAR END * HOME"), "..the band: STOP AT BAR END");
    host_btn(B_GLO, 0);
    host_frame();
    CHECK(gn.page == GP_HOME, "..GLO used with PLAY: its release is no tap (no MIX)");
    flow_until(1, 4u * bar, &at);
    CHECK(at + 2u >= bar && at <= bar + 1u && trk[0].seq_idx % 16u == 15u,
          "..the loop stops on the bar line: the bar's last step played, the next bar's first never");
    CHECK(flow_pat_sig() == sig, "..no step, LEN or DIV changed");
    host_tap(B_PLAY);
    host_wait_ms(500);
    host_btn(B_GLO, 1);                              /* GLO + PLAY twice: stop now */
    host_frame();
    host_tap(B_PLAY);
    host_tap(B_PLAY);
    host_frame();
    CHECK(!song.playing && !gn.tq, "GLO held + PLAY again before the bar: the stop now");
    host_btn(B_GLO, 0);
    host_frame();
    host_tap(B_PLAY);
    host_wait_ms(500);
    host_tap(B_PLAY);                                /* a plain tap: stops at once, when let go */
    host_frame();
    CHECK(!song.playing && !gn.tq, "a PLAY tap while playing: stopped at once (no queue)");
    host_tap(B_GLO);
    CHECK(gn.page == GP_MIX, "GLO tapped alone: the volumes, as before");
    host_tap(B_SEQ);
    host_tap(B_PLAY);                                /* from any page: the loop page too */
    host_btn(B_PLAY, 1);
    host_wait_ms(GN_RESTART_MS + 50u);
    host_btn(B_PLAY, 0);
    CHECK(gn.tq == GN_TQ_RESTART, "..on the loop page too");
    host_tap(B_PLAY);
    host_frame();
    CHECK(!gn.tq && !song.playing, "..a tap stops, and nothing stays queued");
    host_tap(B_PLAY);
    host_btn(B_PLAY, 1);
    host_wait_ms(GN_RESTART_MS + 50u);
    host_btn(B_PLAY, 0);
    host_frame();
    host_hold_switch(2100);                          /* (a restart queued, then into Advanced) */
    CHECK(!gn.on && !gn.tq, "leaving Easy with a restart queued: nothing queued for Advanced's transport");
}

static void t_flow1_fill(void)
{
    uint32_t sig, b, hits = 0, ongrid = 0, i, q, voices;
    host_easy();
    host_tap(B_LFO);                                 /* track 4: the kit */
    flow_two_bars();
    sig = flow_pat_sig();
    host_tap(B_PLAY);
    host_wait_ms(300);
    host_btn(B_GLO, 1);
    host_frame();
    host_key(6, 1);                                  /* GLO held + a key (pad 4): a fill */
    host_frame();
    CHECK(gn.perf_n == 1u && gn.perf_trk == 3u, "GLO held + a key: a fill on the selected track");
    q = beat_samples() / 4u;
    for (b = 0; b < 600u; b++) {                     /* ~0.4 s at 120 BPM: a sixteenth is 125 ms */
        flow_block();
        if (gn.perf_age == CTL) {
            hits++;
            ongrid += beat_pos % q < CTL;
        }
    }
    voices = 0;
    for (i = 0; i < NVOICE; i++)
        voices += trk[3].v[i].active;
    CHECK(hits >= 3u && hits <= 5u && ongrid == hits, "..it rolls a sixteenth at a time, on the beat's grid");
    CHECK(voices, "..the track sounds");
    host_key(6, 0);
    host_frame();
    CHECK(!gn.perf_n, "..the key let go: the fill ends");
    host_btn(B_GLO, 0);
    host_frame();
    CHECK(gn.page == GP_HOME, "..GLO used for a fill: no MIX when let go");
    CHECK(flow_pat_sig() == sig, "..nothing written to the pattern");
    host_tap(B_REC);                                 /* recording: a fill is never recorded */
    for (i = 0; i < 400u && !song.rec; i++)
        host_frame();
    sig = flow_pat_sig();
    host_btn(B_GLO, 1);
    host_frame();
    host_key(9, 1);
    host_wait_ms(600);
    host_btn(B_GLO, 0);                              /* GLO let go first: the fill ends too */
    host_frame();
    CHECK(!gn.perf_n, "GLO let go with the key still down: the fill ends");
    host_key(9, 0);
    host_frame();
    CHECK(song.rec && flow_pat_sig() == sig && !gn.take_keys, "..while recording: the fill recorded nothing");
    host_tap(B_REC);
    host_tap(B_SEQ);
    host_tap(B_SEQ);                                 /* STEPS, stopped: the keys are steps, but not with GLO held */
    host_tap(B_PLAY);
    sig = flow_pat_sig();
    host_btn(B_GLO, 1);
    host_frame();
    host_key(0, 1);
    host_wait_ms(100);
    host_key(0, 0);
    host_btn(B_GLO, 0);
    host_frame();
    CHECK(gn.page == GP_STEPS && flow_pat_sig() == sig, "..on STEPS: GLO + a key is a fill, never a step");
}

static void t_flow1_words(void)
{
    uint32_t x, y, inked = 0;
    host_easy();
    favorites.factory[15][30] = GN_MODE_MARK | GN_MODE_EASY;   /* (the click on, as the firmware's record has it) */
    host_tap(B_REC);
    host_tap(B_PLAY);
    host_wait_ms(300);
    CHECK(gn.arm == GA_COUNT, "(the count-in)");
    ui_draw();
    settle();
    CHECK(str_eq(gs.head, "COUNT-IN 4") || str_eq(gs.head, "COUNT-IN 3"), "the header names the count-in's beat");
    for (y = 6; y < 25u; y++)
        for (x = 159; x < 173u; x++) {
            uint16_t c = host_screen[y * 240u + x];
            inked += (uint16_t)((c >> 8) | (c << 8)) == GX_INK;
        }
    CHECK(inked > 10u, "..and the click's mark beside the tempo (the click is on)");
    while (!song.rec)
        host_frame();
    ui_draw();
    CHECK(str_eq(gs.head, "RECORDING T1"), "recording: the header names the state and the track");
    CHECK(trk[0].p[P_SLEN] == (int16_t)(gn_spb((uint32_t)trk[0].p[P_SDIV])) || !seq_is_empty(&trk[0]),
          "..a blank loop is one bar (spec S-5)");
    host_btn(B_EDIT, 1);
    host_frame();
    ui_draw();
    CHECK(str_eq(gs_hint("x"), "CLICK ON (EDIT + K3)"), "EDIT held: the band says CLICK ON");
    host_turn(EN_K3, -1);
    ui_draw();
    CHECK(str_eq(gs_hint("x"), "CLICK OFF (EDIT + K3)") && !gn_click_on(), "..K3 left: CLICK OFF, said");
    host_btn(B_EDIT, 0);
    host_frame();
}


static uint32_t flow_wkey(uint32_t c)                /* the white key of place c */
{
    uint32_t k;
    for (k = 0; k < 27u && GN_WHITE[k] != c; k++)
        ;
    return k;
}
static uint32_t flow_voices(uint32_t t)
{
    uint32_t i, n = 0;
    for (i = 0; i < NVOICE; i++)
        n += trk[t].v[i].active && trk[t].v[i].gate;
    return n;
}

static void t_flow1_audition(void)
{
    uint32_t sig, i;
    uint8_t a2;
    host_easy();
    host_tap(B_ENV);                                 /* track 3 */
    host_turn(EN_ALGO, 1);                           /* the Gnome there: an Easy sound */
    host_tap(B_HOME);
    host_tap(B_SEQ);                                 /* the loop page */
    sig = flow_pat_sig();
    host_btn(B_FX, 1);
    host_frame();
    ui_draw();
    CHECK(str_eq(gs_hint("x"), "AUDITION (KEYS)  SOUND (K1-K4)"), "FX held: the band says AUDITION");
    host_key(flow_wkey(2), 1);
    host_wait_ms(60);
    CHECK(gn.perf_n == 1u && !gn.perf_roll && flow_voices(2), "FX held + a key: the selected track's sound, held (an audition)");
    host_wait_ms(600);
    CHECK(flow_voices(2), "..it holds while both are held (no roll)");
    a2 = gn.amount[2][2];
    host_turn(EN_K3, 2);
    CHECK(gn.amount[2][2] == a2 + 2u && gn.page == GP_LOOP, "..K3 on the loop page while FX is held: the sound's ECHO");
    host_key(flow_wkey(2), 0);
    host_frame();
    host_btn(B_FX, 0);
    host_frame();
    CHECK(!gn.perf_n && song.sel == 2u, "FX let go: the audition over, the track never selected (track 3 still)");
    CHECK(flow_pat_sig() == sig, "..nothing written");
    host_tap(B_FX);
    CHECK(song.sel == 0u, "FX tapped: track 1, as before (when let go)");
    host_tap(B_ENV);
    host_tap(B_REC);                                 /* recording: an audition records nothing */
    host_tap(B_PLAY);
    for (i = 0; i < 400u && !song.rec; i++)
        host_frame();
    sig = flow_pat_sig();
    host_btn(B_FX, 1);
    host_frame();
    host_key(flow_wkey(4), 1);
    host_wait_ms(300);
    host_key(flow_wkey(4), 0);
    host_btn(B_FX, 0);
    host_frame();
    CHECK(song.rec && flow_pat_sig() == sig && !gn.take_keys && song.sel == 2u, "..recording: nothing recorded, no track selected");
    host_tap(B_REC);
    host_tap(B_PLAY);
    seq_to(GP_STEPS);                                /* STEPS: FX + a key is the sound, never a step */
    sig = flow_pat_sig();
    host_btn(B_FX, 1);
    host_frame();
    host_key(flow_wkey(0), 1);
    host_wait_ms(60);
    CHECK(flow_voices(2), "STEPS, FX held + a key: it sounds");
    host_key(flow_wkey(0), 0);
    host_btn(B_FX, 0);
    host_frame();
    CHECK(flow_pat_sig() == sig, "..and no step written");
}

static void t_flow1_grooves(void)
{
    uint32_t i, at;
    host_easy();
    host_turn(EN_SELECT, 2);                         /* HOME, SOUND, GROOVES */
    CHECK(gn.page == GP_GROOVES && gn_view() == GV_GROOVES, "SELECT twice from Home: GROOVES, the shelf");
    key(flow_wkey(0), 60);                           /* key 1: HEAD NOD, stopped: loads and plays */
    host_frame();
    CHECK(gn.tile == 1u && (gn.grv & GN_GV_NUM) == 1u && song.playing && song.g[G_BPM] == 90, "stopped, key 1: HEAD NOD loads and plays");
    CHECK(!flow_voices(0) || 1, "(the tile keys rest)");
    host_wait_ms(500);
    key(flow_wkey(1), 60);                           /* key 2, playing: queued for the next bar */
    CHECK(gn.tile_q == 2u && gn.tq == GN_TQ_STOP && song.playing && gn.tile == 1u, "playing, key 2: FLOOR FILLER queued for the next bar");
    {
        char b[32];
        ui_draw();
        gs_state_words(b);
        CHECK(str_eq(b, "GROOVE NEXT BAR * GROOVES"), "..the band: GROOVE NEXT BAR");
    }
    for (i = 0; i < 400u && gn.tile != 2u; i++)
        host_frame();
    host_frame();
    CHECK(gn.tile == 2u && song.playing && song.g[G_BPM] == 124 && !gn.tile_q, "..on the bar line it loads and plays from the top");
    host_wait_ms(300);
    key(flow_wkey(2), 60);
    at = song.tick;
    key(flow_wkey(2), 60);                           /* the same key again: now */
    for (i = 0; i < 6u && gn.tile != 3u; i++)
        host_frame();
    CHECK(gn.tile == 3u && song.playing, "key 3, and again: RAINY DESK now");
    (void)at;
    trk[0].step[1].time = ST_NOTE;                   /* the player's own step: not kept */
    trk[0].step[1].n = 1;
    trk[0].step[1].note[0] = 61;
    host_wait_ms(100);
    key(flow_wkey(0), 60);
    CHECK(gn.tile_ask == 1u && !gn.tile_q && gn.tile == 3u, "playing over music not kept: key 1 asks first (STOP, OR PRESS AGAIN)");
    key(flow_wkey(0), 60);
    CHECK(gn.tile_q == 1u && !gn.tile_ask, "..pressed again: queued anyway");
    for (i = 0; i < 400u && gn.tile != 1u; i++)
        host_frame();
    CHECK(gn.tile == 1u, "..and it loads on the bar");
    host_tap(B_PLAY);
    host_frame();
    trk[0].step[1].time = ST_NOTE;                   /* stopped, not kept: Easy keeps it first, then loads */
    trk[0].step[1].n = 1;
    trk[0].step[1].note[0] = 62;
    for (i = 0; i < 4u && project_used(i); i++)
        ;
    CHECK(i < 4u, "(an empty slot)");
    key(flow_wkey(1), 60);
    host_frame();
    CHECK(project_used(i) && gn.tile == 2u && song.playing, "stopped over music not kept: saved first (MINE), then the tile");
    CHECK(gn.mine == 0u || 1, "(the MINE tiles read now and then)");
    host_wait_ms(200);
    key(flow_wkey(GN_NGROOVES + i), 60);             /* that MINE tile: queued, then the song from the slot */
    for (i = 0; i < 400u && gn.tile <= GN_NGROOVES; i++)
        host_frame();
    host_frame();
    CHECK(gn.tile > GN_NGROOVES && song.playing && trk[0].step[1].note[0] == 62u, "a MINE tile: the saved song loads on the bar and plays");
    key(flow_wkey(8), 60);                           /* MINE 4: empty */
    CHECK(gn.tile_ask == (0x40u | 9u), "an empty MINE says so (EMPTY)");
}


static void flow_undo(void)                           /* SAVE held 0.8 s, let go */
{
    host_btn(B_SAVE, 1);
    host_wait_ms(800);
    host_btn(B_SAVE, 0);
    host_frame();
    ui_draw();
}

/* the merge of round EW and flow-1, on the Gnomad engines: the GROOVES tiles load each groove on
 * its own kit with its shape on EDIT 2; a fill on a Gnomad kit's black key rolls that key's own drum (round EW-FIXES:
 * a ghost of a drum beside it); an audition on the Gnome keeps his syllables (TALK 12: the core sings each key) and writes nothing of his */
#ifdef GN_UX_ENGINES
#define EW_KIT(name) EW_KIT_(GN_CONTENT_KN_##name)
#define EW_KIT_(...) EW_KIT__(__VA_ARGS__)
#define EW_KIT__(slot, k) (k)
#endif
static void t_flow1_ew(void)
{
#ifdef GN_UX_ENGINES
    uint32_t i, k, note;
    int16_t e0, e1;
    host_easy();
    host_turn(EN_SELECT, 2);                         /* GROOVES */
    key(flow_wkey(0), 60);                           /* key 1, stopped: HEAD NOD now */
    host_frame();
    CHECK(gn.tile == 1u && gn_gnomad_kit(3) && trk[3].preset == EW_KIT(BOOM_BAP) && trk[3].p[P_E4] == 50 - 64 &&
          trk[3].p[P_E5] == 0 && trk[3].p[P_E6] == 48 - 64 && !trk[3].p[P_E7],
          "GROOVES key 1: HEAD NOD on its own kit (Boom Bap), its TONE DECAY SNAP on EDIT 2");
    host_wait_ms(500);
    key(flow_wkey(1), 60);                           /* key 2, playing: the next bar; again: now */
    key(flow_wkey(1), 60);
    for (i = 0; i < 400u && gn.tile != 2u; i++)
        host_frame();
    CHECK(gn.tile == 2u && trk[3].preset == EW_KIT(HOUSE) && trk[3].p[P_E5] == 52 - 64,
          "..key 2 (again: now): FLOOR FILLER on House, its shape");
    host_tap(B_PLAY);
    host_tap(B_HOME);
    host_tap(B_LFO);                                 /* track 4: the kit, a Gnomad kit */
    for (k = 0; k < 27u && GN_WHITE[k] != 0xFFu; k++)
        ;                                            /* (the first black key) */
    host_btn(B_GLO, 1);
    host_frame();
    host_key(k, 1);
    host_frame();
    note = gn.perf_n ? gn.perf_note[0] : 0u;
    CHECK(gn.perf_n == 1u && gn.perf_trk == 3u && note == gn_black_pad(&trk[3], k) && note == 4u,
          "GLO held + a black key on a Gnomad kit: a fill of that key's own drum (round EW-FIXES: the first black key a "
          "soft KICK, the ghost block's note 4)");
    host_key(k, 0);
    host_btn(B_GLO, 0);
    host_frame();
    host_tap(B_ENV);                                 /* track 3: the Gnome (Easy's, as ALGORITHM puts him) */
    gn_sound_put(2, GN_GROUP[0]);
    host_tap(B_HOME);
    e0 = trk[2].p[P_E0];
    e1 = trk[2].p[P_E1];
    CHECK(gnc_engine(trk[2].eng_req) == GC_GNOME && trk[2].p[P_E7] == 12, "Easy's Gnome: TALK 12 (the core's syllables)");
    host_btn(B_FX, 1);
    host_frame();
    host_key(flow_wkey(3), 1);
    host_wait_ms(60);
    CHECK(gn.perf_n == 1u && gn.perf_note[0] == gn_key_note(2, flow_wkey(3)) && flow_voices(2),
          "..FX held + a key: an audition of the Gnome, the key's own note");
    host_key(flow_wkey(3), 0);
    host_btn(B_FX, 0);
    host_frame();
    CHECK(trk[2].p[P_E7] == 12 && trk[2].p[P_E0] == e0 && trk[2].p[P_E1] == e1,
          "..his syllables as they were (TALK 12), his Bright and Wobble untouched by the keys");
#endif
}

static void t_flow1_undo_named(void)
{
    uint32_t i;
    host_easy();
    host_turn(EN_ALGO, 2);                           /* the Organ on track 1 */
    host_tap(B_HOME);
    host_tap(B_SEQ);
    host_tap(B_REC);
    host_tap(B_PLAY);
    for (i = 0; i < 400u && !song.rec; i++)
        host_frame();
    key(7, 100);
    host_tap(B_REC);
    host_tap(B_PLAY);
    host_wait_ms(100);
    flow_undo();
    CHECK(str_eq(gs_hint("x"), "UNDID: LAST TAKE"), "SAVE held after a take: the band says UNDID: LAST TAKE");
    host_wait_ms(2100);
    ui_draw();
    CHECK(!str_eq(gs_hint("x"), "UNDID: LAST TAKE"), "..for 2 s");
    host_turn(EN_K1, 1);                             /* BARS 1 -> 2 */
    flow_undo();
    CHECK(str_eq(gs_hint("x"), "UNDID: BARS"), "after BARS: UNDID: BARS");
    host_wait_ms(2100);
    seq_to(GP_STEPS);
    key(flow_wkey(3), 60);                           /* a step tapped on STEPS */
    flow_undo();
    CHECK(str_eq(gs_hint("x"), "UNDID: STEPS"), "after a step tapped: UNDID: STEPS");
    host_wait_ms(2100);
    seq_to(GP_RECORD);                               /* step entry, stopped */
    key(7, 60);
    flow_undo();
    CHECK(str_eq(gs_hint("x"), "UNDID: STEP EDIT"), "after step entry: UNDID: STEP EDIT");
    host_wait_ms(2100);
    flow_undo();
    CHECK(str_eq(gs_hint("x"), "NOTHING TO UNDO"), "again, nothing left: NOTHING TO UNDO");
}

static void t_flow1_sound_changed(void)
{
    uint32_t tick;
    host_easy();
    host_tap(B_PLAY);
    host_wait_ms(500);
    tick = song.tick;
    host_turn(EN_ALGO, 1);
    ui_draw();
    CHECK(str_eq(gs_hint("x"), "TRACK 1 * SOUND CHANGED") && song.playing && song.tick > tick,
          "ALGORITHM while playing: TRACK 1 * SOUND CHANGED, the loop playing on");
    host_wait_ms(1600);
    ui_draw();
    CHECK(!gs.chg, "..for 1.5 s");
    host_tap(B_SCL);
    host_turn(EN_ALGO, 2);
    host_turn(EN_PRESET, 1);
    ui_draw();
    CHECK(str_eq(gs_hint("x"), "TRACK 2 * SOUND CHANGED") && song.playing, "PRESETS on track 2: TRACK 2 * SOUND CHANGED");
    host_tap(B_PLAY);
    host_frame();
    host_turn(EN_ALGO, 1);
    ui_draw();
    CHECK(!gs.chg, "stopped: no confirmation needed (the wheel says it)");
}

static void t_flow1_other_sound(void)
{
    host_easy();
    host_tap(B_SCL);                                 /* track 2: Felucca's (another) sound */
    host_tap(B_HOME);                                /* (HOME on Home: the sound page) */
    ui_draw();
    CHECK(gn_view() == GV_PLAY && gs.pic >= GN_NPICS, "(track 2's sound page: another sound)");
    CHECK(str_eq(GS_HINT_OTHER, "NOT EASY: PICK ONE (ALGORITHM)"), "..the band says it is not Easy's and how to pick one");
}

static const struct {
    const char *name;
    void (*run)(void);
} CASES[] = {
    {"UX-1 Home: entered on Home; HOME toggles Home and the sound; GLO and HOME come back to it", t_ux1_home},
    {"UX-1 Home's rows: level, MUTE, selected, sounding; the sound's knobs; one row redrawn", t_ux1_rows},
    {"UX-1 Home: an empty kit wears the Gnome's hat, a hit takes it off", t_ux1_hat},
    {"UX-2 the loop: a block a bar, where it sounds, the playhead; BARS SWING SNAP VOLUME", t_ux2_loop},
    {"UX-2 the steps: the bar switch (K1), following the playhead, note names with sharps", t_ux2_steps},
    {"UX-2 a kit: step entry with K2 picking the drum, NOTE / REST, the drum list", t_ux2_drums},
    {"UX-2 REC arms, PLAY from step entry opens the loop; elsewhere PLAY keeps the page", t_ux2_play_opens},
    {"UX-3 the wheel and the peek list over the page; the sound page's four named knobs", t_ux3_browse},
    {"UX-3 the kit: banks A and B on OCT- / OCT+ (the octave untouched), pads lit, the Gnomad kit's keys", t_ux3_kit},
    {"UX-3 SHIFT: EDIT held 0.5 s, its tap taken back; no deep page in Easy (round FLOW-1); let go, back", t_ux3_shift},
#if GN_ENGINES
    {"UX-3 the Gnomad FM: SHIFT shows no deep page in Easy (round FLOW-1, M2)", t_ux3_fm},
#endif
    {"UX-4 page identity and the selected track across the switch, both ways; the Gnome only on Home", t_ux4_identity},
    {"UX-4 a mode switch never converts a project: an off-grid track byte for byte, every page visited", t_ux4_never_converts},
    {"UX-4 SHIFT's depth: MUTE on Home, a step's note and strength (undone), pan on the volumes", t_ux4_shift},
    {"UX-5 the save words: SAVED only when a save was read back the same; ON SYNTH otherwise, never a guess", t_ux5_words},
    {"UX-5 the status strip: what plays and where you are, on every page", t_ux5_strip},
    {"UX-6 Home: a selection move redraws both rows whole (the outline's right end too)", t_ux6_select},
#if GN_ENGINES
    {"UX-7 Advanced lists only the engines that play (order, PRESETS, EDIT keys, ENG); the editor's names", t_ux7_engines},
#endif
    {"SEQ-1 SELECT turns the page ring HOME SOUND GROOVES LOOP STEPS MIX; the buttons still jump", t_seq1_ring},
    {"SEQ-1 the tempo on EDIT held + SELECT, its big number while held; EDIT tapped is tap tempo", t_seq1_tempo},
    {"SEQ-1 LOOP layout A: four lanes, the selected one tall, one playhead through them", t_seq1_loop},
    {"SEQ-1 STEPS: keys are the bar's places; tap on, tap off; hold + K1..K4; undo; REC gives the keys back", t_seq1_steps},
    {"SEQ-1 STEPS on a kit: K2's drum tapped in and out, the other drums kept and shown", t_seq1_steps_kit},
    {"SEQ-1 the knob turning lights its card and, in its colour, only what it changes", t_seq1_lit},
    {"FLOW-1 PLAY held: restart at the next bar (again: now); GLO + PLAY: stop at the bar's end; a tap stops", t_flow1_transport},
    {"FLOW-1 GLO held + keys: a fill on the beat's grid, never written, never recorded, never a step", t_flow1_fill},
    {"FLOW-1 the record words (header and band), the click's mark, EDIT held says CLICK ON / OFF", t_flow1_words},
    {"FLOW-1 M2 FX held + keys: an audition of the track's sound, its knobs on every page, nothing selected or written", t_flow1_audition},
    {"FLOW-1 M2 GROOVES: nine tiles, a key loads on the next bar (again: now), never over music not kept, MINE", t_flow1_grooves},
#ifdef GN_UX_ENGINES
    {"FLOW-1 x EW (engines): the grooves' own kits from the tiles, a black-key fill on a Gnomad kit, the Gnome's audition", t_flow1_ew},
#endif
    {"FLOW-1 M3 SAVE held names what it undid: LAST TAKE, BARS, STEPS, STEP EDIT, NOTHING TO UNDO", t_flow1_undo_named},
    {"FLOW-1 M3 a sound changed while playing: TRACK n * SOUND CHANGED, the transport untouched", t_flow1_sound_changed},
    {"FLOW-1 M3 another sound on the sound page: the band says so and how to pick an Easy one", t_flow1_other_sound},
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
    printf(failed ? "gn ux tests FAILED\n" : "gn ux tests passed\n");
    return failed ? 1 : 0;
}
