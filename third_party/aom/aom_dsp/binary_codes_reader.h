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
#include "aom_dsp/bitreader_buffer.h"

#define aom_read_primitive_symmetric(r, n, ACCT_STR_NAME) \
  aom_read_primitive_symmetric_(r, n ACCT_STR_ARG(ACCT_STR_NAME))
#define aom_read_primitive_quniform(r, n, ACCT_STR_NAME) \
  aom_read_primitive_quniform_(r, n ACCT_STR_ARG(ACCT_STR_NAME))
#define aom_read_primitive_refbilevel(r, n, p, ref, ACCT_STR_NAME) \
  aom_read_primitive_refbilevel_(r, n, p, ref ACCT_STR_ARG(ACCT_STR_NAME))
#define aom_read_primitive_subexpfin(r, n, k, ACCT_STR_NAME) \
  aom_read_primitive_subexpfin_(r, n, k ACCT_STR_ARG(ACCT_STR_NAME))
#define aom_read_primitive_refsubexpfin(r, n, k, ref, ACCT_STR_NAME) \
  aom_read_primitive_refsubexpfin_(r, n, k, ref ACCT_STR_ARG(ACCT_STR_NAME))
#define aom_read_signed_primitive_refsubexpfin(r, n, k, ref, ACCT_STR_NAME) \
  aom_read_signed_primitive_refsubexpfin_(r, n, k,                          \
                                          ref ACCT_STR_ARG(ACCT_STR_NAME))

int16_t aom_read_primitive_symmetric_(aom_reader *r,
                                      unsigned int mag_bits ACCT_STR_PARAM);
uint16_t aom_read_primitive_quniform_(aom_reader *r, uint16_t n ACCT_STR_PARAM);
uint16_t aom_read_primitive_refbilevel_(aom_reader *r, uint16_t n, uint16_t p,
                                        uint16_t ref ACCT_STR_PARAM);
uint16_t aom_read_primitive_subexpfin_(aom_reader *r, uint16_t n,
                                       uint16_t k ACCT_STR_PARAM);
uint16_t aom_read_primitive_refsubexpfin_(aom_reader *r, uint16_t n, uint16_t k,
                                          uint16_t ref ACCT_STR_PARAM);
int16_t aom_read_signed_primitive_refsubexpfin_(aom_reader *r, uint16_t n,
                                                uint16_t k,
                                                int16_t ref ACCT_STR_PARAM);

int16_t aom_rb_read_signed_primitive_refsubexpfin(
    struct aom_read_bit_buffer *rb, uint16_t n, uint16_t k, int16_t ref);
#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // AOM_DSP_BINARY_CODES_READER_H_
