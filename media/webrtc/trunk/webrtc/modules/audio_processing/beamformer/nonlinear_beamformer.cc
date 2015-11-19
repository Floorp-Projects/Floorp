/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#define _USE_MATH_DEFINES

#include "webrtc/modules/audio_processing/beamformer/nonlinear_beamformer.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include "webrtc/base/arraysize.h"
#include "webrtc/common_audio/window_generator.h"
#include "webrtc/modules/audio_processing/beamformer/covariance_matrix_generator.h"

namespace webrtc {
namespace {

// Alpha for the Kaiser Bessel Derived window.
const float kAlpha = 1.5f;

// The minimum value a post-processing mask can take.
const float kMaskMinimum = 0.01f;

const float kSpeedOfSoundMeterSeconds = 343;

// For both target and interference angles, PI / 2 is perpendicular to the
// microphone array, facing forwards. The positive direction goes
// counterclockwise.
// The angle at which we amplify sound.
const float kTargetAngleRadians = static_cast<float>(M_PI) / 2.f;

// The angle at which we suppress sound. Suppression is symmetric around PI / 2
// radians, so sound is suppressed at both +|kInterfAngleRadians| and
// PI - |kInterfAngleRadians|. Since the beamformer is robust, this should
// suppress sound coming from close angles as well.
const float kInterfAngleRadians = static_cast<float>(M_PI) / 4.f;

// When calculating the interference covariance matrix, this is the weight for
// the weighted average between the uniform covariance matrix and the angled
// covariance matrix.
// Rpsi = Rpsi_angled * kBalance + Rpsi_uniform * (1 - kBalance)
const float kBalance = 0.4f;

// TODO(claguna): need comment here.
const float kBeamwidthConstant = 0.00002f;

// Alpha coefficient for mask smoothing.
const float kMaskSmoothAlpha = 0.2f;

// The average mask is computed from masks in this mid-frequency range. If these
// ranges are changed |kMaskQuantile| might need to be adjusted.
const int kLowAverageStartHz = 200;
const int kLowAverageEndHz = 400;

const int kHighAverageStartHz = 6000;
const int kHighAverageEndHz = 6500;

// Quantile of mask values which is used to estimate target presence.
const float kMaskQuantile = 0.3f;
// Mask threshold over which the data is considered signal and not interference.
const float kMaskTargetThreshold = 0.3f;
// Time in seconds after which the data is considered interference if the mask
// does not pass |kMaskTargetThreshold|.
const float kHoldTargetSeconds = 0.25f;

// Does conjugate(|norm_mat|) * |mat| * transpose(|norm_mat|). No extra space is
// used; to accomplish this, we compute both multiplications in the same loop.
// The returned norm is clamped to be non-negative.
float Norm(const ComplexMatrix<float>& mat,
           const ComplexMatrix<float>& norm_mat) {
  CHECK_EQ(norm_mat.num_rows(), 1);
  CHECK_EQ(norm_mat.num_columns(), mat.num_rows());
  CHECK_EQ(norm_mat.num_columns(), mat.num_columns());

  complex<float> first_product = complex<float>(0.f, 0.f);
  complex<float> second_product = complex<float>(0.f, 0.f);

  const complex<float>* const* mat_els = mat.elements();
  const complex<float>* const* norm_mat_els = norm_mat.elements();

  for (int i = 0; i < norm_mat.num_columns(); ++i) {
    for (int j = 0; j < norm_mat.num_columns(); ++j) {
      first_product += conj(norm_mat_els[0][j]) * mat_els[j][i];
    }
    second_product += first_product * norm_mat_els[0][i];
    first_product = 0.f;
  }
  return std::max(second_product.real(), 0.f);
}

// Does conjugate(|lhs|) * |rhs| for row vectors |lhs| and |rhs|.
complex<float> ConjugateDotProduct(const ComplexMatrix<float>& lhs,
                                   const ComplexMatrix<float>& rhs) {
  CHECK_EQ(lhs.num_rows(), 1);
  CHECK_EQ(rhs.num_rows(), 1);
  CHECK_EQ(lhs.num_columns(), rhs.num_columns());

  const complex<float>* const* lhs_elements = lhs.elements();
  const complex<float>* const* rhs_elements = rhs.elements();

  complex<float> result = complex<float>(0.f, 0.f);
  for (int i = 0; i < lhs.num_columns(); ++i) {
    result += conj(lhs_elements[0][i]) * rhs_elements[0][i];
  }

  return result;
}

// Works for positive numbers only.
int Round(float x) {
  return std::floor(x + 0.5f);
}

// Calculates the sum of absolute values of a complex matrix.
float SumAbs(const ComplexMatrix<float>& mat) {
  float sum_abs = 0.f;
  const complex<float>* const* mat_els = mat.elements();
  for (int i = 0; i < mat.num_rows(); ++i) {
    for (int j = 0; j < mat.num_columns(); ++j) {
      sum_abs += std::abs(mat_els[i][j]);
    }
  }
  return sum_abs;
}

// Calculates the sum of squares of a complex matrix.
float SumSquares(const ComplexMatrix<float>& mat) {
  float sum_squares = 0.f;
  const complex<float>* const* mat_els = mat.elements();
  for (int i = 0; i < mat.num_rows(); ++i) {
    for (int j = 0; j < mat.num_columns(); ++j) {
      float abs_value = std::abs(mat_els[i][j]);
      sum_squares += abs_value * abs_value;
    }
  }
  return sum_squares;
}

// Does |out| = |in|.' * conj(|in|) for row vector |in|.
void TransposedConjugatedProduct(const ComplexMatrix<float>& in,
                                 ComplexMatrix<float>* out) {
  CHECK_EQ(in.num_rows(), 1);
  CHECK_EQ(out->num_rows(), in.num_columns());
  CHECK_EQ(out->num_columns(), in.num_columns());
  const complex<float>* in_elements = in.elements()[0];
  complex<float>* const* out_elements = out->elements();
  for (int i = 0; i < out->num_rows(); ++i) {
    for (int j = 0; j < out->num_columns(); ++j) {
      out_elements[i][j] = in_elements[i] * conj(in_elements[j]);
    }
  }
}

std::vector<Point> GetCenteredArray(std::vector<Point> array_geometry) {
  for (int dim = 0; dim < 3; ++dim) {
    float center = 0.f;
    for (size_t i = 0; i < array_geometry.size(); ++i) {
      center += array_geometry[i].c[dim];
    }
    center /= array_geometry.size();
    for (size_t i = 0; i < array_geometry.size(); ++i) {
      array_geometry[i].c[dim] -= center;
    }
  }
  return array_geometry;
}

}  // namespace

NonlinearBeamformer::NonlinearBeamformer(
    const std::vector<Point>& array_geometry)
  : num_input_channels_(array_geometry.size()),
      array_geometry_(GetCenteredArray(array_geometry)) {
  WindowGenerator::KaiserBesselDerived(kAlpha, kFftSize, window_);
}

void NonlinearBeamformer::Initialize(int chunk_size_ms, int sample_rate_hz) {
  chunk_length_ = sample_rate_hz / (1000.f / chunk_size_ms);
  sample_rate_hz_ = sample_rate_hz;
  low_average_start_bin_ =
      Round(kLowAverageStartHz * kFftSize / sample_rate_hz_);
  low_average_end_bin_ =
      Round(kLowAverageEndHz * kFftSize / sample_rate_hz_);
  high_average_start_bin_ =
      Round(kHighAverageStartHz * kFftSize / sample_rate_hz_);
  high_average_end_bin_ =
      Round(kHighAverageEndHz * kFftSize / sample_rate_hz_);
  high_pass_postfilter_mask_ = 1.f;
  is_target_present_ = false;
  hold_target_blocks_ = kHoldTargetSeconds * 2 * sample_rate_hz / kFftSize;
  interference_blocks_count_ = hold_target_blocks_;

  DCHECK_LE(low_average_end_bin_, kNumFreqBins);
  DCHECK_LT(low_average_start_bin_, low_average_end_bin_);
  DCHECK_LE(high_average_end_bin_, kNumFreqBins);
  DCHECK_LT(high_average_start_bin_, high_average_end_bin_);

  lapped_transform_.reset(new LappedTransform(num_input_channels_,
                                              1,
                                              chunk_length_,
                                              window_,
                                              kFftSize,
                                              kFftSize / 2,
                                              this));
  for (int i = 0; i < kNumFreqBins; ++i) {
    postfilter_mask_[i] = 1.f;
    float freq_hz = (static_cast<float>(i) / kFftSize) * sample_rate_hz_;
    wave_numbers_[i] = 2 * M_PI * freq_hz / kSpeedOfSoundMeterSeconds;
    mask_thresholds_[i] = num_input_channels_ * num_input_channels_ *
                          kBeamwidthConstant * wave_numbers_[i] *
                          wave_numbers_[i];
  }

  // Initialize all nonadaptive values before looping through the frames.
  InitDelaySumMasks();
  InitTargetCovMats();
  InitInterfCovMats();

  for (int i = 0; i < kNumFreqBins; ++i) {
    rxiws_[i] = Norm(target_cov_mats_[i], delay_sum_masks_[i]);
    rpsiws_[i] = Norm(interf_cov_mats_[i], delay_sum_masks_[i]);
    reflected_rpsiws_[i] =
        Norm(reflected_interf_cov_mats_[i], delay_sum_masks_[i]);
  }
}

void NonlinearBeamformer::InitDelaySumMasks() {
  for (int f_ix = 0; f_ix < kNumFreqBins; ++f_ix) {
    delay_sum_masks_[f_ix].Resize(1, num_input_channels_);
    CovarianceMatrixGenerator::PhaseAlignmentMasks(f_ix,
                                                   kFftSize,
                                                   sample_rate_hz_,
                                                   kSpeedOfSoundMeterSeconds,
                                                   array_geometry_,
                                                   kTargetAngleRadians,
                                                   &delay_sum_masks_[f_ix]);

    complex_f norm_factor = sqrt(
        ConjugateDotProduct(delay_sum_masks_[f_ix], delay_sum_masks_[f_ix]));
    delay_sum_masks_[f_ix].Scale(1.f / norm_factor);
    normalized_delay_sum_masks_[f_ix].CopyFrom(delay_sum_masks_[f_ix]);
    normalized_delay_sum_masks_[f_ix].Scale(1.f / SumAbs(
        normalized_delay_sum_masks_[f_ix]));
  }
}

void NonlinearBeamformer::InitTargetCovMats() {
  for (int i = 0; i < kNumFreqBins; ++i) {
    target_cov_mats_[i].Resize(num_input_channels_, num_input_channels_);
    TransposedConjugatedProduct(delay_sum_masks_[i], &target_cov_mats_[i]);
    complex_f normalization_factor = target_cov_mats_[i].Trace();
    target_cov_mats_[i].Scale(1.f / normalization_factor);
  }
}

void NonlinearBeamformer::InitInterfCovMats() {
  for (int i = 0; i < kNumFreqBins; ++i) {
    interf_cov_mats_[i].Resize(num_input_channels_, num_input_channels_);
    ComplexMatrixF uniform_cov_mat(num_input_channels_, num_input_channels_);
    ComplexMatrixF angled_cov_mat(num_input_channels_, num_input_channels_);

    CovarianceMatrixGenerator::UniformCovarianceMatrix(wave_numbers_[i],
                                                       array_geometry_,
                                                       &uniform_cov_mat);

    CovarianceMatrixGenerator::AngledCovarianceMatrix(kSpeedOfSoundMeterSeconds,
                                                      kInterfAngleRadians,
                                                      i,
                                                      kFftSize,
                                                      kNumFreqBins,
                                                      sample_rate_hz_,
                                                      array_geometry_,
                                                      &angled_cov_mat);
    // Normalize matrices before averaging them.
    complex_f normalization_factor = uniform_cov_mat.Trace();
    uniform_cov_mat.Scale(1.f / normalization_factor);
    normalization_factor = angled_cov_mat.Trace();
    angled_cov_mat.Scale(1.f / normalization_factor);

    // Average matrices.
    uniform_cov_mat.Scale(1 - kBalance);
    angled_cov_mat.Scale(kBalance);
    interf_cov_mats_[i].Add(uniform_cov_mat, angled_cov_mat);
    reflected_interf_cov_mats_[i].PointwiseConjugate(interf_cov_mats_[i]);
  }
}

void NonlinearBeamformer::ProcessChunk(const ChannelBuffer<float>& input,
                              ChannelBuffer<float>* output) {
  DCHECK_EQ(input.num_channels(), num_input_channels_);
  DCHECK_EQ(input.num_frames_per_band(), chunk_length_);

  float old_high_pass_mask = high_pass_postfilter_mask_;
  lapped_transform_->ProcessChunk(input.channels(0), output->channels(0));
  // Ramp up/down for smoothing. 1 mask per 10ms results in audible
  // discontinuities.
  const float ramp_increment =
      (high_pass_postfilter_mask_ - old_high_pass_mask) /
      input.num_frames_per_band();
  // Apply delay and sum and post-filter in the time domain. WARNING: only works
  // because delay-and-sum is not frequency dependent.
  for (int i = 1; i < input.num_bands(); ++i) {
    float smoothed_mask = old_high_pass_mask;
    for (int j = 0; j < input.num_frames_per_band(); ++j) {
      smoothed_mask += ramp_increment;

      // Applying the delay and sum (at zero degrees, this is equivalent to
      // averaging).
      float sum = 0.f;
      for (int k = 0; k < input.num_channels(); ++k) {
        sum += input.channels(i)[k][j];
      }
      output->channels(i)[0][j] = sum / input.num_channels() * smoothed_mask;
    }
  }
}

void NonlinearBeamformer::ProcessAudioBlock(const complex_f* const* input,
                                   int num_input_channels,
                                   int num_freq_bins,
                                   int num_output_channels,
                                   complex_f* const* output) {
  CHECK_EQ(num_freq_bins, kNumFreqBins);
  CHECK_EQ(num_input_channels, num_input_channels_);
  CHECK_EQ(num_output_channels, 1);

  // Calculating the post-filter masks. Note that we need two for each
  // frequency bin to account for the positive and negative interferer
  // angle.
  for (int i = low_average_start_bin_; i < high_average_end_bin_; ++i) {
    eig_m_.CopyFromColumn(input, i, num_input_channels_);
    float eig_m_norm_factor = std::sqrt(SumSquares(eig_m_));
    if (eig_m_norm_factor != 0.f) {
      eig_m_.Scale(1.f / eig_m_norm_factor);
    }

    float rxim = Norm(target_cov_mats_[i], eig_m_);
    float ratio_rxiw_rxim = 0.f;
    if (rxim > 0.f) {
      ratio_rxiw_rxim = rxiws_[i] / rxim;
    }

    complex_f rmw = abs(ConjugateDotProduct(delay_sum_masks_[i], eig_m_));
    rmw *= rmw;
    float rmw_r = rmw.real();

    new_mask_[i] = CalculatePostfilterMask(interf_cov_mats_[i],
                                           rpsiws_[i],
                                           ratio_rxiw_rxim,
                                           rmw_r,
                                           mask_thresholds_[i]);

    new_mask_[i] *= CalculatePostfilterMask(reflected_interf_cov_mats_[i],
                                            reflected_rpsiws_[i],
                                            ratio_rxiw_rxim,
                                            rmw_r,
                                            mask_thresholds_[i]);
  }

  ApplyMaskSmoothing();
  ApplyLowFrequencyCorrection();
  ApplyHighFrequencyCorrection();
  ApplyMasks(input, output);

  EstimateTargetPresence();
}

float NonlinearBeamformer::CalculatePostfilterMask(
    const ComplexMatrixF& interf_cov_mat,
    float rpsiw,
    float ratio_rxiw_rxim,
    float rmw_r,
    float mask_threshold) {
  float rpsim = Norm(interf_cov_mat, eig_m_);

  // Find lambda.
  float ratio = 0.f;
  if (rpsim > 0.f) {
    ratio = rpsiw / rpsim;
  }
  float numerator = rmw_r - ratio;
  float denominator = ratio_rxiw_rxim - ratio;

  float mask = 1.f;
  if (denominator > mask_threshold) {
    float lambda = numerator / denominator;
    mask = std::max(lambda * ratio_rxiw_rxim / rmw_r, kMaskMinimum);
  }
  return mask;
}

void NonlinearBeamformer::ApplyMasks(const complex_f* const* input,
                            complex_f* const* output) {
  complex_f* output_channel = output[0];
  for (int f_ix = 0; f_ix < kNumFreqBins; ++f_ix) {
    output_channel[f_ix] = complex_f(0.f, 0.f);

    const complex_f* delay_sum_mask_els =
        normalized_delay_sum_masks_[f_ix].elements()[0];
    for (int c_ix = 0; c_ix < num_input_channels_; ++c_ix) {
      output_channel[f_ix] += input[c_ix][f_ix] * delay_sum_mask_els[c_ix];
    }

    output_channel[f_ix] *= postfilter_mask_[f_ix];
  }
}

void NonlinearBeamformer::ApplyMaskSmoothing() {
  for (int i = 0; i < kNumFreqBins; ++i) {
    postfilter_mask_[i] = kMaskSmoothAlpha * new_mask_[i] +
                          (1.f - kMaskSmoothAlpha) * postfilter_mask_[i];
  }
}

void NonlinearBeamformer::ApplyLowFrequencyCorrection() {
  float low_frequency_mask = 0.f;
  for (int i = low_average_start_bin_; i < low_average_end_bin_; ++i) {
    low_frequency_mask += postfilter_mask_[i];
  }

  low_frequency_mask /= low_average_end_bin_ - low_average_start_bin_;

  for (int i = 0; i < low_average_start_bin_; ++i) {
    postfilter_mask_[i] = low_frequency_mask;
  }
}

void NonlinearBeamformer::ApplyHighFrequencyCorrection() {
  high_pass_postfilter_mask_ = 0.f;
  for (int i = high_average_start_bin_; i < high_average_end_bin_; ++i) {
    high_pass_postfilter_mask_ += postfilter_mask_[i];
  }

  high_pass_postfilter_mask_ /= high_average_end_bin_ - high_average_start_bin_;

  for (int i = high_average_end_bin_; i < kNumFreqBins; ++i) {
    postfilter_mask_[i] = high_pass_postfilter_mask_;
  }
}

void NonlinearBeamformer::EstimateTargetPresence() {
  const int quantile = (1.f - kMaskQuantile) * high_average_end_bin_ +
                       kMaskQuantile * low_average_start_bin_;
  std::nth_element(new_mask_ + low_average_start_bin_,
                   new_mask_ + quantile,
                   new_mask_ + high_average_end_bin_);
  if (new_mask_[quantile] > kMaskTargetThreshold) {
    is_target_present_ = true;
    interference_blocks_count_ = 0;
  } else {
    is_target_present_ = interference_blocks_count_++ < hold_target_blocks_;
  }
}

}  // namespace webrtc
