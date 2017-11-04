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

#ifndef AV1_ENCODER_CONTEXT_TREE_H_
#define AV1_ENCODER_CONTEXT_TREE_H_

#include "av1/common/blockd.h"
#include "av1/encoder/block.h"

#ifdef __cplusplus
extern "C" {
#endif

struct AV1_COMP;
struct AV1Common;
struct ThreadData;

// Structure to hold snapshot of coding context during the mode picking process
typedef struct {
  MODE_INFO mic;
  MB_MODE_INFO_EXT mbmi_ext;
  uint8_t *color_index_map[2];
#if CONFIG_MRC_TX
  uint8_t *mrc_mask;
#endif  // CONFIG_MRC_TX
#if CONFIG_VAR_TX
  uint8_t *blk_skip[MAX_MB_PLANE];
#endif

  tran_low_t *coeff[MAX_MB_PLANE];
  tran_low_t *qcoeff[MAX_MB_PLANE];
  tran_low_t *dqcoeff[MAX_MB_PLANE];
#if CONFIG_PVQ
  tran_low_t *pvq_ref_coeff[MAX_MB_PLANE];
#endif
  uint16_t *eobs[MAX_MB_PLANE];
#if CONFIG_LV_MAP
  uint8_t *txb_entropy_ctx[MAX_MB_PLANE];
#endif

  int num_4x4_blk;
  int skip;
  // For current partition, only if all Y, U, and V transform blocks'
  // coefficients are quantized to 0, skippable is set to 1.
  int skippable;
  int best_mode_index;
  int hybrid_pred_diff;
  int comp_pred_diff;
  int single_pred_diff;

  // TODO(jingning) Use RD_COST struct here instead. This involves a boarder
  // scope of refactoring.
  int rate;
  int64_t dist;

  // motion vector cache for adaptive motion search control in partition
  // search loop
  MV pred_mv[TOTAL_REFS_PER_FRAME];
  InterpFilter pred_interp_filter;
#if CONFIG_EXT_PARTITION_TYPES
  PARTITION_TYPE partition;
#endif
} PICK_MODE_CONTEXT;

typedef struct PC_TREE {
  int index;
  PARTITION_TYPE partitioning;
  BLOCK_SIZE block_size;
  PICK_MODE_CONTEXT none;
  PICK_MODE_CONTEXT horizontal[2];
  PICK_MODE_CONTEXT vertical[2];
#if CONFIG_EXT_PARTITION_TYPES
  PICK_MODE_CONTEXT horizontala[3];
  PICK_MODE_CONTEXT horizontalb[3];
  PICK_MODE_CONTEXT verticala[3];
  PICK_MODE_CONTEXT verticalb[3];
  PICK_MODE_CONTEXT horizontal4[4];
  PICK_MODE_CONTEXT vertical4[4];
#endif
  // TODO(jingning): remove leaf_split[] when cb4x4 experiment flag is removed.
  union {
    struct PC_TREE *split[4];
    PICK_MODE_CONTEXT *leaf_split[4];
  };
#if CONFIG_SUPERTX
  PICK_MODE_CONTEXT horizontal_supertx;
  PICK_MODE_CONTEXT vertical_supertx;
  PICK_MODE_CONTEXT split_supertx;
#if CONFIG_EXT_PARTITION_TYPES
  PICK_MODE_CONTEXT horizontala_supertx;
  PICK_MODE_CONTEXT horizontalb_supertx;
  PICK_MODE_CONTEXT verticala_supertx;
  PICK_MODE_CONTEXT verticalb_supertx;
#endif
#endif
} PC_TREE;

void av1_setup_pc_tree(struct AV1Common *cm, struct ThreadData *td);
void av1_free_pc_tree(struct ThreadData *td);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif /* AV1_ENCODER_CONTEXT_TREE_H_ */
