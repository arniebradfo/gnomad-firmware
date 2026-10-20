/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kyle Palaniuk */
/* Gnomad (FORK.md, "Gnomad engines") host test: the engine swap (GN_ENGINES 1, gn_engines.h) through the
 * firmware's own audio path, tests/hostsim.c (voice.c, fx.c mix_block with gn_core.c's hook, seq.c), as the
 * audio interrupt renders it, 32-frame blocks:
 *   table     the Gnomad engines in the retired engines' slots with their names; slot 8 not offered, nor the
 *             seven retired (UX-L); the order lists every offered engine once; NENGINES 14 (favourites' row 15
 *             is Easy's)
 *   boot      the core starts (its arenas: 7,168 B and the 65,536 B pool)
 *   engines   each Gnomad engine sounds on a key (voice.c -> NOTE_ON); after the note-off its token stays in
 *             Felucca's budget while the core's voice sounds (never a core voice without its token) and ends
 *             soon after; the output is silent after the release
 *   queue     notes queued while the consumer interrupts between a slot and its index (the test hook): every
 *             note reaches the core, nothing lost or rejected
 *   orphans   a token voice.c ends without done() (cleared, killed, a panic) still gets its NOTE_OFF; a
 *             releasing token given to another part cuts its track's tails (budget kept)
 *   fm6       a project's FM6 patch survives unpack, set and pack byte for byte (FM6 retired)
 *   retired   the build retires thirteen Felucca engines: the six whose slots the Gnomad engines take, and the
 *             seven the decision of 2026-10-11 compiled out (gn_engines.h); a key on each of those seven
 *             slots is silent and holds no voice (gn_size_stub.h)
 *   switch    a held Gnomad note is released when the track switches to another Gnomad engine, and to a retired
 *             slot (no hanging core voice); two parts on different Gnomad engines play together, each within
 *             6 dB of the other (with a Felucca engine kept, GN_DROP_ANALOG 0: ANALOG against VA, as before)
 *   controls  the track's delay send is the core's Echo (an echo appears 375 ms later); MUTE and LEVEL 0
 *             silence the track; an 8-note chord with every control moving in one block is accepted
 *             (no rejected render, no lost event)
 *   idle      after a second of silence the core is no longer rendered (no cost while unused)
 *   cost      host ns per block, 8 FM voices with echo and reverb against 8 VA voices dry (ANALOG's when the
 *             build keeps it) (a proxy only: the pi32v2's timing is measured on the synth)
 *   tests/run_fork_tests.sh builds it: build/host-gn/gn_core_test */
#define GN_ENGINES 1
#include "../firmware/src/gn_engines.h"
static void gnc_preempt(void);                          /* the queue's test hook: the consumer between slot and index */
#define GN_CORE_PREEMPT() gnc_preempt()
#define main hostsim_main
#include "hostsim.c"
#undef main

static int preempt_on;
static uint32_t preempts;
static void gnc_preempt(void)
{
    static int in;
    int32_t l[CTL], r[CTL];
    if (!preempt_on || in)
        return;
    in = 1;
    memset(l, 0, sizeof l);
    memset(r, 0, sizeof r);
    gn_core_mix(l, r, CTL);                             /* a whole consumer block, half an event queued */
    preempts++;
    in = 0;
}

static int failures;
#define CHECK(c, ...) do { if (c) { printf("  ok    "); } else { printf("  FAIL  "); failures++; } \
                           printf(__VA_ARGS__); printf("\n"); } while (0)

static int32_t blk[2u * CTL];
static int32_t render(uint32_t blocks)                  /* peak |L| over the blocks */
{
    int32_t peak = 0;
    uint32_t b, i;
    for (b = 0; b < blocks; b++) {
        gn_core_host();                                 /* (round MF: the main loop's part, every block) */
        mix_block(blk, CTL);
        for (i = 0; i < CTL; i++) {
            int32_t a = blk[2u * i] < 0 ? -blk[2u * i] : blk[2u * i];
            if (a > peak)
                peak = a;
        }
    }
    return peak;
}
static uint32_t ms(uint32_t m) { return (m * FS / 1000u + CTL - 1u) / CTL; }

static uint32_t tokens(const track_t *t)
{
    uint32_t i, n = 0;
    for (i = 0; i < NVOICE; i++)
        n += t->v[i].active;
    return n;
}
static uint32_t core_voices(void)
{
    gc_report r;
    gn_core_report(&r);
    return r.voices_active;
}
static void reset_all(void)
{
    uint32_t k;
    host_tracks_init();
    for (k = 0; k < NTRK; k++) {
        trk_all_off(&trk[k]);
        host_preset(&trk[k], 0, 0);
        trk[k].p[P_LEVEL] = 100;
        trk[k].p[P_DLY] = trk[k].p[P_REV] = trk[k].p[P_CHOR] = 0;
    }
    render(ms(4500));                                   /* every tail out (the core clears its effects after 4 s) */
}

static const struct { uint32_t slot; const char *name; uint32_t note; } GN[] = {
    {9, "VA", 60}, {12, "FM", 60}, {10, "DRUMS", 36}, {7, "GNOME", 60}, {13, "SAMPLER", 69}, {8, "SAMPLER", 69},
};

int main(void)
{
    uint32_t i, k, seen[NENGINES] = {0};
    printf("Gnomad engines (gn_core.c) in the firmware's audio path, GN_ENGINES 1\n");

    /* ---- table */
    for (i = 0; i < NELEM(GN); i++)
        CHECK(!strcmp(ENGINES[GN[i].slot]->name, GN[i].name) && ENGINES[GN[i].slot]->note_on == gnc_note_on,
              "table: slot %u is %s (%s)", GN[i].slot, GN[i].name, ENGINES[GN[i].slot]->name);
    CHECK(NENGINES == 14 && NENG_SHOWN == 5, "table: NENGINES %d, offered %d (the five Gnomad engines)", NENGINES,
          NENG_SHOWN);
    for (i = 0; i < NENG_SHOWN; i++)
        seen[ENGINE_ORDER[i] % NENGINES]++;
    for (k = 0, i = 0; i < NENGINES; i++)
        k += seen[i] != (uint32_t)eng_ok(i) || (eng_gone(i) && eng_ok(i));
    CHECK(!k && !eng_ok(8) && !eng_ok(ENGI_DIGITAL), "table: the order offers every pickable engine once, never 1, 8 "
          "or a retired one");
    CHECK(eng_rank(8) == eng_rank(13), "table: slot 8 (GRAIN's sounds) shows as SAMPLER in PRESETS");
    CHECK(GN_DROP_PHYS && GN_DROP_GRAIN && GN_DROP_WHEEL && GN_DROP_FM6 && GN_DROP_SLICE && GN_DROP_DRUM &&
          GN_DROP_ANALOG && GN_DROP_PHASE && GN_DROP_LOFI && GN_DROP_VOICE && GN_DROP_TRIO && GN_DROP_NOISE &&
          GN_DROP_SAMPLE, "build: the thirteen retired (the six the Gnomad engines replace, and the seven the "
          "decision of 2026-10-11 compiled out: ANALOG PHASE LOFI SAMPLE VOICE TRIO NOISE)");
    {   /* ---- retired: the seven slots play nothing (a silent descriptor, gn_size_stub.h) */
        static const uint32_t OUT7[7] = {0, 2, 3, 4, 5, 6, 11};
        uint32_t silent = 0, held = 0;
        int32_t worst = 0, pk;
        reset_all();
        for (i = 0; i < 7u; i++) {
            host_preset(&trk[0], OUT7[i], 0);
            trk_note_on(&trk[0], 60, 100);
            pk = render(ms(100));
            held += tokens(&trk[0]) + core_voices();
            silent += pk == 0;
            worst = pk > worst ? pk : worst;
            trk_note_off(&trk[0], 60);
            render(2);
        }
        CHECK(silent == 7u && !held && ENGINES[0]->note_on == gn_size_note_on, "retired: a key on each of the seven "
              "slots is silent (%u of 7, loudest peak %d) and holds no voice (%u)", silent, worst, held);
    }

    /* ---- presets (round FW-CONTENT-7): a preset is its bank ID; what GN_PRESET reads from the record is, field for
     * field, the preset_t the tables held until 4d8a1b9: the record's name in flash, the knobs tools/gn_gen_content.py
     * computed (gn_content.h X(id, BRIT, WOBL, CHOR, DLY, REV)), the envelope, MONO and DIST every Gnomad preset had */
    {
#define GNT_X(id, b, w, c, d, r) {(id), (b), (w), (c), (d), (r)},
        typedef struct { uint16_t id; uint8_t k[5]; } gnt_x;   /* X's: bank id, BRIT, WOBL, CHOR, DLY, REV */
        static const gnt_x X_VA[] = {GN_CONTENT_VA(GNT_X)}, X_FM[] = {GN_CONTENT_FM(GNT_X)},
            X_KITS[] = {GN_CONTENT_KITS(GNT_X)}, X_GNOME[] = {GN_CONTENT_GNOME(GNT_X)},
            X_SAMPLE[] = {GN_CONTENT_SAMPLE(GNT_X)};
#undef GNT_X
        static const struct { uint32_t slot; const gnt_x *x; uint32_t n; } L[5] = {
            {GN_ENGI_VA, X_VA, NELEM(X_VA)}, {GN_ENGI_FM, X_FM, NELEM(X_FM)}, {GN_ENGI_DRUMS, X_KITS, NELEM(X_KITS)},
            {GN_ENGI_GNOME, X_GNOME, NELEM(X_GNOME)}, {GN_ENGI_SAMPLE, X_SAMPLE, NELEM(X_SAMPLE)}};
        uint32_t bad = 0, n = 0, s, owned_n = 0;
        char first[96] = "";
        for (s = 0; s < 5u; s++) {
            const engine_t *e = ENGINES[L[s].slot];
            int32_t ce = gnc_engine(L[s].slot);
            for (i = 0; i < L[s].n; i++, n++) {
                const gnt_x *x = &L[s].x[i];
                const preset_t *p = GN_PRESET(e, i);
                const char *nm = ce == GC_DRUMS ? (const char *)&GN_CONTENT_KITS[416u * x->id + 16u]
                                                : (const char *)&GN_CONTENT_PITCHED[128u * x->id + 16u];
                int8_t ek[4] = {(int8_t)x->k[0], (int8_t)x->k[1], 0, 0};   /* round MF: a knob of the sound's */
                uint32_t sl, store, rest;                                    /* own rests at its rest place */
                uint64_t par;
                for (sl = 0; sl < 4u; sl++)
                    if (ce != GC_DRUMS && gnc_slot(&GN_CONTENT_PITCHED[128u * x->id], ce, sl, &store, &par, &rest))
                        ek[sl] = (int8_t)gnc_kr(rest), owned_n++;
                int same = GNC_LIST[ce].id[i] == x->id && p->name == nm && GN_PRESET_NAME(e, i) == nm &&
                           p->e[0] == ek[0] && p->e[1] == ek[1] && p->e[2] == ek[2] && p->e[3] == ek[3] && !p->e[4] && !p->e[5] &&
                           !p->e[6] && !p->e[7] && p->env[0] == 0 && p->env[1] == 64 && p->env[2] == 127 &&
                           p->env[3] == 127 && !p->fenv && !p->mono && p->fx[0] == 1 && p->fx[1] == x->k[2] + 1 &&
                           p->fx[2] == x->k[3] + 1 && p->fx[3] == x->k[4] + 1 && !p->pat && !GN_PRESET_PAT(e, i);
                if (!same && !bad++)
                    snprintf(first, sizeof first, " (first: slot %u place %u, bank %u)", L[s].slot, i, x->id);
            }
        }
        CHECK(!bad && n == 416u && !ENGINES[GN_ENGI_VA]->presets && !ENGINES[GN_ENGI_FM]->presets &&
              !ENGINES[GN_ENGI_DRUMS]->presets && !ENGINES[GN_ENGI_GNOME]->presets &&
              !ENGINES[GN_ENGI_SAMPLE]->presets && ENGINES[GN_ENGI_HIDDEN] == ENGINES[GN_ENGI_SAMPLE],
              "presets: no preset_t table; all %u read from their records (name in flash, the generator's knobs, the "
              "envelope and sends as before; %u knobs of a sound's own at their rest places, round MF), %u differ%s", n,
              owned_n, bad, first);
        CHECK(GN_PRESET(ENGINES[0], 0) == &ENGINES[0]->presets[0] &&
              GN_PRESET_NAME(ENGINES[0], 0) == ENGINES[0]->presets[0].name,
              "presets: a Felucca engine's are its table's (%s)", ENGINES[0]->presets[0].name);
    }

    /* ---- boot */
    gn_core_boot();
    CHECK(gnc.core && gnc.init_status == GC_OK, "boot: gc_init status %u", gnc.init_status);
    CHECK(((uintptr_t)gnc_state.b & 7u) == 0 && ((uintptr_t)gnc_fx.b & 7u) == 0 && sizeof gnc_state.b == 7168u &&
          sizeof gnc_fx.b == 65536u, "boot: arenas 7168 B + 65536 B, 8-byte aligned");

    /* ---- engines */
    for (i = 0; i < NELEM(GN); i++) {
        track_t *t = &trk[0];
        int32_t on, tail;
        uint32_t left, gone;
        reset_all();
        host_preset(t, GN[i].slot, 0);
        t->p[P_DLY] = t->p[P_REV] = t->p[P_CHOR] = 0;   /* (dry: a preset sets the record's sends, a kit's at unity) */
        trk_note_on(t, GN[i].note, 100);
        uint32_t b, bad = 0, core_end = 0, tok_end = 0;
        on = render(ms(300));
        trk_note_off(t, GN[i].note);
        render(2);
        left = tokens(t);
        for (b = 0; b < ms(2500); b++) {
            render(1);
            bad += core_voices() && !tokens(t);
            if (!core_end && !core_voices())
                core_end = b;
            if (!tok_end && !tokens(t))
                tok_end = b;
        }
        gone = core_voices();
        tail = render(ms(100));
        CHECK(on > 1000 && left == 1 && !bad && !gone && !tokens(t) && tok_end >= core_end && tok_end <= gnc.tail[0] + 2u
              && tail < 4, "%s (slot %u): peak %d; token kept after the note-off (%u), never a core voice without "
              "its token (%u blocks), core voice idle after %u ms, token after %u ms (its patch's release); then peak %d", GN[i].name,
              GN[i].slot, on, left, bad, core_end * CTL * 1000u / FS, tok_end * CTL * 1000u / FS, tail);
    }
    CHECK(!gnc.errors && !gnc.lost, "engines: no rejected render (%u, last status %u detail %u), no lost event (%u)",
          gnc.errors, gnc.last_status, gnc.last_detail, gnc.lost);

    /* ---- switch */
    {
        track_t *t = &trk[0];
        uint32_t held;
        reset_all();
        host_preset(t, 9, 0);
        trk_note_on(t, 60, 100);
        render(ms(200));
        held = core_voices();
        host_preset_req(t, 12, 0);                      /* the UI asks for FM (eng_req): the ISR switches */
        render(ms(1500));
        CHECK(held == 1 && core_voices() == 0 && t->engine == 12, "switch: a held VA note (%u core voice) released "
              "when the track switches to FM (core voices %u)", held, core_voices());
        reset_all();
        host_preset(t, 9, 0);
        trk_note_on(t, 60, 100);
        render(ms(200));
        held = core_voices();
        host_preset_req(t, 0, 0);                       /* a retired slot (ANALOG's, silent; a 1.0.1 project's) */
        render(ms(1500));
        CHECK(held == 1 && core_voices() == 0 && t->engine == 0, "switch: a held VA note (%u core voice) released "
              "when the track switches to a retired slot, ANALOG's (core voices %u)", held, core_voices());
    }
#if !GN_DROP_ANALOG                                     /* a comparison build that keeps ANALOG (gn_engines.h) */
    {
        int32_t a, g, both;
        reset_all();
        host_preset(&trk[0], 0, 0);
        trk_note_on(&trk[0], 48, 100);
        a = render(ms(200));
        trk_note_off(&trk[0], 48);
        reset_all();
        host_preset(&trk[1], 9, 0);
        trk_note_on(&trk[1], 67, 100);
        g = render(ms(200));
        trk_note_off(&trk[1], 67);
        reset_all();
        host_preset(&trk[0], 0, 0);
        host_preset(&trk[1], 9, 0);
        trk_note_on(&trk[0], 48, 100);
        trk_note_on(&trk[1], 67, 100);
        both = render(ms(200));
        trk_note_off(&trk[0], 48);
        trk_note_off(&trk[1], 67);
        CHECK(a > 1000 && g > 1000 && both > a && both > g && g * 2 > a && g < a * 2, "mix: ANALOG alone %d, VA alone "
              "%d (within 6 dB: GN_CORE_GAIN_Q8), together %d", a, g, both);
    }
#endif
    {   /* the shipping build: every sounding engine is the core's. VA on track 1 against each other Gnomad engine on
         * track 2 (its first preset; DRUMS a kick): each alone, then the two together (the core's part gains) */
        static const struct { uint32_t slot, note; } OTH[3] = {{12, 67}, {7, 67}, {13, 69}};
        static const char *const NM[3] = {"FM", "GNOME", "SAMPLER"};
        int32_t a, g, both;
        for (k = 0; k < 3u; k++) {
            reset_all();
            host_preset(&trk[0], 9, 0);
            trk_note_on(&trk[0], 48, 100);
            a = render(ms(200));
            trk_note_off(&trk[0], 48);
            reset_all();
            host_preset(&trk[1], OTH[k].slot, 0);
            trk_note_on(&trk[1], OTH[k].note, 100);
            g = render(ms(200));
            trk_note_off(&trk[1], OTH[k].note);
            reset_all();
            host_preset(&trk[0], 9, 0);
            host_preset(&trk[1], OTH[k].slot, 0);
            trk_note_on(&trk[0], 48, 100);
            trk_note_on(&trk[1], OTH[k].note, 100);
            both = render(ms(200));
            trk_note_off(&trk[0], 48);
            trk_note_off(&trk[1], OTH[k].note);
            CHECK(a > 1000 && g > 1000 && both > a && both > g && g * 2 > a && g < a * 2, "mix: VA alone %d, %s alone "
                  "%d (within 6 dB), together %d", a, NM[k], g, both);
        }
    }

    /* ---- controls */
    {
        track_t *t = &trk[0];
        int32_t dry, wet;
        reset_all();
        host_preset(t, 9, 0);
        trk_note_on(t, 60, 110);
        render(ms(40));
        trk_note_off(t, 60);
        render(ms(300));                                /* the release ends; the first echo is at 375 ms */
        dry = render(ms(150));
        reset_all();
        host_preset(t, 9, 0);
        t->p[P_DLY] = 127;
        trk_note_on(t, 60, 110);
        render(ms(40));
        trk_note_off(t, 60);
        render(ms(300));
        wet = render(ms(150));
        CHECK(wet > 4 * (dry + 50), "controls: DLY 127 is the core's Echo: 340..490 ms after a tap, peak %d against %d "
              "dry", wet, dry);
    }
    {
        track_t *t = &trk[0];
        int32_t muted, quiet;
        reset_all();
        host_preset(t, 12, 0);
        t->p[P_MUTE] = 1;
        trk_note_on(t, 60, 100);
        muted = render(ms(200));
        trk_note_off(t, 60);
        t->p[P_MUTE] = 0;
        reset_all();
        host_preset(t, 12, 0);
        t->p[P_LEVEL] = 0;
        trk_note_on(t, 60, 100);
        quiet = render(ms(200));
        trk_note_off(t, 60);
        CHECK(muted == 0 && quiet == 0, "controls: MUTE peak %d, LEVEL 0 peak %d", muted, quiet);
    }
    {
        track_t *t = &trk[0];
        static const uint8_t chord[8] = {48, 52, 55, 59, 62, 65, 69, 72};
        uint32_t e0 = gnc.errors, l0 = gnc.lost, n;
        reset_all();
        for (k = 0; k < NTRK; k++)
            host_preset(&trk[k], 12, 0);
        render(2);
        for (i = 0; i < 8u; i++)
            trk_note_on(t, chord[i], 100);
        for (k = 0; k < NTRK; k++) {                    /* every control of every track moves in the same block */
            trk[k].p[P_E0] = 100;
            trk[k].p[P_E1] = 30;
            trk[k].p[P_DLY] = 40;
            trk[k].p[P_REV] = 50;
            trk[k].p[P_CHOR] = 20;
            trk[k].p[P_LEVEL] = 90;
        }
        render(1);
        n = core_voices();
        render(ms(300));
        for (i = 0; i < 8u; i++)
            trk_note_off(t, chord[i]);
        render(ms(3000));
        CHECK(n == 8 && gnc.errors == e0 && gnc.lost == l0 && core_voices() == 0, "controls: an 8-note chord and 24 "
              "control changes in one block: %u core voices, rejected renders %u, lost events %u, voices after 3 s %u",
              n, gnc.errors - e0, gnc.lost - l0, core_voices());
    }

    /* ---- queue: the consumer runs between a slot and its index (GN_CORE_PREEMPT) */
    {
        static const uint8_t chord[8] = {48, 52, 55, 59, 62, 65, 69, 72};
        uint32_t e0 = gnc.errors, l0 = gnc.lost, round, ok = 1, n;
        reset_all();
        host_preset(&trk[0], 9, 0);
        host_preset(&trk[1], 12, 0);
        render(2);
        preempt_on = 1;
        for (round = 0; round < 20u; round++) {
            for (i = 0; i < 4u; i++) {
                trk_note_on(&trk[0], chord[i], 100);
                trk_note_on(&trk[1], chord[4u + i], 90);
            }
            render(1);
            n = core_voices();
            ok &= n == 8u;
            for (i = 0; i < 4u; i++) {
                trk_note_off(&trk[0], chord[i]);
                trk_note_off(&trk[1], chord[4u + i]);
            }
            render(ms(40));
        }
        preempt_on = 0;
        render(ms(3000));
        CHECK(ok && preempts > 100u && gnc.errors == e0 && gnc.lost == l0 && core_voices() == 0, "queue: 20 rounds of "
              "8 notes, the consumer interrupting %u times between a slot and its index: every chord's 8 notes in the "
              "core, rejected %u, lost %u, voices after 3 s %u", preempts, gnc.errors - e0, gnc.lost - l0, core_voices());
    }

    /* ---- orphans: tokens voice.c ends without done() */
    {
        track_t *t = &trk[0];
        uint32_t o0, v1, v2, v3;
        reset_all();
        host_preset(t, 9, 0);
        trk_note_on(t, 60, 100);
        render(ms(50));
        o0 = gnc.orphans;
        for (i = 0; i < NVOICE; i++)
            if (t->v[i].active) {
                t->v[i].active = 0;                     /* (as an engine switch's clear or the ADSR's end) */
                t->v[i].stage = 0;
            }
        render(ms(1000));
        v1 = core_voices();
        trk_note_on(t, 62, 100);
        render(ms(50));
        for (i = 0; i < NVOICE; i++)
            if (t->v[i].active)
                voice_kill(&t->v[i]);                   /* given to another part: ends in the ADSR, before done() */
        render(ms(1000));
        v2 = core_voices();
        trk_note_on(t, 64, 100);
        render(ms(50));
        panic_req |= 1u;                                /* a sound change's panic */
        render(ms(1000));
        v3 = core_voices();
        CHECK(!v1 && !v2 && !v3 && gnc.orphans >= o0 + 2u, "orphans: cleared %u, killed %u, panicked %u core voices "
              "left a second later (%u NOTE_OFFs sent for tokens ended without done)", v1, v2, v3, gnc.orphans - o0);
    }
    {
        static const uint8_t chord[8] = {48, 52, 55, 59, 62, 65, 69, 72};
        uint32_t s0, rel, fel, after;
        reset_all();
        host_preset(&trk[0], 9, 0);
        host_preset(&trk[1], 12, 32);                   /* FM 72 Lamplight, polyphonic (the shipping build sounds no Felucca engine) */
        trk[1].p[P_VOICE] = V_POLY;
        trk[0].p[P_REV] = 0;
        for (i = 0; i < 8u; i++)
            trk_note_on(&trk[0], chord[i], 100);
        render(ms(200));
        for (i = 0; i < 8u; i++)
            trk_note_off(&trk[0], chord[i]);
        render(2);
        rel = core_voices();
        s0 = gnc.steals;
        for (i = 0; i < 8u; i++)
            trk_note_on(&trk[1], chord[i], 100);       /* another track takes the releasing tokens */
        render(2);
        fel = tokens(&trk[1]);
        after = core_voices();
        CHECK(rel == 8 && fel == 8 && !tokens(&trk[0]) && after == 8 && after <= fel && gnc.steals > s0, "budget: 8 "
              "releasing VA voices, then 8 FM notes on another track take their tokens: the VA tails cut (%u adapter "
              "cuts), %u core voices for %u FM tokens, none left on VA (%u)", gnc.steals - s0, after, fel,
              tokens(&trk[0]));
        for (i = 0; i < 8u; i++)
            trk_note_off(&trk[1], chord[i]);
    }

    /* ---- fm6: a 1.0.1 project's patch, FM6 retired (project.c: unpack + set at a load, pack at a save) */
    {
        uint8_t packed[FM6_PACKED], v[FP_SIZE + 1u], back[FM6_PACKED];
        for (i = 0; i < FM6_PACKED; i++)
            packed[i] = (uint8_t)(i * 37u + 11u);
        fm6_unpack(packed, v);
        fm6_set_patch(2, v);
        fm6_pack(fm6_patch[2], back);
        CHECK(!memcmp(packed, back, sizeof back), "fm6: a project's FM6 patch round-trips byte for byte");
    }

    /* ---- idle */
    {
        uint32_t r0;
        reset_all();
        render(ms(1200));
        r0 = gnc.renders;
        render(ms(500));
        CHECK(gnc.renders == r0, "idle: no core render after a silent second (%u renders in the next 0.5 s)",
              gnc.renders - r0);
    }

    /* ---- cost */
    {
        static const uint8_t chord[8] = {48, 52, 55, 59, 62, 65, 69, 72};
        uint64_t t0, tf, ta;
        uint32_t nb = ms(2000);
        reset_all();
        host_preset(&trk[0], 12, 0);
        trk[0].p[P_DLY] = 64;
        trk[0].p[P_REV] = 64;
        for (i = 0; i < 8u; i++)
            trk_note_on(&trk[0], chord[i], 100);
        render(4);
        t0 = now_ns();
        render(nb);
        tf = now_ns() - t0;
        for (i = 0; i < 8u; i++)
            trk_note_off(&trk[0], chord[i]);
        reset_all();
        host_preset(&trk[0], GN_DROP_ANALOG ? 9 : 0, 0);   /* VA dry (ANALOG when the build keeps it) */
        for (i = 0; i < 8u; i++)
            trk_note_on(&trk[0], chord[i], 100);
        render(4);
        t0 = now_ns();
        render(nb);
        ta = now_ns() - t0;
        for (i = 0; i < 8u; i++)
            trk_note_off(&trk[0], chord[i]);
        printf("  cost  host ns per 32-frame block: 8 FM voices + echo + reverb %.0f, 8 %s voices %.0f (x%.2f; a "
               "proxy: the block's budget on the synth is 726 us)\n", (double)tf / nb, GN_DROP_ANALOG ? "VA" : "ANALOG",
               (double)ta / nb,
               (double)tf / (double)(ta ? ta : 1));
    }
    printf(failures ? "GNOMAD ENGINES: %d FAILED\n" : "GNOMAD ENGINES: ALL PASSED\n", failures);
    return failures ? 1 : 0;
}
