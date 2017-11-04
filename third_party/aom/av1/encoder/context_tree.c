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

#include "av1/encoder/context_tree.h"
#include "av1/encoder/encoder.h"

static const BLOCK_SIZE square[MAX_SB_SIZE_LOG2 - 1] = {
#if CONFIG_CB4X4
  BLOCK_4X4,
#endif
  BLOCK_8X8,     BLOCK_16X16, BLOCK_32X32, BLOCK_64X64,
#if CONFIG_EXT_PARTITION
  BLOCK_128X128,
#endif  // CONFIG_EXT_PARTITION
};

static void alloc_mode_context(AV1_COMMON *cm, int num_pix,
#if CONFIG_EXT_PARTITION_TYPES
                               PARTITION_TYPE partition,
#endif
                               PICK_MODE_CONTEXT *ctx) {
  int i;
  const int num_blk = num_pix / 16;
  ctx->num_4x4_blk = num_blk;

#if CONFIG_EXT_PARTITION_TYPES
  ctx->partition = partition;
#endif

  for (i = 0; i < MAX_MB_PLANE; ++i) {
#if CONFIG_VAR_TX
    CHECK_MEM_ERROR(cm, ctx->blk_skip[i], aom_calloc(num_blk, sizeof(uint8_t)));
#endif
    CHECK_MEM_ERROR(cm, ctx->coeff[i],
                    aom_memalign(32, num_pix * sizeof(*ctx->coeff[i])));
    CHECK_MEM_ERROR(cm, ctx->qcoeff[i],
                    aom_memalign(32, num_pix * sizeof(*ctx->qcoeff[i])));
    CHECK_MEM_ERROR(cm, ctx->dqcoeff[i],
                    aom_memalign(32, num_pix * sizeof(*ctx->dqcoeff[i])));
    CHECK_MEM_ERROR(cm, ctx->eobs[i],
                    aom_memalign(32, num_blk * sizeof(*ctx->eobs[i])));
#if CONFIG_LV_MAP
    CHECK_MEM_ERROR(
        cm, ctx->txb_entropy_ctx[i],
        aom_memalign(32, num_blk * sizeof(*ctx->txb_entropy_ctx[i])));
#endif

#if CONFIG_PVQ
    CHECK_MEM_ERROR(cm, ctx->pvq_ref_coeff[i],
                    aom_memalign(32, num_pix * sizeof(*ctx->pvq_ref_coeff[i])));
#endif
  }

  for (i = 0; i < 2; ++i) {
    CHECK_MEM_ERROR(
        cm, ctx->color_index_map[i],
        aom_memalign(32, num_pix * sizeof(*ctx->color_index_map[i])));
  }
#if CONFIG_MRC_TX
  CHECK_MEM_ERROR(cm, ctx->mrc_mask,
                  aom_memalign(32, num_pix * sizeof(*ctx->mrc_mask)));
#endif  // CONFIG_MRC_TX
}

static void free_mode_context(PICK_MODE_CONTEXT *ctx) {
  int i;
  for (i = 0; i < MAX_MB_PLANE; ++i) {
#if CONFIG_VAR_TX
    aom_free(ctx->blk_skip[i]);
    ctx->blk_skip[i] = 0;
#endif
    aom_free(ctx->coeff[i]);
    ctx->coeff[i] = 0;
    aom_free(ctx->qcoeff[i]);
    ctx->qcoeff[i] = 0;
    aom_free(ctx->dqcoeff[i]);
    ctx->dqcoeff[i] = 0;
#if CONFIG_PVQ
    aom_free(ctx->pvq_ref_coeff[i]);
    ctx->pvq_ref_coeff[i] = 0;
#endif
    aom_free(ctx->eobs[i]);
    ctx->eobs[i] = 0;
#if CONFIG_LV_MAP
    aom_free(ctx->txb_entropy_ctx[i]);
    ctx->txb_entropy_ctx[i] = 0;
#endif
  }

  for (i = 0; i < 2; ++i) {
    aom_free(ctx->color_index_map[i]);
    ctx->color_index_map[i] = 0;
  }
#if CONFIG_MRC_TX
  aom_free(ctx->mrc_mask);
  ctx->mrc_mask = 0;
#endif  // CONFIG_MRC_TX
}

static void alloc_tree_contexts(AV1_COMMON *cm, PC_TREE *tree, int num_pix) {
#if CONFIG_EXT_PARTITION_TYPES
  alloc_mode_context(cm, num_pix, PARTITION_NONE, &tree->none);
  alloc_mode_context(cm, num_pix / 2, PARTITION_HORZ, &tree->horizontal[0]);
  alloc_mode_context(cm, num_pix / 2, PARTITION_VERT, &tree->vertical[0]);
  alloc_mode_context(cm, num_pix / 2, PARTITION_VERT, &tree->horizontal[1]);
  alloc_mode_context(cm, num_pix / 2, PARTITION_VERT, &tree->vertical[1]);

  alloc_mode_context(cm, num_pix / 4, PARTITION_HORZ_A, &tree->horizontala[0]);
  alloc_mode_context(cm, num_pix / 4, PARTITION_HORZ_A, &tree->horizontala[1]);
  alloc_mode_context(cm, num_pix / 2, PARTITION_HORZ_A, &tree->horizontala[2]);
  alloc_mode_context(cm, num_pix / 2, PARTITION_HORZ_B, &tree->horizontalb[0]);
  alloc_mode_context(cm, num_pix / 4, PARTITION_HORZ_B, &tree->horizontalb[1]);
  alloc_mode_context(cm, num_pix / 4, PARTITION_HORZ_B, &tree->horizontalb[2]);
  alloc_mode_context(cm, num_pix / 4, PARTITION_VERT_A, &tree->verticala[0]);
  alloc_mode_context(cm, num_pix / 4, PARTITION_VERT_A, &tree->verticala[1]);
  alloc_mode_context(cm, num_pix / 2, PARTITION_VERT_A, &tree->verticala[2]);
  alloc_mode_context(cm, num_pix / 2, PARTITION_VERT_B, &tree->verticalb[0]);
  alloc_mode_context(cm, num_pix / 4, PARTITION_VERT_B, &tree->verticalb[1]);
  alloc_mode_context(cm, num_pix / 4, PARTITION_VERT_B, &tree->verticalb[2]);
  for (int i = 0; i < 4; ++i) {
    alloc_mode_context(cm, num_pix / 4, PARTITION_HORZ_4,
                       &tree->horizontal4[i]);
    alloc_mode_context(cm, num_pix / 4, PARTITION_HORZ_4, &tree->vertical4[i]);
  }
#if CONFIG_SUPERTX
  alloc_mode_context(cm, num_pix, PARTITION_HORZ, &tree->horizontal_supertx);
  alloc_mode_context(cm, num_pix, PARTITION_VERT, &tree->vertical_supertx);
  alloc_mode_context(cm, num_pix, PARTITION_SPLIT, &tree->split_supertx);
  alloc_mode_context(cm, num_pix, PARTITION_HORZ_A, &tree->horizontala_supertx);
  alloc_mode_context(cm, num_pix, PARTITION_HORZ_B, &tree->horizontalb_supertx);
  alloc_mode_context(cm, num_pix, PARTITION_VERT_A, &tree->verticala_supertx);
  alloc_mode_context(cm, num_pix, PARTITION_VERT_B, &tree->verticalb_supertx);
#endif  // CONFIG_SUPERTX
#else
  alloc_mode_context(cm, num_pix, &tree->none);
  alloc_mode_context(cm, num_pix / 2, &tree->horizontal[0]);
  alloc_mode_context(cm, num_pix / 2, &tree->vertical[0]);
#if CONFIG_SUPERTX
  alloc_mode_context(cm, num_pix, &tree->horizontal_supertx);
  alloc_mode_context(cm, num_pix, &tree->vertical_supertx);
  alloc_mode_context(cm, num_pix, &tree->split_supertx);
#endif

  if (num_pix > 16) {
    alloc_mode_context(cm, num_pix / 2, &tree->horizontal[1]);
    alloc_mode_context(cm, num_pix / 2, &tree->vertical[1]);
  } else {
    memset(&tree->horizontal[1], 0, sizeof(tree->horizontal[1]));
    memset(&tree->vertical[1], 0, sizeof(tree->vertical[1]));
  }
#endif  // CONFIG_EXT_PARTITION_TYPES
}

static void free_tree_contexts(PC_TREE *tree) {
#if CONFIG_EXT_PARTITION_TYPES
  int i;
  for (i = 0; i < 3; i++) {
    free_mode_context(&tree->horizontala[i]);
    free_mode_context(&tree->horizontalb[i]);
    free_mode_context(&tree->verticala[i]);
    free_mode_context(&tree->verticalb[i]);
  }
  for (i = 0; i < 4; ++i) {
    free_mode_context(&tree->horizontal4[i]);
    free_mode_context(&tree->vertical4[i]);
  }
#endif  // CONFIG_EXT_PARTITION_TYPES
  free_mode_context(&tree->none);
  free_mode_context(&tree->horizontal[0]);
  free_mode_context(&tree->horizontal[1]);
  free_mode_context(&tree->vertical[0]);
  free_mode_context(&tree->vertical[1]);
#if CONFIG_SUPERTX
  free_mode_context(&tree->horizontal_supertx);
  free_mode_context(&tree->vertical_supertx);
  free_mode_context(&tree->split_supertx);
#if CONFIG_EXT_PARTITION_TYPES
  free_mode_context(&tree->horizontala_supertx);
  free_mode_context(&tree->horizontalb_supertx);
  free_mode_context(&tree->verticala_supertx);
  free_mode_context(&tree->verticalb_supertx);
#endif  // CONFIG_EXT_PARTITION_TYPES
#endif  // CONFIG_SUPERTX
}

// This function sets up a tree of contexts such that at each square
// partition level. There are contexts for none, horizontal, vertical, and
// split.  Along with a block_size value and a selected block_size which
// represents the state of our search.
void av1_setup_pc_tree(AV1_COMMON *cm, ThreadData *td) {
  int i, j;
#if CONFIG_CB4X4
#if CONFIG_EXT_PARTITION
  const int tree_nodes_inc = 1024;
#else
  const int tree_nodes_inc = 256;
#endif  // CONFIG_EXT_PARTITION
  const int leaf_factor = 4;
#else
  const int tree_nodes_inc = 0;
  const int leaf_factor = 1;
#endif
#if CONFIG_EXT_PARTITION
  const int leaf_nodes = 256 * leaf_factor;
  const int tree_nodes = tree_nodes_inc + 256 + 64 + 16 + 4 + 1;
#else
  const int leaf_nodes = 64 * leaf_factor;
  const int tree_nodes = tree_nodes_inc + 64 + 16 + 4 + 1;
#endif  // CONFIG_EXT_PARTITION
  int pc_tree_index = 0;
  PC_TREE *this_pc;
  int square_index = 1;
  int nodes;

#if !CONFIG_CB4X4
  aom_free(td->leaf_tree);
  CHECK_MEM_ERROR(cm, td->leaf_tree,
                  aom_calloc(leaf_nodes, sizeof(*td->leaf_tree)));
  PICK_MODE_CONTEXT *this_leaf = &td->leaf_tree[0];
#endif
  aom_free(td->pc_tree);
  CHECK_MEM_ERROR(cm, td->pc_tree,
                  aom_calloc(tree_nodes, sizeof(*td->pc_tree)));
  this_pc = &td->pc_tree[0];

#if !CONFIG_CB4X4
  // 4x4 blocks smaller than 8x8 but in the same 8x8 block share the same
  // context so we only need to allocate 1 for each 8x8 block.
  for (i = 0; i < leaf_nodes; ++i) {
#if CONFIG_EXT_PARTITION_TYPES
    alloc_mode_context(cm, 4, PARTITION_NONE, &td->leaf_tree[i]);
#else
    alloc_mode_context(cm, 16, &td->leaf_tree[i]);
#endif
  }
#endif

  // Sets up all the leaf nodes in the tree.
  for (pc_tree_index = 0; pc_tree_index < leaf_nodes; ++pc_tree_index) {
    PC_TREE *const tree = &td->pc_tree[pc_tree_index];
    tree->block_size = square[0];
#if CONFIG_CB4X4
    alloc_tree_contexts(cm, tree, 16);
#else
    alloc_tree_contexts(cm, tree, 4);
#endif
#if !CONFIG_CB4X4
    tree->leaf_split[0] = this_leaf++;
    for (j = 1; j < 4; j++) tree->leaf_split[j] = tree->leaf_split[0];
#endif
  }

  // Each node has 4 leaf nodes, fill each block_size level of the tree
  // from leafs to the root.
  for (nodes = leaf_nodes >> 2; nodes > 0; nodes >>= 2) {
    for (i = 0; i < nodes; ++i) {
      PC_TREE *const tree = &td->pc_tree[pc_tree_index];
#if CONFIG_CB4X4
      alloc_tree_contexts(cm, tree, 16 << (2 * square_index));
#else
      alloc_tree_contexts(cm, tree, 4 << (2 * square_index));
#endif
      tree->block_size = square[square_index];
      for (j = 0; j < 4; j++) tree->split[j] = this_pc++;
      ++pc_tree_index;
    }
    ++square_index;
  }

  // Set up the root node for the largest superblock size
  i = MAX_MIB_SIZE_LOG2 - MIN_MIB_SIZE_LOG2;
  td->pc_root[i] = &td->pc_tree[tree_nodes - 1];
  td->pc_root[i]->none.best_mode_index = 2;
  // Set up the root nodes for the rest of the possible superblock sizes
  while (--i >= 0) {
    td->pc_root[i] = td->pc_root[i + 1]->split[0];
    td->pc_root[i]->none.best_mode_index = 2;
  }
}

void av1_free_pc_tree(ThreadData *td) {
#if CONFIG_CB4X4
#if CONFIG_EXT_PARTITION
  const int tree_nodes_inc = 1024;
#else
  const int tree_nodes_inc = 256;
#endif  // CONFIG_EXT_PARTITION
#else
  const int tree_nodes_inc = 0;
#endif

#if CONFIG_EXT_PARTITION
  const int tree_nodes = tree_nodes_inc + 256 + 64 + 16 + 4 + 1;
#else
  const int tree_nodes = tree_nodes_inc + 64 + 16 + 4 + 1;
#endif  // CONFIG_EXT_PARTITION
  int i;
  for (i = 0; i < tree_nodes; ++i) free_tree_contexts(&td->pc_tree[i]);
  aom_free(td->pc_tree);
  td->pc_tree = NULL;
#if !CONFIG_CB4X4
  const int leaf_factor = 1;
#if CONFIG_EXT_PARTITION
  const int leaf_nodes = 256 * leaf_factor;
#else
  const int leaf_nodes = 64 * leaf_factor;
#endif  // CONFIG_EXT_PARTITION
  for (i = 0; i < leaf_nodes; ++i) free_mode_context(&td->leaf_tree[i]);
  aom_free(td->leaf_tree);
  td->leaf_tree = NULL;
#endif
}
