/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VP9_ENCODER_VP9_TREEWRITER_H_
#define VP9_ENCODER_VP9_TREEWRITER_H_

#include "vp9/encoder/vp9_writer.h"

#ifdef __cplusplus
extern "C" {
#endif

#define vp9_cost_zero(prob) (vp9_prob_cost[prob])

#define vp9_cost_one(prob) vp9_cost_zero(vp9_complement(prob))

#define vp9_cost_bit(prob, bit) vp9_cost_zero((bit) ? vp9_complement(prob) \
                                                    : (prob))

static INLINE unsigned int cost_branch256(const unsigned int ct[2],
                                          vp9_prob p) {
  return ct[0] * vp9_cost_zero(p) + ct[1] * vp9_cost_one(p);
}

static INLINE int treed_cost(vp9_tree tree, const vp9_prob *probs,
                             int bits, int len) {
  int cost = 0;
  vp9_tree_index i = 0;

  do {
    const int bit = (bits >> --len) & 1;
    cost += vp9_cost_bit(probs[i >> 1], bit);
    i = tree[i + bit];
  } while (len);

  return cost;
}

void vp9_cost_tokens(int *costs, const vp9_prob *probs, vp9_tree tree);
void vp9_cost_tokens_skip(int *costs, const vp9_prob *probs, vp9_tree tree);

void vp9_tree_probs_from_distribution(vp9_tree tree,
                                      unsigned int branch_ct[ /* n - 1 */ ][2],
                                      const unsigned int num_events[ /* n */ ]);

struct vp9_token {
  int value;
  int len;
};

void vp9_tokens_from_tree(struct vp9_token*, const vp9_tree_index *);

static INLINE void vp9_write_tree(vp9_writer *w, const vp9_tree_index *tree,
                                  const vp9_prob *probs, int bits, int len,
                                  vp9_tree_index i) {
  do {
    const int bit = (bits >> --len) & 1;
    vp9_write(w, bit, probs[i >> 1]);
    i = tree[i + bit];
  } while (len);
}

static INLINE void vp9_write_token(vp9_writer *w, const vp9_tree_index *tree,
                                   const vp9_prob *probs,
                                   const struct vp9_token *token) {
  vp9_write_tree(w, tree, probs, token->value, token->len, 0);
}

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // VP9_ENCODER_VP9_TREEWRITER_H_
