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

#include "./aom_dsp_rtcd.h"
#include "aom_dsp/mips/macros_msa.h"

uint32_t aom_avg_8x8_msa(const uint8_t *src, int32_t src_stride) {
  uint32_t sum_out;
  v16u8 src0, src1, src2, src3, src4, src5, src6, src7;
  v8u16 sum0, sum1, sum2, sum3, sum4, sum5, sum6, sum7;
  v4u32 sum = { 0 };

  LD_UB8(src, src_stride, src0, src1, src2, src3, src4, src5, src6, src7);
  HADD_UB4_UH(src0, src1, src2, src3, sum0, sum1, sum2, sum3);
  HADD_UB4_UH(src4, src5, src6, src7, sum4, sum5, sum6, sum7);
  ADD4(sum0, sum1, sum2, sum3, sum4, sum5, sum6, sum7, sum0, sum2, sum4, sum6);
  ADD2(sum0, sum2, sum4, sum6, sum0, sum4);
  sum0 += sum4;

  sum = __msa_hadd_u_w(sum0, sum0);
  sum0 = (v8u16)__msa_pckev_h((v8i16)sum, (v8i16)sum);
  sum = __msa_hadd_u_w(sum0, sum0);
  sum = (v4u32)__msa_srari_w((v4i32)sum, 6);
  sum_out = __msa_copy_u_w((v4i32)sum, 0);

  return sum_out;
}

uint32_t aom_avg_4x4_msa(const uint8_t *src, int32_t src_stride) {
  uint32_t sum_out;
  uint32_t src0, src1, src2, src3;
  v16u8 vec = { 0 };
  v8u16 sum0;
  v4u32 sum1;
  v2u64 sum2;

  LW4(src, src_stride, src0, src1, src2, src3);
  INSERT_W4_UB(src0, src1, src2, src3, vec);

  sum0 = __msa_hadd_u_h(vec, vec);
  sum1 = __msa_hadd_u_w(sum0, sum0);
  sum0 = (v8u16)__msa_pckev_h((v8i16)sum1, (v8i16)sum1);
  sum1 = __msa_hadd_u_w(sum0, sum0);
  sum2 = __msa_hadd_u_d(sum1, sum1);
  sum1 = (v4u32)__msa_srari_w((v4i32)sum2, 4);
  sum_out = __msa_copy_u_w((v4i32)sum1, 0);

  return sum_out;
}
