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
//  Implements helper functions and classes for intelligibility enhancement.
//

#include "webrtc/modules/audio_processing/intelligibility/intelligibility_utils.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <algorithm>

using std::complex;
using std::min;

namespace webrtc {

namespace intelligibility {

float UpdateFactor(float target, float current, float limit) {
  float delta = fabsf(target - current);
  float sign = copysign(1.0f, target - current);
  return current + sign * fminf(delta, limit);
}

float AddDitherIfZero(float value) {
  return value == 0.f ? std::rand() * 0.01f / RAND_MAX : value;
}

complex<float> zerofudge(complex<float> c) {
  return complex<float>(AddDitherIfZero(c.real()), AddDitherIfZero(c.imag()));
}

complex<float> NewMean(complex<float> mean, complex<float> data, size_t count) {
  return mean + (data - mean) / static_cast<float>(count);
}

void AddToMean(complex<float> data, size_t count, complex<float>* mean) {
  (*mean) = NewMean(*mean, data, count);
}


static const size_t kWindowBlockSize = 10;

VarianceArray::VarianceArray(size_t num_freqs,
                             StepType type,
                             size_t window_size,
                             float decay)
    : running_mean_(new complex<float>[num_freqs]()),
      running_mean_sq_(new complex<float>[num_freqs]()),
      sub_running_mean_(new complex<float>[num_freqs]()),
      sub_running_mean_sq_(new complex<float>[num_freqs]()),
      variance_(new float[num_freqs]()),
      conj_sum_(new float[num_freqs]()),
      num_freqs_(num_freqs),
      window_size_(window_size),
      decay_(decay),
      history_cursor_(0),
      count_(0),
      array_mean_(0.0f),
      buffer_full_(false) {
  history_.reset(new rtc::scoped_ptr<complex<float>[]>[num_freqs_]());
  for (size_t i = 0; i < num_freqs_; ++i) {
    history_[i].reset(new complex<float>[window_size_]());
  }
  subhistory_.reset(new rtc::scoped_ptr<complex<float>[]>[num_freqs_]());
  for (size_t i = 0; i < num_freqs_; ++i) {
    subhistory_[i].reset(new complex<float>[window_size_]());
  }
  subhistory_sq_.reset(new rtc::scoped_ptr<complex<float>[]>[num_freqs_]());
  for (size_t i = 0; i < num_freqs_; ++i) {
    subhistory_sq_[i].reset(new complex<float>[window_size_]());
  }
  switch (type) {
    case kStepInfinite:
      step_func_ = &VarianceArray::InfiniteStep;
      break;
    case kStepDecaying:
      step_func_ = &VarianceArray::DecayStep;
      break;
    case kStepWindowed:
      step_func_ = &VarianceArray::WindowedStep;
      break;
    case kStepBlocked:
      step_func_ = &VarianceArray::BlockedStep;
      break;
    case kStepBlockBasedMovingAverage:
      step_func_ = &VarianceArray::BlockBasedMovingAverage;
      break;
  }
}

// Compute the variance with Welford's algorithm, adding some fudge to
// the input in case of all-zeroes.
void VarianceArray::InfiniteStep(const complex<float>* data, bool skip_fudge) {
  array_mean_ = 0.0f;
  ++count_;
  for (size_t i = 0; i < num_freqs_; ++i) {
    complex<float> sample = data[i];
    if (!skip_fudge) {
      sample = zerofudge(sample);
    }
    if (count_ == 1) {
      running_mean_[i] = sample;
      variance_[i] = 0.0f;
    } else {
      float old_sum = conj_sum_[i];
      complex<float> old_mean = running_mean_[i];
      running_mean_[i] =
          old_mean + (sample - old_mean) / static_cast<float>(count_);
      conj_sum_[i] =
          (old_sum + std::conj(sample - old_mean) * (sample - running_mean_[i]))
              .real();
      variance_[i] =
          conj_sum_[i] / (count_ - 1);
    }
    array_mean_ += (variance_[i] - array_mean_) / (i + 1);
  }
}

// Compute the variance from the beginning, with exponential decaying of the
// series data.
void VarianceArray::DecayStep(const complex<float>* data, bool /*dummy*/) {
  array_mean_ = 0.0f;
  ++count_;
  for (size_t i = 0; i < num_freqs_; ++i) {
    complex<float> sample = data[i];
    sample = zerofudge(sample);

    if (count_ == 1) {
      running_mean_[i] = sample;
      running_mean_sq_[i] = sample * std::conj(sample);
      variance_[i] = 0.0f;
    } else {
      complex<float> prev = running_mean_[i];
      complex<float> prev2 = running_mean_sq_[i];
      running_mean_[i] = decay_ * prev + (1.0f - decay_) * sample;
      running_mean_sq_[i] =
          decay_ * prev2 + (1.0f - decay_) * sample * std::conj(sample);
      variance_[i] = (running_mean_sq_[i] -
                      running_mean_[i] * std::conj(running_mean_[i])).real();
    }

    array_mean_ += (variance_[i] - array_mean_) / (i + 1);
  }
}

// Windowed variance computation. On each step, the variances for the
// window are recomputed from scratch, using Welford's algorithm.
void VarianceArray::WindowedStep(const complex<float>* data, bool /*dummy*/) {
  size_t num = min(count_ + 1, window_size_);
  array_mean_ = 0.0f;
  for (size_t i = 0; i < num_freqs_; ++i) {
    complex<float> mean;
    float conj_sum = 0.0f;

    history_[i][history_cursor_] = data[i];

    mean = history_[i][history_cursor_];
    variance_[i] = 0.0f;
    for (size_t j = 1; j < num; ++j) {
      complex<float> sample =
          zerofudge(history_[i][(history_cursor_ + j) % window_size_]);
      sample = history_[i][(history_cursor_ + j) % window_size_];
      float old_sum = conj_sum;
      complex<float> old_mean = mean;

      mean = old_mean + (sample - old_mean) / static_cast<float>(j + 1);
      conj_sum =
          (old_sum + std::conj(sample - old_mean) * (sample - mean)).real();
      variance_[i] = conj_sum / (j);
    }
    array_mean_ += (variance_[i] - array_mean_) / (i + 1);
  }
  history_cursor_ = (history_cursor_ + 1) % window_size_;
  ++count_;
}

// Variance with a window of blocks. Within each block, the variances are
// recomputed from scratch at every stp, using |Var(X) = E(X^2) - E^2(X)|.
// Once a block is filled with kWindowBlockSize samples, it is added to the
// history window and a new block is started. The variances for the window
// are recomputed from scratch at each of these transitions.
void VarianceArray::BlockedStep(const complex<float>* data, bool /*dummy*/) {
  size_t blocks = min(window_size_, history_cursor_ + 1);
  for (size_t i = 0; i < num_freqs_; ++i) {
    AddToMean(data[i], count_ + 1, &sub_running_mean_[i]);
    AddToMean(data[i] * std::conj(data[i]), count_ + 1,
              &sub_running_mean_sq_[i]);
    subhistory_[i][history_cursor_ % window_size_] = sub_running_mean_[i];
    subhistory_sq_[i][history_cursor_ % window_size_] = sub_running_mean_sq_[i];

    variance_[i] =
        (NewMean(running_mean_sq_[i], sub_running_mean_sq_[i], blocks) -
         NewMean(running_mean_[i], sub_running_mean_[i], blocks) *
             std::conj(NewMean(running_mean_[i], sub_running_mean_[i], blocks)))
            .real();
    if (count_ == kWindowBlockSize - 1) {
      sub_running_mean_[i] = complex<float>(0.0f, 0.0f);
      sub_running_mean_sq_[i] = complex<float>(0.0f, 0.0f);
      running_mean_[i] = complex<float>(0.0f, 0.0f);
      running_mean_sq_[i] = complex<float>(0.0f, 0.0f);
      for (size_t j = 0; j < min(window_size_, history_cursor_); ++j) {
        AddToMean(subhistory_[i][j], j + 1, &running_mean_[i]);
        AddToMean(subhistory_sq_[i][j], j + 1, &running_mean_sq_[i]);
      }
      ++history_cursor_;
    }
  }
  ++count_;
  if (count_ == kWindowBlockSize) {
    count_ = 0;
  }
}

// Recomputes variances for each window from scratch based on previous window.
void VarianceArray::BlockBasedMovingAverage(const std::complex<float>* data,
                                            bool /*dummy*/) {
  // TODO(ekmeyerson) To mitigate potential divergence, add counter so that
  // after every so often sums are computed scratch by summing over all
  // elements instead of subtracting oldest and adding newest.
  for (size_t i = 0; i < num_freqs_; ++i) {
    sub_running_mean_[i] += data[i];
    sub_running_mean_sq_[i] += data[i] * std::conj(data[i]);
  }
  ++count_;

  // TODO(ekmeyerson) Make kWindowBlockSize nonconstant to allow
  // experimentation with different block size,window size pairs.
  if (count_ >= kWindowBlockSize) {
    count_ = 0;

    for (size_t i = 0; i < num_freqs_; ++i) {
      running_mean_[i] -= subhistory_[i][history_cursor_];
      running_mean_sq_[i] -= subhistory_sq_[i][history_cursor_];

      float scale = 1.f / kWindowBlockSize;
      subhistory_[i][history_cursor_] = sub_running_mean_[i] * scale;
      subhistory_sq_[i][history_cursor_] = sub_running_mean_sq_[i] * scale;

      sub_running_mean_[i] = std::complex<float>(0.0f, 0.0f);
      sub_running_mean_sq_[i] = std::complex<float>(0.0f, 0.0f);

      running_mean_[i] += subhistory_[i][history_cursor_];
      running_mean_sq_[i] += subhistory_sq_[i][history_cursor_];

      scale = 1.f / (buffer_full_ ? window_size_ : history_cursor_ + 1);
      variance_[i] = std::real(running_mean_sq_[i] * scale -
                               running_mean_[i] * scale *
                                   std::conj(running_mean_[i]) * scale);
    }

    ++history_cursor_;
    if (history_cursor_ >= window_size_) {
      buffer_full_ = true;
      history_cursor_ = 0;
    }
  }
}

void VarianceArray::Clear() {
  memset(running_mean_.get(), 0, sizeof(*running_mean_.get()) * num_freqs_);
  memset(running_mean_sq_.get(), 0,
         sizeof(*running_mean_sq_.get()) * num_freqs_);
  memset(variance_.get(), 0, sizeof(*variance_.get()) * num_freqs_);
  memset(conj_sum_.get(), 0, sizeof(*conj_sum_.get()) * num_freqs_);
  history_cursor_ = 0;
  count_ = 0;
  array_mean_ = 0.0f;
}

void VarianceArray::ApplyScale(float scale) {
  array_mean_ = 0.0f;
  for (size_t i = 0; i < num_freqs_; ++i) {
    variance_[i] *= scale * scale;
    array_mean_ += (variance_[i] - array_mean_) / (i + 1);
  }
}

GainApplier::GainApplier(size_t freqs, float change_limit)
    : num_freqs_(freqs),
      change_limit_(change_limit),
      target_(new float[freqs]()),
      current_(new float[freqs]()) {
  for (size_t i = 0; i < freqs; ++i) {
    target_[i] = 1.0f;
    current_[i] = 1.0f;
  }
}

void GainApplier::Apply(const complex<float>* in_block,
                        complex<float>* out_block) {
  for (size_t i = 0; i < num_freqs_; ++i) {
    float factor = sqrtf(fabsf(current_[i]));
    if (!std::isnormal(factor)) {
      factor = 1.0f;
    }
    out_block[i] = factor * in_block[i];
    current_[i] = UpdateFactor(target_[i], current_[i], change_limit_);
  }
}

}  // namespace intelligibility

}  // namespace webrtc
