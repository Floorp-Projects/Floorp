/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#ifndef __INC_ENCODEMB_H
#define __INC_ENCODEMB_H


#include "vpx_config.h"
#include "block.h"

#define prototype_mberr(sym) \
    int (sym)(MACROBLOCK *mb, int dc)

#define prototype_berr(sym) \
    int (sym)(short *coeff, short *dqcoeff)

#define prototype_mbuverr(sym) \
    int (sym)(MACROBLOCK *mb)

#define prototype_subb(sym) \
    void (sym)(BLOCK *be,BLOCKD *bd, int pitch)

#define prototype_submby(sym) \
    void (sym)(short *diff, unsigned char *src, int src_stride, \
        unsigned char *pred, int pred_stride)

#define prototype_submbuv(sym) \
    void (sym)(short *diff, unsigned char *usrc, unsigned char *vsrc,\
               int src_stride, unsigned char *upred, unsigned char *vpred,\
               int pred_stride)

#if ARCH_X86 || ARCH_X86_64
#include "x86/encodemb_x86.h"
#endif

#if ARCH_ARM
#include "arm/encodemb_arm.h"
#endif

#ifndef vp8_encodemb_berr
#define vp8_encodemb_berr vp8_block_error_c
#endif
extern prototype_berr(vp8_encodemb_berr);

#ifndef vp8_encodemb_mberr
#define vp8_encodemb_mberr vp8_mbblock_error_c
#endif
extern prototype_mberr(vp8_encodemb_mberr);

#ifndef vp8_encodemb_mbuverr
#define vp8_encodemb_mbuverr vp8_mbuverror_c
#endif
extern prototype_mbuverr(vp8_encodemb_mbuverr);

#ifndef vp8_encodemb_subb
#define vp8_encodemb_subb vp8_subtract_b_c
#endif
extern prototype_subb(vp8_encodemb_subb);

#ifndef vp8_encodemb_submby
#define vp8_encodemb_submby vp8_subtract_mby_c
#endif
extern prototype_submby(vp8_encodemb_submby);

#ifndef vp8_encodemb_submbuv
#define vp8_encodemb_submbuv vp8_subtract_mbuv_c
#endif
extern prototype_submbuv(vp8_encodemb_submbuv);


typedef struct
{
    prototype_berr(*berr);
    prototype_mberr(*mberr);
    prototype_mbuverr(*mbuverr);
    prototype_subb(*subb);
    prototype_submby(*submby);
    prototype_submbuv(*submbuv);
} vp8_encodemb_rtcd_vtable_t;

#if CONFIG_RUNTIME_CPU_DETECT
#define ENCODEMB_INVOKE(ctx,fn) (ctx)->fn
#else
#define ENCODEMB_INVOKE(ctx,fn) vp8_encodemb_##fn
#endif



#include "onyx_int.h"
struct VP8_ENCODER_RTCD;
void vp8_encode_inter16x16(const struct VP8_ENCODER_RTCD *rtcd, MACROBLOCK *x);

void vp8_build_dcblock(MACROBLOCK *b);
void vp8_transform_mb(MACROBLOCK *mb);
void vp8_transform_mbuv(MACROBLOCK *x);
void vp8_transform_intra_mby(MACROBLOCK *x);

void vp8_optimize_mby(MACROBLOCK *x, const struct VP8_ENCODER_RTCD *rtcd);
void vp8_optimize_mbuv(MACROBLOCK *x, const struct VP8_ENCODER_RTCD *rtcd);
void vp8_encode_inter16x16y(const struct VP8_ENCODER_RTCD *rtcd, MACROBLOCK *x);
#endif
