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
#include "vpx_ports/x86.h"
#include "vp8/common/subpixel.h"
#include "vp8/common/loopfilter.h"
#include "vp8/common/recon.h"
#include "vp8/common/idct.h"
#include "vp8/common/variance.h"
#include "vp8/common/pragmas.h"
#include "vp8/common/onyxc_int.h"

void vp8_arch_x86_common_init(VP8_COMMON *ctx)
{
#if CONFIG_RUNTIME_CPU_DETECT
    VP8_COMMON_RTCD *rtcd = &ctx->rtcd;
    int flags = x86_simd_caps();

    /* Note:
     *
     * This platform can be built without runtime CPU detection as well. If
     * you modify any of the function mappings present in this file, be sure
     * to also update them in static mapings (<arch>/filename_<arch>.h)
     */

    /* Override default functions with fastest ones for this CPU. */
#if HAVE_MMX

    if (flags & HAS_MMX)
    {
        rtcd->dequant.block               = vp8_dequantize_b_mmx;
        rtcd->dequant.idct_add            = vp8_dequant_idct_add_mmx;
        rtcd->dequant.idct_add_y_block    = vp8_dequant_idct_add_y_block_mmx;
        rtcd->dequant.idct_add_uv_block   = vp8_dequant_idct_add_uv_block_mmx;

        rtcd->idct.idct16       = vp8_short_idct4x4llm_mmx;
        rtcd->idct.idct1_scalar_add = vp8_dc_only_idct_add_mmx;
        rtcd->idct.iwalsh16     = vp8_short_inv_walsh4x4_mmx;

        rtcd->recon.copy8x8     = vp8_copy_mem8x8_mmx;
        rtcd->recon.copy8x4     = vp8_copy_mem8x4_mmx;
        rtcd->recon.copy16x16   = vp8_copy_mem16x16_mmx;

        rtcd->subpix.sixtap16x16   = vp8_sixtap_predict16x16_mmx;
        rtcd->subpix.sixtap8x8     = vp8_sixtap_predict8x8_mmx;
        rtcd->subpix.sixtap8x4     = vp8_sixtap_predict8x4_mmx;
        rtcd->subpix.sixtap4x4     = vp8_sixtap_predict4x4_mmx;
        rtcd->subpix.bilinear16x16 = vp8_bilinear_predict16x16_mmx;
        rtcd->subpix.bilinear8x8   = vp8_bilinear_predict8x8_mmx;
        rtcd->subpix.bilinear8x4   = vp8_bilinear_predict8x4_mmx;
        rtcd->subpix.bilinear4x4   = vp8_bilinear_predict4x4_mmx;

        rtcd->loopfilter.normal_mb_v = vp8_loop_filter_mbv_mmx;
        rtcd->loopfilter.normal_b_v  = vp8_loop_filter_bv_mmx;
        rtcd->loopfilter.normal_mb_h = vp8_loop_filter_mbh_mmx;
        rtcd->loopfilter.normal_b_h  = vp8_loop_filter_bh_mmx;
        rtcd->loopfilter.simple_mb_v = vp8_loop_filter_simple_vertical_edge_mmx;
        rtcd->loopfilter.simple_b_v  = vp8_loop_filter_bvs_mmx;
        rtcd->loopfilter.simple_mb_h = vp8_loop_filter_simple_horizontal_edge_mmx;
        rtcd->loopfilter.simple_b_h  = vp8_loop_filter_bhs_mmx;

        rtcd->variance.sad16x16              = vp8_sad16x16_mmx;
        rtcd->variance.sad16x8               = vp8_sad16x8_mmx;
        rtcd->variance.sad8x16               = vp8_sad8x16_mmx;
        rtcd->variance.sad8x8                = vp8_sad8x8_mmx;
        rtcd->variance.sad4x4                = vp8_sad4x4_mmx;

        rtcd->variance.var4x4                = vp8_variance4x4_mmx;
        rtcd->variance.var8x8                = vp8_variance8x8_mmx;
        rtcd->variance.var8x16               = vp8_variance8x16_mmx;
        rtcd->variance.var16x8               = vp8_variance16x8_mmx;
        rtcd->variance.var16x16              = vp8_variance16x16_mmx;

        rtcd->variance.subpixvar4x4          = vp8_sub_pixel_variance4x4_mmx;
        rtcd->variance.subpixvar8x8          = vp8_sub_pixel_variance8x8_mmx;
        rtcd->variance.subpixvar8x16         = vp8_sub_pixel_variance8x16_mmx;
        rtcd->variance.subpixvar16x8         = vp8_sub_pixel_variance16x8_mmx;
        rtcd->variance.subpixvar16x16        = vp8_sub_pixel_variance16x16_mmx;
        rtcd->variance.halfpixvar16x16_h     = vp8_variance_halfpixvar16x16_h_mmx;
        rtcd->variance.halfpixvar16x16_v     = vp8_variance_halfpixvar16x16_v_mmx;
        rtcd->variance.halfpixvar16x16_hv    = vp8_variance_halfpixvar16x16_hv_mmx;
        rtcd->variance.subpixmse16x16        = vp8_sub_pixel_mse16x16_mmx;

        rtcd->variance.mse16x16              = vp8_mse16x16_mmx;
        rtcd->variance.getmbss               = vp8_get_mb_ss_mmx;

        rtcd->variance.get4x4sse_cs          = vp8_get4x4sse_cs_mmx;

#if CONFIG_POSTPROC
        rtcd->postproc.down        = vp8_mbpost_proc_down_mmx;
        /*rtcd->postproc.across      = vp8_mbpost_proc_across_ip_c;*/
        rtcd->postproc.downacross  = vp8_post_proc_down_and_across_mmx;
        rtcd->postproc.addnoise    = vp8_plane_add_noise_mmx;
#endif
    }

#endif
#if HAVE_SSE2

    if (flags & HAS_SSE2)
    {
        rtcd->recon.copy16x16   = vp8_copy_mem16x16_sse2;
        rtcd->recon.build_intra_predictors_mbuv =
            vp8_build_intra_predictors_mbuv_sse2;
        rtcd->recon.build_intra_predictors_mbuv_s =
            vp8_build_intra_predictors_mbuv_s_sse2;
        rtcd->recon.build_intra_predictors_mby =
            vp8_build_intra_predictors_mby_sse2;
        rtcd->recon.build_intra_predictors_mby_s =
            vp8_build_intra_predictors_mby_s_sse2;

        rtcd->dequant.idct_add_y_block    = vp8_dequant_idct_add_y_block_sse2;
        rtcd->dequant.idct_add_uv_block   = vp8_dequant_idct_add_uv_block_sse2;

        rtcd->idct.iwalsh16     = vp8_short_inv_walsh4x4_sse2;

        rtcd->subpix.sixtap16x16   = vp8_sixtap_predict16x16_sse2;
        rtcd->subpix.sixtap8x8     = vp8_sixtap_predict8x8_sse2;
        rtcd->subpix.sixtap8x4     = vp8_sixtap_predict8x4_sse2;
        rtcd->subpix.bilinear16x16 = vp8_bilinear_predict16x16_sse2;
        rtcd->subpix.bilinear8x8   = vp8_bilinear_predict8x8_sse2;

        rtcd->loopfilter.normal_mb_v = vp8_loop_filter_mbv_sse2;
        rtcd->loopfilter.normal_b_v  = vp8_loop_filter_bv_sse2;
        rtcd->loopfilter.normal_mb_h = vp8_loop_filter_mbh_sse2;
        rtcd->loopfilter.normal_b_h  = vp8_loop_filter_bh_sse2;
        rtcd->loopfilter.simple_mb_v = vp8_loop_filter_simple_vertical_edge_sse2;
        rtcd->loopfilter.simple_b_v  = vp8_loop_filter_bvs_sse2;
        rtcd->loopfilter.simple_mb_h = vp8_loop_filter_simple_horizontal_edge_sse2;
        rtcd->loopfilter.simple_b_h  = vp8_loop_filter_bhs_sse2;

        rtcd->variance.sad16x16              = vp8_sad16x16_wmt;
        rtcd->variance.sad16x8               = vp8_sad16x8_wmt;
        rtcd->variance.sad8x16               = vp8_sad8x16_wmt;
        rtcd->variance.sad8x8                = vp8_sad8x8_wmt;
        rtcd->variance.sad4x4                = vp8_sad4x4_wmt;
        rtcd->variance.copy32xn              = vp8_copy32xn_sse2;

        rtcd->variance.var4x4                = vp8_variance4x4_wmt;
        rtcd->variance.var8x8                = vp8_variance8x8_wmt;
        rtcd->variance.var8x16               = vp8_variance8x16_wmt;
        rtcd->variance.var16x8               = vp8_variance16x8_wmt;
        rtcd->variance.var16x16              = vp8_variance16x16_wmt;

        rtcd->variance.subpixvar4x4          = vp8_sub_pixel_variance4x4_wmt;
        rtcd->variance.subpixvar8x8          = vp8_sub_pixel_variance8x8_wmt;
        rtcd->variance.subpixvar8x16         = vp8_sub_pixel_variance8x16_wmt;
        rtcd->variance.subpixvar16x8         = vp8_sub_pixel_variance16x8_wmt;
        rtcd->variance.subpixvar16x16        = vp8_sub_pixel_variance16x16_wmt;
        rtcd->variance.halfpixvar16x16_h     = vp8_variance_halfpixvar16x16_h_wmt;
        rtcd->variance.halfpixvar16x16_v     = vp8_variance_halfpixvar16x16_v_wmt;
        rtcd->variance.halfpixvar16x16_hv    = vp8_variance_halfpixvar16x16_hv_wmt;
        rtcd->variance.subpixmse16x16        = vp8_sub_pixel_mse16x16_wmt;

        rtcd->variance.mse16x16              = vp8_mse16x16_wmt;
        rtcd->variance.getmbss               = vp8_get_mb_ss_sse2;

        /* rtcd->variance.get4x4sse_cs  not implemented for wmt */;

#if CONFIG_INTERNAL_STATS
#if ARCH_X86_64
        rtcd->variance.ssimpf_8x8            = vp8_ssim_parms_8x8_sse2;
        rtcd->variance.ssimpf_16x16          = vp8_ssim_parms_16x16_sse2;
#endif
#endif

#if CONFIG_POSTPROC
        rtcd->postproc.down        = vp8_mbpost_proc_down_xmm;
        rtcd->postproc.across      = vp8_mbpost_proc_across_ip_xmm;
        rtcd->postproc.downacross  = vp8_post_proc_down_and_across_xmm;
        rtcd->postproc.addnoise    = vp8_plane_add_noise_wmt;
#endif
    }

#endif

#if HAVE_SSE3

    if (flags & HAS_SSE3)
    {
        rtcd->variance.sad16x16              = vp8_sad16x16_sse3;
        rtcd->variance.sad16x16x3            = vp8_sad16x16x3_sse3;
        rtcd->variance.sad16x8x3             = vp8_sad16x8x3_sse3;
        rtcd->variance.sad8x16x3             = vp8_sad8x16x3_sse3;
        rtcd->variance.sad8x8x3              = vp8_sad8x8x3_sse3;
        rtcd->variance.sad4x4x3              = vp8_sad4x4x3_sse3;
        rtcd->variance.sad16x16x4d           = vp8_sad16x16x4d_sse3;
        rtcd->variance.sad16x8x4d            = vp8_sad16x8x4d_sse3;
        rtcd->variance.sad8x16x4d            = vp8_sad8x16x4d_sse3;
        rtcd->variance.sad8x8x4d             = vp8_sad8x8x4d_sse3;
        rtcd->variance.sad4x4x4d             = vp8_sad4x4x4d_sse3;
        rtcd->variance.copy32xn              = vp8_copy32xn_sse3;

    }
#endif

#if HAVE_SSSE3

    if (flags & HAS_SSSE3)
    {
        rtcd->subpix.sixtap16x16   = vp8_sixtap_predict16x16_ssse3;
        rtcd->subpix.sixtap8x8     = vp8_sixtap_predict8x8_ssse3;
        rtcd->subpix.sixtap8x4     = vp8_sixtap_predict8x4_ssse3;
        rtcd->subpix.sixtap4x4     = vp8_sixtap_predict4x4_ssse3;
        rtcd->subpix.bilinear16x16 = vp8_bilinear_predict16x16_ssse3;
        rtcd->subpix.bilinear8x8   = vp8_bilinear_predict8x8_ssse3;

        rtcd->recon.build_intra_predictors_mbuv =
            vp8_build_intra_predictors_mbuv_ssse3;
        rtcd->recon.build_intra_predictors_mbuv_s =
            vp8_build_intra_predictors_mbuv_s_ssse3;
        rtcd->recon.build_intra_predictors_mby =
            vp8_build_intra_predictors_mby_ssse3;
        rtcd->recon.build_intra_predictors_mby_s =
            vp8_build_intra_predictors_mby_s_ssse3;

        rtcd->variance.sad16x16x3            = vp8_sad16x16x3_ssse3;
        rtcd->variance.sad16x8x3             = vp8_sad16x8x3_ssse3;

        rtcd->variance.subpixvar16x8         = vp8_sub_pixel_variance16x8_ssse3;
        rtcd->variance.subpixvar16x16        = vp8_sub_pixel_variance16x16_ssse3;
    }
#endif

#if HAVE_SSE4_1
    if (flags & HAS_SSE4_1)
    {
        rtcd->variance.sad16x16x8            = vp8_sad16x16x8_sse4;
        rtcd->variance.sad16x8x8             = vp8_sad16x8x8_sse4;
        rtcd->variance.sad8x16x8             = vp8_sad8x16x8_sse4;
        rtcd->variance.sad8x8x8              = vp8_sad8x8x8_sse4;
        rtcd->variance.sad4x4x8              = vp8_sad4x4x8_sse4;
    }
#endif

#endif
}
