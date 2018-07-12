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

#include "./aom_scale_rtcd.h"

#include "aom_dsp/aom_dsp_common.h"
#include "aom_dsp/psnr.h"
#include "aom_mem/aom_mem.h"
#include "aom_ports/mem.h"

#include "av1/common/av1_loopfilter.h"
#include "av1/common/onyxc_int.h"
#include "av1/common/quant_common.h"

#include "av1/encoder/av1_quantize.h"
#include "av1/encoder/encoder.h"
#include "av1/encoder/picklpf.h"

#if CONFIG_LPF_SB
#if CONFIG_HIGHBITDEPTH
static int compute_sb_y_sse_highbd(const YV12_BUFFER_CONFIG *src,
                                   const YV12_BUFFER_CONFIG *frame,
                                   AV1_COMMON *const cm, int mi_row,
                                   int mi_col) {
  int sse = 0;
  const int mi_row_start = AOMMAX(0, mi_row - FILT_BOUNDARY_MI_OFFSET);
  const int mi_col_start = AOMMAX(0, mi_col - FILT_BOUNDARY_MI_OFFSET);
  const int mi_row_range = mi_row - FILT_BOUNDARY_MI_OFFSET + MAX_MIB_SIZE;
  const int mi_col_range = mi_col - FILT_BOUNDARY_MI_OFFSET + MAX_MIB_SIZE;
  const int mi_row_end = AOMMIN(mi_row_range, cm->mi_rows);
  const int mi_col_end = AOMMIN(mi_col_range, cm->mi_cols);

  const int row = mi_row_start * MI_SIZE;
  const int col = mi_col_start * MI_SIZE;
  const uint16_t *src_y =
      CONVERT_TO_SHORTPTR(src->y_buffer) + row * src->y_stride + col;
  const uint16_t *frame_y =
      CONVERT_TO_SHORTPTR(frame->y_buffer) + row * frame->y_stride + col;
  const int row_end = (mi_row_end - mi_row_start) * MI_SIZE;
  const int col_end = (mi_col_end - mi_col_start) * MI_SIZE;

  int x, y;
  for (y = 0; y < row_end; ++y) {
    for (x = 0; x < col_end; ++x) {
      const int diff = src_y[x] - frame_y[x];
      sse += diff * diff;
    }
    src_y += src->y_stride;
    frame_y += frame->y_stride;
  }
  return sse;
}
#endif

static int compute_sb_y_sse(const YV12_BUFFER_CONFIG *src,
                            const YV12_BUFFER_CONFIG *frame,
                            AV1_COMMON *const cm, int mi_row, int mi_col) {
  int sse = 0;
  const int mi_row_start = AOMMAX(0, mi_row - FILT_BOUNDARY_MI_OFFSET);
  const int mi_col_start = AOMMAX(0, mi_col - FILT_BOUNDARY_MI_OFFSET);
  const int mi_row_range = mi_row - FILT_BOUNDARY_MI_OFFSET + MAX_MIB_SIZE;
  const int mi_col_range = mi_col - FILT_BOUNDARY_MI_OFFSET + MAX_MIB_SIZE;
  const int mi_row_end = AOMMIN(mi_row_range, cm->mi_rows);
  const int mi_col_end = AOMMIN(mi_col_range, cm->mi_cols);

  const int row = mi_row_start * MI_SIZE;
  const int col = mi_col_start * MI_SIZE;
  const uint8_t *src_y = src->y_buffer + row * src->y_stride + col;
  const uint8_t *frame_y = frame->y_buffer + row * frame->y_stride + col;
  const int row_end = (mi_row_end - mi_row_start) * MI_SIZE;
  const int col_end = (mi_col_end - mi_col_start) * MI_SIZE;

  int x, y;
  for (y = 0; y < row_end; ++y) {
    for (x = 0; x < col_end; ++x) {
      const int diff = src_y[x] - frame_y[x];
      sse += diff * diff;
    }
    src_y += src->y_stride;
    frame_y += frame->y_stride;
  }
  return sse;
}
#endif  // CONFIG_LPF_SB

#if !CONFIG_LPF_SB
static void yv12_copy_plane(const YV12_BUFFER_CONFIG *src_bc,
                            YV12_BUFFER_CONFIG *dst_bc, int plane) {
  switch (plane) {
    case 0: aom_yv12_copy_y(src_bc, dst_bc); break;
    case 1: aom_yv12_copy_u(src_bc, dst_bc); break;
    case 2: aom_yv12_copy_v(src_bc, dst_bc); break;
    default: assert(plane >= 0 && plane <= 2); break;
  }
}
#endif  // CONFIG_LPF_SB

int av1_get_max_filter_level(const AV1_COMP *cpi) {
  if (cpi->oxcf.pass == 2) {
    return cpi->twopass.section_intra_rating > 8 ? MAX_LOOP_FILTER * 3 / 4
                                                 : MAX_LOOP_FILTER;
  } else {
    return MAX_LOOP_FILTER;
  }
}

#if CONFIG_LPF_SB
// TODO(chengchen): reduce memory usage by copy superblock instead of frame
static int try_filter_superblock(const YV12_BUFFER_CONFIG *sd,
                                 AV1_COMP *const cpi, int filt_level,
                                 int partial_frame, int mi_row, int mi_col) {
  AV1_COMMON *const cm = &cpi->common;
  int filt_err;

#if CONFIG_VAR_TX || CONFIG_EXT_PARTITION || CONFIG_CB4X4
  av1_loop_filter_frame(cm->frame_to_show, cm, &cpi->td.mb.e_mbd, filt_level, 1,
                        partial_frame, mi_row, mi_col);
#else
  if (cpi->num_workers > 1)
    av1_loop_filter_frame_mt(cm->frame_to_show, cm, cpi->td.mb.e_mbd.plane,
                             filt_level, 1, partial_frame, cpi->workers,
                             cpi->num_workers, &cpi->lf_row_sync);
  else
    av1_loop_filter_frame(cm->frame_to_show, cm, &cpi->td.mb.e_mbd, filt_level,
                          1, partial_frame);
#endif

#if CONFIG_HIGHBITDEPTH
  if (cm->use_highbitdepth) {
    filt_err =
        compute_sb_y_sse_highbd(sd, cm->frame_to_show, cm, mi_row, mi_col);
  } else {
    filt_err = compute_sb_y_sse(sd, cm->frame_to_show, cm, mi_row, mi_col);
  }
#else
  filt_err = compute_sb_y_sse(sd, cm->frame_to_show, cm, mi_row, mi_col);
#endif  // CONFIG_HIGHBITDEPTH

  // TODO(chengchen): Copy the superblock only
  // Re-instate the unfiltered frame
  aom_yv12_copy_y(&cpi->last_frame_uf, cm->frame_to_show);

  return filt_err;
}

static int search_filter_level(const YV12_BUFFER_CONFIG *sd, AV1_COMP *cpi,
                               int partial_frame, double *best_cost_ret,
                               int mi_row, int mi_col, int last_lvl) {
  assert(partial_frame == 1);
  assert(last_lvl >= 0);

  const AV1_COMMON *const cm = &cpi->common;
  MACROBLOCK *x = &cpi->td.mb;

  int min_filter_level = AOMMAX(0, last_lvl - MAX_LPF_OFFSET);
  int max_filter_level =
      AOMMIN(av1_get_max_filter_level(cpi), last_lvl + MAX_LPF_OFFSET);

  // search a larger range for the start superblock
  if (mi_row == 0 && mi_col == 0) {
    min_filter_level = 0;
    max_filter_level = av1_get_max_filter_level(cpi);
  }

  // TODO(chengchen): Copy for superblock only
  // Make a copy of the unfiltered / processed recon buffer
  aom_yv12_copy_y(cm->frame_to_show, &cpi->last_frame_uf);

  int estimate_err =
      try_filter_superblock(sd, cpi, last_lvl, partial_frame, mi_row, mi_col);

  int best_err = estimate_err;
  int filt_best = last_lvl;

  int i;
  for (i = min_filter_level; i <= max_filter_level; i += LPF_STEP) {
    if (i == last_lvl) continue;

    int filt_err =
        try_filter_superblock(sd, cpi, i, partial_frame, mi_row, mi_col);

    if (filt_err < best_err) {
      best_err = filt_err;
      filt_best = i;
    }
  }

  // If previous sb filter level has similar filtering performance as current
  // best filter level, use previous level such that we can only send one bit
  // to indicate current filter level is the same as the previous.
  int threshold = 400;

  // ratio = the filtering area / a superblock size
  int ratio = 1;
  if (mi_row + MAX_MIB_SIZE > cm->mi_rows) {
    ratio *= (cm->mi_rows - mi_row);
  } else {
    if (mi_row == 0) {
      ratio *= (MAX_MIB_SIZE - FILT_BOUNDARY_MI_OFFSET);
    } else {
      ratio *= MAX_MIB_SIZE;
    }
  }
  if (mi_col + MAX_MIB_SIZE > cm->mi_cols) {
    ratio *= (cm->mi_cols - mi_col);
  } else {
    if (mi_col == 0) {
      ratio *= (MAX_MIB_SIZE - FILT_BOUNDARY_MI_OFFSET);
    } else {
      ratio *= MAX_MIB_SIZE;
    }
  }
  threshold = threshold * ratio / (MAX_MIB_SIZE * MAX_MIB_SIZE);

  const int diff = abs(estimate_err - best_err);

  const int percent_thresh = (int)((double)estimate_err * 0.01);
  threshold = AOMMAX(threshold, percent_thresh);
  if (diff < threshold) {
    best_err = estimate_err;
    filt_best = last_lvl;
  }

  // Compute rdcost to determine whether to reuse previous filter lvl
  if (filt_best != last_lvl) {
  }

  if (best_cost_ret) *best_cost_ret = RDCOST_DBL(x->rdmult, 0, best_err);
  return filt_best;
}

#else  // CONFIG_LPF_SB
static int64_t try_filter_frame(const YV12_BUFFER_CONFIG *sd,
                                AV1_COMP *const cpi, int filt_level,
                                int partial_frame
#if CONFIG_LOOPFILTER_LEVEL
                                ,
                                int plane, int dir
#endif
                                ) {
  AV1_COMMON *const cm = &cpi->common;
  int64_t filt_err;

#if CONFIG_VAR_TX || CONFIG_EXT_PARTITION || CONFIG_CB4X4
#if CONFIG_LOOPFILTER_LEVEL
  assert(plane >= 0 && plane <= 2);
  int filter_level[2] = { filt_level, filt_level };
  if (plane == 0 && dir == 0) filter_level[1] = cm->lf.filter_level[1];
  if (plane == 0 && dir == 1) filter_level[0] = cm->lf.filter_level[0];

  av1_loop_filter_frame(cm->frame_to_show, cm, &cpi->td.mb.e_mbd,
                        filter_level[0], filter_level[1], plane, partial_frame);
#else
  av1_loop_filter_frame(cm->frame_to_show, cm, &cpi->td.mb.e_mbd, filt_level, 1,
                        partial_frame);
#endif  // CONFIG_LOOPFILTER_LEVEL
#else
  if (cpi->num_workers > 1)
    av1_loop_filter_frame_mt(cm->frame_to_show, cm, cpi->td.mb.e_mbd.plane,
                             filt_level, 1, partial_frame, cpi->workers,
                             cpi->num_workers, &cpi->lf_row_sync);
  else
    av1_loop_filter_frame(cm->frame_to_show, cm, &cpi->td.mb.e_mbd, filt_level,
                          1, partial_frame);
#endif

  int highbd = 0;
#if CONFIG_HIGHBITDEPTH
  highbd = cm->use_highbitdepth;
#endif  // CONFIG_HIGHBITDEPTH

#if CONFIG_LOOPFILTER_LEVEL
  filt_err = aom_get_sse_plane(sd, cm->frame_to_show, plane, highbd);

  // Re-instate the unfiltered frame
  yv12_copy_plane(&cpi->last_frame_uf, cm->frame_to_show, plane);
#else
  filt_err = aom_get_sse_plane(sd, cm->frame_to_show, 0, highbd);

  // Re-instate the unfiltered frame
  yv12_copy_plane(&cpi->last_frame_uf, cm->frame_to_show, 0);
#endif  // CONFIG_LOOPFILTER_LEVEL

  return filt_err;
}

static int search_filter_level(const YV12_BUFFER_CONFIG *sd, AV1_COMP *cpi,
                               int partial_frame, double *best_cost_ret
#if CONFIG_LOOPFILTER_LEVEL
                               ,
                               int plane, int dir
#endif
                               ) {
  const AV1_COMMON *const cm = &cpi->common;
  const struct loopfilter *const lf = &cm->lf;
  const int min_filter_level = 0;
  const int max_filter_level = av1_get_max_filter_level(cpi);
  int filt_direction = 0;
  int64_t best_err;
  int filt_best;
  MACROBLOCK *x = &cpi->td.mb;

// Start the search at the previous frame filter level unless it is now out of
// range.
#if CONFIG_LOOPFILTER_LEVEL
  int lvl;
  switch (plane) {
    case 0: lvl = (dir == 1) ? lf->filter_level[1] : lf->filter_level[0]; break;
    case 1: lvl = lf->filter_level_u; break;
    case 2: lvl = lf->filter_level_v; break;
    default: assert(plane >= 0 && plane <= 2); return 0;
  }
  int filt_mid = clamp(lvl, min_filter_level, max_filter_level);
#else
  int filt_mid = clamp(lf->filter_level, min_filter_level, max_filter_level);
#endif  // CONFIG_LOOPFILTER_LEVEL
  int filter_step = filt_mid < 16 ? 4 : filt_mid / 4;
  // Sum squared error at each filter level
  int64_t ss_err[MAX_LOOP_FILTER + 1];

  // Set each entry to -1
  memset(ss_err, 0xFF, sizeof(ss_err));

#if CONFIG_LOOPFILTER_LEVEL
  yv12_copy_plane(cm->frame_to_show, &cpi->last_frame_uf, plane);
#else
  //  Make a copy of the unfiltered / processed recon buffer
  aom_yv12_copy_y(cm->frame_to_show, &cpi->last_frame_uf);
#endif  // CONFIG_LOOPFILTER_LEVEL

#if CONFIG_LOOPFILTER_LEVEL
  best_err = try_filter_frame(sd, cpi, filt_mid, partial_frame, plane, dir);
#else
  best_err = try_filter_frame(sd, cpi, filt_mid, partial_frame);
#endif  // CONFIG_LOOPFILTER_LEVEL
  filt_best = filt_mid;
  ss_err[filt_mid] = best_err;

  while (filter_step > 0) {
    const int filt_high = AOMMIN(filt_mid + filter_step, max_filter_level);
    const int filt_low = AOMMAX(filt_mid - filter_step, min_filter_level);

    // Bias against raising loop filter in favor of lowering it.
    int64_t bias = (best_err >> (15 - (filt_mid / 8))) * filter_step;

    if ((cpi->oxcf.pass == 2) && (cpi->twopass.section_intra_rating < 20))
      bias = (bias * cpi->twopass.section_intra_rating) / 20;

    // yx, bias less for large block size
    if (cm->tx_mode != ONLY_4X4) bias >>= 1;

    if (filt_direction <= 0 && filt_low != filt_mid) {
      // Get Low filter error score
      if (ss_err[filt_low] < 0) {
#if CONFIG_LOOPFILTER_LEVEL
        ss_err[filt_low] =
            try_filter_frame(sd, cpi, filt_low, partial_frame, plane, dir);
#else
        ss_err[filt_low] = try_filter_frame(sd, cpi, filt_low, partial_frame);
#endif  // CONFIG_LOOPFILTER_LEVEL
      }
      // If value is close to the best so far then bias towards a lower loop
      // filter value.
      if (ss_err[filt_low] < (best_err + bias)) {
        // Was it actually better than the previous best?
        if (ss_err[filt_low] < best_err) {
          best_err = ss_err[filt_low];
        }
        filt_best = filt_low;
      }
    }

    // Now look at filt_high
    if (filt_direction >= 0 && filt_high != filt_mid) {
      if (ss_err[filt_high] < 0) {
#if CONFIG_LOOPFILTER_LEVEL
        ss_err[filt_high] =
            try_filter_frame(sd, cpi, filt_high, partial_frame, plane, dir);
#else
        ss_err[filt_high] = try_filter_frame(sd, cpi, filt_high, partial_frame);
#endif  // CONFIG_LOOPFILTER_LEVEL
      }
      // If value is significantly better than previous best, bias added against
      // raising filter value
      if (ss_err[filt_high] < (best_err - bias)) {
        best_err = ss_err[filt_high];
        filt_best = filt_high;
      }
    }

    // Half the step distance if the best filter value was the same as last time
    if (filt_best == filt_mid) {
      filter_step /= 2;
      filt_direction = 0;
    } else {
      filt_direction = (filt_best < filt_mid) ? -1 : 1;
      filt_mid = filt_best;
    }
  }

  // Update best error
  best_err = ss_err[filt_best];

  if (best_cost_ret) *best_cost_ret = RDCOST_DBL(x->rdmult, 0, best_err);
  return filt_best;
}
#endif  // CONFIG_LPF_SB

void av1_pick_filter_level(const YV12_BUFFER_CONFIG *sd, AV1_COMP *cpi,
                           LPF_PICK_METHOD method) {
  AV1_COMMON *const cm = &cpi->common;
  struct loopfilter *const lf = &cm->lf;

  lf->sharpness_level = cm->frame_type == KEY_FRAME ? 0 : cpi->oxcf.sharpness;

  if (method == LPF_PICK_MINIMAL_LPF) {
#if CONFIG_LOOPFILTER_LEVEL
    lf->filter_level[0] = 0;
    lf->filter_level[1] = 0;
#else
    lf->filter_level = 0;
#endif
  } else if (method >= LPF_PICK_FROM_Q) {
    const int min_filter_level = 0;
    const int max_filter_level = av1_get_max_filter_level(cpi);
    const int q = av1_ac_quant(cm->base_qindex, 0, cm->bit_depth);
// These values were determined by linear fitting the result of the
// searched level, filt_guess = q * 0.316206 + 3.87252
#if CONFIG_HIGHBITDEPTH
    int filt_guess;
    switch (cm->bit_depth) {
      case AOM_BITS_8:
        filt_guess = ROUND_POWER_OF_TWO(q * 20723 + 1015158, 18);
        break;
      case AOM_BITS_10:
        filt_guess = ROUND_POWER_OF_TWO(q * 20723 + 4060632, 20);
        break;
      case AOM_BITS_12:
        filt_guess = ROUND_POWER_OF_TWO(q * 20723 + 16242526, 22);
        break;
      default:
        assert(0 &&
               "bit_depth should be AOM_BITS_8, AOM_BITS_10 "
               "or AOM_BITS_12");
        return;
    }
#else
    int filt_guess = ROUND_POWER_OF_TWO(q * 20723 + 1015158, 18);
#endif  // CONFIG_HIGHBITDEPTH
    if (cm->frame_type == KEY_FRAME) filt_guess -= 4;
#if CONFIG_LOOPFILTER_LEVEL
    lf->filter_level[0] = clamp(filt_guess, min_filter_level, max_filter_level);
    lf->filter_level[1] = clamp(filt_guess, min_filter_level, max_filter_level);
#else
    lf->filter_level = clamp(filt_guess, min_filter_level, max_filter_level);
#endif
  } else {
#if CONFIG_LPF_SB
    int mi_row, mi_col;
    // TODO(chengchen): init last_lvl using previous frame's info?
    int last_lvl = 0;
    // TODO(chengchen): if the frame size makes the last superblock very small,
    // consider merge it to the previous superblock to save bits.
    // Example, if frame size 1080x720, then in the last row of superblock,
    // there're (FILT_BOUNDAR_OFFSET + 16) pixels.
    for (mi_row = 0; mi_row < cm->mi_rows; mi_row += MAX_MIB_SIZE) {
      for (mi_col = 0; mi_col < cm->mi_cols; mi_col += MAX_MIB_SIZE) {
        int lvl =
            search_filter_level(sd, cpi, 1, NULL, mi_row, mi_col, last_lvl);

        av1_loop_filter_sb_level_init(cm, mi_row, mi_col, lvl);

        // For the superblock at row start, its previous filter level should be
        // the one above it, not the one at the end of last row
        if (mi_col + MAX_MIB_SIZE >= cm->mi_cols) {
          last_lvl = cm->mi_grid_visible[mi_row * cm->mi_stride]->mbmi.filt_lvl;
        } else {
          last_lvl = lvl;
        }
      }
    }
#else  // CONFIG_LPF_SB
#if CONFIG_LOOPFILTER_LEVEL
    lf->filter_level[0] = lf->filter_level[1] = search_filter_level(
        sd, cpi, method == LPF_PICK_FROM_SUBIMAGE, NULL, 0, 2);
    lf->filter_level[0] = search_filter_level(
        sd, cpi, method == LPF_PICK_FROM_SUBIMAGE, NULL, 0, 0);
    lf->filter_level[1] = search_filter_level(
        sd, cpi, method == LPF_PICK_FROM_SUBIMAGE, NULL, 0, 1);

    lf->filter_level_u = search_filter_level(
        sd, cpi, method == LPF_PICK_FROM_SUBIMAGE, NULL, 1, 0);
    lf->filter_level_v = search_filter_level(
        sd, cpi, method == LPF_PICK_FROM_SUBIMAGE, NULL, 2, 0);
#else
    lf->filter_level =
        search_filter_level(sd, cpi, method == LPF_PICK_FROM_SUBIMAGE, NULL);
#endif  // CONFIG_LOOPFILTER_LEVEL
#endif  // CONFIG_LPF_SB
  }
}
