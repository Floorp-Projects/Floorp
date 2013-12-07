/*
 *  Copyright (c) 2013 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VP9_COMMON_VP9_SCALE_H_
#define VP9_COMMON_VP9_SCALE_H_

#include "vp9/common/vp9_mv.h"
#include "vp9/common/vp9_convolve.h"

#define REF_SCALE_SHIFT 14
#define REF_NO_SCALE (1 << REF_SCALE_SHIFT)
#define REF_INVALID_SCALE -1

struct scale_factors;
struct scale_factors_common {
  int x_scale_fp;   // horizontal fixed point scale factor
  int y_scale_fp;   // vertical fixed point scale factor
  int x_step_q4;
  int y_step_q4;

  int (*scale_value_x)(int val, const struct scale_factors_common *sfc);
  int (*scale_value_y)(int val, const struct scale_factors_common *sfc);
  void (*set_scaled_offsets)(struct scale_factors *scale, int row, int col);
  MV32 (*scale_mv)(const MV *mv, const struct scale_factors *scale);

  convolve_fn_t predict[2][2][2];  // horiz, vert, avg
};

struct scale_factors {
  int x_offset_q4;
  int y_offset_q4;
  const struct scale_factors_common *sfc;
};

void vp9_setup_scale_factors_for_frame(struct scale_factors *scale,
                                       struct scale_factors_common *scale_comm,
                                       int other_w, int other_h,
                                       int this_w, int this_h);

static int vp9_is_valid_scale(const struct scale_factors_common *sfc) {
  return sfc->x_scale_fp != REF_INVALID_SCALE &&
         sfc->y_scale_fp != REF_INVALID_SCALE;
}

static int vp9_is_scaled(const struct scale_factors_common *sfc) {
  return sfc->x_scale_fp != REF_NO_SCALE ||
         sfc->y_scale_fp != REF_NO_SCALE;
}

#endif  // VP9_COMMON_VP9_SCALE_H_
