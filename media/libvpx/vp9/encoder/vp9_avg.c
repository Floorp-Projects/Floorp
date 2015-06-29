/*
 *  Copyright (c) 2014 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "./vp9_rtcd.h"
#include "vp9/common/vp9_common.h"
#include "vpx_ports/mem.h"

unsigned int vp9_avg_8x8_c(const uint8_t *s, int p) {
  int i, j;
  int sum = 0;
  for (i = 0; i < 8; ++i, s+=p)
    for (j = 0; j < 8; sum += s[j], ++j) {}

  return (sum + 32) >> 6;
}

unsigned int vp9_avg_4x4_c(const uint8_t *s, int p) {
  int i, j;
  int sum = 0;
  for (i = 0; i < 4; ++i, s+=p)
    for (j = 0; j < 4; sum += s[j], ++j) {}

  return (sum + 8) >> 4;
}

static void hadamard_col8(const int16_t *src_diff, int src_stride,
                          int16_t *coeff) {
  int16_t b0 = src_diff[0 * src_stride] + src_diff[1 * src_stride];
  int16_t b1 = src_diff[0 * src_stride] - src_diff[1 * src_stride];
  int16_t b2 = src_diff[2 * src_stride] + src_diff[3 * src_stride];
  int16_t b3 = src_diff[2 * src_stride] - src_diff[3 * src_stride];
  int16_t b4 = src_diff[4 * src_stride] + src_diff[5 * src_stride];
  int16_t b5 = src_diff[4 * src_stride] - src_diff[5 * src_stride];
  int16_t b6 = src_diff[6 * src_stride] + src_diff[7 * src_stride];
  int16_t b7 = src_diff[6 * src_stride] - src_diff[7 * src_stride];

  int16_t c0 = b0 + b2;
  int16_t c1 = b1 + b3;
  int16_t c2 = b0 - b2;
  int16_t c3 = b1 - b3;
  int16_t c4 = b4 + b6;
  int16_t c5 = b5 + b7;
  int16_t c6 = b4 - b6;
  int16_t c7 = b5 - b7;

  coeff[0] = c0 + c4;
  coeff[7] = c1 + c5;
  coeff[3] = c2 + c6;
  coeff[4] = c3 + c7;
  coeff[2] = c0 - c4;
  coeff[6] = c1 - c5;
  coeff[1] = c2 - c6;
  coeff[5] = c3 - c7;
}

void vp9_hadamard_8x8_c(int16_t const *src_diff, int src_stride,
                        int16_t *coeff) {
  int idx;
  int16_t buffer[64];
  int16_t *tmp_buf = &buffer[0];
  for (idx = 0; idx < 8; ++idx) {
    hadamard_col8(src_diff, src_stride, tmp_buf);
    tmp_buf += 8;
    ++src_diff;
  }

  tmp_buf = &buffer[0];
  for (idx = 0; idx < 8; ++idx) {
    hadamard_col8(tmp_buf, 8, coeff);
    coeff += 8;
    ++tmp_buf;
  }
}

// In place 16x16 2D Hadamard transform
void vp9_hadamard_16x16_c(int16_t const *src_diff, int src_stride,
                          int16_t *coeff) {
  int idx;
  for (idx = 0; idx < 4; ++idx) {
    int16_t const *src_ptr = src_diff + (idx >> 1) * 8 * src_stride
                                + (idx & 0x01) * 8;
    vp9_hadamard_8x8_c(src_ptr, src_stride, coeff + idx * 64);
  }

  for (idx = 0; idx < 64; ++idx) {
    int16_t a0 = coeff[0];
    int16_t a1 = coeff[64];
    int16_t a2 = coeff[128];
    int16_t a3 = coeff[192];

    int16_t b0 = a0 + a1;
    int16_t b1 = a0 - a1;
    int16_t b2 = a2 + a3;
    int16_t b3 = a2 - a3;

    coeff[0]   = (b0 + b2) >> 1;
    coeff[64]  = (b1 + b3) >> 1;
    coeff[128] = (b0 - b2) >> 1;
    coeff[192] = (b1 - b3) >> 1;

    ++coeff;
  }
}

int16_t vp9_satd_c(const int16_t *coeff, int length) {
  int i;
  int satd = 0;
  for (i = 0; i < length; ++i)
    satd += abs(coeff[i]);

  return (int16_t)satd;
}

// Integer projection onto row vectors.
void vp9_int_pro_row_c(int16_t *hbuf, uint8_t const *ref,
                       const int ref_stride, const int height) {
  int idx;
  const int norm_factor = MAX(8, height >> 1);
  for (idx = 0; idx < 16; ++idx) {
    int i;
    hbuf[idx] = 0;
    for (i = 0; i < height; ++i)
      hbuf[idx] += ref[i * ref_stride];
    hbuf[idx] /= norm_factor;
    ++ref;
  }
}

int16_t vp9_int_pro_col_c(uint8_t const *ref, const int width) {
  int idx;
  int16_t sum = 0;
  for (idx = 0; idx < width; ++idx)
    sum += ref[idx];
  return sum;
}

int vp9_vector_var_c(int16_t const *ref, int16_t const *src,
                     const int bwl) {
  int i;
  int width = 4 << bwl;
  int sse = 0, mean = 0, var;

  for (i = 0; i < width; ++i) {
    int diff = ref[i] - src[i];
    mean += diff;
    sse += diff * diff;
  }

  var = sse - ((mean * mean) >> (bwl + 2));
  return var;
}

void vp9_minmax_8x8_c(const uint8_t *s, int p, const uint8_t *d, int dp,
                      int *min, int *max) {
  int i, j;
  *min = 255;
  *max = 0;
  for (i = 0; i < 8; ++i, s += p, d += dp) {
    for (j = 0; j < 8; ++j) {
      int diff = abs(s[j]-d[j]);
      *min = diff < *min ? diff : *min;
      *max = diff > *max ? diff : *max;
    }
  }
}

#if CONFIG_VP9_HIGHBITDEPTH
unsigned int vp9_highbd_avg_8x8_c(const uint8_t *s8, int p) {
  int i, j;
  int sum = 0;
  const uint16_t* s = CONVERT_TO_SHORTPTR(s8);
  for (i = 0; i < 8; ++i, s+=p)
    for (j = 0; j < 8; sum += s[j], ++j) {}

  return (sum + 32) >> 6;
}

unsigned int vp9_highbd_avg_4x4_c(const uint8_t *s8, int p) {
  int i, j;
  int sum = 0;
  const uint16_t* s = CONVERT_TO_SHORTPTR(s8);
  for (i = 0; i < 4; ++i, s+=p)
    for (j = 0; j < 4; sum += s[j], ++j) {}

  return (sum + 8) >> 4;
}

void vp9_highbd_minmax_8x8_c(const uint8_t *s8, int p, const uint8_t *d8,
                             int dp, int *min, int *max) {
  int i, j;
  const uint16_t* s = CONVERT_TO_SHORTPTR(s8);
  const uint16_t* d = CONVERT_TO_SHORTPTR(d8);
  *min = 255;
  *max = 0;
  for (i = 0; i < 8; ++i, s += p, d += dp) {
    for (j = 0; j < 8; ++j) {
      int diff = abs(s[j]-d[j]);
      *min = diff < *min ? diff : *min;
      *max = diff > *max ? diff : *max;
    }
  }
}
#endif  // CONFIG_VP9_HIGHBITDEPTH


