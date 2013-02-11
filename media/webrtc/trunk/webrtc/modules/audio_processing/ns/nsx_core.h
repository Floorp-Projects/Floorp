/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_PROCESSING_NS_MAIN_SOURCE_NSX_CORE_H_
#define WEBRTC_MODULES_AUDIO_PROCESSING_NS_MAIN_SOURCE_NSX_CORE_H_

#include "common_audio/signal_processing/include/signal_processing_library.h"
#include "modules/audio_processing/ns/nsx_defines.h"
#include "typedefs.h"

#ifdef NS_FILEDEBUG
#include <stdio.h>
#endif

typedef struct NsxInst_t_ {
  WebRtc_UWord32          fs;

  const WebRtc_Word16*    window;
  WebRtc_Word16           analysisBuffer[ANAL_BLOCKL_MAX];
  WebRtc_Word16           synthesisBuffer[ANAL_BLOCKL_MAX];
  WebRtc_UWord16          noiseSupFilter[HALF_ANAL_BLOCKL];
  WebRtc_UWord16          overdrive; /* Q8 */
  WebRtc_UWord16          denoiseBound; /* Q14 */
  const WebRtc_Word16*    factor2Table;
  WebRtc_Word16           noiseEstLogQuantile[SIMULT* HALF_ANAL_BLOCKL];
  WebRtc_Word16           noiseEstDensity[SIMULT* HALF_ANAL_BLOCKL];
  WebRtc_Word16           noiseEstCounter[SIMULT];
  WebRtc_Word16           noiseEstQuantile[HALF_ANAL_BLOCKL];

  WebRtc_Word16           anaLen;
  int                     anaLen2;
  int                     magnLen;
  int                     aggrMode;
  int                     stages;
  int                     initFlag;
  int                     gainMap;

  WebRtc_Word32           maxLrt;
  WebRtc_Word32           minLrt;
  WebRtc_Word32           logLrtTimeAvgW32[HALF_ANAL_BLOCKL]; //log lrt factor with time-smoothing in Q8
  WebRtc_Word32           featureLogLrt;
  WebRtc_Word32           thresholdLogLrt;
  WebRtc_Word16           weightLogLrt;

  WebRtc_UWord32          featureSpecDiff;
  WebRtc_UWord32          thresholdSpecDiff;
  WebRtc_Word16           weightSpecDiff;

  WebRtc_UWord32          featureSpecFlat;
  WebRtc_UWord32          thresholdSpecFlat;
  WebRtc_Word16           weightSpecFlat;

  WebRtc_Word32           avgMagnPause[HALF_ANAL_BLOCKL]; //conservative estimate of noise spectrum
  WebRtc_UWord32          magnEnergy;
  WebRtc_UWord32          sumMagn;
  WebRtc_UWord32          curAvgMagnEnergy;
  WebRtc_UWord32          timeAvgMagnEnergy;
  WebRtc_UWord32          timeAvgMagnEnergyTmp;

  WebRtc_UWord32          whiteNoiseLevel;              //initial noise estimate
  WebRtc_UWord32          initMagnEst[HALF_ANAL_BLOCKL];//initial magnitude spectrum estimate
  WebRtc_Word32           pinkNoiseNumerator;           //pink noise parameter: numerator
  WebRtc_Word32           pinkNoiseExp;                 //pink noise parameter: power of freq
  int                     minNorm;                      //smallest normalization factor
  int                     zeroInputSignal;              //zero input signal flag

  WebRtc_UWord32          prevNoiseU32[HALF_ANAL_BLOCKL]; //noise spectrum from previous frame
  WebRtc_UWord16          prevMagnU16[HALF_ANAL_BLOCKL]; //magnitude spectrum from previous frame
  WebRtc_Word16           priorNonSpeechProb; //prior speech/noise probability // Q14

  int                     blockIndex; //frame index counter
  int                     modelUpdate; //parameter for updating or estimating thresholds/weights for prior model
  int                     cntThresUpdate;

  //histograms for parameter estimation
  WebRtc_Word16           histLrt[HIST_PAR_EST];
  WebRtc_Word16           histSpecFlat[HIST_PAR_EST];
  WebRtc_Word16           histSpecDiff[HIST_PAR_EST];

  //quantities for high band estimate
  WebRtc_Word16           dataBufHBFX[ANAL_BLOCKL_MAX]; /* Q0 */

  int                     qNoise;
  int                     prevQNoise;
  int                     prevQMagn;
  int                     blockLen10ms;

  WebRtc_Word16           real[ANAL_BLOCKL_MAX];
  WebRtc_Word16           imag[ANAL_BLOCKL_MAX];
  WebRtc_Word32           energyIn;
  int                     scaleEnergyIn;
  int                     normData;

  struct RealFFT* real_fft;
} NsxInst_t;

#ifdef __cplusplus
extern "C"
{
#endif

/****************************************************************************
 * WebRtcNsx_InitCore(...)
 *
 * This function initializes a noise suppression instance
 *
 * Input:
 *      - inst          : Instance that should be initialized
 *      - fs            : Sampling frequency
 *
 * Output:
 *      - inst          : Initialized instance
 *
 * Return value         :  0 - Ok
 *                        -1 - Error
 */
WebRtc_Word32 WebRtcNsx_InitCore(NsxInst_t* inst, WebRtc_UWord32 fs);

/****************************************************************************
 * WebRtcNsx_set_policy_core(...)
 *
 * This changes the aggressiveness of the noise suppression method.
 *
 * Input:
 *      - inst       : Instance that should be initialized
 *      - mode       : 0: Mild (6 dB), 1: Medium (10 dB), 2: Aggressive (15 dB)
 *
 * Output:
 *      - inst       : Initialized instance
 *
 * Return value      :  0 - Ok
 *                     -1 - Error
 */
int WebRtcNsx_set_policy_core(NsxInst_t* inst, int mode);

/****************************************************************************
 * WebRtcNsx_ProcessCore
 *
 * Do noise suppression.
 *
 * Input:
 *      - inst          : Instance that should be initialized
 *      - inFrameLow    : Input speech frame for lower band
 *      - inFrameHigh   : Input speech frame for higher band
 *
 * Output:
 *      - inst          : Updated instance
 *      - outFrameLow   : Output speech frame for lower band
 *      - outFrameHigh  : Output speech frame for higher band
 *
 * Return value         :  0 - OK
 *                        -1 - Error
 */
int WebRtcNsx_ProcessCore(NsxInst_t* inst,
                          short* inFrameLow,
                          short* inFrameHigh,
                          short* outFrameLow,
                          short* outFrameHigh);

/****************************************************************************
 * Some function pointers, for internal functions shared by ARM NEON and 
 * generic C code.
 */
// Noise Estimation.
typedef void (*NoiseEstimation)(NsxInst_t* inst,
                                uint16_t* magn,
                                uint32_t* noise,
                                int16_t* q_noise);
extern NoiseEstimation WebRtcNsx_NoiseEstimation;

// Filter the data in the frequency domain, and create spectrum.
typedef void (*PrepareSpectrum)(NsxInst_t* inst,
                                int16_t* freq_buff);
extern PrepareSpectrum WebRtcNsx_PrepareSpectrum;

// For the noise supression process, synthesis, read out fully processed
// segment, and update synthesis buffer.
typedef void (*SynthesisUpdate)(NsxInst_t* inst,
                                int16_t* out_frame,
                                int16_t gain_factor);
extern SynthesisUpdate WebRtcNsx_SynthesisUpdate;

// Update analysis buffer for lower band, and window data before FFT.
typedef void (*AnalysisUpdate)(NsxInst_t* inst,
                               int16_t* out,
                               int16_t* new_speech);
extern AnalysisUpdate WebRtcNsx_AnalysisUpdate;

// Denormalize the input buffer.
typedef void (*Denormalize)(NsxInst_t* inst,
                            int16_t* in,
                            int factor);
extern Denormalize WebRtcNsx_Denormalize;

// Create a complex number buffer, as the intput interleaved with zeros,
// and normalize it.
typedef void (*CreateComplexBuffer)(NsxInst_t* inst,
                                    int16_t* in,
                                    int16_t* out);
extern CreateComplexBuffer WebRtcNsx_CreateComplexBuffer;

#if (defined WEBRTC_DETECT_ARM_NEON) || defined (WEBRTC_ARCH_ARM_NEON)
// For the above function pointers, functions for generic platforms are declared
// and defined as static in file nsx_core.c, while those for ARM Neon platforms
// are declared below and defined in file nsx_core_neon.S.
void WebRtcNsx_NoiseEstimationNeon(NsxInst_t* inst,
                                   uint16_t* magn,
                                   uint32_t* noise,
                                   int16_t* q_noise);
void WebRtcNsx_CreateComplexBufferNeon(NsxInst_t* inst,
                                       int16_t* in,
                                       int16_t* out);
void WebRtcNsx_SynthesisUpdateNeon(NsxInst_t* inst,
                                   int16_t* out_frame,
                                   int16_t gain_factor);
void WebRtcNsx_AnalysisUpdateNeon(NsxInst_t* inst,
                                  int16_t* out,
                                  int16_t* new_speech);
void WebRtcNsx_DenormalizeNeon(NsxInst_t* inst, int16_t* in, int factor);
void WebRtcNsx_PrepareSpectrumNeon(NsxInst_t* inst, int16_t* freq_buff);
#endif

extern const WebRtc_Word16 WebRtcNsx_kLogTable[9];
extern const WebRtc_Word16 WebRtcNsx_kLogTableFrac[256];
extern const WebRtc_Word16 WebRtcNsx_kCounterDiv[201];

#ifdef __cplusplus
}
#endif

#endif  // WEBRTC_MODULES_AUDIO_PROCESSING_NS_MAIN_SOURCE_NSX_CORE_H_
