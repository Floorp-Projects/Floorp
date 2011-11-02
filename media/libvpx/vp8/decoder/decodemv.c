/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#include "treereader.h"
#include "vp8/common/entropymv.h"
#include "vp8/common/entropymode.h"
#include "onyxd_int.h"
#include "vp8/common/findnearmv.h"

#if CONFIG_DEBUG
#include <assert.h>
#endif
static int vp8_read_bmode(vp8_reader *bc, const vp8_prob *p)
{
    const int i = vp8_treed_read(bc, vp8_bmode_tree, p);

    return i;
}


static int vp8_read_ymode(vp8_reader *bc, const vp8_prob *p)
{
    const int i = vp8_treed_read(bc, vp8_ymode_tree, p);

    return i;
}

static int vp8_kfread_ymode(vp8_reader *bc, const vp8_prob *p)
{
    const int i = vp8_treed_read(bc, vp8_kf_ymode_tree, p);

    return i;
}



static int vp8_read_uv_mode(vp8_reader *bc, const vp8_prob *p)
{
    const int i = vp8_treed_read(bc, vp8_uv_mode_tree, p);

    return i;
}

static void vp8_read_mb_features(vp8_reader *r, MB_MODE_INFO *mi, MACROBLOCKD *x)
{
    /* Is segmentation enabled */
    if (x->segmentation_enabled && x->update_mb_segmentation_map)
    {
        /* If so then read the segment id. */
        if (vp8_read(r, x->mb_segment_tree_probs[0]))
            mi->segment_id = (unsigned char)(2 + vp8_read(r, x->mb_segment_tree_probs[2]));
        else
            mi->segment_id = (unsigned char)(vp8_read(r, x->mb_segment_tree_probs[1]));
    }
}

static void vp8_kfread_modes(VP8D_COMP *pbi, MODE_INFO *m, int mb_row, int mb_col)
{
    vp8_reader *const bc = & pbi->bc;
    const int mis = pbi->common.mode_info_stride;

        {
            MB_PREDICTION_MODE y_mode;

            /* Read the Macroblock segmentation map if it is being updated explicitly this frame (reset to 0 above by default)
             * By default on a key frame reset all MBs to segment 0
             */
            m->mbmi.segment_id = 0;

            if (pbi->mb.update_mb_segmentation_map)
                vp8_read_mb_features(bc, &m->mbmi, &pbi->mb);

            /* Read the macroblock coeff skip flag if this feature is in use, else default to 0 */
            if (pbi->common.mb_no_coeff_skip)
                m->mbmi.mb_skip_coeff = vp8_read(bc, pbi->prob_skip_false);
            else
                m->mbmi.mb_skip_coeff = 0;

            y_mode = (MB_PREDICTION_MODE) vp8_kfread_ymode(bc, pbi->common.kf_ymode_prob);

            m->mbmi.ref_frame = INTRA_FRAME;

            if ((m->mbmi.mode = y_mode) == B_PRED)
            {
                int i = 0;

                do
                {
                    const B_PREDICTION_MODE A = above_block_mode(m, i, mis);
                    const B_PREDICTION_MODE L = left_block_mode(m, i);

                    m->bmi[i].as_mode = (B_PREDICTION_MODE) vp8_read_bmode(bc, pbi->common.kf_bmode_prob [A] [L]);
                }
                while (++i < 16);
            }

            m->mbmi.uv_mode = (MB_PREDICTION_MODE)vp8_read_uv_mode(bc, pbi->common.kf_uv_mode_prob);
        }
}

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

static B_PREDICTION_MODE sub_mv_ref(vp8_reader *bc, const vp8_prob *p)
{
    const int i = vp8_treed_read(bc, vp8_sub_mv_ref_tree, p);

    return (B_PREDICTION_MODE)i;
}

#ifdef VPX_MODE_COUNT
unsigned int vp8_mv_cont_count[5][4] =
{
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 }
};
#endif

static const unsigned char mbsplit_fill_count[4] = {8, 8, 4, 1};
static const unsigned char mbsplit_fill_offset[4][16] = {
    { 0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15},
    { 0,  1,  4,  5,  8,  9, 12, 13,  2,  3,   6,  7, 10, 11, 14, 15},
    { 0,  1,  4,  5,  2,  3,  6,  7,  8,  9,  12, 13, 10, 11, 14, 15},
    { 0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15}
};




static void mb_mode_mv_init(VP8D_COMP *pbi)
{
    vp8_reader *const bc = & pbi->bc;
    MV_CONTEXT *const mvc = pbi->common.fc.mvc;

#if CONFIG_ERROR_CONCEALMENT
    /* Default is that no macroblock is corrupt, therefore we initialize
     * mvs_corrupt_from_mb to something very big, which we can be sure is
     * outside the frame. */
    pbi->mvs_corrupt_from_mb = UINT_MAX;
#endif
    pbi->prob_skip_false = 0;
    if (pbi->common.mb_no_coeff_skip)
        pbi->prob_skip_false = (vp8_prob)vp8_read_literal(bc, 8);

    if(pbi->common.frame_type != KEY_FRAME)
    {
        pbi->prob_intra = (vp8_prob)vp8_read_literal(bc, 8);
        pbi->prob_last  = (vp8_prob)vp8_read_literal(bc, 8);
        pbi->prob_gf    = (vp8_prob)vp8_read_literal(bc, 8);

        if (vp8_read_bit(bc))
        {
            int i = 0;

            do
            {
                pbi->common.fc.ymode_prob[i] = (vp8_prob) vp8_read_literal(bc, 8);
            }
            while (++i < 4);
        }

        if (vp8_read_bit(bc))
        {
            int i = 0;

            do
            {
                pbi->common.fc.uv_mode_prob[i] = (vp8_prob) vp8_read_literal(bc, 8);
            }
            while (++i < 3);
        }

        read_mvcontexts(bc, mvc);
    }
}


static void read_mb_modes_mv(VP8D_COMP *pbi, MODE_INFO *mi, MB_MODE_INFO *mbmi,
                            int mb_row, int mb_col)
{
    vp8_reader *const bc = & pbi->bc;
    MV_CONTEXT *const mvc = pbi->common.fc.mvc;
    const int mis = pbi->common.mode_info_stride;

    int_mv *const mv = & mbmi->mv;
    int mb_to_left_edge;
    int mb_to_right_edge;
    int mb_to_top_edge;
    int mb_to_bottom_edge;

    mb_to_top_edge = pbi->mb.mb_to_top_edge;
    mb_to_bottom_edge = pbi->mb.mb_to_bottom_edge;
    mb_to_top_edge -= LEFT_TOP_MARGIN;
    mb_to_bottom_edge += RIGHT_BOTTOM_MARGIN;

    mbmi->need_to_clamp_mvs = 0;
    /* Distance of Mb to the various image edges.
     * These specified to 8th pel as they are always compared to MV values that are in 1/8th pel units
     */
    pbi->mb.mb_to_left_edge =
    mb_to_left_edge = -((mb_col * 16) << 3);
    mb_to_left_edge -= LEFT_TOP_MARGIN;

    pbi->mb.mb_to_right_edge =
    mb_to_right_edge = ((pbi->common.mb_cols - 1 - mb_col) * 16) << 3;
    mb_to_right_edge += RIGHT_BOTTOM_MARGIN;

    /* If required read in new segmentation data for this MB */
    if (pbi->mb.update_mb_segmentation_map)
        vp8_read_mb_features(bc, mbmi, &pbi->mb);

    /* Read the macroblock coeff skip flag if this feature is in use, else default to 0 */
    if (pbi->common.mb_no_coeff_skip)
        mbmi->mb_skip_coeff = vp8_read(bc, pbi->prob_skip_false);
    else
        mbmi->mb_skip_coeff = 0;

    if ((mbmi->ref_frame = (MV_REFERENCE_FRAME) vp8_read(bc, pbi->prob_intra)))    /* inter MB */
    {
        int rct[4];
        vp8_prob mv_ref_p [VP8_MVREFS-1];
        int_mv nearest, nearby, best_mv;

        if (vp8_read(bc, pbi->prob_last))
        {
            mbmi->ref_frame = (MV_REFERENCE_FRAME)((int)mbmi->ref_frame + (int)(1 + vp8_read(bc, pbi->prob_gf)));
        }

        vp8_find_near_mvs(&pbi->mb, mi, &nearest, &nearby, &best_mv, rct, mbmi->ref_frame, pbi->common.ref_frame_sign_bias);

        vp8_mv_ref_probs(mv_ref_p, rct);

        mbmi->uv_mode = DC_PRED;
        switch (mbmi->mode = read_mv_ref(bc, mv_ref_p))
        {
        case SPLITMV:
        {
            const int s = mbmi->partitioning =
                      vp8_treed_read(bc, vp8_mbsplit_tree, vp8_mbsplit_probs);
            const int num_p = vp8_mbsplit_count [s];
            int j = 0;

            do  /* for each subset j */
            {
                int_mv leftmv, abovemv;
                int_mv blockmv;
                int k;  /* first block in subset j */
                int mv_contz;
                k = vp8_mbsplit_offset[s][j];

                leftmv.as_int = left_block_mv(mi, k);
                abovemv.as_int = above_block_mv(mi, k, mis);
                mv_contz = vp8_mv_cont(&leftmv, &abovemv);

                switch (sub_mv_ref(bc, vp8_sub_mv_ref_prob2 [mv_contz])) /*pc->fc.sub_mv_ref_prob))*/
                {
                case NEW4X4:
                    read_mv(bc, &blockmv.as_mv, (const MV_CONTEXT *) mvc);
                    blockmv.as_mv.row += best_mv.as_mv.row;
                    blockmv.as_mv.col += best_mv.as_mv.col;
  #ifdef VPX_MODE_COUNT
                    vp8_mv_cont_count[mv_contz][3]++;
  #endif
                    break;
                case LEFT4X4:
                    blockmv.as_int = leftmv.as_int;
  #ifdef VPX_MODE_COUNT
                    vp8_mv_cont_count[mv_contz][0]++;
  #endif
                    break;
                case ABOVE4X4:
                    blockmv.as_int = abovemv.as_int;
  #ifdef VPX_MODE_COUNT
                    vp8_mv_cont_count[mv_contz][1]++;
  #endif
                    break;
                case ZERO4X4:
                    blockmv.as_int = 0;
  #ifdef VPX_MODE_COUNT
                    vp8_mv_cont_count[mv_contz][2]++;
  #endif
                    break;
                default:
                    break;
                }

                mbmi->need_to_clamp_mvs |= vp8_check_mv_bounds(&blockmv,
                                                          mb_to_left_edge,
                                                          mb_to_right_edge,
                                                          mb_to_top_edge,
                                                          mb_to_bottom_edge);

                {
                    /* Fill (uniform) modes, mvs of jth subset.
                     Must do it here because ensuing subsets can
                     refer back to us via "left" or "above". */
                    const unsigned char *fill_offset;
                    unsigned int fill_count = mbsplit_fill_count[s];

                    fill_offset = &mbsplit_fill_offset[s][(unsigned char)j * mbsplit_fill_count[s]];

                    do {
                        mi->bmi[ *fill_offset].mv.as_int = blockmv.as_int;
                        fill_offset++;
                    }while (--fill_count);
                }

            }
            while (++j < num_p);
        }

        mv->as_int = mi->bmi[15].mv.as_int;

        break;  /* done with SPLITMV */

        case NEARMV:
            mv->as_int = nearby.as_int;
            /* Clip "next_nearest" so that it does not extend to far out of image */
            vp8_clamp_mv(mv, mb_to_left_edge, mb_to_right_edge,
                         mb_to_top_edge, mb_to_bottom_edge);
            goto propagate_mv;

        case NEARESTMV:
            mv->as_int = nearest.as_int;
            /* Clip "next_nearest" so that it does not extend to far out of image */
            vp8_clamp_mv(mv, mb_to_left_edge, mb_to_right_edge,
                         mb_to_top_edge, mb_to_bottom_edge);
            goto propagate_mv;

        case ZEROMV:
            mv->as_int = 0;
            goto propagate_mv;

        case NEWMV:
            read_mv(bc, &mv->as_mv, (const MV_CONTEXT *) mvc);
            mv->as_mv.row += best_mv.as_mv.row;
            mv->as_mv.col += best_mv.as_mv.col;

            /* Don't need to check this on NEARMV and NEARESTMV modes
             * since those modes clamp the MV. The NEWMV mode does not,
             * so signal to the prediction stage whether special
             * handling may be required.
             */
            mbmi->need_to_clamp_mvs = vp8_check_mv_bounds(mv,
                                                      mb_to_left_edge,
                                                      mb_to_right_edge,
                                                      mb_to_top_edge,
                                                      mb_to_bottom_edge);

        propagate_mv:  /* same MV throughout */
#if CONFIG_ERROR_CONCEALMENT
            if(pbi->ec_enabled)
            {
                mi->bmi[ 0].mv.as_int =
                mi->bmi[ 1].mv.as_int =
                mi->bmi[ 2].mv.as_int =
                mi->bmi[ 3].mv.as_int =
                mi->bmi[ 4].mv.as_int =
                mi->bmi[ 5].mv.as_int =
                mi->bmi[ 6].mv.as_int =
                mi->bmi[ 7].mv.as_int =
                mi->bmi[ 8].mv.as_int =
                mi->bmi[ 9].mv.as_int =
                mi->bmi[10].mv.as_int =
                mi->bmi[11].mv.as_int =
                mi->bmi[12].mv.as_int =
                mi->bmi[13].mv.as_int =
                mi->bmi[14].mv.as_int =
                mi->bmi[15].mv.as_int = mv->as_int;
            }
#endif
            break;
        default:;
  #if CONFIG_DEBUG
            assert(0);
  #endif
        }
    }
    else
    {
        /* required for left and above block mv */
        mbmi->mv.as_int = 0;

        /* MB is intra coded */
        if ((mbmi->mode = (MB_PREDICTION_MODE) vp8_read_ymode(bc, pbi->common.fc.ymode_prob)) == B_PRED)
        {
            int j = 0;
            do
            {
                mi->bmi[j].as_mode = (B_PREDICTION_MODE)vp8_read_bmode(bc, pbi->common.fc.bmode_prob);
            }
            while (++j < 16);
        }

        mbmi->uv_mode = (MB_PREDICTION_MODE)vp8_read_uv_mode(bc, pbi->common.fc.uv_mode_prob);
    }

}

void vp8_decode_mode_mvs(VP8D_COMP *pbi)
{
    MODE_INFO *mi = pbi->common.mi;
    int mb_row = -1;

    mb_mode_mv_init(pbi);

    while (++mb_row < pbi->common.mb_rows)
    {
        int mb_col = -1;
        int mb_to_top_edge;
        int mb_to_bottom_edge;

        pbi->mb.mb_to_top_edge =
        mb_to_top_edge = -((mb_row * 16)) << 3;
        mb_to_top_edge -= LEFT_TOP_MARGIN;

        pbi->mb.mb_to_bottom_edge =
        mb_to_bottom_edge = ((pbi->common.mb_rows - 1 - mb_row) * 16) << 3;
        mb_to_bottom_edge += RIGHT_BOTTOM_MARGIN;

        while (++mb_col < pbi->common.mb_cols)
        {
#if CONFIG_ERROR_CONCEALMENT
            int mb_num = mb_row * pbi->common.mb_cols + mb_col;
#endif
            /*read_mb_modes_mv(pbi, xd->mode_info_context, &xd->mode_info_context->mbmi, mb_row, mb_col);*/
            if(pbi->common.frame_type == KEY_FRAME)
                vp8_kfread_modes(pbi, mi, mb_row, mb_col);
            else
                read_mb_modes_mv(pbi, mi, &mi->mbmi, mb_row, mb_col);

#if CONFIG_ERROR_CONCEALMENT
            /* look for corruption. set mvs_corrupt_from_mb to the current
             * mb_num if the frame is corrupt from this macroblock. */
            if (vp8dx_bool_error(&pbi->bc) && mb_num < pbi->mvs_corrupt_from_mb)
            {
                pbi->mvs_corrupt_from_mb = mb_num;
                /* no need to continue since the partition is corrupt from
                 * here on.
                 */
                return;
            }
#endif

            mi++;       /* next macroblock */
        }

        mi++;           /* skip left predictor each row */
    }
}

