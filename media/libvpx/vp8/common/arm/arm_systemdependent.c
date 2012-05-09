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
#include "vpx_ports/arm.h"
#include "vp8/common/pragmas.h"
#include "vp8/common/subpixel.h"
#include "vp8/common/loopfilter.h"
#include "vp8/common/recon.h"
#include "vp8/common/idct.h"
#include "vp8/common/variance.h"
#include "vp8/common/onyxc_int.h"

void vp8_arch_arm_common_init(VP8_COMMON *ctx)
{
#if CONFIG_RUNTIME_CPU_DETECT
    VP8_COMMON_RTCD *rtcd = &ctx->rtcd;
    int flags = arm_cpu_caps();
    rtcd->flags = flags;

    /* Override default functions with fastest ones for this CPU. */
#if HAVE_ARMV5TE
    if (flags & HAS_EDSP)
    {
    }
#endif

#if HAVE_ARMV6
    if (flags & HAS_MEDIA)
    {
        rtcd->subpix.sixtap16x16   = vp8_sixtap_predict16x16_armv6;
        rtcd->subpix.sixtap8x8     = vp8_sixtap_predict8x8_armv6;
        rtcd->subpix.sixtap8x4     = vp8_sixtap_predict8x4_armv6;
        rtcd->subpix.sixtap4x4     = vp8_sixtap_predict_armv6;
        rtcd->subpix.bilinear16x16 = vp8_bilinear_predict16x16_armv6;
        rtcd->subpix.bilinear8x8   = vp8_bilinear_predict8x8_armv6;
        rtcd->subpix.bilinear8x4   = vp8_bilinear_predict8x4_armv6;
        rtcd->subpix.bilinear4x4   = vp8_bilinear_predict4x4_armv6;

        rtcd->idct.idct16       = vp8_short_idct4x4llm_v6_dual;
        rtcd->idct.iwalsh16     = vp8_short_inv_walsh4x4_v6;

        rtcd->loopfilter.normal_mb_v = vp8_loop_filter_mbv_armv6;
        rtcd->loopfilter.normal_b_v  = vp8_loop_filter_bv_armv6;
        rtcd->loopfilter.normal_mb_h = vp8_loop_filter_mbh_armv6;
        rtcd->loopfilter.normal_b_h  = vp8_loop_filter_bh_armv6;
        rtcd->loopfilter.simple_mb_v =
                vp8_loop_filter_simple_vertical_edge_armv6;
        rtcd->loopfilter.simple_b_v  = vp8_loop_filter_bvs_armv6;
        rtcd->loopfilter.simple_mb_h =
                vp8_loop_filter_simple_horizontal_edge_armv6;
        rtcd->loopfilter.simple_b_h  = vp8_loop_filter_bhs_armv6;

        rtcd->recon.copy16x16   = vp8_copy_mem16x16_v6;
        rtcd->recon.copy8x8     = vp8_copy_mem8x8_v6;
        rtcd->recon.copy8x4     = vp8_copy_mem8x4_v6;
        rtcd->recon.intra4x4_predict = vp8_intra4x4_predict_armv6;

        rtcd->dequant.block               = vp8_dequantize_b_v6;
        rtcd->dequant.idct_add            = vp8_dequant_idct_add_v6;
        rtcd->dequant.idct_add_y_block    = vp8_dequant_idct_add_y_block_v6;
        rtcd->dequant.idct_add_uv_block   = vp8_dequant_idct_add_uv_block_v6;

        rtcd->variance.sad16x16              = vp8_sad16x16_armv6;
        /*rtcd->variance.sad16x8               = vp8_sad16x8_c;
        rtcd->variance.sad8x16               = vp8_sad8x16_c;
        rtcd->variance.sad8x8                = vp8_sad8x8_c;
        rtcd->variance.sad4x4                = vp8_sad4x4_c;*/

        /*rtcd->variance.var4x4                = vp8_variance4x4_c;*/
        rtcd->variance.var8x8                = vp8_variance8x8_armv6;
        /*rtcd->variance.var8x16               = vp8_variance8x16_c;
        rtcd->variance.var16x8               = vp8_variance16x8_c;*/
        rtcd->variance.var16x16              = vp8_variance16x16_armv6;

        /*rtcd->variance.subpixvar4x4          = vp8_sub_pixel_variance4x4_c;*/
        rtcd->variance.subpixvar8x8          = vp8_sub_pixel_variance8x8_armv6;
        /*rtcd->variance.subpixvar8x16         = vp8_sub_pixel_variance8x16_c;
        rtcd->variance.subpixvar16x8         = vp8_sub_pixel_variance16x8_c;*/
        rtcd->variance.subpixvar16x16        = vp8_sub_pixel_variance16x16_armv6;
        rtcd->variance.halfpixvar16x16_h     = vp8_variance_halfpixvar16x16_h_armv6;
        rtcd->variance.halfpixvar16x16_v     = vp8_variance_halfpixvar16x16_v_armv6;
        rtcd->variance.halfpixvar16x16_hv    = vp8_variance_halfpixvar16x16_hv_armv6;

        rtcd->variance.mse16x16              = vp8_mse16x16_armv6;
        /*rtcd->variance.getmbss               = vp8_get_mb_ss_c;*/

        /*rtcd->variance.get4x4sse_cs          = vp8_get4x4sse_cs_c;*/
    }
#endif

#if HAVE_ARMV7
    if (flags & HAS_NEON)
    {
        rtcd->subpix.sixtap16x16   = vp8_sixtap_predict16x16_neon;
        rtcd->subpix.sixtap8x8     = vp8_sixtap_predict8x8_neon;
        rtcd->subpix.sixtap8x4     = vp8_sixtap_predict8x4_neon;
        rtcd->subpix.sixtap4x4     = vp8_sixtap_predict_neon;
        rtcd->subpix.bilinear16x16 = vp8_bilinear_predict16x16_neon;
        rtcd->subpix.bilinear8x8   = vp8_bilinear_predict8x8_neon;
        rtcd->subpix.bilinear8x4   = vp8_bilinear_predict8x4_neon;
        rtcd->subpix.bilinear4x4   = vp8_bilinear_predict4x4_neon;

        rtcd->idct.idct16       = vp8_short_idct4x4llm_neon;
        rtcd->idct.iwalsh16     = vp8_short_inv_walsh4x4_neon;

        rtcd->loopfilter.normal_mb_v = vp8_loop_filter_mbv_neon;
        rtcd->loopfilter.normal_b_v  = vp8_loop_filter_bv_neon;
        rtcd->loopfilter.normal_mb_h = vp8_loop_filter_mbh_neon;
        rtcd->loopfilter.normal_b_h  = vp8_loop_filter_bh_neon;
        rtcd->loopfilter.simple_mb_v = vp8_loop_filter_mbvs_neon;
        rtcd->loopfilter.simple_b_v  = vp8_loop_filter_bvs_neon;
        rtcd->loopfilter.simple_mb_h = vp8_loop_filter_mbhs_neon;
        rtcd->loopfilter.simple_b_h  = vp8_loop_filter_bhs_neon;

        rtcd->recon.copy16x16   = vp8_copy_mem16x16_neon;
        rtcd->recon.copy8x8     = vp8_copy_mem8x8_neon;
        rtcd->recon.copy8x4     = vp8_copy_mem8x4_neon;
        rtcd->recon.build_intra_predictors_mby =
            vp8_build_intra_predictors_mby_neon;
        rtcd->recon.build_intra_predictors_mby_s =
            vp8_build_intra_predictors_mby_s_neon;

        rtcd->dequant.block               = vp8_dequantize_b_neon;
        rtcd->dequant.idct_add            = vp8_dequant_idct_add_neon;
        rtcd->dequant.idct_add_y_block    = vp8_dequant_idct_add_y_block_neon;
        rtcd->dequant.idct_add_uv_block   = vp8_dequant_idct_add_uv_block_neon;

        rtcd->variance.sad16x16              = vp8_sad16x16_neon;
        rtcd->variance.sad16x8               = vp8_sad16x8_neon;
        rtcd->variance.sad8x16               = vp8_sad8x16_neon;
        rtcd->variance.sad8x8                = vp8_sad8x8_neon;
        rtcd->variance.sad4x4                = vp8_sad4x4_neon;

        /*rtcd->variance.var4x4                = vp8_variance4x4_c;*/
        rtcd->variance.var8x8                = vp8_variance8x8_neon;
        rtcd->variance.var8x16               = vp8_variance8x16_neon;
        rtcd->variance.var16x8               = vp8_variance16x8_neon;
        rtcd->variance.var16x16              = vp8_variance16x16_neon;

        /*rtcd->variance.subpixvar4x4          = vp8_sub_pixel_variance4x4_c;*/
        rtcd->variance.subpixvar8x8          = vp8_sub_pixel_variance8x8_neon;
        /*rtcd->variance.subpixvar8x16         = vp8_sub_pixel_variance8x16_c;
        rtcd->variance.subpixvar16x8         = vp8_sub_pixel_variance16x8_c;*/
        rtcd->variance.subpixvar16x16        = vp8_sub_pixel_variance16x16_neon;
        rtcd->variance.halfpixvar16x16_h     = vp8_variance_halfpixvar16x16_h_neon;
        rtcd->variance.halfpixvar16x16_v     = vp8_variance_halfpixvar16x16_v_neon;
        rtcd->variance.halfpixvar16x16_hv    = vp8_variance_halfpixvar16x16_hv_neon;

        rtcd->variance.mse16x16              = vp8_mse16x16_neon;
        /*rtcd->variance.getmbss               = vp8_get_mb_ss_c;*/

        rtcd->variance.get4x4sse_cs          = vp8_get4x4sse_cs_neon;
    }
#endif

#endif
}
