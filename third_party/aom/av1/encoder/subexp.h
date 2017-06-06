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

#ifndef AV1_ENCODER_SUBEXP_H_
#define AV1_ENCODER_SUBEXP_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "aom_dsp/bitwriter.h"
#include "aom_dsp/prob.h"

void av1_write_prob_diff_update(aom_writer *w, aom_prob newp, aom_prob oldpm);

void av1_cond_prob_diff_update(aom_writer *w, aom_prob *oldp,
                               const unsigned int ct[2], int probwt);

int av1_prob_diff_update_savings_search(const unsigned int *ct, aom_prob oldp,
                                        aom_prob *bestp, aom_prob upd,
                                        int probwt);

int av1_prob_diff_update_savings_search_model(const unsigned int *ct,
                                              const aom_prob oldp,
                                              aom_prob *bestp, aom_prob upd,
                                              int stepsize, int probwt);

int av1_cond_prob_diff_update_savings(aom_prob *oldp, const unsigned int ct[2],
                                      int probwt);
#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // AV1_ENCODER_SUBEXP_H_
