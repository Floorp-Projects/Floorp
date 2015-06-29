/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "./vpx_config.h"
#include "./vpx_dsp_rtcd.h"

#include "vpx_ports/mem.h"
#include "vpx/vpx_integer.h"

unsigned int vpx_get4x4sse_cs_c(const unsigned char *a, int  a_stride,
                                const unsigned char *b, int  b_stride) {
  int distortion = 0;
  int r, c;

  for (r = 0; r < 4; r++) {
    for (c = 0; c < 4; c++) {
      int diff = a[c] - b[c];
      distortion += diff * diff;
    }

    a += a_stride;
    b += b_stride;
  }

  return distortion;
}

unsigned int vpx_get_mb_ss_c(const int16_t *a) {
  unsigned int i, sum = 0;

  for (i = 0; i < 256; ++i) {
    sum += a[i] * a[i];
  }

  return sum;
}

static void variance(const uint8_t *a, int  a_stride,
                     const uint8_t *b, int  b_stride,
                     int  w, int  h, unsigned int *sse, int *sum) {
  int i, j;

  *sum = 0;
  *sse = 0;

  for (i = 0; i < h; i++) {
    for (j = 0; j < w; j++) {
      const int diff = a[j] - b[j];
      *sum += diff;
      *sse += diff * diff;
    }

    a += a_stride;
    b += b_stride;
  }
}

#define VAR(W, H) \
unsigned int vpx_variance##W##x##H##_c(const uint8_t *a, int a_stride, \
                                       const uint8_t *b, int b_stride, \
                                       unsigned int *sse) { \
  int sum; \
  variance(a, a_stride, b, b_stride, W, H, sse, &sum); \
  return *sse - (((int64_t)sum * sum) / (W * H)); \
}

/* Identical to the variance call except it takes an additional parameter, sum,
 * and returns that value using pass-by-reference instead of returning
 * sse - sum^2 / w*h
 */
#define GET_VAR(W, H) \
void vpx_get##W##x##H##var_c(const uint8_t *a, int a_stride, \
                             const uint8_t *b, int b_stride, \
                             unsigned int *sse, int *sum) { \
  variance(a, a_stride, b, b_stride, W, H, sse, sum); \
}

/* Identical to the variance call except it does not calculate the
 * sse - sum^2 / w*h and returns sse in addtion to modifying the passed in
 * variable.
 */
#define MSE(W, H) \
unsigned int vpx_mse##W##x##H##_c(const uint8_t *a, int a_stride, \
                                  const uint8_t *b, int b_stride, \
                                  unsigned int *sse) { \
  int sum; \
  variance(a, a_stride, b, b_stride, W, H, sse, &sum); \
  return *sse; \
}

VAR(64, 64)
VAR(64, 32)
VAR(32, 64)
VAR(32, 32)
VAR(32, 16)
VAR(16, 32)
VAR(16, 16)
VAR(16, 8)
VAR(8, 16)
VAR(8, 8)
VAR(8, 4)
VAR(4, 8)
VAR(4, 4)

GET_VAR(16, 16)
GET_VAR(8, 8)

MSE(16, 16)
MSE(16, 8)
MSE(8, 16)
MSE(8, 8)

void vpx_comp_avg_pred_c(uint8_t *comp_pred, const uint8_t *pred, int width,
                         int height, const uint8_t *ref, int ref_stride) {
  int i, j;

  for (i = 0; i < height; i++) {
    for (j = 0; j < width; j++) {
      const int tmp = pred[j] + ref[j];
      comp_pred[j] = ROUND_POWER_OF_TWO(tmp, 1);
    }
    comp_pred += width;
    pred += width;
    ref += ref_stride;
  }
}

#if CONFIG_VP9_HIGHBITDEPTH
static void highbd_variance64(const uint8_t *a8, int  a_stride,
                              const uint8_t *b8, int  b_stride,
                              int w, int h, uint64_t *sse, uint64_t *sum) {
  int i, j;

  uint16_t *a = CONVERT_TO_SHORTPTR(a8);
  uint16_t *b = CONVERT_TO_SHORTPTR(b8);
  *sum = 0;
  *sse = 0;

  for (i = 0; i < h; i++) {
    for (j = 0; j < w; j++) {
      const int diff = a[j] - b[j];
      *sum += diff;
      *sse += diff * diff;
    }
    a += a_stride;
    b += b_stride;
  }
}

static void highbd_8_variance(const uint8_t *a8, int  a_stride,
                              const uint8_t *b8, int  b_stride,
                              int w, int h, unsigned int *sse, int *sum) {
  uint64_t sse_long = 0;
  uint64_t sum_long = 0;
  highbd_variance64(a8, a_stride, b8, b_stride, w, h, &sse_long, &sum_long);
  *sse = (unsigned int)sse_long;
  *sum = (int)sum_long;
}

static void highbd_10_variance(const uint8_t *a8, int  a_stride,
                               const uint8_t *b8, int  b_stride,
                               int w, int h, unsigned int *sse, int *sum) {
  uint64_t sse_long = 0;
  uint64_t sum_long = 0;
  highbd_variance64(a8, a_stride, b8, b_stride, w, h, &sse_long, &sum_long);
  *sse = (unsigned int)ROUND_POWER_OF_TWO(sse_long, 4);
  *sum = (int)ROUND_POWER_OF_TWO(sum_long, 2);
}

static void highbd_12_variance(const uint8_t *a8, int  a_stride,
                               const uint8_t *b8, int  b_stride,
                               int w, int h, unsigned int *sse, int *sum) {
  uint64_t sse_long = 0;
  uint64_t sum_long = 0;
  highbd_variance64(a8, a_stride, b8, b_stride, w, h, &sse_long, &sum_long);
  *sse = (unsigned int)ROUND_POWER_OF_TWO(sse_long, 8);
  *sum = (int)ROUND_POWER_OF_TWO(sum_long, 4);
}

#define HIGHBD_VAR(W, H) \
unsigned int vpx_highbd_8_variance##W##x##H##_c(const uint8_t *a, \
                                                int a_stride, \
                                                const uint8_t *b, \
                                                int b_stride, \
                                                unsigned int *sse) { \
  int sum; \
  highbd_8_variance(a, a_stride, b, b_stride, W, H, sse, &sum); \
  return *sse - (((int64_t)sum * sum) / (W * H)); \
} \
\
unsigned int vpx_highbd_10_variance##W##x##H##_c(const uint8_t *a, \
                                                 int a_stride, \
                                                 const uint8_t *b, \
                                                 int b_stride, \
                                                 unsigned int *sse) { \
  int sum; \
  highbd_10_variance(a, a_stride, b, b_stride, W, H, sse, &sum); \
  return *sse - (((int64_t)sum * sum) / (W * H)); \
} \
\
unsigned int vpx_highbd_12_variance##W##x##H##_c(const uint8_t *a, \
                                                 int a_stride, \
                                                 const uint8_t *b, \
                                                 int b_stride, \
                                                 unsigned int *sse) { \
  int sum; \
  highbd_12_variance(a, a_stride, b, b_stride, W, H, sse, &sum); \
  return *sse - (((int64_t)sum * sum) / (W * H)); \
}

#define HIGHBD_GET_VAR(S) \
void vpx_highbd_8_get##S##x##S##var_c(const uint8_t *src, int src_stride, \
                                    const uint8_t *ref, int ref_stride, \
                                    unsigned int *sse, int *sum) { \
  highbd_8_variance(src, src_stride, ref, ref_stride, S, S, sse, sum); \
} \
\
void vpx_highbd_10_get##S##x##S##var_c(const uint8_t *src, int src_stride, \
                                       const uint8_t *ref, int ref_stride, \
                                       unsigned int *sse, int *sum) { \
  highbd_10_variance(src, src_stride, ref, ref_stride, S, S, sse, sum); \
} \
\
void vpx_highbd_12_get##S##x##S##var_c(const uint8_t *src, int src_stride, \
                                       const uint8_t *ref, int ref_stride, \
                                       unsigned int *sse, int *sum) { \
  highbd_12_variance(src, src_stride, ref, ref_stride, S, S, sse, sum); \
}

#define HIGHBD_MSE(W, H) \
unsigned int vpx_highbd_8_mse##W##x##H##_c(const uint8_t *src, \
                                         int src_stride, \
                                         const uint8_t *ref, \
                                         int ref_stride, \
                                         unsigned int *sse) { \
  int sum; \
  highbd_8_variance(src, src_stride, ref, ref_stride, W, H, sse, &sum); \
  return *sse; \
} \
\
unsigned int vpx_highbd_10_mse##W##x##H##_c(const uint8_t *src, \
                                            int src_stride, \
                                            const uint8_t *ref, \
                                            int ref_stride, \
                                            unsigned int *sse) { \
  int sum; \
  highbd_10_variance(src, src_stride, ref, ref_stride, W, H, sse, &sum); \
  return *sse; \
} \
\
unsigned int vpx_highbd_12_mse##W##x##H##_c(const uint8_t *src, \
                                            int src_stride, \
                                            const uint8_t *ref, \
                                            int ref_stride, \
                                            unsigned int *sse) { \
  int sum; \
  highbd_12_variance(src, src_stride, ref, ref_stride, W, H, sse, &sum); \
  return *sse; \
}

HIGHBD_GET_VAR(8)
HIGHBD_GET_VAR(16)

HIGHBD_MSE(16, 16)
HIGHBD_MSE(16, 8)
HIGHBD_MSE(8, 16)
HIGHBD_MSE(8, 8)

HIGHBD_VAR(64, 64)
HIGHBD_VAR(64, 32)
HIGHBD_VAR(32, 64)
HIGHBD_VAR(32, 32)
HIGHBD_VAR(32, 16)
HIGHBD_VAR(16, 32)
HIGHBD_VAR(16, 16)
HIGHBD_VAR(16, 8)
HIGHBD_VAR(8, 16)
HIGHBD_VAR(8, 8)
HIGHBD_VAR(8, 4)
HIGHBD_VAR(4, 8)
HIGHBD_VAR(4, 4)

void vpx_highbd_comp_avg_pred(uint16_t *comp_pred, const uint8_t *pred8,
                              int width, int height, const uint8_t *ref8,
                              int ref_stride) {
  int i, j;
  uint16_t *pred = CONVERT_TO_SHORTPTR(pred8);
  uint16_t *ref = CONVERT_TO_SHORTPTR(ref8);
  for (i = 0; i < height; i++) {
    for (j = 0; j < width; j++) {
      const int tmp = pred[j] + ref[j];
      comp_pred[j] = ROUND_POWER_OF_TWO(tmp, 1);
    }
    comp_pred += width;
    pred += width;
    ref += ref_stride;
  }
}
#endif  // CONFIG_VP9_HIGHBITDEPTH
