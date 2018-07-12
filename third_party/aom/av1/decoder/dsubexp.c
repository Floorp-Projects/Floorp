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

#include "av1/common/entropy.h"

#include "av1/decoder/dsubexp.h"

static int inv_recenter_nonneg(int v, int m) {
  if (v > 2 * m) return v;

  return (v & 1) ? m - ((v + 1) >> 1) : m + (v >> 1);
}

#define decode_uniform(r, ACCT_STR_NAME) \
  decode_uniform_(r ACCT_STR_ARG(ACCT_STR_NAME))
#define decode_term_subexp(r, ACCT_STR_NAME) \
  decode_term_subexp_(r ACCT_STR_ARG(ACCT_STR_NAME))

static int decode_uniform_(aom_reader *r ACCT_STR_PARAM) {
  const int l = 8;
  const int m = (1 << l) - 190;
  const int v = aom_read_literal(r, l - 1, ACCT_STR_NAME);
  return v < m ? v : (v << 1) - m + aom_read_bit(r, ACCT_STR_NAME);
}

static int inv_remap_prob(int v, int m) {
  /* clang-format off */
  static uint8_t inv_map_table[MAX_PROB - 1] = {
      7,  20,  33,  46,  59,  72,  85,  98, 111, 124, 137, 150, 163, 176, 189,
    202, 215, 228, 241, 254,   1,   2,   3,   4,   5,   6,   8,   9,  10,  11,
     12,  13,  14,  15,  16,  17,  18,  19,  21,  22,  23,  24,  25,  26,  27,
     28,  29,  30,  31,  32,  34,  35,  36,  37,  38,  39,  40,  41,  42,  43,
     44,  45,  47,  48,  49,  50,  51,  52,  53,  54,  55,  56,  57,  58,  60,
     61,  62,  63,  64,  65,  66,  67,  68,  69,  70,  71,  73,  74,  75,  76,
     77,  78,  79,  80,  81,  82,  83,  84,  86,  87,  88,  89,  90,  91,  92,
     93,  94,  95,  96,  97,  99, 100, 101, 102, 103, 104, 105, 106, 107, 108,
    109, 110, 112, 113, 114, 115, 116, 117, 118, 119, 120, 121, 122, 123, 125,
    126, 127, 128, 129, 130, 131, 132, 133, 134, 135, 136, 138, 139, 140, 141,
    142, 143, 144, 145, 146, 147, 148, 149, 151, 152, 153, 154, 155, 156, 157,
    158, 159, 160, 161, 162, 164, 165, 166, 167, 168, 169, 170, 171, 172, 173,
    174, 175, 177, 178, 179, 180, 181, 182, 183, 184, 185, 186, 187, 188, 190,
    191, 192, 193, 194, 195, 196, 197, 198, 199, 200, 201, 203, 204, 205, 206,
    207, 208, 209, 210, 211, 212, 213, 214, 216, 217, 218, 219, 220, 221, 222,
    223, 224, 225, 226, 227, 229, 230, 231, 232, 233, 234, 235, 236, 237, 238,
    239, 240, 242, 243, 244, 245, 246, 247, 248, 249, 250, 251, 252, 253
  }; /* clang-format on */
  assert(v < (int)(sizeof(inv_map_table) / sizeof(inv_map_table[0])));
  v = inv_map_table[v];
  m--;
  if ((m << 1) <= MAX_PROB) {
    return 1 + inv_recenter_nonneg(v, m);
  } else {
    return MAX_PROB - inv_recenter_nonneg(v, MAX_PROB - 1 - m);
  }
}

static int decode_term_subexp_(aom_reader *r ACCT_STR_PARAM) {
  if (!aom_read_bit(r, ACCT_STR_NAME))
    return aom_read_literal(r, 4, ACCT_STR_NAME);
  if (!aom_read_bit(r, ACCT_STR_NAME))
    return aom_read_literal(r, 4, ACCT_STR_NAME) + 16;
  if (!aom_read_bit(r, ACCT_STR_NAME))
    return aom_read_literal(r, 5, ACCT_STR_NAME) + 32;
  return decode_uniform(r, ACCT_STR_NAME) + 64;
}

void av1_diff_update_prob_(aom_reader *r, aom_prob *p ACCT_STR_PARAM) {
  if (aom_read(r, DIFF_UPDATE_PROB, ACCT_STR_NAME)) {
    const int delp = decode_term_subexp(r, ACCT_STR_NAME);
    *p = (aom_prob)inv_remap_prob(delp, *p);
  }
}
