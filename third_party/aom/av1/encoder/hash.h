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

#ifndef AV1_ENCODER_HASH_H_
#define AV1_ENCODER_HASH_H_

#include "./aom_config.h"
#include "aom/aom_integer.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _crc_calculator {
  uint32_t remainder;
  uint32_t trunc_poly;
  uint32_t bits;
  uint32_t table[256];
  uint32_t final_result_mask;
} CRC_CALCULATOR;

// Initialize the crc calculator. It must be executed at least once before
// calling av1_get_crc_value().
void av1_crc_calculator_init(CRC_CALCULATOR *p_crc_calculator, uint32_t bits,
                             uint32_t truncPoly);

uint32_t av1_get_crc_value(CRC_CALCULATOR *p_crc_calculator, uint8_t *p,
                           int length);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // AV1_ENCODER_HASH_H_
