/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VP9_ENCODER_VP9_VARIANCE_H_
#define VP9_ENCODER_VP9_VARIANCE_H_

#include "vpx/vpx_integer.h"

#ifdef __cplusplus
extern "C" {
#endif

void variance(const uint8_t *src_ptr,
              int  source_stride,
              const uint8_t *ref_ptr,
              int  recon_stride,
              int  w,
              int  h,
              unsigned int *sse,
              int *sum);

typedef unsigned int(*vp9_sad_fn_t)(const uint8_t *src_ptr,
                                    int source_stride,
                                    const uint8_t *ref_ptr,
                                    int ref_stride,
                                    unsigned int max_sad);

typedef unsigned int(*vp9_sad_avg_fn_t)(const uint8_t *src_ptr,
                                        int source_stride,
                                        const uint8_t *ref_ptr,
                                        int ref_stride,
                                        const uint8_t *second_pred,
                                        unsigned int max_sad);

typedef void (*vp9_sad_multi_fn_t)(const uint8_t *src_ptr,
                                   int source_stride,
                                   const uint8_t *ref_ptr,
                                   int  ref_stride,
                                   unsigned int *sad_array);

typedef void (*vp9_sad_multi1_fn_t)(const uint8_t *src_ptr,
                                    int source_stride,
                                    const uint8_t *ref_ptr,
                                    int  ref_stride,
                                    unsigned int *sad_array);

typedef void (*vp9_sad_multi_d_fn_t)(const uint8_t *src_ptr,
                                     int source_stride,
                                     const uint8_t* const ref_ptr[],
                                     int  ref_stride, unsigned int *sad_array);

typedef unsigned int (*vp9_variance_fn_t)(const uint8_t *src_ptr,
                                          int source_stride,
                                          const uint8_t *ref_ptr,
                                          int ref_stride,
                                          unsigned int *sse);

typedef unsigned int (*vp9_subpixvariance_fn_t)(const uint8_t *src_ptr,
                                                int source_stride,
                                                int xoffset,
                                                int yoffset,
                                                const uint8_t *ref_ptr,
                                                int Refstride,
                                                unsigned int *sse);

typedef unsigned int (*vp9_subp_avg_variance_fn_t)(const uint8_t *src_ptr,
                                                   int source_stride,
                                                   int xoffset,
                                                   int yoffset,
                                                   const uint8_t *ref_ptr,
                                                   int Refstride,
                                                   unsigned int *sse,
                                                   const uint8_t *second_pred);

typedef unsigned int (*vp9_getmbss_fn_t)(const short *);

typedef unsigned int (*vp9_get16x16prederror_fn_t)(const uint8_t *src_ptr,
                                                   int source_stride,
                                                   const uint8_t *ref_ptr,
                                                   int  ref_stride);

typedef struct vp9_variance_vtable {
  vp9_sad_fn_t               sdf;
  vp9_sad_avg_fn_t           sdaf;
  vp9_variance_fn_t          vf;
  vp9_subpixvariance_fn_t    svf;
  vp9_subp_avg_variance_fn_t svaf;
  vp9_variance_fn_t          svf_halfpix_h;
  vp9_variance_fn_t          svf_halfpix_v;
  vp9_variance_fn_t          svf_halfpix_hv;
  vp9_sad_multi_fn_t         sdx3f;
  vp9_sad_multi1_fn_t        sdx8f;
  vp9_sad_multi_d_fn_t       sdx4df;
} vp9_variance_fn_ptr_t;

static void comp_avg_pred(uint8_t *comp_pred, const uint8_t *pred, int width,
                          int height, const uint8_t *ref, int ref_stride) {
  int i, j;

  for (i = 0; i < height; i++) {
    for (j = 0; j < width; j++) {
      int tmp;
      tmp = pred[j] + ref[j];
      comp_pred[j] = (tmp + 1) >> 1;
    }
    comp_pred += width;
    pred += width;
    ref += ref_stride;
  }
}
#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // VP9_ENCODER_VP9_VARIANCE_H_
