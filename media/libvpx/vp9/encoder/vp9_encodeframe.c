/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <limits.h>
#include <math.h>
#include <stdio.h>

#include "./vp9_rtcd.h"
#include "./vpx_config.h"

#include "vpx_ports/vpx_timer.h"

#include "vp9/common/vp9_common.h"
#include "vp9/common/vp9_entropy.h"
#include "vp9/common/vp9_entropymode.h"
#include "vp9/common/vp9_idct.h"
#include "vp9/common/vp9_mvref_common.h"
#include "vp9/common/vp9_pred_common.h"
#include "vp9/common/vp9_quant_common.h"
#include "vp9/common/vp9_reconintra.h"
#include "vp9/common/vp9_reconinter.h"
#include "vp9/common/vp9_seg_common.h"
#include "vp9/common/vp9_systemdependent.h"
#include "vp9/common/vp9_tile_common.h"

#include "vp9/encoder/vp9_aq_complexity.h"
#include "vp9/encoder/vp9_aq_cyclicrefresh.h"
#include "vp9/encoder/vp9_aq_variance.h"
#include "vp9/encoder/vp9_encodeframe.h"
#include "vp9/encoder/vp9_encodemb.h"
#include "vp9/encoder/vp9_encodemv.h"
#include "vp9/encoder/vp9_extend.h"
#include "vp9/encoder/vp9_pickmode.h"
#include "vp9/encoder/vp9_rd.h"
#include "vp9/encoder/vp9_rdopt.h"
#include "vp9/encoder/vp9_segmentation.h"
#include "vp9/encoder/vp9_tokenize.h"

#define GF_ZEROMV_ZBIN_BOOST 0
#define LF_ZEROMV_ZBIN_BOOST 0
#define MV_ZBIN_BOOST        0
#define SPLIT_MV_ZBIN_BOOST  0
#define INTRA_ZBIN_BOOST     0

static void encode_superblock(VP9_COMP *cpi, TOKENEXTRA **t, int output_enabled,
                              int mi_row, int mi_col, BLOCK_SIZE bsize,
                              PICK_MODE_CONTEXT *ctx);

// Motion vector component magnitude threshold for defining fast motion.
#define FAST_MOTION_MV_THRESH 24

// This is used as a reference when computing the source variance for the
//  purposes of activity masking.
// Eventually this should be replaced by custom no-reference routines,
//  which will be faster.
static const uint8_t VP9_VAR_OFFS[64] = {
  128, 128, 128, 128, 128, 128, 128, 128,
  128, 128, 128, 128, 128, 128, 128, 128,
  128, 128, 128, 128, 128, 128, 128, 128,
  128, 128, 128, 128, 128, 128, 128, 128,
  128, 128, 128, 128, 128, 128, 128, 128,
  128, 128, 128, 128, 128, 128, 128, 128,
  128, 128, 128, 128, 128, 128, 128, 128,
  128, 128, 128, 128, 128, 128, 128, 128
};

static unsigned int get_sby_perpixel_variance(VP9_COMP *cpi,
                                              const struct buf_2d *ref,
                                              BLOCK_SIZE bs) {
  unsigned int sse;
  const unsigned int var = cpi->fn_ptr[bs].vf(ref->buf, ref->stride,
                                              VP9_VAR_OFFS, 0, &sse);
  return ROUND_POWER_OF_TWO(var, num_pels_log2_lookup[bs]);
}

static unsigned int get_sby_perpixel_diff_variance(VP9_COMP *cpi,
                                                   const struct buf_2d *ref,
                                                   int mi_row, int mi_col,
                                                   BLOCK_SIZE bs) {
  const YV12_BUFFER_CONFIG *last = get_ref_frame_buffer(cpi, LAST_FRAME);
  const uint8_t* last_y = &last->y_buffer[mi_row * MI_SIZE * last->y_stride +
                                              mi_col * MI_SIZE];
  unsigned int sse;
  const unsigned int var = cpi->fn_ptr[bs].vf(ref->buf, ref->stride,
                                              last_y, last->y_stride, &sse);
  return ROUND_POWER_OF_TWO(var, num_pels_log2_lookup[bs]);
}

static BLOCK_SIZE get_rd_var_based_fixed_partition(VP9_COMP *cpi,
                                                   int mi_row,
                                                   int mi_col) {
  unsigned int var = get_sby_perpixel_diff_variance(cpi, &cpi->mb.plane[0].src,
                                                    mi_row, mi_col,
                                                    BLOCK_64X64);
  if (var < 8)
    return BLOCK_64X64;
  else if (var < 128)
    return BLOCK_32X32;
  else if (var < 2048)
    return BLOCK_16X16;
  else
    return BLOCK_8X8;
}

static BLOCK_SIZE get_nonrd_var_based_fixed_partition(VP9_COMP *cpi,
                                                      int mi_row,
                                                      int mi_col) {
  unsigned int var = get_sby_perpixel_diff_variance(cpi, &cpi->mb.plane[0].src,
                                                    mi_row, mi_col,
                                                    BLOCK_64X64);
  if (var < 4)
    return BLOCK_64X64;
  else if (var < 10)
    return BLOCK_32X32;
  else
    return BLOCK_16X16;
}

// Lighter version of set_offsets that only sets the mode info
// pointers.
static INLINE void set_modeinfo_offsets(VP9_COMMON *const cm,
                                        MACROBLOCKD *const xd,
                                        int mi_row,
                                        int mi_col) {
  const int idx_str = xd->mi_stride * mi_row + mi_col;
  xd->mi = cm->mi_grid_visible + idx_str;
  xd->mi[0] = cm->mi + idx_str;
}

static void set_offsets(VP9_COMP *cpi, const TileInfo *const tile,
                        int mi_row, int mi_col, BLOCK_SIZE bsize) {
  MACROBLOCK *const x = &cpi->mb;
  VP9_COMMON *const cm = &cpi->common;
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *mbmi;
  const int mi_width = num_8x8_blocks_wide_lookup[bsize];
  const int mi_height = num_8x8_blocks_high_lookup[bsize];
  const struct segmentation *const seg = &cm->seg;

  set_skip_context(xd, mi_row, mi_col);

  set_modeinfo_offsets(cm, xd, mi_row, mi_col);

  mbmi = &xd->mi[0]->mbmi;

  // Set up destination pointers.
  vp9_setup_dst_planes(xd->plane, get_frame_new_buffer(cm), mi_row, mi_col);

  // Set up limit values for MV components.
  // Mv beyond the range do not produce new/different prediction block.
  x->mv_row_min = -(((mi_row + mi_height) * MI_SIZE) + VP9_INTERP_EXTEND);
  x->mv_col_min = -(((mi_col + mi_width) * MI_SIZE) + VP9_INTERP_EXTEND);
  x->mv_row_max = (cm->mi_rows - mi_row) * MI_SIZE + VP9_INTERP_EXTEND;
  x->mv_col_max = (cm->mi_cols - mi_col) * MI_SIZE + VP9_INTERP_EXTEND;

  // Set up distance of MB to edge of frame in 1/8th pel units.
  assert(!(mi_col & (mi_width - 1)) && !(mi_row & (mi_height - 1)));
  set_mi_row_col(xd, tile, mi_row, mi_height, mi_col, mi_width,
                 cm->mi_rows, cm->mi_cols);

  // Set up source buffers.
  vp9_setup_src_planes(x, cpi->Source, mi_row, mi_col);

  // R/D setup.
  x->rddiv = cpi->rd.RDDIV;
  x->rdmult = cpi->rd.RDMULT;

  // Setup segment ID.
  if (seg->enabled) {
    if (cpi->oxcf.aq_mode != VARIANCE_AQ) {
      const uint8_t *const map = seg->update_map ? cpi->segmentation_map
                                                 : cm->last_frame_seg_map;
      mbmi->segment_id = vp9_get_segment_id(cm, map, bsize, mi_row, mi_col);
    }
    vp9_init_plane_quantizers(cpi, x);

    x->encode_breakout = cpi->segment_encode_breakout[mbmi->segment_id];
  } else {
    mbmi->segment_id = 0;
    x->encode_breakout = cpi->encode_breakout;
  }
}

static void duplicate_mode_info_in_sb(VP9_COMMON *cm, MACROBLOCKD *xd,
                                      int mi_row, int mi_col,
                                      BLOCK_SIZE bsize) {
  const int block_width = num_8x8_blocks_wide_lookup[bsize];
  const int block_height = num_8x8_blocks_high_lookup[bsize];
  int i, j;
  for (j = 0; j < block_height; ++j)
    for (i = 0; i < block_width; ++i) {
      if (mi_row + j < cm->mi_rows && mi_col + i < cm->mi_cols)
        xd->mi[j * xd->mi_stride + i] = xd->mi[0];
    }
}

static void set_block_size(VP9_COMP * const cpi,
                           int mi_row, int mi_col,
                           BLOCK_SIZE bsize) {
  if (cpi->common.mi_cols > mi_col && cpi->common.mi_rows > mi_row) {
    MACROBLOCKD *const xd = &cpi->mb.e_mbd;
    set_modeinfo_offsets(&cpi->common, xd, mi_row, mi_col);
    xd->mi[0]->mbmi.sb_type = bsize;
    duplicate_mode_info_in_sb(&cpi->common, xd, mi_row, mi_col, bsize);
  }
}

typedef struct {
  int64_t sum_square_error;
  int64_t sum_error;
  int count;
  int variance;
} var;

typedef struct {
  var none;
  var horz[2];
  var vert[2];
} partition_variance;

typedef struct {
  partition_variance part_variances;
  var split[4];
} v8x8;

typedef struct {
  partition_variance part_variances;
  v8x8 split[4];
} v16x16;

typedef struct {
  partition_variance part_variances;
  v16x16 split[4];
} v32x32;

typedef struct {
  partition_variance part_variances;
  v32x32 split[4];
} v64x64;

typedef struct {
  partition_variance *part_variances;
  var *split[4];
} variance_node;

typedef enum {
  V16X16,
  V32X32,
  V64X64,
} TREE_LEVEL;

static void tree_to_node(void *data, BLOCK_SIZE bsize, variance_node *node) {
  int i;
  node->part_variances = NULL;
  vpx_memset(node->split, 0, sizeof(node->split));
  switch (bsize) {
    case BLOCK_64X64: {
      v64x64 *vt = (v64x64 *) data;
      node->part_variances = &vt->part_variances;
      for (i = 0; i < 4; i++)
        node->split[i] = &vt->split[i].part_variances.none;
      break;
    }
    case BLOCK_32X32: {
      v32x32 *vt = (v32x32 *) data;
      node->part_variances = &vt->part_variances;
      for (i = 0; i < 4; i++)
        node->split[i] = &vt->split[i].part_variances.none;
      break;
    }
    case BLOCK_16X16: {
      v16x16 *vt = (v16x16 *) data;
      node->part_variances = &vt->part_variances;
      for (i = 0; i < 4; i++)
        node->split[i] = &vt->split[i].part_variances.none;
      break;
    }
    case BLOCK_8X8: {
      v8x8 *vt = (v8x8 *) data;
      node->part_variances = &vt->part_variances;
      for (i = 0; i < 4; i++)
        node->split[i] = &vt->split[i];
      break;
    }
    default: {
      assert(0);
      break;
    }
  }
}

// Set variance values given sum square error, sum error, count.
static void fill_variance(int64_t s2, int64_t s, int c, var *v) {
  v->sum_square_error = s2;
  v->sum_error = s;
  v->count = c;
  if (c > 0)
    v->variance = (int)(256 *
                        (v->sum_square_error - v->sum_error * v->sum_error /
                         v->count) / v->count);
  else
    v->variance = 0;
}

void sum_2_variances(const var *a, const var *b, var *r) {
  fill_variance(a->sum_square_error + b->sum_square_error,
                a->sum_error + b->sum_error, a->count + b->count, r);
}

static void fill_variance_tree(void *data, BLOCK_SIZE bsize) {
  variance_node node;
  tree_to_node(data, bsize, &node);
  sum_2_variances(node.split[0], node.split[1], &node.part_variances->horz[0]);
  sum_2_variances(node.split[2], node.split[3], &node.part_variances->horz[1]);
  sum_2_variances(node.split[0], node.split[2], &node.part_variances->vert[0]);
  sum_2_variances(node.split[1], node.split[3], &node.part_variances->vert[1]);
  sum_2_variances(&node.part_variances->vert[0], &node.part_variances->vert[1],
                  &node.part_variances->none);
}

static int set_vt_partitioning(VP9_COMP *cpi,
                               void *data,
                               BLOCK_SIZE bsize,
                               int mi_row,
                               int mi_col) {
  VP9_COMMON * const cm = &cpi->common;
  variance_node vt;
  const int block_width = num_8x8_blocks_wide_lookup[bsize];
  const int block_height = num_8x8_blocks_high_lookup[bsize];
  // TODO(debargha): Choose this more intelligently.
  const int64_t threshold_multiplier = 25;
  int64_t threshold = threshold_multiplier * cpi->common.base_qindex;
  assert(block_height == block_width);

  tree_to_node(data, bsize, &vt);

  // Split none is available only if we have more than half a block size
  // in width and height inside the visible image.
  if (mi_col + block_width / 2 < cm->mi_cols &&
      mi_row + block_height / 2 < cm->mi_rows &&
      vt.part_variances->none.variance < threshold) {
    set_block_size(cpi, mi_row, mi_col, bsize);
    return 1;
  }

  // Vertical split is available on all but the bottom border.
  if (mi_row + block_height / 2 < cm->mi_rows &&
      vt.part_variances->vert[0].variance < threshold &&
      vt.part_variances->vert[1].variance < threshold) {
    BLOCK_SIZE subsize = get_subsize(bsize, PARTITION_VERT);
    set_block_size(cpi, mi_row, mi_col, subsize);
    set_block_size(cpi, mi_row, mi_col + block_width / 2, subsize);
    return 1;
  }

  // Horizontal split is available on all but the right border.
  if (mi_col + block_width / 2 < cm->mi_cols &&
      vt.part_variances->horz[0].variance < threshold &&
      vt.part_variances->horz[1].variance < threshold) {
    BLOCK_SIZE subsize = get_subsize(bsize, PARTITION_HORZ);
    set_block_size(cpi, mi_row, mi_col, subsize);
    set_block_size(cpi, mi_row + block_height / 2, mi_col, subsize);
    return 1;
  }
  return 0;
}

// TODO(debargha): Fix this function and make it work as expected.
static void choose_partitioning(VP9_COMP *cpi,
                                const TileInfo *const tile,
                                int mi_row, int mi_col) {
  VP9_COMMON * const cm = &cpi->common;
  MACROBLOCK *x = &cpi->mb;
  MACROBLOCKD *xd = &cpi->mb.e_mbd;

  int i, j, k;
  v64x64 vt;
  uint8_t *s;
  const uint8_t *d;
  int sp;
  int dp;
  int pixels_wide = 64, pixels_high = 64;
  int_mv nearest_mv, near_mv;
  const YV12_BUFFER_CONFIG *yv12 = get_ref_frame_buffer(cpi, LAST_FRAME);
  const struct scale_factors *const sf = &cm->frame_refs[LAST_FRAME - 1].sf;

  vp9_zero(vt);
  set_offsets(cpi, tile, mi_row, mi_col, BLOCK_64X64);

  if (xd->mb_to_right_edge < 0)
    pixels_wide += (xd->mb_to_right_edge >> 3);
  if (xd->mb_to_bottom_edge < 0)
    pixels_high += (xd->mb_to_bottom_edge >> 3);

  s = x->plane[0].src.buf;
  sp = x->plane[0].src.stride;

  if (cm->frame_type != KEY_FRAME) {
    vp9_setup_pre_planes(xd, 0, yv12, mi_row, mi_col, sf);

    xd->mi[0]->mbmi.ref_frame[0] = LAST_FRAME;
    xd->mi[0]->mbmi.sb_type = BLOCK_64X64;
    vp9_find_best_ref_mvs(xd, cm->allow_high_precision_mv,
                          xd->mi[0]->mbmi.ref_mvs[LAST_FRAME],
                          &nearest_mv, &near_mv);

    xd->mi[0]->mbmi.mv[0] = nearest_mv;
    vp9_build_inter_predictors_sby(xd, mi_row, mi_col, BLOCK_64X64);

    d = xd->plane[0].dst.buf;
    dp = xd->plane[0].dst.stride;
  } else {
    d = VP9_VAR_OFFS;
    dp = 0;
  }

  // Fill in the entire tree of 8x8 variances for splits.
  for (i = 0; i < 4; i++) {
    const int x32_idx = ((i & 1) << 5);
    const int y32_idx = ((i >> 1) << 5);
    for (j = 0; j < 4; j++) {
      const int x16_idx = x32_idx + ((j & 1) << 4);
      const int y16_idx = y32_idx + ((j >> 1) << 4);
      v16x16 *vst = &vt.split[i].split[j];
      for (k = 0; k < 4; k++) {
        int x_idx = x16_idx + ((k & 1) << 3);
        int y_idx = y16_idx + ((k >> 1) << 3);
        unsigned int sse = 0;
        int sum = 0;
        if (x_idx < pixels_wide && y_idx < pixels_high)
          vp9_get8x8var(s + y_idx * sp + x_idx, sp,
                        d + y_idx * dp + x_idx, dp, &sse, &sum);
        fill_variance(sse, sum, 64, &vst->split[k].part_variances.none);
      }
    }
  }
  // Fill the rest of the variance tree by summing split partition values.
  for (i = 0; i < 4; i++) {
    for (j = 0; j < 4; j++) {
      fill_variance_tree(&vt.split[i].split[j], BLOCK_16X16);
    }
    fill_variance_tree(&vt.split[i], BLOCK_32X32);
  }
  fill_variance_tree(&vt, BLOCK_64X64);

  // Now go through the entire structure,  splitting every block size until
  // we get to one that's got a variance lower than our threshold,  or we
  // hit 8x8.
  if (!set_vt_partitioning(cpi, &vt, BLOCK_64X64,
                           mi_row, mi_col)) {
    for (i = 0; i < 4; ++i) {
      const int x32_idx = ((i & 1) << 2);
      const int y32_idx = ((i >> 1) << 2);
      if (!set_vt_partitioning(cpi, &vt.split[i], BLOCK_32X32,
                               (mi_row + y32_idx), (mi_col + x32_idx))) {
        for (j = 0; j < 4; ++j) {
          const int x16_idx = ((j & 1) << 1);
          const int y16_idx = ((j >> 1) << 1);
          // NOTE: This is a temporary hack to disable 8x8 partitions,
          // since it works really bad - possibly due to a bug
#define DISABLE_8X8_VAR_BASED_PARTITION
#ifdef DISABLE_8X8_VAR_BASED_PARTITION
          if (mi_row + y32_idx + y16_idx + 1 < cm->mi_rows &&
              mi_row + x32_idx + x16_idx + 1 < cm->mi_cols) {
            set_block_size(cpi,
                           (mi_row + y32_idx + y16_idx),
                           (mi_col + x32_idx + x16_idx),
                           BLOCK_16X16);
          } else {
            for (k = 0; k < 4; ++k) {
              const int x8_idx = (k & 1);
              const int y8_idx = (k >> 1);
              set_block_size(cpi,
                             (mi_row + y32_idx + y16_idx + y8_idx),
                             (mi_col + x32_idx + x16_idx + x8_idx),
                             BLOCK_8X8);
            }
          }
#else
          if (!set_vt_partitioning(cpi, &vt.split[i].split[j], tile,
                                   BLOCK_16X16,
                                   (mi_row + y32_idx + y16_idx),
                                   (mi_col + x32_idx + x16_idx), 2)) {
            for (k = 0; k < 4; ++k) {
              const int x8_idx = (k & 1);
              const int y8_idx = (k >> 1);
              set_block_size(cpi,
                             (mi_row + y32_idx + y16_idx + y8_idx),
                             (mi_col + x32_idx + x16_idx + x8_idx),
                             BLOCK_8X8);
            }
          }
#endif
        }
      }
    }
  }
}

static void update_state(VP9_COMP *cpi, PICK_MODE_CONTEXT *ctx,
                         int mi_row, int mi_col, BLOCK_SIZE bsize,
                         int output_enabled) {
  int i, x_idx, y;
  VP9_COMMON *const cm = &cpi->common;
  RD_OPT *const rd_opt = &cpi->rd;
  MACROBLOCK *const x = &cpi->mb;
  MACROBLOCKD *const xd = &x->e_mbd;
  struct macroblock_plane *const p = x->plane;
  struct macroblockd_plane *const pd = xd->plane;
  MODE_INFO *mi = &ctx->mic;
  MB_MODE_INFO *const mbmi = &xd->mi[0]->mbmi;
  MODE_INFO *mi_addr = xd->mi[0];
  const struct segmentation *const seg = &cm->seg;

  const int mis = cm->mi_stride;
  const int mi_width = num_8x8_blocks_wide_lookup[bsize];
  const int mi_height = num_8x8_blocks_high_lookup[bsize];
  int max_plane;

  assert(mi->mbmi.sb_type == bsize);

  *mi_addr = *mi;

  // If segmentation in use
  if (seg->enabled && output_enabled) {
    // For in frame complexity AQ copy the segment id from the segment map.
    if (cpi->oxcf.aq_mode == COMPLEXITY_AQ) {
      const uint8_t *const map = seg->update_map ? cpi->segmentation_map
                                                 : cm->last_frame_seg_map;
      mi_addr->mbmi.segment_id =
        vp9_get_segment_id(cm, map, bsize, mi_row, mi_col);
    }
    // Else for cyclic refresh mode update the segment map, set the segment id
    // and then update the quantizer.
    if (cpi->oxcf.aq_mode == CYCLIC_REFRESH_AQ) {
      vp9_cyclic_refresh_update_segment(cpi, &xd->mi[0]->mbmi,
                                        mi_row, mi_col, bsize, 1);
    }
  }

  max_plane = is_inter_block(mbmi) ? MAX_MB_PLANE : 1;
  for (i = 0; i < max_plane; ++i) {
    p[i].coeff = ctx->coeff_pbuf[i][1];
    p[i].qcoeff = ctx->qcoeff_pbuf[i][1];
    pd[i].dqcoeff = ctx->dqcoeff_pbuf[i][1];
    p[i].eobs = ctx->eobs_pbuf[i][1];
  }

  for (i = max_plane; i < MAX_MB_PLANE; ++i) {
    p[i].coeff = ctx->coeff_pbuf[i][2];
    p[i].qcoeff = ctx->qcoeff_pbuf[i][2];
    pd[i].dqcoeff = ctx->dqcoeff_pbuf[i][2];
    p[i].eobs = ctx->eobs_pbuf[i][2];
  }

  // Restore the coding context of the MB to that that was in place
  // when the mode was picked for it
  for (y = 0; y < mi_height; y++)
    for (x_idx = 0; x_idx < mi_width; x_idx++)
      if ((xd->mb_to_right_edge >> (3 + MI_SIZE_LOG2)) + mi_width > x_idx
        && (xd->mb_to_bottom_edge >> (3 + MI_SIZE_LOG2)) + mi_height > y) {
        xd->mi[x_idx + y * mis] = mi_addr;
      }

  if (cpi->oxcf.aq_mode)
    vp9_init_plane_quantizers(cpi, x);

  // FIXME(rbultje) I'm pretty sure this should go to the end of this block
  // (i.e. after the output_enabled)
  if (bsize < BLOCK_32X32) {
    if (bsize < BLOCK_16X16)
      ctx->tx_rd_diff[ALLOW_16X16] = ctx->tx_rd_diff[ALLOW_8X8];
    ctx->tx_rd_diff[ALLOW_32X32] = ctx->tx_rd_diff[ALLOW_16X16];
  }

  if (is_inter_block(mbmi) && mbmi->sb_type < BLOCK_8X8) {
    mbmi->mv[0].as_int = mi->bmi[3].as_mv[0].as_int;
    mbmi->mv[1].as_int = mi->bmi[3].as_mv[1].as_int;
  }

  x->skip = ctx->skip;
  vpx_memcpy(x->zcoeff_blk[mbmi->tx_size], ctx->zcoeff_blk,
             sizeof(uint8_t) * ctx->num_4x4_blk);

  if (!output_enabled)
    return;

  if (!vp9_segfeature_active(&cm->seg, mbmi->segment_id, SEG_LVL_SKIP)) {
    for (i = 0; i < TX_MODES; i++)
      rd_opt->tx_select_diff[i] += ctx->tx_rd_diff[i];
  }

#if CONFIG_INTERNAL_STATS
  if (frame_is_intra_only(cm)) {
    static const int kf_mode_index[] = {
      THR_DC        /*DC_PRED*/,
      THR_V_PRED    /*V_PRED*/,
      THR_H_PRED    /*H_PRED*/,
      THR_D45_PRED  /*D45_PRED*/,
      THR_D135_PRED /*D135_PRED*/,
      THR_D117_PRED /*D117_PRED*/,
      THR_D153_PRED /*D153_PRED*/,
      THR_D207_PRED /*D207_PRED*/,
      THR_D63_PRED  /*D63_PRED*/,
      THR_TM        /*TM_PRED*/,
    };
    ++cpi->mode_chosen_counts[kf_mode_index[mbmi->mode]];
  } else {
    // Note how often each mode chosen as best
    ++cpi->mode_chosen_counts[ctx->best_mode_index];
  }
#endif
  if (!frame_is_intra_only(cm)) {
    if (is_inter_block(mbmi)) {
      vp9_update_mv_count(cm, xd);

      if (cm->interp_filter == SWITCHABLE) {
        const int ctx = vp9_get_pred_context_switchable_interp(xd);
        ++cm->counts.switchable_interp[ctx][mbmi->interp_filter];
      }
    }

    rd_opt->comp_pred_diff[SINGLE_REFERENCE] += ctx->single_pred_diff;
    rd_opt->comp_pred_diff[COMPOUND_REFERENCE] += ctx->comp_pred_diff;
    rd_opt->comp_pred_diff[REFERENCE_MODE_SELECT] += ctx->hybrid_pred_diff;

    for (i = 0; i < SWITCHABLE_FILTER_CONTEXTS; ++i)
      rd_opt->filter_diff[i] += ctx->best_filter_diff[i];
  }
}

void vp9_setup_src_planes(MACROBLOCK *x, const YV12_BUFFER_CONFIG *src,
                          int mi_row, int mi_col) {
  uint8_t *const buffers[3] = {src->y_buffer, src->u_buffer, src->v_buffer };
  const int strides[3] = {src->y_stride, src->uv_stride, src->uv_stride };
  int i;

  // Set current frame pointer.
  x->e_mbd.cur_buf = src;

  for (i = 0; i < MAX_MB_PLANE; i++)
    setup_pred_plane(&x->plane[i].src, buffers[i], strides[i], mi_row, mi_col,
                     NULL, x->e_mbd.plane[i].subsampling_x,
                     x->e_mbd.plane[i].subsampling_y);
}

static void set_mode_info_seg_skip(MACROBLOCK *x, TX_MODE tx_mode, int *rate,
                                   int64_t *dist, BLOCK_SIZE bsize) {
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = &xd->mi[0]->mbmi;
  INTERP_FILTER filter_ref;

  if (xd->up_available)
    filter_ref = xd->mi[-xd->mi_stride]->mbmi.interp_filter;
  else if (xd->left_available)
    filter_ref = xd->mi[-1]->mbmi.interp_filter;
  else
    filter_ref = EIGHTTAP;

  mbmi->sb_type = bsize;
  mbmi->mode = ZEROMV;
  mbmi->tx_size = MIN(max_txsize_lookup[bsize],
                      tx_mode_to_biggest_tx_size[tx_mode]);
  mbmi->skip = 1;
  mbmi->uv_mode = DC_PRED;
  mbmi->ref_frame[0] = LAST_FRAME;
  mbmi->ref_frame[1] = NONE;
  mbmi->mv[0].as_int = 0;
  mbmi->interp_filter = filter_ref;

  xd->mi[0]->bmi[0].as_mv[0].as_int = 0;
  x->skip = 1;

  *rate = 0;
  *dist = 0;
}

static void rd_pick_sb_modes(VP9_COMP *cpi, const TileInfo *const tile,
                             int mi_row, int mi_col,
                             int *totalrate, int64_t *totaldist,
                             BLOCK_SIZE bsize, PICK_MODE_CONTEXT *ctx,
                             int64_t best_rd, int block) {
  VP9_COMMON *const cm = &cpi->common;
  MACROBLOCK *const x = &cpi->mb;
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *mbmi;
  struct macroblock_plane *const p = x->plane;
  struct macroblockd_plane *const pd = xd->plane;
  const AQ_MODE aq_mode = cpi->oxcf.aq_mode;
  int i, orig_rdmult;
  double rdmult_ratio;

  vp9_clear_system_state();
  rdmult_ratio = 1.0;  // avoid uninitialized warnings

  // Use the lower precision, but faster, 32x32 fdct for mode selection.
  x->use_lp32x32fdct = 1;

  // TODO(JBB): Most other places in the code instead of calling the function
  // and then checking if its not the first 8x8 we put the check in the
  // calling function.  Do that here.
  if (bsize < BLOCK_8X8) {
    // When ab_index = 0 all sub-blocks are handled, so for ab_index != 0
    // there is nothing to be done.
    if (block != 0) {
      *totalrate = 0;
      *totaldist = 0;
      return;
    }
  }

  set_offsets(cpi, tile, mi_row, mi_col, bsize);
  mbmi = &xd->mi[0]->mbmi;
  mbmi->sb_type = bsize;

  for (i = 0; i < MAX_MB_PLANE; ++i) {
    p[i].coeff = ctx->coeff_pbuf[i][0];
    p[i].qcoeff = ctx->qcoeff_pbuf[i][0];
    pd[i].dqcoeff = ctx->dqcoeff_pbuf[i][0];
    p[i].eobs = ctx->eobs_pbuf[i][0];
  }
  ctx->is_coded = 0;
  ctx->skippable = 0;
  x->skip_recode = 0;

  // Set to zero to make sure we do not use the previous encoded frame stats
  mbmi->skip = 0;

  x->source_variance = get_sby_perpixel_variance(cpi, &x->plane[0].src, bsize);

  // Save rdmult before it might be changed, so it can be restored later.
  orig_rdmult = x->rdmult;

  if (aq_mode == VARIANCE_AQ) {
    const int energy = bsize <= BLOCK_16X16 ? x->mb_energy
                                            : vp9_block_energy(cpi, x, bsize);
    if (cm->frame_type == KEY_FRAME ||
        cpi->refresh_alt_ref_frame ||
        (cpi->refresh_golden_frame && !cpi->rc.is_src_frame_alt_ref)) {
      mbmi->segment_id = vp9_vaq_segment_id(energy);
    } else {
      const uint8_t *const map = cm->seg.update_map ? cpi->segmentation_map
                                                    : cm->last_frame_seg_map;
      mbmi->segment_id = vp9_get_segment_id(cm, map, bsize, mi_row, mi_col);
    }

    rdmult_ratio = vp9_vaq_rdmult_ratio(energy);
    vp9_init_plane_quantizers(cpi, x);
    vp9_clear_system_state();
    x->rdmult = (int)round(x->rdmult * rdmult_ratio);
  } else if (aq_mode == COMPLEXITY_AQ) {
    const int mi_offset = mi_row * cm->mi_cols + mi_col;
    unsigned char complexity = cpi->complexity_map[mi_offset];
    const int is_edge = (mi_row <= 1) || (mi_row >= (cm->mi_rows - 2)) ||
                        (mi_col <= 1) || (mi_col >= (cm->mi_cols - 2));
    if (!is_edge && (complexity > 128))
      x->rdmult += ((x->rdmult * (complexity - 128)) / 256);
  } else if (aq_mode == CYCLIC_REFRESH_AQ) {
    const uint8_t *const map = cm->seg.update_map ? cpi->segmentation_map
                                                  : cm->last_frame_seg_map;
    // If segment 1, use rdmult for that segment.
    if (vp9_get_segment_id(cm, map, bsize, mi_row, mi_col))
      x->rdmult = vp9_cyclic_refresh_get_rdmult(cpi->cyclic_refresh);
  }

  // Find best coding mode & reconstruct the MB so it is available
  // as a predictor for MBs that follow in the SB
  if (frame_is_intra_only(cm)) {
    vp9_rd_pick_intra_mode_sb(cpi, x, totalrate, totaldist, bsize, ctx,
                              best_rd);
  } else {
    if (bsize >= BLOCK_8X8) {
      if (vp9_segfeature_active(&cm->seg, mbmi->segment_id, SEG_LVL_SKIP))
        vp9_rd_pick_inter_mode_sb_seg_skip(cpi, x, totalrate, totaldist, bsize,
                                           ctx, best_rd);
      else
        vp9_rd_pick_inter_mode_sb(cpi, x, tile, mi_row, mi_col,
                                  totalrate, totaldist, bsize, ctx, best_rd);
    } else {
      vp9_rd_pick_inter_mode_sub8x8(cpi, x, tile, mi_row, mi_col, totalrate,
                                    totaldist, bsize, ctx, best_rd);
    }
  }

  x->rdmult = orig_rdmult;

  if (aq_mode == VARIANCE_AQ && *totalrate != INT_MAX) {
    vp9_clear_system_state();
    *totalrate = (int)round(*totalrate * rdmult_ratio);
  }
}

static void update_stats(VP9_COMMON *cm, const MACROBLOCK *x) {
  const MACROBLOCKD *const xd = &x->e_mbd;
  const MODE_INFO *const mi = xd->mi[0];
  const MB_MODE_INFO *const mbmi = &mi->mbmi;

  if (!frame_is_intra_only(cm)) {
    const int seg_ref_active = vp9_segfeature_active(&cm->seg, mbmi->segment_id,
                                                     SEG_LVL_REF_FRAME);
    if (!seg_ref_active) {
      FRAME_COUNTS *const counts = &cm->counts;
      const int inter_block = is_inter_block(mbmi);

      counts->intra_inter[vp9_get_intra_inter_context(xd)][inter_block]++;

      // If the segment reference feature is enabled we have only a single
      // reference frame allowed for the segment so exclude it from
      // the reference frame counts used to work out probabilities.
      if (inter_block) {
        const MV_REFERENCE_FRAME ref0 = mbmi->ref_frame[0];

        if (cm->reference_mode == REFERENCE_MODE_SELECT)
          counts->comp_inter[vp9_get_reference_mode_context(cm, xd)]
                            [has_second_ref(mbmi)]++;

        if (has_second_ref(mbmi)) {
          counts->comp_ref[vp9_get_pred_context_comp_ref_p(cm, xd)]
                          [ref0 == GOLDEN_FRAME]++;
        } else {
          counts->single_ref[vp9_get_pred_context_single_ref_p1(xd)][0]
                            [ref0 != LAST_FRAME]++;
          if (ref0 != LAST_FRAME)
            counts->single_ref[vp9_get_pred_context_single_ref_p2(xd)][1]
                              [ref0 != GOLDEN_FRAME]++;
        }
      }
    }
  }
}

static void restore_context(VP9_COMP *cpi, int mi_row, int mi_col,
                            ENTROPY_CONTEXT a[16 * MAX_MB_PLANE],
                            ENTROPY_CONTEXT l[16 * MAX_MB_PLANE],
                            PARTITION_CONTEXT sa[8], PARTITION_CONTEXT sl[8],
                            BLOCK_SIZE bsize) {
  MACROBLOCK *const x = &cpi->mb;
  MACROBLOCKD *const xd = &x->e_mbd;
  int p;
  const int num_4x4_blocks_wide = num_4x4_blocks_wide_lookup[bsize];
  const int num_4x4_blocks_high = num_4x4_blocks_high_lookup[bsize];
  int mi_width = num_8x8_blocks_wide_lookup[bsize];
  int mi_height = num_8x8_blocks_high_lookup[bsize];
  for (p = 0; p < MAX_MB_PLANE; p++) {
    vpx_memcpy(
        xd->above_context[p] + ((mi_col * 2) >> xd->plane[p].subsampling_x),
        a + num_4x4_blocks_wide * p,
        (sizeof(ENTROPY_CONTEXT) * num_4x4_blocks_wide) >>
        xd->plane[p].subsampling_x);
    vpx_memcpy(
        xd->left_context[p]
            + ((mi_row & MI_MASK) * 2 >> xd->plane[p].subsampling_y),
        l + num_4x4_blocks_high * p,
        (sizeof(ENTROPY_CONTEXT) * num_4x4_blocks_high) >>
        xd->plane[p].subsampling_y);
  }
  vpx_memcpy(xd->above_seg_context + mi_col, sa,
             sizeof(*xd->above_seg_context) * mi_width);
  vpx_memcpy(xd->left_seg_context + (mi_row & MI_MASK), sl,
             sizeof(xd->left_seg_context[0]) * mi_height);
}

static void save_context(VP9_COMP *cpi, int mi_row, int mi_col,
                         ENTROPY_CONTEXT a[16 * MAX_MB_PLANE],
                         ENTROPY_CONTEXT l[16 * MAX_MB_PLANE],
                         PARTITION_CONTEXT sa[8], PARTITION_CONTEXT sl[8],
                         BLOCK_SIZE bsize) {
  const MACROBLOCK *const x = &cpi->mb;
  const MACROBLOCKD *const xd = &x->e_mbd;
  int p;
  const int num_4x4_blocks_wide = num_4x4_blocks_wide_lookup[bsize];
  const int num_4x4_blocks_high = num_4x4_blocks_high_lookup[bsize];
  int mi_width = num_8x8_blocks_wide_lookup[bsize];
  int mi_height = num_8x8_blocks_high_lookup[bsize];

  // buffer the above/left context information of the block in search.
  for (p = 0; p < MAX_MB_PLANE; ++p) {
    vpx_memcpy(
        a + num_4x4_blocks_wide * p,
        xd->above_context[p] + (mi_col * 2 >> xd->plane[p].subsampling_x),
        (sizeof(ENTROPY_CONTEXT) * num_4x4_blocks_wide) >>
        xd->plane[p].subsampling_x);
    vpx_memcpy(
        l + num_4x4_blocks_high * p,
        xd->left_context[p]
            + ((mi_row & MI_MASK) * 2 >> xd->plane[p].subsampling_y),
        (sizeof(ENTROPY_CONTEXT) * num_4x4_blocks_high) >>
        xd->plane[p].subsampling_y);
  }
  vpx_memcpy(sa, xd->above_seg_context + mi_col,
             sizeof(*xd->above_seg_context) * mi_width);
  vpx_memcpy(sl, xd->left_seg_context + (mi_row & MI_MASK),
             sizeof(xd->left_seg_context[0]) * mi_height);
}

static void encode_b(VP9_COMP *cpi, const TileInfo *const tile,
                     TOKENEXTRA **tp, int mi_row, int mi_col,
                     int output_enabled, BLOCK_SIZE bsize,
                     PICK_MODE_CONTEXT *ctx) {
  set_offsets(cpi, tile, mi_row, mi_col, bsize);
  update_state(cpi, ctx, mi_row, mi_col, bsize, output_enabled);
  encode_superblock(cpi, tp, output_enabled, mi_row, mi_col, bsize, ctx);

  if (output_enabled) {
    update_stats(&cpi->common, &cpi->mb);

    (*tp)->token = EOSB_TOKEN;
    (*tp)++;
  }
}

static void encode_sb(VP9_COMP *cpi, const TileInfo *const tile,
                      TOKENEXTRA **tp, int mi_row, int mi_col,
                      int output_enabled, BLOCK_SIZE bsize,
                      PC_TREE *pc_tree) {
  VP9_COMMON *const cm = &cpi->common;
  MACROBLOCK *const x = &cpi->mb;
  MACROBLOCKD *const xd = &x->e_mbd;

  const int bsl = b_width_log2(bsize), hbs = (1 << bsl) / 4;
  int ctx;
  PARTITION_TYPE partition;
  BLOCK_SIZE subsize = bsize;

  if (mi_row >= cm->mi_rows || mi_col >= cm->mi_cols)
    return;

  if (bsize >= BLOCK_8X8) {
    ctx = partition_plane_context(xd, mi_row, mi_col, bsize);
    subsize = get_subsize(bsize, pc_tree->partitioning);
  } else {
    ctx = 0;
    subsize = BLOCK_4X4;
  }

  partition = partition_lookup[bsl][subsize];
  if (output_enabled && bsize != BLOCK_4X4)
    cm->counts.partition[ctx][partition]++;

  switch (partition) {
    case PARTITION_NONE:
      encode_b(cpi, tile, tp, mi_row, mi_col, output_enabled, subsize,
               &pc_tree->none);
      break;
    case PARTITION_VERT:
      encode_b(cpi, tile, tp, mi_row, mi_col, output_enabled, subsize,
               &pc_tree->vertical[0]);
      if (mi_col + hbs < cm->mi_cols && bsize > BLOCK_8X8) {
        encode_b(cpi, tile, tp, mi_row, mi_col + hbs, output_enabled, subsize,
                 &pc_tree->vertical[1]);
      }
      break;
    case PARTITION_HORZ:
      encode_b(cpi, tile, tp, mi_row, mi_col, output_enabled, subsize,
               &pc_tree->horizontal[0]);
      if (mi_row + hbs < cm->mi_rows && bsize > BLOCK_8X8) {
        encode_b(cpi, tile, tp, mi_row + hbs, mi_col, output_enabled, subsize,
                 &pc_tree->horizontal[1]);
      }
      break;
    case PARTITION_SPLIT:
      if (bsize == BLOCK_8X8) {
        encode_b(cpi, tile, tp, mi_row, mi_col, output_enabled, subsize,
                 pc_tree->leaf_split[0]);
      } else {
        encode_sb(cpi, tile, tp, mi_row, mi_col, output_enabled, subsize,
                  pc_tree->split[0]);
        encode_sb(cpi, tile, tp, mi_row, mi_col + hbs, output_enabled, subsize,
                  pc_tree->split[1]);
        encode_sb(cpi, tile, tp, mi_row + hbs, mi_col, output_enabled, subsize,
                  pc_tree->split[2]);
        encode_sb(cpi, tile, tp, mi_row + hbs, mi_col + hbs, output_enabled,
                  subsize, pc_tree->split[3]);
      }
      break;
    default:
      assert("Invalid partition type.");
      break;
  }

  if (partition != PARTITION_SPLIT || bsize == BLOCK_8X8)
    update_partition_context(xd, mi_row, mi_col, subsize, bsize);
}

// Check to see if the given partition size is allowed for a specified number
// of 8x8 block rows and columns remaining in the image.
// If not then return the largest allowed partition size
static BLOCK_SIZE find_partition_size(BLOCK_SIZE bsize,
                                      int rows_left, int cols_left,
                                      int *bh, int *bw) {
  if (rows_left <= 0 || cols_left <= 0) {
    return MIN(bsize, BLOCK_8X8);
  } else {
    for (; bsize > 0; bsize -= 3) {
      *bh = num_8x8_blocks_high_lookup[bsize];
      *bw = num_8x8_blocks_wide_lookup[bsize];
      if ((*bh <= rows_left) && (*bw <= cols_left)) {
        break;
      }
    }
  }
  return bsize;
}

static void set_partial_b64x64_partition(MODE_INFO *mi, int mis,
    int bh_in, int bw_in, int row8x8_remaining, int col8x8_remaining,
    BLOCK_SIZE bsize, MODE_INFO **mi_8x8) {
  int bh = bh_in;
  int r, c;
  for (r = 0; r < MI_BLOCK_SIZE; r += bh) {
    int bw = bw_in;
    for (c = 0; c < MI_BLOCK_SIZE; c += bw) {
      const int index = r * mis + c;
      mi_8x8[index] = mi + index;
      mi_8x8[index]->mbmi.sb_type = find_partition_size(bsize,
          row8x8_remaining - r, col8x8_remaining - c, &bh, &bw);
    }
  }
}

// This function attempts to set all mode info entries in a given SB64
// to the same block partition size.
// However, at the bottom and right borders of the image the requested size
// may not be allowed in which case this code attempts to choose the largest
// allowable partition.
static void set_fixed_partitioning(VP9_COMP *cpi, const TileInfo *const tile,
                                   MODE_INFO **mi_8x8, int mi_row, int mi_col,
                                   BLOCK_SIZE bsize) {
  VP9_COMMON *const cm = &cpi->common;
  const int mis = cm->mi_stride;
  const int row8x8_remaining = tile->mi_row_end - mi_row;
  const int col8x8_remaining = tile->mi_col_end - mi_col;
  int block_row, block_col;
  MODE_INFO *mi_upper_left = cm->mi + mi_row * mis + mi_col;
  int bh = num_8x8_blocks_high_lookup[bsize];
  int bw = num_8x8_blocks_wide_lookup[bsize];

  assert((row8x8_remaining > 0) && (col8x8_remaining > 0));

  // Apply the requested partition size to the SB64 if it is all "in image"
  if ((col8x8_remaining >= MI_BLOCK_SIZE) &&
      (row8x8_remaining >= MI_BLOCK_SIZE)) {
    for (block_row = 0; block_row < MI_BLOCK_SIZE; block_row += bh) {
      for (block_col = 0; block_col < MI_BLOCK_SIZE; block_col += bw) {
        int index = block_row * mis + block_col;
        mi_8x8[index] = mi_upper_left + index;
        mi_8x8[index]->mbmi.sb_type = bsize;
      }
    }
  } else {
    // Else this is a partial SB64.
    set_partial_b64x64_partition(mi_upper_left, mis, bh, bw, row8x8_remaining,
        col8x8_remaining, bsize, mi_8x8);
  }
}

static void copy_partitioning(VP9_COMMON *cm, MODE_INFO **mi_8x8,
  MODE_INFO **prev_mi_8x8) {
  const int mis = cm->mi_stride;
  int block_row, block_col;

  for (block_row = 0; block_row < 8; ++block_row) {
    for (block_col = 0; block_col < 8; ++block_col) {
      MODE_INFO *const prev_mi = prev_mi_8x8[block_row * mis + block_col];
      const BLOCK_SIZE sb_type = prev_mi ? prev_mi->mbmi.sb_type : 0;

      if (prev_mi) {
        const ptrdiff_t offset = prev_mi - cm->prev_mi;
        mi_8x8[block_row * mis + block_col] = cm->mi + offset;
        mi_8x8[block_row * mis + block_col]->mbmi.sb_type = sb_type;
      }
    }
  }
}

static void constrain_copy_partitioning(VP9_COMP *const cpi,
                                        const TileInfo *const tile,
                                        MODE_INFO **mi_8x8,
                                        MODE_INFO **prev_mi_8x8,
                                        int mi_row, int mi_col,
                                        BLOCK_SIZE bsize) {
  VP9_COMMON *const cm = &cpi->common;
  const int mis = cm->mi_stride;
  const int row8x8_remaining = tile->mi_row_end - mi_row;
  const int col8x8_remaining = tile->mi_col_end - mi_col;
  MODE_INFO *const mi_upper_left = cm->mi + mi_row * mis + mi_col;
  const int bh = num_8x8_blocks_high_lookup[bsize];
  const int bw = num_8x8_blocks_wide_lookup[bsize];
  int block_row, block_col;

  assert((row8x8_remaining > 0) && (col8x8_remaining > 0));

  // If the SB64 if it is all "in image".
  if ((col8x8_remaining >= MI_BLOCK_SIZE) &&
      (row8x8_remaining >= MI_BLOCK_SIZE)) {
    for (block_row = 0; block_row < MI_BLOCK_SIZE; block_row += bh) {
      for (block_col = 0; block_col < MI_BLOCK_SIZE; block_col += bw) {
        const int index = block_row * mis + block_col;
        MODE_INFO *prev_mi = prev_mi_8x8[index];
        const BLOCK_SIZE sb_type = prev_mi ? prev_mi->mbmi.sb_type : 0;
        // Use previous partition if block size is not larger than bsize.
        if (prev_mi && sb_type <= bsize) {
          int block_row2, block_col2;
          for (block_row2 = 0; block_row2 < bh; ++block_row2) {
            for (block_col2 = 0; block_col2 < bw; ++block_col2) {
              const int index2 = (block_row + block_row2) * mis +
                  block_col + block_col2;
              prev_mi = prev_mi_8x8[index2];
              if (prev_mi) {
                const ptrdiff_t offset = prev_mi - cm->prev_mi;
                mi_8x8[index2] = cm->mi + offset;
                mi_8x8[index2]->mbmi.sb_type = prev_mi->mbmi.sb_type;
              }
            }
          }
        } else {
          // Otherwise, use fixed partition of size bsize.
          mi_8x8[index] = mi_upper_left + index;
          mi_8x8[index]->mbmi.sb_type = bsize;
        }
      }
    }
  } else {
    // Else this is a partial SB64, copy previous partition.
    copy_partitioning(cm, mi_8x8, prev_mi_8x8);
  }
}

const struct {
  int row;
  int col;
} coord_lookup[16] = {
    // 32x32 index = 0
    {0, 0}, {0, 2}, {2, 0}, {2, 2},
    // 32x32 index = 1
    {0, 4}, {0, 6}, {2, 4}, {2, 6},
    // 32x32 index = 2
    {4, 0}, {4, 2}, {6, 0}, {6, 2},
    // 32x32 index = 3
    {4, 4}, {4, 6}, {6, 4}, {6, 6},
};

static void set_source_var_based_partition(VP9_COMP *cpi,
                                           const TileInfo *const tile,
                                           MODE_INFO **mi_8x8,
                                           int mi_row, int mi_col) {
  VP9_COMMON *const cm = &cpi->common;
  MACROBLOCK *const x = &cpi->mb;
  const int mis = cm->mi_stride;
  const int row8x8_remaining = tile->mi_row_end - mi_row;
  const int col8x8_remaining = tile->mi_col_end - mi_col;
  MODE_INFO *mi_upper_left = cm->mi + mi_row * mis + mi_col;

  vp9_setup_src_planes(x, cpi->Source, mi_row, mi_col);

  assert((row8x8_remaining > 0) && (col8x8_remaining > 0));

  // In-image SB64
  if ((col8x8_remaining >= MI_BLOCK_SIZE) &&
      (row8x8_remaining >= MI_BLOCK_SIZE)) {
    int i, j;
    int index;
    diff d32[4];
    const int offset = (mi_row >> 1) * cm->mb_cols + (mi_col >> 1);
    int is_larger_better = 0;
    int use32x32 = 0;
    unsigned int thr = cpi->source_var_thresh;

    vpx_memset(d32, 0, 4 * sizeof(diff));

    for (i = 0; i < 4; i++) {
      diff *d16[4];

      for (j = 0; j < 4; j++) {
        int b_mi_row = coord_lookup[i * 4 + j].row;
        int b_mi_col = coord_lookup[i * 4 + j].col;
        int boffset = b_mi_row / 2 * cm->mb_cols +
                      b_mi_col / 2;

        d16[j] = cpi->source_diff_var + offset + boffset;

        index = b_mi_row * mis + b_mi_col;
        mi_8x8[index] = mi_upper_left + index;
        mi_8x8[index]->mbmi.sb_type = BLOCK_16X16;

        // TODO(yunqingwang): If d16[j].var is very large, use 8x8 partition
        // size to further improve quality.
      }

      is_larger_better = (d16[0]->var < thr) && (d16[1]->var < thr) &&
          (d16[2]->var < thr) && (d16[3]->var < thr);

      // Use 32x32 partition
      if (is_larger_better) {
        use32x32 += 1;

        for (j = 0; j < 4; j++) {
          d32[i].sse += d16[j]->sse;
          d32[i].sum += d16[j]->sum;
        }

        d32[i].var = d32[i].sse - (((int64_t)d32[i].sum * d32[i].sum) >> 10);

        index = coord_lookup[i*4].row * mis + coord_lookup[i*4].col;
        mi_8x8[index] = mi_upper_left + index;
        mi_8x8[index]->mbmi.sb_type = BLOCK_32X32;
      }
    }

    if (use32x32 == 4) {
      thr <<= 1;
      is_larger_better = (d32[0].var < thr) && (d32[1].var < thr) &&
          (d32[2].var < thr) && (d32[3].var < thr);

      // Use 64x64 partition
      if (is_larger_better) {
        mi_8x8[0] = mi_upper_left;
        mi_8x8[0]->mbmi.sb_type = BLOCK_64X64;
      }
    }
  } else {   // partial in-image SB64
    int bh = num_8x8_blocks_high_lookup[BLOCK_16X16];
    int bw = num_8x8_blocks_wide_lookup[BLOCK_16X16];
    set_partial_b64x64_partition(mi_upper_left, mis, bh, bw,
        row8x8_remaining, col8x8_remaining, BLOCK_16X16, mi_8x8);
  }
}

static int is_background(const VP9_COMP *cpi, const TileInfo *const tile,
                         int mi_row, int mi_col) {
  // This assumes the input source frames are of the same dimension.
  const int row8x8_remaining = tile->mi_row_end - mi_row;
  const int col8x8_remaining = tile->mi_col_end - mi_col;
  const int x = mi_col * MI_SIZE;
  const int y = mi_row * MI_SIZE;
  const int src_stride = cpi->Source->y_stride;
  const uint8_t *const src = &cpi->Source->y_buffer[y * src_stride + x];
  const int pre_stride = cpi->Last_Source->y_stride;
  const uint8_t *const pre = &cpi->Last_Source->y_buffer[y * pre_stride + x];
  int this_sad = 0;
  int threshold = 0;

  if (row8x8_remaining >= MI_BLOCK_SIZE &&
      col8x8_remaining >= MI_BLOCK_SIZE) {
    this_sad = cpi->fn_ptr[BLOCK_64X64].sdf(src, src_stride, pre, pre_stride);
    threshold = (1 << 12);
  } else {
    int r, c;
    for (r = 0; r < row8x8_remaining; r += 2)
      for (c = 0; c < col8x8_remaining; c += 2)
        this_sad += cpi->fn_ptr[BLOCK_16X16].sdf(src, src_stride,
                                                 pre, pre_stride);
    threshold = (row8x8_remaining * col8x8_remaining) << 6;
  }

  return this_sad < 2 * threshold;
}

static int sb_has_motion(const VP9_COMMON *cm, MODE_INFO **prev_mi_8x8,
                         const int motion_thresh) {
  const int mis = cm->mi_stride;
  int block_row, block_col;

  if (cm->prev_mi) {
    for (block_row = 0; block_row < 8; ++block_row) {
      for (block_col = 0; block_col < 8; ++block_col) {
        const MODE_INFO *prev_mi = prev_mi_8x8[block_row * mis + block_col];
        if (prev_mi) {
          if (abs(prev_mi->mbmi.mv[0].as_mv.row) > motion_thresh ||
              abs(prev_mi->mbmi.mv[0].as_mv.col) > motion_thresh)
            return 1;
        }
      }
    }
  }
  return 0;
}

static void update_state_rt(VP9_COMP *cpi, PICK_MODE_CONTEXT *ctx,
                            int mi_row, int mi_col, int bsize) {
  VP9_COMMON *const cm = &cpi->common;
  MACROBLOCK *const x = &cpi->mb;
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = &xd->mi[0]->mbmi;
  const struct segmentation *const seg = &cm->seg;

  *(xd->mi[0]) = ctx->mic;

  // For in frame adaptive Q, check for reseting the segment_id and updating
  // the cyclic refresh map.
  if ((cpi->oxcf.aq_mode == CYCLIC_REFRESH_AQ) && seg->enabled) {
    vp9_cyclic_refresh_update_segment(cpi, &xd->mi[0]->mbmi,
                                      mi_row, mi_col, bsize, 1);
    vp9_init_plane_quantizers(cpi, x);
  }

  if (is_inter_block(mbmi)) {
    vp9_update_mv_count(cm, xd);

    if (cm->interp_filter == SWITCHABLE) {
      const int pred_ctx = vp9_get_pred_context_switchable_interp(xd);
      ++cm->counts.switchable_interp[pred_ctx][mbmi->interp_filter];
    }
  }

  x->skip = ctx->skip;
  x->skip_txfm[0] = mbmi->segment_id ? 0 : ctx->skip_txfm[0];
}

static void encode_b_rt(VP9_COMP *cpi, const TileInfo *const tile,
                        TOKENEXTRA **tp, int mi_row, int mi_col,
                     int output_enabled, BLOCK_SIZE bsize,
                     PICK_MODE_CONTEXT *ctx) {
  set_offsets(cpi, tile, mi_row, mi_col, bsize);
  update_state_rt(cpi, ctx, mi_row, mi_col, bsize);

#if CONFIG_VP9_TEMPORAL_DENOISING
  if (cpi->oxcf.noise_sensitivity > 0 && output_enabled) {
    vp9_denoiser_denoise(&cpi->denoiser, &cpi->mb, mi_row, mi_col,
                         MAX(BLOCK_8X8, bsize), ctx);
  }
#endif

  encode_superblock(cpi, tp, output_enabled, mi_row, mi_col, bsize, ctx);
  update_stats(&cpi->common, &cpi->mb);

  (*tp)->token = EOSB_TOKEN;
  (*tp)++;
}

static void encode_sb_rt(VP9_COMP *cpi, const TileInfo *const tile,
                         TOKENEXTRA **tp, int mi_row, int mi_col,
                         int output_enabled, BLOCK_SIZE bsize,
                         PC_TREE *pc_tree) {
  VP9_COMMON *const cm = &cpi->common;
  MACROBLOCK *const x = &cpi->mb;
  MACROBLOCKD *const xd = &x->e_mbd;

  const int bsl = b_width_log2(bsize), hbs = (1 << bsl) / 4;
  int ctx;
  PARTITION_TYPE partition;
  BLOCK_SIZE subsize;

  if (mi_row >= cm->mi_rows || mi_col >= cm->mi_cols)
    return;

  if (bsize >= BLOCK_8X8) {
    const int idx_str = xd->mi_stride * mi_row + mi_col;
    MODE_INFO ** mi_8x8 = cm->mi_grid_visible + idx_str;
    ctx = partition_plane_context(xd, mi_row, mi_col, bsize);
    subsize = mi_8x8[0]->mbmi.sb_type;
  } else {
    ctx = 0;
    subsize = BLOCK_4X4;
  }

  partition = partition_lookup[bsl][subsize];
  if (output_enabled && bsize != BLOCK_4X4)
    cm->counts.partition[ctx][partition]++;

  switch (partition) {
    case PARTITION_NONE:
      encode_b_rt(cpi, tile, tp, mi_row, mi_col, output_enabled, subsize,
                  &pc_tree->none);
      break;
    case PARTITION_VERT:
      encode_b_rt(cpi, tile, tp, mi_row, mi_col, output_enabled, subsize,
                  &pc_tree->vertical[0]);
      if (mi_col + hbs < cm->mi_cols && bsize > BLOCK_8X8) {
        encode_b_rt(cpi, tile, tp, mi_row, mi_col + hbs, output_enabled,
                    subsize, &pc_tree->vertical[1]);
      }
      break;
    case PARTITION_HORZ:
      encode_b_rt(cpi, tile, tp, mi_row, mi_col, output_enabled, subsize,
                  &pc_tree->horizontal[0]);
      if (mi_row + hbs < cm->mi_rows && bsize > BLOCK_8X8) {
        encode_b_rt(cpi, tile, tp, mi_row + hbs, mi_col, output_enabled,
                    subsize, &pc_tree->horizontal[1]);
      }
      break;
    case PARTITION_SPLIT:
      subsize = get_subsize(bsize, PARTITION_SPLIT);
      encode_sb_rt(cpi, tile, tp, mi_row, mi_col, output_enabled, subsize,
                   pc_tree->split[0]);
      encode_sb_rt(cpi, tile, tp, mi_row, mi_col + hbs, output_enabled,
                   subsize, pc_tree->split[1]);
      encode_sb_rt(cpi, tile, tp, mi_row + hbs, mi_col, output_enabled,
                   subsize, pc_tree->split[2]);
      encode_sb_rt(cpi, tile, tp, mi_row + hbs, mi_col + hbs, output_enabled,
                   subsize, pc_tree->split[3]);
      break;
    default:
      assert("Invalid partition type.");
      break;
  }

  if (partition != PARTITION_SPLIT || bsize == BLOCK_8X8)
    update_partition_context(xd, mi_row, mi_col, subsize, bsize);
}

static void rd_use_partition(VP9_COMP *cpi,
                             const TileInfo *const tile,
                             MODE_INFO **mi_8x8,
                             TOKENEXTRA **tp, int mi_row, int mi_col,
                             BLOCK_SIZE bsize, int *rate, int64_t *dist,
                             int do_recon, PC_TREE *pc_tree) {
  VP9_COMMON *const cm = &cpi->common;
  MACROBLOCK *const x = &cpi->mb;
  MACROBLOCKD *const xd = &x->e_mbd;
  const int mis = cm->mi_stride;
  const int bsl = b_width_log2(bsize);
  const int mi_step = num_4x4_blocks_wide_lookup[bsize] / 2;
  const int bss = (1 << bsl) / 4;
  int i, pl;
  PARTITION_TYPE partition = PARTITION_NONE;
  BLOCK_SIZE subsize;
  ENTROPY_CONTEXT l[16 * MAX_MB_PLANE], a[16 * MAX_MB_PLANE];
  PARTITION_CONTEXT sl[8], sa[8];
  int last_part_rate = INT_MAX;
  int64_t last_part_dist = INT64_MAX;
  int64_t last_part_rd = INT64_MAX;
  int none_rate = INT_MAX;
  int64_t none_dist = INT64_MAX;
  int64_t none_rd = INT64_MAX;
  int chosen_rate = INT_MAX;
  int64_t chosen_dist = INT64_MAX;
  int64_t chosen_rd = INT64_MAX;
  BLOCK_SIZE sub_subsize = BLOCK_4X4;
  int splits_below = 0;
  BLOCK_SIZE bs_type = mi_8x8[0]->mbmi.sb_type;
  int do_partition_search = 1;
  PICK_MODE_CONTEXT *ctx = &pc_tree->none;

  if (mi_row >= cm->mi_rows || mi_col >= cm->mi_cols)
    return;

  assert(num_4x4_blocks_wide_lookup[bsize] ==
         num_4x4_blocks_high_lookup[bsize]);

  partition = partition_lookup[bsl][bs_type];
  subsize = get_subsize(bsize, partition);

  pc_tree->partitioning = partition;
  save_context(cpi, mi_row, mi_col, a, l, sa, sl, bsize);

  if (bsize == BLOCK_16X16 && cpi->oxcf.aq_mode) {
    set_offsets(cpi, tile, mi_row, mi_col, bsize);
    x->mb_energy = vp9_block_energy(cpi, x, bsize);
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
        MODE_INFO * this_mi = mi_8x8[jj * bss * mis + ii * bss];
        if (this_mi && this_mi->mbmi.sb_type >= sub_subsize) {
          splits_below = 0;
        }
      }
    }

    // If partition is not none try none unless each of the 4 splits are split
    // even further..
    if (partition != PARTITION_NONE && !splits_below &&
        mi_row + (mi_step >> 1) < cm->mi_rows &&
        mi_col + (mi_step >> 1) < cm->mi_cols) {
      pc_tree->partitioning = PARTITION_NONE;
      rd_pick_sb_modes(cpi, tile, mi_row, mi_col, &none_rate, &none_dist, bsize,
                       ctx, INT64_MAX, 0);

      pl = partition_plane_context(xd, mi_row, mi_col, bsize);

      if (none_rate < INT_MAX) {
        none_rate += cpi->partition_cost[pl][PARTITION_NONE];
        none_rd = RDCOST(x->rdmult, x->rddiv, none_rate, none_dist);
      }

      restore_context(cpi, mi_row, mi_col, a, l, sa, sl, bsize);
      mi_8x8[0]->mbmi.sb_type = bs_type;
      pc_tree->partitioning = partition;
    }
  }

  switch (partition) {
    case PARTITION_NONE:
      rd_pick_sb_modes(cpi, tile, mi_row, mi_col, &last_part_rate,
                       &last_part_dist, bsize, ctx, INT64_MAX, 0);
      break;
    case PARTITION_HORZ:
      rd_pick_sb_modes(cpi, tile, mi_row, mi_col, &last_part_rate,
                       &last_part_dist, subsize, &pc_tree->horizontal[0],
                       INT64_MAX, 0);
      if (last_part_rate != INT_MAX &&
          bsize >= BLOCK_8X8 && mi_row + (mi_step >> 1) < cm->mi_rows) {
        int rt = 0;
        int64_t dt = 0;
        PICK_MODE_CONTEXT *ctx = &pc_tree->horizontal[0];
        update_state(cpi, ctx, mi_row, mi_col, subsize, 0);
        encode_superblock(cpi, tp, 0, mi_row, mi_col, subsize, ctx);
        rd_pick_sb_modes(cpi, tile, mi_row + (mi_step >> 1), mi_col, &rt, &dt,
                         subsize, &pc_tree->horizontal[1], INT64_MAX, 1);
        if (rt == INT_MAX || dt == INT64_MAX) {
          last_part_rate = INT_MAX;
          last_part_dist = INT64_MAX;
          break;
        }

        last_part_rate += rt;
        last_part_dist += dt;
      }
      break;
    case PARTITION_VERT:
      rd_pick_sb_modes(cpi, tile, mi_row, mi_col, &last_part_rate,
                       &last_part_dist, subsize, &pc_tree->vertical[0],
                       INT64_MAX, 0);
      if (last_part_rate != INT_MAX &&
          bsize >= BLOCK_8X8 && mi_col + (mi_step >> 1) < cm->mi_cols) {
        int rt = 0;
        int64_t dt = 0;
        PICK_MODE_CONTEXT *ctx = &pc_tree->vertical[0];
        update_state(cpi, ctx, mi_row, mi_col, subsize, 0);
        encode_superblock(cpi, tp, 0, mi_row, mi_col, subsize, ctx);
        rd_pick_sb_modes(cpi, tile, mi_row, mi_col + (mi_step >> 1), &rt, &dt,
                         subsize, &pc_tree->vertical[bsize > BLOCK_8X8],
                         INT64_MAX, 1);
        if (rt == INT_MAX || dt == INT64_MAX) {
          last_part_rate = INT_MAX;
          last_part_dist = INT64_MAX;
          break;
        }
        last_part_rate += rt;
        last_part_dist += dt;
      }
      break;
    case PARTITION_SPLIT:
      if (bsize == BLOCK_8X8) {
        rd_pick_sb_modes(cpi, tile, mi_row, mi_col, &last_part_rate,
                         &last_part_dist, subsize, pc_tree->leaf_split[0],
                         INT64_MAX, 0);
        break;
      }
      last_part_rate = 0;
      last_part_dist = 0;
      for (i = 0; i < 4; i++) {
        int x_idx = (i & 1) * (mi_step >> 1);
        int y_idx = (i >> 1) * (mi_step >> 1);
        int jj = i >> 1, ii = i & 0x01;
        int rt;
        int64_t dt;

        if ((mi_row + y_idx >= cm->mi_rows) || (mi_col + x_idx >= cm->mi_cols))
          continue;

        rd_use_partition(cpi, tile, mi_8x8 + jj * bss * mis + ii * bss, tp,
                         mi_row + y_idx, mi_col + x_idx, subsize, &rt, &dt,
                         i != 3, pc_tree->split[i]);
        if (rt == INT_MAX || dt == INT64_MAX) {
          last_part_rate = INT_MAX;
          last_part_dist = INT64_MAX;
          break;
        }
        last_part_rate += rt;
        last_part_dist += dt;
      }
      break;
    default:
      assert(0);
      break;
  }

  pl = partition_plane_context(xd, mi_row, mi_col, bsize);
  if (last_part_rate < INT_MAX) {
    last_part_rate += cpi->partition_cost[pl][partition];
    last_part_rd = RDCOST(x->rdmult, x->rddiv, last_part_rate, last_part_dist);
  }

  if (do_partition_search
      && cpi->sf.adjust_partitioning_from_last_frame
      && cpi->sf.partition_search_type == SEARCH_PARTITION
      && partition != PARTITION_SPLIT && bsize > BLOCK_8X8
      && (mi_row + mi_step < cm->mi_rows ||
          mi_row + (mi_step >> 1) == cm->mi_rows)
      && (mi_col + mi_step < cm->mi_cols ||
          mi_col + (mi_step >> 1) == cm->mi_cols)) {
    BLOCK_SIZE split_subsize = get_subsize(bsize, PARTITION_SPLIT);
    chosen_rate = 0;
    chosen_dist = 0;
    restore_context(cpi, mi_row, mi_col, a, l, sa, sl, bsize);
    pc_tree->partitioning = PARTITION_SPLIT;

    // Split partition.
    for (i = 0; i < 4; i++) {
      int x_idx = (i & 1) * (mi_step >> 1);
      int y_idx = (i >> 1) * (mi_step >> 1);
      int rt = 0;
      int64_t dt = 0;
      ENTROPY_CONTEXT l[16 * MAX_MB_PLANE], a[16 * MAX_MB_PLANE];
      PARTITION_CONTEXT sl[8], sa[8];

      if ((mi_row + y_idx >= cm->mi_rows) || (mi_col + x_idx >= cm->mi_cols))
        continue;

      save_context(cpi, mi_row, mi_col, a, l, sa, sl, bsize);
      pc_tree->split[i]->partitioning = PARTITION_NONE;
      rd_pick_sb_modes(cpi, tile, mi_row + y_idx, mi_col + x_idx, &rt, &dt,
                       split_subsize, &pc_tree->split[i]->none,
                       INT64_MAX, i);

      restore_context(cpi, mi_row, mi_col, a, l, sa, sl, bsize);

      if (rt == INT_MAX || dt == INT64_MAX) {
        chosen_rate = INT_MAX;
        chosen_dist = INT64_MAX;
        break;
      }

      chosen_rate += rt;
      chosen_dist += dt;

      if (i != 3)
        encode_sb(cpi, tile, tp,  mi_row + y_idx, mi_col + x_idx, 0,
                  split_subsize, pc_tree->split[i]);

      pl = partition_plane_context(xd, mi_row + y_idx, mi_col + x_idx,
                                   split_subsize);
      chosen_rate += cpi->partition_cost[pl][PARTITION_NONE];
    }
    pl = partition_plane_context(xd, mi_row, mi_col, bsize);
    if (chosen_rate < INT_MAX) {
      chosen_rate += cpi->partition_cost[pl][PARTITION_SPLIT];
      chosen_rd = RDCOST(x->rdmult, x->rddiv, chosen_rate, chosen_dist);
    }
  }

  // If last_part is better set the partitioning to that.
  if (last_part_rd < chosen_rd) {
    mi_8x8[0]->mbmi.sb_type = bsize;
    if (bsize >= BLOCK_8X8)
      pc_tree->partitioning = partition;
    chosen_rate = last_part_rate;
    chosen_dist = last_part_dist;
    chosen_rd = last_part_rd;
  }
  // If none was better set the partitioning to that.
  if (none_rd < chosen_rd) {
    if (bsize >= BLOCK_8X8)
      pc_tree->partitioning = PARTITION_NONE;
    chosen_rate = none_rate;
    chosen_dist = none_dist;
  }

  restore_context(cpi, mi_row, mi_col, a, l, sa, sl, bsize);

  // We must have chosen a partitioning and encoding or we'll fail later on.
  // No other opportunities for success.
  if ( bsize == BLOCK_64X64)
    assert(chosen_rate < INT_MAX && chosen_dist < INT64_MAX);

  if (do_recon) {
    int output_enabled = (bsize == BLOCK_64X64);

    // Check the projected output rate for this SB against it's target
    // and and if necessary apply a Q delta using segmentation to get
    // closer to the target.
    if ((cpi->oxcf.aq_mode == COMPLEXITY_AQ) && cm->seg.update_map) {
      vp9_select_in_frame_q_segment(cpi, mi_row, mi_col,
                                    output_enabled, chosen_rate);
    }

    if (cpi->oxcf.aq_mode == CYCLIC_REFRESH_AQ)
      vp9_cyclic_refresh_set_rate_and_dist_sb(cpi->cyclic_refresh,
                                              chosen_rate, chosen_dist);
    encode_sb(cpi, tile, tp, mi_row, mi_col, output_enabled, bsize,
              pc_tree);
  }

  *rate = chosen_rate;
  *dist = chosen_dist;
}

static const BLOCK_SIZE min_partition_size[BLOCK_SIZES] = {
  BLOCK_4X4,   BLOCK_4X4,   BLOCK_4X4,
  BLOCK_4X4,   BLOCK_4X4,   BLOCK_4X4,
  BLOCK_8X8,   BLOCK_8X8,   BLOCK_8X8,
  BLOCK_16X16, BLOCK_16X16, BLOCK_16X16,
  BLOCK_16X16
};

static const BLOCK_SIZE max_partition_size[BLOCK_SIZES] = {
  BLOCK_8X8,   BLOCK_16X16, BLOCK_16X16,
  BLOCK_16X16, BLOCK_32X32, BLOCK_32X32,
  BLOCK_32X32, BLOCK_64X64, BLOCK_64X64,
  BLOCK_64X64, BLOCK_64X64, BLOCK_64X64,
  BLOCK_64X64
};

// Look at all the mode_info entries for blocks that are part of this
// partition and find the min and max values for sb_type.
// At the moment this is designed to work on a 64x64 SB but could be
// adjusted to use a size parameter.
//
// The min and max are assumed to have been initialized prior to calling this
// function so repeat calls can accumulate a min and max of more than one sb64.
static void get_sb_partition_size_range(MACROBLOCKD *xd, MODE_INFO **mi_8x8,
                                        BLOCK_SIZE *min_block_size,
                                        BLOCK_SIZE *max_block_size,
                                        int bs_hist[BLOCK_SIZES]) {
  int sb_width_in_blocks = MI_BLOCK_SIZE;
  int sb_height_in_blocks  = MI_BLOCK_SIZE;
  int i, j;
  int index = 0;

  // Check the sb_type for each block that belongs to this region.
  for (i = 0; i < sb_height_in_blocks; ++i) {
    for (j = 0; j < sb_width_in_blocks; ++j) {
      MODE_INFO * mi = mi_8x8[index+j];
      BLOCK_SIZE sb_type = mi ? mi->mbmi.sb_type : 0;
      bs_hist[sb_type]++;
      *min_block_size = MIN(*min_block_size, sb_type);
      *max_block_size = MAX(*max_block_size, sb_type);
    }
    index += xd->mi_stride;
  }
}

// Next square block size less or equal than current block size.
static const BLOCK_SIZE next_square_size[BLOCK_SIZES] = {
  BLOCK_4X4, BLOCK_4X4, BLOCK_4X4,
  BLOCK_8X8, BLOCK_8X8, BLOCK_8X8,
  BLOCK_16X16, BLOCK_16X16, BLOCK_16X16,
  BLOCK_32X32, BLOCK_32X32, BLOCK_32X32,
  BLOCK_64X64
};

// Look at neighboring blocks and set a min and max partition size based on
// what they chose.
static void rd_auto_partition_range(VP9_COMP *cpi, const TileInfo *const tile,
                                    int mi_row, int mi_col,
                                    BLOCK_SIZE *min_block_size,
                                    BLOCK_SIZE *max_block_size) {
  VP9_COMMON *const cm = &cpi->common;
  MACROBLOCKD *const xd = &cpi->mb.e_mbd;
  MODE_INFO **mi = xd->mi;
  const int left_in_image = xd->left_available && mi[-1];
  const int above_in_image = xd->up_available && mi[-xd->mi_stride];
  const int row8x8_remaining = tile->mi_row_end - mi_row;
  const int col8x8_remaining = tile->mi_col_end - mi_col;
  int bh, bw;
  BLOCK_SIZE min_size = BLOCK_4X4;
  BLOCK_SIZE max_size = BLOCK_64X64;
  int i = 0;
  int bs_hist[BLOCK_SIZES] = {0};

  // Trap case where we do not have a prediction.
  if (left_in_image || above_in_image || cm->frame_type != KEY_FRAME) {
    // Default "min to max" and "max to min"
    min_size = BLOCK_64X64;
    max_size = BLOCK_4X4;

    // NOTE: each call to get_sb_partition_size_range() uses the previous
    // passed in values for min and max as a starting point.
    // Find the min and max partition used in previous frame at this location
    if (cm->frame_type != KEY_FRAME) {
      MODE_INFO **const prev_mi =
          &cm->prev_mi_grid_visible[mi_row * xd->mi_stride + mi_col];
      get_sb_partition_size_range(xd, prev_mi, &min_size, &max_size, bs_hist);
    }
    // Find the min and max partition sizes used in the left SB64
    if (left_in_image) {
      MODE_INFO **left_sb64_mi = &mi[-MI_BLOCK_SIZE];
      get_sb_partition_size_range(xd, left_sb64_mi, &min_size, &max_size,
                                  bs_hist);
    }
    // Find the min and max partition sizes used in the above SB64.
    if (above_in_image) {
      MODE_INFO **above_sb64_mi = &mi[-xd->mi_stride * MI_BLOCK_SIZE];
      get_sb_partition_size_range(xd, above_sb64_mi, &min_size, &max_size,
                                  bs_hist);
    }

    // adjust observed min and max
    if (cpi->sf.auto_min_max_partition_size == RELAXED_NEIGHBORING_MIN_MAX) {
      min_size = min_partition_size[min_size];
      max_size = max_partition_size[max_size];
    } else if (cpi->sf.auto_min_max_partition_size ==
               CONSTRAIN_NEIGHBORING_MIN_MAX) {
      // adjust the search range based on the histogram of the observed
      // partition sizes from left, above the previous co-located blocks
      int sum = 0;
      int first_moment = 0;
      int second_moment = 0;
      int var_unnormalized = 0;

      for (i = 0; i < BLOCK_SIZES; i++) {
        sum += bs_hist[i];
        first_moment += bs_hist[i] * i;
        second_moment += bs_hist[i] * i * i;
      }

      // if variance is small enough,
      // adjust the range around its mean size, which gives a tighter range
      var_unnormalized = second_moment - first_moment * first_moment / sum;
      if (var_unnormalized <= 4 * sum) {
        int mean = first_moment / sum;
        min_size = min_partition_size[mean];
        max_size = max_partition_size[mean];
      } else {
        min_size = min_partition_size[min_size];
        max_size = max_partition_size[max_size];
      }
    }
  }

  // Check border cases where max and min from neighbors may not be legal.
  max_size = find_partition_size(max_size,
                                 row8x8_remaining, col8x8_remaining,
                                 &bh, &bw);
  min_size = MIN(min_size, max_size);

  // When use_square_partition_only is true, make sure at least one square
  // partition is allowed by selecting the next smaller square size as
  // *min_block_size.
  if (cpi->sf.use_square_partition_only &&
      next_square_size[max_size] < min_size) {
     min_size = next_square_size[max_size];
  }

  *min_block_size = min_size;
  *max_block_size = max_size;
}

static void auto_partition_range(VP9_COMP *cpi, const TileInfo *const tile,
                                 int mi_row, int mi_col,
                                 BLOCK_SIZE *min_block_size,
                                 BLOCK_SIZE *max_block_size) {
  VP9_COMMON *const cm = &cpi->common;
  MACROBLOCKD *const xd = &cpi->mb.e_mbd;
  MODE_INFO **mi_8x8 = xd->mi;
  const int left_in_image = xd->left_available && mi_8x8[-1];
  const int above_in_image = xd->up_available &&
                             mi_8x8[-xd->mi_stride];
  int row8x8_remaining = tile->mi_row_end - mi_row;
  int col8x8_remaining = tile->mi_col_end - mi_col;
  int bh, bw;
  BLOCK_SIZE min_size = BLOCK_32X32;
  BLOCK_SIZE max_size = BLOCK_8X8;
  int bsl = mi_width_log2(BLOCK_64X64);
  const int search_range_ctrl = (((mi_row + mi_col) >> bsl) +
                       get_chessboard_index(cm->current_video_frame)) & 0x1;
  // Trap case where we do not have a prediction.
  if (search_range_ctrl &&
      (left_in_image || above_in_image || cm->frame_type != KEY_FRAME)) {
    int block;
    MODE_INFO **mi;
    BLOCK_SIZE sb_type;

    // Find the min and max partition sizes used in the left SB64.
    if (left_in_image) {
      MODE_INFO *cur_mi;
      mi = &mi_8x8[-1];
      for (block = 0; block < MI_BLOCK_SIZE; ++block) {
        cur_mi = mi[block * xd->mi_stride];
        sb_type = cur_mi ? cur_mi->mbmi.sb_type : 0;
        min_size = MIN(min_size, sb_type);
        max_size = MAX(max_size, sb_type);
      }
    }
    // Find the min and max partition sizes used in the above SB64.
    if (above_in_image) {
      mi = &mi_8x8[-xd->mi_stride * MI_BLOCK_SIZE];
      for (block = 0; block < MI_BLOCK_SIZE; ++block) {
        sb_type = mi[block] ? mi[block]->mbmi.sb_type : 0;
        min_size = MIN(min_size, sb_type);
        max_size = MAX(max_size, sb_type);
      }
    }

    min_size = min_partition_size[min_size];
    max_size = find_partition_size(max_size, row8x8_remaining, col8x8_remaining,
                                   &bh, &bw);
    min_size = MIN(min_size, max_size);
    min_size = MAX(min_size, BLOCK_8X8);
    max_size = MIN(max_size, BLOCK_32X32);
  } else {
    min_size = BLOCK_8X8;
    max_size = BLOCK_32X32;
  }

  *min_block_size = min_size;
  *max_block_size = max_size;
}

// TODO(jingning) refactor functions setting partition search range
static void set_partition_range(VP9_COMMON *cm, MACROBLOCKD *xd,
                                int mi_row, int mi_col, BLOCK_SIZE bsize,
                                BLOCK_SIZE *min_bs, BLOCK_SIZE *max_bs) {
  int mi_width  = num_8x8_blocks_wide_lookup[bsize];
  int mi_height = num_8x8_blocks_high_lookup[bsize];
  int idx, idy;

  MODE_INFO *mi;
  MODE_INFO **prev_mi =
      &cm->prev_mi_grid_visible[mi_row * cm->mi_stride + mi_col];
  BLOCK_SIZE bs, min_size, max_size;

  min_size = BLOCK_64X64;
  max_size = BLOCK_4X4;

  if (prev_mi) {
    for (idy = 0; idy < mi_height; ++idy) {
      for (idx = 0; idx < mi_width; ++idx) {
        mi = prev_mi[idy * cm->mi_stride + idx];
        bs = mi ? mi->mbmi.sb_type : bsize;
        min_size = MIN(min_size, bs);
        max_size = MAX(max_size, bs);
      }
    }
  }

  if (xd->left_available) {
    for (idy = 0; idy < mi_height; ++idy) {
      mi = xd->mi[idy * cm->mi_stride - 1];
      bs = mi ? mi->mbmi.sb_type : bsize;
      min_size = MIN(min_size, bs);
      max_size = MAX(max_size, bs);
    }
  }

  if (xd->up_available) {
    for (idx = 0; idx < mi_width; ++idx) {
      mi = xd->mi[idx - cm->mi_stride];
      bs = mi ? mi->mbmi.sb_type : bsize;
      min_size = MIN(min_size, bs);
      max_size = MAX(max_size, bs);
    }
  }

  if (min_size == max_size) {
    min_size = min_partition_size[min_size];
    max_size = max_partition_size[max_size];
  }

  *min_bs = min_size;
  *max_bs = max_size;
}

static INLINE void store_pred_mv(MACROBLOCK *x, PICK_MODE_CONTEXT *ctx) {
  vpx_memcpy(ctx->pred_mv, x->pred_mv, sizeof(x->pred_mv));
}

static INLINE void load_pred_mv(MACROBLOCK *x, PICK_MODE_CONTEXT *ctx) {
  vpx_memcpy(x->pred_mv, ctx->pred_mv, sizeof(x->pred_mv));
}

#if CONFIG_FP_MB_STATS
const int num_16x16_blocks_wide_lookup[BLOCK_SIZES] =
  {1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 4, 4};
const int num_16x16_blocks_high_lookup[BLOCK_SIZES] =
  {1, 1, 1, 1, 1, 1, 1, 2, 1, 2, 4, 2, 4};
const int qindex_skip_threshold_lookup[BLOCK_SIZES] =
  {0, 10, 10, 30, 40, 40, 60, 80, 80, 90, 100, 100, 120};
const int qindex_split_threshold_lookup[BLOCK_SIZES] =
  {0, 3, 3, 7, 15, 15, 30, 40, 40, 60, 80, 80, 120};
const int complexity_16x16_blocks_threshold[BLOCK_SIZES] =
  {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 4, 4, 6};

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

// TODO(jingning,jimbankoski,rbultje): properly skip partition types that are
// unlikely to be selected depending on previous rate-distortion optimization
// results, for encoding speed-up.
static void rd_pick_partition(VP9_COMP *cpi, const TileInfo *const tile,
                              TOKENEXTRA **tp, int mi_row,
                              int mi_col, BLOCK_SIZE bsize, int *rate,
                              int64_t *dist, int64_t best_rd,
                              PC_TREE *pc_tree) {
  VP9_COMMON *const cm = &cpi->common;
  MACROBLOCK *const x = &cpi->mb;
  MACROBLOCKD *const xd = &x->e_mbd;
  const int mi_step = num_8x8_blocks_wide_lookup[bsize] / 2;
  ENTROPY_CONTEXT l[16 * MAX_MB_PLANE], a[16 * MAX_MB_PLANE];
  PARTITION_CONTEXT sl[8], sa[8];
  TOKENEXTRA *tp_orig = *tp;
  PICK_MODE_CONTEXT *ctx = &pc_tree->none;
  int i, pl;
  BLOCK_SIZE subsize;
  int this_rate, sum_rate = 0, best_rate = INT_MAX;
  int64_t this_dist, sum_dist = 0, best_dist = INT64_MAX;
  int64_t sum_rd = 0;
  int do_split = bsize >= BLOCK_8X8;
  int do_rect = 1;

  // Override skipping rectangular partition operations for edge blocks
  const int force_horz_split = (mi_row + mi_step >= cm->mi_rows);
  const int force_vert_split = (mi_col + mi_step >= cm->mi_cols);
  const int xss = x->e_mbd.plane[1].subsampling_x;
  const int yss = x->e_mbd.plane[1].subsampling_y;

  BLOCK_SIZE min_size = cpi->sf.min_partition_size;
  BLOCK_SIZE max_size = cpi->sf.max_partition_size;

#if CONFIG_FP_MB_STATS
  unsigned int src_diff_var = UINT_MAX;
  int none_complexity = 0;
#endif

  int partition_none_allowed = !force_horz_split && !force_vert_split;
  int partition_horz_allowed = !force_vert_split && yss <= xss &&
                               bsize >= BLOCK_8X8;
  int partition_vert_allowed = !force_horz_split && xss <= yss &&
                               bsize >= BLOCK_8X8;
  (void) *tp_orig;

  assert(num_8x8_blocks_wide_lookup[bsize] ==
             num_8x8_blocks_high_lookup[bsize]);

  set_offsets(cpi, tile, mi_row, mi_col, bsize);

  if (bsize == BLOCK_16X16 && cpi->oxcf.aq_mode)
    x->mb_energy = vp9_block_energy(cpi, x, bsize);

  if (cpi->sf.cb_partition_search && bsize == BLOCK_16X16) {
    int cb_partition_search_ctrl = ((pc_tree->index == 0 || pc_tree->index == 3)
        + get_chessboard_index(cm->current_video_frame)) & 0x1;

    if (cb_partition_search_ctrl && bsize > min_size && bsize < max_size)
      set_partition_range(cm, xd, mi_row, mi_col, bsize, &min_size, &max_size);
  }

  // Determine partition types in search according to the speed features.
  // The threshold set here has to be of square block size.
  if (cpi->sf.auto_min_max_partition_size) {
    partition_none_allowed &= (bsize <= max_size && bsize >= min_size);
    partition_horz_allowed &= ((bsize <= max_size && bsize > min_size) ||
                                force_horz_split);
    partition_vert_allowed &= ((bsize <= max_size && bsize > min_size) ||
                                force_vert_split);
    do_split &= bsize > min_size;
  }
  if (cpi->sf.use_square_partition_only) {
    partition_horz_allowed &= force_horz_split;
    partition_vert_allowed &= force_vert_split;
  }

  save_context(cpi, mi_row, mi_col, a, l, sa, sl, bsize);

#if CONFIG_FP_MB_STATS
  if (cpi->use_fp_mb_stats) {
    set_offsets(cpi, tile, mi_row, mi_col, bsize);
    src_diff_var = get_sby_perpixel_diff_variance(cpi, &cpi->mb.plane[0].src,
                                                  mi_row, mi_col, bsize);
  }
#endif

#if CONFIG_FP_MB_STATS
  // Decide whether we shall split directly and skip searching NONE by using
  // the first pass block statistics
  if (cpi->use_fp_mb_stats && bsize >= BLOCK_32X32 && do_split &&
      partition_none_allowed && src_diff_var > 4 &&
      cm->base_qindex < qindex_split_threshold_lookup[bsize]) {
    int mb_row = mi_row >> 1;
    int mb_col = mi_col >> 1;
    int mb_row_end =
        MIN(mb_row + num_16x16_blocks_high_lookup[bsize], cm->mb_rows);
    int mb_col_end =
        MIN(mb_col + num_16x16_blocks_wide_lookup[bsize], cm->mb_cols);
    int r, c;

    // compute a complexity measure, basically measure inconsistency of motion
    // vectors obtained from the first pass in the current block
    for (r = mb_row; r < mb_row_end ; r++) {
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
    rd_pick_sb_modes(cpi, tile, mi_row, mi_col, &this_rate, &this_dist, bsize,
                     ctx, best_rd, 0);
    if (this_rate != INT_MAX) {
      if (bsize >= BLOCK_8X8) {
        pl = partition_plane_context(xd, mi_row, mi_col, bsize);
        this_rate += cpi->partition_cost[pl][PARTITION_NONE];
      }
      sum_rd = RDCOST(x->rdmult, x->rddiv, this_rate, this_dist);

      if (sum_rd < best_rd) {
        int64_t dist_breakout_thr = cpi->sf.partition_search_breakout_dist_thr;
        int rate_breakout_thr = cpi->sf.partition_search_breakout_rate_thr;

        best_rate = this_rate;
        best_dist = this_dist;
        best_rd = sum_rd;
        if (bsize >= BLOCK_8X8)
          pc_tree->partitioning = PARTITION_NONE;

        // Adjust dist breakout threshold according to the partition size.
        dist_breakout_thr >>= 8 - (b_width_log2(bsize) +
            b_height_log2(bsize));

        // If all y, u, v transform blocks in this partition are skippable, and
        // the dist & rate are within the thresholds, the partition search is
        // terminated for current branch of the partition search tree.
        // The dist & rate thresholds are set to 0 at speed 0 to disable the
        // early termination at that speed.
        if (!x->e_mbd.lossless &&
            (ctx->skippable && best_dist < dist_breakout_thr &&
            best_rate < rate_breakout_thr)) {
          do_split = 0;
          do_rect = 0;
        }

#if CONFIG_FP_MB_STATS
        // Check if every 16x16 first pass block statistics has zero
        // motion and the corresponding first pass residue is small enough.
        // If that is the case, check the difference variance between the
        // current frame and the last frame. If the variance is small enough,
        // stop further splitting in RD optimization
        if (cpi->use_fp_mb_stats && do_split != 0 &&
            cm->base_qindex > qindex_skip_threshold_lookup[bsize]) {
          int mb_row = mi_row >> 1;
          int mb_col = mi_col >> 1;
          int mb_row_end =
              MIN(mb_row + num_16x16_blocks_high_lookup[bsize], cm->mb_rows);
          int mb_col_end =
              MIN(mb_col + num_16x16_blocks_wide_lookup[bsize], cm->mb_cols);
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
              set_offsets(cpi, tile, mi_row, mi_col, bsize);
              src_diff_var = get_sby_perpixel_diff_variance(
                  cpi, &cpi->mb.plane[0].src, mi_row, mi_col, bsize);
            }
            if (src_diff_var < 8) {
              do_split = 0;
              do_rect = 0;
            }
          }
        }
#endif
      }
    }
    restore_context(cpi, mi_row, mi_col, a, l, sa, sl, bsize);
  }

  // store estimated motion vector
  if (cpi->sf.adaptive_motion_search)
    store_pred_mv(x, ctx);

  // PARTITION_SPLIT
  sum_rd = 0;
  // TODO(jingning): use the motion vectors given by the above search as
  // the starting point of motion search in the following partition type check.
  if (do_split) {
    subsize = get_subsize(bsize, PARTITION_SPLIT);
    if (bsize == BLOCK_8X8) {
      i = 4;
      if (cpi->sf.adaptive_pred_interp_filter && partition_none_allowed)
        pc_tree->leaf_split[0]->pred_interp_filter =
            ctx->mic.mbmi.interp_filter;
      rd_pick_sb_modes(cpi, tile, mi_row, mi_col, &sum_rate, &sum_dist, subsize,
                       pc_tree->leaf_split[0], best_rd, 0);
      if (sum_rate == INT_MAX)
        sum_rd = INT64_MAX;
      else
        sum_rd = RDCOST(x->rdmult, x->rddiv, sum_rate, sum_dist);
    } else {
      for (i = 0; i < 4 && sum_rd < best_rd; ++i) {
      const int x_idx = (i & 1) * mi_step;
      const int y_idx = (i >> 1) * mi_step;

        if (mi_row + y_idx >= cm->mi_rows || mi_col + x_idx >= cm->mi_cols)
          continue;

        if (cpi->sf.adaptive_motion_search)
          load_pred_mv(x, ctx);

        pc_tree->split[i]->index = i;
        rd_pick_partition(cpi, tile, tp, mi_row + y_idx, mi_col + x_idx,
                          subsize, &this_rate, &this_dist,
                          best_rd - sum_rd, pc_tree->split[i]);

        if (this_rate == INT_MAX) {
          sum_rd = INT64_MAX;
        } else {
          sum_rate += this_rate;
          sum_dist += this_dist;
          sum_rd = RDCOST(x->rdmult, x->rddiv, sum_rate, sum_dist);
        }
      }
    }

    if (sum_rd < best_rd && i == 4) {
      pl = partition_plane_context(xd, mi_row, mi_col, bsize);
      sum_rate += cpi->partition_cost[pl][PARTITION_SPLIT];
      sum_rd = RDCOST(x->rdmult, x->rddiv, sum_rate, sum_dist);

      if (sum_rd < best_rd) {
        best_rate = sum_rate;
        best_dist = sum_dist;
        best_rd = sum_rd;
        pc_tree->partitioning = PARTITION_SPLIT;
      }
    } else {
      // skip rectangular partition test when larger block size
      // gives better rd cost
      if (cpi->sf.less_rectangular_check)
        do_rect &= !partition_none_allowed;
    }
    restore_context(cpi, mi_row, mi_col, a, l, sa, sl, bsize);
  }

  // PARTITION_HORZ
  if (partition_horz_allowed && do_rect) {
    subsize = get_subsize(bsize, PARTITION_HORZ);
    if (cpi->sf.adaptive_motion_search)
      load_pred_mv(x, ctx);
    if (cpi->sf.adaptive_pred_interp_filter && bsize == BLOCK_8X8 &&
        partition_none_allowed)
      pc_tree->horizontal[0].pred_interp_filter =
          ctx->mic.mbmi.interp_filter;
    rd_pick_sb_modes(cpi, tile, mi_row, mi_col, &sum_rate, &sum_dist, subsize,
                     &pc_tree->horizontal[0], best_rd, 0);
    sum_rd = RDCOST(x->rdmult, x->rddiv, sum_rate, sum_dist);

    if (sum_rd < best_rd && mi_row + mi_step < cm->mi_rows) {
      PICK_MODE_CONTEXT *ctx = &pc_tree->horizontal[0];
      update_state(cpi, ctx, mi_row, mi_col, subsize, 0);
      encode_superblock(cpi, tp, 0, mi_row, mi_col, subsize, ctx);

      if (cpi->sf.adaptive_motion_search)
        load_pred_mv(x, ctx);
      if (cpi->sf.adaptive_pred_interp_filter && bsize == BLOCK_8X8 &&
          partition_none_allowed)
        pc_tree->horizontal[1].pred_interp_filter =
            ctx->mic.mbmi.interp_filter;
      rd_pick_sb_modes(cpi, tile, mi_row + mi_step, mi_col, &this_rate,
                       &this_dist, subsize, &pc_tree->horizontal[1],
                       best_rd - sum_rd, 1);
      if (this_rate == INT_MAX) {
        sum_rd = INT64_MAX;
      } else {
        sum_rate += this_rate;
        sum_dist += this_dist;
        sum_rd = RDCOST(x->rdmult, x->rddiv, sum_rate, sum_dist);
      }
    }
    if (sum_rd < best_rd) {
      pl = partition_plane_context(xd, mi_row, mi_col, bsize);
      sum_rate += cpi->partition_cost[pl][PARTITION_HORZ];
      sum_rd = RDCOST(x->rdmult, x->rddiv, sum_rate, sum_dist);
      if (sum_rd < best_rd) {
        best_rd = sum_rd;
        best_rate = sum_rate;
        best_dist = sum_dist;
        pc_tree->partitioning = PARTITION_HORZ;
      }
    }
    restore_context(cpi, mi_row, mi_col, a, l, sa, sl, bsize);
  }
  // PARTITION_VERT
  if (partition_vert_allowed && do_rect) {
    subsize = get_subsize(bsize, PARTITION_VERT);

    if (cpi->sf.adaptive_motion_search)
      load_pred_mv(x, ctx);
    if (cpi->sf.adaptive_pred_interp_filter && bsize == BLOCK_8X8 &&
        partition_none_allowed)
      pc_tree->vertical[0].pred_interp_filter =
          ctx->mic.mbmi.interp_filter;
    rd_pick_sb_modes(cpi, tile, mi_row, mi_col, &sum_rate, &sum_dist, subsize,
                     &pc_tree->vertical[0], best_rd, 0);
    sum_rd = RDCOST(x->rdmult, x->rddiv, sum_rate, sum_dist);
    if (sum_rd < best_rd && mi_col + mi_step < cm->mi_cols) {
      update_state(cpi, &pc_tree->vertical[0], mi_row, mi_col, subsize, 0);
      encode_superblock(cpi, tp, 0, mi_row, mi_col, subsize,
                        &pc_tree->vertical[0]);

      if (cpi->sf.adaptive_motion_search)
        load_pred_mv(x, ctx);
      if (cpi->sf.adaptive_pred_interp_filter && bsize == BLOCK_8X8 &&
          partition_none_allowed)
        pc_tree->vertical[1].pred_interp_filter =
            ctx->mic.mbmi.interp_filter;
      rd_pick_sb_modes(cpi, tile, mi_row, mi_col + mi_step, &this_rate,
                       &this_dist, subsize,
                       &pc_tree->vertical[1], best_rd - sum_rd,
                       1);
      if (this_rate == INT_MAX) {
        sum_rd = INT64_MAX;
      } else {
        sum_rate += this_rate;
        sum_dist += this_dist;
        sum_rd = RDCOST(x->rdmult, x->rddiv, sum_rate, sum_dist);
      }
    }
    if (sum_rd < best_rd) {
      pl = partition_plane_context(xd, mi_row, mi_col, bsize);
      sum_rate += cpi->partition_cost[pl][PARTITION_VERT];
      sum_rd = RDCOST(x->rdmult, x->rddiv, sum_rate, sum_dist);
      if (sum_rd < best_rd) {
        best_rate = sum_rate;
        best_dist = sum_dist;
        best_rd = sum_rd;
        pc_tree->partitioning = PARTITION_VERT;
      }
    }
    restore_context(cpi, mi_row, mi_col, a, l, sa, sl, bsize);
  }

  // TODO(jbb): This code added so that we avoid static analysis
  // warning related to the fact that best_rd isn't used after this
  // point.  This code should be refactored so that the duplicate
  // checks occur in some sub function and thus are used...
  (void) best_rd;
  *rate = best_rate;
  *dist = best_dist;

  if (best_rate < INT_MAX && best_dist < INT64_MAX && pc_tree->index != 3) {
    int output_enabled = (bsize == BLOCK_64X64);

    // Check the projected output rate for this SB against it's target
    // and and if necessary apply a Q delta using segmentation to get
    // closer to the target.
    if ((cpi->oxcf.aq_mode == COMPLEXITY_AQ) && cm->seg.update_map)
      vp9_select_in_frame_q_segment(cpi, mi_row, mi_col, output_enabled,
                                    best_rate);
    if (cpi->oxcf.aq_mode == CYCLIC_REFRESH_AQ)
      vp9_cyclic_refresh_set_rate_and_dist_sb(cpi->cyclic_refresh,
                                              best_rate, best_dist);

    encode_sb(cpi, tile, tp, mi_row, mi_col, output_enabled, bsize, pc_tree);
  }

  if (bsize == BLOCK_64X64) {
    assert(tp_orig < *tp);
    assert(best_rate < INT_MAX);
    assert(best_dist < INT64_MAX);
  } else {
    assert(tp_orig == *tp);
  }
}

static void encode_rd_sb_row(VP9_COMP *cpi, const TileInfo *const tile,
                             int mi_row, TOKENEXTRA **tp) {
  VP9_COMMON *const cm = &cpi->common;
  MACROBLOCKD *const xd = &cpi->mb.e_mbd;
  SPEED_FEATURES *const sf = &cpi->sf;
  int mi_col;

  // Initialize the left context for the new SB row
  vpx_memset(&xd->left_context, 0, sizeof(xd->left_context));
  vpx_memset(xd->left_seg_context, 0, sizeof(xd->left_seg_context));

  // Code each SB in the row
  for (mi_col = tile->mi_col_start; mi_col < tile->mi_col_end;
       mi_col += MI_BLOCK_SIZE) {
    int dummy_rate;
    int64_t dummy_dist;

    int i;

    if (sf->adaptive_pred_interp_filter) {
      for (i = 0; i < 64; ++i)
        cpi->leaf_tree[i].pred_interp_filter = SWITCHABLE;

      for (i = 0; i < 64; ++i) {
        cpi->pc_tree[i].vertical[0].pred_interp_filter = SWITCHABLE;
        cpi->pc_tree[i].vertical[1].pred_interp_filter = SWITCHABLE;
        cpi->pc_tree[i].horizontal[0].pred_interp_filter = SWITCHABLE;
        cpi->pc_tree[i].horizontal[1].pred_interp_filter = SWITCHABLE;
      }
    }

    vp9_zero(cpi->mb.pred_mv);
    cpi->pc_root->index = 0;

    if ((sf->partition_search_type == SEARCH_PARTITION &&
         sf->use_lastframe_partitioning) ||
         sf->partition_search_type == FIXED_PARTITION ||
         sf->partition_search_type == VAR_BASED_PARTITION ||
         sf->partition_search_type == VAR_BASED_FIXED_PARTITION) {
      const int idx_str = cm->mi_stride * mi_row + mi_col;
      MODE_INFO **mi = cm->mi_grid_visible + idx_str;
      MODE_INFO **prev_mi = cm->prev_mi_grid_visible + idx_str;
      cpi->mb.source_variance = UINT_MAX;
      if (sf->partition_search_type == FIXED_PARTITION) {
        set_offsets(cpi, tile, mi_row, mi_col, BLOCK_64X64);
        set_fixed_partitioning(cpi, tile, mi, mi_row, mi_col,
                               sf->always_this_block_size);
        rd_use_partition(cpi, tile, mi, tp, mi_row, mi_col, BLOCK_64X64,
                         &dummy_rate, &dummy_dist, 1, cpi->pc_root);
      } else if (cpi->skippable_frame ||
                 sf->partition_search_type == VAR_BASED_FIXED_PARTITION) {
        BLOCK_SIZE bsize;
        set_offsets(cpi, tile, mi_row, mi_col, BLOCK_64X64);
        bsize = get_rd_var_based_fixed_partition(cpi, mi_row, mi_col);
        set_fixed_partitioning(cpi, tile, mi, mi_row, mi_col, bsize);
        rd_use_partition(cpi, tile, mi, tp, mi_row, mi_col, BLOCK_64X64,
                         &dummy_rate, &dummy_dist, 1, cpi->pc_root);
      } else if (sf->partition_search_type == VAR_BASED_PARTITION) {
        choose_partitioning(cpi, tile, mi_row, mi_col);
        rd_use_partition(cpi, tile, mi, tp, mi_row, mi_col, BLOCK_64X64,
                         &dummy_rate, &dummy_dist, 1, cpi->pc_root);
      } else {
        GF_GROUP * gf_grp = &cpi->twopass.gf_group;
        int last_was_mid_sequence_overlay = 0;
        if ((cpi->oxcf.pass == 2) && (gf_grp->index)) {
          if (gf_grp->update_type[gf_grp->index - 1] == OVERLAY_UPDATE)
            last_was_mid_sequence_overlay = 1;
        }
        if ((cpi->rc.frames_since_key
            % sf->last_partitioning_redo_frequency) == 0
            || last_was_mid_sequence_overlay
            || cm->prev_mi == 0
            || cm->show_frame == 0
            || cm->frame_type == KEY_FRAME
            || cpi->rc.is_src_frame_alt_ref
            || ((sf->use_lastframe_partitioning ==
                 LAST_FRAME_PARTITION_LOW_MOTION) &&
                 sb_has_motion(cm, prev_mi, sf->lf_motion_threshold))) {
          // If required set upper and lower partition size limits
          if (sf->auto_min_max_partition_size) {
            set_offsets(cpi, tile, mi_row, mi_col, BLOCK_64X64);
            rd_auto_partition_range(cpi, tile, mi_row, mi_col,
                                    &sf->min_partition_size,
                                    &sf->max_partition_size);
          }
          rd_pick_partition(cpi, tile, tp, mi_row, mi_col, BLOCK_64X64,
                            &dummy_rate, &dummy_dist, INT64_MAX,
                            cpi->pc_root);
        } else {
          if (sf->constrain_copy_partition &&
              sb_has_motion(cm, prev_mi, sf->lf_motion_threshold))
            constrain_copy_partitioning(cpi, tile, mi, prev_mi,
                                        mi_row, mi_col, BLOCK_16X16);
          else
            copy_partitioning(cm, mi, prev_mi);
          rd_use_partition(cpi, tile, mi, tp, mi_row, mi_col, BLOCK_64X64,
                           &dummy_rate, &dummy_dist, 1, cpi->pc_root);
        }
      }
    } else {
      // If required set upper and lower partition size limits
      if (sf->auto_min_max_partition_size) {
        set_offsets(cpi, tile, mi_row, mi_col, BLOCK_64X64);
        rd_auto_partition_range(cpi, tile, mi_row, mi_col,
                                &sf->min_partition_size,
                                &sf->max_partition_size);
      }
      rd_pick_partition(cpi, tile, tp, mi_row, mi_col, BLOCK_64X64,
                        &dummy_rate, &dummy_dist, INT64_MAX, cpi->pc_root);
    }
  }
}

static void init_encode_frame_mb_context(VP9_COMP *cpi) {
  MACROBLOCK *const x = &cpi->mb;
  VP9_COMMON *const cm = &cpi->common;
  MACROBLOCKD *const xd = &x->e_mbd;
  const int aligned_mi_cols = mi_cols_aligned_to_sb(cm->mi_cols);

  // Copy data over into macro block data structures.
  vp9_setup_src_planes(x, cpi->Source, 0, 0);

  vp9_setup_block_planes(&x->e_mbd, cm->subsampling_x, cm->subsampling_y);

  // Note: this memset assumes above_context[0], [1] and [2]
  // are allocated as part of the same buffer.
  vpx_memset(xd->above_context[0], 0,
             sizeof(*xd->above_context[0]) *
             2 * aligned_mi_cols * MAX_MB_PLANE);
  vpx_memset(xd->above_seg_context, 0,
             sizeof(*xd->above_seg_context) * aligned_mi_cols);
}

static int check_dual_ref_flags(VP9_COMP *cpi) {
  const int ref_flags = cpi->ref_frame_flags;

  if (vp9_segfeature_active(&cpi->common.seg, 1, SEG_LVL_REF_FRAME)) {
    return 0;
  } else {
    return (!!(ref_flags & VP9_GOLD_FLAG) + !!(ref_flags & VP9_LAST_FLAG)
        + !!(ref_flags & VP9_ALT_FLAG)) >= 2;
  }
}

static void reset_skip_tx_size(VP9_COMMON *cm, TX_SIZE max_tx_size) {
  int mi_row, mi_col;
  const int mis = cm->mi_stride;
  MODE_INFO **mi_ptr = cm->mi_grid_visible;

  for (mi_row = 0; mi_row < cm->mi_rows; ++mi_row, mi_ptr += mis) {
    for (mi_col = 0; mi_col < cm->mi_cols; ++mi_col) {
      if (mi_ptr[mi_col]->mbmi.tx_size > max_tx_size)
        mi_ptr[mi_col]->mbmi.tx_size = max_tx_size;
    }
  }
}

static MV_REFERENCE_FRAME get_frame_type(const VP9_COMP *cpi) {
  if (frame_is_intra_only(&cpi->common))
    return INTRA_FRAME;
  else if (cpi->rc.is_src_frame_alt_ref && cpi->refresh_golden_frame)
    return ALTREF_FRAME;
  else if (cpi->refresh_golden_frame || cpi->refresh_alt_ref_frame)
    return GOLDEN_FRAME;
  else
    return LAST_FRAME;
}

static TX_MODE select_tx_mode(const VP9_COMP *cpi) {
  if (cpi->mb.e_mbd.lossless)
    return ONLY_4X4;
  if (cpi->sf.tx_size_search_method == USE_LARGESTALL)
    return ALLOW_32X32;
  else if (cpi->sf.tx_size_search_method == USE_FULL_RD||
           cpi->sf.tx_size_search_method == USE_TX_8X8)
    return TX_MODE_SELECT;
  else
    return cpi->common.tx_mode;
}

static void nonrd_pick_sb_modes(VP9_COMP *cpi, const TileInfo *const tile,
                                int mi_row, int mi_col,
                                int *rate, int64_t *dist,
                                BLOCK_SIZE bsize, PICK_MODE_CONTEXT *ctx) {
  VP9_COMMON *const cm = &cpi->common;
  MACROBLOCK *const x = &cpi->mb;
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *mbmi;
  set_offsets(cpi, tile, mi_row, mi_col, bsize);
  mbmi = &xd->mi[0]->mbmi;
  mbmi->sb_type = bsize;

  if (cpi->oxcf.aq_mode == CYCLIC_REFRESH_AQ && cm->seg.enabled)
    if (mbmi->segment_id && x->in_static_area)
      x->rdmult = vp9_cyclic_refresh_get_rdmult(cpi->cyclic_refresh);

  if (vp9_segfeature_active(&cm->seg, mbmi->segment_id, SEG_LVL_SKIP))
    set_mode_info_seg_skip(x, cm->tx_mode, rate, dist, bsize);
  else
    vp9_pick_inter_mode(cpi, x, tile, mi_row, mi_col, rate, dist, bsize, ctx);

  duplicate_mode_info_in_sb(cm, xd, mi_row, mi_col, bsize);
}

static void fill_mode_info_sb(VP9_COMMON *cm, MACROBLOCK *x,
                              int mi_row, int mi_col,
                              BLOCK_SIZE bsize, BLOCK_SIZE subsize,
                              PC_TREE *pc_tree) {
  MACROBLOCKD *xd = &x->e_mbd;
  int bsl = b_width_log2(bsize), hbs = (1 << bsl) / 4;
  PARTITION_TYPE partition = pc_tree->partitioning;

  assert(bsize >= BLOCK_8X8);

  if (mi_row >= cm->mi_rows || mi_col >= cm->mi_cols)
    return;

  switch (partition) {
    case PARTITION_NONE:
      set_modeinfo_offsets(cm, xd, mi_row, mi_col);
      *(xd->mi[0]) = pc_tree->none.mic;
      duplicate_mode_info_in_sb(cm, xd, mi_row, mi_col, bsize);
      break;
    case PARTITION_VERT:
      set_modeinfo_offsets(cm, xd, mi_row, mi_col);
      *(xd->mi[0]) = pc_tree->vertical[0].mic;
      duplicate_mode_info_in_sb(cm, xd, mi_row, mi_col, bsize);

      if (mi_col + hbs < cm->mi_cols) {
        set_modeinfo_offsets(cm, xd, mi_row, mi_col + hbs);
        *(xd->mi[0]) = pc_tree->vertical[1].mic;
        duplicate_mode_info_in_sb(cm, xd, mi_row, mi_col + hbs, bsize);
      }
      break;
    case PARTITION_HORZ:
      set_modeinfo_offsets(cm, xd, mi_row, mi_col);
      *(xd->mi[0]) = pc_tree->horizontal[0].mic;
      duplicate_mode_info_in_sb(cm, xd, mi_row, mi_col, bsize);
      if (mi_row + hbs < cm->mi_rows) {
        set_modeinfo_offsets(cm, xd, mi_row + hbs, mi_col);
        *(xd->mi[0]) = pc_tree->horizontal[1].mic;
        duplicate_mode_info_in_sb(cm, xd, mi_row + hbs, mi_col, bsize);
      }
      break;
    case PARTITION_SPLIT: {
      BLOCK_SIZE subsubsize = get_subsize(subsize, PARTITION_SPLIT);
      fill_mode_info_sb(cm, x, mi_row, mi_col, subsize,
                        subsubsize, pc_tree->split[0]);
      fill_mode_info_sb(cm, x, mi_row, mi_col + hbs, subsize,
                        subsubsize, pc_tree->split[1]);
      fill_mode_info_sb(cm, x, mi_row + hbs, mi_col, subsize,
                        subsubsize, pc_tree->split[2]);
      fill_mode_info_sb(cm, x, mi_row + hbs, mi_col + hbs, subsize,
                        subsubsize, pc_tree->split[3]);
      break;
    }
    default:
      break;
  }
}

static void nonrd_pick_partition(VP9_COMP *cpi, const TileInfo *const tile,
                                 TOKENEXTRA **tp, int mi_row,
                                 int mi_col, BLOCK_SIZE bsize, int *rate,
                                 int64_t *dist, int do_recon, int64_t best_rd,
                                 PC_TREE *pc_tree) {
  const SPEED_FEATURES *const sf = &cpi->sf;
  const VP9EncoderConfig *const oxcf = &cpi->oxcf;
  VP9_COMMON *const cm = &cpi->common;
  MACROBLOCK *const x = &cpi->mb;
  MACROBLOCKD *const xd = &x->e_mbd;
  const int ms = num_8x8_blocks_wide_lookup[bsize] / 2;
  TOKENEXTRA *tp_orig = *tp;
  PICK_MODE_CONTEXT *ctx = &pc_tree->none;
  int i;
  BLOCK_SIZE subsize = bsize;
  int this_rate, sum_rate = 0, best_rate = INT_MAX;
  int64_t this_dist, sum_dist = 0, best_dist = INT64_MAX;
  int64_t sum_rd = 0;
  int do_split = bsize >= BLOCK_8X8;
  int do_rect = 1;
  // Override skipping rectangular partition operations for edge blocks
  const int force_horz_split = (mi_row + ms >= cm->mi_rows);
  const int force_vert_split = (mi_col + ms >= cm->mi_cols);
  const int xss = x->e_mbd.plane[1].subsampling_x;
  const int yss = x->e_mbd.plane[1].subsampling_y;

  int partition_none_allowed = !force_horz_split && !force_vert_split;
  int partition_horz_allowed = !force_vert_split && yss <= xss &&
                               bsize >= BLOCK_8X8;
  int partition_vert_allowed = !force_horz_split && xss <= yss &&
                               bsize >= BLOCK_8X8;
  (void) *tp_orig;

  assert(num_8x8_blocks_wide_lookup[bsize] ==
             num_8x8_blocks_high_lookup[bsize]);

  // Determine partition types in search according to the speed features.
  // The threshold set here has to be of square block size.
  if (sf->auto_min_max_partition_size) {
    partition_none_allowed &= (bsize <= sf->max_partition_size &&
                               bsize >= sf->min_partition_size);
    partition_horz_allowed &= ((bsize <= sf->max_partition_size &&
                                bsize > sf->min_partition_size) ||
                                force_horz_split);
    partition_vert_allowed &= ((bsize <= sf->max_partition_size &&
                                bsize > sf->min_partition_size) ||
                                force_vert_split);
    do_split &= bsize > sf->min_partition_size;
  }
  if (sf->use_square_partition_only) {
    partition_horz_allowed &= force_horz_split;
    partition_vert_allowed &= force_vert_split;
  }

  // PARTITION_NONE
  if (partition_none_allowed) {
    nonrd_pick_sb_modes(cpi, tile, mi_row, mi_col,
                        &this_rate, &this_dist, bsize, ctx);
    ctx->mic.mbmi = xd->mi[0]->mbmi;
    ctx->skip_txfm[0] = x->skip_txfm[0];
    ctx->skip = x->skip;

    if (this_rate != INT_MAX) {
      int pl = partition_plane_context(xd, mi_row, mi_col, bsize);
      this_rate += cpi->partition_cost[pl][PARTITION_NONE];
      sum_rd = RDCOST(x->rdmult, x->rddiv, this_rate, this_dist);
      if (sum_rd < best_rd) {
        int64_t stop_thresh = 4096;
        int64_t stop_thresh_rd;

        best_rate = this_rate;
        best_dist = this_dist;
        best_rd = sum_rd;
        if (bsize >= BLOCK_8X8)
          pc_tree->partitioning = PARTITION_NONE;

        // Adjust threshold according to partition size.
        stop_thresh >>= 8 - (b_width_log2(bsize) +
            b_height_log2(bsize));

        stop_thresh_rd = RDCOST(x->rdmult, x->rddiv, 0, stop_thresh);
        // If obtained distortion is very small, choose current partition
        // and stop splitting.
        if (!x->e_mbd.lossless && best_rd < stop_thresh_rd) {
          do_split = 0;
          do_rect = 0;
        }
      }
    }
  }

  // store estimated motion vector
  store_pred_mv(x, ctx);

  // PARTITION_SPLIT
  sum_rd = 0;
  if (do_split) {
    int pl = partition_plane_context(xd, mi_row, mi_col, bsize);
    sum_rate += cpi->partition_cost[pl][PARTITION_SPLIT];
    subsize = get_subsize(bsize, PARTITION_SPLIT);
    for (i = 0; i < 4 && sum_rd < best_rd; ++i) {
      const int x_idx = (i & 1) * ms;
      const int y_idx = (i >> 1) * ms;

      if (mi_row + y_idx >= cm->mi_rows || mi_col + x_idx >= cm->mi_cols)
        continue;
      load_pred_mv(x, ctx);
      nonrd_pick_partition(cpi, tile, tp, mi_row + y_idx, mi_col + x_idx,
                           subsize, &this_rate, &this_dist, 0,
                           best_rd - sum_rd, pc_tree->split[i]);

      if (this_rate == INT_MAX) {
        sum_rd = INT64_MAX;
      } else {
        sum_rate += this_rate;
        sum_dist += this_dist;
        sum_rd = RDCOST(x->rdmult, x->rddiv, sum_rate, sum_dist);
      }
    }

    if (sum_rd < best_rd) {
      best_rate = sum_rate;
      best_dist = sum_dist;
      best_rd = sum_rd;
      pc_tree->partitioning = PARTITION_SPLIT;
    } else {
      // skip rectangular partition test when larger block size
      // gives better rd cost
      if (sf->less_rectangular_check)
        do_rect &= !partition_none_allowed;
    }
  }

  // PARTITION_HORZ
  if (partition_horz_allowed && do_rect) {
    subsize = get_subsize(bsize, PARTITION_HORZ);
    if (sf->adaptive_motion_search)
      load_pred_mv(x, ctx);

    nonrd_pick_sb_modes(cpi, tile, mi_row, mi_col,
                        &this_rate, &this_dist, subsize,
                        &pc_tree->horizontal[0]);

    pc_tree->horizontal[0].mic.mbmi = xd->mi[0]->mbmi;
    pc_tree->horizontal[0].skip_txfm[0] = x->skip_txfm[0];
    pc_tree->horizontal[0].skip = x->skip;

    sum_rd = RDCOST(x->rdmult, x->rddiv, sum_rate, sum_dist);

    if (sum_rd < best_rd && mi_row + ms < cm->mi_rows) {
      load_pred_mv(x, ctx);
      nonrd_pick_sb_modes(cpi, tile, mi_row + ms, mi_col,
                          &this_rate, &this_dist, subsize,
                          &pc_tree->horizontal[1]);

      pc_tree->horizontal[1].mic.mbmi = xd->mi[0]->mbmi;
      pc_tree->horizontal[1].skip_txfm[0] = x->skip_txfm[0];
      pc_tree->horizontal[1].skip = x->skip;

      if (this_rate == INT_MAX) {
        sum_rd = INT64_MAX;
      } else {
        int pl = partition_plane_context(xd, mi_row, mi_col, bsize);
        this_rate += cpi->partition_cost[pl][PARTITION_HORZ];
        sum_rate += this_rate;
        sum_dist += this_dist;
        sum_rd = RDCOST(x->rdmult, x->rddiv, sum_rate, sum_dist);
      }
    }
    if (sum_rd < best_rd) {
      best_rd = sum_rd;
      best_rate = sum_rate;
      best_dist = sum_dist;
      pc_tree->partitioning = PARTITION_HORZ;
    }
  }

  // PARTITION_VERT
  if (partition_vert_allowed && do_rect) {
    subsize = get_subsize(bsize, PARTITION_VERT);

    if (sf->adaptive_motion_search)
      load_pred_mv(x, ctx);

    nonrd_pick_sb_modes(cpi, tile, mi_row, mi_col,
                        &this_rate, &this_dist, subsize,
                        &pc_tree->vertical[0]);
    pc_tree->vertical[0].mic.mbmi = xd->mi[0]->mbmi;
    pc_tree->vertical[0].skip_txfm[0] = x->skip_txfm[0];
    pc_tree->vertical[0].skip = x->skip;
    sum_rd = RDCOST(x->rdmult, x->rddiv, sum_rate, sum_dist);
    if (sum_rd < best_rd && mi_col + ms < cm->mi_cols) {
      load_pred_mv(x, ctx);
      nonrd_pick_sb_modes(cpi, tile, mi_row, mi_col + ms,
                          &this_rate, &this_dist, subsize,
                          &pc_tree->vertical[1]);
      pc_tree->vertical[1].mic.mbmi = xd->mi[0]->mbmi;
      pc_tree->vertical[1].skip_txfm[0] = x->skip_txfm[0];
      pc_tree->vertical[1].skip = x->skip;
      if (this_rate == INT_MAX) {
        sum_rd = INT64_MAX;
      } else {
        int pl = partition_plane_context(xd, mi_row, mi_col, bsize);
        this_rate += cpi->partition_cost[pl][PARTITION_VERT];
        sum_rate += this_rate;
        sum_dist += this_dist;
        sum_rd = RDCOST(x->rdmult, x->rddiv, sum_rate, sum_dist);
      }
    }
    if (sum_rd < best_rd) {
      best_rate = sum_rate;
      best_dist = sum_dist;
      best_rd = sum_rd;
      pc_tree->partitioning = PARTITION_VERT;
    }
  }
  // TODO(JBB): The following line is here just to avoid a static warning
  // that occurs because at this point we never again reuse best_rd
  // despite setting it here.  The code should be refactored to avoid this.
  (void) best_rd;

  *rate = best_rate;
  *dist = best_dist;

  if (best_rate == INT_MAX)
    return;

  // update mode info array
  subsize = get_subsize(bsize, pc_tree->partitioning);
  fill_mode_info_sb(cm, x, mi_row, mi_col, bsize, subsize,
                    pc_tree);

  if (best_rate < INT_MAX && best_dist < INT64_MAX && do_recon) {
    int output_enabled = (bsize == BLOCK_64X64);

    // Check the projected output rate for this SB against it's target
    // and and if necessary apply a Q delta using segmentation to get
    // closer to the target.
    if ((oxcf->aq_mode == COMPLEXITY_AQ) && cm->seg.update_map) {
      vp9_select_in_frame_q_segment(cpi, mi_row, mi_col, output_enabled,
                                    best_rate);
    }

    if (oxcf->aq_mode == CYCLIC_REFRESH_AQ)
      vp9_cyclic_refresh_set_rate_and_dist_sb(cpi->cyclic_refresh,
                                              best_rate, best_dist);

    encode_sb_rt(cpi, tile, tp, mi_row, mi_col, output_enabled, bsize, pc_tree);
  }

  if (bsize == BLOCK_64X64) {
    assert(tp_orig < *tp);
    assert(best_rate < INT_MAX);
    assert(best_dist < INT64_MAX);
  } else {
    assert(tp_orig == *tp);
  }
}

static void nonrd_use_partition(VP9_COMP *cpi,
                                const TileInfo *const tile,
                                MODE_INFO **mi,
                                TOKENEXTRA **tp,
                                int mi_row, int mi_col,
                                BLOCK_SIZE bsize, int output_enabled,
                                int *totrate, int64_t *totdist,
                                PC_TREE *pc_tree) {
  VP9_COMMON *const cm = &cpi->common;
  MACROBLOCK *const x = &cpi->mb;
  MACROBLOCKD *const xd = &x->e_mbd;
  const int bsl = b_width_log2(bsize), hbs = (1 << bsl) / 4;
  const int mis = cm->mi_stride;
  PARTITION_TYPE partition;
  BLOCK_SIZE subsize;
  int rate = INT_MAX;
  int64_t dist = INT64_MAX;

  if (mi_row >= cm->mi_rows || mi_col >= cm->mi_cols)
    return;

  subsize = (bsize >= BLOCK_8X8) ? mi[0]->mbmi.sb_type : BLOCK_4X4;
  partition = partition_lookup[bsl][subsize];

  switch (partition) {
    case PARTITION_NONE:
      nonrd_pick_sb_modes(cpi, tile, mi_row, mi_col, totrate, totdist,
                          subsize, &pc_tree->none);
      pc_tree->none.mic.mbmi = xd->mi[0]->mbmi;
      pc_tree->none.skip_txfm[0] = x->skip_txfm[0];
      pc_tree->none.skip = x->skip;
      break;
    case PARTITION_VERT:
      nonrd_pick_sb_modes(cpi, tile, mi_row, mi_col, totrate, totdist,
                          subsize, &pc_tree->vertical[0]);
      pc_tree->vertical[0].mic.mbmi = xd->mi[0]->mbmi;
      pc_tree->vertical[0].skip_txfm[0] = x->skip_txfm[0];
      pc_tree->vertical[0].skip = x->skip;
      if (mi_col + hbs < cm->mi_cols) {
        nonrd_pick_sb_modes(cpi, tile, mi_row, mi_col + hbs,
                            &rate, &dist, subsize, &pc_tree->vertical[1]);
        pc_tree->vertical[1].mic.mbmi = xd->mi[0]->mbmi;
        pc_tree->vertical[1].skip_txfm[0] = x->skip_txfm[0];
        pc_tree->vertical[1].skip = x->skip;
        if (rate != INT_MAX && dist != INT64_MAX &&
            *totrate != INT_MAX && *totdist != INT64_MAX) {
          *totrate += rate;
          *totdist += dist;
        }
      }
      break;
    case PARTITION_HORZ:
      nonrd_pick_sb_modes(cpi, tile, mi_row, mi_col, totrate, totdist,
                          subsize, &pc_tree->horizontal[0]);
      pc_tree->horizontal[0].mic.mbmi = xd->mi[0]->mbmi;
      pc_tree->horizontal[0].skip_txfm[0] = x->skip_txfm[0];
      pc_tree->horizontal[0].skip = x->skip;
      if (mi_row + hbs < cm->mi_rows) {
        nonrd_pick_sb_modes(cpi, tile, mi_row + hbs, mi_col,
                            &rate, &dist, subsize, &pc_tree->horizontal[0]);
        pc_tree->horizontal[1].mic.mbmi = xd->mi[0]->mbmi;
        pc_tree->horizontal[1].skip_txfm[0] = x->skip_txfm[0];
        pc_tree->horizontal[1].skip = x->skip;
        if (rate != INT_MAX && dist != INT64_MAX &&
            *totrate != INT_MAX && *totdist != INT64_MAX) {
          *totrate += rate;
          *totdist += dist;
        }
      }
      break;
    case PARTITION_SPLIT:
      subsize = get_subsize(bsize, PARTITION_SPLIT);
      nonrd_use_partition(cpi, tile, mi, tp, mi_row, mi_col,
                          subsize, output_enabled, totrate, totdist,
                          pc_tree->split[0]);
      nonrd_use_partition(cpi, tile, mi + hbs, tp,
                          mi_row, mi_col + hbs, subsize, output_enabled,
                          &rate, &dist, pc_tree->split[1]);
      if (rate != INT_MAX && dist != INT64_MAX &&
          *totrate != INT_MAX && *totdist != INT64_MAX) {
        *totrate += rate;
        *totdist += dist;
      }
      nonrd_use_partition(cpi, tile, mi + hbs * mis, tp,
                          mi_row + hbs, mi_col, subsize, output_enabled,
                          &rate, &dist, pc_tree->split[2]);
      if (rate != INT_MAX && dist != INT64_MAX &&
          *totrate != INT_MAX && *totdist != INT64_MAX) {
        *totrate += rate;
        *totdist += dist;
      }
      nonrd_use_partition(cpi, tile, mi + hbs * mis + hbs, tp,
                          mi_row + hbs, mi_col + hbs, subsize, output_enabled,
                          &rate, &dist, pc_tree->split[3]);
      if (rate != INT_MAX && dist != INT64_MAX &&
          *totrate != INT_MAX && *totdist != INT64_MAX) {
        *totrate += rate;
        *totdist += dist;
      }
      break;
    default:
      assert("Invalid partition type.");
      break;
  }

  if (bsize == BLOCK_64X64 && output_enabled) {
    if (cpi->oxcf.aq_mode == CYCLIC_REFRESH_AQ)
      vp9_cyclic_refresh_set_rate_and_dist_sb(cpi->cyclic_refresh,
                                              *totrate, *totdist);
    encode_sb_rt(cpi, tile, tp, mi_row, mi_col, 1, bsize, pc_tree);
  }
}

static void encode_nonrd_sb_row(VP9_COMP *cpi, const TileInfo *const tile,
                                int mi_row, TOKENEXTRA **tp) {
  SPEED_FEATURES *const sf = &cpi->sf;
  VP9_COMMON *const cm = &cpi->common;
  MACROBLOCK *const x = &cpi->mb;
  MACROBLOCKD *const xd = &x->e_mbd;
  int mi_col;

  // Initialize the left context for the new SB row
  vpx_memset(&xd->left_context, 0, sizeof(xd->left_context));
  vpx_memset(xd->left_seg_context, 0, sizeof(xd->left_seg_context));

  // Code each SB in the row
  for (mi_col = tile->mi_col_start; mi_col < tile->mi_col_end;
       mi_col += MI_BLOCK_SIZE) {
    int dummy_rate = 0;
    int64_t dummy_dist = 0;
    const int idx_str = cm->mi_stride * mi_row + mi_col;
    MODE_INFO **mi = cm->mi_grid_visible + idx_str;
    MODE_INFO **prev_mi = cm->prev_mi_grid_visible + idx_str;
    BLOCK_SIZE bsize;

    x->in_static_area = 0;
    x->source_variance = UINT_MAX;
    vp9_zero(x->pred_mv);

    // Set the partition type of the 64X64 block
    switch (sf->partition_search_type) {
      case VAR_BASED_PARTITION:
        choose_partitioning(cpi, tile, mi_row, mi_col);
        nonrd_use_partition(cpi, tile, mi, tp, mi_row, mi_col, BLOCK_64X64,
                            1, &dummy_rate, &dummy_dist, cpi->pc_root);
        break;
      case SOURCE_VAR_BASED_PARTITION:
        set_source_var_based_partition(cpi, tile, mi, mi_row, mi_col);
        nonrd_use_partition(cpi, tile, mi, tp, mi_row, mi_col, BLOCK_64X64,
                            1, &dummy_rate, &dummy_dist, cpi->pc_root);
        break;
      case VAR_BASED_FIXED_PARTITION:
      case FIXED_PARTITION:
        bsize = sf->partition_search_type == FIXED_PARTITION ?
                sf->always_this_block_size :
                get_nonrd_var_based_fixed_partition(cpi, mi_row, mi_col);
        set_fixed_partitioning(cpi, tile, mi, mi_row, mi_col, bsize);
        nonrd_use_partition(cpi, tile, mi, tp, mi_row, mi_col, BLOCK_64X64,
                            1, &dummy_rate, &dummy_dist, cpi->pc_root);
        break;
      case REFERENCE_PARTITION:
        if (sf->partition_check ||
            !(x->in_static_area = is_background(cpi, tile, mi_row, mi_col))) {
          set_modeinfo_offsets(cm, xd, mi_row, mi_col);
          auto_partition_range(cpi, tile, mi_row, mi_col,
                               &sf->min_partition_size,
                               &sf->max_partition_size);
          nonrd_pick_partition(cpi, tile, tp, mi_row, mi_col, BLOCK_64X64,
                               &dummy_rate, &dummy_dist, 1, INT64_MAX,
                               cpi->pc_root);
        } else {
          copy_partitioning(cm, mi, prev_mi);
          nonrd_use_partition(cpi, tile, mi, tp, mi_row, mi_col,
                              BLOCK_64X64, 1, &dummy_rate, &dummy_dist,
                              cpi->pc_root);
        }
        break;
      default:
        assert(0);
        break;
    }
  }
}
// end RTC play code

static int set_var_thresh_from_histogram(VP9_COMP *cpi) {
  const SPEED_FEATURES *const sf = &cpi->sf;
  const VP9_COMMON *const cm = &cpi->common;

  const uint8_t *src = cpi->Source->y_buffer;
  const uint8_t *last_src = cpi->Last_Source->y_buffer;
  const int src_stride = cpi->Source->y_stride;
  const int last_stride = cpi->Last_Source->y_stride;

  // Pick cutoff threshold
  const int cutoff = (MIN(cm->width, cm->height) >= 720) ?
      (cm->MBs * VAR_HIST_LARGE_CUT_OFF / 100) :
      (cm->MBs * VAR_HIST_SMALL_CUT_OFF / 100);
  DECLARE_ALIGNED_ARRAY(16, int, hist, VAR_HIST_BINS);
  diff *var16 = cpi->source_diff_var;

  int sum = 0;
  int i, j;

  vpx_memset(hist, 0, VAR_HIST_BINS * sizeof(hist[0]));

  for (i = 0; i < cm->mb_rows; i++) {
    for (j = 0; j < cm->mb_cols; j++) {
      vp9_get16x16var(src, src_stride, last_src, last_stride,
                      &var16->sse, &var16->sum);

      var16->var = var16->sse -
          (((uint32_t)var16->sum * var16->sum) >> 8);

      if (var16->var >= VAR_HIST_MAX_BG_VAR)
        hist[VAR_HIST_BINS - 1]++;
      else
        hist[var16->var / VAR_HIST_FACTOR]++;

      src += 16;
      last_src += 16;
      var16++;
    }

    src = src - cm->mb_cols * 16 + 16 * src_stride;
    last_src = last_src - cm->mb_cols * 16 + 16 * last_stride;
  }

  cpi->source_var_thresh = 0;

  if (hist[VAR_HIST_BINS - 1] < cutoff) {
    for (i = 0; i < VAR_HIST_BINS - 1; i++) {
      sum += hist[i];

      if (sum > cutoff) {
        cpi->source_var_thresh = (i + 1) * VAR_HIST_FACTOR;
        return 0;
      }
    }
  }

  return sf->search_type_check_frequency;
}

static void source_var_based_partition_search_method(VP9_COMP *cpi) {
  VP9_COMMON *const cm = &cpi->common;
  SPEED_FEATURES *const sf = &cpi->sf;

  if (cm->frame_type == KEY_FRAME) {
    // For key frame, use SEARCH_PARTITION.
    sf->partition_search_type = SEARCH_PARTITION;
  } else if (cm->intra_only) {
    sf->partition_search_type = FIXED_PARTITION;
  } else {
    if (cm->last_width != cm->width || cm->last_height != cm->height) {
      if (cpi->source_diff_var)
        vpx_free(cpi->source_diff_var);

        CHECK_MEM_ERROR(cm, cpi->source_diff_var,
                        vpx_calloc(cm->MBs, sizeof(diff)));
      }

    if (!cpi->frames_till_next_var_check)
      cpi->frames_till_next_var_check = set_var_thresh_from_histogram(cpi);

    if (cpi->frames_till_next_var_check > 0) {
      sf->partition_search_type = FIXED_PARTITION;
      cpi->frames_till_next_var_check--;
    }
  }
}

static int get_skip_encode_frame(const VP9_COMMON *cm) {
  unsigned int intra_count = 0, inter_count = 0;
  int j;

  for (j = 0; j < INTRA_INTER_CONTEXTS; ++j) {
    intra_count += cm->counts.intra_inter[j][0];
    inter_count += cm->counts.intra_inter[j][1];
  }

  return (intra_count << 2) < inter_count &&
         cm->frame_type != KEY_FRAME &&
         cm->show_frame;
}

static void encode_tiles(VP9_COMP *cpi) {
  const VP9_COMMON *const cm = &cpi->common;
  const int tile_cols = 1 << cm->log2_tile_cols;
  const int tile_rows = 1 << cm->log2_tile_rows;
  int tile_col, tile_row;
  TOKENEXTRA *tok = cpi->tok;

  for (tile_row = 0; tile_row < tile_rows; ++tile_row) {
    for (tile_col = 0; tile_col < tile_cols; ++tile_col) {
      TileInfo tile;
      TOKENEXTRA *old_tok = tok;
      int mi_row;

      vp9_tile_init(&tile, cm, tile_row, tile_col);
      for (mi_row = tile.mi_row_start; mi_row < tile.mi_row_end;
           mi_row += MI_BLOCK_SIZE) {
        if (cpi->sf.use_nonrd_pick_mode && !frame_is_intra_only(cm))
          encode_nonrd_sb_row(cpi, &tile, mi_row, &tok);
        else
          encode_rd_sb_row(cpi, &tile, mi_row, &tok);
      }
      cpi->tok_count[tile_row][tile_col] = (unsigned int)(tok - old_tok);
      assert(tok - cpi->tok <= get_token_alloc(cm->mb_rows, cm->mb_cols));
    }
  }
}

#if CONFIG_FP_MB_STATS
static int input_fpmb_stats(FIRSTPASS_MB_STATS *firstpass_mb_stats,
                            VP9_COMMON *cm, uint8_t **this_frame_mb_stats) {
  uint8_t *mb_stats_in = firstpass_mb_stats->mb_stats_start +
      cm->current_video_frame * cm->MBs * sizeof(uint8_t);

  if (mb_stats_in > firstpass_mb_stats->mb_stats_end)
    return EOF;

  *this_frame_mb_stats = mb_stats_in;

  return 1;
}
#endif

static void encode_frame_internal(VP9_COMP *cpi) {
  SPEED_FEATURES *const sf = &cpi->sf;
  RD_OPT *const rd_opt = &cpi->rd;
  MACROBLOCK *const x = &cpi->mb;
  VP9_COMMON *const cm = &cpi->common;
  MACROBLOCKD *const xd = &x->e_mbd;

  xd->mi = cm->mi_grid_visible;
  xd->mi[0] = cm->mi;

  vp9_zero(cm->counts);
  vp9_zero(cpi->coef_counts);
  vp9_zero(rd_opt->comp_pred_diff);
  vp9_zero(rd_opt->filter_diff);
  vp9_zero(rd_opt->tx_select_diff);
  vp9_zero(rd_opt->tx_select_threshes);

  xd->lossless = cm->base_qindex == 0 &&
                 cm->y_dc_delta_q == 0 &&
                 cm->uv_dc_delta_q == 0 &&
                 cm->uv_ac_delta_q == 0;

  cm->tx_mode = select_tx_mode(cpi);

  x->fwd_txm4x4 = xd->lossless ? vp9_fwht4x4 : vp9_fdct4x4;
  x->itxm_add = xd->lossless ? vp9_iwht4x4_add : vp9_idct4x4_add;

  if (xd->lossless) {
    x->optimize = 0;
    cm->lf.filter_level = 0;
    cpi->zbin_mode_boost_enabled = 0;
  }

  vp9_frame_init_quantizer(cpi);

  vp9_initialize_rd_consts(cpi);
  vp9_initialize_me_consts(cpi, cm->base_qindex);
  init_encode_frame_mb_context(cpi);
  set_prev_mi(cm);

  x->quant_fp = cpi->sf.use_quant_fp;
  vp9_zero(x->skip_txfm);
  if (sf->use_nonrd_pick_mode) {
    // Initialize internal buffer pointers for rtc coding, where non-RD
    // mode decision is used and hence no buffer pointer swap needed.
    int i;
    struct macroblock_plane *const p = x->plane;
    struct macroblockd_plane *const pd = xd->plane;
    PICK_MODE_CONTEXT *ctx = &cpi->pc_root->none;

    for (i = 0; i < MAX_MB_PLANE; ++i) {
      p[i].coeff = ctx->coeff_pbuf[i][0];
      p[i].qcoeff = ctx->qcoeff_pbuf[i][0];
      pd[i].dqcoeff = ctx->dqcoeff_pbuf[i][0];
      p[i].eobs = ctx->eobs_pbuf[i][0];
    }
    vp9_zero(x->zcoeff_blk);

    if (sf->partition_search_type == SOURCE_VAR_BASED_PARTITION)
      source_var_based_partition_search_method(cpi);
  }

  {
    struct vpx_usec_timer emr_timer;
    vpx_usec_timer_start(&emr_timer);

#if CONFIG_FP_MB_STATS
  if (cpi->use_fp_mb_stats) {
    input_fpmb_stats(&cpi->twopass.firstpass_mb_stats, cm,
                     &cpi->twopass.this_frame_mb_stats);
  }
#endif

    encode_tiles(cpi);

    vpx_usec_timer_mark(&emr_timer);
    cpi->time_encode_sb_row += vpx_usec_timer_elapsed(&emr_timer);
  }

  sf->skip_encode_frame = sf->skip_encode_sb ? get_skip_encode_frame(cm) : 0;

#if 0
  // Keep record of the total distortion this time around for future use
  cpi->last_frame_distortion = cpi->frame_distortion;
#endif
}

static INTERP_FILTER get_interp_filter(
    const int64_t threshes[SWITCHABLE_FILTER_CONTEXTS], int is_alt_ref) {
  if (!is_alt_ref &&
      threshes[EIGHTTAP_SMOOTH] > threshes[EIGHTTAP] &&
      threshes[EIGHTTAP_SMOOTH] > threshes[EIGHTTAP_SHARP] &&
      threshes[EIGHTTAP_SMOOTH] > threshes[SWITCHABLE - 1]) {
    return EIGHTTAP_SMOOTH;
  } else if (threshes[EIGHTTAP_SHARP] > threshes[EIGHTTAP] &&
             threshes[EIGHTTAP_SHARP] > threshes[SWITCHABLE - 1]) {
    return EIGHTTAP_SHARP;
  } else if (threshes[EIGHTTAP] > threshes[SWITCHABLE - 1]) {
    return EIGHTTAP;
  } else {
    return SWITCHABLE;
  }
}

void vp9_encode_frame(VP9_COMP *cpi) {
  VP9_COMMON *const cm = &cpi->common;
  RD_OPT *const rd_opt = &cpi->rd;

  // In the longer term the encoder should be generalized to match the
  // decoder such that we allow compound where one of the 3 buffers has a
  // different sign bias and that buffer is then the fixed ref. However, this
  // requires further work in the rd loop. For now the only supported encoder
  // side behavior is where the ALT ref buffer has opposite sign bias to
  // the other two.
  if (!frame_is_intra_only(cm)) {
    if ((cm->ref_frame_sign_bias[ALTREF_FRAME] ==
             cm->ref_frame_sign_bias[GOLDEN_FRAME]) ||
        (cm->ref_frame_sign_bias[ALTREF_FRAME] ==
             cm->ref_frame_sign_bias[LAST_FRAME])) {
      cm->allow_comp_inter_inter = 0;
    } else {
      cm->allow_comp_inter_inter = 1;
      cm->comp_fixed_ref = ALTREF_FRAME;
      cm->comp_var_ref[0] = LAST_FRAME;
      cm->comp_var_ref[1] = GOLDEN_FRAME;
    }
  }

  if (cpi->sf.frame_parameter_update) {
    int i;

    // This code does a single RD pass over the whole frame assuming
    // either compound, single or hybrid prediction as per whatever has
    // worked best for that type of frame in the past.
    // It also predicts whether another coding mode would have worked
    // better that this coding mode. If that is the case, it remembers
    // that for subsequent frames.
    // It does the same analysis for transform size selection also.
    const MV_REFERENCE_FRAME frame_type = get_frame_type(cpi);
    int64_t *const mode_thrs = rd_opt->prediction_type_threshes[frame_type];
    int64_t *const filter_thrs = rd_opt->filter_threshes[frame_type];
    int *const tx_thrs = rd_opt->tx_select_threshes[frame_type];
    const int is_alt_ref = frame_type == ALTREF_FRAME;

    /* prediction (compound, single or hybrid) mode selection */
    if (is_alt_ref || !cm->allow_comp_inter_inter)
      cm->reference_mode = SINGLE_REFERENCE;
    else if (mode_thrs[COMPOUND_REFERENCE] > mode_thrs[SINGLE_REFERENCE] &&
             mode_thrs[COMPOUND_REFERENCE] >
                 mode_thrs[REFERENCE_MODE_SELECT] &&
             check_dual_ref_flags(cpi) &&
             cpi->static_mb_pct == 100)
      cm->reference_mode = COMPOUND_REFERENCE;
    else if (mode_thrs[SINGLE_REFERENCE] > mode_thrs[REFERENCE_MODE_SELECT])
      cm->reference_mode = SINGLE_REFERENCE;
    else
      cm->reference_mode = REFERENCE_MODE_SELECT;

    if (cm->interp_filter == SWITCHABLE)
      cm->interp_filter = get_interp_filter(filter_thrs, is_alt_ref);

    encode_frame_internal(cpi);

    for (i = 0; i < REFERENCE_MODES; ++i)
      mode_thrs[i] = (mode_thrs[i] + rd_opt->comp_pred_diff[i] / cm->MBs) / 2;

    for (i = 0; i < SWITCHABLE_FILTER_CONTEXTS; ++i)
      filter_thrs[i] = (filter_thrs[i] + rd_opt->filter_diff[i] / cm->MBs) / 2;

    for (i = 0; i < TX_MODES; ++i) {
      int64_t pd = rd_opt->tx_select_diff[i];
      if (i == TX_MODE_SELECT)
        pd -= RDCOST(cpi->mb.rdmult, cpi->mb.rddiv, 2048 * (TX_SIZES - 1), 0);
      tx_thrs[i] = (tx_thrs[i] + (int)(pd / cm->MBs)) / 2;
    }

    if (cm->reference_mode == REFERENCE_MODE_SELECT) {
      int single_count_zero = 0;
      int comp_count_zero = 0;

      for (i = 0; i < COMP_INTER_CONTEXTS; i++) {
        single_count_zero += cm->counts.comp_inter[i][0];
        comp_count_zero += cm->counts.comp_inter[i][1];
      }

      if (comp_count_zero == 0) {
        cm->reference_mode = SINGLE_REFERENCE;
        vp9_zero(cm->counts.comp_inter);
      } else if (single_count_zero == 0) {
        cm->reference_mode = COMPOUND_REFERENCE;
        vp9_zero(cm->counts.comp_inter);
      }
    }

    if (cm->tx_mode == TX_MODE_SELECT) {
      int count4x4 = 0;
      int count8x8_lp = 0, count8x8_8x8p = 0;
      int count16x16_16x16p = 0, count16x16_lp = 0;
      int count32x32 = 0;

      for (i = 0; i < TX_SIZE_CONTEXTS; ++i) {
        count4x4 += cm->counts.tx.p32x32[i][TX_4X4];
        count4x4 += cm->counts.tx.p16x16[i][TX_4X4];
        count4x4 += cm->counts.tx.p8x8[i][TX_4X4];

        count8x8_lp += cm->counts.tx.p32x32[i][TX_8X8];
        count8x8_lp += cm->counts.tx.p16x16[i][TX_8X8];
        count8x8_8x8p += cm->counts.tx.p8x8[i][TX_8X8];

        count16x16_16x16p += cm->counts.tx.p16x16[i][TX_16X16];
        count16x16_lp += cm->counts.tx.p32x32[i][TX_16X16];
        count32x32 += cm->counts.tx.p32x32[i][TX_32X32];
      }

      if (count4x4 == 0 && count16x16_lp == 0 && count16x16_16x16p == 0 &&
          count32x32 == 0) {
        cm->tx_mode = ALLOW_8X8;
        reset_skip_tx_size(cm, TX_8X8);
      } else if (count8x8_8x8p == 0 && count16x16_16x16p == 0 &&
                 count8x8_lp == 0 && count16x16_lp == 0 && count32x32 == 0) {
        cm->tx_mode = ONLY_4X4;
        reset_skip_tx_size(cm, TX_4X4);
      } else if (count8x8_lp == 0 && count16x16_lp == 0 && count4x4 == 0) {
        cm->tx_mode = ALLOW_32X32;
      } else if (count32x32 == 0 && count8x8_lp == 0 && count4x4 == 0) {
        cm->tx_mode = ALLOW_16X16;
        reset_skip_tx_size(cm, TX_16X16);
      }
    }
  } else {
    cm->reference_mode = SINGLE_REFERENCE;
    encode_frame_internal(cpi);
  }
}

static void sum_intra_stats(FRAME_COUNTS *counts, const MODE_INFO *mi) {
  const PREDICTION_MODE y_mode = mi->mbmi.mode;
  const PREDICTION_MODE uv_mode = mi->mbmi.uv_mode;
  const BLOCK_SIZE bsize = mi->mbmi.sb_type;

  if (bsize < BLOCK_8X8) {
    int idx, idy;
    const int num_4x4_w = num_4x4_blocks_wide_lookup[bsize];
    const int num_4x4_h = num_4x4_blocks_high_lookup[bsize];
    for (idy = 0; idy < 2; idy += num_4x4_h)
      for (idx = 0; idx < 2; idx += num_4x4_w)
        ++counts->y_mode[0][mi->bmi[idy * 2 + idx].as_mode];
  } else {
    ++counts->y_mode[size_group_lookup[bsize]][y_mode];
  }

  ++counts->uv_mode[y_mode][uv_mode];
}

static int get_zbin_mode_boost(const MB_MODE_INFO *mbmi, int enabled) {
  if (enabled) {
    if (is_inter_block(mbmi)) {
      if (mbmi->mode == ZEROMV) {
        return mbmi->ref_frame[0] != LAST_FRAME ? GF_ZEROMV_ZBIN_BOOST
                                                : LF_ZEROMV_ZBIN_BOOST;
      } else {
        return mbmi->sb_type < BLOCK_8X8 ? SPLIT_MV_ZBIN_BOOST
                                         : MV_ZBIN_BOOST;
      }
    } else {
      return INTRA_ZBIN_BOOST;
    }
  } else {
    return 0;
  }
}

static void encode_superblock(VP9_COMP *cpi, TOKENEXTRA **t, int output_enabled,
                              int mi_row, int mi_col, BLOCK_SIZE bsize,
                              PICK_MODE_CONTEXT *ctx) {
  VP9_COMMON *const cm = &cpi->common;
  MACROBLOCK *const x = &cpi->mb;
  MACROBLOCKD *const xd = &x->e_mbd;
  MODE_INFO **mi_8x8 = xd->mi;
  MODE_INFO *mi = mi_8x8[0];
  MB_MODE_INFO *mbmi = &mi->mbmi;
  const int seg_skip = vp9_segfeature_active(&cm->seg, mbmi->segment_id,
                                             SEG_LVL_SKIP);
  const int mis = cm->mi_stride;
  const int mi_width = num_8x8_blocks_wide_lookup[bsize];
  const int mi_height = num_8x8_blocks_high_lookup[bsize];

  x->skip_recode = !x->select_tx_size && mbmi->sb_type >= BLOCK_8X8 &&
                   cpi->oxcf.aq_mode != COMPLEXITY_AQ &&
                   cpi->oxcf.aq_mode != CYCLIC_REFRESH_AQ &&
                   cpi->sf.allow_skip_recode;

  if (!x->skip_recode && !cpi->sf.use_nonrd_pick_mode)
    vpx_memset(x->skip_txfm, 0, sizeof(x->skip_txfm));

  x->skip_optimize = ctx->is_coded;
  ctx->is_coded = 1;
  x->use_lp32x32fdct = cpi->sf.use_lp32x32fdct;
  x->skip_encode = (!output_enabled && cpi->sf.skip_encode_frame &&
                    x->q_index < QIDX_SKIP_THRESH);

  if (x->skip_encode)
    return;

  set_ref_ptrs(cm, xd, mbmi->ref_frame[0], mbmi->ref_frame[1]);

  // Experimental code. Special case for gf and arf zeromv modes.
  // Increase zbin size to suppress noise
  cpi->zbin_mode_boost = get_zbin_mode_boost(mbmi,
                                             cpi->zbin_mode_boost_enabled);
  vp9_update_zbin_extra(cpi, x);

  if (!is_inter_block(mbmi)) {
    int plane;
    mbmi->skip = 1;
    for (plane = 0; plane < MAX_MB_PLANE; ++plane)
      vp9_encode_intra_block_plane(x, MAX(bsize, BLOCK_8X8), plane);
    if (output_enabled)
      sum_intra_stats(&cm->counts, mi);
    vp9_tokenize_sb(cpi, t, !output_enabled, MAX(bsize, BLOCK_8X8));
  } else {
    int ref;
    const int is_compound = has_second_ref(mbmi);
    for (ref = 0; ref < 1 + is_compound; ++ref) {
      YV12_BUFFER_CONFIG *cfg = get_ref_frame_buffer(cpi,
                                                     mbmi->ref_frame[ref]);
      vp9_setup_pre_planes(xd, ref, cfg, mi_row, mi_col,
                           &xd->block_refs[ref]->sf);
    }
    if (!cpi->sf.reuse_inter_pred_sby || seg_skip)
      vp9_build_inter_predictors_sby(xd, mi_row, mi_col, MAX(bsize, BLOCK_8X8));

    vp9_build_inter_predictors_sbuv(xd, mi_row, mi_col, MAX(bsize, BLOCK_8X8));

    if (!x->skip) {
      mbmi->skip = 1;
      vp9_encode_sb(x, MAX(bsize, BLOCK_8X8));
      vp9_tokenize_sb(cpi, t, !output_enabled, MAX(bsize, BLOCK_8X8));
    } else {
      mbmi->skip = 1;
      if (output_enabled && !seg_skip)
        cm->counts.skip[vp9_get_skip_context(xd)][1]++;
      reset_skip_context(xd, MAX(bsize, BLOCK_8X8));
    }
  }

  if (output_enabled) {
    if (cm->tx_mode == TX_MODE_SELECT &&
        mbmi->sb_type >= BLOCK_8X8  &&
        !(is_inter_block(mbmi) && (mbmi->skip || seg_skip))) {
      ++get_tx_counts(max_txsize_lookup[bsize], vp9_get_tx_size_context(xd),
                      &cm->counts.tx)[mbmi->tx_size];
    } else {
      int x, y;
      TX_SIZE tx_size;
      // The new intra coding scheme requires no change of transform size
      if (is_inter_block(&mi->mbmi)) {
        tx_size = MIN(tx_mode_to_biggest_tx_size[cm->tx_mode],
                      max_txsize_lookup[bsize]);
      } else {
        tx_size = (bsize >= BLOCK_8X8) ? mbmi->tx_size : TX_4X4;
      }

      for (y = 0; y < mi_height; y++)
        for (x = 0; x < mi_width; x++)
          if (mi_col + x < cm->mi_cols && mi_row + y < cm->mi_rows)
            mi_8x8[mis * y + x]->mbmi.tx_size = tx_size;
    }
  }
}
