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

#ifndef AV1_ENCODER_SPEED_FEATURES_H_
#define AV1_ENCODER_SPEED_FEATURES_H_

#include "av1/common/enums.h"

#ifdef __cplusplus
extern "C" {
#endif

enum {
  INTRA_ALL = (1 << DC_PRED) | (1 << V_PRED) | (1 << H_PRED) | (1 << D45_PRED) |
              (1 << D135_PRED) | (1 << D117_PRED) | (1 << D153_PRED) |
              (1 << D207_PRED) | (1 << D63_PRED) | (1 << SMOOTH_PRED) |
#if CONFIG_SMOOTH_HV
              (1 << SMOOTH_V_PRED) | (1 << SMOOTH_H_PRED) |
#endif  // CONFIG_SMOOTH_HV
              (1 << TM_PRED),
#if CONFIG_CFL
  UV_INTRA_ALL = (1 << UV_DC_PRED) | (1 << UV_V_PRED) | (1 << UV_H_PRED) |
                 (1 << UV_D45_PRED) | (1 << UV_D135_PRED) |
                 (1 << UV_D117_PRED) | (1 << UV_D153_PRED) |
                 (1 << UV_D207_PRED) | (1 << UV_D63_PRED) |
                 (1 << UV_SMOOTH_PRED) |
#if CONFIG_SMOOTH_HV
                 (1 << UV_SMOOTH_V_PRED) | (1 << UV_SMOOTH_H_PRED) |
#endif  // CONFIG_SMOOTH_HV
                 (1 << UV_TM_PRED) | (1 << UV_CFL_PRED),
  UV_INTRA_DC = (1 << UV_DC_PRED),
  UV_INTRA_DC_CFL = (1 << UV_DC_PRED) | (1 << UV_CFL_PRED),
  UV_INTRA_DC_TM = (1 << UV_DC_PRED) | (1 << UV_TM_PRED),
  UV_INTRA_DC_TM_CFL =
      (1 << UV_DC_PRED) | (1 << UV_TM_PRED) | (1 << UV_CFL_PRED),
  UV_INTRA_DC_H_V = (1 << UV_DC_PRED) | (1 << UV_V_PRED) | (1 << UV_H_PRED),
  UV_INTRA_DC_H_V_CFL = (1 << UV_DC_PRED) | (1 << UV_V_PRED) |
                        (1 << UV_H_PRED) | (1 << UV_CFL_PRED),
  UV_INTRA_DC_TM_H_V = (1 << UV_DC_PRED) | (1 << UV_TM_PRED) |
                       (1 << UV_V_PRED) | (1 << UV_H_PRED),
  UV_INTRA_DC_TM_H_V_CFL = (1 << UV_DC_PRED) | (1 << UV_TM_PRED) |
                           (1 << UV_V_PRED) | (1 << UV_H_PRED) |
                           (1 << UV_CFL_PRED),
#endif  // CONFIG_CFL
  INTRA_DC = (1 << DC_PRED),
  INTRA_DC_TM = (1 << DC_PRED) | (1 << TM_PRED),
  INTRA_DC_H_V = (1 << DC_PRED) | (1 << V_PRED) | (1 << H_PRED),
  INTRA_DC_TM_H_V =
      (1 << DC_PRED) | (1 << TM_PRED) | (1 << V_PRED) | (1 << H_PRED)
};

enum {
#if CONFIG_COMPOUND_SINGLEREF
// TODO(zoeliu): To further consider following single ref comp modes:
//               SR_NEAREST_NEARMV, SR_NEAREST_NEWMV, SR_NEAR_NEWMV,
//               SR_ZERO_NEWMV, and SR_NEW_NEWMV.
#endif  // CONFIG_COMPOUND_SINGLEREF
  INTER_ALL = (1 << NEARESTMV) | (1 << NEARMV) | (1 << ZEROMV) | (1 << NEWMV) |
              (1 << NEAREST_NEARESTMV) | (1 << NEAR_NEARMV) | (1 << NEW_NEWMV) |
              (1 << NEAREST_NEWMV) | (1 << NEAR_NEWMV) | (1 << NEW_NEARMV) |
              (1 << NEW_NEARESTMV) | (1 << ZERO_ZEROMV),
  INTER_NEAREST = (1 << NEARESTMV) | (1 << NEAREST_NEARESTMV) |
                  (1 << NEW_NEARESTMV) | (1 << NEAREST_NEWMV),
  INTER_NEAREST_NEW = (1 << NEARESTMV) | (1 << NEWMV) |
                      (1 << NEAREST_NEARESTMV) | (1 << NEW_NEWMV) |
                      (1 << NEW_NEARESTMV) | (1 << NEAREST_NEWMV) |
                      (1 << NEW_NEARMV) | (1 << NEAR_NEWMV),
  INTER_NEAREST_ZERO = (1 << NEARESTMV) | (1 << ZEROMV) |
                       (1 << NEAREST_NEARESTMV) | (1 << ZERO_ZEROMV) |
                       (1 << NEAREST_NEWMV) | (1 << NEW_NEARESTMV),
  INTER_NEAREST_NEW_ZERO = (1 << NEARESTMV) | (1 << ZEROMV) | (1 << NEWMV) |
                           (1 << NEAREST_NEARESTMV) | (1 << ZERO_ZEROMV) |
                           (1 << NEW_NEWMV) | (1 << NEW_NEARESTMV) |
                           (1 << NEAREST_NEWMV) | (1 << NEW_NEARMV) |
                           (1 << NEAR_NEWMV),
  INTER_NEAREST_NEAR_NEW = (1 << NEARESTMV) | (1 << NEARMV) | (1 << NEWMV) |
                           (1 << NEAREST_NEARESTMV) | (1 << NEW_NEWMV) |
                           (1 << NEW_NEARESTMV) | (1 << NEAREST_NEWMV) |
                           (1 << NEW_NEARMV) | (1 << NEAR_NEWMV) |
                           (1 << NEAR_NEARMV),
  INTER_NEAREST_NEAR_ZERO = (1 << NEARESTMV) | (1 << NEARMV) | (1 << ZEROMV) |
                            (1 << NEAREST_NEARESTMV) | (1 << ZERO_ZEROMV) |
                            (1 << NEAREST_NEWMV) | (1 << NEW_NEARESTMV) |
                            (1 << NEW_NEARMV) | (1 << NEAR_NEWMV) |
                            (1 << NEAR_NEARMV),
};

enum {
  DISABLE_ALL_INTER_SPLIT = (1 << THR_COMP_GA) | (1 << THR_COMP_LA) |
                            (1 << THR_ALTR) | (1 << THR_GOLD) | (1 << THR_LAST),

  DISABLE_ALL_SPLIT = (1 << THR_INTRA) | DISABLE_ALL_INTER_SPLIT,

  DISABLE_COMPOUND_SPLIT = (1 << THR_COMP_GA) | (1 << THR_COMP_LA),

  LAST_AND_INTRA_SPLIT_ONLY = (1 << THR_COMP_GA) | (1 << THR_COMP_LA) |
                              (1 << THR_ALTR) | (1 << THR_GOLD)
};

typedef enum {
  DIAMOND = 0,
  NSTEP = 1,
  HEX = 2,
  BIGDIA = 3,
  SQUARE = 4,
  FAST_HEX = 5,
  FAST_DIAMOND = 6
} SEARCH_METHODS;

typedef enum {
  // No recode.
  DISALLOW_RECODE = 0,
  // Allow recode for KF and exceeding maximum frame bandwidth.
  ALLOW_RECODE_KFMAXBW = 1,
  // Allow recode only for KF/ARF/GF frames.
  ALLOW_RECODE_KFARFGF = 2,
  // Allow recode for all frames based on bitrate constraints.
  ALLOW_RECODE = 3,
} RECODE_LOOP_TYPE;

typedef enum {
  SUBPEL_TREE = 0,
  SUBPEL_TREE_PRUNED = 1,           // Prunes 1/2-pel searches
  SUBPEL_TREE_PRUNED_MORE = 2,      // Prunes 1/2-pel searches more aggressively
  SUBPEL_TREE_PRUNED_EVENMORE = 3,  // Prunes 1/2- and 1/4-pel searches
  // Other methods to come
} SUBPEL_SEARCH_METHODS;

typedef enum {
  NO_MOTION_THRESHOLD = 0,
  LOW_MOTION_THRESHOLD = 7
} MOTION_THRESHOLD;

typedef enum {
  USE_FULL_RD = 0,
  USE_LARGESTALL,
  USE_TX_8X8
} TX_SIZE_SEARCH_METHOD;

typedef enum {
  NOT_IN_USE = 0,
  RELAXED_NEIGHBORING_MIN_MAX = 1
} AUTO_MIN_MAX_MODE;

typedef enum {
  // Try the full image with different values.
  LPF_PICK_FROM_FULL_IMAGE,
  // Try a small portion of the image with different values.
  LPF_PICK_FROM_SUBIMAGE,
  // Estimate the level based on quantizer and frame type
  LPF_PICK_FROM_Q,
  // Pick 0 to disable LPF if LPF was enabled last frame
  LPF_PICK_MINIMAL_LPF
} LPF_PICK_METHOD;

typedef enum {
  // Terminate search early based on distortion so far compared to
  // qp step, distortion in the neighborhood of the frame, etc.
  FLAG_EARLY_TERMINATE = 1 << 0,

  // Skips comp inter modes if the best so far is an intra mode.
  FLAG_SKIP_COMP_BESTINTRA = 1 << 1,

  // Skips oblique intra modes if the best so far is an inter mode.
  FLAG_SKIP_INTRA_BESTINTER = 1 << 3,

  // Skips oblique intra modes  at angles 27, 63, 117, 153 if the best
  // intra so far is not one of the neighboring directions.
  FLAG_SKIP_INTRA_DIRMISMATCH = 1 << 4,

  // Skips intra modes other than DC_PRED if the source variance is small
  FLAG_SKIP_INTRA_LOWVAR = 1 << 5,
} MODE_SEARCH_SKIP_LOGIC;

typedef enum {
  FLAG_SKIP_EIGHTTAP_REGULAR = 1 << EIGHTTAP_REGULAR,
  FLAG_SKIP_EIGHTTAP_SMOOTH = 1 << EIGHTTAP_SMOOTH,
  FLAG_SKIP_MULTITAP_SHARP = 1 << MULTITAP_SHARP,
} INTERP_FILTER_MASK;

typedef enum {
  NO_PRUNE = 0,
  // eliminates one tx type in vertical and horizontal direction
  PRUNE_ONE = 1,
#if CONFIG_EXT_TX
  // eliminates two tx types in each direction
  PRUNE_TWO = 2,
#endif
} TX_TYPE_PRUNE_MODE;

typedef struct {
  TX_TYPE_PRUNE_MODE prune_mode;
  int fast_intra_tx_type_search;
  int fast_inter_tx_type_search;

  // Use a skip flag prediction model to detect blocks with skip = 1 early
  // and avoid doing full TX type search for such blocks.
  int use_skip_flag_prediction;
} TX_TYPE_SEARCH;

typedef enum {
  // Search partitions using RD criterion
  SEARCH_PARTITION,

  // Always use a fixed size partition
  FIXED_PARTITION,

  REFERENCE_PARTITION
} PARTITION_SEARCH_TYPE;

typedef enum {
  // Does a dry run to see if any of the contexts need to be updated or not,
  // before the final run.
  TWO_LOOP = 0,

  // No dry run, also only half the coef contexts and bands are updated.
  // The rest are not updated at all.
  ONE_LOOP_REDUCED = 1
} FAST_COEFF_UPDATE;

typedef struct MV_SPEED_FEATURES {
  // Motion search method (Diamond, NSTEP, Hex, Big Diamond, Square, etc).
  SEARCH_METHODS search_method;

  // This parameter controls which step in the n-step process we start at.
  // It's changed adaptively based on circumstances.
  int reduce_first_step_size;

  // If this is set to 1, we limit the motion search range to 2 times the
  // largest motion vector found in the last frame.
  int auto_mv_step_size;

  // Subpel_search_method can only be subpel_tree which does a subpixel
  // logarithmic search that keeps stepping at 1/2 pixel units until
  // you stop getting a gain, and then goes on to 1/4 and repeats
  // the same process. Along the way it skips many diagonals.
  SUBPEL_SEARCH_METHODS subpel_search_method;

  // Maximum number of steps in logarithmic subpel search before giving up.
  int subpel_iters_per_step;

  // Control when to stop subpel search
  int subpel_force_stop;

  // This variable sets the step_param used in full pel motion search.
  int fullpel_search_step_param;
} MV_SPEED_FEATURES;

#define MAX_MESH_STEP 4

typedef struct MESH_PATTERN {
  int range;
  int interval;
} MESH_PATTERN;

#if CONFIG_GLOBAL_MOTION
typedef enum {
  GM_FULL_SEARCH,
  GM_REDUCED_REF_SEARCH,
  GM_DISABLE_SEARCH
} GM_SEARCH_TYPE;
#endif  // CONFIG_GLOBAL_MOTION

typedef struct SPEED_FEATURES {
  MV_SPEED_FEATURES mv;

  // Frame level coding parameter update
  int frame_parameter_update;

  RECODE_LOOP_TYPE recode_loop;

  // Trellis (dynamic programming) optimization of quantized values (+1, 0).
  int optimize_coefficients;

  // Always set to 0. If on it enables 0 cost background transmission
  // (except for the initial transmission of the segmentation). The feature is
  // disabled because the addition of very large block sizes make the
  // backgrounds very to cheap to encode, and the segmentation we have
  // adds overhead.
  int static_segmentation;

  // If 1 we iterate finding a best reference for 2 ref frames together - via
  // a log search that iterates 4 times (check around mv for last for best
  // error of combined predictor then check around mv for alt). If 0 we
  // we just use the best motion vector found for each frame by itself.
  BLOCK_SIZE comp_inter_joint_search_thresh;

  // This variable is used to cap the maximum number of times we skip testing a
  // mode to be evaluated. A high value means we will be faster.
  int adaptive_rd_thresh;

  // Coefficient probability model approximation step size
  int coeff_prob_appx_step;

  // The threshold is to determine how slow the motino is, it is used when
  // use_lastframe_partitioning is set to LAST_FRAME_PARTITION_LOW_MOTION
  MOTION_THRESHOLD lf_motion_threshold;

  // Determine which method we use to determine transform size. We can choose
  // between options like full rd, largest for prediction size, largest
  // for intra and model coefs for the rest.
  TX_SIZE_SEARCH_METHOD tx_size_search_method;

  // After looking at the first set of modes (set by index here), skip
  // checking modes for reference frames that don't match the reference frame
  // of the best so far.
  int mode_skip_start;

  PARTITION_SEARCH_TYPE partition_search_type;

  TX_TYPE_SEARCH tx_type_search;

  // Used if partition_search_type = FIXED_SIZE_PARTITION
  BLOCK_SIZE always_this_block_size;

  // Skip rectangular partition test when partition type none gives better
  // rd than partition type split.
  int less_rectangular_check;

  // Disable testing non square partitions. (eg 16x32)
  int use_square_partition_only;

  // Sets min and max partition sizes for this superblock based on the
  // same superblock in last encoded frame, and the left and above neighbor.
  AUTO_MIN_MAX_MODE auto_min_max_partition_size;
  // Ensures the rd based auto partition search will always
  // go down at least to the specified level.
  BLOCK_SIZE rd_auto_partition_min_limit;

  // Min and max partition size we enable (block_size) as per auto
  // min max, but also used by adjust partitioning, and pick_partitioning.
  BLOCK_SIZE default_min_partition_size;
  BLOCK_SIZE default_max_partition_size;

  // Whether or not we allow partitions one smaller or one greater than the last
  // frame's partitioning. Only used if use_lastframe_partitioning is set.
  int adjust_partitioning_from_last_frame;

  // How frequently we re do the partitioning from scratch. Only used if
  // use_lastframe_partitioning is set.
  int last_partitioning_redo_frequency;

  // Disables sub 8x8 blocksizes in different scenarios: Choices are to disable
  // it always, to allow it for only Last frame and Intra, disable it for all
  // inter modes or to enable it always.
  int disable_split_mask;

  // TODO(jingning): combine the related motion search speed features
  // This allows us to use motion search at other sizes as a starting
  // point for this motion search and limits the search range around it.
  int adaptive_motion_search;

  // Flag for allowing some use of exhaustive searches;
  int allow_exhaustive_searches;

  // Threshold for allowing exhaistive motion search.
  int exhaustive_searches_thresh;

  // Maximum number of exhaustive searches for a frame.
  int max_exaustive_pct;

  // Pattern to be used for any exhaustive mesh searches.
  MESH_PATTERN mesh_patterns[MAX_MESH_STEP];

  int schedule_mode_search;

  // Allows sub 8x8 modes to use the prediction filter that was determined
  // best for 8x8 mode. If set to 0 we always re check all the filters for
  // sizes less than 8x8, 1 means we check all filter modes if no 8x8 filter
  // was selected, and 2 means we use 8 tap if no 8x8 filter mode was selected.
  int adaptive_pred_interp_filter;

  // Adaptive prediction mode search
  int adaptive_mode_search;

  // Chessboard pattern prediction filter type search
  int cb_pred_filter_search;

  int cb_partition_search;

  int alt_ref_search_fp;

  // Use finer quantizer in every other few frames that run variable block
  // partition type search.
  int force_frame_boost;

  // Maximally allowed base quantization index fluctuation.
  int max_delta_qindex;

  // Implements various heuristics to skip searching modes
  // The heuristics selected are based on  flags
  // defined in the MODE_SEARCH_SKIP_HEURISTICS enum
  unsigned int mode_search_skip_flags;

  // A source variance threshold below which filter search is disabled
  // Choose a very large value (UINT_MAX) to use 8-tap always
  unsigned int disable_filter_search_var_thresh;

  // A source variance threshold below which wedge search is disabled
  unsigned int disable_wedge_search_var_thresh;

  // Whether fast wedge sign estimate is used
  int fast_wedge_sign_estimate;

  // These bit masks allow you to enable or disable intra modes for each
  // transform size separately.
  int intra_y_mode_mask[TX_SIZES];
  int intra_uv_mode_mask[TX_SIZES];

  // This variable enables an early break out of mode testing if the model for
  // rd built from the prediction signal indicates a value that's much
  // higher than the best rd we've seen so far.
  int use_rd_breakout;

  // This feature controls how the loop filter level is determined.
  LPF_PICK_METHOD lpf_pick;

  // This feature limits the number of coefficients updates we actually do
  // by only looking at counts from 1/2 the bands.
  FAST_COEFF_UPDATE use_fast_coef_updates;

  // A binary mask indicating if NEARESTMV, NEARMV, ZEROMV, NEWMV
  // modes are used in order from LSB to MSB for each BLOCK_SIZE.
  int inter_mode_mask[BLOCK_SIZES_ALL];

  // This feature controls whether we do the expensive context update and
  // calculation in the rd coefficient costing loop.
  int use_fast_coef_costing;

  // This feature controls the tolerence vs target used in deciding whether to
  // recode a frame. It has no meaning if recode is disabled.
  int recode_tolerance;

  // This variable controls the maximum block size where intra blocks can be
  // used in inter frames.
  // TODO(aconverse): Fold this into one of the other many mode skips
  BLOCK_SIZE max_intra_bsize;

  // The frequency that we check if
  // FIXED_PARTITION search type should be used.
  int search_type_check_frequency;

  // When partition is pre-set, the inter prediction result from pick_inter_mode
  // can be reused in final block encoding process. It is enabled only for real-
  // time mode speed 6.
  int reuse_inter_pred_sby;

  // default interp filter choice
  InterpFilter default_interp_filter;

  // Early termination in transform size search, which only applies while
  // tx_size_search_method is USE_FULL_RD.
  int tx_size_search_breakout;

  // adaptive interp_filter search to allow skip of certain filter types.
  int adaptive_interp_filter_search;

  // mask for skip evaluation of certain interp_filter type.
  INTERP_FILTER_MASK interp_filter_search_mask;

  // Partition search early breakout thresholds.
  int64_t partition_search_breakout_dist_thr;
  int partition_search_breakout_rate_thr;

  // Allow skipping partition search for still image frame
  int allow_partition_search_skip;

  // Fast approximation of av1_model_rd_from_var_lapndz
  int simple_model_rd_from_var;

  // Do sub-pixel search in up-sampled reference frames
  int use_upsampled_references;

  // Whether to compute distortion in the image domain (slower but
  // more accurate), or in the transform domain (faster but less acurate).
  int use_transform_domain_distortion;

#if CONFIG_GLOBAL_MOTION
  GM_SEARCH_TYPE gm_search_type;
#endif  // CONFIG_GLOBAL_MOTION
} SPEED_FEATURES;

struct AV1_COMP;

void av1_set_speed_features_framesize_independent(struct AV1_COMP *cpi);
void av1_set_speed_features_framesize_dependent(struct AV1_COMP *cpi);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // AV1_ENCODER_SPEED_FEATURES_H_
