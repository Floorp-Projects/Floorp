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

#ifndef AV1_INV_TXFM1D_H_
#define AV1_INV_TXFM1D_H_

#include "av1/common/av1_txfm.h"

#ifdef __cplusplus
extern "C" {
#endif

void av1_idct4_new(const int32_t *input, int32_t *output, const int8_t *cos_bit,
                   const int8_t *stage_range);
void av1_idct8_new(const int32_t *input, int32_t *output, const int8_t *cos_bit,
                   const int8_t *stage_range);
void av1_idct16_new(const int32_t *input, int32_t *output,
                    const int8_t *cos_bit, const int8_t *stage_range);
void av1_idct32_new(const int32_t *input, int32_t *output,
                    const int8_t *cos_bit, const int8_t *stage_range);
#if CONFIG_TX64X64
void av1_idct64_new(const int32_t *input, int32_t *output,
                    const int8_t *cos_bit, const int8_t *stage_range);
#endif  // CONFIG_TX64X64

void av1_iadst4_new(const int32_t *input, int32_t *output,
                    const int8_t *cos_bit, const int8_t *stage_range);
void av1_iadst8_new(const int32_t *input, int32_t *output,
                    const int8_t *cos_bit, const int8_t *stage_range);
void av1_iadst16_new(const int32_t *input, int32_t *output,
                     const int8_t *cos_bit, const int8_t *stage_range);
void av1_iadst32_new(const int32_t *input, int32_t *output,
                     const int8_t *cos_bit, const int8_t *stage_range);
#if CONFIG_EXT_TX
void av1_iidentity4_c(const int32_t *input, int32_t *output,
                      const int8_t *cos_bit, const int8_t *stage_range);
void av1_iidentity8_c(const int32_t *input, int32_t *output,
                      const int8_t *cos_bit, const int8_t *stage_range);
void av1_iidentity16_c(const int32_t *input, int32_t *output,
                       const int8_t *cos_bit, const int8_t *stage_range);
void av1_iidentity32_c(const int32_t *input, int32_t *output,
                       const int8_t *cos_bit, const int8_t *stage_range);
#if CONFIG_TX64X64
void av1_iidentity64_c(const int32_t *input, int32_t *output,
                       const int8_t *cos_bit, const int8_t *stage_range);
#endif  // CONFIG_TX64X64
#endif  // CONFIG_EXT_TX

#ifdef __cplusplus
}
#endif

#endif  // AV1_INV_TXFM1D_H_
