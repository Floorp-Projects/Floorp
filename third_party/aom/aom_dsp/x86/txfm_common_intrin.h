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

#ifndef _AOM_DSP_X86_TXFM_COMMON_INTRIN_H_
#define _AOM_DSP_X86_TXFM_COMMON_INTRIN_H_

// Note:
//  This header file should be put below any x86 intrinsics head file

static INLINE void storeu_output(const __m128i *poutput, tran_low_t *dst_ptr) {
  if (sizeof(tran_low_t) == 4) {
    const __m128i zero = _mm_setzero_si128();
    const __m128i sign_bits = _mm_cmplt_epi16(*poutput, zero);
    __m128i out0 = _mm_unpacklo_epi16(*poutput, sign_bits);
    __m128i out1 = _mm_unpackhi_epi16(*poutput, sign_bits);
    _mm_storeu_si128((__m128i *)(dst_ptr), out0);
    _mm_storeu_si128((__m128i *)(dst_ptr + 4), out1);
  } else {
    _mm_storeu_si128((__m128i *)(dst_ptr), *poutput);
  }
}

#endif  // _AOM_DSP_X86_TXFM_COMMON_INTRIN_H_
