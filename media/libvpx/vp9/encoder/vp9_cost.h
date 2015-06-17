/*
 *  Copyright (c) 2014 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VP9_ENCODER_VP9_COST_H_
#define VP9_ENCODER_VP9_COST_H_

#include "vp9/common/vp9_prob.h"

#ifdef __cplusplus
extern "C" {
#endif

extern const unsigned int vp9_prob_cost[256];

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

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // VP9_ENCODER_VP9_COST_H_
