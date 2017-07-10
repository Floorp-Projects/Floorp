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
#include "av1/common/idct.h"
#include "av1/common/txb_common.h"
#include "av1/decoder/decodemv.h"
#include "av1/decoder/decodetxb.h"
#include "av1/decoder/dsubexp.h"

#define ACCT_STR __func__

static int read_golomb(MACROBLOCKD *xd, aom_reader *r) {
  int x = 1;
  int length = 0;
  int i = 0;

  while (!i) {
    i = aom_read_bit(r, ACCT_STR);
    ++length;
    if (length >= 32) {
      aom_internal_error(xd->error_info, AOM_CODEC_CORRUPT_FRAME,
                         "Invalid length in read_golomb");
      break;
    }
  }

  for (i = 0; i < length - 1; ++i) {
    x <<= 1;
    x += aom_read_bit(r, ACCT_STR);
  }

  return x - 1;
}

uint8_t av1_read_coeffs_txb(const AV1_COMMON *const cm, MACROBLOCKD *xd,
                            aom_reader *r, int blk_row, int blk_col, int block,
                            int plane, tran_low_t *tcoeffs, TXB_CTX *txb_ctx,
                            TX_SIZE tx_size, int16_t *max_scan_line, int *eob) {
  FRAME_COUNTS *counts = xd->counts;
  TX_SIZE txs_ctx = get_txsize_context(tx_size);
  PLANE_TYPE plane_type = get_plane_type(plane);
  aom_prob *nz_map = cm->fc->nz_map[txs_ctx][plane_type];
  aom_prob *eob_flag = cm->fc->eob_flag[txs_ctx][plane_type];
  MB_MODE_INFO *mbmi = &xd->mi[0]->mbmi;
  const int seg_eob = tx_size_2d[tx_size];
  int c = 0;
  int update_eob = -1;
  const int16_t *const dequant = xd->plane[plane].seg_dequant[mbmi->segment_id];
  const int shift = av1_get_tx_scale(tx_size);
  const int bwl = b_width_log2_lookup[txsize_to_bsize[tx_size]] + 2;
  const int height = tx_size_high[tx_size];
  int cul_level = 0;
  unsigned int(*nz_map_count)[SIG_COEF_CONTEXTS][2];

  nz_map_count = (counts) ? &counts->nz_map[txs_ctx][plane_type] : NULL;

  memset(tcoeffs, 0, sizeof(*tcoeffs) * seg_eob);

  int all_zero =
      aom_read(r, cm->fc->txb_skip[txs_ctx][txb_ctx->txb_skip_ctx], ACCT_STR);
  if (xd->counts)
    ++xd->counts->txb_skip[txs_ctx][txb_ctx->txb_skip_ctx][all_zero];

  *eob = 0;
  if (all_zero) {
    *max_scan_line = 0;
#if CONFIG_TXK_SEL
    if (plane == 0) mbmi->txk_type[(blk_row << 4) + blk_col] = DCT_DCT;
#endif
    return 0;
  }

  (void)blk_row;
  (void)blk_col;
#if CONFIG_TXK_SEL
  av1_read_tx_type(cm, xd, blk_row, blk_col, block, plane,
                   get_min_tx_size(tx_size), r);
#endif
  const TX_TYPE tx_type =
      av1_get_tx_type(plane_type, xd, blk_row, blk_col, block, tx_size);
  const SCAN_ORDER *const scan_order = get_scan(cm, tx_size, tx_type, mbmi);
  const int16_t *scan = scan_order->scan;
  const int16_t *iscan = scan_order->iscan;

  for (c = 0; c < seg_eob; ++c) {
    int is_nz;
    int coeff_ctx = get_nz_map_ctx(tcoeffs, scan[c], bwl, height, iscan);
    int eob_ctx = get_eob_ctx(tcoeffs, scan[c], txs_ctx);

    if (c < seg_eob - 1)
      is_nz = aom_read(r, nz_map[coeff_ctx], ACCT_STR);
    else
      is_nz = 1;

    // set non-zero coefficient map.
    tcoeffs[scan[c]] = is_nz;

    if (c == seg_eob - 1) {
      ++c;
      break;
    }

    if (counts) ++(*nz_map_count)[coeff_ctx][is_nz];

    if (is_nz) {
      int is_eob = aom_read(r, eob_flag[eob_ctx], ACCT_STR);
      if (counts) ++counts->eob_flag[txs_ctx][plane_type][eob_ctx][is_eob];
      if (is_eob) break;
    }
  }

  *eob = AOMMIN(seg_eob, c + 1);
  *max_scan_line = *eob;

  int i;
  for (i = 0; i < NUM_BASE_LEVELS; ++i) {
    aom_prob *coeff_base = cm->fc->coeff_base[txs_ctx][plane_type][i];

    update_eob = 0;
    for (c = *eob - 1; c >= 0; --c) {
      tran_low_t *v = &tcoeffs[scan[c]];
      int sign;
      int ctx;

      if (*v <= i) continue;

      ctx = get_base_ctx(tcoeffs, scan[c], bwl, height, i + 1);

      if (aom_read(r, coeff_base[ctx], ACCT_STR)) {
        *v = i + 1;
        cul_level += i + 1;

        if (counts) ++counts->coeff_base[txs_ctx][plane_type][i][ctx][1];

        if (c == 0) {
          int dc_sign_ctx = txb_ctx->dc_sign_ctx;
          sign =
              aom_read(r, cm->fc->dc_sign[plane_type][dc_sign_ctx], ACCT_STR);
          if (counts) ++counts->dc_sign[plane_type][dc_sign_ctx][sign];
        } else {
          sign = aom_read_bit(r, ACCT_STR);
        }
        if (sign) *v = -(*v);
        continue;
      }
      *v = i + 2;
      if (counts) ++counts->coeff_base[txs_ctx][plane_type][i][ctx][0];

      // update the eob flag for coefficients with magnitude above 1.
      update_eob = AOMMAX(update_eob, c);
    }
  }

  for (c = update_eob; c >= 0; --c) {
    tran_low_t *v = &tcoeffs[scan[c]];
    int sign;
    int idx;
    int ctx;

    if (*v <= NUM_BASE_LEVELS) continue;

    if (c == 0) {
      int dc_sign_ctx = txb_ctx->dc_sign_ctx;
      sign = aom_read(r, cm->fc->dc_sign[plane_type][dc_sign_ctx], ACCT_STR);
      if (counts) ++counts->dc_sign[plane_type][dc_sign_ctx][sign];
    } else {
      sign = aom_read_bit(r, ACCT_STR);
    }

    ctx = get_br_ctx(tcoeffs, scan[c], bwl, height);

    if (cm->fc->coeff_lps[txs_ctx][plane_type][ctx] == 0) exit(0);

    for (idx = 0; idx < COEFF_BASE_RANGE; ++idx) {
      if (aom_read(r, cm->fc->coeff_lps[txs_ctx][plane_type][ctx], ACCT_STR)) {
        *v = (idx + 1 + NUM_BASE_LEVELS);
        if (sign) *v = -(*v);
        cul_level += abs(*v);

        if (counts) ++counts->coeff_lps[txs_ctx][plane_type][ctx][1];
        break;
      }
      if (counts) ++counts->coeff_lps[txs_ctx][plane_type][ctx][0];
    }
    if (idx < COEFF_BASE_RANGE) continue;

    // decode 0-th order Golomb code
    *v = read_golomb(xd, r) + COEFF_BASE_RANGE + 1 + NUM_BASE_LEVELS;
    if (sign) *v = -(*v);
    cul_level += abs(*v);
  }

  for (c = 0; c < *eob; ++c) {
    int16_t dqv = (c == 0) ? dequant[0] : dequant[1];
    tran_low_t *v = &tcoeffs[scan[c]];
    int sign = (*v) < 0;
    *v = (abs(*v) * dqv) >> shift;
    if (sign) *v = -(*v);
  }

  cul_level = AOMMIN(63, cul_level);

  // DC value
  set_dc_sign(&cul_level, tcoeffs[0]);

  return cul_level;
}

uint8_t av1_read_coeffs_txb_facade(AV1_COMMON *cm, MACROBLOCKD *xd,
                                   aom_reader *r, int row, int col, int block,
                                   int plane, tran_low_t *tcoeffs,
                                   TX_SIZE tx_size, int16_t *max_scan_line,
                                   int *eob) {
  MB_MODE_INFO *mbmi = &xd->mi[0]->mbmi;
  struct macroblockd_plane *pd = &xd->plane[plane];

  const BLOCK_SIZE bsize = mbmi->sb_type;
#if CONFIG_CHROMA_SUB8X8
  const BLOCK_SIZE plane_bsize =
      AOMMAX(BLOCK_4X4, get_plane_block_size(bsize, pd));
#elif CONFIG_CB4X4
  const BLOCK_SIZE plane_bsize = get_plane_block_size(bsize, pd);
#else   // CONFIG_CB4X4
  const BLOCK_SIZE plane_bsize =
      get_plane_block_size(AOMMAX(BLOCK_8X8, bsize), pd);
#endif  // CONFIG_CB4X4

  TXB_CTX txb_ctx;
  get_txb_ctx(plane_bsize, tx_size, plane, pd->above_context + col,
              pd->left_context + row, &txb_ctx);
  uint8_t cul_level =
      av1_read_coeffs_txb(cm, xd, r, row, col, block, plane, tcoeffs, &txb_ctx,
                          tx_size, max_scan_line, eob);
#if CONFIG_ADAPT_SCAN
  PLANE_TYPE plane_type = get_plane_type(plane);
  TX_TYPE tx_type = av1_get_tx_type(plane_type, xd, row, col, block, tx_size);
  if (xd->counts && *eob > 0)
    av1_update_scan_count_facade(cm, xd->counts, tx_size, tx_type, pd->dqcoeff,
                                 *eob);
#endif
  av1_set_contexts(xd, pd, plane, tx_size, cul_level, col, row);
  return cul_level;
}

static void read_txb_probs(FRAME_CONTEXT *fc, const TX_SIZE tx_size,
                           aom_reader *r) {
  int plane, ctx, level;

  if (aom_read_bit(r, ACCT_STR) == 0) return;

  for (ctx = 0; ctx < TXB_SKIP_CONTEXTS; ++ctx)
    av1_diff_update_prob(r, &fc->txb_skip[tx_size][ctx], ACCT_STR);

  for (plane = 0; plane < PLANE_TYPES; ++plane)
    for (ctx = 0; ctx < SIG_COEF_CONTEXTS; ++ctx)
      av1_diff_update_prob(r, &fc->nz_map[tx_size][plane][ctx], ACCT_STR);

  for (plane = 0; plane < PLANE_TYPES; ++plane)
    for (ctx = 0; ctx < EOB_COEF_CONTEXTS; ++ctx)
      av1_diff_update_prob(r, &fc->eob_flag[tx_size][plane][ctx], ACCT_STR);

  for (level = 0; level < NUM_BASE_LEVELS; ++level)
    for (plane = 0; plane < PLANE_TYPES; ++plane)
      for (ctx = 0; ctx < COEFF_BASE_CONTEXTS; ++ctx)
        av1_diff_update_prob(r, &fc->coeff_base[tx_size][plane][level][ctx],
                             ACCT_STR);

  for (plane = 0; plane < PLANE_TYPES; ++plane)
    for (ctx = 0; ctx < LEVEL_CONTEXTS; ++ctx)
      av1_diff_update_prob(r, &fc->coeff_lps[tx_size][plane][ctx], ACCT_STR);
}

void av1_read_txb_probs(FRAME_CONTEXT *fc, TX_MODE tx_mode, aom_reader *r) {
  const TX_SIZE max_tx_size = tx_mode_to_biggest_tx_size[tx_mode];
  TX_SIZE tx_size;
  int ctx, plane;
  for (plane = 0; plane < PLANE_TYPES; ++plane)
    for (ctx = 0; ctx < DC_SIGN_CONTEXTS; ++ctx)
      av1_diff_update_prob(r, &fc->dc_sign[plane][ctx], ACCT_STR);

  for (tx_size = TX_4X4; tx_size <= max_tx_size; ++tx_size)
    read_txb_probs(fc, tx_size, r);
}
