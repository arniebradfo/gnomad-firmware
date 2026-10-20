// Copyright 2013 Emilie Gillet.
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
// Gnomad drum building blocks: a C99 port of the integer state-variable
// filter, the pulse excitation and the six-square metallic source of the
// eurorack "peaks/drums" models (files svf.h, excitation.h, bass_drum.cc,
// snare_drum.cc, high_hat.cc, fm_drum.cc of github.com/pichenettes/eurorack
// at commit d8f473915232af74347762b6365202bfd26f73f6, MIT, header above).
//
// Changes made for the Gnomad sound core (2026-10-11), see PROVENANCE.md:
// classes became static inline C functions with new names; coefficients are
// computed per hit for 44.1 kHz (no upstream lookup table is copied; the
// cutoff and damping laws are evaluated by the engine); signed right shifts
// are written as floor divisions (dr_asr) so the arithmetic is defined C99;
// the pulse delay counters are replaced by comparisons with the hit age; for
// noise/metal inputs the filter's f*hp product is split so it cannot leave
// int32 (dr_svf_w).
// The model topologies that use these blocks are in src/drums.c.
#ifndef GC_DRUM_BLOCKS_H
#define GC_DRUM_BLOCKS_H

#include "gc_internal.h"

/* Note-on helpers stay out of line: the firmware's target cost counts every
 * instruction of the engine's functions, so one copy beats six inlined. */
#if defined(__GNUC__) || defined(__clang__)
#define DR_NOINLINE __attribute__((noinline))
#else
#define DR_NOINLINE
#endif

/* floor(x / 2^s): the arithmetic right shift of the upstream code, written
 * without an implementation-defined signed shift (compiles to one ASR). */
static inline int32_t dr_asr(int32_t x, unsigned s) {
  return x >= 0 ? (int32_t)((uint32_t)x >> s) : -(int32_t)(~(uint32_t)x >> s) - 1;
}

/* CLIP of the upstream code: [-32767, 32767]. */
static inline int32_t dr_clip(int32_t x) {
  return x < -32767 ? -32767 : (x > 32767 ? 32767 : x);
}

/* One step of the upstream two-pole state-variable filter (Svf::Process):
 * notch = in - bp*damp; lp += f*bp; hp = notch - lp; bp += f*hp (Q15, both
 * states clipped). Returns the high-pass output; band and low pass are in
 * the state. f and damp are passed in so callers can modulate them. Used
 * where f*hp provably fits int32 (the pinged bodies: small f). */
static inline int32_t dr_svf(gc_dr_svf *s, int32_t in, int32_t f, int32_t damp) {
  int32_t notch = in - dr_asr(s->bp * damp, 15);
  int32_t lp = dr_clip(s->lp + dr_asr(f * s->bp, 15));
  int32_t hp = notch - lp;
  s->lp = lp;
  s->bp = dr_clip(s->bp + dr_asr(f * hp, 15));
  return hp;
}

/* The same step for noise and metal inputs at high cutoffs, where f*hp can
 * leave int32 (upstream's product would overflow): floor(f*hp/2^15) is
 * formed as f*floor(hp/2^15) + floor(f*(hp mod 2^15)/2^15), exact for any
 * hp, equal to the upstream value whenever that fits. */
static inline int32_t dr_svf_w(gc_dr_svf *s, int32_t in, int32_t f, int32_t damp) {
  int32_t notch = in - dr_asr(s->bp * damp, 15);
  int32_t lp = dr_clip(s->lp + dr_asr(f * s->bp, 15));
  int32_t hp = notch - lp;
  s->lp = lp;
  s->bp = dr_clip(s->bp + f * dr_asr(hp, 15) + dr_asr(f * (hp & 0x7fff), 15));
  return hp;
}

/* The upstream filter's "punch": while the low-pass state is loud it raises
 * the frequency (a pitch sweep that follows the level) and the damping (a
 * faster initial decay). punch is the squared amount (0..255). */
static inline int32_t dr_svf_punch_bp(gc_dr_svf *s, int32_t in, int32_t f, int32_t punch) {
  int32_t ps = s->lp > 4096 ? s->lp : 2048; /* >= 2048: plain shifts */
  (void)dr_svf(s, in, f + (int32_t)(((uint32_t)ps >> 4) * (uint32_t)punch >> 9),
               s->damp + (int32_t)((uint32_t)(ps - 2048) >> 3));
  return s->bp;
}

/* Excitation::Process with the level added on the trigger sample:
 * state = state * decay / 4096 (decay Q12, state >= 0). */
static inline int32_t dr_exc(int32_t state, int32_t decay) {
  return dr_asr(state * decay, 12);
}

/* The six square oscillators of the metallic source (HighHat::Process):
 * the sum of their sign bits, scaled by 4096 (0..24576). */
static inline int32_t dr_metal(uint32_t *ph, const uint32_t *inc) {
  uint32_t n;
  ph[0] += inc[0];
  ph[1] += inc[1];
  ph[2] += inc[2];
  ph[3] += inc[3];
  ph[4] += inc[4];
  ph[5] += inc[5];
  n = (ph[0] >> 31) + (ph[1] >> 31) + (ph[2] >> 31) + (ph[3] >> 31) + (ph[4] >> 31) +
      (ph[5] >> 31);
  return (int32_t)(n << 12);
}

#endif /* GC_DRUM_BLOCKS_H */
