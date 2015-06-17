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

#include "vp8/common/blockd.h"

void vp8_build_intra_predictors_mby_s_neon(MACROBLOCKD *x,
                                           unsigned char * yabove_row,
                                           unsigned char * yleft,
                                           int left_stride,
                                           unsigned char * ypred_ptr,
                                           int y_stride) {
  const int mode = x->mode_info_context->mbmi.mode;
  int i;

  switch (mode) {
    case DC_PRED:
    {
      int shift = x->up_available + x->left_available;
      uint8x16_t v_expected_dc = vdupq_n_u8(128);

      if (shift) {
        unsigned int average = 0;
        int expected_dc;
        if (x->up_available) {
          const uint8x16_t v_above = vld1q_u8(yabove_row);
          const uint16x8_t a = vpaddlq_u8(v_above);
          const uint32x4_t b = vpaddlq_u16(a);
          const uint64x2_t c = vpaddlq_u32(b);
          const uint32x2_t d = vadd_u32(vreinterpret_u32_u64(vget_low_u64(c)),
                                        vreinterpret_u32_u64(vget_high_u64(c)));
          average = vget_lane_u32(d, 0);
        }
        if (x->left_available) {
          for (i = 0; i < 16; ++i) {
              average += yleft[0];
              yleft += left_stride;
          }
        }
        shift += 3;
        expected_dc = (average + (1 << (shift - 1))) >> shift;
        v_expected_dc = vmovq_n_u8((uint8_t)expected_dc);
      }
      for (i = 0; i < 16; ++i) {
        vst1q_u8(ypred_ptr, v_expected_dc);
        ypred_ptr += y_stride;
      }
    }
    break;
    case V_PRED:
    {
      const uint8x16_t v_above = vld1q_u8(yabove_row);
      for (i = 0; i < 16; ++i) {
        vst1q_u8(ypred_ptr, v_above);
        ypred_ptr += y_stride;
      }
    }
    break;
    case H_PRED:
    {
      for (i = 0; i < 16; ++i) {
        const uint8x16_t v_yleft = vmovq_n_u8((uint8_t)yleft[0]);
        yleft += left_stride;
        vst1q_u8(ypred_ptr, v_yleft);
        ypred_ptr += y_stride;
      }
    }
    break;
    case TM_PRED:
    {
      const uint16x8_t v_ytop_left = vmovq_n_u16((int16_t)yabove_row[-1]);
      const uint8x16_t v_above = vld1q_u8(yabove_row);
      for (i = 0; i < 16; ++i) {
        const uint8x8_t v_yleft = vmov_n_u8((int8_t)yleft[0]);
        const uint16x8_t a_lo = vaddl_u8(vget_low_u8(v_above), v_yleft);
        const uint16x8_t a_hi = vaddl_u8(vget_high_u8(v_above), v_yleft);
        const int16x8_t b_lo = vsubq_s16(vreinterpretq_s16_u16(a_lo),
                                         vreinterpretq_s16_u16(v_ytop_left));
        const int16x8_t b_hi = vsubq_s16(vreinterpretq_s16_u16(a_hi),
                                         vreinterpretq_s16_u16(v_ytop_left));
        const uint8x8_t pred_lo = vqmovun_s16(b_lo);
        const uint8x8_t pred_hi = vqmovun_s16(b_hi);

        vst1q_u8(ypred_ptr, vcombine_u8(pred_lo, pred_hi));
        ypred_ptr += y_stride;
        yleft += left_stride;
      }
    }
    break;
  }
}

void vp8_build_intra_predictors_mbuv_s_neon(MACROBLOCKD *x,
                                            unsigned char * uabove_row,
                                            unsigned char * vabove_row,
                                            unsigned char * uleft,
                                            unsigned char * vleft,
                                            int left_stride,
                                            unsigned char * upred_ptr,
                                            unsigned char * vpred_ptr,
                                            int pred_stride) {
  const int mode = x->mode_info_context->mbmi.uv_mode;
  int i;

  switch (mode) {
    case DC_PRED:
    {
      int shift = x->up_available + x->left_available;
      uint8x8_t v_expected_udc = vdup_n_u8(128);
      uint8x8_t v_expected_vdc = vdup_n_u8(128);

      if (shift) {
        unsigned int average_u = 0;
        unsigned int average_v = 0;
        int expected_udc;
        int expected_vdc;
        if (x->up_available) {
          const uint8x8_t v_uabove = vld1_u8(uabove_row);
          const uint8x8_t v_vabove = vld1_u8(vabove_row);
          const uint16x8_t a = vpaddlq_u8(vcombine_u8(v_uabove, v_vabove));
          const uint32x4_t b = vpaddlq_u16(a);
          const uint64x2_t c = vpaddlq_u32(b);
          average_u = vgetq_lane_u32(vreinterpretq_u32_u64((c)), 0);
          average_v = vgetq_lane_u32(vreinterpretq_u32_u64((c)), 2);
        }
        if (x->left_available) {
          for (i = 0; i < 8; ++i) {
              average_u += uleft[0];
              uleft += left_stride;
              average_v += vleft[0];
              vleft += left_stride;
          }
        }
        shift += 2;
        expected_udc = (average_u + (1 << (shift - 1))) >> shift;
        expected_vdc = (average_v + (1 << (shift - 1))) >> shift;
        v_expected_udc = vmov_n_u8((uint8_t)expected_udc);
        v_expected_vdc = vmov_n_u8((uint8_t)expected_vdc);
      }
      for (i = 0; i < 8; ++i) {
        vst1_u8(upred_ptr, v_expected_udc);
        upred_ptr += pred_stride;
        vst1_u8(vpred_ptr, v_expected_vdc);
        vpred_ptr += pred_stride;
      }
    }
    break;
    case V_PRED:
    {
      const uint8x8_t v_uabove = vld1_u8(uabove_row);
      const uint8x8_t v_vabove = vld1_u8(vabove_row);
      for (i = 0; i < 8; ++i) {
        vst1_u8(upred_ptr, v_uabove);
        upred_ptr += pred_stride;
        vst1_u8(vpred_ptr, v_vabove);
        vpred_ptr += pred_stride;
      }
    }
    break;
    case H_PRED:
    {
      for (i = 0; i < 8; ++i) {
        const uint8x8_t v_uleft = vmov_n_u8((uint8_t)uleft[0]);
        const uint8x8_t v_vleft = vmov_n_u8((uint8_t)vleft[0]);
        uleft += left_stride;
        vleft += left_stride;
        vst1_u8(upred_ptr, v_uleft);
        upred_ptr += pred_stride;
        vst1_u8(vpred_ptr, v_vleft);
        vpred_ptr += pred_stride;
      }
    }
    break;
    case TM_PRED:
    {
      const uint16x8_t v_utop_left = vmovq_n_u16((int16_t)uabove_row[-1]);
      const uint16x8_t v_vtop_left = vmovq_n_u16((int16_t)vabove_row[-1]);
      const uint8x8_t v_uabove = vld1_u8(uabove_row);
      const uint8x8_t v_vabove = vld1_u8(vabove_row);
      for (i = 0; i < 8; ++i) {
        const uint8x8_t v_uleft = vmov_n_u8((int8_t)uleft[0]);
        const uint8x8_t v_vleft = vmov_n_u8((int8_t)vleft[0]);
        const uint16x8_t a_u = vaddl_u8(v_uabove, v_uleft);
        const uint16x8_t a_v = vaddl_u8(v_vabove, v_vleft);
        const int16x8_t b_u = vsubq_s16(vreinterpretq_s16_u16(a_u),
                                        vreinterpretq_s16_u16(v_utop_left));
        const int16x8_t b_v = vsubq_s16(vreinterpretq_s16_u16(a_v),
                                        vreinterpretq_s16_u16(v_vtop_left));
        const uint8x8_t pred_u = vqmovun_s16(b_u);
        const uint8x8_t pred_v = vqmovun_s16(b_v);

        vst1_u8(upred_ptr, pred_u);
        vst1_u8(vpred_ptr, pred_v);
        upred_ptr += pred_stride;
        vpred_ptr += pred_stride;
        uleft += left_stride;
        vleft += left_stride;
      }
    }
    break;
  }
}
