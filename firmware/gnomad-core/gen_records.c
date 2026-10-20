/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kyle Palaniuk */
/* Gnomad (FORK.md, "Gnomad engines"): the records the firmware binds into the core, generated at vendor time
 * (firmware/gnomad-core/sync-core.sh) on the host against the vendored core itself, so the firmware's adapter
 * (firmware/src/gn_core.c) uses only the core's public header and constant bytes:
 *   - one patch per engine (VA, FM, DRUMS, GNOME from the core's own default-record builders; SAMPLE built here
 *     per SPEC/14: loop mode over the asset below, root 69), the default kit;
 *   - one small resident PCM16 asset for SAMPLE (a 441 Hz triangle, 20 exact periods, so the loop join is seamless;
 *     integer arithmetic only: the same bytes on every machine).
 * Every record is checked with gc_validate_patch / gc_validate_kit, then a core instance is started with the asset,
 * each patch is bound to track 0 and played (a note, then its release), and the run fails unless each engine
 * sounds and then frees its voices. The checks' summary goes to stdout (sync-core.sh puts it in CORE-VERSION.txt).
 *   cc -std=c99 -I<core>/include -I<core>/src gen_records.c <core>/src/*.c -o gen_records && ./gen_records OUT.h */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gc_internal.h"

#define ASSET_ID 1u
#define ASSET_PERIOD 100u                   /* 44100 / 100 = 441 Hz: MIDI 69 + 3.9 cents */
#define ASSET_FRAMES (20u * ASSET_PERIOD)
#define ASSET_ROOT 69u
#define ASSET_PEAK 24000

static uint8_t pcm[2u * ASSET_FRAMES];
static uint8_t patch[5][GC_PATCH_BYTES];
static uint8_t kit[GC_KIT_BYTES];
static const char *const ENGINE_NAME[5] = {"VA", "FM", "DRUMS", "GNOME", "SAMPLE"};

static void wr16(uint8_t *p, uint32_t v) { p[0] = (uint8_t)(v & 0xffu); p[1] = (uint8_t)(v >> 8); }

static void make_asset(void)
{
    uint32_t i;
    for (i = 0; i < ASSET_FRAMES; i++) {
        uint32_t ph = i % ASSET_PERIOD;      /* 0..99: up from 0 to +peak, down to -peak, back up to 0 */
        int32_t q = (int32_t)ph * 4 * ASSET_PEAK / (int32_t)ASSET_PERIOD, s;
        if (ph < ASSET_PERIOD / 4u)
            s = q;
        else if (ph < 3u * ASSET_PERIOD / 4u)
            s = 2 * ASSET_PEAK - q;
        else
            s = q - 4 * ASSET_PEAK;
        wr16(&pcm[2u * i], (uint32_t)(uint16_t)(int16_t)s);
    }
}

static void make_sample_patch(uint8_t *b)
{
    static const char name[] = "Init Sample";
    uint32_t i;
    memset(b, 0, GC_PATCH_BYTES);
    memcpy(b, "GCP1", 4);
    wr16(b + 4, 2u);
    wr16(b + 6, GC_PATCH_BYTES);
    b[8] = (uint8_t)GC_SAMPLE;
    for (i = 0; name[i]; i++)
        b[16 + i] = (uint8_t)name[i];
    /* SPEC/14 common defaults: Level 49152, A 1 ms, D 180 ms, S 65535, R 80 ms, Bright 32768, the rest 0 */
    wr16(b + 32 + 2 * 0, 49152u);
    wr16(b + 32 + 2 * 1, 1u);
    wr16(b + 32 + 2 * 2, 180u);
    wr16(b + 32 + 2 * 3, 65535u);
    wr16(b + 32 + 2 * 4, 80u);
    wr16(b + 32 + 2 * 5, 32768u);
    wr16(b + 32 + 2 * 12, ASSET_ID);        /* Asset */
    wr16(b + 32 + 2 * 13, 1u);              /* Mode: loop */
    wr16(b + 32 + 2 * 14, ASSET_ROOT);      /* Root */
    wr16(b + 32 + 2 * 15, 1u);              /* one range */
    wr16(b + 32 + 2 * 16, 0u);              /* start 0 */
    wr16(b + 32 + 2 * 17, ASSET_FRAMES);    /* end 0 */
    gc_record_seal(b, GC_PATCH_BYTES);
}

static void dump(FILE *f, const char *decl, const uint8_t *b, uint32_t n)
{
    uint32_t i;
    fprintf(f, "%s = {", decl);
    for (i = 0; i < n; i++)
        fprintf(f, "%s0x%02x%s", i % 16u ? "" : "\n    ", b[i], i + 1u < n ? "," : "");
    fprintf(f, "\n};\n");
}

static uint8_t state[GC_STATE_BYTES] __attribute__((aligned(8)));
static uint8_t effects[GC_FX_BYTES] __attribute__((aligned(8)));

static int play(const gc_sample_asset *asset, uint32_t e)
{
    gc_config cfg;
    gc_core *core = 0;
    gc_report rep;
    gc_event ev;
    int16_t out[128];
    int32_t peak = 0;
    uint32_t blk, i, freed = 0;
    memset(&cfg, 0, sizeof cfg);
    cfg.api_version = GC_API_VERSION;
    cfg.profile = GC_PROFILE_FX1;
    cfg.state = state;
    cfg.state_bytes = sizeof state;
    cfg.effects = effects;
    cfg.effects_bytes = sizeof effects;
    cfg.assets = asset;
    cfg.asset_count = 1;
    if (gc_init(&cfg, &core, &rep) != GC_OK) {
        fprintf(stderr, "gen_records: gc_init: status %u detail %u\n", rep.status, rep.detail);
        return 1;
    }
    if (gc_bind_track(core, 0, (const gc_patch *)patch[e], e == GC_DRUMS ? (const gc_kit *)kit : 0, &rep) != GC_OK) {
        fprintf(stderr, "gen_records: %s: bind: status %u offset %u detail %u\n", ENGINE_NAME[e], rep.status, rep.offset,
                rep.detail);
        return 1;
    }
    for (blk = 0; blk < 4000u; blk++) {      /* 32-frame blocks: 0.36 s held, then up to 2.5 s for the release */
        const gc_event *evp = 0;
        uint16_t nev = 0;
        memset(&ev, 0, sizeof ev);
        ev.track = 0;
        ev.note_id = 1;
        if (blk == 0) {
            ev.type = GC_NOTE_ON;
            ev.key = e == GC_DRUMS ? 0u : 60u;
            ev.velocity = 100;
            evp = &ev, nev = 1;
        } else if (blk == 500u) {
            ev.type = GC_NOTE_OFF;
            evp = &ev, nev = 1;
        }
        if (gc_render_mono(core, out, 32, evp, nev, &rep) != GC_OK) {
            fprintf(stderr, "gen_records: %s: render: status %u\n", ENGINE_NAME[e], rep.status);
            return 1;
        }
        for (i = 0; i < 32u; i++)
            if (abs(out[i]) > peak)
                peak = abs(out[i]);
        if (blk > 500u && rep.voices_active == 0u) {
            freed = blk;
            break;
        }
    }
    printf("  %-6s patch CRC %02x%02x%02x%02x, peak %d, voices free %u ms after the note-off\n", ENGINE_NAME[e],
           patch[e][15], patch[e][14], patch[e][13], patch[e][12], peak, freed ? (freed - 500u) * 32u * 1000u / 44100u : 0u);
    if (peak < 1000) {
        fprintf(stderr, "gen_records: %s: peak %d: the engine does not sound\n", ENGINE_NAME[e], peak);
        return 1;
    }
    if (!freed) {
        fprintf(stderr, "gen_records: %s: voices still active 2.5 s after the note-off\n", ENGINE_NAME[e]);
        return 1;
    }
    return 0;
}

int main(int argc, char **argv)
{
    gc_report rep;
    gc_sample_asset asset;
    gc_caps caps;
    FILE *f;
    uint32_t e, crc;
    char decl[96];
    if (argc != 2) {
        fprintf(stderr, "usage: gen_records OUT.h\n");
        return 2;
    }
    gc_query(&caps);
    make_asset();
    crc = gc_crc32c(pcm, sizeof pcm, 0);
    gc_default_va_patch(patch[GC_VA]);
    gc_default_fm_patch(patch[GC_FM]);
    gc_default_drum_patch(patch[GC_DRUMS]);
    gc_default_gnome_patch(patch[GC_GNOME]);
    make_sample_patch(patch[GC_SAMPLE]);
    gc_default_kit(kit);
    printf("records (gen_records.c): caps api %08x profile %08x state %u B effects %u B engines mask %u\n",
           caps.api_version, caps.profile, caps.state_bytes, caps.effects_bytes, caps.engine_mask);
    for (e = 0; e < 5u; e++)
        if (gc_validate_patch((const gc_patch *)patch[e], &rep) != GC_OK) {
            fprintf(stderr, "gen_records: %s patch invalid: status %u offset %u detail %u\n", ENGINE_NAME[e], rep.status,
                    rep.offset, rep.detail);
            return 1;
        }
    if (gc_validate_kit((const gc_kit *)kit, &rep) != GC_OK) {
        fprintf(stderr, "gen_records: kit invalid: status %u offset %u\n", rep.status, rep.offset);
        return 1;
    }
    asset.asset_id = ASSET_ID;
    asset.pcm_le = pcm;
    asset.frames = ASSET_FRAMES;
    asset.crc32c = crc;
    for (e = 0; e < 5u; e++)
        if (play(&asset, e))
            return 1;
    f = fopen(argv[1], "w");
    if (!f)
        return 1;
    fprintf(f, "/* SPDX-License-Identifier: GPL-3.0-only\n * Generated by firmware/gnomad-core/sync-core.sh (gen_records.c) from the"
               " vendored core: do not edit. */\n/* Gnomad (FORK.md, \"Gnomad engines\"): the records firmware/src/gn_core.c binds:"
               " a patch per engine\n * (index = gc_engine), the default kit, and SAMPLE's one resident asset (a 441 Hz triangle loop,"
               " root 69). */\n");
    fprintf(f, "#define GN_CORE_ASSET_ID %uu\n#define GN_CORE_ASSET_FRAMES %uu\n#define GN_CORE_ASSET_CRC32C 0x%08xu\n",
            ASSET_ID, ASSET_FRAMES, crc);
    dump(f, "static const uint8_t GN_CORE_ASSET_PCM[2u * GN_CORE_ASSET_FRAMES]", pcm, sizeof pcm);
    fprintf(f, "static const uint8_t GN_CORE_PATCH[5][GC_PATCH_BYTES] = {\n");
    for (e = 0; e < 5u; e++) {
        uint32_t i;
        fprintf(f, "    {   /* %s: \"%s\" */", ENGINE_NAME[e], (const char *)&patch[e][16]);
        for (i = 0; i < GC_PATCH_BYTES; i++)
            fprintf(f, "%s0x%02x%s", i % 16u ? "" : "\n        ", patch[e][i], i + 1u < GC_PATCH_BYTES ? "," : "");
        fprintf(f, "\n    }%s\n", e + 1u < 5u ? "," : "");
    }
    fprintf(f, "};\n");
    (void)decl;
    dump(f, "static const uint8_t GN_CORE_KIT[GC_KIT_BYTES]", kit, sizeof kit);
    fclose(f);
    return 0;
}
