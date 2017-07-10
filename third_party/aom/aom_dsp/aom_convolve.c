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

#include <assert.h>
#include <string.h>

#include "./aom_config.h"
#include "./aom_dsp_rtcd.h"
#include "aom/aom_integer.h"
#include "aom_dsp/aom_convolve.h"
#include "aom_dsp/aom_dsp_common.h"
#include "aom_dsp/aom_filter.h"
#include "aom_ports/mem.h"

static void convolve_horiz(const uint8_t *src, ptrdiff_t src_stride,
                           uint8_t *dst, ptrdiff_t dst_stride,
                           const InterpKernel *x_filters, int x0_q4,
                           int x_step_q4, int w, int h) {
  int x, y;
  src -= SUBPEL_TAPS / 2 - 1;
  for (y = 0; y < h; ++y) {
    int x_q4 = x0_q4;
    for (x = 0; x < w; ++x) {
      const uint8_t *const src_x = &src[x_q4 >> SUBPEL_BITS];
      const int16_t *const x_filter = x_filters[x_q4 & SUBPEL_MASK];
      int k, sum = 0;
      for (k = 0; k < SUBPEL_TAPS; ++k) sum += src_x[k] * x_filter[k];
      dst[x] = clip_pixel(ROUND_POWER_OF_TWO(sum, FILTER_BITS));
      x_q4 += x_step_q4;
    }
    src += src_stride;
    dst += dst_stride;
  }
}

static void convolve_horiz_scale_c(const uint8_t *src, ptrdiff_t src_stride,
                                   uint8_t *dst, ptrdiff_t dst_stride,
                                   const InterpKernel *x_filters, int x0_qn,
                                   int x_step_qn, int w, int h) {
  int x, y;
  src -= SUBPEL_TAPS / 2 - 1;
  for (y = 0; y < h; ++y) {
    int x_qn = x0_qn;
    for (x = 0; x < w; ++x) {
      const uint8_t *const src_x = &src[x_qn >> SCALE_SUBPEL_BITS];  // q8
      const int x_filter_idx = (x_qn & SCALE_SUBPEL_MASK) >> SCALE_EXTRA_BITS;
      assert(x_filter_idx < SUBPEL_SHIFTS);
      const int16_t *const x_filter = x_filters[x_filter_idx];
      int k, sum = 0;
      for (k = 0; k < SUBPEL_TAPS; ++k) sum += src_x[k] * x_filter[k];
      dst[x] = clip_pixel(ROUND_POWER_OF_TWO(sum, FILTER_BITS));
      x_qn += x_step_qn;
    }
    src += src_stride;
    dst += dst_stride;
  }
}

static void convolve_avg_horiz(const uint8_t *src, ptrdiff_t src_stride,
                               uint8_t *dst, ptrdiff_t dst_stride,
                               const InterpKernel *x_filters, int x0_q4,
                               int x_step_q4, int w, int h) {
  int x, y;
  src -= SUBPEL_TAPS / 2 - 1;
  for (y = 0; y < h; ++y) {
    int x_q4 = x0_q4;
    for (x = 0; x < w; ++x) {
      const uint8_t *const src_x = &src[x_q4 >> SUBPEL_BITS];
      const int16_t *const x_filter = x_filters[x_q4 & SUBPEL_MASK];
      int k, sum = 0;
      for (k = 0; k < SUBPEL_TAPS; ++k) sum += src_x[k] * x_filter[k];
      dst[x] = ROUND_POWER_OF_TWO(
          dst[x] + clip_pixel(ROUND_POWER_OF_TWO(sum, FILTER_BITS)), 1);
      x_q4 += x_step_q4;
    }
    src += src_stride;
    dst += dst_stride;
  }
}

static void convolve_avg_horiz_scale_c(const uint8_t *src, ptrdiff_t src_stride,
                                       uint8_t *dst, ptrdiff_t dst_stride,
                                       const InterpKernel *x_filters, int x0_qn,
                                       int x_step_qn, int w, int h) {
  int x, y;
  src -= SUBPEL_TAPS / 2 - 1;
  for (y = 0; y < h; ++y) {
    int x_qn = x0_qn;
    for (x = 0; x < w; ++x) {
      const uint8_t *const src_x = &src[x_qn >> SCALE_SUBPEL_BITS];
      const int x_filter_idx = (x_qn & SCALE_SUBPEL_MASK) >> SCALE_EXTRA_BITS;
      assert(x_filter_idx < SUBPEL_SHIFTS);
      const int16_t *const x_filter = x_filters[x_filter_idx];
      int k, sum = 0;
      for (k = 0; k < SUBPEL_TAPS; ++k) sum += src_x[k] * x_filter[k];
      dst[x] = ROUND_POWER_OF_TWO(
          dst[x] + clip_pixel(ROUND_POWER_OF_TWO(sum, FILTER_BITS)), 1);
      x_qn += x_step_qn;
    }
    src += src_stride;
    dst += dst_stride;
  }
}

static void convolve_vert(const uint8_t *src, ptrdiff_t src_stride,
                          uint8_t *dst, ptrdiff_t dst_stride,
                          const InterpKernel *y_filters, int y0_q4,
                          int y_step_q4, int w, int h) {
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

static void convolve_vert_scale_c(const uint8_t *src, ptrdiff_t src_stride,
                                  uint8_t *dst, ptrdiff_t dst_stride,
                                  const InterpKernel *y_filters, int y0_qn,
                                  int y_step_qn, int w, int h) {
  int x, y;
  src -= src_stride * (SUBPEL_TAPS / 2 - 1);

  for (x = 0; x < w; ++x) {
    int y_qn = y0_qn;
    for (y = 0; y < h; ++y) {
      const unsigned char *src_y =
          &src[(y_qn >> SCALE_SUBPEL_BITS) * src_stride];
      const int16_t *const y_filter =
          y_filters[(y_qn & SCALE_SUBPEL_MASK) >> SCALE_EXTRA_BITS];
      int k, sum = 0;
      for (k = 0; k < SUBPEL_TAPS; ++k)
        sum += src_y[k * src_stride] * y_filter[k];
      dst[y * dst_stride] = clip_pixel(ROUND_POWER_OF_TWO(sum, FILTER_BITS));
      y_qn += y_step_qn;
    }
    ++src;
    ++dst;
  }
}

static void convolve_avg_vert(const uint8_t *src, ptrdiff_t src_stride,
                              uint8_t *dst, ptrdiff_t dst_stride,
                              const InterpKernel *y_filters, int y0_q4,
                              int y_step_q4, int w, int h) {
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
      dst[y * dst_stride] = ROUND_POWER_OF_TWO(
          dst[y * dst_stride] +
              clip_pixel(ROUND_POWER_OF_TWO(sum, FILTER_BITS)),
          1);
      y_q4 += y_step_q4;
    }
    ++src;
    ++dst;
  }
}

static void convolve_avg_vert_scale_c(const uint8_t *src, ptrdiff_t src_stride,
                                      uint8_t *dst, ptrdiff_t dst_stride,
                                      const InterpKernel *y_filters, int y0_qn,
                                      int y_step_qn, int w, int h) {
  int x, y;
  src -= src_stride * (SUBPEL_TAPS / 2 - 1);

  for (x = 0; x < w; ++x) {
    int y_qn = y0_qn;
    for (y = 0; y < h; ++y) {
      const unsigned char *src_y =
          &src[(y_qn >> SCALE_SUBPEL_BITS) * src_stride];
      const int16_t *const y_filter =
          y_filters[(y_qn & SCALE_SUBPEL_MASK) >> SCALE_EXTRA_BITS];
      int k, sum = 0;
      for (k = 0; k < SUBPEL_TAPS; ++k)
        sum += src_y[k * src_stride] * y_filter[k];
      dst[y * dst_stride] = ROUND_POWER_OF_TWO(
          dst[y * dst_stride] +
              clip_pixel(ROUND_POWER_OF_TWO(sum, FILTER_BITS)),
          1);
      y_qn += y_step_qn;
    }
    ++src;
    ++dst;
  }
}

static void convolve(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst,
                     ptrdiff_t dst_stride, const InterpKernel *const x_filters,
                     int x0_q4, int x_step_q4,
                     const InterpKernel *const y_filters, int y0_q4,
                     int y_step_q4, int w, int h) {
  // Note: Fixed size intermediate buffer, temp, places limits on parameters.
  // 2d filtering proceeds in 2 steps:
  //   (1) Interpolate horizontally into an intermediate buffer, temp.
  //   (2) Interpolate temp vertically to derive the sub-pixel result.
  // Deriving the maximum number of rows in the temp buffer (135):
  // --Smallest scaling factor is x1/2 ==> y_step_q4 = 32 (Normative).
  // --Largest block size is 64x64 pixels.
  // --64 rows in the downscaled frame span a distance of (64 - 1) * 32 in the
  //   original frame (in 1/16th pixel units).
  // --Must round-up because block may be located at sub-pixel position.
  // --Require an additional SUBPEL_TAPS rows for the 8-tap filter tails.
  // --((64 - 1) * 32 + 15) >> 4 + 8 = 135.
  uint8_t temp[MAX_EXT_SIZE * MAX_SB_SIZE];
  int intermediate_height =
      (((h - 1) * y_step_q4 + y0_q4) >> SUBPEL_BITS) + SUBPEL_TAPS;

  assert(w <= MAX_SB_SIZE);
  assert(h <= MAX_SB_SIZE);

  assert(y_step_q4 <= 32);
  assert(x_step_q4 <= 32);

  convolve_horiz(src - src_stride * (SUBPEL_TAPS / 2 - 1), src_stride, temp,
                 MAX_SB_SIZE, x_filters, x0_q4, x_step_q4, w,
                 intermediate_height);
  convolve_vert(temp + MAX_SB_SIZE * (SUBPEL_TAPS / 2 - 1), MAX_SB_SIZE, dst,
                dst_stride, y_filters, y0_q4, y_step_q4, w, h);
}

static void convolve_scale_c(const uint8_t *src, ptrdiff_t src_stride,
                             uint8_t *dst, ptrdiff_t dst_stride,
                             const InterpKernel *const x_filters, int x0_qn,
                             int x_step_qn, const InterpKernel *const y_filters,
                             int y0_qn, int y_step_qn, int w, int h) {
  // TODO(afergs): Update comment here
  // Note: Fixed size intermediate buffer, temp, places limits on parameters.
  // 2d filtering proceeds in 2 steps:
  //   (1) Interpolate horizontally into an intermediate buffer, temp.
  //   (2) Interpolate temp vertically to derive the sub-pixel result.
  // Deriving the maximum number of rows in the temp buffer (135):
  // --Smallest scaling factor is x1/2 ==> y_step_qn = 32 (Normative).
  // --Largest block size is 64x64 pixels.
  // --64 rows in the downscaled frame span a distance of (64 - 1) * 32 in the
  //   original frame (in 1/16th pixel units).
  // --Must round-up because block may be located at sub-pixel position.
  // --Require an additional SUBPEL_TAPS rows for the 8-tap filter tails.
  // --((64 - 1) * 32 + 15) >> 4 + 8 = 135.
  uint8_t temp[MAX_EXT_SIZE * MAX_SB_SIZE];
  int intermediate_height =
      (((h - 1) * y_step_qn + y0_qn) >> SCALE_SUBPEL_BITS) + SUBPEL_TAPS;

  assert(w <= MAX_SB_SIZE);
  assert(h <= MAX_SB_SIZE);

  assert(y_step_qn <= SCALE_SUBPEL_BITS * 2);
  assert(x_step_qn <= SCALE_SUBPEL_BITS * 2);

  convolve_horiz_scale_c(src - src_stride * (SUBPEL_TAPS / 2 - 1), src_stride,
                         temp, MAX_SB_SIZE, x_filters, x0_qn, x_step_qn, w,
                         intermediate_height);
  convolve_vert_scale_c(temp + MAX_SB_SIZE * (SUBPEL_TAPS / 2 - 1), MAX_SB_SIZE,
                        dst, dst_stride, y_filters, y0_qn, y_step_qn, w, h);
}

static const InterpKernel *get_filter_base(const int16_t *filter) {
  // NOTE: This assumes that the filter table is 256-byte aligned.
  // TODO(agrange) Modify to make independent of table alignment.
  return (const InterpKernel *)(((intptr_t)filter) & ~((intptr_t)0xFF));
}

static int get_filter_offset(const int16_t *f, const InterpKernel *base) {
  return (int)((const InterpKernel *)(intptr_t)f - base);
}

void aom_convolve8_horiz_c(const uint8_t *src, ptrdiff_t src_stride,
                           uint8_t *dst, ptrdiff_t dst_stride,
                           const int16_t *filter_x, int x_step_q4,
                           const int16_t *filter_y, int y_step_q4, int w,
                           int h) {
  const InterpKernel *const filters_x = get_filter_base(filter_x);
  const int x0_q4 = get_filter_offset(filter_x, filters_x);

  (void)filter_y;
  (void)y_step_q4;

  convolve_horiz(src, src_stride, dst, dst_stride, filters_x, x0_q4, x_step_q4,
                 w, h);
}

void aom_convolve8_horiz_scale_c(const uint8_t *src, ptrdiff_t src_stride,
                                 uint8_t *dst, ptrdiff_t dst_stride,
                                 const int16_t *filter_x, int subpel_x,
                                 int x_step_qn, const int16_t *filter_y,
                                 int subpel_y, int y_step_qn, int w, int h) {
  const InterpKernel *const filters_x = get_filter_base(filter_x);

  (void)subpel_y;
  (void)filter_y;
  (void)y_step_qn;

  convolve_horiz_scale_c(src, src_stride, dst, dst_stride, filters_x, subpel_x,
                         x_step_qn, w, h);
}

void aom_convolve8_avg_horiz_c(const uint8_t *src, ptrdiff_t src_stride,
                               uint8_t *dst, ptrdiff_t dst_stride,
                               const int16_t *filter_x, int x_step_q4,
                               const int16_t *filter_y, int y_step_q4, int w,
                               int h) {
  const InterpKernel *const filters_x = get_filter_base(filter_x);
  const int x0_q4 = get_filter_offset(filter_x, filters_x);

  (void)filter_y;
  (void)y_step_q4;

  convolve_avg_horiz(src, src_stride, dst, dst_stride, filters_x, x0_q4,
                     x_step_q4, w, h);
}

void aom_convolve8_avg_horiz_scale_c(const uint8_t *src, ptrdiff_t src_stride,
                                     uint8_t *dst, ptrdiff_t dst_stride,
                                     const int16_t *filter_x, int subpel_x,
                                     int x_step_qn, const int16_t *filter_y,
                                     int subpel_y, int y_step_qn, int w,
                                     int h) {
  const InterpKernel *const filters_x = get_filter_base(filter_x);

  (void)subpel_y;
  (void)filter_y;
  (void)y_step_qn;

  convolve_avg_horiz_scale_c(src, src_stride, dst, dst_stride, filters_x,
                             subpel_x, x_step_qn, w, h);
}

void aom_convolve8_vert_c(const uint8_t *src, ptrdiff_t src_stride,
                          uint8_t *dst, ptrdiff_t dst_stride,
                          const int16_t *filter_x, int x_step_q4,
                          const int16_t *filter_y, int y_step_q4, int w,
                          int h) {
  const InterpKernel *const filters_y = get_filter_base(filter_y);
  const int y0_q4 = get_filter_offset(filter_y, filters_y);

  (void)filter_x;
  (void)x_step_q4;

  convolve_vert(src, src_stride, dst, dst_stride, filters_y, y0_q4, y_step_q4,
                w, h);
}

void aom_convolve8_vert_scale_c(const uint8_t *src, ptrdiff_t src_stride,
                                uint8_t *dst, ptrdiff_t dst_stride,
                                const int16_t *filter_x, int subpel_x,
                                int x_step_qn, const int16_t *filter_y,
                                int subpel_y, int y_step_qn, int w, int h) {
  const InterpKernel *const filters_y = get_filter_base(filter_y);

  (void)subpel_x;
  (void)filter_x;
  (void)x_step_qn;

  convolve_vert_scale_c(src, src_stride, dst, dst_stride, filters_y, subpel_y,
                        y_step_qn, w, h);
}

void aom_convolve8_avg_vert_c(const uint8_t *src, ptrdiff_t src_stride,
                              uint8_t *dst, ptrdiff_t dst_stride,
                              const int16_t *filter_x, int x_step_q4,
                              const int16_t *filter_y, int y_step_q4, int w,
                              int h) {
  const InterpKernel *const filters_y = get_filter_base(filter_y);
  const int y0_q4 = get_filter_offset(filter_y, filters_y);

  (void)filter_x;
  (void)x_step_q4;

  convolve_avg_vert(src, src_stride, dst, dst_stride, filters_y, y0_q4,
                    y_step_q4, w, h);
}

void aom_convolve8_avg_vert_scale_c(const uint8_t *src, ptrdiff_t src_stride,
                                    uint8_t *dst, ptrdiff_t dst_stride,
                                    const int16_t *filter_x, int subpel_x,
                                    int x_step_qn, const int16_t *filter_y,
                                    int subpel_y, int y_step_qn, int w, int h) {
  const InterpKernel *const filters_y = get_filter_base(filter_y);

  (void)subpel_x;
  (void)filter_x;
  (void)x_step_qn;

  convolve_avg_vert_scale_c(src, src_stride, dst, dst_stride, filters_y,
                            subpel_y, y_step_qn, w, h);
}

void aom_convolve8_c(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst,
                     ptrdiff_t dst_stride, const int16_t *filter_x,
                     int x_step_q4, const int16_t *filter_y, int y_step_q4,
                     int w, int h) {
  const InterpKernel *const filters_x = get_filter_base(filter_x);
  const int x0_q4 = get_filter_offset(filter_x, filters_x);

  const InterpKernel *const filters_y = get_filter_base(filter_y);
  const int y0_q4 = get_filter_offset(filter_y, filters_y);

  convolve(src, src_stride, dst, dst_stride, filters_x, x0_q4, x_step_q4,
           filters_y, y0_q4, y_step_q4, w, h);
}

void aom_convolve8_scale_c(const uint8_t *src, ptrdiff_t src_stride,
                           uint8_t *dst, ptrdiff_t dst_stride,
                           const int16_t *filter_x, int subpel_x, int x_step_qn,
                           const int16_t *filter_y, int subpel_y, int y_step_qn,
                           int w, int h) {
  const InterpKernel *const filters_x = get_filter_base(filter_x);

  const InterpKernel *const filters_y = get_filter_base(filter_y);

  convolve_scale_c(src, src_stride, dst, dst_stride, filters_x, subpel_x,
                   x_step_qn, filters_y, subpel_y, y_step_qn, w, h);
}

void aom_convolve8_avg_c(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst,
                         ptrdiff_t dst_stride, const int16_t *filter_x,
                         int x_step_q4, const int16_t *filter_y, int y_step_q4,
                         int w, int h) {
  /* Fixed size intermediate buffer places limits on parameters. */
  DECLARE_ALIGNED(16, uint8_t, temp[MAX_SB_SIZE * MAX_SB_SIZE]);
  assert(w <= MAX_SB_SIZE);
  assert(h <= MAX_SB_SIZE);

  aom_convolve8_c(src, src_stride, temp, MAX_SB_SIZE, filter_x, x_step_q4,
                  filter_y, y_step_q4, w, h);
  aom_convolve_avg_c(temp, MAX_SB_SIZE, dst, dst_stride, NULL, 0, NULL, 0, w,
                     h);
}

void aom_convolve8_avg_scale_c(const uint8_t *src, ptrdiff_t src_stride,
                               uint8_t *dst, ptrdiff_t dst_stride,
                               const int16_t *filter_x, int subpel_x,
                               int x_step_qn, const int16_t *filter_y,
                               int subpel_y, int y_step_qn, int w, int h) {
  /* Fixed size intermediate buffer places limits on parameters. */
  DECLARE_ALIGNED(16, uint8_t, temp[MAX_SB_SIZE * MAX_SB_SIZE]);
  assert(w <= MAX_SB_SIZE);
  assert(h <= MAX_SB_SIZE);

  aom_convolve8_scale_c(src, src_stride, temp, MAX_SB_SIZE, filter_x, subpel_x,
                        x_step_qn, filter_y, subpel_y, y_step_qn, w, h);
  aom_convolve_avg_c(temp, MAX_SB_SIZE, dst, dst_stride, NULL, 0, NULL, 0, w,
                     h);
}

void aom_convolve_copy_c(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst,
                         ptrdiff_t dst_stride, const int16_t *filter_x,
                         int filter_x_stride, const int16_t *filter_y,
                         int filter_y_stride, int w, int h) {
  int r;

  (void)filter_x;
  (void)filter_x_stride;
  (void)filter_y;
  (void)filter_y_stride;

  for (r = h; r > 0; --r) {
    memcpy(dst, src, w);
    src += src_stride;
    dst += dst_stride;
  }
}

void aom_convolve_avg_c(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst,
                        ptrdiff_t dst_stride, const int16_t *filter_x,
                        int filter_x_stride, const int16_t *filter_y,
                        int filter_y_stride, int w, int h) {
  int x, y;

  (void)filter_x;
  (void)filter_x_stride;
  (void)filter_y;
  (void)filter_y_stride;

  for (y = 0; y < h; ++y) {
    for (x = 0; x < w; ++x) dst[x] = ROUND_POWER_OF_TWO(dst[x] + src[x], 1);

    src += src_stride;
    dst += dst_stride;
  }
}

void aom_scaled_horiz_c(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst,
                        ptrdiff_t dst_stride, const int16_t *filter_x,
                        int x_step_q4, const int16_t *filter_y, int y_step_q4,
                        int w, int h) {
  aom_convolve8_horiz_c(src, src_stride, dst, dst_stride, filter_x, x_step_q4,
                        filter_y, y_step_q4, w, h);
}

void aom_scaled_vert_c(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst,
                       ptrdiff_t dst_stride, const int16_t *filter_x,
                       int x_step_q4, const int16_t *filter_y, int y_step_q4,
                       int w, int h) {
  aom_convolve8_vert_c(src, src_stride, dst, dst_stride, filter_x, x_step_q4,
                       filter_y, y_step_q4, w, h);
}

void aom_scaled_2d_c(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst,
                     ptrdiff_t dst_stride, const int16_t *filter_x,
                     int x_step_q4, const int16_t *filter_y, int y_step_q4,
                     int w, int h) {
  aom_convolve8_c(src, src_stride, dst, dst_stride, filter_x, x_step_q4,
                  filter_y, y_step_q4, w, h);
}

void aom_scaled_avg_horiz_c(const uint8_t *src, ptrdiff_t src_stride,
                            uint8_t *dst, ptrdiff_t dst_stride,
                            const int16_t *filter_x, int x_step_q4,
                            const int16_t *filter_y, int y_step_q4, int w,
                            int h) {
  aom_convolve8_avg_horiz_c(src, src_stride, dst, dst_stride, filter_x,
                            x_step_q4, filter_y, y_step_q4, w, h);
}

void aom_scaled_avg_vert_c(const uint8_t *src, ptrdiff_t src_stride,
                           uint8_t *dst, ptrdiff_t dst_stride,
                           const int16_t *filter_x, int x_step_q4,
                           const int16_t *filter_y, int y_step_q4, int w,
                           int h) {
  aom_convolve8_avg_vert_c(src, src_stride, dst, dst_stride, filter_x,
                           x_step_q4, filter_y, y_step_q4, w, h);
}

void aom_scaled_avg_2d_c(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst,
                         ptrdiff_t dst_stride, const int16_t *filter_x,
                         int x_step_q4, const int16_t *filter_y, int y_step_q4,
                         int w, int h) {
  aom_convolve8_avg_c(src, src_stride, dst, dst_stride, filter_x, x_step_q4,
                      filter_y, y_step_q4, w, h);
}

// TODO(afergs): Make sure this works too
#if CONFIG_LOOP_RESTORATION
static void convolve_add_src_horiz(const uint8_t *src, ptrdiff_t src_stride,
                                   uint8_t *dst, ptrdiff_t dst_stride,
                                   const InterpKernel *x_filters, int x0_q4,
                                   int x_step_q4, int w, int h) {
  int x, y, k;
  src -= SUBPEL_TAPS / 2 - 1;
  for (y = 0; y < h; ++y) {
    int x_q4 = x0_q4;
    for (x = 0; x < w; ++x) {
      const uint8_t *const src_x = &src[x_q4 >> SUBPEL_BITS];
      const int16_t *const x_filter = x_filters[x_q4 & SUBPEL_MASK];
      int sum = 0;
      for (k = 0; k < SUBPEL_TAPS; ++k) sum += src_x[k] * x_filter[k];
      dst[x] = clip_pixel(ROUND_POWER_OF_TWO(sum, FILTER_BITS) +
                          src_x[SUBPEL_TAPS / 2 - 1]);
      x_q4 += x_step_q4;
    }
    src += src_stride;
    dst += dst_stride;
  }
}

static void convolve_add_src_vert(const uint8_t *src, ptrdiff_t src_stride,
                                  uint8_t *dst, ptrdiff_t dst_stride,
                                  const InterpKernel *y_filters, int y0_q4,
                                  int y_step_q4, int w, int h) {
  int x, y, k;
  src -= src_stride * (SUBPEL_TAPS / 2 - 1);

  for (x = 0; x < w; ++x) {
    int y_q4 = y0_q4;
    for (y = 0; y < h; ++y) {
      const unsigned char *src_y = &src[(y_q4 >> SUBPEL_BITS) * src_stride];
      const int16_t *const y_filter = y_filters[y_q4 & SUBPEL_MASK];
      int sum = 0;
      for (k = 0; k < SUBPEL_TAPS; ++k)
        sum += src_y[k * src_stride] * y_filter[k];
      dst[y * dst_stride] =
          clip_pixel(ROUND_POWER_OF_TWO(sum, FILTER_BITS) +
                     src_y[(SUBPEL_TAPS / 2 - 1) * src_stride]);
      y_q4 += y_step_q4;
    }
    ++src;
    ++dst;
  }
}

static void convolve_add_src(const uint8_t *src, ptrdiff_t src_stride,
                             uint8_t *dst, ptrdiff_t dst_stride,
                             const InterpKernel *const x_filters, int x0_q4,
                             int x_step_q4, const InterpKernel *const y_filters,
                             int y0_q4, int y_step_q4, int w, int h) {
  uint8_t temp[MAX_EXT_SIZE * MAX_SB_SIZE];
  int intermediate_height =
      (((h - 1) * y_step_q4 + y0_q4) >> SUBPEL_BITS) + SUBPEL_TAPS;

  assert(w <= MAX_SB_SIZE);
  assert(h <= MAX_SB_SIZE);

  assert(y_step_q4 <= 32);
  assert(x_step_q4 <= 32);

  convolve_add_src_horiz(src - src_stride * (SUBPEL_TAPS / 2 - 1), src_stride,
                         temp, MAX_SB_SIZE, x_filters, x0_q4, x_step_q4, w,
                         intermediate_height);
  convolve_add_src_vert(temp + MAX_SB_SIZE * (SUBPEL_TAPS / 2 - 1), MAX_SB_SIZE,
                        dst, dst_stride, y_filters, y0_q4, y_step_q4, w, h);
}

void aom_convolve8_add_src_horiz_c(const uint8_t *src, ptrdiff_t src_stride,
                                   uint8_t *dst, ptrdiff_t dst_stride,
                                   const int16_t *filter_x, int x_step_q4,
                                   const int16_t *filter_y, int y_step_q4,
                                   int w, int h) {
  const InterpKernel *const filters_x = get_filter_base(filter_x);
  const int x0_q4 = get_filter_offset(filter_x, filters_x);

  (void)filter_y;
  (void)y_step_q4;

  convolve_add_src_horiz(src, src_stride, dst, dst_stride, filters_x, x0_q4,
                         x_step_q4, w, h);
}

void aom_convolve8_add_src_vert_c(const uint8_t *src, ptrdiff_t src_stride,
                                  uint8_t *dst, ptrdiff_t dst_stride,
                                  const int16_t *filter_x, int x_step_q4,
                                  const int16_t *filter_y, int y_step_q4, int w,
                                  int h) {
  const InterpKernel *const filters_y = get_filter_base(filter_y);
  const int y0_q4 = get_filter_offset(filter_y, filters_y);

  (void)filter_x;
  (void)x_step_q4;

  convolve_add_src_vert(src, src_stride, dst, dst_stride, filters_y, y0_q4,
                        y_step_q4, w, h);
}

void aom_convolve8_add_src_c(const uint8_t *src, ptrdiff_t src_stride,
                             uint8_t *dst, ptrdiff_t dst_stride,
                             const int16_t *filter_x, int x_step_q4,
                             const int16_t *filter_y, int y_step_q4, int w,
                             int h) {
  const InterpKernel *const filters_x = get_filter_base(filter_x);
  const int x0_q4 = get_filter_offset(filter_x, filters_x);

  const InterpKernel *const filters_y = get_filter_base(filter_y);
  const int y0_q4 = get_filter_offset(filter_y, filters_y);

  convolve_add_src(src, src_stride, dst, dst_stride, filters_x, x0_q4,
                   x_step_q4, filters_y, y0_q4, y_step_q4, w, h);
}

static void convolve_add_src_horiz_hip(const uint8_t *src, ptrdiff_t src_stride,
                                       uint16_t *dst, ptrdiff_t dst_stride,
                                       const InterpKernel *x_filters, int x0_q4,
                                       int x_step_q4, int w, int h) {
  const int bd = 8;
  int x, y, k;
  src -= SUBPEL_TAPS / 2 - 1;
  for (y = 0; y < h; ++y) {
    int x_q4 = x0_q4;
    for (x = 0; x < w; ++x) {
      const uint8_t *const src_x = &src[x_q4 >> SUBPEL_BITS];
      const int16_t *const x_filter = x_filters[x_q4 & SUBPEL_MASK];
      int sum = ((int)src_x[SUBPEL_TAPS / 2 - 1] << FILTER_BITS) +
                (1 << (bd + FILTER_BITS - 1));
      for (k = 0; k < SUBPEL_TAPS; ++k) sum += src_x[k] * x_filter[k];
      dst[x] =
          (uint16_t)clamp(ROUND_POWER_OF_TWO(sum, FILTER_BITS - EXTRAPREC_BITS),
                          0, EXTRAPREC_CLAMP_LIMIT(bd) - 1);
      x_q4 += x_step_q4;
    }
    src += src_stride;
    dst += dst_stride;
  }
}

static void convolve_add_src_vert_hip(const uint16_t *src, ptrdiff_t src_stride,
                                      uint8_t *dst, ptrdiff_t dst_stride,
                                      const InterpKernel *y_filters, int y0_q4,
                                      int y_step_q4, int w, int h) {
  const int bd = 8;
  int x, y, k;
  src -= src_stride * (SUBPEL_TAPS / 2 - 1);

  for (x = 0; x < w; ++x) {
    int y_q4 = y0_q4;
    for (y = 0; y < h; ++y) {
      const uint16_t *src_y = &src[(y_q4 >> SUBPEL_BITS) * src_stride];
      const int16_t *const y_filter = y_filters[y_q4 & SUBPEL_MASK];
      int sum =
          ((int)src_y[(SUBPEL_TAPS / 2 - 1) * src_stride] << FILTER_BITS) -
          (1 << (bd + FILTER_BITS + EXTRAPREC_BITS - 1));
      for (k = 0; k < SUBPEL_TAPS; ++k)
        sum += src_y[k * src_stride] * y_filter[k];
      dst[y * dst_stride] =
          clip_pixel(ROUND_POWER_OF_TWO(sum, FILTER_BITS + EXTRAPREC_BITS));
      y_q4 += y_step_q4;
    }
    ++src;
    ++dst;
  }
}

static void convolve_add_src_hip(const uint8_t *src, ptrdiff_t src_stride,
                                 uint8_t *dst, ptrdiff_t dst_stride,
                                 const InterpKernel *const x_filters, int x0_q4,
                                 int x_step_q4,
                                 const InterpKernel *const y_filters, int y0_q4,
                                 int y_step_q4, int w, int h) {
  uint16_t temp[MAX_EXT_SIZE * MAX_SB_SIZE];
  int intermediate_height =
      (((h - 1) * y_step_q4 + y0_q4) >> SUBPEL_BITS) + SUBPEL_TAPS;

  assert(w <= MAX_SB_SIZE);
  assert(h <= MAX_SB_SIZE);

  assert(y_step_q4 <= 32);
  assert(x_step_q4 <= 32);

  convolve_add_src_horiz_hip(src - src_stride * (SUBPEL_TAPS / 2 - 1),
                             src_stride, temp, MAX_SB_SIZE, x_filters, x0_q4,
                             x_step_q4, w, intermediate_height);
  convolve_add_src_vert_hip(temp + MAX_SB_SIZE * (SUBPEL_TAPS / 2 - 1),
                            MAX_SB_SIZE, dst, dst_stride, y_filters, y0_q4,
                            y_step_q4, w, h);
}

void aom_convolve8_add_src_horiz_hip_c(const uint8_t *src, ptrdiff_t src_stride,
                                       uint16_t *dst, ptrdiff_t dst_stride,
                                       const int16_t *filter_x, int x_step_q4,
                                       const int16_t *filter_y, int y_step_q4,
                                       int w, int h) {
  const InterpKernel *const filters_x = get_filter_base(filter_x);
  const int x0_q4 = get_filter_offset(filter_x, filters_x);

  (void)filter_y;
  (void)y_step_q4;

  convolve_add_src_horiz_hip(src, src_stride, dst, dst_stride, filters_x, x0_q4,
                             x_step_q4, w, h);
}

void aom_convolve8_add_src_vert_hip_c(const uint16_t *src, ptrdiff_t src_stride,
                                      uint8_t *dst, ptrdiff_t dst_stride,
                                      const int16_t *filter_x, int x_step_q4,
                                      const int16_t *filter_y, int y_step_q4,
                                      int w, int h) {
  const InterpKernel *const filters_y = get_filter_base(filter_y);
  const int y0_q4 = get_filter_offset(filter_y, filters_y);

  (void)filter_x;
  (void)x_step_q4;

  convolve_add_src_vert_hip(src, src_stride, dst, dst_stride, filters_y, y0_q4,
                            y_step_q4, w, h);
}

void aom_convolve8_add_src_hip_c(const uint8_t *src, ptrdiff_t src_stride,
                                 uint8_t *dst, ptrdiff_t dst_stride,
                                 const int16_t *filter_x, int x_step_q4,
                                 const int16_t *filter_y, int y_step_q4, int w,
                                 int h) {
  const InterpKernel *const filters_x = get_filter_base(filter_x);
  const int x0_q4 = get_filter_offset(filter_x, filters_x);

  const InterpKernel *const filters_y = get_filter_base(filter_y);
  const int y0_q4 = get_filter_offset(filter_y, filters_y);

  convolve_add_src_hip(src, src_stride, dst, dst_stride, filters_x, x0_q4,
                       x_step_q4, filters_y, y0_q4, y_step_q4, w, h);
}
#endif  // CONFIG_LOOP_RESTORATION

// TODO(afergs): Make sure this works too
#if CONFIG_HIGHBITDEPTH
static void highbd_convolve_horiz(const uint8_t *src8, ptrdiff_t src_stride,
                                  uint8_t *dst8, ptrdiff_t dst_stride,
                                  const InterpKernel *x_filters, int x0_q4,
                                  int x_step_q4, int w, int h, int bd) {
  int x, y;
  uint16_t *src = CONVERT_TO_SHORTPTR(src8);
  uint16_t *dst = CONVERT_TO_SHORTPTR(dst8);
  src -= SUBPEL_TAPS / 2 - 1;
  for (y = 0; y < h; ++y) {
    int x_q4 = x0_q4;
    for (x = 0; x < w; ++x) {
      const uint16_t *const src_x = &src[x_q4 >> SUBPEL_BITS];
      const int16_t *const x_filter = x_filters[x_q4 & SUBPEL_MASK];
      int k, sum = 0;
      for (k = 0; k < SUBPEL_TAPS; ++k) sum += src_x[k] * x_filter[k];
      dst[x] = clip_pixel_highbd(ROUND_POWER_OF_TWO(sum, FILTER_BITS), bd);
      x_q4 += x_step_q4;
    }
    src += src_stride;
    dst += dst_stride;
  }
}

static void highbd_convolve_avg_horiz(const uint8_t *src8, ptrdiff_t src_stride,
                                      uint8_t *dst8, ptrdiff_t dst_stride,
                                      const InterpKernel *x_filters, int x0_q4,
                                      int x_step_q4, int w, int h, int bd) {
  int x, y;
  uint16_t *src = CONVERT_TO_SHORTPTR(src8);
  uint16_t *dst = CONVERT_TO_SHORTPTR(dst8);
  src -= SUBPEL_TAPS / 2 - 1;
  for (y = 0; y < h; ++y) {
    int x_q4 = x0_q4;
    for (x = 0; x < w; ++x) {
      const uint16_t *const src_x = &src[x_q4 >> SUBPEL_BITS];
      const int16_t *const x_filter = x_filters[x_q4 & SUBPEL_MASK];
      int k, sum = 0;
      for (k = 0; k < SUBPEL_TAPS; ++k) sum += src_x[k] * x_filter[k];
      dst[x] = ROUND_POWER_OF_TWO(
          dst[x] + clip_pixel_highbd(ROUND_POWER_OF_TWO(sum, FILTER_BITS), bd),
          1);
      x_q4 += x_step_q4;
    }
    src += src_stride;
    dst += dst_stride;
  }
}

static void highbd_convolve_vert(const uint8_t *src8, ptrdiff_t src_stride,
                                 uint8_t *dst8, ptrdiff_t dst_stride,
                                 const InterpKernel *y_filters, int y0_q4,
                                 int y_step_q4, int w, int h, int bd) {
  int x, y;
  uint16_t *src = CONVERT_TO_SHORTPTR(src8);
  uint16_t *dst = CONVERT_TO_SHORTPTR(dst8);
  src -= src_stride * (SUBPEL_TAPS / 2 - 1);
  for (x = 0; x < w; ++x) {
    int y_q4 = y0_q4;
    for (y = 0; y < h; ++y) {
      const uint16_t *src_y = &src[(y_q4 >> SUBPEL_BITS) * src_stride];
      const int16_t *const y_filter = y_filters[y_q4 & SUBPEL_MASK];
      int k, sum = 0;
      for (k = 0; k < SUBPEL_TAPS; ++k)
        sum += src_y[k * src_stride] * y_filter[k];
      dst[y * dst_stride] =
          clip_pixel_highbd(ROUND_POWER_OF_TWO(sum, FILTER_BITS), bd);
      y_q4 += y_step_q4;
    }
    ++src;
    ++dst;
  }
}

static void highbd_convolve_avg_vert(const uint8_t *src8, ptrdiff_t src_stride,
                                     uint8_t *dst8, ptrdiff_t dst_stride,
                                     const InterpKernel *y_filters, int y0_q4,
                                     int y_step_q4, int w, int h, int bd) {
  int x, y;
  uint16_t *src = CONVERT_TO_SHORTPTR(src8);
  uint16_t *dst = CONVERT_TO_SHORTPTR(dst8);
  src -= src_stride * (SUBPEL_TAPS / 2 - 1);
  for (x = 0; x < w; ++x) {
    int y_q4 = y0_q4;
    for (y = 0; y < h; ++y) {
      const uint16_t *src_y = &src[(y_q4 >> SUBPEL_BITS) * src_stride];
      const int16_t *const y_filter = y_filters[y_q4 & SUBPEL_MASK];
      int k, sum = 0;
      for (k = 0; k < SUBPEL_TAPS; ++k)
        sum += src_y[k * src_stride] * y_filter[k];
      dst[y * dst_stride] = ROUND_POWER_OF_TWO(
          dst[y * dst_stride] +
              clip_pixel_highbd(ROUND_POWER_OF_TWO(sum, FILTER_BITS), bd),
          1);
      y_q4 += y_step_q4;
    }
    ++src;
    ++dst;
  }
}

static void highbd_convolve(const uint8_t *src, ptrdiff_t src_stride,
                            uint8_t *dst, ptrdiff_t dst_stride,
                            const InterpKernel *const x_filters, int x0_q4,
                            int x_step_q4, const InterpKernel *const y_filters,
                            int y0_q4, int y_step_q4, int w, int h, int bd) {
  // Note: Fixed size intermediate buffer, temp, places limits on parameters.
  // 2d filtering proceeds in 2 steps:
  //   (1) Interpolate horizontally into an intermediate buffer, temp.
  //   (2) Interpolate temp vertically to derive the sub-pixel result.
  // Deriving the maximum number of rows in the temp buffer (135):
  // --Smallest scaling factor is x1/2 ==> y_step_q4 = 32 (Normative).
  // --Largest block size is 64x64 pixels.
  // --64 rows in the downscaled frame span a distance of (64 - 1) * 32 in the
  //   original frame (in 1/16th pixel units).
  // --Must round-up because block may be located at sub-pixel position.
  // --Require an additional SUBPEL_TAPS rows for the 8-tap filter tails.
  // --((64 - 1) * 32 + 15) >> 4 + 8 = 135.
  uint16_t temp[MAX_EXT_SIZE * MAX_SB_SIZE];
  int intermediate_height =
      (((h - 1) * y_step_q4 + y0_q4) >> SUBPEL_BITS) + SUBPEL_TAPS;

  assert(w <= MAX_SB_SIZE);
  assert(h <= MAX_SB_SIZE);
  assert(y_step_q4 <= 32);
  assert(x_step_q4 <= 32);

  highbd_convolve_horiz(src - src_stride * (SUBPEL_TAPS / 2 - 1), src_stride,
                        CONVERT_TO_BYTEPTR(temp), MAX_SB_SIZE, x_filters, x0_q4,
                        x_step_q4, w, intermediate_height, bd);
  highbd_convolve_vert(
      CONVERT_TO_BYTEPTR(temp) + MAX_SB_SIZE * (SUBPEL_TAPS / 2 - 1),
      MAX_SB_SIZE, dst, dst_stride, y_filters, y0_q4, y_step_q4, w, h, bd);
}

void aom_highbd_convolve8_horiz_c(const uint8_t *src, ptrdiff_t src_stride,
                                  uint8_t *dst, ptrdiff_t dst_stride,
                                  const int16_t *filter_x, int x_step_q4,
                                  const int16_t *filter_y, int y_step_q4, int w,
                                  int h, int bd) {
  const InterpKernel *const filters_x = get_filter_base(filter_x);
  const int x0_q4 = get_filter_offset(filter_x, filters_x);
  (void)filter_y;
  (void)y_step_q4;

  highbd_convolve_horiz(src, src_stride, dst, dst_stride, filters_x, x0_q4,
                        x_step_q4, w, h, bd);
}

void aom_highbd_convolve8_avg_horiz_c(const uint8_t *src, ptrdiff_t src_stride,
                                      uint8_t *dst, ptrdiff_t dst_stride,
                                      const int16_t *filter_x, int x_step_q4,
                                      const int16_t *filter_y, int y_step_q4,
                                      int w, int h, int bd) {
  const InterpKernel *const filters_x = get_filter_base(filter_x);
  const int x0_q4 = get_filter_offset(filter_x, filters_x);
  (void)filter_y;
  (void)y_step_q4;

  highbd_convolve_avg_horiz(src, src_stride, dst, dst_stride, filters_x, x0_q4,
                            x_step_q4, w, h, bd);
}

void aom_highbd_convolve8_vert_c(const uint8_t *src, ptrdiff_t src_stride,
                                 uint8_t *dst, ptrdiff_t dst_stride,
                                 const int16_t *filter_x, int x_step_q4,
                                 const int16_t *filter_y, int y_step_q4, int w,
                                 int h, int bd) {
  const InterpKernel *const filters_y = get_filter_base(filter_y);
  const int y0_q4 = get_filter_offset(filter_y, filters_y);
  (void)filter_x;
  (void)x_step_q4;

  highbd_convolve_vert(src, src_stride, dst, dst_stride, filters_y, y0_q4,
                       y_step_q4, w, h, bd);
}

void aom_highbd_convolve8_avg_vert_c(const uint8_t *src, ptrdiff_t src_stride,
                                     uint8_t *dst, ptrdiff_t dst_stride,
                                     const int16_t *filter_x, int x_step_q4,
                                     const int16_t *filter_y, int y_step_q4,
                                     int w, int h, int bd) {
  const InterpKernel *const filters_y = get_filter_base(filter_y);
  const int y0_q4 = get_filter_offset(filter_y, filters_y);
  (void)filter_x;
  (void)x_step_q4;

  highbd_convolve_avg_vert(src, src_stride, dst, dst_stride, filters_y, y0_q4,
                           y_step_q4, w, h, bd);
}

void aom_highbd_convolve8_c(const uint8_t *src, ptrdiff_t src_stride,
                            uint8_t *dst, ptrdiff_t dst_stride,
                            const int16_t *filter_x, int x_step_q4,
                            const int16_t *filter_y, int y_step_q4, int w,
                            int h, int bd) {
  const InterpKernel *const filters_x = get_filter_base(filter_x);
  const int x0_q4 = get_filter_offset(filter_x, filters_x);

  const InterpKernel *const filters_y = get_filter_base(filter_y);
  const int y0_q4 = get_filter_offset(filter_y, filters_y);

  highbd_convolve(src, src_stride, dst, dst_stride, filters_x, x0_q4, x_step_q4,
                  filters_y, y0_q4, y_step_q4, w, h, bd);
}

void aom_highbd_convolve8_avg_c(const uint8_t *src, ptrdiff_t src_stride,
                                uint8_t *dst, ptrdiff_t dst_stride,
                                const int16_t *filter_x, int x_step_q4,
                                const int16_t *filter_y, int y_step_q4, int w,
                                int h, int bd) {
  // Fixed size intermediate buffer places limits on parameters.
  DECLARE_ALIGNED(16, uint16_t, temp[MAX_SB_SIZE * MAX_SB_SIZE]);
  assert(w <= MAX_SB_SIZE);
  assert(h <= MAX_SB_SIZE);

  aom_highbd_convolve8_c(src, src_stride, CONVERT_TO_BYTEPTR(temp), MAX_SB_SIZE,
                         filter_x, x_step_q4, filter_y, y_step_q4, w, h, bd);
  aom_highbd_convolve_avg_c(CONVERT_TO_BYTEPTR(temp), MAX_SB_SIZE, dst,
                            dst_stride, NULL, 0, NULL, 0, w, h, bd);
}

void aom_highbd_convolve_copy_c(const uint8_t *src8, ptrdiff_t src_stride,
                                uint8_t *dst8, ptrdiff_t dst_stride,
                                const int16_t *filter_x, int filter_x_stride,
                                const int16_t *filter_y, int filter_y_stride,
                                int w, int h, int bd) {
  int r;
  uint16_t *src = CONVERT_TO_SHORTPTR(src8);
  uint16_t *dst = CONVERT_TO_SHORTPTR(dst8);
  (void)filter_x;
  (void)filter_y;
  (void)filter_x_stride;
  (void)filter_y_stride;
  (void)bd;

  for (r = h; r > 0; --r) {
    memcpy(dst, src, w * sizeof(uint16_t));
    src += src_stride;
    dst += dst_stride;
  }
}

void aom_highbd_convolve_avg_c(const uint8_t *src8, ptrdiff_t src_stride,
                               uint8_t *dst8, ptrdiff_t dst_stride,
                               const int16_t *filter_x, int filter_x_stride,
                               const int16_t *filter_y, int filter_y_stride,
                               int w, int h, int bd) {
  int x, y;
  uint16_t *src = CONVERT_TO_SHORTPTR(src8);
  uint16_t *dst = CONVERT_TO_SHORTPTR(dst8);
  (void)filter_x;
  (void)filter_y;
  (void)filter_x_stride;
  (void)filter_y_stride;
  (void)bd;

  for (y = 0; y < h; ++y) {
    for (x = 0; x < w; ++x) {
      dst[x] = ROUND_POWER_OF_TWO(dst[x] + src[x], 1);
    }
    src += src_stride;
    dst += dst_stride;
  }
}

#if CONFIG_LOOP_RESTORATION
static void highbd_convolve_add_src_horiz(const uint8_t *src8,
                                          ptrdiff_t src_stride, uint8_t *dst8,
                                          ptrdiff_t dst_stride,
                                          const InterpKernel *x_filters,
                                          int x0_q4, int x_step_q4, int w,
                                          int h, int bd) {
  int x, y, k;
  uint16_t *src = CONVERT_TO_SHORTPTR(src8);
  uint16_t *dst = CONVERT_TO_SHORTPTR(dst8);
  src -= SUBPEL_TAPS / 2 - 1;
  for (y = 0; y < h; ++y) {
    int x_q4 = x0_q4;
    for (x = 0; x < w; ++x) {
      const uint16_t *const src_x = &src[x_q4 >> SUBPEL_BITS];
      const int16_t *const x_filter = x_filters[x_q4 & SUBPEL_MASK];
      int sum = 0;
      for (k = 0; k < SUBPEL_TAPS; ++k) sum += src_x[k] * x_filter[k];
      dst[x] = clip_pixel_highbd(
          ROUND_POWER_OF_TWO(sum, FILTER_BITS) + src_x[SUBPEL_TAPS / 2 - 1],
          bd);
      x_q4 += x_step_q4;
    }
    src += src_stride;
    dst += dst_stride;
  }
}

static void highbd_convolve_add_src_vert(const uint8_t *src8,
                                         ptrdiff_t src_stride, uint8_t *dst8,
                                         ptrdiff_t dst_stride,
                                         const InterpKernel *y_filters,
                                         int y0_q4, int y_step_q4, int w, int h,
                                         int bd) {
  int x, y, k;
  uint16_t *src = CONVERT_TO_SHORTPTR(src8);
  uint16_t *dst = CONVERT_TO_SHORTPTR(dst8);
  src -= src_stride * (SUBPEL_TAPS / 2 - 1);
  for (x = 0; x < w; ++x) {
    int y_q4 = y0_q4;
    for (y = 0; y < h; ++y) {
      const uint16_t *src_y = &src[(y_q4 >> SUBPEL_BITS) * src_stride];
      const int16_t *const y_filter = y_filters[y_q4 & SUBPEL_MASK];
      int sum = 0;
      for (k = 0; k < SUBPEL_TAPS; ++k)
        sum += src_y[k * src_stride] * y_filter[k];
      dst[y * dst_stride] =
          clip_pixel_highbd(ROUND_POWER_OF_TWO(sum, FILTER_BITS) +
                                src_y[(SUBPEL_TAPS / 2 - 1) * src_stride],
                            bd);
      y_q4 += y_step_q4;
    }
    ++src;
    ++dst;
  }
}

static void highbd_convolve_add_src(const uint8_t *src, ptrdiff_t src_stride,
                                    uint8_t *dst, ptrdiff_t dst_stride,
                                    const InterpKernel *const x_filters,
                                    int x0_q4, int x_step_q4,
                                    const InterpKernel *const y_filters,
                                    int y0_q4, int y_step_q4, int w, int h,
                                    int bd) {
  // Note: Fixed size intermediate buffer, temp, places limits on parameters.
  // 2d filtering proceeds in 2 steps:
  //   (1) Interpolate horizontally into an intermediate buffer, temp.
  //   (2) Interpolate temp vertically to derive the sub-pixel result.
  // Deriving the maximum number of rows in the temp buffer (135):
  // --Smallest scaling factor is x1/2 ==> y_step_q4 = 32 (Normative).
  // --Largest block size is 64x64 pixels.
  // --64 rows in the downscaled frame span a distance of (64 - 1) * 32 in the
  //   original frame (in 1/16th pixel units).
  // --Must round-up because block may be located at sub-pixel position.
  // --Require an additional SUBPEL_TAPS rows for the 8-tap filter tails.
  // --((64 - 1) * 32 + 15) >> 4 + 8 = 135.
  uint16_t temp[MAX_EXT_SIZE * MAX_SB_SIZE];
  int intermediate_height =
      (((h - 1) * y_step_q4 + y0_q4) >> SUBPEL_BITS) + SUBPEL_TAPS;

  assert(w <= MAX_SB_SIZE);
  assert(h <= MAX_SB_SIZE);
  assert(y_step_q4 <= 32);
  assert(x_step_q4 <= 32);

  highbd_convolve_add_src_horiz(src - src_stride * (SUBPEL_TAPS / 2 - 1),
                                src_stride, CONVERT_TO_BYTEPTR(temp),
                                MAX_SB_SIZE, x_filters, x0_q4, x_step_q4, w,
                                intermediate_height, bd);
  highbd_convolve_add_src_vert(
      CONVERT_TO_BYTEPTR(temp) + MAX_SB_SIZE * (SUBPEL_TAPS / 2 - 1),
      MAX_SB_SIZE, dst, dst_stride, y_filters, y0_q4, y_step_q4, w, h, bd);
}

void aom_highbd_convolve8_add_src_horiz_c(
    const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst,
    ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4,
    const int16_t *filter_y, int y_step_q4, int w, int h, int bd) {
  const InterpKernel *const filters_x = get_filter_base(filter_x);
  const int x0_q4 = get_filter_offset(filter_x, filters_x);
  (void)filter_y;
  (void)y_step_q4;

  highbd_convolve_add_src_horiz(src, src_stride, dst, dst_stride, filters_x,
                                x0_q4, x_step_q4, w, h, bd);
}

void aom_highbd_convolve8_add_src_vert_c(const uint8_t *src,
                                         ptrdiff_t src_stride, uint8_t *dst,
                                         ptrdiff_t dst_stride,
                                         const int16_t *filter_x, int x_step_q4,
                                         const int16_t *filter_y, int y_step_q4,
                                         int w, int h, int bd) {
  const InterpKernel *const filters_y = get_filter_base(filter_y);
  const int y0_q4 = get_filter_offset(filter_y, filters_y);
  (void)filter_x;
  (void)x_step_q4;

  highbd_convolve_add_src_vert(src, src_stride, dst, dst_stride, filters_y,
                               y0_q4, y_step_q4, w, h, bd);
}

void aom_highbd_convolve8_add_src_c(const uint8_t *src, ptrdiff_t src_stride,
                                    uint8_t *dst, ptrdiff_t dst_stride,
                                    const int16_t *filter_x, int x_step_q4,
                                    const int16_t *filter_y, int y_step_q4,
                                    int w, int h, int bd) {
  const InterpKernel *const filters_x = get_filter_base(filter_x);
  const int x0_q4 = get_filter_offset(filter_x, filters_x);

  const InterpKernel *const filters_y = get_filter_base(filter_y);
  const int y0_q4 = get_filter_offset(filter_y, filters_y);

  highbd_convolve_add_src(src, src_stride, dst, dst_stride, filters_x, x0_q4,
                          x_step_q4, filters_y, y0_q4, y_step_q4, w, h, bd);
}

static void highbd_convolve_add_src_horiz_hip(
    const uint8_t *src8, ptrdiff_t src_stride, uint16_t *dst,
    ptrdiff_t dst_stride, const InterpKernel *x_filters, int x0_q4,
    int x_step_q4, int w, int h, int bd) {
  const int extraprec_clamp_limit = EXTRAPREC_CLAMP_LIMIT(bd);
  int x, y, k;
  uint16_t *src = CONVERT_TO_SHORTPTR(src8);
  src -= SUBPEL_TAPS / 2 - 1;
  for (y = 0; y < h; ++y) {
    int x_q4 = x0_q4;
    for (x = 0; x < w; ++x) {
      const uint16_t *const src_x = &src[x_q4 >> SUBPEL_BITS];
      const int16_t *const x_filter = x_filters[x_q4 & SUBPEL_MASK];
      int sum = ((int)src_x[SUBPEL_TAPS / 2 - 1] << FILTER_BITS) +
                (1 << (bd + FILTER_BITS - 1));
      for (k = 0; k < SUBPEL_TAPS; ++k) sum += src_x[k] * x_filter[k];
      dst[x] =
          (uint16_t)clamp(ROUND_POWER_OF_TWO(sum, FILTER_BITS - EXTRAPREC_BITS),
                          0, extraprec_clamp_limit - 1);
      x_q4 += x_step_q4;
    }
    src += src_stride;
    dst += dst_stride;
  }
}

static void highbd_convolve_add_src_vert_hip(
    const uint16_t *src, ptrdiff_t src_stride, uint8_t *dst8,
    ptrdiff_t dst_stride, const InterpKernel *y_filters, int y0_q4,
    int y_step_q4, int w, int h, int bd) {
  int x, y, k;
  uint16_t *dst = CONVERT_TO_SHORTPTR(dst8);
  src -= src_stride * (SUBPEL_TAPS / 2 - 1);
  for (x = 0; x < w; ++x) {
    int y_q4 = y0_q4;
    for (y = 0; y < h; ++y) {
      const uint16_t *src_y = &src[(y_q4 >> SUBPEL_BITS) * src_stride];
      const int16_t *const y_filter = y_filters[y_q4 & SUBPEL_MASK];
      int sum =
          ((int)src_y[(SUBPEL_TAPS / 2 - 1) * src_stride] << FILTER_BITS) -
          (1 << (bd + FILTER_BITS + EXTRAPREC_BITS - 1));
      for (k = 0; k < SUBPEL_TAPS; ++k)
        sum += src_y[k * src_stride] * y_filter[k];
      dst[y * dst_stride] = clip_pixel_highbd(
          ROUND_POWER_OF_TWO(sum, FILTER_BITS + EXTRAPREC_BITS), bd);
      y_q4 += y_step_q4;
    }
    ++src;
    ++dst;
  }
}

static void highbd_convolve_add_src_hip(
    const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst,
    ptrdiff_t dst_stride, const InterpKernel *const x_filters, int x0_q4,
    int x_step_q4, const InterpKernel *const y_filters, int y0_q4,
    int y_step_q4, int w, int h, int bd) {
  // Note: Fixed size intermediate buffer, temp, places limits on parameters.
  // 2d filtering proceeds in 2 steps:
  //   (1) Interpolate horizontally into an intermediate buffer, temp.
  //   (2) Interpolate temp vertically to derive the sub-pixel result.
  // Deriving the maximum number of rows in the temp buffer (135):
  // --Smallest scaling factor is x1/2 ==> y_step_q4 = 32 (Normative).
  // --Largest block size is 64x64 pixels.
  // --64 rows in the downscaled frame span a distance of (64 - 1) * 32 in the
  //   original frame (in 1/16th pixel units).
  // --Must round-up because block may be located at sub-pixel position.
  // --Require an additional SUBPEL_TAPS rows for the 8-tap filter tails.
  // --((64 - 1) * 32 + 15) >> 4 + 8 = 135.
  uint16_t temp[MAX_EXT_SIZE * MAX_SB_SIZE];
  int intermediate_height =
      (((h - 1) * y_step_q4 + y0_q4) >> SUBPEL_BITS) + SUBPEL_TAPS;

  assert(w <= MAX_SB_SIZE);
  assert(h <= MAX_SB_SIZE);
  assert(y_step_q4 <= 32);
  assert(x_step_q4 <= 32);

  highbd_convolve_add_src_horiz_hip(
      src - src_stride * (SUBPEL_TAPS / 2 - 1), src_stride, temp, MAX_SB_SIZE,
      x_filters, x0_q4, x_step_q4, w, intermediate_height, bd);
  highbd_convolve_add_src_vert_hip(temp + MAX_SB_SIZE * (SUBPEL_TAPS / 2 - 1),
                                   MAX_SB_SIZE, dst, dst_stride, y_filters,
                                   y0_q4, y_step_q4, w, h, bd);
}

void aom_highbd_convolve8_add_src_horiz_hip_c(
    const uint8_t *src, ptrdiff_t src_stride, uint16_t *dst,
    ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4,
    const int16_t *filter_y, int y_step_q4, int w, int h, int bd) {
  const InterpKernel *const filters_x = get_filter_base(filter_x);
  const int x0_q4 = get_filter_offset(filter_x, filters_x);
  (void)filter_y;
  (void)y_step_q4;

  highbd_convolve_add_src_horiz_hip(src, src_stride, dst, dst_stride, filters_x,
                                    x0_q4, x_step_q4, w, h, bd);
}

void aom_highbd_convolve8_add_src_vert_hip_c(
    const uint16_t *src, ptrdiff_t src_stride, uint8_t *dst,
    ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4,
    const int16_t *filter_y, int y_step_q4, int w, int h, int bd) {
  const InterpKernel *const filters_y = get_filter_base(filter_y);
  const int y0_q4 = get_filter_offset(filter_y, filters_y);
  (void)filter_x;
  (void)x_step_q4;

  highbd_convolve_add_src_vert_hip(src, src_stride, dst, dst_stride, filters_y,
                                   y0_q4, y_step_q4, w, h, bd);
}

void aom_highbd_convolve8_add_src_hip_c(const uint8_t *src,
                                        ptrdiff_t src_stride, uint8_t *dst,
                                        ptrdiff_t dst_stride,
                                        const int16_t *filter_x, int x_step_q4,
                                        const int16_t *filter_y, int y_step_q4,
                                        int w, int h, int bd) {
  const InterpKernel *const filters_x = get_filter_base(filter_x);
  const int x0_q4 = get_filter_offset(filter_x, filters_x);

  const InterpKernel *const filters_y = get_filter_base(filter_y);
  const int y0_q4 = get_filter_offset(filter_y, filters_y);

  highbd_convolve_add_src_hip(src, src_stride, dst, dst_stride, filters_x,
                              x0_q4, x_step_q4, filters_y, y0_q4, y_step_q4, w,
                              h, bd);
}

#endif  // CONFIG_LOOP_RESTORATION
#endif  // CONFIG_HIGHBITDEPTH
