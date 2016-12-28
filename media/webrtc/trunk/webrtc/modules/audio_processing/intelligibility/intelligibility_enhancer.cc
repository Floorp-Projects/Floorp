/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

//
//  Implements core class for intelligibility enhancer.
//
//  Details of the model and algorithm can be found in the original paper:
//  http://ieeexplore.ieee.org/stamp/stamp.jsp?tp=&arnumber=6882788
//

#include "webrtc/modules/audio_processing/intelligibility/intelligibility_enhancer.h"

#include <math.h>
#include <stdlib.h>
#include <algorithm>
#include <numeric>

#include "webrtc/base/checks.h"
#include "webrtc/common_audio/include/audio_util.h"
#include "webrtc/common_audio/window_generator.h"

namespace webrtc {

namespace {

const size_t kErbResolution = 2;
const int kWindowSizeMs = 2;
const int kChunkSizeMs = 10;  // Size provided by APM.
const float kClipFreq = 200.0f;
const float kConfigRho = 0.02f;  // Default production and interpretation SNR.
const float kKbdAlpha = 1.5f;
const float kLambdaBot = -1.0f;      // Extreme values in bisection
const float kLambdaTop = -10e-18f;  // search for lamda.

}  // namespace

using std::complex;
using std::max;
using std::min;
using VarianceType = intelligibility::VarianceArray::StepType;

IntelligibilityEnhancer::TransformCallback::TransformCallback(
    IntelligibilityEnhancer* parent,
    IntelligibilityEnhancer::AudioSource source)
    : parent_(parent), source_(source) {
}

void IntelligibilityEnhancer::TransformCallback::ProcessAudioBlock(
    const complex<float>* const* in_block,
    size_t in_channels,
    size_t frames,
    size_t /* out_channels */,
    complex<float>* const* out_block) {
  RTC_DCHECK_EQ(parent_->freqs_, frames);
  for (size_t i = 0; i < in_channels; ++i) {
    parent_->DispatchAudio(source_, in_block[i], out_block[i]);
  }
}

IntelligibilityEnhancer::IntelligibilityEnhancer()
    : IntelligibilityEnhancer(IntelligibilityEnhancer::Config()) {
}

IntelligibilityEnhancer::IntelligibilityEnhancer(const Config& config)
    : freqs_(RealFourier::ComplexLength(
          RealFourier::FftOrder(config.sample_rate_hz * kWindowSizeMs / 1000))),
      window_size_(static_cast<size_t>(1 << RealFourier::FftOrder(freqs_))),
      chunk_length_(
          static_cast<size_t>(config.sample_rate_hz * kChunkSizeMs / 1000)),
      bank_size_(GetBankSize(config.sample_rate_hz, kErbResolution)),
      sample_rate_hz_(config.sample_rate_hz),
      erb_resolution_(kErbResolution),
      num_capture_channels_(config.num_capture_channels),
      num_render_channels_(config.num_render_channels),
      analysis_rate_(config.analysis_rate),
      active_(true),
      clear_variance_(freqs_,
                      config.var_type,
                      config.var_window_size,
                      config.var_decay_rate),
      noise_variance_(freqs_,
                      config.var_type,
                      config.var_window_size,
                      config.var_decay_rate),
      filtered_clear_var_(new float[bank_size_]),
      filtered_noise_var_(new float[bank_size_]),
      filter_bank_(bank_size_),
      center_freqs_(new float[bank_size_]),
      rho_(new float[bank_size_]),
      gains_eq_(new float[bank_size_]),
      gain_applier_(freqs_, config.gain_change_limit),
      temp_render_out_buffer_(chunk_length_, num_render_channels_),
      temp_capture_out_buffer_(chunk_length_, num_capture_channels_),
      kbd_window_(new float[window_size_]),
      render_callback_(this, AudioSource::kRenderStream),
      capture_callback_(this, AudioSource::kCaptureStream),
      block_count_(0),
      analysis_step_(0) {
  RTC_DCHECK_LE(config.rho, 1.0f);

  CreateErbBank();

  // Assumes all rho equal.
  for (size_t i = 0; i < bank_size_; ++i) {
    rho_[i] = config.rho * config.rho;
  }

  float freqs_khz = kClipFreq / 1000.0f;
  size_t erb_index = static_cast<size_t>(ceilf(
      11.17f * logf((freqs_khz + 0.312f) / (freqs_khz + 14.6575f)) + 43.0f));
  start_freq_ = std::max(static_cast<size_t>(1), erb_index * erb_resolution_);

  WindowGenerator::KaiserBesselDerived(kKbdAlpha, window_size_,
                                       kbd_window_.get());
  render_mangler_.reset(new LappedTransform(
      num_render_channels_, num_render_channels_, chunk_length_,
      kbd_window_.get(), window_size_, window_size_ / 2, &render_callback_));
  capture_mangler_.reset(new LappedTransform(
      num_capture_channels_, num_capture_channels_, chunk_length_,
      kbd_window_.get(), window_size_, window_size_ / 2, &capture_callback_));
}

void IntelligibilityEnhancer::ProcessRenderAudio(float* const* audio,
                                                 int sample_rate_hz,
                                                 size_t num_channels) {
  RTC_CHECK_EQ(sample_rate_hz_, sample_rate_hz);
  RTC_CHECK_EQ(num_render_channels_, num_channels);

  if (active_) {
    render_mangler_->ProcessChunk(audio, temp_render_out_buffer_.channels());
  }

  if (active_) {
    for (size_t i = 0; i < num_render_channels_; ++i) {
      memcpy(audio[i], temp_render_out_buffer_.channels()[i],
             chunk_length_ * sizeof(**audio));
    }
  }
}

void IntelligibilityEnhancer::AnalyzeCaptureAudio(float* const* audio,
                                                  int sample_rate_hz,
                                                  size_t num_channels) {
  RTC_CHECK_EQ(sample_rate_hz_, sample_rate_hz);
  RTC_CHECK_EQ(num_capture_channels_, num_channels);

  capture_mangler_->ProcessChunk(audio, temp_capture_out_buffer_.channels());
}

void IntelligibilityEnhancer::DispatchAudio(
    IntelligibilityEnhancer::AudioSource source,
    const complex<float>* in_block,
    complex<float>* out_block) {
  switch (source) {
    case kRenderStream:
      ProcessClearBlock(in_block, out_block);
      break;
    case kCaptureStream:
      ProcessNoiseBlock(in_block, out_block);
      break;
  }
}

void IntelligibilityEnhancer::ProcessClearBlock(const complex<float>* in_block,
                                                complex<float>* out_block) {
  if (block_count_ < 2) {
    memset(out_block, 0, freqs_ * sizeof(*out_block));
    ++block_count_;
    return;
  }

  // TODO(ekm): Use VAD to |Step| and |AnalyzeClearBlock| only if necessary.
  if (true) {
    clear_variance_.Step(in_block, false);
    if (block_count_ % analysis_rate_ == analysis_rate_ - 1) {
      const float power_target = std::accumulate(
          clear_variance_.variance(), clear_variance_.variance() + freqs_, 0.f);
      AnalyzeClearBlock(power_target);
      ++analysis_step_;
    }
    ++block_count_;
  }

  if (active_) {
    gain_applier_.Apply(in_block, out_block);
  }
}

void IntelligibilityEnhancer::AnalyzeClearBlock(float power_target) {
  FilterVariance(clear_variance_.variance(), filtered_clear_var_.get());
  FilterVariance(noise_variance_.variance(), filtered_noise_var_.get());

  SolveForGainsGivenLambda(kLambdaTop, start_freq_, gains_eq_.get());
  const float power_top =
      DotProduct(gains_eq_.get(), filtered_clear_var_.get(), bank_size_);
  SolveForGainsGivenLambda(kLambdaBot, start_freq_, gains_eq_.get());
  const float power_bot =
      DotProduct(gains_eq_.get(), filtered_clear_var_.get(), bank_size_);
  if (power_target >= power_bot && power_target <= power_top) {
    SolveForLambda(power_target, power_bot, power_top);
    UpdateErbGains();
  }  // Else experiencing variance underflow, so do nothing.
}

void IntelligibilityEnhancer::SolveForLambda(float power_target,
                                             float power_bot,
                                             float power_top) {
  const float kConvergeThresh = 0.001f;  // TODO(ekmeyerson): Find best values
  const int kMaxIters = 100;             // for these, based on experiments.

  const float reciprocal_power_target = 1.f / power_target;
  float lambda_bot = kLambdaBot;
  float lambda_top = kLambdaTop;
  float power_ratio = 2.0f;  // Ratio of achieved power to target power.
  int iters = 0;
  while (std::fabs(power_ratio - 1.0f) > kConvergeThresh &&
         iters <= kMaxIters) {
    const float lambda = lambda_bot + (lambda_top - lambda_bot) / 2.0f;
    SolveForGainsGivenLambda(lambda, start_freq_, gains_eq_.get());
    const float power =
        DotProduct(gains_eq_.get(), filtered_clear_var_.get(), bank_size_);
    if (power < power_target) {
      lambda_bot = lambda;
    } else {
      lambda_top = lambda;
    }
    power_ratio = std::fabs(power * reciprocal_power_target);
    ++iters;
  }
}

void IntelligibilityEnhancer::UpdateErbGains() {
  // (ERB gain) = filterbank' * (freq gain)
  float* gains = gain_applier_.target();
  for (size_t i = 0; i < freqs_; ++i) {
    gains[i] = 0.0f;
    for (size_t j = 0; j < bank_size_; ++j) {
      gains[i] = fmaf(filter_bank_[j][i], gains_eq_[j], gains[i]);
    }
  }
}

void IntelligibilityEnhancer::ProcessNoiseBlock(const complex<float>* in_block,
                                                complex<float>* /*out_block*/) {
  noise_variance_.Step(in_block);
}

size_t IntelligibilityEnhancer::GetBankSize(int sample_rate,
                                            size_t erb_resolution) {
  float freq_limit = sample_rate / 2000.0f;
  size_t erb_scale = static_cast<size_t>(ceilf(
      11.17f * logf((freq_limit + 0.312f) / (freq_limit + 14.6575f)) + 43.0f));
  return erb_scale * erb_resolution;
}

void IntelligibilityEnhancer::CreateErbBank() {
  size_t lf = 1, rf = 4;

  for (size_t i = 0; i < bank_size_; ++i) {
    float abs_temp = fabsf((i + 1.0f) / static_cast<float>(erb_resolution_));
    center_freqs_[i] = 676170.4f / (47.06538f - expf(0.08950404f * abs_temp));
    center_freqs_[i] -= 14678.49f;
  }
  float last_center_freq = center_freqs_[bank_size_ - 1];
  for (size_t i = 0; i < bank_size_; ++i) {
    center_freqs_[i] *= 0.5f * sample_rate_hz_ / last_center_freq;
  }

  for (size_t i = 0; i < bank_size_; ++i) {
    filter_bank_[i].resize(freqs_);
  }

  for (size_t i = 1; i <= bank_size_; ++i) {
    size_t lll, ll, rr, rrr;
    static const size_t kOne = 1;  // Avoids repeated static_cast<>s below.
    lll = static_cast<size_t>(round(
        center_freqs_[max(kOne, i - lf) - 1] * freqs_ /
            (0.5f * sample_rate_hz_)));
    ll = static_cast<size_t>(round(
        center_freqs_[max(kOne, i) - 1] * freqs_ / (0.5f * sample_rate_hz_)));
    lll = min(freqs_, max(lll, kOne)) - 1;
    ll = min(freqs_, max(ll, kOne)) - 1;

    rrr = static_cast<size_t>(round(
        center_freqs_[min(bank_size_, i + rf) - 1] * freqs_ /
            (0.5f * sample_rate_hz_)));
    rr = static_cast<size_t>(round(
        center_freqs_[min(bank_size_, i + 1) - 1] * freqs_ /
            (0.5f * sample_rate_hz_)));
    rrr = min(freqs_, max(rrr, kOne)) - 1;
    rr = min(freqs_, max(rr, kOne)) - 1;

    float step, element;

    step = 1.0f / (ll - lll);
    element = 0.0f;
    for (size_t j = lll; j <= ll; ++j) {
      filter_bank_[i - 1][j] = element;
      element += step;
    }
    step = 1.0f / (rrr - rr);
    element = 1.0f;
    for (size_t j = rr; j <= rrr; ++j) {
      filter_bank_[i - 1][j] = element;
      element -= step;
    }
    for (size_t j = ll; j <= rr; ++j) {
      filter_bank_[i - 1][j] = 1.0f;
    }
  }

  float sum;
  for (size_t i = 0; i < freqs_; ++i) {
    sum = 0.0f;
    for (size_t j = 0; j < bank_size_; ++j) {
      sum += filter_bank_[j][i];
    }
    for (size_t j = 0; j < bank_size_; ++j) {
      filter_bank_[j][i] /= sum;
    }
  }
}

void IntelligibilityEnhancer::SolveForGainsGivenLambda(float lambda,
                                                       size_t start_freq,
                                                       float* sols) {
  bool quadratic = (kConfigRho < 1.0f);
  const float* var_x0 = filtered_clear_var_.get();
  const float* var_n0 = filtered_noise_var_.get();

  for (size_t n = 0; n < start_freq; ++n) {
    sols[n] = 1.0f;
  }

  // Analytic solution for optimal gains. See paper for derivation.
  for (size_t n = start_freq - 1; n < bank_size_; ++n) {
    float alpha0, beta0, gamma0;
    gamma0 = 0.5f * rho_[n] * var_x0[n] * var_n0[n] +
             lambda * var_x0[n] * var_n0[n] * var_n0[n];
    beta0 = lambda * var_x0[n] * (2 - rho_[n]) * var_x0[n] * var_n0[n];
    if (quadratic) {
      alpha0 = lambda * var_x0[n] * (1 - rho_[n]) * var_x0[n] * var_x0[n];
      sols[n] =
          (-beta0 - sqrtf(beta0 * beta0 - 4 * alpha0 * gamma0)) / (2 * alpha0);
    } else {
      sols[n] = -gamma0 / beta0;
    }
    sols[n] = fmax(0, sols[n]);
  }
}

void IntelligibilityEnhancer::FilterVariance(const float* var, float* result) {
  RTC_DCHECK_GT(freqs_, 0u);
  for (size_t i = 0; i < bank_size_; ++i) {
    result[i] = DotProduct(&filter_bank_[i][0], var, freqs_);
  }
}

float IntelligibilityEnhancer::DotProduct(const float* a,
                                          const float* b,
                                          size_t length) {
  float ret = 0.0f;

  for (size_t i = 0; i < length; ++i) {
    ret = fmaf(a[i], b[i], ret);
  }
  return ret;
}

bool IntelligibilityEnhancer::active() const {
  return active_;
}

}  // namespace webrtc
