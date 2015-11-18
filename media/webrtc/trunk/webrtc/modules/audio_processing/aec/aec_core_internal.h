/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_PROCESSING_AEC_AEC_CORE_INTERNAL_H_
#define WEBRTC_MODULES_AUDIO_PROCESSING_AEC_AEC_CORE_INTERNAL_H_

#include "webrtc/common_audio/ring_buffer.h"
#include "webrtc/common_audio/wav_file.h"
#include "webrtc/modules/audio_processing/aec/aec_common.h"
#include "webrtc/modules/audio_processing/aec/aec_core.h"
#include "webrtc/typedefs.h"

// Number of partitions for the extended filter mode. The first one is an enum
// to be used in array declarations, as it represents the maximum filter length.
enum {
  kExtendedNumPartitions = 32
};
static const int kNormalNumPartitions = 12;

// Delay estimator constants, used for logging and delay compensation if
// if reported delays are disabled.
enum {
  kLookaheadBlocks = 15
};
enum {
  // 500 ms for 16 kHz which is equivalent with the limit of reported delays.
  kHistorySizeBlocks = 125
};

// Extended filter adaptation parameters.
// TODO(ajm): No narrowband tuning yet.
static const float kExtendedMu = 0.4f;
static const float kExtendedErrorThreshold = 1.0e-6f;

typedef struct PowerLevel {
  float sfrsum;
  int sfrcounter;
  float framelevel;
  float frsum;
  int frcounter;
  float minlevel;
  float averagelevel;
} PowerLevel;

struct AecCore {
  int farBufWritePos, farBufReadPos;

  int knownDelay;
  int inSamples, outSamples;
  int delayEstCtr;

  RingBuffer* nearFrBuf;
  RingBuffer* outFrBuf;

  RingBuffer* nearFrBufH[NUM_HIGH_BANDS_MAX];
  RingBuffer* outFrBufH[NUM_HIGH_BANDS_MAX];

  float dBuf[PART_LEN2];  // nearend
  float eBuf[PART_LEN2];  // error

  float dBufH[NUM_HIGH_BANDS_MAX][PART_LEN2];  // nearend

  float xPow[PART_LEN1];
  float dPow[PART_LEN1];
  float dMinPow[PART_LEN1];
  float dInitMinPow[PART_LEN1];
  float* noisePow;

  float xfBuf[2][kExtendedNumPartitions * PART_LEN1];  // farend fft buffer
  float wfBuf[2][kExtendedNumPartitions * PART_LEN1];  // filter fft
  complex_t sde[PART_LEN1];  // cross-psd of nearend and error
  complex_t sxd[PART_LEN1];  // cross-psd of farend and nearend
  // Farend windowed fft buffer.
  complex_t xfwBuf[kExtendedNumPartitions * PART_LEN1];

  float sx[PART_LEN1], sd[PART_LEN1], se[PART_LEN1];  // far, near, error psd
  float hNs[PART_LEN1];
  float hNlFbMin, hNlFbLocalMin;
  float hNlXdAvgMin;
  int hNlNewMin, hNlMinCtr;
  float overDrive, overDriveSm;
  int nlp_mode;
  float outBuf[PART_LEN];
  int delayIdx;

  short stNearState, echoState;
  short divergeState;

  int xfBufBlockPos;

  RingBuffer* far_buf;
  RingBuffer* far_buf_windowed;
  int system_delay;  // Current system delay buffered in AEC.

  int mult;  // sampling frequency multiple
  int sampFreq;
  int num_bands;
  uint32_t seed;

  float normal_mu;               // stepsize
  float normal_error_threshold;  // error threshold

  int noiseEstCtr;

  PowerLevel farlevel;
  PowerLevel nearlevel;
  PowerLevel linoutlevel;
  PowerLevel nlpoutlevel;

  int metricsMode;
  int stateCounter;
  Stats erl;
  Stats erle;
  Stats aNlp;
  Stats rerl;

  // Quantities to control H band scaling for SWB input
  int freq_avg_ic;       // initial bin for averaging nlp gain
  int flag_Hband_cn;     // for comfort noise
  float cn_scale_Hband;  // scale for comfort noise in H band

  int delay_metrics_delivered;
  int delay_histogram[kHistorySizeBlocks];
  int num_delay_values;
  int delay_median;
  int delay_std;
  float fraction_poor_delays;
  int delay_logging_enabled;
  void* delay_estimator_farend;
  void* delay_estimator;
  // Variables associated with delay correction through signal based delay
  // estimation feedback.
  int signal_delay_correction;
  int previous_delay;
  int delay_correction_count;
  int shift_offset;
  float delay_quality_threshold;

  // 0 = reported delay mode disabled (signal based delay correction enabled).
  // otherwise enabled
  int reported_delay_enabled;
  // 1 = extended filter mode enabled, 0 = disabled.
  int extended_filter_enabled;
  // Runtime selection of number of filter partitions.
  int num_partitions;

#ifdef WEBRTC_AEC_DEBUG_DUMP
  // Sequence number of this AEC instance, so that different instances can
  // choose different dump file names.
  int instance_index;

  // Number of times we've restarted dumping; used to pick new dump file names
  // each time.
  int debug_dump_count;

  RingBuffer* far_time_buf;
  rtc_WavWriter* farFile;
  rtc_WavWriter* nearFile;
  rtc_WavWriter* outFile;
  rtc_WavWriter* outLinearFile;
#endif
};

typedef void (*WebRtcAecFilterFar)(AecCore* aec, float yf[2][PART_LEN1]);
extern WebRtcAecFilterFar WebRtcAec_FilterFar;
typedef void (*WebRtcAecScaleErrorSignal)(AecCore* aec, float ef[2][PART_LEN1]);
extern WebRtcAecScaleErrorSignal WebRtcAec_ScaleErrorSignal;
typedef void (*WebRtcAecFilterAdaptation)(AecCore* aec,
                                          float* fft,
                                          float ef[2][PART_LEN1]);
extern WebRtcAecFilterAdaptation WebRtcAec_FilterAdaptation;
typedef void (*WebRtcAecOverdriveAndSuppress)(AecCore* aec,
                                              float hNl[PART_LEN1],
                                              const float hNlFb,
                                              float efw[2][PART_LEN1]);
extern WebRtcAecOverdriveAndSuppress WebRtcAec_OverdriveAndSuppress;

typedef void (*WebRtcAecComfortNoise)(AecCore* aec,
                                      float efw[2][PART_LEN1],
                                      complex_t* comfortNoiseHband,
                                      const float* noisePow,
                                      const float* lambda);
extern WebRtcAecComfortNoise WebRtcAec_ComfortNoise;

typedef void (*WebRtcAecSubBandCoherence)(AecCore* aec,
                                          float efw[2][PART_LEN1],
                                          float xfw[2][PART_LEN1],
                                          float* fft,
                                          float* cohde,
                                          float* cohxd);
extern WebRtcAecSubBandCoherence WebRtcAec_SubbandCoherence;

#endif  // WEBRTC_MODULES_AUDIO_PROCESSING_AEC_AEC_CORE_INTERNAL_H_
