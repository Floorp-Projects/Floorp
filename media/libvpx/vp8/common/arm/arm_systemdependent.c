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
#include "g_common.h"
#include "pragmas.h"
#include "subpixel.h"
#include "loopfilter.h"
#include "recon.h"
#include "idct.h"
#include "onyxc_int.h"

extern void (*vp8_build_intra_predictors_mby_ptr)(MACROBLOCKD *x);
extern void vp8_build_intra_predictors_mby(MACROBLOCKD *x);
extern void vp8_build_intra_predictors_mby_neon(MACROBLOCKD *x);

extern void (*vp8_build_intra_predictors_mby_s_ptr)(MACROBLOCKD *x);
extern void vp8_build_intra_predictors_mby_s(MACROBLOCKD *x);
extern void vp8_build_intra_predictors_mby_s_neon(MACROBLOCKD *x);

void vp8_arch_arm_common_init(VP8_COMMON *ctx)
{
#if CONFIG_RUNTIME_CPU_DETECT
    VP8_COMMON_RTCD *rtcd = &ctx->rtcd;
    int flags = arm_cpu_caps();
    int has_edsp = flags & HAS_EDSP;
    int has_media = flags & HAS_MEDIA;
    int has_neon = flags & HAS_NEON;
    rtcd->flags = flags;

    /* Override default functions with fastest ones for this CPU. */
#if HAVE_ARMV6
    if (has_media)
    {
        rtcd->subpix.sixtap16x16   = vp8_sixtap_predict16x16_armv6;
        rtcd->subpix.sixtap8x8     = vp8_sixtap_predict8x8_armv6;
        rtcd->subpix.sixtap8x4     = vp8_sixtap_predict8x4_armv6;
        rtcd->subpix.sixtap4x4     = vp8_sixtap_predict_armv6;
        rtcd->subpix.bilinear16x16 = vp8_bilinear_predict16x16_armv6;
        rtcd->subpix.bilinear8x8   = vp8_bilinear_predict8x8_armv6;
        rtcd->subpix.bilinear8x4   = vp8_bilinear_predict8x4_armv6;
        rtcd->subpix.bilinear4x4   = vp8_bilinear_predict4x4_armv6;

        rtcd->idct.idct1        = vp8_short_idct4x4llm_1_v6;
        rtcd->idct.idct16       = vp8_short_idct4x4llm_v6_dual;
        rtcd->idct.iwalsh1      = vp8_short_inv_walsh4x4_1_v6;
        rtcd->idct.iwalsh16     = vp8_short_inv_walsh4x4_v6;

        rtcd->loopfilter.normal_mb_v = vp8_loop_filter_mbv_armv6;
        rtcd->loopfilter.normal_b_v  = vp8_loop_filter_bv_armv6;
        rtcd->loopfilter.normal_mb_h = vp8_loop_filter_mbh_armv6;
        rtcd->loopfilter.normal_b_h  = vp8_loop_filter_bh_armv6;
        rtcd->loopfilter.simple_mb_v = vp8_loop_filter_mbvs_armv6;
        rtcd->loopfilter.simple_b_v  = vp8_loop_filter_bvs_armv6;
        rtcd->loopfilter.simple_mb_h = vp8_loop_filter_mbhs_armv6;
        rtcd->loopfilter.simple_b_h  = vp8_loop_filter_bhs_armv6;

        rtcd->recon.copy16x16   = vp8_copy_mem16x16_v6;
        rtcd->recon.copy8x8     = vp8_copy_mem8x8_v6;
        rtcd->recon.copy8x4     = vp8_copy_mem8x4_v6;
        rtcd->recon.recon       = vp8_recon_b_armv6;
        rtcd->recon.recon2      = vp8_recon2b_armv6;
        rtcd->recon.recon4      = vp8_recon4b_armv6;
    }
#endif

#if HAVE_ARMV7
    if (has_neon)
    {
        rtcd->subpix.sixtap16x16   = vp8_sixtap_predict16x16_neon;
        rtcd->subpix.sixtap8x8     = vp8_sixtap_predict8x8_neon;
        rtcd->subpix.sixtap8x4     = vp8_sixtap_predict8x4_neon;
        rtcd->subpix.sixtap4x4     = vp8_sixtap_predict_neon;
        rtcd->subpix.bilinear16x16 = vp8_bilinear_predict16x16_neon;
        rtcd->subpix.bilinear8x8   = vp8_bilinear_predict8x8_neon;
        rtcd->subpix.bilinear8x4   = vp8_bilinear_predict8x4_neon;
        rtcd->subpix.bilinear4x4   = vp8_bilinear_predict4x4_neon;

        rtcd->idct.idct1        = vp8_short_idct4x4llm_1_neon;
        rtcd->idct.idct16       = vp8_short_idct4x4llm_neon;
        rtcd->idct.iwalsh1      = vp8_short_inv_walsh4x4_1_neon;
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
        rtcd->recon.recon       = vp8_recon_b_neon;
        rtcd->recon.recon2      = vp8_recon2b_neon;
        rtcd->recon.recon4      = vp8_recon4b_neon;
        rtcd->recon.recon_mb    = vp8_recon_mb_neon;

    }
#endif

#endif

#if HAVE_ARMV6
#if CONFIG_RUNTIME_CPU_DETECT
    if (has_media)
#endif
    {
        vp8_build_intra_predictors_mby_ptr = vp8_build_intra_predictors_mby;
        vp8_build_intra_predictors_mby_s_ptr = vp8_build_intra_predictors_mby_s;
    }
#endif

#if HAVE_ARMV7
#if CONFIG_RUNTIME_CPU_DETECT
    if (has_neon)
#endif
    {
        vp8_build_intra_predictors_mby_ptr =
         vp8_build_intra_predictors_mby_neon;
        vp8_build_intra_predictors_mby_s_ptr =
         vp8_build_intra_predictors_mby_s_neon;
    }
#endif
}
