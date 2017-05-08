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
extern const int16_t av1_coeff_band_4x4[16];

extern const int16_t av1_coeff_band_8x8[64];

extern const int16_t av1_coeff_band_16x16[256];

extern const int16_t av1_coeff_band_32x32[1024];

typedef struct txb_ctx {
  int txb_skip_ctx;
  int dc_sign_ctx;
} TXB_CTX;

#define BASE_CONTEXT_POSITION_NUM 12
static int base_ref_offset[BASE_CONTEXT_POSITION_NUM][2] = {
  /* clang-format off*/
  { -2, 0 }, { -1, -1 }, { -1, 0 }, { -1, 1 }, { 0, -2 }, { 0, -1 }, { 0, 1 },
  { 0, 2 },  { 1, -1 },  { 1, 0 },  { 1, 1 },  { 2, 0 }
  /* clang-format on*/
};

static INLINE int get_base_ctx(const tran_low_t *tcoeffs,
                               int c,  // raster order
                               const int bwl, const int level) {
  const int row = c >> bwl;
  const int col = c - (row << bwl);
  const int stride = 1 << bwl;
  const int level_minus_1 = level - 1;
  int ctx = 0;
  int mag = 0;
  int idx;
  int ctx_idx = -1;
  tran_low_t abs_coeff;

  ctx = 0;
  for (idx = 0; idx < BASE_CONTEXT_POSITION_NUM; ++idx) {
    int ref_row = row + base_ref_offset[idx][0];
    int ref_col = col + base_ref_offset[idx][1];
    int pos = (ref_row << bwl) + ref_col;

    if (ref_row < 0 || ref_col < 0 || ref_row >= stride || ref_col >= stride)
      continue;

    abs_coeff = abs(tcoeffs[pos]);
    ctx += abs_coeff > level_minus_1;

    if (base_ref_offset[idx][0] >= 0 && base_ref_offset[idx][1] >= 0)
      mag |= abs_coeff > level;
  }
  ctx = (ctx + 1) >> 1;
  if (row == 0 && col == 0) {
    ctx_idx = (ctx << 1) + mag;
    assert(ctx_idx < 8);
  } else if (row == 0) {
    ctx_idx = 8 + (ctx << 1) + mag;
    assert(ctx_idx < 18);
  } else if (col == 0) {
    ctx_idx = 8 + 10 + (ctx << 1) + mag;
    assert(ctx_idx < 28);
  } else {
    ctx_idx = 8 + 10 + 10 + (ctx << 1) + mag;
    assert(ctx_idx < COEFF_BASE_CONTEXTS);
  }
  return ctx_idx;
}

#define BR_CONTEXT_POSITION_NUM 8  // Base range coefficient context
static int br_ref_offset[BR_CONTEXT_POSITION_NUM][2] = {
  /* clang-format off*/
  { -1, -1 }, { -1, 0 }, { -1, 1 }, { 0, -1 },
  { 0, 1 },   { 1, -1 }, { 1, 0 },  { 1, 1 },
  /* clang-format on*/
};

static int br_level_map[9] = {
  0, 0, 1, 1, 2, 2, 3, 3, 3,
};

static INLINE int get_level_ctx(const tran_low_t *tcoeffs,
                                const int c,  // raster order
                                const int bwl) {
  const int row = c >> bwl;
  const int col = c - (row << bwl);
  const int stride = 1 << bwl;
  const int level_minus_1 = NUM_BASE_LEVELS;
  int ctx = 0;
  int idx;
  tran_low_t abs_coeff;
  int mag = 0, offset = 0;

  for (idx = 0; idx < BR_CONTEXT_POSITION_NUM; ++idx) {
    int ref_row = row + br_ref_offset[idx][0];
    int ref_col = col + br_ref_offset[idx][1];
    int pos = (ref_row << bwl) + ref_col;

    if (ref_row < 0 || ref_col < 0 || ref_row >= stride || ref_col >= stride)
      continue;

    abs_coeff = abs(tcoeffs[pos]);
    ctx += abs_coeff > level_minus_1;

    if (br_ref_offset[idx][0] >= 0 && br_ref_offset[idx][1] >= 0)
      mag = AOMMAX(mag, abs_coeff);
  }

  if (mag <= 1)
    offset = 0;
  else if (mag <= 3)
    offset = 1;
  else if (mag <= 6)
    offset = 2;
  else
    offset = 3;

  ctx = br_level_map[ctx];

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

static int sig_ref_offset[11][2] = {
  { -2, -1 }, { -2, 0 }, { -2, 1 }, { -1, -2 }, { -1, -1 }, { -1, 0 },
  { -1, 1 },  { 0, -2 }, { 0, -1 }, { 1, -2 },  { 1, -1 },
};

static INLINE int get_nz_map_ctx(const tran_low_t *tcoeffs,
                                 const uint8_t *txb_mask,
                                 const int c,  // raster order
                                 const int bwl) {
  const int row = c >> bwl;
  const int col = c - (row << bwl);
  int ctx = 0;
  int idx;
  int stride = 1 << bwl;

  if (row == 0 && col == 0) return 0;

  if (row == 0 && col == 1) return 1 + (tcoeffs[0] != 0);

  if (row == 1 && col == 0) return 3 + (tcoeffs[0] != 0);

  if (row == 1 && col == 1) {
    int pos;
    ctx = (tcoeffs[0] != 0);

    if (txb_mask[1]) ctx += (tcoeffs[1] != 0);
    pos = 1 << bwl;
    if (txb_mask[pos]) ctx += (tcoeffs[pos] != 0);

    ctx = (ctx + 1) >> 1;

    assert(5 + ctx <= 7);

    return 5 + ctx;
  }

  for (idx = 0; idx < 11; ++idx) {
    int ref_row = row + sig_ref_offset[idx][0];
    int ref_col = col + sig_ref_offset[idx][1];
    int pos;

    if (ref_row < 0 || ref_col < 0 || ref_row >= stride || ref_col >= stride)
      continue;

    pos = (ref_row << bwl) + ref_col;

    if (txb_mask[pos]) ctx += (tcoeffs[pos] != 0);
  }

  if (row == 0) {
    ctx = (ctx + 1) >> 1;

    assert(ctx < 3);
    return 8 + ctx;
  }

  if (col == 0) {
    ctx = (ctx + 1) >> 1;

    assert(ctx < 3);
    return 11 + ctx;
  }

  ctx >>= 1;

  assert(14 + ctx < 20);

  return 14 + ctx;
}

static INLINE int get_eob_ctx(const tran_low_t *tcoeffs,
                              const int c,  // raster order
                              const int bwl) {
  (void)tcoeffs;
  if (bwl == 2) return av1_coeff_band_4x4[c];
  if (bwl == 3) return av1_coeff_band_8x8[c];
  if (bwl == 4) return av1_coeff_band_16x16[c];
  if (bwl == 5) return av1_coeff_band_32x32[c];

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
  const int tx_size_in_blocks = 1 << tx_size;
  int ctx_offset = (plane == 0) ? 0 : 7;
  int k;

  if (plane_bsize > txsize_to_bsize[tx_size]) ctx_offset += 3;

  int dc_sign = 0;
  for (k = 0; k < tx_size_in_blocks; ++k) {
    int sign = ((uint8_t)a[k]) >> COEFF_CONTEXT_BITS;
    if (sign == 1)
      --dc_sign;
    else if (sign == 2)
      ++dc_sign;
    else if (sign != 0)
      assert(0);

    sign = ((uint8_t)l[k]) >> COEFF_CONTEXT_BITS;
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
    for (k = 0; k < tx_size_in_blocks; ++k) {
      top = AOMMAX(top, ((uint8_t)a[k] & COEFF_CONTEXT_MASK));
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

void av1_adapt_txb_probs(AV1_COMMON *cm, unsigned int count_sat,
                         unsigned int update_factor);
#endif  // AV1_COMMON_TXB_COMMON_H_
