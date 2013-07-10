@
@ Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
@
@ Use of this source code is governed by a BSD-style license
@ that can be found in the LICENSE file in the root of the source
@ tree. An additional intellectual property rights grant can be found
@ in the file PATENTS.  All contributing project authors may
@ be found in the AUTHORS file in the root of the source tree.
@

@ aecm_core_neon.s
@ This file contains some functions in AECM, optimized for ARM Neon
@ platforms. Reference C code is in file aecm_core.c. Bit-exact.

#include "aecm_defines.h"
#include "aecm_core_neon_offsets.h"
#include "webrtc/system_wrappers/interface/asm_defines.h"

GLOBAL_LABEL WebRtcAecm_kSqrtHanning
GLOBAL_FUNCTION WebRtcAecm_WindowAndFFTNeon
GLOBAL_FUNCTION WebRtcAecm_InverseFFTAndWindowNeon
GLOBAL_FUNCTION WebRtcAecm_CalcLinearEnergiesNeon
GLOBAL_FUNCTION WebRtcAecm_StoreAdaptiveChannelNeon
GLOBAL_FUNCTION WebRtcAecm_ResetAdaptiveChannelNeon

@ void WebRtcAecm_WindowAndFFTNeon(AecmCore_t* aecm,
@                                  int16_t* fft,
@                                  const int16_t* time_signal,
@                                  complex16_t* freq_signal,
@                                  int time_signal_scaling);
.align 2
DEFINE_FUNCTION WebRtcAecm_WindowAndFFTNeon
  push {r4, r5, r6, lr}

  ldr r12, [sp, #16]                         @ time_signal_scaling
  vdup.16 d16, r12

  vmov.i16 d21, #0                           @ For imaginary parts of |fft|.
  vmov.i16 d27, #0                           @ For imaginary parts of |fft|.
  adr r5, WebRtcAecm_kSqrtHanning
  adr lr, kSqrtHanningReversed
  add r4, r1, #(PART_LEN2 * 2)               @ &fft[PART_LEN2]
  add r12, r2, #(PART_LEN * 2)               @ time_signal[PART_LEN]
  mov r6, #(PART_LEN / 4)                    @ Loop counter, unrolled by 4

LOOP_PART_LEN:
  vld1.16 d0, [r2, :64]!                     @ time_signal[i]
  vld1.16 d22, [r12, :64]!                   @ time_signal[i + PART_LEN]
  vld1.16 d17, [r5, :64]!                    @ WebRtcAecm_kSqrtHanning[i]
  vld1.16 d23, [lr, :64]!                    @ kSqrtHanningReversed[i]
  vshl.s16  d18, d0, d16
  vshl.s16  d22, d22, d16
  vmull.s16 q9, d18, d17
  vmull.s16 q12, d22, d23
  subs r6, #1
  vshrn.i32 d20, q9, #14
  vshrn.i32 d26, q12, #14
  vst2.16 {d20, d21}, [r1, :128]!            @ fft[j]
  vst2.16 {d26, d27}, [r4, :128]!            @ fft[PART_LEN2 + j]
  bgt LOOP_PART_LEN

  @ WebRtcSpl_RealForwardFFT(aecm->real_fft, fft, (int16_t*)freq_signal);
  movw r12, #offset_aecm_real_fft
  sub r1, #(PART_LEN * 4)                    @ Get r1 back to &fft[0].
  mov r2, r3                                 @ freq_signal
  mov r4, r3
  ldr r0, [r0, r12]                          @ aecm->real_fft
  CALL_FUNCTION WebRtcSpl_RealForwardFFTNeon

  mov r12, #(PART_LEN * 2 / 16)              @ Loop counter, unrolled by 16.

LOOP_PART_LEN2:
  @ freq_signal[i].imag = - freq_signal[i].imag;
  vld2.16 {d20, d21, d22, d23}, [r4, :256]
  subs r12, #1
  vneg.s16 d22, d22
  vneg.s16 d23, d23
  vst2.16 {d20, d21, d22, d23}, [r4, :256]!
  bgt LOOP_PART_LEN2

  pop {r4, r5, r6, pc}

@ void WebRtcAecm_InverseFFTAndWindowNeon(AecmCore_t* aecm,
@                                         int16_t* fft,
@                                         complex16_t* efw,
@                                         int16_t* output,
@                                         const int16_t* nearendClean);
.align 2
DEFINE_FUNCTION WebRtcAecm_InverseFFTAndWindowNeon
  push {r4-r8, lr}

  @ Values of r0, r1, and r3 will change in WebRtcSpl_ComplexIFFT
  @ and WebRtcSpl_ComplexBitReverse.
  mov r4, r1
  mov r5, r0
  mov r7, r3

  add r3, r1, #((PART_LEN4 - 6) * 2)         @ &fft[PART_LEN4 - 6]
  mov r6, #(PART_LEN / 4)                    @ Loop counter, unrolled by 4
  add r12, r2, #(PART_LEN * 4)               @ &efw[PART_LEN]
  mov r8, #-16

LOOP_PRE_IFFT:
  vld2.16 {q10}, [r2, :128]!
  vmov q11, q10
  vneg.s16 d23, d23
  vst2.16 {d22, d23}, [r1, :128]!
  vrev64.16 q10, q10
  subs r6, #1
  vst2.16 {q10}, [r3], r8
  bgt LOOP_PRE_IFFT

  @  fft[PART_LEN2] = efw[PART_LEN].real;
  @  fft[PART_LEN2 + 1] = -efw[PART_LEN].imag;
  ldr r8, [r12]
  ssub16 r12, r6, r8
  mov r3, #(PART_LEN2 * 2)
  pkhbt r8, r8, r12
  str r8, [r4, r3]

  @ outCFFT = WebRtcSpl_RealInverseFFT(aecm->real_fft, fft, (int16_t*)efw);
  movw r12, #offset_aecm_real_fft
  sub r1, #(PART_LEN * 4)                    @ Get r1 back to &fft[0].
  sub r2, #(PART_LEN * 4)                    @ Get r2 back to &efw[0].
  mov r4, r2                                 @ Keep efw in r4.
  ldr r0, [r0, r12]                          @ aecm->real_fft
  CALL_FUNCTION WebRtcSpl_RealInverseFFTNeon

  movw r6, #offset_aecm_outBuf
  movw r12, #offset_aecm_dfaCleanQDomain
  ldr r8, [r5, r6]                           @ &aecm->outBuf[0]
  ldrsh r2, [r5, r12]                        @ &aecm->dfaCleanQDomain[0]

  adr r12, kSqrtHanningReversed
  adr r6, WebRtcAecm_kSqrtHanning
  rsb r0, r2, r0                             @ outCFFT - aecm->dfaCleanQDomain
  vdup.32 q9, r0
  add r0, r4, #(PART_LEN * 4)                @ &efw[PART_LEN]
  mov r3, #(PART_LEN / 4)                    @ Loop counter, unrolled by 4

LOOP_POST_IFFT:
  vld2.16 {d4, d5}, [r4, :128]               @ &efw[i];
  vld1.16 d17, [r6, :64]!                    @ WebRtcAecm_kSqrtHanning[i]
  vld1.16 d20, [r8, :64]                     @ aecm->outBuf[i]
  vmull.s16 q8, d4, d17
  vmovl.s16 q10, d20
  vrshr.s32 q8, q8, #14
  vld1.16 d0, [r0, :64]!                     @ &efw[PART_LEN + i]
  vshl.s32 q8, q8, q9
  vld1.16 d1, [r12, :64]!                    @ kSqrtHanningReversed[i]
  vadd.i32 q8, q10
  vmull.s16 q0, d0, d1
  vqmovn.s32 d16, q8
  vshr.s32 q0, q0, #14
  vst2.16 {d4, d5}, [r4, :128]!              @ &efw[i];
  vshl.s32 q0, q0, q9
  vst1.16 d16, [r7, :64]!                    @ output[i]
  vqmovn.s32 d0, q0
  subs r3, #1
  vst1.16 d0, [r8, :64]!                     @ aecm->outBuf[i]
  bgt LOOP_POST_IFFT

  movw r3, #offset_aecm_xBuf
  movw r12, #offset_aecm_dBufNoisy
  ldr r3, [r5, r3]                           @ &aecm->xBuf[0]
  ldr r1, [r5, r12]                          @ &aecm->dBufNoisy[0]
  add r2, r3, #(PART_LEN * 2)                @ &aecm->xBuf[PART_LEN]
  add r0, r1, #(PART_LEN * 2)                @ &aecm->dBufNoisy[PART_LEN]
  mov r4, #(PART_LEN / 16)                   @ Loop counter, unrolled by 16.

LOOP_COPY:
  vld1.16 {q10, q11}, [r2, :256]!
  vld1.16 {q12, q13}, [r0, :256]!
  subs r4, #1
  vst1.16 {q10, q11}, [r3, :256]!
  vst1.16 {q12, q13}, [r1, :256]!
  bgt LOOP_COPY

  ldr r2, [sp, #16]
  cmp r2, #0                                  @ Check if (nearendClean != NULL).
  beq END

  movw r4, #offset_aecm_dBufClean
  ldr r1, [r5, r4]                            @ &aecm->dBufClean[0]
  add r0, r1, #(PART_LEN * 2)                 @ &aecm->dBufClean[PART_LEN]

  vld1.16 {q10, q11}, [r0, :256]!
  vld1.16 {q12, q13}, [r0, :256]!
  vst1.16 {q10, q11}, [r1, :256]!
  vst1.16 {q12, q13}, [r1, :256]!
  vld1.16 {q10, q11}, [r0, :256]!
  vld1.16 {q12, q13}, [r0, :256]!
  vst1.16 {q10, q11}, [r1, :256]!
  vst1.16 {q12, q13}, [r1, :256]!

END:
  pop {r4-r8, pc}

@ void WebRtcAecm_CalcLinearEnergiesNeon(AecmCore_t* aecm,
@                                        const uint16_t* far_spectrum,
@                                        int32_t* echo_est,
@                                        uint32_t* far_energy,
@                                        uint32_t* echo_energy_adapt,
@                                        uint32_t* echo_energy_stored);
.align 2
DEFINE_FUNCTION WebRtcAecm_CalcLinearEnergiesNeon
  push {r4-r7}

  vmov.i32 q14, #0
  vmov.i32 q8,  #0
  vmov.i32 q9,  #0

  movw r7, #offset_aecm_channelStored
  movw r5, #offset_aecm_channelAdapt16

  mov r4, r2
  mov r12, #(PART_LEN / 8)                   @  Loop counter, unrolled by 8.
  ldr r6, [r0, r7]
  ldr r7, [r0, r5]

LOOP_CALC_LINEAR_ENERGIES:
  vld1.16 {d26, d27}, [r1]!                  @ far_spectrum[i]
  vld1.16 {d24, d25}, [r6, :128]!            @ &aecm->channelStored[i]
  vld1.16 {d0, d1}, [r7, :128]!              @ &aecm->channelAdapt16[i]
  vaddw.u16 q14, q14, d26
  vmull.u16 q10, d26, d24
  vmull.u16 q11, d27, d25
  vaddw.u16 q14, q14, d27
  vmull.u16 q1, d26, d0
  vst1.32 {q10, q11}, [r4, :256]!            @ &echo_est[i]
  vadd.u32 q8, q10
  vmull.u16 q2, d27, d1
  vadd.u32 q8, q11
  vadd.u32 q9, q1
  subs r12, #1
  vadd.u32 q9, q2
  bgt LOOP_CALC_LINEAR_ENERGIES

  vadd.u32 d28, d29
  vpadd.u32 d28, d28
  vmov.32 r12, d28[0]
  vadd.u32 d18, d19
  vpadd.u32 d18, d18
  vmov.32 r5, d18[0]                         @ echo_energy_adapt_r
  vadd.u32 d16, d17
  vpadd.u32 d16, d16

  ldrh  r1, [r1]                             @ far_spectrum[i]
  add r12, r12, r1
  str r12, [r3]                              @ far_energy
  vmov.32 r2, d16[0]

  ldrsh r12, [r6]                            @ aecm->channelStored[i]
  ldrh  r6, [r7]                             @ aecm->channelAdapt16[i]
  mul r0, r12, r1
  mla r1, r6, r1, r5
  add r2, r2, r0
  str r0, [r4]                               @ echo_est[i]
  ldr r4, [sp, #20]                          @ &echo_energy_stored
  str r2, [r4]
  ldr r3, [sp, #16]                          @ &echo_energy_adapt
  str r1, [r3]

  pop {r4-r7}
  bx  lr

@ void WebRtcAecm_StoreAdaptiveChannelNeon(AecmCore_t* aecm,
@                                          const uint16_t* far_spectrum,
@                                          int32_t* echo_est);
.align 2
DEFINE_FUNCTION WebRtcAecm_StoreAdaptiveChannelNeon
  movw r3, #offset_aecm_channelAdapt16
  movw r12, #offset_aecm_channelStored
  ldr r3, [r0, r3]
  ldr r0, [r0, r12]
  mov r12, #(PART_LEN / 8)                   @ Loop counter, unrolled by 8.

LOOP_STORE_ADAPTIVE_CHANNEL:
  vld1.16 {d24, d25}, [r3, :128]!            @ &aecm->channelAdapt16[i]
  vld1.16 {d26, d27}, [r1]!                  @ &far_spectrum[i]
  vst1.16 {d24, d25}, [r0, :128]!            @ &aecm->channelStored[i]
  vmull.u16 q10, d26, d24
  vmull.u16 q11, d27, d25
  vst1.16 {q10, q11}, [r2, :256]!            @ echo_est[i]
  subs r12, #1
  bgt LOOP_STORE_ADAPTIVE_CHANNEL

  ldrsh  r12, [r3]
  strh  r12, [r0]
  ldrh  r1, [r1]
  mul r3, r1, r12
  str r3, [r2]

  bx  lr

@ void WebRtcAecm_ResetAdaptiveChannelNeon(AecmCore_t* aecm);
.align 2
DEFINE_FUNCTION WebRtcAecm_ResetAdaptiveChannelNeon
  movw r1, #offset_aecm_channelAdapt16
  movw r2, #offset_aecm_channelAdapt32
  movw r3, #offset_aecm_channelStored
  ldr r1, [r0, r1]                           @ &aecm->channelAdapt16[0]
  ldr r2, [r0, r2]                           @ &aecm->channelAdapt32[0]
  ldr r0, [r0, r3]                           @ &aecm->channelStored[0]
  mov r3, #(PART_LEN / 8)                    @ Loop counter, unrolled by 8.

LOOP_RESET_ADAPTIVE_CHANNEL:
  vld1.16 {d24, d25}, [r0, :128]!
  subs r3, #1
  vst1.16 {d24, d25}, [r1, :128]!
  vshll.s16 q10, d24, #16
  vshll.s16 q11, d25, #16
  vst1.16 {q10, q11}, [r2, :256]!
  bgt LOOP_RESET_ADAPTIVE_CHANNEL

  ldrh  r0, [r0]
  strh  r0, [r1]
  mov r0, r0, asl #16
  str r0, [r2]

  bx  lr

@ Square root of Hanning window in Q14.
.align 4
WebRtcAecm_kSqrtHanning:
_WebRtcAecm_kSqrtHanning:
  .short 0
  .short 399, 798, 1196, 1594, 1990, 2386, 2780, 3172
  .short 3562, 3951, 4337, 4720, 5101, 5478, 5853, 6224
  .short 6591, 6954, 7313, 7668, 8019, 8364, 8705, 9040
  .short 9370, 9695, 10013, 10326, 10633, 10933, 11227, 11514
  .short 11795, 12068, 12335, 12594, 12845, 13089, 13325, 13553
  .short 13773, 13985, 14189, 14384, 14571, 14749, 14918, 15079
  .short 15231, 15373, 15506, 15631, 15746, 15851, 15947, 16034
  .short 16111, 16179, 16237, 16286, 16325, 16354, 16373, 16384

@ Square root of Hanning window in Q14. Compared to WebRtcAecm_kSqrtHanning,
@ the order was reversed and one element (0) was removed.
.align 4
kSqrtHanningReversed:
  .short 16384, 16373, 16354, 16325, 16286, 16237, 16179, 16111, 16034, 15947
  .short 15851, 15746, 15631, 15506, 15373, 15231, 15079, 14918, 14749, 14571
  .short 14384, 14189, 13985, 13773, 13553, 13325, 13089, 12845, 12594, 12335
  .short 12068, 11795, 11514, 11227, 10933, 10633, 10326, 10013, 9695, 9370
  .short 9040, 8705, 8364, 8019, 7668, 7313, 6954, 6591, 6224, 5853, 5478, 5101
  .short 4720, 4337, 3951, 3562, 3172, 2780, 2386, 1990, 1594, 1196, 798, 399
