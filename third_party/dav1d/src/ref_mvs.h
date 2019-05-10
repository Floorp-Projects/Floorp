/*
 * Copyright (c) 2001-2016, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#ifndef DAV1D_SRC_REF_MVS_H
#define DAV1D_SRC_REF_MVS_H

#include <stddef.h>

#include "src/levels.h"
#include "src/tables.h"

typedef struct refmvs {
    mv mv[2];
    int8_t ref[2]; // [0] = 0: intra=1, [1] = -1: comp=0
    int8_t mode, sb_type;
} refmvs;

typedef struct candidate_mv {
    mv this_mv;
    mv comp_mv;
    int weight;
} candidate_mv;

typedef struct AV1_COMMON AV1_COMMON;

// call once per frame thread
AV1_COMMON *dav1d_alloc_ref_mv_common(void);
void dav1d_free_ref_mv_common(AV1_COMMON *cm);

// call once per frame
int dav1d_init_ref_mv_common(AV1_COMMON *cm, int w8, int h8,
                             ptrdiff_t stride, int allow_sb128,
                             refmvs *cur, refmvs *ref_mvs[7],
                             unsigned cur_poc,
                             const unsigned ref_poc[7],
                             const unsigned ref_ref_poc[7][7],
                             const Dav1dWarpedMotionParams gmv[7],
                             int allow_hp, int force_int_mv,
                             int allow_ref_frame_mvs, int order_hint);

// call for start of each sbrow per tile
void dav1d_init_ref_mv_tile_row(AV1_COMMON *cm,
                                int tile_col_start4, int tile_col_end4,
                                int row_start4, int row_end4);

// call for each block
void dav1d_find_ref_mvs(candidate_mv *mvstack, int *cnt, mv (*mvlist)[2],
                        int *ctx, int refidx[2], int w4, int h4,
                        enum BlockSize bs, enum BlockPartition bp,
                        int by4, int bx4, int tile_col_start4,
                        int tile_col_end4, int tile_row_start4,
                        int tile_row_end4, AV1_COMMON *cm);

extern const uint8_t dav1d_bs_to_sbtype[];
extern const uint8_t dav1d_sbtype_to_bs[];
static inline void splat_oneref_mv(refmvs *r, const ptrdiff_t stride,
                                   const int by4, const int bx4,
                                   const enum BlockSize bs,
                                   const enum InterPredMode mode,
                                   const int ref, const mv mv,
                                   const int is_interintra)
{
    const int bw4 = dav1d_block_dimensions[bs][0];
    int bh4 = dav1d_block_dimensions[bs][1];

    r += by4 * stride + bx4;
    const refmvs tmpl = (refmvs) {
        .ref = { ref + 1, is_interintra ? 0 : -1 },
        .mv = { mv },
        .sb_type = dav1d_bs_to_sbtype[bs],
        .mode = N_INTRA_PRED_MODES + mode,
    };
    do {
        for (int x = 0; x < bw4; x++)
            r[x] = tmpl;
        r += stride;
    } while (--bh4);
}

static inline void splat_intrabc_mv(refmvs *r, const ptrdiff_t stride,
                                    const int by4, const int bx4,
                                    const enum BlockSize bs, const mv mv)
{
    const int bw4 = dav1d_block_dimensions[bs][0];
    int bh4 = dav1d_block_dimensions[bs][1];

    r += by4 * stride + bx4;
    const refmvs tmpl = (refmvs) {
        .ref = { 0, -1 },
        .mv = { mv },
        .sb_type = dav1d_bs_to_sbtype[bs],
        .mode = DC_PRED,
    };
    do {
        for (int x = 0; x < bw4; x++)
            r[x] = tmpl;
        r += stride;
    } while (--bh4);
}

static inline void splat_tworef_mv(refmvs *r, const ptrdiff_t stride,
                                   const int by4, const int bx4,
                                   const enum BlockSize bs,
                                   const enum CompInterPredMode mode,
                                   const int ref1, const int ref2,
                                   const mv mv1, const mv mv2)
{
    const int bw4 = dav1d_block_dimensions[bs][0];
    int bh4 = dav1d_block_dimensions[bs][1];

    r += by4 * stride + bx4;
    const refmvs tmpl = (refmvs) {
        .ref = { ref1 + 1, ref2 + 1 },
        .mv = { mv1, mv2 },
        .sb_type = dav1d_bs_to_sbtype[bs],
        .mode = N_INTRA_PRED_MODES + N_INTER_PRED_MODES + mode,
    };
    do {
        for (int x = 0; x < bw4; x++)
            r[x] = tmpl;
        r += stride;
    } while (--bh4);
}

static inline void splat_intraref(refmvs *r, const ptrdiff_t stride,
                                  const int by4, const int bx4,
                                  const enum BlockSize bs,
                                  const enum IntraPredMode mode)
{
    const int bw4 = dav1d_block_dimensions[bs][0];
    int bh4 = dav1d_block_dimensions[bs][1];

    r += by4 * stride + bx4;
    do {
        int x;

        for (x = 0; x < bw4; x++)
            r[x] = (refmvs) {
                .ref = { 0, -1 },
                .mv = { [0] = { .y = -0x8000, .x = -0x8000 }, },
                .sb_type = dav1d_bs_to_sbtype[bs],
                .mode = mode,
            };
        r += stride;
    } while (--bh4);
}

static inline void fix_mv_precision(const Dav1dFrameHeader *const hdr,
                                    mv *const mv)
{
    if (hdr->force_integer_mv) {
        const int xmod = mv->x & 7;
        mv->x &= ~7;
        mv->x += (xmod > 4 - (mv->x < 0)) << 3;
        const int ymod = mv->y & 7;
        mv->y &= ~7;
        mv->y += (ymod > 4 - (mv->y < 0)) << 3;
    } else if (!hdr->hp) {
        if (mv->x & 1) {
            if (mv->x < 0) mv->x++;
            else           mv->x--;
        }
        if (mv->y & 1) {
            if (mv->y < 0) mv->y++;
            else           mv->y--;
        }
    }
}

#endif /* DAV1D_SRC_REF_MVS_H */
