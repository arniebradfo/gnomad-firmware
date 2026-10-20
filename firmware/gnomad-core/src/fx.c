/* Effects lane (SPEC/20-effects.md "Full-rate echo and exact codec",
 * "Silence, clearing and verification"; SPEC/CHANGES-1.2.1-fx for SPACE
 * and the chorus), revision 1.2.1.
 *
 * Pool (exactly 65536 B, host arena): 44100 echo codes at 0, the plate's
 * 9686-word ring at GC_FX_PLATE_OFF, the 1024-sample chorus line at
 * GC_FX_CHORUS_OFF, the rest reserved zero. Samples are stored
 * little-endian through byte access (no pointer punning).
 *
 * SPACE (P1) is a plate reverb: the Griesinger/Dattorro tank as Clouds'
 * reverb runs it (github.com/pichenettes/eurorack clouds/dsp/fx/reverb.h
 * and fx_engine.h, commit d8f473915232af74347762b6365202bfd26f73f6,
 * Copyright 2014 Emilie Gillet, MIT License; src/fx_plate.h carries the
 * notice and the port, NOTICE item 2), rewritten in C99 fixed point: int16 ring, Q15 gains with truncation toward
 * zero, a 22.05-kHz tank between 11-tap halfband filters, our own lengths,
 * LFOs and size law. The chorus (P2) is an ensemble after Rings'
 * (rings/dsp/fx/ensemble.h, same commit and licence): three taps of one
 * line at 120-degree phases of a slow and a fast LFO, level-matched to the
 * single-tap SPEC/20 chorus. The ensemble is the default (GC_FX_ENSEMBLE=1,
 * SPEC/CHANGES-1.2.2-ensemble); GC_FX_ENSEMBLE=0 keeps the single tap. */
#include "gc_internal.h"
#define GC_TABLES_DEFINE_FX
#include "tables.h"

#define CLEAR_FRAMES 176400u /* four seconds of exactly zero send input */
/* The plate's silence clear comes after eight seconds (CHANGES-1.2.1-fx X3):
 * its longest tail (RT60 5.9 s at the largest size) is then about 80 dB
 * down, where four seconds would cut it at about -40 dB. */
#define PLATE_CLEAR_FRAMES 352800u
/* log2(GC_KRATE): the ensemble taps' per-frame step = difference/GC_KRATE */
#define GC_KRATE_SHIFT                                                        \
  (GC_KRATE == 1 ? 0u : GC_KRATE == 2 ? 1u : GC_KRATE == 4 ? 2u : GC_KRATE == 8 ? 3u :     \
   GC_KRATE == 16 ? 4u : GC_KRATE == 32 ? 5u : 6u)
#define ECHO_LEN 44100u

static inline int32_t ld16(const uint8_t *p) {
  int32_t v = (int32_t)((uint32_t)p[0] | ((uint32_t)p[1] << 8));
  return v >= 32768 ? v - 65536 : v;
}
static inline void st16(uint8_t *p, int32_t v) {
  gc_wr16(p, (uint16_t)(v < 0 ? v + 65536 : v));
}

/* Exact nearest-code encoder: smallest q with U[q] >= |x| by binary search,
 * then the lower code on ties; sign bit only for nonzero codes. */
static uint8_t echo_encode(int32_t x) {
  uint32_t a = (uint32_t)(x < 0 ? -x : x), lo = 0, hi = 127;
  while (lo < hi) {
    uint32_t mid = (lo + hi) >> 1;
    if (gc_echo_u[mid] >= a) {
      hi = mid;
    } else {
      lo = mid + 1u;
    }
  }
  if (lo > 0u && gc_echo_u[lo] >= a &&
      a - gc_echo_u[lo - 1u] <= (uint32_t)gc_echo_u[lo] - a) {
    lo--;
  }
  return (uint8_t)(lo + ((x < 0 && lo != 0u) ? 128u : 0u));
}

static int32_t echo_decode(uint8_t b) {
  int32_t u = gc_echo_u[b & 127u];
  return (b & 128u) ? -u : u;
}

/* Exposed for tests: the SPEC/20 codec. */
uint8_t gc_echo_encode(int32_t x) { return echo_encode(gc_S15(x)); }
int32_t gc_echo_decode(uint8_t b) { return echo_decode(b); }

/* Zero-input clear bookkeeping (SPEC/20): returns 1 on the frame that must
 * clear and return zero (the CLEAR_FRAMES-th consecutive zero input). */
static int silent(uint32_t *count, uint8_t *done, int32_t in, uint32_t frames) {
  if (in != 0) {
    *count = 0;
    *done = 0;
    return 0;
  }
  if (*count < frames) {
    (*count)++;
  }
  if (*count == frames && !*done) {
    *done = 1;
    return 1;
  }
  return 0;
}

/* Lazy clears (SPEC/CHANGES-1.2.1-fx X1): the clear frame resets the write
 * count, so no slot written before it is ever read again (reads of older
 * slots return zero); identical PCM to zeroing the line, O(1) per clear. */
static void clear_echo(gc_core *c) {
  c->fx.ecnt = 0;
  c->fx.eerr = 0;
  c->fx.eold = c->fx.enew; /* fade completes as part of clearing */
  c->fx.ek = 64;
  c->fx.eq = 1;
}
static void clear_plate(gc_core *c) {
  unsigned i;
  c->fx.pcnt = 0;
  c->fx.lp1 = 0;
  c->fx.lp2 = 0;
  for (i = 0; i < 8u; i++) {
    c->fx.pup[i] = 0;
  }
  for (i = 0; i < 16u; i++) {
    c->fx.pdn[i] = 0;
  }
  c->fx.rq = 1;
}
static void clear_chorus(gc_core *c) {
  c->fx.ccnt = 0;
  c->fx.cq = 1;
}

/* Echo taps settle on the current Echo time control (init, reset, PANIC,
 * gc_set_initial): old tap = target, fade complete. */
void gc_fx_settle(gc_core *c) {
  c->fx.enew = c->mstep[GC_MS_ECHO_TIME];
  c->fx.eold = c->fx.enew;
  c->fx.ek = 64;
}

/* PANIC on track 255: all wet audio state cleared (lazily, O(1): every
 * write count zero), controls, LFO phases and control-rate values kept. */
void gc_fx_panic(gc_core *c) {
  c->fx.ezero = c->fx.czero = c->fx.rzero = 0;
  c->fx.eclr = c->fx.cclr = c->fx.rclr = 0;
  c->fx.ew = 0;
  c->fx.cw = 0;
  c->fx.pwp = 0;
  c->fx.pdi = c->fx.pui = 0;
  clear_echo(c);
  clear_plate(c);
  clear_chorus(c);
  gc_fx_settle(c);
}

/* init/reset (never on the audio path): every scalar zero, phases restart,
 * the pool bytes zeroed too, ensemble taps at the line centre. */
void gc_fx_reset(gc_core *c) {
  unsigned i;
  gc_zero(c->fx_pool, GC_FX_BYTES);
  gc_zero(&c->fx, sizeof(c->fx));
  c->fx.pkey = 65536u;
  for (i = 0; i < 3u; i++) {
    c->fx.ctap[i] = (int32_t)(GC_ENS_CENTER << 16);
  }
  gc_fx_panic(c);
}

static int32_t echo_tap(const gc_core *c, uint32_t d) {
  uint32_t w = c->fx.ew;
  if (d > c->fx.ecnt) { /* written before the last clear: logically zero */
    return 0;
  }
  return echo_decode(c->fx_pool[w >= d ? w - d : w + ECHO_LEN - d]);
}

static int32_t fx_echo(gc_core *c, int32_t in) {
  int32_t r;
  uint16_t want = c->mstep[GC_MS_ECHO_TIME];
  if (want != c->fx.enew) { /* time change: old tap = previous target */
    c->fx.eold = c->fx.enew;
    c->fx.enew = want;
    c->fx.ek = 0;
  }
  if (c->fx.ek < 64u) {
    c->fx.ek++; /* fade continues on a clear frame, too */
  }
  if (silent(&c->fx.ezero, &c->fx.eclr, in, CLEAR_FRAMES)) {
    clear_echo(c);
    return 0;
  }
  if (c->fx.eq) { /* logically empty line and zero input: output zero */
    if (in == 0) {
      return 0;
    }
    c->fx.eq = 0;
  }
  r = echo_tap(c, c->fx.enew); /* read BEFORE write */
  if (c->fx.ek < 64u) {
    int32_t o = echo_tap(c, c->fx.eold); /* L(old,new,k,64) */
    r = o + (int32_t)gc_Rp2((int64_t)(r - o) * c->fx.ek, 6u);
  }
  {
    /* First-order error feedback (CHANGES-1.1.2 C21): quantise the target
     * plus the previous residual; keep the new residual (<= half a step). */
    int32_t w = gc_S15((int64_t)gc_S15((int64_t)in + gc_G(r, c->mramp[GC_MR_ECHO_FB].cur)) +
                       c->fx.eerr);
    uint8_t code = echo_encode(w);
    c->fx.eerr = w - echo_decode(code);
    c->fx_pool[c->fx.ew] = code;
  }
  if (c->fx.ecnt < ECHO_LEN) {
    c->fx.ecnt++;
  }
  c->fx.ew = (uint16_t)(c->fx.ew + 1u == ECHO_LEN ? 0u : c->fx.ew + 1u);
  return r;
}

#include "fx_plate.h"

/* One frame of the three shared processors; returns echo+chorus+reverb for
 * the mixer's G(.,wet level). Inputs are the S15 send buses. LFOs and the
 * control-rate values advance every frame; then each processor's zero-input
 * count and clear (SPEC/20); a quiet processor (logically empty, zero
 * input) returns zero without its recurrence (X2), which is exactly what
 * the recurrence would return. */
int32_t gc_fx_process(gc_core *c, int32_t echo_in, int32_t space_in,
                      int32_t chorus_in, uint32_t *sat) {
  int32_t e, ch = 0, rv = 0;
  uint32_t lfo = c->fx.clfo; /* this frame's chorus phase, before advance */
  (void)sat; /* no internal S18 stage in these processors */
  chorus_rate(c);
  if (c->fx.kk == 0u) { /* control tick: LFO-derived values (GC_KRATE) */
#if GC_FX_ENSEMBLE
    chorus_targets(c);
#endif
    c->fx.poff1 = (GC_PLATE_SMEAR_C << 16) +
                  GC_PLATE_SMEAR_A * (uint32_t)(32768 + gc_sin32(c->fx.pl1));
    c->fx.poff2 = (GC_PLATE_MOD_C << 16) +
                  GC_PLATE_MOD_A * (uint32_t)(32768 + gc_sin32(c->fx.pl2));
  }
  c->fx.kk = (uint8_t)((c->fx.kk + 1u) & (GC_KRATE - 1u));
  c->fx.clfo += c->fx.cinc;
  c->fx.pl1 += GC_PLATE_LFO1_INC;
  c->fx.pl2 += GC_PLATE_LFO2_INC;
#if GC_FX_ENSEMBLE
  c->fx.clfo2 += c->fx.cinc2;
  {
    uint32_t k;
    for (k = 0; k < 3u; k++) {
      c->fx.ctap[k] += c->fx.cstep[k];
    }
  }
#endif
  e = fx_echo(c, echo_in);
  if (silent(&c->fx.czero, &c->fx.cclr, chorus_in, CLEAR_FRAMES)) {
    clear_chorus(c);
    (void)lfo;
  } else if (!c->fx.cq || chorus_in != 0) {
    c->fx.cq = 0;
#if GC_FX_ENSEMBLE
    (void)lfo;
    ch = fx_chorus(c, chorus_in);
#else
    ch = fx_chorus(c, chorus_in, lfo);
#endif
  }
  if (silent(&c->fx.rzero, &c->fx.rclr, space_in, PLATE_CLEAR_FRAMES)) {
    clear_plate(c);
  } else if (!c->fx.rq || space_in != 0) {
    c->fx.rq = 0;
    plate_law(c);
    rv = fx_plate(c, space_in);
  }
  c->fx.pp ^= 1u;
  return e + ch + rv;
}
