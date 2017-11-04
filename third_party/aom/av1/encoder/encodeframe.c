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

#include "./av1_rtcd.h"
#include "./aom_dsp_rtcd.h"
#include "./aom_config.h"

#include "aom_dsp/aom_dsp_common.h"
#include "aom_dsp/binary_codes_writer.h"
#include "aom_ports/mem.h"
#include "aom_ports/aom_timer.h"
#include "aom_ports/system_state.h"

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

#include "av1/encoder/aq_complexity.h"
#include "av1/encoder/aq_cyclicrefresh.h"
#include "av1/encoder/aq_variance.h"
#if CONFIG_SUPERTX
#include "av1/encoder/cost.h"
#endif
#if CONFIG_GLOBAL_MOTION || CONFIG_WARPED_MOTION
#include "av1/common/warped_motion.h"
#endif  // CONFIG_GLOBAL_MOTION || CONFIG_WARPED_MOTION
#if CONFIG_GLOBAL_MOTION
#include "av1/encoder/global_motion.h"
#endif  // CONFIG_GLOBAL_MOTION
#include "av1/encoder/encodeframe.h"
#include "av1/encoder/encodemb.h"
#include "av1/encoder/encodemv.h"
#if CONFIG_LV_MAP
#include "av1/encoder/encodetxb.h"
#endif
#include "av1/encoder/ethread.h"
#include "av1/encoder/extend.h"
#include "av1/encoder/rd.h"
#include "av1/encoder/rdopt.h"
#include "av1/encoder/segmentation.h"
#include "av1/encoder/tokenize.h"
#if CONFIG_PVQ
#include "av1/common/pvq.h"
#include "av1/encoder/pvq_encoder.h"
#endif
#if CONFIG_HIGHBITDEPTH
#define IF_HBD(...) __VA_ARGS__
#else
#define IF_HBD(...)
#endif  // CONFIG_HIGHBITDEPTH

static void encode_superblock(const AV1_COMP *const cpi, ThreadData *td,
                              TOKENEXTRA **t, RUN_TYPE dry_run, int mi_row,
                              int mi_col, BLOCK_SIZE bsize, int *rate);

#if CONFIG_SUPERTX
static int check_intra_b(PICK_MODE_CONTEXT *ctx);

static int check_intra_sb(const AV1_COMP *cpi, const TileInfo *const tile,
                          int mi_row, int mi_col, BLOCK_SIZE bsize,
                          PC_TREE *pc_tree);
static void predict_superblock(const AV1_COMP *const cpi, ThreadData *td,
                               int mi_row_ori, int mi_col_ori, int mi_row_pred,
                               int mi_col_pred, int plane,
                               BLOCK_SIZE bsize_pred, int b_sub8x8, int block);
static int check_supertx_sb(BLOCK_SIZE bsize, TX_SIZE supertx_size,
                            PC_TREE *pc_tree);
static void predict_sb_complex(const AV1_COMP *const cpi, ThreadData *td,
                               const TileInfo *const tile, int mi_row,
                               int mi_col, int mi_row_ori, int mi_col_ori,
                               RUN_TYPE dry_run, BLOCK_SIZE bsize,
                               BLOCK_SIZE top_bsize, uint8_t *dst_buf[3],
                               int dst_stride[3], PC_TREE *pc_tree);
static void update_state_sb_supertx(const AV1_COMP *const cpi, ThreadData *td,
                                    const TileInfo *const tile, int mi_row,
                                    int mi_col, BLOCK_SIZE bsize,
                                    RUN_TYPE dry_run, PC_TREE *pc_tree);
static void rd_supertx_sb(const AV1_COMP *const cpi, ThreadData *td,
                          const TileInfo *const tile, int mi_row, int mi_col,
                          BLOCK_SIZE bsize, int *tmp_rate, int64_t *tmp_dist,
                          TX_TYPE *best_tx, PC_TREE *pc_tree);
#endif  // CONFIG_SUPERTX

// This is used as a reference when computing the source variance for the
//  purposes of activity masking.
// Eventually this should be replaced by custom no-reference routines,
//  which will be faster.
static const uint8_t AV1_VAR_OFFS[MAX_SB_SIZE] = {
  128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
  128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
  128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
  128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
  128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
#if CONFIG_EXT_PARTITION
  128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
  128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
  128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
  128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
  128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128
#endif  // CONFIG_EXT_PARTITION
};

#if CONFIG_HIGHBITDEPTH
static const uint16_t AV1_HIGH_VAR_OFFS_8[MAX_SB_SIZE] = {
  128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
  128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
  128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
  128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
  128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
#if CONFIG_EXT_PARTITION
  128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
  128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
  128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
  128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
  128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128
#endif  // CONFIG_EXT_PARTITION
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
#if CONFIG_EXT_PARTITION
  128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4,
  128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4,
  128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4,
  128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4,
  128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4,
  128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4,
  128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4,
  128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4
#endif  // CONFIG_EXT_PARTITION
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
  128 * 16,
#if CONFIG_EXT_PARTITION
  128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16,
  128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16,
  128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16,
  128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16,
  128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16,
  128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16,
  128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16,
  128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16,
  128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16,
  128 * 16
#endif  // CONFIG_EXT_PARTITION
};
#endif  // CONFIG_HIGHBITDEPTH

unsigned int av1_get_sby_perpixel_variance(const AV1_COMP *cpi,
                                           const struct buf_2d *ref,
                                           BLOCK_SIZE bs) {
  unsigned int sse;
  const unsigned int var =
      cpi->fn_ptr[bs].vf(ref->buf, ref->stride, AV1_VAR_OFFS, 0, &sse);
  return ROUND_POWER_OF_TWO(var, num_pels_log2_lookup[bs]);
}

#if CONFIG_HIGHBITDEPTH
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
#endif  // CONFIG_HIGHBITDEPTH

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
  MACROBLOCKD *const xd = &x->e_mbd;
  const int mi_width = mi_size_wide[bsize];
  const int mi_height = mi_size_high[bsize];

  set_mode_info_offsets(cpi, x, xd, mi_row, mi_col);

  set_skip_context(xd, mi_row, mi_col);
#if CONFIG_VAR_TX
  xd->above_txfm_context =
      cm->above_txfm_context + (mi_col << TX_UNIT_WIDE_LOG2);
  xd->left_txfm_context = xd->left_txfm_context_buffer +
                          ((mi_row & MAX_MIB_MASK) << TX_UNIT_HIGH_LOG2);
  xd->max_tx_size = max_txsize_lookup[bsize];
#endif

  // Set up destination pointers.
  av1_setup_dst_planes(xd->plane, bsize, get_frame_new_buffer(cm), mi_row,
                       mi_col);

  // Set up limit values for MV components.
  // Mv beyond the range do not produce new/different prediction block.
  x->mv_limits.row_min =
      -(((mi_row + mi_height) * MI_SIZE) + AOM_INTERP_EXTEND);
  x->mv_limits.col_min = -(((mi_col + mi_width) * MI_SIZE) + AOM_INTERP_EXTEND);
  x->mv_limits.row_max = (cm->mi_rows - mi_row) * MI_SIZE + AOM_INTERP_EXTEND;
  x->mv_limits.col_max = (cm->mi_cols - mi_col) * MI_SIZE + AOM_INTERP_EXTEND;

  set_plane_n4(xd, mi_width, mi_height);

  // Set up distance of MB to edge of frame in 1/8th pel units.
  assert(!(mi_col & (mi_width - 1)) && !(mi_row & (mi_height - 1)));
  set_mi_row_col(xd, tile, mi_row, mi_height, mi_col, mi_width,
#if CONFIG_DEPENDENT_HORZTILES
                 cm->dependent_horz_tiles,
#endif  // CONFIG_DEPENDENT_HORZTILES
                 cm->mi_rows, cm->mi_cols);

  // Set up source buffers.
  av1_setup_src_planes(x, cpi->source, mi_row, mi_col);

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

  mbmi = &xd->mi[0]->mbmi;
#if CONFIG_CFL
  xd->cfl->mi_row = mi_row;
  xd->cfl->mi_col = mi_col;
#endif

  // Setup segment ID.
  if (seg->enabled) {
    if (!cpi->vaq_refresh) {
      const uint8_t *const map =
          seg->update_map ? cpi->segmentation_map : cm->last_frame_seg_map;
      mbmi->segment_id = get_segment_id(cm, map, bsize, mi_row, mi_col);
    }
    av1_init_plane_quantizers(cpi, x, mbmi->segment_id);
  } else {
    mbmi->segment_id = 0;
  }

#if CONFIG_SUPERTX
  mbmi->segment_id_supertx = MAX_SEGMENTS;
#endif  // CONFIG_SUPERTX
}

#if CONFIG_SUPERTX
static void set_offsets_supertx(const AV1_COMP *const cpi, ThreadData *td,
                                const TileInfo *const tile, int mi_row,
                                int mi_col, BLOCK_SIZE bsize) {
  MACROBLOCK *const x = &td->mb;
  const AV1_COMMON *const cm = &cpi->common;
  MACROBLOCKD *const xd = &x->e_mbd;
  const int mi_width = mi_size_wide[bsize];
  const int mi_height = mi_size_high[bsize];
#if CONFIG_DEPENDENT_HORZTILES
  set_mode_info_offsets(cpi, x, xd, mi_row, mi_col, cm->dependent_horz_tiles);
#else
  set_mode_info_offsets(cpi, x, xd, mi_row, mi_col);
#endif

  // Set up distance of MB to edge of frame in 1/8th pel units.
  assert(!(mi_col & (mi_width - 1)) && !(mi_row & (mi_height - 1)));
  set_mi_row_col(xd, tile, mi_row, mi_height, mi_col, mi_width,
#if CONFIG_DEPENDENT_HORZTILES
                 cm->dependent_horz_tiles,
#endif  // CONFIG_DEPENDENT_HORZTILES
                 cm->mi_rows, cm->mi_cols);
}

static void set_offsets_extend(const AV1_COMP *const cpi, ThreadData *td,
                               const TileInfo *const tile, int mi_row_pred,
                               int mi_col_pred, int mi_row_ori, int mi_col_ori,
                               BLOCK_SIZE bsize_pred) {
  // Used in supertx
  // (mi_row_ori, mi_col_ori, bsize_ori): region for mv
  // (mi_row_pred, mi_col_pred, bsize_pred): region to predict
  MACROBLOCK *const x = &td->mb;
  const AV1_COMMON *const cm = &cpi->common;
  MACROBLOCKD *const xd = &x->e_mbd;
  const int mi_width = mi_size_wide[bsize_pred];
  const int mi_height = mi_size_high[bsize_pred];

#if CONFIG_DEPENDENT_HORZTILES
  set_mode_info_offsets(cpi, x, xd, mi_row_ori, mi_col_ori,
                        cm->dependent_horz_tiles);
#else
  set_mode_info_offsets(cpi, x, xd, mi_row_ori, mi_col_ori);
#endif

  // Set up limit values for MV components.
  // Mv beyond the range do not produce new/different prediction block.
  x->mv_limits.row_min =
      -(((mi_row_pred + mi_height) * MI_SIZE) + AOM_INTERP_EXTEND);
  x->mv_limits.col_min =
      -(((mi_col_pred + mi_width) * MI_SIZE) + AOM_INTERP_EXTEND);
  x->mv_limits.row_max =
      (cm->mi_rows - mi_row_pred) * MI_SIZE + AOM_INTERP_EXTEND;
  x->mv_limits.col_max =
      (cm->mi_cols - mi_col_pred) * MI_SIZE + AOM_INTERP_EXTEND;

// Set up distance of MB to edge of frame in 1/8th pel units.
#if !CONFIG_CB4X4
  assert(!(mi_col_pred & (mi_width - mi_size_wide[BLOCK_8X8])) &&
         !(mi_row_pred & (mi_height - mi_size_high[BLOCK_8X8])));
#endif
  set_mi_row_col(xd, tile, mi_row_pred, mi_height, mi_col_pred, mi_width,
#if CONFIG_DEPENDENT_HORZTILES
                 cm->dependent_horz_tiles,
#endif  // CONFIG_DEPENDENT_HORZTILES
                 cm->mi_rows, cm->mi_cols);
  xd->up_available = (mi_row_ori > tile->mi_row_start);
  xd->left_available = (mi_col_ori > tile->mi_col_start);

  // R/D setup.
  x->rdmult = cpi->rd.RDMULT;
}

static void set_segment_id_supertx(const AV1_COMP *const cpi,
                                   MACROBLOCK *const x, const int mi_row,
                                   const int mi_col, const BLOCK_SIZE bsize) {
  const AV1_COMMON *cm = &cpi->common;
  const struct segmentation *seg = &cm->seg;
  const int miw = AOMMIN(mi_size_wide[bsize], cm->mi_cols - mi_col);
  const int mih = AOMMIN(mi_size_high[bsize], cm->mi_rows - mi_row);
  const int mi_offset = mi_row * cm->mi_stride + mi_col;
  MODE_INFO **const mip = cm->mi_grid_visible + mi_offset;
  int r, c;
  int seg_id_supertx = MAX_SEGMENTS;

  if (!seg->enabled) {
    seg_id_supertx = 0;
  } else {
    // Find the minimum segment_id
    for (r = 0; r < mih; r++)
      for (c = 0; c < miw; c++)
        seg_id_supertx =
            AOMMIN(mip[r * cm->mi_stride + c]->mbmi.segment_id, seg_id_supertx);
    assert(0 <= seg_id_supertx && seg_id_supertx < MAX_SEGMENTS);

    // Initialize plane quantisers
    av1_init_plane_quantizers(cpi, x, seg_id_supertx);
  }

  // Assign the the segment_id back to segment_id_supertx
  for (r = 0; r < mih; r++)
    for (c = 0; c < miw; c++)
      mip[r * cm->mi_stride + c]->mbmi.segment_id_supertx = seg_id_supertx;
}
#endif  // CONFIG_SUPERTX

#if CONFIG_DUAL_FILTER
static void reset_intmv_filter_type(const AV1_COMMON *const cm, MACROBLOCKD *xd,
                                    MB_MODE_INFO *mbmi) {
  InterpFilter filters[2];
  InterpFilter default_filter = av1_unswitchable_filter(cm->interp_filter);

  for (int dir = 0; dir < 2; ++dir) {
    filters[dir] = ((!has_subpel_mv_component(xd->mi[0], xd, dir) &&
                     (mbmi->ref_frame[1] == NONE_FRAME ||
                      !has_subpel_mv_component(xd->mi[0], xd, dir + 2)))
                        ? default_filter
                        : av1_extract_interp_filter(mbmi->interp_filters, dir));
  }
  mbmi->interp_filters = av1_make_interp_filters(filters[0], filters[1]);
}

static void update_filter_type_count(FRAME_COUNTS *counts,
                                     const MACROBLOCKD *xd,
                                     const MB_MODE_INFO *mbmi) {
  int dir;
  for (dir = 0; dir < 2; ++dir) {
    if (has_subpel_mv_component(xd->mi[0], xd, dir) ||
        (mbmi->ref_frame[1] > INTRA_FRAME &&
         has_subpel_mv_component(xd->mi[0], xd, dir + 2))) {
      const int ctx = av1_get_pred_context_switchable_interp(xd, dir);
      InterpFilter filter =
          av1_extract_interp_filter(mbmi->interp_filters, dir);
      ++counts->switchable_interp[ctx][filter];
      update_cdf(xd->tile_ctx->switchable_interp_cdf[ctx], filter,
                 SWITCHABLE_FILTERS);
    }
  }
}
#endif
#if CONFIG_GLOBAL_MOTION
static void update_global_motion_used(PREDICTION_MODE mode, BLOCK_SIZE bsize,
                                      const MB_MODE_INFO *mbmi,
                                      RD_COUNTS *rdc) {
  if (mode == ZEROMV || mode == ZERO_ZEROMV) {
    const int num_4x4s =
        num_4x4_blocks_wide_lookup[bsize] * num_4x4_blocks_high_lookup[bsize];
    int ref;
    for (ref = 0; ref < 1 + has_second_ref(mbmi); ++ref) {
      rdc->global_motion_used[mbmi->ref_frame[ref]] += num_4x4s;
    }
  }
}
#endif  // CONFIG_GLOBAL_MOTION

static void reset_tx_size(MACROBLOCKD *xd, MB_MODE_INFO *mbmi,
                          const TX_MODE tx_mode) {
  if (xd->lossless[mbmi->segment_id]) {
    mbmi->tx_size = TX_4X4;
  } else if (tx_mode != TX_MODE_SELECT) {
    mbmi->tx_size =
        tx_size_from_tx_mode(mbmi->sb_type, tx_mode, is_inter_block(mbmi));
  }
}

static void set_ref_and_pred_mvs(MACROBLOCK *const x, int_mv *const mi_pred_mv,
                                 int8_t rf_type) {
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = &xd->mi[0]->mbmi;

  const int bw = xd->n8_w << MI_SIZE_LOG2;
  const int bh = xd->n8_h << MI_SIZE_LOG2;
  int ref_mv_idx = mbmi->ref_mv_idx;
  MB_MODE_INFO_EXT *const mbmi_ext = x->mbmi_ext;
  CANDIDATE_MV *const curr_ref_mv_stack = mbmi_ext->ref_mv_stack[rf_type];

  if (has_second_ref(mbmi)) {
    // Special case: NEAR_NEWMV and NEW_NEARMV modes use 1 + mbmi->ref_mv_idx
    // (like NEARMV) instead
    if (mbmi->mode == NEAR_NEWMV || mbmi->mode == NEW_NEARMV) ref_mv_idx += 1;

    if (compound_ref0_mode(mbmi->mode) == NEWMV) {
      int_mv this_mv = curr_ref_mv_stack[ref_mv_idx].this_mv;
      clamp_mv_ref(&this_mv.as_mv, bw, bh, xd);
      mbmi_ext->ref_mvs[mbmi->ref_frame[0]][0] = this_mv;
      mbmi->pred_mv[0] = this_mv;
      mi_pred_mv[0] = this_mv;
    }
    if (compound_ref1_mode(mbmi->mode) == NEWMV) {
      int_mv this_mv = curr_ref_mv_stack[ref_mv_idx].comp_mv;
      clamp_mv_ref(&this_mv.as_mv, bw, bh, xd);
      mbmi_ext->ref_mvs[mbmi->ref_frame[1]][0] = this_mv;
      mbmi->pred_mv[1] = this_mv;
      mi_pred_mv[1] = this_mv;
    }
#if CONFIG_COMPOUND_SINGLEREF
  } else if (is_inter_singleref_comp_mode(mbmi->mode)) {
    // Special case: SR_NEAR_NEWMV uses 1 + mbmi->ref_mv_idx
    // (like NEARMV) instead
    if (mbmi->mode == SR_NEAR_NEWMV) ref_mv_idx += 1;

    if (compound_ref0_mode(mbmi->mode) == NEWMV ||
        compound_ref1_mode(mbmi->mode) == NEWMV) {
      int_mv this_mv = curr_ref_mv_stack[ref_mv_idx].this_mv;
      clamp_mv_ref(&this_mv.as_mv, bw, bh, xd);
      mbmi_ext->ref_mvs[mbmi->ref_frame[0]][0] = this_mv;
      mbmi->pred_mv[0] = this_mv;
      mi_pred_mv[0] = this_mv;
    }
#endif  // CONFIG_COMPOUND_SINGLEREF
  } else {
    if (mbmi->mode == NEWMV) {
      int i;
      for (i = 0; i < 1 + has_second_ref(mbmi); ++i) {
        int_mv this_mv = (i == 0) ? curr_ref_mv_stack[ref_mv_idx].this_mv
                                  : curr_ref_mv_stack[ref_mv_idx].comp_mv;
        clamp_mv_ref(&this_mv.as_mv, bw, bh, xd);
        mbmi_ext->ref_mvs[mbmi->ref_frame[i]][0] = this_mv;
        mbmi->pred_mv[i] = this_mv;
        mi_pred_mv[i] = this_mv;
      }
    }
  }
}

static void update_state(const AV1_COMP *const cpi, ThreadData *td,
                         PICK_MODE_CONTEXT *ctx, int mi_row, int mi_col,
                         BLOCK_SIZE bsize, RUN_TYPE dry_run) {
  int i, x_idx, y;
  const AV1_COMMON *const cm = &cpi->common;
  RD_COUNTS *const rdc = &td->rd_counts;
  MACROBLOCK *const x = &td->mb;
  MACROBLOCKD *const xd = &x->e_mbd;
  struct macroblock_plane *const p = x->plane;
  struct macroblockd_plane *const pd = xd->plane;
  MODE_INFO *mi = &ctx->mic;
  MB_MODE_INFO *const mbmi = &xd->mi[0]->mbmi;
  MODE_INFO *mi_addr = xd->mi[0];
  const struct segmentation *const seg = &cm->seg;
  const int bw = mi_size_wide[mi->mbmi.sb_type];
  const int bh = mi_size_high[mi->mbmi.sb_type];
  const int mis = cm->mi_stride;
  const int mi_width = mi_size_wide[bsize];
  const int mi_height = mi_size_high[bsize];
  const int unify_bsize = CONFIG_CB4X4;

  int8_t rf_type;

#if !CONFIG_SUPERTX
  assert(mi->mbmi.sb_type == bsize);
#endif

  *mi_addr = *mi;
  *x->mbmi_ext = ctx->mbmi_ext;

#if CONFIG_DUAL_FILTER
  reset_intmv_filter_type(cm, xd, mbmi);
#endif

  rf_type = av1_ref_frame_type(mbmi->ref_frame);
  if (x->mbmi_ext->ref_mv_count[rf_type] > 1 &&
      (mbmi->sb_type >= BLOCK_8X8 || unify_bsize)) {
    set_ref_and_pred_mvs(x, mi->mbmi.pred_mv, rf_type);
  }

  // If segmentation in use
  if (seg->enabled) {
    // For in frame complexity AQ copy the segment id from the segment map.
    if (cpi->oxcf.aq_mode == COMPLEXITY_AQ) {
      const uint8_t *const map =
          seg->update_map ? cpi->segmentation_map : cm->last_frame_seg_map;
      mi_addr->mbmi.segment_id = get_segment_id(cm, map, bsize, mi_row, mi_col);
      reset_tx_size(xd, &mi_addr->mbmi, cm->tx_mode);
    }
    // Else for cyclic refresh mode update the segment map, set the segment id
    // and then update the quantizer.
    if (cpi->oxcf.aq_mode == CYCLIC_REFRESH_AQ) {
      av1_cyclic_refresh_update_segment(cpi, &xd->mi[0]->mbmi, mi_row, mi_col,
                                        bsize, ctx->rate, ctx->dist, x->skip);
      reset_tx_size(xd, &mi_addr->mbmi, cm->tx_mode);
    }
  }

  for (i = 0; i < MAX_MB_PLANE; ++i) {
    p[i].coeff = ctx->coeff[i];
    p[i].qcoeff = ctx->qcoeff[i];
    pd[i].dqcoeff = ctx->dqcoeff[i];
#if CONFIG_PVQ
    pd[i].pvq_ref_coeff = ctx->pvq_ref_coeff[i];
#endif
    p[i].eobs = ctx->eobs[i];
#if CONFIG_LV_MAP
    p[i].txb_entropy_ctx = ctx->txb_entropy_ctx[i];
#endif  // CONFIG_LV_MAP
  }
  for (i = 0; i < 2; ++i) pd[i].color_index_map = ctx->color_index_map[i];
#if CONFIG_MRC_TX
  xd->mrc_mask = ctx->mrc_mask;
#endif  // CONFIG_MRC_TX
  // Restore the coding context of the MB to that that was in place
  // when the mode was picked for it
  for (y = 0; y < mi_height; y++)
    for (x_idx = 0; x_idx < mi_width; x_idx++)
      if ((xd->mb_to_right_edge >> (3 + MI_SIZE_LOG2)) + mi_width > x_idx &&
          (xd->mb_to_bottom_edge >> (3 + MI_SIZE_LOG2)) + mi_height > y) {
        xd->mi[x_idx + y * mis] = mi_addr;
      }

#if !CONFIG_EXT_DELTA_Q
  if (cpi->oxcf.aq_mode > NO_AQ && cpi->oxcf.aq_mode < DELTA_AQ)
    av1_init_plane_quantizers(cpi, x, xd->mi[0]->mbmi.segment_id);
#else
  if (cpi->oxcf.aq_mode)
    av1_init_plane_quantizers(cpi, x, xd->mi[0]->mbmi.segment_id);
#endif

  if (is_inter_block(mbmi) && mbmi->sb_type < BLOCK_8X8 && !unify_bsize) {
    mbmi->mv[0].as_int = mi->bmi[3].as_mv[0].as_int;
    mbmi->mv[1].as_int = mi->bmi[3].as_mv[1].as_int;
  }

  x->skip = ctx->skip;

#if CONFIG_VAR_TX
  for (i = 0; i < 1; ++i)
    memcpy(x->blk_skip[i], ctx->blk_skip[i],
           sizeof(uint8_t) * ctx->num_4x4_blk);
#endif

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
        THR_D117_PRED /*D117_PRED*/,
        THR_D153_PRED /*D153_PRED*/,
        THR_D207_PRED /*D207_PRED*/,
        THR_D63_PRED /*D63_PRED*/,
        THR_SMOOTH, /*SMOOTH_PRED*/
#if CONFIG_SMOOTH_HV
        THR_SMOOTH_V, /*SMOOTH_V_PRED*/
        THR_SMOOTH_H, /*SMOOTH_H_PRED*/
#endif                // CONFIG_SMOOTH_HV
        THR_TM /*TM_PRED*/,
      };
      ++mode_chosen_counts[kf_mode_index[mbmi->mode]];
    } else {
      // Note how often each mode chosen as best
      ++mode_chosen_counts[ctx->best_mode_index];
    }
  }
#endif
  if (!frame_is_intra_only(cm)) {
    if (is_inter_block(mbmi)) {
      av1_update_mv_count(td);
#if CONFIG_GLOBAL_MOTION
      if (bsize >= BLOCK_8X8) {
        // TODO(sarahparker): global motion stats need to be handled per-tile
        // to be compatible with tile-based threading.
        update_global_motion_used(mbmi->mode, bsize, mbmi, rdc);
      } else {
        const int num_4x4_w = num_4x4_blocks_wide_lookup[bsize];
        const int num_4x4_h = num_4x4_blocks_high_lookup[bsize];
        int idx, idy;
        for (idy = 0; idy < 2; idy += num_4x4_h) {
          for (idx = 0; idx < 2; idx += num_4x4_w) {
            const int j = idy * 2 + idx;
            update_global_motion_used(mi->bmi[j].as_mode, bsize, mbmi, rdc);
          }
        }
      }
#endif  // CONFIG_GLOBAL_MOTION
      if (cm->interp_filter == SWITCHABLE
#if CONFIG_WARPED_MOTION
          && mbmi->motion_mode != WARPED_CAUSAL
#endif  // CONFIG_WARPED_MOTION
#if CONFIG_GLOBAL_MOTION
          && !is_nontrans_global_motion(xd)
#endif  // CONFIG_GLOBAL_MOTION
              ) {
#if CONFIG_DUAL_FILTER
        update_filter_type_count(td->counts, xd, mbmi);
#else
        const int switchable_ctx = av1_get_pred_context_switchable_interp(xd);
        const InterpFilter filter =
            av1_extract_interp_filter(mbmi->interp_filters, 0);
        ++td->counts->switchable_interp[switchable_ctx][filter];
#endif
      }
    }

    rdc->comp_pred_diff[SINGLE_REFERENCE] += ctx->single_pred_diff;
    rdc->comp_pred_diff[COMPOUND_REFERENCE] += ctx->comp_pred_diff;
    rdc->comp_pred_diff[REFERENCE_MODE_SELECT] += ctx->hybrid_pred_diff;
  }

  const int x_mis = AOMMIN(bw, cm->mi_cols - mi_col);
  const int y_mis = AOMMIN(bh, cm->mi_rows - mi_row);
  av1_copy_frame_mvs(cm, mi, mi_row, mi_col, x_mis, y_mis);
}

#if CONFIG_SUPERTX
static void update_state_supertx(const AV1_COMP *const cpi, ThreadData *td,
                                 PICK_MODE_CONTEXT *ctx, int mi_row, int mi_col,
                                 BLOCK_SIZE bsize, RUN_TYPE dry_run) {
  int y, x_idx;
#if CONFIG_VAR_TX
  int i;
#endif
  const AV1_COMMON *const cm = &cpi->common;
  RD_COUNTS *const rdc = &td->rd_counts;
  MACROBLOCK *const x = &td->mb;
  MACROBLOCKD *const xd = &x->e_mbd;
  MODE_INFO *mi = &ctx->mic;
  MB_MODE_INFO *const mbmi = &xd->mi[0]->mbmi;
  MODE_INFO *mi_addr = xd->mi[0];
  const struct segmentation *const seg = &cm->seg;
  const int mis = cm->mi_stride;
  const int mi_width = mi_size_wide[bsize];
  const int mi_height = mi_size_high[bsize];
  const int unify_bsize = CONFIG_CB4X4;
  int8_t rf_type;

  *mi_addr = *mi;
  *x->mbmi_ext = ctx->mbmi_ext;
  assert(is_inter_block(mbmi));
  assert(mbmi->tx_size == ctx->mic.mbmi.tx_size);

#if CONFIG_DUAL_FILTER
  reset_intmv_filter_type(cm, xd, mbmi);
#endif

  rf_type = av1_ref_frame_type(mbmi->ref_frame);
  if (x->mbmi_ext->ref_mv_count[rf_type] > 1 &&
      (mbmi->sb_type >= BLOCK_8X8 || unify_bsize)) {
    set_ref_and_pred_mvs(x, mi->mbmi.pred_mv, rf_type);
  }

  // If segmentation in use
  if (seg->enabled) {
    if (cpi->vaq_refresh) {
      const int energy =
          bsize <= BLOCK_16X16 ? x->mb_energy : av1_block_energy(cpi, x, bsize);
      mi_addr->mbmi.segment_id = av1_vaq_segment_id(energy);
    } else if (cpi->oxcf.aq_mode == CYCLIC_REFRESH_AQ) {
      // For cyclic refresh mode, now update the segment map
      // and set the segment id.
      av1_cyclic_refresh_update_segment(cpi, &xd->mi[0]->mbmi, mi_row, mi_col,
                                        bsize, ctx->rate, ctx->dist, 1);
    } else {
      // Otherwise just set the segment id based on the current segment map
      const uint8_t *const map =
          seg->update_map ? cpi->segmentation_map : cm->last_frame_seg_map;
      mi_addr->mbmi.segment_id = get_segment_id(cm, map, bsize, mi_row, mi_col);
    }
    mi_addr->mbmi.segment_id_supertx = MAX_SEGMENTS;
  }
  // Restore the coding context of the MB to that that was in place
  // when the mode was picked for it
  for (y = 0; y < mi_height; y++)
    for (x_idx = 0; x_idx < mi_width; x_idx++)
      if ((xd->mb_to_right_edge >> (3 + MI_SIZE_LOG2)) + mi_width > x_idx &&
          (xd->mb_to_bottom_edge >> (3 + MI_SIZE_LOG2)) + mi_height > y) {
        xd->mi[x_idx + y * mis] = mi_addr;
      }

#if !CONFIG_CB4X4
  if (is_inter_block(mbmi) && mbmi->sb_type < BLOCK_8X8) {
    mbmi->mv[0].as_int = mi->bmi[3].as_mv[0].as_int;
    mbmi->mv[1].as_int = mi->bmi[3].as_mv[1].as_int;
  }
#endif

  x->skip = ctx->skip;

#if CONFIG_VAR_TX
  for (i = 0; i < 1; ++i)
    memcpy(x->blk_skip[i], ctx->blk_skip[i],
           sizeof(uint8_t) * ctx->num_4x4_blk);

  if (!is_inter_block(mbmi) || mbmi->skip)
    mbmi->min_tx_size = get_min_tx_size(mbmi->tx_size);
#endif  // CONFIG_VAR_TX

#if CONFIG_VAR_TX
  {
    const TX_SIZE mtx = mbmi->tx_size;
    const int num_4x4_blocks_wide = tx_size_wide_unit[mtx] >> 1;
    const int num_4x4_blocks_high = tx_size_high_unit[mtx] >> 1;
    int idy, idx;
    mbmi->inter_tx_size[0][0] = mtx;
    for (idy = 0; idy < num_4x4_blocks_high; ++idy)
      for (idx = 0; idx < num_4x4_blocks_wide; ++idx)
        mbmi->inter_tx_size[idy][idx] = mtx;
  }
#endif  // CONFIG_VAR_TX
  // Turn motion variation off for supertx
  mbmi->motion_mode = SIMPLE_TRANSLATION;

  if (dry_run) return;

  if (!frame_is_intra_only(cm)) {
    av1_update_mv_count(td);

#if CONFIG_GLOBAL_MOTION
    if (is_inter_block(mbmi)) {
      if (bsize >= BLOCK_8X8) {
        // TODO(sarahparker): global motion stats need to be handled per-tile
        // to be compatible with tile-based threading.
        update_global_motion_used(mbmi->mode, bsize, mbmi, rdc);
      } else {
        const int num_4x4_w = num_4x4_blocks_wide_lookup[bsize];
        const int num_4x4_h = num_4x4_blocks_high_lookup[bsize];
        int idx, idy;
        for (idy = 0; idy < 2; idy += num_4x4_h) {
          for (idx = 0; idx < 2; idx += num_4x4_w) {
            const int j = idy * 2 + idx;
            update_global_motion_used(mi->bmi[j].as_mode, bsize, mbmi, rdc);
          }
        }
      }
    }
#endif  // CONFIG_GLOBAL_MOTION

    if (cm->interp_filter == SWITCHABLE
#if CONFIG_GLOBAL_MOTION
        && !is_nontrans_global_motion(xd)
#endif  // CONFIG_GLOBAL_MOTION
            ) {
#if CONFIG_DUAL_FILTER
      update_filter_type_count(td->counts, xd, mbmi);
#else
      const int pred_ctx = av1_get_pred_context_switchable_interp(xd);
      ++td->counts->switchable_interp[pred_ctx][mbmi->interp_filter];
#endif
    }

    rdc->comp_pred_diff[SINGLE_REFERENCE] += ctx->single_pred_diff;
    rdc->comp_pred_diff[COMPOUND_REFERENCE] += ctx->comp_pred_diff;
    rdc->comp_pred_diff[REFERENCE_MODE_SELECT] += ctx->hybrid_pred_diff;
  }

  const int x_mis = AOMMIN(mi_width, cm->mi_cols - mi_col);
  const int y_mis = AOMMIN(mi_height, cm->mi_rows - mi_row);
  av1_copy_frame_mvs(cm, mi, mi_row, mi_col, x_mis, y_mis);
}

static void update_state_sb_supertx(const AV1_COMP *const cpi, ThreadData *td,
                                    const TileInfo *const tile, int mi_row,
                                    int mi_col, BLOCK_SIZE bsize,
                                    RUN_TYPE dry_run, PC_TREE *pc_tree) {
  const AV1_COMMON *const cm = &cpi->common;
  MACROBLOCK *const x = &td->mb;
  MACROBLOCKD *const xd = &x->e_mbd;
  struct macroblock_plane *const p = x->plane;
  struct macroblockd_plane *const pd = xd->plane;
  int hbs = mi_size_wide[bsize] / 2;
#if CONFIG_CB4X4
  const int unify_bsize = 1;
#else
  const int unify_bsize = 0;
#endif
  PARTITION_TYPE partition = pc_tree->partitioning;
  BLOCK_SIZE subsize = get_subsize(bsize, partition);
  int i;
#if CONFIG_EXT_PARTITION_TYPES
  BLOCK_SIZE bsize2 = get_subsize(bsize, PARTITION_SPLIT);
#endif
  PICK_MODE_CONTEXT *pmc = NULL;

  if (mi_row >= cm->mi_rows || mi_col >= cm->mi_cols) return;

  if (bsize == BLOCK_16X16 && cpi->vaq_refresh)
    x->mb_energy = av1_block_energy(cpi, x, bsize);

  switch (partition) {
    case PARTITION_NONE:
      set_offsets_supertx(cpi, td, tile, mi_row, mi_col, subsize);
      update_state_supertx(cpi, td, &pc_tree->none, mi_row, mi_col, subsize,
                           dry_run);
      break;
    case PARTITION_VERT:
      set_offsets_supertx(cpi, td, tile, mi_row, mi_col, subsize);
      update_state_supertx(cpi, td, &pc_tree->vertical[0], mi_row, mi_col,
                           subsize, dry_run);
      if (mi_col + hbs < cm->mi_cols && (bsize > BLOCK_8X8 || unify_bsize)) {
        set_offsets_supertx(cpi, td, tile, mi_row, mi_col + hbs, subsize);
        update_state_supertx(cpi, td, &pc_tree->vertical[1], mi_row,
                             mi_col + hbs, subsize, dry_run);
      }
      pmc = &pc_tree->vertical_supertx;
      break;
    case PARTITION_HORZ:
      set_offsets_supertx(cpi, td, tile, mi_row, mi_col, subsize);
      update_state_supertx(cpi, td, &pc_tree->horizontal[0], mi_row, mi_col,
                           subsize, dry_run);
      if (mi_row + hbs < cm->mi_rows && (bsize > BLOCK_8X8 || unify_bsize)) {
        set_offsets_supertx(cpi, td, tile, mi_row + hbs, mi_col, subsize);
        update_state_supertx(cpi, td, &pc_tree->horizontal[1], mi_row + hbs,
                             mi_col, subsize, dry_run);
      }
      pmc = &pc_tree->horizontal_supertx;
      break;
    case PARTITION_SPLIT:
      if (bsize == BLOCK_8X8 && !unify_bsize) {
        set_offsets_supertx(cpi, td, tile, mi_row, mi_col, subsize);
        update_state_supertx(cpi, td, pc_tree->leaf_split[0], mi_row, mi_col,
                             subsize, dry_run);
      } else {
        set_offsets_supertx(cpi, td, tile, mi_row, mi_col, subsize);
        update_state_sb_supertx(cpi, td, tile, mi_row, mi_col, subsize, dry_run,
                                pc_tree->split[0]);
        set_offsets_supertx(cpi, td, tile, mi_row, mi_col + hbs, subsize);
        update_state_sb_supertx(cpi, td, tile, mi_row, mi_col + hbs, subsize,
                                dry_run, pc_tree->split[1]);
        set_offsets_supertx(cpi, td, tile, mi_row + hbs, mi_col, subsize);
        update_state_sb_supertx(cpi, td, tile, mi_row + hbs, mi_col, subsize,
                                dry_run, pc_tree->split[2]);
        set_offsets_supertx(cpi, td, tile, mi_row + hbs, mi_col + hbs, subsize);
        update_state_sb_supertx(cpi, td, tile, mi_row + hbs, mi_col + hbs,
                                subsize, dry_run, pc_tree->split[3]);
      }
      pmc = &pc_tree->split_supertx;
      break;
#if CONFIG_EXT_PARTITION_TYPES
#if CONFIG_EXT_PARTITION_TYPES_AB
#error HORZ/VERT_A/B partitions not yet updated in superres code
#endif
    case PARTITION_HORZ_A:
      set_offsets_supertx(cpi, td, tile, mi_row, mi_col, bsize2);
      update_state_supertx(cpi, td, &pc_tree->horizontala[0], mi_row, mi_col,
                           bsize2, dry_run);
      set_offsets_supertx(cpi, td, tile, mi_row, mi_col + hbs, bsize2);
      update_state_supertx(cpi, td, &pc_tree->horizontala[1], mi_row,
                           mi_col + hbs, bsize2, dry_run);
      set_offsets_supertx(cpi, td, tile, mi_row + hbs, mi_col, subsize);
      update_state_supertx(cpi, td, &pc_tree->horizontala[2], mi_row + hbs,
                           mi_col, subsize, dry_run);
      pmc = &pc_tree->horizontala_supertx;
      break;
    case PARTITION_HORZ_B:
      set_offsets_supertx(cpi, td, tile, mi_row, mi_col, subsize);
      update_state_supertx(cpi, td, &pc_tree->horizontalb[0], mi_row, mi_col,
                           subsize, dry_run);
      set_offsets_supertx(cpi, td, tile, mi_row + hbs, mi_col, bsize2);
      update_state_supertx(cpi, td, &pc_tree->horizontalb[1], mi_row + hbs,
                           mi_col, bsize2, dry_run);
      set_offsets_supertx(cpi, td, tile, mi_row + hbs, mi_col + hbs, bsize2);
      update_state_supertx(cpi, td, &pc_tree->horizontalb[2], mi_row + hbs,
                           mi_col + hbs, bsize2, dry_run);
      pmc = &pc_tree->horizontalb_supertx;
      break;
    case PARTITION_VERT_A:
      set_offsets_supertx(cpi, td, tile, mi_row, mi_col, bsize2);
      update_state_supertx(cpi, td, &pc_tree->verticala[0], mi_row, mi_col,
                           bsize2, dry_run);
      set_offsets_supertx(cpi, td, tile, mi_row + hbs, mi_col, bsize2);
      update_state_supertx(cpi, td, &pc_tree->verticala[1], mi_row + hbs,
                           mi_col, bsize2, dry_run);
      set_offsets_supertx(cpi, td, tile, mi_row, mi_col + hbs, subsize);
      update_state_supertx(cpi, td, &pc_tree->verticala[2], mi_row,
                           mi_col + hbs, subsize, dry_run);
      pmc = &pc_tree->verticala_supertx;
      break;
    case PARTITION_VERT_B:
      set_offsets_supertx(cpi, td, tile, mi_row, mi_col, subsize);
      update_state_supertx(cpi, td, &pc_tree->verticalb[0], mi_row, mi_col,
                           subsize, dry_run);
      set_offsets_supertx(cpi, td, tile, mi_row, mi_col + hbs, bsize2);
      update_state_supertx(cpi, td, &pc_tree->verticalb[1], mi_row,
                           mi_col + hbs, bsize2, dry_run);
      set_offsets_supertx(cpi, td, tile, mi_row + hbs, mi_col + hbs, bsize2);
      update_state_supertx(cpi, td, &pc_tree->verticalb[2], mi_row + hbs,
                           mi_col + hbs, bsize2, dry_run);
      pmc = &pc_tree->verticalb_supertx;
      break;
#endif  // CONFIG_EXT_PARTITION_TYPES
    default: assert(0);
  }

  for (i = 0; i < MAX_MB_PLANE; ++i) {
    if (pmc != NULL) {
      p[i].coeff = pmc->coeff[i];
      p[i].qcoeff = pmc->qcoeff[i];
      pd[i].dqcoeff = pmc->dqcoeff[i];
      p[i].eobs = pmc->eobs[i];
    } else {
      // These should never be used
      p[i].coeff = NULL;
      p[i].qcoeff = NULL;
      pd[i].dqcoeff = NULL;
      p[i].eobs = NULL;
    }
  }
}

static void update_supertx_param(ThreadData *td, PICK_MODE_CONTEXT *ctx,
                                 int best_tx, TX_SIZE supertx_size) {
  MACROBLOCK *const x = &td->mb;
#if CONFIG_VAR_TX
  int i;

  for (i = 0; i < 1; ++i)
    memcpy(ctx->blk_skip[i], x->blk_skip[i],
           sizeof(uint8_t) * ctx->num_4x4_blk);
  ctx->mic.mbmi.min_tx_size = get_min_tx_size(supertx_size);
#endif  // CONFIG_VAR_TX
  ctx->mic.mbmi.tx_size = supertx_size;
  ctx->skip = x->skip;
  ctx->mic.mbmi.tx_type = best_tx;
}

static void update_supertx_param_sb(const AV1_COMP *const cpi, ThreadData *td,
                                    int mi_row, int mi_col, BLOCK_SIZE bsize,
                                    int best_tx, TX_SIZE supertx_size,
                                    PC_TREE *pc_tree) {
  const AV1_COMMON *const cm = &cpi->common;
  const int hbs = mi_size_wide[bsize] / 2;
  PARTITION_TYPE partition = pc_tree->partitioning;
  BLOCK_SIZE subsize = get_subsize(bsize, partition);
#if CONFIG_CB4X4
  const int unify_bsize = 1;
#else
  const int unify_bsize = 0;
#endif
#if CONFIG_EXT_PARTITION_TYPES
  int i;
#endif

  if (mi_row >= cm->mi_rows || mi_col >= cm->mi_cols) return;

  switch (partition) {
    case PARTITION_NONE:
      update_supertx_param(td, &pc_tree->none, best_tx, supertx_size);
      break;
    case PARTITION_VERT:
      update_supertx_param(td, &pc_tree->vertical[0], best_tx, supertx_size);
      if (mi_col + hbs < cm->mi_cols && (bsize > BLOCK_8X8 || unify_bsize))
        update_supertx_param(td, &pc_tree->vertical[1], best_tx, supertx_size);
      break;
    case PARTITION_HORZ:
      update_supertx_param(td, &pc_tree->horizontal[0], best_tx, supertx_size);
      if (mi_row + hbs < cm->mi_rows && (bsize > BLOCK_8X8 || unify_bsize))
        update_supertx_param(td, &pc_tree->horizontal[1], best_tx,
                             supertx_size);
      break;
    case PARTITION_SPLIT:
      if (bsize == BLOCK_8X8 && !unify_bsize) {
        update_supertx_param(td, pc_tree->leaf_split[0], best_tx, supertx_size);
      } else {
        update_supertx_param_sb(cpi, td, mi_row, mi_col, subsize, best_tx,
                                supertx_size, pc_tree->split[0]);
        update_supertx_param_sb(cpi, td, mi_row, mi_col + hbs, subsize, best_tx,
                                supertx_size, pc_tree->split[1]);
        update_supertx_param_sb(cpi, td, mi_row + hbs, mi_col, subsize, best_tx,
                                supertx_size, pc_tree->split[2]);
        update_supertx_param_sb(cpi, td, mi_row + hbs, mi_col + hbs, subsize,
                                best_tx, supertx_size, pc_tree->split[3]);
      }
      break;
#if CONFIG_EXT_PARTITION_TYPES
#if CONFIG_EXT_PARTITION_TYPES_AB
#error HORZ/VERT_A/B partitions not yet updated in superres code
#endif
    case PARTITION_HORZ_A:
      for (i = 0; i < 3; i++)
        update_supertx_param(td, &pc_tree->horizontala[i], best_tx,
                             supertx_size);
      break;
    case PARTITION_HORZ_B:
      for (i = 0; i < 3; i++)
        update_supertx_param(td, &pc_tree->horizontalb[i], best_tx,
                             supertx_size);
      break;
    case PARTITION_VERT_A:
      for (i = 0; i < 3; i++)
        update_supertx_param(td, &pc_tree->verticala[i], best_tx, supertx_size);
      break;
    case PARTITION_VERT_B:
      for (i = 0; i < 3; i++)
        update_supertx_param(td, &pc_tree->verticalb[i], best_tx, supertx_size);
      break;
#endif  // CONFIG_EXT_PARTITION_TYPES
    default: assert(0);
  }
}
#endif  // CONFIG_SUPERTX

#if CONFIG_MOTION_VAR && NC_MODE_INFO
static void set_mode_info_b(const AV1_COMP *const cpi,
                            const TileInfo *const tile, ThreadData *td,
                            int mi_row, int mi_col, BLOCK_SIZE bsize,
                            PICK_MODE_CONTEXT *ctx) {
  MACROBLOCK *const x = &td->mb;
  set_offsets(cpi, tile, x, mi_row, mi_col, bsize);
  update_state(cpi, td, ctx, mi_row, mi_col, bsize, 1);
}

static void set_mode_info_sb(const AV1_COMP *const cpi, ThreadData *td,
                             const TileInfo *const tile, TOKENEXTRA **tp,
                             int mi_row, int mi_col, BLOCK_SIZE bsize,
                             PC_TREE *pc_tree) {
  const AV1_COMMON *const cm = &cpi->common;
  const int hbs = mi_size_wide[bsize] / 2;
  const PARTITION_TYPE partition = pc_tree->partitioning;
  BLOCK_SIZE subsize = get_subsize(bsize, partition);
#if CONFIG_EXT_PARTITION_TYPES
  const BLOCK_SIZE bsize2 = get_subsize(bsize, PARTITION_SPLIT);
  const int quarter_step = mi_size_wide[bsize] / 4;
#endif
#if CONFIG_CB4X4
  const int unify_bsize = 1;
#else
  const int unify_bsize = 0;
  assert(bsize >= BLOCK_8X8);
#endif

  if (mi_row >= cm->mi_rows || mi_col >= cm->mi_cols) return;

  switch (partition) {
    case PARTITION_NONE:
      set_mode_info_b(cpi, tile, td, mi_row, mi_col, subsize, &pc_tree->none);
      break;
    case PARTITION_VERT:
      set_mode_info_b(cpi, tile, td, mi_row, mi_col, subsize,
                      &pc_tree->vertical[0]);
      if (mi_col + hbs < cm->mi_cols && (bsize > BLOCK_8X8 || unify_bsize)) {
        set_mode_info_b(cpi, tile, td, mi_row, mi_col + hbs, subsize,
                        &pc_tree->vertical[1]);
      }
      break;
    case PARTITION_HORZ:
      set_mode_info_b(cpi, tile, td, mi_row, mi_col, subsize,
                      &pc_tree->horizontal[0]);
      if (mi_row + hbs < cm->mi_rows && (bsize > BLOCK_8X8 || unify_bsize)) {
        set_mode_info_b(cpi, tile, td, mi_row + hbs, mi_col, subsize,
                        &pc_tree->horizontal[1]);
      }
      break;
    case PARTITION_SPLIT:
      if (bsize == BLOCK_8X8 && !unify_bsize) {
        set_mode_info_b(cpi, tile, td, mi_row, mi_col, subsize,
                        pc_tree->leaf_split[0]);
      } else {
        set_mode_info_sb(cpi, td, tile, tp, mi_row, mi_col, subsize,
                         pc_tree->split[0]);
        set_mode_info_sb(cpi, td, tile, tp, mi_row, mi_col + hbs, subsize,
                         pc_tree->split[1]);
        set_mode_info_sb(cpi, td, tile, tp, mi_row + hbs, mi_col, subsize,
                         pc_tree->split[2]);
        set_mode_info_sb(cpi, td, tile, tp, mi_row + hbs, mi_col + hbs, subsize,
                         pc_tree->split[3]);
      }
      break;
#if CONFIG_EXT_PARTITION_TYPES
#if CONFIG_EXT_PARTITION_TYPES_AB
#error NC_MODE_INFO+MOTION_VAR not yet supported for new HORZ/VERT_AB partitions
#endif
    case PARTITION_HORZ_A:
      set_mode_info_b(cpi, tile, td, mi_row, mi_col, bsize2,
                      &pc_tree->horizontala[0]);
      set_mode_info_b(cpi, tile, td, mi_row, mi_col + hbs, bsize2,
                      &pc_tree->horizontala[1]);
      set_mode_info_b(cpi, tile, td, mi_row + hbs, mi_col, subsize,
                      &pc_tree->horizontala[2]);
      break;
    case PARTITION_HORZ_B:
      set_mode_info_b(cpi, tile, td, mi_row, mi_col, subsize,
                      &pc_tree->horizontalb[0]);
      set_mode_info_b(cpi, tile, td, mi_row + hbs, mi_col, bsize2,
                      &pc_tree->horizontalb[1]);
      set_mode_info_b(cpi, tile, td, mi_row + hbs, mi_col + hbs, bsize2,
                      &pc_tree->horizontalb[2]);
      break;
    case PARTITION_VERT_A:
      set_mode_info_b(cpi, tile, td, mi_row, mi_col, bsize2,
                      &pc_tree->verticala[0]);
      set_mode_info_b(cpi, tile, td, mi_row + hbs, mi_col, bsize2,
                      &pc_tree->verticala[1]);
      set_mode_info_b(cpi, tile, td, mi_row, mi_col + hbs, subsize,
                      &pc_tree->verticala[2]);
      break;
    case PARTITION_VERT_B:
      set_mode_info_b(cpi, tile, td, mi_row, mi_col, subsize,
                      &pc_tree->verticalb[0]);
      set_mode_info_b(cpi, tile, td, mi_row, mi_col + hbs, bsize2,
                      &pc_tree->verticalb[1]);
      set_mode_info_b(cpi, tile, td, mi_row + hbs, mi_col + hbs, bsize2,
                      &pc_tree->verticalb[2]);
      break;
    case PARTITION_HORZ_4:
      for (int i = 0; i < 4; ++i) {
        int this_mi_row = mi_row + i * quarter_step;
        if (i > 0 && this_mi_row >= cm->mi_rows) break;

        set_mode_info_b(cpi, tile, td, this_mi_row, mi_col, subsize,
                        &pc_tree->horizontal4[i]);
      }
      break;
    case PARTITION_VERT_4:
      for (int i = 0; i < 4; ++i) {
        int this_mi_col = mi_col + i * quarter_step;
        if (i > 0 && this_mi_col >= cm->mi_cols) break;

        set_mode_info_b(cpi, tile, td, mi_row, this_mi_col, subsize,
                        &pc_tree->vertical4[i]);
      }
      break;
#endif  // CONFIG_EXT_PARTITION_TYPES
    default: assert(0 && "Invalid partition type."); break;
  }
}

#if CONFIG_NCOBMC_ADAPT_WEIGHT
static void av1_get_ncobmc_mode_rd(const AV1_COMP *const cpi,
                                   MACROBLOCK *const x, MACROBLOCKD *const xd,
                                   int bsize, const int mi_row,
                                   const int mi_col, NCOBMC_MODE *mode) {
  const AV1_COMMON *const cm = &cpi->common;
  const int mi_width = mi_size_wide[bsize];
  const int mi_height = mi_size_high[bsize];

  assert(bsize >= BLOCK_8X8);

  reset_xd_boundary(xd, mi_row, mi_height, mi_col, mi_width, cm->mi_rows,
                    cm->mi_cols);

  // set up source buffers before calling the mode searching function
  av1_setup_src_planes(x, cpi->source, mi_row, mi_col);

  *mode = get_ncobmc_mode(cpi, x, xd, mi_row, mi_col, bsize);
}
static void get_ncobmc_intrpl_pred(const AV1_COMP *const cpi, ThreadData *td,
                                   int mi_row, int mi_col, BLOCK_SIZE bsize) {
  MACROBLOCK *const x = &td->mb;
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = &xd->mi[0]->mbmi;
  const int mi_width = mi_size_wide[bsize];
  const int mi_height = mi_size_high[bsize];
  const int hbs = AOMMAX(mi_size_wide[bsize] / 2, mi_size_high[bsize] / 2);
  const BLOCK_SIZE sqr_blk = bsize_2_sqr_bsize[bsize];

  if (mi_width > mi_height) {
    // horizontal partition
    av1_get_ncobmc_mode_rd(cpi, x, xd, sqr_blk, mi_row, mi_col,
                           &mbmi->ncobmc_mode[0]);
    xd->mi += hbs;
    av1_get_ncobmc_mode_rd(cpi, x, xd, sqr_blk, mi_row, mi_col + hbs,
                           &mbmi->ncobmc_mode[1]);
  } else if (mi_height > mi_width) {
    // vertical partition
    av1_get_ncobmc_mode_rd(cpi, x, xd, sqr_blk, mi_row, mi_col,
                           &mbmi->ncobmc_mode[0]);
    xd->mi += hbs * xd->mi_stride;
    av1_get_ncobmc_mode_rd(cpi, x, xd, sqr_blk, mi_row + hbs, mi_col,
                           &mbmi->ncobmc_mode[1]);
  } else {
    av1_get_ncobmc_mode_rd(cpi, x, xd, sqr_blk, mi_row, mi_col,
                           &mbmi->ncobmc_mode[0]);
  }
  // restore the info
  av1_setup_src_planes(x, cpi->source, mi_row, mi_col);
  set_mode_info_offsets(cpi, x, xd, mi_row, mi_col);
}
#endif  // CONFIG_NCOBMC_ADAPT_WEIGHT
#endif  // CONFIG_MOTION_VAR && (CONFIG_NCOBMC || CONFIG_NCOBMC_ADAPT_WEIGHT)

void av1_setup_src_planes(MACROBLOCK *x, const YV12_BUFFER_CONFIG *src,
                          int mi_row, int mi_col) {
  uint8_t *const buffers[3] = { src->y_buffer, src->u_buffer, src->v_buffer };
  const int widths[3] = { src->y_crop_width, src->uv_crop_width,
                          src->uv_crop_width };
  const int heights[3] = { src->y_crop_height, src->uv_crop_height,
                           src->uv_crop_height };
  const int strides[3] = { src->y_stride, src->uv_stride, src->uv_stride };
  int i;

  // Set current frame pointer.
  x->e_mbd.cur_buf = src;

  for (i = 0; i < MAX_MB_PLANE; i++)
    setup_pred_plane(&x->plane[i].src, x->e_mbd.mi[0]->mbmi.sb_type, buffers[i],
                     widths[i], heights[i], strides[i], mi_row, mi_col, NULL,
                     x->e_mbd.plane[i].subsampling_x,
                     x->e_mbd.plane[i].subsampling_y);
}

static int set_segment_rdmult(const AV1_COMP *const cpi, MACROBLOCK *const x,
                              int8_t segment_id) {
  int segment_qindex;
  const AV1_COMMON *const cm = &cpi->common;
  av1_init_plane_quantizers(cpi, x, segment_id);
  aom_clear_system_state();
  segment_qindex = av1_get_qindex(&cm->seg, segment_id, cm->base_qindex);
  return av1_compute_rd_mult(cpi, segment_qindex + cm->y_dc_delta_q);
}

#if CONFIG_DIST_8X8 && CONFIG_CB4X4
static void dist_8x8_set_sub8x8_dst(MACROBLOCK *const x, uint8_t *dst8x8,
                                    BLOCK_SIZE bsize, int bw, int bh,
                                    int mi_row, int mi_col) {
  MACROBLOCKD *const xd = &x->e_mbd;
  struct macroblockd_plane *const pd = &xd->plane[0];
  const int dst_stride = pd->dst.stride;
  uint8_t *dst = pd->dst.buf;

  assert(bsize < BLOCK_8X8);

  if (bsize < BLOCK_8X8) {
    int i, j;
#if CONFIG_HIGHBITDEPTH
    if (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH) {
      uint16_t *dst8x8_16 = (uint16_t *)dst8x8;
      uint16_t *dst_sub8x8 = &dst8x8_16[((mi_row & 1) * 8 + (mi_col & 1)) << 2];

      for (j = 0; j < bh; ++j)
        for (i = 0; i < bw; ++i)
          dst_sub8x8[j * 8 + i] = CONVERT_TO_SHORTPTR(dst)[j * dst_stride + i];
    } else {
#endif
      uint8_t *dst_sub8x8 = &dst8x8[((mi_row & 1) * 8 + (mi_col & 1)) << 2];

      for (j = 0; j < bh; ++j)
        for (i = 0; i < bw; ++i)
          dst_sub8x8[j * 8 + i] = dst[j * dst_stride + i];
#if CONFIG_HIGHBITDEPTH
    }
#endif
  }
}
#endif

static void rd_pick_sb_modes(const AV1_COMP *const cpi, TileDataEnc *tile_data,
                             MACROBLOCK *const x, int mi_row, int mi_col,
                             RD_STATS *rd_cost,
#if CONFIG_SUPERTX
                             int *totalrate_nocoef,
#endif
#if CONFIG_EXT_PARTITION_TYPES
                             PARTITION_TYPE partition,
#endif
                             BLOCK_SIZE bsize, PICK_MODE_CONTEXT *ctx,
                             int64_t best_rd) {
  const AV1_COMMON *const cm = &cpi->common;
  TileInfo *const tile_info = &tile_data->tile_info;
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *mbmi;
  struct macroblock_plane *const p = x->plane;
  struct macroblockd_plane *const pd = xd->plane;
  const AQ_MODE aq_mode = cpi->oxcf.aq_mode;
  int i, orig_rdmult;

  aom_clear_system_state();

#if CONFIG_PVQ
  x->pvq_speed = 1;
  x->pvq_coded = 0;
#endif

  set_offsets(cpi, tile_info, x, mi_row, mi_col, bsize);
  mbmi = &xd->mi[0]->mbmi;
  mbmi->sb_type = bsize;
#if CONFIG_RD_DEBUG
  mbmi->mi_row = mi_row;
  mbmi->mi_col = mi_col;
#endif
#if CONFIG_SUPERTX
  // We set tx_size here as skip blocks would otherwise not set it.
  // tx_size needs to be set at this point as supertx_enable in
  // write_modes_sb is computed based on this, and if the garbage in memory
  // just happens to be the supertx_size, then the packer will code this
  // block as a supertx block, even if rdopt did not pick it as such.
  mbmi->tx_size = max_txsize_lookup[bsize];
#endif
#if CONFIG_EXT_PARTITION_TYPES
  mbmi->partition = partition;
#endif

  for (i = 0; i < MAX_MB_PLANE; ++i) {
    p[i].coeff = ctx->coeff[i];
    p[i].qcoeff = ctx->qcoeff[i];
    pd[i].dqcoeff = ctx->dqcoeff[i];
#if CONFIG_PVQ
    pd[i].pvq_ref_coeff = ctx->pvq_ref_coeff[i];
#endif
    p[i].eobs = ctx->eobs[i];
#if CONFIG_LV_MAP
    p[i].txb_entropy_ctx = ctx->txb_entropy_ctx[i];
#endif
  }

  for (i = 0; i < 2; ++i) pd[i].color_index_map = ctx->color_index_map[i];
#if CONFIG_MRC_TX
  xd->mrc_mask = ctx->mrc_mask;
#endif  // CONFIG_MRC_TX

  ctx->skippable = 0;

  // Set to zero to make sure we do not use the previous encoded frame stats
  mbmi->skip = 0;

#if CONFIG_CB4X4
  x->skip_chroma_rd =
      !is_chroma_reference(mi_row, mi_col, bsize, xd->plane[1].subsampling_x,
                           xd->plane[1].subsampling_y);
#endif

#if CONFIG_HIGHBITDEPTH
  if (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH) {
    x->source_variance = av1_high_get_sby_perpixel_variance(
        cpi, &x->plane[0].src, bsize, xd->bd);
  } else {
    x->source_variance =
        av1_get_sby_perpixel_variance(cpi, &x->plane[0].src, bsize);
  }
#else
  x->source_variance =
      av1_get_sby_perpixel_variance(cpi, &x->plane[0].src, bsize);
#endif  // CONFIG_HIGHBITDEPTH

  // Save rdmult before it might be changed, so it can be restored later.
  orig_rdmult = x->rdmult;

  if (aq_mode == VARIANCE_AQ) {
    if (cpi->vaq_refresh) {
      const int energy =
          bsize <= BLOCK_16X16 ? x->mb_energy : av1_block_energy(cpi, x, bsize);
      mbmi->segment_id = av1_vaq_segment_id(energy);
      // Re-initialise quantiser
      av1_init_plane_quantizers(cpi, x, mbmi->segment_id);
    }
    x->rdmult = set_segment_rdmult(cpi, x, mbmi->segment_id);
  } else if (aq_mode == COMPLEXITY_AQ) {
    x->rdmult = set_segment_rdmult(cpi, x, mbmi->segment_id);
  } else if (aq_mode == CYCLIC_REFRESH_AQ) {
    // If segment is boosted, use rdmult for that segment.
    if (cyclic_refresh_segment_id_boosted(mbmi->segment_id))
      x->rdmult = av1_cyclic_refresh_get_rdmult(cpi->cyclic_refresh);
  }

  // Find best coding mode & reconstruct the MB so it is available
  // as a predictor for MBs that follow in the SB
  if (frame_is_intra_only(cm)) {
    av1_rd_pick_intra_mode_sb(cpi, x, rd_cost, bsize, ctx, best_rd);
#if CONFIG_SUPERTX
    *totalrate_nocoef = 0;
#endif  // CONFIG_SUPERTX
  } else {
    if (segfeature_active(&cm->seg, mbmi->segment_id, SEG_LVL_SKIP)) {
      av1_rd_pick_inter_mode_sb_seg_skip(cpi, tile_data, x, mi_row, mi_col,
                                         rd_cost, bsize, ctx, best_rd);
#if CONFIG_SUPERTX
      *totalrate_nocoef = rd_cost->rate;
#endif  // CONFIG_SUPERTX
    } else {
      av1_rd_pick_inter_mode_sb(cpi, tile_data, x, mi_row, mi_col, rd_cost,
#if CONFIG_SUPERTX
                                totalrate_nocoef,
#endif  // CONFIG_SUPERTX
                                bsize, ctx, best_rd);
#if CONFIG_SUPERTX
      assert(*totalrate_nocoef >= 0);
#endif  // CONFIG_SUPERTX
    }
  }

  // Examine the resulting rate and for AQ mode 2 make a segment choice.
  if ((rd_cost->rate != INT_MAX) && (aq_mode == COMPLEXITY_AQ) &&
      (bsize >= BLOCK_16X16) &&
      (cm->frame_type == KEY_FRAME || cpi->refresh_alt_ref_frame ||
#if CONFIG_EXT_REFS
       cpi->refresh_alt2_ref_frame ||
#endif  // CONFIG_EXT_REFS
       (cpi->refresh_golden_frame && !cpi->rc.is_src_frame_alt_ref))) {
    av1_caq_select_segment(cpi, x, bsize, mi_row, mi_col, rd_cost->rate);
  }

  x->rdmult = orig_rdmult;

  // TODO(jingning) The rate-distortion optimization flow needs to be
  // refactored to provide proper exit/return handle.
  if (rd_cost->rate == INT_MAX) rd_cost->rdcost = INT64_MAX;

  ctx->rate = rd_cost->rate;
  ctx->dist = rd_cost->dist;
}

static void update_inter_mode_stats(FRAME_COUNTS *counts, PREDICTION_MODE mode,
                                    int16_t mode_context) {
  int16_t mode_ctx = mode_context & NEWMV_CTX_MASK;
  if (mode == NEWMV) {
    ++counts->newmv_mode[mode_ctx][0];
    return;
  } else {
    ++counts->newmv_mode[mode_ctx][1];

    if (mode_context & (1 << ALL_ZERO_FLAG_OFFSET)) {
      return;
    }

    mode_ctx = (mode_context >> ZEROMV_OFFSET) & ZEROMV_CTX_MASK;
    if (mode == ZEROMV) {
      ++counts->zeromv_mode[mode_ctx][0];
      return;
    } else {
      ++counts->zeromv_mode[mode_ctx][1];
      mode_ctx = (mode_context >> REFMV_OFFSET) & REFMV_CTX_MASK;

      if (mode_context & (1 << SKIP_NEARESTMV_OFFSET)) mode_ctx = 6;
      if (mode_context & (1 << SKIP_NEARMV_OFFSET)) mode_ctx = 7;
      if (mode_context & (1 << SKIP_NEARESTMV_SUB8X8_OFFSET)) mode_ctx = 8;

      ++counts->refmv_mode[mode_ctx][mode != NEARESTMV];
    }
  }
}

static void update_stats(const AV1_COMMON *const cm, ThreadData *td, int mi_row,
                         int mi_col
#if CONFIG_SUPERTX
                         ,
                         int supertx_enabled
#endif
                         ) {
  MACROBLOCK *x = &td->mb;
  MACROBLOCKD *const xd = &x->e_mbd;
  const MODE_INFO *const mi = xd->mi[0];
  const MB_MODE_INFO *const mbmi = &mi->mbmi;
  const MB_MODE_INFO_EXT *const mbmi_ext = x->mbmi_ext;
  const BLOCK_SIZE bsize = mbmi->sb_type;
  FRAME_CONTEXT *fc = xd->tile_ctx;

  // delta quant applies to both intra and inter
  int super_block_upper_left =
      ((mi_row & MAX_MIB_MASK) == 0) && ((mi_col & MAX_MIB_MASK) == 0);

  if (cm->delta_q_present_flag && (bsize != cm->sb_size || !mbmi->skip) &&
      super_block_upper_left) {
    const int dq = (mbmi->current_q_index - xd->prev_qindex) / cm->delta_q_res;
    const int absdq = abs(dq);
    int i;
    for (i = 0; i < AOMMIN(absdq, DELTA_Q_SMALL); ++i) {
      td->counts->delta_q[i][1]++;
    }
    if (absdq < DELTA_Q_SMALL) td->counts->delta_q[absdq][0]++;
    xd->prev_qindex = mbmi->current_q_index;
#if CONFIG_EXT_DELTA_Q
#if CONFIG_LOOPFILTER_LEVEL
    if (cm->delta_lf_present_flag) {
      if (cm->delta_lf_multi) {
        for (int lf_id = 0; lf_id < FRAME_LF_COUNT; ++lf_id) {
          const int delta_lf =
              (mbmi->curr_delta_lf[lf_id] - xd->prev_delta_lf[lf_id]) /
              cm->delta_lf_res;
          const int abs_delta_lf = abs(delta_lf);
          for (i = 0; i < AOMMIN(abs_delta_lf, DELTA_LF_SMALL); ++i) {
            td->counts->delta_lf_multi[lf_id][i][1]++;
          }
          if (abs_delta_lf < DELTA_LF_SMALL)
            td->counts->delta_lf_multi[lf_id][abs_delta_lf][0]++;
          xd->prev_delta_lf[lf_id] = mbmi->curr_delta_lf[lf_id];
        }
      } else {
        const int delta_lf =
            (mbmi->current_delta_lf_from_base - xd->prev_delta_lf_from_base) /
            cm->delta_lf_res;
        const int abs_delta_lf = abs(delta_lf);
        for (i = 0; i < AOMMIN(abs_delta_lf, DELTA_LF_SMALL); ++i) {
          td->counts->delta_lf[i][1]++;
        }
        if (abs_delta_lf < DELTA_LF_SMALL)
          td->counts->delta_lf[abs_delta_lf][0]++;
        xd->prev_delta_lf_from_base = mbmi->current_delta_lf_from_base;
      }
    }
#else
    if (cm->delta_lf_present_flag) {
      const int dlf =
          (mbmi->current_delta_lf_from_base - xd->prev_delta_lf_from_base) /
          cm->delta_lf_res;
      const int absdlf = abs(dlf);
      for (i = 0; i < AOMMIN(absdlf, DELTA_LF_SMALL); ++i) {
        td->counts->delta_lf[i][1]++;
      }
      if (absdlf < DELTA_LF_SMALL) td->counts->delta_lf[absdlf][0]++;
      xd->prev_delta_lf_from_base = mbmi->current_delta_lf_from_base;
    }
#endif  // CONFIG_LOOPFILTER_LEVEL
#endif
  }
  if (!frame_is_intra_only(cm)) {
    FRAME_COUNTS *const counts = td->counts;
    RD_COUNTS *rdc = &td->rd_counts;
    const int inter_block = is_inter_block(mbmi);
    const int seg_ref_active =
        segfeature_active(&cm->seg, mbmi->segment_id, SEG_LVL_REF_FRAME);
    if (!seg_ref_active) {
#if CONFIG_SUPERTX
      if (!supertx_enabled)
#endif
        counts->intra_inter[av1_get_intra_inter_context(xd)][inter_block]++;
#if CONFIG_NEW_MULTISYMBOL
      update_cdf(fc->intra_inter_cdf[av1_get_intra_inter_context(xd)],
                 inter_block, 2);
#endif
      // If the segment reference feature is enabled we have only a single
      // reference frame allowed for the segment so exclude it from
      // the reference frame counts used to work out probabilities.
      if (inter_block) {
        const MV_REFERENCE_FRAME ref0 = mbmi->ref_frame[0];
#if CONFIG_EXT_REFS
        const MV_REFERENCE_FRAME ref1 = mbmi->ref_frame[1];
#endif  // CONFIG_EXT_REFS

        if (cm->reference_mode == REFERENCE_MODE_SELECT) {
          if (has_second_ref(mbmi))
            // This flag is also updated for 4x4 blocks
            rdc->compound_ref_used_flag = 1;
          else
            // This flag is also updated for 4x4 blocks
            rdc->single_ref_used_flag = 1;
          if (is_comp_ref_allowed(mbmi->sb_type)) {
            counts->comp_inter[av1_get_reference_mode_context(cm, xd)]
                              [has_second_ref(mbmi)]++;
#if CONFIG_NEW_MULTISYMBOL
            update_cdf(av1_get_reference_mode_cdf(cm, xd), has_second_ref(mbmi),
                       2);
#endif  // CONFIG_NEW_MULTISYMBOL
          }
        }

        if (has_second_ref(mbmi)) {
#if CONFIG_EXT_COMP_REFS
          const COMP_REFERENCE_TYPE comp_ref_type = has_uni_comp_refs(mbmi)
                                                        ? UNIDIR_COMP_REFERENCE
                                                        : BIDIR_COMP_REFERENCE;
#if !USE_UNI_COMP_REFS
          // TODO(zoeliu): Temporarily turn off uni-directional comp refs
          assert(comp_ref_type == BIDIR_COMP_REFERENCE);
#endif  // !USE_UNI_COMP_REFS
          counts->comp_ref_type[av1_get_comp_reference_type_context(xd)]
                               [comp_ref_type]++;

          if (comp_ref_type == UNIDIR_COMP_REFERENCE) {
            const int bit = (ref0 == BWDREF_FRAME);
            counts->uni_comp_ref[av1_get_pred_context_uni_comp_ref_p(xd)][0]
                                [bit]++;
            if (!bit) {
              const int bit1 = (ref1 == LAST3_FRAME || ref1 == GOLDEN_FRAME);
              counts->uni_comp_ref[av1_get_pred_context_uni_comp_ref_p1(xd)][1]
                                  [bit1]++;
              if (bit1) {
                counts->uni_comp_ref[av1_get_pred_context_uni_comp_ref_p2(xd)]
                                    [2][ref1 == GOLDEN_FRAME]++;
              }
            }
          } else {
#endif  // CONFIG_EXT_COMP_REFS
#if CONFIG_EXT_REFS
            const int bit = (ref0 == GOLDEN_FRAME || ref0 == LAST3_FRAME);

            counts->comp_ref[av1_get_pred_context_comp_ref_p(cm, xd)][0][bit]++;
            if (!bit) {
              counts->comp_ref[av1_get_pred_context_comp_ref_p1(cm, xd)][1]
                              [ref0 == LAST_FRAME]++;
            } else {
              counts->comp_ref[av1_get_pred_context_comp_ref_p2(cm, xd)][2]
                              [ref0 == GOLDEN_FRAME]++;
            }

            counts->comp_bwdref[av1_get_pred_context_comp_bwdref_p(cm, xd)][0]
                               [ref1 == ALTREF_FRAME]++;
            if (ref1 != ALTREF_FRAME)
              counts->comp_bwdref[av1_get_pred_context_comp_bwdref_p1(cm, xd)]
                                 [1][ref1 == ALTREF2_FRAME]++;
#else   // !CONFIG_EXT_REFS
          counts->comp_ref[av1_get_pred_context_comp_ref_p(cm, xd)][0]
                          [ref0 == GOLDEN_FRAME]++;
#endif  // CONFIG_EXT_REFS
#if CONFIG_EXT_COMP_REFS
          }
#endif  // CONFIG_EXT_COMP_REFS
        } else {
#if CONFIG_EXT_REFS
          const int bit = (ref0 >= BWDREF_FRAME);

          counts->single_ref[av1_get_pred_context_single_ref_p1(xd)][0][bit]++;
          if (bit) {
            assert(ref0 <= ALTREF_FRAME);
            counts->single_ref[av1_get_pred_context_single_ref_p2(xd)][1]
                              [ref0 == ALTREF_FRAME]++;
            if (ref0 != ALTREF_FRAME)
              counts->single_ref[av1_get_pred_context_single_ref_p6(xd)][5]
                                [ref0 == ALTREF2_FRAME]++;
          } else {
            const int bit1 = !(ref0 == LAST2_FRAME || ref0 == LAST_FRAME);
            counts
                ->single_ref[av1_get_pred_context_single_ref_p3(xd)][2][bit1]++;
            if (!bit1) {
              counts->single_ref[av1_get_pred_context_single_ref_p4(xd)][3]
                                [ref0 != LAST_FRAME]++;
            } else {
              counts->single_ref[av1_get_pred_context_single_ref_p5(xd)][4]
                                [ref0 != LAST3_FRAME]++;
            }
          }
#else   // !CONFIG_EXT_REFS
          counts->single_ref[av1_get_pred_context_single_ref_p1(xd)][0]
                            [ref0 != LAST_FRAME]++;
          if (ref0 != LAST_FRAME) {
            counts->single_ref[av1_get_pred_context_single_ref_p2(xd)][1]
                              [ref0 != GOLDEN_FRAME]++;
          }
#endif  // CONFIG_EXT_REFS
        }

#if CONFIG_COMPOUND_SINGLEREF
        if (!has_second_ref(mbmi))
          counts->comp_inter_mode[av1_get_inter_mode_context(xd)]
                                 [is_inter_singleref_comp_mode(mbmi->mode)]++;
#endif  // CONFIG_COMPOUND_SINGLEREF

#if CONFIG_INTERINTRA
        if (cm->reference_mode != COMPOUND_REFERENCE &&
#if CONFIG_SUPERTX
            !supertx_enabled &&
#endif
            cm->allow_interintra_compound && is_interintra_allowed(mbmi)) {
          const int bsize_group = size_group_lookup[bsize];
          if (mbmi->ref_frame[1] == INTRA_FRAME) {
            counts->interintra[bsize_group][1]++;
#if CONFIG_NEW_MULTISYMBOL
            update_cdf(fc->interintra_cdf[bsize_group], 1, 2);
#endif
            counts->interintra_mode[bsize_group][mbmi->interintra_mode]++;
            update_cdf(fc->interintra_mode_cdf[bsize_group],
                       mbmi->interintra_mode, INTERINTRA_MODES);
            if (is_interintra_wedge_used(bsize)) {
              counts->wedge_interintra[bsize][mbmi->use_wedge_interintra]++;
#if CONFIG_NEW_MULTISYMBOL
              update_cdf(fc->wedge_interintra_cdf[bsize],
                         mbmi->use_wedge_interintra, 2);
#endif
            }
          } else {
            counts->interintra[bsize_group][0]++;
#if CONFIG_NEW_MULTISYMBOL
            update_cdf(fc->interintra_cdf[bsize_group], 0, 2);
#endif
          }
        }
#endif  // CONFIG_INTERINTRA

#if CONFIG_MOTION_VAR || CONFIG_WARPED_MOTION
#if CONFIG_WARPED_MOTION
        set_ref_ptrs(cm, xd, mbmi->ref_frame[0], mbmi->ref_frame[1]);
#endif
        const MOTION_MODE motion_allowed = motion_mode_allowed(
#if CONFIG_GLOBAL_MOTION
            0, xd->global_motion,
#endif  // CONFIG_GLOBAL_MOTION
#if CONFIG_WARPED_MOTION
            xd,
#endif
            mi);
#if CONFIG_SUPERTX
        if (!supertx_enabled)
#endif  // CONFIG_SUPERTX
          if (mbmi->ref_frame[1] != INTRA_FRAME)
#if CONFIG_MOTION_VAR && CONFIG_WARPED_MOTION
          {
            if (motion_allowed == WARPED_CAUSAL) {
              counts->motion_mode[mbmi->sb_type][mbmi->motion_mode]++;
              update_cdf(fc->motion_mode_cdf[mbmi->sb_type], mbmi->motion_mode,
                         MOTION_MODES);
#if CONFIG_NCOBMC_ADAPT_WEIGHT
            } else if (motion_allowed == NCOBMC_ADAPT_WEIGHT) {
              counts->ncobmc[mbmi->sb_type][mbmi->motion_mode]++;
              update_cdf(fc->ncobmc_cdf[mbmi->sb_type], mbmi->motion_mode,
                         OBMC_FAMILY_MODES);
            } else if (motion_allowed == OBMC_CAUSAL) {
              counts->obmc[mbmi->sb_type][mbmi->motion_mode == OBMC_CAUSAL]++;
              update_cdf(fc->obmc_cdf[mbmi->sb_type], mbmi->motion_mode, 2);
            }
#else
            } else if (motion_allowed == OBMC_CAUSAL) {
              counts->obmc[mbmi->sb_type][mbmi->motion_mode == OBMC_CAUSAL]++;
#if CONFIG_NEW_MULTISYMBOL
              update_cdf(fc->obmc_cdf[mbmi->sb_type],
                         mbmi->motion_mode == OBMC_CAUSAL, 2);
#endif
            }
#endif  // CONFIG_NCOBMC_ADAPT_WEIGHT
          }
#else
          if (motion_allowed > SIMPLE_TRANSLATION) {
            counts->motion_mode[mbmi->sb_type][mbmi->motion_mode]++;
            update_cdf(fc->motion_mode_cdf[mbmi->sb_type], mbmi->motion_mode,
                       MOTION_MODES);
          }
#endif  // CONFIG_MOTION_VAR && CONFIG_WARPED_MOTION

#if CONFIG_NCOBMC_ADAPT_WEIGHT
        if (mbmi->motion_mode == NCOBMC_ADAPT_WEIGHT) {
          ADAPT_OVERLAP_BLOCK ao_block =
              adapt_overlap_block_lookup[mbmi->sb_type];
          ++counts->ncobmc_mode[ao_block][mbmi->ncobmc_mode[0]];
          update_cdf(fc->ncobmc_mode_cdf[ao_block], mbmi->ncobmc_mode[0],
                     MAX_NCOBMC_MODES);
          if (mi_size_wide[mbmi->sb_type] != mi_size_high[mbmi->sb_type]) {
            ++counts->ncobmc_mode[ao_block][mbmi->ncobmc_mode[1]];
            update_cdf(fc->ncobmc_mode_cdf[ao_block], mbmi->ncobmc_mode[1],
                       MAX_NCOBMC_MODES);
          }
        }
#endif

#endif  // CONFIG_MOTION_VAR || CONFIG_WARPED_MOTION

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
#if CONFIG_WEDGE || CONFIG_COMPOUND_SEGMENT
#if CONFIG_WEDGE && CONFIG_COMPOUND_SEGMENT
          if (is_interinter_compound_used(COMPOUND_WEDGE, bsize)) {
#endif
            counts
                ->compound_interinter[bsize][mbmi->interinter_compound_type]++;
            update_cdf(fc->compound_type_cdf[bsize],
                       mbmi->interinter_compound_type, COMPOUND_TYPES);
#if CONFIG_WEDGE && CONFIG_COMPOUND_SEGMENT
          }
#endif
#endif  // CONFIG_WEDGE || CONFIG_COMPOUND_SEGMENT
        }
      }
    }

    if (inter_block &&
        !segfeature_active(&cm->seg, mbmi->segment_id, SEG_LVL_SKIP)) {
      int16_t mode_ctx;
      const PREDICTION_MODE mode = mbmi->mode;
      if (has_second_ref(mbmi)) {
        mode_ctx = mbmi_ext->compound_mode_context[mbmi->ref_frame[0]];
        ++counts->inter_compound_mode[mode_ctx][INTER_COMPOUND_OFFSET(mode)];
        update_cdf(fc->inter_compound_mode_cdf[mode_ctx],
                   INTER_COMPOUND_OFFSET(mode), INTER_COMPOUND_MODES);
#if CONFIG_COMPOUND_SINGLEREF
      } else if (is_inter_singleref_comp_mode(mode)) {
        mode_ctx = mbmi_ext->compound_mode_context[mbmi->ref_frame[0]];
        ++counts->inter_singleref_comp_mode[mode_ctx]
                                           [INTER_SINGLEREF_COMP_OFFSET(mode)];
#endif  // CONFIG_COMPOUND_SINGLEREF
      } else {
        mode_ctx = av1_mode_context_analyzer(mbmi_ext->mode_context,
                                             mbmi->ref_frame, bsize, -1);
        update_inter_mode_stats(counts, mode, mode_ctx);
      }

      int mode_allowed = (mbmi->mode == NEWMV);
      mode_allowed |= (mbmi->mode == NEW_NEWMV);
#if CONFIG_COMPOUND_SINGLEREF
      mode_allowed |= (mbmi->mode == SR_NEW_NEWMV);
#endif  // CONFIG_COMPOUND_SINGLEREF
      if (mode_allowed) {
        uint8_t ref_frame_type = av1_ref_frame_type(mbmi->ref_frame);
        int idx;

        for (idx = 0; idx < 2; ++idx) {
          if (mbmi_ext->ref_mv_count[ref_frame_type] > idx + 1) {
            uint8_t drl_ctx =
                av1_drl_ctx(mbmi_ext->ref_mv_stack[ref_frame_type], idx);
            ++counts->drl_mode[drl_ctx][mbmi->ref_mv_idx != idx];

            if (mbmi->ref_mv_idx == idx) break;
          }
        }
      }

      if (have_nearmv_in_inter_mode(mbmi->mode)) {
        uint8_t ref_frame_type = av1_ref_frame_type(mbmi->ref_frame);
        int idx;

        for (idx = 1; idx < 3; ++idx) {
          if (mbmi_ext->ref_mv_count[ref_frame_type] > idx + 1) {
            uint8_t drl_ctx =
                av1_drl_ctx(mbmi_ext->ref_mv_stack[ref_frame_type], idx);
            ++counts->drl_mode[drl_ctx][mbmi->ref_mv_idx != idx - 1];

            if (mbmi->ref_mv_idx == idx - 1) break;
          }
        }
      }
    }
#if CONFIG_INTRABC
  } else {
    if (av1_allow_intrabc(bsize, cm)) {
      FRAME_COUNTS *const counts = td->counts;
      ++counts->intrabc[mbmi->use_intrabc];
    } else {
      assert(!mbmi->use_intrabc);
    }
#endif
  }
}

typedef struct {
  ENTROPY_CONTEXT a[2 * MAX_MIB_SIZE * MAX_MB_PLANE];
  ENTROPY_CONTEXT l[2 * MAX_MIB_SIZE * MAX_MB_PLANE];
  PARTITION_CONTEXT sa[MAX_MIB_SIZE];
  PARTITION_CONTEXT sl[MAX_MIB_SIZE];
#if CONFIG_VAR_TX
  TXFM_CONTEXT *p_ta;
  TXFM_CONTEXT *p_tl;
  TXFM_CONTEXT ta[2 * MAX_MIB_SIZE];
  TXFM_CONTEXT tl[2 * MAX_MIB_SIZE];
#endif
} RD_SEARCH_MACROBLOCK_CONTEXT;

static void restore_context(MACROBLOCK *x,
                            const RD_SEARCH_MACROBLOCK_CONTEXT *ctx, int mi_row,
                            int mi_col,
#if CONFIG_PVQ
                            od_rollback_buffer *rdo_buf,
#endif
                            BLOCK_SIZE bsize) {
  MACROBLOCKD *xd = &x->e_mbd;
  int p;
  const int num_4x4_blocks_wide =
      block_size_wide[bsize] >> tx_size_wide_log2[0];
  const int num_4x4_blocks_high =
      block_size_high[bsize] >> tx_size_high_log2[0];
  int mi_width = mi_size_wide[bsize];
  int mi_height = mi_size_high[bsize];
  for (p = 0; p < MAX_MB_PLANE; p++) {
    int tx_col;
    int tx_row;
    tx_col = mi_col << (MI_SIZE_LOG2 - tx_size_wide_log2[0]);
    tx_row = (mi_row & MAX_MIB_MASK) << (MI_SIZE_LOG2 - tx_size_high_log2[0]);
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
#if CONFIG_VAR_TX
  xd->above_txfm_context = ctx->p_ta;
  xd->left_txfm_context = ctx->p_tl;
  memcpy(xd->above_txfm_context, ctx->ta,
         sizeof(*xd->above_txfm_context) * (mi_width << TX_UNIT_WIDE_LOG2));
  memcpy(xd->left_txfm_context, ctx->tl,
         sizeof(*xd->left_txfm_context) * (mi_height << TX_UNIT_HIGH_LOG2));
#endif
#if CONFIG_PVQ
  od_encode_rollback(&x->daala_enc, rdo_buf);
#endif
}

static void save_context(const MACROBLOCK *x, RD_SEARCH_MACROBLOCK_CONTEXT *ctx,
                         int mi_row, int mi_col,
#if CONFIG_PVQ
                         od_rollback_buffer *rdo_buf,
#endif
                         BLOCK_SIZE bsize) {
  const MACROBLOCKD *xd = &x->e_mbd;
  int p;
  const int num_4x4_blocks_wide =
      block_size_wide[bsize] >> tx_size_wide_log2[0];
  const int num_4x4_blocks_high =
      block_size_high[bsize] >> tx_size_high_log2[0];
  int mi_width = mi_size_wide[bsize];
  int mi_height = mi_size_high[bsize];

  // buffer the above/left context information of the block in search.
  for (p = 0; p < MAX_MB_PLANE; ++p) {
    int tx_col;
    int tx_row;
    tx_col = mi_col << (MI_SIZE_LOG2 - tx_size_wide_log2[0]);
    tx_row = (mi_row & MAX_MIB_MASK) << (MI_SIZE_LOG2 - tx_size_high_log2[0]);
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
#if CONFIG_VAR_TX
  memcpy(ctx->ta, xd->above_txfm_context,
         sizeof(*xd->above_txfm_context) * (mi_width << TX_UNIT_WIDE_LOG2));
  memcpy(ctx->tl, xd->left_txfm_context,
         sizeof(*xd->left_txfm_context) * (mi_height << TX_UNIT_HIGH_LOG2));
  ctx->p_ta = xd->above_txfm_context;
  ctx->p_tl = xd->left_txfm_context;
#endif
#if CONFIG_PVQ
  od_encode_checkpoint(&x->daala_enc, rdo_buf);
#endif
}

static void encode_b(const AV1_COMP *const cpi, const TileInfo *const tile,
                     ThreadData *td, TOKENEXTRA **tp, int mi_row, int mi_col,
                     RUN_TYPE dry_run, BLOCK_SIZE bsize,
#if CONFIG_EXT_PARTITION_TYPES
                     PARTITION_TYPE partition,
#endif
                     PICK_MODE_CONTEXT *ctx, int *rate) {
  MACROBLOCK *const x = &td->mb;
#if (CONFIG_MOTION_VAR && CONFIG_NCOBMC) | CONFIG_EXT_DELTA_Q | \
    CONFIG_NCOBMC_ADAPT_WEIGHT
  MACROBLOCKD *xd = &x->e_mbd;
  MB_MODE_INFO *mbmi;
#if CONFIG_MOTION_VAR && CONFIG_NCOBMC
  int check_ncobmc;
#endif
#endif

  set_offsets(cpi, tile, x, mi_row, mi_col, bsize);
#if CONFIG_EXT_PARTITION_TYPES
  x->e_mbd.mi[0]->mbmi.partition = partition;
#endif
  update_state(cpi, td, ctx, mi_row, mi_col, bsize, dry_run);
#if CONFIG_MOTION_VAR && (CONFIG_NCOBMC || CONFIG_NCOBMC_ADAPT_WEIGHT)
  mbmi = &xd->mi[0]->mbmi;
#if CONFIG_WARPED_MOTION
  set_ref_ptrs(&cpi->common, xd, mbmi->ref_frame[0], mbmi->ref_frame[1]);
#endif
#endif

#if CONFIG_MOTION_VAR && (CONFIG_NCOBMC || CONFIG_NCOBMC_ADAPT_WEIGHT)
  const MOTION_MODE motion_allowed = motion_mode_allowed(
#if CONFIG_GLOBAL_MOTION
      0, xd->global_motion,
#endif  // CONFIG_GLOBAL_MOTION
#if CONFIG_WARPED_MOTION
      xd,
#endif
      xd->mi[0]);
#endif  // CONFIG_MOTION_VAR && (CONFIG_NCOBMC || CONFIG_NCOBMC_ADAPT_WEIGHT)

#if CONFIG_MOTION_VAR && CONFIG_NCOBMC
  check_ncobmc = is_inter_block(mbmi) && motion_allowed >= OBMC_CAUSAL;
  if (!dry_run && check_ncobmc) {
    av1_check_ncobmc_rd(cpi, x, mi_row, mi_col);
    av1_setup_dst_planes(x->e_mbd.plane, bsize,
                         get_frame_new_buffer(&cpi->common), mi_row, mi_col);
  }
#endif

#if CONFIG_LV_MAP
  av1_set_coeff_buffer(cpi, x, mi_row, mi_col);
#endif

#if CONFIG_NCOBMC_ADAPT_WEIGHT
  if (dry_run == OUTPUT_ENABLED && !frame_is_intra_only(&cpi->common)) {
    if (motion_allowed >= NCOBMC_ADAPT_WEIGHT && is_inter_block(mbmi)) {
      get_ncobmc_intrpl_pred(cpi, td, mi_row, mi_col, bsize);
      av1_check_ncobmc_adapt_weight_rd(cpi, x, mi_row, mi_col);
    }
    av1_setup_dst_planes(x->e_mbd.plane, bsize,
                         get_frame_new_buffer(&cpi->common), mi_row, mi_col);
  }
#endif  // CONFIG_NCOBMC_ADAPT_WEIGHT

  encode_superblock(cpi, td, tp, dry_run, mi_row, mi_col, bsize, rate);

#if CONFIG_LV_MAP
  if (dry_run == 0)
    x->cb_offset += block_size_wide[bsize] * block_size_high[bsize];
#endif

  if (!dry_run) {
#if CONFIG_EXT_DELTA_Q
    mbmi = &xd->mi[0]->mbmi;
    if (bsize == cpi->common.sb_size && mbmi->skip == 1 &&
        cpi->common.delta_lf_present_flag) {
#if CONFIG_LOOPFILTER_LEVEL
      for (int lf_id = 0; lf_id < FRAME_LF_COUNT; ++lf_id)
        mbmi->curr_delta_lf[lf_id] = xd->prev_delta_lf[lf_id];
#endif  // CONFIG_LOOPFILTER_LEVEL
      mbmi->current_delta_lf_from_base = xd->prev_delta_lf_from_base;
    }
#endif
#if CONFIG_SUPERTX
    update_stats(&cpi->common, td, mi_row, mi_col, 0);
#else
    update_stats(&cpi->common, td, mi_row, mi_col);
#endif
  }
}

static void encode_sb(const AV1_COMP *const cpi, ThreadData *td,
                      const TileInfo *const tile, TOKENEXTRA **tp, int mi_row,
                      int mi_col, RUN_TYPE dry_run, BLOCK_SIZE bsize,
                      PC_TREE *pc_tree, int *rate) {
  const AV1_COMMON *const cm = &cpi->common;
  MACROBLOCK *const x = &td->mb;
  MACROBLOCKD *const xd = &x->e_mbd;
  const int hbs = mi_size_wide[bsize] / 2;
#if CONFIG_EXT_PARTITION_TYPES && CONFIG_EXT_PARTITION_TYPES_AB
  const int qbs = mi_size_wide[bsize] / 4;
#endif
  const int is_partition_root = bsize >= BLOCK_8X8;
  const int ctx = is_partition_root
                      ? partition_plane_context(xd, mi_row, mi_col,
#if CONFIG_UNPOISON_PARTITION_CTX
                                                mi_row + hbs < cm->mi_rows,
                                                mi_col + hbs < cm->mi_cols,
#endif
                                                bsize)
                      : -1;
  const PARTITION_TYPE partition = pc_tree->partitioning;
  const BLOCK_SIZE subsize = get_subsize(bsize, partition);
#if CONFIG_EXT_PARTITION_TYPES
  int quarter_step = mi_size_wide[bsize] / 4;
  int i;
#if !CONFIG_EXT_PARTITION_TYPES_AB
  BLOCK_SIZE bsize2 = get_subsize(bsize, PARTITION_SPLIT);
#endif
#endif

#if CONFIG_CB4X4
  const int unify_bsize = 1;
#else
  const int unify_bsize = 0;
  assert(bsize >= BLOCK_8X8);
#endif

  if (mi_row >= cm->mi_rows || mi_col >= cm->mi_cols) return;

  if (!dry_run && ctx >= 0) td->counts->partition[ctx][partition]++;

#if CONFIG_SUPERTX
  if (!frame_is_intra_only(cm) && bsize <= MAX_SUPERTX_BLOCK_SIZE &&
      partition != PARTITION_NONE && !xd->lossless[0]) {
    int supertx_enabled;
    TX_SIZE supertx_size = max_txsize_lookup[bsize];
    supertx_enabled = check_supertx_sb(bsize, supertx_size, pc_tree);
    if (supertx_enabled) {
      const int mi_width = mi_size_wide[bsize];
      const int mi_height = mi_size_high[bsize];
      int x_idx, y_idx, i;
      uint8_t *dst_buf[3];
      int dst_stride[3];
      set_skip_context(xd, mi_row, mi_col);
      set_mode_info_offsets(cpi, x, xd, mi_row, mi_col);
      update_state_sb_supertx(cpi, td, tile, mi_row, mi_col, bsize, dry_run,
                              pc_tree);

      av1_setup_dst_planes(xd->plane, bsize, get_frame_new_buffer(cm), mi_row,
                           mi_col);
      for (i = 0; i < MAX_MB_PLANE; i++) {
        dst_buf[i] = xd->plane[i].dst.buf;
        dst_stride[i] = xd->plane[i].dst.stride;
      }
      predict_sb_complex(cpi, td, tile, mi_row, mi_col, mi_row, mi_col, dry_run,
                         bsize, bsize, dst_buf, dst_stride, pc_tree);

      set_offsets_without_segment_id(cpi, tile, x, mi_row, mi_col, bsize);
      set_segment_id_supertx(cpi, x, mi_row, mi_col, bsize);

      if (!x->skip) {
        int this_rate = 0;
        av1_encode_sb_supertx((AV1_COMMON *)cm, x, bsize);
        av1_tokenize_sb_supertx(cpi, td, tp, dry_run, mi_row, mi_col, bsize,
                                rate);
        if (rate) *rate += this_rate;
      } else {
        xd->mi[0]->mbmi.skip = 1;
        if (!dry_run) td->counts->skip[av1_get_skip_context(xd)][1]++;
        av1_reset_skip_context(xd, mi_row, mi_col, bsize);
      }
      if (!dry_run) {
        for (y_idx = 0; y_idx < mi_height; y_idx++)
          for (x_idx = 0; x_idx < mi_width; x_idx++) {
            if ((xd->mb_to_right_edge >> (3 + MI_SIZE_LOG2)) + mi_width >
                    x_idx &&
                (xd->mb_to_bottom_edge >> (3 + MI_SIZE_LOG2)) + mi_height >
                    y_idx) {
              xd->mi[x_idx + y_idx * cm->mi_stride]->mbmi.skip =
                  xd->mi[0]->mbmi.skip;
            }
          }
        td->counts->supertx[partition_supertx_context_lookup[partition]]
                           [supertx_size][1]++;
        td->counts->supertx_size[supertx_size]++;
#if CONFIG_ENTROPY_STATS
#if CONFIG_EXT_TX
        if (get_ext_tx_types(supertx_size, bsize, 1, cm->reduced_tx_set_used) >
                1 &&
            !xd->mi[0]->mbmi.skip) {
          const int eset =
              get_ext_tx_set(supertx_size, bsize, 1, cm->reduced_tx_set_used);
          if (eset > 0) {
            ++td->counts
                  ->inter_ext_tx[eset][supertx_size][xd->mi[0]->mbmi.tx_type];
          }
        }
#else
        if (supertx_size < TX_32X32 && !xd->mi[0]->mbmi.skip) {
          ++td->counts->inter_ext_tx[supertx_size][xd->mi[0]->mbmi.tx_type];
        }
#endif  // CONFIG_EXT_TX
#endif  // CONFIG_ENTROPY_STATS
      }
#if CONFIG_EXT_PARTITION_TYPES
      update_ext_partition_context(xd, mi_row, mi_col, subsize, bsize,
                                   partition);
#else
      if (partition != PARTITION_SPLIT || bsize == BLOCK_8X8)
        update_partition_context(xd, mi_row, mi_col, subsize, bsize);
#endif
#if CONFIG_VAR_TX
      set_txfm_ctxs(supertx_size, mi_width, mi_height, xd->mi[0]->mbmi.skip,
                    xd);
#endif  // CONFIG_VAR_TX
      return;
    } else {
      if (!dry_run) {
        td->counts->supertx[partition_supertx_context_lookup[partition]]
                           [supertx_size][0]++;
      }
    }
  }
#endif  // CONFIG_SUPERTX

  switch (partition) {
    case PARTITION_NONE:
      encode_b(cpi, tile, td, tp, mi_row, mi_col, dry_run, subsize,
#if CONFIG_EXT_PARTITION_TYPES
               partition,
#endif
               &pc_tree->none, rate);
      break;
    case PARTITION_VERT:
      encode_b(cpi, tile, td, tp, mi_row, mi_col, dry_run, subsize,
#if CONFIG_EXT_PARTITION_TYPES
               partition,
#endif
               &pc_tree->vertical[0], rate);
      if (mi_col + hbs < cm->mi_cols && (bsize > BLOCK_8X8 || unify_bsize)) {
        encode_b(cpi, tile, td, tp, mi_row, mi_col + hbs, dry_run, subsize,
#if CONFIG_EXT_PARTITION_TYPES
                 partition,
#endif
                 &pc_tree->vertical[1], rate);
      }
      break;
    case PARTITION_HORZ:
      encode_b(cpi, tile, td, tp, mi_row, mi_col, dry_run, subsize,
#if CONFIG_EXT_PARTITION_TYPES
               partition,
#endif
               &pc_tree->horizontal[0], rate);
      if (mi_row + hbs < cm->mi_rows && (bsize > BLOCK_8X8 || unify_bsize)) {
        encode_b(cpi, tile, td, tp, mi_row + hbs, mi_col, dry_run, subsize,
#if CONFIG_EXT_PARTITION_TYPES
                 partition,
#endif
                 &pc_tree->horizontal[1], rate);
      }
      break;
    case PARTITION_SPLIT:
      if (bsize == BLOCK_8X8 && !unify_bsize) {
        encode_b(cpi, tile, td, tp, mi_row, mi_col, dry_run, subsize,
#if CONFIG_EXT_PARTITION_TYPES
                 partition,
#endif
                 pc_tree->leaf_split[0], rate);
      } else {
        encode_sb(cpi, td, tile, tp, mi_row, mi_col, dry_run, subsize,
                  pc_tree->split[0], rate);
        encode_sb(cpi, td, tile, tp, mi_row, mi_col + hbs, dry_run, subsize,
                  pc_tree->split[1], rate);
        encode_sb(cpi, td, tile, tp, mi_row + hbs, mi_col, dry_run, subsize,
                  pc_tree->split[2], rate);
        encode_sb(cpi, td, tile, tp, mi_row + hbs, mi_col + hbs, dry_run,
                  subsize, pc_tree->split[3], rate);
      }
      break;

#if CONFIG_EXT_PARTITION_TYPES
#if CONFIG_EXT_PARTITION_TYPES_AB
    case PARTITION_HORZ_A:
      encode_b(cpi, tile, td, tp, mi_row, mi_col, dry_run,
               get_subsize(bsize, PARTITION_HORZ_4), partition,
               &pc_tree->horizontala[0], rate);
      encode_b(cpi, tile, td, tp, mi_row + qbs, mi_col, dry_run,
               get_subsize(bsize, PARTITION_HORZ_4), partition,
               &pc_tree->horizontala[1], rate);
      encode_b(cpi, tile, td, tp, mi_row + hbs, mi_col, dry_run, subsize,
               partition, &pc_tree->horizontala[2], rate);
      break;
    case PARTITION_HORZ_B:
      encode_b(cpi, tile, td, tp, mi_row, mi_col, dry_run, subsize, partition,
               &pc_tree->horizontalb[0], rate);
      encode_b(cpi, tile, td, tp, mi_row + hbs, mi_col, dry_run,
               get_subsize(bsize, PARTITION_HORZ_4), partition,
               &pc_tree->horizontalb[1], rate);
      if (mi_row + 3 * qbs < cm->mi_rows)
        encode_b(cpi, tile, td, tp, mi_row + 3 * qbs, mi_col, dry_run,
                 get_subsize(bsize, PARTITION_HORZ_4), partition,
                 &pc_tree->horizontalb[2], rate);
      break;
    case PARTITION_VERT_A:
      encode_b(cpi, tile, td, tp, mi_row, mi_col, dry_run,
               get_subsize(bsize, PARTITION_VERT_4), partition,
               &pc_tree->verticala[0], rate);
      encode_b(cpi, tile, td, tp, mi_row, mi_col + qbs, dry_run,
               get_subsize(bsize, PARTITION_VERT_4), partition,
               &pc_tree->verticala[1], rate);
      encode_b(cpi, tile, td, tp, mi_row, mi_col + hbs, dry_run, subsize,
               partition, &pc_tree->verticala[2], rate);

      break;
    case PARTITION_VERT_B:
      encode_b(cpi, tile, td, tp, mi_row, mi_col, dry_run, subsize, partition,
               &pc_tree->verticalb[0], rate);
      encode_b(cpi, tile, td, tp, mi_row, mi_col + hbs, dry_run,
               get_subsize(bsize, PARTITION_VERT_4), partition,
               &pc_tree->verticalb[1], rate);
      if (mi_col + 3 * qbs < cm->mi_cols)
        encode_b(cpi, tile, td, tp, mi_row, mi_col + 3 * qbs, dry_run,
                 get_subsize(bsize, PARTITION_VERT_4), partition,
                 &pc_tree->verticalb[2], rate);
      break;
#else
    case PARTITION_HORZ_A:
      encode_b(cpi, tile, td, tp, mi_row, mi_col, dry_run, bsize2, partition,
               &pc_tree->horizontala[0], rate);
      encode_b(cpi, tile, td, tp, mi_row, mi_col + hbs, dry_run, bsize2,
               partition, &pc_tree->horizontala[1], rate);
      encode_b(cpi, tile, td, tp, mi_row + hbs, mi_col, dry_run, subsize,
               partition, &pc_tree->horizontala[2], rate);
      break;
    case PARTITION_HORZ_B:
      encode_b(cpi, tile, td, tp, mi_row, mi_col, dry_run, subsize, partition,
               &pc_tree->horizontalb[0], rate);
      encode_b(cpi, tile, td, tp, mi_row + hbs, mi_col, dry_run, bsize2,
               partition, &pc_tree->horizontalb[1], rate);
      encode_b(cpi, tile, td, tp, mi_row + hbs, mi_col + hbs, dry_run, bsize2,
               partition, &pc_tree->horizontalb[2], rate);
      break;
    case PARTITION_VERT_A:
      encode_b(cpi, tile, td, tp, mi_row, mi_col, dry_run, bsize2, partition,
               &pc_tree->verticala[0], rate);
      encode_b(cpi, tile, td, tp, mi_row + hbs, mi_col, dry_run, bsize2,
               partition, &pc_tree->verticala[1], rate);
      encode_b(cpi, tile, td, tp, mi_row, mi_col + hbs, dry_run, subsize,
               partition, &pc_tree->verticala[2], rate);

      break;
    case PARTITION_VERT_B:
      encode_b(cpi, tile, td, tp, mi_row, mi_col, dry_run, subsize, partition,
               &pc_tree->verticalb[0], rate);
      encode_b(cpi, tile, td, tp, mi_row, mi_col + hbs, dry_run, bsize2,
               partition, &pc_tree->verticalb[1], rate);
      encode_b(cpi, tile, td, tp, mi_row + hbs, mi_col + hbs, dry_run, bsize2,
               partition, &pc_tree->verticalb[2], rate);
      break;
#endif
    case PARTITION_HORZ_4:
      for (i = 0; i < 4; ++i) {
        int this_mi_row = mi_row + i * quarter_step;
        if (i > 0 && this_mi_row >= cm->mi_rows) break;

        encode_b(cpi, tile, td, tp, this_mi_row, mi_col, dry_run, subsize,
                 partition, &pc_tree->horizontal4[i], rate);
      }
      break;
    case PARTITION_VERT_4:
      for (i = 0; i < 4; ++i) {
        int this_mi_col = mi_col + i * quarter_step;
        if (i > 0 && this_mi_col >= cm->mi_cols) break;

        encode_b(cpi, tile, td, tp, mi_row, this_mi_col, dry_run, subsize,
                 partition, &pc_tree->vertical4[i], rate);
      }
      break;
#endif  // CONFIG_EXT_PARTITION_TYPES
    default: assert(0 && "Invalid partition type."); break;
  }

#if CONFIG_EXT_PARTITION_TYPES
  update_ext_partition_context(xd, mi_row, mi_col, subsize, bsize, partition);
#else
  if (partition != PARTITION_SPLIT || bsize == BLOCK_8X8)
    update_partition_context(xd, mi_row, mi_col, subsize, bsize);
#endif  // CONFIG_EXT_PARTITION_TYPES
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

static void set_partial_sb_partition(const AV1_COMMON *const cm, MODE_INFO *mi,
                                     int bh_in, int bw_in,
                                     int mi_rows_remaining,
                                     int mi_cols_remaining, BLOCK_SIZE bsize,
                                     MODE_INFO **mib) {
  int bh = bh_in;
  int r, c;
  for (r = 0; r < cm->mib_size; r += bh) {
    int bw = bw_in;
    for (c = 0; c < cm->mib_size; c += bw) {
      const int index = r * cm->mi_stride + c;
      mib[index] = mi + index;
      mib[index]->mbmi.sb_type = find_partition_size(
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
                                   MODE_INFO **mib, int mi_row, int mi_col,
                                   BLOCK_SIZE bsize) {
  AV1_COMMON *const cm = &cpi->common;
  const int mi_rows_remaining = tile->mi_row_end - mi_row;
  const int mi_cols_remaining = tile->mi_col_end - mi_col;
  int block_row, block_col;
  MODE_INFO *const mi_upper_left = cm->mi + mi_row * cm->mi_stride + mi_col;
  int bh = mi_size_high[bsize];
  int bw = mi_size_wide[bsize];

  assert((mi_rows_remaining > 0) && (mi_cols_remaining > 0));

  // Apply the requested partition size to the SB if it is all "in image"
  if ((mi_cols_remaining >= cm->mib_size) &&
      (mi_rows_remaining >= cm->mib_size)) {
    for (block_row = 0; block_row < cm->mib_size; block_row += bh) {
      for (block_col = 0; block_col < cm->mib_size; block_col += bw) {
        int index = block_row * cm->mi_stride + block_col;
        mib[index] = mi_upper_left + index;
        mib[index]->mbmi.sb_type = bsize;
      }
    }
  } else {
    // Else this is a partial SB.
    set_partial_sb_partition(cm, mi_upper_left, bh, bw, mi_rows_remaining,
                             mi_cols_remaining, bsize, mib);
  }
}

static void rd_use_partition(AV1_COMP *cpi, ThreadData *td,
                             TileDataEnc *tile_data, MODE_INFO **mib,
                             TOKENEXTRA **tp, int mi_row, int mi_col,
                             BLOCK_SIZE bsize, int *rate, int64_t *dist,
#if CONFIG_SUPERTX
                             int *rate_nocoef,
#endif
                             int do_recon, PC_TREE *pc_tree) {
  AV1_COMMON *const cm = &cpi->common;
  TileInfo *const tile_info = &tile_data->tile_info;
  MACROBLOCK *const x = &td->mb;
  MACROBLOCKD *const xd = &x->e_mbd;
  const int bs = mi_size_wide[bsize];
  const int hbs = bs / 2;
  int i;
  const int pl = (bsize >= BLOCK_8X8)
                     ? partition_plane_context(xd, mi_row, mi_col,
#if CONFIG_UNPOISON_PARTITION_CTX
                                               mi_row + hbs < cm->mi_rows,
                                               mi_col + hbs < cm->mi_cols,
#endif
                                               bsize)
                     : 0;
  const PARTITION_TYPE partition =
      (bsize >= BLOCK_8X8) ? get_partition(cm, mi_row, mi_col, bsize)
                           : PARTITION_NONE;
  const BLOCK_SIZE subsize = get_subsize(bsize, partition);
  RD_SEARCH_MACROBLOCK_CONTEXT x_ctx;
  RD_STATS last_part_rdc, none_rdc, chosen_rdc;
  BLOCK_SIZE sub_subsize = BLOCK_4X4;
  int splits_below = 0;
  BLOCK_SIZE bs_type = mib[0]->mbmi.sb_type;
  int do_partition_search = 1;
  PICK_MODE_CONTEXT *ctx_none = &pc_tree->none;
  const int unify_bsize = CONFIG_CB4X4;
#if CONFIG_SUPERTX
  int last_part_rate_nocoef = INT_MAX;
  int none_rate_nocoef = INT_MAX;
  int chosen_rate_nocoef = INT_MAX;
#endif
#if CONFIG_PVQ
  od_rollback_buffer pre_rdo_buf;
#endif
  if (mi_row >= cm->mi_rows || mi_col >= cm->mi_cols) return;

  assert(num_4x4_blocks_wide_lookup[bsize] ==
         num_4x4_blocks_high_lookup[bsize]);

  av1_invalid_rd_stats(&last_part_rdc);
  av1_invalid_rd_stats(&none_rdc);
  av1_invalid_rd_stats(&chosen_rdc);

  pc_tree->partitioning = partition;

#if CONFIG_VAR_TX
  xd->above_txfm_context =
      cm->above_txfm_context + (mi_col << TX_UNIT_WIDE_LOG2);
  xd->left_txfm_context = xd->left_txfm_context_buffer +
                          ((mi_row & MAX_MIB_MASK) << TX_UNIT_HIGH_LOG2);
#endif
#if !CONFIG_PVQ
  save_context(x, &x_ctx, mi_row, mi_col, bsize);
#else
  save_context(x, &x_ctx, mi_row, mi_col, &pre_rdo_buf, bsize);
#endif

  if (bsize == BLOCK_16X16 && cpi->vaq_refresh) {
    set_offsets(cpi, tile_info, x, mi_row, mi_col, bsize);
    x->mb_energy = av1_block_energy(cpi, x, bsize);
  }

  if (do_partition_search &&
      cpi->sf.partition_search_type == SEARCH_PARTITION &&
      cpi->sf.adjust_partitioning_from_last_frame) {
    // Check if any of the sub blocks are further split.
    if (partition == PARTITION_SPLIT && subsize > BLOCK_8X8) {
      sub_subsize = get_subsize(subsize, PARTITION_SPLIT);
      splits_below = 1;
      for (i = 0; i < 4; i++) {
        int jj = i >> 1, ii = i & 0x01;
        MODE_INFO *this_mi = mib[jj * hbs * cm->mi_stride + ii * hbs];
        if (this_mi && this_mi->mbmi.sb_type >= sub_subsize) {
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
#if CONFIG_SUPERTX
                       &none_rate_nocoef,
#endif
#if CONFIG_EXT_PARTITION_TYPES
                       PARTITION_NONE,
#endif
                       bsize, ctx_none, INT64_MAX);

      if (none_rdc.rate < INT_MAX) {
        none_rdc.rate += x->partition_cost[pl][PARTITION_NONE];
        none_rdc.rdcost = RDCOST(x->rdmult, none_rdc.rate, none_rdc.dist);
#if CONFIG_SUPERTX
        none_rate_nocoef += x->partition_cost[pl][PARTITION_NONE];
#endif
      }

#if !CONFIG_PVQ
      restore_context(x, &x_ctx, mi_row, mi_col, bsize);
#else
      restore_context(x, &x_ctx, mi_row, mi_col, &pre_rdo_buf, bsize);
#endif
      mib[0]->mbmi.sb_type = bs_type;
      pc_tree->partitioning = partition;
    }
  }

  switch (partition) {
    case PARTITION_NONE:
      rd_pick_sb_modes(cpi, tile_data, x, mi_row, mi_col, &last_part_rdc,
#if CONFIG_SUPERTX
                       &last_part_rate_nocoef,
#endif
#if CONFIG_EXT_PARTITION_TYPES
                       PARTITION_NONE,
#endif
                       bsize, ctx_none, INT64_MAX);
      break;
    case PARTITION_HORZ:
      rd_pick_sb_modes(cpi, tile_data, x, mi_row, mi_col, &last_part_rdc,
#if CONFIG_SUPERTX
                       &last_part_rate_nocoef,
#endif
#if CONFIG_EXT_PARTITION_TYPES
                       PARTITION_HORZ,
#endif
                       subsize, &pc_tree->horizontal[0], INT64_MAX);
      if (last_part_rdc.rate != INT_MAX && bsize >= BLOCK_8X8 &&
          mi_row + hbs < cm->mi_rows) {
        RD_STATS tmp_rdc;
#if CONFIG_SUPERTX
        int rt_nocoef = 0;
#endif
        PICK_MODE_CONTEXT *ctx_h = &pc_tree->horizontal[0];
        av1_init_rd_stats(&tmp_rdc);
        update_state(cpi, td, ctx_h, mi_row, mi_col, subsize, 1);
        encode_superblock(cpi, td, tp, DRY_RUN_NORMAL, mi_row, mi_col, subsize,
                          NULL);
        rd_pick_sb_modes(cpi, tile_data, x, mi_row + hbs, mi_col, &tmp_rdc,
#if CONFIG_SUPERTX
                         &rt_nocoef,
#endif
#if CONFIG_EXT_PARTITION_TYPES
                         PARTITION_HORZ,
#endif
                         subsize, &pc_tree->horizontal[1], INT64_MAX);
        if (tmp_rdc.rate == INT_MAX || tmp_rdc.dist == INT64_MAX) {
          av1_invalid_rd_stats(&last_part_rdc);
#if CONFIG_SUPERTX
          last_part_rate_nocoef = INT_MAX;
#endif
          break;
        }
        last_part_rdc.rate += tmp_rdc.rate;
        last_part_rdc.dist += tmp_rdc.dist;
        last_part_rdc.rdcost += tmp_rdc.rdcost;
#if CONFIG_SUPERTX
        last_part_rate_nocoef += rt_nocoef;
#endif
      }
      break;
    case PARTITION_VERT:
      rd_pick_sb_modes(cpi, tile_data, x, mi_row, mi_col, &last_part_rdc,
#if CONFIG_SUPERTX
                       &last_part_rate_nocoef,
#endif
#if CONFIG_EXT_PARTITION_TYPES
                       PARTITION_VERT,
#endif
                       subsize, &pc_tree->vertical[0], INT64_MAX);
      if (last_part_rdc.rate != INT_MAX && bsize >= BLOCK_8X8 &&
          mi_col + hbs < cm->mi_cols) {
        RD_STATS tmp_rdc;
#if CONFIG_SUPERTX
        int rt_nocoef = 0;
#endif
        PICK_MODE_CONTEXT *ctx_v = &pc_tree->vertical[0];
        av1_init_rd_stats(&tmp_rdc);
        update_state(cpi, td, ctx_v, mi_row, mi_col, subsize, 1);
        encode_superblock(cpi, td, tp, DRY_RUN_NORMAL, mi_row, mi_col, subsize,
                          NULL);
        rd_pick_sb_modes(cpi, tile_data, x, mi_row, mi_col + hbs, &tmp_rdc,
#if CONFIG_SUPERTX
                         &rt_nocoef,
#endif
#if CONFIG_EXT_PARTITION_TYPES
                         PARTITION_VERT,
#endif
                         subsize, &pc_tree->vertical[bsize > BLOCK_8X8],
                         INT64_MAX);
        if (tmp_rdc.rate == INT_MAX || tmp_rdc.dist == INT64_MAX) {
          av1_invalid_rd_stats(&last_part_rdc);
#if CONFIG_SUPERTX
          last_part_rate_nocoef = INT_MAX;
#endif
          break;
        }
        last_part_rdc.rate += tmp_rdc.rate;
        last_part_rdc.dist += tmp_rdc.dist;
        last_part_rdc.rdcost += tmp_rdc.rdcost;
#if CONFIG_SUPERTX
        last_part_rate_nocoef += rt_nocoef;
#endif
      }
      break;
    case PARTITION_SPLIT:
      if (bsize == BLOCK_8X8 && !unify_bsize) {
        rd_pick_sb_modes(cpi, tile_data, x, mi_row, mi_col, &last_part_rdc,
#if CONFIG_SUPERTX
                         &last_part_rate_nocoef,
#endif
#if CONFIG_EXT_PARTITION_TYPES
                         PARTITION_SPLIT,
#endif
                         subsize, pc_tree->leaf_split[0], INT64_MAX);
        break;
      }
      last_part_rdc.rate = 0;
      last_part_rdc.dist = 0;
      last_part_rdc.rdcost = 0;
#if CONFIG_SUPERTX
      last_part_rate_nocoef = 0;
#endif
      for (i = 0; i < 4; i++) {
        int x_idx = (i & 1) * hbs;
        int y_idx = (i >> 1) * hbs;
        int jj = i >> 1, ii = i & 0x01;
        RD_STATS tmp_rdc;
#if CONFIG_SUPERTX
        int rt_nocoef;
#endif
        if ((mi_row + y_idx >= cm->mi_rows) || (mi_col + x_idx >= cm->mi_cols))
          continue;

        av1_init_rd_stats(&tmp_rdc);
        rd_use_partition(cpi, td, tile_data,
                         mib + jj * hbs * cm->mi_stride + ii * hbs, tp,
                         mi_row + y_idx, mi_col + x_idx, subsize, &tmp_rdc.rate,
                         &tmp_rdc.dist,
#if CONFIG_SUPERTX
                         &rt_nocoef,
#endif
                         i != 3, pc_tree->split[i]);
        if (tmp_rdc.rate == INT_MAX || tmp_rdc.dist == INT64_MAX) {
          av1_invalid_rd_stats(&last_part_rdc);
#if CONFIG_SUPERTX
          last_part_rate_nocoef = INT_MAX;
#endif
          break;
        }
        last_part_rdc.rate += tmp_rdc.rate;
        last_part_rdc.dist += tmp_rdc.dist;
#if CONFIG_SUPERTX
        last_part_rate_nocoef += rt_nocoef;
#endif
      }
      break;
#if CONFIG_EXT_PARTITION_TYPES
    case PARTITION_VERT_A:
    case PARTITION_VERT_B:
    case PARTITION_HORZ_A:
    case PARTITION_HORZ_B:
    case PARTITION_HORZ_4:
    case PARTITION_VERT_4: assert(0 && "Cannot handle extended partiton types");
#endif  //  CONFIG_EXT_PARTITION_TYPES
    default: assert(0); break;
  }

  if (last_part_rdc.rate < INT_MAX) {
    last_part_rdc.rate += x->partition_cost[pl][partition];
    last_part_rdc.rdcost =
        RDCOST(x->rdmult, last_part_rdc.rate, last_part_rdc.dist);
#if CONFIG_SUPERTX
    last_part_rate_nocoef += x->partition_cost[pl][partition];
#endif
  }

  if (do_partition_search && cpi->sf.adjust_partitioning_from_last_frame &&
      cpi->sf.partition_search_type == SEARCH_PARTITION &&
      partition != PARTITION_SPLIT && bsize > BLOCK_8X8 &&
      (mi_row + bs < cm->mi_rows || mi_row + hbs == cm->mi_rows) &&
      (mi_col + bs < cm->mi_cols || mi_col + hbs == cm->mi_cols)) {
    BLOCK_SIZE split_subsize = get_subsize(bsize, PARTITION_SPLIT);
    chosen_rdc.rate = 0;
    chosen_rdc.dist = 0;
#if CONFIG_SUPERTX
    chosen_rate_nocoef = 0;
#endif
#if !CONFIG_PVQ
    restore_context(x, &x_ctx, mi_row, mi_col, bsize);
#else
    restore_context(x, &x_ctx, mi_row, mi_col, &pre_rdo_buf, bsize);
#endif
    pc_tree->partitioning = PARTITION_SPLIT;

    // Split partition.
    for (i = 0; i < 4; i++) {
      int x_idx = (i & 1) * hbs;
      int y_idx = (i >> 1) * hbs;
      RD_STATS tmp_rdc;
#if CONFIG_SUPERTX
      int rt_nocoef = 0;
#endif
#if CONFIG_PVQ
      od_rollback_buffer buf;
#endif
      if ((mi_row + y_idx >= cm->mi_rows) || (mi_col + x_idx >= cm->mi_cols))
        continue;

#if !CONFIG_PVQ
      save_context(x, &x_ctx, mi_row, mi_col, bsize);
#else
      save_context(x, &x_ctx, mi_row, mi_col, &buf, bsize);
#endif
      pc_tree->split[i]->partitioning = PARTITION_NONE;
      rd_pick_sb_modes(cpi, tile_data, x, mi_row + y_idx, mi_col + x_idx,
                       &tmp_rdc,
#if CONFIG_SUPERTX
                       &rt_nocoef,
#endif
#if CONFIG_EXT_PARTITION_TYPES
                       PARTITION_SPLIT,
#endif
                       split_subsize, &pc_tree->split[i]->none, INT64_MAX);

#if !CONFIG_PVQ
      restore_context(x, &x_ctx, mi_row, mi_col, bsize);
#else
      restore_context(x, &x_ctx, mi_row, mi_col, &buf, bsize);
#endif
      if (tmp_rdc.rate == INT_MAX || tmp_rdc.dist == INT64_MAX) {
        av1_invalid_rd_stats(&chosen_rdc);
#if CONFIG_SUPERTX
        chosen_rate_nocoef = INT_MAX;
#endif
        break;
      }

      chosen_rdc.rate += tmp_rdc.rate;
      chosen_rdc.dist += tmp_rdc.dist;
#if CONFIG_SUPERTX
      chosen_rate_nocoef += rt_nocoef;
#endif

      if (i != 3)
        encode_sb(cpi, td, tile_info, tp, mi_row + y_idx, mi_col + x_idx,
                  OUTPUT_ENABLED, split_subsize, pc_tree->split[i], NULL);

      chosen_rdc.rate += x->partition_cost[pl][PARTITION_NONE];
#if CONFIG_SUPERTX
      chosen_rate_nocoef += x->partition_cost[pl][PARTITION_SPLIT];
#endif
    }
    if (chosen_rdc.rate < INT_MAX) {
      chosen_rdc.rate += x->partition_cost[pl][PARTITION_SPLIT];
      chosen_rdc.rdcost = RDCOST(x->rdmult, chosen_rdc.rate, chosen_rdc.dist);
#if CONFIG_SUPERTX
      chosen_rate_nocoef += x->partition_cost[pl][PARTITION_NONE];
#endif
    }
  }

  // If last_part is better set the partitioning to that.
  if (last_part_rdc.rdcost < chosen_rdc.rdcost) {
    mib[0]->mbmi.sb_type = bsize;
    if (bsize >= BLOCK_8X8) pc_tree->partitioning = partition;
    chosen_rdc = last_part_rdc;
#if CONFIG_SUPERTX
    chosen_rate_nocoef = last_part_rate_nocoef;
#endif
  }
  // If none was better set the partitioning to that.
  if (none_rdc.rdcost < chosen_rdc.rdcost) {
    if (bsize >= BLOCK_8X8) pc_tree->partitioning = PARTITION_NONE;
    chosen_rdc = none_rdc;
#if CONFIG_SUPERTX
    chosen_rate_nocoef = none_rate_nocoef;
#endif
  }

#if !CONFIG_PVQ
  restore_context(x, &x_ctx, mi_row, mi_col, bsize);
#else
  restore_context(x, &x_ctx, mi_row, mi_col, &pre_rdo_buf, bsize);
#endif

  // We must have chosen a partitioning and encoding or we'll fail later on.
  // No other opportunities for success.
  if (bsize == cm->sb_size)
    assert(chosen_rdc.rate < INT_MAX && chosen_rdc.dist < INT64_MAX);

  if (do_recon) {
    if (bsize == cm->sb_size) {
      // NOTE: To get estimate for rate due to the tokens, use:
      // int rate_coeffs = 0;
      // encode_sb(cpi, td, tile_info, tp, mi_row, mi_col, DRY_RUN_COSTCOEFFS,
      //           bsize, pc_tree, &rate_coeffs);
      encode_sb(cpi, td, tile_info, tp, mi_row, mi_col, OUTPUT_ENABLED, bsize,
                pc_tree, NULL);
    } else {
      encode_sb(cpi, td, tile_info, tp, mi_row, mi_col, DRY_RUN_NORMAL, bsize,
                pc_tree, NULL);
    }
  }

  *rate = chosen_rdc.rate;
  *dist = chosen_rdc.dist;
#if CONFIG_SUPERTX
  *rate_nocoef = chosen_rate_nocoef;
#endif
}

/* clang-format off */
static const BLOCK_SIZE min_partition_size[BLOCK_SIZES_ALL] = {
#if CONFIG_CHROMA_2X2 || CONFIG_CHROMA_SUB8X8
  BLOCK_2X2,   BLOCK_2X2,   BLOCK_2X2,    //    2x2,    2x4,     4x2
#endif
                            BLOCK_4X4,    //                     4x4
  BLOCK_4X4,   BLOCK_4X4,   BLOCK_4X4,    //    4x8,    8x4,     8x8
  BLOCK_4X4,   BLOCK_4X4,   BLOCK_8X8,    //   8x16,   16x8,   16x16
  BLOCK_8X8,   BLOCK_8X8,   BLOCK_16X16,  //  16x32,  32x16,   32x32
  BLOCK_16X16, BLOCK_16X16, BLOCK_16X16,  //  32x64,  64x32,   64x64
#if CONFIG_EXT_PARTITION
  BLOCK_16X16, BLOCK_16X16, BLOCK_16X16,  // 64x128, 128x64, 128x128
#endif  // CONFIG_EXT_PARTITION
  BLOCK_4X4,   BLOCK_4X4,   BLOCK_8X8,    //   4x16,   16x4,    8x32
  BLOCK_8X8,   BLOCK_16X16, BLOCK_16X16,  //   32x8,  16x64,   64x16
#if CONFIG_EXT_PARTITION
  BLOCK_16X16, BLOCK_16X16                // 32x128, 128x32
#endif  // CONFIG_EXT_PARTITION
};

static const BLOCK_SIZE max_partition_size[BLOCK_SIZES_ALL] = {
#if CONFIG_CHROMA_2X2 || CONFIG_CHROMA_SUB8X8
  BLOCK_4X4,     BLOCK_4X4,       BLOCK_4X4,    //    2x2,    2x4,     4x2
#endif
                                  BLOCK_8X8,    //                     4x4
  BLOCK_16X16,   BLOCK_16X16,   BLOCK_16X16,    //    4x8,    8x4,     8x8
  BLOCK_32X32,   BLOCK_32X32,   BLOCK_32X32,    //   8x16,   16x8,   16x16
  BLOCK_64X64,   BLOCK_64X64,   BLOCK_64X64,    //  16x32,  32x16,   32x32
  BLOCK_LARGEST, BLOCK_LARGEST, BLOCK_LARGEST,  //  32x64,  64x32,   64x64
#if CONFIG_EXT_PARTITION
  BLOCK_LARGEST, BLOCK_LARGEST, BLOCK_LARGEST,  // 64x128, 128x64, 128x128
#endif  // CONFIG_EXT_PARTITION
  BLOCK_16X16,   BLOCK_16X16,   BLOCK_32X32,    //   4x16,   16x4,    8x32
  BLOCK_32X32,   BLOCK_LARGEST, BLOCK_LARGEST,  //   32x8,  16x64,   64x16
#if CONFIG_EXT_PARTITION
  BLOCK_LARGEST, BLOCK_LARGEST                  // 32x128, 128x32
#endif  // CONFIG_EXT_PARTITION
};

// Next square block size less or equal than current block size.
static const BLOCK_SIZE next_square_size[BLOCK_SIZES_ALL] = {
#if CONFIG_CHROMA_2X2 || CONFIG_CHROMA_SUB8X8
  BLOCK_2X2,   BLOCK_2X2,     BLOCK_2X2,    //    2x2,    2x4,     4x2
#endif
                              BLOCK_4X4,    //                     4x4
  BLOCK_4X4,   BLOCK_4X4,     BLOCK_8X8,    //    4x8,    8x4,     8x8
  BLOCK_8X8,   BLOCK_8X8,     BLOCK_16X16,  //   8x16,   16x8,   16x16
  BLOCK_16X16, BLOCK_16X16,   BLOCK_32X32,  //  16x32,  32x16,   32x32
  BLOCK_32X32, BLOCK_32X32,   BLOCK_64X64,  //  32x64,  64x32,   64x64
#if CONFIG_EXT_PARTITION
  BLOCK_64X64, BLOCK_64X64, BLOCK_128X128,  // 64x128, 128x64, 128x128
#endif  // CONFIG_EXT_PARTITION
  BLOCK_4X4,   BLOCK_4X4,   BLOCK_8X8,      //   4x16,   16x4,    8x32
  BLOCK_8X8,   BLOCK_16X16, BLOCK_16X16,    //   32x8,  16x64,   64x16
#if CONFIG_EXT_PARTITION
  BLOCK_32X32, BLOCK_32X32                  // 32x128, 128x32
#endif  // CONFIG_EXT_PARTITION
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
                                        MACROBLOCKD *xd, MODE_INFO **mib,
                                        BLOCK_SIZE *min_block_size,
                                        BLOCK_SIZE *max_block_size) {
  int i, j;
  int index = 0;

  // Check the sb_type for each block that belongs to this region.
  for (i = 0; i < cm->mib_size; ++i) {
    for (j = 0; j < cm->mib_size; ++j) {
      MODE_INFO *mi = mib[index + j];
      BLOCK_SIZE sb_type = mi ? mi->mbmi.sb_type : BLOCK_4X4;
      *min_block_size = AOMMIN(*min_block_size, sb_type);
      *max_block_size = AOMMAX(*max_block_size, sb_type);
    }
    index += xd->mi_stride;
  }
}

// Look at neighboring blocks and set a min and max partition size based on
// what they chose.
static void rd_auto_partition_range(AV1_COMP *cpi, const TileInfo *const tile,
                                    MACROBLOCKD *const xd, int mi_row,
                                    int mi_col, BLOCK_SIZE *min_block_size,
                                    BLOCK_SIZE *max_block_size) {
  AV1_COMMON *const cm = &cpi->common;
  MODE_INFO **mi = xd->mi;
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
      MODE_INFO **prev_mi =
          &cm->prev_mi_grid_visible[mi_row * xd->mi_stride + mi_col];
      get_sb_partition_size_range(cm, xd, prev_mi, &min_size, &max_size);
    }
    // Find the min and max partition sizes used in the left superblock
    if (left_in_image) {
      MODE_INFO **left_sb_mi = &mi[-cm->mib_size];
      get_sb_partition_size_range(cm, xd, left_sb_mi, &min_size, &max_size);
    }
    // Find the min and max partition sizes used in the above suprblock.
    if (above_in_image) {
      MODE_INFO **above_sb_mi = &mi[-xd->mi_stride * cm->mib_size];
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
  if (av1_active_edge_sb(cpi, mi_row, mi_col)) {
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

  *min_block_size = AOMMIN(min_size, cm->sb_size);
  *max_block_size = AOMMIN(max_size, cm->sb_size);
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
  MODE_INFO **const prev_mi = &cm->prev_mi_grid_visible[idx_str];
  BLOCK_SIZE min_size = cm->sb_size;  // default values
  BLOCK_SIZE max_size = BLOCK_4X4;

  if (prev_mi) {
    for (idy = 0; idy < mi_height; ++idy) {
      for (idx = 0; idx < mi_width; ++idx) {
        const MODE_INFO *const mi = prev_mi[idy * cm->mi_stride + idx];
        const BLOCK_SIZE bs = mi ? mi->mbmi.sb_type : bsize;
        min_size = AOMMIN(min_size, bs);
        max_size = AOMMAX(max_size, bs);
      }
    }
  }

  if (xd->left_available) {
    for (idy = 0; idy < mi_height; ++idy) {
      const MODE_INFO *const mi = xd->mi[idy * cm->mi_stride - 1];
      const BLOCK_SIZE bs = mi ? mi->mbmi.sb_type : bsize;
      min_size = AOMMIN(min_size, bs);
      max_size = AOMMAX(max_size, bs);
    }
  }

  if (xd->up_available) {
    for (idx = 0; idx < mi_width; ++idx) {
      const MODE_INFO *const mi = xd->mi[idx - cm->mi_stride];
      const BLOCK_SIZE bs = mi ? mi->mbmi.sb_type : bsize;
      min_size = AOMMIN(min_size, bs);
      max_size = AOMMAX(max_size, bs);
    }
  }

  if (min_size == max_size) {
    min_size = min_partition_size[min_size];
    max_size = max_partition_size[max_size];
  }

  *min_bs = AOMMIN(min_size, cm->sb_size);
  *max_bs = AOMMIN(max_size, cm->sb_size);
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
#if CONFIG_EXT_PARTITION
  // TODO(debargha): What are the correct numbers here?
  130, 130, 150
#endif  // CONFIG_EXT_PARTITION
};
const int qindex_split_threshold_lookup[BLOCK_SIZES] = {
  0, 3, 3, 7, 15, 15, 30, 40, 40, 60, 80, 80, 120,
#if CONFIG_EXT_PARTITION
  // TODO(debargha): What are the correct numbers here?
  160, 160, 240
#endif  // CONFIG_EXT_PARTITION
};
const int complexity_16x16_blocks_threshold[BLOCK_SIZES] = {
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 4, 4, 6,
#if CONFIG_EXT_PARTITION
  // TODO(debargha): What are the correct numbers here?
  8, 8, 10
#endif  // CONFIG_EXT_PARTITION
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

#if CONFIG_EXT_PARTITION_TYPES
// Try searching for an encoding for the given subblock. Returns zero if the
// rdcost is already too high (to tell the caller not to bother searching for
// encodings of further subblocks)
static int rd_try_subblock(const AV1_COMP *const cpi, ThreadData *td,
                           TileDataEnc *tile_data, TOKENEXTRA **tp,
                           int is_first, int is_last, int mi_row, int mi_col,
                           BLOCK_SIZE subsize, RD_STATS *best_rdc,
                           RD_STATS *sum_rdc, RD_STATS *this_rdc,
#if CONFIG_SUPERTX
                           int64_t best_rd, int *sum_rate_nocoef,
                           int *this_rate_nocoef, int *abort_flag,
#endif
                           PARTITION_TYPE partition,
                           PICK_MODE_CONTEXT *prev_ctx,
                           PICK_MODE_CONTEXT *this_ctx) {
#if CONFIG_SUPERTX
#define RTS_X_RATE_NOCOEF_ARG ((is_first) ? sum_rate_nocoef : this_rate_nocoef),
#define RTS_MAX_RDCOST INT64_MAX
#else
#define RTS_X_RATE_NOCOEF_ARG
#define RTS_MAX_RDCOST best_rdc->rdcost
#endif

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

#if CONFIG_SUPERTX
  if (is_first) *abort_flag = sum_rdc->rdcost >= best_rd;
#endif

  if (!is_first) {
    if (this_rdc->rate == INT_MAX) {
      sum_rdc->rdcost = INT64_MAX;
#if CONFIG_SUPERTX
      *sum_rate_nocoef = INT_MAX;
#endif
    } else {
      sum_rdc->rate += this_rdc->rate;
      sum_rdc->dist += this_rdc->dist;
      sum_rdc->rdcost += this_rdc->rdcost;
#if CONFIG_SUPERTX
      *sum_rate_nocoef += *this_rate_nocoef;
#endif
    }
  }

  if (sum_rdc->rdcost >= RTS_MAX_RDCOST) return 0;

  if (!is_last) {
    update_state(cpi, td, this_ctx, mi_row, mi_col, subsize, 1);
    encode_superblock(cpi, td, tp, DRY_RUN_NORMAL, mi_row, mi_col, subsize,
                      NULL);
  }

  return 1;

#undef RTS_X_RATE_NOCOEF_ARG
#undef RTS_MAX_RDCOST
}

static void rd_test_partition3(
    const AV1_COMP *const cpi, ThreadData *td, TileDataEnc *tile_data,
    TOKENEXTRA **tp, PC_TREE *pc_tree, RD_STATS *best_rdc,
    PICK_MODE_CONTEXT ctxs[3], PICK_MODE_CONTEXT *ctx, int mi_row, int mi_col,
    BLOCK_SIZE bsize, PARTITION_TYPE partition,
#if CONFIG_SUPERTX
    int64_t best_rd, int *best_rate_nocoef, RD_SEARCH_MACROBLOCK_CONTEXT *x_ctx,
#endif
    int mi_row0, int mi_col0, BLOCK_SIZE subsize0, int mi_row1, int mi_col1,
    BLOCK_SIZE subsize1, int mi_row2, int mi_col2, BLOCK_SIZE subsize2) {
  MACROBLOCK *const x = &td->mb;
  MACROBLOCKD *const xd = &x->e_mbd;
  RD_STATS sum_rdc, this_rdc;
#if CONFIG_UNPOISON_PARTITION_CTX
  const AV1_COMMON *const cm = &cpi->common;
  const int hbs = mi_size_wide[bsize] / 2;
  const int has_rows = mi_row + hbs < cm->mi_rows;
  const int has_cols = mi_col + hbs < cm->mi_cols;
#endif  // CONFIG_UNPOISON_PARTITION_CTX
#if CONFIG_SUPERTX || CONFIG_EXT_PARTITION_TYPES_AB
  const AV1_COMMON *const cm = &cpi->common;
#endif
#if CONFIG_SUPERTX
  TileInfo *const tile_info = &tile_data->tile_info;
  int sum_rate_nocoef, this_rate_nocoef;
  int abort_flag;
  const int supertx_allowed = !frame_is_intra_only(cm) &&
                              bsize <= MAX_SUPERTX_BLOCK_SIZE &&
                              !xd->lossless[0];

#define RTP_STX_TRY_ARGS \
  best_rd, &sum_rate_nocoef, &this_rate_nocoef, &abort_flag,
#else
#define RTP_STX_TRY_ARGS
#endif

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
#if CONFIG_EXT_PARTITION_TYPES_AB
  const int try_block2 = mi_row2 < cm->mi_rows && mi_col2 < cm->mi_cols;
#else
  const int try_block2 = 1;
#endif
  if (try_block2 &&
      !rd_try_subblock(cpi, td, tile_data, tp, 0, 1, mi_row2, mi_col2, subsize2,
                       best_rdc, &sum_rdc, &this_rdc,
                       RTP_STX_TRY_ARGS partition, &ctxs[1], &ctxs[2]))
    return;

#if CONFIG_SUPERTX
  if (supertx_allowed && !abort_flag && sum_rdc.rdcost < INT64_MAX) {
    TX_SIZE supertx_size = max_txsize_lookup[bsize];
    const PARTITION_TYPE best_partition = pc_tree->partitioning;
    pc_tree->partitioning = partition;
    sum_rdc.rate += av1_cost_bit(
        cm->fc->supertx_prob[partition_supertx_context_lookup[partition]]
                            [supertx_size],
        0);
    sum_rdc.rdcost = RDCOST(x->rdmult, sum_rdc.rate, sum_rdc.dist);

    if (!check_intra_sb(cpi, tile_info, mi_row, mi_col, bsize, pc_tree)) {
      TX_TYPE best_tx = DCT_DCT;
      RD_STATS tmp_rdc = { sum_rate_nocoef, 0, 0 };

      restore_context(x, x_ctx, mi_row, mi_col, bsize);

      rd_supertx_sb(cpi, td, tile_info, mi_row, mi_col, bsize, &tmp_rdc.rate,
                    &tmp_rdc.dist, &best_tx, pc_tree);

      tmp_rdc.rate += av1_cost_bit(
          cm->fc->supertx_prob[partition_supertx_context_lookup[partition]]
                              [supertx_size],
          1);
      tmp_rdc.rdcost = RDCOST(x->rdmult, tmp_rdc.rate, tmp_rdc.dist);
      if (tmp_rdc.rdcost < sum_rdc.rdcost) {
        sum_rdc = tmp_rdc;
        update_supertx_param_sb(cpi, td, mi_row, mi_col, bsize, best_tx,
                                supertx_size, pc_tree);
      }
    }

    pc_tree->partitioning = best_partition;
  }
#endif

  if (sum_rdc.rdcost >= best_rdc->rdcost) return;

  int pl = partition_plane_context(xd, mi_row, mi_col,
#if CONFIG_UNPOISON_PARTITION_CTX
                                   has_rows, has_cols,
#endif
                                   bsize);
  sum_rdc.rate += x->partition_cost[pl][partition];
  sum_rdc.rdcost = RDCOST(x->rdmult, sum_rdc.rate, sum_rdc.dist);
#if CONFIG_SUPERTX
  sum_rate_nocoef += x->partition_cost[pl][partition];
#endif

  if (sum_rdc.rdcost >= best_rdc->rdcost) return;

#if CONFIG_SUPERTX
  *best_rate_nocoef = sum_rate_nocoef;
  assert(*best_rate_nocoef >= 0);
#endif
  *best_rdc = sum_rdc;
  pc_tree->partitioning = partition;

#undef RTP_STX_TRY_ARGS
}
#endif  // CONFIG_EXT_PARTITION_TYPES

#if CONFIG_DIST_8X8 && CONFIG_CB4X4
static int64_t dist_8x8_yuv(const AV1_COMP *const cpi, MACROBLOCK *const x,
                            uint8_t *y_src_8x8) {
  MACROBLOCKD *const xd = &x->e_mbd;
  int64_t dist_8x8, dist_8x8_uv, total_dist;
  const int src_stride = x->plane[0].src.stride;
  uint8_t *decoded_8x8;
  int plane;

#if CONFIG_HIGHBITDEPTH
  if (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH)
    decoded_8x8 = CONVERT_TO_BYTEPTR(x->decoded_8x8);
  else
#endif
    decoded_8x8 = (uint8_t *)x->decoded_8x8;

  dist_8x8 = av1_dist_8x8(cpi, x, y_src_8x8, src_stride, decoded_8x8, 8,
                          BLOCK_8X8, 8, 8, 8, 8, x->qindex)
             << 4;

  // Compute chroma distortion for a luma 8x8 block
  dist_8x8_uv = 0;

  for (plane = 1; plane < MAX_MB_PLANE; ++plane) {
    const int src_stride_uv = x->plane[plane].src.stride;
    const int dst_stride_uv = xd->plane[plane].dst.stride;
    // uv buff pointers now (i.e. the last sub8x8 block) is the same
    // to those at the first sub8x8 block because
    // uv buff pointer is set only once at first sub8x8 block in a 8x8.
    uint8_t *src_uv = x->plane[plane].src.buf;
    uint8_t *dst_uv = xd->plane[plane].dst.buf;
    unsigned sse;
#if CONFIG_CHROMA_SUB8X8
    const BLOCK_SIZE plane_bsize =
        AOMMAX(BLOCK_4X4, get_plane_block_size(BLOCK_8X8, &xd->plane[plane]));
#else
    const BLOCK_SIZE plane_bsize =
        get_plane_block_size(BLOCK_8X8, &xd->plane[plane]);
#endif
    cpi->fn_ptr[plane_bsize].vf(src_uv, src_stride_uv, dst_uv, dst_stride_uv,
                                &sse);
    dist_8x8_uv += (int64_t)sse << 4;
  }

  return total_dist = dist_8x8 + dist_8x8_uv;
}
#endif  // CONFIG_DIST_8X8 && CONFIG_CB4X4

// TODO(jingning,jimbankoski,rbultje): properly skip partition types that are
// unlikely to be selected depending on previous rate-distortion optimization
// results, for encoding speed-up.
static void rd_pick_partition(const AV1_COMP *const cpi, ThreadData *td,
                              TileDataEnc *tile_data, TOKENEXTRA **tp,
                              int mi_row, int mi_col, BLOCK_SIZE bsize,
                              RD_STATS *rd_cost,
#if CONFIG_SUPERTX
                              int *rate_nocoef,
#endif
                              int64_t best_rd, PC_TREE *pc_tree) {
  const AV1_COMMON *const cm = &cpi->common;
  TileInfo *const tile_info = &tile_data->tile_info;
  MACROBLOCK *const x = &td->mb;
  MACROBLOCKD *const xd = &x->e_mbd;
  const int mi_step = mi_size_wide[bsize] / 2;
  RD_SEARCH_MACROBLOCK_CONTEXT x_ctx;
  const TOKENEXTRA *const tp_orig = *tp;
  PICK_MODE_CONTEXT *ctx_none = &pc_tree->none;
#if CONFIG_UNPOISON_PARTITION_CTX
  const int hbs = mi_size_wide[bsize] / 2;
  const int has_rows = mi_row + hbs < cm->mi_rows;
  const int has_cols = mi_col + hbs < cm->mi_cols;
#else
  int tmp_partition_cost[PARTITION_TYPES];
#endif
  BLOCK_SIZE subsize;
  RD_STATS this_rdc, sum_rdc, best_rdc;
  const int bsize_at_least_8x8 = (bsize >= BLOCK_8X8);
  int do_square_split = bsize_at_least_8x8;
#if CONFIG_CB4X4
  const int unify_bsize = 1;
  const int pl = bsize_at_least_8x8
                     ? partition_plane_context(xd, mi_row, mi_col,
#if CONFIG_UNPOISON_PARTITION_CTX
                                               has_rows, has_cols,
#endif
                                               bsize)
                     : 0;
#else
  const int unify_bsize = 0;
  const int pl = partition_plane_context(xd, mi_row, mi_col,
#if CONFIG_UNPOISON_PARTITION_CTX
                                         has_rows, has_cols,
#endif
                                         bsize);
#endif  // CONFIG_CB4X4
  const int *partition_cost =
      pl >= 0 ? x->partition_cost[pl] : x->partition_cost[0];
#if CONFIG_SUPERTX
  int this_rate_nocoef, sum_rate_nocoef = 0, best_rate_nocoef = INT_MAX;
  int abort_flag;
  const int supertx_allowed = !frame_is_intra_only(cm) && bsize >= BLOCK_8X8 &&
                              bsize <= MAX_SUPERTX_BLOCK_SIZE &&
                              !xd->lossless[0];
#endif  // CONFIG_SUPERTX

  int do_rectangular_split = 1;
#if CONFIG_EXT_PARTITION_TYPES && !CONFIG_EXT_PARTITION_TYPES_AB
  BLOCK_SIZE bsize2 = get_subsize(bsize, PARTITION_SPLIT);
#endif

  // Override skipping rectangular partition operations for edge blocks
  const int force_horz_split = (mi_row + mi_step >= cm->mi_rows);
  const int force_vert_split = (mi_col + mi_step >= cm->mi_cols);
  const int xss = x->e_mbd.plane[1].subsampling_x;
  const int yss = x->e_mbd.plane[1].subsampling_y;

  BLOCK_SIZE min_size = x->min_partition_size;
  BLOCK_SIZE max_size = x->max_partition_size;

#if CONFIG_FP_MB_STATS
  unsigned int src_diff_var = UINT_MAX;
  int none_complexity = 0;
#endif

  int partition_none_allowed = !force_horz_split && !force_vert_split;
  int partition_horz_allowed =
      !force_vert_split && yss <= xss && bsize_at_least_8x8;
  int partition_vert_allowed =
      !force_horz_split && xss <= yss && bsize_at_least_8x8;

#if CONFIG_PVQ
  od_rollback_buffer pre_rdo_buf;
#endif

  (void)*tp_orig;

#if !CONFIG_UNPOISON_PARTITION_CTX
  if (force_horz_split || force_vert_split) {
    tmp_partition_cost[PARTITION_NONE] = INT_MAX;

    if (!force_vert_split) {  // force_horz_split only
      tmp_partition_cost[PARTITION_VERT] = INT_MAX;
      tmp_partition_cost[PARTITION_HORZ] =
          av1_cost_bit(cm->fc->partition_prob[pl][PARTITION_HORZ], 0);
      tmp_partition_cost[PARTITION_SPLIT] =
          av1_cost_bit(cm->fc->partition_prob[pl][PARTITION_HORZ], 1);
    } else if (!force_horz_split) {  // force_vert_split only
      tmp_partition_cost[PARTITION_HORZ] = INT_MAX;
      tmp_partition_cost[PARTITION_VERT] =
          av1_cost_bit(cm->fc->partition_prob[pl][PARTITION_VERT], 0);
      tmp_partition_cost[PARTITION_SPLIT] =
          av1_cost_bit(cm->fc->partition_prob[pl][PARTITION_VERT], 1);
    } else {  // force_ horz_split && force_vert_split horz_split
      tmp_partition_cost[PARTITION_HORZ] = INT_MAX;
      tmp_partition_cost[PARTITION_VERT] = INT_MAX;
      tmp_partition_cost[PARTITION_SPLIT] = 0;
    }

    partition_cost = tmp_partition_cost;
  }
#endif

#if CONFIG_VAR_TX
#ifndef NDEBUG
  // Nothing should rely on the default value of this array (which is just
  // leftover from encoding the previous block. Setting it to magic number
  // when debugging.
  memset(x->blk_skip[0], 234, sizeof(x->blk_skip[0]));
#endif  // NDEBUG
#endif  // CONFIG_VAR_TX

  assert(mi_size_wide[bsize] == mi_size_high[bsize]);

  av1_init_rd_stats(&this_rdc);
  av1_init_rd_stats(&sum_rdc);
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
    partition_horz_allowed &= partition_allowed || force_horz_split;
    partition_vert_allowed &= partition_allowed || force_vert_split;
    do_square_split &= bsize > min_size;
  }
  if (cpi->sf.use_square_partition_only) {
    partition_horz_allowed &= force_horz_split;
    partition_vert_allowed &= force_vert_split;
  }

#if CONFIG_VAR_TX
  xd->above_txfm_context =
      cm->above_txfm_context + (mi_col << TX_UNIT_WIDE_LOG2);
  xd->left_txfm_context = xd->left_txfm_context_buffer +
                          ((mi_row & MAX_MIB_MASK) << TX_UNIT_HIGH_LOG2);
#endif
#if !CONFIG_PVQ
  save_context(x, &x_ctx, mi_row, mi_col, bsize);
#else
  save_context(x, &x_ctx, mi_row, mi_col, &pre_rdo_buf, bsize);
#endif

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

  // PARTITION_NONE
  if (partition_none_allowed) {
    rd_pick_sb_modes(cpi, tile_data, x, mi_row, mi_col, &this_rdc,
#if CONFIG_SUPERTX
                     &this_rate_nocoef,
#endif
#if CONFIG_EXT_PARTITION_TYPES
                     PARTITION_NONE,
#endif
                     bsize, ctx_none, best_rdc.rdcost);
    if (this_rdc.rate != INT_MAX) {
      if (bsize_at_least_8x8) {
        const int pt_cost = partition_cost[PARTITION_NONE] < INT_MAX
                                ? partition_cost[PARTITION_NONE]
                                : 0;
        this_rdc.rate += pt_cost;
        this_rdc.rdcost = RDCOST(x->rdmult, this_rdc.rate, this_rdc.dist);
#if CONFIG_SUPERTX
        this_rate_nocoef += pt_cost;
#endif
      }

      if (this_rdc.rdcost < best_rdc.rdcost) {
        // Adjust dist breakout threshold according to the partition size.
        const int64_t dist_breakout_thr =
            cpi->sf.partition_search_breakout_dist_thr >>
            ((2 * (MAX_SB_SIZE_LOG2 - 2)) -
             (b_width_log2_lookup[bsize] + b_height_log2_lookup[bsize]));
        const int rate_breakout_thr =
            cpi->sf.partition_search_breakout_rate_thr *
            num_pels_log2_lookup[bsize];

        best_rdc = this_rdc;
#if CONFIG_SUPERTX
        best_rate_nocoef = this_rate_nocoef;
        assert(best_rate_nocoef >= 0);
#endif
        if (bsize_at_least_8x8) pc_tree->partitioning = PARTITION_NONE;

        // If all y, u, v transform blocks in this partition are skippable, and
        // the dist & rate are within the thresholds, the partition search is
        // terminated for current branch of the partition search tree.
        // The dist & rate thresholds are set to 0 at speed 0 to disable the
        // early termination at that speed.
        if (!x->e_mbd.lossless[xd->mi[0]->mbmi.segment_id] &&
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
#if !CONFIG_PVQ
    restore_context(x, &x_ctx, mi_row, mi_col, bsize);
#else
    restore_context(x, &x_ctx, mi_row, mi_col, &pre_rdo_buf, bsize);
#endif
#if CONFIG_CFL && CONFIG_CHROMA_SUB8X8 && CONFIG_DEBUG
    if (!x->skip_chroma_rd) {
      cfl_clear_sub8x8_val(xd->cfl);
    }
#endif  // CONFIG_CFL && CONFIG_CHROMA_SUB8X8 && CONFIG_DEBUG
  }

  // store estimated motion vector
  if (cpi->sf.adaptive_motion_search) store_pred_mv(x, ctx_none);

#if CONFIG_SUPERTX
  int64_t temp_best_rdcost = INT64_MAX;
#else
  int64_t temp_best_rdcost = best_rdc.rdcost;
#endif

  // PARTITION_SPLIT
  // TODO(jingning): use the motion vectors given by the above search as
  // the starting point of motion search in the following partition type check.
  if (do_square_split) {
    int reached_last_index = 0;
    subsize = get_subsize(bsize, PARTITION_SPLIT);
    if (bsize == BLOCK_8X8 && !unify_bsize) {
      if (cpi->sf.adaptive_pred_interp_filter && partition_none_allowed)
        pc_tree->leaf_split[0]->pred_interp_filter =
            av1_extract_interp_filter(ctx_none->mic.mbmi.interp_filters, 0);

      rd_pick_sb_modes(cpi, tile_data, x, mi_row, mi_col, &sum_rdc,
#if CONFIG_SUPERTX
                       &sum_rate_nocoef,
#endif
#if CONFIG_EXT_PARTITION_TYPES
                       PARTITION_SPLIT,
#endif
                       subsize, pc_tree->leaf_split[0], temp_best_rdcost);
      if (sum_rdc.rate == INT_MAX) {
        sum_rdc.rdcost = INT64_MAX;
#if CONFIG_SUPERTX
        sum_rate_nocoef = INT_MAX;
#endif
      }
#if CONFIG_SUPERTX
      if (supertx_allowed && sum_rdc.rdcost < INT64_MAX) {
        TX_SIZE supertx_size = max_txsize_lookup[bsize];
        const PARTITION_TYPE best_partition = pc_tree->partitioning;

        pc_tree->partitioning = PARTITION_SPLIT;

        sum_rdc.rate += av1_cost_bit(
            cm->fc->supertx_prob[partition_supertx_context_lookup
                                     [PARTITION_SPLIT]][supertx_size],
            0);
        sum_rdc.rdcost = RDCOST(x->rdmult, sum_rdc.rate, sum_rdc.dist);

        if (is_inter_mode(pc_tree->leaf_split[0]->mic.mbmi.mode)) {
          TX_TYPE best_tx = DCT_DCT;
          RD_STATS tmp_rdc;
          av1_init_rd_stats(&tmp_rdc);
          tmp_rdc.rate = sum_rate_nocoef;

          restore_context(x, &x_ctx, mi_row, mi_col, bsize);

          rd_supertx_sb(cpi, td, tile_info, mi_row, mi_col, bsize,
                        &tmp_rdc.rate, &tmp_rdc.dist, &best_tx, pc_tree);

          tmp_rdc.rate += av1_cost_bit(
              cm->fc->supertx_prob[partition_supertx_context_lookup
                                       [PARTITION_SPLIT]][supertx_size],
              1);
          tmp_rdc.rdcost = RDCOST(x->rdmult, tmp_rdc.rate, tmp_rdc.dist);
          if (tmp_rdc.rdcost < sum_rdc.rdcost) {
            sum_rdc = tmp_rdc;
            update_supertx_param_sb(cpi, td, mi_row, mi_col, bsize, best_tx,
                                    supertx_size, pc_tree);
          }
        }

        pc_tree->partitioning = best_partition;
      }
#endif  // CONFIG_SUPERTX
      reached_last_index = 1;
    } else {
      int idx;
      for (idx = 0; idx < 4 && sum_rdc.rdcost < temp_best_rdcost; ++idx) {
        const int x_idx = (idx & 1) * mi_step;
        const int y_idx = (idx >> 1) * mi_step;

        if (mi_row + y_idx >= cm->mi_rows || mi_col + x_idx >= cm->mi_cols)
          continue;

        if (cpi->sf.adaptive_motion_search) load_pred_mv(x, ctx_none);

        pc_tree->split[idx]->index = idx;
        rd_pick_partition(cpi, td, tile_data, tp, mi_row + y_idx,
                          mi_col + x_idx, subsize, &this_rdc,
#if CONFIG_SUPERTX
                          &this_rate_nocoef,
#endif
                          temp_best_rdcost - sum_rdc.rdcost,
                          pc_tree->split[idx]);

        if (this_rdc.rate == INT_MAX) {
          sum_rdc.rdcost = INT64_MAX;
#if CONFIG_SUPERTX
          sum_rate_nocoef = INT_MAX;
#endif  // CONFIG_SUPERTX
          break;
        } else {
          sum_rdc.rate += this_rdc.rate;
          sum_rdc.dist += this_rdc.dist;
          sum_rdc.rdcost += this_rdc.rdcost;
#if CONFIG_SUPERTX
          sum_rate_nocoef += this_rate_nocoef;
#endif  // CONFIG_SUPERTX
        }
      }
      reached_last_index = (idx == 4);

#if CONFIG_DIST_8X8 && CONFIG_CB4X4
      if (x->using_dist_8x8 && reached_last_index &&
          sum_rdc.rdcost != INT64_MAX && bsize == BLOCK_8X8) {
        const int src_stride = x->plane[0].src.stride;
        int64_t dist_8x8;
        dist_8x8 =
            dist_8x8_yuv(cpi, x, x->plane[0].src.buf - 4 * src_stride - 4);
        sum_rdc.dist = dist_8x8;
        sum_rdc.rdcost = RDCOST(x->rdmult, sum_rdc.rate, sum_rdc.dist);
      }
#endif  // CONFIG_DIST_8X8 && CONFIG_CB4X4

#if CONFIG_SUPERTX
      if (supertx_allowed && sum_rdc.rdcost < INT64_MAX && reached_last_index) {
        TX_SIZE supertx_size = max_txsize_lookup[bsize];
        const PARTITION_TYPE best_partition = pc_tree->partitioning;

        pc_tree->partitioning = PARTITION_SPLIT;

        sum_rdc.rate += av1_cost_bit(
            cm->fc->supertx_prob[partition_supertx_context_lookup
                                     [PARTITION_SPLIT]][supertx_size],
            0);
        sum_rdc.rdcost = RDCOST(x->rdmult, sum_rdc.rate, sum_rdc.dist);

        if (!check_intra_sb(cpi, tile_info, mi_row, mi_col, bsize, pc_tree)) {
          TX_TYPE best_tx = DCT_DCT;
          RD_STATS tmp_rdc;
          av1_init_rd_stats(&tmp_rdc);
          tmp_rdc.rate = sum_rate_nocoef;

          restore_context(x, &x_ctx, mi_row, mi_col, bsize);

          rd_supertx_sb(cpi, td, tile_info, mi_row, mi_col, bsize,
                        &tmp_rdc.rate, &tmp_rdc.dist, &best_tx, pc_tree);

          tmp_rdc.rate += av1_cost_bit(
              cm->fc->supertx_prob[partition_supertx_context_lookup
                                       [PARTITION_SPLIT]][supertx_size],
              1);
          tmp_rdc.rdcost = RDCOST(x->rdmult, tmp_rdc.rate, tmp_rdc.dist);
          if (tmp_rdc.rdcost < sum_rdc.rdcost) {
            sum_rdc = tmp_rdc;
            update_supertx_param_sb(cpi, td, mi_row, mi_col, bsize, best_tx,
                                    supertx_size, pc_tree);
          }
        }

        pc_tree->partitioning = best_partition;
      }
#endif  // CONFIG_SUPERTX
    }

#if CONFIG_CFL && CONFIG_CHROMA_SUB8X8 && CONFIG_DEBUG
    if (!reached_last_index && sum_rdc.rdcost >= best_rdc.rdcost)
      cfl_clear_sub8x8_val(xd->cfl);
#endif  // CONFIG_CFL && CONFIG_CHROMA_SUB8X8 && CONFIG_DEBUG

    if (reached_last_index && sum_rdc.rdcost < best_rdc.rdcost) {
      sum_rdc.rate += partition_cost[PARTITION_SPLIT];
      sum_rdc.rdcost = RDCOST(x->rdmult, sum_rdc.rate, sum_rdc.dist);
#if CONFIG_SUPERTX
      sum_rate_nocoef += partition_cost[PARTITION_SPLIT];
#endif  // CONFIG_SUPERTX

      if (sum_rdc.rdcost < best_rdc.rdcost) {
        best_rdc = sum_rdc;
#if CONFIG_SUPERTX
        best_rate_nocoef = sum_rate_nocoef;
        assert(best_rate_nocoef >= 0);
#else
        temp_best_rdcost = best_rdc.rdcost;
#endif  // CONFIG_SUPERTX
        pc_tree->partitioning = PARTITION_SPLIT;
      }
    } else if (cpi->sf.less_rectangular_check) {
      // skip rectangular partition test when larger block size
      // gives better rd cost
      do_rectangular_split &= !partition_none_allowed;
    }
#if !CONFIG_PVQ
    restore_context(x, &x_ctx, mi_row, mi_col, bsize);
#else
    restore_context(x, &x_ctx, mi_row, mi_col, &pre_rdo_buf, bsize);
#endif
  }  // if (do_split)

  // PARTITION_HORZ
  if (partition_horz_allowed &&
      (do_rectangular_split || av1_active_h_edge(cpi, mi_row, mi_step))) {
    subsize = get_subsize(bsize, PARTITION_HORZ);
    if (cpi->sf.adaptive_motion_search) load_pred_mv(x, ctx_none);
    if (cpi->sf.adaptive_pred_interp_filter && bsize == BLOCK_8X8 &&
        partition_none_allowed)
      pc_tree->horizontal[0].pred_interp_filter =
          av1_extract_interp_filter(ctx_none->mic.mbmi.interp_filters, 0);

    rd_pick_sb_modes(cpi, tile_data, x, mi_row, mi_col, &sum_rdc,
#if CONFIG_SUPERTX
                     &sum_rate_nocoef,
#endif  // CONFIG_SUPERTX
#if CONFIG_EXT_PARTITION_TYPES
                     PARTITION_HORZ,
#endif
                     subsize, &pc_tree->horizontal[0], best_rdc.rdcost);

#if CONFIG_SUPERTX
    abort_flag =
        (sum_rdc.rdcost >= best_rd && (bsize > BLOCK_8X8 || unify_bsize)) ||
        (sum_rdc.rate == INT_MAX && bsize == BLOCK_8X8);
#endif
    if (sum_rdc.rdcost < temp_best_rdcost && !force_horz_split &&
        (bsize > BLOCK_8X8 || unify_bsize)) {
      PICK_MODE_CONTEXT *ctx_h = &pc_tree->horizontal[0];
      update_state(cpi, td, ctx_h, mi_row, mi_col, subsize, 1);
      encode_superblock(cpi, td, tp, DRY_RUN_NORMAL, mi_row, mi_col, subsize,
                        NULL);

      if (cpi->sf.adaptive_motion_search) load_pred_mv(x, ctx_h);

      if (cpi->sf.adaptive_pred_interp_filter && bsize == BLOCK_8X8 &&
          partition_none_allowed)
        pc_tree->horizontal[1].pred_interp_filter =
            av1_extract_interp_filter(ctx_h->mic.mbmi.interp_filters, 0);

#if CONFIG_SUPERTX
      rd_pick_sb_modes(cpi, tile_data, x, mi_row + mi_step, mi_col, &this_rdc,
                       &this_rate_nocoef,
#if CONFIG_EXT_PARTITION_TYPES
                       PARTITION_HORZ,
#endif
                       subsize, &pc_tree->horizontal[1], INT64_MAX);
#else
      rd_pick_sb_modes(cpi, tile_data, x, mi_row + mi_step, mi_col, &this_rdc,
#if CONFIG_EXT_PARTITION_TYPES
                       PARTITION_HORZ,
#endif
                       subsize, &pc_tree->horizontal[1],
                       best_rdc.rdcost - sum_rdc.rdcost);
#endif  // CONFIG_SUPERTX

#if CONFIG_DIST_8X8 && CONFIG_CB4X4
      if (x->using_dist_8x8 && this_rdc.rate != INT_MAX && bsize == BLOCK_8X8) {
        update_state(cpi, td, &pc_tree->horizontal[1], mi_row + mi_step, mi_col,
                     subsize, DRY_RUN_NORMAL);
        encode_superblock(cpi, td, tp, DRY_RUN_NORMAL, mi_row + mi_step, mi_col,
                          subsize, NULL);
      }
#endif  // CONFIG_DIST_8X8 && CONFIG_CB4X4

      if (this_rdc.rate == INT_MAX) {
        sum_rdc.rdcost = INT64_MAX;
#if CONFIG_SUPERTX
        sum_rate_nocoef = INT_MAX;
#endif  // CONFIG_SUPERTX
      } else {
        sum_rdc.rate += this_rdc.rate;
        sum_rdc.dist += this_rdc.dist;
        sum_rdc.rdcost += this_rdc.rdcost;
#if CONFIG_SUPERTX
        sum_rate_nocoef += this_rate_nocoef;
#endif  // CONFIG_SUPERTX
      }
#if CONFIG_DIST_8X8 && CONFIG_CB4X4
      if (x->using_dist_8x8 && sum_rdc.rdcost != INT64_MAX &&
          bsize == BLOCK_8X8) {
        const int src_stride = x->plane[0].src.stride;
        int64_t dist_8x8;
        dist_8x8 = dist_8x8_yuv(cpi, x, x->plane[0].src.buf - 4 * src_stride);
        sum_rdc.dist = dist_8x8;
        sum_rdc.rdcost = RDCOST(x->rdmult, sum_rdc.rate, sum_rdc.dist);
      }
#endif  // CONFIG_DIST_8X8 && CONFIG_CB4X4
    }

#if CONFIG_SUPERTX
    if (supertx_allowed && sum_rdc.rdcost < INT64_MAX && !abort_flag) {
      TX_SIZE supertx_size = max_txsize_lookup[bsize];
      const PARTITION_TYPE best_partition = pc_tree->partitioning;

      pc_tree->partitioning = PARTITION_HORZ;

      sum_rdc.rate += av1_cost_bit(
          cm->fc->supertx_prob[partition_supertx_context_lookup[PARTITION_HORZ]]
                              [supertx_size],
          0);
      sum_rdc.rdcost = RDCOST(x->rdmult, sum_rdc.rate, sum_rdc.dist);

      if (!check_intra_sb(cpi, tile_info, mi_row, mi_col, bsize, pc_tree)) {
        TX_TYPE best_tx = DCT_DCT;
        RD_STATS tmp_rdc;
        av1_init_rd_stats(&tmp_rdc);
        tmp_rdc.rate = sum_rate_nocoef;

        restore_context(x, &x_ctx, mi_row, mi_col, bsize);

        rd_supertx_sb(cpi, td, tile_info, mi_row, mi_col, bsize, &tmp_rdc.rate,
                      &tmp_rdc.dist, &best_tx, pc_tree);

        tmp_rdc.rate += av1_cost_bit(
            cm->fc
                ->supertx_prob[partition_supertx_context_lookup[PARTITION_HORZ]]
                              [supertx_size],
            1);
        tmp_rdc.rdcost = RDCOST(x->rdmult, tmp_rdc.rate, tmp_rdc.dist);
        if (tmp_rdc.rdcost < sum_rdc.rdcost) {
          sum_rdc = tmp_rdc;
          update_supertx_param_sb(cpi, td, mi_row, mi_col, bsize, best_tx,
                                  supertx_size, pc_tree);
        }
      }

      pc_tree->partitioning = best_partition;
    }
#endif  // CONFIG_SUPERTX

#if CONFIG_CFL && CONFIG_CHROMA_SUB8X8 && CONFIG_DEBUG
    cfl_clear_sub8x8_val(xd->cfl);
#endif  // CONFIG_CFL && CONFIG_CHROMA_SUB8X8 && CONFIG_DEBUG
    if (sum_rdc.rdcost < best_rdc.rdcost) {
      sum_rdc.rate += partition_cost[PARTITION_HORZ];
      sum_rdc.rdcost = RDCOST(x->rdmult, sum_rdc.rate, sum_rdc.dist);
#if CONFIG_SUPERTX
      sum_rate_nocoef += partition_cost[PARTITION_HORZ];
#endif  // CONFIG_SUPERTX
      if (sum_rdc.rdcost < best_rdc.rdcost) {
        best_rdc = sum_rdc;
#if CONFIG_SUPERTX
        best_rate_nocoef = sum_rate_nocoef;
        assert(best_rate_nocoef >= 0);
#endif  // CONFIG_SUPERTX
        pc_tree->partitioning = PARTITION_HORZ;
      }
    }
#if !CONFIG_PVQ
    restore_context(x, &x_ctx, mi_row, mi_col, bsize);
#else
    restore_context(x, &x_ctx, mi_row, mi_col, &pre_rdo_buf, bsize);
#endif
  }

  // PARTITION_VERT
  if (partition_vert_allowed &&
      (do_rectangular_split || av1_active_v_edge(cpi, mi_col, mi_step))) {
    subsize = get_subsize(bsize, PARTITION_VERT);

    if (cpi->sf.adaptive_motion_search) load_pred_mv(x, ctx_none);

    if (cpi->sf.adaptive_pred_interp_filter && bsize == BLOCK_8X8 &&
        partition_none_allowed)
      pc_tree->vertical[0].pred_interp_filter =
          av1_extract_interp_filter(ctx_none->mic.mbmi.interp_filters, 0);

    rd_pick_sb_modes(cpi, tile_data, x, mi_row, mi_col, &sum_rdc,
#if CONFIG_SUPERTX
                     &sum_rate_nocoef,
#endif  // CONFIG_SUPERTX
#if CONFIG_EXT_PARTITION_TYPES
                     PARTITION_VERT,
#endif
                     subsize, &pc_tree->vertical[0], best_rdc.rdcost);
#if CONFIG_SUPERTX
    abort_flag =
        (sum_rdc.rdcost >= best_rd && (bsize > BLOCK_8X8 || unify_bsize)) ||
        (sum_rdc.rate == INT_MAX && bsize == BLOCK_8X8);
    const int64_t vert_max_rdcost = INT64_MAX;
#else
    const int64_t vert_max_rdcost = best_rdc.rdcost;
#endif  // CONFIG_SUPERTX
    if (sum_rdc.rdcost < vert_max_rdcost && !force_vert_split &&
        (bsize > BLOCK_8X8 || unify_bsize)) {
      update_state(cpi, td, &pc_tree->vertical[0], mi_row, mi_col, subsize, 1);
      encode_superblock(cpi, td, tp, DRY_RUN_NORMAL, mi_row, mi_col, subsize,
                        NULL);

      if (cpi->sf.adaptive_motion_search) load_pred_mv(x, ctx_none);

      if (cpi->sf.adaptive_pred_interp_filter && bsize == BLOCK_8X8 &&
          partition_none_allowed)
        pc_tree->vertical[1].pred_interp_filter =
            av1_extract_interp_filter(ctx_none->mic.mbmi.interp_filters, 0);

#if CONFIG_SUPERTX
      rd_pick_sb_modes(cpi, tile_data, x, mi_row, mi_col + mi_step, &this_rdc,
                       &this_rate_nocoef,
#if CONFIG_EXT_PARTITION_TYPES
                       PARTITION_VERT,
#endif
                       subsize, &pc_tree->vertical[1],
                       INT64_MAX - sum_rdc.rdcost);
#else
      rd_pick_sb_modes(cpi, tile_data, x, mi_row, mi_col + mi_step, &this_rdc,
#if CONFIG_EXT_PARTITION_TYPES
                       PARTITION_VERT,
#endif
                       subsize, &pc_tree->vertical[1],
                       best_rdc.rdcost - sum_rdc.rdcost);
#endif  // CONFIG_SUPERTX

#if CONFIG_DIST_8X8 && CONFIG_CB4X4
      if (x->using_dist_8x8 && this_rdc.rate != INT_MAX && bsize == BLOCK_8X8) {
        update_state(cpi, td, &pc_tree->vertical[1], mi_row, mi_col + mi_step,
                     subsize, DRY_RUN_NORMAL);
        encode_superblock(cpi, td, tp, DRY_RUN_NORMAL, mi_row, mi_col + mi_step,
                          subsize, NULL);
      }
#endif  // CONFIG_DIST_8X8 && CONFIG_CB4X4

      if (this_rdc.rate == INT_MAX) {
        sum_rdc.rdcost = INT64_MAX;
#if CONFIG_SUPERTX
        sum_rate_nocoef = INT_MAX;
#endif  // CONFIG_SUPERTX
      } else {
        sum_rdc.rate += this_rdc.rate;
        sum_rdc.dist += this_rdc.dist;
        sum_rdc.rdcost += this_rdc.rdcost;
#if CONFIG_SUPERTX
        sum_rate_nocoef += this_rate_nocoef;
#endif  // CONFIG_SUPERTX
      }
#if CONFIG_DIST_8X8 && CONFIG_CB4X4
      if (x->using_dist_8x8 && sum_rdc.rdcost != INT64_MAX &&
          bsize == BLOCK_8X8) {
        int64_t dist_8x8;
        dist_8x8 = dist_8x8_yuv(cpi, x, x->plane[0].src.buf - 4);
        sum_rdc.dist = dist_8x8;
        sum_rdc.rdcost = RDCOST(x->rdmult, sum_rdc.rate, sum_rdc.dist);
      }
#endif  // CONFIG_DIST_8X8 && CONFIG_CB4X4
    }
#if CONFIG_SUPERTX
    if (supertx_allowed && sum_rdc.rdcost < INT64_MAX && !abort_flag) {
      TX_SIZE supertx_size = max_txsize_lookup[bsize];
      const PARTITION_TYPE best_partition = pc_tree->partitioning;

      pc_tree->partitioning = PARTITION_VERT;

      sum_rdc.rate += av1_cost_bit(
          cm->fc->supertx_prob[partition_supertx_context_lookup[PARTITION_VERT]]
                              [supertx_size],
          0);
      sum_rdc.rdcost = RDCOST(x->rdmult, sum_rdc.rate, sum_rdc.dist);

      if (!check_intra_sb(cpi, tile_info, mi_row, mi_col, bsize, pc_tree)) {
        TX_TYPE best_tx = DCT_DCT;
        RD_STATS tmp_rdc;
        av1_init_rd_stats(&tmp_rdc);
        tmp_rdc.rate = sum_rate_nocoef;

        restore_context(x, &x_ctx, mi_row, mi_col, bsize);

        rd_supertx_sb(cpi, td, tile_info, mi_row, mi_col, bsize, &tmp_rdc.rate,
                      &tmp_rdc.dist, &best_tx, pc_tree);

        tmp_rdc.rate += av1_cost_bit(
            cm->fc
                ->supertx_prob[partition_supertx_context_lookup[PARTITION_VERT]]
                              [supertx_size],
            1);
        tmp_rdc.rdcost = RDCOST(x->rdmult, tmp_rdc.rate, tmp_rdc.dist);
        if (tmp_rdc.rdcost < sum_rdc.rdcost) {
          sum_rdc = tmp_rdc;
          update_supertx_param_sb(cpi, td, mi_row, mi_col, bsize, best_tx,
                                  supertx_size, pc_tree);
        }
      }

      pc_tree->partitioning = best_partition;
    }
#endif  // CONFIG_SUPERTX

#if CONFIG_CFL && CONFIG_CHROMA_SUB8X8 && CONFIG_DEBUG
    cfl_clear_sub8x8_val(xd->cfl);
#endif  // CONFIG_CFL && CONFIG_CHROMA_SUB8X8 && CONFIG_DEBUG

    if (sum_rdc.rdcost < best_rdc.rdcost) {
      sum_rdc.rate += partition_cost[PARTITION_VERT];
      sum_rdc.rdcost = RDCOST(x->rdmult, sum_rdc.rate, sum_rdc.dist);
#if CONFIG_SUPERTX
      sum_rate_nocoef += partition_cost[PARTITION_VERT];
#endif  // CONFIG_SUPERTX
      if (sum_rdc.rdcost < best_rdc.rdcost) {
        best_rdc = sum_rdc;
#if CONFIG_SUPERTX
        best_rate_nocoef = sum_rate_nocoef;
        assert(best_rate_nocoef >= 0);
#endif  // CONFIG_SUPERTX
        pc_tree->partitioning = PARTITION_VERT;
      }
    }
#if !CONFIG_PVQ
    restore_context(x, &x_ctx, mi_row, mi_col, bsize);
#else
    restore_context(x, &x_ctx, mi_row, mi_col, &pre_rdo_buf, bsize);
#endif
  }

#if CONFIG_EXT_PARTITION_TYPES
  const int ext_partition_allowed =
      do_rectangular_split && bsize > BLOCK_8X8 && partition_none_allowed;

#if CONFIG_EXT_PARTITION && CONFIG_EXT_PARTITION_TYPES_AB
  // Don't allow A/B partitions on 128x128 blocks for now (support for
  // 128x32 and 32x128 blocks doesn't yet exist).
  const int ab_partition_allowed =
      ext_partition_allowed && bsize < BLOCK_128X128;
#else
  const int ab_partition_allowed = ext_partition_allowed;
#endif

  // PARTITION_HORZ_A
  if (partition_horz_allowed && ab_partition_allowed) {
#if CONFIG_EXT_PARTITION_TYPES_AB
    rd_test_partition3(
        cpi, td, tile_data, tp, pc_tree, &best_rdc, pc_tree->horizontala,
        ctx_none, mi_row, mi_col, bsize, PARTITION_HORZ_A,
#if CONFIG_SUPERTX
        best_rd, &best_rate_nocoef, &x_ctx,
#endif
        mi_row, mi_col, get_subsize(bsize, PARTITION_HORZ_4),
        mi_row + mi_step / 2, mi_col, get_subsize(bsize, PARTITION_HORZ_4),
        mi_row + mi_step, mi_col, get_subsize(bsize, PARTITION_HORZ));
#else
    subsize = get_subsize(bsize, PARTITION_HORZ_A);
    rd_test_partition3(cpi, td, tile_data, tp, pc_tree, &best_rdc,
                       pc_tree->horizontala, ctx_none, mi_row, mi_col, bsize,
                       PARTITION_HORZ_A,
#if CONFIG_SUPERTX
                       best_rd, &best_rate_nocoef, &x_ctx,
#endif
                       mi_row, mi_col, bsize2, mi_row, mi_col + mi_step, bsize2,
                       mi_row + mi_step, mi_col, subsize);
#endif
#if !CONFIG_PVQ
    restore_context(x, &x_ctx, mi_row, mi_col, bsize);
#else
    restore_context(x, &x_ctx, mi_row, mi_col, &pre_rdo_buf, bsize);
#endif  // !CONFIG_PVQ
  }
  // PARTITION_HORZ_B
  if (partition_horz_allowed && ab_partition_allowed) {
#if CONFIG_EXT_PARTITION_TYPES_AB
    rd_test_partition3(
        cpi, td, tile_data, tp, pc_tree, &best_rdc, pc_tree->horizontalb,
        ctx_none, mi_row, mi_col, bsize, PARTITION_HORZ_B,
#if CONFIG_SUPERTX
        best_rd, &best_rate_nocoef, &x_ctx,
#endif
        mi_row, mi_col, get_subsize(bsize, PARTITION_HORZ), mi_row + mi_step,
        mi_col, get_subsize(bsize, PARTITION_HORZ_4), mi_row + 3 * mi_step / 2,
        mi_col, get_subsize(bsize, PARTITION_HORZ_4));
#else
    subsize = get_subsize(bsize, PARTITION_HORZ_B);
    rd_test_partition3(cpi, td, tile_data, tp, pc_tree, &best_rdc,
                       pc_tree->horizontalb, ctx_none, mi_row, mi_col, bsize,
                       PARTITION_HORZ_B,
#if CONFIG_SUPERTX
                       best_rd, &best_rate_nocoef, &x_ctx,
#endif
                       mi_row, mi_col, subsize, mi_row + mi_step, mi_col,
                       bsize2, mi_row + mi_step, mi_col + mi_step, bsize2);
#endif
#if !CONFIG_PVQ
    restore_context(x, &x_ctx, mi_row, mi_col, bsize);
#else
    restore_context(x, &x_ctx, mi_row, mi_col, &pre_rdo_buf, bsize);
#endif  // !CONFIG_PVQ
  }
  // PARTITION_VERT_A
  if (partition_vert_allowed && ab_partition_allowed) {
#if CONFIG_EXT_PARTITION_TYPES_AB
    rd_test_partition3(
        cpi, td, tile_data, tp, pc_tree, &best_rdc, pc_tree->verticala,
        ctx_none, mi_row, mi_col, bsize, PARTITION_VERT_A,
#if CONFIG_SUPERTX
        best_rd, &best_rate_nocoef, &x_ctx,
#endif
        mi_row, mi_col, get_subsize(bsize, PARTITION_VERT_4), mi_row,
        mi_col + mi_step / 2, get_subsize(bsize, PARTITION_VERT_4), mi_row,
        mi_col + mi_step, get_subsize(bsize, PARTITION_VERT));
#else
    subsize = get_subsize(bsize, PARTITION_VERT_A);
    rd_test_partition3(cpi, td, tile_data, tp, pc_tree, &best_rdc,
                       pc_tree->verticala, ctx_none, mi_row, mi_col, bsize,
                       PARTITION_VERT_A,
#if CONFIG_SUPERTX
                       best_rd, &best_rate_nocoef, &x_ctx,
#endif
                       mi_row, mi_col, bsize2, mi_row + mi_step, mi_col, bsize2,
                       mi_row, mi_col + mi_step, subsize);
#endif
#if !CONFIG_PVQ
    restore_context(x, &x_ctx, mi_row, mi_col, bsize);
#else
    restore_context(x, &x_ctx, mi_row, mi_col, &pre_rdo_buf, bsize);
#endif  // !CONFIG_PVQ
  }
  // PARTITION_VERT_B
  if (partition_vert_allowed && ab_partition_allowed) {
#if CONFIG_EXT_PARTITION_TYPES_AB
    rd_test_partition3(
        cpi, td, tile_data, tp, pc_tree, &best_rdc, pc_tree->verticalb,
        ctx_none, mi_row, mi_col, bsize, PARTITION_VERT_B,
#if CONFIG_SUPERTX
        best_rd, &best_rate_nocoef, &x_ctx,
#endif
        mi_row, mi_col, get_subsize(bsize, PARTITION_VERT), mi_row,
        mi_col + mi_step, get_subsize(bsize, PARTITION_VERT_4), mi_row,
        mi_col + 3 * mi_step / 2, get_subsize(bsize, PARTITION_VERT_4));
#else
    subsize = get_subsize(bsize, PARTITION_VERT_B);
    rd_test_partition3(cpi, td, tile_data, tp, pc_tree, &best_rdc,
                       pc_tree->verticalb, ctx_none, mi_row, mi_col, bsize,
                       PARTITION_VERT_B,
#if CONFIG_SUPERTX
                       best_rd, &best_rate_nocoef, &x_ctx,
#endif
                       mi_row, mi_col, subsize, mi_row, mi_col + mi_step,
                       bsize2, mi_row + mi_step, mi_col + mi_step, bsize2);
#endif
#if !CONFIG_PVQ
    restore_context(x, &x_ctx, mi_row, mi_col, bsize);
#else
    restore_context(x, &x_ctx, mi_row, mi_col, &pre_rdo_buf, bsize);
#endif  // !CONFIG_PVQ
  }

#if CONFIG_EXT_PARTITION
  const int can_partition_4 = (bsize == BLOCK_128X128 || bsize == BLOCK_64X64 ||
                               bsize == BLOCK_32X32 || bsize == BLOCK_16X16);
#else
  const int can_partition_4 =
      (bsize == BLOCK_64X64 || bsize == BLOCK_32X32 || bsize == BLOCK_16X16);
#endif  // CONFIG_EXT_PARTITION

  // PARTITION_HORZ_4
  // TODO(david.barker): For this and PARTITION_VERT_4,
  // * Add support for BLOCK_16X16 once we support 2x8 and 8x2 blocks for the
  //   chroma plane
  // * Add support for supertx
  if (can_partition_4 && partition_horz_allowed && !force_horz_split &&
      (do_rectangular_split || av1_active_h_edge(cpi, mi_row, mi_step))) {
    const int quarter_step = mi_size_high[bsize] / 4;
    PICK_MODE_CONTEXT *ctx_prev = ctx_none;

    subsize = get_subsize(bsize, PARTITION_HORZ_4);

    for (int i = 0; i < 4; ++i) {
      int this_mi_row = mi_row + i * quarter_step;

      if (i > 0 && this_mi_row >= cm->mi_rows) break;

      PICK_MODE_CONTEXT *ctx_this = &pc_tree->horizontal4[i];

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
#if !CONFIG_PVQ
    restore_context(x, &x_ctx, mi_row, mi_col, bsize);
#else
    restore_context(x, &x_ctx, mi_row, mi_col, &pre_rdo_buf, bsize);
#endif
  }
  // PARTITION_VERT_4
  if (can_partition_4 && partition_vert_allowed && !force_vert_split &&
      (do_rectangular_split || av1_active_v_edge(cpi, mi_row, mi_step))) {
    const int quarter_step = mi_size_wide[bsize] / 4;
    PICK_MODE_CONTEXT *ctx_prev = ctx_none;

    subsize = get_subsize(bsize, PARTITION_VERT_4);

    for (int i = 0; i < 4; ++i) {
      int this_mi_col = mi_col + i * quarter_step;

      if (i > 0 && this_mi_col >= cm->mi_cols) break;

      PICK_MODE_CONTEXT *ctx_this = &pc_tree->vertical4[i];

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
#if !CONFIG_PVQ
    restore_context(x, &x_ctx, mi_row, mi_col, bsize);
#else
    restore_context(x, &x_ctx, mi_row, mi_col, &pre_rdo_buf, bsize);
#endif
  }
#endif  // CONFIG_EXT_PARTITION_TYPES

  // TODO(jbb): This code added so that we avoid static analysis
  // warning related to the fact that best_rd isn't used after this
  // point.  This code should be refactored so that the duplicate
  // checks occur in some sub function and thus are used...
  (void)best_rd;
  *rd_cost = best_rdc;

#if CONFIG_SUPERTX
  *rate_nocoef = best_rate_nocoef;
#endif  // CONFIG_SUPERTX

  if (best_rdc.rate < INT_MAX && best_rdc.dist < INT64_MAX &&
      pc_tree->index != 3) {
    if (bsize == cm->sb_size) {
#if CONFIG_MOTION_VAR && NC_MODE_INFO
      set_mode_info_sb(cpi, td, tile_info, tp, mi_row, mi_col, bsize, pc_tree);
#endif

#if CONFIG_LV_MAP
      x->cb_offset = 0;
#endif

#if CONFIG_NCOBMC_ADAPT_WEIGHT
      set_sb_mi_boundaries(cm, xd, mi_row, mi_col);
#endif
      encode_sb(cpi, td, tile_info, tp, mi_row, mi_col, OUTPUT_ENABLED, bsize,
                pc_tree, NULL);
    } else {
      encode_sb(cpi, td, tile_info, tp, mi_row, mi_col, DRY_RUN_NORMAL, bsize,
                pc_tree, NULL);
    }
  }

#if CONFIG_DIST_8X8 && CONFIG_CB4X4
  if (x->using_dist_8x8 && best_rdc.rate < INT_MAX &&
      best_rdc.dist < INT64_MAX && bsize == BLOCK_4X4 && pc_tree->index == 3) {
    encode_sb(cpi, td, tile_info, tp, mi_row, mi_col, DRY_RUN_NORMAL, bsize,
              pc_tree, NULL);
  }
#endif  // CONFIG_DIST_8X8 && CONFIG_CB4X4

  if (bsize == cm->sb_size) {
#if !CONFIG_PVQ && !CONFIG_LV_MAP
    assert(tp_orig < *tp || (tp_orig == *tp && xd->mi[0]->mbmi.skip));
#endif
    assert(best_rdc.rate < INT_MAX);
    assert(best_rdc.dist < INT64_MAX);
  } else {
    assert(tp_orig == *tp);
  }
}

static void encode_rd_sb_row(AV1_COMP *cpi, ThreadData *td,
                             TileDataEnc *tile_data, int mi_row,
                             TOKENEXTRA **tp) {
  AV1_COMMON *const cm = &cpi->common;
  const TileInfo *const tile_info = &tile_data->tile_info;
  MACROBLOCK *const x = &td->mb;
  MACROBLOCKD *const xd = &x->e_mbd;
  SPEED_FEATURES *const sf = &cpi->sf;
  int mi_col;
#if CONFIG_EXT_PARTITION
  const int leaf_nodes = 256;
#else
  const int leaf_nodes = 64;
#endif  // CONFIG_EXT_PARTITION

  // Initialize the left context for the new SB row
  av1_zero_left_context(xd);

  // Reset delta for every tile
  if (cm->delta_q_present_flag)
    if (mi_row == tile_info->mi_row_start) xd->prev_qindex = cm->base_qindex;
#if CONFIG_EXT_DELTA_Q
  if (cm->delta_lf_present_flag) {
#if CONFIG_LOOPFILTER_LEVEL
    if (mi_row == tile_info->mi_row_start)
      for (int lf_id = 0; lf_id < FRAME_LF_COUNT; ++lf_id)
        xd->prev_delta_lf[lf_id] = 0;
#endif  // CONFIG_LOOPFILTER_LEVEL
    if (mi_row == tile_info->mi_row_start) xd->prev_delta_lf_from_base = 0;
  }
#endif

  // Code each SB in the row
  for (mi_col = tile_info->mi_col_start; mi_col < tile_info->mi_col_end;
       mi_col += cm->mib_size) {
    const struct segmentation *const seg = &cm->seg;
    int dummy_rate;
    int64_t dummy_dist;
    RD_STATS dummy_rdc;
#if CONFIG_SUPERTX
    int dummy_rate_nocoef;
#endif  // CONFIG_SUPERTX
    int i;
    int seg_skip = 0;

    const int idx_str = cm->mi_stride * mi_row + mi_col;
    MODE_INFO **mi = cm->mi_grid_visible + idx_str;
    PC_TREE *const pc_root = td->pc_root[cm->mib_size_log2 - MIN_MIB_SIZE_LOG2];

#if CONFIG_LV_MAP && LV_MAP_PROB
    av1_fill_coeff_costs(&td->mb, xd->tile_ctx);
#else
    av1_fill_token_costs_from_cdf(x->token_head_costs,
                                  x->e_mbd.tile_ctx->coef_head_cdfs);
    av1_fill_token_costs_from_cdf(x->token_tail_costs,
                                  x->e_mbd.tile_ctx->coef_tail_cdfs);
#endif
    av1_fill_mode_rates(cm, x, xd->tile_ctx);

    if (sf->adaptive_pred_interp_filter) {
#if !CONFIG_CB4X4
      for (i = 0; i < leaf_nodes; ++i)
        td->leaf_tree[i].pred_interp_filter = SWITCHABLE;
#endif

      for (i = 0; i < leaf_nodes; ++i) {
        td->pc_tree[i].vertical[0].pred_interp_filter = SWITCHABLE;
        td->pc_tree[i].vertical[1].pred_interp_filter = SWITCHABLE;
        td->pc_tree[i].horizontal[0].pred_interp_filter = SWITCHABLE;
        td->pc_tree[i].horizontal[1].pred_interp_filter = SWITCHABLE;
      }
    }

    x->tx_rd_record.num = x->tx_rd_record.index_start = 0;
    av1_zero(x->pred_mv);
    pc_root->index = 0;

    if (seg->enabled) {
      const uint8_t *const map =
          seg->update_map ? cpi->segmentation_map : cm->last_frame_seg_map;
      int segment_id = get_segment_id(cm, map, cm->sb_size, mi_row, mi_col);
      seg_skip = segfeature_active(seg, segment_id, SEG_LVL_SKIP);
    }
#if CONFIG_AMVR
    xd->cur_frame_mv_precision_level = cm->cur_frame_mv_precision_level;
#endif

    if (cm->delta_q_present_flag) {
      // Test mode for delta quantization
      int sb_row = mi_row >> 3;
      int sb_col = mi_col >> 3;
      int sb_stride = (cm->width + MAX_SB_SIZE - 1) >> MAX_SB_SIZE_LOG2;
      int index = ((sb_row * sb_stride + sb_col + 8) & 31) - 16;

      // Ensure divisibility of delta_qindex by delta_q_res
      int offset_qindex = (index < 0 ? -index - 8 : index - 8);
      int qmask = ~(cm->delta_q_res - 1);
      int current_qindex = clamp(cm->base_qindex + offset_qindex,
                                 cm->delta_q_res, 256 - cm->delta_q_res);

      current_qindex =
          ((current_qindex - cm->base_qindex + cm->delta_q_res / 2) & qmask) +
          cm->base_qindex;
      assert(current_qindex > 0);

      xd->delta_qindex = current_qindex - cm->base_qindex;
      set_offsets(cpi, tile_info, x, mi_row, mi_col, cm->sb_size);
      xd->mi[0]->mbmi.current_q_index = current_qindex;
#if !CONFIG_EXT_DELTA_Q
      xd->mi[0]->mbmi.segment_id = 0;
#endif  // CONFIG_EXT_DELTA_Q
      av1_init_plane_quantizers(cpi, x, xd->mi[0]->mbmi.segment_id);
#if CONFIG_EXT_DELTA_Q
      if (cpi->oxcf.deltaq_mode == DELTA_Q_LF) {
        int j, k;
        int lfmask = ~(cm->delta_lf_res - 1);
        int current_delta_lf_from_base = offset_qindex / 2;
        current_delta_lf_from_base =
            ((current_delta_lf_from_base + cm->delta_lf_res / 2) & lfmask);

        // pre-set the delta lf for loop filter. Note that this value is set
        // before mi is assigned for each block in current superblock
        for (j = 0; j < AOMMIN(cm->mib_size, cm->mi_rows - mi_row); j++) {
          for (k = 0; k < AOMMIN(cm->mib_size, cm->mi_cols - mi_col); k++) {
            cm->mi[(mi_row + j) * cm->mi_stride + (mi_col + k)]
                .mbmi.current_delta_lf_from_base =
                clamp(current_delta_lf_from_base, 0, MAX_LOOP_FILTER);
#if CONFIG_LOOPFILTER_LEVEL
            for (int lf_id = 0; lf_id < FRAME_LF_COUNT; ++lf_id) {
              cm->mi[(mi_row + j) * cm->mi_stride + (mi_col + k)]
                  .mbmi.curr_delta_lf[lf_id] = current_delta_lf_from_base;
            }
#endif  // CONFIG_LOOPFILTER_LEVEL
          }
        }
      }
#endif  // CONFIG_EXT_DELTA_Q
    }

    x->source_variance = UINT_MAX;
    if (sf->partition_search_type == FIXED_PARTITION || seg_skip) {
      BLOCK_SIZE bsize;
      set_offsets(cpi, tile_info, x, mi_row, mi_col, cm->sb_size);
      bsize = seg_skip ? cm->sb_size : sf->always_this_block_size;
      set_fixed_partitioning(cpi, tile_info, mi, mi_row, mi_col, bsize);
      rd_use_partition(cpi, td, tile_data, mi, tp, mi_row, mi_col, cm->sb_size,
                       &dummy_rate, &dummy_dist,
#if CONFIG_SUPERTX
                       &dummy_rate_nocoef,
#endif  // CONFIG_SUPERTX
                       1, pc_root);
    } else if (cpi->partition_search_skippable_frame) {
      BLOCK_SIZE bsize;
      set_offsets(cpi, tile_info, x, mi_row, mi_col, cm->sb_size);
      bsize = get_rd_var_based_fixed_partition(cpi, x, mi_row, mi_col);
      set_fixed_partitioning(cpi, tile_info, mi, mi_row, mi_col, bsize);
      rd_use_partition(cpi, td, tile_data, mi, tp, mi_row, mi_col, cm->sb_size,
                       &dummy_rate, &dummy_dist,
#if CONFIG_SUPERTX
                       &dummy_rate_nocoef,
#endif  // CONFIG_SUPERTX
                       1, pc_root);
    } else {
      // If required set upper and lower partition size limits
      if (sf->auto_min_max_partition_size) {
        set_offsets(cpi, tile_info, x, mi_row, mi_col, cm->sb_size);
        rd_auto_partition_range(cpi, tile_info, xd, mi_row, mi_col,
                                &x->min_partition_size, &x->max_partition_size);
      }
      rd_pick_partition(cpi, td, tile_data, tp, mi_row, mi_col, cm->sb_size,
                        &dummy_rdc,
#if CONFIG_SUPERTX
                        &dummy_rate_nocoef,
#endif  // CONFIG_SUPERTX
                        INT64_MAX, pc_root);
    }
  }
}

static void init_encode_frame_mb_context(AV1_COMP *cpi) {
  MACROBLOCK *const x = &cpi->td.mb;
  AV1_COMMON *const cm = &cpi->common;
  MACROBLOCKD *const xd = &x->e_mbd;

  // Copy data over into macro block data structures.
  av1_setup_src_planes(x, cpi->source, 0, 0);

  av1_setup_block_planes(xd, cm->subsampling_x, cm->subsampling_y);
}

#if !CONFIG_REF_ADAPT
static int check_dual_ref_flags(AV1_COMP *cpi) {
  const int ref_flags = cpi->ref_frame_flags;

  if (segfeature_active(&cpi->common.seg, 1, SEG_LVL_REF_FRAME)) {
    return 0;
  } else {
    return (!!(ref_flags & AOM_GOLD_FLAG) + !!(ref_flags & AOM_LAST_FLAG) +
#if CONFIG_EXT_REFS
            !!(ref_flags & AOM_LAST2_FLAG) + !!(ref_flags & AOM_LAST3_FLAG) +
            !!(ref_flags & AOM_BWD_FLAG) + !!(ref_flags & AOM_ALT2_FLAG) +
#endif  // CONFIG_EXT_REFS
            !!(ref_flags & AOM_ALT_FLAG)) >= 2;
  }
}
#endif  // !CONFIG_REF_ADAPT

#if !CONFIG_VAR_TX
static void reset_skip_tx_size(AV1_COMMON *cm, TX_SIZE max_tx_size) {
  int mi_row, mi_col;
  const int mis = cm->mi_stride;
  MODE_INFO **mi_ptr = cm->mi_grid_visible;

  for (mi_row = 0; mi_row < cm->mi_rows; ++mi_row, mi_ptr += mis) {
    for (mi_col = 0; mi_col < cm->mi_cols; ++mi_col) {
      if (txsize_sqr_up_map[mi_ptr[mi_col]->mbmi.tx_size] > max_tx_size)
        mi_ptr[mi_col]->mbmi.tx_size = max_tx_size;
    }
  }
}
#endif

static MV_REFERENCE_FRAME get_frame_type(const AV1_COMP *cpi) {
  if (frame_is_intra_only(&cpi->common)) return INTRA_FRAME;
#if CONFIG_EXT_REFS
  // We will not update the golden frame with an internal overlay frame
  else if ((cpi->rc.is_src_frame_alt_ref && cpi->refresh_golden_frame) ||
           cpi->rc.is_src_frame_ext_arf)
#else
  else if (cpi->rc.is_src_frame_alt_ref && cpi->refresh_golden_frame)
#endif  // CONFIG_EXT_REFS
    return ALTREF_FRAME;
  else if (cpi->refresh_golden_frame ||
#if CONFIG_EXT_REFS
           cpi->refresh_alt2_ref_frame ||
#endif  // CONFIG_EXT_REFS
           cpi->refresh_alt_ref_frame)
    return GOLDEN_FRAME;
  else
    // TODO(zoeliu): To investigate whether a frame_type other than
    // INTRA/ALTREF/GOLDEN/LAST needs to be specified seperately.
    return LAST_FRAME;
}

static TX_MODE select_tx_mode(const AV1_COMP *cpi) {
  if (cpi->common.all_lossless) return ONLY_4X4;
#if CONFIG_VAR_TX_NO_TX_MODE
  return TX_MODE_SELECT;
#else
  if (cpi->sf.tx_size_search_method == USE_LARGESTALL)
    return ALLOW_32X32 + CONFIG_TX64X64;
  else if (cpi->sf.tx_size_search_method == USE_FULL_RD ||
           cpi->sf.tx_size_search_method == USE_TX_8X8)
    return TX_MODE_SELECT;
  else
    return cpi->common.tx_mode;
#endif  // CONFIG_VAR_TX_NO_TX_MODE
}

void av1_init_tile_data(AV1_COMP *cpi) {
  AV1_COMMON *const cm = &cpi->common;
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
#if CONFIG_PVQ
        // This will be dynamically increased as more pvq block is encoded.
        tile_data->pvq_q.buf_len = 1000;
        CHECK_MEM_ERROR(
            cm, tile_data->pvq_q.buf,
            aom_malloc(tile_data->pvq_q.buf_len * sizeof(PVQ_INFO)));
        tile_data->pvq_q.curr_pos = 0;
#endif
      }
  }

  for (tile_row = 0; tile_row < tile_rows; ++tile_row) {
    for (tile_col = 0; tile_col < tile_cols; ++tile_col) {
      TileInfo *const tile_info =
          &cpi->tile_data[tile_row * tile_cols + tile_col].tile_info;
      av1_tile_init(tile_info, cm, tile_row, tile_col);

      cpi->tile_tok[tile_row][tile_col] = pre_tok + tile_tok;
      pre_tok = cpi->tile_tok[tile_row][tile_col];
      tile_tok = allocated_tokens(*tile_info);
#if CONFIG_PVQ
      cpi->tile_data[tile_row * tile_cols + tile_col].pvq_q.curr_pos = 0;
#endif
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

#if CONFIG_DEPENDENT_HORZTILES
  if ((!cm->dependent_horz_tiles) || (tile_row == 0) ||
      tile_info->tg_horz_boundary) {
    av1_zero_above_context(cm, tile_info->mi_col_start, tile_info->mi_col_end);
  }
#else
  av1_zero_above_context(cm, tile_info->mi_col_start, tile_info->mi_col_end);
#endif

  // Set up pointers to per thread motion search counters.
  this_tile->m_search_count = 0;   // Count of motion search hits.
  this_tile->ex_search_count = 0;  // Exhaustive mesh search hits.
  td->mb.m_search_count_ptr = &this_tile->m_search_count;
  td->mb.ex_search_count_ptr = &this_tile->ex_search_count;

#if CONFIG_PVQ
  td->mb.pvq_q = &this_tile->pvq_q;

  // TODO(yushin) : activity masking info needs be signaled by a bitstream
  td->mb.daala_enc.use_activity_masking = AV1_PVQ_ENABLE_ACTIVITY_MASKING;

  if (td->mb.daala_enc.use_activity_masking)
    td->mb.daala_enc.qm = OD_HVS_QM;  // Hard coded. Enc/dec required to sync.
  else
    td->mb.daala_enc.qm = OD_FLAT_QM;  // Hard coded. Enc/dec required to sync.

  {
    // FIXME: Multiple segments support
    int segment_id = 0;
    int rdmult = set_segment_rdmult(cpi, &td->mb, segment_id);
    int qindex = av1_get_qindex(&cm->seg, segment_id, cm->base_qindex);
#if CONFIG_HIGHBITDEPTH
    const int quantizer_shift = td->mb.e_mbd.bd - 8;
#else
    const int quantizer_shift = 0;
#endif  // CONFIG_HIGHBITDEPTH
    int64_t q_ac = OD_MAXI(
        1, av1_ac_quant(qindex, 0, cpi->common.bit_depth) >> quantizer_shift);
    int64_t q_dc = OD_MAXI(
        1, av1_dc_quant(qindex, 0, cpi->common.bit_depth) >> quantizer_shift);
    /* td->mb.daala_enc.pvq_norm_lambda = OD_PVQ_LAMBDA; */
    td->mb.daala_enc.pvq_norm_lambda =
        (double)rdmult * (64 / 16) / (q_ac * q_ac * (1 << RDDIV_BITS));
    td->mb.daala_enc.pvq_norm_lambda_dc =
        (double)rdmult * (64 / 16) / (q_dc * q_dc * (1 << RDDIV_BITS));
    // printf("%f\n", td->mb.daala_enc.pvq_norm_lambda);
  }
  od_init_qm(td->mb.daala_enc.state.qm, td->mb.daala_enc.state.qm_inv,
             td->mb.daala_enc.qm == OD_HVS_QM ? OD_QM8_Q4_HVS : OD_QM8_Q4_FLAT);

  if (td->mb.daala_enc.use_activity_masking) {
    int pli;
    int use_masking = td->mb.daala_enc.use_activity_masking;
    int segment_id = 0;
    int qindex = av1_get_qindex(&cm->seg, segment_id, cm->base_qindex);

    for (pli = 0; pli < MAX_MB_PLANE; pli++) {
      int i;
      int q;

      q = qindex;
      if (q <= OD_DEFAULT_QMS[use_masking][0][pli].interp_q << OD_COEFF_SHIFT) {
        od_interp_qm(&td->mb.daala_enc.state.pvq_qm_q4[pli][0], q,
                     &OD_DEFAULT_QMS[use_masking][0][pli], NULL);
      } else {
        i = 0;
        while (OD_DEFAULT_QMS[use_masking][i + 1][pli].qm_q4 != NULL &&
               q > OD_DEFAULT_QMS[use_masking][i + 1][pli].interp_q
                       << OD_COEFF_SHIFT) {
          i++;
        }
        od_interp_qm(&td->mb.daala_enc.state.pvq_qm_q4[pli][0], q,
                     &OD_DEFAULT_QMS[use_masking][i][pli],
                     &OD_DEFAULT_QMS[use_masking][i + 1][pli]);
      }
    }
  }

#if !CONFIG_ANS
  od_ec_enc_init(&td->mb.daala_enc.w.ec, 65025);
  od_ec_enc_reset(&td->mb.daala_enc.w.ec);
#else
#error "CONFIG_PVQ currently requires !CONFIG_ANS."
#endif
#endif  // #if CONFIG_PVQ

  this_tile->tctx = *cm->fc;
  td->mb.e_mbd.tile_ctx = &this_tile->tctx;

#if CONFIG_CFL
  MACROBLOCKD *const xd = &td->mb.e_mbd;
  xd->cfl = &this_tile->cfl;
  cfl_init(xd->cfl, cm);
#endif

#if CONFIG_PVQ
  td->mb.daala_enc.state.adapt = &this_tile->tctx.pvq_context;
#endif  // CONFIG_PVQ

#if CONFIG_LOOPFILTERING_ACROSS_TILES
  if (!cm->loop_filter_across_tiles_enabled)
    av1_setup_across_tile_boundary_info(cm, tile_info);
#endif

  av1_crc_calculator_init(&td->mb.tx_rd_record.crc_calculator, 24, 0x5D6DCB);

  for (mi_row = tile_info->mi_row_start; mi_row < tile_info->mi_row_end;
       mi_row += cm->mib_size) {
    encode_rd_sb_row(cpi, td, this_tile, mi_row, &tok);
  }

  cpi->tok_count[tile_row][tile_col] =
      (unsigned int)(tok - cpi->tile_tok[tile_row][tile_col]);
  assert(cpi->tok_count[tile_row][tile_col] <= allocated_tokens(*tile_info));
#if CONFIG_PVQ
#if !CONFIG_ANS
  od_ec_enc_clear(&td->mb.daala_enc.w.ec);
#else
#error "CONFIG_PVQ currently requires !CONFIG_ANS."
#endif

  td->mb.pvq_q->last_pos = td->mb.pvq_q->curr_pos;
  // rewind current position so that bitstream can be written
  // from the 1st pvq block
  td->mb.pvq_q->curr_pos = 0;

  td->mb.pvq_q = NULL;
#endif
}

static void encode_tiles(AV1_COMP *cpi) {
  AV1_COMMON *const cm = &cpi->common;
  int tile_col, tile_row;

  av1_init_tile_data(cpi);

  for (tile_row = 0; tile_row < cm->tile_rows; ++tile_row)
    for (tile_col = 0; tile_col < cm->tile_cols; ++tile_col)
      av1_encode_tile(cpi, &cpi->td, tile_row, tile_col);
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

#if CONFIG_GLOBAL_MOTION
#define GLOBAL_TRANS_TYPES_ENC 3  // highest motion model to search
static int gm_get_params_cost(const WarpedMotionParams *gm,
                              const WarpedMotionParams *ref_gm, int allow_hp) {
  assert(gm->wmtype < GLOBAL_TRANS_TYPES);
  int params_cost = 0;
  int trans_bits, trans_prec_diff;
  switch (gm->wmtype) {
    case HOMOGRAPHY:
    case HORTRAPEZOID:
    case VERTRAPEZOID:
      if (gm->wmtype != HORTRAPEZOID)
        params_cost += aom_count_signed_primitive_refsubexpfin(
            GM_ROW3HOMO_MAX + 1, SUBEXPFIN_K,
            (ref_gm->wmmat[6] >> GM_ROW3HOMO_PREC_DIFF),
            (gm->wmmat[6] >> GM_ROW3HOMO_PREC_DIFF));
      if (gm->wmtype != VERTRAPEZOID)
        params_cost += aom_count_signed_primitive_refsubexpfin(
            GM_ROW3HOMO_MAX + 1, SUBEXPFIN_K,
            (ref_gm->wmmat[7] >> GM_ROW3HOMO_PREC_DIFF),
            (gm->wmmat[7] >> GM_ROW3HOMO_PREC_DIFF));
    // Fallthrough intended
    case AFFINE:
    case ROTZOOM:
      params_cost += aom_count_signed_primitive_refsubexpfin(
          GM_ALPHA_MAX + 1, SUBEXPFIN_K,
          (ref_gm->wmmat[2] >> GM_ALPHA_PREC_DIFF) - (1 << GM_ALPHA_PREC_BITS),
          (gm->wmmat[2] >> GM_ALPHA_PREC_DIFF) - (1 << GM_ALPHA_PREC_BITS));
      if (gm->wmtype != VERTRAPEZOID)
        params_cost += aom_count_signed_primitive_refsubexpfin(
            GM_ALPHA_MAX + 1, SUBEXPFIN_K,
            (ref_gm->wmmat[3] >> GM_ALPHA_PREC_DIFF),
            (gm->wmmat[3] >> GM_ALPHA_PREC_DIFF));
      if (gm->wmtype >= AFFINE) {
        if (gm->wmtype != HORTRAPEZOID)
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
    // Fallthrough intended
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
    // Fallthrough intended
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
#if CONFIG_EXT_REFS
      return !(frame == LAST2_FRAME || frame == LAST3_FRAME);
#else
      return (num_refs_using_gm < 2);
#endif  // CONFIG_EXT_REFS
    case GM_DISABLE_SEARCH: return 0;
    default: assert(0);
  }
  return 1;
}
#endif  // CONFIG_GLOBAL_MOTION

// TODO(anybody) : remove this flag when PVQ supports pallete coding tool
#if !CONFIG_PVQ
// Estimate if the source frame is screen content, based on the portion of
// blocks that have no more than 4 (experimentally selected) luma colors.
static int is_screen_content(const uint8_t *src,
#if CONFIG_HIGHBITDEPTH
                             int use_hbd, int bd,
#endif  // CONFIG_HIGHBITDEPTH
                             int stride, int width, int height) {
  assert(src != NULL);
  int counts = 0;
  const int blk_w = 16;
  const int blk_h = 16;
  const int limit = 4;
  for (int r = 0; r + blk_h <= height; r += blk_h) {
    for (int c = 0; c + blk_w <= width; c += blk_w) {
      const int n_colors =
#if CONFIG_HIGHBITDEPTH
          use_hbd ? av1_count_colors_highbd(src + r * stride + c, stride, blk_w,
                                            blk_h, bd)
                  :
#endif  // CONFIG_HIGHBITDEPTH
                  av1_count_colors(src + r * stride + c, stride, blk_w, blk_h);
      if (n_colors > 1 && n_colors <= limit) counts++;
    }
  }
  // The threshold is 10%.
  return counts * blk_h * blk_w * 10 > width * height;
}
#endif  // !CONFIG_PVQ

static void encode_frame_internal(AV1_COMP *cpi) {
  ThreadData *const td = &cpi->td;
  MACROBLOCK *const x = &td->mb;
  AV1_COMMON *const cm = &cpi->common;
  MACROBLOCKD *const xd = &x->e_mbd;
  RD_COUNTS *const rdc = &cpi->td.rd_counts;
  int i;
#if CONFIG_TEMPMV_SIGNALING || CONFIG_EXT_REFS
  const int last_fb_buf_idx = get_ref_frame_buf_idx(cpi, LAST_FRAME);
#endif  // CONFIG_TEMPMV_SIGNALING || CONFIG_EXT_REFS

#if CONFIG_ADAPT_SCAN
  av1_deliver_eob_threshold(cm, xd);
#endif

  x->min_partition_size = AOMMIN(x->min_partition_size, cm->sb_size);
  x->max_partition_size = AOMMIN(x->max_partition_size, cm->sb_size);
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
// TODO(anybody) : remove this flag when PVQ supports pallete coding tool
#if !CONFIG_PVQ
    cm->allow_screen_content_tools =
        cpi->oxcf.content == AOM_CONTENT_SCREEN ||
        is_screen_content(cpi->source->y_buffer,
#if CONFIG_HIGHBITDEPTH
                          cpi->source->flags & YV12_FLAG_HIGHBITDEPTH, xd->bd,
#endif  // CONFIG_HIGHBITDEPTH
                          cpi->source->y_stride, cpi->source->y_width,
                          cpi->source->y_height);
#else
    cm->allow_screen_content_tools = 0;
#endif  // !CONFIG_PVQ
  }

#if CONFIG_HASH_ME
  if (cpi->oxcf.pass != 1 && cpi->common.allow_screen_content_tools) {
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

    for (k = 0; k < 2; k++) {
      for (j = 0; j < 2; j++) {
        aom_free(block_hash_values[k][j]);
      }

      for (j = 0; j < 3; j++) {
        aom_free(is_block_same[k][j]);
      }
    }
  }
#endif

#if CONFIG_NCOBMC_ADAPT_WEIGHT
  alloc_ncobmc_pred_buffer(xd);
#endif

#if CONFIG_GLOBAL_MOTION
  av1_zero(rdc->global_motion_used);
  av1_zero(cpi->gmparams_cost);
  if (cpi->common.frame_type == INTER_FRAME && cpi->source &&
      !cpi->global_motion_search_done) {
    YV12_BUFFER_CONFIG *ref_buf[TOTAL_REFS_PER_FRAME];
    int frame;
    double params_by_motion[RANSAC_NUM_MOTIONS * (MAX_PARAMDIM - 1)];
    const double *params_this_motion;
    int inliers_by_motion[RANSAC_NUM_MOTIONS];
    WarpedMotionParams tmp_wm_params;
    static const double kIdentityParams[MAX_PARAMDIM - 1] = {
      0.0, 0.0, 1.0, 0.0, 0.0, 1.0, 0.0, 0.0
    };
    int num_refs_using_gm = 0;

    for (frame = LAST_FRAME; frame <= ALTREF_FRAME; ++frame) {
      ref_buf[frame] = get_ref_frame_buffer(cpi, frame);
      int pframe;
      cm->global_motion[frame] = default_warp_params;
      const WarpedMotionParams *ref_params =
          cm->error_resilient_mode ? &default_warp_params
                                   : &cm->prev_frame->global_motion[frame];
      // check for duplicate buffer
      for (pframe = LAST_FRAME; pframe < frame; ++pframe) {
        if (ref_buf[frame] == ref_buf[pframe]) break;
      }
      if (pframe < frame) {
        memcpy(&cm->global_motion[frame], &cm->global_motion[pframe],
               sizeof(WarpedMotionParams));
      } else if (ref_buf[frame] &&
                 ref_buf[frame]->y_crop_width == cpi->source->y_crop_width &&
                 ref_buf[frame]->y_crop_height == cpi->source->y_crop_height &&
                 do_gm_search_logic(&cpi->sf, num_refs_using_gm, frame)) {
        TransformationType model;
        const int64_t ref_frame_error = av1_frame_error(
#if CONFIG_HIGHBITDEPTH
            xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH, xd->bd,
#endif  // CONFIG_HIGHBITDEPTH
            ref_buf[frame]->y_buffer, ref_buf[frame]->y_stride,
            cpi->source->y_buffer, cpi->source->y_width, cpi->source->y_height,
            cpi->source->y_stride);

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
              model, cpi->source, ref_buf[frame],
#if CONFIG_HIGHBITDEPTH
              cpi->common.bit_depth,
#endif  // CONFIG_HIGHBITDEPTH
              inliers_by_motion, params_by_motion, RANSAC_NUM_MOTIONS);

          for (i = 0; i < RANSAC_NUM_MOTIONS; ++i) {
            if (inliers_by_motion[i] == 0) continue;

            params_this_motion = params_by_motion + (MAX_PARAMDIM - 1) * i;
            convert_model_to_params(params_this_motion, &tmp_wm_params);

            if (tmp_wm_params.wmtype != IDENTITY) {
              const int64_t warp_error = refine_integerized_param(
                  &tmp_wm_params, tmp_wm_params.wmtype,
#if CONFIG_HIGHBITDEPTH
                  xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH, xd->bd,
#endif  // CONFIG_HIGHBITDEPTH
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
                                     cm->allow_high_precision_mv))) {
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
         TOTAL_REFS_PER_FRAME * sizeof(WarpedMotionParams));
#endif  // CONFIG_GLOBAL_MOTION

  for (i = 0; i < MAX_SEGMENTS; ++i) {
    const int qindex = cm->seg.enabled
                           ? av1_get_qindex(&cm->seg, i, cm->base_qindex)
                           : cm->base_qindex;
    xd->lossless[i] = qindex == 0 && cm->y_dc_delta_q == 0 &&
                      cm->uv_dc_delta_q == 0 && cm->uv_ac_delta_q == 0;
    xd->qindex[i] = qindex;
  }
  cm->all_lossless = all_lossless(cm, xd);
  if (!cm->seg.enabled && xd->lossless[0]) x->optimize = 0;

  cm->tx_mode = select_tx_mode(cpi);

  // Fix delta q resolution for the moment
  cm->delta_q_res = DEFAULT_DELTA_Q_RES;
// Set delta_q_present_flag before it is used for the first time
#if CONFIG_EXT_DELTA_Q
  cm->delta_lf_res = DEFAULT_DELTA_LF_RES;
  // update delta_q_present_flag and delta_lf_present_flag based on base_qindex
  cm->delta_q_present_flag &= cm->base_qindex > 0;
  cm->delta_lf_present_flag &= cm->base_qindex > 0;
#else
  cm->delta_q_present_flag =
      cpi->oxcf.aq_mode == DELTA_AQ && cm->base_qindex > 0;
#endif  // CONFIG_EXT_DELTA_Q

  av1_frame_init_quantizer(cpi);

  av1_initialize_rd_consts(cpi);
  av1_initialize_me_consts(cpi, x, cm->base_qindex);
  init_encode_frame_mb_context(cpi);

#if CONFIG_EXT_REFS || CONFIG_TEMPMV_SIGNALING
  // NOTE(zoeliu): As cm->prev_frame can take neither a frame of
  //               show_exisiting_frame=1, nor can it take a frame not used as
  //               a reference, it is probable that by the time it is being
  //               referred to, the frame buffer it originally points to may
  //               already get expired and have been reassigned to the current
  //               newly coded frame. Hence, we need to check whether this is
  //               the case, and if yes, we have 2 choices:
  //               (1) Simply disable the use of previous frame mvs; or
  //               (2) Have cm->prev_frame point to one reference frame buffer,
  //                   e.g. LAST_FRAME.
  if (!enc_is_ref_frame_buf(cpi, cm->prev_frame)) {
    // Reassign the LAST_FRAME buffer to cm->prev_frame.
    cm->prev_frame = last_fb_buf_idx != INVALID_IDX
                         ? &cm->buffer_pool->frame_bufs[last_fb_buf_idx]
                         : NULL;
  }
#endif  // CONFIG_EXT_REFS || CONFIG_TEMPMV_SIGNALING

#if CONFIG_TEMPMV_SIGNALING
  cm->use_prev_frame_mvs &= frame_can_use_prev_frame_mvs(cm);
#else
  if (cm->prev_frame) {
    cm->use_prev_frame_mvs = !cm->error_resilient_mode &&
#if CONFIG_FRAME_SUPERRES
                             cm->width == cm->last_width &&
                             cm->height == cm->last_height &&
#else
                             cm->width == cm->prev_frame->buf.y_crop_width &&
                             cm->height == cm->prev_frame->buf.y_crop_height &&
#endif  // CONFIG_FRAME_SUPERRES
                             !cm->intra_only && cm->last_show_frame;
  } else {
    cm->use_prev_frame_mvs = 0;
  }
#endif  // CONFIG_TEMPMV_SIGNALING

  // Special case: set prev_mi to NULL when the previous mode info
  // context cannot be used.
  cm->prev_mi =
      cm->use_prev_frame_mvs ? cm->prev_mip + cm->mi_stride + 1 : NULL;

#if CONFIG_VAR_TX
  x->txb_split_count = 0;
  av1_zero(x->blk_skip_drl);
#endif

#if CONFIG_MFMV
  av1_setup_motion_field(cm);
#endif  // CONFIG_MFMV

  {
    struct aom_usec_timer emr_timer;
    aom_usec_timer_start(&emr_timer);

#if CONFIG_FP_MB_STATS
    if (cpi->use_fp_mb_stats) {
      input_fpmb_stats(&cpi->twopass.firstpass_mb_stats, cm,
                       &cpi->twopass.this_frame_mb_stats);
    }
#endif

    av1_setup_frame_boundary_info(cm);

    // If allowed, encoding tiles in parallel with one thread handling one tile.
    // TODO(geza.lore): The multi-threaded encoder is not safe with more than
    // 1 tile rows, as it uses the single above_context et al arrays from
    // cpi->common
    if (AOMMIN(cpi->oxcf.max_threads, cm->tile_cols) > 1 && cm->tile_rows == 1)
      av1_encode_tiles_mt(cpi);
    else
      encode_tiles(cpi);

    aom_usec_timer_mark(&emr_timer);
    cpi->time_encode_sb_row += aom_usec_timer_elapsed(&emr_timer);
  }
#if CONFIG_NCOBMC_ADAPT_WEIGHT
  free_ncobmc_pred_buffer(xd);
#endif

#if 0
  // Keep record of the total distortion this time around for future use
  cpi->last_frame_distortion = cpi->frame_distortion;
#endif
}

static void make_consistent_compound_tools(AV1_COMMON *cm) {
  (void)cm;
#if CONFIG_INTERINTRA
  if (frame_is_intra_only(cm) || cm->reference_mode == COMPOUND_REFERENCE)
    cm->allow_interintra_compound = 0;
#endif  // CONFIG_INTERINTRA
#if CONFIG_COMPOUND_SEGMENT || CONFIG_WEDGE
#if CONFIG_COMPOUND_SINGLEREF
  if (frame_is_intra_only(cm))
#else   // !CONFIG_COMPOUND_SINGLEREF
  if (frame_is_intra_only(cm) || cm->reference_mode == SINGLE_REFERENCE)
#endif  // CONFIG_COMPOUND_SINGLEREF
    cm->allow_masked_compound = 0;
#endif  // CONFIG_COMPOUND_SEGMENT || CONFIG_WEDGE
}

void av1_encode_frame(AV1_COMP *cpi) {
  AV1_COMMON *const cm = &cpi->common;
#if CONFIG_EXT_TX
  // Indicates whether or not to use a default reduced set for ext-tx
  // rather than the potential full set of 16 transforms
  cm->reduced_tx_set_used = 0;
#endif  // CONFIG_EXT_TX
#if CONFIG_ADAPT_SCAN
  cm->use_adapt_scan = 1;
  // TODO(angiebird): call av1_init_scan_order only when use_adapt_scan
  // switches from 1 to 0
  if (cm->use_adapt_scan == 0) av1_init_scan_order(cm);
#endif

#if CONFIG_FRAME_MARKER
  if (cm->show_frame == 0) {
    int arf_offset = AOMMIN(
        (MAX_GF_INTERVAL - 1),
        cpi->twopass.gf_group.arf_src_offset[cpi->twopass.gf_group.index]);
#if CONFIG_EXT_REFS
    int brf_offset =
        cpi->twopass.gf_group.brf_src_offset[cpi->twopass.gf_group.index];
    arf_offset = AOMMIN((MAX_GF_INTERVAL - 1), arf_offset + brf_offset);
#endif  // CONFIG_EXT_REFS
    cm->frame_offset = cm->current_video_frame + arf_offset;
  } else {
    cm->frame_offset = cm->current_video_frame;
  }
  av1_setup_frame_buf_refs(cm);
#if CONFIG_FRAME_SIGN_BIAS
  av1_setup_frame_sign_bias(cm);
#endif  // CONFIG_FRAME_SIGN_BIAS
#endif  // CONFIG_FRAME_MARKER

  // In the longer term the encoder should be generalized to match the
  // decoder such that we allow compound where one of the 3 buffers has a
  // different sign bias and that buffer is then the fixed ref. However, this
  // requires further work in the rd loop. For now the only supported encoder
  // side behavior is where the ALT ref buffer has opposite sign bias to
  // the other two.
  if (!frame_is_intra_only(cm)) {
#if !CONFIG_ONE_SIDED_COMPOUND
    if ((cm->ref_frame_sign_bias[ALTREF_FRAME] ==
         cm->ref_frame_sign_bias[GOLDEN_FRAME]) ||
        (cm->ref_frame_sign_bias[ALTREF_FRAME] ==
         cm->ref_frame_sign_bias[LAST_FRAME])) {
      cpi->allow_comp_inter_inter = 0;
    } else {
#endif  // !CONFIG_ONE_SIDED_COMPOUND
      cpi->allow_comp_inter_inter = 1;
#if CONFIG_EXT_REFS
      cm->comp_fwd_ref[0] = LAST_FRAME;
      cm->comp_fwd_ref[1] = LAST2_FRAME;
      cm->comp_fwd_ref[2] = LAST3_FRAME;
      cm->comp_fwd_ref[3] = GOLDEN_FRAME;
      cm->comp_bwd_ref[0] = BWDREF_FRAME;
      cm->comp_bwd_ref[1] = ALTREF2_FRAME;
      cm->comp_bwd_ref[2] = ALTREF_FRAME;
#else                           // !CONFIG_EXT_REFS
    cm->comp_fixed_ref = ALTREF_FRAME;
    cm->comp_var_ref[0] = LAST_FRAME;
    cm->comp_var_ref[1] = GOLDEN_FRAME;
#endif                          // CONFIG_EXT_REFS
#if !CONFIG_ONE_SIDED_COMPOUND  // Normative in encoder
    }
#endif  // !CONFIG_ONE_SIDED_COMPOUND
  } else {
    cpi->allow_comp_inter_inter = 0;
  }

  if (cpi->sf.frame_parameter_update) {
    int i;
    RD_OPT *const rd_opt = &cpi->rd;
    FRAME_COUNTS *counts = cpi->td.counts;
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
#if CONFIG_REF_ADAPT
    // NOTE(zoeliu): "is_alt_ref" is true only for OVERLAY/INTNL_OVERLAY frames
    if (is_alt_ref || !cpi->allow_comp_inter_inter)
      cm->reference_mode = SINGLE_REFERENCE;
    else
      cm->reference_mode = REFERENCE_MODE_SELECT;
#else
#if CONFIG_BGSPRITE
    (void)is_alt_ref;
    if (!cpi->allow_comp_inter_inter)
#else
    if (is_alt_ref || !cpi->allow_comp_inter_inter)
#endif  // CONFIG_BGSPRITE
      cm->reference_mode = SINGLE_REFERENCE;
    else if (mode_thrs[COMPOUND_REFERENCE] > mode_thrs[SINGLE_REFERENCE] &&
             mode_thrs[COMPOUND_REFERENCE] > mode_thrs[REFERENCE_MODE_SELECT] &&
             check_dual_ref_flags(cpi) && cpi->static_mb_pct == 100)
      cm->reference_mode = COMPOUND_REFERENCE;
    else if (mode_thrs[SINGLE_REFERENCE] > mode_thrs[REFERENCE_MODE_SELECT])
      cm->reference_mode = SINGLE_REFERENCE;
    else
      cm->reference_mode = REFERENCE_MODE_SELECT;
#endif  // CONFIG_REF_ADAPT

#if CONFIG_DUAL_FILTER
    cm->interp_filter = SWITCHABLE;
#endif

    make_consistent_compound_tools(cm);

    rdc->single_ref_used_flag = 0;
    rdc->compound_ref_used_flag = 0;

    encode_frame_internal(cpi);

    for (i = 0; i < REFERENCE_MODES; ++i)
      mode_thrs[i] = (mode_thrs[i] + rdc->comp_pred_diff[i] / cm->MBs) / 2;

    if (cm->reference_mode == REFERENCE_MODE_SELECT) {
      // Use a flag that includes 4x4 blocks
      if (rdc->compound_ref_used_flag == 0) {
        cm->reference_mode = SINGLE_REFERENCE;
        av1_zero(counts->comp_inter);
#if !CONFIG_REF_ADAPT
        // Use a flag that includes 4x4 blocks
      } else if (rdc->single_ref_used_flag == 0) {
        cm->reference_mode = COMPOUND_REFERENCE;
        av1_zero(counts->comp_inter);
#endif  // !CONFIG_REF_ADAPT
      }
    }
    make_consistent_compound_tools(cm);

#if CONFIG_VAR_TX
#if CONFIG_RECT_TX_EXT
    if (cm->tx_mode == TX_MODE_SELECT && cpi->td.mb.txb_split_count == 0 &&
        counts->quarter_tx_size[1] == 0)
#else
    if (cm->tx_mode == TX_MODE_SELECT && cpi->td.mb.txb_split_count == 0)
#endif
      cm->tx_mode = ALLOW_32X32 + CONFIG_TX64X64;
#else
#if CONFIG_RECT_TX_EXT && CONFIG_EXT_TX
    if (cm->tx_mode == TX_MODE_SELECT && counts->quarter_tx_size[1] == 0)
#else
    if (cm->tx_mode == TX_MODE_SELECT)
#endif
    {
#if CONFIG_TX64X64
      int count4x4 = 0;
      int count8x8_8x8p = 0, count8x8_lp = 0;
      int count16x16_16x16p = 0, count16x16_lp = 0;
      int count32x32_32x32p = 0, count32x32_lp = 0;
      int count64x64_64x64p = 0;
      for (i = 0; i < TX_SIZE_CONTEXTS; ++i) {
        int depth;
        // counts->tx_size[max_depth][context_idx][this_depth_level]
        depth = tx_size_to_depth(TX_4X4);
        count4x4 += counts->tx_size[TX_8X8 - TX_SIZE_CTX_MIN][i][depth];
        count4x4 += counts->tx_size[TX_16X16 - TX_SIZE_CTX_MIN][i][depth];
        count4x4 += counts->tx_size[TX_32X32 - TX_SIZE_CTX_MIN][i][depth];
        count4x4 += counts->tx_size[TX_64X64 - TX_SIZE_CTX_MIN][i][depth];

        depth = tx_size_to_depth(TX_8X8);
        count8x8_8x8p += counts->tx_size[TX_8X8 - TX_SIZE_CTX_MIN][i][depth];
        count8x8_lp += counts->tx_size[TX_16X16 - TX_SIZE_CTX_MIN][i][depth];
        count8x8_lp += counts->tx_size[TX_32X32 - TX_SIZE_CTX_MIN][i][depth];
        count8x8_lp += counts->tx_size[TX_64X64 - TX_SIZE_CTX_MIN][i][depth];

        depth = tx_size_to_depth(TX_16X16);
        count16x16_16x16p +=
            counts->tx_size[TX_16X16 - TX_SIZE_CTX_MIN][i][depth];
        count16x16_lp += counts->tx_size[TX_32X32 - TX_SIZE_CTX_MIN][i][depth];
        count16x16_lp += counts->tx_size[TX_64X64 - TX_SIZE_CTX_MIN][i][depth];

        depth = tx_size_to_depth(TX_32X32);
        count32x32_32x32p +=
            counts->tx_size[TX_32X32 - TX_SIZE_CTX_MIN][i][depth];
        count32x32_lp += counts->tx_size[TX_64X64 - TX_SIZE_CTX_MIN][i][depth];

        depth = tx_size_to_depth(TX_64X64);
        count64x64_64x64p +=
            counts->tx_size[TX_64X64 - TX_SIZE_CTX_MIN][i][depth];
      }
#if CONFIG_EXT_TX && CONFIG_RECT_TX
      count4x4 += counts->tx_size_implied[TX_4X4][TX_4X4];
      count4x4 += counts->tx_size_implied[TX_8X8][TX_4X4];
      count4x4 += counts->tx_size_implied[TX_16X16][TX_4X4];
      count4x4 += counts->tx_size_implied[TX_32X32][TX_4X4];
      count8x8_8x8p += counts->tx_size_implied[TX_8X8][TX_8X8];
      count8x8_lp += counts->tx_size_implied[TX_16X16][TX_8X8];
      count8x8_lp += counts->tx_size_implied[TX_32X32][TX_8X8];
      count8x8_lp += counts->tx_size_implied[TX_64X64][TX_8X8];
      count16x16_16x16p += counts->tx_size_implied[TX_16X16][TX_16X16];
      count16x16_lp += counts->tx_size_implied[TX_32X32][TX_16X16];
      count16x16_lp += counts->tx_size_implied[TX_64X64][TX_16X16];
      count32x32_32x32p += counts->tx_size_implied[TX_32X32][TX_32X32];
      count32x32_lp += counts->tx_size_implied[TX_64X64][TX_32X32];
      count64x64_64x64p += counts->tx_size_implied[TX_64X64][TX_64X64];
#endif  // CONFIG_EXT_TX && CONFIG_RECT_TX
      if (count4x4 == 0 && count16x16_lp == 0 && count16x16_16x16p == 0 &&
          count32x32_lp == 0 && count32x32_32x32p == 0 &&
#if CONFIG_SUPERTX
          cm->counts.supertx_size[TX_16X16] == 0 &&
          cm->counts.supertx_size[TX_32X32] == 0 &&
          cm->counts.supertx_size[TX_64X64] == 0 &&
#endif
          count64x64_64x64p == 0) {
        cm->tx_mode = ALLOW_8X8;
        reset_skip_tx_size(cm, TX_8X8);
      } else if (count8x8_8x8p == 0 && count8x8_lp == 0 &&
                 count16x16_16x16p == 0 && count16x16_lp == 0 &&
                 count32x32_32x32p == 0 && count32x32_lp == 0 &&
#if CONFIG_SUPERTX
                 cm->counts.supertx_size[TX_8X8] == 0 &&
                 cm->counts.supertx_size[TX_16X16] == 0 &&
                 cm->counts.supertx_size[TX_32X32] == 0 &&
                 cm->counts.supertx_size[TX_64X64] == 0 &&
#endif
                 count64x64_64x64p == 0) {
        cm->tx_mode = ONLY_4X4;
        reset_skip_tx_size(cm, TX_4X4);
      } else if (count4x4 == 0 && count8x8_lp == 0 && count16x16_lp == 0 &&
                 count32x32_lp == 0) {
        cm->tx_mode = ALLOW_64X64;
      } else if (count4x4 == 0 && count8x8_lp == 0 && count16x16_lp == 0 &&
#if CONFIG_SUPERTX
                 cm->counts.supertx_size[TX_64X64] == 0 &&
#endif
                 count64x64_64x64p == 0) {
        cm->tx_mode = ALLOW_32X32;
        reset_skip_tx_size(cm, TX_32X32);
      } else if (count4x4 == 0 && count8x8_lp == 0 && count32x32_lp == 0 &&
                 count32x32_32x32p == 0 &&
#if CONFIG_SUPERTX
                 cm->counts.supertx_size[TX_32X32] == 0 &&
                 cm->counts.supertx_size[TX_64X64] == 0 &&
#endif
                 count64x64_64x64p == 0) {
        cm->tx_mode = ALLOW_16X16;
        reset_skip_tx_size(cm, TX_16X16);
      }

#else  // CONFIG_TX64X64

      int count4x4 = 0;
      int count8x8_lp = 0, count8x8_8x8p = 0;
      int count16x16_16x16p = 0, count16x16_lp = 0;
      int count32x32 = 0;
      for (i = 0; i < TX_SIZE_CONTEXTS; ++i) {
        int depth;
        // counts->tx_size[max_depth][context_idx][this_depth_level]
        depth = tx_size_to_depth(TX_4X4);
        count4x4 += counts->tx_size[TX_8X8 - TX_SIZE_CTX_MIN][i][depth];
        count4x4 += counts->tx_size[TX_16X16 - TX_SIZE_CTX_MIN][i][depth];
        count4x4 += counts->tx_size[TX_32X32 - TX_SIZE_CTX_MIN][i][depth];

        depth = tx_size_to_depth(TX_8X8);
        count8x8_8x8p += counts->tx_size[TX_8X8 - TX_SIZE_CTX_MIN][i][depth];
        count8x8_lp += counts->tx_size[TX_16X16 - TX_SIZE_CTX_MIN][i][depth];
        count8x8_lp += counts->tx_size[TX_32X32 - TX_SIZE_CTX_MIN][i][depth];

        depth = tx_size_to_depth(TX_16X16);
        count16x16_16x16p +=
            counts->tx_size[TX_16X16 - TX_SIZE_CTX_MIN][i][depth];
        count16x16_lp += counts->tx_size[TX_32X32 - TX_SIZE_CTX_MIN][i][depth];

        depth = tx_size_to_depth(TX_32X32);
        count32x32 += counts->tx_size[TX_32X32 - TX_SIZE_CTX_MIN][i][depth];
      }
#if CONFIG_EXT_TX && CONFIG_RECT_TX
      count4x4 += counts->tx_size_implied[TX_4X4][TX_4X4];
      count4x4 += counts->tx_size_implied[TX_8X8][TX_4X4];
      count4x4 += counts->tx_size_implied[TX_16X16][TX_4X4];
      count4x4 += counts->tx_size_implied[TX_32X32][TX_4X4];
      count8x8_8x8p += counts->tx_size_implied[TX_8X8][TX_8X8];
      count8x8_lp += counts->tx_size_implied[TX_16X16][TX_8X8];
      count8x8_lp += counts->tx_size_implied[TX_32X32][TX_8X8];
      count16x16_16x16p += counts->tx_size_implied[TX_16X16][TX_16X16];
      count16x16_lp += counts->tx_size_implied[TX_32X32][TX_16X16];
      count32x32 += counts->tx_size_implied[TX_32X32][TX_32X32];
#endif  // CONFIG_EXT_TX && CONFIG_RECT_TX
      if (count4x4 == 0 && count16x16_lp == 0 && count16x16_16x16p == 0 &&
#if CONFIG_SUPERTX
          cm->counts.supertx_size[TX_16X16] == 0 &&
          cm->counts.supertx_size[TX_32X32] == 0 &&
#endif  // CONFIG_SUPERTX
          count32x32 == 0) {
        cm->tx_mode = ALLOW_8X8;
        reset_skip_tx_size(cm, TX_8X8);
      } else if (count8x8_8x8p == 0 && count16x16_16x16p == 0 &&
                 count8x8_lp == 0 && count16x16_lp == 0 &&
#if CONFIG_SUPERTX
                 cm->counts.supertx_size[TX_8X8] == 0 &&
                 cm->counts.supertx_size[TX_16X16] == 0 &&
                 cm->counts.supertx_size[TX_32X32] == 0 &&
#endif  // CONFIG_SUPERTX
                 count32x32 == 0) {
        cm->tx_mode = ONLY_4X4;
        reset_skip_tx_size(cm, TX_4X4);
      } else if (count8x8_lp == 0 && count16x16_lp == 0 && count4x4 == 0) {
        cm->tx_mode = ALLOW_32X32;
      } else if (count32x32 == 0 && count8x8_lp == 0 &&
#if CONFIG_SUPERTX
                 cm->counts.supertx_size[TX_32X32] == 0 &&
#endif  // CONFIG_SUPERTX
                 count4x4 == 0) {
        cm->tx_mode = ALLOW_16X16;
        reset_skip_tx_size(cm, TX_16X16);
      }
#endif  // CONFIG_TX64X64
    }
#endif
  } else {
    make_consistent_compound_tools(cm);
    encode_frame_internal(cpi);
  }
}

static void sum_intra_stats(FRAME_COUNTS *counts, MACROBLOCKD *xd,
                            const MODE_INFO *mi, const MODE_INFO *above_mi,
                            const MODE_INFO *left_mi, const int intraonly,
                            const int mi_row, const int mi_col) {
  FRAME_CONTEXT *fc = xd->tile_ctx;
  const MB_MODE_INFO *const mbmi = &mi->mbmi;
  const PREDICTION_MODE y_mode = mbmi->mode;
  const UV_PREDICTION_MODE uv_mode = mbmi->uv_mode;
  (void)counts;
  const BLOCK_SIZE bsize = mbmi->sb_type;
  const int unify_bsize = CONFIG_CB4X4;

  if (bsize < BLOCK_8X8 && !unify_bsize) {
    int idx, idy;
    const int num_4x4_w = num_4x4_blocks_wide_lookup[bsize];
    const int num_4x4_h = num_4x4_blocks_high_lookup[bsize];
    for (idy = 0; idy < 2; idy += num_4x4_h)
      for (idx = 0; idx < 2; idx += num_4x4_w) {
        const int bidx = idy * 2 + idx;
        const PREDICTION_MODE bmode = mi->bmi[bidx].as_mode;
        if (intraonly) {
#if CONFIG_ENTROPY_STATS
          const PREDICTION_MODE a = av1_above_block_mode(mi, above_mi, bidx);
          const PREDICTION_MODE l = av1_left_block_mode(mi, left_mi, bidx);
          ++counts->kf_y_mode[a][l][bmode];
#endif  // CONFIG_ENTROPY_STATS
          update_cdf(get_y_mode_cdf(fc, mi, above_mi, left_mi, bidx), bmode,
                     INTRA_MODES);
        } else {
#if CONFIG_ENTROPY_STATS
          ++counts->y_mode[0][bmode];
#endif  // CONFIG_ENTROPY_STATS
          update_cdf(fc->y_mode_cdf[0], bmode, INTRA_MODES);
        }
      }
  } else {
    if (intraonly) {
#if CONFIG_ENTROPY_STATS
      const PREDICTION_MODE above = av1_above_block_mode(mi, above_mi, 0);
      const PREDICTION_MODE left = av1_left_block_mode(mi, left_mi, 0);
      ++counts->kf_y_mode[above][left][y_mode];
#endif  // CONFIG_ENTROPY_STATS
      update_cdf(get_y_mode_cdf(fc, mi, above_mi, left_mi, 0), y_mode,
                 INTRA_MODES);
    } else {
#if CONFIG_ENTROPY_STATS
      ++counts->y_mode[size_group_lookup[bsize]][y_mode];
#endif  // CONFIG_ENTROPY_STATS
      update_cdf(fc->y_mode_cdf[size_group_lookup[bsize]], y_mode, INTRA_MODES);
    }

#if CONFIG_FILTER_INTRA
    if (mbmi->mode == DC_PRED && mbmi->palette_mode_info.palette_size[0] == 0) {
      const int use_filter_intra_mode =
          mbmi->filter_intra_mode_info.use_filter_intra_mode[0];
      ++counts->filter_intra[0][use_filter_intra_mode];
    }
    if (mbmi->uv_mode == UV_DC_PRED
#if CONFIG_CB4X4
        &&
        is_chroma_reference(mi_row, mi_col, bsize, xd->plane[1].subsampling_x,
                            xd->plane[1].subsampling_y)
#endif
        && mbmi->palette_mode_info.palette_size[1] == 0) {
      const int use_filter_intra_mode =
          mbmi->filter_intra_mode_info.use_filter_intra_mode[1];
      ++counts->filter_intra[1][use_filter_intra_mode];
    }
#endif  // CONFIG_FILTER_INTRA
#if CONFIG_EXT_INTRA && CONFIG_INTRA_INTERP
    if (av1_is_directional_mode(mbmi->mode, bsize)) {
      const int intra_filter_ctx = av1_get_pred_context_intra_interp(xd);
      const int p_angle =
          mode_to_angle_map[mbmi->mode] + mbmi->angle_delta[0] * ANGLE_STEP;
      if (av1_is_intra_filter_switchable(p_angle))
        ++counts->intra_filter[intra_filter_ctx][mbmi->intra_filter];
    }
#endif  // CONFIG_INTRA_INTERP && CONFIG_INTRA_INTERP
  }

#if CONFIG_CB4X4
  if (!is_chroma_reference(mi_row, mi_col, bsize, xd->plane[1].subsampling_x,
                           xd->plane[1].subsampling_y))
    return;
#else
  (void)mi_row;
  (void)mi_col;
  (void)xd;
#endif
#if CONFIG_ENTROPY_STATS
  ++counts->uv_mode[y_mode][uv_mode];
#endif  // CONFIG_ENTROPY_STATS
  update_cdf(fc->uv_mode_cdf[y_mode], uv_mode, UV_INTRA_MODES);
}

#if CONFIG_VAR_TX
static void update_txfm_count(MACROBLOCK *x, MACROBLOCKD *xd,
                              FRAME_COUNTS *counts, TX_SIZE tx_size, int depth,
                              int blk_row, int blk_col) {
  MB_MODE_INFO *mbmi = &xd->mi[0]->mbmi;
  const int tx_row = blk_row >> 1;
  const int tx_col = blk_col >> 1;
  const int max_blocks_high = max_block_high(xd, mbmi->sb_type, 0);
  const int max_blocks_wide = max_block_wide(xd, mbmi->sb_type, 0);
  int ctx = txfm_partition_context(xd->above_txfm_context + blk_col,
                                   xd->left_txfm_context + blk_row,
                                   mbmi->sb_type, tx_size);
  const TX_SIZE plane_tx_size = mbmi->inter_tx_size[tx_row][tx_col];

  if (blk_row >= max_blocks_high || blk_col >= max_blocks_wide) return;
  assert(tx_size > TX_4X4);

  if (depth == MAX_VARTX_DEPTH) {
// Don't add to counts in this case
#if CONFIG_RECT_TX_EXT
    if (tx_size == plane_tx_size)
#endif
      mbmi->tx_size = tx_size;
    txfm_partition_update(xd->above_txfm_context + blk_col,
                          xd->left_txfm_context + blk_row, tx_size, tx_size);
    return;
  }

#if CONFIG_RECT_TX_EXT
  if (tx_size == plane_tx_size ||
      mbmi->tx_size == quarter_txsize_lookup[mbmi->sb_type])
#else
  if (tx_size == plane_tx_size)
#endif
  {
    ++counts->txfm_partition[ctx][0];
#if CONFIG_RECT_TX_EXT
    if (tx_size == plane_tx_size)
#endif
      mbmi->tx_size = tx_size;
    txfm_partition_update(xd->above_txfm_context + blk_col,
                          xd->left_txfm_context + blk_row, tx_size, tx_size);
  } else {
    const TX_SIZE sub_txs = sub_tx_size_map[tx_size];
    const int bs = tx_size_wide_unit[sub_txs];
    int i;

    ++counts->txfm_partition[ctx][1];
    ++x->txb_split_count;

    if (sub_txs == TX_4X4) {
      mbmi->inter_tx_size[tx_row][tx_col] = TX_4X4;
      mbmi->tx_size = TX_4X4;
      txfm_partition_update(xd->above_txfm_context + blk_col,
                            xd->left_txfm_context + blk_row, TX_4X4, tx_size);
      return;
    }

    for (i = 0; i < 4; ++i) {
      int offsetr = (i >> 1) * bs;
      int offsetc = (i & 0x01) * bs;
      update_txfm_count(x, xd, counts, sub_txs, depth + 1, blk_row + offsetr,
                        blk_col + offsetc);
    }
  }
}

static void tx_partition_count_update(const AV1_COMMON *const cm, MACROBLOCK *x,
                                      BLOCK_SIZE plane_bsize, int mi_row,
                                      int mi_col, FRAME_COUNTS *td_counts) {
  MACROBLOCKD *xd = &x->e_mbd;
  const int mi_width = block_size_wide[plane_bsize] >> tx_size_wide_log2[0];
  const int mi_height = block_size_high[plane_bsize] >> tx_size_wide_log2[0];
  TX_SIZE max_tx_size = get_vartx_max_txsize(&xd->mi[0]->mbmi, plane_bsize, 0);
  const int bh = tx_size_high_unit[max_tx_size];
  const int bw = tx_size_wide_unit[max_tx_size];
  int idx, idy;
  int init_depth =
      (mi_height != mi_width) ? RECT_VARTX_DEPTH_INIT : SQR_VARTX_DEPTH_INIT;

#if CONFIG_INTRABC
  // Intrabc doesn't support var-tx yet. So no need to update tx partition
  // info., except for the split count (otherwise common->tx_mode may be
  // modified, causing mismatch).
  if (is_intrabc_block(&x->e_mbd.mi[0]->mbmi)) {
    if (x->e_mbd.mi[0]->mbmi.tx_size != max_tx_size) ++x->txb_split_count;
    return;
  }
#endif  // CONFIG_INTRABC

  xd->above_txfm_context =
      cm->above_txfm_context + (mi_col << TX_UNIT_WIDE_LOG2);
  xd->left_txfm_context = xd->left_txfm_context_buffer +
                          ((mi_row & MAX_MIB_MASK) << TX_UNIT_HIGH_LOG2);

  for (idy = 0; idy < mi_height; idy += bh)
    for (idx = 0; idx < mi_width; idx += bw)
      update_txfm_count(x, xd, td_counts, max_tx_size, init_depth, idy, idx);
}

static void set_txfm_context(MACROBLOCKD *xd, TX_SIZE tx_size, int blk_row,
                             int blk_col) {
  MB_MODE_INFO *mbmi = &xd->mi[0]->mbmi;
  const int tx_row = blk_row >> 1;
  const int tx_col = blk_col >> 1;
  const int max_blocks_high = max_block_high(xd, mbmi->sb_type, 0);
  const int max_blocks_wide = max_block_wide(xd, mbmi->sb_type, 0);
  const TX_SIZE plane_tx_size = mbmi->inter_tx_size[tx_row][tx_col];

  if (blk_row >= max_blocks_high || blk_col >= max_blocks_wide) return;

  if (tx_size == plane_tx_size) {
    mbmi->tx_size = tx_size;
    txfm_partition_update(xd->above_txfm_context + blk_col,
                          xd->left_txfm_context + blk_row, tx_size, tx_size);

  } else {
    const TX_SIZE sub_txs = sub_tx_size_map[tx_size];
    const int bsl = tx_size_wide_unit[sub_txs];
    int i;

    if (tx_size == TX_8X8) {
      mbmi->inter_tx_size[tx_row][tx_col] = TX_4X4;
      mbmi->tx_size = TX_4X4;
      txfm_partition_update(xd->above_txfm_context + blk_col,
                            xd->left_txfm_context + blk_row, TX_4X4, tx_size);
      return;
    }

    assert(bsl > 0);
    for (i = 0; i < 4; ++i) {
      int offsetr = (i >> 1) * bsl;
      int offsetc = (i & 0x01) * bsl;
      set_txfm_context(xd, sub_txs, blk_row + offsetr, blk_col + offsetc);
    }
  }
}

static void tx_partition_set_contexts(const AV1_COMMON *const cm,
                                      MACROBLOCKD *xd, BLOCK_SIZE plane_bsize,
                                      int mi_row, int mi_col) {
  const int mi_width = block_size_wide[plane_bsize] >> tx_size_wide_log2[0];
  const int mi_height = block_size_high[plane_bsize] >> tx_size_high_log2[0];
  TX_SIZE max_tx_size = get_vartx_max_txsize(&xd->mi[0]->mbmi, plane_bsize, 0);
  const int bh = tx_size_high_unit[max_tx_size];
  const int bw = tx_size_wide_unit[max_tx_size];
  int idx, idy;

  xd->above_txfm_context =
      cm->above_txfm_context + (mi_col << TX_UNIT_WIDE_LOG2);
  xd->left_txfm_context = xd->left_txfm_context_buffer +
                          ((mi_row & MAX_MIB_MASK) << TX_UNIT_HIGH_LOG2);

  for (idy = 0; idy < mi_height; idy += bh)
    for (idx = 0; idx < mi_width; idx += bw)
      set_txfm_context(xd, max_tx_size, idy, idx);
}
#endif

void av1_update_tx_type_count(const AV1_COMMON *cm, MACROBLOCKD *xd,
#if CONFIG_TXK_SEL
                              int blk_row, int blk_col, int block, int plane,
#endif
                              BLOCK_SIZE bsize, TX_SIZE tx_size,
                              FRAME_COUNTS *counts) {
  MB_MODE_INFO *mbmi = &xd->mi[0]->mbmi;
  int is_inter = is_inter_block(mbmi);
  FRAME_CONTEXT *fc = xd->tile_ctx;
#if !CONFIG_ENTROPY_STATS
  (void)counts;
#endif  // !CONFIG_ENTROPY_STATS

#if !CONFIG_TXK_SEL
  TX_TYPE tx_type = mbmi->tx_type;
#else
  (void)blk_row;
  (void)blk_col;
  // Only y plane's tx_type is updated
  if (plane > 0) return;
  TX_TYPE tx_type =
      av1_get_tx_type(PLANE_TYPE_Y, xd, blk_row, blk_col, block, tx_size);
#endif
#if CONFIG_EXT_TX
  if (get_ext_tx_types(tx_size, bsize, is_inter, cm->reduced_tx_set_used) > 1 &&
      cm->base_qindex > 0 && !mbmi->skip &&
      !segfeature_active(&cm->seg, mbmi->segment_id, SEG_LVL_SKIP)) {
    const int eset =
        get_ext_tx_set(tx_size, bsize, is_inter, cm->reduced_tx_set_used);
    if (eset > 0) {
#if !CONFIG_LGT_FROM_PRED
      const TxSetType tx_set_type = get_ext_tx_set_type(
          tx_size, bsize, is_inter, cm->reduced_tx_set_used);
      if (is_inter) {
        update_cdf(fc->inter_ext_tx_cdf[eset][txsize_sqr_map[tx_size]],
                   av1_ext_tx_ind[tx_set_type][tx_type],
                   av1_num_ext_tx_set[tx_set_type]);
#if CONFIG_ENTROPY_STATS
        ++counts->inter_ext_tx[eset][txsize_sqr_map[tx_size]][tx_type];
#endif  // CONFIG_ENTROPY_STATS
      } else {
#if CONFIG_ENTROPY_STATS
        ++counts->intra_ext_tx[eset][txsize_sqr_map[tx_size]][mbmi->mode]
                              [tx_type];
#endif  // CONFIG_ENTROPY_STATS
        update_cdf(
            fc->intra_ext_tx_cdf[eset][txsize_sqr_map[tx_size]][mbmi->mode],
            av1_ext_tx_ind[tx_set_type][tx_type],
            av1_num_ext_tx_set[tx_set_type]);
      }
#else
      (void)tx_type;
      (void)fc;
      if (is_inter) {
        if (LGT_FROM_PRED_INTER) {
          if (is_lgt_allowed(mbmi->mode, tx_size) && !cm->reduced_tx_set_used)
            ++counts->inter_lgt[txsize_sqr_map[tx_size]][mbmi->use_lgt];
#if CONFIG_ENTROPY_STATS
          if (!mbmi->use_lgt)
            ++counts->inter_ext_tx[eset][txsize_sqr_map[tx_size]][tx_type];
          else
#endif  // CONFIG_ENTROPY_STATS
            mbmi->tx_type = DCT_DCT;
        } else {
#if CONFIG_ENTROPY_STATS
          ++counts->inter_ext_tx[eset][txsize_sqr_map[tx_size]][tx_type];
#endif  // CONFIG_ENTROPY_STATS
        }
      } else {
        if (LGT_FROM_PRED_INTRA) {
          if (is_lgt_allowed(mbmi->mode, tx_size) && !cm->reduced_tx_set_used)
            ++counts->intra_lgt[txsize_sqr_map[tx_size]][mbmi->mode]
                               [mbmi->use_lgt];
#if CONFIG_ENTROPY_STATS
          if (!mbmi->use_lgt)
            ++counts->intra_ext_tx[eset][txsize_sqr_map[tx_size]][mbmi->mode]
                                  [tx_type];
          else
#endif  // CONFIG_ENTROPY_STATS
            mbmi->tx_type = DCT_DCT;
        } else {
#if CONFIG_ENTROPY_STATS
          ++counts->intra_ext_tx[eset][txsize_sqr_map[tx_size]][mbmi->mode]
                                [tx_type];
#endif  // CONFIG_ENTROPY_STATS
        }
      }
#endif  // CONFIG_LGT_FROM_PRED
    }
  }
#else
  (void)bsize;
  if (tx_size < TX_32X32 &&
      ((!cm->seg.enabled && cm->base_qindex > 0) ||
       (cm->seg.enabled && xd->qindex[mbmi->segment_id] > 0)) &&
      !mbmi->skip &&
      !segfeature_active(&cm->seg, mbmi->segment_id, SEG_LVL_SKIP)) {
    if (is_inter) {
#if CONFIG_ENTROPY_STATS
      ++counts->inter_ext_tx[tx_size][tx_type];
#endif  // CONFIG_ENTROPY_STATS
      update_cdf(fc->inter_ext_tx_cdf[tx_size], av1_ext_tx_ind[tx_type],
                 TX_TYPES);
    } else {
#if CONFIG_ENTROPY_STATS
      ++counts->intra_ext_tx[tx_size][intra_mode_to_tx_type_context[mbmi->mode]]
                            [tx_type];
#endif  // CONFIG_ENTROPY_STATS
      update_cdf(
          fc->intra_ext_tx_cdf[tx_size]
                              [intra_mode_to_tx_type_context[mbmi->mode]],
          av1_ext_tx_ind[tx_type], TX_TYPES);
    }
  }
#endif  // CONFIG_EXT_TX
}

static void encode_superblock(const AV1_COMP *const cpi, ThreadData *td,
                              TOKENEXTRA **t, RUN_TYPE dry_run, int mi_row,
                              int mi_col, BLOCK_SIZE bsize, int *rate) {
  const AV1_COMMON *const cm = &cpi->common;
  MACROBLOCK *const x = &td->mb;
  MACROBLOCKD *const xd = &x->e_mbd;
  MODE_INFO **mi_8x8 = xd->mi;
  MODE_INFO *mi = mi_8x8[0];
  MB_MODE_INFO *mbmi = &mi->mbmi;
  const int seg_skip =
      segfeature_active(&cm->seg, mbmi->segment_id, SEG_LVL_SKIP);
  const int mis = cm->mi_stride;
  const int mi_width = mi_size_wide[bsize];
  const int mi_height = mi_size_high[bsize];
  const int is_inter = is_inter_block(mbmi);
#if CONFIG_CB4X4
  const BLOCK_SIZE block_size = bsize;
#else
  const BLOCK_SIZE block_size = AOMMAX(bsize, BLOCK_8X8);
#endif

#if CONFIG_PVQ
  x->pvq_speed = 0;
  x->pvq_coded = (dry_run == OUTPUT_ENABLED) ? 1 : 0;
#endif

  if (!is_inter) {
#if CONFIG_CFL
    xd->cfl->store_y = 1;
#endif  // CONFIG_CFL
    int plane;
    mbmi->skip = 1;
    for (plane = 0; plane < MAX_MB_PLANE; ++plane) {
      av1_encode_intra_block_plane((AV1_COMMON *)cm, x, block_size, plane, 1,
                                   mi_row, mi_col);
    }
#if CONFIG_CFL
    xd->cfl->store_y = 0;
#if CONFIG_CHROMA_SUB8X8 && CONFIG_DEBUG
    if (is_chroma_reference(mi_row, mi_col, bsize, xd->cfl->subsampling_x,
                            xd->cfl->subsampling_y) &&
        !xd->cfl->are_parameters_computed) {
      cfl_clear_sub8x8_val(xd->cfl);
    }
#endif  // CONFIG_CHROMA_SUB8X8 && CONFIG_DEBUG
#endif  // CONFIG_CFL
    if (!dry_run) {
      sum_intra_stats(td->counts, xd, mi, xd->above_mi, xd->left_mi,
                      frame_is_intra_only(cm), mi_row, mi_col);
    }

// TODO(anybody) : remove this flag when PVQ supports pallete coding tool
#if !CONFIG_PVQ
    if (bsize >= BLOCK_8X8) {
      for (plane = 0; plane <= 1; ++plane) {
        if (mbmi->palette_mode_info.palette_size[plane] > 0) {
          if (!dry_run)
            av1_tokenize_color_map(x, plane, 0, t, bsize, mbmi->tx_size,
                                   PALETTE_MAP);
          else if (dry_run == DRY_RUN_COSTCOEFFS)
            rate += av1_cost_color_map(x, plane, 0, bsize, mbmi->tx_size,
                                       PALETTE_MAP);
        }
      }
    }
#endif  // !CONFIG_PVQ

#if CONFIG_VAR_TX
    mbmi->min_tx_size = get_min_tx_size(mbmi->tx_size);
#endif
#if CONFIG_LV_MAP
    av1_update_txb_context(cpi, td, dry_run, block_size, rate, mi_row, mi_col);
#else   // CONFIG_LV_MAP
    av1_tokenize_sb(cpi, td, t, dry_run, block_size, rate, mi_row, mi_col);
#endif  // CONFIG_LV_MAP
  } else {
    int ref;
    const int is_compound = has_second_ref(mbmi);

    set_ref_ptrs(cm, xd, mbmi->ref_frame[0], mbmi->ref_frame[1]);
    for (ref = 0; ref < 1 + is_compound; ++ref) {
      YV12_BUFFER_CONFIG *cfg = get_ref_frame_buffer(cpi, mbmi->ref_frame[ref]);
#if CONFIG_INTRABC
      assert(IMPLIES(!is_intrabc_block(mbmi), cfg));
#else
      assert(cfg != NULL);
#endif  // !CONFIG_INTRABC
      av1_setup_pre_planes(xd, ref, cfg, mi_row, mi_col,
                           &xd->block_refs[ref]->sf);
    }
#if CONFIG_COMPOUND_SINGLEREF
    // Single ref compound mode
    if (!is_compound && is_inter_singleref_comp_mode(mbmi->mode)) {
      xd->block_refs[1] = xd->block_refs[0];
      YV12_BUFFER_CONFIG *cfg = get_ref_frame_buffer(cpi, mbmi->ref_frame[0]);
#if CONFIG_INTRABC
      assert(IMPLIES(!is_intrabc_block(mbmi), cfg));
#else
      assert(cfg != NULL);
#endif  // !CONFIG_INTRABC
      av1_setup_pre_planes(xd, 1, cfg, mi_row, mi_col, &xd->block_refs[1]->sf);
    }
#endif  // CONFIG_COMPOUND_SINGLEREF

    av1_build_inter_predictors_sb(cm, xd, mi_row, mi_col, NULL, block_size);

#if !CONFIG_NCOBMC_ADAPT_WEIGHT
#if CONFIG_MOTION_VAR
    if (mbmi->motion_mode == OBMC_CAUSAL) {
#if CONFIG_NCOBMC
      if (dry_run == OUTPUT_ENABLED)
        av1_build_ncobmc_inter_predictors_sb(cm, xd, mi_row, mi_col);
      else
#endif
        av1_build_obmc_inter_predictors_sb(cm, xd, mi_row, mi_col);
    }
#endif  // CONFIG_MOTION_VAR
#else
    if (mbmi->motion_mode == OBMC_CAUSAL) {
      av1_build_obmc_inter_predictors_sb(cm, xd, mi_row, mi_col);
    } else if (mbmi->motion_mode == NCOBMC_ADAPT_WEIGHT &&
               dry_run == OUTPUT_ENABLED) {
      int p;
      for (p = 0; p < MAX_MB_PLANE; ++p) {
        get_pred_from_intrpl_buf(xd, mi_row, mi_col, block_size, p);
      }
    }
#endif

    av1_encode_sb((AV1_COMMON *)cm, x, block_size, mi_row, mi_col);
#if CONFIG_VAR_TX
    if (mbmi->skip) mbmi->min_tx_size = get_min_tx_size(mbmi->tx_size);
    av1_tokenize_sb_vartx(cpi, td, t, dry_run, mi_row, mi_col, block_size,
                          rate);
#else
#if CONFIG_LV_MAP
    av1_update_txb_context(cpi, td, dry_run, block_size, rate, mi_row, mi_col);
#else   // CONFIG_LV_MAP
    av1_tokenize_sb(cpi, td, t, dry_run, block_size, rate, mi_row, mi_col);
#endif  // CONFIG_LV_MAP
#endif
  }

#if CONFIG_DIST_8X8 && CONFIG_CB4X4
  if (x->using_dist_8x8 && bsize < BLOCK_8X8) {
    dist_8x8_set_sub8x8_dst(x, (uint8_t *)x->decoded_8x8, bsize,
                            block_size_wide[bsize], block_size_high[bsize],
                            mi_row, mi_col);
  }
#endif

  if (!dry_run) {
#if CONFIG_VAR_TX
    TX_SIZE tx_size =
        is_inter && !mbmi->skip ? mbmi->min_tx_size : mbmi->tx_size;
#else
    TX_SIZE tx_size = mbmi->tx_size;
#endif
    if (cm->tx_mode == TX_MODE_SELECT && !xd->lossless[mbmi->segment_id] &&
#if CONFIG_CB4X4 && (CONFIG_VAR_TX || CONFIG_EXT_TX) && CONFIG_RECT_TX
        mbmi->sb_type > BLOCK_4X4 &&
#else
        mbmi->sb_type >= BLOCK_8X8 &&
#endif
        !(is_inter && (mbmi->skip || seg_skip))) {
#if CONFIG_VAR_TX
      if (is_inter) {
        tx_partition_count_update(cm, x, bsize, mi_row, mi_col, td->counts);
      } else {
        const int tx_size_ctx = get_tx_size_context(xd);
        const int32_t tx_size_cat = is_inter ? inter_tx_size_cat_lookup[bsize]
                                             : intra_tx_size_cat_lookup[bsize];
        const TX_SIZE coded_tx_size = txsize_sqr_up_map[tx_size];
        const int depth = tx_size_to_depth(coded_tx_size);
        ++td->counts->tx_size[tx_size_cat][tx_size_ctx][depth];
        if (tx_size != max_txsize_rect_lookup[bsize]) ++x->txb_split_count;
      }
#else
      const int tx_size_ctx = get_tx_size_context(xd);
      const int32_t tx_size_cat = is_inter ? inter_tx_size_cat_lookup[bsize]
                                           : intra_tx_size_cat_lookup[bsize];
      const TX_SIZE coded_tx_size = txsize_sqr_up_map[tx_size];
      const int depth = tx_size_to_depth(coded_tx_size);

      ++td->counts->tx_size[tx_size_cat][tx_size_ctx][depth];
#endif

#if CONFIG_RECT_TX_EXT && (CONFIG_EXT_TX || CONFIG_VAR_TX)
      if (is_quarter_tx_allowed(xd, mbmi, is_inter) &&
          quarter_txsize_lookup[bsize] != max_txsize_rect_lookup[bsize] &&
          (mbmi->tx_size == quarter_txsize_lookup[bsize] ||
           mbmi->tx_size == max_txsize_rect_lookup[bsize])) {
        ++td->counts
              ->quarter_tx_size[mbmi->tx_size == quarter_txsize_lookup[bsize]];
      }
#endif
#if CONFIG_EXT_TX && CONFIG_RECT_TX
      assert(IMPLIES(is_rect_tx(tx_size), is_rect_tx_allowed(xd, mbmi)));
#endif  // CONFIG_EXT_TX && CONFIG_RECT_TX
    } else {
      int i, j;
      TX_SIZE intra_tx_size;
      // The new intra coding scheme requires no change of transform size
      if (is_inter) {
        if (xd->lossless[mbmi->segment_id]) {
          intra_tx_size = TX_4X4;
        } else {
          intra_tx_size = tx_size_from_tx_mode(bsize, cm->tx_mode, 1);
        }
      } else {
#if CONFIG_EXT_TX && CONFIG_RECT_TX
        intra_tx_size = tx_size;
#else
        intra_tx_size = (bsize >= BLOCK_8X8) ? tx_size : TX_4X4;
#endif  // CONFIG_EXT_TX && CONFIG_RECT_TX
      }
#if CONFIG_EXT_TX && CONFIG_RECT_TX
      ++td->counts->tx_size_implied[max_txsize_lookup[bsize]]
                                   [txsize_sqr_up_map[tx_size]];
#endif  // CONFIG_EXT_TX && CONFIG_RECT_TX

      for (j = 0; j < mi_height; j++)
        for (i = 0; i < mi_width; i++)
          if (mi_col + i < cm->mi_cols && mi_row + j < cm->mi_rows)
            mi_8x8[mis * j + i]->mbmi.tx_size = intra_tx_size;

#if CONFIG_VAR_TX
      mbmi->min_tx_size = get_min_tx_size(intra_tx_size);
      if (intra_tx_size != max_txsize_rect_lookup[bsize]) ++x->txb_split_count;
#endif
    }

#if !CONFIG_TXK_SEL
    av1_update_tx_type_count(cm, xd, bsize, tx_size, td->counts);
#endif
  }

#if CONFIG_VAR_TX
  if (cm->tx_mode == TX_MODE_SELECT &&
#if CONFIG_CB4X4
      mbmi->sb_type > BLOCK_4X4 &&
#else
      mbmi->sb_type >= BLOCK_8X8 &&
#endif
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
        tx_size = tx_size_from_tx_mode(bsize, cm->tx_mode, is_inter);
      }
    } else {
      tx_size = (bsize > BLOCK_4X4) ? tx_size : TX_4X4;
    }
    mbmi->tx_size = tx_size;
    set_txfm_ctxs(tx_size, xd->n8_w, xd->n8_h, (mbmi->skip || seg_skip), xd);
  }
#endif  // CONFIG_VAR_TX
#if CONFIG_CFL && CONFIG_CHROMA_SUB8X8
  CFL_CTX *const cfl = xd->cfl;
#if CONFIG_DEBUG
  if (is_chroma_reference(mi_row, mi_col, bsize, cfl->subsampling_x,
                          cfl->subsampling_y) &&
      !cfl->are_parameters_computed) {
    cfl_clear_sub8x8_val(cfl);
  }
#endif  // CONFIG_DEBUG
  if (is_inter_block(mbmi) &&
      !is_chroma_reference(mi_row, mi_col, bsize, cfl->subsampling_x,
                           cfl->subsampling_y)) {
    cfl_store_block(xd, mbmi->sb_type, mbmi->tx_size);
  }
#endif  // CONFIG_CFL && CONFIG_CHROMA_SUB8X8
}

#if CONFIG_SUPERTX
static int check_intra_b(PICK_MODE_CONTEXT *ctx) {
  if (!is_inter_mode((&ctx->mic)->mbmi.mode)) return 1;
  if (ctx->mic.mbmi.ref_frame[1] == INTRA_FRAME) return 1;
  return 0;
}

static int check_intra_sb(const AV1_COMP *const cpi, const TileInfo *const tile,
                          int mi_row, int mi_col, BLOCK_SIZE bsize,
                          PC_TREE *pc_tree) {
  const AV1_COMMON *const cm = &cpi->common;
  const int hbs = mi_size_wide[bsize] / 2;
  const PARTITION_TYPE partition = pc_tree->partitioning;
  const BLOCK_SIZE subsize = get_subsize(bsize, partition);
#if CONFIG_EXT_PARTITION_TYPES
  int i;
#endif
#if CONFIG_CB4X4
  const int unify_bsize = 1;
#else
  const int unify_bsize = 0;
#endif

#if !CONFIG_CB4X4
  assert(bsize >= BLOCK_8X8);
#endif

  if (mi_row >= cm->mi_rows || mi_col >= cm->mi_cols) return 1;

  switch (partition) {
    case PARTITION_NONE: return check_intra_b(&pc_tree->none); break;
    case PARTITION_VERT:
      if (check_intra_b(&pc_tree->vertical[0])) return 1;
      if (mi_col + hbs < cm->mi_cols && (bsize > BLOCK_8X8 || unify_bsize)) {
        if (check_intra_b(&pc_tree->vertical[1])) return 1;
      }
      break;
    case PARTITION_HORZ:
      if (check_intra_b(&pc_tree->horizontal[0])) return 1;
      if (mi_row + hbs < cm->mi_rows && (bsize > BLOCK_8X8 || unify_bsize)) {
        if (check_intra_b(&pc_tree->horizontal[1])) return 1;
      }
      break;
    case PARTITION_SPLIT:
      if (bsize == BLOCK_8X8 && !unify_bsize) {
        if (check_intra_b(pc_tree->leaf_split[0])) return 1;
      } else {
        if (check_intra_sb(cpi, tile, mi_row, mi_col, subsize,
                           pc_tree->split[0]))
          return 1;
        if (check_intra_sb(cpi, tile, mi_row, mi_col + hbs, subsize,
                           pc_tree->split[1]))
          return 1;
        if (check_intra_sb(cpi, tile, mi_row + hbs, mi_col, subsize,
                           pc_tree->split[2]))
          return 1;
        if (check_intra_sb(cpi, tile, mi_row + hbs, mi_col + hbs, subsize,
                           pc_tree->split[3]))
          return 1;
      }
      break;
#if CONFIG_EXT_PARTITION_TYPES
#if CONFIG_EXT_PARTITION_TYPES_AB
#error HORZ/VERT_A/B partitions not yet updated in superres code
#endif
    case PARTITION_HORZ_A:
      for (i = 0; i < 3; i++) {
        if (check_intra_b(&pc_tree->horizontala[i])) return 1;
      }
      break;
    case PARTITION_HORZ_B:
      for (i = 0; i < 3; i++) {
        if (check_intra_b(&pc_tree->horizontalb[i])) return 1;
      }
      break;
    case PARTITION_VERT_A:
      for (i = 0; i < 3; i++) {
        if (check_intra_b(&pc_tree->verticala[i])) return 1;
      }
      break;
    case PARTITION_VERT_B:
      for (i = 0; i < 3; i++) {
        if (check_intra_b(&pc_tree->verticalb[i])) return 1;
      }
      break;
#endif  // CONFIG_EXT_PARTITION_TYPES
    default: assert(0);
  }
  return 0;
}

static int check_supertx_b(TX_SIZE supertx_size, PICK_MODE_CONTEXT *ctx) {
  return ctx->mic.mbmi.tx_size == supertx_size;
}

static int check_supertx_sb(BLOCK_SIZE bsize, TX_SIZE supertx_size,
                            PC_TREE *pc_tree) {
  PARTITION_TYPE partition;
  BLOCK_SIZE subsize;
#if CONFIG_CB4X4
  const int unify_bsize = 1;
#else
  const int unify_bsize = 0;
#endif

  partition = pc_tree->partitioning;
  subsize = get_subsize(bsize, partition);
  switch (partition) {
    case PARTITION_NONE: return check_supertx_b(supertx_size, &pc_tree->none);
    case PARTITION_VERT:
      return check_supertx_b(supertx_size, &pc_tree->vertical[0]);
    case PARTITION_HORZ:
      return check_supertx_b(supertx_size, &pc_tree->horizontal[0]);
    case PARTITION_SPLIT:
      if (bsize == BLOCK_8X8 && !unify_bsize)
        return check_supertx_b(supertx_size, pc_tree->leaf_split[0]);
      else
        return check_supertx_sb(subsize, supertx_size, pc_tree->split[0]);
#if CONFIG_EXT_PARTITION_TYPES
#if CONFIG_EXT_PARTITION_TYPES_AB
#error HORZ/VERT_A/B partitions not yet updated in superres code
#endif
    case PARTITION_HORZ_A:
      return check_supertx_b(supertx_size, &pc_tree->horizontala[0]);
    case PARTITION_HORZ_B:
      return check_supertx_b(supertx_size, &pc_tree->horizontalb[0]);
    case PARTITION_VERT_A:
      return check_supertx_b(supertx_size, &pc_tree->verticala[0]);
    case PARTITION_VERT_B:
      return check_supertx_b(supertx_size, &pc_tree->verticalb[0]);
#endif  // CONFIG_EXT_PARTITION_TYPES
    default: assert(0); return 0;
  }
}

static void predict_superblock(const AV1_COMP *const cpi, ThreadData *td,
                               int mi_row_ori, int mi_col_ori, int mi_row_pred,
                               int mi_col_pred, int plane,
                               BLOCK_SIZE bsize_pred, int b_sub8x8, int block) {
  // Used in supertx
  // (mi_row_ori, mi_col_ori): location for mv
  // (mi_row_pred, mi_col_pred, bsize_pred): region to predict
  const AV1_COMMON *const cm = &cpi->common;
  MACROBLOCK *const x = &td->mb;
  MACROBLOCKD *const xd = &x->e_mbd;
  MODE_INFO *mi_8x8 = xd->mi[0];
  MODE_INFO *mi = mi_8x8;
  MB_MODE_INFO *mbmi = &mi->mbmi;
  int ref;
  const int is_compound = has_second_ref(mbmi);

  set_ref_ptrs(cm, xd, mbmi->ref_frame[0], mbmi->ref_frame[1]);

  for (ref = 0; ref < 1 + is_compound; ++ref) {
    YV12_BUFFER_CONFIG *cfg = get_ref_frame_buffer(cpi, mbmi->ref_frame[ref]);
    av1_setup_pre_planes(xd, ref, cfg, mi_row_pred, mi_col_pred,
                         &xd->block_refs[ref]->sf);
  }

#if CONFIG_COMPOUND_SINGLEREF
  // Single ref compound mode
  if (!is_compound && is_inter_singleref_comp_mode(mbmi->mode)) {
    xd->block_refs[1] = xd->block_refs[0];
    YV12_BUFFER_CONFIG *cfg = get_ref_frame_buffer(cpi, mbmi->ref_frame[0]);
    av1_setup_pre_planes(xd, 1, cfg, mi_row_pred, mi_col_pred,
                         &xd->block_refs[1]->sf);
  }
#endif  // CONFIG_COMPOUND_SINGLEREF

  if (!b_sub8x8)
    av1_build_inter_predictor_sb_extend(cm, xd, mi_row_ori, mi_col_ori,
                                        mi_row_pred, mi_col_pred, plane,
                                        bsize_pred);
  else
    av1_build_inter_predictor_sb_sub8x8_extend(cm, xd, mi_row_ori, mi_col_ori,
                                               mi_row_pred, mi_col_pred, plane,
                                               bsize_pred, block);
}

static void predict_b_extend(const AV1_COMP *const cpi, ThreadData *td,
                             const TileInfo *const tile, int block,
                             int mi_row_ori, int mi_col_ori, int mi_row_pred,
                             int mi_col_pred, int mi_row_top, int mi_col_top,
                             int plane, uint8_t *dst_buf, int dst_stride,
                             BLOCK_SIZE bsize_top, BLOCK_SIZE bsize_pred,
                             RUN_TYPE dry_run, int b_sub8x8) {
  // Used in supertx
  // (mi_row_ori, mi_col_ori): location for mv
  // (mi_row_pred, mi_col_pred, bsize_pred): region to predict
  // (mi_row_top, mi_col_top, bsize_top): region of the top partition size
  // block: sub location of sub8x8 blocks
  // b_sub8x8: 1: ori is sub8x8; 0: ori is not sub8x8
  // bextend: 1: region to predict is an extension of ori; 0: not

  MACROBLOCK *const x = &td->mb;
  const AV1_COMMON *const cm = &cpi->common;
  MACROBLOCKD *const xd = &x->e_mbd;
  int r = (mi_row_pred - mi_row_top) * MI_SIZE;
  int c = (mi_col_pred - mi_col_top) * MI_SIZE;
  const int mi_width_top = mi_size_wide[bsize_top];
  const int mi_height_top = mi_size_high[bsize_top];

  if (mi_row_pred < mi_row_top || mi_col_pred < mi_col_top ||
      mi_row_pred >= mi_row_top + mi_height_top ||
      mi_col_pred >= mi_col_top + mi_width_top || mi_row_pred >= cm->mi_rows ||
      mi_col_pred >= cm->mi_cols)
    return;

  set_offsets_extend(cpi, td, tile, mi_row_pred, mi_col_pred, mi_row_ori,
                     mi_col_ori, bsize_pred);
  xd->plane[plane].dst.stride = dst_stride;
  xd->plane[plane].dst.buf =
      dst_buf + (r >> xd->plane[plane].subsampling_y) * dst_stride +
      (c >> xd->plane[plane].subsampling_x);

  predict_superblock(cpi, td, mi_row_ori, mi_col_ori, mi_row_pred, mi_col_pred,
                     plane, bsize_pred, b_sub8x8, block);

  if (!dry_run && (plane == 0) && (block == 0 || !b_sub8x8))
    update_stats(&cpi->common, td, mi_row_pred, mi_col_pred, 1);
}

static void extend_dir(const AV1_COMP *const cpi, ThreadData *td,
                       const TileInfo *const tile, int block, BLOCK_SIZE bsize,
                       BLOCK_SIZE top_bsize, int mi_row_ori, int mi_col_ori,
                       int mi_row, int mi_col, int mi_row_top, int mi_col_top,
                       int plane, uint8_t *dst_buf, int dst_stride, int dir) {
  // dir: 0-lower, 1-upper, 2-left, 3-right
  //      4-lowerleft, 5-upperleft, 6-lowerright, 7-upperright
  MACROBLOCKD *xd = &td->mb.e_mbd;
  const int mi_width = mi_size_wide[bsize];
  const int mi_height = mi_size_high[bsize];
  int xss = xd->plane[1].subsampling_x;
  int yss = xd->plane[1].subsampling_y;
#if CONFIG_CB4X4
  const int unify_bsize = 1;
#else
  const int unify_bsize = 0;
#endif
  int b_sub8x8 = (bsize < BLOCK_8X8) && !unify_bsize ? 1 : 0;
  int wide_unit, high_unit;
  int i, j;
  int ext_offset = 0;

  BLOCK_SIZE extend_bsize;
  int mi_row_pred, mi_col_pred;

  if (dir == 0 || dir == 1) {  // lower and upper
    extend_bsize =
        (mi_width == mi_size_wide[BLOCK_8X8] || bsize < BLOCK_8X8 || xss < yss)
            ? BLOCK_8X8
            : BLOCK_16X8;

#if CONFIG_CB4X4
    if (bsize < BLOCK_8X8) {
      extend_bsize = BLOCK_4X4;
      ext_offset = mi_size_wide[BLOCK_8X8];
    }
#endif
    wide_unit = mi_size_wide[extend_bsize];
    high_unit = mi_size_high[extend_bsize];

    mi_row_pred = mi_row + ((dir == 0) ? mi_height : -(mi_height + ext_offset));
    mi_col_pred = mi_col;

    for (j = 0; j < mi_height + ext_offset; j += high_unit)
      for (i = 0; i < mi_width + ext_offset; i += wide_unit)
        predict_b_extend(cpi, td, tile, block, mi_row_ori, mi_col_ori,
                         mi_row_pred + j, mi_col_pred + i, mi_row_top,
                         mi_col_top, plane, dst_buf, dst_stride, top_bsize,
                         extend_bsize, 1, b_sub8x8);
  } else if (dir == 2 || dir == 3) {  // left and right
    extend_bsize =
        (mi_height == mi_size_high[BLOCK_8X8] || bsize < BLOCK_8X8 || yss < xss)
            ? BLOCK_8X8
            : BLOCK_8X16;
#if CONFIG_CB4X4
    if (bsize < BLOCK_8X8) {
      extend_bsize = BLOCK_4X4;
      ext_offset = mi_size_wide[BLOCK_8X8];
    }
#endif
    wide_unit = mi_size_wide[extend_bsize];
    high_unit = mi_size_high[extend_bsize];

    mi_row_pred = mi_row;
    mi_col_pred = mi_col + ((dir == 3) ? mi_width : -(mi_width + ext_offset));

    for (j = 0; j < mi_height + ext_offset; j += high_unit)
      for (i = 0; i < mi_width + ext_offset; i += wide_unit)
        predict_b_extend(cpi, td, tile, block, mi_row_ori, mi_col_ori,
                         mi_row_pred + j, mi_col_pred + i, mi_row_top,
                         mi_col_top, plane, dst_buf, dst_stride, top_bsize,
                         extend_bsize, 1, b_sub8x8);
  } else {
    extend_bsize = BLOCK_8X8;
#if CONFIG_CB4X4
    if (bsize < BLOCK_8X8) {
      extend_bsize = BLOCK_4X4;
      ext_offset = mi_size_wide[BLOCK_8X8];
    }
#endif
    wide_unit = mi_size_wide[extend_bsize];
    high_unit = mi_size_high[extend_bsize];

    mi_row_pred = mi_row + ((dir == 4 || dir == 6) ? mi_height
                                                   : -(mi_height + ext_offset));
    mi_col_pred =
        mi_col + ((dir == 6 || dir == 7) ? mi_width : -(mi_width + ext_offset));

    for (j = 0; j < mi_height + ext_offset; j += high_unit)
      for (i = 0; i < mi_width + ext_offset; i += wide_unit)
        predict_b_extend(cpi, td, tile, block, mi_row_ori, mi_col_ori,
                         mi_row_pred + j, mi_col_pred + i, mi_row_top,
                         mi_col_top, plane, dst_buf, dst_stride, top_bsize,
                         extend_bsize, 1, b_sub8x8);
  }
}

static void extend_all(const AV1_COMP *const cpi, ThreadData *td,
                       const TileInfo *const tile, int block, BLOCK_SIZE bsize,
                       BLOCK_SIZE top_bsize, int mi_row_ori, int mi_col_ori,
                       int mi_row, int mi_col, int mi_row_top, int mi_col_top,
                       int plane, uint8_t *dst_buf, int dst_stride) {
  assert(block >= 0 && block < 4);
  for (int i = 0; i < 8; ++i) {
    extend_dir(cpi, td, tile, block, bsize, top_bsize, mi_row_ori, mi_col_ori,
               mi_row, mi_col, mi_row_top, mi_col_top, plane, dst_buf,
               dst_stride, i);
  }
}

// This function generates prediction for multiple blocks, between which
// discontinuity around boundary is reduced by smoothing masks. The basic
// smoothing mask is a soft step function along horz/vert direction. In more
// complicated case when a block is split into 4 subblocks, the basic mask is
// first applied to neighboring subblocks (2 pairs) in horizontal direction and
// then applied to the 2 masked prediction mentioned above in vertical direction
// If the block is split into more than one level, at every stage, masked
// prediction is stored in dst_buf[] passed from higher level.
static void predict_sb_complex(const AV1_COMP *const cpi, ThreadData *td,
                               const TileInfo *const tile, int mi_row,
                               int mi_col, int mi_row_top, int mi_col_top,
                               RUN_TYPE dry_run, BLOCK_SIZE bsize,
                               BLOCK_SIZE top_bsize, uint8_t *dst_buf[3],
                               int dst_stride[3], PC_TREE *pc_tree) {
  const AV1_COMMON *const cm = &cpi->common;
  MACROBLOCK *const x = &td->mb;
  MACROBLOCKD *const xd = &x->e_mbd;
  const int hbs = mi_size_wide[bsize] / 2;
  const int is_partition_root = bsize >= BLOCK_8X8;
  const int ctx = is_partition_root
                      ? partition_plane_context(xd, mi_row, mi_col,
#if CONFIG_UNPOISON_PARTITION_CTX
                                                mi_row + hbs < cm->mi_rows,
                                                mi_col + hbs < cm->mi_cols,
#endif
                                                bsize)
                      : -1;
  const PARTITION_TYPE partition = pc_tree->partitioning;
  const BLOCK_SIZE subsize = get_subsize(bsize, partition);
#if CONFIG_EXT_PARTITION_TYPES
  const BLOCK_SIZE bsize2 = get_subsize(bsize, PARTITION_SPLIT);
#endif

  int i;
  uint8_t *dst_buf1[3], *dst_buf2[3], *dst_buf3[3];
  DECLARE_ALIGNED(16, uint8_t, tmp_buf1[MAX_MB_PLANE * MAX_TX_SQUARE * 2]);
  DECLARE_ALIGNED(16, uint8_t, tmp_buf2[MAX_MB_PLANE * MAX_TX_SQUARE * 2]);
  DECLARE_ALIGNED(16, uint8_t, tmp_buf3[MAX_MB_PLANE * MAX_TX_SQUARE * 2]);
  int dst_stride1[3] = { MAX_TX_SIZE, MAX_TX_SIZE, MAX_TX_SIZE };
  int dst_stride2[3] = { MAX_TX_SIZE, MAX_TX_SIZE, MAX_TX_SIZE };
  int dst_stride3[3] = { MAX_TX_SIZE, MAX_TX_SIZE, MAX_TX_SIZE };
#if CONFIG_CB4X4
  const int unify_bsize = 1;
#else
  const int unify_bsize = 0;
  assert(bsize >= BLOCK_8X8);
#endif

  if (mi_row >= cm->mi_rows || mi_col >= cm->mi_cols) return;

#if CONFIG_HIGHBITDEPTH
  if (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH) {
    int len = sizeof(uint16_t);
    dst_buf1[0] = CONVERT_TO_BYTEPTR(tmp_buf1);
    dst_buf1[1] = CONVERT_TO_BYTEPTR(tmp_buf1 + MAX_TX_SQUARE * len);
    dst_buf1[2] = CONVERT_TO_BYTEPTR(tmp_buf1 + 2 * MAX_TX_SQUARE * len);
    dst_buf2[0] = CONVERT_TO_BYTEPTR(tmp_buf2);
    dst_buf2[1] = CONVERT_TO_BYTEPTR(tmp_buf2 + MAX_TX_SQUARE * len);
    dst_buf2[2] = CONVERT_TO_BYTEPTR(tmp_buf2 + 2 * MAX_TX_SQUARE * len);
    dst_buf3[0] = CONVERT_TO_BYTEPTR(tmp_buf3);
    dst_buf3[1] = CONVERT_TO_BYTEPTR(tmp_buf3 + MAX_TX_SQUARE * len);
    dst_buf3[2] = CONVERT_TO_BYTEPTR(tmp_buf3 + 2 * MAX_TX_SQUARE * len);
  } else {
#endif  // CONFIG_HIGHBITDEPTH
    dst_buf1[0] = tmp_buf1;
    dst_buf1[1] = tmp_buf1 + MAX_TX_SQUARE;
    dst_buf1[2] = tmp_buf1 + 2 * MAX_TX_SQUARE;
    dst_buf2[0] = tmp_buf2;
    dst_buf2[1] = tmp_buf2 + MAX_TX_SQUARE;
    dst_buf2[2] = tmp_buf2 + 2 * MAX_TX_SQUARE;
    dst_buf3[0] = tmp_buf3;
    dst_buf3[1] = tmp_buf3 + MAX_TX_SQUARE;
    dst_buf3[2] = tmp_buf3 + 2 * MAX_TX_SQUARE;
#if CONFIG_HIGHBITDEPTH
  }
#endif  // CONFIG_HIGHBITDEPTH

  if (!dry_run && ctx >= 0 && bsize < top_bsize) {
    // Explicitly cast away const.
    FRAME_COUNTS *const frame_counts = (FRAME_COUNTS *)&cm->counts;
    frame_counts->partition[ctx][partition]++;
  }

  for (i = 0; i < MAX_MB_PLANE; i++) {
    xd->plane[i].dst.buf = dst_buf[i];
    xd->plane[i].dst.stride = dst_stride[i];
  }

  switch (partition) {
    case PARTITION_NONE:
      assert(bsize < top_bsize);
      for (i = 0; i < MAX_MB_PLANE; ++i) {
        predict_b_extend(cpi, td, tile, 0, mi_row, mi_col, mi_row, mi_col,
                         mi_row_top, mi_col_top, i, dst_buf[i], dst_stride[i],
                         top_bsize, bsize, dry_run, 0);
        extend_all(cpi, td, tile, 0, bsize, top_bsize, mi_row, mi_col, mi_row,
                   mi_col, mi_row_top, mi_col_top, i, dst_buf[i],
                   dst_stride[i]);
      }
      break;
    case PARTITION_HORZ:
      if (bsize == BLOCK_8X8 && !unify_bsize) {
        for (i = 0; i < MAX_MB_PLANE; ++i) {
          // First half
          predict_b_extend(cpi, td, tile, 0, mi_row, mi_col, mi_row, mi_col,
                           mi_row_top, mi_col_top, i, dst_buf[i], dst_stride[i],
                           top_bsize, BLOCK_8X8, dry_run, 1);
          if (bsize < top_bsize)
            extend_all(cpi, td, tile, 0, subsize, top_bsize, mi_row, mi_col,
                       mi_row, mi_col, mi_row_top, mi_col_top, i, dst_buf[i],
                       dst_stride[i]);

          // Second half
          predict_b_extend(cpi, td, tile, 2, mi_row, mi_col, mi_row, mi_col,
                           mi_row_top, mi_col_top, i, dst_buf1[i],
                           dst_stride1[i], top_bsize, BLOCK_8X8, dry_run, 1);
          if (bsize < top_bsize)
            extend_all(cpi, td, tile, 2, subsize, top_bsize, mi_row, mi_col,
                       mi_row, mi_col, mi_row_top, mi_col_top, i, dst_buf1[i],
                       dst_stride1[i]);
        }

        // Smooth
        xd->plane[0].dst.buf = dst_buf[0];
        xd->plane[0].dst.stride = dst_stride[0];
        av1_build_masked_inter_predictor_complex(
            xd, dst_buf[0], dst_stride[0], dst_buf1[0], dst_stride1[0], mi_row,
            mi_col, mi_row_top, mi_col_top, bsize, top_bsize, PARTITION_HORZ,
            0);
      } else {
        for (i = 0; i < MAX_MB_PLANE; ++i) {
#if CONFIG_CB4X4
          const struct macroblockd_plane *pd = &xd->plane[i];
          int handle_chroma_sub8x8 = need_handle_chroma_sub8x8(
              subsize, pd->subsampling_x, pd->subsampling_y);

          if (handle_chroma_sub8x8) {
            int mode_offset_row = CONFIG_CHROMA_SUB8X8 ? hbs : 0;

            predict_b_extend(cpi, td, tile, 0, mi_row + mode_offset_row, mi_col,
                             mi_row, mi_col, mi_row_top, mi_col_top, i,
                             dst_buf[i], dst_stride[i], top_bsize, bsize,
                             dry_run, 0);
            if (bsize < top_bsize)
              extend_all(cpi, td, tile, 0, bsize, top_bsize,
                         mi_row + mode_offset_row, mi_col, mi_row, mi_col,
                         mi_row_top, mi_col_top, i, dst_buf[i], dst_stride[i]);
          } else {
#endif
            // First half
            predict_b_extend(cpi, td, tile, 0, mi_row, mi_col, mi_row, mi_col,
                             mi_row_top, mi_col_top, i, dst_buf[i],
                             dst_stride[i], top_bsize, subsize, dry_run, 0);
            if (bsize < top_bsize)
              extend_all(cpi, td, tile, 0, subsize, top_bsize, mi_row, mi_col,
                         mi_row, mi_col, mi_row_top, mi_col_top, i, dst_buf[i],
                         dst_stride[i]);
            else
              extend_dir(cpi, td, tile, 0, subsize, top_bsize, mi_row, mi_col,
                         mi_row, mi_col, mi_row_top, mi_col_top, i, dst_buf[i],
                         dst_stride[i], 0);
            xd->plane[i].dst.buf = dst_buf[i];
            xd->plane[i].dst.stride = dst_stride[i];

            if (mi_row + hbs < cm->mi_rows) {
              // Second half
              predict_b_extend(cpi, td, tile, 0, mi_row + hbs, mi_col,
                               mi_row + hbs, mi_col, mi_row_top, mi_col_top, i,
                               dst_buf1[i], dst_stride1[i], top_bsize, subsize,
                               dry_run, 0);
              if (bsize < top_bsize)
                extend_all(cpi, td, tile, 0, subsize, top_bsize, mi_row + hbs,
                           mi_col, mi_row + hbs, mi_col, mi_row_top, mi_col_top,
                           i, dst_buf1[i], dst_stride1[i]);
              else
                extend_dir(cpi, td, tile, 0, subsize, top_bsize, mi_row + hbs,
                           mi_col, mi_row + hbs, mi_col, mi_row_top, mi_col_top,
                           i, dst_buf1[i], dst_stride1[i], 1);
              // Smooth
              xd->plane[i].dst.buf = dst_buf[i];
              xd->plane[i].dst.stride = dst_stride[i];
              av1_build_masked_inter_predictor_complex(
                  xd, dst_buf[i], dst_stride[i], dst_buf1[i], dst_stride1[i],
                  mi_row, mi_col, mi_row_top, mi_col_top, bsize, top_bsize,
                  PARTITION_HORZ, i);
            }
#if CONFIG_CB4X4
          }
#endif
        }
      }
      break;
    case PARTITION_VERT:
      if (bsize == BLOCK_8X8 && !unify_bsize) {
        for (i = 0; i < MAX_MB_PLANE; ++i) {
          // First half
          predict_b_extend(cpi, td, tile, 0, mi_row, mi_col, mi_row, mi_col,
                           mi_row_top, mi_col_top, i, dst_buf[i], dst_stride[i],
                           top_bsize, BLOCK_8X8, dry_run, 1);
          if (bsize < top_bsize)
            extend_all(cpi, td, tile, 0, subsize, top_bsize, mi_row, mi_col,
                       mi_row, mi_col, mi_row_top, mi_col_top, i, dst_buf[i],
                       dst_stride[i]);

          // Second half
          predict_b_extend(cpi, td, tile, 1, mi_row, mi_col, mi_row, mi_col,
                           mi_row_top, mi_col_top, i, dst_buf1[i],
                           dst_stride1[i], top_bsize, BLOCK_8X8, dry_run, 1);
          if (bsize < top_bsize)
            extend_all(cpi, td, tile, 1, subsize, top_bsize, mi_row, mi_col,
                       mi_row, mi_col, mi_row_top, mi_col_top, i, dst_buf1[i],
                       dst_stride1[i]);
        }

        // Smooth
        xd->plane[0].dst.buf = dst_buf[0];
        xd->plane[0].dst.stride = dst_stride[0];
        av1_build_masked_inter_predictor_complex(
            xd, dst_buf[0], dst_stride[0], dst_buf1[0], dst_stride1[0], mi_row,
            mi_col, mi_row_top, mi_col_top, bsize, top_bsize, PARTITION_VERT,
            0);
      } else {
        for (i = 0; i < MAX_MB_PLANE; ++i) {
#if CONFIG_CB4X4
          const struct macroblockd_plane *pd = &xd->plane[i];
          int handle_chroma_sub8x8 = need_handle_chroma_sub8x8(
              subsize, pd->subsampling_x, pd->subsampling_y);

          if (handle_chroma_sub8x8) {
            int mode_offset_col = CONFIG_CHROMA_SUB8X8 ? hbs : 0;

            predict_b_extend(cpi, td, tile, 0, mi_row, mi_col + mode_offset_col,
                             mi_row, mi_col, mi_row_top, mi_col_top, i,
                             dst_buf[i], dst_stride[i], top_bsize, bsize,
                             dry_run, 0);
            if (bsize < top_bsize)
              extend_all(cpi, td, tile, 0, bsize, top_bsize, mi_row,
                         mi_col + mode_offset_col, mi_row, mi_col, mi_row_top,
                         mi_col_top, i, dst_buf[i], dst_stride[i]);
          } else {
#endif
            predict_b_extend(cpi, td, tile, 0, mi_row, mi_col, mi_row, mi_col,
                             mi_row_top, mi_col_top, i, dst_buf[i],
                             dst_stride[i], top_bsize, subsize, dry_run, 0);
            if (bsize < top_bsize)
              extend_all(cpi, td, tile, 0, subsize, top_bsize, mi_row, mi_col,
                         mi_row, mi_col, mi_row_top, mi_col_top, i, dst_buf[i],
                         dst_stride[i]);
            else
              extend_dir(cpi, td, tile, 0, subsize, top_bsize, mi_row, mi_col,
                         mi_row, mi_col, mi_row_top, mi_col_top, i, dst_buf[i],
                         dst_stride[i], 3);
            xd->plane[i].dst.buf = dst_buf[i];
            xd->plane[i].dst.stride = dst_stride[i];

            if (mi_col + hbs < cm->mi_cols) {
              predict_b_extend(cpi, td, tile, 0, mi_row, mi_col + hbs, mi_row,
                               mi_col + hbs, mi_row_top, mi_col_top, i,
                               dst_buf1[i], dst_stride1[i], top_bsize, subsize,
                               dry_run, 0);
              if (bsize < top_bsize)
                extend_all(cpi, td, tile, 0, subsize, top_bsize, mi_row,
                           mi_col + hbs, mi_row, mi_col + hbs, mi_row_top,
                           mi_col_top, i, dst_buf1[i], dst_stride1[i]);
              else
                extend_dir(cpi, td, tile, 0, subsize, top_bsize, mi_row,
                           mi_col + hbs, mi_row, mi_col + hbs, mi_row_top,
                           mi_col_top, i, dst_buf1[i], dst_stride1[i], 2);

              // smooth
              xd->plane[i].dst.buf = dst_buf[i];
              xd->plane[i].dst.stride = dst_stride[i];
              av1_build_masked_inter_predictor_complex(
                  xd, dst_buf[i], dst_stride[i], dst_buf1[i], dst_stride1[i],
                  mi_row, mi_col, mi_row_top, mi_col_top, bsize, top_bsize,
                  PARTITION_VERT, i);
            }
#if CONFIG_CB4X4
          }
#endif
        }
      }
      break;
    case PARTITION_SPLIT:
      if (bsize == BLOCK_8X8 && !unify_bsize) {
        for (i = 0; i < MAX_MB_PLANE; i++) {
          predict_b_extend(cpi, td, tile, 0, mi_row, mi_col, mi_row, mi_col,
                           mi_row_top, mi_col_top, i, dst_buf[i], dst_stride[i],
                           top_bsize, BLOCK_8X8, dry_run, 1);
          predict_b_extend(cpi, td, tile, 1, mi_row, mi_col, mi_row, mi_col,
                           mi_row_top, mi_col_top, i, dst_buf1[i],
                           dst_stride1[i], top_bsize, BLOCK_8X8, dry_run, 1);
          predict_b_extend(cpi, td, tile, 2, mi_row, mi_col, mi_row, mi_col,
                           mi_row_top, mi_col_top, i, dst_buf2[i],
                           dst_stride2[i], top_bsize, BLOCK_8X8, dry_run, 1);
          predict_b_extend(cpi, td, tile, 3, mi_row, mi_col, mi_row, mi_col,
                           mi_row_top, mi_col_top, i, dst_buf3[i],
                           dst_stride3[i], top_bsize, BLOCK_8X8, dry_run, 1);

          if (bsize < top_bsize) {
            extend_all(cpi, td, tile, 0, subsize, top_bsize, mi_row, mi_col,
                       mi_row, mi_col, mi_row_top, mi_col_top, i, dst_buf[i],
                       dst_stride[i]);
            extend_all(cpi, td, tile, 1, subsize, top_bsize, mi_row, mi_col,
                       mi_row, mi_col, mi_row_top, mi_col_top, i, dst_buf1[i],
                       dst_stride1[i]);
            extend_all(cpi, td, tile, 2, subsize, top_bsize, mi_row, mi_col,
                       mi_row, mi_col, mi_row_top, mi_col_top, i, dst_buf2[i],
                       dst_stride2[i]);
            extend_all(cpi, td, tile, 3, subsize, top_bsize, mi_row, mi_col,
                       mi_row, mi_col, mi_row_top, mi_col_top, i, dst_buf3[i],
                       dst_stride3[i]);
          }
        }
#if CONFIG_CB4X4
      } else if (bsize == BLOCK_8X8) {
        for (i = 0; i < MAX_MB_PLANE; i++) {
          const struct macroblockd_plane *pd = &xd->plane[i];
          int handle_chroma_sub8x8 = need_handle_chroma_sub8x8(
              subsize, pd->subsampling_x, pd->subsampling_y);

          if (handle_chroma_sub8x8) {
            int mode_offset_row =
                CONFIG_CHROMA_SUB8X8 && mi_row + hbs < cm->mi_rows ? hbs : 0;
            int mode_offset_col =
                CONFIG_CHROMA_SUB8X8 && mi_col + hbs < cm->mi_cols ? hbs : 0;

            predict_b_extend(cpi, td, tile, 0, mi_row + mode_offset_row,
                             mi_col + mode_offset_col, mi_row, mi_col,
                             mi_row_top, mi_col_top, i, dst_buf[i],
                             dst_stride[i], top_bsize, BLOCK_8X8, dry_run, 0);
            if (bsize < top_bsize)
              extend_all(cpi, td, tile, 0, BLOCK_8X8, top_bsize,
                         mi_row + mode_offset_row, mi_col + mode_offset_col,
                         mi_row, mi_col, mi_row_top, mi_col_top, i, dst_buf[i],
                         dst_stride[i]);
          } else {
            predict_b_extend(cpi, td, tile, 0, mi_row, mi_col, mi_row, mi_col,
                             mi_row_top, mi_col_top, i, dst_buf[i],
                             dst_stride[i], top_bsize, subsize, dry_run, 0);
            if (mi_row < cm->mi_rows && mi_col + hbs < cm->mi_cols)
              predict_b_extend(cpi, td, tile, 0, mi_row, mi_col + hbs, mi_row,
                               mi_col + hbs, mi_row_top, mi_col_top, i,
                               dst_buf1[i], dst_stride1[i], top_bsize, subsize,
                               dry_run, 0);
            if (mi_row + hbs < cm->mi_rows && mi_col < cm->mi_cols)
              predict_b_extend(cpi, td, tile, 0, mi_row + hbs, mi_col,
                               mi_row + hbs, mi_col, mi_row_top, mi_col_top, i,
                               dst_buf2[i], dst_stride2[i], top_bsize, subsize,
                               dry_run, 0);
            if (mi_row + hbs < cm->mi_rows && mi_col + hbs < cm->mi_cols)
              predict_b_extend(cpi, td, tile, 0, mi_row + hbs, mi_col + hbs,
                               mi_row + hbs, mi_col + hbs, mi_row_top,
                               mi_col_top, i, dst_buf3[i], dst_stride3[i],
                               top_bsize, subsize, dry_run, 0);

            if (bsize < top_bsize) {
              extend_all(cpi, td, tile, 0, subsize, top_bsize, mi_row, mi_col,
                         mi_row, mi_col, mi_row_top, mi_col_top, i, dst_buf[i],
                         dst_stride[i]);
              if (mi_row < cm->mi_rows && mi_col + hbs < cm->mi_cols)
                extend_all(cpi, td, tile, 0, subsize, top_bsize, mi_row,
                           mi_col + hbs, mi_row, mi_col + hbs, mi_row_top,
                           mi_col_top, i, dst_buf1[i], dst_stride1[i]);
              if (mi_row + hbs < cm->mi_rows && mi_col < cm->mi_cols)
                extend_all(cpi, td, tile, 0, subsize, top_bsize, mi_row + hbs,
                           mi_col, mi_row + hbs, mi_col, mi_row_top, mi_col_top,
                           i, dst_buf2[i], dst_stride2[i]);
              if (mi_row + hbs < cm->mi_rows && mi_col + hbs < cm->mi_cols)
                extend_all(cpi, td, tile, 0, subsize, top_bsize, mi_row + hbs,
                           mi_col + hbs, mi_row + hbs, mi_col + hbs, mi_row_top,
                           mi_col_top, i, dst_buf3[i], dst_stride3[i]);
            }
          }
        }
#endif
      } else {
        predict_sb_complex(cpi, td, tile, mi_row, mi_col, mi_row_top,
                           mi_col_top, dry_run, subsize, top_bsize, dst_buf,
                           dst_stride, pc_tree->split[0]);
        if (mi_row < cm->mi_rows && mi_col + hbs < cm->mi_cols)
          predict_sb_complex(cpi, td, tile, mi_row, mi_col + hbs, mi_row_top,
                             mi_col_top, dry_run, subsize, top_bsize, dst_buf1,
                             dst_stride1, pc_tree->split[1]);
        if (mi_row + hbs < cm->mi_rows && mi_col < cm->mi_cols)
          predict_sb_complex(cpi, td, tile, mi_row + hbs, mi_col, mi_row_top,
                             mi_col_top, dry_run, subsize, top_bsize, dst_buf2,
                             dst_stride2, pc_tree->split[2]);
        if (mi_row + hbs < cm->mi_rows && mi_col + hbs < cm->mi_cols)
          predict_sb_complex(cpi, td, tile, mi_row + hbs, mi_col + hbs,
                             mi_row_top, mi_col_top, dry_run, subsize,
                             top_bsize, dst_buf3, dst_stride3,
                             pc_tree->split[3]);
      }
      for (i = 0; i < MAX_MB_PLANE; i++) {
#if CONFIG_CB4X4
        const struct macroblockd_plane *pd = &xd->plane[i];
        int handle_chroma_sub8x8 = need_handle_chroma_sub8x8(
            subsize, pd->subsampling_x, pd->subsampling_y);
        if (handle_chroma_sub8x8) continue;  // Skip <4x4 chroma smoothing
#else
        if (bsize == BLOCK_8X8 && i != 0)
          continue;  // Skip <4x4 chroma smoothing
#endif

        if (mi_row < cm->mi_rows && mi_col + hbs < cm->mi_cols) {
          av1_build_masked_inter_predictor_complex(
              xd, dst_buf[i], dst_stride[i], dst_buf1[i], dst_stride1[i],
              mi_row, mi_col, mi_row_top, mi_col_top, bsize, top_bsize,
              PARTITION_VERT, i);
          if (mi_row + hbs < cm->mi_rows) {
            av1_build_masked_inter_predictor_complex(
                xd, dst_buf2[i], dst_stride2[i], dst_buf3[i], dst_stride3[i],
                mi_row, mi_col, mi_row_top, mi_col_top, bsize, top_bsize,
                PARTITION_VERT, i);
            av1_build_masked_inter_predictor_complex(
                xd, dst_buf[i], dst_stride[i], dst_buf2[i], dst_stride2[i],
                mi_row, mi_col, mi_row_top, mi_col_top, bsize, top_bsize,
                PARTITION_HORZ, i);
          }
        } else if (mi_row + hbs < cm->mi_rows && mi_col < cm->mi_cols) {
          av1_build_masked_inter_predictor_complex(
              xd, dst_buf[i], dst_stride[i], dst_buf2[i], dst_stride2[i],
              mi_row, mi_col, mi_row_top, mi_col_top, bsize, top_bsize,
              PARTITION_HORZ, i);
        }
      }
      break;
#if CONFIG_EXT_PARTITION_TYPES
#if CONFIG_EXT_PARTITION_TYPES_AB
#error HORZ/VERT_A/B partitions not yet updated in superres code
#endif
    case PARTITION_HORZ_A:
      predict_b_extend(cpi, td, tile, 0, mi_row, mi_col, mi_row, mi_col,
                       mi_row_top, mi_col_top, dst_buf, dst_stride, top_bsize,
                       bsize2, dry_run, 0, 0);
      extend_all(cpi, td, tile, 0, bsize2, top_bsize, mi_row, mi_col,
                 mi_row_top, mi_col_top, dry_run, dst_buf, dst_stride);

      predict_b_extend(cpi, td, tile, 0, mi_row, mi_col + hbs, mi_row,
                       mi_col + hbs, mi_row_top, mi_col_top, dst_buf1,
                       dst_stride1, top_bsize, bsize2, dry_run, 0, 0);
      extend_all(cpi, td, tile, 0, bsize2, top_bsize, mi_row, mi_col + hbs,
                 mi_row_top, mi_col_top, dry_run, dst_buf1, dst_stride1);

      predict_b_extend(cpi, td, tile, 0, mi_row + hbs, mi_col, mi_row + hbs,
                       mi_col, mi_row_top, mi_col_top, dst_buf2, dst_stride2,
                       top_bsize, subsize, dry_run, 0, 0);
      if (bsize < top_bsize)
        extend_all(cpi, td, tile, 0, subsize, top_bsize, mi_row + hbs, mi_col,
                   mi_row_top, mi_col_top, dry_run, dst_buf2, dst_stride2);
      else
        extend_dir(cpi, td, tile, 0, subsize, top_bsize, mi_row + hbs, mi_col,
                   mi_row_top, mi_col_top, dry_run, dst_buf2, dst_stride2, 1);

      for (i = 0; i < MAX_MB_PLANE; i++) {
        xd->plane[i].dst.buf = dst_buf[i];
        xd->plane[i].dst.stride = dst_stride[i];
        av1_build_masked_inter_predictor_complex(
            xd, dst_buf[i], dst_stride[i], dst_buf1[i], dst_stride1[i], mi_row,
            mi_col, mi_row_top, mi_col_top, bsize, top_bsize, PARTITION_VERT,
            i);
      }
      for (i = 0; i < MAX_MB_PLANE; i++) {
        av1_build_masked_inter_predictor_complex(
            xd, dst_buf[i], dst_stride[i], dst_buf2[i], dst_stride2[i], mi_row,
            mi_col, mi_row_top, mi_col_top, bsize, top_bsize, PARTITION_HORZ,
            i);
      }

      break;
    case PARTITION_VERT_A:

      predict_b_extend(cpi, td, tile, 0, mi_row, mi_col, mi_row, mi_col,
                       mi_row_top, mi_col_top, dst_buf, dst_stride, top_bsize,
                       bsize2, dry_run, 0, 0);
      extend_all(cpi, td, tile, 0, bsize2, top_bsize, mi_row, mi_col,
                 mi_row_top, mi_col_top, dry_run, dst_buf, dst_stride);

      predict_b_extend(cpi, td, tile, 0, mi_row + hbs, mi_col, mi_row + hbs,
                       mi_col, mi_row_top, mi_col_top, dst_buf1, dst_stride1,
                       top_bsize, bsize2, dry_run, 0, 0);
      extend_all(cpi, td, tile, 0, bsize2, top_bsize, mi_row + hbs, mi_col,
                 mi_row_top, mi_col_top, dry_run, dst_buf1, dst_stride1);

      predict_b_extend(cpi, td, tile, 0, mi_row, mi_col + hbs, mi_row,
                       mi_col + hbs, mi_row_top, mi_col_top, dst_buf2,
                       dst_stride2, top_bsize, subsize, dry_run, 0, 0);
      if (bsize < top_bsize)
        extend_all(cpi, td, tile, 0, subsize, top_bsize, mi_row, mi_col + hbs,
                   mi_row_top, mi_col_top, dry_run, dst_buf2, dst_stride2);
      else
        extend_dir(cpi, td, tile, 0, subsize, top_bsize, mi_row, mi_col + hbs,
                   mi_row_top, mi_col_top, dry_run, dst_buf2, dst_stride2, 2);

      for (i = 0; i < MAX_MB_PLANE; i++) {
        xd->plane[i].dst.buf = dst_buf[i];
        xd->plane[i].dst.stride = dst_stride[i];
        av1_build_masked_inter_predictor_complex(
            xd, dst_buf[i], dst_stride[i], dst_buf1[i], dst_stride1[i], mi_row,
            mi_col, mi_row_top, mi_col_top, bsize, top_bsize, PARTITION_HORZ,
            i);
      }
      for (i = 0; i < MAX_MB_PLANE; i++) {
        av1_build_masked_inter_predictor_complex(
            xd, dst_buf[i], dst_stride[i], dst_buf2[i], dst_stride2[i], mi_row,
            mi_col, mi_row_top, mi_col_top, bsize, top_bsize, PARTITION_VERT,
            i);
      }
      break;
    case PARTITION_HORZ_B:

      predict_b_extend(cpi, td, tile, 0, mi_row, mi_col, mi_row, mi_col,
                       mi_row_top, mi_col_top, dst_buf, dst_stride, top_bsize,
                       subsize, dry_run, 0, 0);
      if (bsize < top_bsize)
        extend_all(cpi, td, tile, 0, subsize, top_bsize, mi_row, mi_col,
                   mi_row_top, mi_col_top, dry_run, dst_buf, dst_stride);
      else
        extend_dir(cpi, td, tile, 0, subsize, top_bsize, mi_row, mi_col,
                   mi_row_top, mi_col_top, dry_run, dst_buf, dst_stride, 0);

      predict_b_extend(cpi, td, tile, 0, mi_row + hbs, mi_col, mi_row + hbs,
                       mi_col, mi_row_top, mi_col_top, dst_buf1, dst_stride1,
                       top_bsize, bsize2, dry_run, 0, 0);
      extend_all(cpi, td, tile, 0, bsize2, top_bsize, mi_row + hbs, mi_col,
                 mi_row_top, mi_col_top, dry_run, dst_buf1, dst_stride1);

      predict_b_extend(cpi, td, tile, 0, mi_row + hbs, mi_col + hbs,
                       mi_row + hbs, mi_col + hbs, mi_row_top, mi_col_top,
                       dst_buf2, dst_stride2, top_bsize, bsize2, dry_run, 0, 0);
      extend_all(cpi, td, tile, 0, bsize2, top_bsize, mi_row + hbs,
                 mi_col + hbs, mi_row_top, mi_col_top, dry_run, dst_buf2,
                 dst_stride2);

      for (i = 0; i < MAX_MB_PLANE; i++) {
        xd->plane[i].dst.buf = dst_buf1[i];
        xd->plane[i].dst.stride = dst_stride1[i];
        av1_build_masked_inter_predictor_complex(
            xd, dst_buf1[i], dst_stride1[i], dst_buf2[i], dst_stride2[i],
            mi_row, mi_col, mi_row_top, mi_col_top, bsize, top_bsize,
            PARTITION_VERT, i);
      }
      for (i = 0; i < MAX_MB_PLANE; i++) {
        xd->plane[i].dst.buf = dst_buf[i];
        xd->plane[i].dst.stride = dst_stride[i];
        av1_build_masked_inter_predictor_complex(
            xd, dst_buf[i], dst_stride[i], dst_buf1[i], dst_stride1[i], mi_row,
            mi_col, mi_row_top, mi_col_top, bsize, top_bsize, PARTITION_HORZ,
            i);
      }
      break;
    case PARTITION_VERT_B:

      predict_b_extend(cpi, td, tile, 0, mi_row, mi_col, mi_row, mi_col,
                       mi_row_top, mi_col_top, dst_buf, dst_stride, top_bsize,
                       subsize, dry_run, 0, 0);
      if (bsize < top_bsize)
        extend_all(cpi, td, tile, 0, subsize, top_bsize, mi_row, mi_col,
                   mi_row_top, mi_col_top, dry_run, dst_buf, dst_stride);
      else
        extend_dir(cpi, td, tile, 0, subsize, top_bsize, mi_row, mi_col,
                   mi_row_top, mi_col_top, dry_run, dst_buf, dst_stride, 3);

      predict_b_extend(cpi, td, tile, 0, mi_row, mi_col + hbs, mi_row,
                       mi_col + hbs, mi_row_top, mi_col_top, dst_buf1,
                       dst_stride1, top_bsize, bsize2, dry_run, 0, 0);
      extend_all(cpi, td, tile, 0, bsize2, top_bsize, mi_row, mi_col + hbs,
                 mi_row_top, mi_col_top, dry_run, dst_buf1, dst_stride1);

      predict_b_extend(cpi, td, tile, 0, mi_row + hbs, mi_col + hbs,
                       mi_row + hbs, mi_col + hbs, mi_row_top, mi_col_top,
                       dst_buf2, dst_stride2, top_bsize, bsize2, dry_run, 0, 0);
      extend_all(cpi, td, tile, 0, bsize2, top_bsize, mi_row + hbs,
                 mi_col + hbs, mi_row_top, mi_col_top, dry_run, dst_buf2,
                 dst_stride2);

      for (i = 0; i < MAX_MB_PLANE; i++) {
        xd->plane[i].dst.buf = dst_buf1[i];
        xd->plane[i].dst.stride = dst_stride1[i];
        av1_build_masked_inter_predictor_complex(
            xd, dst_buf1[i], dst_stride1[i], dst_buf2[i], dst_stride2[i],
            mi_row, mi_col, mi_row_top, mi_col_top, bsize, top_bsize,
            PARTITION_HORZ, i);
      }
      for (i = 0; i < MAX_MB_PLANE; i++) {
        xd->plane[i].dst.buf = dst_buf[i];
        xd->plane[i].dst.stride = dst_stride[i];
        av1_build_masked_inter_predictor_complex(
            xd, dst_buf[i], dst_stride[i], dst_buf1[i], dst_stride1[i], mi_row,
            mi_col, mi_row_top, mi_col_top, bsize, top_bsize, PARTITION_VERT,
            i);
      }
      break;
#endif  // CONFIG_EXT_PARTITION_TYPES
    default: assert(0);
  }

#if CONFIG_EXT_PARTITION_TYPES
  if (bsize < top_bsize)
    update_ext_partition_context(xd, mi_row, mi_col, subsize, bsize, partition);
#else
  if (bsize < top_bsize && (partition != PARTITION_SPLIT || bsize == BLOCK_8X8))
    update_partition_context(xd, mi_row, mi_col, subsize, bsize);
#endif  // CONFIG_EXT_PARTITION_TYPES
}

static void rd_supertx_sb(const AV1_COMP *const cpi, ThreadData *td,
                          const TileInfo *const tile, int mi_row, int mi_col,
                          BLOCK_SIZE bsize, int *tmp_rate, int64_t *tmp_dist,
                          TX_TYPE *best_tx, PC_TREE *pc_tree) {
  const AV1_COMMON *const cm = &cpi->common;
  MACROBLOCK *const x = &td->mb;
  MACROBLOCKD *const xd = &x->e_mbd;
  int plane, pnskip, skippable, skippable_uv, rate_uv, this_rate,
      base_rate = *tmp_rate;
  int64_t sse, pnsse, sse_uv, this_dist, dist_uv;
  uint8_t *dst_buf[3];
  int dst_stride[3];
  TX_SIZE tx_size;
  MB_MODE_INFO *mbmi;
  TX_TYPE tx_type, best_tx_nostx;
  int tmp_rate_tx = 0, skip_tx = 0;
  int64_t tmp_dist_tx = 0, rd_tx, bestrd_tx = INT64_MAX;

  set_skip_context(xd, mi_row, mi_col);
  set_mode_info_offsets(cpi, x, xd, mi_row, mi_col);
  update_state_sb_supertx(cpi, td, tile, mi_row, mi_col, bsize, 1, pc_tree);
  av1_setup_dst_planes(xd->plane, bsize, get_frame_new_buffer(cm), mi_row,
                       mi_col);
  for (plane = 0; plane < MAX_MB_PLANE; plane++) {
    dst_buf[plane] = xd->plane[plane].dst.buf;
    dst_stride[plane] = xd->plane[plane].dst.stride;
  }
  predict_sb_complex(cpi, td, tile, mi_row, mi_col, mi_row, mi_col, 1, bsize,
                     bsize, dst_buf, dst_stride, pc_tree);

  set_offsets_without_segment_id(cpi, tile, x, mi_row, mi_col, bsize);
  set_segment_id_supertx(cpi, x, mi_row, mi_col, bsize);

  mbmi = &xd->mi[0]->mbmi;
  best_tx_nostx = mbmi->tx_type;

  *best_tx = DCT_DCT;

  // chroma
  skippable_uv = 1;
  rate_uv = 0;
  dist_uv = 0;
  sse_uv = 0;
  for (plane = 1; plane < MAX_MB_PLANE; ++plane) {
#if CONFIG_VAR_TX
    ENTROPY_CONTEXT ctxa[2 * MAX_MIB_SIZE];
    ENTROPY_CONTEXT ctxl[2 * MAX_MIB_SIZE];
    const struct macroblockd_plane *const pd = &xd->plane[plane];
    RD_STATS this_rd_stats;
    av1_init_rd_stats(&this_rd_stats);

    tx_size = max_txsize_lookup[bsize];
    tx_size =
        uv_txsize_lookup[bsize][tx_size][cm->subsampling_x][cm->subsampling_y];
    av1_get_entropy_contexts(bsize, tx_size, pd, ctxa, ctxl);

    av1_subtract_plane(x, bsize, plane);
    av1_tx_block_rd_b(cpi, x, tx_size, 0, 0, plane, 0,
                      get_plane_block_size(bsize, pd), &ctxa[0], &ctxl[0],
                      &this_rd_stats);

    this_rate = this_rd_stats.rate;
    this_dist = this_rd_stats.dist;
    pnsse = this_rd_stats.sse;
    pnskip = this_rd_stats.skip;
#else
    tx_size = max_txsize_lookup[bsize];
    tx_size =
        uv_txsize_lookup[bsize][tx_size][cm->subsampling_x][cm->subsampling_y];
    av1_subtract_plane(x, bsize, plane);
    av1_txfm_rd_in_plane_supertx(x, cpi, &this_rate, &this_dist, &pnskip,
                                 &pnsse, INT64_MAX, plane, bsize, tx_size, 0);
#endif  // CONFIG_VAR_TX

    rate_uv += this_rate;
    dist_uv += this_dist;
    sse_uv += pnsse;
    skippable_uv &= pnskip;
  }

  // luma
  tx_size = max_txsize_lookup[bsize];
  av1_subtract_plane(x, bsize, 0);
#if CONFIG_EXT_TX
  int ext_tx_set = get_ext_tx_set(tx_size, bsize, 1, cm->reduced_tx_set_used);
  const TxSetType tx_set_type =
      get_ext_tx_set_type(tx_size, bsize, 1, cm->reduced_tx_set_used);
#endif  // CONFIG_EXT_TX
  for (tx_type = DCT_DCT; tx_type < TX_TYPES; ++tx_type) {
#if CONFIG_VAR_TX
    ENTROPY_CONTEXT ctxa[2 * MAX_MIB_SIZE];
    ENTROPY_CONTEXT ctxl[2 * MAX_MIB_SIZE];
    const struct macroblockd_plane *const pd = &xd->plane[0];
    RD_STATS this_rd_stats;
#endif  // CONFIG_VAR_TX

#if CONFIG_EXT_TX
    if (!av1_ext_tx_used[tx_set_type][tx_type]) continue;
#else
    if (tx_size >= TX_32X32 && tx_type != DCT_DCT) continue;
#endif  // CONFIG_EXT_TX
    mbmi->tx_type = tx_type;

#if CONFIG_VAR_TX
    av1_init_rd_stats(&this_rd_stats);
    av1_get_entropy_contexts(bsize, tx_size, pd, ctxa, ctxl);
    av1_tx_block_rd_b(cpi, x, tx_size, 0, 0, 0, 0, bsize, &ctxa[0], &ctxl[0],
                      &this_rd_stats);

    this_rate = this_rd_stats.rate;
    this_dist = this_rd_stats.dist;
    pnsse = this_rd_stats.sse;
    pnskip = this_rd_stats.skip;
#else
    av1_txfm_rd_in_plane_supertx(x, cpi, &this_rate, &this_dist, &pnskip,
                                 &pnsse, INT64_MAX, 0, bsize, tx_size, 0);
#endif  // CONFIG_VAR_TX

#if CONFIG_EXT_TX
    if (get_ext_tx_types(tx_size, bsize, 1, cm->reduced_tx_set_used) > 1 &&
        !xd->lossless[xd->mi[0]->mbmi.segment_id] && this_rate != INT_MAX) {
      if (ext_tx_set > 0)
        this_rate +=
            x->inter_tx_type_costs[ext_tx_set][mbmi->tx_size][mbmi->tx_type];
    }
#else
    if (tx_size < TX_32X32 && !xd->lossless[xd->mi[0]->mbmi.segment_id] &&
        this_rate != INT_MAX) {
      this_rate += x->inter_tx_type_costs[tx_size][mbmi->tx_type];
    }
#endif  // CONFIG_EXT_TX
    *tmp_rate = rate_uv + this_rate;
    *tmp_dist = dist_uv + this_dist;
    sse = sse_uv + pnsse;
    skippable = skippable_uv && pnskip;
    if (skippable) {
      *tmp_rate = av1_cost_bit(av1_get_skip_prob(cm, xd), 1);
      x->skip = 1;
    } else {
      if (RDCOST(x->rdmult, *tmp_rate, *tmp_dist) < RDCOST(x->rdmult, 0, sse)) {
        *tmp_rate += av1_cost_bit(av1_get_skip_prob(cm, xd), 0);
        x->skip = 0;
      } else {
        *tmp_dist = sse;
        *tmp_rate = av1_cost_bit(av1_get_skip_prob(cm, xd), 1);
        x->skip = 1;
      }
    }
    *tmp_rate += base_rate;
    rd_tx = RDCOST(x->rdmult, *tmp_rate, *tmp_dist);
    if (rd_tx < bestrd_tx * 0.99 || tx_type == DCT_DCT) {
      *best_tx = tx_type;
      bestrd_tx = rd_tx;
      tmp_rate_tx = *tmp_rate;
      tmp_dist_tx = *tmp_dist;
      skip_tx = x->skip;
    }
  }
  *tmp_rate = tmp_rate_tx;
  *tmp_dist = tmp_dist_tx;
  x->skip = skip_tx;
#if CONFIG_VAR_TX
  for (plane = 0; plane < 1; ++plane)
    memset(x->blk_skip[plane], x->skip,
           sizeof(uint8_t) * pc_tree->none.num_4x4_blk);
#endif  // CONFIG_VAR_TX
  xd->mi[0]->mbmi.tx_type = best_tx_nostx;
}
#endif  // CONFIG_SUPERTX
