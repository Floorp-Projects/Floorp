/*
 * Copyright (c) 2017, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#ifndef AOM_DSP_BINARY_CODES_READER_H_
#define AOM_DSP_BINARY_CODES_READER_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <assert.h>
#include "./aom_config.h"
#include "aom/aom_integer.h"
#include "aom_dsp/bitreader.h"

int16_t aom_read_primitive_symmetric(aom_reader *r, unsigned int mag_bits);

uint16_t aom_read_primitive_quniform(aom_reader *r, uint16_t n);
uint16_t aom_read_primitive_refbilevel(aom_reader *r, uint16_t n, uint16_t p,
                                       uint16_t ref);
uint16_t aom_read_primitive_subexpfin(aom_reader *r, uint16_t n, uint16_t k);
uint16_t aom_read_primitive_refsubexpfin(aom_reader *r, uint16_t n, uint16_t k,
                                         uint16_t ref);
int16_t aom_read_signed_primitive_refsubexpfin(aom_reader *r, uint16_t n,
                                               uint16_t k, int16_t ref);
#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // AOM_DSP_BINARY_CODES_READER_H_
