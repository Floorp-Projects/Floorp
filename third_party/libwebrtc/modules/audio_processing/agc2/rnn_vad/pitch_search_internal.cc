/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/agc2/rnn_vad/pitch_search_internal.h"

#include <stdlib.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <numeric>

#include "modules/audio_processing/agc2/rnn_vad/common.h"
#include "rtc_base/checks.h"
#include "rtc_base/numerics/safe_compare.h"

namespace webrtc {
namespace rnn_vad {
namespace {

constexpr int kMaxPitch24kHzInt = static_cast<int>(kMaxPitch24kHz);

// Converts a lag to an inverted lag (only for 24kHz).
int GetInvertedLag(int lag) {
  RTC_DCHECK_LE(lag, kMaxPitch24kHzInt);
  return kMaxPitch24kHzInt - lag;
}

float ComputeAutoCorrelationCoeff(rtc::ArrayView<const float> pitch_buf,
                                  int inv_lag,
                                  int max_pitch_period) {
  RTC_DCHECK_LT(inv_lag, static_cast<int>(pitch_buf.size()));
  RTC_DCHECK_LT(max_pitch_period, static_cast<int>(pitch_buf.size()));
  RTC_DCHECK_LE(inv_lag, static_cast<int>(max_pitch_period));
  // TODO(bugs.webrtc.org/9076): Maybe optimize using vectorization.
  return std::inner_product(
      pitch_buf.begin() + static_cast<size_t>(max_pitch_period),
      pitch_buf.end(), pitch_buf.begin() + static_cast<size_t>(inv_lag), 0.f);
}

// Given the auto-correlation coefficients for a lag and its neighbors, computes
// a pseudo-interpolation offset to be applied to the pitch period associated to
// the central auto-correlation coefficient |lag_auto_corr|. The output is a lag
// in {-1, 0, +1}.
// TODO(bugs.webrtc.org/9076): Consider removing pseudo-i since it
// is relevant only if the spectral analysis works at a sample rate that is
// twice as that of the pitch buffer (not so important instead for the estimated
// pitch period feature fed into the RNN).
int GetPitchPseudoInterpolationOffset(float prev_auto_corr,
                                      float lag_auto_corr,
                                      float next_auto_corr) {
  const float& a = prev_auto_corr;
  const float& b = lag_auto_corr;
  const float& c = next_auto_corr;

  int offset = 0;
  if ((c - a) > 0.7f * (b - a)) {
    offset = 1;  // |c| is the largest auto-correlation coefficient.
  } else if ((a - c) > 0.7f * (b - c)) {
    offset = -1;  // |a| is the largest auto-correlation coefficient.
  }
  return offset;
}

// Refines a pitch period |lag| encoded as lag with pseudo-interpolation. The
// output sample rate is twice as that of |lag|.
int PitchPseudoInterpolationLagPitchBuf(
    int lag,
    rtc::ArrayView<const float, kBufSize24kHz> pitch_buf) {
  int offset = 0;
  // Cannot apply pseudo-interpolation at the boundaries.
  if (lag > 0 && lag < kMaxPitch24kHzInt) {
    offset = GetPitchPseudoInterpolationOffset(
        ComputeAutoCorrelationCoeff(pitch_buf, GetInvertedLag(lag - 1),
                                    kMaxPitch24kHzInt),
        ComputeAutoCorrelationCoeff(pitch_buf, GetInvertedLag(lag),
                                    kMaxPitch24kHzInt),
        ComputeAutoCorrelationCoeff(pitch_buf, GetInvertedLag(lag + 1),
                                    kMaxPitch24kHzInt));
  }
  return 2 * lag + offset;
}

// Refines a pitch period |inv_lag| encoded as inverted lag with
// pseudo-interpolation. The output sample rate is twice as that of
// |inv_lag|.
int PitchPseudoInterpolationInvLagAutoCorr(
    int inv_lag,
    rtc::ArrayView<const float> auto_corr) {
  int offset = 0;
  // Cannot apply pseudo-interpolation at the boundaries.
  if (inv_lag > 0 && inv_lag < static_cast<int>(auto_corr.size()) - 1) {
    offset = GetPitchPseudoInterpolationOffset(
        auto_corr[inv_lag + 1], auto_corr[inv_lag], auto_corr[inv_lag - 1]);
  }
  // TODO(bugs.webrtc.org/9076): When retraining, check if |offset| below should
  // be subtracted since |inv_lag| is an inverted lag but offset is a lag.
  return 2 * inv_lag + offset;
}

// Integer multipliers used in CheckLowerPitchPeriodsAndComputePitchGain() when
// looking for sub-harmonics.
// The values have been chosen to serve the following algorithm. Given the
// initial pitch period T, we examine whether one of its harmonics is the true
// fundamental frequency. We consider T/k with k in {2, ..., 15}. For each of
// these harmonics, in addition to the pitch gain of itself, we choose one
// multiple of its pitch period, n*T/k, to validate it (by averaging their pitch
// gains). The multiplier n is chosen so that n*T/k is used only one time over
// all k. When for example k = 4, we should also expect a peak at 3*T/4. When
// k = 8 instead we don't want to look at 2*T/8, since we have already checked
// T/4 before. Instead, we look at T*3/8.
// The array can be generate in Python as follows:
//   from fractions import Fraction
//   # Smallest positive integer not in X.
//   def mex(X):
//     for i in range(1, int(max(X)+2)):
//       if i not in X:
//         return i
//   # Visited multiples of the period.
//   S = {1}
//   for n in range(2, 16):
//     sn = mex({n * i for i in S} | {1})
//     S = S | {Fraction(1, n), Fraction(sn, n)}
//     print(sn, end=', ')
constexpr std::array<int, 14> kSubHarmonicMultipliers = {
    {3, 2, 3, 2, 5, 2, 3, 2, 3, 2, 5, 2, 3, 2}};

// Initial pitch period candidate thresholds for ComputePitchGainThreshold() for
// a sample rate of 24 kHz. Computed as [5*k*k for k in range(16)].
constexpr std::array<int, 14> kInitialPitchPeriodThresholds = {
    {20, 45, 80, 125, 180, 245, 320, 405, 500, 605, 720, 845, 980, 1125}};

}  // namespace

void Decimate2x(rtc::ArrayView<const float, kBufSize24kHz> src,
                rtc::ArrayView<float, kBufSize12kHz> dst) {
  // TODO(bugs.webrtc.org/9076): Consider adding anti-aliasing filter.
  static_assert(2 * dst.size() == src.size(), "");
  for (size_t i = 0; i < dst.size(); ++i) {
    dst[i] = src[2 * i];
  }
}

float ComputePitchGainThreshold(int candidate_pitch_period,
                                int pitch_period_ratio,
                                int initial_pitch_period,
                                float initial_pitch_gain,
                                int prev_pitch_period,
                                float prev_pitch_gain) {
  // Map arguments to more compact aliases.
  const int& t1 = candidate_pitch_period;
  const int& k = pitch_period_ratio;
  const int& t0 = initial_pitch_period;
  const float& g0 = initial_pitch_gain;
  const int& t_prev = prev_pitch_period;
  const float& g_prev = prev_pitch_gain;

  // Validate input.
  RTC_DCHECK_GE(t1, 0);
  RTC_DCHECK_GE(k, 2);
  RTC_DCHECK_GE(t0, 0);
  RTC_DCHECK_GE(t_prev, 0);

  // Compute a term that lowers the threshold when |t1| is close to the last
  // estimated period |t_prev| - i.e., pitch tracking.
  float lower_threshold_term = 0;
  if (abs(t1 - t_prev) <= 1) {
    // The candidate pitch period is within 1 sample from the previous one.
    // Make the candidate at |t1| very easy to be accepted.
    lower_threshold_term = g_prev;
  } else if (abs(t1 - t_prev) == 2 &&
             t0 > kInitialPitchPeriodThresholds[k - 2]) {
    // The candidate pitch period is 2 samples far from the previous one and the
    // period |t0| (from which |t1| has been derived) is greater than a
    // threshold. Make |t1| easy to be accepted.
    lower_threshold_term = 0.5f * g_prev;
  }
  // Set the threshold based on the gain of the initial estimate |t0|. Also
  // reduce the chance of false positives caused by a bias towards high
  // frequencies (originating from short-term correlations).
  float threshold = std::max(0.3f, 0.7f * g0 - lower_threshold_term);
  if (static_cast<size_t>(t1) < 3 * kMinPitch24kHz) {
    // High frequency.
    threshold = std::max(0.4f, 0.85f * g0 - lower_threshold_term);
  } else if (static_cast<size_t>(t1) < 2 * kMinPitch24kHz) {
    // Even higher frequency.
    threshold = std::max(0.5f, 0.9f * g0 - lower_threshold_term);
  }
  return threshold;
}

void ComputeSlidingFrameSquareEnergies(
    rtc::ArrayView<const float, kBufSize24kHz> pitch_buf,
    rtc::ArrayView<float, kMaxPitch24kHz + 1> yy_values) {
  float yy = ComputeAutoCorrelationCoeff(pitch_buf, kMaxPitch24kHzInt,
                                         kMaxPitch24kHzInt);
  yy_values[0] = yy;
  for (size_t i = 1; i < yy_values.size(); ++i) {
    RTC_DCHECK_LE(i, kMaxPitch24kHz + kFrameSize20ms24kHz);
    RTC_DCHECK_LE(i, kMaxPitch24kHz);
    const float old_coeff = pitch_buf[kMaxPitch24kHz + kFrameSize20ms24kHz - i];
    const float new_coeff = pitch_buf[kMaxPitch24kHz - i];
    yy -= old_coeff * old_coeff;
    yy += new_coeff * new_coeff;
    yy = std::max(0.f, yy);
    yy_values[i] = yy;
  }
}

CandidatePitchPeriods FindBestPitchPeriods(
    rtc::ArrayView<const float> auto_corr,
    rtc::ArrayView<const float> pitch_buf,
    int max_pitch_period) {
  // Stores a pitch candidate period and strength information.
  struct PitchCandidate {
    // Pitch period encoded as inverted lag.
    int period_inverted_lag = 0;
    // Pitch strength encoded as a ratio.
    float strength_numerator = -1.f;
    float strength_denominator = 0.f;
    // Compare the strength of two pitch candidates.
    bool HasStrongerPitchThan(const PitchCandidate& b) const {
      // Comparing the numerator/denominator ratios without using divisions.
      return strength_numerator * b.strength_denominator >
             b.strength_numerator * strength_denominator;
    }
  };

  RTC_DCHECK_GT(max_pitch_period, static_cast<int>(auto_corr.size()));
  RTC_DCHECK_LT(max_pitch_period, static_cast<int>(pitch_buf.size()));
  const int frame_size = static_cast<int>(pitch_buf.size()) - max_pitch_period;
  RTC_DCHECK_GT(frame_size, 0);
  // TODO(bugs.webrtc.org/9076): Maybe optimize using vectorization.
  float yy =
      std::inner_product(pitch_buf.begin(), pitch_buf.begin() + frame_size + 1,
                         pitch_buf.begin(), 1.f);
  // Search best and second best pitches by looking at the scaled
  // auto-correlation.
  PitchCandidate candidate;
  PitchCandidate best;
  PitchCandidate second_best;
  second_best.period_inverted_lag = 1;
  for (int inv_lag = 0; inv_lag < static_cast<int>(auto_corr.size());
       ++inv_lag) {
    // A pitch candidate must have positive correlation.
    if (auto_corr[inv_lag] > 0) {
      candidate.period_inverted_lag = inv_lag;
      candidate.strength_numerator = auto_corr[inv_lag] * auto_corr[inv_lag];
      candidate.strength_denominator = yy;
      if (candidate.HasStrongerPitchThan(second_best)) {
        if (candidate.HasStrongerPitchThan(best)) {
          second_best = best;
          best = candidate;
        } else {
          second_best = candidate;
        }
      }
    }
    // Update |squared_energy_y| for the next inverted lag.
    const float old_coeff = pitch_buf[inv_lag];
    const float new_coeff = pitch_buf[inv_lag + frame_size];
    yy -= old_coeff * old_coeff;
    yy += new_coeff * new_coeff;
    yy = std::max(0.f, yy);
  }
  return {best.period_inverted_lag, second_best.period_inverted_lag};
}

int RefinePitchPeriod48kHz(
    rtc::ArrayView<const float, kBufSize24kHz> pitch_buf,
    CandidatePitchPeriods pitch_candidates_inverted_lags) {
  // Compute the auto-correlation terms only for neighbors of the given pitch
  // candidates (similar to what is done in ComputePitchAutoCorrelation(), but
  // for a few lag values).
  std::array<float, kNumInvertedLags24kHz> auto_correlation;
  auto_correlation.fill(
      0.f);  // Zeros become ignored lags in FindBestPitchPeriods().
  auto is_neighbor = [](int i, int j) {
    return ((i > j) ? (i - j) : (j - i)) <= 2;
  };
  // TODO(https://crbug.com/webrtc/10480): Optimize by removing the loop.
  for (int inverted_lag = 0; rtc::SafeLt(inverted_lag, auto_correlation.size());
       ++inverted_lag) {
    if (is_neighbor(inverted_lag, pitch_candidates_inverted_lags.best) ||
        is_neighbor(inverted_lag, pitch_candidates_inverted_lags.second_best))
      auto_correlation[inverted_lag] = ComputeAutoCorrelationCoeff(
          pitch_buf, inverted_lag, kMaxPitch24kHzInt);
  }
  // Find best pitch at 24 kHz.
  const CandidatePitchPeriods pitch_candidates_24kHz =
      FindBestPitchPeriods(auto_correlation, pitch_buf, kMaxPitch24kHzInt);
  // Pseudo-interpolation.
  return PitchPseudoInterpolationInvLagAutoCorr(pitch_candidates_24kHz.best,
                                                auto_correlation);
}

PitchInfo CheckLowerPitchPeriodsAndComputePitchGain(
    rtc::ArrayView<const float, kBufSize24kHz> pitch_buf,
    int initial_pitch_period_48kHz,
    PitchInfo prev_pitch_48kHz) {
  RTC_DCHECK_LE(kMinPitch48kHz, initial_pitch_period_48kHz);
  RTC_DCHECK_LE(initial_pitch_period_48kHz, kMaxPitch48kHz);
  // Stores information for a refined pitch candidate.
  struct RefinedPitchCandidate {
    RefinedPitchCandidate() {}
    RefinedPitchCandidate(int period_24kHz, float gain, float xy, float yy)
        : period_24kHz(period_24kHz), gain(gain), xy(xy), yy(yy) {}
    int period_24kHz;
    // Pitch strength information.
    float gain;
    // Additional pitch strength information used for the final estimation of
    // pitch gain.
    float xy;  // Cross-correlation.
    float yy;  // Auto-correlation.
  };

  // Initialize.
  std::array<float, kMaxPitch24kHz + 1> yy_values;
  ComputeSlidingFrameSquareEnergies(pitch_buf,
                                    {yy_values.data(), yy_values.size()});
  const float xx = yy_values[0];
  // Helper lambdas.
  const auto pitch_gain = [](float xy, float yy, float xx) {
    RTC_DCHECK_LE(0.f, xx * yy);
    return xy / std::sqrt(1.f + xx * yy);
  };
  // Initial pitch candidate gain.
  RefinedPitchCandidate best_pitch;
  best_pitch.period_24kHz =
      std::min(initial_pitch_period_48kHz / 2, kMaxPitch24kHzInt - 1);
  best_pitch.xy = ComputeAutoCorrelationCoeff(
      pitch_buf, GetInvertedLag(best_pitch.period_24kHz), kMaxPitch24kHzInt);
  best_pitch.yy = yy_values[best_pitch.period_24kHz];
  best_pitch.gain = pitch_gain(best_pitch.xy, best_pitch.yy, xx);

  // Store the initial pitch period information.
  const int initial_pitch_period = best_pitch.period_24kHz;
  const float initial_pitch_gain = best_pitch.gain;

  // Given the initial pitch estimation, check lower periods (i.e., harmonics).
  const auto alternative_period = [](int period, int k, int n) -> int {
    RTC_DCHECK_GT(k, 0);
    return (2 * n * period + k) / (2 * k);  // Same as round(n*period/k).
  };
  // |max_k| such that alternative_period(initial_pitch_period, max_k, 1) equals
  // kMinPitch24kHz.
  const int max_k =
      (2 * initial_pitch_period) / (2 * static_cast<int>(kMinPitch24kHz) - 1);
  for (int k = 2; k <= max_k; ++k) {
    int candidate_pitch_period = alternative_period(initial_pitch_period, k, 1);
    RTC_DCHECK_GE(candidate_pitch_period, static_cast<int>(kMinPitch24kHz));
    // When looking at |candidate_pitch_period|, we also look at one of its
    // sub-harmonics. |kSubHarmonicMultipliers| is used to know where to look.
    // |k| == 2 is a special case since |candidate_pitch_secondary_period| might
    // be greater than the maximum pitch period.
    int candidate_pitch_secondary_period = alternative_period(
        initial_pitch_period, k, kSubHarmonicMultipliers[k - 2]);
    RTC_DCHECK_GT(candidate_pitch_secondary_period, 0);
    if (k == 2 && candidate_pitch_secondary_period > kMaxPitch24kHzInt) {
      candidate_pitch_secondary_period = initial_pitch_period;
    }
    RTC_DCHECK_NE(candidate_pitch_period, candidate_pitch_secondary_period)
        << "The lower pitch period and the additional sub-harmonic must not "
           "coincide.";
    // Compute an auto-correlation score for the primary pitch candidate
    // |candidate_pitch_period| by also looking at its possible sub-harmonic
    // |candidate_pitch_secondary_period|.
    float xy_primary_period = ComputeAutoCorrelationCoeff(
        pitch_buf, GetInvertedLag(candidate_pitch_period), kMaxPitch24kHzInt);
    float xy_secondary_period = ComputeAutoCorrelationCoeff(
        pitch_buf, GetInvertedLag(candidate_pitch_secondary_period),
        kMaxPitch24kHzInt);
    float xy = 0.5f * (xy_primary_period + xy_secondary_period);
    float yy = 0.5f * (yy_values[candidate_pitch_period] +
                       yy_values[candidate_pitch_secondary_period]);
    float candidate_pitch_gain = pitch_gain(xy, yy, xx);

    // Maybe update best period.
    float threshold = ComputePitchGainThreshold(
        candidate_pitch_period, k, initial_pitch_period, initial_pitch_gain,
        prev_pitch_48kHz.period / 2, prev_pitch_48kHz.gain);
    if (candidate_pitch_gain > threshold) {
      best_pitch = {candidate_pitch_period, candidate_pitch_gain, xy, yy};
    }
  }

  // Final pitch gain and period.
  best_pitch.xy = std::max(0.f, best_pitch.xy);
  RTC_DCHECK_LE(0.f, best_pitch.yy);
  float final_pitch_gain = (best_pitch.yy <= best_pitch.xy)
                               ? 1.f
                               : best_pitch.xy / (best_pitch.yy + 1.f);
  final_pitch_gain = std::min(best_pitch.gain, final_pitch_gain);
  int final_pitch_period_48kHz = std::max(
      static_cast<int>(kMinPitch48kHz),
      PitchPseudoInterpolationLagPitchBuf(best_pitch.period_24kHz, pitch_buf));

  return {final_pitch_period_48kHz, final_pitch_gain};
}

}  // namespace rnn_vad
}  // namespace webrtc
