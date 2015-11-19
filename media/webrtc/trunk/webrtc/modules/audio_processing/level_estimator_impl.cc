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

#include "webrtc/modules/audio_processing/audio_buffer.h"
#include "webrtc/modules/audio_processing/include/audio_processing.h"
#include "webrtc/modules/audio_processing/rms_level.h"
#include "webrtc/system_wrappers/interface/critical_section_wrapper.h"

namespace webrtc {

LevelEstimatorImpl::LevelEstimatorImpl(const AudioProcessing* apm,
                                       CriticalSectionWrapper* crit)
    : ProcessingComponent(),
      crit_(crit) {}

LevelEstimatorImpl::~LevelEstimatorImpl() {}

int LevelEstimatorImpl::ProcessStream(AudioBuffer* audio) {
  if (!is_component_enabled()) {
    return AudioProcessing::kNoError;
  }

  RMSLevel* rms_level = static_cast<RMSLevel*>(handle(0));
  for (int i = 0; i < audio->num_channels(); ++i) {
    rms_level->Process(audio->channels_const()[i],
                       audio->num_frames());
  }

  return AudioProcessing::kNoError;
}

int LevelEstimatorImpl::Enable(bool enable) {
  CriticalSectionScoped crit_scoped(crit_);
  return EnableComponent(enable);
}

bool LevelEstimatorImpl::is_enabled() const {
  return is_component_enabled();
}

int LevelEstimatorImpl::RMS() {
  if (!is_component_enabled()) {
    return AudioProcessing::kNotEnabledError;
  }

  RMSLevel* rms_level = static_cast<RMSLevel*>(handle(0));
  return rms_level->RMS();
}

// The ProcessingComponent implementation is pretty weird in this class since
// we have only a single instance of the trivial underlying component.
void* LevelEstimatorImpl::CreateHandle() const {
  return new RMSLevel;
}

void LevelEstimatorImpl::DestroyHandle(void* handle) const {
  delete static_cast<RMSLevel*>(handle);
}

int LevelEstimatorImpl::InitializeHandle(void* handle) const {
  static_cast<RMSLevel*>(handle)->Reset();
  return AudioProcessing::kNoError;
}

int LevelEstimatorImpl::ConfigureHandle(void* /*handle*/) const {
  return AudioProcessing::kNoError;
}

int LevelEstimatorImpl::num_handles_required() const {
  return 1;
}

int LevelEstimatorImpl::GetHandleError(void* /*handle*/) const {
  return AudioProcessing::kUnspecifiedError;
}

}  // namespace webrtc
