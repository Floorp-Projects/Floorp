/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#ifndef VARIANCE_H
#define VARIANCE_H

#define prototype_sad(sym)\
    unsigned int (sym)\
    (\
     const unsigned char *src_ptr, \
     int source_stride, \
     const unsigned char *ref_ptr, \
     int  ref_stride, \
     int max_sad\
    )

#define prototype_sad_multi_same_address(sym)\
    void (sym)\
    (\
     const unsigned char *src_ptr, \
     int source_stride, \
     const unsigned char *ref_ptr, \
     int  ref_stride, \
     unsigned int *sad_array\
    )

#define prototype_sad_multi_same_address_1(sym)\
    void (sym)\
    (\
     const unsigned char *src_ptr, \
     int source_stride, \
     const unsigned char *ref_ptr, \
     int  ref_stride, \
     unsigned short *sad_array\
    )

#define prototype_sad_multi_dif_address(sym)\
    void (sym)\
    (\
     const unsigned char *src_ptr, \
     int source_stride, \
     unsigned char *ref_ptr[4], \
     int  ref_stride, \
     unsigned int *sad_array\
    )

#define prototype_variance(sym) \
    unsigned int (sym) \
    (\
     const unsigned char *src_ptr, \
     int source_stride, \
     const unsigned char *ref_ptr, \
     int  ref_stride, \
     unsigned int *sse\
    )

#define prototype_variance2(sym) \
    unsigned int (sym) \
    (\
     const unsigned char *src_ptr, \
     int source_stride, \
     const unsigned char *ref_ptr, \
     int  ref_stride, \
     unsigned int *sse,\
     int *sum\
    )

#define prototype_subpixvariance(sym) \
    unsigned int (sym) \
    ( \
      const unsigned char  *src_ptr, \
      int  source_stride, \
      int  xoffset, \
      int  yoffset, \
      const unsigned char *ref_ptr, \
      int Refstride, \
      unsigned int *sse \
    )

#define prototype_ssimpf(sym) \
    void (sym) \
      ( \
        unsigned char *s, \
        int sp, \
        unsigned char *r, \
        int rp, \
        unsigned long *sum_s, \
        unsigned long *sum_r, \
        unsigned long *sum_sq_s, \
        unsigned long *sum_sq_r, \
        unsigned long *sum_sxr \
      )

#define prototype_getmbss(sym) unsigned int (sym)(const short *)

#define prototype_get16x16prederror(sym)\
    unsigned int (sym)\
    (\
     const unsigned char *src_ptr, \
     int source_stride, \
     const unsigned char *ref_ptr, \
     int  ref_stride \
    )

#if ARCH_X86 || ARCH_X86_64
#include "x86/variance_x86.h"
#endif

#if ARCH_ARM
#include "arm/variance_arm.h"
#endif

#ifndef vp8_variance_sad4x4
#define vp8_variance_sad4x4 vp8_sad4x4_c
#endif
extern prototype_sad(vp8_variance_sad4x4);

#ifndef vp8_variance_sad8x8
#define vp8_variance_sad8x8 vp8_sad8x8_c
#endif
extern prototype_sad(vp8_variance_sad8x8);

#ifndef vp8_variance_sad8x16
#define vp8_variance_sad8x16 vp8_sad8x16_c
#endif
extern prototype_sad(vp8_variance_sad8x16);

#ifndef vp8_variance_sad16x8
#define vp8_variance_sad16x8 vp8_sad16x8_c
#endif
extern prototype_sad(vp8_variance_sad16x8);

#ifndef vp8_variance_sad16x16
#define vp8_variance_sad16x16 vp8_sad16x16_c
#endif
extern prototype_sad(vp8_variance_sad16x16);

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-

#ifndef vp8_variance_sad16x16x3
#define vp8_variance_sad16x16x3 vp8_sad16x16x3_c
#endif
extern prototype_sad_multi_same_address(vp8_variance_sad16x16x3);

#ifndef vp8_variance_sad16x8x3
#define vp8_variance_sad16x8x3 vp8_sad16x8x3_c
#endif
extern prototype_sad_multi_same_address(vp8_variance_sad16x8x3);

#ifndef vp8_variance_sad8x8x3
#define vp8_variance_sad8x8x3 vp8_sad8x8x3_c
#endif
extern prototype_sad_multi_same_address(vp8_variance_sad8x8x3);

#ifndef vp8_variance_sad8x16x3
#define vp8_variance_sad8x16x3 vp8_sad8x16x3_c
#endif
extern prototype_sad_multi_same_address(vp8_variance_sad8x16x3);

#ifndef vp8_variance_sad4x4x3
#define vp8_variance_sad4x4x3 vp8_sad4x4x3_c
#endif
extern prototype_sad_multi_same_address(vp8_variance_sad4x4x3);

#ifndef vp8_variance_sad16x16x8
#define vp8_variance_sad16x16x8 vp8_sad16x16x8_c
#endif
extern prototype_sad_multi_same_address_1(vp8_variance_sad16x16x8);

#ifndef vp8_variance_sad16x8x8
#define vp8_variance_sad16x8x8 vp8_sad16x8x8_c
#endif
extern prototype_sad_multi_same_address_1(vp8_variance_sad16x8x8);

#ifndef vp8_variance_sad8x8x8
#define vp8_variance_sad8x8x8 vp8_sad8x8x8_c
#endif
extern prototype_sad_multi_same_address_1(vp8_variance_sad8x8x8);

#ifndef vp8_variance_sad8x16x8
#define vp8_variance_sad8x16x8 vp8_sad8x16x8_c
#endif
extern prototype_sad_multi_same_address_1(vp8_variance_sad8x16x8);

#ifndef vp8_variance_sad4x4x8
#define vp8_variance_sad4x4x8 vp8_sad4x4x8_c
#endif
extern prototype_sad_multi_same_address_1(vp8_variance_sad4x4x8);

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-

#ifndef vp8_variance_sad16x16x4d
#define vp8_variance_sad16x16x4d vp8_sad16x16x4d_c
#endif
extern prototype_sad_multi_dif_address(vp8_variance_sad16x16x4d);

#ifndef vp8_variance_sad16x8x4d
#define vp8_variance_sad16x8x4d vp8_sad16x8x4d_c
#endif
extern prototype_sad_multi_dif_address(vp8_variance_sad16x8x4d);

#ifndef vp8_variance_sad8x8x4d
#define vp8_variance_sad8x8x4d vp8_sad8x8x4d_c
#endif
extern prototype_sad_multi_dif_address(vp8_variance_sad8x8x4d);

#ifndef vp8_variance_sad8x16x4d
#define vp8_variance_sad8x16x4d vp8_sad8x16x4d_c
#endif
extern prototype_sad_multi_dif_address(vp8_variance_sad8x16x4d);

#ifndef vp8_variance_sad4x4x4d
#define vp8_variance_sad4x4x4d vp8_sad4x4x4d_c
#endif
extern prototype_sad_multi_dif_address(vp8_variance_sad4x4x4d);

#if ARCH_X86 || ARCH_X86_64
#ifndef vp8_variance_copy32xn
#define vp8_variance_copy32xn vp8_copy32xn_c
#endif
extern prototype_sad(vp8_variance_copy32xn);
#endif

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-

#ifndef vp8_variance_var4x4
#define vp8_variance_var4x4 vp8_variance4x4_c
#endif
extern prototype_variance(vp8_variance_var4x4);

#ifndef vp8_variance_var8x8
#define vp8_variance_var8x8 vp8_variance8x8_c
#endif
extern prototype_variance(vp8_variance_var8x8);

#ifndef vp8_variance_var8x16
#define vp8_variance_var8x16 vp8_variance8x16_c
#endif
extern prototype_variance(vp8_variance_var8x16);

#ifndef vp8_variance_var16x8
#define vp8_variance_var16x8 vp8_variance16x8_c
#endif
extern prototype_variance(vp8_variance_var16x8);

#ifndef vp8_variance_var16x16
#define vp8_variance_var16x16 vp8_variance16x16_c
#endif
extern prototype_variance(vp8_variance_var16x16);

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-

#ifndef vp8_variance_subpixvar4x4
#define vp8_variance_subpixvar4x4 vp8_sub_pixel_variance4x4_c
#endif
extern prototype_subpixvariance(vp8_variance_subpixvar4x4);

#ifndef vp8_variance_subpixvar8x8
#define vp8_variance_subpixvar8x8 vp8_sub_pixel_variance8x8_c
#endif
extern prototype_subpixvariance(vp8_variance_subpixvar8x8);

#ifndef vp8_variance_subpixvar8x16
#define vp8_variance_subpixvar8x16 vp8_sub_pixel_variance8x16_c
#endif
extern prototype_subpixvariance(vp8_variance_subpixvar8x16);

#ifndef vp8_variance_subpixvar16x8
#define vp8_variance_subpixvar16x8 vp8_sub_pixel_variance16x8_c
#endif
extern prototype_subpixvariance(vp8_variance_subpixvar16x8);

#ifndef vp8_variance_subpixvar16x16
#define vp8_variance_subpixvar16x16 vp8_sub_pixel_variance16x16_c
#endif
extern prototype_subpixvariance(vp8_variance_subpixvar16x16);

#ifndef vp8_variance_halfpixvar16x16_h
#define vp8_variance_halfpixvar16x16_h vp8_variance_halfpixvar16x16_h_c
#endif
extern prototype_variance(vp8_variance_halfpixvar16x16_h);

#ifndef vp8_variance_halfpixvar16x16_v
#define vp8_variance_halfpixvar16x16_v vp8_variance_halfpixvar16x16_v_c
#endif
extern prototype_variance(vp8_variance_halfpixvar16x16_v);

#ifndef vp8_variance_halfpixvar16x16_hv
#define vp8_variance_halfpixvar16x16_hv vp8_variance_halfpixvar16x16_hv_c
#endif
extern prototype_variance(vp8_variance_halfpixvar16x16_hv);

#ifndef vp8_variance_subpixmse16x16
#define vp8_variance_subpixmse16x16 vp8_sub_pixel_mse16x16_c
#endif
extern prototype_subpixvariance(vp8_variance_subpixmse16x16);

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-

#ifndef vp8_variance_getmbss
#define vp8_variance_getmbss vp8_get_mb_ss_c
#endif
extern prototype_getmbss(vp8_variance_getmbss);

#ifndef vp8_variance_mse16x16
#define vp8_variance_mse16x16 vp8_mse16x16_c
#endif
extern prototype_variance(vp8_variance_mse16x16);

#ifndef vp8_variance_get4x4sse_cs
#define vp8_variance_get4x4sse_cs vp8_get4x4sse_cs_c
#endif
extern prototype_get16x16prederror(vp8_variance_get4x4sse_cs);

#ifndef vp8_ssimpf_8x8
#define vp8_ssimpf_8x8 vp8_ssim_parms_8x8_c
#endif
extern prototype_ssimpf(vp8_ssimpf_8x8);

#ifndef vp8_ssimpf_16x16
#define vp8_ssimpf_16x16 vp8_ssim_parms_16x16_c
#endif
extern prototype_ssimpf(vp8_ssimpf_16x16);

typedef prototype_sad(*vp8_sad_fn_t);
typedef prototype_sad_multi_same_address(*vp8_sad_multi_fn_t);
typedef prototype_sad_multi_same_address_1(*vp8_sad_multi1_fn_t);
typedef prototype_sad_multi_dif_address(*vp8_sad_multi_d_fn_t);
typedef prototype_variance(*vp8_variance_fn_t);
typedef prototype_variance2(*vp8_variance2_fn_t);
typedef prototype_subpixvariance(*vp8_subpixvariance_fn_t);
typedef prototype_getmbss(*vp8_getmbss_fn_t);
typedef prototype_ssimpf(*vp8_ssimpf_fn_t);
typedef prototype_get16x16prederror(*vp8_get16x16prederror_fn_t);

typedef struct
{
    vp8_sad_fn_t             sad4x4;
    vp8_sad_fn_t             sad8x8;
    vp8_sad_fn_t             sad8x16;
    vp8_sad_fn_t             sad16x8;
    vp8_sad_fn_t             sad16x16;

    vp8_variance_fn_t        var4x4;
    vp8_variance_fn_t        var8x8;
    vp8_variance_fn_t        var8x16;
    vp8_variance_fn_t        var16x8;
    vp8_variance_fn_t        var16x16;

    vp8_subpixvariance_fn_t  subpixvar4x4;
    vp8_subpixvariance_fn_t  subpixvar8x8;
    vp8_subpixvariance_fn_t  subpixvar8x16;
    vp8_subpixvariance_fn_t  subpixvar16x8;
    vp8_subpixvariance_fn_t  subpixvar16x16;
    vp8_variance_fn_t        halfpixvar16x16_h;
    vp8_variance_fn_t        halfpixvar16x16_v;
    vp8_variance_fn_t        halfpixvar16x16_hv;
    vp8_subpixvariance_fn_t  subpixmse16x16;

    vp8_getmbss_fn_t         getmbss;
    vp8_variance_fn_t        mse16x16;

    vp8_get16x16prederror_fn_t get4x4sse_cs;

    vp8_sad_multi_fn_t       sad16x16x3;
    vp8_sad_multi_fn_t       sad16x8x3;
    vp8_sad_multi_fn_t       sad8x16x3;
    vp8_sad_multi_fn_t       sad8x8x3;
    vp8_sad_multi_fn_t       sad4x4x3;

    vp8_sad_multi1_fn_t      sad16x16x8;
    vp8_sad_multi1_fn_t      sad16x8x8;
    vp8_sad_multi1_fn_t      sad8x16x8;
    vp8_sad_multi1_fn_t      sad8x8x8;
    vp8_sad_multi1_fn_t      sad4x4x8;

    vp8_sad_multi_d_fn_t     sad16x16x4d;
    vp8_sad_multi_d_fn_t     sad16x8x4d;
    vp8_sad_multi_d_fn_t     sad8x16x4d;
    vp8_sad_multi_d_fn_t     sad8x8x4d;
    vp8_sad_multi_d_fn_t     sad4x4x4d;

#if ARCH_X86 || ARCH_X86_64
    vp8_sad_fn_t             copy32xn;
#endif

#if CONFIG_INTERNAL_STATS
    vp8_ssimpf_fn_t          ssimpf_8x8;
    vp8_ssimpf_fn_t          ssimpf_16x16;
#endif

} vp8_variance_rtcd_vtable_t;

typedef struct
{
    vp8_sad_fn_t            sdf;
    vp8_variance_fn_t       vf;
    vp8_subpixvariance_fn_t svf;
    vp8_variance_fn_t       svf_halfpix_h;
    vp8_variance_fn_t       svf_halfpix_v;
    vp8_variance_fn_t       svf_halfpix_hv;
    vp8_sad_multi_fn_t      sdx3f;
    vp8_sad_multi1_fn_t     sdx8f;
    vp8_sad_multi_d_fn_t    sdx4df;
#if ARCH_X86 || ARCH_X86_64
    vp8_sad_fn_t            copymem;
#endif
} vp8_variance_fn_ptr_t;

#if CONFIG_RUNTIME_CPU_DETECT
#define VARIANCE_INVOKE(ctx,fn) (ctx)->fn
#define SSIMPF_INVOKE(ctx,fn) (ctx)->ssimpf_##fn
#else
#define VARIANCE_INVOKE(ctx,fn) vp8_variance_##fn
#define SSIMPF_INVOKE(ctx,fn) vp8_ssimpf_##fn
#endif

#endif
