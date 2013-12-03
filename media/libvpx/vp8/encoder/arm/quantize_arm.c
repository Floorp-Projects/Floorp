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
#include "vp8/encoder/block.h"
#include <math.h>
#include "vpx_mem/vpx_mem.h"
#include "vp8/encoder/quantize.h"
#include "vp8/common/entropy.h"


#if HAVE_NEON

/* vp8_quantize_mbX functions here differs from corresponding ones in
 * quantize.c only by using quantize_b_pair function pointer instead of
 * the regular quantize_b function pointer */
void vp8_quantize_mby_neon(MACROBLOCK *x)
{
    int i;
    int has_2nd_order = (x->e_mbd.mode_info_context->mbmi.mode != B_PRED
        && x->e_mbd.mode_info_context->mbmi.mode != SPLITMV);

    for (i = 0; i < 16; i+=2)
        x->quantize_b_pair(&x->block[i], &x->block[i+1],
                           &x->e_mbd.block[i], &x->e_mbd.block[i+1]);

    if(has_2nd_order)
        x->quantize_b(&x->block[24], &x->e_mbd.block[24]);
}

void vp8_quantize_mb_neon(MACROBLOCK *x)
{
    int i;
    int has_2nd_order=(x->e_mbd.mode_info_context->mbmi.mode != B_PRED
        && x->e_mbd.mode_info_context->mbmi.mode != SPLITMV);

    for (i = 0; i < 24; i+=2)
        x->quantize_b_pair(&x->block[i], &x->block[i+1],
                           &x->e_mbd.block[i], &x->e_mbd.block[i+1]);

    if (has_2nd_order)
        x->quantize_b(&x->block[24], &x->e_mbd.block[24]);
}


void vp8_quantize_mbuv_neon(MACROBLOCK *x)
{
    int i;

    for (i = 16; i < 24; i+=2)
        x->quantize_b_pair(&x->block[i], &x->block[i+1],
                           &x->e_mbd.block[i], &x->e_mbd.block[i+1]);
}

#endif /* HAVE_NEON */
