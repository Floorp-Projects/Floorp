/*
 *  Copyright (c) 2014 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <arm_neon.h>

#include <math.h>

#include "vpx_mem/vpx_mem.h"

#include "vp9/common/vp9_quant_common.h"
#include "vp9/common/vp9_seg_common.h"

#include "vp9/encoder/vp9_encoder.h"
#include "vp9/encoder/vp9_quantize.h"
#include "vp9/encoder/vp9_rd.h"

void vp9_quantize_fp_neon(const int16_t *coeff_ptr, intptr_t count,
                          int skip_block, const int16_t *zbin_ptr,
                          const int16_t *round_ptr, const int16_t *quant_ptr,
                          const int16_t *quant_shift_ptr, int16_t *qcoeff_ptr,
                          int16_t *dqcoeff_ptr, const int16_t *dequant_ptr,
                          int zbin_oq_value, uint16_t *eob_ptr,
                          const int16_t *scan, const int16_t *iscan) {
  // TODO(jingning) Decide the need of these arguments after the
  // quantization process is completed.
  (void)zbin_ptr;
  (void)quant_shift_ptr;
  (void)zbin_oq_value;
  (void)scan;

  if (!skip_block) {
    // Quantization pass: All coefficients with index >= zero_flag are
    // skippable. Note: zero_flag can be zero.
    int i;
    const int16x8_t v_zero = vdupq_n_s16(0);
    const int16x8_t v_one = vdupq_n_s16(1);
    int16x8_t v_eobmax_76543210 = vdupq_n_s16(-1);
    int16x8_t v_round = vmovq_n_s16(round_ptr[1]);
    int16x8_t v_quant = vmovq_n_s16(quant_ptr[1]);
    int16x8_t v_dequant = vmovq_n_s16(dequant_ptr[1]);
    // adjust for dc
    v_round = vsetq_lane_s16(round_ptr[0], v_round, 0);
    v_quant = vsetq_lane_s16(quant_ptr[0], v_quant, 0);
    v_dequant = vsetq_lane_s16(dequant_ptr[0], v_dequant, 0);
    // process dc and the first seven ac coeffs
    {
      const int16x8_t v_iscan = vld1q_s16(&iscan[0]);
      const int16x8_t v_coeff = vld1q_s16(&coeff_ptr[0]);
      const int16x8_t v_coeff_sign = vshrq_n_s16(v_coeff, 15);
      const int16x8_t v_tmp = vabaq_s16(v_round, v_coeff, v_zero);
      const int32x4_t v_tmp_lo = vmull_s16(vget_low_s16(v_tmp),
                                           vget_low_s16(v_quant));
      const int32x4_t v_tmp_hi = vmull_s16(vget_high_s16(v_tmp),
                                           vget_high_s16(v_quant));
      const int16x8_t v_tmp2 = vcombine_s16(vshrn_n_s32(v_tmp_lo, 16),
                                            vshrn_n_s32(v_tmp_hi, 16));
      const uint16x8_t v_nz_mask = vceqq_s16(v_tmp2, v_zero);
      const int16x8_t v_iscan_plus1 = vaddq_s16(v_iscan, v_one);
      const int16x8_t v_nz_iscan = vbslq_s16(v_nz_mask, v_zero, v_iscan_plus1);
      const int16x8_t v_qcoeff_a = veorq_s16(v_tmp2, v_coeff_sign);
      const int16x8_t v_qcoeff = vsubq_s16(v_qcoeff_a, v_coeff_sign);
      const int16x8_t v_dqcoeff = vmulq_s16(v_qcoeff, v_dequant);
      v_eobmax_76543210 = vmaxq_s16(v_eobmax_76543210, v_nz_iscan);
      vst1q_s16(&qcoeff_ptr[0], v_qcoeff);
      vst1q_s16(&dqcoeff_ptr[0], v_dqcoeff);
      v_round = vmovq_n_s16(round_ptr[1]);
      v_quant = vmovq_n_s16(quant_ptr[1]);
      v_dequant = vmovq_n_s16(dequant_ptr[1]);
    }
    // now process the rest of the ac coeffs
    for (i = 8; i < count; i += 8) {
      const int16x8_t v_iscan = vld1q_s16(&iscan[i]);
      const int16x8_t v_coeff = vld1q_s16(&coeff_ptr[i]);
      const int16x8_t v_coeff_sign = vshrq_n_s16(v_coeff, 15);
      const int16x8_t v_tmp = vabaq_s16(v_round, v_coeff, v_zero);
      const int32x4_t v_tmp_lo = vmull_s16(vget_low_s16(v_tmp),
                                           vget_low_s16(v_quant));
      const int32x4_t v_tmp_hi = vmull_s16(vget_high_s16(v_tmp),
                                           vget_high_s16(v_quant));
      const int16x8_t v_tmp2 = vcombine_s16(vshrn_n_s32(v_tmp_lo, 16),
                                            vshrn_n_s32(v_tmp_hi, 16));
      const uint16x8_t v_nz_mask = vceqq_s16(v_tmp2, v_zero);
      const int16x8_t v_iscan_plus1 = vaddq_s16(v_iscan, v_one);
      const int16x8_t v_nz_iscan = vbslq_s16(v_nz_mask, v_zero, v_iscan_plus1);
      const int16x8_t v_qcoeff_a = veorq_s16(v_tmp2, v_coeff_sign);
      const int16x8_t v_qcoeff = vsubq_s16(v_qcoeff_a, v_coeff_sign);
      const int16x8_t v_dqcoeff = vmulq_s16(v_qcoeff, v_dequant);
      v_eobmax_76543210 = vmaxq_s16(v_eobmax_76543210, v_nz_iscan);
      vst1q_s16(&qcoeff_ptr[i], v_qcoeff);
      vst1q_s16(&dqcoeff_ptr[i], v_dqcoeff);
    }
    {
      const int16x4_t v_eobmax_3210 =
          vmax_s16(vget_low_s16(v_eobmax_76543210),
                   vget_high_s16(v_eobmax_76543210));
      const int64x1_t v_eobmax_xx32 =
          vshr_n_s64(vreinterpret_s64_s16(v_eobmax_3210), 32);
      const int16x4_t v_eobmax_tmp =
          vmax_s16(v_eobmax_3210, vreinterpret_s16_s64(v_eobmax_xx32));
      const int64x1_t v_eobmax_xxx3 =
          vshr_n_s64(vreinterpret_s64_s16(v_eobmax_tmp), 16);
      const int16x4_t v_eobmax_final =
          vmax_s16(v_eobmax_tmp, vreinterpret_s16_s64(v_eobmax_xxx3));

      *eob_ptr = (uint16_t)vget_lane_s16(v_eobmax_final, 0);
    }
  } else {
    vpx_memset(qcoeff_ptr, 0, count * sizeof(int16_t));
    vpx_memset(dqcoeff_ptr, 0, count * sizeof(int16_t));
    *eob_ptr = 0;
  }
}
