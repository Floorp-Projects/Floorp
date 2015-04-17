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
#include "vp9/encoder/vp9_encodeframe.h"
#include "vp9/encoder/vp9_encodemb.h"
#include "vp9/encoder/vp9_encodemv.h"
#include "vp9/encoder/vp9_extend.h"
#include "vp9/encoder/vp9_onyx_int.h"
#include "vp9/encoder/vp9_pickmode.h"
#include "vp9/encoder/vp9_rdopt.h"
#include "vp9/encoder/vp9_segmentation.h"
#include "vp9/encoder/vp9_tokenize.h"
#include "vp9/encoder/vp9_vaq.h"

static INLINE uint8_t *get_sb_index(MACROBLOCK *x, BLOCK_SIZE subsize) {
  switch (subsize) {
    case BLOCK_64X64:
    case BLOCK_64X32:
    case BLOCK_32X64:
    case BLOCK_32X32:
      return &x->sb_index;
    case BLOCK_32X16:
    case BLOCK_16X32:
    case BLOCK_16X16:
      return &x->mb_index;
    case BLOCK_16X8:
    case BLOCK_8X16:
    case BLOCK_8X8:
      return &x->b_index;
    case BLOCK_8X4:
    case BLOCK_4X8:
    case BLOCK_4X4:
      return &x->ab_index;
    default:
      assert(0);
      return NULL;
  }
}

static void encode_superblock(VP9_COMP *cpi, TOKENEXTRA **t, int output_enabled,
                              int mi_row, int mi_col, BLOCK_SIZE bsize);

static void adjust_act_zbin(VP9_COMP *cpi, MACROBLOCK *x);

// activity_avg must be positive, or flat regions could get a zero weight
//  (infinite lambda), which confounds analysis.
// This also avoids the need for divide by zero checks in
//  vp9_activity_masking().
#define ACTIVITY_AVG_MIN (64)

// Motion vector component magnitude threshold for defining fast motion.
#define FAST_MOTION_MV_THRESH (24)

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

static unsigned int get_sby_perpixel_variance(VP9_COMP *cpi, MACROBLOCK *x,
                                              BLOCK_SIZE bs) {
  unsigned int var, sse;
  var = cpi->fn_ptr[bs].vf(x->plane[0].src.buf, x->plane[0].src.stride,
                           VP9_VAR_OFFS, 0, &sse);
  return ROUND_POWER_OF_TWO(var, num_pels_log2_lookup[bs]);
}

// Original activity measure from Tim T's code.
static unsigned int tt_activity_measure(MACROBLOCK *x) {
  unsigned int sse;
  /* TODO: This could also be done over smaller areas (8x8), but that would
   *  require extensive changes elsewhere, as lambda is assumed to be fixed
   *  over an entire MB in most of the code.
   * Another option is to compute four 8x8 variances, and pick a single
   *  lambda using a non-linear combination (e.g., the smallest, or second
   *  smallest, etc.).
   */
  unsigned int act = vp9_variance16x16(x->plane[0].src.buf,
                                       x->plane[0].src.stride,
                                       VP9_VAR_OFFS, 0, &sse) << 4;
  // If the region is flat, lower the activity some more.
  if (act < (8 << 12))
    act = MIN(act, 5 << 12);

  return act;
}

// Stub for alternative experimental activity measures.
static unsigned int alt_activity_measure(MACROBLOCK *x, int use_dc_pred) {
  return vp9_encode_intra(x, use_dc_pred);
}

// Measure the activity of the current macroblock
// What we measure here is TBD so abstracted to this function
#define ALT_ACT_MEASURE 1
static unsigned int mb_activity_measure(MACROBLOCK *x, int mb_row, int mb_col) {
  unsigned int mb_activity;

  if (ALT_ACT_MEASURE) {
    const int use_dc_pred = (mb_col || mb_row) && (!mb_col || !mb_row);

    // Or use and alternative.
    mb_activity = alt_activity_measure(x, use_dc_pred);
  } else {
    // Original activity measure from Tim T's code.
    mb_activity = tt_activity_measure(x);
  }

  return MAX(mb_activity, ACTIVITY_AVG_MIN);
}

// Calculate an "average" mb activity value for the frame
#define ACT_MEDIAN 0
static void calc_av_activity(VP9_COMP *cpi, int64_t activity_sum) {
#if ACT_MEDIAN
  // Find median: Simple n^2 algorithm for experimentation
  {
    unsigned int median;
    unsigned int i, j;
    unsigned int *sortlist;
    unsigned int tmp;

    // Create a list to sort to
    CHECK_MEM_ERROR(&cpi->common, sortlist, vpx_calloc(sizeof(unsigned int),
                    cpi->common.MBs));

    // Copy map to sort list
    vpx_memcpy(sortlist, cpi->mb_activity_map,
        sizeof(unsigned int) * cpi->common.MBs);

    // Ripple each value down to its correct position
    for (i = 1; i < cpi->common.MBs; i ++) {
      for (j = i; j > 0; j --) {
        if (sortlist[j] < sortlist[j - 1]) {
          // Swap values
          tmp = sortlist[j - 1];
          sortlist[j - 1] = sortlist[j];
          sortlist[j] = tmp;
        } else {
          break;
        }
      }
    }

    // Even number MBs so estimate median as mean of two either side.
    median = (1 + sortlist[cpi->common.MBs >> 1] +
        sortlist[(cpi->common.MBs >> 1) + 1]) >> 1;

    cpi->activity_avg = median;

    vpx_free(sortlist);
  }
#else
  // Simple mean for now
  cpi->activity_avg = (unsigned int) (activity_sum / cpi->common.MBs);
#endif  // ACT_MEDIAN

  if (cpi->activity_avg < ACTIVITY_AVG_MIN)
    cpi->activity_avg = ACTIVITY_AVG_MIN;

  // Experimental code: return fixed value normalized for several clips
  if (ALT_ACT_MEASURE)
    cpi->activity_avg = 100000;
}

#define USE_ACT_INDEX   0
#define OUTPUT_NORM_ACT_STATS   0

#if USE_ACT_INDEX
// Calculate an activity index for each mb
static void calc_activity_index(VP9_COMP *cpi, MACROBLOCK *x) {
  VP9_COMMON *const cm = &cpi->common;
  int mb_row, mb_col;

  int64_t act;
  int64_t a;
  int64_t b;

#if OUTPUT_NORM_ACT_STATS
  FILE *f = fopen("norm_act.stt", "a");
  fprintf(f, "\n%12d\n", cpi->activity_avg);
#endif

  // Reset pointers to start of activity map
  x->mb_activity_ptr = cpi->mb_activity_map;

  // Calculate normalized mb activity number.
  for (mb_row = 0; mb_row < cm->mb_rows; mb_row++) {
    // for each macroblock col in image
    for (mb_col = 0; mb_col < cm->mb_cols; mb_col++) {
      // Read activity from the map
      act = *(x->mb_activity_ptr);

      // Calculate a normalized activity number
      a = act + 4 * cpi->activity_avg;
      b = 4 * act + cpi->activity_avg;

      if (b >= a)
      *(x->activity_ptr) = (int)((b + (a >> 1)) / a) - 1;
      else
      *(x->activity_ptr) = 1 - (int)((a + (b >> 1)) / b);

#if OUTPUT_NORM_ACT_STATS
      fprintf(f, " %6d", *(x->mb_activity_ptr));
#endif
      // Increment activity map pointers
      x->mb_activity_ptr++;
    }

#if OUTPUT_NORM_ACT_STATS
    fprintf(f, "\n");
#endif
  }

#if OUTPUT_NORM_ACT_STATS
  fclose(f);
#endif
}
#endif  // USE_ACT_INDEX

// Loop through all MBs. Note activity of each, average activity and
// calculate a normalized activity for each
static void build_activity_map(VP9_COMP *cpi) {
  MACROBLOCK *const x = &cpi->mb;
  MACROBLOCKD *xd = &x->e_mbd;
  VP9_COMMON *const cm = &cpi->common;

#if ALT_ACT_MEASURE
  YV12_BUFFER_CONFIG *new_yv12 = get_frame_new_buffer(cm);
  int recon_yoffset;
  int recon_y_stride = new_yv12->y_stride;
#endif

  int mb_row, mb_col;
  unsigned int mb_activity;
  int64_t activity_sum = 0;

  x->mb_activity_ptr = cpi->mb_activity_map;

  // for each macroblock row in image
  for (mb_row = 0; mb_row < cm->mb_rows; mb_row++) {
#if ALT_ACT_MEASURE
    // reset above block coeffs
    xd->up_available = (mb_row != 0);
    recon_yoffset = (mb_row * recon_y_stride * 16);
#endif
    // for each macroblock col in image
    for (mb_col = 0; mb_col < cm->mb_cols; mb_col++) {
#if ALT_ACT_MEASURE
      xd->plane[0].dst.buf = new_yv12->y_buffer + recon_yoffset;
      xd->left_available = (mb_col != 0);
      recon_yoffset += 16;
#endif

      // measure activity
      mb_activity = mb_activity_measure(x, mb_row, mb_col);

      // Keep frame sum
      activity_sum += mb_activity;

      // Store MB level activity details.
      *x->mb_activity_ptr = mb_activity;

      // Increment activity map pointer
      x->mb_activity_ptr++;

      // adjust to the next column of source macroblocks
      x->plane[0].src.buf += 16;
    }

    // adjust to the next row of mbs
    x->plane[0].src.buf += 16 * x->plane[0].src.stride - 16 * cm->mb_cols;
  }

  // Calculate an "average" MB activity
  calc_av_activity(cpi, activity_sum);

#if USE_ACT_INDEX
  // Calculate an activity index number of each mb
  calc_activity_index(cpi, x);
#endif
}

// Macroblock activity masking
static void activity_masking(VP9_COMP *cpi, MACROBLOCK *x) {
#if USE_ACT_INDEX
  x->rdmult += *(x->mb_activity_ptr) * (x->rdmult >> 2);
  x->errorperbit = x->rdmult * 100 / (110 * x->rddiv);
  x->errorperbit += (x->errorperbit == 0);
#else
  const int64_t act = *(x->mb_activity_ptr);

  // Apply the masking to the RD multiplier.
  const int64_t a = act + (2 * cpi->activity_avg);
  const int64_t b = (2 * act) + cpi->activity_avg;

  x->rdmult = (unsigned int) (((int64_t) x->rdmult * b + (a >> 1)) / a);
  x->errorperbit = x->rdmult * 100 / (110 * x->rddiv);
  x->errorperbit += (x->errorperbit == 0);
#endif

  // Activity based Zbin adjustment
  adjust_act_zbin(cpi, x);
}

// Select a segment for the current SB64
static void select_in_frame_q_segment(VP9_COMP *cpi,
                                      int mi_row, int mi_col,
                                      int output_enabled, int projected_rate) {
  VP9_COMMON *const cm = &cpi->common;

  const int mi_offset = mi_row * cm->mi_cols + mi_col;
  const int bw = num_8x8_blocks_wide_lookup[BLOCK_64X64];
  const int bh = num_8x8_blocks_high_lookup[BLOCK_64X64];
  const int xmis = MIN(cm->mi_cols - mi_col, bw);
  const int ymis = MIN(cm->mi_rows - mi_row, bh);
  int complexity_metric = 64;
  int x, y;

  unsigned char segment;

  if (!output_enabled) {
    segment = 0;
  } else {
    // Rate depends on fraction of a SB64 in frame (xmis * ymis / bw * bh).
    // It is converted to bits * 256 units
    const int target_rate = (cpi->rc.sb64_target_rate * xmis * ymis * 256) /
                            (bw * bh);

    if (projected_rate < (target_rate / 4)) {
      segment = 1;
    } else {
      segment = 0;
    }

    if (target_rate > 0) {
      complexity_metric =
        clamp((int)((projected_rate * 64) / target_rate), 16, 255);
    }
  }

  // Fill in the entires in the segment map corresponding to this SB64
  for (y = 0; y < ymis; y++) {
    for (x = 0; x < xmis; x++) {
      cpi->segmentation_map[mi_offset + y * cm->mi_cols + x] = segment;
      cpi->complexity_map[mi_offset + y * cm->mi_cols + x] =
        (unsigned char)complexity_metric;
    }
  }
}

static void update_state(VP9_COMP *cpi, PICK_MODE_CONTEXT *ctx,
                         BLOCK_SIZE bsize, int output_enabled) {
  int i, x_idx, y;
  VP9_COMMON *const cm = &cpi->common;
  MACROBLOCK *const x = &cpi->mb;
  MACROBLOCKD *const xd = &x->e_mbd;
  struct macroblock_plane *const p = x->plane;
  struct macroblockd_plane *const pd = xd->plane;
  MODE_INFO *mi = &ctx->mic;
  MB_MODE_INFO *const mbmi = &xd->mi_8x8[0]->mbmi;
  MODE_INFO *mi_addr = xd->mi_8x8[0];

  const int mb_mode_index = ctx->best_mode_index;
  const int mis = cm->mode_info_stride;
  const int mi_width = num_8x8_blocks_wide_lookup[bsize];
  const int mi_height = num_8x8_blocks_high_lookup[bsize];
  int max_plane;

  assert(mi->mbmi.mode < MB_MODE_COUNT);
  assert(mi->mbmi.ref_frame[0] < MAX_REF_FRAMES);
  assert(mi->mbmi.ref_frame[1] < MAX_REF_FRAMES);
  assert(mi->mbmi.sb_type == bsize);

  // For in frame adaptive Q copy over the chosen segment id into the
  // mode innfo context for the chosen mode / partition.
  if ((cpi->oxcf.aq_mode == COMPLEXITY_AQ) && output_enabled)
    mi->mbmi.segment_id = xd->mi_8x8[0]->mbmi.segment_id;

  *mi_addr = *mi;

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
        xd->mi_8x8[x_idx + y * mis] = mi_addr;
      }

    if ((cpi->oxcf.aq_mode == VARIANCE_AQ) ||
        (cpi->oxcf.aq_mode == COMPLEXITY_AQ)) {
    vp9_mb_init_quantizer(cpi, x);
  }

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
      cpi->rd_tx_select_diff[i] += ctx->tx_rd_diff[i];
  }

  if (frame_is_intra_only(cm)) {
#if CONFIG_INTERNAL_STATS
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
    cpi->mode_chosen_counts[kf_mode_index[mbmi->mode]]++;
#endif
  } else {
    // Note how often each mode chosen as best
    cpi->mode_chosen_counts[mb_mode_index]++;

    if (is_inter_block(mbmi)) {
      if (mbmi->sb_type < BLOCK_8X8 || mbmi->mode == NEWMV) {
        int_mv best_mv[2];
        for (i = 0; i < 1 + has_second_ref(mbmi); ++i)
          best_mv[i].as_int = mbmi->ref_mvs[mbmi->ref_frame[i]][0].as_int;
        vp9_update_mv_count(cpi, x, best_mv);
      }

      if (cm->interp_filter == SWITCHABLE) {
        const int ctx = vp9_get_pred_context_switchable_interp(xd);
        ++cm->counts.switchable_interp[ctx][mbmi->interp_filter];
      }
    }

    cpi->rd_comp_pred_diff[SINGLE_REFERENCE] += ctx->single_pred_diff;
    cpi->rd_comp_pred_diff[COMPOUND_REFERENCE] += ctx->comp_pred_diff;
    cpi->rd_comp_pred_diff[REFERENCE_MODE_SELECT] += ctx->hybrid_pred_diff;

    for (i = 0; i < SWITCHABLE_FILTER_CONTEXTS; ++i)
      cpi->rd_filter_diff[i] += ctx->best_filter_diff[i];
  }
}

void vp9_setup_src_planes(MACROBLOCK *x, const YV12_BUFFER_CONFIG *src,
                          int mi_row, int mi_col) {
  uint8_t *const buffers[4] = {src->y_buffer, src->u_buffer, src->v_buffer,
                               src->alpha_buffer};
  const int strides[4] = {src->y_stride, src->uv_stride, src->uv_stride,
                          src->alpha_stride};
  int i;

  // Set current frame pointer.
  x->e_mbd.cur_buf = src;

  for (i = 0; i < MAX_MB_PLANE; i++)
    setup_pred_plane(&x->plane[i].src, buffers[i], strides[i], mi_row, mi_col,
                     NULL, x->e_mbd.plane[i].subsampling_x,
                     x->e_mbd.plane[i].subsampling_y);
}

static void set_offsets(VP9_COMP *cpi, const TileInfo *const tile,
                        int mi_row, int mi_col, BLOCK_SIZE bsize) {
  MACROBLOCK *const x = &cpi->mb;
  VP9_COMMON *const cm = &cpi->common;
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *mbmi;
  const int idx_str = xd->mode_info_stride * mi_row + mi_col;
  const int mi_width = num_8x8_blocks_wide_lookup[bsize];
  const int mi_height = num_8x8_blocks_high_lookup[bsize];
  const int mb_row = mi_row >> 1;
  const int mb_col = mi_col >> 1;
  const int idx_map = mb_row * cm->mb_cols + mb_col;
  const struct segmentation *const seg = &cm->seg;

  set_skip_context(xd, cpi->above_context, cpi->left_context, mi_row, mi_col);

  // Activity map pointer
  x->mb_activity_ptr = &cpi->mb_activity_map[idx_map];
  x->active_ptr = cpi->active_map + idx_map;

  xd->mi_8x8 = cm->mi_grid_visible + idx_str;
  xd->prev_mi_8x8 = cm->prev_mi_grid_visible + idx_str;

  // Special case: if prev_mi is NULL, the previous mode info context
  // cannot be used.
  xd->last_mi = cm->prev_mi ? xd->prev_mi_8x8[0] : NULL;

  xd->mi_8x8[0] = cm->mi + idx_str;

  mbmi = &xd->mi_8x8[0]->mbmi;

  // Set up destination pointers
  setup_dst_planes(xd, get_frame_new_buffer(cm), mi_row, mi_col);

  // Set up limit values for MV components
  // mv beyond the range do not produce new/different prediction block
  x->mv_row_min = -(((mi_row + mi_height) * MI_SIZE) + VP9_INTERP_EXTEND);
  x->mv_col_min = -(((mi_col + mi_width) * MI_SIZE) + VP9_INTERP_EXTEND);
  x->mv_row_max = (cm->mi_rows - mi_row) * MI_SIZE + VP9_INTERP_EXTEND;
  x->mv_col_max = (cm->mi_cols - mi_col) * MI_SIZE + VP9_INTERP_EXTEND;

  // Set up distance of MB to edge of frame in 1/8th pel units
  assert(!(mi_col & (mi_width - 1)) && !(mi_row & (mi_height - 1)));
  set_mi_row_col(xd, tile, mi_row, mi_height, mi_col, mi_width,
                 cm->mi_rows, cm->mi_cols);

  /* set up source buffers */
  vp9_setup_src_planes(x, cpi->Source, mi_row, mi_col);

  /* R/D setup */
  x->rddiv = cpi->RDDIV;
  x->rdmult = cpi->RDMULT;

  /* segment ID */
  if (seg->enabled) {
    if (cpi->oxcf.aq_mode != VARIANCE_AQ) {
      const uint8_t *const map = seg->update_map ? cpi->segmentation_map
                                                 : cm->last_frame_seg_map;
      mbmi->segment_id = vp9_get_segment_id(cm, map, bsize, mi_row, mi_col);
    }
    vp9_mb_init_quantizer(cpi, x);

    if (seg->enabled && cpi->seg0_cnt > 0 &&
        !vp9_segfeature_active(seg, 0, SEG_LVL_REF_FRAME) &&
        vp9_segfeature_active(seg, 1, SEG_LVL_REF_FRAME)) {
      cpi->seg0_progress = (cpi->seg0_idx << 16) / cpi->seg0_cnt;
    } else {
      const int y = mb_row & ~3;
      const int x = mb_col & ~3;
      const int p16 = ((mb_row & 1) << 1) + (mb_col & 1);
      const int p32 = ((mb_row & 2) << 2) + ((mb_col & 2) << 1);
      const int tile_progress = tile->mi_col_start * cm->mb_rows >> 1;
      const int mb_cols = (tile->mi_col_end - tile->mi_col_start) >> 1;

      cpi->seg0_progress = ((y * mb_cols + x * 4 + p32 + p16 + tile_progress)
          << 16) / cm->MBs;
    }

    x->encode_breakout = cpi->segment_encode_breakout[mbmi->segment_id];
  } else {
    mbmi->segment_id = 0;
    x->encode_breakout = cpi->encode_breakout;
  }
}

static void rd_pick_sb_modes(VP9_COMP *cpi, const TileInfo *const tile,
                             int mi_row, int mi_col,
                             int *totalrate, int64_t *totaldist,
                             BLOCK_SIZE bsize, PICK_MODE_CONTEXT *ctx,
                             int64_t best_rd) {
  VP9_COMMON *const cm = &cpi->common;
  MACROBLOCK *const x = &cpi->mb;
  MACROBLOCKD *const xd = &x->e_mbd;
  struct macroblock_plane *const p = x->plane;
  struct macroblockd_plane *const pd = xd->plane;
  int i;
  int orig_rdmult = x->rdmult;
  double rdmult_ratio;

  vp9_clear_system_state();  // __asm emms;
  rdmult_ratio = 1.0;  // avoid uninitialized warnings

  // Use the lower precision, but faster, 32x32 fdct for mode selection.
  x->use_lp32x32fdct = 1;

  if (bsize < BLOCK_8X8) {
    // When ab_index = 0 all sub-blocks are handled, so for ab_index != 0
    // there is nothing to be done.
    if (x->ab_index != 0) {
      *totalrate = 0;
      *totaldist = 0;
      return;
    }
  }

  set_offsets(cpi, tile, mi_row, mi_col, bsize);
  xd->mi_8x8[0]->mbmi.sb_type = bsize;

  for (i = 0; i < MAX_MB_PLANE; ++i) {
    p[i].coeff = ctx->coeff_pbuf[i][0];
    p[i].qcoeff = ctx->qcoeff_pbuf[i][0];
    pd[i].dqcoeff = ctx->dqcoeff_pbuf[i][0];
    p[i].eobs = ctx->eobs_pbuf[i][0];
  }
  ctx->is_coded = 0;
  x->skip_recode = 0;

  // Set to zero to make sure we do not use the previous encoded frame stats
  xd->mi_8x8[0]->mbmi.skip = 0;

  x->source_variance = get_sby_perpixel_variance(cpi, x, bsize);

  if (cpi->oxcf.aq_mode == VARIANCE_AQ) {
    const int energy = bsize <= BLOCK_16X16 ? x->mb_energy
                                            : vp9_block_energy(cpi, x, bsize);

    if (cm->frame_type == KEY_FRAME ||
        cpi->refresh_alt_ref_frame ||
        (cpi->refresh_golden_frame && !cpi->rc.is_src_frame_alt_ref)) {
      xd->mi_8x8[0]->mbmi.segment_id = vp9_vaq_segment_id(energy);
    } else {
      const uint8_t *const map = cm->seg.update_map ? cpi->segmentation_map
                                                    : cm->last_frame_seg_map;
      xd->mi_8x8[0]->mbmi.segment_id =
        vp9_get_segment_id(cm, map, bsize, mi_row, mi_col);
    }

    rdmult_ratio = vp9_vaq_rdmult_ratio(energy);
    vp9_mb_init_quantizer(cpi, x);
  }

  if (cpi->oxcf.tuning == VP8_TUNE_SSIM)
    activity_masking(cpi, x);

  if (cpi->oxcf.aq_mode == VARIANCE_AQ) {
    vp9_clear_system_state();  // __asm emms;
    x->rdmult = round(x->rdmult * rdmult_ratio);
  } else if (cpi->oxcf.aq_mode == COMPLEXITY_AQ) {
    const int mi_offset = mi_row * cm->mi_cols + mi_col;
    unsigned char complexity = cpi->complexity_map[mi_offset];
    const int is_edge = (mi_row <= 1) || (mi_row >= (cm->mi_rows - 2)) ||
                        (mi_col <= 1) || (mi_col >= (cm->mi_cols - 2));

    if (!is_edge && (complexity > 128)) {
      x->rdmult = x->rdmult  + ((x->rdmult * (complexity - 128)) / 256);
    }
  }

  // Find best coding mode & reconstruct the MB so it is available
  // as a predictor for MBs that follow in the SB
  if (frame_is_intra_only(cm)) {
    vp9_rd_pick_intra_mode_sb(cpi, x, totalrate, totaldist, bsize, ctx,
                              best_rd);
  } else {
    if (bsize >= BLOCK_8X8)
      vp9_rd_pick_inter_mode_sb(cpi, x, tile, mi_row, mi_col,
                                totalrate, totaldist, bsize, ctx, best_rd);
    else
      vp9_rd_pick_inter_mode_sub8x8(cpi, x, tile, mi_row, mi_col, totalrate,
                                    totaldist, bsize, ctx, best_rd);
  }

  if (cpi->oxcf.aq_mode == VARIANCE_AQ) {
    x->rdmult = orig_rdmult;
    if (*totalrate != INT_MAX) {
      vp9_clear_system_state();  // __asm emms;
      *totalrate = round(*totalrate * rdmult_ratio);
    }
  }
  else if (cpi->oxcf.aq_mode == COMPLEXITY_AQ) {
    x->rdmult = orig_rdmult;
  }
}

static void update_stats(VP9_COMP *cpi) {
  VP9_COMMON *const cm = &cpi->common;
  const MACROBLOCK *const x = &cpi->mb;
  const MACROBLOCKD *const xd = &x->e_mbd;
  const MODE_INFO *const mi = xd->mi_8x8[0];
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

static BLOCK_SIZE *get_sb_partitioning(MACROBLOCK *x, BLOCK_SIZE bsize) {
  switch (bsize) {
    case BLOCK_64X64:
      return &x->sb64_partitioning;
    case BLOCK_32X32:
      return &x->sb_partitioning[x->sb_index];
    case BLOCK_16X16:
      return &x->mb_partitioning[x->sb_index][x->mb_index];
    case BLOCK_8X8:
      return &x->b_partitioning[x->sb_index][x->mb_index][x->b_index];
    default:
      assert(0);
      return NULL;
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
        cpi->above_context[p] + ((mi_col * 2) >> xd->plane[p].subsampling_x),
        a + num_4x4_blocks_wide * p,
        (sizeof(ENTROPY_CONTEXT) * num_4x4_blocks_wide) >>
        xd->plane[p].subsampling_x);
    vpx_memcpy(
        cpi->left_context[p]
            + ((mi_row & MI_MASK) * 2 >> xd->plane[p].subsampling_y),
        l + num_4x4_blocks_high * p,
        (sizeof(ENTROPY_CONTEXT) * num_4x4_blocks_high) >>
        xd->plane[p].subsampling_y);
  }
  vpx_memcpy(cpi->above_seg_context + mi_col, sa,
             sizeof(*cpi->above_seg_context) * mi_width);
  vpx_memcpy(cpi->left_seg_context + (mi_row & MI_MASK), sl,
             sizeof(cpi->left_seg_context[0]) * mi_height);
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
        cpi->above_context[p] + (mi_col * 2 >> xd->plane[p].subsampling_x),
        (sizeof(ENTROPY_CONTEXT) * num_4x4_blocks_wide) >>
        xd->plane[p].subsampling_x);
    vpx_memcpy(
        l + num_4x4_blocks_high * p,
        cpi->left_context[p]
            + ((mi_row & MI_MASK) * 2 >> xd->plane[p].subsampling_y),
        (sizeof(ENTROPY_CONTEXT) * num_4x4_blocks_high) >>
        xd->plane[p].subsampling_y);
  }
  vpx_memcpy(sa, cpi->above_seg_context + mi_col,
             sizeof(*cpi->above_seg_context) * mi_width);
  vpx_memcpy(sl, cpi->left_seg_context + (mi_row & MI_MASK),
             sizeof(cpi->left_seg_context[0]) * mi_height);
}

static void encode_b(VP9_COMP *cpi, const TileInfo *const tile,
                     TOKENEXTRA **tp, int mi_row, int mi_col,
                     int output_enabled, BLOCK_SIZE bsize) {
  MACROBLOCK *const x = &cpi->mb;

  if (bsize < BLOCK_8X8) {
    // When ab_index = 0 all sub-blocks are handled, so for ab_index != 0
    // there is nothing to be done.
    if (x->ab_index > 0)
      return;
  }
  set_offsets(cpi, tile, mi_row, mi_col, bsize);
  update_state(cpi, get_block_context(x, bsize), bsize, output_enabled);
  encode_superblock(cpi, tp, output_enabled, mi_row, mi_col, bsize);

  if (output_enabled) {
    update_stats(cpi);

    (*tp)->token = EOSB_TOKEN;
    (*tp)++;
  }
}

static void encode_sb(VP9_COMP *cpi, const TileInfo *const tile,
                      TOKENEXTRA **tp, int mi_row, int mi_col,
                      int output_enabled, BLOCK_SIZE bsize) {
  VP9_COMMON *const cm = &cpi->common;
  MACROBLOCK *const x = &cpi->mb;
  const int bsl = b_width_log2(bsize), hbs = (1 << bsl) / 4;
  int ctx;
  PARTITION_TYPE partition;
  BLOCK_SIZE subsize;

  if (mi_row >= cm->mi_rows || mi_col >= cm->mi_cols)
    return;

  if (bsize >= BLOCK_8X8) {
    ctx = partition_plane_context(cpi->above_seg_context, cpi->left_seg_context,
                                 mi_row, mi_col, bsize);
    subsize = *get_sb_partitioning(x, bsize);
  } else {
    ctx = 0;
    subsize = BLOCK_4X4;
  }

  partition = partition_lookup[bsl][subsize];

  switch (partition) {
    case PARTITION_NONE:
      if (output_enabled && bsize >= BLOCK_8X8)
        cm->counts.partition[ctx][PARTITION_NONE]++;
      encode_b(cpi, tile, tp, mi_row, mi_col, output_enabled, subsize);
      break;
    case PARTITION_VERT:
      if (output_enabled)
        cm->counts.partition[ctx][PARTITION_VERT]++;
      *get_sb_index(x, subsize) = 0;
      encode_b(cpi, tile, tp, mi_row, mi_col, output_enabled, subsize);
      if (mi_col + hbs < cm->mi_cols) {
        *get_sb_index(x, subsize) = 1;
        encode_b(cpi, tile, tp, mi_row, mi_col + hbs, output_enabled, subsize);
      }
      break;
    case PARTITION_HORZ:
      if (output_enabled)
        cm->counts.partition[ctx][PARTITION_HORZ]++;
      *get_sb_index(x, subsize) = 0;
      encode_b(cpi, tile, tp, mi_row, mi_col, output_enabled, subsize);
      if (mi_row + hbs < cm->mi_rows) {
        *get_sb_index(x, subsize) = 1;
        encode_b(cpi, tile, tp, mi_row + hbs, mi_col, output_enabled, subsize);
      }
      break;
    case PARTITION_SPLIT:
      subsize = get_subsize(bsize, PARTITION_SPLIT);
      if (output_enabled)
        cm->counts.partition[ctx][PARTITION_SPLIT]++;

      *get_sb_index(x, subsize) = 0;
      encode_sb(cpi, tile, tp, mi_row, mi_col, output_enabled, subsize);
      *get_sb_index(x, subsize) = 1;
      encode_sb(cpi, tile, tp, mi_row, mi_col + hbs, output_enabled, subsize);
      *get_sb_index(x, subsize) = 2;
      encode_sb(cpi, tile, tp, mi_row + hbs, mi_col, output_enabled, subsize);
      *get_sb_index(x, subsize) = 3;
      encode_sb(cpi, tile, tp, mi_row + hbs, mi_col + hbs, output_enabled,
                subsize);
      break;
    default:
      assert("Invalid partition type.");
  }

  if (partition != PARTITION_SPLIT || bsize == BLOCK_8X8)
    update_partition_context(cpi->above_seg_context, cpi->left_seg_context,
                             mi_row, mi_col, subsize, bsize);
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
    for (; bsize > 0; --bsize) {
      *bh = num_8x8_blocks_high_lookup[bsize];
      *bw = num_8x8_blocks_wide_lookup[bsize];
      if ((*bh <= rows_left) && (*bw <= cols_left)) {
        break;
      }
    }
  }
  return bsize;
}

// This function attempts to set all mode info entries in a given SB64
// to the same block partition size.
// However, at the bottom and right borders of the image the requested size
// may not be allowed in which case this code attempts to choose the largest
// allowable partition.
static void set_partitioning(VP9_COMP *cpi, const TileInfo *const tile,
                             MODE_INFO **mi_8x8, int mi_row, int mi_col) {
  VP9_COMMON *const cm = &cpi->common;
  BLOCK_SIZE bsize = cpi->sf.always_this_block_size;
  const int mis = cm->mode_info_stride;
  int row8x8_remaining = tile->mi_row_end - mi_row;
  int col8x8_remaining = tile->mi_col_end - mi_col;
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
    for (block_row = 0; block_row < MI_BLOCK_SIZE; block_row += bh) {
      for (block_col = 0; block_col < MI_BLOCK_SIZE; block_col += bw) {
        int index = block_row * mis + block_col;
        // Find a partition size that fits
        bsize = find_partition_size(cpi->sf.always_this_block_size,
                                    (row8x8_remaining - block_row),
                                    (col8x8_remaining - block_col), &bh, &bw);
        mi_8x8[index] = mi_upper_left + index;
        mi_8x8[index]->mbmi.sb_type = bsize;
      }
    }
  }
}

static void copy_partitioning(VP9_COMMON *cm, MODE_INFO **mi_8x8,
                              MODE_INFO **prev_mi_8x8) {
  const int mis = cm->mode_info_stride;
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

static int sb_has_motion(const VP9_COMMON *cm, MODE_INFO **prev_mi_8x8) {
  const int mis = cm->mode_info_stride;
  int block_row, block_col;

  if (cm->prev_mi) {
    for (block_row = 0; block_row < 8; ++block_row) {
      for (block_col = 0; block_col < 8; ++block_col) {
        const MODE_INFO *prev_mi = prev_mi_8x8[block_row * mis + block_col];
        if (prev_mi) {
          if (abs(prev_mi->mbmi.mv[0].as_mv.row) >= 8 ||
              abs(prev_mi->mbmi.mv[0].as_mv.col) >= 8)
            return 1;
        }
      }
    }
  }
  return 0;
}

static void update_state_rt(VP9_COMP *cpi, PICK_MODE_CONTEXT *ctx,
                         BLOCK_SIZE bsize, int output_enabled) {
  int i;
  VP9_COMMON *const cm = &cpi->common;
  MACROBLOCK *const x = &cpi->mb;
  MACROBLOCKD *const xd = &x->e_mbd;
  struct macroblock_plane *const p = x->plane;
  struct macroblockd_plane *const pd = xd->plane;
  MB_MODE_INFO *const mbmi = &xd->mi_8x8[0]->mbmi;

  const int mb_mode_index = ctx->best_mode_index;
  int max_plane;

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

  x->skip = ctx->skip;

  if (frame_is_intra_only(cm)) {
#if CONFIG_INTERNAL_STATS
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
      THR_TM /*TM_PRED*/,
    };
    ++cpi->mode_chosen_counts[kf_mode_index[mbmi->mode]];
#endif
  } else {
    // Note how often each mode chosen as best
    cpi->mode_chosen_counts[mb_mode_index]++;
    if (is_inter_block(mbmi)) {
      if (mbmi->sb_type < BLOCK_8X8 || mbmi->mode == NEWMV) {
        int_mv best_mv[2];
        for (i = 0; i < 1 + has_second_ref(mbmi); ++i)
          best_mv[i].as_int = mbmi->ref_mvs[mbmi->ref_frame[i]][0].as_int;
        vp9_update_mv_count(cpi, x, best_mv);
      }

      if (cm->interp_filter == SWITCHABLE) {
        const int ctx = vp9_get_pred_context_switchable_interp(xd);
        ++cm->counts.switchable_interp[ctx][mbmi->interp_filter];
      }
    }
  }
}

static void encode_b_rt(VP9_COMP *cpi, const TileInfo *const tile,
                     TOKENEXTRA **tp, int mi_row, int mi_col,
                     int output_enabled, BLOCK_SIZE bsize) {
  MACROBLOCK *const x = &cpi->mb;

  if (bsize < BLOCK_8X8) {
    // When ab_index = 0 all sub-blocks are handled, so for ab_index != 0
    // there is nothing to be done.
    if (x->ab_index > 0)
      return;
  }
  set_offsets(cpi, tile, mi_row, mi_col, bsize);
  update_state_rt(cpi, get_block_context(x, bsize), bsize, output_enabled);

  encode_superblock(cpi, tp, output_enabled, mi_row, mi_col, bsize);
  update_stats(cpi);

  (*tp)->token = EOSB_TOKEN;
  (*tp)++;
}

static void encode_sb_rt(VP9_COMP *cpi, const TileInfo *const tile,
                      TOKENEXTRA **tp, int mi_row, int mi_col,
                      int output_enabled, BLOCK_SIZE bsize) {
  VP9_COMMON *const cm = &cpi->common;
  MACROBLOCK *const x = &cpi->mb;
  const int bsl = b_width_log2(bsize), hbs = (1 << bsl) / 4;
  int ctx;
  PARTITION_TYPE partition;
  BLOCK_SIZE subsize;

  if (mi_row >= cm->mi_rows || mi_col >= cm->mi_cols)
    return;

  if (bsize >= BLOCK_8X8) {
    MACROBLOCKD *const xd = &cpi->mb.e_mbd;
    const int idx_str = xd->mode_info_stride * mi_row + mi_col;
    MODE_INFO ** mi_8x8 = cm->mi_grid_visible + idx_str;
    ctx = partition_plane_context(cpi->above_seg_context, cpi->left_seg_context,
                                 mi_row, mi_col, bsize);
    subsize = mi_8x8[0]->mbmi.sb_type;

  } else {
    ctx = 0;
    subsize = BLOCK_4X4;
  }

  partition = partition_lookup[bsl][subsize];

  switch (partition) {
    case PARTITION_NONE:
      if (output_enabled && bsize >= BLOCK_8X8)
        cm->counts.partition[ctx][PARTITION_NONE]++;
      encode_b_rt(cpi, tile, tp, mi_row, mi_col, output_enabled, subsize);
      break;
    case PARTITION_VERT:
      if (output_enabled)
        cm->counts.partition[ctx][PARTITION_VERT]++;
      *get_sb_index(x, subsize) = 0;
      encode_b_rt(cpi, tile, tp, mi_row, mi_col, output_enabled, subsize);
      if (mi_col + hbs < cm->mi_cols) {
        *get_sb_index(x, subsize) = 1;
        encode_b_rt(cpi, tile, tp, mi_row, mi_col + hbs, output_enabled,
                    subsize);
      }
      break;
    case PARTITION_HORZ:
      if (output_enabled)
        cm->counts.partition[ctx][PARTITION_HORZ]++;
      *get_sb_index(x, subsize) = 0;
      encode_b_rt(cpi, tile, tp, mi_row, mi_col, output_enabled, subsize);
      if (mi_row + hbs < cm->mi_rows) {
        *get_sb_index(x, subsize) = 1;
        encode_b_rt(cpi, tile, tp, mi_row + hbs, mi_col, output_enabled,
                    subsize);
      }
      break;
    case PARTITION_SPLIT:
      subsize = get_subsize(bsize, PARTITION_SPLIT);
      if (output_enabled)
        cm->counts.partition[ctx][PARTITION_SPLIT]++;

      *get_sb_index(x, subsize) = 0;
      encode_sb_rt(cpi, tile, tp, mi_row, mi_col, output_enabled, subsize);
      *get_sb_index(x, subsize) = 1;
      encode_sb_rt(cpi, tile, tp, mi_row, mi_col + hbs, output_enabled,
                   subsize);
      *get_sb_index(x, subsize) = 2;
      encode_sb_rt(cpi, tile, tp, mi_row + hbs, mi_col, output_enabled,
                   subsize);
      *get_sb_index(x, subsize) = 3;
      encode_sb_rt(cpi, tile, tp, mi_row + hbs, mi_col + hbs, output_enabled,
                subsize);
      break;
    default:
      assert("Invalid partition type.");
  }

  if (partition != PARTITION_SPLIT || bsize == BLOCK_8X8)
    update_partition_context(cpi->above_seg_context, cpi->left_seg_context,
                             mi_row, mi_col, subsize, bsize);
}

static void rd_use_partition(VP9_COMP *cpi,
                             const TileInfo *const tile,
                             MODE_INFO **mi_8x8,
                             TOKENEXTRA **tp, int mi_row, int mi_col,
                             BLOCK_SIZE bsize, int *rate, int64_t *dist,
                             int do_recon) {
  VP9_COMMON *const cm = &cpi->common;
  MACROBLOCK *const x = &cpi->mb;
  const int mis = cm->mode_info_stride;
  const int bsl = b_width_log2(bsize);
  const int num_4x4_blocks_wide = num_4x4_blocks_wide_lookup[bsize];
  const int num_4x4_blocks_high = num_4x4_blocks_high_lookup[bsize];
  const int ms = num_4x4_blocks_wide / 2;
  const int mh = num_4x4_blocks_high / 2;
  const int bss = (1 << bsl) / 4;
  int i, pl;
  PARTITION_TYPE partition = PARTITION_NONE;
  BLOCK_SIZE subsize;
  ENTROPY_CONTEXT l[16 * MAX_MB_PLANE], a[16 * MAX_MB_PLANE];
  PARTITION_CONTEXT sl[8], sa[8];
  int last_part_rate = INT_MAX;
  int64_t last_part_dist = INT_MAX;
  int split_rate = INT_MAX;
  int64_t split_dist = INT_MAX;
  int none_rate = INT_MAX;
  int64_t none_dist = INT_MAX;
  int chosen_rate = INT_MAX;
  int64_t chosen_dist = INT_MAX;
  BLOCK_SIZE sub_subsize = BLOCK_4X4;
  int splits_below = 0;
  BLOCK_SIZE bs_type = mi_8x8[0]->mbmi.sb_type;

  if (mi_row >= cm->mi_rows || mi_col >= cm->mi_cols)
    return;

  partition = partition_lookup[bsl][bs_type];
  subsize = get_subsize(bsize, partition);

  if (bsize < BLOCK_8X8) {
    // When ab_index = 0 all sub-blocks are handled, so for ab_index != 0
    // there is nothing to be done.
    if (x->ab_index != 0) {
      *rate = 0;
      *dist = 0;
      return;
    }
  } else {
    *(get_sb_partitioning(x, bsize)) = subsize;
  }
  save_context(cpi, mi_row, mi_col, a, l, sa, sl, bsize);

  if (bsize == BLOCK_16X16) {
    set_offsets(cpi, tile, mi_row, mi_col, bsize);
    x->mb_energy = vp9_block_energy(cpi, x, bsize);
  }

  if (cpi->sf.adjust_partitioning_from_last_frame) {
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
        mi_row + (ms >> 1) < cm->mi_rows &&
        mi_col + (ms >> 1) < cm->mi_cols) {
      *(get_sb_partitioning(x, bsize)) = bsize;
      rd_pick_sb_modes(cpi, tile, mi_row, mi_col, &none_rate, &none_dist, bsize,
                       get_block_context(x, bsize), INT64_MAX);

      pl = partition_plane_context(cpi->above_seg_context,
                                   cpi->left_seg_context,
                                   mi_row, mi_col, bsize);
      none_rate += x->partition_cost[pl][PARTITION_NONE];

      restore_context(cpi, mi_row, mi_col, a, l, sa, sl, bsize);
      mi_8x8[0]->mbmi.sb_type = bs_type;
      *(get_sb_partitioning(x, bsize)) = subsize;
    }
  }

  switch (partition) {
    case PARTITION_NONE:
      rd_pick_sb_modes(cpi, tile, mi_row, mi_col, &last_part_rate,
                       &last_part_dist, bsize,
                       get_block_context(x, bsize), INT64_MAX);
      break;
    case PARTITION_HORZ:
      *get_sb_index(x, subsize) = 0;
      rd_pick_sb_modes(cpi, tile, mi_row, mi_col, &last_part_rate,
                       &last_part_dist, subsize,
                       get_block_context(x, subsize), INT64_MAX);
      if (last_part_rate != INT_MAX &&
          bsize >= BLOCK_8X8 && mi_row + (mh >> 1) < cm->mi_rows) {
        int rt = 0;
        int64_t dt = 0;
        update_state(cpi, get_block_context(x, subsize), subsize, 0);
        encode_superblock(cpi, tp, 0, mi_row, mi_col, subsize);
        *get_sb_index(x, subsize) = 1;
        rd_pick_sb_modes(cpi, tile, mi_row + (ms >> 1), mi_col, &rt, &dt,
                         subsize, get_block_context(x, subsize), INT64_MAX);
        if (rt == INT_MAX || dt == INT_MAX) {
          last_part_rate = INT_MAX;
          last_part_dist = INT_MAX;
          break;
        }

        last_part_rate += rt;
        last_part_dist += dt;
      }
      break;
    case PARTITION_VERT:
      *get_sb_index(x, subsize) = 0;
      rd_pick_sb_modes(cpi, tile, mi_row, mi_col, &last_part_rate,
                       &last_part_dist, subsize,
                       get_block_context(x, subsize), INT64_MAX);
      if (last_part_rate != INT_MAX &&
          bsize >= BLOCK_8X8 && mi_col + (ms >> 1) < cm->mi_cols) {
        int rt = 0;
        int64_t dt = 0;
        update_state(cpi, get_block_context(x, subsize), subsize, 0);
        encode_superblock(cpi, tp, 0, mi_row, mi_col, subsize);
        *get_sb_index(x, subsize) = 1;
        rd_pick_sb_modes(cpi, tile, mi_row, mi_col + (ms >> 1), &rt, &dt,
                         subsize, get_block_context(x, subsize), INT64_MAX);
        if (rt == INT_MAX || dt == INT_MAX) {
          last_part_rate = INT_MAX;
          last_part_dist = INT_MAX;
          break;
        }
        last_part_rate += rt;
        last_part_dist += dt;
      }
      break;
    case PARTITION_SPLIT:
      // Split partition.
      last_part_rate = 0;
      last_part_dist = 0;
      for (i = 0; i < 4; i++) {
        int x_idx = (i & 1) * (ms >> 1);
        int y_idx = (i >> 1) * (ms >> 1);
        int jj = i >> 1, ii = i & 0x01;
        int rt;
        int64_t dt;

        if ((mi_row + y_idx >= cm->mi_rows) || (mi_col + x_idx >= cm->mi_cols))
          continue;

        *get_sb_index(x, subsize) = i;

        rd_use_partition(cpi, tile, mi_8x8 + jj * bss * mis + ii * bss, tp,
                         mi_row + y_idx, mi_col + x_idx, subsize, &rt, &dt,
                         i != 3);
        if (rt == INT_MAX || dt == INT_MAX) {
          last_part_rate = INT_MAX;
          last_part_dist = INT_MAX;
          break;
        }
        last_part_rate += rt;
        last_part_dist += dt;
      }
      break;
    default:
      assert(0);
  }

  pl = partition_plane_context(cpi->above_seg_context, cpi->left_seg_context,
                               mi_row, mi_col, bsize);
  if (last_part_rate < INT_MAX)
    last_part_rate += x->partition_cost[pl][partition];

  if (cpi->sf.adjust_partitioning_from_last_frame
      && partition != PARTITION_SPLIT && bsize > BLOCK_8X8
      && (mi_row + ms < cm->mi_rows || mi_row + (ms >> 1) == cm->mi_rows)
      && (mi_col + ms < cm->mi_cols || mi_col + (ms >> 1) == cm->mi_cols)) {
    BLOCK_SIZE split_subsize = get_subsize(bsize, PARTITION_SPLIT);
    split_rate = 0;
    split_dist = 0;
    restore_context(cpi, mi_row, mi_col, a, l, sa, sl, bsize);

    // Split partition.
    for (i = 0; i < 4; i++) {
      int x_idx = (i & 1) * (num_4x4_blocks_wide >> 2);
      int y_idx = (i >> 1) * (num_4x4_blocks_wide >> 2);
      int rt = 0;
      int64_t dt = 0;
      ENTROPY_CONTEXT l[16 * MAX_MB_PLANE], a[16 * MAX_MB_PLANE];
      PARTITION_CONTEXT sl[8], sa[8];

      if ((mi_row + y_idx >= cm->mi_rows) || (mi_col + x_idx >= cm->mi_cols))
        continue;

      *get_sb_index(x, split_subsize) = i;
      *get_sb_partitioning(x, bsize) = split_subsize;
      *get_sb_partitioning(x, split_subsize) = split_subsize;

      save_context(cpi, mi_row, mi_col, a, l, sa, sl, bsize);

      rd_pick_sb_modes(cpi, tile, mi_row + y_idx, mi_col + x_idx, &rt, &dt,
                       split_subsize, get_block_context(x, split_subsize),
                       INT64_MAX);

      restore_context(cpi, mi_row, mi_col, a, l, sa, sl, bsize);

      if (rt == INT_MAX || dt == INT_MAX) {
        split_rate = INT_MAX;
        split_dist = INT_MAX;
        break;
      }

      if (i != 3)
        encode_sb(cpi, tile, tp,  mi_row + y_idx, mi_col + x_idx, 0,
                  split_subsize);

      split_rate += rt;
      split_dist += dt;
      pl = partition_plane_context(cpi->above_seg_context,
                                   cpi->left_seg_context,
                                   mi_row + y_idx, mi_col + x_idx,
                                   split_subsize);
      split_rate += x->partition_cost[pl][PARTITION_NONE];
    }
    pl = partition_plane_context(cpi->above_seg_context, cpi->left_seg_context,
                                 mi_row, mi_col, bsize);
    if (split_rate < INT_MAX) {
      split_rate += x->partition_cost[pl][PARTITION_SPLIT];

      chosen_rate = split_rate;
      chosen_dist = split_dist;
    }
  }

  // If last_part is better set the partitioning to that...
  if (RDCOST(x->rdmult, x->rddiv, last_part_rate, last_part_dist)
      < RDCOST(x->rdmult, x->rddiv, chosen_rate, chosen_dist)) {
    mi_8x8[0]->mbmi.sb_type = bsize;
    if (bsize >= BLOCK_8X8)
      *(get_sb_partitioning(x, bsize)) = subsize;
    chosen_rate = last_part_rate;
    chosen_dist = last_part_dist;
  }
  // If none was better set the partitioning to that...
  if (RDCOST(x->rdmult, x->rddiv, chosen_rate, chosen_dist)
      > RDCOST(x->rdmult, x->rddiv, none_rate, none_dist)) {
    if (bsize >= BLOCK_8X8)
      *(get_sb_partitioning(x, bsize)) = bsize;
    chosen_rate = none_rate;
    chosen_dist = none_dist;
  }

  restore_context(cpi, mi_row, mi_col, a, l, sa, sl, bsize);

  // We must have chosen a partitioning and encoding or we'll fail later on.
  // No other opportunities for success.
  if ( bsize == BLOCK_64X64)
    assert(chosen_rate < INT_MAX && chosen_dist < INT_MAX);

  if (do_recon) {
    int output_enabled = (bsize == BLOCK_64X64);

    // Check the projected output rate for this SB against it's target
    // and and if necessary apply a Q delta using segmentation to get
    // closer to the target.
    if ((cpi->oxcf.aq_mode == COMPLEXITY_AQ) && cm->seg.update_map) {
      select_in_frame_q_segment(cpi, mi_row, mi_col,
                                output_enabled, chosen_rate);
    }

    encode_sb(cpi, tile, tp, mi_row, mi_col, output_enabled, bsize);
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
static void get_sb_partition_size_range(VP9_COMP *cpi, MODE_INFO ** mi_8x8,
                                        BLOCK_SIZE * min_block_size,
                                        BLOCK_SIZE * max_block_size ) {
  MACROBLOCKD *const xd = &cpi->mb.e_mbd;
  int sb_width_in_blocks = MI_BLOCK_SIZE;
  int sb_height_in_blocks  = MI_BLOCK_SIZE;
  int i, j;
  int index = 0;

  // Check the sb_type for each block that belongs to this region.
  for (i = 0; i < sb_height_in_blocks; ++i) {
    for (j = 0; j < sb_width_in_blocks; ++j) {
      MODE_INFO * mi = mi_8x8[index+j];
      BLOCK_SIZE sb_type = mi ? mi->mbmi.sb_type : 0;
      *min_block_size = MIN(*min_block_size, sb_type);
      *max_block_size = MAX(*max_block_size, sb_type);
    }
    index += xd->mode_info_stride;
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
                                    int row, int col,
                                    BLOCK_SIZE *min_block_size,
                                    BLOCK_SIZE *max_block_size) {
  VP9_COMMON * const cm = &cpi->common;
  MACROBLOCKD *const xd = &cpi->mb.e_mbd;
  MODE_INFO ** mi_8x8 = xd->mi_8x8;
  MODE_INFO ** prev_mi_8x8 = xd->prev_mi_8x8;

  const int left_in_image = xd->left_available && mi_8x8[-1];
  const int above_in_image = xd->up_available &&
                             mi_8x8[-xd->mode_info_stride];
  MODE_INFO ** above_sb64_mi_8x8;
  MODE_INFO ** left_sb64_mi_8x8;

  int row8x8_remaining = tile->mi_row_end - row;
  int col8x8_remaining = tile->mi_col_end - col;
  int bh, bw;

  // Trap case where we do not have a prediction.
  if (!left_in_image && !above_in_image &&
      ((cm->frame_type == KEY_FRAME) || !cm->prev_mi)) {
    *min_block_size = BLOCK_4X4;
    *max_block_size = BLOCK_64X64;
  } else {
    // Default "min to max" and "max to min"
    *min_block_size = BLOCK_64X64;
    *max_block_size = BLOCK_4X4;

    // NOTE: each call to get_sb_partition_size_range() uses the previous
    // passed in values for min and max as a starting point.
    //
    // Find the min and max partition used in previous frame at this location
    if (cm->prev_mi && (cm->frame_type != KEY_FRAME)) {
      get_sb_partition_size_range(cpi, prev_mi_8x8,
                                  min_block_size, max_block_size);
    }

    // Find the min and max partition sizes used in the left SB64
    if (left_in_image) {
      left_sb64_mi_8x8 = &mi_8x8[-MI_BLOCK_SIZE];
      get_sb_partition_size_range(cpi, left_sb64_mi_8x8,
                                  min_block_size, max_block_size);
    }

    // Find the min and max partition sizes used in the above SB64.
    if (above_in_image) {
      above_sb64_mi_8x8 = &mi_8x8[-xd->mode_info_stride * MI_BLOCK_SIZE];
      get_sb_partition_size_range(cpi, above_sb64_mi_8x8,
                                  min_block_size, max_block_size);
    }
  }

  // adjust observed min and max
  if (cpi->sf.auto_min_max_partition_size == RELAXED_NEIGHBORING_MIN_MAX) {
    *min_block_size = min_partition_size[*min_block_size];
    *max_block_size = max_partition_size[*max_block_size];
  }

  // Check border cases where max and min from neighbours may not be legal.
  *max_block_size = find_partition_size(*max_block_size,
                                        row8x8_remaining, col8x8_remaining,
                                        &bh, &bw);
  *min_block_size = MIN(*min_block_size, *max_block_size);

  // When use_square_partition_only is true, make sure at least one square
  // partition is allowed by selecting the next smaller square size as
  // *min_block_size.
  if (cpi->sf.use_square_partition_only &&
      (*max_block_size - *min_block_size) < 2) {
    *min_block_size = next_square_size[*min_block_size];
  }
}

static INLINE void store_pred_mv(MACROBLOCK *x, PICK_MODE_CONTEXT *ctx) {
  vpx_memcpy(ctx->pred_mv, x->pred_mv, sizeof(x->pred_mv));
}

static INLINE void load_pred_mv(MACROBLOCK *x, PICK_MODE_CONTEXT *ctx) {
  vpx_memcpy(x->pred_mv, ctx->pred_mv, sizeof(x->pred_mv));
}

// TODO(jingning,jimbankoski,rbultje): properly skip partition types that are
// unlikely to be selected depending on previous rate-distortion optimization
// results, for encoding speed-up.
static void rd_pick_partition(VP9_COMP *cpi, const TileInfo *const tile,
                              TOKENEXTRA **tp, int mi_row,
                              int mi_col, BLOCK_SIZE bsize, int *rate,
                              int64_t *dist, int do_recon, int64_t best_rd) {
  VP9_COMMON *const cm = &cpi->common;
  MACROBLOCK *const x = &cpi->mb;
  const int ms = num_8x8_blocks_wide_lookup[bsize] / 2;
  ENTROPY_CONTEXT l[16 * MAX_MB_PLANE], a[16 * MAX_MB_PLANE];
  PARTITION_CONTEXT sl[8], sa[8];
  TOKENEXTRA *tp_orig = *tp;
  int i, pl;
  BLOCK_SIZE subsize;
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

  if (bsize < BLOCK_8X8) {
    // When ab_index = 0 all sub-blocks are handled, so for ab_index != 0
    // there is nothing to be done.
    if (x->ab_index != 0) {
      *rate = 0;
      *dist = 0;
      return;
    }
  }
  assert(num_8x8_blocks_wide_lookup[bsize] ==
             num_8x8_blocks_high_lookup[bsize]);

  if (bsize == BLOCK_16X16) {
    set_offsets(cpi, tile, mi_row, mi_col, bsize);
    x->mb_energy = vp9_block_energy(cpi, x, bsize);
  }

  // Determine partition types in search according to the speed features.
  // The threshold set here has to be of square block size.
  if (cpi->sf.auto_min_max_partition_size) {
    partition_none_allowed &= (bsize <= cpi->sf.max_partition_size &&
                               bsize >= cpi->sf.min_partition_size);
    partition_horz_allowed &= ((bsize <= cpi->sf.max_partition_size &&
                                bsize >  cpi->sf.min_partition_size) ||
                                force_horz_split);
    partition_vert_allowed &= ((bsize <= cpi->sf.max_partition_size &&
                                bsize >  cpi->sf.min_partition_size) ||
                                force_vert_split);
    do_split &= bsize > cpi->sf.min_partition_size;
  }
  if (cpi->sf.use_square_partition_only) {
    partition_horz_allowed &= force_horz_split;
    partition_vert_allowed &= force_vert_split;
  }

  save_context(cpi, mi_row, mi_col, a, l, sa, sl, bsize);

  if (cpi->sf.disable_split_var_thresh && partition_none_allowed) {
    unsigned int source_variancey;
    vp9_setup_src_planes(x, cpi->Source, mi_row, mi_col);
    source_variancey = get_sby_perpixel_variance(cpi, x, bsize);
    if (source_variancey < cpi->sf.disable_split_var_thresh) {
      do_split = 0;
      if (source_variancey < cpi->sf.disable_split_var_thresh / 2)
        do_rect = 0;
    }
  }

  // PARTITION_NONE
  if (partition_none_allowed) {
    rd_pick_sb_modes(cpi, tile, mi_row, mi_col, &this_rate, &this_dist, bsize,
                     get_block_context(x, bsize), best_rd);
    if (this_rate != INT_MAX) {
      if (bsize >= BLOCK_8X8) {
        pl = partition_plane_context(cpi->above_seg_context,
                                     cpi->left_seg_context,
                                     mi_row, mi_col, bsize);
        this_rate += x->partition_cost[pl][PARTITION_NONE];
      }
      sum_rd = RDCOST(x->rdmult, x->rddiv, this_rate, this_dist);
      if (sum_rd < best_rd) {
        int64_t stop_thresh = 4096;
        int64_t stop_thresh_rd;

        best_rate = this_rate;
        best_dist = this_dist;
        best_rd = sum_rd;
        if (bsize >= BLOCK_8X8)
          *(get_sb_partitioning(x, bsize)) = bsize;

        // Adjust threshold according to partition size.
        stop_thresh >>= 8 - (b_width_log2_lookup[bsize] +
            b_height_log2_lookup[bsize]);

        stop_thresh_rd = RDCOST(x->rdmult, x->rddiv, 0, stop_thresh);
        // If obtained distortion is very small, choose current partition
        // and stop splitting.
        if (!x->e_mbd.lossless && best_rd < stop_thresh_rd) {
          do_split = 0;
          do_rect = 0;
        }
      }
    }
    restore_context(cpi, mi_row, mi_col, a, l, sa, sl, bsize);
  }

  // store estimated motion vector
  if (cpi->sf.adaptive_motion_search)
    store_pred_mv(x, get_block_context(x, bsize));

  // PARTITION_SPLIT
  sum_rd = 0;
  // TODO(jingning): use the motion vectors given by the above search as
  // the starting point of motion search in the following partition type check.
  if (do_split) {
    subsize = get_subsize(bsize, PARTITION_SPLIT);
    for (i = 0; i < 4 && sum_rd < best_rd; ++i) {
      const int x_idx = (i & 1) * ms;
      const int y_idx = (i >> 1) * ms;

      if (mi_row + y_idx >= cm->mi_rows || mi_col + x_idx >= cm->mi_cols)
        continue;

      *get_sb_index(x, subsize) = i;
      if (cpi->sf.adaptive_motion_search)
        load_pred_mv(x, get_block_context(x, bsize));
      if (cpi->sf.adaptive_pred_interp_filter && bsize == BLOCK_8X8 &&
          partition_none_allowed)
        get_block_context(x, subsize)->pred_interp_filter =
            get_block_context(x, bsize)->mic.mbmi.interp_filter;
      rd_pick_partition(cpi, tile, tp, mi_row + y_idx, mi_col + x_idx, subsize,
                        &this_rate, &this_dist, i != 3, best_rd - sum_rd);

      if (this_rate == INT_MAX) {
        sum_rd = INT64_MAX;
      } else {
        sum_rate += this_rate;
        sum_dist += this_dist;
        sum_rd = RDCOST(x->rdmult, x->rddiv, sum_rate, sum_dist);
      }
    }
    if (sum_rd < best_rd && i == 4) {
      pl = partition_plane_context(cpi->above_seg_context,
                                   cpi->left_seg_context,
                                   mi_row, mi_col, bsize);
      sum_rate += x->partition_cost[pl][PARTITION_SPLIT];
      sum_rd = RDCOST(x->rdmult, x->rddiv, sum_rate, sum_dist);
      if (sum_rd < best_rd) {
        best_rate = sum_rate;
        best_dist = sum_dist;
        best_rd = sum_rd;
        *(get_sb_partitioning(x, bsize)) = subsize;
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
    *get_sb_index(x, subsize) = 0;
    if (cpi->sf.adaptive_motion_search)
      load_pred_mv(x, get_block_context(x, bsize));
    if (cpi->sf.adaptive_pred_interp_filter && bsize == BLOCK_8X8 &&
        partition_none_allowed)
      get_block_context(x, subsize)->pred_interp_filter =
          get_block_context(x, bsize)->mic.mbmi.interp_filter;
    rd_pick_sb_modes(cpi, tile, mi_row, mi_col, &sum_rate, &sum_dist, subsize,
                     get_block_context(x, subsize), best_rd);
    sum_rd = RDCOST(x->rdmult, x->rddiv, sum_rate, sum_dist);

    if (sum_rd < best_rd && mi_row + ms < cm->mi_rows) {
      update_state(cpi, get_block_context(x, subsize), subsize, 0);
      encode_superblock(cpi, tp, 0, mi_row, mi_col, subsize);

      *get_sb_index(x, subsize) = 1;
      if (cpi->sf.adaptive_motion_search)
        load_pred_mv(x, get_block_context(x, bsize));
      if (cpi->sf.adaptive_pred_interp_filter && bsize == BLOCK_8X8 &&
          partition_none_allowed)
        get_block_context(x, subsize)->pred_interp_filter =
            get_block_context(x, bsize)->mic.mbmi.interp_filter;
      rd_pick_sb_modes(cpi, tile, mi_row + ms, mi_col, &this_rate,
                       &this_dist, subsize, get_block_context(x, subsize),
                       best_rd - sum_rd);
      if (this_rate == INT_MAX) {
        sum_rd = INT64_MAX;
      } else {
        sum_rate += this_rate;
        sum_dist += this_dist;
        sum_rd = RDCOST(x->rdmult, x->rddiv, sum_rate, sum_dist);
      }
    }
    if (sum_rd < best_rd) {
      pl = partition_plane_context(cpi->above_seg_context,
                                   cpi->left_seg_context,
                                   mi_row, mi_col, bsize);
      sum_rate += x->partition_cost[pl][PARTITION_HORZ];
      sum_rd = RDCOST(x->rdmult, x->rddiv, sum_rate, sum_dist);
      if (sum_rd < best_rd) {
        best_rd = sum_rd;
        best_rate = sum_rate;
        best_dist = sum_dist;
        *(get_sb_partitioning(x, bsize)) = subsize;
      }
    }
    restore_context(cpi, mi_row, mi_col, a, l, sa, sl, bsize);
  }

  // PARTITION_VERT
  if (partition_vert_allowed && do_rect) {
    subsize = get_subsize(bsize, PARTITION_VERT);

    *get_sb_index(x, subsize) = 0;
    if (cpi->sf.adaptive_motion_search)
      load_pred_mv(x, get_block_context(x, bsize));
    if (cpi->sf.adaptive_pred_interp_filter && bsize == BLOCK_8X8 &&
        partition_none_allowed)
      get_block_context(x, subsize)->pred_interp_filter =
          get_block_context(x, bsize)->mic.mbmi.interp_filter;
    rd_pick_sb_modes(cpi, tile, mi_row, mi_col, &sum_rate, &sum_dist, subsize,
                     get_block_context(x, subsize), best_rd);
    sum_rd = RDCOST(x->rdmult, x->rddiv, sum_rate, sum_dist);
    if (sum_rd < best_rd && mi_col + ms < cm->mi_cols) {
      update_state(cpi, get_block_context(x, subsize), subsize, 0);
      encode_superblock(cpi, tp, 0, mi_row, mi_col, subsize);

      *get_sb_index(x, subsize) = 1;
      if (cpi->sf.adaptive_motion_search)
        load_pred_mv(x, get_block_context(x, bsize));
      if (cpi->sf.adaptive_pred_interp_filter && bsize == BLOCK_8X8 &&
          partition_none_allowed)
        get_block_context(x, subsize)->pred_interp_filter =
            get_block_context(x, bsize)->mic.mbmi.interp_filter;
      rd_pick_sb_modes(cpi, tile, mi_row, mi_col + ms, &this_rate,
                       &this_dist, subsize, get_block_context(x, subsize),
                       best_rd - sum_rd);
      if (this_rate == INT_MAX) {
        sum_rd = INT64_MAX;
      } else {
        sum_rate += this_rate;
        sum_dist += this_dist;
        sum_rd = RDCOST(x->rdmult, x->rddiv, sum_rate, sum_dist);
      }
    }
    if (sum_rd < best_rd) {
      pl = partition_plane_context(cpi->above_seg_context,
                                   cpi->left_seg_context,
                                   mi_row, mi_col, bsize);
      sum_rate += x->partition_cost[pl][PARTITION_VERT];
      sum_rd = RDCOST(x->rdmult, x->rddiv, sum_rate, sum_dist);
      if (sum_rd < best_rd) {
        best_rate = sum_rate;
        best_dist = sum_dist;
        best_rd = sum_rd;
        *(get_sb_partitioning(x, bsize)) = subsize;
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

  if (best_rate < INT_MAX && best_dist < INT64_MAX && do_recon) {
    int output_enabled = (bsize == BLOCK_64X64);

    // Check the projected output rate for this SB against it's target
    // and and if necessary apply a Q delta using segmentation to get
    // closer to the target.
    if ((cpi->oxcf.aq_mode == COMPLEXITY_AQ) && cm->seg.update_map) {
      select_in_frame_q_segment(cpi, mi_row, mi_col, output_enabled, best_rate);
    }
    encode_sb(cpi, tile, tp, mi_row, mi_col, output_enabled, bsize);
  }
  if (bsize == BLOCK_64X64) {
    assert(tp_orig < *tp);
    assert(best_rate < INT_MAX);
    assert(best_dist < INT_MAX);
  } else {
    assert(tp_orig == *tp);
  }
}

static void encode_sb_row(VP9_COMP *cpi, const TileInfo *const tile,
                          int mi_row, TOKENEXTRA **tp) {
  VP9_COMMON *const cm = &cpi->common;
  int mi_col;

  // Initialize the left context for the new SB row
  vpx_memset(&cpi->left_context, 0, sizeof(cpi->left_context));
  vpx_memset(cpi->left_seg_context, 0, sizeof(cpi->left_seg_context));

  // Code each SB in the row
  for (mi_col = tile->mi_col_start; mi_col < tile->mi_col_end;
       mi_col += MI_BLOCK_SIZE) {
    int dummy_rate;
    int64_t dummy_dist;

    BLOCK_SIZE i;
    MACROBLOCK *x = &cpi->mb;
    for (i = BLOCK_4X4; i < BLOCK_8X8; ++i) {
      const int num_4x4_w = num_4x4_blocks_wide_lookup[i];
      const int num_4x4_h = num_4x4_blocks_high_lookup[i];
      const int num_4x4_blk = MAX(4, num_4x4_w * num_4x4_h);
      for (x->sb_index = 0; x->sb_index < 4; ++x->sb_index)
        for (x->mb_index = 0; x->mb_index < 4; ++x->mb_index)
          for (x->b_index = 0; x->b_index < 16 / num_4x4_blk; ++x->b_index)
            get_block_context(x, i)->pred_interp_filter = SWITCHABLE;
    }

    vp9_zero(cpi->mb.pred_mv);

    if (cpi->sf.use_lastframe_partitioning ||
        cpi->sf.use_one_partition_size_always ) {
      const int idx_str = cm->mode_info_stride * mi_row + mi_col;
      MODE_INFO **mi_8x8 = cm->mi_grid_visible + idx_str;
      MODE_INFO **prev_mi_8x8 = cm->prev_mi_grid_visible + idx_str;

      cpi->mb.source_variance = UINT_MAX;
      if (cpi->sf.use_one_partition_size_always) {
        set_offsets(cpi, tile, mi_row, mi_col, BLOCK_64X64);
        set_partitioning(cpi, tile, mi_8x8, mi_row, mi_col);
        rd_use_partition(cpi, tile, mi_8x8, tp, mi_row, mi_col, BLOCK_64X64,
                         &dummy_rate, &dummy_dist, 1);
      } else {
        if ((cm->current_video_frame
            % cpi->sf.last_partitioning_redo_frequency) == 0
            || cm->prev_mi == 0
            || cm->show_frame == 0
            || cm->frame_type == KEY_FRAME
            || cpi->rc.is_src_frame_alt_ref
            || ((cpi->sf.use_lastframe_partitioning ==
                 LAST_FRAME_PARTITION_LOW_MOTION) &&
                 sb_has_motion(cm, prev_mi_8x8))) {
          // If required set upper and lower partition size limits
          if (cpi->sf.auto_min_max_partition_size) {
            set_offsets(cpi, tile, mi_row, mi_col, BLOCK_64X64);
            rd_auto_partition_range(cpi, tile, mi_row, mi_col,
                                    &cpi->sf.min_partition_size,
                                    &cpi->sf.max_partition_size);
          }
          rd_pick_partition(cpi, tile, tp, mi_row, mi_col, BLOCK_64X64,
                            &dummy_rate, &dummy_dist, 1, INT64_MAX);
        } else {
          copy_partitioning(cm, mi_8x8, prev_mi_8x8);
          rd_use_partition(cpi, tile, mi_8x8, tp, mi_row, mi_col, BLOCK_64X64,
                           &dummy_rate, &dummy_dist, 1);
        }
      }
    } else {
      // If required set upper and lower partition size limits
      if (cpi->sf.auto_min_max_partition_size) {
        set_offsets(cpi, tile, mi_row, mi_col, BLOCK_64X64);
        rd_auto_partition_range(cpi, tile, mi_row, mi_col,
                                &cpi->sf.min_partition_size,
                                &cpi->sf.max_partition_size);
      }
      rd_pick_partition(cpi, tile, tp, mi_row, mi_col, BLOCK_64X64,
                        &dummy_rate, &dummy_dist, 1, INT64_MAX);
    }
  }
}

static void init_encode_frame_mb_context(VP9_COMP *cpi) {
  MACROBLOCK *const x = &cpi->mb;
  VP9_COMMON *const cm = &cpi->common;
  MACROBLOCKD *const xd = &x->e_mbd;
  const int aligned_mi_cols = mi_cols_aligned_to_sb(cm->mi_cols);

  x->act_zbin_adj = 0;
  cpi->seg0_idx = 0;

  xd->mode_info_stride = cm->mode_info_stride;

  // Copy data over into macro block data structures.
  vp9_setup_src_planes(x, cpi->Source, 0, 0);

  // TODO(jkoleszar): are these initializations required?
  setup_pre_planes(xd, 0, get_ref_frame_buffer(cpi, LAST_FRAME), 0, 0, NULL);
  setup_dst_planes(xd, get_frame_new_buffer(cm), 0, 0);

  vp9_setup_block_planes(&x->e_mbd, cm->subsampling_x, cm->subsampling_y);

  xd->mi_8x8[0]->mbmi.mode = DC_PRED;
  xd->mi_8x8[0]->mbmi.uv_mode = DC_PRED;

  vp9_zero(cm->counts.y_mode);
  vp9_zero(cm->counts.uv_mode);
  vp9_zero(cm->counts.inter_mode);
  vp9_zero(cm->counts.partition);
  vp9_zero(cm->counts.intra_inter);
  vp9_zero(cm->counts.comp_inter);
  vp9_zero(cm->counts.single_ref);
  vp9_zero(cm->counts.comp_ref);
  vp9_zero(cm->counts.tx);
  vp9_zero(cm->counts.skip);

  // Note: this memset assumes above_context[0], [1] and [2]
  // are allocated as part of the same buffer.
  vpx_memset(cpi->above_context[0], 0,
             sizeof(*cpi->above_context[0]) *
             2 * aligned_mi_cols * MAX_MB_PLANE);
  vpx_memset(cpi->above_seg_context, 0,
             sizeof(*cpi->above_seg_context) * aligned_mi_cols);
}

static void switch_lossless_mode(VP9_COMP *cpi, int lossless) {
  if (lossless) {
    // printf("Switching to lossless\n");
    cpi->mb.fwd_txm4x4 = vp9_fwht4x4;
    cpi->mb.e_mbd.itxm_add = vp9_iwht4x4_add;
    cpi->mb.optimize = 0;
    cpi->common.lf.filter_level = 0;
    cpi->zbin_mode_boost_enabled = 0;
    cpi->common.tx_mode = ONLY_4X4;
  } else {
    // printf("Not lossless\n");
    cpi->mb.fwd_txm4x4 = vp9_fdct4x4;
    cpi->mb.e_mbd.itxm_add = vp9_idct4x4_add;
  }
}

static void switch_tx_mode(VP9_COMP *cpi) {
  if (cpi->sf.tx_size_search_method == USE_LARGESTALL &&
      cpi->common.tx_mode >= ALLOW_32X32)
    cpi->common.tx_mode = ALLOW_32X32;
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

static int get_skip_flag(MODE_INFO **mi_8x8, int mis, int ymbs, int xmbs) {
  int x, y;

  for (y = 0; y < ymbs; y++) {
    for (x = 0; x < xmbs; x++) {
      if (!mi_8x8[y * mis + x]->mbmi.skip)
        return 0;
    }
  }

  return 1;
}

static void set_txfm_flag(MODE_INFO **mi_8x8, int mis, int ymbs, int xmbs,
                          TX_SIZE tx_size) {
  int x, y;

  for (y = 0; y < ymbs; y++) {
    for (x = 0; x < xmbs; x++)
      mi_8x8[y * mis + x]->mbmi.tx_size = tx_size;
  }
}

static void reset_skip_txfm_size_b(VP9_COMMON *cm, MODE_INFO **mi_8x8,
                                   int mis, TX_SIZE max_tx_size, int bw, int bh,
                                   int mi_row, int mi_col, BLOCK_SIZE bsize) {
  if (mi_row >= cm->mi_rows || mi_col >= cm->mi_cols) {
    return;
  } else {
    MB_MODE_INFO * const mbmi = &mi_8x8[0]->mbmi;
    if (mbmi->tx_size > max_tx_size) {
      const int ymbs = MIN(bh, cm->mi_rows - mi_row);
      const int xmbs = MIN(bw, cm->mi_cols - mi_col);

      assert(vp9_segfeature_active(&cm->seg, mbmi->segment_id, SEG_LVL_SKIP) ||
             get_skip_flag(mi_8x8, mis, ymbs, xmbs));
      set_txfm_flag(mi_8x8, mis, ymbs, xmbs, max_tx_size);
    }
  }
}

static void reset_skip_txfm_size_sb(VP9_COMMON *cm, MODE_INFO **mi_8x8,
                                    TX_SIZE max_tx_size, int mi_row, int mi_col,
                                    BLOCK_SIZE bsize) {
  const int mis = cm->mode_info_stride;
  int bw, bh;
  const int bs = num_8x8_blocks_wide_lookup[bsize], hbs = bs / 2;

  if (mi_row >= cm->mi_rows || mi_col >= cm->mi_cols)
    return;

  bw = num_8x8_blocks_wide_lookup[mi_8x8[0]->mbmi.sb_type];
  bh = num_8x8_blocks_high_lookup[mi_8x8[0]->mbmi.sb_type];

  if (bw == bs && bh == bs) {
    reset_skip_txfm_size_b(cm, mi_8x8, mis, max_tx_size, bs, bs, mi_row,
                           mi_col, bsize);
  } else if (bw == bs && bh < bs) {
    reset_skip_txfm_size_b(cm, mi_8x8, mis, max_tx_size, bs, hbs, mi_row,
                           mi_col, bsize);
    reset_skip_txfm_size_b(cm, mi_8x8 + hbs * mis, mis, max_tx_size, bs, hbs,
                           mi_row + hbs, mi_col, bsize);
  } else if (bw < bs && bh == bs) {
    reset_skip_txfm_size_b(cm, mi_8x8, mis, max_tx_size, hbs, bs, mi_row,
                           mi_col, bsize);
    reset_skip_txfm_size_b(cm, mi_8x8 + hbs, mis, max_tx_size, hbs, bs, mi_row,
                           mi_col + hbs, bsize);

  } else {
    const BLOCK_SIZE subsize = subsize_lookup[PARTITION_SPLIT][bsize];
    int n;

    assert(bw < bs && bh < bs);

    for (n = 0; n < 4; n++) {
      const int mi_dc = hbs * (n & 1);
      const int mi_dr = hbs * (n >> 1);

      reset_skip_txfm_size_sb(cm, &mi_8x8[mi_dr * mis + mi_dc], max_tx_size,
                              mi_row + mi_dr, mi_col + mi_dc, subsize);
    }
  }
}

static void reset_skip_txfm_size(VP9_COMMON *cm, TX_SIZE txfm_max) {
  int mi_row, mi_col;
  const int mis = cm->mode_info_stride;
  MODE_INFO **mi_8x8, **mi_ptr = cm->mi_grid_visible;

  for (mi_row = 0; mi_row < cm->mi_rows; mi_row += 8, mi_ptr += 8 * mis) {
    mi_8x8 = mi_ptr;
    for (mi_col = 0; mi_col < cm->mi_cols; mi_col += 8, mi_8x8 += 8) {
      reset_skip_txfm_size_sb(cm, mi_8x8, txfm_max, mi_row, mi_col,
                              BLOCK_64X64);
    }
  }
}

static MV_REFERENCE_FRAME get_frame_type(VP9_COMP *cpi) {
  if (frame_is_intra_only(&cpi->common))
    return INTRA_FRAME;
  else if (cpi->rc.is_src_frame_alt_ref && cpi->refresh_golden_frame)
    return ALTREF_FRAME;
  else if (cpi->refresh_golden_frame || cpi->refresh_alt_ref_frame)
    return LAST_FRAME;
  else
    return GOLDEN_FRAME;
}

static void select_tx_mode(VP9_COMP *cpi) {
  if (cpi->oxcf.lossless) {
    cpi->common.tx_mode = ONLY_4X4;
  } else if (cpi->common.current_video_frame == 0) {
    cpi->common.tx_mode = TX_MODE_SELECT;
  } else {
    if (cpi->sf.tx_size_search_method == USE_LARGESTALL) {
      cpi->common.tx_mode = ALLOW_32X32;
    } else if (cpi->sf.tx_size_search_method == USE_FULL_RD) {
      int frame_type = get_frame_type(cpi);
      cpi->common.tx_mode =
          cpi->rd_tx_select_threshes[frame_type][ALLOW_32X32]
          > cpi->rd_tx_select_threshes[frame_type][TX_MODE_SELECT] ?
          ALLOW_32X32 : TX_MODE_SELECT;
    } else {
      unsigned int total = 0;
      int i;
      for (i = 0; i < TX_SIZES; ++i)
        total += cpi->tx_stepdown_count[i];
      if (total) {
        double fraction = (double)cpi->tx_stepdown_count[0] / total;
        cpi->common.tx_mode = fraction > 0.90 ? ALLOW_32X32 : TX_MODE_SELECT;
        // printf("fraction = %f\n", fraction);
      }  // else keep unchanged
    }
  }
}

// Start RTC Exploration
typedef enum {
  BOTH_ZERO = 0,
  ZERO_PLUS_PREDICTED = 1,
  BOTH_PREDICTED = 2,
  NEW_PLUS_NON_INTRA = 3,
  BOTH_NEW = 4,
  INTRA_PLUS_NON_INTRA = 5,
  BOTH_INTRA = 6,
  INVALID_CASE = 9
} motion_vector_context;

static void set_mode_info(MB_MODE_INFO *mbmi, BLOCK_SIZE bsize,
                          MB_PREDICTION_MODE mode, int mi_row, int mi_col) {
  mbmi->interp_filter = EIGHTTAP;
  mbmi->mode = mode;
  mbmi->mv[0].as_int = 0;
  mbmi->mv[1].as_int = 0;
  if (mode < NEARESTMV) {
    mbmi->ref_frame[0] = INTRA_FRAME;
  } else {
    mbmi->ref_frame[0] = LAST_FRAME;
  }

  mbmi->ref_frame[1] = INTRA_FRAME;
  mbmi->tx_size = max_txsize_lookup[bsize];
  mbmi->uv_mode = mode;
  mbmi->skip = 0;
  mbmi->sb_type = bsize;
  mbmi->segment_id = 0;
}

static INLINE int get_block_row(int b32i, int b16i, int b8i) {
  return ((b32i >> 1) << 2) + ((b16i >> 1) << 1) + (b8i >> 1);
}

static INLINE int get_block_col(int b32i, int b16i, int b8i) {
  return ((b32i & 1) << 2) + ((b16i & 1) << 1) + (b8i & 1);
}

static void rtc_use_partition(VP9_COMP *cpi,
                             const TileInfo *const tile,
                             MODE_INFO **mi_8x8,
                             TOKENEXTRA **tp, int mi_row, int mi_col,
                             BLOCK_SIZE bsize, int *rate, int64_t *dist,
                             int do_recon) {
  VP9_COMMON *const cm = &cpi->common;
  MACROBLOCK *const x = &cpi->mb;
  MACROBLOCKD *const xd = &cpi->mb.e_mbd;
  const int mis = cm->mode_info_stride;
  int mi_width = num_8x8_blocks_wide_lookup[cpi->sf.always_this_block_size];
  int mi_height = num_8x8_blocks_high_lookup[cpi->sf.always_this_block_size];
  int i, j;
  int chosen_rate = INT_MAX;
  int64_t chosen_dist = INT_MAX;
  MB_PREDICTION_MODE mode = DC_PRED;
  int row8x8_remaining = tile->mi_row_end - mi_row;
  int col8x8_remaining = tile->mi_col_end - mi_col;
  int b32i;
  for (b32i = 0; b32i < 4; b32i++) {
    int b16i;
    for (b16i = 0; b16i < 4; b16i++) {
      int b8i;
      int block_row = get_block_row(b32i, b16i, 0);
      int block_col = get_block_col(b32i, b16i, 0);
      int index = block_row * mis + block_col;
      int rate;
      int64_t dist;

      // Find a partition size that fits
      bsize = find_partition_size(cpi->sf.always_this_block_size,
                                  (row8x8_remaining - block_row),
                                  (col8x8_remaining - block_col),
                                  &mi_height, &mi_width);
      mi_8x8[index] = mi_8x8[0] + index;

      set_mi_row_col(xd, tile, mi_row + block_row, mi_height,
                     mi_col + block_col, mi_width, cm->mi_rows, cm->mi_cols);

      xd->mi_8x8 = mi_8x8 + index;

      if (cm->frame_type != KEY_FRAME) {
        set_offsets(cpi, tile, mi_row + block_row, mi_col + block_col, bsize);

        vp9_pick_inter_mode(cpi, x, tile,
                            mi_row + block_row, mi_col + block_col,
                            &rate, &dist, bsize);
      } else {
        set_mode_info(&mi_8x8[index]->mbmi, bsize, mode,
                      mi_row + block_row, mi_col + block_col);
      }

      for (j = 0; j < mi_height; j++)
        for (i = 0; i < mi_width; i++)
          if ((xd->mb_to_right_edge >> (3 + MI_SIZE_LOG2)) + mi_width > i
            && (xd->mb_to_bottom_edge >> (3 + MI_SIZE_LOG2)) + mi_height > j) {
            mi_8x8[index+ i + j * mis] = mi_8x8[index];
          }

      for (b8i = 0; b8i < 4; b8i++) {
      }
    }
  }
  encode_sb_rt(cpi, tile, tp, mi_row, mi_col, 1, BLOCK_64X64);

  *rate = chosen_rate;
  *dist = chosen_dist;
}

static void encode_rtc_sb_row(VP9_COMP *cpi, const TileInfo *const tile,
                              int mi_row, TOKENEXTRA **tp) {
  VP9_COMMON * const cm = &cpi->common;
  int mi_col;

  // Initialize the left context for the new SB row
  vpx_memset(&cpi->left_context, 0, sizeof(cpi->left_context));
  vpx_memset(cpi->left_seg_context, 0, sizeof(cpi->left_seg_context));

  // Code each SB in the row
  for (mi_col = tile->mi_col_start; mi_col < tile->mi_col_end;
       mi_col += MI_BLOCK_SIZE) {
    int dummy_rate;
    int64_t dummy_dist;

    const int idx_str = cm->mode_info_stride * mi_row + mi_col;
    MODE_INFO **mi_8x8 = cm->mi_grid_visible + idx_str;
    cpi->mb.source_variance = UINT_MAX;

    set_partitioning(cpi, tile, mi_8x8, mi_row, mi_col);
    rtc_use_partition(cpi, tile, mi_8x8, tp, mi_row, mi_col, BLOCK_64X64,
                     &dummy_rate, &dummy_dist, 1);
  }
}
// end RTC play code

static void encode_frame_internal(VP9_COMP *cpi) {
  int mi_row;
  MACROBLOCK *const x = &cpi->mb;
  VP9_COMMON *const cm = &cpi->common;
  MACROBLOCKD *const xd = &x->e_mbd;

//  fprintf(stderr, "encode_frame_internal frame %d (%d) type %d\n",
//           cpi->common.current_video_frame, cpi->common.show_frame,
//           cm->frame_type);

  vp9_zero(cm->counts.switchable_interp);
  vp9_zero(cpi->tx_stepdown_count);

  xd->mi_8x8 = cm->mi_grid_visible;
  // required for vp9_frame_init_quantizer
  xd->mi_8x8[0] = cm->mi;

  xd->last_mi = cm->prev_mi;

  vp9_zero(cm->counts.mv);
  vp9_zero(cpi->coef_counts);
  vp9_zero(cm->counts.eob_branch);

  cpi->mb.e_mbd.lossless = cm->base_qindex == 0 && cm->y_dc_delta_q == 0
      && cm->uv_dc_delta_q == 0 && cm->uv_ac_delta_q == 0;
  switch_lossless_mode(cpi, cpi->mb.e_mbd.lossless);

  vp9_frame_init_quantizer(cpi);

  vp9_initialize_rd_consts(cpi);
  vp9_initialize_me_consts(cpi, cm->base_qindex);
  switch_tx_mode(cpi);

  if (cpi->oxcf.tuning == VP8_TUNE_SSIM) {
    // Initialize encode frame context.
    init_encode_frame_mb_context(cpi);

    // Build a frame level activity map
    build_activity_map(cpi);
  }

  // Re-initialize encode frame context.
  init_encode_frame_mb_context(cpi);

  vp9_zero(cpi->rd_comp_pred_diff);
  vp9_zero(cpi->rd_filter_diff);
  vp9_zero(cpi->rd_tx_select_diff);
  vp9_zero(cpi->rd_tx_select_threshes);

  set_prev_mi(cm);

  {
    struct vpx_usec_timer emr_timer;
    vpx_usec_timer_start(&emr_timer);

    {
      // Take tiles into account and give start/end MB
      int tile_col, tile_row;
      TOKENEXTRA *tp = cpi->tok;
      const int tile_cols = 1 << cm->log2_tile_cols;
      const int tile_rows = 1 << cm->log2_tile_rows;

      for (tile_row = 0; tile_row < tile_rows; tile_row++) {
        for (tile_col = 0; tile_col < tile_cols; tile_col++) {
          TileInfo tile;
          TOKENEXTRA *tp_old = tp;

          // For each row of SBs in the frame
          vp9_tile_init(&tile, cm, tile_row, tile_col);
          for (mi_row = tile.mi_row_start;
               mi_row < tile.mi_row_end; mi_row += 8) {
            if (cpi->sf.use_pick_mode)
              encode_rtc_sb_row(cpi, &tile, mi_row, &tp);
            else
              encode_sb_row(cpi, &tile, mi_row, &tp);
          }
          cpi->tok_count[tile_row][tile_col] = (unsigned int)(tp - tp_old);
          assert(tp - cpi->tok <= get_token_alloc(cm->mb_rows, cm->mb_cols));
        }
      }
    }

    vpx_usec_timer_mark(&emr_timer);
    cpi->time_encode_sb_row += vpx_usec_timer_elapsed(&emr_timer);
  }

  if (cpi->sf.skip_encode_sb) {
    int j;
    unsigned int intra_count = 0, inter_count = 0;
    for (j = 0; j < INTRA_INTER_CONTEXTS; ++j) {
      intra_count += cm->counts.intra_inter[j][0];
      inter_count += cm->counts.intra_inter[j][1];
    }
    cpi->sf.skip_encode_frame = ((intra_count << 2) < inter_count);
    cpi->sf.skip_encode_frame &= (cm->frame_type != KEY_FRAME);
    cpi->sf.skip_encode_frame &= cm->show_frame;
  } else {
    cpi->sf.skip_encode_frame = 0;
  }

#if 0
  // Keep record of the total distortion this time around for future use
  cpi->last_frame_distortion = cpi->frame_distortion;
#endif
}

void vp9_encode_frame(VP9_COMP *cpi) {
  VP9_COMMON *const cm = &cpi->common;

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
    REFERENCE_MODE reference_mode;
    /*
     * This code does a single RD pass over the whole frame assuming
     * either compound, single or hybrid prediction as per whatever has
     * worked best for that type of frame in the past.
     * It also predicts whether another coding mode would have worked
     * better that this coding mode. If that is the case, it remembers
     * that for subsequent frames.
     * It does the same analysis for transform size selection also.
     */
    const MV_REFERENCE_FRAME frame_type = get_frame_type(cpi);
    const int64_t *mode_thresh = cpi->rd_prediction_type_threshes[frame_type];
    const int64_t *filter_thresh = cpi->rd_filter_threshes[frame_type];

    /* prediction (compound, single or hybrid) mode selection */
    if (frame_type == 3 || !cm->allow_comp_inter_inter)
      reference_mode = SINGLE_REFERENCE;
    else if (mode_thresh[COMPOUND_REFERENCE] > mode_thresh[SINGLE_REFERENCE] &&
             mode_thresh[COMPOUND_REFERENCE] >
                 mode_thresh[REFERENCE_MODE_SELECT] &&
             check_dual_ref_flags(cpi) &&
             cpi->static_mb_pct == 100)
      reference_mode = COMPOUND_REFERENCE;
    else if (mode_thresh[SINGLE_REFERENCE] > mode_thresh[REFERENCE_MODE_SELECT])
      reference_mode = SINGLE_REFERENCE;
    else
      reference_mode = REFERENCE_MODE_SELECT;

    if (cm->interp_filter == SWITCHABLE) {
      if (frame_type != ALTREF_FRAME &&
          filter_thresh[EIGHTTAP_SMOOTH] > filter_thresh[EIGHTTAP] &&
          filter_thresh[EIGHTTAP_SMOOTH] > filter_thresh[EIGHTTAP_SHARP] &&
          filter_thresh[EIGHTTAP_SMOOTH] > filter_thresh[SWITCHABLE - 1]) {
        cm->interp_filter = EIGHTTAP_SMOOTH;
      } else if (filter_thresh[EIGHTTAP_SHARP] > filter_thresh[EIGHTTAP] &&
          filter_thresh[EIGHTTAP_SHARP] > filter_thresh[SWITCHABLE - 1]) {
        cm->interp_filter = EIGHTTAP_SHARP;
      } else if (filter_thresh[EIGHTTAP] > filter_thresh[SWITCHABLE - 1]) {
        cm->interp_filter = EIGHTTAP;
      }
    }

    cpi->mb.e_mbd.lossless = cpi->oxcf.lossless;

    /* transform size selection (4x4, 8x8, 16x16 or select-per-mb) */
    select_tx_mode(cpi);
    cm->reference_mode = reference_mode;

    encode_frame_internal(cpi);

    for (i = 0; i < REFERENCE_MODES; ++i) {
      const int diff = (int) (cpi->rd_comp_pred_diff[i] / cm->MBs);
      cpi->rd_prediction_type_threshes[frame_type][i] += diff;
      cpi->rd_prediction_type_threshes[frame_type][i] >>= 1;
    }

    for (i = 0; i < SWITCHABLE_FILTER_CONTEXTS; i++) {
      const int64_t diff = cpi->rd_filter_diff[i] / cm->MBs;
      cpi->rd_filter_threshes[frame_type][i] =
          (cpi->rd_filter_threshes[frame_type][i] + diff) / 2;
    }

    for (i = 0; i < TX_MODES; ++i) {
      int64_t pd = cpi->rd_tx_select_diff[i];
      int diff;
      if (i == TX_MODE_SELECT)
        pd -= RDCOST(cpi->mb.rdmult, cpi->mb.rddiv, 2048 * (TX_SIZES - 1), 0);
      diff = (int) (pd / cm->MBs);
      cpi->rd_tx_select_threshes[frame_type][i] += diff;
      cpi->rd_tx_select_threshes[frame_type][i] /= 2;
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
        reset_skip_txfm_size(cm, TX_8X8);
      } else if (count8x8_8x8p == 0 && count16x16_16x16p == 0 &&
                 count8x8_lp == 0 && count16x16_lp == 0 && count32x32 == 0) {
        cm->tx_mode = ONLY_4X4;
        reset_skip_txfm_size(cm, TX_4X4);
      } else if (count8x8_lp == 0 && count16x16_lp == 0 && count4x4 == 0) {
        cm->tx_mode = ALLOW_32X32;
      } else if (count32x32 == 0 && count8x8_lp == 0 && count4x4 == 0) {
        cm->tx_mode = ALLOW_16X16;
        reset_skip_txfm_size(cm, TX_16X16);
      }
    }
  } else {
    // Force the usage of the BILINEAR interp_filter.
    cm->interp_filter = BILINEAR;
    encode_frame_internal(cpi);
  }
}

static void sum_intra_stats(FRAME_COUNTS *counts, const MODE_INFO *mi) {
  const MB_PREDICTION_MODE y_mode = mi->mbmi.mode;
  const MB_PREDICTION_MODE uv_mode = mi->mbmi.uv_mode;
  const BLOCK_SIZE bsize = mi->mbmi.sb_type;

  ++counts->uv_mode[y_mode][uv_mode];

  if (bsize < BLOCK_8X8) {
    int idx, idy;
    const int num_4x4_blocks_wide = num_4x4_blocks_wide_lookup[bsize];
    const int num_4x4_blocks_high = num_4x4_blocks_high_lookup[bsize];
    for (idy = 0; idy < 2; idy += num_4x4_blocks_high)
      for (idx = 0; idx < 2; idx += num_4x4_blocks_wide)
        ++counts->y_mode[0][mi->bmi[idy * 2 + idx].as_mode];
  } else {
    ++counts->y_mode[size_group_lookup[bsize]][y_mode];
  }
}

// Experimental stub function to create a per MB zbin adjustment based on
// some previously calculated measure of MB activity.
static void adjust_act_zbin(VP9_COMP *cpi, MACROBLOCK *x) {
#if USE_ACT_INDEX
  x->act_zbin_adj = *(x->mb_activity_ptr);
#else
  int64_t a;
  int64_t b;
  int64_t act = *(x->mb_activity_ptr);

  // Apply the masking to the RD multiplier.
  a = act + 4 * cpi->activity_avg;
  b = 4 * act + cpi->activity_avg;

  if (act > cpi->activity_avg)
    x->act_zbin_adj = (int) (((int64_t) b + (a >> 1)) / a) - 1;
  else
    x->act_zbin_adj = 1 - (int) (((int64_t) a + (b >> 1)) / b);
#endif
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
                              int mi_row, int mi_col, BLOCK_SIZE bsize) {
  VP9_COMMON *const cm = &cpi->common;
  MACROBLOCK *const x = &cpi->mb;
  MACROBLOCKD *const xd = &x->e_mbd;
  MODE_INFO **mi_8x8 = xd->mi_8x8;
  MODE_INFO *mi = mi_8x8[0];
  MB_MODE_INFO *mbmi = &mi->mbmi;
  PICK_MODE_CONTEXT *ctx = get_block_context(x, bsize);
  unsigned int segment_id = mbmi->segment_id;
  const int mis = cm->mode_info_stride;
  const int mi_width = num_8x8_blocks_wide_lookup[bsize];
  const int mi_height = num_8x8_blocks_high_lookup[bsize];
  x->skip_recode = !x->select_txfm_size && mbmi->sb_type >= BLOCK_8X8 &&
                   (cpi->oxcf.aq_mode != COMPLEXITY_AQ) &&
                   !cpi->sf.use_pick_mode;
  x->skip_optimize = ctx->is_coded;
  ctx->is_coded = 1;
  x->use_lp32x32fdct = cpi->sf.use_lp32x32fdct;
  x->skip_encode = (!output_enabled && cpi->sf.skip_encode_frame &&
                    x->q_index < QIDX_SKIP_THRESH);
  if (x->skip_encode)
    return;

  if (cm->frame_type == KEY_FRAME) {
    if (cpi->oxcf.tuning == VP8_TUNE_SSIM) {
      adjust_act_zbin(cpi, x);
      vp9_update_zbin_extra(cpi, x);
    }
  } else {
    set_ref_ptrs(cm, xd, mbmi->ref_frame[0], mbmi->ref_frame[1]);
    xd->interp_kernel = vp9_get_interp_kernel(mbmi->interp_filter);

    if (cpi->oxcf.tuning == VP8_TUNE_SSIM) {
      // Adjust the zbin based on this MB rate.
      adjust_act_zbin(cpi, x);
    }

    // Experimental code. Special case for gf and arf zeromv modes.
    // Increase zbin size to suppress noise
    cpi->zbin_mode_boost = get_zbin_mode_boost(mbmi,
                                               cpi->zbin_mode_boost_enabled);
    vp9_update_zbin_extra(cpi, x);
  }

  if (!is_inter_block(mbmi)) {
    int plane;
    mbmi->skip = 1;
    for (plane = 0; plane < MAX_MB_PLANE; ++plane)
      vp9_encode_intra_block_plane(x, MAX(bsize, BLOCK_8X8), plane);
    if (output_enabled)
      sum_intra_stats(&cm->counts, mi);
  } else {
    int ref;
    const int is_compound = has_second_ref(mbmi);
    for (ref = 0; ref < 1 + is_compound; ++ref) {
      YV12_BUFFER_CONFIG *cfg = get_ref_frame_buffer(cpi,
                                                     mbmi->ref_frame[ref]);
      setup_pre_planes(xd, ref, cfg, mi_row, mi_col, &xd->block_refs[ref]->sf);
    }
    vp9_build_inter_predictors_sb(xd, mi_row, mi_col, MAX(bsize, BLOCK_8X8));
  }

  if (!is_inter_block(mbmi)) {
    vp9_tokenize_sb(cpi, t, !output_enabled, MAX(bsize, BLOCK_8X8));
  } else if (!x->skip) {
    mbmi->skip = 1;
    vp9_encode_sb(x, MAX(bsize, BLOCK_8X8));
    vp9_tokenize_sb(cpi, t, !output_enabled, MAX(bsize, BLOCK_8X8));
  } else {
    mbmi->skip = 1;
    if (output_enabled)
      cm->counts.skip[vp9_get_skip_context(xd)][1]++;
    reset_skip_context(xd, MAX(bsize, BLOCK_8X8));
  }

  if (output_enabled) {
    if (cm->tx_mode == TX_MODE_SELECT &&
        mbmi->sb_type >= BLOCK_8X8  &&
        !(is_inter_block(mbmi) &&
            (mbmi->skip ||
             vp9_segfeature_active(&cm->seg, segment_id, SEG_LVL_SKIP)))) {
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
