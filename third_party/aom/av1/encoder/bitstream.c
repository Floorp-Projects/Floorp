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
#include <limits.h>
#include <stdio.h>

#include "aom/aom_encoder.h"
#include "aom_dsp/aom_dsp_common.h"
#include "aom_dsp/binary_codes_writer.h"
#include "aom_dsp/bitwriter_buffer.h"
#include "aom_mem/aom_mem.h"
#include "aom_ports/mem_ops.h"
#include "aom_ports/system_state.h"
#if CONFIG_BITSTREAM_DEBUG
#include "aom_util/debug_util.h"
#endif  // CONFIG_BITSTREAM_DEBUG

#if CONFIG_CDEF
#include "av1/common/cdef.h"
#endif  // CONFIG_CDEF
#include "av1/common/entropy.h"
#include "av1/common/entropymode.h"
#include "av1/common/entropymv.h"
#include "av1/common/mvref_common.h"
#include "av1/common/odintrin.h"
#include "av1/common/pred_common.h"
#include "av1/common/reconinter.h"
#if CONFIG_EXT_INTRA
#include "av1/common/reconintra.h"
#endif  // CONFIG_EXT_INTRA
#include "av1/common/seg_common.h"
#include "av1/common/tile_common.h"

#if CONFIG_LV_MAP
#include "av1/encoder/encodetxb.h"
#endif  // CONFIG_LV_MAP
#include "av1/encoder/bitstream.h"
#include "av1/encoder/cost.h"
#include "av1/encoder/encodemv.h"
#include "av1/encoder/mcomp.h"
#if CONFIG_PALETTE_DELTA_ENCODING
#include "av1/encoder/palette.h"
#endif  // CONFIG_PALETTE_DELTA_ENCODING
#include "av1/encoder/segmentation.h"
#include "av1/encoder/subexp.h"
#include "av1/encoder/tokenize.h"
#if CONFIG_PVQ
#include "av1/encoder/pvq_encoder.h"
#endif

#define ENC_MISMATCH_DEBUG 0

#if CONFIG_COMPOUND_SINGLEREF
static struct av1_token
    inter_singleref_comp_mode_encodings[INTER_SINGLEREF_COMP_MODES];
#endif  // CONFIG_COMPOUND_SINGLEREF

// TODO(anybody) : remove this flag when PVQ supports pallete coding tool
#if !CONFIG_PVQ || CONFIG_EXT_INTRA
static INLINE void write_uniform(aom_writer *w, int n, int v) {
  const int l = get_unsigned_bits(n);
  const int m = (1 << l) - n;
  if (l == 0) return;
  if (v < m) {
    aom_write_literal(w, v, l - 1);
  } else {
    aom_write_literal(w, m + ((v - m) >> 1), l - 1);
    aom_write_literal(w, (v - m) & 1, 1);
  }
}
#endif  // !CONFIG_PVQ || CONFIG_EXT_INTRA

#if CONFIG_EXT_INTRA
#if CONFIG_INTRA_INTERP
static struct av1_token intra_filter_encodings[INTRA_FILTERS];
#endif  // CONFIG_INTRA_INTERP
#endif  // CONFIG_EXT_INTRA
#if CONFIG_INTERINTRA
static struct av1_token interintra_mode_encodings[INTERINTRA_MODES];
#endif
#if CONFIG_COMPOUND_SEGMENT || CONFIG_WEDGE
static struct av1_token compound_type_encodings[COMPOUND_TYPES];
#endif  // CONFIG_COMPOUND_SEGMENT || CONFIG_WEDGE
#if CONFIG_LOOP_RESTORATION
static struct av1_token switchable_restore_encodings[RESTORE_SWITCHABLE_TYPES];
static void loop_restoration_write_sb_coeffs(const AV1_COMMON *const cm,
                                             MACROBLOCKD *xd,
                                             aom_writer *const w, int plane,
                                             int rtile_idx);
#endif  // CONFIG_LOOP_RESTORATION
#if CONFIG_OBU
static void write_uncompressed_header_obu(AV1_COMP *cpi,
                                          struct aom_write_bit_buffer *wb);
#else
static void write_uncompressed_header_frame(AV1_COMP *cpi,
                                            struct aom_write_bit_buffer *wb);
#endif

static uint32_t write_compressed_header(AV1_COMP *cpi, uint8_t *data);

#if !CONFIG_OBU || CONFIG_EXT_TILE
static int remux_tiles(const AV1_COMMON *const cm, uint8_t *dst,
                       const uint32_t data_size, const uint32_t max_tile_size,
                       const uint32_t max_tile_col_size,
                       int *const tile_size_bytes,
                       int *const tile_col_size_bytes);
#endif
void av1_encode_token_init(void) {
#if CONFIG_EXT_INTRA && CONFIG_INTRA_INTERP
  av1_tokens_from_tree(intra_filter_encodings, av1_intra_filter_tree);
#endif  // CONFIG_EXT_INTRA && CONFIG_INTRA_INTERP
#if CONFIG_INTERINTRA
  av1_tokens_from_tree(interintra_mode_encodings, av1_interintra_mode_tree);
#endif  // CONFIG_INTERINTRA
#if CONFIG_COMPOUND_SINGLEREF
  av1_tokens_from_tree(inter_singleref_comp_mode_encodings,
                       av1_inter_singleref_comp_mode_tree);
#endif  // CONFIG_COMPOUND_SINGLEREF
#if CONFIG_COMPOUND_SEGMENT || CONFIG_WEDGE
  av1_tokens_from_tree(compound_type_encodings, av1_compound_type_tree);
#endif  // CONFIG_COMPOUND_SEGMENT || CONFIG_WEDGE
#if CONFIG_LOOP_RESTORATION
  av1_tokens_from_tree(switchable_restore_encodings,
                       av1_switchable_restore_tree);
#endif  // CONFIG_LOOP_RESTORATION
}

static void write_intra_mode_kf(const AV1_COMMON *cm, FRAME_CONTEXT *frame_ctx,
                                const MODE_INFO *mi, const MODE_INFO *above_mi,
                                const MODE_INFO *left_mi, int block,
                                PREDICTION_MODE mode, aom_writer *w) {
#if CONFIG_INTRABC
  assert(!is_intrabc_block(&mi->mbmi));
#endif  // CONFIG_INTRABC
  aom_write_symbol(w, mode,
                   get_y_mode_cdf(frame_ctx, mi, above_mi, left_mi, block),
                   INTRA_MODES);
  (void)cm;
}

static void write_inter_mode(aom_writer *w, PREDICTION_MODE mode,
                             FRAME_CONTEXT *ec_ctx, const int16_t mode_ctx) {
  const int16_t newmv_ctx = mode_ctx & NEWMV_CTX_MASK;

#if CONFIG_NEW_MULTISYMBOL
  aom_write_symbol(w, mode != NEWMV, ec_ctx->newmv_cdf[newmv_ctx], 2);
#else
  aom_write(w, mode != NEWMV, ec_ctx->newmv_prob[newmv_ctx]);
#endif

  if (mode != NEWMV) {
    if (mode_ctx & (1 << ALL_ZERO_FLAG_OFFSET)) {
      assert(mode == ZEROMV);
      return;
    }

    const int16_t zeromv_ctx = (mode_ctx >> ZEROMV_OFFSET) & ZEROMV_CTX_MASK;
#if CONFIG_NEW_MULTISYMBOL
    aom_write_symbol(w, mode != ZEROMV, ec_ctx->zeromv_cdf[zeromv_ctx], 2);
#else
    aom_write(w, mode != ZEROMV, ec_ctx->zeromv_prob[zeromv_ctx]);
#endif

    if (mode != ZEROMV) {
      int16_t refmv_ctx = (mode_ctx >> REFMV_OFFSET) & REFMV_CTX_MASK;

      if (mode_ctx & (1 << SKIP_NEARESTMV_OFFSET)) refmv_ctx = 6;
      if (mode_ctx & (1 << SKIP_NEARMV_OFFSET)) refmv_ctx = 7;
      if (mode_ctx & (1 << SKIP_NEARESTMV_SUB8X8_OFFSET)) refmv_ctx = 8;
#if CONFIG_NEW_MULTISYMBOL
      aom_write_symbol(w, mode != NEARESTMV, ec_ctx->refmv_cdf[refmv_ctx], 2);
#else
      aom_write(w, mode != NEARESTMV, ec_ctx->refmv_prob[refmv_ctx]);
#endif
    }
  }
}

static void write_drl_idx(FRAME_CONTEXT *ec_ctx, const MB_MODE_INFO *mbmi,
                          const MB_MODE_INFO_EXT *mbmi_ext, aom_writer *w) {
  uint8_t ref_frame_type = av1_ref_frame_type(mbmi->ref_frame);

  assert(mbmi->ref_mv_idx < 3);

#if CONFIG_COMPOUND_SINGLEREF
  if (mbmi->mode == NEWMV || mbmi->mode == NEW_NEWMV ||
      mbmi->mode == SR_NEW_NEWMV) {
#else   // !CONFIG_COMPOUND_SINGLEREF
  if (mbmi->mode == NEWMV || mbmi->mode == NEW_NEWMV) {
#endif  // CONFIG_COMPOUND_SINGLEREF
    int idx;
    for (idx = 0; idx < 2; ++idx) {
      if (mbmi_ext->ref_mv_count[ref_frame_type] > idx + 1) {
        uint8_t drl_ctx =
            av1_drl_ctx(mbmi_ext->ref_mv_stack[ref_frame_type], idx);

#if CONFIG_NEW_MULTISYMBOL
        aom_write_symbol(w, mbmi->ref_mv_idx != idx, ec_ctx->drl_cdf[drl_ctx],
                         2);
#else
        aom_write(w, mbmi->ref_mv_idx != idx, ec_ctx->drl_prob[drl_ctx]);
#endif
        if (mbmi->ref_mv_idx == idx) return;
      }
    }
    return;
  }

  if (have_nearmv_in_inter_mode(mbmi->mode)) {
    int idx;
    // TODO(jingning): Temporary solution to compensate the NEARESTMV offset.
    for (idx = 1; idx < 3; ++idx) {
      if (mbmi_ext->ref_mv_count[ref_frame_type] > idx + 1) {
        uint8_t drl_ctx =
            av1_drl_ctx(mbmi_ext->ref_mv_stack[ref_frame_type], idx);
#if CONFIG_NEW_MULTISYMBOL
        aom_write_symbol(w, mbmi->ref_mv_idx != (idx - 1),
                         ec_ctx->drl_cdf[drl_ctx], 2);
#else
        aom_write(w, mbmi->ref_mv_idx != (idx - 1), ec_ctx->drl_prob[drl_ctx]);
#endif
        if (mbmi->ref_mv_idx == (idx - 1)) return;
      }
    }
    return;
  }
}

static void write_inter_compound_mode(AV1_COMMON *cm, MACROBLOCKD *xd,
                                      aom_writer *w, PREDICTION_MODE mode,
                                      const int16_t mode_ctx) {
  assert(is_inter_compound_mode(mode));
  (void)cm;
  aom_write_symbol(w, INTER_COMPOUND_OFFSET(mode),
                   xd->tile_ctx->inter_compound_mode_cdf[mode_ctx],
                   INTER_COMPOUND_MODES);
}

#if CONFIG_COMPOUND_SINGLEREF
static void write_inter_singleref_comp_mode(MACROBLOCKD *xd, aom_writer *w,
                                            PREDICTION_MODE mode,
                                            const int16_t mode_ctx) {
  assert(is_inter_singleref_comp_mode(mode));
  aom_cdf_prob *const inter_singleref_comp_cdf =
      xd->tile_ctx->inter_singleref_comp_mode_cdf[mode_ctx];

  aom_write_symbol(w, INTER_SINGLEREF_COMP_OFFSET(mode),
                   inter_singleref_comp_cdf, INTER_SINGLEREF_COMP_MODES);
}
#endif  // CONFIG_COMPOUND_SINGLEREF

static void encode_unsigned_max(struct aom_write_bit_buffer *wb, int data,
                                int max) {
  aom_wb_write_literal(wb, data, get_unsigned_bits(max));
}

#if CONFIG_VAR_TX
static void write_tx_size_vartx(const AV1_COMMON *cm, MACROBLOCKD *xd,
                                const MB_MODE_INFO *mbmi, TX_SIZE tx_size,
                                int depth, int blk_row, int blk_col,
                                aom_writer *w) {
#if CONFIG_NEW_MULTISYMBOL
  FRAME_CONTEXT *ec_ctx = xd->tile_ctx;
  (void)cm;
#endif
  const int tx_row = blk_row >> 1;
  const int tx_col = blk_col >> 1;
  const int max_blocks_high = max_block_high(xd, mbmi->sb_type, 0);
  const int max_blocks_wide = max_block_wide(xd, mbmi->sb_type, 0);

  int ctx = txfm_partition_context(xd->above_txfm_context + blk_col,
                                   xd->left_txfm_context + blk_row,
                                   mbmi->sb_type, tx_size);

  if (blk_row >= max_blocks_high || blk_col >= max_blocks_wide) return;

  if (depth == MAX_VARTX_DEPTH) {
    txfm_partition_update(xd->above_txfm_context + blk_col,
                          xd->left_txfm_context + blk_row, tx_size, tx_size);
    return;
  }

#if CONFIG_RECT_TX_EXT
  if (tx_size == mbmi->inter_tx_size[tx_row][tx_col] ||
      mbmi->tx_size == quarter_txsize_lookup[mbmi->sb_type]) {
#else
  if (tx_size == mbmi->inter_tx_size[tx_row][tx_col]) {
#endif
#if CONFIG_NEW_MULTISYMBOL
    aom_write_symbol(w, 0, ec_ctx->txfm_partition_cdf[ctx], 2);
#else
    aom_write(w, 0, cm->fc->txfm_partition_prob[ctx]);
#endif

    txfm_partition_update(xd->above_txfm_context + blk_col,
                          xd->left_txfm_context + blk_row, tx_size, tx_size);
    // TODO(yuec): set correct txfm partition update for qttx
  } else {
    const TX_SIZE sub_txs = sub_tx_size_map[tx_size];
    const int bsl = tx_size_wide_unit[sub_txs];
    int i;

#if CONFIG_NEW_MULTISYMBOL
    aom_write_symbol(w, 1, ec_ctx->txfm_partition_cdf[ctx], 2);
#else
    aom_write(w, 1, cm->fc->txfm_partition_prob[ctx]);
#endif

    if (sub_txs == TX_4X4) {
      txfm_partition_update(xd->above_txfm_context + blk_col,
                            xd->left_txfm_context + blk_row, sub_txs, tx_size);
      return;
    }

    assert(bsl > 0);
    for (i = 0; i < 4; ++i) {
      int offsetr = blk_row + (i >> 1) * bsl;
      int offsetc = blk_col + (i & 0x01) * bsl;
      write_tx_size_vartx(cm, xd, mbmi, sub_txs, depth + 1, offsetr, offsetc,
                          w);
    }
  }
}

#if !CONFIG_NEW_MULTISYMBOL
static void update_txfm_partition_probs(AV1_COMMON *cm, aom_writer *w,
                                        FRAME_COUNTS *counts, int probwt) {
  int k;
  for (k = 0; k < TXFM_PARTITION_CONTEXTS; ++k)
    av1_cond_prob_diff_update(w, &cm->fc->txfm_partition_prob[k],
                              counts->txfm_partition[k], probwt);
}
#endif  // CONFIG_NEW_MULTISYMBOL
#endif  // CONFIG_VAR_TX

static void write_selected_tx_size(const AV1_COMMON *cm, const MACROBLOCKD *xd,
                                   aom_writer *w) {
  const MB_MODE_INFO *const mbmi = &xd->mi[0]->mbmi;
  const BLOCK_SIZE bsize = mbmi->sb_type;
  FRAME_CONTEXT *ec_ctx = xd->tile_ctx;
  (void)cm;
  if (block_signals_txsize(bsize)) {
    const TX_SIZE tx_size = mbmi->tx_size;
    const int is_inter = is_inter_block(mbmi);
    const int tx_size_ctx = get_tx_size_context(xd);
    const int32_t tx_size_cat = is_inter ? inter_tx_size_cat_lookup[bsize]
                                         : intra_tx_size_cat_lookup[bsize];
    const TX_SIZE coded_tx_size = txsize_sqr_up_map[tx_size];
    const int depth = tx_size_to_depth(coded_tx_size);
#if CONFIG_EXT_TX && CONFIG_RECT_TX
    assert(IMPLIES(is_rect_tx(tx_size), is_rect_tx_allowed(xd, mbmi)));
#endif  // CONFIG_EXT_TX && CONFIG_RECT_TX

    aom_write_symbol(w, depth, ec_ctx->tx_size_cdf[tx_size_cat][tx_size_ctx],
                     tx_size_cat + 2);
#if CONFIG_RECT_TX_EXT && (CONFIG_EXT_TX || CONFIG_VAR_TX)
    if (is_quarter_tx_allowed(xd, mbmi, is_inter) && tx_size != coded_tx_size)
#if CONFIG_NEW_MULTISYMBOL
      aom_write_symbol(w, tx_size == quarter_txsize_lookup[bsize],
                       cm->fc->quarter_tx_size_cdf, 2);
#else
      aom_write(w, tx_size == quarter_txsize_lookup[bsize],
                cm->fc->quarter_tx_size_prob);
#endif
#endif
  }
}

#if !CONFIG_NEW_MULTISYMBOL
static void update_inter_mode_probs(AV1_COMMON *cm, aom_writer *w,
                                    FRAME_COUNTS *counts) {
  int i;
  const int probwt = cm->num_tg;
  for (i = 0; i < NEWMV_MODE_CONTEXTS; ++i)
    av1_cond_prob_diff_update(w, &cm->fc->newmv_prob[i], counts->newmv_mode[i],
                              probwt);
  for (i = 0; i < ZEROMV_MODE_CONTEXTS; ++i)
    av1_cond_prob_diff_update(w, &cm->fc->zeromv_prob[i],
                              counts->zeromv_mode[i], probwt);
  for (i = 0; i < REFMV_MODE_CONTEXTS; ++i)
    av1_cond_prob_diff_update(w, &cm->fc->refmv_prob[i], counts->refmv_mode[i],
                              probwt);
  for (i = 0; i < DRL_MODE_CONTEXTS; ++i)
    av1_cond_prob_diff_update(w, &cm->fc->drl_prob[i], counts->drl_mode[i],
                              probwt);
}
#endif

static int write_skip(const AV1_COMMON *cm, const MACROBLOCKD *xd,
                      int segment_id, const MODE_INFO *mi, aom_writer *w) {
  if (segfeature_active(&cm->seg, segment_id, SEG_LVL_SKIP)) {
    return 1;
  } else {
    const int skip = mi->mbmi.skip;
#if CONFIG_NEW_MULTISYMBOL
    FRAME_CONTEXT *ec_ctx = xd->tile_ctx;
    const int ctx = av1_get_skip_context(xd);
    aom_write_symbol(w, skip, ec_ctx->skip_cdfs[ctx], 2);
#else
    aom_write(w, skip, av1_get_skip_prob(cm, xd));
#endif
    return skip;
  }
}

static void write_is_inter(const AV1_COMMON *cm, const MACROBLOCKD *xd,
                           int segment_id, aom_writer *w, const int is_inter) {
  if (!segfeature_active(&cm->seg, segment_id, SEG_LVL_REF_FRAME)) {
#if CONFIG_NEW_MULTISYMBOL
    FRAME_CONTEXT *ec_ctx = xd->tile_ctx;
    const int ctx = av1_get_intra_inter_context(xd);
    aom_write_symbol(w, is_inter, ec_ctx->intra_inter_cdf[ctx], 2);
#else
    aom_write(w, is_inter, av1_get_intra_inter_prob(cm, xd));
#endif
  }
}

#if CONFIG_MOTION_VAR || CONFIG_WARPED_MOTION
static void write_motion_mode(const AV1_COMMON *cm, MACROBLOCKD *xd,
                              const MODE_INFO *mi, aom_writer *w) {
  const MB_MODE_INFO *mbmi = &mi->mbmi;

#if !CONFIG_GLOBAL_MOTION
  // The cm parameter is only used with global_motion or with
  // motion_var and warped_motion. In other cases, explicitly ignore
  // it to avoid a compiler warning.
  (void)cm;
#endif
  MOTION_MODE last_motion_mode_allowed = motion_mode_allowed(
#if CONFIG_GLOBAL_MOTION
      0, cm->global_motion,
#endif  // CONFIG_GLOBAL_MOTION
#if CONFIG_WARPED_MOTION
      xd,
#endif
      mi);
  if (last_motion_mode_allowed == SIMPLE_TRANSLATION) return;
#if CONFIG_MOTION_VAR && CONFIG_WARPED_MOTION
#if CONFIG_NCOBMC_ADAPT_WEIGHT
  if (last_motion_mode_allowed == NCOBMC_ADAPT_WEIGHT) {
    aom_write_symbol(w, mbmi->motion_mode,
                     xd->tile_ctx->ncobmc_cdf[mbmi->sb_type],
                     OBMC_FAMILY_MODES);
  } else if (last_motion_mode_allowed == OBMC_CAUSAL) {
    aom_write_symbol(w, mbmi->motion_mode == OBMC_CAUSAL,
                     xd->tile_ctx->obmc_cdf[mbmi->sb_type], 2);
  } else {
#else
  if (last_motion_mode_allowed == OBMC_CAUSAL) {
#if CONFIG_NEW_MULTISYMBOL
    aom_write_symbol(w, mbmi->motion_mode == OBMC_CAUSAL,
                     xd->tile_ctx->obmc_cdf[mbmi->sb_type], 2);
#else
    aom_write(w, mbmi->motion_mode == OBMC_CAUSAL,
              cm->fc->obmc_prob[mbmi->sb_type]);
#endif
  } else {
#endif  // CONFIG_NCOBMC_ADAPT_WEIGHT
#endif  // CONFIG_MOTION_VAR && CONFIG_WARPED_MOTION
    aom_write_symbol(w, mbmi->motion_mode,
                     xd->tile_ctx->motion_mode_cdf[mbmi->sb_type],
                     MOTION_MODES);
#if CONFIG_MOTION_VAR && CONFIG_WARPED_MOTION
  }
#endif  // CONFIG_MOTION_VAR && CONFIG_WARPED_MOTION
}

#if CONFIG_NCOBMC_ADAPT_WEIGHT
static void write_ncobmc_mode(MACROBLOCKD *xd, const MODE_INFO *mi,
                              aom_writer *w) {
  const MB_MODE_INFO *mbmi = &mi->mbmi;
  ADAPT_OVERLAP_BLOCK ao_block = adapt_overlap_block_lookup[mbmi->sb_type];
  if (mbmi->motion_mode != NCOBMC_ADAPT_WEIGHT) return;

  aom_write_symbol(w, mbmi->ncobmc_mode[0],
                   xd->tile_ctx->ncobmc_mode_cdf[ao_block], MAX_NCOBMC_MODES);
  if (mi_size_wide[mbmi->sb_type] != mi_size_high[mbmi->sb_type]) {
    aom_write_symbol(w, mbmi->ncobmc_mode[1],
                     xd->tile_ctx->ncobmc_mode_cdf[ao_block], MAX_NCOBMC_MODES);
  }
}
#endif
#endif  // CONFIG_MOTION_VAR || CONFIG_WARPED_MOTION

static void write_delta_qindex(const AV1_COMMON *cm, const MACROBLOCKD *xd,
                               int delta_qindex, aom_writer *w) {
  int sign = delta_qindex < 0;
  int abs = sign ? -delta_qindex : delta_qindex;
  int rem_bits, thr;
  int smallval = abs < DELTA_Q_SMALL ? 1 : 0;
  FRAME_CONTEXT *ec_ctx = xd->tile_ctx;
  (void)cm;

  aom_write_symbol(w, AOMMIN(abs, DELTA_Q_SMALL), ec_ctx->delta_q_cdf,
                   DELTA_Q_PROBS + 1);

  if (!smallval) {
    rem_bits = OD_ILOG_NZ(abs - 1) - 1;
    thr = (1 << rem_bits) + 1;
    aom_write_literal(w, rem_bits - 1, 3);
    aom_write_literal(w, abs - thr, rem_bits);
  }
  if (abs > 0) {
    aom_write_bit(w, sign);
  }
}

#if CONFIG_EXT_DELTA_Q
static void write_delta_lflevel(const AV1_COMMON *cm, const MACROBLOCKD *xd,
#if CONFIG_LOOPFILTER_LEVEL
                                int lf_id,
#endif
                                int delta_lflevel, aom_writer *w) {
  int sign = delta_lflevel < 0;
  int abs = sign ? -delta_lflevel : delta_lflevel;
  int rem_bits, thr;
  int smallval = abs < DELTA_LF_SMALL ? 1 : 0;
  FRAME_CONTEXT *ec_ctx = xd->tile_ctx;
  (void)cm;

#if CONFIG_LOOPFILTER_LEVEL
  if (cm->delta_lf_multi) {
    assert(lf_id >= 0 && lf_id < FRAME_LF_COUNT);
    aom_write_symbol(w, AOMMIN(abs, DELTA_LF_SMALL),
                     ec_ctx->delta_lf_multi_cdf[lf_id], DELTA_LF_PROBS + 1);
  } else {
    aom_write_symbol(w, AOMMIN(abs, DELTA_LF_SMALL), ec_ctx->delta_lf_cdf,
                     DELTA_LF_PROBS + 1);
  }
#else
  aom_write_symbol(w, AOMMIN(abs, DELTA_LF_SMALL), ec_ctx->delta_lf_cdf,
                   DELTA_LF_PROBS + 1);
#endif  // CONFIG_LOOPFILTER_LEVEL

  if (!smallval) {
    rem_bits = OD_ILOG_NZ(abs - 1) - 1;
    thr = (1 << rem_bits) + 1;
    aom_write_literal(w, rem_bits - 1, 3);
    aom_write_literal(w, abs - thr, rem_bits);
  }
  if (abs > 0) {
    aom_write_bit(w, sign);
  }
}
#endif  // CONFIG_EXT_DELTA_Q

#if !CONFIG_NEW_MULTISYMBOL
static void update_skip_probs(AV1_COMMON *cm, aom_writer *w,
                              FRAME_COUNTS *counts) {
  int k;
  const int probwt = cm->num_tg;
  for (k = 0; k < SKIP_CONTEXTS; ++k) {
    av1_cond_prob_diff_update(w, &cm->fc->skip_probs[k], counts->skip[k],
                              probwt);
  }
}
#endif

// TODO(anybody) : remove this flag when PVQ supports pallete coding tool
#if !CONFIG_PVQ
static void pack_map_tokens(aom_writer *w, const TOKENEXTRA **tp, int n,
                            int num) {
  const TOKENEXTRA *p = *tp;
  write_uniform(w, n, p->token);  // The first color index.
  ++p;
  --num;
  for (int i = 0; i < num; ++i) {
    aom_write_symbol(w, p->token, p->color_map_cdf, n);
    ++p;
  }
  *tp = p;
}
#endif  // !CONFIG_PVQ

#if !CONFIG_PVQ
#if CONFIG_SUPERTX
static void update_supertx_probs(AV1_COMMON *cm, int probwt, aom_writer *w) {
  const int savings_thresh = av1_cost_one(GROUP_DIFF_UPDATE_PROB) -
                             av1_cost_zero(GROUP_DIFF_UPDATE_PROB);
  int i, j;
  int savings = 0;
  int do_update = 0;
  for (i = 0; i < PARTITION_SUPERTX_CONTEXTS; ++i) {
    for (j = TX_8X8; j < TX_SIZES; ++j) {
      savings += av1_cond_prob_diff_update_savings(
          &cm->fc->supertx_prob[i][j], cm->counts.supertx[i][j], probwt);
    }
  }
  do_update = savings > savings_thresh;
  aom_write(w, do_update, GROUP_DIFF_UPDATE_PROB);
  if (do_update) {
    for (i = 0; i < PARTITION_SUPERTX_CONTEXTS; ++i) {
      for (j = TX_8X8; j < TX_SIZES; ++j) {
        av1_cond_prob_diff_update(w, &cm->fc->supertx_prob[i][j],
                                  cm->counts.supertx[i][j], probwt);
      }
    }
  }
}
#endif  // CONFIG_SUPERTX

#if !CONFIG_LV_MAP
#if CONFIG_NEW_MULTISYMBOL
static INLINE void write_coeff_extra(const aom_cdf_prob *const *cdf, int val,
                                     int n, aom_writer *w) {
  // Code the extra bits from LSB to MSB in groups of 4
  int i = 0;
  int count = 0;
  while (count < n) {
    const int size = AOMMIN(n - count, 4);
    const int mask = (1 << size) - 1;
    aom_write_cdf(w, val & mask, cdf[i++], 1 << size);
    val >>= size;
    count += size;
  }
}
#else
static INLINE void write_coeff_extra(const aom_prob *pb, int value,
                                     int num_bits, int skip_bits, aom_writer *w,
                                     TOKEN_STATS *token_stats) {
  // Code the extra bits from MSB to LSB 1 bit at a time
  int index;
  for (index = skip_bits; index < num_bits; ++index) {
    const int shift = num_bits - index - 1;
    const int bb = (value >> shift) & 1;
    aom_write_record(w, bb, pb[index], token_stats);
  }
}
#endif  // CONFIG_NEW_MULTISYMBOL

static void pack_mb_tokens(aom_writer *w, const TOKENEXTRA **tp,
                           const TOKENEXTRA *const stop,
                           aom_bit_depth_t bit_depth, const TX_SIZE tx_size,
#if CONFIG_MRC_TX && SIGNAL_ANY_MRC_MASK
                           TX_TYPE tx_type, int is_inter,
#endif  // CONFIG_MRC_TX && SIGNAL_ANY_MRC_MASK
                           TOKEN_STATS *token_stats) {
  const TOKENEXTRA *p = *tp;
#if CONFIG_VAR_TX
  int count = 0;
  const int seg_eob = tx_size_2d[tx_size];
#endif

#if CONFIG_MRC_TX && SIGNAL_ANY_MRC_MASK
  if (tx_type == MRC_DCT && ((is_inter && SIGNAL_MRC_MASK_INTER) ||
                             (!is_inter && SIGNAL_MRC_MASK_INTRA))) {
    int rows = tx_size_high[tx_size];
    int cols = tx_size_wide[tx_size];
    assert(tx_size == TX_32X32);
    assert(p < stop);
    pack_map_tokens(w, &p, 2, rows * cols);
  }
#endif  // CONFIG_MRC_TX && SIGNAL_ANY_MRC_MASK

  while (p < stop && p->token != EOSB_TOKEN) {
    const int token = p->token;
    const int eob_val = p->eob_val;
    if (token == BLOCK_Z_TOKEN) {
      aom_write_symbol(w, 0, *p->head_cdf, HEAD_TOKENS + 1);
      p++;
#if CONFIG_VAR_TX
      break;
#endif
      continue;
    }

    const av1_extra_bit *const extra_bits = &av1_extra_bits[token];
    if (eob_val == LAST_EOB) {
      // Just code a flag indicating whether the value is >1 or 1.
      aom_write_bit(w, token != ONE_TOKEN);
    } else {
      int comb_symb = 2 * AOMMIN(token, TWO_TOKEN) - eob_val + p->first_val;
      aom_write_symbol(w, comb_symb, *p->head_cdf, HEAD_TOKENS + p->first_val);
    }
    if (token > ONE_TOKEN) {
      aom_write_symbol(w, token - TWO_TOKEN, *p->tail_cdf, TAIL_TOKENS);
    }

    if (extra_bits->base_val) {
      const int bit_string = p->extra;
      const int bit_string_length = extra_bits->len;  // Length of extra bits to
      const int is_cat6 = (extra_bits->base_val == CAT6_MIN_VAL);
      // be written excluding
      // the sign bit.
      int skip_bits = is_cat6
                          ? (int)sizeof(av1_cat6_prob) -
                                av1_get_cat6_extrabits_size(tx_size, bit_depth)
                          : 0;

      assert(!(bit_string >> (bit_string_length - skip_bits + 1)));
      if (bit_string_length > 0)
#if CONFIG_NEW_MULTISYMBOL
        write_coeff_extra(extra_bits->cdf, bit_string >> 1,
                          bit_string_length - skip_bits, w);
#else
        write_coeff_extra(extra_bits->prob, bit_string >> 1, bit_string_length,
                          skip_bits, w, token_stats);
#endif

      aom_write_bit_record(w, bit_string & 1, token_stats);
    }
    ++p;

#if CONFIG_VAR_TX
    ++count;
    if (eob_val == EARLY_EOB || count == seg_eob) break;
#endif
  }

  *tp = p;
}
#endif  // !CONFIG_LV_MAP
#else   // !CONFIG_PVQ
static PVQ_INFO *get_pvq_block(PVQ_QUEUE *pvq_q) {
  PVQ_INFO *pvq;

  assert(pvq_q->curr_pos <= pvq_q->last_pos);
  assert(pvq_q->curr_pos < pvq_q->buf_len);

  pvq = pvq_q->buf + pvq_q->curr_pos;
  ++pvq_q->curr_pos;

  return pvq;
}

static void pack_pvq_tokens(aom_writer *w, MACROBLOCK *const x,
                            MACROBLOCKD *const xd, int plane, BLOCK_SIZE bsize,
                            const TX_SIZE tx_size) {
  PVQ_INFO *pvq;
  int idx, idy;
  const struct macroblockd_plane *const pd = &xd->plane[plane];
  od_adapt_ctx *adapt;
  int max_blocks_wide;
  int max_blocks_high;
  int step = (1 << tx_size);

#if CONFIG_CHROMA_SUB8X8
  const BLOCK_SIZE plane_bsize =
      AOMMAX(BLOCK_4X4, get_plane_block_size(bsize, pd));
#elif CONFIG_CB4X4
  const BLOCK_SIZE plane_bsize = get_plane_block_size(bsize, pd);
#else
  const BLOCK_SIZE plane_bsize =
      get_plane_block_size(AOMMAX(BLOCK_8X8, bsize), pd);
#endif

  adapt = x->daala_enc.state.adapt;

  max_blocks_wide = max_block_wide(xd, plane_bsize, plane);
  max_blocks_high = max_block_high(xd, plane_bsize, plane);

  for (idy = 0; idy < max_blocks_high; idy += step) {
    for (idx = 0; idx < max_blocks_wide; idx += step) {
      const int is_keyframe = 0;
      const int encode_flip = 0;
      const int flip = 0;
      int i;
      const int has_dc_skip = 1;
      int *exg = &adapt->pvq.pvq_exg[plane][tx_size][0];
      int *ext = adapt->pvq.pvq_ext + tx_size * PVQ_MAX_PARTITIONS;
      generic_encoder *model = adapt->pvq.pvq_param_model;

      pvq = get_pvq_block(x->pvq_q);

      // encode block skip info
      aom_write_symbol(w, pvq->ac_dc_coded,
                       adapt->skip_cdf[2 * tx_size + (plane != 0)], 4);

      // AC coeffs coded?
      if (pvq->ac_dc_coded & AC_CODED) {
        assert(pvq->bs == tx_size);
        for (i = 0; i < pvq->nb_bands; i++) {
          if (i == 0 ||
              (!pvq->skip_rest && !(pvq->skip_dir & (1 << ((i - 1) % 3))))) {
            pvq_encode_partition(
                w, pvq->qg[i], pvq->theta[i], pvq->y + pvq->off[i],
                pvq->size[i], pvq->k[i], model, adapt, exg + i, ext + i,
                (plane != 0) * OD_TXSIZES * PVQ_MAX_PARTITIONS +
                    pvq->bs * PVQ_MAX_PARTITIONS + i,
                is_keyframe, i == 0 && (i < pvq->nb_bands - 1), pvq->skip_rest,
                encode_flip, flip);
          }
          if (i == 0 && !pvq->skip_rest && pvq->bs > 0) {
            aom_write_symbol(
                w, pvq->skip_dir,
                &adapt->pvq
                     .pvq_skip_dir_cdf[(plane != 0) + 2 * (pvq->bs - 1)][0],
                7);
          }
        }
      }
      // Encode residue of DC coeff, if exist.
      if (!has_dc_skip || (pvq->ac_dc_coded & DC_CODED)) {
        generic_encode(w, &adapt->model_dc[plane],
                       abs(pvq->dq_dc_residue) - has_dc_skip,
                       &adapt->ex_dc[plane][pvq->bs][0], 2);
      }
      if ((pvq->ac_dc_coded & DC_CODED)) {
        aom_write_bit(w, pvq->dq_dc_residue < 0);
      }
    }
  }  // for (idy = 0;
}
#endif  // !CONFIG_PVG

#if CONFIG_VAR_TX && !CONFIG_COEF_INTERLEAVE
#if CONFIG_LV_MAP
static void pack_txb_tokens(aom_writer *w,
#if CONFIG_LV_MAP
                            AV1_COMMON *cm,
#endif  // CONFIG_LV_MAP
                            const TOKENEXTRA **tp,
                            const TOKENEXTRA *const tok_end,
#if CONFIG_PVQ || CONFIG_LV_MAP
                            MACROBLOCK *const x,
#endif
                            MACROBLOCKD *xd, MB_MODE_INFO *mbmi, int plane,
                            BLOCK_SIZE plane_bsize, aom_bit_depth_t bit_depth,
                            int block, int blk_row, int blk_col,
                            TX_SIZE tx_size, TOKEN_STATS *token_stats) {
  const struct macroblockd_plane *const pd = &xd->plane[plane];
  const BLOCK_SIZE bsize = txsize_to_bsize[tx_size];
  const int tx_row = blk_row >> (1 - pd->subsampling_y);
  const int tx_col = blk_col >> (1 - pd->subsampling_x);
  TX_SIZE plane_tx_size;
  const int max_blocks_high = max_block_high(xd, plane_bsize, plane);
  const int max_blocks_wide = max_block_wide(xd, plane_bsize, plane);

  if (blk_row >= max_blocks_high || blk_col >= max_blocks_wide) return;

  plane_tx_size =
      plane ? uv_txsize_lookup[bsize][mbmi->inter_tx_size[tx_row][tx_col]][0][0]
            : mbmi->inter_tx_size[tx_row][tx_col];

  if (tx_size == plane_tx_size) {
    TOKEN_STATS tmp_token_stats;
    init_token_stats(&tmp_token_stats);

#if !CONFIG_PVQ
    tran_low_t *tcoeff = BLOCK_OFFSET(x->mbmi_ext->tcoeff[plane], block);
    uint16_t eob = x->mbmi_ext->eobs[plane][block];
    TXB_CTX txb_ctx = { x->mbmi_ext->txb_skip_ctx[plane][block],
                        x->mbmi_ext->dc_sign_ctx[plane][block] };
    av1_write_coeffs_txb(cm, xd, w, blk_row, blk_col, block, plane, tx_size,
                         tcoeff, eob, &txb_ctx);
#else
    pack_pvq_tokens(w, x, xd, plane, bsize, tx_size);
#endif
#if CONFIG_RD_DEBUG
    token_stats->txb_coeff_cost_map[blk_row][blk_col] = tmp_token_stats.cost;
    token_stats->cost += tmp_token_stats.cost;
#endif
  } else {
    const TX_SIZE sub_txs = sub_tx_size_map[tx_size];
    const int bsl = tx_size_wide_unit[sub_txs];
    int i;

    assert(bsl > 0);

    for (i = 0; i < 4; ++i) {
      const int offsetr = blk_row + (i >> 1) * bsl;
      const int offsetc = blk_col + (i & 0x01) * bsl;
      const int step = tx_size_wide_unit[sub_txs] * tx_size_high_unit[sub_txs];

      if (offsetr >= max_blocks_high || offsetc >= max_blocks_wide) continue;

      pack_txb_tokens(w,
#if CONFIG_LV_MAP
                      cm,
#endif
                      tp, tok_end,
#if CONFIG_PVQ || CONFIG_LV_MAP
                      x,
#endif
                      xd, mbmi, plane, plane_bsize, bit_depth, block, offsetr,
                      offsetc, sub_txs, token_stats);
      block += step;
    }
  }
}
#else  // CONFIG_LV_MAP
static void pack_txb_tokens(aom_writer *w, const TOKENEXTRA **tp,
                            const TOKENEXTRA *const tok_end,
#if CONFIG_PVQ
                            MACROBLOCK *const x,
#endif
                            MACROBLOCKD *xd, MB_MODE_INFO *mbmi, int plane,
                            BLOCK_SIZE plane_bsize, aom_bit_depth_t bit_depth,
                            int block, int blk_row, int blk_col,
                            TX_SIZE tx_size, TOKEN_STATS *token_stats) {
  const struct macroblockd_plane *const pd = &xd->plane[plane];
  const BLOCK_SIZE bsize = txsize_to_bsize[tx_size];
  const int tx_row = blk_row >> (1 - pd->subsampling_y);
  const int tx_col = blk_col >> (1 - pd->subsampling_x);
  TX_SIZE plane_tx_size;
  const int max_blocks_high = max_block_high(xd, plane_bsize, plane);
  const int max_blocks_wide = max_block_wide(xd, plane_bsize, plane);
#if CONFIG_MRC_TX && SIGNAL_ANY_MRC_MASK
  TX_TYPE tx_type = av1_get_tx_type(plane ? PLANE_TYPE_UV : PLANE_TYPE_Y, xd,
                                    blk_row, blk_col, block, tx_size);
#endif  // CONFIG_MRC_TX && SIGNAL_ANY_MRC_MASK

  if (blk_row >= max_blocks_high || blk_col >= max_blocks_wide) return;

  plane_tx_size =
      plane ? uv_txsize_lookup[bsize][mbmi->inter_tx_size[tx_row][tx_col]][0][0]
            : mbmi->inter_tx_size[tx_row][tx_col];

  if (tx_size == plane_tx_size) {
    TOKEN_STATS tmp_token_stats;
    init_token_stats(&tmp_token_stats);
#if !CONFIG_PVQ
    pack_mb_tokens(w, tp, tok_end, bit_depth, tx_size,
#if CONFIG_MRC_TX && SIGNAL_ANY_MRC_MASK
                   tx_type, is_inter_block(mbmi),
#endif  // CONFIG_MRC_TX && SIGNAL_ANY_MRC_MASK
                   &tmp_token_stats);
#else
    pack_pvq_tokens(w, x, xd, plane, bsize, tx_size);
#endif
#if CONFIG_RD_DEBUG
    token_stats->txb_coeff_cost_map[blk_row][blk_col] = tmp_token_stats.cost;
    token_stats->cost += tmp_token_stats.cost;
#endif
  } else {
#if CONFIG_RECT_TX_EXT
    int is_qttx = plane_tx_size == quarter_txsize_lookup[plane_bsize];
    const TX_SIZE sub_txs = is_qttx ? plane_tx_size : sub_tx_size_map[tx_size];
#else
    const TX_SIZE sub_txs = sub_tx_size_map[tx_size];
#endif
    const int bsl = tx_size_wide_unit[sub_txs];
    int i;

    assert(bsl > 0);

    for (i = 0; i < 4; ++i) {
#if CONFIG_RECT_TX_EXT
      int is_wide_tx = tx_size_wide_unit[sub_txs] > tx_size_high_unit[sub_txs];
      const int offsetr =
          is_qttx ? (is_wide_tx ? i * tx_size_high_unit[sub_txs] : 0)
                  : blk_row + (i >> 1) * bsl;
      const int offsetc =
          is_qttx ? (is_wide_tx ? 0 : i * tx_size_wide_unit[sub_txs])
                  : blk_col + (i & 0x01) * bsl;
#else
      const int offsetr = blk_row + (i >> 1) * bsl;
      const int offsetc = blk_col + (i & 0x01) * bsl;
#endif
      const int step = tx_size_wide_unit[sub_txs] * tx_size_high_unit[sub_txs];

      if (offsetr >= max_blocks_high || offsetc >= max_blocks_wide) continue;

      pack_txb_tokens(w, tp, tok_end,
#if CONFIG_PVQ
                      x,
#endif
                      xd, mbmi, plane, plane_bsize, bit_depth, block, offsetr,
                      offsetc, sub_txs, token_stats);
      block += step;
    }
  }
}
#endif  // CONFIG_LV_MAP
#endif  // CONFIG_VAR_TX

static void write_segment_id(aom_writer *w, const struct segmentation *seg,
                             struct segmentation_probs *segp, int segment_id) {
  if (seg->enabled && seg->update_map) {
    aom_write_symbol(w, segment_id, segp->tree_cdf, MAX_SEGMENTS);
  }
}

#if CONFIG_NEW_MULTISYMBOL
#define WRITE_REF_BIT(bname, pname) \
  aom_write_symbol(w, bname, av1_get_pred_cdf_##pname(cm, xd), 2)
#define WRITE_REF_BIT2(bname, pname) \
  aom_write_symbol(w, bname, av1_get_pred_cdf_##pname(xd), 2)
#else
#define WRITE_REF_BIT(bname, pname) \
  aom_write(w, bname, av1_get_pred_prob_##pname(cm, xd))
#define WRITE_REF_BIT2(bname, pname) \
  aom_write(w, bname, av1_get_pred_prob_##pname(cm, xd))
#endif

// This function encodes the reference frame
static void write_ref_frames(const AV1_COMMON *cm, const MACROBLOCKD *xd,
                             aom_writer *w) {
  const MB_MODE_INFO *const mbmi = &xd->mi[0]->mbmi;
  const int is_compound = has_second_ref(mbmi);
  const int segment_id = mbmi->segment_id;

  // If segment level coding of this signal is disabled...
  // or the segment allows multiple reference frame options
  if (segfeature_active(&cm->seg, segment_id, SEG_LVL_REF_FRAME)) {
    assert(!is_compound);
    assert(mbmi->ref_frame[0] ==
           get_segdata(&cm->seg, segment_id, SEG_LVL_REF_FRAME));
  } else {
    // does the feature use compound prediction or not
    // (if not specified at the frame/segment level)
    if (cm->reference_mode == REFERENCE_MODE_SELECT) {
      if (is_comp_ref_allowed(mbmi->sb_type))
#if CONFIG_NEW_MULTISYMBOL
        aom_write_symbol(w, is_compound, av1_get_reference_mode_cdf(cm, xd), 2);
#else
        aom_write(w, is_compound, av1_get_reference_mode_prob(cm, xd));
#endif  // CONFIG_NEW_MULTISYMBOL
    } else {
      assert((!is_compound) == (cm->reference_mode == SINGLE_REFERENCE));
    }

    if (is_compound) {
#if CONFIG_EXT_COMP_REFS
      const COMP_REFERENCE_TYPE comp_ref_type = has_uni_comp_refs(mbmi)
                                                    ? UNIDIR_COMP_REFERENCE
                                                    : BIDIR_COMP_REFERENCE;
#if USE_UNI_COMP_REFS
#if CONFIG_VAR_REFS
      if ((L_OR_L2(cm) || L3_OR_G(cm)) && BWD_OR_ALT(cm))
        if (L_AND_L2(cm) || L_AND_L3(cm) || L_AND_G(cm) || BWD_AND_ALT(cm))
#endif  // CONFIG_VAR_REFS
#if CONFIG_NEW_MULTISYMBOL
          aom_write_symbol(w, comp_ref_type,
                           av1_get_comp_reference_type_cdf(xd), 2);
#else
      aom_write(w, comp_ref_type, av1_get_comp_reference_type_prob(cm, xd));
#endif
#if CONFIG_VAR_REFS
        else
          assert(comp_ref_type == BIDIR_COMP_REFERENCE);
      else
        assert(comp_ref_type == UNIDIR_COMP_REFERENCE);
#endif  // CONFIG_VAR_REFS
#else   // !USE_UNI_COMP_REFS
      // NOTE: uni-directional comp refs disabled
      assert(comp_ref_type == BIDIR_COMP_REFERENCE);
#endif  // USE_UNI_COMP_REFS

      if (comp_ref_type == UNIDIR_COMP_REFERENCE) {
        const int bit = mbmi->ref_frame[0] == BWDREF_FRAME;
#if CONFIG_VAR_REFS
        if ((L_AND_L2(cm) || L_AND_L3(cm) || L_AND_G(cm)) && BWD_AND_ALT(cm))
#endif  // CONFIG_VAR_REFS
          WRITE_REF_BIT2(bit, uni_comp_ref_p);

        if (!bit) {
          assert(mbmi->ref_frame[0] == LAST_FRAME);
#if CONFIG_VAR_REFS
          if (L_AND_L2(cm) && (L_AND_L3(cm) || L_AND_G(cm))) {
#endif  // CONFIG_VAR_REFS
            const int bit1 = mbmi->ref_frame[1] == LAST3_FRAME ||
                             mbmi->ref_frame[1] == GOLDEN_FRAME;
            WRITE_REF_BIT2(bit1, uni_comp_ref_p1);
            if (bit1) {
#if CONFIG_VAR_REFS
              if (L_AND_L3(cm) && L_AND_G(cm)) {
#endif  // CONFIG_VAR_REFS
                const int bit2 = mbmi->ref_frame[1] == GOLDEN_FRAME;
                WRITE_REF_BIT2(bit2, uni_comp_ref_p2);
#if CONFIG_VAR_REFS
              }
#endif  // CONFIG_VAR_REFS
            }
#if CONFIG_VAR_REFS
          }
#endif  // CONFIG_VAR_REFS
        } else {
          assert(mbmi->ref_frame[1] == ALTREF_FRAME);
        }

        return;
      }

      assert(comp_ref_type == BIDIR_COMP_REFERENCE);
#endif  // CONFIG_EXT_COMP_REFS

#if CONFIG_EXT_REFS
      const int bit = (mbmi->ref_frame[0] == GOLDEN_FRAME ||
                       mbmi->ref_frame[0] == LAST3_FRAME);
#if CONFIG_VAR_REFS
      // Test need to explicitly code (L,L2) vs (L3,G) branch node in tree
      if (L_OR_L2(cm) && L3_OR_G(cm))
#endif  // CONFIG_VAR_REFS
        WRITE_REF_BIT(bit, comp_ref_p);

      if (!bit) {
#if CONFIG_VAR_REFS
        // Test need to explicitly code (L) vs (L2) branch node in tree
        if (L_AND_L2(cm)) {
#endif  // CONFIG_VAR_REFS
          const int bit1 = mbmi->ref_frame[0] == LAST_FRAME;
          WRITE_REF_BIT(bit1, comp_ref_p1);
#if CONFIG_VAR_REFS
        }
#endif  // CONFIG_VAR_REFS
      } else {
#if CONFIG_VAR_REFS
        // Test need to explicitly code (L3) vs (G) branch node in tree
        if (L3_AND_G(cm)) {
#endif  // CONFIG_VAR_REFS
          const int bit2 = mbmi->ref_frame[0] == GOLDEN_FRAME;
          WRITE_REF_BIT(bit2, comp_ref_p2);
#if CONFIG_VAR_REFS
        }
#endif  // CONFIG_VAR_REFS
      }

#if CONFIG_VAR_REFS
      // Test need to explicitly code (BWD,ALT2) vs (ALT) branch node in tree
      if (BWD_OR_ALT2(cm) && ALTREF_IS_VALID(cm)) {
#endif  // CONFIG_VAR_REFS
        const int bit_bwd = mbmi->ref_frame[1] == ALTREF_FRAME;
        WRITE_REF_BIT(bit_bwd, comp_bwdref_p);

        if (!bit_bwd) {
#if CONFIG_VAR_REFS
          // Test need to explicitly code (BWD,ALT2) vs (ALT) branch node in
          // tree
          if (BWD_AND_ALT2(cm))
#endif  // CONFIG_VAR_REFS
            WRITE_REF_BIT(mbmi->ref_frame[1] == ALTREF2_FRAME, comp_bwdref_p1);
        }
#if CONFIG_VAR_REFS
      }
#endif  // CONFIG_VAR_REFS

#else   // !CONFIG_EXT_REFS
      const int bit = mbmi->ref_frame[0] == GOLDEN_FRAME;
      WRITE_REF_BIT(bit, comp_ref_p);
#endif  // CONFIG_EXT_REFS
    } else {
#if CONFIG_EXT_REFS
      const int bit0 = (mbmi->ref_frame[0] <= ALTREF_FRAME &&
                        mbmi->ref_frame[0] >= BWDREF_FRAME);
#if CONFIG_VAR_REFS
      // Test need to explicitly code (L,L2,L3,G) vs (BWD,ALT2,ALT) branch node
      // in tree
      if ((L_OR_L2(cm) || L3_OR_G(cm)) &&
          (BWD_OR_ALT2(cm) || ALTREF_IS_VALID(cm)))
#endif  // CONFIG_VAR_REFS
        WRITE_REF_BIT(bit0, single_ref_p1);

      if (bit0) {
#if CONFIG_VAR_REFS
        // Test need to explicitly code (BWD,ALT2) vs (ALT) branch node in tree
        if (BWD_OR_ALT2(cm) && ALTREF_IS_VALID(cm)) {
#endif  // CONFIG_VAR_REFS
          const int bit1 = mbmi->ref_frame[0] == ALTREF_FRAME;
          WRITE_REF_BIT(bit1, single_ref_p2);

          if (!bit1) {
#if CONFIG_VAR_REFS
            // Test need to explicitly code (BWD) vs (ALT2) branch node in tree
            if (BWD_AND_ALT2(cm))
#endif  // CONFIG_VAR_REFS
              WRITE_REF_BIT(mbmi->ref_frame[0] == ALTREF2_FRAME, single_ref_p6);
          }
#if CONFIG_VAR_REFS
        }
#endif  // CONFIG_VAR_REFS
      } else {
        const int bit2 = (mbmi->ref_frame[0] == LAST3_FRAME ||
                          mbmi->ref_frame[0] == GOLDEN_FRAME);
#if CONFIG_VAR_REFS
        // Test need to explicitly code (L,L2) vs (L3,G) branch node in tree
        if (L_OR_L2(cm) && L3_OR_G(cm))
#endif  // CONFIG_VAR_REFS
          WRITE_REF_BIT(bit2, single_ref_p3);

        if (!bit2) {
#if CONFIG_VAR_REFS
          // Test need to explicitly code (L) vs (L2) branch node in tree
          if (L_AND_L2(cm)) {
#endif  // CONFIG_VAR_REFS
            const int bit3 = mbmi->ref_frame[0] != LAST_FRAME;
            WRITE_REF_BIT(bit3, single_ref_p4);
#if CONFIG_VAR_REFS
          }
#endif  // CONFIG_VAR_REFS
        } else {
#if CONFIG_VAR_REFS
          // Test need to explicitly code (L3) vs (G) branch node in tree
          if (L3_AND_G(cm)) {
#endif  // CONFIG_VAR_REFS
            const int bit4 = mbmi->ref_frame[0] != LAST3_FRAME;
            WRITE_REF_BIT(bit4, single_ref_p5);
#if CONFIG_VAR_REFS
          }
#endif  // CONFIG_VAR_REFS
        }
      }
#else   // !CONFIG_EXT_REFS
      const int bit0 = mbmi->ref_frame[0] != LAST_FRAME;
      WRITE_REF_BIT(bit0, single_ref_p1);

      if (bit0) {
        const int bit1 = mbmi->ref_frame[0] != GOLDEN_FRAME;
        WRITE_REF_BIT(bit1, single_ref_p2);
      }
#endif  // CONFIG_EXT_REFS
    }
  }
}

#if CONFIG_FILTER_INTRA
static void write_filter_intra_mode_info(const AV1_COMMON *const cm,
                                         const MACROBLOCKD *xd,
                                         const MB_MODE_INFO *const mbmi,
                                         int mi_row, int mi_col,
                                         aom_writer *w) {
  if (mbmi->mode == DC_PRED && mbmi->palette_mode_info.palette_size[0] == 0) {
    aom_write(w, mbmi->filter_intra_mode_info.use_filter_intra_mode[0],
              cm->fc->filter_intra_probs[0]);
    if (mbmi->filter_intra_mode_info.use_filter_intra_mode[0]) {
      const FILTER_INTRA_MODE mode =
          mbmi->filter_intra_mode_info.filter_intra_mode[0];
      write_uniform(w, FILTER_INTRA_MODES, mode);
    }
  }

#if CONFIG_CB4X4
  if (!is_chroma_reference(mi_row, mi_col, mbmi->sb_type,
                           xd->plane[1].subsampling_x,
                           xd->plane[1].subsampling_y))
    return;
#else
  (void)xd;
  (void)mi_row;
  (void)mi_col;
#endif  // CONFIG_CB4X4

  if (mbmi->uv_mode == UV_DC_PRED &&
      mbmi->palette_mode_info.palette_size[1] == 0) {
    aom_write(w, mbmi->filter_intra_mode_info.use_filter_intra_mode[1],
              cm->fc->filter_intra_probs[1]);
    if (mbmi->filter_intra_mode_info.use_filter_intra_mode[1]) {
      const FILTER_INTRA_MODE mode =
          mbmi->filter_intra_mode_info.filter_intra_mode[1];
      write_uniform(w, FILTER_INTRA_MODES, mode);
    }
  }
}
#endif  // CONFIG_FILTER_INTRA

#if CONFIG_EXT_INTRA
static void write_intra_angle_info(const MACROBLOCKD *xd,
                                   FRAME_CONTEXT *const ec_ctx, aom_writer *w) {
  const MB_MODE_INFO *const mbmi = &xd->mi[0]->mbmi;
  const BLOCK_SIZE bsize = mbmi->sb_type;
#if CONFIG_INTRA_INTERP
  const int intra_filter_ctx = av1_get_pred_context_intra_interp(xd);
  int p_angle;
#endif  // CONFIG_INTRA_INTERP

  (void)ec_ctx;
  if (!av1_use_angle_delta(bsize)) return;

  if (av1_is_directional_mode(mbmi->mode, bsize)) {
    write_uniform(w, 2 * MAX_ANGLE_DELTA + 1,
                  MAX_ANGLE_DELTA + mbmi->angle_delta[0]);
#if CONFIG_INTRA_INTERP
    p_angle = mode_to_angle_map[mbmi->mode] + mbmi->angle_delta[0] * ANGLE_STEP;
    if (av1_is_intra_filter_switchable(p_angle)) {
      aom_write_symbol(w, mbmi->intra_filter,
                       ec_ctx->intra_filter_cdf[intra_filter_ctx],
                       INTRA_FILTERS);
    }
#endif  // CONFIG_INTRA_INTERP
  }

  if (av1_is_directional_mode(get_uv_mode(mbmi->uv_mode), bsize)) {
    write_uniform(w, 2 * MAX_ANGLE_DELTA + 1,
                  MAX_ANGLE_DELTA + mbmi->angle_delta[1]);
  }
}
#endif  // CONFIG_EXT_INTRA

static void write_mb_interp_filter(AV1_COMP *cpi, const MACROBLOCKD *xd,
                                   aom_writer *w) {
  AV1_COMMON *const cm = &cpi->common;
  const MB_MODE_INFO *const mbmi = &xd->mi[0]->mbmi;
  FRAME_CONTEXT *ec_ctx = xd->tile_ctx;

  if (!av1_is_interp_needed(xd)) {
    assert(mbmi->interp_filters ==
           av1_broadcast_interp_filter(
               av1_unswitchable_filter(cm->interp_filter)));
    return;
  }
  if (cm->interp_filter == SWITCHABLE) {
#if CONFIG_DUAL_FILTER
    int dir;
    for (dir = 0; dir < 2; ++dir) {
      if (has_subpel_mv_component(xd->mi[0], xd, dir) ||
          (mbmi->ref_frame[1] > INTRA_FRAME &&
           has_subpel_mv_component(xd->mi[0], xd, dir + 2))) {
        const int ctx = av1_get_pred_context_switchable_interp(xd, dir);
        InterpFilter filter =
            av1_extract_interp_filter(mbmi->interp_filters, dir);
        aom_write_symbol(w, filter, ec_ctx->switchable_interp_cdf[ctx],
                         SWITCHABLE_FILTERS);
        ++cpi->interp_filter_selected[0][filter];
      } else {
        assert(av1_extract_interp_filter(mbmi->interp_filters, dir) ==
               EIGHTTAP_REGULAR);
      }
    }
#else
    {
      const int ctx = av1_get_pred_context_switchable_interp(xd);
      InterpFilter filter = av1_extract_interp_filter(mbmi->interp_filters, 0);
      aom_write_symbol(w, filter, ec_ctx->switchable_interp_cdf[ctx],
                       SWITCHABLE_FILTERS);
      ++cpi->interp_filter_selected[0][filter];
    }
#endif  // CONFIG_DUAL_FILTER
  }
}

#if CONFIG_PALETTE_DELTA_ENCODING
// Transmit color values with delta encoding. Write the first value as
// literal, and the deltas between each value and the previous one. "min_val" is
// the smallest possible value of the deltas.
static void delta_encode_palette_colors(const int *colors, int num,
                                        int bit_depth, int min_val,
                                        aom_writer *w) {
  if (num <= 0) return;
  assert(colors[0] < (1 << bit_depth));
  aom_write_literal(w, colors[0], bit_depth);
  if (num == 1) return;
  int max_delta = 0;
  int deltas[PALETTE_MAX_SIZE];
  memset(deltas, 0, sizeof(deltas));
  for (int i = 1; i < num; ++i) {
    assert(colors[i] < (1 << bit_depth));
    const int delta = colors[i] - colors[i - 1];
    deltas[i - 1] = delta;
    assert(delta >= min_val);
    if (delta > max_delta) max_delta = delta;
  }
  const int min_bits = bit_depth - 3;
  int bits = AOMMAX(av1_ceil_log2(max_delta + 1 - min_val), min_bits);
  assert(bits <= bit_depth);
  int range = (1 << bit_depth) - colors[0] - min_val;
  aom_write_literal(w, bits - min_bits, 2);
  for (int i = 0; i < num - 1; ++i) {
    aom_write_literal(w, deltas[i] - min_val, bits);
    range -= deltas[i];
    bits = AOMMIN(bits, av1_ceil_log2(range));
  }
}

// Transmit luma palette color values. First signal if each color in the color
// cache is used. Those colors that are not in the cache are transmitted with
// delta encoding.
static void write_palette_colors_y(const MACROBLOCKD *const xd,
                                   const PALETTE_MODE_INFO *const pmi,
                                   int bit_depth, aom_writer *w) {
  const int n = pmi->palette_size[0];
  uint16_t color_cache[2 * PALETTE_MAX_SIZE];
  const int n_cache = av1_get_palette_cache(xd, 0, color_cache);
  int out_cache_colors[PALETTE_MAX_SIZE];
  uint8_t cache_color_found[2 * PALETTE_MAX_SIZE];
  const int n_out_cache =
      av1_index_color_cache(color_cache, n_cache, pmi->palette_colors, n,
                            cache_color_found, out_cache_colors);
  int n_in_cache = 0;
  for (int i = 0; i < n_cache && n_in_cache < n; ++i) {
    const int found = cache_color_found[i];
    aom_write_bit(w, found);
    n_in_cache += found;
  }
  assert(n_in_cache + n_out_cache == n);
  delta_encode_palette_colors(out_cache_colors, n_out_cache, bit_depth, 1, w);
}

// Write chroma palette color values. U channel is handled similarly to the luma
// channel. For v channel, either use delta encoding or transmit raw values
// directly, whichever costs less.
static void write_palette_colors_uv(const MACROBLOCKD *const xd,
                                    const PALETTE_MODE_INFO *const pmi,
                                    int bit_depth, aom_writer *w) {
  const int n = pmi->palette_size[1];
  const uint16_t *colors_u = pmi->palette_colors + PALETTE_MAX_SIZE;
  const uint16_t *colors_v = pmi->palette_colors + 2 * PALETTE_MAX_SIZE;
  // U channel colors.
  uint16_t color_cache[2 * PALETTE_MAX_SIZE];
  const int n_cache = av1_get_palette_cache(xd, 1, color_cache);
  int out_cache_colors[PALETTE_MAX_SIZE];
  uint8_t cache_color_found[2 * PALETTE_MAX_SIZE];
  const int n_out_cache = av1_index_color_cache(
      color_cache, n_cache, colors_u, n, cache_color_found, out_cache_colors);
  int n_in_cache = 0;
  for (int i = 0; i < n_cache && n_in_cache < n; ++i) {
    const int found = cache_color_found[i];
    aom_write_bit(w, found);
    n_in_cache += found;
  }
  delta_encode_palette_colors(out_cache_colors, n_out_cache, bit_depth, 0, w);

  // V channel colors. Don't use color cache as the colors are not sorted.
  const int max_val = 1 << bit_depth;
  int zero_count = 0, min_bits_v = 0;
  int bits_v =
      av1_get_palette_delta_bits_v(pmi, bit_depth, &zero_count, &min_bits_v);
  const int rate_using_delta =
      2 + bit_depth + (bits_v + 1) * (n - 1) - zero_count;
  const int rate_using_raw = bit_depth * n;
  if (rate_using_delta < rate_using_raw) {  // delta encoding
    assert(colors_v[0] < (1 << bit_depth));
    aom_write_bit(w, 1);
    aom_write_literal(w, bits_v - min_bits_v, 2);
    aom_write_literal(w, colors_v[0], bit_depth);
    for (int i = 1; i < n; ++i) {
      assert(colors_v[i] < (1 << bit_depth));
      if (colors_v[i] == colors_v[i - 1]) {  // No need to signal sign bit.
        aom_write_literal(w, 0, bits_v);
        continue;
      }
      const int delta = abs((int)colors_v[i] - colors_v[i - 1]);
      const int sign_bit = colors_v[i] < colors_v[i - 1];
      if (delta <= max_val - delta) {
        aom_write_literal(w, delta, bits_v);
        aom_write_bit(w, sign_bit);
      } else {
        aom_write_literal(w, max_val - delta, bits_v);
        aom_write_bit(w, !sign_bit);
      }
    }
  } else {  // Transmit raw values.
    aom_write_bit(w, 0);
    for (int i = 0; i < n; ++i) {
      assert(colors_v[i] < (1 << bit_depth));
      aom_write_literal(w, colors_v[i], bit_depth);
    }
  }
}
#endif  // CONFIG_PALETTE_DELTA_ENCODING

static void write_palette_mode_info(const AV1_COMMON *cm, const MACROBLOCKD *xd,
                                    const MODE_INFO *const mi, aom_writer *w) {
  const MB_MODE_INFO *const mbmi = &mi->mbmi;
  const MODE_INFO *const above_mi = xd->above_mi;
  const MODE_INFO *const left_mi = xd->left_mi;
  const BLOCK_SIZE bsize = mbmi->sb_type;
  const PALETTE_MODE_INFO *const pmi = &mbmi->palette_mode_info;

  assert(bsize >= BLOCK_8X8 && bsize <= BLOCK_LARGEST);
  const int block_palette_idx = bsize - BLOCK_8X8;

  if (mbmi->mode == DC_PRED) {
    const int n = pmi->palette_size[0];
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
    aom_write_symbol(
        w, n > 0,
        xd->tile_ctx->palette_y_mode_cdf[block_palette_idx][palette_y_mode_ctx],
        2);
#else
    aom_write(
        w, n > 0,
        av1_default_palette_y_mode_prob[block_palette_idx][palette_y_mode_ctx]);
#endif
    if (n > 0) {
      aom_write_symbol(w, n - PALETTE_MIN_SIZE,
                       xd->tile_ctx->palette_y_size_cdf[block_palette_idx],
                       PALETTE_SIZES);
#if CONFIG_PALETTE_DELTA_ENCODING
      write_palette_colors_y(xd, pmi, cm->bit_depth, w);
#else
      for (int i = 0; i < n; ++i) {
        assert(pmi->palette_colors[i] < (1 << cm->bit_depth));
        aom_write_literal(w, pmi->palette_colors[i], cm->bit_depth);
      }
#endif  // CONFIG_PALETTE_DELTA_ENCODING
    }
  }

  if (mbmi->uv_mode == UV_DC_PRED) {
    const int n = pmi->palette_size[1];
    const int palette_uv_mode_ctx = (pmi->palette_size[0] > 0);
#if CONFIG_NEW_MULTISYMBOL
    aom_write_symbol(w, n > 0,
                     xd->tile_ctx->palette_uv_mode_cdf[palette_uv_mode_ctx], 2);
#else
    aom_write(w, n > 0, av1_default_palette_uv_mode_prob[palette_uv_mode_ctx]);
#endif
    if (n > 0) {
      aom_write_symbol(w, n - PALETTE_MIN_SIZE,
                       xd->tile_ctx->palette_uv_size_cdf[block_palette_idx],
                       PALETTE_SIZES);
#if CONFIG_PALETTE_DELTA_ENCODING
      write_palette_colors_uv(xd, pmi, cm->bit_depth, w);
#else
      for (int i = 0; i < n; ++i) {
        assert(pmi->palette_colors[PALETTE_MAX_SIZE + i] <
               (1 << cm->bit_depth));
        assert(pmi->palette_colors[2 * PALETTE_MAX_SIZE + i] <
               (1 << cm->bit_depth));
        aom_write_literal(w, pmi->palette_colors[PALETTE_MAX_SIZE + i],
                          cm->bit_depth);
        aom_write_literal(w, pmi->palette_colors[2 * PALETTE_MAX_SIZE + i],
                          cm->bit_depth);
      }
#endif  // CONFIG_PALETTE_DELTA_ENCODING
    }
  }
}

void av1_write_tx_type(const AV1_COMMON *const cm, const MACROBLOCKD *xd,
#if CONFIG_SUPERTX
                       const int supertx_enabled,
#endif
#if CONFIG_TXK_SEL
                       int blk_row, int blk_col, int block, int plane,
                       TX_SIZE tx_size,
#endif
                       aom_writer *w) {
  MB_MODE_INFO *mbmi = &xd->mi[0]->mbmi;
  const int is_inter = is_inter_block(mbmi);
#if !CONFIG_TXK_SEL
#if CONFIG_VAR_TX
  const TX_SIZE tx_size = is_inter ? mbmi->min_tx_size : mbmi->tx_size;
#else
  const TX_SIZE tx_size = mbmi->tx_size;
#endif  // CONFIG_VAR_TX
#endif  // !CONFIG_TXK_SEL
  FRAME_CONTEXT *ec_ctx = xd->tile_ctx;

#if !CONFIG_TXK_SEL
  TX_TYPE tx_type = mbmi->tx_type;
#else
  // Only y plane's tx_type is transmitted
  if (plane > 0) return;
  PLANE_TYPE plane_type = get_plane_type(plane);
  TX_TYPE tx_type =
      av1_get_tx_type(plane_type, xd, blk_row, blk_col, block, tx_size);
#endif

  if (!FIXED_TX_TYPE) {
#if CONFIG_EXT_TX
    const TX_SIZE square_tx_size = txsize_sqr_map[tx_size];
    const BLOCK_SIZE bsize = mbmi->sb_type;
    if (get_ext_tx_types(tx_size, bsize, is_inter, cm->reduced_tx_set_used) >
            1 &&
        ((!cm->seg.enabled && cm->base_qindex > 0) ||
         (cm->seg.enabled && xd->qindex[mbmi->segment_id] > 0)) &&
        !mbmi->skip &&
#if CONFIG_SUPERTX
        !supertx_enabled &&
#endif  // CONFIG_SUPERTX
        !segfeature_active(&cm->seg, mbmi->segment_id, SEG_LVL_SKIP)) {
#if CONFIG_MRC_TX
      if (tx_type == MRC_DCT)
        assert(mbmi->valid_mrc_mask && "Invalid MRC mask");
#endif  // CONFIG_MRC_TX
      const TxSetType tx_set_type = get_ext_tx_set_type(
          tx_size, bsize, is_inter, cm->reduced_tx_set_used);
      const int eset =
          get_ext_tx_set(tx_size, bsize, is_inter, cm->reduced_tx_set_used);
      // eset == 0 should correspond to a set with only DCT_DCT and there
      // is no need to send the tx_type
      assert(eset > 0);
      assert(av1_ext_tx_used[tx_set_type][tx_type]);
#if !CONFIG_LGT_FROM_PRED
      if (is_inter) {
        aom_write_symbol(w, av1_ext_tx_ind[tx_set_type][tx_type],
                         ec_ctx->inter_ext_tx_cdf[eset][square_tx_size],
                         av1_num_ext_tx_set[tx_set_type]);
      } else if (ALLOW_INTRA_EXT_TX) {
        aom_write_symbol(
            w, av1_ext_tx_ind[tx_set_type][tx_type],
            ec_ctx->intra_ext_tx_cdf[eset][square_tx_size][mbmi->mode],
            av1_num_ext_tx_set[tx_set_type]);
      }
#else
      // only signal tx_type when lgt is not allowed or not selected
      if (is_inter) {
        if (LGT_FROM_PRED_INTER) {
          if (is_lgt_allowed(mbmi->mode, tx_size) && !cm->reduced_tx_set_used)
            aom_write(w, mbmi->use_lgt, ec_ctx->inter_lgt_prob[square_tx_size]);
          if (!mbmi->use_lgt)
            aom_write_symbol(w, av1_ext_tx_ind[tx_set_type][tx_type],
                             ec_ctx->inter_ext_tx_cdf[eset][square_tx_size],
                             av1_num_ext_tx_set[tx_set_type]);
        } else {
          aom_write_symbol(w, av1_ext_tx_ind[tx_set_type][tx_type],
                           ec_ctx->inter_ext_tx_cdf[eset][square_tx_size],
                           av1_num_ext_tx_set[tx_set_type]);
        }
      } else if (ALLOW_INTRA_EXT_TX) {
        if (LGT_FROM_PRED_INTRA) {
          if (is_lgt_allowed(mbmi->mode, tx_size) && !cm->reduced_tx_set_used)
            aom_write(w, mbmi->use_lgt,
                      ec_ctx->intra_lgt_prob[square_tx_size][mbmi->mode]);
          if (!mbmi->use_lgt)
            aom_write_symbol(
                w, av1_ext_tx_ind[tx_set_type][tx_type],
                ec_ctx->intra_ext_tx_cdf[eset][square_tx_size][mbmi->mode],
                av1_num_ext_tx_set[tx_set_type]);
        } else {
          aom_write_symbol(
              w, av1_ext_tx_ind[tx_set_type][tx_type],
              ec_ctx->intra_ext_tx_cdf[eset][square_tx_size][mbmi->mode],
              av1_num_ext_tx_set[tx_set_type]);
        }
      }
#endif  // CONFIG_LGT_FROM_PRED
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
      if (is_inter) {
        aom_write_symbol(w, av1_ext_tx_ind[tx_type],
                         ec_ctx->inter_ext_tx_cdf[tx_size], TX_TYPES);
      } else {
        aom_write_symbol(
            w, av1_ext_tx_ind[tx_type],
            ec_ctx->intra_ext_tx_cdf[tx_size]
                                    [intra_mode_to_tx_type_context[mbmi->mode]],
            TX_TYPES);
      }
    }
#endif  // CONFIG_EXT_TX
  }
}

static void write_intra_mode(FRAME_CONTEXT *frame_ctx, BLOCK_SIZE bsize,
                             PREDICTION_MODE mode, aom_writer *w) {
  aom_write_symbol(w, mode, frame_ctx->y_mode_cdf[size_group_lookup[bsize]],
                   INTRA_MODES);
}

static void write_intra_uv_mode(FRAME_CONTEXT *frame_ctx,
                                UV_PREDICTION_MODE uv_mode,
                                PREDICTION_MODE y_mode, aom_writer *w) {
#if !CONFIG_CFL
  uv_mode = get_uv_mode(uv_mode);
#endif
  aom_write_symbol(w, uv_mode, frame_ctx->uv_mode_cdf[y_mode], UV_INTRA_MODES);
}

#if CONFIG_CFL
static void write_cfl_alphas(FRAME_CONTEXT *const ec_ctx, int idx,
                             int joint_sign, aom_writer *w) {
  aom_write_symbol(w, joint_sign, ec_ctx->cfl_sign_cdf, CFL_JOINT_SIGNS);
  // Magnitudes are only signaled for nonzero codes.
  if (CFL_SIGN_U(joint_sign) != CFL_SIGN_ZERO) {
    aom_cdf_prob *cdf_u = ec_ctx->cfl_alpha_cdf[CFL_CONTEXT_U(joint_sign)];
    aom_write_symbol(w, CFL_IDX_U(idx), cdf_u, CFL_ALPHABET_SIZE);
  }
  if (CFL_SIGN_V(joint_sign) != CFL_SIGN_ZERO) {
    aom_cdf_prob *cdf_v = ec_ctx->cfl_alpha_cdf[CFL_CONTEXT_V(joint_sign)];
    aom_write_symbol(w, CFL_IDX_V(idx), cdf_v, CFL_ALPHABET_SIZE);
  }
}
#endif

static void pack_inter_mode_mvs(AV1_COMP *cpi, const int mi_row,
                                const int mi_col,
#if CONFIG_SUPERTX
                                int supertx_enabled,
#endif
                                aom_writer *w) {
  AV1_COMMON *const cm = &cpi->common;
  MACROBLOCK *const x = &cpi->td.mb;
  MACROBLOCKD *const xd = &x->e_mbd;
  FRAME_CONTEXT *ec_ctx = xd->tile_ctx;
  const MODE_INFO *mi = xd->mi[0];

  const struct segmentation *const seg = &cm->seg;
  struct segmentation_probs *const segp = &ec_ctx->seg;
  const MB_MODE_INFO *const mbmi = &mi->mbmi;
  const MB_MODE_INFO_EXT *const mbmi_ext = x->mbmi_ext;
  const PREDICTION_MODE mode = mbmi->mode;
  const int segment_id = mbmi->segment_id;
  const BLOCK_SIZE bsize = mbmi->sb_type;
  const int allow_hp = cm->allow_high_precision_mv;
  const int is_inter = is_inter_block(mbmi);
  const int is_compound = has_second_ref(mbmi);
  int skip, ref;
#if CONFIG_CB4X4
  const int unify_bsize = 1;
#else
  const int unify_bsize = 0;
#endif
  (void)mi_row;
  (void)mi_col;

  if (seg->update_map) {
    if (seg->temporal_update) {
      const int pred_flag = mbmi->seg_id_predicted;
#if CONFIG_NEW_MULTISYMBOL
      aom_cdf_prob *pred_cdf = av1_get_pred_cdf_seg_id(segp, xd);
      aom_write_symbol(w, pred_flag, pred_cdf, 2);
#else
      aom_prob pred_prob = av1_get_pred_prob_seg_id(segp, xd);
      aom_write(w, pred_flag, pred_prob);
#endif
      if (!pred_flag) write_segment_id(w, seg, segp, segment_id);
    } else {
      write_segment_id(w, seg, segp, segment_id);
    }
  }

#if CONFIG_SUPERTX
  if (supertx_enabled)
    skip = mbmi->skip;
  else
    skip = write_skip(cm, xd, segment_id, mi, w);
#else
  skip = write_skip(cm, xd, segment_id, mi, w);
#endif  // CONFIG_SUPERTX
  if (cm->delta_q_present_flag) {
    int super_block_upper_left =
        ((mi_row & MAX_MIB_MASK) == 0) && ((mi_col & MAX_MIB_MASK) == 0);
    if ((bsize != BLOCK_LARGEST || skip == 0) && super_block_upper_left) {
      assert(mbmi->current_q_index > 0);
      int reduced_delta_qindex =
          (mbmi->current_q_index - xd->prev_qindex) / cm->delta_q_res;
      write_delta_qindex(cm, xd, reduced_delta_qindex, w);
      xd->prev_qindex = mbmi->current_q_index;
#if CONFIG_EXT_DELTA_Q
#if CONFIG_LOOPFILTER_LEVEL
      if (cm->delta_lf_present_flag) {
        if (cm->delta_lf_multi) {
          for (int lf_id = 0; lf_id < FRAME_LF_COUNT; ++lf_id) {
            int reduced_delta_lflevel =
                (mbmi->curr_delta_lf[lf_id] - xd->prev_delta_lf[lf_id]) /
                cm->delta_lf_res;
            write_delta_lflevel(cm, xd, lf_id, reduced_delta_lflevel, w);
            xd->prev_delta_lf[lf_id] = mbmi->curr_delta_lf[lf_id];
          }
        } else {
          int reduced_delta_lflevel =
              (mbmi->current_delta_lf_from_base - xd->prev_delta_lf_from_base) /
              cm->delta_lf_res;
          write_delta_lflevel(cm, xd, -1, reduced_delta_lflevel, w);
          xd->prev_delta_lf_from_base = mbmi->current_delta_lf_from_base;
        }
      }
#else
      if (cm->delta_lf_present_flag) {
        int reduced_delta_lflevel =
            (mbmi->current_delta_lf_from_base - xd->prev_delta_lf_from_base) /
            cm->delta_lf_res;
        write_delta_lflevel(cm, xd, reduced_delta_lflevel, w);
        xd->prev_delta_lf_from_base = mbmi->current_delta_lf_from_base;
      }
#endif  // CONFIG_LOOPFILTER_LEVEL
#endif  // CONFIG_EXT_DELTA_Q
    }
  }

#if CONFIG_SUPERTX
  if (!supertx_enabled)
#endif  // CONFIG_SUPERTX
    write_is_inter(cm, xd, mbmi->segment_id, w, is_inter);

  if (cm->tx_mode == TX_MODE_SELECT &&
#if CONFIG_CB4X4 && CONFIG_VAR_TX && !CONFIG_RECT_TX
      (bsize >= BLOCK_8X8 || (bsize > BLOCK_4X4 && is_inter)) &&
#else
      block_signals_txsize(bsize) &&
#endif
#if CONFIG_SUPERTX
      !supertx_enabled &&
#endif  // CONFIG_SUPERTX
      !(is_inter && skip) && !xd->lossless[segment_id]) {
#if CONFIG_VAR_TX
    if (is_inter) {  // This implies skip flag is 0.
      const TX_SIZE max_tx_size = get_vartx_max_txsize(mbmi, bsize, 0);
      const int bh = tx_size_high_unit[max_tx_size];
      const int bw = tx_size_wide_unit[max_tx_size];
      const int width = block_size_wide[bsize] >> tx_size_wide_log2[0];
      const int height = block_size_high[bsize] >> tx_size_wide_log2[0];
      int init_depth =
          (height != width) ? RECT_VARTX_DEPTH_INIT : SQR_VARTX_DEPTH_INIT;
      int idx, idy;
      for (idy = 0; idy < height; idy += bh)
        for (idx = 0; idx < width; idx += bw)
          write_tx_size_vartx(cm, xd, mbmi, max_tx_size, init_depth, idy, idx,
                              w);
#if CONFIG_RECT_TX_EXT
      if (is_quarter_tx_allowed(xd, mbmi, is_inter_block(mbmi)) &&
          quarter_txsize_lookup[bsize] != max_tx_size &&
          (mbmi->tx_size == quarter_txsize_lookup[bsize] ||
           mbmi->tx_size == max_tx_size)) {
#if CONFIG_NEW_MULTISYMBOL
        aom_write_symbol(w, mbmi->tx_size != max_tx_size,
                         cm->fc->quarter_tx_size_cdf, 2);
#else
        aom_write(w, mbmi->tx_size != max_tx_size,
                  cm->fc->quarter_tx_size_prob);
#endif
      }
#endif
    } else {
      set_txfm_ctxs(mbmi->tx_size, xd->n8_w, xd->n8_h, skip, xd);
      write_selected_tx_size(cm, xd, w);
    }
  } else {
    set_txfm_ctxs(mbmi->tx_size, xd->n8_w, xd->n8_h, skip, xd);
#else
    write_selected_tx_size(cm, xd, w);
#endif
  }

  if (!is_inter) {
    if (bsize >= BLOCK_8X8 || unify_bsize) {
      write_intra_mode(ec_ctx, bsize, mode, w);
    } else {
      int idx, idy;
      const int num_4x4_w = num_4x4_blocks_wide_lookup[bsize];
      const int num_4x4_h = num_4x4_blocks_high_lookup[bsize];
      for (idy = 0; idy < 2; idy += num_4x4_h) {
        for (idx = 0; idx < 2; idx += num_4x4_w) {
          const PREDICTION_MODE b_mode = mi->bmi[idy * 2 + idx].as_mode;
          write_intra_mode(ec_ctx, bsize, b_mode, w);
        }
      }
    }
#if CONFIG_CB4X4
    if (is_chroma_reference(mi_row, mi_col, bsize, xd->plane[1].subsampling_x,
                            xd->plane[1].subsampling_y)) {
      write_intra_uv_mode(ec_ctx, mbmi->uv_mode, mode, w);
#else  // !CONFIG_CB4X4
    write_intra_uv_mode(ec_ctx, mbmi->uv_mode, mode, w);
#endif  // CONFIG_CB4X4

#if CONFIG_CFL
      if (mbmi->uv_mode == UV_CFL_PRED) {
        write_cfl_alphas(ec_ctx, mbmi->cfl_alpha_idx, mbmi->cfl_alpha_signs, w);
      }
#endif

#if CONFIG_CB4X4
    }
#endif

#if CONFIG_EXT_INTRA
    write_intra_angle_info(xd, ec_ctx, w);
#endif  // CONFIG_EXT_INTRA
    if (av1_allow_palette(cm->allow_screen_content_tools, bsize))
      write_palette_mode_info(cm, xd, mi, w);
#if CONFIG_FILTER_INTRA
    if (bsize >= BLOCK_8X8 || unify_bsize)
      write_filter_intra_mode_info(cm, xd, mbmi, mi_row, mi_col, w);
#endif  // CONFIG_FILTER_INTRA
  } else {
    int16_t mode_ctx;
    write_ref_frames(cm, xd, w);

#if CONFIG_COMPOUND_SINGLEREF
    if (!segfeature_active(seg, segment_id, SEG_LVL_REF_FRAME)) {
      // NOTE: Handle single ref comp mode
      if (!is_compound)
        aom_write(w, is_inter_singleref_comp_mode(mode),
                  av1_get_inter_mode_prob(cm, xd));
    }
#endif  // CONFIG_COMPOUND_SINGLEREF

#if CONFIG_COMPOUND_SINGLEREF
    if (is_compound || is_inter_singleref_comp_mode(mode))
#else   // !CONFIG_COMPOUND_SINGLEREF
    if (is_compound)
#endif  // CONFIG_COMPOUND_SINGLEREF
      mode_ctx = mbmi_ext->compound_mode_context[mbmi->ref_frame[0]];
    else

      mode_ctx = av1_mode_context_analyzer(mbmi_ext->mode_context,
                                           mbmi->ref_frame, bsize, -1);

    // If segment skip is not enabled code the mode.
    if (!segfeature_active(seg, segment_id, SEG_LVL_SKIP)) {
      if (bsize >= BLOCK_8X8 || unify_bsize) {
        if (is_inter_compound_mode(mode))
          write_inter_compound_mode(cm, xd, w, mode, mode_ctx);
#if CONFIG_COMPOUND_SINGLEREF
        else if (is_inter_singleref_comp_mode(mode))
          write_inter_singleref_comp_mode(xd, w, mode, mode_ctx);
#endif  // CONFIG_COMPOUND_SINGLEREF
        else if (is_inter_singleref_mode(mode))
          write_inter_mode(w, mode, ec_ctx, mode_ctx);

        if (mode == NEWMV || mode == NEW_NEWMV ||
#if CONFIG_COMPOUND_SINGLEREF
            mbmi->mode == SR_NEW_NEWMV ||
#endif  // CONFIG_COMPOUND_SINGLEREF
            have_nearmv_in_inter_mode(mode))
          write_drl_idx(ec_ctx, mbmi, mbmi_ext, w);
        else
          assert(mbmi->ref_mv_idx == 0);
      }
    }

#if !CONFIG_DUAL_FILTER && !CONFIG_WARPED_MOTION && !CONFIG_GLOBAL_MOTION
    write_mb_interp_filter(cpi, xd, w);
#endif  // !CONFIG_DUAL_FILTER && !CONFIG_WARPED_MOTION

    if (bsize < BLOCK_8X8 && !unify_bsize) {
#if CONFIG_COMPOUND_SINGLEREF
      /// NOTE: Single ref comp mode does not support sub8x8.
      assert(is_compound || !is_inter_singleref_comp_mode(mbmi->mode));
#endif  // CONFIG_COMPOUND_SINGLEREF
      const int num_4x4_w = num_4x4_blocks_wide_lookup[bsize];
      const int num_4x4_h = num_4x4_blocks_high_lookup[bsize];
      int idx, idy;
      for (idy = 0; idy < 2; idy += num_4x4_h) {
        for (idx = 0; idx < 2; idx += num_4x4_w) {
          const int j = idy * 2 + idx;
          const PREDICTION_MODE b_mode = mi->bmi[j].as_mode;
          if (!is_compound)
            mode_ctx = av1_mode_context_analyzer(mbmi_ext->mode_context,
                                                 mbmi->ref_frame, bsize, j);
          if (is_inter_compound_mode(b_mode))
            write_inter_compound_mode(cm, xd, w, b_mode, mode_ctx);
          else if (is_inter_singleref_mode(b_mode))
            write_inter_mode(w, b_mode, ec_ctx, mode_ctx);

          if (b_mode == NEWMV || b_mode == NEW_NEWMV) {
            for (ref = 0; ref < 1 + is_compound; ++ref) {
              int8_t rf_type = av1_ref_frame_type(mbmi->ref_frame);
              int nmv_ctx = av1_nmv_ctx(mbmi_ext->ref_mv_count[rf_type],
                                        mbmi_ext->ref_mv_stack[rf_type], ref,
                                        mbmi->ref_mv_idx);
              nmv_context *nmvc = &ec_ctx->nmvc[nmv_ctx];
              av1_encode_mv(cpi, w, &mi->bmi[j].as_mv[ref].as_mv,
                            &mi->bmi[j].ref_mv[ref].as_mv, nmvc, allow_hp);
            }
          } else if (b_mode == NEAREST_NEWMV || b_mode == NEAR_NEWMV) {
            int8_t rf_type = av1_ref_frame_type(mbmi->ref_frame);
            int nmv_ctx = av1_nmv_ctx(mbmi_ext->ref_mv_count[rf_type],
                                      mbmi_ext->ref_mv_stack[rf_type], 1,
                                      mbmi->ref_mv_idx);
            nmv_context *nmvc = &ec_ctx->nmvc[nmv_ctx];
            av1_encode_mv(cpi, w, &mi->bmi[j].as_mv[1].as_mv,
                          &mi->bmi[j].ref_mv[1].as_mv, nmvc, allow_hp);
          } else if (b_mode == NEW_NEARESTMV || b_mode == NEW_NEARMV) {
            int8_t rf_type = av1_ref_frame_type(mbmi->ref_frame);
            int nmv_ctx = av1_nmv_ctx(mbmi_ext->ref_mv_count[rf_type],
                                      mbmi_ext->ref_mv_stack[rf_type], 0,
                                      mbmi->ref_mv_idx);
            nmv_context *nmvc = &ec_ctx->nmvc[nmv_ctx];
            av1_encode_mv(cpi, w, &mi->bmi[j].as_mv[0].as_mv,
                          &mi->bmi[j].ref_mv[0].as_mv, nmvc, allow_hp);
          }
        }
      }
    } else {
      if (mode == NEWMV || mode == NEW_NEWMV) {
        int_mv ref_mv;
        for (ref = 0; ref < 1 + is_compound; ++ref) {
          int8_t rf_type = av1_ref_frame_type(mbmi->ref_frame);
          int nmv_ctx = av1_nmv_ctx(mbmi_ext->ref_mv_count[rf_type],
                                    mbmi_ext->ref_mv_stack[rf_type], ref,
                                    mbmi->ref_mv_idx);
          nmv_context *nmvc = &ec_ctx->nmvc[nmv_ctx];
          ref_mv = mbmi_ext->ref_mvs[mbmi->ref_frame[ref]][0];
          av1_encode_mv(cpi, w, &mbmi->mv[ref].as_mv, &ref_mv.as_mv, nmvc,
                        allow_hp);
        }
      } else if (mode == NEAREST_NEWMV || mode == NEAR_NEWMV) {
        int8_t rf_type = av1_ref_frame_type(mbmi->ref_frame);
        int nmv_ctx =
            av1_nmv_ctx(mbmi_ext->ref_mv_count[rf_type],
                        mbmi_ext->ref_mv_stack[rf_type], 1, mbmi->ref_mv_idx);
        nmv_context *nmvc = &ec_ctx->nmvc[nmv_ctx];
        av1_encode_mv(cpi, w, &mbmi->mv[1].as_mv,
                      &mbmi_ext->ref_mvs[mbmi->ref_frame[1]][0].as_mv, nmvc,
                      allow_hp);
      } else if (mode == NEW_NEARESTMV || mode == NEW_NEARMV) {
        int8_t rf_type = av1_ref_frame_type(mbmi->ref_frame);
        int nmv_ctx =
            av1_nmv_ctx(mbmi_ext->ref_mv_count[rf_type],
                        mbmi_ext->ref_mv_stack[rf_type], 0, mbmi->ref_mv_idx);
        nmv_context *nmvc = &ec_ctx->nmvc[nmv_ctx];
        av1_encode_mv(cpi, w, &mbmi->mv[0].as_mv,
                      &mbmi_ext->ref_mvs[mbmi->ref_frame[0]][0].as_mv, nmvc,
                      allow_hp);
#if CONFIG_COMPOUND_SINGLEREF
      } else if (  //  mode == SR_NEAREST_NEWMV ||
          mode == SR_NEAR_NEWMV || mode == SR_ZERO_NEWMV ||
          mode == SR_NEW_NEWMV) {
        int8_t rf_type = av1_ref_frame_type(mbmi->ref_frame);
        int nmv_ctx =
            av1_nmv_ctx(mbmi_ext->ref_mv_count[rf_type],
                        mbmi_ext->ref_mv_stack[rf_type], 0, mbmi->ref_mv_idx);
        nmv_context *nmvc = &ec_ctx->nmvc[nmv_ctx];
        int_mv ref_mv = mbmi_ext->ref_mvs[mbmi->ref_frame[0]][0];
        if (mode == SR_NEW_NEWMV)
          av1_encode_mv(cpi, w, &mbmi->mv[0].as_mv, &ref_mv.as_mv, nmvc,
                        allow_hp);
        av1_encode_mv(cpi, w, &mbmi->mv[1].as_mv, &ref_mv.as_mv, nmvc,
                      allow_hp);
#endif  // CONFIG_COMPOUND_SINGLEREF
      }
    }

#if CONFIG_INTERINTRA
    if (cpi->common.reference_mode != COMPOUND_REFERENCE &&
#if CONFIG_SUPERTX
        !supertx_enabled &&
#endif  // CONFIG_SUPERTX
        cpi->common.allow_interintra_compound && is_interintra_allowed(mbmi)) {
      const int interintra = mbmi->ref_frame[1] == INTRA_FRAME;
      const int bsize_group = size_group_lookup[bsize];
#if CONFIG_NEW_MULTISYMBOL
      aom_write_symbol(w, interintra, ec_ctx->interintra_cdf[bsize_group], 2);
#else
      aom_write(w, interintra, cm->fc->interintra_prob[bsize_group]);
#endif
      if (interintra) {
        aom_write_symbol(w, mbmi->interintra_mode,
                         ec_ctx->interintra_mode_cdf[bsize_group],
                         INTERINTRA_MODES);
        if (is_interintra_wedge_used(bsize)) {
#if CONFIG_NEW_MULTISYMBOL
          aom_write_symbol(w, mbmi->use_wedge_interintra,
                           ec_ctx->wedge_interintra_cdf[bsize], 2);
#else
          aom_write(w, mbmi->use_wedge_interintra,
                    cm->fc->wedge_interintra_prob[bsize]);
#endif
          if (mbmi->use_wedge_interintra) {
            aom_write_literal(w, mbmi->interintra_wedge_index,
                              get_wedge_bits_lookup(bsize));
            assert(mbmi->interintra_wedge_sign == 0);
          }
        }
      }
    }
#endif  // CONFIG_INTERINTRA

#if CONFIG_MOTION_VAR || CONFIG_WARPED_MOTION
#if CONFIG_SUPERTX
    if (!supertx_enabled)
#endif  // CONFIG_SUPERTX
      if (mbmi->ref_frame[1] != INTRA_FRAME) write_motion_mode(cm, xd, mi, w);
#if CONFIG_NCOBMC_ADAPT_WEIGHT
    write_ncobmc_mode(xd, mi, w);
#endif
#endif  // CONFIG_MOTION_VAR || CONFIG_WARPED_MOTION

    if (
#if CONFIG_COMPOUND_SINGLEREF
        is_inter_anyref_comp_mode(mbmi->mode) &&
#else   // !CONFIG_COMPOUND_SINGLEREF
        cpi->common.reference_mode != SINGLE_REFERENCE &&
        is_inter_compound_mode(mbmi->mode) &&
#endif  // CONFIG_COMPOUND_SINGLEREF
#if CONFIG_MOTION_VAR
        mbmi->motion_mode == SIMPLE_TRANSLATION &&
#endif  // CONFIG_MOTION_VAR
        is_any_masked_compound_used(bsize)) {
#if CONFIG_COMPOUND_SEGMENT || CONFIG_WEDGE
      if (cm->allow_masked_compound) {
#if CONFIG_WEDGE && CONFIG_COMPOUND_SEGMENT
        if (!is_interinter_compound_used(COMPOUND_WEDGE, bsize))
          aom_write_bit(w, mbmi->interinter_compound_type == COMPOUND_AVERAGE);
        else
#endif  // CONFIG_WEDGE && CONFIG_COMPOUND_SEGMENT
          aom_write_symbol(w, mbmi->interinter_compound_type,
                           ec_ctx->compound_type_cdf[bsize], COMPOUND_TYPES);
#if CONFIG_WEDGE
        if (is_interinter_compound_used(COMPOUND_WEDGE, bsize) &&
            mbmi->interinter_compound_type == COMPOUND_WEDGE) {
          aom_write_literal(w, mbmi->wedge_index, get_wedge_bits_lookup(bsize));
          aom_write_bit(w, mbmi->wedge_sign);
        }
#endif  // CONFIG_WEDGE
#if CONFIG_COMPOUND_SEGMENT
        if (mbmi->interinter_compound_type == COMPOUND_SEG) {
          aom_write_literal(w, mbmi->mask_type, MAX_SEG_MASK_BITS);
        }
#endif  // CONFIG_COMPOUND_SEGMENT
      }
#endif  // CONFIG_COMPOUND_SEGMENT || CONFIG_WEDGE
    }

#if CONFIG_DUAL_FILTER || CONFIG_WARPED_MOTION || CONFIG_GLOBAL_MOTION
    write_mb_interp_filter(cpi, xd, w);
#endif  // CONFIG_DUAL_FILTE || CONFIG_WARPED_MOTION
  }

#if !CONFIG_TXK_SEL
  av1_write_tx_type(cm, xd,
#if CONFIG_SUPERTX
                    supertx_enabled,
#endif
                    w);
#endif  // !CONFIG_TXK_SEL
}

static void write_mb_modes_kf(AV1_COMMON *cm, MACROBLOCKD *xd,
#if CONFIG_INTRABC
                              const MB_MODE_INFO_EXT *mbmi_ext,
#endif  // CONFIG_INTRABC
                              const int mi_row, const int mi_col,
                              aom_writer *w) {
  FRAME_CONTEXT *ec_ctx = xd->tile_ctx;
  const struct segmentation *const seg = &cm->seg;
  struct segmentation_probs *const segp = &ec_ctx->seg;
  const MODE_INFO *const mi = xd->mi[0];
  const MODE_INFO *const above_mi = xd->above_mi;
  const MODE_INFO *const left_mi = xd->left_mi;
  const MB_MODE_INFO *const mbmi = &mi->mbmi;
  const BLOCK_SIZE bsize = mbmi->sb_type;
#if CONFIG_CB4X4
  const int unify_bsize = 1;
#else
  const int unify_bsize = 0;
#endif
  (void)mi_row;
  (void)mi_col;

  if (seg->update_map) write_segment_id(w, seg, segp, mbmi->segment_id);

  const int skip = write_skip(cm, xd, mbmi->segment_id, mi, w);
  if (cm->delta_q_present_flag) {
    int super_block_upper_left =
        ((mi_row & MAX_MIB_MASK) == 0) && ((mi_col & MAX_MIB_MASK) == 0);
    if ((bsize != BLOCK_LARGEST || skip == 0) && super_block_upper_left) {
      assert(mbmi->current_q_index > 0);
      int reduced_delta_qindex =
          (mbmi->current_q_index - xd->prev_qindex) / cm->delta_q_res;
      write_delta_qindex(cm, xd, reduced_delta_qindex, w);
      xd->prev_qindex = mbmi->current_q_index;
#if CONFIG_EXT_DELTA_Q
#if CONFIG_LOOPFILTER_LEVEL
      if (cm->delta_lf_present_flag) {
        if (cm->delta_lf_multi) {
          for (int lf_id = 0; lf_id < FRAME_LF_COUNT; ++lf_id) {
            int reduced_delta_lflevel =
                (mbmi->curr_delta_lf[lf_id] - xd->prev_delta_lf[lf_id]) /
                cm->delta_lf_res;
            write_delta_lflevel(cm, xd, lf_id, reduced_delta_lflevel, w);
            xd->prev_delta_lf[lf_id] = mbmi->curr_delta_lf[lf_id];
          }
        } else {
          int reduced_delta_lflevel =
              (mbmi->current_delta_lf_from_base - xd->prev_delta_lf_from_base) /
              cm->delta_lf_res;
          write_delta_lflevel(cm, xd, -1, reduced_delta_lflevel, w);
          xd->prev_delta_lf_from_base = mbmi->current_delta_lf_from_base;
        }
      }
#else
      if (cm->delta_lf_present_flag) {
        int reduced_delta_lflevel =
            (mbmi->current_delta_lf_from_base - xd->prev_delta_lf_from_base) /
            cm->delta_lf_res;
        write_delta_lflevel(cm, xd, reduced_delta_lflevel, w);
        xd->prev_delta_lf_from_base = mbmi->current_delta_lf_from_base;
      }
#endif  // CONFIG_LOOPFILTER_LEVEL
#endif  // CONFIG_EXT_DELTA_Q
    }
  }

  int enable_tx_size = cm->tx_mode == TX_MODE_SELECT &&
                       block_signals_txsize(bsize) &&
                       !xd->lossless[mbmi->segment_id];

#if CONFIG_INTRABC
  if (av1_allow_intrabc(bsize, cm)) {
    int use_intrabc = is_intrabc_block(mbmi);
    aom_write_symbol(w, use_intrabc, ec_ctx->intrabc_cdf, 2);
    if (use_intrabc) {
      assert(mbmi->mode == DC_PRED);
      assert(mbmi->uv_mode == UV_DC_PRED);
      if (enable_tx_size && !mbmi->skip) write_selected_tx_size(cm, xd, w);
      int_mv dv_ref = mbmi_ext->ref_mvs[INTRA_FRAME][0];
      av1_encode_dv(w, &mbmi->mv[0].as_mv, &dv_ref.as_mv, &ec_ctx->ndvc);
#if CONFIG_EXT_TX && !CONFIG_TXK_SEL
      av1_write_tx_type(cm, xd,
#if CONFIG_SUPERTX
                        0,
#endif
                        w);
#endif  // CONFIG_EXT_TX && !CONFIG_TXK_SEL
      return;
    }
  }
#endif  // CONFIG_INTRABC
  if (enable_tx_size) write_selected_tx_size(cm, xd, w);

  if (bsize >= BLOCK_8X8 || unify_bsize) {
    write_intra_mode_kf(cm, ec_ctx, mi, above_mi, left_mi, 0, mbmi->mode, w);
  } else {
    const int num_4x4_w = num_4x4_blocks_wide_lookup[bsize];
    const int num_4x4_h = num_4x4_blocks_high_lookup[bsize];
    int idx, idy;

    for (idy = 0; idy < 2; idy += num_4x4_h) {
      for (idx = 0; idx < 2; idx += num_4x4_w) {
        const int block = idy * 2 + idx;
        write_intra_mode_kf(cm, ec_ctx, mi, above_mi, left_mi, block,
                            mi->bmi[block].as_mode, w);
      }
    }
  }

#if CONFIG_CB4X4
  if (is_chroma_reference(mi_row, mi_col, bsize, xd->plane[1].subsampling_x,
                          xd->plane[1].subsampling_y)) {
    write_intra_uv_mode(ec_ctx, mbmi->uv_mode, mbmi->mode, w);
#else  // !CONFIG_CB4X4
  write_intra_uv_mode(ec_ctx, mbmi->uv_mode, mbmi->mode, w);
#endif  // CONFIG_CB4X4

#if CONFIG_CFL
    if (mbmi->uv_mode == UV_CFL_PRED) {
      write_cfl_alphas(ec_ctx, mbmi->cfl_alpha_idx, mbmi->cfl_alpha_signs, w);
    }
#endif

#if CONFIG_CB4X4
  }
#endif
#if CONFIG_EXT_INTRA
  write_intra_angle_info(xd, ec_ctx, w);
#endif  // CONFIG_EXT_INTRA
  if (av1_allow_palette(cm->allow_screen_content_tools, bsize))
    write_palette_mode_info(cm, xd, mi, w);
#if CONFIG_FILTER_INTRA
  if (bsize >= BLOCK_8X8 || unify_bsize)
    write_filter_intra_mode_info(cm, xd, mbmi, mi_row, mi_col, w);
#endif  // CONFIG_FILTER_INTRA

#if !CONFIG_TXK_SEL
  av1_write_tx_type(cm, xd,
#if CONFIG_SUPERTX
                    0,
#endif
                    w);
#endif  // !CONFIG_TXK_SEL
}

#if CONFIG_SUPERTX
#define write_modes_b_wrapper(cpi, tile, w, tok, tok_end, supertx_enabled, \
                              mi_row, mi_col)                              \
  write_modes_b(cpi, tile, w, tok, tok_end, supertx_enabled, mi_row, mi_col)
#else
#define write_modes_b_wrapper(cpi, tile, w, tok, tok_end, supertx_enabled, \
                              mi_row, mi_col)                              \
  write_modes_b(cpi, tile, w, tok, tok_end, mi_row, mi_col)
#endif  // CONFIG_SUPERTX

#if CONFIG_RD_DEBUG
static void dump_mode_info(MODE_INFO *mi) {
  printf("\nmi->mbmi.mi_row == %d\n", mi->mbmi.mi_row);
  printf("&& mi->mbmi.mi_col == %d\n", mi->mbmi.mi_col);
  printf("&& mi->mbmi.sb_type == %d\n", mi->mbmi.sb_type);
  printf("&& mi->mbmi.tx_size == %d\n", mi->mbmi.tx_size);
  if (mi->mbmi.sb_type >= BLOCK_8X8) {
    printf("&& mi->mbmi.mode == %d\n", mi->mbmi.mode);
  } else {
    printf("&& mi->bmi[0].as_mode == %d\n", mi->bmi[0].as_mode);
  }
}
static int rd_token_stats_mismatch(RD_STATS *rd_stats, TOKEN_STATS *token_stats,
                                   int plane) {
  if (rd_stats->txb_coeff_cost[plane] != token_stats->cost) {
#if CONFIG_VAR_TX
    int r, c;
#endif
    printf("\nplane %d rd_stats->txb_coeff_cost %d token_stats->cost %d\n",
           plane, rd_stats->txb_coeff_cost[plane], token_stats->cost);
#if CONFIG_VAR_TX
    printf("rd txb_coeff_cost_map\n");
    for (r = 0; r < TXB_COEFF_COST_MAP_SIZE; ++r) {
      for (c = 0; c < TXB_COEFF_COST_MAP_SIZE; ++c) {
        printf("%d ", rd_stats->txb_coeff_cost_map[plane][r][c]);
      }
      printf("\n");
    }

    printf("pack txb_coeff_cost_map\n");
    for (r = 0; r < TXB_COEFF_COST_MAP_SIZE; ++r) {
      for (c = 0; c < TXB_COEFF_COST_MAP_SIZE; ++c) {
        printf("%d ", token_stats->txb_coeff_cost_map[r][c]);
      }
      printf("\n");
    }
#endif
    return 1;
  }
  return 0;
}
#endif

#if ENC_MISMATCH_DEBUG
static void enc_dump_logs(AV1_COMP *cpi, int mi_row, int mi_col) {
  AV1_COMMON *const cm = &cpi->common;
  MACROBLOCKD *const xd = &cpi->td.mb.e_mbd;
  MODE_INFO *m;
  xd->mi = cm->mi_grid_visible + (mi_row * cm->mi_stride + mi_col);
  m = xd->mi[0];
  if (is_inter_block(&m->mbmi)) {
#define FRAME_TO_CHECK 1
    if (cm->current_video_frame == FRAME_TO_CHECK && cm->show_frame == 1) {
      const MB_MODE_INFO *const mbmi = &m->mbmi;
      const BLOCK_SIZE bsize = mbmi->sb_type;

      int_mv mv[2];
      int is_comp_ref = has_second_ref(&m->mbmi);
      int ref;

      for (ref = 0; ref < 1 + is_comp_ref; ++ref)
        mv[ref].as_mv = m->mbmi.mv[ref].as_mv;

      if (!is_comp_ref) {
#if CONFIG_COMPOUND_SINGLEREF
        if (is_inter_singleref_comp_mode(m->mbmi.mode))
          mv[1].as_mv = m->mbmi.mv[1].as_mv;
        else
#endif  // CONFIG_COMPOUND_SINGLEREF
          mv[1].as_int = 0;
      }

      MACROBLOCK *const x = &cpi->td.mb;
      const MB_MODE_INFO_EXT *const mbmi_ext = x->mbmi_ext;
      const int16_t mode_ctx = av1_mode_context_analyzer(
          mbmi_ext->mode_context, mbmi->ref_frame, bsize, -1);
      const int16_t newmv_ctx = mode_ctx & NEWMV_CTX_MASK;
      int16_t zeromv_ctx = -1;
      int16_t refmv_ctx = -1;
      if (mbmi->mode != NEWMV) {
        zeromv_ctx = (mode_ctx >> ZEROMV_OFFSET) & ZEROMV_CTX_MASK;
        if (mode_ctx & (1 << ALL_ZERO_FLAG_OFFSET)) {
          assert(mbmi->mode == ZEROMV);
        }
        if (mbmi->mode != ZEROMV) {
          refmv_ctx = (mode_ctx >> REFMV_OFFSET) & REFMV_CTX_MASK;
          if (mode_ctx & (1 << SKIP_NEARESTMV_OFFSET)) refmv_ctx = 6;
          if (mode_ctx & (1 << SKIP_NEARMV_OFFSET)) refmv_ctx = 7;
          if (mode_ctx & (1 << SKIP_NEARESTMV_SUB8X8_OFFSET)) refmv_ctx = 8;
        }
      }

      int8_t ref_frame_type = av1_ref_frame_type(mbmi->ref_frame);
      printf(
          "=== ENCODER ===: "
          "Frame=%d, (mi_row,mi_col)=(%d,%d), mode=%d, bsize=%d, "
          "show_frame=%d, mv[0]=(%d,%d), mv[1]=(%d,%d), ref[0]=%d, "
          "ref[1]=%d, motion_mode=%d, inter_mode_ctx=%d, mode_ctx=%d, "
          "newmv_ctx=%d, zeromv_ctx=%d, refmv_ctx=%d\n",
          cm->current_video_frame, mi_row, mi_col, mbmi->mode, bsize,
          cm->show_frame, mv[0].as_mv.row, mv[0].as_mv.col, mv[1].as_mv.row,
          mv[1].as_mv.col, mbmi->ref_frame[0], mbmi->ref_frame[1],
          mbmi->motion_mode, mbmi_ext->mode_context[ref_frame_type], mode_ctx,
          newmv_ctx, zeromv_ctx, refmv_ctx);
    }
  }
}
#endif  // ENC_MISMATCH_DEBUG

static void write_mbmi_b(AV1_COMP *cpi, const TileInfo *const tile,
                         aom_writer *w,
#if CONFIG_SUPERTX
                         int supertx_enabled,
#endif
                         int mi_row, int mi_col) {
  AV1_COMMON *const cm = &cpi->common;
  MACROBLOCKD *const xd = &cpi->td.mb.e_mbd;
  MODE_INFO *m;
  int bh, bw;
  xd->mi = cm->mi_grid_visible + (mi_row * cm->mi_stride + mi_col);
  m = xd->mi[0];

  assert(m->mbmi.sb_type <= cm->sb_size ||
         (m->mbmi.sb_type >= BLOCK_SIZES && m->mbmi.sb_type < BLOCK_SIZES_ALL));

  bh = mi_size_high[m->mbmi.sb_type];
  bw = mi_size_wide[m->mbmi.sb_type];

  cpi->td.mb.mbmi_ext = cpi->mbmi_ext_base + (mi_row * cm->mi_cols + mi_col);

  set_mi_row_col(xd, tile, mi_row, bh, mi_col, bw,
#if CONFIG_DEPENDENT_HORZTILES
                 cm->dependent_horz_tiles,
#endif  // CONFIG_DEPENDENT_HORZTILES
                 cm->mi_rows, cm->mi_cols);

  if (frame_is_intra_only(cm)) {
    write_mb_modes_kf(cm, xd,
#if CONFIG_INTRABC
                      cpi->td.mb.mbmi_ext,
#endif  // CONFIG_INTRABC
                      mi_row, mi_col, w);
  } else {
#if CONFIG_VAR_TX
    xd->above_txfm_context =
        cm->above_txfm_context + (mi_col << TX_UNIT_WIDE_LOG2);
    xd->left_txfm_context = xd->left_txfm_context_buffer +
                            ((mi_row & MAX_MIB_MASK) << TX_UNIT_HIGH_LOG2);
#endif
#if CONFIG_DUAL_FILTER || CONFIG_WARPED_MOTION
    // has_subpel_mv_component needs the ref frame buffers set up to look
    // up if they are scaled. has_subpel_mv_component is in turn needed by
    // write_switchable_interp_filter, which is called by pack_inter_mode_mvs.
    set_ref_ptrs(cm, xd, m->mbmi.ref_frame[0], m->mbmi.ref_frame[1]);
#if CONFIG_COMPOUND_SINGLEREF
    if (!has_second_ref(&m->mbmi) && is_inter_singleref_comp_mode(m->mbmi.mode))
      xd->block_refs[1] = xd->block_refs[0];
#endif  // CONFIG_COMPOUND_SINGLEREF
#endif  // CONFIG_DUAL_FILTER || CONFIG_WARPED_MOTION

#if ENC_MISMATCH_DEBUG
    enc_dump_logs(cpi, mi_row, mi_col);
#endif  // ENC_MISMATCH_DEBUG

    pack_inter_mode_mvs(cpi, mi_row, mi_col,
#if CONFIG_SUPERTX
                        supertx_enabled,
#endif
                        w);
  }
}

static void write_tokens_b(AV1_COMP *cpi, const TileInfo *const tile,
                           aom_writer *w, const TOKENEXTRA **tok,
                           const TOKENEXTRA *const tok_end, int mi_row,
                           int mi_col) {
  AV1_COMMON *const cm = &cpi->common;
  MACROBLOCKD *const xd = &cpi->td.mb.e_mbd;
  const int mi_offset = mi_row * cm->mi_stride + mi_col;
  MODE_INFO *const m = *(cm->mi_grid_visible + mi_offset);
  MB_MODE_INFO *const mbmi = &m->mbmi;
  int plane;
  int bh, bw;
#if CONFIG_PVQ || CONFIG_LV_MAP
  MACROBLOCK *const x = &cpi->td.mb;
  (void)tok;
  (void)tok_end;
#endif
  xd->mi = cm->mi_grid_visible + mi_offset;

  assert(mbmi->sb_type <= cm->sb_size ||
         (mbmi->sb_type >= BLOCK_SIZES && mbmi->sb_type < BLOCK_SIZES_ALL));

  bh = mi_size_high[mbmi->sb_type];
  bw = mi_size_wide[mbmi->sb_type];
  cpi->td.mb.mbmi_ext = cpi->mbmi_ext_base + (mi_row * cm->mi_cols + mi_col);

  set_mi_row_col(xd, tile, mi_row, bh, mi_col, bw,
#if CONFIG_DEPENDENT_HORZTILES
                 cm->dependent_horz_tiles,
#endif  // CONFIG_DEPENDENT_HORZTILES
                 cm->mi_rows, cm->mi_cols);

// TODO(anybody) : remove this flag when PVQ supports pallete coding tool
#if !CONFIG_PVQ
  for (plane = 0; plane <= 1; ++plane) {
    const uint8_t palette_size_plane =
        mbmi->palette_mode_info.palette_size[plane];
    if (palette_size_plane > 0) {
#if CONFIG_INTRABC
      assert(mbmi->use_intrabc == 0);
#endif
      int rows, cols;
      assert(mbmi->sb_type >= BLOCK_8X8);
      av1_get_block_dimensions(mbmi->sb_type, plane, xd, NULL, NULL, &rows,
                               &cols);
      assert(*tok < tok_end);
      pack_map_tokens(w, tok, palette_size_plane, rows * cols);
#if !CONFIG_LV_MAP
      assert(*tok < tok_end + mbmi->skip);
#endif  // !CONFIG_LV_MAP
    }
  }
#endif  // !CONFIG_PVQ

#if CONFIG_COEF_INTERLEAVE
  if (!mbmi->skip) {
    const struct macroblockd_plane *const pd_y = &xd->plane[0];
    const struct macroblockd_plane *const pd_c = &xd->plane[1];
    const TX_SIZE tx_log2_y = mbmi->tx_size;
    const TX_SIZE tx_log2_c = av1_get_uv_tx_size(mbmi, pd_c);
    const int tx_sz_y = (1 << tx_log2_y);
    const int tx_sz_c = (1 << tx_log2_c);

    const BLOCK_SIZE plane_bsize_y =
        get_plane_block_size(AOMMAX(mbmi->sb_type, 3), pd_y);
    const BLOCK_SIZE plane_bsize_c =
        get_plane_block_size(AOMMAX(mbmi->sb_type, 3), pd_c);

    const int num_4x4_w_y = num_4x4_blocks_wide_lookup[plane_bsize_y];
    const int num_4x4_w_c = num_4x4_blocks_wide_lookup[plane_bsize_c];
    const int num_4x4_h_y = num_4x4_blocks_high_lookup[plane_bsize_y];
    const int num_4x4_h_c = num_4x4_blocks_high_lookup[plane_bsize_c];

    const int max_4x4_w_y = get_max_4x4_size(num_4x4_w_y, xd->mb_to_right_edge,
                                             pd_y->subsampling_x);
    const int max_4x4_h_y = get_max_4x4_size(num_4x4_h_y, xd->mb_to_bottom_edge,
                                             pd_y->subsampling_y);
    const int max_4x4_w_c = get_max_4x4_size(num_4x4_w_c, xd->mb_to_right_edge,
                                             pd_c->subsampling_x);
    const int max_4x4_h_c = get_max_4x4_size(num_4x4_h_c, xd->mb_to_bottom_edge,
                                             pd_c->subsampling_y);

    // The max_4x4_w/h may be smaller than tx_sz under some corner cases,
    // i.e. when the SB is splitted by tile boundaries.
    const int tu_num_w_y = (max_4x4_w_y + tx_sz_y - 1) / tx_sz_y;
    const int tu_num_h_y = (max_4x4_h_y + tx_sz_y - 1) / tx_sz_y;
    const int tu_num_w_c = (max_4x4_w_c + tx_sz_c - 1) / tx_sz_c;
    const int tu_num_h_c = (max_4x4_h_c + tx_sz_c - 1) / tx_sz_c;
    const int tu_num_y = tu_num_w_y * tu_num_h_y;
    const int tu_num_c = tu_num_w_c * tu_num_h_c;

    int tu_idx_y = 0, tu_idx_c = 0;
    TOKEN_STATS token_stats;
    init_token_stats(&token_stats);

    assert(*tok < tok_end);

    while (tu_idx_y < tu_num_y) {
      pack_mb_tokens(w, tok, tok_end, cm->bit_depth, tx_log2_y, &token_stats);
      assert(*tok < tok_end && (*tok)->token == EOSB_TOKEN);
      (*tok)++;
      tu_idx_y++;

      if (tu_idx_c < tu_num_c) {
        pack_mb_tokens(w, tok, tok_end, cm->bit_depth, tx_log2_c, &token_stats);
        assert(*tok < tok_end && (*tok)->token == EOSB_TOKEN);
        (*tok)++;

        pack_mb_tokens(w, tok, tok_end, cm->bit_depth, tx_log2_c, &token_stats);
        assert(*tok < tok_end && (*tok)->token == EOSB_TOKEN);
        (*tok)++;

        tu_idx_c++;
      }
    }

    // In 422 case, it's possilbe that Chroma has more TUs than Luma
    while (tu_idx_c < tu_num_c) {
      pack_mb_tokens(w, tok, tok_end, cm->bit_depth, tx_log2_c, &token_stats);
      assert(*tok < tok_end && (*tok)->token == EOSB_TOKEN);
      (*tok)++;

      pack_mb_tokens(w, tok, tok_end, cm->bit_depth, tx_log2_c, &token_stats);
      assert(*tok < tok_end && (*tok)->token == EOSB_TOKEN);
      (*tok)++;

      tu_idx_c++;
    }
  }
#else  // CONFIG_COEF_INTERLEAVE
  if (!mbmi->skip) {
#if !CONFIG_PVQ && !CONFIG_LV_MAP
    assert(*tok < tok_end);
#endif
    for (plane = 0; plane < MAX_MB_PLANE; ++plane) {
#if CONFIG_CB4X4
      if (!is_chroma_reference(mi_row, mi_col, mbmi->sb_type,
                               xd->plane[plane].subsampling_x,
                               xd->plane[plane].subsampling_y)) {
#if !CONFIG_LV_MAP
        (*tok)++;
#endif  // !CONFIG_LV_MAP
        continue;
      }
#endif
#if CONFIG_VAR_TX
      const struct macroblockd_plane *const pd = &xd->plane[plane];
      BLOCK_SIZE bsize = mbmi->sb_type;
#if CONFIG_CHROMA_SUB8X8
      const BLOCK_SIZE plane_bsize =
          AOMMAX(BLOCK_4X4, get_plane_block_size(bsize, pd));
#elif CONFIG_CB4X4
      const BLOCK_SIZE plane_bsize = get_plane_block_size(bsize, pd);
#else
      const BLOCK_SIZE plane_bsize =
          get_plane_block_size(AOMMAX(bsize, BLOCK_8X8), pd);
#endif

      const int num_4x4_w =
          block_size_wide[plane_bsize] >> tx_size_wide_log2[0];
      const int num_4x4_h =
          block_size_high[plane_bsize] >> tx_size_wide_log2[0];
      int row, col;
      TOKEN_STATS token_stats;
      init_token_stats(&token_stats);

      const BLOCK_SIZE max_unit_bsize = get_plane_block_size(BLOCK_64X64, pd);
      int mu_blocks_wide =
          block_size_wide[max_unit_bsize] >> tx_size_wide_log2[0];
      int mu_blocks_high =
          block_size_high[max_unit_bsize] >> tx_size_high_log2[0];

      mu_blocks_wide = AOMMIN(num_4x4_w, mu_blocks_wide);
      mu_blocks_high = AOMMIN(num_4x4_h, mu_blocks_high);

      if (is_inter_block(mbmi)) {
        const TX_SIZE max_tx_size = get_vartx_max_txsize(
            mbmi, plane_bsize, pd->subsampling_x || pd->subsampling_y);
        int block = 0;
        const int step =
            tx_size_wide_unit[max_tx_size] * tx_size_high_unit[max_tx_size];
        const int bkw = tx_size_wide_unit[max_tx_size];
        const int bkh = tx_size_high_unit[max_tx_size];
        assert(bkw <= mu_blocks_wide);
        assert(bkh <= mu_blocks_high);
        for (row = 0; row < num_4x4_h; row += mu_blocks_high) {
          const int unit_height = AOMMIN(mu_blocks_high + row, num_4x4_h);
          for (col = 0; col < num_4x4_w; col += mu_blocks_wide) {
            int blk_row, blk_col;
            const int unit_width = AOMMIN(mu_blocks_wide + col, num_4x4_w);
            for (blk_row = row; blk_row < unit_height; blk_row += bkh) {
              for (blk_col = col; blk_col < unit_width; blk_col += bkw) {
                pack_txb_tokens(w,
#if CONFIG_LV_MAP
                                cm,
#endif
                                tok, tok_end,
#if CONFIG_PVQ || CONFIG_LV_MAP
                                x,
#endif
                                xd, mbmi, plane, plane_bsize, cm->bit_depth,
                                block, blk_row, blk_col, max_tx_size,
                                &token_stats);
                block += step;
              }
            }
          }
        }
#if CONFIG_RD_DEBUG
        if (mbmi->sb_type >= BLOCK_8X8 &&
            rd_token_stats_mismatch(&mbmi->rd_stats, &token_stats, plane)) {
          dump_mode_info(m);
          assert(0);
        }
#endif  // CONFIG_RD_DEBUG
      } else {
#if CONFIG_LV_MAP
        av1_write_coeffs_mb(cm, x, w, plane);
#else
        const TX_SIZE tx = av1_get_tx_size(plane, xd);
        const int bkw = tx_size_wide_unit[tx];
        const int bkh = tx_size_high_unit[tx];
        int blk_row, blk_col;

        for (row = 0; row < num_4x4_h; row += mu_blocks_high) {
          for (col = 0; col < num_4x4_w; col += mu_blocks_wide) {
            const int unit_height = AOMMIN(mu_blocks_high + row, num_4x4_h);
            const int unit_width = AOMMIN(mu_blocks_wide + col, num_4x4_w);

            for (blk_row = row; blk_row < unit_height; blk_row += bkh) {
              for (blk_col = col; blk_col < unit_width; blk_col += bkw) {
#if !CONFIG_PVQ
#if CONFIG_MRC_TX && SIGNAL_ANY_MRC_MASK
                TX_TYPE tx_type =
                    av1_get_tx_type(plane ? PLANE_TYPE_UV : PLANE_TYPE_Y, xd,
                                    blk_row, blk_col, 0, tx);
#endif  // CONFIG_MRC_TX && SIGNAL_ANY_MRC_MASK
                pack_mb_tokens(w, tok, tok_end, cm->bit_depth, tx,
#if CONFIG_MRC_TX && SIGNAL_ANY_MRC_MASK
                               tx_type, is_inter_block(mbmi),
#endif  // CONFIG_MRC_TX && SIGNAL_ANY_MRC_MASK
                               &token_stats);
#else
                pack_pvq_tokens(w, x, xd, plane, bsize, tx);
#endif
              }
            }
          }
        }
#endif  // CONFIG_LV_MAP
      }
#else
      const TX_SIZE tx = av1_get_tx_size(plane, xd);
      TOKEN_STATS token_stats;
#if !CONFIG_PVQ
      init_token_stats(&token_stats);
#if CONFIG_LV_MAP
      (void)tx;
      av1_write_coeffs_mb(cm, x, w, plane);
#else  // CONFIG_LV_MAP
#if CONFIG_MRC_TX && SIGNAL_ANY_MRC_MASK
      TX_TYPE tx_type = av1_get_tx_type(plane ? PLANE_TYPE_UV : PLANE_TYPE_Y,
                                        xd, blk_row, blk_col, 0, tx);
#endif  // CONFIG_MRC_TX && SIGNAL_ANY_MRC_MASK
      pack_mb_tokens(w, tok, tok_end, cm->bit_depth, tx,
#if CONFIG_MRC_TX && SIGNAL_ANY_MRC_MASK
                     tx_type, is_inter_block(mbmi),
#endif  // CONFIG_MRC_TX && SIGNAL_ANY_MRC_MASK
                     &token_stats);
#endif  // CONFIG_LV_MAP

#else
      (void)token_stats;
      pack_pvq_tokens(w, x, xd, plane, mbmi->sb_type, tx);
#endif
#if CONFIG_RD_DEBUG
      if (is_inter_block(mbmi) && mbmi->sb_type >= BLOCK_8X8 &&
          rd_token_stats_mismatch(&mbmi->rd_stats, &token_stats, plane)) {
        dump_mode_info(m);
        assert(0);
      }
#endif  // CONFIG_RD_DEBUG
#endif  // CONFIG_VAR_TX

#if !CONFIG_PVQ && !CONFIG_LV_MAP
      assert(*tok < tok_end && (*tok)->token == EOSB_TOKEN);
      (*tok)++;
#endif
    }
  }
#endif  // CONFIG_COEF_INTERLEAVE
}

#if CONFIG_MOTION_VAR && NC_MODE_INFO
static void write_tokens_sb(AV1_COMP *cpi, const TileInfo *const tile,
                            aom_writer *w, const TOKENEXTRA **tok,
                            const TOKENEXTRA *const tok_end, int mi_row,
                            int mi_col, BLOCK_SIZE bsize) {
  const AV1_COMMON *const cm = &cpi->common;
  const int hbs = mi_size_wide[bsize] / 2;
  PARTITION_TYPE partition;
  BLOCK_SIZE subsize;
#if CONFIG_CB4X4
  const int unify_bsize = 1;
#else
  const int unify_bsize = 0;
#endif

  if (mi_row >= cm->mi_rows || mi_col >= cm->mi_cols) return;

  partition = get_partition(cm, mi_row, mi_col, bsize);
  subsize = get_subsize(bsize, partition);

  if (subsize < BLOCK_8X8 && !unify_bsize) {
    write_tokens_b(cpi, tile, w, tok, tok_end, mi_row, mi_col);
  } else {
    switch (partition) {
      case PARTITION_NONE:
        write_tokens_b(cpi, tile, w, tok, tok_end, mi_row, mi_col);
        break;
      case PARTITION_HORZ:
        write_tokens_b(cpi, tile, w, tok, tok_end, mi_row, mi_col);
        if (mi_row + hbs < cm->mi_rows)
          write_tokens_b(cpi, tile, w, tok, tok_end, mi_row + hbs, mi_col);
        break;
      case PARTITION_VERT:
        write_tokens_b(cpi, tile, w, tok, tok_end, mi_row, mi_col);
        if (mi_col + hbs < cm->mi_cols)
          write_tokens_b(cpi, tile, w, tok, tok_end, mi_row, mi_col + hbs);
        break;
      case PARTITION_SPLIT:
        write_tokens_sb(cpi, tile, w, tok, tok_end, mi_row, mi_col, subsize);
        write_tokens_sb(cpi, tile, w, tok, tok_end, mi_row, mi_col + hbs,
                        subsize);
        write_tokens_sb(cpi, tile, w, tok, tok_end, mi_row + hbs, mi_col,
                        subsize);
        write_tokens_sb(cpi, tile, w, tok, tok_end, mi_row + hbs, mi_col + hbs,
                        subsize);
        break;
#if CONFIG_EXT_PARTITION_TYPES
#if CONFIG_EXT_PARTITION_TYPES_AB
#error NC_MODE_INFO+MOTION_VAR not yet supported for new HORZ/VERT_AB partitions
#endif
      case PARTITION_HORZ_A:
        write_tokens_b(cpi, tile, w, tok, tok_end, mi_row, mi_col);
        write_tokens_b(cpi, tile, w, tok, tok_end, mi_row, mi_col + hbs);
        write_tokens_b(cpi, tile, w, tok, tok_end, mi_row + hbs, mi_col);
        break;
      case PARTITION_HORZ_B:
        write_tokens_b(cpi, tile, w, tok, tok_end, mi_row, mi_col);
        write_tokens_b(cpi, tile, w, tok, tok_end, mi_row + hbs, mi_col);
        write_tokens_b(cpi, tile, w, tok, tok_end, mi_row + hbs, mi_col + hbs);
        break;
      case PARTITION_VERT_A:
        write_tokens_b(cpi, tile, w, tok, tok_end, mi_row, mi_col);
        write_tokens_b(cpi, tile, w, tok, tok_end, mi_row + hbs, mi_col);
        write_tokens_b(cpi, tile, w, tok, tok_end, mi_row, mi_col + hbs);
        break;
      case PARTITION_VERT_B:
        write_tokens_b(cpi, tile, w, tok, tok_end, mi_row, mi_col);
        write_tokens_b(cpi, tile, w, tok, tok_end, mi_row, mi_col + hbs);
        write_tokens_b(cpi, tile, w, tok, tok_end, mi_row + hbs, mi_col + hbs);
        break;
#endif  // CONFIG_EXT_PARTITION_TYPES
      default: assert(0);
    }
  }
}
#endif

static void write_modes_b(AV1_COMP *cpi, const TileInfo *const tile,
                          aom_writer *w, const TOKENEXTRA **tok,
                          const TOKENEXTRA *const tok_end,
#if CONFIG_SUPERTX
                          int supertx_enabled,
#endif
                          int mi_row, int mi_col) {
  write_mbmi_b(cpi, tile, w,
#if CONFIG_SUPERTX
               supertx_enabled,
#endif
               mi_row, mi_col);

#if CONFIG_MOTION_VAR && NC_MODE_INFO
  (void)tok;
  (void)tok_end;
#else
#if !CONFIG_PVQ && CONFIG_SUPERTX
  if (!supertx_enabled)
#endif
    write_tokens_b(cpi, tile, w, tok, tok_end, mi_row, mi_col);
#endif
}

static void write_partition(const AV1_COMMON *const cm,
                            const MACROBLOCKD *const xd, int hbs, int mi_row,
                            int mi_col, PARTITION_TYPE p, BLOCK_SIZE bsize,
                            aom_writer *w) {
  const int has_rows = (mi_row + hbs) < cm->mi_rows;
  const int has_cols = (mi_col + hbs) < cm->mi_cols;
  const int is_partition_point = bsize >= BLOCK_8X8;
  const int ctx = is_partition_point
                      ? partition_plane_context(xd, mi_row, mi_col,
#if CONFIG_UNPOISON_PARTITION_CTX
                                                has_rows, has_cols,
#endif
                                                bsize)
                      : 0;
  FRAME_CONTEXT *ec_ctx = xd->tile_ctx;
  (void)cm;

  if (!is_partition_point) return;

  if (has_rows && has_cols) {
#if CONFIG_EXT_PARTITION_TYPES
    const int num_partition_types =
        (mi_width_log2_lookup[bsize] > mi_width_log2_lookup[BLOCK_8X8])
            ? EXT_PARTITION_TYPES
            : PARTITION_TYPES;
#else
    const int num_partition_types = PARTITION_TYPES;
#endif
    aom_write_symbol(w, p, ec_ctx->partition_cdf[ctx], num_partition_types);
  } else if (!has_rows && has_cols) {
    assert(p == PARTITION_SPLIT || p == PARTITION_HORZ);
    assert(bsize > BLOCK_8X8);
    aom_cdf_prob cdf[2];
    partition_gather_vert_alike(cdf, ec_ctx->partition_cdf[ctx]);
    aom_write_cdf(w, p == PARTITION_SPLIT, cdf, 2);
  } else if (has_rows && !has_cols) {
    assert(p == PARTITION_SPLIT || p == PARTITION_VERT);
    assert(bsize > BLOCK_8X8);
    aom_cdf_prob cdf[2];
    partition_gather_horz_alike(cdf, ec_ctx->partition_cdf[ctx]);
    aom_write_cdf(w, p == PARTITION_SPLIT, cdf, 2);
  } else {
    assert(p == PARTITION_SPLIT);
  }
}

#if CONFIG_SUPERTX
#define write_modes_sb_wrapper(cpi, tile, w, tok, tok_end, supertx_enabled,   \
                               mi_row, mi_col, bsize)                         \
  write_modes_sb(cpi, tile, w, tok, tok_end, supertx_enabled, mi_row, mi_col, \
                 bsize)
#else
#define write_modes_sb_wrapper(cpi, tile, w, tok, tok_end, supertx_enabled, \
                               mi_row, mi_col, bsize)                       \
  write_modes_sb(cpi, tile, w, tok, tok_end, mi_row, mi_col, bsize)
#endif  // CONFIG_SUPERTX

static void write_modes_sb(AV1_COMP *const cpi, const TileInfo *const tile,
                           aom_writer *const w, const TOKENEXTRA **tok,
                           const TOKENEXTRA *const tok_end,
#if CONFIG_SUPERTX
                           int supertx_enabled,
#endif
                           int mi_row, int mi_col, BLOCK_SIZE bsize) {
  const AV1_COMMON *const cm = &cpi->common;
  MACROBLOCKD *const xd = &cpi->td.mb.e_mbd;
  const int hbs = mi_size_wide[bsize] / 2;
#if CONFIG_EXT_PARTITION_TYPES
  const int quarter_step = mi_size_wide[bsize] / 4;
  int i;
#if CONFIG_EXT_PARTITION_TYPES_AB
  const int qbs = mi_size_wide[bsize] / 4;
#endif  // CONFIG_EXT_PARTITION_TYPES_AB
#endif  // CONFIG_EXT_PARTITION_TYPES
  const PARTITION_TYPE partition = get_partition(cm, mi_row, mi_col, bsize);
  const BLOCK_SIZE subsize = get_subsize(bsize, partition);
#if CONFIG_CB4X4
  const int unify_bsize = 1;
#else
  const int unify_bsize = 0;
#endif

#if CONFIG_SUPERTX
  const int mi_offset = mi_row * cm->mi_stride + mi_col;
  MB_MODE_INFO *mbmi;
  const int pack_token = !supertx_enabled;
  TX_SIZE supertx_size;
#endif

  if (mi_row >= cm->mi_rows || mi_col >= cm->mi_cols) return;

  write_partition(cm, xd, hbs, mi_row, mi_col, partition, bsize, w);
#if CONFIG_SUPERTX
  mbmi = &cm->mi_grid_visible[mi_offset]->mbmi;
  xd->mi = cm->mi_grid_visible + mi_offset;
  set_mi_row_col(xd, tile, mi_row, mi_size_high[bsize], mi_col,
                 mi_size_wide[bsize],
#if CONFIG_DEPENDENT_HORZTILES
                 cm->dependent_horz_tiles,
#endif  // CONFIG_DEPENDENT_HORZTILES
                 cm->mi_rows, cm->mi_cols);
  if (!supertx_enabled && !frame_is_intra_only(cm) &&
      partition != PARTITION_NONE && bsize <= MAX_SUPERTX_BLOCK_SIZE &&
      !xd->lossless[0]) {
    aom_prob prob;
    supertx_size = max_txsize_lookup[bsize];
    prob = cm->fc->supertx_prob[partition_supertx_context_lookup[partition]]
                               [supertx_size];
    supertx_enabled = (xd->mi[0]->mbmi.tx_size == supertx_size);
    aom_write(w, supertx_enabled, prob);
  }
#endif  // CONFIG_SUPERTX
  if (subsize < BLOCK_8X8 && !unify_bsize) {
    write_modes_b_wrapper(cpi, tile, w, tok, tok_end, supertx_enabled, mi_row,
                          mi_col);
  } else {
    switch (partition) {
      case PARTITION_NONE:
        write_modes_b_wrapper(cpi, tile, w, tok, tok_end, supertx_enabled,
                              mi_row, mi_col);
        break;
      case PARTITION_HORZ:
        write_modes_b_wrapper(cpi, tile, w, tok, tok_end, supertx_enabled,
                              mi_row, mi_col);
        if (mi_row + hbs < cm->mi_rows)
          write_modes_b_wrapper(cpi, tile, w, tok, tok_end, supertx_enabled,
                                mi_row + hbs, mi_col);
        break;
      case PARTITION_VERT:
        write_modes_b_wrapper(cpi, tile, w, tok, tok_end, supertx_enabled,
                              mi_row, mi_col);
        if (mi_col + hbs < cm->mi_cols)
          write_modes_b_wrapper(cpi, tile, w, tok, tok_end, supertx_enabled,
                                mi_row, mi_col + hbs);
        break;
      case PARTITION_SPLIT:
        write_modes_sb_wrapper(cpi, tile, w, tok, tok_end, supertx_enabled,
                               mi_row, mi_col, subsize);
        write_modes_sb_wrapper(cpi, tile, w, tok, tok_end, supertx_enabled,
                               mi_row, mi_col + hbs, subsize);
        write_modes_sb_wrapper(cpi, tile, w, tok, tok_end, supertx_enabled,
                               mi_row + hbs, mi_col, subsize);
        write_modes_sb_wrapper(cpi, tile, w, tok, tok_end, supertx_enabled,
                               mi_row + hbs, mi_col + hbs, subsize);
        break;
#if CONFIG_EXT_PARTITION_TYPES
#if CONFIG_EXT_PARTITION_TYPES_AB
      case PARTITION_HORZ_A:
        write_modes_b_wrapper(cpi, tile, w, tok, tok_end, supertx_enabled,
                              mi_row, mi_col);
        write_modes_b_wrapper(cpi, tile, w, tok, tok_end, supertx_enabled,
                              mi_row + qbs, mi_col);
        write_modes_b_wrapper(cpi, tile, w, tok, tok_end, supertx_enabled,
                              mi_row + hbs, mi_col);
        break;
      case PARTITION_HORZ_B:
        write_modes_b_wrapper(cpi, tile, w, tok, tok_end, supertx_enabled,
                              mi_row, mi_col);
        write_modes_b_wrapper(cpi, tile, w, tok, tok_end, supertx_enabled,
                              mi_row + hbs, mi_col);
        if (mi_row + 3 * qbs < cm->mi_rows)
          write_modes_b_wrapper(cpi, tile, w, tok, tok_end, supertx_enabled,
                                mi_row + 3 * qbs, mi_col);
        break;
      case PARTITION_VERT_A:
        write_modes_b_wrapper(cpi, tile, w, tok, tok_end, supertx_enabled,
                              mi_row, mi_col);
        write_modes_b_wrapper(cpi, tile, w, tok, tok_end, supertx_enabled,
                              mi_row, mi_col + qbs);
        write_modes_b_wrapper(cpi, tile, w, tok, tok_end, supertx_enabled,
                              mi_row, mi_col + hbs);
        break;
      case PARTITION_VERT_B:
        write_modes_b_wrapper(cpi, tile, w, tok, tok_end, supertx_enabled,
                              mi_row, mi_col);
        write_modes_b_wrapper(cpi, tile, w, tok, tok_end, supertx_enabled,
                              mi_row, mi_col + hbs);
        if (mi_col + 3 * qbs < cm->mi_cols)
          write_modes_b_wrapper(cpi, tile, w, tok, tok_end, supertx_enabled,
                                mi_row, mi_col + 3 * qbs);
        break;
#else
      case PARTITION_HORZ_A:
        write_modes_b_wrapper(cpi, tile, w, tok, tok_end, supertx_enabled,
                              mi_row, mi_col);
        write_modes_b_wrapper(cpi, tile, w, tok, tok_end, supertx_enabled,
                              mi_row, mi_col + hbs);
        write_modes_b_wrapper(cpi, tile, w, tok, tok_end, supertx_enabled,
                              mi_row + hbs, mi_col);
        break;
      case PARTITION_HORZ_B:
        write_modes_b_wrapper(cpi, tile, w, tok, tok_end, supertx_enabled,
                              mi_row, mi_col);
        write_modes_b_wrapper(cpi, tile, w, tok, tok_end, supertx_enabled,
                              mi_row + hbs, mi_col);
        write_modes_b_wrapper(cpi, tile, w, tok, tok_end, supertx_enabled,
                              mi_row + hbs, mi_col + hbs);
        break;
      case PARTITION_VERT_A:
        write_modes_b_wrapper(cpi, tile, w, tok, tok_end, supertx_enabled,
                              mi_row, mi_col);
        write_modes_b_wrapper(cpi, tile, w, tok, tok_end, supertx_enabled,
                              mi_row + hbs, mi_col);
        write_modes_b_wrapper(cpi, tile, w, tok, tok_end, supertx_enabled,
                              mi_row, mi_col + hbs);
        break;
      case PARTITION_VERT_B:
        write_modes_b_wrapper(cpi, tile, w, tok, tok_end, supertx_enabled,
                              mi_row, mi_col);
        write_modes_b_wrapper(cpi, tile, w, tok, tok_end, supertx_enabled,
                              mi_row, mi_col + hbs);
        write_modes_b_wrapper(cpi, tile, w, tok, tok_end, supertx_enabled,
                              mi_row + hbs, mi_col + hbs);
        break;
#endif
      case PARTITION_HORZ_4:
        for (i = 0; i < 4; ++i) {
          int this_mi_row = mi_row + i * quarter_step;
          if (i > 0 && this_mi_row >= cm->mi_rows) break;

          write_modes_b_wrapper(cpi, tile, w, tok, tok_end, supertx_enabled,
                                this_mi_row, mi_col);
        }
        break;
      case PARTITION_VERT_4:
        for (i = 0; i < 4; ++i) {
          int this_mi_col = mi_col + i * quarter_step;
          if (i > 0 && this_mi_col >= cm->mi_cols) break;

          write_modes_b_wrapper(cpi, tile, w, tok, tok_end, supertx_enabled,
                                mi_row, this_mi_col);
        }
        break;
#endif  // CONFIG_EXT_PARTITION_TYPES
      default: assert(0);
    }
  }
#if CONFIG_SUPERTX
  if (partition != PARTITION_NONE && supertx_enabled && pack_token) {
    int skip;
    const int bsw = mi_size_wide[bsize];
    const int bsh = mi_size_high[bsize];

    xd->mi = cm->mi_grid_visible + mi_offset;
    supertx_size = mbmi->tx_size;
    set_mi_row_col(xd, tile, mi_row, bsh, mi_col, bsw,
#if CONFIG_DEPENDENT_HORZTILES
                   cm->dependent_horz_tiles,
#endif  // CONFIG_DEPENDENT_HORZTILES
                   cm->mi_rows, cm->mi_cols);

    assert(IMPLIES(!cm->seg.enabled, mbmi->segment_id_supertx == 0));
    assert(mbmi->segment_id_supertx < MAX_SEGMENTS);

    skip = write_skip(cm, xd, mbmi->segment_id_supertx, xd->mi[0], w);

    FRAME_CONTEXT *ec_ctx = xd->tile_ctx;

#if CONFIG_EXT_TX
    if (get_ext_tx_types(supertx_size, bsize, 1, cm->reduced_tx_set_used) > 1 &&
        !skip) {
      const int eset =
          get_ext_tx_set(supertx_size, bsize, 1, cm->reduced_tx_set_used);
      const int tx_set_type =
          get_ext_tx_set_type(supertx_size, bsize, 1, cm->reduced_tx_set_used);
      if (eset > 0) {
        aom_write_symbol(w, av1_ext_tx_ind[tx_set_type][mbmi->tx_type],
                         ec_ctx->inter_ext_tx_cdf[eset][supertx_size],
                         av1_num_ext_tx_set[tx_set_type]);
      }
    }
#else
    if (supertx_size < TX_32X32 && !skip) {
      aom_write_symbol(w, mbmi->tx_type, ec_ctx->inter_ext_tx_cdf[supertx_size],
                       TX_TYPES);
    }
#endif  // CONFIG_EXT_TX

    if (!skip) {
      assert(*tok < tok_end);
      for (int plane = 0; plane < MAX_MB_PLANE; ++plane) {
#if CONFIG_MRC_TX && SIGNAL_ANY_MRC_MASK
        TX_TYPE tx_type = av1_get_tx_type(plane ? PLANE_TYPE_UV : PLANE_TYPE_Y,
                                          xd, blk_row, blk_col, block, tx_size);
#endif  // CONFIG_MRC_TX && SIGNAL_ANY_MRC_MASK
        const struct macroblockd_plane *const pd = &xd->plane[plane];
        const int mbmi_txb_size = txsize_to_bsize[mbmi->tx_size];
        const BLOCK_SIZE plane_bsize = get_plane_block_size(mbmi_txb_size, pd);

        const int max_blocks_wide = max_block_wide(xd, plane_bsize, plane);
        const int max_blocks_high = max_block_high(xd, plane_bsize, plane);

        int row, col;
        const TX_SIZE tx = av1_get_tx_size(plane, xd);
        BLOCK_SIZE txb_size = txsize_to_bsize[tx];

        const int stepr = tx_size_high_unit[txb_size];
        const int stepc = tx_size_wide_unit[txb_size];

        TOKEN_STATS token_stats;
        token_stats.cost = 0;
        for (row = 0; row < max_blocks_high; row += stepr)
          for (col = 0; col < max_blocks_wide; col += stepc)
            pack_mb_tokens(w, tok, tok_end, cm->bit_depth, tx,
#if CONFIG_MRC_TX && SIGNAL_ANY_MRC_MASK
                           tx_type, is_inter_block(mbmi),
#endif  // CONFIG_MRC_TX && SIGNAL_ANY_MRC_MASK
                           &token_stats);
        assert(*tok < tok_end && (*tok)->token == EOSB_TOKEN);
        (*tok)++;
      }
    }
#if CONFIG_VAR_TX
    xd->above_txfm_context = cm->above_txfm_context + mi_col;
    xd->left_txfm_context =
        xd->left_txfm_context_buffer + (mi_row & MAX_MIB_MASK);
    set_txfm_ctxs(xd->mi[0]->mbmi.tx_size, bsw, bsh, skip, xd);
#endif
  }
#endif  // CONFIG_SUPERTX

// update partition context
#if CONFIG_EXT_PARTITION_TYPES
  update_ext_partition_context(xd, mi_row, mi_col, subsize, bsize, partition);
#else
  if (bsize >= BLOCK_8X8 &&
      (bsize == BLOCK_8X8 || partition != PARTITION_SPLIT))
    update_partition_context(xd, mi_row, mi_col, subsize, bsize);
#endif  // CONFIG_EXT_PARTITION_TYPES

#if CONFIG_LPF_SB
  // send filter level for each superblock (64x64)
  if (bsize == cm->sb_size) {
    if (mi_row == 0 && mi_col == 0) {
      aom_write_literal(w, cm->mi_grid_visible[0]->mbmi.filt_lvl, 6);
      cm->mi_grid_visible[0]->mbmi.reuse_sb_lvl = 0;
      cm->mi_grid_visible[0]->mbmi.delta = 0;
      cm->mi_grid_visible[0]->mbmi.sign = 0;
    } else {
      int prev_mi_row, prev_mi_col;
      if (mi_col - MAX_MIB_SIZE < 0) {
        prev_mi_row = mi_row - MAX_MIB_SIZE;
        prev_mi_col = mi_col;
      } else {
        prev_mi_row = mi_row;
        prev_mi_col = mi_col - MAX_MIB_SIZE;
      }
      MB_MODE_INFO *curr_mbmi =
          &cm->mi_grid_visible[mi_row * cm->mi_stride + mi_col]->mbmi;
      MB_MODE_INFO *prev_mbmi =
          &cm->mi_grid_visible[prev_mi_row * cm->mi_stride + prev_mi_col]->mbmi;

      const uint8_t curr_lvl = curr_mbmi->filt_lvl;
      const uint8_t prev_lvl = prev_mbmi->filt_lvl;

      const int reuse_prev_lvl = curr_lvl == prev_lvl;
      const int reuse_ctx = prev_mbmi->reuse_sb_lvl;
      curr_mbmi->reuse_sb_lvl = reuse_prev_lvl;
      aom_write_symbol(w, reuse_prev_lvl,
                       xd->tile_ctx->lpf_reuse_cdf[reuse_ctx], 2);

      if (reuse_prev_lvl) {
        curr_mbmi->delta = 0;
        curr_mbmi->sign = 0;
      } else {
        const unsigned int delta = abs(curr_lvl - prev_lvl) / LPF_STEP;
        const int delta_ctx = prev_mbmi->delta;
        curr_mbmi->delta = delta;
        aom_write_symbol(w, delta, xd->tile_ctx->lpf_delta_cdf[delta_ctx],
                         DELTA_RANGE);

        if (delta) {
          const int sign = curr_lvl > prev_lvl;
          const int sign_ctx = prev_mbmi->sign;
          curr_mbmi->sign = sign;
          aom_write_symbol(w, sign,
                           xd->tile_ctx->lpf_sign_cdf[reuse_ctx][sign_ctx], 2);
        } else {
          curr_mbmi->sign = 0;
        }
      }
    }
  }
#endif

#if CONFIG_CDEF
  if (bsize == cm->sb_size && cm->cdef_bits != 0 && !cm->all_lossless) {
    int width_step = mi_size_wide[BLOCK_64X64];
    int height_step = mi_size_high[BLOCK_64X64];
    int width, height;
    for (height = 0; (height < mi_size_high[cm->sb_size]) &&
                     (mi_row + height < cm->mi_rows);
         height += height_step) {
      for (width = 0; (width < mi_size_wide[cm->sb_size]) &&
                      (mi_col + width < cm->mi_cols);
           width += width_step) {
        if (!sb_all_skip(cm, mi_row + height, mi_col + width))
          aom_write_literal(
              w,
              cm->mi_grid_visible[(mi_row + height) * cm->mi_stride +
                                  (mi_col + width)]
                  ->mbmi.cdef_strength,
              cm->cdef_bits);
      }
    }
  }
#endif
#if CONFIG_LOOP_RESTORATION
  for (int plane = 0; plane < MAX_MB_PLANE; ++plane) {
    int rcol0, rcol1, rrow0, rrow1, nhtiles;
    if (av1_loop_restoration_corners_in_sb(cm, plane, mi_row, mi_col, bsize,
                                           &rcol0, &rcol1, &rrow0, &rrow1,
                                           &nhtiles)) {
      for (int rrow = rrow0; rrow < rrow1; ++rrow) {
        for (int rcol = rcol0; rcol < rcol1; ++rcol) {
          int rtile_idx = rcol + rrow * nhtiles;
          loop_restoration_write_sb_coeffs(cm, xd, w, plane, rtile_idx);
        }
      }
    }
  }
#endif
}

static void write_modes(AV1_COMP *const cpi, const TileInfo *const tile,
                        aom_writer *const w, const TOKENEXTRA **tok,
                        const TOKENEXTRA *const tok_end) {
  AV1_COMMON *const cm = &cpi->common;
  MACROBLOCKD *const xd = &cpi->td.mb.e_mbd;
  const int mi_row_start = tile->mi_row_start;
  const int mi_row_end = tile->mi_row_end;
  const int mi_col_start = tile->mi_col_start;
  const int mi_col_end = tile->mi_col_end;
  int mi_row, mi_col;

#if CONFIG_DEPENDENT_HORZTILES
  if (!cm->dependent_horz_tiles || mi_row_start == 0 ||
      tile->tg_horz_boundary) {
    av1_zero_above_context(cm, mi_col_start, mi_col_end);
  }
#else
  av1_zero_above_context(cm, mi_col_start, mi_col_end);
#endif
#if CONFIG_PVQ
  assert(cpi->td.mb.pvq_q->curr_pos == 0);
#endif
  if (cpi->common.delta_q_present_flag) {
    xd->prev_qindex = cpi->common.base_qindex;
#if CONFIG_EXT_DELTA_Q
    if (cpi->common.delta_lf_present_flag) {
#if CONFIG_LOOPFILTER_LEVEL
      for (int lf_id = 0; lf_id < FRAME_LF_COUNT; ++lf_id)
        xd->prev_delta_lf[lf_id] = 0;
#endif  // CONFIG_LOOPFILTER_LEVEL
      xd->prev_delta_lf_from_base = 0;
    }
#endif  // CONFIG_EXT_DELTA_Q
  }

  for (mi_row = mi_row_start; mi_row < mi_row_end; mi_row += cm->mib_size) {
    av1_zero_left_context(xd);

    for (mi_col = mi_col_start; mi_col < mi_col_end; mi_col += cm->mib_size) {
      write_modes_sb_wrapper(cpi, tile, w, tok, tok_end, 0, mi_row, mi_col,
                             cm->sb_size);
#if CONFIG_MOTION_VAR && NC_MODE_INFO
      write_tokens_sb(cpi, tile, w, tok, tok_end, mi_row, mi_col, cm->sb_size);
#endif
    }
  }
#if CONFIG_PVQ
  // Check that the number of PVQ blocks encoded and written to the bitstream
  // are the same
  assert(cpi->td.mb.pvq_q->curr_pos == cpi->td.mb.pvq_q->last_pos);
  // Reset curr_pos in case we repack the bitstream
  cpi->td.mb.pvq_q->curr_pos = 0;
#endif
}

#if CONFIG_LOOP_RESTORATION
static void encode_restoration_mode(AV1_COMMON *cm,
                                    struct aom_write_bit_buffer *wb) {
  int p;
  RestorationInfo *rsi = &cm->rst_info[0];
  switch (rsi->frame_restoration_type) {
    case RESTORE_NONE:
      aom_wb_write_bit(wb, 0);
      aom_wb_write_bit(wb, 0);
      break;
    case RESTORE_WIENER:
      aom_wb_write_bit(wb, 1);
      aom_wb_write_bit(wb, 0);
      break;
    case RESTORE_SGRPROJ:
      aom_wb_write_bit(wb, 1);
      aom_wb_write_bit(wb, 1);
      break;
    case RESTORE_SWITCHABLE:
      aom_wb_write_bit(wb, 0);
      aom_wb_write_bit(wb, 1);
      break;
    default: assert(0);
  }
  for (p = 1; p < MAX_MB_PLANE; ++p) {
    rsi = &cm->rst_info[p];
    switch (rsi->frame_restoration_type) {
      case RESTORE_NONE: aom_wb_write_bit(wb, 0); break;
      case RESTORE_WIENER:
        aom_wb_write_bit(wb, 1);
        aom_wb_write_bit(wb, 0);
        break;
      case RESTORE_SGRPROJ:
        aom_wb_write_bit(wb, 1);
        aom_wb_write_bit(wb, 1);
        break;
      default: assert(0);
    }
  }
  if (cm->rst_info[0].frame_restoration_type != RESTORE_NONE ||
      cm->rst_info[1].frame_restoration_type != RESTORE_NONE ||
      cm->rst_info[2].frame_restoration_type != RESTORE_NONE) {
    rsi = &cm->rst_info[0];
    aom_wb_write_bit(wb, rsi->restoration_tilesize != RESTORATION_TILESIZE_MAX);
    if (rsi->restoration_tilesize != RESTORATION_TILESIZE_MAX) {
      aom_wb_write_bit(
          wb, rsi->restoration_tilesize != (RESTORATION_TILESIZE_MAX >> 1));
    }
  }
  int s = AOMMIN(cm->subsampling_x, cm->subsampling_y);
  if (s && (cm->rst_info[1].frame_restoration_type != RESTORE_NONE ||
            cm->rst_info[2].frame_restoration_type != RESTORE_NONE)) {
    aom_wb_write_bit(wb,
                     cm->rst_info[1].restoration_tilesize !=
                         cm->rst_info[0].restoration_tilesize);
    assert(cm->rst_info[1].restoration_tilesize ==
               cm->rst_info[0].restoration_tilesize ||
           cm->rst_info[1].restoration_tilesize ==
               (cm->rst_info[0].restoration_tilesize >> s));
    assert(cm->rst_info[2].restoration_tilesize ==
           cm->rst_info[1].restoration_tilesize);
  } else if (!s) {
    assert(cm->rst_info[1].restoration_tilesize ==
           cm->rst_info[0].restoration_tilesize);
    assert(cm->rst_info[2].restoration_tilesize ==
           cm->rst_info[1].restoration_tilesize);
  }
}

static void write_wiener_filter(int wiener_win, WienerInfo *wiener_info,
                                WienerInfo *ref_wiener_info, aom_writer *wb) {
  if (wiener_win == WIENER_WIN)
    aom_write_primitive_refsubexpfin(
        wb, WIENER_FILT_TAP0_MAXV - WIENER_FILT_TAP0_MINV + 1,
        WIENER_FILT_TAP0_SUBEXP_K,
        ref_wiener_info->vfilter[0] - WIENER_FILT_TAP0_MINV,
        wiener_info->vfilter[0] - WIENER_FILT_TAP0_MINV);
  else
    assert(wiener_info->vfilter[0] == 0 &&
           wiener_info->vfilter[WIENER_WIN - 1] == 0);
  aom_write_primitive_refsubexpfin(
      wb, WIENER_FILT_TAP1_MAXV - WIENER_FILT_TAP1_MINV + 1,
      WIENER_FILT_TAP1_SUBEXP_K,
      ref_wiener_info->vfilter[1] - WIENER_FILT_TAP1_MINV,
      wiener_info->vfilter[1] - WIENER_FILT_TAP1_MINV);
  aom_write_primitive_refsubexpfin(
      wb, WIENER_FILT_TAP2_MAXV - WIENER_FILT_TAP2_MINV + 1,
      WIENER_FILT_TAP2_SUBEXP_K,
      ref_wiener_info->vfilter[2] - WIENER_FILT_TAP2_MINV,
      wiener_info->vfilter[2] - WIENER_FILT_TAP2_MINV);
  if (wiener_win == WIENER_WIN)
    aom_write_primitive_refsubexpfin(
        wb, WIENER_FILT_TAP0_MAXV - WIENER_FILT_TAP0_MINV + 1,
        WIENER_FILT_TAP0_SUBEXP_K,
        ref_wiener_info->hfilter[0] - WIENER_FILT_TAP0_MINV,
        wiener_info->hfilter[0] - WIENER_FILT_TAP0_MINV);
  else
    assert(wiener_info->hfilter[0] == 0 &&
           wiener_info->hfilter[WIENER_WIN - 1] == 0);
  aom_write_primitive_refsubexpfin(
      wb, WIENER_FILT_TAP1_MAXV - WIENER_FILT_TAP1_MINV + 1,
      WIENER_FILT_TAP1_SUBEXP_K,
      ref_wiener_info->hfilter[1] - WIENER_FILT_TAP1_MINV,
      wiener_info->hfilter[1] - WIENER_FILT_TAP1_MINV);
  aom_write_primitive_refsubexpfin(
      wb, WIENER_FILT_TAP2_MAXV - WIENER_FILT_TAP2_MINV + 1,
      WIENER_FILT_TAP2_SUBEXP_K,
      ref_wiener_info->hfilter[2] - WIENER_FILT_TAP2_MINV,
      wiener_info->hfilter[2] - WIENER_FILT_TAP2_MINV);
  memcpy(ref_wiener_info, wiener_info, sizeof(*wiener_info));
}

static void write_sgrproj_filter(SgrprojInfo *sgrproj_info,
                                 SgrprojInfo *ref_sgrproj_info,
                                 aom_writer *wb) {
  aom_write_literal(wb, sgrproj_info->ep, SGRPROJ_PARAMS_BITS);
  aom_write_primitive_refsubexpfin(wb, SGRPROJ_PRJ_MAX0 - SGRPROJ_PRJ_MIN0 + 1,
                                   SGRPROJ_PRJ_SUBEXP_K,
                                   ref_sgrproj_info->xqd[0] - SGRPROJ_PRJ_MIN0,
                                   sgrproj_info->xqd[0] - SGRPROJ_PRJ_MIN0);
  aom_write_primitive_refsubexpfin(wb, SGRPROJ_PRJ_MAX1 - SGRPROJ_PRJ_MIN1 + 1,
                                   SGRPROJ_PRJ_SUBEXP_K,
                                   ref_sgrproj_info->xqd[1] - SGRPROJ_PRJ_MIN1,
                                   sgrproj_info->xqd[1] - SGRPROJ_PRJ_MIN1);
  memcpy(ref_sgrproj_info, sgrproj_info, sizeof(*sgrproj_info));
}

static void loop_restoration_write_sb_coeffs(const AV1_COMMON *const cm,
                                             MACROBLOCKD *xd,
                                             aom_writer *const w, int plane,
                                             int rtile_idx) {
  const RestorationInfo *rsi = cm->rst_info + plane;
  if (rsi->frame_restoration_type == RESTORE_NONE) return;

  const int wiener_win = (plane > 0) ? WIENER_WIN_CHROMA : WIENER_WIN;
  WienerInfo *wiener_info = xd->wiener_info + plane;
  SgrprojInfo *sgrproj_info = xd->sgrproj_info + plane;

  if (rsi->frame_restoration_type == RESTORE_SWITCHABLE) {
    assert(plane == 0);
    av1_write_token(
        w, av1_switchable_restore_tree, cm->fc->switchable_restore_prob,
        &switchable_restore_encodings[rsi->restoration_type[rtile_idx]]);
    if (rsi->restoration_type[rtile_idx] == RESTORE_WIENER) {
      write_wiener_filter(wiener_win, &rsi->wiener_info[rtile_idx], wiener_info,
                          w);
    } else if (rsi->restoration_type[rtile_idx] == RESTORE_SGRPROJ) {
      write_sgrproj_filter(&rsi->sgrproj_info[rtile_idx], sgrproj_info, w);
    }
  } else if (rsi->frame_restoration_type == RESTORE_WIENER) {
    aom_write(w, rsi->restoration_type[rtile_idx] != RESTORE_NONE,
              RESTORE_NONE_WIENER_PROB);
    if (rsi->restoration_type[rtile_idx] != RESTORE_NONE) {
      write_wiener_filter(wiener_win, &rsi->wiener_info[rtile_idx], wiener_info,
                          w);
    }
  } else if (rsi->frame_restoration_type == RESTORE_SGRPROJ) {
    aom_write(w, rsi->restoration_type[rtile_idx] != RESTORE_NONE,
              RESTORE_NONE_SGRPROJ_PROB);
    if (rsi->restoration_type[rtile_idx] != RESTORE_NONE) {
      write_sgrproj_filter(&rsi->sgrproj_info[rtile_idx], sgrproj_info, w);
    }
  }
}

#endif  // CONFIG_LOOP_RESTORATION

static void encode_loopfilter(AV1_COMMON *cm, struct aom_write_bit_buffer *wb) {
  int i;
  struct loopfilter *lf = &cm->lf;

// Encode the loop filter level and type
#if !CONFIG_LPF_SB
#if CONFIG_LOOPFILTER_LEVEL
  aom_wb_write_literal(wb, lf->filter_level[0], 6);
  aom_wb_write_literal(wb, lf->filter_level[1], 6);
  if (lf->filter_level[0] || lf->filter_level[1]) {
    aom_wb_write_literal(wb, lf->filter_level_u, 6);
    aom_wb_write_literal(wb, lf->filter_level_v, 6);
  }
#else
  aom_wb_write_literal(wb, lf->filter_level, 6);
#endif  // CONFIG_LOOPFILTER_LEVEL
#endif  // CONFIG_LPF_SB
  aom_wb_write_literal(wb, lf->sharpness_level, 3);

  // Write out loop filter deltas applied at the MB level based on mode or
  // ref frame (if they are enabled).
  aom_wb_write_bit(wb, lf->mode_ref_delta_enabled);

  if (lf->mode_ref_delta_enabled) {
    aom_wb_write_bit(wb, lf->mode_ref_delta_update);
    if (lf->mode_ref_delta_update) {
      for (i = 0; i < TOTAL_REFS_PER_FRAME; i++) {
        const int delta = lf->ref_deltas[i];
        const int changed = delta != lf->last_ref_deltas[i];
        aom_wb_write_bit(wb, changed);
        if (changed) {
          lf->last_ref_deltas[i] = delta;
          aom_wb_write_inv_signed_literal(wb, delta, 6);
        }
      }

      for (i = 0; i < MAX_MODE_LF_DELTAS; i++) {
        const int delta = lf->mode_deltas[i];
        const int changed = delta != lf->last_mode_deltas[i];
        aom_wb_write_bit(wb, changed);
        if (changed) {
          lf->last_mode_deltas[i] = delta;
          aom_wb_write_inv_signed_literal(wb, delta, 6);
        }
      }
    }
  }
}

#if CONFIG_CDEF
static void encode_cdef(const AV1_COMMON *cm, struct aom_write_bit_buffer *wb) {
  int i;
#if CONFIG_CDEF_SINGLEPASS
  aom_wb_write_literal(wb, cm->cdef_pri_damping - 3, 2);
  assert(cm->cdef_pri_damping == cm->cdef_sec_damping);
#else
  aom_wb_write_literal(wb, cm->cdef_pri_damping - 5, 1);
  aom_wb_write_literal(wb, cm->cdef_sec_damping - 3, 2);
#endif
  aom_wb_write_literal(wb, cm->cdef_bits, 2);
  for (i = 0; i < cm->nb_cdef_strengths; i++) {
    aom_wb_write_literal(wb, cm->cdef_strengths[i], CDEF_STRENGTH_BITS);
    if (cm->subsampling_x == cm->subsampling_y)
      aom_wb_write_literal(wb, cm->cdef_uv_strengths[i], CDEF_STRENGTH_BITS);
  }
}
#endif

static void write_delta_q(struct aom_write_bit_buffer *wb, int delta_q) {
  if (delta_q != 0) {
    aom_wb_write_bit(wb, 1);
    aom_wb_write_inv_signed_literal(wb, delta_q, 6);
  } else {
    aom_wb_write_bit(wb, 0);
  }
}

static void encode_quantization(const AV1_COMMON *const cm,
                                struct aom_write_bit_buffer *wb) {
  aom_wb_write_literal(wb, cm->base_qindex, QINDEX_BITS);
  write_delta_q(wb, cm->y_dc_delta_q);
  write_delta_q(wb, cm->uv_dc_delta_q);
  write_delta_q(wb, cm->uv_ac_delta_q);
#if CONFIG_AOM_QM
  aom_wb_write_bit(wb, cm->using_qmatrix);
  if (cm->using_qmatrix) {
    aom_wb_write_literal(wb, cm->min_qmlevel, QM_LEVEL_BITS);
    aom_wb_write_literal(wb, cm->max_qmlevel, QM_LEVEL_BITS);
  }
#endif
}

static void encode_segmentation(AV1_COMMON *cm, MACROBLOCKD *xd,
                                struct aom_write_bit_buffer *wb) {
  int i, j;
  const struct segmentation *seg = &cm->seg;

  aom_wb_write_bit(wb, seg->enabled);
  if (!seg->enabled) return;

  // Segmentation map
  if (!frame_is_intra_only(cm) && !cm->error_resilient_mode) {
    aom_wb_write_bit(wb, seg->update_map);
  } else {
    assert(seg->update_map == 1);
  }
  if (seg->update_map) {
    // Select the coding strategy (temporal or spatial)
    av1_choose_segmap_coding_method(cm, xd);

    // Write out the chosen coding method.
    if (!frame_is_intra_only(cm) && !cm->error_resilient_mode) {
      aom_wb_write_bit(wb, seg->temporal_update);
    } else {
      assert(seg->temporal_update == 0);
    }
  }

  // Segmentation data
  aom_wb_write_bit(wb, seg->update_data);
  if (seg->update_data) {
    aom_wb_write_bit(wb, seg->abs_delta);

    for (i = 0; i < MAX_SEGMENTS; i++) {
      for (j = 0; j < SEG_LVL_MAX; j++) {
        const int active = segfeature_active(seg, i, j);
        aom_wb_write_bit(wb, active);
        if (active) {
          const int data = get_segdata(seg, i, j);
          const int data_max = av1_seg_feature_data_max(j);

          if (av1_is_segfeature_signed(j)) {
            encode_unsigned_max(wb, abs(data), data_max);
            aom_wb_write_bit(wb, data < 0);
          } else {
            encode_unsigned_max(wb, data, data_max);
          }
        }
      }
    }
  }
}

static void write_tx_mode(AV1_COMMON *cm, TX_MODE *mode,
                          struct aom_write_bit_buffer *wb) {
  if (cm->all_lossless) {
    *mode = ONLY_4X4;
    return;
  }
#if CONFIG_VAR_TX_NO_TX_MODE
  (void)wb;
  *mode = TX_MODE_SELECT;
  return;
#else
#if CONFIG_TX64X64
  aom_wb_write_bit(wb, *mode == TX_MODE_SELECT);
  if (*mode != TX_MODE_SELECT) {
    aom_wb_write_literal(wb, AOMMIN(*mode, ALLOW_32X32), 2);
    if (*mode >= ALLOW_32X32) aom_wb_write_bit(wb, *mode == ALLOW_64X64);
  }
#else
  aom_wb_write_bit(wb, *mode == TX_MODE_SELECT);
  if (*mode != TX_MODE_SELECT) aom_wb_write_literal(wb, *mode, 2);
#endif  // CONFIG_TX64X64
#endif  // CONFIG_VAR_TX_NO_TX_MODE
}

static void write_frame_interp_filter(InterpFilter filter,
                                      struct aom_write_bit_buffer *wb) {
  aom_wb_write_bit(wb, filter == SWITCHABLE);
  if (filter != SWITCHABLE)
    aom_wb_write_literal(wb, filter, LOG_SWITCHABLE_FILTERS);
}

static void fix_interp_filter(AV1_COMMON *cm, FRAME_COUNTS *counts) {
  if (cm->interp_filter == SWITCHABLE) {
    // Check to see if only one of the filters is actually used
    int count[SWITCHABLE_FILTERS];
    int i, j, c = 0;
    for (i = 0; i < SWITCHABLE_FILTERS; ++i) {
      count[i] = 0;
      for (j = 0; j < SWITCHABLE_FILTER_CONTEXTS; ++j)
        count[i] += counts->switchable_interp[j][i];
      c += (count[i] > 0);
    }
    if (c == 1) {
      // Only one filter is used. So set the filter at frame level
      for (i = 0; i < SWITCHABLE_FILTERS; ++i) {
        if (count[i]) {
#if CONFIG_MOTION_VAR && (CONFIG_WARPED_MOTION || CONFIG_GLOBAL_MOTION)
#if CONFIG_WARPED_MOTION
          if (i == EIGHTTAP_REGULAR || WARP_WM_NEIGHBORS_WITH_OBMC)
#else
          if (i == EIGHTTAP_REGULAR || WARP_GM_NEIGHBORS_WITH_OBMC)
#endif  // CONFIG_WARPED_MOTION
#endif  // CONFIG_MOTION_VAR && (CONFIG_WARPED_MOTION || CONFIG_GLOBAL_MOTION)
            cm->interp_filter = i;
          break;
        }
      }
    }
  }
}

#if CONFIG_MAX_TILE

// Same function as write_uniform but writing to uncompresses header wb
static void wb_write_uniform(struct aom_write_bit_buffer *wb, int n, int v) {
  const int l = get_unsigned_bits(n);
  const int m = (1 << l) - n;
  if (l == 0) return;
  if (v < m) {
    aom_wb_write_literal(wb, v, l - 1);
  } else {
    aom_wb_write_literal(wb, m + ((v - m) >> 1), l - 1);
    aom_wb_write_literal(wb, (v - m) & 1, 1);
  }
}

static void write_tile_info_max_tile(const AV1_COMMON *const cm,
                                     struct aom_write_bit_buffer *wb) {
  int width_mi = ALIGN_POWER_OF_TWO(cm->mi_cols, MAX_MIB_SIZE_LOG2);
  int height_mi = ALIGN_POWER_OF_TWO(cm->mi_rows, MAX_MIB_SIZE_LOG2);
  int width_sb = width_mi >> MAX_MIB_SIZE_LOG2;
  int height_sb = height_mi >> MAX_MIB_SIZE_LOG2;
  int size_sb, i;

  aom_wb_write_bit(wb, cm->uniform_tile_spacing_flag);

  if (cm->uniform_tile_spacing_flag) {
    // Uniform spaced tiles with power-of-two number of rows and columns
    // tile columns
    int ones = cm->log2_tile_cols - cm->min_log2_tile_cols;
    while (ones--) {
      aom_wb_write_bit(wb, 1);
    }
    if (cm->log2_tile_cols < cm->max_log2_tile_cols) {
      aom_wb_write_bit(wb, 0);
    }

    // rows
    ones = cm->log2_tile_rows - cm->min_log2_tile_rows;
    while (ones--) {
      aom_wb_write_bit(wb, 1);
    }
    if (cm->log2_tile_rows < cm->max_log2_tile_rows) {
      aom_wb_write_bit(wb, 0);
    }
  } else {
    // Explicit tiles with configurable tile widths and heights
    // columns
    for (i = 0; i < cm->tile_cols; i++) {
      size_sb = cm->tile_col_start_sb[i + 1] - cm->tile_col_start_sb[i];
      wb_write_uniform(wb, AOMMIN(width_sb, MAX_TILE_WIDTH_SB), size_sb - 1);
      width_sb -= size_sb;
    }
    assert(width_sb == 0);

    // rows
    for (i = 0; i < cm->tile_rows; i++) {
      size_sb = cm->tile_row_start_sb[i + 1] - cm->tile_row_start_sb[i];
      wb_write_uniform(wb, AOMMIN(height_sb, cm->max_tile_height_sb),
                       size_sb - 1);
      height_sb -= size_sb;
    }
    assert(height_sb == 0);
  }
}
#endif

static void write_tile_info(const AV1_COMMON *const cm,
                            struct aom_write_bit_buffer *wb) {
#if CONFIG_EXT_TILE
  if (cm->large_scale_tile) {
    const int tile_width =
        ALIGN_POWER_OF_TWO(cm->tile_width, cm->mib_size_log2) >>
        cm->mib_size_log2;
    const int tile_height =
        ALIGN_POWER_OF_TWO(cm->tile_height, cm->mib_size_log2) >>
        cm->mib_size_log2;

    assert(tile_width > 0);
    assert(tile_height > 0);

// Write the tile sizes
#if CONFIG_EXT_PARTITION
    if (cm->sb_size == BLOCK_128X128) {
      assert(tile_width <= 32);
      assert(tile_height <= 32);
      aom_wb_write_literal(wb, tile_width - 1, 5);
      aom_wb_write_literal(wb, tile_height - 1, 5);
    } else {
#endif  // CONFIG_EXT_PARTITION
      assert(tile_width <= 64);
      assert(tile_height <= 64);
      aom_wb_write_literal(wb, tile_width - 1, 6);
      aom_wb_write_literal(wb, tile_height - 1, 6);
#if CONFIG_EXT_PARTITION
    }
#endif  // CONFIG_EXT_PARTITION
  } else {
#endif  // CONFIG_EXT_TILE

#if CONFIG_MAX_TILE
    write_tile_info_max_tile(cm, wb);
#else
  int min_log2_tile_cols, max_log2_tile_cols, ones;
  av1_get_tile_n_bits(cm->mi_cols, &min_log2_tile_cols, &max_log2_tile_cols);

  // columns
  ones = cm->log2_tile_cols - min_log2_tile_cols;
  while (ones--) aom_wb_write_bit(wb, 1);

  if (cm->log2_tile_cols < max_log2_tile_cols) aom_wb_write_bit(wb, 0);

  // rows
  aom_wb_write_bit(wb, cm->log2_tile_rows != 0);
  if (cm->log2_tile_rows != 0) aom_wb_write_bit(wb, cm->log2_tile_rows != 1);
#endif
#if CONFIG_DEPENDENT_HORZTILES
    if (cm->tile_rows > 1) aom_wb_write_bit(wb, cm->dependent_horz_tiles);
#endif
#if CONFIG_EXT_TILE
  }
#endif  // CONFIG_EXT_TILE

#if CONFIG_LOOPFILTERING_ACROSS_TILES
  aom_wb_write_bit(wb, cm->loop_filter_across_tiles_enabled);
#endif  // CONFIG_LOOPFILTERING_ACROSS_TILES
}

#if CONFIG_EXT_REFS
#if USE_GF16_MULTI_LAYER
static int get_refresh_mask_gf16(AV1_COMP *cpi) {
  int refresh_mask = 0;

  if (cpi->refresh_last_frame || cpi->refresh_golden_frame ||
      cpi->refresh_bwd_ref_frame || cpi->refresh_alt2_ref_frame ||
      cpi->refresh_alt_ref_frame) {
    assert(cpi->refresh_fb_idx >= 0 && cpi->refresh_fb_idx < REF_FRAMES);
    refresh_mask |= (1 << cpi->refresh_fb_idx);
  }

  return refresh_mask;
}
#endif  // USE_GF16_MULTI_LAYER
#endif  // CONFIG_EXT_REFS

static int get_refresh_mask(AV1_COMP *cpi) {
  int refresh_mask = 0;
#if CONFIG_EXT_REFS
#if USE_GF16_MULTI_LAYER
  if (cpi->rc.baseline_gf_interval == 16) return get_refresh_mask_gf16(cpi);
#endif  // USE_GF16_MULTI_LAYER

  // NOTE(zoeliu): When LAST_FRAME is to get refreshed, the decoder will be
  // notified to get LAST3_FRAME refreshed and then the virtual indexes for all
  // the 3 LAST reference frames will be updated accordingly, i.e.:
  // (1) The original virtual index for LAST3_FRAME will become the new virtual
  //     index for LAST_FRAME; and
  // (2) The original virtual indexes for LAST_FRAME and LAST2_FRAME will be
  //     shifted and become the new virtual indexes for LAST2_FRAME and
  //     LAST3_FRAME.
  refresh_mask |=
      (cpi->refresh_last_frame << cpi->lst_fb_idxes[LAST_REF_FRAMES - 1]);

  refresh_mask |= (cpi->refresh_bwd_ref_frame << cpi->bwd_fb_idx);
  refresh_mask |= (cpi->refresh_alt2_ref_frame << cpi->alt2_fb_idx);
#else   // !CONFIG_EXT_REFS
  refresh_mask |= (cpi->refresh_last_frame << cpi->lst_fb_idx);
#endif  // CONFIG_EXT_REFS

  if (av1_preserve_existing_gf(cpi)) {
    // We have decided to preserve the previously existing golden frame as our
    // new ARF frame. However, in the short term we leave it in the GF slot and,
    // if we're updating the GF with the current decoded frame, we save it
    // instead to the ARF slot.
    // Later, in the function av1_encoder.c:av1_update_reference_frames() we
    // will swap gld_fb_idx and alt_fb_idx to achieve our objective. We do it
    // there so that it can be done outside of the recode loop.
    // Note: This is highly specific to the use of ARF as a forward reference,
    // and this needs to be generalized as other uses are implemented
    // (like RTC/temporal scalability).
    return refresh_mask | (cpi->refresh_golden_frame << cpi->alt_fb_idx);
  } else {
#if CONFIG_EXT_REFS
    const int arf_idx = cpi->alt_fb_idx;
#else   // !CONFIG_EXT_REFS
    int arf_idx = cpi->alt_fb_idx;
    if ((cpi->oxcf.pass == 2) && cpi->multi_arf_allowed) {
      const GF_GROUP *const gf_group = &cpi->twopass.gf_group;
      arf_idx = gf_group->arf_update_idx[gf_group->index];
    }
#endif  // CONFIG_EXT_REFS
    return refresh_mask | (cpi->refresh_golden_frame << cpi->gld_fb_idx) |
           (cpi->refresh_alt_ref_frame << arf_idx);
  }
}

#if CONFIG_EXT_TILE
static INLINE int find_identical_tile(
    const int tile_row, const int tile_col,
    TileBufferEnc (*const tile_buffers)[1024]) {
  const MV32 candidate_offset[1] = { { 1, 0 } };
  const uint8_t *const cur_tile_data =
      tile_buffers[tile_row][tile_col].data + 4;
  const size_t cur_tile_size = tile_buffers[tile_row][tile_col].size;

  int i;

  if (tile_row == 0) return 0;

  // (TODO: yunqingwang) For now, only above tile is checked and used.
  // More candidates such as left tile can be added later.
  for (i = 0; i < 1; i++) {
    int row_offset = candidate_offset[0].row;
    int col_offset = candidate_offset[0].col;
    int row = tile_row - row_offset;
    int col = tile_col - col_offset;
    uint8_t tile_hdr;
    const uint8_t *tile_data;
    TileBufferEnc *candidate;

    if (row < 0 || col < 0) continue;

    tile_hdr = *(tile_buffers[row][col].data);

    // Read out tcm bit
    if ((tile_hdr >> 7) == 1) {
      // The candidate is a copy tile itself
      row_offset += tile_hdr & 0x7f;
      row = tile_row - row_offset;
    }

    candidate = &tile_buffers[row][col];

    if (row_offset >= 128 || candidate->size != cur_tile_size) continue;

    tile_data = candidate->data + 4;

    if (memcmp(tile_data, cur_tile_data, cur_tile_size) != 0) continue;

    // Identical tile found
    assert(row_offset > 0);
    return row_offset;
  }

  // No identical tile found
  return 0;
}
#endif  // CONFIG_EXT_TILE

#if !CONFIG_OBU || CONFIG_EXT_TILE
static uint32_t write_tiles(AV1_COMP *const cpi, uint8_t *const dst,
                            unsigned int *max_tile_size,
                            unsigned int *max_tile_col_size) {
  const AV1_COMMON *const cm = &cpi->common;
  aom_writer mode_bc;
  int tile_row, tile_col;
  TOKENEXTRA *(*const tok_buffers)[MAX_TILE_COLS] = cpi->tile_tok;
  TileBufferEnc(*const tile_buffers)[MAX_TILE_COLS] = cpi->tile_buffers;
  uint32_t total_size = 0;
  const int tile_cols = cm->tile_cols;
  const int tile_rows = cm->tile_rows;
  unsigned int tile_size = 0;
  const int have_tiles = tile_cols * tile_rows > 1;
  struct aom_write_bit_buffer wb = { dst, 0 };
  const int n_log2_tiles = cm->log2_tile_rows + cm->log2_tile_cols;
  uint32_t compressed_hdr_size;
  // Fixed size tile groups for the moment
  const int num_tg_hdrs = cm->num_tg;
  const int tg_size =
#if CONFIG_EXT_TILE
      (cm->large_scale_tile)
          ? 1
          :
#endif  // CONFIG_EXT_TILE
          (tile_rows * tile_cols + num_tg_hdrs - 1) / num_tg_hdrs;
  int tile_count = 0;
  int tg_count = 1;
  int tile_size_bytes = 4;
  int tile_col_size_bytes;
  uint32_t uncompressed_hdr_size = 0;
  struct aom_write_bit_buffer tg_params_wb;
  struct aom_write_bit_buffer tile_size_bytes_wb;
  uint32_t saved_offset;
  int mtu_size = cpi->oxcf.mtu;
  int curr_tg_data_size = 0;
  int hdr_size;

  *max_tile_size = 0;
  *max_tile_col_size = 0;

// All tile size fields are output on 4 bytes. A call to remux_tiles will
// later compact the data if smaller headers are adequate.

#if CONFIG_EXT_TILE
  if (cm->large_scale_tile) {
    for (tile_col = 0; tile_col < tile_cols; tile_col++) {
      TileInfo tile_info;
      const int is_last_col = (tile_col == tile_cols - 1);
      const uint32_t col_offset = total_size;

      av1_tile_set_col(&tile_info, cm, tile_col);

      // The last column does not have a column header
      if (!is_last_col) total_size += 4;

      for (tile_row = 0; tile_row < tile_rows; tile_row++) {
        TileBufferEnc *const buf = &tile_buffers[tile_row][tile_col];
        const TOKENEXTRA *tok = tok_buffers[tile_row][tile_col];
        const TOKENEXTRA *tok_end = tok + cpi->tok_count[tile_row][tile_col];
        const int data_offset = have_tiles ? 4 : 0;
        const int tile_idx = tile_row * tile_cols + tile_col;
        TileDataEnc *this_tile = &cpi->tile_data[tile_idx];
        av1_tile_set_row(&tile_info, cm, tile_row);

        buf->data = dst + total_size;

        // Is CONFIG_EXT_TILE = 1, every tile in the row has a header,
        // even for the last one, unless no tiling is used at all.
        total_size += data_offset;
        // Initialise tile context from the frame context
        this_tile->tctx = *cm->fc;
        cpi->td.mb.e_mbd.tile_ctx = &this_tile->tctx;
#if CONFIG_PVQ
        cpi->td.mb.pvq_q = &this_tile->pvq_q;
        cpi->td.mb.daala_enc.state.adapt = &this_tile->tctx.pvq_context;
#endif  // CONFIG_PVQ
#if CONFIG_ANS
        mode_bc.size = 1 << cpi->common.ans_window_size_log2;
#endif
        aom_start_encode(&mode_bc, buf->data + data_offset);
        write_modes(cpi, &tile_info, &mode_bc, &tok, tok_end);
        assert(tok == tok_end);
        aom_stop_encode(&mode_bc);
        tile_size = mode_bc.pos;
#if CONFIG_PVQ
        cpi->td.mb.pvq_q = NULL;
#endif
        buf->size = tile_size;

        // Record the maximum tile size we see, so we can compact headers later.
        *max_tile_size = AOMMAX(*max_tile_size, tile_size);

        if (have_tiles) {
          // tile header: size of this tile, or copy offset
          uint32_t tile_header = tile_size;
          const int tile_copy_mode =
              ((AOMMAX(cm->tile_width, cm->tile_height) << MI_SIZE_LOG2) <= 256)
                  ? 1
                  : 0;

          // If tile_copy_mode = 1, check if this tile is a copy tile.
          // Very low chances to have copy tiles on the key frames, so don't
          // search on key frames to reduce unnecessary search.
          if (cm->frame_type != KEY_FRAME && tile_copy_mode) {
            const int idendical_tile_offset =
                find_identical_tile(tile_row, tile_col, tile_buffers);

            if (idendical_tile_offset > 0) {
              tile_size = 0;
              tile_header = idendical_tile_offset | 0x80;
              tile_header <<= 24;
            }
          }

          mem_put_le32(buf->data, tile_header);
        }

        total_size += tile_size;
      }

      if (!is_last_col) {
        uint32_t col_size = total_size - col_offset - 4;
        mem_put_le32(dst + col_offset, col_size);

        // If it is not final packing, record the maximum tile column size we
        // see, otherwise, check if the tile size is out of the range.
        *max_tile_col_size = AOMMAX(*max_tile_col_size, col_size);
      }
    }
  } else {
#endif  // CONFIG_EXT_TILE
    write_uncompressed_header_frame(cpi, &wb);

#if CONFIG_EXT_REFS
    if (cm->show_existing_frame) {
      total_size = aom_wb_bytes_written(&wb);
      return (uint32_t)total_size;
    }
#endif  // CONFIG_EXT_REFS

    // Write the tile length code
    tile_size_bytes_wb = wb;
    aom_wb_write_literal(&wb, 3, 2);

    /* Write a placeholder for the number of tiles in each tile group */
    tg_params_wb = wb;
    saved_offset = wb.bit_offset;
    if (have_tiles) {
      aom_wb_overwrite_literal(&wb, 3, n_log2_tiles);
      aom_wb_overwrite_literal(&wb, (1 << n_log2_tiles) - 1, n_log2_tiles);
    }

    if (!use_compressed_header(cm)) {
      uncompressed_hdr_size = aom_wb_bytes_written(&wb);
      compressed_hdr_size = 0;
    } else {
      /* Write a placeholder for the compressed header length */
      struct aom_write_bit_buffer comp_hdr_len_wb = wb;
      aom_wb_write_literal(&wb, 0, 16);

      uncompressed_hdr_size = aom_wb_bytes_written(&wb);
      compressed_hdr_size =
          write_compressed_header(cpi, dst + uncompressed_hdr_size);
      aom_wb_overwrite_literal(&comp_hdr_len_wb, (int)(compressed_hdr_size),
                               16);
    }

    hdr_size = uncompressed_hdr_size + compressed_hdr_size;
    total_size += hdr_size;

    for (tile_row = 0; tile_row < tile_rows; tile_row++) {
      TileInfo tile_info;
      const int is_last_row = (tile_row == tile_rows - 1);
      av1_tile_set_row(&tile_info, cm, tile_row);

      for (tile_col = 0; tile_col < tile_cols; tile_col++) {
        const int tile_idx = tile_row * tile_cols + tile_col;
        TileBufferEnc *const buf = &tile_buffers[tile_row][tile_col];
        TileDataEnc *this_tile = &cpi->tile_data[tile_idx];
        const TOKENEXTRA *tok = tok_buffers[tile_row][tile_col];
        const TOKENEXTRA *tok_end = tok + cpi->tok_count[tile_row][tile_col];
        const int is_last_col = (tile_col == tile_cols - 1);
        const int is_last_tile = is_last_col && is_last_row;

        if ((!mtu_size && tile_count > tg_size) ||
            (mtu_size && tile_count && curr_tg_data_size >= mtu_size)) {
          // New tile group
          tg_count++;
          // We've exceeded the packet size
          if (tile_count > 1) {
            /* The last tile exceeded the packet size. The tile group size
               should therefore be tile_count-1.
               Move the last tile and insert headers before it
             */
            uint32_t old_total_size = total_size - tile_size - 4;
            memmove(dst + old_total_size + hdr_size, dst + old_total_size,
                    (tile_size + 4) * sizeof(uint8_t));
            // Copy uncompressed header
            memmove(dst + old_total_size, dst,
                    uncompressed_hdr_size * sizeof(uint8_t));
            // Write the number of tiles in the group into the last uncompressed
            // header before the one we've just inserted
            aom_wb_overwrite_literal(&tg_params_wb, tile_idx - tile_count,
                                     n_log2_tiles);
            aom_wb_overwrite_literal(&tg_params_wb, tile_count - 2,
                                     n_log2_tiles);
            // Update the pointer to the last TG params
            tg_params_wb.bit_offset = saved_offset + 8 * old_total_size;
            // Copy compressed header
            memmove(dst + old_total_size + uncompressed_hdr_size,
                    dst + uncompressed_hdr_size,
                    compressed_hdr_size * sizeof(uint8_t));
            total_size += hdr_size;
            tile_count = 1;
            curr_tg_data_size = hdr_size + tile_size + 4;
          } else {
            // We exceeded the packet size in just one tile
            // Copy uncompressed header
            memmove(dst + total_size, dst,
                    uncompressed_hdr_size * sizeof(uint8_t));
            // Write the number of tiles in the group into the last uncompressed
            // header
            aom_wb_overwrite_literal(&tg_params_wb, tile_idx - tile_count,
                                     n_log2_tiles);
            aom_wb_overwrite_literal(&tg_params_wb, tile_count - 1,
                                     n_log2_tiles);
            tg_params_wb.bit_offset = saved_offset + 8 * total_size;
            // Copy compressed header
            memmove(dst + total_size + uncompressed_hdr_size,
                    dst + uncompressed_hdr_size,
                    compressed_hdr_size * sizeof(uint8_t));
            total_size += hdr_size;
            tile_count = 0;
            curr_tg_data_size = hdr_size;
          }
        }
        tile_count++;
        av1_tile_set_col(&tile_info, cm, tile_col);

#if CONFIG_DEPENDENT_HORZTILES
        av1_tile_set_tg_boundary(&tile_info, cm, tile_row, tile_col);
#endif
        buf->data = dst + total_size;

        // The last tile does not have a header.
        if (!is_last_tile) total_size += 4;

        // Initialise tile context from the frame context
        this_tile->tctx = *cm->fc;
        cpi->td.mb.e_mbd.tile_ctx = &this_tile->tctx;
#if CONFIG_PVQ
        cpi->td.mb.pvq_q = &this_tile->pvq_q;
        cpi->td.mb.daala_enc.state.adapt = &this_tile->tctx.pvq_context;
#endif  // CONFIG_PVQ
#if CONFIG_ANS
        mode_bc.size = 1 << cpi->common.ans_window_size_log2;
#endif  // CONFIG_ANS
#if CONFIG_LOOP_RESTORATION
        for (int p = 0; p < MAX_MB_PLANE; ++p) {
          set_default_wiener(cpi->td.mb.e_mbd.wiener_info + p);
          set_default_sgrproj(cpi->td.mb.e_mbd.sgrproj_info + p);
        }
#endif  // CONFIG_LOOP_RESTORATION

        aom_start_encode(&mode_bc, dst + total_size);
        write_modes(cpi, &tile_info, &mode_bc, &tok, tok_end);
#if !CONFIG_LV_MAP
#if !CONFIG_PVQ
        assert(tok == tok_end);
#endif  // !CONFIG_PVQ
#endif  // !CONFIG_LV_MAP
        aom_stop_encode(&mode_bc);
        tile_size = mode_bc.pos;
#if CONFIG_PVQ
        cpi->td.mb.pvq_q = NULL;
#endif

        assert(tile_size > 0);

        curr_tg_data_size += tile_size + 4;
        buf->size = tile_size;

        if (!is_last_tile) {
          *max_tile_size = AOMMAX(*max_tile_size, tile_size);
          // size of this tile
          mem_put_le32(buf->data, tile_size);
        }

        total_size += tile_size;
      }
    }
    // Write the final tile group size
    if (n_log2_tiles) {
      aom_wb_overwrite_literal(
          &tg_params_wb, (tile_cols * tile_rows) - tile_count, n_log2_tiles);
      aom_wb_overwrite_literal(&tg_params_wb, tile_count - 1, n_log2_tiles);
    }
    // Remux if possible. TODO (Thomas Davies): do this for more than one tile
    // group
    if (have_tiles && tg_count == 1) {
      int data_size =
          total_size - (uncompressed_hdr_size + compressed_hdr_size);
      data_size =
          remux_tiles(cm, dst + uncompressed_hdr_size + compressed_hdr_size,
                      data_size, *max_tile_size, *max_tile_col_size,
                      &tile_size_bytes, &tile_col_size_bytes);
      total_size = data_size + uncompressed_hdr_size + compressed_hdr_size;
      aom_wb_overwrite_literal(&tile_size_bytes_wb, tile_size_bytes - 1, 2);
    }

#if CONFIG_EXT_TILE
  }
#endif  // CONFIG_EXT_TILE
  return (uint32_t)total_size;
}
#endif

static void write_render_size(const AV1_COMMON *cm,
                              struct aom_write_bit_buffer *wb) {
  const int scaling_active = !av1_resize_unscaled(cm);
  aom_wb_write_bit(wb, scaling_active);
  if (scaling_active) {
    aom_wb_write_literal(wb, cm->render_width - 1, 16);
    aom_wb_write_literal(wb, cm->render_height - 1, 16);
  }
}

#if CONFIG_FRAME_SUPERRES
static void write_superres_scale(const AV1_COMMON *const cm,
                                 struct aom_write_bit_buffer *wb) {
  // First bit is whether to to scale or not
  if (cm->superres_scale_denominator == SCALE_NUMERATOR) {
    aom_wb_write_bit(wb, 0);  // no scaling
  } else {
    aom_wb_write_bit(wb, 1);  // scaling, write scale factor
    aom_wb_write_literal(
        wb, cm->superres_scale_denominator - SUPERRES_SCALE_DENOMINATOR_MIN,
        SUPERRES_SCALE_BITS);
  }
}
#endif  // CONFIG_FRAME_SUPERRES

static void write_frame_size(const AV1_COMMON *cm,
                             struct aom_write_bit_buffer *wb) {
#if CONFIG_FRAME_SUPERRES
  aom_wb_write_literal(wb, cm->superres_upscaled_width - 1, 16);
  aom_wb_write_literal(wb, cm->superres_upscaled_height - 1, 16);
  write_superres_scale(cm, wb);
#else
  aom_wb_write_literal(wb, cm->width - 1, 16);
  aom_wb_write_literal(wb, cm->height - 1, 16);
#endif  // CONFIG_FRAME_SUPERRES
  write_render_size(cm, wb);
}

static void write_frame_size_with_refs(AV1_COMP *cpi,
                                       struct aom_write_bit_buffer *wb) {
  AV1_COMMON *const cm = &cpi->common;
  int found = 0;

  MV_REFERENCE_FRAME ref_frame;
  for (ref_frame = LAST_FRAME; ref_frame <= ALTREF_FRAME; ++ref_frame) {
    YV12_BUFFER_CONFIG *cfg = get_ref_frame_buffer(cpi, ref_frame);

    if (cfg != NULL) {
#if CONFIG_FRAME_SUPERRES
      found = cm->superres_upscaled_width == cfg->y_crop_width &&
              cm->superres_upscaled_height == cfg->y_crop_height;
#else
      found =
          cm->width == cfg->y_crop_width && cm->height == cfg->y_crop_height;
#endif
      found &= cm->render_width == cfg->render_width &&
               cm->render_height == cfg->render_height;
    }
    aom_wb_write_bit(wb, found);
    if (found) {
#if CONFIG_FRAME_SUPERRES
      write_superres_scale(cm, wb);
#endif  // CONFIG_FRAME_SUPERRES
      break;
    }
  }

  if (!found) write_frame_size(cm, wb);
}

static void write_profile(BITSTREAM_PROFILE profile,
                          struct aom_write_bit_buffer *wb) {
  switch (profile) {
    case PROFILE_0: aom_wb_write_literal(wb, 0, 2); break;
    case PROFILE_1: aom_wb_write_literal(wb, 2, 2); break;
    case PROFILE_2: aom_wb_write_literal(wb, 1, 2); break;
    case PROFILE_3: aom_wb_write_literal(wb, 6, 3); break;
    default: assert(0);
  }
}

static void write_bitdepth_colorspace_sampling(
    AV1_COMMON *const cm, struct aom_write_bit_buffer *wb) {
  if (cm->profile >= PROFILE_2) {
    assert(cm->bit_depth > AOM_BITS_8);
    aom_wb_write_bit(wb, cm->bit_depth == AOM_BITS_10 ? 0 : 1);
  }
#if CONFIG_COLORSPACE_HEADERS
  aom_wb_write_literal(wb, cm->color_space, 5);
  aom_wb_write_literal(wb, cm->transfer_function, 5);
#else
  aom_wb_write_literal(wb, cm->color_space, 3);
#endif
  if (cm->color_space != AOM_CS_SRGB) {
    // 0: [16, 235] (i.e. xvYCC), 1: [0, 255]
    aom_wb_write_bit(wb, cm->color_range);
    if (cm->profile == PROFILE_1 || cm->profile == PROFILE_3) {
      assert(cm->subsampling_x != 1 || cm->subsampling_y != 1);
      aom_wb_write_bit(wb, cm->subsampling_x);
      aom_wb_write_bit(wb, cm->subsampling_y);
      aom_wb_write_bit(wb, 0);  // unused
    } else {
      assert(cm->subsampling_x == 1 && cm->subsampling_y == 1);
    }
#if CONFIG_COLORSPACE_HEADERS
    if (cm->subsampling_x == 1 && cm->subsampling_y == 1) {
      aom_wb_write_literal(wb, cm->chroma_sample_position, 2);
    }
#endif
  } else {
    assert(cm->profile == PROFILE_1 || cm->profile == PROFILE_3);
    aom_wb_write_bit(wb, 0);  // unused
  }
}

#if CONFIG_REFERENCE_BUFFER
void write_sequence_header(AV1_COMMON *const cm,
                           struct aom_write_bit_buffer *wb) {
  SequenceHeader *seq_params = &cm->seq_params;
  /* Placeholder for actually writing to the bitstream */
  seq_params->frame_id_numbers_present_flag =
#if CONFIG_EXT_TILE
      cm->large_scale_tile ? 0 :
#endif  // CONFIG_EXT_TILE
                           FRAME_ID_NUMBERS_PRESENT_FLAG;
  seq_params->frame_id_length_minus7 = FRAME_ID_LENGTH_MINUS7;
  seq_params->delta_frame_id_length_minus2 = DELTA_FRAME_ID_LENGTH_MINUS2;

  aom_wb_write_bit(wb, seq_params->frame_id_numbers_present_flag);
  if (seq_params->frame_id_numbers_present_flag) {
    aom_wb_write_literal(wb, seq_params->frame_id_length_minus7, 4);
    aom_wb_write_literal(wb, seq_params->delta_frame_id_length_minus2, 4);
  }
}
#endif  // CONFIG_REFERENCE_BUFFER

static void write_sb_size(const AV1_COMMON *cm,
                          struct aom_write_bit_buffer *wb) {
  (void)cm;
  (void)wb;
  assert(cm->mib_size == mi_size_wide[cm->sb_size]);
  assert(cm->mib_size == 1 << cm->mib_size_log2);
#if CONFIG_EXT_PARTITION
  assert(cm->sb_size == BLOCK_128X128 || cm->sb_size == BLOCK_64X64);
  aom_wb_write_bit(wb, cm->sb_size == BLOCK_128X128 ? 1 : 0);
#else
  assert(cm->sb_size == BLOCK_64X64);
#endif  // CONFIG_EXT_PARTITION
}

static void write_compound_tools(const AV1_COMMON *cm,
                                 struct aom_write_bit_buffer *wb) {
  (void)cm;
  (void)wb;
#if CONFIG_INTERINTRA
  if (!frame_is_intra_only(cm) && cm->reference_mode != COMPOUND_REFERENCE) {
    aom_wb_write_bit(wb, cm->allow_interintra_compound);
  } else {
    assert(cm->allow_interintra_compound == 0);
  }
#endif  // CONFIG_INTERINTRA
#if CONFIG_WEDGE || CONFIG_COMPOUND_SEGMENT
#if CONFIG_COMPOUND_SINGLEREF
  if (!frame_is_intra_only(cm)) {
#else   // !CONFIG_COMPOUND_SINGLEREF
  if (!frame_is_intra_only(cm) && cm->reference_mode != SINGLE_REFERENCE) {
#endif  // CONFIG_COMPOUND_SINGLEREF
    aom_wb_write_bit(wb, cm->allow_masked_compound);
  } else {
    assert(cm->allow_masked_compound == 0);
  }
#endif  // CONFIG_WEDGE || CONFIG_COMPOUND_SEGMENT
}

#if CONFIG_GLOBAL_MOTION
static void write_global_motion_params(const WarpedMotionParams *params,
                                       const WarpedMotionParams *ref_params,
                                       struct aom_write_bit_buffer *wb,
                                       int allow_hp) {
  TransformationType type = params->wmtype;
  int trans_bits;
  int trans_prec_diff;

  aom_wb_write_bit(wb, type != IDENTITY);
  if (type != IDENTITY) {
#if GLOBAL_TRANS_TYPES > 4
    aom_wb_write_literal(wb, type - 1, GLOBAL_TYPE_BITS);
#else
    aom_wb_write_bit(wb, type == ROTZOOM);
    if (type != ROTZOOM) aom_wb_write_bit(wb, type == TRANSLATION);
#endif  // GLOBAL_TRANS_TYPES > 4
  }

  switch (type) {
    case HOMOGRAPHY:
    case HORTRAPEZOID:
    case VERTRAPEZOID:
      if (type != HORTRAPEZOID)
        aom_wb_write_signed_primitive_refsubexpfin(
            wb, GM_ROW3HOMO_MAX + 1, SUBEXPFIN_K,
            (ref_params->wmmat[6] >> GM_ROW3HOMO_PREC_DIFF),
            (params->wmmat[6] >> GM_ROW3HOMO_PREC_DIFF));
      if (type != VERTRAPEZOID)
        aom_wb_write_signed_primitive_refsubexpfin(
            wb, GM_ROW3HOMO_MAX + 1, SUBEXPFIN_K,
            (ref_params->wmmat[7] >> GM_ROW3HOMO_PREC_DIFF),
            (params->wmmat[7] >> GM_ROW3HOMO_PREC_DIFF));
    // fallthrough intended
    case AFFINE:
    case ROTZOOM:
      aom_wb_write_signed_primitive_refsubexpfin(
          wb, GM_ALPHA_MAX + 1, SUBEXPFIN_K,
          (ref_params->wmmat[2] >> GM_ALPHA_PREC_DIFF) -
              (1 << GM_ALPHA_PREC_BITS),
          (params->wmmat[2] >> GM_ALPHA_PREC_DIFF) - (1 << GM_ALPHA_PREC_BITS));
      if (type != VERTRAPEZOID)
        aom_wb_write_signed_primitive_refsubexpfin(
            wb, GM_ALPHA_MAX + 1, SUBEXPFIN_K,
            (ref_params->wmmat[3] >> GM_ALPHA_PREC_DIFF),
            (params->wmmat[3] >> GM_ALPHA_PREC_DIFF));
      if (type >= AFFINE) {
        if (type != HORTRAPEZOID)
          aom_wb_write_signed_primitive_refsubexpfin(
              wb, GM_ALPHA_MAX + 1, SUBEXPFIN_K,
              (ref_params->wmmat[4] >> GM_ALPHA_PREC_DIFF),
              (params->wmmat[4] >> GM_ALPHA_PREC_DIFF));
        aom_wb_write_signed_primitive_refsubexpfin(
            wb, GM_ALPHA_MAX + 1, SUBEXPFIN_K,
            (ref_params->wmmat[5] >> GM_ALPHA_PREC_DIFF) -
                (1 << GM_ALPHA_PREC_BITS),
            (params->wmmat[5] >> GM_ALPHA_PREC_DIFF) -
                (1 << GM_ALPHA_PREC_BITS));
      }
    // fallthrough intended
    case TRANSLATION:
      trans_bits = (type == TRANSLATION) ? GM_ABS_TRANS_ONLY_BITS - !allow_hp
                                         : GM_ABS_TRANS_BITS;
      trans_prec_diff = (type == TRANSLATION)
                            ? GM_TRANS_ONLY_PREC_DIFF + !allow_hp
                            : GM_TRANS_PREC_DIFF;
      aom_wb_write_signed_primitive_refsubexpfin(
          wb, (1 << trans_bits) + 1, SUBEXPFIN_K,
          (ref_params->wmmat[0] >> trans_prec_diff),
          (params->wmmat[0] >> trans_prec_diff));
      aom_wb_write_signed_primitive_refsubexpfin(
          wb, (1 << trans_bits) + 1, SUBEXPFIN_K,
          (ref_params->wmmat[1] >> trans_prec_diff),
          (params->wmmat[1] >> trans_prec_diff));
      break;
    case IDENTITY: break;
    default: assert(0);
  }
}

static void write_global_motion(AV1_COMP *cpi,
                                struct aom_write_bit_buffer *wb) {
  AV1_COMMON *const cm = &cpi->common;
  int frame;
  for (frame = LAST_FRAME; frame <= ALTREF_FRAME; ++frame) {
    const WarpedMotionParams *ref_params =
        cm->error_resilient_mode ? &default_warp_params
                                 : &cm->prev_frame->global_motion[frame];
    write_global_motion_params(&cm->global_motion[frame], ref_params, wb,
                               cm->allow_high_precision_mv);
    // TODO(sarahparker, debargha): The logic in the commented out code below
    // does not work currently and causes mismatches when resize is on.
    // Fix it before turning the optimization back on.
    /*
    YV12_BUFFER_CONFIG *ref_buf = get_ref_frame_buffer(cpi, frame);
    if (cpi->source->y_crop_width == ref_buf->y_crop_width &&
        cpi->source->y_crop_height == ref_buf->y_crop_height) {
      write_global_motion_params(&cm->global_motion[frame],
                                 &cm->prev_frame->global_motion[frame], wb,
                                 cm->allow_high_precision_mv);
    } else {
      assert(cm->global_motion[frame].wmtype == IDENTITY &&
             "Invalid warp type for frames of different resolutions");
    }
    */
    /*
    printf("Frame %d/%d: Enc Ref %d: %d %d %d %d\n",
           cm->current_video_frame, cm->show_frame, frame,
           cm->global_motion[frame].wmmat[0],
           cm->global_motion[frame].wmmat[1], cm->global_motion[frame].wmmat[2],
           cm->global_motion[frame].wmmat[3]);
           */
  }
}
#endif

#if !CONFIG_OBU
static void write_uncompressed_header_frame(AV1_COMP *cpi,
                                            struct aom_write_bit_buffer *wb) {
  AV1_COMMON *const cm = &cpi->common;
  MACROBLOCKD *const xd = &cpi->td.mb.e_mbd;

  aom_wb_write_literal(wb, AOM_FRAME_MARKER, 2);

  write_profile(cm->profile, wb);

#if CONFIG_EXT_TILE
  aom_wb_write_literal(wb, cm->large_scale_tile, 1);
#endif  // CONFIG_EXT_TILE

#if CONFIG_EXT_REFS
  // NOTE: By default all coded frames to be used as a reference
  cm->is_reference_frame = 1;

  if (cm->show_existing_frame) {
    RefCntBuffer *const frame_bufs = cm->buffer_pool->frame_bufs;
    const int frame_to_show = cm->ref_frame_map[cpi->existing_fb_idx_to_show];

    if (frame_to_show < 0 || frame_bufs[frame_to_show].ref_count < 1) {
      aom_internal_error(&cm->error, AOM_CODEC_UNSUP_BITSTREAM,
                         "Buffer %d does not contain a reconstructed frame",
                         frame_to_show);
    }
    ref_cnt_fb(frame_bufs, &cm->new_fb_idx, frame_to_show);

    aom_wb_write_bit(wb, 1);  // show_existing_frame
    aom_wb_write_literal(wb, cpi->existing_fb_idx_to_show, 3);

#if CONFIG_REFERENCE_BUFFER
    if (cm->seq_params.frame_id_numbers_present_flag) {
      int frame_id_len = cm->seq_params.frame_id_length_minus7 + 7;
      int display_frame_id = cm->ref_frame_id[cpi->existing_fb_idx_to_show];
      aom_wb_write_literal(wb, display_frame_id, frame_id_len);
      /* Add a zero byte to prevent emulation of superframe marker */
      /* Same logic as when when terminating the entropy coder */
      /* Consider to have this logic only one place */
      aom_wb_write_literal(wb, 0, 8);
    }
#endif  // CONFIG_REFERENCE_BUFFER

    return;
  } else {
#endif                        // CONFIG_EXT_REFS
    aom_wb_write_bit(wb, 0);  // show_existing_frame
#if CONFIG_EXT_REFS
  }
#endif  // CONFIG_EXT_REFS

  aom_wb_write_bit(wb, cm->frame_type);
  aom_wb_write_bit(wb, cm->show_frame);
  if (cm->frame_type != KEY_FRAME)
    if (!cm->show_frame) aom_wb_write_bit(wb, cm->intra_only);
  aom_wb_write_bit(wb, cm->error_resilient_mode);

  if (frame_is_intra_only(cm)) {
#if CONFIG_REFERENCE_BUFFER
    write_sequence_header(cm, wb);
#endif  // CONFIG_REFERENCE_BUFFER
  }
#if CONFIG_REFERENCE_BUFFER
  cm->invalid_delta_frame_id_minus1 = 0;
  if (cm->seq_params.frame_id_numbers_present_flag) {
    int frame_id_len = cm->seq_params.frame_id_length_minus7 + 7;
    aom_wb_write_literal(wb, cm->current_frame_id, frame_id_len);
  }
#endif  // CONFIG_REFERENCE_BUFFER
  if (cm->frame_type == KEY_FRAME) {
    write_bitdepth_colorspace_sampling(cm, wb);
    write_frame_size(cm, wb);
    write_sb_size(cm, wb);

#if CONFIG_ANS && ANS_MAX_SYMBOLS
    assert(cpi->common.ans_window_size_log2 >= 8);
    assert(cpi->common.ans_window_size_log2 < 24);
    aom_wb_write_literal(wb, cpi->common.ans_window_size_log2 - 8, 4);
#endif  // CONFIG_ANS && ANS_MAX_SYMBOLS
    aom_wb_write_bit(wb, cm->allow_screen_content_tools);
#if CONFIG_AMVR
    if (cm->allow_screen_content_tools) {
      if (cm->seq_mv_precision_level == 2) {
        aom_wb_write_bit(wb, 1);
      } else {
        aom_wb_write_bit(wb, 0);
        aom_wb_write_bit(wb, cm->seq_mv_precision_level == 0);
      }
    }
#endif
  } else {
    if (cm->intra_only) aom_wb_write_bit(wb, cm->allow_screen_content_tools);
#if !CONFIG_NO_FRAME_CONTEXT_SIGNALING
    if (!cm->error_resilient_mode) {
      if (cm->intra_only) {
        aom_wb_write_bit(wb,
                         cm->reset_frame_context == RESET_FRAME_CONTEXT_ALL);
      } else {
        aom_wb_write_bit(wb,
                         cm->reset_frame_context != RESET_FRAME_CONTEXT_NONE);
        if (cm->reset_frame_context != RESET_FRAME_CONTEXT_NONE)
          aom_wb_write_bit(wb,
                           cm->reset_frame_context == RESET_FRAME_CONTEXT_ALL);
      }
    }
#endif
#if CONFIG_EXT_REFS
    cpi->refresh_frame_mask = get_refresh_mask(cpi);
#endif  // CONFIG_EXT_REFS

    if (cm->intra_only) {
      write_bitdepth_colorspace_sampling(cm, wb);

#if CONFIG_EXT_REFS
      aom_wb_write_literal(wb, cpi->refresh_frame_mask, REF_FRAMES);
#else
      aom_wb_write_literal(wb, get_refresh_mask(cpi), REF_FRAMES);
#endif  // CONFIG_EXT_REFS
      write_frame_size(cm, wb);

#if CONFIG_ANS && ANS_MAX_SYMBOLS
      assert(cpi->common.ans_window_size_log2 >= 8);
      assert(cpi->common.ans_window_size_log2 < 24);
      aom_wb_write_literal(wb, cpi->common.ans_window_size_log2 - 8, 4);
#endif  // CONFIG_ANS && ANS_MAX_SYMBOLS
    } else {
      MV_REFERENCE_FRAME ref_frame;

#if CONFIG_EXT_REFS
      aom_wb_write_literal(wb, cpi->refresh_frame_mask, REF_FRAMES);
#else
      aom_wb_write_literal(wb, get_refresh_mask(cpi), REF_FRAMES);
#endif  // CONFIG_EXT_REFS

#if CONFIG_EXT_REFS
      if (!cpi->refresh_frame_mask) {
        // NOTE: "cpi->refresh_frame_mask == 0" indicates that the coded frame
        //       will not be used as a reference
        cm->is_reference_frame = 0;
      }
#endif  // CONFIG_EXT_REFS

      for (ref_frame = LAST_FRAME; ref_frame <= ALTREF_FRAME; ++ref_frame) {
        assert(get_ref_frame_map_idx(cpi, ref_frame) != INVALID_IDX);
        aom_wb_write_literal(wb, get_ref_frame_map_idx(cpi, ref_frame),
                             REF_FRAMES_LOG2);
#if !CONFIG_FRAME_SIGN_BIAS
        aom_wb_write_bit(wb, cm->ref_frame_sign_bias[ref_frame]);
#endif  // !CONFIG_FRAME_SIGN_BIAS
#if CONFIG_REFERENCE_BUFFER
        if (cm->seq_params.frame_id_numbers_present_flag) {
          int i = get_ref_frame_map_idx(cpi, ref_frame);
          int frame_id_len = cm->seq_params.frame_id_length_minus7 + 7;
          int diff_len = cm->seq_params.delta_frame_id_length_minus2 + 2;
          int delta_frame_id_minus1 =
              ((cm->current_frame_id - cm->ref_frame_id[i] +
                (1 << frame_id_len)) %
               (1 << frame_id_len)) -
              1;
          if (delta_frame_id_minus1 < 0 ||
              delta_frame_id_minus1 >= (1 << diff_len))
            cm->invalid_delta_frame_id_minus1 = 1;
          aom_wb_write_literal(wb, delta_frame_id_minus1, diff_len);
        }
#endif  // CONFIG_REFERENCE_BUFFER
      }

#if CONFIG_FRAME_SIGN_BIAS
#define FRAME_SIGN_BIAS_DEBUG 0
#if FRAME_SIGN_BIAS_DEBUG
      {
        printf("\n\nENCODER: Frame=%d, show_frame=%d:", cm->current_video_frame,
               cm->show_frame);
        for (ref_frame = LAST_FRAME; ref_frame <= ALTREF_FRAME; ++ref_frame) {
          printf(" sign_bias[%d]=%d", ref_frame,
                 cm->ref_frame_sign_bias[ref_frame]);
        }
        printf("\n");
      }
#endif  // FRAME_SIGN_BIAS_DEBUG
#undef FRAME_SIGN_BIAS_DEBUG
#endif  // CONFIG_FRAME_SIGN_BIAS

#if CONFIG_FRAME_SIZE
      if (cm->error_resilient_mode == 0) {
        write_frame_size_with_refs(cpi, wb);
      } else {
        write_frame_size(cm, wb);
      }
#else
      write_frame_size_with_refs(cpi, wb);
#endif

#if CONFIG_AMVR
      if (cm->seq_mv_precision_level == 2) {
        aom_wb_write_bit(wb, cm->cur_frame_mv_precision_level == 0);
      }
#endif
      aom_wb_write_bit(wb, cm->allow_high_precision_mv);

      fix_interp_filter(cm, cpi->td.counts);
      write_frame_interp_filter(cm->interp_filter, wb);
#if CONFIG_TEMPMV_SIGNALING
      if (frame_might_use_prev_frame_mvs(cm)) {
        aom_wb_write_bit(wb, cm->use_prev_frame_mvs);
      }
#endif
    }
  }

#if CONFIG_FRAME_MARKER
  if (cm->show_frame == 0) {
    int arf_offset = AOMMIN(
        (MAX_GF_INTERVAL - 1),
        cpi->twopass.gf_group.arf_src_offset[cpi->twopass.gf_group.index]);
#if CONFIG_EXT_REFS
    int brf_offset =
        cpi->twopass.gf_group.brf_src_offset[cpi->twopass.gf_group.index];

    arf_offset = AOMMIN((MAX_GF_INTERVAL - 1), arf_offset + brf_offset);
#endif
    aom_wb_write_literal(wb, arf_offset, 4);
  }
#endif

#if CONFIG_REFERENCE_BUFFER
  if (cm->seq_params.frame_id_numbers_present_flag) {
    cm->refresh_mask =
        cm->frame_type == KEY_FRAME ? 0xFF : get_refresh_mask(cpi);
  }
#endif  // CONFIG_REFERENCE_BUFFER

  if (!cm->error_resilient_mode) {
    aom_wb_write_bit(
        wb, cm->refresh_frame_context == REFRESH_FRAME_CONTEXT_FORWARD);
  }
#if !CONFIG_NO_FRAME_CONTEXT_SIGNALING
  aom_wb_write_literal(wb, cm->frame_context_idx, FRAME_CONTEXTS_LOG2);
#endif
  encode_loopfilter(cm, wb);
  encode_quantization(cm, wb);
  encode_segmentation(cm, xd, wb);
  {
    int i;
    struct segmentation *const seg = &cm->seg;
    int segment_quantizer_active = 0;
    for (i = 0; i < MAX_SEGMENTS; i++) {
      if (segfeature_active(seg, i, SEG_LVL_ALT_Q)) {
        segment_quantizer_active = 1;
      }
    }

    if (cm->delta_q_present_flag)
      assert(segment_quantizer_active == 0 && cm->base_qindex > 0);
    if (segment_quantizer_active == 0 && cm->base_qindex > 0) {
      aom_wb_write_bit(wb, cm->delta_q_present_flag);
      if (cm->delta_q_present_flag) {
        aom_wb_write_literal(wb, OD_ILOG_NZ(cm->delta_q_res) - 1, 2);
        xd->prev_qindex = cm->base_qindex;
#if CONFIG_EXT_DELTA_Q
        assert(seg->abs_delta == SEGMENT_DELTADATA);
        aom_wb_write_bit(wb, cm->delta_lf_present_flag);
        if (cm->delta_lf_present_flag) {
          aom_wb_write_literal(wb, OD_ILOG_NZ(cm->delta_lf_res) - 1, 2);
          xd->prev_delta_lf_from_base = 0;
#if CONFIG_LOOPFILTER_LEVEL
          aom_wb_write_bit(wb, cm->delta_lf_multi);
          for (int lf_id = 0; lf_id < FRAME_LF_COUNT; ++lf_id)
            xd->prev_delta_lf[lf_id] = 0;
#endif  // CONFIG_LOOPFILTER_LEVEL
        }
#endif  // CONFIG_EXT_DELTA_Q
      }
    }
  }
#if CONFIG_CDEF
  if (!cm->all_lossless) {
    encode_cdef(cm, wb);
  }
#endif
#if CONFIG_LOOP_RESTORATION
  encode_restoration_mode(cm, wb);
#endif  // CONFIG_LOOP_RESTORATION
  write_tx_mode(cm, &cm->tx_mode, wb);

  if (cpi->allow_comp_inter_inter) {
    const int use_hybrid_pred = cm->reference_mode == REFERENCE_MODE_SELECT;
#if !CONFIG_REF_ADAPT
    const int use_compound_pred = cm->reference_mode != SINGLE_REFERENCE;
#endif  // !CONFIG_REF_ADAPT

    aom_wb_write_bit(wb, use_hybrid_pred);
#if !CONFIG_REF_ADAPT
    if (!use_hybrid_pred) aom_wb_write_bit(wb, use_compound_pred);
#endif  // !CONFIG_REF_ADAPT
  }
  write_compound_tools(cm, wb);

#if CONFIG_EXT_TX
  aom_wb_write_bit(wb, cm->reduced_tx_set_used);
#endif  // CONFIG_EXT_TX

#if CONFIG_ADAPT_SCAN
  aom_wb_write_bit(wb, cm->use_adapt_scan);
#endif

#if CONFIG_GLOBAL_MOTION
  if (!frame_is_intra_only(cm)) write_global_motion(cpi, wb);
#endif  // CONFIG_GLOBAL_MOTION

  write_tile_info(cm, wb);
}

#else
// New function based on HLS R18
static void write_uncompressed_header_obu(AV1_COMP *cpi,
                                          struct aom_write_bit_buffer *wb) {
  AV1_COMMON *const cm = &cpi->common;
  MACROBLOCKD *const xd = &cpi->td.mb.e_mbd;

#if CONFIG_EXT_TILE
  aom_wb_write_literal(wb, cm->large_scale_tile, 1);
#endif  // CONFIG_EXT_TILE

#if CONFIG_EXT_REFS
  // NOTE: By default all coded frames to be used as a reference
  cm->is_reference_frame = 1;

  if (cm->show_existing_frame) {
    RefCntBuffer *const frame_bufs = cm->buffer_pool->frame_bufs;
    const int frame_to_show = cm->ref_frame_map[cpi->existing_fb_idx_to_show];

    if (frame_to_show < 0 || frame_bufs[frame_to_show].ref_count < 1) {
      aom_internal_error(&cm->error, AOM_CODEC_UNSUP_BITSTREAM,
                         "Buffer %d does not contain a reconstructed frame",
                         frame_to_show);
    }
    ref_cnt_fb(frame_bufs, &cm->new_fb_idx, frame_to_show);

    aom_wb_write_bit(wb, 1);  // show_existing_frame
    aom_wb_write_literal(wb, cpi->existing_fb_idx_to_show, 3);

#if CONFIG_REFERENCE_BUFFER
    if (cm->seq_params.frame_id_numbers_present_flag) {
      int frame_id_len = cm->seq_params.frame_id_length_minus7 + 7;
      int display_frame_id = cm->ref_frame_id[cpi->existing_fb_idx_to_show];
      aom_wb_write_literal(wb, display_frame_id, frame_id_len);
      /* Add a zero byte to prevent emulation of superframe marker */
      /* Same logic as when when terminating the entropy coder */
      /* Consider to have this logic only one place */
      aom_wb_write_literal(wb, 0, 8);
    }
#endif  // CONFIG_REFERENCE_BUFFER

    return;
  } else {
#endif  // CONFIG_EXT_REFS
    aom_wb_write_bit(wb, 0);  // show_existing_frame
#if CONFIG_EXT_REFS
  }
#endif  // CONFIG_EXT_REFS

  cm->frame_type = cm->intra_only ? INTRA_ONLY_FRAME : cm->frame_type;
  aom_wb_write_literal(wb, cm->frame_type, 2);

  if (cm->intra_only) cm->frame_type = INTRA_ONLY_FRAME;

  aom_wb_write_bit(wb, cm->show_frame);
  aom_wb_write_bit(wb, cm->error_resilient_mode);

#if CONFIG_REFERENCE_BUFFER
  cm->invalid_delta_frame_id_minus1 = 0;
  if (cm->seq_params.frame_id_numbers_present_flag) {
    int frame_id_len = cm->seq_params.frame_id_length_minus7 + 7;
    aom_wb_write_literal(wb, cm->current_frame_id, frame_id_len);
  }
#endif  // CONFIG_REFERENCE_BUFFER
  if (cm->frame_type == KEY_FRAME) {
    write_frame_size(cm, wb);
    write_sb_size(cm, wb);

#if CONFIG_ANS && ANS_MAX_SYMBOLS
    assert(cpi->common.ans_window_size_log2 >= 8);
    assert(cpi->common.ans_window_size_log2 < 24);
    aom_wb_write_literal(wb, cpi->common.ans_window_size_log2 - 8, 4);
#endif  // CONFIG_ANS && ANS_MAX_SYMBOLS
    aom_wb_write_bit(wb, cm->allow_screen_content_tools);
#if CONFIG_AMVR
    if (cm->allow_screen_content_tools) {
      if (cm->seq_mv_precision_level == 2) {
        aom_wb_write_bit(wb, 1);
      } else {
        aom_wb_write_bit(wb, 0);
        aom_wb_write_bit(wb, cm->seq_mv_precision_level == 0);
      }
    }
#endif
  } else if (cm->frame_type == INTRA_ONLY_FRAME) {
    if (cm->intra_only) aom_wb_write_bit(wb, cm->allow_screen_content_tools);
#if !CONFIG_NO_FRAME_CONTEXT_SIGNALING
    if (!cm->error_resilient_mode) {
      if (cm->intra_only) {
        aom_wb_write_bit(wb,
                         cm->reset_frame_context == RESET_FRAME_CONTEXT_ALL);
      }
    }
#endif
#if CONFIG_EXT_REFS
    cpi->refresh_frame_mask = get_refresh_mask(cpi);
#endif  // CONFIG_EXT_REFS

    if (cm->intra_only) {
#if CONFIG_EXT_REFS
      aom_wb_write_literal(wb, cpi->refresh_frame_mask, REF_FRAMES);
#else
      aom_wb_write_literal(wb, get_refresh_mask(cpi), REF_FRAMES);
#endif  // CONFIG_EXT_REFS
      write_frame_size(cm, wb);

#if CONFIG_ANS && ANS_MAX_SYMBOLS
      assert(cpi->common.ans_window_size_log2 >= 8);
      assert(cpi->common.ans_window_size_log2 < 24);
      aom_wb_write_literal(wb, cpi->common.ans_window_size_log2 - 8, 4);
#endif  // CONFIG_ANS && ANS_MAX_SYMBOLS
    }
  } else if (cm->frame_type == INTER_FRAME) {
    MV_REFERENCE_FRAME ref_frame;
#if !CONFIG_NO_FRAME_CONTEXT_SIGNALING
    if (!cm->error_resilient_mode) {
      aom_wb_write_bit(wb, cm->reset_frame_context != RESET_FRAME_CONTEXT_NONE);
      if (cm->reset_frame_context != RESET_FRAME_CONTEXT_NONE)
        aom_wb_write_bit(wb,
                         cm->reset_frame_context == RESET_FRAME_CONTEXT_ALL);
    }
#endif

#if CONFIG_EXT_REFS
    cpi->refresh_frame_mask = get_refresh_mask(cpi);
    aom_wb_write_literal(wb, cpi->refresh_frame_mask, REF_FRAMES);
#else
    aom_wb_write_literal(wb, get_refresh_mask(cpi), REF_FRAMES);
#endif  // CONFIG_EXT_REFS

#if CONFIG_EXT_REFS
    if (!cpi->refresh_frame_mask) {
      // NOTE: "cpi->refresh_frame_mask == 0" indicates that the coded frame
      //       will not be used as a reference
      cm->is_reference_frame = 0;
    }
#endif  // CONFIG_EXT_REFS

    for (ref_frame = LAST_FRAME; ref_frame <= ALTREF_FRAME; ++ref_frame) {
      assert(get_ref_frame_map_idx(cpi, ref_frame) != INVALID_IDX);
      aom_wb_write_literal(wb, get_ref_frame_map_idx(cpi, ref_frame),
                           REF_FRAMES_LOG2);
#if !CONFIG_FRAME_SIGN_BIAS
      aom_wb_write_bit(wb, cm->ref_frame_sign_bias[ref_frame]);
#endif  // !CONFIG_FRAME_SIGN_BIAS
#if CONFIG_REFERENCE_BUFFER
      if (cm->seq_params.frame_id_numbers_present_flag) {
        int i = get_ref_frame_map_idx(cpi, ref_frame);
        int frame_id_len = cm->seq_params.frame_id_length_minus7 + 7;
        int diff_len = cm->seq_params.delta_frame_id_length_minus2 + 2;
        int delta_frame_id_minus1 =
            ((cm->current_frame_id - cm->ref_frame_id[i] +
              (1 << frame_id_len)) %
             (1 << frame_id_len)) -
            1;
        if (delta_frame_id_minus1 < 0 ||
            delta_frame_id_minus1 >= (1 << diff_len))
          cm->invalid_delta_frame_id_minus1 = 1;
        aom_wb_write_literal(wb, delta_frame_id_minus1, diff_len);
      }
#endif  // CONFIG_REFERENCE_BUFFER
    }

#if CONFIG_FRAME_SIZE
    if (cm->error_resilient_mode == 0) {
      write_frame_size_with_refs(cpi, wb);
    } else {
      write_frame_size(cm, wb);
    }
#else
    write_frame_size_with_refs(cpi, wb);
#endif

#if CONFIG_AMVR
    if (cm->seq_mv_precision_level == 2) {
      aom_wb_write_bit(wb, cm->cur_frame_mv_precision_level == 0);
    }
#endif
    aom_wb_write_bit(wb, cm->allow_high_precision_mv);

    fix_interp_filter(cm, cpi->td.counts);
    write_frame_interp_filter(cm->interp_filter, wb);
#if CONFIG_TEMPMV_SIGNALING
    if (frame_might_use_prev_frame_mvs(cm)) {
      aom_wb_write_bit(wb, cm->use_prev_frame_mvs);
    }
#endif
  } else if (cm->frame_type == S_FRAME) {
    MV_REFERENCE_FRAME ref_frame;

#if !CONFIG_NO_FRAME_CONTEXT_SIGNALING
    if (!cm->error_resilient_mode) {
      aom_wb_write_bit(wb, cm->reset_frame_context != RESET_FRAME_CONTEXT_NONE);
      if (cm->reset_frame_context != RESET_FRAME_CONTEXT_NONE)
        aom_wb_write_bit(wb,
                         cm->reset_frame_context == RESET_FRAME_CONTEXT_ALL);
    }
#endif

#if CONFIG_EXT_REFS
    if (!cpi->refresh_frame_mask) {
      // NOTE: "cpi->refresh_frame_mask == 0" indicates that the coded frame
      //       will not be used as a reference
      cm->is_reference_frame = 0;
    }
#endif  // CONFIG_EXT_REFS

    for (ref_frame = LAST_FRAME; ref_frame <= ALTREF_FRAME; ++ref_frame) {
      assert(get_ref_frame_map_idx(cpi, ref_frame) != INVALID_IDX);
      aom_wb_write_literal(wb, get_ref_frame_map_idx(cpi, ref_frame),
                           REF_FRAMES_LOG2);
      assert(cm->ref_frame_sign_bias[ref_frame] == 0);
#if CONFIG_REFERENCE_BUFFER
      if (cm->seq_params.frame_id_numbers_present_flag) {
        int i = get_ref_frame_map_idx(cpi, ref_frame);
        int frame_id_len = cm->seq_params.frame_id_length_minus7 + 7;
        int diff_len = cm->seq_params.delta_frame_id_length_minus2 + 2;
        int delta_frame_id_minus1 =
            ((cm->current_frame_id - cm->ref_frame_id[i] +
              (1 << frame_id_len)) %
             (1 << frame_id_len)) -
            1;
        if (delta_frame_id_minus1 < 0 ||
            delta_frame_id_minus1 >= (1 << diff_len))
          cm->invalid_delta_frame_id_minus1 = 1;
        aom_wb_write_literal(wb, delta_frame_id_minus1, diff_len);
      }
#endif  // CONFIG_REFERENCE_BUFFER
    }

#if CONFIG_FRAME_SIZE
    if (cm->error_resilient_mode == 0) {
      write_frame_size_with_refs(cpi, wb);
    } else {
      write_frame_size(cm, wb);
    }
#else
    write_frame_size_with_refs(cpi, wb);
#endif

    aom_wb_write_bit(wb, cm->allow_high_precision_mv);

    fix_interp_filter(cm, cpi->td.counts);
    write_frame_interp_filter(cm->interp_filter, wb);
#if CONFIG_TEMPMV_SIGNALING
    if (frame_might_use_prev_frame_mvs(cm)) {
      aom_wb_write_bit(wb, cm->use_prev_frame_mvs);
    }
#endif
  }

#if CONFIG_MFMV
  if (cm->show_frame == 0) {
    int arf_offset = AOMMIN(
        (MAX_GF_INTERVAL - 1),
        cpi->twopass.gf_group.arf_src_offset[cpi->twopass.gf_group.index]);
#if CONFIG_EXT_REFS
    int brf_offset =
        cpi->twopass.gf_group.brf_src_offset[cpi->twopass.gf_group.index];

    arf_offset = AOMMIN((MAX_GF_INTERVAL - 1), arf_offset + brf_offset);
#endif
    aom_wb_write_literal(wb, arf_offset, 4);
  }
#endif

#if CONFIG_REFERENCE_BUFFER
  if (cm->seq_params.frame_id_numbers_present_flag) {
    cm->refresh_mask =
        cm->frame_type == KEY_FRAME ? 0xFF : get_refresh_mask(cpi);
  }
#endif  // CONFIG_REFERENCE_BUFFER

  if (!cm->error_resilient_mode) {
    aom_wb_write_bit(
        wb, cm->refresh_frame_context == REFRESH_FRAME_CONTEXT_FORWARD);
  }
#if !CONFIG_NO_FRAME_CONTEXT_SIGNALING
  aom_wb_write_literal(wb, cm->frame_context_idx, FRAME_CONTEXTS_LOG2);
#endif
  encode_loopfilter(cm, wb);
  encode_quantization(cm, wb);
  encode_segmentation(cm, xd, wb);
  {
    int i;
    struct segmentation *const seg = &cm->seg;
    int segment_quantizer_active = 0;
    for (i = 0; i < MAX_SEGMENTS; i++) {
      if (segfeature_active(seg, i, SEG_LVL_ALT_Q)) {
        segment_quantizer_active = 1;
      }
    }

    if (cm->delta_q_present_flag)
      assert(segment_quantizer_active == 0 && cm->base_qindex > 0);
    if (segment_quantizer_active == 0 && cm->base_qindex > 0) {
      aom_wb_write_bit(wb, cm->delta_q_present_flag);
      if (cm->delta_q_present_flag) {
        aom_wb_write_literal(wb, OD_ILOG_NZ(cm->delta_q_res) - 1, 2);
        xd->prev_qindex = cm->base_qindex;
#if CONFIG_EXT_DELTA_Q
        assert(seg->abs_delta == SEGMENT_DELTADATA);
        aom_wb_write_bit(wb, cm->delta_lf_present_flag);
        if (cm->delta_lf_present_flag) {
          aom_wb_write_literal(wb, OD_ILOG_NZ(cm->delta_lf_res) - 1, 2);
#if CONFIG_LOOPFILTER_LEVEL
          for (int lf_id = 0; lf_id < FRAME_LF_COUNT; ++lf_id)
            xd->prev_delta_lf[lf_id] = 0;
#endif  // CONFIG_LOOPFILTER_LEVEL
          xd->prev_delta_lf_from_base = 0;
        }
#endif  // CONFIG_EXT_DELTA_Q
      }
    }
  }
#if CONFIG_CDEF
  if (!cm->all_lossless) {
    encode_cdef(cm, wb);
  }
#endif
#if CONFIG_LOOP_RESTORATION
  encode_restoration_mode(cm, wb);
#endif  // CONFIG_LOOP_RESTORATION
  write_tx_mode(cm, &cm->tx_mode, wb);

  if (cpi->allow_comp_inter_inter) {
    const int use_hybrid_pred = cm->reference_mode == REFERENCE_MODE_SELECT;
#if !CONFIG_REF_ADAPT
    const int use_compound_pred = cm->reference_mode != SINGLE_REFERENCE;
#endif  // !CONFIG_REF_ADAPT

    aom_wb_write_bit(wb, use_hybrid_pred);
#if !CONFIG_REF_ADAPT
    if (!use_hybrid_pred) aom_wb_write_bit(wb, use_compound_pred);
#endif  // !CONFIG_REF_ADAPT
  }
  write_compound_tools(cm, wb);

#if CONFIG_EXT_TX
  aom_wb_write_bit(wb, cm->reduced_tx_set_used);
#endif  // CONFIG_EXT_TX

#if CONFIG_GLOBAL_MOTION
  if (!frame_is_intra_only(cm)) write_global_motion(cpi, wb);
#endif  // CONFIG_GLOBAL_MOTION

  write_tile_info(cm, wb);
}
#endif  // CONFIG_OBU

static uint32_t write_compressed_header(AV1_COMP *cpi, uint8_t *data) {
  AV1_COMMON *const cm = &cpi->common;
#if CONFIG_SUPERTX
  MACROBLOCKD *const xd = &cpi->td.mb.e_mbd;
#endif  // CONFIG_SUPERTX
  FRAME_CONTEXT *const fc = cm->fc;
  aom_writer *header_bc;
  int i;
#if !CONFIG_NEW_MULTISYMBOL
  FRAME_COUNTS *counts = cpi->td.counts;
  int j;
#endif

  const int probwt = cm->num_tg;
  (void)probwt;
  (void)i;
  (void)fc;

  aom_writer real_header_bc;
  header_bc = &real_header_bc;
#if CONFIG_ANS
  header_bc->size = 1 << cpi->common.ans_window_size_log2;
#endif
  aom_start_encode(header_bc, data);

#if CONFIG_RECT_TX_EXT && (CONFIG_EXT_TX || CONFIG_VAR_TX)
  if (cm->tx_mode == TX_MODE_SELECT)
    av1_cond_prob_diff_update(header_bc, &cm->fc->quarter_tx_size_prob,
                              cm->counts.quarter_tx_size, probwt);
#endif
#if CONFIG_LV_MAP
  av1_write_txb_probs(cpi, header_bc);
#endif  // CONFIG_LV_MAP

#if CONFIG_VAR_TX && !CONFIG_NEW_MULTISYMBOL
  if (cm->tx_mode == TX_MODE_SELECT)
    update_txfm_partition_probs(cm, header_bc, counts, probwt);
#endif

#if !CONFIG_NEW_MULTISYMBOL
  update_skip_probs(cm, header_bc, counts);
#endif

  if (!frame_is_intra_only(cm)) {
#if !CONFIG_NEW_MULTISYMBOL
    update_inter_mode_probs(cm, header_bc, counts);
#endif
#if CONFIG_INTERINTRA
    if (cm->reference_mode != COMPOUND_REFERENCE &&
        cm->allow_interintra_compound) {
#if !CONFIG_NEW_MULTISYMBOL
      for (i = 0; i < BLOCK_SIZE_GROUPS; i++) {
        if (is_interintra_allowed_bsize_group(i)) {
          av1_cond_prob_diff_update(header_bc, &fc->interintra_prob[i],
                                    cm->counts.interintra[i], probwt);
        }
      }
#endif
#if CONFIG_WEDGE && !CONFIG_NEW_MULTISYMBOL
#if CONFIG_EXT_PARTITION_TYPES
      int block_sizes_to_update = BLOCK_SIZES_ALL;
#else
      int block_sizes_to_update = BLOCK_SIZES;
#endif
      for (i = 0; i < block_sizes_to_update; i++) {
        if (is_interintra_allowed_bsize(i) && is_interintra_wedge_used(i))
          av1_cond_prob_diff_update(header_bc, &fc->wedge_interintra_prob[i],
                                    cm->counts.wedge_interintra[i], probwt);
      }
#endif  // CONFIG_WEDGE && CONFIG_NEW_MULTISYMBOL
    }
#endif  // CONFIG_INTERINTRA

#if !CONFIG_NEW_MULTISYMBOL
    for (i = 0; i < INTRA_INTER_CONTEXTS; i++)
      av1_cond_prob_diff_update(header_bc, &fc->intra_inter_prob[i],
                                counts->intra_inter[i], probwt);
#endif

#if !CONFIG_NEW_MULTISYMBOL
    if (cpi->allow_comp_inter_inter) {
      const int use_hybrid_pred = cm->reference_mode == REFERENCE_MODE_SELECT;
      if (use_hybrid_pred)
        for (i = 0; i < COMP_INTER_CONTEXTS; i++)
          av1_cond_prob_diff_update(header_bc, &fc->comp_inter_prob[i],
                                    counts->comp_inter[i], probwt);
    }

    if (cm->reference_mode != COMPOUND_REFERENCE) {
      for (i = 0; i < REF_CONTEXTS; i++) {
        for (j = 0; j < (SINGLE_REFS - 1); j++) {
          av1_cond_prob_diff_update(header_bc, &fc->single_ref_prob[i][j],
                                    counts->single_ref[i][j], probwt);
        }
      }
    }

    if (cm->reference_mode != SINGLE_REFERENCE) {
#if CONFIG_EXT_COMP_REFS
      for (i = 0; i < COMP_REF_TYPE_CONTEXTS; i++)
        av1_cond_prob_diff_update(header_bc, &fc->comp_ref_type_prob[i],
                                  counts->comp_ref_type[i], probwt);

      for (i = 0; i < UNI_COMP_REF_CONTEXTS; i++)
        for (j = 0; j < (UNIDIR_COMP_REFS - 1); j++)
          av1_cond_prob_diff_update(header_bc, &fc->uni_comp_ref_prob[i][j],
                                    counts->uni_comp_ref[i][j], probwt);
#endif  // CONFIG_EXT_COMP_REFS

      for (i = 0; i < REF_CONTEXTS; i++) {
#if CONFIG_EXT_REFS
        for (j = 0; j < (FWD_REFS - 1); j++) {
          av1_cond_prob_diff_update(header_bc, &fc->comp_ref_prob[i][j],
                                    counts->comp_ref[i][j], probwt);
        }
        for (j = 0; j < (BWD_REFS - 1); j++) {
          av1_cond_prob_diff_update(header_bc, &fc->comp_bwdref_prob[i][j],
                                    counts->comp_bwdref[i][j], probwt);
        }
#else
        for (j = 0; j < (COMP_REFS - 1); j++) {
          av1_cond_prob_diff_update(header_bc, &fc->comp_ref_prob[i][j],
                                    counts->comp_ref[i][j], probwt);
        }
#endif  // CONFIG_EXT_REFS
      }
    }
#endif  // CONFIG_NEW_MULTISYMBOL

#if CONFIG_COMPOUND_SINGLEREF
    for (i = 0; i < COMP_INTER_MODE_CONTEXTS; i++)
      av1_cond_prob_diff_update(header_bc, &fc->comp_inter_mode_prob[i],
                                counts->comp_inter_mode[i], probwt);
#endif  // CONFIG_COMPOUND_SINGLEREF

#if !CONFIG_NEW_MULTISYMBOL
    av1_write_nmv_probs(cm, cm->allow_high_precision_mv, header_bc, counts->mv);
#endif
#if CONFIG_SUPERTX
    if (!xd->lossless[0]) update_supertx_probs(cm, probwt, header_bc);
#endif  // CONFIG_SUPERTX
  }
  aom_stop_encode(header_bc);
  assert(header_bc->pos <= 0xffff);
  return header_bc->pos;
}

#if !CONFIG_OBU || CONFIG_EXT_TILE
static int choose_size_bytes(uint32_t size, int spare_msbs) {
  // Choose the number of bytes required to represent size, without
  // using the 'spare_msbs' number of most significant bits.

  // Make sure we will fit in 4 bytes to start with..
  if (spare_msbs > 0 && size >> (32 - spare_msbs) != 0) return -1;

  // Normalise to 32 bits
  size <<= spare_msbs;

  if (size >> 24 != 0)
    return 4;
  else if (size >> 16 != 0)
    return 3;
  else if (size >> 8 != 0)
    return 2;
  else
    return 1;
}

static void mem_put_varsize(uint8_t *const dst, const int sz, const int val) {
  switch (sz) {
    case 1: dst[0] = (uint8_t)(val & 0xff); break;
    case 2: mem_put_le16(dst, val); break;
    case 3: mem_put_le24(dst, val); break;
    case 4: mem_put_le32(dst, val); break;
    default: assert(0 && "Invalid size"); break;
  }
}

static int remux_tiles(const AV1_COMMON *const cm, uint8_t *dst,
                       const uint32_t data_size, const uint32_t max_tile_size,
                       const uint32_t max_tile_col_size,
                       int *const tile_size_bytes,
                       int *const tile_col_size_bytes) {
  // Choose the tile size bytes (tsb) and tile column size bytes (tcsb)
  int tsb;
  int tcsb;

#if CONFIG_EXT_TILE
  if (cm->large_scale_tile) {
    // The top bit in the tile size field indicates tile copy mode, so we
    // have 1 less bit to code the tile size
    tsb = choose_size_bytes(max_tile_size, 1);
    tcsb = choose_size_bytes(max_tile_col_size, 0);
  } else {
#endif  // CONFIG_EXT_TILE
    tsb = choose_size_bytes(max_tile_size, 0);
    tcsb = 4;  // This is ignored
    (void)max_tile_col_size;
#if CONFIG_EXT_TILE
  }
#endif  // CONFIG_EXT_TILE

  assert(tsb > 0);
  assert(tcsb > 0);

  *tile_size_bytes = tsb;
  *tile_col_size_bytes = tcsb;

  if (tsb == 4 && tcsb == 4) {
    return data_size;
  } else {
    uint32_t wpos = 0;
    uint32_t rpos = 0;

#if CONFIG_EXT_TILE
    if (cm->large_scale_tile) {
      int tile_row;
      int tile_col;

      for (tile_col = 0; tile_col < cm->tile_cols; tile_col++) {
        // All but the last column has a column header
        if (tile_col < cm->tile_cols - 1) {
          uint32_t tile_col_size = mem_get_le32(dst + rpos);
          rpos += 4;

          // Adjust the tile column size by the number of bytes removed
          // from the tile size fields.
          tile_col_size -= (4 - tsb) * cm->tile_rows;

          mem_put_varsize(dst + wpos, tcsb, tile_col_size);
          wpos += tcsb;
        }

        for (tile_row = 0; tile_row < cm->tile_rows; tile_row++) {
          // All, including the last row has a header
          uint32_t tile_header = mem_get_le32(dst + rpos);
          rpos += 4;

          // If this is a copy tile, we need to shift the MSB to the
          // top bit of the new width, and there is no data to copy.
          if (tile_header >> 31 != 0) {
            if (tsb < 4) tile_header >>= 32 - 8 * tsb;
            mem_put_varsize(dst + wpos, tsb, tile_header);
            wpos += tsb;
          } else {
            mem_put_varsize(dst + wpos, tsb, tile_header);
            wpos += tsb;

            memmove(dst + wpos, dst + rpos, tile_header);
            rpos += tile_header;
            wpos += tile_header;
          }
        }
      }
    } else {
#endif  // CONFIG_EXT_TILE
      const int n_tiles = cm->tile_cols * cm->tile_rows;
      int n;

      for (n = 0; n < n_tiles; n++) {
        int tile_size;

        if (n == n_tiles - 1) {
          tile_size = data_size - rpos;
        } else {
          tile_size = mem_get_le32(dst + rpos);
          rpos += 4;
          mem_put_varsize(dst + wpos, tsb, tile_size);
          wpos += tsb;
        }

        memmove(dst + wpos, dst + rpos, tile_size);

        rpos += tile_size;
        wpos += tile_size;
      }
#if CONFIG_EXT_TILE
    }
#endif  // CONFIG_EXT_TILE

    assert(rpos > wpos);
    assert(rpos == data_size);

    return wpos;
  }
}
#endif

#if CONFIG_OBU
static uint32_t write_obu_header(OBU_TYPE obu_type, int obu_extension,
                                 uint8_t *const dst) {
  struct aom_write_bit_buffer wb = { dst, 0 };
  uint32_t size = 0;

  aom_wb_write_literal(&wb, (int)obu_type, 5);
  aom_wb_write_literal(&wb, 0, 2);
  aom_wb_write_literal(&wb, obu_extension ? 1 : 0, 1);
  if (obu_extension) {
    aom_wb_write_literal(&wb, obu_extension & 0xFF, 8);
  }

  size = aom_wb_bytes_written(&wb);
  return size;
}

static uint32_t write_temporal_delimiter_obu() { return 0; }

static uint32_t write_sequence_header_obu(AV1_COMP *cpi, uint8_t *const dst) {
  AV1_COMMON *const cm = &cpi->common;
  SequenceHeader *const seq_params = &cm->seq_params;
  struct aom_write_bit_buffer wb = { dst, 0 };
  uint32_t size = 0;

  write_profile(cm->profile, &wb);

  aom_wb_write_literal(&wb, 0, 4);

  seq_params->frame_id_numbers_present_flag = FRAME_ID_NUMBERS_PRESENT_FLAG;
  aom_wb_write_literal(&wb, seq_params->frame_id_numbers_present_flag, 1);
  if (seq_params->frame_id_numbers_present_flag) {
    seq_params->frame_id_length_minus7 = FRAME_ID_LENGTH_MINUS7;
    seq_params->delta_frame_id_length_minus2 = DELTA_FRAME_ID_LENGTH_MINUS2;
    aom_wb_write_literal(&wb, seq_params->frame_id_length_minus7, 4);
    aom_wb_write_literal(&wb, seq_params->delta_frame_id_length_minus2, 4);
  }

  // color_config
  write_bitdepth_colorspace_sampling(cm, &wb);

  size = aom_wb_bytes_written(&wb);
  return size;
}

static uint32_t write_frame_header_obu(AV1_COMP *cpi, uint8_t *const dst) {
  AV1_COMMON *const cm = &cpi->common;
  struct aom_write_bit_buffer wb = { dst, 0 };
  uint32_t total_size = 0;
  uint32_t compressed_hdr_size, uncompressed_hdr_size;

  write_uncompressed_header_obu(cpi, &wb);

  if (cm->show_existing_frame) {
    total_size = aom_wb_bytes_written(&wb);
    return total_size;
  }

  // write the tile length code  (Always 4 bytes for now)
  aom_wb_write_literal(&wb, 3, 2);

  if (!use_compressed_header(cm)) {
    uncompressed_hdr_size = aom_wb_bytes_written(&wb);
    compressed_hdr_size = 0;
  } else {
    // placeholder for the compressed header length
    struct aom_write_bit_buffer compr_hdr_len_wb = wb;
    aom_wb_write_literal(&wb, 0, 16);

    uncompressed_hdr_size = aom_wb_bytes_written(&wb);
    compressed_hdr_size =
        write_compressed_header(cpi, dst + uncompressed_hdr_size);
    aom_wb_overwrite_literal(&compr_hdr_len_wb, (int)(compressed_hdr_size), 16);
  }

  total_size = uncompressed_hdr_size + compressed_hdr_size;
  return total_size;
}

static uint32_t write_tile_group_header(uint8_t *const dst, int startTile,
                                        int endTile, int tiles_log2) {
  struct aom_write_bit_buffer wb = { dst, 0 };
  uint32_t size = 0;

  aom_wb_write_literal(&wb, startTile, tiles_log2);
  aom_wb_write_literal(&wb, endTile, tiles_log2);

  size = aom_wb_bytes_written(&wb);
  return size;
}

static uint32_t write_tiles_in_tg_obus(AV1_COMP *const cpi, uint8_t *const dst,
                                       unsigned int *max_tile_size,
                                       unsigned int *max_tile_col_size,
                                       uint8_t *const frame_header_obu_location,
                                       uint32_t frame_header_obu_size,
                                       int insert_frame_header_obu_flag) {
  const AV1_COMMON *const cm = &cpi->common;
  aom_writer mode_bc;
  int tile_row, tile_col;
  TOKENEXTRA *(*const tok_buffers)[MAX_TILE_COLS] = cpi->tile_tok;
  TileBufferEnc(*const tile_buffers)[MAX_TILE_COLS] = cpi->tile_buffers;
  uint32_t total_size = 0;
  const int tile_cols = cm->tile_cols;
  const int tile_rows = cm->tile_rows;
  unsigned int tile_size = 0;
  const int n_log2_tiles = cm->log2_tile_rows + cm->log2_tile_cols;
  // Fixed size tile groups for the moment
  const int num_tg_hdrs = cm->num_tg;
  const int tg_size =
#if CONFIG_EXT_TILE
      (cm->large_scale_tile)
          ? 1
          :
#endif  // CONFIG_EXT_TILE
          (tile_rows * tile_cols + num_tg_hdrs - 1) / num_tg_hdrs;
  int tile_count = 0;
  int curr_tg_data_size = 0;
  uint8_t *data = dst;
  int new_tg = 1;
#if CONFIG_EXT_TILE
  const int have_tiles = tile_cols * tile_rows > 1;
#endif

  *max_tile_size = 0;
  *max_tile_col_size = 0;

#if CONFIG_EXT_TILE
  if (cm->large_scale_tile) {
    for (tile_col = 0; tile_col < tile_cols; tile_col++) {
      TileInfo tile_info;
      const int is_last_col = (tile_col == tile_cols - 1);
      const uint32_t col_offset = total_size;

      av1_tile_set_col(&tile_info, cm, tile_col);

      // The last column does not have a column header
      if (!is_last_col) total_size += 4;

      for (tile_row = 0; tile_row < tile_rows; tile_row++) {
        TileBufferEnc *const buf = &tile_buffers[tile_row][tile_col];
        const TOKENEXTRA *tok = tok_buffers[tile_row][tile_col];
        const TOKENEXTRA *tok_end = tok + cpi->tok_count[tile_row][tile_col];
        const int data_offset = have_tiles ? 4 : 0;
        const int tile_idx = tile_row * tile_cols + tile_col;
        TileDataEnc *this_tile = &cpi->tile_data[tile_idx];
        av1_tile_set_row(&tile_info, cm, tile_row);

        buf->data = dst + total_size;

        // Is CONFIG_EXT_TILE = 1, every tile in the row has a header,
        // even for the last one, unless no tiling is used at all.
        total_size += data_offset;
        // Initialise tile context from the frame context
        this_tile->tctx = *cm->fc;
        cpi->td.mb.e_mbd.tile_ctx = &this_tile->tctx;
#if CONFIG_PVQ
        cpi->td.mb.pvq_q = &this_tile->pvq_q;
        cpi->td.mb.daala_enc.state.adapt = &this_tile->tctx.pvq_context;
#endif  // CONFIG_PVQ
#if CONFIG_ANS
        mode_bc.size = 1 << cpi->common.ans_window_size_log2;
#endif
        aom_start_encode(&mode_bc, buf->data + data_offset);
        write_modes(cpi, &tile_info, &mode_bc, &tok, tok_end);
        assert(tok == tok_end);
        aom_stop_encode(&mode_bc);
        tile_size = mode_bc.pos;
#if CONFIG_PVQ
        cpi->td.mb.pvq_q = NULL;
#endif
        buf->size = tile_size;

        // Record the maximum tile size we see, so we can compact headers later.
        *max_tile_size = AOMMAX(*max_tile_size, tile_size);

        if (have_tiles) {
          // tile header: size of this tile, or copy offset
          uint32_t tile_header = tile_size;
          const int tile_copy_mode =
              ((AOMMAX(cm->tile_width, cm->tile_height) << MI_SIZE_LOG2) <= 256)
                  ? 1
                  : 0;

          // If tile_copy_mode = 1, check if this tile is a copy tile.
          // Very low chances to have copy tiles on the key frames, so don't
          // search on key frames to reduce unnecessary search.
          if (cm->frame_type != KEY_FRAME && tile_copy_mode) {
            const int idendical_tile_offset =
                find_identical_tile(tile_row, tile_col, tile_buffers);

            if (idendical_tile_offset > 0) {
              tile_size = 0;
              tile_header = idendical_tile_offset | 0x80;
              tile_header <<= 24;
            }
          }

          mem_put_le32(buf->data, tile_header);
        }

        total_size += tile_size;
      }

      if (!is_last_col) {
        uint32_t col_size = total_size - col_offset - 4;
        mem_put_le32(dst + col_offset, col_size);

        // If it is not final packing, record the maximum tile column size we
        // see, otherwise, check if the tile size is out of the range.
        *max_tile_col_size = AOMMAX(*max_tile_col_size, col_size);
      }
    }
  } else {
#endif  // CONFIG_EXT_TILE

    for (tile_row = 0; tile_row < tile_rows; tile_row++) {
      TileInfo tile_info;
      const int is_last_row = (tile_row == tile_rows - 1);
      av1_tile_set_row(&tile_info, cm, tile_row);

      for (tile_col = 0; tile_col < tile_cols; tile_col++) {
        const int tile_idx = tile_row * tile_cols + tile_col;
        TileBufferEnc *const buf = &tile_buffers[tile_row][tile_col];
        TileDataEnc *this_tile = &cpi->tile_data[tile_idx];
        const TOKENEXTRA *tok = tok_buffers[tile_row][tile_col];
        const TOKENEXTRA *tok_end = tok + cpi->tok_count[tile_row][tile_col];
        const int is_last_col = (tile_col == tile_cols - 1);
        const int is_last_tile = is_last_col && is_last_row;
        int is_last_tile_in_tg = 0;

        if (new_tg) {
          if (insert_frame_header_obu_flag && tile_idx) {
            // insert a copy of frame header OBU (including 4-byte size),
            // except before the first tile group
            data = dst + total_size;
            memmove(data, frame_header_obu_location, frame_header_obu_size);
            total_size += frame_header_obu_size;
          }
          data = dst + total_size;
          // A new tile group begins at this tile.  Write the obu header and
          // tile group header
          curr_tg_data_size = write_obu_header(OBU_TILE_GROUP, 0, data + 4);
          if (n_log2_tiles)
            curr_tg_data_size += write_tile_group_header(
                data + curr_tg_data_size + 4, tile_idx,
                AOMMIN(tile_idx + tg_size - 1, tile_cols * tile_rows - 1),
                n_log2_tiles);
          total_size += curr_tg_data_size + 4;
          new_tg = 0;
          tile_count = 0;
        }
        tile_count++;
        av1_tile_set_col(&tile_info, cm, tile_col);

        if (tile_count == tg_size || tile_idx == (tile_cols * tile_rows - 1)) {
          is_last_tile_in_tg = 1;
          new_tg = 1;
        } else {
          is_last_tile_in_tg = 0;
        }

#if CONFIG_DEPENDENT_HORZTILES
        av1_tile_set_tg_boundary(&tile_info, cm, tile_row, tile_col);
#endif
        buf->data = dst + total_size;

        // The last tile of the tile group does not have a header.
        if (!is_last_tile_in_tg) total_size += 4;

        // Initialise tile context from the frame context
        this_tile->tctx = *cm->fc;
        cpi->td.mb.e_mbd.tile_ctx = &this_tile->tctx;
#if CONFIG_PVQ
        cpi->td.mb.pvq_q = &this_tile->pvq_q;
        cpi->td.mb.daala_enc.state.adapt = &this_tile->tctx.pvq_context;
#endif  // CONFIG_PVQ
#if CONFIG_ANS
        mode_bc.size = 1 << cpi->common.ans_window_size_log2;
#endif  // CONFIG_ANS
        aom_start_encode(&mode_bc, dst + total_size);
        write_modes(cpi, &tile_info, &mode_bc, &tok, tok_end);
#if !CONFIG_LV_MAP
#if !CONFIG_PVQ
        assert(tok == tok_end);
#endif  // !CONFIG_PVQ
#endif  // !CONFIG_LV_MAP
        aom_stop_encode(&mode_bc);
        tile_size = mode_bc.pos;
#if CONFIG_PVQ
        cpi->td.mb.pvq_q = NULL;
#endif
        assert(tile_size > 0);

        curr_tg_data_size += (tile_size + (is_last_tile_in_tg ? 0 : 4));
        buf->size = tile_size;

        if (!is_last_tile) {
          *max_tile_size = AOMMAX(*max_tile_size, tile_size);
        }
        if (!is_last_tile_in_tg) {
          // size of this tile
          mem_put_le32(buf->data, tile_size);
        } else {
          // write current tile group size
          mem_put_le32(data, curr_tg_data_size);
        }

        total_size += tile_size;
      }
    }
#if CONFIG_EXT_TILE
  }
#endif  // CONFIG_EXT_TILE
  return (uint32_t)total_size;
}

#endif

void av1_pack_bitstream(AV1_COMP *const cpi, uint8_t *dst, size_t *size) {
  uint8_t *data = dst;
  uint32_t data_size;
#if CONFIG_EXT_TILE
  AV1_COMMON *const cm = &cpi->common;
  uint32_t compressed_hdr_size = 0;
  uint32_t uncompressed_hdr_size;
  struct aom_write_bit_buffer saved_wb;
  struct aom_write_bit_buffer wb = { data, 0 };
  const int have_tiles = cm->tile_cols * cm->tile_rows > 1;
  int tile_size_bytes;
  int tile_col_size_bytes;
#endif  // CONFIG_EXT_TILE
  unsigned int max_tile_size;
  unsigned int max_tile_col_size;
#if CONFIG_OBU
#if !CONFIG_EXT_TILE
  AV1_COMMON *const cm = &cpi->common;
#endif
  uint32_t obu_size;
  uint8_t *frame_header_location;
  uint32_t frame_header_size;
#endif

#if CONFIG_BITSTREAM_DEBUG
  bitstream_queue_reset_write();
#endif

#if CONFIG_OBU
  // write temporal delimiter obu, preceded by 4-byte size
  obu_size = write_obu_header(OBU_TD, 0, data + 4);
  obu_size += write_temporal_delimiter_obu(/*data + 4 + obu_size*/);
  mem_put_le32(data, obu_size);
  data += obu_size + 4;

  // write sequence header obu if KEY_FRAME, preceded by 4-byte size
  if (cm->frame_type == KEY_FRAME) {
    obu_size = write_obu_header(OBU_SEQUENCE_HEADER, 0, data + 4);
    obu_size += write_sequence_header_obu(cpi, data + 4 + obu_size);
    mem_put_le32(data, obu_size);
    data += obu_size + 4;
  }

  // write frame header obu, preceded by 4-byte size
  frame_header_location = data + 4;
  obu_size = write_obu_header(OBU_FRAME_HEADER, 0, frame_header_location);
  frame_header_size = write_frame_header_obu(cpi, data + 4 + obu_size);
  obu_size += frame_header_size;
  mem_put_le32(data, obu_size);
  data += obu_size + 4;

  if (cm->show_existing_frame) {
    data_size = 0;
  } else {
    //  Each tile group obu will be preceded by 4-byte size of the tile group
    //  obu
    data_size =
        write_tiles_in_tg_obus(cpi, data, &max_tile_size, &max_tile_col_size,
                               frame_header_location - 4, obu_size + 4,
                               1 /* cm->error_resilient_mode */);
  }

#endif

#if CONFIG_EXT_TILE
  if (cm->large_scale_tile) {
    // Write the uncompressed header
    write_uncompressed_header_frame(cpi, &wb);

#if CONFIG_EXT_REFS
    if (cm->show_existing_frame) {
      *size = aom_wb_bytes_written(&wb);
      return;
    }
#endif  // CONFIG_EXT_REFS

    // We do not know these in advance. Output placeholder bit.
    saved_wb = wb;
    // Write tile size magnitudes
    if (have_tiles) {
      // Note that the last item in the uncompressed header is the data
      // describing tile configuration.
      // Number of bytes in tile column size - 1
      aom_wb_write_literal(&wb, 0, 2);

      // Number of bytes in tile size - 1
      aom_wb_write_literal(&wb, 0, 2);
    }

    if (!use_compressed_header(cm)) {
      uncompressed_hdr_size = (uint32_t)aom_wb_bytes_written(&wb);
      aom_clear_system_state();
      compressed_hdr_size = 0;
    } else {
      // Size of compressed header
      aom_wb_write_literal(&wb, 0, 16);
      uncompressed_hdr_size = (uint32_t)aom_wb_bytes_written(&wb);
      aom_clear_system_state();
      // Write the compressed header
      compressed_hdr_size =
          write_compressed_header(cpi, data + uncompressed_hdr_size);
    }
    data += uncompressed_hdr_size + compressed_hdr_size;

    // Write the encoded tile data
    data_size = write_tiles(cpi, data, &max_tile_size, &max_tile_col_size);
  } else {
#endif  // CONFIG_EXT_TILE
#if !CONFIG_OBU
    data_size = write_tiles(cpi, data, &max_tile_size, &max_tile_col_size);
#endif
#if CONFIG_EXT_TILE
  }
#endif  // CONFIG_EXT_TILE
#if CONFIG_EXT_TILE
  if (cm->large_scale_tile) {
    if (have_tiles) {
      data_size =
          remux_tiles(cm, data, data_size, max_tile_size, max_tile_col_size,
                      &tile_size_bytes, &tile_col_size_bytes);
    }

    data += data_size;

    // Now fill in the gaps in the uncompressed header.
    if (have_tiles) {
      assert(tile_col_size_bytes >= 1 && tile_col_size_bytes <= 4);
      aom_wb_write_literal(&saved_wb, tile_col_size_bytes - 1, 2);

      assert(tile_size_bytes >= 1 && tile_size_bytes <= 4);
      aom_wb_write_literal(&saved_wb, tile_size_bytes - 1, 2);
    }
    // TODO(jbb): Figure out what to do if compressed_hdr_size > 16 bits.
    assert(compressed_hdr_size <= 0xffff);
    aom_wb_write_literal(&saved_wb, compressed_hdr_size, 16);
  } else {
#endif  // CONFIG_EXT_TILE
    data += data_size;
#if CONFIG_EXT_TILE
  }
#endif  // CONFIG_EXT_TILE
#if CONFIG_ANS && ANS_REVERSE
  // Avoid aliasing the superframe index
  *data++ = 0;
#endif
  *size = data - dst;
}
