/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "aecm_core.h"

#include <arm_neon.h>
#include <assert.h>


// Square root of Hanning window in Q14.
static const WebRtc_Word16 kSqrtHanningReversed[] __attribute__((aligned(8))) = {
  16384, 16373, 16354, 16325, 
  16286, 16237, 16179, 16111,
  16034, 15947, 15851, 15746,
  15631, 15506, 15373, 15231,
  15079, 14918, 14749, 14571,
  14384, 14189, 13985, 13773,
  13553, 13325, 13089, 12845,
  12594, 12335, 12068, 11795,
  11514, 11227, 10933, 10633,
  10326, 10013, 9695,  9370,
  9040,  8705,  8364,  8019,
  7668,  7313,  6954,  6591,
  6224,  5853,  5478,  5101,
  4720,  4337,  3951,  3562,
  3172,  2780,  2386,  1990,
  1594,  1196,  798,   399
};

static void WindowAndFFTNeon(WebRtc_Word16* fft,
                             const WebRtc_Word16* time_signal,
                             complex16_t* freq_signal,
                             int time_signal_scaling) {
  int i, j;

  int16x4_t tmp16x4_scaling = vdup_n_s16(time_signal_scaling);
  __asm__("vmov.i16 d21, #0" ::: "d21");

  for (i = 0, j = 0; i < PART_LEN; i += 4, j += 8) {
    int16x4_t tmp16x4_0;
    int16x4_t tmp16x4_1;
    int32x4_t tmp32x4_0;

    /* Window near end */
    // fft[j] = (WebRtc_Word16)WEBRTC_SPL_MUL_16_16_RSFT((time_signal[i]
    //       << time_signal_scaling), WebRtcAecm_kSqrtHanning[i], 14);
    __asm__("vld1.16 %P0, [%1, :64]" : "=w"(tmp16x4_0) : "r"(&time_signal[i]));
    tmp16x4_0 = vshl_s16(tmp16x4_0, tmp16x4_scaling);

    __asm__("vld1.16 %P0, [%1, :64]" : "=w"(tmp16x4_1) : "r"(&WebRtcAecm_kSqrtHanning[i]));
    tmp32x4_0 = vmull_s16(tmp16x4_0, tmp16x4_1);

    __asm__("vshrn.i32 d20, %q0, #14" : : "w"(tmp32x4_0) : "d20");
    __asm__("vst2.16 {d20, d21}, [%0, :128]" : : "r"(&fft[j]) : "q10");

    // fft[PART_LEN2 + j] = (WebRtc_Word16)WEBRTC_SPL_MUL_16_16_RSFT(
    //      (time_signal[PART_LEN + i] << time_signal_scaling),
    //       WebRtcAecm_kSqrtHanning[PART_LEN - i], 14);
    __asm__("vld1.16 %P0, [%1, :64]" : "=w"(tmp16x4_0) : "r"(&time_signal[i + PART_LEN]));
    tmp16x4_0 = vshl_s16(tmp16x4_0, tmp16x4_scaling);

    __asm__("vld1.16 %P0, [%1, :64]" : "=w"(tmp16x4_1) : "r"(&kSqrtHanningReversed[i]));
    tmp32x4_0 = vmull_s16(tmp16x4_0, tmp16x4_1);

    __asm__("vshrn.i32 d20, %q0, #14" : : "w"(tmp32x4_0) : "d20");
    __asm__("vst2.16 {d20, d21}, [%0, :128]" : : "r"(&fft[PART_LEN2 + j]) : "q10");
  }

  WebRtcSpl_ComplexBitReverse(fft, PART_LEN_SHIFT);
  WebRtcSpl_ComplexFFT(fft, PART_LEN_SHIFT, 1);

  // Take only the first PART_LEN2 samples, and switch the sign of the imaginary part.
  for (i = 0, j = 0; j < PART_LEN2; i += 8, j += 16) {
    __asm__("vld2.16 {d20, d21, d22, d23}, [%0, :256]" : : "r"(&fft[j]) : "q10", "q11");
    __asm__("vneg.s16 d22, d22" : : : "q10");
    __asm__("vneg.s16 d23, d23" : : : "q11");
    __asm__("vst2.16 {d20, d21, d22, d23}, [%0, :256]" : :
            "r"(&freq_signal[i].real): "q10", "q11");
  }
}

static void InverseFFTAndWindowNeon(AecmCore_t* aecm,
                                    WebRtc_Word16* fft,
                                    complex16_t* efw,
                                    WebRtc_Word16* output,
                                    const WebRtc_Word16* nearendClean) {
  int i, j, outCFFT;
  WebRtc_Word32 tmp32no1;

  // Synthesis
  for (i = 0, j = 0; i < PART_LEN; i += 4, j += 8) {
    // We overwrite two more elements in fft[], but it's ok.
    __asm__("vld2.16 {d20, d21}, [%0, :128]" : : "r"(&(efw[i].real)) : "q10");
    __asm__("vmov q11, q10" : : : "q10", "q11");

    __asm__("vneg.s16 d23, d23" : : : "q11");
    __asm__("vst2.16 {d22, d23}, [%0, :128]" : : "r"(&fft[j]): "q11");

    __asm__("vrev64.16 q10, q10" : : : "q10");
    __asm__("vst2.16 {d20, d21}, [%0]" : : "r"(&fft[PART_LEN4 - j - 6]): "q10");
  }

  fft[PART_LEN2] = efw[PART_LEN].real;
  fft[PART_LEN2 + 1] = -efw[PART_LEN].imag;

  // Inverse FFT, result should be scaled with outCFFT.
  WebRtcSpl_ComplexBitReverse(fft, PART_LEN_SHIFT);
  outCFFT = WebRtcSpl_ComplexIFFT(fft, PART_LEN_SHIFT, 1);

  // Take only the real values and scale with outCFFT.
  for (i = 0, j = 0; i < PART_LEN2; i += 8, j += 16) {
    __asm__("vld2.16 {d20, d21, d22, d23}, [%0, :256]" : : "r"(&fft[j]) : "q10", "q11");
    __asm__("vst1.16 {d20, d21}, [%0, :128]" : : "r"(&fft[i]): "q10");
  }

  int32x4_t tmp32x4_2;
  __asm__("vdup.32 %q0, %1" : "=w"(tmp32x4_2) : "r"((WebRtc_Word32)
      (outCFFT - aecm->dfaCleanQDomain)));
  for (i = 0; i < PART_LEN; i += 4) {
    int16x4_t tmp16x4_0;
    int16x4_t tmp16x4_1;
    int32x4_t tmp32x4_0;
    int32x4_t tmp32x4_1;

    // fft[i] = (WebRtc_Word16)WEBRTC_SPL_MUL_16_16_RSFT_WITH_ROUND(
    //        fft[i], WebRtcAecm_kSqrtHanning[i], 14);
    __asm__("vld1.16 %P0, [%1, :64]" : "=w"(tmp16x4_0) : "r"(&fft[i]));
    __asm__("vld1.16 %P0, [%1, :64]" : "=w"(tmp16x4_1) : "r"(&WebRtcAecm_kSqrtHanning[i]));
    __asm__("vmull.s16 %q0, %P1, %P2" : "=w"(tmp32x4_0) : "w"(tmp16x4_0), "w"(tmp16x4_1));
    __asm__("vrshr.s32 %q0, %q1, #14" : "=w"(tmp32x4_0) : "0"(tmp32x4_0));

    // tmp32no1 = WEBRTC_SPL_SHIFT_W32((WebRtc_Word32)fft[i],
    //        outCFFT - aecm->dfaCleanQDomain);
    __asm__("vshl.s32 %q0, %q1, %q2" : "=w"(tmp32x4_0) : "0"(tmp32x4_0), "w"(tmp32x4_2));

    // fft[i] = (WebRtc_Word16)WEBRTC_SPL_SAT(WEBRTC_SPL_WORD16_MAX,
    //        tmp32no1 + outBuf[i], WEBRTC_SPL_WORD16_MIN);
    // output[i] = fft[i];
    __asm__("vld1.16 %P0, [%1, :64]" : "=w"(tmp16x4_0) : "r"(&aecm->outBuf[i]));
    __asm__("vmovl.s16 %q0, %P1" : "=w"(tmp32x4_1) : "w"(tmp16x4_0));
    __asm__("vadd.i32 %q0, %q1" : : "w"(tmp32x4_0), "w"(tmp32x4_1));
    __asm__("vqshrn.s32 %P0, %q1, #0" : "=w"(tmp16x4_0) : "w"(tmp32x4_0));
    __asm__("vst1.16 %P0, [%1, :64]" : : "w"(tmp16x4_0), "r"(&fft[i]));
    __asm__("vst1.16 %P0, [%1, :64]" : : "w"(tmp16x4_0), "r"(&output[i]));

    // tmp32no1 = WEBRTC_SPL_MUL_16_16_RSFT(
    //        fft[PART_LEN + i], WebRtcAecm_kSqrtHanning[PART_LEN - i], 14);
    __asm__("vld1.16 %P0, [%1, :64]" : "=w"(tmp16x4_0) : "r"(&fft[PART_LEN + i]));
    __asm__("vld1.16 %P0, [%1, :64]" : "=w"(tmp16x4_1) : "r"(&kSqrtHanningReversed[i]));
    __asm__("vmull.s16 %q0, %P1, %P2" : "=w"(tmp32x4_0) : "w"(tmp16x4_0), "w"(tmp16x4_1));
    __asm__("vshr.s32 %q0, %q1, #14" : "=w"(tmp32x4_0) : "0"(tmp32x4_0));

    // tmp32no1 = WEBRTC_SPL_SHIFT_W32(tmp32no1, outCFFT - aecm->dfaCleanQDomain);
    __asm__("vshl.s32 %q0, %q1, %q2" : "=w"(tmp32x4_0) : "0"(tmp32x4_0), "w"(tmp32x4_2));
    // outBuf[i] = (WebRtc_Word16)WEBRTC_SPL_SAT(
    //        WEBRTC_SPL_WORD16_MAX, tmp32no1, WEBRTC_SPL_WORD16_MIN);
    __asm__("vqshrn.s32 %P0, %q1, #0" : "=w"(tmp16x4_0) : "w"(tmp32x4_0));
    __asm__("vst1.16 %P0, [%1, :64]" : : "w"(tmp16x4_0), "r"(&aecm->outBuf[i]));
  }

  // Copy the current block to the old position (outBuf is shifted elsewhere).
  for (i = 0; i < PART_LEN; i += 16) {
    __asm__("vld1.16 {d20, d21, d22, d23}, [%0, :256]" : :
            "r"(&aecm->xBuf[i + PART_LEN]) : "q10");
    __asm__("vst1.16 {d20, d21, d22, d23}, [%0, :256]" : : "r"(&aecm->xBuf[i]): "q10");
  }
  for (i = 0; i < PART_LEN; i += 16) {
    __asm__("vld1.16 {d20, d21, d22, d23}, [%0, :256]" : :
            "r"(&aecm->dBufNoisy[i + PART_LEN]) : "q10");
    __asm__("vst1.16 {d20, d21, d22, d23}, [%0, :256]" : :
            "r"(&aecm->dBufNoisy[i]): "q10");
  }
  if (nearendClean != NULL) {
    for (i = 0; i < PART_LEN; i += 16) {
      __asm__("vld1.16 {d20, d21, d22, d23}, [%0, :256]" : :
              "r"(&aecm->dBufClean[i + PART_LEN]) : "q10");
      __asm__("vst1.16 {d20, d21, d22, d23}, [%0, :256]" : :
              "r"(&aecm->dBufClean[i]): "q10");
    }
  }
}

static void CalcLinearEnergiesNeon(AecmCore_t* aecm,
                                   const WebRtc_UWord16* far_spectrum,
                                   WebRtc_Word32* echo_est,
                                   WebRtc_UWord32* far_energy,
                                   WebRtc_UWord32* echo_energy_adapt,
                                   WebRtc_UWord32* echo_energy_stored) {
  int i;

  register WebRtc_UWord32 far_energy_r;
  register WebRtc_UWord32 echo_energy_stored_r;
  register WebRtc_UWord32 echo_energy_adapt_r;
  uint32x4_t tmp32x4_0;

  __asm__("vmov.i32 q14, #0" : : : "q14"); // far_energy
  __asm__("vmov.i32 q8,  #0" : : : "q8"); // echo_energy_stored
  __asm__("vmov.i32 q9,  #0" : : : "q9"); // echo_energy_adapt

  for (i = 0; i < PART_LEN - 7; i += 8) {
    // far_energy += (WebRtc_UWord32)(far_spectrum[i]);
    __asm__("vld1.16 {d26, d27}, [%0]" : : "r"(&far_spectrum[i]) : "q13");
    __asm__("vaddw.u16 q14, q14, d26" : : : "q14", "q13");
    __asm__("vaddw.u16 q14, q14, d27" : : : "q14", "q13");

    // Get estimated echo energies for adaptive channel and stored channel.
    // echoEst[i] = WEBRTC_SPL_MUL_16_U16(aecm->channelStored[i], far_spectrum[i]);
    __asm__("vld1.16 {d24, d25}, [%0, :128]" : : "r"(&aecm->channelStored[i]) : "q12");
    __asm__("vmull.u16 q10, d26, d24" : : : "q12", "q13", "q10");
    __asm__("vmull.u16 q11, d27, d25" : : : "q12", "q13", "q11");
    __asm__("vst1.32 {d20, d21, d22, d23}, [%0, :256]" : : "r"(&echo_est[i]):
            "q10", "q11");

    // echo_energy_stored += (WebRtc_UWord32)echoEst[i];
    __asm__("vadd.u32 q8, q10" : : : "q10", "q8");
    __asm__("vadd.u32 q8, q11" : : : "q11", "q8");

    // echo_energy_adapt += WEBRTC_SPL_UMUL_16_16(
    //     aecm->channelAdapt16[i], far_spectrum[i]);
    __asm__("vld1.16 {d24, d25}, [%0, :128]" : : "r"(&aecm->channelAdapt16[i]) : "q12");
    __asm__("vmull.u16 q10, d26, d24" : : : "q12", "q13", "q10");
    __asm__("vmull.u16 q11, d27, d25" : : : "q12", "q13", "q11");
    __asm__("vadd.u32 q9, q10" : : : "q9", "q15");
    __asm__("vadd.u32 q9, q11" : : : "q9", "q11");
  }

  __asm__("vadd.u32 d28, d29" : : : "q14");
  __asm__("vpadd.u32 d28, d28" : : : "q14");
  __asm__("vmov.32 %0, d28[0]" : "=r"(far_energy_r): : "q14");

  __asm__("vadd.u32 d18, d19" : : : "q9");
  __asm__("vpadd.u32 d18, d18" : : : "q9");
  __asm__("vmov.32 %0, d18[0]" : "=r"(echo_energy_adapt_r): : "q9");

  __asm__("vadd.u32 d16, d17" : : : "q8");
  __asm__("vpadd.u32 d16, d16" : : : "q8");
  __asm__("vmov.32 %0, d16[0]" : "=r"(echo_energy_stored_r): : "q8");

  // Get estimated echo energies for adaptive channel and stored channel.
  echo_est[i] = WEBRTC_SPL_MUL_16_U16(aecm->channelStored[i], far_spectrum[i]);
  *echo_energy_stored = echo_energy_stored_r + (WebRtc_UWord32)echo_est[i];
  *far_energy = far_energy_r + (WebRtc_UWord32)(far_spectrum[i]);
  *echo_energy_adapt = echo_energy_adapt_r + WEBRTC_SPL_UMUL_16_16(
      aecm->channelAdapt16[i], far_spectrum[i]);
}

static void StoreAdaptiveChannelNeon(AecmCore_t* aecm,
                                     const WebRtc_UWord16* far_spectrum,
                                     WebRtc_Word32* echo_est) {
  int i;

  // During startup we store the channel every block.
  // Recalculate echo estimate.
  for (i = 0; i < PART_LEN - 7; i += 8) {
    // aecm->channelStored[i] = acem->channelAdapt16[i];
    // echo_est[i] = WEBRTC_SPL_MUL_16_U16(aecm->channelStored[i], far_spectrum[i]);
    __asm__("vld1.16 {d26, d27}, [%0]" : : "r"(&far_spectrum[i]) : "q13");
    __asm__("vld1.16 {d24, d25}, [%0, :128]" : : "r"(&aecm->channelAdapt16[i]) : "q12");
    __asm__("vst1.16 {d24, d25}, [%0, :128]" : : "r"(&aecm->channelStored[i]) : "q12");
    __asm__("vmull.u16 q10, d26, d24" : : : "q12", "q13", "q10");
    __asm__("vmull.u16 q11, d27, d25" : : : "q12", "q13", "q11");
    __asm__("vst1.16 {d20, d21, d22, d23}, [%0, :256]" : :
            "r"(&echo_est[i]) : "q10", "q11");
  }
  aecm->channelStored[i] = aecm->channelAdapt16[i];
  echo_est[i] = WEBRTC_SPL_MUL_16_U16(aecm->channelStored[i], far_spectrum[i]);
}

static void ResetAdaptiveChannelNeon(AecmCore_t* aecm) {
  int i;

  for (i = 0; i < PART_LEN - 7; i += 8) {
    // aecm->channelAdapt16[i] = aecm->channelStored[i];
    // aecm->channelAdapt32[i] = WEBRTC_SPL_LSHIFT_W32((WebRtc_Word32)
    //                           aecm->channelStored[i], 16);
    __asm__("vld1.16 {d24, d25}, [%0, :128]" : :
            "r"(&aecm->channelStored[i]) : "q12");
    __asm__("vst1.16 {d24, d25}, [%0, :128]" : :
            "r"(&aecm->channelAdapt16[i]) : "q12");
    __asm__("vshll.s16 q10, d24, #16" : : : "q12", "q13", "q10");
    __asm__("vshll.s16 q11, d25, #16" : : : "q12", "q13", "q11");
    __asm__("vst1.16 {d20, d21, d22, d23}, [%0, :256]" : :
            "r"(&aecm->channelAdapt32[i]): "q10", "q11");
  }
  aecm->channelAdapt16[i] = aecm->channelStored[i];
  aecm->channelAdapt32[i] = WEBRTC_SPL_LSHIFT_W32(
      (WebRtc_Word32)aecm->channelStored[i], 16);
}

void WebRtcAecm_InitNeon(void) {
  WebRtcAecm_WindowAndFFT = WindowAndFFTNeon;
  WebRtcAecm_InverseFFTAndWindow = InverseFFTAndWindowNeon;
  WebRtcAecm_CalcLinearEnergies = CalcLinearEnergiesNeon;
  WebRtcAecm_StoreAdaptiveChannel = StoreAdaptiveChannelNeon;
  WebRtcAecm_ResetAdaptiveChannel = ResetAdaptiveChannelNeon;
}
