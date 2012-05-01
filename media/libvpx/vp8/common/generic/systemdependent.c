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
#include "vp8/common/subpixel.h"
#include "vp8/common/loopfilter.h"
#include "vp8/common/recon.h"
#include "vp8/common/idct.h"
#include "vp8/common/variance.h"
#include "vp8/common/onyxc_int.h"

#if CONFIG_MULTITHREAD
#if HAVE_UNISTD_H
#include <unistd.h>
#elif defined(_WIN32)
#include <windows.h>
typedef void (WINAPI *PGNSI)(LPSYSTEM_INFO);
#endif
#endif

extern void vp8_arch_x86_common_init(VP8_COMMON *ctx);
extern void vp8_arch_arm_common_init(VP8_COMMON *ctx);

#if CONFIG_MULTITHREAD
static int get_cpu_count()
{
    int core_count = 16;

#if HAVE_UNISTD_H
#if defined(_SC_NPROCESSORS_ONLN)
    core_count = sysconf(_SC_NPROCESSORS_ONLN);
#elif defined(_SC_NPROC_ONLN)
    core_count = sysconf(_SC_NPROC_ONLN);
#endif
#elif defined(_WIN32)
    {
        PGNSI pGNSI;
        SYSTEM_INFO sysinfo;

        /* Call GetNativeSystemInfo if supported or
         * GetSystemInfo otherwise. */

        pGNSI = (PGNSI) GetProcAddress(
                GetModuleHandle(TEXT("kernel32.dll")), "GetNativeSystemInfo");
        if (pGNSI != NULL)
            pGNSI(&sysinfo);
        else
            GetSystemInfo(&sysinfo);

        core_count = sysinfo.dwNumberOfProcessors;
    }
#else
    /* other platforms */
#endif

    return core_count > 0 ? core_count : 1;
}
#endif

void vp8_machine_specific_config(VP8_COMMON *ctx)
{
#if CONFIG_RUNTIME_CPU_DETECT
    VP8_COMMON_RTCD *rtcd = &ctx->rtcd;


    rtcd->dequant.block             = vp8_dequantize_b_c;
    rtcd->dequant.idct_add          = vp8_dequant_idct_add_c;
    rtcd->dequant.idct_add_y_block  = vp8_dequant_idct_add_y_block_c;
    rtcd->dequant.idct_add_uv_block =
        vp8_dequant_idct_add_uv_block_c;


    rtcd->idct.idct16       = vp8_short_idct4x4llm_c;
    rtcd->idct.idct1_scalar_add = vp8_dc_only_idct_add_c;
    rtcd->idct.iwalsh1      = vp8_short_inv_walsh4x4_1_c;
    rtcd->idct.iwalsh16     = vp8_short_inv_walsh4x4_c;

    rtcd->recon.copy16x16   = vp8_copy_mem16x16_c;
    rtcd->recon.copy8x8     = vp8_copy_mem8x8_c;
    rtcd->recon.copy8x4     = vp8_copy_mem8x4_c;

    rtcd->recon.build_intra_predictors_mby =
        vp8_build_intra_predictors_mby;
    rtcd->recon.build_intra_predictors_mby_s =
        vp8_build_intra_predictors_mby_s;
    rtcd->recon.build_intra_predictors_mbuv =
        vp8_build_intra_predictors_mbuv;
    rtcd->recon.build_intra_predictors_mbuv_s =
        vp8_build_intra_predictors_mbuv_s;
    rtcd->recon.intra4x4_predict =
        vp8_intra4x4_predict_c;

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
    rtcd->loopfilter.simple_mb_v = vp8_loop_filter_simple_vertical_edge_c;
    rtcd->loopfilter.simple_b_v  = vp8_loop_filter_bvs_c;
    rtcd->loopfilter.simple_mb_h = vp8_loop_filter_simple_horizontal_edge_c;
    rtcd->loopfilter.simple_b_h  = vp8_loop_filter_bhs_c;

    rtcd->variance.sad16x16              = vp8_sad16x16_c;
    rtcd->variance.sad16x8               = vp8_sad16x8_c;
    rtcd->variance.sad8x16               = vp8_sad8x16_c;
    rtcd->variance.sad8x8                = vp8_sad8x8_c;
    rtcd->variance.sad4x4                = vp8_sad4x4_c;

    rtcd->variance.sad16x16x3            = vp8_sad16x16x3_c;
    rtcd->variance.sad16x8x3             = vp8_sad16x8x3_c;
    rtcd->variance.sad8x16x3             = vp8_sad8x16x3_c;
    rtcd->variance.sad8x8x3              = vp8_sad8x8x3_c;
    rtcd->variance.sad4x4x3              = vp8_sad4x4x3_c;

    rtcd->variance.sad16x16x8            = vp8_sad16x16x8_c;
    rtcd->variance.sad16x8x8             = vp8_sad16x8x8_c;
    rtcd->variance.sad8x16x8             = vp8_sad8x16x8_c;
    rtcd->variance.sad8x8x8              = vp8_sad8x8x8_c;
    rtcd->variance.sad4x4x8              = vp8_sad4x4x8_c;

    rtcd->variance.sad16x16x4d           = vp8_sad16x16x4d_c;
    rtcd->variance.sad16x8x4d            = vp8_sad16x8x4d_c;
    rtcd->variance.sad8x16x4d            = vp8_sad8x16x4d_c;
    rtcd->variance.sad8x8x4d             = vp8_sad8x8x4d_c;
    rtcd->variance.sad4x4x4d             = vp8_sad4x4x4d_c;
#if ARCH_X86 || ARCH_X86_64
    rtcd->variance.copy32xn              = vp8_copy32xn_c;
#endif
    rtcd->variance.var4x4                = vp8_variance4x4_c;
    rtcd->variance.var8x8                = vp8_variance8x8_c;
    rtcd->variance.var8x16               = vp8_variance8x16_c;
    rtcd->variance.var16x8               = vp8_variance16x8_c;
    rtcd->variance.var16x16              = vp8_variance16x16_c;

    rtcd->variance.subpixvar4x4          = vp8_sub_pixel_variance4x4_c;
    rtcd->variance.subpixvar8x8          = vp8_sub_pixel_variance8x8_c;
    rtcd->variance.subpixvar8x16         = vp8_sub_pixel_variance8x16_c;
    rtcd->variance.subpixvar16x8         = vp8_sub_pixel_variance16x8_c;
    rtcd->variance.subpixvar16x16        = vp8_sub_pixel_variance16x16_c;
    rtcd->variance.halfpixvar16x16_h     = vp8_variance_halfpixvar16x16_h_c;
    rtcd->variance.halfpixvar16x16_v     = vp8_variance_halfpixvar16x16_v_c;
    rtcd->variance.halfpixvar16x16_hv    = vp8_variance_halfpixvar16x16_hv_c;
    rtcd->variance.subpixmse16x16        = vp8_sub_pixel_mse16x16_c;

    rtcd->variance.mse16x16              = vp8_mse16x16_c;
    rtcd->variance.getmbss               = vp8_get_mb_ss_c;

    rtcd->variance.get4x4sse_cs          = vp8_get4x4sse_cs_c;
#if CONFIG_INTERNAL_STATS
    rtcd->variance.ssimpf_8x8            = vp8_ssim_parms_8x8_c;
    rtcd->variance.ssimpf_16x16          = vp8_ssim_parms_16x16_c;
#endif

#if CONFIG_POSTPROC || (CONFIG_VP8_ENCODER && CONFIG_INTERNAL_STATS)
    rtcd->postproc.down             = vp8_mbpost_proc_down_c;
    rtcd->postproc.across           = vp8_mbpost_proc_across_ip_c;
    rtcd->postproc.downacross       = vp8_post_proc_down_and_across_c;
    rtcd->postproc.addnoise         = vp8_plane_add_noise_c;
    rtcd->postproc.blend_mb_inner   = vp8_blend_mb_inner_c;
    rtcd->postproc.blend_mb_outer   = vp8_blend_mb_outer_c;
    rtcd->postproc.blend_b          = vp8_blend_b_c;
#endif

#endif

#if ARCH_X86 || ARCH_X86_64
    vp8_arch_x86_common_init(ctx);
#endif

#if ARCH_ARM
    vp8_arch_arm_common_init(ctx);
#endif

#if CONFIG_MULTITHREAD
    ctx->processor_core_count = get_cpu_count();
#endif /* CONFIG_MULTITHREAD */
}
