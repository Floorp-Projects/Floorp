/*
 *  Copyright (c) 2013 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "./vp9_rtcd.h"
#include "vp9/common/vp9_filter.h"
#include "vp9/common/vp9_scale.h"

static INLINE int scaled_x(int val, const struct scale_factors_common *sfc) {
  return val * sfc->x_scale_fp >> REF_SCALE_SHIFT;
}

static INLINE int scaled_y(int val, const struct scale_factors_common *sfc) {
  return val * sfc->y_scale_fp >> REF_SCALE_SHIFT;
}

static int unscaled_value(int val, const struct scale_factors_common *sfc) {
  (void) sfc;
  return val;
}

static MV32 scaled_mv(const MV *mv, const struct scale_factors *scale) {
  const MV32 res = {
    scaled_y(mv->row, scale->sfc) + scale->y_offset_q4,
    scaled_x(mv->col, scale->sfc) + scale->x_offset_q4
  };
  return res;
}

static MV32 unscaled_mv(const MV *mv, const struct scale_factors *scale) {
  const MV32 res = {
    mv->row,
    mv->col
  };
  return res;
}

static void set_offsets_with_scaling(struct scale_factors *scale,
                                     int row, int col) {
  scale->x_offset_q4 = scaled_x(col << SUBPEL_BITS, scale->sfc) & SUBPEL_MASK;
  scale->y_offset_q4 = scaled_y(row << SUBPEL_BITS, scale->sfc) & SUBPEL_MASK;
}

static void set_offsets_without_scaling(struct scale_factors *scale,
                                        int row, int col) {
  scale->x_offset_q4 = 0;
  scale->y_offset_q4 = 0;
}

static int get_fixed_point_scale_factor(int other_size, int this_size) {
  // Calculate scaling factor once for each reference frame
  // and use fixed point scaling factors in decoding and encoding routines.
  // Hardware implementations can calculate scale factor in device driver
  // and use multiplication and shifting on hardware instead of division.
  return (other_size << REF_SCALE_SHIFT) / this_size;
}

static int check_scale_factors(int other_w, int other_h,
                               int this_w, int this_h) {
  return 2 * this_w >= other_w &&
         2 * this_h >= other_h &&
         this_w <= 16 * other_w &&
         this_h <= 16 * other_h;
}

void vp9_setup_scale_factors_for_frame(struct scale_factors *scale,
                                       struct scale_factors_common *scale_comm,
                                       int other_w, int other_h,
                                       int this_w, int this_h) {
  if (!check_scale_factors(other_w, other_h, this_w, this_h)) {
    scale_comm->x_scale_fp = REF_INVALID_SCALE;
    scale_comm->y_scale_fp = REF_INVALID_SCALE;
    return;
  }

  scale_comm->x_scale_fp = get_fixed_point_scale_factor(other_w, this_w);
  scale_comm->y_scale_fp = get_fixed_point_scale_factor(other_h, this_h);
  scale_comm->x_step_q4 = scaled_x(16, scale_comm);
  scale_comm->y_step_q4 = scaled_y(16, scale_comm);

  if (vp9_is_scaled(scale_comm)) {
    scale_comm->scale_value_x = scaled_x;
    scale_comm->scale_value_y = scaled_y;
    scale_comm->set_scaled_offsets = set_offsets_with_scaling;
    scale_comm->scale_mv = scaled_mv;
  } else {
    scale_comm->scale_value_x = unscaled_value;
    scale_comm->scale_value_y = unscaled_value;
    scale_comm->set_scaled_offsets = set_offsets_without_scaling;
    scale_comm->scale_mv = unscaled_mv;
  }

  // TODO(agrange): Investigate the best choice of functions to use here
  // for EIGHTTAP_SMOOTH. Since it is not interpolating, need to choose what
  // to do at full-pel offsets. The current selection, where the filter is
  // applied in one direction only, and not at all for 0,0, seems to give the
  // best quality, but it may be worth trying an additional mode that does
  // do the filtering on full-pel.
  if (scale_comm->x_step_q4 == 16) {
    if (scale_comm->y_step_q4 == 16) {
      // No scaling in either direction.
      scale_comm->predict[0][0][0] = vp9_convolve_copy;
      scale_comm->predict[0][0][1] = vp9_convolve_avg;
      scale_comm->predict[0][1][0] = vp9_convolve8_vert;
      scale_comm->predict[0][1][1] = vp9_convolve8_avg_vert;
      scale_comm->predict[1][0][0] = vp9_convolve8_horiz;
      scale_comm->predict[1][0][1] = vp9_convolve8_avg_horiz;
    } else {
      // No scaling in x direction. Must always scale in the y direction.
      scale_comm->predict[0][0][0] = vp9_convolve8_vert;
      scale_comm->predict[0][0][1] = vp9_convolve8_avg_vert;
      scale_comm->predict[0][1][0] = vp9_convolve8_vert;
      scale_comm->predict[0][1][1] = vp9_convolve8_avg_vert;
      scale_comm->predict[1][0][0] = vp9_convolve8;
      scale_comm->predict[1][0][1] = vp9_convolve8_avg;
    }
  } else {
    if (scale_comm->y_step_q4 == 16) {
      // No scaling in the y direction. Must always scale in the x direction.
      scale_comm->predict[0][0][0] = vp9_convolve8_horiz;
      scale_comm->predict[0][0][1] = vp9_convolve8_avg_horiz;
      scale_comm->predict[0][1][0] = vp9_convolve8;
      scale_comm->predict[0][1][1] = vp9_convolve8_avg;
      scale_comm->predict[1][0][0] = vp9_convolve8_horiz;
      scale_comm->predict[1][0][1] = vp9_convolve8_avg_horiz;
    } else {
      // Must always scale in both directions.
      scale_comm->predict[0][0][0] = vp9_convolve8;
      scale_comm->predict[0][0][1] = vp9_convolve8_avg;
      scale_comm->predict[0][1][0] = vp9_convolve8;
      scale_comm->predict[0][1][1] = vp9_convolve8_avg;
      scale_comm->predict[1][0][0] = vp9_convolve8;
      scale_comm->predict[1][0][1] = vp9_convolve8_avg;
    }
  }
  // 2D subpel motion always gets filtered in both directions
  scale_comm->predict[1][1][0] = vp9_convolve8;
  scale_comm->predict[1][1][1] = vp9_convolve8_avg;

  scale->sfc = scale_comm;
  scale->x_offset_q4 = 0;  // calculated per block
  scale->y_offset_q4 = 0;  // calculated per block
}
