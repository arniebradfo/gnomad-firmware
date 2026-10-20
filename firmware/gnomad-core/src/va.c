/* Gnomad VA, engine 0 (SPEC/10-va.md, revision 1.1.1).
 *
 * Band-limited oscillators are generated harmonic-limited wavetables (D08),
 * the public wavetable method of J. O. Smith, Spectral Audio Signal
 * Processing, "Wavetable Synthesis":
 * https://mail.dsprelated.com/freebooks/sasp/Wavetable_Synthesis.html
 * The low-pass is the twice-iterated Chamberlin state-variable recurrence of
 * SPEC/02; filter family identified by Lazzarini/Timoney 2021,
 * https://arxiv.org/abs/2111.05592 . All arithmetic is SPEC/02 integer. */
#include "gc_internal.h"
#define GC_TABLES_DEFINE_VA
#include "tables.h"

/* P0..P23 domains (P11 reserved; P22 Pitch envelope and P23 Motion depth
 * since revision 1.1.6, SPEC/CHANGES-1.1.6 C4). lo is signed, hi unsigned. */
/* Domains are generated with the waveform tables from SPEC/10. */

int gc_va_param_range(unsigned pid, int32_t *lo, int32_t *hi, int *sgn) {
  if (pid >= 24u || pid == 11u) {
    return 0; /* reserved: must stay zero */
  }
  *lo = va_lo[pid];
  *hi = va_hi[pid];
  *sgn = (pid == 10u || pid == 20u || pid == 22u);
  return 1;
}

/* Words that reach attached sounding voices: envelope times/sustain (latched
 * at next stage entry), Tune, Width, Blend, Sub, Noise, Cutoff, Resonance. */
int gc_va_param_live(unsigned pid) {
  return (pid >= 1u && pid <= 4u) || pid == 10u || pid == 13u ||
         (pid >= 15u && pid <= 19u);
}

/* Largest bank H=2^(i+1)-1 with H*inc <= floor(0.45*2^32), else H=1.
 * Shared with the Gnome excitation (SPEC/13 reuses the VA saw banks). */
uint8_t gc_saw_bank(uint8_t key, int32_t cents) {
  uint32_t inc = gc_cap_inc(gc_X(gc_note_inc[key], cents));
  uint8_t i = 6;
  while (i > 0 && (uint64_t)((2u << i) - 1u) * inc > GC_INC_CAP) {
    i--;
  }
  return i;
}

/* Motion depth: P23 cents, 0 = the 1.1 law of 25 cents (C4). */
static inline uint32_t va_depth(const uint16_t *p) {
  return p[23] ? (uint32_t)p[23] : 25u;
}

/* Bank limit must cover both oscillators: osc1 at tune+pitch env+wob, osc2
 * at tune+Detune+pitch env+wob. Use the maximum wobble (the Motion depth),
 * only positive Detune and only a positive Pitch envelope (its largest
 * offset is at full envelope). */
static int32_t va_bank_cents(int32_t tune, const uint16_t *p) {
  int32_t pe = gc_s16(p[22]);
  return tune + (p[14] > 0u ? (int32_t)p[14] : 0) + (pe > 0 ? pe : 0) +
         (int32_t)va_depth(p);
}

void gc_va_note_on(gc_core *c, gc_voice *v) {
  gc_va_state *s = &v->u.va;
  uint16_t hz;
  (void)c;
  /* Zero per-note state only. Do not clear the SVF coefficient cache and then
   * leave it empty: seed from patch Cutoff so the first rendered sample is
   * filtered at P18, not the 20 Hz clamp floor / F=0 silence path. */
  s->ph1 = 0;
  s->ph2 = 0;
  s->phs = 0;
  s->lfo = 0;
  s->svf[0] = 0;
  s->svf[1] = 0;
  gc_zero(&s->env, sizeof(s->env));

  hz = (uint16_t)gc_clamp32((int64_t)v->p[18], 20, 11000);
  s->cF = gc_svf_f(hz);
  s->spare = 0;
  gc_env_segment(&s->env, 65535u, gc_ms_frames(v->p[1]), GC_ENV_ATTACK);
#if GC_KRATE == 1
  s->cHz = hz;
  s->kcut = 0; /* cutoff key empty: the first sample evaluates X */
  /* P21 Motion rate is next-note: its increment (< 2^21) is fixed here. */
  s->blfo = gc_saw_bank(v->h.key, va_bank_cents(gc_s16(v->p[10]), v->p)) |
            ((uint32_t)GC_RK((int64_t)v->p[21] * 4294967296LL, 4410000u) << 8);
#else
  s->bank = gc_saw_bank(v->h.key, va_bank_cents(gc_s16(v->p[10]), v->p));
#endif
}

void gc_va_note_off(gc_voice *v) {
  gc_va_state *s = &v->u.va;
  if (s->env.stage != GC_ENV_RELEASE && s->env.stage != GC_ENV_IDLE) {
    gc_env_segment(&s->env, 0u, gc_ms_frames(v->p[4]), GC_ENV_RELEASE);
  }
}

/* Live Tune edit: H=min(H,H_new) so pitch moves never switch to a richer
 * bank mid-note (SPEC/10 N4). */
void gc_va_tune_edit(gc_voice *v, uint16_t raw_tune) {
  uint8_t b = gc_saw_bank(v->h.key, va_bank_cents(gc_s16(raw_tune), v->p));
#if GC_KRATE == 1
  if (b < (v->u.va.blfo & 0xffu)) {
    v->u.va.blfo = (v->u.va.blfo & ~0xffu) | b;
  }
#else
  if (b < v->u.va.bank) {
    v->u.va.bank = b;
  }
#endif
}

uint16_t gc_va_amp(const gc_voice *v) { return gc_env_level(&v->u.va.env); }

/* Table lookup at length 2^bits: i=floor(p*N/2^32), u=(p*N) mod 2^32,
 * output L(T[i],T[(i+1) mod N],u,2^32). */
static int32_t va_table(const int16_t *t, uint32_t p, unsigned bits) {
  uint32_t i = p >> (32u - bits);
  uint32_t u = p << bits;
  int32_t a = t[i];
  int32_t b = t[(i + 1u) & ((1u << bits) - 1u)];
  return a + (int32_t)gc_Rp2((int64_t)(b - a) * (int64_t)u, 32u);
}

int32_t gc_saw_at(uint8_t bank, uint32_t p) {
  return va_table(gc_saw[bank], p, 8u);
}

/* One oscillator; off is the pulse phase offset R(width*2^32,65535). */
static int32_t va_osc(unsigned wave, uint8_t bank, uint32_t p, uint32_t off) {
  switch (wave) {
  case 0:
    return gc_sin32(p);
  case 1:
    return va_table(gc_saw[bank], p, 8u);
  case 2: {
    int32_t a = va_table(gc_saw[bank], p, 8u);
    int32_t b = va_table(gc_saw[bank], p + off, 8u);
    return gc_S15(gc_Rp2s(a - b, 1u)); /* S16 difference: 32-bit R */
  }
  default:
    return va_table(gc_tri[bank > 5u ? 5u : bank], p, 7u);
  }
}

/* One VA sample. ctl = current Level, Bright, Wobble, Echo, Space, Chorus
 * (track ramps when attached, frozen snapshot when detached). */
int32_t gc_va_render(gc_voice *v, const uint16_t *ctl, uint32_t *sat) {
  gc_va_state *s = &v->u.va;
  const uint16_t *p = v->p;
  uint8_t bank = GC_VA_BANK(s);
  uint32_t width, off, inc1, inc2;
  int32_t o1, o2, sub, noise, mainv, x, y;
  uint16_t env;
#if GC_KRATE == 1
  int32_t fc;
  {
    int32_t lfo = gc_sin32(s->lfo);
    /* R(depth*P6*sin,65535*32767); plus the Pitch envelope G(P22,env) */
    int32_t wob = gc_Rwk(va_depth(p), ctl[2], lfo) +
                  gc_Gs(gc_s16(p[22]), gc_env_level(&s->env));
    int32_t tune = gc_s16(p[10]);
    uint32_t k = gc_note_inc[v->h.key];
    inc1 = gc_cap_inc(gc_X(k, tune + wob));
    inc2 = gc_cap_inc(gc_X(k, tune + (int32_t)p[14] + wob));
    /* Width raw = clamp(P13+R(3*P6*sin32(lfo),20*32767),6554,58981) */
    width = (uint32_t)gc_clampi((int32_t)p[13] + gc_Rw3(ctl[2], lfo), 6554, 58981);
  }
#else
  int tick = gc_ktick(&v->h);
  if (tick) { /* control tick: LFO, increments, width (cutoff after env) */
    int32_t lfo = gc_sin32(s->lfo);
    /* R(depth*P6*sin,65535*32767); plus the Pitch envelope G(P22,env) */
    int32_t wob = gc_Rwk(va_depth(p), ctl[2], lfo) +
                  gc_Gs(gc_s16(p[22]), gc_env_level(&s->env));
    int32_t tune = gc_s16(p[10]);
    uint32_t k = gc_note_inc[v->h.key];
    s->inc1 = gc_cap_inc(gc_X(k, tune + wob));
    s->inc2 = gc_cap_inc(gc_X(k, tune + (int32_t)p[14] + wob));
    s->width = (uint16_t)gc_clampi((int32_t)p[13] + gc_Rw3(ctl[2], lfo), 6554, 58981);
    /* P21 is next-note; the LFO phase advances GC_KRATE increments */
    s->lfo += (uint32_t)GC_KRATE *
              (uint32_t)GC_RK((int64_t)p[21] * 4294967296LL, 4410000u);
  }
  inc1 = s->inc1;
  inc2 = s->inc2;
  width = s->width;
#endif
  /* pulse offset R(width*2^32,65535), exact 32-bit form */
  off = gc_va_off(width);
  o1 = va_osc(p[12], bank, s->ph1, off);
  o2 = va_osc(p[12], bank, s->ph2, off);
  sub = gc_sin32(s->phs);
  noise = gc_noise(&v->h.rng);
  mainv = o1 + gc_Gs(o2 - o1, p[15]); /* L(o1,o2,P15,65535), |o2-o1| <= 65534 */
  x = gc_S15(GC_RK32(mainv + gc_Gs(sub, p[16]) + gc_Gs(noise, p[17]), 3u));
  env = gc_env_step(&s->env, p);
#if GC_KRATE == 1
  fc = gc_clampi(gc_Rp2s(2400 * ((int32_t)ctl[1] - 32768), 15u) +
                     gc_Gs(gc_s16(p[20]), env),
                 -4800, 4800);
  if (fc != s->kfc || p[18] != s->kcut) {
    /* exact cache: the integer cutoff is a pure function of (P18, fc) */
    uint16_t hz = (uint16_t)gc_clamp32((int64_t)gc_X(p[18], fc), 20, 11000);
    s->kfc = (int16_t)fc;
    s->kcut = p[18];
    if (hz != s->cHz) {
      s->cF = gc_svf_f(hz); /* exact cache keyed by integer Hz */
      s->cHz = hz;
    }
  }
#else
  if (tick) { /* cutoff from Bright and this tick's envelope level */
    int32_t fc = gc_clampi(gc_Rp2s(2400 * ((int32_t)ctl[1] - 32768), 15u) +
                               gc_Gs(gc_s16(p[20]), env),
                           -4800, 4800);
    s->cF = gc_svf_f((uint32_t)gc_clamp32((int64_t)gc_X(p[18], fc), 20, 11000));
  }
#endif
  /* SVF, trim 3/1 (|3f| < 2^20), then ADSR, velocity, Level on the S18
   * magnitude with the sign once, final S15. */
  y = gc_S18i(3 * gc_svf_runi(s->svf, x, s->cF, gc_svf_q(p[19]), sat), sat);
  {
    uint32_t m = gc_Gm(gc_Gm(gc_Gm(y < 0 ? (uint32_t)0 - (uint32_t)y : (uint32_t)y, env),
                             v->h.vel_gain),
                       ctl[0]);
    y = gc_S15(y < 0 ? -(int64_t)m : (int64_t)m);
  }
  s->ph1 += inc1;
  s->ph2 += inc2;
  s->phs += inc1 >> 1;
#if GC_KRATE == 1
  s->lfo += s->blfo >> 8;
#endif
  if (s->env.stage == GC_ENV_IDLE) {
    v->h.flags = 0; /* release complete: slot idle from the next sample */
  }
  return y;
}
