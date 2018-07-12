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

#include <assert.h>

#include "av1/common/enums.h"
#include "aom_dsp/mips/inv_txfm_msa.h"

void av1_iht16x16_256_add_msa(const int16_t *input, uint8_t *dst,
                              int32_t dst_stride, TxfmParam *txfm_param) {
  int32_t i;
  DECLARE_ALIGNED(32, int16_t, out[16 * 16]);
  int16_t *out_ptr = &out[0];
  const TX_TYPE tx_type = txfm_param->tx_type;

  switch (tx_type) {
    case DCT_DCT:
      /* transform rows */
      for (i = 0; i < 2; ++i) {
        /* process 16 * 8 block */
        aom_idct16_1d_rows_msa((input + (i << 7)), (out_ptr + (i << 7)));
      }

      /* transform columns */
      for (i = 0; i < 2; ++i) {
        /* process 8 * 16 block */
        aom_idct16_1d_columns_addblk_msa((out_ptr + (i << 3)), (dst + (i << 3)),
                                         dst_stride);
      }
      break;
    case ADST_DCT:
      /* transform rows */
      for (i = 0; i < 2; ++i) {
        /* process 16 * 8 block */
        aom_idct16_1d_rows_msa((input + (i << 7)), (out_ptr + (i << 7)));
      }

      /* transform columns */
      for (i = 0; i < 2; ++i) {
        aom_iadst16_1d_columns_addblk_msa((out_ptr + (i << 3)),
                                          (dst + (i << 3)), dst_stride);
      }
      break;
    case DCT_ADST:
      /* transform rows */
      for (i = 0; i < 2; ++i) {
        /* process 16 * 8 block */
        aom_iadst16_1d_rows_msa((input + (i << 7)), (out_ptr + (i << 7)));
      }

      /* transform columns */
      for (i = 0; i < 2; ++i) {
        /* process 8 * 16 block */
        aom_idct16_1d_columns_addblk_msa((out_ptr + (i << 3)), (dst + (i << 3)),
                                         dst_stride);
      }
      break;
    case ADST_ADST:
      /* transform rows */
      for (i = 0; i < 2; ++i) {
        /* process 16 * 8 block */
        aom_iadst16_1d_rows_msa((input + (i << 7)), (out_ptr + (i << 7)));
      }

      /* transform columns */
      for (i = 0; i < 2; ++i) {
        aom_iadst16_1d_columns_addblk_msa((out_ptr + (i << 3)),
                                          (dst + (i << 3)), dst_stride);
      }
      break;
    default: assert(0); break;
  }
}
