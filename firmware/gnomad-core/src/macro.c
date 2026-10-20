/* Per-sound macro maps (SPEC/CHANGES-1.3.0-macros, profile 0x00010300).
 *
 * A patch's four knobs are four recipe IDs, record bytes 10, 11, 54, 55
 * (slots 0..3; bytes 10..11 were the zero flags, 54..55 the reserved P11).
 * ID 0 is the slot's default, the 1.2 behaviour: the identity onto P5+slot
 * (TONE, MOTION, ECHO, SPACE). ID n >= 1 is entry n of the record's engine
 * recipe table below: a label and one or two targets, each a parameter (or
 * an FM operator group) with a law and two endpoints. IDs are append-only:
 * an entry never changes meaning once a record can carry it.
 *
 * Everything here runs at host/control rate (a knob move), never on the
 * per-sample path: a move resolves to ordinary PARAMETER writes the host
 * sends (P10..P47 first, so they form one record group). The render loop is
 * untouched, so rendering equals profile 0x00010200 bit for bit.
 *
 * Laws (amount a in 0..65535, the knob's rest r, the record's stored base):
 *   IDENT  value = a (targets P5..P9 only); r = the stored value.
 *   LIN    absolute endpoints lo (a=0) and hi (a=65535) in the target's
 *          semantic unit; r = where base lies on that line (clamped).
 *   CENTS  value = X(base, c), c from lo cents (a=0) through 0 (a=r) to hi
 *          cents (a=65535): a log curve relative to the stored value.
 *   SCALE  value = R(base*f, 256), f from lo/256 through 1 to hi/256.
 * Every target travels two straight pieces, (0, lo-end) -> (r, base) ->
 * (65535, hi-end) in its law's domain, so the rest amount reproduces the
 * stored value exactly. A second target shares the first target's rest.
 * Results clamp to the target's SPEC domain. Integer only; R() is the
 * shift-subtract rounding of SPEC/02 (no division instruction). */
#include "gc_internal.h"

enum { MC_NONE = 0, MC_IDENT, MC_LIN, MC_CENTS, MC_SCALE };

/* Operator-group targets (FM): one write per matching operator word. */
enum {
  MC_FM_CAR_DECAY = 64, /* carriers' decay (word 3 bits 0..13) */
  MC_FM_PUNCH,          /* short modulators' level (decay <= 150 ms) */
  MC_FM_MOD_ATTACK,     /* modulators' attack (word 2 bits 0..12) */
  MC_FM_MOD_RATIO,      /* ratio-mode modulators' pitch code (word 0) */
  MC_FM_MOD_LEVEL,      /* modulators' level (word 1) */
  MC_FM_CAR_ATTACK,     /* carriers' attack (word 2 bits 0..12) */
  MC_FM_MOD_DECAY,      /* modulators' decay (word 3 bits 0..13) */
  MC_FM_MOD_PITCH,      /* every modulator's pitch code, ratio or Hz */
  MC_FM_BODY_LEVEL      /* long modulators' level (decay > 150 ms) */
};

/* Shared label vocabulary (ids are wire values: append-only). */
static const char mc_labels[GC_MACRO_LABELS][8] = {
    "TONE",  "MOTION", "ECHO",  "SPACE", "BRIGHT", "WOBBLE",  "DECAY",
    "ATTACK", "SHIMMER", "PUNCH", "GROWL", "WIDTH", "DRIVE",  "BEND",
    "GLIDE", "VOWEL",  "BREATH", "VIBRATO", "BUZZ", "TALK",   "SWIRL",
    "TAIL",  "COLOR",  "SNAP",  "PITCH", "SPEED",  "DEPTH",   "CHAOS",
    "SIZE",  "SUB",    "KICK",  "SMILE"};
enum {
  MCL_TONE, MCL_MOTION, MCL_ECHO, MCL_SPACE, MCL_BRIGHT, MCL_WOBBLE, MCL_DECAY, MCL_ATTACK,
  MCL_SHIMMER, MCL_PUNCH, MCL_GROWL, MCL_WIDTH, MCL_DRIVE, MCL_BEND, MCL_GLIDE, MCL_VOWEL,
  MCL_BREATH, MCL_VIBRATO, MCL_BUZZ, MCL_TALK, MCL_SWIRL, MCL_TAIL, MCL_COLOR, MCL_SNAP,
  MCL_PITCH, MCL_SPEED, MCL_DEPTH, MCL_CHAOS, MCL_SIZE, MCL_SUB, MCL_KICK, MCL_SMILE
};

typedef struct {
  uint8_t to, law;
  int32_t lo, hi;
} mc_target;
typedef struct {
  uint8_t label;
  mc_target t[2];
} mc_recipe;

#define MC_T1(to, law, lo, hi) {{(to), (law), (lo), (hi)}, {0, MC_NONE, 0, 0}}
#define MC_T2(a, la, alo, ahi, b, lb, blo, bhi)                                   \
  {{(a), (la), (alo), (ahi)}, {(b), (lb), (blo), (bhi)}}

/* Recipe tables, entry 0 unused (ID 0 = slot default). The "@ engine id KEY"
 * markers are read by tools/author/gcrec.py: the source name of a recipe is
 * its KEY, unique per engine. */
static const mc_recipe mc_va[] = {
    {0, MC_T1(0, MC_NONE, 0, 0)},
    /*@ va 1 BRIGHT */ {MCL_BRIGHT, MC_T1(5, MC_IDENT, 0, 0)},
    /*@ va 2 WOBBLE */ {MCL_WOBBLE, MC_T1(6, MC_IDENT, 0, 0)},
    /*@ va 3 VIBRATO */ {MCL_VIBRATO, MC_T1(6, MC_IDENT, 0, 0)},
    /*@ va 4 ECHO */ {MCL_ECHO, MC_T1(7, MC_IDENT, 0, 0)},
    /*@ va 5 SPACE */ {MCL_SPACE, MC_T1(8, MC_IDENT, 0, 0)},
    /* chorus send and oscillator detune (next note) */
    /*@ va 6 SWIRL */ {MCL_SWIRL, MC_T2(9, MC_IDENT, 0, 0, 14, MC_LIN, 0, 30)},
    /* detune (next note) and chorus (live) together */
    /*@ va 7 WIDTH */ {MCL_WIDTH, MC_T2(14, MC_LIN, 0, 40, 9, MC_LIN, 0, 45875)},
    /* pulse width: square (hollow) to thin (buzzy) */
    /*@ va 8 BUZZ */ {MCL_BUZZ, MC_T1(13, MC_LIN, 32768, 6554)},
    /* filter envelope amount */
    /*@ va 9 PUNCH */ {MCL_PUNCH, MC_T1(20, MC_LIN, 0, 4800)},
    /* resonance */
    /*@ va 10 GROWL */ {MCL_GROWL, MC_T1(19, MC_LIN, 0, 60948)},
    /*@ va 11 SUB */ {MCL_SUB, MC_T1(16, MC_LIN, 0, 65535)},
    /* decay time and sustain: a pluck at 0, held all the way at 65535 */
    /*@ va 12 DECAY */
    {MCL_DECAY, MC_T2(2, MC_CENTS, -2400, 2400, 3, MC_LIN, 0, 65535)},
    /*@ va 13 TAIL */ {MCL_TAIL, MC_T1(4, MC_CENTS, -3600, 2400)},
    /* pitch envelope x0..x2 (FUNNY v2 sweeps) */
    /*@ va 14 PITCH */ {MCL_PITCH, MC_T1(22, MC_SCALE, 0, 512)},
    /* faster: shorter envelope (the sweep) and faster LFO */
    /*@ va 15 SPEED */
    {MCL_SPEED, MC_T2(2, MC_CENTS, 1200, -2400, 21, MC_CENTS, -1200, 1200)},
    /* rotary-ish: vibrato/PWM depth and LFO rate rise together */
    /*@ va 16 ROTARY */
    {MCL_SPEED, MC_T2(6, MC_LIN, 0, 52428, 21, MC_LIN, 100, 800)},
    /* noise and wobble */
    /*@ va 17 CHAOS */
    {MCL_CHAOS, MC_T2(17, MC_LIN, 0, 39321, 6, MC_LIN, 0, 65535)},
    /*@ va 18 BREATH */ {MCL_BREATH, MC_T1(17, MC_LIN, 0, 32768)},
    /* the wind's colour: noise into a ringing filter (it whistles) */
    /*@ va 19 COLOR */
    {MCL_COLOR, MC_T2(17, MC_LIN, 0, 45875, 19, MC_LIN, 0, 52428)},
    /* the laser's dive: pitch envelope from a rise (-2 octaves) through
     * none to a 4-octave "pew" */
    /*@ va 20 ZAP */ {MCL_PITCH, MC_T1(22, MC_LIN, -2400, 4800)},
};

static const mc_recipe mc_fm[] = {
    {0, MC_T1(0, MC_NONE, 0, 0)},
    /*@ fm 1 BRIGHT */ {MCL_BRIGHT, MC_T1(5, MC_IDENT, 0, 0)},
    /*@ fm 2 VIBRATO */ {MCL_VIBRATO, MC_T1(6, MC_IDENT, 0, 0)},
    /*@ fm 3 ECHO */ {MCL_ECHO, MC_T1(7, MC_IDENT, 0, 0)},
    /*@ fm 4 SPACE */ {MCL_SPACE, MC_T1(8, MC_IDENT, 0, 0)},
    /*@ fm 5 SWIRL */ {MCL_SWIRL, MC_T1(9, MC_IDENT, 0, 0)},
    /* how long it rings: carriers' decay and the release fallback */
    /*@ fm 6 DECAY */
    {MCL_DECAY, MC_T2(MC_FM_CAR_DECAY, MC_CENTS, -2400, 1800, 4, MC_CENTS, -2400,
                 1800)},
    /*@ fm 7 TAIL */ {MCL_TAIL, MC_T1(4, MC_CENTS, -2400, 2400)},
    /* the modulators' pitch (the bell's metal, inharmonic between the
     * ends) and a little feedback sizzle */
    /*@ fm 8 SHIMMER */
    {MCL_SHIMMER,
     MC_T2(MC_FM_MOD_RATIO, MC_CENTS, -1200, 1200, 13, MC_LIN, 0, 49152)},
    /* the body: the long modulators' level and feedback (PUNCH owns the
     * short ones, so PUNCH and GROWL never write one word) */
    /*@ fm 9 GROWL */
    {MCL_GROWL, MC_T2(MC_FM_BODY_LEVEL, MC_CENTS, -1200, 1200, 13, MC_LIN, 0, 65535)},
    /*@ fm 10 DRIVE */
    {MCL_DRIVE, MC_T2(MC_FM_MOD_LEVEL, MC_CENTS, -1200, 1200, 13, MC_LIN, 0, 65535)},
    /* rotary-ish: vibrato depth and motion rate rise together */
    /*@ fm 11 ROTARY */
    {MCL_SPEED, MC_T2(6, MC_LIN, 0, 52428, 15, MC_LIN, 80, 900)},
    /*@ fm 12 PUNCH */ {MCL_PUNCH, MC_T1(MC_FM_PUNCH, MC_CENTS, -2400, 1200)},
    /* the swell: modulators' attack and the carriers' fade-in */
    /*@ fm 13 ATTACK */
    {MCL_ATTACK, MC_T2(MC_FM_MOD_ATTACK, MC_CENTS, -2400, 2400, MC_FM_CAR_ATTACK,
                  MC_LIN, 0, 300)},
    /* the zap: how fast the modulators collapse (slower .. faster) */
    /*@ fm 14 ZAP */
    {MCL_SPEED, MC_T1(MC_FM_MOD_DECAY, MC_CENTS, 1200, -2400)},
    /* feedback noise and inharmonic modulators together */
    /*@ fm 15 CHAOS */
    {MCL_CHAOS, MC_T2(13, MC_LIN, 0, 65535, MC_FM_MOD_RATIO, MC_CENTS, -700, 700)},
    /*@ fm 16 WOBBLE */ {MCL_WOBBLE, MC_T1(6, MC_IDENT, 0, 0)},
    /* colour: every modulator's pitch (ratio and fixed Hz), an octave each
     * way, no feedback (SHIMMER is the ratio ones plus feedback sizzle) */
    /*@ fm 17 COLOR */ {MCL_COLOR, MC_T1(MC_FM_MOD_PITCH, MC_CENTS, -1200, 1200)},
};

static const mc_recipe mc_gnome[] = {
    {0, MC_T1(0, MC_NONE, 0, 0)},
    /*@ gnome 1 VOWEL */ {MCL_VOWEL, MC_T1(12, MC_LIN, 0, 1024)},
    /* breath under the voice (CHANGES-1.1.7 G4): pitched aspiration and a
     * softer source, never the 1.1 noise crossfade (P14) */
    /*@ gnome 2 BREATH */ {MCL_BREATH, MC_T2(26, MC_LIN, 0, 65535, 27, MC_LIN, 0, 65535)},
    /* chatter: every note a different vowel */
    /*@ gnome 3 TALK */ {MCL_TALK, MC_T1(19, MC_LIN, 0, 128)},
    /*@ gnome 4 VIBRATO */ {MCL_VIBRATO, MC_T1(6, MC_IDENT, 0, 0)},
    /* growl: deeper amplitude modulation turning into a rattle up to 30 Hz
     * (Growl rate P25), no noise rasp (CHANGES-1.1.7 G4/G7) */
    /*@ gnome 5 GROWL */ {MCL_GROWL, MC_T2(15, MC_LIN, 0, 65535, 25, MC_LIN, 0, 300)},
    /* scoop: the note glides up into its pitch */
    /*@ gnome 6 GLIDE */ {MCL_GLIDE, MC_T1(18, MC_LIN, 0, -200)},
    /*@ gnome 7 SWIRL */ {MCL_SWIRL, MC_T1(9, MC_IDENT, 0, 0)},
    /*@ gnome 8 TAIL */ {MCL_TAIL, MC_T1(4, MC_CENTS, -2400, 2400)},
    /*@ gnome 9 ECHO */ {MCL_ECHO, MC_T1(7, MC_IDENT, 0, 0)},
    /*@ gnome 10 SPACE */ {MCL_SPACE, MC_T1(8, MC_IDENT, 0, 0)},
    /* formant size: small to big */
    /*@ gnome 11 SIZE */ {MCL_SIZE, MC_T1(13, MC_LIN, 900, -900)},
    /*@ gnome 12 SMILE */ {MCL_SMILE, MC_T1(17, MC_LIN, 0, 65535)},
};

/* Sample (SPEC/14): Bright is a low-pass below its centre and an open
 * plateau above, so BRIGHT travels the filter half only. */
static const mc_recipe mc_sample[] = {
    {0, MC_T1(0, MC_NONE, 0, 0)},
    /*@ sample 1 BRIGHT */ {MCL_BRIGHT, MC_T1(5, MC_LIN, 0, 32768)},
    /*@ sample 2 VIBRATO */ {MCL_VIBRATO, MC_T1(6, MC_IDENT, 0, 0)},
    /*@ sample 3 ECHO */ {MCL_ECHO, MC_T1(7, MC_IDENT, 0, 0)},
    /*@ sample 4 SPACE */ {MCL_SPACE, MC_T1(8, MC_IDENT, 0, 0)},
    /*@ sample 5 SWIRL */ {MCL_SWIRL, MC_T1(9, MC_IDENT, 0, 0)},
    /* a clipped hit at 0, held all the way at 65535 */
    /*@ sample 6 DECAY */
    {MCL_DECAY, MC_T2(2, MC_CENTS, -2400, 2400, 3, MC_LIN, 0, 65535)},
    /*@ sample 7 TAIL */ {MCL_TAIL, MC_T1(4, MC_CENTS, -2400, 2400)},
    /* the whole sample an octave down .. an octave up (common Tune) */
    /*@ sample 8 PITCH */ {MCL_PITCH, MC_T1(10, MC_LIN, -1200, 1200)},
    /* rotary-ish: vibrato depth and chorus rise together */
    /*@ sample 9 ROTARY */
    {MCL_SPEED, MC_T2(6, MC_LIN, 0, 65535, 9, MC_LIN, 0, 45875)},
};

#undef MC_T1
#undef MC_T2

static const mc_recipe *mc_table(unsigned engine, unsigned *n) {
  switch (engine) {
  case GC_VA:
    *n = (unsigned)(sizeof mc_va / sizeof mc_va[0]);
    return mc_va;
  case GC_FM:
    *n = (unsigned)(sizeof mc_fm / sizeof mc_fm[0]);
    return mc_fm;
  case GC_GNOME:
    *n = (unsigned)(sizeof mc_gnome / sizeof mc_gnome[0]);
    return mc_gnome;
  case GC_SAMPLE:
    *n = (unsigned)(sizeof mc_sample / sizeof mc_sample[0]);
    return mc_sample;
  default: /* Drums keep the kit's own four (hosts) */
    *n = 1u;
    return 0;
  }
}

static const unsigned char mc_byte[GC_MACRO_SLOTS] = {10u, 11u, 54u, 55u};

/* Validator hook (core.c): is id a recipe of this engine's table? */
int gc_macro_id_ok(unsigned engine, unsigned id) {
  unsigned n;
  (void)mc_table(engine, &n);
  return id < n;
}

static const mc_recipe *mc_recipe_of(const gc_patch *p, uint32_t slot) {
  unsigned n, id;
  const mc_recipe *t;
  if (!p || slot >= GC_MACRO_SLOTS || p->bytes[8] > 4u) {
    return 0;
  }
  t = mc_table(p->bytes[8], &n);
  id = p->bytes[mc_byte[slot]];
  return (id == 0u || id >= n || !t) ? 0 : &t[id];
}

static int mc_known(const gc_patch *p, uint32_t slot) {
  unsigned n;
  if (!p || slot >= GC_MACRO_SLOTS || p->bytes[8] > 4u) {
    return 0;
  }
  (void)mc_table(p->bytes[8], &n);
  return p->bytes[mc_byte[slot]] < n;
}

static int32_t mc_word(const gc_patch *p, unsigned pid) {
  return (int32_t)gc_rd16(p->bytes + GC_PATCH_P(pid));
}

/* Rounded line from v0 (at 0) to v1 (at d), evaluated at a (0..d). */
static int32_t mc_line(int32_t v0, int32_t v1, uint32_t a, uint32_t d) {
  if (d == 0u) {
    return v1;
  }
  return v0 + (int32_t)gc_R((int64_t)(v1 - v0) * (int64_t)a, d);
}

/* The two pieces (0, e0) -> (r, eb) -> (65535, e1) at amount a. */
static int32_t mc_travel(int32_t e0, int32_t eb, int32_t e1, uint32_t r,
                         uint32_t a) {
  return a <= r ? mc_line(e0, eb, a, r) : mc_line(eb, e1, a - r, 65535u - r);
}

/* Rest of a law-domain line lo..hi through point x, clamped to 0..65535. */
static uint32_t mc_where(int32_t lo, int32_t hi, int32_t x) {
  int64_t num = (int64_t)x - lo, den = (int64_t)hi - lo, r;
  if (den == 0) {
    return 0u;
  }
  if (den < 0) {
    num = -num;
    den = -den;
  }
  if (num <= 0) {
    return 0u;
  }
  if (num >= den) {
    return 65535u;
  }
  r = gc_R(num * 65535, (uint64_t)den);
  return (uint32_t)r;
}

static uint32_t mc_rest_of(const gc_patch *p, const mc_target *t) {
  switch (t->law) {
  case MC_IDENT:
    return (uint32_t)mc_word(p, t->to);
  case MC_LIN: {
    int32_t lo, hi, base;
    int sgn = 0;
    if (!gc_engine_param_range(p->bytes[8], t->to, &lo, &hi, &sgn)) {
      return 0u;
    }
    base = sgn ? gc_s16((uint16_t)mc_word(p, t->to)) : mc_word(p, t->to);
    return mc_where(t->lo, t->hi, base);
  }
  case MC_CENTS:
    return mc_where(t->lo, t->hi, 0);
  case MC_SCALE:
    return mc_where(t->lo, t->hi, 256);
  default:
    return 0u;
  }
}

/* New value of one base under law t at amount a (rest r), unclamped. */
static int32_t mc_value(const mc_target *t, int32_t base, uint32_t r,
                        uint32_t a) {
  int32_t c;
  switch (t->law) {
  case MC_IDENT:
    return (int32_t)a;
  case MC_LIN:
    return mc_travel(t->lo, base, t->hi, r, a);
  case MC_CENTS:
    c = mc_travel(t->lo, 0, t->hi, r, a);
    if (c == 0 || base <= 0) {
      return base;
    }
    return (int32_t)gc_clamp32((int64_t)gc_X((uint32_t)base, c), 0, 1 << 30);
  case MC_SCALE:
    c = mc_travel(t->lo, 256, t->hi, r, a);
    return (int32_t)gc_R((int64_t)base * c, 256u);
  default:
    return base;
  }
}

/* FM operator groups: one write per matching operator word. */
static uint32_t mc_fm_group(const gc_patch *p, const mc_target *t, uint32_t r,
                            uint32_t a, gc_macro_write *out, uint32_t n) {
  uint32_t carr = gc_fm_carrier_mask(mc_word(p, 12) & 7u);
  unsigned j;
  for (j = 0; j < 6u; j++) {
    unsigned base = 18u + 5u * j, car = (carr >> j) & 1u, pid;
    int32_t w, v, keep;
    if (t->to == MC_FM_CAR_DECAY) {
      if (!car) {
        continue;
      }
      pid = base + 3u;
      w = mc_word(p, pid);
      keep = w & 0xc000;
      v = gc_clampi(mc_value(t, w & 0x3fff, r, a), 0, 10000);
    } else if (t->to == MC_FM_PUNCH) {
      if (car || (mc_word(p, base + 3u) & 0x3fff) > 150) {
        continue;
      }
      pid = base + 1u;
      keep = 0;
      v = gc_clampi(mc_value(t, mc_word(p, pid), r, a), 0, 65535);
    } else if (t->to == MC_FM_MOD_LEVEL || t->to == MC_FM_BODY_LEVEL) {
      if (car || (t->to == MC_FM_BODY_LEVEL &&
                  (mc_word(p, base + 3u) & 0x3fff) <= 150)) {
        continue;
      }
      pid = base + 1u;
      keep = 0;
      v = gc_clampi(mc_value(t, mc_word(p, pid), r, a), 0, 65535);
    } else if (t->to == MC_FM_MOD_ATTACK || t->to == MC_FM_CAR_ATTACK) {
      if (car != (t->to == MC_FM_CAR_ATTACK)) {
        continue;
      }
      pid = base + 2u;
      w = mc_word(p, pid);
      keep = w & 0xe000;
      v = gc_clampi(mc_value(t, w & 0x1fff, r, a), 0, 5000);
    } else if (t->to == MC_FM_MOD_DECAY) {
      if (car) {
        continue;
      }
      pid = base + 3u;
      w = mc_word(p, pid);
      keep = w & 0xc000;
      v = gc_clampi(mc_value(t, w & 0x3fff, r, a), 0, 10000);
    } else { /* MC_FM_MOD_RATIO (ratio-mode modulators) / MC_FM_MOD_PITCH
              * (every modulator): the pitch code */
      int32_t code;
      if (car || (t->to == MC_FM_MOD_RATIO && (mc_word(p, base + 2u) & 0x8000))) {
        continue;
      }
      pid = base;
      w = mc_word(p, pid);
      keep = w & 0xe000;
      code = (w & 0x1fff) ? (w & 0x1fff) : 8192;
      v = gc_clampi(mc_value(t, code, r, a), 1, 8192) & 0x1fff;
    }
    if (n < GC_MACRO_WRITES) {
      out[n].parameter = (uint16_t)pid;
      out[n].value = keep | v;
      n++;
    }
  }
  return n;
}

static uint32_t mc_put(const gc_patch *p, const mc_target *t, uint32_t r,
                       uint32_t a, gc_macro_write *out, uint32_t n) {
  int32_t lo, hi, base, v;
  int sgn = 0;
  if (t->law == MC_NONE) {
    return n;
  }
  if (t->to >= 64u) {
    return mc_fm_group(p, t, r, a, out, n);
  }
  if (!gc_engine_param_range(p->bytes[8], t->to, &lo, &hi, &sgn) ||
      n >= GC_MACRO_WRITES) {
    return n;
  }
  if (t->to == 2u || t->to == 4u) {
    hi = 10000; /* envelope times (SPEC/01 P2, P4) */
  }
  base = sgn ? gc_s16((uint16_t)mc_word(p, t->to)) : mc_word(p, t->to);
  v = gc_clampi(mc_value(t, base, r, a), lo, hi);
  out[n].parameter = t->to;
  out[n].value = v;
  return n + 1u;
}

uint32_t gc_macro_rest(const gc_patch *p, uint32_t slot) {
  const mc_recipe *m;
  if (!mc_known(p, slot)) {
    return 0u;
  }
  m = mc_recipe_of(p, slot);
  if (!m) {
    return (uint32_t)mc_word(p, 5u + slot);
  }
  return mc_rest_of(p, &m->t[0]);
}

uint32_t gc_macro_apply(const gc_patch *p, uint32_t slot, uint32_t amount,
                        gc_macro_write out[GC_MACRO_WRITES]) {
  const mc_recipe *m;
  uint32_t r, n = 0, i, k = 0;
  gc_macro_write tmp[GC_MACRO_WRITES];
  if (!out || !mc_known(p, slot)) {
    return 0u;
  }
  amount = amount > 65535u ? 65535u : amount;
  m = mc_recipe_of(p, slot);
  if (!m) { /* default: the 1.2 knob, P5+slot */
    out[0].parameter = (uint16_t)(5u + slot);
    out[0].value = (int32_t)amount;
    return 1u;
  }
  r = mc_rest_of(p, &m->t[0]);
  n = mc_put(p, &m->t[0], r, amount, tmp, n);
  n = mc_put(p, &m->t[1], r, amount, tmp, n);
  /* record-group writes (P10..P47) first, contiguous, then streaming ones */
  for (i = 0; i < n; i++) {
    if (tmp[i].parameter >= 10u) {
      out[k++] = tmp[i];
    }
  }
  for (i = 0; i < n; i++) {
    if (tmp[i].parameter < 10u) {
      out[k++] = tmp[i];
    }
  }
  return k;
}

uint32_t gc_macro_label_id(const gc_patch *p, uint32_t slot) {
  const mc_recipe *m;
  if (!mc_known(p, slot)) {
    return GC_NONE;
  }
  m = mc_recipe_of(p, slot);
  return m ? m->label : slot; /* defaults: TONE, MOTION, ECHO, SPACE */
}

const char *gc_macro_label_text(uint32_t label_id) {
  return label_id < GC_MACRO_LABELS ? mc_labels[label_id] : "";
}

const char *gc_macro_label(const gc_patch *p, uint32_t slot) {
  return gc_macro_label_text(gc_macro_label_id(p, slot));
}
