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

#ifndef AV1_COMMON_QUANT_COMMON_H_
#define AV1_COMMON_QUANT_COMMON_H_

#include "aom/aom_codec.h"
#include "av1/common/seg_common.h"
#include "av1/common/enums.h"
#include "av1/common/entropy.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MINQ 0
#define MAXQ 255
#define QINDEX_RANGE (MAXQ - MINQ + 1)
#define QINDEX_BITS 8
// Total number of QM sets stored
#define QM_LEVEL_BITS 4
#define NUM_QM_LEVELS (1 << QM_LEVEL_BITS)
/* Range of QMS is between first and last value, with offset applied to inter
 * blocks*/
#define DEFAULT_QM_Y 10
#define DEFAULT_QM_U 11
#define DEFAULT_QM_V 12
#define DEFAULT_QM_FIRST 5
#define DEFAULT_QM_LAST 9

struct AV1Common;

int16_t av1_dc_quant_Q3(int qindex, int delta, aom_bit_depth_t bit_depth);
int16_t av1_ac_quant_Q3(int qindex, int delta, aom_bit_depth_t bit_depth);
int16_t av1_dc_quant_QTX(int qindex, int delta, aom_bit_depth_t bit_depth);
int16_t av1_ac_quant_QTX(int qindex, int delta, aom_bit_depth_t bit_depth);
int16_t av1_qindex_from_ac_Q3(int ac_Q3, aom_bit_depth_t bit_depth);

int av1_get_qindex(const struct segmentation *seg, int segment_id,
                   int base_qindex);
// Reduce the large number of quantizers to a smaller number of levels for which
// different matrices may be defined
static INLINE int aom_get_qmlevel(int qindex, int first, int last) {
  return first + (qindex * (last + 1 - first)) / QINDEX_RANGE;
}
void av1_qm_init(struct AV1Common *cm);
const qm_val_t *av1_iqmatrix(struct AV1Common *cm, int qindex, int comp,
                             TX_SIZE tx_size);
const qm_val_t *av1_qmatrix(struct AV1Common *cm, int qindex, int comp,
                            TX_SIZE tx_size);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // AV1_COMMON_QUANT_COMMON_H_
