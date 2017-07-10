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

#include "av1/common/scan.h"
#include "av1/common/blockd.h"
#include "av1/common/idct.h"
#include "av1/common/pred_common.h"
#include "av1/encoder/bitstream.h"
#include "av1/encoder/encodeframe.h"
#include "av1/encoder/cost.h"
#include "av1/encoder/encodetxb.h"
#include "av1/encoder/rdopt.h"
#include "av1/encoder/subexp.h"
#include "av1/encoder/tokenize.h"

#define TEST_OPTIMIZE_TXB 0

void av1_alloc_txb_buf(AV1_COMP *cpi) {
#if 0
  AV1_COMMON *cm = &cpi->common;
  int mi_block_size = 1 << MI_SIZE_LOG2;
  // TODO(angiebird): Make sure cm->subsampling_x/y is set correctly, and then
  // use precise buffer size according to cm->subsampling_x/y
  int pixel_stride = mi_block_size * cm->mi_cols;
  int pixel_height = mi_block_size * cm->mi_rows;
  int i;
  for (i = 0; i < MAX_MB_PLANE; ++i) {
    CHECK_MEM_ERROR(
        cm, cpi->tcoeff_buf[i],
        aom_malloc(sizeof(*cpi->tcoeff_buf[i]) * pixel_stride * pixel_height));
  }
#else
  (void)cpi;
#endif
}

void av1_free_txb_buf(AV1_COMP *cpi) {
#if 0
  int i;
  for (i = 0; i < MAX_MB_PLANE; ++i) {
    aom_free(cpi->tcoeff_buf[i]);
  }
#else
  (void)cpi;
#endif
}

static void write_golomb(aom_writer *w, int level) {
  int x = level + 1;
  int i = x;
  int length = 0;

  while (i) {
    i >>= 1;
    ++length;
  }
  assert(length > 0);

  for (i = 0; i < length - 1; ++i) aom_write_bit(w, 0);

  for (i = length - 1; i >= 0; --i) aom_write_bit(w, (x >> i) & 0x01);
}

void av1_write_coeffs_txb(const AV1_COMMON *const cm, MACROBLOCKD *xd,
                          aom_writer *w, int blk_row, int blk_col, int block,
                          int plane, TX_SIZE tx_size, const tran_low_t *tcoeff,
                          uint16_t eob, TXB_CTX *txb_ctx) {
  aom_prob *nz_map;
  aom_prob *eob_flag;
  MB_MODE_INFO *mbmi = &xd->mi[0]->mbmi;
  const PLANE_TYPE plane_type = get_plane_type(plane);
  const TX_SIZE txs_ctx = get_txsize_context(tx_size);
  const TX_TYPE tx_type =
      av1_get_tx_type(plane_type, xd, blk_row, blk_col, block, tx_size);
  const SCAN_ORDER *const scan_order = get_scan(cm, tx_size, tx_type, mbmi);
  const int16_t *scan = scan_order->scan;
  const int16_t *iscan = scan_order->iscan;
  int c;
  int is_nz;
  const int bwl = b_width_log2_lookup[txsize_to_bsize[tx_size]] + 2;
  const int height = tx_size_high[tx_size];
  const int seg_eob = tx_size_2d[tx_size];
  uint16_t update_eob = 0;

  (void)blk_row;
  (void)blk_col;

  aom_write(w, eob == 0, cm->fc->txb_skip[txs_ctx][txb_ctx->txb_skip_ctx]);

  if (eob == 0) return;
#if CONFIG_TXK_SEL
  av1_write_tx_type(cm, xd, blk_row, blk_col, block, plane,
                    get_min_tx_size(tx_size), w);
#endif

  nz_map = cm->fc->nz_map[txs_ctx][plane_type];
  eob_flag = cm->fc->eob_flag[txs_ctx][plane_type];

  for (c = 0; c < eob; ++c) {
    int coeff_ctx = get_nz_map_ctx(tcoeff, scan[c], bwl, height, iscan);
    int eob_ctx = get_eob_ctx(tcoeff, scan[c], txs_ctx);

    tran_low_t v = tcoeff[scan[c]];
    is_nz = (v != 0);

    if (c == seg_eob - 1) break;

    aom_write(w, is_nz, nz_map[coeff_ctx]);

    if (is_nz) {
      aom_write(w, c == (eob - 1), eob_flag[eob_ctx]);
    }
  }

  int i;
  for (i = 0; i < NUM_BASE_LEVELS; ++i) {
    aom_prob *coeff_base = cm->fc->coeff_base[txs_ctx][plane_type][i];

    update_eob = 0;
    for (c = eob - 1; c >= 0; --c) {
      tran_low_t v = tcoeff[scan[c]];
      tran_low_t level = abs(v);
      int sign = (v < 0) ? 1 : 0;
      int ctx;

      if (level <= i) continue;

      ctx = get_base_ctx(tcoeff, scan[c], bwl, height, i + 1);

      if (level == i + 1) {
        aom_write(w, 1, coeff_base[ctx]);
        if (c == 0) {
          aom_write(w, sign, cm->fc->dc_sign[plane_type][txb_ctx->dc_sign_ctx]);
        } else {
          aom_write_bit(w, sign);
        }
        continue;
      }
      aom_write(w, 0, coeff_base[ctx]);
      update_eob = AOMMAX(update_eob, c);
    }
  }

  for (c = update_eob; c >= 0; --c) {
    tran_low_t v = tcoeff[scan[c]];
    tran_low_t level = abs(v);
    int sign = (v < 0) ? 1 : 0;
    int idx;
    int ctx;

    if (level <= NUM_BASE_LEVELS) continue;

    if (c == 0) {
      aom_write(w, sign, cm->fc->dc_sign[plane_type][txb_ctx->dc_sign_ctx]);
    } else {
      aom_write_bit(w, sign);
    }

    // level is above 1.
    ctx = get_br_ctx(tcoeff, scan[c], bwl, height);
    for (idx = 0; idx < COEFF_BASE_RANGE; ++idx) {
      if (level == (idx + 1 + NUM_BASE_LEVELS)) {
        aom_write(w, 1, cm->fc->coeff_lps[txs_ctx][plane_type][ctx]);
        break;
      }
      aom_write(w, 0, cm->fc->coeff_lps[txs_ctx][plane_type][ctx]);
    }
    if (idx < COEFF_BASE_RANGE) continue;

    // use 0-th order Golomb code to handle the residual level.
    write_golomb(w, level - COEFF_BASE_RANGE - 1 - NUM_BASE_LEVELS);
  }
}

void av1_write_coeffs_mb(const AV1_COMMON *const cm, MACROBLOCK *x,
                         aom_writer *w, int plane) {
  MACROBLOCKD *xd = &x->e_mbd;
  MB_MODE_INFO *mbmi = &xd->mi[0]->mbmi;
  BLOCK_SIZE bsize = mbmi->sb_type;
  struct macroblockd_plane *pd = &xd->plane[plane];

#if CONFIG_CHROMA_SUB8X8
  const BLOCK_SIZE plane_bsize =
      AOMMAX(BLOCK_4X4, get_plane_block_size(bsize, pd));
#elif CONFIG_CB4X4
  const BLOCK_SIZE plane_bsize = get_plane_block_size(bsize, pd);
#else
  const BLOCK_SIZE plane_bsize =
      get_plane_block_size(AOMMAX(bsize, BLOCK_8X8), pd);
#endif
  const int max_blocks_wide = max_block_wide(xd, plane_bsize, plane);
  const int max_blocks_high = max_block_high(xd, plane_bsize, plane);
  const TX_SIZE tx_size = av1_get_tx_size(plane, xd);
  const int bkw = tx_size_wide_unit[tx_size];
  const int bkh = tx_size_high_unit[tx_size];
  const int step = tx_size_wide_unit[tx_size] * tx_size_high_unit[tx_size];
  int row, col;
  int block = 0;
  for (row = 0; row < max_blocks_high; row += bkh) {
    for (col = 0; col < max_blocks_wide; col += bkw) {
      tran_low_t *tcoeff = BLOCK_OFFSET(x->mbmi_ext->tcoeff[plane], block);
      uint16_t eob = x->mbmi_ext->eobs[plane][block];
      TXB_CTX txb_ctx = { x->mbmi_ext->txb_skip_ctx[plane][block],
                          x->mbmi_ext->dc_sign_ctx[plane][block] };
      av1_write_coeffs_txb(cm, xd, w, row, col, block, plane, tx_size, tcoeff,
                           eob, &txb_ctx);
      block += step;
    }
  }
}

static INLINE void get_base_ctx_set(const tran_low_t *tcoeffs,
                                    int c,  // raster order
                                    const int bwl, const int height,
                                    int ctx_set[NUM_BASE_LEVELS]) {
  const int row = c >> bwl;
  const int col = c - (row << bwl);
  const int stride = 1 << bwl;
  int mag[NUM_BASE_LEVELS] = { 0 };
  int idx;
  tran_low_t abs_coeff;
  int i;

  for (idx = 0; idx < BASE_CONTEXT_POSITION_NUM; ++idx) {
    int ref_row = row + base_ref_offset[idx][0];
    int ref_col = col + base_ref_offset[idx][1];
    int pos = (ref_row << bwl) + ref_col;

    if (ref_row < 0 || ref_col < 0 || ref_row >= height || ref_col >= stride)
      continue;

    abs_coeff = abs(tcoeffs[pos]);

    for (i = 0; i < NUM_BASE_LEVELS; ++i) {
      ctx_set[i] += abs_coeff > i;
      if (base_ref_offset[idx][0] >= 0 && base_ref_offset[idx][1] >= 0)
        mag[i] |= abs_coeff > (i + 1);
    }
  }

  for (i = 0; i < NUM_BASE_LEVELS; ++i) {
    ctx_set[i] = (ctx_set[i] + 1) >> 1;

    if (row == 0 && col == 0)
      ctx_set[i] = (ctx_set[i] << 1) + mag[i];
    else if (row == 0)
      ctx_set[i] = 8 + (ctx_set[i] << 1) + mag[i];
    else if (col == 0)
      ctx_set[i] = 18 + (ctx_set[i] << 1) + mag[i];
    else
      ctx_set[i] = 28 + (ctx_set[i] << 1) + mag[i];
  }
  return;
}

static INLINE int get_br_cost(tran_low_t abs_qc, int ctx,
                              const aom_prob *coeff_lps) {
  const tran_low_t min_level = 1 + NUM_BASE_LEVELS;
  const tran_low_t max_level = 1 + NUM_BASE_LEVELS + COEFF_BASE_RANGE;
  if (abs_qc >= min_level) {
    const int cost0 = av1_cost_bit(coeff_lps[ctx], 0);
    const int cost1 = av1_cost_bit(coeff_lps[ctx], 1);
    if (abs_qc >= max_level)
      return COEFF_BASE_RANGE * cost0;
    else
      return (abs_qc - min_level) * cost0 + cost1;
  } else {
    return 0;
  }
}

static INLINE int get_base_cost(tran_low_t abs_qc, int ctx,
                                aom_prob (*coeff_base)[COEFF_BASE_CONTEXTS],
                                int base_idx) {
  const int level = base_idx + 1;
  if (abs_qc < level)
    return 0;
  else
    return av1_cost_bit(coeff_base[base_idx][ctx], abs_qc == level);
}

int av1_cost_coeffs_txb(const AV1_COMP *const cpi, MACROBLOCK *x, int plane,
                        int blk_row, int blk_col, int block, TX_SIZE tx_size,
                        TXB_CTX *txb_ctx) {
  const AV1_COMMON *const cm = &cpi->common;
  MACROBLOCKD *const xd = &x->e_mbd;
  TX_SIZE txs_ctx = get_txsize_context(tx_size);
  const PLANE_TYPE plane_type = get_plane_type(plane);
  const TX_TYPE tx_type =
      av1_get_tx_type(plane_type, xd, blk_row, blk_col, block, tx_size);
  MB_MODE_INFO *mbmi = &xd->mi[0]->mbmi;
  const struct macroblock_plane *p = &x->plane[plane];
  const int eob = p->eobs[block];
  const tran_low_t *const qcoeff = BLOCK_OFFSET(p->qcoeff, block);
  int c, cost;
  const int seg_eob = AOMMIN(eob, tx_size_2d[tx_size] - 1);
  int txb_skip_ctx = txb_ctx->txb_skip_ctx;
  aom_prob *nz_map = xd->fc->nz_map[txs_ctx][plane_type];

  const int bwl = b_width_log2_lookup[txsize_to_bsize[tx_size]] + 2;
  const int height = tx_size_high[tx_size];

  aom_prob(*coeff_base)[COEFF_BASE_CONTEXTS] =
      xd->fc->coeff_base[txs_ctx][plane_type];

  const SCAN_ORDER *const scan_order = get_scan(cm, tx_size, tx_type, mbmi);
  const int16_t *scan = scan_order->scan;
  const int16_t *iscan = scan_order->iscan;

  cost = 0;

  if (eob == 0) {
    cost = av1_cost_bit(xd->fc->txb_skip[txs_ctx][txb_skip_ctx], 1);
    return cost;
  }

  cost = av1_cost_bit(xd->fc->txb_skip[txs_ctx][txb_skip_ctx], 0);

#if CONFIG_TXK_SEL
  cost += av1_tx_type_cost(cpi, xd, mbmi->sb_type, plane, tx_size, tx_type);
#endif

  for (c = 0; c < eob; ++c) {
    tran_low_t v = qcoeff[scan[c]];
    int is_nz = (v != 0);
    int level = abs(v);

    if (c < seg_eob) {
      int coeff_ctx = get_nz_map_ctx(qcoeff, scan[c], bwl, height, iscan);
      cost += av1_cost_bit(nz_map[coeff_ctx], is_nz);
    }

    if (is_nz) {
      int ctx_ls[NUM_BASE_LEVELS] = { 0 };
      int sign = (v < 0) ? 1 : 0;

      // sign bit cost
      if (c == 0) {
        int dc_sign_ctx = txb_ctx->dc_sign_ctx;

        cost += av1_cost_bit(xd->fc->dc_sign[plane_type][dc_sign_ctx], sign);
      } else {
        cost += av1_cost_bit(128, sign);
      }

      get_base_ctx_set(qcoeff, scan[c], bwl, height, ctx_ls);

      int i;
      for (i = 0; i < NUM_BASE_LEVELS; ++i) {
        if (level <= i) continue;

        if (level == i + 1) {
          cost += av1_cost_bit(coeff_base[i][ctx_ls[i]], 1);
          continue;
        }
        cost += av1_cost_bit(coeff_base[i][ctx_ls[i]], 0);
      }

      if (level > NUM_BASE_LEVELS) {
        int idx;
        int ctx;

        ctx = get_br_ctx(qcoeff, scan[c], bwl, height);

        for (idx = 0; idx < COEFF_BASE_RANGE; ++idx) {
          if (level == (idx + 1 + NUM_BASE_LEVELS)) {
            cost +=
                av1_cost_bit(xd->fc->coeff_lps[txs_ctx][plane_type][ctx], 1);
            break;
          }
          cost += av1_cost_bit(xd->fc->coeff_lps[txs_ctx][plane_type][ctx], 0);
        }

        if (idx >= COEFF_BASE_RANGE) {
          // residual cost
          int r = level - COEFF_BASE_RANGE - NUM_BASE_LEVELS;
          int ri = r;
          int length = 0;

          while (ri) {
            ri >>= 1;
            ++length;
          }

          for (ri = 0; ri < length - 1; ++ri) cost += av1_cost_bit(128, 0);

          for (ri = length - 1; ri >= 0; --ri)
            cost += av1_cost_bit(128, (r >> ri) & 0x01);
        }
      }

      if (c < seg_eob) {
        int eob_ctx = get_eob_ctx(qcoeff, scan[c], txs_ctx);
        cost += av1_cost_bit(xd->fc->eob_flag[txs_ctx][plane_type][eob_ctx],
                             c == (eob - 1));
      }
    }
  }

  return cost;
}

static INLINE int has_base(tran_low_t qc, int base_idx) {
  const int level = base_idx + 1;
  return abs(qc) >= level;
}

static void gen_base_count_mag_arr(int (*base_count_arr)[MAX_TX_SQUARE],
                                   int (*base_mag_arr)[2],
                                   const tran_low_t *qcoeff, int stride,
                                   int height, int eob, const int16_t *scan) {
  for (int c = 0; c < eob; ++c) {
    const int coeff_idx = scan[c];  // raster order
    if (!has_base(qcoeff[coeff_idx], 0)) continue;
    const int row = coeff_idx / stride;
    const int col = coeff_idx % stride;
    int *mag = base_mag_arr[coeff_idx];
    get_mag(mag, qcoeff, stride, height, row, col, base_ref_offset,
            BASE_CONTEXT_POSITION_NUM);
    for (int i = 0; i < NUM_BASE_LEVELS; ++i) {
      if (!has_base(qcoeff[coeff_idx], i)) continue;
      int *count = base_count_arr[i] + coeff_idx;
      *count = get_level_count(qcoeff, stride, height, row, col, i,
                               base_ref_offset, BASE_CONTEXT_POSITION_NUM);
    }
  }
}

static void gen_nz_count_arr(int(*nz_count_arr), const tran_low_t *qcoeff,
                             int stride, int height, int eob,
                             const SCAN_ORDER *scan_order) {
  const int16_t *scan = scan_order->scan;
  const int16_t *iscan = scan_order->iscan;
  for (int c = 0; c < eob; ++c) {
    const int coeff_idx = scan[c];  // raster order
    const int row = coeff_idx / stride;
    const int col = coeff_idx % stride;
    nz_count_arr[coeff_idx] =
        get_nz_count(qcoeff, stride, height, row, col, iscan);
  }
}

static void gen_nz_ctx_arr(int (*nz_ctx_arr)[2], int(*nz_count_arr),
                           const tran_low_t *qcoeff, int bwl, int eob,
                           const SCAN_ORDER *scan_order) {
  const int16_t *scan = scan_order->scan;
  const int16_t *iscan = scan_order->iscan;
  for (int c = 0; c < eob; ++c) {
    const int coeff_idx = scan[c];  // raster order
    const int count = nz_count_arr[coeff_idx];
    nz_ctx_arr[coeff_idx][0] =
        get_nz_map_ctx_from_count(count, qcoeff, coeff_idx, bwl, iscan);
  }
}

static void gen_base_ctx_arr(int (*base_ctx_arr)[MAX_TX_SQUARE][2],
                             int (*base_count_arr)[MAX_TX_SQUARE],
                             int (*base_mag_arr)[2], const tran_low_t *qcoeff,
                             int stride, int eob, const int16_t *scan) {
  (void)qcoeff;
  for (int i = 0; i < NUM_BASE_LEVELS; ++i) {
    for (int c = 0; c < eob; ++c) {
      const int coeff_idx = scan[c];  // raster order
      if (!has_base(qcoeff[coeff_idx], i)) continue;
      const int row = coeff_idx / stride;
      const int col = coeff_idx % stride;
      const int count = base_count_arr[i][coeff_idx];
      const int *mag = base_mag_arr[coeff_idx];
      const int level = i + 1;
      base_ctx_arr[i][coeff_idx][0] =
          get_base_ctx_from_count_mag(row, col, count, mag[0], level);
    }
  }
}

static INLINE int has_br(tran_low_t qc) {
  return abs(qc) >= 1 + NUM_BASE_LEVELS;
}

static void gen_br_count_mag_arr(int *br_count_arr, int (*br_mag_arr)[2],
                                 const tran_low_t *qcoeff, int stride,
                                 int height, int eob, const int16_t *scan) {
  for (int c = 0; c < eob; ++c) {
    const int coeff_idx = scan[c];  // raster order
    if (!has_br(qcoeff[coeff_idx])) continue;
    const int row = coeff_idx / stride;
    const int col = coeff_idx % stride;
    int *count = br_count_arr + coeff_idx;
    int *mag = br_mag_arr[coeff_idx];
    *count = get_level_count(qcoeff, stride, height, row, col, NUM_BASE_LEVELS,
                             br_ref_offset, BR_CONTEXT_POSITION_NUM);
    get_mag(mag, qcoeff, stride, height, row, col, br_ref_offset,
            BR_CONTEXT_POSITION_NUM);
  }
}

static void gen_br_ctx_arr(int (*br_ctx_arr)[2], const int *br_count_arr,
                           int (*br_mag_arr)[2], const tran_low_t *qcoeff,
                           int stride, int eob, const int16_t *scan) {
  (void)qcoeff;
  for (int c = 0; c < eob; ++c) {
    const int coeff_idx = scan[c];  // raster order
    if (!has_br(qcoeff[coeff_idx])) continue;
    const int row = coeff_idx / stride;
    const int col = coeff_idx % stride;
    const int count = br_count_arr[coeff_idx];
    const int *mag = br_mag_arr[coeff_idx];
    br_ctx_arr[coeff_idx][0] =
        get_br_ctx_from_count_mag(row, col, count, mag[0]);
  }
}

static INLINE int get_sign_bit_cost(tran_low_t qc, int coeff_idx,
                                    const aom_prob *dc_sign_prob,
                                    int dc_sign_ctx) {
  const int sign = (qc < 0) ? 1 : 0;
  // sign bit cost
  if (coeff_idx == 0) {
    return av1_cost_bit(dc_sign_prob[dc_sign_ctx], sign);
  } else {
    return av1_cost_bit(128, sign);
  }
}
static INLINE int get_golomb_cost(int abs_qc) {
  if (abs_qc >= 1 + NUM_BASE_LEVELS + COEFF_BASE_RANGE) {
    // residual cost
    int r = abs_qc - COEFF_BASE_RANGE - NUM_BASE_LEVELS;
    int ri = r;
    int length = 0;

    while (ri) {
      ri >>= 1;
      ++length;
    }

    return av1_cost_literal(2 * length - 1);
  } else {
    return 0;
  }
}

// TODO(angiebird): add static once this function is called
void gen_txb_cache(TxbCache *txb_cache, TxbInfo *txb_info) {
  const int16_t *scan = txb_info->scan_order->scan;
  gen_nz_count_arr(txb_cache->nz_count_arr, txb_info->qcoeff, txb_info->stride,
                   txb_info->height, txb_info->eob, txb_info->scan_order);
  gen_nz_ctx_arr(txb_cache->nz_ctx_arr, txb_cache->nz_count_arr,
                 txb_info->qcoeff, txb_info->bwl, txb_info->eob,
                 txb_info->scan_order);
  gen_base_count_mag_arr(txb_cache->base_count_arr, txb_cache->base_mag_arr,
                         txb_info->qcoeff, txb_info->stride, txb_info->height,
                         txb_info->eob, scan);
  gen_base_ctx_arr(txb_cache->base_ctx_arr, txb_cache->base_count_arr,
                   txb_cache->base_mag_arr, txb_info->qcoeff, txb_info->stride,
                   txb_info->eob, scan);
  gen_br_count_mag_arr(txb_cache->br_count_arr, txb_cache->br_mag_arr,
                       txb_info->qcoeff, txb_info->stride, txb_info->height,
                       txb_info->eob, scan);
  gen_br_ctx_arr(txb_cache->br_ctx_arr, txb_cache->br_count_arr,
                 txb_cache->br_mag_arr, txb_info->qcoeff, txb_info->stride,
                 txb_info->eob, scan);
}

static INLINE aom_prob get_level_prob(int level, int coeff_idx,
                                      const TxbCache *txb_cache,
                                      const TxbProbs *txb_probs) {
  if (level == 0) {
    const int ctx = txb_cache->nz_ctx_arr[coeff_idx][0];
    return txb_probs->nz_map[ctx];
  } else if (level >= 1 && level < 1 + NUM_BASE_LEVELS) {
    const int idx = level - 1;
    const int ctx = txb_cache->base_ctx_arr[idx][coeff_idx][0];
    return txb_probs->coeff_base[idx][ctx];
  } else if (level >= 1 + NUM_BASE_LEVELS &&
             level < 1 + NUM_BASE_LEVELS + COEFF_BASE_RANGE) {
    const int ctx = txb_cache->br_ctx_arr[coeff_idx][0];
    return txb_probs->coeff_lps[ctx];
  } else if (level >= 1 + NUM_BASE_LEVELS + COEFF_BASE_RANGE) {
    printf("get_level_prob does not support golomb\n");
    assert(0);
    return 0;
  } else {
    assert(0);
    return 0;
  }
}

static INLINE tran_low_t get_lower_coeff(tran_low_t qc) {
  if (qc == 0) {
    return 0;
  }
  return qc > 0 ? qc - 1 : qc + 1;
}

static INLINE void update_mag_arr(int *mag_arr, int abs_qc) {
  if (mag_arr[0] == abs_qc) {
    mag_arr[1] -= 1;
    assert(mag_arr[1] >= 0);
  }
}

static INLINE int get_mag_from_mag_arr(const int *mag_arr) {
  int mag;
  if (mag_arr[1] > 0) {
    mag = mag_arr[0];
  } else if (mag_arr[0] > 0) {
    mag = mag_arr[0] - 1;
  } else {
    // no neighbor
    assert(mag_arr[0] == 0 && mag_arr[1] == 0);
    mag = 0;
  }
  return mag;
}

static int neighbor_level_down_update(int *new_count, int *new_mag, int count,
                                      const int *mag, int coeff_idx,
                                      tran_low_t abs_nb_coeff, int nb_coeff_idx,
                                      int level, const TxbInfo *txb_info) {
  *new_count = count;
  *new_mag = get_mag_from_mag_arr(mag);

  int update = 0;
  // check if br_count changes
  if (abs_nb_coeff == level) {
    update = 1;
    *new_count -= 1;
    assert(*new_count >= 0);
  }
  const int row = coeff_idx >> txb_info->bwl;
  const int col = coeff_idx - (row << txb_info->bwl);
  const int nb_row = nb_coeff_idx >> txb_info->bwl;
  const int nb_col = nb_coeff_idx - (nb_row << txb_info->bwl);

  // check if mag changes
  if (nb_row >= row && nb_col >= col) {
    if (abs_nb_coeff == mag[0]) {
      assert(mag[1] > 0);
      if (mag[1] == 1) {
        // the nb is the only qc with max mag
        *new_mag -= 1;
        assert(*new_mag >= 0);
        update = 1;
      }
    }
  }
  return update;
}

static int try_neighbor_level_down_br(int coeff_idx, int nb_coeff_idx,
                                      const TxbCache *txb_cache,
                                      const TxbProbs *txb_probs,
                                      const TxbInfo *txb_info) {
  const tran_low_t qc = txb_info->qcoeff[coeff_idx];
  const tran_low_t abs_qc = abs(qc);
  const int level = NUM_BASE_LEVELS + 1;
  if (abs_qc < level) return 0;

  const tran_low_t nb_coeff = txb_info->qcoeff[nb_coeff_idx];
  const tran_low_t abs_nb_coeff = abs(nb_coeff);
  const int count = txb_cache->br_count_arr[coeff_idx];
  const int *mag = txb_cache->br_mag_arr[coeff_idx];
  int new_count;
  int new_mag;
  const int update =
      neighbor_level_down_update(&new_count, &new_mag, count, mag, coeff_idx,
                                 abs_nb_coeff, nb_coeff_idx, level, txb_info);
  if (update) {
    const int row = coeff_idx >> txb_info->bwl;
    const int col = coeff_idx - (row << txb_info->bwl);
    const int ctx = txb_cache->br_ctx_arr[coeff_idx][0];
    const int org_cost = get_br_cost(abs_qc, ctx, txb_probs->coeff_lps);

    const int new_ctx = get_br_ctx_from_count_mag(row, col, new_count, new_mag);
    const int new_cost = get_br_cost(abs_qc, new_ctx, txb_probs->coeff_lps);
    const int cost_diff = -org_cost + new_cost;
    return cost_diff;
  } else {
    return 0;
  }
}

static int try_neighbor_level_down_base(int coeff_idx, int nb_coeff_idx,
                                        const TxbCache *txb_cache,
                                        const TxbProbs *txb_probs,
                                        const TxbInfo *txb_info) {
  const tran_low_t qc = txb_info->qcoeff[coeff_idx];
  const tran_low_t abs_qc = abs(qc);

  int cost_diff = 0;
  for (int base_idx = 0; base_idx < NUM_BASE_LEVELS; ++base_idx) {
    const int level = base_idx + 1;
    if (abs_qc < level) continue;

    const tran_low_t nb_coeff = txb_info->qcoeff[nb_coeff_idx];
    const tran_low_t abs_nb_coeff = abs(nb_coeff);

    const int count = txb_cache->base_count_arr[base_idx][coeff_idx];
    const int *mag = txb_cache->base_mag_arr[coeff_idx];
    int new_count;
    int new_mag;
    const int update =
        neighbor_level_down_update(&new_count, &new_mag, count, mag, coeff_idx,
                                   abs_nb_coeff, nb_coeff_idx, level, txb_info);
    if (update) {
      const int row = coeff_idx >> txb_info->bwl;
      const int col = coeff_idx - (row << txb_info->bwl);
      const int ctx = txb_cache->base_ctx_arr[base_idx][coeff_idx][0];
      const int org_cost =
          get_base_cost(abs_qc, ctx, txb_probs->coeff_base, base_idx);

      const int new_ctx =
          get_base_ctx_from_count_mag(row, col, new_count, new_mag, level);
      const int new_cost =
          get_base_cost(abs_qc, new_ctx, txb_probs->coeff_base, base_idx);
      cost_diff += -org_cost + new_cost;
    }
  }
  return cost_diff;
}

static int try_neighbor_level_down_nz(int coeff_idx, int nb_coeff_idx,
                                      const TxbCache *txb_cache,
                                      const TxbProbs *txb_probs,
                                      TxbInfo *txb_info) {
  // assume eob doesn't change
  const tran_low_t qc = txb_info->qcoeff[coeff_idx];
  const tran_low_t abs_qc = abs(qc);
  const tran_low_t nb_coeff = txb_info->qcoeff[nb_coeff_idx];
  const tran_low_t abs_nb_coeff = abs(nb_coeff);
  if (abs_nb_coeff != 1) return 0;
  const int16_t *iscan = txb_info->scan_order->iscan;
  const int scan_idx = iscan[coeff_idx];
  if (scan_idx == txb_info->seg_eob) return 0;
  const int nb_scan_idx = iscan[nb_coeff_idx];
  if (nb_scan_idx < scan_idx) {
    const int count = txb_cache->nz_count_arr[coeff_idx];
    assert(count > 0);
    txb_info->qcoeff[nb_coeff_idx] = get_lower_coeff(nb_coeff);
    const int new_ctx = get_nz_map_ctx_from_count(
        count - 1, txb_info->qcoeff, coeff_idx, txb_info->bwl, iscan);
    txb_info->qcoeff[nb_coeff_idx] = nb_coeff;
    const int ctx = txb_cache->nz_ctx_arr[coeff_idx][0];
    const int is_nz = abs_qc > 0;
    const int org_cost = av1_cost_bit(txb_probs->nz_map[ctx], is_nz);
    const int new_cost = av1_cost_bit(txb_probs->nz_map[new_ctx], is_nz);
    const int cost_diff = new_cost - org_cost;
    return cost_diff;
  } else {
    return 0;
  }
}

static int try_self_level_down(tran_low_t *low_coeff, int coeff_idx,
                               const TxbCache *txb_cache,
                               const TxbProbs *txb_probs, TxbInfo *txb_info) {
  const tran_low_t qc = txb_info->qcoeff[coeff_idx];
  if (qc == 0) {
    *low_coeff = 0;
    return 0;
  }
  const tran_low_t abs_qc = abs(qc);
  *low_coeff = get_lower_coeff(qc);
  int cost_diff;
  if (*low_coeff == 0) {
    const int scan_idx = txb_info->scan_order->iscan[coeff_idx];
    const aom_prob level_prob =
        get_level_prob(abs_qc, coeff_idx, txb_cache, txb_probs);
    const aom_prob low_level_prob =
        get_level_prob(abs(*low_coeff), coeff_idx, txb_cache, txb_probs);
    if (scan_idx < txb_info->seg_eob) {
      // When level-0, we code the binary of abs_qc > level
      // but when level-k k > 0 we code the binary of abs_qc == level
      // That's why wee need this special treatment for level-0 map
      // TODO(angiebird): make leve-0 consistent to other levels
      cost_diff = -av1_cost_bit(level_prob, 1) +
                  av1_cost_bit(low_level_prob, 0) -
                  av1_cost_bit(low_level_prob, 1);
    } else {
      cost_diff = -av1_cost_bit(level_prob, 1);
    }

    if (scan_idx < txb_info->seg_eob) {
      const int eob_ctx =
          get_eob_ctx(txb_info->qcoeff, coeff_idx, txb_info->txs_ctx);
      cost_diff -= av1_cost_bit(txb_probs->eob_flag[eob_ctx],
                                scan_idx == (txb_info->eob - 1));
    }

    const int sign_cost = get_sign_bit_cost(
        qc, coeff_idx, txb_probs->dc_sign_prob, txb_info->txb_ctx->dc_sign_ctx);
    cost_diff -= sign_cost;
  } else if (abs_qc < 1 + NUM_BASE_LEVELS + COEFF_BASE_RANGE) {
    const aom_prob level_prob =
        get_level_prob(abs_qc, coeff_idx, txb_cache, txb_probs);
    const aom_prob low_level_prob =
        get_level_prob(abs(*low_coeff), coeff_idx, txb_cache, txb_probs);
    cost_diff = -av1_cost_bit(level_prob, 1) + av1_cost_bit(low_level_prob, 1) -
                av1_cost_bit(low_level_prob, 0);
  } else if (abs_qc == 1 + NUM_BASE_LEVELS + COEFF_BASE_RANGE) {
    const aom_prob low_level_prob =
        get_level_prob(abs(*low_coeff), coeff_idx, txb_cache, txb_probs);
    cost_diff = -get_golomb_cost(abs_qc) + av1_cost_bit(low_level_prob, 1) -
                av1_cost_bit(low_level_prob, 0);
  } else {
    assert(abs_qc > 1 + NUM_BASE_LEVELS + COEFF_BASE_RANGE);
    const tran_low_t abs_low_coeff = abs(*low_coeff);
    cost_diff = -get_golomb_cost(abs_qc) + get_golomb_cost(abs_low_coeff);
  }
  return cost_diff;
}

#define COST_MAP_SIZE 5
#define COST_MAP_OFFSET 2

static INLINE int check_nz_neighbor(tran_low_t qc) { return abs(qc) == 1; }

static INLINE int check_base_neighbor(tran_low_t qc) {
  return abs(qc) <= 1 + NUM_BASE_LEVELS;
}

static INLINE int check_br_neighbor(tran_low_t qc) {
  return abs(qc) > BR_MAG_OFFSET;
}

// TODO(angiebird): add static to this function once it's called
int try_level_down(int coeff_idx, const TxbCache *txb_cache,
                   const TxbProbs *txb_probs, TxbInfo *txb_info,
                   int (*cost_map)[COST_MAP_SIZE]) {
  if (cost_map) {
    for (int i = 0; i < COST_MAP_SIZE; ++i) av1_zero(cost_map[i]);
  }

  tran_low_t qc = txb_info->qcoeff[coeff_idx];
  tran_low_t low_coeff;
  if (qc == 0) return 0;
  int accu_cost_diff = 0;

  const int16_t *iscan = txb_info->scan_order->iscan;
  const int eob = txb_info->eob;
  const int scan_idx = iscan[coeff_idx];
  if (scan_idx < eob) {
    const int cost_diff = try_self_level_down(&low_coeff, coeff_idx, txb_cache,
                                              txb_probs, txb_info);
    if (cost_map)
      cost_map[0 + COST_MAP_OFFSET][0 + COST_MAP_OFFSET] = cost_diff;
    accu_cost_diff += cost_diff;
  }

  const int row = coeff_idx >> txb_info->bwl;
  const int col = coeff_idx - (row << txb_info->bwl);
  if (check_nz_neighbor(qc)) {
    for (int i = 0; i < SIG_REF_OFFSET_NUM; ++i) {
      const int nb_row = row - sig_ref_offset[i][0];
      const int nb_col = col - sig_ref_offset[i][1];
      const int nb_coeff_idx = nb_row * txb_info->stride + nb_col;

      if (!(nb_row >= 0 && nb_col >= 0 && nb_row < txb_info->height &&
            nb_col < txb_info->stride))
        continue;

      const int nb_scan_idx = iscan[nb_coeff_idx];
      if (nb_scan_idx < eob) {
        const int cost_diff = try_neighbor_level_down_nz(
            nb_coeff_idx, coeff_idx, txb_cache, txb_probs, txb_info);
        if (cost_map)
          cost_map[nb_row - row + COST_MAP_OFFSET]
                  [nb_col - col + COST_MAP_OFFSET] += cost_diff;
        accu_cost_diff += cost_diff;
      }
    }
  }

  if (check_base_neighbor(qc)) {
    for (int i = 0; i < BASE_CONTEXT_POSITION_NUM; ++i) {
      const int nb_row = row - base_ref_offset[i][0];
      const int nb_col = col - base_ref_offset[i][1];
      const int nb_coeff_idx = nb_row * txb_info->stride + nb_col;

      if (!(nb_row >= 0 && nb_col >= 0 && nb_row < txb_info->height &&
            nb_col < txb_info->stride))
        continue;

      const int nb_scan_idx = iscan[nb_coeff_idx];
      if (nb_scan_idx < eob) {
        const int cost_diff = try_neighbor_level_down_base(
            nb_coeff_idx, coeff_idx, txb_cache, txb_probs, txb_info);
        if (cost_map)
          cost_map[nb_row - row + COST_MAP_OFFSET]
                  [nb_col - col + COST_MAP_OFFSET] += cost_diff;
        accu_cost_diff += cost_diff;
      }
    }
  }

  if (check_br_neighbor(qc)) {
    for (int i = 0; i < BR_CONTEXT_POSITION_NUM; ++i) {
      const int nb_row = row - br_ref_offset[i][0];
      const int nb_col = col - br_ref_offset[i][1];
      const int nb_coeff_idx = nb_row * txb_info->stride + nb_col;

      if (!(nb_row >= 0 && nb_col >= 0 && nb_row < txb_info->height &&
            nb_col < txb_info->stride))
        continue;

      const int nb_scan_idx = iscan[nb_coeff_idx];
      if (nb_scan_idx < eob) {
        const int cost_diff = try_neighbor_level_down_br(
            nb_coeff_idx, coeff_idx, txb_cache, txb_probs, txb_info);
        if (cost_map)
          cost_map[nb_row - row + COST_MAP_OFFSET]
                  [nb_col - col + COST_MAP_OFFSET] += cost_diff;
        accu_cost_diff += cost_diff;
      }
    }
  }

  return accu_cost_diff;
}

static int get_low_coeff_cost(int coeff_idx, const TxbCache *txb_cache,
                              const TxbProbs *txb_probs,
                              const TxbInfo *txb_info) {
  const tran_low_t qc = txb_info->qcoeff[coeff_idx];
  const int abs_qc = abs(qc);
  assert(abs_qc <= 1);
  int cost = 0;
  const int scan_idx = txb_info->scan_order->iscan[coeff_idx];
  if (scan_idx < txb_info->seg_eob) {
    const aom_prob level_prob =
        get_level_prob(0, coeff_idx, txb_cache, txb_probs);
    cost += av1_cost_bit(level_prob, qc != 0);
  }

  if (qc != 0) {
    const int base_idx = 0;
    const int ctx = txb_cache->base_ctx_arr[base_idx][coeff_idx][0];
    cost += get_base_cost(abs_qc, ctx, txb_probs->coeff_base, base_idx);
    if (scan_idx < txb_info->seg_eob) {
      const int eob_ctx =
          get_eob_ctx(txb_info->qcoeff, coeff_idx, txb_info->txs_ctx);
      cost += av1_cost_bit(txb_probs->eob_flag[eob_ctx],
                           scan_idx == (txb_info->eob - 1));
    }
    cost += get_sign_bit_cost(qc, coeff_idx, txb_probs->dc_sign_prob,
                              txb_info->txb_ctx->dc_sign_ctx);
  }
  return cost;
}

static INLINE void set_eob(TxbInfo *txb_info, int eob) {
  txb_info->eob = eob;
  txb_info->seg_eob = AOMMIN(eob, tx_size_2d[txb_info->tx_size] - 1);
}

// TODO(angiebird): add static to this function once it's called
int try_change_eob(int *new_eob, int coeff_idx, const TxbCache *txb_cache,
                   const TxbProbs *txb_probs, TxbInfo *txb_info) {
  assert(txb_info->eob > 0);
  const tran_low_t qc = txb_info->qcoeff[coeff_idx];
  const int abs_qc = abs(qc);
  if (abs_qc != 1) {
    *new_eob = -1;
    return 0;
  }
  const int16_t *iscan = txb_info->scan_order->iscan;
  const int16_t *scan = txb_info->scan_order->scan;
  const int scan_idx = iscan[coeff_idx];
  *new_eob = 0;
  int cost_diff = 0;
  cost_diff -= get_low_coeff_cost(coeff_idx, txb_cache, txb_probs, txb_info);
  // int coeff_cost =
  //     get_coeff_cost(qc, scan_idx, txb_info, txb_probs);
  // if (-cost_diff != coeff_cost) {
  //   printf("-cost_diff %d coeff_cost %d\n", -cost_diff, coeff_cost);
  //   get_low_coeff_cost(coeff_idx, txb_cache, txb_probs, txb_info);
  //   get_coeff_cost(qc, scan_idx, txb_info, txb_probs);
  // }
  for (int si = scan_idx - 1; si >= 0; --si) {
    const int ci = scan[si];
    if (txb_info->qcoeff[ci] != 0) {
      *new_eob = si + 1;
      break;
    } else {
      cost_diff -= get_low_coeff_cost(ci, txb_cache, txb_probs, txb_info);
    }
  }

  const int org_eob = txb_info->eob;
  set_eob(txb_info, *new_eob);
  cost_diff += try_level_down(coeff_idx, txb_cache, txb_probs, txb_info, NULL);
  set_eob(txb_info, org_eob);

  if (*new_eob > 0) {
    // Note that get_eob_ctx does NOT actually account for qcoeff, so we don't
    // need to lower down the qcoeff here
    const int eob_ctx =
        get_eob_ctx(txb_info->qcoeff, scan[*new_eob - 1], txb_info->txs_ctx);
    cost_diff -= av1_cost_bit(txb_probs->eob_flag[eob_ctx], 0);
    cost_diff += av1_cost_bit(txb_probs->eob_flag[eob_ctx], 1);
  } else {
    const int txb_skip_ctx = txb_info->txb_ctx->txb_skip_ctx;
    cost_diff -= av1_cost_bit(txb_probs->txb_skip[txb_skip_ctx], 0);
    cost_diff += av1_cost_bit(txb_probs->txb_skip[txb_skip_ctx], 1);
  }
  return cost_diff;
}

static INLINE tran_low_t qcoeff_to_dqcoeff(tran_low_t qc, int dqv, int shift) {
  int sgn = qc < 0 ? -1 : 1;
  return sgn * ((abs(qc) * dqv) >> shift);
}

// TODO(angiebird): add static to this function it's called
void update_level_down(int coeff_idx, TxbCache *txb_cache, TxbInfo *txb_info) {
  const tran_low_t qc = txb_info->qcoeff[coeff_idx];
  const int abs_qc = abs(qc);
  if (qc == 0) return;
  const tran_low_t low_coeff = get_lower_coeff(qc);
  txb_info->qcoeff[coeff_idx] = low_coeff;
  const int dqv = txb_info->dequant[coeff_idx != 0];
  txb_info->dqcoeff[coeff_idx] =
      qcoeff_to_dqcoeff(low_coeff, dqv, txb_info->shift);

  const int row = coeff_idx >> txb_info->bwl;
  const int col = coeff_idx - (row << txb_info->bwl);
  const int eob = txb_info->eob;
  const int16_t *iscan = txb_info->scan_order->iscan;
  for (int i = 0; i < SIG_REF_OFFSET_NUM; ++i) {
    const int nb_row = row - sig_ref_offset[i][0];
    const int nb_col = col - sig_ref_offset[i][1];

    if (!(nb_row >= 0 && nb_col >= 0 && nb_row < txb_info->height &&
          nb_col < txb_info->stride))
      continue;

    const int nb_coeff_idx = nb_row * txb_info->stride + nb_col;
    const int nb_scan_idx = iscan[nb_coeff_idx];
    if (nb_scan_idx < eob) {
      const int scan_idx = iscan[coeff_idx];
      if (scan_idx < nb_scan_idx) {
        const int level = 1;
        if (abs_qc == level) {
          txb_cache->nz_count_arr[nb_coeff_idx] -= 1;
          assert(txb_cache->nz_count_arr[nb_coeff_idx] >= 0);
        }
        const int count = txb_cache->nz_count_arr[nb_coeff_idx];
        txb_cache->nz_ctx_arr[nb_coeff_idx][0] = get_nz_map_ctx_from_count(
            count, txb_info->qcoeff, nb_coeff_idx, txb_info->bwl, iscan);
        // int ref_ctx = get_nz_map_ctx(txb_info->qcoeff, nb_coeff_idx,
        // txb_info->bwl, iscan);
        // if (ref_ctx != txb_cache->nz_ctx_arr[nb_coeff_idx][0])
        //   printf("nz ctx %d ref_ctx %d\n",
        //   txb_cache->nz_ctx_arr[nb_coeff_idx][0], ref_ctx);
      }
    }
  }

  for (int i = 0; i < BASE_CONTEXT_POSITION_NUM; ++i) {
    const int nb_row = row - base_ref_offset[i][0];
    const int nb_col = col - base_ref_offset[i][1];
    const int nb_coeff_idx = nb_row * txb_info->stride + nb_col;

    if (!(nb_row >= 0 && nb_col >= 0 && nb_row < txb_info->height &&
          nb_col < txb_info->stride))
      continue;

    const tran_low_t nb_coeff = txb_info->qcoeff[nb_coeff_idx];
    if (!has_base(nb_coeff, 0)) continue;
    const int nb_scan_idx = iscan[nb_coeff_idx];
    if (nb_scan_idx < eob) {
      if (row >= nb_row && col >= nb_col)
        update_mag_arr(txb_cache->base_mag_arr[nb_coeff_idx], abs_qc);
      const int mag =
          get_mag_from_mag_arr(txb_cache->base_mag_arr[nb_coeff_idx]);
      for (int base_idx = 0; base_idx < NUM_BASE_LEVELS; ++base_idx) {
        if (!has_base(nb_coeff, base_idx)) continue;
        const int level = base_idx + 1;
        if (abs_qc == level) {
          txb_cache->base_count_arr[base_idx][nb_coeff_idx] -= 1;
          assert(txb_cache->base_count_arr[base_idx][nb_coeff_idx] >= 0);
        }
        const int count = txb_cache->base_count_arr[base_idx][nb_coeff_idx];
        txb_cache->base_ctx_arr[base_idx][nb_coeff_idx][0] =
            get_base_ctx_from_count_mag(nb_row, nb_col, count, mag, level);
        // int ref_ctx = get_base_ctx(txb_info->qcoeff, nb_coeff_idx,
        // txb_info->bwl, level);
        // if (ref_ctx != txb_cache->base_ctx_arr[base_idx][nb_coeff_idx][0]) {
        //   printf("base ctx %d ref_ctx %d\n",
        //   txb_cache->base_ctx_arr[base_idx][nb_coeff_idx][0], ref_ctx);
        // }
      }
    }
  }

  for (int i = 0; i < BR_CONTEXT_POSITION_NUM; ++i) {
    const int nb_row = row - br_ref_offset[i][0];
    const int nb_col = col - br_ref_offset[i][1];
    const int nb_coeff_idx = nb_row * txb_info->stride + nb_col;

    if (!(nb_row >= 0 && nb_col >= 0 && nb_row < txb_info->height &&
          nb_col < txb_info->stride))
      continue;

    const int nb_scan_idx = iscan[nb_coeff_idx];
    const tran_low_t nb_coeff = txb_info->qcoeff[nb_coeff_idx];
    if (!has_br(nb_coeff)) continue;
    if (nb_scan_idx < eob) {
      const int level = 1 + NUM_BASE_LEVELS;
      if (abs_qc == level) {
        txb_cache->br_count_arr[nb_coeff_idx] -= 1;
        assert(txb_cache->br_count_arr[nb_coeff_idx] >= 0);
      }
      if (row >= nb_row && col >= nb_col)
        update_mag_arr(txb_cache->br_mag_arr[nb_coeff_idx], abs_qc);
      const int count = txb_cache->br_count_arr[nb_coeff_idx];
      const int mag = get_mag_from_mag_arr(txb_cache->br_mag_arr[nb_coeff_idx]);
      txb_cache->br_ctx_arr[nb_coeff_idx][0] =
          get_br_ctx_from_count_mag(nb_row, nb_col, count, mag);
      // int ref_ctx = get_level_ctx(txb_info->qcoeff, nb_coeff_idx,
      // txb_info->bwl);
      // if (ref_ctx != txb_cache->br_ctx_arr[nb_coeff_idx][0]) {
      //   printf("base ctx %d ref_ctx %d\n",
      //   txb_cache->br_ctx_arr[nb_coeff_idx][0], ref_ctx);
      // }
    }
  }
}

static int get_coeff_cost(tran_low_t qc, int scan_idx, TxbInfo *txb_info,
                          const TxbProbs *txb_probs) {
  const TXB_CTX *txb_ctx = txb_info->txb_ctx;
  const int is_nz = (qc != 0);
  const tran_low_t abs_qc = abs(qc);
  int cost = 0;
  const int16_t *scan = txb_info->scan_order->scan;
  const int16_t *iscan = txb_info->scan_order->iscan;

  if (scan_idx < txb_info->seg_eob) {
    int coeff_ctx = get_nz_map_ctx(txb_info->qcoeff, scan[scan_idx],
                                   txb_info->bwl, txb_info->height, iscan);
    cost += av1_cost_bit(txb_probs->nz_map[coeff_ctx], is_nz);
  }

  if (is_nz) {
    cost += get_sign_bit_cost(qc, scan_idx, txb_probs->dc_sign_prob,
                              txb_ctx->dc_sign_ctx);

    int ctx_ls[NUM_BASE_LEVELS] = { 0 };
    get_base_ctx_set(txb_info->qcoeff, scan[scan_idx], txb_info->bwl,
                     txb_info->height, ctx_ls);

    int i;
    for (i = 0; i < NUM_BASE_LEVELS; ++i) {
      cost += get_base_cost(abs_qc, ctx_ls[i], txb_probs->coeff_base, i);
    }

    if (abs_qc > NUM_BASE_LEVELS) {
      int ctx = get_br_ctx(txb_info->qcoeff, scan[scan_idx], txb_info->bwl,
                           txb_info->height);
      cost += get_br_cost(abs_qc, ctx, txb_probs->coeff_lps);
      cost += get_golomb_cost(abs_qc);
    }

    if (scan_idx < txb_info->seg_eob) {
      int eob_ctx =
          get_eob_ctx(txb_info->qcoeff, scan[scan_idx], txb_info->txs_ctx);
      cost += av1_cost_bit(txb_probs->eob_flag[eob_ctx],
                           scan_idx == (txb_info->eob - 1));
    }
  }
  return cost;
}

#if TEST_OPTIMIZE_TXB
#define ALL_REF_OFFSET_NUM 17
static int all_ref_offset[ALL_REF_OFFSET_NUM][2] = {
  { 0, 0 },  { -2, -1 }, { -2, 0 }, { -2, 1 }, { -1, -2 }, { -1, -1 },
  { -1, 0 }, { -1, 1 },  { 0, -2 }, { 0, -1 }, { 1, -2 },  { 1, -1 },
  { 1, 0 },  { 2, 0 },   { 0, 1 },  { 0, 2 },  { 1, 1 },
};

static int try_level_down_ref(int coeff_idx, const TxbProbs *txb_probs,
                              TxbInfo *txb_info,
                              int (*cost_map)[COST_MAP_SIZE]) {
  if (cost_map) {
    for (int i = 0; i < COST_MAP_SIZE; ++i) av1_zero(cost_map[i]);
  }
  tran_low_t qc = txb_info->qcoeff[coeff_idx];
  if (qc == 0) return 0;
  int row = coeff_idx >> txb_info->bwl;
  int col = coeff_idx - (row << txb_info->bwl);
  int org_cost = 0;
  for (int i = 0; i < ALL_REF_OFFSET_NUM; ++i) {
    int nb_row = row - all_ref_offset[i][0];
    int nb_col = col - all_ref_offset[i][1];
    int nb_coeff_idx = nb_row * txb_info->stride + nb_col;
    int nb_scan_idx = txb_info->scan_order->iscan[nb_coeff_idx];
    if (nb_scan_idx < txb_info->eob && nb_row >= 0 && nb_col >= 0 &&
        nb_row < txb_info->stride && nb_col < txb_info->stride) {
      tran_low_t nb_coeff = txb_info->qcoeff[nb_coeff_idx];
      int cost = get_coeff_cost(nb_coeff, nb_scan_idx, txb_info, txb_probs);
      if (cost_map)
        cost_map[nb_row - row + COST_MAP_OFFSET]
                [nb_col - col + COST_MAP_OFFSET] -= cost;
      org_cost += cost;
    }
  }
  txb_info->qcoeff[coeff_idx] = get_lower_coeff(qc);
  int new_cost = 0;
  for (int i = 0; i < ALL_REF_OFFSET_NUM; ++i) {
    int nb_row = row - all_ref_offset[i][0];
    int nb_col = col - all_ref_offset[i][1];
    int nb_coeff_idx = nb_row * txb_info->stride + nb_col;
    int nb_scan_idx = txb_info->scan_order->iscan[nb_coeff_idx];
    if (nb_scan_idx < txb_info->eob && nb_row >= 0 && nb_col >= 0 &&
        nb_row < txb_info->stride && nb_col < txb_info->stride) {
      tran_low_t nb_coeff = txb_info->qcoeff[nb_coeff_idx];
      int cost = get_coeff_cost(nb_coeff, nb_scan_idx, txb_info, txb_probs);
      if (cost_map)
        cost_map[nb_row - row + COST_MAP_OFFSET]
                [nb_col - col + COST_MAP_OFFSET] += cost;
      new_cost += cost;
    }
  }
  txb_info->qcoeff[coeff_idx] = qc;
  return new_cost - org_cost;
}

static void test_level_down(int coeff_idx, const TxbCache *txb_cache,
                            const TxbProbs *txb_probs, TxbInfo *txb_info) {
  int cost_map[COST_MAP_SIZE][COST_MAP_SIZE];
  int ref_cost_map[COST_MAP_SIZE][COST_MAP_SIZE];
  const int cost_diff =
      try_level_down(coeff_idx, txb_cache, txb_probs, txb_info, cost_map);
  const int cost_diff_ref =
      try_level_down_ref(coeff_idx, txb_probs, txb_info, ref_cost_map);
  if (cost_diff != cost_diff_ref) {
    printf("qc %d cost_diff %d cost_diff_ref %d\n", txb_info->qcoeff[coeff_idx],
           cost_diff, cost_diff_ref);
    for (int r = 0; r < COST_MAP_SIZE; ++r) {
      for (int c = 0; c < COST_MAP_SIZE; ++c) {
        printf("%d:%d ", cost_map[r][c], ref_cost_map[r][c]);
      }
      printf("\n");
    }
  }
}
#endif

// TODO(angiebird): make this static once it's called
int get_txb_cost(TxbInfo *txb_info, const TxbProbs *txb_probs) {
  int cost = 0;
  int txb_skip_ctx = txb_info->txb_ctx->txb_skip_ctx;
  const int16_t *scan = txb_info->scan_order->scan;
  if (txb_info->eob == 0) {
    cost = av1_cost_bit(txb_probs->txb_skip[txb_skip_ctx], 1);
    return cost;
  }
  cost = av1_cost_bit(txb_probs->txb_skip[txb_skip_ctx], 0);
  for (int c = 0; c < txb_info->eob; ++c) {
    tran_low_t qc = txb_info->qcoeff[scan[c]];
    int coeff_cost = get_coeff_cost(qc, c, txb_info, txb_probs);
    cost += coeff_cost;
  }
  return cost;
}

#if TEST_OPTIMIZE_TXB
void test_try_change_eob(TxbInfo *txb_info, TxbProbs *txb_probs,
                         TxbCache *txb_cache) {
  int eob = txb_info->eob;
  const int16_t *scan = txb_info->scan_order->scan;
  if (eob > 0) {
    int last_si = eob - 1;
    int last_ci = scan[last_si];
    int last_coeff = txb_info->qcoeff[last_ci];
    if (abs(last_coeff) == 1) {
      int new_eob;
      int cost_diff =
          try_change_eob(&new_eob, last_ci, txb_cache, txb_probs, txb_info);
      int org_eob = txb_info->eob;
      int cost = get_txb_cost(txb_info, txb_probs);

      txb_info->qcoeff[last_ci] = get_lower_coeff(last_coeff);
      set_eob(txb_info, new_eob);
      int new_cost = get_txb_cost(txb_info, txb_probs);
      set_eob(txb_info, org_eob);
      txb_info->qcoeff[last_ci] = last_coeff;

      int ref_cost_diff = -cost + new_cost;
      if (cost_diff != ref_cost_diff)
        printf("org_eob %d new_eob %d cost_diff %d ref_cost_diff %d\n", org_eob,
               new_eob, cost_diff, ref_cost_diff);
    }
  }
}
#endif

static INLINE int64_t get_coeff_dist(tran_low_t tcoeff, tran_low_t dqcoeff,
                                     int shift) {
  const int64_t diff = (tcoeff - dqcoeff) * (1 << shift);
  const int64_t error = diff * diff;
  return error;
}

typedef struct LevelDownStats {
  int update;
  tran_low_t low_qc;
  tran_low_t low_dqc;
  int64_t rd_diff;
  int cost_diff;
  int64_t dist_diff;
  int new_eob;
} LevelDownStats;

void try_level_down_facade(LevelDownStats *stats, int scan_idx,
                           const TxbCache *txb_cache, const TxbProbs *txb_probs,
                           TxbInfo *txb_info) {
  const int16_t *scan = txb_info->scan_order->scan;
  const int coeff_idx = scan[scan_idx];
  const tran_low_t qc = txb_info->qcoeff[coeff_idx];
  stats->new_eob = -1;
  stats->update = 0;
  if (qc == 0) {
    return;
  }

  const tran_low_t tqc = txb_info->tcoeff[coeff_idx];
  const int dqv = txb_info->dequant[coeff_idx != 0];

  const tran_low_t dqc = qcoeff_to_dqcoeff(qc, dqv, txb_info->shift);
  const int64_t dqc_dist = get_coeff_dist(tqc, dqc, txb_info->shift);

  stats->low_qc = get_lower_coeff(qc);
  stats->low_dqc = qcoeff_to_dqcoeff(stats->low_qc, dqv, txb_info->shift);
  const int64_t low_dqc_dist =
      get_coeff_dist(tqc, stats->low_dqc, txb_info->shift);

  stats->dist_diff = -dqc_dist + low_dqc_dist;
  stats->cost_diff = 0;
  stats->new_eob = txb_info->eob;
  if (scan_idx == txb_info->eob - 1 && abs(qc) == 1) {
    stats->cost_diff = try_change_eob(&stats->new_eob, coeff_idx, txb_cache,
                                      txb_probs, txb_info);
  } else {
    stats->cost_diff =
        try_level_down(coeff_idx, txb_cache, txb_probs, txb_info, NULL);
#if TEST_OPTIMIZE_TXB
    test_level_down(coeff_idx, txb_cache, txb_probs, txb_info);
#endif
  }
  stats->rd_diff = RDCOST(txb_info->rdmult, stats->cost_diff, stats->dist_diff);
  if (stats->rd_diff < 0) stats->update = 1;
  return;
}

static int optimize_txb(TxbInfo *txb_info, const TxbProbs *txb_probs,
                        TxbCache *txb_cache, int dry_run) {
  int update = 0;
  if (txb_info->eob == 0) return update;
  int cost_diff = 0;
  int64_t dist_diff = 0;
  int64_t rd_diff = 0;
  const int max_eob = tx_size_2d[txb_info->tx_size];

#if TEST_OPTIMIZE_TXB
  int64_t sse;
  int64_t org_dist =
      av1_block_error_c(txb_info->tcoeff, txb_info->dqcoeff, max_eob, &sse) *
      (1 << (2 * txb_info->shift));
  int org_cost = get_txb_cost(txb_info, txb_probs);
#endif

  tran_low_t *org_qcoeff = txb_info->qcoeff;
  tran_low_t *org_dqcoeff = txb_info->dqcoeff;

  tran_low_t tmp_qcoeff[MAX_TX_SQUARE];
  tran_low_t tmp_dqcoeff[MAX_TX_SQUARE];
  const int org_eob = txb_info->eob;
  if (dry_run) {
    memcpy(tmp_qcoeff, org_qcoeff, sizeof(org_qcoeff[0]) * max_eob);
    memcpy(tmp_dqcoeff, org_dqcoeff, sizeof(org_dqcoeff[0]) * max_eob);
    txb_info->qcoeff = tmp_qcoeff;
    txb_info->dqcoeff = tmp_dqcoeff;
  }

  const int16_t *scan = txb_info->scan_order->scan;

  // forward optimize the nz_map
  const int cur_eob = txb_info->eob;
  for (int si = 0; si < cur_eob; ++si) {
    const int coeff_idx = scan[si];
    tran_low_t qc = txb_info->qcoeff[coeff_idx];
    if (abs(qc) == 1) {
      LevelDownStats stats;
      try_level_down_facade(&stats, si, txb_cache, txb_probs, txb_info);
      if (stats.update) {
        update = 1;
        cost_diff += stats.cost_diff;
        dist_diff += stats.dist_diff;
        rd_diff += stats.rd_diff;
        update_level_down(coeff_idx, txb_cache, txb_info);
        set_eob(txb_info, stats.new_eob);
      }
    }
  }

  // backward optimize the level-k map
  for (int si = txb_info->eob - 1; si >= 0; --si) {
    LevelDownStats stats;
    try_level_down_facade(&stats, si, txb_cache, txb_probs, txb_info);
    const int coeff_idx = scan[si];
    if (stats.update) {
#if TEST_OPTIMIZE_TXB
// printf("si %d low_qc %d cost_diff %d dist_diff %ld rd_diff %ld eob %d new_eob
// %d\n", si, stats.low_qc, stats.cost_diff, stats.dist_diff, stats.rd_diff,
// txb_info->eob, stats.new_eob);
#endif
      update = 1;
      cost_diff += stats.cost_diff;
      dist_diff += stats.dist_diff;
      rd_diff += stats.rd_diff;
      update_level_down(coeff_idx, txb_cache, txb_info);
      set_eob(txb_info, stats.new_eob);
    }
    if (si > txb_info->eob) si = txb_info->eob;
  }
#if TEST_OPTIMIZE_TXB
  int64_t new_dist =
      av1_block_error_c(txb_info->tcoeff, txb_info->dqcoeff, max_eob, &sse) *
      (1 << (2 * txb_info->shift));
  int new_cost = get_txb_cost(txb_info, txb_probs);
  int64_t ref_dist_diff = new_dist - org_dist;
  int ref_cost_diff = new_cost - org_cost;
  if (cost_diff != ref_cost_diff || dist_diff != ref_dist_diff)
    printf(
        "overall rd_diff %ld\ncost_diff %d ref_cost_diff%d\ndist_diff %ld "
        "ref_dist_diff %ld\neob %d new_eob %d\n\n",
        rd_diff, cost_diff, ref_cost_diff, dist_diff, ref_dist_diff, org_eob,
        txb_info->eob);
#endif
  if (dry_run) {
    txb_info->qcoeff = org_qcoeff;
    txb_info->dqcoeff = org_dqcoeff;
    set_eob(txb_info, org_eob);
  }
  return update;
}

// These numbers are empirically obtained.
static const int plane_rd_mult[REF_TYPES][PLANE_TYPES] = {
  { 17, 13 }, { 16, 10 },
};

int av1_optimize_txb(const AV1_COMMON *cm, MACROBLOCK *x, int plane,
                     int blk_row, int blk_col, int block, TX_SIZE tx_size,
                     TXB_CTX *txb_ctx) {
  MACROBLOCKD *const xd = &x->e_mbd;
  const PLANE_TYPE plane_type = get_plane_type(plane);
  const TX_SIZE txs_ctx = get_txsize_context(tx_size);
  const TX_TYPE tx_type =
      av1_get_tx_type(plane_type, xd, blk_row, blk_col, block, tx_size);
  const MB_MODE_INFO *mbmi = &xd->mi[0]->mbmi;
  const struct macroblock_plane *p = &x->plane[plane];
  struct macroblockd_plane *pd = &xd->plane[plane];
  const int eob = p->eobs[block];
  tran_low_t *qcoeff = BLOCK_OFFSET(p->qcoeff, block);
  tran_low_t *dqcoeff = BLOCK_OFFSET(pd->dqcoeff, block);
  const tran_low_t *tcoeff = BLOCK_OFFSET(p->coeff, block);
  const int16_t *dequant = pd->dequant;
  const int seg_eob = AOMMIN(eob, tx_size_2d[tx_size] - 1);
  const aom_prob *nz_map = xd->fc->nz_map[txs_ctx][plane_type];

  const int bwl = b_width_log2_lookup[txsize_to_bsize[tx_size]] + 2;
  const int stride = 1 << bwl;
  const int height = tx_size_high[tx_size];
  aom_prob(*coeff_base)[COEFF_BASE_CONTEXTS] =
      xd->fc->coeff_base[txs_ctx][plane_type];

  const aom_prob *coeff_lps = xd->fc->coeff_lps[txs_ctx][plane_type];

  const int is_inter = is_inter_block(mbmi);
  const SCAN_ORDER *const scan_order = get_scan(cm, tx_size, tx_type, mbmi);

  const TxbProbs txb_probs = { xd->fc->dc_sign[plane_type],
                               nz_map,
                               coeff_base,
                               coeff_lps,
                               xd->fc->eob_flag[txs_ctx][plane_type],
                               xd->fc->txb_skip[txs_ctx] };

  const int shift = av1_get_tx_scale(tx_size);
  const int64_t rdmult =
      (x->rdmult * plane_rd_mult[is_inter][plane_type] + 2) >> 2;

  TxbInfo txb_info = { qcoeff,  dqcoeff, tcoeff,     dequant, shift,
                       tx_size, txs_ctx, bwl,        stride,  height,
                       eob,     seg_eob, scan_order, txb_ctx, rdmult };

  TxbCache txb_cache;
  gen_txb_cache(&txb_cache, &txb_info);

  const int update = optimize_txb(&txb_info, &txb_probs, &txb_cache, 0);
  if (update) p->eobs[block] = txb_info.eob;
  return txb_info.eob;
}
int av1_get_txb_entropy_context(const tran_low_t *qcoeff,
                                const SCAN_ORDER *scan_order, int eob) {
  const int16_t *scan = scan_order->scan;
  int cul_level = 0;
  int c;
  for (c = 0; c < eob; ++c) {
    cul_level += abs(qcoeff[scan[c]]);
  }

  cul_level = AOMMIN(COEFF_CONTEXT_MASK, cul_level);
  set_dc_sign(&cul_level, qcoeff[0]);

  return cul_level;
}

void av1_update_txb_context_b(int plane, int block, int blk_row, int blk_col,
                              BLOCK_SIZE plane_bsize, TX_SIZE tx_size,
                              void *arg) {
  struct tokenize_b_args *const args = arg;
  const AV1_COMP *cpi = args->cpi;
  const AV1_COMMON *cm = &cpi->common;
  ThreadData *const td = args->td;
  MACROBLOCK *const x = &td->mb;
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *mbmi = &xd->mi[0]->mbmi;
  struct macroblock_plane *p = &x->plane[plane];
  struct macroblockd_plane *pd = &xd->plane[plane];
  const uint16_t eob = p->eobs[block];
  const tran_low_t *qcoeff = BLOCK_OFFSET(p->qcoeff, block);
  const PLANE_TYPE plane_type = pd->plane_type;
  const TX_TYPE tx_type =
      av1_get_tx_type(plane_type, xd, blk_row, blk_col, block, tx_size);
  const SCAN_ORDER *const scan_order = get_scan(cm, tx_size, tx_type, mbmi);
  (void)plane_bsize;

  int cul_level = av1_get_txb_entropy_context(qcoeff, scan_order, eob);
  av1_set_contexts(xd, pd, plane, tx_size, cul_level, blk_col, blk_row);
}

void av1_update_and_record_txb_context(int plane, int block, int blk_row,
                                       int blk_col, BLOCK_SIZE plane_bsize,
                                       TX_SIZE tx_size, void *arg) {
  struct tokenize_b_args *const args = arg;
  const AV1_COMP *cpi = args->cpi;
  const AV1_COMMON *cm = &cpi->common;
  ThreadData *const td = args->td;
  MACROBLOCK *const x = &td->mb;
  MACROBLOCKD *const xd = &x->e_mbd;
  struct macroblock_plane *p = &x->plane[plane];
  struct macroblockd_plane *pd = &xd->plane[plane];
  MB_MODE_INFO *mbmi = &xd->mi[0]->mbmi;
  int eob = p->eobs[block], update_eob = 0;
  const PLANE_TYPE plane_type = pd->plane_type;
  const tran_low_t *qcoeff = BLOCK_OFFSET(p->qcoeff, block);
  tran_low_t *tcoeff = BLOCK_OFFSET(x->mbmi_ext->tcoeff[plane], block);
  const int segment_id = mbmi->segment_id;
  const TX_TYPE tx_type =
      av1_get_tx_type(plane_type, xd, blk_row, blk_col, block, tx_size);
  const SCAN_ORDER *const scan_order = get_scan(cm, tx_size, tx_type, mbmi);
  const int16_t *scan = scan_order->scan;
  const int16_t *iscan = scan_order->iscan;
  const int seg_eob = get_tx_eob(&cpi->common.seg, segment_id, tx_size);
  int c, i;
  TXB_CTX txb_ctx;
  get_txb_ctx(plane_bsize, tx_size, plane, pd->above_context + blk_col,
              pd->left_context + blk_row, &txb_ctx);
  const int bwl = b_width_log2_lookup[txsize_to_bsize[tx_size]] + 2;
  const int height = tx_size_high[tx_size];
  int cul_level = 0;
  unsigned int(*nz_map_count)[SIG_COEF_CONTEXTS][2];

  TX_SIZE txsize_ctx = get_txsize_context(tx_size);

  nz_map_count = &td->counts->nz_map[txsize_ctx][plane_type];

  memcpy(tcoeff, qcoeff, sizeof(*tcoeff) * seg_eob);

  ++td->counts->txb_skip[txsize_ctx][txb_ctx.txb_skip_ctx][eob == 0];
  x->mbmi_ext->txb_skip_ctx[plane][block] = txb_ctx.txb_skip_ctx;

  x->mbmi_ext->eobs[plane][block] = eob;

  if (eob == 0) {
    av1_set_contexts(xd, pd, plane, tx_size, 0, blk_col, blk_row);
    return;
  }

#if CONFIG_TXK_SEL
  av1_update_tx_type_count(cm, xd, blk_row, blk_col, block, plane,
                           mbmi->sb_type, get_min_tx_size(tx_size), td->counts);
#endif

  for (c = 0; c < eob; ++c) {
    tran_low_t v = qcoeff[scan[c]];
    int is_nz = (v != 0);
    int coeff_ctx = get_nz_map_ctx(tcoeff, scan[c], bwl, height, iscan);
    int eob_ctx = get_eob_ctx(tcoeff, scan[c], txsize_ctx);

    if (c == seg_eob - 1) break;

    ++(*nz_map_count)[coeff_ctx][is_nz];

    if (is_nz) {
      ++td->counts->eob_flag[txsize_ctx][plane_type][eob_ctx][c == (eob - 1)];
    }
  }

  // Reverse process order to handle coefficient level and sign.
  for (i = 0; i < NUM_BASE_LEVELS; ++i) {
    update_eob = 0;
    for (c = eob - 1; c >= 0; --c) {
      tran_low_t v = qcoeff[scan[c]];
      tran_low_t level = abs(v);
      int ctx;

      if (level <= i) continue;

      ctx = get_base_ctx(tcoeff, scan[c], bwl, height, i + 1);

      if (level == i + 1) {
        ++td->counts->coeff_base[txsize_ctx][plane_type][i][ctx][1];
        if (c == 0) {
          int dc_sign_ctx = txb_ctx.dc_sign_ctx;

          ++td->counts->dc_sign[plane_type][dc_sign_ctx][v < 0];
          x->mbmi_ext->dc_sign_ctx[plane][block] = dc_sign_ctx;
        }
        cul_level += level;
        continue;
      }
      ++td->counts->coeff_base[txsize_ctx][plane_type][i][ctx][0];
      update_eob = AOMMAX(update_eob, c);
    }
  }

  for (c = update_eob; c >= 0; --c) {
    tran_low_t v = qcoeff[scan[c]];
    tran_low_t level = abs(v);
    int idx;
    int ctx;

    if (level <= NUM_BASE_LEVELS) continue;

    cul_level += level;
    if (c == 0) {
      int dc_sign_ctx = txb_ctx.dc_sign_ctx;

      ++td->counts->dc_sign[plane_type][dc_sign_ctx][v < 0];
      x->mbmi_ext->dc_sign_ctx[plane][block] = dc_sign_ctx;
    }

    // level is above 1.
    ctx = get_br_ctx(tcoeff, scan[c], bwl, height);
    for (idx = 0; idx < COEFF_BASE_RANGE; ++idx) {
      if (level == (idx + 1 + NUM_BASE_LEVELS)) {
        ++td->counts->coeff_lps[txsize_ctx][plane_type][ctx][1];
        break;
      }
      ++td->counts->coeff_lps[txsize_ctx][plane_type][ctx][0];
    }
    if (idx < COEFF_BASE_RANGE) continue;

    // use 0-th order Golomb code to handle the residual level.
  }

  cul_level = AOMMIN(COEFF_CONTEXT_MASK, cul_level);

  // DC value
  set_dc_sign(&cul_level, tcoeff[0]);
  av1_set_contexts(xd, pd, plane, tx_size, cul_level, blk_col, blk_row);

#if CONFIG_ADAPT_SCAN
  // Since dqcoeff is not available here, we pass qcoeff into
  // av1_update_scan_count_facade(). The update behavior should be the same
  // because av1_update_scan_count_facade() only cares if coefficients are zero
  // or not.
  av1_update_scan_count_facade((AV1_COMMON *)cm, td->counts, tx_size, tx_type,
                               qcoeff, eob);
#endif
}

void av1_update_txb_context(const AV1_COMP *cpi, ThreadData *td,
                            RUN_TYPE dry_run, BLOCK_SIZE bsize, int *rate,
                            int mi_row, int mi_col) {
  const AV1_COMMON *const cm = &cpi->common;
  MACROBLOCK *const x = &td->mb;
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = &xd->mi[0]->mbmi;
  const int ctx = av1_get_skip_context(xd);
  const int skip_inc =
      !segfeature_active(&cm->seg, mbmi->segment_id, SEG_LVL_SKIP);
  struct tokenize_b_args arg = { cpi, td, NULL, 0 };
  (void)rate;
  (void)mi_row;
  (void)mi_col;
  if (mbmi->skip) {
    if (!dry_run) td->counts->skip[ctx][1] += skip_inc;
    av1_reset_skip_context(xd, mi_row, mi_col, bsize);
    return;
  }

  if (!dry_run) {
    td->counts->skip[ctx][0] += skip_inc;
    av1_foreach_transformed_block(xd, bsize, mi_row, mi_col,
                                  av1_update_and_record_txb_context, &arg);
  } else if (dry_run == DRY_RUN_NORMAL) {
    av1_foreach_transformed_block(xd, bsize, mi_row, mi_col,
                                  av1_update_txb_context_b, &arg);
  } else {
    printf("DRY_RUN_COSTCOEFFS is not supported yet\n");
    assert(0);
  }
}

static void find_new_prob(unsigned int *branch_cnt, aom_prob *oldp,
                          int *savings, int *update, aom_writer *const bc) {
  const aom_prob upd = DIFF_UPDATE_PROB;
  int u = 0;
  aom_prob newp = get_binary_prob(branch_cnt[0], branch_cnt[1]);
  int s = av1_prob_diff_update_savings_search(branch_cnt, *oldp, &newp, upd, 1);

  if (s > 0 && newp != *oldp) u = 1;

  if (u)
    *savings += s - (int)(av1_cost_zero(upd));  // TODO(jingning): 1?
  else
    *savings -= (int)(av1_cost_zero(upd));

  if (update) {
    ++update[u];
    return;
  }

  aom_write(bc, u, upd);
  if (u) {
    /* send/use new probability */
    av1_write_prob_diff_update(bc, newp, *oldp);
    *oldp = newp;
  }
}

static void write_txb_probs(aom_writer *const bc, AV1_COMP *cpi,
                            TX_SIZE tx_size) {
  FRAME_CONTEXT *fc = cpi->common.fc;
  FRAME_COUNTS *counts = cpi->td.counts;
  int savings = 0;
  int update[2] = { 0, 0 };
  int plane, ctx, level;

  for (ctx = 0; ctx < TXB_SKIP_CONTEXTS; ++ctx) {
    find_new_prob(counts->txb_skip[tx_size][ctx], &fc->txb_skip[tx_size][ctx],
                  &savings, update, bc);
  }

  for (plane = 0; plane < PLANE_TYPES; ++plane) {
    for (ctx = 0; ctx < SIG_COEF_CONTEXTS; ++ctx) {
      find_new_prob(counts->nz_map[tx_size][plane][ctx],
                    &fc->nz_map[tx_size][plane][ctx], &savings, update, bc);
    }
  }

  for (plane = 0; plane < PLANE_TYPES; ++plane) {
    for (ctx = 0; ctx < EOB_COEF_CONTEXTS; ++ctx) {
      find_new_prob(counts->eob_flag[tx_size][plane][ctx],
                    &fc->eob_flag[tx_size][plane][ctx], &savings, update, bc);
    }
  }

  for (level = 0; level < NUM_BASE_LEVELS; ++level) {
    for (plane = 0; plane < PLANE_TYPES; ++plane) {
      for (ctx = 0; ctx < COEFF_BASE_CONTEXTS; ++ctx) {
        find_new_prob(counts->coeff_base[tx_size][plane][level][ctx],
                      &fc->coeff_base[tx_size][plane][level][ctx], &savings,
                      update, bc);
      }
    }
  }

  for (plane = 0; plane < PLANE_TYPES; ++plane) {
    for (ctx = 0; ctx < LEVEL_CONTEXTS; ++ctx) {
      find_new_prob(counts->coeff_lps[tx_size][plane][ctx],
                    &fc->coeff_lps[tx_size][plane][ctx], &savings, update, bc);
    }
  }

  // Decide if to update the model for this tx_size
  if (update[1] == 0 || savings < 0) {
    aom_write_bit(bc, 0);
    return;
  }
  aom_write_bit(bc, 1);

  for (ctx = 0; ctx < TXB_SKIP_CONTEXTS; ++ctx) {
    find_new_prob(counts->txb_skip[tx_size][ctx], &fc->txb_skip[tx_size][ctx],
                  &savings, NULL, bc);
  }

  for (plane = 0; plane < PLANE_TYPES; ++plane) {
    for (ctx = 0; ctx < SIG_COEF_CONTEXTS; ++ctx) {
      find_new_prob(counts->nz_map[tx_size][plane][ctx],
                    &fc->nz_map[tx_size][plane][ctx], &savings, NULL, bc);
    }
  }

  for (plane = 0; plane < PLANE_TYPES; ++plane) {
    for (ctx = 0; ctx < EOB_COEF_CONTEXTS; ++ctx) {
      find_new_prob(counts->eob_flag[tx_size][plane][ctx],
                    &fc->eob_flag[tx_size][plane][ctx], &savings, NULL, bc);
    }
  }

  for (level = 0; level < NUM_BASE_LEVELS; ++level) {
    for (plane = 0; plane < PLANE_TYPES; ++plane) {
      for (ctx = 0; ctx < COEFF_BASE_CONTEXTS; ++ctx) {
        find_new_prob(counts->coeff_base[tx_size][plane][level][ctx],
                      &fc->coeff_base[tx_size][plane][level][ctx], &savings,
                      NULL, bc);
      }
    }
  }

  for (plane = 0; plane < PLANE_TYPES; ++plane) {
    for (ctx = 0; ctx < LEVEL_CONTEXTS; ++ctx) {
      find_new_prob(counts->coeff_lps[tx_size][plane][ctx],
                    &fc->coeff_lps[tx_size][plane][ctx], &savings, NULL, bc);
    }
  }
}

void av1_write_txb_probs(AV1_COMP *cpi, aom_writer *w) {
  const TX_MODE tx_mode = cpi->common.tx_mode;
  const TX_SIZE max_tx_size = tx_mode_to_biggest_tx_size[tx_mode];
  TX_SIZE tx_size;
  int ctx, plane;

  for (plane = 0; plane < PLANE_TYPES; ++plane)
    for (ctx = 0; ctx < DC_SIGN_CONTEXTS; ++ctx)
      av1_cond_prob_diff_update(w, &cpi->common.fc->dc_sign[plane][ctx],
                                cpi->td.counts->dc_sign[plane][ctx], 1);

  for (tx_size = TX_4X4; tx_size <= max_tx_size; ++tx_size)
    write_txb_probs(w, cpi, tx_size);
}

#if CONFIG_TXK_SEL
int64_t av1_search_txk_type(const AV1_COMP *cpi, MACROBLOCK *x, int plane,
                            int block, int blk_row, int blk_col,
                            BLOCK_SIZE plane_bsize, TX_SIZE tx_size,
                            const ENTROPY_CONTEXT *a, const ENTROPY_CONTEXT *l,
                            int use_fast_coef_costing, RD_STATS *rd_stats) {
  const AV1_COMMON *cm = &cpi->common;
  MACROBLOCKD *xd = &x->e_mbd;
  MB_MODE_INFO *mbmi = &xd->mi[0]->mbmi;
  TX_TYPE txk_start = DCT_DCT;
  TX_TYPE txk_end = TX_TYPES - 1;
  TX_TYPE best_tx_type = txk_start;
  int64_t best_rd = INT64_MAX;
  uint8_t best_eob = 0;
  const int coeff_ctx = combine_entropy_contexts(*a, *l);
  RD_STATS best_rd_stats;
  TX_TYPE tx_type;

  av1_invalid_rd_stats(&best_rd_stats);

  for (tx_type = txk_start; tx_type <= txk_end; ++tx_type) {
    if (plane == 0) mbmi->txk_type[(blk_row << 4) + blk_col] = tx_type;
    TX_TYPE ref_tx_type = av1_get_tx_type(get_plane_type(plane), xd, blk_row,
                                          blk_col, block, tx_size);
    if (tx_type != ref_tx_type) {
      // use av1_get_tx_type() to check if the tx_type is valid for the current
      // mode if it's not, we skip it here.
      continue;
    }

#if CONFIG_EXT_TX
    int is_inter = is_inter_block(mbmi);
    int ext_tx_set = get_ext_tx_set(get_min_tx_size(tx_size), mbmi->sb_type,
                                    is_inter, cm->reduced_tx_set_used);
    if (!(is_inter && ext_tx_used_inter[ext_tx_set][tx_type]) &&
        !(!is_inter && ext_tx_used_intra[ext_tx_set][tx_type]))
      continue;
#endif  // CONFIG_EXT_TX

    RD_STATS this_rd_stats;
    av1_invalid_rd_stats(&this_rd_stats);
    av1_xform_quant(cm, x, plane, block, blk_row, blk_col, plane_bsize, tx_size,
                    coeff_ctx, AV1_XFORM_QUANT_FP);
    av1_optimize_b(cm, x, plane, blk_row, blk_col, block, plane_bsize, tx_size,
                   a, l);
    av1_dist_block(cpi, x, plane, plane_bsize, block, blk_row, blk_col, tx_size,
                   &this_rd_stats.dist, &this_rd_stats.sse,
                   OUTPUT_HAS_PREDICTED_PIXELS);
    const SCAN_ORDER *scan_order = get_scan(cm, tx_size, tx_type, mbmi);
    this_rd_stats.rate =
        av1_cost_coeffs(cpi, x, plane, blk_row, blk_col, block, tx_size,
                        scan_order, a, l, use_fast_coef_costing);
    int rd = RDCOST(x->rdmult, this_rd_stats.rate, this_rd_stats.dist);

    if (rd < best_rd) {
      best_rd = rd;
      best_rd_stats = this_rd_stats;
      best_tx_type = tx_type;
      best_eob = x->plane[plane].txb_entropy_ctx[block];
    }
  }

  av1_merge_rd_stats(rd_stats, &best_rd_stats);

  //  if (x->plane[plane].eobs[block] == 0)
  //    if (best_tx_type != DCT_DCT)
  //      exit(0);

  if (best_eob == 0 && is_inter_block(mbmi)) best_tx_type = DCT_DCT;

  if (plane == 0) mbmi->txk_type[(blk_row << 4) + blk_col] = best_tx_type;
  x->plane[plane].txb_entropy_ctx[block] = best_eob;

  if (!is_inter_block(mbmi)) {
    // intra mode needs decoded result such that the next transform block
    // can use it for prediction.
    av1_xform_quant(cm, x, plane, block, blk_row, blk_col, plane_bsize, tx_size,
                    coeff_ctx, AV1_XFORM_QUANT_FP);
    av1_optimize_b(cm, x, plane, blk_row, blk_col, block, plane_bsize, tx_size,
                   a, l);

    av1_inverse_transform_block_facade(xd, plane, block, blk_row, blk_col,
                                       x->plane[plane].eobs[block]);
  }
  return best_rd;
}
#endif  // CONFIG_TXK_SEL
