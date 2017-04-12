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

#include <emmintrin.h>  // SSE2

#include "./aom_config.h"
#include "./aom_dsp_rtcd.h"

#include "aom_ports/mem.h"

typedef uint32_t (*high_variance_fn_t)(const uint16_t *src, int src_stride,
                                       const uint16_t *ref, int ref_stride,
                                       uint32_t *sse, int *sum);

uint32_t aom_highbd_calc8x8var_sse2(const uint16_t *src, int src_stride,
                                    const uint16_t *ref, int ref_stride,
                                    uint32_t *sse, int *sum);

uint32_t aom_highbd_calc16x16var_sse2(const uint16_t *src, int src_stride,
                                      const uint16_t *ref, int ref_stride,
                                      uint32_t *sse, int *sum);

static void highbd_8_variance_sse2(const uint16_t *src, int src_stride,
                                   const uint16_t *ref, int ref_stride, int w,
                                   int h, uint32_t *sse, int *sum,
                                   high_variance_fn_t var_fn, int block_size) {
  int i, j;

  *sse = 0;
  *sum = 0;

  for (i = 0; i < h; i += block_size) {
    for (j = 0; j < w; j += block_size) {
      unsigned int sse0;
      int sum0;
      var_fn(src + src_stride * i + j, src_stride, ref + ref_stride * i + j,
             ref_stride, &sse0, &sum0);
      *sse += sse0;
      *sum += sum0;
    }
  }
}

static void highbd_10_variance_sse2(const uint16_t *src, int src_stride,
                                    const uint16_t *ref, int ref_stride, int w,
                                    int h, uint32_t *sse, int *sum,
                                    high_variance_fn_t var_fn, int block_size) {
  int i, j;
  uint64_t sse_long = 0;
  int32_t sum_long = 0;

  for (i = 0; i < h; i += block_size) {
    for (j = 0; j < w; j += block_size) {
      unsigned int sse0;
      int sum0;
      var_fn(src + src_stride * i + j, src_stride, ref + ref_stride * i + j,
             ref_stride, &sse0, &sum0);
      sse_long += sse0;
      sum_long += sum0;
    }
  }
  *sum = ROUND_POWER_OF_TWO(sum_long, 2);
  *sse = (uint32_t)ROUND_POWER_OF_TWO(sse_long, 4);
}

static void highbd_12_variance_sse2(const uint16_t *src, int src_stride,
                                    const uint16_t *ref, int ref_stride, int w,
                                    int h, uint32_t *sse, int *sum,
                                    high_variance_fn_t var_fn, int block_size) {
  int i, j;
  uint64_t sse_long = 0;
  int32_t sum_long = 0;

  for (i = 0; i < h; i += block_size) {
    for (j = 0; j < w; j += block_size) {
      unsigned int sse0;
      int sum0;
      var_fn(src + src_stride * i + j, src_stride, ref + ref_stride * i + j,
             ref_stride, &sse0, &sum0);
      sse_long += sse0;
      sum_long += sum0;
    }
  }
  *sum = ROUND_POWER_OF_TWO(sum_long, 4);
  *sse = (uint32_t)ROUND_POWER_OF_TWO(sse_long, 8);
}

#define HIGH_GET_VAR(S)                                                       \
  void aom_highbd_get##S##x##S##var_sse2(const uint8_t *src8, int src_stride, \
                                         const uint8_t *ref8, int ref_stride, \
                                         uint32_t *sse, int *sum) {           \
    uint16_t *src = CONVERT_TO_SHORTPTR(src8);                                \
    uint16_t *ref = CONVERT_TO_SHORTPTR(ref8);                                \
    aom_highbd_calc##S##x##S##var_sse2(src, src_stride, ref, ref_stride, sse, \
                                       sum);                                  \
  }                                                                           \
                                                                              \
  void aom_highbd_10_get##S##x##S##var_sse2(                                  \
      const uint8_t *src8, int src_stride, const uint8_t *ref8,               \
      int ref_stride, uint32_t *sse, int *sum) {                              \
    uint16_t *src = CONVERT_TO_SHORTPTR(src8);                                \
    uint16_t *ref = CONVERT_TO_SHORTPTR(ref8);                                \
    aom_highbd_calc##S##x##S##var_sse2(src, src_stride, ref, ref_stride, sse, \
                                       sum);                                  \
    *sum = ROUND_POWER_OF_TWO(*sum, 2);                                       \
    *sse = ROUND_POWER_OF_TWO(*sse, 4);                                       \
  }                                                                           \
                                                                              \
  void aom_highbd_12_get##S##x##S##var_sse2(                                  \
      const uint8_t *src8, int src_stride, const uint8_t *ref8,               \
      int ref_stride, uint32_t *sse, int *sum) {                              \
    uint16_t *src = CONVERT_TO_SHORTPTR(src8);                                \
    uint16_t *ref = CONVERT_TO_SHORTPTR(ref8);                                \
    aom_highbd_calc##S##x##S##var_sse2(src, src_stride, ref, ref_stride, sse, \
                                       sum);                                  \
    *sum = ROUND_POWER_OF_TWO(*sum, 4);                                       \
    *sse = ROUND_POWER_OF_TWO(*sse, 8);                                       \
  }

HIGH_GET_VAR(16);
HIGH_GET_VAR(8);

#undef HIGH_GET_VAR

#define VAR_FN(w, h, block_size, shift)                                    \
  uint32_t aom_highbd_8_variance##w##x##h##_sse2(                          \
      const uint8_t *src8, int src_stride, const uint8_t *ref8,            \
      int ref_stride, uint32_t *sse) {                                     \
    int sum;                                                               \
    uint16_t *src = CONVERT_TO_SHORTPTR(src8);                             \
    uint16_t *ref = CONVERT_TO_SHORTPTR(ref8);                             \
    highbd_8_variance_sse2(                                                \
        src, src_stride, ref, ref_stride, w, h, sse, &sum,                 \
        aom_highbd_calc##block_size##x##block_size##var_sse2, block_size); \
    return *sse - (uint32_t)(((int64_t)sum * sum) >> shift);               \
  }                                                                        \
                                                                           \
  uint32_t aom_highbd_10_variance##w##x##h##_sse2(                         \
      const uint8_t *src8, int src_stride, const uint8_t *ref8,            \
      int ref_stride, uint32_t *sse) {                                     \
    int sum;                                                               \
    int64_t var;                                                           \
    uint16_t *src = CONVERT_TO_SHORTPTR(src8);                             \
    uint16_t *ref = CONVERT_TO_SHORTPTR(ref8);                             \
    highbd_10_variance_sse2(                                               \
        src, src_stride, ref, ref_stride, w, h, sse, &sum,                 \
        aom_highbd_calc##block_size##x##block_size##var_sse2, block_size); \
    var = (int64_t)(*sse) - (((int64_t)sum * sum) >> shift);               \
    return (var >= 0) ? (uint32_t)var : 0;                                 \
  }                                                                        \
                                                                           \
  uint32_t aom_highbd_12_variance##w##x##h##_sse2(                         \
      const uint8_t *src8, int src_stride, const uint8_t *ref8,            \
      int ref_stride, uint32_t *sse) {                                     \
    int sum;                                                               \
    int64_t var;                                                           \
    uint16_t *src = CONVERT_TO_SHORTPTR(src8);                             \
    uint16_t *ref = CONVERT_TO_SHORTPTR(ref8);                             \
    highbd_12_variance_sse2(                                               \
        src, src_stride, ref, ref_stride, w, h, sse, &sum,                 \
        aom_highbd_calc##block_size##x##block_size##var_sse2, block_size); \
    var = (int64_t)(*sse) - (((int64_t)sum * sum) >> shift);               \
    return (var >= 0) ? (uint32_t)var : 0;                                 \
  }

VAR_FN(64, 64, 16, 12);
VAR_FN(64, 32, 16, 11);
VAR_FN(32, 64, 16, 11);
VAR_FN(32, 32, 16, 10);
VAR_FN(32, 16, 16, 9);
VAR_FN(16, 32, 16, 9);
VAR_FN(16, 16, 16, 8);
VAR_FN(16, 8, 8, 7);
VAR_FN(8, 16, 8, 7);
VAR_FN(8, 8, 8, 6);

#undef VAR_FN

unsigned int aom_highbd_8_mse16x16_sse2(const uint8_t *src8, int src_stride,
                                        const uint8_t *ref8, int ref_stride,
                                        unsigned int *sse) {
  int sum;
  uint16_t *src = CONVERT_TO_SHORTPTR(src8);
  uint16_t *ref = CONVERT_TO_SHORTPTR(ref8);
  highbd_8_variance_sse2(src, src_stride, ref, ref_stride, 16, 16, sse, &sum,
                         aom_highbd_calc16x16var_sse2, 16);
  return *sse;
}

unsigned int aom_highbd_10_mse16x16_sse2(const uint8_t *src8, int src_stride,
                                         const uint8_t *ref8, int ref_stride,
                                         unsigned int *sse) {
  int sum;
  uint16_t *src = CONVERT_TO_SHORTPTR(src8);
  uint16_t *ref = CONVERT_TO_SHORTPTR(ref8);
  highbd_10_variance_sse2(src, src_stride, ref, ref_stride, 16, 16, sse, &sum,
                          aom_highbd_calc16x16var_sse2, 16);
  return *sse;
}

unsigned int aom_highbd_12_mse16x16_sse2(const uint8_t *src8, int src_stride,
                                         const uint8_t *ref8, int ref_stride,
                                         unsigned int *sse) {
  int sum;
  uint16_t *src = CONVERT_TO_SHORTPTR(src8);
  uint16_t *ref = CONVERT_TO_SHORTPTR(ref8);
  highbd_12_variance_sse2(src, src_stride, ref, ref_stride, 16, 16, sse, &sum,
                          aom_highbd_calc16x16var_sse2, 16);
  return *sse;
}

unsigned int aom_highbd_8_mse8x8_sse2(const uint8_t *src8, int src_stride,
                                      const uint8_t *ref8, int ref_stride,
                                      unsigned int *sse) {
  int sum;
  uint16_t *src = CONVERT_TO_SHORTPTR(src8);
  uint16_t *ref = CONVERT_TO_SHORTPTR(ref8);
  highbd_8_variance_sse2(src, src_stride, ref, ref_stride, 8, 8, sse, &sum,
                         aom_highbd_calc8x8var_sse2, 8);
  return *sse;
}

unsigned int aom_highbd_10_mse8x8_sse2(const uint8_t *src8, int src_stride,
                                       const uint8_t *ref8, int ref_stride,
                                       unsigned int *sse) {
  int sum;
  uint16_t *src = CONVERT_TO_SHORTPTR(src8);
  uint16_t *ref = CONVERT_TO_SHORTPTR(ref8);
  highbd_10_variance_sse2(src, src_stride, ref, ref_stride, 8, 8, sse, &sum,
                          aom_highbd_calc8x8var_sse2, 8);
  return *sse;
}

unsigned int aom_highbd_12_mse8x8_sse2(const uint8_t *src8, int src_stride,
                                       const uint8_t *ref8, int ref_stride,
                                       unsigned int *sse) {
  int sum;
  uint16_t *src = CONVERT_TO_SHORTPTR(src8);
  uint16_t *ref = CONVERT_TO_SHORTPTR(ref8);
  highbd_12_variance_sse2(src, src_stride, ref, ref_stride, 8, 8, sse, &sum,
                          aom_highbd_calc8x8var_sse2, 8);
  return *sse;
}

// The 2 unused parameters are place holders for PIC enabled build.
// These definitions are for functions defined in
// highbd_subpel_variance_impl_sse2.asm
#define DECL(w, opt)                                                         \
  int aom_highbd_sub_pixel_variance##w##xh_##opt(                            \
      const uint16_t *src, ptrdiff_t src_stride, int x_offset, int y_offset, \
      const uint16_t *dst, ptrdiff_t dst_stride, int height,                 \
      unsigned int *sse, void *unused0, void *unused);
#define DECLS(opt) \
  DECL(8, opt);    \
  DECL(16, opt)

DECLS(sse2);

#undef DECLS
#undef DECL

#define FN(w, h, wf, wlog2, hlog2, opt, cast)                                  \
  uint32_t aom_highbd_8_sub_pixel_variance##w##x##h##_##opt(                   \
      const uint8_t *src8, int src_stride, int x_offset, int y_offset,         \
      const uint8_t *dst8, int dst_stride, uint32_t *sse_ptr) {                \
    uint32_t sse;                                                              \
    uint16_t *src = CONVERT_TO_SHORTPTR(src8);                                 \
    uint16_t *dst = CONVERT_TO_SHORTPTR(dst8);                                 \
    int se = aom_highbd_sub_pixel_variance##wf##xh_##opt(                      \
        src, src_stride, x_offset, y_offset, dst, dst_stride, h, &sse, NULL,   \
        NULL);                                                                 \
    if (w > wf) {                                                              \
      unsigned int sse2;                                                       \
      int se2 = aom_highbd_sub_pixel_variance##wf##xh_##opt(                   \
          src + 16, src_stride, x_offset, y_offset, dst + 16, dst_stride, h,   \
          &sse2, NULL, NULL);                                                  \
      se += se2;                                                               \
      sse += sse2;                                                             \
      if (w > wf * 2) {                                                        \
        se2 = aom_highbd_sub_pixel_variance##wf##xh_##opt(                     \
            src + 32, src_stride, x_offset, y_offset, dst + 32, dst_stride, h, \
            &sse2, NULL, NULL);                                                \
        se += se2;                                                             \
        sse += sse2;                                                           \
        se2 = aom_highbd_sub_pixel_variance##wf##xh_##opt(                     \
            src + 48, src_stride, x_offset, y_offset, dst + 48, dst_stride, h, \
            &sse2, NULL, NULL);                                                \
        se += se2;                                                             \
        sse += sse2;                                                           \
      }                                                                        \
    }                                                                          \
    *sse_ptr = sse;                                                            \
    return sse - (uint32_t)((cast se * se) >> (wlog2 + hlog2));                \
  }                                                                            \
                                                                               \
  uint32_t aom_highbd_10_sub_pixel_variance##w##x##h##_##opt(                  \
      const uint8_t *src8, int src_stride, int x_offset, int y_offset,         \
      const uint8_t *dst8, int dst_stride, uint32_t *sse_ptr) {                \
    int64_t var;                                                               \
    uint32_t sse;                                                              \
    uint16_t *src = CONVERT_TO_SHORTPTR(src8);                                 \
    uint16_t *dst = CONVERT_TO_SHORTPTR(dst8);                                 \
    int se = aom_highbd_sub_pixel_variance##wf##xh_##opt(                      \
        src, src_stride, x_offset, y_offset, dst, dst_stride, h, &sse, NULL,   \
        NULL);                                                                 \
    if (w > wf) {                                                              \
      uint32_t sse2;                                                           \
      int se2 = aom_highbd_sub_pixel_variance##wf##xh_##opt(                   \
          src + 16, src_stride, x_offset, y_offset, dst + 16, dst_stride, h,   \
          &sse2, NULL, NULL);                                                  \
      se += se2;                                                               \
      sse += sse2;                                                             \
      if (w > wf * 2) {                                                        \
        se2 = aom_highbd_sub_pixel_variance##wf##xh_##opt(                     \
            src + 32, src_stride, x_offset, y_offset, dst + 32, dst_stride, h, \
            &sse2, NULL, NULL);                                                \
        se += se2;                                                             \
        sse += sse2;                                                           \
        se2 = aom_highbd_sub_pixel_variance##wf##xh_##opt(                     \
            src + 48, src_stride, x_offset, y_offset, dst + 48, dst_stride, h, \
            &sse2, NULL, NULL);                                                \
        se += se2;                                                             \
        sse += sse2;                                                           \
      }                                                                        \
    }                                                                          \
    se = ROUND_POWER_OF_TWO(se, 2);                                            \
    sse = ROUND_POWER_OF_TWO(sse, 4);                                          \
    *sse_ptr = sse;                                                            \
    var = (int64_t)(sse) - ((cast se * se) >> (wlog2 + hlog2));                \
    return (var >= 0) ? (uint32_t)var : 0;                                     \
  }                                                                            \
                                                                               \
  uint32_t aom_highbd_12_sub_pixel_variance##w##x##h##_##opt(                  \
      const uint8_t *src8, int src_stride, int x_offset, int y_offset,         \
      const uint8_t *dst8, int dst_stride, uint32_t *sse_ptr) {                \
    int start_row;                                                             \
    uint32_t sse;                                                              \
    int se = 0;                                                                \
    int64_t var;                                                               \
    uint64_t long_sse = 0;                                                     \
    uint16_t *src = CONVERT_TO_SHORTPTR(src8);                                 \
    uint16_t *dst = CONVERT_TO_SHORTPTR(dst8);                                 \
    for (start_row = 0; start_row < h; start_row += 16) {                      \
      uint32_t sse2;                                                           \
      int height = h - start_row < 16 ? h - start_row : 16;                    \
      int se2 = aom_highbd_sub_pixel_variance##wf##xh_##opt(                   \
          src + (start_row * src_stride), src_stride, x_offset, y_offset,      \
          dst + (start_row * dst_stride), dst_stride, height, &sse2, NULL,     \
          NULL);                                                               \
      se += se2;                                                               \
      long_sse += sse2;                                                        \
      if (w > wf) {                                                            \
        se2 = aom_highbd_sub_pixel_variance##wf##xh_##opt(                     \
            src + 16 + (start_row * src_stride), src_stride, x_offset,         \
            y_offset, dst + 16 + (start_row * dst_stride), dst_stride, height, \
            &sse2, NULL, NULL);                                                \
        se += se2;                                                             \
        long_sse += sse2;                                                      \
        if (w > wf * 2) {                                                      \
          se2 = aom_highbd_sub_pixel_variance##wf##xh_##opt(                   \
              src + 32 + (start_row * src_stride), src_stride, x_offset,       \
              y_offset, dst + 32 + (start_row * dst_stride), dst_stride,       \
              height, &sse2, NULL, NULL);                                      \
          se += se2;                                                           \
          long_sse += sse2;                                                    \
          se2 = aom_highbd_sub_pixel_variance##wf##xh_##opt(                   \
              src + 48 + (start_row * src_stride), src_stride, x_offset,       \
              y_offset, dst + 48 + (start_row * dst_stride), dst_stride,       \
              height, &sse2, NULL, NULL);                                      \
          se += se2;                                                           \
          long_sse += sse2;                                                    \
        }                                                                      \
      }                                                                        \
    }                                                                          \
    se = ROUND_POWER_OF_TWO(se, 4);                                            \
    sse = (uint32_t)ROUND_POWER_OF_TWO(long_sse, 8);                           \
    *sse_ptr = sse;                                                            \
    var = (int64_t)(sse) - ((cast se * se) >> (wlog2 + hlog2));                \
    return (var >= 0) ? (uint32_t)var : 0;                                     \
  }

#define FNS(opt)                        \
  FN(64, 64, 16, 6, 6, opt, (int64_t)); \
  FN(64, 32, 16, 6, 5, opt, (int64_t)); \
  FN(32, 64, 16, 5, 6, opt, (int64_t)); \
  FN(32, 32, 16, 5, 5, opt, (int64_t)); \
  FN(32, 16, 16, 5, 4, opt, (int64_t)); \
  FN(16, 32, 16, 4, 5, opt, (int64_t)); \
  FN(16, 16, 16, 4, 4, opt, (int64_t)); \
  FN(16, 8, 16, 4, 3, opt, (int64_t));  \
  FN(8, 16, 8, 3, 4, opt, (int64_t));   \
  FN(8, 8, 8, 3, 3, opt, (int64_t));    \
  FN(8, 4, 8, 3, 2, opt, (int64_t));

FNS(sse2);

#undef FNS
#undef FN

// The 2 unused parameters are place holders for PIC enabled build.
#define DECL(w, opt)                                                         \
  int aom_highbd_sub_pixel_avg_variance##w##xh_##opt(                        \
      const uint16_t *src, ptrdiff_t src_stride, int x_offset, int y_offset, \
      const uint16_t *dst, ptrdiff_t dst_stride, const uint16_t *sec,        \
      ptrdiff_t sec_stride, int height, unsigned int *sse, void *unused0,    \
      void *unused);
#define DECLS(opt1) \
  DECL(16, opt1)    \
  DECL(8, opt1)

DECLS(sse2);
#undef DECL
#undef DECLS

#define FN(w, h, wf, wlog2, hlog2, opt, cast)                                  \
  uint32_t aom_highbd_8_sub_pixel_avg_variance##w##x##h##_##opt(               \
      const uint8_t *src8, int src_stride, int x_offset, int y_offset,         \
      const uint8_t *dst8, int dst_stride, uint32_t *sse_ptr,                  \
      const uint8_t *sec8) {                                                   \
    uint32_t sse;                                                              \
    uint16_t *src = CONVERT_TO_SHORTPTR(src8);                                 \
    uint16_t *dst = CONVERT_TO_SHORTPTR(dst8);                                 \
    uint16_t *sec = CONVERT_TO_SHORTPTR(sec8);                                 \
    int se = aom_highbd_sub_pixel_avg_variance##wf##xh_##opt(                  \
        src, src_stride, x_offset, y_offset, dst, dst_stride, sec, w, h, &sse, \
        NULL, NULL);                                                           \
    if (w > wf) {                                                              \
      uint32_t sse2;                                                           \
      int se2 = aom_highbd_sub_pixel_avg_variance##wf##xh_##opt(               \
          src + 16, src_stride, x_offset, y_offset, dst + 16, dst_stride,      \
          sec + 16, w, h, &sse2, NULL, NULL);                                  \
      se += se2;                                                               \
      sse += sse2;                                                             \
      if (w > wf * 2) {                                                        \
        se2 = aom_highbd_sub_pixel_avg_variance##wf##xh_##opt(                 \
            src + 32, src_stride, x_offset, y_offset, dst + 32, dst_stride,    \
            sec + 32, w, h, &sse2, NULL, NULL);                                \
        se += se2;                                                             \
        sse += sse2;                                                           \
        se2 = aom_highbd_sub_pixel_avg_variance##wf##xh_##opt(                 \
            src + 48, src_stride, x_offset, y_offset, dst + 48, dst_stride,    \
            sec + 48, w, h, &sse2, NULL, NULL);                                \
        se += se2;                                                             \
        sse += sse2;                                                           \
      }                                                                        \
    }                                                                          \
    *sse_ptr = sse;                                                            \
    return sse - (uint32_t)((cast se * se) >> (wlog2 + hlog2));                \
  }                                                                            \
                                                                               \
  uint32_t aom_highbd_10_sub_pixel_avg_variance##w##x##h##_##opt(              \
      const uint8_t *src8, int src_stride, int x_offset, int y_offset,         \
      const uint8_t *dst8, int dst_stride, uint32_t *sse_ptr,                  \
      const uint8_t *sec8) {                                                   \
    int64_t var;                                                               \
    uint32_t sse;                                                              \
    uint16_t *src = CONVERT_TO_SHORTPTR(src8);                                 \
    uint16_t *dst = CONVERT_TO_SHORTPTR(dst8);                                 \
    uint16_t *sec = CONVERT_TO_SHORTPTR(sec8);                                 \
    int se = aom_highbd_sub_pixel_avg_variance##wf##xh_##opt(                  \
        src, src_stride, x_offset, y_offset, dst, dst_stride, sec, w, h, &sse, \
        NULL, NULL);                                                           \
    if (w > wf) {                                                              \
      uint32_t sse2;                                                           \
      int se2 = aom_highbd_sub_pixel_avg_variance##wf##xh_##opt(               \
          src + 16, src_stride, x_offset, y_offset, dst + 16, dst_stride,      \
          sec + 16, w, h, &sse2, NULL, NULL);                                  \
      se += se2;                                                               \
      sse += sse2;                                                             \
      if (w > wf * 2) {                                                        \
        se2 = aom_highbd_sub_pixel_avg_variance##wf##xh_##opt(                 \
            src + 32, src_stride, x_offset, y_offset, dst + 32, dst_stride,    \
            sec + 32, w, h, &sse2, NULL, NULL);                                \
        se += se2;                                                             \
        sse += sse2;                                                           \
        se2 = aom_highbd_sub_pixel_avg_variance##wf##xh_##opt(                 \
            src + 48, src_stride, x_offset, y_offset, dst + 48, dst_stride,    \
            sec + 48, w, h, &sse2, NULL, NULL);                                \
        se += se2;                                                             \
        sse += sse2;                                                           \
      }                                                                        \
    }                                                                          \
    se = ROUND_POWER_OF_TWO(se, 2);                                            \
    sse = ROUND_POWER_OF_TWO(sse, 4);                                          \
    *sse_ptr = sse;                                                            \
    var = (int64_t)(sse) - ((cast se * se) >> (wlog2 + hlog2));                \
    return (var >= 0) ? (uint32_t)var : 0;                                     \
  }                                                                            \
                                                                               \
  uint32_t aom_highbd_12_sub_pixel_avg_variance##w##x##h##_##opt(              \
      const uint8_t *src8, int src_stride, int x_offset, int y_offset,         \
      const uint8_t *dst8, int dst_stride, uint32_t *sse_ptr,                  \
      const uint8_t *sec8) {                                                   \
    int start_row;                                                             \
    int64_t var;                                                               \
    uint32_t sse;                                                              \
    int se = 0;                                                                \
    uint64_t long_sse = 0;                                                     \
    uint16_t *src = CONVERT_TO_SHORTPTR(src8);                                 \
    uint16_t *dst = CONVERT_TO_SHORTPTR(dst8);                                 \
    uint16_t *sec = CONVERT_TO_SHORTPTR(sec8);                                 \
    for (start_row = 0; start_row < h; start_row += 16) {                      \
      uint32_t sse2;                                                           \
      int height = h - start_row < 16 ? h - start_row : 16;                    \
      int se2 = aom_highbd_sub_pixel_avg_variance##wf##xh_##opt(               \
          src + (start_row * src_stride), src_stride, x_offset, y_offset,      \
          dst + (start_row * dst_stride), dst_stride, sec + (start_row * w),   \
          w, height, &sse2, NULL, NULL);                                       \
      se += se2;                                                               \
      long_sse += sse2;                                                        \
      if (w > wf) {                                                            \
        se2 = aom_highbd_sub_pixel_avg_variance##wf##xh_##opt(                 \
            src + 16 + (start_row * src_stride), src_stride, x_offset,         \
            y_offset, dst + 16 + (start_row * dst_stride), dst_stride,         \
            sec + 16 + (start_row * w), w, height, &sse2, NULL, NULL);         \
        se += se2;                                                             \
        long_sse += sse2;                                                      \
        if (w > wf * 2) {                                                      \
          se2 = aom_highbd_sub_pixel_avg_variance##wf##xh_##opt(               \
              src + 32 + (start_row * src_stride), src_stride, x_offset,       \
              y_offset, dst + 32 + (start_row * dst_stride), dst_stride,       \
              sec + 32 + (start_row * w), w, height, &sse2, NULL, NULL);       \
          se += se2;                                                           \
          long_sse += sse2;                                                    \
          se2 = aom_highbd_sub_pixel_avg_variance##wf##xh_##opt(               \
              src + 48 + (start_row * src_stride), src_stride, x_offset,       \
              y_offset, dst + 48 + (start_row * dst_stride), dst_stride,       \
              sec + 48 + (start_row * w), w, height, &sse2, NULL, NULL);       \
          se += se2;                                                           \
          long_sse += sse2;                                                    \
        }                                                                      \
      }                                                                        \
    }                                                                          \
    se = ROUND_POWER_OF_TWO(se, 4);                                            \
    sse = (uint32_t)ROUND_POWER_OF_TWO(long_sse, 8);                           \
    *sse_ptr = sse;                                                            \
    var = (int64_t)(sse) - ((cast se * se) >> (wlog2 + hlog2));                \
    return (var >= 0) ? (uint32_t)var : 0;                                     \
  }

#define FNS(opt1)                        \
  FN(64, 64, 16, 6, 6, opt1, (int64_t)); \
  FN(64, 32, 16, 6, 5, opt1, (int64_t)); \
  FN(32, 64, 16, 5, 6, opt1, (int64_t)); \
  FN(32, 32, 16, 5, 5, opt1, (int64_t)); \
  FN(32, 16, 16, 5, 4, opt1, (int64_t)); \
  FN(16, 32, 16, 4, 5, opt1, (int64_t)); \
  FN(16, 16, 16, 4, 4, opt1, (int64_t)); \
  FN(16, 8, 16, 4, 3, opt1, (int64_t));  \
  FN(8, 16, 8, 4, 3, opt1, (int64_t));   \
  FN(8, 8, 8, 3, 3, opt1, (int64_t));    \
  FN(8, 4, 8, 3, 2, opt1, (int64_t));

FNS(sse2);

#undef FNS
#undef FN

void aom_highbd_upsampled_pred_sse2(uint16_t *comp_pred, int width, int height,
                                    const uint8_t *ref8, int ref_stride) {
  int i, j;
  int stride = ref_stride << 3;
  uint16_t *ref = CONVERT_TO_SHORTPTR(ref8);

  if (width >= 8) {
    // read 8 points at one time
    for (i = 0; i < height; i++) {
      for (j = 0; j < width; j += 8) {
        __m128i s0 = _mm_cvtsi32_si128(*(const uint32_t *)ref);
        __m128i s1 = _mm_cvtsi32_si128(*(const uint32_t *)(ref + 8));
        __m128i s2 = _mm_cvtsi32_si128(*(const uint32_t *)(ref + 16));
        __m128i s3 = _mm_cvtsi32_si128(*(const uint32_t *)(ref + 24));
        __m128i s4 = _mm_cvtsi32_si128(*(const uint32_t *)(ref + 32));
        __m128i s5 = _mm_cvtsi32_si128(*(const uint32_t *)(ref + 40));
        __m128i s6 = _mm_cvtsi32_si128(*(const uint32_t *)(ref + 48));
        __m128i s7 = _mm_cvtsi32_si128(*(const uint32_t *)(ref + 56));
        __m128i t0, t1, t2, t3;

        t0 = _mm_unpacklo_epi16(s0, s1);
        t1 = _mm_unpacklo_epi16(s2, s3);
        t2 = _mm_unpacklo_epi16(s4, s5);
        t3 = _mm_unpacklo_epi16(s6, s7);
        t0 = _mm_unpacklo_epi32(t0, t1);
        t2 = _mm_unpacklo_epi32(t2, t3);
        t0 = _mm_unpacklo_epi64(t0, t2);

        _mm_storeu_si128((__m128i *)(comp_pred), t0);
        comp_pred += 8;
        ref += 64;  // 8 * 8;
      }
      ref += stride - (width << 3);
    }
  } else {
    // read 4 points at one time
    for (i = 0; i < height; i++) {
      for (j = 0; j < width; j += 4) {
        __m128i s0 = _mm_cvtsi32_si128(*(const uint32_t *)ref);
        __m128i s1 = _mm_cvtsi32_si128(*(const uint32_t *)(ref + 8));
        __m128i s2 = _mm_cvtsi32_si128(*(const uint32_t *)(ref + 16));
        __m128i s3 = _mm_cvtsi32_si128(*(const uint32_t *)(ref + 24));
        __m128i t0, t1;

        t0 = _mm_unpacklo_epi16(s0, s1);
        t1 = _mm_unpacklo_epi16(s2, s3);
        t0 = _mm_unpacklo_epi32(t0, t1);

        _mm_storel_epi64((__m128i *)(comp_pred), t0);
        comp_pred += 4;
        ref += 4 * 8;
      }
      ref += stride - (width << 3);
    }
  }
}

void aom_highbd_comp_avg_upsampled_pred_sse2(uint16_t *comp_pred,
                                             const uint8_t *pred8, int width,
                                             int height, const uint8_t *ref8,
                                             int ref_stride) {
  const __m128i one = _mm_set1_epi16(1);
  int i, j;
  int stride = ref_stride << 3;
  uint16_t *pred = CONVERT_TO_SHORTPTR(pred8);
  uint16_t *ref = CONVERT_TO_SHORTPTR(ref8);

  if (width >= 8) {
    // read 8 points at one time
    for (i = 0; i < height; i++) {
      for (j = 0; j < width; j += 8) {
        __m128i s0 = _mm_cvtsi32_si128(*(const uint32_t *)ref);
        __m128i s1 = _mm_cvtsi32_si128(*(const uint32_t *)(ref + 8));
        __m128i s2 = _mm_cvtsi32_si128(*(const uint32_t *)(ref + 16));
        __m128i s3 = _mm_cvtsi32_si128(*(const uint32_t *)(ref + 24));
        __m128i s4 = _mm_cvtsi32_si128(*(const uint32_t *)(ref + 32));
        __m128i s5 = _mm_cvtsi32_si128(*(const uint32_t *)(ref + 40));
        __m128i s6 = _mm_cvtsi32_si128(*(const uint32_t *)(ref + 48));
        __m128i s7 = _mm_cvtsi32_si128(*(const uint32_t *)(ref + 56));
        __m128i p0 = _mm_loadu_si128((const __m128i *)pred);
        __m128i t0, t1, t2, t3;

        t0 = _mm_unpacklo_epi16(s0, s1);
        t1 = _mm_unpacklo_epi16(s2, s3);
        t2 = _mm_unpacklo_epi16(s4, s5);
        t3 = _mm_unpacklo_epi16(s6, s7);
        t0 = _mm_unpacklo_epi32(t0, t1);
        t2 = _mm_unpacklo_epi32(t2, t3);
        t0 = _mm_unpacklo_epi64(t0, t2);

        p0 = _mm_adds_epu16(t0, p0);
        p0 = _mm_adds_epu16(p0, one);
        p0 = _mm_srli_epi16(p0, 1);

        _mm_storeu_si128((__m128i *)(comp_pred), p0);
        comp_pred += 8;
        pred += 8;
        ref += 8 * 8;
      }
      ref += stride - (width << 3);
    }
  } else {
    // read 4 points at one time
    for (i = 0; i < height; i++) {
      for (j = 0; j < width; j += 4) {
        __m128i s0 = _mm_cvtsi32_si128(*(const uint32_t *)ref);
        __m128i s1 = _mm_cvtsi32_si128(*(const uint32_t *)(ref + 8));
        __m128i s2 = _mm_cvtsi32_si128(*(const uint32_t *)(ref + 16));
        __m128i s3 = _mm_cvtsi32_si128(*(const uint32_t *)(ref + 24));
        __m128i p0 = _mm_loadl_epi64((const __m128i *)pred);
        __m128i t0, t1;

        t0 = _mm_unpacklo_epi16(s0, s1);
        t1 = _mm_unpacklo_epi16(s2, s3);
        t0 = _mm_unpacklo_epi32(t0, t1);

        p0 = _mm_adds_epu16(t0, p0);
        p0 = _mm_adds_epu16(p0, one);
        p0 = _mm_srli_epi16(p0, 1);

        _mm_storel_epi64((__m128i *)(comp_pred), p0);
        comp_pred += 4;
        pred += 4;
        ref += 4 * 8;
      }
      ref += stride - (width << 3);
    }
  }
}
