/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#ifndef VP9_ENCODER_VP9_ONYX_INT_H_
#define VP9_ENCODER_VP9_ONYX_INT_H_

#include <stdio.h>
#include "./vpx_config.h"
#include "vp9/common/vp9_onyx.h"
#include "vp9/encoder/vp9_treewriter.h"
#include "vp9/encoder/vp9_tokenize.h"
#include "vp9/common/vp9_onyxc_int.h"
#include "vp9/encoder/vp9_variance.h"
#include "vp9/encoder/vp9_encodemb.h"
#include "vp9/encoder/vp9_quantize.h"
#include "vp9/common/vp9_entropy.h"
#include "vp9/common/vp9_entropymode.h"
#include "vpx_ports/mem.h"
#include "vpx/internal/vpx_codec_internal.h"
#include "vp9/encoder/vp9_mcomp.h"
#include "vp9/common/vp9_findnearmv.h"
#include "vp9/encoder/vp9_lookahead.h"

#define DISABLE_RC_LONG_TERM_MEM 0

// #define MODE_TEST_HIT_STATS

// #define SPEEDSTATS 1
#if CONFIG_MULTIPLE_ARF
// Set MIN_GF_INTERVAL to 1 for the full decomposition.
#define MIN_GF_INTERVAL             2
#else
#define MIN_GF_INTERVAL             4
#endif
#define DEFAULT_GF_INTERVAL         7

#define KEY_FRAME_CONTEXT 5

#define MAX_MODES 30
#define MAX_REFS  6

#define MIN_THRESHMULT  32
#define MAX_THRESHMULT  512

#define GF_ZEROMV_ZBIN_BOOST 0
#define LF_ZEROMV_ZBIN_BOOST 0
#define MV_ZBIN_BOOST        0
#define SPLIT_MV_ZBIN_BOOST  0
#define INTRA_ZBIN_BOOST     0

typedef struct {
  int nmvjointcost[MV_JOINTS];
  int nmvcosts[2][MV_VALS];
  int nmvcosts_hp[2][MV_VALS];

  vp9_prob segment_pred_probs[PREDICTION_PROBS];

  unsigned char *last_frame_seg_map_copy;

  // 0 = Intra, Last, GF, ARF
  signed char last_ref_lf_deltas[MAX_REF_LF_DELTAS];
  // 0 = ZERO_MV, MV
  signed char last_mode_lf_deltas[MAX_MODE_LF_DELTAS];

  int inter_mode_counts[INTER_MODE_CONTEXTS][INTER_MODES - 1][2];
  FRAME_CONTEXT fc;
} CODING_CONTEXT;

typedef struct {
  double frame;
  double intra_error;
  double coded_error;
  double sr_coded_error;
  double ssim_weighted_pred_err;
  double pcnt_inter;
  double pcnt_motion;
  double pcnt_second_ref;
  double pcnt_neutral;
  double MVr;
  double mvr_abs;
  double MVc;
  double mvc_abs;
  double MVrv;
  double MVcv;
  double mv_in_out_count;
  double new_mv_count;
  double duration;
  double count;
} FIRSTPASS_STATS;

typedef struct {
  int frames_so_far;
  double frame_intra_error;
  double frame_coded_error;
  double frame_pcnt_inter;
  double frame_pcnt_motion;
  double frame_mvr;
  double frame_mvr_abs;
  double frame_mvc;
  double frame_mvc_abs;
} ONEPASS_FRAMESTATS;

typedef struct {
  struct {
    int err;
    union {
      int_mv mv;
      MB_PREDICTION_MODE mode;
    } m;
  } ref[MAX_REF_FRAMES];
} MBGRAPH_MB_STATS;

typedef struct {
  MBGRAPH_MB_STATS *mb_stats;
} MBGRAPH_FRAME_STATS;

// This enumerator type needs to be kept aligned with the mode order in
// const MODE_DEFINITION vp9_mode_order[MAX_MODES] used in the rd code.
typedef enum {
  THR_NEARESTMV,
  THR_NEARESTA,
  THR_NEARESTG,

  THR_DC,

  THR_NEWMV,
  THR_NEWA,
  THR_NEWG,

  THR_NEARMV,
  THR_NEARA,
  THR_COMP_NEARESTLA,
  THR_COMP_NEARESTGA,

  THR_TM,

  THR_COMP_NEARLA,
  THR_COMP_NEWLA,
  THR_NEARG,
  THR_COMP_NEARGA,
  THR_COMP_NEWGA,

  THR_ZEROMV,
  THR_ZEROG,
  THR_ZEROA,
  THR_COMP_ZEROLA,
  THR_COMP_ZEROGA,

  THR_H_PRED,
  THR_V_PRED,
  THR_D135_PRED,
  THR_D207_PRED,
  THR_D153_PRED,
  THR_D63_PRED,
  THR_D117_PRED,
  THR_D45_PRED,
} THR_MODES;

typedef enum {
  THR_LAST,
  THR_GOLD,
  THR_ALTR,
  THR_COMP_LA,
  THR_COMP_GA,
  THR_INTRA,
} THR_MODES_SUB8X8;

typedef enum {
  DIAMOND = 0,
  NSTEP = 1,
  HEX = 2,
  BIGDIA = 3,
  SQUARE = 4
} SEARCH_METHODS;

typedef enum {
  USE_FULL_RD = 0,
  USE_LARGESTINTRA,
  USE_LARGESTINTRA_MODELINTER,
  USE_LARGESTALL
} TX_SIZE_SEARCH_METHOD;

typedef enum {
  // Values should be powers of 2 so that they can be selected as bits of
  // an integer flags field

  // terminate search early based on distortion so far compared to
  // qp step, distortion in the neighborhood of the frame, etc.
  FLAG_EARLY_TERMINATE = 1,

  // skips comp inter modes if the best so far is an intra mode
  FLAG_SKIP_COMP_BESTINTRA = 2,

  // skips comp inter modes if the best single intermode so far does
  // not have the same reference as one of the two references being
  // tested
  FLAG_SKIP_COMP_REFMISMATCH = 4,

  // skips oblique intra modes if the best so far is an inter mode
  FLAG_SKIP_INTRA_BESTINTER = 8,

  // skips oblique intra modes  at angles 27, 63, 117, 153 if the best
  // intra so far is not one of the neighboring directions
  FLAG_SKIP_INTRA_DIRMISMATCH = 16,

  // skips intra modes other than DC_PRED if the source variance
  // is small
  FLAG_SKIP_INTRA_LOWVAR = 32,
} MODE_SEARCH_SKIP_LOGIC;

typedef enum {
  SUBPEL_ITERATIVE = 0,
  SUBPEL_TREE = 1,
  // Other methods to come
} SUBPEL_SEARCH_METHODS;

#define ALL_INTRA_MODES 0x3FF
#define INTRA_DC_ONLY 0x01
#define INTRA_DC_TM ((1 << TM_PRED) | (1 << DC_PRED))
#define INTRA_DC_H_V ((1 << DC_PRED) | (1 << V_PRED) | (1 << H_PRED))
#define INTRA_DC_TM_H_V (INTRA_DC_TM | (1 << V_PRED) | (1 << H_PRED))

typedef enum {
  LAST_FRAME_PARTITION_OFF = 0,
  LAST_FRAME_PARTITION_LOW_MOTION = 1,
  LAST_FRAME_PARTITION_ALL = 2
} LAST_FRAME_PARTITION_METHOD;

typedef struct {
  int RD;
  SEARCH_METHODS search_method;
  int auto_filter;
  int recode_loop;
  SUBPEL_SEARCH_METHODS subpel_search_method;
  int subpel_iters_per_step;
  int thresh_mult[MAX_MODES];
  int thresh_mult_sub8x8[MAX_REFS];
  int max_step_search_steps;
  int reduce_first_step_size;
  int auto_mv_step_size;
  int optimize_coefficients;
  int static_segmentation;
  int comp_inter_joint_search_thresh;
  int adaptive_rd_thresh;
  int skip_encode_sb;
  int skip_encode_frame;
  LAST_FRAME_PARTITION_METHOD use_lastframe_partitioning;
  TX_SIZE_SEARCH_METHOD tx_size_search_method;
  int use_lp32x32fdct;
  int use_avoid_tested_higherror;
  int use_one_partition_size_always;
  int less_rectangular_check;
  int use_square_partition_only;
  int mode_skip_start;
  int reference_masking;
  BLOCK_SIZE always_this_block_size;
  int auto_min_max_partition_size;
  BLOCK_SIZE min_partition_size;
  BLOCK_SIZE max_partition_size;
  int adjust_partitioning_from_last_frame;
  int last_partitioning_redo_frequency;
  int disable_split_mask;
  int using_small_partition_info;
  // TODO(jingning): combine the related motion search speed features
  int adaptive_motion_search;

  // Implements various heuristics to skip searching modes
  // The heuristics selected are based on  flags
  // defined in the MODE_SEARCH_SKIP_HEURISTICS enum
  unsigned int mode_search_skip_flags;
  // A source variance threshold below which the split mode is disabled
  unsigned int disable_split_var_thresh;
  // A source variance threshold below which filter search is disabled
  // Choose a very large value (UINT_MAX) to use 8-tap always
  unsigned int disable_filter_search_var_thresh;
  int intra_y_mode_mask[TX_SIZES];
  int intra_uv_mode_mask[TX_SIZES];
  int use_rd_breakout;
  int use_uv_intra_rd_estimate;
  int use_fast_lpf_pick;
  int use_fast_coef_updates;  // 0: 2-loop, 1: 1-loop, 2: 1-loop reduced
} SPEED_FEATURES;

typedef struct VP9_COMP {
  DECLARE_ALIGNED(16, int16_t, y_quant[QINDEX_RANGE][8]);
  DECLARE_ALIGNED(16, int16_t, y_quant_shift[QINDEX_RANGE][8]);
  DECLARE_ALIGNED(16, int16_t, y_zbin[QINDEX_RANGE][8]);
  DECLARE_ALIGNED(16, int16_t, y_round[QINDEX_RANGE][8]);

  DECLARE_ALIGNED(16, int16_t, uv_quant[QINDEX_RANGE][8]);
  DECLARE_ALIGNED(16, int16_t, uv_quant_shift[QINDEX_RANGE][8]);
  DECLARE_ALIGNED(16, int16_t, uv_zbin[QINDEX_RANGE][8]);
  DECLARE_ALIGNED(16, int16_t, uv_round[QINDEX_RANGE][8]);

#if CONFIG_ALPHA
  DECLARE_ALIGNED(16, int16_t, a_quant[QINDEX_RANGE][8]);
  DECLARE_ALIGNED(16, int16_t, a_quant_shift[QINDEX_RANGE][8]);
  DECLARE_ALIGNED(16, int16_t, a_zbin[QINDEX_RANGE][8]);
  DECLARE_ALIGNED(16, int16_t, a_round[QINDEX_RANGE][8]);
#endif

  MACROBLOCK mb;
  VP9_COMMON common;
  VP9_CONFIG oxcf;
  struct rdcost_block_args rdcost_stack;
  struct lookahead_ctx    *lookahead;
  struct lookahead_entry  *source;
#if CONFIG_MULTIPLE_ARF
  struct lookahead_entry  *alt_ref_source[NUM_REF_FRAMES];
#else
  struct lookahead_entry  *alt_ref_source;
#endif

  YV12_BUFFER_CONFIG *Source;
  YV12_BUFFER_CONFIG *un_scaled_source;
  YV12_BUFFER_CONFIG scaled_source;

  unsigned int frames_till_alt_ref_frame;
  int source_alt_ref_pending;
  int source_alt_ref_active;

  int is_src_frame_alt_ref;

  int gold_is_last;  // gold same as last frame ( short circuit gold searches)
  int alt_is_last;  // Alt same as last ( short circuit altref search)
  int gold_is_alt;  // don't do both alt and gold search ( just do gold).

  int scaled_ref_idx[3];
  int lst_fb_idx;
  int gld_fb_idx;
  int alt_fb_idx;

  int current_layer;
  int use_svc;

#if CONFIG_MULTIPLE_ARF
  int alt_ref_fb_idx[NUM_REF_FRAMES - 3];
#endif
  int refresh_last_frame;
  int refresh_golden_frame;
  int refresh_alt_ref_frame;
  YV12_BUFFER_CONFIG last_frame_uf;

  TOKENEXTRA *tok;
  unsigned int tok_count[4][1 << 6];


  unsigned int frames_since_key;
  unsigned int key_frame_frequency;
  unsigned int this_key_frame_forced;
  unsigned int next_key_frame_forced;
#if CONFIG_MULTIPLE_ARF
  // Position within a frame coding order (including any additional ARF frames).
  unsigned int sequence_number;
  // Next frame in naturally occurring order that has not yet been coded.
  int next_frame_in_order;
#endif

  // Ambient reconstruction err target for force key frames
  int ambient_err;

  unsigned int mode_chosen_counts[MAX_MODES];
  unsigned int sub8x8_mode_chosen_counts[MAX_REFS];
  int64_t mode_skip_mask;
  int ref_frame_mask;
  int set_ref_frame_mask;

  int rd_threshes[MAX_SEGMENTS][BLOCK_SIZES][MAX_MODES];
  int rd_thresh_freq_fact[BLOCK_SIZES][MAX_MODES];
  int rd_thresh_sub8x8[MAX_SEGMENTS][BLOCK_SIZES][MAX_REFS];
  int rd_thresh_freq_sub8x8[BLOCK_SIZES][MAX_REFS];

  int64_t rd_comp_pred_diff[NB_PREDICTION_TYPES];
  int64_t rd_prediction_type_threshes[4][NB_PREDICTION_TYPES];
  unsigned int intra_inter_count[INTRA_INTER_CONTEXTS][2];
  unsigned int comp_inter_count[COMP_INTER_CONTEXTS][2];
  unsigned int single_ref_count[REF_CONTEXTS][2][2];
  unsigned int comp_ref_count[REF_CONTEXTS][2];

  int64_t rd_tx_select_diff[TX_MODES];
  // FIXME(rbultje) can this overflow?
  int rd_tx_select_threshes[4][TX_MODES];

  int64_t rd_filter_diff[SWITCHABLE_FILTER_CONTEXTS];
  int64_t rd_filter_threshes[4][SWITCHABLE_FILTER_CONTEXTS];
  int64_t rd_filter_cache[SWITCHABLE_FILTER_CONTEXTS];

  int RDMULT;
  int RDDIV;

  CODING_CONTEXT coding_context;

  // Rate targetting variables
  int this_frame_target;
  int projected_frame_size;
  int last_q[2];                   // Separate values for Intra/Inter
  int last_boosted_qindex;         // Last boosted GF/KF/ARF q

  double rate_correction_factor;
  double key_frame_rate_correction_factor;
  double gf_rate_correction_factor;

  unsigned int frames_since_golden;
  int frames_till_gf_update_due;  // Count down till next GF

  int gf_overspend_bits;  // cumulative bits overspent because of GF boost

  int non_gf_bitrate_adjustment;  // Following GF to recover extra bits spent

  int kf_overspend_bits;  // Bits spent on key frames to be recovered on inters
  int kf_bitrate_adjustment;  // number of bits to recover on each inter frame.
  int max_gf_interval;
  int baseline_gf_interval;
  int active_arnr_frames;           // <= cpi->oxcf.arnr_max_frames
  int active_arnr_strength;         // <= cpi->oxcf.arnr_max_strength

  int64_t key_frame_count;
  int prior_key_frame_distance[KEY_FRAME_CONTEXT];
  int per_frame_bandwidth;  // Current section per frame bandwidth target
  int av_per_frame_bandwidth;  // Average frame size target for clip
  int min_frame_bandwidth;  // Minimum allocation used for any frame
  int inter_frame_target;
  double output_framerate;
  int64_t last_time_stamp_seen;
  int64_t last_end_time_stamp_seen;
  int64_t first_time_stamp_ever;

  int ni_av_qi;
  int ni_tot_qi;
  int ni_frames;
  int avg_frame_qindex;
  double tot_q;
  double avg_q;

  int zbin_mode_boost;
  int zbin_mode_boost_enabled;

  int64_t total_byte_count;

  int buffered_mode;

  int buffer_level;
  int bits_off_target;

  int rolling_target_bits;
  int rolling_actual_bits;

  int long_rolling_target_bits;
  int long_rolling_actual_bits;

  int64_t total_actual_bits;
  int total_target_vs_actual;        // debug stats

  int worst_quality;
  int active_worst_quality;
  int best_quality;
  int active_best_quality;

  int cq_target_quality;

  int y_mode_count[4][INTRA_MODES];
  int y_uv_mode_count[INTRA_MODES][INTRA_MODES];
  unsigned int partition_count[PARTITION_CONTEXTS][PARTITION_TYPES];

  nmv_context_counts NMVcount;

  vp9_coeff_count coef_counts[TX_SIZES][BLOCK_TYPES];
  vp9_coeff_probs_model frame_coef_probs[TX_SIZES][BLOCK_TYPES];
  vp9_coeff_stats frame_branch_ct[TX_SIZES][BLOCK_TYPES];

  int gfu_boost;
  int last_boost;
  int kf_boost;
  int kf_zeromotion_pct;
  int gf_zeromotion_pct;

  int64_t target_bandwidth;
  struct vpx_codec_pkt_list  *output_pkt_list;

#if 0
  // Experimental code for lagged and one pass
  ONEPASS_FRAMESTATS one_pass_frame_stats[MAX_LAG_BUFFERS];
  int one_pass_frame_index;
#endif
  MBGRAPH_FRAME_STATS mbgraph_stats[MAX_LAG_BUFFERS];
  int mbgraph_n_frames;             // number of frames filled in the above
  int static_mb_pct;                // % forced skip mbs by segmentation
  int seg0_progress, seg0_idx, seg0_cnt;

  int decimation_factor;
  int decimation_count;

  // for real time encoding
  int speed;
  int compressor_speed;

  int auto_worst_q;
  int cpu_used;
  int pass;

  vp9_prob last_skip_false_probs[3][MBSKIP_CONTEXTS];
  int last_skip_probs_q[3];

  int ref_frame_flags;

  SPEED_FEATURES sf;
  int error_bins[1024];

  unsigned int max_mv_magnitude;
  int mv_step_param;

  unsigned char *segmentation_map;

  // segment threashold for encode breakout
  int  segment_encode_breakout[MAX_SEGMENTS];

  unsigned char *active_map;
  unsigned int active_map_enabled;

  fractional_mv_step_fp *find_fractional_mv_step;
  fractional_mv_step_comp_fp *find_fractional_mv_step_comp;
  vp9_full_search_fn_t full_search_sad;
  vp9_refining_search_fn_t refining_search_sad;
  vp9_diamond_search_fn_t diamond_search_sad;
  vp9_variance_fn_ptr_t fn_ptr[BLOCK_SIZES];
  uint64_t time_receive_data;
  uint64_t time_compress_data;
  uint64_t time_pick_lpf;
  uint64_t time_encode_sb_row;

  struct twopass_rc {
    unsigned int section_intra_rating;
    unsigned int next_iiratio;
    unsigned int this_iiratio;
    FIRSTPASS_STATS total_stats;
    FIRSTPASS_STATS this_frame_stats;
    FIRSTPASS_STATS *stats_in, *stats_in_end, *stats_in_start;
    FIRSTPASS_STATS total_left_stats;
    int first_pass_done;
    int64_t bits_left;
    int64_t clip_bits_total;
    double avg_iiratio;
    double modified_error_total;
    double modified_error_used;
    double modified_error_left;
    double kf_intra_err_min;
    double gf_intra_err_min;
    int frames_to_key;
    int maxq_max_limit;
    int maxq_min_limit;
    int static_scene_max_gf_interval;
    int kf_bits;
    // Remaining error from uncoded frames in a gf group. Two pass use only
    int64_t gf_group_error_left;

    // Projected total bits available for a key frame group of frames
    int64_t kf_group_bits;

    // Error score of frames still to be coded in kf group
    int64_t kf_group_error_left;

    // Projected Bits available for a group of frames including 1 GF or ARF
    int64_t gf_group_bits;
    // Bits for the golden frame or ARF - 2 pass only
    int gf_bits;
    int alt_extra_bits;

    int sr_update_lag;
    double est_max_qcorrection_factor;
  } twopass;

  YV12_BUFFER_CONFIG alt_ref_buffer;
  YV12_BUFFER_CONFIG *frames[MAX_LAG_BUFFERS];
  int fixed_divide[512];

#if CONFIG_INTERNAL_STATS
  int    count;
  double total_y;
  double total_u;
  double total_v;
  double total;
  double total_sq_error;
  double totalp_y;
  double totalp_u;
  double totalp_v;
  double totalp;
  double total_sq_error2;
  int    bytes;
  double summed_quality;
  double summed_weights;
  double summedp_quality;
  double summedp_weights;
  unsigned int tot_recode_hits;


  double total_ssimg_y;
  double total_ssimg_u;
  double total_ssimg_v;
  double total_ssimg_all;

  int b_calculate_ssimg;
#endif
  int b_calculate_psnr;

  // Per MB activity measurement
  unsigned int activity_avg;
  unsigned int *mb_activity_map;
  int *mb_norm_activity_map;
  int output_partition;

  /* force next frame to intra when kf_auto says so */
  int force_next_frame_intra;

  int droppable;

  int dummy_packing;    /* flag to indicate if packing is dummy */

  unsigned int switchable_interp_count[SWITCHABLE_FILTER_CONTEXTS]
                                      [SWITCHABLE_FILTERS];

  unsigned int tx_stepdown_count[TX_SIZES];

  int initial_width;
  int initial_height;

  int number_spatial_layers;
  int enable_encode_breakout;   // Default value is 1. From first pass stats,
                                // encode_breakout may be disabled.

#if CONFIG_MULTIPLE_ARF
  // ARF tracking variables.
  int multi_arf_enabled;
  unsigned int frame_coding_order_period;
  unsigned int new_frame_coding_order_period;
  int frame_coding_order[MAX_LAG_BUFFERS * 2];
  int arf_buffer_idx[MAX_LAG_BUFFERS * 3 / 2];
  int arf_weight[MAX_LAG_BUFFERS];
  int arf_buffered;
  int this_frame_weight;
  int max_arf_level;
#endif

#ifdef ENTROPY_STATS
  int64_t mv_ref_stats[INTER_MODE_CONTEXTS][INTER_MODES - 1][2];
#endif


#ifdef MODE_TEST_HIT_STATS
  // Debug / test stats
  int64_t mode_test_hits[BLOCK_SIZES];
#endif

  /* Y,U,V,(A) */
  ENTROPY_CONTEXT *above_context[MAX_MB_PLANE];
  ENTROPY_CONTEXT left_context[MAX_MB_PLANE][16];

  PARTITION_CONTEXT *above_seg_context;
  PARTITION_CONTEXT left_seg_context[8];
} VP9_COMP;

static int get_ref_frame_idx(VP9_COMP *cpi, MV_REFERENCE_FRAME ref_frame) {
  if (ref_frame == LAST_FRAME) {
    return cpi->lst_fb_idx;
  } else if (ref_frame == GOLDEN_FRAME) {
    return cpi->gld_fb_idx;
  } else {
    return cpi->alt_fb_idx;
  }
}

static int get_scale_ref_frame_idx(VP9_COMP *cpi,
                                   MV_REFERENCE_FRAME ref_frame) {
  if (ref_frame == LAST_FRAME) {
    return 0;
  } else if (ref_frame == GOLDEN_FRAME) {
    return 1;
  } else {
    return 2;
  }
}

void vp9_encode_frame(VP9_COMP *cpi);

void vp9_pack_bitstream(VP9_COMP *cpi, unsigned char *dest,
                        unsigned long *size);

void vp9_activity_masking(VP9_COMP *cpi, MACROBLOCK *x);

void vp9_set_speed_features(VP9_COMP *cpi);

int vp9_calc_ss_err(YV12_BUFFER_CONFIG *source, YV12_BUFFER_CONFIG *dest);

void vp9_alloc_compressor_data(VP9_COMP *cpi);

int vp9_compute_qdelta(VP9_COMP *cpi, double qstart, double qtarget);

static int get_token_alloc(int mb_rows, int mb_cols) {
  return mb_rows * mb_cols * (48 * 16 + 4);
}

#endif  // VP9_ENCODER_VP9_ONYX_INT_H_
