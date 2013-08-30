/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_processing/level_estimator_impl.h"

#include <assert.h>
#include <math.h>
#include <string.h>

#include "webrtc/modules/audio_processing/audio_buffer.h"
#include "webrtc/modules/audio_processing/audio_processing_impl.h"
#include "webrtc/system_wrappers/interface/critical_section_wrapper.h"

namespace webrtc {
namespace {

const double kMaxSquaredLevel = 32768.0 * 32768.0;

class Level {
 public:
  static const int kMinLevel = 127;

  Level()
    : sum_square_(0.0),
      sample_count_(0) {}
  ~Level() {}

  void Init() {
    sum_square_ = 0.0;
    sample_count_ = 0;
  }

  void Process(int16_t* data, int length) {
    assert(data != NULL);
    assert(length > 0);
    sum_square_ += SumSquare(data, length);
    sample_count_ += length;
  }

  void ProcessMuted(int length) {
    assert(length > 0);
    sample_count_ += length;
  }

  int RMS() {
    if (sample_count_ == 0 || sum_square_ == 0.0) {
      Init();
      return kMinLevel;
    }

    // Normalize by the max level.
    double rms = sum_square_ / (sample_count_ * kMaxSquaredLevel);
    // 20log_10(x^0.5) = 10log_10(x)
    rms = 10 * log10(rms);
    if (rms > 0)
      rms = 0;
    else if (rms < -kMinLevel)
      rms = -kMinLevel;

    rms = -rms;
    Init();
    return static_cast<int>(rms + 0.5);
  }

 private:
  static double SumSquare(int16_t* data, int length) {
    double sum_square = 0.0;
    for (int i = 0; i < length; ++i) {
      double data_d = static_cast<double>(data[i]);
      sum_square += data_d * data_d;
    }
    return sum_square;
  }

  double sum_square_;
  int sample_count_;
};
}  // namespace

LevelEstimatorImpl::LevelEstimatorImpl(const AudioProcessingImpl* apm)
  : ProcessingComponent(apm),
    apm_(apm) {}

LevelEstimatorImpl::~LevelEstimatorImpl() {}

int LevelEstimatorImpl::ProcessStream(AudioBuffer* audio) {
  if (!is_component_enabled()) {
    return apm_->kNoError;
  }

  Level* level = static_cast<Level*>(handle(0));
  if (audio->is_muted()) {
    level->ProcessMuted(audio->samples_per_channel());
    return apm_->kNoError;
  }

  int16_t* mixed_data = audio->data(0);
  if (audio->num_channels() > 1) {
    audio->CopyAndMix(1);
    mixed_data = audio->mixed_data(0);
  }

  level->Process(mixed_data, audio->samples_per_channel());

  return apm_->kNoError;
}

int LevelEstimatorImpl::Enable(bool enable) {
  CriticalSectionScoped crit_scoped(apm_->crit());
  return EnableComponent(enable);
}

bool LevelEstimatorImpl::is_enabled() const {
  return is_component_enabled();
}

int LevelEstimatorImpl::RMS() {
  if (!is_component_enabled()) {
    return apm_->kNotEnabledError;
  }

  Level* level = static_cast<Level*>(handle(0));
  return level->RMS();
}

void* LevelEstimatorImpl::CreateHandle() const {
  return new Level;
}

int LevelEstimatorImpl::DestroyHandle(void* handle) const {
  assert(handle != NULL);
  Level* level = static_cast<Level*>(handle);
  delete level;
  return apm_->kNoError;
}

int LevelEstimatorImpl::InitializeHandle(void* handle) const {
  assert(handle != NULL);
  Level* level = static_cast<Level*>(handle);
  level->Init();

  return apm_->kNoError;
}

int LevelEstimatorImpl::ConfigureHandle(void* /*handle*/) const {
  return apm_->kNoError;
}

int LevelEstimatorImpl::num_handles_required() const {
  return 1;
}

int LevelEstimatorImpl::GetHandleError(void* handle) const {
  // The component has no detailed errors.
  assert(handle != NULL);
  return apm_->kUnspecifiedError;
}
}  // namespace webrtc
