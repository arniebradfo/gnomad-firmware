#ifndef GNOMAD_CORE_H
#define GNOMAD_CORE_H
#include <stdint.h>
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
#define GC_API_VERSION 0x00010300u /* 1.3: gc_macro_* (SPEC/CHANGES-1.3.0-macros) */
#define GC_PROFILE_FX1 0x00010300u /* 1.3: per-sound macro maps (CHANGES-1.3.0-macros); rendering = 1.2 (msfa FM CHANGES-1.1.5, peaks drums CHANGES-1.2.0) + the SPACE plate (CHANGES-1.2.1-fx) + the ensemble chorus (CHANGES-1.2.2-ensemble) */
#define GC_STATE_BYTES 7168u
#define GC_FX_BYTES 65536u
#define GC_ALIGN 8u
#define GC_PATCH_BYTES 128u
#define GC_KIT_BYTES 416u
#define GC_MAX_EVENTS 64u
#define GC_MAX_AT_OFFSET 16u
#define GC_TRACKS 4u
#define GC_VOICES 8u
#define GC_MAX_SAMPLES 3u
#define GC_NONE 0xffffffffu

typedef struct gc_core gc_core;
typedef struct { uint8_t bytes[GC_PATCH_BYTES]; } gc_patch;
typedef struct { uint8_t bytes[GC_KIT_BYTES]; } gc_kit;
typedef enum { GC_VA=0, GC_FM=1, GC_DRUMS=2, GC_GNOME=3,
               GC_SAMPLE=4 } gc_engine;
typedef uint32_t gc_status;
enum {
 GC_OK=0, GC_E_ARGUMENT=1, GC_E_ARENA=2, GC_E_VERSION=3,
 GC_E_FORMAT=4, GC_E_CRC=5, GC_E_RANGE=6, GC_E_CAPACITY=7,
 GC_E_ORDER=8, GC_E_ASSET=9, GC_E_NOTE_ID=10, GC_E_BUSY=11
};
typedef enum { GC_NOTE_ON=0, GC_NOTE_OFF=1, GC_PARAMETER=2,
               GC_ALL_OFF=3, GC_PANIC=4 } gc_event_type;
/* Native ABI: explicitly initialized fields, not a wire structure. */
typedef struct {
 uint16_t offset;       /* 0 .. frames-1 */
 uint8_t type, track;   /* track 0..3; master target uses 255 */
 uint32_t note_id;      /* nonzero for note events, otherwise zero */
 uint16_t key;          /* ON: MIDI 0..127, drums pad 0..15 */
 uint16_t velocity;     /* ON: 0..127; all other events zero */
 uint16_t parameter;    /* PARAMETER only */
 uint16_t reserved;     /* zero */
 int32_t value;         /* decoded semantic integer; PARAMETER only; others zero */
} gc_event;
typedef struct {
 uint32_t asset_id;     /* 1..65535, unique */
 const uint8_t *pcm_le; /* immutable PCM16 little endian */
 uint32_t frames;       /* 1..65535 */
 uint32_t crc32c;       /* PCM bytes only */
} gc_sample_asset;
typedef struct {
 uint32_t api_version, profile;
 void *state; size_t state_bytes;
 void *effects; size_t effects_bytes;
 const gc_sample_asset *assets; uint32_t asset_count;
 uint32_t seed; /* zero requests default 0x474E4F4D */
} gc_config;
typedef struct {
 uint32_t api_version, profile, sample_rate;
 uint32_t state_bytes, effects_bytes, alignment;
 uint16_t voices, tracks, min_frames, max_frames;
 uint16_t max_events, max_at_offset, max_samples, engine_mask;
 uint16_t patch_bytes, kit_bytes, max_kits, flags;
 uint16_t max_presets, reserved; /* 384, zero */
} gc_caps; /* flags: bit0 dual-mono stereo; other bits zero */
typedef struct {
 uint32_t status; /* gc_status numeric code; fixed ABI width */
 uint32_t index; /* offending event/asset; GC_NONE if not applicable */
 uint32_t offset; /* offending file byte offset; GC_NONE otherwise */
 uint32_t detail; /* expected limit/version or offending parameter ID */
 uint32_t voices_active;
 uint32_t steals, chokes, clips, saturations; /* cumulative, saturating */
 uint32_t events_peak; /* highest accepted block event count */
} gc_report;
void gc_query(gc_caps *out);
gc_status gc_validate_patch(const gc_patch *p, gc_report *report);
gc_status gc_validate_kit(const gc_kit *k, gc_report *report);
gc_status gc_init(const gc_config *config, gc_core **out, gc_report *report);
void gc_reset(gc_core *core, uint32_t seed); /* retain bound patches/assets */
gc_status gc_bind_track(gc_core *core, uint8_t track,
                        const gc_patch *patch, const gc_kit *kit,
                        gc_report *report);
gc_status gc_render_mono(gc_core *core, int16_t *out, uint16_t frames,
                         const gc_event *events, uint16_t event_count,
                         gc_report *report);
gc_status gc_render_stereo(gc_core *core, int16_t *left, int16_t *right,
                           uint16_t frames, const gc_event *events,
                           uint16_t event_count, gc_report *report);
gc_status gc_set_initial(gc_core *core, const int32_t master[10],
                         const uint16_t gains[4], gc_report *report);
void gc_get_report(const gc_core *core, gc_report *out);
/* Per-sound macro maps (SPEC/CHANGES-1.3.0-macros). Host-rate helpers: they
 * read a patch record (the record as bound: loaded or saved, never one the
 * macro itself already moved) and never touch an instance. Slot 0..3 is knob
 * 1..4; amount 0..65535. A record's map is bytes 10, 11, 54, 55 (slots 0..3):
 * recipe IDs into its engine's recipe table, 0 = the slot's default (the
 * identity onto P5+slot: TONE, MOTION, ECHO, SPACE). */
#define GC_MACRO_SLOTS 4u
#define GC_MACRO_WRITES 8u   /* most PARAMETER writes one macro move makes */
#define GC_MACRO_LABELS 32u  /* shared label vocabulary, ids 0..31 */
typedef struct { uint16_t parameter; int32_t value; } gc_macro_write;
/* The PARAMETER writes (track's patch parameter IDs, decoded semantic
 * values, P10..P47 first) that put slot at amount; returns their count
 * (1..GC_MACRO_WRITES), 0 for a null/unknown record or slot > 3. */
uint32_t gc_macro_apply(const gc_patch *p, uint32_t slot, uint32_t amount,
                        gc_macro_write out[GC_MACRO_WRITES]);
/* The amount at which slot's writes reproduce the record's stored values. */
uint32_t gc_macro_rest(const gc_patch *p, uint32_t slot);
/* The slot's label id (0..31) and text (uppercase ASCII, <= 7 characters,
 * static storage); "" / GC_NONE for a null/unknown record or slot. */
uint32_t gc_macro_label_id(const gc_patch *p, uint32_t slot);
const char *gc_macro_label(const gc_patch *p, uint32_t slot);
const char *gc_macro_label_text(uint32_t label_id);
#ifdef __cplusplus
}
#endif
#endif
