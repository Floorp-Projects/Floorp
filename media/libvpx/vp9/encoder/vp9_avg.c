/*
 *  Copyright (c) 2014 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
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
#endif  // CONFIG_VP9_HIGHBITDEPTH


