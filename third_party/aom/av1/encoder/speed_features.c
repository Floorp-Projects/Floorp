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

#include <limits.h>

#include "av1/encoder/encoder.h"
#include "av1/encoder/speed_features.h"
#include "av1/encoder/rdopt.h"

#include "aom_dsp/aom_dsp_common.h"

// Setting this to 1 will disable trellis optimization completely.
// Setting this to 2 will disable trellis optimization within the
// transform search. Trellis optimization will still be applied
// in the final encode.
#define DISABLE_TRELLISQ_SEARCH 0

#define MAX_MESH_SPEED 5  // Max speed setting for mesh motion method
static MESH_PATTERN
    good_quality_mesh_patterns[MAX_MESH_SPEED + 1][MAX_MESH_STEP] = {
      { { 64, 8 }, { 28, 4 }, { 15, 1 }, { 7, 1 } },
      { { 64, 8 }, { 28, 4 }, { 15, 1 }, { 7, 1 } },
      { { 64, 8 }, { 14, 2 }, { 7, 1 }, { 7, 1 } },
      { { 64, 16 }, { 24, 8 }, { 12, 4 }, { 7, 1 } },
      { { 64, 16 }, { 24, 8 }, { 12, 4 }, { 7, 1 } },
      { { 64, 16 }, { 24, 8 }, { 12, 4 }, { 7, 1 } },
    };
static unsigned char good_quality_max_mesh_pct[MAX_MESH_SPEED + 1] = {
  50, 50, 25, 15, 5, 1
};

// TODO(huisu@google.com): These settings are pretty relaxed, tune them for
// each speed setting
static MESH_PATTERN intrabc_mesh_patterns[MAX_MESH_SPEED + 1][MAX_MESH_STEP] = {
  { { 256, 1 }, { 256, 1 }, { 0, 0 }, { 0, 0 } },
  { { 256, 1 }, { 256, 1 }, { 0, 0 }, { 0, 0 } },
  { { 64, 1 }, { 64, 1 }, { 0, 0 }, { 0, 0 } },
  { { 64, 1 }, { 64, 1 }, { 0, 0 }, { 0, 0 } },
  { { 64, 4 }, { 16, 1 }, { 0, 0 }, { 0, 0 } },
  { { 64, 4 }, { 16, 1 }, { 0, 0 }, { 0, 0 } },
};
static uint8_t intrabc_max_mesh_pct[MAX_MESH_SPEED + 1] = { 100, 100, 100,
                                                            25,  25,  10 };

// Intra only frames, golden frames (except alt ref overlays) and
// alt ref frames tend to be coded at a higher than ambient quality
static int frame_is_boosted(const AV1_COMP *cpi) {
  return frame_is_kf_gf_arf(cpi);
}

// Sets a partition size down to which the auto partition code will always
// search (can go lower), based on the image dimensions. The logic here
// is that the extent to which ringing artefacts are offensive, depends
// partly on the screen area that over which they propogate. Propogation is
// limited by transform block size but the screen area take up by a given block
// size will be larger for a small image format stretched to full screen.
static BLOCK_SIZE set_partition_min_limit(AV1_COMMON *const cm) {
  unsigned int screen_area = (cm->width * cm->height);

  // Select block size based on image format size.
  if (screen_area < 1280 * 720) {
    // Formats smaller in area than 720P
    return BLOCK_4X4;
  } else if (screen_area < 1920 * 1080) {
    // Format >= 720P and < 1080P
    return BLOCK_8X8;
  } else {
    // Formats 1080P and up
    return BLOCK_16X16;
  }
}

// Do we have an internal image edge (e.g. formatting bars).
static int has_internal_image_edge(const AV1_COMP *cpi) {
  return (cpi->oxcf.pass == 2) &&
         ((cpi->twopass.this_frame_stats.inactive_zone_rows > 0) ||
          (cpi->twopass.this_frame_stats.inactive_zone_cols > 0));
}

static void set_good_speed_feature_framesize_dependent(AV1_COMP *cpi,
                                                       SPEED_FEATURES *sf,
                                                       int speed) {
  AV1_COMMON *const cm = &cpi->common;
  const int is_720p_or_larger = AOMMIN(cm->width, cm->height) >= 720;
  const int is_480p_or_larger = AOMMIN(cm->width, cm->height) >= 480;

  if (is_480p_or_larger) {
    sf->use_square_partition_only_threshold = BLOCK_128X128;
  } else {
    sf->use_square_partition_only_threshold = BLOCK_64X64;
  }

  // TODO(huisu@google.com): train models for 720P and above.
  if (!is_720p_or_larger) {
    sf->ml_partition_search_breakout_thresh[0] = 200;  // BLOCK_8X8
    sf->ml_partition_search_breakout_thresh[1] = 250;  // BLOCK_16X16
    sf->ml_partition_search_breakout_thresh[2] = 300;  // BLOCK_32X32
    sf->ml_partition_search_breakout_thresh[3] = 500;  // BLOCK_64X64
    sf->ml_partition_search_breakout_thresh[4] = -1;   // BLOCK_128X128
  }

  if (speed >= 1) {
    if (is_720p_or_larger) {
      sf->use_square_partition_only_threshold = BLOCK_128X128;
    } else if (is_480p_or_larger) {
      sf->use_square_partition_only_threshold = BLOCK_64X64;
    } else {
      sf->use_square_partition_only_threshold = BLOCK_32X32;
    }

    if (!is_720p_or_larger) {
      sf->ml_partition_search_breakout_thresh[0] = 200;  // BLOCK_8X8
      sf->ml_partition_search_breakout_thresh[1] = 250;  // BLOCK_16X16
      sf->ml_partition_search_breakout_thresh[2] = 300;  // BLOCK_32X32
      sf->ml_partition_search_breakout_thresh[3] = 300;  // BLOCK_64X64
      sf->ml_partition_search_breakout_thresh[4] = -1;   // BLOCK_128X128
    }
  }

  if (speed >= 2) {
    if (is_720p_or_larger) {
      sf->disable_split_mask =
          cm->show_frame ? DISABLE_ALL_SPLIT : DISABLE_ALL_INTER_SPLIT;
      sf->adaptive_pred_interp_filter = 0;
      sf->partition_search_breakout_dist_thr = (1 << 24);
      sf->partition_search_breakout_rate_thr = 120;
    } else {
      sf->disable_split_mask = LAST_AND_INTRA_SPLIT_ONLY;
      sf->partition_search_breakout_dist_thr = (1 << 22);
      sf->partition_search_breakout_rate_thr = 100;
    }
    sf->rd_auto_partition_min_limit = set_partition_min_limit(cm);
  }

  if (speed >= 3) {
    if (is_720p_or_larger) {
      sf->disable_split_mask = DISABLE_ALL_SPLIT;
      sf->partition_search_breakout_dist_thr = (1 << 25);
      sf->partition_search_breakout_rate_thr = 200;
    } else {
      sf->max_intra_bsize = BLOCK_32X32;
      sf->disable_split_mask = DISABLE_ALL_INTER_SPLIT;
      sf->partition_search_breakout_dist_thr = (1 << 23);
      sf->partition_search_breakout_rate_thr = 120;
    }
  }

  // If this is a two pass clip that fits the criteria for animated or
  // graphics content then reset disable_split_mask for speeds 2+.
  // Also if the image edge is internal to the coded area.
  if ((speed >= 2) && (cpi->oxcf.pass == 2) &&
      ((cpi->twopass.fr_content_type == FC_GRAPHICS_ANIMATION) ||
       (has_internal_image_edge(cpi)))) {
    sf->disable_split_mask = DISABLE_COMPOUND_SPLIT;
  }

  if (speed >= 4) {
    if (is_720p_or_larger) {
      sf->partition_search_breakout_dist_thr = (1 << 26);
    } else {
      sf->partition_search_breakout_dist_thr = (1 << 24);
    }
    sf->disable_split_mask = DISABLE_ALL_SPLIT;
  }
}

static void set_good_speed_features_framesize_independent(AV1_COMP *cpi,
                                                          SPEED_FEATURES *sf,
                                                          int speed) {
  AV1_COMMON *const cm = &cpi->common;
  const int boosted = frame_is_boosted(cpi);

  // Speed 0 for all speed features that give neutral coding performance change.
  sf->reduce_inter_modes = 1;
  sf->prune_ext_partition_types_search_level = 1;
  sf->ml_prune_rect_partition = 1;
  sf->ml_prune_ab_partition = 1;
  sf->ml_prune_4_partition = 1;
  sf->adaptive_txb_search_level = 1;
  sf->jnt_comp_skip_mv_search = 1;
  sf->model_based_prune_tx_search_level = 1;
  sf->model_based_post_interp_filter_breakout = 1;
  sf->inter_mode_rd_model_estimation = 1;
  sf->prune_ref_frame_for_rect_partitions =
      !(boosted || cpi->refresh_bwd_ref_frame || cpi->refresh_alt2_ref_frame);
  sf->less_rectangular_check_level = 1;
  sf->gm_search_type = GM_REDUCED_REF_SEARCH;
  sf->gm_disable_recode = 1;

  if (speed >= 1) {
    sf->gm_erroradv_type = GM_ERRORADV_TR_1;
    sf->selective_ref_frame = 1;
    sf->inter_tx_size_search_init_depth_rect = 1;
    sf->inter_tx_size_search_init_depth_sqr = 1;
    sf->intra_tx_size_search_init_depth_rect = 1;
    sf->intra_tx_size_search_init_depth_sqr = 1;
    sf->tx_size_search_lgr_block = 1;
    if (speed >= CONFIG_2PASS_PARTITION_SEARCH_LVL) {
      sf->two_pass_partition_search = 1;
      sf->mode_pruning_based_on_two_pass_partition_search = 1;
    }
    sf->prune_ext_partition_types_search_level = 2;
    sf->use_fast_interpolation_filter_search = 1;
    sf->skip_repeat_interpolation_filter_search = 1;
    sf->tx_type_search.skip_tx_search = 1;
    sf->tx_type_search.ml_tx_split_thresh = 40;
    sf->model_based_prune_tx_search_level = 0;
    sf->model_based_post_interp_filter_breakout = 0;
    // TODO(angiebird): Re-evaluate the impact of inter_mode_rd_model_estimation
    // on speed 1
    sf->inter_mode_rd_model_estimation = 0;
    sf->adaptive_txb_search_level = 2;
    sf->use_intra_txb_hash = 1;
    sf->optimize_b_precheck = 1;
    sf->dual_sgr_penalty_level = 1;
    sf->use_accurate_subpel_search = 1;
    sf->reuse_inter_intra_mode = 1;
    sf->prune_comp_search_by_single_result = 1;
    sf->skip_repeated_newmv = 1;
    sf->obmc_full_pixel_search_level = 1;
  }

  if (speed >= 2) {
    sf->gm_erroradv_type = GM_ERRORADV_TR_2;

    sf->selective_ref_frame = 2;
    sf->fast_cdef_search = 1;

    sf->adaptive_rd_thresh = 1;
    sf->mv.auto_mv_step_size = 1;
    sf->mv.subpel_iters_per_step = 1;
    sf->disable_filter_search_var_thresh = 100;
    sf->comp_inter_joint_search_thresh = BLOCK_SIZES_ALL;

    sf->partition_search_breakout_rate_thr = 80;
    // Note: This speed feature is disable as it seems to be worse in
    // compression/quality and is also slower.
    // sf->auto_min_max_partition_size = RELAXED_NEIGHBORING_MIN_MAX;
    sf->allow_partition_search_skip = 1;
    sf->disable_wedge_search_var_thresh = 100;
    sf->fast_wedge_sign_estimate = 1;
  }

  if (speed >= 3) {
    sf->tx_size_search_method = boosted ? USE_FULL_RD : USE_LARGESTALL;
    sf->less_rectangular_check_level = 2;
    sf->adaptive_pred_interp_filter = 1;
    // adaptive_motion_search breaks encoder multi-thread tests.
    // The values in x->pred_mv[] differ for single and multi-thread cases.
    // See aomedia:1778.
    // sf->adaptive_motion_search = 1;
    sf->recode_loop = ALLOW_RECODE_KFARFGF;
    sf->use_transform_domain_distortion = 1;
    sf->use_accurate_subpel_search = 0;
    sf->adaptive_rd_thresh = 2;
    sf->tx_type_search.prune_mode = PRUNE_2D_FAST;
    sf->gm_search_type = GM_DISABLE_SEARCH;
    sf->prune_comp_search_by_single_result = 2;
  }

  if (speed >= 4) {
    sf->tx_type_search.fast_intra_tx_type_search = 1;
    sf->tx_type_search.fast_inter_tx_type_search = 1;
    sf->use_square_partition_only_threshold =
        boosted ? BLOCK_128X128 : BLOCK_4X4;
    sf->tx_size_search_method =
        frame_is_intra_only(cm) ? USE_FULL_RD : USE_LARGESTALL;
    sf->mv.subpel_search_method = SUBPEL_TREE_PRUNED;
    sf->adaptive_pred_interp_filter = 0;
    sf->adaptive_mode_search = 1;
    sf->cb_partition_search = !boosted;
    sf->alt_ref_search_fp = 1;
  }

  if (speed >= 5) {
    sf->recode_loop = ALLOW_RECODE_KFMAXBW;
    sf->intra_y_mode_mask[TX_64X64] = INTRA_DC_H_V;
    sf->intra_uv_mode_mask[TX_64X64] = UV_INTRA_DC_H_V_CFL;
    sf->intra_y_mode_mask[TX_32X32] = INTRA_DC_H_V;
    sf->intra_uv_mode_mask[TX_32X32] = UV_INTRA_DC_H_V_CFL;
    sf->intra_y_mode_mask[TX_16X16] = INTRA_DC_H_V;
    sf->intra_uv_mode_mask[TX_16X16] = UV_INTRA_DC_H_V_CFL;
    sf->use_square_partition_only_threshold = BLOCK_4X4;
    sf->tx_size_search_method = USE_LARGESTALL;
    sf->mv.search_method = BIGDIA;
    sf->mv.subpel_search_method = SUBPEL_TREE_PRUNED_MORE;
    sf->adaptive_rd_thresh = 4;
    sf->mode_search_skip_flags =
        (cm->frame_type == KEY_FRAME)
            ? 0
            : FLAG_SKIP_INTRA_DIRMISMATCH | FLAG_SKIP_INTRA_BESTINTER |
                  FLAG_SKIP_COMP_BESTINTRA | FLAG_SKIP_INTRA_LOWVAR |
                  FLAG_EARLY_TERMINATE;
    sf->disable_filter_search_var_thresh = 200;
    sf->use_fast_coef_costing = 1;
    sf->partition_search_breakout_rate_thr = 300;
    sf->use_transform_domain_distortion = 2;
  }

  if (speed >= 6) {
    int i;
    sf->optimize_coefficients = NO_TRELLIS_OPT;
    sf->mv.search_method = HEX;
    sf->disable_filter_search_var_thresh = 500;
    for (i = 0; i < TX_SIZES; ++i) {
      sf->intra_y_mode_mask[i] = INTRA_DC;
      sf->intra_uv_mode_mask[i] = UV_INTRA_DC_CFL;
    }
    sf->partition_search_breakout_rate_thr = 500;
    sf->mv.reduce_first_step_size = 1;
    sf->simple_model_rd_from_var = 1;
  }
  if (speed >= 7) {
    sf->default_max_partition_size = BLOCK_32X32;
    sf->default_min_partition_size = BLOCK_8X8;
    sf->intra_y_mode_mask[TX_64X64] = INTRA_DC;
    sf->intra_y_mode_mask[TX_32X32] = INTRA_DC;
    sf->frame_parameter_update = 0;
    sf->mv.search_method = FAST_HEX;
    sf->partition_search_type = REFERENCE_PARTITION;
    sf->mode_search_skip_flags |= FLAG_SKIP_INTRA_DIRMISMATCH;
  }
  if (speed >= 8) {
    sf->mv.search_method = FAST_DIAMOND;
    sf->mv.subpel_force_stop = 2;
    sf->lpf_pick = LPF_PICK_MINIMAL_LPF;
  }
}

void av1_set_speed_features_framesize_dependent(AV1_COMP *cpi) {
  SPEED_FEATURES *const sf = &cpi->sf;
  const AV1EncoderConfig *const oxcf = &cpi->oxcf;
  RD_OPT *const rd = &cpi->rd;
  int i;

  if (oxcf->mode == GOOD) {
    set_good_speed_feature_framesize_dependent(cpi, sf, oxcf->speed);
  }

  if (sf->disable_split_mask == DISABLE_ALL_SPLIT) {
    sf->adaptive_pred_interp_filter = 0;
  }

  // Check for masked out split cases.
  for (i = 0; i < MAX_REFS; ++i) {
    if (sf->disable_split_mask & (1 << i)) {
      rd->thresh_mult_sub8x8[i] = INT_MAX;
    }
  }

  // This is only used in motion vector unit test.
  if (cpi->oxcf.motion_vector_unit_test == 1)
    cpi->find_fractional_mv_step = av1_return_max_sub_pixel_mv;
  else if (cpi->oxcf.motion_vector_unit_test == 2)
    cpi->find_fractional_mv_step = av1_return_min_sub_pixel_mv;
}

void av1_set_speed_features_framesize_independent(AV1_COMP *cpi) {
  AV1_COMMON *const cm = &cpi->common;
  SPEED_FEATURES *const sf = &cpi->sf;
  MACROBLOCK *const x = &cpi->td.mb;
  const AV1EncoderConfig *const oxcf = &cpi->oxcf;
  int i;

  // best quality defaults
  sf->frame_parameter_update = 1;
  sf->mv.search_method = NSTEP;
  sf->recode_loop = ALLOW_RECODE;
  sf->mv.subpel_search_method = SUBPEL_TREE;
  sf->mv.subpel_iters_per_step = 2;
  sf->mv.subpel_force_stop = 0;
#if DISABLE_TRELLISQ_SEARCH == 2
  sf->optimize_coefficients = !is_lossless_requested(&cpi->oxcf)
                                  ? FINAL_PASS_TRELLIS_OPT
                                  : NO_TRELLIS_OPT;
#elif DISABLE_TRELLISQ_SEARCH == 1
  sf->optimize_coefficients = NO_TRELLIS_OPT;
#else
  if (is_lossless_requested(&cpi->oxcf))
    sf->optimize_coefficients = NO_TRELLIS_OPT;
  else
    sf->optimize_coefficients = FULL_TRELLIS_OPT;
#endif  // DISABLE_TRELLISQ_SEARCH
  sf->gm_erroradv_type = GM_ERRORADV_TR_0;
  sf->mv.reduce_first_step_size = 0;
  sf->mv.auto_mv_step_size = 0;
  sf->comp_inter_joint_search_thresh = BLOCK_4X4;
  sf->adaptive_rd_thresh = 0;
  sf->tx_size_search_method = USE_FULL_RD;
  sf->inter_tx_size_search_init_depth_sqr = 0;
  sf->inter_tx_size_search_init_depth_rect = 0;
  sf->intra_tx_size_search_init_depth_rect = 0;
  sf->intra_tx_size_search_init_depth_sqr = 0;
  sf->tx_size_search_lgr_block = 0;
  sf->model_based_prune_tx_search_level = 0;
  sf->model_based_post_interp_filter_breakout = 0;
  sf->reduce_inter_modes = 0;
  sf->selective_ref_gm = 1;
  sf->adaptive_motion_search = 0;
  sf->adaptive_pred_interp_filter = 0;
  sf->adaptive_mode_search = 0;
  sf->cb_partition_search = 0;
  sf->alt_ref_search_fp = 0;
  sf->partition_search_type = SEARCH_PARTITION;
  sf->tx_type_search.prune_mode = PRUNE_2D_ACCURATE;
  sf->tx_type_search.ml_tx_split_thresh = 30;
  sf->tx_type_search.use_skip_flag_prediction = 1;
  sf->tx_type_search.fast_intra_tx_type_search = 0;
  sf->tx_type_search.fast_inter_tx_type_search = 0;
  sf->tx_type_search.skip_tx_search = 0;
  sf->selective_ref_frame = 0;
  sf->less_rectangular_check_level = 0;
  sf->use_square_partition_only_threshold = BLOCK_128X128;
  sf->prune_ref_frame_for_rect_partitions = 0;
  sf->auto_min_max_partition_size = NOT_IN_USE;
  sf->rd_auto_partition_min_limit = BLOCK_4X4;
  sf->default_max_partition_size = BLOCK_LARGEST;
  sf->default_min_partition_size = BLOCK_4X4;
  sf->adjust_partitioning_from_last_frame = 0;
  sf->disable_split_mask = 0;
  sf->mode_search_skip_flags = 0;
  sf->disable_filter_search_var_thresh = 0;
  sf->allow_partition_search_skip = 0;
  sf->use_accurate_subpel_search = 2;
  sf->disable_wedge_search_var_thresh = 0;
  sf->fast_wedge_sign_estimate = 0;
  sf->drop_ref = 0;
  sf->skip_intra_in_interframe = 1;
  sf->txb_split_cap = 1;
  sf->adaptive_txb_search_level = 0;
  sf->two_pass_partition_search = 0;
  sf->mode_pruning_based_on_two_pass_partition_search = 0;
  sf->use_intra_txb_hash = 0;
  sf->use_inter_txb_hash = 1;
  sf->use_mb_rd_hash = 1;
  sf->optimize_b_precheck = 0;
  sf->jnt_comp_fast_tx_search = 0;
  sf->jnt_comp_skip_mv_search = 0;
  sf->reuse_inter_intra_mode = 0;

  for (i = 0; i < TX_SIZES; i++) {
    sf->intra_y_mode_mask[i] = INTRA_ALL;
    sf->intra_uv_mode_mask[i] = UV_INTRA_ALL;
  }
  sf->lpf_pick = LPF_PICK_FROM_FULL_IMAGE;
  sf->use_fast_coef_costing = 0;
  sf->max_intra_bsize = BLOCK_LARGEST;
  // This setting only takes effect when partition_search_type is set
  // to FIXED_PARTITION.
  sf->always_this_block_size = BLOCK_16X16;
  // Recode loop tolerance %.
  sf->recode_tolerance = 25;
  sf->partition_search_breakout_dist_thr = 0;
  sf->partition_search_breakout_rate_thr = 0;
  sf->simple_model_rd_from_var = 0;
  sf->prune_ext_partition_types_search_level = 0;
  sf->ml_prune_rect_partition = 0;
  sf->ml_prune_ab_partition = 0;
  sf->ml_prune_4_partition = 0;
  sf->fast_cdef_search = 0;
  for (i = 0; i < PARTITION_BLOCK_SIZES; ++i)
    sf->ml_partition_search_breakout_thresh[i] = -1;  // -1 means not enabled.

  // Set this at the appropriate speed levels
  sf->use_transform_domain_distortion = 0;
  sf->gm_search_type = GM_FULL_SEARCH;
  sf->gm_disable_recode = 0;
  sf->use_fast_interpolation_filter_search = 0;
  sf->skip_repeat_interpolation_filter_search = 0;
  sf->use_hash_based_trellis = 0;
  sf->prune_comp_search_by_single_result = 0;
  sf->skip_repeated_newmv = 0;

  // Set decoder side speed feature to use less dual sgr modes
  sf->dual_sgr_penalty_level = 0;

  sf->inter_mode_rd_model_estimation = 0;
  sf->obmc_full_pixel_search_level = 0;

  if (oxcf->mode == GOOD)
    set_good_speed_features_framesize_independent(cpi, sf, oxcf->speed);

  // sf->partition_search_breakout_dist_thr is set assuming max 64x64
  // blocks. Normalise this if the blocks are bigger.
  if (MAX_SB_SIZE_LOG2 > 6) {
    sf->partition_search_breakout_dist_thr <<= 2 * (MAX_SB_SIZE_LOG2 - 6);
  }

  cpi->diamond_search_sad = av1_diamond_search_sad;

  sf->allow_exhaustive_searches = 1;
  int speed = (oxcf->speed > MAX_MESH_SPEED) ? MAX_MESH_SPEED : oxcf->speed;
  if (cpi->twopass.fr_content_type == FC_GRAPHICS_ANIMATION)
    sf->exhaustive_searches_thresh = (1 << 24);
  else
    sf->exhaustive_searches_thresh = (1 << 25);
  sf->max_exaustive_pct = good_quality_max_mesh_pct[speed];
  if (speed > 0)
    sf->exhaustive_searches_thresh = sf->exhaustive_searches_thresh << 1;

  for (i = 0; i < MAX_MESH_STEP; ++i) {
    sf->mesh_patterns[i].range = good_quality_mesh_patterns[speed][i].range;
    sf->mesh_patterns[i].interval =
        good_quality_mesh_patterns[speed][i].interval;
  }
  if ((frame_is_intra_only(cm) && cm->allow_screen_content_tools) &&
      (cpi->twopass.fr_content_type == FC_GRAPHICS_ANIMATION ||
       cpi->oxcf.content == AOM_CONTENT_SCREEN)) {
    for (i = 0; i < MAX_MESH_STEP; ++i) {
      sf->mesh_patterns[i].range = intrabc_mesh_patterns[speed][i].range;
      sf->mesh_patterns[i].interval = intrabc_mesh_patterns[speed][i].interval;
    }
    sf->max_exaustive_pct = intrabc_max_mesh_pct[speed];
  }

  // Slow quant, dct and trellis not worthwhile for first pass
  // so make sure they are always turned off.
  if (oxcf->pass == 1) sf->optimize_coefficients = NO_TRELLIS_OPT;

  // No recode for 1 pass.
  if (oxcf->pass == 0) {
    sf->recode_loop = DISALLOW_RECODE;
    sf->optimize_coefficients = NO_TRELLIS_OPT;
  }

  if (sf->mv.subpel_search_method == SUBPEL_TREE) {
    cpi->find_fractional_mv_step = av1_find_best_sub_pixel_tree;
  } else if (sf->mv.subpel_search_method == SUBPEL_TREE_PRUNED) {
    cpi->find_fractional_mv_step = av1_find_best_sub_pixel_tree_pruned;
  } else if (sf->mv.subpel_search_method == SUBPEL_TREE_PRUNED_MORE) {
    cpi->find_fractional_mv_step = av1_find_best_sub_pixel_tree_pruned_more;
  } else if (sf->mv.subpel_search_method == SUBPEL_TREE_PRUNED_EVENMORE) {
    cpi->find_fractional_mv_step = av1_find_best_sub_pixel_tree_pruned_evenmore;
  }

  cpi->optimize_speed_feature =
      oxcf->pass != 1 ? sf->optimize_coefficients : NO_TRELLIS_OPT;
  // FIXME: trellis not very efficient for quantisation matrices
  if (cm->using_qmatrix) cpi->optimize_speed_feature = NO_TRELLIS_OPT;
  if (oxcf->disable_trellis_quant) cpi->optimize_speed_feature = NO_TRELLIS_OPT;

  x->min_partition_size = sf->default_min_partition_size;
  x->max_partition_size = sf->default_max_partition_size;

  // This is only used in motion vector unit test.
  if (cpi->oxcf.motion_vector_unit_test == 1)
    cpi->find_fractional_mv_step = av1_return_max_sub_pixel_mv;
  else if (cpi->oxcf.motion_vector_unit_test == 2)
    cpi->find_fractional_mv_step = av1_return_min_sub_pixel_mv;

#if CONFIG_DIST_8X8
  if (sf->use_transform_domain_distortion > 0) cpi->oxcf.using_dist_8x8 = 0;

  if (cpi->oxcf.using_dist_8x8) x->min_partition_size = BLOCK_8X8;
#endif  // CONFIG_DIST_8X8
}
