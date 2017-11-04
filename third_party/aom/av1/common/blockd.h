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

#ifndef AV1_COMMON_BLOCKD_H_
#define AV1_COMMON_BLOCKD_H_

#include "./aom_config.h"

#include "aom_dsp/aom_dsp_common.h"
#include "aom_ports/mem.h"
#include "aom_scale/yv12config.h"

#include "av1/common/common_data.h"
#include "av1/common/quant_common.h"
#include "av1/common/entropy.h"
#include "av1/common/entropymode.h"
#include "av1/common/mv.h"
#include "av1/common/scale.h"
#include "av1/common/seg_common.h"
#include "av1/common/tile_common.h"
#if CONFIG_PVQ
#include "av1/common/pvq.h"
#include "av1/common/pvq_state.h"
#include "av1/decoder/decint.h"
#endif
#ifdef __cplusplus
extern "C" {
#endif

#if (CONFIG_CHROMA_SUB8X8 || CONFIG_CHROMA_2X2)
#define SUB8X8_COMP_REF 0
#else
#define SUB8X8_COMP_REF 1
#endif

#define MAX_MB_PLANE 3

#if CONFIG_COMPOUND_SEGMENT
// Set COMPOUND_SEGMENT_TYPE to one of the three
// 0: Uniform
// 1: Difference weighted
#define COMPOUND_SEGMENT_TYPE 1
#define MAX_SEG_MASK_BITS 1

// SEG_MASK_TYPES should not surpass 1 << MAX_SEG_MASK_BITS
typedef enum {
#if COMPOUND_SEGMENT_TYPE == 0
  UNIFORM_45 = 0,
  UNIFORM_45_INV,
#elif COMPOUND_SEGMENT_TYPE == 1
  DIFFWTD_38 = 0,
  DIFFWTD_38_INV,
#endif  // COMPOUND_SEGMENT_TYPE
  SEG_MASK_TYPES,
} SEG_MASK_TYPE;

#endif  // CONFIG_COMPOUND_SEGMENT

typedef enum {
  KEY_FRAME = 0,
  INTER_FRAME = 1,
#if CONFIG_OBU
  INTRA_ONLY_FRAME = 2,  // replaces intra-only
  S_FRAME = 3,
#endif
  FRAME_TYPES,
} FRAME_TYPE;

static INLINE int is_comp_ref_allowed(BLOCK_SIZE bsize) {
  (void)bsize;
#if SUB8X8_COMP_REF
  return 1;
#else
  return AOMMIN(block_size_wide[bsize], block_size_high[bsize]) >= 8;
#endif  // SUB8X8_COMP_REF
}

static INLINE int is_inter_mode(PREDICTION_MODE mode) {
  return mode >= NEARESTMV && mode <= NEW_NEWMV;
}

#if CONFIG_PVQ
typedef struct PVQ_INFO {
  int theta[PVQ_MAX_PARTITIONS];
  int qg[PVQ_MAX_PARTITIONS];
  int k[PVQ_MAX_PARTITIONS];
  od_coeff y[OD_TXSIZE_MAX * OD_TXSIZE_MAX];
  int nb_bands;
  int off[PVQ_MAX_PARTITIONS];
  int size[PVQ_MAX_PARTITIONS];
  int skip_rest;
  int skip_dir;
  int bs;  // log of the block size minus two,
           // i.e. equivalent to aom's TX_SIZE
  // Block skip info, indicating whether DC/AC, is coded.
  PVQ_SKIP_TYPE ac_dc_coded;  // bit0: DC coded, bit1 : AC coded (1 means coded)
  tran_low_t dq_dc_residue;
} PVQ_INFO;

typedef struct PVQ_QUEUE {
  PVQ_INFO *buf;  // buffer for pvq info, stored in encoding order
  int curr_pos;   // curr position to write PVQ_INFO
  int buf_len;    // allocated buffer length
  int last_pos;   // last written position of PVQ_INFO in a tile
} PVQ_QUEUE;
#endif

#if CONFIG_NCOBMC_ADAPT_WEIGHT
typedef struct superblock_mi_boundaries {
  int mi_row_begin;
  int mi_col_begin;
  int mi_row_end;
  int mi_col_end;
} SB_MI_BD;

typedef struct { int16_t KERNEL[4][MAX_SB_SIZE][MAX_SB_SIZE]; } NCOBMC_KERNELS;
#endif

typedef struct {
  uint8_t *plane[MAX_MB_PLANE];
  int stride[MAX_MB_PLANE];
} BUFFER_SET;

static INLINE int is_inter_singleref_mode(PREDICTION_MODE mode) {
  return mode >= NEARESTMV && mode <= NEWMV;
}
static INLINE int is_inter_compound_mode(PREDICTION_MODE mode) {
  return mode >= NEAREST_NEARESTMV && mode <= NEW_NEWMV;
}
#if CONFIG_COMPOUND_SINGLEREF
static INLINE int is_inter_singleref_comp_mode(PREDICTION_MODE mode) {
  return mode >= SR_NEAREST_NEARMV && mode <= SR_NEW_NEWMV;
}
static INLINE int is_inter_anyref_comp_mode(PREDICTION_MODE mode) {
  return is_inter_compound_mode(mode) || is_inter_singleref_comp_mode(mode);
}
#endif  // CONFIG_COMPOUND_SINGLEREF

static INLINE PREDICTION_MODE compound_ref0_mode(PREDICTION_MODE mode) {
  static PREDICTION_MODE lut[] = {
    MB_MODE_COUNT,  // DC_PRED
    MB_MODE_COUNT,  // V_PRED
    MB_MODE_COUNT,  // H_PRED
    MB_MODE_COUNT,  // D45_PRED
    MB_MODE_COUNT,  // D135_PRED
    MB_MODE_COUNT,  // D117_PRED
    MB_MODE_COUNT,  // D153_PRED
    MB_MODE_COUNT,  // D207_PRED
    MB_MODE_COUNT,  // D63_PRED
    MB_MODE_COUNT,  // SMOOTH_PRED
#if CONFIG_SMOOTH_HV
    MB_MODE_COUNT,  // SMOOTH_V_PRED
    MB_MODE_COUNT,  // SMOOTH_H_PRED
#endif              // CONFIG_SMOOTH_HV
    MB_MODE_COUNT,  // TM_PRED
    MB_MODE_COUNT,  // NEARESTMV
    MB_MODE_COUNT,  // NEARMV
    MB_MODE_COUNT,  // ZEROMV
    MB_MODE_COUNT,  // NEWMV
#if CONFIG_COMPOUND_SINGLEREF
    NEARESTMV,  // SR_NEAREST_NEARMV
    // NEARESTMV,  // SR_NEAREST_NEWMV
    NEARMV,     // SR_NEAR_NEWMV
    ZEROMV,     // SR_ZERO_NEWMV
    NEWMV,      // SR_NEW_NEWMV
#endif          // CONFIG_COMPOUND_SINGLEREF
    NEARESTMV,  // NEAREST_NEARESTMV
    NEARMV,     // NEAR_NEARMV
    NEARESTMV,  // NEAREST_NEWMV
    NEWMV,      // NEW_NEARESTMV
    NEARMV,     // NEAR_NEWMV
    NEWMV,      // NEW_NEARMV
    ZEROMV,     // ZERO_ZEROMV
    NEWMV,      // NEW_NEWMV
  };
  assert(NELEMENTS(lut) == MB_MODE_COUNT);
#if CONFIG_COMPOUND_SINGLEREF
  assert(is_inter_anyref_comp_mode(mode));
#else   // !CONFIG_COMPOUND_SINGLEREF
  assert(is_inter_compound_mode(mode));
#endif  // CONFIG_COMPOUND_SINGLEREF
  return lut[mode];
}

static INLINE PREDICTION_MODE compound_ref1_mode(PREDICTION_MODE mode) {
  static PREDICTION_MODE lut[] = {
    MB_MODE_COUNT,  // DC_PRED
    MB_MODE_COUNT,  // V_PRED
    MB_MODE_COUNT,  // H_PRED
    MB_MODE_COUNT,  // D45_PRED
    MB_MODE_COUNT,  // D135_PRED
    MB_MODE_COUNT,  // D117_PRED
    MB_MODE_COUNT,  // D153_PRED
    MB_MODE_COUNT,  // D207_PRED
    MB_MODE_COUNT,  // D63_PRED
    MB_MODE_COUNT,  // SMOOTH_PRED
#if CONFIG_SMOOTH_HV
    MB_MODE_COUNT,  // SMOOTH_V_PRED
    MB_MODE_COUNT,  // SMOOTH_H_PRED
#endif              // CONFIG_SMOOTH_HV
    MB_MODE_COUNT,  // TM_PRED
    MB_MODE_COUNT,  // NEARESTMV
    MB_MODE_COUNT,  // NEARMV
    MB_MODE_COUNT,  // ZEROMV
    MB_MODE_COUNT,  // NEWMV
#if CONFIG_COMPOUND_SINGLEREF
    NEARMV,  // SR_NEAREST_NEARMV
    // NEWMV,      // SR_NEAREST_NEWMV
    NEWMV,      // SR_NEAR_NEWMV
    NEWMV,      // SR_ZERO_NEWMV
    NEWMV,      // SR_NEW_NEWMV
#endif          // CONFIG_COMPOUND_SINGLEREF
    NEARESTMV,  // NEAREST_NEARESTMV
    NEARMV,     // NEAR_NEARMV
    NEWMV,      // NEAREST_NEWMV
    NEARESTMV,  // NEW_NEARESTMV
    NEWMV,      // NEAR_NEWMV
    NEARMV,     // NEW_NEARMV
    ZEROMV,     // ZERO_ZEROMV
    NEWMV,      // NEW_NEWMV
  };
  assert(NELEMENTS(lut) == MB_MODE_COUNT);
#if CONFIG_COMPOUND_SINGLEREF
  assert(is_inter_anyref_comp_mode(mode));
#else   // !CONFIG_COMPOUND_SINGLEREF
  assert(is_inter_compound_mode(mode));
#endif  // CONFIG_COMPOUND_SINGLEREF
  return lut[mode];
}

static INLINE int have_nearmv_in_inter_mode(PREDICTION_MODE mode) {
  return (mode == NEARMV || mode == NEAR_NEARMV || mode == NEAR_NEWMV ||
#if CONFIG_COMPOUND_SINGLEREF
          mode == SR_NEAREST_NEARMV || mode == SR_NEAR_NEWMV ||
#endif  // CONFIG_COMPOUND_SINGLEREF
          mode == NEW_NEARMV);
}

static INLINE int have_newmv_in_inter_mode(PREDICTION_MODE mode) {
  return (mode == NEWMV || mode == NEW_NEWMV || mode == NEAREST_NEWMV ||
#if CONFIG_COMPOUND_SINGLEREF
          /* mode == SR_NEAREST_NEWMV || */ mode == SR_NEAR_NEWMV ||
          mode == SR_ZERO_NEWMV || mode == SR_NEW_NEWMV ||
#endif  // CONFIG_COMPOUND_SINGLEREF
          mode == NEW_NEARESTMV || mode == NEAR_NEWMV || mode == NEW_NEARMV);
}

static INLINE int use_masked_motion_search(COMPOUND_TYPE type) {
#if CONFIG_WEDGE
  return (type == COMPOUND_WEDGE);
#else
  (void)type;
  return 0;
#endif
}

static INLINE int is_masked_compound_type(COMPOUND_TYPE type) {
#if CONFIG_COMPOUND_SEGMENT && CONFIG_WEDGE
  return (type == COMPOUND_WEDGE || type == COMPOUND_SEG);
#elif !CONFIG_COMPOUND_SEGMENT && CONFIG_WEDGE
  return (type == COMPOUND_WEDGE);
#elif CONFIG_COMPOUND_SEGMENT && !CONFIG_WEDGE
  return (type == COMPOUND_SEG);
#endif  // CONFIG_COMPOUND_SEGMENT
  (void)type;
  return 0;
}

/* For keyframes, intra block modes are predicted by the (already decoded)
   modes for the Y blocks to the left and above us; for interframes, there
   is a single probability table. */

typedef struct {
  PREDICTION_MODE as_mode;
  int_mv as_mv[2];  // first, second inter predictor motion vectors
  int_mv pred_mv[2];
  int_mv ref_mv[2];
} b_mode_info;

typedef int8_t MV_REFERENCE_FRAME;

typedef struct {
  // Number of base colors for Y (0) and UV (1)
  uint8_t palette_size[2];
  // Value of base colors for Y, U, and V
  uint16_t palette_colors[3 * PALETTE_MAX_SIZE];
} PALETTE_MODE_INFO;

#if CONFIG_FILTER_INTRA
#define USE_3TAP_INTRA_FILTER 1  // 0: 4-tap; 1: 3-tap
typedef struct {
  // 1: an ext intra mode is used; 0: otherwise.
  uint8_t use_filter_intra_mode[PLANE_TYPES];
  FILTER_INTRA_MODE filter_intra_mode[PLANE_TYPES];
} FILTER_INTRA_MODE_INFO;
#endif  // CONFIG_FILTER_INTRA

#if CONFIG_VAR_TX
#if CONFIG_RD_DEBUG
#define TXB_COEFF_COST_MAP_SIZE (2 * MAX_MIB_SIZE)
#endif
#endif

typedef struct RD_STATS {
  int rate;
  int64_t dist;
  // Please be careful of using rdcost, it's not guaranteed to be set all the
  // time.
  // TODO(angiebird): Create a set of functions to manipulate the RD_STATS. In
  // these functions, make sure rdcost is always up-to-date according to
  // rate/dist.
  int64_t rdcost;
  int64_t sse;
  int skip;  // sse should equal to dist when skip == 1
  int64_t ref_rdcost;
  int zero_rate;
  uint8_t invalid_rate;
#if CONFIG_RD_DEBUG
  int txb_coeff_cost[MAX_MB_PLANE];
#if CONFIG_VAR_TX
  int txb_coeff_cost_map[MAX_MB_PLANE][TXB_COEFF_COST_MAP_SIZE]
                        [TXB_COEFF_COST_MAP_SIZE];
#endif  // CONFIG_VAR_TX
#endif  // CONFIG_RD_DEBUG
} RD_STATS;

// This struct is used to group function args that are commonly
// sent together in functions related to interinter compound modes
typedef struct {
#if CONFIG_WEDGE
  int wedge_index;
  int wedge_sign;
#endif  // CONFIG_WEDGE
#if CONFIG_COMPOUND_SEGMENT
  SEG_MASK_TYPE mask_type;
  uint8_t *seg_mask;
#endif  // CONFIG_COMPOUND_SEGMENT
  COMPOUND_TYPE interinter_compound_type;
} INTERINTER_COMPOUND_DATA;

// This structure now relates to 8x8 block regions.
typedef struct MB_MODE_INFO {
  // Common for both INTER and INTRA blocks
  BLOCK_SIZE sb_type;
  PREDICTION_MODE mode;
  TX_SIZE tx_size;
#if CONFIG_VAR_TX
  // TODO(jingning): This effectively assigned a separate entry for each
  // 8x8 block. Apparently it takes much more space than needed.
  TX_SIZE inter_tx_size[MAX_MIB_SIZE][MAX_MIB_SIZE];
  TX_SIZE min_tx_size;
#endif
  int8_t skip;
  int8_t segment_id;
#if CONFIG_SUPERTX
  // Minimum of all segment IDs under the current supertx block.
  int8_t segment_id_supertx;
#endif                      // CONFIG_SUPERTX
  int8_t seg_id_predicted;  // valid only when temporal_update is enabled

#if CONFIG_MRC_TX
  int valid_mrc_mask;
#endif  // CONFIG_MRC_TX

  // Only for INTRA blocks
  UV_PREDICTION_MODE uv_mode;

  PALETTE_MODE_INFO palette_mode_info;
#if CONFIG_INTRABC
  uint8_t use_intrabc;
#endif  // CONFIG_INTRABC

  // Only for INTER blocks
  InterpFilters interp_filters;
  MV_REFERENCE_FRAME ref_frame[2];
  TX_TYPE tx_type;
#if CONFIG_TXK_SEL
  TX_TYPE txk_type[MAX_SB_SQUARE / (TX_SIZE_W_MIN * TX_SIZE_H_MIN)];
#endif
#if CONFIG_LGT_FROM_PRED
  int use_lgt;
#endif

#if CONFIG_FILTER_INTRA
  FILTER_INTRA_MODE_INFO filter_intra_mode_info;
#endif  // CONFIG_FILTER_INTRA
#if CONFIG_EXT_INTRA
  // The actual prediction angle is the base angle + (angle_delta * step).
  int8_t angle_delta[2];
#if CONFIG_INTRA_INTERP
  // To-Do (huisu): this may be replaced by interp_filter
  INTRA_FILTER intra_filter;
#endif  // CONFIG_INTRA_INTERP
#endif  // CONFIG_EXT_INTRA

#if CONFIG_INTERINTRA
  // interintra members
  INTERINTRA_MODE interintra_mode;
#endif
  // TODO(debargha): Consolidate these flags
  int use_wedge_interintra;
  int interintra_wedge_index;
  int interintra_wedge_sign;
  // interinter members
  COMPOUND_TYPE interinter_compound_type;
#if CONFIG_WEDGE
  int wedge_index;
  int wedge_sign;
#endif  // CONFIG_WEDGE
#if CONFIG_COMPOUND_SEGMENT
  SEG_MASK_TYPE mask_type;
#endif  // CONFIG_COMPOUND_SEGMENT
  MOTION_MODE motion_mode;
#if CONFIG_MOTION_VAR
  int overlappable_neighbors[2];
#if CONFIG_NCOBMC_ADAPT_WEIGHT
  // Applying different weighting kernels in ncobmc
  // In current implementation, interpolation modes only defined for squared
  // blocks. A rectangular block is divided into two squared blocks and each
  // squared block has an interpolation mode.
  NCOBMC_MODE ncobmc_mode[2];
#endif  // CONFIG_NCOBMC_ADAPT_WEIGHT
#endif  // CONFIG_MOTION_VAR
  int_mv mv[2];
  int_mv pred_mv[2];
  uint8_t ref_mv_idx;
#if CONFIG_EXT_PARTITION_TYPES
  PARTITION_TYPE partition;
#endif
#if CONFIG_NEW_QUANT
  int dq_off_index;
  int send_dq_bit;
#endif  // CONFIG_NEW_QUANT
  /* deringing gain *per-superblock* */
  int8_t cdef_strength;
  int current_q_index;
#if CONFIG_EXT_DELTA_Q
  int current_delta_lf_from_base;
#if CONFIG_LOOPFILTER_LEVEL
  int curr_delta_lf[FRAME_LF_COUNT];
#endif  // CONFIG_LOOPFILTER_LEVEL
#endif
#if CONFIG_RD_DEBUG
  RD_STATS rd_stats;
  int mi_row;
  int mi_col;
#endif
#if CONFIG_WARPED_MOTION
  int num_proj_ref[2];
  WarpedMotionParams wm_params[2];
#endif  // CONFIG_WARPED_MOTION

#if CONFIG_CFL
  // Index of the alpha Cb and alpha Cr combination
  int cfl_alpha_idx;
  // Joint sign of alpha Cb and alpha Cr
  int cfl_alpha_signs;
#endif

  BOUNDARY_TYPE boundary_info;
#if CONFIG_LPF_SB
  uint8_t filt_lvl;
  int reuse_sb_lvl;
  int sign;
  int delta;
#endif
} MB_MODE_INFO;

typedef struct MODE_INFO {
  MB_MODE_INFO mbmi;
  b_mode_info bmi[4];
} MODE_INFO;

#if CONFIG_INTRABC
static INLINE int is_intrabc_block(const MB_MODE_INFO *mbmi) {
  return mbmi->use_intrabc;
}
#endif

static INLINE PREDICTION_MODE get_y_mode(const MODE_INFO *mi, int block) {
#if CONFIG_CB4X4
  (void)block;
  return mi->mbmi.mode;
#else
  return mi->mbmi.sb_type < BLOCK_8X8 ? mi->bmi[block].as_mode : mi->mbmi.mode;
#endif
}

#if CONFIG_CFL
static INLINE PREDICTION_MODE get_uv_mode(UV_PREDICTION_MODE mode) {
  static const PREDICTION_MODE uv2y[UV_INTRA_MODES] = {
    DC_PRED,      // UV_DC_PRED
    V_PRED,       // UV_V_PRED
    H_PRED,       // UV_H_PRED
    D45_PRED,     // UV_D45_PRED
    D135_PRED,    // UV_D135_PRED
    D117_PRED,    // UV_D117_PRED
    D153_PRED,    // UV_D153_PRED
    D207_PRED,    // UV_D207_PRED
    D63_PRED,     // UV_D63_PRED
    SMOOTH_PRED,  // UV_SMOOTH_PRED
#if CONFIG_SMOOTH_HV
    SMOOTH_V_PRED,  // UV_SMOOTH_V_PRED
    SMOOTH_H_PRED,  // UV_SMOOTH_H_PRED
#endif              // CONFIG_SMOOTH_HV
    TM_PRED,        // UV_TM_PRED
    DC_PRED,        // CFL_PRED
  };
  return uv2y[mode];
}
#else
static INLINE PREDICTION_MODE get_uv_mode(PREDICTION_MODE mode) { return mode; }
#endif  // CONFIG_CFL

static INLINE int is_inter_block(const MB_MODE_INFO *mbmi) {
#if CONFIG_INTRABC
  if (is_intrabc_block(mbmi)) return 1;
#endif
  return mbmi->ref_frame[0] > INTRA_FRAME;
}

static INLINE int has_second_ref(const MB_MODE_INFO *mbmi) {
  return mbmi->ref_frame[1] > INTRA_FRAME;
}

#if CONFIG_EXT_COMP_REFS
static INLINE int has_uni_comp_refs(const MB_MODE_INFO *mbmi) {
  return has_second_ref(mbmi) && (!((mbmi->ref_frame[0] >= BWDREF_FRAME) ^
                                    (mbmi->ref_frame[1] >= BWDREF_FRAME)));
}

static INLINE MV_REFERENCE_FRAME comp_ref0(int ref_idx) {
  static const MV_REFERENCE_FRAME lut[] = {
    LAST_FRAME,    // LAST_LAST2_FRAMES,
    LAST_FRAME,    // LAST_LAST3_FRAMES,
    LAST_FRAME,    // LAST_GOLDEN_FRAMES,
    BWDREF_FRAME,  // BWDREF_ALTREF_FRAMES,
  };
  assert(NELEMENTS(lut) == UNIDIR_COMP_REFS);
  return lut[ref_idx];
}

static INLINE MV_REFERENCE_FRAME comp_ref1(int ref_idx) {
  static const MV_REFERENCE_FRAME lut[] = {
    LAST2_FRAME,   // LAST_LAST2_FRAMES,
    LAST3_FRAME,   // LAST_LAST3_FRAMES,
    GOLDEN_FRAME,  // LAST_GOLDEN_FRAMES,
    ALTREF_FRAME,  // BWDREF_ALTREF_FRAMES,
  };
  assert(NELEMENTS(lut) == UNIDIR_COMP_REFS);
  return lut[ref_idx];
}
#endif  // CONFIG_EXT_COMP_REFS

PREDICTION_MODE av1_left_block_mode(const MODE_INFO *cur_mi,
                                    const MODE_INFO *left_mi, int b);

PREDICTION_MODE av1_above_block_mode(const MODE_INFO *cur_mi,
                                     const MODE_INFO *above_mi, int b);

#if CONFIG_GLOBAL_MOTION
static INLINE int is_global_mv_block(const MODE_INFO *mi, int block,
                                     TransformationType type) {
  PREDICTION_MODE mode = get_y_mode(mi, block);
#if GLOBAL_SUB8X8_USED
  const int block_size_allowed = 1;
#else
  const BLOCK_SIZE bsize = mi->mbmi.sb_type;
  const int block_size_allowed =
      AOMMIN(block_size_wide[bsize], block_size_high[bsize]) >= 8;
#endif  // GLOBAL_SUB8X8_USED
  return (mode == ZEROMV || mode == ZERO_ZEROMV) && type > TRANSLATION &&
         block_size_allowed;
}
#endif  // CONFIG_GLOBAL_MOTION

enum mv_precision { MV_PRECISION_Q3, MV_PRECISION_Q4 };

struct buf_2d {
  uint8_t *buf;
  uint8_t *buf0;
  int width;
  int height;
  int stride;
};

typedef struct macroblockd_plane {
  tran_low_t *dqcoeff;
  PLANE_TYPE plane_type;
  int subsampling_x;
  int subsampling_y;
  struct buf_2d dst;
  struct buf_2d pre[2];
  ENTROPY_CONTEXT *above_context;
  ENTROPY_CONTEXT *left_context;
  int16_t seg_dequant[MAX_SEGMENTS][2];
#if CONFIG_NEW_QUANT
  dequant_val_type_nuq seg_dequant_nuq[MAX_SEGMENTS][QUANT_PROFILES]
                                      [COEF_BANDS];
#endif
  uint8_t *color_index_map;

  // number of 4x4s in current block
  uint16_t n4_w, n4_h;
  // log2 of n4_w, n4_h
  uint8_t n4_wl, n4_hl;
  // block size in pixels
  uint8_t width, height;

#if CONFIG_AOM_QM
  qm_val_t *seg_iqmatrix[MAX_SEGMENTS][2][TX_SIZES_ALL];
  qm_val_t *seg_qmatrix[MAX_SEGMENTS][2][TX_SIZES_ALL];
#endif
  // encoder
  const int16_t *dequant;
#if CONFIG_NEW_QUANT
  const dequant_val_type_nuq *dequant_val_nuq[QUANT_PROFILES];
#endif  // CONFIG_NEW_QUANT

#if CONFIG_PVQ || CONFIG_DIST_8X8
  DECLARE_ALIGNED(16, int16_t, pred[MAX_SB_SQUARE]);
#endif
#if CONFIG_PVQ
  // PVQ: forward transformed predicted image, a reference for PVQ.
  tran_low_t *pvq_ref_coeff;
#endif
} MACROBLOCKD_PLANE;

#define BLOCK_OFFSET(x, i) \
  ((x) + (i) * (1 << (tx_size_wide_log2[0] + tx_size_high_log2[0])))

typedef struct RefBuffer {
  int idx;
  YV12_BUFFER_CONFIG *buf;
  struct scale_factors sf;
#if CONFIG_VAR_REFS
  int is_valid;
#endif  // CONFIG_VAR_REFS
} RefBuffer;

#if CONFIG_ADAPT_SCAN
typedef int16_t EobThresholdMD[TX_TYPES][EOB_THRESHOLD_NUM];
#endif

#if CONFIG_LOOP_RESTORATION
typedef struct {
  DECLARE_ALIGNED(16, InterpKernel, vfilter);
  DECLARE_ALIGNED(16, InterpKernel, hfilter);
} WienerInfo;

typedef struct {
  int ep;
  int xqd[2];
} SgrprojInfo;
#endif  // CONFIG_LOOP_RESTORATION

#if CONFIG_CFL
#if CONFIG_CHROMA_SUB8X8 && CONFIG_DEBUG
#define CFL_SUB8X8_VAL_MI_SIZE (4)
#define CFL_SUB8X8_VAL_MI_SQUARE \
  (CFL_SUB8X8_VAL_MI_SIZE * CFL_SUB8X8_VAL_MI_SIZE)
#endif  // CONFIG_CHROMA_SUB8X8 && CONFIG_DEBUG
typedef struct cfl_ctx {
  // The CfL prediction buffer is used in two steps:
  //   1. Stores Q3 reconstructed luma pixels
  //      (only Q2 is required, but Q3 is used to avoid shifts)
  //   2. Stores Q3 AC contributions (step1 - tx block avg)
  int16_t pred_buf_q3[MAX_SB_SQUARE];

  // Height and width currently used in the CfL prediction buffer.
  int buf_height, buf_width;

  // Height and width of the chroma prediction block currently associated with
  // this context
  int uv_height, uv_width;

  int are_parameters_computed;

  // Chroma subsampling
  int subsampling_x, subsampling_y;

  // Block level DC_PRED for each chromatic plane
  int dc_pred[CFL_PRED_PLANES];

  int mi_row, mi_col;

  // Whether the reconstructed luma pixels need to be stored
  int store_y;

#if CONFIG_CB4X4
  int is_chroma_reference;
#if CONFIG_CHROMA_SUB8X8 && CONFIG_DEBUG
  // The prediction used for sub8x8 blocks originates from multiple luma blocks,
  // this array is used to validate that cfl_store() is called only once for
  // each luma block
  uint8_t sub8x8_val[CFL_SUB8X8_VAL_MI_SQUARE];
#endif  // CONFIG_CHROMA_SUB8X8 && CONFIG_DEBUG
#endif  // CONFIG_CB4X4
} CFL_CTX;
#endif  // CONFIG_CFL

typedef struct macroblockd {
  struct macroblockd_plane plane[MAX_MB_PLANE];
  uint8_t bmode_blocks_wl;
  uint8_t bmode_blocks_hl;

  FRAME_COUNTS *counts;
  TileInfo tile;

  int mi_stride;

  MODE_INFO **mi;
  MODE_INFO *left_mi;
  MODE_INFO *above_mi;
  MB_MODE_INFO *left_mbmi;
  MB_MODE_INFO *above_mbmi;

  int up_available;
  int left_available;
#if CONFIG_CHROMA_SUB8X8
  int chroma_up_available;
  int chroma_left_available;
#endif

  const aom_prob (*partition_probs)[PARTITION_TYPES - 1];

  /* Distance of MB away from frame edges in subpixels (1/8th pixel)  */
  int mb_to_left_edge;
  int mb_to_right_edge;
  int mb_to_top_edge;
  int mb_to_bottom_edge;

  FRAME_CONTEXT *fc;

  /* pointers to reference frames */
  const RefBuffer *block_refs[2];

  /* pointer to current frame */
  const YV12_BUFFER_CONFIG *cur_buf;

#if CONFIG_INTRABC
  /* Scale of the current frame with respect to itself */
  struct scale_factors sf_identity;
#endif

  ENTROPY_CONTEXT *above_context[MAX_MB_PLANE];
  ENTROPY_CONTEXT left_context[MAX_MB_PLANE][2 * MAX_MIB_SIZE];

  PARTITION_CONTEXT *above_seg_context;
  PARTITION_CONTEXT left_seg_context[MAX_MIB_SIZE];

#if CONFIG_VAR_TX
  TXFM_CONTEXT *above_txfm_context;
  TXFM_CONTEXT *left_txfm_context;
  TXFM_CONTEXT left_txfm_context_buffer[2 * MAX_MIB_SIZE];

  TX_SIZE max_tx_size;
#if CONFIG_SUPERTX
  TX_SIZE supertx_size;
#endif
#endif

#if CONFIG_LOOP_RESTORATION
  WienerInfo wiener_info[MAX_MB_PLANE];
  SgrprojInfo sgrproj_info[MAX_MB_PLANE];
#endif  // CONFIG_LOOP_RESTORATION

  // block dimension in the unit of mode_info.
  uint8_t n8_w, n8_h;

  uint8_t ref_mv_count[MODE_CTX_REF_FRAMES];
  CANDIDATE_MV ref_mv_stack[MODE_CTX_REF_FRAMES][MAX_REF_MV_STACK_SIZE];
  uint8_t is_sec_rect;

#if CONFIG_PVQ
  daala_dec_ctx daala_dec;
#endif
  FRAME_CONTEXT *tile_ctx;
  /* Bit depth: 8, 10, 12 */
  int bd;

  int qindex[MAX_SEGMENTS];
  int lossless[MAX_SEGMENTS];
  int corrupted;
#if CONFIG_AMVR
  int cur_frame_mv_precision_level;
// same with that in AV1_COMMON
#endif
  struct aom_internal_error_info *error_info;
#if CONFIG_GLOBAL_MOTION
  WarpedMotionParams *global_motion;
#endif  // CONFIG_GLOBAL_MOTION
  int prev_qindex;
  int delta_qindex;
  int current_qindex;
#if CONFIG_EXT_DELTA_Q
  // Since actual frame level loop filtering level value is not available
  // at the beginning of the tile (only available during actual filtering)
  // at encoder side.we record the delta_lf (against the frame level loop
  // filtering level) and code the delta between previous superblock's delta
  // lf and current delta lf. It is equivalent to the delta between previous
  // superblock's actual lf and current lf.
  int prev_delta_lf_from_base;
  int current_delta_lf_from_base;
#if CONFIG_LOOPFILTER_LEVEL
  // For this experiment, we have four frame filter levels for different plane
  // and direction. So, to support the per superblock update, we need to add
  // a few more params as below.
  // 0: delta loop filter level for y plane vertical
  // 1: delta loop filter level for y plane horizontal
  // 2: delta loop filter level for u plane
  // 3: delta loop filter level for v plane
  // To make it consistent with the reference to each filter level in segment,
  // we need to -1, since
  // SEG_LVL_ALT_LF_Y_V = 1;
  // SEG_LVL_ALT_LF_Y_H = 2;
  // SEG_LVL_ALT_LF_U   = 3;
  // SEG_LVL_ALT_LF_V   = 4;
  int prev_delta_lf[FRAME_LF_COUNT];
  int curr_delta_lf[FRAME_LF_COUNT];
#endif  // CONFIG_LOOPFILTER_LEVEL
#endif
#if CONFIG_ADAPT_SCAN
  const EobThresholdMD *eob_threshold_md;
#endif

#if CONFIG_COMPOUND_SEGMENT
  DECLARE_ALIGNED(16, uint8_t, seg_mask[2 * MAX_SB_SQUARE]);
#endif  // CONFIG_COMPOUND_SEGMENT

#if CONFIG_MRC_TX
  uint8_t *mrc_mask;
#endif  // CONFIG_MRC_TX

#if CONFIG_CFL
  CFL_CTX *cfl;
#endif

#if CONFIG_NCOBMC_ADAPT_WEIGHT
  uint8_t *ncobmc_pred_buf[MAX_MB_PLANE];
  int ncobmc_pred_buf_stride[MAX_MB_PLANE];
  SB_MI_BD sb_mi_bd;
#endif
} MACROBLOCKD;

static INLINE int get_bitdepth_data_path_index(const MACROBLOCKD *xd) {
  return xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH ? 1 : 0;
}

static INLINE BLOCK_SIZE get_subsize(BLOCK_SIZE bsize,
                                     PARTITION_TYPE partition) {
  if (partition == PARTITION_INVALID)
    return BLOCK_INVALID;
  else
    return subsize_lookup[partition][bsize];
}

static const TX_TYPE intra_mode_to_tx_type_context[INTRA_MODES] = {
  DCT_DCT,    // DC
  ADST_DCT,   // V
  DCT_ADST,   // H
  DCT_DCT,    // D45
  ADST_ADST,  // D135
  ADST_DCT,   // D117
  DCT_ADST,   // D153
  DCT_ADST,   // D207
  ADST_DCT,   // D63
  ADST_ADST,  // SMOOTH
#if CONFIG_SMOOTH_HV
  ADST_DCT,   // SMOOTH_V
  DCT_ADST,   // SMOOTH_H
#endif        // CONFIG_SMOOTH_HV
  ADST_ADST,  // TM
};

#if CONFIG_SUPERTX
static INLINE int supertx_enabled(const MB_MODE_INFO *mbmi) {
  TX_SIZE max_tx_size = txsize_sqr_map[mbmi->tx_size];
  return tx_size_wide[max_tx_size] >
         AOMMIN(block_size_wide[mbmi->sb_type], block_size_high[mbmi->sb_type]);
}
#endif  // CONFIG_SUPERTX

#define USE_TXTYPE_SEARCH_FOR_SUB8X8_IN_CB4X4 1

#if CONFIG_RECT_TX
static INLINE int is_rect_tx(TX_SIZE tx_size) { return tx_size >= TX_SIZES; }
#endif  // CONFIG_RECT_TX

static INLINE int block_signals_txsize(BLOCK_SIZE bsize) {
#if CONFIG_CB4X4 && (CONFIG_VAR_TX || CONFIG_EXT_TX) && CONFIG_RECT_TX
  return bsize > BLOCK_4X4;
#else
  return bsize >= BLOCK_8X8;
#endif
}

#if CONFIG_MRC_TX
#define USE_MRC_INTRA 0
#define USE_MRC_INTER 1
#define SIGNAL_MRC_MASK_INTRA (USE_MRC_INTRA && 0)
#define SIGNAL_MRC_MASK_INTER (USE_MRC_INTER && 1)
#define SIGNAL_ANY_MRC_MASK (SIGNAL_MRC_MASK_INTRA || SIGNAL_MRC_MASK_INTER)
#endif  // CONFIG_MRC_TX

#if CONFIG_EXT_TX
#define ALLOW_INTRA_EXT_TX 1

// Number of transform types in each set type
static const int av1_num_ext_tx_set[EXT_TX_SET_TYPES] = {
  1, 2,
#if CONFIG_MRC_TX
  2, 3,
#endif  // CONFIG_MRC_TX
  5, 7, 12, 16,
};

static const int av1_ext_tx_set_idx_to_type[2][AOMMAX(EXT_TX_SETS_INTRA,
                                                      EXT_TX_SETS_INTER)] = {
  {
      // Intra
      EXT_TX_SET_DCTONLY, EXT_TX_SET_DTT4_IDTX_1DDCT, EXT_TX_SET_DTT4_IDTX,
#if CONFIG_MRC_TX
      EXT_TX_SET_MRC_DCT,
#endif  // CONFIG_MRC_TX
  },
  {
      // Inter
      EXT_TX_SET_DCTONLY, EXT_TX_SET_ALL16, EXT_TX_SET_DTT9_IDTX_1DDCT,
      EXT_TX_SET_DCT_IDTX,
#if CONFIG_MRC_TX
      EXT_TX_SET_MRC_DCT_IDTX,
#endif  // CONFIG_MRC_TX
  }
};

#if CONFIG_MRC_TX
static const int av1_ext_tx_used[EXT_TX_SET_TYPES][TX_TYPES] = {
  {
      1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  },
  {
      1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0,
  },
  {
      1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
  },
  {
      1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1,
  },
  {
      1, 1, 1, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0,
  },
  {
      1, 1, 1, 1, 0, 0, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0,
  },
  {
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0,
  },
  {
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0,
  },
};
#else   // CONFIG_MRC_TX
static const int av1_ext_tx_used[EXT_TX_SET_TYPES][TX_TYPES] = {
  {
      1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  },
  {
      1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0,
  },
  {
      1, 1, 1, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0,
  },
  {
      1, 1, 1, 1, 0, 0, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0,
  },
  {
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0,
  },
  {
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  },
};
#endif  // CONFIG_MRC_TX

static INLINE TxSetType get_ext_tx_set_type(TX_SIZE tx_size, BLOCK_SIZE bs,
                                            int is_inter, int use_reduced_set) {
  const TX_SIZE tx_size_sqr_up = txsize_sqr_up_map[tx_size];
  const TX_SIZE tx_size_sqr = txsize_sqr_map[tx_size];
#if CONFIG_CB4X4 && USE_TXTYPE_SEARCH_FOR_SUB8X8_IN_CB4X4
  (void)bs;
  if (tx_size_sqr_up > TX_32X32) return EXT_TX_SET_DCTONLY;
#else
  if (tx_size_sqr_up > TX_32X32 || bs < BLOCK_8X8) return EXT_TX_SET_DCTONLY;
#endif
  if (use_reduced_set)
    return is_inter ? EXT_TX_SET_DCT_IDTX : EXT_TX_SET_DTT4_IDTX;
#if CONFIG_MRC_TX
  if (tx_size == TX_32X32) {
    if (is_inter && USE_MRC_INTER)
      return EXT_TX_SET_MRC_DCT_IDTX;
    else if (!is_inter && USE_MRC_INTRA)
      return EXT_TX_SET_MRC_DCT;
  }
#endif  // CONFIG_MRC_TX
  if (tx_size_sqr_up == TX_32X32)
    return is_inter ? EXT_TX_SET_DCT_IDTX : EXT_TX_SET_DCTONLY;
  if (is_inter)
    return (tx_size_sqr == TX_16X16 ? EXT_TX_SET_DTT9_IDTX_1DDCT
                                    : EXT_TX_SET_ALL16);
  else
    return (tx_size_sqr == TX_16X16 ? EXT_TX_SET_DTT4_IDTX
                                    : EXT_TX_SET_DTT4_IDTX_1DDCT);
}

// Maps tx set types to the indices.
static const int ext_tx_set_index[2][EXT_TX_SET_TYPES] = {
  {
      // Intra
      0, -1,
#if CONFIG_MRC_TX
      3, -1,
#endif  // CONFIG_MRC_TX
      2, 1, -1, -1,
  },
  {
      // Inter
      0, 3,
#if CONFIG_MRC_TX
      -1, 4,
#endif  // CONFIG_MRC_TX
      -1, -1, 2, 1,
  },
};

static INLINE int get_ext_tx_set(TX_SIZE tx_size, BLOCK_SIZE bs, int is_inter,
                                 int use_reduced_set) {
  const TxSetType set_type =
      get_ext_tx_set_type(tx_size, bs, is_inter, use_reduced_set);
  return ext_tx_set_index[is_inter][set_type];
}

static INLINE int get_ext_tx_types(TX_SIZE tx_size, BLOCK_SIZE bs, int is_inter,
                                   int use_reduced_set) {
  const int set_type =
      get_ext_tx_set_type(tx_size, bs, is_inter, use_reduced_set);
  return av1_num_ext_tx_set[set_type];
}

#if CONFIG_LGT_FROM_PRED
static INLINE int is_lgt_allowed(PREDICTION_MODE mode, TX_SIZE tx_size) {
  if (!LGT_FROM_PRED_INTRA && !is_inter_mode(mode)) return 0;
  if (!LGT_FROM_PRED_INTER && is_inter_mode(mode)) return 0;

  switch (mode) {
    case D45_PRED:
    case D63_PRED:
    case D117_PRED:
    case V_PRED:
#if CONFIG_SMOOTH_HV
    case SMOOTH_V_PRED:
#endif
      return tx_size_wide[tx_size] <= 8;
    case D135_PRED:
    case D153_PRED:
    case D207_PRED:
    case H_PRED:
#if CONFIG_SMOOTH_HV
    case SMOOTH_H_PRED:
#endif
      return tx_size_high[tx_size] <= 8;
    case DC_PRED:
    case SMOOTH_PRED: return 0;
    case TM_PRED:
    default: return tx_size_wide[tx_size] <= 8 || tx_size_high[tx_size] <= 8;
  }
}
#endif  // CONFIG_LGT_FROM_PRED

#if CONFIG_RECT_TX
static INLINE int is_rect_tx_allowed_bsize(BLOCK_SIZE bsize) {
  static const char LUT[BLOCK_SIZES_ALL] = {
#if CONFIG_CHROMA_2X2 || CONFIG_CHROMA_SUB8X8
    0,  // BLOCK_2X2
    0,  // BLOCK_2X4
    0,  // BLOCK_4X2
#endif
    0,  // BLOCK_4X4
    1,  // BLOCK_4X8
    1,  // BLOCK_8X4
    0,  // BLOCK_8X8
    1,  // BLOCK_8X16
    1,  // BLOCK_16X8
    0,  // BLOCK_16X16
    1,  // BLOCK_16X32
    1,  // BLOCK_32X16
    0,  // BLOCK_32X32
    1,  // BLOCK_32X64
    1,  // BLOCK_64X32
    0,  // BLOCK_64X64
#if CONFIG_EXT_PARTITION
    0,  // BLOCK_64X128
    0,  // BLOCK_128X64
    0,  // BLOCK_128X128
#endif  // CONFIG_EXT_PARTITION
    0,  // BLOCK_4X16
    0,  // BLOCK_16X4
    0,  // BLOCK_8X32
    0,  // BLOCK_32X8
    0,  // BLOCK_16X64
    0,  // BLOCK_64X16
#if CONFIG_EXT_PARTITION
    0,  // BLOCK_32X128
    0,  // BLOCK_128X32
#endif  // CONFIG_EXT_PARTITION
  };

  return LUT[bsize];
}

static INLINE int is_rect_tx_allowed(const MACROBLOCKD *xd,
                                     const MB_MODE_INFO *mbmi) {
  return is_rect_tx_allowed_bsize(mbmi->sb_type) &&
         !xd->lossless[mbmi->segment_id];
}
#endif  // CONFIG_RECT_TX
#endif  // CONFIG_EXT_TX

#if CONFIG_RECT_TX_EXT && (CONFIG_EXT_TX || CONFIG_VAR_TX)
static INLINE int is_quarter_tx_allowed_bsize(BLOCK_SIZE bsize) {
  static const char LUT_QTTX[BLOCK_SIZES_ALL] = {
#if CONFIG_CHROMA_2X2 || CONFIG_CHROMA_SUB8X8
    0,  // BLOCK_2X2
    0,  // BLOCK_2X4
    0,  // BLOCK_4X2
#endif
    0,  // BLOCK_4X4
    0,  // BLOCK_4X8
    0,  // BLOCK_8X4
    0,  // BLOCK_8X8
    1,  // BLOCK_8X16
    1,  // BLOCK_16X8
    0,  // BLOCK_16X16
    0,  // BLOCK_16X32
    0,  // BLOCK_32X16
    0,  // BLOCK_32X32
    0,  // BLOCK_32X64
    0,  // BLOCK_64X32
    0,  // BLOCK_64X64
#if CONFIG_EXT_PARTITION
    0,  // BLOCK_64X128
    0,  // BLOCK_128X64
    0,  // BLOCK_128X128
#endif  // CONFIG_EXT_PARTITION
    0,  // BLOCK_4X16
    0,  // BLOCK_16X4
    0,  // BLOCK_8X32
    0,  // BLOCK_32X8
    0,  // BLOCK_16X64
    0,  // BLOCK_64X16
#if CONFIG_EXT_PARTITION
    0,  // BLOCK_32X128
    0,  // BLOCK_128X32
#endif  // CONFIG_EXT_PARTITION
  };

  return LUT_QTTX[bsize];
}

static INLINE int is_quarter_tx_allowed(const MACROBLOCKD *xd,
                                        const MB_MODE_INFO *mbmi,
                                        int is_inter) {
  return is_quarter_tx_allowed_bsize(mbmi->sb_type) && is_inter &&
         !xd->lossless[mbmi->segment_id];
}
#endif

static INLINE TX_SIZE tx_size_from_tx_mode(BLOCK_SIZE bsize, TX_MODE tx_mode,
                                           int is_inter) {
  const TX_SIZE largest_tx_size = tx_mode_to_biggest_tx_size[tx_mode];
#if (CONFIG_VAR_TX || CONFIG_EXT_TX) && CONFIG_RECT_TX
  const TX_SIZE max_rect_tx_size = max_txsize_rect_lookup[bsize];
#else
  const TX_SIZE max_tx_size = max_txsize_lookup[bsize];
#endif  // (CONFIG_VAR_TX || CONFIG_EXT_TX) && CONFIG_RECT_TX
  (void)is_inter;
#if CONFIG_VAR_TX && CONFIG_RECT_TX
#if CONFIG_CB4X4
  if (bsize == BLOCK_4X4)
    return AOMMIN(max_txsize_lookup[bsize], largest_tx_size);
#else
  if (bsize < BLOCK_8X8)
    return AOMMIN(max_txsize_lookup[bsize], largest_tx_size);
#endif
  if (txsize_sqr_map[max_rect_tx_size] <= largest_tx_size)
    return max_rect_tx_size;
  else
    return largest_tx_size;
#elif CONFIG_EXT_TX && CONFIG_RECT_TX
  if (txsize_sqr_up_map[max_rect_tx_size] <= largest_tx_size) {
    return max_rect_tx_size;
  } else {
    return largest_tx_size;
  }
#else
  return AOMMIN(max_tx_size, largest_tx_size);
#endif  // CONFIG_VAR_TX && CONFIG_RECT_TX
}

#if CONFIG_EXT_INTRA
#define MAX_ANGLE_DELTA 3
#define ANGLE_STEP 3
extern const int16_t dr_intra_derivative[90];
static const uint8_t mode_to_angle_map[] = {
  0, 90, 180, 45, 135, 111, 157, 203, 67, 0, 0,
#if CONFIG_SMOOTH_HV
  0, 0,
#endif  // CONFIG_SMOOTH_HV
};
#if CONFIG_INTRA_INTERP
// Returns whether filter selection is needed for a given
// intra prediction angle.
int av1_is_intra_filter_switchable(int angle);
#endif  // CONFIG_INTRA_INTERP
#endif  // CONFIG_EXT_INTRA

#if CONFIG_DCT_ONLY
#define FIXED_TX_TYPE 1
#else
#define FIXED_TX_TYPE 0
#endif

// Converts block_index for given transform size to index of the block in raster
// order.
static INLINE int av1_block_index_to_raster_order(TX_SIZE tx_size,
                                                  int block_idx) {
  // For transform size 4x8, the possible block_idx values are 0 & 2, because
  // block_idx values are incremented in steps of size 'tx_width_unit x
  // tx_height_unit'. But, for this transform size, block_idx = 2 corresponds to
  // block number 1 in raster order, inside an 8x8 MI block.
  // For any other transform size, the two indices are equivalent.
  return (tx_size == TX_4X8 && block_idx == 2) ? 1 : block_idx;
}

// Inverse of above function.
// Note: only implemented for transform sizes 4x4, 4x8 and 8x4 right now.
static INLINE int av1_raster_order_to_block_index(TX_SIZE tx_size,
                                                  int raster_order) {
  assert(tx_size == TX_4X4 || tx_size == TX_4X8 || tx_size == TX_8X4);
  // We ensure that block indices are 0 & 2 if tx size is 4x8 or 8x4.
  return (tx_size == TX_4X4) ? raster_order : (raster_order > 0) ? 2 : 0;
}

static INLINE TX_TYPE get_default_tx_type(PLANE_TYPE plane_type,
                                          const MACROBLOCKD *xd, int block_idx,
                                          TX_SIZE tx_size) {
  const MB_MODE_INFO *const mbmi = &xd->mi[0]->mbmi;

  if (CONFIG_DCT_ONLY || is_inter_block(mbmi) || plane_type != PLANE_TYPE_Y ||
      xd->lossless[mbmi->segment_id] || tx_size >= TX_32X32)
    return DCT_DCT;

  return intra_mode_to_tx_type_context[plane_type == PLANE_TYPE_Y
                                           ? get_y_mode(xd->mi[0], block_idx)
                                           : get_uv_mode(mbmi->uv_mode)];
}

static INLINE TX_TYPE av1_get_tx_type(PLANE_TYPE plane_type,
                                      const MACROBLOCKD *xd, int blk_row,
                                      int blk_col, int block, TX_SIZE tx_size) {
  const MODE_INFO *const mi = xd->mi[0];
  const MB_MODE_INFO *const mbmi = &mi->mbmi;
  (void)blk_row;
  (void)blk_col;
#if CONFIG_INTRABC && (!CONFIG_EXT_TX || CONFIG_TXK_SEL)
  // TODO(aconverse@google.com): Handle INTRABC + EXT_TX + TXK_SEL
  if (is_intrabc_block(mbmi)) return DCT_DCT;
#endif  // CONFIG_INTRABC && (!CONFIG_EXT_TX || CONFIG_TXK_SEL)

#if CONFIG_TXK_SEL
  TX_TYPE tx_type;
  if (xd->lossless[mbmi->segment_id] || txsize_sqr_map[tx_size] >= TX_32X32) {
    tx_type = DCT_DCT;
  } else {
    if (plane_type == PLANE_TYPE_Y)
      tx_type = mbmi->txk_type[(blk_row << 4) + blk_col];
    else if (is_inter_block(mbmi))
      tx_type = mbmi->txk_type[(blk_row << 5) + (blk_col << 1)];
    else
      tx_type = intra_mode_to_tx_type_context[mbmi->uv_mode];
  }
  assert(tx_type >= DCT_DCT && tx_type < TX_TYPES);
  return tx_type;
#endif  // CONFIG_TXK_SEL

#if FIXED_TX_TYPE
  const int block_raster_idx = av1_block_index_to_raster_order(tx_size, block);
  return get_default_tx_type(plane_type, xd, block_raster_idx, tx_size);
#endif  // FIXED_TX_TYPE

#if CONFIG_EXT_TX
#if CONFIG_MRC_TX
  if (mbmi->tx_type == MRC_DCT) {
    assert(((is_inter_block(mbmi) && USE_MRC_INTER) ||
            (!is_inter_block(mbmi) && USE_MRC_INTRA)) &&
           "INVALID BLOCK TYPE FOR MRC_DCT");
    if (plane_type == PLANE_TYPE_Y) {
      assert(tx_size == TX_32X32);
      return mbmi->tx_type;
    }
    return DCT_DCT;
  }
#endif  // CONFIG_MRC_TX
  if (xd->lossless[mbmi->segment_id] || txsize_sqr_map[tx_size] > TX_32X32 ||
      (txsize_sqr_map[tx_size] >= TX_32X32 && !is_inter_block(mbmi)))
    return DCT_DCT;
  if (mbmi->sb_type >= BLOCK_8X8 || CONFIG_CB4X4) {
    if (plane_type == PLANE_TYPE_Y) {
#if !ALLOW_INTRA_EXT_TX
      if (is_inter_block(mbmi))
#endif  // ALLOW_INTRA_EXT_TX
        return mbmi->tx_type;
    }

    if (is_inter_block(mbmi)) {
// UV Inter only
#if CONFIG_CHROMA_2X2
      if (tx_size < TX_4X4) return DCT_DCT;
#endif
      return (mbmi->tx_type == IDTX && txsize_sqr_map[tx_size] >= TX_32X32)
                 ? DCT_DCT
                 : mbmi->tx_type;
    }
  }

#if CONFIG_CB4X4
  (void)block;
#if CONFIG_CHROMA_2X2
  if (tx_size < TX_4X4)
    return DCT_DCT;
  else
#endif  // CONFIG_CHROMA_2X2
    return intra_mode_to_tx_type_context[get_uv_mode(mbmi->uv_mode)];
#else   // CONFIG_CB4X4
  // Sub8x8-Inter/Intra OR UV-Intra
  if (is_inter_block(mbmi)) {  // Sub8x8-Inter
    return DCT_DCT;
  } else {  // Sub8x8 Intra OR UV-Intra
    const int block_raster_idx =
        av1_block_index_to_raster_order(tx_size, block);
    return intra_mode_to_tx_type_context[plane_type == PLANE_TYPE_Y
                                             ? get_y_mode(mi, block_raster_idx)
                                             : get_uv_mode(mbmi->uv_mode)];
  }
#endif  // CONFIG_CB4X4
#else   // CONFIG_EXT_TX
  (void)block;
#if CONFIG_MRC_TX
  if (mbmi->tx_type == MRC_DCT) {
    if (plane_type == PLANE_TYPE_Y && !xd->lossless[mbmi->segment_id]) {
      assert(tx_size == TX_32X32);
      return mbmi->tx_type;
    }
    return DCT_DCT;
  }
#endif  // CONFIG_MRC_TX
  if (plane_type != PLANE_TYPE_Y || xd->lossless[mbmi->segment_id] ||
      txsize_sqr_map[tx_size] >= TX_32X32)
    return DCT_DCT;
  return mbmi->tx_type;
#endif  // CONFIG_EXT_TX
}

void av1_setup_block_planes(MACROBLOCKD *xd, int ss_x, int ss_y);

static INLINE int tx_size_to_depth(TX_SIZE tx_size) {
  return (int)(tx_size - TX_SIZE_LUMA_MIN);
}

static INLINE TX_SIZE depth_to_tx_size(int depth) {
  return (TX_SIZE)(depth + TX_SIZE_LUMA_MIN);
}

static INLINE TX_SIZE av1_get_uv_tx_size(const MB_MODE_INFO *mbmi,
                                         const struct macroblockd_plane *pd) {
#if CONFIG_CHROMA_2X2
  assert(mbmi->tx_size > TX_2X2);
#endif  // CONFIG_CHROMA_2X2

#if CONFIG_SUPERTX
  if (supertx_enabled(mbmi))
    return uvsupertx_size_lookup[txsize_sqr_map[mbmi->tx_size]]
                                [pd->subsampling_x][pd->subsampling_y];
#endif  // CONFIG_SUPERTX

  const TX_SIZE uv_txsize =
      uv_txsize_lookup[mbmi->sb_type][mbmi->tx_size][pd->subsampling_x]
                      [pd->subsampling_y];
  assert(uv_txsize != TX_INVALID);
  return uv_txsize;
}

static INLINE TX_SIZE av1_get_tx_size(int plane, const MACROBLOCKD *xd) {
  const MB_MODE_INFO *mbmi = &xd->mi[0]->mbmi;
  if (plane == 0) return mbmi->tx_size;
  const MACROBLOCKD_PLANE *pd = &xd->plane[plane];
  return av1_get_uv_tx_size(mbmi, pd);
}

static INLINE BLOCK_SIZE
get_plane_block_size(BLOCK_SIZE bsize, const struct macroblockd_plane *pd) {
  return ss_size_lookup[bsize][pd->subsampling_x][pd->subsampling_y];
}

void av1_reset_skip_context(MACROBLOCKD *xd, int mi_row, int mi_col,
                            BLOCK_SIZE bsize);

typedef void (*foreach_transformed_block_visitor)(int plane, int block,
                                                  int blk_row, int blk_col,
                                                  BLOCK_SIZE plane_bsize,
                                                  TX_SIZE tx_size, void *arg);

void av1_foreach_transformed_block_in_plane(
    const MACROBLOCKD *const xd, BLOCK_SIZE bsize, int plane,
    foreach_transformed_block_visitor visit, void *arg);

#if CONFIG_LV_MAP
void av1_foreach_transformed_block(const MACROBLOCKD *const xd,
                                   BLOCK_SIZE bsize, int mi_row, int mi_col,
                                   foreach_transformed_block_visitor visit,
                                   void *arg);
#endif

#if CONFIG_COEF_INTERLEAVE
static INLINE int get_max_4x4_size(int num_4x4, int mb_to_edge,
                                   int subsampling) {
  return num_4x4 + (mb_to_edge >= 0 ? 0 : mb_to_edge >> (5 + subsampling));
}

void av1_foreach_transformed_block_interleave(
    const MACROBLOCKD *const xd, BLOCK_SIZE bsize,
    foreach_transformed_block_visitor visit, void *arg);
#endif

void av1_set_contexts(const MACROBLOCKD *xd, struct macroblockd_plane *pd,
                      int plane, TX_SIZE tx_size, int has_eob, int aoff,
                      int loff);

static INLINE int is_interintra_allowed_bsize(const BLOCK_SIZE bsize) {
#if CONFIG_INTERINTRA
  // TODO(debargha): Should this be bsize < BLOCK_LARGEST?
  return (bsize >= BLOCK_8X8) && (bsize < BLOCK_64X64);
#else
  (void)bsize;
  return 0;
#endif  // CONFIG_INTERINTRA
}

static INLINE int is_interintra_allowed_mode(const PREDICTION_MODE mode) {
#if CONFIG_INTERINTRA
  return (mode >= NEARESTMV) && (mode <= NEWMV);
#else
  (void)mode;
  return 0;
#endif  // CONFIG_INTERINTRA
}

static INLINE int is_interintra_allowed_ref(const MV_REFERENCE_FRAME rf[2]) {
#if CONFIG_INTERINTRA
  return (rf[0] > INTRA_FRAME) && (rf[1] <= INTRA_FRAME);
#else
  (void)rf;
  return 0;
#endif  // CONFIG_INTERINTRA
}

static INLINE int is_interintra_allowed(const MB_MODE_INFO *mbmi) {
  return is_interintra_allowed_bsize(mbmi->sb_type) &&
         is_interintra_allowed_mode(mbmi->mode) &&
         is_interintra_allowed_ref(mbmi->ref_frame);
}

static INLINE int is_interintra_allowed_bsize_group(int group) {
  int i;
  for (i = 0; i < BLOCK_SIZES_ALL; i++) {
    if (size_group_lookup[i] == group &&
        is_interintra_allowed_bsize((BLOCK_SIZE)i)) {
      return 1;
    }
  }
  return 0;
}

static INLINE int is_interintra_pred(const MB_MODE_INFO *mbmi) {
  return (mbmi->ref_frame[1] == INTRA_FRAME) && is_interintra_allowed(mbmi);
}

#if CONFIG_VAR_TX
static INLINE int get_vartx_max_txsize(const MB_MODE_INFO *const mbmi,
                                       BLOCK_SIZE bsize, int subsampled) {
#if CONFIG_CB4X4
  (void)mbmi;
  TX_SIZE max_txsize = max_txsize_rect_lookup[bsize];
#else
  TX_SIZE max_txsize = mbmi->sb_type < BLOCK_8X8
                           ? max_txsize_rect_lookup[mbmi->sb_type]
                           : max_txsize_rect_lookup[bsize];
#endif  // CONFIG_C4X4

#if CONFIG_EXT_PARTITION && CONFIG_TX64X64
  // The decoder is designed so that it can process 64x64 luma pixels at a
  // time. If this is a chroma plane with subsampling and bsize corresponds to
  // a subsampled BLOCK_128X128 then the lookup above will give TX_64X64. That
  // mustn't be used for the subsampled plane (because it would be bigger than
  // a 64x64 luma block) so we round down to TX_32X32.
  if (subsampled && max_txsize == TX_64X64) max_txsize = TX_32X32;
#else
  (void)subsampled;
#endif

  return max_txsize;
}
#endif  // CONFIG_VAR_TX

#if CONFIG_MOTION_VAR || CONFIG_WARPED_MOTION
static INLINE int is_motion_variation_allowed_bsize(BLOCK_SIZE bsize) {
  return AOMMIN(block_size_wide[bsize], block_size_high[bsize]) >= 8;
}

static INLINE int is_motion_variation_allowed_compound(
    const MB_MODE_INFO *mbmi) {
#if CONFIG_COMPOUND_SINGLEREF
  if (!has_second_ref(mbmi) && !is_inter_singleref_comp_mode(mbmi->mode))
#else
  if (!has_second_ref(mbmi))
#endif  // CONFIG_COMPOUND_SINGLEREF
    return 1;
  else
    return 0;
}

#if CONFIG_MOTION_VAR
// input: log2 of length, 0(4), 1(8), ...
static const int max_neighbor_obmc[6] = { 0, 1, 2, 3, 4, 4 };

static INLINE int check_num_overlappable_neighbors(const MB_MODE_INFO *mbmi) {
  return !(mbmi->overlappable_neighbors[0] == 0 &&
           mbmi->overlappable_neighbors[1] == 0);
}
#if CONFIG_NCOBMC_ADAPT_WEIGHT
static INLINE NCOBMC_MODE ncobmc_mode_allowed_bsize(BLOCK_SIZE bsize) {
  if (bsize < BLOCK_8X8 || bsize >= BLOCK_64X64)
    return NO_OVERLAP;
  else
    return MAX_NCOBMC_MODES;
}
#endif  // CONFIG_NCOBMC_ADAPT_WEIGHT
#endif  // CONFIG_MOTION_VAR

static INLINE MOTION_MODE motion_mode_allowed(
#if CONFIG_GLOBAL_MOTION
    int block, const WarpedMotionParams *gm_params,
#endif  // CONFIG_GLOBAL_MOTION
#if CONFIG_WARPED_MOTION
    const MACROBLOCKD *xd,
#endif
    const MODE_INFO *mi) {
  const MB_MODE_INFO *mbmi = &mi->mbmi;
#if CONFIG_AMVR
  if (xd->cur_frame_mv_precision_level == 0) {
#endif
#if CONFIG_GLOBAL_MOTION
    const TransformationType gm_type = gm_params[mbmi->ref_frame[0]].wmtype;
    if (is_global_mv_block(mi, block, gm_type)) return SIMPLE_TRANSLATION;
#endif  // CONFIG_GLOBAL_MOTION
#if CONFIG_AMVR
  }
#endif
  if (is_motion_variation_allowed_bsize(mbmi->sb_type) &&
      is_inter_mode(mbmi->mode) && mbmi->ref_frame[1] != INTRA_FRAME &&
      is_motion_variation_allowed_compound(mbmi)) {
#if CONFIG_MOTION_VAR
    if (!check_num_overlappable_neighbors(mbmi)) return SIMPLE_TRANSLATION;
#endif
#if CONFIG_WARPED_MOTION
    if (!has_second_ref(mbmi) && mbmi->num_proj_ref[0] >= 1 &&
        !av1_is_scaled(&(xd->block_refs[0]->sf))) {
#if CONFIG_AMVR
      if (xd->cur_frame_mv_precision_level) {
        return OBMC_CAUSAL;
      }
#endif
      return WARPED_CAUSAL;
    }

#endif  // CONFIG_WARPED_MOTION
#if CONFIG_MOTION_VAR
#if CONFIG_NCOBMC_ADAPT_WEIGHT
    if (ncobmc_mode_allowed_bsize(mbmi->sb_type) < NO_OVERLAP)
      return NCOBMC_ADAPT_WEIGHT;
    else
#endif
      return OBMC_CAUSAL;
#else
    return SIMPLE_TRANSLATION;
#endif  // CONFIG_MOTION_VAR
  } else {
    return SIMPLE_TRANSLATION;
  }
}

static INLINE void assert_motion_mode_valid(MOTION_MODE mode,
#if CONFIG_GLOBAL_MOTION
                                            int block,
                                            const WarpedMotionParams *gm_params,
#endif  // CONFIG_GLOBAL_MOTION
#if CONFIG_WARPED_MOTION
                                            const MACROBLOCKD *xd,
#endif
                                            const MODE_INFO *mi) {
  const MOTION_MODE last_motion_mode_allowed = motion_mode_allowed(
#if CONFIG_GLOBAL_MOTION
      block, gm_params,
#endif  // CONFIG_GLOBAL_MOTION
#if CONFIG_WARPED_MOTION
      xd,
#endif
      mi);

  // Check that the input mode is not illegal
  if (last_motion_mode_allowed < mode)
    assert(0 && "Illegal motion mode selected");
}

#if CONFIG_MOTION_VAR
static INLINE int is_neighbor_overlappable(const MB_MODE_INFO *mbmi) {
  return (is_inter_block(mbmi));
}
#endif  // CONFIG_MOTION_VAR
#endif  // CONFIG_MOTION_VAR || CONFIG_WARPED_MOTION

static INLINE int av1_allow_palette(int allow_screen_content_tools,
                                    BLOCK_SIZE sb_type) {
  return allow_screen_content_tools && sb_type >= BLOCK_8X8 &&
         sb_type <= BLOCK_LARGEST;
}

// Returns sub-sampled dimensions of the given block.
// The output values for 'rows_within_bounds' and 'cols_within_bounds' will
// differ from 'height' and 'width' when part of the block is outside the
// right
// and/or bottom image boundary.
static INLINE void av1_get_block_dimensions(BLOCK_SIZE bsize, int plane,
                                            const MACROBLOCKD *xd, int *width,
                                            int *height,
                                            int *rows_within_bounds,
                                            int *cols_within_bounds) {
  const int block_height = block_size_high[bsize];
  const int block_width = block_size_wide[bsize];
  const int block_rows = (xd->mb_to_bottom_edge >= 0)
                             ? block_height
                             : (xd->mb_to_bottom_edge >> 3) + block_height;
  const int block_cols = (xd->mb_to_right_edge >= 0)
                             ? block_width
                             : (xd->mb_to_right_edge >> 3) + block_width;
  const struct macroblockd_plane *const pd = &xd->plane[plane];
  assert(IMPLIES(plane == PLANE_TYPE_Y, pd->subsampling_x == 0));
  assert(IMPLIES(plane == PLANE_TYPE_Y, pd->subsampling_y == 0));
  assert(block_width >= block_cols);
  assert(block_height >= block_rows);
  if (width) *width = block_width >> pd->subsampling_x;
  if (height) *height = block_height >> pd->subsampling_y;
  if (rows_within_bounds) *rows_within_bounds = block_rows >> pd->subsampling_y;
  if (cols_within_bounds) *cols_within_bounds = block_cols >> pd->subsampling_x;
}

/* clang-format off */
typedef aom_cdf_prob (*MapCdf)[PALETTE_COLOR_INDEX_CONTEXTS]
                              [CDF_SIZE(PALETTE_COLORS)];
typedef const int (*ColorCost)[PALETTE_SIZES][PALETTE_COLOR_INDEX_CONTEXTS]
                              [PALETTE_COLORS];
/* clang-format on */

typedef struct {
  int rows;
  int cols;
  int n_colors;
  int plane_width;
  int plane_height;
  uint8_t *color_map;
  MapCdf map_cdf;
  ColorCost color_cost;
} Av1ColorMapParam;

#if CONFIG_GLOBAL_MOTION
static INLINE int is_nontrans_global_motion(const MACROBLOCKD *xd) {
  const MODE_INFO *mi = xd->mi[0];
  const MB_MODE_INFO *const mbmi = &mi->mbmi;
  int ref;
#if CONFIG_CB4X4
  const int unify_bsize = 1;
#else
  const int unify_bsize = 0;
#endif

  // First check if all modes are ZEROMV
  if (mbmi->sb_type >= BLOCK_8X8 || unify_bsize) {
    if (mbmi->mode != ZEROMV && mbmi->mode != ZERO_ZEROMV) return 0;
  } else {
    if ((mi->bmi[0].as_mode != ZEROMV && mi->bmi[0].as_mode != ZERO_ZEROMV) ||
        (mi->bmi[1].as_mode != ZEROMV && mi->bmi[1].as_mode != ZERO_ZEROMV) ||
        (mi->bmi[2].as_mode != ZEROMV && mi->bmi[2].as_mode != ZERO_ZEROMV) ||
        (mi->bmi[3].as_mode != ZEROMV && mi->bmi[3].as_mode != ZERO_ZEROMV))
      return 0;
  }

#if !GLOBAL_SUB8X8_USED
  if (mbmi->sb_type < BLOCK_8X8) return 0;
#endif

  // Now check if all global motion is non translational
  for (ref = 0; ref < 1 + has_second_ref(mbmi); ++ref) {
    if (xd->global_motion[mbmi->ref_frame[ref]].wmtype <= TRANSLATION) return 0;
  }
  return 1;
}
#endif  // CONFIG_GLOBAL_MOTION

static INLINE PLANE_TYPE get_plane_type(int plane) {
  return (plane == 0) ? PLANE_TYPE_Y : PLANE_TYPE_UV;
}

static INLINE void transpose_uint8(uint8_t *dst, int dst_stride,
                                   const uint8_t *src, int src_stride, int w,
                                   int h) {
  int r, c;
  for (r = 0; r < h; ++r)
    for (c = 0; c < w; ++c) dst[c * dst_stride + r] = src[r * src_stride + c];
}

static INLINE void transpose_uint16(uint16_t *dst, int dst_stride,
                                    const uint16_t *src, int src_stride, int w,
                                    int h) {
  int r, c;
  for (r = 0; r < h; ++r)
    for (c = 0; c < w; ++c) dst[c * dst_stride + r] = src[r * src_stride + c];
}

static INLINE void transpose_int16(int16_t *dst, int dst_stride,
                                   const int16_t *src, int src_stride, int w,
                                   int h) {
  int r, c;
  for (r = 0; r < h; ++r)
    for (c = 0; c < w; ++c) dst[c * dst_stride + r] = src[r * src_stride + c];
}

static INLINE void transpose_int32(int32_t *dst, int dst_stride,
                                   const int32_t *src, int src_stride, int w,
                                   int h) {
  int r, c;
  for (r = 0; r < h; ++r)
    for (c = 0; c < w; ++c) dst[c * dst_stride + r] = src[r * src_stride + c];
}

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // AV1_COMMON_BLOCKD_H_
