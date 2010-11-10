/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#include "findnearmv.h"

#define FINDNEAR_SEARCH_SITES   3

/* Predict motion vectors using those from already-decoded nearby blocks.
   Note that we only consider one 4x4 subblock from each candidate 16x16
   macroblock.   */

typedef union
{
    unsigned int as_int;
    MV           as_mv;
} int_mv;        /* facilitates rapid equality tests */

static void mv_bias(const MODE_INFO *x, int refframe, int_mv *mvp, const int *ref_frame_sign_bias)
{
    MV xmv;
    xmv = x->mbmi.mv.as_mv;

    if (ref_frame_sign_bias[x->mbmi.ref_frame] != ref_frame_sign_bias[refframe])
    {
        xmv.row *= -1;
        xmv.col *= -1;
    }

    mvp->as_mv = xmv;
}


void vp8_clamp_mv(MV *mv, const MACROBLOCKD *xd)
{
    if (mv->col < (xd->mb_to_left_edge - LEFT_TOP_MARGIN))
        mv->col = xd->mb_to_left_edge - LEFT_TOP_MARGIN;
    else if (mv->col > xd->mb_to_right_edge + RIGHT_BOTTOM_MARGIN)
        mv->col = xd->mb_to_right_edge + RIGHT_BOTTOM_MARGIN;

    if (mv->row < (xd->mb_to_top_edge - LEFT_TOP_MARGIN))
        mv->row = xd->mb_to_top_edge - LEFT_TOP_MARGIN;
    else if (mv->row > xd->mb_to_bottom_edge + RIGHT_BOTTOM_MARGIN)
        mv->row = xd->mb_to_bottom_edge + RIGHT_BOTTOM_MARGIN;
}


void vp8_find_near_mvs
(
    MACROBLOCKD *xd,
    const MODE_INFO *here,
    MV *nearest,
    MV *nearby,
    MV *best_mv,
    int cnt[4],
    int refframe,
    int *ref_frame_sign_bias
)
{
    const MODE_INFO *above = here - xd->mode_info_stride;
    const MODE_INFO *left = here - 1;
    const MODE_INFO *aboveleft = above - 1;
    int_mv            near_mvs[4];
    int_mv           *mv = near_mvs;
    int             *cntx = cnt;
    enum {CNT_INTRA, CNT_NEAREST, CNT_NEAR, CNT_SPLITMV};

    /* Zero accumulators */
    mv[0].as_int = mv[1].as_int = mv[2].as_int = 0;
    cnt[0] = cnt[1] = cnt[2] = cnt[3] = 0;

    /* Process above */
    if (above->mbmi.ref_frame != INTRA_FRAME)
    {
        if (above->mbmi.mv.as_int)
        {
            (++mv)->as_int = above->mbmi.mv.as_int;
            mv_bias(above, refframe, mv, ref_frame_sign_bias);
            ++cntx;
        }

        *cntx += 2;
    }

    /* Process left */
    if (left->mbmi.ref_frame != INTRA_FRAME)
    {
        if (left->mbmi.mv.as_int)
        {
            int_mv this_mv;

            this_mv.as_int = left->mbmi.mv.as_int;
            mv_bias(left, refframe, &this_mv, ref_frame_sign_bias);

            if (this_mv.as_int != mv->as_int)
            {
                (++mv)->as_int = this_mv.as_int;
                ++cntx;
            }

            *cntx += 2;
        }
        else
            cnt[CNT_INTRA] += 2;
    }

    /* Process above left */
    if (aboveleft->mbmi.ref_frame != INTRA_FRAME)
    {
        if (aboveleft->mbmi.mv.as_int)
        {
            int_mv this_mv;

            this_mv.as_int = aboveleft->mbmi.mv.as_int;
            mv_bias(aboveleft, refframe, &this_mv, ref_frame_sign_bias);

            if (this_mv.as_int != mv->as_int)
            {
                (++mv)->as_int = this_mv.as_int;
                ++cntx;
            }

            *cntx += 1;
        }
        else
            cnt[CNT_INTRA] += 1;
    }

    /* If we have three distinct MV's ... */
    if (cnt[CNT_SPLITMV])
    {
        /* See if above-left MV can be merged with NEAREST */
        if (mv->as_int == near_mvs[CNT_NEAREST].as_int)
            cnt[CNT_NEAREST] += 1;
    }

    cnt[CNT_SPLITMV] = ((above->mbmi.mode == SPLITMV)
                        + (left->mbmi.mode == SPLITMV)) * 2
                       + (aboveleft->mbmi.mode == SPLITMV);

    /* Swap near and nearest if necessary */
    if (cnt[CNT_NEAR] > cnt[CNT_NEAREST])
    {
        int tmp;
        tmp = cnt[CNT_NEAREST];
        cnt[CNT_NEAREST] = cnt[CNT_NEAR];
        cnt[CNT_NEAR] = tmp;
        tmp = near_mvs[CNT_NEAREST].as_int;
        near_mvs[CNT_NEAREST].as_int = near_mvs[CNT_NEAR].as_int;
        near_mvs[CNT_NEAR].as_int = tmp;
    }

    /* Use near_mvs[0] to store the "best" MV */
    if (cnt[CNT_NEAREST] >= cnt[CNT_INTRA])
        near_mvs[CNT_INTRA] = near_mvs[CNT_NEAREST];

    /* Set up return values */
    *best_mv = near_mvs[0].as_mv;
    *nearest = near_mvs[CNT_NEAREST].as_mv;
    *nearby = near_mvs[CNT_NEAR].as_mv;

    vp8_clamp_mv(nearest, xd);
    vp8_clamp_mv(nearby, xd);
    vp8_clamp_mv(best_mv, xd); /*TODO: move this up before the copy*/
}

vp8_prob *vp8_mv_ref_probs(
    vp8_prob p[VP8_MVREFS-1], const int near_mv_ref_ct[4]
)
{
    p[0] = vp8_mode_contexts [near_mv_ref_ct[0]] [0];
    p[1] = vp8_mode_contexts [near_mv_ref_ct[1]] [1];
    p[2] = vp8_mode_contexts [near_mv_ref_ct[2]] [2];
    p[3] = vp8_mode_contexts [near_mv_ref_ct[3]] [3];
    /*p[3] = vp8_mode_contexts [near_mv_ref_ct[1] + near_mv_ref_ct[2] + near_mv_ref_ct[3]] [3];*/
    return p;
}

const B_MODE_INFO *vp8_left_bmi(const MODE_INFO *cur_mb, int b)
{
    if (!(b & 3))
    {
        /* On L edge, get from MB to left of us */
        --cur_mb;
        b += 4;
    }

    return cur_mb->bmi + b - 1;
}

const B_MODE_INFO *vp8_above_bmi(const MODE_INFO *cur_mb, int b, int mi_stride)
{
    if (!(b >> 2))
    {
        /* On top edge, get from MB above us */
        cur_mb -= mi_stride;
        b += 16;
    }

    return cur_mb->bmi + b - 4;
}
