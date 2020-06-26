/*
 * Copyright © 2018, VideoLAN and dav1d authors
 * Copyright © 2018, Two Orioles, LLC
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef DAV1D_SRC_INTERNAL_H
#define DAV1D_SRC_INTERNAL_H

#include <stdatomic.h>

#include "dav1d/data.h"

typedef struct Dav1dFrameContext Dav1dFrameContext;
typedef struct Dav1dTileState Dav1dTileState;
typedef struct Dav1dTileContext Dav1dTileContext;

#include "common/attributes.h"

#include "src/cdef.h"
#include "src/cdf.h"
#include "src/data.h"
#include "src/env.h"
#include "src/film_grain.h"
#include "src/intra_edge.h"
#include "src/ipred.h"
#include "src/itx.h"
#include "src/levels.h"
#include "src/lf_mask.h"
#include "src/loopfilter.h"
#include "src/looprestoration.h"
#include "src/mc.h"
#include "src/msac.h"
#include "src/picture.h"
#include "src/recon.h"
#include "src/refmvs.h"
#include "src/thread.h"

typedef struct Dav1dDSPContext {
    Dav1dFilmGrainDSPContext fg;
    Dav1dIntraPredDSPContext ipred;
    Dav1dMCDSPContext mc;
    Dav1dInvTxfmDSPContext itx;
    Dav1dLoopFilterDSPContext lf;
    Dav1dCdefDSPContext cdef;
    Dav1dLoopRestorationDSPContext lr;
} Dav1dDSPContext;

struct Dav1dTileGroup {
    Dav1dData data;
    int start, end;
};

struct Dav1dContext {
    Dav1dFrameContext *fc;
    unsigned n_fc;

    // cache of OBUs that make up a single frame before we submit them
    // to a frame worker to be decoded
    struct Dav1dTileGroup *tile;
    int n_tile_data_alloc;
    int n_tile_data;
    int n_tiles;
    Dav1dRef *seq_hdr_ref;
    Dav1dSequenceHeader *seq_hdr;
    Dav1dRef *frame_hdr_ref;
    Dav1dFrameHeader *frame_hdr;

    Dav1dRef *content_light_ref;
    Dav1dContentLightLevel *content_light;
    Dav1dRef *mastering_display_ref;
    Dav1dMasteringDisplay *mastering_display;
    Dav1dRef *itut_t35_ref;
    Dav1dITUTT35 *itut_t35;

    // decoded output picture queue
    Dav1dData in;
    Dav1dPicture out;
    struct {
        Dav1dThreadPicture *out_delayed;
        unsigned next;
        // dummy is a pointer to prevent compiler errors about atomic_load()
        // not taking const arguments; the const attribute is not taken
        // from pointers
        atomic_int flush_mem, *flush;
    } frame_thread;

    // reference/entropy state
    struct {
        Dav1dThreadPicture p;
        Dav1dRef *segmap;
        Dav1dRef *refmvs;
        unsigned refpoc[7];
    } refs[8];
    CdfThreadContext cdf[8];

    Dav1dDSPContext dsp[3 /* 8, 10, 12 bits/component */];

    // tree to keep track of which edges are available
    struct {
        EdgeNode *root[2 /* BL_128X128 vs. BL_64X64 */];
        EdgeBranch branch_sb128[1 + 4 + 16 + 64];
        EdgeBranch branch_sb64[1 + 4 + 16];
        EdgeTip tip_sb128[256];
        EdgeTip tip_sb64[64];
    } intra_edge;

    Dav1dPicAllocator allocator;
    int apply_grain;
    int operating_point;
    unsigned operating_point_idc;
    int all_layers;
    unsigned frame_size_limit;
    int drain;

    Dav1dLogger logger;
};

struct Dav1dFrameContext {
    Dav1dRef *seq_hdr_ref;
    Dav1dSequenceHeader *seq_hdr;
    Dav1dRef *frame_hdr_ref;
    Dav1dFrameHeader *frame_hdr;
    Dav1dThreadPicture refp[7];
    Dav1dPicture cur; // during block coding / reconstruction
    Dav1dThreadPicture sr_cur; // after super-resolution upscaling
    Dav1dRef *mvs_ref;
    refmvs_temporal_block *mvs, *ref_mvs[7];
    Dav1dRef *ref_mvs_ref[7];
    Dav1dRef *cur_segmap_ref, *prev_segmap_ref;
    uint8_t *cur_segmap;
    const uint8_t *prev_segmap;
    unsigned refpoc[7], refrefpoc[7][7];
    uint8_t gmv_warp_allowed[7];
    CdfThreadContext in_cdf, out_cdf;
    struct Dav1dTileGroup *tile;
    int n_tile_data_alloc;
    int n_tile_data;

    // for scalable references
    struct ScalableMotionParams {
        int scale; // if no scaling, this is 0
        int step;
    } svc[7][2 /* x, y */];
    int resize_step[2 /* y, uv */], resize_start[2 /* y, uv */];

    const Dav1dContext *c;
    Dav1dTileContext *tc;
    int n_tc;
    Dav1dTileState *ts;
    int n_ts;
    const Dav1dDSPContext *dsp;
    struct {
        recon_b_intra_fn recon_b_intra;
        recon_b_inter_fn recon_b_inter;
        filter_sbrow_fn filter_sbrow;
        backup_ipred_edge_fn backup_ipred_edge;
        read_coef_blocks_fn read_coef_blocks;
    } bd_fn;

    int ipred_edge_sz;
    pixel *ipred_edge[3];
    ptrdiff_t b4_stride;
    int w4, h4, bw, bh, sb128w, sb128h, sbh, sb_shift, sb_step, sr_sb128w;
    uint16_t dq[DAV1D_MAX_SEGMENTS][3 /* plane */][2 /* dc/ac */];
    const uint8_t *qm[2 /* is_1d */][N_RECT_TX_SIZES][3 /* plane */];
    BlockContext *a;
    int a_sz /* w*tile_rows */;
    refmvs_frame rf;
    uint8_t jnt_weights[7][7];
    int bitdepth_max;

    struct {
        struct thread_data td;
        int pass, die;
        // indexed using t->by * f->b4_stride + t->bx
        Av1Block *b;
        struct CodedBlockInfo {
            int16_t eob[3 /* plane */];
            uint8_t txtp[3 /* plane */];
        } *cbi;
        // indexed using (t->by >> 1) * (f->b4_stride >> 1) + (t->bx >> 1)
        uint16_t (*pal)[3 /* plane */][8 /* idx */];
        // iterated over inside tile state
        uint8_t *pal_idx;
        coef *cf;
        int pal_sz, pal_idx_sz, cf_sz;
        // start offsets per tile
        int *tile_start_off;
    } frame_thread;

    // loopfilter
    struct {
        uint8_t (*level)[4];
        Av1Filter *mask;
        Av1Restoration *lr_mask;
        int top_pre_cdef_toggle;
        int mask_sz /* w*h */, lr_mask_sz, cdef_line_sz[2] /* stride */;
        int lr_line_sz, re_sz /* h */;
        ALIGN(Av1FilterLUT lim_lut, 16);
        int last_sharpness;
        uint8_t lvl[8 /* seg_id */][4 /* dir */][8 /* ref */][2 /* is_gmv */];
        uint8_t *tx_lpf_right_edge[2];
        uint8_t *cdef_line_buf;
        pixel *cdef_line[2 /* pre, post */][3 /* plane */];
        pixel *lr_lpf_line[3 /* plane */];

        // in-loop filter per-frame state keeping
        int tile_row; // for carry-over at tile row edges
        pixel *p[3], *sr_p[3];
        Av1Filter *mask_ptr, *prev_mask_ptr;
        int restore_planes; // enum LrRestorePlanes
    } lf;

    // threading (refer to tc[] for per-thread things)
    struct FrameTileThreadData {
        uint64_t available;
        pthread_mutex_t lock;
        pthread_cond_t cond, icond;
        int tasks_left, num_tasks;
        int (*task_idx_to_sby_and_tile_idx)[2];
        int titsati_sz, titsati_init[2];
        uint16_t titsati_index_rows[1 + DAV1D_MAX_TILE_ROWS];
        int inited;
    } tile_thread;
};

struct Dav1dTileState {
    CdfContext cdf;
    MsacContext msac;

    struct {
        int col_start, col_end, row_start, row_end; // in 4px units
        int col, row; // in tile units
    } tiling;

    atomic_int progress; // in sby units, TILE_ERROR after a decoding error
    struct {
        pthread_mutex_t lock;
        pthread_cond_t cond;
    } tile_thread;
    struct {
        uint8_t *pal_idx;
        coef *cf;
    } frame_thread;

    uint16_t dqmem[DAV1D_MAX_SEGMENTS][3 /* plane */][2 /* dc/ac */];
    const uint16_t (*dq)[3][2];
    int last_qidx;

    int8_t last_delta_lf[4];
    uint8_t lflvlmem[8 /* seg_id */][4 /* dir */][8 /* ref */][2 /* is_gmv */];
    const uint8_t (*lflvl)[4][8][2];

    Av1RestorationUnit *lr_ref[3];
};

struct Dav1dTileContext {
    const Dav1dFrameContext *f;
    Dav1dTileState *ts;
    int bx, by;
    BlockContext l, *a;
    ALIGN(union, 32) {
        int16_t cf_8bpc [32 * 32];
        int32_t cf_16bpc[32 * 32];
    };
    // FIXME types can be changed to pixel (and dynamically allocated)
    // which would make copy/assign operations slightly faster?
    uint16_t al_pal[2 /* a/l */][32 /* bx/y4 */][3 /* plane */][8 /* palette_idx */];
    uint8_t pal_sz_uv[2 /* a/l */][32 /* bx4/by4 */];
    uint8_t txtp_map[32 * 32]; // inter-only
    refmvs_tile rt;
    ALIGN(union, 64) {
        struct {
            union {
                uint8_t  lap_8bpc [128 * 32];
                uint16_t lap_16bpc[128 * 32];
                struct {
                    int16_t compinter[2][128 * 128];
                    uint8_t seg_mask[128 * 128];
                };
            };
            union {
                // stride=192 for non-SVC, or 320 for SVC
                uint8_t  emu_edge_8bpc [320 * (256 + 7)];
                uint16_t emu_edge_16bpc[320 * (256 + 7)];
            };
        };
        struct {
            union {
                uint8_t levels[32 * 34];
                struct {
                    uint8_t pal_order[64][8];
                    uint8_t pal_ctx[64];
                };
            };
            int16_t ac[32 * 32];
            uint8_t pal_idx[2 * 64 * 64];
            uint16_t pal[3 /* plane */][8 /* palette_idx */];
            ALIGN(union, 32) {
                struct {
                    uint8_t interintra_8bpc[64 * 64];
                    uint8_t edge_8bpc[257];
                };
                struct {
                    uint16_t interintra_16bpc[64 * 64];
                    uint16_t edge_16bpc[257];
                };
            };
        };
    } scratch;

    Dav1dWarpedMotionParams warpmv;
    Av1Filter *lf_mask;
    int8_t *cur_sb_cdef_idx_ptr;
    // for chroma sub8x8, we need to know the filter for all 4 subblocks in
    // a 4x4 area, but the top/left one can go out of cache already, so this
    // keeps it accessible
    enum Filter2d tl_4x4_filter;

    struct {
        struct thread_data td;
        struct FrameTileThreadData *fttd;
        int die;
    } tile_thread;
};

#endif /* DAV1D_SRC_INTERNAL_H */
