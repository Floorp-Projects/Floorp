/*
 * Copyright (c) 2017, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */
#include "aom/aom_integer.h"
#include "av1/common/onyxc_int.h"
#include "av1/common/txb_common.h"

const int16_t av1_coeff_band_4x4[16] = { 0, 1, 2,  3,  4,  5,  6,  7,
                                         8, 9, 10, 11, 12, 13, 14, 15 };

const int16_t av1_coeff_band_8x8[64] = {
  0,  1,  2,  2,  3,  3,  4,  4,  5,  6,  2,  2,  3,  3,  4,  4,
  7,  7,  8,  8,  9,  9,  10, 10, 7,  7,  8,  8,  9,  9,  10, 10,
  11, 11, 12, 12, 13, 13, 14, 14, 11, 11, 12, 12, 13, 13, 14, 14,
  15, 15, 16, 16, 17, 17, 18, 18, 15, 15, 16, 16, 17, 17, 18, 18,
};

const int16_t av1_coeff_band_16x16[256] = {
  0,  1,  4,  4,  7,  7,  7,  7,  8,  8,  8,  8,  9,  9,  9,  9,  2,  3,  4,
  4,  7,  7,  7,  7,  8,  8,  8,  8,  9,  9,  9,  9,  5,  5,  6,  6,  7,  7,
  7,  7,  8,  8,  8,  8,  9,  9,  9,  9,  5,  5,  6,  6,  7,  7,  7,  7,  8,
  8,  8,  8,  9,  9,  9,  9,  10, 10, 10, 10, 11, 11, 11, 11, 12, 12, 12, 12,
  13, 13, 13, 13, 10, 10, 10, 10, 11, 11, 11, 11, 12, 12, 12, 12, 13, 13, 13,
  13, 10, 10, 10, 10, 11, 11, 11, 11, 12, 12, 12, 12, 13, 13, 13, 13, 10, 10,
  10, 10, 11, 11, 11, 11, 12, 12, 12, 12, 13, 13, 13, 13, 14, 14, 14, 14, 15,
  15, 15, 15, 16, 16, 16, 16, 17, 17, 17, 17, 14, 14, 14, 14, 15, 15, 15, 15,
  16, 16, 16, 16, 17, 17, 17, 17, 14, 14, 14, 14, 15, 15, 15, 15, 16, 16, 16,
  16, 17, 17, 17, 17, 14, 14, 14, 14, 15, 15, 15, 15, 16, 16, 16, 16, 17, 17,
  17, 17, 18, 18, 18, 18, 19, 19, 19, 19, 20, 20, 20, 20, 21, 21, 21, 21, 18,
  18, 18, 18, 19, 19, 19, 19, 20, 20, 20, 20, 21, 21, 21, 21, 18, 18, 18, 18,
  19, 19, 19, 19, 20, 20, 20, 20, 21, 21, 21, 21, 18, 18, 18, 18, 19, 19, 19,
  19, 20, 20, 20, 20, 21, 21, 21, 21,
};

const int16_t av1_coeff_band_32x32[1024] = {
  0,  1,  4,  4,  7,  7,  7,  7,  10, 10, 10, 10, 10, 10, 10, 10, 11, 11, 11,
  11, 11, 11, 11, 11, 12, 12, 12, 12, 12, 12, 12, 12, 2,  3,  4,  4,  7,  7,
  7,  7,  10, 10, 10, 10, 10, 10, 10, 10, 11, 11, 11, 11, 11, 11, 11, 11, 12,
  12, 12, 12, 12, 12, 12, 12, 5,  5,  6,  6,  7,  7,  7,  7,  10, 10, 10, 10,
  10, 10, 10, 10, 11, 11, 11, 11, 11, 11, 11, 11, 12, 12, 12, 12, 12, 12, 12,
  12, 5,  5,  6,  6,  7,  7,  7,  7,  10, 10, 10, 10, 10, 10, 10, 10, 11, 11,
  11, 11, 11, 11, 11, 11, 12, 12, 12, 12, 12, 12, 12, 12, 8,  8,  8,  8,  9,
  9,  9,  9,  10, 10, 10, 10, 10, 10, 10, 10, 11, 11, 11, 11, 11, 11, 11, 11,
  12, 12, 12, 12, 12, 12, 12, 12, 8,  8,  8,  8,  9,  9,  9,  9,  10, 10, 10,
  10, 10, 10, 10, 10, 11, 11, 11, 11, 11, 11, 11, 11, 12, 12, 12, 12, 12, 12,
  12, 12, 8,  8,  8,  8,  9,  9,  9,  9,  10, 10, 10, 10, 10, 10, 10, 10, 11,
  11, 11, 11, 11, 11, 11, 11, 12, 12, 12, 12, 12, 12, 12, 12, 8,  8,  8,  8,
  9,  9,  9,  9,  10, 10, 10, 10, 10, 10, 10, 10, 11, 11, 11, 11, 11, 11, 11,
  11, 12, 12, 12, 12, 12, 12, 12, 12, 13, 13, 13, 13, 13, 13, 13, 13, 14, 14,
  14, 14, 14, 14, 14, 14, 15, 15, 15, 15, 15, 15, 15, 15, 16, 16, 16, 16, 16,
  16, 16, 16, 13, 13, 13, 13, 13, 13, 13, 13, 14, 14, 14, 14, 14, 14, 14, 14,
  15, 15, 15, 15, 15, 15, 15, 15, 16, 16, 16, 16, 16, 16, 16, 16, 13, 13, 13,
  13, 13, 13, 13, 13, 14, 14, 14, 14, 14, 14, 14, 14, 15, 15, 15, 15, 15, 15,
  15, 15, 16, 16, 16, 16, 16, 16, 16, 16, 13, 13, 13, 13, 13, 13, 13, 13, 14,
  14, 14, 14, 14, 14, 14, 14, 15, 15, 15, 15, 15, 15, 15, 15, 16, 16, 16, 16,
  16, 16, 16, 16, 13, 13, 13, 13, 13, 13, 13, 13, 14, 14, 14, 14, 14, 14, 14,
  14, 15, 15, 15, 15, 15, 15, 15, 15, 16, 16, 16, 16, 16, 16, 16, 16, 13, 13,
  13, 13, 13, 13, 13, 13, 14, 14, 14, 14, 14, 14, 14, 14, 15, 15, 15, 15, 15,
  15, 15, 15, 16, 16, 16, 16, 16, 16, 16, 16, 13, 13, 13, 13, 13, 13, 13, 13,
  14, 14, 14, 14, 14, 14, 14, 14, 15, 15, 15, 15, 15, 15, 15, 15, 16, 16, 16,
  16, 16, 16, 16, 16, 13, 13, 13, 13, 13, 13, 13, 13, 14, 14, 14, 14, 14, 14,
  14, 14, 15, 15, 15, 15, 15, 15, 15, 15, 16, 16, 16, 16, 16, 16, 16, 16, 17,
  17, 17, 17, 17, 17, 17, 17, 18, 18, 18, 18, 18, 18, 18, 18, 19, 19, 19, 19,
  19, 19, 19, 19, 20, 20, 20, 20, 20, 20, 20, 20, 17, 17, 17, 17, 17, 17, 17,
  17, 18, 18, 18, 18, 18, 18, 18, 18, 19, 19, 19, 19, 19, 19, 19, 19, 20, 20,
  20, 20, 20, 20, 20, 20, 17, 17, 17, 17, 17, 17, 17, 17, 18, 18, 18, 18, 18,
  18, 18, 18, 19, 19, 19, 19, 19, 19, 19, 19, 20, 20, 20, 20, 20, 20, 20, 20,
  17, 17, 17, 17, 17, 17, 17, 17, 18, 18, 18, 18, 18, 18, 18, 18, 19, 19, 19,
  19, 19, 19, 19, 19, 20, 20, 20, 20, 20, 20, 20, 20, 17, 17, 17, 17, 17, 17,
  17, 17, 18, 18, 18, 18, 18, 18, 18, 18, 19, 19, 19, 19, 19, 19, 19, 19, 20,
  20, 20, 20, 20, 20, 20, 20, 17, 17, 17, 17, 17, 17, 17, 17, 18, 18, 18, 18,
  18, 18, 18, 18, 19, 19, 19, 19, 19, 19, 19, 19, 20, 20, 20, 20, 20, 20, 20,
  20, 17, 17, 17, 17, 17, 17, 17, 17, 18, 18, 18, 18, 18, 18, 18, 18, 19, 19,
  19, 19, 19, 19, 19, 19, 20, 20, 20, 20, 20, 20, 20, 20, 17, 17, 17, 17, 17,
  17, 17, 17, 18, 18, 18, 18, 18, 18, 18, 18, 19, 19, 19, 19, 19, 19, 19, 19,
  20, 20, 20, 20, 20, 20, 20, 20, 21, 21, 21, 21, 21, 21, 21, 21, 22, 22, 22,
  22, 22, 22, 22, 22, 23, 23, 23, 23, 23, 23, 23, 23, 24, 24, 24, 24, 24, 24,
  24, 24, 21, 21, 21, 21, 21, 21, 21, 21, 22, 22, 22, 22, 22, 22, 22, 22, 23,
  23, 23, 23, 23, 23, 23, 23, 24, 24, 24, 24, 24, 24, 24, 24, 21, 21, 21, 21,
  21, 21, 21, 21, 22, 22, 22, 22, 22, 22, 22, 22, 23, 23, 23, 23, 23, 23, 23,
  23, 24, 24, 24, 24, 24, 24, 24, 24, 21, 21, 21, 21, 21, 21, 21, 21, 22, 22,
  22, 22, 22, 22, 22, 22, 23, 23, 23, 23, 23, 23, 23, 23, 24, 24, 24, 24, 24,
  24, 24, 24, 21, 21, 21, 21, 21, 21, 21, 21, 22, 22, 22, 22, 22, 22, 22, 22,
  23, 23, 23, 23, 23, 23, 23, 23, 24, 24, 24, 24, 24, 24, 24, 24, 21, 21, 21,
  21, 21, 21, 21, 21, 22, 22, 22, 22, 22, 22, 22, 22, 23, 23, 23, 23, 23, 23,
  23, 23, 24, 24, 24, 24, 24, 24, 24, 24, 21, 21, 21, 21, 21, 21, 21, 21, 22,
  22, 22, 22, 22, 22, 22, 22, 23, 23, 23, 23, 23, 23, 23, 23, 24, 24, 24, 24,
  24, 24, 24, 24, 21, 21, 21, 21, 21, 21, 21, 21, 22, 22, 22, 22, 22, 22, 22,
  22, 23, 23, 23, 23, 23, 23, 23, 23, 24, 24, 24, 24, 24, 24, 24, 24,
};

#if LV_MAP_PROB
void av1_init_txb_probs(FRAME_CONTEXT *fc) {
  TX_SIZE tx_size;
  int plane, ctx, level;

  // Update probability models for transform block skip flag
  for (tx_size = 0; tx_size < TX_SIZES; ++tx_size) {
    for (ctx = 0; ctx < TXB_SKIP_CONTEXTS; ++ctx) {
      fc->txb_skip_cdf[tx_size][ctx][0] =
          AOM_ICDF(128 * (aom_cdf_prob)fc->txb_skip[tx_size][ctx]);
      fc->txb_skip_cdf[tx_size][ctx][1] = AOM_ICDF(32768);
      fc->txb_skip_cdf[tx_size][ctx][2] = 0;
    }
  }

  for (plane = 0; plane < PLANE_TYPES; ++plane) {
    for (ctx = 0; ctx < DC_SIGN_CONTEXTS; ++ctx) {
      fc->dc_sign_cdf[plane][ctx][0] =
          AOM_ICDF(128 * (aom_cdf_prob)fc->dc_sign[plane][ctx]);
      fc->dc_sign_cdf[plane][ctx][1] = AOM_ICDF(32768);
      fc->dc_sign_cdf[plane][ctx][2] = 0;
    }
  }

  // Update probability models for non-zero coefficient map and eob flag.
  for (tx_size = 0; tx_size < TX_SIZES; ++tx_size) {
    for (plane = 0; plane < PLANE_TYPES; ++plane) {
      for (level = 0; level < NUM_BASE_LEVELS; ++level) {
        for (ctx = 0; ctx < COEFF_BASE_CONTEXTS; ++ctx) {
          fc->coeff_base_cdf[tx_size][plane][level][ctx][0] = AOM_ICDF(
              128 * (aom_cdf_prob)fc->coeff_base[tx_size][plane][level][ctx]);
          fc->coeff_base_cdf[tx_size][plane][level][ctx][1] = AOM_ICDF(32768);
          fc->coeff_base_cdf[tx_size][plane][level][ctx][2] = 0;
        }
      }
    }
  }

  for (tx_size = 0; tx_size < TX_SIZES; ++tx_size) {
    for (plane = 0; plane < PLANE_TYPES; ++plane) {
      for (ctx = 0; ctx < SIG_COEF_CONTEXTS; ++ctx) {
        fc->nz_map_cdf[tx_size][plane][ctx][0] =
            AOM_ICDF(128 * (aom_cdf_prob)fc->nz_map[tx_size][plane][ctx]);
        fc->nz_map_cdf[tx_size][plane][ctx][1] = AOM_ICDF(32768);
        fc->nz_map_cdf[tx_size][plane][ctx][2] = 0;
      }

      for (ctx = 0; ctx < EOB_COEF_CONTEXTS; ++ctx) {
        fc->eob_flag_cdf[tx_size][plane][ctx][0] =
            AOM_ICDF(128 * (aom_cdf_prob)fc->eob_flag[tx_size][plane][ctx]);
        fc->eob_flag_cdf[tx_size][plane][ctx][1] = AOM_ICDF(32768);
        fc->eob_flag_cdf[tx_size][plane][ctx][2] = 0;
      }
    }
  }

  for (tx_size = 0; tx_size < TX_SIZES; ++tx_size) {
    for (plane = 0; plane < PLANE_TYPES; ++plane) {
      for (ctx = 0; ctx < LEVEL_CONTEXTS; ++ctx) {
        fc->coeff_lps_cdf[tx_size][plane][ctx][0] =
            AOM_ICDF(128 * (aom_cdf_prob)fc->coeff_lps[tx_size][plane][ctx]);
        fc->coeff_lps_cdf[tx_size][plane][ctx][1] = AOM_ICDF(32768);
        fc->coeff_lps_cdf[tx_size][plane][ctx][2] = 0;
      }
#if BR_NODE
      for (int br = 0; br < BASE_RANGE_SETS; ++br) {
        for (ctx = 0; ctx < LEVEL_CONTEXTS; ++ctx) {
          fc->coeff_br_cdf[tx_size][plane][br][ctx][0] = AOM_ICDF(
              128 * (aom_cdf_prob)fc->coeff_br[tx_size][plane][br][ctx]);
          fc->coeff_br_cdf[tx_size][plane][br][ctx][1] = AOM_ICDF(32768);
          fc->coeff_br_cdf[tx_size][plane][br][ctx][2] = 0;
        }
      }
#endif  // BR_NODE
    }
  }
#if CONFIG_CTX1D
  for (tx_size = 0; tx_size < TX_SIZES; ++tx_size) {
    for (plane = 0; plane < PLANE_TYPES; ++plane) {
      for (int tx_class = 0; tx_class < TX_CLASSES; ++tx_class) {
        fc->eob_mode_cdf[tx_size][plane][tx_class][0] = AOM_ICDF(
            128 * (aom_cdf_prob)fc->eob_mode[tx_size][plane][tx_class]);
        fc->eob_mode_cdf[tx_size][plane][tx_class][1] = AOM_ICDF(32768);
        fc->eob_mode_cdf[tx_size][plane][tx_class][2] = 0;
      }
    }
  }
  for (tx_size = 0; tx_size < TX_SIZES; ++tx_size) {
    for (plane = 0; plane < PLANE_TYPES; ++plane) {
      for (int tx_class = 0; tx_class < TX_CLASSES; ++tx_class) {
        for (ctx = 0; ctx < EMPTY_LINE_CONTEXTS; ++ctx) {
          fc->empty_line_cdf[tx_size][plane][tx_class][ctx][0] = AOM_ICDF(
              128 *
              (aom_cdf_prob)fc->empty_line[tx_size][plane][tx_class][ctx]);
          fc->empty_line_cdf[tx_size][plane][tx_class][ctx][1] =
              AOM_ICDF(32768);
          fc->empty_line_cdf[tx_size][plane][tx_class][ctx][2] = 0;
        }
      }
    }
  }
  for (tx_size = 0; tx_size < TX_SIZES; ++tx_size) {
    for (plane = 0; plane < PLANE_TYPES; ++plane) {
      for (int tx_class = 0; tx_class < TX_CLASSES; ++tx_class) {
        for (ctx = 0; ctx < HV_EOB_CONTEXTS; ++ctx) {
          fc->hv_eob_cdf[tx_size][plane][tx_class][ctx][0] = AOM_ICDF(
              128 * (aom_cdf_prob)fc->hv_eob[tx_size][plane][tx_class][ctx]);
          fc->hv_eob_cdf[tx_size][plane][tx_class][ctx][1] = AOM_ICDF(32768);
          fc->hv_eob_cdf[tx_size][plane][tx_class][ctx][2] = 0;
        }
      }
    }
  }
#endif  // CONFIG_CTX1D
}
#endif  // LV_MAP_PROB

void av1_adapt_txb_probs(AV1_COMMON *cm, unsigned int count_sat,
                         unsigned int update_factor) {
  FRAME_CONTEXT *fc = cm->fc;
  const FRAME_CONTEXT *pre_fc = cm->pre_fc;
  const FRAME_COUNTS *counts = &cm->counts;
  TX_SIZE tx_size;
  int plane, ctx, level;

  // Update probability models for transform block skip flag
  for (tx_size = 0; tx_size < TX_SIZES; ++tx_size)
    for (ctx = 0; ctx < TXB_SKIP_CONTEXTS; ++ctx)
      fc->txb_skip[tx_size][ctx] = mode_mv_merge_probs(
          pre_fc->txb_skip[tx_size][ctx], counts->txb_skip[tx_size][ctx]);

  for (plane = 0; plane < PLANE_TYPES; ++plane)
    for (ctx = 0; ctx < DC_SIGN_CONTEXTS; ++ctx)
      fc->dc_sign[plane][ctx] = mode_mv_merge_probs(
          pre_fc->dc_sign[plane][ctx], counts->dc_sign[plane][ctx]);

  // Update probability models for non-zero coefficient map and eob flag.
  for (tx_size = 0; tx_size < TX_SIZES; ++tx_size)
    for (plane = 0; plane < PLANE_TYPES; ++plane)
      for (level = 0; level < NUM_BASE_LEVELS; ++level)
        for (ctx = 0; ctx < COEFF_BASE_CONTEXTS; ++ctx)
          fc->coeff_base[tx_size][plane][level][ctx] =
              merge_probs(pre_fc->coeff_base[tx_size][plane][level][ctx],
                          counts->coeff_base[tx_size][plane][level][ctx],
                          count_sat, update_factor);

  for (tx_size = 0; tx_size < TX_SIZES; ++tx_size) {
    for (plane = 0; plane < PLANE_TYPES; ++plane) {
      for (ctx = 0; ctx < SIG_COEF_CONTEXTS; ++ctx) {
        fc->nz_map[tx_size][plane][ctx] = merge_probs(
            pre_fc->nz_map[tx_size][plane][ctx],
            counts->nz_map[tx_size][plane][ctx], count_sat, update_factor);
      }

      for (ctx = 0; ctx < EOB_COEF_CONTEXTS; ++ctx) {
        fc->eob_flag[tx_size][plane][ctx] = merge_probs(
            pre_fc->eob_flag[tx_size][plane][ctx],
            counts->eob_flag[tx_size][plane][ctx], count_sat, update_factor);
      }
    }
  }

  for (tx_size = 0; tx_size < TX_SIZES; ++tx_size) {
    for (plane = 0; plane < PLANE_TYPES; ++plane) {
      for (ctx = 0; ctx < LEVEL_CONTEXTS; ++ctx) {
        fc->coeff_lps[tx_size][plane][ctx] = merge_probs(
            pre_fc->coeff_lps[tx_size][plane][ctx],
            counts->coeff_lps[tx_size][plane][ctx], count_sat, update_factor);
      }
#if BR_NODE
      for (int br = 0; br < BASE_RANGE_SETS; ++br) {
        for (ctx = 0; ctx < LEVEL_CONTEXTS; ++ctx) {
          fc->coeff_br[tx_size][plane][br][ctx] =
              merge_probs(pre_fc->coeff_br[tx_size][plane][br][ctx],
                          counts->coeff_br[tx_size][plane][br][ctx], count_sat,
                          update_factor);
        }
      }
#endif  // BR_NODE
    }
  }
#if CONFIG_CTX1D
  for (tx_size = 0; tx_size < TX_SIZES; ++tx_size) {
    for (plane = 0; plane < PLANE_TYPES; ++plane)
      for (int tx_class = 0; tx_class < TX_CLASSES; ++tx_class)
        fc->eob_mode[tx_size][plane][tx_class] =
            merge_probs(pre_fc->eob_mode[tx_size][plane][tx_class],
                        counts->eob_mode[tx_size][plane][tx_class], count_sat,
                        update_factor);
  }
  for (tx_size = 0; tx_size < TX_SIZES; ++tx_size) {
    for (plane = 0; plane < PLANE_TYPES; ++plane)
      for (int tx_class = 0; tx_class < TX_CLASSES; ++tx_class)
        for (ctx = 0; ctx < EMPTY_LINE_CONTEXTS; ++ctx)
          fc->empty_line[tx_size][plane][tx_class][ctx] =
              merge_probs(pre_fc->empty_line[tx_size][plane][tx_class][ctx],
                          counts->empty_line[tx_size][plane][tx_class][ctx],
                          count_sat, update_factor);
  }
  for (tx_size = 0; tx_size < TX_SIZES; ++tx_size) {
    for (plane = 0; plane < PLANE_TYPES; ++plane)
      for (int tx_class = 0; tx_class < TX_CLASSES; ++tx_class)
        for (ctx = 0; ctx < HV_EOB_CONTEXTS; ++ctx)
          fc->hv_eob[tx_size][plane][tx_class][ctx] =
              merge_probs(pre_fc->hv_eob[tx_size][plane][tx_class][ctx],
                          counts->hv_eob[tx_size][plane][tx_class][ctx],
                          count_sat, update_factor);
  }
#endif
}

void av1_init_lv_map(AV1_COMMON *cm) {
  LV_MAP_CTX_TABLE *coeff_ctx_table = &cm->coeff_ctx_table;
  for (int row = 0; row < 2; ++row) {
    for (int col = 0; col < 2; ++col) {
      for (int sig_mag = 0; sig_mag < 2; ++sig_mag) {
        for (int count = 0; count < BASE_CONTEXT_POSITION_NUM + 1; ++count) {
          coeff_ctx_table->base_ctx_table[row][col][sig_mag][count] =
              get_base_ctx_from_count_mag(row, col, count, sig_mag);
        }
      }
    }
  }
}
