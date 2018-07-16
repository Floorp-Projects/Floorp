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

#ifndef AV1_COMMON_ONYXC_INT_H_
#define AV1_COMMON_ONYXC_INT_H_

#include "config/aom_config.h"
#include "config/av1_rtcd.h"

#include "aom/internal/aom_codec_internal.h"
#include "aom_util/aom_thread.h"
#include "av1/common/alloccommon.h"
#include "av1/common/av1_loopfilter.h"
#include "av1/common/entropy.h"
#include "av1/common/entropymode.h"
#include "av1/common/entropymv.h"
#include "av1/common/enums.h"
#include "av1/common/frame_buffers.h"
#include "av1/common/mv.h"
#include "av1/common/quant_common.h"
#include "av1/common/restoration.h"
#include "av1/common/tile_common.h"
#include "av1/common/timing.h"
#include "av1/common/odintrin.h"
#include "av1/encoder/hash_motion.h"
#include "aom_dsp/grain_synthesis.h"
#include "aom_dsp/grain_table.h"
#ifdef __cplusplus
extern "C" {
#endif

#if defined(__clang__) && defined(__has_warning)
#if __has_feature(cxx_attributes) && __has_warning("-Wimplicit-fallthrough")
#define AOM_FALLTHROUGH_INTENDED [[clang::fallthrough]]  // NOLINT
#endif
#elif defined(__GNUC__) && __GNUC__ >= 7
#define AOM_FALLTHROUGH_INTENDED __attribute__((fallthrough))  // NOLINT
#endif

#ifndef AOM_FALLTHROUGH_INTENDED
#define AOM_FALLTHROUGH_INTENDED \
  do {                           \
  } while (0)
#endif

#define CDEF_MAX_STRENGTHS 16

/* Constant values while waiting for the sequence header */
#define FRAME_ID_LENGTH 15
#define DELTA_FRAME_ID_LENGTH 14

#define FRAME_CONTEXTS (FRAME_BUFFERS + 1)
// Extra frame context which is always kept at default values
#define FRAME_CONTEXT_DEFAULTS (FRAME_CONTEXTS - 1)
#define PRIMARY_REF_BITS 3
#define PRIMARY_REF_NONE 7

#define NUM_PING_PONG_BUFFERS 2

#define MAX_NUM_TEMPORAL_LAYERS 8
#define MAX_NUM_SPATIAL_LAYERS 4
/* clang-format off */
// clang-format seems to think this is a pointer dereference and not a
// multiplication.
#define MAX_NUM_OPERATING_POINTS \
  MAX_NUM_TEMPORAL_LAYERS * MAX_NUM_SPATIAL_LAYERS
/* clang-format on*/

// TODO(jingning): Turning this on to set up transform coefficient
// processing timer.
#define TXCOEFF_TIMER 0
#define TXCOEFF_COST_TIMER 0

typedef enum {
  SINGLE_REFERENCE = 0,
  COMPOUND_REFERENCE = 1,
  REFERENCE_MODE_SELECT = 2,
  REFERENCE_MODES = 3,
} REFERENCE_MODE;

typedef enum {
  /**
   * Frame context updates are disabled
   */
  REFRESH_FRAME_CONTEXT_DISABLED,
  /**
   * Update frame context to values resulting from backward probability
   * updates based on entropy/counts in the decoded frame
   */
  REFRESH_FRAME_CONTEXT_BACKWARD,
} REFRESH_FRAME_CONTEXT_MODE;

#define MFMV_STACK_SIZE 3
typedef struct {
  int_mv mfmv0;
  uint8_t ref_frame_offset;
} TPL_MV_REF;

typedef struct {
  int_mv mv;
  MV_REFERENCE_FRAME ref_frame;
} MV_REF;

typedef struct {
  int ref_count;

  unsigned int cur_frame_offset;
  unsigned int ref_frame_offset[INTER_REFS_PER_FRAME];

  MV_REF *mvs;
  uint8_t *seg_map;
  struct segmentation seg;
  int mi_rows;
  int mi_cols;
  // Width and height give the size of the buffer (before any upscaling, unlike
  // the sizes that can be derived from the buf structure)
  int width;
  int height;
  WarpedMotionParams global_motion[REF_FRAMES];
  int showable_frame;  // frame can be used as show existing frame in future
  int film_grain_params_present;
  aom_film_grain_t film_grain_params;
  aom_codec_frame_buffer_t raw_frame_buffer;
  YV12_BUFFER_CONFIG buf;
  hash_table hash_table;
  uint8_t intra_only;
  FRAME_TYPE frame_type;
  // The Following variables will only be used in frame parallel decode.

  // frame_worker_owner indicates which FrameWorker owns this buffer. NULL means
  // that no FrameWorker owns, or is decoding, this buffer.
  AVxWorker *frame_worker_owner;

  // row and col indicate which position frame has been decoded to in real
  // pixel unit. They are reset to -1 when decoding begins and set to INT_MAX
  // when the frame is fully decoded.
  int row;
  int col;

  // Inter frame reference frame delta for loop filter
  int8_t ref_deltas[REF_FRAMES];

  // 0 = ZERO_MV, MV
  int8_t mode_deltas[MAX_MODE_LF_DELTAS];
} RefCntBuffer;

typedef struct BufferPool {
// Protect BufferPool from being accessed by several FrameWorkers at
// the same time during frame parallel decode.
// TODO(hkuang): Try to use atomic variable instead of locking the whole pool.
#if CONFIG_MULTITHREAD
  pthread_mutex_t pool_mutex;
#endif

  // Private data associated with the frame buffer callbacks.
  void *cb_priv;

  aom_get_frame_buffer_cb_fn_t get_fb_cb;
  aom_release_frame_buffer_cb_fn_t release_fb_cb;

  RefCntBuffer frame_bufs[FRAME_BUFFERS];

  // Frame buffers allocated internally by the codec.
  InternalFrameBufferList int_frame_buffers;
} BufferPool;

typedef struct {
  int base_ctx_table[2 /*row*/][2 /*col*/][3 /*sig_map*/]
                    [BASE_CONTEXT_POSITION_NUM + 1];
} LV_MAP_CTX_TABLE;
typedef int BASE_CTX_TABLE[2 /*col*/][3 /*sig_map*/]
                          [BASE_CONTEXT_POSITION_NUM + 1];

typedef struct BitstreamLevel {
  uint8_t major;
  uint8_t minor;
} BitstreamLevel;

/* Initial version of sequence header structure */
typedef struct SequenceHeader {
  int num_bits_width;
  int num_bits_height;
  int max_frame_width;
  int max_frame_height;
  int frame_id_numbers_present_flag;
  int frame_id_length;
  int delta_frame_id_length;
  BLOCK_SIZE sb_size;  // Size of the superblock used for this frame
  int mib_size;        // Size of the superblock in units of MI blocks
  int mib_size_log2;   // Log 2 of above.
  int order_hint_bits_minus_1;
  int force_screen_content_tools;  // 0 - force off
                                   // 1 - force on
                                   // 2 - adaptive
  int force_integer_mv;            // 0 - Not to force. MV can be in 1/4 or 1/8
                                   // 1 - force to integer
                                   // 2 - adaptive
  int still_picture;               // Video is a single frame still picture
  int reduced_still_picture_hdr;   // Use reduced header for still picture
  int monochrome;                  // Monochorme video
  int enable_filter_intra;         // enables/disables filterintra
  int enable_intra_edge_filter;    // enables/disables corner/edge/upsampling
  int enable_interintra_compound;  // enables/disables interintra_compound
  int enable_masked_compound;      // enables/disables masked compound
  int enable_dual_filter;          // 0 - disable dual interpolation filter
                                   // 1 - enable vert/horiz filter selection
  int enable_order_hint;           // 0 - disable order hint, and related tools
                                   // jnt_comp, ref_frame_mvs, frame_sign_bias
                                   // if 0, enable_jnt_comp and
                                   // enable_ref_frame_mvs must be set zs 0.
  int enable_jnt_comp;             // 0 - disable joint compound modes
                                   // 1 - enable it
  int enable_ref_frame_mvs;        // 0 - disable ref frame mvs
                                   // 1 - enable it
  int enable_warped_motion;        // 0 - disable warped motion for sequence
                                   // 1 - enable it for the sequence
  int enable_superres;     // 0 - Disable superres for the sequence, and disable
                           //     transmitting per-frame superres enabled flag.
                           // 1 - Enable superres for the sequence, and also
                           //     enable per-frame flag to denote if superres is
                           //     enabled for that frame.
  int enable_cdef;         // To turn on/off CDEF
  int enable_restoration;  // To turn on/off loop restoration
  int operating_points_cnt_minus_1;
  int operating_point_idc[MAX_NUM_OPERATING_POINTS];
  int display_model_info_present_flag;
  int decoder_model_info_present_flag;
  BitstreamLevel level[MAX_NUM_OPERATING_POINTS];
  uint8_t tier[MAX_NUM_OPERATING_POINTS];  // seq_tier in the spec. One bit: 0
                                           // or 1.
} SequenceHeader;

typedef struct AV1Common {
  struct aom_internal_error_info error;
  aom_color_primaries_t color_primaries;
  aom_transfer_characteristics_t transfer_characteristics;
  aom_matrix_coefficients_t matrix_coefficients;
  aom_chroma_sample_position_t chroma_sample_position;
  int color_range;
  int width;
  int height;
  int render_width;
  int render_height;
  int last_width;
  int last_height;
  int timing_info_present;
  aom_timing_info_t timing_info;
  int buffer_removal_delay_present;
  aom_dec_model_info_t buffer_model;
  aom_dec_model_op_parameters_t op_params[MAX_NUM_OPERATING_POINTS + 1];
  aom_op_timing_info_t op_frame_timing[MAX_NUM_OPERATING_POINTS + 1];
  int tu_presentation_delay_flag;
  int64_t tu_presentation_delay;

  // TODO(jkoleszar): this implies chroma ss right now, but could vary per
  // plane. Revisit as part of the future change to YV12_BUFFER_CONFIG to
  // support additional planes.
  int subsampling_x;
  int subsampling_y;

  int largest_tile_id;
  size_t largest_tile_size;
  int context_update_tile_id;

  // Scale of the current frame with respect to itself.
  struct scale_factors sf_identity;

  // Marks if we need to use 16bit frame buffers (1: yes, 0: no).
  int use_highbitdepth;
  YV12_BUFFER_CONFIG *frame_to_show;
  RefCntBuffer *prev_frame;

  // TODO(hkuang): Combine this with cur_buf in macroblockd.
  RefCntBuffer *cur_frame;

  int ref_frame_map[REF_FRAMES]; /* maps fb_idx to reference slot */

  // Prepare ref_frame_map for the next frame.
  // Only used in frame parallel decode.
  int next_ref_frame_map[REF_FRAMES];

  // TODO(jkoleszar): could expand active_ref_idx to 4, with 0 as intra, and
  // roll new_fb_idx into it.

  // Each Inter frame can reference INTER_REFS_PER_FRAME buffers
  RefBuffer frame_refs[INTER_REFS_PER_FRAME];
  int is_skip_mode_allowed;
  int skip_mode_flag;
  int ref_frame_idx_0;
  int ref_frame_idx_1;

  int new_fb_idx;

  FRAME_TYPE last_frame_type; /* last frame's frame type for motion search.*/
  FRAME_TYPE frame_type;

  int show_frame;
  int showable_frame;  // frame can be used as show existing frame in future
  int last_show_frame;
  int show_existing_frame;
  // Flag for a frame used as a reference - not written to the bitstream
  int is_reference_frame;
  int reset_decoder_state;

  // Flag signaling that the frame is encoded using only INTRA modes.
  uint8_t intra_only;
  uint8_t last_intra_only;
  uint8_t disable_cdf_update;
  int allow_high_precision_mv;
  int cur_frame_force_integer_mv;  // 0 the default in AOM, 1 only integer

  int allow_screen_content_tools;
  int allow_intrabc;
  int allow_warped_motion;

  // MBs, mb_rows/cols is in 16-pixel units; mi_rows/cols is in
  // MB_MODE_INFO (8-pixel) units.
  int MBs;
  int mb_rows, mi_rows;
  int mb_cols, mi_cols;
  int mi_stride;

  /* profile settings */
  TX_MODE tx_mode;

#if CONFIG_ENTROPY_STATS
  int coef_cdf_category;
#endif

  int base_qindex;
  int y_dc_delta_q;
  int u_dc_delta_q;
  int v_dc_delta_q;
  int u_ac_delta_q;
  int v_ac_delta_q;

  int separate_uv_delta_q;

  // The dequantizers below are true dequntizers used only in the
  // dequantization process.  They have the same coefficient
  // shift/scale as TX.
  int16_t y_dequant_QTX[MAX_SEGMENTS][2];
  int16_t u_dequant_QTX[MAX_SEGMENTS][2];
  int16_t v_dequant_QTX[MAX_SEGMENTS][2];

  // Global quant matrix tables
  const qm_val_t *giqmatrix[NUM_QM_LEVELS][3][TX_SIZES_ALL];
  const qm_val_t *gqmatrix[NUM_QM_LEVELS][3][TX_SIZES_ALL];

  // Local quant matrix tables for each frame
  const qm_val_t *y_iqmatrix[MAX_SEGMENTS][TX_SIZES_ALL];
  const qm_val_t *u_iqmatrix[MAX_SEGMENTS][TX_SIZES_ALL];
  const qm_val_t *v_iqmatrix[MAX_SEGMENTS][TX_SIZES_ALL];

  // Encoder
  int using_qmatrix;
  int qm_y;
  int qm_u;
  int qm_v;
  int min_qmlevel;
  int max_qmlevel;

  /* We allocate a MB_MODE_INFO struct for each macroblock, together with
     an extra row on top and column on the left to simplify prediction. */
  int mi_alloc_size;
  MB_MODE_INFO *mip; /* Base of allocated array */
  MB_MODE_INFO *mi;  /* Corresponds to upper left visible macroblock */

  // TODO(agrange): Move prev_mi into encoder structure.
  // prev_mip and prev_mi will only be allocated in encoder.
  MB_MODE_INFO *prev_mip; /* MB_MODE_INFO array 'mip' from last decoded frame */
  MB_MODE_INFO *prev_mi;  /* 'mi' from last frame (points into prev_mip) */

  // Separate mi functions between encoder and decoder.
  int (*alloc_mi)(struct AV1Common *cm, int mi_size);
  void (*free_mi)(struct AV1Common *cm);
  void (*setup_mi)(struct AV1Common *cm);

  // Grid of pointers to 8x8 MB_MODE_INFO structs.  Any 8x8 not in the visible
  // area will be NULL.
  MB_MODE_INFO **mi_grid_base;
  MB_MODE_INFO **mi_grid_visible;
  MB_MODE_INFO **prev_mi_grid_base;
  MB_MODE_INFO **prev_mi_grid_visible;

  // Whether to use previous frames' motion vectors for prediction.
  int allow_ref_frame_mvs;

  uint8_t *last_frame_seg_map;
  uint8_t *current_frame_seg_map;
  int seg_map_alloc_size;

  InterpFilter interp_filter;

  int switchable_motion_mode;

  loop_filter_info_n lf_info;
  // The denominator of the superres scale; the numerator is fixed.
  uint8_t superres_scale_denominator;
  int superres_upscaled_width;
  int superres_upscaled_height;
  RestorationInfo rst_info[MAX_MB_PLANE];

  // rst_end_stripe[i] is one more than the index of the bottom stripe
  // for tile row i.
  int rst_end_stripe[MAX_TILE_ROWS];

  // Pointer to a scratch buffer used by self-guided restoration
  int32_t *rst_tmpbuf;
  RestorationLineBuffers *rlbs;

  // Output of loop restoration
  YV12_BUFFER_CONFIG rst_frame;

  // Flag signaling how frame contexts should be updated at the end of
  // a frame decode
  REFRESH_FRAME_CONTEXT_MODE refresh_frame_context;

  int ref_frame_sign_bias[REF_FRAMES]; /* Two state 0, 1 */

  struct loopfilter lf;
  struct segmentation seg;
  int coded_lossless;  // frame is fully lossless at the coded resolution.
  int all_lossless;    // frame is fully lossless at the upscaled resolution.

  int reduced_tx_set_used;

  // Context probabilities for reference frame prediction
  MV_REFERENCE_FRAME comp_fwd_ref[FWD_REFS];
  MV_REFERENCE_FRAME comp_bwd_ref[BWD_REFS];
  REFERENCE_MODE reference_mode;

  FRAME_CONTEXT *fc;              /* this frame entropy */
  FRAME_CONTEXT *frame_contexts;  // FRAME_CONTEXTS
  unsigned int frame_context_idx; /* Context to use/update */
  int fb_of_context_type[REF_FRAMES];
  int primary_ref_frame;

  unsigned int frame_offset;

  unsigned int current_video_frame;
  BITSTREAM_PROFILE profile;

  // AOM_BITS_8 in profile 0 or 1, AOM_BITS_10 or AOM_BITS_12 in profile 2 or 3.
  aom_bit_depth_t bit_depth;
  aom_bit_depth_t dequant_bit_depth;  // bit_depth of current dequantizer

  int error_resilient_mode;
  int force_primary_ref_none;

  int tile_cols, tile_rows;
  int last_tile_cols, last_tile_rows;

  int max_tile_width_sb;
  int min_log2_tile_cols;
  int max_log2_tile_cols;
  int max_log2_tile_rows;
  int min_log2_tile_rows;
  int min_log2_tiles;
  int max_tile_height_sb;
  int uniform_tile_spacing_flag;
  int log2_tile_cols;                        // only valid for uniform tiles
  int log2_tile_rows;                        // only valid for uniform tiles
  int tile_col_start_sb[MAX_TILE_COLS + 1];  // valid for 0 <= i <= tile_cols
  int tile_row_start_sb[MAX_TILE_ROWS + 1];  // valid for 0 <= i <= tile_rows
  int tile_width, tile_height;               // In MI units

  unsigned int large_scale_tile;
  unsigned int single_tile_decoding;

  int byte_alignment;
  int skip_loop_filter;

  // Private data associated with the frame buffer callbacks.
  void *cb_priv;
  aom_get_frame_buffer_cb_fn_t get_fb_cb;
  aom_release_frame_buffer_cb_fn_t release_fb_cb;

  // Handles memory for the codec.
  InternalFrameBufferList int_frame_buffers;

  // External BufferPool passed from outside.
  BufferPool *buffer_pool;

  PARTITION_CONTEXT **above_seg_context;
  ENTROPY_CONTEXT **above_context[MAX_MB_PLANE];
  TXFM_CONTEXT **above_txfm_context;
  WarpedMotionParams global_motion[REF_FRAMES];
  aom_film_grain_table_t *film_grain_table;
  int film_grain_params_present;
  aom_film_grain_t film_grain_params;
  int cdef_pri_damping;
  int cdef_sec_damping;
  int nb_cdef_strengths;
  int cdef_strengths[CDEF_MAX_STRENGTHS];
  int cdef_uv_strengths[CDEF_MAX_STRENGTHS];
  int cdef_bits;

  int delta_q_present_flag;
  // Resolution of delta quant
  int delta_q_res;
  int delta_lf_present_flag;
  // Resolution of delta lf level
  int delta_lf_res;
  // This is a flag for number of deltas of loop filter level
  // 0: use 1 delta, for y_vertical, y_horizontal, u, and v
  // 1: use separate deltas for each filter level
  int delta_lf_multi;
  int num_tg;
  SequenceHeader seq_params;
  int current_frame_id;
  int ref_frame_id[REF_FRAMES];
  int valid_for_referencing[REF_FRAMES];
  int invalid_delta_frame_id_minus_1;
  LV_MAP_CTX_TABLE coeff_ctx_table;
  TPL_MV_REF *tpl_mvs;
  int tpl_mvs_mem_size;
  // TODO(jingning): This can be combined with sign_bias later.
  int8_t ref_frame_side[REF_FRAMES];

  int is_annexb;

  int frame_refs_short_signaling;
  int temporal_layer_id;
  int spatial_layer_id;
  unsigned int number_temporal_layers;
  unsigned int number_spatial_layers;
  int num_allocated_above_context_mi_col;
  int num_allocated_above_contexts;
  int num_allocated_above_context_planes;

#if TXCOEFF_TIMER
  int64_t cum_txcoeff_timer;
  int64_t txcoeff_timer;
  int txb_count;
#endif

#if TXCOEFF_COST_TIMER
  int64_t cum_txcoeff_cost_timer;
  int64_t txcoeff_cost_timer;
  int64_t txcoeff_cost_count;
#endif
  const cfg_options_t *options;
} AV1_COMMON;

// TODO(hkuang): Don't need to lock the whole pool after implementing atomic
// frame reference count.
static void lock_buffer_pool(BufferPool *const pool) {
#if CONFIG_MULTITHREAD
  pthread_mutex_lock(&pool->pool_mutex);
#else
  (void)pool;
#endif
}

static void unlock_buffer_pool(BufferPool *const pool) {
#if CONFIG_MULTITHREAD
  pthread_mutex_unlock(&pool->pool_mutex);
#else
  (void)pool;
#endif
}

static INLINE YV12_BUFFER_CONFIG *get_ref_frame(AV1_COMMON *cm, int index) {
  if (index < 0 || index >= REF_FRAMES) return NULL;
  if (cm->ref_frame_map[index] < 0) return NULL;
  assert(cm->ref_frame_map[index] < FRAME_BUFFERS);
  return &cm->buffer_pool->frame_bufs[cm->ref_frame_map[index]].buf;
}

static INLINE YV12_BUFFER_CONFIG *get_frame_new_buffer(
    const AV1_COMMON *const cm) {
  return &cm->buffer_pool->frame_bufs[cm->new_fb_idx].buf;
}

static INLINE int get_free_fb(AV1_COMMON *cm) {
  RefCntBuffer *const frame_bufs = cm->buffer_pool->frame_bufs;
  int i;

  lock_buffer_pool(cm->buffer_pool);
  for (i = 0; i < FRAME_BUFFERS; ++i)
    if (frame_bufs[i].ref_count == 0) break;

  if (i != FRAME_BUFFERS) {
    if (frame_bufs[i].buf.use_external_refernce_buffers) {
      // If this frame buffer's y_buffer, u_buffer, and v_buffer point to the
      // external reference buffers. Restore the buffer pointers to point to the
      // internally allocated memory.
      YV12_BUFFER_CONFIG *ybf = &frame_bufs[i].buf;
      ybf->y_buffer = ybf->store_buf_adr[0];
      ybf->u_buffer = ybf->store_buf_adr[1];
      ybf->v_buffer = ybf->store_buf_adr[2];
      ybf->use_external_refernce_buffers = 0;
    }

    frame_bufs[i].ref_count = 1;
  } else {
    // Reset i to be INVALID_IDX to indicate no free buffer found.
    i = INVALID_IDX;
  }

  unlock_buffer_pool(cm->buffer_pool);
  return i;
}

static INLINE void ref_cnt_fb(RefCntBuffer *bufs, int *idx, int new_idx) {
  const int ref_index = *idx;

  if (ref_index >= 0 && bufs[ref_index].ref_count > 0)
    bufs[ref_index].ref_count--;

  *idx = new_idx;

  bufs[new_idx].ref_count++;
}

static INLINE int frame_is_intra_only(const AV1_COMMON *const cm) {
  return cm->frame_type == KEY_FRAME || cm->intra_only;
}

static INLINE int frame_is_sframe(const AV1_COMMON *cm) {
  return cm->frame_type == S_FRAME;
}

static INLINE RefCntBuffer *get_prev_frame(const AV1_COMMON *const cm) {
  if (cm->primary_ref_frame == PRIMARY_REF_NONE ||
      cm->frame_refs[cm->primary_ref_frame].idx == INVALID_IDX) {
    return NULL;
  } else {
    return &cm->buffer_pool
                ->frame_bufs[cm->frame_refs[cm->primary_ref_frame].idx];
  }
}

// Returns 1 if this frame might allow mvs from some reference frame.
static INLINE int frame_might_allow_ref_frame_mvs(const AV1_COMMON *cm) {
  return !cm->error_resilient_mode && cm->seq_params.enable_ref_frame_mvs &&
         cm->seq_params.enable_order_hint && !frame_is_intra_only(cm);
}

// Returns 1 if this frame might use warped_motion
static INLINE int frame_might_allow_warped_motion(const AV1_COMMON *cm) {
  return !cm->error_resilient_mode && !frame_is_intra_only(cm) &&
         cm->seq_params.enable_warped_motion;
}

static INLINE void ensure_mv_buffer(RefCntBuffer *buf, AV1_COMMON *cm) {
  const int buf_rows = buf->mi_rows;
  const int buf_cols = buf->mi_cols;

  if (buf->mvs == NULL || buf_rows != cm->mi_rows || buf_cols != cm->mi_cols) {
    aom_free(buf->mvs);
    buf->mi_rows = cm->mi_rows;
    buf->mi_cols = cm->mi_cols;
    CHECK_MEM_ERROR(cm, buf->mvs,
                    (MV_REF *)aom_calloc(
                        ((cm->mi_rows + 1) >> 1) * ((cm->mi_cols + 1) >> 1),
                        sizeof(*buf->mvs)));
    aom_free(buf->seg_map);
    CHECK_MEM_ERROR(cm, buf->seg_map,
                    (uint8_t *)aom_calloc(cm->mi_rows * cm->mi_cols,
                                          sizeof(*buf->seg_map)));
  }

  const int mem_size =
      ((cm->mi_rows + MAX_MIB_SIZE) >> 1) * (cm->mi_stride >> 1);
  int realloc = cm->tpl_mvs == NULL;
  if (cm->tpl_mvs) realloc |= cm->tpl_mvs_mem_size < mem_size;

  if (realloc) {
    aom_free(cm->tpl_mvs);
    CHECK_MEM_ERROR(cm, cm->tpl_mvs,
                    (TPL_MV_REF *)aom_calloc(mem_size, sizeof(*cm->tpl_mvs)));
    cm->tpl_mvs_mem_size = mem_size;
  }
}

static INLINE int mi_cols_aligned_to_sb(const AV1_COMMON *cm) {
  return ALIGN_POWER_OF_TWO(cm->mi_cols, cm->seq_params.mib_size_log2);
}

static INLINE int mi_rows_aligned_to_sb(const AV1_COMMON *cm) {
  return ALIGN_POWER_OF_TWO(cm->mi_rows, cm->seq_params.mib_size_log2);
}

void cfl_init(CFL_CTX *cfl, AV1_COMMON *cm);

static INLINE int av1_num_planes(const AV1_COMMON *cm) {
  return cm->seq_params.monochrome ? 1 : MAX_MB_PLANE;
}

static INLINE void av1_init_above_context(AV1_COMMON *cm, MACROBLOCKD *xd,
                                          const int tile_row) {
  const int num_planes = av1_num_planes(cm);
  for (int i = 0; i < num_planes; ++i) {
    xd->above_context[i] = cm->above_context[i][tile_row];
  }
  xd->above_seg_context = cm->above_seg_context[tile_row];
  xd->above_txfm_context = cm->above_txfm_context[tile_row];
}

static INLINE void av1_init_macroblockd(AV1_COMMON *cm, MACROBLOCKD *xd,
                                        tran_low_t *dqcoeff) {
  const int num_planes = av1_num_planes(cm);
  for (int i = 0; i < num_planes; ++i) {
    xd->plane[i].dqcoeff = dqcoeff;

    if (xd->plane[i].plane_type == PLANE_TYPE_Y) {
      memcpy(xd->plane[i].seg_dequant_QTX, cm->y_dequant_QTX,
             sizeof(cm->y_dequant_QTX));
      memcpy(xd->plane[i].seg_iqmatrix, cm->y_iqmatrix, sizeof(cm->y_iqmatrix));

    } else {
      if (i == AOM_PLANE_U) {
        memcpy(xd->plane[i].seg_dequant_QTX, cm->u_dequant_QTX,
               sizeof(cm->u_dequant_QTX));
        memcpy(xd->plane[i].seg_iqmatrix, cm->u_iqmatrix,
               sizeof(cm->u_iqmatrix));
      } else {
        memcpy(xd->plane[i].seg_dequant_QTX, cm->v_dequant_QTX,
               sizeof(cm->v_dequant_QTX));
        memcpy(xd->plane[i].seg_iqmatrix, cm->v_iqmatrix,
               sizeof(cm->v_iqmatrix));
      }
    }
  }
  xd->mi_stride = cm->mi_stride;
  xd->error_info = &cm->error;
  cfl_init(&xd->cfl, cm);
}

static INLINE void set_skip_context(MACROBLOCKD *xd, int mi_row, int mi_col,
                                    const int num_planes) {
  int i;
  int row_offset = mi_row;
  int col_offset = mi_col;
  for (i = 0; i < num_planes; ++i) {
    struct macroblockd_plane *const pd = &xd->plane[i];
    // Offset the buffer pointer
    const BLOCK_SIZE bsize = xd->mi[0]->sb_type;
    if (pd->subsampling_y && (mi_row & 0x01) && (mi_size_high[bsize] == 1))
      row_offset = mi_row - 1;
    if (pd->subsampling_x && (mi_col & 0x01) && (mi_size_wide[bsize] == 1))
      col_offset = mi_col - 1;
    int above_idx = col_offset;
    int left_idx = row_offset & MAX_MIB_MASK;
    pd->above_context = &xd->above_context[i][above_idx >> pd->subsampling_x];
    pd->left_context = &xd->left_context[i][left_idx >> pd->subsampling_y];
  }
}

static INLINE int calc_mi_size(int len) {
  // len is in mi units. Align to a multiple of SBs.
  return ALIGN_POWER_OF_TWO(len, MAX_MIB_SIZE_LOG2);
}

static INLINE void set_plane_n4(MACROBLOCKD *const xd, int bw, int bh,
                                const int num_planes) {
  int i;
  for (i = 0; i < num_planes; i++) {
    xd->plane[i].width = (bw * MI_SIZE) >> xd->plane[i].subsampling_x;
    xd->plane[i].height = (bh * MI_SIZE) >> xd->plane[i].subsampling_y;

    xd->plane[i].width = AOMMAX(xd->plane[i].width, 4);
    xd->plane[i].height = AOMMAX(xd->plane[i].height, 4);
  }
}

static INLINE void set_mi_row_col(MACROBLOCKD *xd, const TileInfo *const tile,
                                  int mi_row, int bh, int mi_col, int bw,
                                  int mi_rows, int mi_cols) {
  xd->mb_to_top_edge = -((mi_row * MI_SIZE) * 8);
  xd->mb_to_bottom_edge = ((mi_rows - bh - mi_row) * MI_SIZE) * 8;
  xd->mb_to_left_edge = -((mi_col * MI_SIZE) * 8);
  xd->mb_to_right_edge = ((mi_cols - bw - mi_col) * MI_SIZE) * 8;

  // Are edges available for intra prediction?
  xd->up_available = (mi_row > tile->mi_row_start);

  const int ss_x = xd->plane[1].subsampling_x;
  const int ss_y = xd->plane[1].subsampling_y;

  xd->left_available = (mi_col > tile->mi_col_start);
  xd->chroma_up_available = xd->up_available;
  xd->chroma_left_available = xd->left_available;
  if (ss_x && bw < mi_size_wide[BLOCK_8X8])
    xd->chroma_left_available = (mi_col - 1) > tile->mi_col_start;
  if (ss_y && bh < mi_size_high[BLOCK_8X8])
    xd->chroma_up_available = (mi_row - 1) > tile->mi_row_start;
  if (xd->up_available) {
    xd->above_mbmi = xd->mi[-xd->mi_stride];
  } else {
    xd->above_mbmi = NULL;
  }

  if (xd->left_available) {
    xd->left_mbmi = xd->mi[-1];
  } else {
    xd->left_mbmi = NULL;
  }

  const int chroma_ref = ((mi_row & 0x01) || !(bh & 0x01) || !ss_y) &&
                         ((mi_col & 0x01) || !(bw & 0x01) || !ss_x);
  if (chroma_ref) {
    // To help calculate the "above" and "left" chroma blocks, note that the
    // current block may cover multiple luma blocks (eg, if partitioned into
    // 4x4 luma blocks).
    // First, find the top-left-most luma block covered by this chroma block
    MB_MODE_INFO **base_mi =
        &xd->mi[-(mi_row & ss_y) * xd->mi_stride - (mi_col & ss_x)];

    // Then, we consider the luma region covered by the left or above 4x4 chroma
    // prediction. We want to point to the chroma reference block in that
    // region, which is the bottom-right-most mi unit.
    // This leads to the following offsets:
    MB_MODE_INFO *chroma_above_mi =
        xd->chroma_up_available ? base_mi[-xd->mi_stride + ss_x] : NULL;
    xd->chroma_above_mbmi = chroma_above_mi;

    MB_MODE_INFO *chroma_left_mi =
        xd->chroma_left_available ? base_mi[ss_y * xd->mi_stride - 1] : NULL;
    xd->chroma_left_mbmi = chroma_left_mi;
  }

  xd->n8_h = bh;
  xd->n8_w = bw;
  xd->is_sec_rect = 0;
  if (xd->n8_w < xd->n8_h) {
    // Only mark is_sec_rect as 1 for the last block.
    // For PARTITION_VERT_4, it would be (0, 0, 0, 1);
    // For other partitions, it would be (0, 1).
    if (!((mi_col + xd->n8_w) & (xd->n8_h - 1))) xd->is_sec_rect = 1;
  }

  if (xd->n8_w > xd->n8_h)
    if (mi_row & (xd->n8_w - 1)) xd->is_sec_rect = 1;
}

static INLINE aom_cdf_prob *get_y_mode_cdf(FRAME_CONTEXT *tile_ctx,
                                           const MB_MODE_INFO *above_mi,
                                           const MB_MODE_INFO *left_mi) {
  const PREDICTION_MODE above = av1_above_block_mode(above_mi);
  const PREDICTION_MODE left = av1_left_block_mode(left_mi);
  const int above_ctx = intra_mode_context[above];
  const int left_ctx = intra_mode_context[left];
  return tile_ctx->kf_y_cdf[above_ctx][left_ctx];
}

static INLINE void update_partition_context(MACROBLOCKD *xd, int mi_row,
                                            int mi_col, BLOCK_SIZE subsize,
                                            BLOCK_SIZE bsize) {
  PARTITION_CONTEXT *const above_ctx = xd->above_seg_context + mi_col;
  PARTITION_CONTEXT *const left_ctx =
      xd->left_seg_context + (mi_row & MAX_MIB_MASK);

  const int bw = mi_size_wide[bsize];
  const int bh = mi_size_high[bsize];
  memset(above_ctx, partition_context_lookup[subsize].above, bw);
  memset(left_ctx, partition_context_lookup[subsize].left, bh);
}

static INLINE int is_chroma_reference(int mi_row, int mi_col, BLOCK_SIZE bsize,
                                      int subsampling_x, int subsampling_y) {
  const int bw = mi_size_wide[bsize];
  const int bh = mi_size_high[bsize];
  int ref_pos = ((mi_row & 0x01) || !(bh & 0x01) || !subsampling_y) &&
                ((mi_col & 0x01) || !(bw & 0x01) || !subsampling_x);
  return ref_pos;
}

static INLINE BLOCK_SIZE scale_chroma_bsize(BLOCK_SIZE bsize, int subsampling_x,
                                            int subsampling_y) {
  BLOCK_SIZE bs = bsize;
  switch (bsize) {
    case BLOCK_4X4:
      if (subsampling_x == 1 && subsampling_y == 1)
        bs = BLOCK_8X8;
      else if (subsampling_x == 1)
        bs = BLOCK_8X4;
      else if (subsampling_y == 1)
        bs = BLOCK_4X8;
      break;
    case BLOCK_4X8:
      if (subsampling_x == 1 && subsampling_y == 1)
        bs = BLOCK_8X8;
      else if (subsampling_x == 1)
        bs = BLOCK_8X8;
      else if (subsampling_y == 1)
        bs = BLOCK_4X8;
      break;
    case BLOCK_8X4:
      if (subsampling_x == 1 && subsampling_y == 1)
        bs = BLOCK_8X8;
      else if (subsampling_x == 1)
        bs = BLOCK_8X4;
      else if (subsampling_y == 1)
        bs = BLOCK_8X8;
      break;
    case BLOCK_4X16:
      if (subsampling_x == 1 && subsampling_y == 1)
        bs = BLOCK_8X16;
      else if (subsampling_x == 1)
        bs = BLOCK_8X16;
      else if (subsampling_y == 1)
        bs = BLOCK_4X16;
      break;
    case BLOCK_16X4:
      if (subsampling_x == 1 && subsampling_y == 1)
        bs = BLOCK_16X8;
      else if (subsampling_x == 1)
        bs = BLOCK_16X4;
      else if (subsampling_y == 1)
        bs = BLOCK_16X8;
      break;
    default: break;
  }
  return bs;
}

static INLINE aom_cdf_prob cdf_element_prob(const aom_cdf_prob *cdf,
                                            size_t element) {
  assert(cdf != NULL);
  return (element > 0 ? cdf[element - 1] : CDF_PROB_TOP) - cdf[element];
}

static INLINE void partition_gather_horz_alike(aom_cdf_prob *out,
                                               const aom_cdf_prob *const in,
                                               BLOCK_SIZE bsize) {
  (void)bsize;
  out[0] = CDF_PROB_TOP;
  out[0] -= cdf_element_prob(in, PARTITION_HORZ);
  out[0] -= cdf_element_prob(in, PARTITION_SPLIT);
  out[0] -= cdf_element_prob(in, PARTITION_HORZ_A);
  out[0] -= cdf_element_prob(in, PARTITION_HORZ_B);
  out[0] -= cdf_element_prob(in, PARTITION_VERT_A);
  if (bsize != BLOCK_128X128) out[0] -= cdf_element_prob(in, PARTITION_HORZ_4);
  out[0] = AOM_ICDF(out[0]);
  out[1] = AOM_ICDF(CDF_PROB_TOP);
}

static INLINE void partition_gather_vert_alike(aom_cdf_prob *out,
                                               const aom_cdf_prob *const in,
                                               BLOCK_SIZE bsize) {
  (void)bsize;
  out[0] = CDF_PROB_TOP;
  out[0] -= cdf_element_prob(in, PARTITION_VERT);
  out[0] -= cdf_element_prob(in, PARTITION_SPLIT);
  out[0] -= cdf_element_prob(in, PARTITION_HORZ_A);
  out[0] -= cdf_element_prob(in, PARTITION_VERT_A);
  out[0] -= cdf_element_prob(in, PARTITION_VERT_B);
  if (bsize != BLOCK_128X128) out[0] -= cdf_element_prob(in, PARTITION_VERT_4);
  out[0] = AOM_ICDF(out[0]);
  out[1] = AOM_ICDF(CDF_PROB_TOP);
}

static INLINE void update_ext_partition_context(MACROBLOCKD *xd, int mi_row,
                                                int mi_col, BLOCK_SIZE subsize,
                                                BLOCK_SIZE bsize,
                                                PARTITION_TYPE partition) {
  if (bsize >= BLOCK_8X8) {
    const int hbs = mi_size_wide[bsize] / 2;
    BLOCK_SIZE bsize2 = get_partition_subsize(bsize, PARTITION_SPLIT);
    switch (partition) {
      case PARTITION_SPLIT:
        if (bsize != BLOCK_8X8) break;
        AOM_FALLTHROUGH_INTENDED;
      case PARTITION_NONE:
      case PARTITION_HORZ:
      case PARTITION_VERT:
      case PARTITION_HORZ_4:
      case PARTITION_VERT_4:
        update_partition_context(xd, mi_row, mi_col, subsize, bsize);
        break;
      case PARTITION_HORZ_A:
        update_partition_context(xd, mi_row, mi_col, bsize2, subsize);
        update_partition_context(xd, mi_row + hbs, mi_col, subsize, subsize);
        break;
      case PARTITION_HORZ_B:
        update_partition_context(xd, mi_row, mi_col, subsize, subsize);
        update_partition_context(xd, mi_row + hbs, mi_col, bsize2, subsize);
        break;
      case PARTITION_VERT_A:
        update_partition_context(xd, mi_row, mi_col, bsize2, subsize);
        update_partition_context(xd, mi_row, mi_col + hbs, subsize, subsize);
        break;
      case PARTITION_VERT_B:
        update_partition_context(xd, mi_row, mi_col, subsize, subsize);
        update_partition_context(xd, mi_row, mi_col + hbs, bsize2, subsize);
        break;
      default: assert(0 && "Invalid partition type");
    }
  }
}

static INLINE int partition_plane_context(const MACROBLOCKD *xd, int mi_row,
                                          int mi_col, BLOCK_SIZE bsize) {
  const PARTITION_CONTEXT *above_ctx = xd->above_seg_context + mi_col;
  const PARTITION_CONTEXT *left_ctx =
      xd->left_seg_context + (mi_row & MAX_MIB_MASK);
  // Minimum partition point is 8x8. Offset the bsl accordingly.
  const int bsl = mi_size_wide_log2[bsize] - mi_size_wide_log2[BLOCK_8X8];
  int above = (*above_ctx >> bsl) & 1, left = (*left_ctx >> bsl) & 1;

  assert(mi_size_wide_log2[bsize] == mi_size_high_log2[bsize]);
  assert(bsl >= 0);

  return (left * 2 + above) + bsl * PARTITION_PLOFFSET;
}

// Return the number of elements in the partition CDF when
// partitioning the (square) block with luma block size of bsize.
static INLINE int partition_cdf_length(BLOCK_SIZE bsize) {
  if (bsize <= BLOCK_8X8)
    return PARTITION_TYPES;
  else if (bsize == BLOCK_128X128)
    return EXT_PARTITION_TYPES - 2;
  else
    return EXT_PARTITION_TYPES;
}

static INLINE int max_block_wide(const MACROBLOCKD *xd, BLOCK_SIZE bsize,
                                 int plane) {
  int max_blocks_wide = block_size_wide[bsize];
  const struct macroblockd_plane *const pd = &xd->plane[plane];

  if (xd->mb_to_right_edge < 0)
    max_blocks_wide += xd->mb_to_right_edge >> (3 + pd->subsampling_x);

  // Scale the width in the transform block unit.
  return max_blocks_wide >> tx_size_wide_log2[0];
}

static INLINE int max_block_high(const MACROBLOCKD *xd, BLOCK_SIZE bsize,
                                 int plane) {
  int max_blocks_high = block_size_high[bsize];
  const struct macroblockd_plane *const pd = &xd->plane[plane];

  if (xd->mb_to_bottom_edge < 0)
    max_blocks_high += xd->mb_to_bottom_edge >> (3 + pd->subsampling_y);

  // Scale the height in the transform block unit.
  return max_blocks_high >> tx_size_high_log2[0];
}

static INLINE int max_intra_block_width(const MACROBLOCKD *xd,
                                        BLOCK_SIZE plane_bsize, int plane,
                                        TX_SIZE tx_size) {
  const int max_blocks_wide = max_block_wide(xd, plane_bsize, plane)
                              << tx_size_wide_log2[0];
  return ALIGN_POWER_OF_TWO(max_blocks_wide, tx_size_wide_log2[tx_size]);
}

static INLINE int max_intra_block_height(const MACROBLOCKD *xd,
                                         BLOCK_SIZE plane_bsize, int plane,
                                         TX_SIZE tx_size) {
  const int max_blocks_high = max_block_high(xd, plane_bsize, plane)
                              << tx_size_high_log2[0];
  return ALIGN_POWER_OF_TWO(max_blocks_high, tx_size_high_log2[tx_size]);
}

static INLINE void av1_zero_above_context(AV1_COMMON *const cm,
  int mi_col_start, int mi_col_end, const int tile_row) {
  const int num_planes = av1_num_planes(cm);
  const int width = mi_col_end - mi_col_start;
  const int aligned_width =
    ALIGN_POWER_OF_TWO(width, cm->seq_params.mib_size_log2);

  const int offset_y = mi_col_start;
  const int width_y = aligned_width;
  const int offset_uv = offset_y >> cm->subsampling_x;
  const int width_uv = width_y >> cm->subsampling_x;

  av1_zero_array(cm->above_context[0][tile_row] + offset_y, width_y);
  if (num_planes > 1) {
    if (cm->above_context[1][tile_row] && cm->above_context[2][tile_row]) {
      av1_zero_array(cm->above_context[1][tile_row] + offset_uv, width_uv);
      av1_zero_array(cm->above_context[2][tile_row] + offset_uv, width_uv);
    } else {
      aom_internal_error(&cm->error, AOM_CODEC_CORRUPT_FRAME,
                         "Invalid value of planes");
    }
  }

  av1_zero_array(cm->above_seg_context[tile_row] + mi_col_start, aligned_width);

  memset(cm->above_txfm_context[tile_row] + mi_col_start,
    tx_size_wide[TX_SIZES_LARGEST],
    aligned_width * sizeof(TXFM_CONTEXT));
}

static INLINE void av1_zero_left_context(MACROBLOCKD *const xd) {
  av1_zero(xd->left_context);
  av1_zero(xd->left_seg_context);

  memset(xd->left_txfm_context_buffer, tx_size_high[TX_SIZES_LARGEST],
         sizeof(xd->left_txfm_context_buffer));
}

// Disable array-bounds checks as the TX_SIZE enum contains values larger than
// TX_SIZES_ALL (TX_INVALID) which make extending the array as a workaround
// infeasible. The assert is enough for static analysis and this or other tools
// asan, valgrind would catch oob access at runtime.
#if defined(__GNUC__) && __GNUC__ >= 4
#pragma GCC diagnostic ignored "-Warray-bounds"
#endif

#if defined(__GNUC__) && __GNUC__ >= 4
#pragma GCC diagnostic warning "-Warray-bounds"
#endif

static INLINE void set_txfm_ctx(TXFM_CONTEXT *txfm_ctx, uint8_t txs, int len) {
  int i;
  for (i = 0; i < len; ++i) txfm_ctx[i] = txs;
}

static INLINE void set_txfm_ctxs(TX_SIZE tx_size, int n8_w, int n8_h, int skip,
                                 const MACROBLOCKD *xd) {
  uint8_t bw = tx_size_wide[tx_size];
  uint8_t bh = tx_size_high[tx_size];

  if (skip) {
    bw = n8_w * MI_SIZE;
    bh = n8_h * MI_SIZE;
  }

  set_txfm_ctx(xd->above_txfm_context, bw, n8_w);
  set_txfm_ctx(xd->left_txfm_context, bh, n8_h);
}

static INLINE void txfm_partition_update(TXFM_CONTEXT *above_ctx,
                                         TXFM_CONTEXT *left_ctx,
                                         TX_SIZE tx_size, TX_SIZE txb_size) {
  BLOCK_SIZE bsize = txsize_to_bsize[txb_size];
  int bh = mi_size_high[bsize];
  int bw = mi_size_wide[bsize];
  uint8_t txw = tx_size_wide[tx_size];
  uint8_t txh = tx_size_high[tx_size];
  int i;
  for (i = 0; i < bh; ++i) left_ctx[i] = txh;
  for (i = 0; i < bw; ++i) above_ctx[i] = txw;
}

static INLINE TX_SIZE get_sqr_tx_size(int tx_dim) {
  switch (tx_dim) {
    case 128:
    case 64: return TX_64X64; break;
    case 32: return TX_32X32; break;
    case 16: return TX_16X16; break;
    case 8: return TX_8X8; break;
    default: return TX_4X4;
  }
}

static INLINE TX_SIZE get_tx_size(int width, int height) {
  if (width == height) {
    return get_sqr_tx_size(width);
  }
  if (width < height) {
    if (width + width == height) {
      switch (width) {
        case 4: return TX_4X8; break;
        case 8: return TX_8X16; break;
        case 16: return TX_16X32; break;
        case 32: return TX_32X64; break;
      }
    } else {
      switch (width) {
        case 4: return TX_4X16; break;
        case 8: return TX_8X32; break;
        case 16: return TX_16X64; break;
      }
    }
  } else {
    if (height + height == width) {
      switch (height) {
        case 4: return TX_8X4; break;
        case 8: return TX_16X8; break;
        case 16: return TX_32X16; break;
        case 32: return TX_64X32; break;
      }
    } else {
      switch (height) {
        case 4: return TX_16X4; break;
        case 8: return TX_32X8; break;
        case 16: return TX_64X16; break;
      }
    }
  }
  assert(0);
  return TX_4X4;
}

static INLINE int txfm_partition_context(TXFM_CONTEXT *above_ctx,
                                         TXFM_CONTEXT *left_ctx,
                                         BLOCK_SIZE bsize, TX_SIZE tx_size) {
  const uint8_t txw = tx_size_wide[tx_size];
  const uint8_t txh = tx_size_high[tx_size];
  const int above = *above_ctx < txw;
  const int left = *left_ctx < txh;
  int category = TXFM_PARTITION_CONTEXTS;

  // dummy return, not used by others.
  if (tx_size <= TX_4X4) return 0;

  TX_SIZE max_tx_size =
      get_sqr_tx_size(AOMMAX(block_size_wide[bsize], block_size_high[bsize]));

  if (max_tx_size >= TX_8X8) {
    category =
        (txsize_sqr_up_map[tx_size] != max_tx_size && max_tx_size > TX_8X8) +
        (TX_SIZES - 1 - max_tx_size) * 2;
  }
  assert(category != TXFM_PARTITION_CONTEXTS);
  return category * 3 + above + left;
}

// Compute the next partition in the direction of the sb_type stored in the mi
// array, starting with bsize.
static INLINE PARTITION_TYPE get_partition(const AV1_COMMON *const cm,
                                           int mi_row, int mi_col,
                                           BLOCK_SIZE bsize) {
  if (mi_row >= cm->mi_rows || mi_col >= cm->mi_cols) return PARTITION_INVALID;

  const int offset = mi_row * cm->mi_stride + mi_col;
  MB_MODE_INFO **mi = cm->mi_grid_visible + offset;
  const BLOCK_SIZE subsize = mi[0]->sb_type;

  if (subsize == bsize) return PARTITION_NONE;

  const int bhigh = mi_size_high[bsize];
  const int bwide = mi_size_wide[bsize];
  const int sshigh = mi_size_high[subsize];
  const int sswide = mi_size_wide[subsize];

  if (bsize > BLOCK_8X8 && mi_row + bwide / 2 < cm->mi_rows &&
      mi_col + bhigh / 2 < cm->mi_cols) {
    // In this case, the block might be using an extended partition
    // type.
    const MB_MODE_INFO *const mbmi_right = mi[bwide / 2];
    const MB_MODE_INFO *const mbmi_below = mi[bhigh / 2 * cm->mi_stride];

    if (sswide == bwide) {
      // Smaller height but same width. Is PARTITION_HORZ_4, PARTITION_HORZ or
      // PARTITION_HORZ_B. To distinguish the latter two, check if the lower
      // half was split.
      if (sshigh * 4 == bhigh) return PARTITION_HORZ_4;
      assert(sshigh * 2 == bhigh);

      if (mbmi_below->sb_type == subsize)
        return PARTITION_HORZ;
      else
        return PARTITION_HORZ_B;
    } else if (sshigh == bhigh) {
      // Smaller width but same height. Is PARTITION_VERT_4, PARTITION_VERT or
      // PARTITION_VERT_B. To distinguish the latter two, check if the right
      // half was split.
      if (sswide * 4 == bwide) return PARTITION_VERT_4;
      assert(sswide * 2 == bhigh);

      if (mbmi_right->sb_type == subsize)
        return PARTITION_VERT;
      else
        return PARTITION_VERT_B;
    } else {
      // Smaller width and smaller height. Might be PARTITION_SPLIT or could be
      // PARTITION_HORZ_A or PARTITION_VERT_A. If subsize isn't halved in both
      // dimensions, we immediately know this is a split (which will recurse to
      // get to subsize). Otherwise look down and to the right. With
      // PARTITION_VERT_A, the right block will have height bhigh; with
      // PARTITION_HORZ_A, the lower block with have width bwide. Otherwise
      // it's PARTITION_SPLIT.
      if (sswide * 2 != bwide || sshigh * 2 != bhigh) return PARTITION_SPLIT;

      if (mi_size_wide[mbmi_below->sb_type] == bwide) return PARTITION_HORZ_A;
      if (mi_size_high[mbmi_right->sb_type] == bhigh) return PARTITION_VERT_A;

      return PARTITION_SPLIT;
    }
  }
  const int vert_split = sswide < bwide;
  const int horz_split = sshigh < bhigh;
  const int split_idx = (vert_split << 1) | horz_split;
  assert(split_idx != 0);

  static const PARTITION_TYPE base_partitions[4] = {
    PARTITION_INVALID, PARTITION_HORZ, PARTITION_VERT, PARTITION_SPLIT
  };

  return base_partitions[split_idx];
}

static INLINE void set_use_reference_buffer(AV1_COMMON *const cm, int use) {
  cm->seq_params.frame_id_numbers_present_flag = use;
}

static INLINE void set_sb_size(SequenceHeader *const seq_params,
                               BLOCK_SIZE sb_size) {
  seq_params->sb_size = sb_size;
  seq_params->mib_size = mi_size_wide[seq_params->sb_size];
  seq_params->mib_size_log2 = mi_size_wide_log2[seq_params->sb_size];
}

// Returns true if the frame is fully lossless at the coded resolution.
// Note: If super-resolution is used, such a frame will still NOT be lossless at
// the upscaled resolution.
static INLINE int is_coded_lossless(const AV1_COMMON *cm,
                                    const MACROBLOCKD *xd) {
  int coded_lossless = 1;
  if (cm->seg.enabled) {
    for (int i = 0; i < MAX_SEGMENTS; ++i) {
      if (!xd->lossless[i]) {
        coded_lossless = 0;
        break;
      }
    }
  } else {
    coded_lossless = xd->lossless[0];
  }
  return coded_lossless;
}

static INLINE int is_valid_seq_level_idx(uint8_t seq_level_idx) {
  return seq_level_idx < 24 || seq_level_idx == 31;
}

static INLINE uint8_t major_minor_to_seq_level_idx(BitstreamLevel bl) {
  assert(bl.major >= LEVEL_MAJOR_MIN && bl.major <= LEVEL_MAJOR_MAX);
  // Since bl.minor is unsigned a comparison will return a warning:
  // comparison is always true due to limited range of data type
  assert(LEVEL_MINOR_MIN == 0);
  assert(bl.minor <= LEVEL_MINOR_MAX);
  return ((bl.major - LEVEL_MAJOR_MIN) << LEVEL_MINOR_BITS) + bl.minor;
}

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // AV1_COMMON_ONYXC_INT_H_
