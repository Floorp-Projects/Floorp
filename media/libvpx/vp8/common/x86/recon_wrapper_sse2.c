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
#include "vp8/common/recon.h"
#include "recon_x86.h"
#include "vpx_mem/vpx_mem.h"

#define build_intra_predictors_mbuv_prototype(sym) \
    void sym(unsigned char *dst, int dst_stride, \
             const unsigned char *src, int src_stride)
typedef build_intra_predictors_mbuv_prototype((*build_intra_predictors_mbuv_fn_t));

extern build_intra_predictors_mbuv_prototype(vp8_intra_pred_uv_dc_mmx2);
extern build_intra_predictors_mbuv_prototype(vp8_intra_pred_uv_dctop_mmx2);
extern build_intra_predictors_mbuv_prototype(vp8_intra_pred_uv_dcleft_mmx2);
extern build_intra_predictors_mbuv_prototype(vp8_intra_pred_uv_dc128_mmx);
extern build_intra_predictors_mbuv_prototype(vp8_intra_pred_uv_ho_mmx2);
extern build_intra_predictors_mbuv_prototype(vp8_intra_pred_uv_ho_ssse3);
extern build_intra_predictors_mbuv_prototype(vp8_intra_pred_uv_ve_mmx);
extern build_intra_predictors_mbuv_prototype(vp8_intra_pred_uv_tm_sse2);
extern build_intra_predictors_mbuv_prototype(vp8_intra_pred_uv_tm_ssse3);

static void vp8_build_intra_predictors_mbuv_x86(MACROBLOCKD *x,
                                                unsigned char *dst_u,
                                                unsigned char *dst_v,
                                                int dst_stride,
                                                build_intra_predictors_mbuv_fn_t tm_func,
                                                build_intra_predictors_mbuv_fn_t ho_func)
{
    int mode = x->mode_info_context->mbmi.uv_mode;
    build_intra_predictors_mbuv_fn_t fn;
    int src_stride = x->dst.uv_stride;

    switch (mode) {
        case  V_PRED: fn = vp8_intra_pred_uv_ve_mmx; break;
        case  H_PRED: fn = ho_func; break;
        case TM_PRED: fn = tm_func; break;
        case DC_PRED:
            if (x->up_available) {
                if (x->left_available) {
                    fn = vp8_intra_pred_uv_dc_mmx2; break;
                } else {
                    fn = vp8_intra_pred_uv_dctop_mmx2; break;
                }
            } else if (x->left_available) {
                fn = vp8_intra_pred_uv_dcleft_mmx2; break;
            } else {
                fn = vp8_intra_pred_uv_dc128_mmx; break;
            }
            break;
        default: return;
    }

    fn(dst_u, dst_stride, x->dst.u_buffer, src_stride);
    fn(dst_v, dst_stride, x->dst.v_buffer, src_stride);
}

void vp8_build_intra_predictors_mbuv_sse2(MACROBLOCKD *x)
{
    vp8_build_intra_predictors_mbuv_x86(x, &x->predictor[256],
                                        &x->predictor[320], 8,
                                        vp8_intra_pred_uv_tm_sse2,
                                        vp8_intra_pred_uv_ho_mmx2);
}

void vp8_build_intra_predictors_mbuv_ssse3(MACROBLOCKD *x)
{
    vp8_build_intra_predictors_mbuv_x86(x, &x->predictor[256],
                                        &x->predictor[320], 8,
                                        vp8_intra_pred_uv_tm_ssse3,
                                        vp8_intra_pred_uv_ho_ssse3);
}

void vp8_build_intra_predictors_mbuv_s_sse2(MACROBLOCKD *x)
{
    vp8_build_intra_predictors_mbuv_x86(x, x->dst.u_buffer,
                                        x->dst.v_buffer, x->dst.uv_stride,
                                        vp8_intra_pred_uv_tm_sse2,
                                        vp8_intra_pred_uv_ho_mmx2);
}

void vp8_build_intra_predictors_mbuv_s_ssse3(MACROBLOCKD *x)
{
    vp8_build_intra_predictors_mbuv_x86(x, x->dst.u_buffer,
                                        x->dst.v_buffer, x->dst.uv_stride,
                                        vp8_intra_pred_uv_tm_ssse3,
                                        vp8_intra_pred_uv_ho_ssse3);
}

extern build_intra_predictors_mbuv_prototype(vp8_intra_pred_y_dc_sse2);
extern build_intra_predictors_mbuv_prototype(vp8_intra_pred_y_dctop_sse2);
extern build_intra_predictors_mbuv_prototype(vp8_intra_pred_y_dcleft_sse2);
extern build_intra_predictors_mbuv_prototype(vp8_intra_pred_y_dc128_sse2);
extern build_intra_predictors_mbuv_prototype(vp8_intra_pred_y_ho_sse2);
extern build_intra_predictors_mbuv_prototype(vp8_intra_pred_y_ve_sse2);
extern build_intra_predictors_mbuv_prototype(vp8_intra_pred_y_tm_sse2);
extern build_intra_predictors_mbuv_prototype(vp8_intra_pred_y_tm_ssse3);

static void vp8_build_intra_predictors_mby_x86(MACROBLOCKD *x,
                                               unsigned char *dst_y,
                                               int dst_stride,
                                               build_intra_predictors_mbuv_fn_t tm_func)
{
    int mode = x->mode_info_context->mbmi.mode;
    build_intra_predictors_mbuv_fn_t fn;
    int src_stride = x->dst.y_stride;
    switch (mode) {
        case  V_PRED: fn = vp8_intra_pred_y_ve_sse2; break;
        case  H_PRED: fn = vp8_intra_pred_y_ho_sse2; break;
        case TM_PRED: fn = tm_func; break;
        case DC_PRED:
            if (x->up_available) {
                if (x->left_available) {
                    fn = vp8_intra_pred_y_dc_sse2; break;
                } else {
                    fn = vp8_intra_pred_y_dctop_sse2; break;
                }
            } else if (x->left_available) {
                fn = vp8_intra_pred_y_dcleft_sse2; break;
            } else {
                fn = vp8_intra_pred_y_dc128_sse2; break;
            }
            break;
        default: return;
    }

    fn(dst_y, dst_stride, x->dst.y_buffer, src_stride);
    return;
}

void vp8_build_intra_predictors_mby_sse2(MACROBLOCKD *x)
{
    vp8_build_intra_predictors_mby_x86(x, x->predictor, 16,
                                       vp8_intra_pred_y_tm_sse2);
}

void vp8_build_intra_predictors_mby_ssse3(MACROBLOCKD *x)
{
    vp8_build_intra_predictors_mby_x86(x, x->predictor, 16,
                                       vp8_intra_pred_y_tm_ssse3);
}

void vp8_build_intra_predictors_mby_s_sse2(MACROBLOCKD *x)
{
    vp8_build_intra_predictors_mby_x86(x, x->dst.y_buffer, x->dst.y_stride,
                                       vp8_intra_pred_y_tm_sse2);
}

void vp8_build_intra_predictors_mby_s_ssse3(MACROBLOCKD *x)
{
    vp8_build_intra_predictors_mby_x86(x, x->dst.y_buffer, x->dst.y_stride,
                                       vp8_intra_pred_y_tm_ssse3);

}
