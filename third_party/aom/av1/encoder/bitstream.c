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

#include "av1/common/cdef.h"
#include "av1/common/cfl.h"
#include "av1/common/entropy.h"
#include "av1/common/entropymode.h"
#include "av1/common/entropymv.h"
#include "av1/common/mvref_common.h"
#include "av1/common/odintrin.h"
#include "av1/common/pred_common.h"
#include "av1/common/reconinter.h"
#include "av1/common/reconintra.h"
#include "av1/common/seg_common.h"
#include "av1/common/tile_common.h"

#include "av1/encoder/bitstream.h"
#include "av1/encoder/cost.h"
#include "av1/encoder/encodemv.h"
#include "av1/encoder/encodetxb.h"
#include "av1/encoder/mcomp.h"
#include "av1/encoder/palette.h"
#include "av1/encoder/segmentation.h"
#include "av1/encoder/tokenize.h"

#define ENC_MISMATCH_DEBUG 0

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

static void loop_restoration_write_sb_coeffs(const AV1_COMMON *const cm,
                                             MACROBLOCKD *xd,
                                             const RestorationUnitInfo *rui,
                                             aom_writer *const w, int plane,
                                             FRAME_COUNTS *counts);

static void write_intra_mode_kf(FRAME_CONTEXT *frame_ctx,
                                const MB_MODE_INFO *mi,
                                const MB_MODE_INFO *above_mi,
                                const MB_MODE_INFO *left_mi,
                                PREDICTION_MODE mode, aom_writer *w) {
  assert(!is_intrabc_block(mi));
  (void)mi;
  aom_write_symbol(w, mode, get_y_mode_cdf(frame_ctx, above_mi, left_mi),
                   INTRA_MODES);
}

static void write_inter_mode(aom_writer *w, PREDICTION_MODE mode,
                             FRAME_CONTEXT *ec_ctx, const int16_t mode_ctx) {
  const int16_t newmv_ctx = mode_ctx & NEWMV_CTX_MASK;

  aom_write_symbol(w, mode != NEWMV, ec_ctx->newmv_cdf[newmv_ctx], 2);

  if (mode != NEWMV) {
    const int16_t zeromv_ctx =
        (mode_ctx >> GLOBALMV_OFFSET) & GLOBALMV_CTX_MASK;
    aom_write_symbol(w, mode != GLOBALMV, ec_ctx->zeromv_cdf[zeromv_ctx], 2);

    if (mode != GLOBALMV) {
      int16_t refmv_ctx = (mode_ctx >> REFMV_OFFSET) & REFMV_CTX_MASK;
      aom_write_symbol(w, mode != NEARESTMV, ec_ctx->refmv_cdf[refmv_ctx], 2);
    }
  }
}

static void write_drl_idx(FRAME_CONTEXT *ec_ctx, const MB_MODE_INFO *mbmi,
                          const MB_MODE_INFO_EXT *mbmi_ext, aom_writer *w) {
  uint8_t ref_frame_type = av1_ref_frame_type(mbmi->ref_frame);

  assert(mbmi->ref_mv_idx < 3);

  const int new_mv = mbmi->mode == NEWMV || mbmi->mode == NEW_NEWMV;
  if (new_mv) {
    int idx;
    for (idx = 0; idx < 2; ++idx) {
      if (mbmi_ext->ref_mv_count[ref_frame_type] > idx + 1) {
        uint8_t drl_ctx =
            av1_drl_ctx(mbmi_ext->ref_mv_stack[ref_frame_type], idx);

        aom_write_symbol(w, mbmi->ref_mv_idx != idx, ec_ctx->drl_cdf[drl_ctx],
                         2);
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
        aom_write_symbol(w, mbmi->ref_mv_idx != (idx - 1),
                         ec_ctx->drl_cdf[drl_ctx], 2);
        if (mbmi->ref_mv_idx == (idx - 1)) return;
      }
    }
    return;
  }
}

static void write_inter_compound_mode(MACROBLOCKD *xd, aom_writer *w,
                                      PREDICTION_MODE mode,
                                      const int16_t mode_ctx) {
  assert(is_inter_compound_mode(mode));
  aom_write_symbol(w, INTER_COMPOUND_OFFSET(mode),
                   xd->tile_ctx->inter_compound_mode_cdf[mode_ctx],
                   INTER_COMPOUND_MODES);
}

static void write_tx_size_vartx(MACROBLOCKD *xd, const MB_MODE_INFO *mbmi,
                                TX_SIZE tx_size, int depth, int blk_row,
                                int blk_col, aom_writer *w) {
  FRAME_CONTEXT *ec_ctx = xd->tile_ctx;
  const int max_blocks_high = max_block_high(xd, mbmi->sb_type, 0);
  const int max_blocks_wide = max_block_wide(xd, mbmi->sb_type, 0);

  if (blk_row >= max_blocks_high || blk_col >= max_blocks_wide) return;

  if (depth == MAX_VARTX_DEPTH) {
    txfm_partition_update(xd->above_txfm_context + blk_col,
                          xd->left_txfm_context + blk_row, tx_size, tx_size);
    return;
  }

  const int ctx = txfm_partition_context(xd->above_txfm_context + blk_col,
                                         xd->left_txfm_context + blk_row,
                                         mbmi->sb_type, tx_size);
  const int txb_size_index =
      av1_get_txb_size_index(mbmi->sb_type, blk_row, blk_col);
  const int write_txfm_partition =
      tx_size == mbmi->inter_tx_size[txb_size_index];
  if (write_txfm_partition) {
    aom_write_symbol(w, 0, ec_ctx->txfm_partition_cdf[ctx], 2);

    txfm_partition_update(xd->above_txfm_context + blk_col,
                          xd->left_txfm_context + blk_row, tx_size, tx_size);
    // TODO(yuec): set correct txfm partition update for qttx
  } else {
    const TX_SIZE sub_txs = sub_tx_size_map[tx_size];
    const int bsw = tx_size_wide_unit[sub_txs];
    const int bsh = tx_size_high_unit[sub_txs];

    aom_write_symbol(w, 1, ec_ctx->txfm_partition_cdf[ctx], 2);

    if (sub_txs == TX_4X4) {
      txfm_partition_update(xd->above_txfm_context + blk_col,
                            xd->left_txfm_context + blk_row, sub_txs, tx_size);
      return;
    }

    assert(bsw > 0 && bsh > 0);
    for (int row = 0; row < tx_size_high_unit[tx_size]; row += bsh)
      for (int col = 0; col < tx_size_wide_unit[tx_size]; col += bsw) {
        int offsetr = blk_row + row;
        int offsetc = blk_col + col;
        write_tx_size_vartx(xd, mbmi, sub_txs, depth + 1, offsetr, offsetc, w);
      }
  }
}

static void write_selected_tx_size(const MACROBLOCKD *xd, aom_writer *w) {
  const MB_MODE_INFO *const mbmi = xd->mi[0];
  const BLOCK_SIZE bsize = mbmi->sb_type;
  FRAME_CONTEXT *ec_ctx = xd->tile_ctx;
  if (block_signals_txsize(bsize)) {
    const TX_SIZE tx_size = mbmi->tx_size;
    const int tx_size_ctx = get_tx_size_context(xd);
    const int depth = tx_size_to_depth(tx_size, bsize);
    const int max_depths = bsize_to_max_depth(bsize);
    const int32_t tx_size_cat = bsize_to_tx_size_cat(bsize);

    assert(depth >= 0 && depth <= max_depths);
    assert(!is_inter_block(mbmi));
    assert(IMPLIES(is_rect_tx(tx_size), is_rect_tx_allowed(xd, mbmi)));

    aom_write_symbol(w, depth, ec_ctx->tx_size_cdf[tx_size_cat][tx_size_ctx],
                     max_depths + 1);
  }
}

static int write_skip(const AV1_COMMON *cm, const MACROBLOCKD *xd,
                      int segment_id, const MB_MODE_INFO *mi, aom_writer *w) {
  if (segfeature_active(&cm->seg, segment_id, SEG_LVL_SKIP)) {
    return 1;
  } else {
    const int skip = mi->skip;
    const int ctx = av1_get_skip_context(xd);
    FRAME_CONTEXT *ec_ctx = xd->tile_ctx;
    aom_write_symbol(w, skip, ec_ctx->skip_cdfs[ctx], 2);
    return skip;
  }
}

static int write_skip_mode(const AV1_COMMON *cm, const MACROBLOCKD *xd,
                           int segment_id, const MB_MODE_INFO *mi,
                           aom_writer *w) {
  if (!cm->skip_mode_flag) return 0;
  if (segfeature_active(&cm->seg, segment_id, SEG_LVL_SKIP)) {
    return 0;
  }
  const int skip_mode = mi->skip_mode;
  if (!is_comp_ref_allowed(mi->sb_type)) {
    assert(!skip_mode);
    return 0;
  }
  if (segfeature_active(&cm->seg, segment_id, SEG_LVL_REF_FRAME) ||
      segfeature_active(&cm->seg, segment_id, SEG_LVL_GLOBALMV)) {
    // These features imply single-reference mode, while skip mode implies
    // compound reference. Hence, the two are mutually exclusive.
    // In other words, skip_mode is implicitly 0 here.
    assert(!skip_mode);
    return 0;
  }
  const int ctx = av1_get_skip_mode_context(xd);
  aom_write_symbol(w, skip_mode, xd->tile_ctx->skip_mode_cdfs[ctx], 2);
  return skip_mode;
}

static void write_is_inter(const AV1_COMMON *cm, const MACROBLOCKD *xd,
                           int segment_id, aom_writer *w, const int is_inter) {
  if (!segfeature_active(&cm->seg, segment_id, SEG_LVL_REF_FRAME)) {
    if (segfeature_active(&cm->seg, segment_id, SEG_LVL_GLOBALMV)) {
      assert(is_inter);
      return;
    }
    const int ctx = av1_get_intra_inter_context(xd);
    FRAME_CONTEXT *ec_ctx = xd->tile_ctx;
    aom_write_symbol(w, is_inter, ec_ctx->intra_inter_cdf[ctx], 2);
  }
}

static void write_motion_mode(const AV1_COMMON *cm, MACROBLOCKD *xd,
                              const MB_MODE_INFO *mbmi, aom_writer *w) {
  MOTION_MODE last_motion_mode_allowed =
      cm->switchable_motion_mode
          ? motion_mode_allowed(cm->global_motion, xd, mbmi,
                                cm->allow_warped_motion)
          : SIMPLE_TRANSLATION;
  assert(mbmi->motion_mode <= last_motion_mode_allowed);
  switch (last_motion_mode_allowed) {
    case SIMPLE_TRANSLATION: break;
    case OBMC_CAUSAL:
      aom_write_symbol(w, mbmi->motion_mode == OBMC_CAUSAL,
                       xd->tile_ctx->obmc_cdf[mbmi->sb_type], 2);
      break;
    default:
      aom_write_symbol(w, mbmi->motion_mode,
                       xd->tile_ctx->motion_mode_cdf[mbmi->sb_type],
                       MOTION_MODES);
  }
}

static void write_delta_qindex(const MACROBLOCKD *xd, int delta_qindex,
                               aom_writer *w) {
  int sign = delta_qindex < 0;
  int abs = sign ? -delta_qindex : delta_qindex;
  int rem_bits, thr;
  int smallval = abs < DELTA_Q_SMALL ? 1 : 0;
  FRAME_CONTEXT *ec_ctx = xd->tile_ctx;

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

static void write_delta_lflevel(const AV1_COMMON *cm, const MACROBLOCKD *xd,
                                int lf_id, int delta_lflevel, aom_writer *w) {
  int sign = delta_lflevel < 0;
  int abs = sign ? -delta_lflevel : delta_lflevel;
  int rem_bits, thr;
  int smallval = abs < DELTA_LF_SMALL ? 1 : 0;
  FRAME_CONTEXT *ec_ctx = xd->tile_ctx;

  if (cm->delta_lf_multi) {
    assert(lf_id >= 0 && lf_id < (av1_num_planes(cm) > 1 ? FRAME_LF_COUNT
                                                         : FRAME_LF_COUNT - 2));
    aom_write_symbol(w, AOMMIN(abs, DELTA_LF_SMALL),
                     ec_ctx->delta_lf_multi_cdf[lf_id], DELTA_LF_PROBS + 1);
  } else {
    aom_write_symbol(w, AOMMIN(abs, DELTA_LF_SMALL), ec_ctx->delta_lf_cdf,
                     DELTA_LF_PROBS + 1);
  }

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

static void pack_txb_tokens(aom_writer *w, AV1_COMMON *cm, MACROBLOCK *const x,
                            const TOKENEXTRA **tp,
                            const TOKENEXTRA *const tok_end, MACROBLOCKD *xd,
                            MB_MODE_INFO *mbmi, int plane,
                            BLOCK_SIZE plane_bsize, aom_bit_depth_t bit_depth,
                            int block, int blk_row, int blk_col,
                            TX_SIZE tx_size, TOKEN_STATS *token_stats) {
  const int max_blocks_high = max_block_high(xd, plane_bsize, plane);
  const int max_blocks_wide = max_block_wide(xd, plane_bsize, plane);

  if (blk_row >= max_blocks_high || blk_col >= max_blocks_wide) return;

  const struct macroblockd_plane *const pd = &xd->plane[plane];
  const TX_SIZE plane_tx_size =
      plane ? av1_get_max_uv_txsize(mbmi->sb_type, pd->subsampling_x,
                                    pd->subsampling_y)
            : mbmi->inter_tx_size[av1_get_txb_size_index(plane_bsize, blk_row,
                                                         blk_col)];

  if (tx_size == plane_tx_size || plane) {
    tran_low_t *tcoeff = BLOCK_OFFSET(x->mbmi_ext->tcoeff[plane], block);
    const uint16_t eob = x->mbmi_ext->eobs[plane][block];
    TXB_CTX txb_ctx = { x->mbmi_ext->txb_skip_ctx[plane][block],
                        x->mbmi_ext->dc_sign_ctx[plane][block] };
    av1_write_coeffs_txb(cm, xd, w, blk_row, blk_col, plane, tx_size, tcoeff,
                         eob, &txb_ctx);
#if CONFIG_RD_DEBUG
    TOKEN_STATS tmp_token_stats;
    init_token_stats(&tmp_token_stats);
    token_stats->txb_coeff_cost_map[blk_row][blk_col] = tmp_token_stats.cost;
    token_stats->cost += tmp_token_stats.cost;
#endif
  } else {
    const TX_SIZE sub_txs = sub_tx_size_map[tx_size];
    const int bsw = tx_size_wide_unit[sub_txs];
    const int bsh = tx_size_high_unit[sub_txs];
    const int step = bsh * bsw;

    assert(bsw > 0 && bsh > 0);

    for (int r = 0; r < tx_size_high_unit[tx_size]; r += bsh) {
      for (int c = 0; c < tx_size_wide_unit[tx_size]; c += bsw) {
        const int offsetr = blk_row + r;
        const int offsetc = blk_col + c;
        if (offsetr >= max_blocks_high || offsetc >= max_blocks_wide) continue;
        pack_txb_tokens(w, cm, x, tp, tok_end, xd, mbmi, plane, plane_bsize,
                        bit_depth, block, offsetr, offsetc, sub_txs,
                        token_stats);
        block += step;
      }
    }
  }
}

static INLINE void set_spatial_segment_id(const AV1_COMMON *const cm,
                                          uint8_t *segment_ids,
                                          BLOCK_SIZE bsize, int mi_row,
                                          int mi_col, int segment_id) {
  const int mi_offset = mi_row * cm->mi_cols + mi_col;
  const int bw = mi_size_wide[bsize];
  const int bh = mi_size_high[bsize];
  const int xmis = AOMMIN(cm->mi_cols - mi_col, bw);
  const int ymis = AOMMIN(cm->mi_rows - mi_row, bh);
  int x, y;

  for (y = 0; y < ymis; ++y)
    for (x = 0; x < xmis; ++x)
      segment_ids[mi_offset + y * cm->mi_cols + x] = segment_id;
}

int av1_neg_interleave(int x, int ref, int max) {
  assert(x < max);
  const int diff = x - ref;
  if (!ref) return x;
  if (ref >= (max - 1)) return -x + max - 1;
  if (2 * ref < max) {
    if (abs(diff) <= ref) {
      if (diff > 0)
        return (diff << 1) - 1;
      else
        return ((-diff) << 1);
    }
    return x;
  } else {
    if (abs(diff) < (max - ref)) {
      if (diff > 0)
        return (diff << 1) - 1;
      else
        return ((-diff) << 1);
    }
    return (max - x) - 1;
  }
}

static void write_segment_id(AV1_COMP *cpi, const MB_MODE_INFO *const mbmi,
                             aom_writer *w, const struct segmentation *seg,
                             struct segmentation_probs *segp, int mi_row,
                             int mi_col, int skip) {
  if (!seg->enabled || !seg->update_map) return;

  AV1_COMMON *const cm = &cpi->common;
  MACROBLOCKD *const xd = &cpi->td.mb.e_mbd;
  int cdf_num;
  const int pred = av1_get_spatial_seg_pred(cm, xd, mi_row, mi_col, &cdf_num);

  if (skip) {
    // Still need to transmit tx size for intra blocks even if skip is
    // true. Changing segment_id may make the tx size become invalid, e.g
    // changing from lossless to lossy.
    assert(is_inter_block(mbmi) || !cpi->has_lossless_segment);

    set_spatial_segment_id(cm, cm->current_frame_seg_map, mbmi->sb_type, mi_row,
                           mi_col, pred);
    set_spatial_segment_id(cm, cpi->segmentation_map, mbmi->sb_type, mi_row,
                           mi_col, pred);
    /* mbmi is read only but we need to update segment_id */
    ((MB_MODE_INFO *)mbmi)->segment_id = pred;
    return;
  }

  const int coded_id =
      av1_neg_interleave(mbmi->segment_id, pred, seg->last_active_segid + 1);
  aom_cdf_prob *pred_cdf = segp->spatial_pred_seg_cdf[cdf_num];
  aom_write_symbol(w, coded_id, pred_cdf, MAX_SEGMENTS);
  set_spatial_segment_id(cm, cm->current_frame_seg_map, mbmi->sb_type, mi_row,
                         mi_col, mbmi->segment_id);
}

#define WRITE_REF_BIT(bname, pname) \
  aom_write_symbol(w, bname, av1_get_pred_cdf_##pname(xd), 2)

// This function encodes the reference frame
static void write_ref_frames(const AV1_COMMON *cm, const MACROBLOCKD *xd,
                             aom_writer *w) {
  const MB_MODE_INFO *const mbmi = xd->mi[0];
  const int is_compound = has_second_ref(mbmi);
  const int segment_id = mbmi->segment_id;

  // If segment level coding of this signal is disabled...
  // or the segment allows multiple reference frame options
  if (segfeature_active(&cm->seg, segment_id, SEG_LVL_REF_FRAME)) {
    assert(!is_compound);
    assert(mbmi->ref_frame[0] ==
           get_segdata(&cm->seg, segment_id, SEG_LVL_REF_FRAME));
  } else if (segfeature_active(&cm->seg, segment_id, SEG_LVL_SKIP) ||
             segfeature_active(&cm->seg, segment_id, SEG_LVL_GLOBALMV)) {
    assert(!is_compound);
    assert(mbmi->ref_frame[0] == LAST_FRAME);
  } else {
    // does the feature use compound prediction or not
    // (if not specified at the frame/segment level)
    if (cm->reference_mode == REFERENCE_MODE_SELECT) {
      if (is_comp_ref_allowed(mbmi->sb_type))
        aom_write_symbol(w, is_compound, av1_get_reference_mode_cdf(xd), 2);
    } else {
      assert((!is_compound) == (cm->reference_mode == SINGLE_REFERENCE));
    }

    if (is_compound) {
      const COMP_REFERENCE_TYPE comp_ref_type = has_uni_comp_refs(mbmi)
                                                    ? UNIDIR_COMP_REFERENCE
                                                    : BIDIR_COMP_REFERENCE;
      aom_write_symbol(w, comp_ref_type, av1_get_comp_reference_type_cdf(xd),
                       2);

      if (comp_ref_type == UNIDIR_COMP_REFERENCE) {
        const int bit = mbmi->ref_frame[0] == BWDREF_FRAME;
        WRITE_REF_BIT(bit, uni_comp_ref_p);

        if (!bit) {
          assert(mbmi->ref_frame[0] == LAST_FRAME);
          const int bit1 = mbmi->ref_frame[1] == LAST3_FRAME ||
                           mbmi->ref_frame[1] == GOLDEN_FRAME;
          WRITE_REF_BIT(bit1, uni_comp_ref_p1);
          if (bit1) {
            const int bit2 = mbmi->ref_frame[1] == GOLDEN_FRAME;
            WRITE_REF_BIT(bit2, uni_comp_ref_p2);
          }
        } else {
          assert(mbmi->ref_frame[1] == ALTREF_FRAME);
        }

        return;
      }

      assert(comp_ref_type == BIDIR_COMP_REFERENCE);

      const int bit = (mbmi->ref_frame[0] == GOLDEN_FRAME ||
                       mbmi->ref_frame[0] == LAST3_FRAME);
      WRITE_REF_BIT(bit, comp_ref_p);

      if (!bit) {
        const int bit1 = mbmi->ref_frame[0] == LAST2_FRAME;
        WRITE_REF_BIT(bit1, comp_ref_p1);
      } else {
        const int bit2 = mbmi->ref_frame[0] == GOLDEN_FRAME;
        WRITE_REF_BIT(bit2, comp_ref_p2);
      }

      const int bit_bwd = mbmi->ref_frame[1] == ALTREF_FRAME;
      WRITE_REF_BIT(bit_bwd, comp_bwdref_p);

      if (!bit_bwd) {
        WRITE_REF_BIT(mbmi->ref_frame[1] == ALTREF2_FRAME, comp_bwdref_p1);
      }

    } else {
      const int bit0 = (mbmi->ref_frame[0] <= ALTREF_FRAME &&
                        mbmi->ref_frame[0] >= BWDREF_FRAME);
      WRITE_REF_BIT(bit0, single_ref_p1);

      if (bit0) {
        const int bit1 = mbmi->ref_frame[0] == ALTREF_FRAME;
        WRITE_REF_BIT(bit1, single_ref_p2);

        if (!bit1) {
          WRITE_REF_BIT(mbmi->ref_frame[0] == ALTREF2_FRAME, single_ref_p6);
        }
      } else {
        const int bit2 = (mbmi->ref_frame[0] == LAST3_FRAME ||
                          mbmi->ref_frame[0] == GOLDEN_FRAME);
        WRITE_REF_BIT(bit2, single_ref_p3);

        if (!bit2) {
          const int bit3 = mbmi->ref_frame[0] != LAST_FRAME;
          WRITE_REF_BIT(bit3, single_ref_p4);
        } else {
          const int bit4 = mbmi->ref_frame[0] != LAST3_FRAME;
          WRITE_REF_BIT(bit4, single_ref_p5);
        }
      }
    }
  }
}

static void write_filter_intra_mode_info(const AV1_COMMON *cm,
                                         const MACROBLOCKD *xd,
                                         const MB_MODE_INFO *const mbmi,
                                         aom_writer *w) {
  if (av1_filter_intra_allowed(cm, mbmi)) {
    aom_write_symbol(w, mbmi->filter_intra_mode_info.use_filter_intra,
                     xd->tile_ctx->filter_intra_cdfs[mbmi->sb_type], 2);
    if (mbmi->filter_intra_mode_info.use_filter_intra) {
      const FILTER_INTRA_MODE mode =
          mbmi->filter_intra_mode_info.filter_intra_mode;
      aom_write_symbol(w, mode, xd->tile_ctx->filter_intra_mode_cdf,
                       FILTER_INTRA_MODES);
    }
  }
}

static void write_angle_delta(aom_writer *w, int angle_delta,
                              aom_cdf_prob *cdf) {
  aom_write_symbol(w, angle_delta + MAX_ANGLE_DELTA, cdf,
                   2 * MAX_ANGLE_DELTA + 1);
}

static void write_mb_interp_filter(AV1_COMP *cpi, const MACROBLOCKD *xd,
                                   aom_writer *w) {
  AV1_COMMON *const cm = &cpi->common;
  const MB_MODE_INFO *const mbmi = xd->mi[0];
  FRAME_CONTEXT *ec_ctx = xd->tile_ctx;

  if (!av1_is_interp_needed(xd)) {
    assert(mbmi->interp_filters ==
           av1_broadcast_interp_filter(
               av1_unswitchable_filter(cm->interp_filter)));
    return;
  }
  if (cm->interp_filter == SWITCHABLE) {
    int dir;
    for (dir = 0; dir < 2; ++dir) {
      const int ctx = av1_get_pred_context_switchable_interp(xd, dir);
      InterpFilter filter =
          av1_extract_interp_filter(mbmi->interp_filters, dir);
      aom_write_symbol(w, filter, ec_ctx->switchable_interp_cdf[ctx],
                       SWITCHABLE_FILTERS);
      ++cpi->interp_filter_selected[0][filter];
      if (cm->seq_params.enable_dual_filter == 0) return;
    }
  }
}

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

static void write_palette_mode_info(const AV1_COMMON *cm, const MACROBLOCKD *xd,
                                    const MB_MODE_INFO *const mbmi, int mi_row,
                                    int mi_col, aom_writer *w) {
  const int num_planes = av1_num_planes(cm);
  const BLOCK_SIZE bsize = mbmi->sb_type;
  assert(av1_allow_palette(cm->allow_screen_content_tools, bsize));
  const PALETTE_MODE_INFO *const pmi = &mbmi->palette_mode_info;
  const int bsize_ctx = av1_get_palette_bsize_ctx(bsize);

  if (mbmi->mode == DC_PRED) {
    const int n = pmi->palette_size[0];
    const int palette_y_mode_ctx = av1_get_palette_mode_ctx(xd);
    aom_write_symbol(
        w, n > 0,
        xd->tile_ctx->palette_y_mode_cdf[bsize_ctx][palette_y_mode_ctx], 2);
    if (n > 0) {
      aom_write_symbol(w, n - PALETTE_MIN_SIZE,
                       xd->tile_ctx->palette_y_size_cdf[bsize_ctx],
                       PALETTE_SIZES);
      write_palette_colors_y(xd, pmi, cm->bit_depth, w);
    }
  }

  const int uv_dc_pred =
      num_planes > 1 && mbmi->uv_mode == UV_DC_PRED &&
      is_chroma_reference(mi_row, mi_col, bsize, xd->plane[1].subsampling_x,
                          xd->plane[1].subsampling_y);
  if (uv_dc_pred) {
    const int n = pmi->palette_size[1];
    const int palette_uv_mode_ctx = (pmi->palette_size[0] > 0);
    aom_write_symbol(w, n > 0,
                     xd->tile_ctx->palette_uv_mode_cdf[palette_uv_mode_ctx], 2);
    if (n > 0) {
      aom_write_symbol(w, n - PALETTE_MIN_SIZE,
                       xd->tile_ctx->palette_uv_size_cdf[bsize_ctx],
                       PALETTE_SIZES);
      write_palette_colors_uv(xd, pmi, cm->bit_depth, w);
    }
  }
}

void av1_write_tx_type(const AV1_COMMON *const cm, const MACROBLOCKD *xd,
                       int blk_row, int blk_col, int plane, TX_SIZE tx_size,
                       aom_writer *w) {
  MB_MODE_INFO *mbmi = xd->mi[0];
  const int is_inter = is_inter_block(mbmi);
  FRAME_CONTEXT *ec_ctx = xd->tile_ctx;

  // Only y plane's tx_type is transmitted
  if (plane > 0) return;
  PLANE_TYPE plane_type = get_plane_type(plane);
  TX_TYPE tx_type = av1_get_tx_type(plane_type, xd, blk_row, blk_col, tx_size,
                                    cm->reduced_tx_set_used);

  const TX_SIZE square_tx_size = txsize_sqr_map[tx_size];
  if (get_ext_tx_types(tx_size, is_inter, cm->reduced_tx_set_used) > 1 &&
      ((!cm->seg.enabled && cm->base_qindex > 0) ||
       (cm->seg.enabled && xd->qindex[mbmi->segment_id] > 0)) &&
      !mbmi->skip &&
      !segfeature_active(&cm->seg, mbmi->segment_id, SEG_LVL_SKIP)) {
    const TxSetType tx_set_type =
        av1_get_ext_tx_set_type(tx_size, is_inter, cm->reduced_tx_set_used);
    const int eset = get_ext_tx_set(tx_size, is_inter, cm->reduced_tx_set_used);
    // eset == 0 should correspond to a set with only DCT_DCT and there
    // is no need to send the tx_type
    assert(eset > 0);
    assert(av1_ext_tx_used[tx_set_type][tx_type]);
    if (is_inter) {
      aom_write_symbol(w, av1_ext_tx_ind[tx_set_type][tx_type],
                       ec_ctx->inter_ext_tx_cdf[eset][square_tx_size],
                       av1_num_ext_tx_set[tx_set_type]);
    } else {
      PREDICTION_MODE intra_dir;
      if (mbmi->filter_intra_mode_info.use_filter_intra)
        intra_dir =
            fimode_to_intradir[mbmi->filter_intra_mode_info.filter_intra_mode];
      else
        intra_dir = mbmi->mode;
      aom_write_symbol(
          w, av1_ext_tx_ind[tx_set_type][tx_type],
          ec_ctx->intra_ext_tx_cdf[eset][square_tx_size][intra_dir],
          av1_num_ext_tx_set[tx_set_type]);
    }
  }
}

static void write_intra_mode(FRAME_CONTEXT *frame_ctx, BLOCK_SIZE bsize,
                             PREDICTION_MODE mode, aom_writer *w) {
  aom_write_symbol(w, mode, frame_ctx->y_mode_cdf[size_group_lookup[bsize]],
                   INTRA_MODES);
}

static void write_intra_uv_mode(FRAME_CONTEXT *frame_ctx,
                                UV_PREDICTION_MODE uv_mode,
                                PREDICTION_MODE y_mode,
                                CFL_ALLOWED_TYPE cfl_allowed, aom_writer *w) {
  aom_write_symbol(w, uv_mode, frame_ctx->uv_mode_cdf[cfl_allowed][y_mode],
                   UV_INTRA_MODES - !cfl_allowed);
}

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

static void write_cdef(AV1_COMMON *cm, MACROBLOCKD *const xd, aom_writer *w,
                       int skip, int mi_col, int mi_row) {
  if (cm->coded_lossless || cm->allow_intrabc) {
    // Initialize to indicate no CDEF for safety.
    cm->cdef_bits = 0;
    cm->cdef_strengths[0] = 0;
    cm->nb_cdef_strengths = 1;
    cm->cdef_uv_strengths[0] = 0;
    return;
  }

  const int m = ~((1 << (6 - MI_SIZE_LOG2)) - 1);
  const MB_MODE_INFO *mbmi =
      cm->mi_grid_visible[(mi_row & m) * cm->mi_stride + (mi_col & m)];
  // Initialise when at top left part of the superblock
  if (!(mi_row & (cm->seq_params.mib_size - 1)) &&
      !(mi_col & (cm->seq_params.mib_size - 1))) {  // Top left?
    xd->cdef_preset[0] = xd->cdef_preset[1] = xd->cdef_preset[2] =
        xd->cdef_preset[3] = -1;
  }

  // Emit CDEF param at first non-skip coding block
  const int mask = 1 << (6 - MI_SIZE_LOG2);
  const int index = cm->seq_params.sb_size == BLOCK_128X128
                        ? !!(mi_col & mask) + 2 * !!(mi_row & mask)
                        : 0;
  if (xd->cdef_preset[index] == -1 && !skip) {
    aom_write_literal(w, mbmi->cdef_strength, cm->cdef_bits);
    xd->cdef_preset[index] = mbmi->cdef_strength;
  }
}

static void write_inter_segment_id(AV1_COMP *cpi, aom_writer *w,
                                   const struct segmentation *const seg,
                                   struct segmentation_probs *const segp,
                                   int mi_row, int mi_col, int skip,
                                   int preskip) {
  MACROBLOCKD *const xd = &cpi->td.mb.e_mbd;
  const MB_MODE_INFO *const mbmi = xd->mi[0];
  AV1_COMMON *const cm = &cpi->common;

  if (seg->update_map) {
    if (preskip) {
      if (!seg->segid_preskip) return;
    } else {
      if (seg->segid_preskip) return;
      if (skip) {
        write_segment_id(cpi, mbmi, w, seg, segp, mi_row, mi_col, 1);
        if (seg->temporal_update) ((MB_MODE_INFO *)mbmi)->seg_id_predicted = 0;
        return;
      }
    }
    if (seg->temporal_update) {
      const int pred_flag = mbmi->seg_id_predicted;
      aom_cdf_prob *pred_cdf = av1_get_pred_cdf_seg_id(segp, xd);
      aom_write_symbol(w, pred_flag, pred_cdf, 2);
      if (!pred_flag) {
        write_segment_id(cpi, mbmi, w, seg, segp, mi_row, mi_col, 0);
      }
      if (pred_flag) {
        set_spatial_segment_id(cm, cm->current_frame_seg_map, mbmi->sb_type,
                               mi_row, mi_col, mbmi->segment_id);
      }
    } else {
      write_segment_id(cpi, mbmi, w, seg, segp, mi_row, mi_col, 0);
    }
  }
}

static void pack_inter_mode_mvs(AV1_COMP *cpi, const int mi_row,
                                const int mi_col, aom_writer *w) {
  AV1_COMMON *const cm = &cpi->common;
  MACROBLOCK *const x = &cpi->td.mb;
  MACROBLOCKD *const xd = &x->e_mbd;
  FRAME_CONTEXT *ec_ctx = xd->tile_ctx;
  const struct segmentation *const seg = &cm->seg;
  struct segmentation_probs *const segp = &ec_ctx->seg;
  const MB_MODE_INFO *const mbmi = xd->mi[0];
  const MB_MODE_INFO_EXT *const mbmi_ext = x->mbmi_ext;
  const PREDICTION_MODE mode = mbmi->mode;
  const int segment_id = mbmi->segment_id;
  const BLOCK_SIZE bsize = mbmi->sb_type;
  const int allow_hp = cm->allow_high_precision_mv;
  const int is_inter = is_inter_block(mbmi);
  const int is_compound = has_second_ref(mbmi);
  int skip, ref;
  (void)mi_row;
  (void)mi_col;

  write_inter_segment_id(cpi, w, seg, segp, mi_row, mi_col, 0, 1);

  write_skip_mode(cm, xd, segment_id, mbmi, w);

  assert(IMPLIES(mbmi->skip_mode, mbmi->skip));
  skip = mbmi->skip_mode ? 1 : write_skip(cm, xd, segment_id, mbmi, w);

  write_inter_segment_id(cpi, w, seg, segp, mi_row, mi_col, skip, 0);

  write_cdef(cm, xd, w, skip, mi_col, mi_row);

  if (cm->delta_q_present_flag) {
    int super_block_upper_left =
        ((mi_row & (cm->seq_params.mib_size - 1)) == 0) &&
        ((mi_col & (cm->seq_params.mib_size - 1)) == 0);
    if ((bsize != cm->seq_params.sb_size || skip == 0) &&
        super_block_upper_left) {
      assert(mbmi->current_qindex > 0);
      int reduced_delta_qindex =
          (mbmi->current_qindex - xd->current_qindex) / cm->delta_q_res;
      write_delta_qindex(xd, reduced_delta_qindex, w);
      xd->current_qindex = mbmi->current_qindex;
      if (cm->delta_lf_present_flag) {
        if (cm->delta_lf_multi) {
          const int frame_lf_count =
              av1_num_planes(cm) > 1 ? FRAME_LF_COUNT : FRAME_LF_COUNT - 2;
          for (int lf_id = 0; lf_id < frame_lf_count; ++lf_id) {
            int reduced_delta_lflevel =
                (mbmi->delta_lf[lf_id] - xd->delta_lf[lf_id]) /
                cm->delta_lf_res;
            write_delta_lflevel(cm, xd, lf_id, reduced_delta_lflevel, w);
            xd->delta_lf[lf_id] = mbmi->delta_lf[lf_id];
          }
        } else {
          int reduced_delta_lflevel =
              (mbmi->delta_lf_from_base - xd->delta_lf_from_base) /
              cm->delta_lf_res;
          write_delta_lflevel(cm, xd, -1, reduced_delta_lflevel, w);
          xd->delta_lf_from_base = mbmi->delta_lf_from_base;
        }
      }
    }
  }

  if (!mbmi->skip_mode) write_is_inter(cm, xd, mbmi->segment_id, w, is_inter);

  if (mbmi->skip_mode) return;

  if (!is_inter) {
    write_intra_mode(ec_ctx, bsize, mode, w);
    const int use_angle_delta = av1_use_angle_delta(bsize);

    if (use_angle_delta && av1_is_directional_mode(mode)) {
      write_angle_delta(w, mbmi->angle_delta[PLANE_TYPE_Y],
                        ec_ctx->angle_delta_cdf[mode - V_PRED]);
    }

    if (!cm->seq_params.monochrome &&
        is_chroma_reference(mi_row, mi_col, bsize, xd->plane[1].subsampling_x,
                            xd->plane[1].subsampling_y)) {
      const UV_PREDICTION_MODE uv_mode = mbmi->uv_mode;
      write_intra_uv_mode(ec_ctx, uv_mode, mode, is_cfl_allowed(xd), w);
      if (uv_mode == UV_CFL_PRED)
        write_cfl_alphas(ec_ctx, mbmi->cfl_alpha_idx, mbmi->cfl_alpha_signs, w);
      if (use_angle_delta && av1_is_directional_mode(get_uv_mode(uv_mode))) {
        write_angle_delta(w, mbmi->angle_delta[PLANE_TYPE_UV],
                          ec_ctx->angle_delta_cdf[uv_mode - V_PRED]);
      }
    }

    if (av1_allow_palette(cm->allow_screen_content_tools, bsize))
      write_palette_mode_info(cm, xd, mbmi, mi_row, mi_col, w);

    write_filter_intra_mode_info(cm, xd, mbmi, w);
  } else {
    int16_t mode_ctx;

    av1_collect_neighbors_ref_counts(xd);

    write_ref_frames(cm, xd, w);

    mode_ctx =
        av1_mode_context_analyzer(mbmi_ext->mode_context, mbmi->ref_frame);

    // If segment skip is not enabled code the mode.
    if (!segfeature_active(seg, segment_id, SEG_LVL_SKIP)) {
      if (is_inter_compound_mode(mode))
        write_inter_compound_mode(xd, w, mode, mode_ctx);
      else if (is_inter_singleref_mode(mode))
        write_inter_mode(w, mode, ec_ctx, mode_ctx);

      if (mode == NEWMV || mode == NEW_NEWMV || have_nearmv_in_inter_mode(mode))
        write_drl_idx(ec_ctx, mbmi, mbmi_ext, w);
      else
        assert(mbmi->ref_mv_idx == 0);
    }

    if (mode == NEWMV || mode == NEW_NEWMV) {
      for (ref = 0; ref < 1 + is_compound; ++ref) {
        nmv_context *nmvc = &ec_ctx->nmvc;
        const int_mv ref_mv = av1_get_ref_mv(x, ref);
        av1_encode_mv(cpi, w, &mbmi->mv[ref].as_mv, &ref_mv.as_mv, nmvc,
                      allow_hp);
      }
    } else if (mode == NEAREST_NEWMV || mode == NEAR_NEWMV) {
      nmv_context *nmvc = &ec_ctx->nmvc;
      const int_mv ref_mv = av1_get_ref_mv(x, 1);
      av1_encode_mv(cpi, w, &mbmi->mv[1].as_mv, &ref_mv.as_mv, nmvc, allow_hp);
    } else if (mode == NEW_NEARESTMV || mode == NEW_NEARMV) {
      nmv_context *nmvc = &ec_ctx->nmvc;
      const int_mv ref_mv = av1_get_ref_mv(x, 0);
      av1_encode_mv(cpi, w, &mbmi->mv[0].as_mv, &ref_mv.as_mv, nmvc, allow_hp);
    }

    if (cpi->common.reference_mode != COMPOUND_REFERENCE &&
        cpi->common.seq_params.enable_interintra_compound &&
        is_interintra_allowed(mbmi)) {
      const int interintra = mbmi->ref_frame[1] == INTRA_FRAME;
      const int bsize_group = size_group_lookup[bsize];
      aom_write_symbol(w, interintra, ec_ctx->interintra_cdf[bsize_group], 2);
      if (interintra) {
        aom_write_symbol(w, mbmi->interintra_mode,
                         ec_ctx->interintra_mode_cdf[bsize_group],
                         INTERINTRA_MODES);
        if (is_interintra_wedge_used(bsize)) {
          aom_write_symbol(w, mbmi->use_wedge_interintra,
                           ec_ctx->wedge_interintra_cdf[bsize], 2);
          if (mbmi->use_wedge_interintra) {
            aom_write_symbol(w, mbmi->interintra_wedge_index,
                             ec_ctx->wedge_idx_cdf[bsize], 16);
            assert(mbmi->interintra_wedge_sign == 0);
          }
        }
      }
    }

    if (mbmi->ref_frame[1] != INTRA_FRAME) write_motion_mode(cm, xd, mbmi, w);

    // First write idx to indicate current compound inter prediction mode group
    // Group A (0): jnt_comp, compound_average
    // Group B (1): interintra, compound_diffwtd, wedge
    if (has_second_ref(mbmi)) {
      const int masked_compound_used = is_any_masked_compound_used(bsize) &&
                                       cm->seq_params.enable_masked_compound;

      if (masked_compound_used) {
        const int ctx_comp_group_idx = get_comp_group_idx_context(xd);
        aom_write_symbol(w, mbmi->comp_group_idx,
                         ec_ctx->comp_group_idx_cdf[ctx_comp_group_idx], 2);
      } else {
        assert(mbmi->comp_group_idx == 0);
      }

      if (mbmi->comp_group_idx == 0) {
        if (mbmi->compound_idx)
          assert(mbmi->interinter_comp.type == COMPOUND_AVERAGE);

        if (cm->seq_params.enable_jnt_comp) {
          const int comp_index_ctx = get_comp_index_context(cm, xd);
          aom_write_symbol(w, mbmi->compound_idx,
                           ec_ctx->compound_index_cdf[comp_index_ctx], 2);
        } else {
          assert(mbmi->compound_idx == 1);
        }
      } else {
        assert(cpi->common.reference_mode != SINGLE_REFERENCE &&
               is_inter_compound_mode(mbmi->mode) &&
               mbmi->motion_mode == SIMPLE_TRANSLATION);
        assert(masked_compound_used);
        // compound_diffwtd, wedge
        assert(mbmi->interinter_comp.type == COMPOUND_WEDGE ||
               mbmi->interinter_comp.type == COMPOUND_DIFFWTD);

        if (is_interinter_compound_used(COMPOUND_WEDGE, bsize))
          aom_write_symbol(w, mbmi->interinter_comp.type - 1,
                           ec_ctx->compound_type_cdf[bsize],
                           COMPOUND_TYPES - 1);

        if (mbmi->interinter_comp.type == COMPOUND_WEDGE) {
          assert(is_interinter_compound_used(COMPOUND_WEDGE, bsize));
          aom_write_symbol(w, mbmi->interinter_comp.wedge_index,
                           ec_ctx->wedge_idx_cdf[bsize], 16);
          aom_write_bit(w, mbmi->interinter_comp.wedge_sign);
        } else {
          assert(mbmi->interinter_comp.type == COMPOUND_DIFFWTD);
          aom_write_literal(w, mbmi->interinter_comp.mask_type,
                            MAX_DIFFWTD_MASK_BITS);
        }
      }
    }

    write_mb_interp_filter(cpi, xd, w);
  }
}

static void write_intrabc_info(MACROBLOCKD *xd,
                               const MB_MODE_INFO_EXT *mbmi_ext,
                               aom_writer *w) {
  const MB_MODE_INFO *const mbmi = xd->mi[0];
  int use_intrabc = is_intrabc_block(mbmi);
  FRAME_CONTEXT *ec_ctx = xd->tile_ctx;
  aom_write_symbol(w, use_intrabc, ec_ctx->intrabc_cdf, 2);
  if (use_intrabc) {
    assert(mbmi->mode == DC_PRED);
    assert(mbmi->uv_mode == UV_DC_PRED);
    assert(mbmi->motion_mode == SIMPLE_TRANSLATION);
    int_mv dv_ref = mbmi_ext->ref_mv_stack[INTRA_FRAME][0].this_mv;
    av1_encode_dv(w, &mbmi->mv[0].as_mv, &dv_ref.as_mv, &ec_ctx->ndvc);
  }
}

static void write_mb_modes_kf(AV1_COMP *cpi, MACROBLOCKD *xd,
                              const MB_MODE_INFO_EXT *mbmi_ext,
                              const int mi_row, const int mi_col,
                              aom_writer *w) {
  AV1_COMMON *const cm = &cpi->common;
  FRAME_CONTEXT *ec_ctx = xd->tile_ctx;
  const struct segmentation *const seg = &cm->seg;
  struct segmentation_probs *const segp = &ec_ctx->seg;
  const MB_MODE_INFO *const above_mi = xd->above_mbmi;
  const MB_MODE_INFO *const left_mi = xd->left_mbmi;
  const MB_MODE_INFO *const mbmi = xd->mi[0];
  const BLOCK_SIZE bsize = mbmi->sb_type;
  const PREDICTION_MODE mode = mbmi->mode;

  if (seg->segid_preskip && seg->update_map)
    write_segment_id(cpi, mbmi, w, seg, segp, mi_row, mi_col, 0);

  const int skip = write_skip(cm, xd, mbmi->segment_id, mbmi, w);

  if (!seg->segid_preskip && seg->update_map)
    write_segment_id(cpi, mbmi, w, seg, segp, mi_row, mi_col, skip);

  write_cdef(cm, xd, w, skip, mi_col, mi_row);

  if (cm->delta_q_present_flag) {
    int super_block_upper_left =
        ((mi_row & (cm->seq_params.mib_size - 1)) == 0) &&
        ((mi_col & (cm->seq_params.mib_size - 1)) == 0);
    if ((bsize != cm->seq_params.sb_size || skip == 0) &&
        super_block_upper_left) {
      assert(mbmi->current_qindex > 0);
      int reduced_delta_qindex =
          (mbmi->current_qindex - xd->current_qindex) / cm->delta_q_res;
      write_delta_qindex(xd, reduced_delta_qindex, w);
      xd->current_qindex = mbmi->current_qindex;
      if (cm->delta_lf_present_flag) {
        if (cm->delta_lf_multi) {
          const int frame_lf_count =
              av1_num_planes(cm) > 1 ? FRAME_LF_COUNT : FRAME_LF_COUNT - 2;
          for (int lf_id = 0; lf_id < frame_lf_count; ++lf_id) {
            int reduced_delta_lflevel =
                (mbmi->delta_lf[lf_id] - xd->delta_lf[lf_id]) /
                cm->delta_lf_res;
            write_delta_lflevel(cm, xd, lf_id, reduced_delta_lflevel, w);
            xd->delta_lf[lf_id] = mbmi->delta_lf[lf_id];
          }
        } else {
          int reduced_delta_lflevel =
              (mbmi->delta_lf_from_base - xd->delta_lf_from_base) /
              cm->delta_lf_res;
          write_delta_lflevel(cm, xd, -1, reduced_delta_lflevel, w);
          xd->delta_lf_from_base = mbmi->delta_lf_from_base;
        }
      }
    }
  }

  if (av1_allow_intrabc(cm)) {
    write_intrabc_info(xd, mbmi_ext, w);
    if (is_intrabc_block(mbmi)) return;
  }

  write_intra_mode_kf(ec_ctx, mbmi, above_mi, left_mi, mode, w);

  const int use_angle_delta = av1_use_angle_delta(bsize);
  if (use_angle_delta && av1_is_directional_mode(mode)) {
    write_angle_delta(w, mbmi->angle_delta[PLANE_TYPE_Y],
                      ec_ctx->angle_delta_cdf[mode - V_PRED]);
  }

  if (!cm->seq_params.monochrome &&
      is_chroma_reference(mi_row, mi_col, bsize, xd->plane[1].subsampling_x,
                          xd->plane[1].subsampling_y)) {
    const UV_PREDICTION_MODE uv_mode = mbmi->uv_mode;
    write_intra_uv_mode(ec_ctx, uv_mode, mode, is_cfl_allowed(xd), w);
    if (uv_mode == UV_CFL_PRED)
      write_cfl_alphas(ec_ctx, mbmi->cfl_alpha_idx, mbmi->cfl_alpha_signs, w);
    if (use_angle_delta && av1_is_directional_mode(get_uv_mode(uv_mode))) {
      write_angle_delta(w, mbmi->angle_delta[PLANE_TYPE_UV],
                        ec_ctx->angle_delta_cdf[uv_mode - V_PRED]);
    }
  }

  if (av1_allow_palette(cm->allow_screen_content_tools, bsize))
    write_palette_mode_info(cm, xd, mbmi, mi_row, mi_col, w);

  write_filter_intra_mode_info(cm, xd, mbmi, w);
}

#if CONFIG_RD_DEBUG
static void dump_mode_info(MODE_INFO *mi) {
  printf("\nmi->mi_row == %d\n", mi->mi_row);
  printf("&& mi->mi_col == %d\n", mi->mi_col);
  printf("&& mi->sb_type == %d\n", mi->sb_type);
  printf("&& mi->tx_size == %d\n", mi->tx_size);
  printf("&& mi->mode == %d\n", mi->mode);
}
static int rd_token_stats_mismatch(RD_STATS *rd_stats, TOKEN_STATS *token_stats,
                                   int plane) {
  if (rd_stats->txb_coeff_cost[plane] != token_stats->cost) {
    int r, c;
    printf("\nplane %d rd_stats->txb_coeff_cost %d token_stats->cost %d\n",
           plane, rd_stats->txb_coeff_cost[plane], token_stats->cost);
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
    return 1;
  }
  return 0;
}
#endif

#if ENC_MISMATCH_DEBUG
static void enc_dump_logs(AV1_COMP *cpi, int mi_row, int mi_col) {
  AV1_COMMON *const cm = &cpi->common;
  MACROBLOCKD *const xd = &cpi->td.mb.e_mbd;
  xd->mi = cm->mi_grid_visible + (mi_row * cm->mi_stride + mi_col);
  const MB_MODE_INFO *const *mbmi = xd->mi[0];
  if (is_inter_block(mbmi)) {
#define FRAME_TO_CHECK 11
    if (cm->current_video_frame == FRAME_TO_CHECK && cm->show_frame == 1) {
      const BLOCK_SIZE bsize = mbmi->sb_type;

      int_mv mv[2];
      int is_comp_ref = has_second_ref(mbmi);
      int ref;

      for (ref = 0; ref < 1 + is_comp_ref; ++ref)
        mv[ref].as_mv = mbmi->mv[ref].as_mv;

      if (!is_comp_ref) {
        mv[1].as_int = 0;
      }

      MACROBLOCK *const x = &cpi->td.mb;
      const MB_MODE_INFO_EXT *const mbmi_ext = x->mbmi_ext;
      const int16_t mode_ctx =
          is_comp_ref ? mbmi_ext->compound_mode_context[mbmi->ref_frame[0]]
                      : av1_mode_context_analyzer(mbmi_ext->mode_context,
                                                  mbmi->ref_frame);

      const int16_t newmv_ctx = mode_ctx & NEWMV_CTX_MASK;
      int16_t zeromv_ctx = -1;
      int16_t refmv_ctx = -1;

      if (mbmi->mode != NEWMV) {
        zeromv_ctx = (mode_ctx >> GLOBALMV_OFFSET) & GLOBALMV_CTX_MASK;
        if (mbmi->mode != GLOBALMV)
          refmv_ctx = (mode_ctx >> REFMV_OFFSET) & REFMV_CTX_MASK;
      }

      printf(
          "=== ENCODER ===: "
          "Frame=%d, (mi_row,mi_col)=(%d,%d), skip_mode=%d, mode=%d, bsize=%d, "
          "show_frame=%d, mv[0]=(%d,%d), mv[1]=(%d,%d), ref[0]=%d, "
          "ref[1]=%d, motion_mode=%d, mode_ctx=%d, "
          "newmv_ctx=%d, zeromv_ctx=%d, refmv_ctx=%d, tx_size=%d\n",
          cm->current_video_frame, mi_row, mi_col, mbmi->skip_mode, mbmi->mode,
          bsize, cm->show_frame, mv[0].as_mv.row, mv[0].as_mv.col,
          mv[1].as_mv.row, mv[1].as_mv.col, mbmi->ref_frame[0],
          mbmi->ref_frame[1], mbmi->motion_mode, mode_ctx, newmv_ctx,
          zeromv_ctx, refmv_ctx, mbmi->tx_size);
    }
  }
}
#endif  // ENC_MISMATCH_DEBUG

static void write_mbmi_b(AV1_COMP *cpi, const TileInfo *const tile,
                         aom_writer *w, int mi_row, int mi_col) {
  AV1_COMMON *const cm = &cpi->common;
  MACROBLOCKD *const xd = &cpi->td.mb.e_mbd;
  int bh, bw;
  xd->mi = cm->mi_grid_visible + (mi_row * cm->mi_stride + mi_col);
  MB_MODE_INFO *m = xd->mi[0];

  assert(m->sb_type <= cm->seq_params.sb_size ||
         (m->sb_type >= BLOCK_SIZES && m->sb_type < BLOCK_SIZES_ALL));

  bh = mi_size_high[m->sb_type];
  bw = mi_size_wide[m->sb_type];

  cpi->td.mb.mbmi_ext = cpi->mbmi_ext_base + (mi_row * cm->mi_cols + mi_col);

  set_mi_row_col(xd, tile, mi_row, bh, mi_col, bw, cm->mi_rows, cm->mi_cols);

  xd->above_txfm_context = cm->above_txfm_context[tile->tile_row] + mi_col;
  xd->left_txfm_context =
      xd->left_txfm_context_buffer + (mi_row & MAX_MIB_MASK);

  if (frame_is_intra_only(cm)) {
    write_mb_modes_kf(cpi, xd, cpi->td.mb.mbmi_ext, mi_row, mi_col, w);
  } else {
    // has_subpel_mv_component needs the ref frame buffers set up to look
    // up if they are scaled. has_subpel_mv_component is in turn needed by
    // write_switchable_interp_filter, which is called by pack_inter_mode_mvs.
    set_ref_ptrs(cm, xd, m->ref_frame[0], m->ref_frame[1]);

#if ENC_MISMATCH_DEBUG
    enc_dump_logs(cpi, mi_row, mi_col);
#endif  // ENC_MISMATCH_DEBUG

    pack_inter_mode_mvs(cpi, mi_row, mi_col, w);
  }
}

static void write_inter_txb_coeff(AV1_COMMON *const cm, MACROBLOCK *const x,
                                  MB_MODE_INFO *const mbmi, aom_writer *w,
                                  const TOKENEXTRA **tok,
                                  const TOKENEXTRA *const tok_end,
                                  TOKEN_STATS *token_stats, const int row,
                                  const int col, int *block, const int plane) {
  MACROBLOCKD *const xd = &x->e_mbd;
  const struct macroblockd_plane *const pd = &xd->plane[plane];
  const BLOCK_SIZE bsize = mbmi->sb_type;
  const BLOCK_SIZE bsizec =
      scale_chroma_bsize(bsize, pd->subsampling_x, pd->subsampling_y);

  const BLOCK_SIZE plane_bsize =
      get_plane_block_size(bsizec, pd->subsampling_x, pd->subsampling_y);

  const TX_SIZE max_tx_size = get_vartx_max_txsize(xd, plane_bsize, plane);
  const int step =
      tx_size_wide_unit[max_tx_size] * tx_size_high_unit[max_tx_size];
  const int bkw = tx_size_wide_unit[max_tx_size];
  const int bkh = tx_size_high_unit[max_tx_size];

  const BLOCK_SIZE max_unit_bsize =
      get_plane_block_size(BLOCK_64X64, pd->subsampling_x, pd->subsampling_y);
  int mu_blocks_wide = block_size_wide[max_unit_bsize] >> tx_size_wide_log2[0];
  int mu_blocks_high = block_size_high[max_unit_bsize] >> tx_size_high_log2[0];

  int blk_row, blk_col;

  const int num_4x4_w = block_size_wide[plane_bsize] >> tx_size_wide_log2[0];
  const int num_4x4_h = block_size_high[plane_bsize] >> tx_size_high_log2[0];

  const int unit_height =
      AOMMIN(mu_blocks_high + (row >> pd->subsampling_y), num_4x4_h);
  const int unit_width =
      AOMMIN(mu_blocks_wide + (col >> pd->subsampling_x), num_4x4_w);
  for (blk_row = row >> pd->subsampling_y; blk_row < unit_height;
       blk_row += bkh) {
    for (blk_col = col >> pd->subsampling_x; blk_col < unit_width;
         blk_col += bkw) {
      pack_txb_tokens(w, cm, x, tok, tok_end, xd, mbmi, plane, plane_bsize,
                      cm->bit_depth, *block, blk_row, blk_col, max_tx_size,
                      token_stats);
      *block += step;
    }
  }
}

static void write_tokens_b(AV1_COMP *cpi, const TileInfo *const tile,
                           aom_writer *w, const TOKENEXTRA **tok,
                           const TOKENEXTRA *const tok_end, int mi_row,
                           int mi_col) {
  AV1_COMMON *const cm = &cpi->common;
  const int num_planes = av1_num_planes(cm);
  MACROBLOCKD *const xd = &cpi->td.mb.e_mbd;
  const int mi_offset = mi_row * cm->mi_stride + mi_col;
  MB_MODE_INFO *const mbmi = *(cm->mi_grid_visible + mi_offset);
  int plane;
  int bh, bw;
  MACROBLOCK *const x = &cpi->td.mb;
  (void)tok;
  (void)tok_end;
  xd->mi = cm->mi_grid_visible + mi_offset;

  assert(mbmi->sb_type <= cm->seq_params.sb_size ||
         (mbmi->sb_type >= BLOCK_SIZES && mbmi->sb_type < BLOCK_SIZES_ALL));

  bh = mi_size_high[mbmi->sb_type];
  bw = mi_size_wide[mbmi->sb_type];
  cpi->td.mb.mbmi_ext = cpi->mbmi_ext_base + (mi_row * cm->mi_cols + mi_col);

  set_mi_row_col(xd, tile, mi_row, bh, mi_col, bw, cm->mi_rows, cm->mi_cols);

  if (!mbmi->skip) {
    if (!is_inter_block(mbmi))
      av1_write_coeffs_mb(cm, x, mi_row, mi_col, w, mbmi->sb_type);

    if (is_inter_block(mbmi)) {
      int block[MAX_MB_PLANE] = { 0 };
      const BLOCK_SIZE plane_bsize = mbmi->sb_type;
      assert(plane_bsize == get_plane_block_size(mbmi->sb_type,
                                                 xd->plane[0].subsampling_x,
                                                 xd->plane[0].subsampling_y));
      const int num_4x4_w =
          block_size_wide[plane_bsize] >> tx_size_wide_log2[0];
      const int num_4x4_h =
          block_size_high[plane_bsize] >> tx_size_high_log2[0];
      int row, col;
      TOKEN_STATS token_stats;
      init_token_stats(&token_stats);

      const BLOCK_SIZE max_unit_bsize = BLOCK_64X64;
      assert(max_unit_bsize ==
             get_plane_block_size(BLOCK_64X64, xd->plane[0].subsampling_x,
                                  xd->plane[0].subsampling_y));
      int mu_blocks_wide =
          block_size_wide[max_unit_bsize] >> tx_size_wide_log2[0];
      int mu_blocks_high =
          block_size_high[max_unit_bsize] >> tx_size_high_log2[0];

      mu_blocks_wide = AOMMIN(num_4x4_w, mu_blocks_wide);
      mu_blocks_high = AOMMIN(num_4x4_h, mu_blocks_high);

      for (row = 0; row < num_4x4_h; row += mu_blocks_high) {
        for (col = 0; col < num_4x4_w; col += mu_blocks_wide) {
          for (plane = 0; plane < num_planes && is_inter_block(mbmi); ++plane) {
            const struct macroblockd_plane *const pd = &xd->plane[plane];
            if (!is_chroma_reference(mi_row, mi_col, mbmi->sb_type,
                                     pd->subsampling_x, pd->subsampling_y)) {
              continue;
            }
            write_inter_txb_coeff(cm, x, mbmi, w, tok, tok_end, &token_stats,
                                  row, col, &block[plane], plane);
          }
        }
#if CONFIG_RD_DEBUG
        if (mbmi->sb_type >= BLOCK_8X8 &&
            rd_token_stats_mismatch(&mbmi->rd_stats, &token_stats, plane)) {
          dump_mode_info(m);
          assert(0);
        }
#endif  // CONFIG_RD_DEBUG
      }
    }
  }
}

static void write_modes_b(AV1_COMP *cpi, const TileInfo *const tile,
                          aom_writer *w, const TOKENEXTRA **tok,
                          const TOKENEXTRA *const tok_end, int mi_row,
                          int mi_col) {
  write_mbmi_b(cpi, tile, w, mi_row, mi_col);

  AV1_COMMON *cm = &cpi->common;
  MACROBLOCKD *xd = &cpi->td.mb.e_mbd;
  MB_MODE_INFO *mbmi = xd->mi[0];
  for (int plane = 0; plane < AOMMIN(2, av1_num_planes(cm)); ++plane) {
    const uint8_t palette_size_plane =
        mbmi->palette_mode_info.palette_size[plane];
    assert(!mbmi->skip_mode || !palette_size_plane);
    if (palette_size_plane > 0) {
      assert(mbmi->use_intrabc == 0);
      assert(av1_allow_palette(cm->allow_screen_content_tools, mbmi->sb_type));
      int rows, cols;
      av1_get_block_dimensions(mbmi->sb_type, plane, xd, NULL, NULL, &rows,
                               &cols);
      assert(*tok < tok_end);
      pack_map_tokens(w, tok, palette_size_plane, rows * cols);
    }
  }

  BLOCK_SIZE bsize = mbmi->sb_type;
  int is_inter_tx = is_inter_block(mbmi) || is_intrabc_block(mbmi);
  int skip = mbmi->skip;
  int segment_id = mbmi->segment_id;
  if (cm->tx_mode == TX_MODE_SELECT && block_signals_txsize(bsize) &&
      !(is_inter_tx && skip) && !xd->lossless[segment_id]) {
    if (is_inter_tx) {  // This implies skip flag is 0.
      const TX_SIZE max_tx_size = get_vartx_max_txsize(xd, bsize, 0);
      const int txbh = tx_size_high_unit[max_tx_size];
      const int txbw = tx_size_wide_unit[max_tx_size];
      const int width = block_size_wide[bsize] >> tx_size_wide_log2[0];
      const int height = block_size_high[bsize] >> tx_size_high_log2[0];
      int idx, idy;
      for (idy = 0; idy < height; idy += txbh)
        for (idx = 0; idx < width; idx += txbw)
          write_tx_size_vartx(xd, mbmi, max_tx_size, 0, idy, idx, w);
    } else {
      write_selected_tx_size(xd, w);
      set_txfm_ctxs(mbmi->tx_size, xd->n8_w, xd->n8_h, 0, xd);
    }
  } else {
    set_txfm_ctxs(mbmi->tx_size, xd->n8_w, xd->n8_h,
                  skip && is_inter_block(mbmi), xd);
  }

  write_tokens_b(cpi, tile, w, tok, tok_end, mi_row, mi_col);
}

static void write_partition(const AV1_COMMON *const cm,
                            const MACROBLOCKD *const xd, int hbs, int mi_row,
                            int mi_col, PARTITION_TYPE p, BLOCK_SIZE bsize,
                            aom_writer *w) {
  const int is_partition_point = bsize >= BLOCK_8X8;

  if (!is_partition_point) return;

  const int has_rows = (mi_row + hbs) < cm->mi_rows;
  const int has_cols = (mi_col + hbs) < cm->mi_cols;
  const int ctx = partition_plane_context(xd, mi_row, mi_col, bsize);
  FRAME_CONTEXT *ec_ctx = xd->tile_ctx;

  if (!has_rows && !has_cols) {
    assert(p == PARTITION_SPLIT);
    return;
  }

  if (has_rows && has_cols) {
    aom_write_symbol(w, p, ec_ctx->partition_cdf[ctx],
                     partition_cdf_length(bsize));
  } else if (!has_rows && has_cols) {
    assert(p == PARTITION_SPLIT || p == PARTITION_HORZ);
    assert(bsize > BLOCK_8X8);
    aom_cdf_prob cdf[2];
    partition_gather_vert_alike(cdf, ec_ctx->partition_cdf[ctx], bsize);
    aom_write_cdf(w, p == PARTITION_SPLIT, cdf, 2);
  } else {
    assert(has_rows && !has_cols);
    assert(p == PARTITION_SPLIT || p == PARTITION_VERT);
    assert(bsize > BLOCK_8X8);
    aom_cdf_prob cdf[2];
    partition_gather_horz_alike(cdf, ec_ctx->partition_cdf[ctx], bsize);
    aom_write_cdf(w, p == PARTITION_SPLIT, cdf, 2);
  }
}

static void write_modes_sb(AV1_COMP *const cpi, const TileInfo *const tile,
                           aom_writer *const w, const TOKENEXTRA **tok,
                           const TOKENEXTRA *const tok_end, int mi_row,
                           int mi_col, BLOCK_SIZE bsize) {
  const AV1_COMMON *const cm = &cpi->common;
  MACROBLOCKD *const xd = &cpi->td.mb.e_mbd;
  const int hbs = mi_size_wide[bsize] / 2;
  const int quarter_step = mi_size_wide[bsize] / 4;
  int i;
  const PARTITION_TYPE partition = get_partition(cm, mi_row, mi_col, bsize);
  const BLOCK_SIZE subsize = get_partition_subsize(bsize, partition);

  if (mi_row >= cm->mi_rows || mi_col >= cm->mi_cols) return;

  const int num_planes = av1_num_planes(cm);
  for (int plane = 0; plane < num_planes; ++plane) {
    int rcol0, rcol1, rrow0, rrow1, tile_tl_idx;
    if (av1_loop_restoration_corners_in_sb(cm, plane, mi_row, mi_col, bsize,
                                           &rcol0, &rcol1, &rrow0, &rrow1,
                                           &tile_tl_idx)) {
      const int rstride = cm->rst_info[plane].horz_units_per_tile;
      for (int rrow = rrow0; rrow < rrow1; ++rrow) {
        for (int rcol = rcol0; rcol < rcol1; ++rcol) {
          const int runit_idx = tile_tl_idx + rcol + rrow * rstride;
          const RestorationUnitInfo *rui =
              &cm->rst_info[plane].unit_info[runit_idx];
          loop_restoration_write_sb_coeffs(cm, xd, rui, w, plane,
                                           cpi->td.counts);
        }
      }
    }
  }

  write_partition(cm, xd, hbs, mi_row, mi_col, partition, bsize, w);
  switch (partition) {
    case PARTITION_NONE:
      write_modes_b(cpi, tile, w, tok, tok_end, mi_row, mi_col);
      break;
    case PARTITION_HORZ:
      write_modes_b(cpi, tile, w, tok, tok_end, mi_row, mi_col);
      if (mi_row + hbs < cm->mi_rows)
        write_modes_b(cpi, tile, w, tok, tok_end, mi_row + hbs, mi_col);
      break;
    case PARTITION_VERT:
      write_modes_b(cpi, tile, w, tok, tok_end, mi_row, mi_col);
      if (mi_col + hbs < cm->mi_cols)
        write_modes_b(cpi, tile, w, tok, tok_end, mi_row, mi_col + hbs);
      break;
    case PARTITION_SPLIT:
      write_modes_sb(cpi, tile, w, tok, tok_end, mi_row, mi_col, subsize);
      write_modes_sb(cpi, tile, w, tok, tok_end, mi_row, mi_col + hbs, subsize);
      write_modes_sb(cpi, tile, w, tok, tok_end, mi_row + hbs, mi_col, subsize);
      write_modes_sb(cpi, tile, w, tok, tok_end, mi_row + hbs, mi_col + hbs,
                     subsize);
      break;
    case PARTITION_HORZ_A:
      write_modes_b(cpi, tile, w, tok, tok_end, mi_row, mi_col);
      write_modes_b(cpi, tile, w, tok, tok_end, mi_row, mi_col + hbs);
      write_modes_b(cpi, tile, w, tok, tok_end, mi_row + hbs, mi_col);
      break;
    case PARTITION_HORZ_B:
      write_modes_b(cpi, tile, w, tok, tok_end, mi_row, mi_col);
      write_modes_b(cpi, tile, w, tok, tok_end, mi_row + hbs, mi_col);
      write_modes_b(cpi, tile, w, tok, tok_end, mi_row + hbs, mi_col + hbs);
      break;
    case PARTITION_VERT_A:
      write_modes_b(cpi, tile, w, tok, tok_end, mi_row, mi_col);
      write_modes_b(cpi, tile, w, tok, tok_end, mi_row + hbs, mi_col);
      write_modes_b(cpi, tile, w, tok, tok_end, mi_row, mi_col + hbs);
      break;
    case PARTITION_VERT_B:
      write_modes_b(cpi, tile, w, tok, tok_end, mi_row, mi_col);
      write_modes_b(cpi, tile, w, tok, tok_end, mi_row, mi_col + hbs);
      write_modes_b(cpi, tile, w, tok, tok_end, mi_row + hbs, mi_col + hbs);
      break;
    case PARTITION_HORZ_4:
      for (i = 0; i < 4; ++i) {
        int this_mi_row = mi_row + i * quarter_step;
        if (i > 0 && this_mi_row >= cm->mi_rows) break;

        write_modes_b(cpi, tile, w, tok, tok_end, this_mi_row, mi_col);
      }
      break;
    case PARTITION_VERT_4:
      for (i = 0; i < 4; ++i) {
        int this_mi_col = mi_col + i * quarter_step;
        if (i > 0 && this_mi_col >= cm->mi_cols) break;

        write_modes_b(cpi, tile, w, tok, tok_end, mi_row, this_mi_col);
      }
      break;
    default: assert(0);
  }

  // update partition context
  update_ext_partition_context(xd, mi_row, mi_col, subsize, bsize, partition);
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

  av1_zero_above_context(cm, mi_col_start, mi_col_end, tile->tile_row);
  av1_init_above_context(cm, xd, tile->tile_row);

  if (cpi->common.delta_q_present_flag) {
    xd->current_qindex = cpi->common.base_qindex;
    if (cpi->common.delta_lf_present_flag) {
      av1_reset_loop_filter_delta(xd, av1_num_planes(cm));
    }
  }

  for (mi_row = mi_row_start; mi_row < mi_row_end;
       mi_row += cm->seq_params.mib_size) {
    av1_zero_left_context(xd);

    for (mi_col = mi_col_start; mi_col < mi_col_end;
         mi_col += cm->seq_params.mib_size) {
      write_modes_sb(cpi, tile, w, tok, tok_end, mi_row, mi_col,
                     cm->seq_params.sb_size);
    }
  }
}

static void encode_restoration_mode(AV1_COMMON *cm,
                                    struct aom_write_bit_buffer *wb) {
  assert(!cm->all_lossless);
  if (!cm->seq_params.enable_restoration) return;
  if (cm->allow_intrabc) return;
  const int num_planes = av1_num_planes(cm);
  int all_none = 1, chroma_none = 1;
  for (int p = 0; p < num_planes; ++p) {
    RestorationInfo *rsi = &cm->rst_info[p];
    if (rsi->frame_restoration_type != RESTORE_NONE) {
      all_none = 0;
      chroma_none &= p == 0;
    }
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
  }
  if (!all_none) {
    assert(cm->seq_params.sb_size == BLOCK_64X64 ||
           cm->seq_params.sb_size == BLOCK_128X128);
    const int sb_size = cm->seq_params.sb_size == BLOCK_128X128 ? 128 : 64;

    RestorationInfo *rsi = &cm->rst_info[0];

    assert(rsi->restoration_unit_size >= sb_size);
    assert(RESTORATION_UNITSIZE_MAX == 256);

    if (sb_size == 64) {
      aom_wb_write_bit(wb, rsi->restoration_unit_size > 64);
    }
    if (rsi->restoration_unit_size > 64) {
      aom_wb_write_bit(wb, rsi->restoration_unit_size > 128);
    }
  }

  if (num_planes > 1) {
    int s = AOMMIN(cm->subsampling_x, cm->subsampling_y);
    if (s && !chroma_none) {
      aom_wb_write_bit(wb, cm->rst_info[1].restoration_unit_size !=
                               cm->rst_info[0].restoration_unit_size);
      assert(cm->rst_info[1].restoration_unit_size ==
                 cm->rst_info[0].restoration_unit_size ||
             cm->rst_info[1].restoration_unit_size ==
                 (cm->rst_info[0].restoration_unit_size >> s));
      assert(cm->rst_info[2].restoration_unit_size ==
             cm->rst_info[1].restoration_unit_size);
    } else if (!s) {
      assert(cm->rst_info[1].restoration_unit_size ==
             cm->rst_info[0].restoration_unit_size);
      assert(cm->rst_info[2].restoration_unit_size ==
             cm->rst_info[1].restoration_unit_size);
    }
  }
}

static void write_wiener_filter(int wiener_win, const WienerInfo *wiener_info,
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

static void write_sgrproj_filter(const SgrprojInfo *sgrproj_info,
                                 SgrprojInfo *ref_sgrproj_info,
                                 aom_writer *wb) {
  aom_write_literal(wb, sgrproj_info->ep, SGRPROJ_PARAMS_BITS);
  const sgr_params_type *params = &sgr_params[sgrproj_info->ep];

  if (params->r[0] == 0) {
    assert(sgrproj_info->xqd[0] == 0);
    aom_write_primitive_refsubexpfin(
        wb, SGRPROJ_PRJ_MAX1 - SGRPROJ_PRJ_MIN1 + 1, SGRPROJ_PRJ_SUBEXP_K,
        ref_sgrproj_info->xqd[1] - SGRPROJ_PRJ_MIN1,
        sgrproj_info->xqd[1] - SGRPROJ_PRJ_MIN1);
  } else if (params->r[1] == 0) {
    aom_write_primitive_refsubexpfin(
        wb, SGRPROJ_PRJ_MAX0 - SGRPROJ_PRJ_MIN0 + 1, SGRPROJ_PRJ_SUBEXP_K,
        ref_sgrproj_info->xqd[0] - SGRPROJ_PRJ_MIN0,
        sgrproj_info->xqd[0] - SGRPROJ_PRJ_MIN0);
  } else {
    aom_write_primitive_refsubexpfin(
        wb, SGRPROJ_PRJ_MAX0 - SGRPROJ_PRJ_MIN0 + 1, SGRPROJ_PRJ_SUBEXP_K,
        ref_sgrproj_info->xqd[0] - SGRPROJ_PRJ_MIN0,
        sgrproj_info->xqd[0] - SGRPROJ_PRJ_MIN0);
    aom_write_primitive_refsubexpfin(
        wb, SGRPROJ_PRJ_MAX1 - SGRPROJ_PRJ_MIN1 + 1, SGRPROJ_PRJ_SUBEXP_K,
        ref_sgrproj_info->xqd[1] - SGRPROJ_PRJ_MIN1,
        sgrproj_info->xqd[1] - SGRPROJ_PRJ_MIN1);
  }

  memcpy(ref_sgrproj_info, sgrproj_info, sizeof(*sgrproj_info));
}

static void loop_restoration_write_sb_coeffs(const AV1_COMMON *const cm,
                                             MACROBLOCKD *xd,
                                             const RestorationUnitInfo *rui,
                                             aom_writer *const w, int plane,
                                             FRAME_COUNTS *counts) {
  const RestorationInfo *rsi = cm->rst_info + plane;
  RestorationType frame_rtype = rsi->frame_restoration_type;
  if (frame_rtype == RESTORE_NONE) return;

  (void)counts;
  assert(!cm->all_lossless);

  const int wiener_win = (plane > 0) ? WIENER_WIN_CHROMA : WIENER_WIN;
  WienerInfo *wiener_info = xd->wiener_info + plane;
  SgrprojInfo *sgrproj_info = xd->sgrproj_info + plane;
  RestorationType unit_rtype = rui->restoration_type;

  if (frame_rtype == RESTORE_SWITCHABLE) {
    aom_write_symbol(w, unit_rtype, xd->tile_ctx->switchable_restore_cdf,
                     RESTORE_SWITCHABLE_TYPES);
#if CONFIG_ENTROPY_STATS
    ++counts->switchable_restore[unit_rtype];
#endif
    switch (unit_rtype) {
      case RESTORE_WIENER:
        write_wiener_filter(wiener_win, &rui->wiener_info, wiener_info, w);
        break;
      case RESTORE_SGRPROJ:
        write_sgrproj_filter(&rui->sgrproj_info, sgrproj_info, w);
        break;
      default: assert(unit_rtype == RESTORE_NONE); break;
    }
  } else if (frame_rtype == RESTORE_WIENER) {
    aom_write_symbol(w, unit_rtype != RESTORE_NONE,
                     xd->tile_ctx->wiener_restore_cdf, 2);
#if CONFIG_ENTROPY_STATS
    ++counts->wiener_restore[unit_rtype != RESTORE_NONE];
#endif
    if (unit_rtype != RESTORE_NONE) {
      write_wiener_filter(wiener_win, &rui->wiener_info, wiener_info, w);
    }
  } else if (frame_rtype == RESTORE_SGRPROJ) {
    aom_write_symbol(w, unit_rtype != RESTORE_NONE,
                     xd->tile_ctx->sgrproj_restore_cdf, 2);
#if CONFIG_ENTROPY_STATS
    ++counts->sgrproj_restore[unit_rtype != RESTORE_NONE];
#endif
    if (unit_rtype != RESTORE_NONE) {
      write_sgrproj_filter(&rui->sgrproj_info, sgrproj_info, w);
    }
  }
}

static void encode_loopfilter(AV1_COMMON *cm, struct aom_write_bit_buffer *wb) {
  assert(!cm->coded_lossless);
  if (cm->allow_intrabc) return;
  const int num_planes = av1_num_planes(cm);
  int i;
  struct loopfilter *lf = &cm->lf;

  // Encode the loop filter level and type
  aom_wb_write_literal(wb, lf->filter_level[0], 6);
  aom_wb_write_literal(wb, lf->filter_level[1], 6);
  if (num_planes > 1) {
    if (lf->filter_level[0] || lf->filter_level[1]) {
      aom_wb_write_literal(wb, lf->filter_level_u, 6);
      aom_wb_write_literal(wb, lf->filter_level_v, 6);
    }
  }
  aom_wb_write_literal(wb, lf->sharpness_level, 3);

  // Write out loop filter deltas applied at the MB level based on mode or
  // ref frame (if they are enabled).
  aom_wb_write_bit(wb, lf->mode_ref_delta_enabled);

  if (lf->mode_ref_delta_enabled) {
    aom_wb_write_bit(wb, lf->mode_ref_delta_update);

    if (lf->mode_ref_delta_update) {
      const int prime_idx = cm->primary_ref_frame;
      const int buf_idx =
          prime_idx == PRIMARY_REF_NONE ? -1 : cm->frame_refs[prime_idx].idx;
      int8_t last_ref_deltas[REF_FRAMES];
      if (prime_idx == PRIMARY_REF_NONE || buf_idx < 0) {
        av1_set_default_ref_deltas(last_ref_deltas);
      } else {
        memcpy(last_ref_deltas, cm->buffer_pool->frame_bufs[buf_idx].ref_deltas,
               REF_FRAMES);
      }
      for (i = 0; i < REF_FRAMES; i++) {
        const int delta = lf->ref_deltas[i];
        const int changed = delta != last_ref_deltas[i];
        aom_wb_write_bit(wb, changed);
        if (changed) aom_wb_write_inv_signed_literal(wb, delta, 6);
      }

      int8_t last_mode_deltas[MAX_MODE_LF_DELTAS];
      if (prime_idx == PRIMARY_REF_NONE || buf_idx < 0) {
        av1_set_default_mode_deltas(last_mode_deltas);
      } else {
        memcpy(last_mode_deltas,
               cm->buffer_pool->frame_bufs[buf_idx].mode_deltas,
               MAX_MODE_LF_DELTAS);
      }
      for (i = 0; i < MAX_MODE_LF_DELTAS; i++) {
        const int delta = lf->mode_deltas[i];
        const int changed = delta != last_mode_deltas[i];
        aom_wb_write_bit(wb, changed);
        if (changed) aom_wb_write_inv_signed_literal(wb, delta, 6);
      }
    }
  }
}

static void encode_cdef(const AV1_COMMON *cm, struct aom_write_bit_buffer *wb) {
  assert(!cm->coded_lossless);
  if (!cm->seq_params.enable_cdef) return;
  if (cm->allow_intrabc) return;
  const int num_planes = av1_num_planes(cm);
  int i;
  aom_wb_write_literal(wb, cm->cdef_pri_damping - 3, 2);
  assert(cm->cdef_pri_damping == cm->cdef_sec_damping);
  aom_wb_write_literal(wb, cm->cdef_bits, 2);
  for (i = 0; i < cm->nb_cdef_strengths; i++) {
    aom_wb_write_literal(wb, cm->cdef_strengths[i], CDEF_STRENGTH_BITS);
    if (num_planes > 1)
      aom_wb_write_literal(wb, cm->cdef_uv_strengths[i], CDEF_STRENGTH_BITS);
  }
}

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
  const int num_planes = av1_num_planes(cm);

  aom_wb_write_literal(wb, cm->base_qindex, QINDEX_BITS);
  write_delta_q(wb, cm->y_dc_delta_q);
  if (num_planes > 1) {
    int diff_uv_delta = (cm->u_dc_delta_q != cm->v_dc_delta_q) ||
                        (cm->u_ac_delta_q != cm->v_ac_delta_q);
    if (cm->separate_uv_delta_q) aom_wb_write_bit(wb, diff_uv_delta);
    write_delta_q(wb, cm->u_dc_delta_q);
    write_delta_q(wb, cm->u_ac_delta_q);
    if (diff_uv_delta) {
      write_delta_q(wb, cm->v_dc_delta_q);
      write_delta_q(wb, cm->v_ac_delta_q);
    }
  }
  aom_wb_write_bit(wb, cm->using_qmatrix);
  if (cm->using_qmatrix) {
    aom_wb_write_literal(wb, cm->qm_y, QM_LEVEL_BITS);
    aom_wb_write_literal(wb, cm->qm_u, QM_LEVEL_BITS);
    if (!cm->separate_uv_delta_q)
      assert(cm->qm_u == cm->qm_v);
    else
      aom_wb_write_literal(wb, cm->qm_v, QM_LEVEL_BITS);
  }
}

static void encode_segmentation(AV1_COMMON *cm, MACROBLOCKD *xd,
                                struct aom_write_bit_buffer *wb) {
  int i, j;
  struct segmentation *seg = &cm->seg;

  aom_wb_write_bit(wb, seg->enabled);
  if (!seg->enabled) return;

  // Write update flags
  if (cm->primary_ref_frame == PRIMARY_REF_NONE) {
    assert(seg->update_map == 1);
    seg->temporal_update = 0;
    assert(seg->update_data == 1);
  } else {
    aom_wb_write_bit(wb, seg->update_map);
    if (seg->update_map) {
      // Select the coding strategy (temporal or spatial)
      av1_choose_segmap_coding_method(cm, xd);
      aom_wb_write_bit(wb, seg->temporal_update);
    }
    aom_wb_write_bit(wb, seg->update_data);
  }

  // Segmentation data
  if (seg->update_data) {
    for (i = 0; i < MAX_SEGMENTS; i++) {
      for (j = 0; j < SEG_LVL_MAX; j++) {
        const int active = segfeature_active(seg, i, j);
        aom_wb_write_bit(wb, active);
        if (active) {
          const int data_max = av1_seg_feature_data_max(j);
          const int data_min = -data_max;
          const int ubits = get_unsigned_bits(data_max);
          const int data = clamp(get_segdata(seg, i, j), data_min, data_max);

          if (av1_is_segfeature_signed(j)) {
            aom_wb_write_inv_signed_literal(wb, data, ubits);
          } else {
            aom_wb_write_literal(wb, data, ubits);
          }
        }
      }
    }
  }
}

static void write_tx_mode(AV1_COMMON *cm, TX_MODE *mode,
                          struct aom_write_bit_buffer *wb) {
  if (cm->coded_lossless) {
    *mode = ONLY_4X4;
    return;
  }
  aom_wb_write_bit(wb, *mode == TX_MODE_SELECT);
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
          if (i == EIGHTTAP_REGULAR) cm->interp_filter = i;
          break;
        }
      }
    }
  }
}

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
  int width_mi = ALIGN_POWER_OF_TWO(cm->mi_cols, cm->seq_params.mib_size_log2);
  int height_mi = ALIGN_POWER_OF_TWO(cm->mi_rows, cm->seq_params.mib_size_log2);
  int width_sb = width_mi >> cm->seq_params.mib_size_log2;
  int height_sb = height_mi >> cm->seq_params.mib_size_log2;
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
      wb_write_uniform(wb, AOMMIN(width_sb, cm->max_tile_width_sb),
                       size_sb - 1);
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

static void write_tile_info(const AV1_COMMON *const cm,
                            struct aom_write_bit_buffer *saved_wb,
                            struct aom_write_bit_buffer *wb) {
  write_tile_info_max_tile(cm, wb);

  *saved_wb = *wb;
  if (cm->tile_rows * cm->tile_cols > 1) {
    // tile id used for cdf update
    aom_wb_write_literal(wb, 0, cm->log2_tile_cols + cm->log2_tile_rows);
    // Number of bytes in tile size - 1
    aom_wb_write_literal(wb, 3, 2);
  }
}

static void write_ext_tile_info(const AV1_COMMON *const cm,
                                struct aom_write_bit_buffer *saved_wb,
                                struct aom_write_bit_buffer *wb) {
  // This information is stored as a separate byte.
  int mod = wb->bit_offset % CHAR_BIT;
  if (mod > 0) aom_wb_write_literal(wb, 0, CHAR_BIT - mod);
  assert(aom_wb_is_byte_aligned(wb));

  *saved_wb = *wb;
  if (cm->tile_rows * cm->tile_cols > 1) {
    // Note that the last item in the uncompressed header is the data
    // describing tile configuration.
    // Number of bytes in tile column size - 1
    aom_wb_write_literal(wb, 0, 2);
    // Number of bytes in tile size - 1
    aom_wb_write_literal(wb, 0, 2);
  }
}

#if USE_GF16_MULTI_LAYER
static int get_refresh_mask_gf16(AV1_COMP *cpi) {
  if (cpi->common.frame_type == KEY_FRAME || frame_is_sframe(&cpi->common))
    return 0xFF;

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

static int get_refresh_mask(AV1_COMP *cpi) {
  if (cpi->common.frame_type == KEY_FRAME || frame_is_sframe(&cpi->common))
    return 0xFF;

  int refresh_mask = 0;
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
      (cpi->refresh_last_frame << cpi->ref_fb_idx[LAST_REF_FRAMES - 1]);

  refresh_mask |=
      (cpi->refresh_bwd_ref_frame << cpi->ref_fb_idx[BWDREF_FRAME - 1]);
  refresh_mask |=
      (cpi->refresh_alt2_ref_frame << cpi->ref_fb_idx[ALTREF2_FRAME - 1]);

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
    return refresh_mask |
           (cpi->refresh_golden_frame << cpi->ref_fb_idx[ALTREF_FRAME - 1]);
  } else {
    const int arf_idx = cpi->ref_fb_idx[ALTREF_FRAME - 1];
    return refresh_mask |
           (cpi->refresh_golden_frame << cpi->ref_fb_idx[GOLDEN_FRAME - 1]) |
           (cpi->refresh_alt_ref_frame << arf_idx);
  }
}

static INLINE int find_identical_tile(
    const int tile_row, const int tile_col,
    TileBufferEnc (*const tile_buffers)[MAX_TILE_COLS]) {
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

static void write_render_size(const AV1_COMMON *cm,
                              struct aom_write_bit_buffer *wb) {
  const int scaling_active = av1_resize_scaled(cm);
  aom_wb_write_bit(wb, scaling_active);
  if (scaling_active) {
    aom_wb_write_literal(wb, cm->render_width - 1, 16);
    aom_wb_write_literal(wb, cm->render_height - 1, 16);
  }
}

static void write_superres_scale(const AV1_COMMON *const cm,
                                 struct aom_write_bit_buffer *wb) {
  const SequenceHeader *const seq_params = &cm->seq_params;
  if (!seq_params->enable_superres) {
    assert(cm->superres_scale_denominator == SCALE_NUMERATOR);
    return;
  }

  // First bit is whether to to scale or not
  if (cm->superres_scale_denominator == SCALE_NUMERATOR) {
    aom_wb_write_bit(wb, 0);  // no scaling
  } else {
    aom_wb_write_bit(wb, 1);  // scaling, write scale factor
    assert(cm->superres_scale_denominator >= SUPERRES_SCALE_DENOMINATOR_MIN);
    assert(cm->superres_scale_denominator <
           SUPERRES_SCALE_DENOMINATOR_MIN + (1 << SUPERRES_SCALE_BITS));
    aom_wb_write_literal(
        wb, cm->superres_scale_denominator - SUPERRES_SCALE_DENOMINATOR_MIN,
        SUPERRES_SCALE_BITS);
  }
}

static void write_frame_size(const AV1_COMMON *cm, int frame_size_override,
                             struct aom_write_bit_buffer *wb) {
  const int coded_width = cm->superres_upscaled_width - 1;
  const int coded_height = cm->superres_upscaled_height - 1;

  if (frame_size_override) {
    const SequenceHeader *seq_params = &cm->seq_params;
    int num_bits_width = seq_params->num_bits_width;
    int num_bits_height = seq_params->num_bits_height;
    aom_wb_write_literal(wb, coded_width, num_bits_width);
    aom_wb_write_literal(wb, coded_height, num_bits_height);
  }

  write_superres_scale(cm, wb);
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
      found = cm->superres_upscaled_width == cfg->y_crop_width &&
              cm->superres_upscaled_height == cfg->y_crop_height;
      found &= cm->render_width == cfg->render_width &&
               cm->render_height == cfg->render_height;
    }
    aom_wb_write_bit(wb, found);
    if (found) {
      write_superres_scale(cm, wb);
      break;
    }
  }

  if (!found) {
    int frame_size_override = 1;  // Always equal to 1 in this function
    write_frame_size(cm, frame_size_override, wb);
  }
}

static void write_profile(BITSTREAM_PROFILE profile,
                          struct aom_write_bit_buffer *wb) {
  assert(profile >= PROFILE_0 && profile < MAX_PROFILES);
  aom_wb_write_literal(wb, profile, PROFILE_BITS);
}

static void write_bitdepth(AV1_COMMON *const cm,
                           struct aom_write_bit_buffer *wb) {
  // Profile 0/1: [0] for 8 bit, [1]  10-bit
  // Profile   2: [0] for 8 bit, [10] 10-bit, [11] - 12-bit
  aom_wb_write_bit(wb, cm->bit_depth == AOM_BITS_8 ? 0 : 1);
  if (cm->profile == PROFILE_2 && cm->bit_depth != AOM_BITS_8) {
    aom_wb_write_bit(wb, cm->bit_depth == AOM_BITS_10 ? 0 : 1);
  }
}

static void write_color_config(AV1_COMMON *const cm,
                               struct aom_write_bit_buffer *wb) {
  write_bitdepth(cm, wb);
  const int is_monochrome = cm->seq_params.monochrome;
  // monochrome bit
  if (cm->profile != PROFILE_1)
    aom_wb_write_bit(wb, is_monochrome);
  else
    assert(!is_monochrome);
  if (cm->color_primaries == AOM_CICP_CP_UNSPECIFIED &&
      cm->transfer_characteristics == AOM_CICP_TC_UNSPECIFIED &&
      cm->matrix_coefficients == AOM_CICP_MC_UNSPECIFIED) {
    aom_wb_write_bit(wb, 0);  // No color description present
  } else {
    aom_wb_write_bit(wb, 1);  // Color description present
    aom_wb_write_literal(wb, cm->color_primaries, 8);
    aom_wb_write_literal(wb, cm->transfer_characteristics, 8);
    aom_wb_write_literal(wb, cm->matrix_coefficients, 8);
  }
  if (is_monochrome) {
    // 0: [16, 235] (i.e. xvYCC), 1: [0, 255]
    aom_wb_write_bit(wb, cm->color_range);
    return;
  }
  if (cm->color_primaries == AOM_CICP_CP_BT_709 &&
      cm->transfer_characteristics == AOM_CICP_TC_SRGB &&
      cm->matrix_coefficients ==
          AOM_CICP_MC_IDENTITY) {  // it would be better to remove this
                                   // dependency too
    assert(cm->subsampling_x == 0 && cm->subsampling_y == 0);
    assert(cm->profile == PROFILE_1 ||
           (cm->profile == PROFILE_2 && cm->bit_depth == AOM_BITS_12));
  } else {
    // 0: [16, 235] (i.e. xvYCC), 1: [0, 255]
    aom_wb_write_bit(wb, cm->color_range);
    if (cm->profile == PROFILE_0) {
      // 420 only
      assert(cm->subsampling_x == 1 && cm->subsampling_y == 1);
    } else if (cm->profile == PROFILE_1) {
      // 444 only
      assert(cm->subsampling_x == 0 && cm->subsampling_y == 0);
    } else if (cm->profile == PROFILE_2) {
      if (cm->bit_depth == AOM_BITS_12) {
        // 420, 444 or 422
        aom_wb_write_bit(wb, cm->subsampling_x);
        if (cm->subsampling_x == 0) {
          assert(cm->subsampling_y == 0 &&
                 "4:4:0 subsampling not allowed in AV1");
        } else {
          aom_wb_write_bit(wb, cm->subsampling_y);
        }
      } else {
        // 422 only
        assert(cm->subsampling_x == 1 && cm->subsampling_y == 0);
      }
    }
    if (cm->matrix_coefficients == AOM_CICP_MC_IDENTITY) {
      assert(cm->subsampling_x == 0 && cm->subsampling_y == 0);
    }
    if (cm->subsampling_x == 1 && cm->subsampling_y == 1) {
      aom_wb_write_literal(wb, cm->chroma_sample_position, 2);
    }
  }
  aom_wb_write_bit(wb, cm->separate_uv_delta_q);
}

static void write_timing_info_header(AV1_COMMON *const cm,
                                     struct aom_write_bit_buffer *wb) {
  aom_wb_write_unsigned_literal(wb, cm->timing_info.num_units_in_display_tick,
                                32);  // Number of units in tick
  aom_wb_write_unsigned_literal(wb, cm->timing_info.time_scale,
                                32);  // Time scale
  aom_wb_write_bit(
      wb,
      cm->timing_info.equal_picture_interval);  // Equal picture interval bit
  if (cm->timing_info.equal_picture_interval) {
    aom_wb_write_uvlc(
        wb,
        cm->timing_info.num_ticks_per_picture - 1);  // ticks per picture
  }
}

static void write_decoder_model_info(AV1_COMMON *const cm,
                                     struct aom_write_bit_buffer *wb) {
  aom_wb_write_literal(
      wb, cm->buffer_model.encoder_decoder_buffer_delay_length - 1, 5);
  aom_wb_write_unsigned_literal(wb, cm->buffer_model.num_units_in_decoding_tick,
                                32);  // Number of units in decoding tick
  aom_wb_write_literal(wb, cm->buffer_model.buffer_removal_delay_length - 1, 5);
  aom_wb_write_literal(wb, cm->buffer_model.frame_presentation_delay_length - 1,
                       5);
}

static void write_dec_model_op_parameters(AV1_COMMON *const cm,
                                          struct aom_write_bit_buffer *wb,
                                          int op_num) {
  if (op_num > MAX_NUM_OPERATING_POINTS)
    aom_internal_error(
        &cm->error, AOM_CODEC_UNSUP_BITSTREAM,
        "Encoder does not support %d decoder model operating points", op_num);

  //  aom_wb_write_bit(wb, cm->op_params[op_num].has_parameters);
  //  if (!cm->op_params[op_num].has_parameters) return;

  aom_wb_write_literal(wb, cm->op_params[op_num].decoder_buffer_delay,
                       cm->buffer_model.encoder_decoder_buffer_delay_length);

  aom_wb_write_literal(wb, cm->op_params[op_num].encoder_buffer_delay,
                       cm->buffer_model.encoder_decoder_buffer_delay_length);

  aom_wb_write_bit(wb, cm->op_params[op_num].low_delay_mode_flag);

  cm->op_frame_timing[op_num].buffer_removal_delay =
      0;  // reset the decoded frame counter
}

static void write_tu_pts_info(AV1_COMMON *const cm,
                              struct aom_write_bit_buffer *wb) {
  aom_wb_write_unsigned_literal(
      wb, (uint32_t)cm->tu_presentation_delay,
      cm->buffer_model.frame_presentation_delay_length);
}

static void write_film_grain_params(AV1_COMP *cpi,
                                    struct aom_write_bit_buffer *wb) {
  AV1_COMMON *const cm = &cpi->common;
  aom_film_grain_t *pars = &cm->film_grain_params;

  cm->cur_frame->film_grain_params = *pars;

  aom_wb_write_bit(wb, pars->apply_grain);
  if (!pars->apply_grain) return;

  aom_wb_write_literal(wb, pars->random_seed, 16);

  pars->random_seed += 3245;  // For film grain test vectors purposes
  if (!pars->random_seed)     // Random seed should not be zero
    pars->random_seed += 1735;
  if (cm->frame_type == INTER_FRAME)
    aom_wb_write_bit(wb, pars->update_parameters);
  else
    pars->update_parameters = 1;
  if (!pars->update_parameters) {
    RefCntBuffer *const frame_bufs = cm->buffer_pool->frame_bufs;
    int ref_frame, ref_idx, buf_idx;
    for (ref_frame = LAST_FRAME; ref_frame < REF_FRAMES; ref_frame++) {
      ref_idx = get_ref_frame_map_idx(cpi, ref_frame);
      assert(ref_idx != INVALID_IDX);
      buf_idx = cm->ref_frame_map[ref_idx];
      if (frame_bufs[buf_idx].film_grain_params_present &&
          memcmp(pars, &frame_bufs[buf_idx].film_grain_params, sizeof(*pars))) {
        break;
      }
    }
    assert(ref_frame < REF_FRAMES);
    aom_wb_write_literal(wb, ref_idx, 3);
    return;
  }

  // Scaling functions parameters
  aom_wb_write_literal(wb, pars->num_y_points, 4);  // max 14
  for (int i = 0; i < pars->num_y_points; i++) {
    aom_wb_write_literal(wb, pars->scaling_points_y[i][0], 8);
    aom_wb_write_literal(wb, pars->scaling_points_y[i][1], 8);
  }

  if (!cm->seq_params.monochrome)
    aom_wb_write_bit(wb, pars->chroma_scaling_from_luma);
  else
    pars->chroma_scaling_from_luma = 0;  // for monochrome override to 0

  if (cm->seq_params.monochrome || pars->chroma_scaling_from_luma ||
      ((cm->subsampling_x == 1) && (cm->subsampling_y == 1) &&
       (pars->num_y_points == 0))) {
    pars->num_cb_points = 0;
    pars->num_cr_points = 0;
  } else {
    aom_wb_write_literal(wb, pars->num_cb_points, 4);  // max 10
    for (int i = 0; i < pars->num_cb_points; i++) {
      aom_wb_write_literal(wb, pars->scaling_points_cb[i][0], 8);
      aom_wb_write_literal(wb, pars->scaling_points_cb[i][1], 8);
    }

    aom_wb_write_literal(wb, pars->num_cr_points, 4);  // max 10
    for (int i = 0; i < pars->num_cr_points; i++) {
      aom_wb_write_literal(wb, pars->scaling_points_cr[i][0], 8);
      aom_wb_write_literal(wb, pars->scaling_points_cr[i][1], 8);
    }
  }

  aom_wb_write_literal(wb, pars->scaling_shift - 8, 2);  // 8 + value

  // AR coefficients
  // Only sent if the corresponsing scaling function has
  // more than 0 points

  aom_wb_write_literal(wb, pars->ar_coeff_lag, 2);

  int num_pos_luma = 2 * pars->ar_coeff_lag * (pars->ar_coeff_lag + 1);
  int num_pos_chroma = num_pos_luma;
  if (pars->num_y_points > 0) ++num_pos_chroma;

  if (pars->num_y_points)
    for (int i = 0; i < num_pos_luma; i++)
      aom_wb_write_literal(wb, pars->ar_coeffs_y[i] + 128, 8);

  if (pars->num_cb_points || pars->chroma_scaling_from_luma)
    for (int i = 0; i < num_pos_chroma; i++)
      aom_wb_write_literal(wb, pars->ar_coeffs_cb[i] + 128, 8);

  if (pars->num_cr_points || pars->chroma_scaling_from_luma)
    for (int i = 0; i < num_pos_chroma; i++)
      aom_wb_write_literal(wb, pars->ar_coeffs_cr[i] + 128, 8);

  aom_wb_write_literal(wb, pars->ar_coeff_shift - 6, 2);  // 8 + value

  aom_wb_write_literal(wb, pars->grain_scale_shift, 2);

  if (pars->num_cb_points) {
    aom_wb_write_literal(wb, pars->cb_mult, 8);
    aom_wb_write_literal(wb, pars->cb_luma_mult, 8);
    aom_wb_write_literal(wb, pars->cb_offset, 9);
  }

  if (pars->num_cr_points) {
    aom_wb_write_literal(wb, pars->cr_mult, 8);
    aom_wb_write_literal(wb, pars->cr_luma_mult, 8);
    aom_wb_write_literal(wb, pars->cr_offset, 9);
  }

  aom_wb_write_bit(wb, pars->overlap_flag);

  aom_wb_write_bit(wb, pars->clip_to_restricted_range);
}

static void write_sb_size(SequenceHeader *seq_params,
                          struct aom_write_bit_buffer *wb) {
  (void)seq_params;
  (void)wb;
  assert(seq_params->mib_size == mi_size_wide[seq_params->sb_size]);
  assert(seq_params->mib_size == 1 << seq_params->mib_size_log2);
  assert(seq_params->sb_size == BLOCK_128X128 ||
         seq_params->sb_size == BLOCK_64X64);
  aom_wb_write_bit(wb, seq_params->sb_size == BLOCK_128X128 ? 1 : 0);
}

void write_sequence_header(AV1_COMP *cpi, struct aom_write_bit_buffer *wb) {
  AV1_COMMON *const cm = &cpi->common;
  SequenceHeader *seq_params = &cm->seq_params;

  int max_frame_width = cpi->oxcf.forced_max_frame_width
                            ? cpi->oxcf.forced_max_frame_width
                            : cpi->oxcf.width;
  int max_frame_height = cpi->oxcf.forced_max_frame_height
                             ? cpi->oxcf.forced_max_frame_height
                             : cpi->oxcf.height;
  const int num_bits_width =
      (max_frame_width > 1) ? get_msb(max_frame_width - 1) + 1 : 1;
  const int num_bits_height =
      (max_frame_height > 1) ? get_msb(max_frame_height - 1) + 1 : 1;
  assert(num_bits_width <= 16);
  assert(num_bits_height <= 16);

  seq_params->num_bits_width = num_bits_width;
  seq_params->num_bits_height = num_bits_height;
  seq_params->max_frame_width = max_frame_width;
  seq_params->max_frame_height = max_frame_height;

  aom_wb_write_literal(wb, num_bits_width - 1, 4);
  aom_wb_write_literal(wb, num_bits_height - 1, 4);
  aom_wb_write_literal(wb, max_frame_width - 1, num_bits_width);
  aom_wb_write_literal(wb, max_frame_height - 1, num_bits_height);

  /* Placeholder for actually writing to the bitstream */
  if (!seq_params->reduced_still_picture_hdr) {
    seq_params->frame_id_numbers_present_flag =
        cm->large_scale_tile ? 0 : cm->error_resilient_mode;
    seq_params->frame_id_length = FRAME_ID_LENGTH;
    seq_params->delta_frame_id_length = DELTA_FRAME_ID_LENGTH;

    aom_wb_write_bit(wb, seq_params->frame_id_numbers_present_flag);
    if (seq_params->frame_id_numbers_present_flag) {
      // We must always have delta_frame_id_length < frame_id_length,
      // in order for a frame to be referenced with a unique delta.
      // Avoid wasting bits by using a coding that enforces this restriction.
      aom_wb_write_literal(wb, seq_params->delta_frame_id_length - 2, 4);
      aom_wb_write_literal(
          wb,
          seq_params->frame_id_length - seq_params->delta_frame_id_length - 1,
          3);
    }
  }

  write_sb_size(seq_params, wb);

  aom_wb_write_bit(wb, seq_params->enable_filter_intra);
  aom_wb_write_bit(wb, seq_params->enable_intra_edge_filter);

  if (!seq_params->reduced_still_picture_hdr) {
    aom_wb_write_bit(wb, seq_params->enable_interintra_compound);
    aom_wb_write_bit(wb, seq_params->enable_masked_compound);
    aom_wb_write_bit(wb, seq_params->enable_warped_motion);
    aom_wb_write_bit(wb, seq_params->enable_dual_filter);

    aom_wb_write_bit(wb, seq_params->enable_order_hint);

    if (seq_params->enable_order_hint) {
      aom_wb_write_bit(wb, seq_params->enable_jnt_comp);
      aom_wb_write_bit(wb, seq_params->enable_ref_frame_mvs);
    }
    if (seq_params->force_screen_content_tools == 2) {
      aom_wb_write_bit(wb, 1);
    } else {
      aom_wb_write_bit(wb, 0);
      aom_wb_write_bit(wb, seq_params->force_screen_content_tools);
    }
    if (seq_params->force_screen_content_tools > 0) {
      if (seq_params->force_integer_mv == 2) {
        aom_wb_write_bit(wb, 1);
      } else {
        aom_wb_write_bit(wb, 0);
        aom_wb_write_bit(wb, seq_params->force_integer_mv);
      }
    } else {
      assert(seq_params->force_integer_mv == 2);
    }
    if (seq_params->enable_order_hint)
      aom_wb_write_literal(wb, seq_params->order_hint_bits_minus_1, 3);
  }

  aom_wb_write_bit(wb, seq_params->enable_superres);
  aom_wb_write_bit(wb, seq_params->enable_cdef);
  aom_wb_write_bit(wb, seq_params->enable_restoration);
}

static void write_global_motion_params(const WarpedMotionParams *params,
                                       const WarpedMotionParams *ref_params,
                                       struct aom_write_bit_buffer *wb,
                                       int allow_hp) {
  const TransformationType type = params->wmtype;

  aom_wb_write_bit(wb, type != IDENTITY);
  if (type != IDENTITY) {
    aom_wb_write_bit(wb, type == ROTZOOM);
    if (type != ROTZOOM) aom_wb_write_bit(wb, type == TRANSLATION);
  }

  if (type >= ROTZOOM) {
    aom_wb_write_signed_primitive_refsubexpfin(
        wb, GM_ALPHA_MAX + 1, SUBEXPFIN_K,
        (ref_params->wmmat[2] >> GM_ALPHA_PREC_DIFF) -
            (1 << GM_ALPHA_PREC_BITS),
        (params->wmmat[2] >> GM_ALPHA_PREC_DIFF) - (1 << GM_ALPHA_PREC_BITS));
    aom_wb_write_signed_primitive_refsubexpfin(
        wb, GM_ALPHA_MAX + 1, SUBEXPFIN_K,
        (ref_params->wmmat[3] >> GM_ALPHA_PREC_DIFF),
        (params->wmmat[3] >> GM_ALPHA_PREC_DIFF));
  }

  if (type >= AFFINE) {
    aom_wb_write_signed_primitive_refsubexpfin(
        wb, GM_ALPHA_MAX + 1, SUBEXPFIN_K,
        (ref_params->wmmat[4] >> GM_ALPHA_PREC_DIFF),
        (params->wmmat[4] >> GM_ALPHA_PREC_DIFF));
    aom_wb_write_signed_primitive_refsubexpfin(
        wb, GM_ALPHA_MAX + 1, SUBEXPFIN_K,
        (ref_params->wmmat[5] >> GM_ALPHA_PREC_DIFF) -
            (1 << GM_ALPHA_PREC_BITS),
        (params->wmmat[5] >> GM_ALPHA_PREC_DIFF) - (1 << GM_ALPHA_PREC_BITS));
  }

  if (type >= TRANSLATION) {
    const int trans_bits = (type == TRANSLATION)
                               ? GM_ABS_TRANS_ONLY_BITS - !allow_hp
                               : GM_ABS_TRANS_BITS;
    const int trans_prec_diff = (type == TRANSLATION)
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
  }
}

static void write_global_motion(AV1_COMP *cpi,
                                struct aom_write_bit_buffer *wb) {
  AV1_COMMON *const cm = &cpi->common;
  int frame;
  for (frame = LAST_FRAME; frame <= ALTREF_FRAME; ++frame) {
    const WarpedMotionParams *ref_params =
        cm->prev_frame ? &cm->prev_frame->global_motion[frame]
                       : &default_warp_params;
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

static void check_frame_refs_short_signaling(AV1_COMP *const cpi) {
  AV1_COMMON *const cm = &cpi->common;
  if (!cm->frame_refs_short_signaling) return;

  // Check whether all references are distinct frames.
  int buf_markers[FRAME_BUFFERS] = { 0 };
  for (int ref_frame = LAST_FRAME; ref_frame <= ALTREF_FRAME; ++ref_frame) {
    const int buf_idx = get_ref_frame_buf_idx(cpi, ref_frame);
    if (buf_idx != INVALID_IDX) {
      assert(buf_idx >= 0 && buf_idx < FRAME_BUFFERS);
      buf_markers[buf_idx] = 1;
    }
  }

  int num_refs = 0;
  for (int buf_idx = 0; buf_idx < FRAME_BUFFERS; ++buf_idx) {
    num_refs += buf_markers[buf_idx];
  }

  // We only turn on frame_refs_short_signaling when all references are
  // distinct.
  if (num_refs < INTER_REFS_PER_FRAME) {
    // It indicates that there exist more than one reference frame pointing to
    // the same reference buffer, i.e. two or more references are duplicate.
    cm->frame_refs_short_signaling = 0;
    return;
  }

  // Check whether the encoder side ref frame choices are aligned with that to
  // be derived at the decoder side.
  RefBuffer frame_refs_copy[INTER_REFS_PER_FRAME];

  // Backup the frame refs info
  memcpy(frame_refs_copy, cm->frame_refs,
         INTER_REFS_PER_FRAME * sizeof(RefBuffer));

  const int lst_map_idx = get_ref_frame_map_idx(cpi, LAST_FRAME);
  const int gld_map_idx = get_ref_frame_map_idx(cpi, GOLDEN_FRAME);

  // Set up the frame refs mapping indexes according to the
  // frame_refs_short_signaling policy.
  av1_set_frame_refs(cm, lst_map_idx, gld_map_idx);

  // We only turn on frame_refs_short_signaling when the encoder side decision
  // on ref frames is identical to that at the decoder side.
  for (int ref_idx = 0; ref_idx < INTER_REFS_PER_FRAME; ++ref_idx) {
    // Compare the buffer index between two reference frames indexed
    // respectively by the encoder and the decoder side decisions.
    if (cm->frame_refs[ref_idx].idx != frame_refs_copy[ref_idx].idx) {
      cm->frame_refs_short_signaling = 0;
      break;
    }
  }

#if 0   // For debug
  printf("\nFrame=%d: \n", cm->current_video_frame);
  printf("***frame_refs_short_signaling=%d\n", cm->frame_refs_short_signaling);
  for (int ref_frame = LAST_FRAME; ref_frame <= ALTREF_FRAME; ++ref_frame) {
    printf("enc_ref(map_idx=%d, buf_idx=%d)=%d, vs. "
        "dec_ref(map_idx=%d, buf_idx=%d)=%d\n",
        get_ref_frame_map_idx(cpi, ref_frame),
        get_ref_frame_buf_idx(cpi, ref_frame), ref_frame,
        cm->frame_refs[ref_frame - LAST_FRAME].map_idx,
        cm->frame_refs[ref_frame - LAST_FRAME].idx, ref_frame);
  }
#endif  // 0

  // Restore the frame refs info if frame_refs_short_signaling is off.
  if (!cm->frame_refs_short_signaling)
    memcpy(cm->frame_refs, frame_refs_copy,
           INTER_REFS_PER_FRAME * sizeof(RefBuffer));
}

// New function based on HLS R18
static void write_uncompressed_header_obu(AV1_COMP *cpi,
                                          struct aom_write_bit_buffer *saved_wb,
                                          struct aom_write_bit_buffer *wb) {
  AV1_COMMON *const cm = &cpi->common;
  MACROBLOCKD *const xd = &cpi->td.mb.e_mbd;

  // NOTE: By default all coded frames to be used as a reference
  cm->is_reference_frame = 1;
  cm->frame_type = cm->intra_only ? INTRA_ONLY_FRAME : cm->frame_type;

  if (cm->seq_params.still_picture) {
    assert(cm->show_existing_frame == 0);
    assert(cm->show_frame == 1);
    assert(cm->frame_type == KEY_FRAME);
  }
  if (!cm->seq_params.reduced_still_picture_hdr) {
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

      if (cm->seq_params.decoder_model_info_present_flag &&
          cm->timing_info.equal_picture_interval == 0) {
        write_tu_pts_info(cm, wb);
      }
      if (cm->seq_params.frame_id_numbers_present_flag) {
        int frame_id_len = cm->seq_params.frame_id_length;
        int display_frame_id = cm->ref_frame_id[cpi->existing_fb_idx_to_show];
        aom_wb_write_literal(wb, display_frame_id, frame_id_len);
      }

      if (cm->reset_decoder_state &&
          frame_bufs[frame_to_show].frame_type != KEY_FRAME) {
        aom_internal_error(
            &cm->error, AOM_CODEC_UNSUP_BITSTREAM,
            "show_existing_frame to reset state on KEY_FRAME only");
      }

      return;
    } else {
      aom_wb_write_bit(wb, 0);  // show_existing_frame
    }

    aom_wb_write_literal(wb, cm->frame_type, 2);

    aom_wb_write_bit(wb, cm->show_frame);
    if (cm->show_frame) {
      if (cm->seq_params.decoder_model_info_present_flag &&
          cm->timing_info.equal_picture_interval == 0)
        write_tu_pts_info(cm, wb);
    } else {
      aom_wb_write_bit(wb, cm->showable_frame);
    }
    if (frame_is_sframe(cm)) {
      assert(cm->error_resilient_mode);
    } else if (!(cm->frame_type == KEY_FRAME && cm->show_frame)) {
      aom_wb_write_bit(wb, cm->error_resilient_mode);
    }
  }
  aom_wb_write_bit(wb, cm->disable_cdf_update);

  if (cm->seq_params.force_screen_content_tools == 2) {
    aom_wb_write_bit(wb, cm->allow_screen_content_tools);
  } else {
    assert(cm->allow_screen_content_tools ==
           cm->seq_params.force_screen_content_tools);
  }

  if (cm->allow_screen_content_tools) {
    if (cm->seq_params.force_integer_mv == 2) {
      aom_wb_write_bit(wb, cm->cur_frame_force_integer_mv);
    } else {
      assert(cm->cur_frame_force_integer_mv == cm->seq_params.force_integer_mv);
    }
  } else {
    assert(cm->cur_frame_force_integer_mv == 0);
  }

  cm->invalid_delta_frame_id_minus_1 = 0;
  int frame_size_override_flag = 0;
  cm->frame_refs_short_signaling = 0;

  if (cm->seq_params.reduced_still_picture_hdr) {
    assert(cm->width == cm->seq_params.max_frame_width &&
           cm->height == cm->seq_params.max_frame_height);
  } else {
    if (cm->seq_params.frame_id_numbers_present_flag) {
      int frame_id_len = cm->seq_params.frame_id_length;
      aom_wb_write_literal(wb, cm->current_frame_id, frame_id_len);
    }

    if (cm->width > cm->seq_params.max_frame_width ||
        cm->height > cm->seq_params.max_frame_height) {
      aom_internal_error(&cm->error, AOM_CODEC_UNSUP_BITSTREAM,
                         "Frame dimensions are larger than the maximum values");
    }

    frame_size_override_flag =
        frame_is_sframe(cm) ? 1
                            : (cm->width != cm->seq_params.max_frame_width ||
                               cm->height != cm->seq_params.max_frame_height);
    if (!frame_is_sframe(cm)) aom_wb_write_bit(wb, frame_size_override_flag);

    if (cm->seq_params.enable_order_hint)
      aom_wb_write_literal(wb, cm->frame_offset,
                           cm->seq_params.order_hint_bits_minus_1 + 1);

    if (!cm->error_resilient_mode && !frame_is_intra_only(cm)) {
      aom_wb_write_literal(wb, cm->primary_ref_frame, PRIMARY_REF_BITS);
    }
  }

  if (cm->seq_params.decoder_model_info_present_flag) {
    aom_wb_write_bit(wb, cm->buffer_removal_delay_present);
    if (cm->buffer_removal_delay_present) {
      for (int op_num = 0;
           op_num < cm->seq_params.operating_points_cnt_minus_1 + 1; op_num++) {
        if (cm->op_params[op_num].decoder_model_param_present_flag) {
          if (((cm->seq_params.operating_point_idc[op_num] >>
                cm->temporal_layer_id) &
                   0x1 &&
               (cm->seq_params.operating_point_idc[op_num] >>
                (cm->spatial_layer_id + 8)) &
                   0x1) ||
              cm->seq_params.operating_point_idc[op_num] == 0) {
            aom_wb_write_literal(
                wb, (uint32_t)cm->op_frame_timing[op_num].buffer_removal_delay,
                cm->buffer_model.buffer_removal_delay_length);
            cm->op_frame_timing[op_num].buffer_removal_delay++;
          }
        }
      }
    }
  }
  cpi->refresh_frame_mask = get_refresh_mask(cpi);
  if (cm->frame_type == KEY_FRAME) {
    if (!cm->show_frame) {  // unshown keyframe (forward keyframe)
      aom_wb_write_literal(wb, cpi->refresh_frame_mask, REF_FRAMES);
    } else {
      assert(cpi->refresh_frame_mask == 0xFF);
    }
  } else {
    if (cm->frame_type == INTRA_ONLY_FRAME) {
      assert(cpi->refresh_frame_mask != 0xFF);
      int updated_fb = -1;
      for (int i = 0; i < REF_FRAMES; i++) {
        // If more than one frame is refreshed, it doesn't matter which one
        // we pick, so pick the first.
        if (cpi->refresh_frame_mask & (1 << i)) {
          updated_fb = i;
          break;
        }
      }
      assert(updated_fb >= 0);
      cm->fb_of_context_type[cm->frame_context_idx] = updated_fb;
      aom_wb_write_literal(wb, cpi->refresh_frame_mask, REF_FRAMES);
    } else if (cm->frame_type == INTER_FRAME || frame_is_sframe(cm)) {
      if (cm->frame_type == INTER_FRAME) {
        aom_wb_write_literal(wb, cpi->refresh_frame_mask, REF_FRAMES);
      } else {
        assert(frame_is_sframe(cm) && cpi->refresh_frame_mask == 0xFF);
      }
      int updated_fb = -1;
      for (int i = 0; i < REF_FRAMES; i++) {
        // If more than one frame is refreshed, it doesn't matter which one
        // we pick, so pick the first.
        if (cpi->refresh_frame_mask & (1 << i)) {
          updated_fb = i;
          break;
        }
      }
      // large scale tile sometimes won't refresh any fbs
      if (updated_fb >= 0) {
        cm->fb_of_context_type[cm->frame_context_idx] = updated_fb;
      }

      if (!cpi->refresh_frame_mask) {
        // NOTE: "cpi->refresh_frame_mask == 0" indicates that the coded frame
        //       will not be used as a reference
        cm->is_reference_frame = 0;
      }
    }
  }

  if (!frame_is_intra_only(cm) || cpi->refresh_frame_mask != 0xFF) {
    // Write all ref frame order hints if error_resilient_mode == 1
    if (cm->error_resilient_mode && cm->seq_params.enable_order_hint) {
      RefCntBuffer *const frame_bufs = cm->buffer_pool->frame_bufs;
      for (int ref_idx = 0; ref_idx < REF_FRAMES; ref_idx++) {
        // Get buffer index
        const int buf_idx = cm->ref_frame_map[ref_idx];
        assert(buf_idx >= 0 && buf_idx < FRAME_BUFFERS);

        // Write order hint to bit stream
        aom_wb_write_literal(wb, frame_bufs[buf_idx].cur_frame_offset,
                             cm->seq_params.order_hint_bits_minus_1 + 1);
      }
    }
  }

  if (cm->frame_type == KEY_FRAME) {
    write_frame_size(cm, frame_size_override_flag, wb);
    assert(!av1_superres_scaled(cm) || !cm->allow_intrabc);
    if (cm->allow_screen_content_tools && !av1_superres_scaled(cm))
      aom_wb_write_bit(wb, cm->allow_intrabc);
    // all eight fbs are refreshed, pick one that will live long enough
    cm->fb_of_context_type[REGULAR_FRAME] = 0;
  } else {
    if (cm->frame_type == INTRA_ONLY_FRAME) {
      write_frame_size(cm, frame_size_override_flag, wb);
      assert(!av1_superres_scaled(cm) || !cm->allow_intrabc);
      if (cm->allow_screen_content_tools && !av1_superres_scaled(cm))
        aom_wb_write_bit(wb, cm->allow_intrabc);
    } else if (cm->frame_type == INTER_FRAME || frame_is_sframe(cm)) {
      MV_REFERENCE_FRAME ref_frame;

      // NOTE: Error resilient mode turns off frame_refs_short_signaling
      //       automatically.
#define FRAME_REFS_SHORT_SIGNALING 0
#if FRAME_REFS_SHORT_SIGNALING
      cm->frame_refs_short_signaling = cm->seq_params.enable_order_hint;
#endif  // FRAME_REFS_SHORT_SIGNALING

      if (cm->frame_refs_short_signaling) {
        // NOTE(zoeliu@google.com):
        //   An example solution for encoder-side implementation on frame refs
        //   short signaling, which is only turned on when the encoder side
        //   decision on ref frames is identical to that at the decoder side.
        check_frame_refs_short_signaling(cpi);
      }

      if (cm->seq_params.enable_order_hint)
        aom_wb_write_bit(wb, cm->frame_refs_short_signaling);

      if (cm->frame_refs_short_signaling) {
        const int lst_ref = get_ref_frame_map_idx(cpi, LAST_FRAME);
        aom_wb_write_literal(wb, lst_ref, REF_FRAMES_LOG2);

        const int gld_ref = get_ref_frame_map_idx(cpi, GOLDEN_FRAME);
        aom_wb_write_literal(wb, gld_ref, REF_FRAMES_LOG2);
      }

      for (ref_frame = LAST_FRAME; ref_frame <= ALTREF_FRAME; ++ref_frame) {
        assert(get_ref_frame_map_idx(cpi, ref_frame) != INVALID_IDX);
        if (!cm->frame_refs_short_signaling)
          aom_wb_write_literal(wb, get_ref_frame_map_idx(cpi, ref_frame),
                               REF_FRAMES_LOG2);
        if (cm->seq_params.frame_id_numbers_present_flag) {
          int i = get_ref_frame_map_idx(cpi, ref_frame);
          int frame_id_len = cm->seq_params.frame_id_length;
          int diff_len = cm->seq_params.delta_frame_id_length;
          int delta_frame_id_minus_1 =
              ((cm->current_frame_id - cm->ref_frame_id[i] +
                (1 << frame_id_len)) %
               (1 << frame_id_len)) -
              1;
          if (delta_frame_id_minus_1 < 0 ||
              delta_frame_id_minus_1 >= (1 << diff_len))
            cm->invalid_delta_frame_id_minus_1 = 1;
          aom_wb_write_literal(wb, delta_frame_id_minus_1, diff_len);
        }
      }

      if (!cm->error_resilient_mode && frame_size_override_flag) {
        write_frame_size_with_refs(cpi, wb);
      } else {
        write_frame_size(cm, frame_size_override_flag, wb);
      }

      if (cm->cur_frame_force_integer_mv) {
        cm->allow_high_precision_mv = 0;
      } else {
        aom_wb_write_bit(wb, cm->allow_high_precision_mv);
      }
      fix_interp_filter(cm, cpi->td.counts);
      write_frame_interp_filter(cm->interp_filter, wb);
      aom_wb_write_bit(wb, cm->switchable_motion_mode);
      if (frame_might_allow_ref_frame_mvs(cm)) {
        aom_wb_write_bit(wb, cm->allow_ref_frame_mvs);
      } else {
        assert(cm->allow_ref_frame_mvs == 0);
      }
    }
  }

  const int might_bwd_adapt =
      !(cm->seq_params.reduced_still_picture_hdr) && !(cm->disable_cdf_update);
  if (cm->large_scale_tile)
    cm->refresh_frame_context = REFRESH_FRAME_CONTEXT_DISABLED;

  if (might_bwd_adapt) {
    aom_wb_write_bit(
        wb, cm->refresh_frame_context == REFRESH_FRAME_CONTEXT_DISABLED);
  }

  write_tile_info(cm, saved_wb, wb);
  encode_quantization(cm, wb);
  encode_segmentation(cm, xd, wb);

  if (cm->delta_q_present_flag) assert(cm->base_qindex > 0);
  if (cm->base_qindex > 0) {
    aom_wb_write_bit(wb, cm->delta_q_present_flag);
    if (cm->delta_q_present_flag) {
      aom_wb_write_literal(wb, OD_ILOG_NZ(cm->delta_q_res) - 1, 2);
      xd->current_qindex = cm->base_qindex;
      if (cm->allow_intrabc)
        assert(cm->delta_lf_present_flag == 0);
      else
        aom_wb_write_bit(wb, cm->delta_lf_present_flag);
      if (cm->delta_lf_present_flag) {
        aom_wb_write_literal(wb, OD_ILOG_NZ(cm->delta_lf_res) - 1, 2);
        aom_wb_write_bit(wb, cm->delta_lf_multi);
        av1_reset_loop_filter_delta(xd, av1_num_planes(cm));
      }
    }
  }

  if (cm->all_lossless) {
    assert(!av1_superres_scaled(cm));
  } else {
    if (!cm->coded_lossless) {
      encode_loopfilter(cm, wb);
      encode_cdef(cm, wb);
    }
    encode_restoration_mode(cm, wb);
  }

  write_tx_mode(cm, &cm->tx_mode, wb);

  if (cpi->allow_comp_inter_inter) {
    const int use_hybrid_pred = cm->reference_mode == REFERENCE_MODE_SELECT;

    aom_wb_write_bit(wb, use_hybrid_pred);
  }

  if (cm->is_skip_mode_allowed) aom_wb_write_bit(wb, cm->skip_mode_flag);

  if (frame_might_allow_warped_motion(cm))
    aom_wb_write_bit(wb, cm->allow_warped_motion);
  else
    assert(!cm->allow_warped_motion);

  aom_wb_write_bit(wb, cm->reduced_tx_set_used);

  if (!frame_is_intra_only(cm)) write_global_motion(cpi, wb);

  if (cm->film_grain_params_present && (cm->show_frame || cm->showable_frame)) {
    int flip_back_update_parameters_flag = 0;
    if (cm->frame_type != INTER_FRAME &&
        cm->film_grain_params.update_parameters == 0) {
      cm->film_grain_params.update_parameters = 1;
      flip_back_update_parameters_flag = 1;
    }
    write_film_grain_params(cpi, wb);

    if (flip_back_update_parameters_flag)
      cm->film_grain_params.update_parameters = 0;
  }

  if (cm->large_scale_tile) write_ext_tile_info(cm, saved_wb, wb);
}

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

  if (cm->large_scale_tile) {
    // The top bit in the tile size field indicates tile copy mode, so we
    // have 1 less bit to code the tile size
    tsb = choose_size_bytes(max_tile_size, 1);
    tcsb = choose_size_bytes(max_tile_col_size, 0);
  } else {
    tsb = choose_size_bytes(max_tile_size, 0);
    tcsb = 4;  // This is ignored
    (void)max_tile_col_size;
  }

  assert(tsb > 0);
  assert(tcsb > 0);

  *tile_size_bytes = tsb;
  *tile_col_size_bytes = tcsb;
  if (tsb == 4 && tcsb == 4) return data_size;

  uint32_t wpos = 0;
  uint32_t rpos = 0;

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

          tile_header += AV1_MIN_TILE_SIZE_BYTES;
          memmove(dst + wpos, dst + rpos, tile_header);
          rpos += tile_header;
          wpos += tile_header;
        }
      }
    }

    assert(rpos > wpos);
    assert(rpos == data_size);

    return wpos;
  }
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
      tile_size += AV1_MIN_TILE_SIZE_BYTES;
      wpos += tsb;
    }

    memmove(dst + wpos, dst + rpos, tile_size);

    rpos += tile_size;
    wpos += tile_size;
  }

  assert(rpos > wpos);
  assert(rpos == data_size);

  return wpos;
}

uint32_t write_obu_header(OBU_TYPE obu_type, int obu_extension,
                          uint8_t *const dst) {
  struct aom_write_bit_buffer wb = { dst, 0 };
  uint32_t size = 0;

  aom_wb_write_literal(&wb, 0, 1);  // forbidden bit.
  aom_wb_write_literal(&wb, (int)obu_type, 4);
  aom_wb_write_literal(&wb, obu_extension ? 1 : 0, 1);
  aom_wb_write_literal(&wb, 1, 1);  // obu_has_payload_length_field
  aom_wb_write_literal(&wb, 0, 1);  // reserved

  if (obu_extension) {
    aom_wb_write_literal(&wb, obu_extension & 0xFF, 8);
  }

  size = aom_wb_bytes_written(&wb);
  return size;
}

int write_uleb_obu_size(uint32_t obu_header_size, uint32_t obu_payload_size,
                        uint8_t *dest) {
  const uint32_t obu_size = obu_payload_size;
  const uint32_t offset = obu_header_size;
  size_t coded_obu_size = 0;

  if (aom_uleb_encode(obu_size, sizeof(obu_size), dest + offset,
                      &coded_obu_size) != 0) {
    return AOM_CODEC_ERROR;
  }

  return AOM_CODEC_OK;
}

static size_t obu_memmove(uint32_t obu_header_size, uint32_t obu_payload_size,
                          uint8_t *data) {
  const size_t length_field_size = aom_uleb_size_in_bytes(obu_payload_size);
  const uint32_t move_dst_offset =
      (uint32_t)length_field_size + obu_header_size;
  const uint32_t move_src_offset = obu_header_size;
  const uint32_t move_size = obu_payload_size;
  memmove(data + move_dst_offset, data + move_src_offset, move_size);
  return length_field_size;
}

static void add_trailing_bits(struct aom_write_bit_buffer *wb) {
  if (aom_wb_is_byte_aligned(wb)) {
    aom_wb_write_literal(wb, 0x80, 8);
  } else {
    // assumes that the other bits are already 0s
    aom_wb_write_bit(wb, 1);
  }
}

static void write_bitstream_level(BitstreamLevel bl,
                                  struct aom_write_bit_buffer *wb) {
  uint8_t seq_level_idx = major_minor_to_seq_level_idx(bl);
  assert(is_valid_seq_level_idx(seq_level_idx));
  aom_wb_write_literal(wb, seq_level_idx, LEVEL_BITS);
}

static uint32_t write_sequence_header_obu(AV1_COMP *cpi, uint8_t *const dst) {
  AV1_COMMON *const cm = &cpi->common;
  struct aom_write_bit_buffer wb = { dst, 0 };
  uint32_t size = 0;

  write_profile(cm->profile, &wb);

  // Still picture or not
  aom_wb_write_bit(&wb, cm->seq_params.still_picture);
  assert(IMPLIES(!cm->seq_params.still_picture,
                 !cm->seq_params.reduced_still_picture_hdr));
  // whether to use reduced still picture header
  aom_wb_write_bit(&wb, cm->seq_params.reduced_still_picture_hdr);

  if (cm->seq_params.reduced_still_picture_hdr) {
    assert(cm->timing_info_present == 0);
    assert(cm->seq_params.decoder_model_info_present_flag == 0);
    assert(cm->seq_params.display_model_info_present_flag == 0);
    write_bitstream_level(cm->seq_params.level[0], &wb);
  } else {
    aom_wb_write_bit(&wb, cm->timing_info_present);  // timing info present flag

    if (cm->timing_info_present) {
      // timing_info
      write_timing_info_header(cm, &wb);
      aom_wb_write_bit(&wb, cm->seq_params.decoder_model_info_present_flag);
      if (cm->seq_params.decoder_model_info_present_flag) {
        write_decoder_model_info(cm, &wb);
      }
    }
    aom_wb_write_bit(&wb, cm->seq_params.display_model_info_present_flag);
    aom_wb_write_literal(&wb, cm->seq_params.operating_points_cnt_minus_1,
                         OP_POINTS_CNT_MINUS_1_BITS);
    int i;
    for (i = 0; i < cm->seq_params.operating_points_cnt_minus_1 + 1; i++) {
      aom_wb_write_literal(&wb, cm->seq_params.operating_point_idc[i],
                           OP_POINTS_IDC_BITS);
      write_bitstream_level(cm->seq_params.level[i], &wb);
      if (cm->seq_params.level[i].major > 3)
        aom_wb_write_bit(&wb, cm->seq_params.tier[i]);
      if (cm->seq_params.decoder_model_info_present_flag) {
        aom_wb_write_bit(&wb,
                         cm->op_params[i].decoder_model_param_present_flag);
        if (cm->op_params[i].decoder_model_param_present_flag)
          write_dec_model_op_parameters(cm, &wb, i);
      }
      if (cm->seq_params.display_model_info_present_flag) {
        aom_wb_write_bit(&wb,
                         cm->op_params[i].display_model_param_present_flag);
        if (cm->op_params[i].display_model_param_present_flag) {
          assert(cm->op_params[i].initial_display_delay <= 10);
          aom_wb_write_literal(&wb, cm->op_params[i].initial_display_delay - 1,
                               4);
        }
      }
    }
  }
  write_sequence_header(cpi, &wb);

  write_color_config(cm, &wb);

  aom_wb_write_bit(&wb, cm->film_grain_params_present);

  add_trailing_bits(&wb);

  size = aom_wb_bytes_written(&wb);
  return size;
}

static uint32_t write_frame_header_obu(AV1_COMP *cpi,
                                       struct aom_write_bit_buffer *saved_wb,
                                       uint8_t *const dst,
                                       int append_trailing_bits) {
  struct aom_write_bit_buffer wb = { dst, 0 };
  write_uncompressed_header_obu(cpi, saved_wb, &wb);
  if (append_trailing_bits) add_trailing_bits(&wb);
  return aom_wb_bytes_written(&wb);
}

static uint32_t write_tile_group_header(uint8_t *const dst, int startTile,
                                        int endTile, int tiles_log2,
                                        int tile_start_and_end_present_flag) {
  struct aom_write_bit_buffer wb = { dst, 0 };
  uint32_t size = 0;

  if (!tiles_log2) return size;

  aom_wb_write_bit(&wb, tile_start_and_end_present_flag);

  if (tile_start_and_end_present_flag) {
    aom_wb_write_literal(&wb, startTile, tiles_log2);
    aom_wb_write_literal(&wb, endTile, tiles_log2);
  }

  size = aom_wb_bytes_written(&wb);
  return size;
}

typedef struct {
  uint8_t *frame_header;
  size_t obu_header_byte_offset;
  size_t total_length;
} FrameHeaderInfo;

static uint32_t write_tiles_in_tg_obus(AV1_COMP *const cpi, uint8_t *const dst,
                                       struct aom_write_bit_buffer *saved_wb,
                                       uint8_t obu_extension_header,
                                       const FrameHeaderInfo *fh_info) {
  AV1_COMMON *const cm = &cpi->common;
  aom_writer mode_bc;
  int tile_row, tile_col;
  TOKENEXTRA *(*const tok_buffers)[MAX_TILE_COLS] = cpi->tile_tok;
  TileBufferEnc(*const tile_buffers)[MAX_TILE_COLS] = cpi->tile_buffers;
  uint32_t total_size = 0;
  const int tile_cols = cm->tile_cols;
  const int tile_rows = cm->tile_rows;
  unsigned int tile_size = 0;
  unsigned int max_tile_size = 0;
  unsigned int max_tile_col_size = 0;
  const int n_log2_tiles = cm->log2_tile_rows + cm->log2_tile_cols;
  // Fixed size tile groups for the moment
  const int num_tg_hdrs = cm->num_tg;
  const int tg_size =
      (cm->large_scale_tile)
          ? 1
          : (tile_rows * tile_cols + num_tg_hdrs - 1) / num_tg_hdrs;
  int tile_count = 0;
  int curr_tg_data_size = 0;
  uint8_t *data = dst;
  int new_tg = 1;
  const int have_tiles = tile_cols * tile_rows > 1;
  int first_tg = 1;

  cm->largest_tile_id = 0;

  if (cm->large_scale_tile) {
    // For large_scale_tile case, we always have only one tile group, so it can
    // be written as an OBU_FRAME.
    const OBU_TYPE obu_type = OBU_FRAME;
    const uint32_t tg_hdr_size = write_obu_header(obu_type, 0, data);
    data += tg_hdr_size;

    const uint32_t frame_header_size =
        write_frame_header_obu(cpi, saved_wb, data, 0);
    data += frame_header_size;
    total_size += frame_header_size;

#define EXT_TILE_DEBUG 0
#if EXT_TILE_DEBUG
    {
      char fn[20] = "./fh";
      fn[4] = cm->current_video_frame / 100 + '0';
      fn[5] = (cm->current_video_frame % 100) / 10 + '0';
      fn[6] = (cm->current_video_frame % 10) + '0';
      fn[7] = '\0';
      av1_print_uncompressed_frame_header(data - frame_header_size,
                                          frame_header_size, fn);
    }
#endif  // EXT_TILE_DEBUG
#undef EXT_TILE_DEBUG

    int tile_size_bytes = 0;
    int tile_col_size_bytes = 0;

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

        buf->data = dst + total_size + tg_hdr_size;

        // Is CONFIG_EXT_TILE = 1, every tile in the row has a header,
        // even for the last one, unless no tiling is used at all.
        total_size += data_offset;
        // Initialise tile context from the frame context
        this_tile->tctx = *cm->fc;
        cpi->td.mb.e_mbd.tile_ctx = &this_tile->tctx;
        mode_bc.allow_update_cdf = !cm->large_scale_tile;
        mode_bc.allow_update_cdf =
            mode_bc.allow_update_cdf && !cm->disable_cdf_update;
        aom_start_encode(&mode_bc, buf->data + data_offset);
        write_modes(cpi, &tile_info, &mode_bc, &tok, tok_end);
        assert(tok == tok_end);
        aom_stop_encode(&mode_bc);
        tile_size = mode_bc.pos;
        buf->size = tile_size;

        // Record the maximum tile size we see, so we can compact headers later.
        if (tile_size > max_tile_size) {
          max_tile_size = tile_size;
          cm->largest_tile_id = tile_cols * tile_row + tile_col;
        }

        if (have_tiles) {
          // tile header: size of this tile, or copy offset
          uint32_t tile_header = tile_size - AV1_MIN_TILE_SIZE_BYTES;
          const int tile_copy_mode =
              ((AOMMAX(cm->tile_width, cm->tile_height) << MI_SIZE_LOG2) <= 256)
                  ? 1
                  : 0;

          // If tile_copy_mode = 1, check if this tile is a copy tile.
          // Very low chances to have copy tiles on the key frames, so don't
          // search on key frames to reduce unnecessary search.
          if (cm->frame_type != KEY_FRAME && tile_copy_mode) {
            const int identical_tile_offset =
                find_identical_tile(tile_row, tile_col, tile_buffers);

            if (identical_tile_offset > 0) {
              tile_size = 0;
              tile_header = identical_tile_offset | 0x80;
              tile_header <<= 24;
            }
          }

          mem_put_le32(buf->data, tile_header);
        }

        total_size += tile_size;
      }

      if (!is_last_col) {
        uint32_t col_size = total_size - col_offset - 4;
        mem_put_le32(dst + col_offset + tg_hdr_size, col_size);

        // Record the maximum tile column size we see.
        max_tile_col_size = AOMMAX(max_tile_col_size, col_size);
      }
    }

    if (have_tiles) {
      total_size = remux_tiles(cm, data, total_size - frame_header_size,
                               max_tile_size, max_tile_col_size,
                               &tile_size_bytes, &tile_col_size_bytes);
      total_size += frame_header_size;
    }

    // In EXT_TILE case, only use 1 tile group. Follow the obu syntax, write
    // current tile group size before tile data(include tile column header).
    // Tile group size doesn't include the bytes storing tg size.
    total_size += tg_hdr_size;
    const uint32_t obu_payload_size = total_size - tg_hdr_size;
    const size_t length_field_size =
        obu_memmove(tg_hdr_size, obu_payload_size, dst);
    if (write_uleb_obu_size(tg_hdr_size, obu_payload_size, dst) !=
        AOM_CODEC_OK) {
      assert(0);
    }
    total_size += (uint32_t)length_field_size;
    saved_wb->bit_buffer += length_field_size;

    // Now fill in the gaps in the uncompressed header.
    if (have_tiles) {
      assert(tile_col_size_bytes >= 1 && tile_col_size_bytes <= 4);
      aom_wb_overwrite_literal(saved_wb, tile_col_size_bytes - 1, 2);

      assert(tile_size_bytes >= 1 && tile_size_bytes <= 4);
      aom_wb_overwrite_literal(saved_wb, tile_size_bytes - 1, 2);
    }
    return total_size;
  }

  uint32_t obu_header_size = 0;
  uint8_t *tile_data_start = dst + total_size;
  for (tile_row = 0; tile_row < tile_rows; tile_row++) {
    TileInfo tile_info;
    av1_tile_set_row(&tile_info, cm, tile_row);

    for (tile_col = 0; tile_col < tile_cols; tile_col++) {
      const int tile_idx = tile_row * tile_cols + tile_col;
      TileBufferEnc *const buf = &tile_buffers[tile_row][tile_col];
      TileDataEnc *this_tile = &cpi->tile_data[tile_idx];
      const TOKENEXTRA *tok = tok_buffers[tile_row][tile_col];
      const TOKENEXTRA *tok_end = tok + cpi->tok_count[tile_row][tile_col];
      int is_last_tile_in_tg = 0;

      if (new_tg) {
        data = dst + total_size;

        // A new tile group begins at this tile.  Write the obu header and
        // tile group header
        const OBU_TYPE obu_type =
            (num_tg_hdrs == 1) ? OBU_FRAME : OBU_TILE_GROUP;
        curr_tg_data_size =
            write_obu_header(obu_type, obu_extension_header, data);
        obu_header_size = curr_tg_data_size;

        if (num_tg_hdrs == 1) {
          curr_tg_data_size += write_frame_header_obu(
              cpi, saved_wb, data + curr_tg_data_size, 0);
        }
        curr_tg_data_size += write_tile_group_header(
            data + curr_tg_data_size, tile_idx,
            AOMMIN(tile_idx + tg_size - 1, tile_cols * tile_rows - 1),
            n_log2_tiles, cm->num_tg > 1);
        total_size += curr_tg_data_size;
        tile_data_start += curr_tg_data_size;
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

      buf->data = dst + total_size;

      // The last tile of the tile group does not have a header.
      if (!is_last_tile_in_tg) total_size += 4;

      // Initialise tile context from the frame context
      this_tile->tctx = *cm->fc;
      cpi->td.mb.e_mbd.tile_ctx = &this_tile->tctx;
      mode_bc.allow_update_cdf = 1;
      mode_bc.allow_update_cdf =
          mode_bc.allow_update_cdf && !cm->disable_cdf_update;
      const int num_planes = av1_num_planes(cm);
      av1_reset_loop_restoration(&cpi->td.mb.e_mbd, num_planes);

      aom_start_encode(&mode_bc, dst + total_size);
      write_modes(cpi, &tile_info, &mode_bc, &tok, tok_end);
      aom_stop_encode(&mode_bc);
      tile_size = mode_bc.pos;
      assert(tile_size >= AV1_MIN_TILE_SIZE_BYTES);

      curr_tg_data_size += (tile_size + (is_last_tile_in_tg ? 0 : 4));
      buf->size = tile_size;
      if (tile_size > max_tile_size) {
        cm->largest_tile_id = tile_cols * tile_row + tile_col;
        max_tile_size = tile_size;
      }

      if (!is_last_tile_in_tg) {
        // size of this tile
        mem_put_le32(buf->data, tile_size - AV1_MIN_TILE_SIZE_BYTES);
      } else {
        // write current tile group size
        const uint32_t obu_payload_size = curr_tg_data_size - obu_header_size;
        const size_t length_field_size =
            obu_memmove(obu_header_size, obu_payload_size, data);
        if (write_uleb_obu_size(obu_header_size, obu_payload_size, data) !=
            AOM_CODEC_OK) {
          assert(0);
        }
        curr_tg_data_size += (int)length_field_size;
        total_size += (uint32_t)length_field_size;
        tile_data_start += length_field_size;
        if (num_tg_hdrs == 1) {
          // if this tg is combined with the frame header then update saved
          // frame header base offset accroding to length field size
          saved_wb->bit_buffer += length_field_size;
        }

        if (!first_tg && cm->error_resilient_mode) {
          // Make room for a duplicate Frame Header OBU.
          memmove(data + fh_info->total_length, data, curr_tg_data_size);

          // Insert a copy of the Frame Header OBU.
          memcpy(data, fh_info->frame_header, fh_info->total_length);

          // Force context update tile to be the first tile in error
          // resiliant mode as the duplicate frame headers will have
          // context_update_tile_id set to 0
          cm->largest_tile_id = 0;

          // Rewrite the OBU header to change the OBU type to Redundant Frame
          // Header.
          write_obu_header(OBU_REDUNDANT_FRAME_HEADER, obu_extension_header,
                           &data[fh_info->obu_header_byte_offset]);

          data += fh_info->total_length;

          curr_tg_data_size += (int)(fh_info->total_length);
          total_size += (uint32_t)(fh_info->total_length);
        }
        first_tg = 0;
      }

      total_size += tile_size;
    }
  }

  if (have_tiles) {
    // Fill in context_update_tile_id indicating the tile to use for the
    // cdf update. The encoder currently sets it to the largest tile
    // (but is up to the encoder)
    aom_wb_overwrite_literal(saved_wb, cm->largest_tile_id,
                             cm->log2_tile_cols + cm->log2_tile_rows);
    // If more than one tile group. tile_size_bytes takes the default value 4
    // and does not need to be set. For a single tile group it is set in the
    // section below.
    if (num_tg_hdrs == 1) {
      int tile_size_bytes = 4, unused;
      const uint32_t tile_data_offset = (uint32_t)(tile_data_start - dst);
      const uint32_t tile_data_size = total_size - tile_data_offset;

      total_size =
          remux_tiles(cm, tile_data_start, tile_data_size, max_tile_size,
                      max_tile_col_size, &tile_size_bytes, &unused);
      total_size += tile_data_offset;
      assert(tile_size_bytes >= 1 && tile_size_bytes <= 4);

      aom_wb_overwrite_literal(saved_wb, tile_size_bytes - 1, 2);

      // Update the OBU length if remux_tiles() reduced the size.
      uint64_t payload_size;
      size_t length_field_size;
      int res =
          aom_uleb_decode(dst + obu_header_size, total_size - obu_header_size,
                          &payload_size, &length_field_size);
      assert(res == 0);
      (void)res;

      const uint64_t new_payload_size =
          total_size - obu_header_size - length_field_size;
      if (new_payload_size != payload_size) {
        size_t new_length_field_size;
        res = aom_uleb_encode(new_payload_size, length_field_size,
                              dst + obu_header_size, &new_length_field_size);
        assert(res == 0);
        if (new_length_field_size < length_field_size) {
          const size_t src_offset = obu_header_size + length_field_size;
          const size_t dst_offset = obu_header_size + new_length_field_size;
          memmove(dst + dst_offset, dst + src_offset, (size_t)payload_size);
          total_size -= (int)(length_field_size - new_length_field_size);
        }
      }
    }
  }
  return total_size;
}

int av1_pack_bitstream(AV1_COMP *const cpi, uint8_t *dst, size_t *size) {
  uint8_t *data = dst;
  uint32_t data_size;
  AV1_COMMON *const cm = &cpi->common;
  uint32_t obu_header_size = 0;
  uint32_t obu_payload_size = 0;
  FrameHeaderInfo fh_info = { NULL, 0, 0 };
  const uint8_t obu_extension_header =
      cm->temporal_layer_id << 5 | cm->spatial_layer_id << 3 | 0;

#if CONFIG_BITSTREAM_DEBUG
  bitstream_queue_reset_write();
#endif

  // The TD is now written outside the frame encode loop

  // write sequence header obu if KEY_FRAME, preceded by 4-byte size
  if (cm->frame_type == KEY_FRAME) {
    obu_header_size = write_obu_header(OBU_SEQUENCE_HEADER, 0, data);

    obu_payload_size = write_sequence_header_obu(cpi, data + obu_header_size);
    const size_t length_field_size =
        obu_memmove(obu_header_size, obu_payload_size, data);
    if (write_uleb_obu_size(obu_header_size, obu_payload_size, data) !=
        AOM_CODEC_OK) {
      return AOM_CODEC_ERROR;
    }

    data += obu_header_size + obu_payload_size + length_field_size;
  }

  const int write_frame_header = (cm->num_tg > 1 || cm->show_existing_frame);
  struct aom_write_bit_buffer saved_wb;
  if (write_frame_header) {
    // Write Frame Header OBU.
    fh_info.frame_header = data;
    obu_header_size =
        write_obu_header(OBU_FRAME_HEADER, obu_extension_header, data);
    obu_payload_size =
        write_frame_header_obu(cpi, &saved_wb, data + obu_header_size, 1);

    const size_t length_field_size =
        obu_memmove(obu_header_size, obu_payload_size, data);
    if (write_uleb_obu_size(obu_header_size, obu_payload_size, data) !=
        AOM_CODEC_OK) {
      return AOM_CODEC_ERROR;
    }

    fh_info.obu_header_byte_offset = 0;
    fh_info.total_length =
        obu_header_size + obu_payload_size + length_field_size;
    data += fh_info.total_length;

    // Since length_field_size is determined adaptively after frame header
    // encoding, saved_wb must be adjusted accordingly.
    saved_wb.bit_buffer += length_field_size;
  }

  if (cm->show_existing_frame) {
    data_size = 0;
  } else {
    //  Each tile group obu will be preceded by 4-byte size of the tile group
    //  obu
    data_size = write_tiles_in_tg_obus(cpi, data, &saved_wb,
                                       obu_extension_header, &fh_info);
  }
  data += data_size;
  *size = data - dst;
  return AOM_CODEC_OK;
}
