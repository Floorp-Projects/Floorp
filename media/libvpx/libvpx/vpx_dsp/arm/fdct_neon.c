/*
 *  Copyright (c) 2017 The WebM project authors. All Rights Reserved.
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
#include "vpx_dsp/arm/transpose_neon.h"

void vpx_fdct4x4_neon(const int16_t *input, tran_low_t *final_output,
                      int stride) {
  int i;
  // input[M * stride] * 16
  int16x4_t in[4];
  in[0] = vshl_n_s16(vld1_s16(input + 0 * stride), 4);
  in[1] = vshl_n_s16(vld1_s16(input + 1 * stride), 4);
  in[2] = vshl_n_s16(vld1_s16(input + 2 * stride), 4);
  in[3] = vshl_n_s16(vld1_s16(input + 3 * stride), 4);

  // If the very first value != 0, then add 1.
  if (input[0] != 0) {
    const int16x4_t one = vreinterpret_s16_s64(vdup_n_s64(1));
    in[0] = vadd_s16(in[0], one);
  }
  for (i = 0; i < 2; ++i) {
    vpx_fdct4x4_pass1_neon(in);
  }
  {
    // Not quite a rounding shift. Only add 1 despite shifting by 2.
    const int16x8_t one = vdupq_n_s16(1);
    int16x8_t out_01 = vcombine_s16(in[0], in[1]);
    int16x8_t out_23 = vcombine_s16(in[2], in[3]);
    out_01 = vshrq_n_s16(vaddq_s16(out_01, one), 2);
    out_23 = vshrq_n_s16(vaddq_s16(out_23, one), 2);
    store_s16q_to_tran_low(final_output + 0 * 8, out_01);
    store_s16q_to_tran_low(final_output + 1 * 8, out_23);
  }
}
