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

#ifndef AV1_COMMON_TXB_COMMON_H_
#define AV1_COMMON_TXB_COMMON_H_

extern const int16_t k_eob_group_start[12];
extern const int16_t k_eob_offset_bits[12];

extern const int8_t av1_coeff_band_4x4[16];

extern const int8_t av1_coeff_band_8x8[64];

extern const int8_t av1_coeff_band_16x16[256];

extern const int8_t av1_coeff_band_32x32[1024];

extern const int8_t *av1_nz_map_ctx_offset[TX_SIZES_ALL];

typedef struct txb_ctx {
  int txb_skip_ctx;
  int dc_sign_ctx;
} TXB_CTX;

static const int base_level_count_to_index[13] = {
  0, 0, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3,
};

// Note: TX_PAD_2D is dependent to this offset table.
static const int base_ref_offset[BASE_CONTEXT_POSITION_NUM][2] = {
  /* clang-format off*/
  { -2, 0 }, { -1, -1 }, { -1, 0 }, { -1, 1 }, { 0, -2 }, { 0, -1 }, { 0, 1 },
  { 0, 2 },  { 1, -1 },  { 1, 0 },  { 1, 1 },  { 2, 0 }
  /* clang-format on*/
};

#define CONTEXT_MAG_POSITION_NUM 3
static const int mag_ref_offset_with_txclass[3][CONTEXT_MAG_POSITION_NUM][2] = {
  { { 0, 1 }, { 1, 0 }, { 1, 1 } },
  { { 0, 1 }, { 1, 0 }, { 0, 2 } },
  { { 0, 1 }, { 1, 0 }, { 2, 0 } }
};
static const int mag_ref_offset[CONTEXT_MAG_POSITION_NUM][2] = {
  { 0, 1 }, { 1, 0 }, { 1, 1 }
};

static const TX_CLASS tx_type_to_class[TX_TYPES] = {
  TX_CLASS_2D,     // DCT_DCT
  TX_CLASS_2D,     // ADST_DCT
  TX_CLASS_2D,     // DCT_ADST
  TX_CLASS_2D,     // ADST_ADST
  TX_CLASS_2D,     // FLIPADST_DCT
  TX_CLASS_2D,     // DCT_FLIPADST
  TX_CLASS_2D,     // FLIPADST_FLIPADST
  TX_CLASS_2D,     // ADST_FLIPADST
  TX_CLASS_2D,     // FLIPADST_ADST
  TX_CLASS_2D,     // IDTX
  TX_CLASS_VERT,   // V_DCT
  TX_CLASS_HORIZ,  // H_DCT
  TX_CLASS_VERT,   // V_ADST
  TX_CLASS_HORIZ,  // H_ADST
  TX_CLASS_VERT,   // V_FLIPADST
  TX_CLASS_HORIZ,  // H_FLIPADST
};

static const int8_t eob_to_pos_small[33] = {
  0, 1, 2,                                        // 0-2
  3, 3,                                           // 3-4
  4, 4, 4, 4,                                     // 5-8
  5, 5, 5, 5, 5, 5, 5, 5,                         // 9-16
  6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6  // 17-32
};

static const int8_t eob_to_pos_large[17] = {
  6,                               // place holder
  7,                               // 33-64
  8,  8,                           // 65-128
  9,  9,  9,  9,                   // 129-256
  10, 10, 10, 10, 10, 10, 10, 10,  // 257-512
  11                               // 513-
};

static INLINE int get_eob_pos_token(const int eob, int *const extra) {
  int t;

  if (eob < 33) {
    t = eob_to_pos_small[eob];
  } else {
    const int e = AOMMIN((eob - 1) >> 5, 16);
    t = eob_to_pos_large[e];
  }

  *extra = eob - k_eob_group_start[t];

  return t;
}

static INLINE int av1_get_eob_pos_ctx(const TX_TYPE tx_type,
                                      const int eob_token) {
  static const int8_t tx_type_to_offset[TX_TYPES] = {
    -1,  // DCT_DCT
    -1,  // ADST_DCT
    -1,  // DCT_ADST
    -1,  // ADST_ADST
    -1,  // FLIPADST_DCT
    -1,  // DCT_FLIPADST
    -1,  // FLIPADST_FLIPADST
    -1,  // ADST_FLIPADST
    -1,  // FLIPADST_ADST
    -1,  // IDTX
    10,  // V_DCT
    10,  // H_DCT
    10,  // V_ADST
    10,  // H_ADST
    10,  // V_FLIPADST
    10,  // H_FLIPADST
  };
  return eob_token + tx_type_to_offset[tx_type];
}

static INLINE int get_txb_bwl(TX_SIZE tx_size) {
  tx_size = av1_get_adjusted_tx_size(tx_size);
  return tx_size_wide_log2[tx_size];
}

static INLINE int get_txb_wide(TX_SIZE tx_size) {
  tx_size = av1_get_adjusted_tx_size(tx_size);
  return tx_size_wide[tx_size];
}

static INLINE int get_txb_high(TX_SIZE tx_size) {
  tx_size = av1_get_adjusted_tx_size(tx_size);
  return tx_size_high[tx_size];
}

static INLINE void get_base_count_mag(int *mag, int *count,
                                      const tran_low_t *tcoeffs, int bwl,
                                      int height, int row, int col) {
  mag[0] = 0;
  mag[1] = 0;
  for (int i = 0; i < NUM_BASE_LEVELS; ++i) count[i] = 0;
  for (int idx = 0; idx < BASE_CONTEXT_POSITION_NUM; ++idx) {
    const int ref_row = row + base_ref_offset[idx][0];
    const int ref_col = col + base_ref_offset[idx][1];
    if (ref_row < 0 || ref_col < 0 || ref_row >= height ||
        ref_col >= (1 << bwl))
      continue;
    const int pos = (ref_row << bwl) + ref_col;
    tran_low_t abs_coeff = abs(tcoeffs[pos]);
    // count
    for (int i = 0; i < NUM_BASE_LEVELS; ++i) {
      count[i] += abs_coeff > i;
    }
    // mag
    if (base_ref_offset[idx][0] >= 0 && base_ref_offset[idx][1] >= 0) {
      if (abs_coeff > mag[0]) {
        mag[0] = abs_coeff;
        mag[1] = 1;
      } else if (abs_coeff == mag[0]) {
        ++mag[1];
      }
    }
  }
}

static INLINE uint8_t *set_levels(uint8_t *const levels_buf, const int width) {
  return levels_buf + TX_PAD_TOP * (width + TX_PAD_HOR);
}

static INLINE int get_padded_idx(const int idx, const int bwl) {
  return idx + ((idx >> bwl) << TX_PAD_HOR_LOG2);
}

static INLINE int get_level_count(const uint8_t *const levels, const int stride,
                                  const int row, const int col, const int level,
                                  const int (*nb_offset)[2], const int nb_num) {
  int count = 0;

  for (int idx = 0; idx < nb_num; ++idx) {
    const int ref_row = row + nb_offset[idx][0];
    const int ref_col = col + nb_offset[idx][1];
    const int pos = ref_row * stride + ref_col;
    count += levels[pos] > level;
  }
  return count;
}

static INLINE void get_level_mag(const uint8_t *const levels, const int stride,
                                 const int row, const int col, int *const mag) {
  for (int idx = 0; idx < CONTEXT_MAG_POSITION_NUM; ++idx) {
    const int ref_row = row + mag_ref_offset[idx][0];
    const int ref_col = col + mag_ref_offset[idx][1];
    const int pos = ref_row * stride + ref_col;
    mag[idx] = levels[pos];
  }
}

static INLINE int get_base_ctx_from_count_mag(int row, int col, int count,
                                              int sig_mag) {
  const int ctx = base_level_count_to_index[count];
  int ctx_idx = -1;

  if (row == 0 && col == 0) {
    if (sig_mag >= 2) return ctx_idx = 0;
    if (sig_mag == 1) {
      if (count >= 2)
        ctx_idx = 1;
      else
        ctx_idx = 2;

      return ctx_idx;
    }

    ctx_idx = 3 + ctx;
    assert(ctx_idx <= 6);
    return ctx_idx;
  } else if (row == 0) {
    if (sig_mag >= 2) return ctx_idx = 6;
    if (sig_mag == 1) {
      if (count >= 2)
        ctx_idx = 7;
      else
        ctx_idx = 8;
      return ctx_idx;
    }

    ctx_idx = 9 + ctx;
    assert(ctx_idx <= 11);
    return ctx_idx;
  } else if (col == 0) {
    if (sig_mag >= 2) return ctx_idx = 12;
    if (sig_mag == 1) {
      if (count >= 2)
        ctx_idx = 13;
      else
        ctx_idx = 14;

      return ctx_idx;
    }

    ctx_idx = 15 + ctx;
    assert(ctx_idx <= 17);
    // TODO(angiebird): turn this on once the optimization is finalized
    // assert(ctx_idx < 28);
  } else {
    if (sig_mag >= 2) return ctx_idx = 18;
    if (sig_mag == 1) {
      if (count >= 2)
        ctx_idx = 19;
      else
        ctx_idx = 20;
      return ctx_idx;
    }

    ctx_idx = 21 + ctx;

    assert(ctx_idx <= 24);
  }
  return ctx_idx;
}

static INLINE int get_base_ctx(const uint8_t *const levels,
                               const int c,  // raster order
                               const int bwl, const int level_minus_1,
                               const int count) {
  const int row = c >> bwl;
  const int col = c - (row << bwl);
  const int stride = (1 << bwl) + TX_PAD_HOR;
  int mag_count = 0;
  int nb_mag[3] = { 0 };

  get_level_mag(levels, stride, row, col, nb_mag);

  for (int idx = 0; idx < 3; ++idx)
    mag_count += nb_mag[idx] > (level_minus_1 + 1);
  const int ctx_idx =
      get_base_ctx_from_count_mag(row, col, count, AOMMIN(2, mag_count));
  return ctx_idx;
}

#define BR_CONTEXT_POSITION_NUM 8  // Base range coefficient context
// Note: TX_PAD_2D is dependent to this offset table.
static const int br_ref_offset[BR_CONTEXT_POSITION_NUM][2] = {
  /* clang-format off*/
  { -1, -1 }, { -1, 0 }, { -1, 1 }, { 0, -1 },
  { 0, 1 },   { 1, -1 }, { 1, 0 },  { 1, 1 },
  /* clang-format on*/
};

static const int br_level_map[9] = {
  0, 0, 1, 1, 2, 2, 3, 3, 3,
};

// Note: If BR_MAG_OFFSET changes, the calculation of offset in
// get_br_ctx_from_count_mag() must be updated.
#define BR_MAG_OFFSET 1
// TODO(angiebird): optimize this function by using a table to map from
// count/mag to ctx

static INLINE int get_br_count_mag(int *mag, const tran_low_t *tcoeffs, int bwl,
                                   int height, int row, int col, int level) {
  mag[0] = 0;
  mag[1] = 0;
  int count = 0;
  for (int idx = 0; idx < BR_CONTEXT_POSITION_NUM; ++idx) {
    const int ref_row = row + br_ref_offset[idx][0];
    const int ref_col = col + br_ref_offset[idx][1];
    if (ref_row < 0 || ref_col < 0 || ref_row >= height ||
        ref_col >= (1 << bwl))
      continue;
    const int pos = (ref_row << bwl) + ref_col;
    tran_low_t abs_coeff = abs(tcoeffs[pos]);
    count += abs_coeff > level;
    if (br_ref_offset[idx][0] >= 0 && br_ref_offset[idx][1] >= 0) {
      if (abs_coeff > mag[0]) {
        mag[0] = abs_coeff;
        mag[1] = 1;
      } else if (abs_coeff == mag[0]) {
        ++mag[1];
      }
    }
  }
  return count;
}

static INLINE int get_br_ctx_from_count_mag(const int row, const int col,
                                            const int count, const int mag) {
  // DC: 0 - 1
  // Top row: 2 - 4
  // Left column: 5 - 7
  // others: 8 - 11
  static const int offset_pos[2][2] = { { 8, 5 }, { 2, 0 } };
  const int mag_clamp = AOMMIN(mag, 6);
  const int offset = mag_clamp >> 1;
  const int ctx =
      br_level_map[count] + offset * BR_TMP_OFFSET + offset_pos[!row][!col];
  return ctx;
}

static INLINE int get_br_ctx_2d(const uint8_t *const levels,
                                const int c,  // raster order
                                const int bwl) {
  assert(c > 0);
  const int row = c >> bwl;
  const int col = c - (row << bwl);
  const int stride = (1 << bwl) + TX_PAD_HOR;
  const int pos = row * stride + col;
  int mag = AOMMIN(levels[pos + 1], MAX_BASE_BR_RANGE) +
            AOMMIN(levels[pos + stride], MAX_BASE_BR_RANGE) +
            AOMMIN(levels[pos + 1 + stride], MAX_BASE_BR_RANGE);
  mag = AOMMIN((mag + 1) >> 1, 6);
  //((row | col) < 2) is equivalent to ((row < 2) && (col < 2))
  if ((row | col) < 2) return mag + 7;
  return mag + 14;
}

static AOM_FORCE_INLINE int get_br_ctx(const uint8_t *const levels,
                                       const int c,  // raster order
                                       const int bwl, const TX_CLASS tx_class) {
  const int row = c >> bwl;
  const int col = c - (row << bwl);
  const int stride = (1 << bwl) + TX_PAD_HOR;
  const int pos = row * stride + col;
  int mag = levels[pos + 1];
  mag += levels[pos + stride];
  switch (tx_class) {
    case TX_CLASS_2D:
      mag += levels[pos + stride + 1];
      mag = AOMMIN((mag + 1) >> 1, 6);
      if (c == 0) return mag;
      if ((row < 2) && (col < 2)) return mag + 7;
      break;
    case TX_CLASS_HORIZ:
      mag += levels[pos + 2];
      mag = AOMMIN((mag + 1) >> 1, 6);
      if (c == 0) return mag;
      if (col == 0) return mag + 7;
      break;
    case TX_CLASS_VERT:
      mag += levels[pos + (stride << 1)];
      mag = AOMMIN((mag + 1) >> 1, 6);
      if (c == 0) return mag;
      if (row == 0) return mag + 7;
      break;
    default: break;
  }

  return mag + 14;
}

#define SIG_REF_OFFSET_NUM 5

// Note: TX_PAD_2D is dependent to these offset tables.
static const int sig_ref_offset[SIG_REF_OFFSET_NUM][2] = {
  { 0, 1 }, { 1, 0 }, { 1, 1 }, { 0, 2 }, { 2, 0 }
  // , { 1, 2 }, { 2, 1 },
};

static const int sig_ref_offset_vert[SIG_REF_OFFSET_NUM][2] = {
  { 1, 0 }, { 2, 0 }, { 0, 1 }, { 3, 0 }, { 4, 0 }
  // , { 1, 1 }, { 2, 1 },
};

static const int sig_ref_offset_horiz[SIG_REF_OFFSET_NUM][2] = {
  { 0, 1 }, { 0, 2 }, { 1, 0 }, { 0, 3 }, { 0, 4 }
  // , { 1, 1 }, { 1, 2 },
};

#define SIG_REF_DIFF_OFFSET_NUM 3

static const int sig_ref_diff_offset[SIG_REF_DIFF_OFFSET_NUM][2] = {
  { 1, 1 }, { 0, 2 }, { 2, 0 }
};

static const int sig_ref_diff_offset_vert[SIG_REF_DIFF_OFFSET_NUM][2] = {
  { 2, 0 }, { 3, 0 }, { 4, 0 }
};

static const int sig_ref_diff_offset_horiz[SIG_REF_DIFF_OFFSET_NUM][2] = {
  { 0, 2 }, { 0, 3 }, { 0, 4 }
};

static const uint8_t clip_max3[256] = {
  0, 1, 2, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
  3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
  3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
  3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
  3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
  3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
  3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
  3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
  3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
  3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3
};

static AOM_FORCE_INLINE int get_nz_mag(const uint8_t *const levels,
                                       const int bwl, const TX_CLASS tx_class) {
  int mag;

  // Note: AOMMIN(level, 3) is useless for decoder since level < 3.
  mag = clip_max3[levels[1]];                         // { 0, 1 }
  mag += clip_max3[levels[(1 << bwl) + TX_PAD_HOR]];  // { 1, 0 }

  if (tx_class == TX_CLASS_2D) {
    mag += clip_max3[levels[(1 << bwl) + TX_PAD_HOR + 1]];          // { 1, 1 }
    mag += clip_max3[levels[2]];                                    // { 0, 2 }
    mag += clip_max3[levels[(2 << bwl) + (2 << TX_PAD_HOR_LOG2)]];  // { 2, 0 }
  } else if (tx_class == TX_CLASS_VERT) {
    mag += clip_max3[levels[(2 << bwl) + (2 << TX_PAD_HOR_LOG2)]];  // { 2, 0 }
    mag += clip_max3[levels[(3 << bwl) + (3 << TX_PAD_HOR_LOG2)]];  // { 3, 0 }
    mag += clip_max3[levels[(4 << bwl) + (4 << TX_PAD_HOR_LOG2)]];  // { 4, 0 }
  } else {
    mag += clip_max3[levels[2]];  // { 0, 2 }
    mag += clip_max3[levels[3]];  // { 0, 3 }
    mag += clip_max3[levels[4]];  // { 0, 4 }
  }

  return mag;
}

static INLINE int get_nz_count(const uint8_t *const levels, const int bwl,
                               const TX_CLASS tx_class) {
  int count;

  count = (levels[1] != 0);                         // { 0, 1 }
  count += (levels[(1 << bwl) + TX_PAD_HOR] != 0);  // { 1, 0 }

  for (int idx = 0; idx < SIG_REF_DIFF_OFFSET_NUM; ++idx) {
    const int row_offset =
        ((tx_class == TX_CLASS_2D) ? sig_ref_diff_offset[idx][0]
                                   : ((tx_class == TX_CLASS_VERT)
                                          ? sig_ref_diff_offset_vert[idx][0]
                                          : sig_ref_diff_offset_horiz[idx][0]));
    const int col_offset =
        ((tx_class == TX_CLASS_2D) ? sig_ref_diff_offset[idx][1]
                                   : ((tx_class == TX_CLASS_VERT)
                                          ? sig_ref_diff_offset_vert[idx][1]
                                          : sig_ref_diff_offset_horiz[idx][1]));
    const int nb_pos =
        (row_offset << bwl) + (row_offset << TX_PAD_HOR_LOG2) + col_offset;
    count += (levels[nb_pos] != 0);
  }
  return count;
}

#define NZ_MAP_CTX_0 SIG_COEF_CONTEXTS_2D
#define NZ_MAP_CTX_5 (NZ_MAP_CTX_0 + 5)
#define NZ_MAP_CTX_10 (NZ_MAP_CTX_0 + 10)

static const int nz_map_ctx_offset_1d[32] = {
  NZ_MAP_CTX_0,  NZ_MAP_CTX_5,  NZ_MAP_CTX_10, NZ_MAP_CTX_10, NZ_MAP_CTX_10,
  NZ_MAP_CTX_10, NZ_MAP_CTX_10, NZ_MAP_CTX_10, NZ_MAP_CTX_10, NZ_MAP_CTX_10,
  NZ_MAP_CTX_10, NZ_MAP_CTX_10, NZ_MAP_CTX_10, NZ_MAP_CTX_10, NZ_MAP_CTX_10,
  NZ_MAP_CTX_10, NZ_MAP_CTX_10, NZ_MAP_CTX_10, NZ_MAP_CTX_10, NZ_MAP_CTX_10,
  NZ_MAP_CTX_10, NZ_MAP_CTX_10, NZ_MAP_CTX_10, NZ_MAP_CTX_10, NZ_MAP_CTX_10,
  NZ_MAP_CTX_10, NZ_MAP_CTX_10, NZ_MAP_CTX_10, NZ_MAP_CTX_10, NZ_MAP_CTX_10,
  NZ_MAP_CTX_10, NZ_MAP_CTX_10,
};

static AOM_FORCE_INLINE int get_nz_map_ctx_from_stats(
    const int stats,
    const int coeff_idx,  // raster order
    const int bwl, const TX_SIZE tx_size, const TX_CLASS tx_class) {
  // tx_class == 0(TX_CLASS_2D)
  if ((tx_class | coeff_idx) == 0) return 0;
  int ctx = (stats + 1) >> 1;
  ctx = AOMMIN(ctx, 4);
  switch (tx_class) {
    case TX_CLASS_2D: {
      // This is the algorithm to generate av1_nz_map_ctx_offset[][]
      //   const int width = tx_size_wide[tx_size];
      //   const int height = tx_size_high[tx_size];
      //   if (width < height) {
      //     if (row < 2) return 11 + ctx;
      //   } else if (width > height) {
      //     if (col < 2) return 16 + ctx;
      //   }
      //   if (row + col < 2) return ctx + 1;
      //   if (row + col < 4) return 5 + ctx + 1;
      //   return 21 + ctx;
      return ctx + av1_nz_map_ctx_offset[tx_size][coeff_idx];
    }
    case TX_CLASS_HORIZ: {
      const int row = coeff_idx >> bwl;
      const int col = coeff_idx - (row << bwl);
      return ctx + nz_map_ctx_offset_1d[col];
      break;
    }
    case TX_CLASS_VERT: {
      const int row = coeff_idx >> bwl;
      return ctx + nz_map_ctx_offset_1d[row];
      break;
    }
    default: break;
  }
  return 0;
}

typedef aom_cdf_prob (*base_cdf_arr)[CDF_SIZE(4)];
typedef aom_cdf_prob (*br_cdf_arr)[CDF_SIZE(BR_CDF_SIZE)];

static INLINE int get_lower_levels_ctx_eob(int bwl, int height, int scan_idx) {
  if (scan_idx == 0) return 0;
  if (scan_idx <= (height << bwl) / 8) return 1;
  if (scan_idx <= (height << bwl) / 4) return 2;
  return 3;
}

static INLINE int get_lower_levels_ctx_2d(const uint8_t *levels, int coeff_idx,
                                          int bwl, TX_SIZE tx_size) {
  assert(coeff_idx > 0);
  int mag;
  // Note: AOMMIN(level, 3) is useless for decoder since level < 3.
  levels = levels + get_padded_idx(coeff_idx, bwl);
  mag = AOMMIN(levels[1], 3);                                     // { 0, 1 }
  mag += AOMMIN(levels[(1 << bwl) + TX_PAD_HOR], 3);              // { 1, 0 }
  mag += AOMMIN(levels[(1 << bwl) + TX_PAD_HOR + 1], 3);          // { 1, 1 }
  mag += AOMMIN(levels[2], 3);                                    // { 0, 2 }
  mag += AOMMIN(levels[(2 << bwl) + (2 << TX_PAD_HOR_LOG2)], 3);  // { 2, 0 }

  const int ctx = AOMMIN((mag + 1) >> 1, 4);
  return ctx + av1_nz_map_ctx_offset[tx_size][coeff_idx];
}
static AOM_FORCE_INLINE int get_lower_levels_ctx(const uint8_t *levels,
                                                 int coeff_idx, int bwl,
                                                 TX_SIZE tx_size,
                                                 TX_CLASS tx_class) {
  const int stats =
      get_nz_mag(levels + get_padded_idx(coeff_idx, bwl), bwl, tx_class);
  return get_nz_map_ctx_from_stats(stats, coeff_idx, bwl, tx_size, tx_class);
}

static INLINE int get_lower_levels_ctx_general(int is_last, int scan_idx,
                                               int bwl, int height,
                                               const uint8_t *levels,
                                               int coeff_idx, TX_SIZE tx_size,
                                               TX_CLASS tx_class) {
  if (is_last) {
    if (scan_idx == 0) return 0;
    if (scan_idx <= (height << bwl) >> 3) return 1;
    if (scan_idx <= (height << bwl) >> 2) return 2;
    return 3;
  }
  return get_lower_levels_ctx(levels, coeff_idx, bwl, tx_size, tx_class);
}

static INLINE void set_dc_sign(int *cul_level, int dc_val) {
  if (dc_val < 0)
    *cul_level |= 1 << COEFF_CONTEXT_BITS;
  else if (dc_val > 0)
    *cul_level += 2 << COEFF_CONTEXT_BITS;
}

static INLINE void get_txb_ctx(const BLOCK_SIZE plane_bsize,
                               const TX_SIZE tx_size, const int plane,
                               const ENTROPY_CONTEXT *const a,
                               const ENTROPY_CONTEXT *const l,
                               TXB_CTX *const txb_ctx) {
#define MAX_TX_SIZE_UNIT 16
  static const int8_t signs[3] = { 0, -1, 1 };
  static const int8_t dc_sign_contexts[4 * MAX_TX_SIZE_UNIT + 1] = {
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2
  };
  const int txb_w_unit = tx_size_wide_unit[tx_size];
  const int txb_h_unit = tx_size_high_unit[tx_size];
  int dc_sign = 0;
  int k = 0;

  do {
    const unsigned int sign = ((uint8_t)a[k]) >> COEFF_CONTEXT_BITS;
    assert(sign <= 2);
    dc_sign += signs[sign];
  } while (++k < txb_w_unit);

  k = 0;
  do {
    const unsigned int sign = ((uint8_t)l[k]) >> COEFF_CONTEXT_BITS;
    assert(sign <= 2);
    dc_sign += signs[sign];
  } while (++k < txb_h_unit);

  txb_ctx->dc_sign_ctx = dc_sign_contexts[dc_sign + 2 * MAX_TX_SIZE_UNIT];

  if (plane == 0) {
    if (plane_bsize == txsize_to_bsize[tx_size]) {
      txb_ctx->txb_skip_ctx = 0;
    } else {
      // This is the algorithm to generate table skip_contexts[min][max].
      //    if (!max)
      //      txb_skip_ctx = 1;
      //    else if (!min)
      //      txb_skip_ctx = 2 + (max > 3);
      //    else if (max <= 3)
      //      txb_skip_ctx = 4;
      //    else if (min <= 3)
      //      txb_skip_ctx = 5;
      //    else
      //      txb_skip_ctx = 6;
      static const uint8_t skip_contexts[5][5] = { { 1, 2, 2, 2, 3 },
                                                   { 1, 4, 4, 4, 5 },
                                                   { 1, 4, 4, 4, 5 },
                                                   { 1, 4, 4, 4, 5 },
                                                   { 1, 4, 4, 4, 6 } };
      int top = 0;
      int left = 0;

      k = 0;
      do {
        top |= a[k];
      } while (++k < txb_w_unit);
      top &= COEFF_CONTEXT_MASK;

      k = 0;
      do {
        left |= l[k];
      } while (++k < txb_h_unit);
      left &= COEFF_CONTEXT_MASK;
      const int max = AOMMIN(top | left, 4);
      const int min = AOMMIN(AOMMIN(top, left), 4);

      txb_ctx->txb_skip_ctx = skip_contexts[min][max];
    }
  } else {
    const int ctx_base = get_entropy_context(tx_size, a, l);
    const int ctx_offset = (num_pels_log2_lookup[plane_bsize] >
                            num_pels_log2_lookup[txsize_to_bsize[tx_size]])
                               ? 10
                               : 7;
    txb_ctx->txb_skip_ctx = ctx_base + ctx_offset;
  }
#undef MAX_TX_SIZE_UNIT
}

void av1_init_lv_map(AV1_COMMON *cm);

#endif  // AV1_COMMON_TXB_COMMON_H_
