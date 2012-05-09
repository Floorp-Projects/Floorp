/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#ifndef VARIANCE_X86_H
#define VARIANCE_X86_H


/* Note:
 *
 * This platform is commonly built for runtime CPU detection. If you modify
 * any of the function mappings present in this file, be sure to also update
 * them in the function pointer initialization code
 */
#if HAVE_MMX
extern prototype_sad(vp8_sad4x4_mmx);
extern prototype_sad(vp8_sad8x8_mmx);
extern prototype_sad(vp8_sad8x16_mmx);
extern prototype_sad(vp8_sad16x8_mmx);
extern prototype_sad(vp8_sad16x16_mmx);
extern prototype_variance(vp8_variance4x4_mmx);
extern prototype_variance(vp8_variance8x8_mmx);
extern prototype_variance(vp8_variance8x16_mmx);
extern prototype_variance(vp8_variance16x8_mmx);
extern prototype_variance(vp8_variance16x16_mmx);
extern prototype_subpixvariance(vp8_sub_pixel_variance4x4_mmx);
extern prototype_subpixvariance(vp8_sub_pixel_variance8x8_mmx);
extern prototype_subpixvariance(vp8_sub_pixel_variance8x16_mmx);
extern prototype_subpixvariance(vp8_sub_pixel_variance16x8_mmx);
extern prototype_subpixvariance(vp8_sub_pixel_variance16x16_mmx);
extern prototype_variance(vp8_variance_halfpixvar16x16_h_mmx);
extern prototype_variance(vp8_variance_halfpixvar16x16_v_mmx);
extern prototype_variance(vp8_variance_halfpixvar16x16_hv_mmx);
extern prototype_subpixvariance(vp8_sub_pixel_mse16x16_mmx);
extern prototype_getmbss(vp8_get_mb_ss_mmx);
extern prototype_variance(vp8_mse16x16_mmx);
extern prototype_variance2(vp8_get8x8var_mmx);
extern prototype_get16x16prederror(vp8_get4x4sse_cs_mmx);

#if !CONFIG_RUNTIME_CPU_DETECT
#undef  vp8_variance_sad4x4
#define vp8_variance_sad4x4 vp8_sad4x4_mmx

#undef  vp8_variance_sad8x8
#define vp8_variance_sad8x8 vp8_sad8x8_mmx

#undef  vp8_variance_sad8x16
#define vp8_variance_sad8x16 vp8_sad8x16_mmx

#undef  vp8_variance_sad16x8
#define vp8_variance_sad16x8 vp8_sad16x8_mmx

#undef  vp8_variance_sad16x16
#define vp8_variance_sad16x16 vp8_sad16x16_mmx

#undef  vp8_variance_var4x4
#define vp8_variance_var4x4 vp8_variance4x4_mmx

#undef  vp8_variance_var8x8
#define vp8_variance_var8x8 vp8_variance8x8_mmx

#undef  vp8_variance_var8x16
#define vp8_variance_var8x16 vp8_variance8x16_mmx

#undef  vp8_variance_var16x8
#define vp8_variance_var16x8 vp8_variance16x8_mmx

#undef  vp8_variance_var16x16
#define vp8_variance_var16x16 vp8_variance16x16_mmx

#undef  vp8_variance_subpixvar4x4
#define vp8_variance_subpixvar4x4 vp8_sub_pixel_variance4x4_mmx

#undef  vp8_variance_subpixvar8x8
#define vp8_variance_subpixvar8x8 vp8_sub_pixel_variance8x8_mmx

#undef  vp8_variance_subpixvar8x16
#define vp8_variance_subpixvar8x16 vp8_sub_pixel_variance8x16_mmx

#undef  vp8_variance_subpixvar16x8
#define vp8_variance_subpixvar16x8 vp8_sub_pixel_variance16x8_mmx

#undef  vp8_variance_subpixvar16x16
#define vp8_variance_subpixvar16x16 vp8_sub_pixel_variance16x16_mmx

#undef  vp8_variance_halfpixvar16x16_h
#define vp8_variance_halfpixvar16x16_h vp8_variance_halfpixvar16x16_h_mmx

#undef  vp8_variance_halfpixvar16x16_v
#define vp8_variance_halfpixvar16x16_v vp8_variance_halfpixvar16x16_v_mmx

#undef  vp8_variance_halfpixvar16x16_hv
#define vp8_variance_halfpixvar16x16_hv vp8_variance_halfpixvar16x16_hv_mmx

#undef  vp8_variance_subpixmse16x16
#define vp8_variance_subpixmse16x16 vp8_sub_pixel_mse16x16_mmx

#undef  vp8_variance_getmbss
#define vp8_variance_getmbss vp8_get_mb_ss_mmx

#undef  vp8_variance_mse16x16
#define vp8_variance_mse16x16 vp8_mse16x16_mmx

#undef  vp8_variance_get4x4sse_cs
#define vp8_variance_get4x4sse_cs vp8_get4x4sse_cs_mmx

#endif
#endif


#if HAVE_SSE2
extern prototype_sad(vp8_sad4x4_wmt);
extern prototype_sad(vp8_sad8x8_wmt);
extern prototype_sad(vp8_sad8x16_wmt);
extern prototype_sad(vp8_sad16x8_wmt);
extern prototype_sad(vp8_sad16x16_wmt);
extern prototype_sad(vp8_copy32xn_sse2);
extern prototype_variance(vp8_variance4x4_wmt);
extern prototype_variance(vp8_variance8x8_wmt);
extern prototype_variance(vp8_variance8x16_wmt);
extern prototype_variance(vp8_variance16x8_wmt);
extern prototype_variance(vp8_variance16x16_wmt);
extern prototype_subpixvariance(vp8_sub_pixel_variance4x4_wmt);
extern prototype_subpixvariance(vp8_sub_pixel_variance8x8_wmt);
extern prototype_subpixvariance(vp8_sub_pixel_variance8x16_wmt);
extern prototype_subpixvariance(vp8_sub_pixel_variance16x8_wmt);
extern prototype_subpixvariance(vp8_sub_pixel_variance16x16_wmt);
extern prototype_variance(vp8_variance_halfpixvar16x16_h_wmt);
extern prototype_variance(vp8_variance_halfpixvar16x16_v_wmt);
extern prototype_variance(vp8_variance_halfpixvar16x16_hv_wmt);
extern prototype_subpixvariance(vp8_sub_pixel_mse16x16_wmt);
extern prototype_getmbss(vp8_get_mb_ss_sse2);
extern prototype_variance(vp8_mse16x16_wmt);
extern prototype_variance2(vp8_get8x8var_sse2);
extern prototype_variance2(vp8_get16x16var_sse2);
extern prototype_ssimpf(vp8_ssim_parms_8x8_sse2);
extern prototype_ssimpf(vp8_ssim_parms_16x16_sse2);

#if !CONFIG_RUNTIME_CPU_DETECT
#undef  vp8_variance_sad4x4
#define vp8_variance_sad4x4 vp8_sad4x4_wmt

#undef  vp8_variance_sad8x8
#define vp8_variance_sad8x8 vp8_sad8x8_wmt

#undef  vp8_variance_sad8x16
#define vp8_variance_sad8x16 vp8_sad8x16_wmt

#undef  vp8_variance_sad16x8
#define vp8_variance_sad16x8 vp8_sad16x8_wmt

#undef  vp8_variance_sad16x16
#define vp8_variance_sad16x16 vp8_sad16x16_wmt

#undef  vp8_variance_copy32xn
#define vp8_variance_copy32xn vp8_copy32xn_sse2

#undef  vp8_variance_var4x4
#define vp8_variance_var4x4 vp8_variance4x4_wmt

#undef  vp8_variance_var8x8
#define vp8_variance_var8x8 vp8_variance8x8_wmt

#undef  vp8_variance_var8x16
#define vp8_variance_var8x16 vp8_variance8x16_wmt

#undef  vp8_variance_var16x8
#define vp8_variance_var16x8 vp8_variance16x8_wmt

#undef  vp8_variance_var16x16
#define vp8_variance_var16x16 vp8_variance16x16_wmt

#undef  vp8_variance_subpixvar4x4
#define vp8_variance_subpixvar4x4 vp8_sub_pixel_variance4x4_wmt

#undef  vp8_variance_subpixvar8x8
#define vp8_variance_subpixvar8x8 vp8_sub_pixel_variance8x8_wmt

#undef  vp8_variance_subpixvar8x16
#define vp8_variance_subpixvar8x16 vp8_sub_pixel_variance8x16_wmt

#undef  vp8_variance_subpixvar16x8
#define vp8_variance_subpixvar16x8 vp8_sub_pixel_variance16x8_wmt

#undef  vp8_variance_subpixvar16x16
#define vp8_variance_subpixvar16x16 vp8_sub_pixel_variance16x16_wmt

#undef  vp8_variance_halfpixvar16x16_h
#define vp8_variance_halfpixvar16x16_h vp8_variance_halfpixvar16x16_h_wmt

#undef  vp8_variance_halfpixvar16x16_v
#define vp8_variance_halfpixvar16x16_v vp8_variance_halfpixvar16x16_v_wmt

#undef  vp8_variance_halfpixvar16x16_hv
#define vp8_variance_halfpixvar16x16_hv vp8_variance_halfpixvar16x16_hv_wmt

#undef  vp8_variance_subpixmse16x16
#define vp8_variance_subpixmse16x16 vp8_sub_pixel_mse16x16_wmt

#undef  vp8_variance_getmbss
#define vp8_variance_getmbss vp8_get_mb_ss_sse2

#undef  vp8_variance_mse16x16
#define vp8_variance_mse16x16 vp8_mse16x16_wmt

#if ARCH_X86_64
#undef  vp8_ssimpf_8x8
#define vp8_ssimpf_8x8 vp8_ssim_parms_8x8_sse2

#undef  vp8_ssimpf_16x16
#define vp8_ssimpf_16x16 vp8_ssim_parms_16x16_sse2
#endif

#endif
#endif


#if HAVE_SSE3
extern prototype_sad(vp8_sad16x16_sse3);
extern prototype_sad(vp8_sad16x8_sse3);
extern prototype_sad_multi_same_address(vp8_sad16x16x3_sse3);
extern prototype_sad_multi_same_address(vp8_sad16x8x3_sse3);
extern prototype_sad_multi_same_address(vp8_sad8x16x3_sse3);
extern prototype_sad_multi_same_address(vp8_sad8x8x3_sse3);
extern prototype_sad_multi_same_address(vp8_sad4x4x3_sse3);

extern prototype_sad_multi_dif_address(vp8_sad16x16x4d_sse3);
extern prototype_sad_multi_dif_address(vp8_sad16x8x4d_sse3);
extern prototype_sad_multi_dif_address(vp8_sad8x16x4d_sse3);
extern prototype_sad_multi_dif_address(vp8_sad8x8x4d_sse3);
extern prototype_sad_multi_dif_address(vp8_sad4x4x4d_sse3);
extern prototype_sad(vp8_copy32xn_sse3);

#if !CONFIG_RUNTIME_CPU_DETECT

#undef  vp8_variance_sad16x16
#define vp8_variance_sad16x16 vp8_sad16x16_sse3

#undef  vp8_variance_sad16x16x3
#define vp8_variance_sad16x16x3 vp8_sad16x16x3_sse3

#undef  vp8_variance_sad16x8x3
#define vp8_variance_sad16x8x3 vp8_sad16x8x3_sse3

#undef  vp8_variance_sad8x16x3
#define vp8_variance_sad8x16x3 vp8_sad8x16x3_sse3

#undef  vp8_variance_sad8x8x3
#define vp8_variance_sad8x8x3 vp8_sad8x8x3_sse3

#undef  vp8_variance_sad4x4x3
#define vp8_variance_sad4x4x3 vp8_sad4x4x3_sse3

#undef  vp8_variance_sad16x16x4d
#define vp8_variance_sad16x16x4d vp8_sad16x16x4d_sse3

#undef  vp8_variance_sad16x8x4d
#define vp8_variance_sad16x8x4d vp8_sad16x8x4d_sse3

#undef  vp8_variance_sad8x16x4d
#define vp8_variance_sad8x16x4d vp8_sad8x16x4d_sse3

#undef  vp8_variance_sad8x8x4d
#define vp8_variance_sad8x8x4d vp8_sad8x8x4d_sse3

#undef  vp8_variance_sad4x4x4d
#define vp8_variance_sad4x4x4d vp8_sad4x4x4d_sse3

#undef  vp8_variance_copy32xn
#define vp8_variance_copy32xn vp8_copy32xn_sse3

#endif
#endif


#if HAVE_SSSE3
extern prototype_sad_multi_same_address(vp8_sad16x16x3_ssse3);
extern prototype_sad_multi_same_address(vp8_sad16x8x3_ssse3);
extern prototype_subpixvariance(vp8_sub_pixel_variance16x8_ssse3);
extern prototype_subpixvariance(vp8_sub_pixel_variance16x16_ssse3);

#if !CONFIG_RUNTIME_CPU_DETECT
#undef  vp8_variance_sad16x16x3
#define vp8_variance_sad16x16x3 vp8_sad16x16x3_ssse3

#undef  vp8_variance_sad16x8x3
#define vp8_variance_sad16x8x3 vp8_sad16x8x3_ssse3

#undef  vp8_variance_subpixvar16x8
#define vp8_variance_subpixvar16x8 vp8_sub_pixel_variance16x8_ssse3

#undef  vp8_variance_subpixvar16x16
#define vp8_variance_subpixvar16x16 vp8_sub_pixel_variance16x16_ssse3

#endif
#endif


#if HAVE_SSE4_1
extern prototype_sad_multi_same_address_1(vp8_sad16x16x8_sse4);
extern prototype_sad_multi_same_address_1(vp8_sad16x8x8_sse4);
extern prototype_sad_multi_same_address_1(vp8_sad8x16x8_sse4);
extern prototype_sad_multi_same_address_1(vp8_sad8x8x8_sse4);
extern prototype_sad_multi_same_address_1(vp8_sad4x4x8_sse4);

#if !CONFIG_RUNTIME_CPU_DETECT
#undef  vp8_variance_sad16x16x8
#define vp8_variance_sad16x16x8 vp8_sad16x16x8_sse4

#undef  vp8_variance_sad16x8x8
#define vp8_variance_sad16x8x8 vp8_sad16x8x8_sse4

#undef  vp8_variance_sad8x16x8
#define vp8_variance_sad8x16x8 vp8_sad8x16x8_sse4

#undef  vp8_variance_sad8x8x8
#define vp8_variance_sad8x8x8 vp8_sad8x8x8_sse4

#undef  vp8_variance_sad4x4x8
#define vp8_variance_sad4x4x8 vp8_sad4x4x8_sse4

#endif
#endif

#endif
