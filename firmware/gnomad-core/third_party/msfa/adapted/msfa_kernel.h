/*
 * Copyright 2012 Google Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/* MODIFIED FOR GNOMAD (2026-10-11; see ../CHANGES.md and ../NOTICE).
 *
 * C99 adaptation of the msfa ("music-synthesizer-for-android", upstream
 * commit f67d41d313b7dc85f6fb99e79e515cc9d208cfff) FM operator kernel:
 *   sin.h     Sin::lookup            -> msfa_sin_lookup
 *   exp2.h    Exp2::lookup           -> msfa_exp2_lookup
 *   fm_op_kernel.cc FmOpKernel::compute / compute_pure / compute_fb: the
 *             per-sample body (gain step, sine of phase+input, Q24 gain
 *             product, two-sample averaged feedback) -> msfa_dgain,
 *             msfa_mulq24, msfa_fb_in; the block loop itself lives in the
 *             caller (src/fm.c), which runs it sample-major over a block of
 *             2^LG_N samples (identical numbers to the op-major upstream loop:
 *             every operator reads only earlier operators' same-sample
 *             outputs).
 *   env.cc    Env::getsample stepping law -> msfa_env_step.
 * Changes: classes to static inline C99 functions; C++/NEON paths dropped
 * (the NEON kernel is a float approximation, not the scalar reference);
 * every signed right shift replaced by an exact biased unsigned floor
 * (SPEC/02 forbids implementation-defined signed shifts; results are
 * identical to the upstream arithmetic shift); tables are generated offline
 * by tools/gen_tables.py from the upstream init recipes (no runtime init,
 * no libm); the exp2 table stores only the y column (dy is formed here,
 * the same values as upstream's interleaved SIN_DELTA-style layout);
 * feedback takes a Q32 multiplier instead of a shift (fbm = 2^(32-shift)
 * reproduces upstream exactly); the envelope rate is supplied by the
 * caller instead of DX rate codes (the stepping law is upstream's).
 *
 * Requires before inclusion: <stdint.h>, and the generated tables
 *   const int32_t msfa_sin[2048];   (dy, y0) pairs, 1024 steps, Q24
 *   const uint32_t msfa_exp2[1024]; y = 2^(i/1024) in Q30
 */
#ifndef GNOMAD_MSFA_KERNEL_H
#define GNOMAD_MSFA_KERNEL_H

/* Sin::lookup (SIN_LG_N_SAMPLES 10, SIN_DELTA): Q24 phase (only the low 24
 * bits matter) to Q24. |dy| <= 102943 and lowbits < 2^14, so dy*lowbits fits
 * int32 and floor(dy*lowbits/2^14) is a biased unsigned shift. */
static inline int32_t msfa_sin_lookup(uint32_t phase) {
  uint32_t lowbits = phase & 0x3fffu;
  uint32_t i = (phase >> 13) & 0x7feu;
  int32_t dy = msfa_sin[i];
  int32_t y0 = msfa_sin[i + 1u];
  uint32_t t = (uint32_t)(dy * (int32_t)lowbits) + 0x80000000u;
  return y0 + ((int32_t)(t >> 14) - (int32_t)(0x80000000u >> 14));
}

/* Exp2::lookup (EXP2_LG_N_SAMPLES 10): Q24 in, Q24 out. Domain: every
 * int32 x < 6*2^24 (above it the result does not fit upstream's int32).
 * xi = floor(x/2^24) is in [-128, 5], so the final shift 6 - xi is 1..134;
 * y < 2^31 is nonnegative, so a shift of 31 or more gives floor(y/2^s) = 0,
 * and shifts of 32 or more (x < -25*2^24, undefined for a 32-bit shift in
 * C and in upstream's int shift alike) return that 0 explicitly. The FM
 * engine uses x in [floor - 14, 0] doublings (shift 6..20); the msfa-ref
 * parity range [-16, 6) doublings is unchanged. floor(x/2^24) is formed
 * with a bias; upstream's int shift of nonnegative y is an unsigned one. */
static inline int32_t msfa_exp2_lookup(int32_t x) {
  uint32_t ux = (uint32_t)x;
  uint32_t lowbits = ux & 0x3fffu;
  uint32_t i = (ux >> 14) & 1023u;
  uint32_t y0 = msfa_exp2[i];
  uint32_t dy = (i == 1023u ? 0x80000000u : msfa_exp2[i + 1u]) - y0;
  uint32_t y = y0 + (uint32_t)(((uint64_t)dy * lowbits) >> 14);
  int32_t xi = (int32_t)((ux + 0x80000000u) >> 24) - 128; /* floor(x/2^24) */
  unsigned sh = (unsigned)(6 - xi);
  return sh < 32u ? (int32_t)(y >> sh) : 0;
}

/* Gain step of a block of 2^lg samples: floor((gain2-gain1+N/2)/N), N=2^lg.
 * |gain2-gain1| < 2^29 (gains are below 2^26). */
static inline int32_t msfa_dgain(int32_t gain1, int32_t gain2, unsigned lg) {
  int32_t d = gain2 - gain1 + (lg ? (int32_t)1 << (lg - 1u) : 0);
  uint32_t t = (uint32_t)d + 0x40000000u;
  return (int32_t)(t >> lg) - (int32_t)(0x40000000u >> lg);
}

/* One operator sample: floor(y*gain/2^24) for |y| <= 2^24, |gain| < 2^30. */
static inline int32_t msfa_mulq24(int32_t y, int32_t gain) {
  uint64_t u = (uint64_t)((int64_t)y * gain) + ((uint64_t)1 << 62);
  return (int32_t)((int64_t)(u >> 24) - ((int64_t)1 << 38));
}

/* compute_fb's scaled feedback floor((y0+y)/2^(shift+1)) generalised to a
 * Q32 multiplier: floor((y0+y)*fbm/2^33); fbm = 2^(32-shift) is upstream.
 * |y0+y| < 2^27, fbm <= 2^33. */
static inline int32_t msfa_fb_in(int32_t y0, int32_t y, uint64_t fbm) {
  int64_t p = (int64_t)(y0 + y) * (int64_t)fbm;
  uint64_t u = (uint64_t)p + ((uint64_t)1 << 62);
  return (int32_t)((int64_t)(u >> 33) - ((int64_t)1 << 29));
}

/* Env::getsample's stepping law for one envelope tick (level in Q24 log2,
 * 2^24 per doubling). Rising: jump to 1716/64 doublings, then
 * level += (17 - floor(level/2^24)) * inc; falling: level -= inc. Returns 1
 * when the target is reached (level is then exactly the target), as
 * upstream's advance(ix_+1). level <= 17*2^24 always (targets <= 14*2^24);
 * rising needs 11*inc + level < 2^31 (inc < 2^27); falling needs only
 * level - inc > INT32_MIN (inc < 2^30). */
#define MSFA_ENV_JUMP ((int32_t)1716 << 16)
static inline int msfa_env_step(int32_t *level, int32_t target, int32_t inc,
                                int rising) {
  int32_t l = *level;
  if (rising) {
    if (l < MSFA_ENV_JUMP) {
      l = MSFA_ENV_JUMP;
    }
    l += (int32_t)(((uint32_t)(((int32_t)17 << 24) - l)) >> 24) * inc;
    if (l >= target) {
      *level = target;
      return 1;
    }
  } else {
    l -= inc;
    if (l <= target) {
      *level = target;
      return 1;
    }
  }
  *level = l;
  return 0;
}

#endif /* GNOMAD_MSFA_KERNEL_H */
