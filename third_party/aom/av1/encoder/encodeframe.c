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

#include <limits.h>
#include <math.h>
#include <stdio.h>

#include "config/aom_config.h"
#include "config/aom_dsp_rtcd.h"
#include "config/av1_rtcd.h"

#include "aom_dsp/aom_dsp_common.h"
#include "aom_dsp/binary_codes_writer.h"
#include "aom_ports/mem.h"
#include "aom_ports/aom_timer.h"
#include "aom_ports/system_state.h"

#if CONFIG_MISMATCH_DEBUG
#include "aom_util/debug_util.h"
#endif  // CONFIG_MISMATCH_DEBUG

#include "av1/common/cfl.h"
#include "av1/common/common.h"
#include "av1/common/entropy.h"
#include "av1/common/entropymode.h"
#include "av1/common/idct.h"
#include "av1/common/mv.h"
#include "av1/common/mvref_common.h"
#include "av1/common/pred_common.h"
#include "av1/common/quant_common.h"
#include "av1/common/reconintra.h"
#include "av1/common/reconinter.h"
#include "av1/common/seg_common.h"
#include "av1/common/tile_common.h"

#include "av1/encoder/ab_partition_model_weights.h"
#include "av1/encoder/aq_complexity.h"
#include "av1/encoder/aq_cyclicrefresh.h"
#include "av1/encoder/aq_variance.h"
#include "av1/common/warped_motion.h"
#include "av1/encoder/global_motion.h"
#include "av1/encoder/encodeframe.h"
#include "av1/encoder/encodemb.h"
#include "av1/encoder/encodemv.h"
#include "av1/encoder/encodetxb.h"
#include "av1/encoder/ethread.h"
#include "av1/encoder/extend.h"
#include "av1/encoder/ml.h"
#include "av1/encoder/rd.h"
#include "av1/encoder/rdopt.h"
#include "av1/encoder/segmentation.h"
#include "av1/encoder/tokenize.h"

static void encode_superblock(const AV1_COMP *const cpi, TileDataEnc *tile_data,
                              ThreadData *td, TOKENEXTRA **t, RUN_TYPE dry_run,
                              int mi_row, int mi_col, BLOCK_SIZE bsize,
                              int *rate);

// This is used as a reference when computing the source variance for the
//  purposes of activity masking.
// Eventually this should be replaced by custom no-reference routines,
//  which will be faster.
static const uint8_t AV1_VAR_OFFS[MAX_SB_SIZE] = {
  128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
  128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
  128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
  128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
  128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
  128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
  128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
  128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
  128, 128, 128, 128, 128, 128, 128, 128
};

static const uint16_t AV1_HIGH_VAR_OFFS_8[MAX_SB_SIZE] = {
  128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
  128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
  128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
  128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
  128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
  128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
  128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
  128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
  128, 128, 128, 128, 128, 128, 128, 128
};

static const uint16_t AV1_HIGH_VAR_OFFS_10[MAX_SB_SIZE] = {
  128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4,
  128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4,
  128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4,
  128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4,
  128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4,
  128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4,
  128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4,
  128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4,
  128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4,
  128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4,
  128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4,
  128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4,
  128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4,
  128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4,
  128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4,
  128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4
};

static const uint16_t AV1_HIGH_VAR_OFFS_12[MAX_SB_SIZE] = {
  128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16,
  128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16,
  128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16,
  128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16,
  128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16,
  128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16,
  128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16,
  128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16,
  128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16,
  128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16,
  128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16,
  128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16,
  128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16,
  128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16,
  128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16,
  128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16,
  128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16,
  128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16,
  128 * 16, 128 * 16
};

#if CONFIG_FP_MB_STATS
static const uint8_t num_16x16_blocks_wide_lookup[BLOCK_SIZES_ALL] = {
  1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 4, 4, 4, 8, 8, 1, 1, 1, 2, 2, 4
};
static const uint8_t num_16x16_blocks_high_lookup[BLOCK_SIZES_ALL] = {
  1, 1, 1, 1, 1, 1, 1, 2, 1, 2, 4, 2, 4, 8, 4, 8, 1, 1, 2, 1, 4, 2
};
#endif  // CONFIG_FP_MB_STATS

unsigned int av1_get_sby_perpixel_variance(const AV1_COMP *cpi,
                                           const struct buf_2d *ref,
                                           BLOCK_SIZE bs) {
  unsigned int sse;
  const unsigned int var =
      cpi->fn_ptr[bs].vf(ref->buf, ref->stride, AV1_VAR_OFFS, 0, &sse);
  return ROUND_POWER_OF_TWO(var, num_pels_log2_lookup[bs]);
}

unsigned int av1_high_get_sby_perpixel_variance(const AV1_COMP *cpi,
                                                const struct buf_2d *ref,
                                                BLOCK_SIZE bs, int bd) {
  unsigned int var, sse;
  switch (bd) {
    case 10:
      var =
          cpi->fn_ptr[bs].vf(ref->buf, ref->stride,
                             CONVERT_TO_BYTEPTR(AV1_HIGH_VAR_OFFS_10), 0, &sse);
      break;
    case 12:
      var =
          cpi->fn_ptr[bs].vf(ref->buf, ref->stride,
                             CONVERT_TO_BYTEPTR(AV1_HIGH_VAR_OFFS_12), 0, &sse);
      break;
    case 8:
    default:
      var =
          cpi->fn_ptr[bs].vf(ref->buf, ref->stride,
                             CONVERT_TO_BYTEPTR(AV1_HIGH_VAR_OFFS_8), 0, &sse);
      break;
  }
  return ROUND_POWER_OF_TWO(var, num_pels_log2_lookup[bs]);
}

static unsigned int get_sby_perpixel_diff_variance(const AV1_COMP *const cpi,
                                                   const struct buf_2d *ref,
                                                   int mi_row, int mi_col,
                                                   BLOCK_SIZE bs) {
  unsigned int sse, var;
  uint8_t *last_y;
  const YV12_BUFFER_CONFIG *last = get_ref_frame_buffer(cpi, LAST_FRAME);

  assert(last != NULL);
  last_y =
      &last->y_buffer[mi_row * MI_SIZE * last->y_stride + mi_col * MI_SIZE];
  var = cpi->fn_ptr[bs].vf(ref->buf, ref->stride, last_y, last->y_stride, &sse);
  return ROUND_POWER_OF_TWO(var, num_pels_log2_lookup[bs]);
}

static BLOCK_SIZE get_rd_var_based_fixed_partition(AV1_COMP *cpi, MACROBLOCK *x,
                                                   int mi_row, int mi_col) {
  unsigned int var = get_sby_perpixel_diff_variance(
      cpi, &x->plane[0].src, mi_row, mi_col, BLOCK_64X64);
  if (var < 8)
    return BLOCK_64X64;
  else if (var < 128)
    return BLOCK_32X32;
  else if (var < 2048)
    return BLOCK_16X16;
  else
    return BLOCK_8X8;
}

// Lighter version of set_offsets that only sets the mode info
// pointers.
static void set_mode_info_offsets(const AV1_COMP *const cpi,
                                  MACROBLOCK *const x, MACROBLOCKD *const xd,
                                  int mi_row, int mi_col) {
  const AV1_COMMON *const cm = &cpi->common;
  const int idx_str = xd->mi_stride * mi_row + mi_col;
  xd->mi = cm->mi_grid_visible + idx_str;
  xd->mi[0] = cm->mi + idx_str;
  x->mbmi_ext = cpi->mbmi_ext_base + (mi_row * cm->mi_cols + mi_col);
}

static void set_offsets_without_segment_id(const AV1_COMP *const cpi,
                                           const TileInfo *const tile,
                                           MACROBLOCK *const x, int mi_row,
                                           int mi_col, BLOCK_SIZE bsize) {
  const AV1_COMMON *const cm = &cpi->common;
  const int num_planes = av1_num_planes(cm);
  MACROBLOCKD *const xd = &x->e_mbd;
  const int mi_width = mi_size_wide[bsize];
  const int mi_height = mi_size_high[bsize];

  set_mode_info_offsets(cpi, x, xd, mi_row, mi_col);

  set_skip_context(xd, mi_row, mi_col, num_planes);
  xd->above_txfm_context = cm->above_txfm_context[tile->tile_row] + mi_col;
  xd->left_txfm_context =
      xd->left_txfm_context_buffer + (mi_row & MAX_MIB_MASK);

  // Set up destination pointers.
  av1_setup_dst_planes(xd->plane, bsize, get_frame_new_buffer(cm), mi_row,
                       mi_col, 0, num_planes);

  // Set up limit values for MV components.
  // Mv beyond the range do not produce new/different prediction block.
  x->mv_limits.row_min =
      -(((mi_row + mi_height) * MI_SIZE) + AOM_INTERP_EXTEND);
  x->mv_limits.col_min = -(((mi_col + mi_width) * MI_SIZE) + AOM_INTERP_EXTEND);
  x->mv_limits.row_max = (cm->mi_rows - mi_row) * MI_SIZE + AOM_INTERP_EXTEND;
  x->mv_limits.col_max = (cm->mi_cols - mi_col) * MI_SIZE + AOM_INTERP_EXTEND;

  set_plane_n4(xd, mi_width, mi_height, num_planes);

  // Set up distance of MB to edge of frame in 1/8th pel units.
  assert(!(mi_col & (mi_width - 1)) && !(mi_row & (mi_height - 1)));
  set_mi_row_col(xd, tile, mi_row, mi_height, mi_col, mi_width, cm->mi_rows,
                 cm->mi_cols);

  // Set up source buffers.
  av1_setup_src_planes(x, cpi->source, mi_row, mi_col, num_planes);

  // R/D setup.
  x->rdmult = cpi->rd.RDMULT;

  // required by av1_append_sub8x8_mvs_for_idx() and av1_find_best_ref_mvs()
  xd->tile = *tile;
}

static void set_offsets(const AV1_COMP *const cpi, const TileInfo *const tile,
                        MACROBLOCK *const x, int mi_row, int mi_col,
                        BLOCK_SIZE bsize) {
  const AV1_COMMON *const cm = &cpi->common;
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *mbmi;
  const struct segmentation *const seg = &cm->seg;

  set_offsets_without_segment_id(cpi, tile, x, mi_row, mi_col, bsize);

  mbmi = xd->mi[0];
  xd->cfl.mi_row = mi_row;
  xd->cfl.mi_col = mi_col;

  mbmi->segment_id = 0;

  // Setup segment ID.
  if (seg->enabled) {
    if (seg->enabled && !cpi->vaq_refresh) {
      const uint8_t *const map =
          seg->update_map ? cpi->segmentation_map : cm->last_frame_seg_map;
      mbmi->segment_id =
          map ? get_segment_id(cm, map, bsize, mi_row, mi_col) : 0;
    }
    av1_init_plane_quantizers(cpi, x, mbmi->segment_id);
  }
}

static void reset_intmv_filter_type(MB_MODE_INFO *mbmi) {
  InterpFilter filters[2];

  for (int dir = 0; dir < 2; ++dir) {
    filters[dir] = av1_extract_interp_filter(mbmi->interp_filters, dir);
  }
  mbmi->interp_filters = av1_make_interp_filters(filters[0], filters[1]);
}

static void update_filter_type_count(uint8_t allow_update_cdf,
                                     FRAME_COUNTS *counts,
                                     const MACROBLOCKD *xd,
                                     const MB_MODE_INFO *mbmi) {
  int dir;
  for (dir = 0; dir < 2; ++dir) {
    const int ctx = av1_get_pred_context_switchable_interp(xd, dir);
    InterpFilter filter = av1_extract_interp_filter(mbmi->interp_filters, dir);
    ++counts->switchable_interp[ctx][filter];
    if (allow_update_cdf) {
      update_cdf(xd->tile_ctx->switchable_interp_cdf[ctx], filter,
                 SWITCHABLE_FILTERS);
    }
  }
}

static void update_global_motion_used(PREDICTION_MODE mode, BLOCK_SIZE bsize,
                                      const MB_MODE_INFO *mbmi,
                                      RD_COUNTS *rdc) {
  if (mode == GLOBALMV || mode == GLOBAL_GLOBALMV) {
    const int num_4x4s = mi_size_wide[bsize] * mi_size_high[bsize];
    int ref;
    for (ref = 0; ref < 1 + has_second_ref(mbmi); ++ref) {
      rdc->global_motion_used[mbmi->ref_frame[ref]] += num_4x4s;
    }
  }
}

static void reset_tx_size(MACROBLOCK *x, MB_MODE_INFO *mbmi,
                          const TX_MODE tx_mode) {
  MACROBLOCKD *const xd = &x->e_mbd;
  if (xd->lossless[mbmi->segment_id]) {
    mbmi->tx_size = TX_4X4;
  } else if (tx_mode != TX_MODE_SELECT) {
    mbmi->tx_size = tx_size_from_tx_mode(mbmi->sb_type, tx_mode);
  } else {
    BLOCK_SIZE bsize = mbmi->sb_type;
    TX_SIZE min_tx_size = depth_to_tx_size(MAX_TX_DEPTH, bsize);
    mbmi->tx_size = (TX_SIZE)TXSIZEMAX(mbmi->tx_size, min_tx_size);
  }
  if (is_inter_block(mbmi)) {
    memset(mbmi->inter_tx_size, mbmi->tx_size, sizeof(mbmi->inter_tx_size));
  }
  memset(mbmi->txk_type, DCT_DCT, sizeof(mbmi->txk_type[0]) * TXK_TYPE_BUF_LEN);
  av1_zero(x->blk_skip);
  x->skip = 0;
}

static void update_state(const AV1_COMP *const cpi, TileDataEnc *tile_data,
                         ThreadData *td, PICK_MODE_CONTEXT *ctx, int mi_row,
                         int mi_col, BLOCK_SIZE bsize, RUN_TYPE dry_run) {
  int i, x_idx, y;
  const AV1_COMMON *const cm = &cpi->common;
  const int num_planes = av1_num_planes(cm);
  RD_COUNTS *const rdc = &td->rd_counts;
  MACROBLOCK *const x = &td->mb;
  MACROBLOCKD *const xd = &x->e_mbd;
  struct macroblock_plane *const p = x->plane;
  struct macroblockd_plane *const pd = xd->plane;
  MB_MODE_INFO *mi = &ctx->mic;
  MB_MODE_INFO *const mi_addr = xd->mi[0];
  const struct segmentation *const seg = &cm->seg;
  const int bw = mi_size_wide[mi->sb_type];
  const int bh = mi_size_high[mi->sb_type];
  const int mis = cm->mi_stride;
  const int mi_width = mi_size_wide[bsize];
  const int mi_height = mi_size_high[bsize];

  assert(mi->sb_type == bsize);

  *mi_addr = *mi;
  *x->mbmi_ext = ctx->mbmi_ext;

  reset_intmv_filter_type(mi_addr);

  memcpy(x->blk_skip, ctx->blk_skip, sizeof(x->blk_skip[0]) * ctx->num_4x4_blk);

  x->skip = ctx->skip;

  // If segmentation in use
  if (seg->enabled) {
    // For in frame complexity AQ copy the segment id from the segment map.
    if (cpi->oxcf.aq_mode == COMPLEXITY_AQ) {
      const uint8_t *const map =
          seg->update_map ? cpi->segmentation_map : cm->last_frame_seg_map;
      mi_addr->segment_id =
          map ? get_segment_id(cm, map, bsize, mi_row, mi_col) : 0;
      reset_tx_size(x, mi_addr, cm->tx_mode);
    }
    // Else for cyclic refresh mode update the segment map, set the segment id
    // and then update the quantizer.
    if (cpi->oxcf.aq_mode == CYCLIC_REFRESH_AQ) {
      av1_cyclic_refresh_update_segment(cpi, mi_addr, mi_row, mi_col, bsize,
                                        ctx->rate, ctx->dist, x->skip);
      reset_tx_size(x, mi_addr, cm->tx_mode);
    }
    if (mi_addr->uv_mode == UV_CFL_PRED && !is_cfl_allowed(xd))
      mi_addr->uv_mode = UV_DC_PRED;
  }

  for (i = 0; i < num_planes; ++i) {
    p[i].coeff = ctx->coeff[i];
    p[i].qcoeff = ctx->qcoeff[i];
    pd[i].dqcoeff = ctx->dqcoeff[i];
    p[i].eobs = ctx->eobs[i];
    p[i].txb_entropy_ctx = ctx->txb_entropy_ctx[i];
  }
  for (i = 0; i < 2; ++i) pd[i].color_index_map = ctx->color_index_map[i];
  // Restore the coding context of the MB to that that was in place
  // when the mode was picked for it
  for (y = 0; y < mi_height; y++)
    for (x_idx = 0; x_idx < mi_width; x_idx++)
      if ((xd->mb_to_right_edge >> (3 + MI_SIZE_LOG2)) + mi_width > x_idx &&
          (xd->mb_to_bottom_edge >> (3 + MI_SIZE_LOG2)) + mi_height > y) {
        xd->mi[x_idx + y * mis] = mi_addr;
      }

  if (cpi->oxcf.aq_mode) av1_init_plane_quantizers(cpi, x, mi_addr->segment_id);

  if (dry_run) return;

#if CONFIG_INTERNAL_STATS
  {
    unsigned int *const mode_chosen_counts =
        (unsigned int *)cpi->mode_chosen_counts;  // Cast const away.
    if (frame_is_intra_only(cm)) {
      static const int kf_mode_index[] = {
        THR_DC /*DC_PRED*/,
        THR_V_PRED /*V_PRED*/,
        THR_H_PRED /*H_PRED*/,
        THR_D45_PRED /*D45_PRED*/,
        THR_D135_PRED /*D135_PRED*/,
        THR_D113_PRED /*D113_PRED*/,
        THR_D157_PRED /*D157_PRED*/,
        THR_D203_PRED /*D203_PRED*/,
        THR_D67_PRED /*D67_PRED*/,
        THR_SMOOTH,   /*SMOOTH_PRED*/
        THR_SMOOTH_V, /*SMOOTH_V_PRED*/
        THR_SMOOTH_H, /*SMOOTH_H_PRED*/
        THR_PAETH /*PAETH_PRED*/,
      };
      ++mode_chosen_counts[kf_mode_index[mi_addr->mode]];
    } else {
      // Note how often each mode chosen as best
      ++mode_chosen_counts[ctx->best_mode_index];
    }
  }
#endif
  if (!frame_is_intra_only(cm)) {
    if (is_inter_block(mi_addr)) {
      // TODO(sarahparker): global motion stats need to be handled per-tile
      // to be compatible with tile-based threading.
      update_global_motion_used(mi_addr->mode, bsize, mi_addr, rdc);
    }

    if (cm->interp_filter == SWITCHABLE &&
        mi_addr->motion_mode != WARPED_CAUSAL &&
        !is_nontrans_global_motion(xd, xd->mi[0])) {
      update_filter_type_count(tile_data->allow_update_cdf, td->counts, xd,
                               mi_addr);
    }

    rdc->comp_pred_diff[SINGLE_REFERENCE] += ctx->single_pred_diff;
    rdc->comp_pred_diff[COMPOUND_REFERENCE] += ctx->comp_pred_diff;
    rdc->comp_pred_diff[REFERENCE_MODE_SELECT] += ctx->hybrid_pred_diff;
  }

  const int x_mis = AOMMIN(bw, cm->mi_cols - mi_col);
  const int y_mis = AOMMIN(bh, cm->mi_rows - mi_row);
  av1_copy_frame_mvs(cm, mi, mi_row, mi_col, x_mis, y_mis);
}

void av1_setup_src_planes(MACROBLOCK *x, const YV12_BUFFER_CONFIG *src,
                          int mi_row, int mi_col, const int num_planes) {
  // Set current frame pointer.
  x->e_mbd.cur_buf = src;

  // We use AOMMIN(num_planes, MAX_MB_PLANE) instead of num_planes to quiet
  // the static analysis warnings.
  for (int i = 0; i < AOMMIN(num_planes, MAX_MB_PLANE); i++) {
    const int is_uv = i > 0;
    setup_pred_plane(&x->plane[i].src, x->e_mbd.mi[0]->sb_type, src->buffers[i],
                     src->crop_widths[is_uv], src->crop_heights[is_uv],
                     src->strides[is_uv], mi_row, mi_col, NULL,
                     x->e_mbd.plane[i].subsampling_x,
                     x->e_mbd.plane[i].subsampling_y);
  }
}

static int set_segment_rdmult(const AV1_COMP *const cpi, MACROBLOCK *const x,
                              int8_t segment_id) {
  const AV1_COMMON *const cm = &cpi->common;
  av1_init_plane_quantizers(cpi, x, segment_id);
  aom_clear_system_state();
  int segment_qindex = av1_get_qindex(&cm->seg, segment_id, cm->base_qindex);
  return av1_compute_rd_mult(cpi, segment_qindex + cm->y_dc_delta_q);
}

static int set_deltaq_rdmult(const AV1_COMP *const cpi, MACROBLOCKD *const xd) {
  const AV1_COMMON *const cm = &cpi->common;

  return av1_compute_rd_mult(
      cpi, cm->base_qindex + xd->delta_qindex + cm->y_dc_delta_q);
}

static void rd_pick_sb_modes(const AV1_COMP *const cpi, TileDataEnc *tile_data,
                             MACROBLOCK *const x, int mi_row, int mi_col,
                             RD_STATS *rd_cost, PARTITION_TYPE partition,
                             BLOCK_SIZE bsize, PICK_MODE_CONTEXT *ctx,
                             int64_t best_rd) {
  const AV1_COMMON *const cm = &cpi->common;
  const int num_planes = av1_num_planes(cm);
  TileInfo *const tile_info = &tile_data->tile_info;
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *mbmi;
  MB_MODE_INFO *ctx_mbmi = &ctx->mic;
  struct macroblock_plane *const p = x->plane;
  struct macroblockd_plane *const pd = xd->plane;
  const AQ_MODE aq_mode = cpi->oxcf.aq_mode;
  const DELTAQ_MODE deltaq_mode = cpi->oxcf.deltaq_mode;
  int i, orig_rdmult;

  aom_clear_system_state();

  set_offsets(cpi, tile_info, x, mi_row, mi_col, bsize);

  mbmi = xd->mi[0];

  if (ctx->rd_mode_is_ready) {
    assert(ctx_mbmi->sb_type == bsize);
    assert(ctx_mbmi->partition == partition);
    *mbmi = *ctx_mbmi;
    rd_cost->rate = ctx->rate;
    rd_cost->dist = ctx->dist;
    rd_cost->rdcost = ctx->rdcost;
  } else {
    mbmi->sb_type = bsize;
    mbmi->partition = partition;
  }

#if CONFIG_RD_DEBUG
  mbmi->mi_row = mi_row;
  mbmi->mi_col = mi_col;
#endif

  for (i = 0; i < num_planes; ++i) {
    p[i].coeff = ctx->coeff[i];
    p[i].qcoeff = ctx->qcoeff[i];
    pd[i].dqcoeff = ctx->dqcoeff[i];
    p[i].eobs = ctx->eobs[i];
    p[i].txb_entropy_ctx = ctx->txb_entropy_ctx[i];
  }

  for (i = 0; i < 2; ++i) pd[i].color_index_map = ctx->color_index_map[i];

  if (!ctx->rd_mode_is_ready) {
    ctx->skippable = 0;

    // Set to zero to make sure we do not use the previous encoded frame stats
    mbmi->skip = 0;

    // Reset skip mode flag.
    mbmi->skip_mode = 0;
  }

  x->skip_chroma_rd =
      !is_chroma_reference(mi_row, mi_col, bsize, xd->plane[1].subsampling_x,
                           xd->plane[1].subsampling_y);

  if (ctx->rd_mode_is_ready) {
    x->skip = ctx->skip;
    *x->mbmi_ext = ctx->mbmi_ext;
    return;
  }

  if (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH) {
    x->source_variance = av1_high_get_sby_perpixel_variance(
        cpi, &x->plane[0].src, bsize, xd->bd);
  } else {
    x->source_variance =
        av1_get_sby_perpixel_variance(cpi, &x->plane[0].src, bsize);
  }

  // Save rdmult before it might be changed, so it can be restored later.
  orig_rdmult = x->rdmult;

  if (aq_mode == VARIANCE_AQ) {
    if (cpi->vaq_refresh) {
      const int energy =
          bsize <= BLOCK_16X16 ? x->mb_energy : av1_block_energy(cpi, x, bsize);
      mbmi->segment_id = av1_vaq_segment_id(energy);
    }
    x->rdmult = set_segment_rdmult(cpi, x, mbmi->segment_id);
  } else if (aq_mode == COMPLEXITY_AQ) {
    x->rdmult = set_segment_rdmult(cpi, x, mbmi->segment_id);
  } else if (aq_mode == CYCLIC_REFRESH_AQ) {
    // If segment is boosted, use rdmult for that segment.
    if (cyclic_refresh_segment_id_boosted(mbmi->segment_id))
      x->rdmult = av1_cyclic_refresh_get_rdmult(cpi->cyclic_refresh);
  }

  if (deltaq_mode > 0) x->rdmult = set_deltaq_rdmult(cpi, xd);

  // Find best coding mode & reconstruct the MB so it is available
  // as a predictor for MBs that follow in the SB
  if (frame_is_intra_only(cm)) {
    av1_rd_pick_intra_mode_sb(cpi, x, mi_row, mi_col, rd_cost, bsize, ctx,
                              best_rd);
  } else {
    if (segfeature_active(&cm->seg, mbmi->segment_id, SEG_LVL_SKIP)) {
      av1_rd_pick_inter_mode_sb_seg_skip(cpi, tile_data, x, mi_row, mi_col,
                                         rd_cost, bsize, ctx, best_rd);
    } else {
      av1_rd_pick_inter_mode_sb(cpi, tile_data, x, mi_row, mi_col, rd_cost,
                                bsize, ctx, best_rd);
    }
  }

  // Examine the resulting rate and for AQ mode 2 make a segment choice.
  if ((rd_cost->rate != INT_MAX) && (aq_mode == COMPLEXITY_AQ) &&
      (bsize >= BLOCK_16X16) &&
      (cm->frame_type == KEY_FRAME || cpi->refresh_alt_ref_frame ||
       cpi->refresh_alt2_ref_frame ||
       (cpi->refresh_golden_frame && !cpi->rc.is_src_frame_alt_ref))) {
    av1_caq_select_segment(cpi, x, bsize, mi_row, mi_col, rd_cost->rate);
  }

  x->rdmult = orig_rdmult;

  // TODO(jingning) The rate-distortion optimization flow needs to be
  // refactored to provide proper exit/return handle.
  if (rd_cost->rate == INT_MAX) rd_cost->rdcost = INT64_MAX;

  ctx->rate = rd_cost->rate;
  ctx->dist = rd_cost->dist;
  ctx->rdcost = rd_cost->rdcost;
}

static void update_inter_mode_stats(FRAME_CONTEXT *fc, FRAME_COUNTS *counts,
                                    PREDICTION_MODE mode, int16_t mode_context,
                                    uint8_t allow_update_cdf) {
  (void)counts;

  int16_t mode_ctx = mode_context & NEWMV_CTX_MASK;
  if (mode == NEWMV) {
#if CONFIG_ENTROPY_STATS
    ++counts->newmv_mode[mode_ctx][0];
#endif
    if (allow_update_cdf) update_cdf(fc->newmv_cdf[mode_ctx], 0, 2);
    return;
  } else {
#if CONFIG_ENTROPY_STATS
    ++counts->newmv_mode[mode_ctx][1];
#endif
    if (allow_update_cdf) update_cdf(fc->newmv_cdf[mode_ctx], 1, 2);

    mode_ctx = (mode_context >> GLOBALMV_OFFSET) & GLOBALMV_CTX_MASK;
    if (mode == GLOBALMV) {
#if CONFIG_ENTROPY_STATS
      ++counts->zeromv_mode[mode_ctx][0];
#endif
      if (allow_update_cdf) update_cdf(fc->zeromv_cdf[mode_ctx], 0, 2);
      return;
    } else {
#if CONFIG_ENTROPY_STATS
      ++counts->zeromv_mode[mode_ctx][1];
#endif
      if (allow_update_cdf) update_cdf(fc->zeromv_cdf[mode_ctx], 1, 2);
      mode_ctx = (mode_context >> REFMV_OFFSET) & REFMV_CTX_MASK;
#if CONFIG_ENTROPY_STATS
      ++counts->refmv_mode[mode_ctx][mode != NEARESTMV];
#endif
      if (allow_update_cdf)
        update_cdf(fc->refmv_cdf[mode_ctx], mode != NEARESTMV, 2);
    }
  }
}

static void update_palette_cdf(MACROBLOCKD *xd, const MB_MODE_INFO *const mbmi,
                               FRAME_COUNTS *counts, uint8_t allow_update_cdf) {
  FRAME_CONTEXT *fc = xd->tile_ctx;
  const BLOCK_SIZE bsize = mbmi->sb_type;
  const PALETTE_MODE_INFO *const pmi = &mbmi->palette_mode_info;
  const int palette_bsize_ctx = av1_get_palette_bsize_ctx(bsize);

  (void)counts;

  if (mbmi->mode == DC_PRED) {
    const int n = pmi->palette_size[0];
    const int palette_mode_ctx = av1_get_palette_mode_ctx(xd);

#if CONFIG_ENTROPY_STATS
    ++counts->palette_y_mode[palette_bsize_ctx][palette_mode_ctx][n > 0];
#endif
    if (allow_update_cdf)
      update_cdf(fc->palette_y_mode_cdf[palette_bsize_ctx][palette_mode_ctx],
                 n > 0, 2);
    if (n > 0) {
#if CONFIG_ENTROPY_STATS
      ++counts->palette_y_size[palette_bsize_ctx][n - PALETTE_MIN_SIZE];
#endif
      if (allow_update_cdf) {
        update_cdf(fc->palette_y_size_cdf[palette_bsize_ctx],
                   n - PALETTE_MIN_SIZE, PALETTE_SIZES);
      }
    }
  }

  if (mbmi->uv_mode == UV_DC_PRED) {
    const int n = pmi->palette_size[1];
    const int palette_uv_mode_ctx = (pmi->palette_size[0] > 0);

#if CONFIG_ENTROPY_STATS
    ++counts->palette_uv_mode[palette_uv_mode_ctx][n > 0];
#endif
    if (allow_update_cdf)
      update_cdf(fc->palette_uv_mode_cdf[palette_uv_mode_ctx], n > 0, 2);

    if (n > 0) {
#if CONFIG_ENTROPY_STATS
      ++counts->palette_uv_size[palette_bsize_ctx][n - PALETTE_MIN_SIZE];
#endif
      if (allow_update_cdf) {
        update_cdf(fc->palette_uv_size_cdf[palette_bsize_ctx],
                   n - PALETTE_MIN_SIZE, PALETTE_SIZES);
      }
    }
  }
}

static void sum_intra_stats(const AV1_COMMON *const cm, FRAME_COUNTS *counts,
                            MACROBLOCKD *xd, const MB_MODE_INFO *const mbmi,
                            const MB_MODE_INFO *above_mi,
                            const MB_MODE_INFO *left_mi, const int intraonly,
                            const int mi_row, const int mi_col,
                            uint8_t allow_update_cdf) {
  FRAME_CONTEXT *fc = xd->tile_ctx;
  const PREDICTION_MODE y_mode = mbmi->mode;
  const UV_PREDICTION_MODE uv_mode = mbmi->uv_mode;
  (void)counts;
  const BLOCK_SIZE bsize = mbmi->sb_type;

  if (intraonly) {
#if CONFIG_ENTROPY_STATS
    const PREDICTION_MODE above = av1_above_block_mode(above_mi);
    const PREDICTION_MODE left = av1_left_block_mode(left_mi);
    const int above_ctx = intra_mode_context[above];
    const int left_ctx = intra_mode_context[left];
    ++counts->kf_y_mode[above_ctx][left_ctx][y_mode];
#endif  // CONFIG_ENTROPY_STATS
    if (allow_update_cdf)
      update_cdf(get_y_mode_cdf(fc, above_mi, left_mi), y_mode, INTRA_MODES);
  } else {
#if CONFIG_ENTROPY_STATS
    ++counts->y_mode[size_group_lookup[bsize]][y_mode];
#endif  // CONFIG_ENTROPY_STATS
    if (allow_update_cdf)
      update_cdf(fc->y_mode_cdf[size_group_lookup[bsize]], y_mode, INTRA_MODES);
  }

  if (av1_filter_intra_allowed(cm, mbmi)) {
    const int use_filter_intra_mode =
        mbmi->filter_intra_mode_info.use_filter_intra;
#if CONFIG_ENTROPY_STATS
    ++counts->filter_intra[mbmi->sb_type][use_filter_intra_mode];
    if (use_filter_intra_mode) {
      ++counts
            ->filter_intra_mode[mbmi->filter_intra_mode_info.filter_intra_mode];
    }
#endif  // CONFIG_ENTROPY_STATS
    if (allow_update_cdf) {
      update_cdf(fc->filter_intra_cdfs[mbmi->sb_type], use_filter_intra_mode,
                 2);
      if (use_filter_intra_mode) {
        update_cdf(fc->filter_intra_mode_cdf,
                   mbmi->filter_intra_mode_info.filter_intra_mode,
                   FILTER_INTRA_MODES);
      }
    }
  }
  if (av1_is_directional_mode(mbmi->mode) && av1_use_angle_delta(bsize)) {
#if CONFIG_ENTROPY_STATS
    ++counts->angle_delta[mbmi->mode - V_PRED]
                         [mbmi->angle_delta[PLANE_TYPE_Y] + MAX_ANGLE_DELTA];
#endif
    if (allow_update_cdf) {
      update_cdf(fc->angle_delta_cdf[mbmi->mode - V_PRED],
                 mbmi->angle_delta[PLANE_TYPE_Y] + MAX_ANGLE_DELTA,
                 2 * MAX_ANGLE_DELTA + 1);
    }
  }

  if (!is_chroma_reference(mi_row, mi_col, bsize,
                           xd->plane[AOM_PLANE_U].subsampling_x,
                           xd->plane[AOM_PLANE_U].subsampling_y))
    return;

#if CONFIG_ENTROPY_STATS
  ++counts->uv_mode[is_cfl_allowed(xd)][y_mode][uv_mode];
#endif  // CONFIG_ENTROPY_STATS
  if (allow_update_cdf) {
    const CFL_ALLOWED_TYPE cfl_allowed = is_cfl_allowed(xd);
    update_cdf(fc->uv_mode_cdf[cfl_allowed][y_mode], uv_mode,
               UV_INTRA_MODES - !cfl_allowed);
  }
  if (uv_mode == UV_CFL_PRED) {
    const int joint_sign = mbmi->cfl_alpha_signs;
    const int idx = mbmi->cfl_alpha_idx;

#if CONFIG_ENTROPY_STATS
    ++counts->cfl_sign[joint_sign];
#endif
    if (allow_update_cdf)
      update_cdf(fc->cfl_sign_cdf, joint_sign, CFL_JOINT_SIGNS);
    if (CFL_SIGN_U(joint_sign) != CFL_SIGN_ZERO) {
      aom_cdf_prob *cdf_u = fc->cfl_alpha_cdf[CFL_CONTEXT_U(joint_sign)];

#if CONFIG_ENTROPY_STATS
      ++counts->cfl_alpha[CFL_CONTEXT_U(joint_sign)][CFL_IDX_U(idx)];
#endif
      if (allow_update_cdf)
        update_cdf(cdf_u, CFL_IDX_U(idx), CFL_ALPHABET_SIZE);
    }
    if (CFL_SIGN_V(joint_sign) != CFL_SIGN_ZERO) {
      aom_cdf_prob *cdf_v = fc->cfl_alpha_cdf[CFL_CONTEXT_V(joint_sign)];

#if CONFIG_ENTROPY_STATS
      ++counts->cfl_alpha[CFL_CONTEXT_V(joint_sign)][CFL_IDX_V(idx)];
#endif
      if (allow_update_cdf)
        update_cdf(cdf_v, CFL_IDX_V(idx), CFL_ALPHABET_SIZE);
    }
  }
  if (av1_is_directional_mode(get_uv_mode(uv_mode)) &&
      av1_use_angle_delta(bsize)) {
#if CONFIG_ENTROPY_STATS
    ++counts->angle_delta[uv_mode - UV_V_PRED]
                         [mbmi->angle_delta[PLANE_TYPE_UV] + MAX_ANGLE_DELTA];
#endif
    if (allow_update_cdf) {
      update_cdf(fc->angle_delta_cdf[uv_mode - UV_V_PRED],
                 mbmi->angle_delta[PLANE_TYPE_UV] + MAX_ANGLE_DELTA,
                 2 * MAX_ANGLE_DELTA + 1);
    }
  }
  if (av1_allow_palette(cm->allow_screen_content_tools, bsize))
    update_palette_cdf(xd, mbmi, counts, allow_update_cdf);
}

static void update_stats(const AV1_COMMON *const cm, TileDataEnc *tile_data,
                         ThreadData *td, int mi_row, int mi_col) {
  MACROBLOCK *x = &td->mb;
  MACROBLOCKD *const xd = &x->e_mbd;
  const MB_MODE_INFO *const mbmi = xd->mi[0];
  const MB_MODE_INFO_EXT *const mbmi_ext = x->mbmi_ext;
  const BLOCK_SIZE bsize = mbmi->sb_type;
  FRAME_CONTEXT *fc = xd->tile_ctx;
  const uint8_t allow_update_cdf = tile_data->allow_update_cdf;

  // delta quant applies to both intra and inter
  const int super_block_upper_left =
      ((mi_row & (cm->seq_params.mib_size - 1)) == 0) &&
      ((mi_col & (cm->seq_params.mib_size - 1)) == 0);

  const int seg_ref_active =
      segfeature_active(&cm->seg, mbmi->segment_id, SEG_LVL_REF_FRAME);

  if (cm->skip_mode_flag && !seg_ref_active && is_comp_ref_allowed(bsize)) {
    const int skip_mode_ctx = av1_get_skip_mode_context(xd);
#if CONFIG_ENTROPY_STATS
    td->counts->skip_mode[skip_mode_ctx][mbmi->skip_mode]++;
#endif
    if (allow_update_cdf)
      update_cdf(fc->skip_mode_cdfs[skip_mode_ctx], mbmi->skip_mode, 2);
  }

  if (!mbmi->skip_mode) {
    if (!seg_ref_active) {
      const int skip_ctx = av1_get_skip_context(xd);
#if CONFIG_ENTROPY_STATS
      td->counts->skip[skip_ctx][mbmi->skip]++;
#endif
      if (allow_update_cdf) update_cdf(fc->skip_cdfs[skip_ctx], mbmi->skip, 2);
    }
  }

  if (cm->delta_q_present_flag &&
      (bsize != cm->seq_params.sb_size || !mbmi->skip) &&
      super_block_upper_left) {
#if CONFIG_ENTROPY_STATS
    const int dq =
        (mbmi->current_qindex - xd->current_qindex) / cm->delta_q_res;
    const int absdq = abs(dq);
    for (int i = 0; i < AOMMIN(absdq, DELTA_Q_SMALL); ++i) {
      td->counts->delta_q[i][1]++;
    }
    if (absdq < DELTA_Q_SMALL) td->counts->delta_q[absdq][0]++;
#endif
    xd->current_qindex = mbmi->current_qindex;
    if (cm->delta_lf_present_flag) {
      if (cm->delta_lf_multi) {
        const int frame_lf_count =
            av1_num_planes(cm) > 1 ? FRAME_LF_COUNT : FRAME_LF_COUNT - 2;
        for (int lf_id = 0; lf_id < frame_lf_count; ++lf_id) {
#if CONFIG_ENTROPY_STATS
          const int delta_lf =
              (mbmi->delta_lf[lf_id] - xd->delta_lf[lf_id]) / cm->delta_lf_res;
          const int abs_delta_lf = abs(delta_lf);
          for (int i = 0; i < AOMMIN(abs_delta_lf, DELTA_LF_SMALL); ++i) {
            td->counts->delta_lf_multi[lf_id][i][1]++;
          }
          if (abs_delta_lf < DELTA_LF_SMALL)
            td->counts->delta_lf_multi[lf_id][abs_delta_lf][0]++;
#endif
          xd->delta_lf[lf_id] = mbmi->delta_lf[lf_id];
        }
      } else {
#if CONFIG_ENTROPY_STATS
        const int delta_lf =
            (mbmi->delta_lf_from_base - xd->delta_lf_from_base) /
            cm->delta_lf_res;
        const int abs_delta_lf = abs(delta_lf);
        for (int i = 0; i < AOMMIN(abs_delta_lf, DELTA_LF_SMALL); ++i) {
          td->counts->delta_lf[i][1]++;
        }
        if (abs_delta_lf < DELTA_LF_SMALL)
          td->counts->delta_lf[abs_delta_lf][0]++;
#endif
        xd->delta_lf_from_base = mbmi->delta_lf_from_base;
      }
    }
  }

  if (!is_inter_block(mbmi)) {
    sum_intra_stats(cm, td->counts, xd, mbmi, xd->above_mbmi, xd->left_mbmi,
                    frame_is_intra_only(cm), mi_row, mi_col,
                    tile_data->allow_update_cdf);
  }

  if (av1_allow_intrabc(cm)) {
    if (allow_update_cdf)
      update_cdf(fc->intrabc_cdf, is_intrabc_block(mbmi), 2);
#if CONFIG_ENTROPY_STATS
    ++td->counts->intrabc[is_intrabc_block(mbmi)];
#endif  // CONFIG_ENTROPY_STATS
  }

  if (!frame_is_intra_only(cm)) {
    RD_COUNTS *rdc = &td->rd_counts;

    FRAME_COUNTS *const counts = td->counts;

    if (mbmi->skip_mode) {
      rdc->skip_mode_used_flag = 1;
      if (cm->reference_mode == REFERENCE_MODE_SELECT) {
        assert(has_second_ref(mbmi));
        rdc->compound_ref_used_flag = 1;
      }
      set_ref_ptrs(cm, xd, mbmi->ref_frame[0], mbmi->ref_frame[1]);
      return;
    }

    const int inter_block = is_inter_block(mbmi);

    if (!seg_ref_active) {
#if CONFIG_ENTROPY_STATS
      counts->intra_inter[av1_get_intra_inter_context(xd)][inter_block]++;
#endif
      if (allow_update_cdf) {
        update_cdf(fc->intra_inter_cdf[av1_get_intra_inter_context(xd)],
                   inter_block, 2);
      }
      // If the segment reference feature is enabled we have only a single
      // reference frame allowed for the segment so exclude it from
      // the reference frame counts used to work out probabilities.
      if (inter_block) {
        const MV_REFERENCE_FRAME ref0 = mbmi->ref_frame[0];
        const MV_REFERENCE_FRAME ref1 = mbmi->ref_frame[1];

        av1_collect_neighbors_ref_counts(xd);

        if (cm->reference_mode == REFERENCE_MODE_SELECT) {
          if (has_second_ref(mbmi))
            // This flag is also updated for 4x4 blocks
            rdc->compound_ref_used_flag = 1;
          if (is_comp_ref_allowed(bsize)) {
#if CONFIG_ENTROPY_STATS
            counts->comp_inter[av1_get_reference_mode_context(xd)]
                              [has_second_ref(mbmi)]++;
#endif  // CONFIG_ENTROPY_STATS
            if (allow_update_cdf) {
              update_cdf(av1_get_reference_mode_cdf(xd), has_second_ref(mbmi),
                         2);
            }
          }
        }

        if (has_second_ref(mbmi)) {
          const COMP_REFERENCE_TYPE comp_ref_type = has_uni_comp_refs(mbmi)
                                                        ? UNIDIR_COMP_REFERENCE
                                                        : BIDIR_COMP_REFERENCE;
          if (allow_update_cdf) {
            update_cdf(av1_get_comp_reference_type_cdf(xd), comp_ref_type,
                       COMP_REFERENCE_TYPES);
          }
#if CONFIG_ENTROPY_STATS
          counts->comp_ref_type[av1_get_comp_reference_type_context(xd)]
                               [comp_ref_type]++;
#endif  // CONFIG_ENTROPY_STATS

          if (comp_ref_type == UNIDIR_COMP_REFERENCE) {
            const int bit = (ref0 == BWDREF_FRAME);
            if (allow_update_cdf)
              update_cdf(av1_get_pred_cdf_uni_comp_ref_p(xd), bit, 2);
#if CONFIG_ENTROPY_STATS
            counts->uni_comp_ref[av1_get_pred_context_uni_comp_ref_p(xd)][0]
                                [bit]++;
#endif  // CONFIG_ENTROPY_STATS
            if (!bit) {
              const int bit1 = (ref1 == LAST3_FRAME || ref1 == GOLDEN_FRAME);
              if (allow_update_cdf)
                update_cdf(av1_get_pred_cdf_uni_comp_ref_p1(xd), bit1, 2);
#if CONFIG_ENTROPY_STATS
              counts->uni_comp_ref[av1_get_pred_context_uni_comp_ref_p1(xd)][1]
                                  [bit1]++;
#endif  // CONFIG_ENTROPY_STATS
              if (bit1) {
                if (allow_update_cdf) {
                  update_cdf(av1_get_pred_cdf_uni_comp_ref_p2(xd),
                             ref1 == GOLDEN_FRAME, 2);
                }
#if CONFIG_ENTROPY_STATS
                counts->uni_comp_ref[av1_get_pred_context_uni_comp_ref_p2(xd)]
                                    [2][ref1 == GOLDEN_FRAME]++;
#endif  // CONFIG_ENTROPY_STATS
              }
            }
          } else {
            const int bit = (ref0 == GOLDEN_FRAME || ref0 == LAST3_FRAME);
            if (allow_update_cdf)
              update_cdf(av1_get_pred_cdf_comp_ref_p(xd), bit, 2);
#if CONFIG_ENTROPY_STATS
            counts->comp_ref[av1_get_pred_context_comp_ref_p(xd)][0][bit]++;
#endif  // CONFIG_ENTROPY_STATS
            if (!bit) {
              if (allow_update_cdf) {
                update_cdf(av1_get_pred_cdf_comp_ref_p1(xd),
                           ref0 == LAST2_FRAME, 2);
              }
#if CONFIG_ENTROPY_STATS
              counts->comp_ref[av1_get_pred_context_comp_ref_p1(xd)][1]
                              [ref0 == LAST2_FRAME]++;
#endif  // CONFIG_ENTROPY_STATS
            } else {
              if (allow_update_cdf) {
                update_cdf(av1_get_pred_cdf_comp_ref_p2(xd),
                           ref0 == GOLDEN_FRAME, 2);
              }
#if CONFIG_ENTROPY_STATS
              counts->comp_ref[av1_get_pred_context_comp_ref_p2(xd)][2]
                              [ref0 == GOLDEN_FRAME]++;
#endif  // CONFIG_ENTROPY_STATS
            }
            if (allow_update_cdf) {
              update_cdf(av1_get_pred_cdf_comp_bwdref_p(xd),
                         ref1 == ALTREF_FRAME, 2);
            }
#if CONFIG_ENTROPY_STATS
            counts->comp_bwdref[av1_get_pred_context_comp_bwdref_p(xd)][0]
                               [ref1 == ALTREF_FRAME]++;
#endif  // CONFIG_ENTROPY_STATS
            if (ref1 != ALTREF_FRAME) {
              if (allow_update_cdf) {
                update_cdf(av1_get_pred_cdf_comp_bwdref_p1(xd),
                           ref1 == ALTREF2_FRAME, 2);
              }
#if CONFIG_ENTROPY_STATS
              counts->comp_bwdref[av1_get_pred_context_comp_bwdref_p1(xd)][1]
                                 [ref1 == ALTREF2_FRAME]++;
#endif  // CONFIG_ENTROPY_STATS
            }
          }
        } else {
          const int bit = (ref0 >= BWDREF_FRAME);
          if (allow_update_cdf)
            update_cdf(av1_get_pred_cdf_single_ref_p1(xd), bit, 2);
#if CONFIG_ENTROPY_STATS
          counts->single_ref[av1_get_pred_context_single_ref_p1(xd)][0][bit]++;
#endif  // CONFIG_ENTROPY_STATS
          if (bit) {
            assert(ref0 <= ALTREF_FRAME);
            if (allow_update_cdf) {
              update_cdf(av1_get_pred_cdf_single_ref_p2(xd),
                         ref0 == ALTREF_FRAME, 2);
            }
#if CONFIG_ENTROPY_STATS
            counts->single_ref[av1_get_pred_context_single_ref_p2(xd)][1]
                              [ref0 == ALTREF_FRAME]++;
#endif  // CONFIG_ENTROPY_STATS
            if (ref0 != ALTREF_FRAME) {
              if (allow_update_cdf) {
                update_cdf(av1_get_pred_cdf_single_ref_p6(xd),
                           ref0 == ALTREF2_FRAME, 2);
              }
#if CONFIG_ENTROPY_STATS
              counts->single_ref[av1_get_pred_context_single_ref_p6(xd)][5]
                                [ref0 == ALTREF2_FRAME]++;
#endif  // CONFIG_ENTROPY_STATS
            }
          } else {
            const int bit1 = !(ref0 == LAST2_FRAME || ref0 == LAST_FRAME);
            if (allow_update_cdf)
              update_cdf(av1_get_pred_cdf_single_ref_p3(xd), bit1, 2);
#if CONFIG_ENTROPY_STATS
            counts
                ->single_ref[av1_get_pred_context_single_ref_p3(xd)][2][bit1]++;
#endif  // CONFIG_ENTROPY_STATS
            if (!bit1) {
              if (allow_update_cdf) {
                update_cdf(av1_get_pred_cdf_single_ref_p4(xd),
                           ref0 != LAST_FRAME, 2);
              }
#if CONFIG_ENTROPY_STATS
              counts->single_ref[av1_get_pred_context_single_ref_p4(xd)][3]
                                [ref0 != LAST_FRAME]++;
#endif  // CONFIG_ENTROPY_STATS
            } else {
              if (allow_update_cdf) {
                update_cdf(av1_get_pred_cdf_single_ref_p5(xd),
                           ref0 != LAST3_FRAME, 2);
              }
#if CONFIG_ENTROPY_STATS
              counts->single_ref[av1_get_pred_context_single_ref_p5(xd)][4]
                                [ref0 != LAST3_FRAME]++;
#endif  // CONFIG_ENTROPY_STATS
            }
          }
        }

        if (cm->seq_params.enable_interintra_compound &&
            is_interintra_allowed(mbmi)) {
          const int bsize_group = size_group_lookup[bsize];
          if (mbmi->ref_frame[1] == INTRA_FRAME) {
#if CONFIG_ENTROPY_STATS
            counts->interintra[bsize_group][1]++;
#endif
            if (allow_update_cdf)
              update_cdf(fc->interintra_cdf[bsize_group], 1, 2);
#if CONFIG_ENTROPY_STATS
            counts->interintra_mode[bsize_group][mbmi->interintra_mode]++;
#endif
            if (allow_update_cdf) {
              update_cdf(fc->interintra_mode_cdf[bsize_group],
                         mbmi->interintra_mode, INTERINTRA_MODES);
            }
            if (is_interintra_wedge_used(bsize)) {
#if CONFIG_ENTROPY_STATS
              counts->wedge_interintra[bsize][mbmi->use_wedge_interintra]++;
#endif
              if (allow_update_cdf) {
                update_cdf(fc->wedge_interintra_cdf[bsize],
                           mbmi->use_wedge_interintra, 2);
              }
              if (mbmi->use_wedge_interintra) {
#if CONFIG_ENTROPY_STATS
                counts->wedge_idx[bsize][mbmi->interintra_wedge_index]++;
#endif
                if (allow_update_cdf) {
                  update_cdf(fc->wedge_idx_cdf[bsize],
                             mbmi->interintra_wedge_index, 16);
                }
              }
            }
          } else {
#if CONFIG_ENTROPY_STATS
            counts->interintra[bsize_group][0]++;
#endif
            if (allow_update_cdf)
              update_cdf(fc->interintra_cdf[bsize_group], 0, 2);
          }
        }

        set_ref_ptrs(cm, xd, mbmi->ref_frame[0], mbmi->ref_frame[1]);
        const MOTION_MODE motion_allowed =
            cm->switchable_motion_mode
                ? motion_mode_allowed(xd->global_motion, xd, mbmi,
                                      cm->allow_warped_motion)
                : SIMPLE_TRANSLATION;
        if (mbmi->ref_frame[1] != INTRA_FRAME) {
          if (motion_allowed == WARPED_CAUSAL) {
#if CONFIG_ENTROPY_STATS
            counts->motion_mode[bsize][mbmi->motion_mode]++;
#endif
            if (allow_update_cdf) {
              update_cdf(fc->motion_mode_cdf[bsize], mbmi->motion_mode,
                         MOTION_MODES);
            }
          } else if (motion_allowed == OBMC_CAUSAL) {
#if CONFIG_ENTROPY_STATS
            counts->obmc[bsize][mbmi->motion_mode == OBMC_CAUSAL]++;
#endif
            if (allow_update_cdf) {
              update_cdf(fc->obmc_cdf[bsize], mbmi->motion_mode == OBMC_CAUSAL,
                         2);
            }
          }
        }

        if (has_second_ref(mbmi)) {
          assert(cm->reference_mode != SINGLE_REFERENCE &&
                 is_inter_compound_mode(mbmi->mode) &&
                 mbmi->motion_mode == SIMPLE_TRANSLATION);

          const int masked_compound_used =
              is_any_masked_compound_used(bsize) &&
              cm->seq_params.enable_masked_compound;
          if (masked_compound_used) {
            const int comp_group_idx_ctx = get_comp_group_idx_context(xd);
#if CONFIG_ENTROPY_STATS
            ++counts->comp_group_idx[comp_group_idx_ctx][mbmi->comp_group_idx];
#endif
            if (allow_update_cdf) {
              update_cdf(fc->comp_group_idx_cdf[comp_group_idx_ctx],
                         mbmi->comp_group_idx, 2);
            }
          }

          if (mbmi->comp_group_idx == 0) {
            const int comp_index_ctx = get_comp_index_context(cm, xd);
#if CONFIG_ENTROPY_STATS
            ++counts->compound_index[comp_index_ctx][mbmi->compound_idx];
#endif
            if (allow_update_cdf) {
              update_cdf(fc->compound_index_cdf[comp_index_ctx],
                         mbmi->compound_idx, 2);
            }
          } else {
            assert(masked_compound_used);
            if (is_interinter_compound_used(COMPOUND_WEDGE, bsize)) {
#if CONFIG_ENTROPY_STATS
              ++counts->compound_type[bsize][mbmi->interinter_comp.type - 1];
#endif
              if (allow_update_cdf) {
                update_cdf(fc->compound_type_cdf[bsize],
                           mbmi->interinter_comp.type - 1, COMPOUND_TYPES - 1);
              }
            }
          }
        }
        if (mbmi->interinter_comp.type == COMPOUND_WEDGE) {
          if (is_interinter_compound_used(COMPOUND_WEDGE, bsize)) {
#if CONFIG_ENTROPY_STATS
            counts->wedge_idx[bsize][mbmi->interinter_comp.wedge_index]++;
#endif
            if (allow_update_cdf) {
              update_cdf(fc->wedge_idx_cdf[bsize],
                         mbmi->interinter_comp.wedge_index, 16);
            }
          }
        }
      }
    }

    if (inter_block &&
        !segfeature_active(&cm->seg, mbmi->segment_id, SEG_LVL_SKIP)) {
      int16_t mode_ctx;
      const PREDICTION_MODE mode = mbmi->mode;

      mode_ctx =
          av1_mode_context_analyzer(mbmi_ext->mode_context, mbmi->ref_frame);
      if (has_second_ref(mbmi)) {
#if CONFIG_ENTROPY_STATS
        ++counts->inter_compound_mode[mode_ctx][INTER_COMPOUND_OFFSET(mode)];
#endif
        if (allow_update_cdf)
          update_cdf(fc->inter_compound_mode_cdf[mode_ctx],
                     INTER_COMPOUND_OFFSET(mode), INTER_COMPOUND_MODES);
      } else {
        update_inter_mode_stats(fc, counts, mode, mode_ctx, allow_update_cdf);
      }

      int mode_allowed = (mbmi->mode == NEWMV);
      mode_allowed |= (mbmi->mode == NEW_NEWMV);
      if (mode_allowed) {
        uint8_t ref_frame_type = av1_ref_frame_type(mbmi->ref_frame);
        int idx;

        for (idx = 0; idx < 2; ++idx) {
          if (mbmi_ext->ref_mv_count[ref_frame_type] > idx + 1) {
#if CONFIG_ENTROPY_STATS
            uint8_t drl_ctx =
                av1_drl_ctx(mbmi_ext->ref_mv_stack[ref_frame_type], idx);
            ++counts->drl_mode[drl_ctx][mbmi->ref_mv_idx != idx];
#endif

            if (mbmi->ref_mv_idx == idx) break;
          }
        }
      }

      if (have_nearmv_in_inter_mode(mbmi->mode)) {
        uint8_t ref_frame_type = av1_ref_frame_type(mbmi->ref_frame);
        int idx;

        for (idx = 1; idx < 3; ++idx) {
          if (mbmi_ext->ref_mv_count[ref_frame_type] > idx + 1) {
#if CONFIG_ENTROPY_STATS
            uint8_t drl_ctx =
                av1_drl_ctx(mbmi_ext->ref_mv_stack[ref_frame_type], idx);
            ++counts->drl_mode[drl_ctx][mbmi->ref_mv_idx != idx - 1];
#endif

            if (mbmi->ref_mv_idx == idx - 1) break;
          }
        }
      }
    }
  }
}

typedef struct {
  ENTROPY_CONTEXT a[MAX_MIB_SIZE * MAX_MB_PLANE];
  ENTROPY_CONTEXT l[MAX_MIB_SIZE * MAX_MB_PLANE];
  PARTITION_CONTEXT sa[MAX_MIB_SIZE];
  PARTITION_CONTEXT sl[MAX_MIB_SIZE];
  TXFM_CONTEXT *p_ta;
  TXFM_CONTEXT *p_tl;
  TXFM_CONTEXT ta[MAX_MIB_SIZE];
  TXFM_CONTEXT tl[MAX_MIB_SIZE];
} RD_SEARCH_MACROBLOCK_CONTEXT;

static void restore_context(MACROBLOCK *x,
                            const RD_SEARCH_MACROBLOCK_CONTEXT *ctx, int mi_row,
                            int mi_col, BLOCK_SIZE bsize,
                            const int num_planes) {
  MACROBLOCKD *xd = &x->e_mbd;
  int p;
  const int num_4x4_blocks_wide =
      block_size_wide[bsize] >> tx_size_wide_log2[0];
  const int num_4x4_blocks_high =
      block_size_high[bsize] >> tx_size_high_log2[0];
  int mi_width = mi_size_wide[bsize];
  int mi_height = mi_size_high[bsize];
  for (p = 0; p < num_planes; p++) {
    int tx_col = mi_col;
    int tx_row = mi_row & MAX_MIB_MASK;
    memcpy(xd->above_context[p] + (tx_col >> xd->plane[p].subsampling_x),
           ctx->a + num_4x4_blocks_wide * p,
           (sizeof(ENTROPY_CONTEXT) * num_4x4_blocks_wide) >>
               xd->plane[p].subsampling_x);
    memcpy(xd->left_context[p] + (tx_row >> xd->plane[p].subsampling_y),
           ctx->l + num_4x4_blocks_high * p,
           (sizeof(ENTROPY_CONTEXT) * num_4x4_blocks_high) >>
               xd->plane[p].subsampling_y);
  }
  memcpy(xd->above_seg_context + mi_col, ctx->sa,
         sizeof(*xd->above_seg_context) * mi_width);
  memcpy(xd->left_seg_context + (mi_row & MAX_MIB_MASK), ctx->sl,
         sizeof(xd->left_seg_context[0]) * mi_height);
  xd->above_txfm_context = ctx->p_ta;
  xd->left_txfm_context = ctx->p_tl;
  memcpy(xd->above_txfm_context, ctx->ta,
         sizeof(*xd->above_txfm_context) * mi_width);
  memcpy(xd->left_txfm_context, ctx->tl,
         sizeof(*xd->left_txfm_context) * mi_height);
}

static void save_context(const MACROBLOCK *x, RD_SEARCH_MACROBLOCK_CONTEXT *ctx,
                         int mi_row, int mi_col, BLOCK_SIZE bsize,
                         const int num_planes) {
  const MACROBLOCKD *xd = &x->e_mbd;
  int p;
  const int num_4x4_blocks_wide =
      block_size_wide[bsize] >> tx_size_wide_log2[0];
  const int num_4x4_blocks_high =
      block_size_high[bsize] >> tx_size_high_log2[0];
  int mi_width = mi_size_wide[bsize];
  int mi_height = mi_size_high[bsize];

  // buffer the above/left context information of the block in search.
  for (p = 0; p < num_planes; ++p) {
    int tx_col = mi_col;
    int tx_row = mi_row & MAX_MIB_MASK;
    memcpy(ctx->a + num_4x4_blocks_wide * p,
           xd->above_context[p] + (tx_col >> xd->plane[p].subsampling_x),
           (sizeof(ENTROPY_CONTEXT) * num_4x4_blocks_wide) >>
               xd->plane[p].subsampling_x);
    memcpy(ctx->l + num_4x4_blocks_high * p,
           xd->left_context[p] + (tx_row >> xd->plane[p].subsampling_y),
           (sizeof(ENTROPY_CONTEXT) * num_4x4_blocks_high) >>
               xd->plane[p].subsampling_y);
  }
  memcpy(ctx->sa, xd->above_seg_context + mi_col,
         sizeof(*xd->above_seg_context) * mi_width);
  memcpy(ctx->sl, xd->left_seg_context + (mi_row & MAX_MIB_MASK),
         sizeof(xd->left_seg_context[0]) * mi_height);
  memcpy(ctx->ta, xd->above_txfm_context,
         sizeof(*xd->above_txfm_context) * mi_width);
  memcpy(ctx->tl, xd->left_txfm_context,
         sizeof(*xd->left_txfm_context) * mi_height);
  ctx->p_ta = xd->above_txfm_context;
  ctx->p_tl = xd->left_txfm_context;
}

static void encode_b(const AV1_COMP *const cpi, TileDataEnc *tile_data,
                     ThreadData *td, TOKENEXTRA **tp, int mi_row, int mi_col,
                     RUN_TYPE dry_run, BLOCK_SIZE bsize,
                     PARTITION_TYPE partition, PICK_MODE_CONTEXT *ctx,
                     int *rate) {
  TileInfo *const tile = &tile_data->tile_info;
  MACROBLOCK *const x = &td->mb;
  MACROBLOCKD *xd = &x->e_mbd;

  set_offsets(cpi, tile, x, mi_row, mi_col, bsize);
  MB_MODE_INFO *mbmi = xd->mi[0];
  mbmi->partition = partition;
  update_state(cpi, tile_data, td, ctx, mi_row, mi_col, bsize, dry_run);

  if (!dry_run) av1_set_coeff_buffer(cpi, x, mi_row, mi_col);

  encode_superblock(cpi, tile_data, td, tp, dry_run, mi_row, mi_col, bsize,
                    rate);

  if (dry_run == 0)
    x->cb_offset += block_size_wide[bsize] * block_size_high[bsize];

  if (!dry_run) {
    if (bsize == cpi->common.seq_params.sb_size && mbmi->skip == 1 &&
        cpi->common.delta_lf_present_flag) {
      const int frame_lf_count = av1_num_planes(&cpi->common) > 1
                                     ? FRAME_LF_COUNT
                                     : FRAME_LF_COUNT - 2;
      for (int lf_id = 0; lf_id < frame_lf_count; ++lf_id)
        mbmi->delta_lf[lf_id] = xd->delta_lf[lf_id];
      mbmi->delta_lf_from_base = xd->delta_lf_from_base;
    }
    if (has_second_ref(mbmi)) {
      if (mbmi->compound_idx == 0 ||
          mbmi->interinter_comp.type == COMPOUND_AVERAGE)
        mbmi->comp_group_idx = 0;
      else
        mbmi->comp_group_idx = 1;
    }
    update_stats(&cpi->common, tile_data, td, mi_row, mi_col);
  }
}

static void encode_sb(const AV1_COMP *const cpi, ThreadData *td,
                      TileDataEnc *tile_data, TOKENEXTRA **tp, int mi_row,
                      int mi_col, RUN_TYPE dry_run, BLOCK_SIZE bsize,
                      PC_TREE *pc_tree, int *rate) {
  const AV1_COMMON *const cm = &cpi->common;
  MACROBLOCK *const x = &td->mb;
  MACROBLOCKD *const xd = &x->e_mbd;
  const int hbs = mi_size_wide[bsize] / 2;
  const int is_partition_root = bsize >= BLOCK_8X8;
  const int ctx = is_partition_root
                      ? partition_plane_context(xd, mi_row, mi_col, bsize)
                      : -1;
  const PARTITION_TYPE partition = pc_tree->partitioning;
  const BLOCK_SIZE subsize = get_partition_subsize(bsize, partition);
  int quarter_step = mi_size_wide[bsize] / 4;
  int i;
  BLOCK_SIZE bsize2 = get_partition_subsize(bsize, PARTITION_SPLIT);

  if (mi_row >= cm->mi_rows || mi_col >= cm->mi_cols) return;

  if (!dry_run && ctx >= 0) {
    const int has_rows = (mi_row + hbs) < cm->mi_rows;
    const int has_cols = (mi_col + hbs) < cm->mi_cols;

    if (has_rows && has_cols) {
#if CONFIG_ENTROPY_STATS
      td->counts->partition[ctx][partition]++;
#endif

      if (tile_data->allow_update_cdf) {
        FRAME_CONTEXT *fc = xd->tile_ctx;
        update_cdf(fc->partition_cdf[ctx], partition,
                   partition_cdf_length(bsize));
      }
    }
  }

  switch (partition) {
    case PARTITION_NONE:
      encode_b(cpi, tile_data, td, tp, mi_row, mi_col, dry_run, subsize,
               partition, &pc_tree->none, rate);
      break;
    case PARTITION_VERT:
      encode_b(cpi, tile_data, td, tp, mi_row, mi_col, dry_run, subsize,
               partition, &pc_tree->vertical[0], rate);
      if (mi_col + hbs < cm->mi_cols) {
        encode_b(cpi, tile_data, td, tp, mi_row, mi_col + hbs, dry_run, subsize,
                 partition, &pc_tree->vertical[1], rate);
      }
      break;
    case PARTITION_HORZ:
      encode_b(cpi, tile_data, td, tp, mi_row, mi_col, dry_run, subsize,
               partition, &pc_tree->horizontal[0], rate);
      if (mi_row + hbs < cm->mi_rows) {
        encode_b(cpi, tile_data, td, tp, mi_row + hbs, mi_col, dry_run, subsize,
                 partition, &pc_tree->horizontal[1], rate);
      }
      break;
    case PARTITION_SPLIT:
      encode_sb(cpi, td, tile_data, tp, mi_row, mi_col, dry_run, subsize,
                pc_tree->split[0], rate);
      encode_sb(cpi, td, tile_data, tp, mi_row, mi_col + hbs, dry_run, subsize,
                pc_tree->split[1], rate);
      encode_sb(cpi, td, tile_data, tp, mi_row + hbs, mi_col, dry_run, subsize,
                pc_tree->split[2], rate);
      encode_sb(cpi, td, tile_data, tp, mi_row + hbs, mi_col + hbs, dry_run,
                subsize, pc_tree->split[3], rate);
      break;

    case PARTITION_HORZ_A:
      encode_b(cpi, tile_data, td, tp, mi_row, mi_col, dry_run, bsize2,
               partition, &pc_tree->horizontala[0], rate);
      encode_b(cpi, tile_data, td, tp, mi_row, mi_col + hbs, dry_run, bsize2,
               partition, &pc_tree->horizontala[1], rate);
      encode_b(cpi, tile_data, td, tp, mi_row + hbs, mi_col, dry_run, subsize,
               partition, &pc_tree->horizontala[2], rate);
      break;
    case PARTITION_HORZ_B:
      encode_b(cpi, tile_data, td, tp, mi_row, mi_col, dry_run, subsize,
               partition, &pc_tree->horizontalb[0], rate);
      encode_b(cpi, tile_data, td, tp, mi_row + hbs, mi_col, dry_run, bsize2,
               partition, &pc_tree->horizontalb[1], rate);
      encode_b(cpi, tile_data, td, tp, mi_row + hbs, mi_col + hbs, dry_run,
               bsize2, partition, &pc_tree->horizontalb[2], rate);
      break;
    case PARTITION_VERT_A:
      encode_b(cpi, tile_data, td, tp, mi_row, mi_col, dry_run, bsize2,
               partition, &pc_tree->verticala[0], rate);
      encode_b(cpi, tile_data, td, tp, mi_row + hbs, mi_col, dry_run, bsize2,
               partition, &pc_tree->verticala[1], rate);
      encode_b(cpi, tile_data, td, tp, mi_row, mi_col + hbs, dry_run, subsize,
               partition, &pc_tree->verticala[2], rate);

      break;
    case PARTITION_VERT_B:
      encode_b(cpi, tile_data, td, tp, mi_row, mi_col, dry_run, subsize,
               partition, &pc_tree->verticalb[0], rate);
      encode_b(cpi, tile_data, td, tp, mi_row, mi_col + hbs, dry_run, bsize2,
               partition, &pc_tree->verticalb[1], rate);
      encode_b(cpi, tile_data, td, tp, mi_row + hbs, mi_col + hbs, dry_run,
               bsize2, partition, &pc_tree->verticalb[2], rate);
      break;
    case PARTITION_HORZ_4:
      for (i = 0; i < 4; ++i) {
        int this_mi_row = mi_row + i * quarter_step;
        if (i > 0 && this_mi_row >= cm->mi_rows) break;

        encode_b(cpi, tile_data, td, tp, this_mi_row, mi_col, dry_run, subsize,
                 partition, &pc_tree->horizontal4[i], rate);
      }
      break;
    case PARTITION_VERT_4:
      for (i = 0; i < 4; ++i) {
        int this_mi_col = mi_col + i * quarter_step;
        if (i > 0 && this_mi_col >= cm->mi_cols) break;

        encode_b(cpi, tile_data, td, tp, mi_row, this_mi_col, dry_run, subsize,
                 partition, &pc_tree->vertical4[i], rate);
      }
      break;
    default: assert(0 && "Invalid partition type."); break;
  }

  update_ext_partition_context(xd, mi_row, mi_col, subsize, bsize, partition);
}

// Check to see if the given partition size is allowed for a specified number
// of mi block rows and columns remaining in the image.
// If not then return the largest allowed partition size
static BLOCK_SIZE find_partition_size(BLOCK_SIZE bsize, int rows_left,
                                      int cols_left, int *bh, int *bw) {
  if (rows_left <= 0 || cols_left <= 0) {
    return AOMMIN(bsize, BLOCK_8X8);
  } else {
    for (; bsize > 0; bsize -= 3) {
      *bh = mi_size_high[bsize];
      *bw = mi_size_wide[bsize];
      if ((*bh <= rows_left) && (*bw <= cols_left)) {
        break;
      }
    }
  }
  return bsize;
}

static void set_partial_sb_partition(const AV1_COMMON *const cm,
                                     MB_MODE_INFO *mi, int bh_in, int bw_in,
                                     int mi_rows_remaining,
                                     int mi_cols_remaining, BLOCK_SIZE bsize,
                                     MB_MODE_INFO **mib) {
  int bh = bh_in;
  int r, c;
  for (r = 0; r < cm->seq_params.mib_size; r += bh) {
    int bw = bw_in;
    for (c = 0; c < cm->seq_params.mib_size; c += bw) {
      const int index = r * cm->mi_stride + c;
      mib[index] = mi + index;
      mib[index]->sb_type = find_partition_size(
          bsize, mi_rows_remaining - r, mi_cols_remaining - c, &bh, &bw);
    }
  }
}

// This function attempts to set all mode info entries in a given superblock
// to the same block partition size.
// However, at the bottom and right borders of the image the requested size
// may not be allowed in which case this code attempts to choose the largest
// allowable partition.
static void set_fixed_partitioning(AV1_COMP *cpi, const TileInfo *const tile,
                                   MB_MODE_INFO **mib, int mi_row, int mi_col,
                                   BLOCK_SIZE bsize) {
  AV1_COMMON *const cm = &cpi->common;
  const int mi_rows_remaining = tile->mi_row_end - mi_row;
  const int mi_cols_remaining = tile->mi_col_end - mi_col;
  int block_row, block_col;
  MB_MODE_INFO *const mi_upper_left = cm->mi + mi_row * cm->mi_stride + mi_col;
  int bh = mi_size_high[bsize];
  int bw = mi_size_wide[bsize];

  assert((mi_rows_remaining > 0) && (mi_cols_remaining > 0));

  // Apply the requested partition size to the SB if it is all "in image"
  if ((mi_cols_remaining >= cm->seq_params.mib_size) &&
      (mi_rows_remaining >= cm->seq_params.mib_size)) {
    for (block_row = 0; block_row < cm->seq_params.mib_size; block_row += bh) {
      for (block_col = 0; block_col < cm->seq_params.mib_size;
           block_col += bw) {
        int index = block_row * cm->mi_stride + block_col;
        mib[index] = mi_upper_left + index;
        mib[index]->sb_type = bsize;
      }
    }
  } else {
    // Else this is a partial SB.
    set_partial_sb_partition(cm, mi_upper_left, bh, bw, mi_rows_remaining,
                             mi_cols_remaining, bsize, mib);
  }
}

static void rd_use_partition(AV1_COMP *cpi, ThreadData *td,
                             TileDataEnc *tile_data, MB_MODE_INFO **mib,
                             TOKENEXTRA **tp, int mi_row, int mi_col,
                             BLOCK_SIZE bsize, int *rate, int64_t *dist,
                             int do_recon, PC_TREE *pc_tree) {
  AV1_COMMON *const cm = &cpi->common;
  const int num_planes = av1_num_planes(cm);
  TileInfo *const tile_info = &tile_data->tile_info;
  MACROBLOCK *const x = &td->mb;
  MACROBLOCKD *const xd = &x->e_mbd;
  const int bs = mi_size_wide[bsize];
  const int hbs = bs / 2;
  int i;
  const int pl = (bsize >= BLOCK_8X8)
                     ? partition_plane_context(xd, mi_row, mi_col, bsize)
                     : 0;
  const PARTITION_TYPE partition =
      (bsize >= BLOCK_8X8) ? get_partition(cm, mi_row, mi_col, bsize)
                           : PARTITION_NONE;
  const BLOCK_SIZE subsize = get_partition_subsize(bsize, partition);
  RD_SEARCH_MACROBLOCK_CONTEXT x_ctx;
  RD_STATS last_part_rdc, none_rdc, chosen_rdc;
  BLOCK_SIZE sub_subsize = BLOCK_4X4;
  int splits_below = 0;
  BLOCK_SIZE bs_type = mib[0]->sb_type;
  int do_partition_search = 1;
  PICK_MODE_CONTEXT *ctx_none = &pc_tree->none;

  if (mi_row >= cm->mi_rows || mi_col >= cm->mi_cols) return;

  assert(mi_size_wide[bsize] == mi_size_high[bsize]);

  av1_invalid_rd_stats(&last_part_rdc);
  av1_invalid_rd_stats(&none_rdc);
  av1_invalid_rd_stats(&chosen_rdc);

  pc_tree->partitioning = partition;

  xd->above_txfm_context = cm->above_txfm_context[tile_info->tile_row] + mi_col;
  xd->left_txfm_context =
      xd->left_txfm_context_buffer + (mi_row & MAX_MIB_MASK);
  save_context(x, &x_ctx, mi_row, mi_col, bsize, num_planes);

  if (bsize == BLOCK_16X16 && cpi->vaq_refresh) {
    set_offsets(cpi, tile_info, x, mi_row, mi_col, bsize);
    x->mb_energy = av1_block_energy(cpi, x, bsize);
  }

  if (do_partition_search &&
      cpi->sf.partition_search_type == SEARCH_PARTITION &&
      cpi->sf.adjust_partitioning_from_last_frame) {
    // Check if any of the sub blocks are further split.
    if (partition == PARTITION_SPLIT && subsize > BLOCK_8X8) {
      sub_subsize = get_partition_subsize(subsize, PARTITION_SPLIT);
      splits_below = 1;
      for (i = 0; i < 4; i++) {
        int jj = i >> 1, ii = i & 0x01;
        MB_MODE_INFO *this_mi = mib[jj * hbs * cm->mi_stride + ii * hbs];
        if (this_mi && this_mi->sb_type >= sub_subsize) {
          splits_below = 0;
        }
      }
    }

    // If partition is not none try none unless each of the 4 splits are split
    // even further..
    if (partition != PARTITION_NONE && !splits_below &&
        mi_row + hbs < cm->mi_rows && mi_col + hbs < cm->mi_cols) {
      pc_tree->partitioning = PARTITION_NONE;
      rd_pick_sb_modes(cpi, tile_data, x, mi_row, mi_col, &none_rdc,
                       PARTITION_NONE, bsize, ctx_none, INT64_MAX);

      if (none_rdc.rate < INT_MAX) {
        none_rdc.rate += x->partition_cost[pl][PARTITION_NONE];
        none_rdc.rdcost = RDCOST(x->rdmult, none_rdc.rate, none_rdc.dist);
      }

      restore_context(x, &x_ctx, mi_row, mi_col, bsize, num_planes);
      mib[0]->sb_type = bs_type;
      pc_tree->partitioning = partition;
    }
  }

  switch (partition) {
    case PARTITION_NONE:
      rd_pick_sb_modes(cpi, tile_data, x, mi_row, mi_col, &last_part_rdc,
                       PARTITION_NONE, bsize, ctx_none, INT64_MAX);
      break;
    case PARTITION_HORZ:
      rd_pick_sb_modes(cpi, tile_data, x, mi_row, mi_col, &last_part_rdc,
                       PARTITION_HORZ, subsize, &pc_tree->horizontal[0],
                       INT64_MAX);
      if (last_part_rdc.rate != INT_MAX && bsize >= BLOCK_8X8 &&
          mi_row + hbs < cm->mi_rows) {
        RD_STATS tmp_rdc;
        PICK_MODE_CONTEXT *ctx_h = &pc_tree->horizontal[0];
        av1_init_rd_stats(&tmp_rdc);
        update_state(cpi, tile_data, td, ctx_h, mi_row, mi_col, subsize, 1);
        encode_superblock(cpi, tile_data, td, tp, DRY_RUN_NORMAL, mi_row,
                          mi_col, subsize, NULL);
        rd_pick_sb_modes(cpi, tile_data, x, mi_row + hbs, mi_col, &tmp_rdc,
                         PARTITION_HORZ, subsize, &pc_tree->horizontal[1],
                         INT64_MAX);
        if (tmp_rdc.rate == INT_MAX || tmp_rdc.dist == INT64_MAX) {
          av1_invalid_rd_stats(&last_part_rdc);
          break;
        }
        last_part_rdc.rate += tmp_rdc.rate;
        last_part_rdc.dist += tmp_rdc.dist;
        last_part_rdc.rdcost += tmp_rdc.rdcost;
      }
      break;
    case PARTITION_VERT:
      rd_pick_sb_modes(cpi, tile_data, x, mi_row, mi_col, &last_part_rdc,
                       PARTITION_VERT, subsize, &pc_tree->vertical[0],
                       INT64_MAX);
      if (last_part_rdc.rate != INT_MAX && bsize >= BLOCK_8X8 &&
          mi_col + hbs < cm->mi_cols) {
        RD_STATS tmp_rdc;
        PICK_MODE_CONTEXT *ctx_v = &pc_tree->vertical[0];
        av1_init_rd_stats(&tmp_rdc);
        update_state(cpi, tile_data, td, ctx_v, mi_row, mi_col, subsize, 1);
        encode_superblock(cpi, tile_data, td, tp, DRY_RUN_NORMAL, mi_row,
                          mi_col, subsize, NULL);
        rd_pick_sb_modes(cpi, tile_data, x, mi_row, mi_col + hbs, &tmp_rdc,
                         PARTITION_VERT, subsize,
                         &pc_tree->vertical[bsize > BLOCK_8X8], INT64_MAX);
        if (tmp_rdc.rate == INT_MAX || tmp_rdc.dist == INT64_MAX) {
          av1_invalid_rd_stats(&last_part_rdc);
          break;
        }
        last_part_rdc.rate += tmp_rdc.rate;
        last_part_rdc.dist += tmp_rdc.dist;
        last_part_rdc.rdcost += tmp_rdc.rdcost;
      }
      break;
    case PARTITION_SPLIT:
      last_part_rdc.rate = 0;
      last_part_rdc.dist = 0;
      last_part_rdc.rdcost = 0;
      for (i = 0; i < 4; i++) {
        int x_idx = (i & 1) * hbs;
        int y_idx = (i >> 1) * hbs;
        int jj = i >> 1, ii = i & 0x01;
        RD_STATS tmp_rdc;
        if ((mi_row + y_idx >= cm->mi_rows) || (mi_col + x_idx >= cm->mi_cols))
          continue;

        av1_init_rd_stats(&tmp_rdc);
        rd_use_partition(cpi, td, tile_data,
                         mib + jj * hbs * cm->mi_stride + ii * hbs, tp,
                         mi_row + y_idx, mi_col + x_idx, subsize, &tmp_rdc.rate,
                         &tmp_rdc.dist, i != 3, pc_tree->split[i]);
        if (tmp_rdc.rate == INT_MAX || tmp_rdc.dist == INT64_MAX) {
          av1_invalid_rd_stats(&last_part_rdc);
          break;
        }
        last_part_rdc.rate += tmp_rdc.rate;
        last_part_rdc.dist += tmp_rdc.dist;
      }
      break;
    case PARTITION_VERT_A:
    case PARTITION_VERT_B:
    case PARTITION_HORZ_A:
    case PARTITION_HORZ_B:
    case PARTITION_HORZ_4:
    case PARTITION_VERT_4: assert(0 && "Cannot handle extended partiton types");
    default: assert(0); break;
  }

  if (last_part_rdc.rate < INT_MAX) {
    last_part_rdc.rate += x->partition_cost[pl][partition];
    last_part_rdc.rdcost =
        RDCOST(x->rdmult, last_part_rdc.rate, last_part_rdc.dist);
  }

  if (do_partition_search && cpi->sf.adjust_partitioning_from_last_frame &&
      cpi->sf.partition_search_type == SEARCH_PARTITION &&
      partition != PARTITION_SPLIT && bsize > BLOCK_8X8 &&
      (mi_row + bs < cm->mi_rows || mi_row + hbs == cm->mi_rows) &&
      (mi_col + bs < cm->mi_cols || mi_col + hbs == cm->mi_cols)) {
    BLOCK_SIZE split_subsize = get_partition_subsize(bsize, PARTITION_SPLIT);
    chosen_rdc.rate = 0;
    chosen_rdc.dist = 0;

    restore_context(x, &x_ctx, mi_row, mi_col, bsize, num_planes);
    pc_tree->partitioning = PARTITION_SPLIT;

    // Split partition.
    for (i = 0; i < 4; i++) {
      int x_idx = (i & 1) * hbs;
      int y_idx = (i >> 1) * hbs;
      RD_STATS tmp_rdc;

      if ((mi_row + y_idx >= cm->mi_rows) || (mi_col + x_idx >= cm->mi_cols))
        continue;

      save_context(x, &x_ctx, mi_row, mi_col, bsize, num_planes);
      pc_tree->split[i]->partitioning = PARTITION_NONE;
      rd_pick_sb_modes(cpi, tile_data, x, mi_row + y_idx, mi_col + x_idx,
                       &tmp_rdc, PARTITION_SPLIT, split_subsize,
                       &pc_tree->split[i]->none, INT64_MAX);

      restore_context(x, &x_ctx, mi_row, mi_col, bsize, num_planes);
      if (tmp_rdc.rate == INT_MAX || tmp_rdc.dist == INT64_MAX) {
        av1_invalid_rd_stats(&chosen_rdc);
        break;
      }

      chosen_rdc.rate += tmp_rdc.rate;
      chosen_rdc.dist += tmp_rdc.dist;

      if (i != 3)
        encode_sb(cpi, td, tile_data, tp, mi_row + y_idx, mi_col + x_idx,
                  OUTPUT_ENABLED, split_subsize, pc_tree->split[i], NULL);

      chosen_rdc.rate += x->partition_cost[pl][PARTITION_NONE];
    }
    if (chosen_rdc.rate < INT_MAX) {
      chosen_rdc.rate += x->partition_cost[pl][PARTITION_SPLIT];
      chosen_rdc.rdcost = RDCOST(x->rdmult, chosen_rdc.rate, chosen_rdc.dist);
    }
  }

  // If last_part is better set the partitioning to that.
  if (last_part_rdc.rdcost < chosen_rdc.rdcost) {
    mib[0]->sb_type = bsize;
    if (bsize >= BLOCK_8X8) pc_tree->partitioning = partition;
    chosen_rdc = last_part_rdc;
  }
  // If none was better set the partitioning to that.
  if (none_rdc.rdcost < chosen_rdc.rdcost) {
    if (bsize >= BLOCK_8X8) pc_tree->partitioning = PARTITION_NONE;
    chosen_rdc = none_rdc;
  }

  restore_context(x, &x_ctx, mi_row, mi_col, bsize, num_planes);

  // We must have chosen a partitioning and encoding or we'll fail later on.
  // No other opportunities for success.
  if (bsize == cm->seq_params.sb_size)
    assert(chosen_rdc.rate < INT_MAX && chosen_rdc.dist < INT64_MAX);

  if (do_recon) {
    if (bsize == cm->seq_params.sb_size) {
      // NOTE: To get estimate for rate due to the tokens, use:
      // int rate_coeffs = 0;
      // encode_sb(cpi, td, tile_data, tp, mi_row, mi_col, DRY_RUN_COSTCOEFFS,
      //           bsize, pc_tree, &rate_coeffs);
      x->cb_offset = 0;
      encode_sb(cpi, td, tile_data, tp, mi_row, mi_col, OUTPUT_ENABLED, bsize,
                pc_tree, NULL);
    } else {
      encode_sb(cpi, td, tile_data, tp, mi_row, mi_col, DRY_RUN_NORMAL, bsize,
                pc_tree, NULL);
    }
  }

  *rate = chosen_rdc.rate;
  *dist = chosen_rdc.dist;
}

/* clang-format off */
static const BLOCK_SIZE min_partition_size[BLOCK_SIZES_ALL] = {
                            BLOCK_4X4,    //                     4x4
  BLOCK_4X4,   BLOCK_4X4,   BLOCK_4X4,    //    4x8,    8x4,     8x8
  BLOCK_4X4,   BLOCK_4X4,   BLOCK_8X8,    //   8x16,   16x8,   16x16
  BLOCK_8X8,   BLOCK_8X8,   BLOCK_16X16,  //  16x32,  32x16,   32x32
  BLOCK_16X16, BLOCK_16X16, BLOCK_16X16,  //  32x64,  64x32,   64x64
  BLOCK_16X16, BLOCK_16X16, BLOCK_16X16,  // 64x128, 128x64, 128x128
  BLOCK_4X4,   BLOCK_4X4,   BLOCK_8X8,    //   4x16,   16x4,    8x32
  BLOCK_8X8,   BLOCK_16X16, BLOCK_16X16,  //   32x8,  16x64,   64x16
};

static const BLOCK_SIZE max_partition_size[BLOCK_SIZES_ALL] = {
                                  BLOCK_8X8,    //                     4x4
  BLOCK_16X16,   BLOCK_16X16,   BLOCK_16X16,    //    4x8,    8x4,     8x8
  BLOCK_32X32,   BLOCK_32X32,   BLOCK_32X32,    //   8x16,   16x8,   16x16
  BLOCK_64X64,   BLOCK_64X64,   BLOCK_64X64,    //  16x32,  32x16,   32x32
  BLOCK_LARGEST, BLOCK_LARGEST, BLOCK_LARGEST,  //  32x64,  64x32,   64x64
  BLOCK_LARGEST, BLOCK_LARGEST, BLOCK_LARGEST,  // 64x128, 128x64, 128x128
  BLOCK_16X16,   BLOCK_16X16,   BLOCK_32X32,    //   4x16,   16x4,    8x32
  BLOCK_32X32,   BLOCK_LARGEST, BLOCK_LARGEST,  //   32x8,  16x64,   64x16
};

// Next square block size less or equal than current block size.
static const BLOCK_SIZE next_square_size[BLOCK_SIZES_ALL] = {
                              BLOCK_4X4,    //                     4x4
  BLOCK_4X4,   BLOCK_4X4,     BLOCK_8X8,    //    4x8,    8x4,     8x8
  BLOCK_8X8,   BLOCK_8X8,     BLOCK_16X16,  //   8x16,   16x8,   16x16
  BLOCK_16X16, BLOCK_16X16,   BLOCK_32X32,  //  16x32,  32x16,   32x32
  BLOCK_32X32, BLOCK_32X32,   BLOCK_64X64,  //  32x64,  64x32,   64x64
  BLOCK_64X64, BLOCK_64X64, BLOCK_128X128,  // 64x128, 128x64, 128x128
  BLOCK_4X4,   BLOCK_4X4,   BLOCK_8X8,      //   4x16,   16x4,    8x32
  BLOCK_8X8,   BLOCK_16X16, BLOCK_16X16,    //   32x8,  16x64,   64x16
};
/* clang-format on */

// Look at all the mode_info entries for blocks that are part of this
// partition and find the min and max values for sb_type.
// At the moment this is designed to work on a superblock but could be
// adjusted to use a size parameter.
//
// The min and max are assumed to have been initialized prior to calling this
// function so repeat calls can accumulate a min and max of more than one
// superblock.
static void get_sb_partition_size_range(const AV1_COMMON *const cm,
                                        MACROBLOCKD *xd, MB_MODE_INFO **mib,
                                        BLOCK_SIZE *min_block_size,
                                        BLOCK_SIZE *max_block_size) {
  int i, j;
  int index = 0;

  // Check the sb_type for each block that belongs to this region.
  for (i = 0; i < cm->seq_params.mib_size; ++i) {
    for (j = 0; j < cm->seq_params.mib_size; ++j) {
      MB_MODE_INFO *mi = mib[index + j];
      BLOCK_SIZE sb_type = mi ? mi->sb_type : BLOCK_4X4;
      *min_block_size = AOMMIN(*min_block_size, sb_type);
      *max_block_size = AOMMAX(*max_block_size, sb_type);
    }
    index += xd->mi_stride;
  }
}

// Checks to see if a super block is on a horizontal image edge.
// In most cases this is the "real" edge unless there are formatting
// bars embedded in the stream.
static int active_h_edge(const AV1_COMP *cpi, int mi_row, int mi_step) {
  int top_edge = 0;
  int bottom_edge = cpi->common.mi_rows;
  int is_active_h_edge = 0;

  // For two pass account for any formatting bars detected.
  if (cpi->oxcf.pass == 2) {
    const TWO_PASS *const twopass = &cpi->twopass;

    // The inactive region is specified in MBs not mi units.
    // The image edge is in the following MB row.
    top_edge += (int)(twopass->this_frame_stats.inactive_zone_rows * 2);

    bottom_edge -= (int)(twopass->this_frame_stats.inactive_zone_rows * 2);
    bottom_edge = AOMMAX(top_edge, bottom_edge);
  }

  if (((top_edge >= mi_row) && (top_edge < (mi_row + mi_step))) ||
      ((bottom_edge >= mi_row) && (bottom_edge < (mi_row + mi_step)))) {
    is_active_h_edge = 1;
  }
  return is_active_h_edge;
}

// Checks to see if a super block is on a vertical image edge.
// In most cases this is the "real" edge unless there are formatting
// bars embedded in the stream.
static int active_v_edge(const AV1_COMP *cpi, int mi_col, int mi_step) {
  int left_edge = 0;
  int right_edge = cpi->common.mi_cols;
  int is_active_v_edge = 0;

  // For two pass account for any formatting bars detected.
  if (cpi->oxcf.pass == 2) {
    const TWO_PASS *const twopass = &cpi->twopass;

    // The inactive region is specified in MBs not mi units.
    // The image edge is in the following MB row.
    left_edge += (int)(twopass->this_frame_stats.inactive_zone_cols * 2);

    right_edge -= (int)(twopass->this_frame_stats.inactive_zone_cols * 2);
    right_edge = AOMMAX(left_edge, right_edge);
  }

  if (((left_edge >= mi_col) && (left_edge < (mi_col + mi_step))) ||
      ((right_edge >= mi_col) && (right_edge < (mi_col + mi_step)))) {
    is_active_v_edge = 1;
  }
  return is_active_v_edge;
}

// Checks to see if a super block is at the edge of the active image.
// In most cases this is the "real" edge unless there are formatting
// bars embedded in the stream.
static int active_edge_sb(const AV1_COMP *cpi, int mi_row, int mi_col) {
  return active_h_edge(cpi, mi_row, cpi->common.seq_params.mib_size) ||
         active_v_edge(cpi, mi_col, cpi->common.seq_params.mib_size);
}

// Look at neighboring blocks and set a min and max partition size based on
// what they chose.
static void rd_auto_partition_range(AV1_COMP *cpi, const TileInfo *const tile,
                                    MACROBLOCKD *const xd, int mi_row,
                                    int mi_col, BLOCK_SIZE *min_block_size,
                                    BLOCK_SIZE *max_block_size) {
  AV1_COMMON *const cm = &cpi->common;
  MB_MODE_INFO **mi = xd->mi;
  const int left_in_image = xd->left_available && mi[-1];
  const int above_in_image = xd->up_available && mi[-xd->mi_stride];
  const int mi_rows_remaining = tile->mi_row_end - mi_row;
  const int mi_cols_remaining = tile->mi_col_end - mi_col;
  int bh, bw;
  BLOCK_SIZE min_size = BLOCK_4X4;
  BLOCK_SIZE max_size = BLOCK_LARGEST;

  // Trap case where we do not have a prediction.
  if (left_in_image || above_in_image || cm->frame_type != KEY_FRAME) {
    // Default "min to max" and "max to min"
    min_size = BLOCK_LARGEST;
    max_size = BLOCK_4X4;

    // NOTE: each call to get_sb_partition_size_range() uses the previous
    // passed in values for min and max as a starting point.
    // Find the min and max partition used in previous frame at this location
    if (cm->frame_type != KEY_FRAME) {
      MB_MODE_INFO **prev_mi =
          &cm->prev_mi_grid_visible[mi_row * xd->mi_stride + mi_col];
      get_sb_partition_size_range(cm, xd, prev_mi, &min_size, &max_size);
    }
    // Find the min and max partition sizes used in the left superblock
    if (left_in_image) {
      MB_MODE_INFO **left_sb_mi = &mi[-cm->seq_params.mib_size];
      get_sb_partition_size_range(cm, xd, left_sb_mi, &min_size, &max_size);
    }
    // Find the min and max partition sizes used in the above suprblock.
    if (above_in_image) {
      MB_MODE_INFO **above_sb_mi =
          &mi[-xd->mi_stride * cm->seq_params.mib_size];
      get_sb_partition_size_range(cm, xd, above_sb_mi, &min_size, &max_size);
    }

    // Adjust observed min and max for "relaxed" auto partition case.
    if (cpi->sf.auto_min_max_partition_size == RELAXED_NEIGHBORING_MIN_MAX) {
      min_size = min_partition_size[min_size];
      max_size = max_partition_size[max_size];
    }
  }

  // Check border cases where max and min from neighbors may not be legal.
  max_size = find_partition_size(max_size, mi_rows_remaining, mi_cols_remaining,
                                 &bh, &bw);
  min_size = AOMMIN(min_size, max_size);

  // Test for blocks at the edge of the active image.
  // This may be the actual edge of the image or where there are formatting
  // bars.
  if (active_edge_sb(cpi, mi_row, mi_col)) {
    min_size = BLOCK_4X4;
  } else {
    min_size = AOMMIN(cpi->sf.rd_auto_partition_min_limit, min_size);
  }

  // When use_square_partition_only is true, make sure at least one square
  // partition is allowed by selecting the next smaller square size as
  // *min_block_size.
  if (cpi->sf.use_square_partition_only) {
    min_size = AOMMIN(min_size, next_square_size[max_size]);
  }

  *min_block_size = AOMMIN(min_size, cm->seq_params.sb_size);
  *max_block_size = AOMMIN(max_size, cm->seq_params.sb_size);
}

// TODO(jingning) refactor functions setting partition search range
static void set_partition_range(const AV1_COMMON *const cm,
                                const MACROBLOCKD *const xd, int mi_row,
                                int mi_col, BLOCK_SIZE bsize,
                                BLOCK_SIZE *const min_bs,
                                BLOCK_SIZE *const max_bs) {
  const int mi_width = mi_size_wide[bsize];
  const int mi_height = mi_size_high[bsize];
  int idx, idy;

  const int idx_str = cm->mi_stride * mi_row + mi_col;
  MB_MODE_INFO **const prev_mi = &cm->prev_mi_grid_visible[idx_str];
  BLOCK_SIZE min_size = cm->seq_params.sb_size;  // default values
  BLOCK_SIZE max_size = BLOCK_4X4;

  if (prev_mi) {
    for (idy = 0; idy < mi_height; ++idy) {
      for (idx = 0; idx < mi_width; ++idx) {
        const MB_MODE_INFO *const mi = prev_mi[idy * cm->mi_stride + idx];
        const BLOCK_SIZE bs = mi ? mi->sb_type : bsize;
        min_size = AOMMIN(min_size, bs);
        max_size = AOMMAX(max_size, bs);
      }
    }
  }

  if (xd->left_available) {
    for (idy = 0; idy < mi_height; ++idy) {
      const MB_MODE_INFO *const mi = xd->mi[idy * cm->mi_stride - 1];
      const BLOCK_SIZE bs = mi ? mi->sb_type : bsize;
      min_size = AOMMIN(min_size, bs);
      max_size = AOMMAX(max_size, bs);
    }
  }

  if (xd->up_available) {
    for (idx = 0; idx < mi_width; ++idx) {
      const MB_MODE_INFO *const mi = xd->mi[idx - cm->mi_stride];
      const BLOCK_SIZE bs = mi ? mi->sb_type : bsize;
      min_size = AOMMIN(min_size, bs);
      max_size = AOMMAX(max_size, bs);
    }
  }

  if (min_size == max_size) {
    min_size = min_partition_size[min_size];
    max_size = max_partition_size[max_size];
  }

  *min_bs = AOMMIN(min_size, cm->seq_params.sb_size);
  *max_bs = AOMMIN(max_size, cm->seq_params.sb_size);
}

static INLINE void store_pred_mv(MACROBLOCK *x, PICK_MODE_CONTEXT *ctx) {
  memcpy(ctx->pred_mv, x->pred_mv, sizeof(x->pred_mv));
}

static INLINE void load_pred_mv(MACROBLOCK *x, PICK_MODE_CONTEXT *ctx) {
  memcpy(x->pred_mv, ctx->pred_mv, sizeof(x->pred_mv));
}

#if CONFIG_FP_MB_STATS
const int qindex_skip_threshold_lookup[BLOCK_SIZES] = {
  0, 10, 10, 30, 40, 40, 60, 80, 80, 90, 100, 100, 120,
  // TODO(debargha): What are the correct numbers here?
  130, 130, 150
};
const int qindex_split_threshold_lookup[BLOCK_SIZES] = {
  0, 3, 3, 7, 15, 15, 30, 40, 40, 60, 80, 80, 120,
  // TODO(debargha): What are the correct numbers here?
  160, 160, 240
};
const int complexity_16x16_blocks_threshold[BLOCK_SIZES] = {
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 4, 4, 6,
  // TODO(debargha): What are the correct numbers here?
  8, 8, 10
};

typedef enum {
  MV_ZERO = 0,
  MV_LEFT = 1,
  MV_UP = 2,
  MV_RIGHT = 3,
  MV_DOWN = 4,
  MV_INVALID
} MOTION_DIRECTION;

static INLINE MOTION_DIRECTION get_motion_direction_fp(uint8_t fp_byte) {
  if (fp_byte & FPMB_MOTION_ZERO_MASK) {
    return MV_ZERO;
  } else if (fp_byte & FPMB_MOTION_LEFT_MASK) {
    return MV_LEFT;
  } else if (fp_byte & FPMB_MOTION_RIGHT_MASK) {
    return MV_RIGHT;
  } else if (fp_byte & FPMB_MOTION_UP_MASK) {
    return MV_UP;
  } else {
    return MV_DOWN;
  }
}

static INLINE int get_motion_inconsistency(MOTION_DIRECTION this_mv,
                                           MOTION_DIRECTION that_mv) {
  if (this_mv == that_mv) {
    return 0;
  } else {
    return abs(this_mv - that_mv) == 2 ? 2 : 1;
  }
}
#endif

// Try searching for an encoding for the given subblock. Returns zero if the
// rdcost is already too high (to tell the caller not to bother searching for
// encodings of further subblocks)
static int rd_try_subblock(const AV1_COMP *const cpi, ThreadData *td,
                           TileDataEnc *tile_data, TOKENEXTRA **tp,
                           int is_first, int is_last, int mi_row, int mi_col,
                           BLOCK_SIZE subsize, RD_STATS *best_rdc,
                           RD_STATS *sum_rdc, RD_STATS *this_rdc,
                           PARTITION_TYPE partition,
                           PICK_MODE_CONTEXT *prev_ctx,
                           PICK_MODE_CONTEXT *this_ctx) {
#define RTS_X_RATE_NOCOEF_ARG
#define RTS_MAX_RDCOST best_rdc->rdcost

  MACROBLOCK *const x = &td->mb;

  if (cpi->sf.adaptive_motion_search) load_pred_mv(x, prev_ctx);

  // On the first time around, write the rd stats straight to sum_rdc. Also, we
  // should treat sum_rdc as containing zeros (even if it doesn't) to avoid
  // having to zero it at the start.
  if (is_first) this_rdc = sum_rdc;
  const int64_t spent_rdcost = is_first ? 0 : sum_rdc->rdcost;
  const int64_t rdcost_remaining = best_rdc->rdcost - spent_rdcost;

  rd_pick_sb_modes(cpi, tile_data, x, mi_row, mi_col, this_rdc,
                   RTS_X_RATE_NOCOEF_ARG partition, subsize, this_ctx,
                   rdcost_remaining);

  if (!is_first) {
    if (this_rdc->rate == INT_MAX) {
      sum_rdc->rdcost = INT64_MAX;
    } else {
      sum_rdc->rate += this_rdc->rate;
      sum_rdc->dist += this_rdc->dist;
      sum_rdc->rdcost += this_rdc->rdcost;
    }
  }

  if (sum_rdc->rdcost >= RTS_MAX_RDCOST) return 0;

  if (!is_last) {
    update_state(cpi, tile_data, td, this_ctx, mi_row, mi_col, subsize, 1);
    encode_superblock(cpi, tile_data, td, tp, DRY_RUN_NORMAL, mi_row, mi_col,
                      subsize, NULL);
  }

  return 1;

#undef RTS_X_RATE_NOCOEF_ARG
#undef RTS_MAX_RDCOST
}

static void rd_test_partition3(const AV1_COMP *const cpi, ThreadData *td,
                               TileDataEnc *tile_data, TOKENEXTRA **tp,
                               PC_TREE *pc_tree, RD_STATS *best_rdc,
                               PICK_MODE_CONTEXT ctxs[3],
                               PICK_MODE_CONTEXT *ctx, int mi_row, int mi_col,
                               BLOCK_SIZE bsize, PARTITION_TYPE partition,
                               int mi_row0, int mi_col0, BLOCK_SIZE subsize0,
                               int mi_row1, int mi_col1, BLOCK_SIZE subsize1,
                               int mi_row2, int mi_col2, BLOCK_SIZE subsize2) {
  MACROBLOCK *const x = &td->mb;
  MACROBLOCKD *const xd = &x->e_mbd;
  RD_STATS sum_rdc, this_rdc;
#define RTP_STX_TRY_ARGS

  if (!rd_try_subblock(cpi, td, tile_data, tp, 1, 0, mi_row0, mi_col0, subsize0,
                       best_rdc, &sum_rdc, &this_rdc,
                       RTP_STX_TRY_ARGS partition, ctx, &ctxs[0]))
    return;

  if (!rd_try_subblock(cpi, td, tile_data, tp, 0, 0, mi_row1, mi_col1, subsize1,
                       best_rdc, &sum_rdc, &this_rdc,
                       RTP_STX_TRY_ARGS partition, &ctxs[0], &ctxs[1]))
    return;

  // With the new layout of mixed partitions for PARTITION_HORZ_B and
  // PARTITION_VERT_B, the last subblock might start past halfway through the
  // main block, so we might signal it even though the subblock lies strictly
  // outside the image. In that case, we won't spend any bits coding it and the
  // difference (obviously) doesn't contribute to the error.
  const int try_block2 = 1;
  if (try_block2 &&
      !rd_try_subblock(cpi, td, tile_data, tp, 0, 1, mi_row2, mi_col2, subsize2,
                       best_rdc, &sum_rdc, &this_rdc,
                       RTP_STX_TRY_ARGS partition, &ctxs[1], &ctxs[2]))
    return;

  if (sum_rdc.rdcost >= best_rdc->rdcost) return;

  int pl = partition_plane_context(xd, mi_row, mi_col, bsize);
  sum_rdc.rate += x->partition_cost[pl][partition];
  sum_rdc.rdcost = RDCOST(x->rdmult, sum_rdc.rate, sum_rdc.dist);

  if (sum_rdc.rdcost >= best_rdc->rdcost) return;

  *best_rdc = sum_rdc;
  pc_tree->partitioning = partition;

#undef RTP_STX_TRY_ARGS
}

#if CONFIG_DIST_8X8
static int64_t dist_8x8_yuv(const AV1_COMP *const cpi, MACROBLOCK *const x,
                            uint8_t *src_plane_8x8[MAX_MB_PLANE],
                            uint8_t *dst_plane_8x8[MAX_MB_PLANE]) {
  const AV1_COMMON *const cm = &cpi->common;
  const int num_planes = av1_num_planes(cm);
  MACROBLOCKD *const xd = &x->e_mbd;
  int64_t dist_8x8, dist_8x8_uv, total_dist;
  const int src_stride = x->plane[0].src.stride;
  int plane;

  const int dst_stride = xd->plane[0].dst.stride;
  dist_8x8 =
      av1_dist_8x8(cpi, x, src_plane_8x8[0], src_stride, dst_plane_8x8[0],
                   dst_stride, BLOCK_8X8, 8, 8, 8, 8, x->qindex)
      << 4;

  // Compute chroma distortion for a luma 8x8 block
  dist_8x8_uv = 0;

  if (num_planes > 1) {
    for (plane = 1; plane < MAX_MB_PLANE; ++plane) {
      unsigned sse;
      const int src_stride_uv = x->plane[plane].src.stride;
      const int dst_stride_uv = xd->plane[plane].dst.stride;
      const int ssx = xd->plane[plane].subsampling_x;
      const int ssy = xd->plane[plane].subsampling_y;
      const BLOCK_SIZE plane_bsize = get_plane_block_size(BLOCK_8X8, ssx, ssy);

      cpi->fn_ptr[plane_bsize].vf(src_plane_8x8[plane], src_stride_uv,
                                  dst_plane_8x8[plane], dst_stride_uv, &sse);
      dist_8x8_uv += (int64_t)sse << 4;
    }
  }

  return total_dist = dist_8x8 + dist_8x8_uv;
}
#endif  // CONFIG_DIST_8X8

static void reset_partition(PC_TREE *pc_tree, BLOCK_SIZE bsize) {
  pc_tree->partitioning = PARTITION_NONE;
  pc_tree->cb_search_range = SEARCH_FULL_PLANE;

  if (bsize >= BLOCK_8X8) {
    BLOCK_SIZE subsize = get_partition_subsize(bsize, PARTITION_SPLIT);
    for (int idx = 0; idx < 4; ++idx)
      reset_partition(pc_tree->split[idx], subsize);
  }
}

static void rd_pick_sqr_partition(const AV1_COMP *const cpi, ThreadData *td,
                                  TileDataEnc *tile_data, TOKENEXTRA **tp,
                                  int mi_row, int mi_col, BLOCK_SIZE bsize,
                                  RD_STATS *rd_cost, int64_t best_rd,
                                  PC_TREE *pc_tree, int64_t *none_rd) {
  const AV1_COMMON *const cm = &cpi->common;
  TileInfo *const tile_info = &tile_data->tile_info;
  MACROBLOCK *const x = &td->mb;
  MACROBLOCKD *const xd = &x->e_mbd;
  const int mi_step = mi_size_wide[bsize] / 2;
  RD_SEARCH_MACROBLOCK_CONTEXT x_ctx;
  const TOKENEXTRA *const tp_orig = *tp;
  PICK_MODE_CONTEXT *ctx_none = &pc_tree->none;
  int tmp_partition_cost[PARTITION_TYPES];
  BLOCK_SIZE subsize;
  RD_STATS this_rdc, sum_rdc, best_rdc, pn_rdc;
  const int bsize_at_least_8x8 = (bsize >= BLOCK_8X8);
  int do_square_split = bsize_at_least_8x8;
  const int pl = bsize_at_least_8x8
                     ? partition_plane_context(xd, mi_row, mi_col, bsize)
                     : 0;
  const int *partition_cost =
      pl >= 0 ? x->partition_cost[pl] : x->partition_cost[0];
  const int num_planes = av1_num_planes(cm);

  int64_t split_rd[4] = { 0, 0, 0, 0 };

  // Override skipping rectangular partition operations for edge blocks
  const int has_rows = (mi_row + mi_step < cm->mi_rows);
  const int has_cols = (mi_col + mi_step < cm->mi_cols);

  if (none_rd) *none_rd = 0;

  int partition_none_allowed = has_rows && has_cols;

  (void)*tp_orig;
  (void)split_rd;

  av1_zero(pc_tree->pc_tree_stats);
  pc_tree->pc_tree_stats.valid = 1;

  // Override partition costs at the edges of the frame in the same
  // way as in read_partition (see decodeframe.c)
  if (!(has_rows && has_cols)) {
    assert(bsize_at_least_8x8 && pl >= 0);
    const aom_cdf_prob *partition_cdf = cm->fc->partition_cdf[pl];
    for (int i = 0; i < PARTITION_TYPES; ++i) tmp_partition_cost[i] = INT_MAX;
    if (has_cols) {
      // At the bottom, the two possibilities are HORZ and SPLIT
      aom_cdf_prob bot_cdf[2];
      partition_gather_vert_alike(bot_cdf, partition_cdf, bsize);
      static const int bot_inv_map[2] = { PARTITION_HORZ, PARTITION_SPLIT };
      av1_cost_tokens_from_cdf(tmp_partition_cost, bot_cdf, bot_inv_map);
    } else if (has_rows) {
      // At the right, the two possibilities are VERT and SPLIT
      aom_cdf_prob rhs_cdf[2];
      partition_gather_horz_alike(rhs_cdf, partition_cdf, bsize);
      static const int rhs_inv_map[2] = { PARTITION_VERT, PARTITION_SPLIT };
      av1_cost_tokens_from_cdf(tmp_partition_cost, rhs_cdf, rhs_inv_map);
    } else {
      // At the bottom right, we always split
      tmp_partition_cost[PARTITION_SPLIT] = 0;
    }

    partition_cost = tmp_partition_cost;
  }

#ifndef NDEBUG
  // Nothing should rely on the default value of this array (which is just
  // leftover from encoding the previous block. Setting it to magic number
  // when debugging.
  memset(x->blk_skip, 234, sizeof(x->blk_skip));
#endif  // NDEBUG

  assert(mi_size_wide[bsize] == mi_size_high[bsize]);

  av1_init_rd_stats(&this_rdc);
  av1_init_rd_stats(&sum_rdc);
  av1_invalid_rd_stats(&best_rdc);
  best_rdc.rdcost = best_rd;

  set_offsets(cpi, tile_info, x, mi_row, mi_col, bsize);

  if (bsize == BLOCK_16X16 && cpi->vaq_refresh)
    x->mb_energy = av1_block_energy(cpi, x, bsize);

  xd->above_txfm_context = cm->above_txfm_context[tile_info->tile_row] + mi_col;
  xd->left_txfm_context =
      xd->left_txfm_context_buffer + (mi_row & MAX_MIB_MASK);
  save_context(x, &x_ctx, mi_row, mi_col, bsize, num_planes);

  // PARTITION_NONE
  if (partition_none_allowed) {
    if (bsize_at_least_8x8) pc_tree->partitioning = PARTITION_NONE;

    rd_pick_sb_modes(cpi, tile_data, x, mi_row, mi_col, &this_rdc,
                     PARTITION_NONE, bsize, ctx_none, best_rdc.rdcost);

    pc_tree->pc_tree_stats.rdcost = ctx_none->rdcost;
    pc_tree->pc_tree_stats.skip = ctx_none->skip;

    if (none_rd) *none_rd = this_rdc.rdcost;
    if (this_rdc.rate != INT_MAX) {
      if (bsize_at_least_8x8) {
        const int pt_cost = partition_cost[PARTITION_NONE] < INT_MAX
                                ? partition_cost[PARTITION_NONE]
                                : 0;
        this_rdc.rate += pt_cost;
        this_rdc.rdcost = RDCOST(x->rdmult, this_rdc.rate, this_rdc.dist);
      }

      if (this_rdc.rdcost < best_rdc.rdcost) {
        // Adjust dist breakout threshold according to the partition size.
        const int64_t dist_breakout_thr =
            cpi->sf.partition_search_breakout_dist_thr >>
            ((2 * (MAX_SB_SIZE_LOG2 - 2)) -
             (mi_size_wide_log2[bsize] + mi_size_high_log2[bsize]));
        const int rate_breakout_thr =
            cpi->sf.partition_search_breakout_rate_thr *
            num_pels_log2_lookup[bsize];

        best_rdc = this_rdc;
        if (bsize_at_least_8x8) pc_tree->partitioning = PARTITION_NONE;

        pc_tree->cb_search_range = SEARCH_FULL_PLANE;

        // If all y, u, v transform blocks in this partition are skippable, and
        // the dist & rate are within the thresholds, the partition search is
        // terminated for current branch of the partition search tree.
        // The dist & rate thresholds are set to 0 at speed 0 to disable the
        // early termination at that speed.
        if (!x->e_mbd.lossless[xd->mi[0]->segment_id] &&
            (ctx_none->skippable && best_rdc.dist < dist_breakout_thr &&
             best_rdc.rate < rate_breakout_thr)) {
          do_square_split = 0;
        }
      }
    }

    restore_context(x, &x_ctx, mi_row, mi_col, bsize, num_planes);
  }

  // store estimated motion vector
  if (cpi->sf.adaptive_motion_search) store_pred_mv(x, ctx_none);

  int64_t temp_best_rdcost = best_rdc.rdcost;
  pn_rdc = best_rdc;

#if CONFIG_DIST_8X8
  uint8_t *src_plane_8x8[MAX_MB_PLANE], *dst_plane_8x8[MAX_MB_PLANE];

  if (x->using_dist_8x8 && bsize == BLOCK_8X8) {
    for (int i = 0; i < MAX_MB_PLANE; i++) {
      src_plane_8x8[i] = x->plane[i].src.buf;
      dst_plane_8x8[i] = xd->plane[i].dst.buf;
    }
  }
#endif  // CONFIG_DIST_8X8

  // PARTITION_SPLIT
  if (do_square_split) {
    int reached_last_index = 0;
    subsize = get_partition_subsize(bsize, PARTITION_SPLIT);
    int idx;

    for (idx = 0; idx < 4 && sum_rdc.rdcost < temp_best_rdcost; ++idx) {
      const int x_idx = (idx & 1) * mi_step;
      const int y_idx = (idx >> 1) * mi_step;

      if (mi_row + y_idx >= cm->mi_rows || mi_col + x_idx >= cm->mi_cols)
        continue;

      if (cpi->sf.adaptive_motion_search) load_pred_mv(x, ctx_none);

      pc_tree->split[idx]->index = idx;
      int64_t *p_split_rd = &split_rd[idx];
      rd_pick_sqr_partition(cpi, td, tile_data, tp, mi_row + y_idx,
                            mi_col + x_idx, subsize, &this_rdc,
                            temp_best_rdcost - sum_rdc.rdcost,
                            pc_tree->split[idx], p_split_rd);

      pc_tree->pc_tree_stats.sub_block_rdcost[idx] = this_rdc.rdcost;
      pc_tree->pc_tree_stats.sub_block_skip[idx] =
          pc_tree->split[idx]->none.skip;

      if (this_rdc.rate == INT_MAX) {
        sum_rdc.rdcost = INT64_MAX;
        break;
      } else {
        sum_rdc.rate += this_rdc.rate;
        sum_rdc.dist += this_rdc.dist;
        sum_rdc.rdcost += this_rdc.rdcost;
      }
    }
    reached_last_index = (idx == 4);

#if CONFIG_DIST_8X8
    if (x->using_dist_8x8 && reached_last_index &&
        sum_rdc.rdcost != INT64_MAX && bsize == BLOCK_8X8) {
      sum_rdc.dist = dist_8x8_yuv(cpi, x, src_plane_8x8, dst_plane_8x8);
      sum_rdc.rdcost = RDCOST(x->rdmult, sum_rdc.rate, sum_rdc.dist);
    }
#endif  // CONFIG_DIST_8X8

    if (reached_last_index && sum_rdc.rdcost < best_rdc.rdcost) {
      sum_rdc.rate += partition_cost[PARTITION_SPLIT];
      sum_rdc.rdcost = RDCOST(x->rdmult, sum_rdc.rate, sum_rdc.dist);

      if (sum_rdc.rdcost < best_rdc.rdcost) {
        best_rdc = sum_rdc;
        pc_tree->partitioning = PARTITION_SPLIT;
      }
    }

    int has_split = 0;
    if (pc_tree->partitioning == PARTITION_SPLIT) {
      for (int cb_idx = 0; cb_idx <= AOMMIN(idx, 3); ++cb_idx) {
        if (pc_tree->split[cb_idx]->partitioning == PARTITION_SPLIT)
          ++has_split;
      }

      if (has_split >= 3 || sum_rdc.rdcost < (pn_rdc.rdcost >> 1)) {
        pc_tree->cb_search_range = SPLIT_PLANE;
      }
    }

    if (pc_tree->partitioning == PARTITION_NONE) {
      pc_tree->cb_search_range = SEARCH_SAME_PLANE;
      if (pn_rdc.dist <= sum_rdc.dist)
        pc_tree->cb_search_range = NONE_PARTITION_PLANE;
    }

    if (pn_rdc.rate == INT_MAX) pc_tree->cb_search_range = NONE_PARTITION_PLANE;

    restore_context(x, &x_ctx, mi_row, mi_col, bsize, num_planes);
  }  // if (do_split)

  pc_tree->pc_tree_stats.split = pc_tree->partitioning == PARTITION_SPLIT;
  if (do_square_split) {
    for (int i = 0; i < 4; ++i) {
      pc_tree->pc_tree_stats.sub_block_split[i] =
          pc_tree->split[i]->partitioning == PARTITION_SPLIT;
    }
  }

  // TODO(jbb): This code added so that we avoid static analysis
  // warning related to the fact that best_rd isn't used after this
  // point.  This code should be refactored so that the duplicate
  // checks occur in some sub function and thus are used...
  (void)best_rd;
  *rd_cost = best_rdc;

  if (best_rdc.rate < INT_MAX && best_rdc.dist < INT64_MAX &&
      pc_tree->index != 3) {
    if (bsize == cm->seq_params.sb_size) {
      restore_context(x, &x_ctx, mi_row, mi_col, bsize, num_planes);
    } else {
      encode_sb(cpi, td, tile_data, tp, mi_row, mi_col, DRY_RUN_NORMAL, bsize,
                pc_tree, NULL);
    }
  }

#if CONFIG_DIST_8X8
  if (x->using_dist_8x8 && best_rdc.rate < INT_MAX &&
      best_rdc.dist < INT64_MAX && bsize == BLOCK_4X4 && pc_tree->index == 3) {
    encode_sb(cpi, td, tile_data, tp, mi_row, mi_col, DRY_RUN_NORMAL, bsize,
              pc_tree, NULL);
  }
#endif  // CONFIG_DIST_8X8

  if (bsize == cm->seq_params.sb_size) {
    assert(best_rdc.rate < INT_MAX);
    assert(best_rdc.dist < INT64_MAX);
  } else {
    assert(tp_orig == *tp);
  }
}

#define FEATURE_SIZE 19
static const float two_pass_split_partition_weights_128[FEATURE_SIZE + 1] = {
  2.683936f, -0.193620f, -4.106470f, -0.141320f, -0.282289f,
  0.125296f, -1.134961f, 0.862757f,  -0.418799f, -0.637666f,
  0.016232f, 0.345013f,  0.018823f,  -0.393394f, -1.130700f,
  0.695357f, 0.112569f,  -0.341975f, -0.513882f, 5.7488966f,
};

static const float two_pass_split_partition_weights_64[FEATURE_SIZE + 1] = {
  2.990993f,  0.423273f,  -0.926544f, 0.454646f,  -0.292698f,
  -1.311632f, -0.284432f, 0.717141f,  -0.419257f, -0.574760f,
  -0.674444f, 0.669047f,  -0.374255f, 0.380624f,  -0.804036f,
  0.264021f,  0.004163f,  1.896802f,  0.924287f,  0.13490619f,
};

static const float two_pass_split_partition_weights_32[FEATURE_SIZE + 1] = {
  2.795181f,  -0.136943f, -0.924842f, 0.405330f,  -0.463505f,
  -0.584076f, -0.831472f, 0.382985f,  -0.597544f, -0.138915f,
  -1.354350f, 0.466035f,  -0.553961f, 0.213202f,  -1.166429f,
  0.010776f,  -0.096236f, 2.335084f,  1.699857f,  -0.58178353f,
};

static const float two_pass_split_partition_weights_16[FEATURE_SIZE + 1] = {
  1.987888f,  -0.431100f, -1.687703f, 0.262602f,  -0.425298f,
  -0.463870f, -1.493457f, 0.470917f,  -0.528457f, -0.087700f,
  -1.815092f, 0.152883f,  -0.337908f, 0.093679f,  -1.548267f,
  -0.042387f, -0.000861f, 2.556746f,  1.619192f,  0.03643292f,
};

static const float two_pass_split_partition_weights_8[FEATURE_SIZE + 1] = {
  2.188344f,  -0.817528f, -2.119219f, 0.000000f,  -0.348167f,
  -0.658074f, -1.960362f, 0.000000f,  -0.403080f, 0.282699f,
  -2.061088f, 0.000000f,  -0.431919f, -0.127960f, -1.099550f,
  0.000000f,  0.121622f,  2.017455f,  2.058228f,  -0.15475988f,
};

static const float two_pass_none_partition_weights_128[FEATURE_SIZE + 1] = {
  -1.006689f, 0.777908f,  4.461072f,  -0.395782f, -0.014610f,
  -0.853863f, 0.729997f,  -0.420477f, 0.282429f,  -1.194595f,
  3.181220f,  -0.511416f, 0.117084f,  -1.149348f, 1.507990f,
  -0.477212f, 0.202963f,  -1.469581f, 0.624461f,  -0.89081228f,
};

static const float two_pass_none_partition_weights_64[FEATURE_SIZE + 1] = {
  -1.241117f, 0.844878f,  5.638803f,  -0.489780f, -0.108796f,
  -4.576821f, 1.540624f,  -0.477519f, 0.227791f,  -1.443968f,
  1.586911f,  -0.505125f, 0.140764f,  -0.464194f, 1.466658f,
  -0.641166f, 0.195412f,  1.427905f,  2.080007f,  -1.98272777f,
};

static const float two_pass_none_partition_weights_32[FEATURE_SIZE + 1] = {
  -2.130825f, 0.476023f,  5.907343f,  -0.516002f, -0.097471f,
  -2.662754f, 0.614858f,  -0.576728f, 0.085261f,  -0.031901f,
  0.727842f,  -0.600034f, 0.079326f,  0.324328f,  0.504502f,
  -0.547105f, -0.037670f, 0.304995f,  0.369018f,  -2.66299987f,
};

static const float two_pass_none_partition_weights_16[FEATURE_SIZE + 1] = {
  -1.626410f, 0.872047f,  5.414965f,  -0.554781f, -0.084514f,
  -3.020550f, 0.467632f,  -0.382280f, 0.199568f,  0.426220f,
  0.829426f,  -0.467100f, 0.153098f,  0.662994f,  0.327545f,
  -0.560106f, -0.141610f, 0.403372f,  0.523991f,  -3.02891231f,
};

static const float two_pass_none_partition_weights_8[FEATURE_SIZE + 1] = {
  -1.463349f, 0.375376f,  4.751430f, 0.000000f, -0.184451f,
  -1.655447f, 0.443214f,  0.000000f, 0.127961f, 0.152435f,
  0.083288f,  0.000000f,  0.143105f, 0.438012f, 0.073238f,
  0.000000f,  -0.278137f, 0.186134f, 0.073737f, -1.6494962f,
};

// split_score indicates confidence of picking split partition;
// none_score indicates confidence of picking none partition;
static int ml_prune_2pass_split_partition(const PC_TREE_STATS *pc_tree_stats,
                                          BLOCK_SIZE bsize, int *split_score,
                                          int *none_score) {
  if (!pc_tree_stats->valid) return 0;
  const float *split_weights = NULL;
  const float *none_weights = NULL;
  switch (bsize) {
    case BLOCK_4X4: break;
    case BLOCK_8X8:
      split_weights = two_pass_split_partition_weights_8;
      none_weights = two_pass_none_partition_weights_8;
      break;
    case BLOCK_16X16:
      split_weights = two_pass_split_partition_weights_16;
      none_weights = two_pass_none_partition_weights_16;
      break;
    case BLOCK_32X32:
      split_weights = two_pass_split_partition_weights_32;
      none_weights = two_pass_none_partition_weights_32;
      break;
    case BLOCK_64X64:
      split_weights = two_pass_split_partition_weights_64;
      none_weights = two_pass_none_partition_weights_64;
      break;
    case BLOCK_128X128:
      split_weights = two_pass_split_partition_weights_128;
      none_weights = two_pass_none_partition_weights_128;
      break;
    default: assert(0 && "Unexpected bsize.");
  }
  if (!split_weights || !none_weights) return 0;

  aom_clear_system_state();

  float features[FEATURE_SIZE];
  int feature_index = 0;
  features[feature_index++] = (float)pc_tree_stats->split;
  features[feature_index++] = (float)pc_tree_stats->skip;
  const int rdcost = (int)AOMMIN(INT_MAX, pc_tree_stats->rdcost);
  const int rd_valid = rdcost > 0 && rdcost < 1000000000;
  features[feature_index++] = (float)rd_valid;
  for (int i = 0; i < 4; ++i) {
    features[feature_index++] = (float)pc_tree_stats->sub_block_split[i];
    features[feature_index++] = (float)pc_tree_stats->sub_block_skip[i];
    const int sub_rdcost =
        (int)AOMMIN(INT_MAX, pc_tree_stats->sub_block_rdcost[i]);
    const int sub_rd_valid = sub_rdcost > 0 && sub_rdcost < 1000000000;
    features[feature_index++] = (float)sub_rd_valid;
    // Ratio between the sub-block RD and the whole-block RD.
    float rd_ratio = 1.0f;
    if (rd_valid && sub_rd_valid && sub_rdcost < rdcost)
      rd_ratio = (float)sub_rdcost / (float)rdcost;
    features[feature_index++] = rd_ratio;
  }
  assert(feature_index == FEATURE_SIZE);

  float score_1 = split_weights[FEATURE_SIZE];
  float score_2 = none_weights[FEATURE_SIZE];
  for (int i = 0; i < FEATURE_SIZE; ++i) {
    score_1 += features[i] * split_weights[i];
    score_2 += features[i] * none_weights[i];
  }
  *split_score = (int)(score_1 * 100);
  *none_score = (int)(score_2 * 100);
  return 1;
}
#undef FEATURE_SIZE

// Use a ML model to predict if horz_a, horz_b, vert_a, and vert_b should be
// considered.
static void ml_prune_ab_partition(BLOCK_SIZE bsize, int part_ctx, int var_ctx,
                                  int64_t best_rd, int64_t horz_rd[2],
                                  int64_t vert_rd[2], int64_t split_rd[4],
                                  int *const horza_partition_allowed,
                                  int *const horzb_partition_allowed,
                                  int *const verta_partition_allowed,
                                  int *const vertb_partition_allowed) {
  if (bsize < BLOCK_8X8 || best_rd >= 1000000000) return;
  const NN_CONFIG *nn_config = NULL;
  switch (bsize) {
    case BLOCK_8X8: nn_config = NULL; break;
    case BLOCK_16X16: nn_config = &av1_ab_partition_nnconfig_16; break;
    case BLOCK_32X32: nn_config = &av1_ab_partition_nnconfig_32; break;
    case BLOCK_64X64: nn_config = &av1_ab_partition_nnconfig_64; break;
    case BLOCK_128X128: nn_config = &av1_ab_partition_nnconfig_128; break;
    default: assert(0 && "Unexpected bsize.");
  }
  if (!nn_config) return;

  aom_clear_system_state();

  // Generate features.
  float features[10];
  int feature_index = 0;
  features[feature_index++] = (float)part_ctx;
  features[feature_index++] = (float)var_ctx;
  const int rdcost = (int)AOMMIN(INT_MAX, best_rd);
  int sub_block_rdcost[8] = { 0 };
  int rd_index = 0;
  for (int i = 0; i < 2; ++i) {
    if (horz_rd[i] > 0 && horz_rd[i] < 1000000000)
      sub_block_rdcost[rd_index] = (int)horz_rd[i];
    ++rd_index;
  }
  for (int i = 0; i < 2; ++i) {
    if (vert_rd[i] > 0 && vert_rd[i] < 1000000000)
      sub_block_rdcost[rd_index] = (int)vert_rd[i];
    ++rd_index;
  }
  for (int i = 0; i < 4; ++i) {
    if (split_rd[i] > 0 && split_rd[i] < 1000000000)
      sub_block_rdcost[rd_index] = (int)split_rd[i];
    ++rd_index;
  }
  for (int i = 0; i < 8; ++i) {
    // Ratio between the sub-block RD and the whole-block RD.
    float rd_ratio = 1.0f;
    if (sub_block_rdcost[i] > 0 && sub_block_rdcost[i] < rdcost)
      rd_ratio = (float)sub_block_rdcost[i] / (float)rdcost;
    features[feature_index++] = rd_ratio;
  }
  assert(feature_index == 10);

  // Calculate scores using the NN model.
  float score[16] = { 0.0f };
  av1_nn_predict(features, nn_config, score);
  int int_score[16];
  int max_score = -1000;
  for (int i = 0; i < 16; ++i) {
    int_score[i] = (int)(100 * score[i]);
    max_score = AOMMAX(int_score[i], max_score);
  }

  // Make decisions based on the model scores.
  int thresh = max_score;
  switch (bsize) {
    case BLOCK_16X16: thresh -= 150; break;
    case BLOCK_32X32: thresh -= 100; break;
    default: break;
  }
  *horza_partition_allowed = 0;
  *horzb_partition_allowed = 0;
  *verta_partition_allowed = 0;
  *vertb_partition_allowed = 0;
  for (int i = 0; i < 16; ++i) {
    if (int_score[i] >= thresh) {
      if ((i >> 0) & 1) *horza_partition_allowed = 1;
      if ((i >> 1) & 1) *horzb_partition_allowed = 1;
      if ((i >> 2) & 1) *verta_partition_allowed = 1;
      if ((i >> 3) & 1) *vertb_partition_allowed = 1;
    }
  }
}

// TODO(jingning,jimbankoski,rbultje): properly skip partition types that are
// unlikely to be selected depending on previous rate-distortion optimization
// results, for encoding speed-up.
static void rd_pick_partition(const AV1_COMP *const cpi, ThreadData *td,
                              TileDataEnc *tile_data, TOKENEXTRA **tp,
                              int mi_row, int mi_col, BLOCK_SIZE bsize,
                              RD_STATS *rd_cost, int64_t best_rd,
                              PC_TREE *pc_tree, int64_t *none_rd) {
  const AV1_COMMON *const cm = &cpi->common;
  const int num_planes = av1_num_planes(cm);
  TileInfo *const tile_info = &tile_data->tile_info;
  MACROBLOCK *const x = &td->mb;
  MACROBLOCKD *const xd = &x->e_mbd;
  const int mi_step = mi_size_wide[bsize] / 2;
  RD_SEARCH_MACROBLOCK_CONTEXT x_ctx;
  const TOKENEXTRA *const tp_orig = *tp;
  PICK_MODE_CONTEXT *ctx_none = &pc_tree->none;
  int tmp_partition_cost[PARTITION_TYPES];
  BLOCK_SIZE subsize;
  RD_STATS this_rdc, sum_rdc, best_rdc;
  const int bsize_at_least_8x8 = (bsize >= BLOCK_8X8);
  int do_square_split = bsize_at_least_8x8;
  const int pl = bsize_at_least_8x8
                     ? partition_plane_context(xd, mi_row, mi_col, bsize)
                     : 0;
  const int *partition_cost =
      pl >= 0 ? x->partition_cost[pl] : x->partition_cost[0];

  int do_rectangular_split = 1;
  int64_t split_rd[4] = { 0, 0, 0, 0 };
  int64_t horz_rd[2] = { 0, 0 };
  int64_t vert_rd[2] = { 0, 0 };

  int split_ctx_is_ready[2] = { 0, 0 };
  int horz_ctx_is_ready = 0;
  int vert_ctx_is_ready = 0;
  BLOCK_SIZE bsize2 = get_partition_subsize(bsize, PARTITION_SPLIT);

  if (bsize == cm->seq_params.sb_size) x->must_find_valid_partition = 0;

  // Override skipping rectangular partition operations for edge blocks
  const int has_rows = (mi_row + mi_step < cm->mi_rows);
  const int has_cols = (mi_col + mi_step < cm->mi_cols);
  const int xss = x->e_mbd.plane[1].subsampling_x;
  const int yss = x->e_mbd.plane[1].subsampling_y;

  BLOCK_SIZE min_size = x->min_partition_size;
  BLOCK_SIZE max_size = x->max_partition_size;

  if (none_rd) *none_rd = 0;

#if CONFIG_FP_MB_STATS
  unsigned int src_diff_var = UINT_MAX;
  int none_complexity = 0;
#endif

  int partition_none_allowed = has_rows && has_cols;
  int partition_horz_allowed = has_cols && yss <= xss && bsize_at_least_8x8;
  int partition_vert_allowed = has_rows && xss <= yss && bsize_at_least_8x8;

  (void)*tp_orig;

  // Override partition costs at the edges of the frame in the same
  // way as in read_partition (see decodeframe.c)
  if (!(has_rows && has_cols)) {
    assert(bsize_at_least_8x8 && pl >= 0);
    const aom_cdf_prob *partition_cdf = cm->fc->partition_cdf[pl];
    for (int i = 0; i < PARTITION_TYPES; ++i) tmp_partition_cost[i] = INT_MAX;
    if (has_cols) {
      // At the bottom, the two possibilities are HORZ and SPLIT
      aom_cdf_prob bot_cdf[2];
      partition_gather_vert_alike(bot_cdf, partition_cdf, bsize);
      static const int bot_inv_map[2] = { PARTITION_HORZ, PARTITION_SPLIT };
      av1_cost_tokens_from_cdf(tmp_partition_cost, bot_cdf, bot_inv_map);
    } else if (has_rows) {
      // At the right, the two possibilities are VERT and SPLIT
      aom_cdf_prob rhs_cdf[2];
      partition_gather_horz_alike(rhs_cdf, partition_cdf, bsize);
      static const int rhs_inv_map[2] = { PARTITION_VERT, PARTITION_SPLIT };
      av1_cost_tokens_from_cdf(tmp_partition_cost, rhs_cdf, rhs_inv_map);
    } else {
      // At the bottom right, we always split
      tmp_partition_cost[PARTITION_SPLIT] = 0;
    }

    partition_cost = tmp_partition_cost;
  }

#ifndef NDEBUG
  // Nothing should rely on the default value of this array (which is just
  // leftover from encoding the previous block. Setting it to magic number
  // when debugging.
  memset(x->blk_skip, 234, sizeof(x->blk_skip));
#endif  // NDEBUG

  assert(mi_size_wide[bsize] == mi_size_high[bsize]);

  av1_init_rd_stats(&this_rdc);
  av1_invalid_rd_stats(&best_rdc);
  best_rdc.rdcost = best_rd;

  set_offsets(cpi, tile_info, x, mi_row, mi_col, bsize);

  if (bsize == BLOCK_16X16 && cpi->vaq_refresh)
    x->mb_energy = av1_block_energy(cpi, x, bsize);

  if (cpi->sf.cb_partition_search && bsize == BLOCK_16X16) {
    const int cb_partition_search_ctrl =
        ((pc_tree->index == 0 || pc_tree->index == 3) +
         get_chessboard_index(cm->current_video_frame)) &
        0x1;

    if (cb_partition_search_ctrl && bsize > min_size && bsize < max_size)
      set_partition_range(cm, xd, mi_row, mi_col, bsize, &min_size, &max_size);
  }

  // Determine partition types in search according to the speed features.
  // The threshold set here has to be of square block size.
  if (cpi->sf.auto_min_max_partition_size) {
    const int no_partition_allowed = (bsize <= max_size && bsize >= min_size);
    // Note: Further partitioning is NOT allowed when bsize == min_size already.
    const int partition_allowed = (bsize <= max_size && bsize > min_size);
    partition_none_allowed &= no_partition_allowed;
    partition_horz_allowed &= partition_allowed || !has_rows;
    partition_vert_allowed &= partition_allowed || !has_cols;
    do_square_split &= bsize > min_size;
  }
  if (cpi->sf.use_square_partition_only) {
    partition_horz_allowed &= !has_rows;
    partition_vert_allowed &= !has_cols;
  }

  if (bsize > BLOCK_4X4 && x->use_cb_search_range &&
      cpi->sf.auto_min_max_partition_size == 0) {
    int split_score = 0;
    int none_score = 0;
    const int score_valid = ml_prune_2pass_split_partition(
        &pc_tree->pc_tree_stats, bsize, &split_score, &none_score);
    if (score_valid) {
      {
        const int only_split_thresh = 300;
        const int no_none_thresh = 250;
        const int no_split_thresh = 0;
        if (split_score > only_split_thresh) {
          partition_none_allowed = 0;
          partition_horz_allowed = 0;
          partition_vert_allowed = 0;
        } else if (split_score > no_none_thresh) {
          partition_none_allowed = 0;
        }
        if (split_score < no_split_thresh) do_square_split = 0;
      }
      {
        const int no_split_thresh = 120;
        const int no_none_thresh = -120;
        if (none_score > no_split_thresh && partition_none_allowed)
          do_square_split = 0;
        if (none_score < no_none_thresh) partition_none_allowed = 0;
      }
    } else {
      if (pc_tree->cb_search_range == SPLIT_PLANE) {
        partition_none_allowed = 0;
        partition_horz_allowed = 0;
        partition_vert_allowed = 0;
      }
      if (pc_tree->cb_search_range == SEARCH_SAME_PLANE) do_square_split = 0;
      if (pc_tree->cb_search_range == NONE_PARTITION_PLANE) {
        do_square_split = 0;
        partition_horz_allowed = 0;
        partition_vert_allowed = 0;
      }
    }

    // Fall back to default values in case all partition modes are rejected.
    if (partition_none_allowed == 0 && do_square_split == 0 &&
        partition_horz_allowed == 0 && partition_vert_allowed == 0) {
      do_square_split = bsize_at_least_8x8;
      partition_none_allowed = has_rows && has_cols;
      partition_horz_allowed = has_cols && yss <= xss && bsize_at_least_8x8;
      partition_vert_allowed = has_rows && xss <= yss && bsize_at_least_8x8;
    }
  }

  xd->above_txfm_context = cm->above_txfm_context[tile_info->tile_row] + mi_col;
  xd->left_txfm_context =
      xd->left_txfm_context_buffer + (mi_row & MAX_MIB_MASK);
  save_context(x, &x_ctx, mi_row, mi_col, bsize, num_planes);

#if CONFIG_FP_MB_STATS
  if (cpi->use_fp_mb_stats) {
    set_offsets(cpi, tile_info, x, mi_row, mi_col, bsize);
    src_diff_var = get_sby_perpixel_diff_variance(cpi, &x->plane[0].src, mi_row,
                                                  mi_col, bsize);
  }

  // Decide whether we shall split directly and skip searching NONE by using
  // the first pass block statistics
  if (cpi->use_fp_mb_stats && bsize >= BLOCK_32X32 && do_square_split &&
      partition_none_allowed && src_diff_var > 4 &&
      cm->base_qindex < qindex_split_threshold_lookup[bsize]) {
    int mb_row = mi_row >> 1;
    int mb_col = mi_col >> 1;
    int mb_row_end =
        AOMMIN(mb_row + num_16x16_blocks_high_lookup[bsize], cm->mb_rows);
    int mb_col_end =
        AOMMIN(mb_col + num_16x16_blocks_wide_lookup[bsize], cm->mb_cols);
    int r, c;

    // compute a complexity measure, basically measure inconsistency of motion
    // vectors obtained from the first pass in the current block
    for (r = mb_row; r < mb_row_end; r++) {
      for (c = mb_col; c < mb_col_end; c++) {
        const int mb_index = r * cm->mb_cols + c;

        MOTION_DIRECTION this_mv;
        MOTION_DIRECTION right_mv;
        MOTION_DIRECTION bottom_mv;

        this_mv =
            get_motion_direction_fp(cpi->twopass.this_frame_mb_stats[mb_index]);

        // to its right
        if (c != mb_col_end - 1) {
          right_mv = get_motion_direction_fp(
              cpi->twopass.this_frame_mb_stats[mb_index + 1]);
          none_complexity += get_motion_inconsistency(this_mv, right_mv);
        }

        // to its bottom
        if (r != mb_row_end - 1) {
          bottom_mv = get_motion_direction_fp(
              cpi->twopass.this_frame_mb_stats[mb_index + cm->mb_cols]);
          none_complexity += get_motion_inconsistency(this_mv, bottom_mv);
        }

        // do not count its left and top neighbors to avoid double counting
      }
    }

    if (none_complexity > complexity_16x16_blocks_threshold[bsize]) {
      partition_none_allowed = 0;
    }
  }
#endif

BEGIN_PARTITION_SEARCH:
  if (x->must_find_valid_partition) {
    partition_none_allowed = has_rows && has_cols;
    partition_horz_allowed = has_cols && yss <= xss && bsize_at_least_8x8;
    partition_vert_allowed = has_rows && xss <= yss && bsize_at_least_8x8;
  }
  // PARTITION_NONE
  if (partition_none_allowed) {
    rd_pick_sb_modes(cpi, tile_data, x, mi_row, mi_col, &this_rdc,
                     PARTITION_NONE, bsize, ctx_none, best_rdc.rdcost);
    if (none_rd) *none_rd = this_rdc.rdcost;
    if (this_rdc.rate != INT_MAX) {
      if (bsize_at_least_8x8) {
        const int pt_cost = partition_cost[PARTITION_NONE] < INT_MAX
                                ? partition_cost[PARTITION_NONE]
                                : 0;
        this_rdc.rate += pt_cost;
        this_rdc.rdcost = RDCOST(x->rdmult, this_rdc.rate, this_rdc.dist);
      }

      if (this_rdc.rdcost < best_rdc.rdcost) {
        // Adjust dist breakout threshold according to the partition size.
        const int64_t dist_breakout_thr =
            cpi->sf.partition_search_breakout_dist_thr >>
            ((2 * (MAX_SB_SIZE_LOG2 - 2)) -
             (mi_size_wide_log2[bsize] + mi_size_high_log2[bsize]));
        const int rate_breakout_thr =
            cpi->sf.partition_search_breakout_rate_thr *
            num_pels_log2_lookup[bsize];

        best_rdc = this_rdc;
        if (bsize_at_least_8x8) pc_tree->partitioning = PARTITION_NONE;

        // If all y, u, v transform blocks in this partition are skippable, and
        // the dist & rate are within the thresholds, the partition search is
        // terminated for current branch of the partition search tree.
        // The dist & rate thresholds are set to 0 at speed 0 to disable the
        // early termination at that speed.
        if (!x->e_mbd.lossless[xd->mi[0]->segment_id] &&
            (ctx_none->skippable && best_rdc.dist < dist_breakout_thr &&
             best_rdc.rate < rate_breakout_thr)) {
          do_square_split = 0;
          do_rectangular_split = 0;
        }

#if CONFIG_FP_MB_STATS
        // Check if every 16x16 first pass block statistics has zero
        // motion and the corresponding first pass residue is small enough.
        // If that is the case, check the difference variance between the
        // current frame and the last frame. If the variance is small enough,
        // stop further splitting in RD optimization
        if (cpi->use_fp_mb_stats && do_square_split &&
            cm->base_qindex > qindex_skip_threshold_lookup[bsize]) {
          int mb_row = mi_row >> 1;
          int mb_col = mi_col >> 1;
          int mb_row_end =
              AOMMIN(mb_row + num_16x16_blocks_high_lookup[bsize], cm->mb_rows);
          int mb_col_end =
              AOMMIN(mb_col + num_16x16_blocks_wide_lookup[bsize], cm->mb_cols);
          int r, c;

          int skip = 1;
          for (r = mb_row; r < mb_row_end; r++) {
            for (c = mb_col; c < mb_col_end; c++) {
              const int mb_index = r * cm->mb_cols + c;
              if (!(cpi->twopass.this_frame_mb_stats[mb_index] &
                    FPMB_MOTION_ZERO_MASK) ||
                  !(cpi->twopass.this_frame_mb_stats[mb_index] &
                    FPMB_ERROR_SMALL_MASK)) {
                skip = 0;
                break;
              }
            }
            if (skip == 0) {
              break;
            }
          }
          if (skip) {
            if (src_diff_var == UINT_MAX) {
              set_offsets(cpi, tile_info, x, mi_row, mi_col, bsize);
              src_diff_var = get_sby_perpixel_diff_variance(
                  cpi, &x->plane[0].src, mi_row, mi_col, bsize);
            }
            if (src_diff_var < 8) {
              do_square_split = 0;
              do_rectangular_split = 0;
            }
          }
        }
#endif
      }
    }

    restore_context(x, &x_ctx, mi_row, mi_col, bsize, num_planes);
  }

  // store estimated motion vector
  if (cpi->sf.adaptive_motion_search) store_pred_mv(x, ctx_none);

#if CONFIG_DIST_8X8
  uint8_t *src_plane_8x8[MAX_MB_PLANE], *dst_plane_8x8[MAX_MB_PLANE];

  if (x->using_dist_8x8 && bsize == BLOCK_8X8) {
    for (int i = 0; i < num_planes; i++) {
      src_plane_8x8[i] = x->plane[i].src.buf;
      dst_plane_8x8[i] = xd->plane[i].dst.buf;
    }
  }
#endif  // CONFIG_DIST_8X8

  // PARTITION_SPLIT
  if (do_square_split) {
    av1_init_rd_stats(&sum_rdc);
    int reached_last_index = 0;
    subsize = get_partition_subsize(bsize, PARTITION_SPLIT);
    int idx;

    for (idx = 0; idx < 4 && sum_rdc.rdcost < best_rdc.rdcost; ++idx) {
      const int x_idx = (idx & 1) * mi_step;
      const int y_idx = (idx >> 1) * mi_step;

      if (mi_row + y_idx >= cm->mi_rows || mi_col + x_idx >= cm->mi_cols)
        continue;

      if (cpi->sf.adaptive_motion_search) load_pred_mv(x, ctx_none);

      pc_tree->split[idx]->index = idx;
      int64_t *p_split_rd = &split_rd[idx];
      rd_pick_partition(cpi, td, tile_data, tp, mi_row + y_idx, mi_col + x_idx,
                        subsize, &this_rdc, best_rdc.rdcost - sum_rdc.rdcost,
                        pc_tree->split[idx], p_split_rd);

      if (this_rdc.rate == INT_MAX) {
        sum_rdc.rdcost = INT64_MAX;
        break;
      } else {
        sum_rdc.rate += this_rdc.rate;
        sum_rdc.dist += this_rdc.dist;
        sum_rdc.rdcost += this_rdc.rdcost;

        if (idx <= 1 && (bsize <= BLOCK_8X8 ||
                         pc_tree->split[idx]->partitioning == PARTITION_NONE)) {
          MB_MODE_INFO *const mbmi = &(pc_tree->split[idx]->none.mic);
          PALETTE_MODE_INFO *const pmi = &mbmi->palette_mode_info;
          // Neither palette mode nor cfl predicted
          if (pmi->palette_size[0] == 0 && pmi->palette_size[1] == 0) {
            if (mbmi->uv_mode != UV_CFL_PRED) split_ctx_is_ready[idx] = 1;
          }
        }
      }
    }
    reached_last_index = (idx == 4);

#if CONFIG_DIST_8X8
    if (x->using_dist_8x8 && reached_last_index &&
        sum_rdc.rdcost != INT64_MAX && bsize == BLOCK_8X8) {
      int64_t dist_8x8;
      dist_8x8 = dist_8x8_yuv(cpi, x, src_plane_8x8, dst_plane_8x8);
#ifdef DEBUG_DIST_8X8
      // TODO(anyone): Fix dist-8x8 assert failure here when CFL is enabled
      if (x->tune_metric == AOM_TUNE_PSNR && xd->bd == 8 && 0 /*!CONFIG_CFL*/)
        assert(sum_rdc.dist == dist_8x8);
#endif  // DEBUG_DIST_8X8
      sum_rdc.dist = dist_8x8;
      sum_rdc.rdcost = RDCOST(x->rdmult, sum_rdc.rate, sum_rdc.dist);
    }
#endif  // CONFIG_DIST_8X8

    if (reached_last_index && sum_rdc.rdcost < best_rdc.rdcost) {
      sum_rdc.rate += partition_cost[PARTITION_SPLIT];
      sum_rdc.rdcost = RDCOST(x->rdmult, sum_rdc.rate, sum_rdc.dist);

      if (sum_rdc.rdcost < best_rdc.rdcost) {
        best_rdc = sum_rdc;
        pc_tree->partitioning = PARTITION_SPLIT;
      }
    } else if (cpi->sf.less_rectangular_check) {
      // skip rectangular partition test when larger block size
      // gives better rd cost
      do_rectangular_split &= !partition_none_allowed;
    }

    restore_context(x, &x_ctx, mi_row, mi_col, bsize, num_planes);
  }  // if (do_split)

  // PARTITION_HORZ
  if (partition_horz_allowed &&
      (do_rectangular_split || active_h_edge(cpi, mi_row, mi_step))) {
    av1_init_rd_stats(&sum_rdc);
    subsize = get_partition_subsize(bsize, PARTITION_HORZ);
    if (cpi->sf.adaptive_motion_search) load_pred_mv(x, ctx_none);
    if (cpi->sf.adaptive_pred_interp_filter && bsize == BLOCK_8X8 &&
        partition_none_allowed)
      pc_tree->horizontal[0].pred_interp_filter =
          av1_extract_interp_filter(ctx_none->mic.interp_filters, 0);

    rd_pick_sb_modes(cpi, tile_data, x, mi_row, mi_col, &sum_rdc,
                     PARTITION_HORZ, subsize, &pc_tree->horizontal[0],
                     best_rdc.rdcost);
    horz_rd[0] = sum_rdc.rdcost;

    if (sum_rdc.rdcost < best_rdc.rdcost && has_rows) {
      PICK_MODE_CONTEXT *ctx_h = &pc_tree->horizontal[0];
      MB_MODE_INFO *const mbmi = &(pc_tree->horizontal[0].mic);
      PALETTE_MODE_INFO *const pmi = &mbmi->palette_mode_info;
      // Neither palette mode nor cfl predicted
      if (pmi->palette_size[0] == 0 && pmi->palette_size[1] == 0) {
        if (mbmi->uv_mode != UV_CFL_PRED) horz_ctx_is_ready = 1;
      }
      update_state(cpi, tile_data, td, ctx_h, mi_row, mi_col, subsize, 1);
      encode_superblock(cpi, tile_data, td, tp, DRY_RUN_NORMAL, mi_row, mi_col,
                        subsize, NULL);

      if (cpi->sf.adaptive_motion_search) load_pred_mv(x, ctx_h);

      if (cpi->sf.adaptive_pred_interp_filter && bsize == BLOCK_8X8 &&
          partition_none_allowed)
        pc_tree->horizontal[1].pred_interp_filter =
            av1_extract_interp_filter(ctx_h->mic.interp_filters, 0);

      rd_pick_sb_modes(cpi, tile_data, x, mi_row + mi_step, mi_col, &this_rdc,
                       PARTITION_HORZ, subsize, &pc_tree->horizontal[1],
                       best_rdc.rdcost - sum_rdc.rdcost);
      horz_rd[1] = this_rdc.rdcost;

#if CONFIG_DIST_8X8
      if (x->using_dist_8x8 && this_rdc.rate != INT_MAX && bsize == BLOCK_8X8) {
        update_state(cpi, tile_data, td, &pc_tree->horizontal[1],
                     mi_row + mi_step, mi_col, subsize, DRY_RUN_NORMAL);
        encode_superblock(cpi, tile_data, td, tp, DRY_RUN_NORMAL,
                          mi_row + mi_step, mi_col, subsize, NULL);
      }
#endif  // CONFIG_DIST_8X8

      if (this_rdc.rate == INT_MAX) {
        sum_rdc.rdcost = INT64_MAX;
      } else {
        sum_rdc.rate += this_rdc.rate;
        sum_rdc.dist += this_rdc.dist;
        sum_rdc.rdcost += this_rdc.rdcost;
      }
#if CONFIG_DIST_8X8
      if (x->using_dist_8x8 && sum_rdc.rdcost != INT64_MAX &&
          bsize == BLOCK_8X8) {
        int64_t dist_8x8;
        dist_8x8 = dist_8x8_yuv(cpi, x, src_plane_8x8, dst_plane_8x8);
#ifdef DEBUG_DIST_8X8
        // TODO(anyone): Fix dist-8x8 assert failure here when CFL is enabled
        if (x->tune_metric == AOM_TUNE_PSNR && xd->bd == 8 && 0 /*!CONFIG_CFL*/)
          assert(sum_rdc.dist == dist_8x8);
#endif  // DEBUG_DIST_8X8
        sum_rdc.dist = dist_8x8;
        sum_rdc.rdcost = RDCOST(x->rdmult, sum_rdc.rate, sum_rdc.dist);
      }
#endif  // CONFIG_DIST_8X8
    }

    if (sum_rdc.rdcost < best_rdc.rdcost) {
      sum_rdc.rate += partition_cost[PARTITION_HORZ];
      sum_rdc.rdcost = RDCOST(x->rdmult, sum_rdc.rate, sum_rdc.dist);
      if (sum_rdc.rdcost < best_rdc.rdcost) {
        best_rdc = sum_rdc;
        pc_tree->partitioning = PARTITION_HORZ;
      }
    }

    restore_context(x, &x_ctx, mi_row, mi_col, bsize, num_planes);
  }

  // PARTITION_VERT
  if (partition_vert_allowed &&
      (do_rectangular_split || active_v_edge(cpi, mi_col, mi_step))) {
    av1_init_rd_stats(&sum_rdc);
    subsize = get_partition_subsize(bsize, PARTITION_VERT);

    if (cpi->sf.adaptive_motion_search) load_pred_mv(x, ctx_none);

    if (cpi->sf.adaptive_pred_interp_filter && bsize == BLOCK_8X8 &&
        partition_none_allowed)
      pc_tree->vertical[0].pred_interp_filter =
          av1_extract_interp_filter(ctx_none->mic.interp_filters, 0);

    rd_pick_sb_modes(cpi, tile_data, x, mi_row, mi_col, &sum_rdc,
                     PARTITION_VERT, subsize, &pc_tree->vertical[0],
                     best_rdc.rdcost);
    vert_rd[0] = sum_rdc.rdcost;
    const int64_t vert_max_rdcost = best_rdc.rdcost;
    if (sum_rdc.rdcost < vert_max_rdcost && has_cols) {
      MB_MODE_INFO *const mbmi = &(pc_tree->vertical[0].mic);
      PALETTE_MODE_INFO *const pmi = &mbmi->palette_mode_info;
      // Neither palette mode nor cfl predicted
      if (pmi->palette_size[0] == 0 && pmi->palette_size[1] == 0) {
        if (mbmi->uv_mode != UV_CFL_PRED) vert_ctx_is_ready = 1;
      }
      update_state(cpi, tile_data, td, &pc_tree->vertical[0], mi_row, mi_col,
                   subsize, 1);
      encode_superblock(cpi, tile_data, td, tp, DRY_RUN_NORMAL, mi_row, mi_col,
                        subsize, NULL);

      if (cpi->sf.adaptive_motion_search) load_pred_mv(x, ctx_none);

      if (cpi->sf.adaptive_pred_interp_filter && bsize == BLOCK_8X8 &&
          partition_none_allowed)
        pc_tree->vertical[1].pred_interp_filter =
            av1_extract_interp_filter(ctx_none->mic.interp_filters, 0);

      rd_pick_sb_modes(cpi, tile_data, x, mi_row, mi_col + mi_step, &this_rdc,
                       PARTITION_VERT, subsize, &pc_tree->vertical[1],
                       best_rdc.rdcost - sum_rdc.rdcost);
      vert_rd[1] = this_rdc.rdcost;

#if CONFIG_DIST_8X8
      if (x->using_dist_8x8 && this_rdc.rate != INT_MAX && bsize == BLOCK_8X8) {
        update_state(cpi, tile_data, td, &pc_tree->vertical[1], mi_row,
                     mi_col + mi_step, subsize, DRY_RUN_NORMAL);
        encode_superblock(cpi, tile_data, td, tp, DRY_RUN_NORMAL, mi_row,
                          mi_col + mi_step, subsize, NULL);
      }
#endif  // CONFIG_DIST_8X8

      if (this_rdc.rate == INT_MAX) {
        sum_rdc.rdcost = INT64_MAX;
      } else {
        sum_rdc.rate += this_rdc.rate;
        sum_rdc.dist += this_rdc.dist;
        sum_rdc.rdcost += this_rdc.rdcost;
      }
#if CONFIG_DIST_8X8
      if (x->using_dist_8x8 && sum_rdc.rdcost != INT64_MAX &&
          bsize == BLOCK_8X8) {
        int64_t dist_8x8;
        dist_8x8 = dist_8x8_yuv(cpi, x, src_plane_8x8, dst_plane_8x8);
#ifdef DEBUG_DIST_8X8
        // TODO(anyone): Fix dist-8x8 assert failure here when CFL is enabled
        if (x->tune_metric == AOM_TUNE_PSNR && xd->bd == 8 &&
            0 /* !CONFIG_CFL */)
          assert(sum_rdc.dist == dist_8x8);
#endif  // DEBUG_DIST_8X8
        sum_rdc.dist = dist_8x8;
        sum_rdc.rdcost = RDCOST(x->rdmult, sum_rdc.rate, sum_rdc.dist);
      }
#endif  // CONFIG_DIST_8X8
    }

    if (sum_rdc.rdcost < best_rdc.rdcost) {
      sum_rdc.rate += partition_cost[PARTITION_VERT];
      sum_rdc.rdcost = RDCOST(x->rdmult, sum_rdc.rate, sum_rdc.dist);
      if (sum_rdc.rdcost < best_rdc.rdcost) {
        best_rdc = sum_rdc;
        pc_tree->partitioning = PARTITION_VERT;
      }
    }

    restore_context(x, &x_ctx, mi_row, mi_col, bsize, num_planes);
  }

  const int ext_partition_allowed =
      do_rectangular_split && bsize > BLOCK_8X8 && partition_none_allowed;

  // partition4_allowed is 1 if we can use a PARTITION_HORZ_4 or
  // PARTITION_VERT_4 for this block. This is almost the same as
  // ext_partition_allowed, except that we don't allow 128x32 or 32x128 blocks,
  // so we require that bsize is not BLOCK_128X128.
  const int partition4_allowed =
      ext_partition_allowed && bsize != BLOCK_128X128;

  // The standard AB partitions are allowed whenever ext-partition-types are
  // allowed
  int horzab_partition_allowed = ext_partition_allowed;
  int vertab_partition_allowed = ext_partition_allowed;

  if (cpi->sf.prune_ext_partition_types_search_level) {
    if (cpi->sf.prune_ext_partition_types_search_level == 1) {
      horzab_partition_allowed &= (pc_tree->partitioning == PARTITION_HORZ ||
                                   (pc_tree->partitioning == PARTITION_NONE &&
                                    x->source_variance < 32) ||
                                   pc_tree->partitioning == PARTITION_SPLIT);
      vertab_partition_allowed &= (pc_tree->partitioning == PARTITION_VERT ||
                                   (pc_tree->partitioning == PARTITION_NONE &&
                                    x->source_variance < 32) ||
                                   pc_tree->partitioning == PARTITION_SPLIT);
    } else {
      horzab_partition_allowed &= (pc_tree->partitioning == PARTITION_HORZ ||
                                   pc_tree->partitioning == PARTITION_SPLIT);
      vertab_partition_allowed &= (pc_tree->partitioning == PARTITION_VERT ||
                                   pc_tree->partitioning == PARTITION_SPLIT);
    }
    horz_rd[0] = (horz_rd[0] < INT64_MAX ? horz_rd[0] : 0);
    horz_rd[1] = (horz_rd[1] < INT64_MAX ? horz_rd[1] : 0);
    vert_rd[0] = (vert_rd[0] < INT64_MAX ? vert_rd[0] : 0);
    vert_rd[1] = (vert_rd[1] < INT64_MAX ? vert_rd[1] : 0);
    split_rd[0] = (split_rd[0] < INT64_MAX ? split_rd[0] : 0);
    split_rd[1] = (split_rd[1] < INT64_MAX ? split_rd[1] : 0);
    split_rd[2] = (split_rd[2] < INT64_MAX ? split_rd[2] : 0);
    split_rd[3] = (split_rd[3] < INT64_MAX ? split_rd[3] : 0);
  }
  int horza_partition_allowed = horzab_partition_allowed;
  int horzb_partition_allowed = horzab_partition_allowed;
  if (cpi->sf.prune_ext_partition_types_search_level) {
    const int64_t horz_a_rd = horz_rd[1] + split_rd[0] + split_rd[1];
    const int64_t horz_b_rd = horz_rd[0] + split_rd[2] + split_rd[3];
    switch (cpi->sf.prune_ext_partition_types_search_level) {
      case 1:
        horza_partition_allowed &= (horz_a_rd / 16 * 14 < best_rdc.rdcost);
        horzb_partition_allowed &= (horz_b_rd / 16 * 14 < best_rdc.rdcost);
        break;
      case 2:
      default:
        horza_partition_allowed &= (horz_a_rd / 16 * 15 < best_rdc.rdcost);
        horzb_partition_allowed &= (horz_b_rd / 16 * 15 < best_rdc.rdcost);
        break;
    }
  }

  int verta_partition_allowed = vertab_partition_allowed;
  int vertb_partition_allowed = vertab_partition_allowed;
  if (cpi->sf.prune_ext_partition_types_search_level) {
    const int64_t vert_a_rd = vert_rd[1] + split_rd[0] + split_rd[2];
    const int64_t vert_b_rd = vert_rd[0] + split_rd[1] + split_rd[3];
    switch (cpi->sf.prune_ext_partition_types_search_level) {
      case 1:
        verta_partition_allowed &= (vert_a_rd / 16 * 14 < best_rdc.rdcost);
        vertb_partition_allowed &= (vert_b_rd / 16 * 14 < best_rdc.rdcost);
        break;
      case 2:
      default:
        verta_partition_allowed &= (vert_a_rd / 16 * 15 < best_rdc.rdcost);
        vertb_partition_allowed &= (vert_b_rd / 16 * 15 < best_rdc.rdcost);
        break;
    }
  }

  if (cpi->sf.ml_prune_ab_partition && ext_partition_allowed &&
      partition_horz_allowed && partition_vert_allowed) {
    ml_prune_ab_partition(bsize, pc_tree->partitioning,
                          get_unsigned_bits(x->source_variance),
                          best_rdc.rdcost, horz_rd, vert_rd, split_rd,
                          &horza_partition_allowed, &horzb_partition_allowed,
                          &verta_partition_allowed, &vertb_partition_allowed);
  }

  // PARTITION_HORZ_A
  if (partition_horz_allowed && horza_partition_allowed) {
    subsize = get_partition_subsize(bsize, PARTITION_HORZ_A);
    pc_tree->horizontala[0].rd_mode_is_ready = 0;
    pc_tree->horizontala[1].rd_mode_is_ready = 0;
    pc_tree->horizontala[2].rd_mode_is_ready = 0;
    if (split_ctx_is_ready[0]) {
      av1_copy_tree_context(&pc_tree->horizontala[0], &pc_tree->split[0]->none);
      pc_tree->horizontala[0].mic.partition = PARTITION_HORZ_A;
      pc_tree->horizontala[0].rd_mode_is_ready = 1;
      if (split_ctx_is_ready[1]) {
        av1_copy_tree_context(&pc_tree->horizontala[1],
                              &pc_tree->split[1]->none);
        pc_tree->horizontala[1].mic.partition = PARTITION_HORZ_A;
        pc_tree->horizontala[1].rd_mode_is_ready = 1;
      }
    }
    rd_test_partition3(cpi, td, tile_data, tp, pc_tree, &best_rdc,
                       pc_tree->horizontala, ctx_none, mi_row, mi_col, bsize,
                       PARTITION_HORZ_A, mi_row, mi_col, bsize2, mi_row,
                       mi_col + mi_step, bsize2, mi_row + mi_step, mi_col,
                       subsize);
    restore_context(x, &x_ctx, mi_row, mi_col, bsize, num_planes);
  }
  // PARTITION_HORZ_B
  if (partition_horz_allowed && horzb_partition_allowed) {
    subsize = get_partition_subsize(bsize, PARTITION_HORZ_B);
    pc_tree->horizontalb[0].rd_mode_is_ready = 0;
    pc_tree->horizontalb[1].rd_mode_is_ready = 0;
    pc_tree->horizontalb[2].rd_mode_is_ready = 0;
    if (horz_ctx_is_ready) {
      av1_copy_tree_context(&pc_tree->horizontalb[0], &pc_tree->horizontal[0]);
      pc_tree->horizontalb[0].mic.partition = PARTITION_HORZ_B;
      pc_tree->horizontalb[0].rd_mode_is_ready = 1;
    }
    rd_test_partition3(cpi, td, tile_data, tp, pc_tree, &best_rdc,
                       pc_tree->horizontalb, ctx_none, mi_row, mi_col, bsize,
                       PARTITION_HORZ_B, mi_row, mi_col, subsize,
                       mi_row + mi_step, mi_col, bsize2, mi_row + mi_step,
                       mi_col + mi_step, bsize2);
    restore_context(x, &x_ctx, mi_row, mi_col, bsize, num_planes);
  }

  // PARTITION_VERT_A
  if (partition_vert_allowed && verta_partition_allowed) {
    subsize = get_partition_subsize(bsize, PARTITION_VERT_A);
    pc_tree->verticala[0].rd_mode_is_ready = 0;
    pc_tree->verticala[1].rd_mode_is_ready = 0;
    pc_tree->verticala[2].rd_mode_is_ready = 0;
    if (split_ctx_is_ready[0]) {
      av1_copy_tree_context(&pc_tree->verticala[0], &pc_tree->split[0]->none);
      pc_tree->verticala[0].mic.partition = PARTITION_VERT_A;
      pc_tree->verticala[0].rd_mode_is_ready = 1;
    }
    rd_test_partition3(cpi, td, tile_data, tp, pc_tree, &best_rdc,
                       pc_tree->verticala, ctx_none, mi_row, mi_col, bsize,
                       PARTITION_VERT_A, mi_row, mi_col, bsize2,
                       mi_row + mi_step, mi_col, bsize2, mi_row,
                       mi_col + mi_step, subsize);
    restore_context(x, &x_ctx, mi_row, mi_col, bsize, num_planes);
  }
  // PARTITION_VERT_B
  if (partition_vert_allowed && vertb_partition_allowed) {
    subsize = get_partition_subsize(bsize, PARTITION_VERT_B);
    pc_tree->verticalb[0].rd_mode_is_ready = 0;
    pc_tree->verticalb[1].rd_mode_is_ready = 0;
    pc_tree->verticalb[2].rd_mode_is_ready = 0;
    if (vert_ctx_is_ready) {
      av1_copy_tree_context(&pc_tree->verticalb[0], &pc_tree->vertical[0]);
      pc_tree->verticalb[0].mic.partition = PARTITION_VERT_B;
      pc_tree->verticalb[0].rd_mode_is_ready = 1;
    }
    rd_test_partition3(cpi, td, tile_data, tp, pc_tree, &best_rdc,
                       pc_tree->verticalb, ctx_none, mi_row, mi_col, bsize,
                       PARTITION_VERT_B, mi_row, mi_col, subsize, mi_row,
                       mi_col + mi_step, bsize2, mi_row + mi_step,
                       mi_col + mi_step, bsize2);
    restore_context(x, &x_ctx, mi_row, mi_col, bsize, num_planes);
  }

  // PARTITION_HORZ_4
  int partition_horz4_allowed = partition4_allowed && partition_horz_allowed;
  if (cpi->sf.prune_ext_partition_types_search_level == 2) {
    partition_horz4_allowed &= (pc_tree->partitioning == PARTITION_HORZ ||
                                pc_tree->partitioning == PARTITION_HORZ_A ||
                                pc_tree->partitioning == PARTITION_HORZ_B ||
                                pc_tree->partitioning == PARTITION_SPLIT ||
                                pc_tree->partitioning == PARTITION_NONE);
  }
  if (partition_horz4_allowed && has_rows &&
      (do_rectangular_split || active_h_edge(cpi, mi_row, mi_step))) {
    av1_init_rd_stats(&sum_rdc);
    const int quarter_step = mi_size_high[bsize] / 4;
    PICK_MODE_CONTEXT *ctx_prev = ctx_none;

    subsize = get_partition_subsize(bsize, PARTITION_HORZ_4);

    for (int i = 0; i < 4; ++i) {
      int this_mi_row = mi_row + i * quarter_step;

      if (i > 0 && this_mi_row >= cm->mi_rows) break;

      PICK_MODE_CONTEXT *ctx_this = &pc_tree->horizontal4[i];

      ctx_this->rd_mode_is_ready = 0;
      if (!rd_try_subblock(cpi, td, tile_data, tp, (i == 0), (i == 3),
                           this_mi_row, mi_col, subsize, &best_rdc, &sum_rdc,
                           &this_rdc, PARTITION_HORZ_4, ctx_prev, ctx_this))
        break;

      ctx_prev = ctx_this;
    }

    if (sum_rdc.rdcost < best_rdc.rdcost) {
      sum_rdc.rate += partition_cost[PARTITION_HORZ_4];
      sum_rdc.rdcost = RDCOST(x->rdmult, sum_rdc.rate, sum_rdc.dist);
      if (sum_rdc.rdcost < best_rdc.rdcost) {
        best_rdc = sum_rdc;
        pc_tree->partitioning = PARTITION_HORZ_4;
      }
    }
    restore_context(x, &x_ctx, mi_row, mi_col, bsize, num_planes);
  }

  // PARTITION_VERT_4
  int partition_vert4_allowed = partition4_allowed && partition_vert_allowed;
  if (cpi->sf.prune_ext_partition_types_search_level == 2) {
    partition_vert4_allowed &= (pc_tree->partitioning == PARTITION_VERT ||
                                pc_tree->partitioning == PARTITION_VERT_A ||
                                pc_tree->partitioning == PARTITION_VERT_B ||
                                pc_tree->partitioning == PARTITION_SPLIT ||
                                pc_tree->partitioning == PARTITION_NONE);
  }
  if (partition_vert4_allowed && has_cols &&
      (do_rectangular_split || active_v_edge(cpi, mi_row, mi_step))) {
    av1_init_rd_stats(&sum_rdc);
    const int quarter_step = mi_size_wide[bsize] / 4;
    PICK_MODE_CONTEXT *ctx_prev = ctx_none;

    subsize = get_partition_subsize(bsize, PARTITION_VERT_4);

    for (int i = 0; i < 4; ++i) {
      int this_mi_col = mi_col + i * quarter_step;

      if (i > 0 && this_mi_col >= cm->mi_cols) break;

      PICK_MODE_CONTEXT *ctx_this = &pc_tree->vertical4[i];

      ctx_this->rd_mode_is_ready = 0;
      if (!rd_try_subblock(cpi, td, tile_data, tp, (i == 0), (i == 3), mi_row,
                           this_mi_col, subsize, &best_rdc, &sum_rdc, &this_rdc,
                           PARTITION_VERT_4, ctx_prev, ctx_this))
        break;

      ctx_prev = ctx_this;
    }

    if (sum_rdc.rdcost < best_rdc.rdcost) {
      sum_rdc.rate += partition_cost[PARTITION_VERT_4];
      sum_rdc.rdcost = RDCOST(x->rdmult, sum_rdc.rate, sum_rdc.dist);
      if (sum_rdc.rdcost < best_rdc.rdcost) {
        best_rdc = sum_rdc;
        pc_tree->partitioning = PARTITION_VERT_4;
      }
    }
    restore_context(x, &x_ctx, mi_row, mi_col, bsize, num_planes);
  }

  if (bsize == cm->seq_params.sb_size && best_rdc.rate == INT_MAX) {
    // Did not find a valid partition, go back and search again, with less
    // constraint on which partition types to search.
    x->must_find_valid_partition = 1;
    goto BEGIN_PARTITION_SEARCH;
  }

  // TODO(jbb): This code added so that we avoid static analysis
  // warning related to the fact that best_rd isn't used after this
  // point.  This code should be refactored so that the duplicate
  // checks occur in some sub function and thus are used...
  (void)best_rd;
  *rd_cost = best_rdc;

  if (best_rdc.rate < INT_MAX && best_rdc.dist < INT64_MAX &&
      pc_tree->index != 3) {
    if (bsize == cm->seq_params.sb_size) {
      x->cb_offset = 0;
      encode_sb(cpi, td, tile_data, tp, mi_row, mi_col, OUTPUT_ENABLED, bsize,
                pc_tree, NULL);
    } else {
      encode_sb(cpi, td, tile_data, tp, mi_row, mi_col, DRY_RUN_NORMAL, bsize,
                pc_tree, NULL);
    }
  }

#if CONFIG_DIST_8X8
  if (x->using_dist_8x8 && best_rdc.rate < INT_MAX &&
      best_rdc.dist < INT64_MAX && bsize == BLOCK_4X4 && pc_tree->index == 3) {
    encode_sb(cpi, td, tile_data, tp, mi_row, mi_col, DRY_RUN_NORMAL, bsize,
              pc_tree, NULL);
  }
#endif  // CONFIG_DIST_8X8

  if (bsize == cm->seq_params.sb_size) {
    assert(best_rdc.rate < INT_MAX);
    assert(best_rdc.dist < INT64_MAX);
  } else {
    assert(tp_orig == *tp);
  }
}

// Set all the counters as max.
static void init_first_partition_pass_stats_tables(
    FIRST_PARTITION_PASS_STATS *stats) {
  for (int i = 0; i < FIRST_PARTITION_PASS_STATS_TABLES; ++i) {
    memset(stats[i].ref0_counts, 0xff, sizeof(stats[i].ref0_counts));
    memset(stats[i].ref1_counts, 0xff, sizeof(stats[i].ref1_counts));
    stats[i].sample_counts = INT_MAX;
  }
}

// Minimum number of samples to trigger the
// mode_pruning_based_on_two_pass_partition_search feature.
#define FIRST_PARTITION_PASS_MIN_SAMPLES 16

static void encode_rd_sb_row(AV1_COMP *cpi, ThreadData *td,
                             TileDataEnc *tile_data, int mi_row,
                             TOKENEXTRA **tp) {
  AV1_COMMON *const cm = &cpi->common;
  const int num_planes = av1_num_planes(cm);
  const TileInfo *const tile_info = &tile_data->tile_info;
  MACROBLOCK *const x = &td->mb;
  MACROBLOCKD *const xd = &x->e_mbd;
  SPEED_FEATURES *const sf = &cpi->sf;
  int mi_col;
  const int leaf_nodes = 256;

  // Initialize the left context for the new SB row
  av1_zero_left_context(xd);

  // Reset delta for every tile
  if (mi_row == tile_info->mi_row_start) {
    if (cm->delta_q_present_flag) xd->current_qindex = cm->base_qindex;
    if (cm->delta_lf_present_flag) {
      av1_reset_loop_filter_delta(xd, av1_num_planes(cm));
    }
  }

  // Code each SB in the row
  for (mi_col = tile_info->mi_col_start; mi_col < tile_info->mi_col_end;
       mi_col += cm->seq_params.mib_size) {
    const struct segmentation *const seg = &cm->seg;
    int dummy_rate;
    int64_t dummy_dist;
    RD_STATS dummy_rdc;
    int i;
    int seg_skip = 0;

    const int idx_str = cm->mi_stride * mi_row + mi_col;
    MB_MODE_INFO **mi = cm->mi_grid_visible + idx_str;
    PC_TREE *const pc_root =
        td->pc_root[cm->seq_params.mib_size_log2 - MIN_MIB_SIZE_LOG2];

    av1_fill_coeff_costs(&td->mb, xd->tile_ctx, num_planes);
    av1_fill_mode_rates(cm, x, xd->tile_ctx);

    if (sf->adaptive_pred_interp_filter) {
      for (i = 0; i < leaf_nodes; ++i) {
        td->pc_tree[i].vertical[0].pred_interp_filter = SWITCHABLE;
        td->pc_tree[i].vertical[1].pred_interp_filter = SWITCHABLE;
        td->pc_tree[i].horizontal[0].pred_interp_filter = SWITCHABLE;
        td->pc_tree[i].horizontal[1].pred_interp_filter = SWITCHABLE;
      }
    }

    x->mb_rd_record.num = x->mb_rd_record.index_start = 0;

    av1_zero(x->txb_rd_record_8X8);
    av1_zero(x->txb_rd_record_16X16);
    av1_zero(x->txb_rd_record_32X32);
    av1_zero(x->txb_rd_record_64X64);
    av1_zero(x->txb_rd_record_intra);

    av1_zero(x->pred_mv);
    pc_root->index = 0;

    if (seg->enabled) {
      const uint8_t *const map =
          seg->update_map ? cpi->segmentation_map : cm->last_frame_seg_map;
      int segment_id =
          map ? get_segment_id(cm, map, cm->seq_params.sb_size, mi_row, mi_col)
              : 0;
      seg_skip = segfeature_active(seg, segment_id, SEG_LVL_SKIP);
    }
    xd->cur_frame_force_integer_mv = cm->cur_frame_force_integer_mv;

    if (cm->delta_q_present_flag) {
      // Delta-q modulation based on variance
      av1_setup_src_planes(x, cpi->source, mi_row, mi_col, num_planes);

      int offset_qindex;
      if (DELTAQ_MODULATION == 1) {
        const int block_wavelet_energy_level =
            av1_block_wavelet_energy_level(cpi, x, cm->seq_params.sb_size);
        offset_qindex = av1_compute_deltaq_from_energy_level(
            cpi, block_wavelet_energy_level);
      } else {
        const int block_var_level =
            av1_block_energy(cpi, x, cm->seq_params.sb_size);
        offset_qindex =
            av1_compute_deltaq_from_energy_level(cpi, block_var_level);
      }
      int qmask = ~(cm->delta_q_res - 1);
      int current_qindex = clamp(cm->base_qindex + offset_qindex,
                                 cm->delta_q_res, 256 - cm->delta_q_res);

      current_qindex =
          ((current_qindex - cm->base_qindex + cm->delta_q_res / 2) & qmask) +
          cm->base_qindex;
      assert(current_qindex > 0);

      xd->delta_qindex = current_qindex - cm->base_qindex;
      set_offsets(cpi, tile_info, x, mi_row, mi_col, cm->seq_params.sb_size);
      xd->mi[0]->current_qindex = current_qindex;
      av1_init_plane_quantizers(cpi, x, xd->mi[0]->segment_id);
      if (cpi->oxcf.deltaq_mode == DELTA_Q_LF) {
        int j, k;
        int lfmask = ~(cm->delta_lf_res - 1);
        int delta_lf_from_base = offset_qindex / 2;
        delta_lf_from_base =
            ((delta_lf_from_base + cm->delta_lf_res / 2) & lfmask);

        // pre-set the delta lf for loop filter. Note that this value is set
        // before mi is assigned for each block in current superblock
        for (j = 0; j < AOMMIN(cm->seq_params.mib_size, cm->mi_rows - mi_row);
             j++) {
          for (k = 0; k < AOMMIN(cm->seq_params.mib_size, cm->mi_cols - mi_col);
               k++) {
            cm->mi[(mi_row + j) * cm->mi_stride + (mi_col + k)]
                .delta_lf_from_base =
                clamp(delta_lf_from_base, -MAX_LOOP_FILTER, MAX_LOOP_FILTER);
            const int frame_lf_count =
                av1_num_planes(cm) > 1 ? FRAME_LF_COUNT : FRAME_LF_COUNT - 2;
            for (int lf_id = 0; lf_id < frame_lf_count; ++lf_id) {
              cm->mi[(mi_row + j) * cm->mi_stride + (mi_col + k)]
                  .delta_lf[lf_id] =
                  clamp(delta_lf_from_base, -MAX_LOOP_FILTER, MAX_LOOP_FILTER);
            }
          }
        }
      }
    }

    x->source_variance = UINT_MAX;
    if (sf->partition_search_type == FIXED_PARTITION || seg_skip) {
      BLOCK_SIZE bsize;
      set_offsets(cpi, tile_info, x, mi_row, mi_col, cm->seq_params.sb_size);
      bsize = seg_skip ? cm->seq_params.sb_size : sf->always_this_block_size;
      set_fixed_partitioning(cpi, tile_info, mi, mi_row, mi_col, bsize);
      rd_use_partition(cpi, td, tile_data, mi, tp, mi_row, mi_col,
                       cm->seq_params.sb_size, &dummy_rate, &dummy_dist, 1,
                       pc_root);
    } else if (cpi->partition_search_skippable_frame) {
      BLOCK_SIZE bsize;
      set_offsets(cpi, tile_info, x, mi_row, mi_col, cm->seq_params.sb_size);
      bsize = get_rd_var_based_fixed_partition(cpi, x, mi_row, mi_col);
      set_fixed_partitioning(cpi, tile_info, mi, mi_row, mi_col, bsize);
      rd_use_partition(cpi, td, tile_data, mi, tp, mi_row, mi_col,
                       cm->seq_params.sb_size, &dummy_rate, &dummy_dist, 1,
                       pc_root);
    } else {
      // If required set upper and lower partition size limits
      if (sf->auto_min_max_partition_size) {
        set_offsets(cpi, tile_info, x, mi_row, mi_col, cm->seq_params.sb_size);
        rd_auto_partition_range(cpi, tile_info, xd, mi_row, mi_col,
                                &x->min_partition_size, &x->max_partition_size);
      }

      reset_partition(pc_root, cm->seq_params.sb_size);
      x->use_cb_search_range = 0;
      init_first_partition_pass_stats_tables(x->first_partition_pass_stats);
      if (cpi->sf.two_pass_partition_search &&
          mi_row + mi_size_high[cm->seq_params.sb_size] < cm->mi_rows &&
          mi_col + mi_size_wide[cm->seq_params.sb_size] < cm->mi_cols &&
          cm->frame_type != KEY_FRAME) {
        x->cb_partition_scan = 1;
        // Reset the stats tables.
        if (sf->mode_pruning_based_on_two_pass_partition_search)
          av1_zero(x->first_partition_pass_stats);
        rd_pick_sqr_partition(cpi, td, tile_data, tp, mi_row, mi_col,
                              cm->seq_params.sb_size, &dummy_rdc, INT64_MAX,
                              pc_root, NULL);
        x->cb_partition_scan = 0;

        x->source_variance = UINT_MAX;
        if (sf->adaptive_pred_interp_filter) {
          for (i = 0; i < leaf_nodes; ++i) {
            td->pc_tree[i].vertical[0].pred_interp_filter = SWITCHABLE;
            td->pc_tree[i].vertical[1].pred_interp_filter = SWITCHABLE;
            td->pc_tree[i].horizontal[0].pred_interp_filter = SWITCHABLE;
            td->pc_tree[i].horizontal[1].pred_interp_filter = SWITCHABLE;
          }
        }

        x->mb_rd_record.num = x->mb_rd_record.index_start = 0;
        av1_zero(x->txb_rd_record_8X8);
        av1_zero(x->txb_rd_record_16X16);
        av1_zero(x->txb_rd_record_32X32);
        av1_zero(x->txb_rd_record_64X64);
        av1_zero(x->txb_rd_record_intra);
        av1_zero(x->pred_mv);
        pc_root->index = 0;

        for (int idy = 0; idy < mi_size_high[cm->seq_params.sb_size]; ++idy) {
          for (int idx = 0; idx < mi_size_wide[cm->seq_params.sb_size]; ++idx) {
            const int offset = cm->mi_stride * (mi_row + idy) + (mi_col + idx);
            cm->mi_grid_visible[offset] = 0;
          }
        }

        x->use_cb_search_range = 1;

        if (sf->mode_pruning_based_on_two_pass_partition_search) {
          for (i = 0; i < FIRST_PARTITION_PASS_STATS_TABLES; ++i) {
            FIRST_PARTITION_PASS_STATS *const stat =
                &x->first_partition_pass_stats[i];
            if (stat->sample_counts < FIRST_PARTITION_PASS_MIN_SAMPLES) {
              // If there are not enough samples collected, make all available.
              memset(stat->ref0_counts, 0xff, sizeof(stat->ref0_counts));
              memset(stat->ref1_counts, 0xff, sizeof(stat->ref1_counts));
            } else if (sf->selective_ref_frame < 2) {
              // ALTREF2_FRAME and BWDREF_FRAME may be skipped during the
              // initial partition scan, so we don't eliminate them.
              stat->ref0_counts[ALTREF2_FRAME] = 0xff;
              stat->ref1_counts[ALTREF2_FRAME] = 0xff;
              stat->ref0_counts[BWDREF_FRAME] = 0xff;
              stat->ref1_counts[BWDREF_FRAME] = 0xff;
            }
          }
        }

        rd_pick_partition(cpi, td, tile_data, tp, mi_row, mi_col,
                          cm->seq_params.sb_size, &dummy_rdc, INT64_MAX,
                          pc_root, NULL);
      } else {
        rd_pick_partition(cpi, td, tile_data, tp, mi_row, mi_col,
                          cm->seq_params.sb_size, &dummy_rdc, INT64_MAX,
                          pc_root, NULL);
      }
    }
#if CONFIG_COLLECT_INTER_MODE_RD_STATS
    // TODO(angiebird): Let inter_mode_rd_model_estimation support multi-tile.
    if (cpi->sf.inter_mode_rd_model_estimation && cm->tile_cols == 1 &&
        cm->tile_rows == 1) {
      av1_inter_mode_data_fit(x->rdmult);
    }
#endif
  }
}

static void init_encode_frame_mb_context(AV1_COMP *cpi) {
  AV1_COMMON *const cm = &cpi->common;
  const int num_planes = av1_num_planes(cm);
  MACROBLOCK *const x = &cpi->td.mb;
  MACROBLOCKD *const xd = &x->e_mbd;

  // Copy data over into macro block data structures.
  av1_setup_src_planes(x, cpi->source, 0, 0, num_planes);

  av1_setup_block_planes(xd, cm->subsampling_x, cm->subsampling_y, num_planes);
}

static MV_REFERENCE_FRAME get_frame_type(const AV1_COMP *cpi) {
  if (frame_is_intra_only(&cpi->common)) return INTRA_FRAME;
  // We will not update the golden frame with an internal overlay frame
  else if ((cpi->rc.is_src_frame_alt_ref && cpi->refresh_golden_frame) ||
           cpi->rc.is_src_frame_ext_arf)
    return ALTREF_FRAME;
  else if (cpi->refresh_golden_frame || cpi->refresh_alt2_ref_frame ||
           cpi->refresh_alt_ref_frame)
    return GOLDEN_FRAME;
  else
    // TODO(zoeliu): To investigate whether a frame_type other than
    // INTRA/ALTREF/GOLDEN/LAST needs to be specified seperately.
    return LAST_FRAME;
}

static TX_MODE select_tx_mode(const AV1_COMP *cpi) {
  if (cpi->common.coded_lossless) return ONLY_4X4;
  if (cpi->sf.tx_size_search_method == USE_LARGESTALL)
    return TX_MODE_LARGEST;
  else if (cpi->sf.tx_size_search_method == USE_FULL_RD ||
           cpi->sf.tx_size_search_method == USE_FAST_RD)
    return TX_MODE_SELECT;
  else
    return cpi->common.tx_mode;
}

void av1_init_tile_data(AV1_COMP *cpi) {
  AV1_COMMON *const cm = &cpi->common;
  const int num_planes = av1_num_planes(cm);
  const int tile_cols = cm->tile_cols;
  const int tile_rows = cm->tile_rows;
  int tile_col, tile_row;
  TOKENEXTRA *pre_tok = cpi->tile_tok[0][0];
  unsigned int tile_tok = 0;

  if (cpi->tile_data == NULL || cpi->allocated_tiles < tile_cols * tile_rows) {
    if (cpi->tile_data != NULL) aom_free(cpi->tile_data);
    CHECK_MEM_ERROR(
        cm, cpi->tile_data,
        aom_memalign(32, tile_cols * tile_rows * sizeof(*cpi->tile_data)));
    cpi->allocated_tiles = tile_cols * tile_rows;

    for (tile_row = 0; tile_row < tile_rows; ++tile_row)
      for (tile_col = 0; tile_col < tile_cols; ++tile_col) {
        TileDataEnc *const tile_data =
            &cpi->tile_data[tile_row * tile_cols + tile_col];
        int i, j;
        for (i = 0; i < BLOCK_SIZES_ALL; ++i) {
          for (j = 0; j < MAX_MODES; ++j) {
            tile_data->thresh_freq_fact[i][j] = 32;
            tile_data->mode_map[i][j] = j;
          }
        }
      }
  }

  for (tile_row = 0; tile_row < tile_rows; ++tile_row) {
    for (tile_col = 0; tile_col < tile_cols; ++tile_col) {
      TileDataEnc *const tile_data =
          &cpi->tile_data[tile_row * tile_cols + tile_col];
      TileInfo *const tile_info = &tile_data->tile_info;
      av1_tile_init(tile_info, cm, tile_row, tile_col);

      cpi->tile_tok[tile_row][tile_col] = pre_tok + tile_tok;
      pre_tok = cpi->tile_tok[tile_row][tile_col];
      tile_tok = allocated_tokens(
          *tile_info, cm->seq_params.mib_size_log2 + MI_SIZE_LOG2, num_planes);
      tile_data->allow_update_cdf = !cm->large_scale_tile;
      tile_data->allow_update_cdf =
          tile_data->allow_update_cdf && !cm->disable_cdf_update;
    }
  }
}

void av1_encode_tile(AV1_COMP *cpi, ThreadData *td, int tile_row,
                     int tile_col) {
  AV1_COMMON *const cm = &cpi->common;
  TileDataEnc *const this_tile =
      &cpi->tile_data[tile_row * cm->tile_cols + tile_col];
  const TileInfo *const tile_info = &this_tile->tile_info;
  TOKENEXTRA *tok = cpi->tile_tok[tile_row][tile_col];
  int mi_row;

  av1_zero_above_context(cm, tile_info->mi_col_start, tile_info->mi_col_end,
                         tile_row);
  av1_init_above_context(cm, &td->mb.e_mbd, tile_row);

  // Set up pointers to per thread motion search counters.
  this_tile->m_search_count = 0;   // Count of motion search hits.
  this_tile->ex_search_count = 0;  // Exhaustive mesh search hits.
  td->mb.m_search_count_ptr = &this_tile->m_search_count;
  td->mb.ex_search_count_ptr = &this_tile->ex_search_count;
  this_tile->tctx = *cm->fc;
  td->mb.e_mbd.tile_ctx = &this_tile->tctx;

  cfl_init(&td->mb.e_mbd.cfl, cm);

  av1_crc32c_calculator_init(&td->mb.mb_rd_record.crc_calculator);

  td->intrabc_used_this_tile = 0;

  for (mi_row = tile_info->mi_row_start; mi_row < tile_info->mi_row_end;
       mi_row += cm->seq_params.mib_size) {
    encode_rd_sb_row(cpi, td, this_tile, mi_row, &tok);
  }

  cpi->tok_count[tile_row][tile_col] =
      (unsigned int)(tok - cpi->tile_tok[tile_row][tile_col]);
  assert(cpi->tok_count[tile_row][tile_col] <=
         allocated_tokens(*tile_info,
                          cm->seq_params.mib_size_log2 + MI_SIZE_LOG2,
                          av1_num_planes(cm)));
}

static void encode_tiles(AV1_COMP *cpi) {
  AV1_COMMON *const cm = &cpi->common;
  int tile_col, tile_row;

  av1_init_tile_data(cpi);

  for (tile_row = 0; tile_row < cm->tile_rows; ++tile_row) {
    for (tile_col = 0; tile_col < cm->tile_cols; ++tile_col) {
      av1_encode_tile(cpi, &cpi->td, tile_row, tile_col);
      cpi->intrabc_used |= cpi->td.intrabc_used_this_tile;
    }
  }
}

#if CONFIG_FP_MB_STATS
static int input_fpmb_stats(FIRSTPASS_MB_STATS *firstpass_mb_stats,
                            AV1_COMMON *cm, uint8_t **this_frame_mb_stats) {
  uint8_t *mb_stats_in = firstpass_mb_stats->mb_stats_start +
                         cm->current_video_frame * cm->MBs * sizeof(uint8_t);

  if (mb_stats_in > firstpass_mb_stats->mb_stats_end) return EOF;

  *this_frame_mb_stats = mb_stats_in;

  return 1;
}
#endif

#define GLOBAL_TRANS_TYPES_ENC 3  // highest motion model to search
static int gm_get_params_cost(const WarpedMotionParams *gm,
                              const WarpedMotionParams *ref_gm, int allow_hp) {
  int params_cost = 0;
  int trans_bits, trans_prec_diff;
  switch (gm->wmtype) {
    case AFFINE:
    case ROTZOOM:
      params_cost += aom_count_signed_primitive_refsubexpfin(
          GM_ALPHA_MAX + 1, SUBEXPFIN_K,
          (ref_gm->wmmat[2] >> GM_ALPHA_PREC_DIFF) - (1 << GM_ALPHA_PREC_BITS),
          (gm->wmmat[2] >> GM_ALPHA_PREC_DIFF) - (1 << GM_ALPHA_PREC_BITS));
      params_cost += aom_count_signed_primitive_refsubexpfin(
          GM_ALPHA_MAX + 1, SUBEXPFIN_K,
          (ref_gm->wmmat[3] >> GM_ALPHA_PREC_DIFF),
          (gm->wmmat[3] >> GM_ALPHA_PREC_DIFF));
      if (gm->wmtype >= AFFINE) {
        params_cost += aom_count_signed_primitive_refsubexpfin(
            GM_ALPHA_MAX + 1, SUBEXPFIN_K,
            (ref_gm->wmmat[4] >> GM_ALPHA_PREC_DIFF),
            (gm->wmmat[4] >> GM_ALPHA_PREC_DIFF));
        params_cost += aom_count_signed_primitive_refsubexpfin(
            GM_ALPHA_MAX + 1, SUBEXPFIN_K,
            (ref_gm->wmmat[5] >> GM_ALPHA_PREC_DIFF) -
                (1 << GM_ALPHA_PREC_BITS),
            (gm->wmmat[5] >> GM_ALPHA_PREC_DIFF) - (1 << GM_ALPHA_PREC_BITS));
      }
      AOM_FALLTHROUGH_INTENDED;
    case TRANSLATION:
      trans_bits = (gm->wmtype == TRANSLATION)
                       ? GM_ABS_TRANS_ONLY_BITS - !allow_hp
                       : GM_ABS_TRANS_BITS;
      trans_prec_diff = (gm->wmtype == TRANSLATION)
                            ? GM_TRANS_ONLY_PREC_DIFF + !allow_hp
                            : GM_TRANS_PREC_DIFF;
      params_cost += aom_count_signed_primitive_refsubexpfin(
          (1 << trans_bits) + 1, SUBEXPFIN_K,
          (ref_gm->wmmat[0] >> trans_prec_diff),
          (gm->wmmat[0] >> trans_prec_diff));
      params_cost += aom_count_signed_primitive_refsubexpfin(
          (1 << trans_bits) + 1, SUBEXPFIN_K,
          (ref_gm->wmmat[1] >> trans_prec_diff),
          (gm->wmmat[1] >> trans_prec_diff));
      AOM_FALLTHROUGH_INTENDED;
    case IDENTITY: break;
    default: assert(0);
  }
  return (params_cost << AV1_PROB_COST_SHIFT);
}

static int do_gm_search_logic(SPEED_FEATURES *const sf, int num_refs_using_gm,
                              int frame) {
  (void)num_refs_using_gm;
  (void)frame;
  switch (sf->gm_search_type) {
    case GM_FULL_SEARCH: return 1;
    case GM_REDUCED_REF_SEARCH:
      return !(frame == LAST2_FRAME || frame == LAST3_FRAME);
    case GM_DISABLE_SEARCH: return 0;
    default: assert(0);
  }
  return 1;
}

// Estimate if the source frame is screen content, based on the portion of
// blocks that have no more than 4 (experimentally selected) luma colors.
static int is_screen_content(const uint8_t *src, int use_hbd, int bd,
                             int stride, int width, int height) {
  assert(src != NULL);
  int counts = 0;
  const int blk_w = 16;
  const int blk_h = 16;
  const int limit = 4;
  for (int r = 0; r + blk_h <= height; r += blk_h) {
    for (int c = 0; c + blk_w <= width; c += blk_w) {
      int count_buf[1 << 12];  // Maximum (1 << 12) color levels.
      const int n_colors =
          use_hbd ? av1_count_colors_highbd(src + r * stride + c, stride, blk_w,
                                            blk_h, bd, count_buf)
                  : av1_count_colors(src + r * stride + c, stride, blk_w, blk_h,
                                     count_buf);
      if (n_colors > 1 && n_colors <= limit) counts++;
    }
  }
  // The threshold is 10%.
  return counts * blk_h * blk_w * 10 > width * height;
}

// Enforce the number of references for each arbitrary frame limited to
// (INTER_REFS_PER_FRAME - 1)
static void enforce_max_ref_frames(AV1_COMP *cpi) {
  AV1_COMMON *const cm = &cpi->common;
  static const int flag_list[REF_FRAMES] = { 0,
                                             AOM_LAST_FLAG,
                                             AOM_LAST2_FLAG,
                                             AOM_LAST3_FLAG,
                                             AOM_GOLD_FLAG,
                                             AOM_BWD_FLAG,
                                             AOM_ALT2_FLAG,
                                             AOM_ALT_FLAG };
  MV_REFERENCE_FRAME ref_frame;
  int total_valid_refs = 0;

  (void)flag_list;

  for (ref_frame = LAST_FRAME; ref_frame <= ALTREF_FRAME; ++ref_frame) {
    if (cpi->ref_frame_flags & flag_list[ref_frame]) total_valid_refs++;
  }

  // NOTE(zoeliu): When all the possible reference frames are availble, we
  // reduce the number of reference frames by 1, following the rules of:
  // (1) Retain GOLDEN_FARME/ALTEF_FRAME;
  // (2) Check the earliest 2 remaining reference frames, and remove the one
  //     with the lower quality factor, otherwise if both have been coded at
  //     the same quality level, remove the earliest reference frame.

  if (total_valid_refs == INTER_REFS_PER_FRAME) {
    unsigned int min_ref_offset = UINT_MAX;
    unsigned int second_min_ref_offset = UINT_MAX;
    MV_REFERENCE_FRAME earliest_ref_frames[2] = { LAST3_FRAME, LAST2_FRAME };
    int earliest_buf_idxes[2] = { 0 };

    // Locate the earliest two reference frames except GOLDEN/ALTREF.
    for (ref_frame = LAST_FRAME; ref_frame <= ALTREF_FRAME; ++ref_frame) {
      // Retain GOLDEN/ALTERF
      if (ref_frame == GOLDEN_FRAME || ref_frame == ALTREF_FRAME) continue;

      const int buf_idx = cm->frame_refs[ref_frame - LAST_FRAME].idx;
      if (buf_idx >= 0) {
        const unsigned int ref_offset =
            cm->buffer_pool->frame_bufs[buf_idx].cur_frame_offset;

        if (min_ref_offset == UINT_MAX) {
          min_ref_offset = ref_offset;
          earliest_ref_frames[0] = ref_frame;
          earliest_buf_idxes[0] = buf_idx;
        } else {
          if (get_relative_dist(cm, ref_offset, min_ref_offset) < 0) {
            second_min_ref_offset = min_ref_offset;
            earliest_ref_frames[1] = earliest_ref_frames[0];
            earliest_buf_idxes[1] = earliest_buf_idxes[0];

            min_ref_offset = ref_offset;
            earliest_ref_frames[0] = ref_frame;
            earliest_buf_idxes[0] = buf_idx;
          } else if (second_min_ref_offset == UINT_MAX ||
                     get_relative_dist(cm, ref_offset, second_min_ref_offset) <
                         0) {
            second_min_ref_offset = ref_offset;
            earliest_ref_frames[1] = ref_frame;
            earliest_buf_idxes[1] = buf_idx;
          }
        }
      }
    }
    // Check the coding quality factors of the two earliest reference frames.
    RATE_FACTOR_LEVEL ref_rf_level[2];
    double ref_rf_deltas[2];
    for (int i = 0; i < 2; ++i) {
      ref_rf_level[i] = cpi->frame_rf_level[earliest_buf_idxes[i]];
      ref_rf_deltas[i] = rate_factor_deltas[ref_rf_level[i]];
    }
    (void)ref_rf_level;
    (void)ref_rf_deltas;

#define USE_RF_LEVEL_TO_ENFORCE 1
#if USE_RF_LEVEL_TO_ENFORCE
    // If both earliest two reference frames are coded using the same rate-
    // factor, disable the earliest reference frame; Otherwise disable the
    // reference frame that uses a lower rate-factor delta.
    const MV_REFERENCE_FRAME ref_frame_to_disable =
        (ref_rf_deltas[0] <= ref_rf_deltas[1]) ? earliest_ref_frames[0]
                                               : earliest_ref_frames[1];
#else
    // Always disable the earliest reference frame
    const MV_REFERENCE_FRAME ref_frame_to_disable = earliest_ref_frames[0];
#endif  // USE_RF_LEVEL_TO_ENFORCE
#undef USE_RF_LEVEL_TO_ENFORCE

    switch (ref_frame_to_disable) {
      case LAST_FRAME: cpi->ref_frame_flags &= ~AOM_LAST_FLAG; break;
      case LAST2_FRAME: cpi->ref_frame_flags &= ~AOM_LAST2_FLAG; break;
      case LAST3_FRAME: cpi->ref_frame_flags &= ~AOM_LAST3_FLAG; break;
      case BWDREF_FRAME: cpi->ref_frame_flags &= ~AOM_BWD_FLAG; break;
      case ALTREF2_FRAME: cpi->ref_frame_flags &= ~AOM_ALT2_FLAG; break;
      default: break;
    }
  }
}

static INLINE int av1_refs_are_one_sided(const AV1_COMMON *cm) {
  assert(!frame_is_intra_only(cm));

  int one_sided_refs = 1;
  for (int ref = 0; ref < INTER_REFS_PER_FRAME; ++ref) {
    const int buf_idx = cm->frame_refs[ref].idx;
    if (buf_idx == INVALID_IDX) continue;

    const int ref_offset =
        cm->buffer_pool->frame_bufs[buf_idx].cur_frame_offset;
    if (get_relative_dist(cm, ref_offset, (int)cm->frame_offset) > 0) {
      one_sided_refs = 0;  // bwd reference
      break;
    }
  }
  return one_sided_refs;
}

static INLINE void get_skip_mode_ref_offsets(const AV1_COMMON *cm,
                                             int ref_offset[2]) {
  ref_offset[0] = ref_offset[1] = 0;
  if (!cm->is_skip_mode_allowed) return;

  const int buf_idx_0 = cm->frame_refs[cm->ref_frame_idx_0].idx;
  const int buf_idx_1 = cm->frame_refs[cm->ref_frame_idx_1].idx;
  assert(buf_idx_0 != INVALID_IDX && buf_idx_1 != INVALID_IDX);

  ref_offset[0] = cm->buffer_pool->frame_bufs[buf_idx_0].cur_frame_offset;
  ref_offset[1] = cm->buffer_pool->frame_bufs[buf_idx_1].cur_frame_offset;
}

static int check_skip_mode_enabled(AV1_COMP *const cpi) {
  AV1_COMMON *const cm = &cpi->common;

  av1_setup_skip_mode_allowed(cm);
  if (!cm->is_skip_mode_allowed) return 0;

  // Turn off skip mode if the temporal distances of the reference pair to the
  // current frame are different by more than 1 frame.
  const int cur_offset = (int)cm->frame_offset;
  int ref_offset[2];
  get_skip_mode_ref_offsets(cm, ref_offset);
  const int cur_to_ref0 = get_relative_dist(cm, cur_offset, ref_offset[0]);
  const int cur_to_ref1 = abs(get_relative_dist(cm, cur_offset, ref_offset[1]));
  if (abs(cur_to_ref0 - cur_to_ref1) > 1) return 0;

  // High Latency: Turn off skip mode if all refs are fwd.
  if (cpi->all_one_sided_refs && cpi->oxcf.lag_in_frames > 0) return 0;

  static const int flag_list[REF_FRAMES] = { 0,
                                             AOM_LAST_FLAG,
                                             AOM_LAST2_FLAG,
                                             AOM_LAST3_FLAG,
                                             AOM_GOLD_FLAG,
                                             AOM_BWD_FLAG,
                                             AOM_ALT2_FLAG,
                                             AOM_ALT_FLAG };
  const int ref_frame[2] = { cm->ref_frame_idx_0 + LAST_FRAME,
                             cm->ref_frame_idx_1 + LAST_FRAME };
  if (!(cpi->ref_frame_flags & flag_list[ref_frame[0]]) ||
      !(cpi->ref_frame_flags & flag_list[ref_frame[1]]))
    return 0;

  return 1;
}

// Function to decide if we can skip the global motion parameter computation
// for a particular ref frame
static INLINE int skip_gm_frame(AV1_COMMON *const cm, int ref_frame) {
  if ((ref_frame == LAST3_FRAME || ref_frame == LAST2_FRAME) &&
      cm->global_motion[GOLDEN_FRAME].wmtype != IDENTITY) {
    return get_relative_dist(
               cm, cm->cur_frame->ref_frame_offset[ref_frame - LAST_FRAME],
               cm->cur_frame->ref_frame_offset[GOLDEN_FRAME - LAST_FRAME]) <= 0;
  }
  return 0;
}

static void encode_frame_internal(AV1_COMP *cpi) {
  ThreadData *const td = &cpi->td;
  MACROBLOCK *const x = &td->mb;
  AV1_COMMON *const cm = &cpi->common;
  MACROBLOCKD *const xd = &x->e_mbd;
  RD_COUNTS *const rdc = &cpi->td.rd_counts;
  int i;

  x->min_partition_size = AOMMIN(x->min_partition_size, cm->seq_params.sb_size);
  x->max_partition_size = AOMMIN(x->max_partition_size, cm->seq_params.sb_size);
#if CONFIG_DIST_8X8
  x->using_dist_8x8 = cpi->oxcf.using_dist_8x8;
  x->tune_metric = cpi->oxcf.tuning;
#endif
  cm->setup_mi(cm);

  xd->mi = cm->mi_grid_visible;
  xd->mi[0] = cm->mi;

  av1_zero(*td->counts);
  av1_zero(rdc->comp_pred_diff);

  if (frame_is_intra_only(cm)) {
    if (cm->seq_params.force_screen_content_tools == 2) {
      cm->allow_screen_content_tools =
          cpi->oxcf.content == AOM_CONTENT_SCREEN ||
          is_screen_content(cpi->source->y_buffer,
                            cpi->source->flags & YV12_FLAG_HIGHBITDEPTH, xd->bd,
                            cpi->source->y_stride, cpi->source->y_width,
                            cpi->source->y_height);
    } else {
      cm->allow_screen_content_tools =
          cm->seq_params.force_screen_content_tools;
    }
  }

  // Allow intrabc when screen content tools are enabled.
  cm->allow_intrabc = cm->allow_screen_content_tools;
  // Reset the flag.
  cpi->intrabc_used = 0;
  // Need to disable intrabc when superres is selected
  if (av1_superres_scaled(cm)) {
    cm->allow_intrabc = 0;
  }

  if (cpi->oxcf.pass != 1 && av1_use_hash_me(cm)) {
    // add to hash table
    const int pic_width = cpi->source->y_crop_width;
    const int pic_height = cpi->source->y_crop_height;
    uint32_t *block_hash_values[2][2];
    int8_t *is_block_same[2][3];
    int k, j;

    for (k = 0; k < 2; k++) {
      for (j = 0; j < 2; j++) {
        CHECK_MEM_ERROR(cm, block_hash_values[k][j],
                        aom_malloc(sizeof(uint32_t) * pic_width * pic_height));
      }

      for (j = 0; j < 3; j++) {
        CHECK_MEM_ERROR(cm, is_block_same[k][j],
                        aom_malloc(sizeof(int8_t) * pic_width * pic_height));
      }
    }

    av1_hash_table_create(&cm->cur_frame->hash_table);
    av1_generate_block_2x2_hash_value(cpi->source, block_hash_values[0],
                                      is_block_same[0]);
    av1_generate_block_hash_value(cpi->source, 4, block_hash_values[0],
                                  block_hash_values[1], is_block_same[0],
                                  is_block_same[1]);
    av1_add_to_hash_map_by_row_with_precal_data(
        &cm->cur_frame->hash_table, block_hash_values[1], is_block_same[1][2],
        pic_width, pic_height, 4);
    av1_generate_block_hash_value(cpi->source, 8, block_hash_values[1],
                                  block_hash_values[0], is_block_same[1],
                                  is_block_same[0]);
    av1_add_to_hash_map_by_row_with_precal_data(
        &cm->cur_frame->hash_table, block_hash_values[0], is_block_same[0][2],
        pic_width, pic_height, 8);
    av1_generate_block_hash_value(cpi->source, 16, block_hash_values[0],
                                  block_hash_values[1], is_block_same[0],
                                  is_block_same[1]);
    av1_add_to_hash_map_by_row_with_precal_data(
        &cm->cur_frame->hash_table, block_hash_values[1], is_block_same[1][2],
        pic_width, pic_height, 16);
    av1_generate_block_hash_value(cpi->source, 32, block_hash_values[1],
                                  block_hash_values[0], is_block_same[1],
                                  is_block_same[0]);
    av1_add_to_hash_map_by_row_with_precal_data(
        &cm->cur_frame->hash_table, block_hash_values[0], is_block_same[0][2],
        pic_width, pic_height, 32);
    av1_generate_block_hash_value(cpi->source, 64, block_hash_values[0],
                                  block_hash_values[1], is_block_same[0],
                                  is_block_same[1]);
    av1_add_to_hash_map_by_row_with_precal_data(
        &cm->cur_frame->hash_table, block_hash_values[1], is_block_same[1][2],
        pic_width, pic_height, 64);

    av1_generate_block_hash_value(cpi->source, 128, block_hash_values[1],
                                  block_hash_values[0], is_block_same[1],
                                  is_block_same[0]);
    av1_add_to_hash_map_by_row_with_precal_data(
        &cm->cur_frame->hash_table, block_hash_values[0], is_block_same[0][2],
        pic_width, pic_height, 128);

    for (k = 0; k < 2; k++) {
      for (j = 0; j < 2; j++) {
        aom_free(block_hash_values[k][j]);
      }

      for (j = 0; j < 3; j++) {
        aom_free(is_block_same[k][j]);
      }
    }
  }

  for (i = 0; i < MAX_SEGMENTS; ++i) {
    const int qindex = cm->seg.enabled
                           ? av1_get_qindex(&cm->seg, i, cm->base_qindex)
                           : cm->base_qindex;
    xd->lossless[i] = qindex == 0 && cm->y_dc_delta_q == 0 &&
                      cm->u_dc_delta_q == 0 && cm->u_ac_delta_q == 0 &&
                      cm->v_dc_delta_q == 0 && cm->v_ac_delta_q == 0;
    if (xd->lossless[i]) cpi->has_lossless_segment = 1;
    xd->qindex[i] = qindex;
    if (xd->lossless[i]) {
      cpi->optimize_seg_arr[i] = 0;
    } else {
      cpi->optimize_seg_arr[i] = cpi->optimize_speed_feature;
    }
  }
  cm->coded_lossless = is_coded_lossless(cm, xd);
  cm->all_lossless = cm->coded_lossless && !av1_superres_scaled(cm);

  cm->tx_mode = select_tx_mode(cpi);

  // Fix delta q resolution for the moment
  cm->delta_q_res = DEFAULT_DELTA_Q_RES;
  // Set delta_q_present_flag before it is used for the first time
  cm->delta_lf_res = DEFAULT_DELTA_LF_RES;
  cm->delta_q_present_flag = cpi->oxcf.deltaq_mode != NO_DELTA_Q;
  cm->delta_lf_present_flag = cpi->oxcf.deltaq_mode == DELTA_Q_LF;
  cm->delta_lf_multi = DEFAULT_DELTA_LF_MULTI;
  // update delta_q_present_flag and delta_lf_present_flag based on base_qindex
  cm->delta_q_present_flag &= cm->base_qindex > 0;
  cm->delta_lf_present_flag &= cm->base_qindex > 0;

  av1_frame_init_quantizer(cpi);

  av1_initialize_rd_consts(cpi);
  av1_initialize_me_consts(cpi, x, cm->base_qindex);
  init_encode_frame_mb_context(cpi);

  if (cm->prev_frame)
    cm->last_frame_seg_map = cm->prev_frame->seg_map;
  else
    cm->last_frame_seg_map = NULL;
  cm->current_frame_seg_map = cm->cur_frame->seg_map;
  if (cm->allow_intrabc || cm->coded_lossless) {
    av1_set_default_ref_deltas(cm->lf.ref_deltas);
    av1_set_default_mode_deltas(cm->lf.mode_deltas);
  } else if (cm->prev_frame) {
    memcpy(cm->lf.ref_deltas, cm->prev_frame->ref_deltas, REF_FRAMES);
    memcpy(cm->lf.mode_deltas, cm->prev_frame->mode_deltas, MAX_MODE_LF_DELTAS);
  }
  memcpy(cm->cur_frame->ref_deltas, cm->lf.ref_deltas, REF_FRAMES);
  memcpy(cm->cur_frame->mode_deltas, cm->lf.mode_deltas, MAX_MODE_LF_DELTAS);

  // Special case: set prev_mi to NULL when the previous mode info
  // context cannot be used.
  cm->prev_mi = cm->allow_ref_frame_mvs ? cm->prev_mip : NULL;

  x->txb_split_count = 0;
  av1_zero(x->blk_skip_drl);

  av1_zero(rdc->global_motion_used);
  av1_zero(cpi->gmparams_cost);
  if (cpi->common.frame_type == INTER_FRAME && cpi->source &&
      !cpi->global_motion_search_done) {
    YV12_BUFFER_CONFIG *ref_buf[REF_FRAMES];
    int frame;
    double params_by_motion[RANSAC_NUM_MOTIONS * (MAX_PARAMDIM - 1)];
    const double *params_this_motion;
    int inliers_by_motion[RANSAC_NUM_MOTIONS];
    WarpedMotionParams tmp_wm_params;
    static const double kIdentityParams[MAX_PARAMDIM - 1] = {
      0.0, 0.0, 1.0, 0.0, 0.0, 1.0, 0.0, 0.0
    };
    int num_refs_using_gm = 0;

    for (frame = ALTREF_FRAME; frame >= LAST_FRAME; --frame) {
      ref_buf[frame] = get_ref_frame_buffer(cpi, frame);
      int pframe;
      cm->global_motion[frame] = default_warp_params;
      const WarpedMotionParams *ref_params =
          cm->prev_frame ? &cm->prev_frame->global_motion[frame]
                         : &default_warp_params;
      // check for duplicate buffer
      for (pframe = ALTREF_FRAME; pframe > frame; --pframe) {
        if (ref_buf[frame] == ref_buf[pframe]) break;
      }
      if (pframe > frame) {
        memcpy(&cm->global_motion[frame], &cm->global_motion[pframe],
               sizeof(WarpedMotionParams));
      } else if (ref_buf[frame] &&
                 ref_buf[frame]->y_crop_width == cpi->source->y_crop_width &&
                 ref_buf[frame]->y_crop_height == cpi->source->y_crop_height &&
                 do_gm_search_logic(&cpi->sf, num_refs_using_gm, frame) &&
                 !(cpi->sf.selective_ref_gm && skip_gm_frame(cm, frame))) {
        TransformationType model;
        const int64_t ref_frame_error =
            av1_frame_error(xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH, xd->bd,
                            ref_buf[frame]->y_buffer, ref_buf[frame]->y_stride,
                            cpi->source->y_buffer, cpi->source->y_width,
                            cpi->source->y_height, cpi->source->y_stride);

        if (ref_frame_error == 0) continue;

        aom_clear_system_state();
        for (model = ROTZOOM; model < GLOBAL_TRANS_TYPES_ENC; ++model) {
          int64_t best_warp_error = INT64_MAX;
          // Initially set all params to identity.
          for (i = 0; i < RANSAC_NUM_MOTIONS; ++i) {
            memcpy(params_by_motion + (MAX_PARAMDIM - 1) * i, kIdentityParams,
                   (MAX_PARAMDIM - 1) * sizeof(*params_by_motion));
          }

          compute_global_motion_feature_based(
              model, cpi->source, ref_buf[frame], cpi->common.bit_depth,
              inliers_by_motion, params_by_motion, RANSAC_NUM_MOTIONS);

          for (i = 0; i < RANSAC_NUM_MOTIONS; ++i) {
            if (inliers_by_motion[i] == 0) continue;

            params_this_motion = params_by_motion + (MAX_PARAMDIM - 1) * i;
            convert_model_to_params(params_this_motion, &tmp_wm_params);

            if (tmp_wm_params.wmtype != IDENTITY) {
              const int64_t warp_error = refine_integerized_param(
                  &tmp_wm_params, tmp_wm_params.wmtype,
                  xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH, xd->bd,
                  ref_buf[frame]->y_buffer, ref_buf[frame]->y_width,
                  ref_buf[frame]->y_height, ref_buf[frame]->y_stride,
                  cpi->source->y_buffer, cpi->source->y_width,
                  cpi->source->y_height, cpi->source->y_stride, 5,
                  best_warp_error);
              if (warp_error < best_warp_error) {
                best_warp_error = warp_error;
                // Save the wm_params modified by refine_integerized_param()
                // rather than motion index to avoid rerunning refine() below.
                memcpy(&(cm->global_motion[frame]), &tmp_wm_params,
                       sizeof(WarpedMotionParams));
              }
            }
          }
          if (cm->global_motion[frame].wmtype <= AFFINE)
            if (!get_shear_params(&cm->global_motion[frame]))
              cm->global_motion[frame] = default_warp_params;

          if (cm->global_motion[frame].wmtype == TRANSLATION) {
            cm->global_motion[frame].wmmat[0] =
                convert_to_trans_prec(cm->allow_high_precision_mv,
                                      cm->global_motion[frame].wmmat[0]) *
                GM_TRANS_ONLY_DECODE_FACTOR;
            cm->global_motion[frame].wmmat[1] =
                convert_to_trans_prec(cm->allow_high_precision_mv,
                                      cm->global_motion[frame].wmmat[1]) *
                GM_TRANS_ONLY_DECODE_FACTOR;
          }

          // If the best error advantage found doesn't meet the threshold for
          // this motion type, revert to IDENTITY.
          if (!is_enough_erroradvantage(
                  (double)best_warp_error / ref_frame_error,
                  gm_get_params_cost(&cm->global_motion[frame], ref_params,
                                     cm->allow_high_precision_mv),
                  cpi->sf.gm_erroradv_type)) {
            cm->global_motion[frame] = default_warp_params;
          }
          if (cm->global_motion[frame].wmtype != IDENTITY) break;
        }
        aom_clear_system_state();
      }
      if (cm->global_motion[frame].wmtype != IDENTITY) num_refs_using_gm++;
      cpi->gmparams_cost[frame] =
          gm_get_params_cost(&cm->global_motion[frame], ref_params,
                             cm->allow_high_precision_mv) +
          cpi->gmtype_cost[cm->global_motion[frame].wmtype] -
          cpi->gmtype_cost[IDENTITY];
    }
    cpi->global_motion_search_done = 1;
  }
  memcpy(cm->cur_frame->global_motion, cm->global_motion,
         REF_FRAMES * sizeof(WarpedMotionParams));

  av1_setup_motion_field(cm);

  cpi->all_one_sided_refs =
      frame_is_intra_only(cm) ? 0 : av1_refs_are_one_sided(cm);

  cm->skip_mode_flag = check_skip_mode_enabled(cpi);

  {
    struct aom_usec_timer emr_timer;
    aom_usec_timer_start(&emr_timer);

#if CONFIG_FP_MB_STATS
    if (cpi->use_fp_mb_stats) {
      input_fpmb_stats(&cpi->twopass.firstpass_mb_stats, cm,
                       &cpi->twopass.this_frame_mb_stats);
    }
#endif

#if CONFIG_COLLECT_INTER_MODE_RD_STATS
    av1_inter_mode_data_init();
#endif

    // If allowed, encoding tiles in parallel with one thread handling one tile.
    // TODO(geza.lore): The multi-threaded encoder is not safe with more than
    // 1 tile rows, as it uses the single above_context et al arrays from
    // cpi->common
    if (AOMMIN(cpi->oxcf.max_threads, cm->tile_cols) > 1 && cm->tile_rows == 1)
      av1_encode_tiles_mt(cpi);
    else
      encode_tiles(cpi);

#if CONFIG_COLLECT_INTER_MODE_RD_STATS
#if INTER_MODE_RD_TEST
    if (cpi->sf.inter_mode_rd_model_estimation) {
      av1_inter_mode_data_show(cm);
    }
#endif
#endif

    aom_usec_timer_mark(&emr_timer);
    cpi->time_encode_sb_row += aom_usec_timer_elapsed(&emr_timer);
  }

  // If intrabc is allowed but never selected, reset the allow_intrabc flag.
  if (cm->allow_intrabc && !cpi->intrabc_used) cm->allow_intrabc = 0;
  if (cm->allow_intrabc) cm->delta_lf_present_flag = 0;
}

void av1_encode_frame(AV1_COMP *cpi) {
  AV1_COMMON *const cm = &cpi->common;
  const int num_planes = av1_num_planes(cm);
  // Indicates whether or not to use a default reduced set for ext-tx
  // rather than the potential full set of 16 transforms
  cm->reduced_tx_set_used = 0;

  if (cm->show_frame == 0) {
    int arf_offset = AOMMIN(
        (MAX_GF_INTERVAL - 1),
        cpi->twopass.gf_group.arf_src_offset[cpi->twopass.gf_group.index]);
    int brf_offset =
        cpi->twopass.gf_group.brf_src_offset[cpi->twopass.gf_group.index];
    arf_offset = AOMMIN((MAX_GF_INTERVAL - 1), arf_offset + brf_offset);
    cm->frame_offset = cm->current_video_frame + arf_offset;
  } else {
    cm->frame_offset = cm->current_video_frame;
  }
  cm->frame_offset %= (1 << (cm->seq_params.order_hint_bits_minus_1 + 1));

  // Make sure segment_id is no larger than last_active_segid.
  if (cm->seg.enabled && cm->seg.update_map) {
    const int mi_rows = cm->mi_rows;
    const int mi_cols = cm->mi_cols;
    const int last_active_segid = cm->seg.last_active_segid;
    uint8_t *map = cpi->segmentation_map;
    for (int mi_row = 0; mi_row < mi_rows; ++mi_row) {
      for (int mi_col = 0; mi_col < mi_cols; ++mi_col) {
        map[mi_col] = AOMMIN(map[mi_col], last_active_segid);
      }
      map += mi_cols;
    }
  }

  av1_setup_frame_buf_refs(cm);
  if (cpi->sf.selective_ref_frame >= 2) enforce_max_ref_frames(cpi);
  av1_setup_frame_sign_bias(cm);

#if CONFIG_MISMATCH_DEBUG
  mismatch_reset_frame(num_planes);
#else
  (void)num_planes;
#endif

  cpi->allow_comp_inter_inter = !frame_is_intra_only(cm);

  if (cpi->sf.frame_parameter_update) {
    int i;
    RD_OPT *const rd_opt = &cpi->rd;
    RD_COUNTS *const rdc = &cpi->td.rd_counts;

    // This code does a single RD pass over the whole frame assuming
    // either compound, single or hybrid prediction as per whatever has
    // worked best for that type of frame in the past.
    // It also predicts whether another coding mode would have worked
    // better than this coding mode. If that is the case, it remembers
    // that for subsequent frames.
    // It does the same analysis for transform size selection also.
    //
    // TODO(zoeliu): To investigate whether a frame_type other than
    // INTRA/ALTREF/GOLDEN/LAST needs to be specified seperately.
    const MV_REFERENCE_FRAME frame_type = get_frame_type(cpi);
    int64_t *const mode_thrs = rd_opt->prediction_type_threshes[frame_type];
    const int is_alt_ref = frame_type == ALTREF_FRAME;

    /* prediction (compound, single or hybrid) mode selection */
    // NOTE: "is_alt_ref" is true only for OVERLAY/INTNL_OVERLAY frames
    if (is_alt_ref || !cpi->allow_comp_inter_inter)
      cm->reference_mode = SINGLE_REFERENCE;
    else
      cm->reference_mode = REFERENCE_MODE_SELECT;

    cm->interp_filter = SWITCHABLE;
    if (cm->large_scale_tile) cm->interp_filter = EIGHTTAP_REGULAR;

    cm->switchable_motion_mode = 1;

    rdc->compound_ref_used_flag = 0;
    rdc->skip_mode_used_flag = 0;

    encode_frame_internal(cpi);

    for (i = 0; i < REFERENCE_MODES; ++i)
      mode_thrs[i] = (mode_thrs[i] + rdc->comp_pred_diff[i] / cm->MBs) / 2;

    if (cm->reference_mode == REFERENCE_MODE_SELECT) {
      // Use a flag that includes 4x4 blocks
      if (rdc->compound_ref_used_flag == 0) {
        cm->reference_mode = SINGLE_REFERENCE;
#if CONFIG_ENTROPY_STATS
        av1_zero(cpi->td.counts->comp_inter);
#endif  // CONFIG_ENTROPY_STATS
      }
    }
    // Re-check on the skip mode status as reference mode may have been changed.
    if (frame_is_intra_only(cm) || cm->reference_mode == SINGLE_REFERENCE) {
      cm->is_skip_mode_allowed = 0;
      cm->skip_mode_flag = 0;
    }
    if (cm->skip_mode_flag && rdc->skip_mode_used_flag == 0)
      cm->skip_mode_flag = 0;

    if (!cm->large_scale_tile) {
      if (cm->tx_mode == TX_MODE_SELECT && cpi->td.mb.txb_split_count == 0)
        cm->tx_mode = TX_MODE_LARGEST;
    }
  } else {
    encode_frame_internal(cpi);
  }
}

static void update_txfm_count(MACROBLOCK *x, MACROBLOCKD *xd,
                              FRAME_COUNTS *counts, TX_SIZE tx_size, int depth,
                              int blk_row, int blk_col,
                              uint8_t allow_update_cdf) {
  MB_MODE_INFO *mbmi = xd->mi[0];
  const BLOCK_SIZE bsize = mbmi->sb_type;
  const int max_blocks_high = max_block_high(xd, bsize, 0);
  const int max_blocks_wide = max_block_wide(xd, bsize, 0);
  int ctx = txfm_partition_context(xd->above_txfm_context + blk_col,
                                   xd->left_txfm_context + blk_row,
                                   mbmi->sb_type, tx_size);
  const int txb_size_index = av1_get_txb_size_index(bsize, blk_row, blk_col);
  const TX_SIZE plane_tx_size = mbmi->inter_tx_size[txb_size_index];

  if (blk_row >= max_blocks_high || blk_col >= max_blocks_wide) return;
  assert(tx_size > TX_4X4);

  if (depth == MAX_VARTX_DEPTH) {
    // Don't add to counts in this case
    mbmi->tx_size = tx_size;
    txfm_partition_update(xd->above_txfm_context + blk_col,
                          xd->left_txfm_context + blk_row, tx_size, tx_size);
    return;
  }

  if (tx_size == plane_tx_size) {
#if CONFIG_ENTROPY_STATS
    ++counts->txfm_partition[ctx][0];
#endif
    if (allow_update_cdf)
      update_cdf(xd->tile_ctx->txfm_partition_cdf[ctx], 0, 2);
    mbmi->tx_size = tx_size;
    txfm_partition_update(xd->above_txfm_context + blk_col,
                          xd->left_txfm_context + blk_row, tx_size, tx_size);
  } else {
    const TX_SIZE sub_txs = sub_tx_size_map[tx_size];
    const int bsw = tx_size_wide_unit[sub_txs];
    const int bsh = tx_size_high_unit[sub_txs];

#if CONFIG_ENTROPY_STATS
    ++counts->txfm_partition[ctx][1];
#endif
    if (allow_update_cdf)
      update_cdf(xd->tile_ctx->txfm_partition_cdf[ctx], 1, 2);
    ++x->txb_split_count;

    if (sub_txs == TX_4X4) {
      mbmi->inter_tx_size[txb_size_index] = TX_4X4;
      mbmi->tx_size = TX_4X4;
      txfm_partition_update(xd->above_txfm_context + blk_col,
                            xd->left_txfm_context + blk_row, TX_4X4, tx_size);
      return;
    }

    for (int row = 0; row < tx_size_high_unit[tx_size]; row += bsh) {
      for (int col = 0; col < tx_size_wide_unit[tx_size]; col += bsw) {
        int offsetr = row;
        int offsetc = col;

        update_txfm_count(x, xd, counts, sub_txs, depth + 1, blk_row + offsetr,
                          blk_col + offsetc, allow_update_cdf);
      }
    }
  }
}

static void tx_partition_count_update(const AV1_COMMON *const cm, MACROBLOCK *x,
                                      BLOCK_SIZE plane_bsize, int mi_row,
                                      int mi_col, FRAME_COUNTS *td_counts,
                                      uint8_t allow_update_cdf) {
  MACROBLOCKD *xd = &x->e_mbd;
  const int mi_width = block_size_wide[plane_bsize] >> tx_size_wide_log2[0];
  const int mi_height = block_size_high[plane_bsize] >> tx_size_high_log2[0];
  const TX_SIZE max_tx_size = get_vartx_max_txsize(xd, plane_bsize, 0);
  const int bh = tx_size_high_unit[max_tx_size];
  const int bw = tx_size_wide_unit[max_tx_size];
  int idx, idy;

  xd->above_txfm_context = cm->above_txfm_context[xd->tile.tile_row] + mi_col;
  xd->left_txfm_context =
      xd->left_txfm_context_buffer + (mi_row & MAX_MIB_MASK);

  for (idy = 0; idy < mi_height; idy += bh)
    for (idx = 0; idx < mi_width; idx += bw)
      update_txfm_count(x, xd, td_counts, max_tx_size, 0, idy, idx,
                        allow_update_cdf);
}

static void set_txfm_context(MACROBLOCKD *xd, TX_SIZE tx_size, int blk_row,
                             int blk_col) {
  MB_MODE_INFO *mbmi = xd->mi[0];
  const BLOCK_SIZE bsize = mbmi->sb_type;
  const int max_blocks_high = max_block_high(xd, bsize, 0);
  const int max_blocks_wide = max_block_wide(xd, bsize, 0);
  const int txb_size_index = av1_get_txb_size_index(bsize, blk_row, blk_col);
  const TX_SIZE plane_tx_size = mbmi->inter_tx_size[txb_size_index];

  if (blk_row >= max_blocks_high || blk_col >= max_blocks_wide) return;

  if (tx_size == plane_tx_size) {
    mbmi->tx_size = tx_size;
    txfm_partition_update(xd->above_txfm_context + blk_col,
                          xd->left_txfm_context + blk_row, tx_size, tx_size);

  } else {
    if (tx_size == TX_8X8) {
      mbmi->inter_tx_size[txb_size_index] = TX_4X4;
      mbmi->tx_size = TX_4X4;
      txfm_partition_update(xd->above_txfm_context + blk_col,
                            xd->left_txfm_context + blk_row, TX_4X4, tx_size);
      return;
    }
    const TX_SIZE sub_txs = sub_tx_size_map[tx_size];
    const int bsw = tx_size_wide_unit[sub_txs];
    const int bsh = tx_size_high_unit[sub_txs];
    for (int row = 0; row < tx_size_high_unit[tx_size]; row += bsh) {
      for (int col = 0; col < tx_size_wide_unit[tx_size]; col += bsw) {
        const int offsetr = blk_row + row;
        const int offsetc = blk_col + col;
        if (offsetr >= max_blocks_high || offsetc >= max_blocks_wide) continue;
        set_txfm_context(xd, sub_txs, offsetr, offsetc);
      }
    }
  }
}

static void tx_partition_set_contexts(const AV1_COMMON *const cm,
                                      MACROBLOCKD *xd, BLOCK_SIZE plane_bsize,
                                      int mi_row, int mi_col) {
  const int mi_width = block_size_wide[plane_bsize] >> tx_size_wide_log2[0];
  const int mi_height = block_size_high[plane_bsize] >> tx_size_high_log2[0];
  const TX_SIZE max_tx_size = get_vartx_max_txsize(xd, plane_bsize, 0);
  const int bh = tx_size_high_unit[max_tx_size];
  const int bw = tx_size_wide_unit[max_tx_size];
  int idx, idy;

  xd->above_txfm_context = cm->above_txfm_context[xd->tile.tile_row] + mi_col;
  xd->left_txfm_context =
      xd->left_txfm_context_buffer + (mi_row & MAX_MIB_MASK);

  for (idy = 0; idy < mi_height; idy += bh)
    for (idx = 0; idx < mi_width; idx += bw)
      set_txfm_context(xd, max_tx_size, idy, idx);
}

static void encode_superblock(const AV1_COMP *const cpi, TileDataEnc *tile_data,
                              ThreadData *td, TOKENEXTRA **t, RUN_TYPE dry_run,
                              int mi_row, int mi_col, BLOCK_SIZE bsize,
                              int *rate) {
  const AV1_COMMON *const cm = &cpi->common;
  const int num_planes = av1_num_planes(cm);
  MACROBLOCK *const x = &td->mb;
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO **mi_4x4 = xd->mi;
  MB_MODE_INFO *mbmi = mi_4x4[0];
  const int seg_skip =
      segfeature_active(&cm->seg, mbmi->segment_id, SEG_LVL_SKIP);
  const int mis = cm->mi_stride;
  const int mi_width = mi_size_wide[bsize];
  const int mi_height = mi_size_high[bsize];
  const int is_inter = is_inter_block(mbmi);

  if (cpi->sf.mode_pruning_based_on_two_pass_partition_search &&
      x->cb_partition_scan) {
    for (int row = mi_row; row < mi_row + mi_width;
         row += FIRST_PARTITION_PASS_SAMPLE_REGION) {
      for (int col = mi_col; col < mi_col + mi_height;
           col += FIRST_PARTITION_PASS_SAMPLE_REGION) {
        const int index = av1_first_partition_pass_stats_index(row, col);
        FIRST_PARTITION_PASS_STATS *const stats =
            &x->first_partition_pass_stats[index];
        // Increase the counter of data samples.
        ++stats->sample_counts;
        // Increase the counter for ref_frame[0] and ref_frame[1].
        if (stats->ref0_counts[mbmi->ref_frame[0]] < 255)
          ++stats->ref0_counts[mbmi->ref_frame[0]];
        if (mbmi->ref_frame[1] >= 0 &&
            stats->ref1_counts[mbmi->ref_frame[0]] < 255)
          ++stats->ref1_counts[mbmi->ref_frame[1]];
      }
    }
  }

  if (!is_inter) {
    xd->cfl.is_chroma_reference = is_chroma_reference(
        mi_row, mi_col, bsize, cm->subsampling_x, cm->subsampling_y);
    xd->cfl.store_y = store_cfl_required(cm, xd);
    mbmi->skip = 1;
    for (int plane = 0; plane < num_planes; ++plane) {
      av1_encode_intra_block_plane(cpi, x, bsize, plane,
                                   cpi->optimize_seg_arr[mbmi->segment_id],
                                   mi_row, mi_col);
    }

    // If there is at least one lossless segment, force the skip for intra
    // block to be 0, in order to avoid the segment_id to be changed by in
    // write_segment_id().
    if (!cpi->common.seg.segid_preskip && cpi->common.seg.update_map &&
        cpi->has_lossless_segment)
      mbmi->skip = 0;

    xd->cfl.store_y = 0;
    if (av1_allow_palette(cm->allow_screen_content_tools, bsize)) {
      for (int plane = 0; plane < AOMMIN(2, num_planes); ++plane) {
        if (mbmi->palette_mode_info.palette_size[plane] > 0) {
          if (!dry_run) {
            av1_tokenize_color_map(x, plane, t, bsize, mbmi->tx_size,
                                   PALETTE_MAP, tile_data->allow_update_cdf,
                                   td->counts);
          } else if (dry_run == DRY_RUN_COSTCOEFFS) {
            rate +=
                av1_cost_color_map(x, plane, bsize, mbmi->tx_size, PALETTE_MAP);
          }
        }
      }
    }

    av1_update_txb_context(cpi, td, dry_run, bsize, rate, mi_row, mi_col,
                           tile_data->allow_update_cdf);
  } else {
    int ref;
    const int is_compound = has_second_ref(mbmi);

    set_ref_ptrs(cm, xd, mbmi->ref_frame[0], mbmi->ref_frame[1]);
    for (ref = 0; ref < 1 + is_compound; ++ref) {
      YV12_BUFFER_CONFIG *cfg = get_ref_frame_buffer(cpi, mbmi->ref_frame[ref]);
      assert(IMPLIES(!is_intrabc_block(mbmi), cfg));
      av1_setup_pre_planes(xd, ref, cfg, mi_row, mi_col,
                           &xd->block_refs[ref]->sf, num_planes);
    }

    av1_build_inter_predictors_sb(cm, xd, mi_row, mi_col, NULL, bsize);
    if (mbmi->motion_mode == OBMC_CAUSAL)
      av1_build_obmc_inter_predictors_sb(cm, xd, mi_row, mi_col);

#if CONFIG_MISMATCH_DEBUG
    if (dry_run == OUTPUT_ENABLED) {
      for (int plane = 0; plane < num_planes; ++plane) {
        const struct macroblockd_plane *pd = &xd->plane[plane];
        int pixel_c, pixel_r;
        mi_to_pixel_loc(&pixel_c, &pixel_r, mi_col, mi_row, 0, 0,
                        pd->subsampling_x, pd->subsampling_y);
        if (!is_chroma_reference(mi_row, mi_col, bsize, pd->subsampling_x,
                                 pd->subsampling_y))
          continue;
        mismatch_record_block_pre(pd->dst.buf, pd->dst.stride, cm->frame_offset,
                                  plane, pixel_c, pixel_r, pd->width,
                                  pd->height,
                                  xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH);
      }
    }
#else
    (void)num_planes;
#endif

    av1_encode_sb(cpi, x, bsize, mi_row, mi_col, dry_run);
    av1_tokenize_sb_vartx(cpi, td, t, dry_run, mi_row, mi_col, bsize, rate,
                          tile_data->allow_update_cdf);
  }

  if (!dry_run) {
    if (av1_allow_intrabc(cm) && is_intrabc_block(mbmi))
      td->intrabc_used_this_tile = 1;
    if (cm->tx_mode == TX_MODE_SELECT && !xd->lossless[mbmi->segment_id] &&
        mbmi->sb_type > BLOCK_4X4 && !(is_inter && (mbmi->skip || seg_skip))) {
      if (is_inter) {
        tx_partition_count_update(cm, x, bsize, mi_row, mi_col, td->counts,
                                  tile_data->allow_update_cdf);
      } else {
        if (mbmi->tx_size != max_txsize_rect_lookup[bsize])
          ++x->txb_split_count;
        if (block_signals_txsize(bsize)) {
          const int tx_size_ctx = get_tx_size_context(xd);
          const int32_t tx_size_cat = bsize_to_tx_size_cat(bsize);
          const int depth = tx_size_to_depth(mbmi->tx_size, bsize);
          const int max_depths = bsize_to_max_depth(bsize);

          if (tile_data->allow_update_cdf)
            update_cdf(xd->tile_ctx->tx_size_cdf[tx_size_cat][tx_size_ctx],
                       depth, max_depths + 1);
#if CONFIG_ENTROPY_STATS
          ++td->counts->intra_tx_size[tx_size_cat][tx_size_ctx][depth];
#endif
        }
      }
      assert(IMPLIES(is_rect_tx(mbmi->tx_size), is_rect_tx_allowed(xd, mbmi)));
    } else {
      int i, j;
      TX_SIZE intra_tx_size;
      // The new intra coding scheme requires no change of transform size
      if (is_inter) {
        if (xd->lossless[mbmi->segment_id]) {
          intra_tx_size = TX_4X4;
        } else {
          intra_tx_size = tx_size_from_tx_mode(bsize, cm->tx_mode);
        }
      } else {
        intra_tx_size = mbmi->tx_size;
      }

      for (j = 0; j < mi_height; j++)
        for (i = 0; i < mi_width; i++)
          if (mi_col + i < cm->mi_cols && mi_row + j < cm->mi_rows)
            mi_4x4[mis * j + i]->tx_size = intra_tx_size;

      if (intra_tx_size != max_txsize_rect_lookup[bsize]) ++x->txb_split_count;
    }
  }

  if (cm->tx_mode == TX_MODE_SELECT && block_signals_txsize(mbmi->sb_type) &&
      is_inter && !(mbmi->skip || seg_skip) &&
      !xd->lossless[mbmi->segment_id]) {
    if (dry_run) tx_partition_set_contexts(cm, xd, bsize, mi_row, mi_col);
  } else {
    TX_SIZE tx_size = mbmi->tx_size;
    // The new intra coding scheme requires no change of transform size
    if (is_inter) {
      if (xd->lossless[mbmi->segment_id]) {
        tx_size = TX_4X4;
      } else {
        tx_size = tx_size_from_tx_mode(bsize, cm->tx_mode);
      }
    } else {
      tx_size = (bsize > BLOCK_4X4) ? tx_size : TX_4X4;
    }
    mbmi->tx_size = tx_size;
    set_txfm_ctxs(tx_size, xd->n8_w, xd->n8_h,
                  (mbmi->skip || seg_skip) && is_inter_block(mbmi), xd);
  }
  CFL_CTX *const cfl = &xd->cfl;
  if (is_inter_block(mbmi) &&
      !is_chroma_reference(mi_row, mi_col, bsize, cfl->subsampling_x,
                           cfl->subsampling_y) &&
      is_cfl_allowed(xd)) {
    cfl_store_block(xd, mbmi->sb_type, mbmi->tx_size);
  }
}
