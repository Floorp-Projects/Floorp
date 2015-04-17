/*
 *  Copyright (c) 2013 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VP9_COMMON_VP9_PROB_H_
#define VP9_COMMON_VP9_PROB_H_

#include "./vpx_config.h"

#include "vpx_ports/mem.h"
#include "vpx/vpx_integer.h"

#include "vp9/common/vp9_common.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef uint8_t vp9_prob;

#define MAX_PROB 255

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

static INLINE vp9_prob clip_prob(int p) {
  return (p > 255) ? 255u : (p < 1) ? 1u : p;
}

// int64 is not needed for normal frame level calculations.
// However when outputting entropy stats accumulated over many frames
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

/* This function assumes prob1 and prob2 are already within [1,255] range. */
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

void vp9_tree_merge_probs(const vp9_tree_index *tree, const vp9_prob *pre_probs,
                          const unsigned int *counts, unsigned int count_sat,
                          unsigned int max_update_factor, vp9_prob *probs);


DECLARE_ALIGNED(16, extern const uint8_t, vp9_norm[256]);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // VP9_COMMON_VP9_PROB_H_
