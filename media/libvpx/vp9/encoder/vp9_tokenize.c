/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "vpx_mem/vpx_mem.h"

#include "vp9/common/vp9_entropy.h"
#include "vp9/common/vp9_pred_common.h"
#include "vp9/common/vp9_seg_common.h"

#include "vp9/encoder/vp9_cost.h"
#include "vp9/encoder/vp9_encoder.h"
#include "vp9/encoder/vp9_tokenize.h"

static TOKENVALUE dct_value_tokens[DCT_MAX_VALUE * 2];
const TOKENVALUE *vp9_dct_value_tokens_ptr;
static int16_t dct_value_cost[DCT_MAX_VALUE * 2];
const int16_t *vp9_dct_value_cost_ptr;

#if CONFIG_VP9_HIGHBITDEPTH
static TOKENVALUE dct_value_tokens_high10[DCT_MAX_VALUE_HIGH10 * 2];
const TOKENVALUE *vp9_dct_value_tokens_high10_ptr;
static int16_t dct_value_cost_high10[DCT_MAX_VALUE_HIGH10 * 2];
const int16_t *vp9_dct_value_cost_high10_ptr;

static TOKENVALUE dct_value_tokens_high12[DCT_MAX_VALUE_HIGH12 * 2];
const TOKENVALUE *vp9_dct_value_tokens_high12_ptr;
static int16_t dct_value_cost_high12[DCT_MAX_VALUE_HIGH12 * 2];
const int16_t *vp9_dct_value_cost_high12_ptr;
#endif

// Array indices are identical to previously-existing CONTEXT_NODE indices
const vp9_tree_index vp9_coef_tree[TREE_SIZE(ENTROPY_TOKENS)] = {
  -EOB_TOKEN, 2,                       // 0  = EOB
  -ZERO_TOKEN, 4,                      // 1  = ZERO
  -ONE_TOKEN, 6,                       // 2  = ONE
  8, 12,                               // 3  = LOW_VAL
  -TWO_TOKEN, 10,                      // 4  = TWO
  -THREE_TOKEN, -FOUR_TOKEN,           // 5  = THREE
  14, 16,                              // 6  = HIGH_LOW
  -CATEGORY1_TOKEN, -CATEGORY2_TOKEN,  // 7  = CAT_ONE
  18, 20,                              // 8  = CAT_THREEFOUR
  -CATEGORY3_TOKEN, -CATEGORY4_TOKEN,  // 9  = CAT_THREE
  -CATEGORY5_TOKEN, -CATEGORY6_TOKEN   // 10 = CAT_FIVE
};

// Unconstrained Node Tree
const vp9_tree_index vp9_coef_con_tree[TREE_SIZE(ENTROPY_TOKENS)] = {
  2, 6,                                // 0 = LOW_VAL
  -TWO_TOKEN, 4,                       // 1 = TWO
  -THREE_TOKEN, -FOUR_TOKEN,           // 2 = THREE
  8, 10,                               // 3 = HIGH_LOW
  -CATEGORY1_TOKEN, -CATEGORY2_TOKEN,  // 4 = CAT_ONE
  12, 14,                              // 5 = CAT_THREEFOUR
  -CATEGORY3_TOKEN, -CATEGORY4_TOKEN,  // 6 = CAT_THREE
  -CATEGORY5_TOKEN, -CATEGORY6_TOKEN   // 7 = CAT_FIVE
};

static vp9_tree_index cat1[2], cat2[4], cat3[6], cat4[8], cat5[10], cat6[28];

#if CONFIG_VP9_HIGHBITDEPTH
static vp9_tree_index cat1_high10[2];
static vp9_tree_index cat2_high10[4];
static vp9_tree_index cat3_high10[6];
static vp9_tree_index cat4_high10[8];
static vp9_tree_index cat5_high10[10];
static vp9_tree_index cat6_high10[32];
static vp9_tree_index cat1_high12[2];
static vp9_tree_index cat2_high12[4];
static vp9_tree_index cat3_high12[6];
static vp9_tree_index cat4_high12[8];
static vp9_tree_index cat5_high12[10];
static vp9_tree_index cat6_high12[36];
#endif

static void init_bit_tree(vp9_tree_index *p, int n) {
  int i = 0;

  while (++i < n) {
    p[0] = p[1] = i << 1;
    p += 2;
  }

  p[0] = p[1] = 0;
}

static void init_bit_trees() {
  init_bit_tree(cat1, 1);
  init_bit_tree(cat2, 2);
  init_bit_tree(cat3, 3);
  init_bit_tree(cat4, 4);
  init_bit_tree(cat5, 5);
  init_bit_tree(cat6, 14);
#if CONFIG_VP9_HIGHBITDEPTH
  init_bit_tree(cat1_high10, 1);
  init_bit_tree(cat2_high10, 2);
  init_bit_tree(cat3_high10, 3);
  init_bit_tree(cat4_high10, 4);
  init_bit_tree(cat5_high10, 5);
  init_bit_tree(cat6_high10, 16);
  init_bit_tree(cat1_high12, 1);
  init_bit_tree(cat2_high12, 2);
  init_bit_tree(cat3_high12, 3);
  init_bit_tree(cat4_high12, 4);
  init_bit_tree(cat5_high12, 5);
  init_bit_tree(cat6_high12, 18);
#endif
}

const vp9_extra_bit vp9_extra_bits[ENTROPY_TOKENS] = {
  {0, 0, 0, 0},                              // ZERO_TOKEN
  {0, 0, 0, 1},                              // ONE_TOKEN
  {0, 0, 0, 2},                              // TWO_TOKEN
  {0, 0, 0, 3},                              // THREE_TOKEN
  {0, 0, 0, 4},                              // FOUR_TOKEN
  {cat1, vp9_cat1_prob, 1,  CAT1_MIN_VAL},   // CATEGORY1_TOKEN
  {cat2, vp9_cat2_prob, 2,  CAT2_MIN_VAL},   // CATEGORY2_TOKEN
  {cat3, vp9_cat3_prob, 3,  CAT3_MIN_VAL},   // CATEGORY3_TOKEN
  {cat4, vp9_cat4_prob, 4,  CAT4_MIN_VAL},   // CATEGORY4_TOKEN
  {cat5, vp9_cat5_prob, 5,  CAT5_MIN_VAL},   // CATEGORY5_TOKEN
  {cat6, vp9_cat6_prob, 14, CAT6_MIN_VAL},   // CATEGORY6_TOKEN
  {0, 0, 0, 0}                               // EOB_TOKEN
};

#if CONFIG_VP9_HIGHBITDEPTH
const vp9_extra_bit vp9_extra_bits_high10[ENTROPY_TOKENS] = {
  {0, 0, 0, 0},                                            // ZERO_TOKEN
  {0, 0, 0, 1},                                            // ONE_TOKEN
  {0, 0, 0, 2},                                            // TWO_TOKEN
  {0, 0, 0, 3},                                            // THREE_TOKEN
  {0, 0, 0, 4},                                            // FOUR_TOKEN
  {cat1_high10, vp9_cat1_prob_high10, 1,  CAT1_MIN_VAL},   // CATEGORY1_TOKEN
  {cat2_high10, vp9_cat2_prob_high10, 2,  CAT2_MIN_VAL},   // CATEGORY2_TOKEN
  {cat3_high10, vp9_cat3_prob_high10, 3,  CAT3_MIN_VAL},   // CATEGORY3_TOKEN
  {cat4_high10, vp9_cat4_prob_high10, 4,  CAT4_MIN_VAL},   // CATEGORY4_TOKEN
  {cat5_high10, vp9_cat5_prob_high10, 5,  CAT5_MIN_VAL},   // CATEGORY5_TOKEN
  {cat6_high10, vp9_cat6_prob_high10, 16, CAT6_MIN_VAL},   // CATEGORY6_TOKEN
  {0, 0, 0, 0}                                             // EOB_TOKEN
};
const vp9_extra_bit vp9_extra_bits_high12[ENTROPY_TOKENS] = {
  {0, 0, 0, 0},                                            // ZERO_TOKEN
  {0, 0, 0, 1},                                            // ONE_TOKEN
  {0, 0, 0, 2},                                            // TWO_TOKEN
  {0, 0, 0, 3},                                            // THREE_TOKEN
  {0, 0, 0, 4},                                            // FOUR_TOKEN
  {cat1_high12, vp9_cat1_prob_high12, 1,  CAT1_MIN_VAL},   // CATEGORY1_TOKEN
  {cat2_high12, vp9_cat2_prob_high12, 2,  CAT2_MIN_VAL},   // CATEGORY2_TOKEN
  {cat3_high12, vp9_cat3_prob_high12, 3,  CAT3_MIN_VAL},   // CATEGORY3_TOKEN
  {cat4_high12, vp9_cat4_prob_high12, 4,  CAT4_MIN_VAL},   // CATEGORY4_TOKEN
  {cat5_high12, vp9_cat5_prob_high12, 5,  CAT5_MIN_VAL},   // CATEGORY5_TOKEN
  {cat6_high12, vp9_cat6_prob_high12, 18, CAT6_MIN_VAL},   // CATEGORY6_TOKEN
  {0, 0, 0, 0}                                             // EOB_TOKEN
};
#endif

struct vp9_token vp9_coef_encodings[ENTROPY_TOKENS];

void vp9_coef_tree_initialize() {
  init_bit_trees();
  vp9_tokens_from_tree(vp9_coef_encodings, vp9_coef_tree);
}

static void tokenize_init_one(TOKENVALUE *t, const vp9_extra_bit *const e,
                              int16_t *value_cost, int max_value) {
  int i = -max_value;
  int sign = 1;

  do {
    if (!i)
      sign = 0;

    {
      const int a = sign ? -i : i;
      int eb = sign;

      if (a > 4) {
        int j = 4;

        while (++j < 11  &&  e[j].base_val <= a) {}

        t[i].token = --j;
        eb |= (a - e[j].base_val) << 1;
      } else {
        t[i].token = a;
      }
      t[i].extra = eb;
    }

    // initialize the cost for extra bits for all possible coefficient value.
    {
      int cost = 0;
      const vp9_extra_bit *p = &e[t[i].token];

      if (p->base_val) {
        const int extra = t[i].extra;
        const int length = p->len;

        if (length)
          cost += treed_cost(p->tree, p->prob, extra >> 1, length);

        cost += vp9_cost_bit(vp9_prob_half, extra & 1); /* sign */
        value_cost[i] = cost;
      }
    }
  } while (++i < max_value);
}

void vp9_tokenize_initialize() {
  vp9_dct_value_tokens_ptr = dct_value_tokens + DCT_MAX_VALUE;
  vp9_dct_value_cost_ptr = dct_value_cost + DCT_MAX_VALUE;

  tokenize_init_one(dct_value_tokens + DCT_MAX_VALUE, vp9_extra_bits,
                    dct_value_cost + DCT_MAX_VALUE, DCT_MAX_VALUE);
#if CONFIG_VP9_HIGHBITDEPTH
  vp9_dct_value_tokens_high10_ptr = dct_value_tokens_high10 +
      DCT_MAX_VALUE_HIGH10;
  vp9_dct_value_cost_high10_ptr = dct_value_cost_high10 + DCT_MAX_VALUE_HIGH10;

  tokenize_init_one(dct_value_tokens_high10 + DCT_MAX_VALUE_HIGH10,
                    vp9_extra_bits_high10,
                    dct_value_cost_high10 + DCT_MAX_VALUE_HIGH10,
                    DCT_MAX_VALUE_HIGH10);
  vp9_dct_value_tokens_high12_ptr = dct_value_tokens_high12 +
      DCT_MAX_VALUE_HIGH12;
  vp9_dct_value_cost_high12_ptr = dct_value_cost_high12 + DCT_MAX_VALUE_HIGH12;

  tokenize_init_one(dct_value_tokens_high12 + DCT_MAX_VALUE_HIGH12,
                    vp9_extra_bits_high12,
                    dct_value_cost_high12 + DCT_MAX_VALUE_HIGH12,
                    DCT_MAX_VALUE_HIGH12);
#endif
}

struct tokenize_b_args {
  VP9_COMP *cpi;
  MACROBLOCKD *xd;
  TOKENEXTRA **tp;
};

static void set_entropy_context_b(int plane, int block, BLOCK_SIZE plane_bsize,
                                  TX_SIZE tx_size, void *arg) {
  struct tokenize_b_args* const args = arg;
  MACROBLOCKD *const xd = args->xd;
  struct macroblock_plane *p = &args->cpi->mb.plane[plane];
  struct macroblockd_plane *pd = &xd->plane[plane];
  int aoff, loff;
  txfrm_block_to_raster_xy(plane_bsize, tx_size, block, &aoff, &loff);
  vp9_set_contexts(xd, pd, plane_bsize, tx_size, p->eobs[block] > 0,
                   aoff, loff);
}

static INLINE void add_token(TOKENEXTRA **t, const vp9_prob *context_tree,
                             int16_t extra, uint8_t token,
                             uint8_t skip_eob_node,
                             unsigned int *counts) {
  (*t)->token = token;
  (*t)->extra = extra;
  (*t)->context_tree = context_tree;
  (*t)->skip_eob_node = skip_eob_node;
  (*t)++;
  ++counts[token];
}

static INLINE void add_token_no_extra(TOKENEXTRA **t,
                                      const vp9_prob *context_tree,
                                      uint8_t token,
                                      uint8_t skip_eob_node,
                                      unsigned int *counts) {
  (*t)->token = token;
  (*t)->context_tree = context_tree;
  (*t)->skip_eob_node = skip_eob_node;
  (*t)++;
  ++counts[token];
}

static INLINE int get_tx_eob(const struct segmentation *seg, int segment_id,
                             TX_SIZE tx_size) {
  const int eob_max = 16 << (tx_size << 1);
  return vp9_segfeature_active(seg, segment_id, SEG_LVL_SKIP) ? 0 : eob_max;
}

static void tokenize_b(int plane, int block, BLOCK_SIZE plane_bsize,
                       TX_SIZE tx_size, void *arg) {
  struct tokenize_b_args* const args = arg;
  VP9_COMP *cpi = args->cpi;
  MACROBLOCKD *xd = args->xd;
  TOKENEXTRA **tp = args->tp;
  uint8_t token_cache[32 * 32];
  struct macroblock_plane *p = &cpi->mb.plane[plane];
  struct macroblockd_plane *pd = &xd->plane[plane];
  MB_MODE_INFO *mbmi = &xd->mi[0].src_mi->mbmi;
  int pt; /* near block/prev token context index */
  int c;
  TOKENEXTRA *t = *tp;        /* store tokens starting here */
  int eob = p->eobs[block];
  const PLANE_TYPE type = pd->plane_type;
  const tran_low_t *qcoeff = BLOCK_OFFSET(p->qcoeff, block);
  const int segment_id = mbmi->segment_id;
  const int16_t *scan, *nb;
  const scan_order *so;
  const int ref = is_inter_block(mbmi);
  unsigned int (*const counts)[COEFF_CONTEXTS][ENTROPY_TOKENS] =
      cpi->coef_counts[tx_size][type][ref];
  vp9_prob (*const coef_probs)[COEFF_CONTEXTS][UNCONSTRAINED_NODES] =
      cpi->common.fc.coef_probs[tx_size][type][ref];
  unsigned int (*const eob_branch)[COEFF_CONTEXTS] =
      cpi->common.counts.eob_branch[tx_size][type][ref];
  const uint8_t *const band = get_band_translate(tx_size);
  const int seg_eob = get_tx_eob(&cpi->common.seg, segment_id, tx_size);
  const TOKENVALUE *dct_value_tokens;

  int aoff, loff;
  txfrm_block_to_raster_xy(plane_bsize, tx_size, block, &aoff, &loff);

  pt = get_entropy_context(tx_size, pd->above_context + aoff,
                           pd->left_context + loff);
  so = get_scan(xd, tx_size, type, block);
  scan = so->scan;
  nb = so->neighbors;
  c = 0;
#if CONFIG_VP9_HIGH && CONFIG_HIGH_QUANT
  if (cpi->common.profile >= PROFILE_2) {
    dct_value_tokens = (cpi->common.bit_depth == VPX_BITS_10 ?
                        vp9_dct_value_tokens_high10_ptr :
                        vp9_dct_value_tokens_high12_ptr);
  } else {
    dct_value_tokens = vp9_dct_value_tokens_ptr;
  }
#else
  dct_value_tokens = vp9_dct_value_tokens_ptr;
#endif

  while (c < eob) {
    int v = 0;
    int skip_eob = 0;
    v = qcoeff[scan[c]];

    while (!v) {
      add_token_no_extra(&t, coef_probs[band[c]][pt], ZERO_TOKEN, skip_eob,
                         counts[band[c]][pt]);
      eob_branch[band[c]][pt] += !skip_eob;

      skip_eob = 1;
      token_cache[scan[c]] = 0;
      ++c;
      pt = get_coef_context(nb, token_cache, c);
      v = qcoeff[scan[c]];
    }

    add_token(&t, coef_probs[band[c]][pt],
              dct_value_tokens[v].extra,
              (uint8_t)dct_value_tokens[v].token,
              (uint8_t)skip_eob,
              counts[band[c]][pt]);
    eob_branch[band[c]][pt] += !skip_eob;

    token_cache[scan[c]] = vp9_pt_energy_class[dct_value_tokens[v].token];
    ++c;
    pt = get_coef_context(nb, token_cache, c);
  }
  if (c < seg_eob) {
    add_token_no_extra(&t, coef_probs[band[c]][pt], EOB_TOKEN, 0,
                       counts[band[c]][pt]);
    ++eob_branch[band[c]][pt];
  }

  *tp = t;

  vp9_set_contexts(xd, pd, plane_bsize, tx_size, c > 0, aoff, loff);
}

struct is_skippable_args {
  MACROBLOCK *x;
  int *skippable;
};
static void is_skippable(int plane, int block,
                         BLOCK_SIZE plane_bsize, TX_SIZE tx_size,
                         void *argv) {
  struct is_skippable_args *args = argv;
  (void)plane_bsize;
  (void)tx_size;
  args->skippable[0] &= (!args->x->plane[plane].eobs[block]);
}

// TODO(yaowu): rewrite and optimize this function to remove the usage of
//              vp9_foreach_transform_block() and simplify is_skippable().
int vp9_is_skippable_in_plane(MACROBLOCK *x, BLOCK_SIZE bsize, int plane) {
  int result = 1;
  struct is_skippable_args args = {x, &result};
  vp9_foreach_transformed_block_in_plane(&x->e_mbd, bsize, plane, is_skippable,
                                         &args);
  return result;
}

void vp9_tokenize_sb(VP9_COMP *cpi, TOKENEXTRA **t, int dry_run,
                     BLOCK_SIZE bsize) {
  VP9_COMMON *const cm = &cpi->common;
  MACROBLOCKD *const xd = &cpi->mb.e_mbd;
  MB_MODE_INFO *const mbmi = &xd->mi[0].src_mi->mbmi;
  TOKENEXTRA *t_backup = *t;
  const int ctx = vp9_get_skip_context(xd);
  const int skip_inc = !vp9_segfeature_active(&cm->seg, mbmi->segment_id,
                                              SEG_LVL_SKIP);
  struct tokenize_b_args arg = {cpi, xd, t};
  if (mbmi->skip) {
    if (!dry_run)
      cm->counts.skip[ctx][1] += skip_inc;
    reset_skip_context(xd, bsize);
    if (dry_run)
      *t = t_backup;
    return;
  }

  if (!dry_run) {
    cm->counts.skip[ctx][0] += skip_inc;
    vp9_foreach_transformed_block(xd, bsize, tokenize_b, &arg);
  } else {
    vp9_foreach_transformed_block(xd, bsize, set_entropy_context_b, &arg);
    *t = t_backup;
  }
}
