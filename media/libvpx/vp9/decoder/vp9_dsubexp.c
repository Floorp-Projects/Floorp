/*
  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "vp9/common/vp9_entropy.h"

#include "vp9/decoder/vp9_dsubexp.h"

static int inv_recenter_nonneg(int v, int m) {
  if (v > 2 * m)
    return v;

  return v % 2 ? m - (v + 1) / 2 : m + v / 2;
}

static int decode_uniform(vp9_reader *r) {
  const int l = 8;
  const int m = (1 << l) - 191;
  const int v = vp9_read_literal(r, l - 1);
  return v < m ?  v : (v << 1) - m + vp9_read_bit(r);
}

static int inv_remap_prob(int v, int m) {
  static int inv_map_table[MAX_PROB - 1] = {
      6,  19,  32,  45,  58,  71,  84,  97, 110, 123, 136, 149, 162, 175, 188,
    201, 214, 227, 240, 253,   0,   1,   2,   3,   4,   5,   7,   8,   9,  10,
     11,  12,  13,  14,  15,  16,  17,  18,  20,  21,  22,  23,  24,  25,  26,
     27,  28,  29,  30,  31,  33,  34,  35,  36,  37,  38,  39,  40,  41,  42,
     43,  44,  46,  47,  48,  49,  50,  51,  52,  53,  54,  55,  56,  57,  59,
     60,  61,  62,  63,  64,  65,  66,  67,  68,  69,  70,  72,  73,  74,  75,
     76,  77,  78,  79,  80,  81,  82,  83,  85,  86,  87,  88,  89,  90,  91,
     92,  93,  94,  95,  96,  98,  99, 100, 101, 102, 103, 104, 105, 106, 107,
    108, 109, 111, 112, 113, 114, 115, 116, 117, 118, 119, 120, 121, 122, 124,
    125, 126, 127, 128, 129, 130, 131, 132, 133, 134, 135, 137, 138, 139, 140,
    141, 142, 143, 144, 145, 146, 147, 148, 150, 151, 152, 153, 154, 155, 156,
    157, 158, 159, 160, 161, 163, 164, 165, 166, 167, 168, 169, 170, 171, 172,
    173, 174, 176, 177, 178, 179, 180, 181, 182, 183, 184, 185, 186, 187, 189,
    190, 191, 192, 193, 194, 195, 196, 197, 198, 199, 200, 202, 203, 204, 205,
    206, 207, 208, 209, 210, 211, 212, 213, 215, 216, 217, 218, 219, 220, 221,
    222, 223, 224, 225, 226, 228, 229, 230, 231, 232, 233, 234, 235, 236, 237,
    238, 239, 241, 242, 243, 244, 245, 246, 247, 248, 249, 250, 251, 252
  };
  // The clamp is not necessary for conforming VP9 stream, it is added to
  // prevent out of bound access for bad input data
  v = clamp(v, 0, 253);
  v = inv_map_table[v];
  m--;
  if ((m << 1) <= MAX_PROB) {
    return 1 + inv_recenter_nonneg(v + 1, m);
  } else {
    return MAX_PROB - inv_recenter_nonneg(v + 1, MAX_PROB - 1 - m);
  }
}

static int decode_term_subexp(vp9_reader *r) {
  if (!vp9_read_bit(r))
    return vp9_read_literal(r, 4);
  if (!vp9_read_bit(r))
    return vp9_read_literal(r, 4) + 16;
  if (!vp9_read_bit(r))
    return vp9_read_literal(r, 5) + 32;
  return decode_uniform(r) + 64;
}

void vp9_diff_update_prob(vp9_reader *r, vp9_prob* p) {
  if (vp9_read(r, DIFF_UPDATE_PROB)) {
    const int delp = decode_term_subexp(r);
    *p = (vp9_prob)inv_remap_prob(delp, *p);
  }
}
