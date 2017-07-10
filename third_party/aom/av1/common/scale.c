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

#include "./aom_dsp_rtcd.h"
#include "av1/common/filter.h"
#include "av1/common/scale.h"
#include "aom_dsp/aom_filter.h"

// Note: Expect val to be in q4 precision
static INLINE int scaled_x(int val, const struct scale_factors *sf) {
  const int off =
      (sf->x_scale_fp - (1 << REF_SCALE_SHIFT)) * (1 << (SUBPEL_BITS - 1));
  const int64_t tval = (int64_t)val * sf->x_scale_fp + off;
  return (int)ROUND_POWER_OF_TWO_SIGNED_64(tval,
                                           REF_SCALE_SHIFT - SCALE_EXTRA_BITS);
}

// Note: Expect val to be in q4 precision
static INLINE int scaled_y(int val, const struct scale_factors *sf) {
  const int off =
      (sf->y_scale_fp - (1 << REF_SCALE_SHIFT)) * (1 << (SUBPEL_BITS - 1));
  const int64_t tval = (int64_t)val * sf->y_scale_fp + off;
  return (int)ROUND_POWER_OF_TWO_SIGNED_64(tval,
                                           REF_SCALE_SHIFT - SCALE_EXTRA_BITS);
}

// Note: Expect val to be in q4 precision
static int unscaled_value(int val, const struct scale_factors *sf) {
  (void)sf;
  return val << SCALE_EXTRA_BITS;
}

static int get_fixed_point_scale_factor(int other_size, int this_size) {
  // Calculate scaling factor once for each reference frame
  // and use fixed point scaling factors in decoding and encoding routines.
  // Hardware implementations can calculate scale factor in device driver
  // and use multiplication and shifting on hardware instead of division.
  return ((other_size << REF_SCALE_SHIFT) + this_size / 2) / this_size;
}

static int get_coarse_point_scale_factor(int other_size, int this_size) {
  // Calculate scaling factor once for each reference frame
  // and use fixed point scaling factors in decoding and encoding routines.
  // Hardware implementations can calculate scale factor in device driver
  // and use multiplication and shifting on hardware instead of division.
  return ((other_size << SCALE_SUBPEL_BITS) + this_size / 2) / this_size;
}

// Note: x and y are integer precision, mvq4 is q4 precision.
MV32 av1_scale_mv(const MV *mvq4, int x, int y,
                  const struct scale_factors *sf) {
  const int x_off_q4 = scaled_x(x << SUBPEL_BITS, sf);
  const int y_off_q4 = scaled_y(y << SUBPEL_BITS, sf);
  const MV32 res = { scaled_y((y << SUBPEL_BITS) + mvq4->row, sf) - y_off_q4,
                     scaled_x((x << SUBPEL_BITS) + mvq4->col, sf) - x_off_q4 };
  return res;
}

#if CONFIG_HIGHBITDEPTH
void av1_setup_scale_factors_for_frame(struct scale_factors *sf, int other_w,
                                       int other_h, int this_w, int this_h,
                                       int use_highbd) {
#else
void av1_setup_scale_factors_for_frame(struct scale_factors *sf, int other_w,
                                       int other_h, int this_w, int this_h) {
#endif
  if (!valid_ref_frame_size(other_w, other_h, this_w, this_h)) {
    sf->x_scale_fp = REF_INVALID_SCALE;
    sf->y_scale_fp = REF_INVALID_SCALE;
    return;
  }

  sf->x_scale_fp = get_fixed_point_scale_factor(other_w, this_w);
  sf->y_scale_fp = get_fixed_point_scale_factor(other_h, this_h);

  sf->x_step_q4 = get_coarse_point_scale_factor(other_w, this_w);
  sf->y_step_q4 = get_coarse_point_scale_factor(other_h, this_h);

  if (av1_is_scaled(sf)) {
    sf->scale_value_x = scaled_x;
    sf->scale_value_y = scaled_y;
  } else {
    sf->scale_value_x = unscaled_value;
    sf->scale_value_y = unscaled_value;
  }

  // TODO(agrange): Investigate the best choice of functions to use here
  // for EIGHTTAP_SMOOTH. Since it is not interpolating, need to choose what
  // to do at full-pel offsets. The current selection, where the filter is
  // applied in one direction only, and not at all for 0,0, seems to give the
  // best quality, but it may be worth trying an additional mode that does
  // do the filtering on full-pel.
  if (sf->x_step_q4 == SCALE_SUBPEL_SHIFTS) {
    if (sf->y_step_q4 == SCALE_SUBPEL_SHIFTS) {
      // No scaling in either direction.
      sf->predict[0][0][0] = aom_convolve_copy;
      sf->predict[0][0][1] = aom_convolve_avg;
      sf->predict[0][1][0] = aom_convolve8_vert;
      sf->predict[0][1][1] = aom_convolve8_avg_vert;
      sf->predict[1][0][0] = aom_convolve8_horiz;
      sf->predict[1][0][1] = aom_convolve8_avg_horiz;
    } else {
      // No scaling in x direction. Must always scale in the y direction.
      sf->predict[0][0][0] = aom_convolve8_vert;
      sf->predict[0][0][1] = aom_convolve8_avg_vert;
      sf->predict[0][1][0] = aom_convolve8_vert;
      sf->predict[0][1][1] = aom_convolve8_avg_vert;
      sf->predict[1][0][0] = aom_convolve8;
      sf->predict[1][0][1] = aom_convolve8_avg;
    }
  } else {
    if (sf->y_step_q4 == SCALE_SUBPEL_SHIFTS) {
      // No scaling in the y direction. Must always scale in the x direction.
      sf->predict[0][0][0] = aom_convolve8_horiz;
      sf->predict[0][0][1] = aom_convolve8_avg_horiz;
      sf->predict[0][1][0] = aom_convolve8;
      sf->predict[0][1][1] = aom_convolve8_avg;
      sf->predict[1][0][0] = aom_convolve8_horiz;
      sf->predict[1][0][1] = aom_convolve8_avg_horiz;
    } else {
      // Must always scale in both directions.
      sf->predict[0][0][0] = aom_convolve8;
      sf->predict[0][0][1] = aom_convolve8_avg;
      sf->predict[0][1][0] = aom_convolve8;
      sf->predict[0][1][1] = aom_convolve8_avg;
      sf->predict[1][0][0] = aom_convolve8;
      sf->predict[1][0][1] = aom_convolve8_avg;
    }
  }
  // 2D subpel motion always gets filtered in both directions
  sf->predict[1][1][0] = aom_convolve8;
  sf->predict[1][1][1] = aom_convolve8_avg;

#if CONFIG_HIGHBITDEPTH
  if (use_highbd) {
    if (sf->x_step_q4 == SCALE_SUBPEL_SHIFTS) {
      if (sf->y_step_q4 == SCALE_SUBPEL_SHIFTS) {
        // No scaling in either direction.
        sf->highbd_predict[0][0][0] = aom_highbd_convolve_copy;
        sf->highbd_predict[0][0][1] = aom_highbd_convolve_avg;
        sf->highbd_predict[0][1][0] = aom_highbd_convolve8_vert;
        sf->highbd_predict[0][1][1] = aom_highbd_convolve8_avg_vert;
        sf->highbd_predict[1][0][0] = aom_highbd_convolve8_horiz;
        sf->highbd_predict[1][0][1] = aom_highbd_convolve8_avg_horiz;
      } else {
        // No scaling in x direction. Must always scale in the y direction.
        sf->highbd_predict[0][0][0] = aom_highbd_convolve8_vert;
        sf->highbd_predict[0][0][1] = aom_highbd_convolve8_avg_vert;
        sf->highbd_predict[0][1][0] = aom_highbd_convolve8_vert;
        sf->highbd_predict[0][1][1] = aom_highbd_convolve8_avg_vert;
        sf->highbd_predict[1][0][0] = aom_highbd_convolve8;
        sf->highbd_predict[1][0][1] = aom_highbd_convolve8_avg;
      }
    } else {
      if (sf->y_step_q4 == SCALE_SUBPEL_SHIFTS) {
        // No scaling in the y direction. Must always scale in the x direction.
        sf->highbd_predict[0][0][0] = aom_highbd_convolve8_horiz;
        sf->highbd_predict[0][0][1] = aom_highbd_convolve8_avg_horiz;
        sf->highbd_predict[0][1][0] = aom_highbd_convolve8;
        sf->highbd_predict[0][1][1] = aom_highbd_convolve8_avg;
        sf->highbd_predict[1][0][0] = aom_highbd_convolve8_horiz;
        sf->highbd_predict[1][0][1] = aom_highbd_convolve8_avg_horiz;
      } else {
        // Must always scale in both directions.
        sf->highbd_predict[0][0][0] = aom_highbd_convolve8;
        sf->highbd_predict[0][0][1] = aom_highbd_convolve8_avg;
        sf->highbd_predict[0][1][0] = aom_highbd_convolve8;
        sf->highbd_predict[0][1][1] = aom_highbd_convolve8_avg;
        sf->highbd_predict[1][0][0] = aom_highbd_convolve8;
        sf->highbd_predict[1][0][1] = aom_highbd_convolve8_avg;
      }
    }
    // 2D subpel motion always gets filtered in both directions.
    sf->highbd_predict[1][1][0] = aom_highbd_convolve8;
    sf->highbd_predict[1][1][1] = aom_highbd_convolve8_avg;
  }
#endif  // CONFIG_HIGHBITDEPTH
}
