/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "aecm_core.h"

#include <assert.h>
#include <stdlib.h>

#include "cpu_features_wrapper.h"
#include "delay_estimator_wrapper.h"
#include "echo_control_mobile.h"
#include "ring_buffer.h"
#include "typedefs.h"

#ifdef ARM_WINM_LOG
#include <stdio.h>
#include <windows.h>
#endif

#ifdef AEC_DEBUG
FILE *dfile;
FILE *testfile;
#endif

#ifdef _MSC_VER // visual c++
#define ALIGN8_BEG __declspec(align(8))
#define ALIGN8_END
#else // gcc or icc
#define ALIGN8_BEG
#define ALIGN8_END __attribute__((aligned(8)))
#endif

#ifdef AECM_SHORT

// Square root of Hanning window in Q14
const WebRtc_Word16 WebRtcAecm_kSqrtHanning[] =
{
    0, 804, 1606, 2404, 3196, 3981, 4756, 5520,
    6270, 7005, 7723, 8423, 9102, 9760, 10394, 11003,
    11585, 12140, 12665, 13160, 13623, 14053, 14449, 14811,
    15137, 15426, 15679, 15893, 16069, 16207, 16305, 16364,
    16384
};

#else

// Square root of Hanning window in Q14
const ALIGN8_BEG WebRtc_Word16 WebRtcAecm_kSqrtHanning[] ALIGN8_END =
{
    0, 399, 798, 1196, 1594, 1990, 2386, 2780, 3172,
    3562, 3951, 4337, 4720, 5101, 5478, 5853, 6224, 6591, 6954, 7313, 7668, 8019, 8364,
    8705, 9040, 9370, 9695, 10013, 10326, 10633, 10933, 11227, 11514, 11795, 12068, 12335,
    12594, 12845, 13089, 13325, 13553, 13773, 13985, 14189, 14384, 14571, 14749, 14918,
    15079, 15231, 15373, 15506, 15631, 15746, 15851, 15947, 16034, 16111, 16179, 16237,
    16286, 16325, 16354, 16373, 16384
};

#endif

//Q15 alpha = 0.99439986968132  const Factor for magnitude approximation
static const WebRtc_UWord16 kAlpha1 = 32584;
//Q15 beta = 0.12967166976970   const Factor for magnitude approximation
static const WebRtc_UWord16 kBeta1 = 4249;
//Q15 alpha = 0.94234827210087  const Factor for magnitude approximation
static const WebRtc_UWord16 kAlpha2 = 30879;
//Q15 beta = 0.33787806009150   const Factor for magnitude approximation
static const WebRtc_UWord16 kBeta2 = 11072;
//Q15 alpha = 0.82247698684306  const Factor for magnitude approximation
static const WebRtc_UWord16 kAlpha3 = 26951;
//Q15 beta = 0.57762063060713   const Factor for magnitude approximation
static const WebRtc_UWord16 kBeta3 = 18927;

// Initialization table for echo channel in 8 kHz
static const WebRtc_Word16 kChannelStored8kHz[PART_LEN1] = {
    2040,   1815,   1590,   1498,   1405,   1395,   1385,   1418,
    1451,   1506,   1562,   1644,   1726,   1804,   1882,   1918,
    1953,   1982,   2010,   2025,   2040,   2034,   2027,   2021,
    2014,   1997,   1980,   1925,   1869,   1800,   1732,   1683,
    1635,   1604,   1572,   1545,   1517,   1481,   1444,   1405,
    1367,   1331,   1294,   1270,   1245,   1239,   1233,   1247,
    1260,   1282,   1303,   1338,   1373,   1407,   1441,   1470,
    1499,   1524,   1549,   1565,   1582,   1601,   1621,   1649,
    1676
};

// Initialization table for echo channel in 16 kHz
static const WebRtc_Word16 kChannelStored16kHz[PART_LEN1] = {
    2040,   1590,   1405,   1385,   1451,   1562,   1726,   1882,
    1953,   2010,   2040,   2027,   2014,   1980,   1869,   1732,
    1635,   1572,   1517,   1444,   1367,   1294,   1245,   1233,
    1260,   1303,   1373,   1441,   1499,   1549,   1582,   1621,
    1676,   1741,   1802,   1861,   1921,   1983,   2040,   2102,
    2170,   2265,   2375,   2515,   2651,   2781,   2922,   3075,
    3253,   3471,   3738,   3976,   4151,   4258,   4308,   4288,
    4270,   4253,   4237,   4179,   4086,   3947,   3757,   3484,
    3153
};

static const WebRtc_Word16 kCosTable[] = {
    8192,  8190,  8187,  8180,  8172,  8160,  8147,  8130,  8112,
    8091,  8067,  8041,  8012,  7982,  7948,  7912,  7874,  7834,
    7791,  7745,  7697,  7647,  7595,  7540,  7483,  7424,  7362,
    7299,  7233,  7164,  7094,  7021,  6947,  6870,  6791,  6710,
    6627,  6542,  6455,  6366,  6275,  6182,  6087,  5991,  5892,
    5792,  5690,  5586,  5481,  5374,  5265,  5155,  5043,  4930,
    4815,  4698,  4580,  4461,  4341,  4219,  4096,  3971,  3845,
    3719,  3591,  3462,  3331,  3200,  3068,  2935,  2801,  2667,
    2531,  2395,  2258,  2120,  1981,  1842,  1703,  1563,  1422,
    1281,  1140,   998,   856,   713,   571,   428,   285,   142,
       0,  -142,  -285,  -428,  -571,  -713,  -856,  -998, -1140,
   -1281, -1422, -1563, -1703, -1842, -1981, -2120, -2258, -2395,
   -2531, -2667, -2801, -2935, -3068, -3200, -3331, -3462, -3591,
   -3719, -3845, -3971, -4095, -4219, -4341, -4461, -4580, -4698,
   -4815, -4930, -5043, -5155, -5265, -5374, -5481, -5586, -5690,
   -5792, -5892, -5991, -6087, -6182, -6275, -6366, -6455, -6542,
   -6627, -6710, -6791, -6870, -6947, -7021, -7094, -7164, -7233,
   -7299, -7362, -7424, -7483, -7540, -7595, -7647, -7697, -7745,
   -7791, -7834, -7874, -7912, -7948, -7982, -8012, -8041, -8067,
   -8091, -8112, -8130, -8147, -8160, -8172, -8180, -8187, -8190,
   -8191, -8190, -8187, -8180, -8172, -8160, -8147, -8130, -8112,
   -8091, -8067, -8041, -8012, -7982, -7948, -7912, -7874, -7834,
   -7791, -7745, -7697, -7647, -7595, -7540, -7483, -7424, -7362,
   -7299, -7233, -7164, -7094, -7021, -6947, -6870, -6791, -6710,
   -6627, -6542, -6455, -6366, -6275, -6182, -6087, -5991, -5892,
   -5792, -5690, -5586, -5481, -5374, -5265, -5155, -5043, -4930,
   -4815, -4698, -4580, -4461, -4341, -4219, -4096, -3971, -3845,
   -3719, -3591, -3462, -3331, -3200, -3068, -2935, -2801, -2667,
   -2531, -2395, -2258, -2120, -1981, -1842, -1703, -1563, -1422,
   -1281, -1140,  -998,  -856,  -713,  -571,  -428,  -285,  -142,
       0,   142,   285,   428,   571,   713,   856,   998,  1140,
    1281,  1422,  1563,  1703,  1842,  1981,  2120,  2258,  2395,
    2531,  2667,  2801,  2935,  3068,  3200,  3331,  3462,  3591,
    3719,  3845,  3971,  4095,  4219,  4341,  4461,  4580,  4698,
    4815,  4930,  5043,  5155,  5265,  5374,  5481,  5586,  5690,
    5792,  5892,  5991,  6087,  6182,  6275,  6366,  6455,  6542,
    6627,  6710,  6791,  6870,  6947,  7021,  7094,  7164,  7233,
    7299,  7362,  7424,  7483,  7540,  7595,  7647,  7697,  7745,
    7791,  7834,  7874,  7912,  7948,  7982,  8012,  8041,  8067,
    8091,  8112,  8130,  8147,  8160,  8172,  8180,  8187,  8190
};

static const WebRtc_Word16 kSinTable[] = {
       0,    142,    285,    428,    571,    713,    856,    998,
    1140,   1281,   1422,   1563,   1703,   1842,   1981,   2120,
    2258,   2395,   2531,   2667,   2801,   2935,   3068,   3200,
    3331,   3462,   3591,   3719,   3845,   3971,   4095,   4219,
    4341,   4461,   4580,   4698,   4815,   4930,   5043,   5155,
    5265,   5374,   5481,   5586,   5690,   5792,   5892,   5991,
    6087,   6182,   6275,   6366,   6455,   6542,   6627,   6710,
    6791,   6870,   6947,   7021,   7094,   7164,   7233,   7299,
    7362,   7424,   7483,   7540,   7595,   7647,   7697,   7745,
    7791,   7834,   7874,   7912,   7948,   7982,   8012,   8041,
    8067,   8091,   8112,   8130,   8147,   8160,   8172,   8180,
    8187,   8190,   8191,   8190,   8187,   8180,   8172,   8160,
    8147,   8130,   8112,   8091,   8067,   8041,   8012,   7982,
    7948,   7912,   7874,   7834,   7791,   7745,   7697,   7647,
    7595,   7540,   7483,   7424,   7362,   7299,   7233,   7164,
    7094,   7021,   6947,   6870,   6791,   6710,   6627,   6542,
    6455,   6366,   6275,   6182,   6087,   5991,   5892,   5792,
    5690,   5586,   5481,   5374,   5265,   5155,   5043,   4930,
    4815,   4698,   4580,   4461,   4341,   4219,   4096,   3971,
    3845,   3719,   3591,   3462,   3331,   3200,   3068,   2935,
    2801,   2667,   2531,   2395,   2258,   2120,   1981,   1842,
    1703,   1563,   1422,   1281,   1140,    998,    856,    713,
     571,    428,    285,    142,      0,   -142,   -285,   -428,
    -571,   -713,   -856,   -998,  -1140,  -1281,  -1422,  -1563,
   -1703,  -1842,  -1981,  -2120,  -2258,  -2395,  -2531,  -2667,
   -2801,  -2935,  -3068,  -3200,  -3331,  -3462,  -3591,  -3719,
   -3845,  -3971,  -4095,  -4219,  -4341,  -4461,  -4580,  -4698,
   -4815,  -4930,  -5043,  -5155,  -5265,  -5374,  -5481,  -5586,
   -5690,  -5792,  -5892,  -5991,  -6087,  -6182,  -6275,  -6366,
   -6455,  -6542,  -6627,  -6710,  -6791,  -6870,  -6947,  -7021,
   -7094,  -7164,  -7233,  -7299,  -7362,  -7424,  -7483,  -7540,
   -7595,  -7647,  -7697,  -7745,  -7791,  -7834,  -7874,  -7912,
   -7948,  -7982,  -8012,  -8041,  -8067,  -8091,  -8112,  -8130,
   -8147,  -8160,  -8172,  -8180,  -8187,  -8190,  -8191,  -8190,
   -8187,  -8180,  -8172,  -8160,  -8147,  -8130,  -8112,  -8091,
   -8067,  -8041,  -8012,  -7982,  -7948,  -7912,  -7874,  -7834,
   -7791,  -7745,  -7697,  -7647,  -7595,  -7540,  -7483,  -7424,
   -7362,  -7299,  -7233,  -7164,  -7094,  -7021,  -6947,  -6870,
   -6791,  -6710,  -6627,  -6542,  -6455,  -6366,  -6275,  -6182,
   -6087,  -5991,  -5892,  -5792,  -5690,  -5586,  -5481,  -5374,
   -5265,  -5155,  -5043,  -4930,  -4815,  -4698,  -4580,  -4461,
   -4341,  -4219,  -4096,  -3971,  -3845,  -3719,  -3591,  -3462,
   -3331,  -3200,  -3068,  -2935,  -2801,  -2667,  -2531,  -2395,
   -2258,  -2120,  -1981,  -1842,  -1703,  -1563,  -1422,  -1281,
   -1140,   -998,   -856,   -713,   -571,   -428,   -285,   -142
};

static const WebRtc_Word16 kNoiseEstQDomain = 15;
static const WebRtc_Word16 kNoiseEstIncCount = 5;

static void ComfortNoise(AecmCore_t* aecm,
                         const WebRtc_UWord16* dfa,
                         complex16_t* out,
                         const WebRtc_Word16* lambda);

static WebRtc_Word16 CalcSuppressionGain(AecmCore_t * const aecm);

// Moves the pointer to the next entry and inserts |far_spectrum| and
// corresponding Q-domain in its buffer.
//
// Inputs:
//      - self          : Pointer to the delay estimation instance
//      - far_spectrum  : Pointer to the far end spectrum
//      - far_q         : Q-domain of far end spectrum
//
static void UpdateFarHistory(AecmCore_t* self,
                             uint16_t* far_spectrum,
                             int far_q) {
  // Get new buffer position
  self->far_history_pos++;
  if (self->far_history_pos >= MAX_DELAY) {
    self->far_history_pos = 0;
  }
  // Update Q-domain buffer
  self->far_q_domains[self->far_history_pos] = far_q;
  // Update far end spectrum buffer
  memcpy(&(self->far_history[self->far_history_pos * PART_LEN1]),
         far_spectrum,
         sizeof(uint16_t) * PART_LEN1);
}

// Returns a pointer to the far end spectrum aligned to current near end
// spectrum. The function WebRtc_DelayEstimatorProcessFix(...) should have been
// called before AlignedFarend(...). Otherwise, you get the pointer to the
// previous frame. The memory is only valid until the next call of
// WebRtc_DelayEstimatorProcessFix(...).
//
// Inputs:
//      - self              : Pointer to the AECM instance.
//      - delay             : Current delay estimate.
//
// Output:
//      - far_q             : The Q-domain of the aligned far end spectrum
//
// Return value:
//      - far_spectrum      : Pointer to the aligned far end spectrum
//                            NULL - Error
//
static const uint16_t* AlignedFarend(AecmCore_t* self, int* far_q, int delay) {
  int buffer_position = 0;
  assert(self != NULL);
  buffer_position = self->far_history_pos - delay;

  // Check buffer position
  if (buffer_position < 0) {
    buffer_position += MAX_DELAY;
  }
  // Get Q-domain
  *far_q = self->far_q_domains[buffer_position];
  // Return far end spectrum
  return &(self->far_history[buffer_position * PART_LEN1]);
}

#ifdef ARM_WINM_LOG
HANDLE logFile = NULL;
#endif

// Declare function pointers.
CalcLinearEnergies WebRtcAecm_CalcLinearEnergies;
StoreAdaptiveChannel WebRtcAecm_StoreAdaptiveChannel;
ResetAdaptiveChannel WebRtcAecm_ResetAdaptiveChannel;
WindowAndFFT WebRtcAecm_WindowAndFFT;
InverseFFTAndWindow WebRtcAecm_InverseFFTAndWindow;

int WebRtcAecm_CreateCore(AecmCore_t **aecmInst)
{
    AecmCore_t *aecm = malloc(sizeof(AecmCore_t));
    *aecmInst = aecm;
    if (aecm == NULL)
    {
        return -1;
    }

    if (WebRtc_CreateBuffer(&aecm->farFrameBuf, FRAME_LEN + PART_LEN,
                            sizeof(int16_t)) == -1)
    {
        WebRtcAecm_FreeCore(aecm);
        aecm = NULL;
        return -1;
    }

    if (WebRtc_CreateBuffer(&aecm->nearNoisyFrameBuf, FRAME_LEN + PART_LEN,
                            sizeof(int16_t)) == -1)
    {
        WebRtcAecm_FreeCore(aecm);
        aecm = NULL;
        return -1;
    }

    if (WebRtc_CreateBuffer(&aecm->nearCleanFrameBuf, FRAME_LEN + PART_LEN,
                            sizeof(int16_t)) == -1)
    {
        WebRtcAecm_FreeCore(aecm);
        aecm = NULL;
        return -1;
    }

    if (WebRtc_CreateBuffer(&aecm->outFrameBuf, FRAME_LEN + PART_LEN,
                            sizeof(int16_t)) == -1)
    {
        WebRtcAecm_FreeCore(aecm);
        aecm = NULL;
        return -1;
    }

    if (WebRtc_CreateDelayEstimator(&aecm->delay_estimator,
                                    PART_LEN1,
                                    MAX_DELAY,
                                    0) == -1) {
      WebRtcAecm_FreeCore(aecm);
      aecm = NULL;
      return -1;
    }

    // Init some aecm pointers. 16 and 32 byte alignment is only necessary
    // for Neon code currently.
    aecm->xBuf = (WebRtc_Word16*) (((uintptr_t)aecm->xBuf_buf + 31) & ~ 31);
    aecm->dBufClean = (WebRtc_Word16*) (((uintptr_t)aecm->dBufClean_buf + 31) & ~ 31);
    aecm->dBufNoisy = (WebRtc_Word16*) (((uintptr_t)aecm->dBufNoisy_buf + 31) & ~ 31);
    aecm->outBuf = (WebRtc_Word16*) (((uintptr_t)aecm->outBuf_buf + 15) & ~ 15);
    aecm->channelStored = (WebRtc_Word16*) (((uintptr_t)
                                             aecm->channelStored_buf + 15) & ~ 15);
    aecm->channelAdapt16 = (WebRtc_Word16*) (((uintptr_t)
                                              aecm->channelAdapt16_buf + 15) & ~ 15);
    aecm->channelAdapt32 = (WebRtc_Word32*) (((uintptr_t)
                                              aecm->channelAdapt32_buf + 31) & ~ 31);

    return 0;
}

void WebRtcAecm_InitEchoPathCore(AecmCore_t* aecm, const WebRtc_Word16* echo_path)
{
    int i = 0;

    // Reset the stored channel
    memcpy(aecm->channelStored, echo_path, sizeof(WebRtc_Word16) * PART_LEN1);
    // Reset the adapted channels
    memcpy(aecm->channelAdapt16, echo_path, sizeof(WebRtc_Word16) * PART_LEN1);
    for (i = 0; i < PART_LEN1; i++)
    {
        aecm->channelAdapt32[i] = WEBRTC_SPL_LSHIFT_W32(
            (WebRtc_Word32)(aecm->channelAdapt16[i]), 16);
    }

    // Reset channel storing variables
    aecm->mseAdaptOld = 1000;
    aecm->mseStoredOld = 1000;
    aecm->mseThreshold = WEBRTC_SPL_WORD32_MAX;
    aecm->mseChannelCount = 0;
}

static void WindowAndFFTC(WebRtc_Word16* fft,
                          const WebRtc_Word16* time_signal,
                          complex16_t* freq_signal,
                          int time_signal_scaling)
{
    int i, j;

    memset(fft, 0, sizeof(WebRtc_Word16) * PART_LEN4);
    // FFT of signal
    for (i = 0, j = 0; i < PART_LEN; i++, j += 2)
    {
        // Window time domain signal and insert into real part of
        // transformation array |fft|
        fft[j] = (WebRtc_Word16)WEBRTC_SPL_MUL_16_16_RSFT(
            (time_signal[i] << time_signal_scaling),
            WebRtcAecm_kSqrtHanning[i],
            14);
        fft[PART_LEN2 + j] = (WebRtc_Word16)WEBRTC_SPL_MUL_16_16_RSFT(
            (time_signal[i + PART_LEN] << time_signal_scaling),
            WebRtcAecm_kSqrtHanning[PART_LEN - i],
            14);
        // Inserting zeros in imaginary parts not necessary since we
        // initialized the array with all zeros
    }

    WebRtcSpl_ComplexBitReverse(fft, PART_LEN_SHIFT);
    WebRtcSpl_ComplexFFT(fft, PART_LEN_SHIFT, 1);

    // Take only the first PART_LEN2 samples
    for (i = 0, j = 0; j < PART_LEN2; i += 1, j += 2)
    {
        freq_signal[i].real = fft[j];

        // The imaginary part has to switch sign
        freq_signal[i].imag = - fft[j+1];
    }
}

static void InverseFFTAndWindowC(AecmCore_t* aecm,
                                 WebRtc_Word16* fft,
                                 complex16_t* efw,
                                 WebRtc_Word16* output,
                                 const WebRtc_Word16* nearendClean)
{
    int i, j, outCFFT;
    WebRtc_Word32 tmp32no1;

    // Synthesis
    for (i = 1; i < PART_LEN; i++)
    {
        j = WEBRTC_SPL_LSHIFT_W32(i, 1);
        fft[j] = efw[i].real;

        // mirrored data, even
        fft[PART_LEN4 - j] = efw[i].real;
        fft[j + 1] = -efw[i].imag;

        //mirrored data, odd
        fft[PART_LEN4 - (j - 1)] = efw[i].imag;
    }
    fft[0] = efw[0].real;
    fft[1] = -efw[0].imag;

    fft[PART_LEN2] = efw[PART_LEN].real;
    fft[PART_LEN2 + 1] = -efw[PART_LEN].imag;

    // inverse FFT, result should be scaled with outCFFT
    WebRtcSpl_ComplexBitReverse(fft, PART_LEN_SHIFT);
    outCFFT = WebRtcSpl_ComplexIFFT(fft, PART_LEN_SHIFT, 1);

    //take only the real values and scale with outCFFT
    for (i = 0; i < PART_LEN2; i++)
    {
        j = WEBRTC_SPL_LSHIFT_W32(i, 1);
        fft[i] = fft[j];
    }

    for (i = 0; i < PART_LEN; i++)
    {
        fft[i] = (WebRtc_Word16)WEBRTC_SPL_MUL_16_16_RSFT_WITH_ROUND(
                fft[i],
                WebRtcAecm_kSqrtHanning[i],
                14);
        tmp32no1 = WEBRTC_SPL_SHIFT_W32((WebRtc_Word32)fft[i],
                outCFFT - aecm->dfaCleanQDomain);
        fft[i] = (WebRtc_Word16)WEBRTC_SPL_SAT(WEBRTC_SPL_WORD16_MAX,
                tmp32no1 + aecm->outBuf[i],
                WEBRTC_SPL_WORD16_MIN);
        output[i] = fft[i];

        tmp32no1 = WEBRTC_SPL_MUL_16_16_RSFT(
                fft[PART_LEN + i],
                WebRtcAecm_kSqrtHanning[PART_LEN - i],
                14);
        tmp32no1 = WEBRTC_SPL_SHIFT_W32(tmp32no1,
                outCFFT - aecm->dfaCleanQDomain);
        aecm->outBuf[i] = (WebRtc_Word16)WEBRTC_SPL_SAT(
                WEBRTC_SPL_WORD16_MAX,
                tmp32no1,
                WEBRTC_SPL_WORD16_MIN);
    }

#ifdef ARM_WINM_LOG_
    // measure tick end
    QueryPerformanceCounter((LARGE_INTEGER*)&end);
    diff__ = ((end - start) * 1000) / (freq/1000);
    milliseconds = (unsigned int)(diff__ & 0xffffffff);
    WriteFile (logFile, &milliseconds, sizeof(unsigned int), &temp, NULL);
#endif

    // Copy the current block to the old position (aecm->outBuf is shifted elsewhere)
    memcpy(aecm->xBuf, aecm->xBuf + PART_LEN, sizeof(WebRtc_Word16) * PART_LEN);
    memcpy(aecm->dBufNoisy, aecm->dBufNoisy + PART_LEN, sizeof(WebRtc_Word16) * PART_LEN);
    if (nearendClean != NULL)
    {
        memcpy(aecm->dBufClean, aecm->dBufClean + PART_LEN, sizeof(WebRtc_Word16) * PART_LEN);
    }
}

static void CalcLinearEnergiesC(AecmCore_t* aecm,
                                const WebRtc_UWord16* far_spectrum,
                                WebRtc_Word32* echo_est,
                                WebRtc_UWord32* far_energy,
                                WebRtc_UWord32* echo_energy_adapt,
                                WebRtc_UWord32* echo_energy_stored)
{
    int i;

    // Get energy for the delayed far end signal and estimated
    // echo using both stored and adapted channels.
    for (i = 0; i < PART_LEN1; i++)
    {
        echo_est[i] = WEBRTC_SPL_MUL_16_U16(aecm->channelStored[i],
                                           far_spectrum[i]);
        (*far_energy) += (WebRtc_UWord32)(far_spectrum[i]);
        (*echo_energy_adapt) += WEBRTC_SPL_UMUL_16_16(aecm->channelAdapt16[i],
                                          far_spectrum[i]);
        (*echo_energy_stored) += (WebRtc_UWord32)echo_est[i];
    }
}

static void StoreAdaptiveChannelC(AecmCore_t* aecm,
                                  const WebRtc_UWord16* far_spectrum,
                                  WebRtc_Word32* echo_est)
{
    int i;

    // During startup we store the channel every block.
    memcpy(aecm->channelStored, aecm->channelAdapt16, sizeof(WebRtc_Word16) * PART_LEN1);
    // Recalculate echo estimate
    for (i = 0; i < PART_LEN; i += 4)
    {
        echo_est[i] = WEBRTC_SPL_MUL_16_U16(aecm->channelStored[i],
                                           far_spectrum[i]);
        echo_est[i + 1] = WEBRTC_SPL_MUL_16_U16(aecm->channelStored[i + 1],
                                           far_spectrum[i + 1]);
        echo_est[i + 2] = WEBRTC_SPL_MUL_16_U16(aecm->channelStored[i + 2],
                                           far_spectrum[i + 2]);
        echo_est[i + 3] = WEBRTC_SPL_MUL_16_U16(aecm->channelStored[i + 3],
                                           far_spectrum[i + 3]);
    }
    echo_est[i] = WEBRTC_SPL_MUL_16_U16(aecm->channelStored[i],
                                       far_spectrum[i]);
}

static void ResetAdaptiveChannelC(AecmCore_t* aecm)
{
    int i;

    // The stored channel has a significantly lower MSE than the adaptive one for
    // two consecutive calculations. Reset the adaptive channel.
    memcpy(aecm->channelAdapt16, aecm->channelStored,
           sizeof(WebRtc_Word16) * PART_LEN1);
    // Restore the W32 channel
    for (i = 0; i < PART_LEN; i += 4)
    {
        aecm->channelAdapt32[i] = WEBRTC_SPL_LSHIFT_W32(
                (WebRtc_Word32)aecm->channelStored[i], 16);
        aecm->channelAdapt32[i + 1] = WEBRTC_SPL_LSHIFT_W32(
                (WebRtc_Word32)aecm->channelStored[i + 1], 16);
        aecm->channelAdapt32[i + 2] = WEBRTC_SPL_LSHIFT_W32(
                (WebRtc_Word32)aecm->channelStored[i + 2], 16);
        aecm->channelAdapt32[i + 3] = WEBRTC_SPL_LSHIFT_W32(
                (WebRtc_Word32)aecm->channelStored[i + 3], 16);
    }
    aecm->channelAdapt32[i] = WEBRTC_SPL_LSHIFT_W32((WebRtc_Word32)aecm->channelStored[i], 16);
}

// WebRtcAecm_InitCore(...)
//
// This function initializes the AECM instant created with WebRtcAecm_CreateCore(...)
// Input:
//      - aecm            : Pointer to the Echo Suppression instance
//      - samplingFreq   : Sampling Frequency
//
// Output:
//      - aecm            : Initialized instance
//
// Return value         :  0 - Ok
//                        -1 - Error
//
int WebRtcAecm_InitCore(AecmCore_t * const aecm, int samplingFreq)
{
    int i = 0;
    WebRtc_Word32 tmp32 = PART_LEN1 * PART_LEN1;
    WebRtc_Word16 tmp16 = PART_LEN1;

    if (samplingFreq != 8000 && samplingFreq != 16000)
    {
        samplingFreq = 8000;
        return -1;
    }
    // sanity check of sampling frequency
    aecm->mult = (WebRtc_Word16)samplingFreq / 8000;

    aecm->farBufWritePos = 0;
    aecm->farBufReadPos = 0;
    aecm->knownDelay = 0;
    aecm->lastKnownDelay = 0;

    WebRtc_InitBuffer(aecm->farFrameBuf);
    WebRtc_InitBuffer(aecm->nearNoisyFrameBuf);
    WebRtc_InitBuffer(aecm->nearCleanFrameBuf);
    WebRtc_InitBuffer(aecm->outFrameBuf);

    memset(aecm->xBuf_buf, 0, sizeof(aecm->xBuf_buf));
    memset(aecm->dBufClean_buf, 0, sizeof(aecm->dBufClean_buf));
    memset(aecm->dBufNoisy_buf, 0, sizeof(aecm->dBufNoisy_buf));
    memset(aecm->outBuf_buf, 0, sizeof(aecm->outBuf_buf));

    aecm->seed = 666;
    aecm->totCount = 0;

    if (WebRtc_InitDelayEstimator(aecm->delay_estimator) != 0) {
      return -1;
    }
    // Set far end histories to zero
    memset(aecm->far_history, 0, sizeof(uint16_t) * PART_LEN1 * MAX_DELAY);
    memset(aecm->far_q_domains, 0, sizeof(int) * MAX_DELAY);
    aecm->far_history_pos = MAX_DELAY;

    aecm->nlpFlag = 1;
    aecm->fixedDelay = -1;

    aecm->dfaCleanQDomain = 0;
    aecm->dfaCleanQDomainOld = 0;
    aecm->dfaNoisyQDomain = 0;
    aecm->dfaNoisyQDomainOld = 0;

    memset(aecm->nearLogEnergy, 0, sizeof(aecm->nearLogEnergy));
    aecm->farLogEnergy = 0;
    memset(aecm->echoAdaptLogEnergy, 0, sizeof(aecm->echoAdaptLogEnergy));
    memset(aecm->echoStoredLogEnergy, 0, sizeof(aecm->echoStoredLogEnergy));

    // Initialize the echo channels with a stored shape.
    if (samplingFreq == 8000)
    {
        WebRtcAecm_InitEchoPathCore(aecm, kChannelStored8kHz);
    }
    else
    {
        WebRtcAecm_InitEchoPathCore(aecm, kChannelStored16kHz);
    }

    memset(aecm->echoFilt, 0, sizeof(aecm->echoFilt));
    memset(aecm->nearFilt, 0, sizeof(aecm->nearFilt));
    aecm->noiseEstCtr = 0;

    aecm->cngMode = AecmTrue;

    memset(aecm->noiseEstTooLowCtr, 0, sizeof(aecm->noiseEstTooLowCtr));
    memset(aecm->noiseEstTooHighCtr, 0, sizeof(aecm->noiseEstTooHighCtr));
    // Shape the initial noise level to an approximate pink noise.
    for (i = 0; i < (PART_LEN1 >> 1) - 1; i++)
    {
        aecm->noiseEst[i] = (tmp32 << 8);
        tmp16--;
        tmp32 -= (WebRtc_Word32)((tmp16 << 1) + 1);
    }
    for (; i < PART_LEN1; i++)
    {
        aecm->noiseEst[i] = (tmp32 << 8);
    }

    aecm->farEnergyMin = WEBRTC_SPL_WORD16_MAX;
    aecm->farEnergyMax = WEBRTC_SPL_WORD16_MIN;
    aecm->farEnergyMaxMin = 0;
    aecm->farEnergyVAD = FAR_ENERGY_MIN; // This prevents false speech detection at the
                                         // beginning.
    aecm->farEnergyMSE = 0;
    aecm->currentVADValue = 0;
    aecm->vadUpdateCount = 0;
    aecm->firstVAD = 1;

    aecm->startupState = 0;
    aecm->supGain = SUPGAIN_DEFAULT;
    aecm->supGainOld = SUPGAIN_DEFAULT;

    aecm->supGainErrParamA = SUPGAIN_ERROR_PARAM_A;
    aecm->supGainErrParamD = SUPGAIN_ERROR_PARAM_D;
    aecm->supGainErrParamDiffAB = SUPGAIN_ERROR_PARAM_A - SUPGAIN_ERROR_PARAM_B;
    aecm->supGainErrParamDiffBD = SUPGAIN_ERROR_PARAM_B - SUPGAIN_ERROR_PARAM_D;

    assert(PART_LEN % 16 == 0);

    // Initialize function pointers.
    WebRtcAecm_WindowAndFFT = WindowAndFFTC;
    WebRtcAecm_InverseFFTAndWindow = InverseFFTAndWindowC;
    WebRtcAecm_CalcLinearEnergies = CalcLinearEnergiesC;
    WebRtcAecm_StoreAdaptiveChannel = StoreAdaptiveChannelC;
    WebRtcAecm_ResetAdaptiveChannel = ResetAdaptiveChannelC;

#ifdef WEBRTC_DETECT_ARM_NEON
    uint64_t features = WebRtc_GetCPUFeaturesARM();
    if ((features & kCPUFeatureNEON) != 0)
    {
        WebRtcAecm_InitNeon();
    }
#elif defined(WEBRTC_ARCH_ARM_NEON)
    WebRtcAecm_InitNeon();
#endif

    return 0;
}

// TODO(bjornv): This function is currently not used. Add support for these
// parameters from a higher level
int WebRtcAecm_Control(AecmCore_t *aecm, int delay, int nlpFlag)
{
    aecm->nlpFlag = nlpFlag;
    aecm->fixedDelay = delay;

    return 0;
}

int WebRtcAecm_FreeCore(AecmCore_t *aecm)
{
    if (aecm == NULL)
    {
        return -1;
    }

    WebRtc_FreeBuffer(aecm->farFrameBuf);
    WebRtc_FreeBuffer(aecm->nearNoisyFrameBuf);
    WebRtc_FreeBuffer(aecm->nearCleanFrameBuf);
    WebRtc_FreeBuffer(aecm->outFrameBuf);

    WebRtc_FreeDelayEstimator(aecm->delay_estimator);
    free(aecm);

    return 0;
}

int WebRtcAecm_ProcessFrame(AecmCore_t * aecm,
                            const WebRtc_Word16 * farend,
                            const WebRtc_Word16 * nearendNoisy,
                            const WebRtc_Word16 * nearendClean,
                            WebRtc_Word16 * out)
{
    WebRtc_Word16 outBlock_buf[PART_LEN + 8]; // Align buffer to 8-byte boundary.
    WebRtc_Word16* outBlock = (WebRtc_Word16*) (((uintptr_t) outBlock_buf + 15) & ~ 15);

    WebRtc_Word16 farFrame[FRAME_LEN];
    const int16_t* out_ptr = NULL;
    int size = 0;

    // Buffer the current frame.
    // Fetch an older one corresponding to the delay.
    WebRtcAecm_BufferFarFrame(aecm, farend, FRAME_LEN);
    WebRtcAecm_FetchFarFrame(aecm, farFrame, FRAME_LEN, aecm->knownDelay);

    // Buffer the synchronized far and near frames,
    // to pass the smaller blocks individually.
    WebRtc_WriteBuffer(aecm->farFrameBuf, farFrame, FRAME_LEN);
    WebRtc_WriteBuffer(aecm->nearNoisyFrameBuf, nearendNoisy, FRAME_LEN);
    if (nearendClean != NULL)
    {
        WebRtc_WriteBuffer(aecm->nearCleanFrameBuf, nearendClean, FRAME_LEN);
    }

    // Process as many blocks as possible.
    while (WebRtc_available_read(aecm->farFrameBuf) >= PART_LEN)
    {
        int16_t far_block[PART_LEN];
        const int16_t* far_block_ptr = NULL;
        int16_t near_noisy_block[PART_LEN];
        const int16_t* near_noisy_block_ptr = NULL;

        WebRtc_ReadBuffer(aecm->farFrameBuf, (void**) &far_block_ptr, far_block,
                          PART_LEN);
        WebRtc_ReadBuffer(aecm->nearNoisyFrameBuf,
                          (void**) &near_noisy_block_ptr,
                          near_noisy_block,
                          PART_LEN);
        if (nearendClean != NULL)
        {
            int16_t near_clean_block[PART_LEN];
            const int16_t* near_clean_block_ptr = NULL;

            WebRtc_ReadBuffer(aecm->nearCleanFrameBuf,
                              (void**) &near_clean_block_ptr,
                              near_clean_block,
                              PART_LEN);
            if (WebRtcAecm_ProcessBlock(aecm,
                                        far_block_ptr,
                                        near_noisy_block_ptr,
                                        near_clean_block_ptr,
                                        outBlock) == -1)
            {
                return -1;
            }
        } else
        {
            if (WebRtcAecm_ProcessBlock(aecm,
                                        far_block_ptr,
                                        near_noisy_block_ptr,
                                        NULL,
                                        outBlock) == -1)
            {
                return -1;
            }
        }

        WebRtc_WriteBuffer(aecm->outFrameBuf, outBlock, PART_LEN);
    }

    // Stuff the out buffer if we have less than a frame to output.
    // This should only happen for the first frame.
    size = (int) WebRtc_available_read(aecm->outFrameBuf);
    if (size < FRAME_LEN)
    {
        WebRtc_MoveReadPtr(aecm->outFrameBuf, size - FRAME_LEN);
    }

    // Obtain an output frame.
    WebRtc_ReadBuffer(aecm->outFrameBuf, (void**) &out_ptr, out, FRAME_LEN);
    if (out_ptr != out) {
      // ReadBuffer() hasn't copied to |out| in this case.
      memcpy(out, out_ptr, FRAME_LEN * sizeof(int16_t));
    }

    return 0;
}

// WebRtcAecm_AsymFilt(...)
//
// Performs asymmetric filtering.
//
// Inputs:
//      - filtOld       : Previous filtered value.
//      - inVal         : New input value.
//      - stepSizePos   : Step size when we have a positive contribution.
//      - stepSizeNeg   : Step size when we have a negative contribution.
//
// Output:
//
// Return: - Filtered value.
//
WebRtc_Word16 WebRtcAecm_AsymFilt(const WebRtc_Word16 filtOld, const WebRtc_Word16 inVal,
                                  const WebRtc_Word16 stepSizePos,
                                  const WebRtc_Word16 stepSizeNeg)
{
    WebRtc_Word16 retVal;

    if ((filtOld == WEBRTC_SPL_WORD16_MAX) | (filtOld == WEBRTC_SPL_WORD16_MIN))
    {
        return inVal;
    }
    retVal = filtOld;
    if (filtOld > inVal)
    {
        retVal -= WEBRTC_SPL_RSHIFT_W16(filtOld - inVal, stepSizeNeg);
    } else
    {
        retVal += WEBRTC_SPL_RSHIFT_W16(inVal - filtOld, stepSizePos);
    }

    return retVal;
}

// WebRtcAecm_CalcEnergies(...)
//
// This function calculates the log of energies for nearend, farend and estimated
// echoes. There is also an update of energy decision levels, i.e. internal VAD.
//
//
// @param  aecm         [i/o]   Handle of the AECM instance.
// @param  far_spectrum [in]    Pointer to farend spectrum.
// @param  far_q        [in]    Q-domain of farend spectrum.
// @param  nearEner     [in]    Near end energy for current block in
//                              Q(aecm->dfaQDomain).
// @param  echoEst      [out]   Estimated echo in Q(xfa_q+RESOLUTION_CHANNEL16).
//
void WebRtcAecm_CalcEnergies(AecmCore_t * aecm,
                             const WebRtc_UWord16* far_spectrum,
                             const WebRtc_Word16 far_q,
                             const WebRtc_UWord32 nearEner,
                             WebRtc_Word32 * echoEst)
{
    // Local variables
    WebRtc_UWord32 tmpAdapt = 0;
    WebRtc_UWord32 tmpStored = 0;
    WebRtc_UWord32 tmpFar = 0;

    int i;

    WebRtc_Word16 zeros, frac;
    WebRtc_Word16 tmp16;
    WebRtc_Word16 increase_max_shifts = 4;
    WebRtc_Word16 decrease_max_shifts = 11;
    WebRtc_Word16 increase_min_shifts = 11;
    WebRtc_Word16 decrease_min_shifts = 3;
    WebRtc_Word16 kLogLowValue = WEBRTC_SPL_LSHIFT_W16(PART_LEN_SHIFT, 7);

    // Get log of near end energy and store in buffer

    // Shift buffer
    memmove(aecm->nearLogEnergy + 1, aecm->nearLogEnergy,
            sizeof(WebRtc_Word16) * (MAX_BUF_LEN - 1));

    // Logarithm of integrated magnitude spectrum (nearEner)
    tmp16 = kLogLowValue;
    if (nearEner)
    {
        zeros = WebRtcSpl_NormU32(nearEner);
        frac = (WebRtc_Word16)WEBRTC_SPL_RSHIFT_U32(
                              (WEBRTC_SPL_LSHIFT_U32(nearEner, zeros) & 0x7FFFFFFF),
                              23);
        // log2 in Q8
        tmp16 += WEBRTC_SPL_LSHIFT_W16((31 - zeros), 8) + frac;
        tmp16 -= WEBRTC_SPL_LSHIFT_W16(aecm->dfaNoisyQDomain, 8);
    }
    aecm->nearLogEnergy[0] = tmp16;
    // END: Get log of near end energy

    WebRtcAecm_CalcLinearEnergies(aecm, far_spectrum, echoEst, &tmpFar, &tmpAdapt, &tmpStored);

    // Shift buffers
    memmove(aecm->echoAdaptLogEnergy + 1, aecm->echoAdaptLogEnergy,
            sizeof(WebRtc_Word16) * (MAX_BUF_LEN - 1));
    memmove(aecm->echoStoredLogEnergy + 1, aecm->echoStoredLogEnergy,
            sizeof(WebRtc_Word16) * (MAX_BUF_LEN - 1));

    // Logarithm of delayed far end energy
    tmp16 = kLogLowValue;
    if (tmpFar)
    {
        zeros = WebRtcSpl_NormU32(tmpFar);
        frac = (WebRtc_Word16)WEBRTC_SPL_RSHIFT_U32((WEBRTC_SPL_LSHIFT_U32(tmpFar, zeros)
                        & 0x7FFFFFFF), 23);
        // log2 in Q8
        tmp16 += WEBRTC_SPL_LSHIFT_W16((31 - zeros), 8) + frac;
        tmp16 -= WEBRTC_SPL_LSHIFT_W16(far_q, 8);
    }
    aecm->farLogEnergy = tmp16;

    // Logarithm of estimated echo energy through adapted channel
    tmp16 = kLogLowValue;
    if (tmpAdapt)
    {
        zeros = WebRtcSpl_NormU32(tmpAdapt);
        frac = (WebRtc_Word16)WEBRTC_SPL_RSHIFT_U32((WEBRTC_SPL_LSHIFT_U32(tmpAdapt, zeros)
                        & 0x7FFFFFFF), 23);
        //log2 in Q8
        tmp16 += WEBRTC_SPL_LSHIFT_W16((31 - zeros), 8) + frac;
        tmp16 -= WEBRTC_SPL_LSHIFT_W16(RESOLUTION_CHANNEL16 + far_q, 8);
    }
    aecm->echoAdaptLogEnergy[0] = tmp16;

    // Logarithm of estimated echo energy through stored channel
    tmp16 = kLogLowValue;
    if (tmpStored)
    {
        zeros = WebRtcSpl_NormU32(tmpStored);
        frac = (WebRtc_Word16)WEBRTC_SPL_RSHIFT_U32((WEBRTC_SPL_LSHIFT_U32(tmpStored, zeros)
                        & 0x7FFFFFFF), 23);
        //log2 in Q8
        tmp16 += WEBRTC_SPL_LSHIFT_W16((31 - zeros), 8) + frac;
        tmp16 -= WEBRTC_SPL_LSHIFT_W16(RESOLUTION_CHANNEL16 + far_q, 8);
    }
    aecm->echoStoredLogEnergy[0] = tmp16;

    // Update farend energy levels (min, max, vad, mse)
    if (aecm->farLogEnergy > FAR_ENERGY_MIN)
    {
        if (aecm->startupState == 0)
        {
            increase_max_shifts = 2;
            decrease_min_shifts = 2;
            increase_min_shifts = 8;
        }

        aecm->farEnergyMin = WebRtcAecm_AsymFilt(aecm->farEnergyMin, aecm->farLogEnergy,
                                                 increase_min_shifts, decrease_min_shifts);
        aecm->farEnergyMax = WebRtcAecm_AsymFilt(aecm->farEnergyMax, aecm->farLogEnergy,
                                                 increase_max_shifts, decrease_max_shifts);
        aecm->farEnergyMaxMin = (aecm->farEnergyMax - aecm->farEnergyMin);

        // Dynamic VAD region size
        tmp16 = 2560 - aecm->farEnergyMin;
        if (tmp16 > 0)
        {
            tmp16 = (WebRtc_Word16)WEBRTC_SPL_MUL_16_16_RSFT(tmp16, FAR_ENERGY_VAD_REGION, 9);
        } else
        {
            tmp16 = 0;
        }
        tmp16 += FAR_ENERGY_VAD_REGION;

        if ((aecm->startupState == 0) | (aecm->vadUpdateCount > 1024))
        {
            // In startup phase or VAD update halted
            aecm->farEnergyVAD = aecm->farEnergyMin + tmp16;
        } else
        {
            if (aecm->farEnergyVAD > aecm->farLogEnergy)
            {
                aecm->farEnergyVAD += WEBRTC_SPL_RSHIFT_W16(aecm->farLogEnergy +
                                                            tmp16 -
                                                            aecm->farEnergyVAD,
                                                            6);
                aecm->vadUpdateCount = 0;
            } else
            {
                aecm->vadUpdateCount++;
            }
        }
        // Put MSE threshold higher than VAD
        aecm->farEnergyMSE = aecm->farEnergyVAD + (1 << 8);
    }

    // Update VAD variables
    if (aecm->farLogEnergy > aecm->farEnergyVAD)
    {
        if ((aecm->startupState == 0) | (aecm->farEnergyMaxMin > FAR_ENERGY_DIFF))
        {
            // We are in startup or have significant dynamics in input speech level
            aecm->currentVADValue = 1;
        }
    } else
    {
        aecm->currentVADValue = 0;
    }
    if ((aecm->currentVADValue) && (aecm->firstVAD))
    {
        aecm->firstVAD = 0;
        if (aecm->echoAdaptLogEnergy[0] > aecm->nearLogEnergy[0])
        {
            // The estimated echo has higher energy than the near end signal.
            // This means that the initialization was too aggressive. Scale
            // down by a factor 8
            for (i = 0; i < PART_LEN1; i++)
            {
                aecm->channelAdapt16[i] >>= 3;
            }
            // Compensate the adapted echo energy level accordingly.
            aecm->echoAdaptLogEnergy[0] -= (3 << 8);
            aecm->firstVAD = 1;
        }
    }
}

// WebRtcAecm_CalcStepSize(...)
//
// This function calculates the step size used in channel estimation
//
//
// @param  aecm  [in]    Handle of the AECM instance.
// @param  mu    [out]   (Return value) Stepsize in log2(), i.e. number of shifts.
//
//
WebRtc_Word16 WebRtcAecm_CalcStepSize(AecmCore_t * const aecm)
{

    WebRtc_Word32 tmp32;
    WebRtc_Word16 tmp16;
    WebRtc_Word16 mu = MU_MAX;

    // Here we calculate the step size mu used in the
    // following NLMS based Channel estimation algorithm
    if (!aecm->currentVADValue)
    {
        // Far end energy level too low, no channel update
        mu = 0;
    } else if (aecm->startupState > 0)
    {
        if (aecm->farEnergyMin >= aecm->farEnergyMax)
        {
            mu = MU_MIN;
        } else
        {
            tmp16 = (aecm->farLogEnergy - aecm->farEnergyMin);
            tmp32 = WEBRTC_SPL_MUL_16_16(tmp16, MU_DIFF);
            tmp32 = WebRtcSpl_DivW32W16(tmp32, aecm->farEnergyMaxMin);
            mu = MU_MIN - 1 - (WebRtc_Word16)(tmp32);
            // The -1 is an alternative to rounding. This way we get a larger
            // stepsize, so we in some sense compensate for truncation in NLMS
        }
        if (mu < MU_MAX)
        {
            mu = MU_MAX; // Equivalent with maximum step size of 2^-MU_MAX
        }
    }

    return mu;
}

// WebRtcAecm_UpdateChannel(...)
//
// This function performs channel estimation. NLMS and decision on channel storage.
//
//
// @param  aecm         [i/o]   Handle of the AECM instance.
// @param  far_spectrum [in]    Absolute value of the farend signal in Q(far_q)
// @param  far_q        [in]    Q-domain of the farend signal
// @param  dfa          [in]    Absolute value of the nearend signal (Q[aecm->dfaQDomain])
// @param  mu           [in]    NLMS step size.
// @param  echoEst      [i/o]   Estimated echo in Q(far_q+RESOLUTION_CHANNEL16).
//
void WebRtcAecm_UpdateChannel(AecmCore_t * aecm,
                              const WebRtc_UWord16* far_spectrum,
                              const WebRtc_Word16 far_q,
                              const WebRtc_UWord16 * const dfa,
                              const WebRtc_Word16 mu,
                              WebRtc_Word32 * echoEst)
{

    WebRtc_UWord32 tmpU32no1, tmpU32no2;
    WebRtc_Word32 tmp32no1, tmp32no2;
    WebRtc_Word32 mseStored;
    WebRtc_Word32 mseAdapt;

    int i;

    WebRtc_Word16 zerosFar, zerosNum, zerosCh, zerosDfa;
    WebRtc_Word16 shiftChFar, shiftNum, shift2ResChan;
    WebRtc_Word16 tmp16no1;
    WebRtc_Word16 xfaQ, dfaQ;

    // This is the channel estimation algorithm. It is base on NLMS but has a variable step
    // length, which was calculated above.
    if (mu)
    {
        for (i = 0; i < PART_LEN1; i++)
        {
            // Determine norm of channel and farend to make sure we don't get overflow in
            // multiplication
            zerosCh = WebRtcSpl_NormU32(aecm->channelAdapt32[i]);
            zerosFar = WebRtcSpl_NormU32((WebRtc_UWord32)far_spectrum[i]);
            if (zerosCh + zerosFar > 31)
            {
                // Multiplication is safe
                tmpU32no1 = WEBRTC_SPL_UMUL_32_16(aecm->channelAdapt32[i],
                        far_spectrum[i]);
                shiftChFar = 0;
            } else
            {
                // We need to shift down before multiplication
                shiftChFar = 32 - zerosCh - zerosFar;
                tmpU32no1 = WEBRTC_SPL_UMUL_32_16(
                    WEBRTC_SPL_RSHIFT_W32(aecm->channelAdapt32[i], shiftChFar),
                    far_spectrum[i]);
            }
            // Determine Q-domain of numerator
            zerosNum = WebRtcSpl_NormU32(tmpU32no1);
            if (dfa[i])
            {
                zerosDfa = WebRtcSpl_NormU32((WebRtc_UWord32)dfa[i]);
            } else
            {
                zerosDfa = 32;
            }
            tmp16no1 = zerosDfa - 2 + aecm->dfaNoisyQDomain -
                RESOLUTION_CHANNEL32 - far_q + shiftChFar;
            if (zerosNum > tmp16no1 + 1)
            {
                xfaQ = tmp16no1;
                dfaQ = zerosDfa - 2;
            } else
            {
                xfaQ = zerosNum - 2;
                dfaQ = RESOLUTION_CHANNEL32 + far_q - aecm->dfaNoisyQDomain -
                    shiftChFar + xfaQ;
            }
            // Add in the same Q-domain
            tmpU32no1 = WEBRTC_SPL_SHIFT_W32(tmpU32no1, xfaQ);
            tmpU32no2 = WEBRTC_SPL_SHIFT_W32((WebRtc_UWord32)dfa[i], dfaQ);
            tmp32no1 = (WebRtc_Word32)tmpU32no2 - (WebRtc_Word32)tmpU32no1;
            zerosNum = WebRtcSpl_NormW32(tmp32no1);
            if ((tmp32no1) && (far_spectrum[i] > (CHANNEL_VAD << far_q)))
            {
                //
                // Update is needed
                //
                // This is what we would like to compute
                //
                // tmp32no1 = dfa[i] - (aecm->channelAdapt[i] * far_spectrum[i])
                // tmp32norm = (i + 1)
                // aecm->channelAdapt[i] += (2^mu) * tmp32no1
                //                        / (tmp32norm * far_spectrum[i])
                //

                // Make sure we don't get overflow in multiplication.
                if (zerosNum + zerosFar > 31)
                {
                    if (tmp32no1 > 0)
                    {
                        tmp32no2 = (WebRtc_Word32)WEBRTC_SPL_UMUL_32_16(tmp32no1,
                                                                        far_spectrum[i]);
                    } else
                    {
                        tmp32no2 = -(WebRtc_Word32)WEBRTC_SPL_UMUL_32_16(-tmp32no1,
                                                                         far_spectrum[i]);
                    }
                    shiftNum = 0;
                } else
                {
                    shiftNum = 32 - (zerosNum + zerosFar);
                    if (tmp32no1 > 0)
                    {
                        tmp32no2 = (WebRtc_Word32)WEBRTC_SPL_UMUL_32_16(
                                WEBRTC_SPL_RSHIFT_W32(tmp32no1, shiftNum),
                                far_spectrum[i]);
                    } else
                    {
                        tmp32no2 = -(WebRtc_Word32)WEBRTC_SPL_UMUL_32_16(
                                WEBRTC_SPL_RSHIFT_W32(-tmp32no1, shiftNum),
                                far_spectrum[i]);
                    }
                }
                // Normalize with respect to frequency bin
                tmp32no2 = WebRtcSpl_DivW32W16(tmp32no2, i + 1);
                // Make sure we are in the right Q-domain
                shift2ResChan = shiftNum + shiftChFar - xfaQ - mu - ((30 - zerosFar) << 1);
                if (WebRtcSpl_NormW32(tmp32no2) < shift2ResChan)
                {
                    tmp32no2 = WEBRTC_SPL_WORD32_MAX;
                } else
                {
                    tmp32no2 = WEBRTC_SPL_SHIFT_W32(tmp32no2, shift2ResChan);
                }
                aecm->channelAdapt32[i] = WEBRTC_SPL_ADD_SAT_W32(aecm->channelAdapt32[i],
                        tmp32no2);
                if (aecm->channelAdapt32[i] < 0)
                {
                    // We can never have negative channel gain
                    aecm->channelAdapt32[i] = 0;
                }
                aecm->channelAdapt16[i]
                        = (WebRtc_Word16)WEBRTC_SPL_RSHIFT_W32(aecm->channelAdapt32[i], 16);
            }
        }
    }
    // END: Adaptive channel update

    // Determine if we should store or restore the channel
    if ((aecm->startupState == 0) & (aecm->currentVADValue))
    {
        // During startup we store the channel every block,
        // and we recalculate echo estimate
        WebRtcAecm_StoreAdaptiveChannel(aecm, far_spectrum, echoEst);
    } else
    {
        if (aecm->farLogEnergy < aecm->farEnergyMSE)
        {
            aecm->mseChannelCount = 0;
        } else
        {
            aecm->mseChannelCount++;
        }
        // Enough data for validation. Store channel if we can.
        if (aecm->mseChannelCount >= (MIN_MSE_COUNT + 10))
        {
            // We have enough data.
            // Calculate MSE of "Adapt" and "Stored" versions.
            // It is actually not MSE, but average absolute error.
            mseStored = 0;
            mseAdapt = 0;
            for (i = 0; i < MIN_MSE_COUNT; i++)
            {
                tmp32no1 = ((WebRtc_Word32)aecm->echoStoredLogEnergy[i]
                        - (WebRtc_Word32)aecm->nearLogEnergy[i]);
                tmp32no2 = WEBRTC_SPL_ABS_W32(tmp32no1);
                mseStored += tmp32no2;

                tmp32no1 = ((WebRtc_Word32)aecm->echoAdaptLogEnergy[i]
                        - (WebRtc_Word32)aecm->nearLogEnergy[i]);
                tmp32no2 = WEBRTC_SPL_ABS_W32(tmp32no1);
                mseAdapt += tmp32no2;
            }
            if (((mseStored << MSE_RESOLUTION) < (MIN_MSE_DIFF * mseAdapt))
                    & ((aecm->mseStoredOld << MSE_RESOLUTION) < (MIN_MSE_DIFF
                            * aecm->mseAdaptOld)))
            {
                // The stored channel has a significantly lower MSE than the adaptive one for
                // two consecutive calculations. Reset the adaptive channel.
                WebRtcAecm_ResetAdaptiveChannel(aecm);
            } else if (((MIN_MSE_DIFF * mseStored) > (mseAdapt << MSE_RESOLUTION)) & (mseAdapt
                    < aecm->mseThreshold) & (aecm->mseAdaptOld < aecm->mseThreshold))
            {
                // The adaptive channel has a significantly lower MSE than the stored one.
                // The MSE for the adaptive channel has also been low for two consecutive
                // calculations. Store the adaptive channel.
                WebRtcAecm_StoreAdaptiveChannel(aecm, far_spectrum, echoEst);

                // Update threshold
                if (aecm->mseThreshold == WEBRTC_SPL_WORD32_MAX)
                {
                    aecm->mseThreshold = (mseAdapt + aecm->mseAdaptOld);
                } else
                {
                    aecm->mseThreshold += WEBRTC_SPL_MUL_16_16_RSFT(mseAdapt
                            - WEBRTC_SPL_MUL_16_16_RSFT(aecm->mseThreshold, 5, 3), 205, 8);
                }

            }

            // Reset counter
            aecm->mseChannelCount = 0;

            // Store the MSE values.
            aecm->mseStoredOld = mseStored;
            aecm->mseAdaptOld = mseAdapt;
        }
    }
    // END: Determine if we should store or reset channel estimate.
}

// CalcSuppressionGain(...)
//
// This function calculates the suppression gain that is used in the Wiener filter.
//
//
// @param  aecm     [i/n]   Handle of the AECM instance.
// @param  supGain  [out]   (Return value) Suppression gain with which to scale the noise
//                          level (Q14).
//
//
static WebRtc_Word16 CalcSuppressionGain(AecmCore_t * const aecm)
{
    WebRtc_Word32 tmp32no1;

    WebRtc_Word16 supGain = SUPGAIN_DEFAULT;
    WebRtc_Word16 tmp16no1;
    WebRtc_Word16 dE = 0;

    // Determine suppression gain used in the Wiener filter. The gain is based on a mix of far
    // end energy and echo estimation error.
    // Adjust for the far end signal level. A low signal level indicates no far end signal,
    // hence we set the suppression gain to 0
    if (!aecm->currentVADValue)
    {
        supGain = 0;
    } else
    {
        // Adjust for possible double talk. If we have large variations in estimation error we
        // likely have double talk (or poor channel).
        tmp16no1 = (aecm->nearLogEnergy[0] - aecm->echoStoredLogEnergy[0] - ENERGY_DEV_OFFSET);
        dE = WEBRTC_SPL_ABS_W16(tmp16no1);

        if (dE < ENERGY_DEV_TOL)
        {
            // Likely no double talk. The better estimation, the more we can suppress signal.
            // Update counters
            if (dE < SUPGAIN_EPC_DT)
            {
                tmp32no1 = WEBRTC_SPL_MUL_16_16(aecm->supGainErrParamDiffAB, dE);
                tmp32no1 += (SUPGAIN_EPC_DT >> 1);
                tmp16no1 = (WebRtc_Word16)WebRtcSpl_DivW32W16(tmp32no1, SUPGAIN_EPC_DT);
                supGain = aecm->supGainErrParamA - tmp16no1;
            } else
            {
                tmp32no1 = WEBRTC_SPL_MUL_16_16(aecm->supGainErrParamDiffBD,
                                                (ENERGY_DEV_TOL - dE));
                tmp32no1 += ((ENERGY_DEV_TOL - SUPGAIN_EPC_DT) >> 1);
                tmp16no1 = (WebRtc_Word16)WebRtcSpl_DivW32W16(tmp32no1, (ENERGY_DEV_TOL
                        - SUPGAIN_EPC_DT));
                supGain = aecm->supGainErrParamD + tmp16no1;
            }
        } else
        {
            // Likely in double talk. Use default value
            supGain = aecm->supGainErrParamD;
        }
    }

    if (supGain > aecm->supGainOld)
    {
        tmp16no1 = supGain;
    } else
    {
        tmp16no1 = aecm->supGainOld;
    }
    aecm->supGainOld = supGain;
    if (tmp16no1 < aecm->supGain)
    {
        aecm->supGain += (WebRtc_Word16)((tmp16no1 - aecm->supGain) >> 4);
    } else
    {
        aecm->supGain += (WebRtc_Word16)((tmp16no1 - aecm->supGain) >> 4);
    }

    // END: Update suppression gain

    return aecm->supGain;
}

// Transforms a time domain signal into the frequency domain, outputting the
// complex valued signal, absolute value and sum of absolute values.
//
// time_signal          [in]    Pointer to time domain signal
// freq_signal_real     [out]   Pointer to real part of frequency domain array
// freq_signal_imag     [out]   Pointer to imaginary part of frequency domain
//                              array
// freq_signal_abs      [out]   Pointer to absolute value of frequency domain
//                              array
// freq_signal_sum_abs  [out]   Pointer to the sum of all absolute values in
//                              the frequency domain array
// return value                 The Q-domain of current frequency values
//
static int TimeToFrequencyDomain(const WebRtc_Word16* time_signal,
                                 complex16_t* freq_signal,
                                 WebRtc_UWord16* freq_signal_abs,
                                 WebRtc_UWord32* freq_signal_sum_abs)
{
    int i = 0;
    int time_signal_scaling = 0;

    WebRtc_Word32 tmp32no1;
    WebRtc_Word32 tmp32no2;

    // In fft_buf, +16 for 32-byte alignment.
    WebRtc_Word16 fft_buf[PART_LEN4 + 16];
    WebRtc_Word16 *fft = (WebRtc_Word16 *) (((uintptr_t) fft_buf + 31) & ~31);

    WebRtc_Word16 tmp16no1;
    WebRtc_Word16 tmp16no2;
#ifdef AECM_WITH_ABS_APPROX
    WebRtc_Word16 max_value = 0;
    WebRtc_Word16 min_value = 0;
    WebRtc_UWord16 alpha = 0;
    WebRtc_UWord16 beta = 0;
#endif

#ifdef AECM_DYNAMIC_Q
    tmp16no1 = WebRtcSpl_MaxAbsValueW16(time_signal, PART_LEN2);
    time_signal_scaling = WebRtcSpl_NormW16(tmp16no1);
#endif

    WebRtcAecm_WindowAndFFT(fft, time_signal, freq_signal, time_signal_scaling);

    // Extract imaginary and real part, calculate the magnitude for all frequency bins
    freq_signal[0].imag = 0;
    freq_signal[PART_LEN].imag = 0;
    freq_signal[PART_LEN].real = fft[PART_LEN2];
    freq_signal_abs[0] = (WebRtc_UWord16)WEBRTC_SPL_ABS_W16(
        freq_signal[0].real);
    freq_signal_abs[PART_LEN] = (WebRtc_UWord16)WEBRTC_SPL_ABS_W16(
        freq_signal[PART_LEN].real);
    (*freq_signal_sum_abs) = (WebRtc_UWord32)(freq_signal_abs[0]) +
        (WebRtc_UWord32)(freq_signal_abs[PART_LEN]);

    for (i = 1; i < PART_LEN; i++)
    {
        if (freq_signal[i].real == 0)
        {
            freq_signal_abs[i] = (WebRtc_UWord16)WEBRTC_SPL_ABS_W16(
                freq_signal[i].imag);
        }
        else if (freq_signal[i].imag == 0)
        {
            freq_signal_abs[i] = (WebRtc_UWord16)WEBRTC_SPL_ABS_W16(
                freq_signal[i].real);
        }
        else
        {
            // Approximation for magnitude of complex fft output
            // magn = sqrt(real^2 + imag^2)
            // magn ~= alpha * max(|imag|,|real|) + beta * min(|imag|,|real|)
            //
            // The parameters alpha and beta are stored in Q15

#ifdef AECM_WITH_ABS_APPROX
            tmp16no1 = WEBRTC_SPL_ABS_W16(freq_signal[i].real);
            tmp16no2 = WEBRTC_SPL_ABS_W16(freq_signal[i].imag);

            if(tmp16no1 > tmp16no2)
            {
                max_value = tmp16no1;
                min_value = tmp16no2;
            } else
            {
                max_value = tmp16no2;
                min_value = tmp16no1;
            }

            // Magnitude in Q(-6)
            if ((max_value >> 2) > min_value)
            {
                alpha = kAlpha1;
                beta = kBeta1;
            } else if ((max_value >> 1) > min_value)
            {
                alpha = kAlpha2;
                beta = kBeta2;
            } else
            {
                alpha = kAlpha3;
                beta = kBeta3;
            }
            tmp16no1 = (WebRtc_Word16)WEBRTC_SPL_MUL_16_16_RSFT(max_value,
                                                                alpha,
                                                                15);
            tmp16no2 = (WebRtc_Word16)WEBRTC_SPL_MUL_16_16_RSFT(min_value,
                                                                beta,
                                                                15);
            freq_signal_abs[i] = (WebRtc_UWord16)tmp16no1 +
                (WebRtc_UWord16)tmp16no2;
#else
#ifdef WEBRTC_ARCH_ARM_V7A
            __asm __volatile(
              "smulbb %[tmp32no1], %[real], %[real]\n\t"
              "smlabb %[tmp32no2], %[imag], %[imag], %[tmp32no1]\n\t"
              :[tmp32no1]"=r"(tmp32no1),
               [tmp32no2]"=r"(tmp32no2)
              :[real]"r"(freq_signal[i].real),
               [imag]"r"(freq_signal[i].imag)
            );
#else
            tmp16no1 = WEBRTC_SPL_ABS_W16(freq_signal[i].real);
            tmp16no2 = WEBRTC_SPL_ABS_W16(freq_signal[i].imag);
            tmp32no1 = WEBRTC_SPL_MUL_16_16(tmp16no1, tmp16no1);
            tmp32no2 = WEBRTC_SPL_MUL_16_16(tmp16no2, tmp16no2);
            tmp32no2 = WEBRTC_SPL_ADD_SAT_W32(tmp32no1, tmp32no2);
#endif // WEBRTC_ARCH_ARM_V7A
            tmp32no1 = WebRtcSpl_SqrtFloor(tmp32no2);

            freq_signal_abs[i] = (WebRtc_UWord16)tmp32no1;
#endif // AECM_WITH_ABS_APPROX
        }
        (*freq_signal_sum_abs) += (WebRtc_UWord32)freq_signal_abs[i];
    }

    return time_signal_scaling;
}

int WebRtcAecm_ProcessBlock(AecmCore_t * aecm,
                            const WebRtc_Word16 * farend,
                            const WebRtc_Word16 * nearendNoisy,
                            const WebRtc_Word16 * nearendClean,
                            WebRtc_Word16 * output)
{
    int i;

    WebRtc_UWord32 xfaSum;
    WebRtc_UWord32 dfaNoisySum;
    WebRtc_UWord32 dfaCleanSum;
    WebRtc_UWord32 echoEst32Gained;
    WebRtc_UWord32 tmpU32;

    WebRtc_Word32 tmp32no1;

    WebRtc_UWord16 xfa[PART_LEN1];
    WebRtc_UWord16 dfaNoisy[PART_LEN1];
    WebRtc_UWord16 dfaClean[PART_LEN1];
    WebRtc_UWord16* ptrDfaClean = dfaClean;
    const WebRtc_UWord16* far_spectrum_ptr = NULL;

    // 32 byte aligned buffers (with +8 or +16).
    // TODO (kma): define fft with complex16_t.
    WebRtc_Word16 fft_buf[PART_LEN4 + 2 + 16]; // +2 to make a loop safe.
    WebRtc_Word32 echoEst32_buf[PART_LEN1 + 8];
    WebRtc_Word32 dfw_buf[PART_LEN1 + 8];
    WebRtc_Word32 efw_buf[PART_LEN1 + 8];

    WebRtc_Word16* fft = (WebRtc_Word16*) (((uintptr_t) fft_buf + 31) & ~ 31);
    WebRtc_Word32* echoEst32 = (WebRtc_Word32*) (((uintptr_t) echoEst32_buf + 31) & ~ 31);
    complex16_t* dfw = (complex16_t*) (((uintptr_t) dfw_buf + 31) & ~ 31);
    complex16_t* efw = (complex16_t*) (((uintptr_t) efw_buf + 31) & ~ 31);

    WebRtc_Word16 hnl[PART_LEN1];
    WebRtc_Word16 numPosCoef = 0;
    WebRtc_Word16 nlpGain = ONE_Q14;
    int delay;
    WebRtc_Word16 tmp16no1;
    WebRtc_Word16 tmp16no2;
    WebRtc_Word16 mu;
    WebRtc_Word16 supGain;
    WebRtc_Word16 zeros32, zeros16;
    WebRtc_Word16 zerosDBufNoisy, zerosDBufClean, zerosXBuf;
    int far_q;
    WebRtc_Word16 resolutionDiff, qDomainDiff;

    const int kMinPrefBand = 4;
    const int kMaxPrefBand = 24;
    WebRtc_Word32 avgHnl32 = 0;

#ifdef ARM_WINM_LOG_
    DWORD temp;
    static int flag0 = 0;
    __int64 freq, start, end, diff__;
    unsigned int milliseconds;
#endif

    // Determine startup state. There are three states:
    // (0) the first CONV_LEN blocks
    // (1) another CONV_LEN blocks
    // (2) the rest

    if (aecm->startupState < 2)
    {
        aecm->startupState = (aecm->totCount >= CONV_LEN) + (aecm->totCount >= CONV_LEN2);
    }
    // END: Determine startup state

    // Buffer near and far end signals
    memcpy(aecm->xBuf + PART_LEN, farend, sizeof(WebRtc_Word16) * PART_LEN);
    memcpy(aecm->dBufNoisy + PART_LEN, nearendNoisy, sizeof(WebRtc_Word16) * PART_LEN);
    if (nearendClean != NULL)
    {
        memcpy(aecm->dBufClean + PART_LEN, nearendClean, sizeof(WebRtc_Word16) * PART_LEN);
    }

#ifdef ARM_WINM_LOG_
    // measure tick start
    QueryPerformanceFrequency((LARGE_INTEGER*)&freq);
    QueryPerformanceCounter((LARGE_INTEGER*)&start);
#endif

    // Transform far end signal from time domain to frequency domain.
    far_q = TimeToFrequencyDomain(aecm->xBuf,
                                  dfw,
                                  xfa,
                                  &xfaSum);

    // Transform noisy near end signal from time domain to frequency domain.
    zerosDBufNoisy = TimeToFrequencyDomain(aecm->dBufNoisy,
                                           dfw,
                                           dfaNoisy,
                                           &dfaNoisySum);
    aecm->dfaNoisyQDomainOld = aecm->dfaNoisyQDomain;
    aecm->dfaNoisyQDomain = (WebRtc_Word16)zerosDBufNoisy;


    if (nearendClean == NULL)
    {
        ptrDfaClean = dfaNoisy;
        aecm->dfaCleanQDomainOld = aecm->dfaNoisyQDomainOld;
        aecm->dfaCleanQDomain = aecm->dfaNoisyQDomain;
        dfaCleanSum = dfaNoisySum;
    } else
    {
        // Transform clean near end signal from time domain to frequency domain.
        zerosDBufClean = TimeToFrequencyDomain(aecm->dBufClean,
                                               dfw,
                                               dfaClean,
                                               &dfaCleanSum);
        aecm->dfaCleanQDomainOld = aecm->dfaCleanQDomain;
        aecm->dfaCleanQDomain = (WebRtc_Word16)zerosDBufClean;
    }

#ifdef ARM_WINM_LOG_
    // measure tick end
    QueryPerformanceCounter((LARGE_INTEGER*)&end);
    diff__ = ((end - start) * 1000) / (freq/1000);
    milliseconds = (unsigned int)(diff__ & 0xffffffff);
    WriteFile (logFile, &milliseconds, sizeof(unsigned int), &temp, NULL);
    // measure tick start
    QueryPerformanceCounter((LARGE_INTEGER*)&start);
#endif

    // Get the delay
    // Save far-end history and estimate delay
    UpdateFarHistory(aecm, xfa, far_q);
    delay = WebRtc_DelayEstimatorProcessFix(aecm->delay_estimator,
                                            xfa,
                                            dfaNoisy,
                                            PART_LEN1,
                                            far_q,
                                            zerosDBufNoisy);
    if (delay == -1)
    {
        return -1;
    }
    else if (delay == -2)
    {
        // If the delay is unknown, we assume zero.
        // NOTE: this will have to be adjusted if we ever add lookahead.
        delay = 0;
    }

    if (aecm->fixedDelay >= 0)
    {
        // Use fixed delay
        delay = aecm->fixedDelay;
    }

#ifdef ARM_WINM_LOG_
    // measure tick end
    QueryPerformanceCounter((LARGE_INTEGER*)&end);
    diff__ = ((end - start) * 1000) / (freq/1000);
    milliseconds = (unsigned int)(diff__ & 0xffffffff);
    WriteFile (logFile, &milliseconds, sizeof(unsigned int), &temp, NULL);
    // measure tick start
    QueryPerformanceCounter((LARGE_INTEGER*)&start);
#endif
    // Get aligned far end spectrum
    far_spectrum_ptr = AlignedFarend(aecm, &far_q, delay);
    zerosXBuf = (WebRtc_Word16) far_q;
    if (far_spectrum_ptr == NULL)
    {
        return -1;
    }

    // Calculate log(energy) and update energy threshold levels
    WebRtcAecm_CalcEnergies(aecm,
                            far_spectrum_ptr,
                            zerosXBuf,
                            dfaNoisySum,
                            echoEst32);

    // Calculate stepsize
    mu = WebRtcAecm_CalcStepSize(aecm);

    // Update counters
    aecm->totCount++;

    // This is the channel estimation algorithm.
    // It is base on NLMS but has a variable step length, which was calculated above.
    WebRtcAecm_UpdateChannel(aecm, far_spectrum_ptr, zerosXBuf, dfaNoisy, mu, echoEst32);
    supGain = CalcSuppressionGain(aecm);

#ifdef ARM_WINM_LOG_
    // measure tick end
    QueryPerformanceCounter((LARGE_INTEGER*)&end);
    diff__ = ((end - start) * 1000) / (freq/1000);
    milliseconds = (unsigned int)(diff__ & 0xffffffff);
    WriteFile (logFile, &milliseconds, sizeof(unsigned int), &temp, NULL);
    // measure tick start
    QueryPerformanceCounter((LARGE_INTEGER*)&start);
#endif

    // Calculate Wiener filter hnl[]
    for (i = 0; i < PART_LEN1; i++)
    {
        // Far end signal through channel estimate in Q8
        // How much can we shift right to preserve resolution
        tmp32no1 = echoEst32[i] - aecm->echoFilt[i];
        aecm->echoFilt[i] += WEBRTC_SPL_RSHIFT_W32(WEBRTC_SPL_MUL_32_16(tmp32no1, 50), 8);

        zeros32 = WebRtcSpl_NormW32(aecm->echoFilt[i]) + 1;
        zeros16 = WebRtcSpl_NormW16(supGain) + 1;
        if (zeros32 + zeros16 > 16)
        {
            // Multiplication is safe
            // Result in Q(RESOLUTION_CHANNEL+RESOLUTION_SUPGAIN+aecm->xfaQDomainBuf[diff])
            echoEst32Gained = WEBRTC_SPL_UMUL_32_16((WebRtc_UWord32)aecm->echoFilt[i],
                                                    (WebRtc_UWord16)supGain);
            resolutionDiff = 14 - RESOLUTION_CHANNEL16 - RESOLUTION_SUPGAIN;
            resolutionDiff += (aecm->dfaCleanQDomain - zerosXBuf);
        } else
        {
            tmp16no1 = 17 - zeros32 - zeros16;
            resolutionDiff = 14 + tmp16no1 - RESOLUTION_CHANNEL16 - RESOLUTION_SUPGAIN;
            resolutionDiff += (aecm->dfaCleanQDomain - zerosXBuf);
            if (zeros32 > tmp16no1)
            {
                echoEst32Gained = WEBRTC_SPL_UMUL_32_16((WebRtc_UWord32)aecm->echoFilt[i],
                        (WebRtc_UWord16)WEBRTC_SPL_RSHIFT_W16(supGain,
                                tmp16no1)); // Q-(RESOLUTION_CHANNEL+RESOLUTION_SUPGAIN-16)
            } else
            {
                // Result in Q-(RESOLUTION_CHANNEL+RESOLUTION_SUPGAIN-16)
                echoEst32Gained = WEBRTC_SPL_UMUL_32_16(
                        (WebRtc_UWord32)WEBRTC_SPL_RSHIFT_W32(aecm->echoFilt[i], tmp16no1),
                        (WebRtc_UWord16)supGain);
            }
        }

        zeros16 = WebRtcSpl_NormW16(aecm->nearFilt[i]);
        if ((zeros16 < (aecm->dfaCleanQDomain - aecm->dfaCleanQDomainOld))
                & (aecm->nearFilt[i]))
        {
            tmp16no1 = WEBRTC_SPL_SHIFT_W16(aecm->nearFilt[i], zeros16);
            qDomainDiff = zeros16 - aecm->dfaCleanQDomain + aecm->dfaCleanQDomainOld;
        } else
        {
            tmp16no1 = WEBRTC_SPL_SHIFT_W16(aecm->nearFilt[i],
                                            aecm->dfaCleanQDomain - aecm->dfaCleanQDomainOld);
            qDomainDiff = 0;
        }
        tmp16no2 = WEBRTC_SPL_SHIFT_W16(ptrDfaClean[i], qDomainDiff);
        tmp32no1 = (WebRtc_Word32)(tmp16no2 - tmp16no1);
        tmp16no2 = (WebRtc_Word16)WEBRTC_SPL_RSHIFT_W32(tmp32no1, 4);
        tmp16no2 += tmp16no1;
        zeros16 = WebRtcSpl_NormW16(tmp16no2);
        if ((tmp16no2) & (-qDomainDiff > zeros16))
        {
            aecm->nearFilt[i] = WEBRTC_SPL_WORD16_MAX;
        } else
        {
            aecm->nearFilt[i] = WEBRTC_SPL_SHIFT_W16(tmp16no2, -qDomainDiff);
        }

        // Wiener filter coefficients, resulting hnl in Q14
        if (echoEst32Gained == 0)
        {
            hnl[i] = ONE_Q14;
        } else if (aecm->nearFilt[i] == 0)
        {
            hnl[i] = 0;
        } else
        {
            // Multiply the suppression gain
            // Rounding
            echoEst32Gained += (WebRtc_UWord32)(aecm->nearFilt[i] >> 1);
            tmpU32 = WebRtcSpl_DivU32U16(echoEst32Gained, (WebRtc_UWord16)aecm->nearFilt[i]);

            // Current resolution is
            // Q-(RESOLUTION_CHANNEL + RESOLUTION_SUPGAIN - max(0, 17 - zeros16 - zeros32))
            // Make sure we are in Q14
            tmp32no1 = (WebRtc_Word32)WEBRTC_SPL_SHIFT_W32(tmpU32, resolutionDiff);
            if (tmp32no1 > ONE_Q14)
            {
                hnl[i] = 0;
            } else if (tmp32no1 < 0)
            {
                hnl[i] = ONE_Q14;
            } else
            {
                // 1-echoEst/dfa
                hnl[i] = ONE_Q14 - (WebRtc_Word16)tmp32no1;
                if (hnl[i] < 0)
                {
                    hnl[i] = 0;
                }
            }
        }
        if (hnl[i])
        {
            numPosCoef++;
        }
    }
    // Only in wideband. Prevent the gain in upper band from being larger than
    // in lower band.
    if (aecm->mult == 2)
    {
        // TODO(bjornv): Investigate if the scaling of hnl[i] below can cause
        //               speech distortion in double-talk.
        for (i = 0; i < PART_LEN1; i++)
        {
            hnl[i] = (WebRtc_Word16)WEBRTC_SPL_MUL_16_16_RSFT(hnl[i], hnl[i], 14);
        }

        for (i = kMinPrefBand; i <= kMaxPrefBand; i++)
        {
            avgHnl32 += (WebRtc_Word32)hnl[i];
        }
        assert(kMaxPrefBand - kMinPrefBand + 1 > 0);
        avgHnl32 /= (kMaxPrefBand - kMinPrefBand + 1);

        for (i = kMaxPrefBand; i < PART_LEN1; i++)
        {
            if (hnl[i] > (WebRtc_Word16)avgHnl32)
            {
                hnl[i] = (WebRtc_Word16)avgHnl32;
            }
        }
    }

#ifdef ARM_WINM_LOG_
    // measure tick end
    QueryPerformanceCounter((LARGE_INTEGER*)&end);
    diff__ = ((end - start) * 1000) / (freq/1000);
    milliseconds = (unsigned int)(diff__ & 0xffffffff);
    WriteFile (logFile, &milliseconds, sizeof(unsigned int), &temp, NULL);
    // measure tick start
    QueryPerformanceCounter((LARGE_INTEGER*)&start);
#endif

    // Calculate NLP gain, result is in Q14
    if (aecm->nlpFlag)
    {
        for (i = 0; i < PART_LEN1; i++)
        {
            // Truncate values close to zero and one.
            if (hnl[i] > NLP_COMP_HIGH)
            {
                hnl[i] = ONE_Q14;
            } else if (hnl[i] < NLP_COMP_LOW)
            {
                hnl[i] = 0;
            }
    
            // Remove outliers
            if (numPosCoef < 3)
            {
                nlpGain = 0;
            } else
            {
                nlpGain = ONE_Q14;
            }

            // NLP
            if ((hnl[i] == ONE_Q14) && (nlpGain == ONE_Q14))
            {
                hnl[i] = ONE_Q14;
            } else
            {
                hnl[i] = (WebRtc_Word16)WEBRTC_SPL_MUL_16_16_RSFT(hnl[i], nlpGain, 14);
            }

            // multiply with Wiener coefficients
            efw[i].real = (WebRtc_Word16)(WEBRTC_SPL_MUL_16_16_RSFT_WITH_ROUND(dfw[i].real,
                                                                            hnl[i], 14));
            efw[i].imag = (WebRtc_Word16)(WEBRTC_SPL_MUL_16_16_RSFT_WITH_ROUND(dfw[i].imag,
                                                                            hnl[i], 14));
        }
    }
    else
    {
        // multiply with Wiener coefficients
        for (i = 0; i < PART_LEN1; i++)
        {
            efw[i].real = (WebRtc_Word16)(WEBRTC_SPL_MUL_16_16_RSFT_WITH_ROUND(dfw[i].real,
                                                                           hnl[i], 14));
            efw[i].imag = (WebRtc_Word16)(WEBRTC_SPL_MUL_16_16_RSFT_WITH_ROUND(dfw[i].imag,
                                                                           hnl[i], 14));
        }
    }

    if (aecm->cngMode == AecmTrue)
    {
        ComfortNoise(aecm, ptrDfaClean, efw, hnl);
    }

#ifdef ARM_WINM_LOG_
    // measure tick end
    QueryPerformanceCounter((LARGE_INTEGER*)&end);
    diff__ = ((end - start) * 1000) / (freq/1000);
    milliseconds = (unsigned int)(diff__ & 0xffffffff);
    WriteFile (logFile, &milliseconds, sizeof(unsigned int), &temp, NULL);
    // measure tick start
    QueryPerformanceCounter((LARGE_INTEGER*)&start);
#endif

    WebRtcAecm_InverseFFTAndWindow(aecm, fft, efw, output, nearendClean);

    return 0;
}


// Generate comfort noise and add to output signal.
//
// \param[in]     aecm     Handle of the AECM instance.
// \param[in]     dfa     Absolute value of the nearend signal (Q[aecm->dfaQDomain]).
// \param[in,out] outReal Real part of the output signal (Q[aecm->dfaQDomain]).
// \param[in,out] outImag Imaginary part of the output signal (Q[aecm->dfaQDomain]).
// \param[in]     lambda  Suppression gain with which to scale the noise level (Q14).
//
static void ComfortNoise(AecmCore_t* aecm,
                         const WebRtc_UWord16* dfa,
                         complex16_t* out,
                         const WebRtc_Word16* lambda)
{
    WebRtc_Word16 i;
    WebRtc_Word16 tmp16;
    WebRtc_Word32 tmp32;

    WebRtc_Word16 randW16[PART_LEN];
    WebRtc_Word16 uReal[PART_LEN1];
    WebRtc_Word16 uImag[PART_LEN1];
    WebRtc_Word32 outLShift32;
    WebRtc_Word16 noiseRShift16[PART_LEN1];

    WebRtc_Word16 shiftFromNearToNoise = kNoiseEstQDomain - aecm->dfaCleanQDomain;
    WebRtc_Word16 minTrackShift;

    assert(shiftFromNearToNoise >= 0);
    assert(shiftFromNearToNoise < 16);

    if (aecm->noiseEstCtr < 100)
    {
        // Track the minimum more quickly initially.
        aecm->noiseEstCtr++;
        minTrackShift = 6;
    } else
    {
        minTrackShift = 9;
    }

    // Estimate noise power.
    for (i = 0; i < PART_LEN1; i++)
    {

        // Shift to the noise domain.
        tmp32 = (WebRtc_Word32)dfa[i];
        outLShift32 = WEBRTC_SPL_LSHIFT_W32(tmp32, shiftFromNearToNoise);

        if (outLShift32 < aecm->noiseEst[i])
        {
            // Reset "too low" counter
            aecm->noiseEstTooLowCtr[i] = 0;
            // Track the minimum.
            if (aecm->noiseEst[i] < (1 << minTrackShift))
            {
                // For small values, decrease noiseEst[i] every
                // |kNoiseEstIncCount| block. The regular approach below can not
                // go further down due to truncation.
                aecm->noiseEstTooHighCtr[i]++;
                if (aecm->noiseEstTooHighCtr[i] >= kNoiseEstIncCount)
                {
                    aecm->noiseEst[i]--;
                    aecm->noiseEstTooHighCtr[i] = 0; // Reset the counter
                }
            }
            else
            {
                aecm->noiseEst[i] -= ((aecm->noiseEst[i] - outLShift32) >> minTrackShift);
            }
        } else
        {
            // Reset "too high" counter
            aecm->noiseEstTooHighCtr[i] = 0;
            // Ramp slowly upwards until we hit the minimum again.
            if ((aecm->noiseEst[i] >> 19) > 0)
            {
                // Avoid overflow.
                // Multiplication with 2049 will cause wrap around. Scale
                // down first and then multiply
                aecm->noiseEst[i] >>= 11;
                aecm->noiseEst[i] *= 2049;
            }
            else if ((aecm->noiseEst[i] >> 11) > 0)
            {
                // Large enough for relative increase
                aecm->noiseEst[i] *= 2049;
                aecm->noiseEst[i] >>= 11;
            }
            else
            {
                // Make incremental increases based on size every
                // |kNoiseEstIncCount| block
                aecm->noiseEstTooLowCtr[i]++;
                if (aecm->noiseEstTooLowCtr[i] >= kNoiseEstIncCount)
                {
                    aecm->noiseEst[i] += (aecm->noiseEst[i] >> 9) + 1;
                    aecm->noiseEstTooLowCtr[i] = 0; // Reset counter
                }
            }
        }
    }

    for (i = 0; i < PART_LEN1; i++)
    {
        tmp32 = WEBRTC_SPL_RSHIFT_W32(aecm->noiseEst[i], shiftFromNearToNoise);
        if (tmp32 > 32767)
        {
            tmp32 = 32767;
            aecm->noiseEst[i] = WEBRTC_SPL_LSHIFT_W32(tmp32, shiftFromNearToNoise);
        }
        noiseRShift16[i] = (WebRtc_Word16)tmp32;

        tmp16 = ONE_Q14 - lambda[i];
        noiseRShift16[i]
                = (WebRtc_Word16)WEBRTC_SPL_MUL_16_16_RSFT(tmp16, noiseRShift16[i], 14);
    }

    // Generate a uniform random array on [0 2^15-1].
    WebRtcSpl_RandUArray(randW16, PART_LEN, &aecm->seed);

    // Generate noise according to estimated energy.
    uReal[0] = 0; // Reject LF noise.
    uImag[0] = 0;
    for (i = 1; i < PART_LEN1; i++)
    {
        // Get a random index for the cos and sin tables over [0 359].
        tmp16 = (WebRtc_Word16)WEBRTC_SPL_MUL_16_16_RSFT(359, randW16[i - 1], 15);

        // Tables are in Q13.
        uReal[i] = (WebRtc_Word16)WEBRTC_SPL_MUL_16_16_RSFT(noiseRShift16[i],
                kCosTable[tmp16], 13);
        uImag[i] = (WebRtc_Word16)WEBRTC_SPL_MUL_16_16_RSFT(-noiseRShift16[i],
                kSinTable[tmp16], 13);
    }
    uImag[PART_LEN] = 0;

#if (!defined ARM_WINM) && (!defined ARM9E_GCC) && (!defined ANDROID_AECOPT)
    for (i = 0; i < PART_LEN1; i++)
    {
        out[i].real = WEBRTC_SPL_ADD_SAT_W16(out[i].real, uReal[i]);
        out[i].imag = WEBRTC_SPL_ADD_SAT_W16(out[i].imag, uImag[i]);
    }
#else
    for (i = 0; i < PART_LEN1 -1; )
    {
        out[i].real = WEBRTC_SPL_ADD_SAT_W16(out[i].real, uReal[i]);
        out[i].imag = WEBRTC_SPL_ADD_SAT_W16(out[i].imag, uImag[i]);
        i++;

        out[i].real = WEBRTC_SPL_ADD_SAT_W16(out[i].real, uReal[i]);
        out[i].imag = WEBRTC_SPL_ADD_SAT_W16(out[i].imag, uImag[i]);
        i++;
    }
    out[i].real = WEBRTC_SPL_ADD_SAT_W16(out[i].real, uReal[i]);
    out[i].imag = WEBRTC_SPL_ADD_SAT_W16(out[i].imag, uImag[i]);
#endif
}

void WebRtcAecm_BufferFarFrame(AecmCore_t* const aecm,
                               const WebRtc_Word16* const farend,
                               const int farLen)
{
    int writeLen = farLen, writePos = 0;

    // Check if the write position must be wrapped
    while (aecm->farBufWritePos + writeLen > FAR_BUF_LEN)
    {
        // Write to remaining buffer space before wrapping
        writeLen = FAR_BUF_LEN - aecm->farBufWritePos;
        memcpy(aecm->farBuf + aecm->farBufWritePos, farend + writePos,
               sizeof(WebRtc_Word16) * writeLen);
        aecm->farBufWritePos = 0;
        writePos = writeLen;
        writeLen = farLen - writeLen;
    }

    memcpy(aecm->farBuf + aecm->farBufWritePos, farend + writePos,
           sizeof(WebRtc_Word16) * writeLen);
    aecm->farBufWritePos += writeLen;
}

void WebRtcAecm_FetchFarFrame(AecmCore_t * const aecm, WebRtc_Word16 * const farend,
                              const int farLen, const int knownDelay)
{
    int readLen = farLen;
    int readPos = 0;
    int delayChange = knownDelay - aecm->lastKnownDelay;

    aecm->farBufReadPos -= delayChange;

    // Check if delay forces a read position wrap
    while (aecm->farBufReadPos < 0)
    {
        aecm->farBufReadPos += FAR_BUF_LEN;
    }
    while (aecm->farBufReadPos > FAR_BUF_LEN - 1)
    {
        aecm->farBufReadPos -= FAR_BUF_LEN;
    }

    aecm->lastKnownDelay = knownDelay;

    // Check if read position must be wrapped
    while (aecm->farBufReadPos + readLen > FAR_BUF_LEN)
    {

        // Read from remaining buffer space before wrapping
        readLen = FAR_BUF_LEN - aecm->farBufReadPos;
        memcpy(farend + readPos, aecm->farBuf + aecm->farBufReadPos,
               sizeof(WebRtc_Word16) * readLen);
        aecm->farBufReadPos = 0;
        readPos = readLen;
        readLen = farLen - readLen;
    }
    memcpy(farend + readPos, aecm->farBuf + aecm->farBufReadPos,
           sizeof(WebRtc_Word16) * readLen);
    aecm->farBufReadPos += readLen;
}


