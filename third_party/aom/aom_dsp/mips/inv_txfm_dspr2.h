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

#ifndef AOM_DSP_MIPS_INV_TXFM_DSPR2_H_
#define AOM_DSP_MIPS_INV_TXFM_DSPR2_H_

#include <assert.h>

#include "./aom_config.h"
#include "aom/aom_integer.h"
#include "aom_dsp/inv_txfm.h"
#include "aom_dsp/mips/common_dspr2.h"

#ifdef __cplusplus
extern "C" {
#endif

#if HAVE_DSPR2
/* Note: this macro expects a local int32_t named out to exist, and will write
 * to that variable. */
#define DCT_CONST_ROUND_SHIFT_TWICE_COSPI_16_64(input)                         \
  ({                                                                           \
                                                                               \
    int32_t tmp;                                                               \
    int dct_cost_rounding = DCT_CONST_ROUNDING;                                \
    int in = input;                                                            \
                                                                               \
    __asm__ __volatile__(/* out = dct_const_round_shift(dc *  cospi_16_64); */ \
                         "mtlo     %[dct_cost_rounding],   $ac1              " \
                         "                \n\t"                                \
                         "mthi     $zero,                  $ac1              " \
                         "                \n\t"                                \
                         "madd     $ac1,                   %[in],            " \
                         "%[cospi_16_64]  \n\t"                                \
                         "extp     %[tmp],                 $ac1,             " \
                         "31              \n\t"                                \
                                                                               \
                         /* out = dct_const_round_shift(out * cospi_16_64); */ \
                         "mtlo     %[dct_cost_rounding],   $ac2              " \
                         "                \n\t"                                \
                         "mthi     $zero,                  $ac2              " \
                         "                \n\t"                                \
                         "madd     $ac2,                   %[tmp],           " \
                         "%[cospi_16_64]  \n\t"                                \
                         "extp     %[out],                 $ac2,             " \
                         "31              \n\t"                                \
                                                                               \
                         : [tmp] "=&r"(tmp), [out] "=r"(out)                   \
                         : [in] "r"(in),                                       \
                           [dct_cost_rounding] "r"(dct_cost_rounding),         \
                           [cospi_16_64] "r"(cospi_16_64));                    \
    out;                                                                       \
  })

void aom_idct32_cols_add_blk_dspr2(int16_t *input, uint8_t *dest,
                                   int dest_stride);
void aom_idct4_rows_dspr2(const int16_t *input, int16_t *output);
void aom_idct4_columns_add_blk_dspr2(int16_t *input, uint8_t *dest,
                                     int dest_stride);
void iadst4_dspr2(const int16_t *input, int16_t *output);
void idct8_rows_dspr2(const int16_t *input, int16_t *output, uint32_t no_rows);
void idct8_columns_add_blk_dspr2(int16_t *input, uint8_t *dest,
                                 int dest_stride);
void iadst8_dspr2(const int16_t *input, int16_t *output);
void idct16_rows_dspr2(const int16_t *input, int16_t *output, uint32_t no_rows);
void idct16_cols_add_blk_dspr2(int16_t *input, uint8_t *dest, int dest_stride);
void iadst16_dspr2(const int16_t *input, int16_t *output);

#endif  // #if HAVE_DSPR2
#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // AOM_DSP_MIPS_INV_TXFM_DSPR2_H_
