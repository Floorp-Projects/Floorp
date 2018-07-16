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
              (1 << D135_PRED) | (1 << D113_PRED) | (1 << D157_PRED) |
              (1 << D203_PRED) | (1 << D67_PRED) | (1 << SMOOTH_PRED) |
              (1 << SMOOTH_V_PRED) | (1 << SMOOTH_H_PRED) | (1 << PAETH_PRED),
  UV_INTRA_ALL =
      (1 << UV_DC_PRED) | (1 << UV_V_PRED) | (1 << UV_H_PRED) |
      (1 << UV_D45_PRED) | (1 << UV_D135_PRED) | (1 << UV_D113_PRED) |
      (1 << UV_D157_PRED) | (1 << UV_D203_PRED) | (1 << UV_D67_PRED) |
      (1 << UV_SMOOTH_PRED) | (1 << UV_SMOOTH_V_PRED) |
      (1 << UV_SMOOTH_H_PRED) | (1 << UV_PAETH_PRED) | (1 << UV_CFL_PRED),
  UV_INTRA_DC = (1 << UV_DC_PRED),
  UV_INTRA_DC_CFL = (1 << UV_DC_PRED) | (1 << UV_CFL_PRED),
  UV_INTRA_DC_TM = (1 << UV_DC_PRED) | (1 << UV_PAETH_PRED),
  UV_INTRA_DC_PAETH_CFL =
      (1 << UV_DC_PRED) | (1 << UV_PAETH_PRED) | (1 << UV_CFL_PRED),
  UV_INTRA_DC_H_V = (1 << UV_DC_PRED) | (1 << UV_V_PRED) | (1 << UV_H_PRED),
  UV_INTRA_DC_H_V_CFL = (1 << UV_DC_PRED) | (1 << UV_V_PRED) |
                        (1 << UV_H_PRED) | (1 << UV_CFL_PRED),
  UV_INTRA_DC_PAETH_H_V = (1 << UV_DC_PRED) | (1 << UV_PAETH_PRED) |
                          (1 << UV_V_PRED) | (1 << UV_H_PRED),
  UV_INTRA_DC_PAETH_H_V_CFL = (1 << UV_DC_PRED) | (1 << UV_PAETH_PRED) |
                              (1 << UV_V_PRED) | (1 << UV_H_PRED) |
                              (1 << UV_CFL_PRED),
  INTRA_DC = (1 << DC_PRED),
  INTRA_DC_TM = (1 << DC_PRED) | (1 << PAETH_PRED),
  INTRA_DC_H_V = (1 << DC_PRED) | (1 << V_PRED) | (1 << H_PRED),
  INTRA_DC_PAETH_H_V =
      (1 << DC_PRED) | (1 << PAETH_PRED) | (1 << V_PRED) | (1 << H_PRED)
};

enum {
  INTER_ALL = (1 << NEARESTMV) | (1 << NEARMV) | (1 << GLOBALMV) |
              (1 << NEWMV) | (1 << NEAREST_NEARESTMV) | (1 << NEAR_NEARMV) |
              (1 << NEW_NEWMV) | (1 << NEAREST_NEWMV) | (1 << NEAR_NEWMV) |
              (1 << NEW_NEARMV) | (1 << NEW_NEARESTMV) | (1 << GLOBAL_GLOBALMV),
  INTER_NEAREST = (1 << NEARESTMV) | (1 << NEAREST_NEARESTMV) |
                  (1 << NEW_NEARESTMV) | (1 << NEAREST_NEWMV),
  INTER_NEAREST_NEW = (1 << NEARESTMV) | (1 << NEWMV) |
                      (1 << NEAREST_NEARESTMV) | (1 << NEW_NEWMV) |
                      (1 << NEW_NEARESTMV) | (1 << NEAREST_NEWMV) |
                      (1 << NEW_NEARMV) | (1 << NEAR_NEWMV),
  INTER_NEAREST_ZERO = (1 << NEARESTMV) | (1 << GLOBALMV) |
                       (1 << NEAREST_NEARESTMV) | (1 << GLOBAL_GLOBALMV) |
                       (1 << NEAREST_NEWMV) | (1 << NEW_NEARESTMV),
  INTER_NEAREST_NEW_ZERO = (1 << NEARESTMV) | (1 << GLOBALMV) | (1 << NEWMV) |
                           (1 << NEAREST_NEARESTMV) | (1 << GLOBAL_GLOBALMV) |
                           (1 << NEW_NEWMV) | (1 << NEW_NEARESTMV) |
                           (1 << NEAREST_NEWMV) | (1 << NEW_NEARMV) |
                           (1 << NEAR_NEWMV),
  INTER_NEAREST_NEAR_NEW = (1 << NEARESTMV) | (1 << NEARMV) | (1 << NEWMV) |
                           (1 << NEAREST_NEARESTMV) | (1 << NEW_NEWMV) |
                           (1 << NEW_NEARESTMV) | (1 << NEAREST_NEWMV) |
                           (1 << NEW_NEARMV) | (1 << NEAR_NEWMV) |
                           (1 << NEAR_NEARMV),
  INTER_NEAREST_NEAR_ZERO = (1 << NEARESTMV) | (1 << NEARMV) | (1 << GLOBALMV) |
                            (1 << NEAREST_NEARESTMV) | (1 << GLOBAL_GLOBALMV) |
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
  TXFM_CODING_SF = 1,
  INTER_PRED_SF = 2,
  INTRA_PRED_SF = 4,
  PARTITION_SF = 8,
  LOOP_FILTER_SF = 16,
  RD_SKIP_SF = 32,
  RESERVE_2_SF = 64,
  RESERVE_3_SF = 128,
} DEV_SPEED_FEATURES;

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
  USE_FAST_RD,
  USE_LARGESTALL,
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
  // eliminates two tx types in each direction
  PRUNE_TWO = 2,
  // adaptively prunes the least perspective tx types out of all 16
  // (tuned to provide negligible quality loss)
  PRUNE_2D_ACCURATE = 3,
  // similar, but applies much more aggressive pruning to get better speed-up
  PRUNE_2D_FAST = 4,
} TX_TYPE_PRUNE_MODE;

typedef struct {
  TX_TYPE_PRUNE_MODE prune_mode;
  int fast_intra_tx_type_search;
  int fast_inter_tx_type_search;

  // Use a skip flag prediction model to detect blocks with skip = 1 early
  // and avoid doing full TX type search for such blocks.
  int use_skip_flag_prediction;

  // Threshold used by the ML based method to predict TX block split decisions.
  int ml_tx_split_thresh;

  // skip remaining transform type search when we found the rdcost of skip is
  // better than applying transform
  int skip_tx_search;
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

typedef enum {
  GM_FULL_SEARCH,
  GM_REDUCED_REF_SEARCH,
  GM_DISABLE_SEARCH
} GM_SEARCH_TYPE;

typedef enum {
  GM_ERRORADV_TR_0,
  GM_ERRORADV_TR_1,
  GM_ERRORADV_TR_2,
  GM_ERRORADV_TR_TYPES,
} GM_ERRORADV_TYPE;

typedef enum {
  NO_TRELLIS_OPT,         // No trellis optimization
  FULL_TRELLIS_OPT,       // Trellis optimization in all stages
  FINAL_PASS_TRELLIS_OPT  // Trellis optimization in only the final encode pass
} TRELLIS_OPT_TYPE;

typedef enum {
  FULL_TXFM_RD,
  LOW_TXFM_RD,
} TXFM_RD_MODEL;

typedef struct SPEED_FEATURES {
  MV_SPEED_FEATURES mv;

  // Frame level coding parameter update
  int frame_parameter_update;

  RECODE_LOOP_TYPE recode_loop;

  // Trellis (dynamic programming) optimization of quantized values
  TRELLIS_OPT_TYPE optimize_coefficients;

  // Global motion warp error threshold
  GM_ERRORADV_TYPE gm_erroradv_type;

  // Always set to 0. If on it enables 0 cost background transmission
  // (except for the initial transmission of the segmentation). The feature is
  // disabled because the addition of very large block sizes make the
  // backgrounds very to cheap to encode, and the segmentation we have
  // adds overhead.
  int static_segmentation;

  // Limit the inter mode tested in the RD loop
  int reduce_inter_modes;

  // Do not compute the global motion parameters for a LAST2_FRAME or
  // LAST3_FRAME if the GOLDEN_FRAME is closer and it has a non identity
  // global model.
  int selective_ref_gm;

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

  // Init search depth for square and rectangular transform partitions.
  // Values:
  // 0 - search full tree, 1: search 1 level, 2: search the highest level only
  int inter_tx_size_search_init_depth_sqr;
  int inter_tx_size_search_init_depth_rect;
  int intra_tx_size_search_init_depth_sqr;
  int intra_tx_size_search_init_depth_rect;
  // If any dimension of a coding block size above 64, always search the
  // largest transform only, since the largest transform block size is 64x64.
  int tx_size_search_lgr_block;

  // After looking at the first set of modes (set by index here), skip
  // checking modes for reference frames that don't match the reference frame
  // of the best so far.
  int mode_skip_start;

  PARTITION_SEARCH_TYPE partition_search_type;

  TX_TYPE_SEARCH tx_type_search;

  // Skip split transform block partition when the collocated bigger block
  // is selected as all zero coefficients.
  int txb_split_cap;

  // Shortcut the transform block partition and type search when the target
  // rdcost is relatively lower.
  // Values are 0 (not used) , or 1 - 2 with progressively increasing
  // aggressiveness
  int adaptive_txb_search_level;

  // Prune level for tx_size_type search for inter based on rd model
  // 0: no pruning
  // 1-2: progressively increasing aggressiveness of pruning
  int model_based_prune_tx_search_level;

  // Model based breakout after interpolation filter search
  // 0: no breakout
  // 1: use model based rd breakout
  int model_based_post_interp_filter_breakout;

  // Used if partition_search_type = FIXED_SIZE_PARTITION
  BLOCK_SIZE always_this_block_size;

  // Drop less likely to be picked reference frames in the RD search.
  // Has three levels for now: 0, 1 and 2, where higher levels prune more
  // aggressively than lower ones. (0 means no pruning).
  int selective_ref_frame;

  // Prune extended partition types search
  // Can take values 0 - 2, 0 referring to no pruning, and 1 - 2 increasing
  // aggressiveness of pruning in order.
  int prune_ext_partition_types_search_level;

  // Use a ML model to prune horz_a, horz_b, vert_a and vert_b partitions.
  int ml_prune_ab_partition;

  int fast_cdef_search;

  // 2-pass coding block partition search
  int two_pass_partition_search;

  // Use the mode decisions made in the initial partition search to prune mode
  // candidates, e.g. ref frames.
  int mode_pruning_based_on_two_pass_partition_search;

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

  // A binary mask indicating if NEARESTMV, NEARMV, GLOBALMV, NEWMV
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

  // If true, sub-pixel search uses the exact convolve function used for final
  // encoding and decoding; otherwise, it uses bilinear interpolation.
  int use_accurate_subpel_search;

  // Whether to compute distortion in the image domain (slower but
  // more accurate), or in the transform domain (faster but less acurate).
  // 0: use image domain
  // 1: use transform domain in tx_type search, and use image domain for
  // RD_STATS
  // 2: use transform domain
  int use_transform_domain_distortion;

  GM_SEARCH_TYPE gm_search_type;

  // Do limited interpolation filter search for dual filters, since best choice
  // usually includes EIGHTTAP_REGULAR.
  int use_fast_interpolation_filter_search;

  // Save results of interpolation_filter_search for a block
  // Check mv and ref_frames before search, if they are same with previous
  // saved results, it can be skipped.
  int skip_repeat_interpolation_filter_search;

  // Use a hash table to store previously computed optimized qcoeffs from
  // expensive calls to optimize_txb.
  int use_hash_based_trellis;

  // flag to drop some ref frames in compound motion search
  int drop_ref;

  // flag to allow skipping intra mode for inter frame prediction
  int skip_intra_in_interframe;

  // Use hash table to store intra(keyframe only) txb transform search results
  // to avoid repeated search on the same residue signal.
  int use_intra_txb_hash;

  // Use hash table to store inter txb transform search results
  // to avoid repeated search on the same residue signal.
  int use_inter_txb_hash;

  // Use hash table to store macroblock RD search results
  // to avoid repeated search on the same residue signal.
  int use_mb_rd_hash;

  // Calculate RD cost before doing optimize_b, and skip if the cost is large.
  int optimize_b_precheck;

  // Use model rd instead of transform search in jnt_comp
  int jnt_comp_fast_tx_search;

  // Skip mv search in jnt_comp
  int jnt_comp_skip_mv_search;

  // Decoder side speed feature to add penalty for use of dual-sgr filters.
  // Takes values 0 - 10, 0 indicating no penalty and each additional level
  // adding a penalty of 1%
  int dual_sgr_penalty_level;

  // Dynamically estimate final rd from prediction error and mode cost
  int inter_mode_rd_model_estimation;
} SPEED_FEATURES;

struct AV1_COMP;

void av1_set_speed_features_framesize_independent(struct AV1_COMP *cpi);
void av1_set_speed_features_framesize_dependent(struct AV1_COMP *cpi);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // AV1_ENCODER_SPEED_FEATURES_H_
