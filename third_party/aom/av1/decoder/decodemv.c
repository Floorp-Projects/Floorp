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

#include "av1/common/common.h"
#include "av1/common/entropy.h"
#include "av1/common/entropymode.h"
#include "av1/common/entropymv.h"
#include "av1/common/mvref_common.h"
#include "av1/common/pred_common.h"
#include "av1/common/reconinter.h"
#if CONFIG_EXT_INTRA
#include "av1/common/reconintra.h"
#endif  // CONFIG_EXT_INTRA
#include "av1/common/seg_common.h"
#if CONFIG_WARPED_MOTION
#include "av1/common/warped_motion.h"
#endif  // CONFIG_WARPED_MOTION

#include "av1/decoder/decodeframe.h"
#include "av1/decoder/decodemv.h"

#include "aom_dsp/aom_dsp_common.h"

#define ACCT_STR __func__

#define DEC_MISMATCH_DEBUG 0

static PREDICTION_MODE read_intra_mode(aom_reader *r, aom_cdf_prob *cdf) {
  return (PREDICTION_MODE)aom_read_symbol(r, cdf, INTRA_MODES, ACCT_STR);
}

static int read_delta_qindex(AV1_COMMON *cm, MACROBLOCKD *xd, aom_reader *r,
                             MB_MODE_INFO *const mbmi, int mi_col, int mi_row) {
  FRAME_COUNTS *counts = xd->counts;
  int sign, abs, reduced_delta_qindex = 0;
  BLOCK_SIZE bsize = mbmi->sb_type;
  const int b_col = mi_col & MAX_MIB_MASK;
  const int b_row = mi_row & MAX_MIB_MASK;
  const int read_delta_q_flag = (b_col == 0 && b_row == 0);
  int rem_bits, thr;
  int i, smallval;
  FRAME_CONTEXT *ec_ctx = xd->tile_ctx;
  (void)cm;

  if ((bsize != BLOCK_LARGEST || mbmi->skip == 0) && read_delta_q_flag) {
    abs = aom_read_symbol(r, ec_ctx->delta_q_cdf, DELTA_Q_PROBS + 1, ACCT_STR);
    smallval = (abs < DELTA_Q_SMALL);
    if (counts) {
      for (i = 0; i < abs; ++i) counts->delta_q[i][1]++;
      if (smallval) counts->delta_q[abs][0]++;
    }

    if (!smallval) {
      rem_bits = aom_read_literal(r, 3, ACCT_STR) + 1;
      thr = (1 << rem_bits) + 1;
      abs = aom_read_literal(r, rem_bits, ACCT_STR) + thr;
    }

    if (abs) {
      sign = aom_read_bit(r, ACCT_STR);
    } else {
      sign = 1;
    }

    reduced_delta_qindex = sign ? -abs : abs;
  }
  return reduced_delta_qindex;
}
#if CONFIG_EXT_DELTA_Q
static int read_delta_lflevel(AV1_COMMON *cm, MACROBLOCKD *xd, aom_reader *r,
#if CONFIG_LOOPFILTER_LEVEL
                              int lf_id,
#endif
                              MB_MODE_INFO *const mbmi, int mi_col,
                              int mi_row) {
  FRAME_COUNTS *counts = xd->counts;
  int sign, abs, reduced_delta_lflevel = 0;
  BLOCK_SIZE bsize = mbmi->sb_type;
  const int b_col = mi_col & MAX_MIB_MASK;
  const int b_row = mi_row & MAX_MIB_MASK;
  const int read_delta_lf_flag = (b_col == 0 && b_row == 0);
  int rem_bits, thr;
  int i, smallval;
  FRAME_CONTEXT *ec_ctx = xd->tile_ctx;
  (void)cm;

  if ((bsize != cm->sb_size || mbmi->skip == 0) && read_delta_lf_flag) {
#if CONFIG_LOOPFILTER_LEVEL
    if (cm->delta_lf_multi) {
      assert(lf_id >= 0 && lf_id < FRAME_LF_COUNT);
      abs = aom_read_symbol(r, ec_ctx->delta_lf_multi_cdf[lf_id],
                            DELTA_LF_PROBS + 1, ACCT_STR);
    } else {
      abs = aom_read_symbol(r, ec_ctx->delta_lf_cdf, DELTA_LF_PROBS + 1,
                            ACCT_STR);
    }
#else
    abs =
        aom_read_symbol(r, ec_ctx->delta_lf_cdf, DELTA_LF_PROBS + 1, ACCT_STR);
#endif  // CONFIG_LOOPFILTER_LEVEL
    smallval = (abs < DELTA_LF_SMALL);
    if (counts) {
#if CONFIG_LOOPFILTER_LEVEL
      if (cm->delta_lf_multi) {
        for (i = 0; i < abs; ++i) counts->delta_lf_multi[lf_id][i][1]++;
        if (smallval) counts->delta_lf_multi[lf_id][abs][0]++;
      } else {
        for (i = 0; i < abs; ++i) counts->delta_lf[i][1]++;
        if (smallval) counts->delta_lf[abs][0]++;
      }
#else
      for (i = 0; i < abs; ++i) counts->delta_lf[i][1]++;
      if (smallval) counts->delta_lf[abs][0]++;
#endif  // CONFIG_LOOPFILTER_LEVEL
    }
    if (!smallval) {
      rem_bits = aom_read_literal(r, 3, ACCT_STR) + 1;
      thr = (1 << rem_bits) + 1;
      abs = aom_read_literal(r, rem_bits, ACCT_STR) + thr;
    }

    if (abs) {
      sign = aom_read_bit(r, ACCT_STR);
    } else {
      sign = 1;
    }

    reduced_delta_lflevel = sign ? -abs : abs;
  }
  return reduced_delta_lflevel;
}
#endif

static UV_PREDICTION_MODE read_intra_mode_uv(FRAME_CONTEXT *ec_ctx,
                                             aom_reader *r,
                                             PREDICTION_MODE y_mode) {
  const UV_PREDICTION_MODE uv_mode =
#if CONFIG_CFL
      aom_read_symbol(r, ec_ctx->uv_mode_cdf[y_mode], UV_INTRA_MODES, ACCT_STR);
#else
      read_intra_mode(r, ec_ctx->uv_mode_cdf[y_mode]);
#endif  // CONFIG_CFL
  return uv_mode;
}

#if CONFIG_CFL
static int read_cfl_alphas(FRAME_CONTEXT *const ec_ctx, aom_reader *r,
                           int *signs_out) {
  const int joint_sign =
      aom_read_symbol(r, ec_ctx->cfl_sign_cdf, CFL_JOINT_SIGNS, "cfl:signs");
  int idx = 0;
  // Magnitudes are only coded for nonzero values
  if (CFL_SIGN_U(joint_sign) != CFL_SIGN_ZERO) {
    aom_cdf_prob *cdf_u = ec_ctx->cfl_alpha_cdf[CFL_CONTEXT_U(joint_sign)];
    idx = aom_read_symbol(r, cdf_u, CFL_ALPHABET_SIZE, "cfl:alpha_u")
          << CFL_ALPHABET_SIZE_LOG2;
  }
  if (CFL_SIGN_V(joint_sign) != CFL_SIGN_ZERO) {
    aom_cdf_prob *cdf_v = ec_ctx->cfl_alpha_cdf[CFL_CONTEXT_V(joint_sign)];
    idx += aom_read_symbol(r, cdf_v, CFL_ALPHABET_SIZE, "cfl:alpha_v");
  }
  *signs_out = joint_sign;
  return idx;
}
#endif

#if CONFIG_INTERINTRA
static INTERINTRA_MODE read_interintra_mode(AV1_COMMON *cm, MACROBLOCKD *xd,
                                            aom_reader *r, int size_group) {
  (void)cm;
  const INTERINTRA_MODE ii_mode = (INTERINTRA_MODE)aom_read_symbol(
      r, xd->tile_ctx->interintra_mode_cdf[size_group], INTERINTRA_MODES,
      ACCT_STR);
  FRAME_COUNTS *counts = xd->counts;
  if (counts) ++counts->interintra_mode[size_group][ii_mode];
  return ii_mode;
}
#endif  // CONFIG_INTERINTRA

static PREDICTION_MODE read_inter_mode(FRAME_CONTEXT *ec_ctx, MACROBLOCKD *xd,
                                       aom_reader *r, int16_t ctx) {
  FRAME_COUNTS *counts = xd->counts;
  int16_t mode_ctx = ctx & NEWMV_CTX_MASK;
  int is_newmv, is_zeromv, is_refmv;
#if CONFIG_NEW_MULTISYMBOL
  is_newmv = aom_read_symbol(r, ec_ctx->newmv_cdf[mode_ctx], 2, ACCT_STR) == 0;
#else
  is_newmv = aom_read(r, ec_ctx->newmv_prob[mode_ctx], ACCT_STR) == 0;
#endif

  if (is_newmv) {
    if (counts) ++counts->newmv_mode[mode_ctx][0];
    return NEWMV;
  }
  if (counts) ++counts->newmv_mode[mode_ctx][1];

  if (ctx & (1 << ALL_ZERO_FLAG_OFFSET)) return ZEROMV;

  mode_ctx = (ctx >> ZEROMV_OFFSET) & ZEROMV_CTX_MASK;

#if CONFIG_NEW_MULTISYMBOL
  is_zeromv =
      aom_read_symbol(r, ec_ctx->zeromv_cdf[mode_ctx], 2, ACCT_STR) == 0;
#else
  is_zeromv = aom_read(r, ec_ctx->zeromv_prob[mode_ctx], ACCT_STR) == 0;
#endif
  if (is_zeromv) {
    if (counts) ++counts->zeromv_mode[mode_ctx][0];
    return ZEROMV;
  }
  if (counts) ++counts->zeromv_mode[mode_ctx][1];

  mode_ctx = (ctx >> REFMV_OFFSET) & REFMV_CTX_MASK;

  if (ctx & (1 << SKIP_NEARESTMV_OFFSET)) mode_ctx = 6;
  if (ctx & (1 << SKIP_NEARMV_OFFSET)) mode_ctx = 7;
  if (ctx & (1 << SKIP_NEARESTMV_SUB8X8_OFFSET)) mode_ctx = 8;

#if CONFIG_NEW_MULTISYMBOL
  is_refmv = aom_read_symbol(r, ec_ctx->refmv_cdf[mode_ctx], 2, ACCT_STR) == 0;
#else
  is_refmv = aom_read(r, ec_ctx->refmv_prob[mode_ctx], ACCT_STR) == 0;
#endif

  if (is_refmv) {
    if (counts) ++counts->refmv_mode[mode_ctx][0];

    return NEARESTMV;
  } else {
    if (counts) ++counts->refmv_mode[mode_ctx][1];
    return NEARMV;
  }

  // Invalid prediction mode.
  assert(0);
}

static void read_drl_idx(FRAME_CONTEXT *ec_ctx, MACROBLOCKD *xd,
                         MB_MODE_INFO *mbmi, aom_reader *r) {
  uint8_t ref_frame_type = av1_ref_frame_type(mbmi->ref_frame);
  mbmi->ref_mv_idx = 0;

  if (mbmi->mode == NEWMV || mbmi->mode == NEW_NEWMV
#if CONFIG_COMPOUND_SINGLEREF
      || mbmi->mode == SR_NEW_NEWMV
#endif  // CONFIG_COMPOUND_SINGLEREF
      ) {
    int idx;
    for (idx = 0; idx < 2; ++idx) {
      if (xd->ref_mv_count[ref_frame_type] > idx + 1) {
        uint8_t drl_ctx = av1_drl_ctx(xd->ref_mv_stack[ref_frame_type], idx);
#if CONFIG_NEW_MULTISYMBOL
        int drl_idx = aom_read_symbol(r, ec_ctx->drl_cdf[drl_ctx], 2, ACCT_STR);
#else
        int drl_idx = aom_read(r, ec_ctx->drl_prob[drl_ctx], ACCT_STR);
#endif
        mbmi->ref_mv_idx = idx + drl_idx;
        if (xd->counts) ++xd->counts->drl_mode[drl_ctx][drl_idx];
        if (!drl_idx) return;
      }
    }
  }

  if (have_nearmv_in_inter_mode(mbmi->mode)) {
    int idx;
    // Offset the NEARESTMV mode.
    // TODO(jingning): Unify the two syntax decoding loops after the NEARESTMV
    // mode is factored in.
    for (idx = 1; idx < 3; ++idx) {
      if (xd->ref_mv_count[ref_frame_type] > idx + 1) {
        uint8_t drl_ctx = av1_drl_ctx(xd->ref_mv_stack[ref_frame_type], idx);
#if CONFIG_NEW_MULTISYMBOL
        int drl_idx = aom_read_symbol(r, ec_ctx->drl_cdf[drl_ctx], 2, ACCT_STR);
#else
        int drl_idx = aom_read(r, ec_ctx->drl_prob[drl_ctx], ACCT_STR);
#endif
        mbmi->ref_mv_idx = idx + drl_idx - 1;
        if (xd->counts) ++xd->counts->drl_mode[drl_ctx][drl_idx];
        if (!drl_idx) return;
      }
    }
  }
}

#if CONFIG_MOTION_VAR || CONFIG_WARPED_MOTION
static MOTION_MODE read_motion_mode(AV1_COMMON *cm, MACROBLOCKD *xd,
                                    MODE_INFO *mi, aom_reader *r) {
  MB_MODE_INFO *mbmi = &mi->mbmi;
#if !CONFIG_MOTION_VAR || !CONFIG_WARPED_MOTION || CONFIG_NEW_MULTISYMBOL || \
    CONFIG_NCOBMC_ADAPT_WEIGHT
  (void)cm;
#endif

  const MOTION_MODE last_motion_mode_allowed = motion_mode_allowed(
#if CONFIG_GLOBAL_MOTION
      0, xd->global_motion,
#endif  // CONFIG_GLOBAL_MOTION
#if CONFIG_WARPED_MOTION
      xd,
#endif
      mi);
  int motion_mode;
  FRAME_COUNTS *counts = xd->counts;

  if (last_motion_mode_allowed == SIMPLE_TRANSLATION) return SIMPLE_TRANSLATION;
#if CONFIG_MOTION_VAR && CONFIG_WARPED_MOTION
#if CONFIG_NCOBMC_ADAPT_WEIGHT
  if (last_motion_mode_allowed == NCOBMC_ADAPT_WEIGHT) {
    motion_mode = aom_read_symbol(r, xd->tile_ctx->ncobmc_cdf[mbmi->sb_type],
                                  OBMC_FAMILY_MODES, ACCT_STR);
    if (counts) ++counts->ncobmc[mbmi->sb_type][motion_mode];
    return (MOTION_MODE)(SIMPLE_TRANSLATION + motion_mode);
  } else if (last_motion_mode_allowed == OBMC_CAUSAL) {
    motion_mode =
        aom_read_symbol(r, xd->tile_ctx->obmc_cdf[mbmi->sb_type], 2, ACCT_STR);
    if (counts) ++counts->obmc[mbmi->sb_type][motion_mode];
    return (MOTION_MODE)(SIMPLE_TRANSLATION + motion_mode);
  } else {
#else
  if (last_motion_mode_allowed == OBMC_CAUSAL) {
#if CONFIG_NEW_MULTISYMBOL
    motion_mode =
        aom_read_symbol(r, xd->tile_ctx->obmc_cdf[mbmi->sb_type], 2, ACCT_STR);
#else
    motion_mode = aom_read(r, cm->fc->obmc_prob[mbmi->sb_type], ACCT_STR);
#endif
    if (counts) ++counts->obmc[mbmi->sb_type][motion_mode];
    return (MOTION_MODE)(SIMPLE_TRANSLATION + motion_mode);
  } else {
#endif  // CONFIG_NCOBMC_ADAPT_WEIGHT
#endif  // CONFIG_MOTION_VAR && CONFIG_WARPED_MOTION
    motion_mode =
        aom_read_symbol(r, xd->tile_ctx->motion_mode_cdf[mbmi->sb_type],
                        MOTION_MODES, ACCT_STR);
    if (counts) ++counts->motion_mode[mbmi->sb_type][motion_mode];
    return (MOTION_MODE)(SIMPLE_TRANSLATION + motion_mode);
#if CONFIG_MOTION_VAR && CONFIG_WARPED_MOTION
  }
#endif  // CONFIG_MOTION_VAR && CONFIG_WARPED_MOTION
}

#if CONFIG_NCOBMC_ADAPT_WEIGHT
static void read_ncobmc_mode(MACROBLOCKD *xd, MODE_INFO *mi,
                             NCOBMC_MODE ncobmc_mode[2], aom_reader *r) {
  MB_MODE_INFO *mbmi = &mi->mbmi;
  FRAME_COUNTS *counts = xd->counts;
  ADAPT_OVERLAP_BLOCK ao_block = adapt_overlap_block_lookup[mbmi->sb_type];
  if (mbmi->motion_mode != NCOBMC_ADAPT_WEIGHT) return;

  ncobmc_mode[0] = aom_read_symbol(r, xd->tile_ctx->ncobmc_mode_cdf[ao_block],
                                   MAX_NCOBMC_MODES, ACCT_STR);
  if (counts) ++counts->ncobmc_mode[ao_block][ncobmc_mode[0]];

  if (mi_size_wide[mbmi->sb_type] != mi_size_high[mbmi->sb_type]) {
    ncobmc_mode[1] = aom_read_symbol(r, xd->tile_ctx->ncobmc_mode_cdf[ao_block],
                                     MAX_NCOBMC_MODES, ACCT_STR);
    if (counts) ++counts->ncobmc_mode[ao_block][ncobmc_mode[1]];
  }
}
#endif  // CONFIG_NCOBMC_ADAPT_WEIGHT
#endif  // CONFIG_MOTION_VAR || CONFIG_WARPED_MOTION

static PREDICTION_MODE read_inter_compound_mode(AV1_COMMON *cm, MACROBLOCKD *xd,
                                                aom_reader *r, int16_t ctx) {
  (void)cm;
  const int mode =
      aom_read_symbol(r, xd->tile_ctx->inter_compound_mode_cdf[ctx],
                      INTER_COMPOUND_MODES, ACCT_STR);
  FRAME_COUNTS *counts = xd->counts;

  if (counts) ++counts->inter_compound_mode[ctx][mode];

  assert(is_inter_compound_mode(NEAREST_NEARESTMV + mode));
  return NEAREST_NEARESTMV + mode;
}

#if CONFIG_COMPOUND_SINGLEREF
static PREDICTION_MODE read_inter_singleref_comp_mode(MACROBLOCKD *xd,
                                                      aom_reader *r,
                                                      int16_t ctx) {
  const int mode =
      aom_read_symbol(r, xd->tile_ctx->inter_singleref_comp_mode_cdf[ctx],
                      INTER_SINGLEREF_COMP_MODES, ACCT_STR);
  FRAME_COUNTS *counts = xd->counts;

  if (counts) ++counts->inter_singleref_comp_mode[ctx][mode];

  assert(is_inter_singleref_comp_mode(SR_NEAREST_NEARMV + mode));
  return SR_NEAREST_NEARMV + mode;
}
#endif  // CONFIG_COMPOUND_SINGLEREF

static int read_segment_id(aom_reader *r, struct segmentation_probs *segp) {
  return aom_read_symbol(r, segp->tree_cdf, MAX_SEGMENTS, ACCT_STR);
}

#if CONFIG_VAR_TX
static void read_tx_size_vartx(AV1_COMMON *cm, MACROBLOCKD *xd,
                               MB_MODE_INFO *mbmi, FRAME_COUNTS *counts,
                               TX_SIZE tx_size, int depth, int blk_row,
                               int blk_col, aom_reader *r) {
#if CONFIG_NEW_MULTISYMBOL
  FRAME_CONTEXT *ec_ctx = xd->tile_ctx;
  (void)cm;
#endif
  int is_split = 0;
  const int tx_row = blk_row >> 1;
  const int tx_col = blk_col >> 1;
  const int max_blocks_high = max_block_high(xd, mbmi->sb_type, 0);
  const int max_blocks_wide = max_block_wide(xd, mbmi->sb_type, 0);
  int ctx = txfm_partition_context(xd->above_txfm_context + blk_col,
                                   xd->left_txfm_context + blk_row,
                                   mbmi->sb_type, tx_size);
  TX_SIZE(*const inter_tx_size)
  [MAX_MIB_SIZE] =
      (TX_SIZE(*)[MAX_MIB_SIZE]) & mbmi->inter_tx_size[tx_row][tx_col];
  if (blk_row >= max_blocks_high || blk_col >= max_blocks_wide) return;
  assert(tx_size > TX_4X4);

  if (depth == MAX_VARTX_DEPTH) {
    int idx, idy;
    inter_tx_size[0][0] = tx_size;
    for (idy = 0; idy < tx_size_high_unit[tx_size] / 2; ++idy)
      for (idx = 0; idx < tx_size_wide_unit[tx_size] / 2; ++idx)
        inter_tx_size[idy][idx] = tx_size;
    mbmi->tx_size = tx_size;
    mbmi->min_tx_size = AOMMIN(mbmi->min_tx_size, get_min_tx_size(tx_size));
    txfm_partition_update(xd->above_txfm_context + blk_col,
                          xd->left_txfm_context + blk_row, tx_size, tx_size);
    return;
  }

#if CONFIG_NEW_MULTISYMBOL
  is_split = aom_read_symbol(r, ec_ctx->txfm_partition_cdf[ctx], 2, ACCT_STR);
#else
  is_split = aom_read(r, cm->fc->txfm_partition_prob[ctx], ACCT_STR);
#endif

  if (is_split) {
    const TX_SIZE sub_txs = sub_tx_size_map[tx_size];
    const int bsl = tx_size_wide_unit[sub_txs];
    int i;

    if (counts) ++counts->txfm_partition[ctx][1];

    if (sub_txs == TX_4X4) {
      int idx, idy;
      inter_tx_size[0][0] = sub_txs;
      for (idy = 0; idy < tx_size_high_unit[tx_size] / 2; ++idy)
        for (idx = 0; idx < tx_size_wide_unit[tx_size] / 2; ++idx)
          inter_tx_size[idy][idx] = inter_tx_size[0][0];
      mbmi->tx_size = sub_txs;
      mbmi->min_tx_size = get_min_tx_size(mbmi->tx_size);
      txfm_partition_update(xd->above_txfm_context + blk_col,
                            xd->left_txfm_context + blk_row, sub_txs, tx_size);
      return;
    }

    assert(bsl > 0);
    for (i = 0; i < 4; ++i) {
      int offsetr = blk_row + (i >> 1) * bsl;
      int offsetc = blk_col + (i & 0x01) * bsl;
      read_tx_size_vartx(cm, xd, mbmi, counts, sub_txs, depth + 1, offsetr,
                         offsetc, r);
    }
  } else {
    int idx, idy;
    inter_tx_size[0][0] = tx_size;
    for (idy = 0; idy < tx_size_high_unit[tx_size] / 2; ++idy)
      for (idx = 0; idx < tx_size_wide_unit[tx_size] / 2; ++idx)
        inter_tx_size[idy][idx] = tx_size;
    mbmi->tx_size = tx_size;
    mbmi->min_tx_size = AOMMIN(mbmi->min_tx_size, get_min_tx_size(tx_size));
    if (counts) ++counts->txfm_partition[ctx][0];
    txfm_partition_update(xd->above_txfm_context + blk_col,
                          xd->left_txfm_context + blk_row, tx_size, tx_size);
  }
}
#endif

static TX_SIZE read_selected_tx_size(AV1_COMMON *cm, MACROBLOCKD *xd,
                                     int32_t tx_size_cat, aom_reader *r) {
  FRAME_COUNTS *counts = xd->counts;
  const int ctx = get_tx_size_context(xd);
  FRAME_CONTEXT *ec_ctx = xd->tile_ctx;
  (void)cm;

  const int depth = aom_read_symbol(r, ec_ctx->tx_size_cdf[tx_size_cat][ctx],
                                    tx_size_cat + 2, ACCT_STR);
  const TX_SIZE tx_size = depth_to_tx_size(depth);
#if CONFIG_RECT_TX
  assert(!is_rect_tx(tx_size));
#endif  // CONFIG_RECT_TX
  if (counts) ++counts->tx_size[tx_size_cat][ctx][depth];
  return tx_size;
}

static TX_SIZE read_tx_size(AV1_COMMON *cm, MACROBLOCKD *xd, int is_inter,
                            int allow_select_inter, aom_reader *r) {
  const TX_MODE tx_mode = cm->tx_mode;
  const BLOCK_SIZE bsize = xd->mi[0]->mbmi.sb_type;
  if (xd->lossless[xd->mi[0]->mbmi.segment_id]) return TX_4X4;

  if (block_signals_txsize(bsize)) {
    if ((!is_inter || allow_select_inter) && tx_mode == TX_MODE_SELECT) {
      const int32_t tx_size_cat = is_inter ? inter_tx_size_cat_lookup[bsize]
                                           : intra_tx_size_cat_lookup[bsize];
      const TX_SIZE coded_tx_size =
          read_selected_tx_size(cm, xd, tx_size_cat, r);
#if CONFIG_RECT_TX && (CONFIG_EXT_TX || CONFIG_VAR_TX)
      if (coded_tx_size > max_txsize_lookup[bsize]) {
        assert(coded_tx_size == max_txsize_lookup[bsize] + 1);
#if CONFIG_RECT_TX_EXT
        if (is_quarter_tx_allowed(xd, &xd->mi[0]->mbmi, is_inter)) {
          int quarter_tx;

          if (quarter_txsize_lookup[bsize] != max_txsize_lookup[bsize]) {
#if CONFIG_NEW_MULTISYMBOL
            quarter_tx =
                aom_read_symbol(r, cm->fc->quarter_tx_size_cdf, 2, ACCT_STR);
#else
            quarter_tx = aom_read(r, cm->fc->quarter_tx_size_prob, ACCT_STR);
            FRAME_COUNTS *counts = xd->counts;
            if (counts) ++counts->quarter_tx_size[quarter_tx];
#endif
          } else {
            quarter_tx = 1;
          }
          return quarter_tx ? quarter_txsize_lookup[bsize]
                            : max_txsize_rect_lookup[bsize];
        }
#endif  // CONFIG_RECT_TX_EXT

        return max_txsize_rect_lookup[bsize];
      }
#else
      assert(coded_tx_size <= max_txsize_lookup[bsize]);
#endif  // CONFIG_RECT_TX && (CONFIG_EXT_TX || CONFIG_VAR_TX)
      return coded_tx_size;
    } else {
      return tx_size_from_tx_mode(bsize, tx_mode, is_inter);
    }
  } else {
#if CONFIG_EXT_TX && CONFIG_RECT_TX
    assert(IMPLIES(tx_mode == ONLY_4X4, bsize == BLOCK_4X4));
    return max_txsize_rect_lookup[bsize];
#else
    return TX_4X4;
#endif  // CONFIG_EXT_TX && CONFIG_RECT_TX
  }
}

static int dec_get_segment_id(const AV1_COMMON *cm, const uint8_t *segment_ids,
                              int mi_offset, int x_mis, int y_mis) {
  int x, y, segment_id = INT_MAX;

  for (y = 0; y < y_mis; y++)
    for (x = 0; x < x_mis; x++)
      segment_id =
          AOMMIN(segment_id, segment_ids[mi_offset + y * cm->mi_cols + x]);

  assert(segment_id >= 0 && segment_id < MAX_SEGMENTS);
  return segment_id;
}

static void set_segment_id(AV1_COMMON *cm, int mi_offset, int x_mis, int y_mis,
                           int segment_id) {
  int x, y;

  assert(segment_id >= 0 && segment_id < MAX_SEGMENTS);

  for (y = 0; y < y_mis; y++)
    for (x = 0; x < x_mis; x++)
      cm->current_frame_seg_map[mi_offset + y * cm->mi_cols + x] = segment_id;
}

static int read_intra_segment_id(AV1_COMMON *const cm, MACROBLOCKD *const xd,
                                 int mi_offset, int x_mis, int y_mis,
                                 aom_reader *r) {
  struct segmentation *const seg = &cm->seg;
  FRAME_COUNTS *counts = xd->counts;
  FRAME_CONTEXT *ec_ctx = xd->tile_ctx;
  struct segmentation_probs *const segp = &ec_ctx->seg;
  int segment_id;

  if (!seg->enabled) return 0;  // Default for disabled segmentation

  assert(seg->update_map && !seg->temporal_update);

  segment_id = read_segment_id(r, segp);
  if (counts) ++counts->seg.tree_total[segment_id];
  set_segment_id(cm, mi_offset, x_mis, y_mis, segment_id);
  return segment_id;
}

static void copy_segment_id(const AV1_COMMON *cm,
                            const uint8_t *last_segment_ids,
                            uint8_t *current_segment_ids, int mi_offset,
                            int x_mis, int y_mis) {
  int x, y;

  for (y = 0; y < y_mis; y++)
    for (x = 0; x < x_mis; x++)
      current_segment_ids[mi_offset + y * cm->mi_cols + x] =
          last_segment_ids ? last_segment_ids[mi_offset + y * cm->mi_cols + x]
                           : 0;
}

static int read_inter_segment_id(AV1_COMMON *const cm, MACROBLOCKD *const xd,
                                 int mi_row, int mi_col, aom_reader *r) {
  struct segmentation *const seg = &cm->seg;
  FRAME_COUNTS *counts = xd->counts;
  FRAME_CONTEXT *ec_ctx = xd->tile_ctx;
  struct segmentation_probs *const segp = &ec_ctx->seg;

  MB_MODE_INFO *const mbmi = &xd->mi[0]->mbmi;
  int predicted_segment_id, segment_id;
  const int mi_offset = mi_row * cm->mi_cols + mi_col;
  const int bw = mi_size_wide[mbmi->sb_type];
  const int bh = mi_size_high[mbmi->sb_type];

  // TODO(slavarnway): move x_mis, y_mis into xd ?????
  const int x_mis = AOMMIN(cm->mi_cols - mi_col, bw);
  const int y_mis = AOMMIN(cm->mi_rows - mi_row, bh);

  if (!seg->enabled) return 0;  // Default for disabled segmentation

  predicted_segment_id = cm->last_frame_seg_map
                             ? dec_get_segment_id(cm, cm->last_frame_seg_map,
                                                  mi_offset, x_mis, y_mis)
                             : 0;

  if (!seg->update_map) {
    copy_segment_id(cm, cm->last_frame_seg_map, cm->current_frame_seg_map,
                    mi_offset, x_mis, y_mis);
    return predicted_segment_id;
  }

  if (seg->temporal_update) {
    const int ctx = av1_get_pred_context_seg_id(xd);
#if CONFIG_NEW_MULTISYMBOL
    aom_cdf_prob *pred_cdf = segp->pred_cdf[ctx];
    mbmi->seg_id_predicted = aom_read_symbol(r, pred_cdf, 2, ACCT_STR);
#else
    const aom_prob pred_prob = segp->pred_probs[ctx];
    mbmi->seg_id_predicted = aom_read(r, pred_prob, ACCT_STR);
#endif
    if (counts) ++counts->seg.pred[ctx][mbmi->seg_id_predicted];
    if (mbmi->seg_id_predicted) {
      segment_id = predicted_segment_id;
    } else {
      segment_id = read_segment_id(r, segp);
      if (counts) ++counts->seg.tree_mispred[segment_id];
    }
  } else {
    segment_id = read_segment_id(r, segp);
    if (counts) ++counts->seg.tree_total[segment_id];
  }
  set_segment_id(cm, mi_offset, x_mis, y_mis, segment_id);
  return segment_id;
}

static int read_skip(AV1_COMMON *cm, const MACROBLOCKD *xd, int segment_id,
                     aom_reader *r) {
  if (segfeature_active(&cm->seg, segment_id, SEG_LVL_SKIP)) {
    return 1;
  } else {
    const int ctx = av1_get_skip_context(xd);
#if CONFIG_NEW_MULTISYMBOL
    FRAME_CONTEXT *ec_ctx = xd->tile_ctx;
    const int skip = aom_read_symbol(r, ec_ctx->skip_cdfs[ctx], 2, ACCT_STR);
#else
    const int skip = aom_read(r, cm->fc->skip_probs[ctx], ACCT_STR);
#endif
    FRAME_COUNTS *counts = xd->counts;
    if (counts) ++counts->skip[ctx][skip];
    return skip;
  }
}

#if CONFIG_PALETTE_DELTA_ENCODING
// Merge the sorted list of cached colors(cached_colors[0...n_cached_colors-1])
// and the sorted list of transmitted colors(colors[n_cached_colors...n-1]) into
// one single sorted list(colors[...]).
static void merge_colors(uint16_t *colors, uint16_t *cached_colors,
                         int n_colors, int n_cached_colors) {
  if (n_cached_colors == 0) return;
  int cache_idx = 0, trans_idx = n_cached_colors;
  for (int i = 0; i < n_colors; ++i) {
    if (cache_idx < n_cached_colors &&
        (trans_idx >= n_colors ||
         cached_colors[cache_idx] <= colors[trans_idx])) {
      colors[i] = cached_colors[cache_idx++];
    } else {
      assert(trans_idx < n_colors);
      colors[i] = colors[trans_idx++];
    }
  }
}

static void read_palette_colors_y(MACROBLOCKD *const xd, int bit_depth,
                                  PALETTE_MODE_INFO *const pmi, aom_reader *r) {
  uint16_t color_cache[2 * PALETTE_MAX_SIZE];
  uint16_t cached_colors[PALETTE_MAX_SIZE];
  const int n_cache = av1_get_palette_cache(xd, 0, color_cache);
  const int n = pmi->palette_size[0];
  int idx = 0;
  for (int i = 0; i < n_cache && idx < n; ++i)
    if (aom_read_bit(r, ACCT_STR)) cached_colors[idx++] = color_cache[i];
  if (idx < n) {
    const int n_cached_colors = idx;
    pmi->palette_colors[idx++] = aom_read_literal(r, bit_depth, ACCT_STR);
    if (idx < n) {
      const int min_bits = bit_depth - 3;
      int bits = min_bits + aom_read_literal(r, 2, ACCT_STR);
      int range = (1 << bit_depth) - pmi->palette_colors[idx - 1] - 1;
      for (; idx < n; ++idx) {
        assert(range >= 0);
        const int delta = aom_read_literal(r, bits, ACCT_STR) + 1;
        pmi->palette_colors[idx] = clamp(pmi->palette_colors[idx - 1] + delta,
                                         0, (1 << bit_depth) - 1);
        range -= (pmi->palette_colors[idx] - pmi->palette_colors[idx - 1]);
        bits = AOMMIN(bits, av1_ceil_log2(range));
      }
    }
    merge_colors(pmi->palette_colors, cached_colors, n, n_cached_colors);
  } else {
    memcpy(pmi->palette_colors, cached_colors, n * sizeof(cached_colors[0]));
  }
}

static void read_palette_colors_uv(MACROBLOCKD *const xd, int bit_depth,
                                   PALETTE_MODE_INFO *const pmi,
                                   aom_reader *r) {
  const int n = pmi->palette_size[1];
  // U channel colors.
  uint16_t color_cache[2 * PALETTE_MAX_SIZE];
  uint16_t cached_colors[PALETTE_MAX_SIZE];
  const int n_cache = av1_get_palette_cache(xd, 1, color_cache);
  int idx = 0;
  for (int i = 0; i < n_cache && idx < n; ++i)
    if (aom_read_bit(r, ACCT_STR)) cached_colors[idx++] = color_cache[i];
  if (idx < n) {
    const int n_cached_colors = idx;
    idx += PALETTE_MAX_SIZE;
    pmi->palette_colors[idx++] = aom_read_literal(r, bit_depth, ACCT_STR);
    if (idx < PALETTE_MAX_SIZE + n) {
      const int min_bits = bit_depth - 3;
      int bits = min_bits + aom_read_literal(r, 2, ACCT_STR);
      int range = (1 << bit_depth) - pmi->palette_colors[idx - 1];
      for (; idx < PALETTE_MAX_SIZE + n; ++idx) {
        assert(range >= 0);
        const int delta = aom_read_literal(r, bits, ACCT_STR);
        pmi->palette_colors[idx] = clamp(pmi->palette_colors[idx - 1] + delta,
                                         0, (1 << bit_depth) - 1);
        range -= (pmi->palette_colors[idx] - pmi->palette_colors[idx - 1]);
        bits = AOMMIN(bits, av1_ceil_log2(range));
      }
    }
    merge_colors(pmi->palette_colors + PALETTE_MAX_SIZE, cached_colors, n,
                 n_cached_colors);
  } else {
    memcpy(pmi->palette_colors + PALETTE_MAX_SIZE, cached_colors,
           n * sizeof(cached_colors[0]));
  }

  // V channel colors.
  if (aom_read_bit(r, ACCT_STR)) {  // Delta encoding.
    const int min_bits_v = bit_depth - 4;
    const int max_val = 1 << bit_depth;
    int bits = min_bits_v + aom_read_literal(r, 2, ACCT_STR);
    pmi->palette_colors[2 * PALETTE_MAX_SIZE] =
        aom_read_literal(r, bit_depth, ACCT_STR);
    for (int i = 1; i < n; ++i) {
      int delta = aom_read_literal(r, bits, ACCT_STR);
      if (delta && aom_read_bit(r, ACCT_STR)) delta = -delta;
      int val = (int)pmi->palette_colors[2 * PALETTE_MAX_SIZE + i - 1] + delta;
      if (val < 0) val += max_val;
      if (val >= max_val) val -= max_val;
      pmi->palette_colors[2 * PALETTE_MAX_SIZE + i] = val;
    }
  } else {
    for (int i = 0; i < n; ++i) {
      pmi->palette_colors[2 * PALETTE_MAX_SIZE + i] =
          aom_read_literal(r, bit_depth, ACCT_STR);
    }
  }
}
#endif  // CONFIG_PALETTE_DELTA_ENCODING

static void read_palette_mode_info(AV1_COMMON *const cm, MACROBLOCKD *const xd,
                                   aom_reader *r) {
  MODE_INFO *const mi = xd->mi[0];
  MB_MODE_INFO *const mbmi = &mi->mbmi;
  const MODE_INFO *const above_mi = xd->above_mi;
  const MODE_INFO *const left_mi = xd->left_mi;
  const BLOCK_SIZE bsize = mbmi->sb_type;
  PALETTE_MODE_INFO *const pmi = &mbmi->palette_mode_info;

  assert(bsize >= BLOCK_8X8 && bsize <= BLOCK_LARGEST);
  const int block_palette_idx = bsize - BLOCK_8X8;
  int modev;

  if (mbmi->mode == DC_PRED) {
    int palette_y_mode_ctx = 0;
    if (above_mi) {
      palette_y_mode_ctx +=
          (above_mi->mbmi.palette_mode_info.palette_size[0] > 0);
    }
    if (left_mi) {
      palette_y_mode_ctx +=
          (left_mi->mbmi.palette_mode_info.palette_size[0] > 0);
    }
#if CONFIG_NEW_MULTISYMBOL
    modev = aom_read_symbol(
        r,
        xd->tile_ctx->palette_y_mode_cdf[block_palette_idx][palette_y_mode_ctx],
        2, ACCT_STR);
#else
    modev = aom_read(
        r,
        av1_default_palette_y_mode_prob[block_palette_idx][palette_y_mode_ctx],
        ACCT_STR);
#endif
    if (modev) {
      pmi->palette_size[0] =
          aom_read_symbol(r,
                          xd->tile_ctx->palette_y_size_cdf[block_palette_idx],
                          PALETTE_SIZES, ACCT_STR) +
          2;
#if CONFIG_PALETTE_DELTA_ENCODING
      read_palette_colors_y(xd, cm->bit_depth, pmi, r);
#else
      for (int i = 0; i < pmi->palette_size[0]; ++i)
        pmi->palette_colors[i] = aom_read_literal(r, cm->bit_depth, ACCT_STR);
#endif  // CONFIG_PALETTE_DELTA_ENCODING
    }
  }
  if (mbmi->uv_mode == UV_DC_PRED) {
    const int palette_uv_mode_ctx = (pmi->palette_size[0] > 0);
#if CONFIG_NEW_MULTISYMBOL
    modev = aom_read_symbol(
        r, xd->tile_ctx->palette_uv_mode_cdf[palette_uv_mode_ctx], 2, ACCT_STR);
#else
    modev = aom_read(r, av1_default_palette_uv_mode_prob[palette_uv_mode_ctx],
                     ACCT_STR);
#endif
    if (modev) {
      pmi->palette_size[1] =
          aom_read_symbol(r,
                          xd->tile_ctx->palette_uv_size_cdf[block_palette_idx],
                          PALETTE_SIZES, ACCT_STR) +
          2;
#if CONFIG_PALETTE_DELTA_ENCODING
      read_palette_colors_uv(xd, cm->bit_depth, pmi, r);
#else
      for (int i = 0; i < pmi->palette_size[1]; ++i) {
        pmi->palette_colors[PALETTE_MAX_SIZE + i] =
            aom_read_literal(r, cm->bit_depth, ACCT_STR);
        pmi->palette_colors[2 * PALETTE_MAX_SIZE + i] =
            aom_read_literal(r, cm->bit_depth, ACCT_STR);
      }
#endif  // CONFIG_PALETTE_DELTA_ENCODING
    }
  }
}

#if CONFIG_FILTER_INTRA
static void read_filter_intra_mode_info(AV1_COMMON *const cm,
                                        MACROBLOCKD *const xd, int mi_row,
                                        int mi_col, aom_reader *r) {
  MODE_INFO *const mi = xd->mi[0];
  MB_MODE_INFO *const mbmi = &mi->mbmi;
  FRAME_COUNTS *counts = xd->counts;
  FILTER_INTRA_MODE_INFO *filter_intra_mode_info =
      &mbmi->filter_intra_mode_info;

  if (mbmi->mode == DC_PRED && mbmi->palette_mode_info.palette_size[0] == 0) {
    filter_intra_mode_info->use_filter_intra_mode[0] =
        aom_read(r, cm->fc->filter_intra_probs[0], ACCT_STR);
    if (filter_intra_mode_info->use_filter_intra_mode[0]) {
      filter_intra_mode_info->filter_intra_mode[0] =
          av1_read_uniform(r, FILTER_INTRA_MODES);
    }
    if (counts) {
      ++counts
            ->filter_intra[0][filter_intra_mode_info->use_filter_intra_mode[0]];
    }
  }

#if CONFIG_CB4X4
  if (!is_chroma_reference(mi_row, mi_col, mbmi->sb_type,
                           xd->plane[1].subsampling_x,
                           xd->plane[1].subsampling_y))
    return;
#else
  (void)mi_row;
  (void)mi_col;
#endif  // CONFIG_CB4X4

  if (mbmi->uv_mode == UV_DC_PRED &&
      mbmi->palette_mode_info.palette_size[1] == 0) {
    filter_intra_mode_info->use_filter_intra_mode[1] =
        aom_read(r, cm->fc->filter_intra_probs[1], ACCT_STR);
    if (filter_intra_mode_info->use_filter_intra_mode[1]) {
      filter_intra_mode_info->filter_intra_mode[1] =
          av1_read_uniform(r, FILTER_INTRA_MODES);
    }
    if (counts) {
      ++counts
            ->filter_intra[1][filter_intra_mode_info->use_filter_intra_mode[1]];
    }
  }
}
#endif  // CONFIG_FILTER_INTRA

#if CONFIG_EXT_INTRA
static void read_intra_angle_info(AV1_COMMON *const cm, MACROBLOCKD *const xd,
                                  aom_reader *r) {
  MB_MODE_INFO *const mbmi = &xd->mi[0]->mbmi;
  const BLOCK_SIZE bsize = mbmi->sb_type;
#if CONFIG_INTRA_INTERP
  FRAME_CONTEXT *const ec_ctx = xd->tile_ctx;
  const int ctx = av1_get_pred_context_intra_interp(xd);
  int p_angle;
#endif  // CONFIG_INTRA_INTERP

  (void)cm;

  mbmi->angle_delta[0] = 0;
  mbmi->angle_delta[1] = 0;
#if CONFIG_INTRA_INTERP
  mbmi->intra_filter = INTRA_FILTER_LINEAR;
#endif  // CONFIG_INTRA_INTERP

  if (!av1_use_angle_delta(bsize)) return;

  if (av1_is_directional_mode(mbmi->mode, bsize)) {
    mbmi->angle_delta[0] =
        av1_read_uniform(r, 2 * MAX_ANGLE_DELTA + 1) - MAX_ANGLE_DELTA;
#if CONFIG_INTRA_INTERP
    p_angle = mode_to_angle_map[mbmi->mode] + mbmi->angle_delta[0] * ANGLE_STEP;
    if (av1_is_intra_filter_switchable(p_angle)) {
      FRAME_COUNTS *counts = xd->counts;
      mbmi->intra_filter = aom_read_symbol(r, ec_ctx->intra_filter_cdf[ctx],
                                           INTRA_FILTERS, ACCT_STR);
      if (counts) ++counts->intra_filter[ctx][mbmi->intra_filter];
    }
#endif  // CONFIG_INTRA_INTERP
  }

  if (av1_is_directional_mode(get_uv_mode(mbmi->uv_mode), bsize)) {
    mbmi->angle_delta[1] =
        av1_read_uniform(r, 2 * MAX_ANGLE_DELTA + 1) - MAX_ANGLE_DELTA;
  }
}
#endif  // CONFIG_EXT_INTRA

void av1_read_tx_type(const AV1_COMMON *const cm, MACROBLOCKD *xd,
#if CONFIG_SUPERTX
                      int supertx_enabled,
#endif
#if CONFIG_TXK_SEL
                      int blk_row, int blk_col, int block, int plane,
                      TX_SIZE tx_size,
#endif
                      aom_reader *r) {
  MB_MODE_INFO *mbmi = &xd->mi[0]->mbmi;
  const int inter_block = is_inter_block(mbmi);
#if !CONFIG_TXK_SEL
#if CONFIG_VAR_TX
  const TX_SIZE tx_size = inter_block ? mbmi->min_tx_size : mbmi->tx_size;
#else
  const TX_SIZE tx_size = mbmi->tx_size;
#endif
#endif  // !CONFIG_TXK_SEL
  FRAME_CONTEXT *ec_ctx = xd->tile_ctx;

#if !CONFIG_TXK_SEL
  TX_TYPE *tx_type = &mbmi->tx_type;
#else
  // only y plane's tx_type is transmitted
  if (plane > 0) return;
  (void)block;
  TX_TYPE *tx_type = &mbmi->txk_type[(blk_row << 4) + blk_col];
#endif
#if CONFIG_LGT_FROM_PRED
  mbmi->use_lgt = 0;
#endif

  if (!FIXED_TX_TYPE) {
#if CONFIG_EXT_TX
    const TX_SIZE square_tx_size = txsize_sqr_map[tx_size];
    if (get_ext_tx_types(tx_size, mbmi->sb_type, inter_block,
                         cm->reduced_tx_set_used) > 1 &&
        ((!cm->seg.enabled && cm->base_qindex > 0) ||
         (cm->seg.enabled && xd->qindex[mbmi->segment_id] > 0)) &&
        !mbmi->skip &&
#if CONFIG_SUPERTX
        !supertx_enabled &&
#endif  // CONFIG_SUPERTX
        !segfeature_active(&cm->seg, mbmi->segment_id, SEG_LVL_SKIP)) {
      const TxSetType tx_set_type = get_ext_tx_set_type(
          tx_size, mbmi->sb_type, inter_block, cm->reduced_tx_set_used);
      const int eset = get_ext_tx_set(tx_size, mbmi->sb_type, inter_block,
                                      cm->reduced_tx_set_used);
      // eset == 0 should correspond to a set with only DCT_DCT and
      // there is no need to read the tx_type
      assert(eset != 0);

#if !CONFIG_LGT_FROM_PRED
      if (inter_block) {
        *tx_type = av1_ext_tx_inv[tx_set_type][aom_read_symbol(
            r, ec_ctx->inter_ext_tx_cdf[eset][square_tx_size],
            av1_num_ext_tx_set[tx_set_type], ACCT_STR)];
      } else if (ALLOW_INTRA_EXT_TX) {
        *tx_type = av1_ext_tx_inv[tx_set_type][aom_read_symbol(
            r, ec_ctx->intra_ext_tx_cdf[eset][square_tx_size][mbmi->mode],
            av1_num_ext_tx_set[tx_set_type], ACCT_STR)];
      }
#else
      // only signal tx_type when lgt is not allowed or not selected
      if (inter_block) {
        if (LGT_FROM_PRED_INTER) {
          if (is_lgt_allowed(mbmi->mode, tx_size) && !cm->reduced_tx_set_used) {
            mbmi->use_lgt =
                aom_read(r, ec_ctx->inter_lgt_prob[square_tx_size], ACCT_STR);
#if CONFIG_ENTROPY_STATS
            if (counts) ++counts->inter_lgt[square_tx_size][mbmi->use_lgt];
#endif  // CONFIG_ENTROPY_STATS
          }
          if (!mbmi->use_lgt) {
            *tx_type = av1_ext_tx_inv[tx_set_type][aom_read_symbol(
                r, ec_ctx->inter_ext_tx_cdf[eset][square_tx_size],
                av1_num_ext_tx_set[tx_set_type], ACCT_STR)];
#if CONFIG_ENTROPY_STATS
            if (counts) ++counts->inter_ext_tx[eset][square_tx_size][*tx_type];
#endif  // CONFIG_ENTROPY_STATS
          } else {
            *tx_type = DCT_DCT;  // assign a dummy tx_type
          }
        } else {
          *tx_type = av1_ext_tx_inv[tx_set_type][aom_read_symbol(
              r, ec_ctx->inter_ext_tx_cdf[eset][square_tx_size],
              av1_num_ext_tx_set[tx_set_type], ACCT_STR)];
#if CONFIG_ENTROPY_STATS
          if (counts) ++counts->inter_ext_tx[eset][square_tx_size][*tx_type];
#endif  // CONFIG_ENTROPY_STATS
        }
      } else if (ALLOW_INTRA_EXT_TX) {
        if (LGT_FROM_PRED_INTRA) {
          if (is_lgt_allowed(mbmi->mode, tx_size) && !cm->reduced_tx_set_used) {
            mbmi->use_lgt =
                aom_read(r, ec_ctx->intra_lgt_prob[square_tx_size][mbmi->mode],
                         ACCT_STR);
#if CONFIG_ENTROPY_STATS
            if (counts)
              ++counts->intra_lgt[square_tx_size][mbmi->mode][mbmi->use_lgt];
#endif  // CONFIG_ENTROPY_STATS
          }
          if (!mbmi->use_lgt) {
            *tx_type = av1_ext_tx_inv[tx_set_type][aom_read_symbol(
                r, ec_ctx->intra_ext_tx_cdf[eset][square_tx_size][mbmi->mode],
                av1_num_ext_tx_set[tx_set_type], ACCT_STR)];
#if CONFIG_ENTROPY_STATS
            if (counts)
              ++counts
                    ->intra_ext_tx[eset][square_tx_size][mbmi->mode][*tx_type];
#endif  // CONFIG_ENTROPY_STATS
          } else {
            *tx_type = DCT_DCT;  // assign a dummy tx_type
          }
        } else {
          *tx_type = av1_ext_tx_inv[tx_set_type][aom_read_symbol(
              r, ec_ctx->intra_ext_tx_cdf[eset][square_tx_size][mbmi->mode],
              av1_num_ext_tx_set[tx_set_type], ACCT_STR)];
#if CONFIG_ENTROPY_STATS
          if (counts)
            ++counts->intra_ext_tx[eset][square_tx_size][mbmi->mode][*tx_type];
#endif  // CONFIG_ENTROPY_STATS
        }
      }
#endif  // CONFIG_LGT_FROM_PRED
    } else {
      *tx_type = DCT_DCT;
    }
#else  // CONFIG_EXT_TX

    if (tx_size < TX_32X32 &&
        ((!cm->seg.enabled && cm->base_qindex > 0) ||
         (cm->seg.enabled && xd->qindex[mbmi->segment_id] > 0)) &&
        !mbmi->skip &&
#if CONFIG_SUPERTX
        !supertx_enabled &&
#endif  // CONFIG_SUPERTX
        !segfeature_active(&cm->seg, mbmi->segment_id, SEG_LVL_SKIP)) {
#if CONFIG_ENTROPY_STATS
      FRAME_COUNTS *counts = xd->counts;
#endif  // CONFIG_ENTROPY_STATS
      if (inter_block) {
        *tx_type = av1_ext_tx_inv[aom_read_symbol(
            r, ec_ctx->inter_ext_tx_cdf[tx_size], TX_TYPES, ACCT_STR)];
#if CONFIG_ENTROPY_STATS
        if (counts) ++counts->inter_ext_tx[tx_size][*tx_type];
#endif  // CONFIG_ENTROPY_STATS
      } else {
        const TX_TYPE tx_type_nom = intra_mode_to_tx_type_context[mbmi->mode];
        *tx_type = av1_ext_tx_inv[aom_read_symbol(
            r, ec_ctx->intra_ext_tx_cdf[tx_size][tx_type_nom], TX_TYPES,
            ACCT_STR)];
#if CONFIG_ENTROPY_STATS
        if (counts) ++counts->intra_ext_tx[tx_size][tx_type_nom][*tx_type];
#endif  // CONFIG_ENTROPY_STATS
      }
    } else {
      *tx_type = DCT_DCT;
    }
#endif  // CONFIG_EXT_TX
  }
#if FIXED_TX_TYPE
  assert(mbmi->tx_type == DCT_DCT);
#endif
}

#if CONFIG_INTRABC
static INLINE void read_mv(aom_reader *r, MV *mv, const MV *ref,
                           nmv_context *ctx, nmv_context_counts *counts,
                           MvSubpelPrecision precision);

static INLINE int is_mv_valid(const MV *mv);

static INLINE int assign_dv(AV1_COMMON *cm, MACROBLOCKD *xd, int_mv *mv,
                            const int_mv *ref_mv, int mi_row, int mi_col,
                            BLOCK_SIZE bsize, aom_reader *r) {
  FRAME_CONTEXT *ec_ctx = xd->tile_ctx;
  (void)cm;
  FRAME_COUNTS *counts = xd->counts;
  nmv_context_counts *const dv_counts = counts ? &counts->dv : NULL;
  read_mv(r, &mv->as_mv, &ref_mv->as_mv, &ec_ctx->ndvc, dv_counts,
          MV_SUBPEL_NONE);
  int valid = is_mv_valid(&mv->as_mv) &&
              is_dv_valid(mv->as_mv, &xd->tile, mi_row, mi_col, bsize);
  return valid;
}
#endif  // CONFIG_INTRABC

static void read_intra_frame_mode_info(AV1_COMMON *const cm,
                                       MACROBLOCKD *const xd, int mi_row,
                                       int mi_col, aom_reader *r) {
  MODE_INFO *const mi = xd->mi[0];
  MB_MODE_INFO *const mbmi = &mi->mbmi;
  const MODE_INFO *above_mi = xd->above_mi;
  const MODE_INFO *left_mi = xd->left_mi;
  const BLOCK_SIZE bsize = mbmi->sb_type;
  int i;
  const int mi_offset = mi_row * cm->mi_cols + mi_col;
  const int bw = mi_size_wide[bsize];
  const int bh = mi_size_high[bsize];

  // TODO(slavarnway): move x_mis, y_mis into xd ?????
  const int x_mis = AOMMIN(cm->mi_cols - mi_col, bw);
  const int y_mis = AOMMIN(cm->mi_rows - mi_row, bh);
  FRAME_CONTEXT *ec_ctx = xd->tile_ctx;

  mbmi->segment_id = read_intra_segment_id(cm, xd, mi_offset, x_mis, y_mis, r);
  mbmi->skip = read_skip(cm, xd, mbmi->segment_id, r);

  if (cm->delta_q_present_flag) {
    xd->current_qindex =
        xd->prev_qindex +
        read_delta_qindex(cm, xd, r, mbmi, mi_col, mi_row) * cm->delta_q_res;
    /* Normative: Clamp to [1,MAXQ] to not interfere with lossless mode */
    xd->current_qindex = clamp(xd->current_qindex, 1, MAXQ);
    xd->prev_qindex = xd->current_qindex;
#if CONFIG_EXT_DELTA_Q
    if (cm->delta_lf_present_flag) {
#if CONFIG_LOOPFILTER_LEVEL
      if (cm->delta_lf_multi) {
        for (int lf_id = 0; lf_id < FRAME_LF_COUNT; ++lf_id) {
          mbmi->curr_delta_lf[lf_id] = xd->curr_delta_lf[lf_id] =
              xd->prev_delta_lf[lf_id] +
              read_delta_lflevel(cm, xd, r, lf_id, mbmi, mi_col, mi_row) *
                  cm->delta_lf_res;
          xd->prev_delta_lf[lf_id] = xd->curr_delta_lf[lf_id];
        }
      } else {
        mbmi->current_delta_lf_from_base = xd->current_delta_lf_from_base =
            xd->prev_delta_lf_from_base +
            read_delta_lflevel(cm, xd, r, -1, mbmi, mi_col, mi_row) *
                cm->delta_lf_res;
        xd->prev_delta_lf_from_base = xd->current_delta_lf_from_base;
      }
#else
      const int current_delta_lf_from_base =
          xd->prev_delta_lf_from_base +
          read_delta_lflevel(cm, xd, r, mbmi, mi_col, mi_row) *
              cm->delta_lf_res;
      mbmi->current_delta_lf_from_base = xd->current_delta_lf_from_base =
          clamp(current_delta_lf_from_base, 0, MAX_LOOP_FILTER);
      xd->prev_delta_lf_from_base = xd->current_delta_lf_from_base;
#endif  // CONFIG_LOOPFILTER_LEVEL
    }
#endif
  }

  mbmi->ref_frame[0] = INTRA_FRAME;
  mbmi->ref_frame[1] = NONE_FRAME;

#if CONFIG_INTRABC
  if (av1_allow_intrabc(bsize, cm)) {
    mbmi->use_intrabc = aom_read_symbol(r, ec_ctx->intrabc_cdf, 2, ACCT_STR);
    if (mbmi->use_intrabc) {
      mbmi->tx_size = read_tx_size(cm, xd, 1, !mbmi->skip, r);
      mbmi->mode = mbmi->uv_mode = UV_DC_PRED;
      mbmi->interp_filters = av1_broadcast_interp_filter(BILINEAR);

      int16_t inter_mode_ctx[MODE_CTX_REF_FRAMES];
      int_mv ref_mvs[MAX_MV_REF_CANDIDATES];

      av1_find_mv_refs(cm, xd, mi, INTRA_FRAME, &xd->ref_mv_count[INTRA_FRAME],
                       xd->ref_mv_stack[INTRA_FRAME], NULL, ref_mvs, mi_row,
                       mi_col, NULL, NULL, inter_mode_ctx);

      int_mv nearestmv, nearmv;
      av1_find_best_ref_mvs(0, ref_mvs, &nearestmv, &nearmv);

      int_mv dv_ref = nearestmv.as_int == 0 ? nearmv : nearestmv;
      if (dv_ref.as_int == 0) av1_find_ref_dv(&dv_ref, mi_row, mi_col);

      xd->corrupted |=
          !assign_dv(cm, xd, &mbmi->mv[0], &dv_ref, mi_row, mi_col, bsize, r);
#if CONFIG_VAR_TX
      // TODO(aconverse@google.com): Evaluate allowing VAR TX on intrabc blocks
      const int width = block_size_wide[bsize] >> tx_size_wide_log2[0];
      const int height = block_size_high[bsize] >> tx_size_high_log2[0];
      int idx, idy;
      for (idy = 0; idy < height; ++idy)
        for (idx = 0; idx < width; ++idx)
          mbmi->inter_tx_size[idy >> 1][idx >> 1] = mbmi->tx_size;
      mbmi->min_tx_size = get_min_tx_size(mbmi->tx_size);
#endif  // CONFIG_VAR_TX
#if CONFIG_EXT_TX && !CONFIG_TXK_SEL
      av1_read_tx_type(cm, xd,
#if CONFIG_SUPERTX
                       0,
#endif
                       r);
#endif  // CONFIG_EXT_TX && !CONFIG_TXK_SEL
      return;
    }
  }
#endif  // CONFIG_INTRABC

  mbmi->tx_size = read_tx_size(cm, xd, 0, 1, r);

#if CONFIG_CB4X4
  (void)i;
  mbmi->mode =
      read_intra_mode(r, get_y_mode_cdf(ec_ctx, mi, above_mi, left_mi, 0));
#else
  switch (bsize) {
    case BLOCK_4X4:
      for (i = 0; i < 4; ++i)
        mi->bmi[i].as_mode = read_intra_mode(
            r, get_y_mode_cdf(ec_ctx, mi, above_mi, left_mi, i));
      mbmi->mode = mi->bmi[3].as_mode;
      break;
    case BLOCK_4X8:
      mi->bmi[0].as_mode = mi->bmi[2].as_mode =
          read_intra_mode(r, get_y_mode_cdf(ec_ctx, mi, above_mi, left_mi, 0));
      mi->bmi[1].as_mode = mi->bmi[3].as_mode = mbmi->mode =
          read_intra_mode(r, get_y_mode_cdf(ec_ctx, mi, above_mi, left_mi, 1));
      break;
    case BLOCK_8X4:
      mi->bmi[0].as_mode = mi->bmi[1].as_mode =
          read_intra_mode(r, get_y_mode_cdf(ec_ctx, mi, above_mi, left_mi, 0));
      mi->bmi[2].as_mode = mi->bmi[3].as_mode = mbmi->mode =
          read_intra_mode(r, get_y_mode_cdf(ec_ctx, mi, above_mi, left_mi, 2));
      break;
    default:
      mbmi->mode =
          read_intra_mode(r, get_y_mode_cdf(ec_ctx, mi, above_mi, left_mi, 0));
  }
#endif

#if CONFIG_CB4X4
  if (is_chroma_reference(mi_row, mi_col, bsize, xd->plane[1].subsampling_x,
                          xd->plane[1].subsampling_y)) {
#if CONFIG_CFL
    xd->cfl->is_chroma_reference = 1;
#endif  // CONFIG_CFL
#endif  // CONFIG_CB4X4
    mbmi->uv_mode = read_intra_mode_uv(ec_ctx, r, mbmi->mode);

#if CONFIG_CFL
    if (mbmi->uv_mode == UV_CFL_PRED) {
      mbmi->cfl_alpha_idx = read_cfl_alphas(ec_ctx, r, &mbmi->cfl_alpha_signs);
      xd->cfl->store_y = 1;
    } else {
      xd->cfl->store_y = 0;
    }
#endif  // CONFIG_CFL

#if CONFIG_CB4X4
  } else {
    // Avoid decoding angle_info if there is is no chroma prediction
    mbmi->uv_mode = UV_DC_PRED;
#if CONFIG_CFL
    xd->cfl->is_chroma_reference = 0;
    xd->cfl->store_y = 1;
#endif
  }
#endif

#if CONFIG_EXT_INTRA
  read_intra_angle_info(cm, xd, r);
#endif  // CONFIG_EXT_INTRA
  mbmi->palette_mode_info.palette_size[0] = 0;
  mbmi->palette_mode_info.palette_size[1] = 0;
  if (av1_allow_palette(cm->allow_screen_content_tools, bsize))
    read_palette_mode_info(cm, xd, r);
#if CONFIG_FILTER_INTRA
  mbmi->filter_intra_mode_info.use_filter_intra_mode[0] = 0;
  mbmi->filter_intra_mode_info.use_filter_intra_mode[1] = 0;
  if (bsize >= BLOCK_8X8 || CONFIG_CB4X4)
    read_filter_intra_mode_info(cm, xd, mi_row, mi_col, r);
#endif  // CONFIG_FILTER_INTRA

#if !CONFIG_TXK_SEL
  av1_read_tx_type(cm, xd,
#if CONFIG_SUPERTX
                   0,
#endif
                   r);
#endif  // !CONFIG_TXK_SEL
}

static int read_mv_component(aom_reader *r, nmv_component *mvcomp,
#if CONFIG_INTRABC || CONFIG_AMVR
                             int use_subpel,
#endif  // CONFIG_INTRABC || CONFIG_AMVR
                             int usehp) {
  int mag, d, fr, hp;
#if CONFIG_NEW_MULTISYMBOL
  const int sign = aom_read_bit(r, ACCT_STR);
#else
  const int sign = aom_read(r, mvcomp->sign, ACCT_STR);
#endif
  const int mv_class =
      aom_read_symbol(r, mvcomp->class_cdf, MV_CLASSES, ACCT_STR);
  const int class0 = mv_class == MV_CLASS_0;

  // Integer part
  if (class0) {
#if CONFIG_NEW_MULTISYMBOL
    d = aom_read_symbol(r, mvcomp->class0_cdf, CLASS0_SIZE, ACCT_STR);
#else
    d = aom_read(r, mvcomp->class0[0], ACCT_STR);
#endif
    mag = 0;
  } else {
    int i;
    const int n = mv_class + CLASS0_BITS - 1;  // number of bits
    d = 0;
#if CONFIG_NEW_MULTISYMBOL
    for (i = 0; i < n; ++i)
      d |= aom_read_symbol(r, mvcomp->bits_cdf[(i + 1) / 2], 2, ACCT_STR) << i;
#else
    for (i = 0; i < n; ++i) d |= aom_read(r, mvcomp->bits[i], ACCT_STR) << i;
#endif
    mag = CLASS0_SIZE << (mv_class + 2);
  }

#if CONFIG_INTRABC || CONFIG_AMVR
  if (use_subpel) {
#endif  // CONFIG_INTRABC || CONFIG_AMVR
        // Fractional part
    fr = aom_read_symbol(r, class0 ? mvcomp->class0_fp_cdf[d] : mvcomp->fp_cdf,
                         MV_FP_SIZE, ACCT_STR);

// High precision part (if hp is not used, the default value of the hp is 1)
#if CONFIG_NEW_MULTISYMBOL
    hp = usehp ? aom_read_symbol(
                     r, class0 ? mvcomp->class0_hp_cdf : mvcomp->hp_cdf, 2,
                     ACCT_STR)
               : 1;
#else
  hp = usehp ? aom_read(r, class0 ? mvcomp->class0_hp : mvcomp->hp, ACCT_STR)
             : 1;
#endif
#if CONFIG_INTRABC || CONFIG_AMVR
  } else {
    fr = 3;
    hp = 1;
  }
#endif  // CONFIG_INTRABC || CONFIG_AMVR

  // Result
  mag += ((d << 3) | (fr << 1) | hp) + 1;
  return sign ? -mag : mag;
}

static INLINE void read_mv(aom_reader *r, MV *mv, const MV *ref,
                           nmv_context *ctx, nmv_context_counts *counts,
                           MvSubpelPrecision precision) {
  MV_JOINT_TYPE joint_type;
  MV diff = { 0, 0 };
  joint_type =
      (MV_JOINT_TYPE)aom_read_symbol(r, ctx->joint_cdf, MV_JOINTS, ACCT_STR);

  if (mv_joint_vertical(joint_type))
    diff.row = read_mv_component(r, &ctx->comps[0],
#if CONFIG_INTRABC || CONFIG_AMVR
                                 precision > MV_SUBPEL_NONE,
#endif  // CONFIG_INTRABC || CONFIG_AMVR
                                 precision > MV_SUBPEL_LOW_PRECISION);

  if (mv_joint_horizontal(joint_type))
    diff.col = read_mv_component(r, &ctx->comps[1],
#if CONFIG_INTRABC || CONFIG_AMVR
                                 precision > MV_SUBPEL_NONE,
#endif  // CONFIG_INTRABC || CONFIG_AMVR
                                 precision > MV_SUBPEL_LOW_PRECISION);

  av1_inc_mv(&diff, counts, precision);

  mv->row = ref->row + diff.row;
  mv->col = ref->col + diff.col;
}

static REFERENCE_MODE read_block_reference_mode(AV1_COMMON *cm,
                                                const MACROBLOCKD *xd,
                                                aom_reader *r) {
  if (!is_comp_ref_allowed(xd->mi[0]->mbmi.sb_type)) return SINGLE_REFERENCE;
  if (cm->reference_mode == REFERENCE_MODE_SELECT) {
    const int ctx = av1_get_reference_mode_context(cm, xd);
#if CONFIG_NEW_MULTISYMBOL
    const REFERENCE_MODE mode = (REFERENCE_MODE)aom_read_symbol(
        r, xd->tile_ctx->comp_inter_cdf[ctx], 2, ACCT_STR);
#else
    const REFERENCE_MODE mode =
        (REFERENCE_MODE)aom_read(r, cm->fc->comp_inter_prob[ctx], ACCT_STR);
#endif
    FRAME_COUNTS *counts = xd->counts;
    if (counts) ++counts->comp_inter[ctx][mode];
    return mode;  // SINGLE_REFERENCE or COMPOUND_REFERENCE
  } else {
    return cm->reference_mode;
  }
}

#if CONFIG_NEW_MULTISYMBOL
#define READ_REF_BIT(pname) \
  aom_read_symbol(r, av1_get_pred_cdf_##pname(cm, xd), 2, ACCT_STR)
#define READ_REF_BIT2(pname) \
  aom_read_symbol(r, av1_get_pred_cdf_##pname(xd), 2, ACCT_STR)
#else
#define READ_REF_BIT(pname) \
  aom_read(r, av1_get_pred_prob_##pname(cm, xd), ACCT_STR)
#define READ_REF_BIT2(pname) \
  aom_read(r, av1_get_pred_prob_##pname(cm, xd), ACCT_STR)
#endif

#if CONFIG_EXT_COMP_REFS
static COMP_REFERENCE_TYPE read_comp_reference_type(AV1_COMMON *cm,
                                                    const MACROBLOCKD *xd,
                                                    aom_reader *r) {
  const int ctx = av1_get_comp_reference_type_context(xd);
#if USE_UNI_COMP_REFS
  COMP_REFERENCE_TYPE comp_ref_type;
#if CONFIG_VAR_REFS
  if ((L_OR_L2(cm) || L3_OR_G(cm)) && BWD_OR_ALT(cm)) {
    if (L_AND_L2(cm) || L_AND_L3(cm) || L_AND_G(cm) || BWD_AND_ALT(cm)) {
#endif  // CONFIG_VAR_REFS
#if CONFIG_NEW_MULTISYMBOL
      (void)cm;
      comp_ref_type = (COMP_REFERENCE_TYPE)aom_read_symbol(
          r, xd->tile_ctx->comp_ref_type_cdf[ctx], 2, ACCT_STR);
#else
  comp_ref_type = (COMP_REFERENCE_TYPE)aom_read(
      r, cm->fc->comp_ref_type_prob[ctx], ACCT_STR);
#endif
#if CONFIG_VAR_REFS
    } else {
      comp_ref_type = BIDIR_COMP_REFERENCE;
    }
  } else {
    comp_ref_type = UNIDIR_COMP_REFERENCE;
  }
#endif  // CONFIG_VAR_REFS
#else   // !USE_UNI_COMP_REFS
  // TODO(zoeliu): Temporarily turn off uni-directional comp refs
  const COMP_REFERENCE_TYPE comp_ref_type = BIDIR_COMP_REFERENCE;
#endif  // USE_UNI_COMP_REFS
  FRAME_COUNTS *counts = xd->counts;
  if (counts) ++counts->comp_ref_type[ctx][comp_ref_type];
  return comp_ref_type;  // UNIDIR_COMP_REFERENCE or BIDIR_COMP_REFERENCE
}
#endif  // CONFIG_EXT_COMP_REFS

// Read the referncence frame
static void read_ref_frames(AV1_COMMON *const cm, MACROBLOCKD *const xd,
                            aom_reader *r, int segment_id,
                            MV_REFERENCE_FRAME ref_frame[2]) {
  FRAME_COUNTS *counts = xd->counts;

  if (segfeature_active(&cm->seg, segment_id, SEG_LVL_REF_FRAME)) {
    ref_frame[0] = (MV_REFERENCE_FRAME)get_segdata(&cm->seg, segment_id,
                                                   SEG_LVL_REF_FRAME);
    ref_frame[1] = NONE_FRAME;
  } else {
    const REFERENCE_MODE mode = read_block_reference_mode(cm, xd, r);
    // FIXME(rbultje) I'm pretty sure this breaks segmentation ref frame coding
    if (mode == COMPOUND_REFERENCE) {
#if CONFIG_EXT_COMP_REFS
      const COMP_REFERENCE_TYPE comp_ref_type =
          read_comp_reference_type(cm, xd, r);

#if !USE_UNI_COMP_REFS
      // TODO(zoeliu): Temporarily turn off uni-directional comp refs
      assert(comp_ref_type == BIDIR_COMP_REFERENCE);
#endif  // !USE_UNI_COMP_REFS

      if (comp_ref_type == UNIDIR_COMP_REFERENCE) {
        const int ctx = av1_get_pred_context_uni_comp_ref_p(xd);
        int bit;
#if CONFIG_VAR_REFS
        if ((L_AND_L2(cm) || L_AND_L3(cm) || L_AND_G(cm)) && BWD_AND_ALT(cm))
#endif  // CONFIG_VAR_REFS
          bit = READ_REF_BIT2(uni_comp_ref_p);
#if CONFIG_VAR_REFS
        else
          bit = BWD_AND_ALT(cm);
#endif  // CONFIG_VAR_REFS
        if (counts) ++counts->uni_comp_ref[ctx][0][bit];

        if (bit) {
          ref_frame[0] = BWDREF_FRAME;
          ref_frame[1] = ALTREF_FRAME;
        } else {
          const int ctx1 = av1_get_pred_context_uni_comp_ref_p1(xd);
          int bit1;
#if CONFIG_VAR_REFS
          if (L_AND_L2(cm) && (L_AND_L3(cm) || L_AND_G(cm)))
#endif  // CONFIG_VAR_REFS
            bit1 = READ_REF_BIT2(uni_comp_ref_p1);
#if CONFIG_VAR_REFS
          else
            bit1 = L_AND_L3(cm) || L_AND_G(cm);
#endif  // CONFIG_VAR_REFS
          if (counts) ++counts->uni_comp_ref[ctx1][1][bit1];

          if (bit1) {
            const int ctx2 = av1_get_pred_context_uni_comp_ref_p2(xd);
            int bit2;
#if CONFIG_VAR_REFS
            if (L_AND_L3(cm) && L_AND_G(cm))
#endif  // CONFIG_VAR_REFS
              bit2 = READ_REF_BIT2(uni_comp_ref_p2);
#if CONFIG_VAR_REFS
            else
              bit2 = L_AND_G(cm);
#endif  // CONFIG_VAR_REFS
            if (counts) ++counts->uni_comp_ref[ctx2][2][bit2];

            if (bit2) {
              ref_frame[0] = LAST_FRAME;
              ref_frame[1] = GOLDEN_FRAME;
            } else {
              ref_frame[0] = LAST_FRAME;
              ref_frame[1] = LAST3_FRAME;
            }
          } else {
            ref_frame[0] = LAST_FRAME;
            ref_frame[1] = LAST2_FRAME;
          }
        }

        return;
      }

      assert(comp_ref_type == BIDIR_COMP_REFERENCE);
#endif  // CONFIG_EXT_COMP_REFS

// Normative in decoder (for low delay)
#if CONFIG_ONE_SIDED_COMPOUND || CONFIG_FRAME_SIGN_BIAS
      const int idx = 1;
#else  // !(CONFIG_ONE_SIDED_COMPOUND || CONFIG_FRAME_SIGN_BIAS)
#if CONFIG_EXT_REFS
      const int idx = cm->ref_frame_sign_bias[cm->comp_bwd_ref[0]];
#else   // !CONFIG_EXT_REFS
      const int idx = cm->ref_frame_sign_bias[cm->comp_fixed_ref];
#endif  // CONFIG_EXT_REFS
#endif  // CONFIG_ONE_SIDED_COMPOUND || CONFIG_FRAME_SIGN_BIAS)

      const int ctx = av1_get_pred_context_comp_ref_p(cm, xd);
#if CONFIG_VAR_REFS
      int bit;
      // Test need to explicitly code (L,L2) vs (L3,G) branch node in tree
      if (L_OR_L2(cm) && L3_OR_G(cm))
        bit = READ_REF_BIT(comp_ref_p);
      else
        bit = L3_OR_G(cm);
#else   // !CONFIG_VAR_REFS
      const int bit = READ_REF_BIT(comp_ref_p);
#endif  // CONFIG_VAR_REFS
      if (counts) ++counts->comp_ref[ctx][0][bit];

#if CONFIG_EXT_REFS
      // Decode forward references.
      if (!bit) {
        const int ctx1 = av1_get_pred_context_comp_ref_p1(cm, xd);
#if CONFIG_VAR_REFS
        int bit1;
        // Test need to explicitly code (L) vs (L2) branch node in tree
        if (L_AND_L2(cm))
          bit1 = READ_REF_BIT(comp_ref_p1);
        else
          bit1 = LAST_IS_VALID(cm);
#else   // !CONFIG_VAR_REFS
        const int bit1 = READ_REF_BIT(comp_ref_p1);
#endif  // CONFIG_VAR_REFS
        if (counts) ++counts->comp_ref[ctx1][1][bit1];
        ref_frame[!idx] = cm->comp_fwd_ref[bit1 ? 0 : 1];
      } else {
        const int ctx2 = av1_get_pred_context_comp_ref_p2(cm, xd);
#if CONFIG_VAR_REFS
        int bit2;
        // Test need to explicitly code (L3) vs (G) branch node in tree
        if (L3_AND_G(cm))
          bit2 = READ_REF_BIT(comp_ref_p2);
        else
          bit2 = GOLDEN_IS_VALID(cm);
#else   // !CONFIG_VAR_REFS
        const int bit2 = READ_REF_BIT(comp_ref_p2);
#endif  // CONFIG_VAR_REFS
        if (counts) ++counts->comp_ref[ctx2][2][bit2];
        ref_frame[!idx] = cm->comp_fwd_ref[bit2 ? 3 : 2];
      }

      // Decode backward references.
      const int ctx_bwd = av1_get_pred_context_comp_bwdref_p(cm, xd);
#if CONFIG_VAR_REFS
      int bit_bwd;
      // Test need to explicitly code (BWD/ALT2) vs (ALT) branch node in tree
      const int bit_bwd_uncertain = BWD_OR_ALT2(cm) && ALTREF_IS_VALID(cm);
      if (bit_bwd_uncertain)
        bit_bwd = READ_REF_BIT(comp_bwdref_p);
      else
        bit_bwd = ALTREF_IS_VALID(cm);
#else  // !CONFIG_VAR_REFS
      const int bit_bwd = READ_REF_BIT(comp_bwdref_p);
#endif  // CONFIG_VAR_REFS
      if (counts) ++counts->comp_bwdref[ctx_bwd][0][bit_bwd];
      if (!bit_bwd) {
        const int ctx1_bwd = av1_get_pred_context_comp_bwdref_p1(cm, xd);
#if CONFIG_VAR_REFS
        int bit1_bwd;
        if (BWD_AND_ALT2(cm))
          bit1_bwd = READ_REF_BIT(comp_bwdref_p1);
        else
          bit1_bwd = ALTREF2_IS_VALID(cm);
#else  // !CONFIG_VAR_REFS
        const int bit1_bwd = READ_REF_BIT(comp_bwdref_p1);
#endif  // CONFIG_VAR_REFS
        if (counts) ++counts->comp_bwdref[ctx1_bwd][1][bit1_bwd];
        ref_frame[idx] = cm->comp_bwd_ref[bit1_bwd];
      } else {
        ref_frame[idx] = cm->comp_bwd_ref[2];
      }
#else   // !CONFIG_EXT_REFS
      ref_frame[!idx] = cm->comp_var_ref[bit];
      ref_frame[idx] = cm->comp_fixed_ref;
#endif  // CONFIG_EXT_REFS
    } else if (mode == SINGLE_REFERENCE) {
#if CONFIG_EXT_REFS
      const int ctx0 = av1_get_pred_context_single_ref_p1(xd);
#if CONFIG_VAR_REFS
      int bit0;
      // Test need to explicitly code (L,L2,L3,G) vs (BWD,ALT2,ALT) branch node
      // in tree
      if ((L_OR_L2(cm) || L3_OR_G(cm)) &&
          (BWD_OR_ALT2(cm) || ALTREF_IS_VALID(cm)))
        bit0 = READ_REF_BIT(single_ref_p1);
      else
        bit0 = (BWD_OR_ALT2(cm) || ALTREF_IS_VALID(cm));
#else   // !CONFIG_VAR_REFS
      const int bit0 = READ_REF_BIT(single_ref_p1);
#endif  // CONFIG_VAR_REFS
      if (counts) ++counts->single_ref[ctx0][0][bit0];

      if (bit0) {
        const int ctx1 = av1_get_pred_context_single_ref_p2(xd);
#if CONFIG_VAR_REFS
        int bit1;
        // Test need to explicitly code (BWD/ALT2) vs (ALT) branch node in tree
        const int bit1_uncertain = BWD_OR_ALT2(cm) && ALTREF_IS_VALID(cm);
        if (bit1_uncertain)
          bit1 = READ_REF_BIT(single_ref_p2);
        else
          bit1 = ALTREF_IS_VALID(cm);
#else  // !CONFIG_VAR_REFS
        const int bit1 = READ_REF_BIT(single_ref_p2);
#endif  // CONFIG_VAR_REFS
        if (counts) ++counts->single_ref[ctx1][1][bit1];
        if (!bit1) {
          const int ctx5 = av1_get_pred_context_single_ref_p6(xd);
#if CONFIG_VAR_REFS
          int bit5;
          if (BWD_AND_ALT2(cm))
            bit5 = READ_REF_BIT(single_ref_p6);
          else
            bit5 = ALTREF2_IS_VALID(cm);
#else  // !CONFIG_VAR_REFS
          const int bit5 = READ_REF_BIT(single_ref_p6);
#endif  // CONFIG_VAR_REFS
          if (counts) ++counts->single_ref[ctx5][5][bit5];
          ref_frame[0] = bit5 ? ALTREF2_FRAME : BWDREF_FRAME;
        } else {
          ref_frame[0] = ALTREF_FRAME;
        }
      } else {
        const int ctx2 = av1_get_pred_context_single_ref_p3(xd);
#if CONFIG_VAR_REFS
        int bit2;
        // Test need to explicitly code (L,L2) vs (L3,G) branch node in tree
        if (L_OR_L2(cm) && L3_OR_G(cm))
          bit2 = READ_REF_BIT(single_ref_p3);
        else
          bit2 = L3_OR_G(cm);
#else  // !CONFIG_VAR_REFS
        const int bit2 = READ_REF_BIT(single_ref_p3);
#endif  // CONFIG_VAR_REFS
        if (counts) ++counts->single_ref[ctx2][2][bit2];
        if (bit2) {
          const int ctx4 = av1_get_pred_context_single_ref_p5(xd);
#if CONFIG_VAR_REFS
          int bit4;
          // Test need to explicitly code (L3) vs (G) branch node in tree
          if (L3_AND_G(cm))
            bit4 = READ_REF_BIT(single_ref_p5);
          else
            bit4 = GOLDEN_IS_VALID(cm);
#else  // !CONFIG_VAR_REFS
          const int bit4 = READ_REF_BIT(single_ref_p5);
#endif  // CONFIG_VAR_REFS
          if (counts) ++counts->single_ref[ctx4][4][bit4];
          ref_frame[0] = bit4 ? GOLDEN_FRAME : LAST3_FRAME;
        } else {
          const int ctx3 = av1_get_pred_context_single_ref_p4(xd);
#if CONFIG_VAR_REFS
          int bit3;
          // Test need to explicitly code (L) vs (L2) branch node in tree
          if (L_AND_L2(cm))
            bit3 = READ_REF_BIT(single_ref_p4);
          else
            bit3 = LAST2_IS_VALID(cm);
#else  // !CONFIG_VAR_REFS
          const int bit3 = READ_REF_BIT(single_ref_p4);
#endif  // CONFIG_VAR_REFS
          if (counts) ++counts->single_ref[ctx3][3][bit3];
          ref_frame[0] = bit3 ? LAST2_FRAME : LAST_FRAME;
        }
      }
#else   // !CONFIG_EXT_REFS
      const int ctx0 = av1_get_pred_context_single_ref_p1(xd);
      const int bit0 = READ_REF_BIT(single_ref_p1);
      if (counts) ++counts->single_ref[ctx0][0][bit0];

      if (bit0) {
        const int ctx1 = av1_get_pred_context_single_ref_p2(xd);
        const int bit1 = READ_REF_BIT(single_ref_p2);
        if (counts) ++counts->single_ref[ctx1][1][bit1];
        ref_frame[0] = bit1 ? ALTREF_FRAME : GOLDEN_FRAME;
      } else {
        ref_frame[0] = LAST_FRAME;
      }
#endif  // CONFIG_EXT_REFS

      ref_frame[1] = NONE_FRAME;
    } else {
      assert(0 && "Invalid prediction mode.");
    }
  }
}

static INLINE void read_mb_interp_filter(AV1_COMMON *const cm,
                                         MACROBLOCKD *const xd,
                                         MB_MODE_INFO *const mbmi,
                                         aom_reader *r) {
  FRAME_COUNTS *counts = xd->counts;
  FRAME_CONTEXT *ec_ctx = xd->tile_ctx;

  if (!av1_is_interp_needed(xd)) {
    set_default_interp_filters(mbmi, cm->interp_filter);
    return;
  }

  if (cm->interp_filter != SWITCHABLE) {
    mbmi->interp_filters = av1_broadcast_interp_filter(cm->interp_filter);
  } else {
#if CONFIG_DUAL_FILTER
    InterpFilter ref0_filter[2] = { EIGHTTAP_REGULAR, EIGHTTAP_REGULAR };
    for (int dir = 0; dir < 2; ++dir) {
      if (has_subpel_mv_component(xd->mi[0], xd, dir) ||
          (mbmi->ref_frame[1] > INTRA_FRAME &&
           has_subpel_mv_component(xd->mi[0], xd, dir + 2))) {
        const int ctx = av1_get_pred_context_switchable_interp(xd, dir);
        ref0_filter[dir] =
            (InterpFilter)aom_read_symbol(r, ec_ctx->switchable_interp_cdf[ctx],
                                          SWITCHABLE_FILTERS, ACCT_STR);
        if (counts) ++counts->switchable_interp[ctx][ref0_filter[dir]];
      }
    }
    // The index system works as: (0, 1) -> (vertical, horizontal) filter types
    mbmi->interp_filters =
        av1_make_interp_filters(ref0_filter[0], ref0_filter[1]);
#else   // CONFIG_DUAL_FILTER
    const int ctx = av1_get_pred_context_switchable_interp(xd);
    InterpFilter filter = (InterpFilter)aom_read_symbol(
        r, ec_ctx->switchable_interp_cdf[ctx], SWITCHABLE_FILTERS, ACCT_STR);
    mbmi->interp_filters = av1_broadcast_interp_filter(filter);
    if (counts) ++counts->switchable_interp[ctx][filter];
#endif  // CONFIG_DUAL_FILTER
  }
}

static void read_intra_block_mode_info(AV1_COMMON *const cm, const int mi_row,
                                       const int mi_col, MACROBLOCKD *const xd,
                                       MODE_INFO *mi, aom_reader *r) {
  MB_MODE_INFO *const mbmi = &mi->mbmi;
  const BLOCK_SIZE bsize = mi->mbmi.sb_type;
  int i;

  mbmi->ref_frame[0] = INTRA_FRAME;
  mbmi->ref_frame[1] = NONE_FRAME;

  FRAME_CONTEXT *ec_ctx = xd->tile_ctx;

#if CONFIG_CB4X4
  (void)i;
  mbmi->mode = read_intra_mode(r, ec_ctx->y_mode_cdf[size_group_lookup[bsize]]);
#else
  switch (bsize) {
    case BLOCK_4X4:
      for (i = 0; i < 4; ++i)
        mi->bmi[i].as_mode = read_intra_mode(r, ec_ctx->y_mode_cdf[0]);
      mbmi->mode = mi->bmi[3].as_mode;
      break;
    case BLOCK_4X8:
      mi->bmi[0].as_mode = mi->bmi[2].as_mode =
          read_intra_mode(r, ec_ctx->y_mode_cdf[0]);
      mi->bmi[1].as_mode = mi->bmi[3].as_mode = mbmi->mode =
          read_intra_mode(r, ec_ctx->y_mode_cdf[0]);
      break;
    case BLOCK_8X4:
      mi->bmi[0].as_mode = mi->bmi[1].as_mode =
          read_intra_mode(r, ec_ctx->y_mode_cdf[0]);
      mi->bmi[2].as_mode = mi->bmi[3].as_mode = mbmi->mode =
          read_intra_mode(r, ec_ctx->y_mode_cdf[0]);
      break;
    default:
      mbmi->mode =
          read_intra_mode(r, ec_ctx->y_mode_cdf[size_group_lookup[bsize]]);
  }
#endif

#if CONFIG_CB4X4
  if (is_chroma_reference(mi_row, mi_col, bsize, xd->plane[1].subsampling_x,
                          xd->plane[1].subsampling_y)) {
    mbmi->uv_mode = read_intra_mode_uv(ec_ctx, r, mbmi->mode);
#else
  mbmi->uv_mode = read_intra_mode_uv(ec_ctx, r, mbmi->mode);
  (void)mi_row;
  (void)mi_col;
#endif

#if CONFIG_CFL
    if (mbmi->uv_mode == UV_CFL_PRED) {
      mbmi->cfl_alpha_idx =
          read_cfl_alphas(xd->tile_ctx, r, &mbmi->cfl_alpha_signs);
      xd->cfl->store_y = 1;
    } else {
      xd->cfl->store_y = 0;
    }
#endif  // CONFIG_CFL

#if CONFIG_CB4X4
  } else {
    // Avoid decoding angle_info if there is is no chroma prediction
    mbmi->uv_mode = UV_DC_PRED;
#if CONFIG_CFL
    xd->cfl->is_chroma_reference = 0;
    xd->cfl->store_y = 1;
#endif
  }
#endif

  // Explicitly ignore cm here to avoid a compile warning if none of
  // ext-intra, palette and filter-intra are enabled.
  (void)cm;

#if CONFIG_EXT_INTRA
  read_intra_angle_info(cm, xd, r);
#endif  // CONFIG_EXT_INTRA
  mbmi->palette_mode_info.palette_size[0] = 0;
  mbmi->palette_mode_info.palette_size[1] = 0;
  if (av1_allow_palette(cm->allow_screen_content_tools, bsize))
    read_palette_mode_info(cm, xd, r);
#if CONFIG_FILTER_INTRA
  mbmi->filter_intra_mode_info.use_filter_intra_mode[0] = 0;
  mbmi->filter_intra_mode_info.use_filter_intra_mode[1] = 0;
  if (bsize >= BLOCK_8X8 || CONFIG_CB4X4)
    read_filter_intra_mode_info(cm, xd, mi_row, mi_col, r);
#endif  // CONFIG_FILTER_INTRA
}

static INLINE int is_mv_valid(const MV *mv) {
  return mv->row > MV_LOW && mv->row < MV_UPP && mv->col > MV_LOW &&
         mv->col < MV_UPP;
}

static INLINE int assign_mv(AV1_COMMON *cm, MACROBLOCKD *xd,
                            PREDICTION_MODE mode,
                            MV_REFERENCE_FRAME ref_frame[2], int block,
                            int_mv mv[2], int_mv ref_mv[2],
                            int_mv nearest_mv[2], int_mv near_mv[2], int mi_row,
                            int mi_col, int is_compound, int allow_hp,
                            aom_reader *r) {
  int i;
  int ret = 1;
  FRAME_CONTEXT *ec_ctx = xd->tile_ctx;
  BLOCK_SIZE bsize = xd->mi[0]->mbmi.sb_type;
  MB_MODE_INFO *mbmi = &xd->mi[0]->mbmi;
#if CONFIG_CB4X4
  int_mv *pred_mv = mbmi->pred_mv;
  (void)block;
#else
  int_mv *pred_mv =
      (bsize >= BLOCK_8X8) ? mbmi->pred_mv : xd->mi[0]->bmi[block].pred_mv;
#endif  // CONFIG_CB4X4
  (void)ref_frame;
  (void)cm;
  (void)mi_row;
  (void)mi_col;
  (void)bsize;
#if CONFIG_AMVR
  if (cm->cur_frame_mv_precision_level) {
    allow_hp = MV_SUBPEL_NONE;
  }
#endif
  switch (mode) {
    case NEWMV: {
      FRAME_COUNTS *counts = xd->counts;
      for (i = 0; i < 1 + is_compound; ++i) {
        int8_t rf_type = av1_ref_frame_type(mbmi->ref_frame);
        int nmv_ctx =
            av1_nmv_ctx(xd->ref_mv_count[rf_type], xd->ref_mv_stack[rf_type], i,
                        mbmi->ref_mv_idx);
        nmv_context *const nmvc = &ec_ctx->nmvc[nmv_ctx];
        nmv_context_counts *const mv_counts =
            counts ? &counts->mv[nmv_ctx] : NULL;
        read_mv(r, &mv[i].as_mv, &ref_mv[i].as_mv, nmvc, mv_counts, allow_hp);
        ret = ret && is_mv_valid(&mv[i].as_mv);

        pred_mv[i].as_int = ref_mv[i].as_int;
      }
      break;
    }
    case NEARESTMV: {
      mv[0].as_int = nearest_mv[0].as_int;
      if (is_compound) mv[1].as_int = nearest_mv[1].as_int;

      pred_mv[0].as_int = nearest_mv[0].as_int;
      if (is_compound) pred_mv[1].as_int = nearest_mv[1].as_int;
      break;
    }
    case NEARMV: {
      mv[0].as_int = near_mv[0].as_int;
      if (is_compound) mv[1].as_int = near_mv[1].as_int;

      pred_mv[0].as_int = near_mv[0].as_int;
      if (is_compound) pred_mv[1].as_int = near_mv[1].as_int;
      break;
    }
    case ZEROMV: {
#if CONFIG_GLOBAL_MOTION
      mv[0].as_int = gm_get_motion_vector(&cm->global_motion[ref_frame[0]],
                                          cm->allow_high_precision_mv, bsize,
                                          mi_col, mi_row, block
#if CONFIG_AMVR
                                          ,
                                          cm->cur_frame_mv_precision_level
#endif
                                          )
                         .as_int;
      if (is_compound)
        mv[1].as_int = gm_get_motion_vector(&cm->global_motion[ref_frame[1]],
                                            cm->allow_high_precision_mv, bsize,
                                            mi_col, mi_row, block
#if CONFIG_AMVR
                                            ,
                                            cm->cur_frame_mv_precision_level
#endif
                                            )
                           .as_int;
#else
      mv[0].as_int = 0;
      if (is_compound) mv[1].as_int = 0;
#endif  // CONFIG_GLOBAL_MOTION

      pred_mv[0].as_int = mv[0].as_int;
      if (is_compound) pred_mv[1].as_int = mv[1].as_int;
      break;
    }
#if CONFIG_COMPOUND_SINGLEREF
    case SR_NEAREST_NEARMV: {
      assert(!is_compound);
      mv[0].as_int = nearest_mv[0].as_int;
      mv[1].as_int = near_mv[0].as_int;
      break;
    }
    /*
    case SR_NEAREST_NEWMV: {
      assert(!is_compound);
      mv[0].as_int = nearest_mv[0].as_int;

      FRAME_COUNTS *counts = xd->counts;
      int8_t rf_type = av1_ref_frame_type(mbmi->ref_frame);
      int nmv_ctx = av1_nmv_ctx(xd->ref_mv_count[rf_type],
                                xd->ref_mv_stack[rf_type], 0, mbmi->ref_mv_idx);
      nmv_context *const nmvc = &ec_ctx->nmvc[nmv_ctx];
      nmv_context_counts *const mv_counts =
          counts ? &counts->mv[nmv_ctx] : NULL;
      read_mv(r, &mv[1].as_mv, &ref_mv[0].as_mv, nmvc, mv_counts, allow_hp);
      ret = ret && is_mv_valid(&mv[1].as_mv);
      break;
    }*/
    case SR_NEAR_NEWMV: {
      assert(!is_compound);
      mv[0].as_int = near_mv[0].as_int;

      FRAME_COUNTS *counts = xd->counts;
      int8_t rf_type = av1_ref_frame_type(mbmi->ref_frame);
      int nmv_ctx = av1_nmv_ctx(xd->ref_mv_count[rf_type],
                                xd->ref_mv_stack[rf_type], 0, mbmi->ref_mv_idx);
      nmv_context *const nmvc = &ec_ctx->nmvc[nmv_ctx];
      nmv_context_counts *const mv_counts =
          counts ? &counts->mv[nmv_ctx] : NULL;
      read_mv(r, &mv[1].as_mv, &ref_mv[0].as_mv, nmvc, mv_counts, allow_hp);
      ret = ret && is_mv_valid(&mv[1].as_mv);
      break;
    }
    case SR_ZERO_NEWMV: {
      assert(!is_compound);
#if CONFIG_GLOBAL_MOTION
      mv[0].as_int = gm_get_motion_vector(&cm->global_motion[ref_frame[0]],
                                          cm->allow_high_precision_mv, bsize,
                                          mi_col, mi_row, block)
                         .as_int;
#else
      mv[0].as_int = 0;
#endif  // CONFIG_GLOBAL_MOTION

      FRAME_COUNTS *counts = xd->counts;
      int8_t rf_type = av1_ref_frame_type(mbmi->ref_frame);
      int nmv_ctx = av1_nmv_ctx(xd->ref_mv_count[rf_type],
                                xd->ref_mv_stack[rf_type], 0, mbmi->ref_mv_idx);
      nmv_context *const nmvc = &ec_ctx->nmvc[nmv_ctx];
      nmv_context_counts *const mv_counts =
          counts ? &counts->mv[nmv_ctx] : NULL;
      read_mv(r, &mv[1].as_mv, &ref_mv[0].as_mv, nmvc, mv_counts, allow_hp);
      ret = ret && is_mv_valid(&mv[1].as_mv);
      break;
    }
    case SR_NEW_NEWMV: {
      assert(!is_compound);

      FRAME_COUNTS *counts = xd->counts;
      for (i = 0; i < 2; ++i) {
        int8_t rf_type = av1_ref_frame_type(mbmi->ref_frame);
        int nmv_ctx =
            av1_nmv_ctx(xd->ref_mv_count[rf_type], xd->ref_mv_stack[rf_type], 0,
                        mbmi->ref_mv_idx);
        nmv_context *const nmvc = &ec_ctx->nmvc[nmv_ctx];
        nmv_context_counts *const mv_counts =
            counts ? &counts->mv[nmv_ctx] : NULL;
        read_mv(r, &mv[i].as_mv, &ref_mv[0].as_mv, nmvc, mv_counts, allow_hp);
        ret = ret && is_mv_valid(&mv[i].as_mv);
      }
      break;
    }
#endif  // CONFIG_COMPOUND_SINGLEREF
    case NEW_NEWMV: {
      FRAME_COUNTS *counts = xd->counts;
      assert(is_compound);
      for (i = 0; i < 2; ++i) {
        int8_t rf_type = av1_ref_frame_type(mbmi->ref_frame);
        int nmv_ctx =
            av1_nmv_ctx(xd->ref_mv_count[rf_type], xd->ref_mv_stack[rf_type], i,
                        mbmi->ref_mv_idx);
        nmv_context *const nmvc = &ec_ctx->nmvc[nmv_ctx];
        nmv_context_counts *const mv_counts =
            counts ? &counts->mv[nmv_ctx] : NULL;
        read_mv(r, &mv[i].as_mv, &ref_mv[i].as_mv, nmvc, mv_counts, allow_hp);
        ret = ret && is_mv_valid(&mv[i].as_mv);
      }
      break;
    }
    case NEAREST_NEARESTMV: {
      assert(is_compound);
      mv[0].as_int = nearest_mv[0].as_int;
      mv[1].as_int = nearest_mv[1].as_int;
      break;
    }
    case NEAR_NEARMV: {
      assert(is_compound);
      mv[0].as_int = near_mv[0].as_int;
      mv[1].as_int = near_mv[1].as_int;
      break;
    }
    case NEW_NEARESTMV: {
      FRAME_COUNTS *counts = xd->counts;
      int8_t rf_type = av1_ref_frame_type(mbmi->ref_frame);
      int nmv_ctx = av1_nmv_ctx(xd->ref_mv_count[rf_type],
                                xd->ref_mv_stack[rf_type], 0, mbmi->ref_mv_idx);
      nmv_context *const nmvc = &ec_ctx->nmvc[nmv_ctx];
      nmv_context_counts *const mv_counts =
          counts ? &counts->mv[nmv_ctx] : NULL;
      read_mv(r, &mv[0].as_mv, &ref_mv[0].as_mv, nmvc, mv_counts, allow_hp);
      assert(is_compound);
      ret = ret && is_mv_valid(&mv[0].as_mv);
      mv[1].as_int = nearest_mv[1].as_int;
      break;
    }
    case NEAREST_NEWMV: {
      FRAME_COUNTS *counts = xd->counts;
      int8_t rf_type = av1_ref_frame_type(mbmi->ref_frame);
      int nmv_ctx = av1_nmv_ctx(xd->ref_mv_count[rf_type],
                                xd->ref_mv_stack[rf_type], 1, mbmi->ref_mv_idx);
      nmv_context_counts *const mv_counts =
          counts ? &counts->mv[nmv_ctx] : NULL;
      nmv_context *const nmvc = &ec_ctx->nmvc[nmv_ctx];
      mv[0].as_int = nearest_mv[0].as_int;
      read_mv(r, &mv[1].as_mv, &ref_mv[1].as_mv, nmvc, mv_counts, allow_hp);
      assert(is_compound);
      ret = ret && is_mv_valid(&mv[1].as_mv);
      break;
    }
    case NEAR_NEWMV: {
      FRAME_COUNTS *counts = xd->counts;
      int8_t rf_type = av1_ref_frame_type(mbmi->ref_frame);
      int nmv_ctx = av1_nmv_ctx(xd->ref_mv_count[rf_type],
                                xd->ref_mv_stack[rf_type], 1, mbmi->ref_mv_idx);
      nmv_context *const nmvc = &ec_ctx->nmvc[nmv_ctx];
      nmv_context_counts *const mv_counts =
          counts ? &counts->mv[nmv_ctx] : NULL;
      mv[0].as_int = near_mv[0].as_int;
      read_mv(r, &mv[1].as_mv, &ref_mv[1].as_mv, nmvc, mv_counts, allow_hp);
      assert(is_compound);

      ret = ret && is_mv_valid(&mv[1].as_mv);
      break;
    }
    case NEW_NEARMV: {
      FRAME_COUNTS *counts = xd->counts;
      int8_t rf_type = av1_ref_frame_type(mbmi->ref_frame);
      int nmv_ctx = av1_nmv_ctx(xd->ref_mv_count[rf_type],
                                xd->ref_mv_stack[rf_type], 0, mbmi->ref_mv_idx);
      nmv_context *const nmvc = &ec_ctx->nmvc[nmv_ctx];
      nmv_context_counts *const mv_counts =
          counts ? &counts->mv[nmv_ctx] : NULL;
      read_mv(r, &mv[0].as_mv, &ref_mv[0].as_mv, nmvc, mv_counts, allow_hp);
      assert(is_compound);
      ret = ret && is_mv_valid(&mv[0].as_mv);
      mv[1].as_int = near_mv[1].as_int;
      break;
    }
    case ZERO_ZEROMV: {
      assert(is_compound);
#if CONFIG_GLOBAL_MOTION
      mv[0].as_int = gm_get_motion_vector(&cm->global_motion[ref_frame[0]],
                                          cm->allow_high_precision_mv, bsize,
                                          mi_col, mi_row, block
#if CONFIG_AMVR
                                          ,
                                          cm->cur_frame_mv_precision_level
#endif
                                          )
                         .as_int;
      mv[1].as_int = gm_get_motion_vector(&cm->global_motion[ref_frame[1]],
                                          cm->allow_high_precision_mv, bsize,
                                          mi_col, mi_row, block
#if CONFIG_AMVR
                                          ,
                                          cm->cur_frame_mv_precision_level
#endif
                                          )
                         .as_int;
#else
      mv[0].as_int = 0;
      mv[1].as_int = 0;
#endif  // CONFIG_GLOBAL_MOTION
      break;
    }
    default: { return 0; }
  }
  return ret;
}

static int read_is_inter_block(AV1_COMMON *const cm, MACROBLOCKD *const xd,
                               int segment_id, aom_reader *r) {
  if (segfeature_active(&cm->seg, segment_id, SEG_LVL_REF_FRAME)) {
    return get_segdata(&cm->seg, segment_id, SEG_LVL_REF_FRAME) != INTRA_FRAME;
  } else {
    const int ctx = av1_get_intra_inter_context(xd);
#if CONFIG_NEW_MULTISYMBOL
    FRAME_CONTEXT *ec_ctx = xd->tile_ctx;
    const int is_inter =
        aom_read_symbol(r, ec_ctx->intra_inter_cdf[ctx], 2, ACCT_STR);
#else
    const int is_inter = aom_read(r, cm->fc->intra_inter_prob[ctx], ACCT_STR);
#endif
    FRAME_COUNTS *counts = xd->counts;
    if (counts) ++counts->intra_inter[ctx][is_inter];
    return is_inter;
  }
}

#if CONFIG_COMPOUND_SINGLEREF
static int read_is_inter_singleref_comp_mode(AV1_COMMON *const cm,
                                             MACROBLOCKD *const xd,
                                             int segment_id, aom_reader *r) {
  if (segfeature_active(&cm->seg, segment_id, SEG_LVL_REF_FRAME)) return 0;

  const int ctx = av1_get_inter_mode_context(xd);
  const int is_singleref_comp_mode =
      aom_read(r, cm->fc->comp_inter_mode_prob[ctx], ACCT_STR);
  FRAME_COUNTS *counts = xd->counts;

  if (counts) ++counts->comp_inter_mode[ctx][is_singleref_comp_mode];
  return is_singleref_comp_mode;
}
#endif  // CONFIG_COMPOUND_SINGLEREF

static void fpm_sync(void *const data, int mi_row) {
  AV1Decoder *const pbi = (AV1Decoder *)data;
  av1_frameworker_wait(pbi->frame_worker_owner, pbi->common.prev_frame,
                       mi_row << pbi->common.mib_size_log2);
}

#if DEC_MISMATCH_DEBUG
static void dec_dump_logs(AV1_COMMON *cm, MODE_INFO *const mi, int mi_row,
                          int mi_col,
                          int16_t inter_mode_ctx[MODE_CTX_REF_FRAMES],
                          int16_t mode_ctx) {
  int_mv mv[2] = { { 0 } };
  int ref;
  MB_MODE_INFO *const mbmi = &mi->mbmi;
  for (ref = 0; ref < 1 + has_second_ref(mbmi); ++ref)
    mv[ref].as_mv = mbmi->mv[ref].as_mv;

  const int16_t newmv_ctx = mode_ctx & NEWMV_CTX_MASK;
  int16_t zeromv_ctx = -1;
  int16_t refmv_ctx = -1;
  if (mbmi->mode != NEWMV) {
    if (mode_ctx & (1 << ALL_ZERO_FLAG_OFFSET)) assert(mbmi->mode == ZEROMV);
    zeromv_ctx = (mode_ctx >> ZEROMV_OFFSET) & ZEROMV_CTX_MASK;
    if (mbmi->mode != ZEROMV) {
      refmv_ctx = (mode_ctx >> REFMV_OFFSET) & REFMV_CTX_MASK;
      if (mode_ctx & (1 << SKIP_NEARESTMV_OFFSET)) refmv_ctx = 6;
      if (mode_ctx & (1 << SKIP_NEARMV_OFFSET)) refmv_ctx = 7;
      if (mode_ctx & (1 << SKIP_NEARESTMV_SUB8X8_OFFSET)) refmv_ctx = 8;
    }
  }

  int8_t ref_frame_type = av1_ref_frame_type(mbmi->ref_frame);
#define FRAME_TO_CHECK 1
  if (cm->current_video_frame == FRAME_TO_CHECK && cm->show_frame == 1) {
    printf(
        "=== DECODER ===: "
        "Frame=%d, (mi_row,mi_col)=(%d,%d), mode=%d, bsize=%d, "
        "show_frame=%d, mv[0]=(%d,%d), mv[1]=(%d,%d), ref[0]=%d, "
        "ref[1]=%d, motion_mode=%d, inter_mode_ctx=%d, mode_ctx=%d, "
        "newmv_ctx=%d, zeromv_ctx=%d, refmv_ctx=%d\n",
        cm->current_video_frame, mi_row, mi_col, mbmi->mode, mbmi->sb_type,
        cm->show_frame, mv[0].as_mv.row, mv[0].as_mv.col, mv[1].as_mv.row,
        mv[1].as_mv.col, mbmi->ref_frame[0], mbmi->ref_frame[1],
        mbmi->motion_mode, inter_mode_ctx[ref_frame_type], mode_ctx, newmv_ctx,
        zeromv_ctx, refmv_ctx);
  }
}
#endif  // DEC_MISMATCH_DEBUG

static void read_inter_block_mode_info(AV1Decoder *const pbi,
                                       MACROBLOCKD *const xd,
                                       MODE_INFO *const mi,
#if CONFIG_SUPERTX
                                       int mi_row, int mi_col, aom_reader *r,
                                       int supertx_enabled) {
#else
                                       int mi_row, int mi_col, aom_reader *r) {
#endif  // CONFIG_MOTION_VAR && CONFIG_SUPERTX
  AV1_COMMON *const cm = &pbi->common;
  MB_MODE_INFO *const mbmi = &mi->mbmi;
  const BLOCK_SIZE bsize = mbmi->sb_type;
  const int allow_hp = cm->allow_high_precision_mv;
  const int unify_bsize = CONFIG_CB4X4;
  int_mv nearestmv[2], nearmv[2];
  int_mv ref_mvs[MODE_CTX_REF_FRAMES][MAX_MV_REF_CANDIDATES];
  int ref, is_compound;
#if CONFIG_COMPOUND_SINGLEREF
  int is_singleref_comp_mode = 0;
#endif  // CONFIG_COMPOUND_SINGLEREF
  int16_t inter_mode_ctx[MODE_CTX_REF_FRAMES];
  int16_t compound_inter_mode_ctx[MODE_CTX_REF_FRAMES];
  int16_t mode_ctx = 0;
#if CONFIG_WARPED_MOTION
  int pts[SAMPLES_ARRAY_SIZE], pts_inref[SAMPLES_ARRAY_SIZE];
#if WARPED_MOTION_SORT_SAMPLES
  int pts_mv[SAMPLES_ARRAY_SIZE];
#endif  // WARPED_MOTION_SORT_SAMPLES
#endif  // CONFIG_WARPED_MOTION
  FRAME_CONTEXT *ec_ctx = xd->tile_ctx;

  assert(NELEMENTS(mode_2_counter) == MB_MODE_COUNT);

  mbmi->uv_mode = UV_DC_PRED;
  mbmi->palette_mode_info.palette_size[0] = 0;
  mbmi->palette_mode_info.palette_size[1] = 0;

  memset(ref_mvs, 0, sizeof(ref_mvs));

  read_ref_frames(cm, xd, r, mbmi->segment_id, mbmi->ref_frame);
  is_compound = has_second_ref(mbmi);

#if CONFIG_EXT_COMP_REFS
#if !USE_UNI_COMP_REFS
  // NOTE: uni-directional comp refs disabled
  if (is_compound)
    assert(mbmi->ref_frame[0] < BWDREF_FRAME &&
           mbmi->ref_frame[1] >= BWDREF_FRAME);
#endif  // !USE_UNI_COMP_REFS
#endif  // CONFIG_EXT_COMP_REFS

#if CONFIG_COMPOUND_SINGLEREF
  if (!is_compound)
    is_singleref_comp_mode =
        read_is_inter_singleref_comp_mode(cm, xd, mbmi->segment_id, r);
#endif  // CONFIG_COMPOUND_SINGLEREF

  for (ref = 0; ref < 1 + is_compound; ++ref) {
    MV_REFERENCE_FRAME frame = mbmi->ref_frame[ref];

    av1_find_mv_refs(cm, xd, mi, frame, &xd->ref_mv_count[frame],
                     xd->ref_mv_stack[frame], compound_inter_mode_ctx,
                     ref_mvs[frame], mi_row, mi_col, fpm_sync, (void *)pbi,
                     inter_mode_ctx);
  }

  if (is_compound) {
    MV_REFERENCE_FRAME ref_frame = av1_ref_frame_type(mbmi->ref_frame);
    av1_find_mv_refs(cm, xd, mi, ref_frame, &xd->ref_mv_count[ref_frame],
                     xd->ref_mv_stack[ref_frame], compound_inter_mode_ctx,
                     ref_mvs[ref_frame], mi_row, mi_col, fpm_sync, (void *)pbi,
                     inter_mode_ctx);

    if (xd->ref_mv_count[ref_frame] < 2) {
      MV_REFERENCE_FRAME rf[2];
      int_mv zeromv[2];
      av1_set_ref_frame(rf, ref_frame);
#if CONFIG_GLOBAL_MOTION
      zeromv[0].as_int = gm_get_motion_vector(&cm->global_motion[rf[0]],
                                              cm->allow_high_precision_mv,
                                              bsize, mi_col, mi_row, 0
#if CONFIG_AMVR
                                              ,
                                              cm->cur_frame_mv_precision_level
#endif
                                              )
                             .as_int;
      zeromv[1].as_int =
          (rf[1] != NONE_FRAME)
              ? gm_get_motion_vector(&cm->global_motion[rf[1]],
                                     cm->allow_high_precision_mv, bsize, mi_col,
                                     mi_row, 0
#if CONFIG_AMVR
                                     ,
                                     cm->cur_frame_mv_precision_level
#endif
                                     )
                    .as_int
              : 0;
#else
      zeromv[0].as_int = zeromv[1].as_int = 0;
#endif
      for (ref = 0; ref < 2; ++ref) {
        if (rf[ref] == NONE_FRAME) continue;
#if CONFIG_AMVR
        lower_mv_precision(&ref_mvs[rf[ref]][0].as_mv, allow_hp,
                           cm->cur_frame_mv_precision_level);
        lower_mv_precision(&ref_mvs[rf[ref]][1].as_mv, allow_hp,
                           cm->cur_frame_mv_precision_level);
#else
        lower_mv_precision(&ref_mvs[rf[ref]][0].as_mv, allow_hp);
        lower_mv_precision(&ref_mvs[rf[ref]][1].as_mv, allow_hp);
#endif
        if (ref_mvs[rf[ref]][0].as_int != zeromv[ref].as_int ||
            ref_mvs[rf[ref]][1].as_int != zeromv[ref].as_int)
          inter_mode_ctx[ref_frame] &= ~(1 << ALL_ZERO_FLAG_OFFSET);
      }
    }
  }

#if CONFIG_COMPOUND_SINGLEREF
  if (is_compound || is_singleref_comp_mode)
#else   // !CONFIG_COMPOUND_SINGLEREF
  if (is_compound)
#endif  // CONFIG_COMPOUND_SINGLEREF
    mode_ctx = compound_inter_mode_ctx[mbmi->ref_frame[0]];
  else
    mode_ctx =
        av1_mode_context_analyzer(inter_mode_ctx, mbmi->ref_frame, bsize, -1);
  mbmi->ref_mv_idx = 0;

#if CONFIG_SEGMENT_ZEROMV
  if (segfeature_active(&cm->seg, mbmi->segment_id, SEG_LVL_SKIP) ||
      segfeature_active(&cm->seg, mbmi->segment_id, SEG_LVL_ZEROMV)) {
#else
  if (segfeature_active(&cm->seg, mbmi->segment_id, SEG_LVL_SKIP)) {
#endif
    mbmi->mode = ZEROMV;
    if (bsize < BLOCK_8X8 && !unify_bsize) {
      aom_internal_error(xd->error_info, AOM_CODEC_UNSUP_BITSTREAM,
                         "Invalid usage of segment feature on small blocks");
      return;
    }
  } else {
    if (bsize >= BLOCK_8X8 || unify_bsize) {
      if (is_compound)
        mbmi->mode = read_inter_compound_mode(cm, xd, r, mode_ctx);
#if CONFIG_COMPOUND_SINGLEREF
      else if (is_singleref_comp_mode)
        mbmi->mode = read_inter_singleref_comp_mode(xd, r, mode_ctx);
#endif  // CONFIG_COMPOUND_SINGLEREF
      else
        mbmi->mode = read_inter_mode(ec_ctx, xd, r, mode_ctx);
      if (mbmi->mode == NEWMV || mbmi->mode == NEW_NEWMV ||
#if CONFIG_COMPOUND_SINGLEREF
          mbmi->mode == SR_NEW_NEWMV ||
#endif  // CONFIG_COMPOUND_SINGLEREF
          have_nearmv_in_inter_mode(mbmi->mode))
        read_drl_idx(ec_ctx, xd, mbmi, r);
    }
  }

  if ((bsize < BLOCK_8X8 && !unify_bsize) ||
      (mbmi->mode != ZEROMV && mbmi->mode != ZERO_ZEROMV)) {
    for (ref = 0; ref < 1 + is_compound; ++ref) {
#if CONFIG_AMVR
      av1_find_best_ref_mvs(allow_hp, ref_mvs[mbmi->ref_frame[ref]],
                            &nearestmv[ref], &nearmv[ref],
                            cm->cur_frame_mv_precision_level);
#else
      av1_find_best_ref_mvs(allow_hp, ref_mvs[mbmi->ref_frame[ref]],
                            &nearestmv[ref], &nearmv[ref]);
#endif
    }
  }

#if CONFIG_COMPOUND_SINGLEREF
  if ((is_compound || is_singleref_comp_mode) &&
      (bsize >= BLOCK_8X8 || unify_bsize) && mbmi->mode != ZERO_ZEROMV)
#else   // !CONFIG_COMPOUND_SINGLEREF
  if (is_compound && (bsize >= BLOCK_8X8 || unify_bsize) &&
      mbmi->mode != ZERO_ZEROMV)
#endif  // CONFIG_COMPOUND_SINGLEREF
  {
    uint8_t ref_frame_type = av1_ref_frame_type(mbmi->ref_frame);

    if (xd->ref_mv_count[ref_frame_type] > 0) {
      if (mbmi->mode == NEAREST_NEARESTMV) {
        nearestmv[0] = xd->ref_mv_stack[ref_frame_type][0].this_mv;
        nearestmv[1] = xd->ref_mv_stack[ref_frame_type][0].comp_mv;
#if CONFIG_AMVR
        lower_mv_precision(&nearestmv[0].as_mv, allow_hp,
                           cm->cur_frame_mv_precision_level);
        lower_mv_precision(&nearestmv[1].as_mv, allow_hp,
                           cm->cur_frame_mv_precision_level);
#else
        lower_mv_precision(&nearestmv[0].as_mv, allow_hp);
        lower_mv_precision(&nearestmv[1].as_mv, allow_hp);
#endif
      } else if (mbmi->mode == NEAREST_NEWMV
#if CONFIG_COMPOUND_SINGLEREF
                 || mbmi->mode == SR_NEAREST_NEARMV
// || mbmi->mode == SR_NEAREST_NEWMV
#endif  // CONFIG_COMPOUND_SINGLEREF
                 ) {
        nearestmv[0] = xd->ref_mv_stack[ref_frame_type][0].this_mv;

#if CONFIG_AMVR
        lower_mv_precision(&nearestmv[0].as_mv, allow_hp,
                           cm->cur_frame_mv_precision_level);
#else
        lower_mv_precision(&nearestmv[0].as_mv, allow_hp);
#endif
      } else if (mbmi->mode == NEW_NEARESTMV) {
        nearestmv[1] = xd->ref_mv_stack[ref_frame_type][0].comp_mv;
#if CONFIG_AMVR
        lower_mv_precision(&nearestmv[1].as_mv, allow_hp,
                           cm->cur_frame_mv_precision_level);
#else
        lower_mv_precision(&nearestmv[1].as_mv, allow_hp);
#endif
      }
    }

    if (xd->ref_mv_count[ref_frame_type] > 1) {
      int ref_mv_idx = 1 + mbmi->ref_mv_idx;
#if CONFIG_COMPOUND_SINGLEREF
      if (is_compound) {
#endif  // CONFIG_COMPOUND_SINGLEREF
        if (compound_ref0_mode(mbmi->mode) == NEARMV) {
          nearmv[0] = xd->ref_mv_stack[ref_frame_type][ref_mv_idx].this_mv;
#if CONFIG_AMVR
          lower_mv_precision(&nearmv[0].as_mv, allow_hp,
                             cm->cur_frame_mv_precision_level);
#else
        lower_mv_precision(&nearmv[0].as_mv, allow_hp);
#endif
        }

        if (compound_ref1_mode(mbmi->mode) == NEARMV) {
          nearmv[1] = xd->ref_mv_stack[ref_frame_type][ref_mv_idx].comp_mv;
#if CONFIG_AMVR
          lower_mv_precision(&nearmv[1].as_mv, allow_hp,
                             cm->cur_frame_mv_precision_level);
#else
        lower_mv_precision(&nearmv[1].as_mv, allow_hp);
#endif
        }
#if CONFIG_COMPOUND_SINGLEREF
      } else {
        assert(is_singleref_comp_mode);
        if (compound_ref0_mode(mbmi->mode) == NEARMV ||
            compound_ref1_mode(mbmi->mode) == NEARMV) {
          nearmv[0] = xd->ref_mv_stack[ref_frame_type][ref_mv_idx].this_mv;
          lower_mv_precision(&nearmv[0].as_mv, allow_hp);
        }
      }
#endif  // CONFIG_COMPOUND_SINGLEREF
    }
  } else if (mbmi->ref_mv_idx > 0 && mbmi->mode == NEARMV) {
    int_mv cur_mv =
        xd->ref_mv_stack[mbmi->ref_frame[0]][1 + mbmi->ref_mv_idx].this_mv;
    nearmv[0] = cur_mv;
  }

#if !CONFIG_DUAL_FILTER && !CONFIG_WARPED_MOTION && !CONFIG_GLOBAL_MOTION
  read_mb_interp_filter(cm, xd, mbmi, r);
#endif  // !CONFIG_DUAL_FILTER && !CONFIG_WARPED_MOTION

  if (bsize < BLOCK_8X8 && !unify_bsize) {
    const int num_4x4_w = 1 << xd->bmode_blocks_wl;
    const int num_4x4_h = 1 << xd->bmode_blocks_hl;
    int idx, idy;
    PREDICTION_MODE b_mode;
    int_mv nearest_sub8x8[2], near_sub8x8[2];
    int_mv ref_mv[2][2];
    for (idy = 0; idy < 2; idy += num_4x4_h) {
      for (idx = 0; idx < 2; idx += num_4x4_w) {
        int_mv block[2];
        const int j = idy * 2 + idx;
        int_mv ref_mv_s8[2];
        if (!is_compound)
          mode_ctx = av1_mode_context_analyzer(inter_mode_ctx, mbmi->ref_frame,
                                               bsize, j);
        if (is_compound)
          b_mode = read_inter_compound_mode(cm, xd, r, mode_ctx);
        else
          b_mode = read_inter_mode(ec_ctx, xd, r, mode_ctx);

        if (b_mode != ZEROMV && b_mode != ZERO_ZEROMV) {
          CANDIDATE_MV ref_mv_stack[2][MAX_REF_MV_STACK_SIZE];
          uint8_t ref_mv_count[2];
          for (ref = 0; ref < 1 + is_compound; ++ref) {
            int_mv mv_ref_list[MAX_MV_REF_CANDIDATES];
            av1_update_mv_context(cm, xd, mi, mbmi->ref_frame[ref], mv_ref_list,
                                  j, mi_row, mi_col, NULL);
            av1_append_sub8x8_mvs_for_idx(cm, xd, j, ref, mi_row, mi_col,
                                          ref_mv_stack[ref], &ref_mv_count[ref],
                                          mv_ref_list, &nearest_sub8x8[ref],
                                          &near_sub8x8[ref]);
            if (have_newmv_in_inter_mode(b_mode)) {
              mv_ref_list[0].as_int = nearest_sub8x8[ref].as_int;
              mv_ref_list[1].as_int = near_sub8x8[ref].as_int;
#if CONFIG_AMVR
              av1_find_best_ref_mvs(allow_hp, mv_ref_list, &ref_mv[0][ref],
                                    &ref_mv[1][ref],
                                    cm->cur_frame_mv_precision_level);
#else
              av1_find_best_ref_mvs(allow_hp, mv_ref_list, &ref_mv[0][ref],
                                    &ref_mv[1][ref]);
#endif
            }
          }
        }

        for (ref = 0; ref < 1 + is_compound && b_mode != ZEROMV; ++ref) {
          ref_mv_s8[ref] = nearest_sub8x8[ref];
#if CONFIG_AMVR
          lower_mv_precision(&ref_mv_s8[ref].as_mv, allow_hp,
                             cm->cur_frame_mv_precision_level);
#else
          lower_mv_precision(&ref_mv_s8[ref].as_mv, allow_hp);
#endif
        }
        (void)ref_mv_s8;

        if (!assign_mv(cm, xd, b_mode, mbmi->ref_frame, j, block, ref_mv[0],
                       nearest_sub8x8, near_sub8x8, mi_row, mi_col, is_compound,
                       allow_hp, r)) {
          aom_merge_corrupted_flag(&xd->corrupted, 1);
          break;
        };

        mi->bmi[j].as_mv[0].as_int = block[0].as_int;
        mi->bmi[j].as_mode = b_mode;
        if (is_compound) mi->bmi[j].as_mv[1].as_int = block[1].as_int;

        if (num_4x4_h == 2) mi->bmi[j + 2] = mi->bmi[j];
        if (num_4x4_w == 2) mi->bmi[j + 1] = mi->bmi[j];
      }
    }

    mbmi->pred_mv[0].as_int = mi->bmi[3].pred_mv[0].as_int;
    mbmi->pred_mv[1].as_int = mi->bmi[3].pred_mv[1].as_int;
    mi->mbmi.mode = b_mode;

    mbmi->mv[0].as_int = mi->bmi[3].as_mv[0].as_int;
    mbmi->mv[1].as_int = mi->bmi[3].as_mv[1].as_int;
  } else {
    int_mv ref_mv[2];
    ref_mv[0] = nearestmv[0];
    ref_mv[1] = nearestmv[1];

    if (is_compound) {
      int ref_mv_idx = mbmi->ref_mv_idx;
      // Special case: NEAR_NEWMV and NEW_NEARMV modes use
      // 1 + mbmi->ref_mv_idx (like NEARMV) instead of
      // mbmi->ref_mv_idx (like NEWMV)
      if (mbmi->mode == NEAR_NEWMV || mbmi->mode == NEW_NEARMV)
        ref_mv_idx = 1 + mbmi->ref_mv_idx;

      if (compound_ref0_mode(mbmi->mode) == NEWMV) {
        uint8_t ref_frame_type = av1_ref_frame_type(mbmi->ref_frame);
        if (xd->ref_mv_count[ref_frame_type] > 1) {
          ref_mv[0] = xd->ref_mv_stack[ref_frame_type][ref_mv_idx].this_mv;
          clamp_mv_ref(&ref_mv[0].as_mv, xd->n8_w << MI_SIZE_LOG2,
                       xd->n8_h << MI_SIZE_LOG2, xd);
        }
        nearestmv[0] = ref_mv[0];
      }
      if (compound_ref1_mode(mbmi->mode) == NEWMV) {
        uint8_t ref_frame_type = av1_ref_frame_type(mbmi->ref_frame);
        if (xd->ref_mv_count[ref_frame_type] > 1) {
          ref_mv[1] = xd->ref_mv_stack[ref_frame_type][ref_mv_idx].comp_mv;
          clamp_mv_ref(&ref_mv[1].as_mv, xd->n8_w << MI_SIZE_LOG2,
                       xd->n8_h << MI_SIZE_LOG2, xd);
        }
        nearestmv[1] = ref_mv[1];
      }
#if CONFIG_COMPOUND_SINGLEREF
    } else if (is_singleref_comp_mode) {
      int ref_mv_idx = mbmi->ref_mv_idx;
      // Special case: SR_NEAR_NEWMV use 1 + mbmi->ref_mv_idx (like NEARMV)
      //               instead of mbmi->ref_mv_idx (like NEWMV)
      if (mbmi->mode == SR_NEAR_NEWMV) ref_mv_idx = 1 + mbmi->ref_mv_idx;

      if (compound_ref0_mode(mbmi->mode) == NEWMV ||
          compound_ref1_mode(mbmi->mode) == NEWMV) {
        uint8_t ref_frame_type = av1_ref_frame_type(mbmi->ref_frame);
        if (xd->ref_mv_count[ref_frame_type] > 1) {
          ref_mv[0] = xd->ref_mv_stack[ref_frame_type][ref_mv_idx].this_mv;
          clamp_mv_ref(&ref_mv[0].as_mv, xd->n8_w << MI_SIZE_LOG2,
                       xd->n8_h << MI_SIZE_LOG2, xd);
        }
        // TODO(zoeliu): To further investigate why this would not cause a
        //               mismatch for the mode of SR_NEAREST_NEWMV.
        nearestmv[0] = ref_mv[0];
      }
#endif  // CONFIG_COMPOUND_SINGLEREF
    } else {
      if (mbmi->mode == NEWMV) {
        for (ref = 0; ref < 1 + is_compound; ++ref) {
          uint8_t ref_frame_type = av1_ref_frame_type(mbmi->ref_frame);
          if (xd->ref_mv_count[ref_frame_type] > 1) {
            ref_mv[ref] =
                (ref == 0)
                    ? xd->ref_mv_stack[ref_frame_type][mbmi->ref_mv_idx].this_mv
                    : xd->ref_mv_stack[ref_frame_type][mbmi->ref_mv_idx]
                          .comp_mv;
            clamp_mv_ref(&ref_mv[ref].as_mv, xd->n8_w << MI_SIZE_LOG2,
                         xd->n8_h << MI_SIZE_LOG2, xd);
          }
          nearestmv[ref] = ref_mv[ref];
        }
      }
    }

    int mv_corrupted_flag =
        !assign_mv(cm, xd, mbmi->mode, mbmi->ref_frame, 0, mbmi->mv, ref_mv,
                   nearestmv, nearmv, mi_row, mi_col, is_compound, allow_hp, r);
    aom_merge_corrupted_flag(&xd->corrupted, mv_corrupted_flag);
  }

#if CONFIG_INTERINTRA
  mbmi->use_wedge_interintra = 0;
  if (cm->reference_mode != COMPOUND_REFERENCE &&
#if CONFIG_SUPERTX
      !supertx_enabled &&
#endif
      cm->allow_interintra_compound && is_interintra_allowed(mbmi)) {
    const int bsize_group = size_group_lookup[bsize];
#if CONFIG_NEW_MULTISYMBOL
    const int interintra =
        aom_read_symbol(r, ec_ctx->interintra_cdf[bsize_group], 2, ACCT_STR);
#else
    const int interintra =
        aom_read(r, cm->fc->interintra_prob[bsize_group], ACCT_STR);
#endif
    if (xd->counts) xd->counts->interintra[bsize_group][interintra]++;
    assert(mbmi->ref_frame[1] == NONE_FRAME);
    if (interintra) {
      const INTERINTRA_MODE interintra_mode =
          read_interintra_mode(cm, xd, r, bsize_group);
      mbmi->ref_frame[1] = INTRA_FRAME;
      mbmi->interintra_mode = interintra_mode;
#if CONFIG_EXT_INTRA
      mbmi->angle_delta[0] = 0;
      mbmi->angle_delta[1] = 0;
#if CONFIG_INTRA_INTERP
      mbmi->intra_filter = INTRA_FILTER_LINEAR;
#endif  // CONFIG_INTRA_INTERP
#endif  // CONFIG_EXT_INTRA
#if CONFIG_FILTER_INTRA
      mbmi->filter_intra_mode_info.use_filter_intra_mode[0] = 0;
      mbmi->filter_intra_mode_info.use_filter_intra_mode[1] = 0;
#endif  // CONFIG_FILTER_INTRA
      if (is_interintra_wedge_used(bsize)) {
#if CONFIG_NEW_MULTISYMBOL
        mbmi->use_wedge_interintra = aom_read_symbol(
            r, ec_ctx->wedge_interintra_cdf[bsize], 2, ACCT_STR);
#else
        mbmi->use_wedge_interintra =
            aom_read(r, cm->fc->wedge_interintra_prob[bsize], ACCT_STR);
#endif
        if (xd->counts)
          xd->counts->wedge_interintra[bsize][mbmi->use_wedge_interintra]++;
        if (mbmi->use_wedge_interintra) {
          mbmi->interintra_wedge_index =
              aom_read_literal(r, get_wedge_bits_lookup(bsize), ACCT_STR);
          mbmi->interintra_wedge_sign = 0;
        }
      }
    }
  }
#endif  // CONFIG_INTERINTRA

#if CONFIG_WARPED_MOTION
  for (ref = 0; ref < 1 + has_second_ref(mbmi); ++ref) {
    const MV_REFERENCE_FRAME frame = mbmi->ref_frame[ref];
    RefBuffer *ref_buf = &cm->frame_refs[frame - LAST_FRAME];

    xd->block_refs[ref] = ref_buf;
  }
#endif

#if CONFIG_MOTION_VAR || CONFIG_WARPED_MOTION
  mbmi->motion_mode = SIMPLE_TRANSLATION;
#if CONFIG_WARPED_MOTION
  if (mbmi->sb_type >= BLOCK_8X8 && !has_second_ref(mbmi))
#if WARPED_MOTION_SORT_SAMPLES
    mbmi->num_proj_ref[0] =
        findSamples(cm, xd, mi_row, mi_col, pts, pts_inref, pts_mv);
#else
    mbmi->num_proj_ref[0] = findSamples(cm, xd, mi_row, mi_col, pts, pts_inref);
#endif  // WARPED_MOTION_SORT_SAMPLES
#endif  // CONFIG_WARPED_MOTION
#if CONFIG_MOTION_VAR
  av1_count_overlappable_neighbors(cm, xd, mi_row, mi_col);
#endif

#if CONFIG_SUPERTX
  if (!supertx_enabled) {
#endif  // CONFIG_SUPERTX
    if (mbmi->ref_frame[1] != INTRA_FRAME)
      mbmi->motion_mode = read_motion_mode(cm, xd, mi, r);

#if CONFIG_NCOBMC_ADAPT_WEIGHT
    read_ncobmc_mode(xd, mi, mbmi->ncobmc_mode, r);
#endif

#if CONFIG_COMPOUND_SINGLEREF
    if (is_singleref_comp_mode) assert(mbmi->motion_mode == SIMPLE_TRANSLATION);
#endif  // CONFIG_COMPOUND_SINGLEREF
#if CONFIG_WARPED_MOTION
    if (mbmi->motion_mode == WARPED_CAUSAL) {
      mbmi->wm_params[0].wmtype = DEFAULT_WMTYPE;

#if WARPED_MOTION_SORT_SAMPLES
      if (mbmi->num_proj_ref[0] > 1)
        mbmi->num_proj_ref[0] = sortSamples(pts_mv, &mbmi->mv[0].as_mv, pts,
                                            pts_inref, mbmi->num_proj_ref[0]);
#endif  // WARPED_MOTION_SORT_SAMPLES

      if (find_projection(mbmi->num_proj_ref[0], pts, pts_inref, bsize,
                          mbmi->mv[0].as_mv.row, mbmi->mv[0].as_mv.col,
                          &mbmi->wm_params[0], mi_row, mi_col)) {
        aom_internal_error(&cm->error, AOM_CODEC_ERROR, "Invalid Warped Model");
      }
    }
#endif  // CONFIG_WARPED_MOTION
#if CONFIG_SUPERTX
  }
#endif  // CONFIG_SUPERTX
#endif  // CONFIG_MOTION_VAR || CONFIG_WARPED_MOTION

  mbmi->interinter_compound_type = COMPOUND_AVERAGE;
  if (
#if CONFIG_COMPOUND_SINGLEREF
      is_inter_anyref_comp_mode(mbmi->mode)
#else   // !CONFIG_COMPOUND_SINGLEREF
      cm->reference_mode != SINGLE_REFERENCE &&
      is_inter_compound_mode(mbmi->mode)
#endif  // CONFIG_COMPOUND_SINGLEREF
#if CONFIG_MOTION_VAR || CONFIG_WARPED_MOTION
      && mbmi->motion_mode == SIMPLE_TRANSLATION
#endif  // CONFIG_MOTION_VAR || CONFIG_WARPED_MOTION
      ) {
    if (is_any_masked_compound_used(bsize)) {
#if CONFIG_COMPOUND_SEGMENT || CONFIG_WEDGE
      if (cm->allow_masked_compound) {
#if CONFIG_WEDGE && CONFIG_COMPOUND_SEGMENT
        if (!is_interinter_compound_used(COMPOUND_WEDGE, bsize))
          mbmi->interinter_compound_type =
              aom_read_bit(r, ACCT_STR) ? COMPOUND_AVERAGE : COMPOUND_SEG;
        else
#endif  // CONFIG_WEDGE && CONFIG_COMPOUND_SEGMENT
          mbmi->interinter_compound_type = aom_read_symbol(
              r, ec_ctx->compound_type_cdf[bsize], COMPOUND_TYPES, ACCT_STR);
#if CONFIG_WEDGE
        if (mbmi->interinter_compound_type == COMPOUND_WEDGE) {
          assert(is_interinter_compound_used(COMPOUND_WEDGE, bsize));
          mbmi->wedge_index =
              aom_read_literal(r, get_wedge_bits_lookup(bsize), ACCT_STR);
          mbmi->wedge_sign = aom_read_bit(r, ACCT_STR);
        }
#endif  // CONFIG_WEDGE
#if CONFIG_COMPOUND_SEGMENT
        if (mbmi->interinter_compound_type == COMPOUND_SEG) {
          mbmi->mask_type = aom_read_literal(r, MAX_SEG_MASK_BITS, ACCT_STR);
        }
#endif  // CONFIG_COMPOUND_SEGMENT
      }
#endif  // CONFIG_COMPOUND_SEGMENT || CONFIG_WEDGE
    } else {
      mbmi->interinter_compound_type = COMPOUND_AVERAGE;
    }
    if (xd->counts)
      xd->counts->compound_interinter[bsize][mbmi->interinter_compound_type]++;
  }

#if CONFIG_DUAL_FILTER || CONFIG_WARPED_MOTION || CONFIG_GLOBAL_MOTION
  read_mb_interp_filter(cm, xd, mbmi, r);
#endif  // CONFIG_DUAL_FILTER || CONFIG_WARPED_MOTION

#if DEC_MISMATCH_DEBUG
  dec_dump_logs(cm, mi, mi_row, mi_col, inter_mode_ctx, mode_ctx);
#endif  // DEC_MISMATCH_DEBUG
}

static void read_inter_frame_mode_info(AV1Decoder *const pbi,
                                       MACROBLOCKD *const xd,
#if CONFIG_SUPERTX
                                       int supertx_enabled,
#endif  // CONFIG_SUPERTX
                                       int mi_row, int mi_col, aom_reader *r) {
  AV1_COMMON *const cm = &pbi->common;
  MODE_INFO *const mi = xd->mi[0];
  MB_MODE_INFO *const mbmi = &mi->mbmi;
  int inter_block = 1;
#if CONFIG_VAR_TX
  BLOCK_SIZE bsize = mbmi->sb_type;
#endif  // CONFIG_VAR_TX

  mbmi->mv[0].as_int = 0;
  mbmi->mv[1].as_int = 0;
  mbmi->segment_id = read_inter_segment_id(cm, xd, mi_row, mi_col, r);
#if CONFIG_SUPERTX
  if (!supertx_enabled)
#endif  // CONFIG_SUPERTX
    mbmi->skip = read_skip(cm, xd, mbmi->segment_id, r);

  if (cm->delta_q_present_flag) {
    xd->current_qindex =
        xd->prev_qindex +
        read_delta_qindex(cm, xd, r, mbmi, mi_col, mi_row) * cm->delta_q_res;
    /* Normative: Clamp to [1,MAXQ] to not interfere with lossless mode */
    xd->current_qindex = clamp(xd->current_qindex, 1, MAXQ);
    xd->prev_qindex = xd->current_qindex;
#if CONFIG_EXT_DELTA_Q
    if (cm->delta_lf_present_flag) {
#if CONFIG_LOOPFILTER_LEVEL
      if (cm->delta_lf_multi) {
        for (int lf_id = 0; lf_id < FRAME_LF_COUNT; ++lf_id) {
          mbmi->curr_delta_lf[lf_id] = xd->curr_delta_lf[lf_id] =
              xd->prev_delta_lf[lf_id] +
              read_delta_lflevel(cm, xd, r, lf_id, mbmi, mi_col, mi_row) *
                  cm->delta_lf_res;
          xd->prev_delta_lf[lf_id] = xd->curr_delta_lf[lf_id];
        }
      } else {
        mbmi->current_delta_lf_from_base = xd->current_delta_lf_from_base =
            xd->prev_delta_lf_from_base +
            read_delta_lflevel(cm, xd, r, -1, mbmi, mi_col, mi_row) *
                cm->delta_lf_res;
        xd->prev_delta_lf_from_base = xd->current_delta_lf_from_base;
      }
#else
      const int current_delta_lf_from_base =
          xd->prev_delta_lf_from_base +
          read_delta_lflevel(cm, xd, r, mbmi, mi_col, mi_row) *
              cm->delta_lf_res;
      mbmi->current_delta_lf_from_base = xd->current_delta_lf_from_base =
          clamp(current_delta_lf_from_base, 0, MAX_LOOP_FILTER);
      xd->prev_delta_lf_from_base = xd->current_delta_lf_from_base;
#endif  // CONFIG_LOOPFILTER_LEVEL
    }
#endif
  }

#if CONFIG_SUPERTX
  if (!supertx_enabled) {
#endif  // CONFIG_SUPERTX
    inter_block = read_is_inter_block(cm, xd, mbmi->segment_id, r);

#if CONFIG_VAR_TX
    xd->above_txfm_context =
        cm->above_txfm_context + (mi_col << TX_UNIT_WIDE_LOG2);
    xd->left_txfm_context = xd->left_txfm_context_buffer +
                            ((mi_row & MAX_MIB_MASK) << TX_UNIT_HIGH_LOG2);

    if (cm->tx_mode == TX_MODE_SELECT &&
#if CONFIG_CB4X4
        bsize > BLOCK_4X4 &&
#else
        bsize >= BLOCK_8X8 &&
#endif
        !mbmi->skip && inter_block && !xd->lossless[mbmi->segment_id]) {
      const TX_SIZE max_tx_size = max_txsize_rect_lookup[bsize];
      const int bh = tx_size_high_unit[max_tx_size];
      const int bw = tx_size_wide_unit[max_tx_size];
      const int width = block_size_wide[bsize] >> tx_size_wide_log2[0];
      const int height = block_size_high[bsize] >> tx_size_wide_log2[0];
      int idx, idy;
      int init_depth =
          (height != width) ? RECT_VARTX_DEPTH_INIT : SQR_VARTX_DEPTH_INIT;

      mbmi->min_tx_size = TX_SIZES_ALL;
      for (idy = 0; idy < height; idy += bh)
        for (idx = 0; idx < width; idx += bw)
          read_tx_size_vartx(cm, xd, mbmi, xd->counts, max_tx_size, init_depth,
                             idy, idx, r);
#if CONFIG_RECT_TX_EXT
      if (is_quarter_tx_allowed(xd, mbmi, inter_block) &&
          mbmi->tx_size == max_tx_size) {
        int quarter_tx;

        if (quarter_txsize_lookup[bsize] != max_tx_size) {
#if CONFIG_NEW_MULTISYMBOL
          quarter_tx =
              aom_read_symbol(r, cm->fc->quarter_tx_size_cdf, 2, ACCT_STR);
#else
          quarter_tx = aom_read(r, cm->fc->quarter_tx_size_prob, ACCT_STR);
          if (xd->counts) ++xd->counts->quarter_tx_size[quarter_tx];
#endif
        } else {
          quarter_tx = 1;
        }
        if (quarter_tx) {
          mbmi->tx_size = quarter_txsize_lookup[bsize];
          for (idy = 0; idy < tx_size_high_unit[max_tx_size] / 2; ++idy)
            for (idx = 0; idx < tx_size_wide_unit[max_tx_size] / 2; ++idx)
              mbmi->inter_tx_size[idy][idx] = mbmi->tx_size;
          mbmi->min_tx_size = get_min_tx_size(mbmi->tx_size);
        }
      }
#endif
    } else {
      mbmi->tx_size = read_tx_size(cm, xd, inter_block, !mbmi->skip, r);

      if (inter_block) {
        const int width = block_size_wide[bsize] >> tx_size_wide_log2[0];
        const int height = block_size_high[bsize] >> tx_size_high_log2[0];
        int idx, idy;
        for (idy = 0; idy < height; ++idy)
          for (idx = 0; idx < width; ++idx)
            mbmi->inter_tx_size[idy >> 1][idx >> 1] = mbmi->tx_size;
      }
      mbmi->min_tx_size = get_min_tx_size(mbmi->tx_size);
      set_txfm_ctxs(mbmi->tx_size, xd->n8_w, xd->n8_h, mbmi->skip, xd);
    }
#else
  mbmi->tx_size = read_tx_size(cm, xd, inter_block, !mbmi->skip, r);
#endif  // CONFIG_VAR_TX
#if CONFIG_SUPERTX
  }
#if CONFIG_VAR_TX
  else if (inter_block) {
    const int width = num_4x4_blocks_wide_lookup[bsize];
    const int height = num_4x4_blocks_high_lookup[bsize];
    int idx, idy;
    xd->mi[0]->mbmi.tx_size = xd->supertx_size;
    for (idy = 0; idy < height; ++idy)
      for (idx = 0; idx < width; ++idx)
        xd->mi[0]->mbmi.inter_tx_size[idy >> 1][idx >> 1] = xd->supertx_size;
  }
#endif  // CONFIG_VAR_TX
#endif  // CONFIG_SUPERTX

  if (inter_block)
    read_inter_block_mode_info(pbi, xd,
#if CONFIG_SUPERTX
                               mi, mi_row, mi_col, r, supertx_enabled);
#else
                               mi, mi_row, mi_col, r);
#endif  // CONFIG_MOTION_VAR && CONFIG_SUPERTX
  else
    read_intra_block_mode_info(cm, mi_row, mi_col, xd, mi, r);

#if !CONFIG_TXK_SEL
  av1_read_tx_type(cm, xd,
#if CONFIG_SUPERTX
                   supertx_enabled,
#endif
                   r);
#endif  // !CONFIG_TXK_SEL
}

static void av1_intra_copy_frame_mvs(AV1_COMMON *const cm, int mi_row,
                                     int mi_col, int x_mis, int y_mis) {
#if CONFIG_TMV
  const int frame_mvs_stride = ROUND_POWER_OF_TWO(cm->mi_cols, 1);
  MV_REF *frame_mvs = cm->cur_frame->mvs +
                      ((mi_row & 0xfffe) >> 1) * frame_mvs_stride +
                      ((mi_col & 0xfffe) >> 1);
  x_mis = ROUND_POWER_OF_TWO(x_mis, 1);
  y_mis = ROUND_POWER_OF_TWO(y_mis, 1);
#else
  const int frame_mvs_stride = cm->mi_cols;
  MV_REF *frame_mvs = cm->cur_frame->mvs +
                      (mi_row & 0xfffe) * frame_mvs_stride + (mi_col & 0xfffe);
  x_mis = AOMMAX(x_mis, 2);
  y_mis = AOMMAX(y_mis, 2);
#endif  // CONFIG_TMV
  int w, h;

  for (h = 0; h < y_mis; h++) {
    MV_REF *const frame_mv = frame_mvs + h * frame_mvs_stride;
    for (w = 0; w < x_mis; w++) {
      MV_REF *const mv = frame_mv + w;
      mv->ref_frame[0] = NONE_FRAME;
      mv->ref_frame[1] = NONE_FRAME;
    }
  }
}

void av1_read_mode_info(AV1Decoder *const pbi, MACROBLOCKD *xd,
#if CONFIG_SUPERTX
                        int supertx_enabled,
#endif  // CONFIG_SUPERTX
                        int mi_row, int mi_col, aom_reader *r, int x_mis,
                        int y_mis) {
  AV1_COMMON *const cm = &pbi->common;
  MODE_INFO *const mi = xd->mi[0];
#if CONFIG_INTRABC
  mi->mbmi.use_intrabc = 0;
#endif  // CONFIG_INTRABC

  if (frame_is_intra_only(cm)) {
    read_intra_frame_mode_info(cm, xd, mi_row, mi_col, r);
    av1_intra_copy_frame_mvs(cm, mi_row, mi_col, x_mis, y_mis);
  } else {
    read_inter_frame_mode_info(pbi, xd,
#if CONFIG_SUPERTX
                               supertx_enabled,
#endif  // CONFIG_SUPERTX
                               mi_row, mi_col, r);
    av1_copy_frame_mvs(cm, mi, mi_row, mi_col, x_mis, y_mis);
  }
}
