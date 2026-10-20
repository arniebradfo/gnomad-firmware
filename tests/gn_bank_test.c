/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kyle Palaniuk */
/* Gnomad (FORK.md, "Gnomad engines", round FW-CONTENT-4) host test: the Gnomad engines' presets are the factory banks'
 * (firmware/gnomad-content), through the firmware's own code (tests/gn_host.h with the swap on: the editor, Easy, the
 * shim and the core), every expectation keyed by bank ID or slot and every name read from the bank files at run time
 * (names may be renamed in the banks: the test follows the bytes):
 *   flash     GN_CONTENT_PITCHED / _KITS / _PCM are the vendored files' bytes; the core boots with the three assets
 *   census    each engine's whole list (gn_core.c GNC_LIST all) is exactly the records the core's own validator
 *             accepts for that engine, in bank order (gc_validate_patch / gc_validate_kit on all 384 + 32 slots): no
 *             valid one missing, no invalid one listed; it offers the first GN_PRESETS_MAX (round FW-CONTENT-5)
 *   NAMES     the editor's NAMES for slots 9 (VA), 12 (FM), 10 (DRUMS), 7 (GNOME), 13 and 8 (SAMPLER): the engine
 *             offers min(authored, 127) presets; the reply holds the first of them as whole names, each the record's
 *             (12 characters, as NAMES sends a name), then the two titles, within the editor's 600 bytes, and as many
 *             as fit (round FW-CONTENT-5: the count is the names it holds)
 *   bank      every authored record of the banks (384) and every kit (32), offered or not, bound through the shim
 *             (gn_easy.c gn_preset_put, the path Easy binds by: PRESET's own for an offered preset) on its engine: the
 *             core is given the bank's bytes at that ID and a note sounds (round FW-CONTENT-6)
 *   PRESET    every preset of every Gnomad engine through the editor's PRESET: the reply echoes it, the record the
 *             core was given (gn_core.c's bind, GN_CORE_BOUND) is the bank's bytes at that ID (a kit: the kit's 416
 *             bytes with the core's drum patch), the controls the shim sends are the record's stored macros (within
 *             a knob step), and a note sounds
 *   kits      every kit's 16 pads each sound on their keys (36..51)
 *   Easy      each Easy sound on a Gnomad slot plays the bank ID FORK.md lists (round FW-CONTENT-3), bound and
 *             sounding, its Bright the record's, the track's preset its place in the whole list, also past the
 *             offered list (round FW-CONTENT-6: bound by bank ID, never wrapped); the sounds on Felucca's kept engines are the engines-off build's
 *             (engine and preset), compared with that build's dump
 *   tests/run_fork_tests.sh builds it three times: gn_bank_test_off (-DGN_BANK_OFF) DUMP, then gn_bank_test DUMP,
 *   then gn_bank_test_synth (-DGN_BANK_SYNTH, round FW-CONTENT-5: the 127-preset cap, below; since round
 *   FW-CONTENT-6 the real FM list is 192 long and over the cap too, the synthetic one stays as the cap's own proof)
 *
 * GN_BANK_SYNTH (FORK.md, round FW-CONTENT-5): FM with 200 authored sounds, more than NAMES and PRESET's 7-bit
 * numbers reach. The test includes gn_content.h first (its #pragma once makes gn_core.c's include a no-op) and then
 * points the pitched bank and FM's list at its own: the snapshot's 384 slots copied, and 200 FM records appended at
 * IDs 384..583 (the shim indexes the bank by ID; the core never sees one), each sealed in the core's record format
 * (gnomad_core.h's 128 bytes: an authored FM record as the template, a name of its own, CRC32C over the record with
 * bytes 12..15 zero) and accepted by the core's own validator. Then:
 *   cap       FM offers 127 presets, the first 127 in bank order; the other engines as before
 *   UI        the device's preset list for FM (KNOB 2's list, preset_shown) is the same 127, wrapping 126 <-> 0
 *   PRESET    126 binds the 127th record; 127 is handled as an out-of-range preset is today (VA's 16 shows it): the
 *             track's preset is taken modulo the count, so preset 0 is bound and echoed, never the 128th record
 *   NAMES     never past the editor's 600-byte reply, ending on a whole name and the two titles; its count is the
 *             names it holds (12-character names: 44 of the 127, the protocol's "count, count names"), each the
 *             device's preset of that number; short names: all 127 */
#include <ctype.h>
#ifndef GN_BANK_OFF
#define GN_ENGINES 1
#include "../firmware/src/gn_engines.h"
#include <stdint.h>
static void bound_hook(uint32_t ti, const uint8_t *patch, const uint8_t *kit);
#define GN_CORE_BOUND(ti, patch, kit) bound_hook(ti, patch, kit)
#endif
#ifdef GN_BANK_SYNTH
#include "../firmware/gnomad-content/gn_content.h"     /* first: gn_core.c's own include of it is then a no-op */
#define SYN_N 200u                                      /* FM records in the synthetic bank */
#define SYN_BASE 384u                                   /* their IDs: SYN_BASE.. (after the snapshot's 384 slots) */
#define SYN_REAL_ID(id, b, w, c, d, r) (id),
static const uint16_t syn_real_fm[] = {GN_CONTENT_FM(SYN_REAL_ID)};   /* the snapshot's FM list (the template) */
static const uint8_t *const syn_real_pitched = GN_CONTENT_PITCHED;
static uint8_t syn_pitched[(SYN_BASE + SYN_N) * 128u];
#undef GN_CONTENT_FM
#undef GN_CONTENT_FM_COUNT
#define SYN1(X, i) X(SYN_BASE + (i), 64, 0, 0, 0, 0)
#define SYN10(X, i) SYN1(X, (i)) SYN1(X, (i) + 1) SYN1(X, (i) + 2) SYN1(X, (i) + 3) SYN1(X, (i) + 4) \
                    SYN1(X, (i) + 5) SYN1(X, (i) + 6) SYN1(X, (i) + 7) SYN1(X, (i) + 8) SYN1(X, (i) + 9)
#define SYN100(X, i) SYN10(X, (i)) SYN10(X, (i) + 10) SYN10(X, (i) + 20) SYN10(X, (i) + 30) SYN10(X, (i) + 40) \
                     SYN10(X, (i) + 50) SYN10(X, (i) + 60) SYN10(X, (i) + 70) SYN10(X, (i) + 80) SYN10(X, (i) + 90)
#define GN_CONTENT_FM(X) SYN100(X, 0) SYN100(X, 100)
#define GN_CONTENT_FM_COUNT SYN_N
#define GN_CONTENT_PITCHED syn_pitched
#endif
#include "gn_host.h"
#include <stdio.h>

static int fails;
#define CHECK(c, ...) do { if (c) { printf("  ok    "); } else { printf("  FAIL  "); fails++; } \
                           printf(__VA_ARGS__); printf("\n"); } while (0)

#ifdef GN_BANK_OFF
/* the engines-off build: Easy's sounds as Felucca's engines play them, for the swap build to compare */
int main(int argc, char **argv)
{
    FILE *f;
    uint32_t i;
    if (argc != 2 || !(f = fopen(argv[1], "w")))
        return 2;
    for (i = 0; i < GN_NSOUNDS; i++)
        fprintf(f, "%s\t%u\t%u\n", GN_SOUNDS[i].name, GN_SOUNDS[i].engine, GN_SOUNDS[i].preset);
    fclose(f);
    printf("gn_bank_test_off: %u sounds of the engines-off build in %s\n", (unsigned)GN_NSOUNDS, argv[1]);
    return 0;
}
#else

static uint8_t bound_p[NTRK][GC_PATCH_BYTES], bound_k[NTRK][GC_KIT_BYTES];
static uint32_t bound_n[NTRK];
static void bound_hook(uint32_t ti, const uint8_t *patch, const uint8_t *kit)
{
    memcpy(bound_p[ti % NTRK], patch, GC_PATCH_BYTES);
    if (kit)
        memcpy(bound_k[ti % NTRK], kit, GC_KIT_BYTES);
    else
        memset(bound_k[ti % NTRK], 0, GC_KIT_BYTES);
    bound_n[ti % NTRK]++;
}

/* the bank files, read at run time */
static uint8_t pitched[384u * 128u], kits[32u * 416u], pcm[131072u];
static long slurp(const char *path, uint8_t *buf, long max)
{
    FILE *f = fopen(path, "rb");
    long n;
    if (!f)
        return -1;
    n = (long)fread(buf, 1, (size_t)max, f);
    if (fgetc(f) != EOF)
        n = -2;                                         /* (longer than expected) */
    fclose(f);
    return n;
}
#define DIR "firmware/gnomad-content/"
static long read_pcm(void)                              /* the manifest's assets, in its order */
{
    FILE *m = fopen(DIR "factory.manifest", "r");
    char line[512], name[64], path[160];
    long off = 0;
    unsigned id;
    if (!m)
        return -1;
    while (fgets(line, sizeof line, m))
        if (sscanf(line, "%u %63s", &id, name) == 2) {
            long n;
            snprintf(path, sizeof path, DIR "%s.pcm", name);
            n = slurp(path, pcm + off, (long)sizeof pcm - off);
            if (n < 0)
                return -1;
            off += n;
        }
    fclose(m);
    return off;
}
static void rec_name(const uint8_t *rec, char out[17])
{
    memcpy(out, rec + 16, 16);
    out[16] = 0;
}
static uint32_t rd16(const uint8_t *p);                 /* (gn_core.c's) */

static const struct { uint32_t slot; const char *name; int32_t ce; uint32_t note; } SLOTS[] = {
    {9, "VA", GC_VA, 60}, {12, "FM", GC_FM, 60}, {10, "DRUMS", GC_DRUMS, 36}, {7, "GNOME", GC_GNOME, 60},
    {13, "SAMPLER", GC_SAMPLE, 60}, {8, "SAMPLER", GC_SAMPLE, 60},
};

/* Easy's sounds on the Gnomad engines and the bank sound each plays (FORK.md, round FW-CONTENT-3; round CR: by the
 * bank sound's NAME, which a renumbered bank keeps; the test finds the record of that name in the vendored bank, there
 * must be exactly one, and checks it is the one Easy binds). Each line: Easy's name, a kit or not, the bank name */
static const struct { const char *easy; int kit; const char *bank; } EASY[] = {
    {"ORGAN", 0, "Parlor Organ"}, {"FULL ORGAN", 0, "Tin Harmonium"}, {"GOSPEL", 0, "Steady Tine"},
    {"JAZZ", 0, "Reed Keys"}, {"ROCK DRIVE", 0, "Copper Clav"}, {"TINE PIANO", 0, "Lamplight"},
    {"GLASS BELL", 0, "Ice Chimes"}, {"BELL TREE", 0, "Starlight Glock"}, {"MARIMBA", 0, "Tick Tock"},
    {"KALIMBA", 0, "Kalimba Seed"}, {"GLITCH", 0, "Robot Hiccup"}, {"FM BASS", 0, "Deep Current"},
    {"SHIMMER", 0, "Glass Harmonics"}, {"DRONE STRING", 0, "Warm Blanket"}, {"CLOUD", 0, "Morning Fog"},
    {"FROZEN", 0, "Northern Air"}, {"SOFT FLUTE", 0, "Glass Whistle"}, {"PIANO", 0, "Felt Hammer"},
    {"PLUCK", 0, "Snap Pea"}, {"HARP", 0, "Harp Drops"}, {"NYLON GUITAR", 0, "Silk Pluck"}, {"SAX", 0, "Reed Whistle"},
    {"BRASS", 0, "Brass Kite"}, {"KIT", 1, "Big Step"}, {"DRUM MACHINE", 1, "House"}, {"HAND DRUM", 1, "Latin"},
    {"TOMS", 1, "Woodshed"}, {"CHOPPED BREAK", 0, "Woodpile"},
#if GN_DROP_ANALOG && GN_DROP_PHASE && GN_DROP_LOFI && GN_DROP_VOICE && GN_DROP_TRIO && GN_DROP_NOISE
    /* round UX-R: the 21 on Felucca's seven engines the decision retired too (gn_state.c GN_FEL); round CR:
     * the five rebinds (RING BELL, CHIP CHOIR, FAT BASS, WIND, RAIN) and the two whose sounds 1.1.6 removed
     * (8-BIT ARP was Bright Square, STEP LEAD Twin Pulses) */
    {"GNOME", 0, "Sing Ah"}, {"VOX LEAD", 0, "Sing Oo"}, {"WHISPER", 0, "Whisper Oo"}, {"WOW BASS", 0, "Rumble Ah"},
    {"PHASE BELL", 0, "Tower Bell"}, {"RING BELL", 0, "Handbell"}, {"LASER", 0, "Laser Line"},
    {"PULSE", 0, "Pure Square"}, {"8-BIT ARP", 0, "Glass Square"}, {"STEP LEAD", 0, "Hollow Square"},
    {"CHIP CHOIR", 0, "Velvet Pulse"}, {"ARCADE", 0, "Space Zap"}, {"WHALE", 0, "Giant Ah"}, {"SUB BASS", 0, "Moss Sub"},
    {"SQUARE BASS", 0, "Square Root"}, {"FAT BASS", 0, "Thick Felt"}, {"CHOIR", 0, "Mushroom Choir"},
    {"SOFT PAD", 0, "Wool Pad"}, {"STRINGS", 0, "Soft Strings"}, {"WIND", 0, "Paper Lantern"}, {"RAIN", 0, "Rain Chime"},
    /* round CR: FUNNY, gnomad-core easy.toml's eight (FUNNY v2) */
    {"WHOOPEE CUSHION", 0, "Whoopee Cushion"}, {"BIG BURP", 0, "Big Burp"}, {"BIG BOING", 0, "Big Boing"},
    {"SLIDE WHISTLE", 0, "Slide Whistle"}, {"SAD TROMBONE", 0, "Sad Trombone"}, {"BOMB DROP", 0, "Bomb Drop"},
    {"DUCK QUACK", 0, "Duck Quack"}, {"PEW PEW", 0, "Pew Pew"},
#endif
};
/* the one record of the bank named nm (pitched or kits), its id; -1 none, -2 more than one */
static int32_t bank_id_of(const uint8_t *bank, uint32_t n, uint32_t size, const char *nm)
{
    int32_t found = -1;
    uint32_t i;
    for (i = 0; i < n; i++) {
        const uint8_t *r = bank + size * i;
        if (!r[0] || strncmp((const char *)r + 16, nm, 16) || strlen(nm) > 15)
            continue;
        if (found >= 0)
            return -2;
        found = (int32_t)i;
    }
    return found;
}

static int32_t peak_ms(uint32_t ms)                    /* the output's peak |L| over ms of audio */
{
    int32_t out[2u * CTL], peak = 0;
    uint32_t b, i, n = (ms * FS / 1000u + CTL - 1u) / CTL;
    for (b = 0; b < n; b++) {
        gn_core_host();                                 /* (round MF: the main loop's part, every block) */
        mix_block(out, CTL);
        for (i = 0; i < CTL; i++) {
            int32_t a = out[2u * i] < 0 ? -out[2u * i] : out[2u * i];
            peak = a > peak ? a : peak;
        }
    }
    return peak;
}
static uint32_t core_voices(void)
{
    gc_report r;
    gn_core_report(&r);
    return r.voices_active;
}
static void quiet(track_t *t)                           /* every note off, and the core silent (at most 5 s) */
{
    uint32_t b;
    trk_all_off(t);
    for (b = 0; b < 5000u * FS / 1000u / CTL && core_voices(); b++)
        peak_ms(1);
}
/* round MF: a render's spectrum in 24 log-spaced bands, 80 Hz .. 16 kHz, in dB (an FFT of its first 65,536 frames, a
 * Hann window): the band measure of the core's G14 gate ("band spectrum >= 3 dB"), cheap enough here */
#define MF_N 65536u
static void mf_bands(const int16_t *x, uint32_t n, double out[24])
{
    static double re[MF_N], im[MF_N];
    uint32_t i, j, k, len;
    for (i = 0; i < MF_N; i++) {
        re[i] = i < n ? x[i] * (0.5 - 0.5 * cos(2.0 * M_PI * i / (MF_N - 1u))) : 0.0;
        im[i] = 0.0;
    }
    for (i = 1, j = 0; i < MF_N; i++) {           /* bit reversal */
        uint32_t bit = MF_N >> 1;
        for (; j & bit; bit >>= 1)
            j ^= bit;
        j ^= bit;
        if (i < j) {
            double t = re[i]; re[i] = re[j]; re[j] = t;
            t = im[i]; im[i] = im[j]; im[j] = t;
        }
    }
    for (len = 2; len <= MF_N; len <<= 1) {
        double a = -2.0 * M_PI / len, wr = cos(a), wi = sin(a);
        for (i = 0; i < MF_N; i += len) {
            double cr = 1.0, ci = 0.0;
            for (k = 0; k < len / 2u; k++) {
                double ur = re[i + k], ui = im[i + k];
                double vr = re[i + k + len / 2u] * cr - im[i + k + len / 2u] * ci;
                double vi = re[i + k + len / 2u] * ci + im[i + k + len / 2u] * cr;
                double t;
                re[i + k] = ur + vr; im[i + k] = ui + vi;
                re[i + k + len / 2u] = ur - vr; im[i + k + len / 2u] = ui - vi;
                t = cr * wr - ci * wi; ci = cr * wi + ci * wr; cr = t;
            }
        }
    }
    for (k = 0; k < 24u; k++) {
        double lo = 80.0 * pow(200.0, k / 24.0), hi = 80.0 * pow(200.0, (k + 1u) / 24.0), e = 1.0;
        uint32_t b0 = (uint32_t)(lo * MF_N / FS), b1 = (uint32_t)(hi * MF_N / FS);
        for (i = b0; i < b1 && i < MF_N / 2u; i++)
            e += re[i] * re[i] + im[i] * im[i];
        out[k] = 10.0 * log10(e);
    }
}
/* the most a band of b differs from a, beyond what it differs between two takes of a (a, c), among the bands within
 * 40 dB of a's loudest */
static double mf_band_move(const double a[24], const double b[24], const double c[24])
{
    double top = -1e9, best = 0.0;
    uint32_t k;
    for (k = 0; k < 24u; k++)
        top = a[k] > top ? a[k] : top;
    for (k = 0; k < 24u; k++) {
        double d;
        if (a[k] < top - 40.0 && b[k] < top - 40.0)
            continue;
        d = fabs(b[k] - a[k]) - fabs(c[k] - a[k]);
        best = d > best ? d : best;
    }
    return best;
}

/* the controls the shim sends on track ti are the record's stored macros (P5 Bright, P6 Wobble, P7 Echo, P8 Space,
 * P9 Chorus), within half a knob step (Space: the send doubled, a step of 1,032). Round MF: a knob of the sound's own
 * (gn_core.c gnc_slot) instead rests exactly at its rest amount, and its writes there are the record's stored values,
 * word for word; the controls it owns are not sent from their settings */
static int macros_are(const track_t *t, const uint8_t *rec, char *why, size_t n)
{
    static const uint32_t K[5] = {GNC_BRIGHT, GNC_WOBBLE, GNC_ECHO, GNC_SPACE, GNC_CHORUS};
    static const int32_t TOL[5] = {261, 259, 259, 517, 259};
    int32_t ce = gnc_engine(t->engine);
    uint32_t k, owned = 0, s, i;
    for (s = 0; s < 4u; s++) {
        uint32_t store, rest, m;
        uint64_t par;
        gc_macro_write w[GC_MACRO_WRITES];
        if (!gnc_slot(rec, ce, s, &store, &par, &rest))
            continue;
        owned |= (uint32_t)((par >> 5) & 0x1Fu) | (s < 2u ? 1u << s : 0u);
        if (gnc_amount(t->p[store], rest, gnc_kr(rest)) != (int32_t)rest) {
            snprintf(why, n, "K%u (%s) at amount %d, its rest %u", s + 1u, gc_macro_label((const gc_patch *)rec, s),
                     gnc_amount(t->p[store], rest, gnc_kr(rest)), rest);
            return 0;
        }
        m = gc_macro_apply((const gc_patch *)rec, s, rest, w);
        for (i = 0; i < m; i++)
            if (w[i].value != (int32_t)rd16(rec + 32u + 2u * w[i].parameter) &&
                !((int16_t)rd16(rec + 32u + 2u * w[i].parameter) == w[i].value)) {
                snprintf(why, n, "K%u (%s) at rest writes P%u %d, stored %d", s + 1u,
                         gc_macro_label((const gc_patch *)rec, s), w[i].parameter, w[i].value,
                         (int32_t)rd16(rec + 32u + 2u * w[i].parameter));
                return 0;
            }
    }
    for (k = 0; k < 5u; k++) {
        int32_t want = (int32_t)rd16(rec + 32u + 2u * (5u + k)), got = gnc_ctl(t, K[k]);
        if ((owned >> k) & 1u)
            continue;
        if (got - want > TOL[k] || want - got > TOL[k]) {
            snprintf(why, n, "P%u sent %d, stored %d", 5u + k, got, want);
            return 0;
        }
    }
    snprintf(why, n, "macros as stored");
    return 1;
}

#ifdef GN_BANK_SYNTH
static uint32_t syn_crc32c(const uint8_t *p, uint32_t n)
{
    uint32_t c = 0xFFFFFFFFu, k;
    while (n--) {
        c ^= *p++;
        for (k = 0; k < 8u; k++)
            c = (c >> 1) ^ (c & 1u ? 0x82F63B78u : 0u);
    }
    return c ^ 0xFFFFFFFFu;
}
/* the synthetic records' names (fmt: "%03u Synthetic", 13 characters, NAMES sends 12; or "%03u"), resealed */
static uint32_t syn_seal(const char *fmt)
{
    uint32_t j, valid = 0, crc;
    gc_report r;
    for (j = 0; j < SYN_N; j++) {
        uint8_t *rec = &syn_pitched[128u * (SYN_BASE + j)];
        memset(rec + 16, 0, 16);
        snprintf((char *)rec + 16, 16, fmt, j);
        memset(rec + 12, 0, 4);
        crc = syn_crc32c(rec, 128);
        rec[12] = (uint8_t)crc, rec[13] = (uint8_t)(crc >> 8);
        rec[14] = (uint8_t)(crc >> 16), rec[15] = (uint8_t)(crc >> 24);
        valid += gc_validate_patch((const gc_patch *)rec, &r) == GC_OK && rec[8] == GC_FM;
    }
    return valid;
}
/* NAMES of slot 12 (FM) as upstream's editor parses it (web/editor.html: count, count strings, two titles): the
 * names it holds, 0 if the reply is malformed (past the buffer, a partial string, bytes after the titles) */
static uint32_t syn_names(uint32_t *len, uint32_t *differ)
{
    uint8_t a[1] = {GN_ENGI_FM};
    uint32_t n, p = 7, i, t;
    const engine_t *e = ENGINES[GN_ENGI_FM];
    *len = host_request(ED_NAMES, a, 1);
    *differ = 0;
    if (*len < 9 || *len > sizeof ed_out || host_wire[0] != 0xF0 || host_wire[4] != ED_NAMES ||
        host_wire[5] != GN_ENGI_FM || host_wire[*len - 1] != 0xF7)
        return 0;
    n = host_wire[6];
    for (i = 0; i < n + 2u; i++) {                      /* the names, then the titles: each whole, 0-terminated */
        char got[16];
        uint32_t j = 0;
        while (p < *len - 1u && host_wire[p] && j < sizeof got - 1u)
            got[j++] = (char)host_wire[p++];
        got[j] = 0;
        if (p >= *len - 1u || host_wire[p])
            return 0;                                   /* (no terminator before F7: a partial record) */
        p++;
        if (i < n) {                                    /* name i is the device's preset i, as NAMES sends it */
            char want[13];
            snprintf(want, sizeof want, "%s", GN_PRESET_NAME(e, i));
            *differ += strcmp(want, got) != 0;
        } else {
            t = i - n;
            *differ += strcmp(got, e->page_title[t]) != 0;
        }
    }
    return p == *len - 1u ? n : 0;                      /* (nothing between the titles and F7) */
}

int main(void)
{
    uint32_t i, ok, len, differ, n, total;
    const engine_t *fm = ENGINES[GN_ENGI_FM];
    const uint8_t *tmpl = &syn_real_pitched[128u * syn_real_fm[0]];
    track_t *t;
    printf("Gnomad engines: at most %u presets an engine (round FW-CONTENT-5), a synthetic FM bank of %u, "
           "GN_ENGINES 1\n", (unsigned)GN_PRESETS_MAX, (unsigned)SYN_N);
    memcpy(syn_pitched, syn_real_pitched, 384u * 128u);
    for (i = 0; i < SYN_N; i++)
        memcpy(&syn_pitched[128u * (SYN_BASE + i)], tmpl, 128);
    n = syn_seal("%03u Synthetic");
    CHECK(n == SYN_N, "bank: %u FM records sealed from the template (bank %u), %u accepted by the core's "
          "validator as FM", SYN_N, syn_real_fm[0], n);
    host_power_on();
    usb.config = 1;
    gn_core_boot();

    /* ---- cap */
    for (ok = 1, i = 0; i < GNC_LIST[GC_FM].n; i++)
        ok &= GNC_LIST[GC_FM].id[i] == SYN_BASE + i && GN_PRESET_NAME(fm, i) == (const char *)&syn_pitched[128u *
              (SYN_BASE + i) + 16u];
    CHECK(GNC_LIST[GC_FM].n == 127u && fm->npresets == 127u && ok, "cap: FM offers %u presets of %u authored "
          "(GN_PRESETS_MAX %u), bank IDs %u..%u, the first in bank order", fm->npresets, SYN_N,
          (unsigned)GN_PRESETS_MAX, SYN_BASE, SYN_BASE + 126u);
    CHECK(GNC_LIST[GC_VA].n == GN_CONTENT_VA_COUNT && ENGINES[GN_ENGI_VA]->npresets == GN_CONTENT_VA_COUNT &&
          GNC_LIST[GC_GNOME].n == GN_CONTENT_GNOME_COUNT && GNC_LIST[GC_SAMPLE].n == GN_CONTENT_SAMPLE_COUNT &&
          GNC_LIST[GC_DRUMS].n == GN_CONTENT_KITS_COUNT, "cap: the other engines' lists are the "
          "snapshot's (VA %u, DRUMS %u, GNOME %u, SAMPLER %u)", GNC_LIST[GC_VA].n, GNC_LIST[GC_DRUMS].n,
          GNC_LIST[GC_GNOME].n, GNC_LIST[GC_SAMPLE].n);

    /* ---- PRESET: an out-of-range preset today (VA has GN_CONTENT_VA_COUNT: PRESET that), then FM's 126 and 127 */
    {
        uint8_t a[2] = {GN_ENGI_VA, (uint8_t)GN_CONTENT_VA_COUNT};
        uint32_t before = bound_n[0];
        t = &trk[song.sel % NTRK];
        len = host_request(ED_PRESET, a, 2);
        peak_ms(5);
        CHECK(len >= 8 && host_wire[5] == GN_ENGI_VA && host_wire[6] == 0 && t->preset == 0 && bound_n[0] != before &&
              !memcmp(bound_p[0], &syn_pitched[128u * GNC_LIST[GC_VA].id[0]], 128), "PRESET VA %u (out of range "
              "today, %u presets): taken modulo the count, preset %u loaded, bound and echoed (%u)",
              (unsigned)GN_CONTENT_VA_COUNT, (unsigned)GN_CONTENT_VA_COUNT, t->preset,
              len >= 8 ? host_wire[6] : 255u);
    }
    {
        uint8_t a[2] = {GN_ENGI_FM, 126};
        uint32_t before = bound_n[0];
        int32_t pk;
        len = host_request(ED_PRESET, a, 2);
        peak_ms(5);
        t = &trk[song.sel % NTRK];
        CHECK(len >= 8 && host_wire[5] == GN_ENGI_FM && host_wire[6] == 126 && t->engine == GN_ENGI_FM &&
              t->preset == 126 && bound_n[0] != before &&
              !memcmp(bound_p[0], &syn_pitched[128u * (SYN_BASE + 126u)], 128), "PRESET FM 126: echoed %u, the "
              "127th record (bank %u, \"%.12s\") bound", len >= 8 ? host_wire[6] : 255u, SYN_BASE + 126u,
              GN_PRESET_NAME(fm, 126));
        t->p[P_LEVEL] = 100;
        trk_note_on(t, 60, 100);
        pk = peak_ms(150);
        quiet(t);
        CHECK(pk >= 300 && !gnc.errors, "PRESET FM 126: a note sounds (peak %d), no bind error", pk);
    }
    {
        uint8_t a[2] = {GN_ENGI_FM, 127};
        uint32_t before = bound_n[0];
        len = host_request(ED_PRESET, a, 2);
        peak_ms(5);
        t = &trk[song.sel % NTRK];
        CHECK(len >= 8 && host_wire[5] == GN_ENGI_FM && host_wire[6] == 0 && t->preset == 0 && bound_n[0] != before &&
              !memcmp(bound_p[0], &syn_pitched[128u * SYN_BASE], 128) &&
              memcmp(bound_p[0], &syn_pitched[128u * (SYN_BASE + 127u)], 128), "PRESET FM 127: as VA's above, "
              "modulo the count: preset %u bound and echoed (%u), never the 128th record (bank %u)", t->preset,
              len >= 8 ? host_wire[6] : 255u, SYN_BASE + 127u);
    }

    /* ---- the device's list: KNOB 2 (eng_list_pos / eng_list_step) and the browser's preset_shown */
    {
        uint8_t a[2] = {GN_ENGI_FM, 126};
        uint32_t used = 0, k, wrap_up, wrap_down;
        for (k = 0; k < UP_SLOTS; k++)
            used += up_used(k) && up_engine(k) == GN_ENGI_FM;
        host_request(ED_PRESET, a, 2);
        eng_list_pos(&total);
        eng_list_step(1);
        wrap_up = TSEL->preset;
        eng_list_step(-1);
        wrap_down = TSEL->preset;
        CHECK(used == 0 && total == 127u && preset_shown(GN_ENGI_FM) == 127u && wrap_up == 0 && wrap_down == 126,
              "UI: FM's preset list is %u long (preset_shown %u, %u user presets), 126 steps to %u and back to %u",
              total, (unsigned)preset_shown(GN_ENGI_FM), used, wrap_up, wrap_down);
    }

    /* ---- NAMES: within the reply, whole names, the device's own */
    n = syn_names(&len, &differ);
    CHECK(n == 44u && !differ && len <= sizeof ed_out && 7u + 13u * (n + 1u) + 8u + 1u > sizeof ed_out,
          "NAMES FM (12-character names): %u whole names of the device's 127 in %u bytes of %u, each the device's "
          "preset of that number, then the titles (%u differ); a 45th would not fit", n, len,
          (unsigned)sizeof ed_out, differ);
    syn_seal("%03u");
    n = syn_names(&len, &differ);
    CHECK(n == 127u && n == fm->npresets && n == preset_shown(GN_ENGI_FM) && !differ && len <= sizeof ed_out,
          "NAMES FM (3-character names): all %u, the device's list (%u), in %u bytes (%u differ)", n,
          (unsigned)preset_shown(GN_ENGI_FM), len, differ);

    printf(fails ? "GNOMAD PRESET CAP: %d FAILED\n" : "GNOMAD PRESET CAP: ALL PASSED\n", fails);
    return fails ? 1 : 0;
}
#else

/* round FW-CONTENT-8: the power-on sounds (FORK.md's table; the firmware's is gn_state.c GN_POWER_ON); round CR: by
 * the bank sound's name, its id found in the vendored bank at the start (main) */
static struct { uint32_t slot; int32_t ce; const char *name; uint32_t id; } POWER_ON[NTRK] = {
    {GN_ENGI_VA, GC_VA, "Acid Lead", 0}, {GN_ENGI_FM, GC_FM, "Lamplight", 0}, {GN_ENGI_GNOME, GC_GNOME, "Sing Ah", 0},
    {GN_ENGI_DRUMS, GC_DRUMS, "Big Step", 0}};
static int32_t bank_id_of(const uint8_t *bank, uint32_t n, uint32_t size, const char *nm);
/* how many tracks hold their power-on sound: the engine (asked and playing), the bank ID at the track's preset, the
 * record's bytes bound by the core, the steps empty, and a note sounds; what each holds into out */
static uint32_t po_check(char *out, size_t n)
{
    uint32_t i, ok = 0;
    size_t at = 0;
    out[0] = 0;
    for (i = 0; i < NTRK; i++) {
        track_t *t = &trk[i];
        int32_t ce = POWER_ON[i].ce, pk;
        uint32_t id = t->preset < GNC_LIST[ce].all ? GNC_LIST[ce].id[t->preset] : 0xFFFFu, k, empty = 1;
        int bytes = ce == GC_DRUMS ? !memcmp(bound_k[i], &kits[416u * POWER_ON[i].id], 416) &&
                                     !memcmp(bound_p[i], GN_CORE_PATCH[GC_DRUMS], 128)
                                   : !memcmp(bound_p[i], &pitched[128u * POWER_ON[i].id], 128);
        char nm[17];
        for (k = 0; k < NSTEP; k++)
            empty &= t->step[k].time == ST_REST;
        quiet(t);
        trk_note_on(t, ce == GC_DRUMS ? 36u : 60u, 100);
        pk = peak_ms(150);
        quiet(t);
        rec_name(ce == GC_DRUMS ? &kits[416u * POWER_ON[i].id] : &pitched[128u * POWER_ON[i].id], nm);
        if (t->eng_req == POWER_ON[i].slot && t->engine == POWER_ON[i].slot && id == POWER_ON[i].id && bytes && empty &&
            pk >= 300) {
            ok++;
            if (at < n)
                at += (size_t)snprintf(out + at, n - at, "%strack %u %s %u \"%s\"", i ? ", " : "", i + 1u,
                                       ce == GC_DRUMS ? "kit" : "bank", POWER_ON[i].id, nm);
        } else if (at < n)
            at += (size_t)snprintf(out + at, n - at, "%strack %u NOT (engine %u/%u preset %u id %u bytes %d steps %u "
                                   "peak %d)", i ? ", " : "", i + 1u, t->eng_req, t->engine, t->preset, id, bytes, empty, pk);
    }
    return ok;
}

int main(int argc, char **argv)
{
    uint32_t i, k, s, ok, n;
    long np = slurp(DIR "pitched.gcpbank", pitched, sizeof pitched), nk = slurp(DIR "kits.gckbank", kits, sizeof kits);
    long npcm = read_pcm();
    char why[96];
    printf("Gnomad engines: the presets are the banks' (firmware/gnomad-content), GN_ENGINES 1\n");
    for (i = 0; i < NTRK; i++) {                        /* (round CR: the power-on sounds' ids, by name) */
        int32_t id = POWER_ON[i].ce == GC_DRUMS ? bank_id_of(kits, 32u, 416u, POWER_ON[i].name)
                                               : bank_id_of(pitched, 384u, 128u, POWER_ON[i].name);
        CHECK(id >= 0, "power-on track %u: the bank has one record named \"%s\" (id %d)", i + 1u, POWER_ON[i].name, id);
        POWER_ON[i].id = id < 0 ? 0u : (uint32_t)id;
    }
    host_power_on();
    usb.config = 1;                                     /* (USB up: a reply longer than the queue waits for it, as on the synth) */

    /* ---- flash */
    CHECK(np == (long)sizeof pitched && nk == (long)sizeof kits && npcm == (long)sizeof pcm &&
          !memcmp(GN_CONTENT_PITCHED, pitched, sizeof pitched) && !memcmp(GN_CONTENT_KITS, kits, sizeof kits) &&
          !memcmp(GN_CONTENT_PCM, pcm, sizeof pcm), "flash: the arrays are the vendored files (pitched %ld B, kits "
          "%ld B, PCM %ld B)", np, nk, npcm);
    gn_core_boot();
    CHECK(gnc.core && gnc.init_status == GC_OK && GN_CONTENT_NASSETS == 3u, "flash: the core boots with the factory "
          "PCM's %u assets from flash (status %u)", (unsigned)GN_CONTENT_NASSETS, gnc.init_status);

    /* ---- census: the core's validator decides what is authored */
    for (s = 0; s < 5u; s++) {
        uint32_t want[384], nw = 0, same;
        gc_report r;
        if (s == GC_DRUMS) {
            for (i = 0; i < 32u; i++)
                if (gc_validate_kit((const gc_kit *)&kits[416u * i], &r) == GC_OK)
                    want[nw++] = i;
        } else {
            for (i = 0; i < 384u; i++)
                if (gc_validate_patch((const gc_patch *)&pitched[128u * i], &r) == GC_OK && pitched[128u * i + 8u] == s)
                    want[nw++] = i;
        }
        same = nw == GNC_LIST[s].all && GNC_LIST[s].n == (nw < GN_PRESETS_MAX ? nw : GN_PRESETS_MAX);
        for (i = 0; same && i < nw; i++)
            same = GNC_LIST[s].id[i] == want[i];
        CHECK(same && nw > 0, "census: engine %u (gc_engine): %u authored by the core's validator, %u listed, the same "
              "IDs in bank order; %u offered", s, nw, GNC_LIST[s].all, GNC_LIST[s].n);
    }
    CHECK(GNC_LIST[GC_VA].all + GNC_LIST[GC_FM].all + GNC_LIST[GC_GNOME].all + GNC_LIST[GC_SAMPLE].all == 384u &&
          GNC_LIST[GC_DRUMS].all == 32u, "census: the whole catalogue: VA %u, FM %u, GNOME %u, SAMPLER %u (384 pitched), "
          "%u kits", GNC_LIST[GC_VA].all, GNC_LIST[GC_FM].all, GNC_LIST[GC_GNOME].all, GNC_LIST[GC_SAMPLE].all,
          GNC_LIST[GC_DRUMS].all);
    {
        gc_report r;
        const uint8_t *d = GN_CORE_PATCH[GC_DRUMS];
        CHECK(gc_validate_patch((const gc_patch *)d, &r) == GC_OK && d[8] == GC_DRUMS && rd16(d + 42) == 32768u &&
              !rd16(d + 44) && rd16(d + 46) == 16384u && rd16(d + 48) == 16384u && rd16(d + 50) == 16384u,
              "census: the drum patch a kit binds with stores Bright 32768, Wobble 0, the sends 16384 (the kits' "
              "knobs, tools/gn_gen_content.py KIT_MACROS)");
    }

    /* ---- NAMES */
    for (k = 0; k < NELEM(SLOTS); k++) {
        uint8_t a[1] = {(uint8_t)SLOTS[k].slot};
        uint32_t len = host_request(ED_NAMES, a, 1), p = 7, bad = 0, cnt, whole = 1, more = 0;
        int32_t ce = SLOTS[k].ce;
        const engine_t *e = ENGINES[SLOTS[k].slot];
        char nm[17], got[16];
        cnt = len > 6 ? host_wire[6] : 0;
        for (i = 0; i < cnt + 2u && whole; i++) {       /* the names, then the two titles: each whole, 0-terminated */
            uint32_t j = 0;
            if (i < cnt) {
                rec_name(ce == GC_DRUMS ? &kits[416u * GNC_LIST[ce].id[i]] : &pitched[128u * GNC_LIST[ce].id[i]], nm);
                nm[12] = 0;                             /* (NAMES sends 12 characters) */
            } else
                snprintf(nm, sizeof nm, "%s", e->page_title[i - cnt]);
            while (p + 1u < len && host_wire[p] && j < sizeof got - 1u)
                got[j++] = (char)host_wire[p++];
            got[j] = 0;
            whole = p + 1u < len && !host_wire[p];
            p++;
            bad += strcmp(nm, got) != 0;
        }
        whole &= p == len - 1u;                         /* (nothing between the titles and F7) */
        if (cnt < GNC_LIST[ce].n) {                     /* the next name would not have fitted */
            rec_name(ce == GC_DRUMS ? &kits[416u * GNC_LIST[ce].id[cnt]] : &pitched[128u * GNC_LIST[ce].id[cnt]], nm);
            nm[12] = 0;
            more = len + strlen(nm) + 1u > sizeof ed_out;
        }
        CHECK(len > 7 && host_wire[0] == 0xF0 && host_wire[4] == ED_NAMES && host_wire[5] == SLOTS[k].slot &&
              e->npresets == GNC_LIST[ce].n && whole && !bad && host_wire[len - 1] == 0xF7 && len <= sizeof ed_out &&
              (cnt == GNC_LIST[ce].n || (cnt < GNC_LIST[ce].n && more)), "NAMES %s (slot %u): %u offered of %u "
              "authored; the reply holds %u whole names, the records' own, then the titles (%u differ), %u bytes of the "
              "editor's %u%s", SLOTS[k].name, SLOTS[k].slot, GNC_LIST[ce].n, GNC_LIST[ce].all, cnt, bad, len,
              (unsigned)sizeof ed_out, cnt < GNC_LIST[ce].n ? " (the next would not fit)" : "");
    }

    /* ---- PRESET: every preset of every Gnomad engine, the bytes the core was given */
    for (k = 0; k < NELEM(SLOTS); k++) {
        int32_t ce = SLOTS[k].ce;
        uint32_t bad = 0, silent = 0, macros = 0;
        char first[96] = "";
        for (i = 0; i < GNC_LIST[ce].n; i++) {
            uint8_t a[2] = {(uint8_t)SLOTS[k].slot, (uint8_t)i};
            uint32_t len = host_request(ED_PRESET, a, 2), id = GNC_LIST[ce].id[i], before = bound_n[0];
            track_t *t = &trk[song.sel % NTRK];
            int32_t pk;
            peak_ms(5);
            if (len < 8 || host_wire[5] != SLOTS[k].slot || host_wire[6] != i || t->engine != SLOTS[k].slot ||
                t->preset != i || bound_n[0] == before ||
                (ce == GC_DRUMS ? memcmp(bound_k[0], &kits[416u * id], 416) || memcmp(bound_p[0], GN_CORE_PATCH[GC_DRUMS], 128)
                                : memcmp(bound_p[0], &pitched[128u * id], 128))) {
                if (!bad++)
                    snprintf(first, sizeof first, "preset %u (bank %u): reply %u B, engine %u preset %u, binds %u", i, id,
                             len, t->engine, t->preset, bound_n[0] - before);
                continue;
            }
            if (!macros_are(t, ce == GC_DRUMS ? GN_CORE_PATCH[GC_DRUMS] : &pitched[128u * id], why, sizeof why)) {
                if (!macros++ && !bad)
                    snprintf(first, sizeof first, "preset %u (bank %u): %s", i, id, why);
            }
            t->p[P_LEVEL] = 100;
            trk_note_on(t, SLOTS[k].note, 100);
            pk = peak_ms(150);
            silent += pk < 300;
            if (pk < 300 && !bad && !macros)
                snprintf(first, sizeof first, "preset %u (bank %u) peak %d", i, id, pk);
            quiet(t);
        }
        CHECK(!bad && !macros && !silent && !gnc.errors, "PRESET %s: %u presets, each echoed, its record's bytes bound (%u "
              "not), the macros the record's (%u not), a note sounds (%u silent)%s%s", SLOTS[k].name, GNC_LIST[ce].n, bad,
              macros, silent, first[0] ? ": " : "", first);
    }

    /* ---- the whole bank: every authored record and kit, offered or not, bound through the shim (gn_preset_put) */
    for (k = 0; k < 5u; k++) {                         /* SLOTS[0..4]: one slot an engine */
        int32_t ce = SLOTS[k].ce;
        uint32_t bad = 0, silent = 0, past = 0;
        char first[96] = "";
        track_t *t = &trk[0];
        for (i = 0; i < GNC_LIST[ce].all; i++) {
            uint32_t id = GNC_LIST[ce].id[i], before = bound_n[0];
            int32_t pk;
            gc_report r;
            int valid = ce == GC_DRUMS ? gc_validate_kit((const gc_kit *)&kits[416u * id], &r) == GC_OK
                                       : gc_validate_patch((const gc_patch *)&pitched[128u * id], &r) == GC_OK;
            if (t->eng_req != SLOTS[k].slot)
                set_engine_of(t, SLOTS[k].slot);
            gn_preset_put(t, i);
            peak_ms(5);
            past += i >= GNC_LIST[ce].n;
            if (!valid || t->engine != SLOTS[k].slot || t->preset != i || bound_n[0] == before ||
                (ce == GC_DRUMS ? memcmp(bound_k[0], &kits[416u * id], 416) || memcmp(bound_p[0], GN_CORE_PATCH[GC_DRUMS], 128)
                                : memcmp(bound_p[0], &pitched[128u * id], 128))) {
                if (!bad++)
                    snprintf(first, sizeof first, "place %u (bank %u): valid %d, engine %u preset %u, binds %u", i, id,
                             valid, t->engine, t->preset, bound_n[0] - before);
                continue;
            }
            t->p[P_LEVEL] = 100;
            trk_note_on(t, SLOTS[k].note, 100);
            pk = peak_ms(60);
            silent += pk < 300;
            if (pk < 300 && !bad && silent == 1u)
                snprintf(first, sizeof first, "place %u (bank %u) peak %d", i, id, pk);
            quiet(t);
        }
        CHECK(!bad && !silent && !gnc.errors, "bank %s: all %u authored %s (%u past the offered %u) validate, bind "
              "their bytes through the shim and sound (%u not bound, %u silent)%s%s", SLOTS[k].name, GNC_LIST[ce].all,
              ce == GC_DRUMS ? "kits" : "records", past, GNC_LIST[ce].n, bad, silent, first[0] ? ": " : "", first);
    }

    /* ---- kits: the sixteen pads */
    for (i = 0; i < GNC_LIST[GC_DRUMS].n; i++) {
        uint8_t a[2] = {10, (uint8_t)i};
        track_t *t = &trk[song.sel % NTRK];
        uint32_t sounding = 0, pad;
        int32_t low = 1 << 30;
        char nm[17];
        host_request(ED_PRESET, a, 2);
        peak_ms(5);
        t->p[P_DLY] = t->p[P_REV] = t->p[P_CHOR] = 0;   /* (dry: each pad's own sound) */
        t->p[P_LEVEL] = 100;
        for (pad = 0; pad < 16u; pad++) {
            int32_t pk;
            quiet(t);
            trk_note_on(t, 36u + pad, 100);
            pk = peak_ms(80);
            trk_note_off(t, 36u + pad);
            sounding += pk >= 300;
            low = pk < low ? pk : low;
        }
        quiet(t);
        rec_name(&kits[416u * GNC_LIST[GC_DRUMS].id[i]], nm);
        CHECK(sounding == 16u, "kit %u (\"%s\"): %u of its 16 pads sound on keys 36..51 (quietest peak %d)",
              GNC_LIST[GC_DRUMS].id[i], nm, sounding, low);
    }

    /* ---- Easy: its sounds on the Gnomad engines, by bank ID */
    {
        FILE *f = argc > 1 ? fopen(argv[1], "r") : 0;
        FILE *table = argc > 2 ? fopen(argv[2], "w") : 0;   /* round CR: Easy's whole table, generated, for the app */
        if (table)
            fprintf(table, "index\tpicture\tpicture_index\tslot\teasy_name\tkind\tbank_id\tbank_name\toctave\tengine\n");
        uint32_t gnomad = 0, mapped = 0, kept = 0, same = 0, lines = 0, past = 0;
        char line[128];
        for (i = 0; i < GN_NSOUNDS; i++) {
            const gn_sound_t *so = &GN_SOUNDS[i];
            int32_t ce = gnc_engine(so->engine);
            char off_name[64] = "";
            unsigned off_eng = 0, off_pre = 0;
            if (f && fgets(line, sizeof line, f) && sscanf(line, "%63[^\t]\t%u\t%u", off_name, &off_eng, &off_pre) == 3)
                lines++;
            if (ce < 0) {                               /* Felucca's kept engines: as without the swap */
                kept++;
                /* (round UX-R) never a silent engine: a slot gn_engines.h compiled out (gn_size_stub.h) */
                CHECK(ENGINES[so->engine]->note_on != gn_size_note_on, "Easy: %s is on %s, an engine this build "
                      "compiled out (silent)", so->name, ENGINES[so->engine]->name);
                same += !strcmp(off_name, so->name) && off_eng == so->engine && off_pre == so->preset;
                continue;
            }
            gnomad++;
            same += !strcmp(off_name, so->name);
            for (k = 0; k < NELEM(EASY) && strcmp(EASY[k].easy, so->name); k++)
                ;
            if (k == NELEM(EASY)) {
                CHECK(0, "Easy: %s plays a Gnomad engine but has no bank ID in the list", so->name);
                continue;
            }
            {
                int32_t bid = EASY[k].kit ? bank_id_of(kits, 32u, 416u, EASY[k].bank) : bank_id_of(pitched, 384u, 128u, EASY[k].bank);
                uint32_t id = bid < 0 ? 0u : (uint32_t)bid, p = GN_MELODY, before = bound_n[p];
                CHECK(bid >= 0, "Easy %s: the bank has one record named \"%s\" (id %d; -1 none, -2 several)", so->name,
                      EASY[k].bank, bid);
                if (table)                              /* round CR: the table the app reads (build/gn_easy_table.tsv) */
                    fprintf(table, "%u\t%s\t%u\t%u\t%s\t%s\t%u\t%s\t%d\t%s\n", i, GN_PIC_NAME[so->pic], so->pic,
                            i - GN_GROUP[so->pic], so->name, EASY[k].kit ? "k" : "p", id, EASY[k].bank, so->octave,
                            ENGINES[so->engine]->name);
                track_t *t = &trk[p];
                const uint8_t *rec = EASY[k].kit ? &kits[416u * id] : &pitched[128u * id];
                int bytes, kind = (ce == GC_DRUMS) == EASY[k].kit;
                int32_t pk;
                char nm[17];
                gn_sound_put(p, i);
                peak_ms(5);
                bytes = bound_n[p] != before && (EASY[k].kit ? !memcmp(bound_k[p], rec, 416) : !memcmp(bound_p[p], rec, 128));
                {
                    const uint8_t *src = EASY[k].kit ? GN_CORE_PATCH[GC_DRUMS] : rec;
                    int32_t want = (int32_t)rd16(src + 42), got = gnc_ctl(t, GNC_BRIGHT);
                    {                                   /* round MF: K1 the sound's own: exactly at its rest */
                        uint32_t store, rest;
                        uint64_t par;
                        if (!EASY[k].kit && gnc_slot(rec, ce, 0, &store, &par, &rest)) {
                            want = (int32_t)rest;
                            got = gnc_amount(t->p[store], rest, gnc_kr(rest));
                        }
                    }
                    trk_note_on(t, EASY[k].kit ? 36u : 60u, 100);
                    pk = peak_ms(150);
                    quiet(t);
                    rec_name(rec, nm);
                    int at = t->preset == so->preset && so->preset < GNC_LIST[ce].all && GNC_LIST[ce].id[so->preset] == id;
                    past += so->preset >= GNC_LIST[ce].n;
                    mapped += kind && bytes && at && pk >= 300 && got - want <= 261 && want - got <= 261;
                    CHECK(kind && bytes && at && pk >= 300 && got - want <= 261 && want - got <= 261,
                          "Easy %-13s -> %s %3u (\"%s\"): slot %u place %u%s, the record bound, Bright sent %d (stored "
                          "%d), peak %d", so->name, EASY[k].kit ? "kit " : "bank", id, nm, so->engine, so->preset,
                          so->preset >= GNC_LIST[ce].n ? " (past the offered list)" : "", got, want, pk);
                }
            }
        }
        if (f)
            fclose(f);
        if (table)
            fclose(table);
        CHECK(gnomad == NELEM(EASY) && mapped == gnomad, "Easy: %u sounds on the Gnomad engines, %u of them the bank ID "
              "FORK.md lists (%u listed), %u of them past their engine's offered list", gnomad, mapped,
              (unsigned)NELEM(EASY), past);
        CHECK(f && lines == GN_NSOUNDS && same == GN_NSOUNDS, "Easy: the %u sounds on Felucca's kept engines are the "
              "engines-off build's (engine, preset), every name in its place (%u of %u lines agree; dump %s)", kept, same,
              lines, argc > 1 ? argv[1] : "not given");
    }

    /* ---- round MF: each Easy sound's own four knobs, from its record's macro map, through Easy, the
     * shim and the core: the SOUND page names them (gc_macro_label; a kit and a sound without a map: TONE MOTION ECHO
     * SPACE), Easy loads each own knob at its rest place and the shim sends exactly its rest amount, the ends send 0
     * and 65535 (gc_macro_apply's writes), a kit's four are the controls, and every own knob moves the sound it plays
     * (a fresh note at each end of the knob, measured: level, brightness, length or the difference itself). Figures:
     * build/gn_macros.tsv; MF_WAVS=dir: each own knob swept 0 .. 16 and back over 6 s of notes, a WAV each */
    {
        static int16_t ra[FS * 2u], rb[FS * 2u], rc[FS * 2u];
        FILE *fig = fopen("build/gn_macros.tsv", "w");
        const char *wavs = getenv("MF_WAVS");
        uint32_t own_n = 0, moved = 0, lbl_ok = 0, rest_ok = 0, ends_ok = 0, kits_ok = 0, nk = 0, nsnd = 0;
        char first[160] = "", lfirst[96] = "";
        if (fig)
            fprintf(fig, "sound\tknob\tlabel\trest\tlevel_db\tbright_ratio\tlength_ratio\tband_db\tdiff_db\ttwo_takes_db\tmoves\n");
        for (i = 0; i < GN_NSOUNDS; i++) {
            const gn_sound_t *so = &GN_SOUNDS[i];
            int32_t ce = gnc_engine(so->engine);
            uint32_t p = GN_MELODY, kk, own;
            track_t *t = &trk[p];
            const uint8_t *kit, *rec;
            if (ce < 0)
                continue;
            gn_sound_put(p, i);
            peak_ms(20);                                /* (bound; one own knob's move a block) */
            rec = gnc_record(ce, t->preset, &kit);
            gs_snapshot(GV_PLAY);
            own = ce == GC_DRUMS ? 0u : gnh.own[p];
            nsnd++;
            for (kk = 0; kk < 4u; kk++) {
                static const char *const DEF[4] = {"TONE", "MOTION", "ECHO", "SPACE"};
                const char *want = ce == GC_DRUMS ? DEF[kk] : gc_macro_label((const gc_patch *)rec, kk);
                lbl_ok += !strcmp(gs.lbl[kk], want);
                if (strcmp(gs.lbl[kk], want) && !lfirst[0])
                    snprintf(lfirst, sizeof lfirst, "%s K%u shows %s, its record %s", so->name, kk + 1u, gs.lbl[kk], want);
            }
            if (ce == GC_DRUMS) {
                nk++;
                kits_ok += !gn_cm.own[p] && gn_map(p, 0) == &GN_CORE_KNOBS[0] && gn_map(p, 3) == &GN_CORE_KNOBS[3] &&
                           !gnh.own[p] && !gn_knob_label(p, 0);
                continue;
            }
            for (kk = 0; kk < 4u; kk++) {
                const gn_knob_t *m = gn_map(p, kk);
                uint32_t rest = gnh.rest[p][kk], j, a0, a1;
                double e0 = 0, e1 = 0, ed = 0, ec = 0, z0 = 0, z1 = 0, l0 = 0, l1 = 0, lv, br, ln, df, d0, bd;
                int mv;
                if (!((own >> kk) & 1u))
                    continue;
                own_n++;
                rest_ok += gn.amount[p][kk] == m->rest && gnh.hsent[p][kk] == (int32_t)rest &&
                           t->p[m->id] == gnh.kr[p][kk];
                /* each end: Easy's knob there, then a fresh note (next-note targets), 0.8 s held + 0.7 s; a second
                 * take at 0 (rc): what two takes of the same knob differ by (noise, free-running motion) */
                for (j = 0; j < 3u; j++) {
                    int16_t *r = j == 2u ? rc : j ? rb : ra;
                    uint32_t b, x, n = 0;
                    quiet(t);
                    gn.amount[p][kk] = (uint8_t)(j == 1u ? GN_STEPS : 0u);
                    gn_knob_write(p, kk);
                    peak_ms(10);
                    if (j < 2u)
                        (j ? &a1 : &a0)[0] = (uint32_t)gnh.hsent[p][kk];
                    trk_note_on(t, 60u, 100);
                    for (b = 0; b < FS * 3u / 2u / CTL; b++) {
                        int32_t out[2u * CTL];
                        if (b == FS * 8u / 10u / CTL)
                            trk_note_off(t, 60u);
                        gn_core_host();
                        mix_block(out, CTL);
                        for (x = 0; x < CTL; x++)
                            r[n++] = (int16_t)(out[2u * x] > 32767 ? 32767 : out[2u * x] < -32768 ? -32768 : out[2u * x]);
                    }
                }
                ends_ok += a0 == 0u && a1 == 65535u;
                for (j = 0; j < FS * 3u / 2u; j++) {
                    double d = (double)ra[j] - rb[j], dc = (double)ra[j] - rc[j];
                    ec += dc * dc;
                    e0 += (double)ra[j] * ra[j];
                    e1 += (double)rb[j] * rb[j];
                    ed += d * d;
                    if (j) {
                        z0 += (ra[j] >= 0) != (ra[j - 1] >= 0);
                        z1 += (rb[j] >= 0) != (rb[j - 1] >= 0);
                    }
                    if (ra[j] > 300 || ra[j] < -300)
                        l0 = j;
                    if (rb[j] > 300 || rb[j] < -300)
                        l1 = j;
                }
                lv = 10.0 * log10((e1 + 1.0) / (e0 + 1.0));
                br = (z1 + 1.0) / (z0 + 1.0);
                ln = (l1 + 1.0) / (l0 + 1.0);
                df = 10.0 * log10((ed + 1.0) / ((e0 > e1 ? e0 : e1) + 1.0));
                d0 = 10.0 * log10((ec + 1.0) / ((e0 > e1 ? e0 : e1) + 1.0));
                {
                    static double ba[24], bb[24], bc[24];
                    mf_bands(ra, FS * 3u / 2u, ba);
                    mf_bands(rb, FS * 3u / 2u, bb);
                    mf_bands(rc, FS * 3u / 2u, bc);
                    bd = mf_band_move(ba, bb, bc);
                }
                /* it moves: the level by 1 dB, the brightness (zero crossings) by 8 %, the length by 30 %, a band of the
                 * spectrum by 3 dB beyond two takes at 0 (G14's band measure), or the two ends differ by more than
                 * -20 dB of the louder and 3 dB beyond two takes at 0 */
                mv = fabs(lv) >= 1.0 || br >= 1.08 || br <= 1.0 / 1.08 || ln >= 1.3 || ln <= 1.0 / 1.3 || bd >= 3.0 ||
                     (df >= -20.0 && df >= d0 + 3.0);
                moved += mv;
                if (!mv && !first[0])
                    snprintf(first, sizeof first, "%s K%u %s: level %+.1f dB, brightness x%.2f, length x%.2f, "
                             "band %.1f dB, difference %.1f dB (two takes at 0: %.1f)", so->name, kk + 1u, gs.lbl[kk], lv, br, ln,
                             bd, df, d0);
                if (fig)
                    fprintf(fig, "%s\tK%u\t%s\t%u\t%+.2f\t%.3f\t%.3f\t%.1f\t%.1f\t%.1f\t%s\n", so->name, kk + 1u,
                            gs.lbl[kk], rest, lv, br, ln, bd, df, d0, mv ? "yes" : "NO");
                if (wavs) {                             /* the knob swept on the firmware's path: 0 -> 16 -> 0 */
                    char path[512], nm2[40];
                    FILE *w;
                    uint32_t b, x, step = 0, frames = FS * 6u, c;
                    static int16_t sw[FS * 6u];
                    quiet(t);
                    for (c = 0; so->name[c] && c < sizeof nm2 - 1u; c++)
                        nm2[c] = so->name[c] == ' ' ? '-' : (char)tolower((unsigned char)so->name[c]);
                    nm2[c] = 0;
                    snprintf(path, sizeof path, "%s/%02u-%s-k%u-%s.wav", wavs, i, nm2, kk + 1u, gs.lbl[kk]);
                    for (b = 0; b < frames / CTL; b++) {
                        int32_t out[2u * CTL];
                        uint32_t ms = b * CTL * 1000u / FS;
                        if (ms / 375u != step) {        /* a note every 375 ms (eighths at 80 BPM), 300 ms long */
                            step = ms / 375u;
                            trk_note_on(t, so->pic == GN_FUNNY_PIC ? 60u : 60u + (step % 4u == 3u ? 7u : step % 4u == 1u ? 4u : 0u), 100);
                        } else if (ms % 375u >= 300u && ms % 375u < 300u + CTL * 1000u / FS + 1u)
                            trk_all_off(t);
                        {                               /* the knob: 0 at 0 s, 16 at 3 s, 0 at 6 s */
                            uint32_t a = ms < 3000u ? ms * GN_STEPS / 3000u : (6000u - ms) * GN_STEPS / 3000u;
                            if (a != gn.amount[p][kk]) {
                                gn.amount[p][kk] = (uint8_t)a;
                                gn_knob_write(p, kk);
                            }
                        }
                        gn_core_host();
                        mix_block(out, CTL);
                        for (x = 0; x < CTL; x++)
                            sw[b * CTL + x] = (int16_t)(out[2u * x] > 32767 ? 32767 : out[2u * x] < -32768 ? -32768 :
                                                        out[2u * x]);
                    }
                    trk_all_off(t);
                    if ((w = fopen(path, "wb"))) {
                        uint32_t bytes = frames * 2u, v;
                        fwrite("RIFF", 1, 4, w);
                        v = 36u + bytes; fwrite(&v, 4, 1, w);
                        fwrite("WAVEfmt ", 1, 8, w);
                        v = 16; fwrite(&v, 4, 1, w);
                        {
                            uint16_t h[2] = {1, 1};
                            fwrite(h, 2, 2, w);
                        }
                        v = FS; fwrite(&v, 4, 1, w);
                        v = FS * 2u; fwrite(&v, 4, 1, w);
                        {
                            uint16_t h[2] = {2, 16};
                            fwrite(h, 2, 2, w);
                        }
                        fwrite("data", 1, 4, w);
                        fwrite(&bytes, 4, 1, w);
                        fwrite(sw, 2, frames, w);
                        fclose(w);
                    }
                }
                gn.amount[p][kk] = m->rest;             /* (back at rest) */
                gn_knob_write(p, kk);
                peak_ms(5);
            }
            quiet(t);
        }
        if (fig)
            fclose(fig);
        CHECK(lbl_ok == 4u * nsnd, "macros: the SOUND page names each of the %u Easy sounds' four knobs as its record "
              "does (%u of %u)%s%s", nsnd, lbl_ok, 4u * nsnd, lfirst[0] ? ": " : "", lfirst);
        CHECK(own_n && rest_ok == own_n && ends_ok == own_n, "macros: %u knobs of a sound's own: each loads at its "
              "rest place and sends exactly its rest amount (%u), its ends send 0 and 65535 (%u)", own_n, rest_ok,
              ends_ok);
        CHECK(moved == own_n, "macros: every knob of a sound's own moves the sound it plays (%u of %u; figures in "
              "build/gn_macros.tsv)%s%s", moved, own_n, first[0] ? ": " : "", first);
        CHECK(nk && kits_ok == nk, "macros: the kits keep their four (TONE MOTION ECHO SPACE, the controls): %u of %u",
              kits_ok, nk);
    }

    /* ---- power-on (round FW-CONTENT-8): the four tracks' sounds are chosen by bank ID */
    {
        static uint8_t nor_kept[sizeof host_nor];
        static project_store_t slot_kept[4];
        struct { uint8_t eng, preset; int16_t p[P_COUNT]; step_t step[NSTEP]; } mine[NTRK];
        uint32_t applied, same;
        char got[160];
        /* fresh: no settings record of ours (an erased flash: a new synth; Felucca 1.0.1's record is unmarked the
         * same way, gn_settings_migrate), starting in Easy as the synth does */
        host_start_easy = 1;
        host_power_on();
        CHECK(trk[1].eng_req == GN_ENGI_FM && trk[1].preset == 4u && trk[3].eng_req == GN_ENGI_DRUMS &&
              GNC_LIST[GC_DRUMS].id[trk[3].preset] == 0u, "power-on: main.c's felucca_init puts TRK_DEF's preset positions "
              "(track 2 FM preset %u = bank %u, track 4 kit %u) before the first main-loop frame", trk[1].preset,
              GNC_LIST[GC_FM].id[trk[1].preset], GNC_LIST[GC_DRUMS].id[trk[3].preset]);
        host_frame();
        host_wait_ms(200);
        applied = po_check(got, sizeof got);
        CHECK(applied == NTRK && gn.on && gn.entry_put && undo.trk == 0u, "power-on, fresh: %s; Easy on, the entry's "
              "Gnome not put until used, no undo copy", got);
        /* a Gnomad record left in Advanced (marked, mode 0): every power-on starts from the same sounds */
        host_power_on();
        favorites.factory[15][30] = 0xA0u;
        host_frame();
        host_wait_ms(200);
        applied = po_check(got, sizeof got);
        CHECK(applied == NTRK && !gn.on, "power-on, a Gnomad record left in Advanced: %s; Advanced", got);
        /* the build without the start in Easy (GN_START_EASY 0) */
        host_start_easy = 0;
        host_power_on();
        host_frame();
        host_wait_ms(200);
        applied = po_check(got, sizeof got);
        CHECK(applied == NTRK && !gn.on, "power-on, GN_START_EASY 0: %s", got);
        /* the app's command before the first frame: the power-on sounds are put first, its load after them */
        host_power_on();
        usb.config = 1;
        {
            uint8_t a[2] = {GN_ENGI_FM, 4};
            host_request(ED_PRESET, a, 2);
        }
        host_frame();
        host_wait_ms(200);
        applied = po_check(got, sizeof got);
        CHECK(applied == NTRK - 1u && trk[0].eng_req == GN_ENGI_FM && trk[0].preset == 4u, "power-on, PRESET from the "
              "app before the first frame: track 1 is the app's (FM preset %u, bank %u), the others the power-on's (%s)",
              trk[0].preset, GNC_LIST[GC_FM].id[trk[0].preset % GNC_LIST[GC_FM].all], got);
        /* a project: saved with other sounds (TRK_DEF's old FM place among them), the synth switched off and on with
         * it in flash, loaded by the app before the first frame and again after: its tracks exactly as saved */
        set_engine_of(&trk[0], 0);                      /* ANALOG ACID (Felucca's track 1) */
        apply_preset_to(&trk[0], 4);
        trk[0].p[P_LEVEL] = 77;
        set_engine_of(&trk[1], GN_ENGI_FM);             /* FM preset 4: what TRK_DEF found (bank 40 at 1.1.6) */
        apply_preset_to(&trk[1], 4);
        set_engine_of(&trk[2], 3);                      /* LOFI PULSE LD (Felucca's track 3) */
        apply_preset_to(&trk[2], 0);
        apply_preset_to(&trk[3], 2);                    /* DRUMS kit 2 */
        trk[3].step[0].time = ST_NOTE;
        trk[3].step[0].note[0] = 36;
        trk[3].step[0].n = 1;
        host_wait_ms(100);
        {
            uint8_t a[2] = {1, 0};
            host_request(ED_PROJECT, a, 2);
        }
        for (i = 0; i < NTRK; i++) {
            mine[i].eng = trk[i].eng_req;
            mine[i].preset = trk[i].preset;
            memcpy(mine[i].p, trk[i].p, sizeof mine[i].p);
            memcpy(mine[i].step, trk[i].step, sizeof mine[i].step);
        }
        memcpy(nor_kept, host_nor, sizeof nor_kept);
        memcpy(slot_kept, proj_slot, sizeof slot_kept);
        host_power_on();                                /* (the flash kept, its project slots as persist_boot reads them) */
        memcpy(host_nor, nor_kept, sizeof nor_kept);
        memcpy(proj_slot, slot_kept, sizeof slot_kept);
        usb.config = 1;
        {
            uint8_t a[2] = {0, 0};
            host_request(ED_PROJECT, a, 2);
        }
        host_frame();
        host_wait_ms(200);
        for (same = 0, i = 0; i < NTRK; i++)
            same += trk[i].eng_req == mine[i].eng && trk[i].engine == mine[i].eng && trk[i].preset == mine[i].preset &&
                    !memcmp(trk[i].p, mine[i].p, sizeof mine[i].p) && !memcmp(trk[i].step, mine[i].step, sizeof mine[i].step);
        CHECK(project_used(0) && same == NTRK && (gn.booted & GN_BOOT_SOUNDS), "power-on, a project loaded by the app "
              "before the first frame: its %u tracks as saved (%u of %u: engine, preset, every parameter, the steps; "
              "track 2 still FM preset %u, bank %u)", NTRK, same, NTRK, trk[1].preset, GNC_LIST[GC_FM].id[trk[1].preset]);
        host_power_on();
        memcpy(host_nor, nor_kept, sizeof nor_kept);
        memcpy(proj_slot, slot_kept, sizeof slot_kept);
        usb.config = 1;
        host_frame();
        host_wait_ms(200);
        applied = po_check(got, sizeof got);
        {
            uint8_t a[2] = {0, 0};
            host_request(ED_PROJECT, a, 2);
        }
        host_wait_ms(200);
        for (same = 0, i = 0; i < NTRK; i++)
            same += trk[i].eng_req == mine[i].eng && trk[i].engine == mine[i].eng && trk[i].preset == mine[i].preset &&
                    !memcmp(trk[i].p, mine[i].p, sizeof mine[i].p) && !memcmp(trk[i].step, mine[i].step, sizeof mine[i].step);
        CHECK(applied == NTRK && same == NTRK, "power-on with a project in flash: the power-on sounds (none loads at "
              "power-on); the project loaded after: its tracks as saved (%u of %u)", same, NTRK);

        /* round FW-CONTENT-9, "turn it off and on": a song made in Easy with the user's own sounds on the four tracks
         * (none of them the power-on's), notes on three tracks and a tempo, saved as Easy saves it (SAVE: the song into
         * its slot, the slot's sequence and fingerprint into the record's bytes 17..29, the mode in byte 30); the
         * synth switched off and on with the flash kept, the record imported as project.c persist_boot does; the
         * power-on sounds put at the first frames. Nothing loads a project at power-on (FW-CONTENT-8), so the
         * remembered song is the slot and the record: both byte for byte, Easy back on, the slot still Easy's; loaded,
         * its sounds, notes and tempo are the saved ones, and stay so through the next editor command. The mirror
         * (no record: exactly the power-on sounds) is "power-on, fresh" above */
        {
            static const char *const pick[NTRK] = {"SOFT FLUTE", "ORGAN", "SHIMMER", "DRUM MACHINE"};
            static project_store_t easy_slot;
            static persist_t rec_fl;
            uint8_t rec_ram[14], a[2];
            uint32_t slot, picked = 0, other = 0, mode_ok, bpm, id[NTRK];
            int n;
            host_start_easy = 1;
            host_power_on();
            usb.config = 1;
            host_frame();
            host_wait_ms(3100);                         /* (the start-up picture) */
            for (i = 0; i < NTRK; i++)
                for (k = 0; k < GN_NSOUNDS; k++)
                    if (!strcmp(GN_SOUNDS[k].name, pick[i])) {
                        gn_sound_put(i, k);
                        picked++;
                        break;
                    }
            peak_ms(5);
            for (i = 0; i < NTRK; i++) {
                int32_t ce = gnc_engine(trk[i].eng_req);
                id[i] = ce >= 0 && trk[i].preset < GNC_LIST[ce].all ? GNC_LIST[ce].id[trk[i].preset] : 0xFFFFu;
                other += ce >= 0 && (trk[i].eng_req != POWER_ON[i].slot || id[i] != POWER_ON[i].id);
            }
            for (k = 0; k < 4u; k++) {                  /* notes: track 1 a tune, track 2 two roots, track 4 kicks */
                trk[0].step[4u * k].time = ST_NOTE;
                trk[0].step[4u * k].note[0] = (uint8_t)(60u + 2u * k);
                trk[0].step[4u * k].n = 1;
                trk[3].step[4u * k].time = ST_NOTE;
                trk[3].step[4u * k].note[0] = 36;
                trk[3].step[4u * k].n = 1;
            }
            trk[1].step[0].time = trk[1].step[8].time = ST_NOTE;
            trk[1].step[0].note[0] = 48;
            trk[1].step[8].note[0] = 53;
            trk[1].step[0].n = trk[1].step[8].n = 1;
            song.g[G_BPM] = 97;
            host_wait_ms(100);
            host_tap(B_SAVE);
            host_wait_ms(200);
            slot = gn_song_owned();
            CHECK(gn.on && picked == NTRK && other == NTRK && gn.saved_mine == 1u && slot < 4u, "Easy, a song of the "
                  "user's own sounds saved: tracks on bank %u, %u, %u, kit %u (%u of %u not the power-on's), SAVED, "
                  "Easy's slot %u", id[0], id[1], id[2], id[3], other, NTRK, slot + 1u);
            for (i = 0; i < NTRK; i++) {
                mine[i].eng = trk[i].eng_req;
                mine[i].preset = trk[i].preset;
                memcpy(mine[i].p, trk[i].p, sizeof mine[i].p);
                memcpy(mine[i].step, trk[i].step, sizeof mine[i].step);
            }
            bpm = (uint32_t)song.g[G_BPM];
            memcpy(rec_ram, &favorites.factory[15][17], sizeof rec_ram);
            memcpy(&easy_slot, &proj_slot[slot % 4u], sizeof easy_slot);
            memcpy(nor_kept, host_nor, sizeof nor_kept);
            /* off and on: the RAM as at power-on, the flash kept; persist_boot's import of the record and the slots */
            host_power_on();
            memcpy(host_nor, nor_kept, sizeof nor_kept);
            memset(&persist_saved, 0, sizeof persist_saved);
            persist_pending = 0;
            n = st_load(OBJ_SETTINGS, &rec_fl, sizeof rec_fl);
            if (settings_import(&rec_fl, n))
                persist_saved = rec_fl;
            for (k = 0; k < 4u; k++)
                if (!proj_import(&proj_scratch, &proj_slot[k], sizeof(project_store_t)))
                    proj_fetch(k);
            settings_init();
            palette_set(settings.palette);
            usb.config = 1;
            host_frame();
            host_wait_ms(200);
            applied = po_check(got, sizeof got);
            mode_ok = (favorites.factory[15][30] & 0xF0u) == 0xA0u && (favorites.factory[15][30] & 1u);
            /* round 25 (merged into gn/ux by the FW-P0 catch-up): a power-on in Easy loads Easy's last saved
             * song at the first frame (gn_easy.c gn_song_boot), after the power-on sounds were put (round FW-CONTENT-8) */
            for (same = 0, other = 0, i = 0; i < NTRK; i++) {
                int32_t ce = gnc_engine(trk[i].eng_req);
                uint32_t now = ce >= 0 && trk[i].preset < GNC_LIST[ce].all ? GNC_LIST[ce].id[trk[i].preset] : 0xFFFFu;
                int bytes = ce == GC_DRUMS ? !memcmp(bound_k[i], &kits[416u * now], 416) :
                            ce >= 0 && !memcmp(bound_p[i], &pitched[128u * now], 128);
                same += trk[i].eng_req == mine[i].eng && trk[i].engine == mine[i].eng && trk[i].preset == mine[i].preset &&
                        now == id[i] && bytes && !memcmp(trk[i].step, mine[i].step, sizeof mine[i].step);
                other += trk[i].eng_req != POWER_ON[i].slot || now != POWER_ON[i].id;
            }
            CHECK((gn.booted & GN_BOOT_SOUNDS) && same == NTRK && other == NTRK && (uint32_t)song.g[G_BPM] == bpm &&
                  !song.playing, "off and on, Easy's song in flash: the power-on sounds put at the first frame, then "
                  "Easy's saved song loaded, stopped (round 25: %u of %u tracks as saved, %u of %u not the "
                  "power-on's, %d BPM, saved %u)", same, NTRK, other, NTRK, song.g[G_BPM], bpm);
            (void)applied;
            CHECK(!memcmp(rec_ram, &favorites.factory[15][17], sizeof rec_ram) && mode_ok &&
                  st_load(OBJ_SETTINGS, &rec_fl, sizeof rec_fl) == (int)sizeof rec_fl &&
                  !memcmp(rec_ram, &rec_fl.favorites.factory[15][17], sizeof rec_ram) && !memcmp(&proj_slot[slot % 4u],
                  &easy_slot, sizeof easy_slot) && st_load(OBJ_PROJECT0 + slot % 4u, &proj_scratch, sizeof easy_slot) ==
                  (int)sizeof easy_slot && !memcmp(&proj_scratch, &easy_slot, sizeof easy_slot),
                  "..the remembered song unchanged byte for byte: the slot (RAM and flash), the record's song bytes "
                  "17..29 and mode byte 30 (0x%02X, marked, Easy; RAM and flash)", favorites.factory[15][30]);
            CHECK(gn.on && gn_song_owned() == slot, "..Easy back on, as left, the slot still Easy's (%u)",
                  gn_song_owned() + 1u);
            a[0] = 0;                                   /* the remembered song loaded (by the app: Easy has no LOAD) */
            a[1] = (uint8_t)slot;
            host_request(ED_PROJECT, a, 2);
            host_wait_ms(200);
            host_request(ED_PING, a, 0);                /* (another command: the hook must not run again) */
            host_frame();
            host_wait_ms(200);
            for (same = 0, other = 0, i = 0; i < NTRK; i++) {
                int32_t ce = gnc_engine(trk[i].eng_req);
                uint32_t now = ce >= 0 && trk[i].preset < GNC_LIST[ce].all ? GNC_LIST[ce].id[trk[i].preset] : 0xFFFFu;
                int bytes = ce == GC_DRUMS ? !memcmp(bound_k[i], &kits[416u * now], 416) :
                            ce >= 0 && !memcmp(bound_p[i], &pitched[128u * now], 128);
                same += trk[i].eng_req == mine[i].eng && trk[i].engine == mine[i].eng && trk[i].preset == mine[i].preset &&
                        now == id[i] && bytes && !memcmp(trk[i].p, mine[i].p, sizeof mine[i].p) &&
                        !memcmp(trk[i].step, mine[i].step, sizeof mine[i].step);
                other += trk[i].eng_req != POWER_ON[i].slot || now != POWER_ON[i].id;
            }
            CHECK(same == NTRK && other == NTRK && (uint32_t)song.g[G_BPM] == bpm && gn.on && gn_song_owned() == slot,
                  "..loaded: its %u tracks as saved (%u of %u: engine, bank ID, the record bound, every parameter, the "
                  "notes), none on Acid Lead / Lamplight / Sing Ah / Big Step (%u of %u), %d BPM (saved %u), Easy on, "
                  "the slot still Easy's", NTRK, same, NTRK, other, NTRK, song.g[G_BPM], bpm);
        }
    }

    printf(fails ? "GNOMAD BANKS: %d FAILED\n" : "GNOMAD BANKS: ALL PASSED\n", fails);
    return fails ? 1 : 0;
}
#endif /* GN_BANK_SYNTH */
#endif
