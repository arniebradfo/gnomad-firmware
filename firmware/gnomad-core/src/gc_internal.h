#ifndef GC_INTERNAL_H
#define GC_INTERNAL_H
/* GC-FX1 internal contracts. Clean-room implementation of SPEC/01, SPEC/02,
 * SPEC/10..14 (all five engines) and SPEC/20 (mixer, master and effects). No libc beyond the
 * freestanding headers: the core builds with -ffreestanding for 32-bit
 * targets, and never allocates, locks, prints or touches files. */

#include "gnomad_core.h"

#define GC_CORE_MAGIC 0x47434658u /* 'GCFX' */
#define GC_DEFAULT_SEED 0x474E4F4Du
#define GC_INC_CAP 1932735283u /* floor(9*2^32/20), SPEC/02 */
#define GC_PATCH_P(i) (32u + 2u * (unsigned)(i))

/* Keep a function out of line (tools/divcheck.py names stage-entry
 * functions; inlining would hide them inside the render loop). */
#if defined(__GNUC__) || defined(__clang__)
#define GC_NOINLINE __attribute__((noinline))
#else
#define GC_NOINLINE
#endif

/* C99 compile-time assertion. */
#define GC_SA_CAT2(a, b) a##b
#define GC_SA_CAT(a, b) GC_SA_CAT2(a, b)
#define GC_STATIC_ASSERT(cond) \
  typedef char GC_SA_CAT(gc_static_assert_, __LINE__)[(cond) ? 1 : -1]

/* Engines implemented in this build. gc_query reports exactly this set as
 * engine_mask (SPEC/01:106 requires 31, all five engines), and the record
 * validator rejects any other engine byte with GC_E_FORMAT at offset 8. */
#define GC_ENGINES_AVAILABLE \
  ((1u << GC_VA) | (1u << GC_FM) | (1u << GC_DRUMS) | (1u << GC_GNOME) | \
   (1u << GC_SAMPLE))

/* ------------------------------------------------------------------------
 * Arithmetic primitives (SPEC/02 "Arithmetic primitives") */

static inline uint64_t gc_mag64(int64_t n) {
  return n < 0 ? (uint64_t)0 - (uint64_t)n : (uint64_t)n;
}

/* R(n,d): nearest, ties away from zero, d > 0 (prims.c). Quotient plus
 * remainder test against ceil(d/2); no negation of INT64_MIN. */
int64_t gc_R(int64_t n, uint64_t d);
/* Exact 64-bit quotient/remainder without a runtime division helper. */
uint64_t gc_udivmod64(uint64_t n, uint64_t d, uint64_t *rem);

/* floor(v/2^s) for |v| < 2^62, 1 <= s <= 62, with unsigned shifts only:
 * bias v by 2^62 to a nonnegative value, shift, remove the shifted bias. */
static inline int64_t gc_floor2(int64_t v, unsigned s) {
  uint64_t u = (uint64_t)v + ((uint64_t)1 << 62);
  return (int64_t)(u >> s) - ((int64_t)1 << (62u - s));
}

/* R(n, 2^s) for 1 <= s <= 61 and |n| < 2^61 (every caller is far below):
 * n >= 0: floor((n+h)/2^s); n < 0: -floor((|n|+h)/2^s) = floor((n+h-1)/2^s),
 * h = 2^(s-1). Exact (CORE-PERF: replaces the magnitude/negate form). */
static inline int64_t gc_Rp2(int64_t n, unsigned s) {
  return gc_floor2(n + ((int64_t)1 << (s - 1u)) - (n < 0 ? 1 : 0), s);
}

/* The same for 32-bit operands, |n| < 2^30: bias 2^31 in uint32. */
static inline int32_t gc_Rp2s(int32_t n, unsigned s) {
  uint32_t u = (uint32_t)(n + (1 << (s - 1u)) - (n < 0 ? 1 : 0)) + 0x80000000u;
  return (int32_t)(u >> s) - (int32_t)(0x80000000u >> s);
}

static inline int32_t gc_S15(int64_t x) {
  return x > 32767 ? 32767 : (x < -32768 ? -32768 : (int32_t)x);
}

/* S18 with internal-saturation telemetry (SPEC/20 `saturations`). */
static inline int32_t gc_S18(int64_t x, uint32_t *sat) {
  if (x > 262143) {
    (*sat)++;
    return 262143;
  }
  if (x < -262144) {
    (*sat)++;
    return -262144;
  }
  return (int32_t)x;
}

/* G(x,g)=R(x*g,65535) by the exact SPEC/02 B18 decomposition, g <= 65535,
 * |x| <= 2^31. R(m,65535)=floor((m+32767)/65535) (ceil(65535/2)=32768);
 * y=m+32767=a*65536+b=a*65535+t with a=y>>16, t=a+b, and the bounded
 * identity floor(t/65535)=(t+1+(t>>16))>>16, exact for t < 65535*65537.
 * a < 2^31 and t < 2^31+2^16, so after the one widening product everything
 * is 32-bit (CORE-PERF; identical results to the 64-bit form it replaces).
 * tests/test_prims.c proves the identity at every quotient boundary and
 * checks G against R over all gains at the audio and control bounds. */
static inline int32_t gc_G(int32_t x, uint32_t g) {
  uint32_t ax = x < 0 ? (uint32_t)0 - (uint32_t)x : (uint32_t)x;
  uint64_t m = (uint64_t)ax * g + 32767u;
  uint32_t a = (uint32_t)(m >> 16);
  uint32_t t = a + ((uint32_t)m & 0xffffu);
  uint32_t q = a + ((t + 1u + (t >> 16)) >> 16);
  if (x < 0 && q >= 2147483648u) return INT32_MIN; /* never wraps positive */
  return x < 0 ? -(int32_t)q : (int32_t)q;
}

/* Magnitude form: G(x,g) = sign(x)*gc_Gu(|x|,g) for |x| <= 65535, so a
 * chain of gains can run on the magnitude and take the sign once. */
static inline uint32_t gc_Gu(uint32_t ax, uint32_t g) {
  uint32_t y = ax * g + 32767u;
  return (y + 1u + (y >> 16)) >> 16;
}

/* General magnitude form: |G(x,g)| = gc_Gm(|x|,g) for |x| <= 2^31 (the
 * arithmetic of gc_G without the sign), for gain chains on S18 values. */
static inline uint32_t gc_Gm(uint32_t ax, uint32_t g) {
  uint64_t m = (uint64_t)ax * g + 32767u;
  uint32_t a = (uint32_t)(m >> 16);
  uint32_t t = a + ((uint32_t)m & 0xffffu);
  return a + ((t + 1u + (t >> 16)) >> 16);
}

/* G for |x| <= 65535 (S15 audio, gains, indices): y=|x|*g+32767 <= 65535^2+
 * 32767 < 65535*65537 fits uint32, so G = (y+1+(y>>16))>>16 with no 64-bit
 * arithmetic at all. Callers state why their operand is in range. */
static inline int32_t gc_Gs(int32_t x, uint32_t g) {
  uint32_t ax = x < 0 ? (uint32_t)0 - (uint32_t)x : (uint32_t)x;
  uint32_t y = ax * g + 32767u;
  uint32_t q = (y + 1u + (y >> 16)) >> 16;
  return x < 0 ? -(int32_t)q : (int32_t)q;
}

/* Exact constant-divisor helpers for render paths (SPEC/02 B18: a division
 * in the reference is not a division instruction per sample).
 * floor(x/65535), x < 2^47: x=a*65535+t with a=x>>16, t=a+(x mod 2^16) <
 * 65535*65537, where floor(t/65535)=(t+1+(t>>16))>>16 (as in gc_G). */
static inline uint64_t gc_fdiv65535(uint64_t x) {
  uint64_t a = x >> 16, t = a + (x & 0xffffu);
  return a + ((t + 1u + (t >> 16)) >> 16);
}
/* floor(x/32767), x < 2^40: two folds x=(a+b)*32767+t, then t < 2*32767. */
static inline uint64_t gc_fdiv32767(uint64_t x) {
  uint64_t a = x >> 15, t = a + (x & 0x7fffu), b = t >> 15;
  t = b + (t & 0x7fffu);
  return a + b + (t >= 32767u ? 1u : 0u);
}
/* R(n, d) with d = 65535*32767 = 2^31 - 98303, for |n| < 2^44 (vibrato/
 * motion laws K*macro*sin with K <= 128 stay below 2^38): y=|n|+floor(d/2),
 * q0=floor(y/2^31) <= q, remainder y-q0*d = (y mod 2^31)+q0*98303 < 2d, so
 * one compare finishes it, all 32-bit after the shift (CORE-PERF; exact). */
static inline int32_t gc_Rw(int64_t n) {
  uint64_t y = gc_mag64(n) + 1073692672u;
  uint32_t q0 = (uint32_t)(y >> 31);
  uint32_t r = ((uint32_t)y & 0x7fffffffu) + q0 * 98303u;
  uint32_t q = q0 + (r >= 2147385345u ? 1u : 0u);
  return n < 0 ? -(int32_t)q : (int32_t)q;
}

/* R(k*w*s, 65535*32767) for the LFO laws (k <= 1200: the law literals and
 * the VA Motion depth P23, CHANGES-1.1.6 C4; macro w <= 65535, |s| <= 32767
 * a sine): u=w*|s| <= d fits uint32 and k*u < 2^42, so q0 <= 1201 and the
 * remainder (y mod 2^31)+q0*98303 < 2^31+2^27 < 2d: one compare finishes it,
 * on the magnitude with the sign of s. Exact (test prim_rwk at k=1200). */
static inline int32_t gc_Rwk(uint32_t k, uint32_t w, int32_t s) {
  uint32_t u = w * (s < 0 ? (uint32_t)0 - (uint32_t)s : (uint32_t)s);
  uint64_t y = (uint64_t)k * u + 1073692672u;
  uint32_t q0 = (uint32_t)(y >> 31);
  uint32_t r = ((uint32_t)y & 0x7fffffffu) + q0 * 98303u;
  uint32_t q = q0 + (r >= 2147385345u ? 1u : 0u);
  return s < 0 ? -(int32_t)q : (int32_t)q;
}

/* R(3*w*s, 20*32767) (VA Width wobble), w <= 65535, |s| <= 32767: with
 * y=3*w*|s|+327670 < 2^33, floor(y/655340) = floor(floor(floor(y/4)/5)/
 * 32767) (nested floors), /5 by the exact 32-bit reciprocal 0xCCCCCCCD>>34,
 * /32767 by one fold t=a*32768+b (a+b < 2^16). Exhaustive test prim_va_forms. */
static inline int32_t gc_Rw3(uint32_t w, int32_t s) {
  uint64_t y = 3u * (uint64_t)(w * (uint32_t)(s < 0 ? -s : s)) + 327670u;
  uint32_t z = (uint32_t)(y >> 2);
  uint32_t t = (uint32_t)(((uint64_t)z * 0xCCCCCCCDu) >> 34);
  uint32_t a = t >> 15, q = a + ((a + (t & 0x7fffu)) >= 32767u ? 1u : 0u);
  return s < 0 ? -(int32_t)q : (int32_t)q;
}

/* R(width*2^32, 65535) for 0 <= width < 65535: 2^32 = 65535*65537+1, so
 * width*65537 + R(width,65535) = width*65537 + (width >= 32768). */
static inline uint32_t gc_va_off(uint32_t width) {
  return width * 65537u + (width >= 32768u ? 1u : 0u);
}

/* 32-bit clamp for int32 expressions known not to overflow. */
static inline int32_t gc_clampi(int32_t x, int32_t lo, int32_t hi) {
  return x < lo ? lo : (x > hi ? hi : x);
}

/* Exact division-free quotients for every render path (SPEC/02 B18).
 * gc_fdivk: floor(y/d) for a divisor d >= 2 given m=floor(2^32/d) and
 * r=2^32 mod d: the high word folds as a*2^32 = a*m*d + a*r, then one
 * multiply estimate and at most one correction. GC_RK(n,d) = R(n,d) for a
 * literal d (m and r are compile-time constants). gc_Rv(n,d,bits) = R(n,d)
 * for a variable d when the quotient is below 2^bits (restoring division,
 * shift-and-subtract only). prims.c; tests/test_prims.c. */
static inline uint64_t gc_fdivk(uint64_t y, uint32_t d, uint32_t m, uint32_t r) {
  uint64_t q = 0;
  uint32_t lo, t;
  while (y >> 32) {
    uint64_t a = y >> 32;
    q += a * m;
    y = a * r + (y & 0xffffffffu);
  }
  lo = (uint32_t)y;
  t = (uint32_t)(((uint64_t)lo * m) >> 32); /* floor(lo/d) or one less */
  lo -= t * d;
  return q + t + (lo >= d ? 1u : 0u);
}
static inline int64_t gc_Rk(int64_t n, uint32_t d, uint32_t m, uint32_t r) {
  uint64_t q = gc_fdivk(gc_mag64(n) + (d >> 1), d, m, r);
  return n < 0 ? -(int64_t)q : (int64_t)q;
}
int64_t gc_Rv(int64_t n, uint64_t d, unsigned bits);
#define GC_RK(n, d) \
  gc_Rk((n), (d), (uint32_t)(4294967296ULL / (d)), (uint32_t)(4294967296ULL % (d)))
/* R(x, d) for a literal d and |x| < 2^31: m=floor(2^32/d) estimates the
 * quotient of a=|x|+floor(d/2) < 2^32 from below by at most one. */
static inline int32_t gc_Rk32(int32_t x, uint32_t d, uint32_t m) {
  uint32_t a = (x < 0 ? (uint32_t)0 - (uint32_t)x : (uint32_t)x) + (d >> 1);
  uint32_t q = (uint32_t)(((uint64_t)a * m) >> 32);
  q += (a - q * d >= d) ? 1u : 0u;
  return x < 0 ? -(int32_t)q : (int32_t)q;
}
#define GC_RK32(x, d) gc_Rk32((x), (d), (uint32_t)(4294967296ULL / (d)))

/* Divisor known only at note-on (drum lifetimes): precomputed m and r. */
typedef struct {
  uint32_t d, m, r;
} gc_recip;
void gc_recip_init(gc_recip *k, uint32_t d); /* note-on time; d >= 1 */
uint64_t gc_fdiv_recip(uint64_t y, const gc_recip *k);
void gc_recip19(uint32_t d, uint32_t *m, uint32_t *r); /* 2^16 < d < 2^19 */
uint32_t gc_udiv_nr(uint64_t n, uint64_t d); /* 2^30<=d<2^38, n<2^53, q<2^32 */
/* Qsin(s)=R(s*2^30,32767) = sign(s)*(32769*|s|+(|s|>=16384)), since
 * 2^30=32767*32769+1 (SPEC/02 Qsin, exact). */
static inline int64_t gc_qsin30(int32_t s) {
  uint32_t a = (uint32_t)(s < 0 ? -s : s);
  int64_t q = 32769LL * a + (a >= 16384u ? 1 : 0);
  return s < 0 ? -q : q;
}

/* M(a,b)=R(a*b,2^30): Q*.15 sample times Q2.30 coefficient. */
static inline int32_t gc_M(int32_t a, int32_t b) {
  return (int32_t)gc_Rp2((int64_t)a * (int64_t)b, 30u);
}

/* S18 of an int32 expression (no 64-bit compare), with telemetry. */
static inline int32_t gc_S18i(int32_t x, uint32_t *sat) {
  if (x > 262143) {
    (*sat)++;
    return 262143;
  }
  if (x < -262144) {
    (*sat)++;
    return -262144;
  }
  return x;
}

/* Inline twice-iterated Chamberlin step (SPEC/02 "Shared SVF"), identical
 * to gc_svf_run: with |x|, low, band, high <= 2^18 (S18) and |F|, |Q| < 2^31
 * every M is below 2^19 in magnitude and every sum below 2^21, so the sums
 * and S18 clamps are exact in int32. */
static inline int32_t gc_svf_runi(int32_t *st, int32_t x, int32_t f, int32_t q,
                                  uint32_t *sat) {
  int32_t low = st[0], band = st[1], high;
  low = gc_S18i(low + gc_M(band, f), sat);
  high = gc_S18i(x - low - gc_M(band, q), sat);
  band = gc_S18i(band + gc_M(high, f), sat);
  low = gc_S18i(low + gc_M(band, f), sat);
  high = gc_S18i(x - low - gc_M(band, q), sat);
  band = gc_S18i(band + gc_M(high, f), sat);
  st[0] = low;
  st[1] = band;
  return low;
}

/* L(a,b,u,D)=a+R((b-a)*u,D). */
static inline int32_t gc_L(int32_t a, int32_t b, int64_t u, uint64_t d) {
  return a + (int32_t)gc_R((int64_t)(b - a) * u, d);
}

static inline int32_t gc_clamp32(int64_t x, int32_t lo, int32_t hi) {
  return x < lo ? lo : (x > hi ? hi : (int32_t)x);
}

static inline uint32_t gc_cap_inc(uint64_t x) {
  return x < 1u ? 1u : (x > GC_INC_CAP ? GC_INC_CAP : (uint32_t)x);
}

/* Signed decode of a 16-bit word by subtraction (SPEC/01 wire layout). */
static inline int32_t gc_s16(uint16_t w) {
  return w >= 32768u ? (int32_t)w - 65536 : (int32_t)w;
}

/* xorshift32 noise, SPEC/02 D18. Returns int32(x>>16)-32768. */
static inline int32_t gc_noise(uint32_t *x) {
  uint32_t v = *x;
  v ^= v << 13;
  v ^= v >> 17;
  v ^= v << 5;
  *x = v;
  return (int32_t)(v >> 16) - 32768;
}

static inline uint16_t gc_rd16(const uint8_t *p) {
  return (uint16_t)((unsigned)p[0] | ((unsigned)p[1] << 8));
}
static inline void gc_wr16(uint8_t *p, uint16_t v) {
  p[0] = (uint8_t)(v & 0xffu);
  p[1] = (uint8_t)(v >> 8);
}
static inline uint32_t gc_rd32(const uint8_t *p) {
  return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
         ((uint32_t)p[3] << 24);
}

/* Quarter-sine lookup (SPEC/02 "Sine lookup"): mirror by quadrant, 22-bit
 * linear interpolation inside one table interval, magnitude before sign.
 * The table is nondecreasing, so the interpolation delta is nonnegative.
 * Inline form for the hottest loops; gc_sin32 (prims.c) is the same code. */
extern const int16_t gc_qsin[257];
/* |sin32(p)| (the magnitude before the quadrant 2/3 sign). */
static inline uint32_t gc_sinmag(uint32_t p) {
  uint32_t local = p & 0x3fffffffu;
  if (p & 0x40000000u) {
    local = 0x40000000u - local;
  }
  if (local == 0x40000000u) {
    return 32767u;
  } else {
    uint32_t i = local >> 22;
    uint32_t u = local & 0x3fffffu;
    uint32_t a = (uint32_t)gc_qsin[i];
    uint32_t d = (uint32_t)gc_qsin[i + 1u] - a;
    return a + ((d * u + 0x200000u) >> 22);
  }
}
static inline int32_t gc_sin32i(uint32_t p) {
  int32_t mag = (int32_t)gc_sinmag(p);
  return (p & 0x80000000u) ? -mag : mag;
}

/* prims.c */
int32_t gc_sin32(uint32_t phase);
uint64_t gc_X(uint32_t k, int32_t cents);
uint32_t gc_X_e(int32_t cents, unsigned *shift); /* X(K,c)=R(K*e,2^shift) */
uint32_t gc_ms_frames(uint32_t ms);
uint32_t gc_crc32c(const uint8_t *p, uint32_t n, int skip_crc_field);
void gc_zero(void *p, uint32_t n);
void gc_copy(void *dst, const void *src, uint32_t n);

/* Shared SVF (SPEC/02 "Shared SVF"): F=2*Qsin(sin32(R(c*2^32,176400))),
 * damping raw = min(2^31-1, 2^31 - R(res*3*2^30, 2*65535)). */
int32_t gc_svf_f(uint32_t hz);
int32_t gc_svf_q(uint32_t res);
/* Twice-iterated Chamberlin step; st[0]=low, st[1]=band; returns low. */
int32_t gc_svf_run(int32_t *st, int32_t x, int32_t f, int32_t q, uint32_t *sat);

/* Linear ADSR with the exact quotient/remainder stepper (SPEC/02 B18).
 * cur is the current level A +/- magnitude, kept directly (each step moves
 * it by q+carry toward the endpoint); stage parameters latch at stage
 * start. 24 bytes with padding. */
enum { GC_ENV_IDLE = 0, GC_ENV_ATTACK, GC_ENV_DECAY, GC_ENV_SUSTAIN,
       GC_ENV_RELEASE };
typedef struct {
  uint32_t r, h, n, k;
  uint16_t cur, q;
  uint8_t stage, neg;
} gc_env;
void gc_env_segment(gc_env *e, uint16_t to, uint32_t frames, uint8_t stage);
static inline uint16_t gc_env_level(const gc_env *e) { return e->cur; }
/* One sample of a moving segment (attack, decay or release): k++, the B18
 * remainder carry, level moves by q + carry toward the endpoint. */
static inline void gc_env_adv(gc_env *e) {
  uint32_t h = e->h + e->r, step = e->q;
  e->k++;
  if (h >= e->n) {
    h -= e->n;
    step++;
  }
  e->h = h;
  e->cur = (uint16_t)(e->neg ? e->cur - step : e->cur + step);
}
static inline int gc_env_moving(uint8_t stage) {
  return stage == GC_ENV_ATTACK || stage == GC_ENV_DECAY ||
         stage == GC_ENV_RELEASE;
}
/* Stage entry after a completed attack/decay (out of line, event rate). */
void gc_env_next(gc_env *e, const uint16_t *p);
/* p: snapshot words, P2 decay ms and P3 sustain read at decay entry. */
static inline uint16_t gc_env_step(gc_env *e, const uint16_t *p) {
  uint16_t level;
  if (e->k == e->n &&
      (e->stage == GC_ENV_ATTACK || e->stage == GC_ENV_DECAY)) {
    gc_env_next(e, p); /* the previous sample ended a stage */
  }
  if (gc_env_moving(e->stage)) {
    gc_env_adv(e);
  }
  level = gc_env_level(e);
  if (e->stage == GC_ENV_RELEASE && e->k == e->n) {
    e->stage = GC_ENV_IDLE; /* complete: the slot frees after this sample */
  }
  return level;
}

/* 64-sample performance ramp (SPEC/01 D07, SPEC/02 L ramp). */
typedef struct {
  uint16_t start, target, cur;
  uint8_t k, spare;
} gc_ramp;

/* ------------------------------------------------------------------------
 * Voice slots: eight 512-byte slots (SPEC/00 budget table). */

#define GC_SNAP_WORDS 48 /* P0..P47; 96-B payload (SPEC/10, SPEC/11) */

enum { GC_VF_ACTIVE = 1, GC_VF_ATTACHED = 2, GC_VF_FORCED = 4, GC_VF_XFADE = 8 };

typedef struct {
  uint64_t start;    /* absolute frame of note-on; ties by slot index */
  uint32_t note_id;  /* 0 = anonymous (released/detached ID) */
  uint32_t rng;      /* voice-local xorshift32 */
  uint16_t vel_gain; /* R(velocity*65535,127) */
  int16_t prev_sample;  /* most recent emitted sample (steal source) */
  int16_t xf_old;    /* steal transition source sample */
  uint16_t forced_g0;
  uint8_t engine, track, flags, key, vel, xf_k, forced_k;
  uint8_t kt; /* control-rate phase (GC_KRATE), 0 at note-on */
} gc_voice_head; /* 32 bytes */

/* Control rate (CHANGES-1.1.3 proposal P1). GC_KRATE=1 is the exact SPEC/02
 * profile 1.1.2 path: LFOs and every LFO-/macro-derived control (vibrato,
 * wobble and vowel-motion cents, pitch increments, FM index, VA width and
 * cutoff, Gnome vowel) are evaluated on every sample. GC_KRATE=N (a power of
 * two <= 64) evaluates them on the first sample of a note and then every Nth
 * sample of that voice, holding them in between; LFO phases still advance
 * exactly (N increments per tick), envelopes, audio oscillators, filters and
 * gains stay per sample. */
#ifndef GC_KRATE
#define GC_KRATE 16
#endif
GC_STATIC_ASSERT(GC_KRATE >= 1 && GC_KRATE <= 64 && (GC_KRATE & (GC_KRATE - 1)) == 0);
static inline int gc_ktick(gc_voice_head *h) {
#if GC_KRATE == 1
  (void)h;
  return 1;
#else
  uint8_t k = h->kt;
  h->kt = (uint8_t)((k + 1u) & (GC_KRATE - 1u));
  return k == 0u;
#endif
}

#if GC_KRATE == 1
typedef struct {
  uint32_t ph1, ph2, phs, lfo;
  gc_env env;
  int32_t svf[2];     /* low, band */
  int32_t cF;         /* SVF frequency coefficient for cHz (exact cache) */
  uint32_t blfo;      /* bank (bits 0..7) | LFO increment R(P21*2^32,4410000)
                         < 2^21 (bits 8..31), both fixed per note */
  uint16_t cHz;       /* integer cutoff of cF */
  uint16_t kcut;      /* P18 of the cutoff key; 0 = empty (P18 >= 20) */
  int16_t kfc;        /* envelope/Bright cents of the cutoff key */
  uint16_t spare;
} gc_va_state;
#define GC_VA_BANK(s) ((uint8_t)((s)->blfo & 0xffu))
#else
/* Control-rate VA: increments, pulse width and the cutoff coefficient are
 * held from the last control tick (the LFO increment is recomputed there). */
typedef struct {
  uint32_t ph1, ph2, phs, lfo;
  gc_env env;
  int32_t svf[2];     /* low, band */
  int32_t cF;         /* SVF frequency coefficient from the last tick */
  uint32_t inc1, inc2; /* oscillator increments from the last tick */
  uint16_t width;     /* pulse width raw from the last tick */
  uint8_t bank, spare;
} gc_va_state;
#define GC_VA_BANK(s) ((s)->bank)
#endif

/* Drums (SPEC/12 revision 2.0). The voice models are ported from MIT-licensed
 * open-source eurorack drum code (see src/drum_blocks.h, PROVENANCE.md). A
 * two-pole state-variable filter (low, band) with its frequency and damping
 * coefficients, Q15. */
typedef struct {
  int32_t lp, bp;
  int16_t f;
  uint16_t damp; /* 0..65534 (2.0) */
} gc_dr_svf;

typedef struct {
  uint32_t t, T;     /* age and lifetime in frames */
  uint32_t env, envd; /* main exponential envelope, Q32 level and multiplier */
  uint32_t lfo;      /* Wobble LFO phase */
  int32_t tilt;      /* Bright tilt low-pass state */
  int16_t tune, wob; /* pad tune; Wobble pitch offset Q15 (control tick) */
  uint16_t gain;     /* velocity x pad level x identity trim, Q14 */
  uint16_t chr, send[3];
  uint8_t choke, model, rec, spare;
  union {
    struct { /* K: resonant body pinged by two pulses, punch, tone LP */
      int32_t up, dn, lp;
      gc_dr_svf res;
      int16_t f_lo, f_hi, lpk, punch;
    } k;
    struct { /* S: two pinged bodies and band-passed noise */
      int32_t e1u, e1d, e2;
      gc_dr_svf b1, b2, nz;
      uint16_t g1, g2;
      int16_t snap, spare;
    } s;
    struct { /* H: six square oscillators and/or noise, band-pass, VCA, HP */
      uint32_t ph[6], inc[6];
      gc_dr_svf bp, hp;
      int32_t lp;    /* output smoothing low-pass state */
      int16_t mix;
      uint16_t gain; /* tone loudness compensation, Q12 */
      uint8_t rect, spare[3];
    } h;
    struct { /* M: sine with pitch envelopes, feedback, noise/overdrive */
      uint32_t ph, base, kfm, kaux, fm, fmd, aux, auxd;
      int32_t kfb;
      int16_t prev, nz, od, spare;
    } m;
    struct { /* N: band-passed noise, burst train and tail */
      gc_dr_svf bp;
      int32_t burst;
      uint32_t o1, o2;
      uint16_t bdec, spare;
    } n;
  } u;
} gc_drum_state;

/* FM renders msfa-style blocks of GC_FM_BLOCK samples in every profile
 * (CHANGES-1.1.5 F1): envelopes, gains, pitch and index once per block. */
#define GC_FM_BLOCK 16
#define GC_FM_LGN 4u

/* Per-operator FM state <=48 B (SPEC/11; msfa kernel, CHANGES-1.1.5 F1). */
typedef struct {
  uint32_t phase; /* msfa Q24 phase (the low 24 bits are one turn) */
  uint32_t minc;  /* ratio m_j (D09) or fixed-Hz Q32 increment */
  uint32_t inc;   /* Q24 increment for cents key pkey (exact cache) */
  int32_t level;  /* msfa log-domain envelope level, 2^24 per doubling */
  int32_t target; /* stage target level */
  int32_t einc;   /* stage step per control tick (msfa Env inc_) */
  int32_t gain;   /* Q24 gain at the end of the last block (msfa gain_out) */
  uint32_t jm;    /* Q24 phase per unit output: edge index Jj for jkey */
  uint8_t stage;  /* GC_ENV_* */
  uint8_t rising; /* msfa Env rising_ */
  uint8_t mode;   /* 0 ratio, 1 fixed */
  uint8_t rs;     /* rate scale 0..3 */
  uint8_t vs;     /* velocity sensitivity 0..3 */
  uint8_t car;    /* 1 if an audible carrier in this note's G6 route */
  uint8_t t0, t1; /* modulation targets (route out-degree <= 2); 6, 7 = none */
} gc_fm_op;

typedef struct {
  gc_fm_op op[6];
  int32_t fb0, fb1; /* msfa fb_buf[0], fb_buf[1]: the FB operator's last two */
  uint32_t lfo, lfo_inc;
  uint32_t carr_mul; /* R(2^32,ncar) for mean; 0 = identity (ncar==1) */
  int32_t pkey;      /* Tune+vibrato cents of the cached increments */
  uint16_t vtab[4];  /* note-latched per-source velocity factors by code */
  uint16_t vel_v;    /* V=65535-R(P16*(127-vel),127): note-on and P16 edits */
  int16_t key_j;     /* R(P17*(key-60),12): note-on and P17 edits */
  int16_t jkey;      /* Jk of the cached jm; -1 = empty (live edits) */
  uint8_t alg, ncar, fb_op, carr;
  int16_t blk[GC_FM_BLOCK]; /* the block rendered at the last FM tick */
} gc_fm_state;

/* Gnome Voice state (SPEC/13 revision 2, CHANGES-1.1.7): envelope,
 * saw/sub/LFO phases, resonator and shared input histories, exact
 * coefficient cache keyed by the effective vowel (Size changes only by live
 * edit, which empties it: gc_gnome_live). The cache keeps a and the integer
 * bandwidth B of each section; b and d are the generated functions of B
 * (gn_btab, gn_dtab). One note age t serves Scoop, the gesture (syllable
 * glide, Bend) and the consonant onset. */
typedef struct {
  gc_env env;
  uint32_t ph, sub, lfo, lfo_inc;
  uint32_t inc;     /* cached fundamental increment for cents key pkey */
  int32_t y[3][2]; /* per-resonator y1, y2 */
  int32_t ca[3];   /* a of each section for the cached vowel */
  int16_t x1, x2;  /* excitation history (S16) */
  int16_t pkey;    /* Tune+vibrato+scoop+bend cents of inc; INT16_MIN = empty */
  uint16_t ckey_v; /* vowel of ca/bw; 0xffff = empty (also on Size edits) */
  int16_t chatter;
  uint16_t t;      /* note age in frames, saturating at 65535 */
  int16_t slp;     /* Softness one-pole state (S16) */
  int16_t alp;     /* aspiration/onset noise one-pole state (S16) */
  uint8_t bw[3];   /* integer bandwidth B (80..200 Hz) of each section */
  uint8_t bank;
  uint8_t syl;     /* syllable of this note (P20), 0xff = none */
} gc_gnome_state;

/* Sample state (SPEC/14): 48.16 position, borrowed asset bytes, range. */
typedef struct {
  gc_env env;
  uint64_t pos;
  const uint8_t *pcm; /* borrowed immutable PCM16 LE, valid for the instance */
  uint32_t lfo;
  int32_t svf[2], cF;
  uint16_t cHz, start, end;
  uint8_t mode, endk; /* endk: 32-frame End fade step (0 = playing) */
  int16_t last;        /* last emitted output (fade source) */
  uint32_t step;       /* cached source step for clamped cents pkey */
  int16_t pkey;        /* INT16_MIN = empty */
  uint16_t cB;         /* Bright value of cHz/cF; 0xffff = empty */
} gc_sample_state;

typedef struct {
  gc_voice_head h;
  uint16_t p[GC_SNAP_WORDS];
  union {
    gc_va_state va;
    gc_fm_state fm;
    gc_drum_state dr;
    gc_gnome_state gn;
    gc_sample_state sm;
    uint8_t raw[512 - 32 - 2 * GC_SNAP_WORDS];
  } u;
} gc_voice;

GC_STATIC_ASSERT(sizeof(gc_voice_head) == 32);
GC_STATIC_ASSERT(sizeof(gc_fm_op) <= 48);
GC_STATIC_ASSERT(sizeof(gc_voice) == 512);
/* Per-engine state ceilings: VA 192 B (SPEC/10), FM 512 B (SPEC/11),
 * Drums 256 B (SPEC/12). */
GC_STATIC_ASSERT(32 + 2 * GC_SNAP_WORDS + sizeof(gc_va_state) <= 192);
GC_STATIC_ASSERT(32 + 2 * GC_SNAP_WORDS + sizeof(gc_fm_state) <= 512);
GC_STATIC_ASSERT(32 + 2 * GC_SNAP_WORDS + sizeof(gc_drum_state) <= 256);
/* Gnome 192 B (SPEC/13): the ceiling counts the 28 nonreserved words
 * (P0..P27, revision 2); the slot physically keeps the known-zero P28..P47
 * (CHANGES-1.1.2 C5). */
#define GC_GNOME_WORDS 28
GC_STATIC_ASSERT(32 + 2 * GC_GNOME_WORDS + sizeof(gc_gnome_state) <= 192);
/* Sample 256 B (SPEC/14:30), all 48 words being potentially nonreserved. */
GC_STATIC_ASSERT(32 + 2 * GC_SNAP_WORDS + sizeof(gc_sample_state) <= 256);

/* ------------------------------------------------------------------------
 * Instance state, laid out in the host's 7168-byte arena (SPEC/00). */

enum { GC_TR_LEVEL = 0, GC_TR_BRIGHT, GC_TR_WOBBLE, GC_TR_ECHO, GC_TR_SPACE,
       GC_TR_CHORUS, GC_TR_GAIN, GC_TR_RAMPS };
enum { GC_MR_LEVEL = 0, GC_MR_DRIVE, GC_MR_ECHO_FB, GC_MR_CHORUS_DEPTH,
       GC_MR_REVERB_FB, GC_MR_WET, GC_MR_RAMPS };
#define GC_RAMPS_MASTER 16u /* gc_core.ramp_busy: bit 4 */
#define GC_RAMPS_ALL 31u
enum { GC_MS_CUTOFF = 0, GC_MS_RES, GC_MS_ECHO_TIME, GC_MS_CHORUS_RATE,
       GC_MS_STEPS };

#define GC_FX_SCALAR_BYTES 440 /* effect-lane scalars (512 minus master below) */

/* SPEC/CHANGES-1.2.1-fx profile constants of the plate and the ensemble. */
#define GC_PLATE_KAP 20480u   /* diffusion 0.625, Q15 (Clouds') */
#define GC_PLATE_GIN 3277u    /* tank input 0.1 x S15: Clouds' 0.2*(l+r) at 1/8192 */
/* Plate output gain, Q16: 262144 = 4 = Clouds' mono wet level (tank units
 * are 1/8192 of Clouds' float units; S15 is 1/32768). */
#define GC_PLATE_GOUT 131072
#define GC_ENS_CENTER 800u    /* ensemble tap centre delay, frames (18.1 ms) */
/* WOBBLE's chorus: 1 = the ensemble (CHANGES-1.2.1-fx P2; the product's
 * chorus since CHANGES-1.2.2-ensemble, after a listening test
 * and the content re-voicing), 0 = the single-tap SPEC/20 chorus, kept
 * selectable with its own runner and goldens (tests/golden/single-tap). */
#ifndef GC_FX_ENSEMBLE
#define GC_FX_ENSEMBLE 1
#endif

/* Effect scalars (SPEC/20: indices, filter states, counters and phases
 * live in the ordinary arena; audio lines live in the 64-KiB pool).
 * SPEC/CHANGES-1.2.1-fx: SPACE is the plate (P1), WOBBLE's chorus the
 * ensemble (P2); clears are lazy (X1) and quiet processors skip (X2). */
typedef struct {
  /* -- phases and control-rate values: kept by PANIC (reset zeroes) -- */
  uint32_t clfo, clfo2;  /* ensemble slow (Chorus rate) and fast LFO phases */
  uint32_t pl1, pl2;     /* plate LFO phases (0.5 Hz, 0.3 Hz) */
  uint32_t cinc, cinc2;  /* ensemble LFO increments for rate key ckey */
  uint32_t poff1, poff2; /* plate modulated read offsets, Q16, held */
  int32_t ctap[3];       /* ensemble tap delays, Q16 frames */
  int32_t cstep[3];      /* their per-frame steps toward the next targets */
  uint32_t pkey;         /* plate law cache key: Reverb ramp value, or 65536 */
  uint16_t pkrt, poml;   /* plate krt and 1-klp for pkey, Q15 */
  uint16_t ckey;         /* ckey 0 = empty (rate is 10..200) */
  uint8_t kk, pp;        /* control-rate counter (GC_KRATE); plate tick parity */
  /* -- audio state: cleared by clears, PANIC and reset -- */
  uint32_t ezero, czero, rzero; /* consecutive zero-input frames */
  int32_t eerr;                 /* echo encoder error-feedback residual */
  int32_t lp1, lp2;             /* plate in-loop low-pass states */
  int32_t pup[8];               /* plate tank outputs, ring at pui */
  int16_t pdn[16];              /* plate decimator input ring at pdi */
  uint16_t ew, eold, enew;      /* echo write index, old/new taps */
  uint16_t cw;                  /* chorus write index */
  uint16_t pwp;                 /* plate ring write pointer */
  /* Lazy clears (X1): writes (plate: ticks) since the processor's last
   * clear, saturating at its longest delay. A read of a slot older than
   * that count returns zero, so a clear is O(1) and never zeroes the pool
   * on the audio path. */
  uint16_t ecnt, ccnt, pcnt;
  uint8_t ek, eclr, cclr, rclr; /* echo fade step (64 = settled), cleared */
  /* Quiet (X2): the processor's audio state is logically all zero (init,
   * reset, PANIC, a silence clear) and stays so while its input is zero,
   * so its recurrence is skipped; any nonzero input clears the flag. */
  uint8_t eq, cq, rq;
  uint8_t pdi, pui;
} gc_fx_state;
GC_STATIC_ASSERT(sizeof(gc_fx_state) <= GC_FX_SCALAR_BYTES);

struct gc_core {
  uint32_t magic, seed, serial;
  uint8_t rendered, dirty_patch, dirty_kit;
  /* Ramps that may still move, set where ramps start or jump: bit t (0..3)
   * for track t's ramps (and its tctl copy), bit 4 for the master ramps. */
  uint8_t ramp_busy;
  uint64_t frame;
  gc_report last;
  uint8_t *fx_pool;
  const gc_sample_asset *assets;
  uint32_t asset_count, spare2;
  gc_ramp tramp[GC_TRACKS][GC_TR_RAMPS];
  /* Current Level, Bright, Wobble, Echo, Space, Chorus per track, copied from
   * tramp[t][0..5].cur whenever a ramp pass runs (ramp_busy): attached voices
   * read their controls here instead of six strided loads per voice. */
  uint16_t tctl[GC_TRACKS][6];
  gc_ramp mramp[GC_MR_RAMPS];
  uint16_t mstep[GC_MS_STEPS];
  int32_t msvf[2], dc_x, dc_pad;
  int32_t mF, mQ;      /* master SVF coefficients for cutoff/res keys mkc/mkr */
  uint16_t mkc, mkr;   /* mkc 0 = empty (cutoff is 20..8000) */
  int64_t dc_y; /* DC blocker output history with 16 fractional bits */
  uint8_t patch[GC_TRACKS][GC_PATCH_BYTES];
  uint8_t kit[GC_TRACKS][GC_KIT_BYTES];
  gc_fx_state fx;
  gc_voice voice[GC_VOICES];
};
GC_STATIC_ASSERT(sizeof(struct gc_core) <= GC_STATE_BYTES);
GC_STATIC_ASSERT(GC_ENGINES_AVAILABLE == 31u); /* SPEC/01:106 */

/* core.c helpers shared with tests */
void gc_default_va_patch(uint8_t *out128);
void gc_default_fm_patch(uint8_t *out128);
void gc_default_drum_patch(uint8_t *out128);
void gc_default_gnome_patch(uint8_t *out128);
/* Default one-shot Sample record over [0,frames) of asset `asset_id`. */
void gc_default_sample_patch(uint8_t *out128, uint16_t asset_id, uint16_t frames);
void gc_default_kit(uint8_t *out416);
void gc_record_seal(uint8_t *rec, uint32_t len); /* write CRC32C at 12..15 */
void gc_record_head(uint8_t *rec, uint32_t len, const char *magic, uint8_t b8,
                    const char *name); /* zeroed record with magic/version/len */
int32_t gc_master_default(unsigned idx);

/* voices.c */
void gc_ev_note_on(gc_core *c, uint8_t track, uint32_t id, uint8_t key,
                   uint8_t vel);
void gc_ev_note_off(gc_core *c, uint8_t track, uint32_t id);
void gc_ev_all_off(gc_core *c, uint8_t track);
void gc_ev_panic(gc_core *c, uint8_t track);
void gc_ev_parameter(gc_core *c, uint8_t track, uint16_t pid, int32_t value);
void gc_detach_track(gc_core *c, uint8_t track);
void gc_ramp_set(gc_ramp *r, uint16_t target);
void gc_ramp_jump(gc_ramp *r, uint16_t value);
uint32_t gc_voices_active(const gc_core *c);

/* mixer.c: one output frame (events of this frame already executed). */
int16_t gc_mix_frame(gc_core *c, uint32_t *sat, uint32_t *clips);
void gc_master_clear(gc_core *c);
/* Soft output ceiling of SPEC/20 (odd, monotone, |y| <= 29204). */
int32_t gc_ceiling(int32_t x, uint32_t *clips);

/* fx.c: companded echo, chorus, FDN reverb (SPEC/20). */
void gc_fx_reset(gc_core *c);  /* init/reset: clear audio and phases */
void gc_fx_panic(gc_core *c);  /* PANIC 255: clear audio, keep LFO phase */
void gc_fx_settle(gc_core *c); /* echo taps = current time, no fade */
int32_t gc_fx_process(gc_core *c, int32_t echo_in, int32_t space_in,
                      int32_t chorus_in, uint32_t *sat);
uint8_t gc_echo_encode(int32_t x);
int32_t gc_echo_decode(uint8_t b);

/* Engines. param_range: 1 if pid is an editable record word for the engine
 * (sets lo/hi and whether the wire word is signed), else 0 (rejected). */
/* Macro maps (macro.c, SPEC/CHANGES-1.3.0-macros). */
int gc_macro_id_ok(unsigned engine, unsigned id);
int gc_engine_param_range(unsigned engine, unsigned pid, int32_t *lo,
                          int32_t *hi, int *sgn);
uint32_t gc_fm_carrier_mask(unsigned alg);
int gc_va_param_range(unsigned pid, int32_t *lo, int32_t *hi, int *sgn);
int gc_va_param_live(unsigned pid);
void gc_va_note_on(gc_core *c, gc_voice *v);
void gc_va_note_off(gc_voice *v);
void gc_va_tune_edit(gc_voice *v, uint16_t raw_tune);
uint16_t gc_va_amp(const gc_voice *v);
int32_t gc_va_render(gc_voice *v, const uint16_t *ctl, uint32_t *sat);
/* VA saw banks, shared with the Gnome excitation (charged to VA). */
uint8_t gc_saw_bank(uint8_t key, int32_t cents);
int32_t gc_saw_at(uint8_t bank, uint32_t phase);

int gc_drums_param_range(unsigned pid, int32_t *lo, int32_t *hi, int *sgn);
void gc_drums_note_on(gc_core *c, gc_voice *v, const uint8_t *pad24);
uint16_t gc_drums_amp(const gc_voice *v);
int32_t gc_drums_render(gc_voice *v, const uint16_t *ctl, uint32_t *sat);
/* send = min(65535, R(pad_send*macro, 16384)) (SPEC/01, SPEC/12); the
 * product is nonnegative and below 2^32. */
static inline uint16_t gc_drums_send(const gc_voice *v, unsigned which,
                                     uint16_t macro) {
  uint32_t s = ((uint32_t)v->u.dr.send[which] * macro + 8192u) >> 14;
  return (uint16_t)(s > 65535u ? 65535u : s);
}

/* FM engine 1 (SPEC/11). */
int gc_fm_param_range(unsigned pid, int32_t *lo, int32_t *hi, int *sgn);
int gc_fm_param_live(unsigned pid);
int gc_fm_word_ok(unsigned pid, int32_t value); /* subfield domains */
void gc_fm_note_on(gc_core *c, gc_voice *v);
void gc_fm_note_off(gc_voice *v);
uint16_t gc_fm_amp(const gc_voice *v);
int32_t gc_fm_render(gc_voice *v, const uint16_t *ctl, uint32_t *sat);
void gc_fm_live(gc_voice *v, unsigned pid); /* refresh caches after a live edit */

/* Gnome Voice engine 3 (SPEC/13). */
int gc_gnome_param_range(unsigned pid, int32_t *lo, int32_t *hi, int *sgn);
int gc_gnome_param_live(unsigned pid);
void gc_gnome_note_on(gc_core *c, gc_voice *v);
void gc_gnome_note_off(gc_voice *v);
void gc_gnome_tune_edit(gc_voice *v, uint16_t raw_tune);
void gc_gnome_live(gc_voice *v, unsigned pid); /* after a live word edit */
uint16_t gc_gnome_amp(const gc_voice *v);
int32_t gc_gnome_render(gc_voice *v, const uint16_t *ctl, uint32_t *sat);
void gc_gnome_coeffs(uint32_t vowel, int32_t size, int32_t *a, int32_t *d,
                     int32_t *b);
uint32_t gc_gnome_shift(uint32_t hz, int32_t size); /* clamp 200..6000 Hz */
int32_t gc_gnome_scoop(int32_t scoop, uint32_t age); /* L(P18,0,min(age,1323),1323) */
void gc_gnome_bd(uint8_t bw_index, int32_t *b, int32_t *d); /* b, d of B=80+index */

/* Sample engine 4 (SPEC/14). */
int gc_sample_param_range(unsigned pid, int32_t *lo, int32_t *hi, int *sgn);
gc_status gc_sample_shape(const uint8_t *rec, unsigned pid);
const gc_sample_asset *gc_sample_find(const gc_core *c, uint32_t id);
gc_status gc_sample_assets(const gc_core *c, const uint8_t *rec, unsigned *pid);
int gc_sample_param_live(unsigned pid);
void gc_sample_note_on(gc_core *c, gc_voice *v);
void gc_sample_note_off(gc_voice *v);
uint16_t gc_sample_amp(const gc_voice *v);
int32_t gc_sample_render(gc_voice *v, const uint16_t *ctl, uint32_t *sat);
gc_status gc_sample_group_check(const gc_core *c, const gc_event *ev,
                                uint16_t first, uint16_t last, uint32_t *index,
                                uint32_t *detail);

#endif
