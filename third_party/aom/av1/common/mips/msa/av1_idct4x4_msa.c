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

void av1_iht4x4_16_add_msa(const int16_t *input, uint8_t *dst,
                           int32_t dst_stride, TxfmParam *txfm_param) {
  v8i16 in0, in1, in2, in3;
  int32_t tx_type = txfm_param->tx_type;

  /* load vector elements of 4x4 block */
  LD4x4_SH(input, in0, in1, in2, in3);
  TRANSPOSE4x4_SH_SH(in0, in1, in2, in3, in0, in1, in2, in3);

  switch (tx_type) {
    case DCT_DCT:
      /* DCT in horizontal */
      AOM_IDCT4x4(in0, in1, in2, in3, in0, in1, in2, in3);
      /* DCT in vertical */
      TRANSPOSE4x4_SH_SH(in0, in1, in2, in3, in0, in1, in2, in3);
      AOM_IDCT4x4(in0, in1, in2, in3, in0, in1, in2, in3);
      break;
    case ADST_DCT:
      /* DCT in horizontal */
      AOM_IDCT4x4(in0, in1, in2, in3, in0, in1, in2, in3);
      /* ADST in vertical */
      TRANSPOSE4x4_SH_SH(in0, in1, in2, in3, in0, in1, in2, in3);
      AOM_IADST4x4(in0, in1, in2, in3, in0, in1, in2, in3);
      break;
    case DCT_ADST:
      /* ADST in horizontal */
      AOM_IADST4x4(in0, in1, in2, in3, in0, in1, in2, in3);
      /* DCT in vertical */
      TRANSPOSE4x4_SH_SH(in0, in1, in2, in3, in0, in1, in2, in3);
      AOM_IDCT4x4(in0, in1, in2, in3, in0, in1, in2, in3);
      break;
    case ADST_ADST:
      /* ADST in horizontal */
      AOM_IADST4x4(in0, in1, in2, in3, in0, in1, in2, in3);
      /* ADST in vertical */
      TRANSPOSE4x4_SH_SH(in0, in1, in2, in3, in0, in1, in2, in3);
      AOM_IADST4x4(in0, in1, in2, in3, in0, in1, in2, in3);
      break;
    default: assert(0); break;
  }

  /* final rounding (add 2^3, divide by 2^4) and shift */
  SRARI_H4_SH(in0, in1, in2, in3, 4);
  /* add block and store 4x4 */
  ADDBLK_ST4x4_UB(in0, in1, in2, in3, dst, dst_stride);
}
