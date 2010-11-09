/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#include "vpx_ports/config.h"
#include "vpx_ports/x86.h"
#include "onyxd_int.h"


#if HAVE_MMX
void vp8_dequantize_b_impl_mmx(short *sq, short *dq, short *q);

void vp8_dequantize_b_mmx(BLOCKD *d)
{
    short *sq = (short *) d->qcoeff;
    short *dq = (short *) d->dqcoeff;
    short *q = (short *) d->dequant;
    vp8_dequantize_b_impl_mmx(sq, dq, q);
}
#endif

void vp8_arch_x86_decode_init(VP8D_COMP *pbi)
{
    int flags = x86_simd_caps();

    /* Note:
     *
     * This platform can be built without runtime CPU detection as well. If
     * you modify any of the function mappings present in this file, be sure
     * to also update them in static mapings (<arch>/filename_<arch>.h)
     */
#if CONFIG_RUNTIME_CPU_DETECT
    /* Override default functions with fastest ones for this CPU. */
#if HAVE_MMX
    if (flags & HAS_MMX)
    {
        pbi->dequant.block               = vp8_dequantize_b_mmx;
        pbi->dequant.idct_add            = vp8_dequant_idct_add_mmx;
        pbi->dequant.dc_idct_add         = vp8_dequant_dc_idct_add_mmx;
        pbi->dequant.dc_idct_add_y_block = vp8_dequant_dc_idct_add_y_block_mmx;
        pbi->dequant.idct_add_y_block    = vp8_dequant_idct_add_y_block_mmx;
        pbi->dequant.idct_add_uv_block   = vp8_dequant_idct_add_uv_block_mmx;
    }
#endif
#if HAVE_SSE2
    if (flags & HAS_SSE2)
    {
        pbi->dequant.dc_idct_add_y_block = vp8_dequant_dc_idct_add_y_block_sse2;
        pbi->dequant.idct_add_y_block    = vp8_dequant_idct_add_y_block_sse2;
        pbi->dequant.idct_add_uv_block   = vp8_dequant_idct_add_uv_block_sse2;
    }
#endif

#endif
}
