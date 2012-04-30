/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#include <stdio.h>
#include <math.h>
#include <limits.h>
#include <assert.h>
#include "vp8/common/pragmas.h"

#include "tokenize.h"
#include "treewriter.h"
#include "onyx_int.h"
#include "modecosts.h"
#include "encodeintra.h"
#include "vp8/common/entropymode.h"
#include "vp8/common/reconinter.h"
#include "vp8/common/reconintra.h"
#include "vp8/common/reconintra4x4.h"
#include "vp8/common/findnearmv.h"
#include "encodemb.h"
#include "quantize.h"
#include "vp8/common/idct.h"
#include "vp8/common/variance.h"
#include "mcomp.h"
#include "rdopt.h"
#include "vpx_mem/vpx_mem.h"
#include "dct.h"
#include "vp8/common/systemdependent.h"

#if CONFIG_RUNTIME_CPU_DETECT
#define IF_RTCD(x)  (x)
#else
#define IF_RTCD(x)  NULL
#endif


extern void vp8_update_zbin_extra(VP8_COMP *cpi, MACROBLOCK *x);

#define MAXF(a,b)            (((a) > (b)) ? (a) : (b))

static const int auto_speed_thresh[17] =
{
    1000,
    200,
    150,
    130,
    150,
    125,
    120,
    115,
    115,
    115,
    115,
    115,
    115,
    115,
    115,
    115,
    105
};

const MB_PREDICTION_MODE vp8_mode_order[MAX_MODES] =
{
    ZEROMV,
    DC_PRED,

    NEARESTMV,
    NEARMV,

    ZEROMV,
    NEARESTMV,

    ZEROMV,
    NEARESTMV,

    NEARMV,
    NEARMV,

    V_PRED,
    H_PRED,
    TM_PRED,

    NEWMV,
    NEWMV,
    NEWMV,

    SPLITMV,
    SPLITMV,
    SPLITMV,

    B_PRED,
};

/* This table determines the search order in reference frame priority order,
 * which may not necessarily match INTRA,LAST,GOLDEN,ARF
 */
const int vp8_ref_frame_order[MAX_MODES] =
{
    1,
    0,

    1,
    1,

    2,
    2,

    3,
    3,

    2,
    3,

    0,
    0,
    0,

    1,
    2,
    3,

    1,
    2,
    3,

    0,
};

static void fill_token_costs(
    unsigned int c      [BLOCK_TYPES] [COEF_BANDS] [PREV_COEF_CONTEXTS] [MAX_ENTROPY_TOKENS],
    const vp8_prob p    [BLOCK_TYPES] [COEF_BANDS] [PREV_COEF_CONTEXTS] [ENTROPY_NODES]
)
{
    int i, j, k;


    for (i = 0; i < BLOCK_TYPES; i++)
        for (j = 0; j < COEF_BANDS; j++)
            for (k = 0; k < PREV_COEF_CONTEXTS; k++)

                vp8_cost_tokens((int *)(c [i][j][k]), p [i][j][k], vp8_coef_tree);

}

static int rd_iifactor [ 32 ] =  {    4,   4,   3,   2,   1,   0,   0,   0,
                                      0,   0,   0,   0,   0,   0,   0,   0,
                                      0,   0,   0,   0,   0,   0,   0,   0,
                                      0,   0,   0,   0,   0,   0,   0,   0,
                                 };

/* values are now correlated to quantizer */
static int sad_per_bit16lut[QINDEX_RANGE] =
{
    2,  2,  2,  2,  2,  2,  2,  2,
    2,  2,  2,  2,  2,  2,  2,  2,
    3,  3,  3,  3,  3,  3,  3,  3,
    3,  3,  3,  3,  3,  3,  4,  4,
    4,  4,  4,  4,  4,  4,  4,  4,
    4,  4,  5,  5,  5,  5,  5,  5,
    5,  5,  5,  5,  5,  5,  6,  6,
    6,  6,  6,  6,  6,  6,  6,  6,
    6,  6,  7,  7,  7,  7,  7,  7,
    7,  7,  7,  7,  7,  7,  8,  8,
    8,  8,  8,  8,  8,  8,  8,  8,
    8,  8,  9,  9,  9,  9,  9,  9,
    9,  9,  9,  9,  9,  9,  10, 10,
    10, 10, 10, 10, 10, 10, 11, 11,
    11, 11, 11, 11, 12, 12, 12, 12,
    12, 12, 13, 13, 13, 13, 14, 14
};
static int sad_per_bit4lut[QINDEX_RANGE] =
{
    2,  2,  2,  2,  2,  2,  3,  3,
    3,  3,  3,  3,  3,  3,  3,  3,
    3,  3,  3,  3,  4,  4,  4,  4,
    4,  4,  4,  4,  4,  4,  5,  5,
    5,  5,  5,  5,  6,  6,  6,  6,
    6,  6,  6,  6,  6,  6,  6,  6,
    7,  7,  7,  7,  7,  7,  7,  7,
    7,  7,  7,  7,  7,  8,  8,  8,
    8,  8,  9,  9,  9,  9,  9,  9,
    10, 10, 10, 10, 10, 10, 10, 10,
    11, 11, 11, 11, 11, 11, 11, 11,
    12, 12, 12, 12, 12, 12, 12, 12,
    13, 13, 13, 13, 13, 13, 13, 14,
    14, 14, 14, 14, 15, 15, 15, 15,
    16, 16, 16, 16, 17, 17, 17, 18,
    18, 18, 19, 19, 19, 20, 20, 20,
};

void vp8cx_initialize_me_consts(VP8_COMP *cpi, int QIndex)
{
    cpi->mb.sadperbit16 =  sad_per_bit16lut[QIndex];
    cpi->mb.sadperbit4  =  sad_per_bit4lut[QIndex];
}

void vp8_initialize_rd_consts(VP8_COMP *cpi, int Qvalue)
{
    int q;
    int i;
    double capped_q = (Qvalue < 160) ? (double)Qvalue : 160.0;
    double rdconst = 2.70;

    vp8_clear_system_state();  //__asm emms;

    // Further tests required to see if optimum is different
    // for key frames, golden frames and arf frames.
    // if (cpi->common.refresh_golden_frame ||
    //     cpi->common.refresh_alt_ref_frame)
    cpi->RDMULT = (int)(rdconst * (capped_q * capped_q));

    // Extend rate multiplier along side quantizer zbin increases
    if (cpi->zbin_over_quant  > 0)
    {
        double oq_factor;
        double modq;

        // Experimental code using the same basic equation as used for Q above
        // The units of cpi->zbin_over_quant are 1/128 of Q bin size
        oq_factor = 1.0 + ((double)0.0015625 * cpi->zbin_over_quant);
        modq = (int)((double)capped_q * oq_factor);
        cpi->RDMULT = (int)(rdconst * (modq * modq));
    }

    if (cpi->pass == 2 && (cpi->common.frame_type != KEY_FRAME))
    {
        if (cpi->twopass.next_iiratio > 31)
            cpi->RDMULT += (cpi->RDMULT * rd_iifactor[31]) >> 4;
        else
            cpi->RDMULT +=
                (cpi->RDMULT * rd_iifactor[cpi->twopass.next_iiratio]) >> 4;
    }

    cpi->mb.errorperbit = (cpi->RDMULT / 110);
    cpi->mb.errorperbit += (cpi->mb.errorperbit==0);

    vp8_set_speed_features(cpi);

    q = (int)pow(Qvalue, 1.25);

    if (q < 8)
        q = 8;

    if (cpi->RDMULT > 1000)
    {
        cpi->RDDIV = 1;
        cpi->RDMULT /= 100;

        for (i = 0; i < MAX_MODES; i++)
        {
            if (cpi->sf.thresh_mult[i] < INT_MAX)
            {
                cpi->rd_threshes[i] = cpi->sf.thresh_mult[i] * q / 100;
            }
            else
            {
                cpi->rd_threshes[i] = INT_MAX;
            }

            cpi->rd_baseline_thresh[i] = cpi->rd_threshes[i];
        }
    }
    else
    {
        cpi->RDDIV = 100;

        for (i = 0; i < MAX_MODES; i++)
        {
            if (cpi->sf.thresh_mult[i] < (INT_MAX / q))
            {
                cpi->rd_threshes[i] = cpi->sf.thresh_mult[i] * q;
            }
            else
            {
                cpi->rd_threshes[i] = INT_MAX;
            }

            cpi->rd_baseline_thresh[i] = cpi->rd_threshes[i];
        }
    }

    {
      // build token cost array for the type of frame we have now
      FRAME_CONTEXT *l = &cpi->lfc_n;

      if(cpi->common.refresh_alt_ref_frame)
          l = &cpi->lfc_a;
      else if(cpi->common.refresh_golden_frame)
          l = &cpi->lfc_g;

      fill_token_costs(
          cpi->mb.token_costs,
          (const vp8_prob( *)[8][3][11]) l->coef_probs
      );
      /*
      fill_token_costs(
          cpi->mb.token_costs,
          (const vp8_prob( *)[8][3][11]) cpi->common.fc.coef_probs);
      */


      // TODO make these mode costs depend on last,alt or gold too.  (jbb)
      vp8_init_mode_costs(cpi);

      // TODO figure onnnnuut why making mv cost frame type dependent didn't help (jbb)
      //vp8_build_component_cost_table(cpi->mb.mvcost, (const MV_CONTEXT *) l->mvc, flags);

    }

}

void vp8_auto_select_speed(VP8_COMP *cpi)
{
    int milliseconds_for_compress = (int)(1000000 / cpi->frame_rate);

    milliseconds_for_compress = milliseconds_for_compress * (16 - cpi->oxcf.cpu_used) / 16;

#if 0

    if (0)
    {
        FILE *f;

        f = fopen("speed.stt", "a");
        fprintf(f, " %8ld %10ld %10ld %10ld\n",
                cpi->common.current_video_frame, cpi->Speed, milliseconds_for_compress, cpi->avg_pick_mode_time);
        fclose(f);
    }

#endif

    /*
    // this is done during parameter valid check
    if( cpi->oxcf.cpu_used > 16)
        cpi->oxcf.cpu_used = 16;
    if( cpi->oxcf.cpu_used < -16)
        cpi->oxcf.cpu_used = -16;
    */

    if (cpi->avg_pick_mode_time < milliseconds_for_compress && (cpi->avg_encode_time - cpi->avg_pick_mode_time) < milliseconds_for_compress)
    {
        if (cpi->avg_pick_mode_time == 0)
        {
            cpi->Speed = 4;
        }
        else
        {
            if (milliseconds_for_compress * 100 < cpi->avg_encode_time * 95)
            {
                cpi->Speed          += 2;
                cpi->avg_pick_mode_time = 0;
                cpi->avg_encode_time = 0;

                if (cpi->Speed > 16)
                {
                    cpi->Speed = 16;
                }
            }

            if (milliseconds_for_compress * 100 > cpi->avg_encode_time * auto_speed_thresh[cpi->Speed])
            {
                cpi->Speed          -= 1;
                cpi->avg_pick_mode_time = 0;
                cpi->avg_encode_time = 0;

                // In real-time mode, cpi->speed is in [4, 16].
                if (cpi->Speed < 4)        //if ( cpi->Speed < 0 )
                {
                    cpi->Speed = 4;        //cpi->Speed = 0;
                }
            }
        }
    }
    else
    {
        cpi->Speed += 4;

        if (cpi->Speed > 16)
            cpi->Speed = 16;


        cpi->avg_pick_mode_time = 0;
        cpi->avg_encode_time = 0;
    }
}

int vp8_block_error_c(short *coeff, short *dqcoeff)
{
    int i;
    int error = 0;

    for (i = 0; i < 16; i++)
    {
        int this_diff = coeff[i] - dqcoeff[i];
        error += this_diff * this_diff;
    }

    return error;
}

int vp8_mbblock_error_c(MACROBLOCK *mb, int dc)
{
    BLOCK  *be;
    BLOCKD *bd;
    int i, j;
    int berror, error = 0;

    for (i = 0; i < 16; i++)
    {
        be = &mb->block[i];
        bd = &mb->e_mbd.block[i];

        berror = 0;

        for (j = dc; j < 16; j++)
        {
            int this_diff = be->coeff[j] - bd->dqcoeff[j];
            berror += this_diff * this_diff;
        }

        error += berror;
    }

    return error;
}

int vp8_mbuverror_c(MACROBLOCK *mb)
{

    BLOCK  *be;
    BLOCKD *bd;


    int i;
    int error = 0;

    for (i = 16; i < 24; i++)
    {
        be = &mb->block[i];
        bd = &mb->e_mbd.block[i];

        error += vp8_block_error_c(be->coeff, bd->dqcoeff);
    }

    return error;
}

int VP8_UVSSE(MACROBLOCK *x, const vp8_variance_rtcd_vtable_t *rtcd)
{
    unsigned char *uptr, *vptr;
    unsigned char *upred_ptr = (*(x->block[16].base_src) + x->block[16].src);
    unsigned char *vpred_ptr = (*(x->block[20].base_src) + x->block[20].src);
    int uv_stride = x->block[16].src_stride;

    unsigned int sse1 = 0;
    unsigned int sse2 = 0;
    int mv_row = x->e_mbd.mode_info_context->mbmi.mv.as_mv.row;
    int mv_col = x->e_mbd.mode_info_context->mbmi.mv.as_mv.col;
    int offset;
    int pre_stride = x->e_mbd.block[16].pre_stride;

    if (mv_row < 0)
        mv_row -= 1;
    else
        mv_row += 1;

    if (mv_col < 0)
        mv_col -= 1;
    else
        mv_col += 1;

    mv_row /= 2;
    mv_col /= 2;

    offset = (mv_row >> 3) * pre_stride + (mv_col >> 3);
    uptr = x->e_mbd.pre.u_buffer + offset;
    vptr = x->e_mbd.pre.v_buffer + offset;

    if ((mv_row | mv_col) & 7)
    {
        VARIANCE_INVOKE(rtcd, subpixvar8x8)(uptr, pre_stride,
            mv_col & 7, mv_row & 7, upred_ptr, uv_stride, &sse2);
        VARIANCE_INVOKE(rtcd, subpixvar8x8)(vptr, pre_stride,
            mv_col & 7, mv_row & 7, vpred_ptr, uv_stride, &sse1);
        sse2 += sse1;
    }
    else
    {
        VARIANCE_INVOKE(rtcd, var8x8)(uptr, pre_stride,
            upred_ptr, uv_stride, &sse2);
        VARIANCE_INVOKE(rtcd, var8x8)(vptr, pre_stride,
            vpred_ptr, uv_stride, &sse1);
        sse2 += sse1;
    }
    return sse2;

}

static int cost_coeffs(MACROBLOCK *mb, BLOCKD *b, int type, ENTROPY_CONTEXT *a, ENTROPY_CONTEXT *l)
{
    int c = !type;              /* start at coef 0, unless Y with Y2 */
    int eob = (int)(*b->eob);
    int pt ;    /* surrounding block/prev coef predictor */
    int cost = 0;
    short *qcoeff_ptr = b->qcoeff;

    VP8_COMBINEENTROPYCONTEXTS(pt, *a, *l);

# define QC( I)  ( qcoeff_ptr [vp8_default_zig_zag1d[I]] )

    for (; c < eob; c++)
    {
        int v = QC(c);
        int t = vp8_dct_value_tokens_ptr[v].Token;
        cost += mb->token_costs [type] [vp8_coef_bands[c]] [pt] [t];
        cost += vp8_dct_value_cost_ptr[v];
        pt = vp8_prev_token_class[t];
    }

# undef QC

    if (c < 16)
        cost += mb->token_costs [type] [vp8_coef_bands[c]] [pt] [DCT_EOB_TOKEN];

    pt = (c != !type); // is eob first coefficient;
    *a = *l = pt;

    return cost;
}

static int vp8_rdcost_mby(MACROBLOCK *mb)
{
    int cost = 0;
    int b;
    MACROBLOCKD *x = &mb->e_mbd;
    ENTROPY_CONTEXT_PLANES t_above, t_left;
    ENTROPY_CONTEXT *ta;
    ENTROPY_CONTEXT *tl;

    vpx_memcpy(&t_above, mb->e_mbd.above_context, sizeof(ENTROPY_CONTEXT_PLANES));
    vpx_memcpy(&t_left, mb->e_mbd.left_context, sizeof(ENTROPY_CONTEXT_PLANES));

    ta = (ENTROPY_CONTEXT *)&t_above;
    tl = (ENTROPY_CONTEXT *)&t_left;

    for (b = 0; b < 16; b++)
        cost += cost_coeffs(mb, x->block + b, PLANE_TYPE_Y_NO_DC,
                    ta + vp8_block2above[b], tl + vp8_block2left[b]);

    cost += cost_coeffs(mb, x->block + 24, PLANE_TYPE_Y2,
                ta + vp8_block2above[24], tl + vp8_block2left[24]);

    return cost;
}

static void macro_block_yrd( MACROBLOCK *mb,
                             int *Rate,
                             int *Distortion,
                             const vp8_encodemb_rtcd_vtable_t *rtcd)
{
    int b;
    MACROBLOCKD *const x = &mb->e_mbd;
    BLOCK   *const mb_y2 = mb->block + 24;
    BLOCKD *const x_y2  = x->block + 24;
    short *Y2DCPtr = mb_y2->src_diff;
    BLOCK *beptr;
    int d;

    ENCODEMB_INVOKE(rtcd, submby)( mb->src_diff, *(mb->block[0].base_src),
        mb->block[0].src_stride,  mb->e_mbd.predictor, 16);

    // Fdct and building the 2nd order block
    for (beptr = mb->block; beptr < mb->block + 16; beptr += 2)
    {
        mb->vp8_short_fdct8x4(beptr->src_diff, beptr->coeff, 32);
        *Y2DCPtr++ = beptr->coeff[0];
        *Y2DCPtr++ = beptr->coeff[16];
    }

    // 2nd order fdct
    mb->short_walsh4x4(mb_y2->src_diff, mb_y2->coeff, 8);

    // Quantization
    for (b = 0; b < 16; b++)
    {
        mb->quantize_b(&mb->block[b], &mb->e_mbd.block[b]);
    }

    // DC predication and Quantization of 2nd Order block
    mb->quantize_b(mb_y2, x_y2);

    // Distortion
    d = ENCODEMB_INVOKE(rtcd, mberr)(mb, 1) << 2;
    d += ENCODEMB_INVOKE(rtcd, berr)(mb_y2->coeff, x_y2->dqcoeff);

    *Distortion = (d >> 4);

    // rate
    *Rate = vp8_rdcost_mby(mb);
}

static void copy_predictor(unsigned char *dst, const unsigned char *predictor)
{
    const unsigned int *p = (const unsigned int *)predictor;
    unsigned int *d = (unsigned int *)dst;
    d[0] = p[0];
    d[4] = p[4];
    d[8] = p[8];
    d[12] = p[12];
}
static int rd_pick_intra4x4block(
    VP8_COMP *cpi,
    MACROBLOCK *x,
    BLOCK *be,
    BLOCKD *b,
    B_PREDICTION_MODE *best_mode,
    unsigned int *bmode_costs,
    ENTROPY_CONTEXT *a,
    ENTROPY_CONTEXT *l,

    int *bestrate,
    int *bestratey,
    int *bestdistortion)
{
    B_PREDICTION_MODE mode;
    int best_rd = INT_MAX;
    int rate = 0;
    int distortion;

    ENTROPY_CONTEXT ta = *a, tempa = *a;
    ENTROPY_CONTEXT tl = *l, templ = *l;
    /*
     * The predictor buffer is a 2d buffer with a stride of 16.  Create
     * a temp buffer that meets the stride requirements, but we are only
     * interested in the left 4x4 block
     * */
    DECLARE_ALIGNED_ARRAY(16, unsigned char,  best_predictor, 16*4);
    DECLARE_ALIGNED_ARRAY(16, short, best_dqcoeff, 16);

    for (mode = B_DC_PRED; mode <= B_HU_PRED; mode++)
    {
        int this_rd;
        int ratey;

        rate = bmode_costs[mode];

        RECON_INVOKE(&cpi->rtcd.common->recon, intra4x4_predict)
                     (*(b->base_dst) + b->dst, b->dst_stride,
                      mode, b->predictor, 16);
        ENCODEMB_INVOKE(IF_RTCD(&cpi->rtcd.encodemb), subb)(be, b, 16);
        x->vp8_short_fdct4x4(be->src_diff, be->coeff, 32);
        x->quantize_b(be, b);

        tempa = ta;
        templ = tl;

        ratey = cost_coeffs(x, b, PLANE_TYPE_Y_WITH_DC, &tempa, &templ);
        rate += ratey;
        distortion = ENCODEMB_INVOKE(IF_RTCD(&cpi->rtcd.encodemb), berr)(be->coeff, b->dqcoeff) >> 2;

        this_rd = RDCOST(x->rdmult, x->rddiv, rate, distortion);

        if (this_rd < best_rd)
        {
            *bestrate = rate;
            *bestratey = ratey;
            *bestdistortion = distortion;
            best_rd = this_rd;
            *best_mode = mode;
            *a = tempa;
            *l = templ;
            copy_predictor(best_predictor, b->predictor);
            vpx_memcpy(best_dqcoeff, b->dqcoeff, 32);
        }
    }
    b->bmi.as_mode = (B_PREDICTION_MODE)(*best_mode);

    IDCT_INVOKE(IF_RTCD(&cpi->rtcd.common->idct), idct16)(best_dqcoeff,
        best_predictor, 16, *(b->base_dst) + b->dst, b->dst_stride);

    return best_rd;
}

static int rd_pick_intra4x4mby_modes(VP8_COMP *cpi, MACROBLOCK *mb, int *Rate,
                                     int *rate_y, int *Distortion, int best_rd)
{
    MACROBLOCKD *const xd = &mb->e_mbd;
    int i;
    int cost = mb->mbmode_cost [xd->frame_type] [B_PRED];
    int distortion = 0;
    int tot_rate_y = 0;
    int64_t total_rd = 0;
    ENTROPY_CONTEXT_PLANES t_above, t_left;
    ENTROPY_CONTEXT *ta;
    ENTROPY_CONTEXT *tl;
    unsigned int *bmode_costs;

    vpx_memcpy(&t_above, mb->e_mbd.above_context, sizeof(ENTROPY_CONTEXT_PLANES));
    vpx_memcpy(&t_left, mb->e_mbd.left_context, sizeof(ENTROPY_CONTEXT_PLANES));

    ta = (ENTROPY_CONTEXT *)&t_above;
    tl = (ENTROPY_CONTEXT *)&t_left;

    vp8_intra_prediction_down_copy(xd);

    bmode_costs = mb->inter_bmode_costs;

    for (i = 0; i < 16; i++)
    {
        MODE_INFO *const mic = xd->mode_info_context;
        const int mis = xd->mode_info_stride;
        B_PREDICTION_MODE UNINITIALIZED_IS_SAFE(best_mode);
        int UNINITIALIZED_IS_SAFE(r), UNINITIALIZED_IS_SAFE(ry), UNINITIALIZED_IS_SAFE(d);

        if (mb->e_mbd.frame_type == KEY_FRAME)
        {
            const B_PREDICTION_MODE A = above_block_mode(mic, i, mis);
            const B_PREDICTION_MODE L = left_block_mode(mic, i);

            bmode_costs  = mb->bmode_costs[A][L];
        }

        total_rd += rd_pick_intra4x4block(
            cpi, mb, mb->block + i, xd->block + i, &best_mode, bmode_costs,
            ta + vp8_block2above[i],
            tl + vp8_block2left[i], &r, &ry, &d);

        cost += r;
        distortion += d;
        tot_rate_y += ry;

        mic->bmi[i].as_mode = best_mode;

        if(total_rd >= (int64_t)best_rd)
            break;
    }

    if(total_rd >= (int64_t)best_rd)
        return INT_MAX;

    *Rate = cost;
    *rate_y = tot_rate_y;
    *Distortion = distortion;

    return RDCOST(mb->rdmult, mb->rddiv, cost, distortion);
}


static int rd_pick_intra16x16mby_mode(VP8_COMP *cpi,
                                      MACROBLOCK *x,
                                      int *Rate,
                                      int *rate_y,
                                      int *Distortion)
{
    MB_PREDICTION_MODE mode;
    MB_PREDICTION_MODE UNINITIALIZED_IS_SAFE(mode_selected);
    int rate, ratey;
    int distortion;
    int best_rd = INT_MAX;
    int this_rd;

    //Y Search for 16x16 intra prediction mode
    for (mode = DC_PRED; mode <= TM_PRED; mode++)
    {
        x->e_mbd.mode_info_context->mbmi.mode = mode;

        RECON_INVOKE(&cpi->common.rtcd.recon, build_intra_predictors_mby)
            (&x->e_mbd);

        macro_block_yrd(x, &ratey, &distortion, IF_RTCD(&cpi->rtcd.encodemb));
        rate = ratey + x->mbmode_cost[x->e_mbd.frame_type]
                                     [x->e_mbd.mode_info_context->mbmi.mode];

        this_rd = RDCOST(x->rdmult, x->rddiv, rate, distortion);

        if (this_rd < best_rd)
        {
            mode_selected = mode;
            best_rd = this_rd;
            *Rate = rate;
            *rate_y = ratey;
            *Distortion = distortion;
        }
    }

    x->e_mbd.mode_info_context->mbmi.mode = mode_selected;
    return best_rd;
}

static int rd_cost_mbuv(MACROBLOCK *mb)
{
    int b;
    int cost = 0;
    MACROBLOCKD *x = &mb->e_mbd;
    ENTROPY_CONTEXT_PLANES t_above, t_left;
    ENTROPY_CONTEXT *ta;
    ENTROPY_CONTEXT *tl;

    vpx_memcpy(&t_above, mb->e_mbd.above_context, sizeof(ENTROPY_CONTEXT_PLANES));
    vpx_memcpy(&t_left, mb->e_mbd.left_context, sizeof(ENTROPY_CONTEXT_PLANES));

    ta = (ENTROPY_CONTEXT *)&t_above;
    tl = (ENTROPY_CONTEXT *)&t_left;

    for (b = 16; b < 24; b++)
        cost += cost_coeffs(mb, x->block + b, PLANE_TYPE_UV,
                    ta + vp8_block2above[b], tl + vp8_block2left[b]);

    return cost;
}


static int rd_inter16x16_uv(VP8_COMP *cpi, MACROBLOCK *x, int *rate,
                            int *distortion, int fullpixel)
{
    vp8_build_inter16x16_predictors_mbuv(&x->e_mbd);
    ENCODEMB_INVOKE(IF_RTCD(&cpi->rtcd.encodemb), submbuv)(x->src_diff,
        x->src.u_buffer, x->src.v_buffer, x->src.uv_stride,
        &x->e_mbd.predictor[256], &x->e_mbd.predictor[320], 8);

    vp8_transform_mbuv(x);
    vp8_quantize_mbuv(x);

    *rate       = rd_cost_mbuv(x);
    *distortion = ENCODEMB_INVOKE(&cpi->rtcd.encodemb, mbuverr)(x) / 4;

    return RDCOST(x->rdmult, x->rddiv, *rate, *distortion);
}

static int rd_inter4x4_uv(VP8_COMP *cpi, MACROBLOCK *x, int *rate,
                          int *distortion, int fullpixel)
{
    vp8_build_inter4x4_predictors_mbuv(&x->e_mbd);
    ENCODEMB_INVOKE(IF_RTCD(&cpi->rtcd.encodemb), submbuv)(x->src_diff,
        x->src.u_buffer, x->src.v_buffer, x->src.uv_stride,
        &x->e_mbd.predictor[256], &x->e_mbd.predictor[320], 8);

    vp8_transform_mbuv(x);
    vp8_quantize_mbuv(x);

    *rate       = rd_cost_mbuv(x);
    *distortion = ENCODEMB_INVOKE(&cpi->rtcd.encodemb, mbuverr)(x) / 4;

    return RDCOST(x->rdmult, x->rddiv, *rate, *distortion);
}

static void rd_pick_intra_mbuv_mode(VP8_COMP *cpi, MACROBLOCK *x, int *rate, int *rate_tokenonly, int *distortion)
{
    MB_PREDICTION_MODE mode;
    MB_PREDICTION_MODE UNINITIALIZED_IS_SAFE(mode_selected);
    int best_rd = INT_MAX;
    int UNINITIALIZED_IS_SAFE(d), UNINITIALIZED_IS_SAFE(r);
    int rate_to;

    for (mode = DC_PRED; mode <= TM_PRED; mode++)
    {
        int rate;
        int distortion;
        int this_rd;

        x->e_mbd.mode_info_context->mbmi.uv_mode = mode;
        RECON_INVOKE(&cpi->rtcd.common->recon, build_intra_predictors_mbuv)
                     (&x->e_mbd);
        ENCODEMB_INVOKE(IF_RTCD(&cpi->rtcd.encodemb), submbuv)(x->src_diff,
                      x->src.u_buffer, x->src.v_buffer, x->src.uv_stride,
                      &x->e_mbd.predictor[256], &x->e_mbd.predictor[320], 8);
        vp8_transform_mbuv(x);
        vp8_quantize_mbuv(x);

        rate_to = rd_cost_mbuv(x);
        rate = rate_to + x->intra_uv_mode_cost[x->e_mbd.frame_type][x->e_mbd.mode_info_context->mbmi.uv_mode];

        distortion = ENCODEMB_INVOKE(&cpi->rtcd.encodemb, mbuverr)(x) / 4;

        this_rd = RDCOST(x->rdmult, x->rddiv, rate, distortion);

        if (this_rd < best_rd)
        {
            best_rd = this_rd;
            d = distortion;
            r = rate;
            *rate_tokenonly = rate_to;
            mode_selected = mode;
        }
    }

    *rate = r;
    *distortion = d;

    x->e_mbd.mode_info_context->mbmi.uv_mode = mode_selected;
}

int vp8_cost_mv_ref(MB_PREDICTION_MODE m, const int near_mv_ref_ct[4])
{
    vp8_prob p [VP8_MVREFS-1];
    assert(NEARESTMV <= m  &&  m <= SPLITMV);
    vp8_mv_ref_probs(p, near_mv_ref_ct);
    return vp8_cost_token(vp8_mv_ref_tree, p,
                          vp8_mv_ref_encoding_array - NEARESTMV + m);
}

void vp8_set_mbmode_and_mvs(MACROBLOCK *x, MB_PREDICTION_MODE mb, int_mv *mv)
{
    x->e_mbd.mode_info_context->mbmi.mode = mb;
    x->e_mbd.mode_info_context->mbmi.mv.as_int = mv->as_int;
}

static int labels2mode(
    MACROBLOCK *x,
    int const *labelings, int which_label,
    B_PREDICTION_MODE this_mode,
    int_mv *this_mv, int_mv *best_ref_mv,
    int *mvcost[2]
)
{
    MACROBLOCKD *const xd = & x->e_mbd;
    MODE_INFO *const mic = xd->mode_info_context;
    const int mis = xd->mode_info_stride;

    int cost = 0;
    int thismvcost = 0;

    /* We have to be careful retrieving previously-encoded motion vectors.
       Ones from this macroblock have to be pulled from the BLOCKD array
       as they have not yet made it to the bmi array in our MB_MODE_INFO. */

    int i = 0;

    do
    {
        BLOCKD *const d = xd->block + i;
        const int row = i >> 2,  col = i & 3;

        B_PREDICTION_MODE m;

        if (labelings[i] != which_label)
            continue;

        if (col  &&  labelings[i] == labelings[i-1])
            m = LEFT4X4;
        else if (row  &&  labelings[i] == labelings[i-4])
            m = ABOVE4X4;
        else
        {
            // the only time we should do costing for new motion vector or mode
            // is when we are on a new label  (jbb May 08, 2007)
            switch (m = this_mode)
            {
            case NEW4X4 :
                thismvcost  = vp8_mv_bit_cost(this_mv, best_ref_mv, mvcost, 102);
                break;
            case LEFT4X4:
                this_mv->as_int = col ? d[-1].bmi.mv.as_int : left_block_mv(mic, i);
                break;
            case ABOVE4X4:
                this_mv->as_int = row ? d[-4].bmi.mv.as_int : above_block_mv(mic, i, mis);
                break;
            case ZERO4X4:
                this_mv->as_int = 0;
                break;
            default:
                break;
            }

            if (m == ABOVE4X4)  // replace above with left if same
            {
                int_mv left_mv;

                left_mv.as_int = col ? d[-1].bmi.mv.as_int :
                                        left_block_mv(mic, i);

                if (left_mv.as_int == this_mv->as_int)
                    m = LEFT4X4;
            }

            cost = x->inter_bmode_costs[ m];
        }

        d->bmi.mv.as_int = this_mv->as_int;

        x->partition_info->bmi[i].mode = m;
        x->partition_info->bmi[i].mv.as_int = this_mv->as_int;

    }
    while (++i < 16);

    cost += thismvcost ;
    return cost;
}

static int rdcost_mbsegment_y(MACROBLOCK *mb, const int *labels,
                              int which_label, ENTROPY_CONTEXT *ta,
                              ENTROPY_CONTEXT *tl)
{
    int cost = 0;
    int b;
    MACROBLOCKD *x = &mb->e_mbd;

    for (b = 0; b < 16; b++)
        if (labels[ b] == which_label)
            cost += cost_coeffs(mb, x->block + b, PLANE_TYPE_Y_WITH_DC,
                                ta + vp8_block2above[b],
                                tl + vp8_block2left[b]);

    return cost;

}
static unsigned int vp8_encode_inter_mb_segment(MACROBLOCK *x, int const *labels, int which_label, const vp8_encodemb_rtcd_vtable_t *rtcd)
{
    int i;
    unsigned int distortion = 0;

    for (i = 0; i < 16; i++)
    {
        if (labels[i] == which_label)
        {
            BLOCKD *bd = &x->e_mbd.block[i];
            BLOCK *be = &x->block[i];


            vp8_build_inter_predictors_b(bd, 16, x->e_mbd.subpixel_predict);
            ENCODEMB_INVOKE(rtcd, subb)(be, bd, 16);
            x->vp8_short_fdct4x4(be->src_diff, be->coeff, 32);

            // set to 0 no way to account for 2nd order DC so discount
            //be->coeff[0] = 0;
            x->quantize_b(be, bd);

            distortion += ENCODEMB_INVOKE(rtcd, berr)(be->coeff, bd->dqcoeff);
        }
    }

    return distortion;
}


static const unsigned int segmentation_to_sseshift[4] = {3, 3, 2, 0};


typedef struct
{
  int_mv *ref_mv;
  int_mv mvp;

  int segment_rd;
  int segment_num;
  int r;
  int d;
  int segment_yrate;
  B_PREDICTION_MODE modes[16];
  int_mv mvs[16];
  unsigned char eobs[16];

  int mvthresh;
  int *mdcounts;

  int_mv sv_mvp[4];     // save 4 mvp from 8x8
  int sv_istep[2];  // save 2 initial step_param for 16x8/8x16

} BEST_SEG_INFO;


static void rd_check_segment(VP8_COMP *cpi, MACROBLOCK *x,
                             BEST_SEG_INFO *bsi, unsigned int segmentation)
{
    int i;
    int const *labels;
    int br = 0;
    int bd = 0;
    B_PREDICTION_MODE this_mode;


    int label_count;
    int this_segment_rd = 0;
    int label_mv_thresh;
    int rate = 0;
    int sbr = 0;
    int sbd = 0;
    int segmentyrate = 0;

    vp8_variance_fn_ptr_t *v_fn_ptr;

    ENTROPY_CONTEXT_PLANES t_above, t_left;
    ENTROPY_CONTEXT *ta;
    ENTROPY_CONTEXT *tl;
    ENTROPY_CONTEXT_PLANES t_above_b, t_left_b;
    ENTROPY_CONTEXT *ta_b;
    ENTROPY_CONTEXT *tl_b;

    vpx_memcpy(&t_above, x->e_mbd.above_context, sizeof(ENTROPY_CONTEXT_PLANES));
    vpx_memcpy(&t_left, x->e_mbd.left_context, sizeof(ENTROPY_CONTEXT_PLANES));

    ta = (ENTROPY_CONTEXT *)&t_above;
    tl = (ENTROPY_CONTEXT *)&t_left;
    ta_b = (ENTROPY_CONTEXT *)&t_above_b;
    tl_b = (ENTROPY_CONTEXT *)&t_left_b;

    br = 0;
    bd = 0;

    v_fn_ptr = &cpi->fn_ptr[segmentation];
    labels = vp8_mbsplits[segmentation];
    label_count = vp8_mbsplit_count[segmentation];

    // 64 makes this threshold really big effectively
    // making it so that we very rarely check mvs on
    // segments.   setting this to 1 would make mv thresh
    // roughly equal to what it is for macroblocks
    label_mv_thresh = 1 * bsi->mvthresh / label_count ;

    // Segmentation method overheads
    rate = vp8_cost_token(vp8_mbsplit_tree, vp8_mbsplit_probs, vp8_mbsplit_encodings + segmentation);
    rate += vp8_cost_mv_ref(SPLITMV, bsi->mdcounts);
    this_segment_rd += RDCOST(x->rdmult, x->rddiv, rate, 0);
    br += rate;

    for (i = 0; i < label_count; i++)
    {
        int_mv mode_mv[B_MODE_COUNT];
        int best_label_rd = INT_MAX;
        B_PREDICTION_MODE mode_selected = ZERO4X4;
        int bestlabelyrate = 0;

        // search for the best motion vector on this segment
        for (this_mode = LEFT4X4; this_mode <= NEW4X4 ; this_mode ++)
        {
            int this_rd;
            int distortion;
            int labelyrate;
            ENTROPY_CONTEXT_PLANES t_above_s, t_left_s;
            ENTROPY_CONTEXT *ta_s;
            ENTROPY_CONTEXT *tl_s;

            vpx_memcpy(&t_above_s, &t_above, sizeof(ENTROPY_CONTEXT_PLANES));
            vpx_memcpy(&t_left_s, &t_left, sizeof(ENTROPY_CONTEXT_PLANES));

            ta_s = (ENTROPY_CONTEXT *)&t_above_s;
            tl_s = (ENTROPY_CONTEXT *)&t_left_s;

            if (this_mode == NEW4X4)
            {
                int sseshift;
                int num00;
                int step_param = 0;
                int further_steps;
                int n;
                int thissme;
                int bestsme = INT_MAX;
                int_mv  temp_mv;
                BLOCK *c;
                BLOCKD *e;

                // Is the best so far sufficiently good that we cant justify doing and new motion search.
                if (best_label_rd < label_mv_thresh)
                    break;

                if(cpi->compressor_speed)
                {
                    if (segmentation == BLOCK_8X16 || segmentation == BLOCK_16X8)
                    {
                        bsi->mvp.as_int = bsi->sv_mvp[i].as_int;
                        if (i==1 && segmentation == BLOCK_16X8)
                          bsi->mvp.as_int = bsi->sv_mvp[2].as_int;

                        step_param = bsi->sv_istep[i];
                    }

                    // use previous block's result as next block's MV predictor.
                    if (segmentation == BLOCK_4X4 && i>0)
                    {
                        bsi->mvp.as_int = x->e_mbd.block[i-1].bmi.mv.as_int;
                        if (i==4 || i==8 || i==12)
                            bsi->mvp.as_int = x->e_mbd.block[i-4].bmi.mv.as_int;
                        step_param = 2;
                    }
                }

                further_steps = (MAX_MVSEARCH_STEPS - 1) - step_param;

                {
                    int sadpb = x->sadperbit4;
                    int_mv mvp_full;

                    mvp_full.as_mv.row = bsi->mvp.as_mv.row >>3;
                    mvp_full.as_mv.col = bsi->mvp.as_mv.col >>3;

                    // find first label
                    n = vp8_mbsplit_offset[segmentation][i];

                    c = &x->block[n];
                    e = &x->e_mbd.block[n];

                    {
                        bestsme = cpi->diamond_search_sad(x, c, e, &mvp_full,
                                                &mode_mv[NEW4X4], step_param,
                                                sadpb, &num00, v_fn_ptr,
                                                x->mvcost, bsi->ref_mv);

                        n = num00;
                        num00 = 0;

                        while (n < further_steps)
                        {
                            n++;

                            if (num00)
                                num00--;
                            else
                            {
                                thissme = cpi->diamond_search_sad(x, c, e,
                                                    &mvp_full, &temp_mv,
                                                    step_param + n, sadpb,
                                                    &num00, v_fn_ptr,
                                                    x->mvcost, bsi->ref_mv);

                                if (thissme < bestsme)
                                {
                                    bestsme = thissme;
                                    mode_mv[NEW4X4].as_int = temp_mv.as_int;
                                }
                            }
                        }
                    }

                    sseshift = segmentation_to_sseshift[segmentation];

                    // Should we do a full search (best quality only)
                    if ((cpi->compressor_speed == 0) && (bestsme >> sseshift) > 4000)
                    {
                        /* Check if mvp_full is within the range. */
                        vp8_clamp_mv(&mvp_full, x->mv_col_min, x->mv_col_max, x->mv_row_min, x->mv_row_max);

                        thissme = cpi->full_search_sad(x, c, e, &mvp_full,
                                                       sadpb, 16, v_fn_ptr,
                                                       x->mvcost, bsi->ref_mv);

                        if (thissme < bestsme)
                        {
                            bestsme = thissme;
                            mode_mv[NEW4X4].as_int = e->bmi.mv.as_int;
                        }
                        else
                        {
                            // The full search result is actually worse so re-instate the previous best vector
                            e->bmi.mv.as_int = mode_mv[NEW4X4].as_int;
                        }
                    }
                }

                if (bestsme < INT_MAX)
                {
                    int distortion;
                    unsigned int sse;
                    cpi->find_fractional_mv_step(x, c, e, &mode_mv[NEW4X4],
                        bsi->ref_mv, x->errorperbit, v_fn_ptr, x->mvcost,
                        &distortion, &sse);

                }
            } /* NEW4X4 */

            rate = labels2mode(x, labels, i, this_mode, &mode_mv[this_mode],
                               bsi->ref_mv, x->mvcost);

            // Trap vectors that reach beyond the UMV borders
            if (((mode_mv[this_mode].as_mv.row >> 3) < x->mv_row_min) || ((mode_mv[this_mode].as_mv.row >> 3) > x->mv_row_max) ||
                ((mode_mv[this_mode].as_mv.col >> 3) < x->mv_col_min) || ((mode_mv[this_mode].as_mv.col >> 3) > x->mv_col_max))
            {
                continue;
            }

            distortion = vp8_encode_inter_mb_segment(x, labels, i, IF_RTCD(&cpi->rtcd.encodemb)) / 4;

            labelyrate = rdcost_mbsegment_y(x, labels, i, ta_s, tl_s);
            rate += labelyrate;

            this_rd = RDCOST(x->rdmult, x->rddiv, rate, distortion);

            if (this_rd < best_label_rd)
            {
                sbr = rate;
                sbd = distortion;
                bestlabelyrate = labelyrate;
                mode_selected = this_mode;
                best_label_rd = this_rd;

                vpx_memcpy(ta_b, ta_s, sizeof(ENTROPY_CONTEXT_PLANES));
                vpx_memcpy(tl_b, tl_s, sizeof(ENTROPY_CONTEXT_PLANES));

            }
        } /*for each 4x4 mode*/

        vpx_memcpy(ta, ta_b, sizeof(ENTROPY_CONTEXT_PLANES));
        vpx_memcpy(tl, tl_b, sizeof(ENTROPY_CONTEXT_PLANES));

        labels2mode(x, labels, i, mode_selected, &mode_mv[mode_selected],
                    bsi->ref_mv, x->mvcost);

        br += sbr;
        bd += sbd;
        segmentyrate += bestlabelyrate;
        this_segment_rd += best_label_rd;

        if (this_segment_rd >= bsi->segment_rd)
            break;

    } /* for each label */

    if (this_segment_rd < bsi->segment_rd)
    {
        bsi->r = br;
        bsi->d = bd;
        bsi->segment_yrate = segmentyrate;
        bsi->segment_rd = this_segment_rd;
        bsi->segment_num = segmentation;

        // store everything needed to come back to this!!
        for (i = 0; i < 16; i++)
        {
            bsi->mvs[i].as_mv = x->partition_info->bmi[i].mv.as_mv;
            bsi->modes[i] = x->partition_info->bmi[i].mode;
            bsi->eobs[i] = x->e_mbd.eobs[i];
        }
    }
}

static
void vp8_cal_step_param(int sr, int *sp)
{
    int step = 0;

    if (sr > MAX_FIRST_STEP) sr = MAX_FIRST_STEP;
    else if (sr < 1) sr = 1;

    while (sr>>=1)
        step++;

    *sp = MAX_MVSEARCH_STEPS - 1 - step;
}

static int vp8_rd_pick_best_mbsegmentation(VP8_COMP *cpi, MACROBLOCK *x,
                                           int_mv *best_ref_mv, int best_rd,
                                           int *mdcounts, int *returntotrate,
                                           int *returnyrate, int *returndistortion,
                                           int mvthresh)
{
    int i;
    BEST_SEG_INFO bsi;

    vpx_memset(&bsi, 0, sizeof(bsi));

    bsi.segment_rd = best_rd;
    bsi.ref_mv = best_ref_mv;
    bsi.mvp.as_int = best_ref_mv->as_int;
    bsi.mvthresh = mvthresh;
    bsi.mdcounts = mdcounts;

    for(i = 0; i < 16; i++)
    {
        bsi.modes[i] = ZERO4X4;
    }

    if(cpi->compressor_speed == 0)
    {
        /* for now, we will keep the original segmentation order
           when in best quality mode */
        rd_check_segment(cpi, x, &bsi, BLOCK_16X8);
        rd_check_segment(cpi, x, &bsi, BLOCK_8X16);
        rd_check_segment(cpi, x, &bsi, BLOCK_8X8);
        rd_check_segment(cpi, x, &bsi, BLOCK_4X4);
    }
    else
    {
        int sr;

        rd_check_segment(cpi, x, &bsi, BLOCK_8X8);

        if (bsi.segment_rd < best_rd)
        {
            int col_min = ((best_ref_mv->as_mv.col+7)>>3) - MAX_FULL_PEL_VAL;
            int row_min = ((best_ref_mv->as_mv.row+7)>>3) - MAX_FULL_PEL_VAL;
            int col_max = (best_ref_mv->as_mv.col>>3) + MAX_FULL_PEL_VAL;
            int row_max = (best_ref_mv->as_mv.row>>3) + MAX_FULL_PEL_VAL;

            int tmp_col_min = x->mv_col_min;
            int tmp_col_max = x->mv_col_max;
            int tmp_row_min = x->mv_row_min;
            int tmp_row_max = x->mv_row_max;

            /* Get intersection of UMV window and valid MV window to reduce # of checks in diamond search. */
            if (x->mv_col_min < col_min )
                x->mv_col_min = col_min;
            if (x->mv_col_max > col_max )
                x->mv_col_max = col_max;
            if (x->mv_row_min < row_min )
                x->mv_row_min = row_min;
            if (x->mv_row_max > row_max )
                x->mv_row_max = row_max;

            /* Get 8x8 result */
            bsi.sv_mvp[0].as_int = bsi.mvs[0].as_int;
            bsi.sv_mvp[1].as_int = bsi.mvs[2].as_int;
            bsi.sv_mvp[2].as_int = bsi.mvs[8].as_int;
            bsi.sv_mvp[3].as_int = bsi.mvs[10].as_int;

            /* Use 8x8 result as 16x8/8x16's predictor MV. Adjust search range according to the closeness of 2 MV. */
            /* block 8X16 */
            {
                sr = MAXF((abs(bsi.sv_mvp[0].as_mv.row - bsi.sv_mvp[2].as_mv.row))>>3, (abs(bsi.sv_mvp[0].as_mv.col - bsi.sv_mvp[2].as_mv.col))>>3);
                vp8_cal_step_param(sr, &bsi.sv_istep[0]);

                sr = MAXF((abs(bsi.sv_mvp[1].as_mv.row - bsi.sv_mvp[3].as_mv.row))>>3, (abs(bsi.sv_mvp[1].as_mv.col - bsi.sv_mvp[3].as_mv.col))>>3);
                vp8_cal_step_param(sr, &bsi.sv_istep[1]);

                rd_check_segment(cpi, x, &bsi, BLOCK_8X16);
            }

            /* block 16X8 */
            {
                sr = MAXF((abs(bsi.sv_mvp[0].as_mv.row - bsi.sv_mvp[1].as_mv.row))>>3, (abs(bsi.sv_mvp[0].as_mv.col - bsi.sv_mvp[1].as_mv.col))>>3);
                vp8_cal_step_param(sr, &bsi.sv_istep[0]);

                sr = MAXF((abs(bsi.sv_mvp[2].as_mv.row - bsi.sv_mvp[3].as_mv.row))>>3, (abs(bsi.sv_mvp[2].as_mv.col - bsi.sv_mvp[3].as_mv.col))>>3);
                vp8_cal_step_param(sr, &bsi.sv_istep[1]);

                rd_check_segment(cpi, x, &bsi, BLOCK_16X8);
            }

            /* If 8x8 is better than 16x8/8x16, then do 4x4 search */
            /* Not skip 4x4 if speed=0 (good quality) */
            if (cpi->sf.no_skip_block4x4_search || bsi.segment_num == BLOCK_8X8)  /* || (sv_segment_rd8x8-bsi.segment_rd) < sv_segment_rd8x8>>5) */
            {
                bsi.mvp.as_int = bsi.sv_mvp[0].as_int;
                rd_check_segment(cpi, x, &bsi, BLOCK_4X4);
            }

            /* restore UMV window */
            x->mv_col_min = tmp_col_min;
            x->mv_col_max = tmp_col_max;
            x->mv_row_min = tmp_row_min;
            x->mv_row_max = tmp_row_max;
        }
    }

    /* set it to the best */
    for (i = 0; i < 16; i++)
    {
        BLOCKD *bd = &x->e_mbd.block[i];

        bd->bmi.mv.as_int = bsi.mvs[i].as_int;
        *bd->eob = bsi.eobs[i];
    }

    *returntotrate = bsi.r;
    *returndistortion = bsi.d;
    *returnyrate = bsi.segment_yrate;

    /* save partitions */
    x->e_mbd.mode_info_context->mbmi.partitioning = bsi.segment_num;
    x->partition_info->count = vp8_mbsplit_count[bsi.segment_num];

    for (i = 0; i < x->partition_info->count; i++)
    {
        int j;

        j = vp8_mbsplit_offset[bsi.segment_num][i];

        x->partition_info->bmi[i].mode = bsi.modes[j];
        x->partition_info->bmi[i].mv.as_mv = bsi.mvs[j].as_mv;
    }
    /*
     * used to set x->e_mbd.mode_info_context->mbmi.mv.as_int
     */
    x->partition_info->bmi[15].mv.as_int = bsi.mvs[15].as_int;

    return bsi.segment_rd;
}

//The improved MV prediction
void vp8_mv_pred
(
    VP8_COMP *cpi,
    MACROBLOCKD *xd,
    const MODE_INFO *here,
    int_mv *mvp,
    int refframe,
    int *ref_frame_sign_bias,
    int *sr,
    int near_sadidx[]
)
{
    const MODE_INFO *above = here - xd->mode_info_stride;
    const MODE_INFO *left = here - 1;
    const MODE_INFO *aboveleft = above - 1;
    int_mv           near_mvs[8];
    int              near_ref[8];
    int_mv           mv;
    int              vcnt=0;
    int              find=0;
    int              mb_offset;

    int              mvx[8];
    int              mvy[8];
    int              i;

    mv.as_int = 0;

    if(here->mbmi.ref_frame != INTRA_FRAME)
    {
        near_mvs[0].as_int = near_mvs[1].as_int = near_mvs[2].as_int = near_mvs[3].as_int = near_mvs[4].as_int = near_mvs[5].as_int = near_mvs[6].as_int = near_mvs[7].as_int = 0;
        near_ref[0] = near_ref[1] = near_ref[2] = near_ref[3] = near_ref[4] = near_ref[5] = near_ref[6] = near_ref[7] = 0;

        // read in 3 nearby block's MVs from current frame as prediction candidates.
        if (above->mbmi.ref_frame != INTRA_FRAME)
        {
            near_mvs[vcnt].as_int = above->mbmi.mv.as_int;
            mv_bias(ref_frame_sign_bias[above->mbmi.ref_frame], refframe, &near_mvs[vcnt], ref_frame_sign_bias);
            near_ref[vcnt] =  above->mbmi.ref_frame;
        }
        vcnt++;
        if (left->mbmi.ref_frame != INTRA_FRAME)
        {
            near_mvs[vcnt].as_int = left->mbmi.mv.as_int;
            mv_bias(ref_frame_sign_bias[left->mbmi.ref_frame], refframe, &near_mvs[vcnt], ref_frame_sign_bias);
            near_ref[vcnt] =  left->mbmi.ref_frame;
        }
        vcnt++;
        if (aboveleft->mbmi.ref_frame != INTRA_FRAME)
        {
            near_mvs[vcnt].as_int = aboveleft->mbmi.mv.as_int;
            mv_bias(ref_frame_sign_bias[aboveleft->mbmi.ref_frame], refframe, &near_mvs[vcnt], ref_frame_sign_bias);
            near_ref[vcnt] =  aboveleft->mbmi.ref_frame;
        }
        vcnt++;

        // read in 5 nearby block's MVs from last frame.
        if(cpi->common.last_frame_type != KEY_FRAME)
        {
            mb_offset = (-xd->mb_to_top_edge/128 + 1) * (xd->mode_info_stride +1) + (-xd->mb_to_left_edge/128 +1) ;

            // current in last frame
            if (cpi->lf_ref_frame[mb_offset] != INTRA_FRAME)
            {
                near_mvs[vcnt].as_int = cpi->lfmv[mb_offset].as_int;
                mv_bias(cpi->lf_ref_frame_sign_bias[mb_offset], refframe, &near_mvs[vcnt], ref_frame_sign_bias);
                near_ref[vcnt] =  cpi->lf_ref_frame[mb_offset];
            }
            vcnt++;

            // above in last frame
            if (cpi->lf_ref_frame[mb_offset - xd->mode_info_stride-1] != INTRA_FRAME)
            {
                near_mvs[vcnt].as_int = cpi->lfmv[mb_offset - xd->mode_info_stride-1].as_int;
                mv_bias(cpi->lf_ref_frame_sign_bias[mb_offset - xd->mode_info_stride-1], refframe, &near_mvs[vcnt], ref_frame_sign_bias);
                near_ref[vcnt] =  cpi->lf_ref_frame[mb_offset - xd->mode_info_stride-1];
            }
            vcnt++;

            // left in last frame
            if (cpi->lf_ref_frame[mb_offset-1] != INTRA_FRAME)
            {
                near_mvs[vcnt].as_int = cpi->lfmv[mb_offset -1].as_int;
                mv_bias(cpi->lf_ref_frame_sign_bias[mb_offset -1], refframe, &near_mvs[vcnt], ref_frame_sign_bias);
                near_ref[vcnt] =  cpi->lf_ref_frame[mb_offset - 1];
            }
            vcnt++;

            // right in last frame
            if (cpi->lf_ref_frame[mb_offset +1] != INTRA_FRAME)
            {
                near_mvs[vcnt].as_int = cpi->lfmv[mb_offset +1].as_int;
                mv_bias(cpi->lf_ref_frame_sign_bias[mb_offset +1], refframe, &near_mvs[vcnt], ref_frame_sign_bias);
                near_ref[vcnt] =  cpi->lf_ref_frame[mb_offset +1];
            }
            vcnt++;

            // below in last frame
            if (cpi->lf_ref_frame[mb_offset + xd->mode_info_stride +1] != INTRA_FRAME)
            {
                near_mvs[vcnt].as_int = cpi->lfmv[mb_offset + xd->mode_info_stride +1].as_int;
                mv_bias(cpi->lf_ref_frame_sign_bias[mb_offset + xd->mode_info_stride +1], refframe, &near_mvs[vcnt], ref_frame_sign_bias);
                near_ref[vcnt] =  cpi->lf_ref_frame[mb_offset + xd->mode_info_stride +1];
            }
            vcnt++;
        }

        for(i=0; i< vcnt; i++)
        {
            if(near_ref[near_sadidx[i]] != INTRA_FRAME)
            {
                if(here->mbmi.ref_frame == near_ref[near_sadidx[i]])
                {
                    mv.as_int = near_mvs[near_sadidx[i]].as_int;
                    find = 1;
                    if (i < 3)
                        *sr = 3;
                    else
                        *sr = 2;
                    break;
                }
            }
        }

        if(!find)
        {
            for(i=0; i<vcnt; i++)
            {
                mvx[i] = near_mvs[i].as_mv.row;
                mvy[i] = near_mvs[i].as_mv.col;
            }

            insertsortmv(mvx, vcnt);
            insertsortmv(mvy, vcnt);
            mv.as_mv.row = mvx[vcnt/2];
            mv.as_mv.col = mvy[vcnt/2];

            find = 1;
            //sr is set to 0 to allow calling function to decide the search range.
            *sr = 0;
        }
    }

    /* Set up return values */
    mvp->as_int = mv.as_int;
    vp8_clamp_mv2(mvp, xd);
}

void vp8_cal_sad(VP8_COMP *cpi, MACROBLOCKD *xd, MACROBLOCK *x, int recon_yoffset, int near_sadidx[])
{

    int near_sad[8] = {0}; // 0-cf above, 1-cf left, 2-cf aboveleft, 3-lf current, 4-lf above, 5-lf left, 6-lf right, 7-lf below
    BLOCK *b = &x->block[0];
    unsigned char *src_y_ptr = *(b->base_src);

    //calculate sad for current frame 3 nearby MBs.
    if( xd->mb_to_top_edge==0 && xd->mb_to_left_edge ==0)
    {
        near_sad[0] = near_sad[1] = near_sad[2] = INT_MAX;
    }else if(xd->mb_to_top_edge==0)
    {   //only has left MB for sad calculation.
        near_sad[0] = near_sad[2] = INT_MAX;
        near_sad[1] = cpi->fn_ptr[BLOCK_16X16].sdf(src_y_ptr, b->src_stride, xd->dst.y_buffer - 16,xd->dst.y_stride, 0x7fffffff);
    }else if(xd->mb_to_left_edge ==0)
    {   //only has left MB for sad calculation.
        near_sad[1] = near_sad[2] = INT_MAX;
        near_sad[0] = cpi->fn_ptr[BLOCK_16X16].sdf(src_y_ptr, b->src_stride, xd->dst.y_buffer - xd->dst.y_stride *16,xd->dst.y_stride, 0x7fffffff);
    }else
    {
        near_sad[0] = cpi->fn_ptr[BLOCK_16X16].sdf(src_y_ptr, b->src_stride, xd->dst.y_buffer - xd->dst.y_stride *16,xd->dst.y_stride, 0x7fffffff);
        near_sad[1] = cpi->fn_ptr[BLOCK_16X16].sdf(src_y_ptr, b->src_stride, xd->dst.y_buffer - 16,xd->dst.y_stride, 0x7fffffff);
        near_sad[2] = cpi->fn_ptr[BLOCK_16X16].sdf(src_y_ptr, b->src_stride, xd->dst.y_buffer - xd->dst.y_stride *16 -16,xd->dst.y_stride, 0x7fffffff);
    }

    if(cpi->common.last_frame_type != KEY_FRAME)
    {
        //calculate sad for last frame 5 nearby MBs.
        unsigned char *pre_y_buffer = cpi->common.yv12_fb[cpi->common.lst_fb_idx].y_buffer + recon_yoffset;
        int pre_y_stride = cpi->common.yv12_fb[cpi->common.lst_fb_idx].y_stride;

        if(xd->mb_to_top_edge==0) near_sad[4] = INT_MAX;
        if(xd->mb_to_left_edge ==0) near_sad[5] = INT_MAX;
        if(xd->mb_to_right_edge ==0) near_sad[6] = INT_MAX;
        if(xd->mb_to_bottom_edge==0) near_sad[7] = INT_MAX;

        if(near_sad[4] != INT_MAX)
            near_sad[4] = cpi->fn_ptr[BLOCK_16X16].sdf(src_y_ptr, b->src_stride, pre_y_buffer - pre_y_stride *16, pre_y_stride, 0x7fffffff);
        if(near_sad[5] != INT_MAX)
            near_sad[5] = cpi->fn_ptr[BLOCK_16X16].sdf(src_y_ptr, b->src_stride, pre_y_buffer - 16, pre_y_stride, 0x7fffffff);
        near_sad[3] = cpi->fn_ptr[BLOCK_16X16].sdf(src_y_ptr, b->src_stride, pre_y_buffer, pre_y_stride, 0x7fffffff);
        if(near_sad[6] != INT_MAX)
            near_sad[6] = cpi->fn_ptr[BLOCK_16X16].sdf(src_y_ptr, b->src_stride, pre_y_buffer + 16, pre_y_stride, 0x7fffffff);
        if(near_sad[7] != INT_MAX)
            near_sad[7] = cpi->fn_ptr[BLOCK_16X16].sdf(src_y_ptr, b->src_stride, pre_y_buffer + pre_y_stride *16, pre_y_stride, 0x7fffffff);
    }

    if(cpi->common.last_frame_type != KEY_FRAME)
    {
        insertsortsad(near_sad, near_sadidx, 8);
    }else
    {
        insertsortsad(near_sad, near_sadidx, 3);
    }
}

static void rd_update_mvcount(VP8_COMP *cpi, MACROBLOCK *x, int_mv *best_ref_mv)
{
    if (x->e_mbd.mode_info_context->mbmi.mode == SPLITMV)
    {
        int i;

        for (i = 0; i < x->partition_info->count; i++)
        {
            if (x->partition_info->bmi[i].mode == NEW4X4)
            {
                cpi->MVcount[0][mv_max+((x->partition_info->bmi[i].mv.as_mv.row
                                          - best_ref_mv->as_mv.row) >> 1)]++;
                cpi->MVcount[1][mv_max+((x->partition_info->bmi[i].mv.as_mv.col
                                          - best_ref_mv->as_mv.col) >> 1)]++;
            }
        }
    }
    else if (x->e_mbd.mode_info_context->mbmi.mode == NEWMV)
    {
        cpi->MVcount[0][mv_max+((x->e_mbd.mode_info_context->mbmi.mv.as_mv.row
                                          - best_ref_mv->as_mv.row) >> 1)]++;
        cpi->MVcount[1][mv_max+((x->e_mbd.mode_info_context->mbmi.mv.as_mv.col
                                          - best_ref_mv->as_mv.col) >> 1)]++;
    }
}


void vp8_rd_pick_inter_mode(VP8_COMP *cpi, MACROBLOCK *x, int recon_yoffset,
                            int recon_uvoffset, int *returnrate,
                            int *returndistortion, int *returnintra)
{
    BLOCK *b = &x->block[0];
    BLOCKD *d = &x->e_mbd.block[0];
    MACROBLOCKD *xd = &x->e_mbd;
    union b_mode_info best_bmodes[16];
    MB_MODE_INFO best_mbmode;
    PARTITION_INFO best_partition;
    int_mv best_ref_mv_sb[2];
    int_mv mode_mv_sb[2][MB_MODE_COUNT];
    int_mv best_ref_mv;
    int_mv *mode_mv;
    MB_PREDICTION_MODE this_mode;
    int num00;
    int best_mode_index = 0;

    int i;
    int mode_index;
    int mdcounts[4];
    int rate;
    int distortion;
    int best_rd = INT_MAX;
    int best_intra_rd = INT_MAX;
    int rate2, distortion2;
    int uv_intra_rate, uv_intra_distortion, uv_intra_rate_tokenonly;
    int rate_y, UNINITIALIZED_IS_SAFE(rate_uv);
    int distortion_uv;
    int best_yrd = INT_MAX;

    MB_PREDICTION_MODE uv_intra_mode;
    int_mv mvp;
    int near_sadidx[8] = {0, 1, 2, 3, 4, 5, 6, 7};
    int saddone=0;
    int sr=0;    //search range got from mv_pred(). It uses step_param levels. (0-7)

    unsigned char *plane[4][3];
    int ref_frame_map[4];
    int sign_bias = 0;

    mode_mv = mode_mv_sb[sign_bias];
    best_ref_mv.as_int = 0;
    vpx_memset(mode_mv_sb, 0, sizeof(mode_mv_sb));
    vpx_memset(&best_mbmode, 0, sizeof(best_mbmode));
    vpx_memset(&best_bmodes, 0, sizeof(best_bmodes));

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

    *returnintra = INT_MAX;
    cpi->mbs_tested_so_far++;          // Count of the number of MBs tested so far this frame

    x->skip = 0;

    x->e_mbd.mode_info_context->mbmi.ref_frame = INTRA_FRAME;
    rd_pick_intra_mbuv_mode(cpi, x, &uv_intra_rate, &uv_intra_rate_tokenonly, &uv_intra_distortion);
    uv_intra_mode = x->e_mbd.mode_info_context->mbmi.uv_mode;

    for (mode_index = 0; mode_index < MAX_MODES; mode_index++)
    {
        int this_rd = INT_MAX;
        int disable_skip = 0;
        int other_cost = 0;
        int this_ref_frame = ref_frame_map[vp8_ref_frame_order[mode_index]];

        // Test best rd so far against threshold for trying this mode.
        if (best_rd <= cpi->rd_threshes[mode_index])
            continue;

        if (this_ref_frame < 0)
            continue;

        // These variables hold are rolling total cost and distortion for this mode
        rate2 = 0;
        distortion2 = 0;

        this_mode = vp8_mode_order[mode_index];

        x->e_mbd.mode_info_context->mbmi.mode = this_mode;
        x->e_mbd.mode_info_context->mbmi.uv_mode = DC_PRED;
        x->e_mbd.mode_info_context->mbmi.ref_frame = this_ref_frame;

        // Only consider ZEROMV/ALTREF_FRAME for alt ref frame,
        // unless ARNR filtering is enabled in which case we want
        // an unfiltered alternative
        if (cpi->is_src_frame_alt_ref && (cpi->oxcf.arnr_max_frames == 0))
        {
            if (this_mode != ZEROMV || x->e_mbd.mode_info_context->mbmi.ref_frame != ALTREF_FRAME)
                continue;
        }

        /* everything but intra */
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
        }

        // Check to see if the testing frequency for this mode is at its max
        // If so then prevent it from being tested and increase the threshold for its testing
        if (cpi->mode_test_hit_counts[mode_index] && (cpi->mode_check_freq[mode_index] > 1))
        {
            if (cpi->mbs_tested_so_far  <= cpi->mode_check_freq[mode_index] * cpi->mode_test_hit_counts[mode_index])
            {
                // Increase the threshold for coding this mode to make it less likely to be chosen
                cpi->rd_thresh_mult[mode_index] += 4;

                if (cpi->rd_thresh_mult[mode_index] > MAX_THRESHMULT)
                    cpi->rd_thresh_mult[mode_index] = MAX_THRESHMULT;

                cpi->rd_threshes[mode_index] = (cpi->rd_baseline_thresh[mode_index] >> 7) * cpi->rd_thresh_mult[mode_index];

                continue;
            }
        }

        // We have now reached the point where we are going to test the current mode so increment the counter for the number of times it has been tested
        cpi->mode_test_hit_counts[mode_index] ++;

        // Experimental code. Special case for gf and arf zeromv modes. Increase zbin size to supress noise
        if (cpi->zbin_mode_boost_enabled)
        {
            if ( this_ref_frame == INTRA_FRAME )
                cpi->zbin_mode_boost = 0;
            else
            {
                if (vp8_mode_order[mode_index] == ZEROMV)
                {
                    if (this_ref_frame != LAST_FRAME)
                        cpi->zbin_mode_boost = GF_ZEROMV_ZBIN_BOOST;
                    else
                        cpi->zbin_mode_boost = LF_ZEROMV_ZBIN_BOOST;
                }
                else if (vp8_mode_order[mode_index] == SPLITMV)
                    cpi->zbin_mode_boost = 0;
                else
                    cpi->zbin_mode_boost = MV_ZBIN_BOOST;
            }

            vp8_update_zbin_extra(cpi, x);
        }

        switch (this_mode)
        {
        case B_PRED:
        {
            int tmp_rd;

            // Note the rate value returned here includes the cost of coding the BPRED mode : x->mbmode_cost[x->e_mbd.frame_type][BPRED];
            tmp_rd = rd_pick_intra4x4mby_modes(cpi, x, &rate, &rate_y, &distortion, best_yrd);
            rate2 += rate;
            distortion2 += distortion;

            if(tmp_rd < best_yrd)
            {
                rate2 += uv_intra_rate;
                rate_uv = uv_intra_rate_tokenonly;
                distortion2 += uv_intra_distortion;
                distortion_uv = uv_intra_distortion;
            }
            else
            {
                this_rd = INT_MAX;
                disable_skip = 1;
            }
        }
        break;

        case SPLITMV:
        {
            int tmp_rd;
            int this_rd_thresh;

            this_rd_thresh = (vp8_ref_frame_order[mode_index] == 1) ? cpi->rd_threshes[THR_NEW1] : cpi->rd_threshes[THR_NEW3];
            this_rd_thresh = (vp8_ref_frame_order[mode_index] == 2) ? cpi->rd_threshes[THR_NEW2] : this_rd_thresh;

            tmp_rd = vp8_rd_pick_best_mbsegmentation(cpi, x, &best_ref_mv,
                                                     best_yrd, mdcounts,
                                                     &rate, &rate_y, &distortion, this_rd_thresh) ;

            rate2 += rate;
            distortion2 += distortion;

            // If even the 'Y' rd value of split is higher than best so far then dont bother looking at UV
            if (tmp_rd < best_yrd)
            {
                // Now work out UV cost and add it in
                rd_inter4x4_uv(cpi, x, &rate_uv, &distortion_uv, cpi->common.full_pixel);
                rate2 += rate_uv;
                distortion2 += distortion_uv;
            }
            else
            {
                this_rd = INT_MAX;
                disable_skip = 1;
            }
        }
        break;
        case DC_PRED:
        case V_PRED:
        case H_PRED:
        case TM_PRED:
            x->e_mbd.mode_info_context->mbmi.ref_frame = INTRA_FRAME;
            RECON_INVOKE(&cpi->common.rtcd.recon, build_intra_predictors_mby)
                (&x->e_mbd);
            macro_block_yrd(x, &rate_y, &distortion, IF_RTCD(&cpi->rtcd.encodemb)) ;
            rate2 += rate_y;
            distortion2 += distortion;
            rate2 += x->mbmode_cost[x->e_mbd.frame_type][x->e_mbd.mode_info_context->mbmi.mode];
            rate2 += uv_intra_rate;
            rate_uv = uv_intra_rate_tokenonly;
            distortion2 += uv_intra_distortion;
            distortion_uv = uv_intra_distortion;
            break;

        case NEWMV:
        {
            int thissme;
            int bestsme = INT_MAX;
            int step_param = cpi->sf.first_step;
            int further_steps;
            int n;
            int do_refine=1;   /* If last step (1-away) of n-step search doesn't pick the center point as the best match,
                                  we will do a final 1-away diamond refining search  */

            int sadpb = x->sadperbit16;
            int_mv mvp_full;

            int col_min = ((best_ref_mv.as_mv.col+7)>>3) - MAX_FULL_PEL_VAL;
            int row_min = ((best_ref_mv.as_mv.row+7)>>3) - MAX_FULL_PEL_VAL;
            int col_max = (best_ref_mv.as_mv.col>>3) + MAX_FULL_PEL_VAL;
            int row_max = (best_ref_mv.as_mv.row>>3) + MAX_FULL_PEL_VAL;

            int tmp_col_min = x->mv_col_min;
            int tmp_col_max = x->mv_col_max;
            int tmp_row_min = x->mv_row_min;
            int tmp_row_max = x->mv_row_max;

            if(!saddone)
            {
                vp8_cal_sad(cpi,xd,x, recon_yoffset ,&near_sadidx[0] );
                saddone = 1;
            }

            vp8_mv_pred(cpi, &x->e_mbd, x->e_mbd.mode_info_context, &mvp,
                        x->e_mbd.mode_info_context->mbmi.ref_frame, cpi->common.ref_frame_sign_bias, &sr, &near_sadidx[0]);

            mvp_full.as_mv.col = mvp.as_mv.col>>3;
            mvp_full.as_mv.row = mvp.as_mv.row>>3;

            // Get intersection of UMV window and valid MV window to reduce # of checks in diamond search.
            if (x->mv_col_min < col_min )
                x->mv_col_min = col_min;
            if (x->mv_col_max > col_max )
                x->mv_col_max = col_max;
            if (x->mv_row_min < row_min )
                x->mv_row_min = row_min;
            if (x->mv_row_max > row_max )
                x->mv_row_max = row_max;

            //adjust search range according to sr from mv prediction
            if(sr > step_param)
                step_param = sr;

            // Initial step/diamond search
            {
                bestsme = cpi->diamond_search_sad(x, b, d, &mvp_full, &d->bmi.mv,
                                        step_param, sadpb, &num00,
                                        &cpi->fn_ptr[BLOCK_16X16],
                                        x->mvcost, &best_ref_mv);
                mode_mv[NEWMV].as_int = d->bmi.mv.as_int;

                // Further step/diamond searches as necessary
                n = 0;
                further_steps = (cpi->sf.max_step_search_steps - 1) - step_param;

                n = num00;
                num00 = 0;

                /* If there won't be more n-step search, check to see if refining search is needed. */
                if (n > further_steps)
                    do_refine = 0;

                while (n < further_steps)
                {
                    n++;

                    if (num00)
                        num00--;
                    else
                    {
                        thissme = cpi->diamond_search_sad(x, b, d, &mvp_full,
                                    &d->bmi.mv, step_param + n, sadpb, &num00,
                                    &cpi->fn_ptr[BLOCK_16X16], x->mvcost,
                                    &best_ref_mv);

                        /* check to see if refining search is needed. */
                        if (num00 > (further_steps-n))
                            do_refine = 0;

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

            /* final 1-away diamond refining search */
            if (do_refine == 1)
            {
                int search_range;

                //It seems not a good way to set search_range. Need further investigation.
                //search_range = MAXF(abs((mvp.row>>3) - d->bmi.mv.as_mv.row), abs((mvp.col>>3) - d->bmi.mv.as_mv.col));
                search_range = 8;

                //thissme = cpi->full_search_sad(x, b, d, &d->bmi.mv.as_mv, sadpb, search_range, &cpi->fn_ptr[BLOCK_16X16], x->mvcost, &best_ref_mv);
                thissme = cpi->refining_search_sad(x, b, d, &d->bmi.mv, sadpb,
                                       search_range, &cpi->fn_ptr[BLOCK_16X16],
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

            x->mv_col_min = tmp_col_min;
            x->mv_col_max = tmp_col_max;
            x->mv_row_min = tmp_row_min;
            x->mv_row_max = tmp_row_max;

            if (bestsme < INT_MAX)
            {
                int dis; /* TODO: use dis in distortion calculation later. */
                unsigned int sse;
                cpi->find_fractional_mv_step(x, b, d, &d->bmi.mv, &best_ref_mv,
                                             x->errorperbit,
                                             &cpi->fn_ptr[BLOCK_16X16],
                                             x->mvcost, &dis, &sse);
            }

            mode_mv[NEWMV].as_int = d->bmi.mv.as_int;

            // Add the new motion vector cost to our rolling cost variable
            rate2 += vp8_mv_bit_cost(&mode_mv[NEWMV], &best_ref_mv, x->mvcost, 96);
        }

        case NEARESTMV:
        case NEARMV:
            // Clip "next_nearest" so that it does not extend to far out of image
            vp8_clamp_mv2(&mode_mv[this_mode], xd);

            // Do not bother proceeding if the vector (from newmv,nearest or near) is 0,0 as this should then be coded using the zeromv mode.
            if (((this_mode == NEARMV) || (this_mode == NEARESTMV)) && (mode_mv[this_mode].as_int == 0))
                continue;

        case ZEROMV:

            // Trap vectors that reach beyond the UMV borders
            // Note that ALL New MV, Nearest MV Near MV and Zero MV code drops through to this point
            // because of the lack of break statements in the previous two cases.
            if (((mode_mv[this_mode].as_mv.row >> 3) < x->mv_row_min) || ((mode_mv[this_mode].as_mv.row >> 3) > x->mv_row_max) ||
                ((mode_mv[this_mode].as_mv.col >> 3) < x->mv_col_min) || ((mode_mv[this_mode].as_mv.col >> 3) > x->mv_col_max))
                continue;

            vp8_set_mbmode_and_mvs(x, this_mode, &mode_mv[this_mode]);
            vp8_build_inter16x16_predictors_mby(&x->e_mbd, x->e_mbd.predictor, 16);

            if (cpi->active_map_enabled && x->active_ptr[0] == 0) {
                x->skip = 1;
            }
            else if (x->encode_breakout)
            {
                unsigned int sse;
                unsigned int var;
                int threshold = (xd->block[0].dequant[1]
                            * xd->block[0].dequant[1] >>4);

                if(threshold < x->encode_breakout)
                    threshold = x->encode_breakout;

                var = VARIANCE_INVOKE(&cpi->common.rtcd.variance, var16x16)
                        (*(b->base_src), b->src_stride,
                        x->e_mbd.predictor, 16, &sse);

                if (sse < threshold)
                {
                     unsigned int q2dc = xd->block[24].dequant[0];
                    /* If theres is no codeable 2nd order dc
                       or a very small uniform pixel change change */
                    if ((sse - var < q2dc * q2dc >>4) ||
                        (sse /2 > var && sse-var < 64))
                    {
                        // Check u and v to make sure skip is ok
                        int sse2=  VP8_UVSSE(x, IF_RTCD(&cpi->common.rtcd.variance));
                        if (sse2 * 2 < threshold)
                        {
                            x->skip = 1;
                            distortion2 = sse + sse2;
                            rate2 = 500;

                            /* for best_yrd calculation */
                            rate_uv = 0;
                            distortion_uv = sse2;

                            disable_skip = 1;
                            this_rd = RDCOST(x->rdmult, x->rddiv, rate2, distortion2);

                            break;
                        }
                    }
                }
            }


            //intermodecost[mode_index] = vp8_cost_mv_ref(this_mode, mdcounts);   // Experimental debug code

            // Add in the Mv/mode cost
            rate2 += vp8_cost_mv_ref(this_mode, mdcounts);

            // Y cost and distortion
            macro_block_yrd(x, &rate_y, &distortion, IF_RTCD(&cpi->rtcd.encodemb));
            rate2 += rate_y;
            distortion2 += distortion;

            // UV cost and distortion
            rd_inter16x16_uv(cpi, x, &rate_uv, &distortion_uv, cpi->common.full_pixel);
            rate2 += rate_uv;
            distortion2 += distortion_uv;
            break;

        default:
            break;
        }

        // Where skip is allowable add in the default per mb cost for the no skip case.
        // where we then decide to skip we have to delete this and replace it with the
        // cost of signallying a skip
        if (cpi->common.mb_no_coeff_skip)
        {
            other_cost += vp8_cost_bit(cpi->prob_skip_false, 0);
            rate2 += other_cost;
        }

        /* Estimate the reference frame signaling cost and add it
         * to the rolling cost variable.
         */
        rate2 +=
            x->e_mbd.ref_frame_cost[x->e_mbd.mode_info_context->mbmi.ref_frame];

        if (!disable_skip)
        {
            // Test for the condition where skip block will be activated because there are no non zero coefficients and make any necessary adjustment for rate
            if (cpi->common.mb_no_coeff_skip)
            {
                int tteob;

                tteob = 0;

                for (i = 0; i <= 24; i++)
                {
                    tteob += x->e_mbd.eobs[i];
                }

                if (tteob == 0)
                {
                    rate2 -= (rate_y + rate_uv);
                    //for best_yrd calculation
                    rate_uv = 0;

                    // Back out no skip flag costing and add in skip flag costing
                    if (cpi->prob_skip_false)
                    {
                        int prob_skip_cost;

                        prob_skip_cost = vp8_cost_bit(cpi->prob_skip_false, 1);
                        prob_skip_cost -= vp8_cost_bit(cpi->prob_skip_false, 0);
                        rate2 += prob_skip_cost;
                        other_cost += prob_skip_cost;
                    }
                }
            }
            // Calculate the final RD estimate for this mode
            this_rd = RDCOST(x->rdmult, x->rddiv, rate2, distortion2);
        }

        // Keep record of best intra distortion
        if ((x->e_mbd.mode_info_context->mbmi.ref_frame == INTRA_FRAME) &&
            (this_rd < best_intra_rd) )
        {
            best_intra_rd = this_rd;
            *returnintra = distortion2 ;
        }

        // Did this mode help.. i.i is it the new best mode
        if (this_rd < best_rd || x->skip)
        {
            // Note index of best mode so far
            best_mode_index = mode_index;

            if (this_mode <= B_PRED)
            {
                x->e_mbd.mode_info_context->mbmi.uv_mode = uv_intra_mode;
                /* required for left and above block mv */
                x->e_mbd.mode_info_context->mbmi.mv.as_int = 0;
            }

            other_cost +=
            x->e_mbd.ref_frame_cost[x->e_mbd.mode_info_context->mbmi.ref_frame];

            /* Calculate the final y RD estimate for this mode */
            best_yrd = RDCOST(x->rdmult, x->rddiv, (rate2-rate_uv-other_cost),
                              (distortion2-distortion_uv));

            *returnrate = rate2;
            *returndistortion = distortion2;
            best_rd = this_rd;
            vpx_memcpy(&best_mbmode, &x->e_mbd.mode_info_context->mbmi, sizeof(MB_MODE_INFO));
            vpx_memcpy(&best_partition, x->partition_info, sizeof(PARTITION_INFO));

            if ((this_mode == B_PRED) || (this_mode == SPLITMV))
                for (i = 0; i < 16; i++)
                {
                    best_bmodes[i] = x->e_mbd.block[i].bmi;
                }


            // Testing this mode gave rise to an improvement in best error score. Lower threshold a bit for next time
            cpi->rd_thresh_mult[mode_index] = (cpi->rd_thresh_mult[mode_index] >= (MIN_THRESHMULT + 2)) ? cpi->rd_thresh_mult[mode_index] - 2 : MIN_THRESHMULT;
            cpi->rd_threshes[mode_index] = (cpi->rd_baseline_thresh[mode_index] >> 7) * cpi->rd_thresh_mult[mode_index];
        }

        // If the mode did not help improve the best error case then raise the threshold for testing that mode next time around.
        else
        {
            cpi->rd_thresh_mult[mode_index] += 4;

            if (cpi->rd_thresh_mult[mode_index] > MAX_THRESHMULT)
                cpi->rd_thresh_mult[mode_index] = MAX_THRESHMULT;

            cpi->rd_threshes[mode_index] = (cpi->rd_baseline_thresh[mode_index] >> 7) * cpi->rd_thresh_mult[mode_index];
        }

        if (x->skip)
            break;

    }

    // Reduce the activation RD thresholds for the best choice mode
    if ((cpi->rd_baseline_thresh[best_mode_index] > 0) && (cpi->rd_baseline_thresh[best_mode_index] < (INT_MAX >> 2)))
    {
        int best_adjustment = (cpi->rd_thresh_mult[best_mode_index] >> 2);

        cpi->rd_thresh_mult[best_mode_index] = (cpi->rd_thresh_mult[best_mode_index] >= (MIN_THRESHMULT + best_adjustment)) ? cpi->rd_thresh_mult[best_mode_index] - best_adjustment : MIN_THRESHMULT;
        cpi->rd_threshes[best_mode_index] = (cpi->rd_baseline_thresh[best_mode_index] >> 7) * cpi->rd_thresh_mult[best_mode_index];

        // If we chose a split mode then reset the new MV thresholds as well
        /*if ( vp8_mode_order[best_mode_index] == SPLITMV )
        {
            best_adjustment = 4; //(cpi->rd_thresh_mult[THR_NEWMV] >> 4);
            cpi->rd_thresh_mult[THR_NEWMV] = (cpi->rd_thresh_mult[THR_NEWMV] >= (MIN_THRESHMULT+best_adjustment)) ? cpi->rd_thresh_mult[THR_NEWMV]-best_adjustment: MIN_THRESHMULT;
            cpi->rd_threshes[THR_NEWMV] = (cpi->rd_baseline_thresh[THR_NEWMV] >> 7) * cpi->rd_thresh_mult[THR_NEWMV];

            best_adjustment = 4; //(cpi->rd_thresh_mult[THR_NEWG] >> 4);
            cpi->rd_thresh_mult[THR_NEWG] = (cpi->rd_thresh_mult[THR_NEWG] >= (MIN_THRESHMULT+best_adjustment)) ? cpi->rd_thresh_mult[THR_NEWG]-best_adjustment: MIN_THRESHMULT;
            cpi->rd_threshes[THR_NEWG] = (cpi->rd_baseline_thresh[THR_NEWG] >> 7) * cpi->rd_thresh_mult[THR_NEWG];

            best_adjustment = 4; //(cpi->rd_thresh_mult[THR_NEWA] >> 4);
            cpi->rd_thresh_mult[THR_NEWA] = (cpi->rd_thresh_mult[THR_NEWA] >= (MIN_THRESHMULT+best_adjustment)) ? cpi->rd_thresh_mult[THR_NEWA]-best_adjustment: MIN_THRESHMULT;
            cpi->rd_threshes[THR_NEWA] = (cpi->rd_baseline_thresh[THR_NEWA] >> 7) * cpi->rd_thresh_mult[THR_NEWA];
        }*/

    }

    // Note how often each mode chosen as best
    cpi->mode_chosen_counts[best_mode_index] ++;


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


    // macroblock modes
    vpx_memcpy(&x->e_mbd.mode_info_context->mbmi, &best_mbmode, sizeof(MB_MODE_INFO));

    if (best_mbmode.mode == B_PRED)
    {
        for (i = 0; i < 16; i++)
            xd->mode_info_context->bmi[i].as_mode = best_bmodes[i].as_mode;
    }

    if (best_mbmode.mode == SPLITMV)
    {
        for (i = 0; i < 16; i++)
            xd->mode_info_context->bmi[i].mv.as_int = best_bmodes[i].mv.as_int;

        vpx_memcpy(x->partition_info, &best_partition, sizeof(PARTITION_INFO));

        x->e_mbd.mode_info_context->mbmi.mv.as_int =
                                      x->partition_info->bmi[15].mv.as_int;
    }

    if (sign_bias
        != cpi->common.ref_frame_sign_bias[xd->mode_info_context->mbmi.ref_frame])
        best_ref_mv.as_int = best_ref_mv_sb[!sign_bias].as_int;

    rd_update_mvcount(cpi, x, &best_ref_mv);
}

void vp8_rd_pick_intra_mode(VP8_COMP *cpi, MACROBLOCK *x, int *rate_)
{
    int error4x4, error16x16;
    int rate4x4, rate16x16 = 0, rateuv;
    int dist4x4, dist16x16, distuv;
    int rate;
    int rate4x4_tokenonly = 0;
    int rate16x16_tokenonly = 0;
    int rateuv_tokenonly = 0;

    x->e_mbd.mode_info_context->mbmi.ref_frame = INTRA_FRAME;

    rd_pick_intra_mbuv_mode(cpi, x, &rateuv, &rateuv_tokenonly, &distuv);
    rate = rateuv;

    error16x16 = rd_pick_intra16x16mby_mode(cpi, x,
                                            &rate16x16, &rate16x16_tokenonly,
                                            &dist16x16);

    error4x4 = rd_pick_intra4x4mby_modes(cpi, x,
                                         &rate4x4, &rate4x4_tokenonly,
                                         &dist4x4, error16x16);

    if (error4x4 < error16x16)
    {
        x->e_mbd.mode_info_context->mbmi.mode = B_PRED;
        rate += rate4x4;
    }
    else
    {
        rate += rate16x16;
    }

    *rate_ = rate;
}
