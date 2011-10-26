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
#include "vpx_ports/arm.h"
#include "vp8/common/blockd.h"
#include "vp8/common/pragmas.h"
#include "vp8/decoder/dequantize.h"
#include "vp8/decoder/onyxd_int.h"

void vp8_arch_arm_decode_init(VP8D_COMP *pbi)
{
#if CONFIG_RUNTIME_CPU_DETECT
    int flags = pbi->common.rtcd.flags;

#if HAVE_ARMV5TE
    if (flags & HAS_EDSP)
    {
    }
#endif

#if HAVE_ARMV6
    if (flags & HAS_MEDIA)
    {
        pbi->dequant.block               = vp8_dequantize_b_v6;
        pbi->dequant.idct_add            = vp8_dequant_idct_add_v6;
        pbi->dequant.dc_idct_add         = vp8_dequant_dc_idct_add_v6;
        pbi->dequant.dc_idct_add_y_block = vp8_dequant_dc_idct_add_y_block_v6;
        pbi->dequant.idct_add_y_block    = vp8_dequant_idct_add_y_block_v6;
        pbi->dequant.idct_add_uv_block   = vp8_dequant_idct_add_uv_block_v6;
    }
#endif

#if HAVE_ARMV7
    if (flags & HAS_NEON)
    {
        pbi->dequant.block               = vp8_dequantize_b_neon;
        pbi->dequant.idct_add            = vp8_dequant_idct_add_neon;
        /*This is not used: NEON always dequants two blocks at once.
        pbi->dequant.dc_idct_add         = vp8_dequant_dc_idct_add_neon;*/
        pbi->dequant.dc_idct_add_y_block = vp8_dequant_dc_idct_add_y_block_neon;
        pbi->dequant.idct_add_y_block    = vp8_dequant_idct_add_y_block_neon;
        pbi->dequant.idct_add_uv_block   = vp8_dequant_idct_add_uv_block_neon;
    }
#endif
#endif
}
