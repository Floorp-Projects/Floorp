/*
 *  Copyright (c) 2013 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <assert.h>

#include "./vpx_config.h"
#include "./vp9_rtcd.h"
#include "vp9/common/vp9_common.h"
#include "vp9/common/vp9_convolve.h"
#include "vp9/common/vp9_filter.h"
#include "vpx/vpx_integer.h"
#include "vpx_ports/mem.h"

static void convolve_horiz(const uint8_t *src, ptrdiff_t src_stride,
                           uint8_t *dst, ptrdiff_t dst_stride,
                           const InterpKernel *x_filters,
                           int x0_q4, int x_step_q4, int w, int h) {
  int x, y;
  src -= SUBPEL_TAPS / 2 - 1;
  for (y = 0; y < h; ++y) {
    int x_q4 = x0_q4;
    for (x = 0; x < w; ++x) {
      const uint8_t *const src_x = &src[x_q4 >> SUBPEL_BITS];
      const int16_t *const x_filter = x_filters[x_q4 & SUBPEL_MASK];
      int k, sum = 0;
      for (k = 0; k < SUBPEL_TAPS; ++k)
        sum += src_x[k] * x_filter[k];
      dst[x] = clip_pixel(ROUND_POWER_OF_TWO(sum, FILTER_BITS));
      x_q4 += x_step_q4;
    }
    src += src_stride;
    dst += dst_stride;
  }
}

static void convolve_avg_horiz(const uint8_t *src, ptrdiff_t src_stride,
                               uint8_t *dst, ptrdiff_t dst_stride,
                               const InterpKernel *x_filters,
                               int x0_q4, int x_step_q4, int w, int h) {
  int x, y;
  src -= SUBPEL_TAPS / 2 - 1;
  for (y = 0; y < h; ++y) {
    int x_q4 = x0_q4;
    for (x = 0; x < w; ++x) {
      const uint8_t *const src_x = &src[x_q4 >> SUBPEL_BITS];
      const int16_t *const x_filter = x_filters[x_q4 & SUBPEL_MASK];
      int k, sum = 0;
      for (k = 0; k < SUBPEL_TAPS; ++k)
        sum += src_x[k] * x_filter[k];
      dst[x] = ROUND_POWER_OF_TWO(dst[x] +
          clip_pixel(ROUND_POWER_OF_TWO(sum, FILTER_BITS)), 1);
      x_q4 += x_step_q4;
    }
    src += src_stride;
    dst += dst_stride;
  }
}

static void convolve_vert(const uint8_t *src, ptrdiff_t src_stride,
                          uint8_t *dst, ptrdiff_t dst_stride,
                          const InterpKernel *y_filters,
                          int y0_q4, int y_step_q4, int w, int h) {
  int x, y;
  src -= src_stride * (SUBPEL_TAPS / 2 - 1);

  for (x = 0; x < w; ++x) {
    int y_q4 = y0_q4;
    for (y = 0; y < h; ++y) {
      const unsigned char *src_y = &src[(y_q4 >> SUBPEL_BITS) * src_stride];
      const int16_t *const y_filter = y_filters[y_q4 & SUBPEL_MASK];
      int k, sum = 0;
      for (k = 0; k < SUBPEL_TAPS; ++k)
        sum += src_y[k * src_stride] * y_filter[k];
      dst[y * dst_stride] = clip_pixel(ROUND_POWER_OF_TWO(sum, FILTER_BITS));
      y_q4 += y_step_q4;
    }
    ++src;
    ++dst;
  }
}

static void convolve_avg_vert(const uint8_t *src, ptrdiff_t src_stride,
                              uint8_t *dst, ptrdiff_t dst_stride,
                              const InterpKernel *y_filters,
                              int y0_q4, int y_step_q4, int w, int h) {
  int x, y;
  src -= src_stride * (SUBPEL_TAPS / 2 - 1);

  for (x = 0; x < w; ++x) {
    int y_q4 = y0_q4;
    for (y = 0; y < h; ++y) {
      const unsigned char *src_y = &src[(y_q4 >> SUBPEL_BITS) * src_stride];
      const int16_t *const y_filter = y_filters[y_q4 & SUBPEL_MASK];
      int k, sum = 0;
      for (k = 0; k < SUBPEL_TAPS; ++k)
        sum += src_y[k * src_stride] * y_filter[k];
      dst[y * dst_stride] = ROUND_POWER_OF_TWO(dst[y * dst_stride] +
          clip_pixel(ROUND_POWER_OF_TWO(sum, FILTER_BITS)), 1);
      y_q4 += y_step_q4;
    }
    ++src;
    ++dst;
  }
}

static void convolve(const uint8_t *src, ptrdiff_t src_stride,
                     uint8_t *dst, ptrdiff_t dst_stride,
                     const InterpKernel *const x_filters,
                     int x0_q4, int x_step_q4,
                     const InterpKernel *const y_filters,
                     int y0_q4, int y_step_q4,
                     int w, int h) {
  // Fixed size intermediate buffer places limits on parameters.
  // Maximum intermediate_height is 324, for y_step_q4 == 80,
  // h == 64, taps == 8.
  // y_step_q4 of 80 allows for 1/10 scale for 5 layer svc
  uint8_t temp[64 * 324];
  int intermediate_height = (((h - 1) * y_step_q4 + 15) >> 4) + SUBPEL_TAPS;

  assert(w <= 64);
  assert(h <= 64);
  assert(y_step_q4 <= 80);
  assert(x_step_q4 <= 80);

  if (intermediate_height < h)
    intermediate_height = h;

  convolve_horiz(src - src_stride * (SUBPEL_TAPS / 2 - 1), src_stride, temp, 64,
                 x_filters, x0_q4, x_step_q4, w, intermediate_height);
  convolve_vert(temp + 64 * (SUBPEL_TAPS / 2 - 1), 64, dst, dst_stride,
                y_filters, y0_q4, y_step_q4, w, h);
}

static const InterpKernel *get_filter_base(const int16_t *filter) {
  // NOTE: This assumes that the filter table is 256-byte aligned.
  // TODO(agrange) Modify to make independent of table alignment.
  return (const InterpKernel *)(((intptr_t)filter) & ~((intptr_t)0xFF));
}

static int get_filter_offset(const int16_t *f, const InterpKernel *base) {
  return (int)((const InterpKernel *)(intptr_t)f - base);
}

void vp9_convolve8_horiz_c(const uint8_t *src, ptrdiff_t src_stride,
                           uint8_t *dst, ptrdiff_t dst_stride,
                           const int16_t *filter_x, int x_step_q4,
                           const int16_t *filter_y, int y_step_q4,
                           int w, int h) {
  const InterpKernel *const filters_x = get_filter_base(filter_x);
  const int x0_q4 = get_filter_offset(filter_x, filters_x);

  convolve_horiz(src, src_stride, dst, dst_stride, filters_x,
                 x0_q4, x_step_q4, w, h);
}

void vp9_convolve8_avg_horiz_c(const uint8_t *src, ptrdiff_t src_stride,
                               uint8_t *dst, ptrdiff_t dst_stride,
                               const int16_t *filter_x, int x_step_q4,
                               const int16_t *filter_y, int y_step_q4,
                               int w, int h) {
  const InterpKernel *const filters_x = get_filter_base(filter_x);
  const int x0_q4 = get_filter_offset(filter_x, filters_x);

  convolve_avg_horiz(src, src_stride, dst, dst_stride, filters_x,
                     x0_q4, x_step_q4, w, h);
}

void vp9_convolve8_vert_c(const uint8_t *src, ptrdiff_t src_stride,
                          uint8_t *dst, ptrdiff_t dst_stride,
                          const int16_t *filter_x, int x_step_q4,
                          const int16_t *filter_y, int y_step_q4,
                          int w, int h) {
  const InterpKernel *const filters_y = get_filter_base(filter_y);
  const int y0_q4 = get_filter_offset(filter_y, filters_y);
  convolve_vert(src, src_stride, dst, dst_stride, filters_y,
                y0_q4, y_step_q4, w, h);
}

void vp9_convolve8_avg_vert_c(const uint8_t *src, ptrdiff_t src_stride,
                              uint8_t *dst, ptrdiff_t dst_stride,
                              const int16_t *filter_x, int x_step_q4,
                              const int16_t *filter_y, int y_step_q4,
                              int w, int h) {
  const InterpKernel *const filters_y = get_filter_base(filter_y);
  const int y0_q4 = get_filter_offset(filter_y, filters_y);
  convolve_avg_vert(src, src_stride, dst, dst_stride, filters_y,
                    y0_q4, y_step_q4, w, h);
}

void vp9_convolve8_c(const uint8_t *src, ptrdiff_t src_stride,
                     uint8_t *dst, ptrdiff_t dst_stride,
                     const int16_t *filter_x, int x_step_q4,
                     const int16_t *filter_y, int y_step_q4,
                     int w, int h) {
  const InterpKernel *const filters_x = get_filter_base(filter_x);
  const int x0_q4 = get_filter_offset(filter_x, filters_x);

  const InterpKernel *const filters_y = get_filter_base(filter_y);
  const int y0_q4 = get_filter_offset(filter_y, filters_y);

  convolve(src, src_stride, dst, dst_stride,
           filters_x, x0_q4, x_step_q4,
           filters_y, y0_q4, y_step_q4, w, h);
}

void vp9_convolve8_avg_c(const uint8_t *src, ptrdiff_t src_stride,
                         uint8_t *dst, ptrdiff_t dst_stride,
                         const int16_t *filter_x, int x_step_q4,
                         const int16_t *filter_y, int y_step_q4,
                         int w, int h) {
  /* Fixed size intermediate buffer places limits on parameters. */
  DECLARE_ALIGNED_ARRAY(16, uint8_t, temp, 64 * 64);
  assert(w <= 64);
  assert(h <= 64);

  vp9_convolve8_c(src, src_stride, temp, 64,
                  filter_x, x_step_q4, filter_y, y_step_q4, w, h);
  vp9_convolve_avg_c(temp, 64, dst, dst_stride, NULL, 0, NULL, 0, w, h);
}

void vp9_convolve_copy_c(const uint8_t *src, ptrdiff_t src_stride,
                         uint8_t *dst, ptrdiff_t dst_stride,
                         const int16_t *filter_x, int filter_x_stride,
                         const int16_t *filter_y, int filter_y_stride,
                         int w, int h) {
  int r;

  for (r = h; r > 0; --r) {
    vpx_memcpy(dst, src, w);
    src += src_stride;
    dst += dst_stride;
  }
}

void vp9_convolve_avg_c(const uint8_t *src, ptrdiff_t src_stride,
                        uint8_t *dst, ptrdiff_t dst_stride,
                        const int16_t *filter_x, int filter_x_stride,
                        const int16_t *filter_y, int filter_y_stride,
                        int w, int h) {
  int x, y;

  for (y = 0; y < h; ++y) {
    for (x = 0; x < w; ++x)
      dst[x] = ROUND_POWER_OF_TWO(dst[x] + src[x], 1);

    src += src_stride;
    dst += dst_stride;
  }
}
