/* Public API: capabilities, record validation, lifecycle, binding, event
 * prevalidation and the render loop (SPEC/01-api.md, revision 1.1.1). */
#include "gc_internal.h"
#define GC_TABLES_DEFINE_CORE
#include "tables.h"

/* ------------------------------------------------------------------------
 * Reports */

static void rep_fill(gc_report *r, gc_status st, uint32_t index,
                     uint32_t offset, uint32_t detail) {
  if (!r) {
    return;
  }
  gc_zero(r, sizeof(*r));
  r->status = st;
  r->index = index;
  r->offset = offset;
  r->detail = detail;
}

static void rep_copy(gc_report *d, const gc_report *s) {
  d->status = s->status;
  d->index = s->index;
  d->offset = s->offset;
  d->detail = s->detail;
  d->voices_active = s->voices_active;
  d->steals = s->steals;
  d->chokes = s->chokes;
  d->clips = s->clips;
  d->saturations = s->saturations;
  d->events_peak = s->events_peak;
}

/* Instance-level result: update the last-status report (counters kept). */
static gc_status rep_core(gc_core *c, gc_report *r, gc_status st,
                          uint32_t index, uint32_t offset, uint32_t detail) {
  c->last.status = st;
  c->last.index = index;
  c->last.offset = offset;
  c->last.detail = detail;
  c->last.voices_active = gc_voices_active(c);
  if (r) {
    rep_copy(r, &c->last);
  }
  return st;
}

static gc_status fail(gc_report *r, gc_status st, uint32_t detail) {
  rep_fill(r, st, GC_NONE, GC_NONE, detail);
  return st;
}

static int core_ok(const gc_core *c) { return c && c->magic == GC_CORE_MAGIC; }

void gc_query(gc_caps *out) {
  if (!out) {
    return;
  }
  gc_zero(out, sizeof(*out));
  out->api_version = GC_API_VERSION;
  out->profile = GC_PROFILE_FX1;
  out->sample_rate = 44100u;
  out->state_bytes = GC_STATE_BYTES;
  out->effects_bytes = GC_FX_BYTES;
  out->alignment = GC_ALIGN;
  out->voices = (uint16_t)GC_VOICES;
  out->tracks = (uint16_t)GC_TRACKS;
  out->min_frames = 32u;
  out->max_frames = 512u;
  out->max_events = (uint16_t)GC_MAX_EVENTS;
  out->max_at_offset = (uint16_t)GC_MAX_AT_OFFSET;
  out->max_samples = (uint16_t)GC_MAX_SAMPLES;
  out->engine_mask = (uint16_t)GC_ENGINES_AVAILABLE; /* exactly what binds */
  out->patch_bytes = (uint16_t)GC_PATCH_BYTES;
  out->kit_bytes = (uint16_t)GC_KIT_BYTES;
  out->max_kits = 32u;
  out->flags = 1u;
  out->max_presets = 384u;
  out->reserved = 0u;
}

/* ------------------------------------------------------------------------
 * Wire records (SPEC/01 "Wire layout") */

/* Control defaults/domains are generated from SPEC/01,10,12,20 in tables.h. */

int32_t gc_master_default(unsigned idx) { return m_def[idx]; }

static int engine_range(unsigned engine, unsigned pid, int32_t *lo, int32_t *hi,
                        int *sgn) {
  switch (engine) {
  case GC_VA:
    return gc_va_param_range(pid, lo, hi, sgn);
  case GC_DRUMS:
    return gc_drums_param_range(pid, lo, hi, sgn);
  case GC_FM:
    return gc_fm_param_range(pid, lo, hi, sgn);
  case GC_GNOME:
    return gc_gnome_param_range(pid, lo, hi, sgn);
  default:
    return gc_sample_param_range(pid, lo, hi, sgn);
  }
}

int gc_engine_param_range(unsigned engine, unsigned pid, int32_t *lo,
                          int32_t *hi, int *sgn) {
  return pid < 48u && engine <= 4u && engine_range(engine, pid, lo, hi, sgn);
}

/* Magic, version, length, CRC (in that order), then flags and name. */
static gc_status check_header(const uint8_t *b, uint32_t len, const char *magic,
                              gc_report *r) {
  uint32_t i, crc;
  for (i = 0; i < 4u; i++) {
    if (b[i] != (uint8_t)magic[i]) {
      rep_fill(r, GC_E_FORMAT, GC_NONE, i, i);
      return GC_E_FORMAT;
    }
  }
  if (gc_rd16(b + 4) != 2u) {
    rep_fill(r, GC_E_VERSION, GC_NONE, 4u, 2u);
    return GC_E_VERSION;
  }
  if (gc_rd16(b + 6) != len) {
    rep_fill(r, GC_E_FORMAT, GC_NONE, 6u, len);
    return GC_E_FORMAT;
  }
  crc = gc_crc32c(b, len, 1);
  if (crc != gc_rd32(b + 12)) {
    rep_fill(r, GC_E_CRC, GC_NONE, 12u, crc);
    return GC_E_CRC;
  }
  return GC_OK;
}

/* Kits: flags 10..11 zero. Patches: bytes 10..11 are macro slots 0 and 1
 * (SPEC/CHANGES-1.3.0-macros), recipe IDs of the record's engine. */
static gc_status check_flags_name(const uint8_t *b, gc_report *r,
                                  int patch) {
  uint32_t i = 0;
  if (patch ? (!gc_macro_id_ok(b[8], b[10]) || !gc_macro_id_ok(b[8], b[11]))
            : (b[10] != 0u || b[11] != 0u)) {
    i = (patch ? !gc_macro_id_ok(b[8], b[10]) : b[10] != 0u) ? 10u : 11u;
    rep_fill(r, GC_E_FORMAT, GC_NONE, i, i);
    return GC_E_FORMAT;
  }
  i = 0;
  while (i < 15u && b[16 + i] >= 0x20u && b[16 + i] <= 0x7eu) {
    i++;
  }
  if (i == 0u) {
    rep_fill(r, GC_E_FORMAT, GC_NONE, 16u, 16u);
    return GC_E_FORMAT;
  }
  for (; i < 16u; i++) {
    if (b[16 + i] != 0u) {
      rep_fill(r, GC_E_FORMAT, GC_NONE, 16u + i, 16u + i);
      return GC_E_FORMAT;
    }
  }
  return GC_OK;
}

static gc_status validate_patch_bytes(const uint8_t *b, gc_report *r) {
  gc_status st = check_header(b, GC_PATCH_BYTES, "GCP1", r);
  unsigned i, engine;
  if (st != GC_OK) {
    return st;
  }
  engine = b[8];
  if (engine > 4u || !((GC_ENGINES_AVAILABLE >> engine) & 1u)) {
    rep_fill(r, GC_E_FORMAT, GC_NONE, 8u, 8u); /* not an engine of this build */
    return GC_E_FORMAT;
  }
  if (b[9] > 7u) {
    rep_fill(r, GC_E_FORMAT, GC_NONE, 9u, 9u);
    return GC_E_FORMAT;
  }
  st = check_flags_name(b, r, 1);
  if (st != GC_OK) {
    return st;
  }
  for (i = 0; i < 48u; i++) {
    uint16_t raw = gc_rd16(b + GC_PATCH_P(i));
    int32_t lo, hi, v;
    int sgn;
    if (i == 11u) { /* P11: macro slots 2 (low byte) and 3 (high byte) */
      if (!gc_macro_id_ok(engine, raw & 0xffu) ||
          !gc_macro_id_ok(engine, raw >> 8)) {
        rep_fill(r, GC_E_FORMAT, GC_NONE, GC_PATCH_P(i), i);
        return GC_E_FORMAT;
      }
    } else if (engine == GC_DRUMS && i >= 1u && i <= 4u) {
      if (raw != drum_fixed[i - 1u]) {
        rep_fill(r, GC_E_RANGE, GC_NONE, GC_PATCH_P(i), i);
        return GC_E_RANGE;
      }
    } else if (engine == GC_FM && i >= 1u && i <= 3u) {
      /* P1=1, P2=180, P3=49152 unused fixed (SPEC/11). */
      if (raw != drum_fixed[i - 1u]) {
        rep_fill(r, GC_E_RANGE, GC_NONE, GC_PATCH_P(i), i);
        return GC_E_RANGE;
      }
    } else if (engine == GC_DRUMS && i == 10u) {
      if (raw != 0u) {
        rep_fill(r, GC_E_RANGE, GC_NONE, GC_PATCH_P(i), i);
        return GC_E_RANGE;
      }
    } else if (engine_range(engine, i, &lo, &hi, &sgn)) {
      v = sgn ? gc_s16(raw) : (int32_t)raw;
      if (v < lo || v > hi) {
        rep_fill(r, GC_E_RANGE, GC_NONE, GC_PATCH_P(i), i);
        return GC_E_RANGE;
      }
      if (engine == GC_FM && !gc_fm_word_ok(i, v)) {
        rep_fill(r, GC_E_RANGE, GC_NONE, GC_PATCH_P(i), i);
        return GC_E_RANGE;
      }
      if (engine == GC_SAMPLE && gc_sample_shape(b, i) != GC_OK) {
        rep_fill(r, GC_E_FORMAT, GC_NONE, GC_PATCH_P(i), i);
        return GC_E_FORMAT;
      }
    } else if (raw != 0u) {
      rep_fill(r, GC_E_FORMAT, GC_NONE, GC_PATCH_P(i), i); /* reserved word */
      return GC_E_FORMAT;
    }
  }
  rep_fill(r, GC_OK, GC_NONE, GC_NONE, 0u);
  return GC_OK;
}

static int kit_field_ok(unsigned field, int32_t v) {
  switch (field) {
  case 0:
    return v >= -2400 && v <= 2400;
  case 1:
    return v >= 10 && v <= 4000;
  case 5:
    return v >= 0 && v <= 15;
  default:
    return field <= 8u && v >= 0 && v <= 65535;
  }
}

static gc_status validate_kit_bytes(const uint8_t *b, gc_report *r) {
  gc_status st = check_header(b, GC_KIT_BYTES, "GCK1", r);
  unsigned p, j;
  if (st != GC_OK) {
    return st;
  }
  if (b[8] != 16u) {
    rep_fill(r, GC_E_FORMAT, GC_NONE, 8u, 16u);
    return GC_E_FORMAT;
  }
  if (b[9] > 31u) {
    rep_fill(r, GC_E_FORMAT, GC_NONE, 9u, 9u);
    return GC_E_FORMAT;
  }
  st = check_flags_name(b, r, 0);
  if (st != GC_OK) {
    return st;
  }
  for (p = 0; p < 16u; p++) {
    const uint8_t *d = b + 32u + 24u * p;
    uint32_t off = 32u + 24u * p, id = 0x200u + 16u * p;
    if (d[0] != p) {
      rep_fill(r, GC_E_FORMAT, GC_NONE, off, off);
      return GC_E_FORMAT;
    }
    if (!kit_field_ok(5u, d[1])) {
      rep_fill(r, GC_E_RANGE, GC_NONE, off + 1u, id + 5u);
      return GC_E_RANGE;
    }
    if (!kit_field_ok(0u, gc_s16(gc_rd16(d + 2)))) {
      rep_fill(r, GC_E_RANGE, GC_NONE, off + 2u, id);
      return GC_E_RANGE;
    }
    if (!kit_field_ok(1u, gc_rd16(d + 4))) {
      rep_fill(r, GC_E_RANGE, GC_NONE, off + 4u, id + 1u);
      return GC_E_RANGE;
    }
    for (j = 18u; j < 24u; j++) {
      if (d[j] != 0u) {
        rep_fill(r, GC_E_FORMAT, GC_NONE, off + j, off + j);
        return GC_E_FORMAT;
      }
    }
  }
  rep_fill(r, GC_OK, GC_NONE, GC_NONE, 0u);
  return GC_OK;
}

gc_status gc_validate_patch(const gc_patch *p, gc_report *report) {
  if (!p) {
    return fail(report, GC_E_ARGUMENT, 0u);
  }
  return validate_patch_bytes(p->bytes, report);
}

gc_status gc_validate_kit(const gc_kit *k, gc_report *report) {
  if (!k) {
    return fail(report, GC_E_ARGUMENT, 0u);
  }
  return validate_kit_bytes(k->bytes, report);
}

/* ------------------------------------------------------------------------
 * Default records (SPEC/10 and SPEC/12 defaults) */

void gc_record_seal(uint8_t *rec, uint32_t len) {
  uint32_t crc = gc_crc32c(rec, len, 1);
  rec[12] = (uint8_t)(crc & 0xffu);
  rec[13] = (uint8_t)((crc >> 8) & 0xffu);
  rec[14] = (uint8_t)((crc >> 16) & 0xffu);
  rec[15] = (uint8_t)(crc >> 24);
}

void gc_record_head(uint8_t *b, uint32_t len, const char *magic, uint8_t b8,
                    const char *name) {
  uint32_t i;
  gc_zero(b, len);
  for (i = 0; i < 4u; i++) {
    b[i] = (uint8_t)magic[i];
  }
  gc_wr16(b + 4, 2u);
  gc_wr16(b + 6, (uint16_t)len);
  b[8] = b8;
  for (i = 0; name[i] != '\0'; i++) {
    b[16 + i] = (uint8_t)name[i];
  }
}

void gc_default_va_patch(uint8_t *b) {
  unsigned i;
  gc_record_head(b, GC_PATCH_BYTES, "GCP1", GC_VA, "Init VA");
  for (i = 0; i < 22u; i++) {
    gc_wr16(b + GC_PATCH_P(i), va_defaults[i]);
  }
  gc_record_seal(b, GC_PATCH_BYTES);
}

void gc_default_drum_patch(uint8_t *b) {
  unsigned i;
  gc_record_head(b, GC_PATCH_BYTES, "GCP1", GC_DRUMS, "Init Drums");
  for (i = 0; i < 10u; i++) {
    gc_wr16(b + GC_PATCH_P(i), drum_defaults[i]);
  }
  gc_record_seal(b, GC_PATCH_BYTES);
}

void gc_default_kit(uint8_t *b) {
  unsigned p;
  gc_record_head(b, GC_KIT_BYTES, "GCK1", 16u, "Init Kit");
  for (p = 0; p < 16u; p++) {
    uint8_t *d = b + 32u + 24u * p;
    d[0] = (uint8_t)p;
    d[1] = gc_drum_desc[p].choke;
    gc_wr16(d + 4, gc_drum_desc[p].decay_ms);
    gc_wr16(d + 6, 49152u);
    gc_wr16(d + 8, 32768u);
    /* SPEC/12 rev 2.0: the hats default to the noise colour */
    gc_wr16(d + 10, (gc_drum_desc[p].recipe & 0x80u) ? 65535u : 32768u);
    gc_wr16(d + 12, 8192u);
    gc_wr16(d + 14, 8192u);
  }
  gc_record_seal(b, GC_KIT_BYTES);
}

/* ------------------------------------------------------------------------
 * Lifecycle */

static void track_controls_from_patch(gc_core *c, unsigned t) {
  unsigned i;
  c->ramp_busy |= (uint8_t)(1u << t); /* jumped: the next frame refreshes tctl */
  gc_ramp_jump(&c->tramp[t][GC_TR_LEVEL], gc_rd16(c->patch[t] + GC_PATCH_P(0)));
  for (i = 0; i < 5u; i++) {
    gc_ramp_jump(&c->tramp[t][GC_TR_BRIGHT + i],
                 gc_rd16(c->patch[t] + GC_PATCH_P(5u + i)));
  }
}

static void master_set(gc_core *c, unsigned m, uint16_t v) {
  if (master_ramp_of[m] != 255u) {
    gc_ramp_jump(&c->mramp[master_ramp_of[m]], v);
  } else {
    c->mstep[master_step_of[m]] = v;
  }
}

/* Dynamic state to its post-init/post-reset values; records retained. */
static void core_setup(gc_core *c, uint32_t seed) {
  unsigned t, m;
  c->magic = GC_CORE_MAGIC;
  c->seed = seed ? seed : GC_DEFAULT_SEED;
  c->serial = 0;
  c->rendered = 0;
  c->dirty_patch = 0;
  c->dirty_kit = 0;
  c->ramp_busy = GC_RAMPS_ALL; /* conservative: the first frame rescans all */
  c->mkc = 0;       /* master SVF coefficient cache empty */
  c->frame = 0;
  rep_fill(&c->last, GC_OK, GC_NONE, GC_NONE, 0u);
  gc_zero(c->voice, sizeof(c->voice));
  for (t = 0; t < GC_TRACKS; t++) {
    track_controls_from_patch(c, t);
    gc_ramp_jump(&c->tramp[t][GC_TR_GAIN], 65535u);
  }
  for (m = 0; m < 10u; m++) {
    master_set(c, m, m_def[m]);
  }
  gc_master_clear(c);
  gc_fx_reset(c);
}

static int overlap(const void *a, size_t an, const void *b, size_t bn) {
  uintptr_t pa = (uintptr_t)a, pb = (uintptr_t)b;
  /* Address differences avoid comparisons between unrelated objects and
   * avoid overflowing end-address arithmetic. */
  return pa <= pb ? pb - pa < an : pa - pb < bn;
}

gc_status gc_init(const gc_config *cfg, gc_core **out, gc_report *report) {
  gc_core *c;
  uint32_t i, j;
  if (!out) {
    return fail(report, GC_E_ARGUMENT, 0u);
  }
  *out = 0;
  if (!cfg || !cfg->state || !cfg->effects) {
    return fail(report, GC_E_ARGUMENT, 0u);
  }
  if (cfg->api_version != GC_API_VERSION) {
    return fail(report, GC_E_VERSION, GC_API_VERSION);
  }
  if (cfg->profile != GC_PROFILE_FX1) {
    return fail(report, GC_E_VERSION, GC_PROFILE_FX1);
  }
  if (cfg->state_bytes < GC_STATE_BYTES) {
    return fail(report, GC_E_ARENA, GC_STATE_BYTES);
  }
  if (cfg->effects_bytes != GC_FX_BYTES) {
    return fail(report, GC_E_ARENA, GC_FX_BYTES);
  }
  if (((uintptr_t)cfg->state % GC_ALIGN) != 0u ||
      ((uintptr_t)cfg->effects % GC_ALIGN) != 0u) {
    return fail(report, GC_E_ARENA, GC_ALIGN);
  }
  if (overlap(cfg->state, cfg->state_bytes, cfg->effects, cfg->effects_bytes)) {
    return fail(report, GC_E_ARENA, GC_STATE_BYTES);
  }
  if (cfg->asset_count > GC_MAX_SAMPLES) {
    return fail(report, GC_E_CAPACITY, GC_MAX_SAMPLES);
  }
  if (cfg->asset_count > 0u && !cfg->assets) {
    return fail(report, GC_E_ARGUMENT, 0u);
  }
  for (i = 0; i < cfg->asset_count; i++) {
    const gc_sample_asset *a = &cfg->assets[i];
    uint32_t crc;
    int bad = a->asset_id == 0u || a->asset_id > 65535u || a->frames == 0u ||
              a->frames > 65535u || !a->pcm_le;
    for (j = 0; j < i; j++) {
      bad |= cfg->assets[j].asset_id == a->asset_id;
    }
    if (bad) {
      rep_fill(report, GC_E_ASSET, i, GC_NONE, a->asset_id);
      return GC_E_ASSET;
    }
    crc = gc_crc32c(a->pcm_le, a->frames * 2u, 0);
    if (crc != a->crc32c) {
      rep_fill(report, GC_E_CRC, i, GC_NONE, crc);
      return GC_E_CRC;
    }
  }
  gc_zero(cfg->state, GC_STATE_BYTES); /* extra caller bytes are unused */
  gc_zero(cfg->effects, GC_FX_BYTES);
  c = (gc_core *)cfg->state;
  c->fx_pool = (uint8_t *)cfg->effects;
  c->assets = cfg->assets;
  c->asset_count = cfg->asset_count;
  for (i = 0; i < GC_TRACKS; i++) {
    gc_default_va_patch(c->patch[i]);
    gc_default_kit(c->kit[i]);
  }
  core_setup(c, cfg->seed);
  *out = c;
  rep_core(c, report, GC_OK, GC_NONE, GC_NONE, 0u);
  return GC_OK;
}

void gc_reset(gc_core *c, uint32_t seed) {
  if (!core_ok(c)) {
    return;
  }
  core_setup(c, seed);
}

gc_status gc_bind_track(gc_core *c, uint8_t track, const gc_patch *patch,
                        const gc_kit *kit, gc_report *report) {
  gc_report tmp;
  gc_status st;
  if (!core_ok(c)) {
    return fail(report, GC_E_ARGUMENT, 0u);
  }
  if (track >= GC_TRACKS) {
    return rep_core(c, report, GC_E_ARGUMENT, GC_NONE, GC_NONE, track);
  }
  if (!patch) {
    return rep_core(c, report, GC_E_ARGUMENT, GC_NONE, GC_NONE, 0u);
  }
  st = validate_patch_bytes(patch->bytes, &tmp);
  if (st == GC_OK && patch->bytes[8] == GC_DRUMS && !kit) {
    return rep_core(c, report, GC_E_ARGUMENT, GC_NONE, GC_NONE, 0u);
  }
  if (st == GC_OK && kit) {
    st = validate_kit_bytes(kit->bytes, &tmp);
  }
  if (st != GC_OK) {
    return rep_core(c, report, st, tmp.index, tmp.offset, tmp.detail);
  }
  if (patch->bytes[8] == GC_SAMPLE) { /* resident asset and range bounds */
    unsigned pid;
    if (gc_sample_assets(c, patch->bytes, &pid) != GC_OK) {
      return rep_core(c, report, GC_E_ASSET, GC_NONE, GC_PATCH_P(pid),
                      gc_rd16(patch->bytes + GC_PATCH_P(12)));
    }
  }
  /* Whole command valid: detach sounding voices, then copy records. */
  gc_detach_track(c, track);
  gc_copy(c->patch[track], patch->bytes, GC_PATCH_BYTES);
  if (kit) {
    gc_copy(c->kit[track], kit->bytes, GC_KIT_BYTES);
  }
  c->dirty_patch &= (uint8_t)~(1u << track);
  c->dirty_kit &= (uint8_t)~(1u << track);
  track_controls_from_patch(c, track);
  return rep_core(c, report, GC_OK, GC_NONE, GC_NONE, 0u);
}

gc_status gc_set_initial(gc_core *c, const int32_t master[10],
                         const uint16_t gains[4], gc_report *report) {
  unsigned i;
  if (!core_ok(c)) {
    return fail(report, GC_E_ARGUMENT, 0u);
  }
  if (!master || !gains) {
    return rep_core(c, report, GC_E_ARGUMENT, GC_NONE, GC_NONE, 0u);
  }
  if (c->rendered || c->frame != 0u || gc_voices_active(c) != 0u) {
    return rep_core(c, report, GC_E_BUSY, GC_NONE, GC_NONE, 0u);
  }
  for (i = 0; i < 10u; i++) {
    if (master[i] < m_lo[i] || master[i] > m_hi[i]) {
      return rep_core(c, report, GC_E_RANGE, GC_NONE, GC_NONE, 0x300u + i);
    }
  }
  for (i = 0; i < 10u; i++) {
    master_set(c, i, (uint16_t)master[i]);
  }
  for (i = 0; i < GC_TRACKS; i++) {
    gc_ramp_jump(&c->tramp[i][GC_TR_GAIN], gains[i]);
  }
  c->ramp_busy = GC_RAMPS_ALL;
  gc_fx_settle(c); /* echo old/target taps together: no initial crossfade */
  return rep_core(c, report, GC_OK, GC_NONE, GC_NONE, 0u);
}

void gc_get_report(const gc_core *c, gc_report *out) {
  if (!core_ok(c) || !out) {
    return;
  }
  rep_copy(out, &c->last);
}

/* ------------------------------------------------------------------------
 * Event prevalidation (SPEC/01 "Events and timing", precedence order) */

#define SEL_KEY 0x10000u
#define SEL_VELOCITY 0x10001u
#define SEL_NOTE_ID 0x10002u
#define SEL_PARAMETER 0x10003u
#define SEL_RESERVED 0x10004u
#define SEL_VALUE 0x10005u

static int param_ok(const gc_core *c, uint8_t track, unsigned pid, int32_t v) {
  int32_t lo = 0, hi = 65535;
  int sgn;
  if (track == 255u) {
    if (pid < 0x300u || pid > 0x309u) {
      return 0;
    }
    return v >= m_lo[pid - 0x300u] && v <= m_hi[pid - 0x300u];
  }
  if (pid == 0x100u) {
    return v >= 0 && v <= 65535;
  }
  if (pid >= 0x200u && pid < 0x300u) {
    return kit_field_ok(pid & 15u, v);
  }
  if (pid < 48u && engine_range(c->patch[track][8], pid, &lo, &hi, &sgn)) {
    if (v < lo || v > hi) {
      return 0;
    }
    if (c->patch[track][8] == GC_FM && !gc_fm_word_ok(pid, v)) {
      return 0;
    }
    return 1;
  }
  return 0;
}

static gc_status validate_events(gc_core *c, uint16_t frames,
                                 const gc_event *ev, uint16_t n,
                                 uint32_t *index, uint32_t *detail) {
  uint16_t i;
  unsigned same = 0;
  uint8_t groups[GC_TRACKS][2];
  uint16_t gs = 0;
  int pk = -1;
  uint8_t pt = 0;
  uint16_t po = 0;
  gc_zero(groups, sizeof(groups));
  /* Pass 1: offsets, ordering, same-offset cap, type and field checks. */
  for (i = 0; i < n; i++) {
    const gc_event *e = &ev[i];
    *index = i;
    if (e->offset >= frames) {
      *detail = (uint32_t)frames - 1u;
      return GC_E_ARGUMENT;
    }
    if (i > 0 && e->offset < ev[i - 1].offset) {
      *detail = ev[i - 1].offset;
      return GC_E_ORDER;
    }
    same = (i > 0 && e->offset == ev[i - 1].offset) ? same + 1u : 1u;
    if (same > GC_MAX_AT_OFFSET) {
      *detail = GC_MAX_AT_OFFSET;
      return GC_E_CAPACITY;
    }
    if (e->type > GC_PANIC) {
      *detail = e->type;
      return GC_E_ARGUMENT;
    }
    if (e->track >= GC_TRACKS &&
        !(e->track == 255u && e->type >= GC_PARAMETER)) {
      *detail = e->track;
      return GC_E_ARGUMENT;
    }
    *detail = e->reserved ? SEL_RESERVED : 0u;
    if (e->type == GC_NOTE_ON) {
      unsigned kmax = c->patch[e->track][8] == GC_DRUMS ? 15u : 127u;
      if (e->parameter) {
        *detail = SEL_PARAMETER;
      } else if (e->value) {
        *detail = SEL_VALUE;
      }
      if (*detail) {
        return GC_E_ARGUMENT;
      }
      if (e->key > kmax) {
        *detail = SEL_KEY;
        return GC_E_RANGE;
      }
      if (e->velocity > 127u) {
        *detail = SEL_VELOCITY;
        return GC_E_RANGE;
      }
    } else {
      if (e->type != GC_NOTE_OFF && e->note_id) {
        *detail = SEL_NOTE_ID;
      } else if (e->key) {
        *detail = SEL_KEY;
      } else if (e->velocity) {
        *detail = SEL_VELOCITY;
      } else if (e->type != GC_PARAMETER && e->parameter) {
        *detail = SEL_PARAMETER;
      } else if (e->type != GC_PARAMETER && e->value) {
        *detail = SEL_VALUE;
      }
      if (*detail) {
        return GC_E_ARGUMENT;
      }
    }
  }
  /* Pass 2: record groups (P10..P47 / kit fields, one each per track) and
   * parameter domains, streaming in event order. */
  for (i = 0; i < n; i++) {
    const gc_event *e = &ev[i];
    int kind = -1;
    *index = i;
    if (e->type == GC_PARAMETER && e->track < GC_TRACKS) {
      if (e->parameter >= 10u && e->parameter < 48u) {
        kind = 0;
      } else if (e->parameter >= 0x200u && e->parameter < 0x300u) {
        kind = 1;
      }
    }
    if (kind >= 0 && !(pk == kind && pt == e->track && po == e->offset)) {
      if (++groups[e->track][kind] > 1u) {
        *detail = 1u;
        return GC_E_CAPACITY;
      }
      gs = i;
    }
    pk = kind;
    pt = e->track;
    po = e->offset;
    if (e->type == GC_PARAMETER &&
        !param_ok(c, e->track, e->parameter, e->value)) {
      *detail = e->parameter;
      return GC_E_RANGE;
    }
    if (kind == 0 && c->patch[e->track][8] == GC_SAMPLE &&
        (i + 1u == n || ev[i + 1].type != GC_PARAMETER ||
         ev[i + 1].track != e->track || ev[i + 1].offset != e->offset ||
         ev[i + 1].parameter < 10u || ev[i + 1].parameter >= 48u)) {
      gc_status st = gc_sample_group_check(c, ev, gs, i, index, detail);
      if (st != GC_OK) {
        return st;
      }
    }
  }
  /* Pass 3: note IDs (only ID 0 is an error, N3). */
  for (i = 0; i < n; i++) {
    *index = i;
    if (ev[i].type <= GC_NOTE_OFF && ev[i].note_id == 0u) {
      *detail = 0u;
      return GC_E_NOTE_ID;
    }
  }
  return GC_OK;
}

static void exec_event(gc_core *c, const gc_event *e) {
  switch (e->type) {
  case GC_NOTE_ON:
    gc_ev_note_on(c, e->track, e->note_id, (uint8_t)e->key, (uint8_t)e->velocity);
    break;
  case GC_NOTE_OFF:
    gc_ev_note_off(c, e->track, e->note_id);
    break;
  case GC_PARAMETER:
    gc_ev_parameter(c, e->track, e->parameter, e->value);
    break;
  case GC_ALL_OFF:
    gc_ev_all_off(c, e->track);
    break;
  default:
    gc_ev_panic(c, e->track);
    break;
  }
}

static void sat_add(uint32_t *x, uint32_t d) {
  *x = (*x > 0xffffffffu - d) ? 0xffffffffu : *x + d;
}

static gc_status render_common(gc_core *c, int16_t *out, uint16_t frames,
                               const gc_event *events, uint16_t count,
                               gc_report *report) {
  uint32_t index = GC_NONE, detail = 0, sat = 0, clips = 0;
  gc_status st;
  uint16_t n, ei = 0;
  unsigned t;
  if (!core_ok(c)) {
    return fail(report, GC_E_ARGUMENT, 0u);
  }
  if (!out) {
    return rep_core(c, report, GC_E_ARGUMENT, GC_NONE, GC_NONE, 0u);
  }
  if (frames < 32u || frames > 512u) {
    return rep_core(c, report, GC_E_ARGUMENT, GC_NONE, GC_NONE,
                    frames < 32u ? 32u : 512u);
  }
  if (count > 0u && !events) {
    st = GC_E_ARGUMENT;
  } else if (count > GC_MAX_EVENTS) {
    st = GC_E_CAPACITY;
    index = GC_MAX_EVENTS;
    detail = GC_MAX_EVENTS;
  } else {
    st = validate_events(c, frames, events, count, &index, &detail);
  }
  if (st != GC_OK) {
    gc_zero(out, (uint32_t)frames * 2u); /* silence, no state advance */
    return rep_core(c, report, st, index, GC_NONE, detail);
  }
  for (n = 0; n < frames; n++) {
    while (ei < count && events[ei].offset == n) {
      exec_event(c, &events[ei]);
      ei++;
    }
    out[n] = gc_mix_frame(c, &sat, &clips);
    c->frame++;
  }
  for (t = 0; t < GC_TRACKS; t++) {
    if (c->dirty_patch & (1u << t)) {
      gc_record_seal(c->patch[t], GC_PATCH_BYTES);
    }
    if (c->dirty_kit & (1u << t)) {
      gc_record_seal(c->kit[t], GC_KIT_BYTES);
    }
  }
  c->dirty_patch = 0;
  c->dirty_kit = 0;
  c->rendered = 1;
  sat_add(&c->last.saturations, sat);
  sat_add(&c->last.clips, clips);
  if (count > c->last.events_peak) {
    c->last.events_peak = count;
  }
  return rep_core(c, report, GC_OK, GC_NONE, GC_NONE, 0u);
}

gc_status gc_render_mono(gc_core *c, int16_t *out, uint16_t frames,
                         const gc_event *events, uint16_t event_count,
                         gc_report *report) {
  return render_common(c, out, frames, events, event_count, report);
}

/* D03: advance once, write identical L/R. */
gc_status gc_render_stereo(gc_core *c, int16_t *left, int16_t *right,
                           uint16_t frames, const gc_event *events,
                           uint16_t event_count, gc_report *report) {
  gc_status st;
  uint16_t i;
  if (core_ok(c) && (!right || left == right)) {
    return rep_core(c, report, GC_E_ARGUMENT, GC_NONE, GC_NONE, 0u);
  }
  st = render_common(c, left, frames, events, event_count, report);
  if (left && right && frames >= 32u && frames <= 512u && core_ok(c)) {
    for (i = 0; i < frames; i++) {
      right[i] = left[i];
    }
  }
  return st;
}
