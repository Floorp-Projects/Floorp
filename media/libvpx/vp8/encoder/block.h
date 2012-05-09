/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#ifndef __INC_BLOCK_H
#define __INC_BLOCK_H

#include "vp8/common/onyx.h"
#include "vp8/common/blockd.h"
#include "vp8/common/entropymv.h"
#include "vp8/common/entropy.h"
#include "vpx_ports/mem.h"

// motion search site
typedef struct
{
    MV mv;
    int offset;
} search_site;

typedef struct
{
    // 16 Y blocks, 4 U blocks, 4 V blocks each with 16 entries
    short *src_diff;
    short *coeff;

    // 16 Y blocks, 4 U blocks, 4 V blocks each with 16 entries
    short *quant;
    short *quant_fast;
    unsigned char *quant_shift;
    short *zbin;
    short *zrun_zbin_boost;
    short *round;

    // Zbin Over Quant value
    short zbin_extra;

    unsigned char **base_src;
    int src;
    int src_stride;
} BLOCK;

typedef struct
{
    int count;
    struct
    {
        B_PREDICTION_MODE mode;
        int_mv mv;
    } bmi[16];
} PARTITION_INFO;

typedef struct
{
    DECLARE_ALIGNED(16, short, src_diff[400]);       // 16x16 Y 8x8 U 8x8 V 4x4 2nd Y
    DECLARE_ALIGNED(16, short, coeff[400]);     // 16x16 Y 8x8 U 8x8 V 4x4 2nd Y
    DECLARE_ALIGNED(16, unsigned char, thismb[256]);

    unsigned char *thismb_ptr;
    // 16 Y blocks, 4 U blocks, 4 V blocks, 1 DC 2nd order block each with 16 entries
    BLOCK block[25];

    YV12_BUFFER_CONFIG src;

    MACROBLOCKD e_mbd;
    PARTITION_INFO *partition_info; /* work pointer */
    PARTITION_INFO *pi;   /* Corresponds to upper left visible macroblock */
    PARTITION_INFO *pip;  /* Base of allocated array */

    search_site *ss;
    int ss_count;
    int searches_per_step;

    int errorperbit;
    int sadperbit16;
    int sadperbit4;
    int rddiv;
    int rdmult;
    unsigned int * mb_activity_ptr;
    int * mb_norm_activity_ptr;
    signed int act_zbin_adj;
    signed int last_act_zbin_adj;

    int mvcosts[2][MVvals+1];
    int *mvcost[2];
    int mvsadcosts[2][MVfpvals+1];
    int *mvsadcost[2];
    int mbmode_cost[2][MB_MODE_COUNT];
    int intra_uv_mode_cost[2][MB_MODE_COUNT];
    unsigned int bmode_costs[10][10][10];
    unsigned int inter_bmode_costs[B_MODE_COUNT];

    // These define limits to motion vector components to prevent them from extending outside the UMV borders
    int mv_col_min;
    int mv_col_max;
    int mv_row_min;
    int mv_row_max;

    int skip;

    int encode_breakout;

    //char * gf_active_ptr;
    signed char *gf_active_ptr;

    unsigned char *active_ptr;
    MV_CONTEXT *mvc;

    unsigned int token_costs[BLOCK_TYPES] [COEF_BANDS] [PREV_COEF_CONTEXTS] [MAX_ENTROPY_TOKENS];
    int optimize;
    int q_index;

    void (*vp8_short_fdct4x4)(short *input, short *output, int pitch);
    void (*vp8_short_fdct8x4)(short *input, short *output, int pitch);
    void (*short_walsh4x4)(short *input, short *output, int pitch);
    void (*quantize_b)(BLOCK *b, BLOCKD *d);
    void (*quantize_b_pair)(BLOCK *b1, BLOCK *b2, BLOCKD *d0, BLOCKD *d1);

} MACROBLOCK;


#endif
