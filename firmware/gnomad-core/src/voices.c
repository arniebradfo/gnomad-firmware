/* Voice allocation, note lifecycle, choke, ALL_OFF/PANIC and parameter
 * execution (SPEC/01 "Voice policy and macros", D04/D05/D07; SPEC/12 choke). */
#include "gc_internal.h"
#define GC_TABLES_DEFINE_VOICES
#include "tables.h"

void gc_ramp_set(gc_ramp *r, uint16_t target) {
  r->start = r->cur; /* a new target during a ramp starts from current */
  r->target = target;
  r->k = 0;
}

void gc_ramp_jump(gc_ramp *r, uint16_t value) {
  r->start = value;
  r->target = value;
  r->cur = value;
  r->k = 64;
}

uint32_t gc_voices_active(const gc_core *c) {
  uint32_t n = 0;
  unsigned i;
  for (i = 0; i < GC_VOICES; i++) {
    n += (c->voice[i].h.flags & GC_VF_ACTIVE) ? 1u : 0u;
  }
  return n;
}

static void sat_inc(uint32_t *x) {
  if (*x != 0xffffffffu) {
    (*x)++;
  }
}

static uint16_t gc_voice_gain(const gc_voice *v) {
  if (v->h.engine == GC_DRUMS) {
    return gc_drums_amp(v);
  }
  if (v->h.engine == GC_FM) {
    return gc_fm_amp(v);
  }
  if (v->h.engine == GC_GNOME) {
    return gc_gnome_amp(v);
  }
  if (v->h.engine == GC_SAMPLE) {
    return gc_sample_amp(v);
  }
  return gc_va_amp(v);
}

/* D05: lowest idle slot; else victims on the incoming track if any exist,
 * else all; quietest releasing (ties oldest start, then slot), else oldest
 * held (ties slot). "Releasing" = no longer holding a note ID, except a drum
 * one-shot whose natural lifetime T has not ended (SPEC/12 N1: NOTE_OFF only
 * detaches the ID; choke/ALL_OFF/PANIC/forced steal still cut). */
static int voice_releasing(const gc_voice *v) {
  if (v->h.note_id != 0u) {
    return 0;
  }
  if (v->h.engine == GC_DRUMS && !(v->h.flags & GC_VF_FORCED) &&
      v->u.dr.t < v->u.dr.T) {
    return 0; /* still sounding through T */
  }
  return 1;
}

static unsigned choose_slot(const gc_core *c, uint8_t track, int *stolen) {
  unsigned i, best = GC_VOICES;
  int same = 0, best_rel = 0;
  uint16_t best_amp = 0;
  *stolen = 0;
  for (i = 0; i < GC_VOICES; i++) {
    if (!(c->voice[i].h.flags & GC_VF_ACTIVE)) {
      return i;
    }
    if (c->voice[i].h.track == track) {
      same = 1;
    }
  }
  *stolen = 1;
  for (i = 0; i < GC_VOICES; i++) {
    const gc_voice *v = &c->voice[i];
    int rel = voice_releasing(v);
    uint16_t amp;
    if (same && v->h.track != track) {
      continue;
    }
    amp = rel ? gc_voice_gain(v) : 0u;
    if (best == GC_VOICES) {
      best = i;
      best_rel = rel;
      best_amp = amp;
      continue;
    }
    if (rel != best_rel) {
      if (rel) {
        best = i;
        best_rel = 1;
        best_amp = amp;
      }
      continue;
    }
    if (rel && amp != best_amp) {
      if (amp < best_amp) {
        best = i;
        best_amp = amp;
      }
      continue;
    }
    if (v->h.start < c->voice[best].h.start) {
      best = i;
      best_amp = amp;
    }
  }
  return best;
}

static gc_voice *find_holder(gc_core *c, uint8_t track, uint32_t id) {
  unsigned i;
  for (i = 0; i < GC_VOICES; i++) {
    gc_voice *v = &c->voice[i];
    if ((v->h.flags & GC_VF_ACTIVE) && v->h.track == track && v->h.note_id == id) {
      return v;
    }
  }
  return 0;
}

/* Pitched holder enters release; a drum holder only loses its ID (N1). */
static void release_holder(gc_voice *v) {
  if (v->h.engine == GC_FM) {
    gc_fm_note_off(v);
  } else if (v->h.engine == GC_GNOME) {
    gc_gnome_note_off(v);
  } else if (v->h.engine == GC_SAMPLE) {
    gc_sample_note_off(v);
  } else if (v->h.engine != GC_DRUMS) {
    gc_va_note_off(v);
  }
  v->h.note_id = 0;
}

/* 32-sample forced release, restarting from the current forced gain. */
static void force_release(gc_voice *v) {
  uint16_t g0 = 65535u;
  if (v->h.flags & GC_VF_FORCED) {
    g0 = (uint16_t)(v->h.forced_g0 -
                    gc_Rp2((int64_t)v->h.forced_g0 * v->h.forced_k, 5u)); /* L(g0,0,k,32) */
  }
  v->h.forced_g0 = g0;
  v->h.forced_k = 0;
  v->h.flags |= GC_VF_FORCED;
  v->h.note_id = 0;
}

void gc_ev_note_off(gc_core *c, uint8_t track, uint32_t id) {
  gc_voice *v = find_holder(c, track, id);
  if (v) {
    release_holder(v); /* absent or stolen IDs are a no-op */
  }
}

void gc_ev_note_on(gc_core *c, uint8_t track, uint32_t id, uint8_t key,
                   uint8_t vel) {
  const uint8_t *patch = c->patch[track];
  uint8_t engine = patch[8];
  unsigned slot, i;
  int stolen;
  gc_voice *v;
  uint32_t seed;
  uint8_t choke = 0;
  if (vel == 0u) {
    gc_ev_note_off(c, track, id);
    return;
  }
  v = find_holder(c, track, id);
  if (v) {
    release_holder(v); /* N3: reuse detaches the older holder */
  }
  if (engine == GC_DRUMS) {
    choke = c->kit[track][32u + 24u * key + 1u];
    if (choke != 0u) {
      for (i = 0; i < GC_VOICES; i++) {
        gc_voice *o = &c->voice[i];
        if ((o->h.flags & GC_VF_ACTIVE) && o->h.track == track &&
            o->h.engine == GC_DRUMS && o->u.dr.choke == choke) {
          force_release(o);
          sat_inc(&c->last.chokes);
        }
      }
    }
  }
  slot = choose_slot(c, track, &stolen);
  v = &c->voice[slot];
  if (stolen) {
    sat_inc(&c->last.steals);
  }
  {
    int16_t old = v->h.prev_sample;
    gc_zero(&v->h, sizeof(v->h));
    v->h.xf_old = old;
    v->h.flags = GC_VF_ACTIVE | GC_VF_ATTACHED | (stolen ? GC_VF_XFADE : 0);
  }
  c->serial++;
  seed = c->seed ^ (c->serial * 0x9e3779b9u) ^ ((uint32_t)track * 0x85ebca6bu) ^
         (uint32_t)key;
  v->h.rng = seed ? seed : 1u;
  v->h.start = c->frame;
  v->h.note_id = id;
  v->h.engine = engine;
  v->h.track = track;
  v->h.key = key;
  v->h.vel = vel;
  v->h.vel_gain = (uint16_t)gc_R((int64_t)vel * 65535, 127u);
  for (i = 0; i < GC_SNAP_WORDS; i++) {
    v->p[i] = gc_rd16(patch + GC_PATCH_P(i));
  }
  if (engine == GC_DRUMS) {
    gc_drums_note_on(c, v, c->kit[track] + 32u + 24u * key);
  } else if (engine == GC_FM) {
    gc_fm_note_on(c, v);
  } else if (engine == GC_GNOME) {
    gc_gnome_note_on(c, v);
  } else if (engine == GC_SAMPLE) {
    gc_sample_note_on(c, v);
  } else {
    gc_va_note_on(c, v);
  }
}

static int targeted(const gc_voice *v, uint8_t track) {
  return (v->h.flags & GC_VF_ACTIVE) && (track == 255u || v->h.track == track);
}

void gc_ev_all_off(gc_core *c, uint8_t track) {
  unsigned i;
  for (i = 0; i < GC_VOICES; i++) {
    gc_voice *v = &c->voice[i];
    if (!targeted(v, track)) {
      continue;
    }
    if (v->h.engine == GC_DRUMS) {
      force_release(v);
    } else {
      release_holder(v);
    }
  }
}

void gc_ev_panic(gc_core *c, uint8_t track) {
  unsigned i;
  for (i = 0; i < GC_VOICES; i++) {
    gc_voice *v = &c->voice[i];
    if (targeted(v, track)) {
      v->h.flags = 0;
      v->h.note_id = 0;
      v->h.prev_sample = 0;
    }
  }
  if (track == 255u) {
    gc_fx_panic(c);
    gc_master_clear(c);
  }
}

/* Freeze current Level/macros/Chorus into each old voice's snapshot and
 * detach it from live edits (SPEC/01 bind rules). */
void gc_detach_track(gc_core *c, uint8_t track) {
  unsigned i;
  for (i = 0; i < GC_VOICES; i++) {
    gc_voice *v = &c->voice[i];
    if ((v->h.flags & GC_VF_ACTIVE) && v->h.track == track &&
        (v->h.flags & GC_VF_ATTACHED)) {
      v->p[0] = c->tramp[track][GC_TR_LEVEL].cur;
      v->p[5] = c->tramp[track][GC_TR_BRIGHT].cur;
      v->p[6] = c->tramp[track][GC_TR_WOBBLE].cur;
      v->p[7] = c->tramp[track][GC_TR_ECHO].cur;
      v->p[8] = c->tramp[track][GC_TR_SPACE].cur;
      v->p[9] = c->tramp[track][GC_TR_CHORUS].cur;
      v->h.flags &= (uint8_t)~GC_VF_ATTACHED;
    }
  }
}

/* Execute one prevalidated PARAMETER event. */
void gc_ev_parameter(gc_core *c, uint8_t track, uint16_t pid, int32_t value) {
  unsigned i;
  if (track == 255u) {
    unsigned m = pid - 0x300u;
    if (master_ramp_of[m] != 255u) {
      gc_ramp_set(&c->mramp[master_ramp_of[m]], (uint16_t)value);
      c->ramp_busy |= GC_RAMPS_MASTER;
    } else {
      c->mstep[master_step_of[m]] = (uint16_t)value;
    }
    return;
  }
  if (pid == 0x100u) {
    gc_ramp_set(&c->tramp[track][GC_TR_GAIN], (uint16_t)value);
    c->ramp_busy |= (uint8_t)(1u << track);
    return;
  }
  if (pid >= 0x200u) {
    unsigned pad = (pid - 0x200u) >> 4, f = pid & 15u;
    uint8_t *p = c->kit[track] + 32u + 24u * pad + kit_field_off[f];
    if (f == 5u) {
      *p = (uint8_t)value;
    } else {
      gc_wr16(p, (uint16_t)(value & 0xffff));
    }
    c->dirty_kit |= (uint8_t)(1u << track);
    return; /* kit edits affect future hits only */
  }
  gc_wr16(c->patch[track] + GC_PATCH_P(pid), (uint16_t)(value & 0xffff));
  c->dirty_patch |= (uint8_t)(1u << track);
  if (pid == 0u) {
    gc_ramp_set(&c->tramp[track][GC_TR_LEVEL], (uint16_t)value);
    c->ramp_busy |= (uint8_t)(1u << track);
    return;
  }
  if (pid >= 5u && pid <= 9u) {
    gc_ramp_set(&c->tramp[track][GC_TR_BRIGHT + (pid - 5u)], (uint16_t)value);
    c->ramp_busy |= (uint8_t)(1u << track);
    return;
  }
  {
    uint8_t eng = c->patch[track][8];
    int live = (eng == GC_VA && gc_va_param_live(pid)) ||
               (eng == GC_FM && gc_fm_param_live(pid)) ||
               (eng == GC_GNOME && gc_gnome_param_live(pid)) ||
               (eng == GC_SAMPLE && gc_sample_param_live(pid));
    if (!live) {
      return; /* structural / next-note fields */
    }
    for (i = 0; i < GC_VOICES; i++) {
      gc_voice *v = &c->voice[i];
      if ((v->h.flags & (GC_VF_ACTIVE | GC_VF_ATTACHED)) ==
              (GC_VF_ACTIVE | GC_VF_ATTACHED) &&
          v->h.track == track && v->h.engine == eng) {
        if (eng == GC_VA && pid == 10u) {
          gc_va_tune_edit(v, (uint16_t)(value & 0xffff));
        } else if (eng == GC_GNOME && pid == 10u) {
          gc_gnome_tune_edit(v, (uint16_t)(value & 0xffff));
        }
        v->p[pid] = (uint16_t)(value & 0xffff);
        if (eng == GC_FM) {
          gc_fm_live(v, pid);
        } else if (eng == GC_GNOME) {
          gc_gnome_live(v, pid);
        }
      }
    }
  }
}
