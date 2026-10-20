/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kyle Palaniuk */
/* Gnomad (FORK.md) host test: Easy's eight sounds and its drums, heard through the firmware's own
 * audio (tests/gn_host.h: the engines, voices, effects and master as the audio ISR renders them,
 * fx.c mix_block), each picked as a child picks it (PRESETS) and played on the keys:
 *   speak     a 40 ms tap is heard within 25 ms; loud enough and below clipping; the sounds' levels
 *             within a few dB of each other
 *   knobs     each of Bright, Wobble, Echo and Space makes a clear change on each sound, amount 0
 *             against amount 16 (the thresholds and how each is measured: FORK.md, Easy: sounds):
 *               Bright  a held note's level above 2 kHz against the whole: >= 6 dB more; or its
 *                       spectral centroid: >= 1.5 x as high
 *               Wobble  a held note's pitch (its strongest spectral peak, folded into an octave, so an
 *                       arpeggio's octaves do not count) against the same note at amount 0: a swing
 *                       at some rate between 2 and 9 Hz of >= 25 cents peak to peak; or of loudness,
 *                       >= 3 dB
 *               Echo    after a tap, what amount 16 adds to amount 0 (the two renders differ only by
 *                       the delay send: their difference is the echo alone), where the first echo
 *                       falls: no more than 10 dB under the note's first 60 ms
 *               Space   the same for the reverb, 0.1 to 0.5 s after the tap: no more than 12 dB
 *                       under the note
 *   drums     each pad sounds on the speaker (above 500 Hz, the loudest 20 ms of a hit); a tap sounds
 *             as long as a held key (the long release); the toms and the congas, which have no pad,
 *             are far quieter there
 * Each measurement runs in a process of its own from power-on. Prints the table it checks; with an
 * argument (a folder) it also writes each tap as a WAV.
 *   tests/run_fork_tests.sh builds it: build/host-gn/gn_sound_test */
#include "gn_host.h"
#include <math.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

#define CAP_MAX (FS * 3u)
static int32_t cap[CAP_MAX];
static const char *wav_dir;
static double swing(const double *s, const uint8_t *ok, uint32_t n, double rate);

/* ------------------------------------------------------------ analysis --- */
static double rms_db(uint32_t a, uint32_t b)       /* samples a..b of the capture, dBFS */
{
    double e = 0;
    uint32_t i;
    if (b > host_cap_n)
        b = host_cap_n;
    for (i = a; i < b; i++)
        e += (double)cap[i] * cap[i];
    return b > a ? 10 * log10(e / (b - a) / (32768.0 * 32768.0) + 1e-12) : -120;
}

static double peak_of(uint32_t a, uint32_t b)
{
    double p = 0;
    uint32_t i;
    for (i = a; i < b && i < host_cap_n; i++)
        p = fabs((double)cap[i]) > p ? fabs((double)cap[i]) : p;
    return p;
}

/* a biquad (RBJ) high-pass at f, applied in place to x */
static void highpass(double *x, uint32_t n, double f)
{
    double w = 2 * M_PI * f / FS, cw = cos(w), al = sin(w) / (2 * 0.7071067811865476), a0 = 1 + al;
    double b0 = (1 + cw) / 2 / a0, b1 = -(1 + cw) / a0, a1 = -2 * cw / a0, a2 = (1 - al) / a0;
    double x1 = 0, x2 = 0, y1 = 0, y2 = 0;
    uint32_t i;
    for (i = 0; i < n; i++) {
        double y = b0 * x[i] + b1 * x1 + b0 * x2 - a1 * y1 - a2 * y2;
        x2 = x1;
        x1 = x[i];
        y2 = y1;
        y1 = y;
        x[i] = y;
    }
}

static double hf_db(uint32_t a, uint32_t b)        /* the level above 2 kHz against the whole */
{
    static double x[CAP_MAX];
    double e = 0, h = 0;
    uint32_t i, n = b - a;
    for (i = 0; i < n; i++) {
        x[i] = cap[a + i];
        e += x[i] * x[i];
    }
    highpass(x, n, 2000);
    highpass(x, n, 2000);
    for (i = n / 10u; i < n; i++)
        h += x[i] * x[i];
    return 10 * log10((h + 1e-9) / (e + 1e-9));
}

static void fft(double *re, double *im, uint32_t n)   /* radix 2, in place */
{
    uint32_t i, j, k, m;
    for (i = 1, j = 0; i < n; i++) {
        uint32_t bit = n >> 1;
        for (; j & bit; bit >>= 1)
            j ^= bit;
        j ^= bit;
        if (i < j) {
            double t = re[i];
            re[i] = re[j];
            re[j] = t;
            t = im[i];
            im[i] = im[j];
            im[j] = t;
        }
    }
    for (m = 2; m <= n; m <<= 1)
        for (k = 0; k < n; k += m)
            for (j = 0; j < m / 2u; j++) {
                double a = -2 * M_PI * j / m, wr = cos(a), wi = sin(a);
                double tr = wr * re[k + j + m / 2u] - wi * im[k + j + m / 2u];
                double ti = wr * im[k + j + m / 2u] + wi * re[k + j + m / 2u];
                re[k + j + m / 2u] = re[k + j] - tr;
                im[k + j + m / 2u] = im[k + j] - ti;
                re[k + j] += tr;
                im[k + j] += ti;
            }
}

/* the strongest swing of a series (one value a frame, `rate` frames a second) at one frequency between
 * 2 and 9 Hz: a least-squares sine at each frequency, peak to peak */
static double swing(const double *s, const uint8_t *ok, uint32_t n, double rate)
{
    double best = 0, f;
    for (f = 2.0; f <= 9.0; f += 0.1) {
        double sc = 0, ss = 0, cc = 0, ys = 0, yc = 0, mean = 0, a, b, det;
        uint32_t i, m = 0;
        for (i = 0; i < n; i++)
            if (ok[i]) {
                mean += s[i];
                m++;
            }
        if (m < 8u)
            return 0;
        mean /= m;
        for (i = 0; i < n; i++) {
            double t = i / rate, c = cos(2 * M_PI * f * t), sn = sin(2 * M_PI * f * t), y = s[i] - mean;
            if (!ok[i])
                continue;
            cc += c * c;
            ss += sn * sn;
            sc += sn * c;
            yc += y * c;
            ys += y * sn;
        }
        det = cc * ss - sc * sc;
        if (fabs(det) < 1e-9)
            continue;
        a = (yc * ss - ys * sc) / det;
        b = (ys * cc - yc * sc) / det;
        if (2 * sqrt(a * a + b * b) > best)
            best = 2 * sqrt(a * a + b * b);
    }
    return best;
}

/* a held note, frame by frame (2048 samples, a hop of 256): its pitch (the strongest spectral peak
 * between 80 Hz and 4 kHz, in cents, folded into one octave: an arpeggio's octaves do not count) and
 * its loudness (dB) */
enum { WN = 2048, WHOP = 256, WMAX = 1024 };
typedef struct {
    double pc[WMAX], lv[WMAX];
    uint8_t ok[WMAX];
    uint32_t n;
} track_series_t;

static void series_of(const int32_t *x, uint32_t a, uint32_t b, track_series_t *t)
{
    static double re[WN], im[WN];
    uint32_t f, i;
    t->n = 0;
    for (f = a; f + WN <= b && t->n < WMAX; f += WHOP, t->n++) {
        double best = 0, e = 0;
        uint32_t k, kb = 0;
        for (i = 0; i < WN; i++) {
            double w = 0.5 - 0.5 * cos(2 * M_PI * i / WN);
            re[i] = x[f + i] * w;
            im[i] = 0;
            e += (double)x[f + i] * x[f + i];
        }
        fft(re, im, WN);
        for (k = (uint32_t)(80.0 * WN / FS); k < (uint32_t)(4000.0 * WN / FS); k++) {
            double m = re[k] * re[k] + im[k] * im[k];
            if (m > best) {
                best = m;
                kb = k;
            }
        }
        t->lv[t->n] = 10 * log10(e / WN + 1e-9);
        t->ok[t->n] = kb > 1u && best > 1e3;
        if (t->ok[t->n]) {                          /* the peak between bins (on log magnitudes) */
            double l = log(re[kb - 1] * re[kb - 1] + im[kb - 1] * im[kb - 1] + 1e-9), c = log(best);
            double r = log(re[kb + 1] * re[kb + 1] + im[kb + 1] * im[kb + 1] + 1e-9);
            double d = 0.5 * (l - r) / (l - 2 * c + r);
            t->pc[t->n] = 1200 * log2((kb + d) * FS / WN / 440.0);
        }
    }
}

/* Wobble, against the same note at amount 0 (the same frames, the renders are the same but for the
 * knob): the swing of the pitch difference (folded into +-600 cents) and of the loudness difference */
static void wobble_of(const track_series_t *t0, const track_series_t *t1, double *cents, double *db)
{
    static double dp[WMAX], dl[WMAX];
    static uint8_t ok[WMAX], all[WMAX];
    uint32_t i, n = t0->n < t1->n ? t0->n : t1->n;
    for (i = 0; i < n; i++) {
        double d = t1->pc[i] - t0->pc[i];
        dp[i] = d - 1200 * floor(d / 1200 + 0.5);
        ok[i] = t0->ok[i] && t1->ok[i];
        dl[i] = t1->lv[i] - t0->lv[i];
        all[i] = 1;
    }
    *cents = swing(dp, ok, n, (double)FS / WHOP);
    *db = swing(dl, all, n, (double)FS / WHOP);
}

static double centroid_hz(const int32_t *x, uint32_t a, uint32_t b)   /* the spectrum's centre of mass */
{
    static double re[WN], im[WN];
    double num = 0, den = 0;
    uint32_t f, i, k;
    for (f = a; f + WN <= b; f += WN) {
        for (i = 0; i < WN; i++) {
            re[i] = x[f + i] * (0.5 - 0.5 * cos(2 * M_PI * i / WN));
            im[i] = 0;
        }
        fft(re, im, WN);
        for (k = 1; k < WN / 2u; k++) {
            double m = sqrt(re[k] * re[k] + im[k] * im[k]);
            num += m * k * FS / WN;
            den += m;
        }
    }
    return den > 0 ? num / den : 0;
}

/* ------------------------------------------------------- measuring --- */
#define NCAP 2u
static int32_t *capture[NCAP];                      /* shared with the children: two renders */
static uint32_t *capture_n, *capture_on;
static double *capture_delay;

enum { M_TAP, M_HOLD };
/* the key a sound is measured on: C4; a kit's: B4, pad 11, the cowbell */
static uint32_t key_of(uint32_t snd) { return GN_SOUNDS[snd].flags & GNF_PADS ? 18u : 7u; }

/* sound snd on track 1 as a child picks it: ALGORITHM to its picture (from another sound: the first, or
 * on round), PRESETS to it inside the picture */
static void pick_sound(uint32_t snd)
{
    uint32_t pic = GN_SOUNDS[snd].pic;
    host_easy();
    host_turn(EN_ALGO, (int32_t)pic + 1);
    if (snd > GN_GROUP[pic])
        host_turn(EN_PRESET, (int32_t)(snd - GN_GROUP[pic]));
    host_wait_ms(GN_PEEK_MS + 200u);
}
static void pick_picture(uint32_t pic) { pick_sound(GN_GROUP[pic % GN_NPICS]); }

/* into capture[c]: sound pic (a GN_SOUNDS index), knob k at amount a (k < 0: every knob at rest), a 40 ms tap or a
 * note held 1.2 s on key kk, from the key going down to 2.6 s */
static void render(uint32_t c, uint32_t pic, int32_t k, int32_t a, uint32_t mode, uint32_t kk)
{
    pid_t pid;
    fflush(stdout);
    pid = fork();
    if (pid == 0) {
        uint32_t i, hold_ms = mode == M_HOLD ? 1200u : 40u;
        pick_sound(pic);
        if (k >= 0) {
            host_turn(EN_K1 + (uint32_t)k, -40);
            host_turn(EN_K1 + (uint32_t)k, a);
        }
        host_wait_ms(1500);                         /* (the echoes and the reverb of what came before) */
        host_cap = cap;
        host_cap_max = CAP_MAX;
        host_cap_n = 0;
        host_key(kk, 1);
        host_wait_ms(hold_ms);
        host_key(kk, 0);
        host_wait_ms(2600u - hold_ms);
        memcpy(capture[c], cap, host_cap_n * sizeof cap[0]);
        capture_n[c] = host_cap_n;
        for (i = 0; i < host_cap_n && abs(cap[i]) <= 300; i++)
            ;
        capture_on[c] = i;
        capture_delay[c] = delay_samples();
        if (wav_dir && mode == M_TAP && (k < 0 || a == GN_STEPS)) {
            char path[512];
            FILE *w;
            snprintf(path, sizeof path, "%s/%u-%s%s%s.wav", wav_dir, pic, gn_sound_def(pic)->name, k < 0 ? "" : "-",
                     k < 0 ? "" : k == 0 ? "bright" : k == 1 ? "wobble" : k == 2 ? "echo" : "space");
            if ((w = fopen(path, "wb"))) {
                wav_hdr(w, host_cap_n);
                for (i = 0; i < host_cap_n; i++)
                    wav_put(w, cap[i], cap[i]);
                fclose(w);
            }
        }
        _exit(0);
    }
    waitpid(pid, 0, 0);
}

static double level(uint32_t c, uint32_t a, uint32_t b)   /* dBFS of capture c, samples a..b */
{
    memcpy(cap, capture[c], capture_n[c] * sizeof cap[0]);
    host_cap_n = capture_n[c];
    return rms_db(a, b);
}

static double diff_level(uint32_t a, uint32_t b)   /* dBFS of capture 1 - capture 0, samples a..b */
{
    double e = 0;
    uint32_t i, n = capture_n[0] < capture_n[1] ? capture_n[0] : capture_n[1];
    if (b > n)
        b = n;
    for (i = a; i < b; i++) {
        double d = (double)capture[1][i] - capture[0][i];
        e += d * d;
    }
    return b > a ? 10 * log10(e / (b - a) / (32768.0 * 32768.0) + 1e-12) : -120;
}

static int fails;
#define CHECK(c, what)                                                                   \
    do {                                                                                 \
        if (!(c)) {                                                                      \
            fails++;                                                                     \
            printf("    FAIL %s: %s\n", gn_sound_def(s)->name, what);                   \
        }                                                                                \
    } while (0)

/* knob k on sound s at amounts a and b (a M_HOLD render each, captures 0 and 1): Bright's level above 2 kHz
 * against the whole and the spectral centroids; Wobble's swing of pitch and loudness */
static void bright_of(uint32_t s, uint32_t a, uint32_t b, double *hf0, double *hf1, double *c0, double *c1)
{
    uint32_t kk = key_of(s);
    render(0, s, GK_BRIGHT, (int32_t)a, M_HOLD, kk);
    render(1, s, GK_BRIGHT, (int32_t)b, M_HOLD, kk);
    memcpy(cap, capture[0], capture_n[0] * sizeof cap[0]);
    host_cap_n = capture_n[0];
    *hf0 = hf_db(capture_on[0] + FS / 10u, capture_on[0] + FS * 9u / 10u);
    *c0 = centroid_hz(capture[0], capture_on[0] + FS / 10u, capture_on[0] + FS * 9u / 10u);
    memcpy(cap, capture[1], capture_n[1] * sizeof cap[0]);
    host_cap_n = capture_n[1];
    *hf1 = hf_db(capture_on[1] + FS / 10u, capture_on[1] + FS * 9u / 10u);
    *c1 = centroid_hz(capture[1], capture_on[1] + FS / 10u, capture_on[1] + FS * 9u / 10u);
}

static void wobble_at(uint32_t s, uint32_t a, uint32_t b, double *cents, double *trem)
{
    static track_series_t w0, w1;
    uint32_t kk = key_of(s);
    render(0, s, GK_WOBBLE, (int32_t)a, M_HOLD, kk);
    render(1, s, GK_WOBBLE, (int32_t)b, M_HOLD, kk);
    series_of(capture[0], capture_on[0] + FS / 10u, capture_on[0] + FS * 11u / 10u, &w0);
    series_of(capture[1], capture_on[0] + FS / 10u, capture_on[0] + FS * 11u / 10u, &w1);
    wobble_of(&w0, &w1, cents, trem);
}

/* Echo (k GK_ECHO) or Space: what amount b adds to amount a after a tap, where the echo falls or 0.1 to
 * 0.5 s after; *dry the note's first 60 ms at amount a */
static double send_of(uint32_t s, uint32_t k, uint32_t a, uint32_t b, double *dry)
{
    uint32_t kk = key_of(s), on;
    render(0, s, (int32_t)k, (int32_t)a, M_TAP, kk);
    render(1, s, (int32_t)k, (int32_t)b, M_TAP, kk);
    on = capture_on[0];
    *dry = level(0, on, on + FS * 60u / 1000u);
    if (k == GK_ECHO)
        return diff_level(on + (uint32_t)capture_delay[0] - FS / 100u, on + (uint32_t)capture_delay[0] + FS * 6u / 100u);
    return diff_level(on + FS / 10u, on + FS / 2u);
}

/* every sound of the nine pictures (pick 3): each speaks at once; each knob a clear change, amount 0 against
 * 16 (the thresholds: FORK.md, Easy: sounds), and across its whole travel: 0 against 8 and 8 against 16 each
 * at least half that (Bright 3 dB above 2 kHz or the centroid 1.22 x; Wobble 12.5 cents or 1.5 dB; Echo and
 * Space within 16 and 18 dB of the note: a send half as high is 6 dB under) */
static void t_sounds(void)
{
    uint32_t s;
    double lo = 1e9, hi = -1e9, tlo = 1e9, thi = -1e9, alo = 1e9, ahi = -1e9;
    printf("    sound          onset  peak  tap  held | Bright >2k dB 0 8 16, centroid x 0-8 8-16 | Wobble cents 0-16 "
           "0-8 8-16 | Echo 0-16 0-8 8-16 (note) | Space 0-16 0-8 8-16\n");
    for (s = 0; s < GN_NSOUNDS; s++) {
        uint32_t kk = key_of(s), on;
        if (getenv("GN_SOUND_ONLY") && !strstr(getenv("GN_SOUND_ONLY"), GN_SOUNDS[s].name))
            continue;                               /* (by hand: some of the sounds, while tuning them) */
        double onset, peak, tap, held, h0, h8, h16, c0, c8, c16, x0, x1, cents, trem, cents8, trem8, cents16, trem16;
        double dry, d8, echo, echo8, echo16, tail, tail8, tail16;
        render(0, s, -1, 0, M_TAP, kk);
        on = capture_on[0];
        onset = on < capture_n[0] ? on * 1000.0 / FS : -1;
        memcpy(cap, capture[0], capture_n[0] * sizeof cap[0]);
        host_cap_n = capture_n[0];
        peak = peak_of(0, host_cap_n);
        tap = rms_db(on, on + FS * 60u / 1000u);
        render(0, s, -1, 0, M_HOLD, kk);
        held = level(0, capture_on[0] + FS / 10u, capture_on[0] + FS * 7u / 10u);
        bright_of(s, 0, GN_STEPS, &h0, &h16, &c0, &c16);
        bright_of(s, GN_STEPS / 2u, GN_STEPS / 2u, &h8, &x0, &c8, &x1);
        wobble_at(s, 0, GN_STEPS, &cents, &trem);
        wobble_at(s, 0, GN_STEPS / 2u, &cents8, &trem8);
        wobble_at(s, GN_STEPS / 2u, GN_STEPS, &cents16, &trem16);
        echo = send_of(s, GK_ECHO, 0, GN_STEPS, &dry);
        echo8 = send_of(s, GK_ECHO, 0, GN_STEPS / 2u, &d8);
        echo16 = send_of(s, GK_ECHO, GN_STEPS / 2u, GN_STEPS, &d8);
        tail = send_of(s, GK_SPACE, 0, GN_STEPS, &d8);
        tail8 = send_of(s, GK_SPACE, 0, GN_STEPS / 2u, &d8);
        tail16 = send_of(s, GK_SPACE, GN_STEPS / 2u, GN_STEPS, &d8);
        printf("    %-14s %5.1f %5.0f %5.1f %5.1f | %5.1f %5.1f %5.1f, %4.2f %4.2f | %5.0f %5.0f %5.0f (%3.1f %3.1f %3.1f dB)"
               " | %5.1f %5.1f %5.1f (%5.1f) | %5.1f %5.1f %5.1f\n",
               GN_SOUNDS[s].name, onset, peak, tap, held, h0, h8, h16, c8 / (c0 + 1e-9), c16 / (c8 + 1e-9), cents, cents8,
               cents16, trem, trem8, trem16, echo, echo8, echo16, dry, tail, tail8, tail16);
        CHECK(onset >= 0 && onset <= 25, "a 40 ms tap is heard within 25 ms");
        CHECK(peak >= 3000 && peak < 32700, "loud enough, below clipping");
        CHECK(h16 - h0 >= 6 || c16 >= 1.5 * c0, "Bright: >= 6 dB more above 2 kHz, or the centroid 1.5 x as high, at 16 than at 0");
        CHECK(h8 - h0 >= 3 || c8 >= 1.22 * c0, "Bright: 0 to 8 a change (3 dB above 2 kHz, or the centroid 1.22 x)");
        CHECK(h16 - h8 >= 3 || c16 >= 1.22 * c8, "Bright: 8 to 16 a change (3 dB above 2 kHz, or the centroid 1.22 x)");
        CHECK(cents >= 25 || trem >= 3, "Wobble: a swing of >= 25 cents or >= 3 dB at 16 (against 0)");
        CHECK(cents8 >= 12.5 || trem8 >= 1.5, "Wobble: 0 to 8 a swing (12.5 cents or 1.5 dB)");
        CHECK(cents16 >= 12.5 || trem16 >= 1.5, "Wobble: 8 to 16 a swing (12.5 cents or 1.5 dB)");
        CHECK(echo >= dry - 10, "Echo: the first echo no more than 10 dB under the note");
        CHECK(echo8 >= dry - 16 && echo16 >= dry - 16, "Echo: 0 to 8 and 8 to 16 each an echo within 16 dB of the note");
        CHECK(tail >= dry - 12, "Space: the reverb 0.1 to 0.5 s after a tap no more than 12 dB under the note");
        CHECK(tail8 >= dry - 18 && tail16 >= dry - 18, "Space: 0 to 8 and 8 to 16 each within 18 dB of the note");
        if (!(GN_SOUNDS[s].flags & GNF_PADS) && GN_SOUNDS[s].pic != GN_KIT) {
            alo = tap < alo ? tap : alo;
            ahi = tap > ahi ? tap : ahi;
        }
        if (s == GN_GROUP[GN_SOUNDS[s].pic] && GN_SOUNDS[s].pic != GN_KIT) {
            lo = held < lo ? held : lo;
            hi = held > hi ? held : hi;
            tlo = tap < tlo ? tap : tlo;
            thi = tap > thi ? tap : thi;
        }
    }
    s = 0;
    printf("    the eight pictures' first sounds: a held note %.1f to %.1f dBFS (%.1f dB), a tap's first 60 ms %.1f to %.1f (%.1f dB)\n",
           lo, hi, hi - lo, tlo, thi, thi - tlo);
    printf("    every melodic sound: a tap's first 60 ms %.1f to %.1f dBFS (%.1f dB)\n", alo, ahi, ahi - alo);
    CHECK(thi - tlo <= 6, "the eight pictures' first sounds' taps within 6 dB of each other");
    CHECK(ahi - alo <= 10, "every melodic sound's tap within 10 dB of every other");
}

/* ------------------------------------------------------------ drums --- */
/* the loudest 20 ms of a hit above 500 Hz (a 4th-order high-pass), dBFS: what a speaker that small
 * gives back */
static double band_db(const int32_t *x, uint32_t n)
{
    static double y[CAP_MAX];
    double e = 0, best = 0;
    uint32_t i, W = FS / 50u;
    for (i = 0; i < n; i++)
        y[i] = x[i];
    highpass(y, n, 500);
    highpass(y, n, 500);
    for (i = 0; i < n; i++) {
        e += y[i] * y[i];
        if (i >= W)
            e -= y[i - W] * y[i - W];
        if (i + 1u >= W && e > best)
            best = e;
    }
    return 10 * log10(best / W / (32768.0 * 32768.0) + 1e-12);
}

/* the drums as a child plays them: Easy's kit (the ninth picture), the n-th white key; a tap (40 ms)
 * or a key held 1 s; or the GM note itself (a tom or a conga, which no pad plays) through seq.c's
 * input, as a key of the kit would play it */
static void render_drum(uint32_t c, uint32_t pad, uint32_t note, uint32_t hold_ms)
{
    pid_t pid;
    fflush(stdout);
    pid = fork();
    if (pid == 0) {
        uint32_t k;
        pick_picture(GN_KIT);
        host_wait_ms(1500);
        k = gn_part();
        host_cap = cap;
        host_cap_max = CAP_MAX;
        host_cap_n = 0;
        if (note) {
            input_on(&trk[k], note, 100);
            host_wait_ms(hold_ms);
            input_off(&trk[k], note);
        } else {
            for (k = 0; k < 27u && GN_WHITE[k] != pad; k++)
                ;
            host_key(k, 1);
            host_wait_ms(hold_ms);
            host_key(k, 0);
        }
        host_wait_ms(1500u - hold_ms);
        memcpy(capture[c], cap, host_cap_n * sizeof cap[0]);
        capture_n[c] = host_cap_n;
        _exit(0);
    }
    waitpid(pid, 0, 0);
}

static double energy_db(uint32_t c)               /* all of capture c */
{
    double e = 0;
    uint32_t i;
    for (i = 0; i < capture_n[c]; i++)
        e += (double)capture[c][i] * capture[c][i];
    return 10 * log10(e / (32768.0 * 32768.0) + 1e-12);
}

static void t_drums(void)
{
    static const char *const PAD[16] = {"kick", "tambourine", "snare", "snare 2", "rim", "clap", "closed hat",
                                        "pedal hat", "open hat", "ride", "cowbell", "maracas", "claves",
                                        "wood block", "wood block hi", "crash"};
    static const uint8_t LOW[9] = {41, 43, 45, 47, 48, 50, 62, 63, 64};   /* the toms, the congas */
    uint32_t i, s = GN_SND_KIT, short_ok = 1;
    double lo = 0, hi = -200, db[16];
    printf("    the pads above 500 Hz, the loudest 20 ms of a hit (dBFS); a tap against a key held 1 s:\n     ");
    for (i = 0; i < 16u; i++) {
        double tap, held;
        render_drum(0, i, 0, 40);
        db[i] = band_db(capture[0], capture_n[0]);
        tap = energy_db(0);
        render_drum(1, i, 0, 1000);
        held = energy_db(1);
        short_ok &= fabs(tap - held) <= 1.0;
        printf(" %s %.1f (%+.1f)%s", PAD[i], db[i], tap - held, i == 15u ? "\n" : i % 4u == 3u ? ",\n     " : ",");
        if (i) {
            lo = db[i] < lo ? db[i] : lo;
            CHECK(db[i] >= -32.0, "each pad but the kick at least -32 dBFS above 500 Hz");
        }
    }
    CHECK(short_ok, "a 40 ms tap of each pad sounds as a key held 1 s (within 1 dB: the long release)");
    printf("     no pad: the toms (41 43 45 47 48 50) and the congas (62 63 64):");
    for (i = 0; i < 9u; i++) {
        double d;
        render_drum(0, 0, LOW[i], 40);
        d = band_db(capture[0], capture_n[0]);
        hi = d > hi ? d : hi;
        printf(" %.1f", d);
    }
    printf("\n     the quietest pad but the kick %.1f, the loudest tom or conga %.1f; the kick %.1f\n", lo, hi, db[0]);
    CHECK(hi <= lo - 3.0, "the toms and the congas quieter on the speaker than every pad but the kick");
}

/* pick 10: how far each of the kit's knobs reaches above where it rests (rounds 1 to 6: Bright rested at its
 * top, CUT open, so it reached nothing further; Wobble's top was LD_PIT 10, +-1.9 semitones; Space's top the
 * reverb send alone). Now: Bright from rest to the top, the crunch (BITS) added; Wobble's top LD_PIT 30
 * (+-5.6 semitones); Space's top the reverb and the chorus sends. Printed for the report; each must be a
 * clear change */
static void t_drum_reach(void)
{
    const gn_sound_t *d = &GN_SOUNDS[GN_SND_KIT];
    uint32_t s = GN_SND_KIT;
    double h0, h1, c0, c1, dry, sp_rest, sp_top;
    bright_of(s, gn_snd_knob(d, GK_BRIGHT)->rest, GN_STEPS, &h0, &h1, &c0, &c1);
    sp_rest = send_of(s, GK_SPACE, 0, gn_snd_knob(d, GK_SPACE)->rest, &dry);
    sp_top = send_of(s, GK_SPACE, 0, GN_STEPS, &dry);
    printf("    the kit above rest: Bright %u -> 16: above 2 kHz %+.1f dB, the centroid %.2f x; Wobble's top +-%.1f semitones "
           "(rounds 1-6: +-1.9); Space at rest %.1f dB, at 16 %.1f dB (the note %.1f)\n", gn_snd_knob(d, GK_BRIGHT)->rest,
           h1 - h0, c1 / (c0 + 1e-9), gn_snd_knob(d, GK_WOBBLE)->hi * 3.0 / 16.0, sp_rest, sp_top, dry);
    CHECK(h1 - h0 >= 3 || c1 >= 1.22 * c0, "the kit's Bright reaches past its rest (rounds 1-6: it rested at its top)");
    CHECK(gn_snd_knob(d, GK_WOBBLE)->hi > 10, "the kit's Wobble reaches past +-1.9 semitones");
    CHECK(gn_snd_knob(d, GK_SPACE)->id2 != GN_NONE && sp_top > sp_rest + 6, "the kit's Space reaches further");
}

/* round 24: the Whale's sounds are basses: the lowest white key (F3 on the panel, key 0) at the middle octave
 * plays a fundamental at or below A2 (110 Hz), the bass clef's, and above 35 Hz (still a note, not a rumble).
 * The fundamental of a held note: the shortest period whose normalised autocorrelation is within 10 % of the
 * strongest between 25 and 400 Hz (a bass's strong second harmonic is no octave error) */
static double f0_of(const int32_t *x, uint32_t a, uint32_t b)
{
    uint32_t lag, lo = FS / 400u, hi = FS / 25u, i, best = 0;
    static double r[FS / 25u + 1u];
    double e0 = 0, m = 0;
    for (i = a; i < b; i++)
        e0 += (double)x[i] * x[i];
    if (e0 <= 0)
        return 0;
    for (lag = lo; lag <= hi; lag++) {
        double c = 0, e1 = 0;
        for (i = a; i < b; i++) {
            c += (double)x[i] * x[i + lag];
            e1 += (double)x[i + lag] * x[i + lag];
        }
        r[lag] = c / sqrt(e0 * e1 + 1e-9);
        if (r[lag] > m)
            m = r[lag];
    }
    for (lag = lo + 1u; lag < hi; lag++)             /* the first peak near the strongest */
        if (r[lag] >= 0.9 * m && r[lag] >= r[lag - 1u] && r[lag] >= r[lag + 1u]) {
            best = lag;
            break;
        }
    return best ? (double)FS / best : 0;
}

static void t_whale_bass(void)
{
    uint32_t s;
    printf("    the Whale's sounds, the lowest white key (key 0) at the middle octave: the fundamental\n");
    for (s = GN_GROUP[4]; s < GN_GROUP[5]; s++) {
        double f;
        render(0, s, -1, 0, M_HOLD, 0u);
        f = f0_of(capture[0], capture_on[0] + FS / 5u, capture_on[0] + FS * 7u / 10u);
        printf("      %-12s %5.1f Hz (octave %+d)\n", GN_SOUNDS[s].name, f, GN_SOUNDS[s].octave);
        CHECK(GN_SOUNDS[s].pic == 4u && f >= 35.0 && f <= 110.0, "a Whale sound's lowest key plays a bass note (35 to 110 Hz)");
    }
}

int main(int argc, char **argv)
{
    uint32_t c;
    wav_dir = argc > 1 ? argv[1] : 0;
    for (c = 0; c < NCAP; c++)
        if ((capture[c] = mmap(0, CAP_MAX * sizeof(int32_t), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANON, -1, 0)) ==
            MAP_FAILED)
            return 2;
    capture_n = mmap(0, 4096, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANON, -1, 0);
    capture_on = capture_n + NCAP;
    capture_delay = (double *)(capture_on + NCAP);
    if (!getenv("GN_SOUND_ONLY") || strstr(getenv("GN_SOUND_ONLY"), "WHALE"))
        t_whale_bass();
    t_sounds();
    if (!getenv("GN_SOUND_ONLY")) {
        t_drums();
        t_drum_reach();
    }
    printf(fails ? "gn sound tests FAILED\n" : "gn sound tests passed\n");
    return fails ? 1 : 0;
}
