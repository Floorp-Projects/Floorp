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

#define REDUCE_CONTEXT_DEPENDENCY 0
#define MIN_SCAN_IDX_REDUCE_CONTEXT_DEPENDENCY 0

extern const int16_t av1_coeff_band_4x4[16];

extern const int16_t av1_coeff_band_8x8[64];

extern const int16_t av1_coeff_band_16x16[256];

extern const int16_t av1_coeff_band_32x32[1024];

typedef struct txb_ctx {
  int txb_skip_ctx;
  int dc_sign_ctx;
} TXB_CTX;

static INLINE TX_SIZE get_txsize_context(TX_SIZE tx_size) {
  return txsize_sqr_up_map[tx_size];
}

static int base_ref_offset[BASE_CONTEXT_POSITION_NUM][2] = {
  /* clang-format off*/
  { -2, 0 }, { -1, -1 }, { -1, 0 }, { -1, 1 }, { 0, -2 }, { 0, -1 }, { 0, 1 },
  { 0, 2 },  { 1, -1 },  { 1, 0 },  { 1, 1 },  { 2, 0 }
  /* clang-format on*/
};

static INLINE int get_level_count(const tran_low_t *tcoeffs, int bwl,
                                  int height, int row, int col, int level,
                                  int (*nb_offset)[2], int nb_num) {
  int count = 0;
  for (int idx = 0; idx < nb_num; ++idx) {
    const int ref_row = row + nb_offset[idx][0];
    const int ref_col = col + nb_offset[idx][1];
    if (ref_row < 0 || ref_col < 0 || ref_row >= height ||
        ref_col >= (1 << bwl))
      continue;
    const int pos = (ref_row << bwl) + ref_col;
    tran_low_t abs_coeff = abs(tcoeffs[pos]);
    count += abs_coeff > level;
  }
  return count;
}

static INLINE void get_mag(int *mag, const tran_low_t *tcoeffs, int bwl,
                           int height, int row, int col, int (*nb_offset)[2],
                           int nb_num) {
  mag[0] = 0;
  mag[1] = 0;
  for (int idx = 0; idx < nb_num; ++idx) {
    const int ref_row = row + nb_offset[idx][0];
    const int ref_col = col + nb_offset[idx][1];
    if (ref_row < 0 || ref_col < 0 || ref_row >= height ||
        ref_col >= (1 << bwl))
      continue;
    const int pos = (ref_row << bwl) + ref_col;
    tran_low_t abs_coeff = abs(tcoeffs[pos]);
    if (nb_offset[idx][0] >= 0 && nb_offset[idx][1] >= 0) {
      if (abs_coeff > mag[0]) {
        mag[0] = abs_coeff;
        mag[1] = 1;
      } else if (abs_coeff == mag[0]) {
        ++mag[1];
      }
    }
  }
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

static INLINE int get_level_count_mag(int *mag, const tran_low_t *tcoeffs,
                                      int bwl, int height, int row, int col,
                                      int level, int (*nb_offset)[2],
                                      int nb_num) {
  const int stride = 1 << bwl;
  int count = 0;
  *mag = 0;
  for (int idx = 0; idx < nb_num; ++idx) {
    const int ref_row = row + nb_offset[idx][0];
    const int ref_col = col + nb_offset[idx][1];
    if (ref_row < 0 || ref_col < 0 || ref_row >= height || ref_col >= stride)
      continue;
    const int pos = (ref_row << bwl) + ref_col;
    tran_low_t abs_coeff = abs(tcoeffs[pos]);
    count += abs_coeff > level;
    if (nb_offset[idx][0] >= 0 && nb_offset[idx][1] >= 0)
      *mag = AOMMAX(*mag, abs_coeff);
  }
  return count;
}

static INLINE int get_base_ctx_from_count_mag(int row, int col, int count,
                                              int sig_mag) {
  const int ctx = (count + 1) >> 1;
  int ctx_idx = -1;
  if (row == 0 && col == 0) {
    ctx_idx = (ctx << 1) + sig_mag;
    // TODO(angiebird): turn this on once the optimization is finalized
    // assert(ctx_idx < 8);
  } else if (row == 0) {
    ctx_idx = 8 + (ctx << 1) + sig_mag;
    // TODO(angiebird): turn this on once the optimization is finalized
    // assert(ctx_idx < 18);
  } else if (col == 0) {
    ctx_idx = 8 + 10 + (ctx << 1) + sig_mag;
    // TODO(angiebird): turn this on once the optimization is finalized
    // assert(ctx_idx < 28);
  } else {
    ctx_idx = 8 + 10 + 10 + (ctx << 1) + sig_mag;
    assert(ctx_idx < COEFF_BASE_CONTEXTS);
  }
  return ctx_idx;
}

static INLINE int get_base_ctx(const tran_low_t *tcoeffs,
                               int c,  // raster order
                               const int bwl, const int height,
                               const int level) {
  const int row = c >> bwl;
  const int col = c - (row << bwl);
  const int level_minus_1 = level - 1;
  int mag;
  int count =
      get_level_count_mag(&mag, tcoeffs, bwl, height, row, col, level_minus_1,
                          base_ref_offset, BASE_CONTEXT_POSITION_NUM);
  int ctx_idx = get_base_ctx_from_count_mag(row, col, count, mag > level);
  return ctx_idx;
}

#define BR_CONTEXT_POSITION_NUM 8  // Base range coefficient context
static int br_ref_offset[BR_CONTEXT_POSITION_NUM][2] = {
  /* clang-format off*/
  { -1, -1 }, { -1, 0 }, { -1, 1 }, { 0, -1 },
  { 0, 1 },   { 1, -1 }, { 1, 0 },  { 1, 1 },
  /* clang-format on*/
};

static const int br_level_map[9] = {
  0, 0, 1, 1, 2, 2, 3, 3, 3,
};

static const int coeff_to_br_index[COEFF_BASE_RANGE] = {
  0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2,
};

static const int br_index_to_coeff[BASE_RANGE_SETS] = {
  0, 2, 6,
};

static const int br_extra_bits[BASE_RANGE_SETS] = {
  1, 2, 3,
};

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

static INLINE int get_br_ctx_from_count_mag(int row, int col, int count,
                                            int mag) {
  int offset = 0;
  if (mag <= BR_MAG_OFFSET)
    offset = 0;
  else if (mag <= 3)
    offset = 1;
  else if (mag <= 5)
    offset = 2;
  else
    offset = 3;

  int ctx = br_level_map[count];
  ctx += offset * BR_TMP_OFFSET;

  // DC: 0 - 1
  if (row == 0 && col == 0) return ctx;

  // Top row: 2 - 4
  if (row == 0) return 2 + ctx;

  // Left column: 5 - 7
  if (col == 0) return 5 + ctx;

  // others: 8 - 11
  return 8 + ctx;
}

static INLINE int get_br_ctx(const tran_low_t *tcoeffs,
                             const int c,  // raster order
                             const int bwl, const int height) {
  const int row = c >> bwl;
  const int col = c - (row << bwl);
  const int level_minus_1 = NUM_BASE_LEVELS;
  int mag;
  const int count =
      get_level_count_mag(&mag, tcoeffs, bwl, height, row, col, level_minus_1,
                          br_ref_offset, BR_CONTEXT_POSITION_NUM);
  const int ctx = get_br_ctx_from_count_mag(row, col, count, mag);
  return ctx;
}

#define SIG_REF_OFFSET_NUM 7
static int sig_ref_offset[SIG_REF_OFFSET_NUM][2] = {
  { -2, -1 }, { -2, 0 }, { -1, -2 }, { -1, -1 },
  { -1, 0 },  { 0, -2 }, { 0, -1 },
};

#if REDUCE_CONTEXT_DEPENDENCY
static INLINE int get_nz_count(const tran_low_t *tcoeffs, int bwl, int height,
                               int row, int col, int prev_row, int prev_col) {
  int count = 0;
  for (int idx = 0; idx < SIG_REF_OFFSET_NUM; ++idx) {
    const int ref_row = row + sig_ref_offset[idx][0];
    const int ref_col = col + sig_ref_offset[idx][1];
    if (ref_row < 0 || ref_col < 0 || ref_row >= height ||
        ref_col >= (1 << bwl) || (prev_row == ref_row && prev_col == ref_col))
      continue;
    const int nb_pos = (ref_row << bwl) + ref_col;
    count += (tcoeffs[nb_pos] != 0);
  }
  return count;
}
#else
static INLINE int get_nz_count(const tran_low_t *tcoeffs, int bwl, int height,
                               int row, int col) {
  int count = 0;
  for (int idx = 0; idx < SIG_REF_OFFSET_NUM; ++idx) {
    const int ref_row = row + sig_ref_offset[idx][0];
    const int ref_col = col + sig_ref_offset[idx][1];
    if (ref_row < 0 || ref_col < 0 || ref_row >= height ||
        ref_col >= (1 << bwl))
      continue;
    const int nb_pos = (ref_row << bwl) + ref_col;
    count += (tcoeffs[nb_pos] != 0);
  }
  return count;
}
#endif

static INLINE TX_CLASS get_tx_class(TX_TYPE tx_type) {
  switch (tx_type) {
#if CONFIG_EXT_TX
    case V_DCT:
    case V_ADST:
    case V_FLIPADST: return TX_CLASS_VERT;
    case H_DCT:
    case H_ADST:
    case H_FLIPADST: return TX_CLASS_HORIZ;
#endif
    default: return TX_CLASS_2D;
  }
}

// TODO(angiebird): optimize this function by generate a table that maps from
// count to ctx
static INLINE int get_nz_map_ctx_from_count(int count,
                                            int coeff_idx,  // raster order
                                            int bwl, TX_TYPE tx_type) {
  (void)tx_type;
  const int row = coeff_idx >> bwl;
  const int col = coeff_idx - (row << bwl);
  int ctx = 0;
#if CONFIG_EXT_TX
  int tx_class = get_tx_class(tx_type);
  int offset;
  if (tx_class == TX_CLASS_2D)
    offset = 0;
  else if (tx_class == TX_CLASS_VERT)
    offset = SIG_COEF_CONTEXTS_2D;
  else
    offset = SIG_COEF_CONTEXTS_2D + SIG_COEF_CONTEXTS_1D;
#else
  int offset = 0;
#endif

  if (row == 0 && col == 0) return offset + 0;

  if (row == 0 && col == 1) return offset + 1 + count;

  if (row == 1 && col == 0) return offset + 3 + count;

  if (row == 1 && col == 1) {
    ctx = (count + 1) >> 1;

    assert(5 + ctx <= 7);

    return offset + 5 + ctx;
  }

  if (row == 0) {
    ctx = (count + 1) >> 1;

    assert(ctx < 2);
    return offset + 8 + ctx;
  }

  if (col == 0) {
    ctx = (count + 1) >> 1;

    assert(ctx < 2);
    return offset + 10 + ctx;
  }

  ctx = count >> 1;

  assert(12 + ctx < 16);

  return offset + 12 + ctx;
}

static INLINE int get_nz_map_ctx(const tran_low_t *tcoeffs, const int scan_idx,
                                 const int16_t *scan, const int bwl,
                                 const int height, TX_TYPE tx_type) {
  const int coeff_idx = scan[scan_idx];
  const int row = coeff_idx >> bwl;
  const int col = coeff_idx - (row << bwl);
#if REDUCE_CONTEXT_DEPENDENCY
  int prev_coeff_idx;
  int prev_row;
  int prev_col;
  if (scan_idx > MIN_SCAN_IDX_REDUCE_CONTEXT_DEPENDENCY) {
    prev_coeff_idx = scan[scan_idx - 1];  // raster order
    prev_row = prev_coeff_idx >> bwl;
    prev_col = prev_coeff_idx - (prev_row << bwl);
  } else {
    prev_coeff_idx = -1;
    prev_row = -1;
    prev_col = -1;
  }
  int count = get_nz_count(tcoeffs, bwl, height, row, col, prev_row, prev_col);
#else
  int count = get_nz_count(tcoeffs, bwl, height, row, col);
#endif
  return get_nz_map_ctx_from_count(count, coeff_idx, bwl, tx_type);
}

static INLINE int get_eob_ctx(const tran_low_t *tcoeffs,
                              const int coeff_idx,  // raster order
                              const TX_SIZE txs_ctx, TX_TYPE tx_type) {
  (void)tcoeffs;
  int offset = 0;
#if CONFIG_CTX1D
  TX_CLASS tx_class = get_tx_class(tx_type);
  if (tx_class == TX_CLASS_VERT)
    offset = EOB_COEF_CONTEXTS_2D;
  else if (tx_class == TX_CLASS_HORIZ)
    offset = EOB_COEF_CONTEXTS_2D + EOB_COEF_CONTEXTS_1D;
#else
  (void)tx_type;
#endif

  if (txs_ctx == TX_4X4) return offset + av1_coeff_band_4x4[coeff_idx];
  if (txs_ctx == TX_8X8) return offset + av1_coeff_band_8x8[coeff_idx];
  if (txs_ctx == TX_16X16) return offset + av1_coeff_band_16x16[coeff_idx];
  if (txs_ctx == TX_32X32) return offset + av1_coeff_band_32x32[coeff_idx];

  assert(0);
  return 0;
}

static INLINE void set_dc_sign(int *cul_level, tran_low_t v) {
  if (v < 0)
    *cul_level |= 1 << COEFF_CONTEXT_BITS;
  else if (v > 0)
    *cul_level += 2 << COEFF_CONTEXT_BITS;
}

static INLINE int get_dc_sign_ctx(int dc_sign) {
  int dc_sign_ctx = 0;
  if (dc_sign < 0)
    dc_sign_ctx = 1;
  else if (dc_sign > 0)
    dc_sign_ctx = 2;

  return dc_sign_ctx;
}

static INLINE void get_txb_ctx(BLOCK_SIZE plane_bsize, TX_SIZE tx_size,
                               int plane, const ENTROPY_CONTEXT *a,
                               const ENTROPY_CONTEXT *l, TXB_CTX *txb_ctx) {
  const int txb_w_unit = tx_size_wide_unit[tx_size];
  const int txb_h_unit = tx_size_high_unit[tx_size];
  int ctx_offset = (plane == 0) ? 0 : 7;

  if (plane_bsize > txsize_to_bsize[tx_size]) ctx_offset += 3;

  int dc_sign = 0;
  for (int k = 0; k < txb_w_unit; ++k) {
    int sign = ((uint8_t)a[k]) >> COEFF_CONTEXT_BITS;
    if (sign == 1)
      --dc_sign;
    else if (sign == 2)
      ++dc_sign;
    else if (sign != 0)
      assert(0);
  }

  for (int k = 0; k < txb_h_unit; ++k) {
    int sign = ((uint8_t)l[k]) >> COEFF_CONTEXT_BITS;
    if (sign == 1)
      --dc_sign;
    else if (sign == 2)
      ++dc_sign;
    else if (sign != 0)
      assert(0);
  }

  txb_ctx->dc_sign_ctx = get_dc_sign_ctx(dc_sign);

  if (plane == 0) {
    int top = 0;
    int left = 0;

    for (int k = 0; k < txb_w_unit; ++k) {
      top = AOMMAX(top, ((uint8_t)a[k] & COEFF_CONTEXT_MASK));
    }

    for (int k = 0; k < txb_h_unit; ++k) {
      left = AOMMAX(left, ((uint8_t)l[k] & COEFF_CONTEXT_MASK));
    }

    top = AOMMIN(top, 255);
    left = AOMMIN(left, 255);

    if (plane_bsize == txsize_to_bsize[tx_size])
      txb_ctx->txb_skip_ctx = 0;
    else if (top == 0 && left == 0)
      txb_ctx->txb_skip_ctx = 1;
    else if (top == 0 || left == 0)
      txb_ctx->txb_skip_ctx = 2 + (AOMMAX(top, left) > 3);
    else if (AOMMAX(top, left) <= 3)
      txb_ctx->txb_skip_ctx = 4;
    else if (AOMMIN(top, left) <= 3)
      txb_ctx->txb_skip_ctx = 5;
    else
      txb_ctx->txb_skip_ctx = 6;
  } else {
    int ctx_base = get_entropy_context(tx_size, a, l);
    txb_ctx->txb_skip_ctx = ctx_offset + ctx_base;
  }
}

#if LV_MAP_PROB
void av1_init_txb_probs(FRAME_CONTEXT *fc);
#endif  // LV_MAP_PROB

void av1_adapt_txb_probs(AV1_COMMON *cm, unsigned int count_sat,
                         unsigned int update_factor);

void av1_init_lv_map(AV1_COMMON *cm);

#if CONFIG_CTX1D
static INLINE void get_eob_vert(int16_t *eob_ls, const tran_low_t *tcoeff,
                                int w, int h) {
  for (int c = 0; c < w; ++c) {
    eob_ls[c] = 0;
    for (int r = h - 1; r >= 0; --r) {
      int coeff_idx = r * w + c;
      if (tcoeff[coeff_idx] != 0) {
        eob_ls[c] = r + 1;
        break;
      }
    }
  }
}

static INLINE void get_eob_horiz(int16_t *eob_ls, const tran_low_t *tcoeff,
                                 int w, int h) {
  for (int r = 0; r < h; ++r) {
    eob_ls[r] = 0;
    for (int c = w - 1; c >= 0; --c) {
      int coeff_idx = r * w + c;
      if (tcoeff[coeff_idx] != 0) {
        eob_ls[r] = c + 1;
        break;
      }
    }
  }
}

static INLINE int get_empty_line_ctx(int line_idx, int16_t *eob_ls) {
  if (line_idx > 0) {
    int prev_eob = eob_ls[line_idx - 1];
    if (prev_eob == 0) {
      return 1;
    } else if (prev_eob < 3) {
      return 2;
    } else if (prev_eob < 6) {
      return 3;
    } else {
      return 4;
    }
  } else {
    return 0;
  }
}

#define MAX_POS_CTX 8
static int pos_ctx[MAX_HVTX_SIZE] = {
  0, 1, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5,
  6, 6, 6, 6, 6, 6, 6, 6, 7, 7, 7, 7, 7, 7, 7, 7,
};
static INLINE int get_hv_eob_ctx(int line_idx, int pos, int16_t *eob_ls) {
  if (line_idx > 0) {
    int prev_eob = eob_ls[line_idx - 1];
    int diff = pos + 1 - prev_eob;
    int abs_diff = abs(diff);
    int ctx_idx = pos_ctx[abs_diff];
    assert(ctx_idx < MAX_POS_CTX);
    if (diff < 0) {
      ctx_idx += MAX_POS_CTX;
      assert(ctx_idx >= MAX_POS_CTX);
      assert(ctx_idx < 2 * MAX_POS_CTX);
    }
    return ctx_idx;
  } else {
    int ctx_idx = MAX_POS_CTX + MAX_POS_CTX + pos_ctx[pos];
    assert(ctx_idx < HV_EOB_CONTEXTS);
    assert(HV_EOB_CONTEXTS == MAX_POS_CTX * 3);
    return ctx_idx;
  }
}
#endif  // CONFIG_CTX1D

#endif  // AV1_COMMON_TXB_COMMON_H_
