/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VP9_COMMON_VP9_TREECODER_H_
#define VP9_COMMON_VP9_TREECODER_H_

#include "./vpx_config.h"
#include "vpx/vpx_integer.h"
#include "vp9/common/vp9_common.h"

typedef uint8_t vp9_prob;

#define vp9_prob_half ((vp9_prob) 128)

typedef int8_t vp9_tree_index;

#define TREE_SIZE(leaf_count) (2 * (leaf_count) - 2)

#define vp9_complement(x) (255 - x)

/* We build coding trees compactly in arrays.
   Each node of the tree is a pair of vp9_tree_indices.
   Array index often references a corresponding probability table.
   Index <= 0 means done encoding/decoding and value = -Index,
   Index > 0 means need another bit, specification at index.
   Nonnegative indices are always even;  processing begins at node 0. */

typedef const vp9_tree_index vp9_tree[];

struct vp9_token {
  int value;
  int len;
};

/* Construct encoding array from tree. */

void vp9_tokens_from_tree(struct vp9_token*, vp9_tree);

/* Convert array of token occurrence counts into a table of probabilities
   for the associated binary encoding tree.  Also writes count of branches
   taken for each node on the tree; this facilitiates decisions as to
   probability updates. */

void vp9_tree_probs_from_distribution(vp9_tree tree,
                                      unsigned int branch_ct[ /* n - 1 */ ][2],
                                      const unsigned int num_events[ /* n */ ]);


static INLINE vp9_prob clip_prob(int p) {
  return (p > 255) ? 255u : (p < 1) ? 1u : p;
}

// int64 is not needed for normal frame level calculations.
// However when outputing entropy stats accumulated over many frames
// or even clips we can overflow int math.
#ifdef ENTROPY_STATS
static INLINE vp9_prob get_prob(int num, int den) {
  return (den == 0) ? 128u : clip_prob(((int64_t)num * 256 + (den >> 1)) / den);
}
#else
static INLINE vp9_prob get_prob(int num, int den) {
  return (den == 0) ? 128u : clip_prob((num * 256 + (den >> 1)) / den);
}
#endif

static INLINE vp9_prob get_binary_prob(int n0, int n1) {
  return get_prob(n0, n0 + n1);
}

/* this function assumes prob1 and prob2 are already within [1,255] range */
static INLINE vp9_prob weighted_prob(int prob1, int prob2, int factor) {
  return ROUND_POWER_OF_TWO(prob1 * (256 - factor) + prob2 * factor, 8);
}

static INLINE vp9_prob merge_probs(vp9_prob pre_prob,
                                   const unsigned int ct[2],
                                   unsigned int count_sat,
                                   unsigned int max_update_factor) {
  const vp9_prob prob = get_binary_prob(ct[0], ct[1]);
  const unsigned int count = MIN(ct[0] + ct[1], count_sat);
  const unsigned int factor = max_update_factor * count / count_sat;
  return weighted_prob(pre_prob, prob, factor);
}

static unsigned int tree_merge_probs_impl(unsigned int i,
                                          const vp9_tree_index *tree,
                                          const vp9_prob *pre_probs,
                                          const unsigned int *counts,
                                          unsigned int count_sat,
                                          unsigned int max_update_factor,
                                          vp9_prob *probs) {
  const int l = tree[i];
  const unsigned int left_count = (l <= 0)
                 ? counts[-l]
                 : tree_merge_probs_impl(l, tree, pre_probs, counts,
                                         count_sat, max_update_factor, probs);
  const int r = tree[i + 1];
  const unsigned int right_count = (r <= 0)
                 ? counts[-r]
                 : tree_merge_probs_impl(r, tree, pre_probs, counts,
                                         count_sat, max_update_factor, probs);
  const unsigned int ct[2] = { left_count, right_count };
  probs[i >> 1] = merge_probs(pre_probs[i >> 1], ct,
                              count_sat, max_update_factor);
  return left_count + right_count;
}

static void tree_merge_probs(const vp9_tree_index *tree,
                             const vp9_prob *pre_probs,
                             const unsigned int *counts,
                             unsigned int count_sat,
                             unsigned int max_update_factor, vp9_prob *probs) {
  tree_merge_probs_impl(0, tree, pre_probs, counts,
                        count_sat, max_update_factor, probs);
}


#endif  // VP9_COMMON_VP9_TREECODER_H_
