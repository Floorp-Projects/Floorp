/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#ifndef VP8_COMMON_VARIANCE_H_
#define VP8_COMMON_VARIANCE_H_

#include "vpx_config.h"

#include "vpx/vpx_integer.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned int(*vpx_sad_fn_t)(
    const uint8_t *src_ptr,
    int source_stride,
    const uint8_t *ref_ptr,
    int ref_stride);

typedef void (*vp8_copy32xn_fn_t)(
    const unsigned char *src_ptr,
    int source_stride,
    unsigned char *ref_ptr,
    int ref_stride,
    int n);

typedef void (*vpx_sad_multi_fn_t)(
    const unsigned char *src_ptr,
    int source_stride,
    const unsigned char *ref_array,
    int  ref_stride,
    unsigned int *sad_array);

typedef void (*vpx_sad_multi_d_fn_t)
    (
     const unsigned char *src_ptr,
     int source_stride,
     const unsigned char * const ref_array[],
     int  ref_stride,
     unsigned int *sad_array
    );

typedef unsigned int (*vpx_variance_fn_t)
    (
     const unsigned char *src_ptr,
     int source_stride,
     const unsigned char *ref_ptr,
     int  ref_stride,
     unsigned int *sse
    );

typedef unsigned int (*vp8_subpixvariance_fn_t)
    (
      const unsigned char  *src_ptr,
      int  source_stride,
      int  xoffset,
      int  yoffset,
      const unsigned char *ref_ptr,
      int Refstride,
      unsigned int *sse
    );

typedef struct variance_vtable
{
    vpx_sad_fn_t            sdf;
    vpx_variance_fn_t       vf;
    vp8_subpixvariance_fn_t svf;
    vpx_variance_fn_t       svf_halfpix_h;
    vpx_variance_fn_t       svf_halfpix_v;
    vpx_variance_fn_t       svf_halfpix_hv;
    vpx_sad_multi_fn_t      sdx3f;
    vpx_sad_multi_fn_t      sdx8f;
    vpx_sad_multi_d_fn_t    sdx4df;
#if ARCH_X86 || ARCH_X86_64
    vp8_copy32xn_fn_t       copymem;
#endif
} vp8_variance_fn_ptr_t;

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // VP8_COMMON_VARIANCE_H_
