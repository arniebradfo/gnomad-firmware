/* SPDX-License-Identifier: GPL-3.0-only
 * Silent descriptor for a removed engine. Shared voices and formats stay intact. */
#pragma once
#if GN_DROP_ANALOG || GN_DROP_PHASE || GN_DROP_LOFI || GN_DROP_SAMPLE || GN_DROP_VOICE || GN_DROP_TRIO || GN_DROP_WHEEL || GN_DROP_GRAIN || GN_DROP_PHYS || GN_DROP_DRUM || GN_DROP_NOISE || GN_DROP_FM6 || GN_DROP_SLICE
static void gn_size_note_on(track_t *t, voice_t *v) { (void)t; v->active = 0; }
static void gn_size_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{ (void)t; (void)out; (void)n; (void)m; v->active = 0; }
static const preset_t GN_SIZE_PRESET[] = {{.name = "SILENT"}};
#define GN_SIZE_EDIT {"-", F_INT, -127, 127, 0, 0, 0}
#define GN_SIZE_ENGINE(symbol, label) static const engine_t symbol = { \
    .name = label, .page_title = {"-", "-"}, \
    .edit = {GN_SIZE_EDIT, GN_SIZE_EDIT, GN_SIZE_EDIT, GN_SIZE_EDIT, \
             GN_SIZE_EDIT, GN_SIZE_EDIT, GN_SIZE_EDIT, GN_SIZE_EDIT}, \
    .presets = GN_SIZE_PRESET, .npresets = 1, .knob = {P_E0, P_E1, P_ATK, P_REL}, \
    .note_on = gn_size_note_on, .render = gn_size_render }

#endif
