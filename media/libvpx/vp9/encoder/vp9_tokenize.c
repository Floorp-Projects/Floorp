/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#include <math.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "vp9/encoder/vp9_onyx_int.h"
#include "vp9/encoder/vp9_tokenize.h"
#include "vpx_mem/vpx_mem.h"

#include "vp9/common/vp9_pred_common.h"
#include "vp9/common/vp9_seg_common.h"
#include "vp9/common/vp9_entropy.h"

static TOKENVALUE dct_value_tokens[DCT_MAX_VALUE * 2];
const TOKENVALUE *vp9_dct_value_tokens_ptr;
static int dct_value_cost[DCT_MAX_VALUE * 2];
const int *vp9_dct_value_cost_ptr;

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

static const vp9_prob Pcat1[] = { 159};
static const vp9_prob Pcat2[] = { 165, 145};
static const vp9_prob Pcat3[] = { 173, 148, 140};
static const vp9_prob Pcat4[] = { 176, 155, 140, 135};
static const vp9_prob Pcat5[] = { 180, 157, 141, 134, 130};
static const vp9_prob Pcat6[] = {
  254, 254, 254, 252, 249, 243, 230, 196, 177, 153, 140, 133, 130, 129
};

static vp9_tree_index cat1[2], cat2[4], cat3[6], cat4[8], cat5[10], cat6[28];

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
}

const vp9_extra_bit vp9_extra_bits[ENTROPY_TOKENS] = {
  {0, 0, 0, 0},           // ZERO_TOKEN
  {0, 0, 0, 1},           // ONE_TOKEN
  {0, 0, 0, 2},           // TWO_TOKEN
  {0, 0, 0, 3},           // THREE_TOKEN
  {0, 0, 0, 4},           // FOUR_TOKEN
  {cat1, Pcat1, 1, 5},    // CATEGORY1_TOKEN
  {cat2, Pcat2, 2, 7},    // CATEGORY2_TOKEN
  {cat3, Pcat3, 3, 11},   // CATEGORY3_TOKEN
  {cat4, Pcat4, 4, 19},   // CATEGORY4_TOKEN
  {cat5, Pcat5, 5, 35},   // CATEGORY5_TOKEN
  {cat6, Pcat6, 14, 67},  // CATEGORY6_TOKEN
  {0, 0, 0, 0}            // EOB_TOKEN
};

struct vp9_token vp9_coef_encodings[ENTROPY_TOKENS];

void vp9_coef_tree_initialize() {
  init_bit_trees();
  vp9_tokens_from_tree(vp9_coef_encodings, vp9_coef_tree);
}

static void fill_value_tokens() {
  TOKENVALUE *const t = dct_value_tokens + DCT_MAX_VALUE;
  const vp9_extra_bit *const e = vp9_extra_bits;

  int i = -DCT_MAX_VALUE;
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
      const vp9_extra_bit *p = &vp9_extra_bits[t[i].token];

      if (p->base_val) {
        const int extra = t[i].extra;
        const int length = p->len;

        if (length)
          cost += treed_cost(p->tree, p->prob, extra >> 1, length);

        cost += vp9_cost_bit(vp9_prob_half, extra & 1); /* sign */
        dct_value_cost[i + DCT_MAX_VALUE] = cost;
      }
    }
  } while (++i < DCT_MAX_VALUE);

  vp9_dct_value_tokens_ptr = dct_value_tokens + DCT_MAX_VALUE;
  vp9_dct_value_cost_ptr = dct_value_cost + DCT_MAX_VALUE;
}

struct tokenize_b_args {
  VP9_COMP *cpi;
  MACROBLOCKD *xd;
  TOKENEXTRA **tp;
  uint8_t *token_cache;
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

static void tokenize_b(int plane, int block, BLOCK_SIZE plane_bsize,
                       TX_SIZE tx_size, void *arg) {
  struct tokenize_b_args* const args = arg;
  VP9_COMP *cpi = args->cpi;
  MACROBLOCKD *xd = args->xd;
  TOKENEXTRA **tp = args->tp;
  uint8_t *token_cache = args->token_cache;
  struct macroblock_plane *p = &cpi->mb.plane[plane];
  struct macroblockd_plane *pd = &xd->plane[plane];
  MB_MODE_INFO *mbmi = &xd->mi_8x8[0]->mbmi;
  int pt; /* near block/prev token context index */
  int c;
  TOKENEXTRA *t = *tp;        /* store tokens starting here */
  int eob = p->eobs[block];
  const PLANE_TYPE type = pd->plane_type;
  const int16_t *qcoeff_ptr = BLOCK_OFFSET(p->qcoeff, block);
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

  int aoff, loff;
  txfrm_block_to_raster_xy(plane_bsize, tx_size, block, &aoff, &loff);

  pt = get_entropy_context(tx_size, pd->above_context + aoff,
                           pd->left_context + loff);
  so = get_scan(xd, tx_size, type, block);
  scan = so->scan;
  nb = so->neighbors;
  c = 0;
  while (c < eob) {
    int v = 0;
    int skip_eob = 0;
    v = qcoeff_ptr[scan[c]];

    while (!v) {
      add_token_no_extra(&t, coef_probs[band[c]][pt], ZERO_TOKEN, skip_eob,
                         counts[band[c]][pt]);
      eob_branch[band[c]][pt] += !skip_eob;

      skip_eob = 1;
      token_cache[scan[c]] = 0;
      ++c;
      pt = get_coef_context(nb, token_cache, c);
      v = qcoeff_ptr[scan[c]];
    }

    add_token(&t, coef_probs[band[c]][pt],
              vp9_dct_value_tokens_ptr[v].extra,
              vp9_dct_value_tokens_ptr[v].token, skip_eob,
              counts[band[c]][pt]);
    eob_branch[band[c]][pt] += !skip_eob;

    token_cache[scan[c]] =
        vp9_pt_energy_class[vp9_dct_value_tokens_ptr[v].token];
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
  args->skippable[0] &= (!args->x->plane[plane].eobs[block]);
}

static int sb_is_skippable(MACROBLOCK *x, BLOCK_SIZE bsize) {
  int result = 1;
  struct is_skippable_args args = {x, &result};
  vp9_foreach_transformed_block(&x->e_mbd, bsize, is_skippable, &args);
  return result;
}

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
  MB_MODE_INFO *const mbmi = &xd->mi_8x8[0]->mbmi;
  TOKENEXTRA *t_backup = *t;
  const int ctx = vp9_get_skip_context(xd);
  const int skip_inc = !vp9_segfeature_active(&cm->seg, mbmi->segment_id,
                                              SEG_LVL_SKIP);
  struct tokenize_b_args arg = {cpi, xd, t, cpi->mb.token_cache};
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

void vp9_tokenize_initialize() {
  fill_value_tokens();
}
