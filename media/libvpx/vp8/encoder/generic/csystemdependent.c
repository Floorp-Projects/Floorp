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
#include "vp8/encoder/variance.h"
#include "vp8/encoder/onyx_int.h"


void vp8_arch_x86_encoder_init(VP8_COMP *cpi);
void vp8_arch_arm_encoder_init(VP8_COMP *cpi);

void (*vp8_yv12_copy_partial_frame_ptr)(YV12_BUFFER_CONFIG *src_ybc, YV12_BUFFER_CONFIG *dst_ybc, int Fraction);
extern void vp8_yv12_copy_partial_frame(YV12_BUFFER_CONFIG *src_ybc, YV12_BUFFER_CONFIG *dst_ybc, int Fraction);

void vp8_cmachine_specific_config(VP8_COMP *cpi)
{
#if CONFIG_RUNTIME_CPU_DETECT
    cpi->rtcd.common                    = &cpi->common.rtcd;
    cpi->rtcd.variance.sad16x16              = vp8_sad16x16_c;
    cpi->rtcd.variance.sad16x8               = vp8_sad16x8_c;
    cpi->rtcd.variance.sad8x16               = vp8_sad8x16_c;
    cpi->rtcd.variance.sad8x8                = vp8_sad8x8_c;
    cpi->rtcd.variance.sad4x4                = vp8_sad4x4_c;

    cpi->rtcd.variance.sad16x16x3            = vp8_sad16x16x3_c;
    cpi->rtcd.variance.sad16x8x3             = vp8_sad16x8x3_c;
    cpi->rtcd.variance.sad8x16x3             = vp8_sad8x16x3_c;
    cpi->rtcd.variance.sad8x8x3              = vp8_sad8x8x3_c;
    cpi->rtcd.variance.sad4x4x3              = vp8_sad4x4x3_c;

    cpi->rtcd.variance.sad16x16x8            = vp8_sad16x16x8_c;
    cpi->rtcd.variance.sad16x8x8             = vp8_sad16x8x8_c;
    cpi->rtcd.variance.sad8x16x8             = vp8_sad8x16x8_c;
    cpi->rtcd.variance.sad8x8x8              = vp8_sad8x8x8_c;
    cpi->rtcd.variance.sad4x4x8              = vp8_sad4x4x8_c;

    cpi->rtcd.variance.sad16x16x4d           = vp8_sad16x16x4d_c;
    cpi->rtcd.variance.sad16x8x4d            = vp8_sad16x8x4d_c;
    cpi->rtcd.variance.sad8x16x4d            = vp8_sad8x16x4d_c;
    cpi->rtcd.variance.sad8x8x4d             = vp8_sad8x8x4d_c;
    cpi->rtcd.variance.sad4x4x4d             = vp8_sad4x4x4d_c;
#if ARCH_X86 || ARCH_X86_64
    cpi->rtcd.variance.copy32xn              = vp8_copy32xn_c;
#endif
    cpi->rtcd.variance.var4x4                = vp8_variance4x4_c;
    cpi->rtcd.variance.var8x8                = vp8_variance8x8_c;
    cpi->rtcd.variance.var8x16               = vp8_variance8x16_c;
    cpi->rtcd.variance.var16x8               = vp8_variance16x8_c;
    cpi->rtcd.variance.var16x16              = vp8_variance16x16_c;

    cpi->rtcd.variance.subpixvar4x4          = vp8_sub_pixel_variance4x4_c;
    cpi->rtcd.variance.subpixvar8x8          = vp8_sub_pixel_variance8x8_c;
    cpi->rtcd.variance.subpixvar8x16         = vp8_sub_pixel_variance8x16_c;
    cpi->rtcd.variance.subpixvar16x8         = vp8_sub_pixel_variance16x8_c;
    cpi->rtcd.variance.subpixvar16x16        = vp8_sub_pixel_variance16x16_c;
    cpi->rtcd.variance.halfpixvar16x16_h     = vp8_variance_halfpixvar16x16_h_c;
    cpi->rtcd.variance.halfpixvar16x16_v     = vp8_variance_halfpixvar16x16_v_c;
    cpi->rtcd.variance.halfpixvar16x16_hv    = vp8_variance_halfpixvar16x16_hv_c;
    cpi->rtcd.variance.subpixmse16x16        = vp8_sub_pixel_mse16x16_c;

    cpi->rtcd.variance.mse16x16              = vp8_mse16x16_c;
    cpi->rtcd.variance.getmbss               = vp8_get_mb_ss_c;

    cpi->rtcd.variance.get4x4sse_cs          = vp8_get4x4sse_cs_c;

    cpi->rtcd.fdct.short4x4                  = vp8_short_fdct4x4_c;
    cpi->rtcd.fdct.short8x4                  = vp8_short_fdct8x4_c;
    cpi->rtcd.fdct.fast4x4                   = vp8_short_fdct4x4_c;
    cpi->rtcd.fdct.fast8x4                   = vp8_short_fdct8x4_c;
    cpi->rtcd.fdct.walsh_short4x4            = vp8_short_walsh4x4_c;

    cpi->rtcd.encodemb.berr                  = vp8_block_error_c;
    cpi->rtcd.encodemb.mberr                 = vp8_mbblock_error_c;
    cpi->rtcd.encodemb.mbuverr               = vp8_mbuverror_c;
    cpi->rtcd.encodemb.subb                  = vp8_subtract_b_c;
    cpi->rtcd.encodemb.submby                = vp8_subtract_mby_c;
    cpi->rtcd.encodemb.submbuv               = vp8_subtract_mbuv_c;

    cpi->rtcd.quantize.quantb                = vp8_regular_quantize_b;
    cpi->rtcd.quantize.quantb_pair           = vp8_regular_quantize_b_pair;
    cpi->rtcd.quantize.fastquantb            = vp8_fast_quantize_b_c;
    cpi->rtcd.quantize.fastquantb_pair       = vp8_fast_quantize_b_pair_c;
    cpi->rtcd.search.full_search             = vp8_full_search_sad;
    cpi->rtcd.search.refining_search         = vp8_refining_search_sad;
    cpi->rtcd.search.diamond_search          = vp8_diamond_search_sad;
#if !(CONFIG_REALTIME_ONLY)
    cpi->rtcd.temporal.apply                 = vp8_temporal_filter_apply_c;
#endif
#endif

    // Pure C:
    vp8_yv12_copy_partial_frame_ptr = vp8_yv12_copy_partial_frame;

#if CONFIG_INTERNAL_STATS
    cpi->rtcd.variance.ssimpf_8x8            = ssim_parms_8x8_c;
    cpi->rtcd.variance.ssimpf                = ssim_parms_c;
#endif

#if ARCH_X86 || ARCH_X86_64
    vp8_arch_x86_encoder_init(cpi);
#endif

#if ARCH_ARM
    vp8_arch_arm_encoder_init(cpi);
#endif

}
