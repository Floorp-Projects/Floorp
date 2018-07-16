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

#ifndef AV1_ENCODER_TOKENIZE_H_
#define AV1_ENCODER_TOKENIZE_H_

#include "av1/common/entropy.h"
#include "av1/encoder/block.h"
#include "aom_dsp/bitwriter.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  aom_cdf_prob *color_map_cdf;
  // TODO(yaowu: use packed enum type if appropriate)
  uint8_t token;
} TOKENEXTRA;

struct AV1_COMP;
struct ThreadData;
struct FRAME_COUNTS;

struct tokenize_b_args {
  const struct AV1_COMP *cpi;
  struct ThreadData *td;
  TOKENEXTRA **tp;
  int this_rate;
  uint8_t allow_update_cdf;
};

typedef enum {
  OUTPUT_ENABLED = 0,
  DRY_RUN_NORMAL,
  DRY_RUN_COSTCOEFFS,
} RUN_TYPE;

// Note in all the tokenize functions rate if non NULL is incremented
// with the coefficient token cost only if dry_run = DRY_RUN_COSTCOEFS,
// otherwise rate is not incremented.
void av1_tokenize_sb_vartx(const struct AV1_COMP *cpi, struct ThreadData *td,
                           TOKENEXTRA **t, RUN_TYPE dry_run, int mi_row,
                           int mi_col, BLOCK_SIZE bsize, int *rate,
                           uint8_t allow_update_cdf);

int av1_cost_color_map(const MACROBLOCK *const x, int plane, BLOCK_SIZE bsize,
                       TX_SIZE tx_size, COLOR_MAP_TYPE type);

void av1_tokenize_color_map(const MACROBLOCK *const x, int plane,
                            TOKENEXTRA **t, BLOCK_SIZE bsize, TX_SIZE tx_size,
                            COLOR_MAP_TYPE type, int allow_update_cdf,
                            struct FRAME_COUNTS *counts);

static INLINE int av1_get_tx_eob(const struct segmentation *seg, int segment_id,
                                 TX_SIZE tx_size) {
  const int eob_max = av1_get_max_eob(tx_size);
  return segfeature_active(seg, segment_id, SEG_LVL_SKIP) ? 0 : eob_max;
}

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // AV1_ENCODER_TOKENIZE_H_
