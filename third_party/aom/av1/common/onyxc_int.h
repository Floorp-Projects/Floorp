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

#include "./aom_config.h"
#include "./av1_rtcd.h"
#include "aom/internal/aom_codec_internal.h"
#include "aom_util/aom_thread.h"
#if CONFIG_ANS
#include "aom_dsp/ans.h"
#endif
#include "av1/common/alloccommon.h"
#include "av1/common/av1_loopfilter.h"
#include "av1/common/entropy.h"
#include "av1/common/entropymode.h"
#include "av1/common/entropymv.h"
#include "av1/common/frame_buffers.h"
#include "av1/common/mv.h"
#include "av1/common/quant_common.h"
#if CONFIG_LOOP_RESTORATION
#include "av1/common/restoration.h"
#endif  // CONFIG_LOOP_RESTORATION
#include "av1/common/tile_common.h"
#include "av1/common/odintrin.h"
#if CONFIG_PVQ
#include "av1/common/pvq.h"
#endif
#if CONFIG_CFL
#include "av1/common/cfl.h"
#endif
#if CONFIG_HASH_ME
// TODO(youzhou@microsoft.com): Encoder only. Move it out of common
#include "av1/encoder/hash_motion.h"
#endif
#ifdef __cplusplus
extern "C" {
#endif

#define CDEF_MAX_STRENGTHS 16

#define REF_FRAMES_LOG2 3
#define REF_FRAMES (1 << REF_FRAMES_LOG2)

// 4 scratch frames for the new frames to support a maximum of 4 cores decoding
// in parallel, 3 for scaled references on the encoder.
// TODO(hkuang): Add ondemand frame buffers instead of hardcoding the number
// of framebuffers.
// TODO(jkoleszar): These 3 extra references could probably come from the
// normal reference pool.
#define FRAME_BUFFERS (REF_FRAMES + 7)

#if CONFIG_REFERENCE_BUFFER
/* Constant values while waiting for the sequence header */
#define FRAME_ID_NUMBERS_PRESENT_FLAG 1
#define FRAME_ID_LENGTH_MINUS7 8         // Allows frame id up to 2^15-1
#define DELTA_FRAME_ID_LENGTH_MINUS2 12  // Allows frame id deltas up to 2^14-1
#endif                                   // CONFIG_REFERENCE_BUFFER

#if CONFIG_NO_FRAME_CONTEXT_SIGNALING
#define FRAME_CONTEXTS (FRAME_BUFFERS + 1)
// Extra frame context which is always kept at default values
#define FRAME_CONTEXT_DEFAULTS (FRAME_CONTEXTS - 1)
#else

#if CONFIG_EXT_REFS
#define FRAME_CONTEXTS_LOG2 3
#else
#define FRAME_CONTEXTS_LOG2 2
#endif

#define FRAME_CONTEXTS (1 << FRAME_CONTEXTS_LOG2)
#endif  // CONFIG_NO_FRAME_CONTEXT_SIGNALING

#define NUM_PING_PONG_BUFFERS 2

typedef enum {
  SINGLE_REFERENCE = 0,
  COMPOUND_REFERENCE = 1,
  REFERENCE_MODE_SELECT = 2,
  REFERENCE_MODES = 3,
} REFERENCE_MODE;

#if !CONFIG_NO_FRAME_CONTEXT_SIGNALING
typedef enum {
  RESET_FRAME_CONTEXT_NONE = 0,
  RESET_FRAME_CONTEXT_CURRENT = 1,
  RESET_FRAME_CONTEXT_ALL = 2,
} RESET_FRAME_CONTEXT_MODE;
#endif

typedef enum {
  /**
   * Update frame context to values resulting from forward probability
   * updates signaled in the frame header
   */
  REFRESH_FRAME_CONTEXT_FORWARD,
  /**
   * Update frame context to values resulting from backward probability
   * updates based on entropy/counts in the decoded frame
   */
  REFRESH_FRAME_CONTEXT_BACKWARD,
} REFRESH_FRAME_CONTEXT_MODE;

#if CONFIG_MFMV
#define MFMV_STACK_SIZE INTER_REFS_PER_FRAME

typedef struct {
  int_mv mfmv[INTER_REFS_PER_FRAME][MFMV_STACK_SIZE];
} TPL_MV_REF;
#endif

typedef struct {
  int_mv mv[2];
  int_mv pred_mv[2];
  MV_REFERENCE_FRAME ref_frame[2];
} MV_REF;

typedef struct {
  int ref_count;

#if CONFIG_FRAME_MARKER
  int cur_frame_offset;
  int lst_frame_offset;
  int alt_frame_offset;
  int gld_frame_offset;
#if CONFIG_EXT_REFS
  int lst2_frame_offset;
  int lst3_frame_offset;
  int bwd_frame_offset;
  int alt2_frame_offset;
#endif
#endif  // CONFIG_FRAME_MARKER

#if CONFIG_MFMV
  TPL_MV_REF *tpl_mvs;
#endif
  MV_REF *mvs;
  int mi_rows;
  int mi_cols;
  // Width and height give the size of the buffer (before any upscaling, unlike
  // the sizes that can be derived from the buf structure)
  int width;
  int height;
#if CONFIG_GLOBAL_MOTION
  WarpedMotionParams global_motion[TOTAL_REFS_PER_FRAME];
#endif  // CONFIG_GLOBAL_MOTION
  aom_codec_frame_buffer_t raw_frame_buffer;
  YV12_BUFFER_CONFIG buf;
#if CONFIG_HASH_ME
  hash_table hash_table;
#endif
#if CONFIG_TEMPMV_SIGNALING
  uint8_t intra_only;
#endif
  // The Following variables will only be used in frame parallel decode.

  // frame_worker_owner indicates which FrameWorker owns this buffer. NULL means
  // that no FrameWorker owns, or is decoding, this buffer.
  AVxWorker *frame_worker_owner;

  // row and col indicate which position frame has been decoded to in real
  // pixel unit. They are reset to -1 when decoding begins and set to INT_MAX
  // when the frame is fully decoded.
  int row;
  int col;
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

#if CONFIG_LV_MAP
typedef struct {
  int base_ctx_table[2 /*row*/][2 /*col*/][2 /*sig_map*/]
                    [BASE_CONTEXT_POSITION_NUM + 1];
} LV_MAP_CTX_TABLE;
typedef int BASE_CTX_TABLE[2 /*col*/][2 /*sig_map*/]
                          [BASE_CONTEXT_POSITION_NUM + 1];
#endif

#if CONFIG_REFERENCE_BUFFER
/* Initial version of sequence header structure */
typedef struct SequenceHeader {
  int frame_id_numbers_present_flag;
  int frame_id_length_minus7;
  int delta_frame_id_length_minus2;
} SequenceHeader;
#endif  // CONFIG_REFERENCE_BUFFER

typedef struct AV1Common {
  struct aom_internal_error_info error;
  aom_color_space_t color_space;
  aom_transfer_function_t transfer_function;
  aom_chroma_sample_position_t chroma_sample_position;
  int color_range;
  int width;
  int height;
  int render_width;
  int render_height;
  int last_width;
  int last_height;

  // TODO(jkoleszar): this implies chroma ss right now, but could vary per
  // plane. Revisit as part of the future change to YV12_BUFFER_CONFIG to
  // support additional planes.
  int subsampling_x;
  int subsampling_y;

#if CONFIG_HIGHBITDEPTH
  // Marks if we need to use 16bit frame buffers (1: yes, 0: no).
  int use_highbitdepth;
#endif
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

  int new_fb_idx;

  FRAME_TYPE last_frame_type; /* last frame's frame type for motion search.*/
  FRAME_TYPE frame_type;

  int show_frame;
  int last_show_frame;
  int show_existing_frame;
#if CONFIG_EXT_REFS
  // Flag for a frame used as a reference - not written to the bitstream
  int is_reference_frame;
#endif  // CONFIG_EXT_REFS

  // Flag signaling that the frame is encoded using only INTRA modes.
  uint8_t intra_only;
  uint8_t last_intra_only;

  int allow_high_precision_mv;
#if CONFIG_AMVR
  int seq_mv_precision_level;        // 0 the default in AOM, 1 only integer, 2
                                     // adaptive
  int cur_frame_mv_precision_level;  // 0 the default in AOM, 1 only integer
#endif

  int allow_screen_content_tools;
#if CONFIG_INTERINTRA
  int allow_interintra_compound;
#endif  // CONFIG_INTERINTRA
#if CONFIG_WEDGE || CONFIG_COMPOUND_SEGMENT
  int allow_masked_compound;
#endif  // CONFIG_WEDGE || CONFIG_COMPOUND_SEGMENT

#if !CONFIG_NO_FRAME_CONTEXT_SIGNALING
  // Flag signaling which frame contexts should be reset to default values.
  RESET_FRAME_CONTEXT_MODE reset_frame_context;
#endif

  // MBs, mb_rows/cols is in 16-pixel units; mi_rows/cols is in
  // MODE_INFO (8-pixel) units.
  int MBs;
  int mb_rows, mi_rows;
  int mb_cols, mi_cols;
  int mi_stride;

  /* profile settings */
  TX_MODE tx_mode;

  int base_qindex;
  int y_dc_delta_q;
  int uv_dc_delta_q;
  int uv_ac_delta_q;
  int16_t y_dequant[MAX_SEGMENTS][2];
  int16_t uv_dequant[MAX_SEGMENTS][2];

#if CONFIG_AOM_QM
  // Global quant matrix tables
  const qm_val_t *giqmatrix[NUM_QM_LEVELS][2][2][TX_SIZES_ALL];
  const qm_val_t *gqmatrix[NUM_QM_LEVELS][2][2][TX_SIZES_ALL];

  // Local quant matrix tables for each frame
  const qm_val_t *y_iqmatrix[MAX_SEGMENTS][2][TX_SIZES_ALL];
  const qm_val_t *uv_iqmatrix[MAX_SEGMENTS][2][TX_SIZES_ALL];
  // Encoder
  const qm_val_t *y_qmatrix[MAX_SEGMENTS][2][TX_SIZES_ALL];
  const qm_val_t *uv_qmatrix[MAX_SEGMENTS][2][TX_SIZES_ALL];

  int using_qmatrix;
  int min_qmlevel;
  int max_qmlevel;
#endif
#if CONFIG_NEW_QUANT
  dequant_val_type_nuq y_dequant_nuq[MAX_SEGMENTS][QUANT_PROFILES][COEF_BANDS];
  dequant_val_type_nuq uv_dequant_nuq[MAX_SEGMENTS][QUANT_PROFILES][COEF_BANDS];
#endif

  /* We allocate a MODE_INFO struct for each macroblock, together with
     an extra row on top and column on the left to simplify prediction. */
  int mi_alloc_size;
  MODE_INFO *mip; /* Base of allocated array */
  MODE_INFO *mi;  /* Corresponds to upper left visible macroblock */

  // TODO(agrange): Move prev_mi into encoder structure.
  // prev_mip and prev_mi will only be allocated in encoder.
  MODE_INFO *prev_mip; /* MODE_INFO array 'mip' from last decoded frame */
  MODE_INFO *prev_mi;  /* 'mi' from last frame (points into prev_mip) */

  // Separate mi functions between encoder and decoder.
  int (*alloc_mi)(struct AV1Common *cm, int mi_size);
  void (*free_mi)(struct AV1Common *cm);
  void (*setup_mi)(struct AV1Common *cm);

  // Grid of pointers to 8x8 MODE_INFO structs.  Any 8x8 not in the visible
  // area will be NULL.
  MODE_INFO **mi_grid_base;
  MODE_INFO **mi_grid_visible;
  MODE_INFO **prev_mi_grid_base;
  MODE_INFO **prev_mi_grid_visible;

  // Whether to use previous frame's motion vectors for prediction.
  int use_prev_frame_mvs;

  // Persistent mb segment id map used in prediction.
  int seg_map_idx;
  int prev_seg_map_idx;

  uint8_t *seg_map_array[NUM_PING_PONG_BUFFERS];
  uint8_t *last_frame_seg_map;
  uint8_t *current_frame_seg_map;
  int seg_map_alloc_size;

  InterpFilter interp_filter;

  loop_filter_info_n lf_info;
#if CONFIG_FRAME_SUPERRES
  // The denominator of the superres scale; the numerator is fixed.
  uint8_t superres_scale_denominator;
  int superres_upscaled_width;
  int superres_upscaled_height;
#endif  // CONFIG_FRAME_SUPERRES
#if CONFIG_LOOP_RESTORATION
  RestorationInfo rst_info[MAX_MB_PLANE];
  RestorationInternal rst_internal;
#endif  // CONFIG_LOOP_RESTORATION

  // Flag signaling how frame contexts should be updated at the end of
  // a frame decode
  REFRESH_FRAME_CONTEXT_MODE refresh_frame_context;

  int ref_frame_sign_bias[TOTAL_REFS_PER_FRAME]; /* Two state 0, 1 */

  struct loopfilter lf;
  struct segmentation seg;
  int all_lossless;
  int frame_parallel_decode;  // frame-based threading.

#if CONFIG_EXT_TX
  int reduced_tx_set_used;
#endif  // CONFIG_EXT_TX

// Context probabilities for reference frame prediction
#if CONFIG_EXT_REFS
  MV_REFERENCE_FRAME comp_fwd_ref[FWD_REFS];
  MV_REFERENCE_FRAME comp_bwd_ref[BWD_REFS];
#else
  MV_REFERENCE_FRAME comp_fixed_ref;
  MV_REFERENCE_FRAME comp_var_ref[COMP_REFS];
#endif  // CONFIG_EXT_REFS
  REFERENCE_MODE reference_mode;

  FRAME_CONTEXT *fc;              /* this frame entropy */
  FRAME_CONTEXT *frame_contexts;  // FRAME_CONTEXTS
  FRAME_CONTEXT *pre_fc;          // Context referenced in this frame
#if !CONFIG_NO_FRAME_CONTEXT_SIGNALING
  unsigned int frame_context_idx; /* Context to use/update */
#endif
  FRAME_COUNTS counts;

#if CONFIG_FRAME_MARKER
  unsigned int frame_offset;
#endif

  unsigned int current_video_frame;
  BITSTREAM_PROFILE profile;

  // AOM_BITS_8 in profile 0 or 1, AOM_BITS_10 or AOM_BITS_12 in profile 2 or 3.
  aom_bit_depth_t bit_depth;
  aom_bit_depth_t dequant_bit_depth;  // bit_depth of current dequantizer

  int error_resilient_mode;

  int tile_cols, tile_rows;
  int last_tile_cols, last_tile_rows;

#if CONFIG_MAX_TILE
  int min_log2_tile_cols;
  int max_log2_tile_cols;
  int max_log2_tile_rows;
  int min_log2_tile_rows;
  int min_log2_tiles;
  int max_tile_width_sb;
  int max_tile_height_sb;
  int uniform_tile_spacing_flag;
  int log2_tile_cols;                        // only valid for uniform tiles
  int log2_tile_rows;                        // only valid for uniform tiles
  int tile_col_start_sb[MAX_TILE_COLS + 1];  // valid for 0 <= i <= tile_cols
  int tile_row_start_sb[MAX_TILE_ROWS + 1];  // valid for 0 <= i <= tile_rows
#if CONFIG_DEPENDENT_HORZTILES
  int tile_row_independent[MAX_TILE_ROWS];  // valid for 0 <= i <  tile_rows
#endif
#else
  int log2_tile_cols, log2_tile_rows;  // Used in non-large_scale_tile_coding.
  int tile_width, tile_height;         // In MI units
#endif  // CONFIG_MAX_TILE

#if CONFIG_EXT_TILE
  unsigned int large_scale_tile;
  unsigned int single_tile_decoding;
#endif  // CONFIG_EXT_TILE

#if CONFIG_DEPENDENT_HORZTILES
  int dependent_horz_tiles;
  int tile_group_start_row[MAX_TILE_ROWS][MAX_TILE_COLS];
  int tile_group_start_col[MAX_TILE_ROWS][MAX_TILE_COLS];
#endif
#if CONFIG_LOOPFILTERING_ACROSS_TILES
  int loop_filter_across_tiles_enabled;
#endif  // CONFIG_LOOPFILTERING_ACROSS_TILES

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

  PARTITION_CONTEXT *above_seg_context;
  ENTROPY_CONTEXT *above_context[MAX_MB_PLANE];
#if CONFIG_VAR_TX
  TXFM_CONTEXT *above_txfm_context;
  TXFM_CONTEXT *top_txfm_context[MAX_MB_PLANE];
  TXFM_CONTEXT left_txfm_context[MAX_MB_PLANE][2 * MAX_MIB_SIZE];
#endif
  int above_context_alloc_cols;

  // scratch memory for intraonly/keyframe forward updates from default tables
  // - this is intentionally not placed in FRAME_CONTEXT since it's reset upon
  // each keyframe and not used afterwards
  aom_prob kf_y_prob[INTRA_MODES][INTRA_MODES][INTRA_MODES - 1];
#if CONFIG_GLOBAL_MOTION
  WarpedMotionParams global_motion[TOTAL_REFS_PER_FRAME];
#endif

  BLOCK_SIZE sb_size;  // Size of the superblock used for this frame
  int mib_size;        // Size of the superblock in units of MI blocks
  int mib_size_log2;   // Log 2 of above.
#if CONFIG_CDEF
  int cdef_pri_damping;
  int cdef_sec_damping;
  int nb_cdef_strengths;
  int cdef_strengths[CDEF_MAX_STRENGTHS];
  int cdef_uv_strengths[CDEF_MAX_STRENGTHS];
  int cdef_bits;
#endif

  int delta_q_present_flag;
  // Resolution of delta quant
  int delta_q_res;
#if CONFIG_EXT_DELTA_Q
  int delta_lf_present_flag;
  // Resolution of delta lf level
  int delta_lf_res;
#if CONFIG_LOOPFILTER_LEVEL
  // This is a flag for number of deltas of loop filter level
  // 0: use 1 delta, for y_vertical, y_horizontal, u, and v
  // 1: use separate deltas for each filter level
  int delta_lf_multi;
#endif  // CONFIG_LOOPFILTER_LEVEL
#endif
  int num_tg;
#if CONFIG_REFERENCE_BUFFER
  SequenceHeader seq_params;
  int current_frame_id;
  int ref_frame_id[REF_FRAMES];
  int valid_for_referencing[REF_FRAMES];
  int refresh_mask;
  int invalid_delta_frame_id_minus1;
#endif  // CONFIG_REFERENCE_BUFFER
#if CONFIG_ANS && ANS_MAX_SYMBOLS
  int ans_window_size_log2;
#endif
#if CONFIG_NCOBMC_ADAPT_WEIGHT
  NCOBMC_KERNELS ncobmc_kernels[ADAPT_OVERLAP_BLOCKS][ALL_NCOBMC_MODES];
  uint8_t *ncobmcaw_buf[4];
#endif
#if CONFIG_LV_MAP
  LV_MAP_CTX_TABLE coeff_ctx_table;
#endif
#if CONFIG_LPF_SB
  int final_lpf_encode;
#endif
#if CONFIG_ADAPT_SCAN
  int use_adapt_scan;
#endif
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

#if CONFIG_TEMPMV_SIGNALING
// Returns 1 if this frame might use mvs from some previous frame. This
// function doesn't consider whether prev_frame is actually suitable (see
// frame_can_use_prev_frame_mvs for that)
static INLINE int frame_might_use_prev_frame_mvs(const AV1_COMMON *cm) {
  return !cm->error_resilient_mode && !cm->intra_only;
}

// Returns 1 if this frame really can use MVs from some previous frame.
static INLINE int frame_can_use_prev_frame_mvs(const AV1_COMMON *cm) {
  return (frame_might_use_prev_frame_mvs(cm) && cm->last_show_frame &&
          cm->prev_frame && !cm->prev_frame->intra_only &&
          cm->width == cm->prev_frame->width &&
          cm->height == cm->prev_frame->height);
}
#endif

static INLINE void ensure_mv_buffer(RefCntBuffer *buf, AV1_COMMON *cm) {
  if (buf->mvs == NULL || buf->mi_rows < cm->mi_rows ||
      buf->mi_cols < cm->mi_cols) {
    aom_free(buf->mvs);
    buf->mi_rows = cm->mi_rows;
    buf->mi_cols = cm->mi_cols;
#if CONFIG_TMV
    CHECK_MEM_ERROR(cm, buf->mvs,
                    (MV_REF *)aom_calloc(
                        ((cm->mi_rows + 1) >> 1) * ((cm->mi_cols + 1) >> 1),
                        sizeof(*buf->mvs)));
#else
    CHECK_MEM_ERROR(
        cm, buf->mvs,
        (MV_REF *)aom_calloc(cm->mi_rows * cm->mi_cols, sizeof(*buf->mvs)));
#endif  // CONFIG_TMV

#if CONFIG_MFMV
    aom_free(buf->tpl_mvs);
    CHECK_MEM_ERROR(
        cm, buf->tpl_mvs,
        (TPL_MV_REF *)aom_calloc((cm->mi_rows + MAX_MIB_SIZE) * cm->mi_stride,
                                 sizeof(*buf->tpl_mvs)));
#endif
  }
}

#if CONFIG_VAR_REFS
#define LAST_IS_VALID(cm) ((cm)->frame_refs[LAST_FRAME - 1].is_valid)
#define LAST2_IS_VALID(cm) ((cm)->frame_refs[LAST2_FRAME - 1].is_valid)
#define LAST3_IS_VALID(cm) ((cm)->frame_refs[LAST3_FRAME - 1].is_valid)
#define GOLDEN_IS_VALID(cm) ((cm)->frame_refs[GOLDEN_FRAME - 1].is_valid)
#define BWDREF_IS_VALID(cm) ((cm)->frame_refs[BWDREF_FRAME - 1].is_valid)
#define ALTREF2_IS_VALID(cm) ((cm)->frame_refs[ALTREF2_FRAME - 1].is_valid)
#define ALTREF_IS_VALID(cm) ((cm)->frame_refs[ALTREF_FRAME - 1].is_valid)

#define L_OR_L2(cm) (LAST_IS_VALID(cm) || LAST2_IS_VALID(cm))
#define L_AND_L2(cm) (LAST_IS_VALID(cm) && LAST2_IS_VALID(cm))
#define L_AND_L3(cm) (LAST_IS_VALID(cm) && LAST3_IS_VALID(cm))
#define L_AND_G(cm) (LAST_IS_VALID(cm) && GOLDEN_IS_VALID(cm))

#define L3_OR_G(cm) (LAST3_IS_VALID(cm) || GOLDEN_IS_VALID(cm))
#define L3_AND_G(cm) (LAST3_IS_VALID(cm) && GOLDEN_IS_VALID(cm))

#define BWD_OR_ALT2(cm) (BWDREF_IS_VALID(cm) || ALTREF2_IS_VALID(cm))
#define BWD_AND_ALT2(cm) (BWDREF_IS_VALID(cm) && ALTREF2_IS_VALID(cm))
#define BWD_OR_ALT(cm) (BWDREF_IS_VALID(cm) || ALTREF_IS_VALID(cm))
#define BWD_AND_ALT(cm) (BWDREF_IS_VALID(cm) && ALTREF_IS_VALID(cm))
#endif  // CONFIG_VAR_REFS

static INLINE int mi_cols_aligned_to_sb(const AV1_COMMON *cm) {
  return ALIGN_POWER_OF_TWO(cm->mi_cols, cm->mib_size_log2);
}

static INLINE int mi_rows_aligned_to_sb(const AV1_COMMON *cm) {
  return ALIGN_POWER_OF_TWO(cm->mi_rows, cm->mib_size_log2);
}

static INLINE int frame_is_intra_only(const AV1_COMMON *const cm) {
  return cm->frame_type == KEY_FRAME || cm->intra_only;
}

#if CONFIG_CFL
#if CONFIG_CHROMA_SUB8X8 && CONFIG_DEBUG
static INLINE void cfl_clear_sub8x8_val(CFL_CTX *cfl) {
  memset(cfl->sub8x8_val, 0, sizeof(cfl->sub8x8_val));
}
#endif  // CONFIG_CHROMA_SUB8X8 && CONFIG_DEBUG
void cfl_init(CFL_CTX *cfl, AV1_COMMON *cm);
#endif  // CONFIG_CFL

static INLINE void av1_init_macroblockd(AV1_COMMON *cm, MACROBLOCKD *xd,
#if CONFIG_PVQ
                                        tran_low_t *pvq_ref_coeff,
#endif
#if CONFIG_CFL
                                        CFL_CTX *cfl,
#endif
                                        tran_low_t *dqcoeff) {
  for (int i = 0; i < MAX_MB_PLANE; ++i) {
    xd->plane[i].dqcoeff = dqcoeff;
#if CONFIG_PVQ
    xd->plane[i].pvq_ref_coeff = pvq_ref_coeff;
#endif
    xd->above_context[i] = cm->above_context[i];
    if (xd->plane[i].plane_type == PLANE_TYPE_Y) {
      memcpy(xd->plane[i].seg_dequant, cm->y_dequant, sizeof(cm->y_dequant));
#if CONFIG_AOM_QM
      memcpy(xd->plane[i].seg_iqmatrix, cm->y_iqmatrix, sizeof(cm->y_iqmatrix));
#endif

#if CONFIG_NEW_QUANT
      memcpy(xd->plane[i].seg_dequant_nuq, cm->y_dequant_nuq,
             sizeof(cm->y_dequant_nuq));
#endif
    } else {
      memcpy(xd->plane[i].seg_dequant, cm->uv_dequant, sizeof(cm->uv_dequant));
#if CONFIG_AOM_QM
      memcpy(xd->plane[i].seg_iqmatrix, cm->uv_iqmatrix,
             sizeof(cm->uv_iqmatrix));
#endif
#if CONFIG_NEW_QUANT
      memcpy(xd->plane[i].seg_dequant_nuq, cm->uv_dequant_nuq,
             sizeof(cm->uv_dequant_nuq));
#endif
    }
  }
  xd->fc = cm->fc;
  xd->above_seg_context = cm->above_seg_context;
#if CONFIG_VAR_TX
  xd->above_txfm_context = cm->above_txfm_context;
#endif
#if CONFIG_CFL
  cfl_init(cfl, cm);
  xd->cfl = cfl;
#endif
  xd->mi_stride = cm->mi_stride;
  xd->error_info = &cm->error;
}

static INLINE void set_skip_context(MACROBLOCKD *xd, int mi_row, int mi_col) {
  int i;
  int row_offset = mi_row;
  int col_offset = mi_col;
  for (i = 0; i < MAX_MB_PLANE; ++i) {
    struct macroblockd_plane *const pd = &xd->plane[i];
#if CONFIG_CHROMA_SUB8X8
    // Offset the buffer pointer
    const BLOCK_SIZE bsize = xd->mi[0]->mbmi.sb_type;
    if (pd->subsampling_y && (mi_row & 0x01) && (mi_size_high[bsize] == 1))
      row_offset = mi_row - 1;
    if (pd->subsampling_x && (mi_col & 0x01) && (mi_size_wide[bsize] == 1))
      col_offset = mi_col - 1;
#endif
    int above_idx = col_offset << (MI_SIZE_LOG2 - tx_size_wide_log2[0]);
    int left_idx = (row_offset & MAX_MIB_MASK)
                   << (MI_SIZE_LOG2 - tx_size_high_log2[0]);
    pd->above_context = &xd->above_context[i][above_idx >> pd->subsampling_x];
    pd->left_context = &xd->left_context[i][left_idx >> pd->subsampling_y];
  }
}

static INLINE int calc_mi_size(int len) {
  // len is in mi units.
  return len + MAX_MIB_SIZE;
}

static INLINE void set_plane_n4(MACROBLOCKD *const xd, int bw, int bh) {
  int i;
  for (i = 0; i < MAX_MB_PLANE; i++) {
    xd->plane[i].n4_w = (bw << 1) >> xd->plane[i].subsampling_x;
    xd->plane[i].n4_h = (bh << 1) >> xd->plane[i].subsampling_y;

    xd->plane[i].width = (bw * MI_SIZE) >> xd->plane[i].subsampling_x;
    xd->plane[i].height = (bh * MI_SIZE) >> xd->plane[i].subsampling_y;

#if !CONFIG_CHROMA_2X2
    xd->plane[i].width = AOMMAX(xd->plane[i].width, 4);
    xd->plane[i].height = AOMMAX(xd->plane[i].height, 4);
#endif
  }
}

static INLINE void set_mi_row_col(MACROBLOCKD *xd, const TileInfo *const tile,
                                  int mi_row, int bh, int mi_col, int bw,
#if CONFIG_DEPENDENT_HORZTILES
                                  int dependent_horz_tile_flag,
#endif  // CONFIG_DEPENDENT_HORZTILES
                                  int mi_rows, int mi_cols) {
  xd->mb_to_top_edge = -((mi_row * MI_SIZE) * 8);
  xd->mb_to_bottom_edge = ((mi_rows - bh - mi_row) * MI_SIZE) * 8;
  xd->mb_to_left_edge = -((mi_col * MI_SIZE) * 8);
  xd->mb_to_right_edge = ((mi_cols - bw - mi_col) * MI_SIZE) * 8;

#if CONFIG_DEPENDENT_HORZTILES
  if (dependent_horz_tile_flag) {
    xd->up_available = (mi_row > tile->mi_row_start) || !tile->tg_horz_boundary;
  } else {
#endif  // CONFIG_DEPENDENT_HORZTILES
    // Are edges available for intra prediction?
    xd->up_available = (mi_row > tile->mi_row_start);
#if CONFIG_DEPENDENT_HORZTILES
  }
#endif  // CONFIG_DEPENDENT_HORZTILES

  xd->left_available = (mi_col > tile->mi_col_start);
#if CONFIG_CHROMA_SUB8X8
  xd->chroma_up_available = xd->up_available;
  xd->chroma_left_available = xd->left_available;
  if (xd->plane[1].subsampling_x && bw < mi_size_wide[BLOCK_8X8])
    xd->chroma_left_available = (mi_col - 1) > tile->mi_col_start;
  if (xd->plane[1].subsampling_y && bh < mi_size_high[BLOCK_8X8])
    xd->chroma_up_available = (mi_row - 1) > tile->mi_row_start;
#endif
  if (xd->up_available) {
    xd->above_mi = xd->mi[-xd->mi_stride];
    // above_mi may be NULL in encoder's first pass.
    xd->above_mbmi = xd->above_mi ? &xd->above_mi->mbmi : NULL;
  } else {
    xd->above_mi = NULL;
    xd->above_mbmi = NULL;
  }

  if (xd->left_available) {
    xd->left_mi = xd->mi[-1];
    // left_mi may be NULL in encoder's first pass.
    xd->left_mbmi = xd->left_mi ? &xd->left_mi->mbmi : NULL;
  } else {
    xd->left_mi = NULL;
    xd->left_mbmi = NULL;
  }

  xd->n8_h = bh;
  xd->n8_w = bw;
  xd->is_sec_rect = 0;
  if (xd->n8_w < xd->n8_h)
    if (mi_col & (xd->n8_h - 1)) xd->is_sec_rect = 1;

  if (xd->n8_w > xd->n8_h)
    if (mi_row & (xd->n8_w - 1)) xd->is_sec_rect = 1;
}

static INLINE const aom_prob *get_y_mode_probs(const AV1_COMMON *cm,
                                               const MODE_INFO *mi,
                                               const MODE_INFO *above_mi,
                                               const MODE_INFO *left_mi,
                                               int block) {
  const PREDICTION_MODE above = av1_above_block_mode(mi, above_mi, block);
  const PREDICTION_MODE left = av1_left_block_mode(mi, left_mi, block);
  return cm->kf_y_prob[above][left];
}

static INLINE aom_cdf_prob *get_y_mode_cdf(FRAME_CONTEXT *tile_ctx,
                                           const MODE_INFO *mi,
                                           const MODE_INFO *above_mi,
                                           const MODE_INFO *left_mi,
                                           int block) {
  const PREDICTION_MODE above = av1_above_block_mode(mi, above_mi, block);
  const PREDICTION_MODE left = av1_left_block_mode(mi, left_mi, block);

#if CONFIG_KF_CTX
  int above_ctx = intra_mode_context[above];
  int left_ctx = intra_mode_context[left];
  return tile_ctx->kf_y_cdf[above_ctx][left_ctx];
#else
  return tile_ctx->kf_y_cdf[above][left];
#endif
}

static INLINE void update_partition_context(MACROBLOCKD *xd, int mi_row,
                                            int mi_col, BLOCK_SIZE subsize,
                                            BLOCK_SIZE bsize) {
  PARTITION_CONTEXT *const above_ctx = xd->above_seg_context + mi_col;
  PARTITION_CONTEXT *const left_ctx =
      xd->left_seg_context + (mi_row & MAX_MIB_MASK);

#if CONFIG_EXT_PARTITION_TYPES
  const int bw = mi_size_wide[bsize];
  const int bh = mi_size_high[bsize];
  memset(above_ctx, partition_context_lookup[subsize].above, bw);
  memset(left_ctx, partition_context_lookup[subsize].left, bh);
#else
  // num_4x4_blocks_wide_lookup[bsize] / 2
  const int bs = mi_size_wide[bsize];

  // update the partition context at the end notes. set partition bits
  // of block sizes larger than the current one to be one, and partition
  // bits of smaller block sizes to be zero.
  memset(above_ctx, partition_context_lookup[subsize].above, bs);
  memset(left_ctx, partition_context_lookup[subsize].left, bs);
#endif  // CONFIG_EXT_PARTITION_TYPES
}

#if CONFIG_CB4X4
static INLINE int is_chroma_reference(int mi_row, int mi_col, BLOCK_SIZE bsize,
                                      int subsampling_x, int subsampling_y) {
#if CONFIG_CHROMA_2X2
  return 1;
#endif

#if CONFIG_CHROMA_SUB8X8
  const int bw = mi_size_wide[bsize];
  const int bh = mi_size_high[bsize];

  int ref_pos = ((mi_row & 0x01) || !(bh & 0x01) || !subsampling_y) &&
                ((mi_col & 0x01) || !(bw & 0x01) || !subsampling_x);

  return ref_pos;
#else
  int ref_pos = !(((mi_row & 0x01) && subsampling_y) ||
                  ((mi_col & 0x01) && subsampling_x));

  if (bsize >= BLOCK_8X8) ref_pos = 1;

  return ref_pos;
#endif
}

#if CONFIG_SUPERTX
static INLINE int need_handle_chroma_sub8x8(BLOCK_SIZE bsize, int subsampling_x,
                                            int subsampling_y) {
  const int bw = mi_size_wide[bsize];
  const int bh = mi_size_high[bsize];

  if (bsize >= BLOCK_8X8 ||
      ((!(bh & 0x01) || !subsampling_y) && (!(bw & 0x01) || !subsampling_x)))
    return 0;
  else
    return 1;
}
#endif

static INLINE BLOCK_SIZE scale_chroma_bsize(BLOCK_SIZE bsize, int subsampling_x,
                                            int subsampling_y) {
  BLOCK_SIZE bs = bsize;

  if (bs < BLOCK_8X8) {
    if (subsampling_x == 1 && subsampling_y == 1)
      bs = BLOCK_8X8;
    else if (subsampling_x == 1)
      bs = BLOCK_8X4;
    else if (subsampling_y == 1)
      bs = BLOCK_4X8;
  }

  return bs;
}
#endif

static INLINE aom_cdf_prob cdf_element_prob(const aom_cdf_prob *cdf,
                                            size_t element) {
  assert(cdf != NULL);
#if !CONFIG_ANS
  return (element > 0 ? cdf[element - 1] : CDF_PROB_TOP) - cdf[element];
#else
  return cdf[element] - (element > 0 ? cdf[element - 1] : 0);
#endif
}

static INLINE void partition_gather_horz_alike(aom_cdf_prob *out,
                                               const aom_cdf_prob *const in) {
  out[0] = CDF_PROB_TOP;
  out[0] -= cdf_element_prob(in, PARTITION_HORZ);
  out[0] -= cdf_element_prob(in, PARTITION_SPLIT);
#if CONFIG_EXT_PARTITION_TYPES
  out[0] -= cdf_element_prob(in, PARTITION_HORZ_A);
  out[0] -= cdf_element_prob(in, PARTITION_HORZ_B);
  out[0] -= cdf_element_prob(in, PARTITION_VERT_A);
#endif
  out[0] = AOM_ICDF(out[0]);
  out[1] = AOM_ICDF(CDF_PROB_TOP);
}

static INLINE void partition_gather_vert_alike(aom_cdf_prob *out,
                                               const aom_cdf_prob *const in) {
  out[0] = CDF_PROB_TOP;
  out[0] -= cdf_element_prob(in, PARTITION_VERT);
  out[0] -= cdf_element_prob(in, PARTITION_SPLIT);
#if CONFIG_EXT_PARTITION_TYPES
  out[0] -= cdf_element_prob(in, PARTITION_HORZ_A);
  out[0] -= cdf_element_prob(in, PARTITION_VERT_A);
  out[0] -= cdf_element_prob(in, PARTITION_VERT_B);
#endif
  out[0] = AOM_ICDF(out[0]);
  out[1] = AOM_ICDF(CDF_PROB_TOP);
}

#if CONFIG_EXT_PARTITION_TYPES
static INLINE void update_ext_partition_context(MACROBLOCKD *xd, int mi_row,
                                                int mi_col, BLOCK_SIZE subsize,
                                                BLOCK_SIZE bsize,
                                                PARTITION_TYPE partition) {
  if (bsize >= BLOCK_8X8) {
#if !CONFIG_EXT_PARTITION_TYPES_AB
    const int hbs = mi_size_wide[bsize] / 2;
    BLOCK_SIZE bsize2 = get_subsize(bsize, PARTITION_SPLIT);
#endif
    switch (partition) {
      case PARTITION_SPLIT:
        if (bsize != BLOCK_8X8) break;
      case PARTITION_NONE:
      case PARTITION_HORZ:
      case PARTITION_VERT:
      case PARTITION_HORZ_4:
      case PARTITION_VERT_4:
        update_partition_context(xd, mi_row, mi_col, subsize, bsize);
        break;
#if CONFIG_EXT_PARTITION_TYPES_AB
      case PARTITION_HORZ_A:
        update_partition_context(xd, mi_row, mi_col,
                                 get_subsize(bsize, PARTITION_HORZ_4), subsize);
        update_partition_context(xd, mi_row + mi_size_high[bsize] / 2, mi_col,
                                 subsize, subsize);
        break;
      case PARTITION_HORZ_B:
        update_partition_context(xd, mi_row, mi_col, subsize, subsize);
        update_partition_context(xd, mi_row + mi_size_high[bsize] / 2, mi_col,
                                 get_subsize(bsize, PARTITION_HORZ_4), subsize);
        break;
      case PARTITION_VERT_A:
        update_partition_context(xd, mi_row, mi_col,
                                 get_subsize(bsize, PARTITION_VERT_4), subsize);
        update_partition_context(xd, mi_row, mi_col + mi_size_wide[bsize] / 2,
                                 subsize, subsize);
        break;
      case PARTITION_VERT_B:
        update_partition_context(xd, mi_row, mi_col, subsize, subsize);
        update_partition_context(xd, mi_row, mi_col + mi_size_wide[bsize] / 2,
                                 get_subsize(bsize, PARTITION_VERT_4), subsize);
        break;
#else
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
#endif
      default: assert(0 && "Invalid partition type");
    }
  }
}
#endif  // CONFIG_EXT_PARTITION_TYPES

static INLINE int partition_plane_context(const MACROBLOCKD *xd, int mi_row,
                                          int mi_col,
#if CONFIG_UNPOISON_PARTITION_CTX
                                          int has_rows, int has_cols,
#endif
                                          BLOCK_SIZE bsize) {
  const PARTITION_CONTEXT *above_ctx = xd->above_seg_context + mi_col;
  const PARTITION_CONTEXT *left_ctx =
      xd->left_seg_context + (mi_row & MAX_MIB_MASK);
  // Minimum partition point is 8x8. Offset the bsl accordingly.
  const int bsl = mi_width_log2_lookup[bsize] - mi_width_log2_lookup[BLOCK_8X8];
  int above = (*above_ctx >> bsl) & 1, left = (*left_ctx >> bsl) & 1;

  assert(b_width_log2_lookup[bsize] == b_height_log2_lookup[bsize]);
  assert(bsl >= 0);

#if CONFIG_UNPOISON_PARTITION_CTX
  if (has_rows && has_cols)
    return (left * 2 + above) + bsl * PARTITION_PLOFFSET;
  else if (has_rows && !has_cols)
    return PARTITION_CONTEXTS_PRIMARY + bsl;
  else if (!has_rows && has_cols)
    return PARTITION_CONTEXTS_PRIMARY + PARTITION_BLOCK_SIZES + bsl;
  else
    return INVALID_PARTITION_CTX;  // Bogus context, forced SPLIT
#else
  return (left * 2 + above) + bsl * PARTITION_PLOFFSET;
#endif
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

  // Scale the width in the transform block unit.
  return max_blocks_high >> tx_size_wide_log2[0];
}

#if CONFIG_CFL
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
#endif  // CONFIG_CFL

static INLINE void av1_zero_above_context(AV1_COMMON *const cm,
                                          int mi_col_start, int mi_col_end) {
  const int width = mi_col_end - mi_col_start;
  const int aligned_width = ALIGN_POWER_OF_TWO(width, cm->mib_size_log2);

  const int offset_y = mi_col_start << (MI_SIZE_LOG2 - tx_size_wide_log2[0]);
  const int width_y = aligned_width << (MI_SIZE_LOG2 - tx_size_wide_log2[0]);
  const int offset_uv = offset_y >> cm->subsampling_x;
  const int width_uv = width_y >> cm->subsampling_x;

  av1_zero_array(cm->above_context[0] + offset_y, width_y);
  av1_zero_array(cm->above_context[1] + offset_uv, width_uv);
  av1_zero_array(cm->above_context[2] + offset_uv, width_uv);

  av1_zero_array(cm->above_seg_context + mi_col_start, aligned_width);

#if CONFIG_VAR_TX
  av1_zero_array(cm->above_txfm_context + (mi_col_start << TX_UNIT_WIDE_LOG2),
                 aligned_width << TX_UNIT_WIDE_LOG2);
#endif  // CONFIG_VAR_TX
}

static INLINE void av1_zero_left_context(MACROBLOCKD *const xd) {
  av1_zero(xd->left_context);
  av1_zero(xd->left_seg_context);
#if CONFIG_VAR_TX
  av1_zero(xd->left_txfm_context_buffer);
#endif
}

// Disable array-bounds checks as the TX_SIZE enum contains values larger than
// TX_SIZES_ALL (TX_INVALID) which make extending the array as a workaround
// infeasible. The assert is enough for static analysis and this or other tools
// asan, valgrind would catch oob access at runtime.
#if defined(__GNUC__) && __GNUC__ >= 4
#pragma GCC diagnostic ignored "-Warray-bounds"
#endif
static INLINE TX_SIZE get_min_tx_size(TX_SIZE tx_size) {
  assert(tx_size < TX_SIZES_ALL);
  return txsize_sqr_map[tx_size];
}
#if defined(__GNUC__) && __GNUC__ >= 4
#pragma GCC diagnostic warning "-Warray-bounds"
#endif

#if CONFIG_VAR_TX
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

  set_txfm_ctx(xd->above_txfm_context, bw, n8_w << TX_UNIT_WIDE_LOG2);
  set_txfm_ctx(xd->left_txfm_context, bh, n8_h << TX_UNIT_HIGH_LOG2);
}

static INLINE void txfm_partition_update(TXFM_CONTEXT *above_ctx,
                                         TXFM_CONTEXT *left_ctx,
                                         TX_SIZE tx_size, TX_SIZE txb_size) {
  BLOCK_SIZE bsize = txsize_to_bsize[txb_size];
  int bh = mi_size_high[bsize] << TX_UNIT_HIGH_LOG2;
  int bw = mi_size_wide[bsize] << TX_UNIT_WIDE_LOG2;
  uint8_t txw = tx_size_wide[tx_size];
  uint8_t txh = tx_size_high[tx_size];
  int i;
  for (i = 0; i < bh; ++i) left_ctx[i] = txh;
  for (i = 0; i < bw; ++i) above_ctx[i] = txw;
}

static INLINE TX_SIZE get_sqr_tx_size(int tx_dim) {
  switch (tx_dim) {
#if CONFIG_EXT_PARTITION
    case 128:
#endif  // CONFIG_EXT_PARTITION
    case 64:
#if CONFIG_TX64X64
      return TX_64X64;
#else
      return TX_32X32;
#endif  // CONFIG_TX64X64
      break;
    case 32: return TX_32X32; break;
    case 16: return TX_16X16; break;
    case 8: return TX_8X8; break;
    default: return TX_4X4;
  }
}

static INLINE int txfm_partition_context(TXFM_CONTEXT *above_ctx,
                                         TXFM_CONTEXT *left_ctx,
                                         BLOCK_SIZE bsize, TX_SIZE tx_size) {
  const uint8_t txw = tx_size_wide[tx_size];
  const uint8_t txh = tx_size_high[tx_size];
  const int above = *above_ctx < txw;
  const int left = *left_ctx < txh;
  int category = TXFM_PARTITION_CONTEXTS - 1;

  // dummy return, not used by others.
  if (tx_size <= TX_4X4) return 0;

  TX_SIZE max_tx_size =
      get_sqr_tx_size(AOMMAX(block_size_wide[bsize], block_size_high[bsize]));

  if (max_tx_size >= TX_8X8) {
    category = (tx_size != max_tx_size && max_tx_size > TX_8X8) +
               (TX_SIZES - 1 - max_tx_size) * 2;
  }
  if (category == TXFM_PARTITION_CONTEXTS - 1) return category;
  return category * 3 + above + left;
}
#endif

// Compute the next partition in the direction of the sb_type stored in the mi
// array, starting with bsize.
static INLINE PARTITION_TYPE get_partition(const AV1_COMMON *const cm,
                                           int mi_row, int mi_col,
                                           BLOCK_SIZE bsize) {
  if (mi_row >= cm->mi_rows || mi_col >= cm->mi_cols) return PARTITION_INVALID;

  const int offset = mi_row * cm->mi_stride + mi_col;
  MODE_INFO **mi = cm->mi_grid_visible + offset;
  const BLOCK_SIZE subsize = mi[0]->mbmi.sb_type;

  if (subsize == bsize) return PARTITION_NONE;

  const int bhigh = mi_size_high[bsize];
  const int bwide = mi_size_wide[bsize];
  const int sshigh = mi_size_high[subsize];
  const int sswide = mi_size_wide[subsize];

#if CONFIG_EXT_PARTITION_TYPES
  if (bsize > BLOCK_8X8 && mi_row + bwide / 2 < cm->mi_rows &&
      mi_col + bhigh / 2 < cm->mi_cols) {
    // In this case, the block might be using an extended partition
    // type.
    const MB_MODE_INFO *const mbmi_right = &mi[bwide / 2]->mbmi;
    const MB_MODE_INFO *const mbmi_below = &mi[bhigh / 2 * cm->mi_stride]->mbmi;

    if (sswide == bwide) {
#if CONFIG_EXT_PARTITION_TYPES_AB
      // Smaller height but same width. Is PARTITION_HORZ, PARTITION_HORZ_4,
      // PARTITION_HORZ_A or PARTITION_HORZ_B.
      if (sshigh * 2 == bhigh)
        return (mbmi_below->sb_type == subsize) ? PARTITION_HORZ
                                                : PARTITION_HORZ_B;
      assert(sshigh * 4 == bhigh);
      return (mbmi_below->sb_type == subsize) ? PARTITION_HORZ_4
                                              : PARTITION_HORZ_A;
#else
      // Smaller height but same width. Is PARTITION_HORZ_4, PARTITION_HORZ or
      // PARTITION_HORZ_B. To distinguish the latter two, check if the lower
      // half was split.
      if (sshigh * 4 == bhigh) return PARTITION_HORZ_4;
      assert(sshigh * 2 == bhigh);

      if (mbmi_below->sb_type == subsize)
        return PARTITION_HORZ;
      else
        return PARTITION_HORZ_B;
#endif
    } else if (sshigh == bhigh) {
#if CONFIG_EXT_PARTITION_TYPES_AB
      // Smaller width but same height. Is PARTITION_VERT, PARTITION_VERT_4,
      // PARTITION_VERT_A or PARTITION_VERT_B.
      if (sswide * 2 == bwide)
        return (mbmi_right->sb_type == subsize) ? PARTITION_VERT
                                                : PARTITION_VERT_B;
      assert(sswide * 4 == bwide);
      return (mbmi_right->sb_type == subsize) ? PARTITION_VERT_4
                                              : PARTITION_VERT_A;
#else
      // Smaller width but same height. Is PARTITION_VERT_4, PARTITION_VERT or
      // PARTITION_VERT_B. To distinguish the latter two, check if the right
      // half was split.
      if (sswide * 4 == bwide) return PARTITION_VERT_4;
      assert(sswide * 2 == bhigh);

      if (mbmi_right->sb_type == subsize)
        return PARTITION_VERT;
      else
        return PARTITION_VERT_B;
#endif
    } else {
#if !CONFIG_EXT_PARTITION_TYPES_AB
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
#endif

      return PARTITION_SPLIT;
    }
  }
#endif
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
#if CONFIG_REFERENCE_BUFFER
  cm->seq_params.frame_id_numbers_present_flag = use;
#else
  (void)cm;
  (void)use;
#endif
}

static INLINE void set_sb_size(AV1_COMMON *const cm, BLOCK_SIZE sb_size) {
  cm->sb_size = sb_size;
  cm->mib_size = mi_size_wide[cm->sb_size];
#if CONFIG_CB4X4
  cm->mib_size_log2 = b_width_log2_lookup[cm->sb_size];
#else
  cm->mib_size_log2 = mi_width_log2_lookup[cm->sb_size];
#endif
}

static INLINE int all_lossless(const AV1_COMMON *cm, const MACROBLOCKD *xd) {
  int i;
  int all_lossless = 1;
  if (cm->seg.enabled) {
    for (i = 0; i < MAX_SEGMENTS; ++i) {
      if (!xd->lossless[i]) {
        all_lossless = 0;
        break;
      }
    }
  } else {
    all_lossless = xd->lossless[0];
  }
  return all_lossless;
}

static INLINE int use_compressed_header(const AV1_COMMON *cm) {
  (void)cm;
#if CONFIG_RESTRICT_COMPRESSED_HDR && CONFIG_NEW_MULTISYMBOL
  return 0;
#elif CONFIG_RESTRICT_COMPRESSED_HDR
  return cm->refresh_frame_context == REFRESH_FRAME_CONTEXT_FORWARD;
#else
  return 1;
#endif  // CONFIG_RESTRICT_COMPRESSED_HDR && CONFIG_NEW_MULTISYMBOL
}

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // AV1_COMMON_ONYXC_INT_H_
