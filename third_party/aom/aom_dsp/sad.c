/*
 * Copyright (c) 2016, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <stdlib.h>

#include "config/aom_config.h"
#include "config/aom_dsp_rtcd.h"

#include "aom/aom_integer.h"
#include "aom_ports/mem.h"
#include "aom_dsp/blend.h"

/* Sum the difference between every corresponding element of the buffers. */
static INLINE unsigned int sad(const uint8_t *a, int a_stride, const uint8_t *b,
                               int b_stride, int width, int height) {
  int y, x;
  unsigned int sad = 0;

  for (y = 0; y < height; y++) {
    for (x = 0; x < width; x++) sad += abs(a[x] - b[x]);

    a += a_stride;
    b += b_stride;
  }
  return sad;
}

#define sadMxh(m)                                                          \
  unsigned int aom_sad##m##xh_c(const uint8_t *a, int a_stride,            \
                                const uint8_t *b, int b_stride, int width, \
                                int height) {                              \
    return sad(a, a_stride, b, b_stride, width, height);                   \
  }

#define sadMxN(m, n)                                                          \
  unsigned int aom_sad##m##x##n##_c(const uint8_t *src, int src_stride,       \
                                    const uint8_t *ref, int ref_stride) {     \
    return sad(src, src_stride, ref, ref_stride, m, n);                       \
  }                                                                           \
  unsigned int aom_sad##m##x##n##_avg_c(const uint8_t *src, int src_stride,   \
                                        const uint8_t *ref, int ref_stride,   \
                                        const uint8_t *second_pred) {         \
    uint8_t comp_pred[m * n];                                                 \
    aom_comp_avg_pred(comp_pred, second_pred, m, n, ref, ref_stride);         \
    return sad(src, src_stride, comp_pred, m, m, n);                          \
  }                                                                           \
  unsigned int aom_jnt_sad##m##x##n##_avg_c(                                  \
      const uint8_t *src, int src_stride, const uint8_t *ref, int ref_stride, \
      const uint8_t *second_pred, const JNT_COMP_PARAMS *jcp_param) {         \
    uint8_t comp_pred[m * n];                                                 \
    aom_jnt_comp_avg_pred_c(comp_pred, second_pred, m, n, ref, ref_stride,    \
                            jcp_param);                                       \
    return sad(src, src_stride, comp_pred, m, m, n);                          \
  }

// Calculate sad against 4 reference locations and store each in sad_array
#define sadMxNx4D(m, n)                                                    \
  void aom_sad##m##x##n##x4d_c(const uint8_t *src, int src_stride,         \
                               const uint8_t *const ref_array[],           \
                               int ref_stride, uint32_t *sad_array) {      \
    int i;                                                                 \
    for (i = 0; i < 4; ++i)                                                \
      sad_array[i] =                                                       \
          aom_sad##m##x##n##_c(src, src_stride, ref_array[i], ref_stride); \
  }

/* clang-format off */
// 128x128
sadMxN(128, 128)
sadMxNx4D(128, 128)

// 128x64
sadMxN(128, 64)
sadMxNx4D(128, 64)

// 64x128
sadMxN(64, 128)
sadMxNx4D(64, 128)

// 64x64
sadMxN(64, 64)
sadMxNx4D(64, 64)

// 64x32
sadMxN(64, 32)
sadMxNx4D(64, 32)

// 32x64
sadMxN(32, 64)
sadMxNx4D(32, 64)

// 32x32
sadMxN(32, 32)
sadMxNx4D(32, 32)

// 32x16
sadMxN(32, 16)
sadMxNx4D(32, 16)

// 16x32
sadMxN(16, 32)
sadMxNx4D(16, 32)

// 16x16
sadMxN(16, 16)
sadMxNx4D(16, 16)

// 16x8
sadMxN(16, 8)
sadMxNx4D(16, 8)

// 8x16
sadMxN(8, 16)
sadMxNx4D(8, 16)

// 8x8
sadMxN(8, 8)
sadMxNx4D(8, 8)

// 8x4
sadMxN(8, 4)
sadMxNx4D(8, 4)

// 4x8
sadMxN(4, 8)
sadMxNx4D(4, 8)

// 4x4
sadMxN(4, 4)
sadMxNx4D(4, 4)

sadMxh(128);
sadMxh(64);
sadMxh(32);
sadMxh(16);
sadMxh(8);
sadMxh(4);

sadMxN(4, 16)
sadMxNx4D(4, 16)
sadMxN(16, 4)
sadMxNx4D(16, 4)
sadMxN(8, 32)
sadMxNx4D(8, 32)
sadMxN(32, 8)
sadMxNx4D(32, 8)
sadMxN(16, 64)
sadMxNx4D(16, 64)
sadMxN(64, 16)
sadMxNx4D(64, 16)

    /* clang-format on */

    static INLINE
    unsigned int highbd_sad(const uint8_t *a8, int a_stride, const uint8_t *b8,
                            int b_stride, int width, int height) {
  int y, x;
  unsigned int sad = 0;
  const uint16_t *a = CONVERT_TO_SHORTPTR(a8);
  const uint16_t *b = CONVERT_TO_SHORTPTR(b8);
  for (y = 0; y < height; y++) {
    for (x = 0; x < width; x++) sad += abs(a[x] - b[x]);

    a += a_stride;
    b += b_stride;
  }
  return sad;
}

static INLINE unsigned int highbd_sadb(const uint8_t *a8, int a_stride,
                                       const uint16_t *b, int b_stride,
                                       int width, int height) {
  int y, x;
  unsigned int sad = 0;
  const uint16_t *a = CONVERT_TO_SHORTPTR(a8);
  for (y = 0; y < height; y++) {
    for (x = 0; x < width; x++) sad += abs(a[x] - b[x]);

    a += a_stride;
    b += b_stride;
  }
  return sad;
}

#define highbd_sadMxN(m, n)                                                    \
  unsigned int aom_highbd_sad##m##x##n##_c(const uint8_t *src, int src_stride, \
                                           const uint8_t *ref,                 \
                                           int ref_stride) {                   \
    return highbd_sad(src, src_stride, ref, ref_stride, m, n);                 \
  }                                                                            \
  unsigned int aom_highbd_sad##m##x##n##_avg_c(                                \
      const uint8_t *src, int src_stride, const uint8_t *ref, int ref_stride,  \
      const uint8_t *second_pred) {                                            \
    uint16_t comp_pred[m * n];                                                 \
    aom_highbd_comp_avg_pred(CONVERT_TO_BYTEPTR(comp_pred), second_pred, m, n, \
                             ref, ref_stride);                                 \
    return highbd_sadb(src, src_stride, comp_pred, m, m, n);                   \
  }                                                                            \
  unsigned int aom_highbd_jnt_sad##m##x##n##_avg_c(                            \
      const uint8_t *src, int src_stride, const uint8_t *ref, int ref_stride,  \
      const uint8_t *second_pred, const JNT_COMP_PARAMS *jcp_param) {          \
    uint16_t comp_pred[m * n];                                                 \
    aom_highbd_jnt_comp_avg_pred(CONVERT_TO_BYTEPTR(comp_pred), second_pred,   \
                                 m, n, ref, ref_stride, jcp_param);            \
    return highbd_sadb(src, src_stride, comp_pred, m, m, n);                   \
  }

#define highbd_sadMxNx4D(m, n)                                               \
  void aom_highbd_sad##m##x##n##x4d_c(const uint8_t *src, int src_stride,    \
                                      const uint8_t *const ref_array[],      \
                                      int ref_stride, uint32_t *sad_array) { \
    int i;                                                                   \
    for (i = 0; i < 4; ++i) {                                                \
      sad_array[i] = aom_highbd_sad##m##x##n##_c(src, src_stride,            \
                                                 ref_array[i], ref_stride);  \
    }                                                                        \
  }

/* clang-format off */
// 128x128
highbd_sadMxN(128, 128)
highbd_sadMxNx4D(128, 128)

// 128x64
highbd_sadMxN(128, 64)
highbd_sadMxNx4D(128, 64)

// 64x128
highbd_sadMxN(64, 128)
highbd_sadMxNx4D(64, 128)

// 64x64
highbd_sadMxN(64, 64)
highbd_sadMxNx4D(64, 64)

// 64x32
highbd_sadMxN(64, 32)
highbd_sadMxNx4D(64, 32)

// 32x64
highbd_sadMxN(32, 64)
highbd_sadMxNx4D(32, 64)

// 32x32
highbd_sadMxN(32, 32)
highbd_sadMxNx4D(32, 32)

// 32x16
highbd_sadMxN(32, 16)
highbd_sadMxNx4D(32, 16)

// 16x32
highbd_sadMxN(16, 32)
highbd_sadMxNx4D(16, 32)

// 16x16
highbd_sadMxN(16, 16)
highbd_sadMxNx4D(16, 16)

// 16x8
highbd_sadMxN(16, 8)
highbd_sadMxNx4D(16, 8)

// 8x16
highbd_sadMxN(8, 16)
highbd_sadMxNx4D(8, 16)

// 8x8
highbd_sadMxN(8, 8)
highbd_sadMxNx4D(8, 8)

// 8x4
highbd_sadMxN(8, 4)
highbd_sadMxNx4D(8, 4)

// 4x8
highbd_sadMxN(4, 8)
highbd_sadMxNx4D(4, 8)

// 4x4
highbd_sadMxN(4, 4)
highbd_sadMxNx4D(4, 4)

highbd_sadMxN(4, 16)
highbd_sadMxNx4D(4, 16)
highbd_sadMxN(16, 4)
highbd_sadMxNx4D(16, 4)
highbd_sadMxN(8, 32)
highbd_sadMxNx4D(8, 32)
highbd_sadMxN(32, 8)
highbd_sadMxNx4D(32, 8)
highbd_sadMxN(16, 64)
highbd_sadMxNx4D(16, 64)
highbd_sadMxN(64, 16)
highbd_sadMxNx4D(64, 16)
    /* clang-format on */
