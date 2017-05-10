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

#include "./aom_dsp_rtcd.h"
#include "aom_ports/mem.h"

// src_diff: first pass, 9 bit, dynamic range [-255, 255]
//           second pass, 12 bit, dynamic range [-2040, 2040]
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

// The order of the output coeff of the hadamard is not important. For
// optimization purposes the final transpose may be skipped.
void aom_hadamard_8x8_c(const int16_t *src_diff, int src_stride,
                        int16_t *coeff) {
  int idx;
  int16_t buffer[64];
  int16_t *tmp_buf = &buffer[0];
  for (idx = 0; idx < 8; ++idx) {
    hadamard_col8(src_diff, src_stride, tmp_buf);  // src_diff: 9 bit
                                                   // dynamic range [-255, 255]
    tmp_buf += 8;
    ++src_diff;
  }

  tmp_buf = &buffer[0];
  for (idx = 0; idx < 8; ++idx) {
    hadamard_col8(tmp_buf, 8, coeff);  // tmp_buf: 12 bit
                                       // dynamic range [-2040, 2040]
    coeff += 8;                        // coeff: 15 bit
                                       // dynamic range [-16320, 16320]
    ++tmp_buf;
  }
}

// In place 16x16 2D Hadamard transform
void aom_hadamard_16x16_c(const int16_t *src_diff, int src_stride,
                          int16_t *coeff) {
  int idx;
  for (idx = 0; idx < 4; ++idx) {
    // src_diff: 9 bit, dynamic range [-255, 255]
    const int16_t *src_ptr =
        src_diff + (idx >> 1) * 8 * src_stride + (idx & 0x01) * 8;
    aom_hadamard_8x8_c(src_ptr, src_stride, coeff + idx * 64);
  }

  // coeff: 15 bit, dynamic range [-16320, 16320]
  for (idx = 0; idx < 64; ++idx) {
    int16_t a0 = coeff[0];
    int16_t a1 = coeff[64];
    int16_t a2 = coeff[128];
    int16_t a3 = coeff[192];

    int16_t b0 = (a0 + a1) >> 1;  // (a0 + a1): 16 bit, [-32640, 32640]
    int16_t b1 = (a0 - a1) >> 1;  // b0-b3: 15 bit, dynamic range
    int16_t b2 = (a2 + a3) >> 1;  // [-16320, 16320]
    int16_t b3 = (a2 - a3) >> 1;

    coeff[0] = b0 + b2;  // 16 bit, [-32640, 32640]
    coeff[64] = b1 + b3;
    coeff[128] = b0 - b2;
    coeff[192] = b1 - b3;

    ++coeff;
  }
}

// coeff: 16 bits, dynamic range [-32640, 32640].
// length: value range {16, 64, 256, 1024}.
int aom_satd_c(const int16_t *coeff, int length) {
  int i;
  int satd = 0;
  for (i = 0; i < length; ++i) satd += abs(coeff[i]);

  // satd: 26 bits, dynamic range [-32640 * 1024, 32640 * 1024]
  return satd;
}

// Integer projection onto row vectors.
// height: value range {16, 32, 64}.
void aom_int_pro_row_c(int16_t hbuf[16], const uint8_t *ref, int ref_stride,
                       int height) {
  int idx;
  const int norm_factor = height >> 1;
  for (idx = 0; idx < 16; ++idx) {
    int i;
    hbuf[idx] = 0;
    // hbuf[idx]: 14 bit, dynamic range [0, 16320].
    for (i = 0; i < height; ++i) hbuf[idx] += ref[i * ref_stride];
    // hbuf[idx]: 9 bit, dynamic range [0, 510].
    hbuf[idx] /= norm_factor;
    ++ref;
  }
}

// width: value range {16, 32, 64}.
int16_t aom_int_pro_col_c(const uint8_t *ref, int width) {
  int idx;
  int16_t sum = 0;
  // sum: 14 bit, dynamic range [0, 16320]
  for (idx = 0; idx < width; ++idx) sum += ref[idx];
  return sum;
}

// ref: [0 - 510]
// src: [0 - 510]
// bwl: {2, 3, 4}
int aom_vector_var_c(const int16_t *ref, const int16_t *src, int bwl) {
  int i;
  int width = 4 << bwl;
  int sse = 0, mean = 0, var;

  for (i = 0; i < width; ++i) {
    int diff = ref[i] - src[i];  // diff: dynamic range [-510, 510], 10 bits.
    mean += diff;                // mean: dynamic range 16 bits.
    sse += diff * diff;          // sse:  dynamic range 26 bits.
  }

  // (mean * mean): dynamic range 31 bits.
  var = sse - ((mean * mean) >> (bwl + 2));
  return var;
}

void aom_minmax_8x8_c(const uint8_t *src, int src_stride, const uint8_t *ref,
                      int ref_stride, int *min, int *max) {
  int i, j;
  *min = 255;
  *max = 0;
  for (i = 0; i < 8; ++i, src += src_stride, ref += ref_stride) {
    for (j = 0; j < 8; ++j) {
      int diff = abs(src[j] - ref[j]);
      *min = diff < *min ? diff : *min;
      *max = diff > *max ? diff : *max;
    }
  }
}

#if CONFIG_HIGHBITDEPTH
void aom_highbd_minmax_8x8_c(const uint8_t *s8, int p, const uint8_t *d8,
                             int dp, int *min, int *max) {
  int i, j;
  const uint16_t *s = CONVERT_TO_SHORTPTR(s8);
  const uint16_t *d = CONVERT_TO_SHORTPTR(d8);
  *min = 255;
  *max = 0;
  for (i = 0; i < 8; ++i, s += p, d += dp) {
    for (j = 0; j < 8; ++j) {
      int diff = abs(s[j] - d[j]);
      *min = diff < *min ? diff : *min;
      *max = diff > *max ? diff : *max;
    }
  }
}
#endif  // CONFIG_HIGHBITDEPTH
