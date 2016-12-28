/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/*
 * The core AEC algorithm, which is presented with time-aligned signals.
 */

#include "webrtc/modules/audio_processing/aec/aec_core.h"

#ifdef WEBRTC_AEC_DEBUG_DUMP
#include <stdio.h>
#endif

#include <assert.h>
#include <math.h>
#include <stddef.h>  // size_t
#include <stdlib.h>
#include <string.h>

#include "webrtc/common_audio/ring_buffer.h"
#include "webrtc/common_audio/signal_processing/include/signal_processing_library.h"
#include "webrtc/modules/audio_processing/aec/aec_common.h"
#include "webrtc/modules/audio_processing/aec/aec_core_internal.h"
#include "webrtc/modules/audio_processing/aec/aec_rdft.h"
#include "webrtc/modules/audio_processing/logging/aec_logging.h"
#include "webrtc/modules/audio_processing/utility/delay_estimator_wrapper.h"
#include "webrtc/system_wrappers/include/cpu_features_wrapper.h"
#include "webrtc/typedefs.h"

extern int AECDebug();
extern uint32_t AECDebugMaxSize();
extern void AECDebugEnable(uint32_t enable);
extern void AECDebugFilenameBase(char *buffer, size_t size);
static void OpenCoreDebugFiles(AecCore* aec, int *instance_count);

// Buffer size (samples)
static const size_t kBufSizePartitions = 250;  // 1 second of audio in 16 kHz.

// Metrics
static const int subCountLen = 4;
static const int countLen = 50;
static const int kDelayMetricsAggregationWindow = 1250;  // 5 seconds at 16 kHz.

// Quantities to control H band scaling for SWB input
static const float cnScaleHband =
    (float)0.4;  // scale for comfort noise in H band
// Initial bin for averaging nlp gain in low band
static const int freqAvgIc = PART_LEN / 2;

// Matlab code to produce table:
// win = sqrt(hanning(63)); win = [0 ; win(1:32)];
// fprintf(1, '\t%.14f, %.14f, %.14f,\n', win);
ALIGN16_BEG const float ALIGN16_END WebRtcAec_sqrtHanning[65] = {
    0.00000000000000f, 0.02454122852291f, 0.04906767432742f, 0.07356456359967f,
    0.09801714032956f, 0.12241067519922f, 0.14673047445536f, 0.17096188876030f,
    0.19509032201613f, 0.21910124015687f, 0.24298017990326f, 0.26671275747490f,
    0.29028467725446f, 0.31368174039889f, 0.33688985339222f, 0.35989503653499f,
    0.38268343236509f, 0.40524131400499f, 0.42755509343028f, 0.44961132965461f,
    0.47139673682600f, 0.49289819222978f, 0.51410274419322f, 0.53499761988710f,
    0.55557023301960f, 0.57580819141785f, 0.59569930449243f, 0.61523159058063f,
    0.63439328416365f, 0.65317284295378f, 0.67155895484702f, 0.68954054473707f,
    0.70710678118655f, 0.72424708295147f, 0.74095112535496f, 0.75720884650648f,
    0.77301045336274f, 0.78834642762661f, 0.80320753148064f, 0.81758481315158f,
    0.83146961230255f, 0.84485356524971f, 0.85772861000027f, 0.87008699110871f,
    0.88192126434835f, 0.89322430119552f, 0.90398929312344f, 0.91420975570353f,
    0.92387953251129f, 0.93299279883474f, 0.94154406518302f, 0.94952818059304f,
    0.95694033573221f, 0.96377606579544f, 0.97003125319454f, 0.97570213003853f,
    0.98078528040323f, 0.98527764238894f, 0.98917650996478f, 0.99247953459871f,
    0.99518472667220f, 0.99729045667869f, 0.99879545620517f, 0.99969881869620f,
    1.00000000000000f};

// Matlab code to produce table:
// weightCurve = [0 ; 0.3 * sqrt(linspace(0,1,64))' + 0.1];
// fprintf(1, '\t%.4f, %.4f, %.4f, %.4f, %.4f, %.4f,\n', weightCurve);
ALIGN16_BEG const float ALIGN16_END WebRtcAec_weightCurve[65] = {
    0.0000f, 0.1000f, 0.1378f, 0.1535f, 0.1655f, 0.1756f, 0.1845f, 0.1926f,
    0.2000f, 0.2069f, 0.2134f, 0.2195f, 0.2254f, 0.2309f, 0.2363f, 0.2414f,
    0.2464f, 0.2512f, 0.2558f, 0.2604f, 0.2648f, 0.2690f, 0.2732f, 0.2773f,
    0.2813f, 0.2852f, 0.2890f, 0.2927f, 0.2964f, 0.3000f, 0.3035f, 0.3070f,
    0.3104f, 0.3138f, 0.3171f, 0.3204f, 0.3236f, 0.3268f, 0.3299f, 0.3330f,
    0.3360f, 0.3390f, 0.3420f, 0.3449f, 0.3478f, 0.3507f, 0.3535f, 0.3563f,
    0.3591f, 0.3619f, 0.3646f, 0.3673f, 0.3699f, 0.3726f, 0.3752f, 0.3777f,
    0.3803f, 0.3828f, 0.3854f, 0.3878f, 0.3903f, 0.3928f, 0.3952f, 0.3976f,
    0.4000f};

// Matlab code to produce table:
// overDriveCurve = [sqrt(linspace(0,1,65))' + 1];
// fprintf(1, '\t%.4f, %.4f, %.4f, %.4f, %.4f, %.4f,\n', overDriveCurve);
ALIGN16_BEG const float ALIGN16_END WebRtcAec_overDriveCurve[65] = {
    1.0000f, 1.1250f, 1.1768f, 1.2165f, 1.2500f, 1.2795f, 1.3062f, 1.3307f,
    1.3536f, 1.3750f, 1.3953f, 1.4146f, 1.4330f, 1.4507f, 1.4677f, 1.4841f,
    1.5000f, 1.5154f, 1.5303f, 1.5449f, 1.5590f, 1.5728f, 1.5863f, 1.5995f,
    1.6124f, 1.6250f, 1.6374f, 1.6495f, 1.6614f, 1.6731f, 1.6847f, 1.6960f,
    1.7071f, 1.7181f, 1.7289f, 1.7395f, 1.7500f, 1.7603f, 1.7706f, 1.7806f,
    1.7906f, 1.8004f, 1.8101f, 1.8197f, 1.8292f, 1.8385f, 1.8478f, 1.8570f,
    1.8660f, 1.8750f, 1.8839f, 1.8927f, 1.9014f, 1.9100f, 1.9186f, 1.9270f,
    1.9354f, 1.9437f, 1.9520f, 1.9601f, 1.9682f, 1.9763f, 1.9843f, 1.9922f,
    2.0000f};

// Delay Agnostic AEC parameters, still under development and may change.
static const float kDelayQualityThresholdMax = 0.07f;
static const float kDelayQualityThresholdMin = 0.01f;
static const int kInitialShiftOffset = 5;
#if !defined(WEBRTC_ANDROID)
static const int kDelayCorrectionStart = 1500;  // 10 ms chunks
#endif

// Target suppression levels for nlp modes.
// log{0.001, 0.00001, 0.00000001}
static const float kTargetSupp[3] = {-6.9f, -11.5f, -18.4f};

// Two sets of parameters, one for the extended filter mode.
static const float kExtendedMinOverDrive[3] = {3.0f, 6.0f, 15.0f};
static const float kNormalMinOverDrive[3] = {1.0f, 2.0f, 5.0f};
const float WebRtcAec_kExtendedSmoothingCoefficients[2][2] = {{0.9f, 0.1f},
                                                              {0.92f, 0.08f}};
const float WebRtcAec_kNormalSmoothingCoefficients[2][2] = {{0.9f, 0.1f},
                                                            {0.93f, 0.07f}};

// Number of partitions forming the NLP's "preferred" bands.
enum {
  kPrefBandSize = 24
};

#ifdef WEBRTC_AEC_DEBUG_DUMP
extern int webrtc_aec_instance_count;
#endif

WebRtcAecFilterFar WebRtcAec_FilterFar;
WebRtcAecScaleErrorSignal WebRtcAec_ScaleErrorSignal;
WebRtcAecFilterAdaptation WebRtcAec_FilterAdaptation;
WebRtcAecOverdriveAndSuppress WebRtcAec_OverdriveAndSuppress;
WebRtcAecComfortNoise WebRtcAec_ComfortNoise;
WebRtcAecSubBandCoherence WebRtcAec_SubbandCoherence;
WebRtcAecStoreAsComplex WebRtcAec_StoreAsComplex;
WebRtcAecPartitionDelay WebRtcAec_PartitionDelay;
WebRtcAecWindowData WebRtcAec_WindowData;

__inline static float MulRe(float aRe, float aIm, float bRe, float bIm) {
  return aRe * bRe - aIm * bIm;
}

__inline static float MulIm(float aRe, float aIm, float bRe, float bIm) {
  return aRe * bIm + aIm * bRe;
}

static int CmpFloat(const void* a, const void* b) {
  const float* da = (const float*)a;
  const float* db = (const float*)b;

  return (*da > *db) - (*da < *db);
}

static void FilterFar(
    int num_partitions,
    int x_fft_buf_block_pos,
    float x_fft_buf[2][kExtendedNumPartitions * PART_LEN1],
    float h_fft_buf[2][kExtendedNumPartitions * PART_LEN1],
    float y_fft[2][PART_LEN1]) {
  int i;
  for (i = 0; i < num_partitions; i++) {
    int j;
    int xPos = (i + x_fft_buf_block_pos) * PART_LEN1;
    int pos = i * PART_LEN1;
    // Check for wrap
    if (i + x_fft_buf_block_pos >= num_partitions) {
      xPos -= num_partitions * (PART_LEN1);
    }

    for (j = 0; j < PART_LEN1; j++) {
      y_fft[0][j] += MulRe(x_fft_buf[0][xPos + j],
                           x_fft_buf[1][xPos + j],
                           h_fft_buf[0][pos + j],
                           h_fft_buf[1][pos + j]);
      y_fft[1][j] += MulIm(x_fft_buf[0][xPos + j],
                           x_fft_buf[1][xPos + j],
                           h_fft_buf[0][pos + j],
                           h_fft_buf[1][pos + j]);
    }
  }
}

static void ScaleErrorSignal(int extended_filter_enabled,
                             float normal_mu,
                             float normal_error_threshold,
                             float x_pow[PART_LEN1],
                             float ef[2][PART_LEN1]) {
  const float mu = extended_filter_enabled ? kExtendedMu : normal_mu;
  const float error_threshold = extended_filter_enabled
                                    ? kExtendedErrorThreshold
                                    : normal_error_threshold;
  int i;
  float abs_ef;
  for (i = 0; i < (PART_LEN1); i++) {
    ef[0][i] /= (x_pow[i] + 1e-10f);
    ef[1][i] /= (x_pow[i] + 1e-10f);
    abs_ef = sqrtf(ef[0][i] * ef[0][i] + ef[1][i] * ef[1][i]);

    if (abs_ef > error_threshold) {
      abs_ef = error_threshold / (abs_ef + 1e-10f);
      ef[0][i] *= abs_ef;
      ef[1][i] *= abs_ef;
    }

    // Stepsize factor
    ef[0][i] *= mu;
    ef[1][i] *= mu;
  }
}


static void FilterAdaptation(
    int num_partitions,
    int x_fft_buf_block_pos,
    float x_fft_buf[2][kExtendedNumPartitions * PART_LEN1],
    float e_fft[2][PART_LEN1],
    float h_fft_buf[2][kExtendedNumPartitions * PART_LEN1]) {
  int i, j;
  float fft[PART_LEN2];
  for (i = 0; i < num_partitions; i++) {
    int xPos = (i + x_fft_buf_block_pos) * (PART_LEN1);
    int pos;
    // Check for wrap
    if (i + x_fft_buf_block_pos >= num_partitions) {
      xPos -= num_partitions * PART_LEN1;
    }

    pos = i * PART_LEN1;

    for (j = 0; j < PART_LEN; j++) {

      fft[2 * j] = MulRe(x_fft_buf[0][xPos + j],
                         -x_fft_buf[1][xPos + j],
                         e_fft[0][j],
                         e_fft[1][j]);
      fft[2 * j + 1] = MulIm(x_fft_buf[0][xPos + j],
                             -x_fft_buf[1][xPos + j],
                             e_fft[0][j],
                             e_fft[1][j]);
    }
    fft[1] = MulRe(x_fft_buf[0][xPos + PART_LEN],
                   -x_fft_buf[1][xPos + PART_LEN],
                   e_fft[0][PART_LEN],
                   e_fft[1][PART_LEN]);

    aec_rdft_inverse_128(fft);
    memset(fft + PART_LEN, 0, sizeof(float) * PART_LEN);

    // fft scaling
    {
      float scale = 2.0f / PART_LEN2;
      for (j = 0; j < PART_LEN; j++) {
        fft[j] *= scale;
      }
    }
    aec_rdft_forward_128(fft);

    h_fft_buf[0][pos] += fft[0];
    h_fft_buf[0][pos + PART_LEN] += fft[1];

    for (j = 1; j < PART_LEN; j++) {
      h_fft_buf[0][pos + j] += fft[2 * j];
      h_fft_buf[1][pos + j] += fft[2 * j + 1];
    }
  }
}

static void OverdriveAndSuppress(AecCore* aec,
                                 float hNl[PART_LEN1],
                                 const float hNlFb,
                                 float efw[2][PART_LEN1]) {
  int i;
  for (i = 0; i < PART_LEN1; i++) {
    // Weight subbands
    if (hNl[i] > hNlFb) {
      hNl[i] = WebRtcAec_weightCurve[i] * hNlFb +
               (1 - WebRtcAec_weightCurve[i]) * hNl[i];
    }
    hNl[i] = powf(hNl[i], aec->overDriveSm * WebRtcAec_overDriveCurve[i]);

    // Suppress error signal
    efw[0][i] *= hNl[i];
    efw[1][i] *= hNl[i];

    // Ooura fft returns incorrect sign on imaginary component. It matters here
    // because we are making an additive change with comfort noise.
    efw[1][i] *= -1;
  }
}

static int PartitionDelay(const AecCore* aec) {
  // Measures the energy in each filter partition and returns the partition with
  // highest energy.
  // TODO(bjornv): Spread computational cost by computing one partition per
  // block?
  float wfEnMax = 0;
  int i;
  int delay = 0;

  for (i = 0; i < aec->num_partitions; i++) {
    int j;
    int pos = i * PART_LEN1;
    float wfEn = 0;
    for (j = 0; j < PART_LEN1; j++) {
      wfEn += aec->wfBuf[0][pos + j] * aec->wfBuf[0][pos + j] +
          aec->wfBuf[1][pos + j] * aec->wfBuf[1][pos + j];
    }

    if (wfEn > wfEnMax) {
      wfEnMax = wfEn;
      delay = i;
    }
  }
  return delay;
}

// Threshold to protect against the ill-effects of a zero far-end.
const float WebRtcAec_kMinFarendPSD = 15;

// Updates the following smoothed  Power Spectral Densities (PSD):
//  - sd  : near-end
//  - se  : residual echo
//  - sx  : far-end
//  - sde : cross-PSD of near-end and residual echo
//  - sxd : cross-PSD of near-end and far-end
//
// In addition to updating the PSDs, also the filter diverge state is
// determined.
static void SmoothedPSD(AecCore* aec,
                        float efw[2][PART_LEN1],
                        float dfw[2][PART_LEN1],
                        float xfw[2][PART_LEN1],
                        int* extreme_filter_divergence) {
  // Power estimate smoothing coefficients.
  const float* ptrGCoh = aec->extended_filter_enabled
      ? WebRtcAec_kExtendedSmoothingCoefficients[aec->mult - 1]
      : WebRtcAec_kNormalSmoothingCoefficients[aec->mult - 1];
  int i;
  float sdSum = 0, seSum = 0;

  for (i = 0; i < PART_LEN1; i++) {
    aec->sd[i] = ptrGCoh[0] * aec->sd[i] +
                 ptrGCoh[1] * (dfw[0][i] * dfw[0][i] + dfw[1][i] * dfw[1][i]);
    aec->se[i] = ptrGCoh[0] * aec->se[i] +
                 ptrGCoh[1] * (efw[0][i] * efw[0][i] + efw[1][i] * efw[1][i]);
    // We threshold here to protect against the ill-effects of a zero farend.
    // The threshold is not arbitrarily chosen, but balances protection and
    // adverse interaction with the algorithm's tuning.
    // TODO(bjornv): investigate further why this is so sensitive.
    aec->sx[i] =
        ptrGCoh[0] * aec->sx[i] +
        ptrGCoh[1] * WEBRTC_SPL_MAX(
            xfw[0][i] * xfw[0][i] + xfw[1][i] * xfw[1][i],
            WebRtcAec_kMinFarendPSD);

    aec->sde[i][0] =
        ptrGCoh[0] * aec->sde[i][0] +
        ptrGCoh[1] * (dfw[0][i] * efw[0][i] + dfw[1][i] * efw[1][i]);
    aec->sde[i][1] =
        ptrGCoh[0] * aec->sde[i][1] +
        ptrGCoh[1] * (dfw[0][i] * efw[1][i] - dfw[1][i] * efw[0][i]);

    aec->sxd[i][0] =
        ptrGCoh[0] * aec->sxd[i][0] +
        ptrGCoh[1] * (dfw[0][i] * xfw[0][i] + dfw[1][i] * xfw[1][i]);
    aec->sxd[i][1] =
        ptrGCoh[0] * aec->sxd[i][1] +
        ptrGCoh[1] * (dfw[0][i] * xfw[1][i] - dfw[1][i] * xfw[0][i]);

    sdSum += aec->sd[i];
    seSum += aec->se[i];
  }

  // Divergent filter safeguard update.
  aec->divergeState = (aec->divergeState ? 1.05f : 1.0f) * seSum > sdSum;

  // Signal extreme filter divergence if the error is significantly larger
  // than the nearend (13 dB).
  *extreme_filter_divergence = (seSum > (19.95f * sdSum));
}

// Window time domain data to be used by the fft.
__inline static void WindowData(float* x_windowed, const float* x) {
  int i;
  for (i = 0; i < PART_LEN; i++) {
    x_windowed[i] = x[i] * WebRtcAec_sqrtHanning[i];
    x_windowed[PART_LEN + i] =
        x[PART_LEN + i] * WebRtcAec_sqrtHanning[PART_LEN - i];
  }
}

// Puts fft output data into a complex valued array.
__inline static void StoreAsComplex(const float* data,
                                    float data_complex[2][PART_LEN1]) {
  int i;
  data_complex[0][0] = data[0];
  data_complex[1][0] = 0;
  for (i = 1; i < PART_LEN; i++) {
    data_complex[0][i] = data[2 * i];
    data_complex[1][i] = data[2 * i + 1];
  }
  data_complex[0][PART_LEN] = data[1];
  data_complex[1][PART_LEN] = 0;
}

static void SubbandCoherence(AecCore* aec,
                             float efw[2][PART_LEN1],
                             float dfw[2][PART_LEN1],
                             float xfw[2][PART_LEN1],
                             float* fft,
                             float* cohde,
                             float* cohxd,
                             int* extreme_filter_divergence) {
  int i;

  SmoothedPSD(aec, efw, dfw, xfw, extreme_filter_divergence);

  // Subband coherence
  for (i = 0; i < PART_LEN1; i++) {
    cohde[i] =
        (aec->sde[i][0] * aec->sde[i][0] + aec->sde[i][1] * aec->sde[i][1]) /
        (aec->sd[i] * aec->se[i] + 1e-10f);
    cohxd[i] =
        (aec->sxd[i][0] * aec->sxd[i][0] + aec->sxd[i][1] * aec->sxd[i][1]) /
        (aec->sx[i] * aec->sd[i] + 1e-10f);
  }
}

static void GetHighbandGain(const float* lambda, float* nlpGainHband) {
  int i;

  *nlpGainHband = (float)0.0;
  for (i = freqAvgIc; i < PART_LEN1 - 1; i++) {
    *nlpGainHband += lambda[i];
  }
  *nlpGainHband /= (float)(PART_LEN1 - 1 - freqAvgIc);
}

static void ComfortNoise(AecCore* aec,
                         float efw[2][PART_LEN1],
                         float comfortNoiseHband[2][PART_LEN1],
                         const float* noisePow,
                         const float* lambda) {
  int i, num;
  float rand[PART_LEN];
  float noise, noiseAvg, tmp, tmpAvg;
  int16_t randW16[PART_LEN];
  float u[2][PART_LEN1];

  const float pi2 = 6.28318530717959f;

  // Generate a uniform random array on [0 1]
  WebRtcSpl_RandUArray(randW16, PART_LEN, &aec->seed);
  for (i = 0; i < PART_LEN; i++) {
    rand[i] = ((float)randW16[i]) / 32768;
  }

  // Reject LF noise
  u[0][0] = 0;
  u[1][0] = 0;
  for (i = 1; i < PART_LEN1; i++) {
    tmp = pi2 * rand[i - 1];

    noise = sqrtf(noisePow[i]);
    u[0][i] = noise * cosf(tmp);
    u[1][i] = -noise * sinf(tmp);
  }
  u[1][PART_LEN] = 0;

  for (i = 0; i < PART_LEN1; i++) {
    // This is the proper weighting to match the background noise power
    tmp = sqrtf(WEBRTC_SPL_MAX(1 - lambda[i] * lambda[i], 0));
    // tmp = 1 - lambda[i];
    efw[0][i] += tmp * u[0][i];
    efw[1][i] += tmp * u[1][i];
  }

  // For H band comfort noise
  // TODO: don't compute noise and "tmp" twice. Use the previous results.
  noiseAvg = 0.0;
  tmpAvg = 0.0;
  num = 0;
  if (aec->num_bands > 1) {

    // average noise scale
    // average over second half of freq spectrum (i.e., 4->8khz)
    // TODO: we shouldn't need num. We know how many elements we're summing.
    for (i = PART_LEN1 >> 1; i < PART_LEN1; i++) {
      num++;
      noiseAvg += sqrtf(noisePow[i]);
    }
    noiseAvg /= (float)num;

    // average nlp scale
    // average over second half of freq spectrum (i.e., 4->8khz)
    // TODO: we shouldn't need num. We know how many elements we're summing.
    num = 0;
    for (i = PART_LEN1 >> 1; i < PART_LEN1; i++) {
      num++;
      tmpAvg += sqrtf(WEBRTC_SPL_MAX(1 - lambda[i] * lambda[i], 0));
    }
    tmpAvg /= (float)num;

    // Use average noise for H band
    // TODO: we should probably have a new random vector here.
    // Reject LF noise
    u[0][0] = 0;
    u[1][0] = 0;
    for (i = 1; i < PART_LEN1; i++) {
      tmp = pi2 * rand[i - 1];

      // Use average noise for H band
      u[0][i] = noiseAvg * (float)cos(tmp);
      u[1][i] = -noiseAvg * (float)sin(tmp);
    }
    u[1][PART_LEN] = 0;

    for (i = 0; i < PART_LEN1; i++) {
      // Use average NLP weight for H band
      comfortNoiseHband[0][i] = tmpAvg * u[0][i];
      comfortNoiseHband[1][i] = tmpAvg * u[1][i];
    }
  } else {
    memset(comfortNoiseHband, 0,
           2 * PART_LEN1 * sizeof(comfortNoiseHband[0][0]));
  }
}

static void InitLevel(PowerLevel* level) {
  const float kBigFloat = 1E17f;

  level->averagelevel = 0;
  level->framelevel = 0;
  level->minlevel = kBigFloat;
  level->frsum = 0;
  level->sfrsum = 0;
  level->frcounter = 0;
  level->sfrcounter = 0;
}

static void InitStats(Stats* stats) {
  stats->instant = kOffsetLevel;
  stats->average = kOffsetLevel;
  stats->max = kOffsetLevel;
  stats->min = kOffsetLevel * (-1);
  stats->sum = 0;
  stats->hisum = 0;
  stats->himean = kOffsetLevel;
  stats->counter = 0;
  stats->hicounter = 0;
}

static void InitMetrics(AecCore* self) {
  self->stateCounter = 0;
  InitLevel(&self->farlevel);
  InitLevel(&self->nearlevel);
  InitLevel(&self->linoutlevel);
  InitLevel(&self->nlpoutlevel);

  InitStats(&self->erl);
  InitStats(&self->erle);
  InitStats(&self->aNlp);
  InitStats(&self->rerl);
}

static void UpdateLevel(PowerLevel* level, float in[2][PART_LEN1]) {
  // Do the energy calculation in the frequency domain. The FFT is performed on
  // a segment of PART_LEN2 samples due to overlap, but we only want the energy
  // of half that data (the last PART_LEN samples). Parseval's relation states
  // that the energy is preserved according to
  //
  // \sum_{n=0}^{N-1} |x(n)|^2 = 1/N * \sum_{n=0}^{N-1} |X(n)|^2
  //                           = ENERGY,
  //
  // where N = PART_LEN2. Since we are only interested in calculating the energy
  // for the last PART_LEN samples we approximate by calculating ENERGY and
  // divide by 2,
  //
  // \sum_{n=N/2}^{N-1} |x(n)|^2 ~= ENERGY / 2
  //
  // Since we deal with real valued time domain signals we only store frequency
  // bins [0, PART_LEN], which is what |in| consists of. To calculate ENERGY we
  // need to add the contribution from the missing part in
  // [PART_LEN+1, PART_LEN2-1]. These values are, up to a phase shift, identical
  // with the values in [1, PART_LEN-1], hence multiply those values by 2. This
  // is the values in the for loop below, but multiplication by 2 and division
  // by 2 cancel.

  // TODO(bjornv): Investigate reusing energy calculations performed at other
  // places in the code.
  int k = 1;
  // Imaginary parts are zero at end points and left out of the calculation.
  float energy = (in[0][0] * in[0][0]) / 2;
  energy += (in[0][PART_LEN] * in[0][PART_LEN]) / 2;

  for (k = 1; k < PART_LEN; k++) {
    energy += (in[0][k] * in[0][k] + in[1][k] * in[1][k]);
  }
  energy /= PART_LEN2;

  level->sfrsum += energy;
  level->sfrcounter++;

  if (level->sfrcounter > subCountLen) {
    level->framelevel = level->sfrsum / (subCountLen * PART_LEN);
    level->sfrsum = 0;
    level->sfrcounter = 0;
    if (level->framelevel > 0) {
      if (level->framelevel < level->minlevel) {
        level->minlevel = level->framelevel;  // New minimum.
      } else {
        level->minlevel *= (1 + 0.001f);  // Small increase.
      }
    }
    level->frcounter++;
    level->frsum += level->framelevel;
    if (level->frcounter > countLen) {
      level->averagelevel = level->frsum / countLen;
      level->frsum = 0;
      level->frcounter = 0;
    }
  }
}

static void UpdateMetrics(AecCore* aec) {
  float dtmp, dtmp2;

  const float actThresholdNoisy = 8.0f;
  const float actThresholdClean = 40.0f;
  const float safety = 0.99995f;
  const float noisyPower = 300000.0f;

  float actThreshold;
  float echo, suppressedEcho;

  if (aec->echoState) {  // Check if echo is likely present
    aec->stateCounter++;
  }

  if (aec->farlevel.frcounter == 0) {

    if (aec->farlevel.minlevel < noisyPower) {
      actThreshold = actThresholdClean;
    } else {
      actThreshold = actThresholdNoisy;
    }

    if ((aec->stateCounter > (0.5f * countLen * subCountLen)) &&
        (aec->farlevel.sfrcounter == 0)

        // Estimate in active far-end segments only
        &&
        (aec->farlevel.averagelevel >
         (actThreshold * aec->farlevel.minlevel))) {

      // Subtract noise power
      echo = aec->nearlevel.averagelevel - safety * aec->nearlevel.minlevel;

      // ERL
      dtmp = 10 * (float)log10(aec->farlevel.averagelevel /
                                   aec->nearlevel.averagelevel +
                               1e-10f);
      dtmp2 = 10 * (float)log10(aec->farlevel.averagelevel / echo + 1e-10f);

      aec->erl.instant = dtmp;
      if (dtmp > aec->erl.max) {
        aec->erl.max = dtmp;
      }

      if (dtmp < aec->erl.min) {
        aec->erl.min = dtmp;
      }

      aec->erl.counter++;
      aec->erl.sum += dtmp;
      aec->erl.average = aec->erl.sum / aec->erl.counter;

      // Upper mean
      if (dtmp > aec->erl.average) {
        aec->erl.hicounter++;
        aec->erl.hisum += dtmp;
        aec->erl.himean = aec->erl.hisum / aec->erl.hicounter;
      }

      // A_NLP
      dtmp = 10 * (float)log10(aec->nearlevel.averagelevel /
                                   (2 * aec->linoutlevel.averagelevel) +
                               1e-10f);

      // subtract noise power
      suppressedEcho = 2 * (aec->linoutlevel.averagelevel -
                            safety * aec->linoutlevel.minlevel);

      dtmp2 = 10 * (float)log10(echo / suppressedEcho + 1e-10f);

      aec->aNlp.instant = dtmp2;
      if (dtmp > aec->aNlp.max) {
        aec->aNlp.max = dtmp;
      }

      if (dtmp < aec->aNlp.min) {
        aec->aNlp.min = dtmp;
      }

      aec->aNlp.counter++;
      aec->aNlp.sum += dtmp;
      aec->aNlp.average = aec->aNlp.sum / aec->aNlp.counter;

      // Upper mean
      if (dtmp > aec->aNlp.average) {
        aec->aNlp.hicounter++;
        aec->aNlp.hisum += dtmp;
        aec->aNlp.himean = aec->aNlp.hisum / aec->aNlp.hicounter;
      }

      // ERLE

      // subtract noise power
      suppressedEcho = 2 * (aec->nlpoutlevel.averagelevel -
                            safety * aec->nlpoutlevel.minlevel);

      dtmp = 10 * (float)log10(aec->nearlevel.averagelevel /
                                   (2 * aec->nlpoutlevel.averagelevel) +
                               1e-10f);
      dtmp2 = 10 * (float)log10(echo / suppressedEcho + 1e-10f);

      dtmp = dtmp2;
      aec->erle.instant = dtmp;
      if (dtmp > aec->erle.max) {
        aec->erle.max = dtmp;
      }

      if (dtmp < aec->erle.min) {
        aec->erle.min = dtmp;
      }

      aec->erle.counter++;
      aec->erle.sum += dtmp;
      aec->erle.average = aec->erle.sum / aec->erle.counter;

      // Upper mean
      if (dtmp > aec->erle.average) {
        aec->erle.hicounter++;
        aec->erle.hisum += dtmp;
        aec->erle.himean = aec->erle.hisum / aec->erle.hicounter;
      }
    }

    aec->stateCounter = 0;
  }
}

static void UpdateDelayMetrics(AecCore* self) {
  int i = 0;
  int delay_values = 0;
  int median = 0;
  int lookahead = WebRtc_lookahead(self->delay_estimator);
  const int kMsPerBlock = PART_LEN / (self->mult * 8);
  int64_t l1_norm = 0;

  if (self->num_delay_values == 0) {
    // We have no new delay value data. Even though -1 is a valid |median| in
    // the sense that we allow negative values, it will practically never be
    // used since multiples of |kMsPerBlock| will always be returned.
    // We therefore use -1 to indicate in the logs that the delay estimator was
    // not able to estimate the delay.
    self->delay_median = -1;
    self->delay_std = -1;
    self->fraction_poor_delays = -1;
    return;
  }

  // Start value for median count down.
  delay_values = self->num_delay_values >> 1;
  // Get median of delay values since last update.
  for (i = 0; i < kHistorySizeBlocks; i++) {
    delay_values -= self->delay_histogram[i];
    if (delay_values < 0) {
      median = i;
      break;
    }
  }
  // Account for lookahead.
  self->delay_median = (median - lookahead) * kMsPerBlock;

  // Calculate the L1 norm, with median value as central moment.
  for (i = 0; i < kHistorySizeBlocks; i++) {
    l1_norm += abs(i - median) * self->delay_histogram[i];
  }
  self->delay_std = (int)((l1_norm + self->num_delay_values / 2) /
      self->num_delay_values) * kMsPerBlock;

  // Determine fraction of delays that are out of bounds, that is, either
  // negative (anti-causal system) or larger than the AEC filter length.
  {
    int num_delays_out_of_bounds = self->num_delay_values;
    const int histogram_length = sizeof(self->delay_histogram) /
      sizeof(self->delay_histogram[0]);
    for (i = lookahead; i < lookahead + self->num_partitions; ++i) {
      if (i < histogram_length)
        num_delays_out_of_bounds -= self->delay_histogram[i];
    }
    self->fraction_poor_delays = (float)num_delays_out_of_bounds /
        self->num_delay_values;
  }

  // Reset histogram.
  memset(self->delay_histogram, 0, sizeof(self->delay_histogram));
  self->num_delay_values = 0;

  return;
}

static void ScaledInverseFft(float freq_data[2][PART_LEN1],
                             float time_data[PART_LEN2],
                             float scale,
                             int conjugate) {
  int i;
  const float normalization = scale / ((float)PART_LEN2);
  const float sign = (conjugate ? -1 : 1);
  time_data[0] = freq_data[0][0] * normalization;
  time_data[1] = freq_data[0][PART_LEN] * normalization;
  for (i = 1; i < PART_LEN; i++) {
    time_data[2 * i] = freq_data[0][i] * normalization;
    time_data[2 * i + 1] = sign * freq_data[1][i] * normalization;
  }
  aec_rdft_inverse_128(time_data);
}


static void Fft(float time_data[PART_LEN2],
                float freq_data[2][PART_LEN1]) {
  int i;
  aec_rdft_forward_128(time_data);

  // Reorder fft output data.
  freq_data[1][0] = 0;
  freq_data[1][PART_LEN] = 0;
  freq_data[0][0] = time_data[0];
  freq_data[0][PART_LEN] = time_data[1];
  for (i = 1; i < PART_LEN; i++) {
    freq_data[0][i] = time_data[2 * i];
    freq_data[1][i] = time_data[2 * i + 1];
  }
}


static int SignalBasedDelayCorrection(AecCore* self) {
  int delay_correction = 0;
  int last_delay = -2;
  assert(self != NULL);
#if !defined(WEBRTC_ANDROID)
  // On desktops, turn on correction after |kDelayCorrectionStart| frames.  This
  // is to let the delay estimation get a chance to converge.  Also, if the
  // playout audio volume is low (or even muted) the delay estimation can return
  // a very large delay, which will break the AEC if it is applied.
  if (self->frame_count < kDelayCorrectionStart) {
    return 0;
  }
#endif

  // 1. Check for non-negative delay estimate.  Note that the estimates we get
  //    from the delay estimation are not compensated for lookahead.  Hence, a
  //    negative |last_delay| is an invalid one.
  // 2. Verify that there is a delay change.  In addition, only allow a change
  //    if the delay is outside a certain region taking the AEC filter length
  //    into account.
  // TODO(bjornv): Investigate if we can remove the non-zero delay change check.
  // 3. Only allow delay correction if the delay estimation quality exceeds
  //    |delay_quality_threshold|.
  // 4. Finally, verify that the proposed |delay_correction| is feasible by
  //    comparing with the size of the far-end buffer.
  last_delay = WebRtc_last_delay(self->delay_estimator);
  if ((last_delay >= 0) &&
      (last_delay != self->previous_delay) &&
      (WebRtc_last_delay_quality(self->delay_estimator) >
           self->delay_quality_threshold)) {
    int delay = last_delay - WebRtc_lookahead(self->delay_estimator);
    // Allow for a slack in the actual delay, defined by a |lower_bound| and an
    // |upper_bound|.  The adaptive echo cancellation filter is currently
    // |num_partitions| (of 64 samples) long.  If the delay estimate is negative
    // or at least 3/4 of the filter length we open up for correction.
    const int lower_bound = 0;
    const int upper_bound = self->num_partitions * 3 / 4;
    const int do_correction = delay <= lower_bound || delay > upper_bound;
    if (do_correction == 1) {
      int available_read = (int)WebRtc_available_read(self->far_time_buf);
      // With |shift_offset| we gradually rely on the delay estimates.  For
      // positive delays we reduce the correction by |shift_offset| to lower the
      // risk of pushing the AEC into a non causal state.  For negative delays
      // we rely on the values up to a rounding error, hence compensate by 1
      // element to make sure to push the delay into the causal region.
      delay_correction = -delay;
      delay_correction += delay > self->shift_offset ? self->shift_offset : 1;
      self->shift_offset--;
      self->shift_offset = (self->shift_offset <= 1 ? 1 : self->shift_offset);
      if (delay_correction > available_read - self->mult - 1) {
        // There is not enough data in the buffer to perform this shift.  Hence,
        // we do not rely on the delay estimate and do nothing.
        delay_correction = 0;
      } else {
        self->previous_delay = last_delay;
        ++self->delay_correction_count;
      }
    }
  }
  // Update the |delay_quality_threshold| once we have our first delay
  // correction.
  if (self->delay_correction_count > 0) {
    float delay_quality = WebRtc_last_delay_quality(self->delay_estimator);
    delay_quality = (delay_quality > kDelayQualityThresholdMax ?
        kDelayQualityThresholdMax : delay_quality);
    self->delay_quality_threshold =
        (delay_quality > self->delay_quality_threshold ? delay_quality :
            self->delay_quality_threshold);
  }
  return delay_correction;
}

#ifdef WEBRTC_AEC_DEBUG_DUMP
static void
OpenCoreDebugFiles(AecCore* aec, int *aec_instance_count)
{
  if (AECDebug())
  {
    if (!aec->farFile)
    {
      int process_rate = aec->sampFreq > 16000 ? 16000 : aec->sampFreq;
      RTC_AEC_DEBUG_WAV_REOPEN("aec_far", aec->instance_index, aec->debug_dump_count,
                               process_rate, &aec->farFile);
      RTC_AEC_DEBUG_WAV_REOPEN("aec_near", aec->instance_index, aec->debug_dump_count,
                               process_rate, &aec->nearFile);
      RTC_AEC_DEBUG_WAV_REOPEN("aec_out", aec->instance_index, aec->debug_dump_count,
                               process_rate, &aec->outFile);
      RTC_AEC_DEBUG_WAV_REOPEN("aec_out_linear", aec->instance_index, aec->debug_dump_count,
                               process_rate, &aec->outLinearFile);
      RTC_AEC_DEBUG_RAW_CLOSE(aec->e_fft_file);
      RTC_AEC_DEBUG_RAW_OPEN("aec_fft", aec->instance_index, aec->debug_dump_count,
                             &aec->e_fft_file);
      ++aec->debug_dump_count;
    }
  } else {
    if (aec->farFile) {
      RTC_AEC_DEBUG_WAV_CLOSE(aec->farFile);
    }
    if (aec->nearFile) {
      RTC_AEC_DEBUG_WAV_CLOSE(aec->nearFile);
    }
    if (aec->outFile) {
      RTC_AEC_DEBUG_WAV_CLOSE(aec->outFile);
    }
    if (aec->outLinearFile) {
      RTC_AEC_DEBUG_WAV_CLOSE(aec->outLinearFile);
    }
    if (aec->e_fft_file) {
      RTC_AEC_DEBUG_RAW_CLOSE(aec->e_fft_file);
    }
    aec->outLinearFile = aec->outFile = aec->nearFile = aec->farFile = NULL;
    aec->e_fft_file = NULL;
    aec->debugWritten = 0;
  }
}
#endif

static void EchoSubtraction(
    AecCore* aec,
    int num_partitions,
    int x_fft_buf_block_pos,
    int metrics_mode,
    int extended_filter_enabled,
    float normal_mu,
    float normal_error_threshold,
    float x_fft_buf[2][kExtendedNumPartitions * PART_LEN1],
    float* const y,
    float x_pow[PART_LEN1],
    float h_fft_buf[2][kExtendedNumPartitions * PART_LEN1],
    PowerLevel* linout_level,
    float echo_subtractor_output[PART_LEN]) {
  float s_fft[2][PART_LEN1];
  float e_extended[PART_LEN2];
  float s_extended[PART_LEN2];
  float *s;
  float e[PART_LEN];
  float e_fft[2][PART_LEN1];
  int i;
  memset(s_fft, 0, sizeof(s_fft));

  // Conditionally reset the echo subtraction filter if the filter has diverged
  // significantly.
  if (!aec->extended_filter_enabled &&
      aec->extreme_filter_divergence) {
    memset(aec->wfBuf, 0, sizeof(aec->wfBuf));
    aec->extreme_filter_divergence = 0;
  }

  // Produce echo estimate s_fft.
  WebRtcAec_FilterFar(num_partitions,
                      x_fft_buf_block_pos,
                      x_fft_buf,
                      h_fft_buf,
                      s_fft);

  // Compute the time-domain echo estimate s.
  ScaledInverseFft(s_fft, s_extended, 2.0f, 0);
  s = &s_extended[PART_LEN];

  // Compute the time-domain echo prediction error.
  for (i = 0; i < PART_LEN; ++i) {
    e[i] = y[i] - s[i];
  }

  // Compute the frequency domain echo prediction error.
  memset(e_extended, 0, sizeof(float) * PART_LEN);
  memcpy(e_extended + PART_LEN, e, sizeof(float) * PART_LEN);
  Fft(e_extended, e_fft);

#ifdef WEBRTC_AEC_DEBUG_DUMP
  if (aec->e_fft_file) {
    RTC_AEC_DEBUG_RAW_WRITE(aec->e_fft_file,
                            &e_fft[0][0],
                            sizeof(e_fft[0][0]) * PART_LEN1 * 2);
  }
#endif

  if (metrics_mode == 1) {
    // Note that the first PART_LEN samples in fft (before transformation) are
    // zero. Hence, the scaling by two in UpdateLevel() should not be
    // performed. That scaling is taken care of in UpdateMetrics() instead.
    UpdateLevel(linout_level, e_fft);
  }

  // Scale error signal inversely with far power.
  WebRtcAec_ScaleErrorSignal(extended_filter_enabled,
                             normal_mu,
                             normal_error_threshold,
                             x_pow,
                             e_fft);
  WebRtcAec_FilterAdaptation(num_partitions,
                             x_fft_buf_block_pos,
                             x_fft_buf,
                             e_fft,
                             h_fft_buf);
  memcpy(echo_subtractor_output, e, sizeof(float) * PART_LEN);
}


static void EchoSuppression(AecCore* aec,
                            float farend[PART_LEN2],
                            float* echo_subtractor_output,
                            float* output,
                            float* const* outputH) {
  float efw[2][PART_LEN1];
  float xfw[2][PART_LEN1];
  float dfw[2][PART_LEN1];
  float comfortNoiseHband[2][PART_LEN1];
  float fft[PART_LEN2];
  float nlpGainHband;
  int i;
  size_t j;

  // Coherence and non-linear filter
  float cohde[PART_LEN1], cohxd[PART_LEN1];
  float hNlDeAvg, hNlXdAvg;
  float hNl[PART_LEN1];
  float hNlPref[kPrefBandSize];
  float hNlFb = 0, hNlFbLow = 0;
  const float prefBandQuant = 0.75f, prefBandQuantLow = 0.5f;
  const int prefBandSize = kPrefBandSize / aec->mult;
  const int minPrefBand = 4 / aec->mult;
  // Power estimate smoothing coefficients.
  const float* min_overdrive = aec->extended_filter_enabled
                                   ? kExtendedMinOverDrive
                                   : kNormalMinOverDrive;

  // Filter energy
  const int delayEstInterval = 10 * aec->mult;

  float* xfw_ptr = NULL;

  // Update eBuf with echo subtractor output.
  memcpy(aec->eBuf + PART_LEN,
         echo_subtractor_output,
         sizeof(float) * PART_LEN);

  // Analysis filter banks for the echo suppressor.
  // Windowed near-end ffts.
  WindowData(fft, aec->dBuf);
  aec_rdft_forward_128(fft);
  StoreAsComplex(fft, dfw);

  // Windowed echo suppressor output ffts.
  WindowData(fft, aec->eBuf);
  aec_rdft_forward_128(fft);
  StoreAsComplex(fft, efw);

  // NLP

  // Convert far-end partition to the frequency domain with windowing.
  WindowData(fft, farend);
  Fft(fft, xfw);
  xfw_ptr = &xfw[0][0];

  // Buffer far.
  memcpy(aec->xfwBuf, xfw_ptr, sizeof(float) * 2 * PART_LEN1);

  aec->delayEstCtr++;
  if (aec->delayEstCtr == delayEstInterval) {
    aec->delayEstCtr = 0;
    aec->delayIdx = WebRtcAec_PartitionDelay(aec);
  }

  // Use delayed far.
  memcpy(xfw,
         aec->xfwBuf + aec->delayIdx * PART_LEN1,
         sizeof(xfw[0][0]) * 2 * PART_LEN1);

  WebRtcAec_SubbandCoherence(aec, efw, dfw, xfw, fft, cohde, cohxd,
                             &aec->extreme_filter_divergence);

  // Select the microphone signal as output if the filter is deemed to have
  // diverged.
  if (aec->divergeState) {
    memcpy(efw, dfw, sizeof(efw[0][0]) * 2 * PART_LEN1);
  }

  hNlXdAvg = 0;
  for (i = minPrefBand; i < prefBandSize + minPrefBand; i++) {
    hNlXdAvg += cohxd[i];
  }
  hNlXdAvg /= prefBandSize;
  hNlXdAvg = 1 - hNlXdAvg;

  hNlDeAvg = 0;
  for (i = minPrefBand; i < prefBandSize + minPrefBand; i++) {
    hNlDeAvg += cohde[i];
  }
  hNlDeAvg /= prefBandSize;

  if (hNlXdAvg < 0.75f && hNlXdAvg < aec->hNlXdAvgMin) {
    aec->hNlXdAvgMin = hNlXdAvg;
  }

  if (hNlDeAvg > 0.98f && hNlXdAvg > 0.9f) {
    aec->stNearState = 1;
  } else if (hNlDeAvg < 0.95f || hNlXdAvg < 0.8f) {
    aec->stNearState = 0;
  }

  if (aec->hNlXdAvgMin == 1) {
    aec->echoState = 0;
    aec->overDrive = min_overdrive[aec->nlp_mode];

    if (aec->stNearState == 1) {
      memcpy(hNl, cohde, sizeof(hNl));
      hNlFb = hNlDeAvg;
      hNlFbLow = hNlDeAvg;
    } else {
      for (i = 0; i < PART_LEN1; i++) {
        hNl[i] = 1 - cohxd[i];
      }
      hNlFb = hNlXdAvg;
      hNlFbLow = hNlXdAvg;
    }
  } else {

    if (aec->stNearState == 1) {
      aec->echoState = 0;
      memcpy(hNl, cohde, sizeof(hNl));
      hNlFb = hNlDeAvg;
      hNlFbLow = hNlDeAvg;
    } else {
      aec->echoState = 1;
      for (i = 0; i < PART_LEN1; i++) {
        hNl[i] = WEBRTC_SPL_MIN(cohde[i], 1 - cohxd[i]);
      }

      // Select an order statistic from the preferred bands.
      // TODO: Using quicksort now, but a selection algorithm may be preferred.
      memcpy(hNlPref, &hNl[minPrefBand], sizeof(float) * prefBandSize);
      qsort(hNlPref, prefBandSize, sizeof(float), CmpFloat);
      hNlFb = hNlPref[(int)floor(prefBandQuant * (prefBandSize - 1))];
      hNlFbLow = hNlPref[(int)floor(prefBandQuantLow * (prefBandSize - 1))];
    }
  }

  // Track the local filter minimum to determine suppression overdrive.
  if (hNlFbLow < 0.6f && hNlFbLow < aec->hNlFbLocalMin) {
    aec->hNlFbLocalMin = hNlFbLow;
    aec->hNlFbMin = hNlFbLow;
    aec->hNlNewMin = 1;
    aec->hNlMinCtr = 0;
  }
  aec->hNlFbLocalMin =
      WEBRTC_SPL_MIN(aec->hNlFbLocalMin + 0.0008f / aec->mult, 1);
  aec->hNlXdAvgMin = WEBRTC_SPL_MIN(aec->hNlXdAvgMin + 0.0006f / aec->mult, 1);

  if (aec->hNlNewMin == 1) {
    aec->hNlMinCtr++;
  }
  if (aec->hNlMinCtr == 2) {
    aec->hNlNewMin = 0;
    aec->hNlMinCtr = 0;
    aec->overDrive =
        WEBRTC_SPL_MAX(kTargetSupp[aec->nlp_mode] /
                           ((float)log(aec->hNlFbMin + 1e-10f) + 1e-10f),
                       min_overdrive[aec->nlp_mode]);
  }

  // Smooth the overdrive.
  if (aec->overDrive < aec->overDriveSm) {
    aec->overDriveSm = 0.99f * aec->overDriveSm + 0.01f * aec->overDrive;
  } else {
    aec->overDriveSm = 0.9f * aec->overDriveSm + 0.1f * aec->overDrive;
  }

  WebRtcAec_OverdriveAndSuppress(aec, hNl, hNlFb, efw);

  // Add comfort noise.
  WebRtcAec_ComfortNoise(aec, efw, comfortNoiseHband, aec->noisePow, hNl);

  // TODO(bjornv): Investigate how to take the windowing below into account if
  // needed.
  if (aec->metricsMode == 1) {
    // Note that we have a scaling by two in the time domain |eBuf|.
    // In addition the time domain signal is windowed before transformation,
    // losing half the energy on the average. We take care of the first
    // scaling only in UpdateMetrics().
    UpdateLevel(&aec->nlpoutlevel, efw);
  }

  // Inverse error fft.
  ScaledInverseFft(efw, fft, 2.0f, 1);

  // Overlap and add to obtain output.
  for (i = 0; i < PART_LEN; i++) {
    output[i] = (fft[i] * WebRtcAec_sqrtHanning[i] +
                 aec->outBuf[i] * WebRtcAec_sqrtHanning[PART_LEN - i]);

    // Saturate output to keep it in the allowed range.
    output[i] = WEBRTC_SPL_SAT(
        WEBRTC_SPL_WORD16_MAX, output[i], WEBRTC_SPL_WORD16_MIN);
  }
  memcpy(aec->outBuf, &fft[PART_LEN], PART_LEN * sizeof(aec->outBuf[0]));

  // For H band
  if (aec->num_bands > 1) {
    // H band gain
    // average nlp over low band: average over second half of freq spectrum
    // (4->8khz)
    GetHighbandGain(hNl, &nlpGainHband);

    // Inverse comfort_noise
    ScaledInverseFft(comfortNoiseHband, fft, 2.0f, 0);

    // compute gain factor
    for (j = 0; j < aec->num_bands - 1; ++j) {
      for (i = 0; i < PART_LEN; i++) {
        outputH[j][i] = aec->dBufH[j][i] * nlpGainHband;
      }
    }

    // Add some comfort noise where Hband is attenuated.
    for (i = 0; i < PART_LEN; i++) {
      outputH[0][i] += cnScaleHband * fft[i];
    }

    // Saturate output to keep it in the allowed range.
    for (j = 0; j < aec->num_bands - 1; ++j) {
      for (i = 0; i < PART_LEN; i++) {
        outputH[j][i] = WEBRTC_SPL_SAT(
            WEBRTC_SPL_WORD16_MAX, outputH[j][i], WEBRTC_SPL_WORD16_MIN);
      }
    }

  }

  // Copy the current block to the old position.
  memcpy(aec->dBuf, aec->dBuf + PART_LEN, sizeof(float) * PART_LEN);
  memcpy(aec->eBuf, aec->eBuf + PART_LEN, sizeof(float) * PART_LEN);

  // Copy the current block to the old position for H band
  for (j = 0; j < aec->num_bands - 1; ++j) {
    memcpy(aec->dBufH[j], aec->dBufH[j] + PART_LEN, sizeof(float) * PART_LEN);
  }

  memmove(aec->xfwBuf + PART_LEN1,
          aec->xfwBuf,
          sizeof(aec->xfwBuf) - sizeof(complex_t) * PART_LEN1);
}

static void ProcessBlock(AecCore* aec) {
  size_t i;

  float fft[PART_LEN2];
  float xf[2][PART_LEN1];
  float df[2][PART_LEN1];
  float far_spectrum = 0.0f;
  float near_spectrum = 0.0f;
  float abs_far_spectrum[PART_LEN1];
  float abs_near_spectrum[PART_LEN1];

  const float gPow[2] = {0.9f, 0.1f};

  // Noise estimate constants.
  const int noiseInitBlocks = 500 * aec->mult;
  const float step = 0.1f;
  const float ramp = 1.0002f;
  const float gInitNoise[2] = {0.999f, 0.001f};

  float nearend[PART_LEN];
  float* nearend_ptr = NULL;
  float farend[PART_LEN2];
  float* farend_ptr = NULL;
  float echo_subtractor_output[PART_LEN];
  float output[PART_LEN];
  float outputH[NUM_HIGH_BANDS_MAX][PART_LEN];
  float* outputH_ptr[NUM_HIGH_BANDS_MAX];
  float* xf_ptr = NULL;

  for (i = 0; i < NUM_HIGH_BANDS_MAX; ++i) {
    outputH_ptr[i] = outputH[i];
  }

  // Concatenate old and new nearend blocks.
  for (i = 0; i < aec->num_bands - 1; ++i) {
    WebRtc_ReadBuffer(aec->nearFrBufH[i],
                      (void**)&nearend_ptr,
                      nearend,
                      PART_LEN);
    memcpy(aec->dBufH[i] + PART_LEN, nearend_ptr, sizeof(nearend));
  }
  WebRtc_ReadBuffer(aec->nearFrBuf, (void**)&nearend_ptr, nearend, PART_LEN);
  memcpy(aec->dBuf + PART_LEN, nearend_ptr, sizeof(nearend));

  // We should always have at least one element stored in |far_buf|.
  assert(WebRtc_available_read(aec->far_time_buf) > 0);
  WebRtc_ReadBuffer(aec->far_time_buf, (void**)&farend_ptr, farend, 1);

#ifdef WEBRTC_AEC_DEBUG_DUMP
  {
    // TODO(minyue): |farend_ptr| starts from buffered samples. This will be
    // modified when |aec->far_time_buf| is revised.
    OpenCoreDebugFiles(aec, &webrtc_aec_instance_count);
    if (aec->farFile) {
      RTC_AEC_DEBUG_WAV_WRITE(aec->farFile, &farend_ptr[PART_LEN], PART_LEN);
      RTC_AEC_DEBUG_WAV_WRITE(aec->nearFile, nearend_ptr, PART_LEN);
      aec->debugWritten += sizeof(int16_t) * PART_LEN;
      if (aec->debugWritten >= AECDebugMaxSize()) {
        AECDebugEnable(0);
      }
    }
  }
#endif

  // Convert far-end signal to the frequency domain.
  memcpy(fft, farend_ptr, sizeof(float) * PART_LEN2);
  Fft(fft, xf);
  xf_ptr = &xf[0][0];

  // Near fft
  memcpy(fft, aec->dBuf, sizeof(float) * PART_LEN2);
  Fft(fft, df);

  // Power smoothing
  for (i = 0; i < PART_LEN1; i++) {
    far_spectrum = (xf_ptr[i] * xf_ptr[i]) +
                   (xf_ptr[PART_LEN1 + i] * xf_ptr[PART_LEN1 + i]);
    aec->xPow[i] =
        gPow[0] * aec->xPow[i] + gPow[1] * aec->num_partitions * far_spectrum;
    // Calculate absolute spectra
    abs_far_spectrum[i] = sqrtf(far_spectrum);

    near_spectrum = df[0][i] * df[0][i] + df[1][i] * df[1][i];
    aec->dPow[i] = gPow[0] * aec->dPow[i] + gPow[1] * near_spectrum;
    // Calculate absolute spectra
    abs_near_spectrum[i] = sqrtf(near_spectrum);
  }

  // Estimate noise power. Wait until dPow is more stable.
  if (aec->noiseEstCtr > 50) {
    for (i = 0; i < PART_LEN1; i++) {
      if (aec->dPow[i] < aec->dMinPow[i]) {
        aec->dMinPow[i] =
            (aec->dPow[i] + step * (aec->dMinPow[i] - aec->dPow[i])) * ramp;
      } else {
        aec->dMinPow[i] *= ramp;
      }
    }
  }

  // Smooth increasing noise power from zero at the start,
  // to avoid a sudden burst of comfort noise.
  if (aec->noiseEstCtr < noiseInitBlocks) {
    aec->noiseEstCtr++;
    for (i = 0; i < PART_LEN1; i++) {
      if (aec->dMinPow[i] > aec->dInitMinPow[i]) {
        aec->dInitMinPow[i] = gInitNoise[0] * aec->dInitMinPow[i] +
                              gInitNoise[1] * aec->dMinPow[i];
      } else {
        aec->dInitMinPow[i] = aec->dMinPow[i];
      }
    }
    aec->noisePow = aec->dInitMinPow;
  } else {
    aec->noisePow = aec->dMinPow;
  }

  // Block wise delay estimation used for logging
  if (aec->delay_logging_enabled) {
    if (WebRtc_AddFarSpectrumFloat(
            aec->delay_estimator_farend, abs_far_spectrum, PART_LEN1) == 0) {
      int delay_estimate = WebRtc_DelayEstimatorProcessFloat(
          aec->delay_estimator, abs_near_spectrum, PART_LEN1);
      if (delay_estimate >= 0) {
        // Update delay estimate buffer.
        aec->delay_histogram[delay_estimate]++;
        aec->num_delay_values++;
      }
      if (aec->delay_metrics_delivered == 1 &&
          aec->num_delay_values >= kDelayMetricsAggregationWindow) {
        UpdateDelayMetrics(aec);
      }
    }
  }

  // Update the xfBuf block position.
  aec->xfBufBlockPos--;
  if (aec->xfBufBlockPos == -1) {
    aec->xfBufBlockPos = aec->num_partitions - 1;
  }

  // Buffer xf
  memcpy(aec->xfBuf[0] + aec->xfBufBlockPos * PART_LEN1,
         xf_ptr,
         sizeof(float) * PART_LEN1);
  memcpy(aec->xfBuf[1] + aec->xfBufBlockPos * PART_LEN1,
         &xf_ptr[PART_LEN1],
         sizeof(float) * PART_LEN1);

  // Perform echo subtraction.
  EchoSubtraction(aec,
                  aec->num_partitions,
                  aec->xfBufBlockPos,
                  aec->metricsMode,
                  aec->extended_filter_enabled,
                  aec->normal_mu,
                  aec->normal_error_threshold,
                  aec->xfBuf,
                  nearend_ptr,
                  aec->xPow,
                  aec->wfBuf,
                  &aec->linoutlevel,
                  echo_subtractor_output);


  // Perform echo suppression.
  EchoSuppression(aec, farend_ptr, echo_subtractor_output, output, outputH_ptr);

  if (aec->metricsMode == 1) {
    // Update power levels and echo metrics
    UpdateLevel(&aec->farlevel, (float(*)[PART_LEN1])xf_ptr);
    UpdateLevel(&aec->nearlevel, df);
    UpdateMetrics(aec);
  }

  // Store the output block.
  WebRtc_WriteBuffer(aec->outFrBuf, output, PART_LEN);
  // For high bands
  for (i = 0; i < aec->num_bands - 1; ++i) {
    WebRtc_WriteBuffer(aec->outFrBufH[i], outputH[i], PART_LEN);
  }

#ifdef WEBRTC_AEC_DEBUG_DUMP
  OpenCoreDebugFiles(aec, &webrtc_aec_instance_count);
  if (aec->outLinearFile) {
    RTC_AEC_DEBUG_WAV_WRITE(aec->outLinearFile, echo_subtractor_output, PART_LEN);
    RTC_AEC_DEBUG_WAV_WRITE(aec->outFile, output, PART_LEN);
  }
#endif
}

AecCore* WebRtcAec_CreateAec() {
  int i;
  AecCore* aec = malloc(sizeof(AecCore));
  if (!aec) {
    return NULL;
  }

  aec->nearFrBuf = WebRtc_CreateBuffer(FRAME_LEN + PART_LEN, sizeof(float));
  if (!aec->nearFrBuf) {
    WebRtcAec_FreeAec(aec);
    return NULL;
  }

  aec->outFrBuf = WebRtc_CreateBuffer(FRAME_LEN + PART_LEN, sizeof(float));
  if (!aec->outFrBuf) {
    WebRtcAec_FreeAec(aec);
    return NULL;
  }

  for (i = 0; i < NUM_HIGH_BANDS_MAX; ++i) {
    aec->nearFrBufH[i] = WebRtc_CreateBuffer(FRAME_LEN + PART_LEN,
                                             sizeof(float));
    if (!aec->nearFrBufH[i]) {
      WebRtcAec_FreeAec(aec);
      return NULL;
    }
    aec->outFrBufH[i] = WebRtc_CreateBuffer(FRAME_LEN + PART_LEN,
                                            sizeof(float));
    if (!aec->outFrBufH[i]) {
      WebRtcAec_FreeAec(aec);
      return NULL;
    }
  }

  // Create far-end buffers.
  // For bit exactness with legacy code, each element in |far_time_buf| is
  // supposed to contain |PART_LEN2| samples with an overlap of |PART_LEN|
  // samples from the last frame.
  // TODO(minyue): reduce |far_time_buf| to non-overlapped |PART_LEN| samples.
  aec->far_time_buf =
      WebRtc_CreateBuffer(kBufSizePartitions, sizeof(float) * PART_LEN2);
  if (!aec->far_time_buf) {
    WebRtcAec_FreeAec(aec);
    return NULL;
  }

#ifdef WEBRTC_AEC_DEBUG_DUMP
  aec->instance_index = webrtc_aec_instance_count;
  aec->farFile = aec->nearFile = aec->outFile = aec->outLinearFile = NULL;
  aec->e_fft_file = NULL;
  aec->debug_dump_count = 0;
  aec->debugWritten = 0;
#endif
  aec->delay_estimator_farend =
      WebRtc_CreateDelayEstimatorFarend(PART_LEN1, kHistorySizeBlocks);
  if (aec->delay_estimator_farend == NULL) {
    WebRtcAec_FreeAec(aec);
    return NULL;
  }
  // We create the delay_estimator with the same amount of maximum lookahead as
  // the delay history size (kHistorySizeBlocks) for symmetry reasons.
  aec->delay_estimator = WebRtc_CreateDelayEstimator(
      aec->delay_estimator_farend, kHistorySizeBlocks);
  if (aec->delay_estimator == NULL) {
    WebRtcAec_FreeAec(aec);
    return NULL;
  }
#ifdef WEBRTC_ANDROID
  aec->delay_agnostic_enabled = 1;  // DA-AEC enabled by default.
  // DA-AEC assumes the system is causal from the beginning and will self adjust
  // the lookahead when shifting is required.
  WebRtc_set_lookahead(aec->delay_estimator, 0);
#else
  aec->delay_agnostic_enabled = 0;
  WebRtc_set_lookahead(aec->delay_estimator, kLookaheadBlocks);
#endif
  aec->extended_filter_enabled = 0;

  // Assembly optimization
  WebRtcAec_FilterFar = FilterFar;
  WebRtcAec_ScaleErrorSignal = ScaleErrorSignal;
  WebRtcAec_FilterAdaptation = FilterAdaptation;
  WebRtcAec_OverdriveAndSuppress = OverdriveAndSuppress;
  WebRtcAec_ComfortNoise = ComfortNoise;
  WebRtcAec_SubbandCoherence = SubbandCoherence;
  WebRtcAec_StoreAsComplex = StoreAsComplex;
  WebRtcAec_PartitionDelay = PartitionDelay;
  WebRtcAec_WindowData = WindowData;


#if defined(WEBRTC_ARCH_X86_FAMILY)
  if (WebRtc_GetCPUInfo(kSSE2)) {
    WebRtcAec_InitAec_SSE2();
  }
#endif

#if defined(MIPS_FPU_LE)
  WebRtcAec_InitAec_mips();
#endif

#if defined(WEBRTC_HAS_NEON)
  WebRtcAec_InitAec_neon();
#elif defined(WEBRTC_DETECT_NEON)
  if ((WebRtc_GetCPUFeaturesARM() & kCPUFeatureNEON) != 0) {
    WebRtcAec_InitAec_neon();
  }
#endif

  aec_rdft_init();

  return aec;
}

void WebRtcAec_FreeAec(AecCore* aec) {
  int i;
  if (aec == NULL) {
    return;
  }

  WebRtc_FreeBuffer(aec->nearFrBuf);
  WebRtc_FreeBuffer(aec->outFrBuf);

  for (i = 0; i < NUM_HIGH_BANDS_MAX; ++i) {
    WebRtc_FreeBuffer(aec->nearFrBufH[i]);
    WebRtc_FreeBuffer(aec->outFrBufH[i]);
  }

  WebRtc_FreeBuffer(aec->far_time_buf);

#ifdef WEBRTC_AEC_DEBUG_DUMP
  if (aec->farFile) {
    RTC_AEC_DEBUG_WAV_CLOSE(aec->farFile);
    RTC_AEC_DEBUG_WAV_CLOSE(aec->nearFile);
    RTC_AEC_DEBUG_WAV_CLOSE(aec->outFile);
    RTC_AEC_DEBUG_WAV_CLOSE(aec->outLinearFile);
    RTC_AEC_DEBUG_RAW_CLOSE(aec->e_fft_file);
  }
#endif

  WebRtc_FreeDelayEstimator(aec->delay_estimator);
  WebRtc_FreeDelayEstimatorFarend(aec->delay_estimator_farend);

  free(aec);
}

int WebRtcAec_InitAec(AecCore* aec, int sampFreq) {
  int i;

  aec->sampFreq = sampFreq;

  if (sampFreq == 8000) {
    aec->normal_mu = 0.6f;
    aec->normal_error_threshold = 2e-6f;
    aec->num_bands = 1;
  } else {
    aec->normal_mu = 0.5f;
    aec->normal_error_threshold = 1.5e-6f;
    aec->num_bands = (size_t)(sampFreq / 16000);
  }

  WebRtc_InitBuffer(aec->nearFrBuf);
  WebRtc_InitBuffer(aec->outFrBuf);
  for (i = 0; i < NUM_HIGH_BANDS_MAX; ++i) {
    WebRtc_InitBuffer(aec->nearFrBufH[i]);
    WebRtc_InitBuffer(aec->outFrBufH[i]);
  }

  // Initialize far-end buffers.
  WebRtc_InitBuffer(aec->far_time_buf);

#ifdef WEBRTC_AEC_DEBUG_DUMP
  aec->instance_index = webrtc_aec_instance_count;
  OpenCoreDebugFiles(aec, &webrtc_aec_instance_count);
  ++aec->debug_dump_count;
#endif
  aec->system_delay = 0;

  if (WebRtc_InitDelayEstimatorFarend(aec->delay_estimator_farend) != 0) {
    return -1;
  }
  if (WebRtc_InitDelayEstimator(aec->delay_estimator) != 0) {
    return -1;
  }
  aec->delay_logging_enabled = 0;
  aec->delay_metrics_delivered = 0;
  memset(aec->delay_histogram, 0, sizeof(aec->delay_histogram));
  aec->num_delay_values = 0;
  aec->delay_median = -1;
  aec->delay_std = -1;
  aec->fraction_poor_delays = -1.0f;

  aec->signal_delay_correction = 0;
  aec->previous_delay = -2;  // (-2): Uninitialized.
  aec->delay_correction_count = 0;
  aec->shift_offset = kInitialShiftOffset;
  aec->delay_quality_threshold = kDelayQualityThresholdMin;

  aec->num_partitions = kNormalNumPartitions;

  // Update the delay estimator with filter length.  We use half the
  // |num_partitions| to take the echo path into account.  In practice we say
  // that the echo has a duration of maximum half |num_partitions|, which is not
  // true, but serves as a crude measure.
  WebRtc_set_allowed_offset(aec->delay_estimator, aec->num_partitions / 2);
  // TODO(bjornv): I currently hard coded the enable.  Once we've established
  // that AECM has no performance regression, robust_validation will be enabled
  // all the time and the APIs to turn it on/off will be removed.  Hence, remove
  // this line then.
  WebRtc_enable_robust_validation(aec->delay_estimator, 1);
  aec->frame_count = 0;

  // Default target suppression mode.
  aec->nlp_mode = 1;

  // Sampling frequency multiplier w.r.t. 8 kHz.
  // In case of multiple bands we process the lower band in 16 kHz, hence the
  // multiplier is always 2.
  if (aec->num_bands > 1) {
    aec->mult = 2;
  } else {
    aec->mult = (short)aec->sampFreq / 8000;
  }

  aec->farBufWritePos = 0;
  aec->farBufReadPos = 0;

  aec->inSamples = 0;
  aec->outSamples = 0;
  aec->knownDelay = 0;

  // Initialize buffers
  memset(aec->dBuf, 0, sizeof(aec->dBuf));
  memset(aec->eBuf, 0, sizeof(aec->eBuf));
  // For H bands
  for (i = 0; i < NUM_HIGH_BANDS_MAX; ++i) {
    memset(aec->dBufH[i], 0, sizeof(aec->dBufH[i]));
  }

  memset(aec->xPow, 0, sizeof(aec->xPow));
  memset(aec->dPow, 0, sizeof(aec->dPow));
  memset(aec->dInitMinPow, 0, sizeof(aec->dInitMinPow));
  aec->noisePow = aec->dInitMinPow;
  aec->noiseEstCtr = 0;

  // Initial comfort noise power
  for (i = 0; i < PART_LEN1; i++) {
    aec->dMinPow[i] = 1.0e6f;
  }

  // Holds the last block written to
  aec->xfBufBlockPos = 0;
  // TODO: Investigate need for these initializations. Deleting them doesn't
  //       change the output at all and yields 0.4% overall speedup.
  memset(aec->xfBuf, 0, sizeof(complex_t) * kExtendedNumPartitions * PART_LEN1);
  memset(aec->wfBuf, 0, sizeof(complex_t) * kExtendedNumPartitions * PART_LEN1);
  memset(aec->sde, 0, sizeof(complex_t) * PART_LEN1);
  memset(aec->sxd, 0, sizeof(complex_t) * PART_LEN1);
  memset(
      aec->xfwBuf, 0, sizeof(complex_t) * kExtendedNumPartitions * PART_LEN1);
  memset(aec->se, 0, sizeof(float) * PART_LEN1);

  // To prevent numerical instability in the first block.
  for (i = 0; i < PART_LEN1; i++) {
    aec->sd[i] = 1;
  }
  for (i = 0; i < PART_LEN1; i++) {
    aec->sx[i] = 1;
  }

  memset(aec->hNs, 0, sizeof(aec->hNs));
  memset(aec->outBuf, 0, sizeof(float) * PART_LEN);

  aec->hNlFbMin = 1;
  aec->hNlFbLocalMin = 1;
  aec->hNlXdAvgMin = 1;
  aec->hNlNewMin = 0;
  aec->hNlMinCtr = 0;
  aec->overDrive = 2;
  aec->overDriveSm = 2;
  aec->delayIdx = 0;
  aec->stNearState = 0;
  aec->echoState = 0;
  aec->divergeState = 0;

  aec->seed = 777;
  aec->delayEstCtr = 0;

  aec->extreme_filter_divergence = 0;

  // Metrics disabled by default
  aec->metricsMode = 0;
  InitMetrics(aec);

  return 0;
}


// For bit exactness with a legacy code, |farend| is supposed to contain
// |PART_LEN2| samples with an overlap of |PART_LEN| samples from the last
// frame.
// TODO(minyue): reduce |farend| to non-overlapped |PART_LEN| samples.
void WebRtcAec_BufferFarendPartition(AecCore* aec, const float* farend) {
  // Check if the buffer is full, and in that case flush the oldest data.
  if (WebRtc_available_write(aec->far_time_buf) < 1) {
    WebRtcAec_MoveFarReadPtr(aec, 1);
  }

  WebRtc_WriteBuffer(aec->far_time_buf, farend, 1);
}

int WebRtcAec_MoveFarReadPtr(AecCore* aec, int elements) {
  int elements_moved = WebRtc_MoveReadPtr(aec->far_time_buf, elements);
  aec->system_delay -= elements_moved * PART_LEN;
  return elements_moved;
}

void WebRtcAec_ProcessFrames(AecCore* aec,
                             const float* const* nearend,
                             size_t num_bands,
                             size_t num_samples,
                             int knownDelay,
                             float* const* out) {
  size_t i, j;
  int out_elements = 0;

  aec->frame_count++;
  // For each frame the process is as follows:
  // 1) If the system_delay indicates on being too small for processing a
  //    frame we stuff the buffer with enough data for 10 ms.
  // 2 a) Adjust the buffer to the system delay, by moving the read pointer.
  //   b) Apply signal based delay correction, if we have detected poor AEC
  //    performance.
  // 3) TODO(bjornv): Investigate if we need to add this:
  //    If we can't move read pointer due to buffer size limitations we
  //    flush/stuff the buffer.
  // 4) Process as many partitions as possible.
  // 5) Update the |system_delay| with respect to a full frame of FRAME_LEN
  //    samples. Even though we will have data left to process (we work with
  //    partitions) we consider updating a whole frame, since that's the
  //    amount of data we input and output in audio_processing.
  // 6) Update the outputs.

  // The AEC has two different delay estimation algorithms built in.  The
  // first relies on delay input values from the user and the amount of
  // shifted buffer elements is controlled by |knownDelay|.  This delay will
  // give a guess on how much we need to shift far-end buffers to align with
  // the near-end signal.  The other delay estimation algorithm uses the
  // far- and near-end signals to find the offset between them.  This one
  // (called "signal delay") is then used to fine tune the alignment, or
  // simply compensate for errors in the system based one.
  // Note that the two algorithms operate independently.  Currently, we only
  // allow one algorithm to be turned on.

  assert(aec->num_bands == num_bands);

  for (j = 0; j < num_samples; j+= FRAME_LEN) {
    // TODO(bjornv): Change the near-end buffer handling to be the same as for
    // far-end, that is, with a near_pre_buf.
    // Buffer the near-end frame.
    WebRtc_WriteBuffer(aec->nearFrBuf, &nearend[0][j], FRAME_LEN);
    // For H band
    for (i = 1; i < num_bands; ++i) {
      WebRtc_WriteBuffer(aec->nearFrBufH[i - 1], &nearend[i][j], FRAME_LEN);
    }

    // 1) At most we process |aec->mult|+1 partitions in 10 ms. Make sure we
    // have enough far-end data for that by stuffing the buffer if the
    // |system_delay| indicates others.
    if (aec->system_delay < FRAME_LEN) {
      // We don't have enough data so we rewind 10 ms.
      WebRtcAec_MoveFarReadPtr(aec, -(aec->mult + 1));
    }

    if (!aec->delay_agnostic_enabled) {
      // 2 a) Compensate for a possible change in the system delay.

      // TODO(bjornv): Investigate how we should round the delay difference;
      // right now we know that incoming |knownDelay| is underestimated when
      // it's less than |aec->knownDelay|. We therefore, round (-32) in that
      // direction. In the other direction, we don't have this situation, but
      // might flush one partition too little. This can cause non-causality,
      // which should be investigated. Maybe, allow for a non-symmetric
      // rounding, like -16.
      int move_elements = (aec->knownDelay - knownDelay - 32) / PART_LEN;
      int moved_elements =
          WebRtc_MoveReadPtr(aec->far_time_buf, move_elements);
      aec->knownDelay -= moved_elements * PART_LEN;
    } else {
      // 2 b) Apply signal based delay correction.
      int move_elements = SignalBasedDelayCorrection(aec);
      int moved_elements =
          WebRtc_MoveReadPtr(aec->far_time_buf, move_elements);
      int far_near_buffer_diff = WebRtc_available_read(aec->far_time_buf) -
          WebRtc_available_read(aec->nearFrBuf) / PART_LEN;
      WebRtc_SoftResetDelayEstimator(aec->delay_estimator, moved_elements);
      WebRtc_SoftResetDelayEstimatorFarend(aec->delay_estimator_farend,
                                           moved_elements);
      aec->signal_delay_correction += moved_elements;
      // If we rely on reported system delay values only, a buffer underrun here
      // can never occur since we've taken care of that in 1) above.  Here, we
      // apply signal based delay correction and can therefore end up with
      // buffer underruns since the delay estimation can be wrong.  We therefore
      // stuff the buffer with enough elements if needed.
      if (far_near_buffer_diff < 0) {
        WebRtcAec_MoveFarReadPtr(aec, far_near_buffer_diff);
      }
    }

    // 4) Process as many blocks as possible.
    while (WebRtc_available_read(aec->nearFrBuf) >= PART_LEN) {
      ProcessBlock(aec);
    }

    // 5) Update system delay with respect to the entire frame.
    aec->system_delay -= FRAME_LEN;

    // 6) Update output frame.
    // Stuff the out buffer if we have less than a frame to output.
    // This should only happen for the first frame.
    out_elements = (int)WebRtc_available_read(aec->outFrBuf);
    if (out_elements < FRAME_LEN) {
      WebRtc_MoveReadPtr(aec->outFrBuf, out_elements - FRAME_LEN);
      for (i = 0; i < num_bands - 1; ++i) {
        WebRtc_MoveReadPtr(aec->outFrBufH[i], out_elements - FRAME_LEN);
      }
    }
    // Obtain an output frame.
    WebRtc_ReadBuffer(aec->outFrBuf, NULL, &out[0][j], FRAME_LEN);
    // For H bands.
    for (i = 1; i < num_bands; ++i) {
      WebRtc_ReadBuffer(aec->outFrBufH[i - 1], NULL, &out[i][j], FRAME_LEN);
    }
  }
}

int WebRtcAec_GetDelayMetricsCore(AecCore* self, int* median, int* std,
                                  float* fraction_poor_delays) {
  assert(self != NULL);
  assert(median != NULL);
  assert(std != NULL);

  if (self->delay_logging_enabled == 0) {
    // Logging disabled.
    return -1;
  }

  if (self->delay_metrics_delivered == 0) {
    UpdateDelayMetrics(self);
    self->delay_metrics_delivered = 1;
  }
  *median = self->delay_median;
  *std = self->delay_std;
  *fraction_poor_delays = self->fraction_poor_delays;

  return 0;
}

int WebRtcAec_echo_state(AecCore* self) { return self->echoState; }

void WebRtcAec_GetEchoStats(AecCore* self,
                            Stats* erl,
                            Stats* erle,
                            Stats* a_nlp) {
  assert(erl != NULL);
  assert(erle != NULL);
  assert(a_nlp != NULL);
  *erl = self->erl;
  *erle = self->erle;
  *a_nlp = self->aNlp;
}

void WebRtcAec_SetConfigCore(AecCore* self,
                             int nlp_mode,
                             int metrics_mode,
                             int delay_logging) {
  assert(nlp_mode >= 0 && nlp_mode < 3);
  self->nlp_mode = nlp_mode;
  self->metricsMode = metrics_mode;
  if (self->metricsMode) {
    InitMetrics(self);
  }
  // Turn on delay logging if it is either set explicitly or if delay agnostic
  // AEC is enabled (which requires delay estimates).
  self->delay_logging_enabled = delay_logging || self->delay_agnostic_enabled;
  if (self->delay_logging_enabled) {
    memset(self->delay_histogram, 0, sizeof(self->delay_histogram));
  }
}

void WebRtcAec_enable_delay_agnostic(AecCore* self, int enable) {
  self->delay_agnostic_enabled = enable;
}

int WebRtcAec_delay_agnostic_enabled(AecCore* self) {
  return self->delay_agnostic_enabled;
}

void WebRtcAec_enable_extended_filter(AecCore* self, int enable) {
  self->extended_filter_enabled = enable;
  self->num_partitions = enable ? kExtendedNumPartitions : kNormalNumPartitions;
  // Update the delay estimator with filter length.  See InitAEC() for details.
  WebRtc_set_allowed_offset(self->delay_estimator, self->num_partitions / 2);
}

int WebRtcAec_extended_filter_enabled(AecCore* self) {
  return self->extended_filter_enabled;
}

int WebRtcAec_system_delay(AecCore* self) { return self->system_delay; }

void WebRtcAec_SetSystemDelay(AecCore* self, int delay) {
  assert(delay >= 0);
  self->system_delay = delay;
}
