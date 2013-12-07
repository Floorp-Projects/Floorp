/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "vp9/encoder/vp9_treewriter.h"

static void cost(int *costs, vp9_tree tree, const vp9_prob *probs,
                 int i, int c) {
  const vp9_prob prob = probs[i / 2];
  int b;

  for (b = 0; b <= 1; ++b) {
    const int cc = c + vp9_cost_bit(prob, b);
    const vp9_tree_index ii = tree[i + b];

    if (ii <= 0)
      costs[-ii] = cc;
    else
      cost(costs, tree, probs, ii, cc);
  }
}

void vp9_cost_tokens(int *costs, const vp9_prob *probs, vp9_tree tree) {
  cost(costs, tree, probs, 0, 0);
}

void vp9_cost_tokens_skip(int *costs, const vp9_prob *probs, vp9_tree tree) {
  assert(tree[0] <= 0 && tree[1] > 0);

  costs[-tree[0]] = vp9_cost_bit(probs[0], 0);
  cost(costs, tree, probs, 2, 0);
}
