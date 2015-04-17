/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VP9_COMMON_VP9_ENTROPYMODE_H_
#define VP9_COMMON_VP9_ENTROPYMODE_H_

#include "vp9/common/vp9_blockd.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TX_SIZE_CONTEXTS 2
#define SWITCHABLE_FILTERS 3   // number of switchable filters
#define SWITCHABLE_FILTER_CONTEXTS (SWITCHABLE_FILTERS + 1)

struct VP9Common;

struct tx_probs {
  vp9_prob p32x32[TX_SIZE_CONTEXTS][TX_SIZES - 1];
  vp9_prob p16x16[TX_SIZE_CONTEXTS][TX_SIZES - 2];
  vp9_prob p8x8[TX_SIZE_CONTEXTS][TX_SIZES - 3];
};

struct tx_counts {
  unsigned int p32x32[TX_SIZE_CONTEXTS][TX_SIZES];
  unsigned int p16x16[TX_SIZE_CONTEXTS][TX_SIZES - 1];
  unsigned int p8x8[TX_SIZE_CONTEXTS][TX_SIZES - 2];
};

extern const vp9_prob vp9_kf_uv_mode_prob[INTRA_MODES][INTRA_MODES - 1];
extern const vp9_prob vp9_kf_y_mode_prob[INTRA_MODES][INTRA_MODES]
                                        [INTRA_MODES - 1];
extern const vp9_prob vp9_kf_partition_probs[PARTITION_CONTEXTS]
                                            [PARTITION_TYPES - 1];
extern const vp9_tree_index vp9_intra_mode_tree[TREE_SIZE(INTRA_MODES)];
extern const vp9_tree_index vp9_inter_mode_tree[TREE_SIZE(INTER_MODES)];
extern const vp9_tree_index vp9_partition_tree[TREE_SIZE(PARTITION_TYPES)];
extern const vp9_tree_index vp9_switchable_interp_tree
                                [TREE_SIZE(SWITCHABLE_FILTERS)];

void vp9_setup_past_independence(struct VP9Common *cm);

void vp9_init_mbmode_probs(struct VP9Common *cm);

void vp9_adapt_mode_probs(struct VP9Common *cm);

void tx_counts_to_branch_counts_32x32(const unsigned int *tx_count_32x32p,
                                      unsigned int (*ct_32x32p)[2]);
void tx_counts_to_branch_counts_16x16(const unsigned int *tx_count_16x16p,
                                      unsigned int (*ct_16x16p)[2]);
void tx_counts_to_branch_counts_8x8(const unsigned int *tx_count_8x8p,
                                    unsigned int (*ct_8x8p)[2]);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // VP9_COMMON_VP9_ENTROPYMODE_H_
