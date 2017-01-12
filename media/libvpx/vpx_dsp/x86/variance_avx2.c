/*
 *  Copyright (c) 2012 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "./vpx_dsp_rtcd.h"

typedef void (*get_var_avx2)(const uint8_t *src, int src_stride,
                             const uint8_t *ref, int ref_stride,
                             unsigned int *sse, int *sum);

void vpx_get32x32var_avx2(const uint8_t *src, int src_stride,
                          const uint8_t *ref, int ref_stride,
                          unsigned int *sse, int *sum);

static void variance_avx2(const uint8_t *src, int src_stride,
                          const uint8_t *ref, int  ref_stride,
                          int w, int h, unsigned int *sse, int *sum,
                          get_var_avx2 var_fn, int block_size) {
  int i, j;

  *sse = 0;
  *sum = 0;

  for (i = 0; i < h; i += 16) {
    for (j = 0; j < w; j += block_size) {
      unsigned int sse0;
      int sum0;
      var_fn(&src[src_stride * i + j], src_stride,
             &ref[ref_stride * i + j], ref_stride, &sse0, &sum0);
      *sse += sse0;
      *sum += sum0;
    }
  }
}


unsigned int vpx_variance16x16_avx2(const uint8_t *src, int src_stride,
                                    const uint8_t *ref, int ref_stride,
                                    unsigned int *sse) {
  int sum;
  variance_avx2(src, src_stride, ref, ref_stride, 16, 16,
                sse, &sum, vpx_get16x16var_avx2, 16);
  return *sse - (((unsigned int)sum * sum) >> 8);
}

unsigned int vpx_mse16x16_avx2(const uint8_t *src, int src_stride,
                               const uint8_t *ref, int ref_stride,
                               unsigned int *sse) {
  int sum;
  vpx_get16x16var_avx2(src, src_stride, ref, ref_stride, sse, &sum);
  return *sse;
}

unsigned int vpx_variance32x16_avx2(const uint8_t *src, int src_stride,
                                    const uint8_t *ref, int ref_stride,
                                    unsigned int *sse) {
  int sum;
  variance_avx2(src, src_stride, ref, ref_stride, 32, 16,
                sse, &sum, vpx_get32x32var_avx2, 32);
  return *sse - (((int64_t)sum * sum) >> 9);
}

unsigned int vpx_variance32x32_avx2(const uint8_t *src, int src_stride,
                                    const uint8_t *ref, int ref_stride,
                                    unsigned int *sse) {
  int sum;
  variance_avx2(src, src_stride, ref, ref_stride, 32, 32,
                sse, &sum, vpx_get32x32var_avx2, 32);
  return *sse - (((int64_t)sum * sum) >> 10);
}

unsigned int vpx_variance64x64_avx2(const uint8_t *src, int src_stride,
                                    const uint8_t *ref, int ref_stride,
                                    unsigned int *sse) {
  int sum;
  variance_avx2(src, src_stride, ref, ref_stride, 64, 64,
                sse, &sum, vpx_get32x32var_avx2, 32);
  return *sse - (((int64_t)sum * sum) >> 12);
}

unsigned int vpx_variance64x32_avx2(const uint8_t *src, int src_stride,
                                    const uint8_t *ref, int ref_stride,
                                    unsigned int *sse) {
  int sum;
  variance_avx2(src, src_stride, ref, ref_stride, 64, 32,
                sse, &sum, vpx_get32x32var_avx2, 32);
  return *sse - (((int64_t)sum * sum) >> 11);
}
