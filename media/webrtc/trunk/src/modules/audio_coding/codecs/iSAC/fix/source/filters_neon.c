/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/*
 * filters_neon.c
 *
 * This file contains function WebRtcIsacfix_AutocorrNeon, optimized for
 * ARM Neon platform.
 *
 */

#include <arm_neon.h>
#include <assert.h>

#include "codec.h"

// Autocorrelation function in fixed point.
// NOTE! Different from SPLIB-version in how it scales the signal.
int WebRtcIsacfix_AutocorrNeon(
    WebRtc_Word32* __restrict r,
    const WebRtc_Word16* __restrict x,
    WebRtc_Word16 N,
    WebRtc_Word16 order,
    WebRtc_Word16* __restrict scale) {

  // The 1st for loop assumed N % 4 == 0.
  assert(N % 4 == 0);

  int i = 0;
  int zeros_low = 0;
  int zeros_high = 0;
  int16_t scaling = 0;
  int32_t sum = 0;

  // Step 1, calculate r[0] and how much scaling is needed.

  int16x4_t reg16x4;
  int64x1_t reg64x1a;
  int64x1_t reg64x1b;
  int32x4_t reg32x4;
  int64x2_t reg64x2 = vdupq_n_s64(0); // zeros

  // Loop over the samples and do:
  // sum += WEBRTC_SPL_MUL_16_16(x[i], x[i]);
  for (i = 0; i < N; i += 4) {
    reg16x4 = vld1_s16(&x[i]);
    reg32x4 = vmull_s16(reg16x4, reg16x4);
    reg64x2 = vpadalq_s32(reg64x2, reg32x4);
  }
  reg64x1a = vget_low_s64(reg64x2);
  reg64x1b = vget_high_s64(reg64x2);
  reg64x1a = vadd_s64(reg64x1a, reg64x1b);

  // Calculate the value of shifting (scaling).
  __asm__ __volatile__(
    "vmov %[z_l], %[z_h], %P[reg]\n\t"
    "clz %[z_l], %[z_l]\n\t"
    "clz %[z_h], %[z_h]\n\t"
    :[z_l]"+r"(zeros_low),
     [z_h]"+r"(zeros_high)
    :[reg]"w"(reg64x1a)
  );
  if (zeros_high != 32) {
    scaling = (32 - zeros_high + 1);
  } else if (zeros_low == 0) {
    scaling = 1;
  }
  reg64x1b = -scaling;
  reg64x1a = vshl_s64(reg64x1a, reg64x1b);

  // Record the result.
  r[0] = (int32_t)vget_lane_s64(reg64x1a, 0);


  // Step 2, perform the actual correlation calculation.

  /* Original C code (for the rest of the function):
  for (i = 1; i < order + 1; i++)  {
    prod = 0;
    for (j = 0; j < N - i; j++) {
      prod += WEBRTC_SPL_MUL_16_16(x[j], x[i + j]);
    }
    sum = (int32_t)(prod >> scaling);
    r[i] = sum;
  }
  */

  for (i = 1; i < order + 1; i++) {
    int32_t prod_lower = 0;
    int32_t prod_upper = 0;
    int16_t* ptr0 = &x[0];
    int16_t* ptr1 = &x[i];
    int32_t tmp = 0;

    // Initialize the sum (q9) to zero.
    __asm__ __volatile__("vmov.i32 q9, #0\n\t":::"q9");

    // Calculate the major block of the samples (a multiple of 8).
    for (; ptr0 < &x[N - i - 7];) {
      __asm__ __volatile__(
        "vld1.16 {d20, d21}, [%[ptr0]]!\n\t"
        "vld1.16 {d22, d23}, [%[ptr1]]!\n\t"
        "vmull.s16 q12, d20, d22\n\t"
        "vmull.s16 q13, d21, d23\n\t"
        "vpadal.s32 q9, q12\n\t"
        "vpadal.s32 q9, q13\n\t"

        // Specify constraints.
        :[ptr0]"+r"(ptr0),
        [ptr1]"+r"(ptr1)
        :
        :"d18", "d19", "d20", "d21", "d22", "d23", "d24", "d25", "d26", "d27"
      );
    }

    // Calculate the rest of the samples.
    for (; ptr0 < &x[N - i]; ptr0++, ptr1++) {
      __asm__ __volatile__(
        "smulbb %[tmp], %[ptr0], %[ptr1]\n\t"
        "adds %[prod_lower], %[prod_lower], %[tmp]\n\t"
        "adc %[prod_upper], %[prod_upper], %[tmp], asr #31\n\t"

        // Specify constraints.
        :[prod_lower]"+r"(prod_lower),
        [prod_upper]"+r"(prod_upper),
        [tmp]"+r"(tmp)
        :[ptr0]"r"(*ptr0),
        [ptr1]"r"(*ptr1)
      );
    }

    // Sum the results up, and do shift.
    __asm__ __volatile__(
      "vadd.i64 d18, d19\n\t"
      "vmov.32 d17[0], %[prod_lower]\n\t"
      "vmov.32 d17[1], %[prod_upper]\n\t"
      "vadd.i64 d17, d18\n\t"
      "mov %[tmp], %[scaling], asr #31\n\t"
      "vmov.32 d16, %[scaling], %[tmp]\n\t"
      "vshl.s64 d17, d16\n\t"
      "vmov.32 %[sum], d17[0]\n\t"

      // Specify constraints.
      :[sum]"=r"(sum),
      [tmp]"+r"(tmp)
      :[prod_upper]"r"(prod_upper),
      [prod_lower]"r"(prod_lower),
      [scaling]"r"(-scaling)
      :"d16", "d17", "d18", "d19"
    );

    // Record the result.
    r[i] = sum;
  }

  // Record the result.
  *scale = scaling;

  return(order + 1);
}
