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

#ifndef AV1_COMMON_RECONINTRA_H_
#define AV1_COMMON_RECONINTRA_H_

#include "aom/aom_integer.h"
#include "av1/common/blockd.h"
#include "av1/common/onyxc_int.h"

#ifdef __cplusplus
extern "C" {
#endif

void av1_init_intra_predictors(void);
void av1_predict_intra_block_facade(const AV1_COMMON *cm, MACROBLOCKD *xd,
                                    int plane, int blk_col, int blk_row,
                                    TX_SIZE tx_size);
void av1_predict_intra_block(const AV1_COMMON *cm, const MACROBLOCKD *xd,
                             int bw, int bh, TX_SIZE tx_size,
                             PREDICTION_MODE mode, int angle_delta,
                             int use_palette,
                             FILTER_INTRA_MODE filter_intra_mode,
                             const uint8_t *ref, int ref_stride, uint8_t *dst,
                             int dst_stride, int aoff, int loff, int plane);

// Mapping of interintra to intra mode for use in the intra component
static const PREDICTION_MODE interintra_to_intra_mode[INTERINTRA_MODES] = {
  DC_PRED, V_PRED, H_PRED, SMOOTH_PRED
};

// Mapping of intra mode to the interintra mode
static const INTERINTRA_MODE intra_to_interintra_mode[INTRA_MODES] = {
  II_DC_PRED, II_V_PRED, II_H_PRED, II_V_PRED,      II_SMOOTH_PRED, II_V_PRED,
  II_H_PRED,  II_H_PRED, II_V_PRED, II_SMOOTH_PRED, II_SMOOTH_PRED
};

#define FILTER_INTRA_SCALE_BITS 4

static INLINE int av1_is_directional_mode(PREDICTION_MODE mode) {
  return mode >= V_PRED && mode <= D67_PRED;
}

static INLINE int av1_use_angle_delta(BLOCK_SIZE bsize) {
  return bsize >= BLOCK_8X8;
}

static INLINE int av1_allow_intrabc(const AV1_COMMON *const cm) {
  return frame_is_intra_only(cm) && cm->allow_screen_content_tools &&
         cm->allow_intrabc;
}

static INLINE int av1_filter_intra_allowed_bsize(const AV1_COMMON *const cm,
                                                 BLOCK_SIZE bs) {
  if (!cm->seq_params.enable_filter_intra || bs == BLOCK_INVALID) return 0;

  return block_size_wide[bs] <= 32 && block_size_high[bs] <= 32;
}

static INLINE int av1_filter_intra_allowed(const AV1_COMMON *const cm,
                                           const MB_MODE_INFO *mbmi) {
  return mbmi->mode == DC_PRED &&
         mbmi->palette_mode_info.palette_size[0] == 0 &&
         av1_filter_intra_allowed_bsize(cm, mbmi->sb_type);
}

extern const int8_t av1_filter_intra_taps[FILTER_INTRA_MODES][8][8];

// Get the shift (up-scaled by 256) in X w.r.t a unit change in Y.
// If angle > 0 && angle < 90, dx = -((int)(256 / t));
// If angle > 90 && angle < 180, dx = (int)(256 / t);
// If angle > 180 && angle < 270, dx = 1;
static INLINE int av1_get_dx(int angle) {
  if (angle > 0 && angle < 90) {
    return dr_intra_derivative[angle];
  } else if (angle > 90 && angle < 180) {
    return dr_intra_derivative[180 - angle];
  } else {
    // In this case, we are not really going to use dx. We may return any value.
    return 1;
  }
}

// Get the shift (up-scaled by 256) in Y w.r.t a unit change in X.
// If angle > 0 && angle < 90, dy = 1;
// If angle > 90 && angle < 180, dy = (int)(256 * t);
// If angle > 180 && angle < 270, dy = -((int)(256 * t));
static INLINE int av1_get_dy(int angle) {
  if (angle > 90 && angle < 180) {
    return dr_intra_derivative[angle - 90];
  } else if (angle > 180 && angle < 270) {
    return dr_intra_derivative[270 - angle];
  } else {
    // In this case, we are not really going to use dy. We may return any value.
    return 1;
  }
}
#ifdef __cplusplus
}  // extern "C"
#endif
#endif  // AV1_COMMON_RECONINTRA_H_
