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

/* Trees map alphabets into huffman-like codes suitable for an arithmetic
   bit coder.  Timothy S Murphy  11 October 2004 */

#include "vp9/common/vp9_treecoder.h"

#include "vp9/encoder/vp9_boolhuff.h"       /* for now */


#define vp9_write_prob(w, v) vp9_write_literal((w), (v), 8)

/* Approximate length of an encoded bool in 256ths of a bit at given prob */

#define vp9_cost_zero(x) (vp9_prob_cost[x])
#define vp9_cost_one(x) vp9_cost_zero(vp9_complement(x))

#define vp9_cost_bit(x, b) vp9_cost_zero((b) ? vp9_complement(x) : (x))

/* VP8BC version is scaled by 2^20 rather than 2^8; see bool_coder.h */


/* Both of these return bits, not scaled bits. */
static INLINE unsigned int cost_branch256(const unsigned int ct[2],
                                          vp9_prob p) {
  return ct[0] * vp9_cost_zero(p) + ct[1] * vp9_cost_one(p);
}

static INLINE unsigned int cost_branch(const unsigned int ct[2],
                                       vp9_prob p) {
  return cost_branch256(ct, p) >> 8;
}


static INLINE void treed_write(vp9_writer *w,
                               vp9_tree tree, const vp9_prob *probs,
                               int bits, int len) {
  vp9_tree_index i = 0;

  do {
    const int bit = (bits >> --len) & 1;
    vp9_write(w, bit, probs[i >> 1]);
    i = tree[i + bit];
  } while (len);
}

static INLINE void write_token(vp9_writer *w, vp9_tree tree,
                               const vp9_prob *probs,
                               const struct vp9_token *token) {
  treed_write(w, tree, probs, token->value, token->len);
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

static INLINE int cost_token(vp9_tree tree, const vp9_prob *probs,
                             const struct vp9_token *token) {
  return treed_cost(tree, probs, token->value, token->len);
}

void vp9_cost_tokens(int *costs, const vp9_prob *probs, vp9_tree tree);
void vp9_cost_tokens_skip(int *costs, const vp9_prob *probs, vp9_tree tree);

#endif  // VP9_ENCODER_VP9_TREEWRITER_H_
