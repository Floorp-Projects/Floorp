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

#include "aom_dsp/psnr.h"
#include "aom_dsp/aom_dsp_common.h"
#include "aom_mem/aom_mem.h"
#include "aom_ports/mem.h"

#include "av1/common/av1_loopfilter.h"
#include "av1/common/onyxc_int.h"
#include "av1/common/quant_common.h"

#include "av1/encoder/av1_quantize.h"
#include "av1/encoder/encoder.h"
#include "av1/encoder/picklpf.h"

int av1_get_max_filter_level(const AV1_COMP *cpi) {
  if (cpi->oxcf.pass == 2) {
    return cpi->twopass.section_intra_rating > 8 ? MAX_LOOP_FILTER * 3 / 4
                                                 : MAX_LOOP_FILTER;
  } else {
    return MAX_LOOP_FILTER;
  }
}

static int64_t try_filter_frame(const YV12_BUFFER_CONFIG *sd,
                                AV1_COMP *const cpi, int filt_level,
                                int partial_frame
#if CONFIG_UV_LVL
                                ,
                                int plane
#endif
                                ) {
  AV1_COMMON *const cm = &cpi->common;
  int64_t filt_err;

#if CONFIG_VAR_TX || CONFIG_EXT_PARTITION || CONFIG_CB4X4
#if CONFIG_UV_LVL
  av1_loop_filter_frame(cm->frame_to_show, cm, &cpi->td.mb.e_mbd, filt_level,
                        plane, partial_frame);
#else
  av1_loop_filter_frame(cm->frame_to_show, cm, &cpi->td.mb.e_mbd, filt_level, 1,
                        partial_frame);
#endif  // CONFIG_UV_LVL
#else
  if (cpi->num_workers > 1)
    av1_loop_filter_frame_mt(cm->frame_to_show, cm, cpi->td.mb.e_mbd.plane,
                             filt_level, 1, partial_frame, cpi->workers,
                             cpi->num_workers, &cpi->lf_row_sync);
  else
    av1_loop_filter_frame(cm->frame_to_show, cm, &cpi->td.mb.e_mbd, filt_level,
                          1, partial_frame);
#endif

#if CONFIG_UV_LVL
#if CONFIG_HIGHBITDEPTH
  if (cm->use_highbitdepth) {
    if (plane == 0)
      filt_err = aom_highbd_get_y_sse(sd, cm->frame_to_show);
    else if (plane == 1)
      filt_err = aom_highbd_get_u_sse(sd, cm->frame_to_show);
    else
      filt_err = aom_highbd_get_v_sse(sd, cm->frame_to_show);
  } else {
    if (plane == 0)
      filt_err = aom_get_y_sse(sd, cm->frame_to_show);
    else if (plane == 1)
      filt_err = aom_get_u_sse(sd, cm->frame_to_show);
    else
      filt_err = aom_get_v_sse(sd, cm->frame_to_show);
  }
#else
  if (plane == 0)
    filt_err = aom_get_y_sse(sd, cm->frame_to_show);
  else if (plane == 1)
    filt_err = aom_get_u_sse(sd, cm->frame_to_show);
  else
    filt_err = aom_get_v_sse(sd, cm->frame_to_show);
#endif  // CONFIG_HIGHBITDEPTH

  // Re-instate the unfiltered frame
  if (plane == 0)
    aom_yv12_copy_y(&cpi->last_frame_uf, cm->frame_to_show);
  else if (plane == 1)
    aom_yv12_copy_u(&cpi->last_frame_uf, cm->frame_to_show);
  else
    aom_yv12_copy_v(&cpi->last_frame_uf, cm->frame_to_show);
#else
#if CONFIG_HIGHBITDEPTH
  if (cm->use_highbitdepth) {
    filt_err = aom_highbd_get_y_sse(sd, cm->frame_to_show);
  } else {
    filt_err = aom_get_y_sse(sd, cm->frame_to_show);
  }
#else
  filt_err = aom_get_y_sse(sd, cm->frame_to_show);
#endif  // CONFIG_HIGHBITDEPTH

  // Re-instate the unfiltered frame
  aom_yv12_copy_y(&cpi->last_frame_uf, cm->frame_to_show);
#endif  // CONFIG_UV_LVL

  return filt_err;
}

int av1_search_filter_level(const YV12_BUFFER_CONFIG *sd, AV1_COMP *cpi,
                            int partial_frame, double *best_cost_ret
#if CONFIG_UV_LVL
                            ,
                            int plane
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
#if CONFIG_UV_LVL
  int lvl;
  switch (plane) {
    case 0: lvl = lf->filter_level; break;
    case 1: lvl = lf->filter_level_u; break;
    case 2: lvl = lf->filter_level_v; break;
    default: lvl = lf->filter_level; break;
  }
  int filt_mid = clamp(lvl, min_filter_level, max_filter_level);
#else
  int filt_mid = clamp(lf->filter_level, min_filter_level, max_filter_level);
#endif  // CONFIG_UV_LVL
  int filter_step = filt_mid < 16 ? 4 : filt_mid / 4;
  // Sum squared error at each filter level
  int64_t ss_err[MAX_LOOP_FILTER + 1];

  // Set each entry to -1
  memset(ss_err, 0xFF, sizeof(ss_err));

#if CONFIG_UV_LVL
  if (plane == 0)
    aom_yv12_copy_y(cm->frame_to_show, &cpi->last_frame_uf);
  else if (plane == 1)
    aom_yv12_copy_u(cm->frame_to_show, &cpi->last_frame_uf);
  else if (plane == 2)
    aom_yv12_copy_v(cm->frame_to_show, &cpi->last_frame_uf);
#else
  //  Make a copy of the unfiltered / processed recon buffer
  aom_yv12_copy_y(cm->frame_to_show, &cpi->last_frame_uf);
#endif  // CONFIG_UV_LVL

#if CONFIG_UV_LVL
  best_err = try_filter_frame(sd, cpi, filt_mid, partial_frame, plane);
#else
  best_err = try_filter_frame(sd, cpi, filt_mid, partial_frame);
#endif  // CONFIG_UV_LVL
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
#if CONFIG_UV_LVL
        ss_err[filt_low] =
            try_filter_frame(sd, cpi, filt_low, partial_frame, plane);
#else
        ss_err[filt_low] = try_filter_frame(sd, cpi, filt_low, partial_frame);
#endif  // CONFIG_UV_LVL
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
#if CONFIG_UV_LVL
        ss_err[filt_high] =
            try_filter_frame(sd, cpi, filt_high, partial_frame, plane);
#else
        ss_err[filt_high] = try_filter_frame(sd, cpi, filt_high, partial_frame);
#endif  // CONFIG_UV_LVL
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

void av1_pick_filter_level(const YV12_BUFFER_CONFIG *sd, AV1_COMP *cpi,
                           LPF_PICK_METHOD method) {
  AV1_COMMON *const cm = &cpi->common;
  struct loopfilter *const lf = &cm->lf;

  lf->sharpness_level = cm->frame_type == KEY_FRAME ? 0 : cpi->oxcf.sharpness;

  if (method == LPF_PICK_MINIMAL_LPF && lf->filter_level) {
    lf->filter_level = 0;
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
    lf->filter_level = clamp(filt_guess, min_filter_level, max_filter_level);
  } else {
#if CONFIG_UV_LVL
    lf->filter_level = av1_search_filter_level(
        sd, cpi, method == LPF_PICK_FROM_SUBIMAGE, NULL, 0);
    lf->filter_level_u = av1_search_filter_level(
        sd, cpi, method == LPF_PICK_FROM_SUBIMAGE, NULL, 1);
    lf->filter_level_v = av1_search_filter_level(
        sd, cpi, method == LPF_PICK_FROM_SUBIMAGE, NULL, 2);
#else
    lf->filter_level = av1_search_filter_level(
        sd, cpi, method == LPF_PICK_FROM_SUBIMAGE, NULL);
#endif  // CONFIG_UV_LVL
  }
}
