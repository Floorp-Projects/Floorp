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

#ifndef AV1_ENCODER_BLOCK_H_
#define AV1_ENCODER_BLOCK_H_

#include "av1/common/entropymv.h"
#include "av1/common/entropy.h"
#include "av1/common/mvref_common.h"
#include "av1/encoder/hash.h"
#if CONFIG_DIST_8X8
#include "aom/aomcx.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  unsigned int sse;
  int sum;
  unsigned int var;
} DIFF;

typedef struct macroblock_plane {
  DECLARE_ALIGNED(16, int16_t, src_diff[MAX_SB_SQUARE]);
  tran_low_t *qcoeff;
  tran_low_t *coeff;
  uint16_t *eobs;
  uint8_t *txb_entropy_ctx;
  struct buf_2d src;

  // Quantizer setings
  // These are used/accessed only in the quantization process
  // RDO does not / must not depend on any of these values
  // All values below share the coefficient scale/shift used in TX
  const int16_t *quant_fp_QTX;
  const int16_t *round_fp_QTX;
  const int16_t *quant_QTX;
  const int16_t *quant_shift_QTX;
  const int16_t *zbin_QTX;
  const int16_t *round_QTX;
  const int16_t *dequant_QTX;
} MACROBLOCK_PLANE;

typedef struct {
  int txb_skip_cost[TXB_SKIP_CONTEXTS][2];
  int base_eob_cost[SIG_COEF_CONTEXTS_EOB][3];
  int base_cost[SIG_COEF_CONTEXTS][4];
  int eob_extra_cost[EOB_COEF_CONTEXTS][2];
  int dc_sign_cost[DC_SIGN_CONTEXTS][2];
  int lps_cost[LEVEL_CONTEXTS][COEFF_BASE_RANGE + 1];
} LV_MAP_COEFF_COST;

typedef struct {
  int eob_cost[2][11];
} LV_MAP_EOB_COST;

typedef struct {
  tran_low_t tcoeff[MAX_MB_PLANE][MAX_SB_SQUARE];
  uint16_t eobs[MAX_MB_PLANE][MAX_SB_SQUARE / (TX_SIZE_W_MIN * TX_SIZE_H_MIN)];
  uint8_t txb_skip_ctx[MAX_MB_PLANE]
                      [MAX_SB_SQUARE / (TX_SIZE_W_MIN * TX_SIZE_H_MIN)];
  int dc_sign_ctx[MAX_MB_PLANE]
                 [MAX_SB_SQUARE / (TX_SIZE_W_MIN * TX_SIZE_H_MIN)];
} CB_COEFF_BUFFER;

typedef struct {
  int16_t mode_context[MODE_CTX_REF_FRAMES];
  // TODO(angiebird): Reduce the buffer size according to sb_type
  tran_low_t *tcoeff[MAX_MB_PLANE];
  uint16_t *eobs[MAX_MB_PLANE];
  uint8_t *txb_skip_ctx[MAX_MB_PLANE];
  int *dc_sign_ctx[MAX_MB_PLANE];
  uint8_t ref_mv_count[MODE_CTX_REF_FRAMES];
  CANDIDATE_MV ref_mv_stack[MODE_CTX_REF_FRAMES][MAX_REF_MV_STACK_SIZE];
  int_mv global_mvs[REF_FRAMES];
  int16_t compound_mode_context[MODE_CTX_REF_FRAMES];
} MB_MODE_INFO_EXT;

typedef struct {
  int col_min;
  int col_max;
  int row_min;
  int row_max;
} MvLimits;

typedef struct {
  uint8_t best_palette_color_map[MAX_PALETTE_SQUARE];
  int kmeans_data_buf[2 * MAX_PALETTE_SQUARE];
} PALETTE_BUFFER;

typedef struct {
  TX_SIZE tx_size;
  TX_SIZE inter_tx_size[INTER_TX_SIZE_BUF_LEN];
  uint8_t blk_skip[MAX_MIB_SIZE * MAX_MIB_SIZE];
  TX_TYPE txk_type[TXK_TYPE_BUF_LEN];
  RD_STATS rd_stats;
  uint32_t hash_value;
} MB_RD_INFO;

#define RD_RECORD_BUFFER_LEN 8
typedef struct {
  MB_RD_INFO tx_rd_info[RD_RECORD_BUFFER_LEN];  // Circular buffer.
  int index_start;
  int num;
  CRC32C crc_calculator;  // Hash function.
} MB_RD_RECORD;

typedef struct {
  int64_t dist;
  int64_t sse;
  int rate;
  uint16_t eob;
  TX_TYPE tx_type;
  uint16_t entropy_context;
  uint8_t txb_entropy_ctx;
  uint8_t valid;
  uint8_t fast;  // This is not being used now.
} TXB_RD_INFO;

#define TX_SIZE_RD_RECORD_BUFFER_LEN 256
typedef struct {
  uint32_t hash_vals[TX_SIZE_RD_RECORD_BUFFER_LEN];
  TXB_RD_INFO tx_rd_info[TX_SIZE_RD_RECORD_BUFFER_LEN];
  int index_start;
  int num;
} TXB_RD_RECORD;

typedef struct tx_size_rd_info_node {
  TXB_RD_INFO *rd_info_array;  // Points to array of size TX_TYPES.
  struct tx_size_rd_info_node *children[4];
} TXB_RD_INFO_NODE;

// Region size for mode decision sampling in the first pass of partition
// search(two_pass_partition_search speed feature), in units of mi size(4).
// Used by the mode_pruning_based_on_two_pass_partition_search speed feature.
#define FIRST_PARTITION_PASS_SAMPLE_REGION 8
#define FIRST_PARTITION_PASS_SAMPLE_REGION_LOG2 3
#define FIRST_PARTITION_PASS_STATS_TABLES                     \
  (MAX_MIB_SIZE >> FIRST_PARTITION_PASS_SAMPLE_REGION_LOG2) * \
      (MAX_MIB_SIZE >> FIRST_PARTITION_PASS_SAMPLE_REGION_LOG2)
#define FIRST_PARTITION_PASS_STATS_STRIDE \
  (MAX_MIB_SIZE_LOG2 - FIRST_PARTITION_PASS_SAMPLE_REGION_LOG2)

static INLINE int av1_first_partition_pass_stats_index(int mi_row, int mi_col) {
  const int row =
      (mi_row & MAX_MIB_MASK) >> FIRST_PARTITION_PASS_SAMPLE_REGION_LOG2;
  const int col =
      (mi_col & MAX_MIB_MASK) >> FIRST_PARTITION_PASS_SAMPLE_REGION_LOG2;
  return (row << FIRST_PARTITION_PASS_STATS_STRIDE) + col;
}

typedef struct {
  uint8_t ref0_counts[REF_FRAMES];  // Counters for ref_frame[0].
  uint8_t ref1_counts[REF_FRAMES];  // Counters for ref_frame[1].
  int sample_counts;                // Number of samples collected.
} FIRST_PARTITION_PASS_STATS;

#define MAX_INTERP_FILTER_STATS 64
typedef struct {
  InterpFilters filters;
  int_mv mv[2];
  int8_t ref_frames[2];
} INTERPOLATION_FILTER_STATS;

typedef struct macroblock MACROBLOCK;
struct macroblock {
  struct macroblock_plane plane[MAX_MB_PLANE];

  // Determine if one would go with reduced complexity transform block
  // search model to select prediction modes, or full complexity model
  // to select transform kernel.
  int rd_model;

  // Indicate if the encoder is running in the first pass partition search.
  // In that case, apply certain speed features therein to reduce the overhead
  // cost in the first pass search.
  int cb_partition_scan;

  FIRST_PARTITION_PASS_STATS
  first_partition_pass_stats[FIRST_PARTITION_PASS_STATS_TABLES];

  // [comp_idx][saved stat_idx]
  INTERPOLATION_FILTER_STATS interp_filter_stats[2][MAX_INTERP_FILTER_STATS];
  int interp_filter_stats_idx[2];

  // Activate constrained coding block partition search range.
  int use_cb_search_range;

  // Inter macroblock RD search info.
  MB_RD_RECORD mb_rd_record;

  // Inter transform block RD search info. for square TX sizes.
  TXB_RD_RECORD txb_rd_record_8X8[(MAX_MIB_SIZE >> 1) * (MAX_MIB_SIZE >> 1)];
  TXB_RD_RECORD txb_rd_record_16X16[(MAX_MIB_SIZE >> 2) * (MAX_MIB_SIZE >> 2)];
  TXB_RD_RECORD txb_rd_record_32X32[(MAX_MIB_SIZE >> 3) * (MAX_MIB_SIZE >> 3)];
  TXB_RD_RECORD txb_rd_record_64X64[(MAX_MIB_SIZE >> 4) * (MAX_MIB_SIZE >> 4)];

  // Intra transform block RD search info. for square TX sizes.
  TXB_RD_RECORD txb_rd_record_intra;

  MACROBLOCKD e_mbd;
  MB_MODE_INFO_EXT *mbmi_ext;
  int skip_block;
  int qindex;

  // The equivalent error at the current rdmult of one whole bit (not one
  // bitcost unit).
  int errorperbit;
  // The equivalend SAD error of one (whole) bit at the current quantizer
  // for large blocks.
  int sadperbit16;
  // The equivalend SAD error of one (whole) bit at the current quantizer
  // for sub-8x8 blocks.
  int sadperbit4;
  int rdmult;
  int mb_energy;
  int *m_search_count_ptr;
  int *ex_search_count_ptr;

  unsigned int txb_split_count;

  // These are set to their default values at the beginning, and then adjusted
  // further in the encoding process.
  BLOCK_SIZE min_partition_size;
  BLOCK_SIZE max_partition_size;

  unsigned int max_mv_context[REF_FRAMES];
  unsigned int source_variance;
  unsigned int pred_sse[REF_FRAMES];
  int pred_mv_sad[REF_FRAMES];

  int *nmvjointcost;
  int nmv_vec_cost[MV_JOINTS];
  int *nmvcost[2];
  int *nmvcost_hp[2];
  int **mv_cost_stack;
  int **mvcost;

  int32_t *wsrc_buf;
  int32_t *mask_buf;
  uint8_t *above_pred_buf;
  uint8_t *left_pred_buf;

  PALETTE_BUFFER *palette_buffer;

  // These define limits to motion vector components to prevent them
  // from extending outside the UMV borders
  MvLimits mv_limits;

  uint8_t blk_skip[MAX_MIB_SIZE * MAX_MIB_SIZE];
  uint8_t blk_skip_drl[MAX_MIB_SIZE * MAX_MIB_SIZE];

  int skip;
  int skip_chroma_rd;
  int skip_cost[SKIP_CONTEXTS][2];

  int skip_mode;  // 0: off; 1: on
  int skip_mode_cost[SKIP_CONTEXTS][2];

  int compound_idx;

  LV_MAP_COEFF_COST coeff_costs[TX_SIZES][PLANE_TYPES];
  LV_MAP_EOB_COST eob_costs[7][2];
  uint16_t cb_offset;

  // mode costs
  int intra_inter_cost[INTRA_INTER_CONTEXTS][2];

  int mbmode_cost[BLOCK_SIZE_GROUPS][INTRA_MODES];
  int newmv_mode_cost[NEWMV_MODE_CONTEXTS][2];
  int zeromv_mode_cost[GLOBALMV_MODE_CONTEXTS][2];
  int refmv_mode_cost[REFMV_MODE_CONTEXTS][2];
  int drl_mode_cost0[DRL_MODE_CONTEXTS][2];

  int comp_inter_cost[COMP_INTER_CONTEXTS][2];
  int single_ref_cost[REF_CONTEXTS][SINGLE_REFS - 1][2];
  int comp_ref_type_cost[COMP_REF_TYPE_CONTEXTS]
                        [CDF_SIZE(COMP_REFERENCE_TYPES)];
  int uni_comp_ref_cost[UNI_COMP_REF_CONTEXTS][UNIDIR_COMP_REFS - 1]
                       [CDF_SIZE(2)];
  // Cost for signaling ref_frame[0] (LAST_FRAME, LAST2_FRAME, LAST3_FRAME or
  // GOLDEN_FRAME) in bidir-comp mode.
  int comp_ref_cost[REF_CONTEXTS][FWD_REFS - 1][2];
  // Cost for signaling ref_frame[1] (ALTREF_FRAME, ALTREF2_FRAME, or
  // BWDREF_FRAME) in bidir-comp mode.
  int comp_bwdref_cost[REF_CONTEXTS][BWD_REFS - 1][2];
  int inter_compound_mode_cost[INTER_MODE_CONTEXTS][INTER_COMPOUND_MODES];
  int compound_type_cost[BLOCK_SIZES_ALL][COMPOUND_TYPES - 1];
  int wedge_idx_cost[BLOCK_SIZES_ALL][16];
  int interintra_cost[BLOCK_SIZE_GROUPS][2];
  int wedge_interintra_cost[BLOCK_SIZES_ALL][2];
  int interintra_mode_cost[BLOCK_SIZE_GROUPS][INTERINTRA_MODES];
  int motion_mode_cost[BLOCK_SIZES_ALL][MOTION_MODES];
  int motion_mode_cost1[BLOCK_SIZES_ALL][2];
  int intra_uv_mode_cost[CFL_ALLOWED_TYPES][INTRA_MODES][UV_INTRA_MODES];
  int y_mode_costs[INTRA_MODES][INTRA_MODES][INTRA_MODES];
  int filter_intra_cost[BLOCK_SIZES_ALL][2];
  int filter_intra_mode_cost[FILTER_INTRA_MODES];
  int switchable_interp_costs[SWITCHABLE_FILTER_CONTEXTS][SWITCHABLE_FILTERS];
  int partition_cost[PARTITION_CONTEXTS][EXT_PARTITION_TYPES];
  int palette_y_size_cost[PALATTE_BSIZE_CTXS][PALETTE_SIZES];
  int palette_uv_size_cost[PALATTE_BSIZE_CTXS][PALETTE_SIZES];
  int palette_y_color_cost[PALETTE_SIZES][PALETTE_COLOR_INDEX_CONTEXTS]
                          [PALETTE_COLORS];
  int palette_uv_color_cost[PALETTE_SIZES][PALETTE_COLOR_INDEX_CONTEXTS]
                           [PALETTE_COLORS];
  int palette_y_mode_cost[PALATTE_BSIZE_CTXS][PALETTE_Y_MODE_CONTEXTS][2];
  int palette_uv_mode_cost[PALETTE_UV_MODE_CONTEXTS][2];
  // The rate associated with each alpha codeword
  int cfl_cost[CFL_JOINT_SIGNS][CFL_PRED_PLANES][CFL_ALPHABET_SIZE];
  int tx_size_cost[TX_SIZES - 1][TX_SIZE_CONTEXTS][TX_SIZES];
  int txfm_partition_cost[TXFM_PARTITION_CONTEXTS][2];
  int inter_tx_type_costs[EXT_TX_SETS_INTER][EXT_TX_SIZES][TX_TYPES];
  int intra_tx_type_costs[EXT_TX_SETS_INTRA][EXT_TX_SIZES][INTRA_MODES]
                         [TX_TYPES];
  int angle_delta_cost[DIRECTIONAL_MODES][2 * MAX_ANGLE_DELTA + 1];
  int switchable_restore_cost[RESTORE_SWITCHABLE_TYPES];
  int wiener_restore_cost[2];
  int sgrproj_restore_cost[2];
  int intrabc_cost[2];

  // Used to store sub partition's choices.
  MV pred_mv[REF_FRAMES];

  // Store the best motion vector during motion search
  int_mv best_mv;
  // Store the second best motion vector during full-pixel motion search
  int_mv second_best_mv;

  // use default transform and skip transform type search for intra modes
  int use_default_intra_tx_type;
  // use default transform and skip transform type search for inter modes
  int use_default_inter_tx_type;
#if CONFIG_DIST_8X8
  int using_dist_8x8;
  aom_tune_metric tune_metric;
  DECLARE_ALIGNED(16, int16_t, pred_luma[MAX_SB_SQUARE]);
#endif  // CONFIG_DIST_8X8
  int comp_idx_cost[COMP_INDEX_CONTEXTS][2];
  int comp_group_idx_cost[COMP_GROUP_IDX_CONTEXTS][2];
  // Bit flags for pruning tx type search, tx split, etc.
  int tx_search_prune[EXT_TX_SET_TYPES];
  int must_find_valid_partition;
  int tx_split_prune_flag;  // Flag to skip tx split RD search.
};

static INLINE int is_rect_tx_allowed_bsize(BLOCK_SIZE bsize) {
  static const char LUT[BLOCK_SIZES_ALL] = {
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
    0,  // BLOCK_64X128
    0,  // BLOCK_128X64
    0,  // BLOCK_128X128
    1,  // BLOCK_4X16
    1,  // BLOCK_16X4
    1,  // BLOCK_8X32
    1,  // BLOCK_32X8
    1,  // BLOCK_16X64
    1,  // BLOCK_64X16
  };

  return LUT[bsize];
}

static INLINE int is_rect_tx_allowed(const MACROBLOCKD *xd,
                                     const MB_MODE_INFO *mbmi) {
  return is_rect_tx_allowed_bsize(mbmi->sb_type) &&
         !xd->lossless[mbmi->segment_id];
}

static INLINE int tx_size_to_depth(TX_SIZE tx_size, BLOCK_SIZE bsize) {
  TX_SIZE ctx_size = max_txsize_rect_lookup[bsize];
  int depth = 0;
  while (tx_size != ctx_size) {
    depth++;
    ctx_size = sub_tx_size_map[ctx_size];
    assert(depth <= MAX_TX_DEPTH);
  }
  return depth;
}

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // AV1_ENCODER_BLOCK_H_
