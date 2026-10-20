/* SPDX-License-Identifier: GPL-3.0-only
 * FM6 retired (GN_DROP_FM6): byte counts/IDs are the existing project/editor contract. Patches kept opaquely per
 * track (a project round trip keeps them); the bank's existing error codes; no synthesis state or bank pool. */
enum {
    FP_R1 = 0, FP_L1 = 4, FP_BP = 8, FP_LD, FP_RD, FP_LC, FP_RC, FP_RS, FP_AMS, FP_KVS, FP_OL, FP_MODE, FP_FC, FP_FF,
    FP_DET, FP_OP = 21,
    FP_PR1 = 126, FP_PL1 = 130, FP_ALG = 134, FP_FB, FP_OKS, FP_LFS, FP_LFD, FP_LPMD, FP_LAMD, FP_LKS, FP_LFW,
    FP_LPMS, FP_TRNSP, FP_NAME, FP_SIZE = 155
};

#define ENGI_FM6 12u
#define FM6_BANK_N 27u
#define FM6_NFACTORY 8u
#define FM6_PACKED 128u
#define FM6_NSLOT (FM6_NFACTORY + FM6_BANK_N)
static const uint8_t FM6_INIT[FM6_PACKED] = {0};
/* (the factory patches: every one reads as FM6_INIT's zeros, as the 1,024 B of zeros the table held did: editor_fm6.c
 * FM6_FACTORY_ROW, the editor's GET and LIST answer the same bytes) */
#define FM6_FACTORY_ROW(k) ((void)(k), FM6_INIT)
/* FM6 retired: each track keeps the packed patch it was given (a project's, the editor's) opaquely, so loading and
 * saving a 1.0.1 project gives its FM6 patches back byte for byte. The "unpacked" patch here is the packed bytes, then
 * zeros (FP_SIZE + 1 > FM6_PACKED); nothing renders or edits it. */
_Static_assert(FP_SIZE + 1u >= FM6_PACKED, "the packed patch fits the unpacked one");
static uint8_t fm6_patch[NTRK][FP_SIZE + 1u];
static uint8_t fm6_pgen[NTRK];
static uint8_t fm6_slot[NTRK]; /* callers keep slot selection */
static void fm6_pack(const uint8_t *v, uint8_t *b) { memcpy(b, v, FM6_PACKED); }
static void fm6_unpack(const uint8_t *b, uint8_t *v) { memset(v, 0, FP_SIZE + 1u); memcpy(v, b, FM6_PACKED); }
static void fm6_set_patch(uint32_t tr, const uint8_t *v)
{
    memcpy(fm6_patch[tr % NTRK], v, FP_SIZE + 1u);
    fm6_pgen[tr % NTRK]++;
}
static void fm6_track_loaded(const track_t *t) { (void)t; }
static void fm6_init(void) { }
static void fm6_poll(void) { }
GN_SIZE_ENGINE(ENG_FM6, "FM6");
