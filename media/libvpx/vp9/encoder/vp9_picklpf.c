/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <assert.h>
#include <limits.h>
#include "vp9/common/vp9_onyxc_int.h"
#include "vp9/encoder/vp9_onyx_int.h"
#include "vp9/encoder/vp9_picklpf.h"
#include "vp9/encoder/vp9_quantize.h"
#include "vpx_mem/vpx_mem.h"
#include "vpx_scale/vpx_scale.h"
#include "vp9/common/vp9_alloccommon.h"
#include "vp9/common/vp9_loopfilter.h"
#include "./vpx_scale_rtcd.h"

void vp9_yv12_copy_partial_frame_c(YV12_BUFFER_CONFIG *src_ybc,
                                   YV12_BUFFER_CONFIG *dst_ybc, int fraction) {
  const int height = src_ybc->y_height;
  const int stride = src_ybc->y_stride;
  const int offset = stride * ((height >> 5) * 16 - 8);
  const int lines_to_copy = MAX(height >> (fraction + 4), 1) << 4;

  assert(src_ybc->y_stride == dst_ybc->y_stride);
  vpx_memcpy(dst_ybc->y_buffer + offset, src_ybc->y_buffer + offset,
             stride * (lines_to_copy + 16));
}

static int calc_partial_ssl_err(YV12_BUFFER_CONFIG *source,
                                YV12_BUFFER_CONFIG *dest, int Fraction) {
  int i, j;
  int Total = 0;
  int srcoffset, dstoffset;
  uint8_t *src = source->y_buffer;
  uint8_t *dst = dest->y_buffer;

  int linestocopy = (source->y_height >> (Fraction + 4));

  if (linestocopy < 1)
    linestocopy = 1;

  linestocopy <<= 4;


  srcoffset = source->y_stride   * (dest->y_height >> 5) * 16;
  dstoffset = dest->y_stride     * (dest->y_height >> 5) * 16;

  src += srcoffset;
  dst += dstoffset;

  // Loop through the raw Y plane and reconstruction data summing the square
  // differences.
  for (i = 0; i < linestocopy; i += 16) {
    for (j = 0; j < source->y_width; j += 16) {
      unsigned int sse;
      Total += vp9_mse16x16(src + j, source->y_stride, dst + j, dest->y_stride,
                            &sse);
    }

    src += 16 * source->y_stride;
    dst += 16 * dest->y_stride;
  }

  return Total;
}

// Enforce a minimum filter level based upon baseline Q
static int get_min_filter_level(VP9_COMP *cpi, int base_qindex) {
  int min_filter_level;
  min_filter_level = 0;

  return min_filter_level;
}

// Enforce a maximum filter level based upon baseline Q
static int get_max_filter_level(VP9_COMP *cpi, int base_qindex) {
  int max_filter_level = MAX_LOOP_FILTER;
  (void)base_qindex;

  if (cpi->twopass.section_intra_rating > 8)
    max_filter_level = MAX_LOOP_FILTER * 3 / 4;

  return max_filter_level;
}


// Stub function for now Alt LF not used
void vp9_set_alt_lf_level(VP9_COMP *cpi, int filt_val) {
}

void vp9_pick_filter_level(YV12_BUFFER_CONFIG *sd, VP9_COMP *cpi, int partial) {
  VP9_COMMON *const cm = &cpi->common;
  struct loopfilter *const lf = &cm->lf;

  int best_err = 0;
  int filt_err = 0;
  const int min_filter_level = get_min_filter_level(cpi, cm->base_qindex);
  const int max_filter_level = get_max_filter_level(cpi, cm->base_qindex);

  int filter_step;
  int filt_high = 0;
  // Start search at previous frame filter level
  int filt_mid = lf->filter_level;
  int filt_low = 0;
  int filt_best;
  int filt_direction = 0;

  int Bias = 0;  // Bias against raising loop filter in favor of lowering it.

  //  Make a copy of the unfiltered / processed recon buffer
  vpx_yv12_copy_y(cm->frame_to_show, &cpi->last_frame_uf);

  lf->sharpness_level = cm->frame_type == KEY_FRAME ? 0
                                                    : cpi->oxcf.Sharpness;

  // Start the search at the previous frame filter level unless it is now out of
  // range.
  filt_mid = clamp(lf->filter_level, min_filter_level, max_filter_level);

  // Define the initial step size
  filter_step = filt_mid < 16 ? 4 : filt_mid / 4;

  // Get baseline error score
  vp9_set_alt_lf_level(cpi, filt_mid);
  vp9_loop_filter_frame(cm, &cpi->mb.e_mbd, filt_mid, 1, partial);

  best_err = vp9_calc_ss_err(sd, cm->frame_to_show);
  filt_best = filt_mid;

  //  Re-instate the unfiltered frame
  vpx_yv12_copy_y(&cpi->last_frame_uf, cm->frame_to_show);

  while (filter_step > 0) {
    Bias = (best_err >> (15 - (filt_mid / 8))) * filter_step;

    if (cpi->twopass.section_intra_rating < 20)
      Bias = Bias * cpi->twopass.section_intra_rating / 20;

    // yx, bias less for large block size
    if (cpi->common.tx_mode != ONLY_4X4)
      Bias >>= 1;

    filt_high = ((filt_mid + filter_step) > max_filter_level)
                    ? max_filter_level
                    : (filt_mid + filter_step);
    filt_low = ((filt_mid - filter_step) < min_filter_level)
                   ? min_filter_level
                   : (filt_mid - filter_step);

    if ((filt_direction <= 0) && (filt_low != filt_mid)) {
      // Get Low filter error score
      vp9_set_alt_lf_level(cpi, filt_low);
      vp9_loop_filter_frame(cm, &cpi->mb.e_mbd, filt_low, 1, partial);

      filt_err = vp9_calc_ss_err(sd, cm->frame_to_show);

      //  Re-instate the unfiltered frame
      vpx_yv12_copy_y(&cpi->last_frame_uf, cm->frame_to_show);

      // If value is close to the best so far then bias towards a lower loop
      // filter value.
      if ((filt_err - Bias) < best_err) {
        // Was it actually better than the previous best?
        if (filt_err < best_err)
          best_err = filt_err;

        filt_best = filt_low;
      }
    }

    // Now look at filt_high
    if ((filt_direction >= 0) && (filt_high != filt_mid)) {
      vp9_set_alt_lf_level(cpi, filt_high);
      vp9_loop_filter_frame(cm, &cpi->mb.e_mbd, filt_high, 1, partial);

      filt_err = vp9_calc_ss_err(sd, cm->frame_to_show);

      //  Re-instate the unfiltered frame
      vpx_yv12_copy_y(&cpi->last_frame_uf, cm->frame_to_show);

      // Was it better than the previous best?
      if (filt_err < (best_err - Bias)) {
        best_err = filt_err;
        filt_best = filt_high;
      }
    }

    // Half the step distance if the best filter value was the same as last time
    if (filt_best == filt_mid) {
      filter_step = filter_step / 2;
      filt_direction = 0;
    } else {
      filt_direction = (filt_best < filt_mid) ? -1 : 1;
      filt_mid = filt_best;
    }
  }

  lf->filter_level = filt_best;
}
