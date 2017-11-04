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

#ifndef AOM_DSP_BITWRITER_H_
#define AOM_DSP_BITWRITER_H_

#include <assert.h>
#include "./aom_config.h"

#if CONFIG_ANS
#include "aom_dsp/buf_ans.h"
#else
#include "aom_dsp/daalaboolwriter.h"
#endif
#include "aom_dsp/prob.h"

#if CONFIG_RD_DEBUG
#include "av1/common/blockd.h"
#include "av1/encoder/cost.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

#if CONFIG_ANS
typedef struct BufAnsCoder aom_writer;
#else
typedef struct daala_writer aom_writer;
#endif

typedef struct TOKEN_STATS {
  int cost;
#if CONFIG_VAR_TX
#if CONFIG_RD_DEBUG
  int txb_coeff_cost_map[TXB_COEFF_COST_MAP_SIZE][TXB_COEFF_COST_MAP_SIZE];
#endif
#endif
} TOKEN_STATS;

static INLINE void init_token_stats(TOKEN_STATS *token_stats) {
#if CONFIG_VAR_TX
#if CONFIG_RD_DEBUG
  int r, c;
  for (r = 0; r < TXB_COEFF_COST_MAP_SIZE; ++r) {
    for (c = 0; c < TXB_COEFF_COST_MAP_SIZE; ++c) {
      token_stats->txb_coeff_cost_map[r][c] = 0;
    }
  }
#endif
#endif
  token_stats->cost = 0;
}

static INLINE void aom_start_encode(aom_writer *bc, uint8_t *buffer) {
#if CONFIG_ANS
  aom_buf_ans_alloc(bc, /* error context*/ NULL);
  buf_ans_write_init(bc, buffer);
#else
  aom_daala_start_encode(bc, buffer);
#endif
}

static INLINE void aom_stop_encode(aom_writer *bc) {
#if CONFIG_ANS
  aom_buf_ans_flush(bc);
  bc->pos = buf_ans_write_end(bc);
#else
  aom_daala_stop_encode(bc);
#endif
}

static INLINE void aom_write(aom_writer *br, int bit, int probability) {
#if CONFIG_ANS
  buf_rabs_write(br, bit, probability);
#else
  aom_daala_write(br, bit, probability);
#endif
}

static INLINE void aom_write_record(aom_writer *br, int bit, int probability,
                                    TOKEN_STATS *token_stats) {
  aom_write(br, bit, probability);
#if CONFIG_RD_DEBUG
  token_stats->cost += av1_cost_bit(probability, bit);
#else
  (void)token_stats;
#endif
}

static INLINE void aom_write_bit(aom_writer *w, int bit) {
#if CONFIG_ANS
  buf_rabs_write_bit(w, bit);
#elif CONFIG_RAWBITS
  // Note this uses raw bits and is not the same as aom_daala_write(r, 128);
  aom_daala_write_bit(w, bit);
#else
  aom_write(w, bit, 128);  // aom_prob_half
#endif
}

static INLINE void aom_write_bit_record(aom_writer *w, int bit,
                                        TOKEN_STATS *token_stats) {
  aom_write_bit(w, bit);
#if CONFIG_RD_DEBUG
  token_stats->cost += av1_cost_bit(128, bit);  // aom_prob_half
#else
  (void)token_stats;
#endif
}

static INLINE void aom_write_literal(aom_writer *w, int data, int bits) {
  int bit;

  for (bit = bits - 1; bit >= 0; bit--) aom_write_bit(w, 1 & (data >> bit));
}

static INLINE void aom_write_cdf(aom_writer *w, int symb,
                                 const aom_cdf_prob *cdf, int nsymbs) {
#if CONFIG_ANS
  (void)nsymbs;
  assert(cdf);
  const aom_cdf_prob cum_prob = symb > 0 ? cdf[symb - 1] : 0;
  const aom_cdf_prob prob = cdf[symb] - cum_prob;
  buf_rans_write(w, cum_prob, prob);
#else
  daala_write_symbol(w, symb, cdf, nsymbs);
#endif
}

static INLINE void aom_write_symbol(aom_writer *w, int symb, aom_cdf_prob *cdf,
                                    int nsymbs) {
  aom_write_cdf(w, symb, cdf, nsymbs);
  update_cdf(cdf, symb, nsymbs);
}

#if CONFIG_LV_MAP
static INLINE void aom_write_bin(aom_writer *w, int symb, aom_cdf_prob *cdf,
                                 int nsymbs) {
  aom_write_cdf(w, symb, cdf, nsymbs);
  update_cdf(cdf, symb, nsymbs);
}
#endif

static INLINE void aom_write_tree_as_cdf(aom_writer *w,
                                         const aom_tree_index *tree,
                                         const aom_prob *probs, int bits,
                                         int len, aom_tree_index i) {
  aom_tree_index root;
  root = i;
  do {
    aom_cdf_prob cdf[16];
    aom_tree_index index[16];
    int path[16];
    int dist[16];
    int nsymbs;
    int symb;
    int j;
    /* Compute the CDF of the binary tree using the given probabilities. */
    nsymbs = tree_to_cdf(tree, probs, root, cdf, index, path, dist);
    /* Find the symbol to code. */
    symb = -1;
    for (j = 0; j < nsymbs; j++) {
      /* If this symbol codes a leaf node,  */
      if (index[j] <= 0) {
        if (len == dist[j] && path[j] == bits) {
          symb = j;
          break;
        }
      } else {
        if (len > dist[j] && path[j] == bits >> (len - dist[j])) {
          symb = j;
          break;
        }
      }
    }
    OD_ASSERT(symb != -1);
    aom_write_cdf(w, symb, cdf, nsymbs);
    bits &= (1 << (len - dist[symb])) - 1;
    len -= dist[symb];
  } while (len);
}

static INLINE void aom_write_tree(aom_writer *w, const aom_tree_index *tree,
                                  const aom_prob *probs, int bits, int len,
                                  aom_tree_index i) {
  aom_write_tree_as_cdf(w, tree, probs, bits, len, i);
}

static INLINE void aom_write_tree_record(aom_writer *w,
                                         const aom_tree_index *tree,
                                         const aom_prob *probs, int bits,
                                         int len, aom_tree_index i,
                                         TOKEN_STATS *token_stats) {
  (void)token_stats;
  aom_write_tree_as_cdf(w, tree, probs, bits, len, i);
}

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // AOM_DSP_BITWRITER_H_
