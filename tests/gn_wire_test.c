/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kyle Palaniuk */
/* Gnomad (FORK.md, round EW) host test: the Advanced pages on the Gnomad engines, through the
 * firmware's own code (tests/gn_host.h with the swap on: Advanced's pages and knobs, the shim, the core):
 *   pages     on each engine every page's knob either reaches the core (it is offered and has a descriptor) or does
 *             nothing there and says why (params.c page_desc gives it none; the skin's words, gn_core_pages.c); SLICER
 *             and MOD are never offered on a Gnomad sound, ENV DEST and LFO DEST only where a knob of theirs plays
 *   moves     every knob that reaches the core changes a fresh note audibly at its two ends, on each engine (ENV,
 *             ENV DEST, LFO, LFO DEST, EDIT 2), and the song's DLY, REVERB and CHORUS pages change the effects (the
 *             MF measures: the level by 1 dB, the brightness by 8 %, the length by 30 %, a band by 3 dB beyond two
 *             takes, or the difference itself); figures in build/gn_wire.tsv
 *   rest      a sound loads with its absolute pages at its rest places, and there nothing is sent: the core plays the
 *             record as stored (the batch empty, the words the record's); EDIT 1 is the sound's four knobs by name
 *   kits      each of Easy's kits on the Gnomad DRUMS (KIT, DRUM MACHINE, HAND DRUM, TOMS): the white keys the
 *             sixteen pads in order, the black keys eleven more, none twice (16/16 and 11/11 distinct)
 *   gnome     Easy's Gnome loads with its syllables (TALK 12: the core's P20), each key its own syllable
 *   pan       no pan on the mono core: VOICE 2's and the MIXER's knob do nothing, Easy's SHIFT on the volumes neither
 *   GW_WAVS=dir  the sweeps for listening (ENV / LFO on three engines, DLY / REVERB / CHORUS, the Gnome's syllables)
 *   GW_PNGS=dir  the pages as the skin draws them (EDIT 1, EDIT 2, ENV, LFO, FX, DLY, REVERB, CHORUS, MIXER)
 *   tests/run_fork_tests.sh builds it: build/host-gn/gn_wire_test */
#define GN_ENGINES 1
#include "../firmware/src/gn_engines.h"
#include "gn_host.h"
#include "gn_png.h"
#include <math.h>

static int fails;
#define CHECK(c, ...) do { if (c) { printf("  ok    "); } else { printf("  FAIL  "); fails++; } \
                           printf(__VA_ARGS__); printf("\n"); } while (0)

static void blocks(uint32_t n, int16_t *cap)
{
    int32_t out[2u * CTL];
    uint32_t b, x;
    for (b = 0; b < n; b++) {
        gn_core_host();
        mix_block(out, CTL);
        for (x = 0; cap && x < CTL; x++)
            *cap++ = (int16_t)(out[2u * x] > 32767 ? 32767 : out[2u * x] < -32768 ? -32768 : out[2u * x]);
    }
}
static uint32_t ms(uint32_t m) { return (m * FS / 1000u + CTL - 1u) / CTL; }
static uint32_t core_voices(void)
{
    gc_report r;
    gn_core_report(&r);
    return r.voices_active;
}
static void quiet(track_t *t)
{
    uint32_t b;
    trk_all_off(t);
    for (b = 0; b < 6000u && core_voices(); b++)
        blocks(ms(1), 0);
    blocks(ms(30), 0);
}

#define TAKE (FS * 3u / 2u)
/* a fresh note (0.8 s held, then 0.7 s) into r, after the pages' batches went out */
static void take(track_t *t, uint32_t note, int16_t *r)
{
    quiet(t);
    blocks(ms(60), 0);
    trk_note_on(t, note, 100);
    blocks(ms(800), r);
    trk_note_off(t, note);
    blocks(TAKE / CTL - ms(800), r + ms(800) * CTL);
}

/* the MF measures (tests/gn_bank_test.c): 24 log bands of a take, and the most a band moves beyond two takes */
#define MF_N 65536u
static void bands(const int16_t *x, uint32_t n, double out[24])
{
    static double re[MF_N], im[MF_N];
    uint32_t i, j, k, len;
    for (i = 0; i < MF_N; i++) {
        re[i] = i < n ? x[i] * (0.5 - 0.5 * cos(2.0 * M_PI * i / (MF_N - 1u))) : 0.0;
        im[i] = 0.0;
    }
    for (i = 1, j = 0; i < MF_N; i++) {
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
static double band_move(const double a[24], const double b[24], const double c[24])
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
/* takes a (one end) and b (the other) differ audibly, c a second take of a; why: the figures */
static int moved(const int16_t *a, const int16_t *b, const int16_t *c, char *why, size_t n)
{
    static double ba[24], bb[24], bc[24];
    double e0 = 0, e1 = 0, ed = 0, ec = 0, z0 = 0, z1 = 0, l0 = 0, l1 = 0, lv, br, ln, df, d0, bd;
    uint32_t j;
    for (j = 0; j < TAKE; j++) {
        double d = (double)a[j] - b[j], dc = (double)a[j] - c[j];
        ec += dc * dc;
        e0 += (double)a[j] * a[j];
        e1 += (double)b[j] * b[j];
        ed += d * d;
        if (j) {
            z0 += (a[j] >= 0) != (a[j - 1] >= 0);
            z1 += (b[j] >= 0) != (b[j - 1] >= 0);
        }
        if (a[j] > 300 || a[j] < -300)
            l0 = j;
        if (b[j] > 300 || b[j] < -300)
            l1 = j;
    }
    lv = 10.0 * log10((e1 + 1.0) / (e0 + 1.0));
    br = (z1 + 1.0) / (z0 + 1.0);
    ln = (l1 + 1.0) / (l0 + 1.0);
    df = 10.0 * log10((ed + 1.0) / ((e0 > e1 ? e0 : e1) + 1.0));
    d0 = 10.0 * log10((ec + 1.0) / ((e0 > e1 ? e0 : e1) + 1.0));
    bands(a, TAKE, ba);
    bands(b, TAKE, bb);
    bands(c, TAKE, bc);
    bd = band_move(ba, bb, bc);
    snprintf(why, n, "level %+.1f dB, brightness x%.2f, length x%.2f, band %.1f dB, difference %.1f dB (two takes %.1f)",
             lv, br, ln, bd, df, d0);
    return fabs(lv) >= 1.0 || br >= 1.08 || br <= 1.0 / 1.08 || ln >= 1.3 || ln <= 1.0 / 1.3 || bd >= 3.0 ||
           (df >= -20.0 && df >= d0 + 3.0);
}

static void wav(const char *dir, const char *name, const int16_t *x, uint32_t frames)
{
    char path[512];
    FILE *w;
    uint32_t bytes = frames * 2u, v;
    uint16_t h[2];
    if (!dir)
        return;
    snprintf(path, sizeof path, "%s/%s.wav", dir, name);
    if (!(w = fopen(path, "wb")))
        return;
    fwrite("RIFF", 1, 4, w);
    v = 36u + bytes; fwrite(&v, 4, 1, w);
    fwrite("WAVEfmt ", 1, 8, w);
    v = 16; fwrite(&v, 4, 1, w);
    h[0] = 1; h[1] = 1; fwrite(h, 2, 2, w);
    v = FS; fwrite(&v, 4, 1, w);
    v = FS * 2u; fwrite(&v, 4, 1, w);
    h[0] = 2; h[1] = 16; fwrite(h, 2, 2, w);
    fwrite("data", 1, 4, w);
    fwrite(&bytes, 4, 1, w);
    fwrite(x, 2, frames, w);
    fclose(w);
}

static void shot(const char *dir, const char *name)
{
    static uint8_t rgb[240 * 240 * 3], big[480 * 480 * 3];
    char path[512];
    uint32_t x, y;
    size_t n;
    uint8_t *png;
    FILE *f;
    if (!dir)
        return;
    ui.force = 1;
    ui_draw();
    ui_draw();
    for (y = 0; y < 240u; y++)
        for (x = 0; x < 240u; x++) {
            uint16_t c = host_screen[y * 240u + x];
            c = (uint16_t)((c >> 8) | (c << 8));
            rgb[(y * 240u + x) * 3u] = (uint8_t)(((c >> 11) & 31u) * 255u / 31u);
            rgb[(y * 240u + x) * 3u + 1u] = (uint8_t)(((c >> 5) & 63u) * 255u / 63u);
            rgb[(y * 240u + x) * 3u + 2u] = (uint8_t)((c & 31u) * 255u / 31u);
        }
    for (y = 0; y < 480u; y++)
        for (x = 0; x < 480u; x++)
            memcpy(&big[(y * 480u + x) * 3u], &rgb[((y / 2u) * 240u + x / 2u) * 3u], 3);
    png = gn_png(big, 480, 480, &n);
    snprintf(path, sizeof path, "%s/%s.png", dir, name);
    if ((f = fopen(path, "wb"))) {
        fwrite(png, 1, n, f);
        fclose(f);
    }
    free(png);
}

static const struct { uint32_t slot; int32_t ce; const char *name; uint32_t note; } ENG[5] = {
    {GN_ENGI_VA, GC_VA, "va", 57}, {GN_ENGI_FM, GC_FM, "fm", 60}, {GN_ENGI_GNOME, GC_GNOME, "gnome", 60},
    {GN_ENGI_SAMPLE, GC_SAMPLE, "sampler", 60}, {GN_ENGI_DRUMS, GC_DRUMS, "drums", 36}};

/* engine ce's sound for the measures: the first in bank order whose second knob is the Wobble control (so the LFO
 * pages have a depth to work with) and with the fewest knobs of its own (pages they own say so instead) */
static uint32_t pick(int32_t ce)
{
    uint32_t pi, best = 0, bown = 9, s, store, rest;
    uint64_t par;
    const uint8_t *kit;
    for (pi = 0; pi < GNC_LIST[ce].n; pi++) {
        const uint8_t *rec = gnc_record(ce, pi, &kit);
        uint32_t own = 0;
        if (ce == GC_DRUMS)
            return 0;
        if (ce == GC_SAMPLE && gna_word(rec, 13u) >= 2u)
            continue;                                   /* (a one-shot or a loop: START, END and LOOP play) */
        if (gnc_slot(rec, ce, 1, &store, &par, &rest))
            continue;
        for (s = 0; s < 4u; s++)
            own += (uint32_t)gnc_slot(rec, ce, s, &store, &par, &rest);
        if (own < bown) {
            bown = own;
            best = pi;
        }
    }
    return best;
}
static void load(track_t *t, uint32_t slot, uint32_t pi)
{
    set_engine_of(t, slot);
    apply_preset_to(t, pi);
    t->p[P_LEVEL] = 100;
    t->p[P_DLY] = t->p[P_REV] = t->p[P_CHOR] = 0;
    blocks(ms(80), 0);
    t->p[P_E1] = 110;                                   /* (Wobble up: the LFO pages' depth) */
    blocks(ms(40), 0);
}
static double rms_db(const int16_t *x, uint32_t n)     /* a take's level, dB re full scale */
{
    double a = 1e-9;
    uint32_t i;
    for (i = 0; i < n; i++)
        a += (double)x[i] * x[i];
    return 10.0 * log10(a / n / (32768.0 * 32768.0));
}
static uint32_t page_of(uint32_t fam, uint32_t id0)
{
    uint32_t i;
    for (i = 0; i < NPAGES && !(PAGES[i].fam == fam && PAGES[i].id[0] == id0); i++)
        ;
    return i;
}

int main(void)
{
    const char *wavs = getenv("GW_WAVS"), *pngs = getenv("GW_PNGS");
    static int16_t ra[TAKE], rb[TAKE], rc[TAKE];
    static const uint8_t TRACK_PAGES[][2] = {{FAM_ENV, P_ATK}, {FAM_ENV, P_ED_FLT}, {FAM_LFO, P_LRATE}, {FAM_LFO, P_LD_PIT},
                                             {FAM_FX, P_DIST}, {FAM_EDIT, P_VOICE}, {FAM_EDIT, P_ALLOC}};
    FILE *fig = fopen("build/gn_wire.tsv", "w");
    uint32_t e, i, k, n_live = 0, n_moved = 0, n_dead = 0, dead_ok = 0, n_rest = 0, rest_ok = 0;
    char first[256] = "", dfirst[160] = "";
    printf("Gnomad round EW: the Advanced pages on the Gnomad engines\n");
    host_power_on();
    settings.palette = GSK_PAL;
    palette_set(GSK_PAL);
    if (fig)
        fprintf(fig, "engine\tsound\tpage\tknob\tlo\thi\tmoves\tfigures\n");

    /* ---- pages and moves, engine by engine */
    for (e = 0; e < 5u; e++) {
        track_t *t = &trk[0];
        uint32_t pi = pick(ENG[e].ce), pg, sl;
        const uint8_t *kit, *rec = gnc_record(ENG[e].ce, pi, &kit);
        char nm[17];
        song.sel = 0;
        load(t, ENG[e].slot, pi);
        memcpy(nm, (kit ? kit : rec) + 16, 16);
        nm[16] = 0;
        printf("  -- %s: %s (preset %u)\n", ENG[e].name, nm, pi);
        /* rest: every absolute setting at its place, and nothing of the pages sent: the core has the record's words */
        {
            uint32_t c, ok = 1, p;
            for (c = 0; c < GNA_N; c++)
                if (GNA_P[ENG[e].ce][c])
                    ok &= t->p[GNA_ID[c]] == gna.rest[0][c] && gna.rest[0][c] == gna_rest(ENG[e].ce, c, rec);
            for (p = 1; p < 48u; p++)
                ok &= p >= 5u && p <= 9u ? 1 : gna.w[0][p] == gna_word(rec, p) || ((gnh.reach[0] >> p) & 1u);
            n_rest++;
            rest_ok += ok && t->p[P_ED_FX] == GNA_MARK_OF(pi) && !gna.pend[0];
            CHECK(ok && t->p[P_ED_FX] == GNA_MARK_OF(pi), "rest: %s %s loads with its pages at its own (nothing sent)",
                  ENG[e].name, nm);
        }
        /* EDIT 1: the sound's four, as Home and Easy have them */
        {
            uint32_t ok = 1, s, store, rest;
            uint64_t par;
            ui.home = 0;
            ui.page = (uint8_t)page_of(FAM_EDIT, P_E0);
            for (s = 0; s < 4u; s++) {
                int16_t *vp;
                const param_desc_t *d = page_desc(&PAGES[ui.page], s, &vp);
                const char *want = ENG[e].ce == GC_DRUMS ? (s == 0 ? "TONE" : s == 1 ? "MOTION" : s == 2 ? "ECHO" : "SPACE")
                                                         : gc_macro_label((const gc_patch *)rec, s);
                gnc_slot(rec, ENG[e].ce, s, &store, &par, &rest);
                ok &= d && vp == &t->p[store] && (!want[0] || !strcasecmp(d->label, want));
            }
            CHECK(ok, "EDIT 1: %s's four knobs, each its record's name and where its amount is kept", nm);
        }
        if (pngs) {
            static const struct { uint8_t fam, id0; const char *tag; } SH[] = {
                {FAM_EDIT, P_E0, "1-edit1"}, {FAM_EDIT, P_E4, "2-edit2"}, {FAM_ENV, P_ATK, "3-env"},
                {FAM_ENV, P_ED_FLT, "4-envdest"}, {FAM_LFO, P_LRATE, "5-lfo"}, {FAM_LFO, P_LD_PIT, "6-lfodest"},
                {FAM_FX, P_DIST, "7-fx"}};
            for (k = 0; k < NELEM(SH); k++) {
                char b[64];
                ui.home = 0;
                ui.page = (uint8_t)page_of(SH[k].fam, SH[k].id0);
                if (!page_visible(ui.page))
                    continue;
                snprintf(b, sizeof b, "%s-%s", ENG[e].name, SH[k].tag);
                shot(pngs, b);
            }
        }
        if (pngs && ENG[e].ce != GC_DRUMS) {            /* .. and EDIT 1, EDIT 2 of the engine's sound with most knobs */
            uint32_t q, best = 0, bown = 0, s2, st2, rs2;   /* of its own (their names; what they own, said) */
            uint64_t pr2;
            const uint8_t *k2;
            char b[64];
            for (q = 0; q < GNC_LIST[ENG[e].ce].n; q++) {
                uint32_t own = 0;
                for (s2 = 0; s2 < 4u; s2++)
                    own += (uint32_t)gnc_slot(gnc_record(ENG[e].ce, q, &k2), ENG[e].ce, s2, &st2, &pr2, &rs2);
                if (own > bown) {
                    bown = own;
                    best = q;
                }
            }
            load(t, ENG[e].slot, best);
            ui.home = 0;
            ui.page = (uint8_t)page_of(FAM_EDIT, P_E0);
            snprintf(b, sizeof b, "%s-8-edit1-own", ENG[e].name);
            shot(pngs, b);
            ui.page = (uint8_t)page_of(FAM_EDIT, P_E4);
            snprintf(b, sizeof b, "%s-9-edit2-own", ENG[e].name);
            shot(pngs, b);
            load(t, ENG[e].slot, pi);
        }
        /* each page's knob: dead (no descriptor, a reason) or live (it moves the sound) */
        for (pg = 0; pg < NELEM(TRACK_PAGES) + 1u; pg++) {
            uint32_t pidx = pg < NELEM(TRACK_PAGES) ? page_of(TRACK_PAGES[pg][0], TRACK_PAGES[pg][1])
                                                     : page_of(FAM_EDIT, P_E4);
            const page_t *P = &PAGES[pidx];
            ui.home = 0;
            ui.page = (uint8_t)pidx;
            for (sl = 0; sl < 4u; sl++) {
                int16_t *vp;
                const param_desc_t *d;
                uint32_t why, id = P->id[sl], kind = P->scope == SC_ENGINE ? GNK_EDIT2 : GNK_TRACK;
                int32_t lo, hi, keep;
                char fg[200], un[17];
                int mv;
                if (id == 0xFFu)
                    continue;
                d = page_desc(P, sl, &vp);
                why = gn_page_why(kind, kind == GNK_EDIT2 ? sl : id);
                if (!d || !vp) {                        /* dead: said, and the page is offered only if one plays */
                    n_dead++;
                    dead_ok += why != GNW_LIVE && gsk_gn_why(sl) != 0;
                    if (!(why != GNW_LIVE && gsk_gn_why(sl)) && !dfirst[0])
                        snprintf(dfirst, sizeof dfirst, "%s %s K%u inert but says nothing", ENG[e].name, P->title, sl + 1u);
                    continue;
                }
                if (id == P_VOICE || id == P_GLIDE || id == P_GLMODE || id == P_PRIO || id == P_ALLOC || id == P_DETUNE ||
                    id == P_MUTE || id == P_CHOR || id == P_DLY || id == P_REV)
                    continue;                           /* (Felucca's voice settings and the sends: not this round's) */
                if (why != GNW_LIVE)
                    continue;
                lo = d->min;
                hi = d->max;
                if (d->fmt == F_ENUM && d->names == GNC_N_LOOP)
                    lo = 1;                             /* (LOOP: ONCE against LOOP) */
                {   /* the knob's two ends, each a fresh note: on the measures' sound, else (a setting this sound
                     * hardly uses, FM's feedback on a sound whose feedback operator is quiet) on the next few of the
                     * engine's sounds where the knob plays: the table names the sound */
                    uint32_t tries, alt = pi, note = ENG[e].note;
                    int16_t sus = t->p[P_SUS];
                    if (ENG[e].ce == GC_DRUMS && sl == 2u)
                        note = 37;                      /* (SNAP: the snare, a pad it shapes) */
                    if (ENG[e].ce == GC_GNOME && sl == 3u && kind == GNK_EDIT2)
                        note = 62;                      /* (TALK: D's syllable, not C's, which is the sound's own vowel) */
                    for (tries = 0, mv = 0; tries < 6u && !mv; tries++) {
                        if (tries) {
                            const uint8_t *k2, *r2;
                            do
                                alt = (alt + 1u) % GNC_LIST[ENG[e].ce].n;
                            while ((r2 = gnc_record(ENG[e].ce, alt, &k2)), ENG[e].ce == GC_SAMPLE && gna_word(r2, 13u) >= 2u);
                            load(t, ENG[e].slot, alt);
                            sus = t->p[P_SUS];
                            if (page_desc(P, sl, &vp) == 0 || gn_page_why(kind, kind == GNK_EDIT2 ? sl : id) != GNW_LIVE)
                                continue;
                            memcpy(nm, (k2 ? k2 : r2) + 16, 16);
                        }
                        if (d->names == GNC_N_LOOP)
                            t->p[P_SUS] = 127;          /* (LOOP: a held note sustains past the sample's end) */
                        keep = *vp;
                        *vp = (int16_t)lo;
                        take(t, note, ra);
                        take(t, note, rc);
                        *vp = (int16_t)hi;
                        take(t, note, rb);
                        *vp = (int16_t)keep;
                        t->p[P_SUS] = sus;
                        mv = moved(ra, rb, rc, fg, sizeof fg);
                        memcpy(un, nm, sizeof un);
                    }
                    if (alt != pi) {                    /* (the measures' sound again for the next knob) */
                        load(t, ENG[e].slot, pi);
                        memcpy(nm, (kit ? kit : rec) + 16, 16);
                    }
                }
                n_live++;
                n_moved += mv;
                if (!mv && !first[0])
                    snprintf(first, sizeof first, "%s %s %s: %s", ENG[e].name, P->title, d->label, fg);
                if (fig)
                    fprintf(fig, "%s\t%s\t%s\t%s\t%d\t%d\t%s\t%s\n", ENG[e].name, un, P->title, d->label, lo, hi,
                            mv ? "yes" : "NO", fg);
            }
        }
        /* pages offered: never SLICER or MOD; ENV DEST, LFO DEST only with a knob that plays */
        {
            uint32_t ok = 1, j;
            for (j = 0; j < NPAGES; j++) {
                const page_t *P = &PAGES[j];
                if (P->graph == GR_SLCR || P->graph == GR_MOD)
                    ok &= !page_visible(j);
                if (P->id[0] == P_ED_FLT && P->fam == FAM_ENV)
                    ok &= page_visible(j) == !(gna_none(gn_page_why(GNK_TRACK, P_ED_FLT)) &&
                                               gna_none(gn_page_why(GNK_TRACK, P_ED_PIT)));
            }
            CHECK(ok, "pages: %s offers neither SLICER nor MOD; ENV DEST %s", ENG[e].name,
                  page_visible(page_of(FAM_ENV, P_ED_FLT)) ? "offered" : "not offered (nothing of it plays here)");
        }
        quiet(t);
    }
    CHECK(n_live && n_moved == n_live, "moves: every page knob that reaches the core moves a fresh note at its two ends: "
          "%u of %u (build/gn_wire.tsv)%s%s", n_moved, n_live, first[0] ? ": " : "", first);
    CHECK(n_dead && dead_ok == n_dead, "inert: every knob that does nothing on its engine says why (%u of %u)%s%s", dead_ok,
          n_dead, dfirst[0] ? ": " : "", dfirst);
    CHECK(rest_ok == n_rest, "rest: %u of %u sounds load with nothing of the pages sent", rest_ok, n_rest);

    /* ---- the song's effects: DLY, REVERB, CHORUS (a VA sound with every send up) */
    {
        static const struct { uint8_t id, lo, hi; const char *what; } FXK[] = {
            {G_DTIME, 2, 6, "DLY Time"}, {G_DFDBK, 0, 120, "DLY Repeat"}, {G_DMIX, 0, 127, "DLY Mix"},
            {G_RSIZE, 0, 127, "REVERB Size"}, {G_CRATE, 0, 127, "CHORUS Speed"}, {G_CDEPTH, 0, 127, "CHORUS Depth"}};
        track_t *t = &trk[0];
        uint32_t ok = 0;
        char fl[200] = "";
        load(t, GN_ENGI_VA, pick(GC_VA));
        t->p[P_E1] = 0;
        for (k = 0; k < NELEM(FXK); k++) {
            int16_t keep = song.g[FXK[k].id];
            char fg[200];
            int mv;
            t->p[P_DLY] = FXK[k].id <= G_DMIX ? 100 : 0;
            t->p[P_REV] = FXK[k].id == G_RSIZE ? 100 : 0;
            t->p[P_CHOR] = FXK[k].id >= G_CRATE ? 127 : 0;
            song.g[FXK[k].id] = FXK[k].lo;
            take(t, 57, ra);
            take(t, 57, rc);
            song.g[FXK[k].id] = FXK[k].hi;
            take(t, 57, rb);
            song.g[FXK[k].id] = keep;
            mv = moved(ra, rb, rc, fg, sizeof fg);
            ok += mv;
            if (!mv && !fl[0])
                snprintf(fl, sizeof fl, "%s: %s", FXK[k].what, fg);
            if (fig)
                fprintf(fig, "song\t-\t%s\t-\t%d\t%d\t%s\t%s\n", FXK[k].what, FXK[k].lo, FXK[k].hi, mv ? "yes" : "NO", fg);
        }
        t->p[P_DLY] = t->p[P_REV] = t->p[P_CHOR] = 0;
        CHECK(ok == NELEM(FXK), "effects: DLY Time, Repeat, Mix, REVERB Size, CHORUS Speed and Depth each change the "
              "sound (%u of %u)%s%s", ok, (uint32_t)NELEM(FXK), fl[0] ? ": " : "", fl);
        if (pngs) {
            static const uint8_t G[3] = {G_DTIME, G_RTYPE, G_CRATE};
            static const char *const NM[3] = {"song-dly", "song-reverb", "song-chorus"};
            for (k = 0; k < 3u; k++) {
                ui.home = 0;
                ui.page = (uint8_t)page_of(FAM_FX, G[k]);
                shot(pngs, NM[k]);
            }
            for (k = 0; k < NPAGES && PAGES[k].scope != SC_TRK; k++)
                ;
            ui.page = (uint8_t)k;
            shot(pngs, "song-mixer");
        }
    }

    /* ---- pan: none on the mono core */
    {
        track_t *t = &trk[0];
        int16_t pan = t->p[P_PAN];
        int16_t *vp;
        uint32_t j;
        for (j = 0; j < NPAGES && PAGES[j].scope != SC_TRK; j++)
            ;
        ui.home = 0;
        ui.page = (uint8_t)j;
        tracks_edit(1, 5);
        CHECK(t->p[P_PAN] == pan && !page_desc(&PAGES[page_of(FAM_EDIT, P_ALLOC)], 2, &vp) &&
              gn_page_why(GNK_TRACK, P_PAN) == GNW_PAN, "pan: the MIXER's and VOICE 2's PAN do nothing (the core is mono) "
              "and say so");
    }

    /* ---- kits: Easy's four on the Gnomad DRUMS, every key a pad, no pad twice on the white keys nor on the black */
    {
        static const char *const KITS[4] = {"KIT", "DRUM MACHINE", "HAND DRUM", "TOMS"};
        uint32_t good = 0;
        char kf[120] = "";
        host_power_on();
        host_easy();
        for (k = 0; k < 4u; k++) {
            uint32_t s, key, white = 0, black = 0, nw = 0, nb = 0;
            for (s = 0; s < GN_NSOUNDS && strcmp(GN_SOUNDS[s].name, KITS[k]); s++)
                ;
            gn_sound_put(song.sel, s);
            host_wait_ms(100);
            for (key = 0; key < 27u; key++) {
                uint32_t note = gn_key(TSEL, key, 60), pad = (note + 128u - 36u) % 16u;
                if (GN_WHITE[key] != 0xFFu) {
                    nw++;
                    white |= (pad == GN_WHITE[key]) << pad;
                } else {
                    nb++;
                    black |= 1u << pad;
                }
            }
            if ((GN_SOUNDS[s].flags & GNF_PADS) && gnc_engine(TSEL->eng_req) == GC_DRUMS && white == 0xFFFFu && nw == 16u &&
                __builtin_popcount(black) == 11 && nb == 11u)
                good++;
            else if (!kf[0])
                snprintf(kf, sizeof kf, "%s: white %04x of %u, black %u distinct of %u", KITS[k], white, nw,
                         __builtin_popcount(black), nb);
        }
        CHECK(good == 4u, "kits: KIT, DRUM MACHINE, HAND DRUM and TOMS: the 16 white keys the 16 pads in order, the 11 "
              "black keys 11 distinct pads (%u of 4)%s%s", good, kf[0] ? ": " : "", kf);
    }

    /* ---- round EW-FIXES: a user preset of a Gnomad sound reloads sounding as it was saved: the record it was made
     * from (not preset 0's) under the pages' settings authored for it; one saved before (mark 0: before round EW;
     * mark 1: round EW's) loads as it did; a song's track settled by round EW keeps its pages */
    {
        static int16_t a0[TAKE], a1[TAKE];
        static uint16_t w0[48];
        uint32_t good = 0, legacy = 0, p, d, dc;
        char uf[160] = "";
        for (e = 0; e < 5u; e++) {
            track_t *t = &trk[0];
            const uint32_t pi = 5u;
            int16_t rel, e4;
            int rc;
            host_power_on();
            song.sel = 0;
            load(t, ENG[e].slot, pi);
            t->p[P_REL] = (int16_t)(t->p[P_REL] > 64 ? t->p[P_REL] - 40 : t->p[P_REL] + 40);   /* authored: ENV */
            t->p[P_E4] = 24;                                                                  /* .. and EDIT 2 */
            blocks(ms(120), 0);
            rel = t->p[P_REL];
            e4 = t->p[P_E4];
            take(t, ENG[e].note, a1);
            take(t, ENG[e].note, a0);                           /* (two takes: what a take varies by itself) */
            for (p = dc = 0; p < TAKE; p++)
                dc = abs(a0[p] - a1[p]) > (int)dc ? (uint32_t)abs(a0[p] - a1[p]) : dc;
            memcpy(w0, gna.w[0], sizeof w0);
            rc = up_store(3, "EW FIX");
            load(t, ENG[(e + 1u) % 5u].slot, 0);                /* another sound between */
            up_load(3);
            blocks(ms(120), 0);
            take(t, ENG[e].note, a1);
            for (p = d = 0; p < TAKE; p++)
                d = abs(a0[p] - a1[p]) > (int)d ? (uint32_t)abs(a0[p] - a1[p]) : d;
            if ((rc == 0 || rc == 3) && t->preset == pi && gnc.preset[0] == pi && t->p[P_REL] == rel &&
                t->p[P_E4] == e4 && !memcmp(w0, gna.w[0], sizeof w0) && d <= dc + 2u)
                good++;
            printf("        %s: reloaded take differs by %u, a second take of the original by %u\n", ENG[e].name, d, dc);
            if (good != e + 1u && !uf[0])
                snprintf(uf, sizeof uf, "%s: preset %u (bound %u) of %u, words %s, takes differ by %u", ENG[e].name,
                         t->preset, gnc.preset[0], pi, memcmp(w0, gna.w[0], sizeof w0) ? "differ" : "same", d);
            /* the same record as saved before round EW (mark 0) and by it (mark 1): as they loaded */
            up_set_value(up_rec(3), P_ED_FX, 0);
            up_load(3);
            blocks(ms(120), 0);
            legacy += t->preset == 0u && t->p[P_ED_FX] == GNA_MARK && t->p[P_E4] == e4 &&
                      (!GNA_P[ENG[e].ce][GNA_REL] || t->p[P_REL] == gna.rest[0][GNA_REL]);
            up_set_value(up_rec(3), P_ED_FX, GNA_MARK);
            up_load(3);
            blocks(ms(120), 0);
            legacy += t->preset == 0u && t->p[P_ED_FX] == GNA_MARK && t->p[P_REL] == rel && t->p[P_E4] == e4;
            /* a song's track settled by round EW (mark 1) on another preset: its pages stay as the song has them */
            load(t, ENG[e].slot, pi);
            t->p[P_REL] = rel;
            t->p[P_ED_FX] = GNA_MARK;
            blocks(ms(120), 0);
            legacy += t->preset == pi && t->p[P_ED_FX] == GNA_MARK && t->p[P_REL] == rel;
        }
        CHECK(good == 5u, "user presets: VA FM GNOME SAMPLER DRUMS saved from preset 5 with ENV and EDIT 2 moved reload "
              "on preset 5, the same words to the core, the same take (within what two takes differ by) (%u of 5)%s%s", good,
              uf[0] ? ": " : "", uf);
        CHECK(legacy == 15u, "user presets saved before (round EW's mark 1, older mark 0) load as they did, a song's "
              "settled track keeps its pages (%u of 15)", legacy);
    }

    /* ---- round EW-FIXES: VOICE's Glide and Mode, VOICE 2's Detune: the core gets the key alone; they say so */
    {
        uint32_t good = 0;
        int16_t *vp;
        for (e = 0; e < 5u; e++) {
            host_power_on();
            song.sel = 0;
            load(&trk[0], ENG[e].slot, 0);
            good += !page_desc(&PAGES[page_of(FAM_EDIT, P_VOICE)], 1, &vp) && gn_why[1] == GNW_FIXED &&
                    !page_desc(&PAGES[page_of(FAM_EDIT, P_VOICE)], 2, &vp) && gn_why[2] == GNW_FIXED &&
                    page_desc(&PAGES[page_of(FAM_EDIT, P_VOICE)], 0, &vp) != 0 &&
                    !page_desc(&PAGES[page_of(FAM_EDIT, P_ALLOC)], 1, &vp) && gn_why[1] == GNW_FIXED;
        }
        CHECK(good == 5u, "VOICE's Glide and Mode and VOICE 2's Detune do nothing on the core and say so (\"Not on this "
              "sound\"); Voices stays (%u of 5 engines)", good);
    }

    /* ---- round EW-FIXES: on a Gnomad kit no black key plays what a white key plays: each is a soft hit (a ghost) of a
     * drum beside it, its own note (the ghost block), audibly softer and still heard; Advanced's 27 keys are 27 notes */
    {
        static const char *const KITS[4] = {"KIT", "DRUM MACHINE", "HAND DRUM", "TOMS"};
        static int16_t gw[TAKE], gb[TAKE];
        uint32_t good = 0, nb = 0, soft = 0, adv = 0, s, key, j;
        double lo = 99, hi = -99;
        char gf[160] = "";
        host_easy();
        for (k = 0; k < 4u; k++) {
            uint32_t wn[27], nw = 0;
            for (s = 0; s < GN_NSOUNDS && strcmp(GN_SOUNDS[s].name, KITS[k]); s++)
                ;
            gn_sound_put(song.sel, s);
            host_wait_ms(100);
            for (key = 0; key < 27u; key++)
                if (GN_WHITE[key] != 0xFFu)
                    wn[nw++] = gn_key(TSEL, key, 60);
            for (key = 0; key < 27u; key++) {
                uint32_t note, pad, dup = 0;
                if (GN_WHITE[key] != 0xFFu)
                    continue;
                note = gn_key(TSEL, key, 60);
                pad = (note + 128u - 36u) % 16u;
                for (j = 0; j < nw; j++)
                    dup |= wn[j] == note;
                nb++;
                if (!dup && (pad == GN_WHITE[key - 1u] || pad == GN_WHITE[key + 1u]))
                    good++;
                else if (!gf[0])
                    snprintf(gf, sizeof gf, "%s key %u: note %u (pad %u)%s", KITS[k], key, note, pad,
                             dup ? ", a white key's" : ", not beside it");
                if (k == 0u) {                                  /* KIT: each ghost against its drum's own hit */
                    double dw, db;
                    take(TSEL, 36u + pad, gw);
                    take(TSEL, note, gb);
                    dw = rms_db(gw, TAKE);
                    db = rms_db(gb, TAKE);
                    lo = dw - db < lo ? dw - db : lo;
                    hi = dw - db > hi ? dw - db : hi;
                    soft += dw - db >= 3.0 && db > -60.0;
                }
            }
        }
        for (key = 0; key < 27u; key++) {                       /* Advanced: the kit's keys (gnc_drum_keys) */
            uint32_t a = (uint32_t)gnc_drum_keys(TSEL, key), b;
            for (b = 0; b < key && (uint32_t)gnc_drum_keys(TSEL, b) != a; b++)
                ;
            adv += b == key;
        }
        CHECK(good == 44u && nb == 44u, "kits: on KIT, DRUM MACHINE, HAND DRUM and TOMS each black key is a drum beside it "
              "on a note no white key plays (%u of %u)%s%s", good, nb, gf[0] ? ": " : "", gf);
        CHECK(soft == 11u, "kits: KIT's 11 black keys are their drums soft, %.1f to %.1f dB below the white key's hit and "
              "heard (%u of 11)", lo, hi, soft);
        CHECK(adv == 27u, "kits: Advanced's 27 keys play 27 notes, keys 17 to 27 the first eleven pads soft (%u of 27)", adv);
    }

    /* ---- the Gnome's syllables: Easy's Gnome sings a syllable a key (TALK 12, the core's P20) */
    {
        static int16_t g0[TAKE], g1[TAKE], s0[TAKE], s1[TAKE];
        uint32_t s, z[4] = {0, 0, 0, 0}, j, p = song.sel % NTRK;
        double r0, r1;
        track_t *t = &trk[p];
        for (s = 0; s < GN_NSOUNDS && strcmp(GN_SOUNDS[s].name, "GNOME"); s++)
            ;
        gn_sound_put(p, s);
        host_wait_ms(200);
        CHECK(gnc_engine(t->eng_req) == GC_GNOME && t->p[P_E7] == 12 && gna.w[p][20] == 12u,
              "gnome: Easy's GNOME loads with TALK 12, the core's syllables P20 = %u", gna.w[p][20]);
        take(t, 60, s0);                                /* keys C4 and E4: syllables 0 and 4 (ah, oo) */
        take(t, 64, s1);
        t->p[P_E7] = 0;                                 /* TALK OWN: the sound's own vowel on every key */
        take(t, 60, g0);
        take(t, 64, g1);
        for (j = 1; j < TAKE / 2u; j++) {
            z[0] += (s0[j] >= 0) != (s0[j - 1] >= 0);
            z[1] += (s1[j] >= 0) != (s1[j - 1] >= 0);
            z[2] += (g0[j] >= 0) != (g0[j - 1] >= 0);
            z[3] += (g1[j] >= 0) != (g1[j - 1] >= 0);
        }
        r0 = (double)(z[1] + 1) / (z[0] + 1);           /* (the brightness of E4 over C4, syllables on and off) */
        r1 = (double)(z[3] + 1) / (z[2] + 1);
        CHECK(fabs(r0 - r1) / r1 > 0.08, "gnome: with its syllables C4 and E4 sing different vowels (E4/C4 brightness x%.2f, "
              "without them x%.2f)", r0, r1);
        t->p[P_E7] = 12;
        if (wavs) {                                     /* a walk up the white keys, a syllable each */
            static int16_t run[FS * 7u];
            static const uint8_t W[12] = {60, 62, 64, 65, 67, 69, 71, 72, 74, 76, 77, 79};
            uint32_t b = 0;
            quiet(t);
            for (j = 0; j < 12u; j++) {
                trk_note_on(t, W[j], 100);
                blocks(ms(330), run + b * CTL);
                b += ms(330);
                trk_note_off(t, W[j]);
                blocks(ms(170), run + b * CTL);
                b += ms(170);
            }
            wav(wavs, "gnome-syllables", run, b * CTL);
        }
    }

    /* ---- the listening sweeps: a page's knob turned 0 .. top .. 0 over 8 s of notes, on three engines */
    if (wavs) {
        static int16_t sw[FS * 8u];
        static const struct { uint32_t slot; int32_t ce; uint8_t id; const char *name; } SW[] = {
            {GN_ENGI_VA, GC_VA, P_ATK, "va-env-attack"}, {GN_ENGI_VA, GC_VA, P_REL, "va-env-release"},
            {GN_ENGI_VA, GC_VA, P_ED_FLT, "va-envdest-filter"}, {GN_ENGI_VA, GC_VA, P_LRATE, "va-lfo-speed"},
            {GN_ENGI_VA, GC_VA, P_LD_PIT, "va-lfodest-pitch"},
            {GN_ENGI_FM, GC_FM, P_DEC, "fm-env-decay"}, {GN_ENGI_FM, GC_FM, P_LRATE, "fm-lfo-speed"},
            {GN_ENGI_FM, GC_FM, P_E4, "fm-edit2-depth"}, {GN_ENGI_FM, GC_FM, P_E6, "fm-edit2-ratio"},
            {GN_ENGI_GNOME, GC_GNOME, P_ATK, "gnome-env-attack"}, {GN_ENGI_GNOME, GC_GNOME, P_LRATE, "gnome-lfo-speed"},
            {GN_ENGI_GNOME, GC_GNOME, P_LD_AMP, "gnome-lfodest-growl"}, {GN_ENGI_GNOME, GC_GNOME, P_E4, "gnome-edit2-vowel"},
            {GN_ENGI_VA, GC_VA, P_E4, "va-edit2-cutoff"}, {GN_ENGI_DRUMS, GC_DRUMS, P_E5, "drums-edit2-decay"},
            {GN_ENGI_DRUMS, GC_DRUMS, P_E4, "drums-edit2-tone"}};
        static const struct { uint8_t id, lo, hi; const char *name; } SG[] = {
            {G_DTIME, 0, 9, "song-dly-time"}, {G_DFDBK, 0, 120, "song-dly-repeat"}, {G_RSIZE, 0, 127, "song-reverb-size"},
            {G_CRATE, 0, 127, "song-chorus-speed"}, {G_CDEPTH, 0, 127, "song-chorus-depth"}};
        track_t *t = &trk[0];
        host_power_on();
        song.sel = 0;
        for (k = 0; k < NELEM(SW) + NELEM(SG); k++) {
            uint32_t b, step = 0xFFFFFFFFu, frames = FS * 8u;
            int is_song = k >= NELEM(SW);
            const param_desc_t *d = 0;
            int16_t *vp = 0, keep = 0, lo, hi;
            if (!is_song) {
                load(t, SW[k].slot, pick(SW[k].ce));
                if (SW[k].id >= P_E0)
                    d = &gna_desc_of(SW[k].ce)->edit[SW[k].id - P_E0];
                else
                    d = &TP[SW[k].id];
                vp = &t->p[SW[k].id];
                lo = d->min;
                hi = d->max;
            } else {
                load(t, GN_ENGI_VA, pick(GC_VA));
                t->p[P_E1] = 0;
                t->p[P_DLY] = SG[k - NELEM(SW)].id <= G_DMIX ? 100 : 0;
                t->p[P_REV] = SG[k - NELEM(SW)].id == G_RSIZE ? 100 : 0;
                t->p[P_CHOR] = SG[k - NELEM(SW)].id >= G_CRATE ? 127 : 0;
                vp = &song.g[SG[k - NELEM(SW)].id];
                lo = SG[k - NELEM(SW)].lo;
                hi = SG[k - NELEM(SW)].hi;
            }
            keep = *vp;
            quiet(t);
            for (b = 0; b < frames / CTL; b++) {
                uint32_t m = b * CTL * 1000u / FS, a = m < 4000u ? m : 8000u - m;
                *vp = (int16_t)(lo + (int32_t)((hi - lo) * (int32_t)a / 4000));
                if (m / 500u != step) {             /* a note every half second, 350 ms long */
                    static const uint8_t NOTE[4] = {0, 3, 7, 10};
                    step = m / 500u;
                    trk_note_on(t, SW[is_song ? 0 : k].ce == GC_DRUMS && !is_song ? 36u + (step % 4u) * 3u
                                                                               : 57u + NOTE[step % 4u], 100);
                } else if (m % 500u >= 350u && m % 500u < 351u + CTL * 1000u / FS) {
                    trk_all_off(t);
                }
                blocks(1, sw + b * CTL);
            }
            trk_all_off(t);
            *vp = keep;
            wav(wavs, is_song ? SG[k - NELEM(SW)].name : SW[k].name, sw, frames);
        }
        t->p[P_DLY] = t->p[P_REV] = t->p[P_CHOR] = 0;
    }
    if (fig)
        fclose(fig);
    printf(fails ? "GNOMAD ENGINE WIRING: %d FAILED\n" : "GNOMAD ENGINE WIRING: ALL PASSED\n", fails);
    return fails != 0;
}
