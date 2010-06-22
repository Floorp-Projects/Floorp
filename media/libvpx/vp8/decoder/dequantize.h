/*
 *  Copyright (c) 2010 The VP8 project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license 
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may 
 *  be found in the AUTHORS file in the root of the source tree.
 */


#ifndef DEQUANTIZE_H
#define DEQUANTIZE_H
#include "blockd.h"

#define prototype_dequant_block(sym) \
    void sym(BLOCKD *x)

#define prototype_dequant_idct(sym) \
    void sym(short *input, short *dq, short *output, int pitch)

#define prototype_dequant_idct_dc(sym) \
    void sym(short *input, short *dq, short *output, int pitch, int dc)

#if ARCH_X86 || ARCH_X86_64
#include "x86/dequantize_x86.h"
#endif

#if ARCH_ARM
#include "arm/dequantize_arm.h"
#endif

#ifndef vp8_dequant_block
#define vp8_dequant_block vp8_dequantize_b_c
#endif
extern prototype_dequant_block(vp8_dequant_block);

#ifndef vp8_dequant_idct
#define vp8_dequant_idct vp8_dequant_idct_c
#endif
extern prototype_dequant_idct(vp8_dequant_idct);

#ifndef vp8_dequant_idct_dc
#define vp8_dequant_idct_dc vp8_dequant_dc_idct_c
#endif
extern prototype_dequant_idct_dc(vp8_dequant_idct_dc);


typedef prototype_dequant_block((*vp8_dequant_block_fn_t));
typedef prototype_dequant_idct((*vp8_dequant_idct_fn_t));
typedef prototype_dequant_idct_dc((*vp8_dequant_idct_dc_fn_t));
typedef struct
{
    vp8_dequant_block_fn_t    block;
    vp8_dequant_idct_fn_t     idct;
    vp8_dequant_idct_dc_fn_t  idct_dc;
} vp8_dequant_rtcd_vtable_t;

#if CONFIG_RUNTIME_CPU_DETECT
#define DEQUANT_INVOKE(ctx,fn) (ctx)->fn
#else
#define DEQUANT_INVOKE(ctx,fn) vp8_dequant_##fn
#endif

#endif
