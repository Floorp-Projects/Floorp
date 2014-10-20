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

#include "vp9/encoder/vp9_encoder.h"
#include "vp9/encoder/vp9_speed_features.h"

enum {
  INTRA_ALL       = (1 << DC_PRED) |
                    (1 << V_PRED) | (1 << H_PRED) |
                    (1 << D45_PRED) | (1 << D135_PRED) |
                    (1 << D117_PRED) | (1 << D153_PRED) |
                    (1 << D207_PRED) | (1 << D63_PRED) |
                    (1 << TM_PRED),
  INTRA_DC        = (1 << DC_PRED),
  INTRA_DC_TM     = (1 << DC_PRED) | (1 << TM_PRED),
  INTRA_DC_H_V    = (1 << DC_PRED) | (1 << V_PRED) | (1 << H_PRED),
  INTRA_DC_TM_H_V = (1 << DC_PRED) | (1 << TM_PRED) | (1 << V_PRED) |
                    (1 << H_PRED)
};

enum {
  INTER_ALL = (1 << NEARESTMV) | (1 << NEARMV) | (1 << ZEROMV) | (1 << NEWMV),
  INTER_NEAREST = (1 << NEARESTMV),
  INTER_NEAREST_NEAR_NEW = (1 << NEARESTMV) | (1 << NEARMV) | (1 << NEWMV)
};

enum {
  DISABLE_ALL_INTER_SPLIT   = (1 << THR_COMP_GA) |
                              (1 << THR_COMP_LA) |
                              (1 << THR_ALTR) |
                              (1 << THR_GOLD) |
                              (1 << THR_LAST),

  DISABLE_ALL_SPLIT         = (1 << THR_INTRA) | DISABLE_ALL_INTER_SPLIT,

  DISABLE_COMPOUND_SPLIT    = (1 << THR_COMP_GA) | (1 << THR_COMP_LA),

  LAST_AND_INTRA_SPLIT_ONLY = (1 << THR_COMP_GA) |
                              (1 << THR_COMP_LA) |
                              (1 << THR_ALTR) |
                              (1 << THR_GOLD)
};

// Intra only frames, golden frames (except alt ref overlays) and
// alt ref frames tend to be coded at a higher than ambient quality
static int frame_is_boosted(const VP9_COMP *cpi) {
  return frame_is_intra_only(&cpi->common) ||
         cpi->refresh_alt_ref_frame ||
         (cpi->refresh_golden_frame && !cpi->rc.is_src_frame_alt_ref) ||
         vp9_is_upper_layer_key_frame(cpi);
}


static void set_good_speed_feature(VP9_COMP *cpi, VP9_COMMON *cm,
                                   SPEED_FEATURES *sf, int speed) {
  const int boosted = frame_is_boosted(cpi);

  sf->adaptive_rd_thresh = 1;
  sf->allow_skip_recode = 1;

  if (speed >= 1) {
    sf->use_square_partition_only = !frame_is_intra_only(cm);
    sf->less_rectangular_check  = 1;

    if (MIN(cm->width, cm->height) >= 720)
      sf->disable_split_mask = cm->show_frame ? DISABLE_ALL_SPLIT
                                              : DISABLE_ALL_INTER_SPLIT;
    else
      sf->disable_split_mask = DISABLE_COMPOUND_SPLIT;
    sf->use_rd_breakout = 1;
    sf->adaptive_motion_search = 1;
    sf->mv.auto_mv_step_size = 1;
    sf->adaptive_rd_thresh = 2;
    sf->mv.subpel_iters_per_step = 1;
    sf->mode_skip_start = 10;
    sf->adaptive_pred_interp_filter = 1;

    sf->recode_loop = ALLOW_RECODE_KFARFGF;
    sf->intra_y_mode_mask[TX_32X32] = INTRA_DC_H_V;
    sf->intra_uv_mode_mask[TX_32X32] = INTRA_DC_H_V;
    sf->intra_y_mode_mask[TX_16X16] = INTRA_DC_H_V;
    sf->intra_uv_mode_mask[TX_16X16] = INTRA_DC_H_V;

    sf->tx_size_search_breakout = 1;

    if (MIN(cm->width, cm->height) >= 720)
      sf->partition_search_breakout_dist_thr = (1 << 23);
    else
      sf->partition_search_breakout_dist_thr = (1 << 21);
    sf->partition_search_breakout_rate_thr = 500;
  }

  if (speed >= 2) {
    sf->tx_size_search_method = frame_is_boosted(cpi) ? USE_FULL_RD
                                                      : USE_LARGESTALL;

    if (MIN(cm->width, cm->height) >= 720) {
      sf->lf_motion_threshold = LOW_MOTION_THRESHOLD;
      sf->last_partitioning_redo_frequency = 3;
      sf->disable_split_mask = cm->show_frame ? DISABLE_ALL_SPLIT
                                              : DISABLE_ALL_INTER_SPLIT;
      sf->adaptive_pred_interp_filter = 0;
    } else {
      sf->disable_split_mask = LAST_AND_INTRA_SPLIT_ONLY;
      sf->last_partitioning_redo_frequency = 2;
      sf->lf_motion_threshold = NO_MOTION_THRESHOLD;
    }

    sf->reference_masking = 1;
    sf->mode_search_skip_flags = FLAG_SKIP_INTRA_DIRMISMATCH |
                                 FLAG_SKIP_INTRA_BESTINTER |
                                 FLAG_SKIP_COMP_BESTINTRA |
                                 FLAG_SKIP_INTRA_LOWVAR;
    sf->disable_filter_search_var_thresh = 100;
    sf->comp_inter_joint_search_thresh = BLOCK_SIZES;
    sf->auto_min_max_partition_size = CONSTRAIN_NEIGHBORING_MIN_MAX;
    sf->use_lastframe_partitioning = LAST_FRAME_PARTITION_LOW_MOTION;
    sf->adjust_partitioning_from_last_frame = 1;

    if (MIN(cm->width, cm->height) >= 720)
      sf->partition_search_breakout_dist_thr = (1 << 24);
    else
      sf->partition_search_breakout_dist_thr = (1 << 22);
    sf->partition_search_breakout_rate_thr = 700;
  }

  if (speed >= 3) {
    sf->tx_size_search_method = frame_is_intra_only(cm) ? USE_FULL_RD
                                                        : USE_LARGESTALL;
    if (MIN(cm->width, cm->height) >= 720) {
      sf->disable_split_mask = DISABLE_ALL_SPLIT;
    } else {
      sf->max_intra_bsize = BLOCK_32X32;
      sf->disable_split_mask = DISABLE_ALL_INTER_SPLIT;
    }
    sf->adaptive_pred_interp_filter = 0;
    sf->adaptive_mode_search = 1;
    sf->cb_partition_search = !boosted;
    sf->cb_pred_filter_search = 1;
    sf->alt_ref_search_fp = 1;
    sf->motion_field_mode_search = !boosted;
    sf->lf_motion_threshold = LOW_MOTION_THRESHOLD;
    sf->last_partitioning_redo_frequency = 2;
    sf->recode_loop = ALLOW_RECODE_KFMAXBW;
    sf->adaptive_rd_thresh = 3;
    sf->mode_skip_start = 6;
    sf->intra_y_mode_mask[TX_32X32] = INTRA_DC;
    sf->intra_uv_mode_mask[TX_32X32] = INTRA_DC;
    sf->adaptive_interp_filter_search = 1;

    if (MIN(cm->width, cm->height) >= 720)
      sf->partition_search_breakout_dist_thr = (1 << 25);
    else
      sf->partition_search_breakout_dist_thr = (1 << 23);
    sf->partition_search_breakout_rate_thr = 1000;
  }

  if (speed >= 4) {
    sf->use_square_partition_only = 1;
    sf->tx_size_search_method = USE_LARGESTALL;
    sf->disable_split_mask = DISABLE_ALL_SPLIT;
    sf->adaptive_rd_thresh = 4;
    sf->mode_search_skip_flags |= FLAG_SKIP_COMP_REFMISMATCH |
                                  FLAG_EARLY_TERMINATE;
    sf->disable_filter_search_var_thresh = 200;
    sf->use_lastframe_partitioning = LAST_FRAME_PARTITION_ALL;
    sf->use_lp32x32fdct = 1;
    sf->use_fast_coef_updates = ONE_LOOP_REDUCED;
    sf->use_fast_coef_costing = 1;

    if (MIN(cm->width, cm->height) >= 720)
      sf->partition_search_breakout_dist_thr = (1 << 26);
    else
      sf->partition_search_breakout_dist_thr = (1 << 24);
    sf->partition_search_breakout_rate_thr = 1500;
  }

  if (speed >= 5) {
    int i;

    sf->partition_search_type = FIXED_PARTITION;
    sf->optimize_coefficients = 0;
    sf->mv.search_method = HEX;
    sf->disable_filter_search_var_thresh = 500;
    for (i = 0; i < TX_SIZES; ++i) {
      sf->intra_y_mode_mask[i] = INTRA_DC;
      sf->intra_uv_mode_mask[i] = INTRA_DC;
    }
    cpi->allow_encode_breakout = ENCODE_BREAKOUT_ENABLED;
  }
  if (speed >= 6) {
    sf->mv.reduce_first_step_size = 1;
  }
}

static void set_rt_speed_feature(VP9_COMP *cpi, SPEED_FEATURES *sf,
                                 int speed, vp9e_tune_content content) {
  VP9_COMMON *const cm = &cpi->common;
  const int is_keyframe = cm->frame_type == KEY_FRAME;
  const int frames_since_key = is_keyframe ? 0 : cpi->rc.frames_since_key;
  sf->static_segmentation = 0;
  sf->adaptive_rd_thresh = 1;
  sf->use_fast_coef_costing = 1;

  if (speed >= 1) {
    sf->use_square_partition_only = !frame_is_intra_only(cm);
    sf->less_rectangular_check = 1;
    sf->tx_size_search_method = frame_is_intra_only(cm) ? USE_FULL_RD
                                                        : USE_LARGESTALL;

    if (MIN(cm->width, cm->height) >= 720)
      sf->disable_split_mask = cm->show_frame ? DISABLE_ALL_SPLIT
                                              : DISABLE_ALL_INTER_SPLIT;
    else
      sf->disable_split_mask = DISABLE_COMPOUND_SPLIT;

    sf->use_rd_breakout = 1;

    sf->adaptive_motion_search = 1;
    sf->adaptive_pred_interp_filter = 1;
    sf->mv.auto_mv_step_size = 1;
    sf->adaptive_rd_thresh = 2;
    sf->intra_y_mode_mask[TX_32X32] = INTRA_DC_H_V;
    sf->intra_uv_mode_mask[TX_32X32] = INTRA_DC_H_V;
    sf->intra_uv_mode_mask[TX_16X16] = INTRA_DC_H_V;
  }

  if (speed >= 2) {
    if (MIN(cm->width, cm->height) >= 720)
      sf->disable_split_mask = cm->show_frame ? DISABLE_ALL_SPLIT
                                              : DISABLE_ALL_INTER_SPLIT;
    else
      sf->disable_split_mask = LAST_AND_INTRA_SPLIT_ONLY;

    sf->mode_search_skip_flags = FLAG_SKIP_INTRA_DIRMISMATCH |
                                 FLAG_SKIP_INTRA_BESTINTER |
                                 FLAG_SKIP_COMP_BESTINTRA |
                                 FLAG_SKIP_INTRA_LOWVAR;
    sf->adaptive_pred_interp_filter = 2;
    sf->reference_masking = 1;
    sf->disable_filter_search_var_thresh = 50;
    sf->comp_inter_joint_search_thresh = BLOCK_SIZES;
    sf->auto_min_max_partition_size = RELAXED_NEIGHBORING_MIN_MAX;
    sf->use_lastframe_partitioning = LAST_FRAME_PARTITION_LOW_MOTION;
    sf->lf_motion_threshold = LOW_MOTION_THRESHOLD;
    sf->adjust_partitioning_from_last_frame = 1;
    sf->last_partitioning_redo_frequency = 3;
    sf->use_lp32x32fdct = 1;
    sf->mode_skip_start = 11;
    sf->intra_y_mode_mask[TX_16X16] = INTRA_DC_H_V;
  }

  if (speed >= 3) {
    sf->use_square_partition_only = 1;
    sf->disable_filter_search_var_thresh = 100;
    sf->use_lastframe_partitioning = LAST_FRAME_PARTITION_ALL;
    sf->constrain_copy_partition = 1;
    sf->use_uv_intra_rd_estimate = 1;
    sf->skip_encode_sb = 1;
    sf->mv.subpel_iters_per_step = 1;
    sf->use_fast_coef_updates = ONE_LOOP_REDUCED;
    sf->adaptive_rd_thresh = 4;
    sf->mode_skip_start = 6;
    sf->allow_skip_recode = 0;
    sf->optimize_coefficients = 0;
    sf->disable_split_mask = DISABLE_ALL_SPLIT;
    sf->lpf_pick = LPF_PICK_FROM_Q;
  }

  if (speed >= 4) {
    int i;
    sf->last_partitioning_redo_frequency = 4;
    sf->adaptive_rd_thresh = 5;
    sf->use_fast_coef_costing = 0;
    sf->auto_min_max_partition_size = STRICT_NEIGHBORING_MIN_MAX;
    sf->adjust_partitioning_from_last_frame =
        cm->last_frame_type != cm->frame_type || (0 ==
        (frames_since_key + 1) % sf->last_partitioning_redo_frequency);
    sf->mv.subpel_force_stop = 1;
    for (i = 0; i < TX_SIZES; i++) {
      sf->intra_y_mode_mask[i] = INTRA_DC_H_V;
      sf->intra_uv_mode_mask[i] = INTRA_DC;
    }
    sf->intra_y_mode_mask[TX_32X32] = INTRA_DC;
    sf->frame_parameter_update = 0;
    sf->mv.search_method = FAST_HEX;
    sf->inter_mode_mask[BLOCK_32X32] = INTER_NEAREST_NEAR_NEW;
    sf->inter_mode_mask[BLOCK_32X64] = INTER_NEAREST;
    sf->inter_mode_mask[BLOCK_64X32] = INTER_NEAREST;
    sf->inter_mode_mask[BLOCK_64X64] = INTER_NEAREST;
    sf->max_intra_bsize = BLOCK_32X32;
    sf->allow_skip_recode = 1;
  }

  if (speed >= 5) {
    sf->use_quant_fp = !is_keyframe;
    sf->auto_min_max_partition_size = is_keyframe ? RELAXED_NEIGHBORING_MIN_MAX
                                                  : STRICT_NEIGHBORING_MIN_MAX;
    sf->max_partition_size = BLOCK_32X32;
    sf->min_partition_size = BLOCK_8X8;
    sf->partition_check =
        (frames_since_key % sf->last_partitioning_redo_frequency == 1);
    sf->force_frame_boost = is_keyframe ||
        (frames_since_key % (sf->last_partitioning_redo_frequency << 1) == 1);
    sf->max_delta_qindex = is_keyframe ? 20 : 15;
    sf->partition_search_type = REFERENCE_PARTITION;
    sf->use_nonrd_pick_mode = 1;
    sf->allow_skip_recode = 0;
  }

  if (speed >= 6) {
    if (content == VP9E_CONTENT_SCREEN) {
      int i;
      // Allow fancy modes at all sizes since SOURCE_VAR_BASED_PARTITION is used
      for (i = 0; i < BLOCK_SIZES; ++i)
        sf->inter_mode_mask[i] = INTER_ALL;
    }

    // Adaptively switch between SOURCE_VAR_BASED_PARTITION and FIXED_PARTITION.
    sf->partition_search_type = SOURCE_VAR_BASED_PARTITION;
    sf->search_type_check_frequency = 50;

    sf->tx_size_search_method = is_keyframe ? USE_LARGESTALL : USE_TX_8X8;

    // This feature is only enabled when partition search is disabled.
    sf->reuse_inter_pred_sby = 1;

    // Increase mode checking threshold for NEWMV.
    sf->elevate_newmv_thresh = 2000;

    sf->mv.reduce_first_step_size = 1;
  }

  if (speed >= 7) {
    sf->mv.search_method = FAST_DIAMOND;
    sf->mv.fullpel_search_step_param = 10;
    sf->lpf_pick = LPF_PICK_MINIMAL_LPF;
    sf->encode_breakout_thresh = (MIN(cm->width, cm->height) >= 720) ?
        800 : 300;
    sf->elevate_newmv_thresh = 2500;
  }

  if (speed >= 12) {
    sf->elevate_newmv_thresh = 4000;
    sf->mv.subpel_force_stop = 2;
  }

  if (speed >= 13) {
    int i;
    sf->max_intra_bsize = BLOCK_32X32;
    for (i = 0; i < BLOCK_SIZES; ++i)
      sf->inter_mode_mask[i] = INTER_NEAREST;
  }
}

void vp9_set_speed_features(VP9_COMP *cpi) {
  SPEED_FEATURES *const sf = &cpi->sf;
  VP9_COMMON *const cm = &cpi->common;
  const VP9EncoderConfig *const oxcf = &cpi->oxcf;
  int i;

  // best quality defaults
  sf->frame_parameter_update = 1;
  sf->mv.search_method = NSTEP;
  sf->recode_loop = ALLOW_RECODE;
  sf->mv.subpel_search_method = SUBPEL_TREE;
  sf->mv.subpel_iters_per_step = 2;
  sf->mv.subpel_force_stop = 0;
  sf->optimize_coefficients = !is_lossless_requested(&cpi->oxcf);
  sf->mv.reduce_first_step_size = 0;
  sf->mv.auto_mv_step_size = 0;
  sf->mv.fullpel_search_step_param = 6;
  sf->comp_inter_joint_search_thresh = BLOCK_4X4;
  sf->adaptive_rd_thresh = 0;
  sf->use_lastframe_partitioning = LAST_FRAME_PARTITION_OFF;
  sf->tx_size_search_method = USE_FULL_RD;
  sf->use_lp32x32fdct = 0;
  sf->adaptive_motion_search = 0;
  sf->adaptive_pred_interp_filter = 0;
  sf->adaptive_mode_search = 0;
  sf->cb_pred_filter_search = 0;
  sf->cb_partition_search = 0;
  sf->motion_field_mode_search = 0;
  sf->alt_ref_search_fp = 0;
  sf->use_quant_fp = 0;
  sf->reference_masking = 0;
  sf->partition_search_type = SEARCH_PARTITION;
  sf->less_rectangular_check = 0;
  sf->use_square_partition_only = 0;
  sf->auto_min_max_partition_size = NOT_IN_USE;
  sf->max_partition_size = BLOCK_64X64;
  sf->min_partition_size = BLOCK_4X4;
  sf->adjust_partitioning_from_last_frame = 0;
  sf->last_partitioning_redo_frequency = 4;
  sf->constrain_copy_partition = 0;
  sf->disable_split_mask = 0;
  sf->mode_search_skip_flags = 0;
  sf->force_frame_boost = 0;
  sf->max_delta_qindex = 0;
  sf->disable_filter_search_var_thresh = 0;
  sf->adaptive_interp_filter_search = 0;

  for (i = 0; i < TX_SIZES; i++) {
    sf->intra_y_mode_mask[i] = INTRA_ALL;
    sf->intra_uv_mode_mask[i] = INTRA_ALL;
  }
  sf->use_rd_breakout = 0;
  sf->skip_encode_sb = 0;
  sf->use_uv_intra_rd_estimate = 0;
  sf->allow_skip_recode = 0;
  sf->lpf_pick = LPF_PICK_FROM_FULL_IMAGE;
  sf->use_fast_coef_updates = TWO_LOOP;
  sf->use_fast_coef_costing = 0;
  sf->mode_skip_start = MAX_MODES;  // Mode index at which mode skip mask set
  sf->use_nonrd_pick_mode = 0;
  for (i = 0; i < BLOCK_SIZES; ++i)
    sf->inter_mode_mask[i] = INTER_ALL;
  sf->max_intra_bsize = BLOCK_64X64;
  sf->reuse_inter_pred_sby = 0;
  // This setting only takes effect when partition_search_type is set
  // to FIXED_PARTITION.
  sf->always_this_block_size = BLOCK_16X16;
  sf->search_type_check_frequency = 50;
  sf->encode_breakout_thresh = 0;
  sf->elevate_newmv_thresh = 0;
  // Recode loop tolerence %.
  sf->recode_tolerance = 25;
  sf->default_interp_filter = SWITCHABLE;
  sf->tx_size_search_breakout = 0;
  sf->partition_search_breakout_dist_thr = 0;
  sf->partition_search_breakout_rate_thr = 0;

  if (oxcf->mode == REALTIME)
    set_rt_speed_feature(cpi, sf, oxcf->speed, oxcf->content);
  else if (oxcf->mode == GOOD)
    set_good_speed_feature(cpi, cm, sf, oxcf->speed);

  cpi->full_search_sad = vp9_full_search_sad;
  cpi->diamond_search_sad = oxcf->mode == BEST ? vp9_full_range_search
                                               : vp9_diamond_search_sad;
  cpi->refining_search_sad = vp9_refining_search_sad;


  // Slow quant, dct and trellis not worthwhile for first pass
  // so make sure they are always turned off.
  if (oxcf->pass == 1)
    sf->optimize_coefficients = 0;

  // No recode for 1 pass.
  if (oxcf->pass == 0) {
    sf->recode_loop = DISALLOW_RECODE;
    sf->optimize_coefficients = 0;
  }

  if (sf->mv.subpel_search_method == SUBPEL_TREE) {
    cpi->find_fractional_mv_step = vp9_find_best_sub_pixel_tree;
  } else if (sf->mv.subpel_search_method == SUBPEL_TREE_PRUNED) {
    cpi->find_fractional_mv_step = vp9_find_best_sub_pixel_tree_pruned;
  }

  cpi->mb.optimize = sf->optimize_coefficients == 1 && oxcf->pass != 1;

  if (sf->disable_split_mask == DISABLE_ALL_SPLIT)
    sf->adaptive_pred_interp_filter = 0;

  if (!cpi->oxcf.frame_periodic_boost) {
    sf->max_delta_qindex = 0;
  }

  if (cpi->encode_breakout && oxcf->mode == REALTIME &&
      sf->encode_breakout_thresh > cpi->encode_breakout)
    cpi->encode_breakout = sf->encode_breakout_thresh;
}
