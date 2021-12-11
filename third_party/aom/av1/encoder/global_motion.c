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

#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <math.h>
#include <assert.h>

#include "av1/encoder/global_motion.h"

#include "av1/common/warped_motion.h"

#include "av1/encoder/segmentation.h"
#include "av1/encoder/corner_detect.h"
#include "av1/encoder/corner_match.h"
#include "av1/encoder/ransac.h"

#define MAX_CORNERS 4096
#define MIN_INLIER_PROB 0.1

#define MIN_TRANS_THRESH (1 * GM_TRANS_DECODE_FACTOR)

// Border over which to compute the global motion
#define ERRORADV_BORDER 0

static const double erroradv_tr[] = { 0.65, 0.60, 0.55 };
static const double erroradv_prod_tr[] = { 20000, 18000, 16000 };

int is_enough_erroradvantage(double best_erroradvantage, int params_cost,
                             int erroradv_type) {
  assert(erroradv_type < GM_ERRORADV_TR_TYPES);
  return best_erroradvantage < erroradv_tr[erroradv_type] &&
         best_erroradvantage * params_cost < erroradv_prod_tr[erroradv_type];
}

static void convert_to_params(const double *params, int32_t *model) {
  int i;
  int alpha_present = 0;
  model[0] = (int32_t)floor(params[0] * (1 << GM_TRANS_PREC_BITS) + 0.5);
  model[1] = (int32_t)floor(params[1] * (1 << GM_TRANS_PREC_BITS) + 0.5);
  model[0] = (int32_t)clamp(model[0], GM_TRANS_MIN, GM_TRANS_MAX) *
             GM_TRANS_DECODE_FACTOR;
  model[1] = (int32_t)clamp(model[1], GM_TRANS_MIN, GM_TRANS_MAX) *
             GM_TRANS_DECODE_FACTOR;

  for (i = 2; i < 6; ++i) {
    const int diag_value = ((i == 2 || i == 5) ? (1 << GM_ALPHA_PREC_BITS) : 0);
    model[i] = (int32_t)floor(params[i] * (1 << GM_ALPHA_PREC_BITS) + 0.5);
    model[i] =
        (int32_t)clamp(model[i] - diag_value, GM_ALPHA_MIN, GM_ALPHA_MAX);
    alpha_present |= (model[i] != 0);
    model[i] = (model[i] + diag_value) * GM_ALPHA_DECODE_FACTOR;
  }
  for (; i < 8; ++i) {
    model[i] = (int32_t)floor(params[i] * (1 << GM_ROW3HOMO_PREC_BITS) + 0.5);
    model[i] = (int32_t)clamp(model[i], GM_ROW3HOMO_MIN, GM_ROW3HOMO_MAX) *
               GM_ROW3HOMO_DECODE_FACTOR;
    alpha_present |= (model[i] != 0);
  }

  if (!alpha_present) {
    if (abs(model[0]) < MIN_TRANS_THRESH && abs(model[1]) < MIN_TRANS_THRESH) {
      model[0] = 0;
      model[1] = 0;
    }
  }
}

void convert_model_to_params(const double *params, WarpedMotionParams *model) {
  convert_to_params(params, model->wmmat);
  model->wmtype = get_gmtype(model);
  model->invalid = 0;
}

// Adds some offset to a global motion parameter and handles
// all of the necessary precision shifts, clamping, and
// zero-centering.
static int32_t add_param_offset(int param_index, int32_t param_value,
                                int32_t offset) {
  const int scale_vals[3] = { GM_TRANS_PREC_DIFF, GM_ALPHA_PREC_DIFF,
                              GM_ROW3HOMO_PREC_DIFF };
  const int clamp_vals[3] = { GM_TRANS_MAX, GM_ALPHA_MAX, GM_ROW3HOMO_MAX };
  // type of param: 0 - translation, 1 - affine, 2 - homography
  const int param_type = (param_index < 2 ? 0 : (param_index < 6 ? 1 : 2));
  const int is_one_centered = (param_index == 2 || param_index == 5);

  // Make parameter zero-centered and offset the shift that was done to make
  // it compatible with the warped model
  param_value = (param_value - (is_one_centered << WARPEDMODEL_PREC_BITS)) >>
                scale_vals[param_type];
  // Add desired offset to the rescaled/zero-centered parameter
  param_value += offset;
  // Clamp the parameter so it does not overflow the number of bits allotted
  // to it in the bitstream
  param_value = (int32_t)clamp(param_value, -clamp_vals[param_type],
                               clamp_vals[param_type]);
  // Rescale the parameter to WARPEDMODEL_PRECISION_BITS so it is compatible
  // with the warped motion library
  param_value *= (1 << scale_vals[param_type]);

  // Undo the zero-centering step if necessary
  return param_value + (is_one_centered << WARPEDMODEL_PREC_BITS);
}

static void force_wmtype(WarpedMotionParams *wm, TransformationType wmtype) {
  switch (wmtype) {
    case IDENTITY:
      wm->wmmat[0] = 0;
      wm->wmmat[1] = 0;
      AOM_FALLTHROUGH_INTENDED;
    case TRANSLATION:
      wm->wmmat[2] = 1 << WARPEDMODEL_PREC_BITS;
      wm->wmmat[3] = 0;
      AOM_FALLTHROUGH_INTENDED;
    case ROTZOOM:
      wm->wmmat[4] = -wm->wmmat[3];
      wm->wmmat[5] = wm->wmmat[2];
      AOM_FALLTHROUGH_INTENDED;
    case AFFINE: wm->wmmat[6] = wm->wmmat[7] = 0; break;
    default: assert(0);
  }
  wm->wmtype = wmtype;
}

int64_t refine_integerized_param(WarpedMotionParams *wm,
                                 TransformationType wmtype, int use_hbd, int bd,
                                 uint8_t *ref, int r_width, int r_height,
                                 int r_stride, uint8_t *dst, int d_width,
                                 int d_height, int d_stride, int n_refinements,
                                 int64_t best_frame_error) {
  static const int max_trans_model_params[TRANS_TYPES] = { 0, 2, 4, 6 };
  const int border = ERRORADV_BORDER;
  int i = 0, p;
  int n_params = max_trans_model_params[wmtype];
  int32_t *param_mat = wm->wmmat;
  int64_t step_error, best_error;
  int32_t step;
  int32_t *param;
  int32_t curr_param;
  int32_t best_param;

  force_wmtype(wm, wmtype);
  best_error = av1_warp_error(wm, use_hbd, bd, ref, r_width, r_height, r_stride,
                              dst + border * d_stride + border, border, border,
                              d_width - 2 * border, d_height - 2 * border,
                              d_stride, 0, 0, best_frame_error);
  best_error = AOMMIN(best_error, best_frame_error);
  step = 1 << (n_refinements - 1);
  for (i = 0; i < n_refinements; i++, step >>= 1) {
    for (p = 0; p < n_params; ++p) {
      int step_dir = 0;
      // Skip searches for parameters that are forced to be 0
      param = param_mat + p;
      curr_param = *param;
      best_param = curr_param;
      // look to the left
      *param = add_param_offset(p, curr_param, -step);
      step_error =
          av1_warp_error(wm, use_hbd, bd, ref, r_width, r_height, r_stride,
                         dst + border * d_stride + border, border, border,
                         d_width - 2 * border, d_height - 2 * border, d_stride,
                         0, 0, best_error);
      if (step_error < best_error) {
        best_error = step_error;
        best_param = *param;
        step_dir = -1;
      }

      // look to the right
      *param = add_param_offset(p, curr_param, step);
      step_error =
          av1_warp_error(wm, use_hbd, bd, ref, r_width, r_height, r_stride,
                         dst + border * d_stride + border, border, border,
                         d_width - 2 * border, d_height - 2 * border, d_stride,
                         0, 0, best_error);
      if (step_error < best_error) {
        best_error = step_error;
        best_param = *param;
        step_dir = 1;
      }
      *param = best_param;

      // look to the direction chosen above repeatedly until error increases
      // for the biggest step size
      while (step_dir) {
        *param = add_param_offset(p, best_param, step * step_dir);
        step_error =
            av1_warp_error(wm, use_hbd, bd, ref, r_width, r_height, r_stride,
                           dst + border * d_stride + border, border, border,
                           d_width - 2 * border, d_height - 2 * border,
                           d_stride, 0, 0, best_error);
        if (step_error < best_error) {
          best_error = step_error;
          best_param = *param;
        } else {
          *param = best_param;
          step_dir = 0;
        }
      }
    }
  }
  force_wmtype(wm, wmtype);
  wm->wmtype = get_gmtype(wm);
  return best_error;
}

static INLINE RansacFunc get_ransac_type(TransformationType type) {
  switch (type) {
    case AFFINE: return ransac_affine;
    case ROTZOOM: return ransac_rotzoom;
    case TRANSLATION: return ransac_translation;
    default: assert(0); return NULL;
  }
}

static unsigned char *downconvert_frame(YV12_BUFFER_CONFIG *frm,
                                        int bit_depth) {
  int i, j;
  uint16_t *orig_buf = CONVERT_TO_SHORTPTR(frm->y_buffer);
  uint8_t *buf_8bit = frm->y_buffer_8bit;
  assert(buf_8bit);
  if (!frm->buf_8bit_valid) {
    for (i = 0; i < frm->y_height; ++i) {
      for (j = 0; j < frm->y_width; ++j) {
        buf_8bit[i * frm->y_stride + j] =
            orig_buf[i * frm->y_stride + j] >> (bit_depth - 8);
      }
    }
    frm->buf_8bit_valid = 1;
  }
  return buf_8bit;
}

int compute_global_motion_feature_based(TransformationType type,
                                        YV12_BUFFER_CONFIG *frm,
                                        YV12_BUFFER_CONFIG *ref, int bit_depth,
                                        int *num_inliers_by_motion,
                                        double *params_by_motion,
                                        int num_motions) {
  int i;
  int num_frm_corners, num_ref_corners;
  int num_correspondences;
  int *correspondences;
  int frm_corners[2 * MAX_CORNERS], ref_corners[2 * MAX_CORNERS];
  unsigned char *frm_buffer = frm->y_buffer;
  unsigned char *ref_buffer = ref->y_buffer;
  RansacFunc ransac = get_ransac_type(type);

  if (frm->flags & YV12_FLAG_HIGHBITDEPTH) {
    // The frame buffer is 16-bit, so we need to convert to 8 bits for the
    // following code. We cache the result until the frame is released.
    frm_buffer = downconvert_frame(frm, bit_depth);
  }
  if (ref->flags & YV12_FLAG_HIGHBITDEPTH) {
    ref_buffer = downconvert_frame(ref, bit_depth);
  }

  // compute interest points in images using FAST features
  num_frm_corners = fast_corner_detect(frm_buffer, frm->y_width, frm->y_height,
                                       frm->y_stride, frm_corners, MAX_CORNERS);
  num_ref_corners = fast_corner_detect(ref_buffer, ref->y_width, ref->y_height,
                                       ref->y_stride, ref_corners, MAX_CORNERS);

  // find correspondences between the two images
  correspondences =
      (int *)malloc(num_frm_corners * 4 * sizeof(*correspondences));
  num_correspondences = determine_correspondence(
      frm_buffer, (int *)frm_corners, num_frm_corners, ref_buffer,
      (int *)ref_corners, num_ref_corners, frm->y_width, frm->y_height,
      frm->y_stride, ref->y_stride, correspondences);

  ransac(correspondences, num_correspondences, num_inliers_by_motion,
         params_by_motion, num_motions);

  free(correspondences);

  // Set num_inliers = 0 for motions with too few inliers so they are ignored.
  for (i = 0; i < num_motions; ++i) {
    if (num_inliers_by_motion[i] < MIN_INLIER_PROB * num_correspondences) {
      num_inliers_by_motion[i] = 0;
    }
  }

  // Return true if any one of the motions has inliers.
  for (i = 0; i < num_motions; ++i) {
    if (num_inliers_by_motion[i] > 0) return 1;
  }
  return 0;
}
