/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_PROCESSING_NS_NS_CORE_H_
#define WEBRTC_MODULES_AUDIO_PROCESSING_NS_NS_CORE_H_

#include "webrtc/modules/audio_processing/ns/defines.h"

typedef struct NSParaExtract_t_ {
  // Bin size of histogram.
  float binSizeLrt;
  float binSizeSpecFlat;
  float binSizeSpecDiff;
  // Range of histogram over which LRT threshold is computed.
  float rangeAvgHistLrt;
  // Scale parameters: multiply dominant peaks of the histograms by scale factor
  // to obtain thresholds for prior model.
  float factor1ModelPars;  // For LRT and spectral difference.
  float factor2ModelPars;  // For spectral_flatness: used when noise is flatter
                           // than speech.
  // Peak limit for spectral flatness (varies between 0 and 1).
  float thresPosSpecFlat;
  // Limit on spacing of two highest peaks in histogram: spacing determined by
  // bin size.
  float limitPeakSpacingSpecFlat;
  float limitPeakSpacingSpecDiff;
  // Limit on relevance of second peak.
  float limitPeakWeightsSpecFlat;
  float limitPeakWeightsSpecDiff;
  // Limit on fluctuation of LRT feature.
  float thresFluctLrt;
  // Limit on the max and min values for the feature thresholds.
  float maxLrt;
  float minLrt;
  float maxSpecFlat;
  float minSpecFlat;
  float maxSpecDiff;
  float minSpecDiff;
  // Criteria of weight of histogram peak to accept/reject feature.
  int thresWeightSpecFlat;
  int thresWeightSpecDiff;

} NSParaExtract_t;

typedef struct NSinst_t_ {
  uint32_t fs;
  int blockLen;
  int windShift;
  int anaLen;
  int magnLen;
  int aggrMode;
  const float* window;
  float analyzeBuf[ANAL_BLOCKL_MAX];
  float dataBuf[ANAL_BLOCKL_MAX];
  float syntBuf[ANAL_BLOCKL_MAX];

  int initFlag;
  // Parameters for quantile noise estimation.
  float density[SIMULT * HALF_ANAL_BLOCKL];
  float lquantile[SIMULT * HALF_ANAL_BLOCKL];
  float quantile[HALF_ANAL_BLOCKL];
  int counter[SIMULT];
  int updates;
  // Parameters for Wiener filter.
  float smooth[HALF_ANAL_BLOCKL];
  float overdrive;
  float denoiseBound;
  int gainmap;
  // FFT work arrays.
  int ip[IP_LENGTH];
  float wfft[W_LENGTH];

  // Parameters for new method: some not needed, will reduce/cleanup later.
  int32_t blockInd;  // Frame index counter.
  int modelUpdatePars[4];  // Parameters for updating or estimating.
  // Thresholds/weights for prior model.
  float priorModelPars[7];  // Parameters for prior model.
  float noise[HALF_ANAL_BLOCKL];  // Noise spectrum from current frame.
  float noisePrev[HALF_ANAL_BLOCKL];  // Noise spectrum from previous frame.
  // Magnitude spectrum of previous analyze frame.
  float magnPrevAnalyze[HALF_ANAL_BLOCKL];
  // Magnitude spectrum of previous process frame.
  float magnPrevProcess[HALF_ANAL_BLOCKL];
  float logLrtTimeAvg[HALF_ANAL_BLOCKL];  // Log LRT factor with time-smoothing.
  float priorSpeechProb;  // Prior speech/noise probability.
  float featureData[7];
  // Conservative noise spectrum estimate.
  float magnAvgPause[HALF_ANAL_BLOCKL];
  float signalEnergy;  // Energy of |magn|.
  float sumMagn;
  float whiteNoiseLevel;  // Initial noise estimate.
  float initMagnEst[HALF_ANAL_BLOCKL];  // Initial magnitude spectrum estimate.
  float pinkNoiseNumerator;  // Pink noise parameter: numerator.
  float pinkNoiseExp;  // Pink noise parameter: power of frequencies.
  float parametricNoise[HALF_ANAL_BLOCKL];
  // Parameters for feature extraction.
  NSParaExtract_t featureExtractionParams;
  // Histograms for parameter estimation.
  int histLrt[HIST_PAR_EST];
  int histSpecFlat[HIST_PAR_EST];
  int histSpecDiff[HIST_PAR_EST];
  // Quantities for high band estimate.
  float speechProb[HALF_ANAL_BLOCKL];  // Final speech/noise prob: prior + LRT.
  float dataBufHB[ANAL_BLOCKL_MAX];  // Buffering data for HB.

} NSinst_t;

#ifdef __cplusplus
extern "C" {
#endif

/****************************************************************************
 * WebRtcNs_InitCore(...)
 *
 * This function initializes a noise suppression instance
 *
 * Input:
 *      - self          : Instance that should be initialized
 *      - fs            : Sampling frequency
 *
 * Output:
 *      - self          : Initialized instance
 *
 * Return value         :  0 - Ok
 *                        -1 - Error
 */
int WebRtcNs_InitCore(NSinst_t* self, uint32_t fs);

/****************************************************************************
 * WebRtcNs_set_policy_core(...)
 *
 * This changes the aggressiveness of the noise suppression method.
 *
 * Input:
 *      - self          : Instance that should be initialized
 *      - mode          : 0: Mild (6dB), 1: Medium (10dB), 2: Aggressive (15dB)
 *
 * Output:
 *      - self          : Initialized instance
 *
 * Return value         :  0 - Ok
 *                        -1 - Error
 */
int WebRtcNs_set_policy_core(NSinst_t* self, int mode);

/****************************************************************************
 * WebRtcNs_AnalyzeCore
 *
 * Estimate the background noise.
 *
 * Input:
 *      - self          : Instance that should be initialized
 *      - speechFrame   : Input speech frame for lower band
 *
 * Output:
 *      - self          : Updated instance
 *
 * Return value         :  0 - OK
 *                        -1 - Error
 */
int WebRtcNs_AnalyzeCore(NSinst_t* self, float* speechFrame);

/****************************************************************************
 * WebRtcNs_ProcessCore
 *
 * Do noise suppression.
 *
 * Input:
 *      - self          : Instance that should be initialized
 *      - inFrameLow    : Input speech frame for lower band
 *      - inFrameHigh   : Input speech frame for higher band
 *
 * Output:
 *      - self          : Updated instance
 *      - outFrameLow   : Output speech frame for lower band
 *      - outFrameHigh  : Output speech frame for higher band
 *
 * Return value         :  0 - OK
 *                        -1 - Error
 */
int WebRtcNs_ProcessCore(NSinst_t* self,
                         float* inFrameLow,
                         float* inFrameHigh,
                         float* outFrameLow,
                         float* outFrameHigh);

#ifdef __cplusplus
}
#endif
#endif  // WEBRTC_MODULES_AUDIO_PROCESSING_NS_NS_CORE_H_
