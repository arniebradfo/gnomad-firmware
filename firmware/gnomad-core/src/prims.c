/* Shared primitives: sine lookup, cents-to-ratio X(), ms conversion,
 * CRC32C, linear ADSR stepper. SPEC/02-fixed-point.md. */
#include "gc_internal.h"
#include "tables.h"

/* Quarter-sine lookup (SPEC/02 "Sine lookup"); body in gc_internal.h. */
int32_t gc_sin32(uint32_t p) { return gc_sin32i(p); }

/* X(K,c) (SPEC/02): clamp cents to +-4800, split into octave q and remainder,
 * interpolate the exp2 table in 1/1200 steps, then one final rounding of
 * K*e by 2^(30-q). K < 2^32 and e <= 2^31 keep the product below 2^63. */
/* The K-independent part of X: e=L(E2[idx],E2[idx+1],u,1200) and the shift
 * s=30-q of the clamped cents (callers with one cents value and several K
 * evaluate it once via gc_X_e). */
static inline uint32_t x_e(int32_t c, unsigned *shift) {
  uint32_t cc, q4, r, rr, idx, u, e0, e;
  if (c < -4800) {
    c = -4800;
  } else if (c > 4800) {
    c = 4800;
  }
  /* q=floor(c/1200) via cc=c+4800 in 0..9600; idx=floor(r*256/1200).
   * floor(x/1200)=(x*3579140)>>32 exactly for x < 2^32/704 (both cases). */
  cc = (uint32_t)(c + 4800);
  q4 = (uint32_t)(((uint64_t)cc * 3579140u) >> 32); /* q + 4 */
  r = cc - 1200u * q4;
  rr = r * 256u;
  idx = (uint32_t)(((uint64_t)rr * 3579140u) >> 32);
  u = rr - idx * 1200u;
  e0 = gc_exp2[idx];
  {
    /* floor(y/1200), y=(e1-e0)*u+600 < 2^33: 2^32 = 1200*3579139+496 folds
     * the high bit (y = hi*2^32+lo), then t=lo2*3579139>>32 is the quotient
     * or one less (gc_fdivk, inline and without the general loop). */
    uint64_t y = (uint64_t)(gc_exp2[idx + 1u] - e0) * u + 600u;
    uint32_t hi = (uint32_t)(y >> 32), q = hi * 3579139u;
    uint64_t y2 = (uint64_t)hi * 496u + (uint32_t)y;
    uint32_t lo, t;
    if (y2 >> 32) { /* at most once more: y2 < 2^32 + 496 */
      q += 3579139u;
      y2 = 496u + (uint32_t)y2;
    }
    lo = (uint32_t)y2;
    t = (uint32_t)(((uint64_t)lo * 3579139u) >> 32);
    lo -= t * 1200u;
    e = e0 + q + t + (lo >= 1200u ? 1u : 0u);
  }
  *shift = 34u - q4; /* 30 - q */
  return e;
}

uint32_t gc_X_e(int32_t c, unsigned *shift) { return x_e(c, shift); }

uint64_t gc_X(uint32_t k, int32_t c) {
  unsigned s;
  uint32_t e = x_e(c, &s);
  /* one final rounding of K*e by 2^(30-q): R of a nonnegative product */
  return ((uint64_t)k * (uint64_t)e + ((uint64_t)1 << (s - 1u))) >> s;
}

/* Restoring division, quotient below 2^bits (1 <= bits <= 32, d << (bits-1)
 * fits 64 bits for every caller): the divisor shifts right one place per
 * step instead of being re-shifted left (same quotient, fewer operations). */
int64_t gc_Rv(int64_t n, uint64_t d, unsigned bits) {
  uint64_t x = gc_mag64(n) + (d >> 1), dd = d << (bits - 1u);
  uint32_t q = 0;
  while (bits-- > 0u) {
    q <<= 1;
    if (x >= dd) {
      x -= dd;
      q |= 1u;
    }
    dd >>= 1;
  }
  return n < 0 ? -(int64_t)q : (int64_t)q;
}

/* floor(n/d) for 2^30 <= d < 2^38, n < 2^53 and quotient < 2^32, without a
 * division: d is normalized to dt in [2^31,2^32) (n shifted alike), 1/dt is
 * estimated in Q30 by Newton-Raphson from the linear seed 48/17-32/17*d'
 * (three steps, relative error far below 2^-28), the quotient estimated from
 * the high product, and then corrected exactly against the remainder
 * n-q*d. The result is exact for any estimate; the estimate only bounds the
 * correction loop (a few steps). Exhaustive-boundary test prim_udiv_nr. */
uint32_t gc_udiv_nr(uint64_t n, uint64_t d) {
  uint64_t nn = n, dd = d;
  uint32_t dt, x, q;
  int64_t rem;
  int it;
  if (dd < 2147483648u) { /* 2^30 <= d < 2^31: scale both up by 2 */
    dd <<= 1;
    nn <<= 1;
  }
  while (dd >> 32) { /* at most 6 halvings; truncation only perturbs q */
    dd >>= 1;
    nn >>= 1;
  }
  dt = (uint32_t)dd;
  x = 3031741621u - (uint32_t)(((uint64_t)dt * 2021161081u) >> 32);
  for (it = 0; it < 3; it++) {
    uint32_t t = (uint32_t)(((uint64_t)dt * x) >> 32); /* d'*x, Q30 */
    x = (uint32_t)(((uint64_t)x * (2147483648u - t)) >> 30);
  }
  {
    uint64_t hi = (nn >> 32) * x + (((nn & 0xffffffffu) * x) >> 32);
    q = (uint32_t)(hi >> 30);
  }
  rem = (int64_t)n - (int64_t)((uint64_t)q * d);
  while (rem < 0) {
    q--;
    rem += (int64_t)d;
  }
  while (rem >= (int64_t)d) {
    q++;
    rem -= (int64_t)d;
  }
  return q;
}

void gc_recip_init(gc_recip *k, uint32_t d) {
  uint64_t r = 0, m = d > 1u ? gc_udivmod64(4294967296ULL, d, &r) : 0u;
  k->d = d;
  k->m = (uint32_t)m;
  k->r = (uint32_t)r;
}

/* m=floor(2^32/d), r=2^32 mod d for 2^16 < d < 2^19 (m < 2^16): 16-step
 * 32-bit restoring division of 2^32-1, then the +1 (CORE-PERF; exhaustive
 * test prim_recip19). */
void gc_recip19(uint32_t d, uint32_t *m, uint32_t *r) {
  uint32_t rem = 0xffffffffu, q = 0;
  int b;
  for (b = 15; b >= 0; b--) {
    if (d <= (0xffffffffu >> b) && rem >= (d << b)) {
      rem -= d << b;
      q |= 1u << b;
    }
  }
  if (rem + 1u == d) {
    q++;
    rem = 0;
  } else {
    rem++;
  }
  *m = q;
  *r = rem;
}

uint64_t gc_fdiv_recip(uint64_t y, const gc_recip *k) {
  return k->d > 1u ? gc_fdivk(y, k->d, k->m, k->r) : y;
}

/* N = max(1, R(ms*44100, 1000)). */
uint32_t gc_ms_frames(uint32_t ms) {
  uint32_t n = (ms * 44100u + 500u) / 1000u;
  return n ? n : 1u;
}

/* floor(n/d) and n mod d for any d >= 1 without a 64-bit divide helper
 * (__udivdi3 / __aeabi_uldivmod are absent on the firmware toolchain):
 * align the divisor under the dividend, then restoring shift-and-subtract.
 * Exact for all 64-bit inputs; used only at event/init rate. */
uint64_t gc_udivmod64(uint64_t n, uint64_t d, uint64_t *rem) {
  uint64_t q = 0, bit = 1;
  if (d > n) {
    *rem = n;
    return 0;
  }
  while (!(d & 0x8000000000000000ULL) && (d << 1) <= n) {
    d <<= 1;
    bit <<= 1;
  }
  while (bit) {
    if (n >= d) {
      n -= d;
      q |= bit;
    }
    d >>= 1;
    bit >>= 1;
  }
  *rem = n;
  return q;
}

int64_t gc_R(int64_t n, uint64_t d) {
  uint64_t m = gc_mag64(n), r;
  uint64_t q = gc_udivmod64(m, d, &r);
  q += (r >= d - (d >> 1)) ? 1u : 0u; /* remainder vs ceil(d/2) */
  if (n < 0 && q == (UINT64_C(1) << 63)) return INT64_MIN;
  return n < 0 ? -(int64_t)q : (int64_t)q;
}

/* R(hz*2^32,176400) = 24347*hz + R(156496*hz,176400) (2^32 = 176400*24347
 * + 156496); for hz <= 11000 (every caller clamps) y=156496*hz+88200 < 2^31
 * and floor(y/176400) is the 32-bit reciprocal estimate plus one compare. */
int32_t gc_svf_f(uint32_t hz) {
  uint32_t ph;
  int32_t s;
  if (hz <= 11000u) {
    uint32_t y = 156496u * hz + 88200u;
    uint32_t t = (uint32_t)(((uint64_t)y * 24347u) >> 32);
    t += (y - t * 176400u >= 176400u) ? 1u : 0u;
    ph = 24347u * hz + t;
  } else {
    ph = (uint32_t)GC_RK((int64_t)hz * 4294967296LL, 176400u);
  }
  s = gc_sin32i(ph);
  return (int32_t)(2 * gc_qsin30(s));
}

/* R(res*3*2^30, 131070) = floor((t+32767)/65535) with t = res*3*2^29 (the
 * denominator 2*65535, 65535 odd), i.e. the G form |G(3*2^29, res)|
 * (exhaustive test prim_svf_q). */
int32_t gc_svf_q(uint32_t res) {
  int64_t q = 2147483648LL - (int64_t)gc_Gm(1610612736u, res);
  return (int32_t)(q > 2147483647LL ? 2147483647LL : q);
}

int32_t gc_svf_run(int32_t *st, int32_t x, int32_t f, int32_t q, uint32_t *sat) {
  return gc_svf_runi(st, x, f, q, sat);
}

/* CRC32C, reflected polynomial 0x82F63B78, init/final 0xffffffff, bitwise
 * (no table). With skip_crc_field the record bytes 12..15 count as zero. */
uint32_t gc_crc32c(const uint8_t *p, uint32_t n, int skip_crc_field) {
  uint32_t crc = 0xffffffffu;
  uint32_t i;
  int b;
  for (i = 0; i < n; i++) {
    uint32_t byte = (skip_crc_field && i >= 12u && i < 16u) ? 0u : p[i];
    crc ^= byte;
    for (b = 0; b < 8; b++) {
      crc = (crc >> 1) ^ (0x82F63B78u & (0u - (crc & 1u)));
    }
  }
  return crc ^ 0xffffffffu;
}

void gc_zero(void *p, uint32_t n) {
  uint8_t *d = (uint8_t *)p;
  while (n--) {
    *d++ = 0;
  }
}

void gc_copy(void *dst, const void *src, uint32_t n) {
  uint8_t *d = (uint8_t *)dst;
  const uint8_t *s = (const uint8_t *)src;
  while (n--) {
    *d++ = *s++;
  }
}

/* Envelope segment from the current level to `to` over `frames` samples:
 * sample k outputs L(A,B,k,N) through h/r/q accumulation (SPEC/02 B18). */
void gc_env_segment(gc_env *e, uint16_t to, uint32_t frames, uint8_t stage) {
  uint16_t from = gc_env_level(e);
  uint32_t a = to >= from ? (uint32_t)(to - from) : (uint32_t)(from - to);
  e->neg = (uint8_t)(to < from);
  e->cur = from;
  e->n = frames;
  e->q = (uint16_t)(a / frames);
  e->r = a - (uint32_t)e->q * frames;
  e->h = frames / 2u;
  e->k = 0;
  e->stage = stage;
}

/* Enter the stage that follows a completed attack (decay, latching P2/P3 on
 * its own first sample) or decay (sustain). Inline gc_env_step calls this
 * only on the sample after a stage ends. */
void gc_env_next(gc_env *e, const uint16_t *p) {
  if (e->stage == GC_ENV_ATTACK) {
    gc_env_segment(e, p[3], gc_ms_frames(p[2]), GC_ENV_DECAY);
  } else if (e->stage == GC_ENV_DECAY) {
    e->stage = GC_ENV_SUSTAIN;
  }
}
