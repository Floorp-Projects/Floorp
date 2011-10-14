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
#include "vp8/decoder/dequantize.h"
#include "vp8/decoder/onyxd_int.h"

extern void vp8_arch_x86_decode_init(VP8D_COMP *pbi);
extern void vp8_arch_arm_decode_init(VP8D_COMP *pbi);

void vp8_dmachine_specific_config(VP8D_COMP *pbi)
{
    /* Pure C: */
#if CONFIG_RUNTIME_CPU_DETECT
    pbi->mb.rtcd                     = &pbi->common.rtcd;
    pbi->dequant.block               = vp8_dequantize_b_c;
    pbi->dequant.idct_add            = vp8_dequant_idct_add_c;
    pbi->dequant.dc_idct_add         = vp8_dequant_dc_idct_add_c;
    pbi->dequant.dc_idct_add_y_block = vp8_dequant_dc_idct_add_y_block_c;
    pbi->dequant.idct_add_y_block    = vp8_dequant_idct_add_y_block_c;
    pbi->dequant.idct_add_uv_block   = vp8_dequant_idct_add_uv_block_c;
#endif

#if ARCH_X86 || ARCH_X86_64
    vp8_arch_x86_decode_init(pbi);
#endif

#if ARCH_ARM
    vp8_arch_arm_decode_init(pbi);
#endif
}
