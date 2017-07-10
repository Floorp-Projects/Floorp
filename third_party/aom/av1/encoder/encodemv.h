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

#ifndef AV1_ENCODER_ENCODEMV_H_
#define AV1_ENCODER_ENCODEMV_H_

#include "av1/encoder/encoder.h"

#ifdef __cplusplus
extern "C" {
#endif

void av1_entropy_mv_init(void);

#if !CONFIG_NEW_MULTISYMBOL
void av1_write_nmv_probs(AV1_COMMON *cm, int usehp, aom_writer *w,
                         nmv_context_counts *const counts);
#endif

void av1_encode_mv(AV1_COMP *cpi, aom_writer *w, const MV *mv, const MV *ref,
                   nmv_context *mvctx, int usehp);

void av1_build_nmv_cost_table(int *mvjoint, int *mvcost[2],
                              const nmv_context *mvctx,
                              MvSubpelPrecision precision);

void av1_update_mv_count(ThreadData *td);

#if CONFIG_INTRABC
void av1_encode_dv(aom_writer *w, const MV *mv, const MV *ref,
                   nmv_context *mvctx);
#endif  // CONFIG_INTRABC

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // AV1_ENCODER_ENCODEMV_H_
