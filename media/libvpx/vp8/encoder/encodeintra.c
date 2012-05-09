/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#include "vpx_config.h"
#include "vp8/common/idct.h"
#include "quantize.h"
#include "vp8/common/reconintra.h"
#include "vp8/common/reconintra4x4.h"
#include "encodemb.h"
#include "vp8/common/invtrans.h"
#include "vp8/common/recon.h"
#include "dct.h"
#include "encodeintra.h"


#if CONFIG_RUNTIME_CPU_DETECT
#define IF_RTCD(x) (x)
#else
#define IF_RTCD(x) NULL
#endif

int vp8_encode_intra(VP8_COMP *cpi, MACROBLOCK *x, int use_dc_pred)
{

    int i;
    int intra_pred_var = 0;
    (void) cpi;

    if (use_dc_pred)
    {
        const VP8_ENCODER_RTCD *rtcd = IF_RTCD(&cpi->rtcd);

        x->e_mbd.mode_info_context->mbmi.mode = DC_PRED;
        x->e_mbd.mode_info_context->mbmi.uv_mode = DC_PRED;
        x->e_mbd.mode_info_context->mbmi.ref_frame = INTRA_FRAME;

        vp8_encode_intra16x16mby(rtcd, x);

        vp8_inverse_transform_mby(&x->e_mbd, IF_RTCD(&cpi->common.rtcd));
    }
    else
    {
        for (i = 0; i < 16; i++)
        {
            x->e_mbd.block[i].bmi.as_mode = B_DC_PRED;
            vp8_encode_intra4x4block(IF_RTCD(&cpi->rtcd), x, i);
        }
    }

    intra_pred_var = VARIANCE_INVOKE(&cpi->common.rtcd.variance, getmbss)(x->src_diff);

    return intra_pred_var;
}

void vp8_encode_intra4x4block(const VP8_ENCODER_RTCD *rtcd,
                              MACROBLOCK *x, int ib)
{
    BLOCKD *b = &x->e_mbd.block[ib];
    BLOCK *be = &x->block[ib];

    RECON_INVOKE(&rtcd->common->recon, intra4x4_predict)
                (*(b->base_dst) + b->dst, b->dst_stride,
                 b->bmi.as_mode, b->predictor, 16);

    ENCODEMB_INVOKE(&rtcd->encodemb, subb)(be, b, 16);

    x->vp8_short_fdct4x4(be->src_diff, be->coeff, 32);

    x->quantize_b(be, b);

    if (*b->eob > 1)
    {
        IDCT_INVOKE(IF_RTCD(&rtcd->common->idct), idct16)(b->dqcoeff,
            b->predictor, 16, *(b->base_dst) + b->dst, b->dst_stride);
    }
    else
    {
        IDCT_INVOKE(IF_RTCD(&rtcd->common->idct), idct1_scalar_add)
            (b->dqcoeff[0], b->predictor, 16, *(b->base_dst) + b->dst,
                b->dst_stride);
    }
}

void vp8_encode_intra4x4mby(const VP8_ENCODER_RTCD *rtcd, MACROBLOCK *mb)
{
    int i;

    MACROBLOCKD *x = &mb->e_mbd;
    vp8_intra_prediction_down_copy(x);

    for (i = 0; i < 16; i++)
        vp8_encode_intra4x4block(rtcd, mb, i);
    return;
}

void vp8_encode_intra16x16mby(const VP8_ENCODER_RTCD *rtcd, MACROBLOCK *x)
{
    BLOCK *b = &x->block[0];
    MACROBLOCKD *xd = &x->e_mbd;

    RECON_INVOKE(&rtcd->common->recon, build_intra_predictors_mby_s)(&x->e_mbd);

    ENCODEMB_INVOKE(&rtcd->encodemb, submby) (x->src_diff, *(b->base_src),
        b->src_stride, xd->dst.y_buffer, xd->dst.y_stride);

    vp8_transform_intra_mby(x);

    vp8_quantize_mby(x);

    if (x->optimize)
        vp8_optimize_mby(x, rtcd);
}

void vp8_encode_intra16x16mbuv(const VP8_ENCODER_RTCD *rtcd, MACROBLOCK *x)
{
    MACROBLOCKD *xd = &x->e_mbd;

    RECON_INVOKE(&rtcd->common->recon, build_intra_predictors_mbuv_s)(&x->e_mbd);

    ENCODEMB_INVOKE(&rtcd->encodemb, submbuv)(x->src_diff, x->src.u_buffer,
        x->src.v_buffer, x->src.uv_stride, xd->dst.u_buffer,
        xd->dst.v_buffer, xd->dst.uv_stride);

    vp8_transform_mbuv(x);

    vp8_quantize_mbuv(x);

    if (x->optimize)
        vp8_optimize_mbuv(x, rtcd);
}
