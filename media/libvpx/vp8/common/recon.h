/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#ifndef __INC_RECON_H
#define __INC_RECON_H

#include "blockd.h"

#define prototype_copy_block(sym) \
    void sym(unsigned char *src, int src_pitch, unsigned char *dst, int dst_pitch)

#define prototype_recon_block(sym) \
    void sym(unsigned char *pred, short *diff, unsigned char *dst, int pitch)

#define prototype_recon_macroblock(sym) \
    void sym(const struct vp8_recon_rtcd_vtable *rtcd, MACROBLOCKD *x)

#define prototype_build_intra_predictors(sym) \
    void sym(MACROBLOCKD *x)

#define prototype_intra4x4_predict(sym) \
    void sym(BLOCKD *x, int b_mode, unsigned char *predictor)

struct vp8_recon_rtcd_vtable;

#if ARCH_X86 || ARCH_X86_64
#include "x86/recon_x86.h"
#endif

#if ARCH_ARM
#include "arm/recon_arm.h"
#endif

#ifndef vp8_recon_copy16x16
#define vp8_recon_copy16x16 vp8_copy_mem16x16_c
#endif
extern prototype_copy_block(vp8_recon_copy16x16);

#ifndef vp8_recon_copy8x8
#define vp8_recon_copy8x8 vp8_copy_mem8x8_c
#endif
extern prototype_copy_block(vp8_recon_copy8x8);

#ifndef vp8_recon_copy8x4
#define vp8_recon_copy8x4 vp8_copy_mem8x4_c
#endif
extern prototype_copy_block(vp8_recon_copy8x4);

#ifndef vp8_recon_recon
#define vp8_recon_recon vp8_recon_b_c
#endif
extern prototype_recon_block(vp8_recon_recon);

#ifndef vp8_recon_recon2
#define vp8_recon_recon2 vp8_recon2b_c
#endif
extern prototype_recon_block(vp8_recon_recon2);

#ifndef vp8_recon_recon4
#define vp8_recon_recon4 vp8_recon4b_c
#endif
extern prototype_recon_block(vp8_recon_recon4);

#ifndef vp8_recon_recon_mb
#define vp8_recon_recon_mb vp8_recon_mb_c
#endif
extern prototype_recon_macroblock(vp8_recon_recon_mb);

#ifndef vp8_recon_recon_mby
#define vp8_recon_recon_mby vp8_recon_mby_c
#endif
extern prototype_recon_macroblock(vp8_recon_recon_mby);

#ifndef vp8_recon_build_intra_predictors_mby
#define vp8_recon_build_intra_predictors_mby vp8_build_intra_predictors_mby
#endif
extern prototype_build_intra_predictors\
    (vp8_recon_build_intra_predictors_mby);

#ifndef vp8_recon_build_intra_predictors_mby_s
#define vp8_recon_build_intra_predictors_mby_s vp8_build_intra_predictors_mby_s
#endif
extern prototype_build_intra_predictors\
    (vp8_recon_build_intra_predictors_mby_s);

#ifndef vp8_recon_build_intra_predictors_mbuv
#define vp8_recon_build_intra_predictors_mbuv vp8_build_intra_predictors_mbuv
#endif
extern prototype_build_intra_predictors\
    (vp8_recon_build_intra_predictors_mbuv);

#ifndef vp8_recon_build_intra_predictors_mbuv_s
#define vp8_recon_build_intra_predictors_mbuv_s vp8_build_intra_predictors_mbuv_s
#endif
extern prototype_build_intra_predictors\
    (vp8_recon_build_intra_predictors_mbuv_s);

#ifndef vp8_recon_intra4x4_predict
#define vp8_recon_intra4x4_predict vp8_intra4x4_predict
#endif
extern prototype_intra4x4_predict\
    (vp8_recon_intra4x4_predict);


typedef prototype_copy_block((*vp8_copy_block_fn_t));
typedef prototype_recon_block((*vp8_recon_fn_t));
typedef prototype_recon_macroblock((*vp8_recon_mb_fn_t));
typedef prototype_build_intra_predictors((*vp8_build_intra_pred_fn_t));
typedef prototype_intra4x4_predict((*vp8_intra4x4_pred_fn_t));
typedef struct vp8_recon_rtcd_vtable
{
    vp8_copy_block_fn_t  copy16x16;
    vp8_copy_block_fn_t  copy8x8;
    vp8_copy_block_fn_t  copy8x4;
    vp8_recon_fn_t       recon;
    vp8_recon_fn_t       recon2;
    vp8_recon_fn_t       recon4;
    vp8_recon_mb_fn_t    recon_mb;
    vp8_recon_mb_fn_t    recon_mby;
    vp8_build_intra_pred_fn_t  build_intra_predictors_mby_s;
    vp8_build_intra_pred_fn_t  build_intra_predictors_mby;
    vp8_build_intra_pred_fn_t  build_intra_predictors_mbuv_s;
    vp8_build_intra_pred_fn_t  build_intra_predictors_mbuv;
    vp8_intra4x4_pred_fn_t intra4x4_predict;
} vp8_recon_rtcd_vtable_t;

#if CONFIG_RUNTIME_CPU_DETECT
#define RECON_INVOKE(ctx,fn) (ctx)->fn
#else
#define RECON_INVOKE(ctx,fn) vp8_recon_##fn
#endif

void vp8_recon_intra_mbuv(const vp8_recon_rtcd_vtable_t *rtcd, MACROBLOCKD *x);
#endif
