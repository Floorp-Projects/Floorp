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
#include "av1/encoder/mips/msa/fdct_msa.h"

void av1_fwht4x4_msa(const int16_t *input, int16_t *output,
                     int32_t src_stride) {
  v8i16 in0, in1, in2, in3, in4;

  LD_SH4(input, src_stride, in0, in1, in2, in3);

  in0 += in1;
  in3 -= in2;
  in4 = (in0 - in3) >> 1;
  SUB2(in4, in1, in4, in2, in1, in2);
  in0 -= in2;
  in3 += in1;

  TRANSPOSE4x4_SH_SH(in0, in2, in3, in1, in0, in2, in3, in1);

  in0 += in2;
  in1 -= in3;
  in4 = (in0 - in1) >> 1;
  SUB2(in4, in2, in4, in3, in2, in3);
  in0 -= in3;
  in1 += in2;

  SLLI_4V(in0, in1, in2, in3, 2);

  TRANSPOSE4x4_SH_SH(in0, in3, in1, in2, in0, in3, in1, in2);

  ST4x2_UB(in0, output, 4);
  ST4x2_UB(in3, output + 4, 4);
  ST4x2_UB(in1, output + 8, 4);
  ST4x2_UB(in2, output + 12, 4);
}

void av1_fht4x4_msa(const int16_t *input, int16_t *output, int32_t stride,
                    int32_t tx_type) {
  v8i16 in0, in1, in2, in3;

  LD_SH4(input, stride, in0, in1, in2, in3);

  /* fdct4 pre-process */
  {
    v8i16 temp, mask;
    v16i8 zero = { 0 };
    v16i8 one = __msa_ldi_b(1);

    mask = (v8i16)__msa_sldi_b(zero, one, 15);
    SLLI_4V(in0, in1, in2, in3, 4);
    temp = __msa_ceqi_h(in0, 0);
    temp = (v8i16)__msa_xori_b((v16u8)temp, 255);
    temp = mask & temp;
    in0 += temp;
  }

  switch (tx_type) {
    case DCT_DCT:
      AOM_FDCT4(in0, in1, in2, in3, in0, in1, in2, in3);
      TRANSPOSE4x4_SH_SH(in0, in1, in2, in3, in0, in1, in2, in3);
      AOM_FDCT4(in0, in1, in2, in3, in0, in1, in2, in3);
      break;
    case ADST_DCT:
      AOM_FADST4(in0, in1, in2, in3, in0, in1, in2, in3);
      TRANSPOSE4x4_SH_SH(in0, in1, in2, in3, in0, in1, in2, in3);
      AOM_FDCT4(in0, in1, in2, in3, in0, in1, in2, in3);
      break;
    case DCT_ADST:
      AOM_FDCT4(in0, in1, in2, in3, in0, in1, in2, in3);
      TRANSPOSE4x4_SH_SH(in0, in1, in2, in3, in0, in1, in2, in3);
      AOM_FADST4(in0, in1, in2, in3, in0, in1, in2, in3);
      break;
    case ADST_ADST:
      AOM_FADST4(in0, in1, in2, in3, in0, in1, in2, in3);
      TRANSPOSE4x4_SH_SH(in0, in1, in2, in3, in0, in1, in2, in3);
      AOM_FADST4(in0, in1, in2, in3, in0, in1, in2, in3);
      break;
    default: assert(0); break;
  }

  TRANSPOSE4x4_SH_SH(in0, in1, in2, in3, in0, in1, in2, in3);
  ADD4(in0, 1, in1, 1, in2, 1, in3, 1, in0, in1, in2, in3);
  SRA_4V(in0, in1, in2, in3, 2);
  PCKEV_D2_SH(in1, in0, in3, in2, in0, in2);
  ST_SH2(in0, in2, output, 8);
}
