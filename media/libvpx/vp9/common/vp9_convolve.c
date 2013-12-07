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

static void convolve_horiz_c(const uint8_t *src, ptrdiff_t src_stride,
                             uint8_t *dst, ptrdiff_t dst_stride,
                             const int16_t *filter_x0, int x_step_q4,
                             const int16_t *filter_y, int y_step_q4,
                             int w, int h, int taps) {
  int x, y, k;

  /* NOTE: This assumes that the filter table is 256-byte aligned. */
  /* TODO(agrange) Modify to make independent of table alignment. */
  const int16_t *const filter_x_base =
      (const int16_t *)(((intptr_t)filter_x0) & ~(intptr_t)0xff);

  /* Adjust base pointer address for this source line */
  src -= taps / 2 - 1;

  for (y = 0; y < h; ++y) {
    /* Initial phase offset */
    int x_q4 = (int)(filter_x0 - filter_x_base) / taps;

    for (x = 0; x < w; ++x) {
      /* Per-pixel src offset */
      const int src_x = x_q4 >> SUBPEL_BITS;
      int sum = 0;

      /* Pointer to filter to use */
      const int16_t *const filter_x = filter_x_base +
          (x_q4 & SUBPEL_MASK) * taps;

      for (k = 0; k < taps; ++k)
        sum += src[src_x + k] * filter_x[k];

      dst[x] = clip_pixel(ROUND_POWER_OF_TWO(sum, FILTER_BITS));

      /* Move to the next source pixel */
      x_q4 += x_step_q4;
    }
    src += src_stride;
    dst += dst_stride;
  }
}

static void convolve_avg_horiz_c(const uint8_t *src, ptrdiff_t src_stride,
                                 uint8_t *dst, ptrdiff_t dst_stride,
                                 const int16_t *filter_x0, int x_step_q4,
                                 const int16_t *filter_y, int y_step_q4,
                                 int w, int h, int taps) {
  int x, y, k;

  /* NOTE: This assumes that the filter table is 256-byte aligned. */
  /* TODO(agrange) Modify to make independent of table alignment. */
  const int16_t *const filter_x_base =
      (const int16_t *)(((intptr_t)filter_x0) & ~(intptr_t)0xff);

  /* Adjust base pointer address for this source line */
  src -= taps / 2 - 1;

  for (y = 0; y < h; ++y) {
    /* Initial phase offset */
    int x_q4 = (int)(filter_x0 - filter_x_base) / taps;

    for (x = 0; x < w; ++x) {
      /* Per-pixel src offset */
      const int src_x = x_q4 >> SUBPEL_BITS;
      int sum = 0;

      /* Pointer to filter to use */
      const int16_t *const filter_x = filter_x_base +
          (x_q4 & SUBPEL_MASK) * taps;

      for (k = 0; k < taps; ++k)
        sum += src[src_x + k] * filter_x[k];

      dst[x] = ROUND_POWER_OF_TWO(dst[x] +
                   clip_pixel(ROUND_POWER_OF_TWO(sum, FILTER_BITS)), 1);

      /* Move to the next source pixel */
      x_q4 += x_step_q4;
    }
    src += src_stride;
    dst += dst_stride;
  }
}

static void convolve_vert_c(const uint8_t *src, ptrdiff_t src_stride,
                            uint8_t *dst, ptrdiff_t dst_stride,
                            const int16_t *filter_x, int x_step_q4,
                            const int16_t *filter_y0, int y_step_q4,
                            int w, int h, int taps) {
  int x, y, k;

  /* NOTE: This assumes that the filter table is 256-byte aligned. */
  /* TODO(agrange) Modify to make independent of table alignment. */
  const int16_t *const filter_y_base =
      (const int16_t *)(((intptr_t)filter_y0) & ~(intptr_t)0xff);

  /* Adjust base pointer address for this source column */
  src -= src_stride * (taps / 2 - 1);

  for (x = 0; x < w; ++x) {
    /* Initial phase offset */
    int y_q4 = (int)(filter_y0 - filter_y_base) / taps;

    for (y = 0; y < h; ++y) {
      /* Per-pixel src offset */
      const int src_y = y_q4 >> SUBPEL_BITS;
      int sum = 0;

      /* Pointer to filter to use */
      const int16_t *const filter_y = filter_y_base +
          (y_q4 & SUBPEL_MASK) * taps;

      for (k = 0; k < taps; ++k)
        sum += src[(src_y + k) * src_stride] * filter_y[k];

      dst[y * dst_stride] =
          clip_pixel(ROUND_POWER_OF_TWO(sum, FILTER_BITS));

      /* Move to the next source pixel */
      y_q4 += y_step_q4;
    }
    ++src;
    ++dst;
  }
}

static void convolve_avg_vert_c(const uint8_t *src, ptrdiff_t src_stride,
                                uint8_t *dst, ptrdiff_t dst_stride,
                                const int16_t *filter_x, int x_step_q4,
                                const int16_t *filter_y0, int y_step_q4,
                                int w, int h, int taps) {
  int x, y, k;

  /* NOTE: This assumes that the filter table is 256-byte aligned. */
  /* TODO(agrange) Modify to make independent of table alignment. */
  const int16_t *const filter_y_base =
      (const int16_t *)(((intptr_t)filter_y0) & ~(intptr_t)0xff);

  /* Adjust base pointer address for this source column */
  src -= src_stride * (taps / 2 - 1);

  for (x = 0; x < w; ++x) {
    /* Initial phase offset */
    int y_q4 = (int)(filter_y0 - filter_y_base) / taps;

    for (y = 0; y < h; ++y) {
      /* Per-pixel src offset */
      const int src_y = y_q4 >> SUBPEL_BITS;
      int sum = 0;

      /* Pointer to filter to use */
      const int16_t *const filter_y = filter_y_base +
          (y_q4 & SUBPEL_MASK) * taps;

      for (k = 0; k < taps; ++k)
        sum += src[(src_y + k) * src_stride] * filter_y[k];

      dst[y * dst_stride] = ROUND_POWER_OF_TWO(dst[y * dst_stride] +
           clip_pixel(ROUND_POWER_OF_TWO(sum, FILTER_BITS)), 1);

      /* Move to the next source pixel */
      y_q4 += y_step_q4;
    }
    ++src;
    ++dst;
  }
}

static void convolve_c(const uint8_t *src, ptrdiff_t src_stride,
                       uint8_t *dst, ptrdiff_t dst_stride,
                       const int16_t *filter_x, int x_step_q4,
                       const int16_t *filter_y, int y_step_q4,
                       int w, int h, int taps) {
  /* Fixed size intermediate buffer places limits on parameters.
   * Maximum intermediate_height is 324, for y_step_q4 == 80,
   * h == 64, taps == 8.
   * y_step_q4 of 80 allows for 1/10 scale for 5 layer svc
   */
  uint8_t temp[64 * 324];
  int intermediate_height = (((h - 1) * y_step_q4 + 15) >> 4) + taps;

  assert(w <= 64);
  assert(h <= 64);
  assert(taps <= 8);
  assert(y_step_q4 <= 80);
  assert(x_step_q4 <= 80);

  if (intermediate_height < h)
    intermediate_height = h;

  convolve_horiz_c(src - src_stride * (taps / 2 - 1), src_stride, temp, 64,
                   filter_x, x_step_q4, filter_y, y_step_q4, w,
                   intermediate_height, taps);
  convolve_vert_c(temp + 64 * (taps / 2 - 1), 64, dst, dst_stride, filter_x,
                  x_step_q4, filter_y, y_step_q4, w, h, taps);
}

void vp9_convolve8_horiz_c(const uint8_t *src, ptrdiff_t src_stride,
                           uint8_t *dst, ptrdiff_t dst_stride,
                           const int16_t *filter_x, int x_step_q4,
                           const int16_t *filter_y, int y_step_q4,
                           int w, int h) {
  convolve_horiz_c(src, src_stride, dst, dst_stride,
                   filter_x, x_step_q4, filter_y, y_step_q4, w, h, 8);
}

void vp9_convolve8_avg_horiz_c(const uint8_t *src, ptrdiff_t src_stride,
                               uint8_t *dst, ptrdiff_t dst_stride,
                               const int16_t *filter_x, int x_step_q4,
                               const int16_t *filter_y, int y_step_q4,
                               int w, int h) {
  convolve_avg_horiz_c(src, src_stride, dst, dst_stride,
                       filter_x, x_step_q4, filter_y, y_step_q4, w, h, 8);
}

void vp9_convolve8_vert_c(const uint8_t *src, ptrdiff_t src_stride,
                          uint8_t *dst, ptrdiff_t dst_stride,
                          const int16_t *filter_x, int x_step_q4,
                          const int16_t *filter_y, int y_step_q4,
                          int w, int h) {
  convolve_vert_c(src, src_stride, dst, dst_stride,
                  filter_x, x_step_q4, filter_y, y_step_q4, w, h, 8);
}

void vp9_convolve8_avg_vert_c(const uint8_t *src, ptrdiff_t src_stride,
                              uint8_t *dst, ptrdiff_t dst_stride,
                              const int16_t *filter_x, int x_step_q4,
                              const int16_t *filter_y, int y_step_q4,
                              int w, int h) {
  convolve_avg_vert_c(src, src_stride, dst, dst_stride,
                      filter_x, x_step_q4, filter_y, y_step_q4, w, h, 8);
}

void vp9_convolve8_c(const uint8_t *src, ptrdiff_t src_stride,
                     uint8_t *dst, ptrdiff_t dst_stride,
                     const int16_t *filter_x, int x_step_q4,
                     const int16_t *filter_y, int y_step_q4,
                     int w, int h) {
  convolve_c(src, src_stride, dst, dst_stride,
             filter_x, x_step_q4, filter_y, y_step_q4, w, h, 8);
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

  vp9_convolve8(src, src_stride, temp, 64,
               filter_x, x_step_q4, filter_y, y_step_q4, w, h);
  vp9_convolve_avg(temp, 64, dst, dst_stride, NULL, 0, NULL, 0, w, h);
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
