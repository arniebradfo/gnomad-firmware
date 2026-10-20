/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kyle Palaniuk */
/* Gnomad (FORK.md) host test: Easy mode's state machine, through the firmware's own code
 * (tests/gn_host.h: the sound, the sequencer, the UI, storage on a RAM flash, the editor, and the
 * fork's code), driven as a person drives the panel and as the app drives the editor. Each case starts
 * from power-on in a process of its own. Every finding of the review of the 0.9 Easy
 * (the M2 review) has a case that fails without its fix; so do the
 * white-key table and the pads on the lower row. tests/run_fork_tests.sh builds it with
 * -fsanitize=address,undefined: any read or write out of an object, in the audio renderer too, stops it.
 *   tests/run_fork_tests.sh builds it: build/host-gn/gn_easy_test */
#include "gn_host.h"
#include <sys/mman.h>
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

static int sounding(uint32_t t, uint32_t note)      /* a voice of track t holds the note */
{
    uint32_t i;
    for (i = 0; i < NVOICE; i++)
        if (trk[t].v[i].active && trk[t].v[i].gate && trk[t].v[i].note == note)
            return 1;
    return 0;
}

static uint32_t voices_on(uint32_t t)               /* voices of track t sounding (gate or not) */
{
    uint32_t i, n = 0;
    for (i = 0; i < NVOICE; i++)
        n += trk[t].v[i].active;
    return n;
}

static void host_ed_val(uint8_t *b, int32_t v)           /* an editor value: 14 bits around 8192 */
{
    b[0] = (uint8_t)((v + 8192) & 127);
    b[1] = (uint8_t)(((v + 8192) >> 7) & 127);
}

static void app_preset(uint32_t e, uint32_t pr)     /* the app: a factory sound on the selected track */
{
    uint8_t a[2] = {(uint8_t)e, (uint8_t)pr};
    host_request(ED_PRESET, a, 2);
}

/* round 24: the app's PRESET (e, pr) is one of Easy's 49 sounds, which Easy adopts as its pick */
static int easy_preset(uint32_t e, uint32_t pr)
{
    uint8_t a[2] = {(uint8_t)e, (uint8_t)pr};
    return gn_adopt_of(0, a, 2) != 0u;
}

static void app_set(uint32_t id, int32_t v)         /* the app: a SET of the selected track's id */
{
    uint8_t a[4] = {0, (uint8_t)id, 0, 0};
    host_ed_val(a + 2, v);
    host_request(ED_SET, a, 4);
}

static void app_project(uint32_t op, uint32_t slot)
{
    uint8_t a[2] = {(uint8_t)op, (uint8_t)slot};
    host_request(ED_PROJECT, a, 2);
}

static void app_track_step(uint32_t t, uint32_t i, uint32_t note)   /* the app: a step of track t */
{
    uint8_t a[10] = {(uint8_t)t, (uint8_t)i, 1, (uint8_t)note, 0, 0, 0, ST_NOTE, 0, 100};
    host_request(ED_TRACK_STEP, a, 10);
}

static void bk_u35(uint8_t *a, uint32_t v);

static void app_step_set(uint32_t i, uint32_t note)   /* the app: step i of the selected track (STEP_SET) */
{
    uint8_t a[9] = {(uint8_t)i, 1, (uint8_t)note, 0, 0, 0, ST_NOTE, 0, 100};
    host_request(ED_STEP_SET, a, 9);
}

static void app_track_param(uint32_t t, uint32_t id, int32_t v)   /* the app: track t's id (TRACK_PARAM) */
{
    uint8_t a[4] = {(uint8_t)t, (uint8_t)id, 0, 0};
    host_ed_val(a + 2, v);
    host_request(ED_TRACK_PARAM, a, 4);
}

static void app_engine(uint32_t e)                  /* the app: the selected track's engine (SET G_ENGSEL) */
{
    uint8_t a[4] = {1, G_ENGSEL, 0, 0};
    host_ed_val(a + 2, (int32_t)e);
    host_request(ED_SET, a, 4);
}

static void app_track_mix(uint32_t t, int32_t level, uint32_t mute)   /* the app: track t's level and mute */
{
    uint8_t a[4] = {(uint8_t)t, 0, 0, (uint8_t)(mute ? 1 : 0)};
    host_ed_val(a + 1, level);
    host_request(ED_TRACK_MIX, a, 4);
}

static void app_motion_event(uint32_t t, uint32_t step, uint32_t id, int32_t v)   /* the app: a motion event */
{
    uint8_t a[6] = {(uint8_t)t, 3, (uint8_t)step, (uint8_t)id, 0, 0};
    host_ed_val(a + 4, v);
    host_request(ED_MOTION, a, 6);
}

static void app_song(uint32_t op, uint32_t slot)    /* the app: SONG op 1 (one row: slot, 16 times), 2 play, 3 stop */
{
    uint8_t a[4] = {(uint8_t)op, 1, (uint8_t)slot, 16};
    host_request(ED_SONG, a, op == 1u ? 4u : 1u);
}

static void app_fm6_bank_put(uint32_t k)            /* the app: FM6 bank slot k := track 1's patch, packed */
{
    static uint8_t a[2 + FM6_PACKED];
    a[0] = ED_FM6_BANK;
    a[1] = (uint8_t)k;
    fm6_pack(fm6_patch[0], a + 2);
    host_request(ED_FM6_PUT, a, 2 + FM6_PACKED);
}

static void app_fm6_bank_erase(uint32_t k)
{
    uint8_t a[1] = {(uint8_t)k};
    host_request(ED_FM6_ERASE, a, 1);
}

static void app_song_read(void)                     /* the app: SONG op 0, the chain as it is */
{
    uint8_t a[1] = {0};
    host_request(ED_SONG, a, 1);
}

/* the web editor's connection (web/editor.html load, readMixer, loadSteps, readMotion, startWatch,
 * bank.list, syncPreferences): every read the protocol has, as the editor sends them */
static void app_read_all(void)
{
    uint8_t a[8] = {0};
    uint32_t i, t;
    host_request(ED_INFO, 0, 0);
    for (i = 0; i < P_COUNT; i++) {
        a[0] = 0;
        a[1] = (uint8_t)i;
        host_request(ED_DESC, a, 2);
    }
    for (i = 0; i < G_COUNT; i++) {
        a[0] = 1;
        a[1] = (uint8_t)i;
        host_request(ED_DESC, a, 2);
    }
    for (i = 0; i < NENGINES; i++) {
        a[0] = (uint8_t)i;
        host_request(ED_NAMES, a, 1);
    }
    for (i = 0; i < 4u; i++) {                      /* PROJECT op 2: is the slot used */
        a[0] = 2;
        a[1] = (uint8_t)i;
        host_request(ED_PROJECT, a, 2);
    }
    host_request(ED_SMP_INFO, 0, 0);
    host_request(ED_TRACK, 0, 0);
    host_request(ED_DUMP, 0, 0);
    app_song_read();
    for (i = 0; i < NSTEP; i++) {
        a[0] = (uint8_t)i;
        host_request(ED_STEP_GET, a, 1);
    }
    for (t = 0; t < NTRK; t++) {
        a[0] = (uint8_t)t;
        host_request(ED_MOTION, a, 1);
        host_request(ED_TRACK_DUMP, a, 1);
        host_request(ED_TRACK_MIX, a, 1);
        a[1] = P_PAN;
        host_request(ED_TRACK_PARAM, a, 2);
        a[1] = 3;
        host_request(ED_TRACK_STEP, a, 2);
    }
    a[0] = 3;
    host_request(ED_WATCH, a, 1);
    a[0] = 0;
    a[1] = 8;
    host_request(ED_UP_LIST, a, 2);
    a[0] = 0;
    host_request(ED_UP_GET, a, 1);
    host_request(ED_UI_STATE, 0, 0);
    host_request(ED_UI_PALETTES, 0, 0);
    a[0] = 0;
    a[1] = 0;
    a[2] = 64;
    a[3] = 8;
    host_request(ED_FAV_GET, a, 4);
    host_request(ED_FM6_LIST, 0, 0);
    a[0] = ED_FM6_TRACK;
    a[1] = 0;
    host_request(ED_FM6_GET, a, 2);
    a[0] = 1;
    a[1] = 0;
    host_request(ED_GET, a, 2);
    host_request(ED_PING, 0, 0);
    host_request(ED_BACKUP_LIST, 0, 0);             /* the backup page: LIST, then each object read */
    for (i = 0; i < 9u; i++) {
        uint32_t len, off;
        ed_bk_object(i, &len);
        for (off = 0; off < len; off += 256u) {
            uint8_t g[8] = {(uint8_t)i};
            uint32_t n = len - off > 256u ? 256u : len - off;
            bk_u35(g + 1, off);
            g[6] = (uint8_t)(n & 127u);
            g[7] = (uint8_t)(n >> 7);
            host_request(ED_BACKUP_GET, g, 8);
        }
    }
    a[0] = 0;
    host_request(ED_WATCH, a, 1);
}

static uint32_t app_reply(uint32_t i)               /* byte i of the editor's reply, after F0 7D 46 4C cmd */
{
    return host_wire_n > 5u + i && host_wire[0] == 0xF0u ? host_wire[5u + i] : 0xFFu;
}

static void app_user(uint32_t slot)                 /* the app: the selected track's sound into a user slot, */
{                                                   /* then that user sound loaded (UP_STORE, UP_LOAD) */
    uint8_t a[2] = {(uint8_t)slot, 0};
    host_request(ED_UP_STORE, a, 2);
    host_request(ED_UP_LOAD, a, 1);
}

/* the app's backup of the runtime project: BACKUP_LIST captures it (object 0, editor_backup.c), and
 * BACKUP_PUT puts it back (begin, 256-byte chunks, commit: project_restore_runtime) */
static uint8_t bk_obj[sizeof(project_store_t)];
static void bk_u35(uint8_t *a, uint32_t v)
{
    uint32_t i;
    for (i = 0; i < 5u; i++)
        a[i] = (uint8_t)((v >> (7u * i)) & 127u);
}
static void app_backup_take(void)
{
    uint32_t len;
    const uint8_t *o;
    host_request(ED_BACKUP_LIST, 0, 0);
    o = ed_bk_object(0, &len);
    memcpy(bk_obj, o, sizeof bk_obj);
}
static uint32_t app_backup_restore(void)            /* the commit's rc (0: restored) */
{
    static uint8_t a[12 + 320];
    uint32_t len = sizeof bk_obj, off, rc;
    a[0] = 0;
    a[1] = 0;
    bk_u35(a + 2, len);
    bk_u35(a + 7, st_crc32(bk_obj, len));
    host_request(ED_BACKUP_PUT, a, 12);
    if ((rc = app_reply(2)) != 0u)
        return rc;
    for (off = 0; off < len; off += 256u) {
        uint32_t n = len - off > 256u ? 256u : len - off, k = 7, i;
        const uint8_t *q = bk_obj + off;
        a[0] = 1;
        a[1] = 0;
        bk_u35(a + 2, off);
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
        if ((rc = app_reply(2)) != 0u)
            return rc;
    }
    a[0] = 2;
    a[1] = 0;
    host_request(ED_BACKUP_PUT, a, 2);
    return app_reply(2);
}

/* the drums as rounds 1 to 6 put them: Easy's kit on track 4 (LFO: track 4, then ALGORITHM to the Drums
 * picture unless it shows it already), the keys there */
static void easy_drums(void)
{
    uint32_t pic;
    host_tap(B_LFO);
    pic = gn_picture();
    if (pic != GN_KIT)
        host_turn(EN_ALGO, pic < GN_NPICS ? (int32_t)GN_KIT - (int32_t)pic : -1);
}

/* back to track 1 (FX) and picture pic there (ALGORITHM from what it shows) */
static void easy_melody(uint32_t pic)
{
    uint32_t cur;
    host_tap(B_FX);
    cur = gn_picture();
    if (cur != pic)
        host_turn(EN_ALGO, cur < GN_NPICS ? (int32_t)pic - (int32_t)cur : (int32_t)pic + 1);
}

/* sound i on the selected track as a child picks it: ALGORITHM to its picture (from another sound: the first
 * picture is a detent), then PRESETS to it inside the picture */
static void easy_pick_sound(uint32_t i)
{
    uint32_t pic = GN_SOUNDS[i].pic, cur = gn_picture(), snd;
    if (cur != pic)
        host_turn(EN_ALGO, cur < GN_NPICS ? (int32_t)pic - (int32_t)cur : (int32_t)pic + 1);
    snd = gn_easy_of(TSEL);
    if (snd != i)
        host_turn(EN_PRESET, (int32_t)i - (int32_t)snd);
}

static void easy_rec_hold(void)                          /* SAVE held 0.8 s and let go: Easy's undo (round 7) */
{
    host_btn(B_SAVE, 1);
    host_wait_ms(800);
    host_btn(B_SAVE, 0);
    host_frame();
}

/* REC armed and the recording begun (round 7, pick 5): PLAY's count-in when stopped, bar 1 when playing */
static void easy_record(void)
{
    uint32_t g;
    host_tap(B_REC);
    if (!song.playing)
        host_tap(B_PLAY);
    for (g = 0; g < 400u && !song.rec; g++)
        host_frame();
}

/* every parameter of every track inside its range for the engine the track has */
static int params_in_range(uint32_t *bad_t, uint32_t *bad_id)
{
    uint32_t t, id;
    for (t = 0; t < NTRK; t++)
        for (id = 0; id < P_COUNT; id++) {
            const param_desc_t *d = track_desc(&trk[t], id);
            int32_t lo = d->min, hi = d->max;
            if (id >= P_E0) {
                const param_desc_t *s = &ENGINES[trk[t].eng_req % NENGINES]->edit[id - P_E0];
                lo = s->min > lo ? s->min : lo;
                hi = s->max < hi ? s->max : hi;
            }
            if (hi > lo && (trk[t].p[id] < lo || trk[t].p[id] > hi)) {
                *bad_t = t;
                *bad_id = id;
                return 0;
            }
        }
    return 1;
}

/* the music: every track's sound, pattern and motion, the song chain, the globals; what a switch must
 * not change */
typedef struct {
    int16_t p[NTRK][P_COUNT];
    step_t step[NTRK][NSTEP];
    uint8_t eng[NTRK], pre[NTRK], usr[NTRK];
    int16_t g[G_COUNT];
    uint8_t fm6[NTRK][FP_SIZE + 1u];
    int8_t octave;
    motion_store_t motion;
    chain_config_t chain;
} music_t;

static int motion_same(const motion_store_t *a, const motion_store_t *b, uint32_t t)   /* track t's motion */
{
    uint32_t i = 0, j = 0;
    if (((a->on >> t) & 1u) != ((b->on >> t) & 1u))
        return 0;
    for (;;) {
        while (i < a->count && (a->event[i].place >> 6) != t)
            i++;
        while (j < b->count && (b->event[j].place >> 6) != t)
            j++;
        if (i == a->count || j == b->count)
            return i == a->count && j == b->count;
        if (a->event[i].place != b->event[j].place || a->event[i].param != b->event[j].param ||
            a->event[i].value != b->event[j].value)
            return 0;
        i++;
        j++;
    }
}

static void music_get(music_t *m)
{
    uint32_t t;
    memset(m, 0, sizeof *m);
    for (t = 0; t < NTRK; t++) {
        memcpy(m->p[t], trk[t].p, sizeof trk[t].p);
        memcpy(m->step[t], trk[t].step, sizeof trk[t].step);
        m->eng[t] = trk[t].eng_req;
        m->pre[t] = trk[t].preset;
        m->usr[t] = trk[t].user;
        memcpy(m->fm6[t], fm6_patch[t], FP_SIZE);
    }
    memcpy(m->g, song.g, sizeof song.g);
    m->octave = song.octave;
    m->motion = motion;
    m->chain = chain_config;
}

static int music_same(const music_t *a, const music_t *b, uint32_t skip_trk)   /* skip_trk: bits */
{
    uint32_t t;
    for (t = 0; t < NTRK; t++) {
        if ((skip_trk >> t) & 1u)
            continue;
        if (memcmp(a->p[t], b->p[t], sizeof a->p[t]) || memcmp(a->step[t], b->step[t], sizeof a->step[t]) ||
            a->eng[t] != b->eng[t] || a->pre[t] != b->pre[t] || a->usr[t] != b->usr[t] ||
            memcmp(a->fm6[t], b->fm6[t], FP_SIZE) || !motion_same(&a->motion, &b->motion, t))
            return 0;
    }
    return !memcmp(a->g, b->g, sizeof a->g) && a->octave == b->octave && !memcmp(&a->chain, &b->chain, sizeof a->chain);
}

/* round 13: the same but for what the knobs of track p's other sound may move (the engine's fallback map: each
 * knob's setting and its second one) */
static int music_same_fb(const music_t *a, const music_t *b, uint32_t p)
{
    static music_t x, y;
    uint32_t k, e = trk[p % NTRK].eng_req;
    x = *a;
    y = *b;
    for (k = 0; e < 14u && k < 4u; k++) {
        const gn_knob_t *m = gn_fb_map(e, k);
        x.p[p % NTRK][m->id] = y.p[p % NTRK][m->id];
        if (m->id2 != GN_NONE)
            x.p[p % NTRK][m->id2] = y.p[p % NTRK][m->id2];
    }
    x.p[p % NTRK][P_LRATE] = y.p[p % NTRK][P_LRATE];   /* (Wobble's claim: the LFO's rate) */
    return music_same(&x, &y, 0);
}

/* Easy's sound, all of it, on a track (easy_whole: a picture's first sound, on the selected track): the engine, preset and user slot
 * of the sound, GN_COMMON, its own settings and its knobs at rest, each as gn_set leaves it (inside the
 * engine's range); read off the track, not from what Easy thinks it holds (gn_easy_of) */
static int easy_whole_on(uint32_t snd, uint32_t trk_i);
static int easy_whole(uint32_t pic) { return easy_whole_on(GN_GROUP[pic % GN_NPICS], song.sel); }   /* (selected track) */
static int easy_whole_on(uint32_t snd, uint32_t trk_i)
{
    static int32_t want[P_COUNT];
    static uint8_t has[P_COUNT];
    const gn_sound_t *s = gn_sound_def(snd);
    track_t *t = &trk[trk_i % NTRK];
    uint32_t k, id;
    if (!s || t->eng_req != s->engine || t->preset != s->preset || t->user)
        return 0;
    memset(has, 0, sizeof has);
#define WANT(i_, v_) (want[(i_)] = (v_), has[(i_)] = 1)
    for (k = 0; k < NELEM(GN_COMMON); k++)
        WANT(GN_COMMON[k].id, GN_COMMON[k].v);
    for (k = 0; k < s->nset; k++)
        WANT(s->set[k].id, s->set[k].v);
    for (k = 0; k < 4u; k++) {
        const gn_knob_t *m = gn_snd_knob(s, k);
        WANT(m->id, gn_travel(m->lo, m->mid, m->hi, m->rest, m->rest));
        if (m->id2 != GN_NONE)
            WANT(m->id2, gn_travel(m->lo2, m->mid2, m->hi2, m->rest, m->rest));
    }
#undef WANT
    for (id = 0; id < P_COUNT; id++) {
        const param_desc_t *d = track_desc(t, id);
        int32_t lo = d->min, hi = d->max;
        if (!has[id])
            continue;
        if (id >= P_E0) {
            const param_desc_t *e = &ENGINES[t->eng_req % NENGINES]->edit[id - P_E0];
            lo = e->min > lo ? e->min : lo;
            hi = e->max < hi ? e->max : hi;
        }
        if (hi > lo && t->p[id] != clamp(want[id], lo, hi))
            return 0;
    }
    return 1;
}

static void music_diff(const music_t *a, const music_t *b)   /* (what differs: the first of it) */
{
    uint32_t t, i;
    for (t = 0; t < NTRK; t++) {
        for (i = 0; i < P_COUNT; i++)
            if (a->p[t][i] != b->p[t][i]) {
                printf("      track %u parameter %u: %d, then %d\n", t + 1u, i, a->p[t][i], b->p[t][i]);
                return;
            }
        if (memcmp(a->step[t], b->step[t], sizeof a->step[t]) || a->eng[t] != b->eng[t] || a->pre[t] != b->pre[t] ||
            a->usr[t] != b->usr[t] || memcmp(a->fm6[t], b->fm6[t], FP_SIZE)) {
            printf("      track %u: steps, engine, preset, user or patch\n", t + 1u);
            return;
        }
    }
    printf("      the globals or the octave\n");
}

static void some_steps(uint32_t t, uint32_t base)   /* a pattern of the user's */
{
    uint32_t i;
    track_defaults_steps(&trk[t]);
    for (i = 0; i < 16u; i += 3u) {
        trk[t].step[i].note[0] = (uint8_t)(base + i);
        trk[t].step[i].n = 1;
        trk[t].step[i].time = ST_NOTE;
        trk[t].step[i].vel = 99;
    }
    trk[t].p[P_SLEN] = 16;
}

/* ---------------------------------------------------------- Easy off --- */
static void t_off(void)
{
    uint32_t k;
    host_power_on();
    host_wait_ms(100);
    CHECK(!gn.on, "power-on: Felucca's pages (Easy off)");
    for (k = 0; k < 27u; k++)
        CHECK(gn_key(&trk[0], k, 53u + k) == 53u + k, "Easy off: gn_key passes every note through");
    key(1, 80);                                     /* F#3, a black key: Felucca plays it */
    host_key(1, 1);
    host_wait_ms(40);
    CHECK(sounding(0, 54), "Easy off: a black key plays its note on track 1");
    host_key(1, 0);
    host_wait_ms(40);
    host_tap(B_SAVE);
    CHECK(!ui.home, "Easy off: SAVE opens Felucca's SAVE pages");
    host_tap(B_HOME);
    CHECK(ui.home, "Easy off: HOME goes home");
    host_btn(B_HOME, 1);
    host_wait_ms(900);
    CHECK(ui.menu == 1u, "Easy off: HOME held 0.7 s opens Felucca's menu");
    host_btn(B_HOME, 0);
    host_wait_ms(30);
    CHECK(!gn.on && !gn.drawn, "Easy off: nothing of Easy drew");
}

/* ------------------------------------------------------ in and out --- */
static void t_switch(void)
{
    host_power_on();
    host_wait_ms(100);
    host_hold_switch(1900);
    CHECK(!gn.on, "HOME + SAVE held 1.9 s: still Felucca");
    CHECK(ui.home && !ui.menu, "..and Felucca took neither press: no SAVE page, no menu");
    host_hold_switch(2100);
    CHECK(gn.on, "held 2.1 s: Easy");
    CHECK(!ui.menu && ui.home, "..no menu opened, no SAVE page");
    host_wait_ms(100);
    CHECK(gn_view() == GV_BOOT, "..the start-up picture (3i)");
    host_wait_ms(3100);
    CHECK(gn_view() == GV_HOME, "..then Easy's Home (round UX-1: the four tracks)");
    host_btn(B_HOME, 1);
    host_btn(B_SAVE, 1);
    host_wait_ms(800);
    CHECK(gn.on && gn_view() == GV_SWITCH, "the hold that leaves: 3h while it fills");
    host_wait_ms(1400);
    CHECK(!gn.on, "..2 s: Felucca's pages");
    host_btn(B_HOME, 0);
    host_btn(B_SAVE, 0);
    host_wait_ms(60);
    CHECK(ui.home && !ui.menu && !gn.drawn, "..HOME, no menu, Felucca's screen");
    host_tap(B_SAVE);
    CHECK(!ui.home, "..and Felucca's own SAVE answers again");
}

/* finding 7: the hold is one unbroken hold of both; either up starts the count again */
static void t_switch_unbroken(void)
{
    uint32_t i;
    host_power_on();
    host_wait_ms(100);
    host_btn(B_HOME, 1);
    host_btn(B_SAVE, 1);
    host_wait_ms(1000);
    host_btn(B_HOME, 0);                            /* HOME up, SAVE still held */
    host_wait_ms(2500);
    CHECK(!gn.on, "both, then HOME up for 2.5 s: no switch");
    CHECK(trk[0].preset == TRK_DEF[0][1], "..and SAVE held alone was no undo");
    host_btn(B_HOME, 1);                            /* HOME again */
    host_wait_ms(60);
    CHECK(!gn.on, "HOME down again: no switch at once (finding 7)");
    host_wait_ms(1800);
    CHECK(!gn.on, "..nor 1.86 s later");
    host_wait_ms(300);
    CHECK(gn.on, "..2.16 s of both: Easy");
    host_btn(B_HOME, 0);
    host_btn(B_SAVE, 0);
    host_wait_ms(3200);
    for (i = 0; i < 2u; i++) {                      /* the same leaving, with SAVE up then down again */
        host_btn(B_HOME, 1);
        host_btn(B_SAVE, 1);
        host_wait_ms(1200);
        host_btn(B_SAVE, 0);
        host_wait_ms(900);
        host_btn(B_SAVE, 1);
        host_wait_ms(900);
        CHECK(gn.on, "leaving: SAVE let go between: the count starts again");
        host_wait_ms(1300);
        CHECK(!gn.on, "..2.2 s of both after it: Advanced");
        host_btn(B_HOME, 0);
        host_btn(B_SAVE, 0);
        host_wait_ms(100);
        host_hold_switch(2100);
        host_wait_ms(3200);
    }
}

/* the hold beside Felucca's own held buttons: SAVE held = undo, HOME held = the menu, the layers */
static void t_switch_collisions(void)
{
    music_t m;
    host_power_on();
    host_wait_ms(100);
    host_turn(EN_PRESET, 1);                        /* (Advanced) a sound load: Felucca's undo has a copy */
    host_wait_ms(100);
    CHECK(undo.trk == 1u, "a PRESETS turn on HOME: Felucca's undo copy");
    music_get(&m);
    /* HOME, then SAVE 300 ms later: the hold, neither a menu nor an undo */
    host_btn(B_HOME, 1);
    host_wait_ms(300);
    host_btn(B_SAVE, 1);
    host_wait_ms(2100);
    CHECK(gn.on, "HOME, SAVE 0.3 s later, both 2 s: Easy");
    CHECK(!ui.menu, "..HOME's hold did not open the menu");
    { music_t m2; music_get(&m2); CHECK(music_same(&m, &m2, 0) && gn_picture() == 0u,
                                        "..the entry's Gnome shown (round 14: not put there yet), all as it was"); }
    host_btn(B_HOME, 0);
    host_btn(B_SAVE, 0);
    host_wait_ms(100);
    host_hold_switch(2100);                         /* (back) */
    CHECK(!gn.on, "back in Advanced");
    { music_t m2; music_get(&m2); CHECK(music_same(&m, &m2, 0),
                                        "..the Gnome untouched: track 1's sound back; SAVE's hold had not undone the load"); }
    /* HOME held long enough for the menu first: the hold does not count */
    host_btn(B_HOME, 1);
    host_wait_ms(800);
    CHECK(ui.menu == 1u, "HOME held 0.8 s: the menu");
    host_btn(B_SAVE, 1);
    host_wait_ms(2500);
    CHECK(!gn.on && ui.menu, "..then SAVE, both 2.5 s: no Easy (the press Felucca acted on)");
    host_btn(B_HOME, 0);
    host_btn(B_SAVE, 0);
    host_wait_ms(60);
    host_btn(B_HOME, 1);                            /* (the menu closes with HOME held again) */
    host_wait_ms(800);
    host_btn(B_HOME, 0);
    host_wait_ms(60);
    /* SAVE held long enough for the undo first */
    host_btn(B_SAVE, 1);
    host_wait_ms(800);
    CHECK(trk[0].preset != m.pre[0] || trk[0].eng_req != m.eng[0], "SAVE held 0.8 s: Felucca's undo");
    host_btn(B_HOME, 1);
    host_wait_ms(2500);
    CHECK(!gn.on, "..then HOME, both 2.5 s: no Easy");
    host_btn(B_HOME, 0);
    host_btn(B_SAVE, 0);
    host_wait_ms(60);
    /* with a layer's button held, or SEQ */
    host_btn(B_FX, 1);
    host_wait_ms(100);
    host_hold_switch(2500);
    CHECK(!gn.on, "FX held with HOME + SAVE: no Easy");
    host_btn(B_FX, 0);
    host_wait_ms(100);
    host_btn(B_SEQ, 1);
    host_hold_switch(2500);
    CHECK(!gn.on, "SEQ held with HOME + SAVE: no Easy");
    host_btn(B_SEQ, 0);
    host_wait_ms(100);
    host_btn(B_HOME, 1);                            /* another button during the count: it starts again */
    host_btn(B_SAVE, 1);
    host_wait_ms(1500);
    host_tap(B_PLAY);
    host_wait_ms(1500);
    CHECK(!gn.on, "PLAY pressed 1.5 s into the hold: no Easy 1.5 s later");
    host_wait_ms(700);
    CHECK(gn.on, "..2 s of the chord alone after it: Easy");
    host_btn(B_HOME, 0);
    host_btn(B_SAVE, 0);
    host_wait_ms(100);
    host_hold_switch(2100);
    host_wait_ms(100);
    host_hold_switch(2100);
    CHECK(gn.on, "and alone: Easy");
}

/* finding 2: entering Easy and leaving it changes nothing musical; a picture changes its track only */
static void t_switch_keeps(void)
{
    music_t a, b;
    uint32_t i;
    host_power_on();
    host_wait_ms(60);
    app_preset(GE_PHASE, 3);                        /* Advanced: a sound, its settings, patterns, a tempo */
    app_set(P_E2, 99);
    app_set(P_LEVEL, 77);
    some_steps(0, 40);
    some_steps(1, 50);
    trk[1].p[P_SLEN] = 12;
    song.g[G_BPM] = 133;
    song.g[G_RSIZE] = 20;
    song.octave = 1;
    host_wait_ms(60);
    music_get(&a);
    host_hold_switch(2100);
    CHECK(gn.on, "Easy");
    host_wait_ms(3200);
    music_get(&b);
    CHECK(music_same(&a, &b, 0) && song.sel == 0u && gn_picture() == 0u && gn_view() == GV_HOME,
          "into Easy: the Gnome shown on track 1, selected (round 14: not yet put there); nothing musical changed");
    for (i = 0; i < 3u; i++)                        /* the keys: the Gnome put there, then played */
        key(7u + 2u * i, 60);
    music_get(&b);
    for (i = 0; i < NTRK; i++)                      /* (what the keys recorded: nothing, REC is off) */
        CHECK(!memcmp(a.step[i], b.step[i], sizeof a.step[i]), "the patterns as they were");
    CHECK(music_same(&a, &b, 1u) && gn_easy_of(&trk[0]) == GN_GROUP[0], "..a key: the Gnome on track 1; nothing else changed");
    CHECK(song.g[G_BPM] == 133, "the tempo as it was (Easy turns the real tempo, round 7)");
    host_hold_switch(2100);
    CHECK(!gn.on, "back to Advanced");
    music_get(&b);
    CHECK(music_same(&a, &b, 1u) && trk[0].eng_req == GE_VOICE, "..the Gnome used: he stays; nothing else changed");
    host_hold_switch(2100);
    host_wait_ms(3200);
    host_turn(EN_ALGO, 1);                        /* a picture: Easy's Organ on track 1 */
    host_wait_ms(60);
    music_get(&b);
    CHECK(trk[0].eng_req == GE_WHEEL && gn_easy_of(&trk[0]) == GN_GROUP[1], "ALGORITHM: the Organ on track 1");
    CHECK(music_same(&a, &b, 1u), "..nothing else changed");
    CHECK(!memcmp(a.step[0], b.step[0], sizeof a.step[0]) && b.p[0][P_SLEN] == a.p[0][P_SLEN],
          "..not even track 1's pattern");
    easy_drums();                                   /* the drums, the ninth picture, on track 4 */
    host_wait_ms(60);
    music_get(&b);
    CHECK(trk[3].eng_req == GE_SAMPLE && trk[3].p[P_E0] == GN_KIT_SET && song.sel == 3u, "..the drums on track 4");
    CHECK(music_same(&a, &b, 1u | 8u), "..tracks 2 and 3 and the globals untouched");
}

/* review 4: the settings a sound load keeps (param_kept: level, the arpeggiator, chord keys, scale snap,
 * transpose) that Easy's picks set go back on leaving, each one no one changed since; three picks in a
 * row are one load for Felucca's undo, so SAVE held in Advanced afterwards gives back the whole track as
 * it was before the first (every track, every setting, the FM6 patch, the globals: music_same) */
static void t_retained(void)
{
    music_t before, after;
    host_power_on();
    host_wait_ms(60);
    app_set(P_TRANS, 5);                            /* Advanced: the track's own settings */
    app_set(P_CHRD, 2);
    app_set(P_AMODE, 1);
    app_set(P_ARATE, 4);
    app_set(P_AGATE, 70);
    app_set(P_QUANT, 1);
    app_set(P_LEVEL, 77);
    host_wait_ms(1600);                             /* (no longer part of a load: ui.c load_extend) */
    music_get(&before);
    host_hold_switch(2100);
    CHECK(gn.on, "Easy");
    host_wait_ms(3200);                             /* (round 13: the entry's Gnome, the first pick) */
    host_turn(EN_ALGO, 3);                        /* the Laser (its own arpeggiator), */
    host_turn(EN_ALGO, 4);                        /* the Piano: three picks in a row */
    host_wait_ms(60);
    CHECK(easy_whole(7) && trk[0].p[P_TRANS] == 0 && trk[0].p[P_CHRD] == 0 && trk[0].p[P_AMODE] == 0 &&
          trk[0].p[P_LEVEL] != 77, "the picks set what Easy's sounds need");
    host_hold_switch(2100);
    CHECK(!gn.on, "back to Advanced");
    CHECK(trk[0].p[P_TRANS] == 5 && trk[0].p[P_CHRD] == 2 && trk[0].p[P_AMODE] == 1 && trk[0].p[P_ARATE] == 4 &&
          trk[0].p[P_AGATE] == 70 && trk[0].p[P_QUANT] == 1 && trk[0].p[P_LEVEL] == 77,
          "..leaving: transpose, chord keys, arpeggiator, scale snap and level as they were");
    host_wait_ms(100);
    host_btn(B_SAVE, 1);                            /* Felucca's undo */
    host_wait_ms(800);
    host_btn(B_SAVE, 0);
    host_wait_ms(60);
    music_get(&after);
    CHECK(music_same(&before, &after, 0), "..SAVE held: the whole track as before the first of the three picks");
    /* one the app changed while Easy was on stays the app's */
    host_hold_switch(2100);
    host_turn(EN_ALGO, 1);
    host_wait_ms(30);
    app_set(P_LEVEL, 50);
    host_hold_switch(2100);
    CHECK(!gn.on && trk[0].p[P_LEVEL] == 50 && trk[0].p[P_TRANS] == 5, "a level the app set in Easy stays; the rest back");
}

/* round 3 (high): Easy's claims to put the kept settings back end at every runtime replacement (a
 * project load, the runtime backup put back) and at every outside write of them, the same value or
 * not; Easy's next write takes the track's value then as its baseline, never the entered project's.
 * Project A: transpose +5, chord keys 2, level 77 on track 1 (slot 1); project B: transpose 0, chord
 * keys 0, level 90 (slot 2). The proof is the whole state, and that state saved and loaded again */
static void kept_ab(music_t *ma, music_t *mb)
{
    host_power_on();
    host_wait_ms(60);
    app_set(P_TRANS, 5);
    app_set(P_CHRD, 2);
    app_set(P_LEVEL, 77);
    app_project(1, 0);                              /* A */
    app_set(P_TRANS, 0);
    app_set(P_CHRD, 0);
    app_set(P_LEVEL, 90);
    app_project(1, 1);                              /* B */
    host_wait_ms(1600);
    app_project(0, 1);
    host_wait_ms(60);
    music_get(mb);
    app_project(0, 0);
    host_wait_ms(60);
    music_get(ma);
}

static int saved_again_same(const music_t *want)    /* the state saved (slot 3), loaded again: the same? */
{
    music_t m;
    app_project(1, 2);
    app_project(0, 2);
    host_wait_ms(60);
    music_get(&m);
    return music_same(want, &m, 0);
}

static void t_kept_projects(void)
{
    music_t ma, mb, mbk, m;
    /* 1: A, Easy, a pick, project B loaded, leave: B as loaded, all of it */
    kept_ab(&ma, &mb);
    CHECK(ma.p[0][P_TRANS] == 5 && mb.p[0][P_TRANS] == 0 && mb.p[0][P_LEVEL] == 90, "(A +5, B 0)");
    host_hold_switch(2100);
    host_turn(EN_ALGO, 1);                        /* the Gnome: transpose 0, chord keys off, its level */
    host_wait_ms(60);
    CHECK(gn.on && trk[0].p[P_TRANS] == 0 && trk[0].p[P_CHRD] == 0, "Easy, the Gnome: transpose 0, chord keys off");
    app_project(0, 1);                              /* the app loads B */
    host_wait_ms(60);
    host_hold_switch(2100);
    CHECK(!gn.on, "left Easy");
    music_get(&m);
    CHECK(music_same(&mb, &m, 0), "A, Easy, a pick, B loaded, leave: B as loaded, all of it (A's +5 not put into B)");
    CHECK(saved_again_same(&mb), "..and so saved and loaded again");
    /* 2: the same through the runtime backup */
    kept_ab(&ma, &mb);
    app_project(0, 1);
    host_wait_ms(60);
    app_backup_take();                              /* B's runtime, taken in Advanced */
    CHECK(app_backup_restore() == 0u, "(B put back in Advanced)");
    host_wait_ms(60);
    music_get(&mbk);
    app_project(0, 0);                              /* A again */
    host_wait_ms(60);
    host_hold_switch(2100);
    host_turn(EN_ALGO, 1);
    host_wait_ms(60);
    CHECK(app_backup_restore() == 0u, "in Easy, after a pick: the app puts B's runtime back");
    host_wait_ms(60);
    host_hold_switch(2100);
    music_get(&m);
    CHECK(!gn.on && music_same(&mbk, &m, 0), "..leave: B as put back, all of it");
    CHECK(saved_again_same(&mbk), "..and so saved and loaded again");
    /* 3: outside writes equal to Easy's own values, by SET, TRACK_PARAM, TRACK_MIX: the app's now */
    kept_ab(&ma, &mb);
    host_hold_switch(2100);
    host_turn(EN_ALGO, 1);
    host_wait_ms(60);
    app_set(P_TRANS, 0);                            /* "transpose 0": what the Gnome has already */
    app_track_param(0, P_CHRD, 0);
    app_track_mix(0, trk[0].p[P_LEVEL], 0);         /* the Gnome's own level, set by the app */
    host_wait_ms(30);
    music_get(&mbk);
    host_hold_switch(2100);
    music_get(&m);
    CHECK(!gn.on && m.p[0][P_TRANS] == 0 && m.p[0][P_CHRD] == 0 && m.p[0][P_LEVEL] == mbk.p[0][P_LEVEL],
          "the app set transpose, chord keys and level to Easy's values: leaving keeps the app's");
    CHECK(music_same(&mbk, &m, 0), "..the whole state as the app left it (the rest Easy wrote: A's values again, equal)");
    CHECK(saved_again_same(&m), "..and so saved and loaded again");
    /* 4: after the app's write, Easy's next pick takes the track's value then as the baseline */
    kept_ab(&ma, &mb);
    host_hold_switch(2100);
    host_turn(EN_ALGO, 1);                        /* the Gnome (A's +5 the baseline) */
    host_wait_ms(60);
    app_set(P_TRANS, 3);                            /* the app: +3 */
    host_turn(EN_ALGO, 1);                        /* the Organ: transpose 0 again, its baseline +3 */
    host_wait_ms(60);
    CHECK(trk[0].p[P_TRANS] == 0, "a pick after the app's +3: transpose 0 for the Organ");
    host_hold_switch(2100);
    CHECK(trk[0].p[P_TRANS] == 3 && trk[0].p[P_CHRD] == 2 && trk[0].p[P_LEVEL] == 77,
          "leave: +3 (the baseline taken at the pick after the app's write), A's chord keys and level");
    /* 5: B loaded in Easy, then a pick: B's values the baseline */
    kept_ab(&ma, &mb);
    host_hold_switch(2100);
    host_turn(EN_ALGO, 1);
    app_project(0, 1);                              /* B */
    host_wait_ms(60);
    host_turn(EN_ALGO, 2);                        /* (from B's sound, another: the Organ) */
    host_wait_ms(60);
    CHECK(easy_whole(1), "B, then the Organ picked");
    host_hold_switch(2100);
    music_get(&m);
    CHECK(m.p[0][P_TRANS] == 0 && m.p[0][P_CHRD] == 0 && m.p[0][P_LEVEL] == 90, "leave: B's transpose, chord keys, level");
    CHECK(saved_again_same(&m), "..and so saved and loaded again");
    /* 7: a project save in Easy (the editor's PROJECT save: the panel cannot save while Easy has it) ends
     * every claim: leaving keeps what was saved, the live state is the saved one, loaded again too */
    kept_ab(&ma, &mb);
    host_hold_switch(2100);
    host_turn(EN_ALGO, 1);                        /* the Gnome: transpose 0, chord keys off, its level */
    host_wait_ms(60);
    app_project(1, 2);                              /* saved to slot 3 */
    host_wait_ms(60);
    host_hold_switch(2100);
    music_get(&m);
    CHECK(!gn.on && m.p[0][P_TRANS] == 0 && m.p[0][P_CHRD] == 0 && m.p[0][P_LEVEL] != 77,
          "a save in Easy, then leave: the Gnome's transpose, chord keys and level kept (A's not put back)");
    app_project(0, 2);
    host_wait_ms(60);
    music_get(&mbk);
    CHECK(music_same(&m, &mbk, 0), "..the live state is the saved one: slot 3 loaded again, the same");
    /* .. and after the save, a pick takes the saved values as its baseline */
    app_project(0, 0);                              /* A */
    host_wait_ms(60);
    host_hold_switch(2100);
    host_turn(EN_ALGO, 1);
    host_wait_ms(60);
    app_project(1, 2);
    host_turn(EN_ALGO, 2);                        /* the Laser after the save: its own arpeggio */
    host_wait_ms(60);
    host_hold_switch(2100);
    CHECK(trk[0].p[P_TRANS] == 0 && trk[0].p[P_AMODE] == 0 && trk[0].p[P_CHRD] == 0,
          "a pick after the save: leaving puts back the saved values (transpose 0, no arpeggio), not A's");
    /* 6: a different value from the app, no pick after it: the app's */
    kept_ab(&ma, &mb);
    host_hold_switch(2100);
    host_turn(EN_ALGO, 1);
    app_track_param(0, P_TRANS, -2);
    host_hold_switch(2100);
    CHECK(trk[0].p[P_TRANS] == -2 && trk[0].p[P_CHRD] == 2, "the app's -2 stays; A's chord keys back");
}

/* round 5 (medium): a song's motion moves the level, which motion_step writes without asking anyone: a
 * song started ends Easy's claims to the level, and none is taken while one is prepared or plays. Project
 * 2 has motion on track 1's level, 96 (the Gnome's own level) at four steps; the song plays it 16 times.
 * Level 77; Easy; the Gnome (96); the song played and stopped (stopping puts back what motion moved:
 * motion_end); leave: the level is not put back to 77, leaving changes nothing at all. A pick while the
 * song plays takes no claim to the level either */
static void song_setup(void)
{
    uint32_t i;
    host_power_on();
    host_wait_ms(60);
    app_set(P_LEVEL, 77);
    for (i = 0; i < 16u; i += 4u)
        app_motion_event(0, i, P_LEVEL, 96);
    app_project(1, 1);                              /* project 2: the motion with it */
    {
        uint8_t a[2] = {0, 2};                      /* track 1's motion cleared, live */
        host_request(ED_MOTION, a, 2);
    }
    app_song(1, 1);                                 /* the song: project 2, 16 times */
    host_wait_ms(1600);
}

static void t_song_kept(void)
{
    music_t m, m2;
    song_setup();
    CHECK(trk[0].p[P_LEVEL] == 77 && motion_count(&trk[0]) == 0u && chain_config.count == 1u, "(level 77, a song of project 2)");
    host_hold_switch(2100);                         /* the entry's Gnome, put there by a key (round 14): level 96, */
    key(7, 60);                                     /* a claim to put 77 back */
    host_wait_ms(60);
    CHECK(trk[0].p[P_LEVEL] == 96 && (gn.kept_mask[0] & 1u), "the Gnome: level 96, Easy's claim to 77");
    app_song(2, 0);                                 /* the song plays */
    host_frame();                                   /* (Easy's next input call: before anything else of it) */
    CHECK(chain_busy() && !(gn.kept_mask[0] & 1u), "the song started: no claim to the level");
    host_wait_ms(3000);
    CHECK(chain.running && ((motion_active[0][P_LEVEL / 32u] >> (P_LEVEL % 32u)) & 1u),
          "..it plays, its motion writing the level (96: the same value as Easy's)");
    app_song(3, 0);                                 /* stop: motion_end puts back what motion moved */
    host_wait_ms(200);
    CHECK(!chain_busy() && trk[0].p[P_LEVEL] == 96, "stopped: the level as before the song (96)");
    music_get(&m);
    host_hold_switch(2100);
    music_get(&m2);
    CHECK(!gn.on && m2.p[0][P_LEVEL] == 96 && music_same(&m, &m2, 0),
          "leave: the level not put back to 77; leaving changes nothing at all (motion, chain included)");
    /* a pick while the song plays */
    song_setup();
    host_hold_switch(2100);                         /* (the entry's Gnome, put there by a key) */
    key(7, 60);
    host_wait_ms(60);
    app_song(2, 0);
    host_wait_ms(1500);
    host_turn(EN_ALGO, 1);                        /* the Organ, while the song plays */
    host_wait_ms(60);
    CHECK(chain.running && easy_whole(1) && !(gn.kept_mask[0] & 1u), "the Organ picked while the song plays: no claim to the level");
    host_wait_ms(1500);
    app_song(3, 0);
    host_wait_ms(200);
    music_get(&m);
    host_hold_switch(2100);
    music_get(&m2);
    CHECK(!gn.on && music_same(&m, &m2, 0), "..stopped, leave: leaving changes nothing at all");
    /* the song started from Advanced, then Easy: none taken while it plays */
    song_setup();
    app_song(2, 0);
    host_wait_ms(200);
    host_hold_switch(2100);
    host_turn(EN_ALGO, 1);
    host_wait_ms(60);
    CHECK(chain_busy() && !(gn.kept_mask[0] & 1u), "a song from Advanced, then a pick in Easy: no claim to the level");
    app_song(3, 0);
    host_wait_ms(200);
    music_get(&m);
    host_hold_switch(2100);
    music_get(&m2);
    CHECK(!gn.on && music_same(&m, &m2, 0), "..stopped, leave: leaving changes nothing");
}

/* round 6 (the deciding one): a song's motion moves the setting that chooses the sound (an FM6 track's
 * PTCH, the kit's SET) away and back while HOME + SAVE are held, the hold let go before 2 s: the change
 * was seen (Easy's housekeeping runs every call, the chord counting or not), so the sound is no longer
 * Easy's, for good. Project 2: track 1 FM6 (the Bells' preset) with PTCH motion, or track 4 SAMPLE with
 * SET motion, away at step 2 and back at step 6; the song plays it */
static void ident_song(uint32_t t, uint32_t engine, uint32_t preset, uint32_t id, int32_t away, int32_t back)
{
    uint8_t a[2] = {(uint8_t)t, 2};
    host_power_on();
    host_wait_ms(60);
    host_request(ED_TRACK, a, 1);                   /* (track t selected) */
    app_preset(engine, preset);
    app_motion_event(t, 2, id, away);
    app_motion_event(t, 6, id, back);
    app_project(1, 1);
    host_request(ED_MOTION, a, 2);                  /* (the live motion cleared) */
    app_song(1, 1);
    a[0] = 0;
    host_request(ED_TRACK, a, 1);
    host_wait_ms(1600);
}

static void t_hold_replacement(void)
{
    uint32_t w;
    for (w = 0; w < 2u; w++) {
        const uint32_t fm6 = w == 0u, t = fm6 ? 0u : 3u, id = fm6 ? P_E7 : P_E0;
        const int32_t mine = fm6 ? ENGINES[GE_FM6]->presets[GN_SOUNDS[GN_GROUP[2]].preset].e[7] : (int32_t)GN_KIT_SET;
        const int32_t away = fm6 ? (mine + 1) % (int32_t)FM6_NFACTORY : 0;
        uint32_t i, saw_away = 0, loaded = 0, back = 0, t0;
        uint8_t gen0;
        ident_song(t, fm6 ? GE_FM6 : GE_SAMPLE, fm6 ? GN_SOUNDS[GN_GROUP[2]].preset : 0u, id, away, mine);
        host_hold_switch(2100);
        if (fm6)
            host_turn(EN_ALGO, 2);                  /* the Bells on track 1 (from the entry's Gnome), or the drums on track 4 */
        else
            easy_drums();
        host_wait_ms(60);
        CHECK(gn_easy_of(&trk[t]) == (fm6 ? GN_GROUP[2] : GN_SND_KIT) && trk[t].p[id] == mine, fm6 ? "(Easy's Bells)" : "(Easy's drums)");
        gen0 = fm6_pgen[t];
        app_song(2, 0);                             /* the song plays ... */
        host_btn(B_HOME, 1);                        /* ... and HOME + SAVE are held across its motion */
        host_btn(B_SAVE, 1);
        t0 = fm1_ms;
        for (i = 0; i < 120u && !back; i++) {
            host_frame();
            if (trk[t].p[id] == away)
                saw_away = 1;
            if (fm6 && fm6_slot[t] == (uint32_t)away)
                loaded = 1;                         /* (a different patch actually loaded) */
            back = saw_away && trk[t].p[id] == mine;
        }
        host_wait_ms(60);
        host_btn(B_HOME, 0);                        /* let go before 2 s */
        host_btn(B_SAVE, 0);
        host_frame();
        CHECK(saw_away && back && (!fm6 || (loaded && fm6_pgen[t] != gen0)) && fm1_ms - t0 < 2000u && gn.on,
              fm6 ? "the song moved PTCH away (another patch loaded) and back, the hold let go before 2 s"
                  : "the song moved SET away and back, the hold let go before 2 s");
        CHECK(!gn.own[t] && gn_easy_of(&trk[t]) == GN_NOSND,
              fm6 ? "..the Bells are no longer Easy's, though PTCH came back" : "..the kit is no longer Easy's, though SET came back");
        host_key(13, 1);                            /* (a black key: Felucca's note, not Easy's rest) */
        host_wait_ms(30);
        CHECK(kb_note[13] == gn_chromatic(&trk[t], 13), "..its keys another sound's (chromatic, round 13)");
        host_key(13, 0);
        host_wait_ms(30);
        app_song(3, 0);
        host_wait_ms(100);
        CHECK(!gn.own[t] && gn_easy_of(&trk[t]) == GN_NOSND, "..and stay so after the song stops");
    }
    /* an FM6 patch loaded again under the same PTCH (the generation, not the number) */
    host_easy();
    host_turn(EN_ALGO, 3);
    host_wait_ms(60);
    CHECK(gn_easy_of(&trk[0]) == GN_GROUP[2], "(Easy's Bells)");
    fm6_load_slot(0, (uint32_t)trk[0].p[P_E7]);
    host_frame();
    CHECK(!gn.own[0], "the Bells' patch loaded again by someone else, the same PTCH: no longer Easy's");
}

/* round 6: a SONG play that fails (no chain) leaves Easy's claim to the level: leaving puts it back */
static void t_song_rejected(void)
{
    host_power_on();
    host_wait_ms(60);
    app_set(P_LEVEL, 77);
    host_wait_ms(1600);
    host_hold_switch(2100);
    host_turn(EN_ALGO, 1);
    host_wait_ms(60);
    CHECK(chain_config.count == 0u && (gn.kept_mask[0] & 1u), "(no song; the Gnome, a claim to 77)");
    app_song(2, 0);
    host_wait_ms(60);
    CHECK(!chain_busy() && app_reply(1) != 0u && (gn.kept_mask[0] & 1u), "SONG play with no song: refused, the claim stays");
    host_hold_switch(2100);
    CHECK(!gn.on && trk[0].p[P_LEVEL] == 77, "..leave: the level back to 77");
}

/* round 3 (medium): the reads the web editor makes (connecting, after a save or a load: SONG), the
 * backup page's reads, the preferences, a save: none of them drops Easy's sounds or its undo copy. Easy
 * on: the Gnome on track 1, the drums on track 4 and a recording of track 4; after each sequence the
 * pads, the knobs and REC held still work. Easy off: the same sequences leave Easy's state and the
 * music as they were */
static void t_editor_reads(void)
{
    static const char *const SEQ[4] = {"the editor connecting (every read)", "a project save, then SONG's read",
                                       "the backup page's reads", "the preferences, the user bank"};
    music_t before, m;
    uint32_t w;
    for (w = 0; w < 4u; w++) {
        uint8_t a[2];
        host_easy();
        host_turn(EN_ALGO, 1);                    /* the Gnome on track 1 */
        easy_drums();                               /* the drums on track 4 */
        host_wait_ms(60);
        music_get(&before);
        easy_record();                                  /* a recording of the drums */
        key(9, 60);
        host_tap(B_REC);
        host_tap(B_PLAY);
        host_wait_ms(60);
        CHECK(gn.own[0] && gn.own[3] && gn.undo_of == 4u, "(the Gnome, the drums, a recording of track 4)");
        switch (w) {
        case 0:
            app_read_all();
            break;
        case 1:
            app_project(1, 1);
            app_song_read();
            break;
        case 2:
            host_request(ED_BACKUP_LIST, 0, 0);
            app_backup_take();
            break;
        default:
            a[0] = 0;                               /* the palette: the one it is (UI_SET) */
            a[1] = (uint8_t)settings.palette;
            host_request(ED_UI_SET, a, 2);
            {
                uint8_t f[4] = {0, 0, 64, 1};       /* a favourite: engine 0, preset 0 */
                host_request(ED_FAV_SET, f, 4);
            }
            a[0] = 20;
            a[1] = 0;
            host_request(ED_UP_STORE, a, 2);        /* the drums stored as a user sound */
            break;
        }
        host_wait_ms(60);
        if (!gn.own[0] || !gn.own[3] || gn.undo_of != 4u)
            printf("    after %s: owned %u %u, undo of %u\n", SEQ[w], gn.own[0], gn.own[3], gn.undo_of);
        CHECK(gn.own[0] && gn.own[3] && gn.undo_of == 4u, "after the editor's reads: Easy's sounds and its undo copy");
        CHECK(easy_whole(GN_KIT) && song.sel == 3u, "..the drums whole on track 4");
        host_key(12, 1);                            /* (F4: white key 5, pad 5) */
        host_wait_ms(30);
        CHECK(kb_note[12] == GN_PADS[key_place(12)], "..a white key is still its pad");
        host_key(12, 0);
        host_wait_ms(200);
        host_key(13, 1);
        host_wait_ms(30);
        CHECK(kb_note[13] == gn_black_pad(&trk[3], 13), "..a black key still the kit's other sound (round 13)");
        host_key(13, 0);
        host_wait_ms(30);
        {
            int16_t b = trk[3].p[P_E4];
            host_turn(EN_K1, -6);
            CHECK(trk[3].p[P_E4] != b, "..Bright still moves the kit's cut");
            host_turn(EN_K1, 6);
        }
        easy_rec_hold();
        music_get(&m);
        CHECK(!memcmp(m.step[3], before.step[3], sizeof m.step[3]) && m.p[3][P_SLEN] == before.p[3][P_SLEN],
              "..REC held: the recording undone");
    }
    /* Easy off: the same sequences change nothing of Easy's, nor the music */
    host_power_on();
    host_wait_ms(60);
    for (w = 0; w < 4u; w++) {
        static uint8_t gn_was[sizeof gn];
        uint8_t a[2] = {0, (uint8_t)settings.palette};
        music_get(&before);
        memcpy(gn_was, &gn, sizeof gn);
        if (w == 0)
            app_read_all();
        else if (w == 1) {
            app_project(1, 1);
            app_song_read();
        } else if (w == 2)
            app_backup_take();
        else
            host_request(ED_UI_SET, a, 2);
        host_wait_ms(60);
        music_get(&m);
        CHECK(!memcmp(gn_was, &gn, sizeof gn), "Easy off: the editor's reads leave Easy's state as it was");
        CHECK(music_same(&before, &m, 0), "..and the music");
    }
}

/* review 5: a REC press under way when the HOME + SAVE chord takes the panel is no tap afterwards, in
 * Easy or in Advanced, either side of the switch: REC down, HOME and SAVE added, REC let go, the hold
 * completed, out and in again */
static void rec_then_chord(void)
{
    host_btn(B_REC, 1);
    host_wait_ms(100);
    host_btn(B_HOME, 1);
    host_btn(B_SAVE, 1);
    host_wait_ms(100);
    host_btn(B_REC, 0);
    host_wait_ms(2100);
    host_btn(B_HOME, 0);
    host_btn(B_SAVE, 0);
    host_wait_ms(200);
}

static void t_rec_interrupted(void)
{
    host_easy();
    host_turn(EN_ALGO, 2);
    host_wait_ms(60);
    rec_then_chord();
    CHECK(!gn.on, "Easy: REC down, HOME + SAVE, REC let go, the hold: Advanced");
    CHECK(!song.rec && !song.playing && transport_req != 1u, "..no recording, no loop");
    host_hold_switch(2100);
    host_wait_ms(200);
    CHECK(gn.on && !song.rec && !song.playing && transport_req != 1u, "..and Easy again: no REC tap left over");
    host_hold_switch(2100);
    host_wait_ms(200);
    CHECK(!gn.on && !song.rec && !song.playing, "..nor in Advanced after");
    rec_then_chord();
    CHECK(gn.on, "Advanced: REC down, HOME + SAVE, REC let go, the hold: Easy");
    CHECK(!song.rec && !song.playing && transport_req != 1u, "..no recording either");
    host_hold_switch(2100);
    host_wait_ms(200);
    CHECK(!gn.on && !song.rec && !song.playing, "..out again: none");
    CHECK(ui.home, "(Advanced, on HOME)");
    host_btn(B_SEQ, 1);                             /* SEQ the same (a tap: the SEQ pages) */
    host_wait_ms(100);
    host_btn(B_HOME, 1);
    host_btn(B_SAVE, 1);
    host_wait_ms(100);
    host_btn(B_SEQ, 0);
    host_wait_ms(100);
    CHECK(ui.home && !ui.menu, "SEQ down, HOME + SAVE, SEQ let go: no SEQ tap (still HOME)");
    host_wait_ms(2000);
    host_btn(B_HOME, 0);
    host_btn(B_SAVE, 0);
    host_wait_ms(100);
    CHECK(gn.on, "..the hold: Easy");
}

/* -------------------------------------------------------- the keys --- */
/* the two further findings: the white-key table (F5 and F#5 were swapped), and the pads on the lower row */
static void t_white_keys(void)
{
    uint32_t k, w = 0;
    for (k = 0; k < 27u; k++) {
        CHECK(GN_WHITE[k] == (key_black(k) ? 0xFFu : key_place(k)), "GN_WHITE: seq.c's key_place of each white key");
        w += !key_black(k);
    }
    CHECK(w == 16u, "sixteen white keys");
    CHECK(GN_WHITE[24] == 14u && GN_WHITE[25] == 0xFFu && GN_WHITE[26] == 15u, "F5 the 15th white key, F#5 black");
}

static void t_keys_melody(void)
{
    uint32_t k;
    host_easy();
    host_turn(EN_ALGO, 2);                        /* the Organ: one of Easy's sounds, no octave */
    host_wait_ms(GN_PICK_MS);
    CHECK(gn_easy_of(&trk[0]) == GN_GROUP[1], "the Organ on track 1");
    for (k = 0; k < 27u; k++) {
        host_key(k, 1);
        host_wait_ms(45);
        CHECK(sounding(0, 53u + k), "Easy's sound: every key plays its own note, chromatic (round 13: the black keys too)");
        host_key(k, 0);
        host_wait_ms(200);
    }
    host_tap(B_OCTUP);
    host_key(7, 1);
    host_wait_ms(45);
    CHECK(sounding(0, 72), "OCT+: an octave up");
    host_key(7, 0);
    host_wait_ms(100);
    host_tap(B_OCTUP);
    host_tap(B_OCTUP);
    CHECK(song.octave == 2, "two octaves up at most");
}

static void t_keys_pads(void)
{
    uint32_t k;
    host_easy();
    easy_drums();                                   /* the drums */
    host_wait_ms(GN_PICK_MS);
    CHECK(song.sel == GN_DRUMS && gn_easy_of(&trk[3]) == GN_SND_KIT, "the drums: Easy's kit, the keys on track 4");
    for (k = 0; k < 27u; k++) {
        host_key(k, 1);
        host_wait_ms(30);
        if (key_black(k))
            CHECK(sounding(3, GN_BLACK_PADS[key_place(k)]), "the kit: the n-th black key (upper row) the kit's n-th other sound (round 13)");
        else
            CHECK(sounding(3, GN_PADS[key_place(k)]), "the kit: the n-th white key (lower row) is pad n");
        host_key(k, 0);
        host_wait_ms(900);                          /* (each hit plays out) */
    }
    CHECK(trk_midi_ch(3) == 3u, "the drums go out on MIDI channel 4");
}

static void t_keys_other(void)
{
    uint32_t k;
    host_easy();
    CHECK(gn_easy_of(&trk[0]) == GN_NOSND, "track 1 holds Felucca's ACID: another sound");
    for (k = 0; k < 27u; k += 5u) {
        host_key(k, 1);
        host_wait_ms(45);
        CHECK(sounding(0, gn_chromatic(&trk[0], k)), "another sound: every key plays, chromatic (round 13)");
        host_key(k, 0);
        host_wait_ms(150);
    }
}

static void t_gnome(void)
{
    uint32_t k;
    host_easy();
    host_turn(EN_ALGO, 1);
    host_wait_ms(GN_PICK_MS);
    for (k = 0; k < 27u; k++) {
        if (key_black(k))
            continue;
        host_key(k, 1);
        host_wait_ms(30);
        CHECK(trk[0].p[P_E0] == GN_SYLLABLE[key_place(k) & 7u][0] && trk[0].p[P_E1] == GN_SYLLABLE[key_place(k) & 7u][1],
              "the Gnome: each white key its syllable (VOWL, VOWL2)");
        host_key(k, 0);
        host_wait_ms(60);
    }
    host_key(7, 1);
    host_wait_ms(150);
    CHECK(gs_vowel() != 0u, "his mouth open while he sings");
    host_key(7, 0);
    host_wait_ms(900);
    CHECK(gs_vowel() == 0u, "..shut when he is silent");
}

/* ------------------------------------------------------- the knobs --- */
/* finding 1: a knob writes only to Easy's own sound, keyed on the engine the track really has, every
 * write in the engine's ranges; another sound (the app loaded it) and its bubbles rest */
static void t_knobs(void)
{
    music_t a, b;
    uint32_t e, pr, k, i, bt = 0, bid = 0, ok = 1, n = 0, picked = 1;
    host_easy();
    music_get(&a);
    for (k = 0; k < 4u; k++)
        host_turn(EN_K1 + k, k & 1u ? 9 : -9);
    music_get(&b);
    CHECK(music_same_fb(&a, &b, 0) && !music_same(&a, &b, 0), "another sound (ACID): the knobs move its engine's fallback map only (round 13)");
    host_turn(EN_ALGO, 2);                        /* the Organ */
    host_wait_ms(GN_PICK_MS);
    CHECK(trk[0].p[P_E3] == 0 && trk[0].p[P_E6] == 20 && gn.amount[0][0] == 8u, "the Organ: Bright at rest, TOP 0, DRV 20");
    host_turn(EN_K1, 8);
    CHECK(trk[0].p[P_E3] == 0 && trk[0].p[P_E6] == 127, "Bright up: its bars full, the drive up (DRV 127)");
    host_turn(EN_K1, -40);
    CHECK(trk[0].p[P_E3] == -8 && trk[0].p[P_E6] == 20 && gn.amount[0][0] == 0u, "Bright down: TOP -8, amount 0");
    host_turn(EN_K3, 40);
    CHECK(trk[0].p[P_DLY] == 127, "Echo up: the delay send");
    host_turn(EN_K4, 40);
    CHECK(trk[0].p[P_REV] == 127, "Space up: the reverb send");
    app_set(P_REV, 32);                             /* the app moves Space's setting: the bubble follows */
    host_wait_ms(30);
    CHECK(gn.amount[0][3] == 4u, "the app sets REV 32: the bubble follows (4 of 16)");
    /* the review's trigger: the Organ's mapping on a sound the app loads */
    app_preset(GE_FM6, 4);                          /* (round 24: FM6 PAD, none of Easy's; TINE EP is) */
    host_wait_ms(60);
    CHECK(gn_easy_of(&trk[0]) == GN_NOSND && gn_picture() == GN_OTHER, "the app loads FM6 PAD: another sound");
    music_get(&a);
    host_turn(EN_K1, -40);
    host_turn(EN_K2, 40);
    music_get(&b);
    CHECK(music_same_fb(&a, &b, 0) && trk[0].p[P_E2] == -64 && trk[0].p[P_LD_PIT] == 10,
          "..Bright down, Wobble up: FM6's MLVL and the LFO's pitch only (round 13: the fallback map)");
    app_preset(GE_WHEEL, 1);                        /* the Organ's engine, another preset: Easy's Jazz */
    host_wait_ms(60);
    CHECK(gn_easy_of(&trk[0]) == GN_GROUP[1] + 3u && str_eq(GN_SOUNDS[GN_GROUP[1] + 3u].name, "JAZZ") &&
          easy_whole_on(GN_GROUP[1] + 3u, 0) && gn_picture() == 1u, "WHEEL JAZZ PERC from the app: Easy's Jazz, whole");
    app_preset(GE_PHASE, 3);                        /* PHASE RESO: none of Easy's */
    host_wait_ms(60);
    CHECK(gn_easy_of(&trk[0]) == GN_NOSND, "PHASE RESO: another sound");
    /* every engine's every preset from the app over each of Easy's nine sounds, each picked first (from
     * the app's sound before it, another sound: PRESETS turned i + 1 detents is picture i) and checked
     * whole on its track: what the app loads is the app's. Every knob both ways and every key leave all
     * of it as the load left it: compared with the state right after the load, not with what Easy
     * thinks it holds (review 3: the old matrix let a write pass whenever gn_easy_of said "ours") */
    for (i = 0; i < GN_NPICS; i++)
        for (e = 0; e < NENGINES; e++)
            for (pr = 0; pr < (ENGINES[e]->npresets ? ENGINES[e]->npresets : 1u); pr++) {
                host_turn(EN_ALGO, (int32_t)i + 1);
                picked &= (uint32_t)easy_whole(i);
                if (easy_preset(e, pr)) {           /* (round 24: one of Easy's, adopted: t_r24_adopt) */
                    app_preset(GE_PHASE, 3);        /* (another sound again, for the next ALGORITHM turn) */
                    host_frame();
                    continue;
                }
                app_preset(e, pr);
                host_frame();
                music_get(&a);
                for (k = 0; k < 4u; k++) {
                    host_turn(EN_K1 + k, -20);
                    host_turn(EN_K1 + k, 20);
                }
                for (k = 0; k < 27u; k++)
                    host_key(k, 1);
                host_frame();
                for (k = 0; k < 27u; k++)
                    host_key(k, 0);
                host_frame();
                music_get(&b);
                if (!music_same_fb(&a, &b, song.sel) && ok)
                    printf("    picture %u, then the app's engine %u preset %u: written to\n", i, e, pr);
                ok &= (uint32_t)music_same_fb(&a, &b, song.sel);
                ok &= (uint32_t)params_in_range(&bt, &bid);
                n++;
            }
    if (!ok)
        printf("    track %u parameter %u out of its range (or written to the app's sound)\n", bt, bid);
    CHECK(picked, "each picture picked: Easy's whole sound on its track, every time");
    printf("    %u loads of another sound over a picture (round 24: the presets of Easy's own, adopted, are t_r24_adopt's)\n", n);
    CHECK(ok && n > 200u, "every engine's presets but Easy's own from the app, over each picture: keys write nothing, knobs only the fallback map");
}

/* review 3: ownership ends at every load Easy did not make. Each of the nine pictures, then: the same
 * factory sound loaded by the app (the Gnome, then VOICE preset 1; the drums, then SAMPLE preset 0:
 * ED_PRESET(4,0)), the engine away and back, a user sound, a project, a runtime backup put back, and a
 * writer no hook sees (the preset number changed and changed back under Easy). After each, the sound is
 * the one loaded and stays it under every key and knob, and every key plays as Felucca has it (read off
 * the track and the voices, never off gn_easy_of); the picture picked again puts Easy's whole sound
 * back (the drums set up again, not skipped) */
static void t_own_keys(uint32_t p, uint32_t *ok)    /* every key as Felucca plays it on track p */
{
    uint32_t k;
    for (k = 0; k < 27u; k++) {                     /* (black keys and white, both rows) */
        uint32_t note = gn_chromatic(&trk[p], k);  /* (round 13: chromatic) */
        host_key(k, 1);
        host_wait_ms(30);
        *ok &= (uint32_t)(kb_note[k] == note);      /* seq.c: the note the key sent */
        if (k % 4u == 1u && trk[p].eng_req != GE_SAMPLE)
            *ok &= (uint32_t)sounding(p, note);     /* (a sample set may have no zone at a note) */
        host_key(k, 0);
        host_wait_ms(30);
    }
}

static void t_ownership(void)
{
    static const char *const WHAT[10] = {"the same factory sound", "the engine away and back", "a user sound",
                                         "a project", "a runtime backup", "a writer no hook sees",
                                         "its SET or PTCH by SET", "PTCH to a bank slot, the slot put",
                                         "PTCH to a bank slot, the slot erased", ""};
    uint32_t i, w, ok_keep = 1, ok_keys = 1, ok_back = 1, ok_first = 1, ok_bank = 1, ok_adopt = 1;
    for (i = 0; i < GN_NSOUNDS; i++) {              /* every sound of every picture (round 7), on track 1 */
        const gn_sound_t *s = gn_sound_def(i);
        uint32_t p = GN_MELODY, fm6 = s->engine == GE_FM6;
        host_easy();
        app_fm6_bank_put(0);                        /* (FM6 bank slots 1 and 2: patches to point PTCH at) */
        app_fm6_bank_put(1);
        for (w = 0; w < 9u; w++) {
            music_t a, b;
            uint32_t k, keep = 1, keys = 1;
            if ((w == 6u && gn_id_param(s->engine) == P_COUNT) || (w > 6u && !fm6))
                continue;                           /* (no SET or PTCH; no bank slot to point at) */
            easy_pick_sound(i);                     /* (from another sound: sound i) */
            host_wait_ms(30);
            if (!easy_whole_on(i, p) || song.sel != p)
                printf("    sound %u (before %s): not Easy's whole sound (engine %u preset %u, track %u)\n", i, WHAT[w],
                       trk[p].eng_req, trk[p].preset, song.sel + 1u);
            ok_first &= (uint32_t)(easy_whole_on(i, p) && song.sel == p);
            switch (w) {
            case 0:                                 /* round 24: Easy's own sound from the app is adopted */
                app_preset(s->engine, s->preset);
                if (i == GN_SND_KIT)
                    CHECK(trk[p].p[P_E0] != GN_KIT_SET, "the drums, then ED_PRESET(4,0): SAMPLE preset 0's own SET");
                else {
                    host_frame();
                    ok_adopt &= (uint32_t)(gn_easy_of(&trk[p]) == i && easy_whole_on(i, p));
                    if (gn_easy_of(&trk[p]) != i)
                        printf("    sound %u, then the app's PRESET of it: not adopted (%u)\n", i, gn_easy_of(&trk[p]));
                    continue;
                }
                break;
            case 1:
                app_engine(s->engine == GE_ANALOG ? GE_PHASE : GE_ANALOG);
                app_engine(s->engine);
                break;
            case 2:
                app_user(10u + i % 16u);
                break;
            case 3:
                app_project(1, 1);
                app_project(0, 1);
                break;
            case 4:
                app_backup_take();
                CHECK(app_backup_restore() == 0u, "the runtime backup put back");
                break;
            case 5:
                trk[p].preset ^= 1u;
                host_frame();
                trk[p].preset ^= 1u;
                break;
            case 6:                                 /* the kit's SET, Bells' or the Piano's PTCH, by SET */
                if (fm6) {
                    app_fm6_bank_put(5);            /* (a bank slot no track plays: Easy's sound stays Easy's) */
                    app_fm6_bank_erase(6);
                    ok_bank &= (uint32_t)(gn_easy_of(&trk[p]) == i && easy_whole_on(i, p));
                }
                app_set(gn_id_param(s->engine), fm6 ? (int32_t)FM6_NFACTORY : 0);
                break;
            default:                                /* PTCH to bank slot 2, then that slot rewritten or erased */
                app_track_param(p, P_E7, (int32_t)FM6_NFACTORY + 1);
                host_frame();
                if (w == 7u)
                    app_fm6_bank_put(1);
                else
                    app_fm6_bank_erase(1);
                host_frame();
                break;
            }
            host_frame();
            music_get(&a);
            for (k = 0; k < 4u; k++) {
                host_turn(EN_K1 + k, -20);
                host_turn(EN_K1 + k, 20);
                host_turn(EN_K1 + k, -7);
            }
            for (k = 0; k < 27u; k++)
                host_key(k, 1);
            host_wait_ms(45);
            for (k = 0; k < 27u; k++)
                host_key(k, 0);
            host_wait_ms(45);
            t_own_keys(p, &keys);
            music_get(&b);
            keep = (uint32_t)music_same_fb(&a, &b, p);
            if (!keep || !keys)
                printf("    sound %u, then %s:%s%s\n", i, WHAT[w], keep ? "" : " written to", keys ? "" : " a key not Felucca's");
            if (!keep)
                music_diff(&a, &b);
            ok_keep &= keep;
            ok_keys &= keys;
        }
        easy_pick_sound(i);                         /* and the sound again: Easy's whole sound */
        host_wait_ms(30);
        ok_back &= (uint32_t)easy_whole_on(i, p);
    }
    CHECK(ok_first, "each sound of each picture: Easy's whole sound on its track");
    CHECK(ok_keep, "after each load Easy did not make: every key leaves the sound as loaded, the knobs move only the fallback map");
    CHECK(ok_keys, "..and every key plays as Felucca has it (the kit's upper row too)");
    CHECK(ok_back, "the picture again: Easy's whole sound back (the drums set up again)");
    CHECK(ok_bank, "an FM6 bank slot put or erased that Easy's Bells or Piano does not play: still Easy's");
    CHECK(ok_adopt, "round 24: each sound but the kit, then the app's PRESET of it: still that sound, whole, Easy's");
    /* the hook itself (gn_editor): a bank slot put or erased disowns every FM6 track on it. Easy's own
     * sounds never play a bank slot (a PTCH moved there is no longer Easy's), so the track is made one
     * here by hand: Easy's Bells, its PTCH, its patch and Easy's record of them moved to bank slot 2
     * together */
    for (w = 0; w < 2u; w++) {
        host_easy();
        host_turn(EN_ALGO, 3);                    /* the Bells */
        host_wait_ms(30);
        trk[0].p[P_E7] = (int16_t)(FM6_NFACTORY + 1u);
        fm6_poll();                                 /* (its patch loaded) */
        gn.own_id[0] = trk[0].p[P_E7];
        gn.own_gen[0] = fm6_pgen[0];
        host_frame();
        CHECK(gn_easy_of(&trk[0]) == GN_GROUP[2], "(the Bells on bank slot 2, Easy's)");
        if (w)
            app_fm6_bank_erase(1);
        else
            app_fm6_bank_put(1);
        CHECK(!gn.own[0], w ? "FM6 bank slot 2 erased: the track on it is no longer Easy's"
                            : "FM6 bank slot 2 put: the track on it is no longer Easy's");
    }
}

static int in_range(const param_desc_t *pd, int32_t v) { return v >= pd->min && v <= pd->max; }
static void t_sounds_defined(void)
{
    uint32_t s, k;
    for (s = 0; s < GN_NSOUNDS; s++) {
        const gn_sound_t *d = gn_sound_def(s);
        const engine_t *e = ENGINES[d->engine];
        CHECK(d->engine < NENGINES && d->engine != ENGI_DIGITAL && (!e->npresets || d->preset < e->npresets),
              "each sound: an engine 1.0.1 has, and one of its presets");
        CHECK(d->pic < GN_NPICS && d->name && d->name[0] && str_len(d->name) <= 15u, "each sound: a picture, a name (round CR: up to 15 letters, a bank name's longest)");
        for (k = 0; k < d->nset; k++)
            CHECK(d->set[k].id < P_COUNT, "each setting a parameter");
        for (k = 0; k < 4u; k++) {
            const gn_knob_t *m = gn_snd_knob(d, k);
            const param_desc_t *pd = m->id >= P_E0 ? &e->edit[m->id - P_E0] : &TP[m->id];
            CHECK(m->id < P_COUNT && in_range(pd, m->lo) && in_range(pd, m->mid) && in_range(pd, m->hi) &&
                  (m->lo != m->hi || (m->id2 != GN_NONE && m->lo2 != m->hi2)) && m->rest <= GN_STEPS,
                  "each knob's travel inside its setting's range, and a travel");
            if (m->id2 != GN_NONE) {
                const param_desc_t *p2 = m->id2 >= P_E0 ? &e->edit[m->id2 - P_E0] : &TP[m->id2];
                CHECK(m->id2 < P_COUNT && in_range(p2, m->lo2) && in_range(p2, m->mid2) && in_range(p2, m->hi2),
                      "a knob's second setting inside its range");
            }
        }
    }
    CHECK(GN_SOUNDS[GN_GROUP[2]].engine == GE_FM6, "the Bells: FM6");
    CHECK(str_eq(ENGINES[GE_VOICE]->name, "VOICE") && str_eq(ENGINES[GE_WHEEL]->name, "WHEEL") &&
          str_eq(ENGINES[GE_FM6]->name, "FM6") && str_eq(ENGINES[GE_LOFI]->name, "LOFI") &&
          str_eq(ENGINES[GE_GRAIN]->name, "GRAIN") && str_eq(ENGINES[GE_SAMPLE]->name, "SAMPLE") &&
          str_eq(ENGINES[GE_DRUM]->name, "DRUM"), "the engine numbers Easy names");
    CHECK(str_eq(SMP_ALL_NAMES[GN_KIT_SET], "PERC") && (int32_t)GN_KIT_SET == smp_perc_set(), "SET 4 is PERC");
    for (s = 0; s < 16u; s++) {                     /* each pad a zone of its own in the kit */
        const smp_set_t *set = &SMP_SETS[GN_KIT_SET];
        uint32_t i, width = 128u, hit = 0;
        for (i = 0; i < set->nz; i++) {
            const smp_zone_t *z = &SMP_ZONES[set->z0 + i];
            if (GN_PADS[s] >= z->lo && GN_PADS[s] <= z->hi && z->hi - z->lo <= width) {
                width = z->hi - z->lo;
                hit = 1;
            }
        }
        CHECK(hit && width <= 1u, "each pad a sound of its own (no tom or conga range)");
    }
}

/* the drums are set up only when the child picks them; picking them again keeps the kit as it is */
static void t_drums(void)
{
    host_easy();
    CHECK(trk[3].eng_req == GE_DRUM, "into Easy: track 4 still Felucca's DRUM KIT");
    easy_drums();
    host_wait_ms(60);
    CHECK(trk[3].eng_req == GE_SAMPLE && trk[3].p[P_E0] == GN_KIT_SET && trk[3].p[P_E3] == 0, "the drums: SAMPLE PERC");
    CHECK(trk[3].p[P_ATK] == 0 && trk[3].p[P_SUS] == 127 && trk[3].p[P_REL] >= 100 && trk[3].p[P_VOICE] == V_POLY,
          "..attack 0, sustain full, a long release, POLY");
    CHECK(trk[0].eng_req != GE_SAMPLE, "..on track 4 only");
    host_turn(EN_K1, -5);
    {
        int16_t cut = trk[3].p[P_E4];
        host_tap(B_FX);                             /* track 1, and back to the drums' track */
        host_tap(B_LFO);
        host_wait_ms(60);
        CHECK(trk[3].p[P_E4] == cut && gn_easy_of(&trk[3]) == GN_SND_KIT, "back on the drums' track: the kit as it was");
    }
}

/* round 7, pick 4: SELECT turns the real tempo, 40 to 240 (Felucca's range), and every Easy header shows
 * it; EDIT taps set it (Felucca's own tap tempo, the last four taps), the big number while tapping; with
 * an external clock the header shows the clock's tempo and SELECT and EDIT set nothing */
static void t_tempo(void)
{
    static const uint32_t PAGE[3] = {B_SEQ, B_GLO, B_SEQ};
    uint32_t i;
    int16_t k0;
    host_easy();
    CHECK(song.g[G_BPM] == 120, "(the tempo: Felucca's 120)");
    host_tempo(5);
    CHECK(song.g[G_BPM] == 125, "SELECT: a beat a detent (125)");
    host_tempo(-3);
    CHECK(song.g[G_BPM] == 122, "..and back (122)");
    for (i = 0; i < 30u; i++)
        host_tempo(-9);
    CHECK(song.g[G_BPM] == 40, "..down to 40, no further");
    for (i = 0; i < 30u; i++)
        host_tempo(9);
    CHECK(song.g[G_BPM] == 240, "..up to 240, no further");
    host_tempo(-140);
    CHECK(song.g[G_BPM] == 100, "(100)");
    for (i = 0; i < 4u; i++) {                       /* the header's number on every page */
        host_wait_ms(100);
        ui_draw();
        CHECK(gs.bpm == 100 && !gs.clock, "the header shows the tempo, on every page");
        if (i < 3u)
            host_tap(PAGE[i]);
    }
    host_tap(B_HOME);
    for (i = 0; i < 4u; i++) {                      /* EDIT tapped every 500 ms: 120 */
        host_tap(B_EDIT);
        CHECK(gn_view() == GV_TAP, "EDIT: the big number while tapping");
        host_wait_ms(500 - 45);                     /* (a tap is two frames: 495 ms apart) */
    }
    CHECK(song.g[G_BPM] >= 118 && song.g[G_BPM] <= 122, "four taps 500 ms apart: 120 BPM (a frame either way)");
    for (i = 0; i < 4u; i++) {
        host_tap(B_EDIT);
        host_wait_ms(400 - 30);
    }
    CHECK(song.g[G_BPM] >= 147 && song.g[G_BPM] <= 153, "..400 ms apart: 150");
    ui_draw();
    CHECK(gs.taps == 4u, "..four taps counted on the screen");
    host_wait_ms(GN_TAP_MS + 100u);
    CHECK(gn_view() == GV_HOME, "2 s after the last tap: the page again (Home, round UX-1)");
    /* an external clock: its tempo in the header, SELECT and EDIT set nothing */
    song.g[G_CLOCK] = 1;
    host_wait_ms(60);
    midi_beat_samples = FS * 60u / 90u;
    k0 = song.g[G_BPM];
    host_tempo(7);
    for (i = 0; i < 4u; i++) {
        host_tap(B_EDIT);
        host_wait_ms(300);
    }
    CHECK(song.g[G_BPM] == k0, "an external clock: SELECT and EDIT change nothing");
    ui_draw();
    CHECK(gs.clock && gs.bpm == 90, "..the header shows the clock's tempo (90)");
    CHECK(!ui.msg_t, "..and Felucca's message for it is not left for later");
}

/* round 7, pick 3: ALGORITHM picks the picture on the selected track (its first sound: rounds 1 to 6's), PRESETS
 * the sound inside it, round and round, with the peek list for 2 s and the play screen after; every sound of
 * every picture is put whole (its preset, GN_COMMON, its settings, its knobs at rest), and Easy owns it */
static void t_groups(void)
{
    uint32_t pic, i, all = 1, own = 1;
    char b[24];
    host_easy();
    host_tap(B_SCL);                                /* track 2 */
    host_turn(EN_PRESET, 1);
    CHECK(gn_easy_of(&trk[1]) == GN_NOSND && gn_view() != GV_PEEK, "another sound: PRESETS rests");
    host_turn(EN_ALGO, 3);                          /* from another sound: the third picture, the Bells */
    CHECK(gn_picture() == 2u && easy_whole_on(GN_GROUP[2], 1) && gn_view() == GV_PICK, "ALGORITHM: the Bells on track 2");
    CHECK(trk[0].eng_req == TRK_DEF[0][0], "..track 1 untouched");
    host_turn(EN_PRESET, 1);
    CHECK(gn_easy_of(&trk[1]) == GN_GROUP[2] + 1u && easy_whole_on(GN_GROUP[2] + 1u, 1) && gn_view() == GV_PEEK,
          "PRESETS: the Bells' second sound (PHASE BELL), the peek list");
    gs_head_words(1, gn_picture(), b);
    CHECK(str_eq(b, "BELLS * PHASE BELL"), "..the header: the picture and the sound");
    host_turn(EN_PRESET, -2);                       /* round: the last of the group */
    CHECK(gn_easy_of(&trk[1]) == GN_GROUP[3] - 1u, "PRESETS round the group: its last sound (KALIMBA)");
    host_wait_ms(GN_PEEK_MS + 100u);
    CHECK(gn_view() == GV_PEEK, "2 s after the last turn: the list stays (round FLOW-1, M2: no time-out)");
    host_tap(B_HOME);
    CHECK(gn_view() == GV_HOME && gn_easy_of(&trk[1]) == GN_GROUP[3] - 1u, "..HOME: the page again (Home), the sound kept");
    host_turn(EN_ALGO, -1);                         /* the picture before: the Organ's first sound */
    CHECK(gn_easy_of(&trk[1]) == GN_GROUP[1], "ALGORITHM back: the Organ (its first sound)");
    for (pic = 0; pic < GN_NPICS; pic++) {          /* every sound of every picture, on track 2 */
        host_turn(EN_ALGO, (int32_t)pic - (int32_t)gn_picture());
        for (i = GN_GROUP[pic]; i < GN_GROUP[pic + 1u]; i++) {
            if (i > GN_GROUP[pic])
                host_turn(EN_PRESET, 1);
            host_wait_ms(30);
            all &= (uint32_t)easy_whole_on(i, 1);
            own &= gn_easy_of(&trk[1]) == i;
        }
    }
    CHECK(all, "every sound of the ten pictures, put whole (preset, settings, knobs at rest)");
    CHECK(own, "..each Easy's, and the one Easy remembers on the track");
    CHECK(GN_GROUP[GN_NPICS] == GN_NSOUNDS, "(the groups cover the table)");
    for (i = 0; i < GN_NSOUNDS; i++) {             /* (round CR: any count of pictures) */
        for (pic = 0; pic + 1u < GN_NPICS && i >= GN_GROUP[pic + 1u]; pic++)
            ;
        if (GN_SOUNDS[i].pic != pic)
            all = 0;
    }
    CHECK(all, "each sound in its picture's group");
}

/* ------------------------------------------------- loop and undo --- */
static void t_loop(void)
{
    step_t before[NSTEP];
    host_easy();
    host_turn(EN_ALGO, 2);                        /* the Organ */
    host_wait_ms(GN_PICK_MS);
    memcpy(before, trk[0].step, sizeof before);
    easy_record();
    CHECK(song.rec == 1u && (song.playing || transport_req == 1u), "REC, PLAY, the count-in: track 1 records, the loop plays");
    CHECK(trk[0].p[P_SLEN] == 32 && trk[0].p[P_SDIV] == GN_DIV32, "..an empty pattern: one bar (round FLOW-1, spec S-5); a melody not snapped (1/32)");
    host_wait_ms(100);
    CHECK(gn_view() == GV_HOME, "..the page as it was (REC never leaves it)");
    key(7, 100);
    key(9, 100);
    host_tap(B_REC);
    CHECK(!song.rec && !seq_is_empty(&trk[0]), "REC again: stops recording; the notes are in");
    easy_rec_hold();
    CHECK(!memcmp(trk[0].step, before, sizeof before) && trk[0].p[P_SLEN] == 16 && trk[0].p[P_SDIV] == GN_DIV16,
          "SAVE held: the recording undone (its grid too)");
}

/* finding 3: the undo copy is Easy's own last recording only. After each case REC held changes
 * nothing at all: every track's sound and pattern, the globals (music_same), not only track 1's steps */
static void t_undo_rules(void)
{
    music_t m, m2;
    host_easy();
    host_turn(EN_ALGO, 2);
    host_wait_ms(GN_PICK_MS);
    some_steps(0, 60);
    app_project(1, 2);                              /* the app saves project 3 (these steps) */
    host_wait_ms(30);
    track_defaults_steps(&trk[0]);
    host_wait_ms(30);
    easy_record();                                      /* record into an empty track 1 */
    key(7, 100);
    host_tap(B_REC);
    host_tap(B_PLAY);
    host_wait_ms(60);
    app_project(0, 2);                              /* the app loads project 3 */
    host_wait_ms(60);
    music_get(&m);
    easy_rec_hold();
    music_get(&m2);
    CHECK(music_same(&m, &m2, 0), "the app loads a project; REC held: the project as loaded, all of it (finding 3)");
    /* a project whose patterns are the ones Easy's recording left (no pattern changes at the load) */
    easy_record();      
    key(9, 100);
    host_tap(B_REC);
    host_tap(B_PLAY);
    host_wait_ms(60);
    app_project(1, 3);                              /* saved as project 4 ... */
    app_project(0, 3);                              /* ... and loaded: another project, the same steps */
    host_wait_ms(60);
    music_get(&m);
    easy_rec_hold();
    music_get(&m2);
    CHECK(music_same(&m, &m2, 0), "..a loaded project with the same steps: no undo either");
    /* a step the app sets on another track, after Easy's recording: no undo either */
    easy_record();      
    key(7, 100);
    host_tap(B_REC);
    host_tap(B_PLAY);
    host_wait_ms(60);
    app_track_step(1, 5, 64);
    host_wait_ms(60);
    music_get(&m);
    easy_rec_hold();
    music_get(&m2);
    CHECK(music_same(&m, &m2, 0), "the app set a step of track 2: REC held undoes nothing");
    /* on the recorded track itself, after the recording */
    easy_record();      
    key(9, 100);
    host_tap(B_REC);
    host_tap(B_PLAY);
    host_wait_ms(60);
    app_track_step(0, 7, 66);
    host_wait_ms(60);
    music_get(&m);
    easy_rec_hold();
    music_get(&m2);
    CHECK(music_same(&m, &m2, 0), "the app set a step of track 1 after it: no undo");
    /* another track (SCL, then FX), or Advanced and back: none */
    easy_record();      
    key(11, 100);
    host_tap(B_REC);
    host_tap(B_PLAY);
    host_tap(B_SCL);
    host_tap(B_FX);
    music_get(&m);
    easy_rec_hold();
    music_get(&m2);
    CHECK(music_same(&m, &m2, 0), "another track and back: no undo");
}

/* review 2 (high): the app's writes while Easy records. The copy goes before the app's command runs,
 * whatever records: a STEP_SET while REC is armed, a LEN while armed, a TRACK_STEP while a recorded key
 * is still held (seq.c rh_n: its release still to come), a runtime backup put back whose steps are the
 * ones Easy's recording left (another sound). After each, REC held changes nothing at all */
static void t_undo_external(void)
{
    music_t m, m2;
    host_easy();
    host_turn(EN_ALGO, 2);                        /* the Organ: POLY, sustains */
    host_wait_ms(GN_PICK_MS);
    easy_record();      
    CHECK(song.rec == 1u && gn.undo_of == 1u, "REC: recording track 1, a copy for the undo");
    key(7, 40);
    app_step_set(3, 70);                            /* the app, during the recording */
    CHECK(gn.undo_of == 0u, "the app's STEP_SET while REC is armed: the copy dropped before it ran");
    key(9, 40);
    host_tap(B_REC);
    host_tap(B_PLAY);
    host_wait_ms(60);
    CHECK(trk[0].step[3].note[0] == 70u, "..the app's step is in");
    music_get(&m);
    easy_rec_hold();
    music_get(&m2);
    CHECK(music_same(&m, &m2, 0), "..REC held: nothing at all changes (the app's step, the notes, the rest)");
    easy_record();      
    CHECK(gn.undo_of == 1u, "REC again: a new copy");
    app_track_param(0, P_SLEN, 24);
    CHECK(gn.undo_of == 0u, "the app's LEN of track 1 while armed: dropped");
    key(11, 100);
    host_tap(B_REC);
    host_tap(B_PLAY);
    host_wait_ms(60);
    music_get(&m);
    easy_rec_hold();
    music_get(&m2);
    CHECK(music_same(&m, &m2, 0) && trk[0].p[P_SLEN] == 24, "..REC held: nothing changes, LEN the app's");
    easy_record();      
    host_key(12, 1);                                /* a key down, recorded, still held ... */
    host_wait_ms(150);
    host_tap(B_REC);                                /* ... after the recording stops */
    CHECK(gn.undo_of == 1u && trk[0].rh_n > 0u, "a recorded key still held after REC (its release to come)");
    app_track_step(0, 9, 66);
    CHECK(gn.undo_of == 0u, "the app's TRACK_STEP of track 1 while it is held: dropped");
    host_key(12, 0);
    host_wait_ms(60);
    host_tap(B_PLAY);
    host_wait_ms(60);
    music_get(&m);
    easy_rec_hold();
    music_get(&m2);
    CHECK(music_same(&m, &m2, 0) && trk[0].step[9].note[0] == 66u, "..REC held after: nothing changes");
    /* the runtime backup: the steps Easy's recording left, another sound */
    easy_record();      
    key(14, 100);
    host_tap(B_REC);
    host_tap(B_PLAY);
    host_wait_ms(60);
    CHECK(gn.undo_of == 1u, "a recording of Easy's: its copy");
    app_preset(GE_PHASE, 3);                        /* (a sound load: the steps stay, so does the copy) */
    host_wait_ms(30);
    CHECK(gn.undo_of == 1u, "..an app's sound load changes no step: the copy stays");
    app_backup_take();                              /* B: Easy's recorded steps, PHASE on track 1 */
    host_turn(EN_ALGO, 3);                        /* Easy's Bells there now (the steps as they were) */
    host_wait_ms(30);
    CHECK(gn.undo_of == 1u && trk[0].eng_req == GE_FM6, "..Easy's Bells on track 1, the copy still Easy's");
    CHECK(app_backup_restore() == 0u, "the app puts B back (BACKUP_PUT, project_restore_runtime)");
    CHECK(gn.undo_of == 0u && trk[0].eng_req == GE_PHASE, "..the copy dropped before it ran; B's sound");
    music_get(&m);
    easy_rec_hold();
    music_get(&m2);
    CHECK(music_same(&m, &m2, 0), "..REC held: B as restored, all of it (review 2)");
}

/* finding 4: an undo leaves no held recording behind to write a step back on release. The review's
 * case: in one recording, a note goes into step 5 on lap 1; on lap 2 a key held from step 1 makes
 * steps 2 to 5 TIEs (seq.c rec_hold keeps what step 5 held, the lap-1 note, to put it back if the key
 * is let go early in it); the undo fires early in step 5 and the key is let go there: rec_release
 * must not put the lap-1 note into the pattern the undo restored */
static void t_undo_held(void)
{
    step_t snap[NSTEP];
    uint32_t i, guard, half;
    host_easy();
    host_turn(EN_ALGO, 2);                        /* the Organ: POLY, sustains */
    host_wait_ms(GN_PICK_MS);
    host_tempo(-40);                      /* 80 BPM: a 1/16 step is 187.5 ms */
    for (i = 0; i < NSTEP; i++)
        step_clear(&trk[0].step[i]);
    trk[0].step[7].note[0] = 30;                    /* not empty (no two bars): LEN 8, a lap 1.5 s */
    trk[0].step[7].n = 1;
    trk[0].step[7].time = ST_NOTE;
    trk[0].step[7].vel = 90;
    trk[0].p[P_SLEN] = 8;
    trk[0].p[P_SDIV] = 2;
    host_wait_ms(30);
    memcpy(snap, trk[0].step, sizeof snap);
    easy_record();                                      /* lap 1: a note into step 5 */
    for (guard = 0; guard < 400u && trk[0].seq_idx != 5u; guard++)
        host_frame();
    key(12, 40);                                    /* (F4, white) */
    CHECK(trk[0].step[5].n == 1u, "lap 1: the note in step 5");
    for (guard = 0; guard < 400u && trk[0].seq_idx != 1u; guard++)
        host_frame();
    host_key(14, 1);                                /* lap 2: G4 held from step 1 */
    host_wait_ms(75);
    host_btn(B_SAVE, 1);                            /* SAVE held 0.7 s or more, let go early in step 5: the undo */
    {
        uint32_t t0 = fm1_ms;
        for (guard = 0; guard < 400u; guard++) {
            host_frame();
            if (fm1_ms - t0 >= 750u && trk[0].seq_idx == 5u &&
                trk[0].seq_pos < step_samples(&trk[0], div_samples((uint32_t)trk[0].p[P_SDIV]), 5) / 3u)
                break;
        }
    }
    host_btn(B_SAVE, 0);
    host_frame();
    half = !gn.undo_of && trk[0].seq_idx == 5u &&
           trk[0].seq_pos < step_samples(&trk[0], div_samples((uint32_t)trk[0].p[P_SDIV]), 5) / 2u;
    host_key(14, 0);                                /* let go there */
    host_frame();
    host_wait_ms(400);
    CHECK(guard < 200u && half, "the undo fired, and the key went up, in the first half of step 5");
    CHECK(!memcmp(trk[0].step, snap, sizeof snap), "a key let go after the undo writes nothing back (finding 4)");
}

/* ------------------------------------------------------- recording (pick 5) --- */
static uint32_t notes_in(uint32_t t)               /* steps of track t holding a note */
{
    uint32_t i, n = 0;
    for (i = 0; i < NSTEP; i++)
        n += trk[t].step[i].time == ST_NOTE && trk[t].step[i].n;
    return n;
}

static void wait_step(uint32_t t, uint32_t idx)    /* the playhead of track t into step idx */
{
    uint32_t g;
    for (g = 0; g < 600u && trk[t].seq_idx != idx; g++)
        host_frame();
}

/* REC arms and never leaves the page; stopped, PLAY counts in 4 3 2 1 (four beats) and then records; playing,
 * the recording waits for bar 1; another track while armed takes the arming; K4 is KEEP or REPLACE while
 * armed or recording (REPLACE clears what the recording passes over, once); REC held while playing punches
 * in until let go; undo is SAVE held, never when HOME joins; REC has no other hold */
static void t_rec(void)
{
    uint32_t t0, n;
    host_easy();
    host_turn(EN_ALGO, 2);                          /* the Organ on track 1 */
    host_wait_ms(GN_PICK_MS);
    host_tap(B_SEQ);                                /* (the loop page) */
    host_tap(B_REC);
    CHECK(gn.arm == GA_READY && !song.rec && !song.playing && gn.page == GP_LOOP && gn_view() == GV_LOOP,
          "REC: armed, nothing recording, the page as it was");
    ui_draw();
    CHECK(gs.rec == 1u && gs.replace == 0u, "..the header armed, K4's card KEEP");
    host_wait_ms(250);
    {
        uint32_t lit = 0, dark = 0, i;
        for (i = 0; i < 40u; i++) {
            host_frame();
            lit += host_led(panel.btn[B_REC]);
            dark += !host_led(panel.btn[B_REC]);
        }
        CHECK(lit && dark, "..REC's LED blinks while armed");
    }
    host_tap(B_PLAY);
    CHECK(gn.arm == GA_COUNT && !song.playing && gn_view() == GV_COUNT, "PLAY: the count-in, the loop not started yet");
    ui_draw();
    CHECK(gs.count == 4u, "..4");
    t0 = fm1_ms;
    host_wait_ms(1100);
    ui_draw();
    CHECK(gs.count == 2u && !song.rec, "..2 (a beat 500 ms at 120 BPM)");
    while (!song.rec && fm1_ms - t0 < 4000u)
        host_frame();
    CHECK(song.rec == 1u && (song.playing || transport_req == 1u) && fm1_ms - t0 >= 1980u && fm1_ms - t0 <= 2100u,
          "..after four beats: recording, the loop playing");
    host_wait_ms(60);
    CHECK(gn_view() == GV_LOOP, "..the page still the loop page");
    key(7, 80);
    key(9, 80);
    host_tap(B_REC);
    CHECK(!song.rec && !gn.arm && song.playing && notes_in(0) >= 2u, "REC again: recording off, the loop plays on, the notes in");
    /* playing: armed waits for bar 1 */
    wait_step(0, 5);
    host_tap(B_REC);
    wait_step(0, 20);
    key(14, 60);
    CHECK(!song.rec && notes_in(0) == 2u, "playing, armed: not yet recording before bar 1 (a key plays, nothing recorded)");
    wait_step(0, 1);
    CHECK(song.rec == 1u, "..recording from bar 1");
    host_tap(B_REC);
    /* another track while armed: the arming goes with it */
    host_tap(B_REC);
    host_tap(B_SCL);
    CHECK(gn.arm == GA_READY && song.sel == 1u, "armed, SCL: track 2 armed");
    wait_step(1, (uint32_t)trk[1].p[P_SLEN] - 2u);
    CHECK(!song.rec, "..not before its bar 1");
    wait_step(1, 1);
    CHECK(song.rec == 2u, "..track 2 records from its bar 1");
    host_tap(B_ENV);
    CHECK(gn.arm == GA_READY && !song.rec, "recording, ENV: track 3 armed, track 2 not recording");
    host_tap(B_REC);
    host_tap(B_FX);
    /* REPLACE: K4 while armed */
    n = notes_in(0);
    CHECK(n >= 2u, "(track 1's notes)");
    host_tap(B_REC);
    host_turn(EN_K4, 1);
    CHECK(gn.replace == 1u && gn.arm, "K4 while armed: REPLACE");
    ui_draw();
    CHECK(gs.replace == 1u, "..K4's card says so");
    {
        uint32_t g;
        for (g = 0; g < 900u && !song.rec; g++)
            host_frame();
    }
    wait_step(0, GN_LOOP - 1u);                     /* one pass recorded over: a whole lap */
    wait_step(0, 1);
    CHECK(song.rec && notes_in(0) == n, "REPLACE: nothing deleted while the take goes on (round 9)");
    key(7, 60);                                     /* (a key on the second lap) */
    host_wait_ms(200);
    host_turn(EN_K4, -1);
    CHECK(gn.replace == 0u, "K4 back: KEEP");
    host_turn(EN_K4, 1);
    host_tap(B_REC);                                /* the take ends: every step it crossed holds the take's only */
    CHECK(!song.rec && notes_in(0) == 1u && trk[0].step[0].n == 0u, "REC: the take ends; crossed steps keep only the take's (one note)");
    {
        int16_t rev = trk[0].p[P_REV];
        host_turn(EN_K4, 3);                        /* (not armed: K4 is Space again) */
        CHECK(gn.replace == 1u && gn_knob_role(3) == KR_LOOP && rev == trk[0].p[P_REV], "not armed: K4 is the loop page's VOLUME again");
    }
    /* punch in: REC held while playing */
    gn.replace = 0;                                 /* (KEEP: an overdub punch) */
    host_btn(B_REC, 1);
    host_wait_ms(200);
    CHECK(gn.prov && !gn.punch, "REC down while playing: a take that is a tap until 0.4 s");
    host_wait_ms(300);
    CHECK(song.rec == 1u && gn.punch && !gn.prov && !gn.arm, "REC held 0.4 s while playing: punched in");
    key(11, 60);
    host_btn(B_REC, 0);
    host_frame();
    CHECK(!song.rec && !gn.arm && !gn.punch, "..let go: punched out, nothing armed (no tap)");
    CHECK(notes_in(0) == 2u, "..the note punched in");
    /* undo: SAVE held; never with HOME */
    easy_rec_hold();
    CHECK(notes_in(0) == 1u, "SAVE held: the punch undone");
    host_tap(B_PLAY);
    host_wait_ms(60);
    easy_record();
    key(12, 60);
    host_tap(B_REC);
    host_tap(B_PLAY);
    host_wait_ms(60);
    n = notes_in(0);
    host_btn(B_SAVE, 1);
    host_wait_ms(800);
    host_btn(B_HOME, 1);
    host_wait_ms(100);
    host_btn(B_HOME, 0);
    host_wait_ms(100);
    host_btn(B_SAVE, 0);
    host_wait_ms(60);
    CHECK(notes_in(0) == n && gn.on, "SAVE held, HOME joins, both let go before 2 s: no undo");
    host_btn(B_HOME, 1);
    host_wait_ms(100);
    host_btn(B_SAVE, 1);
    host_wait_ms(900);
    host_btn(B_SAVE, 0);
    host_wait_ms(60);
    host_btn(B_HOME, 0);
    host_wait_ms(60);
    CHECK(notes_in(0) == n && gn.on, "HOME, then SAVE held 0.9 s, let go: no undo");
    host_hold_switch(2100);
    CHECK(!gn.on && notes_in(0) == n, "HOME + SAVE 2 s: Advanced, nothing undone");
    host_hold_switch(2100);
    host_wait_ms(3100);
    /* REC has no other hold: held 2 s while stopped it is a tap (armed), nothing else */
    host_btn(B_REC, 1);
    host_wait_ms(2100);
    host_btn(B_REC, 0);
    host_frame();
    CHECK(gn.arm == GA_READY && !song.rec && !song.playing, "REC held 2 s while stopped: armed, as a tap");
    CHECK(!(favorites.factory[15][30] & 2u), "the first-key style bit: reserved, 0 (count-in)");
}

/* ------------------------------------------------- the loop pages (picks 6 to 9) --- */
static void put_note(uint32_t t, uint32_t i, uint32_t note)
{
    step_clear(&trk[t].step[i]);
    trk[t].step[i].time = ST_NOTE;
    trk[t].step[i].n = 1;
    trk[t].step[i].note[0] = (uint8_t)note;
    trk[t].step[i].vel = 100;
}

/* SEQ: circle, steps, record, circle; on them K1-K4 are BARS, SWING, SNAP, VOLUME, back to Bright to Space off
 * them; BARS 1 2 4 8 and SNAP set the grid (SNAP off: 1/32 for up to 2 bars; on: 1/16; 8 bars: 1/8), each only
 * when turned, the music kept (spread exactly; merged onto the 16th before); the motion with the steps; both
 * rest while recording; the steps view shows the bar; a melody's first recording is not snapped, the drums' is */
static void t_loop_pages(void)
{
    uint32_t i, ok = 1;
    int16_t lvl;
    host_easy();
    host_turn(EN_ALGO, 2);                          /* the Organ on track 1 */
    host_wait_ms(GN_PICK_MS);
    for (i = 0; i < NSTEP; i++)
        step_clear(&trk[0].step[i]);
    trk[0].p[P_SDIV] = GN_DIV32;                    /* a melody of 2 bars at 1/32, notes on 32nds 0 4 9 63 */
    trk[0].p[P_SLEN] = 64;
    put_note(0, 0, 60);
    put_note(0, 4, 62);
    put_note(0, 9, 64);
    put_note(0, 63, 67);
    motion.count = 1;                               /* a motion event of track 1 on its step 8 */
    motion.event[0].place = (uint8_t)(0u << 6 | 8u);
    motion.event[0].param = P_DLY;
    motion.event[0].value = 50;
    host_tap(B_SEQ);
    ui_draw();
    CHECK(gs.bars == 2u && !gs.snap && gs.grid && gs.spb == 32u, "the loop page: 2 bars, SNAP off (1/32)");
    host_turn(EN_K1, 1);
    CHECK(trk[0].p[P_SDIV] == GN_DIV16 && trk[0].p[P_SLEN] == 64, "BARS 4: the grid 1/16 (64 steps of 1/32 hold 2 bars)");
    CHECK(trk[0].step[0].note[0] == 60u && trk[0].step[2].note[0] == 62u && trk[0].step[4].note[0] == 64u &&
          trk[0].step[31].note[0] == 67u && !trk[0].step[1].n && !trk[0].step[3].n, "..the notes onto their 16ths (32nd 9 to 16th 4)");
    CHECK(motion.event[0].place == 4u, "..the motion with them (32nd 8: 16th 4)");
    ui_draw();
    CHECK(gs.snap && gs.bars == 4u, "..SNAP shows on (1/16)");
    host_turn(EN_K3, -1);
    CHECK(trk[0].p[P_SDIV] == GN_DIV16, "SNAP off at 4 bars: refused (1/32 holds 2)");
    host_turn(EN_K1, 1);
    CHECK(trk[0].p[P_SDIV] == GN_DIV8 && trk[0].p[P_SLEN] == 64 && trk[0].step[0].note[0] == 60u &&
          trk[0].step[1].note[0] == 62u && trk[0].step[2].note[0] == 64u, "BARS 8: 1/8, the notes onto 8ths");
    host_turn(EN_K1, 9);
    CHECK(trk[0].p[P_SDIV] == GN_DIV8 && trk[0].p[P_SLEN] == 64, "..no further than 8");
    host_turn(EN_K1, -1);
    CHECK(trk[0].p[P_SDIV] == GN_DIV16 && trk[0].p[P_SLEN] == 64 && trk[0].step[2].note[0] == 62u &&
          trk[0].step[4].note[0] == 64u, "BARS 4: 1/16 again, spread back exactly");
    host_turn(EN_K1, -1);
    CHECK(trk[0].p[P_SDIV] == GN_DIV16 && trk[0].p[P_SLEN] == 32, "BARS 2: the 1/16 grid kept, LEN 32");
    host_turn(EN_K3, -1);
    CHECK(trk[0].p[P_SDIV] == GN_DIV32 && trk[0].p[P_SLEN] == 64 && trk[0].step[4].note[0] == 62u &&
          trk[0].step[8].note[0] == 64u && !trk[0].step[2].n, "SNAP off: 1/32, every step spread onto every other 32nd");
    host_turn(EN_K3, 1);
    CHECK(trk[0].p[P_SDIV] == GN_DIV16 && trk[0].p[P_SLEN] == 32 && trk[0].step[2].note[0] == 62u, "SNAP on: back, the same");
    host_turn(EN_K2, 2);
    CHECK(trk[0].p[P_SSWING] == 10, "K2 SWING: 10 %");
    lvl = trk[0].p[P_LEVEL];
    host_turn(EN_K4, -2);
    CHECK(trk[0].p[P_LEVEL] == lvl - 8, "K4 VOLUME");
    host_tap(B_SEQ);
    ui_draw();
    CHECK(gn_view() == GV_STEPS && !gs.drums && gs.pitch[0] == 60u && gs.pitch[2] == 62u && gs.pitch[4] == 64u &&
          !gs.pitch[1], "the steps view: bar 1 up close, the melody's pitches");
    host_tap(B_HOME);
    host_turn(EN_K1, 3);
    CHECK(trk[0].p[P_SDIV] == GN_DIV16 && trk[0].p[P_SLEN] == 32 && gn.amount[0][0] == 11u, "off the loop page: K1 is Bright again");
    /* the record screen, stopped: keys at the cursor */
    host_tap(B_SEQ);
    host_tap(B_SEQ);
    host_tap(B_SEQ);
    CHECK(gn_view() == GV_RECORD && gn.cursor == 0u, "SEQ, SEQ, SEQ: the record screen, the cursor at the start");
    host_turn(EN_K1, 5);
    CHECK(gn.cursor == 5u, "K1: the cursor moves");
    key(7, 40);                                     /* C4 */
    CHECK(trk[0].step[5].n == 1u && trk[0].step[5].note[0] == 60u && gn.cursor == 6u && gn.undo_of == 1u,
          "a key: its note at the cursor, the cursor on, an undo copy");
    key(9, 40);
    CHECK(trk[0].step[6].note[0] == 62u && gn.cursor == 7u, "..the next key at the next step");
    {
        step_t after[4];                            /* the steps after the cursor, never K2's */
        memcpy(after, &trk[0].step[7], sizeof after);
        host_turn(EN_K1, -1);
        host_turn(EN_K2, -1);
        CHECK(!trk[0].step[6].n && trk[0].step[6].time == ST_REST && gn.cursor == 6u, "K2 down: a rest there, in place");
        host_turn(EN_K2, -3);
        CHECK(!trk[0].step[6].n && gn.cursor == 6u && !memcmp(after, &trk[0].step[7], sizeof after),
              "K2 turned on writes the same step again, the cursor stays, the steps after it untouched");
        host_turn(EN_K2, 1);
        CHECK(trk[0].step[6].n == 1u && trk[0].step[6].note[0] == 62u && gn.cursor == 6u, "K2 up: the last note again, in place");
        host_turn(EN_K2, -1);
        host_turn(EN_K2, 1);
        CHECK(trk[0].step[6].note[0] == 62u && gn.cursor == 6u && !memcmp(after, &trk[0].step[7], sizeof after),
              "K2 down and up again: the one step a rest then the note, the cursor never moves");
    }
    host_turn(EN_K1, 2);
    CHECK(gn.cursor == 8u, "K1 (MOVE) alone moves the cursor");
    host_turn(EN_K1, -2);
    easy_rec_hold();
    CHECK(trk[0].p[P_SDIV] == GN_DIV32 && trk[0].p[P_SLEN] == 64 && trk[0].step[4].note[0] == 62u &&
          trk[0].step[9].note[0] == 64u && trk[0].step[63].note[0] == 67u && !trk[0].step[5].n &&
          motion.event[0].place == 8u, "SAVE held: BARS, SNAP and the step entry of this session undone (grid, notes, motion)");
    /* while recording BARS and SNAP rest */
    host_tap(B_HOME);
    easy_record();
    host_tap(B_SEQ);
    {
        int16_t div = trk[0].p[P_SDIV], len = trk[0].p[P_SLEN];
        host_turn(EN_K1, 1);
        host_turn(EN_K3, 1);
        CHECK(trk[0].p[P_SDIV] == div && trk[0].p[P_SLEN] == len, "recording: BARS and SNAP rest");
    }
    host_tap(B_REC);
    host_tap(B_PLAY);
    host_tap(B_HOME);
    /* the defaults of a first recording (pick 9): the drums snapped, a melody not */
    host_tap(B_LFO);
    host_turn(EN_ALGO, -1);                         /* the drums on track 4 */
    for (i = 0; i < NSTEP; i++)
        step_clear(&trk[3].step[i]);
    trk[3].p[P_SDIV] = GN_DIV32;
    easy_record();
    CHECK(trk[3].p[P_SDIV] == GN_DIV16 && trk[3].p[P_SLEN] == 16, "the drums' first recording: snapped (1/16, 1 bar: round FLOW-1)");
    host_tap(B_REC);
    host_tap(B_PLAY);
    host_tap(B_ENV);
    host_turn(EN_ALGO, 1);                          /* the Gnome on track 3 */
    for (i = 0; i < NSTEP; i++)
        step_clear(&trk[2].step[i]);
    trk[2].p[P_SDIV] = GN_DIV16;
    easy_record();
    CHECK(trk[2].p[P_SDIV] == GN_DIV32 && trk[2].p[P_SLEN] == 32, "a melody's first recording: not snapped (1/32, 1 bar: round FLOW-1)");
    /* SNAP off records between the 16ths: a key between steps 2 and 3 of 1/16 lands on 32nd 5 */
    {
        uint32_t g;
        for (g = 0; g < 900u && !(trk[2].seq_idx == 5u && trk[2].seq_pos < 200u); g++)
            host_frame();
        key(12, 20);
        ok = trk[2].step[5].n == 1u || trk[2].step[6].n == 1u;
        CHECK(ok, "SNAP off: a key at the 5th 32nd lands on it (not on a 16th's)");
    }
    host_tap(B_REC);
    host_tap(B_PLAY);
}

/* ------------------------------------------------------- round 8 (the review of e8cda2e) --- */
/* (2), (4): a grid change keeps the motion store valid (the pair's first fine step wins), so the project can be
 * saved and backed up; SAVE held puts the whole motion store back, byte for byte */
static int motion_set_same(const motion_store_t *a, const motion_store_t *b)   /* the same events, any order */
{
    uint32_t i, j, ok = a->count == b->count && a->on == b->on;
    for (i = 0; ok && i < a->count; i++) {
        for (j = 0; j < b->count && memcmp(&a->event[i], &b->event[j], sizeof a->event[i]); j++)
            ;
        ok = j < b->count;
    }
    return (int)ok;
}

static void t_r8_motion(void)
{
    motion_store_t before;
    uint32_t i, len;
    host_easy();
    host_turn(EN_ALGO, 2);                          /* the Organ on track 1 */
    host_wait_ms(GN_PICK_MS);
    for (i = 0; i < NSTEP; i++)
        step_clear(&trk[0].step[i]);
    trk[0].p[P_SDIV] = GN_DIV32;
    trk[0].p[P_SLEN] = 64;
    put_note(0, 8, 60);
    app_motion_event(0, 8, P_DLY, 20);              /* the same setting on 32nds 8 and 9: one 16th */
    app_motion_event(0, 9, P_DLY, 90);
    app_motion_event(0, 9, P_REV, 40);              /* odd steps, the last one */
    app_motion_event(0, 63, P_REV, 70);
    app_motion_event(1, 9, P_DLY, 30);              /* (another track's: untouched) */
    host_wait_ms(60);
    before = motion;
    CHECK(motion_valid(&motion) && motion.count == 5u, "(five motion events, valid)");
    host_tap(B_SEQ);
    host_turn(EN_K3, 1);                            /* SNAP on: 1/32 to 1/16 */
    CHECK(trk[0].p[P_SDIV] == GN_DIV16, "SNAP on: the grid 1/16");
    CHECK(motion_valid(&motion) && motion.count == 4u, "..the motion store valid: the twin on 32nd 9 dropped");
    for (i = 0; i < motion.count; i++)
        if (motion.event[i].place == 4u && motion.event[i].param == P_DLY)
            CHECK(motion.event[i].value == 20, "..the pair's first fine step's event won (32nd 8)");
    CHECK(project_save(3) == 0, "..the project saves");
    app_backup_take();
    CHECK(ed_bk_object(0, &len) && len == sizeof(project_store_t), "..and the runtime backup captures it");
    host_turn(EN_K1, 3);                            /* BARS 8: 1/8 (more merging) */
    CHECK(motion_valid(&motion), "BARS 8: still valid");
    easy_rec_hold();
    CHECK(motion_set_same(&motion, &before) && trk[0].p[P_SDIV] == GN_DIV32 && trk[0].p[P_SLEN] == 64,
          "SAVE held: the motion store as before, every event (odd steps, step 63), the grid back");
    CHECK(motion_valid(&motion) && project_save(3) == 0, "..valid, and saves");
}

/* (3): an armed count-in, a ready arm, a pending SAVE end when the music is replaced (a project loaded, the
 * runtime backup put back): nothing starts recording into the new music */
static void t_r8_replaced(void)
{
    music_t b, now;
    uint32_t w;
    for (w = 0; w < 2u; w++) {
        host_easy();
        host_turn(EN_ALGO, 2);
        host_wait_ms(GN_PICK_MS);
        some_steps(0, 44);                          /* B: notes, saved in slot 2 (or the runtime backup) */
        if (w)
            app_backup_take();
        else
            app_project(1, 1);
        host_wait_ms(GN_SAVED_MS + 100u);
        music_get(&b);
        {
            uint32_t i;
            for (i = 0; i < NSTEP; i++)
                step_clear(&trk[0].step[i]);         /* A: another pattern */
        }
        host_tap(B_REC);
        host_turn(EN_K4, 1);                        /* REPLACE */
        host_tap(B_PLAY);
        CHECK(gn.arm == GA_COUNT, "(A: counting in, REPLACE)");
        if (w)
            CHECK(app_backup_restore() == 0u, "(the runtime backup put back during the count-in)");
        else
            app_project(0, 1);                      /* B loaded during the count-in */
        host_wait_ms(3000);
        music_get(&now);
        CHECK(!song.playing && !song.rec && !gn.arm && transport_req != 1u, "the music replaced: no recording, no loop");
        CHECK(music_same(&b, &now, 0), "..B as it was, all of it");
    }
}

/* (5): REPLACE takes out only the notes of steps the take enters; a short punch leaves the rest as it was;
 * KEEP before a step leaves it */
static void t_r8_replace_entry(void)
{
    step_t snap[NSTEP];
    uint32_t i, g;
    host_easy();
    host_turn(EN_ALGO, 2);
    host_wait_ms(GN_PICK_MS);
    host_tempo(-80);                      /* 40 BPM: a 1/16 step is 375 ms */
    for (i = 0; i < NSTEP; i++)
        step_clear(&trk[0].step[i]);
    trk[0].p[P_SDIV] = GN_DIV16;
    trk[0].p[P_SLEN] = 16;
    put_note(0, 2, 62);
    put_note(0, 5, 65);
    gn.replace = 1;
    host_tap(B_PLAY);
    for (g = 0; g < 900u && !(trk[0].seq_idx == 0u && trk[0].seq_pos < 2000u); g++)
        host_frame();
    memcpy(snap, trk[0].step, sizeof snap);
    host_btn(B_REC, 1);                             /* a punch from early in step 0 .. */
    host_wait_ms(450);
    host_btn(B_REC, 0);                             /* .. let go in step 1 */
    host_frame();
    CHECK(trk[0].seq_idx == 1u, "(the punch ended in step 1)");
    CHECK(!memcmp(&trk[0].step[2], &snap[2], sizeof snap[2]) && !memcmp(&trk[0].step[5], &snap[5], sizeof snap[5]),
          "a short REPLACE punch: the steps it never entered are as they were");
    host_tap(B_PLAY);
}

/* (6): the default grid only for an untouched empty pattern: BARS and SNAP the child chose stay */
static void t_r8_defaults(void)
{
    uint32_t c, i;
    for (c = 0; c < 4u; c++) {
        static const int32_t K1[4] = {9, -9, 1, -9}, K3[4] = {0, 1, -1, 0};
        int16_t div, len;
        host_easy();
        host_turn(EN_ALGO, 2);
        host_wait_ms(GN_PICK_MS);
        for (i = 0; i < NSTEP; i++)
            step_clear(&trk[0].step[i]);
        trk[0].p[P_SDIV] = GN_DIV16;
        trk[0].p[P_SLEN] = 16;
        host_tap(B_SEQ);
        if (K3[c])
            host_turn(EN_K3, K3[c]);
        host_turn(EN_K1, K1[c]);                    /* BARS 8, BARS 1 (SNAP on), .. */
        div = trk[0].p[P_SDIV];
        len = trk[0].p[P_SLEN];
        if (c & 1u) {
            transport_req = 1;                      /* (a start while playing: bar 1) */
            host_wait_ms(100);
        }
        easy_record();
        CHECK(song.rec && trk[0].p[P_SDIV] == div && trk[0].p[P_SLEN] == len,
              "an empty pattern whose BARS or SNAP the child set: recording keeps them (stopped and playing)");
        host_tap(B_REC);
        host_tap(B_PLAY);
    }
}

/* (7): the loop's own level motion retires Easy's claim to the level (an equal value too): leaving keeps it */
static void t_r8_level_motion(void)
{
    host_power_on();
    host_wait_ms(60);
    app_track_mix(1, 76, 0);
    app_motion_event(1, 0, P_LEVEL, 96);            /* track 2's own motion writes 96 */
    host_wait_ms(1600);
    host_hold_switch(2100);
    host_wait_ms(3100);
    host_tap(B_GLO);
    host_turn(EN_K2, 5);                            /* GLO: 76 + 20 = 96, a claim to put 76 back */
    CHECK(trk[1].p[P_LEVEL] == 96 && (gn.kept_mask[1] & 1u), "(GLO: track 2 at 96, a claim to 76)");
    host_tap(B_PLAY);
    host_wait_ms(600);                              /* the loop plays: its motion writes 96 (the same value) */
    CHECK(!(gn.kept_mask[1] & 1u), "the loop's level motion: the claim retired");
    host_turn(EN_K2, -1);
    CHECK(!(gn.kept_mask[1] & 1u), "..and none taken while it plays");
    host_tap(B_PLAY);
    host_wait_ms(100);
    host_hold_switch(2100);
    CHECK(!gn.on && trk[1].p[P_LEVEL] != 76, "leaving: the level not put back to 76");
}

/* (8): what each knob shows is what it does, under every screen; K4 KEEP / REPLACE over GLO while armed */
static void t_r8_knob_map(void)
{
    static const uint32_t PAGE_BTN[3] = {B_SEQ, B_GLO, 0};
    uint32_t pg, ov, k, ok = 1;
    host_easy();
    host_turn(EN_ALGO, 2);
    host_wait_ms(GN_PICK_MS);
    for (pg = 0; pg < 3u; pg++) {
        host_tap(B_HOME);
        if (PAGE_BTN[pg])
            host_tap(PAGE_BTN[pg]);
        for (ov = 0; ov < 4u; ov++) {               /* the peek list, tap tempo, the count-in, saved */
            if (ov == 0u)
                host_turn(EN_PRESET, 1);
            else if (ov == 1u)
                host_tap(B_EDIT);
            else if (ov == 2u) {
                host_tap(B_REC);
                host_tap(B_PLAY);
            } else {
                uint8_t a[2] = {1, 0};
                host_request(ED_PROJECT, a, 2);
            }
            host_frame();
            ui_draw();
            for (k = 0; k < 4u; k++)
                ok &= gs.role[k] == gn_knob_role(k);
            if (pg == 0u) {                         /* the loop page under the overlay: K1 is BARS, shown so */
                int16_t len = trk[0].p[P_SLEN];
                ok &= gs.role[0] == KR_LOOP;
                if (!gn.arm && !song.rec) {
                    host_turn(EN_K1, 1);
                    ok &= trk[0].p[P_SLEN] != len || gn_bars(0) == 8u;
                }
            }
            if (gn.arm == GA_COUNT) {
                host_tap(B_PLAY);
                host_tap(B_REC);
            }
            host_wait_ms(GN_SAVED_MS + 100u);
        }
    }
    CHECK(ok, "under every overlay the knob row shows what the knobs do (the loop page's BARS too)");
    host_tap(B_HOME);
    host_tap(B_GLO);
    host_tap(B_REC);
    ui_draw();
    CHECK(gn_knob_role(3) == KR_KEEP && gs.role[3] == KR_KEEP, "armed on GLO: K4 is KEEP / REPLACE, shown so");
    {
        int16_t lvl = trk[3].p[P_LEVEL];
        host_turn(EN_K4, 1);
        CHECK(gn.replace == 1u && trk[3].p[P_LEVEL] == lvl, "..K4 switches REPLACE, not track 4's volume");
    }
}

/* (10): a punch keeps the keys played during REC's 0.4 s hold; a tap under 0.4 s only arms */
static void t_r8_punch_keys(void)
{
    uint32_t i, n;
    host_easy();
    host_turn(EN_ALGO, 2);
    host_wait_ms(GN_PICK_MS);
    for (i = 0; i < NSTEP; i++)
        step_clear(&trk[0].step[i]);
    trk[0].p[P_SDIV] = GN_DIV16;
    trk[0].p[P_SLEN] = 32;
    host_tap(B_PLAY);
    host_wait_ms(300);
    host_btn(B_REC, 1);
    host_wait_ms(60);
    key(12, 40);                                    /* a key in the hold, before the punch */
    host_wait_ms(400);
    CHECK(song.rec == 1u, "(punched in)");
    for (i = 0, n = 0; i < NSTEP; i++)
        n += trk[0].step[i].n && trk[0].step[i].note[0] == 65u;
    CHECK(n == 1u, "..the key played during the hold is in");
    host_btn(B_REC, 0);
    host_frame();
    host_btn(B_REC, 1);                             /* a tap with a key: armed, the key not recorded */
    host_wait_ms(30);
    key(14, 30);
    host_btn(B_REC, 0);
    host_frame();
    for (i = 0, n = 0; i < NSTEP; i++)
        n += trk[0].step[i].n && trk[0].step[i].note[0] == 67u;
    CHECK(gn.arm == GA_READY && !song.rec && n == 0u, "a tap under 0.4 s: armed only, its key not recorded");
    host_tap(B_REC);
    host_tap(B_PLAY);
}

/* ------------------------------------------------------- round 9 (the recheck of 180e412) --- */
static void organ_empty(uint32_t len, uint32_t div)   /* the Organ on track 1, an empty pattern */
{
    uint32_t i;
    host_easy();
    host_turn(EN_ALGO, 2);
    host_wait_ms(GN_PICK_MS);
    for (i = 0; i < NSTEP; i++)
        step_clear(&trk[0].step[i]);
    trk[0].p[P_SDIV] = (int16_t)div;
    trk[0].p[P_SLEN] = (int16_t)len;
}

/* (3): undo's motion only while no one else can have written motion since (an ED_PRESET clears the track's),
 * and only the undone track's */
static void t_r9_motion_gen(void)
{
    uint32_t i, n0 = 0;
    organ_empty(64, GN_DIV32);
    put_note(0, 8, 60);
    app_motion_event(0, 8, P_DLY, 20);
    app_motion_event(1, 3, P_DLY, 30);
    host_wait_ms(60);
    host_tap(B_SEQ);
    host_turn(EN_K3, 1);                            /* SNAP on: an undo copy with the motion */
    app_preset(GE_WHEEL, 0);                        /* the app loads a sound: track 1's motion gone */
    host_wait_ms(60);
    for (i = 0; i < motion.count; i++)
        n0 += (motion.event[i].place >> 6) == 0u;
    easy_rec_hold();
    for (i = 0, n0 = 0; i < motion.count; i++)
        n0 += (motion.event[i].place >> 6) == 0u;
    CHECK(n0 == 0u, "an ED_PRESET after the edit: SAVE held brings no old motion back");
    CHECK(trk[0].p[P_SDIV] == GN_DIV32, "..the steps and grid undone as before");
    for (i = 0, n0 = 0; i < motion.count; i++)
        n0 += (motion.event[i].place >> 6) == 1u && motion.event[i].value == 30;
    CHECK(n0 == 1u, "..track 2's motion untouched");
}

/* (4): REPLACE keeps what the take played, the same pitch as an old note too; ties the take held; the first
 * step of the take; steps it never crossed as they were */
static void t_r9_replace(void)
{
    step_t snap[NSTEP];
    uint32_t g;
    organ_empty(16, GN_DIV16);
    host_tempo(-60);                      /* 60 BPM: a 1/16 step is 250 ms */
    put_note(0, 4, 60);
    put_note(0, 5, 62);
    put_note(0, 12, 64);
    gn.replace = 1;
    host_tap(B_PLAY);
    for (g = 0; g < 900u && !(trk[0].seq_idx == 3u && trk[0].seq_pos > 9000u); g++)
        host_frame();
    memcpy(snap, trk[0].step, sizeof snap);
    host_btn(B_REC, 1);                             /* a punch from late in step 3 .. */
    host_frame();
    host_key(7, 1);                                 /* C4 (60): the same pitch as step 4's old note, into step 4 */
    host_wait_ms(600);                              /* .. held into steps 5 and 6 */
    host_key(7, 0);
    host_wait_ms(200);
    host_btn(B_REC, 0);                             /* .. let go in step 6 or 7 */
    host_frame();
    CHECK(trk[0].step[4].time == ST_NOTE && trk[0].step[4].n == 1u && trk[0].step[4].note[0] == 60u,
          "REPLACE: the take's C4 in step 4 kept (the same pitch as the old note there)");
    CHECK(trk[0].step[5].time == ST_TIE, "..its hold a tie over step 5 (the old D4 there gone)");
    CHECK(!memcmp(&trk[0].step[12], &snap[12], sizeof snap[12]) && !memcmp(&trk[0].step[0], &snap[0], sizeof snap[0]),
          "..steps the take never crossed as they were");
    host_tap(B_PLAY);
}

/* (5): a START (here: a restart between two frames, GLO + PLAY's way) ends the take there, never a wrap that
 * clears the rest; a STOP during a punch drops its arming too */
static void t_r9_restart(void)
{
    step_t snap[NSTEP];
    uint32_t g;
    organ_empty(16, GN_DIV16);
    host_tempo(-60);
    put_note(0, 9, 64);
    put_note(0, 14, 67);
    gn.replace = 1;
    host_tap(B_PLAY);
    host_wait_ms(100);
    memcpy(snap, trk[0].step, sizeof snap);
    host_btn(B_REC, 1);
    host_wait_ms(500);
    CHECK(gn.punch, "(punching in)");
    for (g = 0; g < 900u && trk[0].seq_idx != 4u; g++)
        host_frame();
    transport_req = 3;                              /* STOP and START between two frames */
    host_frame();
    host_frame();
    CHECK(!memcmp(&trk[0].step[9], &snap[9], sizeof snap[9]) && !memcmp(&trk[0].step[14], &snap[14], sizeof snap[14]),
          "a restart during a REPLACE take: the steps it never crossed (9, 14) as they were");
    host_btn(B_REC, 0);
    host_frame();
    host_btn(B_REC, 1);                             /* a punch, then STOP */
    host_wait_ms(500);
    host_tap(B_PLAY);
    host_btn(B_REC, 0);
    host_wait_ms(100);
    CHECK(!gn.arm && !song.rec && !song.playing, "STOP during a punch: the take ends, nothing left armed");
    host_tap(B_PLAY);
    host_wait_ms(1500);
    CHECK(!song.rec, "..PLAY again: no recording without a new REC");
    host_tap(B_PLAY);
}

/* (5) through the host's MIDI queue: an external clock's START, CONTINUE and STOP while a REPLACE take goes on */
static void midi_rt(uint32_t st) { midi_enqueue(st << 8 | 0x0Fu, 1); }
static void clock_frames(uint32_t n)              /* frames with a clock pulse each (USB, about 160 BPM) */
{
    while (n--) {
        midi_rt(0xF8);
        host_frame();
    }
}

static void t_r9_midi_start(void)
{
    step_t snap[NSTEP];
    uint32_t g;
    organ_empty(16, GN_DIV16);
    put_note(0, 9, 64);
    put_note(0, 14, 67);
    song.g[G_CLOCK] = 1;                            /* the clock from USB */
    host_frame();
    midi_rt(0xFA);                                  /* START */
    clock_frames(3);
    CHECK(song.playing, "(the external clock plays)");
    gn.replace = 1;
    memcpy(snap, trk[0].step, sizeof snap);
    host_btn(B_REC, 1);
    clock_frames(30);
    CHECK(gn.punch && trk[0].seq_idx < 8u, "(punching in, before step 8)");
    (void)g;
    midi_rt(0xFA);                                  /* START again, mid-take */
    clock_frames(2);
    CHECK(!memcmp(&trk[0].step[9], &snap[9], sizeof snap[9]) && !memcmp(&trk[0].step[14], &snap[14], sizeof snap[14]),
          "an external START mid-take: a new start, not a wrap; steps 9 and 14 as they were");
    midi_rt(0xFC);                                  /* STOP, then CONTINUE */
    clock_frames(2);
    CHECK(!song.playing && !gn.take_on, "..STOP: the take ended");
    midi_rt(0xFB);
    clock_frames(20);
    host_btn(B_REC, 0);
    host_frame();
    CHECK(!song.rec && !gn.arm, "..CONTINUE: no recording without a new REC");
    CHECK(!memcmp(&trk[0].step[14], &snap[14], sizeof snap[14]), "..step 14 still as it was");
    midi_rt(0xFC);
    clock_frames(2);
}

/* (6): the music replaced mid-gesture: a REC under 0.4 s, an active punch, a SAVE held: none acts afterwards,
 * no button does until all are up; a recorded note held over the replacement writes nothing on release */
static void t_r9_cancel(void)
{
    organ_empty(32, GN_DIV16);
    host_tap(B_PLAY);
    host_wait_ms(100);
    host_btn(B_REC, 1);                             /* REC under 0.4 s ... */
    host_wait_ms(100);
    app_step_set(20, 70);                           /* ... the app writes a step */
    host_wait_ms(500);
    CHECK(!song.rec && !gn.punch && !gn.prov, "a REC press under way when the app writes: no punch (past 0.4 s)");
    host_btn(B_REC, 0);
    host_frame();
    CHECK(!gn.arm && !song.rec, "..its release: no tap, not armed");
    CHECK(trk[0].step[20].note[0] == 70u, "..the app's step as written");
    host_btn(B_REC, 1);                             /* an active punch, a key held, the app's write */
    host_wait_ms(500);
    host_key(9, 1);
    host_wait_ms(300);
    app_track_step(0, 23, 71);
    host_key(9, 0);
    host_wait_ms(200);
    host_btn(B_REC, 0);
    host_frame();
    CHECK(!song.rec && !gn.arm && trk[0].step[23].note[0] == 71u, "an active punch when the app writes: ended; the held key's release writes nothing");
    host_tap(B_PLAY);
    host_wait_ms(60);
    host_btn(B_SAVE, 1);                            /* SAVE held, the app loads a project, SAVE let go */
    host_wait_ms(200);
    app_project(0, 0);
    host_btn(B_SAVE, 0);
    host_wait_ms(100);
    CHECK(!gn.saved_t && !gn.save_pend, "a SAVE press under way when the project is loaded: no save");
    host_btn(B_FX, 1);                              /* (all up again: buttons act) */
    host_frame();
    host_btn(B_FX, 0);
    host_tap(B_SCL);
    CHECK(song.sel == 1u, "..once every button is up, the panel acts again");
}

/* (7): the child's grid choice is per track: an undo or a write on another track leaves it */
static void t_r9_grid_per_track(void)
{
    uint32_t i;
    organ_empty(16, GN_DIV16);
    host_tap(B_SEQ);
    host_turn(EN_K1, 1);                            /* track 1: 8 bars, chosen */
    host_turn(EN_K1, 1);
    host_turn(EN_K1, 1);
    CHECK(trk[0].p[P_SDIV] == GN_DIV8, "(track 1: 8 bars)");
    host_tap(B_SCL);                                /* track 2: an edit, its undo, an app's write there */
    for (i = 0; i < NSTEP; i++)
        step_clear(&trk[1].step[i]);
    host_turn(EN_K1, 1);
    easy_rec_hold();
    app_track_step(1, 2, 64);
    host_tap(B_FX);
    easy_record();
    CHECK(song.rec == 1u && trk[0].p[P_SDIV] == GN_DIV8 && trk[0].p[P_SLEN] == 64,
          "track 1's empty pattern still 8 bars when it records (track 2's undo and write left it)");
    host_tap(B_REC);
    host_tap(B_PLAY);
}

/* the punch's own recording: a chord and a key held across 0.4 s tie as Felucca records them; another track
 * during it: nothing written there */
static void t_r9_punch_hold(void)
{
    uint32_t i, n = 0;
    organ_empty(32, GN_DIV16);
    host_tempo(-60);
    host_tap(B_PLAY);
    host_wait_ms(100);
    host_btn(B_REC, 1);
    host_frame();
    host_key(7, 1);                                 /* a chord, held across the threshold */
    host_key(11, 1);
    host_wait_ms(800);
    host_key(7, 0);
    host_key(11, 0);
    host_wait_ms(100);
    host_btn(B_REC, 0);
    host_frame();
    for (i = 0; i < NSTEP; i++)
        n += trk[0].step[i].time == ST_TIE;
    CHECK(n >= 1u, "a chord held across REC's 0.4 s: recorded with its ties (Felucca's own recording)");
    for (i = 0, n = 0; i < NSTEP; i++)
        n += trk[0].step[i].time == ST_NOTE && trk[0].step[i].n == 2u;
    CHECK(n == 1u, "..one step holds the chord");
    host_btn(B_REC, 1);                             /* a tap, track 2 picked during it */
    host_key(9, 1);
    host_wait_ms(100);
    host_tap(B_SCL);
    host_key(9, 0);
    host_btn(B_REC, 0);
    host_wait_ms(100);
    for (i = 0, n = 0; i < NSTEP; i++)
        n += trk[1].step[i].n && trk[1].step[i].note[0] == 62u;
    CHECK(n == 0u && !song.rec, "..another track during a hold: nothing of it written there, no recording");
    host_tap(B_PLAY);
}

/* ------------------------------------------------------------ round 10 --- */
/* motion undo: Easy's own sound pick advances the generation (32 bits now) */
static void t_r10_motion_gen(void)
{
    uint32_t i, n0 = 0;
    CHECK(sizeof gn.mgen == 4u && sizeof gn.undo_mgen == 4u, "the motion generation: 32 bits");
    organ_empty(64, GN_DIV32);
    put_note(0, 8, 60);
    app_motion_event(0, 8, P_DLY, 20);
    host_wait_ms(60);
    host_tap(B_SEQ);
    host_turn(EN_K3, 1);                            /* SNAP on: an undo copy with the motion */
    host_tap(B_HOME);
    i = gn.mgen;
    easy_pick_sound(GN_GROUP[3]);                   /* Easy's own pick on track 1 */
    host_wait_ms(GN_PICK_MS + 60);
    CHECK(gn.mgen != i, "an Easy sound pick advances the motion generation");
    {
        motion_store_t m0 = motion;
        easy_rec_hold();
        CHECK(motion_set_same(&motion, &m0), "..SAVE held after it: the motion store as the pick left it");
        CHECK(trk[0].p[P_SDIV] == GN_DIV32, "..the steps and grid undone");
    }
    (void)n0;
}

/* a key of a DRUM track that plays the lane of note */
static uint32_t drum_key(uint32_t t, uint32_t note)
{
    uint32_t k;
    for (k = 0; k < 27u; k++)
        if (kb_map(&trk[t], k) == note)
            return k;
    return 27u;
}

/* wait for the playhead in the late half of a step (60 BPM, 1/16: a step is 12000 samples) */
static void late_in_step(uint32_t t)
{
    uint32_t g;
    for (g = 0; g < 900u && !(trk[t].seq_pos > 6500u && trk[t].seq_pos < 8500u && trk[t].seq_pos < 0x7FFFFFFFu); g++)
        host_frame();
}

/* REPLACE: a DRUM track's hit recorded ahead into the step not yet crossed is kept (the drums as the notes) */
static void t_r10_drums_ahead(void)
{
    uint32_t i, k, j;
    host_easy();
    host_tap(B_LFO);
    app_engine(GE_DRUM);                            /* (round 24: the engine, not PRESET, which Easy adopts) */
    host_wait_ms(60);
    for (i = 0; i < NSTEP; i++)
        step_clear(&trk[3].step[i]);
    trk[3].p[P_SDIV] = GN_DIV16;
    trk[3].p[P_SLEN] = 16;
    host_tempo(-60);
    k = drum_key(3, 36);
    CHECK(drum_track(&trk[3]) && k < 27u, "(track 4: the DRUM engine, a key for its kick)");
    gn.replace = 1;
    host_tap(B_PLAY);
    host_btn(B_REC, 1);
    host_wait_ms(500);
    CHECK(gn.punch, "(punching in)");
    late_in_step(3);
    j = (trk[3].seq_idx + 1u) % 16u;               /* the kick lands in the next step */
    host_key(k, 1);
    host_frame();
    host_key(k, 0);
    host_btn(B_REC, 0);
    host_frame();
    CHECK(trk[3].seq_idx != j, "(let go before the playhead reached it)");
    CHECK(trk[3].step[j].time == ST_NOTE && (trk[3].step[j].hit & 1u),
          "REPLACE: the kick recorded ahead into a step the take never crossed is kept");
    host_tap(B_PLAY);
}

/* the onset log: 64 presses never reach the fallback; past it both are kept, nothing deleted */
static void presses(uint32_t n, uint32_t key)
{
    while (n--) {
        host_key(key, 1);
        host_frame();
        host_frame();
        host_key(key, 0);
        host_frame();
        host_frame();
    }
}

static void t_r10_log(void)
{
    uint32_t i, both = 0, d4 = 0, crossed = 0;
    organ_empty(32, GN_DIV16);
    host_tempo(-60);
    for (i = 0; i < 32u; i++)
        put_note(0, i, 62);                         /* D4 everywhere */
    gn.replace = 1;
    host_tap(B_PLAY);
    host_btn(B_REC, 1);
    host_wait_ms(450);
    presses(64, 7);                                 /* C4, 64 times */
    CHECK(gn.tlog_n == 64u && !gn.tlog_over, "64 presses: all in the onset log, no fallback");
    for (i = 0; i < 32u; i++)
        crossed += gn_bit(gn.crossed, i);
    host_btn(B_REC, 0);
    host_frame();
    for (i = 0; i < 32u; i++)
        if (gn_bit(gn.crossed, i))
            d4 += trk[0].step[i].time == ST_NOTE && trk[0].step[i].n && trk[0].step[i].note[0] == 62u;
    CHECK(crossed >= 10u && d4 == 0u, "..REPLACE: the old D4 out of every crossed step");
    host_tap(B_PLAY);
    host_wait_ms(60);
    for (i = 0; i < 32u; i++)
        put_note(0, i, 60);                         /* C4 everywhere: the take's pitch */
    host_tap(B_PLAY);
    host_btn(B_REC, 1);
    host_wait_ms(450);
    presses(70, 11);                                /* E4, past the log */
    CHECK(gn.tlog_over, "(70 presses: past the log)");
    host_btn(B_REC, 0);
    host_frame();
    for (i = 0; i < 32u; i++)
        if (gn_bit(gn.crossed, i)) {
            uint32_t a = 0, b = 0, q;
            for (q = 0; q < trk[0].step[i].n; q++) {
                a |= trk[0].step[i].note[q] == 60u;
                b |= trk[0].step[i].note[q] == 64u;
            }
            both += a && b;
        }
    CHECK(both >= 1u, "..past the log: a crossed step keeps both the old note and the take's");
    host_tap(B_PLAY);
}

/* STOP: every step up to the playhead processed, the step it has just entered too */
static void t_r10_stop_entered(void)
{
    uint32_t g, j;
    organ_empty(16, GN_DIV16);
    host_tempo(-60);
    for (j = 0; j < 16u; j++)
        put_note(0, j, 62);
    gn.replace = 1;
    host_tap(B_PLAY);
    host_btn(B_REC, 1);
    host_wait_ms(500);
    CHECK(gn.punch, "(punching in)");
    j = (trk[0].seq_idx + 2u) % 16u;
    for (g = 0; g < 4000u && trk[0].seq_idx != j; g++)
        host_audio_us(1000);                        /* the playhead into step j, no frame since */
    transport_req = 2;                              /* .. and STOP */
    host_frame();
    CHECK(!song.playing && !gn.take_on, "(stopped, the take ended)");
    CHECK(!(trk[0].step[j].time == ST_NOTE && trk[0].step[j].n && trk[0].step[j].note[0] == 62u),
          "STOP just after the playhead entered a step: that step replaced too");
    CHECK(trk[0].step[(j + 3u) % 16u].note[0] == 62u, "..steps ahead as they were");
    host_btn(B_REC, 0);
    host_frame();
}

/* HOME + SAVE: the music replaced during the chord cancels the switch */
static void t_r10_chord(void)
{
    host_easy();
    host_btn(B_HOME, 1);
    host_btn(B_SAVE, 1);
    host_wait_ms(800);
    app_project(0, 0);                              /* the app loads a project mid-hold */
    host_wait_ms(2500);
    CHECK(gn.on, "a project loaded during HOME + SAVE: the hold does not switch");
    host_btn(B_HOME, 0);
    host_btn(B_SAVE, 0);
    host_wait_ms(60);
    CHECK(gn.on, "..let go: still in Easy");
    host_hold_switch(2200);
    CHECK(!gn.on, "..a new hold switches");
}

/* a tap under 0.4 s: the last recording's undo intact, the pattern put back exactly (an audio write and a key
 * released between two UI calls of one frame) */
static void t_r10_tap_undo(void)
{
    step_t before[NSTEP], after[NSTEP];
    int16_t l0, d0, l1, d1;
    organ_empty(16, GN_DIV16);
    host_tempo(-60);
    put_note(0, 12, 67);
    host_wait_ms(60);
    memcpy(before, trk[0].step, sizeof before);
    l0 = trk[0].p[P_SLEN];
    d0 = trk[0].p[P_SDIV];
    host_tap(B_PLAY);
    host_btn(B_REC, 1);                             /* a recording: a punch with a note */
    host_wait_ms(500);
    host_key(9, 1);
    host_wait_ms(300);
    host_key(9, 0);
    host_wait_ms(60);
    host_btn(B_REC, 0);
    host_frame();
    CHECK(gn.undo_of == 1u, "(an undo copy of the recording)");
    memcpy(after, trk[0].step, sizeof after);
    l1 = trk[0].p[P_SLEN];
    d1 = trk[0].p[P_SDIV];
    host_wait_ms(100);
    host_btn(B_REC, 1);                             /* a tap */
    host_frame();
    host_key(11, 1);
    host_audio_us(15000);
    ui_input();
    host_key(11, 0);                                /* released, the audio on, between two UI calls */
    host_audio_us(8000);
    ui_input();
    host_btn(B_REC, 0);
    host_audio_us(2000);
    ui_input();
    host_frame();
    host_frame();
    CHECK(!memcmp(trk[0].step, after, sizeof after) && trk[0].p[P_SLEN] == l1 && trk[0].p[P_SDIV] == d1,
          "a tap under 0.4 s: the pattern exactly as before the tap");
    CHECK(gn.undo_of == 1u && gn.arm == GA_READY, "..the last recording's undo intact, REC armed");
    host_tap(B_REC);
    easy_rec_hold();
    CHECK(!memcmp(trk[0].step, before, sizeof before) && trk[0].p[P_SLEN] == l0 && trk[0].p[P_SDIV] == d0,
          "..SAVE held: the recording before the tap undone");
    host_tap(B_PLAY);
}

/* a key already held when the punch begins: its note recorded from there */
static void t_r10_held_key(void)
{
    uint32_t i, n = 0;
    organ_empty(16, GN_DIV16);
    host_tempo(-60);
    gn.replace = 1;
    host_tap(B_PLAY);
    host_wait_ms(100);
    host_key(7, 1);                                 /* C4 down first */
    host_wait_ms(100);
    host_btn(B_REC, 1);                             /* then REC held */
    host_wait_ms(600);
    host_key(7, 0);
    host_wait_ms(60);
    host_btn(B_REC, 0);
    host_frame();
    for (i = 0; i < 16u; i++)
        n += trk[0].step[i].time == ST_NOTE && trk[0].step[i].n && trk[0].step[i].note[0] == 60u;
    CHECK(n == 1u, "a key held when the punch begins: its note recorded (REPLACE kept it)");
    for (i = 0, n = 0; i < 16u; i++)
        n += trk[0].step[i].time == ST_TIE;
    CHECK(n >= 1u, "..held on: its ties");
    host_tap(B_PLAY);
}

/* round 11: a tap is never a punch before 400 ms. 240 BPM, SNAP off (1/32), a 350 ms tap with a key and a
 * knob turned: never a punch, the pattern and the last recording's undo intact, the motion store untouched
 * (Easy's knobs never record motion) */
static void t_r11_fast_tap(void)
{
    step_t before[NSTEP];
    motion_store_t m0;
    uint32_t i, g, punched = 0;
    organ_empty(64, GN_DIV32);
    host_tempo(200);                      /* 240 BPM */
    CHECK(song.g[G_BPM] == 240, "(240 BPM)");
    for (i = 0; i < 64u; i += 2u)
        put_note(0, i, 62);
    gn.replace = 1;
    host_tap(B_PLAY);
    host_btn(B_REC, 1);                             /* a recording first: its undo copy */
    host_wait_ms(500);
    host_btn(B_REC, 0);
    host_frame();
    CHECK(gn.undo_of == 1u, "(the last recording's undo copy)");
    host_wait_ms(60);
    memcpy(before, trk[0].step, sizeof before);
    m0 = motion;
    host_btn(B_REC, 1);                             /* a 350 ms tap */
    host_frame();
    host_key(9, 1);
    host_turn(EN_K1, 3);
    for (g = 0; g < 21u; g++) {
        punched |= gn.punch;
        host_frame();
    }
    host_key(9, 0);
    host_frame();
    punched |= gn.punch;
    host_btn(B_REC, 0);
    host_frame();
    CHECK(!punched, "240 BPM, 1/32: a 350 ms REC tap is never a punch");
    CHECK(!memcmp(trk[0].step, before, sizeof before), "..the pattern exactly as before (REPLACE erased nothing)");
    CHECK(gn.undo_of == 1u && gn.arm == GA_READY, "..the last recording's undo intact, REC armed");
    CHECK(!memcmp(&motion, &m0, sizeof m0), "..a knob turned during it: no motion recorded");
    host_tap(B_REC);
    host_tap(B_PLAY);
    host_tempo(-120);
}

/* a 48-step pattern: no punch at once (the window wraps at LEN), a tap put back */
static void t_r11_len48(void)
{
    step_t before[NSTEP];
    uint32_t i, g, punched = 0;
    organ_empty(48, GN_DIV16);
    for (i = 0; i < 48u; i += 3u)
        put_note(0, i, 64);
    gn.replace = 1;
    host_tap(B_PLAY);
    for (g = 0; g < 400u && trk[0].seq_idx != 44u; g++)
        host_frame();                               /* the playhead near the loop's end */
    memcpy(before, trk[0].step, sizeof before);
    host_btn(B_REC, 1);
    for (g = 0; g < 20u; g++) {
        host_frame();
        punched |= gn.punch;
    }
    CHECK(!punched && gn.prov, "48 steps: REC held 300 ms is still a tap, not a punch");
    host_btn(B_REC, 0);
    host_frame();
    CHECK(!memcmp(trk[0].step, before, sizeof before) && gn.arm == GA_READY, "..let go: the pattern as it was, armed");
    host_tap(B_REC);
    host_tap(B_PLAY);
}

/* a stalled panel: the tap's window would overflow before the panel sees 400 ms: put back and armed, never
 * a punch */
static void t_r11_stall(void)
{
    step_t before[NSTEP];
    uint32_t i;
    organ_empty(64, GN_DIV32);
    host_tempo(200);
    for (i = 0; i < 64u; i += 2u)
        put_note(0, i, 62);
    gn.replace = 1;
    host_tap(B_PLAY);
    host_wait_ms(60);
    memcpy(before, trk[0].step, sizeof before);
    host_btn(B_REC, 1);
    host_frame();
    host_frame();
    CHECK(gn.prov, "(a tap under way)");
    host_audio_us(700000);                          /* the panel stalls 0.7 s; the loop plays on */
    ui_input();
    host_frame();
    CHECK(!gn.punch && !gn.prov && !song.rec && gn.arm == GA_READY, "a stalled panel: the tap put back and armed, no punch");
    CHECK(!memcmp(trk[0].step, before, sizeof before), "..the pattern as it was");
    host_wait_ms(200);
    host_btn(B_REC, 0);
    host_frame();
    CHECK(gn.arm == GA_READY && !gn.punch, "..REC let go: still armed, no tap");
    host_tap(B_REC);
    host_tap(B_PLAY);
    host_tempo(-120);
}

/* round 12: a take that begins at the end of a count-in begins with a fresh tick (the transport starts from
 * 0: no restart seen, the take never restarts at once) and its undo copy is the pattern as the count-in
 * found it, even with a key played in the loop's first moment (the simulator's tour: the take's first note
 * landed before the first frame, and the false restart took the undo copy again, the note in it) */
static void t_r12_count_in(void)
{
    uint32_t g, i, nu = 0, restarts = 0;
    uint32_t tt;
    organ_empty(16, GN_DIV16);
    host_tap(B_PLAY);                               /* a play first: the tick runs on */
    host_wait_ms(3000);
    host_tap(B_PLAY);
    host_wait_ms(200);
    CHECK(song.tick > 100u, "(the tick from the last play)");
    host_tap(B_REC);
    host_tap(B_PLAY);                               /* the count-in */
    for (g = 0; g < 400u && !gn.take_on; g++)
        host_frame();
    CHECK(gn.take_on && !song.playing, "(the count-in over: the take begun, the loop about to start)");
    host_key(7, 1);                                 /* C4 at the loop's first moment */
    tt = gn.take_tick;
    for (g = 0; g < 20u; g++) {
        host_frame();
        restarts += gn.take_tick < tt;
        tt = gn.take_tick;
        if (g == 0)
            for (i = 0; i < NSTEP; i++)
                nu += gn.undo[i].time == ST_NOTE && gn.undo[i].n;
    }
    host_key(7, 0);
    CHECK(nu == 0u && !restarts && song.rec == 1u, "a take begun by the count-in: no restart at the start, its undo copy without the take's note");
    host_wait_ms(300);
    host_tap(B_REC);
    for (i = 0, g = 0; i < NSTEP; i++)
        g += trk[0].step[i].n != 0;
    CHECK(g >= 1u, "(the take recorded its note)");
    easy_rec_hold();
    for (i = 0, g = 0; i < NSTEP; i++)
        g += trk[0].step[i].n != 0;
    CHECK(g == 0u, "..SAVE held: the pattern as the count-in found it (empty)");
    host_tap(B_PLAY);
}

/* ------------------------------------------------------------ round 26 --- */
/* The recording's metronome (FORK.md, round 26): a click on each beat, the bar's 1 accented, during the count-in
 * and while recording, on the beat to a block; added after the USB audio tap (the USB audio, the patterns, the
 * voices and MIDI the same as with it off); ON unless switched off with EDIT held + K3, the switch kept in the
 * settings record across a power cycle. The click is what the codec's output has more than the USB audio's */
#define R26_BEAT 22050u                              /* 120 BPM */
#define R26_MAX (14u * 44100u)
typedef struct {
    uint32_t at;                                     /* the sample it begins at */
    int32_t peak;                                    /* its loudest sample */
} r26_onset_t;

/* the clicks in a capture: the codec's minus the USB audio's, each begun after 4,000 silent samples */
static uint32_t r26_onsets(const int32_t *cap, const int32_t *usb, uint32_t n, r26_onset_t *o, uint32_t max)
{
    uint32_t i, j, quiet = 4000u, k = 0;
    for (i = 0; i < n; i++) {
        int32_t d = cap[i] - usb[i];
        if (!d) {
            quiet++;
            continue;
        }
        if (quiet >= 4000u && k < max) {
            o[k].at = i;
            o[k].peak = 0;
            for (j = i; j < n && j < i + GN_CLICK_LEN; j++) {
                int32_t a = cap[j] - usb[j];
                a = a < 0 ? -a : a;
                o[k].peak = a > o[k].peak ? a : o[k].peak;
            }
            k++;
        }
        quiet = 0;
    }
    return k;
}
static int r26_accent(const r26_onset_t *o) { return o->peak > 5800; }
static int r26_near(uint32_t a, uint32_t b, uint32_t want, uint32_t slack)   /* b - a is want, to slack */
{
    return b > a && b - a + slack >= want && b - a <= want + slack;
}

static void r26_easy(void)                           /* the first power-on: Easy, the Gnome on track 1, 120 BPM */
{
    host_start_easy = 1;
    host_power_on();
    host_frame();
    host_wait_ms(3100);
    song.g[G_BPM] = 120;
}

/* what one run leaves for the other to compare (shared with the parent: mmap) */
typedef struct {
    int32_t cap[R26_MAX], usb[R26_MAX];
    uint32_t n;
    step_t step[NTRK][NSTEP];
    uint32_t voices, midi;                           /* every frame's voices sounding, summed; MIDI and SysEx out */
    uint32_t count_at;                               /* the sample the count-in began at (its PLAY's next frame) */
    uint32_t rec_at, rec_end;                        /* .. the frame the take began, ended */
} r26_run_t;

/* REC, PLAY (the count-in), two bars recorded with keys played, REC (off), the loop on a bar, stopped */
static void r26_scenario(r26_run_t *r, int click)
{
    uint32_t f;
    r26_easy();
    gn_click_set(click);
    host_cap = r->cap;
    host_usb_cap = r->usb;
    host_cap_n = host_usb_n = 0;
    host_cap_max = R26_MAX;
    host_wait_ms(300);
    host_tap(B_REC);
    host_btn(B_PLAY, 1);
    host_frame();                                    /* (PLAY: the count-in, from the next frame's audio) */
    r->count_at = host_cap_n;
    host_btn(B_PLAY, 0);
    for (f = 0; f < 400u && !song.rec; f++)
        host_frame();
    r->rec_at = host_cap_n;
    for (f = 0; f < 255u; f++) {                     /* (two bars, but for a few frames: REC is let go before
                                                      * the third bar's 1) */
        host_key(f % 3u ? 9 : 7, (f / 10u) & 1u);
        host_frame();
        r->voices += voices_on(0) + voices_on(1) + voices_on(2) + voices_on(3);
    }
    host_key(7, 0);
    host_key(9, 0);
    host_tap(B_REC);
    r->rec_end = host_cap_n;
    host_wait_ms(2000);                              /* (the loop plays on: a bar, not recording) */
    host_tap(B_PLAY);
    host_wait_ms(300);
    r->n = host_cap_n;
    for (f = 0; f < NTRK; f++)
        memcpy(r->step[f], trk[f].step, sizeof r->step[f]);
    r->midi = so_w;
    host_cap = host_usb_cap = 0;
}

static r26_run_t *r26_fork(int click)                 /* the scenario in a process of its own (the same start) */
{
    r26_run_t *r = mmap(0, sizeof *r, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    pid_t pid;
    int st = 0;
    if (r == MAP_FAILED)
        return 0;
    memset(r, 0, sizeof *r);
    fflush(stdout);
    pid = fork();
    if (pid == 0) {
        r26_scenario(r, click);
        fflush(stdout);
        _exit(fails ? 1 : 0);
    }
    if (pid < 0 || waitpid(pid, &st, 0) != pid || !WIFEXITED(st) || WEXITSTATUS(st))
        fails++;
    return r;
}

static void t_r26_click_timing(void)
{
    static r26_onset_t o[64];
    r26_run_t *a = r26_fork(1), *b = r26_fork(0);
    uint32_t n, i, cnt = 0, loop = 0, first = 0xFFFFFFFFu, bad = 0, after = 0;
    if (!a || !b) {
        CHECK(0, "(the runs' shared memory)");
        return;
    }
    CHECK(a->n > 8u * R26_BEAT && a->n == b->n, "(both runs captured the whole scenario, the same length)");
    n = r26_onsets(a->cap, a->usb, a->n, o, 64);
    for (i = 0; i < n; i++) {
        if (o[i].at < a->rec_at - 2000u)
            cnt++;
        else if (o[i].at < a->rec_end + 2000u) {
            loop++;
            first = first == 0xFFFFFFFFu ? i : first;
        } else
            after++;
    }
    CHECK(cnt == 4u, "the count-in: four clicks");
    CHECK(n >= 4u && o[0].at >= a->count_at && o[0].at < a->count_at + CTL,
          "..the first at the count-in's start (its first block)");
    for (i = 0; i + 1u < cnt; i++)
        bad += !r26_near(o[i].at, o[i + 1u].at, R26_BEAT, CTL);
    CHECK(cnt == 4u && !bad, "..a beat apart (120 BPM: 22,050 samples, to a block)");
    CHECK(cnt == 4u && r26_accent(&o[0]) && !r26_accent(&o[1]) && !r26_accent(&o[2]) && !r26_accent(&o[3]),
          "..the first accented (the higher, louder click), the other three not");
    CHECK(first == 4u && r26_near(o[3].at, o[4].at, R26_BEAT, 15u * 44100u / 1000u + 2u * CTL),
          "..no fifth: the next click is the loop's 1, a beat after the fourth (a frame late at most: the count-in's end)");
    CHECK(loop == 8u, "recording two bars: eight clicks");
    for (i = first, bad = 0; first < n && i + 1u < first + loop; i++)
        bad += !r26_near(o[i].at, o[i + 1u].at, R26_BEAT, CTL);
    CHECK(loop == 8u && !bad, "..a beat apart, on the sequencer's beat");
    for (i = first, bad = 0; first < n && i < first + loop; i++)
        bad += r26_accent(&o[i]) != ((i - first) % 4u == 0u);
    CHECK(loop == 8u && !bad, "..each bar's 1 accented (bar 1 and bar 2), the others not");
    CHECK(loop == 8u && o[first].at >= a->rec_at && o[first].at < a->rec_at + 15u * 44100u / 1000u + 2u * CTL,
          "..the first click at the loop's start (the frame the take began)");
    CHECK(after == 0u, "REC off, the loop playing on, stopped: no click");
    /* never recorded, never on USB or MIDI, no voice taken: the run with the click off is the same in every other way */
    CHECK(!memcmp(b->cap, b->usb, b->n * sizeof b->cap[0]), "the metronome OFF: no click at all (the codec's = the USB's)");
    CHECK(!memcmp(a->usb, b->usb, a->n * sizeof a->usb[0]), "ON: the USB audio the same samples as OFF (the click never in it)");
    CHECK(!memcmp(a->step, b->step, sizeof a->step), "..the recording the same steps (the click never recorded)");
    for (i = 0, bad = 0; i < NSTEP; i++)
        bad += a->step[0][i].time == ST_NOTE;
    CHECK(bad > 0u, "(the take recorded the keys)");
    CHECK(a->voices == b->voices && a->voices > 0u, "..the same voices sounding every frame (no voice of the tracks taken)");
    CHECK(a->midi == b->midi, "..the same MIDI and SysEx out (none of it the click's)");
}

/* armed while the loop plays: the clicks from the arming, on the loop's own bars (the take begins at an accented 1) */
static void t_r26_click_armed(void)
{
    static int32_t cap[6u * 44100u], usb[6u * 44100u];
    static r26_onset_t o[32];
    uint32_t n, i, f, armed, began = 0, bad = 0, acc = 0xFFFFFFFFu;
    r26_easy();
    host_tap(B_PLAY);
    host_wait_ms(1300);                              /* (the loop plays, not armed: no click) */
    host_cap = cap;
    host_usb_cap = usb;
    host_cap_n = host_usb_n = 0;
    host_cap_max = NELEM(cap);
    host_wait_ms(400);
    armed = host_cap_n;
    host_tap(B_REC);
    for (f = 0; f < 320u; f++) {
        host_frame();
        if (!began && song.rec)
            began = host_cap_n;
    }
    host_cap = host_usb_cap = 0;
    n = r26_onsets(cap, usb, host_cap_n, o, 32);
    CHECK(n >= 6u && o[0].at > armed, "armed while playing: clicks, none before the arming");
    for (i = 0; i + 1u < n; i++)
        bad += !r26_near(o[i].at, o[i + 1u].at, R26_BEAT, CTL);
    CHECK(!bad, "..a beat apart");
    for (i = 0; i < n; i++)
        if (r26_accent(&o[i])) {
            if (acc != 0xFFFFFFFFu)
                bad += (i - acc) % 4u != 0u;
            acc = i;
        }
    CHECK(acc != 0xFFFFFFFFu && !bad, "..an accent every fourth click (the bar's 1)");
    for (i = 0, f = 0; i < n; i++)                  /* (Easy begins the take in the last step's second half, so the
                                                      * first note lands on step 1: up to half a 1/16 early) */
        f |= r26_accent(&o[i]) && began + R26_BEAT / 8u + 15u * 44100u / 1000u >= o[i].at &&
             began < o[i].at + 15u * 44100u / 1000u + 2u * CTL;
    CHECK(began && f, "..the take begins at bar 1: an accented click (to half a step and a frame)");
    host_tap(B_REC);
    host_tap(B_PLAY);
}

/* the switch: EDIT held + K3 (any page; the record screen's SNAP is K3 without EDIT), its card, the record kept */
static uint8_t r26_nor[sizeof host_nor];
static void r26_power_cycle(void)                    /* the flash kept, the RAM as at power-on (persist_boot) */
{
    persist_t p;
    int n;
    memcpy(r26_nor, host_nor, sizeof r26_nor);
    host_power_on();
    memcpy(host_nor, r26_nor, sizeof r26_nor);
    memset(&persist_saved, 0, sizeof persist_saved);
    persist_pending = 0;
    n = st_load(OBJ_SETTINGS, &p, sizeof p);
    if (settings_import(&p, n))
        persist_saved = p;
    settings_init();
    panel_init();
    palette_set(settings.palette);
    host_frame();
    host_wait_ms(3100);
}
static int r26_flash_click(void)                     /* the record on flash says ON */
{
    persist_t p;
    return st_load(OBJ_SETTINGS, &p, sizeof p) == (int)sizeof p && (p.favorites.factory[15][30] & 0xF0u) == GN_MODE_MARK &&
           !(p.favorites.factory[15][30] & GN_MODE_NOCLICK);
}
static void t_r26_click_switch(void)
{
    uint8_t snap;
    r26_easy();
    CHECK(gn_click_on() && (favorites.factory[15][30] & 0xF7u) == (GN_MODE_MARK | GN_MODE_EASY),
          "the first power-on: the metronome ON (bit 2 of the mode byte 0, as every record before round 26)");
    CHECK(gn_knob_role(2) != KR_CLICK_ON, "EDIT not held: K3 is not the metronome");
    host_tap(B_SEQ);
    host_tap(B_SEQ);
    host_tap(B_SEQ);
    host_frame();
    CHECK(gn.page == GP_RECORD && gn_knob_role(2) == KR_LOOP, "(the record screen: K3 is SNAP)");
    snap = (uint8_t)gn_snap(0);
    host_btn(B_EDIT, 1);
    host_frame();
    CHECK(gn_knob_role(2) == KR_CLICK_ON && gs.role[2] == KR_CLICK_ON, "EDIT held: K3 is the metronome, its card CLICK ON");
    host_turn(EN_K3, -1);
    CHECK(!gn_click_on() && gn_knob_role(2) == KR_CLICK_OFF && gs.role[2] == KR_CLICK_OFF && gn_snap(0) == snap,
          "..K3 left: OFF (the card CLICK OFF), SNAP untouched");
    host_turn(EN_K3, -1);
    CHECK(!gn_click_on(), "..left again: still OFF");
    host_btn(B_EDIT, 0);
    host_wait_ms(300);
    CHECK(gn_knob_role(2) == KR_LOOP, "EDIT let go: K3 is SNAP again");
    CHECK(!r26_flash_click(), "..OFF written to flash by the normal settings save");
    host_hold_switch(2100);
    host_wait_ms(300);
    CHECK(!gn.on && !gn_click_on(), "into Advanced: the setting kept (the mode byte's other bits as they were)");
    r26_power_cycle();
    CHECK(!gn.on && !gn_click_on(), "power cycle: Advanced, the metronome still OFF");
    host_hold_switch(2100);
    host_wait_ms(300);
    CHECK(gn.on && !gn_click_on(), "..back into Easy: OFF");
    host_btn(B_EDIT, 1);
    host_frame();
    host_turn(EN_K3, 1);
    host_btn(B_EDIT, 0);
    host_wait_ms(300);
    CHECK(gn_click_on() && r26_flash_click(), "EDIT held + K3 right: ON, written");
    r26_power_cycle();
    CHECK(gn.on && gn_click_on(), "power cycle: Easy, the metronome ON");
    /* a record of other firmware (no Gnomad marker): never a click */
    favorites.factory[15][30] = 0;
    CHECK(!gn_click_on(), "a record without Gnomad's marker: no click (upstream's tests, another firmware's record)");
}

/* Advanced: Felucca's own recording clicks too (the same setting), off when it is off */
static void t_r26_click_advanced(void)
{
    static int32_t cap[3u * 44100u], usb[3u * 44100u];
    static r26_onset_t o[16];
    uint32_t n, pass;
    r26_easy();
    host_hold_switch(2100);
    host_wait_ms(300);
    CHECK(!gn.on, "(Advanced)");
    for (pass = 0; pass < 2u; pass++) {
        gn_click_set(!pass);
        host_cap = cap;
        host_usb_cap = usb;
        host_cap_n = host_usb_n = 0;
        host_cap_max = NELEM(cap);
        song.rec = 1;                                /* (Felucca's REC on track 1) */
        transport_req = 1;
        host_wait_ms(2100);
        transport_req = 2;
        host_frame();
        song.rec = 0;
        host_cap = host_usb_cap = 0;
        n = r26_onsets(cap, usb, host_cap_n, o, 16);
        if (!pass)
            CHECK(n == 5u && r26_accent(&o[0]) && !r26_accent(&o[1]) && r26_accent(&o[4]),
                  "Advanced, recording: a click a beat, the bar's 1 accented (the setting is the same)");
        else
            CHECK(n == 0u, "..the metronome OFF: none");
        host_wait_ms(300);
    }
}

/* ------------------------------------------------------------ round 13 --- */
/* (2) chromatic: another sound with WHITE keys (its scale setting) plays every key in Easy, untouched */
static void t_r13_chromatic(void)
{
    uint32_t k, ok = 1;
    host_easy();
    app_set(P_QUANT, QN_WHITE);
    host_wait_ms(60);
    for (k = 0; k < 27u; k++) {
        host_key(k, 1);
        host_wait_ms(30);
        ok &= kb_note[k] == gn_chromatic(&trk[0], k) && kb_note[k] != KB_SILENT;
        host_key(k, 0);
        host_wait_ms(60);
    }
    CHECK(ok && trk[0].p[P_QUANT] == QN_WHITE, "another sound with WHITE keys: every key plays, chromatic; its setting kept");
}

/* (3) any track: the four knobs move the sound's Bright .. Space; another sound by its engine's map */
static void t_r13_fallback(void)
{
    music_t a, b;
    uint32_t e = 0, k;
    host_easy();
    host_tap(B_SCL);                                /* track 2: Felucca's own sound */
    host_wait_ms(GN_PICK_MS);
    CHECK(song.sel == 1u && gn_easy_of(&trk[1]) == GN_NOSND && gn_map(1, 0), "track 2: another sound, its engine's map");
    e = trk[1].eng_req;
    for (k = 0; k < 4u; k++) {
        const gn_knob_t *m = gn_fb_map(e, k);
        int16_t v0 = trk[1].p[m->id];
        music_get(&a);
        host_turn(EN_K1 + k, v0 > (m->lo + m->hi) / 2 ? -6 : 6);
        music_get(&b);
        CHECK(trk[1].p[m->id] != v0 && music_same_fb(&a, &b, 1), "..each knob moves its setting, nothing else");
    }
    app_set(P_DLY, 127);
    host_wait_ms(30);
    CHECK(gn.amount[1][2] == GN_STEPS, "..the app moves Echo's setting: the bubble follows");
    host_frame();
    CHECK(!gs.idle, "..the bubbles live");
}

/* (4) a reported bug: a kit's pads recorded (kicks, two bars), then a take of held hats (each over steps):
 * Felucca's recording tied each held hat over the steps after it, the kicks there gone, and the undo copy
 * dropped. A pad is a hit, never held: the kicks stay; SAVE held undoes only the hats */
static void t_r13_kit_holds(void)
{
    uint32_t i, kicks = 0, hats = 0, ties = 0;
    host_easy();
    easy_pick_sound(GN_SND_KIT);
    host_wait_ms(GN_PICK_MS);
    for (i = 0; i < NSTEP; i++)
        step_clear(&trk[0].step[i]);
    trk[0].p[P_SDIV] = GN_DIV16;                    /* (two bars, the child's BARS: round FLOW-1's blank loop is one) */
    trk[0].p[P_SLEN] = 32;
    gn.grid_set |= 1u;
    host_tempo(-60);                      /* 60 BPM: a 1/16 step is 250 ms */
    host_tap(B_REC);
    host_tap(B_PLAY);
    for (i = 0; i < 400u && !song.rec; i++)
        host_frame();
    for (i = 0; i < 8u; i++) {                      /* kicks (pad 1, F3), a beat apart */
        key(0, 120);
        host_wait_ms(880);
    }
    host_tap(B_REC);
    CHECK(trk[0].p[P_SLEN] == 32, "(two bars at 1/16)");
    for (i = 0; i < NSTEP; i++)
        kicks += trk[0].step[i].time == ST_NOTE && trk[0].step[i].n && trk[0].step[i].note[0] == 36u;
    CHECK(kicks >= 7u, "(the kicks recorded)");
    host_btn(B_REC, 1);                             /* a punch of held hats (pad 7, E4), each 0.6 s */
    host_wait_ms(500);
    for (i = 0; i < 8u; i++) {
        host_key(11, 1);
        host_wait_ms(600);
        host_key(11, 0);
        host_wait_ms(400);
    }
    host_btn(B_REC, 0);
    host_frame();
    for (i = 0, kicks = 0; i < NSTEP; i++) {
        uint32_t q;
        ties += trk[0].step[i].time == ST_TIE;
        for (q = 0; trk[0].step[i].time == ST_NOTE && q < trk[0].step[i].n; q++) {
            kicks += trk[0].step[i].note[q] == 36u;
            hats += trk[0].step[i].note[q] == 42u;
        }
    }
    CHECK(kicks >= 7u && hats >= 6u && !ties, "a take of held hats over the kicks: every kick kept, the hats added, no ties");
    CHECK(gn.undo_of == 1u, "..its undo copy kept");
    easy_rec_hold();
    for (i = 0, hats = 0, kicks = 0; i < NSTEP; i++) {
        uint32_t q;
        for (q = 0; trk[0].step[i].time == ST_NOTE && q < trk[0].step[i].n; q++) {
            kicks += trk[0].step[i].note[q] == 36u;
            hats += trk[0].step[i].note[q] == 42u;
        }
    }
    CHECK(!hats && kicks >= 7u, "..SAVE held: the hats undone, the kicks as they were");
    host_tap(B_PLAY);
    host_tempo(60);
}

/* round 14 (1, 2): the entry's Gnome costs nothing. Shown at once, put on track 1 only when used (a key, a
 * knob, PRESETS, ARP, REC); never used, track 1 (its sound and motion) and Felucca's undo copy are byte for
 * byte as before, a pick on another track meanwhile included; used, he stays and SAVE held in Advanced gives
 * back track 1 as it was before Easy */
static void r14_setup(music_t *a)
{
    host_power_on();
    host_frame();
    app_preset(GE_PHASE, 3);                        /* Advanced: track 1's sound, its motion, Felucca's undo */
    app_motion_event(0, 4, P_DLY, 40);
    host_wait_ms(60);
    host_turn(EN_PRESET, 1);                        /* (a PRESETS turn: Felucca's undo copy) */
    host_wait_ms(1600);
    track_select(2);
    host_frame();
    music_get(a);
}

static void t_r14_entry(void)
{
    static uint8_t undo0[sizeof undo];
    music_t a, b;
    uint32_t n;
    host_start_easy = 1;                            /* the first power-on */
    host_power_on();
    host_frame();
    host_wait_ms(3100);
    n = trk[0].eng_req;
    CHECK(gn.on && song.sel == 0u && gn_picture() == 0u && gn_view() == GV_HOME && gn_easy_of(&trk[0]) == GN_NOSND,
          "the first power-on: Home (round UX-1), the Gnome shown on track 1 (not yet put there)");
    host_key(7, 1);
    host_frame();
    host_wait_ms(100);                              /* (the engine's switch, a fade: the note queued after it) */
    CHECK(easy_whole_on(GN_GROUP[0], 0) && voices_on(0) > 0u && n != GE_VOICE, "..a key: the Gnome put there, the key sounds");
    host_key(7, 0);
    host_start_easy = 0;
    /* never used: nothing at all */
    r14_setup(&a);
    memcpy(undo0, &undo, sizeof undo);
    host_hold_switch(2100);
    host_wait_ms(3100);
    CHECK(gn.on && song.sel == 0u && gn_picture() == 0u && gn_view() == GV_HOME, "into Easy: the Gnome shown, track 1 selected");
    host_hold_switch(2100);
    music_get(&b);
    CHECK(!gn.on && music_same(&a, &b, 0) && song.sel == 2u && !memcmp(undo0, &undo, sizeof undo),
          "..left unused: track 1 and Felucca's undo copy byte for byte as before, track 3 selected");
    /* a pick on track 2 meanwhile */
    r14_setup(&a);
    host_hold_switch(2100);
    host_wait_ms(3100);
    host_tap(B_SCL);
    host_turn(EN_ALGO, 2);
    host_wait_ms(60);
    CHECK(gn_easy_of(&trk[1]) < GN_NSOUNDS, "(a pick on track 2)");
    host_hold_switch(2100);
    music_get(&b);
    CHECK(!memcmp(a.p[0], b.p[0], sizeof a.p[0]) && !memcmp(a.step[0], b.step[0], sizeof a.step[0]) && a.eng[0] == b.eng[0] &&
              a.pre[0] == b.pre[0] && !memcmp(a.fm6[0], b.fm6[0], FP_SIZE) && motion_same(&a.motion, &b.motion, 0),
          "..a pick on track 2, then left: track 1 (sound and motion) byte for byte as before");
    CHECK(undo.trk == 2u && undo.eng == a.eng[1] && undo.preset == a.pre[1], "..Felucca's undo copy the pick's own (track 2 before it)");
    /* used: he stays; Felucca's undo gives back track 1 as before Easy */
    r14_setup(&a);
    host_hold_switch(2100);
    host_wait_ms(3100);
    key(7, 100);                                    /* played */
    key(11, 100);
    CHECK(easy_whole_on(GN_GROUP[0], 0) || gn_easy_of(&trk[0]) == GN_GROUP[0], "a key: the Gnome on track 1");
    host_hold_switch(2100);
    CHECK(!gn.on && trk[0].eng_req == GE_VOICE, "..played, then left: he stays");
    host_btn(B_SAVE, 1);                            /* Felucca's undo */
    host_wait_ms(800);
    host_btn(B_SAVE, 0);
    host_wait_ms(60);
    music_get(&b);
    CHECK(music_same(&a, &b, 0), "..SAVE held in Advanced: track 1's sound and motion as before Easy, byte for byte");
}

/* round 14 (3): the fallback Wobble's LFO: its rate brought to 5.4 Hz while off its place, rate and depth back
 * exactly when the knob comes back and when Easy is left, each unless someone else wrote it since */
static void t_r14_wobble(void)
{
    int16_t r0, d0;
    uint32_t id;
    host_easy();
    host_tap(B_SCL);
    host_wait_ms(GN_PICK_MS);
    id = gn_fb_map(trk[1].eng_req, 1u)->id;
    app_track_param(1, P_LRATE, 20);                /* (the sound's own slow LFO) */
    host_wait_ms(1600);
    r0 = trk[1].p[P_LRATE];
    d0 = trk[1].p[id];
    host_turn(EN_K2, 3);
    CHECK(trk[1].p[P_LRATE] == 89 && trk[1].p[id] != d0, "another sound's Wobble moved: the LFO at 5.4 Hz, the depth moved");
    host_turn(EN_K2, -3);
    CHECK(trk[1].p[P_LRATE] == r0 && trk[1].p[id] == d0 && !(gn.wob_mask & 2u), "..back to its place: rate and depth exactly as they were");
    host_turn(EN_K2, 2);
    host_hold_switch(2100);
    CHECK(!gn.on && trk[1].p[P_LRATE] == r0 && trk[1].p[id] == d0, "..moved, then Easy left: rate and depth put back");
    host_hold_switch(2100);
    host_wait_ms(3100);
    host_tap(B_SCL);
    host_turn(EN_K2, 2);
    app_track_param(1, P_LRATE, 50);                /* the app writes the rate meanwhile */
    host_wait_ms(30);
    host_hold_switch(2100);
    CHECK(trk[1].p[P_LRATE] == 50 && trk[1].p[id] == d0, "..the app's rate written meanwhile stays the app's; the depth put back");
}

/* round 14 (4): the kit's hold stopped where the recording makes it: the panel stalled for more than a step
 * (audio only, no UI frame), a held pad still never ties over the steps after; the crash's tail and repeated
 * hits still sound */
static void t_r14_kit_stall(void)
{
    uint32_t i, ties = 0, hits = 0;
    host_easy();
    easy_pick_sound(GN_SND_KIT);
    host_wait_ms(GN_PICK_MS);
    for (i = 0; i < NSTEP; i++) {
        step_clear(&trk[0].step[i]);
        put_note(0, i, 36);                         /* a kick on every step */
    }
    host_tempo(-60);                      /* 60 BPM: a step 250 ms */
    host_tap(B_PLAY);
    host_btn(B_REC, 1);                             /* a punch */
    host_wait_ms(500);
    CHECK(gn.punch, "(punching in)");
    host_key(26, 1);                                /* the crash (pad 16), held */
    host_audio_us(15000);
    ui_input();
    host_audio_us(1200000);                         /* the panel stalled 1.2 s: more than four steps */
    host_key(26, 0);
    host_frame();
    host_wait_ms(200);
    CHECK(voices_on(0) > 0u, "..the crash's tail still sounds after the key is up");
    key(11, 60);                                    /* a hat twice, quickly */
    host_wait_ms(40);
    host_key(11, 1);
    host_wait_ms(30);
    CHECK(sounding(0, 42), "..a repeated hit sounds again");
    host_key(11, 0);
    host_btn(B_REC, 0);
    host_frame();
    for (i = 0; i < 16u; i++) {
        uint32_t q;
        ties += trk[0].step[i].time == ST_TIE;
        for (q = 0; trk[0].step[i].time == ST_NOTE && q < trk[0].step[i].n; q++)
            hits += trk[0].step[i].note[q] == 36u;
    }
    CHECK(!ties && hits == 16u, "a pad held through a stalled panel: no tie, every kick of the steps after kept");
    host_tap(B_PLAY);
    host_tempo(60);
}

/* round 15 (1): the fallback Wobble's claim is to what Easy wrote, never to what the knob followed: the app's
 * depth written after Easy's (a UI frame since: the knob follows it) stays the app's when Easy is left, and so does
 * the app's write of the very value Easy wrote; Easy's next move of Wobble takes the app's depth as its baseline */
static void t_r15_wobble_app(void)
{
    int16_t r0, d0, w, x;
    uint32_t id;
    host_easy();
    host_tap(B_SCL);
    host_wait_ms(GN_PICK_MS);
    id = gn_fb_map(trk[1].eng_req, 1u)->id;
    app_track_param(1, P_LRATE, 20);                /* (the sound's own slow LFO) */
    host_wait_ms(1600);
    r0 = trk[1].p[P_LRATE];
    d0 = trk[1].p[id];
    host_turn(EN_K2, 3);
    w = trk[1].p[id];
    CHECK(trk[1].p[P_LRATE] == 89 && w != d0 && (gn.wob_mask & 2u), "(another sound's Wobble moved: Easy's claim)");
    for (x = 1; x == w || x == d0; x++)
        ;
    app_track_param(1, id, x);                      /* the app edits the LFO's depth */
    host_frame();                                   /* a UI frame: the knob follows it */
    CHECK(trk[1].p[id] == x && gn.on, "..the app's depth, a UI frame since");
    host_hold_switch(2100);
    CHECK(!gn.on && trk[1].p[id] == x && trk[1].p[P_LRATE] == r0, "..Easy left: the app's depth stays the app's, the rate put back");
    /* the app writes the very value Easy wrote */
    host_hold_switch(2100);
    host_wait_ms(3100);
    host_tap(B_SCL);
    host_turn(EN_K2, 2);
    w = trk[1].p[id];
    CHECK(w != x && trk[1].p[P_LRATE] == 89, "(moved again: a new claim, the app's depth its baseline)");
    app_track_param(1, id, w);
    host_frame();
    host_hold_switch(2100);
    CHECK(!gn.on && trk[1].p[id] == w && trk[1].p[P_LRATE] == r0, "..the app's write of Easy's own value stays the app's");
    /* the app's depth, then Wobble moved and back: the app's depth again */
    host_hold_switch(2100);
    host_wait_ms(3100);
    host_tap(B_SCL);
    host_turn(EN_K2, 2);
    for (x = 1; x == w || x == trk[1].p[id]; x++)
        ;
    app_track_param(1, id, x);
    host_frame();
    host_turn(EN_K2, 2);
    host_turn(EN_K2, -2);
    CHECK(trk[1].p[id] == x && trk[1].p[P_LRATE] == r0 && !(gn.wob_mask & 2u),
          "..the app's depth, Wobble moved and back to its place: the app's depth, the rate as it was");
    host_hold_switch(2100);
    CHECK(!gn.on && trk[1].p[id] == x && trk[1].p[P_LRATE] == r0, "..then left: nothing changed");
}

/* round 15 (2): the first key always sounds. (a) Track 1 already on VOICE (the Gnome's engine: no engine switch
 * to wait for): the Gnome's load asks the audio to release the track's notes (seq.c panic_req); the key is played
 * after that, never before it. (b) The app changes track 1's sound before the UI's frame sees it: no Gnome, and
 * the key plays the app's sound */
static void t_r15_first_note(void)
{
    uint32_t e, pr;
    host_power_on();
    host_frame();
    app_preset(GE_VOICE, 2);                        /* Advanced: track 1 on VOICE, not the Gnome's preset */
    host_wait_ms(60);
    host_hold_switch(2100);
    host_wait_ms(3100);
    CHECK(gn.on && gn.entry_put && trk[0].engine == GE_VOICE && trk[0].preset == 2u, "(Easy, the Gnome shown, track 1 on VOICE)");
    host_key(7, 1);
    host_frame();
    host_wait_ms(60);
    CHECK(gn_easy_of(&trk[0]) == GN_GROUP[0] && voices_on(0) > 0u, "(a) the first key: the Gnome put there, the key sounds");
    host_wait_ms(300);
    CHECK(voices_on(0) > 0u, "..and keeps sounding while held");
    host_key(7, 0);
    host_wait_ms(1000);
    /* (b) */
    host_power_on();
    host_frame();
    host_hold_switch(2100);
    host_wait_ms(3100);
    e = GE_PHASE;                                   /* (round 24: PHASE RESO, none of Easy's, which it would adopt) */
    pr = 3u;
    CHECK(gn.on && gn.entry_put && song.sel == 0u && (trk[0].eng_req != e || pr != trk[0].preset) && !easy_preset(e, pr),
          "(Easy, the Gnome shown on track 1)");
    app_preset(e, pr);                              /* the app: track 1's sound */
    host_key(7, 1);                                 /* a key before the UI's frame */
    host_frame();
    host_wait_ms(60);
    CHECK(!gn.entry_put && gn_easy_of(&trk[0]) == GN_NOSND && trk[0].eng_req == e && trk[0].preset == pr,
          "(b) the app's sound on track 1: no Gnome");
    CHECK(voices_on(0) > 0u, "..the key played before the UI saw it sounds, the app's sound");
    host_wait_ms(300);
    CHECK(voices_on(0) > 0u, "..and keeps sounding while held");
    host_key(7, 0);
}

/* round 16 (1): a TRACK_PARAM rejected for its track cannot retire another track's Wobble claim. */
static void t_r16_wobble_invalid_track(void)
{
    int16_t r0, d0;
    uint32_t id;
    host_easy();
    app_preset(GE_ANALOG, 0);                       /* track 0 now uses its fallback knobs (SAW LEAD: none of Easy's) */
    app_track_param(0, P_LRATE, 20);
    host_wait_ms(1600);
    id = gn_fb_map(trk[0].eng_req, 1u)->id;
    r0 = trk[0].p[P_LRATE];
    d0 = trk[0].p[id];
    host_turn(EN_K2, 3);
    CHECK(trk[0].p[P_LRATE] == 89 && trk[0].p[id] != d0 && (gn.wob_mask & 1u),
          "(track 0's fallback Wobble moved: Easy's claim)");
    app_track_param(4, P_LRATE, 50);                /* rejected by the editor: no such track */
    CHECK(trk[0].p[P_LRATE] == 89 && gn.wob_rate[0] == r0 && gn.wob_depth[0] == d0,
          "a rejected track 4 write leaves track 0's Wobble claim intact");
    host_hold_switch(2100);
    CHECK(!gn.on && trk[0].p[P_LRATE] == r0 && trk[0].p[id] == d0,
          "..Easy left: both rate and depth restored to their baseline");
}

/* round 16 (2): release and repress after the load's panic, before the UI replays the queued entry key.
 * Count MIDI note-ons: VOICE's mono stack can hide a duplicate same-pitch onset from the voice age. */
static void t_r16_entry_repress(void)
{
    host_power_on();
    host_frame();
    app_preset(GE_VOICE, 2);
    host_wait_ms(60);
    host_hold_switch(2100);
    host_wait_ms(3100);
    CHECK(gn.entry_put && trk[0].engine == GE_VOICE, "(Gnome shown, track 0 already on VOICE)");
    host_key(7, 1);
    host_audio_us(15000);
    CHECK(gn.entry_keys & (1u << 7), "(first key queued)");
    ui_input();                                    /* load the Gnome; wait for its panic */
    CHECK(!gn.entry_put && (panic_req & 1u) && (gn.entry_keys & (1u << 7)),
          "(Gnome loaded; first key still queued behind panic)");
    host_audio_us(15000);
    CHECK(!(panic_req & 1u) && !sounding(0, 60), "(panic consumed; no UI replay yet)");
    host_key(7, 0);
    host_audio_us(15000);                           /* release seen by audio, UI still stalled */
    usb.config = 1;
    mo_w = mo_r = 0;
    host_key(7, 1);
    host_audio_us(15000);
    CHECK(mo_w == 1u && (midi_out_q[0] & 0x00FFFF0Fu) == 0x003C9009u && sounding(0, 60),
          "the fresh press sends exactly one note-on for C4 on track 0 and sounds");
    ui_input();                                    /* the old queued press must not play again */
    CHECK(mo_w == 1u && sounding(0, 60), "..UI replay adds no second note-on for that key");
    host_wait_ms(300);
    CHECK(mo_w == 1u && sounding(0, 60), "..one note-on, and the held key keeps sounding");
    host_key(7, 0);
}

/* round 17: a rejected track must not retire track 0's kept transpose baseline. */
static void t_r17_kept_invalid_track(void)
{
    host_power_on();
    host_frame();
    app_track_param(0, P_TRANS, 12);
    host_hold_switch(2100);
    host_wait_ms(3100);
    CHECK(gn.on && gn.entry_put && trk[0].p[P_TRANS] == 12, "(Easy entered, Gnome still pending, TRANS 12)");
    key(7, 60);
    CHECK(gn_easy_of(&trk[0]) == GN_GROUP[0] && trk[0].p[P_TRANS] == 0 &&
          (gn.kept_mask[0] & (1u << 7)) && gn.kept_was[0][7] == 12,
          "(first key loaded the Gnome: TRANS 0, kept baseline 12)");
    app_track_param(4, P_TRANS, 3);
    CHECK(trk[0].p[P_TRANS] == 0 && (gn.kept_mask[0] & (1u << 7)) && gn.kept_was[0][7] == 12,
          "a rejected track 4 TRANS write leaves track 0's kept claim intact");
    host_hold_switch(2100);
    CHECK(!gn.on && trk[0].p[P_TRANS] == 12, "..Easy left: TRANS restored to 12");
}

/* Invalid setting P_COUNT must not match gn_id_param's no-selector sentinel. */
static void t_r17_sound_invalid_setting(void)
{
    host_easy_gnome();
    CHECK(gn_easy_of(&trk[0]) == GN_GROUP[0], "(Gnome owned)");
    app_set(P_COUNT, 3);
    CHECK(gn_easy_of(&trk[0]) == GN_GROUP[0], "SET with invalid setting keeps the Gnome's ownership");
    host_easy_gnome();
    app_track_param(0, P_COUNT, 3);
    CHECK(gn_easy_of(&trk[0]) == GN_GROUP[0], "TRACK_PARAM with invalid setting keeps the Gnome's ownership");
}

/* Rejected indices cannot cancel the grid's undo (including motion) or the kept level. */
static void t_r17_ownership_invalid_indices(void)
{
    static const struct { uint8_t cmd, n, a[2 + FM6_PACKED]; } bad[] = {
        {ED_FM6_PUT, 2 + FM6_PACKED, {ED_FM6_TRACK, NTRK}},
        {ED_TRACK_PARAM, 4, {NTRK, P_SLEN, 24, 64}},
        {ED_TRACK_MIX, 4, {NTRK, 3, 64, 0}},
        {ED_TRACK_STEP, 10, {NTRK, 0, 1, 60, 0, 0, 0, ST_NOTE, 0, 100}},
        {ED_TRACK_STEP, 10, {0, NSTEP, 1, 60, 0, 0, 0, ST_NOTE, 0, 100}},
        {ED_STEP_SET, 9, {NSTEP, 1, 60, 0, 0, 0, ST_NOTE, 0, 100}},
        {ED_MOTION, 6, {0, 3, 0, P_COUNT, 20, 64}},
        {ED_MOTION, 6, {0, 3, NSTEP, P_DLY, 20, 64}},
        {ED_MOTION, 4, {0, 4, 0, P_COUNT}},
        {ED_MOTION, 4, {0, 4, NSTEP, P_DLY}},
    };
    uint32_t i, gen, grid, kept;
    motion_store_t before;
    for (i = 0; i < NELEM(bad); i++) {
        organ_empty(64, GN_DIV32);
        put_note(0, 8, 60);
        app_motion_event(0, 8, P_DLY, 20);
        before = motion;
        host_tap(B_GLO);
        host_turn(EN_K1, -1);                           /* a kept level claim */
        host_tap(B_SEQ);
        host_turn(EN_K3, 1);                            /* SNAP on: steps and motion have an undo */
        gen = gn.mgen;
        grid = gn.grid_set;
        kept = gn.kept_mask[0];
        CHECK(gn.undo_of == 1u && grid && (kept & 1u) && trk[0].p[P_SDIV] == GN_DIV16,
              "(grid edit has undo, chosen grid and kept level)");
        host_request(bad[i].cmd, bad[i].a, bad[i].n);
        if (gn.undo_of != 1u || gn.mgen != gen || gn.grid_set != grid || gn.kept_mask[0] != kept)
            printf("    rejected index case %u (command %u)\n", i, bad[i].cmd);
        CHECK(gn.undo_of == 1u && gn.mgen == gen && gn.grid_set == grid && gn.kept_mask[0] == kept,
              "rejected index preserves undo, motion generation, grid choice and kept level");
        easy_rec_hold();
        CHECK(trk[0].p[P_SDIV] == GN_DIV32 && trk[0].step[8].n == 1u && trk[0].step[8].note[0] == 60 &&
              !memcmp(&motion, &before, sizeof motion), "..SAVE held restores the original steps, grid and motion");
    }
}

/* ------------------------------------------------------- the panel --- */
/* round 7, pick 2: FX SCL ENV LFO pick tracks 1 to 4, each lit; SEQ the loop page; GLO the four volumes on
 * the four knobs; PLAY starts and stops from any page; ARP the arpeggio on and off (Easy's sound only);
 * HOME back to playing; what Easy writes of a track's kept settings (the levels, the arpeggio) goes back
 * when Easy is left */
static void t_panel(void)
{
    static const uint32_t TB[4] = {B_FX, B_SCL, B_ENV, B_LFO};
    static const uint32_t VIEW_BTN[4] = {0, B_SEQ, B_GLO, B_EDIT};
    int16_t lvl[NTRK], arp[4];
    uint32_t k, v;
    host_easy();
    for (k = 0; k < NTRK; k++)
        lvl[k] = trk[k].p[P_LEVEL];
    for (k = 0; k < 4u; k++) {
        host_tap(TB[k]);
        CHECK(song.sel == k, "FX SCL ENV LFO: tracks 1 to 4");
        CHECK(host_led(panel.btn[TB[k]]) && !host_led(panel.btn[TB[(k + 1u) % 4u]]), "..the selected one lit");
    }
    host_tap(B_FX);
    host_tap(B_SEQ);
    CHECK(gn_view() == GV_LOOP && host_led(panel.btn[B_SEQ]), "SEQ: the loop page (the circle), SEQ lit");
    host_tap(B_SEQ);
    CHECK(gn_view() == GV_STEPS, "SEQ again: the steps");
    host_tap(B_SEQ);
    CHECK(gn_view() == GV_RECORD, "SEQ a third time: the record screen");
    host_tap(B_SEQ);
    CHECK(gn_view() == GV_LOOP, "SEQ again: the circle");
    host_tap(B_HOME);
    CHECK(gn_view() == GV_HOME, "HOME: Home (round UX-1)");
    host_tap(B_GLO);
    CHECK(gn_view() == GV_MIX && host_led(panel.btn[B_GLO]), "GLO: the four volumes, GLO lit");
    for (k = 0; k < 4u; k++)
        host_turn(EN_K1 + k, -(int32_t)(k + 1u) * 3);
    for (k = 0; k < NTRK; k++)
        CHECK(trk[k].p[P_LEVEL] == clamp(lvl[k] - (int32_t)(k + 1u) * 12, 0, 127), "..K1 to K4: tracks 1 to 4's volumes");
    host_tap(B_HOME);
    CHECK(gn_view() == GV_HOME && gn.page == GP_HOME, "HOME: Home (round UX-1)");
    for (v = 0; v < 4u; v++) {                      /* PLAY from every page */
        host_tap(B_HOME);
        if (VIEW_BTN[v])
            host_tap(VIEW_BTN[v]);
        host_tap(B_PLAY);
        host_wait_ms(60);
        CHECK(song.playing, "PLAY starts the loop, on every page");
        host_tap(B_PLAY);
        host_wait_ms(60);
        CHECK(!song.playing, "..and stops it");
    }
    host_tap(B_HOME);
    host_turn(EN_ALGO, 1);                        /* the Gnome on track 1 */
    host_wait_ms(GN_PICK_MS);
    for (k = 0; k < 4u; k++)
        arp[k] = trk[0].p[P_AMODE + k];
    host_tap(B_ARP);
    CHECK(trk[0].p[P_AMODE] == 1 && trk[0].p[P_ARATE] == 2 && trk[0].p[P_AOCT] == 1 && host_led(panel.btn[B_ARP]),
          "ARP: the arpeggio on (up, 1/16, an octave), ARP lit");
    host_key(7, 1);
    host_wait_ms(300);
    CHECK(trk[0].arp_note != 0u, "..a key held: the arpeggio plays");
    host_key(7, 0);
    host_wait_ms(60);
    host_tap(B_ARP);
    CHECK(trk[0].p[P_AMODE] == 0 && !host_led(panel.btn[B_ARP]), "ARP again: off");
    host_tap(B_ARP);
    host_tap(B_SCL);                                /* track 2: another sound (Felucca's), ARP rests */
    host_tap(B_ARP);
    CHECK(trk[1].p[P_AMODE] == 0, "ARP on another sound: rests");
    host_hold_switch(2100);
    CHECK(!gn.on, "(Advanced)");
    for (k = 0; k < NTRK; k++)
        CHECK(trk[k].p[P_LEVEL] == lvl[k], "leaving: the volumes Easy set go back");
    for (k = 0; k < 4u; k++)
        CHECK(trk[0].p[P_AMODE + k] == arp[k], "..and the arpeggio Easy set");
}

/* round 7, pick 2: SAVE saves the song (the Gnome says SAVED, once read back from flash): into the slot of
 * Easy's own song, else the first empty one, never over a song of someone else's (FULL); while the loop
 * plays it stops first. A SAVE press that HOME joined saves nothing, and the hold to Advanced neither */
/* the app's BACKUP_PUT of project slot `slot` (object 2 + slot): begin, chunks, commit; its rc */
static uint32_t app_backup_put_slot(uint32_t slot, const void *obj, uint32_t len)
{
    static uint8_t a[12 + 320];
    const uint8_t *src = obj;
    uint32_t off, i;
    a[0] = 0;
    a[1] = (uint8_t)(2u + slot);
    bk_u35(a + 2, len);
    bk_u35(a + 7, st_crc32(src, len));
    host_request(ED_BACKUP_PUT, a, 12);
    if (app_reply(2))
        return app_reply(2);
    for (off = 0; off < len; off += 256u) {
        uint32_t n = len - off > 256u ? 256u : len - off, k = 7;
        const uint8_t *q = src + off;
        a[0] = 1;
        a[1] = (uint8_t)(2u + slot);
        bk_u35(a + 2, off);
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
        if (app_reply(2))
            return app_reply(2);
    }
    a[0] = 2;
    a[1] = (uint8_t)(2u + slot);
    host_request(ED_BACKUP_PUT, a, 2);
    return app_reply(2);
}

static int slot_same(uint32_t slot, const project_store_t *ram)   /* RAM and flash both as they were */
{
    static project_store_t fl;
    return !memcmp(&proj_slot[slot], ram, sizeof *ram) &&
           st_load(OBJ_PROJECT0 + slot, &fl, sizeof fl) == (int)sizeof fl && !memcmp(&fl, ram, sizeof fl);
}

/* round 7, pick 2; round 8: SAVE saves the song (the Gnome says SAVED, once read back from flash) into Easy's
 * own slot, which is the one Easy last wrote and still holds exactly that (a fingerprint in the settings
 * record), else the first empty one, never over a song of someone else's (FULL); a name proves nothing (a
 * project of the app's named GNOMAD SONG is the app's); an editor save or a backup restore into Easy's slot
 * ends it; while the loop plays SAVE stops it first. A SAVE press that HOME joined saves nothing */
static void t_save_song(void)
{
    char b[PROJ_NAME_LEN + 1u];
    project_store_t was[4], theirs;
    uint32_t k;
    host_start_easy = 1;                            /* (the slot's ownership rides in the settings record) */
    host_power_on();
    host_frame();
    host_wait_ms(3100);
    CHECK(gn.on, "(Easy, as the first power-on starts)");
    app_project(1, 0);                              /* the app saved a song of its own in slot 1 */
    str_cpy(proj_name, GN_NAME " SONG", sizeof proj_name);
    app_project(1, 1);                              /* ..and one named GNOMAD SONG in slot 2 */
    str_cpy(proj_name, "THEIRS", sizeof proj_name);
    host_wait_ms(GN_SAVED_MS + 100u);
    memcpy(&theirs, &proj_slot[1], sizeof theirs);
    some_steps(0, 40);
    host_tap(B_SAVE);
    host_wait_ms(60);
    CHECK(project_name(2, b) && str_eq(b, GN_NAME " SONG"), "SAVE: the song into the first empty slot (3), named");
    CHECK(slot_same(1, &theirs), "..the app's GNOMAD SONG in slot 2 untouched, RAM and flash (a name is no ownership)");
    CHECK(project_name(0, b) && !str_eq(b, GN_NAME " SONG"), "..the app's song in slot 1 untouched");
    CHECK(gn_view() == GV_SAVED && gn.saved_mine == 1u, "..the Gnome says SAVED (read back from flash)");
    CHECK((favorites.factory[15][29] & 0xECu) == 0x80u && (favorites.factory[15][29] & 3u) == 2u &&   /* (round 27: bit 4) */
          gn_song_owned() == 2u, "..the slot Easy owns now: 3, with its fingerprint in the record");
    host_wait_ms(GN_SAVED_MS + 100u);
    some_steps(0, 50);
    transport_req = 1;
    host_wait_ms(100);
    host_tap(B_SAVE);
    host_wait_ms(100);
    CHECK(!song.playing && gn.saved_mine == 1u, "SAVE while playing: the loop stops, then the song is saved");
    CHECK(project_name(2, b) && !memcmp(proj_scratch.t[0].step, trk[0].step, sizeof trk[0].step) && !project_name(3, b) &&
          slot_same(1, &theirs), "..into the same slot (Easy's, the pattern as now), no other");
    host_wait_ms(GN_SAVED_MS + 100u);
    some_steps(1, 60);                              /* (the music changed, by the app) */
    app_project(1, 2);                              /* the app saves its music over Easy's slot 3 */
    host_wait_ms(GN_SAVED_MS + 100u);
    CHECK(gn_song_owned() == GN_NO_SLOT, "an editor save over Easy's slot: no longer Easy's");
    memcpy(was, proj_slot, sizeof was);
    host_tap(B_SAVE);
    host_wait_ms(60);
    CHECK(gn.saved_mine == 1u && slot_same(2, &was[2]) && project_used(3) && gn_song_owned() == 3u,
          "..SAVE: into the empty slot 4, slot 3 the app's as it saved it");
    host_wait_ms(GN_SAVED_MS + 100u);
    CHECK(app_backup_put_slot(3, &theirs, sizeof theirs) == 0u, "(the app restores slot 4 from a backup)");
    CHECK(gn_song_owned() == GN_NO_SLOT, "a backup restored into Easy's slot: no longer Easy's");
    memcpy(was, proj_slot, sizeof was);
    host_tap(B_SAVE);
    host_wait_ms(60);
    CHECK(gn.saved_mine == 2u && !memcmp(was, proj_slot, sizeof was) && slot_same(3, &was[3]),
          "every slot someone else's: FULL, nothing written");
    host_wait_ms(GN_SAVED_MS + 100u);
    /* SAVE with HOME joining: nothing, either order; a release before 2 s too */
    host_btn(B_SAVE, 1);
    host_wait_ms(200);
    host_btn(B_HOME, 1);
    host_wait_ms(300);
    host_btn(B_HOME, 0);
    host_wait_ms(100);
    host_btn(B_SAVE, 0);
    host_wait_ms(60);
    CHECK(gn.on && !gn.saved_t && !gn.save_pend && !memcmp(was, proj_slot, sizeof was), "SAVE, HOME joins, let go: no save");
    host_btn(B_HOME, 1);
    host_wait_ms(100);
    host_btn(B_SAVE, 1);
    host_wait_ms(300);
    host_btn(B_SAVE, 0);
    host_wait_ms(100);
    host_btn(B_HOME, 0);
    host_wait_ms(60);
    CHECK(gn.on && !gn.saved_t && !gn.save_pend && gn.page == GP_HOME, "HOME, SAVE, let go before 2 s: no save, no HOME (the page as it was)");
    host_hold_switch(2100);
    CHECK(!gn.on && !gn.save_pend && !memcmp(was, proj_slot, sizeof was), "HOME + SAVE held 2 s: Advanced, nothing saved");
}

/* ------------------------------------------------------ the screen --- */
/* finding 6, review 6: in each frame Easy has the screen (gs_owned: gn_draw returned 1), every pixel
 * sent to the LCD, fills and blits alike (host_lcd_px: the host's own count of all LCD traffic, not
 * Easy's), at most GS_BUDGET_PX (240 x 32); a frame in Easy that is not Easy's own is the update
 * countdown's only (Felucca's draw_uboot, upstream's) */
static uint32_t px_max, px_stray;
#define GN_FELUCCA_PAGE_PX 57600u                   /* FORK.md: Felucca's HOME page drawn whole */
#define GN_COUNTDOWN_PX 74160u                          /* FORK.md: the countdown's frame of a new second */
#define GN_COUNTDOWN_IDLE_PX 0u                     /* FORK.md: .. a frame while it stands */

static void frames_watch(uint32_t n)
{
    while (n--) {
        uint64_t px0 = host_lcd_px;
        host_frame();
        if (gs_owned && host_lcd_px - px0 > px_max)
            px_max = (uint32_t)(host_lcd_px - px0);
        if (gn.on && !gs_owned && !ui.uboot)
            px_stray++;
    }
}

static int screen_whole_same(void)                  /* time stands still: drawn whole, the same pixels? */
{
    static uint16_t was[240 * 240];
    uint32_t i;
    for (i = 0; i < 300u && (gs_job.on || gs_px_call); i++)
        ui_draw();
    memcpy(was, host_screen, sizeof was);
    gs_shown_view = GV_NONE;                        /* (all of it again) */
    for (i = 0; i < 300u; i++) {
        ui_draw();
        if (!gs_job.on && !gs_px_call)
            break;
    }
    if (memcmp(was, host_screen, sizeof was)) {
        uint32_t k, n = 0, fx = 0, fy = 0;
        for (k = 0; k < 240u * 240u; k++)
            if (was[k] != host_screen[k] && !n++) {
                fx = k % 240u;
                fy = k / 240u;
            }
        printf("    view %u: %u pixels differ, the first at (%u, %u)\n", gn_view(), n, fx, fy);
        return 0;
    }
    return 1;
}

static void t_screen(void)
{
    uint64_t px0;
    uint32_t i, ok = 1;
    host_easy();
    frames_watch(10);
    for (i = 0; i < 9u; i++) {
        host_turn(EN_ALGO, 1);
        frames_watch(30);
        host_key(i % 2u ? 7u : 9u, 1);
        frames_watch(20);
        ok &= (uint32_t)screen_whole_same();
        host_key(i % 2u ? 7u : 9u, 0);
        frames_watch(120);
        ok &= (uint32_t)screen_whole_same();
        host_turn(EN_K1 + i % 4u, i % 2u ? 3 : -3);
        frames_watch(5);
        ok &= (uint32_t)screen_whole_same();
    }
    easy_record();      
    frames_watch(60);
    ok &= (uint32_t)screen_whole_same();
    host_tap(B_REC);
    host_tap(B_HOME);
    frames_watch(40);
    ok &= (uint32_t)screen_whole_same();
    host_tap(B_HOME);
    frames_watch(40);
    easy_drums();                                   /* the drums, a pad lit */
    frames_watch(40);
    host_key(9, 1);
    frames_watch(3);
    ok &= (uint32_t)screen_whole_same();
    host_key(9, 0);
    {
        uint8_t a[2] = {1, 0};                      /* saved */
        host_request(ED_PROJECT, a, 2);
    }
    frames_watch(20);
    ok &= (uint32_t)screen_whole_same();
    key(7, 30);
    host_btn(B_HOME, 1);                            /* the hold that leaves, part way */
    host_btn(B_SAVE, 1);
    frames_watch(60);
    ok &= (uint32_t)screen_whole_same();
    host_btn(B_HOME, 0);
    host_btn(B_SAVE, 0);
    frames_watch(5);
    host_hold_switch(2100);                         /* out, and in: the start-up part way */
    host_hold_switch(2100);
    frames_watch(30);
    ok &= (uint32_t)screen_whole_same();
    CHECK(ok, "the screen drawn part by part equals it drawn whole, in every view");
    CHECK(px_max <= GS_BUDGET_PX, "never more than 240 x 32 pixels sent in a frame of Easy's (all LCD traffic)");
    CHECK(px_stray == 0u, "every frame in Easy drawn by Easy (but the update countdown)");
    frames_watch(200);
    px0 = host_lcd_px;
    for (i = 0; i < 10u; i++) {
        app_set(P_ATK, 10 + (int32_t)i);            /* the app moves a setting: ui.force each time */
        frames_watch(2);
    }
    CHECK(host_lcd_px - px0 < 240u * 240u, "ten editor SETs: less than one screen's pixels (finding 6)");
    CHECK(px_max <= GS_BUDGET_PX, "..each frame within 240 x 32");
}

/* review 6: leaving. Felucca's ground a strip a frame, each within GS_BUDGET_PX of all LCD traffic,
 * 240 x 240 in all, then Felucca's own first frame: upstream's whole page, as at any page change of
 * Felucca's (upstream's drawing, outside Easy's budget: measured here, not bounded). The screen is then
 * Felucca's page on its own ground: the pixels Felucca draws on a cleared screen. The update countdown
 * (main.c: OCT- + OCT+ past 2 s; main.c's loop is not in the host, so its two lines are played here),
 * in Easy and during the leaving: Felucca's frames, then Easy's whole screen again strip by strip, or
 * Felucca's page whole */
static int felucca_whole_same(void)                 /* Felucca's page as on a cleared screen? */
{
    static uint16_t was[240 * 240];
    memcpy(was, host_screen, sizeof was);
    lcd_fill(0, 0, 240, 240, T_BG);
    ui.force = 1;
    ui_draw();
    return !memcmp(was, host_screen, sizeof was);
}

/* every pixel one frame sends (fills and blits: host_lcd_px), and the largest of each kind so far: the
 * frames Easy has (gs_owned), and Felucca's (its pages, the update countdown) */
static uint32_t fx_easy, fx_felucca;
static uint32_t frame_px(void)
{
    uint64_t px0 = host_lcd_px;
    uint32_t d;
    host_frame();
    d = (uint32_t)(host_lcd_px - px0);
    if (gs_owned)
        fx_easy = d > fx_easy ? d : fx_easy;
    else
        fx_felucca = d > fx_felucca ? d : fx_felucca;
    return d;
}

/* Felucca's own: its page drawn whole (a page change, ui.force), and the countdown's first frame, as in
 * Advanced; the screen and the state as they were after (palette-independent references) */
static uint32_t felucca_page_px(void)
{
    static uint16_t was[240 * 240];
    uint64_t px0;
    memcpy(was, host_screen, sizeof was);
    ui.force = 1;
    px0 = host_lcd_px;
    ui_draw();
    memcpy(host_screen, was, sizeof was);
    return (uint32_t)(host_lcd_px - px0);
}

static uint32_t felucca_countdown_px(void)
{
    static uint16_t was[240 * 240];
    uint64_t px0;
    memcpy(was, host_screen, sizeof was);
    ui.uboot = 3;
    ui.force = 1;
    px0 = host_lcd_px;
    ui_draw();
    ui.uboot = 0;
    ui.force = 1;
    ui_draw();
    memcpy(host_screen, was, sizeof was);
    return (uint32_t)(host_lcd_px - px0);   /* (both frames: the countdown's and the page's after it) */
}

static void t_screen_exit(void)
{
    uint32_t i, strips = 0, big = 0, left = 0, total = 0, felucca = 0, page, cd, cd_first, cd_rest = 0, cd2, cd1;
    host_easy();
    host_turn(EN_ALGO, 1);
    frames_watch(60);
    fx_easy = fx_felucca = 0;
    host_btn(B_HOME, 1);
    host_btn(B_SAVE, 1);
    for (i = 0; i < 400u; i++) {
        uint32_t on = gn.on, d = frame_px();
        left |= on && !gn.on;
        if (!left)
            continue;
        if (!gs_owned) {
            felucca = d;
            break;
        }
        strips++;
        total += d;
        big = d > big ? d : big;
    }
    host_btn(B_HOME, 0);
    host_btn(B_SAVE, 0);
    frame_px();
    page = felucca_page_px();
    printf("    leaving: %u frames of Easy's, %u pixels, at most %u a frame; then Felucca's first frame %u "
           "(its own page change: %u)\n", strips, total, big, felucca, page);
    CHECK(left && strips == 8u && total == 240u * 240u && big == GS_BUDGET_PX,
          "leaving: Felucca's ground in 8 strips, 7,680 pixels the largest, all of the screen");
    CHECK(!gn.drawn && felucca == page, "..then Felucca's own first frame: exactly its own page change");
#if GSK_BUILD
    if (settings.palette != GSK_PAL)
#endif
        CHECK(felucca == GN_FELUCCA_PAGE_PX, "..57,600 pixels (FORK.md: upstream's palettes)");
    CHECK(felucca_whole_same(), "..the screen: Felucca's page on its ground, nothing of Easy's left");
    /* the countdown in Easy (main.c's two lines played here: main.c's loop is not in the host) */
    host_hold_switch(2100);
    px_max = px_stray = 0;
    frames_watch(60);
    ui.uboot = 3;
    ui.force = 1;
    cd_first = frame_px();
    CHECK(!gs_owned && gn.on, "the countdown in Easy: Felucca's frame");
    for (i = 0; i < 5u; i++) {
        uint32_t d = frame_px();
        cd_rest = d > cd_rest ? d : cd_rest;
    }
    ui.uboot = 2;
    ui.force = 1;
    cd2 = frame_px();
    frames_watch(4);
    ui.uboot = 1;                                   /* (and the last second) */
    ui.force = 1;
    cd1 = frame_px();
    frames_watch(4);
    ui.uboot = 0;                                   /* let go */
    ui.force = 1;
    ui_message("UPDATE CANCELLED");
    frames_watch(20);
    CHECK(px_max <= GS_BUDGET_PX && px_stray == 0u, "..then Easy's screen again, each frame within 240 x 32");
    CHECK(screen_whole_same(), "..all of it: drawn bit by bit as drawn whole");
    host_hold_switch(2100);                         /* (out, to measure Felucca's own countdown there) */
    host_wait_ms(300);
    cd = felucca_countdown_px() - felucca_page_px();
    printf("    the countdown: its first frame %u pixels (in Advanced: %u), %u while it stands, %u and %u at 2 and 1\n",
           cd_first, cd, cd_rest, cd2, cd1);
    CHECK(cd_first == cd && cd2 == cd && cd1 == cd && cd_rest < cd, "the countdown in Easy, 3, 2 and 1: exactly Felucca's own frames");
#if GSK_BUILD
    if (settings.palette != GSK_PAL)
#endif
        CHECK(cd == GN_COUNTDOWN_PX && cd_rest == GN_COUNTDOWN_IDLE_PX, "..the numbers FORK.md states (upstream's palettes)");
    printf("    the largest frames: Easy's %u, Felucca's %u\n", fx_easy, fx_felucca);
    CHECK(fx_easy == GS_BUDGET_PX && fx_felucca == (page > cd ? page : cd),
          "every frame of the test: Easy's 7,680 at most, Felucca's its page change or the countdown");
    /* the countdown while the leaving clears */
    host_hold_switch(2100);
    host_wait_ms(300);
    host_btn(B_HOME, 1);
    host_btn(B_SAVE, 1);
    for (i = 0; i < 400u && gn.on; i++)
        host_frame();
    host_frame();
    CHECK(!gn.on && gs_owned && gn.exit_y > 0u && gn.exit_y < 240u, "leaving, part way");
    ui.uboot = 3;
    ui.force = 1;
    host_frame();
    CHECK(!gs_owned && !gn.drawn && !gn.exit_y, "..the countdown: Felucca's, the rest of the clearing the countdown's");
    ui.uboot = 0;
    ui.force = 1;
    host_frame();
    host_btn(B_HOME, 0);
    host_btn(B_SAVE, 0);
    host_wait_ms(60);
    CHECK(felucca_whole_same(), "..let go: Felucca's page whole");
}

/* round 3: Easy with the palette under it, the editor's save traffic and the leaving, together (the
 * merged firmware's: where the skin is built in, GNOMAD; else HI-CON). The palette stays through Easy, an
 * editor save and SONG's read keep Easy's sounds and undo, the leaving gives Felucca's page whole on its
 * ground, its first frame exactly its own page change, and the setting is in flash as the palette's
 * stored id, which reads back as the palette (a power-on's import) */
static void t_palette_session(void)
{
#if GSK_BUILD
    const uint32_t pal = GSK_PAL;
#else
    const uint32_t pal = 7u;
#endif
    uint8_t a[2] = {0, (uint8_t)pal};
    uint32_t i, left = 0, felucca = 0;
    persist_t p;
    host_power_on();
    host_wait_ms(60);
    host_request(ED_UI_SET, a, 2);
    host_wait_ms(200);
    CHECK(settings.palette == pal, "the app picks the palette");
#if GSK_BUILD
    CHECK(gsk.on, "..GNOMAD: the skin on");
#endif
    host_hold_switch(2100);
    easy_drums();                                   /* the drums, a recording */
    host_wait_ms(60);
    easy_record();      
    key(9, 60);
    host_tap(B_REC);
    host_tap(B_PLAY);
    host_wait_ms(60);
    app_project(1, 1);                              /* the app saves, then reads the song (web/editor.html) */
    app_song_read();
    host_wait_ms(60);
    CHECK(gn.own[3] && gn.undo_of == 4u && settings.palette == pal, "Easy's drums and undo, the palette: as they were");
    fx_easy = fx_felucca = 0;
    host_btn(B_HOME, 1);
    host_btn(B_SAVE, 1);
    for (i = 0; i < 400u; i++) {
        uint32_t on = gn.on, d = frame_px();
        left |= on && !gn.on;
        if (left && !gs_owned) {
            felucca = d;
            break;
        }
    }
    host_btn(B_HOME, 0);
    host_btn(B_SAVE, 0);
    frame_px();
    CHECK(left && fx_easy == GS_BUDGET_PX && felucca == felucca_page_px(),
          "leaving: Easy's strips, then Felucca's own page change exactly");
    CHECK(felucca == GN_FELUCCA_PAGE_PX, "..57,600 pixels, the default palette's figure (FORK.md)");
    CHECK(felucca_whole_same(), "..Felucca's page whole, in the palette, nothing of Easy's left");
    CHECK(settings.palette == pal, "..the palette still the one picked");
#if GSK_BUILD
    CHECK(gsk.on, "..GNOMAD still on");
#endif
    {                                               /* the countdown in Easy, in the palette: 3, 2, 1 */
        uint32_t c3, c2, c1, idle = 0;
        host_hold_switch(2100);
        host_wait_ms(300);
        fx_easy = fx_felucca = 0;
        ui.uboot = 3;
        ui.force = 1;
        c3 = frame_px();
        for (i = 0; i < 3u; i++) {
            uint32_t d = frame_px();
            idle = d > idle ? d : idle;
        }
        ui.uboot = 2;
        ui.force = 1;
        c2 = frame_px();
        ui.uboot = 1;
        ui.force = 1;
        c1 = frame_px();
        ui.uboot = 0;
        ui.force = 1;
        frames_watch(20);
        printf("    the countdown in the palette: %u %u %u pixels, %u while a second stands\n", c3, c2, c1, idle);
        CHECK(c3 == GN_COUNTDOWN_PX && c2 == GN_COUNTDOWN_PX && c1 == GN_COUNTDOWN_PX && idle == GN_COUNTDOWN_IDLE_PX,
              "..the countdown in Easy, 3, 2, 1: 74,160 pixels each, none while a second stands (FORK.md)");
        CHECK(fx_easy <= GS_BUDGET_PX, "..then Easy's screen again, each frame within 240 x 32");
        host_hold_switch(2100);
        host_wait_ms(300);
    }
    host_wait_ms(200);                              /* (settings_poll: the flash copy, stopped) */
    CHECK(st_load(OBJ_SETTINGS, &p, sizeof p) > 0 && p.palette == palette_to_stored(pal) &&
          palette_from_stored(p.palette) == pal, "the setting in flash: the palette's stored id, read back as it");
#if GSK_BUILD
    CHECK(p.palette == GSK_STORED, "..GNOMAD stored as 13");
#endif
}

/* finding 9: the saved screen's lines say what seq.c's routing does, for every channel */
static void t_saved_routing(void)
{
    uint32_t rout, sel, i, ch, ok = 1, cover;
    host_easy();
    for (rout = 0; rout < 2u; rout++)
        for (sel = 0; sel < NTRK; sel++) {
            gs_route_t r[3];
            uint32_t n;
            song.g[G_ROUTE] = (int16_t)rout;
            song.sel = (uint8_t)sel;
            n = gs_routes(r);
            ok &= n > 0u;
            for (i = 0, cover = 0; i < n; i++)
                for (ch = r[i].lo; ch <= r[i].hi; ch++) {
                    uint32_t want = r[i].trk == 0xFFu ? ch : r[i].trk;
                    ok &= (uint32_t)(midi_track(ch) == &trk[want]);
                    cover |= 1u << ch;
                }
            ok &= cover == 0xFFFFu;
        }
    CHECK(ok, "every line true of every channel it names, every channel named (ROUT CH1-4 and SEL)");
    song.g[G_ROUTE] = 0;
    song.sel = 0;
}

static void t_saved(void)
{
    host_easy();
    app_project(1, 1);
    host_wait_ms(30);
    CHECK(gn_view() == GV_SAVED, "the app saves: read back from flash, the saved screen (3g)");
    key(7, 30);
    CHECK(gn_view() != GV_SAVED, "..a key ends it");
    app_project(1, 1);
    host_wait_ms(GN_SAVED_MS + 100u);
    CHECK(gn_view() != GV_SAVED, "..or 6 s");
    host_nor[0x97000u + 2u * 4096u + 4096u + 200u] ^= 0x10u;   /* (a bit of slot 2's copy B: the old copy) */
    flash_ok = 0;
    app_project(1, 1);
    host_wait_ms(30);
    CHECK(gn_view() != GV_SAVED, "no flash: no saved screen");
    flash_ok = 1;
}

/* ------------------------------------------------------- the LEDs --- */
static void t_leds(void)
{
    uint32_t k;
    host_easy();
    host_turn(EN_ALGO, 2);
    host_wait_ms(GN_PICK_MS);
    for (k = 0; k < 27u; k++)
        CHECK(host_led_dim(14u + k), "Easy's sound: every key glows (round 13: the black keys play too)");
    host_key(7, 1);
    host_wait_ms(30);
    CHECK(host_led(14u + 7u), "a white key held: lit");
    host_key(7, 0);
    host_wait_ms(30);
    easy_record();      
    CHECK(host_led(panel.btn[B_REC]), "REC lit while recording");
    host_tap(B_REC);
}

/* ---------------------------------------------------------- upkeep --- */
static void t_upkeep(void)
{
    host_easy();
    CHECK(perf_kill == 1u && kb_mask == 0u && song.grid == 0u, "in Easy: no FX layer effect, no layer keys, no grid");
    CHECK(!ui.menu && !ui.confirm && !name_on(), "..no menu, dialog or NAME");
    CHECK(host_irq_off == 0u, "every fm1_irq_off has its fm1_irq_on");
}

/* -------------------------------------------------------------- fuzz --- */
/* a long random walk over the panel and the app (a fixed seed: the same walk every run), Easy on and
 * off: every parameter of every track stays in its engine's range, the part lists never overflow, no
 * fm1_irq_off is left open, and under the sanitizers no read or write leaves its object. SAVE is never
 * held long enough for Felucca's own undo: upstream 1.0.1's undo_swap of a track with knob motion
 * leaves the motion's base values from the other sound, and at the loop's next turn motion_restore
 * writes them back unclamped (GRAIN PTCH 90, say): Felucca's alone, Easy off, reported in FORK.md */
static uint32_t rnd_state = 12345u;
static uint32_t rnd(uint32_t n)
{
    rnd_state = rnd_state * 1103515245u + 12345u;
    return (rnd_state >> 8) % n;
}

static void t_fuzz(void)
{
    static const uint32_t BTN[14] = {B_HOME, B_SAVE, B_REC, B_PLAY, B_OCTDN, B_OCTUP, B_FX, B_SEQ, B_SCL, B_ENV,
                                     B_LFO, B_EDIT, B_GLO, B_ARP};
    uint32_t i, bt = 0, bid = 0, ok = 1, parts_ok = 1, ons = 0, steps = 3000u;
    if (getenv("GN_FUZZ_SEED"))                     /* (by hand: other walks, longer ones) */
        rnd_state = (uint32_t)atoi(getenv("GN_FUZZ_SEED"));
    if (getenv("GN_FUZZ_STEPS"))
        steps = (uint32_t)atoi(getenv("GN_FUZZ_STEPS"));
    host_power_on();
    host_wait_ms(60);
    for (i = 0; i < steps; i++) {
        uint32_t a = rnd(100);
        if (a < 18u) {
            host_key(rnd(27), (int)rnd(2));
        } else if (a < 30u) {
            host_turn(EN_SELECT + rnd(NE), (int32_t)rnd(9) - 4);
        } else if (a < 40u) {                       /* a tap, or a hold of 0.1 to 1.2 s (SAVE: under 0.7 s) */
            uint32_t b = BTN[rnd(14)];
            host_btn(b, 1);
            host_wait_ms(rnd(2) ? 15u : 100u + rnd(b == B_SAVE ? 500 : 1100));
            host_btn(b, 0);
        } else if (a < 46u) {                       /* the hold, either way */
            fm1_in.buttons = 0;
            host_btn(B_HOME, 1);
            host_btn(B_SAVE, 1);
            host_wait_ms(1500u + rnd(1200));
            host_btn(B_HOME, 0);
            host_btn(B_SAVE, 0);
        } else if (a < 54u) {
            uint32_t e = rnd(NENGINES);
            app_preset(e, rnd(ENGINES[e]->npresets ? ENGINES[e]->npresets + 2u : 2u));
        } else if (a < 62u) {
            app_set(rnd(P_COUNT), (int32_t)rnd(300) - 150);
        } else if (a < 66u) {
            app_track_step(rnd(NTRK), rnd(NSTEP), 30u + rnd(60));
        } else if (a < 68u) {
            app_project(rnd(2), rnd(4));
        } else if (a < 70u) {
            uint8_t b[2] = {0, (uint8_t)rnd(NTRK)};
            host_request(ED_TRACK, b + 1, 1);
        } else if (a < 71u) {                       /* a runtime backup taken and put back */
            app_backup_take();
            app_backup_restore();
        } else if (a < 72u) {
            app_user(rnd(16));
        } else if (a < 73u) {
            app_step_set(rnd(NSTEP), 30u + rnd(60));
        }
        host_frame();
        ons += gn.on;
        ok &= (uint32_t)params_in_range(&bt, &bid);
        parts_ok &= gs_n < GS_NPARTS && gs_shown_n <= GS_NPARTS && gs_pass_n <= GS_NPARTS;
        if (!ok)
            break;
    }
    if (!ok)
        printf("    step %u: track %u parameter %u out of its range\n", i, bt, bid);
    printf("    %u random steps, %u frames of them in Easy\n", steps, ons);
    CHECK(ok, "every parameter in its engine's range all the way");
    CHECK(parts_ok, "the part lists never full");
    CHECK(host_irq_off == 0u, "every fm1_irq_off has its fm1_irq_on");
    CHECK(ons > steps / 10u, "the walk spent a good part of its time in Easy");
}

/* Round 19: reproduce the coverage tour through real panel / MIDI paths. */
static void t_r19_bars(void)
{
    step_t before[NSTEP];
    __typeof__(motion) before_motion;
    uint32_t i;
    host_easy_gnome();
    host_tap(B_SEQ);
    for (i = 0; i < 3; i++) host_turn(EN_K1, 1);
    CHECK(gn_bars(0) == 8, "fixture: eight bars");
    app_step_set(31, 67);
    app_step_set(63, 72);
    motion.count = 1;
    motion.event[0].place = 63;
    motion.event[0].param = P_DLY;
    motion.event[0].value = 50;
    before_motion = motion;
    memcpy(before, trk[0].step, sizeof before);
    for (i = 0; i < 3; i++) host_turn(EN_K1, -1);
    CHECK(gn_bars(0) == 1, "shortened to one bar");
    for (i = 0; i < 3; i++) host_turn(EN_K1, 1);
    CHECK(gn_bars(0) == 8 && !memcmp(before, trk[0].step, sizeof before),
          "8 -> 1 -> 8: all steps, including hidden tail, intact");
    CHECK(!memcmp(&motion, &before_motion, sizeof motion), "hidden motion returns intact");
}

static void r19_notes(uint32_t event)
{
    uint32_t i, n = 0, off = 0, local = 0, local_note;
    host_start_easy = 1;
    host_power_on();
    host_frame();
    host_wait_ms(3100);
    key(7, 50);
    host_wait_ms(100);
    if (event != 2) host_hold_switch(2100);
    usb.config = 1;
    host_frame();
    if (event == 0) host_tap(B_PLAY);
    midi_enqueue(0x643C9009u, 1);
    host_wait_ms(50);
    CHECK(sounding(0, 60), "fixture: incoming USB note is gated");
    host_key(12, 1);
    host_wait_ms(15);
    local_note = kb_chord[12][0];
    mo_r = mo_w = 0;
    if (event == 0) { host_btn(B_PLAY, 1); ui_input(); host_audio_us(1000); }
    else if (event == 1 || event == 2) {
        uint32_t mode = gn.on;
        host_btn(B_HOME, 1); host_btn(B_SAVE, 1);
        for (i = 0; i < 160 && gn.on == mode; i++) host_frame();
        CHECK(gn.on != mode, "mode transition observed");
    } else if (event == 3) host_turn(EN_PRESET, 1);
    else if (event == 5) app_engine(GE_WHEEL);
    else { usb.config = 0; }
    host_audio_us(100000);
    for (i = 0; i < NTRK; i++) n += voices_on(i);
    CHECK(!n, "no sounding voices, including release tails, after 100 ms");
    CHECK(!midi_owners[0], "incoming ownership cleared");
    for (i = mo_r; i < mo_w; i++) {
        uint32_t pkt = midi_out_q[i % MQ];
        if (((pkt >> 8) & 0xF0u) == 0x80u) {
            off |= ((pkt >> 16) & 127u) == 60u;
            local |= ((pkt >> 16) & 127u) == local_note;
        }
    }
    if (event != 4) CHECK(off && local, "incoming and panel notes have outgoing MIDI note-offs");
    CHECK(!kb_chn[12], "held panel key retired without retrigger");
}
static void t_r19_stop(void) { r19_notes(0); }
static void t_r19_enter(void) { r19_notes(1); }
static void t_r19_leave(void) { r19_notes(2); }
static void t_r19_sound(void) { r19_notes(3); }
static void t_r19_usb(void) { r19_notes(4); }
static void t_r19_engine(void) { r19_notes(5); }

/* Round 20: the driver freezes config on unplug; only suspended changes. */
static void r20_usb_note(void)
{
    host_easy();                         /* activates cleanup even after leaving Easy */
    host_hold_switch(2100);
    usb.config = 1;
    usb.suspended = 0;
    host_audio_us(15000);
    midi_enqueue(0x643C9009u, 1);
    host_audio_us(50000);
    CHECK(sounding(0, 60) && midi_owners[0], "fixture: USB note gated in Advanced after Easy");
}
static void r20_usb_suspend(int wrap)
{
    uint32_t i, n = 0;
    r20_usb_note();
    CHECK(offsetof(__typeof__(gn), live_ms) == 1704u && offsetof(__typeof__(gn), mgen) == 640u,
          "round-19 state size and following offset unchanged (the UX rounds' state appended after it)");
    if (wrap) {
        host_us = 65500ull * 1000u;
        host_ticks = (uint32_t)host_us;
        fm1_ms = 65500u;
    }
    usb.suspended = 1;                    /* usb_poll's frozen-frame / unplug report */
    host_audio_us(490000);
    CHECK(usb.config == 1 && sounding(0, 60) && midi_owners[0],
          "no cleanup before the 500 ms qualification");
    host_audio_us(110000);
    for (i = 0; i < NTRK; i++) n += voices_on(i);
    CHECK(usb.config == 1 && !n && !midi_owners[0],
          "persistent driver suspension: all voices and ownership gone after timeout");
    /* A later TRS note must not be killed repeatedly by the same USB outage. */
    midi_enqueue(0x64409009u, 2);
    host_audio_us(700000);
    CHECK(sounding(0, 64), "one panic per continuous suspension");
}
static void t_r20_usb_suspend(void) { r20_usb_suspend(0); }
static void t_r20_usb_wrap(void) { r20_usb_suspend(1); }
static void t_r20_usb_resume(void)
{
    uint32_t i;
    r20_usb_note();
    for (i = 0; i < 2; i++) {
        usb.suspended = 1;
        host_audio_us(400000);
        CHECK(sounding(0, 60) && midi_owners[0], "short suspension keeps held note");
        usb.suspended = 0;
        host_audio_us(300000);
        CHECK(sounding(0, 60) && midi_owners[0], "resume cancels timeout; separate stalls do not accumulate");
    }
}
static void t_r20_bars_erased(void)
{
    uint32_t i, j, resurrected = 0;
    uint8_t erase[9] = {63, 0, 0, 0, 0, 0, ST_NOTE, 0, 100};
    host_easy_gnome();
    host_tap(B_SEQ);
    for (i = 0; i < 3; i++) host_turn(EN_K1, 1);
    app_step_set(31, 67);
    app_step_set(63, 72);
    CHECK(trk[0].step[63].n == 1, "fixture: tail note exists before explicit erase");
    host_request(ED_STEP_SET, erase, sizeof erase);
    CHECK(!trk[0].step[63].n, "app erased tail note");
    for (i = 0; i < 3; i++) host_turn(EN_K1, -1);
    CHECK(gn_bars(0) == 1, "erased pattern shortened");
    for (i = 0; i < 3; i++) host_turn(EN_K1, 1);
    for (i = 0; i < NSTEP; i++)
        for (j = 0; j < trk[0].step[i].n; j++)
            resurrected |= trk[0].step[i].note[j] == 72;
    CHECK(gn_bars(0) == 8 && !resurrected && !trk[0].step[63].n,
          "erased note never returns after shorten / lengthen");
    CHECK(trk[0].step[31].n == 1 && trk[0].step[31].note[0] == 67,
          "unerased hidden note returns");
}

/* Round 21: suspension is also host sleep, not evidence of physical detach. */
static uint32_t r21_note_age(uint32_t p, uint32_t note)
{
    uint32_t i;
    for (i = 0; i < NVOICE; i++)
        if (trk[p].v[i].active && trk[p].v[i].gate && trk[p].v[i].note == note)
            return trk[p].v[i].age;
    return 0;
}
static void r21_fixture(void)
{
    uint32_t p, i;
    host_easy();
    host_hold_switch(2100);
    for (p = 0; p < NTRK; p++) {
        song.sel = p;
        app_preset(GE_ANALOG, 0);
        app_set(P_VOICE, V_POLY);
        app_set(P_AMODE, 0);
        app_set(P_CHRD, 0);
        app_set(P_ATK, 0);
        app_set(P_SUS, 127);
        app_set(P_REL, 127);
        for (i = 0; i < NSTEP; i++) step_clear(&trk[p].step[i]);
    }
    song.sel = 0;
    usb.config = 1;
    usb.suspended = 0;
    host_wait_ms(100);
}
static void t_r21_suspend_local(void)
{
    uint32_t i, panel_age, trs_age, seq_age, tick;
    r21_fixture();
    trk[3].p[P_SLEN] = NSTEP;
    trk[3].p[P_SDIV] = GN_DIV8;
    put_note(3, 0, 72);
    for (i = 1; i < NSTEP; i++) trk[3].step[i].time = ST_TIE;
    host_tap(B_PLAY);
    host_key(7, 1);
    host_wait_ms(30);
    midi_enqueue(0x64409009u, 1);            /* USB C4 */
    midi_enqueue(0x64439009u, 2);            /* TRS G4, same channel and track */
    host_audio_us(50000);
    panel_age = r21_note_age(0, 60);
    trs_age = r21_note_age(0, 67);
    seq_age = r21_note_age(3, 72);
    tick = song.tick;
    CHECK(panel_age && trs_age && seq_age && sounding(0, 64), "fixture: panel, TRS, song and USB sound together");
    usb.suspended = 1;
    host_audio_us(3000000);
    CHECK(!sounding(0, 64), "sleep timeout releases USB-origin note");
    CHECK(r21_note_age(0, 60) == panel_age && r21_note_age(0, 67) == trs_age &&
          r21_note_age(3, 72) == seq_age, "three-second host sleep never cuts or retriggers local voices");
    CHECK(song.playing && song.tick > tick && kb_chn[7] && midi_notes[0][67],
          "song and local ownership continue during sleep");
    usb.suspended = 0;
    host_audio_us(300000);
    CHECK(song.playing && r21_note_age(0, 60) == panel_age && r21_note_age(0, 67) == trs_age &&
          r21_note_age(3, 72) == seq_age, "local playback continues through resume");
}
static void t_r21_suspend_shared(void)
{
    uint32_t age;
    r21_fixture();
    midi_enqueue(0x64409009u, 1);
    midi_enqueue(0x64409009u, 2);            /* same channel / pitch has two origins */
    host_audio_us(50000);
    age = r21_note_age(0, 64);
    CHECK(age, "fixture: shared pitch sounds");
    usb.suspended = 1;
    host_audio_us(3000000);
    CHECK(r21_note_age(0, 64) == age && midi_notes[0][64], "USB cleanup preserves shared TRS pitch");
    midi_enqueue(0x00408008u, 2);
    host_audio_us(10000);
    CHECK(!sounding(0, 64), "TRS release still ends the surviving ownership");
}

/* Round 23: the shared cleanup call must retain both distinct scopes. */
static void t_r23_usb_cleanup_scope(void)
{
    uint32_t i, n = 0, panel_age, trs_age;
    r21_fixture();
    host_key(7, 1);
    host_audio_us(15000);
    midi_enqueue(0x64409009u, 1);
    midi_enqueue(0x64439009u, 2);
    host_audio_us(50000);
    panel_age = r21_note_age(0, 60);
    trs_age = r21_note_age(0, 67);
    CHECK(panel_age && trs_age && sounding(0, 64), "fixture: panel, USB and TRS notes gated");
    usb.suspended = 1;
    host_audio_us(600000);
    CHECK(!sounding(0, 64) && r21_note_age(0, 60) == panel_age && r21_note_age(0, 67) == trs_age,
          "shared call: suspension releases USB only");
    usb.config = 0;                       /* verified loss after the suspension already fired */
    host_audio_us(100000);
    for (i = 0; i < NTRK; i++) n += voices_on(i);
    CHECK(!n && !midi_owners[0] && !kb_chn[7], "same shared call: config loss still releases every origin");
}

/* round 24 (finding F6): the app's PRESET of one of Easy's 49 sounds while Easy runs lands as Easy's pick of it:
 * Easy owns it, its settings and knobs are Easy's (the GN_KEPT claims too: put back on leaving), Easy's screen shows
 * the pick (or the peek list), and it is one of Felucca's loads in a row with the app's for SAVE held in Advanced.
 * Which sound: the one the track shows when it is that preset, else the one named for the preset; never the kit.
 * A command right behind the PRESET (no UI frame between) runs after the adoption. No new protocol command */
static uint32_t snd_named(const char *n)
{
    uint32_t i;
    for (i = 0; i < GN_NSOUNDS && !str_eq(GN_SOUNDS[i].name, n); i++)
        ;
    return i;
}

static void t_r24_adopt(void)
{
    music_t before, after;
    uint32_t bells = GN_GROUP[2], tine = snd_named("TINE PIANO"), vox = snd_named("VOX LEAD"), n, e, pr, ok = 1;
    /* every one of the 49 has a PRESET that lands on it, or on a sound of the same preset; the kit none */
    for (e = 0, n = 0; e < NENGINES; e++)
        for (pr = 0; pr < ENGINES[e]->npresets; pr++)
            n += (uint32_t)easy_preset(e, pr);
    for (pr = 0; pr < GN_NSOUNDS; pr++)
        ok &= (uint32_t)(pr == GN_SND_KIT ? !easy_preset(GN_SOUNDS[pr].engine, GN_SOUNDS[pr].preset)
                                         : easy_preset(GN_SOUNDS[pr].engine, GN_SOUNDS[pr].preset));
    CHECK(ok && n >= 40u && !easy_preset(GE_SAMPLE, 0) && !easy_preset(GE_PHASE, 3) && !easy_preset(GE_FM6, 4),
          "(each of Easy's sounds but the kit has a PRESET Easy adopts; SAMPLE PIANO, PHASE RESO, FM6 PAD are none)");
    /* the entry's Gnome shown on track 1: the app's VOICE VOX LEAD is the Gnome (he is what the track shows) */
    host_power_on();
    host_frame();
    host_hold_switch(2100);
    host_wait_ms(3100);
    CHECK(gn.on && gn.entry_put && song.sel == 0u, "(Easy, the entry's Gnome shown on track 1)");
    app_preset(GE_VOICE, 1);
    host_frame();
    CHECK(!gn.entry_put && gn_easy_of(&trk[0]) == GN_GROUP[0] && easy_whole_on(GN_GROUP[0], 0) && gn_view() == GV_PICK,
          "the app's VOX LEAD over the shown Gnome: the Gnome, whole, Easy's; the pick screen, not Felucca's");
    /* from another sound: the sound named for the preset */
    app_preset(GE_PHASE, 3);
    host_frame();
    CHECK(gn_easy_of(&trk[0]) == GN_NOSND && gn_picture() == GN_OTHER, "(PHASE RESO from the app: another sound)");
    app_preset(GE_VOICE, 1);
    host_frame();
    CHECK(gn_easy_of(&trk[0]) == vox && easy_whole_on(vox, 0) && gn_view() == GV_PEEK && gn_picture() == 0u,
          "VOICE VOX LEAD from another sound: Easy's Vox lead (named for it), whole; the peek list");
    app_preset(GE_FM6, 0);
    host_frame();
    CHECK(gn_easy_of(&trk[0]) == tine && easy_whole_on(tine, 0) && gn_picture() == 1u,
          "FM6 TINE EP: Easy's Tine piano (the Organ's), not the Piano picture's first sound");
    n = gn.amount[0][0];
    host_turn(EN_K1, 3);
    CHECK(gn.amount[0][0] == n + 3u && trk[0].p[gn_map(0, 0)->id] == gn.wrote[0][0] && !gn_fallback(0),
          "..its knobs are Easy's own map (not the fallback)");
    /* a command right behind it, no UI frame between: the adoption first, then the app's SET on Easy's sound */
    app_preset(GE_FM6, 1);
    app_set(P_E2, 20);
    CHECK(gn_easy_of(&trk[0]) == bells && trk[0].p[P_E2] == 20, "PRESET then SET at once: Easy's Bells, then the app's MLVL 20");
    host_frame();
    CHECK(gn_easy_of(&trk[0]) == bells && gn.wrote[0][0] == 20, "..still Easy's, the Bright bubble follows the app's value");
    /* never the kit: SAMPLE preset 0 is PIANO */
    host_tap(B_LFO);
    host_turn(EN_ALGO, (int32_t)GN_KIT + 1);
    CHECK(gn_easy_of(&trk[3]) == GN_SND_KIT, "(the drums on track 4)");
    app_preset(GE_SAMPLE, 0);
    host_frame();
    CHECK(gn_easy_of(&trk[3]) == GN_NOSND && trk[3].p[P_E0] != GN_KIT_SET, "SAMPLE preset 0 from the app: the app's PIANO, not the kit");
    app_preset(GE_SAMPLE, 3);
    host_frame();
    CHECK(gn_easy_of(&trk[3]) == snd_named("SAX") && easy_whole_on(snd_named("SAX"), 3), "SAMPLE SAX on track 4: Easy's Sax there");
    /* Easy leaves: the kept settings Easy's adoption wrote go back; then Felucca's undo gives back the sound from
     * before the app's load (the app's load and Easy's one load in a row) */
    host_power_on();
    host_wait_ms(60);
    app_preset(GE_PHASE, 3);                        /* Advanced: track 1 PHASE RESO, its own transpose and level */
    host_wait_ms(1600);                             /* (then its own settings: no longer part of the load) */
    app_set(P_TRANS, 5);
    app_set(P_LEVEL, 77);
    host_wait_ms(1600);
    music_get(&before);
    host_hold_switch(2100);
    host_wait_ms(3100);
    CHECK(gn.on && gn.entry_put, "(Easy, the Gnome only shown)");
    app_preset(GE_FM6, 1);                          /* the app picks GLASS BELL on the phone */
    host_frame();
    CHECK(gn_easy_of(&trk[0]) == bells && easy_whole_on(bells, 0) && trk[0].p[P_TRANS] == 0 && trk[0].p[P_LEVEL] != 77,
          "the phone's Bell from Advanced's PHASE: Easy's Bells, Easy's transpose and level");
    host_hold_switch(2100);
    CHECK(!gn.on && trk[0].p[P_TRANS] == 5 && trk[0].p[P_LEVEL] == 77, "..leaving: transpose and level back (the GN_KEPT claims)");
    host_wait_ms(100);
    host_btn(B_SAVE, 1);                            /* Felucca's undo */
    host_wait_ms(800);
    host_btn(B_SAVE, 0);
    host_wait_ms(60);
    music_get(&after);
    if (!music_same(&before, &after, 0))
        music_diff(&before, &after);
    CHECK(music_same(&before, &after, 0), "..SAVE held in Advanced: the whole track as before the app's load");
    /* Easy off: the app's PRESET is the app's, nothing adopted */
    app_preset(GE_FM6, 1);
    host_frame();
    CHECK(!gn.on && !gn.adopt && gn_easy_of(&trk[0]) == GN_NOSND, "Easy off: the app's PRESET is only the app's");
}

/* round 24: the loop Easy wrote is saved by itself once stopped and settled (FORK.md, round 24). The synth
 * switched off and on (the flash kept, the RAM as at power-on, the slots and the settings record read back as
 * persist_boot reads them) gives the loop back */
static uint8_t r24_nor[sizeof host_nor];
static void r24_power_cycle(void)
{
    persist_t p;
    int n;
    memcpy(r24_nor, host_nor, sizeof r24_nor);
    host_power_on();
    memcpy(host_nor, r24_nor, sizeof r24_nor);
    memset(&persist_saved, 0, sizeof persist_saved);
    persist_pending = 0;
    n = st_load(OBJ_SETTINGS, &p, sizeof p);
    if (settings_import(&p, n))
        persist_saved = p;
    for (n = 0; n < 4; n++)
        if (!proj_import(&proj_scratch, &proj_slot[n], sizeof(project_store_t)))
            proj_fetch((uint32_t)n);
    settings_init();
    panel_init();
    palette_set(settings.palette);
}

static uint32_t r24_seq(uint32_t slot)              /* the slot's commit sequence on flash; 0: none */
{
    uint32_t seq = 0;
    uint64_t fp;
    return gn_slot_now(slot, &seq, &fp) ? seq : 0u;
}

static int r24_slot_is_now(uint32_t slot)          /* the slot holds the music as it is now (steps of every track) */
{
    uint32_t t;
    if (!project_used(slot))
        return 0;
    for (t = 0; t < NTRK; t++)
        if (memcmp(proj_scratch.t[t].step, trk[t].step, sizeof trk[t].step))
            return 0;
    return 1;
}

static void t_r24_autosave(void)
{
    step_t rec[NSTEP];
    uint32_t i, seq, used = 0;
    host_start_easy = 1;                            /* (the slot's ownership rides in the settings record) */
    host_power_on();
    host_frame();
    host_wait_ms(3100);
    CHECK(gn.on && !gn.auto_dirty, "(Easy, as the first power-on starts; nothing to save)");
    /* playing and stopping, a pick: no write of the loop, nothing saved */
    host_turn(EN_ALGO, 2);
    host_tap(B_PLAY);
    host_wait_ms(1500);
    host_tap(B_PLAY);
    host_wait_ms(6000);
    for (i = 0; i < 4u; i++)
        used += (uint32_t)project_used(i);
    CHECK(!used && !gn.auto_dirty, "play, stop, a pick: Easy wrote no loop, no slot written");
    /* a recording while playing; the loop stops: saved 4 s after, not before, silently, into the first empty slot */
    easy_record();
    key(7, 100);
    key(9, 100);
    host_tap(B_REC);
    CHECK(gn.auto_dirty, "a recording that wrote: the loop unsaved");
    host_wait_ms(2000);
    CHECK(!project_used(0), "..while the loop plays: no save (a flash erase silences the audio)");
    host_tap(B_PLAY);
    host_wait_ms(3000);
    CHECK(!song.playing && !project_used(0), "..stopped, under 4 s: not yet");
    host_wait_ms(1500);
    CHECK(r24_slot_is_now(0) && gn_song_owned() == 0u && gn.auto_dirty == GN_LOOP_SAVED && gn_view() != GV_SAVED,
          "stopped and settled: the loop in the first empty slot (Easy's now), nothing on the screen but the header's mark (round 25)");
    memcpy(rec, trk[0].step, sizeof rec);
    seq = r24_seq(0);
    /* step entry while stopped: each key a write; not saved while keys come, nor within 10 s of the last save */
    host_tap(B_SEQ);
    host_tap(B_SEQ);
    host_tap(B_SEQ);
    CHECK(gn_view() == GV_RECORD, "(the record screen, stopped)");
    for (i = 0; i < 4u; i++) {
        key(11, 60);
        host_wait_ms(1500);
    }
    CHECK(gn.auto_dirty && r24_seq(0) == seq, "step entry, a key every 1.6 s: no save under the child's fingers");
    host_key(12, 1);                                /* a key held: never under it */
    host_wait_ms(6000);
    CHECK(r24_seq(0) == seq, "..a key held 6 s: still not");
    host_key(12, 0);
    host_wait_ms(4200);
    CHECK(gn.auto_dirty == GN_LOOP_SAVED && r24_seq(0) != seq && r24_slot_is_now(0) && gn_song_owned() == 0u,
          "let go, 4 s: saved into Easy's own slot again (the same slot)");
    seq = r24_seq(0);
    host_tap(B_HOME);
    host_turn(EN_K1, 0);
    /* the rate limit: a write right after an autosave waits for 10 s from it */
    gn_cursor_note(0, 1);
    host_wait_ms(4500);
    CHECK(gn.auto_dirty && r24_seq(0) == seq, "a write 0 s after an autosave: not saved 4.5 s on (10 s apart)");
    host_wait_ms(6000);
    CHECK(gn.auto_dirty == GN_LOOP_SAVED && r24_seq(0) != seq, "..saved once 10 s have passed");
    memcpy(rec, trk[0].step, sizeof rec);
    /* the power is cut (project RAM does not survive): the slot gives the loop back */
    r24_power_cycle();
    host_frame();
    host_wait_ms(3100);
    CHECK(gn.on && gn_song_owned() == 0u, "off and on again: Easy, its slot still its own");
    project_load(0);
    CHECK(!memcmp(trk[0].step, rec, sizeof rec), "..the slot loaded: the loop as it was before the power went");
    /* the music replaced by the app before the autosave: nothing of Easy's to save */
    easy_record();
    key(14, 100);
    host_tap(B_REC);
    host_tap(B_PLAY);
    seq = r24_seq(0);
    app_project(0, 0);                              /* the app loads slot 1 (Easy's) */
    host_wait_ms(5000);
    CHECK(!gn.auto_dirty && r24_seq(0) == seq && !project_used(1), "the app loads a project before the autosave: nothing saved");
    /* never over someone else's: every slot someone else's: FULL once, the slots untouched, no warning again */
    for (i = 0; i < 4u; i++)
        app_project(1, i);                          /* the app saves its own songs in all four */
    host_wait_ms(GN_SAVED_MS + 100u);
    CHECK(gn_song_owned() == GN_NO_SLOT, "(every slot the app's)");
    {
        project_store_t was[4];
        memcpy(was, proj_slot, sizeof was);
        easy_record();
        key(16, 100);
        host_tap(B_REC);
        host_tap(B_PLAY);
        host_wait_ms(4500);
        CHECK(gn_view() == GV_SAVED && gn.saved_mine == 2u && !memcmp(was, proj_slot, sizeof was) &&
              slot_same(3, &was[3]), "no slot Easy may write: FULL shown, nothing written");
        host_key(7, 1);
        host_wait_ms(60);
        host_key(7, 0);
        host_wait_ms(15000);
        CHECK(!gn.saved_t && !gn.auto_dirty && !memcmp(was, proj_slot, sizeof was), "..once: no warning again until Easy writes again");
    }
    /* SAVE tapped clears it too; leaving Easy drops the mark */
    easy_record();
    key(18, 100);
    host_tap(B_REC);
    host_tap(B_PLAY);
    CHECK(gn.auto_dirty, "(a write)");
    host_hold_switch(2100);
    CHECK(!gn.on && !gn.auto_dirty, "leaving Easy: the mark dropped (Advanced's music is Advanced's to save)");
    host_start_easy = 0;
}

/* ------------------------------------------------------- the demo grooves (gn_grooves.c) --- */
#if GN_DEMO_GROOVES
/* each groove's sounds by name, track 1 to 4 (a lead, chords, a bass, Easy's kit): GN_SOUNDS may be renumbered */
static const char *const GV_WANT[5][NTRK] = {
    {"GLASS BELL", "PIANO", "FM BASS", "DRUM MACHINE"},
    {"PLUCK", "ORGAN", "SQUARE BASS", "DRUM MACHINE"},
    {"SOFT FLUTE", "NYLON GUITAR", "SUB BASS", "DRUM MACHINE"},
    {"VOX LEAD", "SHIMMER", "FAT BASS", "DRUM MACHINE"},
    {"KALIMBA", "CHOIR", "WHALE", "DRUM MACHINE"},
};

static int gv_kit_note(uint32_t n)                   /* one of Easy's kit keys (the pads, the black keys) */
{
    uint32_t i;
    for (i = 0; i < 16u; i++)
        if (GN_PADS[i] == n)
            return 1;
    for (i = 0; i < 11u; i++)
        if (GN_BLACK_PADS[i] == n)
            return 1;
    return 0;
}

static int gv_steps_same(uint32_t g)                 /* the four tracks hold groove g's steps and settings */
{
    static step_t want[NSTEP];
    uint32_t p;
    for (p = 0; p < NTRK; p++) {
        gn_gv_steps(&GN_GROOVES[g], p, want);
        if (memcmp(want, trk[p].step, sizeof want) || trk[p].p[P_SLEN] != GN_GROOVES[g].len ||
            trk[p].p[P_SDIV] != GN_GROOVES[g].div || trk[p].p[P_SSWING] != GN_GROOVES[g].swing ||
            trk[p].p[P_SGATE] != GN_GROOVES[g].t[p].gate) {
            uint32_t i;
            for (i = 0; i < NSTEP && !memcmp(&want[i], &trk[p].step[i], sizeof want[i]); i++)
                ;
            printf("    (groove %u, track %u: LEN %d DIV %d SWING %d GATE %d; the first step that differs %u", (unsigned)g + 1u,
                   (unsigned)p + 1u, trk[p].p[P_SLEN], trk[p].p[P_SDIV], trk[p].p[P_SSWING], trk[p].p[P_SGATE],
                   (unsigned)i);
            if (i < NSTEP)
                printf(": n %u time %u vel %u note %u, wanted n %u time %u vel %u note %u", trk[p].step[i].n,
                       trk[p].step[i].time, trk[p].step[i].vel, trk[p].step[i].note[0], want[i].n, want[i].time,
                       want[i].vel, want[i].note[0]);
            printf(")\n");
            return 0;
        }
    }
    return 1;
}

/* the grooves as written: five, each named in 12 letters at most (a project's name), every track Easy's sound by
 * name, the kit on track 4 and only on its keys, the bass low, at most four notes a step, every event inside the
 * loop (its own parse, independent of gn_gv_steps), velocity that moves, the lengths 1, 2, 4 and 8 bars, the bytes */
static void t_grooves_defined(void)
{
    uint32_t g, p, i, bars_seen = 0, bytes = (uint32_t)sizeof GN_GROOVES;
    CHECK(GN_NGROOVES == 5u, "five grooves");
    for (g = 0; g < GN_NGROOVES; g++) {
        const gn_groove_t *G = &GN_GROOVES[g];
        uint32_t spb = G->div == GN_DIV8 ? 8u : G->div == GN_DIV16 ? 16u : 0u, nl = str_len(G->name);
        CHECK(nl >= 1u && nl <= PROJ_NAME_LEN && nl <= 15u, "a groove's name: 1 to 12 letters (15 the label limit)");
        for (i = 0; i < nl; i++)
            CHECK(G->name[i] >= 32 && G->name[i] <= 126 && !(G->name[i] >= 'a' && G->name[i] <= 'z'),
                  "..upper-case ASCII, as a project's name");
        CHECK(G->bpm >= 40u && G->bpm <= 240u, "the tempo inside SELECT's 40 to 240");
        CHECK(spb && G->len >= spb && G->len <= NSTEP && G->len % spb == 0u, "LEN whole bars of Easy's grid");
        CHECK(G->swing <= 60u && G->swing % 5u == 0u, "SWING as Easy's knob sets it (5 % a detent, up to 60)");
        if (spb)
            bars_seen |= 1u << (G->len / spb);
        for (p = 0; p < NTRK; p++) {
            const gn_gvtrk_t *T = &G->t[p];
            uint32_t a = 0, last = 0, first = 1, vels = 0, notes = 0, k;
            int pads = T->snd < GN_NSOUNDS && (GN_SOUNDS[T->snd].flags & GNF_PADS);
            CHECK(T->snd < GN_NSOUNDS && str_eq(GN_SOUNDS[T->snd].name, GV_WANT[g][p]), GV_WANT[g][p]);
            CHECK(pads == (p == NTRK - 1u) && (p != NTRK - 1u || GN_SOUNDS[T->snd].engine == GE_DRUM),
                  "the drum machine (DRUM engine, Easy's pads) on track 4, a melodic sound on 1 to 3");
            CHECK(T->gate >= 1u && T->gate <= 127u && T->tile <= G->len && T->level >= -16 && T->level <= 16,
                  "GATE, the tile, the level in range");
            for (k = 0; k < 4u; k++)
                CHECK(((T->knobs >> (4u * k)) & 15u) <= 15u, "a knob's amount 0..14 or the rest");
            bytes += T->n;
            while (a < T->n) {
                uint32_t e = T->ev[a++], at = e & 63u, cnt = 0, b, len = 1, hat = 0;
                CHECK(first || at > last, "the events go up the steps");
                CHECK(at < (T->tile ? T->tile : G->len), "an event inside the loop (its tile)");
                first = 0;
                last = at;
                vels |= 1u << (e >> 6);
                do {
                    CHECK(a < T->n, "an event's notes inside its bytes");
                    b = a < T->n ? T->ev[a++] : 0u;
                    cnt++;
                    if (pads) {
                        uint32_t nb = b & 127u;
                        CHECK(gv_kit_note(nb), "the kit: only notes of Easy's kit keys");
                        hat += nb == 42u || nb == 44u || nb == 54u || nb == 69u || nb == 70u || nb == 73u ||
                               nb == 74u || nb == 46u;
                    }
                    else if (p == 2u)
                        CHECK((b & 127u) >= 28u && (b & 127u) <= 47u, "the bass in the low octave (E1 .. B2)");
                    else
                        CHECK((b & 127u) >= 55u && (b & 127u) <= 96u, "the lead and chords in the middle");
                    notes++;
                } while (b & 0x80u);
                CHECK(cnt <= 4u, "at most four notes a step");
                CHECK(hat <= 1u, "one hat a step (the DRUM engine's hat lane is mono, a closed hat chokes the open)");
                if (!pads) {
                    CHECK(a < T->n, "a melodic event's length inside its bytes");
                    len = a < T->n ? T->ev[a++] : 1u;
                }
                CHECK(len >= 1u && at + len <= G->len, "a held note ends inside the loop");
            }
            CHECK(notes >= 4u, "every track plays");
            if (pads)
                CHECK(vels & (vels - 1u), "the kit's velocity moves (two classes at least)");
        }
    }
    for (g = 0; g < GN_NGROOVES; g++)
        CHECK(GN_GROOVES[g].kit[0] <= 127u && GN_GROOVES[g].kit[1] <= 127u && GN_GROOVES[g].kit[2] <= 127u &&
              GN_GROOVES[g].kit[3] <= 1u, "the drum machine's TONE DECY SNAP 0..127, KICK PUNCH or ROUND");
    CHECK(bars_seen == ((1u << 1) | (1u << 2) | (1u << 4) | (1u << 8)), "the lengths: 1, 2, 4 and 8 bars");
    printf("    %u grooves, %u bytes of tables and events\n", (unsigned)GN_NGROOVES, (unsigned)bytes);
    CHECK(bytes <= 1536u, "the grooves' data at most 1,536 bytes (the image budget)");
}

static void gv_loop_page(void)
{
    host_tap(B_SEQ);
    host_wait_ms(30);
}

/* a groove from the loop page: PRESETS loads it on all four tracks as Easy's own picks (Easy owns each sound, the
 * knobs and level Easy's), its steps, tempo, from the top; the list shows 2 s; the next and the one before; the
 * project slots untouched; elsewhere PRESETS is the sound inside the picture */
static void t_grooves_load(void)
{
    project_store_t was[4];
    uint32_t p, k, ok = 1;
    host_easy();
    for (p = 0; p < NTRK; p++)
        CHECK(seq_is_empty(&trk[p]), "(power-on: every pattern empty)");
    memcpy(was, proj_slot, sizeof was);
    host_turn(EN_PRESET, 1);                         /* (the play page: the sound inside the picture, no groove) */
    CHECK(!(gn.grv & GN_GV_NUM) && seq_is_empty(&trk[1]), "PRESETS off the loop page: no groove");
    gv_loop_page();
    CHECK(gn.page == GP_LOOP, "(SEQ: the loop page)");
    host_turn(EN_PRESET, 1);
    host_wait_ms(30);
    CHECK((gn.grv & GN_GV_NUM) == 1u && gv_steps_same(0), "PRESETS on the loop page: groove 1 on all four tracks");
    for (p = 0; p < NTRK; p++) {
        const gn_gvtrk_t *T = &GN_GROOVES[0].t[p];
        ok &= gn_easy_of(&trk[p]) == T->snd && gn.own[p] == T->snd + 1u;
        for (k = 0; k < 4u; k++)
            if (((T->knobs >> (4u * k)) & 15u) != 15u)
                ok &= gn.amount[p][k] == ((T->knobs >> (4u * k)) & 15u);
    }
    CHECK(ok, "..each track's sound Easy's own pick, its knobs where the groove sets them");
    CHECK(song.g[G_BPM] == 90 && song.playing, "..its tempo, and it plays");
    CHECK(!memcmp(was, proj_slot, sizeof was), "..no project slot written");
    ui_draw();
    CHECK(gn_view() == GV_PEEK && (gs.grv & GN_GV_LIST) && (gs.grv & GN_GV_NUM) == 1u && !(gs.grv & GN_GV_NO),
          "..the grooves' list shows, groove 1 picked");
    host_wait_ms(GN_PEEK_MS + 100u);
    CHECK(gn_view() == GV_LOOP, "..2 s later the loop page again");
    host_turn(EN_PRESET, 1);
    host_wait_ms(30);
    CHECK((gn.grv & GN_GV_NUM) == 2u && gv_steps_same(1) && song.g[G_BPM] == 124 && song.playing,
          "the next detent: groove 2 over groove 1 (untouched: nothing lost), from the top");
    host_turn(EN_PRESET, -2);
    host_wait_ms(30);
    CHECK((gn.grv & GN_GV_NUM) == GN_NGROOVES && gv_steps_same(GN_NGROOVES - 1u) && trk[0].p[P_SDIV] == GN_DIV8,
          "two back: round to the last groove (8 bars of 1/8)");
    host_turn(EN_PRESET, 1);
    host_wait_ms(30);
    CHECK((gn.grv & GN_GV_NUM) == 1u && gv_steps_same(0), "..and on round to the first");
    CHECK(!memcmp(was, proj_slot, sizeof was), "no project slot written by any of it");
    host_tap(B_HOME);
    host_turn(EN_PRESET, 1);
    CHECK((gn.grv & GN_GV_NUM) == 1u && gv_steps_same(0) && gn_view() == GV_PEEK && !(gn.grv & GN_GV_LIST),
          "PRESETS on the play page: the sound inside the picture again (the peek list), the groove's steps kept");
}

/* the player's music is never lost to a groove: a step of the player's refuses the next (SAVE FIRST, nothing
 * changes); saved, the groove loads, the saved song stays in its slot, and the groove saved goes to an empty slot */
static void t_grooves_owner(void)
{
    char b[PROJ_NAME_LEN + 1u];
    step_t mine[NTRK][NSTEP];
    project_store_t saved;
    uint32_t p, bpm;
    host_start_easy = 1;                             /* (the song's slot rides in the settings record) */
    host_power_on();
    host_frame();
    host_wait_ms(3100);
    CHECK(gn.on, "(Easy, as the first power-on starts)");
    some_steps(0, 60);                               /* the player's own pattern on track 1 */
    gv_loop_page();
    for (p = 0; p < NTRK; p++)
        memcpy(mine[p], trk[p].step, sizeof mine[p]);
    bpm = (uint32_t)song.g[G_BPM];
    host_turn(EN_PRESET, 1);
    host_wait_ms(30);
    for (p = 0; p < NTRK; p++)
        CHECK(!memcmp(mine[p], trk[p].step, sizeof mine[p]), "a pattern of the player's: no groove loads over it");
    CHECK((gn.grv & GN_GV_NO) && !(gn.grv & GN_GV_NUM) && song.g[G_BPM] == (int16_t)bpm && !song.playing,
          "..nothing changes, not the tempo, not the transport");
    ui_draw();
    CHECK(gn_view() == GV_PEEK && (gs.grv & GN_GV_NO) && str_eq(gs_gv_hint(), "SAVE FIRST (SAVE)"),
          "..the list says SAVE FIRST (SAVE)");
    host_wait_ms(GN_PEEK_MS + 100u);
    host_tap(B_SAVE);
    host_wait_ms(100);
    CHECK(gn.saved_mine == 1u && project_name(0, b) && str_eq(b, GN_NAME " SONG") && gn_song_owned() == 0u,
          "SAVE: the song into slot 1, Easy's");
    memcpy(&saved, &proj_slot[0], sizeof saved);
    host_wait_ms(GN_SAVED_MS + 100u);
    CHECK(gn.page == GP_LOOP, "(still the loop page)");
    host_turn(EN_PRESET, 1);
    host_wait_ms(30);
    CHECK((gn.grv & GN_GV_NUM) == 1u && !(gn.grv & GN_GV_NO) && gv_steps_same(0),
          "saved: now the groove loads");
    CHECK(slot_same(0, &saved) && !project_used(1) && !project_used(2) && !project_used(3),
          "..the saved song stays in its slot, the other slots empty");
    CHECK(gn_song_owned() == GN_NO_SLOT, "..and is no longer Easy's song: a groove is a new one");
    some_steps(0, 64);                               /* the player changes the groove */
    host_turn(EN_PRESET, 1);
    host_wait_ms(30);
    CHECK((gn.grv & GN_GV_NO) && (gn.grv & GN_GV_NUM) == 1u, "a groove changed by the player: the next refused");
    host_wait_ms(GN_PEEK_MS + 100u);
    host_tap(B_SAVE);                                /* (playing: SAVE stops the loop first) */
    host_wait_ms(200);
    CHECK(gn.saved_mine == 1u && slot_same(0, &saved) && project_name(1, b) && str_eq(b, GN_NAME " SONG") &&
          gn_song_owned() == 1u, "SAVE: the changed groove into the first empty slot (2), slot 1 as saved");
    host_wait_ms(GN_SAVED_MS + 100u);
    host_turn(EN_PRESET, 1);
    host_wait_ms(30);
    CHECK((gn.grv & GN_GV_NUM) == 2u && gv_steps_same(1) && slot_same(0, &saved) && project_used(1),
          "saved again: the next groove loads, both songs kept");
}

/* the music replaced: armed, a take, Easy's undo copy end; a song plays: no groove; an external clock: the tempo
 * and the transport are the clock's; leaving puts every kept level back, as after any pick */
static void t_grooves_rules(void)
{
    int16_t lvl[NTRK];
    uint32_t p, ok = 1;
    host_easy();
    for (p = 0; p < NTRK; p++)
        lvl[p] = trk[p].p[P_LEVEL];
    gv_loop_page();
    host_tap(B_REC);                                 /* armed */
    CHECK(gn.arm, "(REC: armed)");
    host_turn(EN_PRESET, 1);
    host_wait_ms(30);
    CHECK((gn.grv & GN_GV_NUM) == 1u && !gn.arm && !song.rec && !gn.undo_of, "a groove ends the arming, no undo copy");
    host_turn(EN_PRESET, 1);
    host_wait_ms(30);
    CHECK(song.playing, "(groove 2 plays)");
    song.g[G_CLOCK] = 1;                             /* an external clock: MIDI clock in */
    transport_req = 2;
    host_wait_ms(30);
    song.g[G_BPM] = 100;
    host_turn(EN_PRESET, 1);
    host_wait_ms(30);
    CHECK((gn.grv & GN_GV_NUM) == 3u && gv_steps_same(2) && song.g[G_BPM] == 100 && !song.playing,
          "an external clock: the groove loads, the tempo and the transport the clock's");
    song.g[G_CLOCK] = 0;
    host_hold_switch(2100);                          /* leaving */
    host_wait_ms(300);
    CHECK(!gn.on, "(Advanced)");
    for (p = 0; p < NTRK; p++)
        ok &= trk[p].p[P_LEVEL] == lvl[p];
    CHECK(ok, "leaving: every level the groove set put back (Easy's kept writes)");
    CHECK(gv_steps_same(2), "..the groove's steps stay (the music is the player's now)");
}
#endif
/* round 25 (a report on V1.1, 2026-10-11): "recorded a beat in Easy, SAVE said SAVED, off and on: the loop was gone".
 * His exact sequence on the firmware's own code: Easy (the first power-on), the drums on track 4, a recorded beat,
 * the loop stopped, SAVE tapped (SAVED), a wait, the power cut (the flash kept, the RAM as at power-on, the slots
 * and the settings record read back as persist_boot reads them), the power on again. Prints what the slot and the
 * tracks hold; the finding: the slot and Easy's record of it are on flash (hypothesis b is not it), and power-on
 * started Felucca's default tracks (main.c felucca_init, TRK_DEF): nothing loaded the song (hypothesis a). Easy
 * starts with its last saved song loaded, stopped */
static void t_r25_power_cycle(void)
{
    step_t beat[NSTEP];
    uint32_t i, hits = 0, owned, used = 0, same;
    host_start_easy = 1;
    host_power_on();
    host_frame();
    host_wait_ms(3100);
    CHECK(gn.on, "(Easy, as the first power-on starts)");
    host_tap(B_LFO);                                /* track 4 */
    host_turn(EN_ALGO, (int32_t)GN_KIT + 1);        /* the drums */
    host_wait_ms(GN_PICK_MS + 100u);
    CHECK(gn_easy_of(&trk[3]) == GN_SND_KIT, "(the drums on track 4)");
    easy_record();
    for (i = 0; i < 6u; i++) {                      /* a beat: kick, snare, hats */
        key(i & 1u ? 4u : 0u, 60);
        host_wait_ms(120);
    }
    host_tap(B_REC);
    host_tap(B_PLAY);
    host_wait_ms(200);
    for (i = 0; i < NSTEP; i++)
        hits += gn_step_used(&trk[3].step[i]);
    CHECK(!song.playing && hits > 0u, "(a beat recorded on track 4, the loop stopped)");
    memcpy(beat, trk[3].step, sizeof beat);
    host_tap(B_SAVE);
    host_wait_ms(100);
    CHECK(gn_view() == GV_SAVED && gn.saved_mine == 1u, "SAVE: the Gnome says SAVED (read back from flash)");
    host_wait_ms(GN_SAVED_MS + 1000u);
    ui_draw();
    CHECK(gn_view() == GV_HOME && gs.mark && gn.auto_dirty == GN_LOOP_SAVED,   /* (gn/ux: Easy opens on Home, UX-1) */
          "(3) the header's mark: the loop on the tracks is the saved song");
    r24_power_cycle();                              /* off, and on */
    host_frame();
    host_wait_ms(3100);
    owned = gn_song_owned();
    for (i = 0; i < 4u; i++)
        used += (uint32_t)project_used(i);
    same = !memcmp(trk[3].step, beat, sizeof beat);
    printf("    after the power cycle: Easy %s, %u slot(s) used, Easy's slot %d (flash), track 4 %s the beat\n",
           gn.on ? "on" : "off", used, owned == GN_NO_SLOT ? -1 : (int)owned, same ? "holds" : "does NOT hold");
    if (owned != GN_NO_SLOT) {
        project_t q;
        CHECK(proj_import(&q, &proj_slot[owned], sizeof(project_store_t)) && !memcmp(q.t[3].step, beat, sizeof beat),
              "the slot on flash holds the beat (the save reached flash: hypothesis b ruled out)");
    }
    CHECK(owned != GN_NO_SLOT, "Easy's record of its slot survived the power cycle");
    CHECK(gn.on && same && !song.playing, "power on in Easy: the last saved song loaded, the beat there, not playing");
    CHECK(gn_easy_of(&trk[3]) == GN_SND_KIT, "..its drums Easy's drums again (the pads, the knobs)");
    CHECK(!gn.entry_put, "..no entry Gnome over the song's track 1");
    ui_draw();
    CHECK(gs.mark && gn.auto_dirty == GN_LOOP_SAVED, "(3) ..and the header's mark: the saved song");
    key(0, 60);                                     /* a pad (playing is no write) */
    host_frame();
    ui_draw();
    CHECK(gs.mark, "..a pad played: still the saved song");
    easy_record();                                  /* another hit recorded: the mark goes, the autosave comes */
    key(2, 60);
    host_tap(B_REC);
    host_tap(B_PLAY);
    host_frame();
    ui_draw();
    CHECK(!gs.mark && gn.auto_dirty == GN_LOOP_DIRTY, "(3) a recording that wrote: no mark (not saved yet)");
    host_wait_ms(4500);
    ui_draw();
    CHECK(gs.mark && gn_song_owned() == owned, "..stopped and settled: autosaved into the same slot, the mark back");
    /* the synth left in Advanced, Easy's autosave the last save: round 27 (F15) starts where that save was made,
     * Easy, the song loaded (round 25 started Felucca's power-on here, nothing loaded) */
    memcpy(beat, trk[3].step, sizeof beat);
    host_hold_switch(2100);
    CHECK(!gn.on, "(Advanced)");
    host_wait_ms(500);
    settings_poll();
    r24_power_cycle();
    host_frame();
    host_wait_ms(500);
    CHECK(gn.on && !memcmp(trk[3].step, beat, sizeof beat) && proj_cur == owned && !song.playing,
          "round 27: off in Advanced after Easy's autosave, on again: Easy, the last save loaded, stopped");
    host_start_easy = 0;
}

/* round 25, the follow-up (the report: "my beat is in no slot after the power cycle"): the SAVE path traced on
 * the firmware's own storage code over the simulated NOR, in the ways the beat may have been made: (0) Easy's kit
 * (SAMPLE PERC), (1) the drum machine (the DRUM engine, hits in lanes), (2) a demo groove loaded, then a beat
 * recorded over it; each with SAVE tapped while the loop still plays (it stops, then saves) and the power cut 3 s
 * after SAVED. After the power-on the slot is read as Advanced reads its slot list (proj_slot filled from flash by
 * persist_boot, project_used, the name) and loaded as Advanced loads it (project_load): the beat must be there */
static void r25_variant(uint32_t how)
{
    step_t beat[NSTEP];
    uint32_t i, slot, used = 0, hits = 0;
    char nm[PROJ_NAME_LEN + 1u];
    host_start_easy = 1;
    host_power_on();
    host_frame();
    host_wait_ms(3100);
    if (how == 2u) {                                /* a groove first (the loop page, PRESETS) */
        host_tap(B_SEQ);
        host_turn(EN_PRESET, 1);
        host_wait_ms(GN_PEEK_MS + 200u);
        host_tap(B_PLAY);                           /* (it plays: stopped) */
        host_tap(B_HOME);
    }
    host_tap(B_LFO);
    if (how == 1u) {
        host_turn(EN_ALGO, (int32_t)GN_KIT + 1);
        host_turn(EN_PRESET, 1);                    /* the drum machine */
    } else if (how == 0u) {
        host_turn(EN_ALGO, (int32_t)GN_KIT + 1);
    }
    host_wait_ms(GN_PEEK_MS + 200u);
    easy_record();
    for (i = 0; i < 6u; i++) {
        key(i & 1u ? 4u : 2u, 60);
        host_wait_ms(110);
    }
    host_tap(B_REC);
    host_wait_ms(300);
    memcpy(beat, trk[3].step, sizeof beat);
    for (i = 0; i < NSTEP; i++)
        hits += gn_step_used(&beat[i]);
    host_tap(B_SAVE);                               /* while it plays: it stops, then saves */
    for (i = 0; i < 40u && gn.saved_t == 0u; i++)
        host_frame();
    printf("    variant %u (track 4 engine %u, %u steps used): SAVE -> %s\n", how, trk[3].eng_req, hits,
           gn.saved_mine == 1u ? "SAVED" : gn.saved_mine == 2u ? "FULL" : "NOT SAVED / warning");
    CHECK(hits > 0u && gn.saved_mine == 1u && !song.playing, "(a beat, SAVE while playing: stopped, SAVED)");
    CHECK(!memcmp(trk[3].step, beat, sizeof beat), "..the beat as recorded when SAVED shows");
    host_wait_ms(3000);                             /* the power cut 3 s after SAVED */
    r24_power_cycle();
    for (i = 0; i < 4u; i++)
        used += (uint32_t)project_used(i);
    slot = GN_NO_SLOT;
    for (i = 0; i < 4u && slot == GN_NO_SLOT; i++) {
        project_t q;
        if (proj_import(&q, &proj_slot[i], sizeof(project_store_t)) && !memcmp(q.t[3].step, beat, sizeof beat))
            slot = i;
    }
    printf("      after the power cycle: %u slot(s) in the list, the beat in slot %d%s\n", used,
           slot == GN_NO_SLOT ? -1 : (int)slot + 1, slot != GN_NO_SLOT && project_name(slot, nm) ? "" : "");
    CHECK(slot != GN_NO_SLOT && project_name(slot, nm) && str_eq(nm, GN_NAME " SONG"),
          "the beat is on flash in a slot of Advanced's list, named GNOMAD SONG");
    host_frame();                                   /* (Easy starts: round 25 loads it) */
    host_wait_ms(3100);
    CHECK(gn.on && !memcmp(trk[3].step, beat, sizeof beat) && !song.playing, "Easy starts with it, stopped");
    host_hold_switch(2100);                          /* Advanced: the slot loaded as Felucca loads it */
    memset(trk[3].step, 0, sizeof trk[3].step);
    if (slot != GN_NO_SLOT)
        project_load(slot);
    CHECK(!gn.on && !memcmp(trk[3].step, beat, sizeof beat), "Advanced: the slot loaded, the beat there");
    host_start_easy = 0;
}
static void t_r25_kit(void) { r25_variant(0); }

/* round 25: a .noinit RAM copy of a slot that survives the power-off but is older than the flash's (persist_boot keeps
 * any RAM copy that still reads as a project): the slot list, SAVE's rules and the power-on load are the flash's */
static void t_r25_stale_ram(void)
{
    static project_store_t old[4];
    step_t beat[NSTEP];
    project_t q;
    uint32_t i;
    persist_t p;
    int n;
    host_start_easy = 1;
    host_power_on();
    host_frame();
    host_wait_ms(3100);
    some_steps(0, 40);
    host_tap(B_SAVE);
    host_wait_ms(100);
    CHECK(gn.saved_mine == 1u && gn_song_owned() == 0u, "(a first song saved, slot 1)");
    memcpy(old, proj_slot, sizeof old);              /* (the RAM as it was then) */
    host_wait_ms(GN_SAVED_MS + 100u);
    some_steps(0, 60);                               /* the beat, then SAVE again: the same slot */
    memcpy(beat, trk[0].step, sizeof beat);
    host_tap(B_SAVE);
    host_wait_ms(100);
    CHECK(gn.saved_mine == 1u && gn_song_owned() == 0u, "(the beat saved over it, SAVED)");
    host_wait_ms(3000);
    memcpy(r24_nor, host_nor, sizeof r24_nor);       /* off and on, the RAM's slot copies the old ones */
    host_power_on();
    memcpy(host_nor, r24_nor, sizeof r24_nor);
    memcpy(proj_slot, old, sizeof old);
    memset(&persist_saved, 0, sizeof persist_saved);
    persist_pending = 0;
    n = st_load(OBJ_SETTINGS, &p, sizeof p);
    if (settings_import(&p, n))
        persist_saved = p;
    for (i = 0; i < 4u; i++)                         /* (persist_boot: RAM that reads as a project is kept) */
        if (!proj_import(&proj_scratch, &proj_slot[i], sizeof(project_store_t)))
            proj_fetch(i);
    settings_init();
    panel_init();
    palette_set(settings.palette);
    CHECK(proj_import(&q, &proj_slot[0], sizeof(project_store_t)) && memcmp(q.t[0].step, beat, sizeof beat),
          "(persist_boot alone: the slot list shows the old song)");
    host_frame();
    host_wait_ms(3100);
    CHECK(proj_import(&q, &proj_slot[0], sizeof(project_store_t)) && !memcmp(q.t[0].step, beat, sizeof beat),
          "round 25: at the first frame the slot is the flash's: the beat");
    CHECK(gn.on && gn_song_owned() == 0u && !memcmp(trk[0].step, beat, sizeof beat), "..and Easy starts with it");
    host_start_easy = 0;
}
static void t_r25_drum_machine(void) { r25_variant(1); }
static void t_r25_groove(void) { r25_variant(2); }

/* round 27 (F15, a report on V1.5 1bd4432): "the beat saved that I build in advanced, but then I turned it off then back
 * on and it loaded the ones you made from the easy mode, and that seemed to overwrite what I recorded". Traced in
 * the host (the flash kept, the RAM as at power-on, the slots and the settings record read back as persist_boot
 * reads them): Advanced's SAVE (ui_name.c name_ok, project.c project_save_as) wrote the beat to its slot and left
 * no record anywhere, and power-on (gn_easy.c gn_boot) knew only Easy's song: left in Easy it loaded Easy's older
 * song over the tracks (gn_song_boot), left in Advanced it started Felucca's default tracks (main.c felucca_init,
 * TRK_DEF); either way the beat was not what played, though it was never overwritten. Round 27's rule: power-on
 * restores the last thing saved, whichever mode saved it, in the mode it was saved from; the autosave never writes
 * a slot it does not own (a save of Advanced's or the app's, or an older song of Easy's) */
static void r27_slot_steps(uint32_t slot, uint32_t t, step_t *out)   /* the steps of track t as slot holds them */
{
    project_t q;
    memset(out, 0xEE, sizeof(step_t) * NSTEP);
    if (proj_import(&q, &proj_slot[slot & 3u], sizeof(project_store_t)))
        memcpy(out, q.t[t].step, sizeof(step_t) * NSTEP);
}

static int r27_slot_holds(uint32_t slot, uint32_t t, const step_t *steps)
{
    step_t s[NSTEP];
    r27_slot_steps(slot, t, s);
    return project_used(slot) && !memcmp(s, steps, sizeof s);
}

/* the first power-on (Easy), a song on track 1, SAVE: slot A is Easy's */
static void r27_easy_song(step_t *song0)
{
    host_start_easy = 1;
    host_power_on();
    host_frame();
    host_wait_ms(3100);
    some_steps(0, 40);
    memcpy(song0, trk[0].step, sizeof(step_t) * NSTEP);
    host_tap(B_SAVE);
    host_wait_ms(100);
    CHECK(gn.saved_mine == 1u && gn_song_owned() == 0u, "(Easy: a song saved, slot A Easy's)");
    host_wait_ms(GN_SAVED_MS + 100u);
}

/* Advanced, a beat on track 4 (Felucca's pages write the steps) */
static void r27_adv_beat(step_t *beat, uint32_t base)
{
    host_hold_switch(2100);
    host_wait_ms(200);
    some_steps(3, base);
    trk[3].step[1].note[0] = (uint8_t)(base + 1u);
    trk[3].step[1].n = 1;
    trk[3].step[1].time = ST_NOTE;
    trk[3].step[1].vel = 120;
    memcpy(beat, trk[3].step, sizeof(step_t) * NSTEP);
}

/* Felucca's SAVE (ui_input.c G_SAVE): the slot, the NAME screen, OCT+ writes it (ui_name.c name_ok) */
static void r27_adv_save(uint32_t slot)
{
    name_open(NK_PROJ_SAVE, slot);
    name_ok();
    host_frame();
    host_wait_ms(200);
}

static void r27_boot_says(const char *when)
{
    printf("    %s: %s, slot loaded %d, Easy's slot %d, playing %d\n", when, gn.on ? "Easy" : "Advanced",
           proj_cur == PROJ_NO_SLOT ? -1 : (int)proj_cur, gn_song_owned() == GN_NO_SLOT ? -1 : (int)gn_song_owned(),
           song.playing);
}

static void r27_f15(int back_to_easy)
{
    step_t song0[NSTEP], beat[NSTEP];
    r27_easy_song(song0);
    r27_adv_beat(beat, 36);
    CHECK(!gn.on, "(Advanced)");
    r27_adv_save(1);
    CHECK(r27_slot_holds(1, 3, beat) && proj_cur == 1u, "(Advanced's SAVE: the beat in slot B)");
    if (back_to_easy) {
        host_hold_switch(2100);
        host_wait_ms(500);
        CHECK(gn.on, "(back in Easy before the power goes)");
    }
    host_wait_ms(500);
    r24_power_cycle();
    host_frame();
    host_wait_ms(3100);
    r27_boot_says("after the power cycle");
    CHECK(r27_slot_holds(0, 0, song0) && r27_slot_holds(1, 3, beat), "nothing was overwritten: slot A Easy's song, slot B the beat");
    CHECK(!gn.on, "the last save was Advanced's: the synth starts in Advanced");
    CHECK(proj_cur == 1u && !memcmp(trk[3].step, beat, sizeof beat), "..with that project loaded (slot B), the beat on track 4");
    CHECK(!song.playing, "..stopped (no autoplay, as round 25)");
    host_wait_ms(6000);
    CHECK(r27_slot_holds(0, 0, song0) && r27_slot_holds(1, 3, beat), "..and 6 s on, nothing written over either");
    host_start_easy = 0;
}
static void t_r27_f15_left_advanced(void) { r27_f15(0); }
static void t_r27_f15_left_easy(void) { r27_f15(1); }

/* the other way: Easy's save was the last; the synth left in Advanced without a save: Easy, the song loaded */
static void t_r27_easy_last(void)
{
    step_t song0[NSTEP], beat[NSTEP];
    r27_easy_song(song0);
    r27_adv_beat(beat, 50);                          /* Advanced, steps written, never saved */
    host_wait_ms(500);
    settings_poll();
    r24_power_cycle();
    host_frame();
    host_wait_ms(3100);
    r27_boot_says("Easy's save the last, left in Advanced");
    CHECK(gn.on && gn_song_owned() == 0u && !memcmp(trk[0].step, song0, sizeof song0) && !song.playing,
          "the last save was Easy's: Easy, its song loaded, stopped");
    ui_draw();
    CHECK(gs.mark && gn.auto_dirty == GN_LOOP_SAVED, "..the header's mark: the saved song");
    /* nothing saved at all since: the mode it was left in, as before (tests/gn_start_test.c t_power_cycles) */
    host_start_easy = 0;
}

/* the autosave never writes a slot that holds someone else's save (Advanced's, here over Easy's own slot A), nor an
 * older song of Easy's it no longer owns: a new slot, and that is the last save */
static void t_r27_autosave_refuses(void)
{
    step_t song0[NSTEP], beat[NSTEP], rec[NSTEP];
    uint32_t seq0, seq1;
    /* (1) Advanced saved over Easy's own slot A */
    r27_easy_song(song0);
    r27_adv_beat(beat, 36);
    r27_adv_save(0);
    CHECK(r27_slot_holds(0, 3, beat) && gn_song_owned() == GN_NO_SLOT, "(Advanced's SAVE over slot A: no longer Easy's)");
    seq0 = r24_seq(0);
    host_hold_switch(2100);
    host_wait_ms(300);
    easy_record();
    key(7, 100);
    host_tap(B_REC);
    host_tap(B_PLAY);
    host_wait_ms(4600);
    memcpy(rec, trk[0].step, sizeof rec);
    CHECK(r24_seq(0) == seq0 && r27_slot_holds(0, 3, beat), "Easy's autosave never writes Advanced's save (slot A as saved)");
    CHECK(gn_song_owned() == 1u && r27_slot_holds(1, 0, rec), "..it took the first empty slot (B), Easy's now");
    r24_power_cycle();
    host_frame();
    host_wait_ms(3100);
    CHECK(gn.on && proj_cur == 1u && !memcmp(trk[0].step, rec, sizeof rec), "..the last save: Easy, slot B");
    host_start_easy = 0;
    /* (2) Advanced saved into another slot: Easy's older song (A) is not Easy's to write any more */
    r27_easy_song(song0);
    r27_adv_beat(beat, 36);
    r27_adv_save(1);
    seq0 = r24_seq(0);
    seq1 = r24_seq(1);
    host_hold_switch(2100);
    host_wait_ms(300);
    easy_record();
    key(9, 100);
    host_tap(B_REC);
    host_tap(B_PLAY);
    host_wait_ms(4600);
    CHECK(r24_seq(0) == seq0 && r24_seq(1) == seq1 && r27_slot_holds(0, 0, song0) && r27_slot_holds(1, 3, beat),
          "neither Easy's older song (A) nor Advanced's beat (B) written");
    CHECK(gn_song_owned() == 2u && project_used(2), "..the autosave in slot C");
    host_start_easy = 0;
}

/* four slots taken (Easy's older song and three of Advanced's): FULL once, nothing written; the power-on: the last */
static void t_r27_full(void)
{
    step_t song0[NSTEP], beat[NSTEP];
    uint32_t seq[4], i, same = 1;
    r27_easy_song(song0);
    r27_adv_beat(beat, 30);
    r27_adv_save(1);
    some_steps(2, 44);
    r27_adv_save(2);
    some_steps(1, 52);
    r27_adv_save(3);
    for (i = 0; i < 4u; i++)
        seq[i] = r24_seq(i);
    host_hold_switch(2100);
    host_wait_ms(300);
    easy_record();
    key(7, 100);
    host_tap(B_REC);
    host_tap(B_PLAY);
    for (i = 0; i < 400u && gn.saved_t == 0u; i++)
        host_frame();
    for (i = 0; i < 4u; i++)
        same &= r24_seq(i) == seq[i];
    CHECK(gn.saved_mine == 2u && same, "no slot Easy may write: FULL, nothing written");
    CHECK(r27_slot_holds(0, 0, song0), "..Easy's older song (A) kept");
    r24_power_cycle();
    host_frame();
    host_wait_ms(3100);
    CHECK(!gn.on && proj_cur == 3u, "..the power-on: the last save (Advanced's, slot D), in Advanced");
    host_start_easy = 0;
}

/* the app's project save while Easy is on (editor.c ED_PROJECT): the last save, in Easy, but not Easy's own song */
static void t_r27_app_save(void)
{
    step_t song0[NSTEP], app[NSTEP];
    uint32_t seq2;
    r27_easy_song(song0);
    some_steps(1, 60);
    memcpy(app, trk[1].step, sizeof app);
    CHECK(!project_save(2), "(the app saved slot C)");
    gn_project(1, 2);
    host_wait_ms(100);                               /* (the next main-loop frame writes the record) */
    seq2 = r24_seq(2);
    r24_power_cycle();
    host_frame();
    host_wait_ms(3100);
    CHECK(gn.on && proj_cur == 2u && !memcmp(trk[1].step, app, sizeof app) && !song.playing,
          "the app's save the last: Easy, slot C loaded, stopped");
    CHECK(gn_song_owned() == GN_NO_SLOT && gn.auto_dirty != GN_LOOP_SAVED, "..not Easy's own song (no mark)");
    easy_record();
    key(7, 100);
    host_tap(B_REC);
    host_tap(B_PLAY);
    host_wait_ms(4600);
    CHECK(r24_seq(2) == seq2 && r27_slot_holds(0, 0, song0) && gn_song_owned() == 1u,
          "..Easy's autosave: neither the app's slot C nor Easy's older A, the empty B");
    host_start_easy = 0;
}

/* a rename (Advanced's NAME, project.c project_rename) writes the slot: the last save, by whoever renamed it; Easy's
 * own song renamed in Advanced is no longer Easy's to write (round 9: a rename ends it) */
static void t_r27_rename(void)
{
    step_t song0[NSTEP];
    char nm[PROJ_NAME_LEN + 1u];
    uint32_t seq0;
    r27_easy_song(song0);
    host_hold_switch(2100);
    host_wait_ms(200);
    CHECK(!gn.on && !project_rename(0, "KYLE"), "(Advanced: slot A renamed)");
    host_wait_ms(100);
    seq0 = r24_seq(0);
    CHECK(gn_song_owned() == GN_NO_SLOT, "renamed in Advanced: no longer Easy's");
    r24_power_cycle();
    host_frame();
    host_wait_ms(3100);
    CHECK(!gn.on && proj_cur == 0u && !memcmp(trk[0].step, song0, sizeof song0) && project_name(0, nm) && str_eq(nm, "KYLE"),
          "..the power-on: Advanced (the rename the last write), slot A loaded");
    host_hold_switch(2100);
    host_wait_ms(300);
    easy_record();
    key(7, 100);
    host_tap(B_REC);
    host_tap(B_PLAY);
    host_wait_ms(4600);
    CHECK(r24_seq(0) == seq0 && gn_song_owned() == 1u, "..Easy's autosave: slot B, never the renamed A");
    host_start_easy = 0;
}

/* migration: a round 25/26 record (Easy's song, byte 29 0x80 | slot, no round 27 bit) starts as before: in the mode
 * the synth was left in, Easy's song loaded only in Easy; the next save writes round 27's record */
static void r27_migrate(int left_easy)
{
    step_t song0[NSTEP];
    r27_easy_song(song0);
    GN_SONG_BYTE = 0x80u;                            /* (slot A, as round 26 wrote it) */
    settings_save();
    if (!left_easy) {
        host_hold_switch(2100);
        host_wait_ms(300);
        CHECK(!gn.on, "(left in Advanced)");
    }
    settings_poll();
    CHECK(gn_song_owned() == 0u, "(a round 26 record: slot A Easy's)");
    r24_power_cycle();
    host_frame();
    host_wait_ms(3100);
    r27_boot_says(left_easy ? "a round 26 record, left in Easy" : "a round 26 record, left in Advanced");
    if (left_easy) {
        CHECK(gn.on && proj_cur == 0u && !memcmp(trk[0].step, song0, sizeof song0), "migration: Easy, its song (as before)");
        easy_record();
        key(7, 100);
        host_tap(B_REC);
        host_tap(B_PLAY);
        host_wait_ms(4600);
        CHECK(gn_song_owned() == 0u && GN_SONG_BYTE != 0x80u, "..its autosave: the same slot A, round 27's record now");
    } else {
        CHECK(!gn.on && proj_cur == PROJ_NO_SLOT && memcmp(trk[0].step, song0, sizeof song0),
              "migration: Advanced, Felucca's power-on, nothing loaded (as before)");
    }
    host_start_easy = 0;
}
static void t_r27_migrate_easy(void) { r27_migrate(1); }
static void t_r27_migrate_advanced(void) { r27_migrate(0); }

static const struct {
    const char *name;
    void (*run)(void);
} CASES[] = {
    {"round 27 (F15): Advanced's SAVE, left in Advanced, power cycle: Advanced, that project", t_r27_f15_left_advanced},
    {"round 27 (F15): Advanced's SAVE, back to Easy, power cycle: Advanced, that project", t_r27_f15_left_easy},
    {"round 27: Easy's save the last, left in Advanced: Easy, its song", t_r27_easy_last},
    {"round 27: the autosave never writes Advanced's save nor an older song of Easy's", t_r27_autosave_refuses},
    {"round 27: four slots taken: FULL, nothing written; the power-on the last save", t_r27_full},
    {"round 27: the app's save in Easy: the last, loaded in Easy, never autosaved over", t_r27_app_save},
    {"round 27: a rename writes the slot: the last save, no longer Easy's", t_r27_rename},
    {"round 27: migration: a round 26 record left in Easy starts as before", t_r27_migrate_easy},
    {"round 27: migration: a round 26 record left in Advanced starts as before", t_r27_migrate_advanced},
    {"round 25: SAVE path, Easy's kit, power cut 3 s after SAVED", t_r25_kit},
    {"round 25: a stale RAM copy of a slot: the flash's wins at the first frame", t_r25_stale_ram},
    {"round 25: SAVE path, the drum machine, power cut 3 s after SAVED", t_r25_drum_machine},
    {"round 25: SAVE path, over a demo groove, power cut 3 s after SAVED", t_r25_groove},
    {"round 25: the reported sequence: record, SAVE, power off and on", t_r25_power_cycle},
    {"round 24: the loop Easy wrote saved by itself, stopped and settled", t_r24_autosave},
    {"round 24: the app's PRESET of an Easy sound is Easy's pick", t_r24_adopt},
#if GN_DEMO_GROOVES
    {"grooves: five, by Easy's sounds, the kit's keys, the bass low, 1 2 4 8 bars, the bytes", t_grooves_defined},
    {"grooves: PRESETS on the loop page loads one on four tracks as Easy's picks, no slot written", t_grooves_load},
    {"grooves: never over the player's music (SAVE FIRST); saved, it loads; saved again, a new slot", t_grooves_owner},
    {"grooves: arming and undo end, an external clock's tempo, leaving puts the levels back", t_grooves_rules},
#endif
    {"round 23: shared USB cleanup retains both scopes", t_r23_usb_cleanup_scope},
    {"round 21: long host sleep preserves panel, TRS and song", t_r21_suspend_local},
    {"round 21: USB and TRS share a channel and pitch", t_r21_suspend_shared},
    {"round 20: driver suspension timeout", t_r20_usb_suspend},
    {"round 20: suspension across timer wrap", t_r20_usb_wrap},
    {"round 20: short suspension and resume", t_r20_usb_resume},
    {"round 20: BARS erased tail stays erased", t_r20_bars_erased},
    {"round 19: BARS hidden tail", t_r19_bars},
    {"round 19: STOP", t_r19_stop},
    {"round 19: Advanced to Easy", t_r19_enter},
    {"round 19: Easy to Advanced", t_r19_leave},
    {"round 19: sound change", t_r19_sound},
    {"round 19: USB disconnect", t_r19_usb},
    {"round 19: engine change", t_r19_engine},
    {"round 16: a rejected TRACK_PARAM leaves Wobble's claim intact", t_r16_wobble_invalid_track},
    {"round 16: a queued entry key released and repressed sounds once", t_r16_entry_repress},
    {"round 17: a rejected track preserves the kept transpose baseline", t_r17_kept_invalid_track},
    {"round 17: an invalid setting cannot disown the sound", t_r17_sound_invalid_setting},
    {"round 17: rejected indices preserve kept level, grid and motion undo", t_r17_ownership_invalid_indices},
    {"Easy off: Felucca as it is; every hook passes", t_off},
    {"in and out: HOME + SAVE 2 s either way; a short hold changes nothing", t_switch},
    {"the hold is unbroken: either button up starts it again (finding 7)", t_switch_unbroken},
    {"the hold beside SAVE held (undo), HOME held (menu), the layers, SEQ", t_switch_collisions},
    {"in and out change nothing musical; a picture changes its own track (finding 2)", t_switch_keeps},
    {"retained settings back on leaving; three picks one load for Felucca's undo (review 4)", t_retained},
    {"a REC or SEQ press under way when the chord takes the panel: no tap either side (review 5)", t_rec_interrupted},
    {"kept settings: no claim across a project load, a backup, an outside write; new baselines (round 3)", t_kept_projects},
    {"the editor's reads (connecting, SONG after a save, backup, preferences) keep Easy's sounds and undo (round 3)", t_editor_reads},
    {"a song's motion and the kept level: a song ends the claim, none taken while it plays (round 5)", t_song_kept},
    {"a song moves PTCH or SET away and back during a held HOME + SAVE: no longer Easy's (round 6)", t_hold_replacement},
    {"a SONG play refused (no song): the level claim stays (round 6)", t_song_rejected},
    {"the white-key table, 27 keys (F5, F#5)", t_white_keys},
    {"Easy's sound: every key chromatic (round 13), octaves", t_keys_melody},
    {"the drums: the white keys pads 1 to 16, the black keys the kit's other sounds", t_keys_pads},
    {"another sound: every key as in Felucca", t_keys_other},
    {"the Gnome: a syllable per white key, his mouth on the vowel", t_gnome},
    {"the knobs: Easy's sounds only, every write in range, any engine from the app (finding 1)", t_knobs},
    {"ownership ends at every load Easy did not make; keys and knobs leave it as loaded (review 3)", t_ownership},
    {"the ten pictures' definitions; the kit's pads", t_sounds_defined},
    {"the drums: set up only when picked, kept when picked again", t_drums},
    {"SELECT: the real tempo 40 to 240 in every header; EDIT taps it; an external clock's (pick 4)", t_tempo},
    {"pictures and their sounds: ALGORITHM the picture, PRESETS the sound, the peek list (pick 3)", t_groups},
    {"loop: REC records, REC held undoes", t_loop},
    {"undo: Easy's own last recording only; a project load, the app's steps, another track (finding 3)", t_undo_rules},
    {"undo: the app's writes while armed, while a key is held, a runtime backup (review 2)", t_undo_external},
    {"undo: no held recording left to write back on release (finding 4)", t_undo_held},
    {"recording: REC arms, PLAY counts in, bar 1, retarget, KEEP / REPLACE, punch in, undo on SAVE (pick 5)", t_rec},
    {"the loop pages: SEQ circle steps record; BARS SWING SNAP VOLUME; the grid; step entry (picks 6-9)", t_loop_pages},
    {"round 8 (2, 4): a grid change keeps motion valid; undo puts the motion store back exactly", t_r8_motion},
    {"round 8 (3): a count-in ends when the music is replaced (a project, the runtime backup)", t_r8_replaced},
    {"round 8 (5): REPLACE only in steps the take enters; a short punch leaves the rest", t_r8_replace_entry},
    {"round 8 (6): the default grid only on an untouched pattern; the child's BARS / SNAP stay", t_r8_defaults},
    {"round 8 (7): the loop's level motion retires the level claim (an equal value too)", t_r8_level_motion},
    {"round 8 (8): one knob map for the screen and the input; K4 KEEP over GLO while armed", t_r8_knob_map},
    {"round 8 (10): the punch keeps the hold's keys; a tap only arms", t_r8_punch_keys},
    {"round 9 (3): undo's motion only while unwritten since, only the undone track's", t_r9_motion_gen},
    {"round 9 (4): REPLACE keeps the take's same-pitch note, its ties; unreached steps as they were", t_r9_replace},
    {"round 9 (5): a restart ends the take without clearing the rest; STOP drops a punch's arming", t_r9_restart},
    {"round 9 (5): an external clock's START, STOP, CONTINUE through the MIDI queue during a take", t_r9_midi_start},
    {"round 9 (6): the music replaced mid-gesture: no punch, tap, save, or held write afterwards", t_r9_cancel},
    {"round 9 (7): the grid choice per track", t_r9_grid_per_track},
    {"round 9: the punch's chord and ties; another track during the hold", t_r9_punch_hold},
    {"round 10: an Easy pick advances the motion generation (32 bits)", t_r10_motion_gen},
    {"round 10: REPLACE keeps a DRUM hit recorded ahead", t_r10_drums_ahead},
    {"round 10: 64 presses in the log; past it both kept", t_r10_log},
    {"round 10: STOP processes the step just entered", t_r10_stop_entered},
    {"round 10: the music replaced during HOME + SAVE cancels the switch", t_r10_chord},
    {"round 10: a tap keeps the last recording's undo; its rollback exact", t_r10_tap_undo},
    {"round 10: a key held at punch start is recorded", t_r10_held_key},
    {"round 11: 240 BPM, 1/32: a 350 ms tap is never a punch; no motion from it", t_r11_fast_tap},
    {"round 11: a 48-step pattern: the tap's window wraps at LEN", t_r11_len48},
    {"round 11: a stalled panel: the tap put back and armed", t_r11_stall},
    {"round 12: a count-in's take: a fresh tick, its undo copy the pattern before it", t_r12_count_in},
    {"round 26: the metronome: the count-in's four, each bar's 1 accented, never recorded, USB or MIDI", t_r26_click_timing},
    {"round 26: the metronome armed while playing: on the loop's bars, the take at an accented 1", t_r26_click_armed},
    {"round 26: the metronome's switch: EDIT held + K3, its card, kept across a power cycle", t_r26_click_switch},
    {"round 26: the metronome in Advanced: Felucca's recording clicks too, the same setting", t_r26_click_advanced},
    {"round 13 (2): every key chromatic, another sound's scale setting kept", t_r13_chromatic},
    {"round 13 (3): any track's knobs move its sound (the engine's fallback map)", t_r13_fallback},
    {"round 13 (4): a kit's held pads never tie over the steps after (a reported case: toms)", t_r13_kit_holds},
    {"round 14 (1, 2): the entry's Gnome costs nothing: unused, track 1 and the undo byte for byte; used, undo gives back", t_r14_entry},
    {"round 14 (3): the fallback Wobble's LFO rate and depth put back (its place, leaving), ownership kept", t_r14_wobble},
    {"round 14 (4): the kit's hold stopped in the recording path: a stalled panel, no tie; tails and repeats sound", t_r14_kit_stall},
    {"round 15 (1): the fallback Wobble's depth: the app's write after Easy's stays the app's on leaving (the knob followed it)", t_r15_wobble_app},
    {"round 15 (2): the first key always sounds: track 1 already on VOICE; the app's sound before the UI's frame", t_r15_first_note},
    {"the panel: four track buttons, SEQ, GLO's volumes, PLAY anywhere, ARP, HOME (pick 2)", t_panel},
    {"SAVE: the song, never over someone else's; HOME joining saves nothing (pick 2)", t_save_song},
    {"the screen: bounded strips, editor SETs, drawn part by part = drawn whole (finding 6)", t_screen},
    {"the screen: leaving in strips, the update countdown, all LCD traffic counted (review 6)", t_screen_exit},
    {"Easy, the palette under it, an editor save and SONG, the leaving, the setting in flash (round 3)", t_palette_session},
    {"saved: the channel lines are seq.c's routing (finding 9)", t_saved_routing},
    {"saved: shown after a verified save, a key or 6 s ends it", t_saved},
    {"LEDs", t_leds},
    {"upkeep: no layer, no grid, no FX effect, no dialog", t_upkeep},
    {"a random walk over the panel and the app: every setting in range, nothing overflows", t_fuzz},
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
    printf(failed ? "gn easy tests FAILED\n" : "gn easy tests passed\n");
    return failed ? 1 : 0;
}
