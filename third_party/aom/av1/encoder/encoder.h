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

#ifndef AV1_ENCODER_ENCODER_H_
#define AV1_ENCODER_ENCODER_H_

#include <stdio.h>

#include "config/aom_config.h"

#include "aom/aomcx.h"

#include "av1/common/alloccommon.h"
#include "av1/common/entropymode.h"
#include "av1/common/thread_common.h"
#include "av1/common/onyxc_int.h"
#include "av1/common/resize.h"
#include "av1/common/timing.h"
#include "av1/encoder/aq_cyclicrefresh.h"
#include "av1/encoder/av1_quantize.h"
#include "av1/encoder/context_tree.h"
#include "av1/encoder/encodemb.h"
#include "av1/encoder/firstpass.h"
#include "av1/encoder/lookahead.h"
#include "av1/encoder/mbgraph.h"
#include "av1/encoder/mcomp.h"
#include "av1/encoder/ratectrl.h"
#include "av1/encoder/rd.h"
#include "av1/encoder/speed_features.h"
#include "av1/encoder/tokenize.h"

#if CONFIG_INTERNAL_STATS
#include "aom_dsp/ssim.h"
#endif
#include "aom_dsp/variance.h"
#include "aom/internal/aom_codec_internal.h"
#include "aom_util/aom_thread.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  int nmv_vec_cost[MV_JOINTS];
  int nmv_costs[2][MV_VALS];
  int nmv_costs_hp[2][MV_VALS];

  FRAME_CONTEXT fc;
} CODING_CONTEXT;

typedef enum {
  // regular inter frame
  REGULAR_FRAME = 0,
  // alternate reference frame
  ARF_FRAME = 1,
  // overlay frame
  OVERLAY_FRAME = 2,
  // golden frame
  GLD_FRAME = 3,
  // backward reference frame
  BRF_FRAME = 4,
  // extra alternate reference frame
  EXT_ARF_FRAME = 5,
  FRAME_CONTEXT_INDEXES
} FRAME_CONTEXT_INDEX;

typedef enum {
  NORMAL = 0,
  FOURFIVE = 1,
  THREEFIVE = 2,
  ONETWO = 3
} AOM_SCALING;

typedef enum {
  // Good Quality Fast Encoding. The encoder balances quality with the amount of
  // time it takes to encode the output. Speed setting controls how fast.
  GOOD
} MODE;

typedef enum {
  FRAMEFLAGS_KEY = 1 << 0,
  FRAMEFLAGS_GOLDEN = 1 << 1,
  FRAMEFLAGS_BWDREF = 1 << 2,
  // TODO(zoeliu): To determine whether a frame flag is needed for ALTREF2_FRAME
  FRAMEFLAGS_ALTREF = 1 << 3,
} FRAMETYPE_FLAGS;

typedef enum {
  NO_AQ = 0,
  VARIANCE_AQ = 1,
  COMPLEXITY_AQ = 2,
  CYCLIC_REFRESH_AQ = 3,
  AQ_MODE_COUNT  // This should always be the last member of the enum
} AQ_MODE;
typedef enum {
  NO_DELTA_Q = 0,
  DELTA_Q_ONLY = 1,
  DELTA_Q_LF = 2,
  DELTAQ_MODE_COUNT  // This should always be the last member of the enum
} DELTAQ_MODE;

typedef enum {
  RESIZE_NONE = 0,    // No frame resizing allowed.
  RESIZE_FIXED = 1,   // All frames are coded at the specified scale.
  RESIZE_RANDOM = 2,  // All frames are coded at a random scale.
  RESIZE_MODES
} RESIZE_MODE;

typedef enum {
  SUPERRES_NONE = 0,     // No frame superres allowed
  SUPERRES_FIXED = 1,    // All frames are coded at the specified scale,
                         // and super-resolved.
  SUPERRES_RANDOM = 2,   // All frames are coded at a random scale,
                         // and super-resolved.
  SUPERRES_QTHRESH = 3,  // Superres scale for a frame is determined based on
                         // q_index
  SUPERRES_MODES
} SUPERRES_MODE;

typedef struct AV1EncoderConfig {
  BITSTREAM_PROFILE profile;
  aom_bit_depth_t bit_depth;     // Codec bit-depth.
  int width;                     // width of data passed to the compressor
  int height;                    // height of data passed to the compressor
  int forced_max_frame_width;    // forced maximum width of frame (if != 0)
  int forced_max_frame_height;   // forced maximum height of frame (if != 0)
  unsigned int input_bit_depth;  // Input bit depth.
  double init_framerate;         // set to passed in framerate
  int64_t target_bandwidth;      // bandwidth to be used in bits per second

  int noise_sensitivity;  // pre processing blur: recommendation 0
  int sharpness;          // sharpening output: recommendation 0:
  int speed;
  int dev_sf;
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
  int sframe_dist;
  int sframe_mode;
  int sframe_enabled;
  int lag_in_frames;  // how many frames lag before we start encoding
  int fwd_kf_enabled;

  // ----------------------------------------------------------------
  // DATARATE CONTROL OPTIONS

  // vbr, cbr, constrained quality or constant quality
  enum aom_rc_mode rc_mode;

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
  DELTAQ_MODE deltaq_mode;
  int enable_cdef;
  int enable_restoration;
  int disable_trellis_quant;
  int using_qm;
  int qm_y;
  int qm_u;
  int qm_v;
  int qm_minlevel;
  int qm_maxlevel;
#if CONFIG_DIST_8X8
  int using_dist_8x8;
#endif
  unsigned int num_tile_groups;
  unsigned int mtu;

  // Internal frame size scaling.
  RESIZE_MODE resize_mode;
  uint8_t resize_scale_denominator;
  uint8_t resize_kf_scale_denominator;

  // Frame Super-Resolution size scaling.
  SUPERRES_MODE superres_mode;
  uint8_t superres_scale_denominator;
  uint8_t superres_kf_scale_denominator;
  int superres_qthresh;
  int superres_kf_qthresh;

  // Enable feature to reduce the frame quantization every x frames.
  int frame_periodic_boost;

  // two pass datarate control
  int two_pass_vbrbias;  // two pass datarate control tweaks
  int two_pass_vbrmin_section;
  int two_pass_vbrmax_section;
  // END DATARATE CONTROL OPTIONS
  // ----------------------------------------------------------------

  int enable_auto_arf;
  int enable_auto_brf;  // (b)ackward (r)ef (f)rame

  /* Bitfield defining the error resiliency features to enable.
   * Can provide decodable frames after losses in previous
   * frames and decodable partitions after losses in the same frame.
   */
  unsigned int error_resilient_mode;

  unsigned int s_frame_mode;

  /* Bitfield defining the parallel decoding mode where the
   * decoding in successive frames may be conducted in parallel
   * just by decoding the frame headers.
   */
  unsigned int frame_parallel_decoding_mode;

  unsigned int limit;

  int arnr_max_frames;
  int arnr_strength;

  int min_gf_interval;
  int max_gf_interval;

  int tile_columns;
  int tile_rows;
  int tile_width_count;
  int tile_height_count;
  int tile_widths[MAX_TILE_COLS];
  int tile_heights[MAX_TILE_ROWS];

  int max_threads;

  aom_fixed_buf_t two_pass_stats_in;
  struct aom_codec_pkt_list *output_pkt_list;

#if CONFIG_FP_MB_STATS
  aom_fixed_buf_t firstpass_mb_stats_in;
#endif

  aom_tune_metric tuning;
  aom_tune_content content;
  int use_highbitdepth;
  aom_color_primaries_t color_primaries;
  aom_transfer_characteristics_t transfer_characteristics;
  aom_matrix_coefficients_t matrix_coefficients;
  aom_chroma_sample_position_t chroma_sample_position;
  int color_range;
  int render_width;
  int render_height;
  aom_timing_info_type_t timing_info_type;
  int timing_info_present;
  aom_timing_info_t timing_info;
  int decoder_model_info_present_flag;
  int display_model_info_present_flag;
  int buffer_removal_delay_present;
  aom_dec_model_info_t buffer_model;
  aom_dec_model_op_parameters_t op_params[MAX_NUM_OPERATING_POINTS + 1];
  aom_op_timing_info_t op_frame_timing[MAX_NUM_OPERATING_POINTS + 1];
  int film_grain_test_vector;
  const char *film_grain_table_filename;

  uint8_t cdf_update_mode;
  aom_superblock_size_t superblock_size;
  unsigned int large_scale_tile;
  unsigned int single_tile_decoding;
  int monochrome;
  unsigned int full_still_picture_hdr;
  int enable_dual_filter;
  unsigned int motion_vector_unit_test;
  const cfg_options_t *cfg;
  int enable_order_hint;
  int enable_jnt_comp;
  int enable_ref_frame_mvs;
  unsigned int allow_ref_frame_mvs;
  int enable_warped_motion;
  int allow_warped_motion;
  int enable_superres;
  unsigned int save_as_annexb;
} AV1EncoderConfig;

static INLINE int is_lossless_requested(const AV1EncoderConfig *cfg) {
  return cfg->best_allowed_q == 0 && cfg->worst_allowed_q == 0;
}

typedef struct FRAME_COUNTS {
// Note: This structure should only contain 'unsigned int' fields, or
// aggregates built solely from 'unsigned int' fields/elements
#if CONFIG_ENTROPY_STATS
  unsigned int kf_y_mode[KF_MODE_CONTEXTS][KF_MODE_CONTEXTS][INTRA_MODES];
  unsigned int angle_delta[DIRECTIONAL_MODES][2 * MAX_ANGLE_DELTA + 1];
  unsigned int y_mode[BLOCK_SIZE_GROUPS][INTRA_MODES];
  unsigned int uv_mode[CFL_ALLOWED_TYPES][INTRA_MODES][UV_INTRA_MODES];
  unsigned int cfl_sign[CFL_JOINT_SIGNS];
  unsigned int cfl_alpha[CFL_ALPHA_CONTEXTS][CFL_ALPHABET_SIZE];
  unsigned int palette_y_mode[PALATTE_BSIZE_CTXS][PALETTE_Y_MODE_CONTEXTS][2];
  unsigned int palette_uv_mode[PALETTE_UV_MODE_CONTEXTS][2];
  unsigned int palette_y_size[PALATTE_BSIZE_CTXS][PALETTE_SIZES];
  unsigned int palette_uv_size[PALATTE_BSIZE_CTXS][PALETTE_SIZES];
  unsigned int palette_y_color_index[PALETTE_SIZES]
                                    [PALETTE_COLOR_INDEX_CONTEXTS]
                                    [PALETTE_COLORS];
  unsigned int palette_uv_color_index[PALETTE_SIZES]
                                     [PALETTE_COLOR_INDEX_CONTEXTS]
                                     [PALETTE_COLORS];
  unsigned int partition[PARTITION_CONTEXTS][EXT_PARTITION_TYPES];
  unsigned int txb_skip[TOKEN_CDF_Q_CTXS][TX_SIZES][TXB_SKIP_CONTEXTS][2];
  unsigned int eob_extra[TOKEN_CDF_Q_CTXS][TX_SIZES][PLANE_TYPES]
                        [EOB_COEF_CONTEXTS][2];
  unsigned int dc_sign[PLANE_TYPES][DC_SIGN_CONTEXTS][2];
  unsigned int coeff_lps[TX_SIZES][PLANE_TYPES][BR_CDF_SIZE - 1][LEVEL_CONTEXTS]
                        [2];
  unsigned int eob_flag[TX_SIZES][PLANE_TYPES][EOB_COEF_CONTEXTS][2];
  unsigned int eob_multi16[TOKEN_CDF_Q_CTXS][PLANE_TYPES][2][5];
  unsigned int eob_multi32[TOKEN_CDF_Q_CTXS][PLANE_TYPES][2][6];
  unsigned int eob_multi64[TOKEN_CDF_Q_CTXS][PLANE_TYPES][2][7];
  unsigned int eob_multi128[TOKEN_CDF_Q_CTXS][PLANE_TYPES][2][8];
  unsigned int eob_multi256[TOKEN_CDF_Q_CTXS][PLANE_TYPES][2][9];
  unsigned int eob_multi512[TOKEN_CDF_Q_CTXS][PLANE_TYPES][2][10];
  unsigned int eob_multi1024[TOKEN_CDF_Q_CTXS][PLANE_TYPES][2][11];
  unsigned int coeff_lps_multi[TOKEN_CDF_Q_CTXS][TX_SIZES][PLANE_TYPES]
                              [LEVEL_CONTEXTS][BR_CDF_SIZE];
  unsigned int coeff_base_multi[TOKEN_CDF_Q_CTXS][TX_SIZES][PLANE_TYPES]
                               [SIG_COEF_CONTEXTS][NUM_BASE_LEVELS + 2];
  unsigned int coeff_base_eob_multi[TOKEN_CDF_Q_CTXS][TX_SIZES][PLANE_TYPES]
                                   [SIG_COEF_CONTEXTS_EOB][NUM_BASE_LEVELS + 1];
  unsigned int newmv_mode[NEWMV_MODE_CONTEXTS][2];
  unsigned int zeromv_mode[GLOBALMV_MODE_CONTEXTS][2];
  unsigned int refmv_mode[REFMV_MODE_CONTEXTS][2];
  unsigned int drl_mode[DRL_MODE_CONTEXTS][2];
  unsigned int inter_compound_mode[INTER_MODE_CONTEXTS][INTER_COMPOUND_MODES];
  unsigned int wedge_idx[BLOCK_SIZES_ALL][16];
  unsigned int interintra[BLOCK_SIZE_GROUPS][2];
  unsigned int interintra_mode[BLOCK_SIZE_GROUPS][INTERINTRA_MODES];
  unsigned int wedge_interintra[BLOCK_SIZES_ALL][2];
  unsigned int compound_type[BLOCK_SIZES_ALL][COMPOUND_TYPES - 1];
  unsigned int motion_mode[BLOCK_SIZES_ALL][MOTION_MODES];
  unsigned int obmc[BLOCK_SIZES_ALL][2];
  unsigned int intra_inter[INTRA_INTER_CONTEXTS][2];
  unsigned int comp_inter[COMP_INTER_CONTEXTS][2];
  unsigned int comp_ref_type[COMP_REF_TYPE_CONTEXTS][2];
  unsigned int uni_comp_ref[UNI_COMP_REF_CONTEXTS][UNIDIR_COMP_REFS - 1][2];
  unsigned int single_ref[REF_CONTEXTS][SINGLE_REFS - 1][2];
  unsigned int comp_ref[REF_CONTEXTS][FWD_REFS - 1][2];
  unsigned int comp_bwdref[REF_CONTEXTS][BWD_REFS - 1][2];
  unsigned int intrabc[2];

  unsigned int txfm_partition[TXFM_PARTITION_CONTEXTS][2];
  unsigned int intra_tx_size[MAX_TX_CATS][TX_SIZE_CONTEXTS][MAX_TX_DEPTH + 1];
  unsigned int skip_mode[SKIP_MODE_CONTEXTS][2];
  unsigned int skip[SKIP_CONTEXTS][2];
  unsigned int compound_index[COMP_INDEX_CONTEXTS][2];
  unsigned int comp_group_idx[COMP_GROUP_IDX_CONTEXTS][2];
  unsigned int delta_q[DELTA_Q_PROBS][2];
  unsigned int delta_lf_multi[FRAME_LF_COUNT][DELTA_LF_PROBS][2];
  unsigned int delta_lf[DELTA_LF_PROBS][2];

  unsigned int inter_ext_tx[EXT_TX_SETS_INTER][EXT_TX_SIZES][TX_TYPES];
  unsigned int intra_ext_tx[EXT_TX_SETS_INTRA][EXT_TX_SIZES][INTRA_MODES]
                           [TX_TYPES];
  unsigned int filter_intra_mode[FILTER_INTRA_MODES];
  unsigned int filter_intra[BLOCK_SIZES_ALL][2];
  unsigned int switchable_restore[RESTORE_SWITCHABLE_TYPES];
  unsigned int wiener_restore[2];
  unsigned int sgrproj_restore[2];
#endif  // CONFIG_ENTROPY_STATS

  unsigned int switchable_interp[SWITCHABLE_FILTER_CONTEXTS]
                                [SWITCHABLE_FILTERS];
} FRAME_COUNTS;

// TODO(jingning) All spatially adaptive variables should go to TileDataEnc.
typedef struct TileDataEnc {
  TileInfo tile_info;
  int thresh_freq_fact[BLOCK_SIZES_ALL][MAX_MODES];
  int mode_map[BLOCK_SIZES_ALL][MAX_MODES];
  int m_search_count;
  int ex_search_count;
  CFL_CTX cfl;
  DECLARE_ALIGNED(16, FRAME_CONTEXT, tctx);
  uint8_t allow_update_cdf;
} TileDataEnc;

typedef struct RD_COUNTS {
  int64_t comp_pred_diff[REFERENCE_MODES];
  // Stores number of 4x4 blocks using global motion per reference frame.
  int global_motion_used[REF_FRAMES];
  int compound_ref_used_flag;
  int skip_mode_used_flag;
} RD_COUNTS;

typedef struct ThreadData {
  MACROBLOCK mb;
  RD_COUNTS rd_counts;
  FRAME_COUNTS *counts;
  PC_TREE *pc_tree;
  PC_TREE *pc_root[MAX_MIB_SIZE_LOG2 - MIN_MIB_SIZE_LOG2 + 1];
  int32_t *wsrc_buf;
  int32_t *mask_buf;
  uint8_t *above_pred_buf;
  uint8_t *left_pred_buf;
  PALETTE_BUFFER *palette_buffer;
  int intrabc_used_this_tile;
} ThreadData;

struct EncWorkerData;

typedef struct ActiveMap {
  int enabled;
  int update;
  unsigned char *map;
} ActiveMap;

#if CONFIG_INTERNAL_STATS
// types of stats
typedef enum {
  STAT_Y,
  STAT_U,
  STAT_V,
  STAT_ALL,
  NUM_STAT_TYPES  // This should always be the last member of the enum
} StatType;

typedef struct IMAGE_STAT {
  double stat[NUM_STAT_TYPES];
  double worst;
} ImageStat;
#endif  // CONFIG_INTERNAL_STATS

typedef struct {
  int ref_count;
  YV12_BUFFER_CONFIG buf;
} EncRefCntBuffer;

typedef struct TileBufferEnc {
  uint8_t *data;
  size_t size;
} TileBufferEnc;

typedef struct AV1_COMP {
  QUANTS quants;
  ThreadData td;
  FRAME_COUNTS counts;
  MB_MODE_INFO_EXT *mbmi_ext_base;
  CB_COEFF_BUFFER *coeff_buffer_base;
  Dequants dequants;
  AV1_COMMON common;
  AV1EncoderConfig oxcf;
  struct lookahead_ctx *lookahead;
  struct lookahead_entry *alt_ref_source;

  int optimize_speed_feature;
  int optimize_seg_arr[MAX_SEGMENTS];

  YV12_BUFFER_CONFIG *source;
  YV12_BUFFER_CONFIG *last_source;  // NULL for first frame and alt_ref frames
  YV12_BUFFER_CONFIG *unscaled_source;
  YV12_BUFFER_CONFIG scaled_source;
  YV12_BUFFER_CONFIG *unscaled_last_source;
  YV12_BUFFER_CONFIG scaled_last_source;

  // For a still frame, this flag is set to 1 to skip partition search.
  int partition_search_skippable_frame;
  double csm_rate_array[32];
  double m_rate_array[32];
  int rate_size;
  int rate_index;
  hash_table *previous_hash_table;
  int previous_index;
  int cur_poc;  // DebugInfo

  int scaled_ref_idx[REF_FRAMES];
  int ref_fb_idx[REF_FRAMES];
  int refresh_fb_idx;  // ref frame buffer index to refresh

  int last_show_frame_buf_idx;  // last show frame buffer index

  int refresh_last_frame;
  int refresh_golden_frame;
  int refresh_bwd_ref_frame;
  int refresh_alt2_ref_frame;
  int refresh_alt_ref_frame;

  int ext_refresh_frame_flags_pending;
  int ext_refresh_last_frame;
  int ext_refresh_golden_frame;
  int ext_refresh_bwd_ref_frame;
  int ext_refresh_alt2_ref_frame;
  int ext_refresh_alt_ref_frame;

  int ext_refresh_frame_context_pending;
  int ext_refresh_frame_context;
  int ext_use_ref_frame_mvs;
  int ext_use_error_resilient;
  int ext_use_s_frame;
  int ext_use_primary_ref_none;

  YV12_BUFFER_CONFIG last_frame_uf;
  YV12_BUFFER_CONFIG trial_frame_rst;

  // Ambient reconstruction err target for force key frames
  int64_t ambient_err;

  RD_OPT rd;

  CODING_CONTEXT coding_context;

  int gmtype_cost[TRANS_TYPES];
  int gmparams_cost[REF_FRAMES];

  int nmv_costs[2][MV_VALS];
  int nmv_costs_hp[2][MV_VALS];

  int64_t last_time_stamp_seen;
  int64_t last_end_time_stamp_seen;
  int64_t first_time_stamp_ever;

  RATE_CONTROL rc;
  double framerate;

  // NOTE(zoeliu): Any inter frame allows maximum of REF_FRAMES inter
  // references; Plus the currently coded frame itself, it is needed to allocate
  // sufficient space to the size of the maximum possible number of frames.
  int interp_filter_selected[REF_FRAMES + 1][SWITCHABLE];

  struct aom_codec_pkt_list *output_pkt_list;

  MBGRAPH_FRAME_STATS mbgraph_stats[MAX_LAG_BUFFERS];
  int mbgraph_n_frames;  // number of frames filled in the above
  int static_mb_pct;     // % forced skip mbs by segmentation
  int ref_frame_flags;
  int ext_ref_frame_flags;
  RATE_FACTOR_LEVEL frame_rf_level[FRAME_BUFFERS];

  SPEED_FEATURES sf;

  unsigned int max_mv_magnitude;
  int mv_step_param;

  int allow_comp_inter_inter;
  int all_one_sided_refs;

  uint8_t *segmentation_map;

  CYCLIC_REFRESH *cyclic_refresh;
  ActiveMap active_map;

  fractional_mv_step_fp *find_fractional_mv_step;
  av1_diamond_search_fn_t diamond_search_sad;
  aom_variance_fn_ptr_t fn_ptr[BLOCK_SIZES_ALL];
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

  double total_blockiness;
  double worst_blockiness;

  int bytes;
  double summed_quality;
  double summed_weights;
  unsigned int tot_recode_hits;
  double worst_ssim;

  ImageStat fastssim;
  ImageStat psnrhvs;

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

  // When resize is triggered through external control, the desired width/height
  // are stored here until use in the next frame coded. They are effective only
  // for
  // one frame and are reset after use.
  int resize_pending_width;
  int resize_pending_height;

  int frame_flags;

  search_site_config ss_cfg;

  int multi_arf_allowed;

  TileDataEnc *tile_data;
  int allocated_tiles;  // Keep track of memory allocated for tiles.

  TOKENEXTRA *tile_tok[MAX_TILE_ROWS][MAX_TILE_COLS];
  unsigned int tok_count[MAX_TILE_ROWS][MAX_TILE_COLS];

  TileBufferEnc tile_buffers[MAX_TILE_ROWS][MAX_TILE_COLS];

  int resize_state;
  int resize_avg_qp;
  int resize_buffer_underflow;
  int resize_count;

  // Sequence parameters have been transmitted already and locked
  // or not. Once locked av1_change_config cannot change the seq
  // parameters.
  int seq_params_locked;

  // VARIANCE_AQ segment map refresh
  int vaq_refresh;

  // Multi-threading
  int num_workers;
  AVxWorker *workers;
  struct EncWorkerData *tile_thr_data;
  int refresh_frame_mask;
  int existing_fb_idx_to_show;
  int is_arf_filter_off[MAX_EXT_ARFS + 1];
  int num_extra_arfs;
  int arf_map[MAX_EXT_ARFS + 1];
  int arf_pos_in_gf[MAX_EXT_ARFS + 1];
  int arf_pos_for_ovrly[MAX_EXT_ARFS + 1];
  int global_motion_search_done;
  tran_low_t *tcoeff_buf[MAX_MB_PLANE];
  int extra_arf_allowed;
  // A flag to indicate if intrabc is ever used in current frame.
  int intrabc_used;
  int dv_cost[2][MV_VALS];
  // TODO(huisu@google.com): we can update dv_joint_cost per SB.
  int dv_joint_cost[MV_JOINTS];
  int has_lossless_segment;

  // For frame refs short signaling:
  //   A mapping of each reference frame from its encoder side value to the
  //   decoder side value obtained following the short signaling procedure.
  int ref_conv[REF_FRAMES];

  AV1LfSync lf_row_sync;
  AV1LrSync lr_row_sync;
  AV1LrStruct lr_ctxt;
} AV1_COMP;

void av1_initialize_enc(void);

struct AV1_COMP *av1_create_compressor(AV1EncoderConfig *oxcf,
                                       BufferPool *const pool);
void av1_remove_compressor(AV1_COMP *cpi);

void av1_change_config(AV1_COMP *cpi, const AV1EncoderConfig *oxcf);

// receive a frames worth of data. caller can assume that a copy of this
// frame is made and not just a copy of the pointer..
int av1_receive_raw_frame(AV1_COMP *cpi, aom_enc_frame_flags_t frame_flags,
                          YV12_BUFFER_CONFIG *sd, int64_t time_stamp,
                          int64_t end_time_stamp);

int av1_get_compressed_data(AV1_COMP *cpi, unsigned int *frame_flags,
                            size_t *size, uint8_t *dest, int64_t *time_stamp,
                            int64_t *time_end, int flush,
                            const aom_rational_t *timebase);

int av1_get_preview_raw_frame(AV1_COMP *cpi, YV12_BUFFER_CONFIG *dest);

int av1_get_last_show_frame(AV1_COMP *cpi, YV12_BUFFER_CONFIG *frame);

aom_codec_err_t av1_copy_new_frame_enc(AV1_COMMON *cm,
                                       YV12_BUFFER_CONFIG *new_frame,
                                       YV12_BUFFER_CONFIG *sd);

int av1_use_as_reference(AV1_COMP *cpi, int ref_frame_flags);

void av1_update_reference(AV1_COMP *cpi, int ref_frame_flags);

int av1_copy_reference_enc(AV1_COMP *cpi, int idx, YV12_BUFFER_CONFIG *sd);

int av1_set_reference_enc(AV1_COMP *cpi, int idx, YV12_BUFFER_CONFIG *sd);

int av1_update_entropy(AV1_COMP *cpi, int update);

int av1_set_active_map(AV1_COMP *cpi, unsigned char *map, int rows, int cols);

int av1_get_active_map(AV1_COMP *cpi, unsigned char *map, int rows, int cols);

int av1_set_internal_size(AV1_COMP *cpi, AOM_SCALING horiz_mode,
                          AOM_SCALING vert_mode);

int av1_get_quantizer(struct AV1_COMP *cpi);

int av1_convert_sect5obus_to_annexb(uint8_t *buffer, size_t *input_size);

int64_t timebase_units_to_ticks(const aom_rational_t *timebase, int64_t n);
int64_t ticks_to_timebase_units(const aom_rational_t *timebase, int64_t n);

static INLINE int frame_is_kf_gf_arf(const AV1_COMP *cpi) {
  return frame_is_intra_only(&cpi->common) || cpi->refresh_alt_ref_frame ||
         (cpi->refresh_golden_frame && !cpi->rc.is_src_frame_alt_ref);
}

static INLINE int get_ref_frame_map_idx(const AV1_COMP *cpi,
                                        MV_REFERENCE_FRAME ref_frame) {
  return (ref_frame >= 1) ? cpi->ref_fb_idx[ref_frame - 1] : INVALID_IDX;
}

static INLINE int get_ref_frame_buf_idx(const AV1_COMP *cpi,
                                        MV_REFERENCE_FRAME ref_frame) {
  const AV1_COMMON *const cm = &cpi->common;
  const int map_idx = get_ref_frame_map_idx(cpi, ref_frame);
  return (map_idx != INVALID_IDX) ? cm->ref_frame_map[map_idx] : INVALID_IDX;
}

// TODO(huisu@google.com, youzhou@microsoft.com): enable hash-me for HBD.
static INLINE int av1_use_hash_me(const AV1_COMMON *const cm) {
  return cm->allow_screen_content_tools;
}

static INLINE hash_table *av1_get_ref_frame_hash_map(
    const AV1_COMP *cpi, MV_REFERENCE_FRAME ref_frame) {
  const AV1_COMMON *const cm = &cpi->common;
  const int buf_idx = get_ref_frame_buf_idx(cpi, ref_frame);
  return buf_idx != INVALID_IDX
             ? &cm->buffer_pool->frame_bufs[buf_idx].hash_table
             : NULL;
}

static INLINE YV12_BUFFER_CONFIG *get_ref_frame_buffer(
    const AV1_COMP *cpi, MV_REFERENCE_FRAME ref_frame) {
  const AV1_COMMON *const cm = &cpi->common;
  const int buf_idx = get_ref_frame_buf_idx(cpi, ref_frame);
  return buf_idx != INVALID_IDX ? &cm->buffer_pool->frame_bufs[buf_idx].buf
                                : NULL;
}

static INLINE int enc_is_ref_frame_buf(AV1_COMP *cpi, RefCntBuffer *frame_buf) {
  MV_REFERENCE_FRAME ref_frame;
  AV1_COMMON *const cm = &cpi->common;
  for (ref_frame = LAST_FRAME; ref_frame <= ALTREF_FRAME; ++ref_frame) {
    const int buf_idx = get_ref_frame_buf_idx(cpi, ref_frame);
    if (buf_idx == INVALID_IDX) continue;
    if (frame_buf == &cm->buffer_pool->frame_bufs[buf_idx]) break;
  }
  return (ref_frame <= ALTREF_FRAME);
}

// Token buffer is only used for palette tokens.
static INLINE unsigned int get_token_alloc(int mb_rows, int mb_cols,
                                           int sb_size_log2,
                                           const int num_planes) {
  // Calculate the maximum number of max superblocks in the image.
  const int shift = sb_size_log2 - 4;
  const int sb_size = 1 << sb_size_log2;
  const int sb_size_square = sb_size * sb_size;
  const int sb_rows = ALIGN_POWER_OF_TWO(mb_rows, shift) >> shift;
  const int sb_cols = ALIGN_POWER_OF_TWO(mb_cols, shift) >> shift;

  // One palette token for each pixel. There can be palettes on two planes.
  const int sb_palette_toks = AOMMIN(2, num_planes) * sb_size_square;

  return sb_rows * sb_cols * sb_palette_toks;
}

// Get the allocated token size for a tile. It does the same calculation as in
// the frame token allocation.
static INLINE unsigned int allocated_tokens(TileInfo tile, int sb_size_log2,
                                            int num_planes) {
  int tile_mb_rows = (tile.mi_row_end - tile.mi_row_start + 2) >> 2;
  int tile_mb_cols = (tile.mi_col_end - tile.mi_col_start + 2) >> 2;

  return get_token_alloc(tile_mb_rows, tile_mb_cols, sb_size_log2, num_planes);
}

void av1_apply_encoding_flags(AV1_COMP *cpi, aom_enc_frame_flags_t flags);

#define ALT_MIN_LAG 3
static INLINE int is_altref_enabled(const AV1_COMP *const cpi) {
  return cpi->oxcf.lag_in_frames >= ALT_MIN_LAG && cpi->oxcf.enable_auto_arf;
}

// TODO(zoeliu): To set up cpi->oxcf.enable_auto_brf

static INLINE void set_ref_ptrs(const AV1_COMMON *cm, MACROBLOCKD *xd,
                                MV_REFERENCE_FRAME ref0,
                                MV_REFERENCE_FRAME ref1) {
  xd->block_refs[0] =
      &cm->frame_refs[ref0 >= LAST_FRAME ? ref0 - LAST_FRAME : 0];
  xd->block_refs[1] =
      &cm->frame_refs[ref1 >= LAST_FRAME ? ref1 - LAST_FRAME : 0];
}

static INLINE int get_chessboard_index(int frame_index) {
  return frame_index & 0x1;
}

static INLINE int *cond_cost_list(const struct AV1_COMP *cpi, int *cost_list) {
  return cpi->sf.mv.subpel_search_method != SUBPEL_TREE ? cost_list : NULL;
}

void av1_new_framerate(AV1_COMP *cpi, double framerate);

#define LAYER_IDS_TO_IDX(sl, tl, num_tl) ((sl) * (num_tl) + (tl))

// Update up-sampled reference frame index.
static INLINE void uref_cnt_fb(EncRefCntBuffer *ubufs, int *uidx,
                               int new_uidx) {
  const int ref_index = *uidx;

  if (ref_index >= 0 && ubufs[ref_index].ref_count > 0)
    ubufs[ref_index].ref_count--;

  *uidx = new_uidx;
  ubufs[new_uidx].ref_count++;
}

// Returns 1 if a frame is scaled and 0 otherwise.
static INLINE int av1_resize_scaled(const AV1_COMMON *cm) {
  return !(cm->superres_upscaled_width == cm->render_width &&
           cm->superres_upscaled_height == cm->render_height);
}

static INLINE int av1_frame_scaled(const AV1_COMMON *cm) {
  return !av1_superres_scaled(cm) && av1_resize_scaled(cm);
}

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // AV1_ENCODER_ENCODER_H_
