/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#include <limits.h>
#include "vpx_config.h"
#include "onyx_int.h"
#include "modecosts.h"
#include "encodeintra.h"
#include "vp8/common/entropymode.h"
#include "pickinter.h"
#include "vp8/common/findnearmv.h"
#include "encodemb.h"
#include "vp8/common/reconinter.h"
#include "vp8/common/reconintra.h"
#include "vp8/common/reconintra4x4.h"
#include "vp8/common/variance.h"
#include "mcomp.h"
#include "rdopt.h"
#include "vpx_mem/vpx_mem.h"

#if CONFIG_RUNTIME_CPU_DETECT
#define IF_RTCD(x) (x)
#else
#define IF_RTCD(x)  NULL
#endif

extern int VP8_UVSSE(MACROBLOCK *x, const vp8_variance_rtcd_vtable_t *rtcd);

#ifdef SPEEDSTATS
extern unsigned int cnt_pm;
#endif

extern const int vp8_ref_frame_order[MAX_MODES];
extern const MB_PREDICTION_MODE vp8_mode_order[MAX_MODES];

extern unsigned int (*vp8_get4x4sse_cs)(unsigned char *src_ptr, int  source_stride, unsigned char *ref_ptr, int  recon_stride);
extern int vp8_cost_mv_ref(MB_PREDICTION_MODE m, const int near_mv_ref_ct[4]);


int vp8_skip_fractional_mv_step(MACROBLOCK *mb, BLOCK *b, BLOCKD *d,
                                int_mv *bestmv, int_mv *ref_mv,
                                int error_per_bit,
                                const vp8_variance_fn_ptr_t *vfp,
                                int *mvcost[2], int *distortion,
                                unsigned int *sse)
{
    (void) b;
    (void) d;
    (void) ref_mv;
    (void) error_per_bit;
    (void) vfp;
    (void) mvcost;
    (void) distortion;
    (void) sse;
    bestmv->as_mv.row <<= 3;
    bestmv->as_mv.col <<= 3;
    return 0;
}


static int get_inter_mbpred_error(MACROBLOCK *mb,
                                  const vp8_variance_fn_ptr_t *vfp,
                                  unsigned int *sse,
                                  int_mv this_mv)
{

    BLOCK *b = &mb->block[0];
    BLOCKD *d = &mb->e_mbd.block[0];
    unsigned char *what = (*(b->base_src) + b->src);
    int what_stride = b->src_stride;
    unsigned char *in_what = *(d->base_pre) + d->pre ;
    int in_what_stride = d->pre_stride;
    int xoffset = this_mv.as_mv.col & 7;
    int yoffset = this_mv.as_mv.row & 7;

    in_what += (this_mv.as_mv.row >> 3) * d->pre_stride + (this_mv.as_mv.col >> 3);

    if (xoffset | yoffset)
    {
        return vfp->svf(in_what, in_what_stride, xoffset, yoffset, what, what_stride, sse);
    }
    else
    {
        return vfp->vf(what, what_stride, in_what, in_what_stride, sse);
    }

}


static int get_prediction_error(BLOCK *be, BLOCKD *b, const vp8_variance_rtcd_vtable_t *rtcd)
{
    unsigned char *sptr;
    unsigned char *dptr;
    sptr = (*(be->base_src) + be->src);
    dptr = b->predictor;

    return VARIANCE_INVOKE(rtcd, get4x4sse_cs)(sptr, be->src_stride, dptr, 16);

}

static int pick_intra4x4block(
    const VP8_ENCODER_RTCD *rtcd,
    MACROBLOCK *x,
    int ib,
    B_PREDICTION_MODE *best_mode,
    unsigned int *mode_costs,

    int *bestrate,
    int *bestdistortion)
{

    BLOCKD *b = &x->e_mbd.block[ib];
    BLOCK *be = &x->block[ib];
    B_PREDICTION_MODE mode;
    int best_rd = INT_MAX;       // 1<<30
    int rate;
    int distortion;

    for (mode = B_DC_PRED; mode <= B_HE_PRED /*B_HU_PRED*/; mode++)
    {
        int this_rd;

        rate = mode_costs[mode];
        RECON_INVOKE(&rtcd->common->recon, intra4x4_predict)
                     (*(b->base_dst) + b->dst, b->dst_stride,
                      mode, b->predictor, 16);
        distortion = get_prediction_error(be, b, &rtcd->common->variance);
        this_rd = RDCOST(x->rdmult, x->rddiv, rate, distortion);

        if (this_rd < best_rd)
        {
            *bestrate = rate;
            *bestdistortion = distortion;
            best_rd = this_rd;
            *best_mode = mode;
        }
    }

    b->bmi.as_mode = (B_PREDICTION_MODE)(*best_mode);
    vp8_encode_intra4x4block(rtcd, x, ib);
    return best_rd;
}


static int pick_intra4x4mby_modes
(
    const VP8_ENCODER_RTCD *rtcd,
    MACROBLOCK *mb,
    int *Rate,
    int *best_dist
)
{
    MACROBLOCKD *const xd = &mb->e_mbd;
    int i;
    int cost = mb->mbmode_cost [xd->frame_type] [B_PRED];
    int error;
    int distortion = 0;
    unsigned int *bmode_costs;

    vp8_intra_prediction_down_copy(xd);

    bmode_costs = mb->inter_bmode_costs;

    for (i = 0; i < 16; i++)
    {
        MODE_INFO *const mic = xd->mode_info_context;
        const int mis = xd->mode_info_stride;

        B_PREDICTION_MODE UNINITIALIZED_IS_SAFE(best_mode);
        int UNINITIALIZED_IS_SAFE(r), UNINITIALIZED_IS_SAFE(d);

        if (mb->e_mbd.frame_type == KEY_FRAME)
        {
            const B_PREDICTION_MODE A = above_block_mode(mic, i, mis);
            const B_PREDICTION_MODE L = left_block_mode(mic, i);

            bmode_costs  = mb->bmode_costs[A][L];
        }


        pick_intra4x4block(rtcd, mb, i, &best_mode, bmode_costs, &r, &d);

        cost += r;
        distortion += d;
        mic->bmi[i].as_mode = best_mode;

        // Break out case where we have already exceeded best so far value
        // that was passed in
        if (distortion > *best_dist)
            break;
    }

    *Rate = cost;

    if (i == 16)
    {
        *best_dist = distortion;
        error = RDCOST(mb->rdmult, mb->rddiv, cost, distortion);
    }
    else
    {
        *best_dist = INT_MAX;
        error = INT_MAX;
    }

    return error;
}

static void pick_intra_mbuv_mode(MACROBLOCK *mb)
{

    MACROBLOCKD *x = &mb->e_mbd;
    unsigned char *uabove_row = x->dst.u_buffer - x->dst.uv_stride;
    unsigned char *vabove_row = x->dst.v_buffer - x->dst.uv_stride;
    unsigned char *usrc_ptr = (mb->block[16].src + *mb->block[16].base_src);
    unsigned char *vsrc_ptr = (mb->block[20].src + *mb->block[20].base_src);
    int uvsrc_stride = mb->block[16].src_stride;
    unsigned char uleft_col[8];
    unsigned char vleft_col[8];
    unsigned char utop_left = uabove_row[-1];
    unsigned char vtop_left = vabove_row[-1];
    int i, j;
    int expected_udc;
    int expected_vdc;
    int shift;
    int Uaverage = 0;
    int Vaverage = 0;
    int diff;
    int pred_error[4] = {0, 0, 0, 0}, best_error = INT_MAX;
    MB_PREDICTION_MODE UNINITIALIZED_IS_SAFE(best_mode);


    for (i = 0; i < 8; i++)
    {
        uleft_col[i] = x->dst.u_buffer [i* x->dst.uv_stride -1];
        vleft_col[i] = x->dst.v_buffer [i* x->dst.uv_stride -1];
    }

    if (!x->up_available && !x->left_available)
    {
        expected_udc = 128;
        expected_vdc = 128;
    }
    else
    {
        shift = 2;

        if (x->up_available)
        {

            for (i = 0; i < 8; i++)
            {
                Uaverage += uabove_row[i];
                Vaverage += vabove_row[i];
            }

            shift ++;

        }

        if (x->left_available)
        {
            for (i = 0; i < 8; i++)
            {
                Uaverage += uleft_col[i];
                Vaverage += vleft_col[i];
            }

            shift ++;

        }

        expected_udc = (Uaverage + (1 << (shift - 1))) >> shift;
        expected_vdc = (Vaverage + (1 << (shift - 1))) >> shift;
    }


    for (i = 0; i < 8; i++)
    {
        for (j = 0; j < 8; j++)
        {

            int predu = uleft_col[i] + uabove_row[j] - utop_left;
            int predv = vleft_col[i] + vabove_row[j] - vtop_left;
            int u_p, v_p;

            u_p = usrc_ptr[j];
            v_p = vsrc_ptr[j];

            if (predu < 0)
                predu = 0;

            if (predu > 255)
                predu = 255;

            if (predv < 0)
                predv = 0;

            if (predv > 255)
                predv = 255;


            diff = u_p - expected_udc;
            pred_error[DC_PRED] += diff * diff;
            diff = v_p - expected_vdc;
            pred_error[DC_PRED] += diff * diff;


            diff = u_p - uabove_row[j];
            pred_error[V_PRED] += diff * diff;
            diff = v_p - vabove_row[j];
            pred_error[V_PRED] += diff * diff;


            diff = u_p - uleft_col[i];
            pred_error[H_PRED] += diff * diff;
            diff = v_p - vleft_col[i];
            pred_error[H_PRED] += diff * diff;


            diff = u_p - predu;
            pred_error[TM_PRED] += diff * diff;
            diff = v_p - predv;
            pred_error[TM_PRED] += diff * diff;


        }

        usrc_ptr += uvsrc_stride;
        vsrc_ptr += uvsrc_stride;

        if (i == 3)
        {
            usrc_ptr = (mb->block[18].src + *mb->block[18].base_src);
            vsrc_ptr = (mb->block[22].src + *mb->block[22].base_src);
        }



    }


    for (i = DC_PRED; i <= TM_PRED; i++)
    {
        if (best_error > pred_error[i])
        {
            best_error = pred_error[i];
            best_mode = (MB_PREDICTION_MODE)i;
        }
    }


    mb->e_mbd.mode_info_context->mbmi.uv_mode = best_mode;

}

static void update_mvcount(VP8_COMP *cpi, MACROBLOCKD *xd, int_mv *best_ref_mv)
{
    /* Split MV modes currently not supported when RD is nopt enabled,
     * therefore, only need to modify MVcount in NEWMV mode. */
    if (xd->mode_info_context->mbmi.mode == NEWMV)
    {
        cpi->MVcount[0][mv_max+((xd->mode_info_context->mbmi.mv.as_mv.row -
                                      best_ref_mv->as_mv.row) >> 1)]++;
        cpi->MVcount[1][mv_max+((xd->mode_info_context->mbmi.mv.as_mv.col -
                                      best_ref_mv->as_mv.col) >> 1)]++;
    }
}


#if CONFIG_MULTI_RES_ENCODING
static
void get_lower_res_motion_info(VP8_COMP *cpi, MACROBLOCKD *xd, int *dissim,
                               int *parent_ref_frame,
                               MB_PREDICTION_MODE *parent_mode,
                               int_mv *parent_ref_mv, int mb_row, int mb_col)
{
    LOWER_RES_INFO* store_mode_info
                          = (LOWER_RES_INFO*)cpi->oxcf.mr_low_res_mode_info;
    unsigned int parent_mb_index;
    //unsigned int parent_mb_index = map_640x480_to_320x240[mb_row][mb_col];

    /* Consider different down_sampling_factor.  */
    {
        /* TODO: Removed the loop that supports special down_sampling_factor
         * such as 2, 4, 8. Will revisit it if needed.
         * Should also try using a look-up table to see if it helps
         * performance. */
        int round = cpi->oxcf.mr_down_sampling_factor.num/2;
        int parent_mb_row, parent_mb_col;

        parent_mb_row = (mb_row*cpi->oxcf.mr_down_sampling_factor.den+round)
                    /cpi->oxcf.mr_down_sampling_factor.num;
        parent_mb_col = (mb_col*cpi->oxcf.mr_down_sampling_factor.den+round)
                    /cpi->oxcf.mr_down_sampling_factor.num;
        parent_mb_index = parent_mb_row*cpi->mr_low_res_mb_cols + parent_mb_col;
    }

    /* Read lower-resolution mode & motion result from memory.*/
    *parent_ref_frame = store_mode_info[parent_mb_index].ref_frame;
    *parent_mode =  store_mode_info[parent_mb_index].mode;
    *dissim = store_mode_info[parent_mb_index].dissim;

    /* For highest-resolution encoder, adjust dissim value. Lower its quality
     * for good performance. */
    if (cpi->oxcf.mr_encoder_id == (cpi->oxcf.mr_total_resolutions - 1))
        *dissim>>=1;

    if(*parent_ref_frame != INTRA_FRAME)
    {
        /* Consider different down_sampling_factor.
         * The result can be rounded to be more precise, but it takes more time.
         */
        //int round = cpi->oxcf.mr_down_sampling_factor.den/2;
        (*parent_ref_mv).as_mv.row = store_mode_info[parent_mb_index].mv.as_mv.row
                                  *cpi->oxcf.mr_down_sampling_factor.num
                                  /cpi->oxcf.mr_down_sampling_factor.den;
        (*parent_ref_mv).as_mv.col = store_mode_info[parent_mb_index].mv.as_mv.col
                                  *cpi->oxcf.mr_down_sampling_factor.num
                                  /cpi->oxcf.mr_down_sampling_factor.den;

        vp8_clamp_mv2(parent_ref_mv, xd);
    }
}
#endif


void vp8_pick_inter_mode(VP8_COMP *cpi, MACROBLOCK *x, int recon_yoffset,
                         int recon_uvoffset, int *returnrate,
                         int *returndistortion, int *returnintra, int mb_row,
                         int mb_col)
{
    BLOCK *b = &x->block[0];
    BLOCKD *d = &x->e_mbd.block[0];
    MACROBLOCKD *xd = &x->e_mbd;
    MB_MODE_INFO best_mbmode;

    int_mv best_ref_mv_sb[2];
    int_mv mode_mv_sb[2][MB_MODE_COUNT];
    int_mv best_ref_mv;
    int_mv *mode_mv;
    MB_PREDICTION_MODE this_mode;
    int num00;
    int mdcounts[4];
    int best_rd = INT_MAX; // 1 << 30;
    int best_intra_rd = INT_MAX;
    int mode_index;
    int rate;
    int rate2;
    int distortion2;
    int bestsme = INT_MAX;
    int best_mode_index = 0;
    unsigned int sse = INT_MAX, best_sse = INT_MAX;

    int_mv mvp;

    int near_sadidx[8] = {0, 1, 2, 3, 4, 5, 6, 7};
    int saddone=0;
    int sr=0;    //search range got from mv_pred(). It uses step_param levels. (0-7)

    unsigned char *plane[4][3];
    int ref_frame_map[4];
    int sign_bias = 0;

    int have_subp_search = cpi->sf.half_pixel_search;  /* In real-time mode,
                                       when Speed >= 15, no sub-pixel search. */

#if CONFIG_MULTI_RES_ENCODING
    int dissim = INT_MAX;
    int parent_ref_frame = 0;
    int_mv parent_ref_mv;
    MB_PREDICTION_MODE parent_mode = 0;

    if (cpi->oxcf.mr_encoder_id)
        get_lower_res_motion_info(cpi, xd, &dissim, &parent_ref_frame,
                                  &parent_mode, &parent_ref_mv, mb_row, mb_col);
#endif

    mode_mv = mode_mv_sb[sign_bias];
    best_ref_mv.as_int = 0;
    vpx_memset(mode_mv_sb, 0, sizeof(mode_mv_sb));
    vpx_memset(&best_mbmode, 0, sizeof(best_mbmode));

    /* Setup search priorities */
    get_reference_search_order(cpi, ref_frame_map);

    /* Check to see if there is at least 1 valid reference frame that we need
     * to calculate near_mvs.
     */
    if (ref_frame_map[1] > 0)
    {
        sign_bias = vp8_find_near_mvs_bias(&x->e_mbd,
                                           x->e_mbd.mode_info_context,
                                           mode_mv_sb,
                                           best_ref_mv_sb,
                                           mdcounts,
                                           ref_frame_map[1],
                                           cpi->common.ref_frame_sign_bias);

        mode_mv = mode_mv_sb[sign_bias];
        best_ref_mv.as_int = best_ref_mv_sb[sign_bias].as_int;
    }

    get_predictor_pointers(cpi, plane, recon_yoffset, recon_uvoffset);

    cpi->mbs_tested_so_far++; // Count of the number of MBs tested so far this frame

    *returnintra = INT_MAX;
    x->skip = 0;

    x->e_mbd.mode_info_context->mbmi.ref_frame = INTRA_FRAME;

    // if we encode a new mv this is important
    // find the best new motion vector
    for (mode_index = 0; mode_index < MAX_MODES; mode_index++)
    {
        int frame_cost;
        int this_rd = INT_MAX;
        int this_ref_frame = ref_frame_map[vp8_ref_frame_order[mode_index]];

        if (best_rd <= cpi->rd_threshes[mode_index])
            continue;

        if (this_ref_frame < 0)
            continue;

        x->e_mbd.mode_info_context->mbmi.ref_frame = this_ref_frame;

#if CONFIG_MULTI_RES_ENCODING
        if (cpi->oxcf.mr_encoder_id)
        {
            /* If parent MB is intra, child MB is intra. */
            if (!parent_ref_frame && this_ref_frame)
                continue;

            /* If parent MB is inter, and it is unlikely there are multiple
             * objects in parent MB, we use parent ref frame as child MB's
             * ref frame. */
            if (parent_ref_frame && dissim < 8
                && parent_ref_frame != this_ref_frame)
                continue;
        }
#endif

        // everything but intra
        if (x->e_mbd.mode_info_context->mbmi.ref_frame)
        {
            x->e_mbd.pre.y_buffer = plane[this_ref_frame][0];
            x->e_mbd.pre.u_buffer = plane[this_ref_frame][1];
            x->e_mbd.pre.v_buffer = plane[this_ref_frame][2];

            if (sign_bias != cpi->common.ref_frame_sign_bias[this_ref_frame])
            {
                sign_bias = cpi->common.ref_frame_sign_bias[this_ref_frame];
                mode_mv = mode_mv_sb[sign_bias];
                best_ref_mv.as_int = best_ref_mv_sb[sign_bias].as_int;
            }

#if CONFIG_MULTI_RES_ENCODING
            if (cpi->oxcf.mr_encoder_id)
            {
                if (vp8_mode_order[mode_index] == NEARESTMV &&
                    mode_mv[NEARESTMV].as_int ==0)
                    continue;
                if (vp8_mode_order[mode_index] == NEARMV &&
                    mode_mv[NEARMV].as_int ==0)
                    continue;

                if (vp8_mode_order[mode_index] == NEWMV && parent_mode == ZEROMV
                    && best_ref_mv.as_int==0) //&& dissim==0
                    continue;
                else if(vp8_mode_order[mode_index] == NEWMV && dissim==0
                    && best_ref_mv.as_int==parent_ref_mv.as_int)
                    continue;
            }
#endif
        }

        /* Check to see if the testing frequency for this mode is at its max
         * If so then prevent it from being tested and increase the threshold
         * for its testing */
        if (cpi->mode_test_hit_counts[mode_index] &&
                                         (cpi->mode_check_freq[mode_index] > 1))
        {
            if (cpi->mbs_tested_so_far <= (cpi->mode_check_freq[mode_index] *
                                         cpi->mode_test_hit_counts[mode_index]))
            {
                /* Increase the threshold for coding this mode to make it less
                 * likely to be chosen */
                cpi->rd_thresh_mult[mode_index] += 4;

                if (cpi->rd_thresh_mult[mode_index] > MAX_THRESHMULT)
                    cpi->rd_thresh_mult[mode_index] = MAX_THRESHMULT;

                cpi->rd_threshes[mode_index] =
                                 (cpi->rd_baseline_thresh[mode_index] >> 7) *
                                 cpi->rd_thresh_mult[mode_index];
                continue;
            }
        }

        /* We have now reached the point where we are going to test the current
         * mode so increment the counter for the number of times it has been
         * tested */
        cpi->mode_test_hit_counts[mode_index] ++;

        rate2 = 0;
        distortion2 = 0;

        this_mode = vp8_mode_order[mode_index];

        x->e_mbd.mode_info_context->mbmi.mode = this_mode;
        x->e_mbd.mode_info_context->mbmi.uv_mode = DC_PRED;

        /* Work out the cost assosciated with selecting the reference frame */
        frame_cost =
            x->e_mbd.ref_frame_cost[x->e_mbd.mode_info_context->mbmi.ref_frame];
        rate2 += frame_cost;

        /* Only consider ZEROMV/ALTREF_FRAME for alt ref frame,
         * unless ARNR filtering is enabled in which case we want
         * an unfiltered alternative */
        if (cpi->is_src_frame_alt_ref && (cpi->oxcf.arnr_max_frames == 0))
        {
            if (this_mode != ZEROMV ||
                x->e_mbd.mode_info_context->mbmi.ref_frame != ALTREF_FRAME)
                continue;
        }

        switch (this_mode)
        {
        case B_PRED:
            /* Pass best so far to pick_intra4x4mby_modes to use as breakout */
            distortion2 = best_sse;
            pick_intra4x4mby_modes(IF_RTCD(&cpi->rtcd), x, &rate, &distortion2);

            if (distortion2 == INT_MAX)
            {
                this_rd = INT_MAX;
            }
            else
            {
                rate2 += rate;
                distortion2 = VARIANCE_INVOKE
                                (&cpi->common.rtcd.variance, var16x16)(
                                    *(b->base_src), b->src_stride,
                                    x->e_mbd.predictor, 16, &sse);
                this_rd = RDCOST(x->rdmult, x->rddiv, rate2, distortion2);

                if (this_rd < best_intra_rd)
                {
                    best_intra_rd = this_rd;
                    *returnintra = distortion2;
                }
            }

            break;

        case SPLITMV:

            // Split MV modes currently not supported when RD is nopt enabled.
            break;

        case DC_PRED:
        case V_PRED:
        case H_PRED:
        case TM_PRED:
            RECON_INVOKE(&cpi->common.rtcd.recon, build_intra_predictors_mby)
                (&x->e_mbd);
            distortion2 = VARIANCE_INVOKE(&cpi->common.rtcd.variance, var16x16)
                                          (*(b->base_src), b->src_stride,
                                          x->e_mbd.predictor, 16, &sse);
            rate2 += x->mbmode_cost[x->e_mbd.frame_type][x->e_mbd.mode_info_context->mbmi.mode];
            this_rd = RDCOST(x->rdmult, x->rddiv, rate2, distortion2);

            if (this_rd < best_intra_rd)
            {
                best_intra_rd = this_rd;
                *returnintra = distortion2;
            }
            break;

        case NEWMV:
        {
            int thissme;
            int step_param;
            int further_steps;
            int n = 0;
            int sadpb = x->sadperbit16;
            int_mv mvp_full;

            int col_min = ((best_ref_mv.as_mv.col+7)>>3) - MAX_FULL_PEL_VAL;
            int row_min = ((best_ref_mv.as_mv.row+7)>>3) - MAX_FULL_PEL_VAL;
            int col_max = (best_ref_mv.as_mv.col>>3)
                         + MAX_FULL_PEL_VAL;
            int row_max = (best_ref_mv.as_mv.row>>3)
                         + MAX_FULL_PEL_VAL;

            int tmp_col_min = x->mv_col_min;
            int tmp_col_max = x->mv_col_max;
            int tmp_row_min = x->mv_row_min;
            int tmp_row_max = x->mv_row_max;

            int speed_adjust = (cpi->Speed > 5) ? ((cpi->Speed >= 8)? 3 : 2) : 1;

            // Further step/diamond searches as necessary
            step_param = cpi->sf.first_step + speed_adjust;

#if CONFIG_MULTI_RES_ENCODING
            if (cpi->oxcf.mr_encoder_id)
            {
                // Use parent MV as predictor. Adjust search range accordingly.
                mvp.as_int = parent_ref_mv.as_int;
                mvp_full.as_mv.col = parent_ref_mv.as_mv.col>>3;
                mvp_full.as_mv.row = parent_ref_mv.as_mv.row>>3;

                if(dissim <=32) step_param += 3;
                else if(dissim <=128) step_param += 2;
                else step_param += 1;
            }else
#endif
            {
                if(cpi->sf.improved_mv_pred)
                {
                    if(!saddone)
                    {
                        vp8_cal_sad(cpi,xd,x, recon_yoffset ,&near_sadidx[0] );
                        saddone = 1;
                    }

                    vp8_mv_pred(cpi, &x->e_mbd, x->e_mbd.mode_info_context,
                                &mvp,x->e_mbd.mode_info_context->mbmi.ref_frame,
                                cpi->common.ref_frame_sign_bias, &sr,
                                &near_sadidx[0]);

                    sr += speed_adjust;
                    //adjust search range according to sr from mv prediction
                    if(sr > step_param)
                        step_param = sr;

                    mvp_full.as_mv.col = mvp.as_mv.col>>3;
                    mvp_full.as_mv.row = mvp.as_mv.row>>3;
                }else
                {
                    mvp.as_int = best_ref_mv.as_int;
                    mvp_full.as_mv.col = best_ref_mv.as_mv.col>>3;
                    mvp_full.as_mv.row = best_ref_mv.as_mv.row>>3;
                }
            }

#if CONFIG_MULTI_RES_ENCODING
            if (cpi->oxcf.mr_encoder_id && dissim <= 2 &&
                MAX(abs(best_ref_mv.as_mv.row - parent_ref_mv.as_mv.row),
                    abs(best_ref_mv.as_mv.col - parent_ref_mv.as_mv.col)) <= 4)
            {
                d->bmi.mv.as_int = mvp_full.as_int;
                mode_mv[NEWMV].as_int = mvp_full.as_int;

                cpi->find_fractional_mv_step(x, b, d, &d->bmi.mv, &best_ref_mv,
                                             x->errorperbit,
                                             &cpi->fn_ptr[BLOCK_16X16],
                                             cpi->mb.mvcost,
                                             &distortion2,&sse);
            }else
#endif
            {
                /* Get intersection of UMV window and valid MV window to
                 * reduce # of checks in diamond search. */
                if (x->mv_col_min < col_min )
                    x->mv_col_min = col_min;
                if (x->mv_col_max > col_max )
                    x->mv_col_max = col_max;
                if (x->mv_row_min < row_min )
                    x->mv_row_min = row_min;
                if (x->mv_row_max > row_max )
                    x->mv_row_max = row_max;

                further_steps = (cpi->Speed >= 8)?
                           0: (cpi->sf.max_step_search_steps - 1 - step_param);

                if (cpi->sf.search_method == HEX)
                {
#if CONFIG_MULTI_RES_ENCODING
                /* TODO: In higher-res pick_inter_mode, step_param is used to
                 * modify hex search range. Here, set step_param to 0 not to
                 * change the behavior in lowest-resolution encoder.
                 * Will improve it later.
                 */
                if (!cpi->oxcf.mr_encoder_id)
                    step_param = 0;
#endif
                    bestsme = vp8_hex_search(x, b, d, &mvp_full, &d->bmi.mv,
                                          step_param, sadpb,
                                          &cpi->fn_ptr[BLOCK_16X16],
                                          x->mvsadcost, x->mvcost, &best_ref_mv);
                    mode_mv[NEWMV].as_int = d->bmi.mv.as_int;
                }
                else
                {
                    bestsme = cpi->diamond_search_sad(x, b, d, &mvp_full,
                                          &d->bmi.mv, step_param, sadpb, &num00,
                                          &cpi->fn_ptr[BLOCK_16X16],
                                          x->mvcost, &best_ref_mv);
                    mode_mv[NEWMV].as_int = d->bmi.mv.as_int;

                    // Further step/diamond searches as necessary
                    n = 0;
                    //further_steps = (cpi->sf.max_step_search_steps - 1) - step_param;

                    n = num00;
                    num00 = 0;

                    while (n < further_steps)
                    {
                        n++;

                        if (num00)
                            num00--;
                        else
                        {
                            thissme =
                            cpi->diamond_search_sad(x, b, d, &mvp_full,
                                                    &d->bmi.mv,
                                                    step_param + n,
                                                    sadpb, &num00,
                                                    &cpi->fn_ptr[BLOCK_16X16],
                                                    x->mvcost, &best_ref_mv);
                            if (thissme < bestsme)
                            {
                                bestsme = thissme;
                                mode_mv[NEWMV].as_int = d->bmi.mv.as_int;
                            }
                            else
                            {
                                d->bmi.mv.as_int = mode_mv[NEWMV].as_int;
                            }
                        }
                    }
                }

                x->mv_col_min = tmp_col_min;
                x->mv_col_max = tmp_col_max;
                x->mv_row_min = tmp_row_min;
                x->mv_row_max = tmp_row_max;

                if (bestsme < INT_MAX)
                    cpi->find_fractional_mv_step(x, b, d, &d->bmi.mv,
                                             &best_ref_mv, x->errorperbit,
                                             &cpi->fn_ptr[BLOCK_16X16],
                                             cpi->mb.mvcost,
                                             &distortion2,&sse);
            }

            mode_mv[NEWMV].as_int = d->bmi.mv.as_int;

            // mv cost;
            rate2 += vp8_mv_bit_cost(&mode_mv[NEWMV], &best_ref_mv,
                                     cpi->mb.mvcost, 128);
        }

        case NEARESTMV:
        case NEARMV:

            if (mode_mv[this_mode].as_int == 0)
                continue;

        case ZEROMV:

            /* Trap vectors that reach beyond the UMV borders
             * Note that ALL New MV, Nearest MV Near MV and Zero MV code drops
             * through to this point because of the lack of break statements
             * in the previous two cases.
             */
            if (((mode_mv[this_mode].as_mv.row >> 3) < x->mv_row_min) ||
                ((mode_mv[this_mode].as_mv.row >> 3) > x->mv_row_max) ||
                ((mode_mv[this_mode].as_mv.col >> 3) < x->mv_col_min) ||
                ((mode_mv[this_mode].as_mv.col >> 3) > x->mv_col_max))
                continue;

            rate2 += vp8_cost_mv_ref(this_mode, mdcounts);
            x->e_mbd.mode_info_context->mbmi.mv.as_int =
                                                    mode_mv[this_mode].as_int;

            /* Exit early and don't compute the distortion if this macroblock
             * is marked inactive. */
            if (cpi->active_map_enabled && x->active_ptr[0] == 0)
            {
                sse = 0;
                distortion2 = 0;
                x->skip = 1;
                break;
            }

            if((this_mode != NEWMV) ||
                !(have_subp_search) || cpi->common.full_pixel==1)
                distortion2 = get_inter_mbpred_error(x,
                                                     &cpi->fn_ptr[BLOCK_16X16],
                                                     &sse, mode_mv[this_mode]);

            this_rd = RDCOST(x->rdmult, x->rddiv, rate2, distortion2);

            if (sse < x->encode_breakout)
            {
                // Check u and v to make sure skip is ok
                int sse2 = 0;

                sse2 = VP8_UVSSE(x, IF_RTCD(&cpi->common.rtcd.variance));

                if (sse2 * 2 < x->encode_breakout)
                    x->skip = 1;
                else
                    x->skip = 0;
            }

            break;
        default:
            break;
        }

        if (this_rd < best_rd || x->skip)
        {
            // Note index of best mode
            best_mode_index = mode_index;

            *returnrate = rate2;
            *returndistortion = distortion2;
            best_sse = sse;
            best_rd = this_rd;
            vpx_memcpy(&best_mbmode, &x->e_mbd.mode_info_context->mbmi,
                       sizeof(MB_MODE_INFO));

            /* Testing this mode gave rise to an improvement in best error
             * score. Lower threshold a bit for next time
             */
            cpi->rd_thresh_mult[mode_index] =
                     (cpi->rd_thresh_mult[mode_index] >= (MIN_THRESHMULT + 2)) ?
                     cpi->rd_thresh_mult[mode_index] - 2 : MIN_THRESHMULT;
            cpi->rd_threshes[mode_index] =
                                   (cpi->rd_baseline_thresh[mode_index] >> 7) *
                                   cpi->rd_thresh_mult[mode_index];
        }

        /* If the mode did not help improve the best error case then raise the
         * threshold for testing that mode next time around.
         */
        else
        {
            cpi->rd_thresh_mult[mode_index] += 4;

            if (cpi->rd_thresh_mult[mode_index] > MAX_THRESHMULT)
                cpi->rd_thresh_mult[mode_index] = MAX_THRESHMULT;

            cpi->rd_threshes[mode_index] =
                         (cpi->rd_baseline_thresh[mode_index] >> 7) *
                         cpi->rd_thresh_mult[mode_index];
        }

        if (x->skip)
            break;
    }

    // Reduce the activation RD thresholds for the best choice mode
    if ((cpi->rd_baseline_thresh[best_mode_index] > 0) && (cpi->rd_baseline_thresh[best_mode_index] < (INT_MAX >> 2)))
    {
        int best_adjustment = (cpi->rd_thresh_mult[best_mode_index] >> 3);

        cpi->rd_thresh_mult[best_mode_index] =
                        (cpi->rd_thresh_mult[best_mode_index]
                        >= (MIN_THRESHMULT + best_adjustment)) ?
                        cpi->rd_thresh_mult[best_mode_index] - best_adjustment :
                        MIN_THRESHMULT;
        cpi->rd_threshes[best_mode_index] =
                        (cpi->rd_baseline_thresh[best_mode_index] >> 7) *
                        cpi->rd_thresh_mult[best_mode_index];
    }


    {
        int this_rdbin = (*returndistortion >> 7);

        if (this_rdbin >= 1024)
        {
            this_rdbin = 1023;
        }

        cpi->error_bins[this_rdbin] ++;
    }

    if (cpi->is_src_frame_alt_ref &&
        (best_mbmode.mode != ZEROMV || best_mbmode.ref_frame != ALTREF_FRAME))
    {
        x->e_mbd.mode_info_context->mbmi.mode = ZEROMV;
        x->e_mbd.mode_info_context->mbmi.ref_frame = ALTREF_FRAME;
        x->e_mbd.mode_info_context->mbmi.mv.as_int = 0;
        x->e_mbd.mode_info_context->mbmi.uv_mode = DC_PRED;
        x->e_mbd.mode_info_context->mbmi.mb_skip_coeff =
                                        (cpi->common.mb_no_coeff_skip);
        x->e_mbd.mode_info_context->mbmi.partitioning = 0;

        return;
    }

    /* set to the best mb mode, this copy can be skip if x->skip since it
     * already has the right content */
    if (!x->skip)
        vpx_memcpy(&x->e_mbd.mode_info_context->mbmi, &best_mbmode,
                   sizeof(MB_MODE_INFO));

    if (best_mbmode.mode <= B_PRED)
    {
        /* set mode_info_context->mbmi.uv_mode */
        pick_intra_mbuv_mode(x);
    }

    if (sign_bias
      != cpi->common.ref_frame_sign_bias[xd->mode_info_context->mbmi.ref_frame])
        best_ref_mv.as_int = best_ref_mv_sb[!sign_bias].as_int;

    update_mvcount(cpi, &x->e_mbd, &best_ref_mv);
}


void vp8_pick_intra_mode(VP8_COMP *cpi, MACROBLOCK *x, int *rate_)
{
    int error4x4, error16x16 = INT_MAX;
    int rate, best_rate = 0, distortion, best_sse;
    MB_PREDICTION_MODE mode, best_mode = DC_PRED;
    int this_rd;
    unsigned int sse;
    BLOCK *b = &x->block[0];

    x->e_mbd.mode_info_context->mbmi.ref_frame = INTRA_FRAME;

    pick_intra_mbuv_mode(x);

    for (mode = DC_PRED; mode <= TM_PRED; mode ++)
    {
        x->e_mbd.mode_info_context->mbmi.mode = mode;
        RECON_INVOKE(&cpi->common.rtcd.recon, build_intra_predictors_mby)
            (&x->e_mbd);
        distortion = VARIANCE_INVOKE(&cpi->common.rtcd.variance, var16x16)
            (*(b->base_src), b->src_stride, x->e_mbd.predictor, 16, &sse);
        rate = x->mbmode_cost[x->e_mbd.frame_type][mode];
        this_rd = RDCOST(x->rdmult, x->rddiv, rate, distortion);

        if (error16x16 > this_rd)
        {
            error16x16 = this_rd;
            best_mode = mode;
            best_sse = sse;
            best_rate = rate;
        }
    }
    x->e_mbd.mode_info_context->mbmi.mode = best_mode;

    error4x4 = pick_intra4x4mby_modes(IF_RTCD(&cpi->rtcd), x, &rate,
                                      &best_sse);
    if (error4x4 < error16x16)
    {
        x->e_mbd.mode_info_context->mbmi.mode = B_PRED;
        best_rate = rate;
    }

    *rate_ = best_rate;
}
