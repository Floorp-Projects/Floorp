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
#include "blockd.h"
#include "pragmas.h"
#include "postproc.h"
#include "dboolhuff.h"
#include "dequantize.h"
#include "onyxd_int.h"

void vp8_arch_arm_decode_init(VP8D_COMP *pbi)
{
#if CONFIG_RUNTIME_CPU_DETECT
    int flags = pbi->common.rtcd.flags;
    int has_edsp = flags & HAS_EDSP;
    int has_media = flags & HAS_MEDIA;
    int has_neon = flags & HAS_NEON;

#if HAVE_ARMV6
    if (has_media)
    {
        pbi->dequant.block               = vp8_dequantize_b_v6;
        pbi->dequant.idct_add            = vp8_dequant_idct_add_v6;
        pbi->dequant.dc_idct_add         = vp8_dequant_dc_idct_add_v6;
        pbi->dequant.dc_idct_add_y_block = vp8_dequant_dc_idct_add_y_block_v6;
        pbi->dequant.idct_add_y_block    = vp8_dequant_idct_add_y_block_v6;
        pbi->dequant.idct_add_uv_block   = vp8_dequant_idct_add_uv_block_v6;
#if 0 /*For use with RTCD, when implemented*/
        pbi->dboolhuff.start             = vp8dx_start_decode_c;
        pbi->dboolhuff.fill              = vp8dx_bool_decoder_fill_c;
        pbi->dboolhuff.debool            = vp8dx_decode_bool_c;
        pbi->dboolhuff.devalue           = vp8dx_decode_value_c;
#endif
    }
#endif

#if HAVE_ARMV7
    if (has_neon)
    {
        pbi->dequant.block               = vp8_dequantize_b_neon;
        pbi->dequant.idct_add            = vp8_dequant_idct_add_neon;
        /*This is not used: NEON always dequants two blocks at once.
        pbi->dequant.dc_idct_add         = vp8_dequant_dc_idct_add_neon;*/
        pbi->dequant.dc_idct_add_y_block = vp8_dequant_dc_idct_add_y_block_neon;
        pbi->dequant.idct_add_y_block    = vp8_dequant_idct_add_y_block_neon;
        pbi->dequant.idct_add_uv_block   = vp8_dequant_idct_add_uv_block_neon;
#if 0 /*For use with RTCD, when implemented*/
        pbi->dboolhuff.start             = vp8dx_start_decode_c;
        pbi->dboolhuff.fill              = vp8dx_bool_decoder_fill_c;
        pbi->dboolhuff.debool            = vp8dx_decode_bool_c;
        pbi->dboolhuff.devalue           = vp8dx_decode_value_c;
#endif
    }
#endif
#endif
}
