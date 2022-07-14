/*
 *  Copyright (c) 2015 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <arm_neon.h>

#include "./vpx_config.h"
#include "./vpx_dsp_rtcd.h"
#include "vpx_dsp/txfm_common.h"
#include "vpx_dsp/vpx_dsp_common.h"
#include "vpx_dsp/arm/idct_neon.h"
#include "vpx_dsp/arm/fdct_neon.h"
#include "vpx_dsp/arm/mem_neon.h"

void vpx_fdct8x8_neon(const int16_t *input, tran_low_t *final_output,
                      int stride) {
  int i;
  // stage 1
  int16x8_t in[8];
  in[0] = vshlq_n_s16(vld1q_s16(&input[0 * stride]), 2);
  in[1] = vshlq_n_s16(vld1q_s16(&input[1 * stride]), 2);
  in[2] = vshlq_n_s16(vld1q_s16(&input[2 * stride]), 2);
  in[3] = vshlq_n_s16(vld1q_s16(&input[3 * stride]), 2);
  in[4] = vshlq_n_s16(vld1q_s16(&input[4 * stride]), 2);
  in[5] = vshlq_n_s16(vld1q_s16(&input[5 * stride]), 2);
  in[6] = vshlq_n_s16(vld1q_s16(&input[6 * stride]), 2);
  in[7] = vshlq_n_s16(vld1q_s16(&input[7 * stride]), 2);
  for (i = 0; i < 2; ++i) {
    vpx_fdct8x8_pass1_neon(in);
  }  // for
  {
    // from vpx_dct_sse2.c
    // Post-condition (division by two)
    //    division of two 16 bits signed numbers using shifts
    //    n / 2 = (n - (n >> 15)) >> 1
    const int16x8_t sign_in0 = vshrq_n_s16(in[0], 15);
    const int16x8_t sign_in1 = vshrq_n_s16(in[1], 15);
    const int16x8_t sign_in2 = vshrq_n_s16(in[2], 15);
    const int16x8_t sign_in3 = vshrq_n_s16(in[3], 15);
    const int16x8_t sign_in4 = vshrq_n_s16(in[4], 15);
    const int16x8_t sign_in5 = vshrq_n_s16(in[5], 15);
    const int16x8_t sign_in6 = vshrq_n_s16(in[6], 15);
    const int16x8_t sign_in7 = vshrq_n_s16(in[7], 15);
    in[0] = vhsubq_s16(in[0], sign_in0);
    in[1] = vhsubq_s16(in[1], sign_in1);
    in[2] = vhsubq_s16(in[2], sign_in2);
    in[3] = vhsubq_s16(in[3], sign_in3);
    in[4] = vhsubq_s16(in[4], sign_in4);
    in[5] = vhsubq_s16(in[5], sign_in5);
    in[6] = vhsubq_s16(in[6], sign_in6);
    in[7] = vhsubq_s16(in[7], sign_in7);
    // store results
    store_s16q_to_tran_low(final_output + 0 * 8, in[0]);
    store_s16q_to_tran_low(final_output + 1 * 8, in[1]);
    store_s16q_to_tran_low(final_output + 2 * 8, in[2]);
    store_s16q_to_tran_low(final_output + 3 * 8, in[3]);
    store_s16q_to_tran_low(final_output + 4 * 8, in[4]);
    store_s16q_to_tran_low(final_output + 5 * 8, in[5]);
    store_s16q_to_tran_low(final_output + 6 * 8, in[6]);
    store_s16q_to_tran_low(final_output + 7 * 8, in[7]);
  }
}
