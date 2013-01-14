@
@ Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
@
@ Use of this source code is governed by a BSD-style license
@ that can be found in the LICENSE file in the root of the source
@ tree. An additional intellectual property rights grant can be found
@ in the file PATENTS.  All contributing project authors may
@ be found in the AUTHORS file in the root of the source tree.
@
@ Reference code in transform.c. Bit not exact due to how rounding is
@ done in C code and ARM instructions, but quality by assembly code is
@ not worse.

#include "webrtc/modules/audio_coding/codecs/isac/fix/source/settings.h"
#include "webrtc/system_wrappers/interface/asm_defines.h"

GLOBAL_FUNCTION WebRtcIsacfix_Spec2TimeNeon

@ void WebRtcIsacfix_Spec2TimeNeon(WebRtc_Word16 *inreQ7,
@                                  WebRtc_Word16 *inimQ7,
@                                  WebRtc_Word32 *outre1Q16,
@                                  WebRtc_Word32 *outre2Q16);

DEFINE_FUNCTION WebRtcIsacfix_Spec2TimeNeon
.align  2
  push {r4-r11,lr}
  vpush {q4-q7}
  sub sp, sp, #16
  str r0, [sp]                @ inreQ7
  str r1, [sp, #4]            @ inimQ7
  str r2, [sp, #8]            @ outre1Q16
  str r3, [sp, #12]           @ outre2Q16

  mov r8, #(FRAMESAMPLES - 16)
  add r12, r0, r8             @ &inreQ7[FRAMESAMPLES/2 - 8]
  add r11, r1, r8             @ &inimQ7[FRAMESAMPLES/2 - 8]
  add r4, r2, r8, lsl #1      @ &outRe1Q16[FRAMESAMPLES/2 - 8]
  add r6, r3, r8, lsl #1      @ &outRe2Q16[FRAMESAMPLES/2 - 8]

  mov r8, #(FRAMESAMPLES / 2) @ loop counter
  ldr r9, =kCosTab2
  ldr r10, =kSinTab2
  mov r5, #-32
  mov r7, #-16
  vmov.u32 q6, #0             @ Initialize the maximum values for tmpInIm.
  vmov.u32 q7, #0             @ Initialize the maximum values for tmpInRe.

TRANSFORM_AND_FIND_MAX:
@ Use ">> 5", instead of "<< 9" and then ">> 14" as in the C code.
@ Bit-exact.

  vld1.16 {q0}, [r9]!         @ kCosTab2[]
  vld1.16 {q1}, [r10]!        @ kSinTab2[]
  vld1.16 {q2}, [r0]!         @ inreQ7[]
  vld1.16 {q3}, [r1]!         @ inimQ7[]

  vmull.s16 q8, d0, d4        @ kCosTab2[k] x inreQ7[k]
  vmull.s16 q9, d1, d5        @ kCosTab2[k] x inreQ7[k]
  vmull.s16 q10, d2, d6       @ kSinTab2[k] x inimQ7[k]
  vmull.s16 q11, d3, d7       @ kSinTab2[k] x inimQ7[k]
  vmull.s16 q12, d0, d6       @ kCosTab2[k] x inimQ7[k]
  vmull.s16 q13, d1, d7       @ kCosTab2[k] x inimQ7[k]
  vmull.s16 q14, d2, d4       @ kSinTab2[k] x inreQ7[k]
  vmull.s16 q15, d3, d5       @ kSinTab2[k] x inreQ7[k]

  vld1.16 {q2}, [r11], r7     @ inimQ7[FRAMESAMPLES/2 - 9 - i]
  vld1.16 {q3}, [r12], r7     @ inreQ7[FRAMESAMPLES/2 - 9 - i]

  vadd.s32 q8, q8, q10
  vadd.s32 q9, q9, q11
  vsub.s32 q12, q12, q14
  vsub.s32 q13, q13, q15

  subs r8, #16

  vrev64.16 q2, q2            @ Reverse the order of the samples
  vrev64.16 q3, q3            @ Reverse the order of the samples

  vshr.s32 q8, q8, #5         @ xrQ16
  vshr.s32 q9, q9, #5         @ xrQ16
  vshr.s32 q12, q12, #5       @ xiQ16
  vshr.s32 q13, q13, #5       @ xiQ16

  vmull.s16 q10, d0, d7       @ kCosTab2[k] * inreQ7[k]
  vmull.s16 q11, d1, d6       @ kCosTab2[k] * inreQ7[k]
  vmull.s16 q14, d2, d5       @ kSinTab2[k] * inimQ7[k]
  vmull.s16 q15, d3, d4       @ kSinTab2[k] * inimQ7[k]

  vmull.s16 q4, d0, d5        @ kCosTab2[k] * inimQ7[]
  vmull.s16 q5, d1, d4        @ kCosTab2[k] * inimQ7[]
  vmull.s16 q0, d2, d7        @ kSinTab2[k] * inreQ7[]
  vmull.s16 q2, d3, d6        @ kSinTab2[k] * inreQ7[]

  vsub.s32 q14, q14, q10      @ kSinTab2[k] * inimQ7[k] -kCosTab2[k] * inreQ7[k]
  vsub.s32 q15, q15, q11      @ kSinTab2[k] * inimQ7[k] -kCosTab2[k] * inreQ7[k]
  vadd.s32 q10, q4, q0        @ kCosTab2[k] * inimQ7[] + kSinTab2[k] * inreQ7[]
  vadd.s32 q11, q5, q2        @ kCosTab2[k] * inimQ7[] + kSinTab2[k] * inreQ7[]

  vshr.s32 q14, q14, #5       @ yiQ16
  vshr.s32 q15, q15, #5       @ yiQ16

  vneg.s32 q10, q10
  vneg.s32 q11, q11

  @ xrQ16 - yiQ16
  vsub.s32 q0, q8, q14
  vsub.s32 q1, q9, q15

  vshr.s32 q10, q10, #5       @ yrQ16
  vshr.s32 q11, q11, #5       @ yrQ16

  @ xrQ16 + yiQ16
  vadd.s32 q3, q8, q14
  vadd.s32 q2, q9, q15

  @ yrQ16 + xiQ16
  vadd.s32 q4, q10, q12
  vadd.s32 q5, q11, q13

  @ yrQ16 - xiQ16
  vsub.s32 q9, q10, q12
  vsub.s32 q8, q11, q13

  @ Reverse the order of the samples
  vrev64.32 q2, q2
  vrev64.32 q3, q3
  vrev64.32 q8, q8
  vrev64.32 q9, q9
  vswp d4, d5
  vswp d6, d7
  vswp d16, d17
  vswp d18, d19

  vst1.32  {q0, q1}, [r2]!    @ outre1Q16[k]
  vst1.32  {q2, q3}, [r4], r5 @ outre1Q16[FRAMESAMPLES/2 - 1 - k]
  vst1.32  {q4, q5}, [r3]!    @ outre2Q16[k]
  vst1.32  {q8, q9}, [r6], r5 @ outre2Q16[FRAMESAMPLES/2 - 1 - k]

  @ Find the absolute maximum in the vectors and store them in q6 and q7.
  vabs.s32 q10, q0
  vabs.s32 q11, q1
  vabs.s32 q12, q2
  vabs.s32 q13, q3
  vabs.s32 q14, q4
  vmax.u32 q6, q10             @ Use u32 so we don't lose the value 0x80000000.
  vmax.u32 q7, q14             @ Maximum for outre2Q16[].
  vabs.s32 q15, q5
  vmax.u32 q6, q11             @ Maximum for outre1Q16[].
  vmax.u32 q7, q15
  vabs.s32 q0, q8
  vmax.u32 q6, q12
  vmax.u32 q7, q0
  vabs.s32 q1, q9
  vmax.u32 q6, q13
  vmax.u32 q7, q1

  bgt TRANSFORM_AND_FIND_MAX

  @ Find the maximum value in the Neon registers
  vmax.u32 d12, d13
  vmax.u32 d14, d15
  vpmax.u32 d12, d12, d12     @ Both 32 bits words hold the same value tmpInIm.
  vpmax.u32 d14, d14, d14     @ Both 32 bits words hold the same value tmpInRe.
  vmax.s32 d14, d12, d14      @ if (tmpInIm>tmpInRe) tmpInRe = tmpInIm;

  ldr r4, [sp]                @ inreQ7
  vcls.s32  d15, d14          @ sh = WebRtcSpl_NormW32(tmpInRe);
  ldr r5, [sp, #4]            @ inimQ7
  vmov.i32  d14, #24          @ sh = sh-24;
  ldr r6, [sp, #8]            @ outre1Q16
  vsub.s32  d15, d15, d14
  ldr r7, [sp, #12]           @ outre2Q16
  vdup.s32  q8, d15[0]        @ sh

  mov r8, #(FRAMESAMPLES / 2)

PRE_FFT_SHIFT:
  vld1.32 {q0, q1}, [r6]!     @ outre1Q16[]
  vld1.32 {q2, q3}, [r6]!     @ outre1Q16[]
  vld1.32 {q4, q5}, [r7]!     @ outre2Q16[]
  vld1.32 {q6, q7}, [r7]!     @ outre2Q16[]

  subs r8, #16

  vrshl.s32 q0, q0, q8
  vrshl.s32 q1, q1, q8
  vrshl.s32 q2, q2, q8
  vrshl.s32 q3, q3, q8
  vrshl.s32 q4, q4, q8
  vrshl.s32 q5, q5, q8
  vrshl.s32 q6, q6, q8
  vrshl.s32 q7, q7, q8

  vmovn.s32 d0, q0
  vmovn.s32 d1, q1
  vmovn.s32 d2, q2
  vmovn.s32 d3, q3
  vmovn.s32 d4, q4
  vmovn.s32 d5, q5
  vmovn.s32 d6, q6
  vmovn.s32 d7, q7

  vst1.16 {q0, q1}, [r4]!     @ inreQ7[]
  vst1.16 {q2, q3}, [r5]!     @ inimQ7[]

  bgt PRE_FFT_SHIFT

  ldr r0, [sp]                @ inreQ7
  ldr r1, [sp, #4]            @ inimQ7
  mov r2, #1
  bl  WebRtcIsacfix_FftRadix16Fastest(PLT)

  ldr r4, [sp]                @ inreQ7
  ldr r5, [sp, #4]            @ inimQ7
  ldr r6, [sp, #8]            @ outre1Q16
  ldr r7, [sp, #12]           @ outre2Q16
  mov r8, #(FRAMESAMPLES / 2)
  vneg.s32 q5, q8             @ -sh
  movw r0, #273
  vdup.s32 d8, r0

POST_FFT_SHIFT_DIVIDE:
  vld1.16 {q0, q1}, [r4]!     @ inreQ7
  vld1.16 {q2, q3}, [r5]!     @ inimQ7

  subs r8, #16

  vmovl.s16 q6, d0
  vmovl.s16 q7, d1
  vmovl.s16 q8, d2
  vmovl.s16 q9, d3
  vmovl.s16 q0, d4
  vmovl.s16 q1, d5
  vmovl.s16 q2, d6
  vmovl.s16 q3, d7

  vshl.s32 q6, q6, q5
  vshl.s32 q7, q7, q5
  vshl.s32 q8, q8, q5
  vshl.s32 q9, q9, q5
  vshl.s32 q0, q0, q5
  vshl.s32 q1, q1, q5
  vshl.s32 q2, q2, q5
  vshl.s32 q3, q3, q5

  @ WEBRTC_SPL_MUL_16_32_RSFT16(273, outre1Q16[k])
  vmull.s32 q10, d12, d8
  vmull.s32 q11, d13, d8
  vmull.s32 q12, d14, d8
  vmull.s32 q13, d15, d8
  vshrn.s64 d12, q10, #16
  vshrn.s64 d13, q11, #16
  vshrn.s64 d14, q12, #16
  vshrn.s64 d15, q13, #16

  vmull.s32 q10, d16, d8
  vmull.s32 q11, d17, d8
  vmull.s32 q12, d18, d8
  vmull.s32 q13, d19, d8
  vshrn.s64 d16, q10, #16
  vshrn.s64 d17, q11, #16
  vshrn.s64 d18, q12, #16
  vshrn.s64 d19, q13, #16

  @ WEBRTC_SPL_MUL_16_32_RSFT16(273, outre2Q16[k])
  vmull.s32 q10, d0, d8
  vmull.s32 q11, d1, d8
  vmull.s32 q12, d2, d8
  vmull.s32 q13, d3, d8
  vshrn.s64 d0, q10, #16
  vshrn.s64 d1, q11, #16
  vshrn.s64 d2, q12, #16
  vshrn.s64 d3, q13, #16

  vmull.s32 q10, d4, d8
  vmull.s32 q11, d5, d8
  vmull.s32 q12, d6, d8
  vmull.s32 q13, d7, d8
  vshrn.s64 d4, q10, #16
  vshrn.s64 d5, q11, #16
  vshrn.s64 d6, q12, #16
  vshrn.s64 d7, q13, #16

  vst1.32 {q6, q7}, [r6]!     @ outre1Q16[]
  vst1.32 {q8, q9}, [r6]!     @ outre1Q16[]
  vst1.32 {q0, q1}, [r7]!     @ outre2Q16[]
  vst1.32 {q2, q3}, [r7]!     @ outre2Q16[]

  bgt POST_FFT_SHIFT_DIVIDE

  mov r8, #(FRAMESAMPLES / 2)
  ldr r9, =kCosTab1
  ldr r10, =kSinTab1
  ldr r2, [sp, #8]            @ outre1Q16
  ldr r3, [sp, #12]           @ outre2Q16
  movw r0, #31727

DEMODULATE_AND_SEPARATE:
  vld1.16 {q0}, [r9]!         @ kCosTab1[]
  vld1.16 {q1}, [r10]!        @ kSinTab1[]
  vld1.32 {q2, q3}, [r2]      @ outre1Q16
  vld1.32 {q4, q5}, [r3]      @ outre2Q16

  vmovl.s16 q6, d0            @ kCosTab1[]
  vmovl.s16 q7, d1            @ kCosTab1[]
  vmovl.s16 q8, d2            @ kSinTab1[]
  vmovl.s16 q9, d3            @ kSinTab1[]

  vmull.s32 q10, d12, d4      @ kCosTab1[k] * outre1Q16[k]
  vmull.s32 q11, d13, d5      @ kCosTab1[k] * outre1Q16[k]
  vmull.s32 q12, d14, d6      @ kCosTab1[k] * outre1Q16[k]
  vmull.s32 q13, d15, d7      @ kCosTab1[k] * outre1Q16[k]

  vmull.s32 q0, d16, d8       @ kSinTab1[k] * outre2Q16[k]
  vmull.s32 q1, d17, d9       @ kSinTab1[k] * outre2Q16[k]
  vmull.s32 q14, d18, d10     @ kSinTab1[k] * outre2Q16[k]
  vmull.s32 q15, d19, d11     @ kSinTab1[k] * outre2Q16[k]

  vsub.s64 q10, q10, q0
  vsub.s64 q11, q11, q1
  vsub.s64 q12, q12, q14
  vsub.s64 q13, q13, q15

  vrshrn.s64 d20, q10, #14    @ xrQ16
  vrshrn.s64 d21, q11, #14    @ xrQ16
  vrshrn.s64 d22, q12, #14    @ xrQ16
  vrshrn.s64 d23, q13, #14    @ xrQ16

  subs r8, #8

  vmull.s32 q12, d12, d8      @ kCosTab1[k] * outre2Q16[k]
  vmull.s32 q13, d13, d9      @ kCosTab1[k] * outre2Q16[k]
  vmull.s32 q14, d14, d10     @ kCosTab1[k] * outre2Q16[k]
  vmull.s32 q15, d15, d11     @ kCosTab1[k] * outre2Q16[k]

  vdup.s32 d9, r0             @ generic -> Neon doesn't cost extra cycles.

  vmull.s32 q0, d16, d4       @ kSinTab1[k] * outre1Q16[k]
  vmull.s32 q1, d17, d5       @ kSinTab1[k] * outre1Q16[k]
  vmull.s32 q6, d18, d6       @ kSinTab1[k] * outre1Q16[k]
  vmull.s32 q7, d19, d7       @ kSinTab1[k] * outre1Q16[k]

  vadd.s64 q12, q12, q0
  vadd.s64 q13, q13, q1
  vadd.s64 q14, q14, q6
  vadd.s64 q15, q15, q7

  vrshrn.s64 d24, q12, #14    @ xiQ16
  vrshrn.s64 d25, q13, #14    @ xiQ16
  vrshrn.s64 d26, q14, #14    @ xiQ16
  vrshrn.s64 d27, q15, #14    @ xiQ16

  @ WEBRTC_SPL_MUL_16_32_RSFT11(factQ11, xrQ16)
  vmull.s32 q0, d20, d9
  vmull.s32 q1, d21, d9
  vmull.s32 q2, d22, d9
  vmull.s32 q3, d23, d9

  vrshrn.s64 d0, q0, #11
  vrshrn.s64 d1, q1, #11
  vrshrn.s64 d2, q2, #11
  vrshrn.s64 d3, q3, #11

  @ WEBRTC_SPL_MUL_16_32_RSFT11(factQ11, xiQ16)
  vmull.s32 q6, d24, d9
  vmull.s32 q7, d25, d9
  vmull.s32 q8, d26, d9
  vmull.s32 q9, d27, d9

  vrshrn.s64 d4, q6, #11
  vrshrn.s64 d5, q7, #11
  vrshrn.s64 d6, q8, #11
  vrshrn.s64 d7, q9, #11

  vst1.16 {q0, q1}, [r2]!     @ outre1Q16[]
  vst1.16 {q2, q3}, [r3]!     @ outre2Q16[]

  bgt DEMODULATE_AND_SEPARATE

  add sp, sp, #16
  vpop {q4-q7}
  pop {r4-r11,pc}
