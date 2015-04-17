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
#include "vpx_ports/mem.h"
#include "vpx/internal/vpx_codec_internal.h"

#include "vp9/common/vp9_entropy.h"
#include "vp9/common/vp9_entropymode.h"
#include "vp9/common/vp9_onyx.h"
#include "vp9/common/vp9_onyxc_int.h"

#include "vp9/encoder/vp9_encodemb.h"
#include "vp9/encoder/vp9_firstpass.h"
#include "vp9/encoder/vp9_lookahead.h"
#include "vp9/encoder/vp9_mbgraph.h"
#include "vp9/encoder/vp9_mcomp.h"
#include "vp9/encoder/vp9_quantize.h"
#include "vp9/encoder/vp9_ratectrl.h"
#include "vp9/encoder/vp9_tokenize.h"
#include "vp9/encoder/vp9_treewriter.h"
#include "vp9/encoder/vp9_variance.h"

#ifdef __cplusplus
extern "C" {
#endif

// #define MODE_TEST_HIT_STATS

#if CONFIG_MULTIPLE_ARF
// Set MIN_GF_INTERVAL to 1 for the full decomposition.
#define MIN_GF_INTERVAL             2
#else
#define MIN_GF_INTERVAL             4
#endif
#define DEFAULT_GF_INTERVAL         10
#define DEFAULT_KF_BOOST            2000
#define DEFAULT_GF_BOOST            2000

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

  FRAME_CONTEXT fc;
} CODING_CONTEXT;

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
  NOT_IN_USE = 0,
  RELAXED_NEIGHBORING_MIN_MAX = 1,
  STRICT_NEIGHBORING_MIN_MAX = 2
} AUTO_MIN_MAX_MODE;

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
  SUBPEL_TREE = 0,
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
  // encode_breakout is disabled.
  ENCODE_BREAKOUT_DISABLED = 0,
  // encode_breakout is enabled.
  ENCODE_BREAKOUT_ENABLED = 1,
  // encode_breakout is enabled with small max_thresh limit.
  ENCODE_BREAKOUT_LIMITED = 2
} ENCODE_BREAKOUT_TYPE;

typedef struct {
  // Frame level coding parameter update
  int frame_parameter_update;

  // Motion search method (Diamond, NSTEP, Hex, Big Diamond, Square, etc).
  SEARCH_METHODS search_method;

  RECODE_LOOP_TYPE recode_loop;

  // Subpel_search_method can only be subpel_tree which does a subpixel
  // logarithmic search that keeps stepping at 1/2 pixel units until
  // you stop getting a gain, and then goes on to 1/4 and repeats
  // the same process. Along the way it skips many diagonals.
  SUBPEL_SEARCH_METHODS subpel_search_method;

  // Maximum number of steps in logarithmic subpel search before giving up.
  int subpel_iters_per_step;

  // Control when to stop subpel search
  int subpel_force_stop;

  // Thresh_mult is used to set a threshold for the rd score. A higher value
  // means that we will accept the best mode so far more often. This number
  // is used in combination with the current block size, and thresh_freq_fact
  // to pick a threshold.
  int thresh_mult[MAX_MODES];
  int thresh_mult_sub8x8[MAX_REFS];

  // This parameter controls the number of steps we'll do in a diamond
  // search.
  int max_step_search_steps;

  // This parameter controls which step in the n-step process we start at.
  // It's changed adaptively based on circumstances.
  int reduce_first_step_size;

  // If this is set to 1, we limit the motion search range to 2 times the
  // largest motion vector found in the last frame.
  int auto_mv_step_size;

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
  int comp_inter_joint_search_thresh;

  // This variable is used to cap the maximum number of times we skip testing a
  // mode to be evaluated. A high value means we will be faster.
  int adaptive_rd_thresh;

  // Enables skipping the reconstruction step (idct, recon) in the
  // intermediate steps assuming the last frame didn't have too many intra
  // blocks and the q is less than a threshold.
  int skip_encode_sb;
  int skip_encode_frame;

  // This variable allows us to reuse the last frames partition choices
  // (64x64 v 32x32 etc) for this frame. It can be set to only use the last
  // frame as a starting point in low motion scenes or always use it. If set
  // we use last partitioning_redo frequency to determine how often to redo
  // the partitioning from scratch. Adjust_partitioning_from_last_frame
  // enables us to adjust up or down one partitioning from the last frames
  // partitioning.
  LAST_FRAME_PARTITION_METHOD use_lastframe_partitioning;

  // Determine which method we use to determine transform size. We can choose
  // between options like full rd, largest for prediction size, largest
  // for intra and model coefs for the rest.
  TX_SIZE_SEARCH_METHOD tx_size_search_method;

  // Low precision 32x32 fdct keeps everything in 16 bits and thus is less
  // precise but significantly faster than the non lp version.
  int use_lp32x32fdct;

  // TODO(JBB): remove this as its no longer used.

  // If set partition size will always be always_this_block_size.
  int use_one_partition_size_always;

  // Skip rectangular partition test when partition type none gives better
  // rd than partition type split.
  int less_rectangular_check;

  // Disable testing non square partitions. (eg 16x32)
  int use_square_partition_only;

  // After looking at the first set of modes (set by index here), skip
  // checking modes for reference frames that don't match the reference frame
  // of the best so far.
  int mode_skip_start;

  // TODO(JBB): Remove this.
  int reference_masking;

  // Used in conjunction with use_one_partition_size_always.
  BLOCK_SIZE always_this_block_size;

  // Sets min and max partition sizes for this 64x64 region based on the
  // same 64x64 in last encoded frame, and the left and above neighbor.
  AUTO_MIN_MAX_MODE auto_min_max_partition_size;

  // Min and max partition size we enable (block_size) as per auto
  // min max, but also used by adjust partitioning, and pick_partitioning.
  BLOCK_SIZE min_partition_size;
  BLOCK_SIZE max_partition_size;

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

  // Allows sub 8x8 modes to use the prediction filter that was determined
  // best for 8x8 mode. If set to 0 we always re check all the filters for
  // sizes less than 8x8, 1 means we check all filter modes if no 8x8 filter
  // was selected, and 2 means we use 8 tap if no 8x8 filter mode was selected.
  int adaptive_pred_interp_filter;

  // Implements various heuristics to skip searching modes
  // The heuristics selected are based on  flags
  // defined in the MODE_SEARCH_SKIP_HEURISTICS enum
  unsigned int mode_search_skip_flags;

  // A source variance threshold below which the split mode is disabled
  unsigned int disable_split_var_thresh;

  // A source variance threshold below which filter search is disabled
  // Choose a very large value (UINT_MAX) to use 8-tap always
  unsigned int disable_filter_search_var_thresh;

  // These bit masks allow you to enable or disable intra modes for each
  // transform size separately.
  int intra_y_mode_mask[TX_SIZES];
  int intra_uv_mode_mask[TX_SIZES];

  // This variable enables an early break out of mode testing if the model for
  // rd built from the prediction signal indicates a value that's much
  // higher than the best rd we've seen so far.
  int use_rd_breakout;

  // This enables us to use an estimate for intra rd based on dc mode rather
  // than choosing an actual uv mode in the stage of encoding before the actual
  // final encode.
  int use_uv_intra_rd_estimate;

  // This feature controls how the loop filter level is determined:
  // 0: Try the full image with different values.
  // 1: Try a small portion of the image with different values.
  // 2: Estimate the level based on quantizer and frame type
  int use_fast_lpf_pick;

  // This feature limits the number of coefficients updates we actually do
  // by only looking at counts from 1/2 the bands.
  int use_fast_coef_updates;  // 0: 2-loop, 1: 1-loop, 2: 1-loop reduced

  // This flag controls the use of non-RD mode decision.
  int use_pick_mode;

  // This variable sets the encode_breakout threshold. Currently, it is only
  // enabled in real time mode.
  int encode_breakout_thresh;
} SPEED_FEATURES;

typedef struct {
  RATE_CONTROL rc;
  int target_bandwidth;
  int64_t starting_buffer_level;
  int64_t optimal_buffer_level;
  int64_t maximum_buffer_size;
  double framerate;
  int avg_frame_size;
} LAYER_CONTEXT;

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
  struct lookahead_ctx    *lookahead;
  struct lookahead_entry  *source;
#if CONFIG_MULTIPLE_ARF
  struct lookahead_entry  *alt_ref_source[REF_FRAMES];
#else
  struct lookahead_entry  *alt_ref_source;
#endif

  YV12_BUFFER_CONFIG *Source;
  YV12_BUFFER_CONFIG *un_scaled_source;
  YV12_BUFFER_CONFIG scaled_source;

  unsigned int key_frame_frequency;

  int gold_is_last;  // gold same as last frame ( short circuit gold searches)
  int alt_is_last;  // Alt same as last ( short circuit altref search)
  int gold_is_alt;  // don't do both alt and gold search ( just do gold).

  int scaled_ref_idx[3];
  int lst_fb_idx;
  int gld_fb_idx;
  int alt_fb_idx;

#if CONFIG_MULTIPLE_ARF
  int alt_ref_fb_idx[REF_FRAMES - 3];
#endif
  int refresh_last_frame;
  int refresh_golden_frame;
  int refresh_alt_ref_frame;

  int ext_refresh_frame_flags_pending;
  int ext_refresh_last_frame;
  int ext_refresh_golden_frame;
  int ext_refresh_alt_ref_frame;

  int ext_refresh_frame_context_pending;
  int ext_refresh_frame_context;

  YV12_BUFFER_CONFIG last_frame_uf;

  TOKENEXTRA *tok;
  unsigned int tok_count[4][1 << 6];

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

  int64_t rd_comp_pred_diff[REFERENCE_MODES];
  int64_t rd_prediction_type_threshes[4][REFERENCE_MODES];
  int64_t rd_tx_select_diff[TX_MODES];
  // FIXME(rbultje) can this overflow?
  int rd_tx_select_threshes[4][TX_MODES];

  int64_t rd_filter_diff[SWITCHABLE_FILTER_CONTEXTS];
  int64_t rd_filter_threshes[4][SWITCHABLE_FILTER_CONTEXTS];
  int64_t rd_filter_cache[SWITCHABLE_FILTER_CONTEXTS];
  int64_t mask_filter_rd;

  int RDMULT;
  int RDDIV;

  CODING_CONTEXT coding_context;

  int zbin_mode_boost;
  int zbin_mode_boost_enabled;
  int active_arnr_frames;           // <= cpi->oxcf.arnr_max_frames
  int active_arnr_strength;         // <= cpi->oxcf.arnr_max_strength

  double output_framerate;
  int64_t last_time_stamp_seen;
  int64_t last_end_time_stamp_seen;
  int64_t first_time_stamp_ever;

  RATE_CONTROL rc;

  int cq_target_quality;

  vp9_coeff_count coef_counts[TX_SIZES][PLANE_TYPES];
  vp9_coeff_probs_model frame_coef_probs[TX_SIZES][PLANE_TYPES];
  vp9_coeff_stats frame_branch_ct[TX_SIZES][PLANE_TYPES];

  struct vpx_codec_pkt_list  *output_pkt_list;

  MBGRAPH_FRAME_STATS mbgraph_stats[MAX_LAG_BUFFERS];
  int mbgraph_n_frames;             // number of frames filled in the above
  int static_mb_pct;                // % forced skip mbs by segmentation
  int seg0_progress, seg0_idx, seg0_cnt;

  // for real time encoding
  int speed;

  int cpu_used;
  int pass;

  vp9_prob last_skip_false_probs[3][SKIP_CONTEXTS];
  int last_skip_probs_q[3];

  int ref_frame_flags;

  SPEED_FEATURES sf;

  unsigned int max_mv_magnitude;
  int mv_step_param;

  // Default value is 1. From first pass stats, encode_breakout may be disabled.
  ENCODE_BREAKOUT_TYPE allow_encode_breakout;

  // Get threshold from external input. In real time mode, it can be
  // overwritten according to encoding speed.
  int encode_breakout;

  unsigned char *segmentation_map;

  // segment threashold for encode breakout
  int  segment_encode_breakout[MAX_SEGMENTS];

  unsigned char *complexity_map;

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

  struct twopass_rc twopass;

  YV12_BUFFER_CONFIG alt_ref_buffer;
  YV12_BUFFER_CONFIG *frames[MAX_LAG_BUFFERS];
  int fixed_divide[512];

#if CONFIG_INTERNAL_STATS
  int    count;
  double total_y;
  double total_u;
  double total_v;
  double total;
  uint64_t total_sq_error;
  uint64_t total_samples;

  double totalp_y;
  double totalp_u;
  double totalp_v;
  double totalp;
  uint64_t totalp_sq_error;
  uint64_t totalp_samples;

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

  // Force next frame to intra when kf_auto says so.
  int force_next_frame_intra;

  int droppable;

  int dummy_packing;    /* flag to indicate if packing is dummy */

  unsigned int tx_stepdown_count[TX_SIZES];

  int initial_width;
  int initial_height;

  int use_svc;

  struct svc {
    int spatial_layer_id;
    int temporal_layer_id;
    int number_spatial_layers;
    int number_temporal_layers;
    // Layer context used for rate control in CBR mode, only defined for
    // temporal layers for now.
    LAYER_CONTEXT layer_context[VPX_TS_MAX_LAYERS];
  } svc;

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

  // Y,U,V,(A)
  ENTROPY_CONTEXT *above_context[MAX_MB_PLANE];
  ENTROPY_CONTEXT left_context[MAX_MB_PLANE][16];

  PARTITION_CONTEXT *above_seg_context;
  PARTITION_CONTEXT left_seg_context[8];
} VP9_COMP;

static int get_ref_frame_idx(const VP9_COMP *cpi,
                             MV_REFERENCE_FRAME ref_frame) {
  if (ref_frame == LAST_FRAME) {
    return cpi->lst_fb_idx;
  } else if (ref_frame == GOLDEN_FRAME) {
    return cpi->gld_fb_idx;
  } else {
    return cpi->alt_fb_idx;
  }
}

static YV12_BUFFER_CONFIG *get_ref_frame_buffer(VP9_COMP *cpi,
                                                MV_REFERENCE_FRAME ref_frame) {
  VP9_COMMON *const cm = &cpi->common;
  return &cm->frame_bufs[cm->ref_frame_map[get_ref_frame_idx(cpi,
                                                             ref_frame)]].buf;
}

void vp9_encode_frame(VP9_COMP *cpi);

void vp9_pack_bitstream(VP9_COMP *cpi, uint8_t *dest, size_t *size);

void vp9_set_speed_features(VP9_COMP *cpi);

int vp9_calc_ss_err(const YV12_BUFFER_CONFIG *source,
                    const YV12_BUFFER_CONFIG *reference);

void vp9_alloc_compressor_data(VP9_COMP *cpi);

int vp9_compute_qdelta(const VP9_COMP *cpi, double qstart, double qtarget);

static int get_token_alloc(int mb_rows, int mb_cols) {
  return mb_rows * mb_cols * (48 * 16 + 4);
}

static void set_ref_ptrs(VP9_COMMON *cm, MACROBLOCKD *xd,
                         MV_REFERENCE_FRAME ref0, MV_REFERENCE_FRAME ref1) {
  xd->block_refs[0] = &cm->frame_refs[ref0 >= LAST_FRAME ? ref0 - LAST_FRAME
                                                         : 0];
  xd->block_refs[1] = &cm->frame_refs[ref1 >= LAST_FRAME ? ref1 - LAST_FRAME
                                                         : 0];
}

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // VP9_ENCODER_VP9_ONYX_INT_H_
