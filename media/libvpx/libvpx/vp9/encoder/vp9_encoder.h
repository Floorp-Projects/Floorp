/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VP9_ENCODER_VP9_ENCODER_H_
#define VP9_ENCODER_VP9_ENCODER_H_

#include <stdio.h>

#include "./vpx_config.h"
#include "vpx/internal/vpx_codec_internal.h"
#include "vpx/vp8cx.h"
#if CONFIG_INTERNAL_STATS
#include "vpx_dsp/ssim.h"
#endif
#include "vpx_dsp/variance.h"
#include "vpx_ports/system_state.h"
#include "vpx_util/vpx_thread.h"

#include "vp9/common/vp9_alloccommon.h"
#include "vp9/common/vp9_ppflags.h"
#include "vp9/common/vp9_entropymode.h"
#include "vp9/common/vp9_thread_common.h"
#include "vp9/common/vp9_onyxc_int.h"

#include "vp9/encoder/vp9_aq_cyclicrefresh.h"
#include "vp9/encoder/vp9_context_tree.h"
#include "vp9/encoder/vp9_encodemb.h"
#include "vp9/encoder/vp9_firstpass.h"
#include "vp9/encoder/vp9_lookahead.h"
#include "vp9/encoder/vp9_mbgraph.h"
#include "vp9/encoder/vp9_mcomp.h"
#include "vp9/encoder/vp9_noise_estimate.h"
#include "vp9/encoder/vp9_quantize.h"
#include "vp9/encoder/vp9_ratectrl.h"
#include "vp9/encoder/vp9_rd.h"
#include "vp9/encoder/vp9_speed_features.h"
#include "vp9/encoder/vp9_svc_layercontext.h"
#include "vp9/encoder/vp9_tokenize.h"

#if CONFIG_VP9_TEMPORAL_DENOISING
#include "vp9/encoder/vp9_denoiser.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

// vp9 uses 10,000,000 ticks/second as time stamp
#define TICKS_PER_SEC 10000000

typedef struct {
  int nmvjointcost[MV_JOINTS];
  int nmvcosts[2][MV_VALS];
  int nmvcosts_hp[2][MV_VALS];

  vpx_prob segment_pred_probs[PREDICTION_PROBS];

  unsigned char *last_frame_seg_map_copy;

  // 0 = Intra, Last, GF, ARF
  signed char last_ref_lf_deltas[MAX_REF_LF_DELTAS];
  // 0 = ZERO_MV, MV
  signed char last_mode_lf_deltas[MAX_MODE_LF_DELTAS];

  FRAME_CONTEXT fc;
} CODING_CONTEXT;


typedef enum {
  // encode_breakout is disabled.
  ENCODE_BREAKOUT_DISABLED = 0,
  // encode_breakout is enabled.
  ENCODE_BREAKOUT_ENABLED = 1,
  // encode_breakout is enabled with small max_thresh limit.
  ENCODE_BREAKOUT_LIMITED = 2
} ENCODE_BREAKOUT_TYPE;

typedef enum {
  NORMAL      = 0,
  FOURFIVE    = 1,
  THREEFIVE   = 2,
  ONETWO      = 3
} VPX_SCALING;

typedef enum {
  // Good Quality Fast Encoding. The encoder balances quality with the amount of
  // time it takes to encode the output. Speed setting controls how fast.
  GOOD,

  // The encoder places priority on the quality of the output over encoding
  // speed. The output is compressed at the highest possible quality. This
  // option takes the longest amount of time to encode. Speed setting ignored.
  BEST,

  // Realtime/Live Encoding. This mode is optimized for realtime encoding (for
  // example, capturing a television signal or feed from a live camera). Speed
  // setting controls how fast.
  REALTIME
} MODE;

typedef enum {
  FRAMEFLAGS_KEY    = 1 << 0,
  FRAMEFLAGS_GOLDEN = 1 << 1,
  FRAMEFLAGS_ALTREF = 1 << 2,
} FRAMETYPE_FLAGS;

typedef enum {
  NO_AQ = 0,
  VARIANCE_AQ = 1,
  COMPLEXITY_AQ = 2,
  CYCLIC_REFRESH_AQ = 3,
  EQUATOR360_AQ = 4,
  AQ_MODE_COUNT  // This should always be the last member of the enum
} AQ_MODE;

typedef enum {
  RESIZE_NONE = 0,    // No frame resizing allowed (except for SVC).
  RESIZE_FIXED = 1,   // All frames are coded at the specified dimension.
  RESIZE_DYNAMIC = 2  // Coded size of each frame is determined by the codec.
} RESIZE_TYPE;

typedef struct VP9EncoderConfig {
  BITSTREAM_PROFILE profile;
  vpx_bit_depth_t bit_depth;     // Codec bit-depth.
  int width;  // width of data passed to the compressor
  int height;  // height of data passed to the compressor
  unsigned int input_bit_depth;  // Input bit depth.
  double init_framerate;  // set to passed in framerate
  int64_t target_bandwidth;  // bandwidth to be used in bits per second

  int noise_sensitivity;  // pre processing blur: recommendation 0
  int sharpness;  // sharpening output: recommendation 0:
  int speed;
  // maximum allowed bitrate for any intra frame in % of bitrate target.
  unsigned int rc_max_intra_bitrate_pct;
  // maximum allowed bitrate for any inter frame in % of bitrate target.
  unsigned int rc_max_inter_bitrate_pct;
  // percent of rate boost for golden frame in CBR mode.
  unsigned int gf_cbr_boost_pct;

  MODE mode;
  int pass;

  // Key Framing Operations
  int auto_key;  // autodetect cut scenes and set the keyframes
  int key_freq;  // maximum distance to key frame.

  int lag_in_frames;  // how many frames lag before we start encoding

  // ----------------------------------------------------------------
  // DATARATE CONTROL OPTIONS

  // vbr, cbr, constrained quality or constant quality
  enum vpx_rc_mode rc_mode;

  // buffer targeting aggressiveness
  int under_shoot_pct;
  int over_shoot_pct;

  // buffering parameters
  int64_t starting_buffer_level_ms;
  int64_t optimal_buffer_level_ms;
  int64_t maximum_buffer_size_ms;

  // Frame drop threshold.
  int drop_frames_water_mark;

  // controlling quality
  int fixed_q;
  int worst_allowed_q;
  int best_allowed_q;
  int cq_level;
  AQ_MODE aq_mode;  // Adaptive Quantization mode

  // Internal frame size scaling.
  RESIZE_TYPE resize_mode;
  int scaled_frame_width;
  int scaled_frame_height;

  // Enable feature to reduce the frame quantization every x frames.
  int frame_periodic_boost;

  // two pass datarate control
  int two_pass_vbrbias;        // two pass datarate control tweaks
  int two_pass_vbrmin_section;
  int two_pass_vbrmax_section;
  // END DATARATE CONTROL OPTIONS
  // ----------------------------------------------------------------

  // Spatial and temporal scalability.
  int ss_number_layers;  // Number of spatial layers.
  int ts_number_layers;  // Number of temporal layers.
  // Bitrate allocation for spatial layers.
  int layer_target_bitrate[VPX_MAX_LAYERS];
  int ss_target_bitrate[VPX_SS_MAX_LAYERS];
  int ss_enable_auto_arf[VPX_SS_MAX_LAYERS];
  // Bitrate allocation (CBR mode) and framerate factor, for temporal layers.
  int ts_rate_decimator[VPX_TS_MAX_LAYERS];

  int enable_auto_arf;

  int encode_breakout;  // early breakout : for video conf recommend 800

  /* Bitfield defining the error resiliency features to enable.
   * Can provide decodable frames after losses in previous
   * frames and decodable partitions after losses in the same frame.
   */
  unsigned int error_resilient_mode;

  /* Bitfield defining the parallel decoding mode where the
   * decoding in successive frames may be conducted in parallel
   * just by decoding the frame headers.
   */
  unsigned int frame_parallel_decoding_mode;

  int arnr_max_frames;
  int arnr_strength;

  int min_gf_interval;
  int max_gf_interval;

  int tile_columns;
  int tile_rows;

  int max_threads;

  int target_level;

  vpx_fixed_buf_t two_pass_stats_in;
  struct vpx_codec_pkt_list *output_pkt_list;

#if CONFIG_FP_MB_STATS
  vpx_fixed_buf_t firstpass_mb_stats_in;
#endif

  vp8e_tuning tuning;
  vp9e_tune_content content;
#if CONFIG_VP9_HIGHBITDEPTH
  int use_highbitdepth;
#endif
  vpx_color_space_t color_space;
  vpx_color_range_t color_range;
  int render_width;
  int render_height;
  VP9E_TEMPORAL_LAYERING_MODE temporal_layering_mode;
} VP9EncoderConfig;

static INLINE int is_lossless_requested(const VP9EncoderConfig *cfg) {
  return cfg->best_allowed_q == 0 && cfg->worst_allowed_q == 0;
}

// TODO(jingning) All spatially adaptive variables should go to TileDataEnc.
typedef struct TileDataEnc {
  TileInfo tile_info;
  int thresh_freq_fact[BLOCK_SIZES][MAX_MODES];
  int mode_map[BLOCK_SIZES][MAX_MODES];
} TileDataEnc;

typedef struct RD_COUNTS {
  vp9_coeff_count coef_counts[TX_SIZES][PLANE_TYPES];
  int64_t comp_pred_diff[REFERENCE_MODES];
  int64_t filter_diff[SWITCHABLE_FILTER_CONTEXTS];
  int m_search_count;
  int ex_search_count;
} RD_COUNTS;

typedef struct ThreadData {
  MACROBLOCK mb;
  RD_COUNTS rd_counts;
  FRAME_COUNTS *counts;

  PICK_MODE_CONTEXT *leaf_tree;
  PC_TREE *pc_tree;
  PC_TREE *pc_root;
} ThreadData;

struct EncWorkerData;

typedef struct ActiveMap {
  int enabled;
  int update;
  unsigned char *map;
} ActiveMap;

typedef enum {
  Y,
  U,
  V,
  ALL
} STAT_TYPE;

typedef struct IMAGE_STAT {
  double stat[ALL+1];
  double worst;
} ImageStat;

#define CPB_WINDOW_SIZE 4
#define FRAME_WINDOW_SIZE 128
#define SAMPLE_RATE_GRACE_P 0.015
#define VP9_LEVELS 14

typedef enum {
  LEVEL_UNKNOWN = 0,
  LEVEL_1 = 10,
  LEVEL_1_1 = 11,
  LEVEL_2 = 20,
  LEVEL_2_1 = 21,
  LEVEL_3 = 30,
  LEVEL_3_1 = 31,
  LEVEL_4 = 40,
  LEVEL_4_1 = 41,
  LEVEL_5 = 50,
  LEVEL_5_1 = 51,
  LEVEL_5_2 = 52,
  LEVEL_6 = 60,
  LEVEL_6_1 = 61,
  LEVEL_6_2 = 62,
  LEVEL_MAX = 255
} VP9_LEVEL;

typedef struct {
  VP9_LEVEL level;
  uint64_t max_luma_sample_rate;
  uint32_t max_luma_picture_size;
  double average_bitrate;  // in kilobits per second
  double max_cpb_size;  // in kilobits
  double compression_ratio;
  uint8_t max_col_tiles;
  uint32_t min_altref_distance;
  uint8_t max_ref_frame_buffers;
} Vp9LevelSpec;

typedef struct {
  int64_t ts;  // timestamp
  uint32_t luma_samples;
  uint32_t size;  // in bytes
} FrameRecord;

typedef struct {
  FrameRecord buf[FRAME_WINDOW_SIZE];
  uint8_t start;
  uint8_t len;
} FrameWindowBuffer;

typedef struct {
  uint8_t seen_first_altref;
  uint32_t frames_since_last_altref;
  uint64_t total_compressed_size;
  uint64_t total_uncompressed_size;
  double time_encoded;  // in seconds
  FrameWindowBuffer frame_window_buffer;
  int ref_refresh_map;
} Vp9LevelStats;

typedef struct {
  Vp9LevelStats level_stats;
  Vp9LevelSpec level_spec;
} Vp9LevelInfo;

typedef struct VP9_COMP {
  QUANTS quants;
  ThreadData td;
  MB_MODE_INFO_EXT *mbmi_ext_base;
  DECLARE_ALIGNED(16, int16_t, y_dequant[QINDEX_RANGE][8]);
  DECLARE_ALIGNED(16, int16_t, uv_dequant[QINDEX_RANGE][8]);
  VP9_COMMON common;
  VP9EncoderConfig oxcf;
  struct lookahead_ctx    *lookahead;
  struct lookahead_entry  *alt_ref_source;

  YV12_BUFFER_CONFIG *Source;
  YV12_BUFFER_CONFIG *Last_Source;  // NULL for first frame and alt_ref frames
  YV12_BUFFER_CONFIG *un_scaled_source;
  YV12_BUFFER_CONFIG scaled_source;
  YV12_BUFFER_CONFIG *unscaled_last_source;
  YV12_BUFFER_CONFIG scaled_last_source;

  TileDataEnc *tile_data;
  int allocated_tiles;  // Keep track of memory allocated for tiles.

  // For a still frame, this flag is set to 1 to skip partition search.
  int partition_search_skippable_frame;

  int scaled_ref_idx[MAX_REF_FRAMES];
  int lst_fb_idx;
  int gld_fb_idx;
  int alt_fb_idx;

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

  TOKENEXTRA *tile_tok[4][1 << 6];
  uint32_t tok_count[4][1 << 6];

  // Ambient reconstruction err target for force key frames
  int64_t ambient_err;

  RD_OPT rd;

  CODING_CONTEXT coding_context;

  int *nmvcosts[2];
  int *nmvcosts_hp[2];
  int *nmvsadcosts[2];
  int *nmvsadcosts_hp[2];

  int64_t last_time_stamp_seen;
  int64_t last_end_time_stamp_seen;
  int64_t first_time_stamp_ever;

  RATE_CONTROL rc;
  double framerate;

  int interp_filter_selected[MAX_REF_FRAMES][SWITCHABLE];

  struct vpx_codec_pkt_list  *output_pkt_list;

  MBGRAPH_FRAME_STATS mbgraph_stats[MAX_LAG_BUFFERS];
  int mbgraph_n_frames;             // number of frames filled in the above
  int static_mb_pct;                // % forced skip mbs by segmentation
  int ref_frame_flags;

  SPEED_FEATURES sf;

  uint32_t max_mv_magnitude;
  int mv_step_param;

  int allow_comp_inter_inter;

  // Default value is 1. From first pass stats, encode_breakout may be disabled.
  ENCODE_BREAKOUT_TYPE allow_encode_breakout;

  // Get threshold from external input. A suggested threshold is 800 for HD
  // clips, and 300 for < HD clips.
  int encode_breakout;

  uint8_t *segmentation_map;

  // segment threashold for encode breakout
  int segment_encode_breakout[MAX_SEGMENTS];

  CYCLIC_REFRESH *cyclic_refresh;
  ActiveMap active_map;

  fractional_mv_step_fp *find_fractional_mv_step;
  vp9_full_search_fn_t full_search_sad;
  vp9_diamond_search_fn_t diamond_search_sad;
  vp9_variance_fn_ptr_t fn_ptr[BLOCK_SIZES];
  uint64_t time_receive_data;
  uint64_t time_compress_data;
  uint64_t time_pick_lpf;
  uint64_t time_encode_sb_row;

#if CONFIG_FP_MB_STATS
  int use_fp_mb_stats;
#endif

  TWO_PASS twopass;

  YV12_BUFFER_CONFIG alt_ref_buffer;

#if CONFIG_INTERNAL_STATS
  unsigned int mode_chosen_counts[MAX_MODES];

  int count;
  uint64_t total_sq_error;
  uint64_t total_samples;
  ImageStat psnr;

  uint64_t totalp_sq_error;
  uint64_t totalp_samples;
  ImageStat psnrp;

  double total_blockiness;
  double worst_blockiness;

  int    bytes;
  double summed_quality;
  double summed_weights;
  double summedp_quality;
  double summedp_weights;
  unsigned int tot_recode_hits;
  double worst_ssim;

  ImageStat ssimg;
  ImageStat fastssim;
  ImageStat psnrhvs;

  int b_calculate_ssimg;
  int b_calculate_blockiness;

  int b_calculate_consistency;

  double total_inconsistency;
  double worst_consistency;
  Ssimv *ssim_vars;
  Metrics metrics;
#endif
  int b_calculate_psnr;

  int droppable;

  int initial_width;
  int initial_height;
  int initial_mbs;  // Number of MBs in the full-size frame; to be used to
                    // normalize the firstpass stats. This will differ from the
                    // number of MBs in the current frame when the frame is
                    // scaled.

  int use_svc;

  SVC svc;

  // Store frame variance info in SOURCE_VAR_BASED_PARTITION search type.
  diff *source_diff_var;
  // The threshold used in SOURCE_VAR_BASED_PARTITION search type.
  unsigned int source_var_thresh;
  int frames_till_next_var_check;

  int frame_flags;

  search_site_config ss_cfg;

  int mbmode_cost[INTRA_MODES];
  unsigned int inter_mode_cost[INTER_MODE_CONTEXTS][INTER_MODES];
  int intra_uv_mode_cost[FRAME_TYPES][INTRA_MODES][INTRA_MODES];
  int y_mode_costs[INTRA_MODES][INTRA_MODES][INTRA_MODES];
  int switchable_interp_costs[SWITCHABLE_FILTER_CONTEXTS][SWITCHABLE_FILTERS];
  int partition_cost[PARTITION_CONTEXTS][PARTITION_TYPES];

  int multi_arf_allowed;
  int multi_arf_enabled;
  int multi_arf_last_grp_enabled;

#if CONFIG_VP9_TEMPORAL_DENOISING
  VP9_DENOISER denoiser;
#endif

  int resize_pending;
  int resize_state;
  int external_resize;
  int resize_scale_num;
  int resize_scale_den;
  int resize_avg_qp;
  int resize_buffer_underflow;
  int resize_count;

  int use_skin_detection;

  int target_level;

  NOISE_ESTIMATE noise_estimate;

  // Count on how many consecutive times a block uses small/zeromv for encoding.
  uint8_t *consec_zero_mv;

  // VAR_BASED_PARTITION thresholds
  // 0 - threshold_64x64; 1 - threshold_32x32;
  // 2 - threshold_16x16; 3 - vbp_threshold_8x8;
  int64_t vbp_thresholds[4];
  int64_t vbp_threshold_minmax;
  int64_t vbp_threshold_sad;
  BLOCK_SIZE vbp_bsize_min;

  // Multi-threading
  int num_workers;
  VPxWorker *workers;
  struct EncWorkerData *tile_thr_data;
  VP9LfSync lf_row_sync;

  int keep_level_stats;
  Vp9LevelInfo level_info;
} VP9_COMP;

void vp9_initialize_enc(void);

struct VP9_COMP *vp9_create_compressor(VP9EncoderConfig *oxcf,
                                       BufferPool *const pool);
void vp9_remove_compressor(VP9_COMP *cpi);

void vp9_change_config(VP9_COMP *cpi, const VP9EncoderConfig *oxcf);

  // receive a frames worth of data. caller can assume that a copy of this
  // frame is made and not just a copy of the pointer..
int vp9_receive_raw_frame(VP9_COMP *cpi, unsigned int frame_flags,
                          YV12_BUFFER_CONFIG *sd, int64_t time_stamp,
                          int64_t end_time_stamp);

int vp9_get_compressed_data(VP9_COMP *cpi, unsigned int *frame_flags,
                            size_t *size, uint8_t *dest,
                            int64_t *time_stamp, int64_t *time_end, int flush);

int vp9_get_preview_raw_frame(VP9_COMP *cpi, YV12_BUFFER_CONFIG *dest,
                              vp9_ppflags_t *flags);

int vp9_use_as_reference(VP9_COMP *cpi, int ref_frame_flags);

void vp9_update_reference(VP9_COMP *cpi, int ref_frame_flags);

int vp9_copy_reference_enc(VP9_COMP *cpi, VP9_REFFRAME ref_frame_flag,
                           YV12_BUFFER_CONFIG *sd);

int vp9_set_reference_enc(VP9_COMP *cpi, VP9_REFFRAME ref_frame_flag,
                          YV12_BUFFER_CONFIG *sd);

int vp9_update_entropy(VP9_COMP *cpi, int update);

int vp9_set_active_map(VP9_COMP *cpi, unsigned char *map, int rows, int cols);

int vp9_get_active_map(VP9_COMP *cpi, unsigned char *map, int rows, int cols);

int vp9_set_internal_size(VP9_COMP *cpi,
                          VPX_SCALING horiz_mode, VPX_SCALING vert_mode);

int vp9_set_size_literal(VP9_COMP *cpi, unsigned int width,
                         unsigned int height);

void vp9_set_svc(VP9_COMP *cpi, int use_svc);

int vp9_get_quantizer(struct VP9_COMP *cpi);

static INLINE int frame_is_kf_gf_arf(const VP9_COMP *cpi) {
  return frame_is_intra_only(&cpi->common) ||
         cpi->refresh_alt_ref_frame ||
         (cpi->refresh_golden_frame && !cpi->rc.is_src_frame_alt_ref);
}

static INLINE int get_ref_frame_map_idx(const VP9_COMP *cpi,
                                        MV_REFERENCE_FRAME ref_frame) {
  if (ref_frame == LAST_FRAME) {
    return cpi->lst_fb_idx;
  } else if (ref_frame == GOLDEN_FRAME) {
    return cpi->gld_fb_idx;
  } else {
    return cpi->alt_fb_idx;
  }
}

static INLINE int get_ref_frame_buf_idx(const VP9_COMP *const cpi,
                                        int ref_frame) {
  const VP9_COMMON *const cm = &cpi->common;
  const int map_idx = get_ref_frame_map_idx(cpi, ref_frame);
  return (map_idx != INVALID_IDX) ? cm->ref_frame_map[map_idx] : INVALID_IDX;
}

static INLINE YV12_BUFFER_CONFIG *get_ref_frame_buffer(
    VP9_COMP *cpi, MV_REFERENCE_FRAME ref_frame) {
  VP9_COMMON *const cm = &cpi->common;
  const int buf_idx = get_ref_frame_buf_idx(cpi, ref_frame);
  return
      buf_idx != INVALID_IDX ? &cm->buffer_pool->frame_bufs[buf_idx].buf : NULL;
}

static INLINE int get_token_alloc(int mb_rows, int mb_cols) {
  // TODO(JBB): double check we can't exceed this token count if we have a
  // 32x32 transform crossing a boundary at a multiple of 16.
  // mb_rows, cols are in units of 16 pixels. We assume 3 planes all at full
  // resolution. We assume up to 1 token per pixel, and then allow
  // a head room of 4.
  return mb_rows * mb_cols * (16 * 16 * 3 + 4);
}

// Get the allocated token size for a tile. It does the same calculation as in
// the frame token allocation.
static INLINE int allocated_tokens(TileInfo tile) {
  int tile_mb_rows = (tile.mi_row_end - tile.mi_row_start + 1) >> 1;
  int tile_mb_cols = (tile.mi_col_end - tile.mi_col_start + 1) >> 1;

  return get_token_alloc(tile_mb_rows, tile_mb_cols);
}

int64_t vp9_get_y_sse(const YV12_BUFFER_CONFIG *a, const YV12_BUFFER_CONFIG *b);
#if CONFIG_VP9_HIGHBITDEPTH
int64_t vp9_highbd_get_y_sse(const YV12_BUFFER_CONFIG *a,
                             const YV12_BUFFER_CONFIG *b);
#endif  // CONFIG_VP9_HIGHBITDEPTH

void vp9_scale_references(VP9_COMP *cpi);

void vp9_update_reference_frames(VP9_COMP *cpi);

void vp9_set_high_precision_mv(VP9_COMP *cpi, int allow_high_precision_mv);

YV12_BUFFER_CONFIG *vp9_svc_twostage_scale(VP9_COMMON *cm,
                                           YV12_BUFFER_CONFIG *unscaled,
                                           YV12_BUFFER_CONFIG *scaled,
                                           YV12_BUFFER_CONFIG *scaled_temp);

YV12_BUFFER_CONFIG *vp9_scale_if_required(VP9_COMMON *cm,
                                          YV12_BUFFER_CONFIG *unscaled,
                                          YV12_BUFFER_CONFIG *scaled,
                                          int use_normative_scaler);

void vp9_apply_encoding_flags(VP9_COMP *cpi, vpx_enc_frame_flags_t flags);

static INLINE int is_two_pass_svc(const struct VP9_COMP *const cpi) {
  return cpi->use_svc && cpi->oxcf.pass != 0;
}

static INLINE int is_one_pass_cbr_svc(const struct VP9_COMP *const cpi) {
  return (cpi->use_svc && cpi->oxcf.pass == 0);
}

static INLINE int is_altref_enabled(const VP9_COMP *const cpi) {
  return cpi->oxcf.mode != REALTIME && cpi->oxcf.lag_in_frames > 0 &&
         (cpi->oxcf.enable_auto_arf &&
          (!is_two_pass_svc(cpi) ||
           cpi->oxcf.ss_enable_auto_arf[cpi->svc.spatial_layer_id]));
}

static INLINE void set_ref_ptrs(VP9_COMMON *cm, MACROBLOCKD *xd,
                                MV_REFERENCE_FRAME ref0,
                                MV_REFERENCE_FRAME ref1) {
  xd->block_refs[0] = &cm->frame_refs[ref0 >= LAST_FRAME ? ref0 - LAST_FRAME
                                                         : 0];
  xd->block_refs[1] = &cm->frame_refs[ref1 >= LAST_FRAME ? ref1 - LAST_FRAME
                                                         : 0];
}

static INLINE int get_chessboard_index(const int frame_index) {
  return frame_index & 0x1;
}

static INLINE int *cond_cost_list(const struct VP9_COMP *cpi, int *cost_list) {
  return cpi->sf.mv.subpel_search_method != SUBPEL_TREE ? cost_list : NULL;
}

VP9_LEVEL vp9_get_level(const Vp9LevelSpec *const level_spec);

void vp9_new_framerate(VP9_COMP *cpi, double framerate);

#define LAYER_IDS_TO_IDX(sl, tl, num_tl) ((sl) * (num_tl) + (tl))

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // VP9_ENCODER_VP9_ENCODER_H_
