/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#ifndef VARIANCE_ARM_H
#define VARIANCE_ARM_H

#if HAVE_ARMV6

extern prototype_sad(vp8_sad16x16_armv6);
extern prototype_variance(vp8_variance16x16_armv6);
extern prototype_variance(vp8_variance8x8_armv6);
extern prototype_subpixvariance(vp8_sub_pixel_variance16x16_armv6);
extern prototype_subpixvariance(vp8_sub_pixel_variance8x8_armv6);
extern prototype_variance(vp8_variance_halfpixvar16x16_h_armv6);
extern prototype_variance(vp8_variance_halfpixvar16x16_v_armv6);
extern prototype_variance(vp8_variance_halfpixvar16x16_hv_armv6);
extern prototype_variance(vp8_mse16x16_armv6);

#if !CONFIG_RUNTIME_CPU_DETECT

#undef  vp8_variance_sad16x16
#define vp8_variance_sad16x16 vp8_sad16x16_armv6

#undef  vp8_variance_subpixvar16x16
#define vp8_variance_subpixvar16x16 vp8_sub_pixel_variance16x16_armv6

#undef  vp8_variance_subpixvar8x8
#define vp8_variance_subpixvar8x8 vp8_sub_pixel_variance8x8_armv6

#undef  vp8_variance_var16x16
#define vp8_variance_var16x16 vp8_variance16x16_armv6

#undef  vp8_variance_mse16x16
#define vp8_variance_mse16x16 vp8_mse16x16_armv6

#undef  vp8_variance_var8x8
#define vp8_variance_var8x8 vp8_variance8x8_armv6

#undef  vp8_variance_halfpixvar16x16_h
#define vp8_variance_halfpixvar16x16_h vp8_variance_halfpixvar16x16_h_armv6

#undef  vp8_variance_halfpixvar16x16_v
#define vp8_variance_halfpixvar16x16_v vp8_variance_halfpixvar16x16_v_armv6

#undef  vp8_variance_halfpixvar16x16_hv
#define vp8_variance_halfpixvar16x16_hv vp8_variance_halfpixvar16x16_hv_armv6

#endif /* !CONFIG_RUNTIME_CPU_DETECT */

#endif /* HAVE_ARMV6 */


#if HAVE_ARMV7
extern prototype_sad(vp8_sad4x4_neon);
extern prototype_sad(vp8_sad8x8_neon);
extern prototype_sad(vp8_sad8x16_neon);
extern prototype_sad(vp8_sad16x8_neon);
extern prototype_sad(vp8_sad16x16_neon);

//extern prototype_variance(vp8_variance4x4_c);
extern prototype_variance(vp8_variance8x8_neon);
extern prototype_variance(vp8_variance8x16_neon);
extern prototype_variance(vp8_variance16x8_neon);
extern prototype_variance(vp8_variance16x16_neon);

//extern prototype_subpixvariance(vp8_sub_pixel_variance4x4_c);
extern prototype_subpixvariance(vp8_sub_pixel_variance8x8_neon);
//extern prototype_subpixvariance(vp8_sub_pixel_variance8x16_c);
//extern prototype_subpixvariance(vp8_sub_pixel_variance16x8_c);
extern prototype_subpixvariance(vp8_sub_pixel_variance16x16_neon);
extern prototype_subpixvariance(vp8_sub_pixel_variance16x16_neon_func);
extern prototype_variance(vp8_variance_halfpixvar16x16_h_neon);
extern prototype_variance(vp8_variance_halfpixvar16x16_v_neon);
extern prototype_variance(vp8_variance_halfpixvar16x16_hv_neon);

//extern prototype_getmbss(vp8_get_mb_ss_c);
extern prototype_variance(vp8_mse16x16_neon);
extern prototype_get16x16prederror(vp8_get4x4sse_cs_neon);

#if !CONFIG_RUNTIME_CPU_DETECT
#undef  vp8_variance_sad4x4
#define vp8_variance_sad4x4 vp8_sad4x4_neon

#undef  vp8_variance_sad8x8
#define vp8_variance_sad8x8 vp8_sad8x8_neon

#undef  vp8_variance_sad8x16
#define vp8_variance_sad8x16 vp8_sad8x16_neon

#undef  vp8_variance_sad16x8
#define vp8_variance_sad16x8 vp8_sad16x8_neon

#undef  vp8_variance_sad16x16
#define vp8_variance_sad16x16 vp8_sad16x16_neon

//#undef  vp8_variance_var4x4
//#define vp8_variance_var4x4 vp8_variance4x4_c

#undef  vp8_variance_var8x8
#define vp8_variance_var8x8 vp8_variance8x8_neon

#undef  vp8_variance_var8x16
#define vp8_variance_var8x16 vp8_variance8x16_neon

#undef  vp8_variance_var16x8
#define vp8_variance_var16x8 vp8_variance16x8_neon

#undef  vp8_variance_var16x16
#define vp8_variance_var16x16 vp8_variance16x16_neon

//#undef  vp8_variance_subpixvar4x4
//#define vp8_variance_subpixvar4x4 vp8_sub_pixel_variance4x4_c

#undef  vp8_variance_subpixvar8x8
#define vp8_variance_subpixvar8x8 vp8_sub_pixel_variance8x8_neon

//#undef  vp8_variance_subpixvar8x16
//#define vp8_variance_subpixvar8x16 vp8_sub_pixel_variance8x16_c

//#undef  vp8_variance_subpixvar16x8
//#define vp8_variance_subpixvar16x8 vp8_sub_pixel_variance16x8_c

#undef  vp8_variance_subpixvar16x16
#define vp8_variance_subpixvar16x16 vp8_sub_pixel_variance16x16_neon

#undef  vp8_variance_halfpixvar16x16_h
#define vp8_variance_halfpixvar16x16_h vp8_variance_halfpixvar16x16_h_neon

#undef  vp8_variance_halfpixvar16x16_v
#define vp8_variance_halfpixvar16x16_v vp8_variance_halfpixvar16x16_v_neon

#undef  vp8_variance_halfpixvar16x16_hv
#define vp8_variance_halfpixvar16x16_hv vp8_variance_halfpixvar16x16_hv_neon

//#undef  vp8_variance_getmbss
//#define vp8_variance_getmbss vp8_get_mb_ss_c

#undef  vp8_variance_mse16x16
#define vp8_variance_mse16x16 vp8_mse16x16_neon

#undef  vp8_variance_get4x4sse_cs
#define vp8_variance_get4x4sse_cs vp8_get4x4sse_cs_neon
#endif

#endif

#endif
