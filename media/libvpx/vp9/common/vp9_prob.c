/*
 *  Copyright (c) 2013 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "vp9/common/vp9_prob.h"

DECLARE_ALIGNED(16, const uint8_t, vp9_norm[256]) = {
  0, 7, 6, 6, 5, 5, 5, 5, 4, 4, 4, 4, 4, 4, 4, 4,
  3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
  2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
  2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};


static unsigned int tree_merge_probs_impl(unsigned int i,
                                          const vp9_tree_index *tree,
                                          const vp9_prob *pre_probs,
                                          const unsigned int *counts,
                                          unsigned int count_sat,
                                          unsigned int max_update,
                                          vp9_prob *probs) {
  const int l = tree[i];
  const unsigned int left_count = (l <= 0)
                 ? counts[-l]
                 : tree_merge_probs_impl(l, tree, pre_probs, counts,
                                         count_sat, max_update, probs);
  const int r = tree[i + 1];
  const unsigned int right_count = (r <= 0)
                 ? counts[-r]
                 : tree_merge_probs_impl(r, tree, pre_probs, counts,
                                         count_sat, max_update, probs);
  const unsigned int ct[2] = { left_count, right_count };
  probs[i >> 1] = merge_probs(pre_probs[i >> 1], ct,
                              count_sat, max_update);
  return left_count + right_count;
}

void vp9_tree_merge_probs(const vp9_tree_index *tree, const vp9_prob *pre_probs,
                          const unsigned int *counts, unsigned int count_sat,
                          unsigned int max_update_factor, vp9_prob *probs) {
  tree_merge_probs_impl(0, tree, pre_probs, counts, count_sat,
                        max_update_factor, probs);
}
