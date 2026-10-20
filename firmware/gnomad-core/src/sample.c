/* Gnomad Sample, engine 4 (SPEC/14-sample.md, revision 1.1.1).
 *
 * Resident host-provided PCM16 assets (SPEC/01 gc_sample_asset), linear
 * interpolation on a 48.16 position, one-shot / loop / sixteen manual slices,
 * a darkening-only SVF below Bright centre, ADSR, velocity and Level. The
 * playback model follows the public sampled-tone description of J. O. Smith,
 * Spectral Audio Signal Processing, "Wavetable Synthesis",
 * https://mail.dsprelated.com/freebooks/sasp/Wavetable_Synthesis.html ;
 * the implementation is the SPEC/14 recurrence in SPEC/02 integer math. */
#include "gc_internal.h"
#define GC_TABLES_DEFINE_SAMPLE
#include "tables.h"

#define SVF_Q_RES0 2147483647 /* gc_svf_q(0): damping 2 capped (SPEC/02) */

int gc_sample_param_range(unsigned pid, int32_t *lo, int32_t *hi, int *sgn) {
  *sgn = pid == 10u;
  if (pid == 11u || pid >= 48u) {
    return 0;
  }
  if (pid < 16u) {
    *lo = sm_lo[pid];
    *hi = sm_hi[pid];
  } else {
    *lo = 0; /* Start 0..65534 (even words), End 0..65535 (odd words); */
    *hi = (pid & 1u) ? 65535 : 65534; /* activity rules in gc_sample_shape */
  }
  return 1;
}

/* Cross-word shape rule for word `pid` of a Sample record whose earlier
 * words are already valid (SPEC/14 Patch and asset contract). Streams in
 * word order, so the first failure is the lowest offending offset. Returns 0
 * or GC_E_FORMAT ("malformed range structure", SPEC/01 error table). */
gc_status gc_sample_shape(const uint8_t *rec, unsigned pid) {
  unsigned count, k;
  uint16_t w = gc_rd16(rec + GC_PATCH_P(pid));
  if (pid == 15u) { /* Modes 0/1 require count 1 */
    return (gc_rd16(rec + GC_PATCH_P(13)) < 2u && w != 1u) ? GC_E_FORMAT : GC_OK;
  }
  if (pid < 16u) {
    return GC_OK;
  }
  count = gc_rd16(rec + GC_PATCH_P(15));
  k = (pid - 16u) >> 1;
  if (k >= count) {
    return w ? GC_E_FORMAT : GC_OK; /* inactive pairs are zero */
  }
  if ((pid & 1u) == 0u) { /* Start: nondecreasing in slice order */
    return (k > 0u && w < gc_rd16(rec + GC_PATCH_P(pid - 2u))) ? GC_E_FORMAT
                                                                : GC_OK;
  }
  /* End: at least 2 frames after Start (so start < end). */
  return (uint32_t)w < (uint32_t)gc_rd16(rec + GC_PATCH_P(pid - 1u)) + 2u
             ? GC_E_FORMAT
             : GC_OK;
}

const gc_sample_asset *gc_sample_find(const gc_core *c, uint32_t id) {
  uint32_t i;
  for (i = 0; i < c->asset_count; i++) {
    if (c->assets[i].asset_id == id) {
      return &c->assets[i];
    }
  }
  return 0;
}

/* Resident-asset check of a shape-valid record (bind and record groups):
 * the asset exists and every active range ends within its frames. Returns
 * GC_OK or GC_E_ASSET, with *pid the offending word (12 or an End word). */
gc_status gc_sample_assets(const gc_core *c, const uint8_t *rec, unsigned *pid) {
  const gc_sample_asset *a = gc_sample_find(c, gc_rd16(rec + GC_PATCH_P(12)));
  unsigned k, count = gc_rd16(rec + GC_PATCH_P(15));
  *pid = 12u;
  if (!a) {
    return GC_E_ASSET;
  }
  for (k = 0; k < count; k++) {
    *pid = 17u + 2u * k;
    if (gc_rd16(rec + GC_PATCH_P(*pid)) > a->frames) {
      return GC_E_ASSET;
    }
  }
  return GC_OK;
}

/* Live words: envelope times/sustain (latched at stage entry) and Tune. */
int gc_sample_param_live(unsigned pid) {
  return (pid >= 1u && pid <= 4u) || pid == 10u;
}

static int32_t sm_read(const uint8_t *pcm, uint32_t i) {
  int32_t v = (int32_t)((uint32_t)pcm[2u * i] | ((uint32_t)pcm[2u * i + 1u] << 8));
  return v >= 32768 ? v - 65536 : v; /* signed decode by subtraction */
}

void gc_sample_note_on(gc_core *c, gc_voice *v) {
  gc_sample_state *s = &v->u.sm;
  const gc_sample_asset *a = gc_sample_find(c, v->p[12]);
  unsigned slice = 0, count = v->p[15];
  gc_zero(s, sizeof(*s));
  s->mode = (uint8_t)v->p[13];
  if (s->mode == 2u) { /* Euclidean remainder (key-36) mod count */
    int32_t r = ((int32_t)v->h.key - 36) % (int32_t)count;
    slice = (unsigned)(r < 0 ? r + (int32_t)count : r);
  }
  s->start = v->p[16u + 2u * slice];
  s->end = v->p[17u + 2u * slice];
  s->pos = (uint64_t)s->start << 16;
  s->pcm = a ? a->pcm_le : 0;
  if (!a || s->end > a->frames) {
    v->h.flags = 0; /* unreachable after bind/group validation */
    return;
  }
  gc_env_segment(&s->env, 65535u, gc_ms_frames(v->p[1]), GC_ENV_ATTACK);
  s->pkey = INT16_MIN; /* step cache empty */
  s->cB = 0xffffu;     /* filter cutoff cache empty */
}

void gc_sample_note_off(gc_voice *v) {
  gc_sample_state *s = &v->u.sm;
  if (s->env.stage != GC_ENV_RELEASE && s->env.stage != GC_ENV_IDLE) {
    gc_env_segment(&s->env, 0u, gc_ms_frames(v->p[4]), GC_ENV_RELEASE);
  }
}

uint16_t gc_sample_amp(const gc_voice *v) { return gc_env_level(&v->u.sm.env); }

/* One Sample frame. ctl = Level, Bright, Wobble, Echo, Space, Chorus. */
int32_t gc_sample_render(gc_voice *v, const uint16_t *ctl, uint32_t *sat) {
  gc_sample_state *s = &v->u.sm;
  const uint16_t *p = v->p;
  uint64_t endq = (uint64_t)s->end << 16;
  uint32_t i0, step;
  int32_t p0, p1, x, cents;
  uint16_t env;
  if (s->mode != 1u && s->pos >= endq) {
    /* Source end (CHANGES-1.1.2 C20): no read at or past End; a 32-frame
     * linear fade from the last output, L(last,0,k,32), then the slot frees. */
    s->endk++;
    x = s->last - (int32_t)gc_Rp2((int64_t)s->last * s->endk, 5u);
    if (s->endk >= 32u) {
      v->h.flags = 0;
    }
    return x;
  }
  i0 = (uint32_t)(s->pos >> 16);
  p0 = sm_read(s->pcm, i0);
  if (i0 + 1u < s->end) {
    p1 = sm_read(s->pcm, i0 + 1u);
  } else {
    p1 = s->mode == 1u ? sm_read(s->pcm, s->start) : 0; /* no sentinel read */
  }
  x = p0 + (int32_t)gc_Rp2((int64_t)(p1 - p0) * (int64_t)(s->pos & 0xffffu), 16u);
  /* Pitch: 100*(key-Root) (not for slices) + Tune + vibrato, one clamp;
   * the step is an exact cache keyed on the clamped cents (control tick). */
  if (gc_ktick(&v->h)) {
    cents = gc_s16(p[10]) + gc_Rwk(50u, ctl[2], gc_sin32(s->lfo)); /* R(.,65535*32767) */
    if (s->mode != 2u) {
      cents += 100 * ((int32_t)v->h.key - (int32_t)p[14]);
    }
    cents = gc_clampi(cents, -4800, 4800);
    if (cents != s->pkey) {
      s->pkey = (int16_t)cents;
      s->step = (uint32_t)gc_clamp32((int64_t)gc_X(65536u, cents), 4096, 1048576);
    }
    s->lfo += (uint32_t)GC_KRATE * GC_SAMPLE_LFO_INC;
  }
  step = s->step;
  /* Bright below centre darkens (resonance 0); centre and above is an exact
   * bypass with histories zeroed every frame (the labelled Open plateau).
   * The cutoff is a pure function of Bright: cached on its value. */
  if (ctl[1] < 32768u) {
    if (ctl[1] != s->cB) {
      int32_t fc = gc_Rp2s(4800 * ((int32_t)ctl[1] - 32768), 15u);
      uint16_t hz = (uint16_t)gc_clamp32((int64_t)gc_X(11000u, fc), 20, 11000);
      s->cB = ctl[1];
      if (hz != s->cHz) {
        s->cF = gc_svf_f(hz); /* exact cache keyed by integer Hz */
        s->cHz = hz;
      }
    }
    x = gc_svf_runi(s->svf, x, s->cF, SVF_Q_RES0, sat);
  } else {
    s->svf[0] = 0;
    s->svf[1] = 0;
  }
  env = gc_env_step(&s->env, p);
  {
    /* trim 1/1; G by ADSR, velocity, Level on the S18 magnitude, sign once */
    uint32_t m = gc_Gm(gc_Gm(gc_Gm(x < 0 ? (uint32_t)0 - (uint32_t)x : (uint32_t)x, env),
                             v->h.vel_gain),
                       ctl[0]);
    x = gc_S15(x < 0 ? -(int64_t)m : (int64_t)m);
  }
  s->last = (int16_t)x;
  s->pos += step;
  if (s->mode == 1u) { /* loop: unsigned modulo of the Q16 loop length */
    uint64_t lenq = (uint64_t)(s->end - s->start) << 16;
    while (s->pos >= endq) {
      s->pos -= lenq; /* at most 8 passes: step <= 16 frames, length >= 2 */
    }
  }
  if (s->env.stage == GC_ENV_IDLE) {
    v->h.flags = 0;
  }
  return x;
}

/* SPEC/01:162 complete-record validation of one Sample patch group
 * ev[first..last]: apply the targets in caller order to one 128-B scratch,
 * then stream the shape rules and the resident-asset bounds. VA/FM/Gnome
 * records have no cross-word rules, so per-event domains are equivalent. */
gc_status gc_sample_group_check(const gc_core *c, const gc_event *ev,
                                uint16_t first, uint16_t last, uint32_t *index,
                                uint32_t *detail) {
  uint8_t tmp[GC_PATCH_BYTES];
  unsigned w, pid = 0;
  gc_status st = GC_OK;
  uint16_t k;
  gc_copy(tmp, c->patch[ev[first].track], GC_PATCH_BYTES);
  for (k = first; k <= last; k++) {
    gc_wr16(tmp + GC_PATCH_P(ev[k].parameter), (uint16_t)(ev[k].value & 0xffff));
  }
  for (w = 12u; w < 48u && st == GC_OK; w++) {
    if (gc_sample_shape(tmp, w) != GC_OK) {
      st = GC_E_FORMAT;
      pid = w;
      *detail = w;
    }
  }
  if (st == GC_OK && gc_sample_assets(c, tmp, &pid) != GC_OK) {
    st = GC_E_ASSET;
    *detail = gc_rd16(tmp + GC_PATCH_P(12));
  }
  if (st != GC_OK) {
    *index = first; /* first member naming the offending word, else first */
    for (k = first; k <= last; k++) {
      if (ev[k].parameter == pid) {
        *index = k;
        break;
      }
    }
  }
  return st;
}

void gc_default_sample_patch(uint8_t *b, uint16_t asset_id, uint16_t frames) {
  unsigned i;
  gc_record_head(b, GC_PATCH_BYTES, "GCP1", GC_SAMPLE, "Init Sample");
  for (i = 0; i < 16u; i++) {
    gc_wr16(b + GC_PATCH_P(i), sample_defaults[i]);
  }
  gc_wr16(b + GC_PATCH_P(12), asset_id);
  gc_wr16(b + GC_PATCH_P(17), frames); /* End0 = asset frames */
  gc_record_seal(b, GC_PATCH_BYTES);
}

