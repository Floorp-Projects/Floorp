/*
 *  Copyright (c) 2010 The VP8 project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#include "vpx_ports/config.h"
#include "g_common.h"
#include "subpixel.h"
#include "loopfilter.h"
#include "recon.h"
#include "idct.h"
#include "onyxc_int.h"

extern void vp8_arch_x86_common_init(VP8_COMMON *ctx);

void (*vp8_build_intra_predictors_mby_ptr)(MACROBLOCKD *x);
extern void vp8_build_intra_predictors_mby(MACROBLOCKD *x);

void (*vp8_build_intra_predictors_mby_s_ptr)(MACROBLOCKD *x);
extern void vp8_build_intra_predictors_mby_s(MACROBLOCKD *x);

void vp8_machine_specific_config(VP8_COMMON *ctx)
{
#if CONFIG_RUNTIME_CPU_DETECT
    VP8_COMMON_RTCD *rtcd = &ctx->rtcd;

    rtcd->idct.idct1        = vp8_short_idct4x4llm_1_c;
    rtcd->idct.idct16       = vp8_short_idct4x4llm_c;
    rtcd->idct.idct1_scalar_add = vp8_dc_only_idct_add_c;
    rtcd->idct.iwalsh1      = vp8_short_inv_walsh4x4_1_c;
    rtcd->idct.iwalsh16     = vp8_short_inv_walsh4x4_c;

    rtcd->recon.copy16x16   = vp8_copy_mem16x16_c;
    rtcd->recon.copy8x8     = vp8_copy_mem8x8_c;
    rtcd->recon.copy8x4     = vp8_copy_mem8x4_c;
    rtcd->recon.recon      = vp8_recon_b_c;
    rtcd->recon.recon2      = vp8_recon2b_c;
    rtcd->recon.recon4     = vp8_recon4b_c;

    rtcd->subpix.sixtap16x16   = vp8_sixtap_predict16x16_c;
    rtcd->subpix.sixtap8x8     = vp8_sixtap_predict8x8_c;
    rtcd->subpix.sixtap8x4     = vp8_sixtap_predict8x4_c;
    rtcd->subpix.sixtap4x4     = vp8_sixtap_predict_c;
    rtcd->subpix.bilinear16x16 = vp8_bilinear_predict16x16_c;
    rtcd->subpix.bilinear8x8   = vp8_bilinear_predict8x8_c;
    rtcd->subpix.bilinear8x4   = vp8_bilinear_predict8x4_c;
    rtcd->subpix.bilinear4x4   = vp8_bilinear_predict4x4_c;

    rtcd->loopfilter.normal_mb_v = vp8_loop_filter_mbv_c;
    rtcd->loopfilter.normal_b_v  = vp8_loop_filter_bv_c;
    rtcd->loopfilter.normal_mb_h = vp8_loop_filter_mbh_c;
    rtcd->loopfilter.normal_b_h  = vp8_loop_filter_bh_c;
    rtcd->loopfilter.simple_mb_v = vp8_loop_filter_mbvs_c;
    rtcd->loopfilter.simple_b_v  = vp8_loop_filter_bvs_c;
    rtcd->loopfilter.simple_mb_h = vp8_loop_filter_mbhs_c;
    rtcd->loopfilter.simple_b_h  = vp8_loop_filter_bhs_c;

#if CONFIG_POSTPROC || (CONFIG_VP8_ENCODER && CONFIG_PSNR)
    rtcd->postproc.down        = vp8_mbpost_proc_down_c;
    rtcd->postproc.across      = vp8_mbpost_proc_across_ip_c;
    rtcd->postproc.downacross  = vp8_post_proc_down_and_across_c;
    rtcd->postproc.addnoise    = vp8_plane_add_noise_c;
#endif

#endif
    // Pure C:
    vp8_build_intra_predictors_mby_ptr = vp8_build_intra_predictors_mby;
    vp8_build_intra_predictors_mby_s_ptr = vp8_build_intra_predictors_mby_s;

#if ARCH_X86 || ARCH_X86_64
    vp8_arch_x86_common_init(ctx);
#endif

}
