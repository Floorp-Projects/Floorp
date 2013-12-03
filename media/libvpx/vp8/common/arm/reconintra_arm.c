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
#include "vpx_rtcd.h"
#include "vp8/common/blockd.h"
#include "vpx_mem/vpx_mem.h"

#if HAVE_NEON
extern void vp8_build_intra_predictors_mby_neon_func(
    unsigned char *y_buffer,
    unsigned char *ypred_ptr,
    int y_stride,
    int mode,
    int Up,
    int Left);

void vp8_build_intra_predictors_mby_neon(MACROBLOCKD *x)
{
    unsigned char *y_buffer = x->dst.y_buffer;
    unsigned char *ypred_ptr = x->predictor;
    int y_stride = x->dst.y_stride;
    int mode = x->mode_info_context->mbmi.mode;
    int Up = x->up_available;
    int Left = x->left_available;

    vp8_build_intra_predictors_mby_neon_func(y_buffer, ypred_ptr, y_stride, mode, Up, Left);
}

extern void vp8_build_intra_predictors_mby_s_neon_func(
    unsigned char *y_buffer,
    unsigned char *ypred_ptr,
    int y_stride,
    int mode,
    int Up,
    int Left);

void vp8_build_intra_predictors_mby_s_neon(MACROBLOCKD *x)
{
    unsigned char *y_buffer = x->dst.y_buffer;
    unsigned char *ypred_ptr = x->predictor;
    int y_stride = x->dst.y_stride;
    int mode = x->mode_info_context->mbmi.mode;
    int Up = x->up_available;
    int Left = x->left_available;

    vp8_build_intra_predictors_mby_s_neon_func(y_buffer, ypred_ptr, y_stride, mode, Up, Left);
}

#endif
