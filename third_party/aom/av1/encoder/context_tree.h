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

typedef enum {
  // Search all the partition types in this plane.
  SEARCH_FULL_PLANE = 0,
  // Only search none_partition coding block.
  NONE_PARTITION_PLANE = 1,
  // Search all the partition types in this plane except split.
  SEARCH_SAME_PLANE = 2,
  // Skip search partition on this plane. Go split directly.
  SPLIT_PLANE = 3,
} CB_TREE_SEARCH;

// Structure to hold snapshot of coding context during the mode picking process
typedef struct {
  MB_MODE_INFO mic;
  MB_MODE_INFO_EXT mbmi_ext;
  uint8_t *color_index_map[2];
  uint8_t *blk_skip;

  tran_low_t *coeff[MAX_MB_PLANE];
  tran_low_t *qcoeff[MAX_MB_PLANE];
  tran_low_t *dqcoeff[MAX_MB_PLANE];
  uint16_t *eobs[MAX_MB_PLANE];
  uint8_t *txb_entropy_ctx[MAX_MB_PLANE];

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
  int64_t rdcost;
  int rd_mode_is_ready;  // Flag to indicate whether rd pick mode decision has
                         // been made.

  // motion vector cache for adaptive motion search control in partition
  // search loop
  MV pred_mv[REF_FRAMES];
  InterpFilter pred_interp_filter;
  PARTITION_TYPE partition;
} PICK_MODE_CONTEXT;

typedef struct {
  int valid;
  int split;
  int skip;
  int64_t rdcost;
  int sub_block_split[4];
  int sub_block_skip[4];
  int64_t sub_block_rdcost[4];
} PC_TREE_STATS;

typedef struct PC_TREE {
  int index;
  PARTITION_TYPE partitioning;
  BLOCK_SIZE block_size;
  PICK_MODE_CONTEXT none;
  PICK_MODE_CONTEXT horizontal[2];
  PICK_MODE_CONTEXT vertical[2];
  PICK_MODE_CONTEXT horizontala[3];
  PICK_MODE_CONTEXT horizontalb[3];
  PICK_MODE_CONTEXT verticala[3];
  PICK_MODE_CONTEXT verticalb[3];
  PICK_MODE_CONTEXT horizontal4[4];
  PICK_MODE_CONTEXT vertical4[4];
  CB_TREE_SEARCH cb_search_range;
  struct PC_TREE *split[4];
  PC_TREE_STATS pc_tree_stats;
} PC_TREE;

void av1_setup_pc_tree(struct AV1Common *cm, struct ThreadData *td);
void av1_free_pc_tree(struct ThreadData *td, const int num_planes);
void av1_copy_tree_context(PICK_MODE_CONTEXT *dst_ctx,
                           PICK_MODE_CONTEXT *src_ctx);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif /* AV1_ENCODER_CONTEXT_TREE_H_ */
