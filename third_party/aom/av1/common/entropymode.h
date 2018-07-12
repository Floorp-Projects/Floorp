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

#ifndef AV1_COMMON_ENTROPYMODE_H_
#define AV1_COMMON_ENTROPYMODE_H_

#include "av1/common/entropy.h"
#include "av1/common/entropymv.h"
#include "av1/common/filter.h"
#include "av1/common/seg_common.h"
#include "aom_dsp/aom_filter.h"

#if CONFIG_PVQ
#include "av1/common/pvq.h"
#include "av1/common/pvq_state.h"
#include "av1/common/generic_code.h"
#endif  // CONFIG_PVQ

#ifdef __cplusplus
extern "C" {
#endif

#define BLOCK_SIZE_GROUPS 4

#define TX_SIZE_CONTEXTS 2

#define INTER_OFFSET(mode) ((mode)-NEARESTMV)
#if CONFIG_COMPOUND_SINGLEREF
#define INTER_SINGLEREF_COMP_OFFSET(mode) ((mode)-SR_NEAREST_NEARMV)
#endif  // CONFIG_COMPOUND_SINGLEREF
#define INTER_COMPOUND_OFFSET(mode) ((mode)-NEAREST_NEARESTMV)

// Number of possible contexts for a color index.
// As can be seen from av1_get_palette_color_index_context(), the possible
// contexts are (2,0,0), (2,2,1), (3,2,0), (4,1,0), (5,0,0). These are mapped to
// a value from 0 to 4 using 'palette_color_index_context_lookup' table.
#define PALETTE_COLOR_INDEX_CONTEXTS 5

// Maximum number of colors in a palette.
#define PALETTE_MAX_SIZE 8
// Minimum number of colors in a palette.
#define PALETTE_MIN_SIZE 2

// Palette mode is available for block sizes >= 8x8.
#define PALETTE_BLOCK_SIZES (BLOCK_LARGEST - BLOCK_8X8 + 1)

// Palette Y mode context for a block is determined by number of neighboring
// blocks (top and/or left) using a palette for Y plane. So, possible Y mode'
// context values are:
// 0 if neither left nor top block uses palette for Y plane,
// 1 if exactly one of left or top block uses palette for Y plane, and
// 2 if both left and top blocks use palette for Y plane.
#define PALETTE_Y_MODE_CONTEXTS 3

// Palette UV mode context for a block is determined by whether this block uses
// palette for the Y plane. So, possible values are:
// 0 if this block doesn't use palette for Y plane.
// 1 if this block uses palette for Y plane (i.e. Y palette size > 0).
#define PALETTE_UV_MODE_CONTEXTS 2

#define PALETTE_MAX_BLOCK_SIZE (64 * 64)

#if CONFIG_KF_CTX
#define KF_MODE_CONTEXTS 5
#endif

struct AV1Common;

typedef struct {
  const int16_t *scan;
  const int16_t *iscan;
  const int16_t *neighbors;
} SCAN_ORDER;

struct seg_counts {
  unsigned int tree_total[MAX_SEGMENTS];
  unsigned int tree_mispred[MAX_SEGMENTS];
  unsigned int pred[PREDICTION_PROBS][2];
};

typedef struct frame_contexts {
  aom_prob y_mode_prob[BLOCK_SIZE_GROUPS][INTRA_MODES - 1];
  aom_prob uv_mode_prob[INTRA_MODES][UV_INTRA_MODES - 1];
#if CONFIG_EXT_PARTITION_TYPES
  aom_prob partition_prob[PARTITION_CONTEXTS][EXT_PARTITION_TYPES - 1];
#else
  aom_prob partition_prob[PARTITION_CONTEXTS][PARTITION_TYPES - 1];
#endif
  coeff_cdf_model coef_tail_cdfs[TX_SIZES][PLANE_TYPES];
  coeff_cdf_model coef_head_cdfs[TX_SIZES][PLANE_TYPES];
#if CONFIG_ADAPT_SCAN
// TODO(angiebird): try aom_prob
#if CONFIG_CHROMA_2X2
  uint32_t non_zero_prob_2x2[TX_TYPES][4];
#endif
  uint32_t non_zero_prob_4X4[TX_TYPES][16];
  uint32_t non_zero_prob_8X8[TX_TYPES][64];
  uint32_t non_zero_prob_16X16[TX_TYPES][256];
  uint32_t non_zero_prob_32X32[TX_TYPES][1024];

  uint32_t non_zero_prob_4X8[TX_TYPES][32];
  uint32_t non_zero_prob_8X4[TX_TYPES][32];
  uint32_t non_zero_prob_16X8[TX_TYPES][128];
  uint32_t non_zero_prob_8X16[TX_TYPES][128];
  uint32_t non_zero_prob_32X16[TX_TYPES][512];
  uint32_t non_zero_prob_16X32[TX_TYPES][512];

#if CONFIG_CHROMA_2X2
  DECLARE_ALIGNED(16, int16_t, scan_2x2[TX_TYPES][4]);
#endif
  DECLARE_ALIGNED(16, int16_t, scan_4X4[TX_TYPES][16]);
  DECLARE_ALIGNED(16, int16_t, scan_8X8[TX_TYPES][64]);
  DECLARE_ALIGNED(16, int16_t, scan_16X16[TX_TYPES][256]);
  DECLARE_ALIGNED(16, int16_t, scan_32X32[TX_TYPES][1024]);

  DECLARE_ALIGNED(16, int16_t, scan_4X8[TX_TYPES][32]);
  DECLARE_ALIGNED(16, int16_t, scan_8X4[TX_TYPES][32]);
  DECLARE_ALIGNED(16, int16_t, scan_8X16[TX_TYPES][128]);
  DECLARE_ALIGNED(16, int16_t, scan_16X8[TX_TYPES][128]);
  DECLARE_ALIGNED(16, int16_t, scan_16X32[TX_TYPES][512]);
  DECLARE_ALIGNED(16, int16_t, scan_32X16[TX_TYPES][512]);

#if CONFIG_CHROMA_2X2
  DECLARE_ALIGNED(16, int16_t, iscan_2x2[TX_TYPES][4]);
#endif
  DECLARE_ALIGNED(16, int16_t, iscan_4X4[TX_TYPES][16]);
  DECLARE_ALIGNED(16, int16_t, iscan_8X8[TX_TYPES][64]);
  DECLARE_ALIGNED(16, int16_t, iscan_16X16[TX_TYPES][256]);
  DECLARE_ALIGNED(16, int16_t, iscan_32X32[TX_TYPES][1024]);

  DECLARE_ALIGNED(16, int16_t, iscan_4X8[TX_TYPES][32]);
  DECLARE_ALIGNED(16, int16_t, iscan_8X4[TX_TYPES][32]);
  DECLARE_ALIGNED(16, int16_t, iscan_8X16[TX_TYPES][128]);
  DECLARE_ALIGNED(16, int16_t, iscan_16X8[TX_TYPES][128]);
  DECLARE_ALIGNED(16, int16_t, iscan_16X32[TX_TYPES][512]);
  DECLARE_ALIGNED(16, int16_t, iscan_32X16[TX_TYPES][512]);

#if CONFIG_CHROMA_2X2
  int16_t nb_2x2[TX_TYPES][(4 + 1) * 2];
#endif
  int16_t nb_4X4[TX_TYPES][(16 + 1) * 2];
  int16_t nb_8X8[TX_TYPES][(64 + 1) * 2];
  int16_t nb_16X16[TX_TYPES][(256 + 1) * 2];
  int16_t nb_32X32[TX_TYPES][(1024 + 1) * 2];

  int16_t nb_4X8[TX_TYPES][(32 + 1) * 2];
  int16_t nb_8X4[TX_TYPES][(32 + 1) * 2];
  int16_t nb_8X16[TX_TYPES][(128 + 1) * 2];
  int16_t nb_16X8[TX_TYPES][(128 + 1) * 2];
  int16_t nb_16X32[TX_TYPES][(512 + 1) * 2];
  int16_t nb_32X16[TX_TYPES][(512 + 1) * 2];

  SCAN_ORDER sc[TX_SIZES_ALL][TX_TYPES];

  int16_t eob_threshold[TX_SIZES_ALL][TX_TYPES][EOB_THRESHOLD_NUM];
#endif  // CONFIG_ADAPT_SCAN

#if CONFIG_LV_MAP
  aom_prob txb_skip[TX_SIZES][TXB_SKIP_CONTEXTS];
  aom_prob nz_map[TX_SIZES][PLANE_TYPES][SIG_COEF_CONTEXTS];
  aom_prob eob_flag[TX_SIZES][PLANE_TYPES][EOB_COEF_CONTEXTS];
  aom_prob dc_sign[PLANE_TYPES][DC_SIGN_CONTEXTS];
  aom_prob coeff_base[TX_SIZES][PLANE_TYPES][NUM_BASE_LEVELS]
                     [COEFF_BASE_CONTEXTS];
  aom_prob coeff_lps[TX_SIZES][PLANE_TYPES][LEVEL_CONTEXTS];
#if BR_NODE
  aom_prob coeff_br[TX_SIZES][PLANE_TYPES][BASE_RANGE_SETS][LEVEL_CONTEXTS];
#endif
#if CONFIG_CTX1D
  aom_prob eob_mode[TX_SIZES][PLANE_TYPES][TX_CLASSES];
  aom_prob empty_line[TX_SIZES][PLANE_TYPES][TX_CLASSES][EMPTY_LINE_CONTEXTS];
  aom_prob hv_eob[TX_SIZES][PLANE_TYPES][TX_CLASSES][HV_EOB_CONTEXTS];
#endif  // CONFIG_CTX1D

#if LV_MAP_PROB
  aom_cdf_prob txb_skip_cdf[TX_SIZES][TXB_SKIP_CONTEXTS][CDF_SIZE(2)];
  aom_cdf_prob nz_map_cdf[TX_SIZES][PLANE_TYPES][SIG_COEF_CONTEXTS]
                         [CDF_SIZE(2)];
  aom_cdf_prob eob_flag_cdf[TX_SIZES][PLANE_TYPES][EOB_COEF_CONTEXTS]
                           [CDF_SIZE(2)];
  aom_cdf_prob dc_sign_cdf[PLANE_TYPES][DC_SIGN_CONTEXTS][CDF_SIZE(2)];
  aom_cdf_prob coeff_base_cdf[TX_SIZES][PLANE_TYPES][NUM_BASE_LEVELS]
                             [COEFF_BASE_CONTEXTS][CDF_SIZE(2)];
  aom_cdf_prob coeff_lps_cdf[TX_SIZES][PLANE_TYPES][LEVEL_CONTEXTS]
                            [CDF_SIZE(2)];
#if BR_NODE
  aom_cdf_prob coeff_br_cdf[TX_SIZES][PLANE_TYPES][BASE_RANGE_SETS]
                           [LEVEL_CONTEXTS][CDF_SIZE(2)];
#endif
#if CONFIG_CTX1D
  aom_cdf_prob eob_mode_cdf[TX_SIZES][PLANE_TYPES][TX_CLASSES][CDF_SIZE(2)];
  aom_cdf_prob empty_line_cdf[TX_SIZES][PLANE_TYPES][TX_CLASSES]
                             [EMPTY_LINE_CONTEXTS][CDF_SIZE(2)];
  aom_cdf_prob hv_eob_cdf[TX_SIZES][PLANE_TYPES][TX_CLASSES][HV_EOB_CONTEXTS]
                         [CDF_SIZE(2)];
#endif  // CONFIG_CTX1D
#endif  // LV_MAP_PROB
#endif

  aom_prob newmv_prob[NEWMV_MODE_CONTEXTS];
  aom_prob zeromv_prob[ZEROMV_MODE_CONTEXTS];
  aom_prob refmv_prob[REFMV_MODE_CONTEXTS];
  aom_prob drl_prob[DRL_MODE_CONTEXTS];
#if CONFIG_NEW_MULTISYMBOL
  aom_cdf_prob newmv_cdf[NEWMV_MODE_CONTEXTS][CDF_SIZE(2)];
  aom_cdf_prob zeromv_cdf[ZEROMV_MODE_CONTEXTS][CDF_SIZE(2)];
  aom_cdf_prob refmv_cdf[REFMV_MODE_CONTEXTS][CDF_SIZE(2)];
  aom_cdf_prob drl_cdf[DRL_MODE_CONTEXTS][CDF_SIZE(2)];
#endif

  aom_prob inter_compound_mode_probs[INTER_MODE_CONTEXTS]
                                    [INTER_COMPOUND_MODES - 1];
  aom_cdf_prob inter_compound_mode_cdf[INTER_MODE_CONTEXTS]
                                      [CDF_SIZE(INTER_COMPOUND_MODES)];
#if CONFIG_COMPOUND_SINGLEREF
  aom_prob inter_singleref_comp_mode_probs[INTER_MODE_CONTEXTS]
                                          [INTER_SINGLEREF_COMP_MODES - 1];
  aom_cdf_prob inter_singleref_comp_mode_cdf[INTER_MODE_CONTEXTS][CDF_SIZE(
      INTER_SINGLEREF_COMP_MODES)];
#endif  // CONFIG_COMPOUND_SINGLEREF
  aom_prob compound_type_prob[BLOCK_SIZES_ALL][COMPOUND_TYPES - 1];
#if CONFIG_WEDGE || CONFIG_COMPOUND_SEGMENT
  aom_cdf_prob compound_type_cdf[BLOCK_SIZES_ALL][CDF_SIZE(COMPOUND_TYPES)];
#endif  // CONFIG_WEDGE || CONFIG_COMPOUND_SEGMENT
#if CONFIG_INTERINTRA
  aom_prob interintra_prob[BLOCK_SIZE_GROUPS];
  aom_prob wedge_interintra_prob[BLOCK_SIZES_ALL];
  aom_prob interintra_mode_prob[BLOCK_SIZE_GROUPS][INTERINTRA_MODES - 1];
#if CONFIG_NEW_MULTISYMBOL
  aom_cdf_prob interintra_cdf[BLOCK_SIZE_GROUPS][CDF_SIZE(2)];
  aom_cdf_prob wedge_interintra_cdf[BLOCK_SIZES_ALL][CDF_SIZE(2)];
#endif
  aom_cdf_prob interintra_mode_cdf[BLOCK_SIZE_GROUPS]
                                  [CDF_SIZE(INTERINTRA_MODES)];
#endif  // CONFIG_INTERINTRA
#if CONFIG_MOTION_VAR || CONFIG_WARPED_MOTION
  aom_prob motion_mode_prob[BLOCK_SIZES_ALL][MOTION_MODES - 1];
  aom_cdf_prob motion_mode_cdf[BLOCK_SIZES_ALL][CDF_SIZE(MOTION_MODES)];
#if CONFIG_NCOBMC_ADAPT_WEIGHT && CONFIG_MOTION_VAR
  aom_prob ncobmc_mode_prob[ADAPT_OVERLAP_BLOCKS][MAX_NCOBMC_MODES - 1];
  aom_cdf_prob ncobmc_mode_cdf[ADAPT_OVERLAP_BLOCKS]
                              [CDF_SIZE(MAX_NCOBMC_MODES)];
#endif
#if CONFIG_MOTION_VAR && CONFIG_WARPED_MOTION
#if CONFIG_NCOBMC_ADAPT_WEIGHT
  aom_prob ncobmc_prob[BLOCK_SIZES_ALL][OBMC_FAMILY_MODES - 1];
  aom_cdf_prob ncobmc_cdf[BLOCK_SIZES_ALL][CDF_SIZE(OBMC_FAMILY_MODES)];
#endif  // CONFIG_NCOBMC_ADAPT_WEIGHT
  aom_prob obmc_prob[BLOCK_SIZES_ALL];
#if CONFIG_NEW_MULTISYMBOL || CONFIG_NCOBMC_ADAPT_WEIGHT
  aom_cdf_prob obmc_cdf[BLOCK_SIZES_ALL][CDF_SIZE(2)];
#endif  // CONFIG_NEW_MULTISYMBOL
#endif  // CONFIG_MOTION_VAR && CONFIG_WARPED_MOTION
#endif  // CONFIG_MOTION_VAR || CONFIG_WARPED_MOTION
  aom_prob intra_inter_prob[INTRA_INTER_CONTEXTS];
  aom_prob comp_inter_prob[COMP_INTER_CONTEXTS];
  aom_cdf_prob palette_y_size_cdf[PALETTE_BLOCK_SIZES][CDF_SIZE(PALETTE_SIZES)];
  aom_cdf_prob palette_uv_size_cdf[PALETTE_BLOCK_SIZES]
                                  [CDF_SIZE(PALETTE_SIZES)];
  aom_cdf_prob palette_y_color_index_cdf[PALETTE_SIZES]
                                        [PALETTE_COLOR_INDEX_CONTEXTS]
                                        [CDF_SIZE(PALETTE_COLORS)];
  aom_cdf_prob palette_uv_color_index_cdf[PALETTE_SIZES]
                                         [PALETTE_COLOR_INDEX_CONTEXTS]
                                         [CDF_SIZE(PALETTE_COLORS)];
#if CONFIG_MRC_TX
  aom_cdf_prob mrc_mask_inter_cdf[PALETTE_SIZES][PALETTE_COLOR_INDEX_CONTEXTS]
                                 [CDF_SIZE(PALETTE_COLORS)];
  aom_cdf_prob mrc_mask_intra_cdf[PALETTE_SIZES][PALETTE_COLOR_INDEX_CONTEXTS]
                                 [CDF_SIZE(PALETTE_COLORS)];
#endif  // CONFIG_MRC_TX
#if CONFIG_NEW_MULTISYMBOL
  aom_cdf_prob palette_y_mode_cdf[PALETTE_BLOCK_SIZES][PALETTE_Y_MODE_CONTEXTS]
                                 [CDF_SIZE(2)];
  aom_cdf_prob palette_uv_mode_cdf[PALETTE_UV_MODE_CONTEXTS][CDF_SIZE(2)];
  aom_cdf_prob comp_inter_cdf[COMP_INTER_CONTEXTS][CDF_SIZE(2)];
  aom_cdf_prob single_ref_cdf[REF_CONTEXTS][SINGLE_REFS - 1][CDF_SIZE(2)];
#endif
#if CONFIG_EXT_COMP_REFS
  aom_prob comp_ref_type_prob[COMP_REF_TYPE_CONTEXTS];
  aom_prob uni_comp_ref_prob[UNI_COMP_REF_CONTEXTS][UNIDIR_COMP_REFS - 1];
#if CONFIG_NEW_MULTISYMBOL
  aom_cdf_prob comp_ref_type_cdf[COMP_REF_TYPE_CONTEXTS][CDF_SIZE(2)];
  aom_cdf_prob uni_comp_ref_cdf[UNI_COMP_REF_CONTEXTS][UNIDIR_COMP_REFS - 1]
                               [CDF_SIZE(2)];
#endif  // CONFIG_NEW_MULTISYMBOL
#endif  // CONFIG_EXT_COMP_REFS
  aom_prob single_ref_prob[REF_CONTEXTS][SINGLE_REFS - 1];
#if CONFIG_EXT_REFS
  aom_prob comp_ref_prob[REF_CONTEXTS][FWD_REFS - 1];
  aom_prob comp_bwdref_prob[REF_CONTEXTS][BWD_REFS - 1];
#else
  aom_prob comp_ref_prob[REF_CONTEXTS][COMP_REFS - 1];
#endif  // CONFIG_EXT_REFS
#if CONFIG_NEW_MULTISYMBOL
#if CONFIG_EXT_REFS
  aom_cdf_prob comp_ref_cdf[REF_CONTEXTS][FWD_REFS - 1][CDF_SIZE(2)];
  aom_cdf_prob comp_bwdref_cdf[REF_CONTEXTS][BWD_REFS - 1][CDF_SIZE(2)];
#else
  aom_cdf_prob comp_ref_cdf[REF_CONTEXTS][COMP_REFS - 1][CDF_SIZE(2)];
#endif  // CONFIG_EXT_REFS
#endif
#if CONFIG_COMPOUND_SINGLEREF
  aom_prob comp_inter_mode_prob[COMP_INTER_MODE_CONTEXTS];
#endif  // CONFIG_COMPOUND_SINGLEREF
#if CONFIG_RECT_TX_EXT && (CONFIG_EXT_TX || CONFIG_VAR_TX)
  aom_prob quarter_tx_size_prob;
#if CONFIG_NEW_MULTISYMBOL
  aom_cdf_prob quarter_tx_size_cdf[CDF_SIZE(2)];
#endif
#endif
#if CONFIG_VAR_TX
  aom_prob txfm_partition_prob[TXFM_PARTITION_CONTEXTS];
#if CONFIG_NEW_MULTISYMBOL
  aom_cdf_prob txfm_partition_cdf[TXFM_PARTITION_CONTEXTS][CDF_SIZE(2)];
#endif
#endif  // CONFIG_VAR_TX
  aom_prob skip_probs[SKIP_CONTEXTS];
#if CONFIG_NEW_MULTISYMBOL
  aom_cdf_prob skip_cdfs[SKIP_CONTEXTS][CDF_SIZE(2)];
  aom_cdf_prob intra_inter_cdf[INTRA_INTER_CONTEXTS][CDF_SIZE(2)];
#endif
  nmv_context nmvc[NMV_CONTEXTS];
#if CONFIG_INTRABC
  nmv_context ndvc;
  aom_cdf_prob intrabc_cdf[CDF_SIZE(2)];
#endif
  int initialized;
#if CONFIG_SUPERTX
  aom_prob supertx_prob[PARTITION_SUPERTX_CONTEXTS][TX_SIZES];
#endif  // CONFIG_SUPERTX
  struct segmentation_probs seg;
#if CONFIG_EXT_INTRA
#if CONFIG_INTRA_INTERP
  aom_prob intra_filter_probs[INTRA_FILTERS + 1][INTRA_FILTERS - 1];
#endif  // CONFIG_INTRA_INTERP
#endif  // CONFIG_EXT_INTRA
#if CONFIG_FILTER_INTRA
  aom_prob filter_intra_probs[PLANE_TYPES];
#endif  // CONFIG_FILTER_INTRA
#if CONFIG_LOOP_RESTORATION
  aom_prob switchable_restore_prob[RESTORE_SWITCHABLE_TYPES - 1];
#endif  // CONFIG_LOOP_RESTORATION
  aom_cdf_prob y_mode_cdf[BLOCK_SIZE_GROUPS][CDF_SIZE(INTRA_MODES)];
  aom_cdf_prob uv_mode_cdf[INTRA_MODES][CDF_SIZE(UV_INTRA_MODES)];
#if CONFIG_EXT_PARTITION_TYPES
  aom_cdf_prob partition_cdf[PARTITION_CONTEXTS][CDF_SIZE(EXT_PARTITION_TYPES)];
#else
  aom_cdf_prob partition_cdf[PARTITION_CONTEXTS][CDF_SIZE(PARTITION_TYPES)];
#endif
  aom_cdf_prob switchable_interp_cdf[SWITCHABLE_FILTER_CONTEXTS]
                                    [CDF_SIZE(SWITCHABLE_FILTERS)];
/* kf_y_cdf is discarded after use, so does not require persistent storage.
   However, we keep it with the other CDFs in this struct since it needs to
   be copied to each tile to support parallelism just like the others.
*/
#if CONFIG_KF_CTX
  aom_cdf_prob kf_y_cdf[KF_MODE_CONTEXTS][KF_MODE_CONTEXTS]
                       [CDF_SIZE(INTRA_MODES)];
#else
  aom_cdf_prob kf_y_cdf[INTRA_MODES][INTRA_MODES][CDF_SIZE(INTRA_MODES)];
#endif
  aom_cdf_prob tx_size_cdf[MAX_TX_DEPTH][TX_SIZE_CONTEXTS]
                          [CDF_SIZE(MAX_TX_DEPTH + 1)];
  aom_cdf_prob delta_q_cdf[CDF_SIZE(DELTA_Q_PROBS + 1)];
#if CONFIG_EXT_DELTA_Q
#if CONFIG_LOOPFILTER_LEVEL
  aom_cdf_prob delta_lf_multi_cdf[FRAME_LF_COUNT][CDF_SIZE(DELTA_LF_PROBS + 1)];
#endif  // CONFIG_LOOPFILTER_LEVEL
  aom_cdf_prob delta_lf_cdf[CDF_SIZE(DELTA_LF_PROBS + 1)];
#endif
#if CONFIG_EXT_TX
  aom_cdf_prob intra_ext_tx_cdf[EXT_TX_SETS_INTRA][EXT_TX_SIZES][INTRA_MODES]
                               [CDF_SIZE(TX_TYPES)];
  aom_cdf_prob inter_ext_tx_cdf[EXT_TX_SETS_INTER][EXT_TX_SIZES]
                               [CDF_SIZE(TX_TYPES)];
#else
  aom_cdf_prob intra_ext_tx_cdf[EXT_TX_SIZES][TX_TYPES][CDF_SIZE(TX_TYPES)];
  aom_cdf_prob inter_ext_tx_cdf[EXT_TX_SIZES][CDF_SIZE(TX_TYPES)];
#endif  // CONFIG_EXT_TX
#if CONFIG_LGT_FROM_PRED
  aom_prob intra_lgt_prob[LGT_SIZES][INTRA_MODES];
  aom_prob inter_lgt_prob[LGT_SIZES];
#endif  // CONFIG_LGT_FROM_PRED
#if CONFIG_EXT_INTRA && CONFIG_INTRA_INTERP
  aom_cdf_prob intra_filter_cdf[INTRA_FILTERS + 1][CDF_SIZE(INTRA_FILTERS)];
#endif  // CONFIG_EXT_INTRA && CONFIG_INTRA_INTERP
  aom_prob delta_q_prob[DELTA_Q_PROBS];
#if CONFIG_EXT_DELTA_Q
#if CONFIG_LOOPFILTER_LEVEL
  aom_prob delta_lf_multi_prob[FRAME_LF_COUNT][DELTA_LF_PROBS];
#endif  // CONFIG_LOOPFILTER_LEVEL
  aom_prob delta_lf_prob[DELTA_LF_PROBS];
#endif
#if CONFIG_PVQ
  // TODO(any): If PVQ is enabled, most of coefficient related cdf,
  // such as coef_cdfs[], coef_tail_cdfs[], and coef_heaf_cdfs[] can be removed.
  od_adapt_ctx pvq_context;
#endif  // CONFIG_PVQ
#if CONFIG_CFL
  aom_cdf_prob cfl_sign_cdf[CDF_SIZE(CFL_JOINT_SIGNS)];
  aom_cdf_prob cfl_alpha_cdf[CFL_ALPHA_CONTEXTS][CDF_SIZE(CFL_ALPHABET_SIZE)];
#endif
#if CONFIG_LPF_SB
  aom_cdf_prob lpf_reuse_cdf[LPF_REUSE_CONTEXT][CDF_SIZE(2)];
  aom_cdf_prob lpf_delta_cdf[LPF_DELTA_CONTEXT][CDF_SIZE(DELTA_RANGE)];
  aom_cdf_prob lpf_sign_cdf[LPF_REUSE_CONTEXT][LPF_SIGN_CONTEXT][CDF_SIZE(2)];
#endif  // CONFIG_LPF_SB
} FRAME_CONTEXT;

typedef struct FRAME_COUNTS {
// Note: This structure should only contain 'unsigned int' fields, or
// aggregates built solely from 'unsigned int' fields/elements
#if CONFIG_ENTROPY_STATS
  unsigned int kf_y_mode[INTRA_MODES][INTRA_MODES][INTRA_MODES];
  unsigned int y_mode[BLOCK_SIZE_GROUPS][INTRA_MODES];
  unsigned int uv_mode[INTRA_MODES][UV_INTRA_MODES];
#endif  // CONFIG_ENTROPY_STATS
#if CONFIG_EXT_PARTITION_TYPES
  unsigned int partition[PARTITION_CONTEXTS][EXT_PARTITION_TYPES];
#else
  unsigned int partition[PARTITION_CONTEXTS][PARTITION_TYPES];
#endif
  unsigned int switchable_interp[SWITCHABLE_FILTER_CONTEXTS]
                                [SWITCHABLE_FILTERS];
#if CONFIG_ADAPT_SCAN
#if CONFIG_CHROMA_2X2
  unsigned int non_zero_count_2x2[TX_TYPES][4];
#endif  // CONFIG_CHROMA_2X2
  unsigned int non_zero_count_4X4[TX_TYPES][16];
  unsigned int non_zero_count_8X8[TX_TYPES][64];
  unsigned int non_zero_count_16X16[TX_TYPES][256];
  unsigned int non_zero_count_32X32[TX_TYPES][1024];

  unsigned int non_zero_count_4x8[TX_TYPES][32];
  unsigned int non_zero_count_8x4[TX_TYPES][32];
  unsigned int non_zero_count_8x16[TX_TYPES][128];
  unsigned int non_zero_count_16x8[TX_TYPES][128];
  unsigned int non_zero_count_16x32[TX_TYPES][512];
  unsigned int non_zero_count_32x16[TX_TYPES][512];

  unsigned int txb_count[TX_SIZES_ALL][TX_TYPES];
#endif  // CONFIG_ADAPT_SCAN

#if CONFIG_LV_MAP
  unsigned int txb_skip[TX_SIZES][TXB_SKIP_CONTEXTS][2];
  unsigned int nz_map[TX_SIZES][PLANE_TYPES][SIG_COEF_CONTEXTS][2];
  unsigned int eob_flag[TX_SIZES][PLANE_TYPES][EOB_COEF_CONTEXTS][2];
  unsigned int dc_sign[PLANE_TYPES][DC_SIGN_CONTEXTS][2];
  unsigned int coeff_base[TX_SIZES][PLANE_TYPES][NUM_BASE_LEVELS]
                         [COEFF_BASE_CONTEXTS][2];
  unsigned int coeff_lps[TX_SIZES][PLANE_TYPES][LEVEL_CONTEXTS][2];
  unsigned int coeff_br[TX_SIZES][PLANE_TYPES][BASE_RANGE_SETS][LEVEL_CONTEXTS]
                       [2];
#if CONFIG_CTX1D
  unsigned int eob_mode[TX_SIZES][PLANE_TYPES][TX_CLASSES][2];
  unsigned int empty_line[TX_SIZES][PLANE_TYPES][TX_CLASSES]
                         [EMPTY_LINE_CONTEXTS][2];
  unsigned int hv_eob[TX_SIZES][PLANE_TYPES][TX_CLASSES][HV_EOB_CONTEXTS][2];
#endif  // CONFIG_CTX1D
#endif  // CONFIG_LV_MAP

#if CONFIG_SYMBOLRATE
  unsigned int coeff_num[2];   // 0: zero coeff 1: non-zero coeff
  unsigned int symbol_num[2];  // 0: entropy symbol 1: non-entropy symbol
#endif

  unsigned int newmv_mode[NEWMV_MODE_CONTEXTS][2];
  unsigned int zeromv_mode[ZEROMV_MODE_CONTEXTS][2];
  unsigned int refmv_mode[REFMV_MODE_CONTEXTS][2];
  unsigned int drl_mode[DRL_MODE_CONTEXTS][2];

  unsigned int inter_compound_mode[INTER_MODE_CONTEXTS][INTER_COMPOUND_MODES];
#if CONFIG_COMPOUND_SINGLEREF
  unsigned int inter_singleref_comp_mode[INTER_MODE_CONTEXTS]
                                        [INTER_SINGLEREF_COMP_MODES];
#endif  // CONFIG_COMPOUND_SINGLEREF
#if CONFIG_INTERINTRA
  unsigned int interintra[BLOCK_SIZE_GROUPS][2];
  unsigned int interintra_mode[BLOCK_SIZE_GROUPS][INTERINTRA_MODES];
  unsigned int wedge_interintra[BLOCK_SIZES_ALL][2];
#endif  // CONFIG_INTERINTRA
  unsigned int compound_interinter[BLOCK_SIZES_ALL][COMPOUND_TYPES];
#if CONFIG_MOTION_VAR || CONFIG_WARPED_MOTION
  unsigned int motion_mode[BLOCK_SIZES_ALL][MOTION_MODES];
#if CONFIG_NCOBMC_ADAPT_WEIGHT && CONFIG_MOTION_VAR
  unsigned int ncobmc_mode[ADAPT_OVERLAP_BLOCKS][MAX_NCOBMC_MODES];
#endif
#if CONFIG_MOTION_VAR && CONFIG_WARPED_MOTION
#if CONFIG_NCOBMC_ADAPT_WEIGHT
  unsigned int ncobmc[BLOCK_SIZES_ALL][OBMC_FAMILY_MODES];
#endif  // CONFIG_NCOBMC_ADAPT_WEIGHT
  unsigned int obmc[BLOCK_SIZES_ALL][2];
#endif  // CONFIG_MOTION_VAR && CONFIG_WARPED_MOTION
#endif  // CONFIG_MOTION_VAR || CONFIG_WARPED_MOTION
  unsigned int intra_inter[INTRA_INTER_CONTEXTS][2];
  unsigned int comp_inter[COMP_INTER_CONTEXTS][2];
#if CONFIG_EXT_COMP_REFS
  unsigned int comp_ref_type[COMP_REF_TYPE_CONTEXTS][2];
  unsigned int uni_comp_ref[UNI_COMP_REF_CONTEXTS][UNIDIR_COMP_REFS - 1][2];
#endif  // CONFIG_EXT_COMP_REFS
  unsigned int single_ref[REF_CONTEXTS][SINGLE_REFS - 1][2];
#if CONFIG_EXT_REFS
  unsigned int comp_ref[REF_CONTEXTS][FWD_REFS - 1][2];
  unsigned int comp_bwdref[REF_CONTEXTS][BWD_REFS - 1][2];
#else
  unsigned int comp_ref[REF_CONTEXTS][COMP_REFS - 1][2];
#endif  // CONFIG_EXT_REFS
#if CONFIG_COMPOUND_SINGLEREF
  unsigned int comp_inter_mode[COMP_INTER_MODE_CONTEXTS][2];
#endif  // CONFIG_COMPOUND_SINGLEREF
  // TODO(urvang): Only needed for !CONFIG_VAR_TX case. So can be removed when
  // CONFIG_VAR_TX flag is removed.
  unsigned int tx_size[MAX_TX_DEPTH][TX_SIZE_CONTEXTS][MAX_TX_DEPTH + 1];
#if CONFIG_RECT_TX_EXT && (CONFIG_EXT_TX || CONFIG_VAR_TX)
  unsigned int quarter_tx_size[2];
#endif
#if CONFIG_VAR_TX
  unsigned int txfm_partition[TXFM_PARTITION_CONTEXTS][2];
#endif
  unsigned int skip[SKIP_CONTEXTS][2];
  nmv_context_counts mv[NMV_CONTEXTS];
#if CONFIG_INTRABC
  unsigned int intrabc[2];
  nmv_context_counts dv;
#endif
#if CONFIG_LGT_FROM_PRED
  unsigned int intra_lgt[LGT_SIZES][INTRA_MODES][2];
  unsigned int inter_lgt[LGT_SIZES][2];
#endif  // CONFIG_LGT_FROM_PRED
  unsigned int delta_q[DELTA_Q_PROBS][2];
#if CONFIG_EXT_DELTA_Q
#if CONFIG_LOOPFILTER_LEVEL
  unsigned int delta_lf_multi[FRAME_LF_COUNT][DELTA_LF_PROBS][2];
#endif  // CONFIG_LOOPFILTER_LEVEL
  unsigned int delta_lf[DELTA_LF_PROBS][2];
#endif
#if CONFIG_EXT_TX && CONFIG_RECT_TX
  unsigned int tx_size_implied[TX_SIZES][TX_SIZES];
#endif  // CONFIG_EXT_TX && CONFIG_RECT_TX
#if CONFIG_ENTROPY_STATS
#if CONFIG_EXT_TX
  unsigned int inter_ext_tx[EXT_TX_SETS_INTER][EXT_TX_SIZES][TX_TYPES];
  unsigned int intra_ext_tx[EXT_TX_SETS_INTRA][EXT_TX_SIZES][INTRA_MODES]
                           [TX_TYPES];
#else
  unsigned int intra_ext_tx[EXT_TX_SIZES][TX_TYPES][TX_TYPES];
  unsigned int inter_ext_tx[EXT_TX_SIZES][TX_TYPES];
#endif  // CONFIG_EXT_TX
#endif  // CONFIG_ENTROPY_STATS
#if CONFIG_SUPERTX
  unsigned int supertx[PARTITION_SUPERTX_CONTEXTS][TX_SIZES][2];
  unsigned int supertx_size[TX_SIZES];
#endif  // CONFIG_SUPERTX
  struct seg_counts seg;
#if CONFIG_EXT_INTRA
#if CONFIG_INTRA_INTERP
  unsigned int intra_filter[INTRA_FILTERS + 1][INTRA_FILTERS];
#endif  // CONFIG_INTRA_INTERP
#endif  // CONFIG_EXT_INTRA
#if CONFIG_FILTER_INTRA
  unsigned int filter_intra[PLANE_TYPES][2];
#endif  // CONFIG_FILTER_INTRA
#if CONFIG_LPF_SB
  unsigned int lpf_reuse[LPF_REUSE_CONTEXT][2];
  unsigned int lpf_delta[LPF_DELTA_CONTEXT][DELTA_RANGE];
  unsigned int lpf_sign[LPF_SIGN_CONTEXT][2];
#endif  // CONFIG_LPF_SB
} FRAME_COUNTS;

#if CONFIG_KF_CTX
extern const aom_cdf_prob default_kf_y_mode_cdf[KF_MODE_CONTEXTS]
                                               [KF_MODE_CONTEXTS]
                                               [CDF_SIZE(INTRA_MODES)];
#else
extern const aom_cdf_prob default_kf_y_mode_cdf[INTRA_MODES][INTRA_MODES]
                                               [CDF_SIZE(INTRA_MODES)];
#endif

extern const aom_prob av1_default_palette_y_mode_prob[PALETTE_BLOCK_SIZES]
                                                     [PALETTE_Y_MODE_CONTEXTS];
extern const aom_prob
    av1_default_palette_uv_mode_prob[PALETTE_UV_MODE_CONTEXTS];

#if CONFIG_EXT_TX
static const int av1_ext_tx_ind[EXT_TX_SET_TYPES][TX_TYPES] = {
  {
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  },
  {
      1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  },
#if CONFIG_MRC_TX
  {
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
  },
  {
      1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2,
  },
#endif  // CONFIG_MRC_TX
  {
      1, 3, 4, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  },
  {
      1, 5, 6, 4, 0, 0, 0, 0, 0, 0, 2, 3, 0, 0, 0, 0,
  },
  {
      3, 4, 5, 8, 6, 7, 9, 10, 11, 0, 1, 2, 0, 0, 0, 0,
  },
  {
      7, 8, 9, 12, 10, 11, 13, 14, 15, 0, 1, 2, 3, 4, 5, 6,
  },
};

static const int av1_ext_tx_inv[EXT_TX_SET_TYPES][TX_TYPES] = {
  {
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  },
  {
      9, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  },
#if CONFIG_MRC_TX
  {
      0, 16, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  },
  {
      9, 0, 16, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  },
#endif  // CONFIG_MRC_TX
  {
      9, 0, 3, 1, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  },
  {
      9, 0, 10, 11, 3, 1, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  },
  {
      9, 10, 11, 0, 1, 2, 4, 5, 3, 6, 7, 8, 0, 0, 0, 0,
  },
  {
      9, 10, 11, 12, 13, 14, 15, 0, 1, 2, 4, 5, 3, 6, 7, 8,
  },
};
#else
#if CONFIG_MRC_TX
static const int av1_ext_tx_ind[TX_TYPES] = {
  0, 3, 4, 2, 1,
};
static const int av1_ext_tx_inv[TX_TYPES] = {
  0, 4, 3, 1, 2,
};
#else
static const int av1_ext_tx_ind[TX_TYPES] = {
  0, 2, 3, 1,
};
static const int av1_ext_tx_inv[TX_TYPES] = {
  0, 3, 1, 2,
};
#endif  // CONFIG_MRC_TX
#endif  // CONFIG_EXT_TX

#if CONFIG_INTERINTRA
extern const aom_tree_index
    av1_interintra_mode_tree[TREE_SIZE(INTERINTRA_MODES)];
#endif
extern const aom_tree_index
    av1_inter_compound_mode_tree[TREE_SIZE(INTER_COMPOUND_MODES)];
#if CONFIG_COMPOUND_SINGLEREF
extern const aom_tree_index
    av1_inter_singleref_comp_mode_tree[TREE_SIZE(INTER_SINGLEREF_COMP_MODES)];
#endif  // CONFIG_COMPOUND_SINGLEREF
extern const aom_tree_index av1_compound_type_tree[TREE_SIZE(COMPOUND_TYPES)];
extern const aom_tree_index av1_partition_tree[TREE_SIZE(PARTITION_TYPES)];
#if CONFIG_EXT_PARTITION_TYPES
extern const aom_tree_index
    av1_ext_partition_tree[TREE_SIZE(EXT_PARTITION_TYPES)];
#endif
extern const aom_tree_index
    av1_palette_color_index_tree[PALETTE_SIZES][TREE_SIZE(PALETTE_COLORS)];
#if CONFIG_EXT_INTRA && CONFIG_INTRA_INTERP
extern const aom_tree_index av1_intra_filter_tree[TREE_SIZE(INTRA_FILTERS)];
#endif  // CONFIG_EXT_INTRA && CONFIG_INTRA_INTERP
#if CONFIG_EXT_TX
extern const aom_tree_index av1_ext_tx_tree[EXT_TX_SET_TYPES]
                                           [TREE_SIZE(TX_TYPES)];
#else
extern const aom_tree_index av1_ext_tx_tree[TREE_SIZE(TX_TYPES)];
#endif  // CONFIG_EXT_TX
#if CONFIG_MOTION_VAR || CONFIG_WARPED_MOTION
extern const aom_tree_index av1_motion_mode_tree[TREE_SIZE(MOTION_MODES)];
#endif  // CONFIG_MOTION_VAR || CONFIG_WARPED_MOTION
#if CONFIG_NCOBMC_ADAPT_WEIGHT
extern const aom_tree_index av1_ncobmc_mode_tree[TREE_SIZE(MAX_NCOBMC_MODES)];
#if CONFIG_WARPED_MOTION
extern const aom_tree_index av1_ncobmc_tree[TREE_SIZE(OBMC_FAMILY_MODES)];
#endif  // CONFIG_WARPED_MOTION
#endif  // CONFIG_NCOBMC_ADAPT_WEIGHT
#if CONFIG_LOOP_RESTORATION
#define RESTORE_NONE_SGRPROJ_PROB 64
#define RESTORE_NONE_BILATERAL_PROB 16
#define RESTORE_NONE_WIENER_PROB 64
#define RESTORE_NONE_DOMAINTXFMRF_PROB 64
extern const aom_tree_index
    av1_switchable_restore_tree[TREE_SIZE(RESTORE_SWITCHABLE_TYPES)];
#endif  // CONFIG_LOOP_RESTORATION

void av1_setup_past_independence(struct AV1Common *cm);

void av1_adapt_intra_frame_probs(struct AV1Common *cm);
void av1_adapt_inter_frame_probs(struct AV1Common *cm);

static INLINE int av1_ceil_log2(int n) {
  int i = 1, p = 2;
  while (p < n) {
    i++;
    p = p << 1;
  }
  return i;
}

// Returns the context for palette color index at row 'r' and column 'c',
// along with the 'color_order' of neighbors and the 'color_idx'.
// The 'color_map' is a 2D array with the given 'stride'.
int av1_get_palette_color_index_context(const uint8_t *color_map, int stride,
                                        int r, int c, int palette_size,
                                        uint8_t *color_order, int *color_idx);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // AV1_COMMON_ENTROPYMODE_H_
