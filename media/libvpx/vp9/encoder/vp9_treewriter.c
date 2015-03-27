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

static void tree2tok(struct vp9_token *tokens, const vp9_tree_index *tree,
                     int i, int v, int l) {
  v += v;
  ++l;

  do {
    const vp9_tree_index j = tree[i++];
    if (j <= 0) {
      tokens[-j].value = v;
      tokens[-j].len = l;
    } else {
      tree2tok(tokens, tree, j, v, l);
    }
  } while (++v & 1);
}

void vp9_tokens_from_tree(struct vp9_token *tokens,
                          const vp9_tree_index *tree) {
  tree2tok(tokens, tree, 0, 0, 0);
}

static unsigned int convert_distribution(unsigned int i, vp9_tree tree,
                                         unsigned int branch_ct[][2],
                                         const unsigned int num_events[]) {
  unsigned int left, right;

  if (tree[i] <= 0)
    left = num_events[-tree[i]];
  else
    left = convert_distribution(tree[i], tree, branch_ct, num_events);

  if (tree[i + 1] <= 0)
    right = num_events[-tree[i + 1]];
  else
    right = convert_distribution(tree[i + 1], tree, branch_ct, num_events);

  branch_ct[i >> 1][0] = left;
  branch_ct[i >> 1][1] = right;
  return left + right;
}

void vp9_tree_probs_from_distribution(vp9_tree tree,
                                      unsigned int branch_ct[/* n-1 */][2],
                                      const unsigned int num_events[/* n */]) {
  convert_distribution(0, tree, branch_ct, num_events);
}
