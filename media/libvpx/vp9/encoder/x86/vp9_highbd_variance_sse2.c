/*
 *  Copyright (c) 2014 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "./vpx_config.h"
#include "vp9/common/vp9_common.h"

#include "vp9/encoder/vp9_variance.h"
#include "vpx_ports/mem.h"

#define DECL(w, opt) \
int vp9_highbd_sub_pixel_variance##w##xh_##opt(const uint16_t *src, \
                                               ptrdiff_t src_stride, \
                                               int x_offset, int y_offset, \
                                               const uint16_t *dst, \
                                               ptrdiff_t dst_stride, \
                                               int height, unsigned int *sse);
#define DECLS(opt1, opt2) \
DECL(8, opt1); \
DECL(16, opt1)

DECLS(sse2, sse);
// DECLS(ssse3, ssse3);
#undef DECLS
#undef DECL

#define FN(w, h, wf, wlog2, hlog2, opt, cast) \
uint32_t vp9_highbd_sub_pixel_variance##w##x##h##_##opt(const uint8_t *src8, \
                                                        int src_stride, \
                                                        int x_offset, \
                                                        int y_offset, \
                                                        const uint8_t *dst8, \
                                                        int dst_stride, \
                                                        uint32_t *sse_ptr) { \
  uint32_t sse; \
  uint16_t *src = CONVERT_TO_SHORTPTR(src8); \
  uint16_t *dst = CONVERT_TO_SHORTPTR(dst8); \
  int se = vp9_highbd_sub_pixel_variance##wf##xh_##opt(src, src_stride, \
                                                       x_offset, y_offset, \
                                                       dst, dst_stride, h, \
                                                       &sse); \
  if (w > wf) { \
    unsigned int sse2; \
    int se2 = vp9_highbd_sub_pixel_variance##wf##xh_##opt(src + 16, \
                                                          src_stride, \
                                                          x_offset, y_offset, \
                                                          dst + 16, \
                                                          dst_stride, \
                                                          h, &sse2); \
    se += se2; \
    sse += sse2; \
    if (w > wf * 2) { \
      se2 = vp9_highbd_sub_pixel_variance##wf##xh_##opt(src + 32, src_stride, \
                                                        x_offset, y_offset, \
                                                        dst + 32, dst_stride, \
                                                        h, &sse2); \
      se += se2; \
      sse += sse2; \
      se2 = vp9_highbd_sub_pixel_variance##wf##xh_##opt( \
          src + 48, src_stride, x_offset, y_offset, \
          dst + 48, dst_stride, h, &sse2); \
      se += se2; \
      sse += sse2; \
    } \
  } \
  *sse_ptr = sse; \
  return sse - ((cast se * se) >> (wlog2 + hlog2)); \
} \
\
uint32_t vp9_highbd_10_sub_pixel_variance##w##x##h##_##opt( \
    const uint8_t *src8, int src_stride, int x_offset, int y_offset, \
    const uint8_t *dst8, int dst_stride, uint32_t *sse_ptr) { \
  uint32_t sse; \
  uint16_t *src = CONVERT_TO_SHORTPTR(src8); \
  uint16_t *dst = CONVERT_TO_SHORTPTR(dst8); \
  int se = vp9_highbd_sub_pixel_variance##wf##xh_##opt(src, src_stride, \
                                                       x_offset, y_offset, \
                                                       dst, dst_stride, \
                                                       h, &sse); \
  if (w > wf) { \
    uint32_t sse2; \
    int se2 = vp9_highbd_sub_pixel_variance##wf##xh_##opt(src + 16, \
                                                          src_stride, \
                                                          x_offset, y_offset, \
                                                          dst + 16, \
                                                          dst_stride, \
                                                          h, &sse2); \
    se += se2; \
    sse += sse2; \
    if (w > wf * 2) { \
      se2 = vp9_highbd_sub_pixel_variance##wf##xh_##opt(src + 32, src_stride, \
                                                        x_offset, y_offset, \
                                                        dst + 32, dst_stride, \
                                                        h, &sse2); \
      se += se2; \
      sse += sse2; \
      se2 = vp9_highbd_sub_pixel_variance##wf##xh_##opt(src + 48, src_stride, \
                                                        x_offset, y_offset, \
                                                        dst + 48, dst_stride, \
                                                        h, &sse2); \
      se += se2; \
      sse += sse2; \
    } \
  } \
  se = ROUND_POWER_OF_TWO(se, 2); \
  sse = ROUND_POWER_OF_TWO(sse, 4); \
  *sse_ptr = sse; \
  return sse - ((cast se * se) >> (wlog2 + hlog2)); \
} \
\
uint32_t vp9_highbd_12_sub_pixel_variance##w##x##h##_##opt( \
    const uint8_t *src8, int src_stride, int x_offset, int y_offset, \
    const uint8_t *dst8, int dst_stride, uint32_t *sse_ptr) { \
  int start_row; \
  uint32_t sse; \
  int se = 0; \
  uint64_t long_sse = 0; \
  uint16_t *src = CONVERT_TO_SHORTPTR(src8); \
  uint16_t *dst = CONVERT_TO_SHORTPTR(dst8); \
  for (start_row = 0; start_row < h; start_row +=16) { \
    uint32_t sse2; \
    int height = h - start_row < 16 ? h - start_row : 16; \
    int se2 = vp9_highbd_sub_pixel_variance##wf##xh_##opt( \
        src + (start_row * src_stride), src_stride, \
        x_offset, y_offset, dst + (start_row * dst_stride), \
        dst_stride, height, &sse2); \
    se += se2; \
    long_sse += sse2; \
    if (w > wf) { \
      se2 = vp9_highbd_sub_pixel_variance##wf##xh_##opt( \
          src + 16 + (start_row * src_stride), src_stride, \
          x_offset, y_offset, dst + 16 + (start_row * dst_stride), \
          dst_stride, height, &sse2); \
      se += se2; \
      long_sse += sse2; \
      if (w > wf * 2) { \
        se2 = vp9_highbd_sub_pixel_variance##wf##xh_##opt( \
            src + 32 + (start_row * src_stride), src_stride, \
            x_offset, y_offset, dst + 32 + (start_row * dst_stride), \
            dst_stride, height, &sse2); \
        se += se2; \
        long_sse += sse2; \
        se2 = vp9_highbd_sub_pixel_variance##wf##xh_##opt( \
            src + 48 + (start_row * src_stride), src_stride, \
            x_offset, y_offset, dst + 48 + (start_row * dst_stride), \
            dst_stride, height, &sse2); \
        se += se2; \
        long_sse += sse2; \
      }\
    } \
  } \
  se = ROUND_POWER_OF_TWO(se, 4); \
  sse = ROUND_POWER_OF_TWO(long_sse, 8); \
  *sse_ptr = sse; \
  return sse - ((cast se * se) >> (wlog2 + hlog2)); \
}

#define FNS(opt1, opt2) \
FN(64, 64, 16, 6, 6, opt1, (int64_t)); \
FN(64, 32, 16, 6, 5, opt1, (int64_t)); \
FN(32, 64, 16, 5, 6, opt1, (int64_t)); \
FN(32, 32, 16, 5, 5, opt1, (int64_t)); \
FN(32, 16, 16, 5, 4, opt1, (int64_t)); \
FN(16, 32, 16, 4, 5, opt1, (int64_t)); \
FN(16, 16, 16, 4, 4, opt1, (int64_t)); \
FN(16, 8, 16, 4, 3, opt1, (int64_t)); \
FN(8, 16, 8, 3, 4, opt1, (int64_t)); \
FN(8, 8, 8, 3, 3, opt1, (int64_t)); \
FN(8, 4, 8, 3, 2, opt1, (int64_t));


FNS(sse2, sse);

#undef FNS
#undef FN

#define DECL(w, opt) \
int vp9_highbd_sub_pixel_avg_variance##w##xh_##opt(const uint16_t *src, \
                                                   ptrdiff_t src_stride, \
                                                   int x_offset, int y_offset, \
                                                   const uint16_t *dst, \
                                                   ptrdiff_t dst_stride, \
                                                   const uint16_t *sec, \
                                                   ptrdiff_t sec_stride, \
                                                   int height, \
                                                   unsigned int *sse);
#define DECLS(opt1) \
DECL(16, opt1) \
DECL(8, opt1)

DECLS(sse2);
#undef DECL
#undef DECLS

#define FN(w, h, wf, wlog2, hlog2, opt, cast) \
uint32_t vp9_highbd_sub_pixel_avg_variance##w##x##h##_##opt( \
    const uint8_t *src8, int src_stride, int x_offset, int y_offset, \
    const uint8_t *dst8, int dst_stride, uint32_t *sse_ptr, \
    const uint8_t *sec8) { \
  uint32_t sse; \
  uint16_t *src = CONVERT_TO_SHORTPTR(src8); \
  uint16_t *dst = CONVERT_TO_SHORTPTR(dst8); \
  uint16_t *sec = CONVERT_TO_SHORTPTR(sec8); \
  int se = vp9_highbd_sub_pixel_avg_variance##wf##xh_##opt( \
               src, src_stride, x_offset, \
               y_offset, dst, dst_stride, sec, w, h, &sse); \
  if (w > wf) { \
    uint32_t sse2; \
    int se2 = vp9_highbd_sub_pixel_avg_variance##wf##xh_##opt( \
                  src + 16, src_stride, x_offset, y_offset, \
                  dst + 16, dst_stride, sec + 16, w, h, &sse2); \
    se += se2; \
    sse += sse2; \
    if (w > wf * 2) { \
      se2 = vp9_highbd_sub_pixel_avg_variance##wf##xh_##opt( \
                src + 32, src_stride, x_offset, y_offset, \
                dst + 32, dst_stride, sec + 32, w, h, &sse2); \
      se += se2; \
      sse += sse2; \
      se2 = vp9_highbd_sub_pixel_avg_variance##wf##xh_##opt( \
                src + 48, src_stride, x_offset, y_offset, \
                dst + 48, dst_stride, sec + 48, w, h, &sse2); \
      se += se2; \
      sse += sse2; \
    } \
  } \
  *sse_ptr = sse; \
  return sse - ((cast se * se) >> (wlog2 + hlog2)); \
} \
\
uint32_t vp9_highbd_10_sub_pixel_avg_variance##w##x##h##_##opt( \
    const uint8_t *src8, int src_stride, int x_offset, int y_offset, \
    const uint8_t *dst8, int dst_stride, uint32_t *sse_ptr, \
    const uint8_t *sec8) { \
  uint32_t sse; \
  uint16_t *src = CONVERT_TO_SHORTPTR(src8); \
  uint16_t *dst = CONVERT_TO_SHORTPTR(dst8); \
  uint16_t *sec = CONVERT_TO_SHORTPTR(sec8); \
  int se = vp9_highbd_sub_pixel_avg_variance##wf##xh_##opt( \
                                            src, src_stride, x_offset, \
                                            y_offset, dst, dst_stride, \
                                            sec, w, h, &sse); \
  if (w > wf) { \
    uint32_t sse2; \
    int se2 = vp9_highbd_sub_pixel_avg_variance##wf##xh_##opt( \
                                            src + 16, src_stride, \
                                            x_offset, y_offset, \
                                            dst + 16, dst_stride, \
                                            sec + 16, w, h, &sse2); \
    se += se2; \
    sse += sse2; \
    if (w > wf * 2) { \
      se2 = vp9_highbd_sub_pixel_avg_variance##wf##xh_##opt( \
                                            src + 32, src_stride, \
                                            x_offset, y_offset, \
                                            dst + 32, dst_stride, \
                                            sec + 32, w, h, &sse2); \
      se += se2; \
      sse += sse2; \
      se2 = vp9_highbd_sub_pixel_avg_variance##wf##xh_##opt( \
                                            src + 48, src_stride, \
                                            x_offset, y_offset, \
                                            dst + 48, dst_stride, \
                                            sec + 48, w, h, &sse2); \
      se += se2; \
      sse += sse2; \
    } \
  } \
  se = ROUND_POWER_OF_TWO(se, 2); \
  sse = ROUND_POWER_OF_TWO(sse, 4); \
  *sse_ptr = sse; \
  return sse - ((cast se * se) >> (wlog2 + hlog2)); \
} \
\
uint32_t vp9_highbd_12_sub_pixel_avg_variance##w##x##h##_##opt( \
    const uint8_t *src8, int src_stride, int x_offset, int y_offset, \
    const uint8_t *dst8, int dst_stride, uint32_t *sse_ptr, \
    const uint8_t *sec8) { \
  int start_row; \
  uint32_t sse; \
  int se = 0; \
  uint64_t long_sse = 0; \
  uint16_t *src = CONVERT_TO_SHORTPTR(src8); \
  uint16_t *dst = CONVERT_TO_SHORTPTR(dst8); \
  uint16_t *sec = CONVERT_TO_SHORTPTR(sec8); \
  for (start_row = 0; start_row < h; start_row +=16) { \
    uint32_t sse2; \
    int height = h - start_row < 16 ? h - start_row : 16; \
    int se2 = vp9_highbd_sub_pixel_avg_variance##wf##xh_##opt( \
                src + (start_row * src_stride), src_stride, x_offset, \
                y_offset, dst + (start_row * dst_stride), dst_stride, \
                sec + (start_row * w), w, height, &sse2); \
    se += se2; \
    long_sse += sse2; \
    if (w > wf) { \
      se2 = vp9_highbd_sub_pixel_avg_variance##wf##xh_##opt( \
                src + 16 + (start_row * src_stride), src_stride, \
                x_offset, y_offset, \
                dst + 16 + (start_row * dst_stride), dst_stride, \
                sec + 16 + (start_row * w), w, height, &sse2); \
      se += se2; \
      long_sse += sse2; \
      if (w > wf * 2) { \
        se2 = vp9_highbd_sub_pixel_avg_variance##wf##xh_##opt( \
                src + 32 + (start_row * src_stride), src_stride, \
                x_offset, y_offset, \
                dst + 32 + (start_row * dst_stride), dst_stride, \
                sec + 32 + (start_row * w), w, height, &sse2); \
        se += se2; \
        long_sse += sse2; \
        se2 = vp9_highbd_sub_pixel_avg_variance##wf##xh_##opt( \
                src + 48 + (start_row * src_stride), src_stride, \
                x_offset, y_offset, \
                dst + 48 + (start_row * dst_stride), dst_stride, \
                sec + 48 + (start_row * w), w, height, &sse2); \
        se += se2; \
        long_sse += sse2; \
      } \
    } \
  } \
  se = ROUND_POWER_OF_TWO(se, 4); \
  sse = ROUND_POWER_OF_TWO(long_sse, 8); \
  *sse_ptr = sse; \
  return sse - ((cast se * se) >> (wlog2 + hlog2)); \
}


#define FNS(opt1) \
FN(64, 64, 16, 6, 6, opt1, (int64_t)); \
FN(64, 32, 16, 6, 5, opt1, (int64_t)); \
FN(32, 64, 16, 5, 6, opt1, (int64_t)); \
FN(32, 32, 16, 5, 5, opt1, (int64_t)); \
FN(32, 16, 16, 5, 4, opt1, (int64_t)); \
FN(16, 32, 16, 4, 5, opt1, (int64_t)); \
FN(16, 16, 16, 4, 4, opt1, (int64_t)); \
FN(16, 8, 16, 4, 3, opt1, (int64_t)); \
FN(8, 16, 8, 4, 3, opt1, (int64_t)); \
FN(8, 8, 8, 3, 3, opt1, (int64_t)); \
FN(8, 4, 8, 3, 2, opt1, (int64_t));

FNS(sse2);

#undef FNS
#undef FN
