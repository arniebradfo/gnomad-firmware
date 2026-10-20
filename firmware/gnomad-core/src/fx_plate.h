// Copyright 2014 Emilie Gillet.
//
// Author: Emilie Gillet (emilie.o.gillet@gmail.com)
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//
// See http://creativecommons.org/licenses/MIT/ for more information.
//
// -----------------------------------------------------------------------------
//
// The SPACE plate and the ensemble chorus in this file are a C99 fixed-point port of
// the reverb and ensemble of github.com/pichenettes/eurorack (clouds/dsp/fx/
// reverb.h and fx_engine.h; rings/dsp/fx/ensemble.h) at commit
// d8f473915232af74347762b6365202bfd26f73f6, MIT, header above. Changes made
// for the Gnomad sound core (2026-10-12, SPEC/CHANGES-1.2.1-fx, PROVENANCE.md):
// float became int16 lines and Q15 gains truncated toward zero; the tank runs
// at 22.05 kHz between halfband filters with its line lengths scaled by
// 0.5914 to the nearest primes; our own LFOs, size law, output level and
// lazy clears; the ensemble is mono with our centre, depth/rate controls and
// tap weight. Included once by src/fx.c (owner-authored echo, codec, silence
// logic and the frame driver, SPEC/20), whose helpers it uses.
#ifndef GC_FX_PLATE_H
#define GC_FX_PLATE_H

/* ---- WOBBLE's chorus: the ensemble (P2) ---------------------------- */

/* Exact increment caches: slow = R(rate*2^32,4410000) (SPEC/20), fast =
 * floor(slow*279/32), 8.72x the slow rate (Rings' 1.37e-4/1.57e-5). */
static void chorus_rate(gc_core *c) {
  if (c->mstep[GC_MS_CHORUS_RATE] != c->fx.ckey) {
    c->fx.ckey = c->mstep[GC_MS_CHORUS_RATE];
    c->fx.cinc = (uint32_t)GC_RK((int64_t)c->fx.ckey * 4294967296LL, 4410000u);
    c->fx.cinc2 = (uint32_t)(((uint64_t)c->fx.cinc * 279u) >> 5);
  }
}

static inline int32_t chorus_rd(const gc_core *c, const uint8_t *line, uint32_t i) {
  /* age 0 = the slot written this frame; older than the count: zero */
  return ((c->fx.cw - i) & 1023u) < c->fx.ccnt ? ld16(line + 2u * i) : 0;
}

/* Control tick: tap k's delay target is GC_ENS_CENTER + depth*(sin(slow_k) +
 * sin(fast_k)/10) frames, slow_k/fast_k the two LFOs at k*120 degrees,
 * depth = Chorus depth (0..192 frames); Q16 = floor of
 * (depth*(10*s+f)*13107 + 2^15) / 2^16 (13107/2^16 = 0.19999 ~ 65536/327670).
 * The taps then move linearly to the targets over the next GC_KRATE
 * frames. Delays stay within 800 +- 211.2 frames of the 1024-word line. */
#if GC_FX_ENSEMBLE
static void chorus_targets(gc_core *c) {
  uint32_t depth = c->mramp[GC_MR_CHORUS_DEPTH].cur, k;
  for (k = 0; k < 3u; k++) {
    int32_t s = gc_sin32(c->fx.clfo + k * 1431655765u);
    int32_t f = gc_sin32(c->fx.clfo2 + k * 1431655765u);
    int32_t t = (int32_t)(GC_ENS_CENTER << 16) +
                (int32_t)gc_Rp2((int64_t)(int32_t)depth * (10 * s + f) * 13107, 16u);
    c->fx.cstep[k] = GC_KRATE == 1 ? t - c->fx.ctap[k]
                                   : (int32_t)gc_floor2(t - c->fx.ctap[k], GC_KRATE_SHIFT);
  }
}
#endif

#if GC_FX_ENSEMBLE
static int32_t fx_chorus(gc_core *c, int32_t in) {
  uint8_t *line = c->fx_pool + GC_FX_CHORUS_OFF;
  int32_t sum = 0;
  uint32_t k;
  st16(line + 2u * c->fx.cw, in);
  if (c->fx.ccnt < 1024u) {
    c->fx.ccnt++;
  }
  for (k = 0; k < 3u; k++) {
    uint32_t p = (((uint32_t)c->fx.cw << 16) - (uint32_t)c->fx.ctap[k]) & ((1024u << 16) - 1u);
    uint32_t i = p >> 16;
    int32_t a = chorus_rd(c, line, i), b = chorus_rd(c, line, (i + 1u) & 1023u);
    sum += a + (int32_t)gc_Rp2((int64_t)(b - a) * (int64_t)(p & 0xffffu), 16u);
  }
  c->fx.cw = (uint16_t)((c->fx.cw + 1u) & 1023u);
  /* x 5/8 per tap: Rings weighs each tap 0.33; three partly decorrelated
   * taps then return 5.5 dB less energy than the single-tap chorus did, so
   * 5/8 (+5.5 dB) keeps WOBBLE's chorus amount where content left it. */
  return gc_Rp2s(sum * 5, 3u);
}
#else
/* The SPEC/20 single-tap chorus: delay_Q16 = 528*65536 + R(depth*65536*
 * sin32(lfo),32767), lfo = this frame's phase before its advance. */
static int32_t fx_chorus(gc_core *c, int32_t in, uint32_t lfo) {
  uint8_t *line = c->fx_pool + GC_FX_CHORUS_OFF;
  int32_t sl = gc_sin32(lfo), a, b;
  uint32_t p, i, dm;
  st16(line + 2u * c->fx.cw, in);
  if (c->fx.ccnt < 1024u) {
    c->fx.ccnt++;
  }
  /* |n| < 192*65536*32767 < 2^40: the magnitude by the exact nested /32767
   * fold, then the sign. */
  dm = (uint32_t)gc_fdiv32767((uint64_t)c->mramp[GC_MR_CHORUS_DEPTH].cur * 65536u *
                                  (uint32_t)(sl < 0 ? -sl : sl) +
                              16383u);
  p = ((uint32_t)c->fx.cw << 16) - (uint32_t)(528 * 65536) +
      (sl < 0 ? dm : (uint32_t)0 - dm);
  p &= (1024u << 16) - 1u; /* Euclidean modulo 1024*65536 */
  i = p >> 16;
  a = chorus_rd(c, line, i);
  b = chorus_rd(c, line, (i + 1u) & 1023u);
  c->fx.cw = (uint16_t)((c->fx.cw + 1u) & 1023u);
  return a + (int32_t)gc_Rp2((int64_t)(b - a) * (int64_t)(p & 0xffffu), 16u);
}
#endif

/* ---- SPACE: the plate (P1) ------------------------------------------ */

enum { AP1, AP2, AP3, AP4, DAP1A, DAP1B, DEL1, DAP2A, DAP2B, DEL2 };
#define KAP GC_PLATE_KAP
#define GIN GC_PLATE_GIN

/* trunc(x*k/2^15) toward zero; |x| < 2^18, k < 2^14, or |x| < 2^17, k < 2^15
 * (the product's magnitude stays below 2^32). */
static inline int32_t pm(int32_t x, uint32_t k) {
  uint32_t m = ((x < 0 ? (uint32_t)0 - (uint32_t)x : (uint32_t)x) * k) >> 15;
  return x < 0 ? -(int32_t)m : (int32_t)m;
}
static inline int32_t sat16(int32_t x) {
  return x > 32767 ? 32767 : (x < -32768 ? -32768 : x);
}

/* Word at offset off of line k (ring index wp+base+off, wrapped once). */
static inline uint8_t *pw(gc_core *c, unsigned k, uint32_t off) {
  uint32_t i = c->fx.pwp + plate_base[k] + off;
  if (i >= GC_FX_PLATE_WORDS) {
    i -= GC_FX_PLATE_WORDS;
  }
  return c->fx_pool + GC_FX_PLATE_OFF + 2u * i;
}
/* A read of a word last written `age` ticks ago: zero if before the clear. */
static inline int32_t prd(gc_core *c, unsigned k, uint32_t off, uint32_t age) {
  return age <= c->fx.pcnt ? ld16(pw(c, k, off)) : 0;
}
/* Linear interpolation at Q16 offset q (Clouds' Interpolate). */
static inline int32_t pint(gc_core *c, unsigned k, uint32_t q) {
  uint32_t o = q >> 16;
  int32_t a = prd(c, k, o, o), b = prd(c, k, o + 1u, o + 1u);
  return a + pm(b - a, (q & 0xffffu) >> 1);
}
/* Clouds' Read(TAIL, s) then WriteAllPass(s'): returns the new accumulator.
 * neg: the line read with -kap and written with +kap (dap1a, dap2b). */
static inline int32_t pap(gc_core *c, unsigned k, int32_t acc, int neg) {
  int32_t r = prd(c, k, plate_tail[k], plate_age[k]), w;
  acc += neg ? -pm(r, KAP) : pm(r, KAP);
  w = sat16(acc);
  st16(pw(c, k, 0), w);
  return neg ? r + pm(w, KAP) : r - pm(w, KAP);
}

/* Size law (P1): s = Reverb feedback / 65535; krt = 0.114 + 0.686*s^2,
 * klp = 0.7226 + 0.1414*s (in-loop low-pass, about 4.5..7 kHz at 22.05 kHz):
 * Q15 krt = 3736 + G(G(c,c),22479), 1-klp = 9090 - G(c,4634). */
static void plate_law(gc_core *c) {
  uint32_t v = c->mramp[GC_MR_REVERB_FB].cur;
  if (v != c->fx.pkey) {
    c->fx.pkey = v;
    c->fx.pkrt = (uint16_t)(3736u + gc_Gu(gc_Gu(v, v), 22479u));
    c->fx.poml = (uint16_t)(13926u - gc_Gu(v, 6062u));
  }
}

/* One tank tick at 22.05 kHz; u is the decimated S15 input. Returns the
 * mono tank output del1 + del2 (Clouds' (wet_l + wet_r)/2, tank units). */
static int32_t plate_tick(gc_core *c, int32_t u) {
  int32_t acc, apout, w1, w2;
  uint32_t krt = c->fx.pkrt, oml = c->fx.poml;
  c->fx.pwp = (uint16_t)(c->fx.pwp ? c->fx.pwp - 1u : GC_FX_PLATE_WORDS - 1u);
  /* Smear ap1 inside the loop: rewrite offset W from offset 6..41. */
  st16(pw(c, AP1, GC_PLATE_SMEAR_W), pint(c, AP1, c->fx.poff1));
  /* Diffuse through four allpasses. */
  acc = pm(u, GIN);
  acc = pap(c, AP1, acc, 0);
  acc = pap(c, AP2, acc, 0);
  acc = pap(c, AP3, acc, 0);
  acc = pap(c, AP4, acc, 0);
  apout = acc;
  /* Loop, first half: modulated del2 read, damping, two allpasses, del1. */
  acc = apout + pm(pint(c, DEL2, c->fx.poff2), krt);
  c->fx.lp1 = acc - pm(acc - c->fx.lp1, oml);
  acc = pap(c, DAP1A, c->fx.lp1, 1);
  acc = pap(c, DAP1B, acc, 0);
  w1 = sat16(acc);
  st16(pw(c, DEL1, 0), w1);
  /* Second half: del1, damping, two allpasses, del2. */
  acc = apout + pm(prd(c, DEL1, plate_tail[DEL1], plate_age[DEL1]), krt);
  c->fx.lp2 = acc - pm(acc - c->fx.lp2, oml);
  acc = pap(c, DAP2A, c->fx.lp2, 0);
  acc = pap(c, DAP2B, acc, 1);
  w2 = sat16(acc);
  st16(pw(c, DEL2, 0), w2);
  if (c->fx.pcnt < 4095u) {
    c->fx.pcnt++;
  }
  return w1 + w2;
}

/* 44.1-kHz wrapper: 11-tap halfband (3,0,-25,0,150,256,150,0,-25,0,3)/512
 * decimator in, tank on every second frame, the same halfband as the 2x
 * interpolator out (polyphase: on tick frames the even taps over the last
 * six tank outputs, between them the tank output two ticks old), then
 * S15 at Clouds' mono level (x4 tank units, GC_PLATE_GOUT/2^16). */
static int32_t fx_plate(gc_core *c, int32_t in) {
  const int16_t *d = c->fx.pdn;
  const int32_t *t = c->fx.pup;
  uint32_t i = c->fx.pdi = (uint8_t)((c->fx.pdi + 1u) & 15u), j;
  int32_t y;
  c->fx.pdn[i] = (int16_t)in;
  if (c->fx.pp) {
    int32_t u = 3 * (d[i] + d[(i - 10u) & 15u]) - 25 * (d[(i - 2u) & 15u] + d[(i - 8u) & 15u]) +
                150 * (d[(i - 4u) & 15u] + d[(i - 6u) & 15u]) + 256 * d[(i - 5u) & 15u];
    j = c->fx.pui = (uint8_t)((c->fx.pui + 1u) & 7u);
    c->fx.pup[j] = plate_tick(c, gc_Rp2s(u, 9u));
    y = gc_Rp2s(3 * (t[j] + t[(j - 5u) & 7u]) - 25 * (t[(j - 1u) & 7u] + t[(j - 4u) & 7u]) +
                    150 * (t[(j - 2u) & 7u] + t[(j - 3u) & 7u]),
                8u);
  } else {
    y = t[(c->fx.pui - 2u) & 7u];
  }
  return gc_S15(gc_Rp2((int64_t)y * GC_PLATE_GOUT, 16u));
}

#endif
