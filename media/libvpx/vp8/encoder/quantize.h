/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#ifndef __INC_QUANTIZE_H
#define __INC_QUANTIZE_H

#include "block.h"

#define prototype_quantize_block(sym) \
    void (sym)(BLOCK *b,BLOCKD *d)

#define prototype_quantize_block_pair(sym) \
    void (sym)(BLOCK *b1, BLOCK *b2, BLOCKD *d1, BLOCKD *d2)

#define prototype_quantize_mb(sym) \
    void (sym)(MACROBLOCK *x)

#if ARCH_X86 || ARCH_X86_64
#include "x86/quantize_x86.h"
#endif

#if ARCH_ARM
#include "arm/quantize_arm.h"
#endif

#ifndef vp8_quantize_quantb
#define vp8_quantize_quantb vp8_regular_quantize_b
#endif
extern prototype_quantize_block(vp8_quantize_quantb);

#ifndef vp8_quantize_quantb_pair
#define vp8_quantize_quantb_pair vp8_regular_quantize_b_pair
#endif
extern prototype_quantize_block_pair(vp8_quantize_quantb_pair);

#ifndef vp8_quantize_fastquantb
#define vp8_quantize_fastquantb vp8_fast_quantize_b_c
#endif
extern prototype_quantize_block(vp8_quantize_fastquantb);

#ifndef vp8_quantize_fastquantb_pair
#define vp8_quantize_fastquantb_pair vp8_fast_quantize_b_pair_c
#endif
extern prototype_quantize_block_pair(vp8_quantize_fastquantb_pair);

typedef struct
{
    prototype_quantize_block(*quantb);
    prototype_quantize_block_pair(*quantb_pair);
    prototype_quantize_block(*fastquantb);
    prototype_quantize_block_pair(*fastquantb_pair);
} vp8_quantize_rtcd_vtable_t;

#ifndef vp8_quantize_mb
#define vp8_quantize_mb vp8_quantize_mb_c
#endif
extern prototype_quantize_mb(vp8_quantize_mb);

#ifndef vp8_quantize_mbuv
#define vp8_quantize_mbuv vp8_quantize_mbuv_c
#endif
extern prototype_quantize_mb(vp8_quantize_mbuv);

#ifndef vp8_quantize_mby
#define vp8_quantize_mby vp8_quantize_mby_c
#endif
extern prototype_quantize_mb(vp8_quantize_mby);

#if CONFIG_RUNTIME_CPU_DETECT
#define QUANTIZE_INVOKE(ctx,fn) (ctx)->fn
#else
#define QUANTIZE_INVOKE(ctx,fn) vp8_quantize_##fn
#endif

extern void vp8_strict_quantize_b(BLOCK *b,BLOCKD *d);

struct VP8_COMP;
extern void vp8_set_quantizer(struct VP8_COMP *cpi, int Q);
extern void vp8cx_frame_init_quantizer(struct VP8_COMP *cpi);
extern void vp8_update_zbin_extra(struct VP8_COMP *cpi, MACROBLOCK *x);
extern void vp8cx_mb_init_quantizer(struct VP8_COMP *cpi, MACROBLOCK *x, int ok_to_skip);
extern void vp8cx_init_quantizer(struct VP8_COMP *cpi);

#endif
