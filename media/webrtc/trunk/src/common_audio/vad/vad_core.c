/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "vad_core.h"

#include "signal_processing_library.h"
#include "typedefs.h"
#include "vad_filterbank.h"
#include "vad_gmm.h"
#include "vad_sp.h"

// Spectrum Weighting
static const int16_t kSpectrumWeight[6] = { 6, 8, 10, 12, 14, 16 };
static const int16_t kNoiseUpdateConst = 655; // Q15
static const int16_t kSpeechUpdateConst = 6554; // Q15
static const int16_t kBackEta = 154; // Q8
// Minimum difference between the two models, Q5
static const int16_t kMinimumDifference[6] = {
    544, 544, 576, 576, 576, 576 };
// Upper limit of mean value for speech model, Q7
static const int16_t kMaximumSpeech[6] = {
    11392, 11392, 11520, 11520, 11520, 11520 };
// Minimum value for mean value
static const int16_t kMinimumMean[2] = { 640, 768 };
// Upper limit of mean value for noise model, Q7
static const int16_t kMaximumNoise[6] = {
    9216, 9088, 8960, 8832, 8704, 8576 };
// Start values for the Gaussian models, Q7
// Weights for the two Gaussians for the six channels (noise)
static const int16_t kNoiseDataWeights[12] = {
    34, 62, 72, 66, 53, 25, 94, 66, 56, 62, 75, 103 };
// Weights for the two Gaussians for the six channels (speech)
static const int16_t kSpeechDataWeights[12] = {
    48, 82, 45, 87, 50, 47, 80, 46, 83, 41, 78, 81 };
// Means for the two Gaussians for the six channels (noise)
static const int16_t kNoiseDataMeans[12] = {
    6738, 4892, 7065, 6715, 6771, 3369, 7646, 3863, 7820, 7266, 5020, 4362 };
// Means for the two Gaussians for the six channels (speech)
static const int16_t kSpeechDataMeans[12] = {
    8306, 10085, 10078, 11823, 11843, 6309, 9473, 9571, 10879, 7581, 8180, 7483
};
// Stds for the two Gaussians for the six channels (noise)
static const int16_t kNoiseDataStds[12] = {
    378, 1064, 493, 582, 688, 593, 474, 697, 475, 688, 421, 455 };
// Stds for the two Gaussians for the six channels (speech)
static const int16_t kSpeechDataStds[12] = {
    555, 505, 567, 524, 585, 1231, 509, 828, 492, 1540, 1079, 850 };

// Constants used in GmmProbability().
//
// Maximum number of counted speech (VAD = 1) frames in a row.
static const int16_t kMaxSpeechFrames = 6;
// Minimum standard deviation for both speech and noise.
static const int16_t kMinStd = 384;

// Constants in WebRtcVad_InitCore().
// Default aggressiveness mode.
static const short kDefaultMode = 0;
static const int kInitCheck = 42;

// Constants used in WebRtcVad_set_mode_core().
//
// Thresholds for different frame lengths (10 ms, 20 ms and 30 ms).
//
// Mode 0, Quality.
static const int16_t kOverHangMax1Q[3] = { 8, 4, 3 };
static const int16_t kOverHangMax2Q[3] = { 14, 7, 5 };
static const int16_t kLocalThresholdQ[3] = { 24, 21, 24 };
static const int16_t kGlobalThresholdQ[3] = { 57, 48, 57 };
// Mode 1, Low bitrate.
static const int16_t kOverHangMax1LBR[3] = { 8, 4, 3 };
static const int16_t kOverHangMax2LBR[3] = { 14, 7, 5 };
static const int16_t kLocalThresholdLBR[3] = { 37, 32, 37 };
static const int16_t kGlobalThresholdLBR[3] = { 100, 80, 100 };
// Mode 2, Aggressive.
static const int16_t kOverHangMax1AGG[3] = { 6, 3, 2 };
static const int16_t kOverHangMax2AGG[3] = { 9, 5, 3 };
static const int16_t kLocalThresholdAGG[3] = { 82, 78, 82 };
static const int16_t kGlobalThresholdAGG[3] = { 285, 260, 285 };
// Mode 3, Very aggressive.
static const int16_t kOverHangMax1VAG[3] = { 6, 3, 2 };
static const int16_t kOverHangMax2VAG[3] = { 9, 5, 3 };
static const int16_t kLocalThresholdVAG[3] = { 94, 94, 94 };
static const int16_t kGlobalThresholdVAG[3] = { 1100, 1050, 1100 };

// Calculates the probabilities for both speech and background noise using
// Gaussian Mixture Models (GMM). A hypothesis-test is performed to decide which
// type of signal is most probable.
//
// - self           [i/o] : Pointer to VAD instance
// - feature_vector [i]   : Feature vector = log10(energy in frequency band)
// - total_power    [i]   : Total power in audio frame.
// - frame_length   [i]   : Number of input samples
//
// - returns              : the VAD decision (0 - noise, 1 - speech).
static int16_t GmmProbability(VadInstT* self, int16_t* feature_vector,
                              int16_t total_power, int frame_length) {
  int n, k;
  int16_t backval;
  int16_t h0, h1;
  int16_t ratvec, xval;
  int16_t vadflag = 0;
  int16_t shifts0, shifts1;
  int16_t tmp16, tmp16_1, tmp16_2;
  int16_t diff;
  int nr, pos;
  int16_t nmk, nmk2, nmk3, smk, smk2, nsk, ssk;
  int16_t delt, ndelt;
  int16_t maxspe, maxmu;
  int16_t deltaN[kTableSize], deltaS[kTableSize];
  int16_t ngprvec[kTableSize], sgprvec[kTableSize];
  int32_t h0test, h1test;
  int32_t tmp32_1, tmp32_2;
  int32_t dotVal;
  int32_t nmid, smid;
  int32_t probn[kNumGaussians], probs[kNumGaussians];
  int16_t *nmean1ptr, *nmean2ptr, *smean1ptr, *smean2ptr;
  int16_t *nstd1ptr, *nstd2ptr, *sstd1ptr, *sstd2ptr;
  int16_t overhead1, overhead2, individualTest, totalTest;

  // Set various thresholds based on frame lengths (80, 160 or 240 samples).
  if (frame_length == 80) {
    overhead1 = self->over_hang_max_1[0];
    overhead2 = self->over_hang_max_2[0];
    individualTest = self->individual[0];
    totalTest = self->total[0];
  } else if (frame_length == 160) {
    overhead1 = self->over_hang_max_1[1];
    overhead2 = self->over_hang_max_2[1];
    individualTest = self->individual[1];
    totalTest = self->total[1];
  } else {
    overhead1 = self->over_hang_max_1[2];
    overhead2 = self->over_hang_max_2[2];
    individualTest = self->individual[2];
    totalTest = self->total[2];
  }

  if (total_power > kMinEnergy) {
    // We have a signal present.
    // Set pointers to the Gaussian parameters.
    nmean1ptr = &self->noise_means[0];
    nmean2ptr = &self->noise_means[kNumChannels];
    smean1ptr = &self->speech_means[0];
    smean2ptr = &self->speech_means[kNumChannels];
    nstd1ptr = &self->noise_stds[0];
    nstd2ptr = &self->noise_stds[kNumChannels];
    sstd1ptr = &self->speech_stds[0];
    sstd2ptr = &self->speech_stds[kNumChannels];

    dotVal = 0;
    for (n = 0; n < kNumChannels; n++) {
      // Perform for all channels.
      pos = (n << 1);
      xval = feature_vector[n];

      // Probability for Noise, Q7 * Q20 = Q27.
      tmp32_1 = WebRtcVad_GaussianProbability(xval, *nmean1ptr++, *nstd1ptr++,
                                              &deltaN[pos]);
      probn[0] = kNoiseDataWeights[n] * tmp32_1;
      tmp32_1 = WebRtcVad_GaussianProbability(xval, *nmean2ptr++, *nstd2ptr++,
                                              &deltaN[pos + 1]);
      probn[1] = kNoiseDataWeights[n + kNumChannels] * tmp32_1;
      h0test = probn[0] + probn[1];  // Q27
      h0 = (int16_t) (h0test >> 12);  // Q15

      // Probability for Speech.
      tmp32_1 = WebRtcVad_GaussianProbability(xval, *smean1ptr++, *sstd1ptr++,
                                              &deltaS[pos]);
      probs[0] = kSpeechDataWeights[n] * tmp32_1;
      tmp32_1 = WebRtcVad_GaussianProbability(xval, *smean2ptr++, *sstd2ptr++,
                                              &deltaS[pos + 1]);
      probs[1] = kSpeechDataWeights[n + kNumChannels] * tmp32_1;
      h1test = probs[0] + probs[1];  // Q27
      h1 = (int16_t) (h1test >> 12);  // Q15

      // Calculate the log likelihood ratio. Approximate log2(H1/H0) with
      // |shifts0| - |shifts1|.
      shifts0 = WebRtcSpl_NormW32(h0test);
      shifts1 = WebRtcSpl_NormW32(h1test);

      if ((h0test > 0) && (h1test > 0)) {
        ratvec = shifts0 - shifts1;
      } else if (h1test > 0) {
        ratvec = 31 - shifts1;
      } else if (h0test > 0) {
        ratvec = shifts0 - 31;
      } else {
        ratvec = 0;
      }

      // VAD decision with spectrum weighting.
      dotVal += WEBRTC_SPL_MUL_16_16(ratvec, kSpectrumWeight[n]);

      // Individual channel test.
      if ((ratvec << 2) > individualTest) {
        vadflag = 1;
      }

      // Probabilities used when updating model.
      if (h0 > 0) {
        tmp32_1 = probn[0] & 0xFFFFF000;  // Q27
        tmp32_2 = (tmp32_1 << 2);  // Q29
        ngprvec[pos] = (int16_t) WebRtcSpl_DivW32W16(tmp32_2, h0);  // Q14
        ngprvec[pos + 1] = 16384 - ngprvec[pos];
      } else {
        ngprvec[pos] = 16384;
        ngprvec[pos + 1] = 0;
      }

      // Probabilities used when updating model.
      if (h1 > 0) {
        tmp32_1 = probs[0] & 0xFFFFF000;
        tmp32_2 = (tmp32_1 << 2);
        sgprvec[pos] = (int16_t) WebRtcSpl_DivW32W16(tmp32_2, h1);
        sgprvec[pos + 1] = 16384 - sgprvec[pos];
      } else {
        sgprvec[pos] = 0;
        sgprvec[pos + 1] = 0;
      }
    }

    // Overall test.
    if (dotVal >= totalTest) {
      vadflag |= 1;
    }

    // Set pointers to the means and standard deviations.
    nmean1ptr = &self->noise_means[0];
    smean1ptr = &self->speech_means[0];
    nstd1ptr = &self->noise_stds[0];
    sstd1ptr = &self->speech_stds[0];

    maxspe = 12800;

    // Update the model parameters.
    for (n = 0; n < kNumChannels; n++) {
      pos = (n << 1);

      // Get minimum value in past which is used for long term correction.
      backval = WebRtcVad_FindMinimum(self, feature_vector[n], n);  // Q4

      // Compute the "global" mean, that is the sum of the two means weighted.
      nmid = WEBRTC_SPL_MUL_16_16(kNoiseDataWeights[n], *nmean1ptr);  // Q7 * Q7
      nmid += WEBRTC_SPL_MUL_16_16(kNoiseDataWeights[n + kNumChannels],
                                   *(nmean1ptr + kNumChannels));
      tmp16_1 = (int16_t) (nmid >> 6);  // Q8

      for (k = 0; k < kNumGaussians; k++) {
        nr = pos + k;

        nmean2ptr = nmean1ptr + k * kNumChannels;
        smean2ptr = smean1ptr + k * kNumChannels;
        nstd2ptr = nstd1ptr + k * kNumChannels;
        sstd2ptr = sstd1ptr + k * kNumChannels;
        nmk = *nmean2ptr;
        smk = *smean2ptr;
        nsk = *nstd2ptr;
        ssk = *sstd2ptr;

        // Update noise mean vector if the frame consists of noise only.
        nmk2 = nmk;
        if (!vadflag) {
          // deltaN = (x-mu)/sigma^2
          // ngprvec[k] = probn[k]/(probn[0] + probn[1])

          // (Q14 * Q11 >> 11) = Q14.
          delt = (int16_t) WEBRTC_SPL_MUL_16_16_RSFT(ngprvec[nr], deltaN[nr],
                                                     11);
          // Q7 + (Q14 * Q15 >> 22) = Q7.
          nmk2 = nmk + (int16_t) WEBRTC_SPL_MUL_16_16_RSFT(delt,
                                                           kNoiseUpdateConst,
                                                           22);
        }

        // Long term correction of the noise mean.
        // Q8 - Q8 = Q8.
        ndelt = (backval << 4) - tmp16_1;
        // Q7 + (Q8 * Q8) >> 9 = Q7.
        nmk3 = nmk2 + (int16_t) WEBRTC_SPL_MUL_16_16_RSFT(ndelt, kBackEta, 9);

        // Control that the noise mean does not drift to much.
        tmp16 = (int16_t) ((k + 5) << 7);
        if (nmk3 < tmp16) {
          nmk3 = tmp16;
        }
        tmp16 = (int16_t) ((72 + k - n) << 7);
        if (nmk3 > tmp16) {
          nmk3 = tmp16;
        }
        *nmean2ptr = nmk3;

        if (vadflag) {
          // Update speech mean vector:
          // |deltaS| = (x-mu)/sigma^2
          // sgprvec[k] = probn[k]/(probn[0] + probn[1])

          // (Q14 * Q11) >> 11 = Q14.
          delt = (int16_t) WEBRTC_SPL_MUL_16_16_RSFT(sgprvec[nr], deltaS[nr],
                                                     11);
          // Q14 * Q15 >> 21 = Q8.
          tmp16 = (int16_t) WEBRTC_SPL_MUL_16_16_RSFT(delt, kSpeechUpdateConst,
                                                      21);
          // Q7 + (Q8 >> 1) = Q7. With rounding.
          smk2 = smk + ((tmp16 + 1) >> 1);

          // Control that the speech mean does not drift to much.
          maxmu = maxspe + 640;
          if (smk2 < kMinimumMean[k]) {
            smk2 = kMinimumMean[k];
          }
          if (smk2 > maxmu) {
            smk2 = maxmu;
          }
          *smean2ptr = smk2;  // Q7.

          // (Q7 >> 3) = Q4. With rounding.
          tmp16 = ((smk + 4) >> 3);

          tmp16 = feature_vector[n] - tmp16;  // Q4
          // (Q11 * Q4 >> 3) = Q12.
          tmp32_1 = WEBRTC_SPL_MUL_16_16_RSFT(deltaS[nr], tmp16, 3);
          tmp32_2 = tmp32_1 - 4096;
          tmp16 = (sgprvec[nr] >> 2);
          // (Q14 >> 2) * Q12 = Q24.
          tmp32_1 = tmp16 * tmp32_2;

          tmp32_2 = (tmp32_1 >> 4);  // Q20

          // 0.1 * Q20 / Q7 = Q13.
          if (tmp32_2 > 0) {
            tmp16 = (int16_t) WebRtcSpl_DivW32W16(tmp32_2, ssk * 10);
          } else {
            tmp16 = (int16_t) WebRtcSpl_DivW32W16(-tmp32_2, ssk * 10);
            tmp16 = -tmp16;
          }
          // Divide by 4 giving an update factor of 0.025 (= 0.1 / 4).
          // Note that division by 4 equals shift by 2, hence,
          // (Q13 >> 8) = (Q13 >> 6) / 4 = Q7.
          tmp16 += 128;  // Rounding.
          ssk += (tmp16 >> 8);
          if (ssk < kMinStd) {
            ssk = kMinStd;
          }
          *sstd2ptr = ssk;
        } else {
          // Update GMM variance vectors.
          // deltaN * (feature_vector[n] - nmk) - 1
          // Q4 - (Q7 >> 3) = Q4.
          tmp16 = feature_vector[n] - (nmk >> 3);
          // (Q11 * Q4 >> 3) = Q12.
          tmp32_1 = WEBRTC_SPL_MUL_16_16_RSFT(deltaN[nr], tmp16, 3) - 4096;

          // (Q14 >> 2) * Q12 = Q24.
          tmp16 = ((ngprvec[nr] + 2) >> 2);
          tmp32_2 = tmp16 * tmp32_1;
          // Q20  * approx 0.001 (2^-10=0.0009766), hence,
          // (Q24 >> 14) = (Q24 >> 4) / 2^10 = Q20.
          tmp32_1 = (tmp32_2 >> 14);

          // Q20 / Q7 = Q13.
          if (tmp32_1 > 0) {
            tmp16 = (int16_t) WebRtcSpl_DivW32W16(tmp32_1, nsk);
          } else {
            tmp16 = (int16_t) WebRtcSpl_DivW32W16(-tmp32_1, nsk);
            tmp16 = -tmp16;
          }
          tmp16 += 32;  // Rounding
          nsk += (tmp16 >> 6);  // Q13 >> 6 = Q7.
          if (nsk < kMinStd) {
            nsk = kMinStd;
          }
          *nstd2ptr = nsk;
        }
      }

      // Separate models if they are too close.
      // |nmid| in Q14 (= Q7 * Q7).
      nmid = WEBRTC_SPL_MUL_16_16(kNoiseDataWeights[n], *nmean1ptr);
      nmid += WEBRTC_SPL_MUL_16_16(kNoiseDataWeights[n + kNumChannels],
                                   *nmean2ptr);

      // |smid| in Q14 (= Q7 * Q7).
      smid = WEBRTC_SPL_MUL_16_16(kSpeechDataWeights[n], *smean1ptr);
      smid += WEBRTC_SPL_MUL_16_16(kSpeechDataWeights[n + kNumChannels],
                                   *smean2ptr);

      // |diff| = "global" speech mean - "global" noise mean.
      // (Q14 >> 9) - (Q14 >> 9) = Q5.
      diff = (int16_t) (smid >> 9) - (int16_t) (nmid >> 9);
      if (diff < kMinimumDifference[n]) {
        tmp16 = kMinimumDifference[n] - diff;

        // |tmp16_1| = ~0.8 * (kMinimumDifference - diff) in Q7.
        // |tmp16_2| = ~0.2 * (kMinimumDifference - diff) in Q7.
        tmp16_1 = (int16_t) WEBRTC_SPL_MUL_16_16_RSFT(13, tmp16, 2);
        tmp16_2 = (int16_t) WEBRTC_SPL_MUL_16_16_RSFT(3, tmp16, 2);

        // First Gaussian, speech model.
        tmp16 = tmp16_1 + *smean1ptr;
        *smean1ptr = tmp16;
        smid = WEBRTC_SPL_MUL_16_16(tmp16, kSpeechDataWeights[n]);

        // Second Gaussian, speech model.
        tmp16 = tmp16_1 + *smean2ptr;
        *smean2ptr = tmp16;
        smid += WEBRTC_SPL_MUL_16_16(tmp16,
                                     kSpeechDataWeights[n + kNumChannels]);

        // First Gaussian, noise model.
        tmp16 = *nmean1ptr - tmp16_2;
        *nmean1ptr = tmp16;
        nmid = WEBRTC_SPL_MUL_16_16(tmp16, kNoiseDataWeights[n]);

        // Second Gaussian, noise model.
        tmp16 = *nmean2ptr - tmp16_2;
        *nmean2ptr = tmp16;
        nmid += WEBRTC_SPL_MUL_16_16(tmp16,
                                     kNoiseDataWeights[n + kNumChannels]);
      }

      // Control that the speech & noise means do not drift to much.
      maxspe = kMaximumSpeech[n];
      tmp16_2 = (int16_t) (smid >> 7);
      if (tmp16_2 > maxspe) {
        // Upper limit of speech model.
        tmp16_2 -= maxspe;

        *smean1ptr -= tmp16_2;
        *smean2ptr -= tmp16_2;
      }

      tmp16_2 = (int16_t) (nmid >> 7);
      if (tmp16_2 > kMaximumNoise[n]) {
        tmp16_2 -= kMaximumNoise[n];

        *nmean1ptr -= tmp16_2;
        *nmean2ptr -= tmp16_2;
      }

      nmean1ptr++;
      smean1ptr++;
      nstd1ptr++;
      sstd1ptr++;
    }
    self->frame_counter++;
  }

  // Smooth with respect to transition hysteresis.
  if (!vadflag) {
    if (self->over_hang > 0) {
      vadflag = 2 + self->over_hang;
      self->over_hang--;
    }
    self->num_of_speech = 0;
  } else {
    self->num_of_speech++;
    if (self->num_of_speech > kMaxSpeechFrames) {
      self->num_of_speech = kMaxSpeechFrames;
      self->over_hang = overhead2;
    } else {
      self->over_hang = overhead1;
    }
  }
  return vadflag;
}

// Initialize the VAD. Set aggressiveness mode to default value.
int WebRtcVad_InitCore(VadInstT* self) {
  int i;

  if (self == NULL) {
    return -1;
  }

  // Initialization of general struct variables.
  self->vad = 1;  // Speech active (=1).
  self->frame_counter = 0;
  self->over_hang = 0;
  self->num_of_speech = 0;

  // Initialization of downsampling filter state.
  memset(self->downsampling_filter_states, 0,
         sizeof(self->downsampling_filter_states));

  // Read initial PDF parameters.
  for (i = 0; i < kTableSize; i++) {
    self->noise_means[i] = kNoiseDataMeans[i];
    self->speech_means[i] = kSpeechDataMeans[i];
    self->noise_stds[i] = kNoiseDataStds[i];
    self->speech_stds[i] = kSpeechDataStds[i];
  }

  // Initialize Index and Minimum value vectors.
  for (i = 0; i < 16 * kNumChannels; i++) {
    self->low_value_vector[i] = 10000;
    self->index_vector[i] = 0;
  }

  // Initialize splitting filter states.
  memset(self->upper_state, 0, sizeof(self->upper_state));
  memset(self->lower_state, 0, sizeof(self->lower_state));

  // Initialize high pass filter states.
  memset(self->hp_filter_state, 0, sizeof(self->hp_filter_state));

  // Initialize mean value memory, for WebRtcVad_FindMinimum().
  for (i = 0; i < kNumChannels; i++) {
    self->mean_value[i] = 1600;
  }

  // Set aggressiveness mode to default (=|kDefaultMode|).
  if (WebRtcVad_set_mode_core(self, kDefaultMode) != 0) {
    return -1;
  }

  self->init_flag = kInitCheck;

  return 0;
}

// Set aggressiveness mode
int WebRtcVad_set_mode_core(VadInstT* self, int mode) {
  int return_value = 0;

  switch (mode) {
    case 0:
      // Quality mode.
      memcpy(self->over_hang_max_1, kOverHangMax1Q,
             sizeof(self->over_hang_max_1));
      memcpy(self->over_hang_max_2, kOverHangMax2Q,
             sizeof(self->over_hang_max_2));
      memcpy(self->individual, kLocalThresholdQ,
             sizeof(self->individual));
      memcpy(self->total, kGlobalThresholdQ,
             sizeof(self->total));
      break;
    case 1:
      // Low bitrate mode.
      memcpy(self->over_hang_max_1, kOverHangMax1LBR,
             sizeof(self->over_hang_max_1));
      memcpy(self->over_hang_max_2, kOverHangMax2LBR,
             sizeof(self->over_hang_max_2));
      memcpy(self->individual, kLocalThresholdLBR,
             sizeof(self->individual));
      memcpy(self->total, kGlobalThresholdLBR,
             sizeof(self->total));
      break;
    case 2:
      // Aggressive mode.
      memcpy(self->over_hang_max_1, kOverHangMax1AGG,
             sizeof(self->over_hang_max_1));
      memcpy(self->over_hang_max_2, kOverHangMax2AGG,
             sizeof(self->over_hang_max_2));
      memcpy(self->individual, kLocalThresholdAGG,
             sizeof(self->individual));
      memcpy(self->total, kGlobalThresholdAGG,
             sizeof(self->total));
      break;
    case 3:
      // Very aggressive mode.
      memcpy(self->over_hang_max_1, kOverHangMax1VAG,
             sizeof(self->over_hang_max_1));
      memcpy(self->over_hang_max_2, kOverHangMax2VAG,
             sizeof(self->over_hang_max_2));
      memcpy(self->individual, kLocalThresholdVAG,
             sizeof(self->individual));
      memcpy(self->total, kGlobalThresholdVAG,
             sizeof(self->total));
      break;
    default:
      return_value = -1;
      break;
  }

  return return_value;
}

// Calculate VAD decision by first extracting feature values and then calculate
// probability for both speech and background noise.

int16_t WebRtcVad_CalcVad32khz(VadInstT* inst, int16_t* speech_frame,
                               int frame_length)
{
    int16_t len, vad;
    int16_t speechWB[480]; // Downsampled speech frame: 960 samples (30ms in SWB)
    int16_t speechNB[240]; // Downsampled speech frame: 480 samples (30ms in WB)


    // Downsample signal 32->16->8 before doing VAD
    WebRtcVad_Downsampling(speech_frame, speechWB, &(inst->downsampling_filter_states[2]),
                           frame_length);
    len = WEBRTC_SPL_RSHIFT_W16(frame_length, 1);

    WebRtcVad_Downsampling(speechWB, speechNB, inst->downsampling_filter_states, len);
    len = WEBRTC_SPL_RSHIFT_W16(len, 1);

    // Do VAD on an 8 kHz signal
    vad = WebRtcVad_CalcVad8khz(inst, speechNB, len);

    return vad;
}

int16_t WebRtcVad_CalcVad16khz(VadInstT* inst, int16_t* speech_frame,
                               int frame_length)
{
    int16_t len, vad;
    int16_t speechNB[240]; // Downsampled speech frame: 480 samples (30ms in WB)

    // Wideband: Downsample signal before doing VAD
    WebRtcVad_Downsampling(speech_frame, speechNB, inst->downsampling_filter_states,
                           frame_length);

    len = WEBRTC_SPL_RSHIFT_W16(frame_length, 1);
    vad = WebRtcVad_CalcVad8khz(inst, speechNB, len);

    return vad;
}

int16_t WebRtcVad_CalcVad8khz(VadInstT* inst, int16_t* speech_frame,
                              int frame_length)
{
    int16_t feature_vector[kNumChannels], total_power;

    // Get power in the bands
    total_power = WebRtcVad_CalculateFeatures(inst, speech_frame, frame_length,
                                              feature_vector);

    // Make a VAD
    inst->vad = GmmProbability(inst, feature_vector, total_power, frame_length);

    return inst->vad;
}
