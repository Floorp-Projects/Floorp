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
#include <emmintrin.h>
#include <stddef.h>

#include "./aom_config.h"
#include "./aom_dsp_rtcd.h"

typedef void (*SubtractWxHFuncType)(int16_t *diff, ptrdiff_t diff_stride,
                                    const uint16_t *src, ptrdiff_t src_stride,
                                    const uint16_t *pred,
                                    ptrdiff_t pred_stride);

static void subtract_4x4(int16_t *diff, ptrdiff_t diff_stride,
                         const uint16_t *src, ptrdiff_t src_stride,
                         const uint16_t *pred, ptrdiff_t pred_stride) {
  __m128i u0, u1, u2, u3;
  __m128i v0, v1, v2, v3;
  __m128i x0, x1, x2, x3;
  int64_t *store_diff = (int64_t *)(diff + 0 * diff_stride);

  u0 = _mm_loadu_si128((__m128i const *)(src + 0 * src_stride));
  u1 = _mm_loadu_si128((__m128i const *)(src + 1 * src_stride));
  u2 = _mm_loadu_si128((__m128i const *)(src + 2 * src_stride));
  u3 = _mm_loadu_si128((__m128i const *)(src + 3 * src_stride));

  v0 = _mm_loadu_si128((__m128i const *)(pred + 0 * pred_stride));
  v1 = _mm_loadu_si128((__m128i const *)(pred + 1 * pred_stride));
  v2 = _mm_loadu_si128((__m128i const *)(pred + 2 * pred_stride));
  v3 = _mm_loadu_si128((__m128i const *)(pred + 3 * pred_stride));

  x0 = _mm_sub_epi16(u0, v0);
  x1 = _mm_sub_epi16(u1, v1);
  x2 = _mm_sub_epi16(u2, v2);
  x3 = _mm_sub_epi16(u3, v3);

  _mm_storel_epi64((__m128i *)store_diff, x0);
  store_diff = (int64_t *)(diff + 1 * diff_stride);
  _mm_storel_epi64((__m128i *)store_diff, x1);
  store_diff = (int64_t *)(diff + 2 * diff_stride);
  _mm_storel_epi64((__m128i *)store_diff, x2);
  store_diff = (int64_t *)(diff + 3 * diff_stride);
  _mm_storel_epi64((__m128i *)store_diff, x3);
}

static void subtract_4x8(int16_t *diff, ptrdiff_t diff_stride,
                         const uint16_t *src, ptrdiff_t src_stride,
                         const uint16_t *pred, ptrdiff_t pred_stride) {
  __m128i u0, u1, u2, u3, u4, u5, u6, u7;
  __m128i v0, v1, v2, v3, v4, v5, v6, v7;
  __m128i x0, x1, x2, x3, x4, x5, x6, x7;
  int64_t *store_diff = (int64_t *)(diff + 0 * diff_stride);

  u0 = _mm_loadu_si128((__m128i const *)(src + 0 * src_stride));
  u1 = _mm_loadu_si128((__m128i const *)(src + 1 * src_stride));
  u2 = _mm_loadu_si128((__m128i const *)(src + 2 * src_stride));
  u3 = _mm_loadu_si128((__m128i const *)(src + 3 * src_stride));
  u4 = _mm_loadu_si128((__m128i const *)(src + 4 * src_stride));
  u5 = _mm_loadu_si128((__m128i const *)(src + 5 * src_stride));
  u6 = _mm_loadu_si128((__m128i const *)(src + 6 * src_stride));
  u7 = _mm_loadu_si128((__m128i const *)(src + 7 * src_stride));

  v0 = _mm_loadu_si128((__m128i const *)(pred + 0 * pred_stride));
  v1 = _mm_loadu_si128((__m128i const *)(pred + 1 * pred_stride));
  v2 = _mm_loadu_si128((__m128i const *)(pred + 2 * pred_stride));
  v3 = _mm_loadu_si128((__m128i const *)(pred + 3 * pred_stride));
  v4 = _mm_loadu_si128((__m128i const *)(pred + 4 * pred_stride));
  v5 = _mm_loadu_si128((__m128i const *)(pred + 5 * pred_stride));
  v6 = _mm_loadu_si128((__m128i const *)(pred + 6 * pred_stride));
  v7 = _mm_loadu_si128((__m128i const *)(pred + 7 * pred_stride));

  x0 = _mm_sub_epi16(u0, v0);
  x1 = _mm_sub_epi16(u1, v1);
  x2 = _mm_sub_epi16(u2, v2);
  x3 = _mm_sub_epi16(u3, v3);
  x4 = _mm_sub_epi16(u4, v4);
  x5 = _mm_sub_epi16(u5, v5);
  x6 = _mm_sub_epi16(u6, v6);
  x7 = _mm_sub_epi16(u7, v7);

  _mm_storel_epi64((__m128i *)store_diff, x0);
  store_diff = (int64_t *)(diff + 1 * diff_stride);
  _mm_storel_epi64((__m128i *)store_diff, x1);
  store_diff = (int64_t *)(diff + 2 * diff_stride);
  _mm_storel_epi64((__m128i *)store_diff, x2);
  store_diff = (int64_t *)(diff + 3 * diff_stride);
  _mm_storel_epi64((__m128i *)store_diff, x3);
  store_diff = (int64_t *)(diff + 4 * diff_stride);
  _mm_storel_epi64((__m128i *)store_diff, x4);
  store_diff = (int64_t *)(diff + 5 * diff_stride);
  _mm_storel_epi64((__m128i *)store_diff, x5);
  store_diff = (int64_t *)(diff + 6 * diff_stride);
  _mm_storel_epi64((__m128i *)store_diff, x6);
  store_diff = (int64_t *)(diff + 7 * diff_stride);
  _mm_storel_epi64((__m128i *)store_diff, x7);
}

static void subtract_8x4(int16_t *diff, ptrdiff_t diff_stride,
                         const uint16_t *src, ptrdiff_t src_stride,
                         const uint16_t *pred, ptrdiff_t pred_stride) {
  __m128i u0, u1, u2, u3;
  __m128i v0, v1, v2, v3;
  __m128i x0, x1, x2, x3;

  u0 = _mm_loadu_si128((__m128i const *)(src + 0 * src_stride));
  u1 = _mm_loadu_si128((__m128i const *)(src + 1 * src_stride));
  u2 = _mm_loadu_si128((__m128i const *)(src + 2 * src_stride));
  u3 = _mm_loadu_si128((__m128i const *)(src + 3 * src_stride));

  v0 = _mm_loadu_si128((__m128i const *)(pred + 0 * pred_stride));
  v1 = _mm_loadu_si128((__m128i const *)(pred + 1 * pred_stride));
  v2 = _mm_loadu_si128((__m128i const *)(pred + 2 * pred_stride));
  v3 = _mm_loadu_si128((__m128i const *)(pred + 3 * pred_stride));

  x0 = _mm_sub_epi16(u0, v0);
  x1 = _mm_sub_epi16(u1, v1);
  x2 = _mm_sub_epi16(u2, v2);
  x3 = _mm_sub_epi16(u3, v3);

  _mm_storeu_si128((__m128i *)(diff + 0 * diff_stride), x0);
  _mm_storeu_si128((__m128i *)(diff + 1 * diff_stride), x1);
  _mm_storeu_si128((__m128i *)(diff + 2 * diff_stride), x2);
  _mm_storeu_si128((__m128i *)(diff + 3 * diff_stride), x3);
}

static void subtract_8x8(int16_t *diff, ptrdiff_t diff_stride,
                         const uint16_t *src, ptrdiff_t src_stride,
                         const uint16_t *pred, ptrdiff_t pred_stride) {
  __m128i u0, u1, u2, u3, u4, u5, u6, u7;
  __m128i v0, v1, v2, v3, v4, v5, v6, v7;
  __m128i x0, x1, x2, x3, x4, x5, x6, x7;

  u0 = _mm_loadu_si128((__m128i const *)(src + 0 * src_stride));
  u1 = _mm_loadu_si128((__m128i const *)(src + 1 * src_stride));
  u2 = _mm_loadu_si128((__m128i const *)(src + 2 * src_stride));
  u3 = _mm_loadu_si128((__m128i const *)(src + 3 * src_stride));
  u4 = _mm_loadu_si128((__m128i const *)(src + 4 * src_stride));
  u5 = _mm_loadu_si128((__m128i const *)(src + 5 * src_stride));
  u6 = _mm_loadu_si128((__m128i const *)(src + 6 * src_stride));
  u7 = _mm_loadu_si128((__m128i const *)(src + 7 * src_stride));

  v0 = _mm_loadu_si128((__m128i const *)(pred + 0 * pred_stride));
  v1 = _mm_loadu_si128((__m128i const *)(pred + 1 * pred_stride));
  v2 = _mm_loadu_si128((__m128i const *)(pred + 2 * pred_stride));
  v3 = _mm_loadu_si128((__m128i const *)(pred + 3 * pred_stride));
  v4 = _mm_loadu_si128((__m128i const *)(pred + 4 * pred_stride));
  v5 = _mm_loadu_si128((__m128i const *)(pred + 5 * pred_stride));
  v6 = _mm_loadu_si128((__m128i const *)(pred + 6 * pred_stride));
  v7 = _mm_loadu_si128((__m128i const *)(pred + 7 * pred_stride));

  x0 = _mm_sub_epi16(u0, v0);
  x1 = _mm_sub_epi16(u1, v1);
  x2 = _mm_sub_epi16(u2, v2);
  x3 = _mm_sub_epi16(u3, v3);
  x4 = _mm_sub_epi16(u4, v4);
  x5 = _mm_sub_epi16(u5, v5);
  x6 = _mm_sub_epi16(u6, v6);
  x7 = _mm_sub_epi16(u7, v7);

  _mm_storeu_si128((__m128i *)(diff + 0 * diff_stride), x0);
  _mm_storeu_si128((__m128i *)(diff + 1 * diff_stride), x1);
  _mm_storeu_si128((__m128i *)(diff + 2 * diff_stride), x2);
  _mm_storeu_si128((__m128i *)(diff + 3 * diff_stride), x3);
  _mm_storeu_si128((__m128i *)(diff + 4 * diff_stride), x4);
  _mm_storeu_si128((__m128i *)(diff + 5 * diff_stride), x5);
  _mm_storeu_si128((__m128i *)(diff + 6 * diff_stride), x6);
  _mm_storeu_si128((__m128i *)(diff + 7 * diff_stride), x7);
}

static void subtract_8x16(int16_t *diff, ptrdiff_t diff_stride,
                          const uint16_t *src, ptrdiff_t src_stride,
                          const uint16_t *pred, ptrdiff_t pred_stride) {
  subtract_8x8(diff, diff_stride, src, src_stride, pred, pred_stride);
  diff += diff_stride << 3;
  src += src_stride << 3;
  pred += pred_stride << 3;
  subtract_8x8(diff, diff_stride, src, src_stride, pred, pred_stride);
}

static void subtract_16x8(int16_t *diff, ptrdiff_t diff_stride,
                          const uint16_t *src, ptrdiff_t src_stride,
                          const uint16_t *pred, ptrdiff_t pred_stride) {
  subtract_8x8(diff, diff_stride, src, src_stride, pred, pred_stride);
  diff += 8;
  src += 8;
  pred += 8;
  subtract_8x8(diff, diff_stride, src, src_stride, pred, pred_stride);
}

static void subtract_16x16(int16_t *diff, ptrdiff_t diff_stride,
                           const uint16_t *src, ptrdiff_t src_stride,
                           const uint16_t *pred, ptrdiff_t pred_stride) {
  subtract_16x8(diff, diff_stride, src, src_stride, pred, pred_stride);
  diff += diff_stride << 3;
  src += src_stride << 3;
  pred += pred_stride << 3;
  subtract_16x8(diff, diff_stride, src, src_stride, pred, pred_stride);
}

static void subtract_16x32(int16_t *diff, ptrdiff_t diff_stride,
                           const uint16_t *src, ptrdiff_t src_stride,
                           const uint16_t *pred, ptrdiff_t pred_stride) {
  subtract_16x16(diff, diff_stride, src, src_stride, pred, pred_stride);
  diff += diff_stride << 4;
  src += src_stride << 4;
  pred += pred_stride << 4;
  subtract_16x16(diff, diff_stride, src, src_stride, pred, pred_stride);
}

static void subtract_32x16(int16_t *diff, ptrdiff_t diff_stride,
                           const uint16_t *src, ptrdiff_t src_stride,
                           const uint16_t *pred, ptrdiff_t pred_stride) {
  subtract_16x16(diff, diff_stride, src, src_stride, pred, pred_stride);
  diff += 16;
  src += 16;
  pred += 16;
  subtract_16x16(diff, diff_stride, src, src_stride, pred, pred_stride);
}

static void subtract_32x32(int16_t *diff, ptrdiff_t diff_stride,
                           const uint16_t *src, ptrdiff_t src_stride,
                           const uint16_t *pred, ptrdiff_t pred_stride) {
  subtract_32x16(diff, diff_stride, src, src_stride, pred, pred_stride);
  diff += diff_stride << 4;
  src += src_stride << 4;
  pred += pred_stride << 4;
  subtract_32x16(diff, diff_stride, src, src_stride, pred, pred_stride);
}

static void subtract_32x64(int16_t *diff, ptrdiff_t diff_stride,
                           const uint16_t *src, ptrdiff_t src_stride,
                           const uint16_t *pred, ptrdiff_t pred_stride) {
  subtract_32x32(diff, diff_stride, src, src_stride, pred, pred_stride);
  diff += diff_stride << 5;
  src += src_stride << 5;
  pred += pred_stride << 5;
  subtract_32x32(diff, diff_stride, src, src_stride, pred, pred_stride);
}

static void subtract_64x32(int16_t *diff, ptrdiff_t diff_stride,
                           const uint16_t *src, ptrdiff_t src_stride,
                           const uint16_t *pred, ptrdiff_t pred_stride) {
  subtract_32x32(diff, diff_stride, src, src_stride, pred, pred_stride);
  diff += 32;
  src += 32;
  pred += 32;
  subtract_32x32(diff, diff_stride, src, src_stride, pred, pred_stride);
}

static void subtract_64x64(int16_t *diff, ptrdiff_t diff_stride,
                           const uint16_t *src, ptrdiff_t src_stride,
                           const uint16_t *pred, ptrdiff_t pred_stride) {
  subtract_64x32(diff, diff_stride, src, src_stride, pred, pred_stride);
  diff += diff_stride << 5;
  src += src_stride << 5;
  pred += pred_stride << 5;
  subtract_64x32(diff, diff_stride, src, src_stride, pred, pred_stride);
}

static void subtract_64x128(int16_t *diff, ptrdiff_t diff_stride,
                            const uint16_t *src, ptrdiff_t src_stride,
                            const uint16_t *pred, ptrdiff_t pred_stride) {
  subtract_64x64(diff, diff_stride, src, src_stride, pred, pred_stride);
  diff += diff_stride << 6;
  src += src_stride << 6;
  pred += pred_stride << 6;
  subtract_64x64(diff, diff_stride, src, src_stride, pred, pred_stride);
}

static void subtract_128x64(int16_t *diff, ptrdiff_t diff_stride,
                            const uint16_t *src, ptrdiff_t src_stride,
                            const uint16_t *pred, ptrdiff_t pred_stride) {
  subtract_64x64(diff, diff_stride, src, src_stride, pred, pred_stride);
  diff += 64;
  src += 64;
  pred += 64;
  subtract_64x64(diff, diff_stride, src, src_stride, pred, pred_stride);
}

static void subtract_128x128(int16_t *diff, ptrdiff_t diff_stride,
                             const uint16_t *src, ptrdiff_t src_stride,
                             const uint16_t *pred, ptrdiff_t pred_stride) {
  subtract_128x64(diff, diff_stride, src, src_stride, pred, pred_stride);
  diff += diff_stride << 6;
  src += src_stride << 6;
  pred += pred_stride << 6;
  subtract_128x64(diff, diff_stride, src, src_stride, pred, pred_stride);
}

static SubtractWxHFuncType getSubtractFunc(int rows, int cols) {
  SubtractWxHFuncType ret_func_ptr = NULL;
  if (rows == 4) {
    if (cols == 4) {
      ret_func_ptr = subtract_4x4;
    } else if (cols == 8) {
      ret_func_ptr = subtract_8x4;
    }
  } else if (rows == 8) {
    if (cols == 4) {
      ret_func_ptr = subtract_4x8;
    } else if (cols == 8) {
      ret_func_ptr = subtract_8x8;
    } else if (cols == 16) {
      ret_func_ptr = subtract_16x8;
    }
  } else if (rows == 16) {
    if (cols == 8) {
      ret_func_ptr = subtract_8x16;
    } else if (cols == 16) {
      ret_func_ptr = subtract_16x16;
    } else if (cols == 32) {
      ret_func_ptr = subtract_32x16;
    }
  } else if (rows == 32) {
    if (cols == 16) {
      ret_func_ptr = subtract_16x32;
    } else if (cols == 32) {
      ret_func_ptr = subtract_32x32;
    } else if (cols == 64) {
      ret_func_ptr = subtract_64x32;
    }
  } else if (rows == 64) {
    if (cols == 32) {
      ret_func_ptr = subtract_32x64;
    } else if (cols == 64) {
      ret_func_ptr = subtract_64x64;
    } else if (cols == 128) {
      ret_func_ptr = subtract_128x64;
    }
  } else if (rows == 128) {
    if (cols == 64) {
      ret_func_ptr = subtract_64x128;
    } else if (cols == 128) {
      ret_func_ptr = subtract_128x128;
    }
  }
  if (!ret_func_ptr) {
    assert(0);
  }
  return ret_func_ptr;
}

void aom_highbd_subtract_block_sse2(int rows, int cols, int16_t *diff,
                                    ptrdiff_t diff_stride, const uint8_t *src8,
                                    ptrdiff_t src_stride, const uint8_t *pred8,
                                    ptrdiff_t pred_stride, int bd) {
  uint16_t *src = CONVERT_TO_SHORTPTR(src8);
  uint16_t *pred = CONVERT_TO_SHORTPTR(pred8);
  SubtractWxHFuncType func;
  (void)bd;

  func = getSubtractFunc(rows, cols);
  func(diff, diff_stride, src, src_stride, pred, pred_stride);
}
