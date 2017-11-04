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

void av1_iht8x8_64_add_msa(const int16_t *input, uint8_t *dst,
                           int32_t dst_stride, TxfmParam *txfm_param) {
  v8i16 in0, in1, in2, in3, in4, in5, in6, in7;
  const TX_TYPE tx_type = txfm_param->tx_type;

  /* load vector elements of 8x8 block */
  LD_SH8(input, 8, in0, in1, in2, in3, in4, in5, in6, in7);

  TRANSPOSE8x8_SH_SH(in0, in1, in2, in3, in4, in5, in6, in7, in0, in1, in2, in3,
                     in4, in5, in6, in7);

  switch (tx_type) {
    case DCT_DCT:
      /* DCT in horizontal */
      AOM_IDCT8x8_1D(in0, in1, in2, in3, in4, in5, in6, in7, in0, in1, in2, in3,
                     in4, in5, in6, in7);
      /* DCT in vertical */
      TRANSPOSE8x8_SH_SH(in0, in1, in2, in3, in4, in5, in6, in7, in0, in1, in2,
                         in3, in4, in5, in6, in7);
      AOM_IDCT8x8_1D(in0, in1, in2, in3, in4, in5, in6, in7, in0, in1, in2, in3,
                     in4, in5, in6, in7);
      break;
    case ADST_DCT:
      /* DCT in horizontal */
      AOM_IDCT8x8_1D(in0, in1, in2, in3, in4, in5, in6, in7, in0, in1, in2, in3,
                     in4, in5, in6, in7);
      /* ADST in vertical */
      TRANSPOSE8x8_SH_SH(in0, in1, in2, in3, in4, in5, in6, in7, in0, in1, in2,
                         in3, in4, in5, in6, in7);
      AOM_ADST8(in0, in1, in2, in3, in4, in5, in6, in7, in0, in1, in2, in3, in4,
                in5, in6, in7);
      break;
    case DCT_ADST:
      /* ADST in horizontal */
      AOM_ADST8(in0, in1, in2, in3, in4, in5, in6, in7, in0, in1, in2, in3, in4,
                in5, in6, in7);
      /* DCT in vertical */
      TRANSPOSE8x8_SH_SH(in0, in1, in2, in3, in4, in5, in6, in7, in0, in1, in2,
                         in3, in4, in5, in6, in7);
      AOM_IDCT8x8_1D(in0, in1, in2, in3, in4, in5, in6, in7, in0, in1, in2, in3,
                     in4, in5, in6, in7);
      break;
    case ADST_ADST:
      /* ADST in horizontal */
      AOM_ADST8(in0, in1, in2, in3, in4, in5, in6, in7, in0, in1, in2, in3, in4,
                in5, in6, in7);
      /* ADST in vertical */
      TRANSPOSE8x8_SH_SH(in0, in1, in2, in3, in4, in5, in6, in7, in0, in1, in2,
                         in3, in4, in5, in6, in7);
      AOM_ADST8(in0, in1, in2, in3, in4, in5, in6, in7, in0, in1, in2, in3, in4,
                in5, in6, in7);
      break;
    default: assert(0); break;
  }

  /* final rounding (add 2^4, divide by 2^5) and shift */
  SRARI_H4_SH(in0, in1, in2, in3, 5);
  SRARI_H4_SH(in4, in5, in6, in7, 5);

  /* add block and store 8x8 */
  AOM_ADDBLK_ST8x4_UB(dst, dst_stride, in0, in1, in2, in3);
  dst += (4 * dst_stride);
  AOM_ADDBLK_ST8x4_UB(dst, dst_stride, in4, in5, in6, in7);
}
