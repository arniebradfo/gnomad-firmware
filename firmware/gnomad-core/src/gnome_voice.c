/* Gnome Voice, engine 3 (SPEC/13-gnome-voice.md, revision 2; CHANGES-1.1.7
 * adds the voice gestures P20..P27 and the Bright tilt, every one the 1.1
 * law at its default, so a 1.1 record renders bit-identically).
 *
 * Source/filter synthesis: a harmonic-limited saw (the VA banks) mixed with
 * noise, an amplitude "growl" at the subharmonic, and three parallel
 * two-pole resonators at interpolated formant anchors. The parallel-formant
 * method is the public one identified by the Oxford Phonetics IPOX lecture
 * "Parameter generation and synthesis" (Klatt 1980 identification),
 * https://www.phon.ox.ac.uk/files/IPOX_lectures/arch/synth.htm ; anchors and
 * every equation are SPEC/13's, all arithmetic is SPEC/02 integer. */
#include "gc_internal.h"
#define GC_TABLES_DEFINE_GNOME
#include "tables.h"

#define Q30 1073741824LL
#define SCOOP_FRAMES 1323u

int gc_gnome_param_range(unsigned pid, int32_t *lo, int32_t *hi, int *sgn) {
  if (pid >= GC_GNOME_WORDS || pid == 11u) {
    return 0; /* reserved: must stay zero */
  }
  *lo = gn_lo[pid];
  *hi = gn_hi[pid];
  *sgn = pid == 10u || pid == 13u || pid == 18u || pid == 22u;
  return 1;
}

/* Live words reaching attached voices (SPEC/13 Update column; SPEC/01 D07):
 * envelope times/sustain (latched at stage entry), Tune, Vowel, Size,
 * Breath, Growl, Smile, Aspiration, Softness. Motion rate, Scoop, Chatter
 * and the gesture words P20..P25 are next-note. */
int gc_gnome_param_live(unsigned pid) {
  return (pid >= 1u && pid <= 4u) || pid == 10u || (pid >= 12u && pid <= 15u) ||
         pid == 17u || pid == 26u || pid == 27u;
}

/* Syllables (P20, CHANGES-1.1.7 G1): start and end vowel of each, glided by
 * the gesture position, and its consonant flavour. The first five are the
 * plain vowels A E I O U (a 5-key cycle sings them), then seven glides. */
enum { GN_CB = 0, GN_CD = 1, GN_CK = 2 };
static const uint16_t gn_syl_v[12][2] = {
    {512, 512},  /* ah */ {768, 768},   /* eh */ {1024, 1024}, /* ee */
    {256, 256},  /* oh */ {0, 0},       /* oo */ {0, 512},     /* wa */
    {1024, 256}, /* yo */ {256, 1024},  /* oi */ {512, 1024},  /* ai */
    {512, 0},    /* ow */ {1024, 512},  /* ya */ {0, 768}};    /* we */
static const uint8_t gn_syl_c[12] = {GN_CD, GN_CB, GN_CD, GN_CK, GN_CB, GN_CB,
                                     GN_CD, GN_CB, GN_CK, GN_CD, GN_CK, GN_CB};
/* Consonant onsets (P24, G5): length L frames and floor(2^24/L). b and d
 * colour the excitation (so the vowel's formants shape them), k is a short
 * brighter burst after the formants. */
static const uint16_t gn_on_len[3] = {264u, 353u, 573u};
static const uint16_t gn_on_rcp[3] = {63550u, 47527u, 29279u};

#define GN_GLIDE_END 1048576u /* gesture position 2^20 = done */

/* Positive pitch excursion of a note above Tune: vibrato depth (P23, 0 = the
 * 1.1 law of 20 cents), a rising Scoop's start and a positive Bend's start. */
static int32_t gn_up_cents(const gc_voice *v) {
  int32_t scoop = gc_s16(v->p[18]), bend = gc_s16(v->p[22]);
  return (v->p[23] ? (int32_t)v->p[23] : 20) + (scoop > 0 ? scoop : 0) +
         (bend > 0 ? bend : 0);
}

/* Saw bank covering the vibrato, both signed Scoop endpoints and Bend. */
static uint8_t gn_bank(const gc_voice *v, int32_t tune) {
  return gc_saw_bank(v->h.key, tune + gn_up_cents(v));
}

void gc_gnome_note_on(gc_core *c, gc_voice *v) {
  gc_gnome_state *s = &v->u.gn;
  uint32_t n = v->p[20];
  (void)c;
  gc_zero(s, sizeof(*s));
  /* chatter=R(((voice_seed mod 65536)-32768)*P19,32768), PRNG not advanced. */
  s->chatter = (int16_t)gc_R(((int64_t)(v->h.rng & 0xffffu) - 32768) * v->p[19],
                             32768u);
  s->lfo_inc = (uint32_t)gc_R((int64_t)v->p[16] * 4294967296LL, 4410000u);
  s->ckey_v = 0xffffu; /* coefficient cache empty */
  s->bank = gn_bank(v, gc_s16(v->p[10]));
  s->pkey = INT16_MIN; /* increment cache empty */
  /* Syllable: P20 1..12 cycles the first P20 syllables by key (key mod P20),
   * 13..24 sings syllable P20-13 on every key, 0 none. */
  if (n >= 13u) {
    s->syl = (uint8_t)(n - 13u);
  } else if (n > 0u) {
    uint32_t k = v->h.key;
    while (k >= n) { /* key mod n, n <= 12: at most 127 subtractions */
      k -= n;
    }
    s->syl = (uint8_t)k;
  } else {
    s->syl = 0xffu;
  }
  gc_env_segment(&s->env, 65535u, gc_ms_frames(v->p[1]), GC_ENV_ATTACK);
}

void gc_gnome_note_off(gc_voice *v) {
  gc_gnome_state *s = &v->u.gn;
  if (s->env.stage != GC_ENV_RELEASE && s->env.stage != GC_ENV_IDLE) {
    gc_env_segment(&s->env, 0u, gc_ms_frames(v->p[4]), GC_ENV_RELEASE);
  }
}

/* Live Tune: never switch to a richer bank mid-note (as VA, SPEC/10 N4). */
void gc_gnome_tune_edit(gc_voice *v, uint16_t raw_tune) {
  uint8_t b = gn_bank(v, gc_s16(raw_tune));
  if (b < v->u.gn.bank) {
    v->u.gn.bank = b;
  }
}

uint16_t gc_gnome_amp(const gc_voice *v) { return gc_env_level(&v->u.gn.env); }

/* A live Size edit (P13) empties the vowel-keyed coefficient cache. */
void gc_gnome_live(gc_voice *v, unsigned pid) {
  if (pid == 13u) {
    v->u.gn.ckey_v = 0xffffu;
  }
}

/* floor(y/44100) for y < 2^32 without a division: estimate with
 * floor(2^32/44100)=97391 (never above the quotient), one correction. */
static uint32_t gn_fdiv44100(uint32_t y) {
  uint32_t q = (uint32_t)(((uint64_t)y * 97391u) >> 32);
  return q + ((y - q * 44100u) >= 44100u ? 1u : 0u);
}

/* Formant shift X(F,Size) clamped to 200..6000 Hz on the unsigned value
 * (no signed reinterpretation of the uint64 result). */
uint32_t gc_gnome_shift(uint32_t hz, int32_t size) {
  uint64_t f = gc_X(hz, size);
  return f < 200u ? 200u : (f > 6000u ? 6000u : (uint32_t)f);
}

/* Resonator coefficients for one effective vowel and Size (SPEC/13):
 * anchors interpolated by L(.,.,u,256), F shifted by X(F,Size) and clamped to
 * 200..6000 Hz, cq=Qsin(sin32(R(F*2^32,44100)+2^30)), a=R(2*Rq*cq,2^30) with
 * Rq=2^30-R(PIQ*B,44100); b=2^30-Rq and d=R(Rq*Rq,2^30) depend on the
 * integer bandwidth B only and are the generated gn_btab/gn_dtab[B-80]
 * (CHANGES-1.1.7 G6). Runs on cache misses in the render path, so every
 * rational is evaluated without a division:
 *  R(PIQ*B,44100) = B*floor(PIQ/44100) + floor((B*(PIQ mod 44100)+22050)/44100)
 *  R(F*2^32,44100) = F*97391 + floor((F*24196+22050)/44100), 2^32=44100*97391+24196
 *  Qsin(s)=R(s*2^30,32767) = sign(s)*(32769*|s| + (|s|>=16384)), 2^30=32767*32769+1
 * (tests/test_gnome.c checks each against gc_R over its whole domain). */
static void gn_coeffs(uint32_t vowel, int32_t size, int32_t *a, uint8_t *bwi) {
  uint32_t i = vowel >> 8, u = vowel & 255u, f;
  const uint16_t *r0, *r1;
  unsigned xs;
  uint32_t xe = gc_X_e(size, &xs); /* X(F,Size)=R(F*xe,2^xs), once for 3 F */
  if (i >= 4u) {
    i = 3u; /* vowel 1024: row 4 exactly, as L(row3,row4,256,256) */
    u = 256u;
  }
  r0 = gn_anchor + 6u * i;
  r1 = r0 + 6;
  for (f = 0; f < 3u; f++) {
    /* anchors are small: L(.,.,u,256) in 32 bits */
    int32_t hz = r0[f] + gc_Rp2s((r1[f] - r0[f]) * (int32_t)u, 8u);
    int32_t bw = r0[3u + f] + gc_Rp2s((r1[3u + f] - r0[3u + f]) * (int32_t)u, 8u);
    uint64_t fx = ((uint64_t)(uint32_t)hz * xe + ((uint64_t)1 << (xs - 1u))) >> xs;
    uint32_t fs = fx < 200u ? 200u : (fx > 6000u ? 6000u : (uint32_t)fx);
    /* 0 < rq <= 2^30 and |cq| <= 2^30: int32 operands, 32x32 products */
    int32_t rq = (int32_t)(Q30 - gn_btab[bw - 80]);
    uint32_t ph = fs * 97391u + gn_fdiv44100(fs * 24196u + 22050u) + 0x40000000u;
    int32_t cq = (int32_t)gc_qsin30(gc_sin32i(ph));
    a[f] = (int32_t)gc_Rp2(2 * ((int64_t)rq * cq), 30u);
    bwi[f] = (uint8_t)(bw - 80);
  }
}

void gc_gnome_bd(uint8_t bwi, int32_t *b, int32_t *d) {
  *b = gn_btab[bwi];
  *d = gn_dtab[bwi];
}

void gc_gnome_coeffs(uint32_t vowel, int32_t size, int32_t *a, int32_t *d,
                     int32_t *b) {
  uint8_t bw[3];
  unsigned f;
  gn_coeffs(vowel, size, a, bw);
  for (f = 0; f < 3u; f++) {
    b[f] = gn_btab[bw[f]];
    d[f] = gn_dtab[bw[f]];
  }
}

/* Scoop L(P18,0,age,1323), age=min(t,1323): with a=|P18| <= 200 the B18
 * stepper's carry count is floor((661+age*a)/1323), and floor(y/1323) =
 * (y*3246385)>>32 exactly for y < 2^19 (test gnome_exact_render_math). */
int32_t gc_gnome_scoop(int32_t sc, uint32_t t) {
  uint32_t age = t < SCOOP_FRAMES ? t : SCOOP_FRAMES;
  uint32_t a = (uint32_t)(sc < 0 ? -sc : sc);
  int32_t mag = (int32_t)(((uint64_t)(661u + age * a) * 3246385u) >> 32);
  return sc < 0 ? sc + mag : sc - mag;
}

/* One Gnome sample. ctl = Level, Bright, Wobble, Echo, Space, Chorus. */
int32_t gc_gnome_render(gc_voice *v, const uint16_t *ctl, uint32_t *sat) {
  gc_gnome_state *s = &v->u.gn;
  const uint16_t *p = v->p;
  int32_t noise = gc_noise(&v->h.rng); /* one draw per frame */
  int32_t x, z, y, f[3], post = 0;
  uint32_t inc, m;
  uint16_t env;
  unsigned i;

  if (gc_ktick(&v->h)) { /* control tick: LFO, vowel, pitch (exact caches) */
    int32_t lfo = gc_sin32(s->lfo);
    int32_t motion = gc_Rwk(128u, ctl[2], lfo); /* R(128*P6*sin,65535*32767) */
    /* vibrato R(D*P6*sin,65535*32767), D = P23 or the 1.1 law's 20 cents */
    int32_t vib = gc_Rwk(p[23] ? (uint32_t)p[23] : 20u, ctl[2], lfo);
    /* Bright offset R((P5-32768)*256,32768): |n| < 2^23, 32-bit R. */
    int32_t bo = gc_Rp2s(((int32_t)ctl[1] - 32768) * 256, 15u);
    /* gesture position min(t*P21, 2^20); P21 = 0: done at once */
    uint32_t pos = p[21] == 0u ? GN_GLIDE_END : (uint32_t)s->t * p[21];
    int32_t base = p[12], cents;
    uint32_t vowel;
    if (pos > GN_GLIDE_END) {
      pos = GN_GLIDE_END;
    }
    if (s->syl != 0xffu) { /* syllable glide L(v0,v1,pos,2^20) + P12-512 */
      const uint16_t *sv = gn_syl_v[s->syl];
      base += sv[0] - 512 + (int32_t)gc_Rp2((int64_t)(sv[1] - sv[0]) * pos, 20u);
    }
    vowel = (uint32_t)gc_clampi(base + bo + s->chatter + motion, 0, 1024);
    /* Tune + vibrato + Scoop + Bend R(P22*(2^20-pos),2^20); |.| <= 5000 */
    cents = gc_s16(p[10]) + vib + gc_gnome_scoop(gc_s16(p[18]), s->t);
    if (p[22] != 0u) {
      cents += (int32_t)gc_Rp2((int64_t)gc_s16(p[22]) * (GN_GLIDE_END - pos), 20u);
    }
    if (cents != s->pkey) { /* exact cache: X(I[key],cents) of an integer key */
      s->pkey = (int16_t)cents;
      s->inc = gc_cap_inc(gc_X(gc_note_inc[v->h.key], cents));
    }
    if (vowel != s->ckey_v) {
      gn_coeffs(vowel, gc_s16(p[13]), s->ca, s->bw); /* exact cache */
      s->ckey_v = (uint16_t)vowel;
    }
    s->lfo += (uint32_t)GC_KRATE * s->lfo_inc;
  }
  inc = s->inc;
  {
    /* L(saw,noise,Breath,65535) = saw + G(noise-saw, Breath): the result lies
     * between saw and noise (S16), |noise-saw| <= 65535, so gc_Gs. */
    int32_t saw = gc_saw_at(s->bank, s->ph);
    if (p[27] != 0u) {
      /* Softness: one-pole tilt of the voiced source, coefficient
       * k=16384-G(P27,15500) in Q14 (|saw-slp|*k < 2^30). */
      int32_t k = 16384 - gc_Gs(p[27], 15500u);
      s->slp = (int16_t)(s->slp + gc_Rp2s((saw - s->slp) * k, 14u));
      saw = s->slp;
    } else {
      s->slp = (int16_t)saw;
    }
    x = saw + gc_Gs(noise - saw, p[14]);
    if ((p[26] | p[24]) != 0u) {
      /* Breath that sits under the voice (G4): noise low-passed by one pole
       * (k=1/4, about 2 kHz), gated by the glottal cycle (a triangle of the
       * saw phase, so it is pitched and fuses with the voice), added to the
       * excitation without lowering the voiced part, so the formants shape
       * it like the vowel. */
      s->alp = (int16_t)(s->alp + gc_Rp2s((noise - s->alp) * 16384, 16u));
      if (p[26] != 0u) {
        uint32_t h = s->ph >> 16;
        uint32_t tri = h < 32768u ? 2u * h : 2u * (65535u - h);
        x += gc_Gs(gc_Gs(s->alp, tri), p[26]);
      }
      if (p[24] != 0u) {
        /* Consonant onset (G5): flavour from the syllable (d without one),
         * envelope ((L-t)/L)^2 over L frames. */
        unsigned fl = s->syl == 0xffu ? (unsigned)GN_CD : gn_syl_c[s->syl];
        if (s->t < gn_on_len[fl]) {
          uint32_t e = ((uint32_t)(gn_on_len[fl] - s->t) * gn_on_rcp[fl]) >> 8;
          int32_t c;
          e = e > 65535u ? 65535u : e;
          e = (uint32_t)gc_Gs((int32_t)e, e);
          if (fl == GN_CK) { /* bright: the noise above the one-pole */
            post = gc_Gs(gc_Gs(noise - s->alp, e), p[24]);
          } else {
            c = fl == GN_CB ? s->alp : gc_Rp2s(s->alp + noise, 1u);
            x += gc_Gs(gc_Gs(c, e), p[24]);
          }
        }
      }
      x = gc_clampi(x, -32768, 32767);
    }
    if (p[15] != 0u) {
      /* growl gain 65535-R(P15*(32767+sin32(sub)),4*32767): n < 2^32 and
       * n+65534 < 2^32; R = floor(floor((n+65534)/4)/32767) with z=a*32768+b
       * = a*32767+(a+b), a+b < 2^16. Growl 0 is gain 65535, G identity. */
      /* With a Growl rate (P25 != 0, G7) the depth is twice the 1.1 law's:
       * 65535-R(P15*(32767+sin32(sub)),2*32767), a full chop at P15 65535
       * (zz = floor((n+32767)/2), the same fold by 32767 follows). */
      uint32_t n = (uint32_t)p[15] * (uint32_t)(32767 + gc_sin32(s->sub));
      uint32_t zz = p[25] != 0u ? (n + 32767u) >> 1 : (n + 65534u) >> 2;
      uint32_t a = zz >> 15, t = a + (zz & 0x7fffu);
      uint32_t q = a + (t >= 32767u ? 1u : 0u) + (t >= 65534u ? 1u : 0u);
      x = gc_Gs(x, 65535u - q);
    }
    if (ctl[1] != 32768u) {
      /* Bright tilt of the excitation (G2), on the shared history x1: below
       * the centre a one-pole low-pass x1+G(x-x1,k), k=0.08+0.92u (u =
       * Bright/32768); above it a high shelf x+G(x-x1,k), k up to 0.6 (about
       * +12 dB at the top). Both are the identity at 32768. */
      uint32_t b = ctl[1];
      if (b < 32768u) {
        x = s->x1 + gc_Gs(x - s->x1, 6554u + (uint32_t)gc_Gs(58981, 2u * b));
      } else {
        x = gc_clampi(x + gc_Gs(x - s->x1, (uint32_t)gc_Gs(52428, 2u * (b - 32768u))),
                      -32768, 32767);
      }
    }
  }
  {
    /* Resonators: x-x2 is S16 and y1, y2 are S18, so every product is a
     * 32x32 multiply; |acc| < 2^51 and R(acc,2^30) fits int32 before S18. */
    int32_t dx = x - s->x2;
    for (i = 0; i < 3u; i++) {
      int64_t acc = (int64_t)gn_btab[s->bw[i]] * dx + (int64_t)s->ca[i] * s->y[i][0] -
                    (int64_t)gn_dtab[s->bw[i]] * s->y[i][1];
      f[i] = gc_S18i((int32_t)gc_Rp2(acc, 30u), sat);
      s->y[i][1] = s->y[i][0];
      s->y[i][0] = f[i];
    }
  }
  s->x2 = s->x1;
  s->x1 = (int16_t)x;
  {
    /* weighted sum of three S18 sections: |.| < 2^20, int32. Bright tilt
     * (G2): at 32768 the 1.1 weights 1, 0.75, 0.25+Smile/2; below it F2 and
     * F3 fall by g=0.1+0.9u and g^2 (u=Bright/32768); above it F1 falls to
     * 0.35 while F2 rises to 1.5 and F3 to 2 (u=(Bright-32768)/32768). */
    uint32_t w2 = 16384u + ((uint32_t)p[17] + 1u) / 2u, b = ctl[1];
#ifdef GN_NO_TILT
    b = 32768u;
#endif
    if (b == 32768u) {
      z = f[0] + gc_G(f[1], 49152u) + gc_G(f[2], w2);
    } else if (b < 32768u) {
      uint32_t g = 6554u + (uint32_t)gc_Gs(58981, 2u * b);
      z = f[0] + gc_G(f[1], (uint32_t)gc_Gs(49152, g)) +
          gc_G(f[2], (uint32_t)gc_Gs(gc_Gs((int32_t)w2, g), g));
    } else {
      uint32_t u = 2u * (b - 32768u), h2 = (w2 + 1u) >> 1;
      z = gc_G(f[0], 65535u - (uint32_t)gc_Gs(42598, u)) +
          2 * gc_G(f[1], 24576u + (uint32_t)gc_Gs(24576, u)) +
          2 * gc_G(f[2], h2 + (uint32_t)gc_Gs((int32_t)(65535u - h2), u));
    }
    z = gc_S18i(z + post, sat);
  }
  /* Trim 4/1 (SPEC/02, D11/D16), then ADSR, velocity, Level (one sign, the
   * magnitude through three gains), S15. */
  env = gc_env_step(&s->env, p);
  y = gc_S18i(4 * z, sat);
  m = gc_Gm(gc_Gm(gc_Gm(y < 0 ? (uint32_t)0 - (uint32_t)y : (uint32_t)y, env),
                  v->h.vel_gain),
            ctl[0]);
  y = m > 32767u ? (y < 0 ? -32768 : 32767) : (y < 0 ? -(int32_t)m : (int32_t)m);
  s->ph += inc;
  /* growl phase: the subharmonic R(inc,2), or P25*9739 (P25 in 0.1 Hz,
   * 9739 = floor(2^32/441000)) for a low-rate flutter (G7) */
  s->sub += p[25] != 0u ? (uint32_t)p[25] * 9739u : (inc + 1u) >> 1;
  if (s->t < 65535u) {
    s->t++;
  }
  if (s->env.stage == GC_ENV_IDLE) {
    v->h.flags = 0; /* release complete: slot idle from the next sample */
  }
  return y;
}

void gc_default_gnome_patch(uint8_t *b) {
  unsigned i;
  gc_record_head(b, GC_PATCH_BYTES, "GCP1", GC_GNOME, "Init Gnome");
  for (i = 0; i < GC_GNOME_WORDS; i++) {
    gc_wr16(b + GC_PATCH_P(i), gnome_defaults[i]);
  }
  gc_record_seal(b, GC_PATCH_BYTES);
}

