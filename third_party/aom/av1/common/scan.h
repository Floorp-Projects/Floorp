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

#ifndef AV1_COMMON_SCAN_H_
#define AV1_COMMON_SCAN_H_

#include "aom/aom_integer.h"
#include "aom_ports/mem.h"

#include "av1/common/enums.h"
#include "av1/common/onyxc_int.h"
#include "av1/common/blockd.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_NEIGHBORS 2

extern const SCAN_ORDER av1_default_scan_orders[TX_SIZES];
extern const SCAN_ORDER av1_intra_scan_orders[TX_SIZES_ALL][TX_TYPES];
extern const SCAN_ORDER av1_inter_scan_orders[TX_SIZES_ALL][TX_TYPES];

#if CONFIG_ADAPT_SCAN
void av1_update_scan_count_facade(AV1_COMMON *cm, FRAME_COUNTS *counts,
                                  TX_SIZE tx_size, TX_TYPE tx_type,
                                  const tran_low_t *dqcoeffs, int max_scan);

// embed r + c and coeff_idx info with nonzero probabilities. When sorting the
// nonzero probabilities, if there is a tie, the coefficient with smaller r + c
// will be scanned first
void av1_augment_prob(TX_SIZE tx_size, TX_TYPE tx_type, uint32_t *prob);

// apply quick sort on nonzero probabilities to obtain a sort order
void av1_update_sort_order(TX_SIZE tx_size, TX_TYPE tx_type,
                           const uint32_t *non_zero_prob, int16_t *sort_order);

// apply topological sort on the nonzero probabilities sorting order to
// guarantee each to-be-scanned coefficient's upper and left coefficient will be
// scanned before the to-be-scanned coefficient.
void av1_update_scan_order(TX_SIZE tx_size, int16_t *sort_order, int16_t *scan,
                           int16_t *iscan);

// For each coeff_idx in scan[], update its above and left neighbors in
// neighbors[] accordingly.
void av1_update_neighbors(int tx_size, const int16_t *scan,
                          const int16_t *iscan, int16_t *neighbors);
void av1_init_scan_order(AV1_COMMON *cm);
void av1_adapt_scan_order(AV1_COMMON *cm);
#endif
void av1_deliver_eob_threshold(const AV1_COMMON *cm, MACROBLOCKD *xd);

static INLINE int get_coef_context(const int16_t *neighbors,
                                   const uint8_t *token_cache, int c) {
  return (1 + token_cache[neighbors[MAX_NEIGHBORS * c + 0]] +
          token_cache[neighbors[MAX_NEIGHBORS * c + 1]]) >>
         1;
}

static INLINE const SCAN_ORDER *get_default_scan(TX_SIZE tx_size,
                                                 TX_TYPE tx_type,
                                                 int is_inter) {
#if CONFIG_EXT_TX || CONFIG_VAR_TX
  return is_inter ? &av1_inter_scan_orders[tx_size][tx_type]
                  : &av1_intra_scan_orders[tx_size][tx_type];
#else
  (void)is_inter;
  return &av1_intra_scan_orders[tx_size][tx_type];
#endif  // CONFIG_EXT_TX
}

static INLINE const SCAN_ORDER *get_scan(const AV1_COMMON *cm, TX_SIZE tx_size,
                                         TX_TYPE tx_type,
                                         const MB_MODE_INFO *mbmi) {
#if CONFIG_MRC_TX
  // use the DCT_DCT scan order for MRC_DCT for now
  if (tx_type == MRC_DCT) tx_type = DCT_DCT;
#endif  // CONFIG_MRC_TX
  const int is_inter = is_inter_block(mbmi);
#if CONFIG_ADAPT_SCAN
  (void)mbmi;
  (void)is_inter;
#if CONFIG_EXT_TX
  if (tx_type >= IDTX)
    return get_default_scan(tx_size, tx_type, is_inter);
  else
#endif  // CONFIG_EXT_TX
    return &cm->fc->sc[tx_size][tx_type];
#else   // CONFIG_ADAPT_SCAN
  (void)cm;
  return get_default_scan(tx_size, tx_type, is_inter);
#endif  // CONFIG_ADAPT_SCAN
}

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // AV1_COMMON_SCAN_H_
