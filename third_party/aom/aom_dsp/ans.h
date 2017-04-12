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

#ifndef AOM_DSP_ANS_H_
#define AOM_DSP_ANS_H_
// Constants, types and utilities for Asymmetric Numeral Systems
// http://arxiv.org/abs/1311.2540v2

#include <assert.h>
#include "./aom_config.h"
#include "aom/aom_integer.h"
#include "aom_dsp/prob.h"

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

// Use windowed ANS, size is passed in at initialization
#define ANS_MAX_SYMBOLS 1
#define ANS_REVERSE 1

typedef uint8_t AnsP8;
#define ANS_P8_PRECISION 256u
#define ANS_P8_SHIFT 8
#define RANS_PROB_BITS 15
#define RANS_PRECISION (1u << RANS_PROB_BITS)

// L_BASE is the ANS base state. L_BASE % PRECISION must be 0.
#define L_BASE (1u << 17)
#define IO_BASE 256
// Range I = { L_BASE, L_BASE + 1, ..., L_BASE * IO_BASE - 1 }

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus
#endif  // AOM_DSP_ANS_H_
