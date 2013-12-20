/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
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
#include "vpx_ports/mem.h"
///////////////////////////////////////////////////////////////////////////
// the mmx function that does the bilinear filtering and var calculation //
// int one pass                                                          //
///////////////////////////////////////////////////////////////////////////
DECLARE_ALIGNED(16, const short, vp9_bilinear_filters_mmx[16][8]) = {
  { 128, 128, 128, 128,  0,  0,  0,  0 },
  { 120, 120, 120, 120,  8,  8,  8,  8 },
  { 112, 112, 112, 112, 16, 16, 16, 16 },
  { 104, 104, 104, 104, 24, 24, 24, 24 },
  {  96, 96, 96, 96, 32, 32, 32, 32 },
  {  88, 88, 88, 88, 40, 40, 40, 40 },
  {  80, 80, 80, 80, 48, 48, 48, 48 },
  {  72, 72, 72, 72, 56, 56, 56, 56 },
  {  64, 64, 64, 64, 64, 64, 64, 64 },
  {  56, 56, 56, 56, 72, 72, 72, 72 },
  {  48, 48, 48, 48, 80, 80, 80, 80 },
  {  40, 40, 40, 40, 88, 88, 88, 88 },
  {  32, 32, 32, 32, 96, 96, 96, 96 },
  {  24, 24, 24, 24, 104, 104, 104, 104 },
  {  16, 16, 16, 16, 112, 112, 112, 112 },
  {   8,  8,  8,  8, 120, 120, 120, 120 }
};

typedef void filter8_1dfunction (
  const unsigned char *src_ptr,
  const unsigned int src_pitch,
  unsigned char *output_ptr,
  unsigned int out_pitch,
  unsigned int output_height,
  const short *filter
);

#if HAVE_SSSE3
filter8_1dfunction vp9_filter_block1d16_v8_ssse3;
filter8_1dfunction vp9_filter_block1d16_h8_ssse3;
filter8_1dfunction vp9_filter_block1d8_v8_ssse3;
filter8_1dfunction vp9_filter_block1d8_h8_ssse3;
filter8_1dfunction vp9_filter_block1d4_v8_ssse3;
filter8_1dfunction vp9_filter_block1d4_h8_ssse3;
filter8_1dfunction vp9_filter_block1d16_v8_avg_ssse3;
filter8_1dfunction vp9_filter_block1d16_h8_avg_ssse3;
filter8_1dfunction vp9_filter_block1d8_v8_avg_ssse3;
filter8_1dfunction vp9_filter_block1d8_h8_avg_ssse3;
filter8_1dfunction vp9_filter_block1d4_v8_avg_ssse3;
filter8_1dfunction vp9_filter_block1d4_h8_avg_ssse3;

void vp9_convolve8_horiz_ssse3(const uint8_t *src, ptrdiff_t src_stride,
                               uint8_t *dst, ptrdiff_t dst_stride,
                               const int16_t *filter_x, int x_step_q4,
                               const int16_t *filter_y, int y_step_q4,
                               int w, int h) {
  /* Ensure the filter can be compressed to int16_t. */
  if (x_step_q4 == 16 && filter_x[3] != 128) {
    while (w >= 16) {
      vp9_filter_block1d16_h8_ssse3(src, src_stride,
                                    dst, dst_stride,
                                    h, filter_x);
      src += 16;
      dst += 16;
      w -= 16;
    }
    while (w >= 8) {
      vp9_filter_block1d8_h8_ssse3(src, src_stride,
                                   dst, dst_stride,
                                   h, filter_x);
      src += 8;
      dst += 8;
      w -= 8;
    }
    while (w >= 4) {
      vp9_filter_block1d4_h8_ssse3(src, src_stride,
                                   dst, dst_stride,
                                   h, filter_x);
      src += 4;
      dst += 4;
      w -= 4;
    }
  }
  if (w) {
    vp9_convolve8_horiz_c(src, src_stride, dst, dst_stride,
                          filter_x, x_step_q4, filter_y, y_step_q4,
                          w, h);
  }
}

void vp9_convolve8_vert_ssse3(const uint8_t *src, ptrdiff_t src_stride,
                              uint8_t *dst, ptrdiff_t dst_stride,
                              const int16_t *filter_x, int x_step_q4,
                              const int16_t *filter_y, int y_step_q4,
                              int w, int h) {
  if (y_step_q4 == 16 && filter_y[3] != 128) {
    while (w >= 16) {
      vp9_filter_block1d16_v8_ssse3(src - src_stride * 3, src_stride,
                                    dst, dst_stride,
                                    h, filter_y);
      src += 16;
      dst += 16;
      w -= 16;
    }
    while (w >= 8) {
      vp9_filter_block1d8_v8_ssse3(src - src_stride * 3, src_stride,
                                   dst, dst_stride,
                                   h, filter_y);
      src += 8;
      dst += 8;
      w -= 8;
    }
    while (w >= 4) {
      vp9_filter_block1d4_v8_ssse3(src - src_stride * 3, src_stride,
                                   dst, dst_stride,
                                   h, filter_y);
      src += 4;
      dst += 4;
      w -= 4;
    }
  }
  if (w) {
    vp9_convolve8_vert_c(src, src_stride, dst, dst_stride,
                         filter_x, x_step_q4, filter_y, y_step_q4,
                         w, h);
  }
}

void vp9_convolve8_avg_horiz_ssse3(const uint8_t *src, ptrdiff_t src_stride,
                               uint8_t *dst, ptrdiff_t dst_stride,
                               const int16_t *filter_x, int x_step_q4,
                               const int16_t *filter_y, int y_step_q4,
                               int w, int h) {
  if (x_step_q4 == 16 && filter_x[3] != 128) {
    while (w >= 16) {
      vp9_filter_block1d16_h8_avg_ssse3(src, src_stride,
                                    dst, dst_stride,
                                    h, filter_x);
      src += 16;
      dst += 16;
      w -= 16;
    }
    while (w >= 8) {
      vp9_filter_block1d8_h8_avg_ssse3(src, src_stride,
                                   dst, dst_stride,
                                   h, filter_x);
      src += 8;
      dst += 8;
      w -= 8;
    }
    while (w >= 4) {
      vp9_filter_block1d4_h8_avg_ssse3(src, src_stride,
                                   dst, dst_stride,
                                   h, filter_x);
      src += 4;
      dst += 4;
      w -= 4;
    }
  }
  if (w) {
    vp9_convolve8_avg_horiz_c(src, src_stride, dst, dst_stride,
                              filter_x, x_step_q4, filter_y, y_step_q4,
                              w, h);
  }
}

void vp9_convolve8_avg_vert_ssse3(const uint8_t *src, ptrdiff_t src_stride,
                              uint8_t *dst, ptrdiff_t dst_stride,
                              const int16_t *filter_x, int x_step_q4,
                              const int16_t *filter_y, int y_step_q4,
                              int w, int h) {
  if (y_step_q4 == 16 && filter_y[3] != 128) {
    while (w >= 16) {
      vp9_filter_block1d16_v8_avg_ssse3(src - src_stride * 3, src_stride,
                                    dst, dst_stride,
                                    h, filter_y);
      src += 16;
      dst += 16;
      w -= 16;
    }
    while (w >= 8) {
      vp9_filter_block1d8_v8_avg_ssse3(src - src_stride * 3, src_stride,
                                   dst, dst_stride,
                                   h, filter_y);
      src += 8;
      dst += 8;
      w -= 8;
    }
    while (w >= 4) {
      vp9_filter_block1d4_v8_avg_ssse3(src - src_stride * 3, src_stride,
                                   dst, dst_stride,
                                   h, filter_y);
      src += 4;
      dst += 4;
      w -= 4;
    }
  }
  if (w) {
    vp9_convolve8_avg_vert_c(src, src_stride, dst, dst_stride,
                             filter_x, x_step_q4, filter_y, y_step_q4,
                             w, h);
  }
}

void vp9_convolve8_ssse3(const uint8_t *src, ptrdiff_t src_stride,
                         uint8_t *dst, ptrdiff_t dst_stride,
                         const int16_t *filter_x, int x_step_q4,
                         const int16_t *filter_y, int y_step_q4,
                         int w, int h) {
  DECLARE_ALIGNED_ARRAY(16, unsigned char, fdata2, 64 * 71);

  assert(w <= 64);
  assert(h <= 64);
  if (x_step_q4 == 16 && y_step_q4 == 16) {
    vp9_convolve8_horiz_ssse3(src - 3 * src_stride, src_stride, fdata2, 64,
                              filter_x, x_step_q4, filter_y, y_step_q4,
                              w, h + 7);
    vp9_convolve8_vert_ssse3(fdata2 + 3 * 64, 64, dst, dst_stride,
                             filter_x, x_step_q4, filter_y, y_step_q4, w, h);
  } else {
    vp9_convolve8_c(src, src_stride, dst, dst_stride,
                    filter_x, x_step_q4, filter_y, y_step_q4, w, h);
  }
}

void vp9_convolve8_avg_ssse3(const uint8_t *src, ptrdiff_t src_stride,
                         uint8_t *dst, ptrdiff_t dst_stride,
                         const int16_t *filter_x, int x_step_q4,
                         const int16_t *filter_y, int y_step_q4,
                         int w, int h) {
  DECLARE_ALIGNED_ARRAY(16, unsigned char, fdata2, 64 * 71);

  assert(w <= 64);
  assert(h <= 64);
  if (x_step_q4 == 16 && y_step_q4 == 16) {
    vp9_convolve8_horiz_ssse3(src - 3 * src_stride, src_stride, fdata2, 64,
                              filter_x, x_step_q4, filter_y, y_step_q4,
                              w, h + 7);
    vp9_convolve8_avg_vert_ssse3(fdata2 + 3 * 64, 64, dst, dst_stride,
                                 filter_x, x_step_q4, filter_y, y_step_q4,
                                 w, h);
  } else {
    vp9_convolve8_avg_c(src, src_stride, dst, dst_stride,
                        filter_x, x_step_q4, filter_y, y_step_q4, w, h);
  }
}
#endif

#if HAVE_SSE2
filter8_1dfunction vp9_filter_block1d16_v8_sse2;
filter8_1dfunction vp9_filter_block1d16_h8_sse2;
filter8_1dfunction vp9_filter_block1d8_v8_sse2;
filter8_1dfunction vp9_filter_block1d8_h8_sse2;
filter8_1dfunction vp9_filter_block1d4_v8_sse2;
filter8_1dfunction vp9_filter_block1d4_h8_sse2;
filter8_1dfunction vp9_filter_block1d16_v8_avg_sse2;
filter8_1dfunction vp9_filter_block1d16_h8_avg_sse2;
filter8_1dfunction vp9_filter_block1d8_v8_avg_sse2;
filter8_1dfunction vp9_filter_block1d8_h8_avg_sse2;
filter8_1dfunction vp9_filter_block1d4_v8_avg_sse2;
filter8_1dfunction vp9_filter_block1d4_h8_avg_sse2;

void vp9_convolve8_horiz_sse2(const uint8_t *src, ptrdiff_t src_stride,
                               uint8_t *dst, ptrdiff_t dst_stride,
                               const int16_t *filter_x, int x_step_q4,
                               const int16_t *filter_y, int y_step_q4,
                               int w, int h) {
  /* Ensure the filter can be compressed to int16_t. */
  if (x_step_q4 == 16 && filter_x[3] != 128) {
    while (w >= 16) {
      vp9_filter_block1d16_h8_sse2(src, src_stride,
                                    dst, dst_stride,
                                    h, filter_x);
      src += 16;
      dst += 16;
      w -= 16;
    }
    while (w >= 8) {
      vp9_filter_block1d8_h8_sse2(src, src_stride,
                                   dst, dst_stride,
                                   h, filter_x);
      src += 8;
      dst += 8;
      w -= 8;
    }
    while (w >= 4) {
      vp9_filter_block1d4_h8_sse2(src, src_stride,
                                   dst, dst_stride,
                                   h, filter_x);
      src += 4;
      dst += 4;
      w -= 4;
    }
  }
  if (w) {
    vp9_convolve8_horiz_c(src, src_stride, dst, dst_stride,
                          filter_x, x_step_q4, filter_y, y_step_q4,
                          w, h);
  }
}

void vp9_convolve8_vert_sse2(const uint8_t *src, ptrdiff_t src_stride,
                              uint8_t *dst, ptrdiff_t dst_stride,
                              const int16_t *filter_x, int x_step_q4,
                              const int16_t *filter_y, int y_step_q4,
                              int w, int h) {
  if (y_step_q4 == 16 && filter_y[3] != 128) {
    while (w >= 16) {
      vp9_filter_block1d16_v8_sse2(src - src_stride * 3, src_stride,
                                    dst, dst_stride,
                                    h, filter_y);
      src += 16;
      dst += 16;
      w -= 16;
    }
    while (w >= 8) {
      vp9_filter_block1d8_v8_sse2(src - src_stride * 3, src_stride,
                                   dst, dst_stride,
                                   h, filter_y);
      src += 8;
      dst += 8;
      w -= 8;
    }
    while (w >= 4) {
      vp9_filter_block1d4_v8_sse2(src - src_stride * 3, src_stride,
                                   dst, dst_stride,
                                   h, filter_y);
      src += 4;
      dst += 4;
      w -= 4;
    }
  }
  if (w) {
    vp9_convolve8_vert_c(src, src_stride, dst, dst_stride,
                         filter_x, x_step_q4, filter_y, y_step_q4,
                         w, h);
  }
}

void vp9_convolve8_avg_horiz_sse2(const uint8_t *src, ptrdiff_t src_stride,
                               uint8_t *dst, ptrdiff_t dst_stride,
                               const int16_t *filter_x, int x_step_q4,
                               const int16_t *filter_y, int y_step_q4,
                               int w, int h) {
  if (x_step_q4 == 16 && filter_x[3] != 128) {
    while (w >= 16) {
      vp9_filter_block1d16_h8_avg_sse2(src, src_stride,
                                    dst, dst_stride,
                                    h, filter_x);
      src += 16;
      dst += 16;
      w -= 16;
    }
    while (w >= 8) {
      vp9_filter_block1d8_h8_avg_sse2(src, src_stride,
                                   dst, dst_stride,
                                   h, filter_x);
      src += 8;
      dst += 8;
      w -= 8;
    }
    while (w >= 4) {
      vp9_filter_block1d4_h8_avg_sse2(src, src_stride,
                                   dst, dst_stride,
                                   h, filter_x);
      src += 4;
      dst += 4;
      w -= 4;
    }
  }
  if (w) {
    vp9_convolve8_avg_horiz_c(src, src_stride, dst, dst_stride,
                              filter_x, x_step_q4, filter_y, y_step_q4,
                              w, h);
  }
}

void vp9_convolve8_avg_vert_sse2(const uint8_t *src, ptrdiff_t src_stride,
                              uint8_t *dst, ptrdiff_t dst_stride,
                              const int16_t *filter_x, int x_step_q4,
                              const int16_t *filter_y, int y_step_q4,
                              int w, int h) {
  if (y_step_q4 == 16 && filter_y[3] != 128) {
    while (w >= 16) {
      vp9_filter_block1d16_v8_avg_sse2(src - src_stride * 3, src_stride,
                                    dst, dst_stride,
                                    h, filter_y);
      src += 16;
      dst += 16;
      w -= 16;
    }
    while (w >= 8) {
      vp9_filter_block1d8_v8_avg_sse2(src - src_stride * 3, src_stride,
                                   dst, dst_stride,
                                   h, filter_y);
      src += 8;
      dst += 8;
      w -= 8;
    }
    while (w >= 4) {
      vp9_filter_block1d4_v8_avg_sse2(src - src_stride * 3, src_stride,
                                   dst, dst_stride,
                                   h, filter_y);
      src += 4;
      dst += 4;
      w -= 4;
    }
  }
  if (w) {
    vp9_convolve8_avg_vert_c(src, src_stride, dst, dst_stride,
                             filter_x, x_step_q4, filter_y, y_step_q4,
                             w, h);
  }
}

void vp9_convolve8_sse2(const uint8_t *src, ptrdiff_t src_stride,
                         uint8_t *dst, ptrdiff_t dst_stride,
                         const int16_t *filter_x, int x_step_q4,
                         const int16_t *filter_y, int y_step_q4,
                         int w, int h) {
  DECLARE_ALIGNED_ARRAY(16, unsigned char, fdata2, 64 * 71);

  assert(w <= 64);
  assert(h <= 64);
  if (x_step_q4 == 16 && y_step_q4 == 16) {
    vp9_convolve8_horiz_sse2(src - 3 * src_stride, src_stride, fdata2, 64,
                              filter_x, x_step_q4, filter_y, y_step_q4,
                              w, h + 7);
    vp9_convolve8_vert_sse2(fdata2 + 3 * 64, 64, dst, dst_stride,
                             filter_x, x_step_q4, filter_y, y_step_q4, w, h);
  } else {
    vp9_convolve8_c(src, src_stride, dst, dst_stride,
                    filter_x, x_step_q4, filter_y, y_step_q4, w, h);
  }
}

void vp9_convolve8_avg_sse2(const uint8_t *src, ptrdiff_t src_stride,
                         uint8_t *dst, ptrdiff_t dst_stride,
                         const int16_t *filter_x, int x_step_q4,
                         const int16_t *filter_y, int y_step_q4,
                         int w, int h) {
  DECLARE_ALIGNED_ARRAY(16, unsigned char, fdata2, 64 * 71);

  assert(w <= 64);
  assert(h <= 64);
  if (x_step_q4 == 16 && y_step_q4 == 16) {
    vp9_convolve8_horiz_sse2(src - 3 * src_stride, src_stride, fdata2, 64,
                              filter_x, x_step_q4, filter_y, y_step_q4,
                              w, h + 7);
    vp9_convolve8_avg_vert_sse2(fdata2 + 3 * 64, 64, dst, dst_stride,
                                 filter_x, x_step_q4, filter_y, y_step_q4,
                                 w, h);
  } else {
    vp9_convolve8_avg_c(src, src_stride, dst, dst_stride,
                        filter_x, x_step_q4, filter_y, y_step_q4, w, h);
  }
}
#endif
