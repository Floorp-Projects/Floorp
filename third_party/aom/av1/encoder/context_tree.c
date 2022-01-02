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
  BLOCK_4X4, BLOCK_8X8, BLOCK_16X16, BLOCK_32X32, BLOCK_64X64, BLOCK_128X128,
};

static void alloc_mode_context(AV1_COMMON *cm, int num_pix,
                               PICK_MODE_CONTEXT *ctx) {
  const int num_planes = av1_num_planes(cm);
  int i;
  const int num_blk = num_pix / 16;
  ctx->num_4x4_blk = num_blk;

  CHECK_MEM_ERROR(cm, ctx->blk_skip, aom_calloc(num_blk, sizeof(uint8_t)));
  for (i = 0; i < num_planes; ++i) {
    CHECK_MEM_ERROR(cm, ctx->coeff[i],
                    aom_memalign(32, num_pix * sizeof(*ctx->coeff[i])));
    CHECK_MEM_ERROR(cm, ctx->qcoeff[i],
                    aom_memalign(32, num_pix * sizeof(*ctx->qcoeff[i])));
    CHECK_MEM_ERROR(cm, ctx->dqcoeff[i],
                    aom_memalign(32, num_pix * sizeof(*ctx->dqcoeff[i])));
    CHECK_MEM_ERROR(cm, ctx->eobs[i],
                    aom_memalign(32, num_blk * sizeof(*ctx->eobs[i])));
    CHECK_MEM_ERROR(
        cm, ctx->txb_entropy_ctx[i],
        aom_memalign(32, num_blk * sizeof(*ctx->txb_entropy_ctx[i])));
  }

  if (num_pix <= MAX_PALETTE_SQUARE) {
    for (i = 0; i < 2; ++i) {
      CHECK_MEM_ERROR(
          cm, ctx->color_index_map[i],
          aom_memalign(32, num_pix * sizeof(*ctx->color_index_map[i])));
    }
  }
}

static void free_mode_context(PICK_MODE_CONTEXT *ctx, const int num_planes) {
  int i;
  aom_free(ctx->blk_skip);
  ctx->blk_skip = 0;
  for (i = 0; i < num_planes; ++i) {
    aom_free(ctx->coeff[i]);
    ctx->coeff[i] = 0;
    aom_free(ctx->qcoeff[i]);
    ctx->qcoeff[i] = 0;
    aom_free(ctx->dqcoeff[i]);
    ctx->dqcoeff[i] = 0;
    aom_free(ctx->eobs[i]);
    ctx->eobs[i] = 0;
    aom_free(ctx->txb_entropy_ctx[i]);
    ctx->txb_entropy_ctx[i] = 0;
  }

  for (i = 0; i < 2; ++i) {
    aom_free(ctx->color_index_map[i]);
    ctx->color_index_map[i] = 0;
  }
}

static void alloc_tree_contexts(AV1_COMMON *cm, PC_TREE *tree, int num_pix,
                                int is_leaf) {
  alloc_mode_context(cm, num_pix, &tree->none);

  if (is_leaf) return;

  alloc_mode_context(cm, num_pix / 2, &tree->horizontal[0]);
  alloc_mode_context(cm, num_pix / 2, &tree->vertical[0]);

  alloc_mode_context(cm, num_pix / 2, &tree->horizontal[1]);
  alloc_mode_context(cm, num_pix / 2, &tree->vertical[1]);

  alloc_mode_context(cm, num_pix / 4, &tree->horizontala[0]);
  alloc_mode_context(cm, num_pix / 4, &tree->horizontala[1]);
  alloc_mode_context(cm, num_pix / 2, &tree->horizontala[2]);

  alloc_mode_context(cm, num_pix / 2, &tree->horizontalb[0]);
  alloc_mode_context(cm, num_pix / 4, &tree->horizontalb[1]);
  alloc_mode_context(cm, num_pix / 4, &tree->horizontalb[2]);

  alloc_mode_context(cm, num_pix / 4, &tree->verticala[0]);
  alloc_mode_context(cm, num_pix / 4, &tree->verticala[1]);
  alloc_mode_context(cm, num_pix / 2, &tree->verticala[2]);

  alloc_mode_context(cm, num_pix / 2, &tree->verticalb[0]);
  alloc_mode_context(cm, num_pix / 4, &tree->verticalb[1]);
  alloc_mode_context(cm, num_pix / 4, &tree->verticalb[2]);

  for (int i = 0; i < 4; ++i) {
    alloc_mode_context(cm, num_pix / 4, &tree->horizontal4[i]);
    alloc_mode_context(cm, num_pix / 4, &tree->vertical4[i]);
  }
}

static void free_tree_contexts(PC_TREE *tree, const int num_planes) {
  int i;
  for (i = 0; i < 3; i++) {
    free_mode_context(&tree->horizontala[i], num_planes);
    free_mode_context(&tree->horizontalb[i], num_planes);
    free_mode_context(&tree->verticala[i], num_planes);
    free_mode_context(&tree->verticalb[i], num_planes);
  }
  for (i = 0; i < 4; ++i) {
    free_mode_context(&tree->horizontal4[i], num_planes);
    free_mode_context(&tree->vertical4[i], num_planes);
  }
  free_mode_context(&tree->none, num_planes);
  free_mode_context(&tree->horizontal[0], num_planes);
  free_mode_context(&tree->horizontal[1], num_planes);
  free_mode_context(&tree->vertical[0], num_planes);
  free_mode_context(&tree->vertical[1], num_planes);
}

// This function sets up a tree of contexts such that at each square
// partition level. There are contexts for none, horizontal, vertical, and
// split.  Along with a block_size value and a selected block_size which
// represents the state of our search.
void av1_setup_pc_tree(AV1_COMMON *cm, ThreadData *td) {
  int i, j;
  const int tree_nodes_inc = 1024;
  const int leaf_factor = 4;
  const int leaf_nodes = 256 * leaf_factor;
  const int tree_nodes = tree_nodes_inc + 256 + 64 + 16 + 4 + 1;
  int pc_tree_index = 0;
  PC_TREE *this_pc;
  int square_index = 1;
  int nodes;

  aom_free(td->pc_tree);
  CHECK_MEM_ERROR(cm, td->pc_tree,
                  aom_calloc(tree_nodes, sizeof(*td->pc_tree)));
  this_pc = &td->pc_tree[0];

  // Sets up all the leaf nodes in the tree.
  for (pc_tree_index = 0; pc_tree_index < leaf_nodes; ++pc_tree_index) {
    PC_TREE *const tree = &td->pc_tree[pc_tree_index];
    tree->block_size = square[0];
    alloc_tree_contexts(cm, tree, 16, 1);
  }

  // Each node has 4 leaf nodes, fill each block_size level of the tree
  // from leafs to the root.
  for (nodes = leaf_nodes >> 2; nodes > 0; nodes >>= 2) {
    for (i = 0; i < nodes; ++i) {
      PC_TREE *const tree = &td->pc_tree[pc_tree_index];
      alloc_tree_contexts(cm, tree, 16 << (2 * square_index), 0);
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

void av1_free_pc_tree(ThreadData *td, const int num_planes) {
  if (td->pc_tree != NULL) {
    const int tree_nodes_inc = 1024;
    const int tree_nodes = tree_nodes_inc + 256 + 64 + 16 + 4 + 1;
    for (int i = 0; i < tree_nodes; ++i) {
      free_tree_contexts(&td->pc_tree[i], num_planes);
    }
    aom_free(td->pc_tree);
    td->pc_tree = NULL;
  }
}

void av1_copy_tree_context(PICK_MODE_CONTEXT *dst_ctx,
                           PICK_MODE_CONTEXT *src_ctx) {
  dst_ctx->mic = src_ctx->mic;
  dst_ctx->mbmi_ext = src_ctx->mbmi_ext;

  dst_ctx->num_4x4_blk = src_ctx->num_4x4_blk;
  dst_ctx->skip = src_ctx->skip;
  dst_ctx->skippable = src_ctx->skippable;
  dst_ctx->best_mode_index = src_ctx->best_mode_index;

  memcpy(dst_ctx->blk_skip, src_ctx->blk_skip,
         sizeof(uint8_t) * src_ctx->num_4x4_blk);

  dst_ctx->hybrid_pred_diff = src_ctx->hybrid_pred_diff;
  dst_ctx->comp_pred_diff = src_ctx->comp_pred_diff;
  dst_ctx->single_pred_diff = src_ctx->single_pred_diff;

  dst_ctx->rate = src_ctx->rate;
  dst_ctx->dist = src_ctx->dist;
  dst_ctx->rdcost = src_ctx->rdcost;
  dst_ctx->rd_mode_is_ready = src_ctx->rd_mode_is_ready;

  memcpy(dst_ctx->pred_mv, src_ctx->pred_mv, sizeof(MV) * REF_FRAMES);
  dst_ctx->pred_interp_filter = src_ctx->pred_interp_filter;

  dst_ctx->partition = src_ctx->partition;
}
