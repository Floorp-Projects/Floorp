/*
 *  Copyright (c) 2010 The VP8 project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#include "treereader.h"
#include "entropymv.h"
#include "entropymode.h"
#include "onyxd_int.h"
#include "findnearmv.h"
#include "demode.h"
#if CONFIG_DEBUG
#include <assert.h>
#endif

static int read_mvcomponent(vp8_reader *r, const MV_CONTEXT *mvc)
{
    const vp8_prob *const p = (const vp8_prob *) mvc;
    int x = 0;

    if (vp8_read(r, p [mvpis_short]))  /* Large */
    {
        int i = 0;

        do
        {
            x += vp8_read(r, p [MVPbits + i]) << i;
        }
        while (++i < 3);

        i = mvlong_width - 1;  /* Skip bit 3, which is sometimes implicit */

        do
        {
            x += vp8_read(r, p [MVPbits + i]) << i;
        }
        while (--i > 3);

        if (!(x & 0xFFF0)  ||  vp8_read(r, p [MVPbits + 3]))
            x += 8;
    }
    else   /* small */
        x = vp8_treed_read(r, vp8_small_mvtree, p + MVPshort);

    if (x  &&  vp8_read(r, p [MVPsign]))
        x = -x;

    return x;
}

static void read_mv(vp8_reader *r, MV *mv, const MV_CONTEXT *mvc)
{
    mv->row = (short)(read_mvcomponent(r,   mvc) << 1);
    mv->col = (short)(read_mvcomponent(r, ++mvc) << 1);
}


static void read_mvcontexts(vp8_reader *bc, MV_CONTEXT *mvc)
{
    int i = 0;

    do
    {
        const vp8_prob *up = vp8_mv_update_probs[i].prob;
        vp8_prob *p = (vp8_prob *)(mvc + i);
        vp8_prob *const pstop = p + MVPcount;

        do
        {
            if (vp8_read(bc, *up++))
            {
                const vp8_prob x = (vp8_prob)vp8_read_literal(bc, 7);

                *p = x ? x << 1 : 1;
            }
        }
        while (++p < pstop);
    }
    while (++i < 2);
}


static MB_PREDICTION_MODE read_mv_ref(vp8_reader *bc, const vp8_prob *p)
{
    const int i = vp8_treed_read(bc, vp8_mv_ref_tree, p);

    return (MB_PREDICTION_MODE)i;
}

static MB_PREDICTION_MODE sub_mv_ref(vp8_reader *bc, const vp8_prob *p)
{
    const int i = vp8_treed_read(bc, vp8_sub_mv_ref_tree, p);

    return (MB_PREDICTION_MODE)i;
}
unsigned int vp8_mv_cont_count[5][4] =
{
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 }
};

void vp8_decode_mode_mvs(VP8D_COMP *pbi)
{
    const MV Zero = { 0, 0};

    VP8_COMMON *const pc = & pbi->common;
    vp8_reader *const bc = & pbi->bc;

    MODE_INFO *mi = pc->mi, *ms;
    const int mis = pc->mode_info_stride;

    MV_CONTEXT *const mvc = pc->fc.mvc;

    int mb_row = -1;

    vp8_prob prob_intra;
    vp8_prob prob_last;
    vp8_prob prob_gf;
    vp8_prob prob_skip_false = 0;

    if (pc->mb_no_coeff_skip)
        prob_skip_false = (vp8_prob)vp8_read_literal(bc, 8);

    prob_intra = (vp8_prob)vp8_read_literal(bc, 8);
    prob_last  = (vp8_prob)vp8_read_literal(bc, 8);
    prob_gf    = (vp8_prob)vp8_read_literal(bc, 8);

    ms = pc->mi - 1;

    if (vp8_read_bit(bc))
    {
        int i = 0;

        do
        {
            pc->fc.ymode_prob[i] = (vp8_prob) vp8_read_literal(bc, 8);
        }
        while (++i < 4);
    }

    if (vp8_read_bit(bc))
    {
        int i = 0;

        do
        {
            pc->fc.uv_mode_prob[i] = (vp8_prob) vp8_read_literal(bc, 8);
        }
        while (++i < 3);
    }

    read_mvcontexts(bc, mvc);

    while (++mb_row < pc->mb_rows)
    {
        int mb_col = -1;

        while (++mb_col < pc->mb_cols)
        {
            MB_MODE_INFO *const mbmi = & mi->mbmi;
            MV *const mv = & mbmi->mv.as_mv;
            VP8_COMMON *const pc = &pbi->common;
            MACROBLOCKD *xd = &pbi->mb;

            mbmi->need_to_clamp_mvs = 0;
            // Distance of Mb to the various image edges.
            // These specified to 8th pel as they are always compared to MV values that are in 1/8th pel units
            xd->mb_to_left_edge = -((mb_col * 16) << 3);
            xd->mb_to_right_edge = ((pc->mb_cols - 1 - mb_col) * 16) << 3;
            xd->mb_to_top_edge = -((mb_row * 16)) << 3;
            xd->mb_to_bottom_edge = ((pc->mb_rows - 1 - mb_row) * 16) << 3;

            // If required read in new segmentation data for this MB
            if (pbi->mb.update_mb_segmentation_map)
                vp8_read_mb_features(bc, mbmi, &pbi->mb);

            // Read the macroblock coeff skip flag if this feature is in use, else default to 0
            if (pc->mb_no_coeff_skip)
                mbmi->mb_skip_coeff = vp8_read(bc, prob_skip_false);
            else
                mbmi->mb_skip_coeff = 0;

            mbmi->uv_mode = DC_PRED;

            if ((mbmi->ref_frame = (MV_REFERENCE_FRAME) vp8_read(bc, prob_intra)))    /* inter MB */
            {
                int rct[4];
                vp8_prob mv_ref_p [VP8_MVREFS-1];
                MV nearest, nearby, best_mv;

                if (vp8_read(bc, prob_last))
                {
                    mbmi->ref_frame = (MV_REFERENCE_FRAME)((int)mbmi->ref_frame + (int)(1 + vp8_read(bc, prob_gf)));
                }

                vp8_find_near_mvs(xd, mi, &nearest, &nearby, &best_mv, rct, mbmi->ref_frame, pbi->common.ref_frame_sign_bias);

                vp8_mv_ref_probs(mv_ref_p, rct);

                switch (mbmi->mode = read_mv_ref(bc, mv_ref_p))
                {
                case SPLITMV:
                {
                    const int s = mbmi->partitioning = vp8_treed_read(
                                                           bc, vp8_mbsplit_tree, vp8_mbsplit_probs
                                                       );
                    const int num_p = vp8_mbsplit_count [s];
                    const int *const  L = vp8_mbsplits [s];
                    int j = 0;

                    do  /* for each subset j */
                    {
                        B_MODE_INFO *const bmi = mbmi->partition_bmi + j;
                        MV *const mv = & bmi->mv.as_mv;

                        int k = -1;  /* first block in subset j */
                        int mv_contz;

                        while (j != L[++k])
                        {
#if CONFIG_DEBUG
                            if (k >= 16)
                            {
                                assert(0);
                            }
#endif
                        }

                        mv_contz = vp8_mv_cont(&(vp8_left_bmi(mi, k)->mv.as_mv), &(vp8_above_bmi(mi, k, mis)->mv.as_mv));

                        switch (bmi->mode = (B_PREDICTION_MODE) sub_mv_ref(bc, vp8_sub_mv_ref_prob2 [mv_contz])) //pc->fc.sub_mv_ref_prob))
                        {
                        case NEW4X4:
                            read_mv(bc, mv, (const MV_CONTEXT *) mvc);
                            mv->row += best_mv.row;
                            mv->col += best_mv.col;
#ifdef VPX_MODE_COUNT
                            vp8_mv_cont_count[mv_contz][3]++;
#endif
                            break;
                        case LEFT4X4:
                            *mv = vp8_left_bmi(mi, k)->mv.as_mv;
#ifdef VPX_MODE_COUNT
                            vp8_mv_cont_count[mv_contz][0]++;
#endif
                            break;
                        case ABOVE4X4:
                            *mv = vp8_above_bmi(mi, k, mis)->mv.as_mv;
#ifdef VPX_MODE_COUNT
                            vp8_mv_cont_count[mv_contz][1]++;
#endif
                            break;
                        case ZERO4X4:
                            *mv = Zero;
#ifdef VPX_MODE_COUNT
                            vp8_mv_cont_count[mv_contz][2]++;
#endif
                            break;
                        default:
                            break;
                        }

                        if (mv->col < xd->mb_to_left_edge
                                      - LEFT_TOP_MARGIN
                            || mv->col > xd->mb_to_right_edge
                                         + RIGHT_BOTTOM_MARGIN
                            || mv->row < xd->mb_to_top_edge
                                         - LEFT_TOP_MARGIN
                            || mv->row > xd->mb_to_bottom_edge
                                         + RIGHT_BOTTOM_MARGIN
                            )
                            mbmi->need_to_clamp_mvs = 1;

                        /* Fill (uniform) modes, mvs of jth subset.
                           Must do it here because ensuing subsets can
                           refer back to us via "left" or "above". */
                        do
                            if (j == L[k])
                                mi->bmi[k] = *bmi;

                        while (++k < 16);
                    }
                    while (++j < num_p);
                }

                *mv = mi->bmi[15].mv.as_mv;

                break;  /* done with SPLITMV */

                case NEARMV:
                    *mv = nearby;

                    // Clip "next_nearest" so that it does not extend to far out of image
                    if (mv->col < (xd->mb_to_left_edge - LEFT_TOP_MARGIN))
                        mv->col = xd->mb_to_left_edge - LEFT_TOP_MARGIN;
                    else if (mv->col > xd->mb_to_right_edge + RIGHT_BOTTOM_MARGIN)
                        mv->col = xd->mb_to_right_edge + RIGHT_BOTTOM_MARGIN;

                    if (mv->row < (xd->mb_to_top_edge - LEFT_TOP_MARGIN))
                        mv->row = xd->mb_to_top_edge - LEFT_TOP_MARGIN;
                    else if (mv->row > xd->mb_to_bottom_edge + RIGHT_BOTTOM_MARGIN)
                        mv->row = xd->mb_to_bottom_edge + RIGHT_BOTTOM_MARGIN;

                    goto propagate_mv;

                case NEARESTMV:
                    *mv = nearest;

                    // Clip "next_nearest" so that it does not extend to far out of image
                    if (mv->col < (xd->mb_to_left_edge - LEFT_TOP_MARGIN))
                        mv->col = xd->mb_to_left_edge - LEFT_TOP_MARGIN;
                    else if (mv->col > xd->mb_to_right_edge + RIGHT_BOTTOM_MARGIN)
                        mv->col = xd->mb_to_right_edge + RIGHT_BOTTOM_MARGIN;

                    if (mv->row < (xd->mb_to_top_edge - LEFT_TOP_MARGIN))
                        mv->row = xd->mb_to_top_edge - LEFT_TOP_MARGIN;
                    else if (mv->row > xd->mb_to_bottom_edge + RIGHT_BOTTOM_MARGIN)
                        mv->row = xd->mb_to_bottom_edge + RIGHT_BOTTOM_MARGIN;

                    goto propagate_mv;

                case ZEROMV:
                    *mv = Zero;
                    goto propagate_mv;

                case NEWMV:
                    read_mv(bc, mv, (const MV_CONTEXT *) mvc);
                    mv->row += best_mv.row;
                    mv->col += best_mv.col;

                    /* Don't need to check this on NEARMV and NEARESTMV modes
                     * since those modes clamp the MV. The NEWMV mode does not,
                     * so signal to the prediction stage whether special
                     * handling may be required.
                     */
                    if (mv->col < xd->mb_to_left_edge - LEFT_TOP_MARGIN
                        || mv->col > xd->mb_to_right_edge + RIGHT_BOTTOM_MARGIN
                        || mv->row < xd->mb_to_top_edge - LEFT_TOP_MARGIN
                        || mv->row > xd->mb_to_bottom_edge + RIGHT_BOTTOM_MARGIN
                        )
                        mbmi->need_to_clamp_mvs = 1;

                propagate_mv:  /* same MV throughout */
                    {
                        //int i=0;
                        //do
                        //{
                        //  mi->bmi[i].mv.as_mv = *mv;
                        //}
                        //while( ++i < 16);

                        mi->bmi[0].mv.as_mv = *mv;
                        mi->bmi[1].mv.as_mv = *mv;
                        mi->bmi[2].mv.as_mv = *mv;
                        mi->bmi[3].mv.as_mv = *mv;
                        mi->bmi[4].mv.as_mv = *mv;
                        mi->bmi[5].mv.as_mv = *mv;
                        mi->bmi[6].mv.as_mv = *mv;
                        mi->bmi[7].mv.as_mv = *mv;
                        mi->bmi[8].mv.as_mv = *mv;
                        mi->bmi[9].mv.as_mv = *mv;
                        mi->bmi[10].mv.as_mv = *mv;
                        mi->bmi[11].mv.as_mv = *mv;
                        mi->bmi[12].mv.as_mv = *mv;
                        mi->bmi[13].mv.as_mv = *mv;
                        mi->bmi[14].mv.as_mv = *mv;
                        mi->bmi[15].mv.as_mv = *mv;
                    }

                    break;

                default:;
#if CONFIG_DEBUG
                    assert(0);
#endif
                }
            }
            else
            {
                /* MB is intra coded */

                int j = 0;

                do
                {
                    mi->bmi[j].mv.as_mv = Zero;
                }
                while (++j < 16);

                *mv = Zero;

                if ((mbmi->mode = (MB_PREDICTION_MODE) vp8_read_ymode(bc, pc->fc.ymode_prob)) == B_PRED)
                {
                    int j = 0;

                    do
                    {
                        mi->bmi[j].mode = (B_PREDICTION_MODE)vp8_read_bmode(bc, pc->fc.bmode_prob);
                    }
                    while (++j < 16);
                }

                mbmi->uv_mode = (MB_PREDICTION_MODE)vp8_read_uv_mode(bc, pc->fc.uv_mode_prob);
            }

            mi++;       // next macroblock
        }

        mi++;           // skip left predictor each row
    }
}
