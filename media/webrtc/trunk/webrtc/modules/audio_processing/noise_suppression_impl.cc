/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "noise_suppression_impl.h"

#include <cassert>

#include "critical_section_wrapper.h"
#if defined(WEBRTC_NS_FLOAT)
#include "noise_suppression.h"
#elif defined(WEBRTC_NS_FIXED)
#include "noise_suppression_x.h"
#endif

#include "audio_processing_impl.h"
#include "audio_buffer.h"

namespace webrtc {

#if defined(WEBRTC_NS_FLOAT)
typedef NsHandle Handle;
#elif defined(WEBRTC_NS_FIXED)
typedef NsxHandle Handle;
#endif

namespace {
int MapSetting(NoiseSuppression::Level level) {
  switch (level) {
    case NoiseSuppression::kLow:
      return 0;
    case NoiseSuppression::kModerate:
      return 1;
    case NoiseSuppression::kHigh:
      return 2;
    case NoiseSuppression::kVeryHigh:
      return 3;
  }
  assert(false);
  return -1;
}
}  // namespace

NoiseSuppressionImpl::NoiseSuppressionImpl(const AudioProcessingImpl* apm)
  : ProcessingComponent(apm),
    apm_(apm),
    level_(kModerate) {}

NoiseSuppressionImpl::~NoiseSuppressionImpl() {}

int NoiseSuppressionImpl::ProcessCaptureAudio(AudioBuffer* audio) {
  int err = apm_->kNoError;

  if (!is_component_enabled()) {
    return apm_->kNoError;
  }
  assert(audio->samples_per_split_channel() <= 160);
  assert(audio->num_channels() == num_handles());

  for (int i = 0; i < num_handles(); i++) {
    Handle* my_handle = static_cast<Handle*>(handle(i));
#if defined(WEBRTC_NS_FLOAT)
    err = WebRtcNs_Process(static_cast<Handle*>(handle(i)),
                           audio->low_pass_split_data(i),
                           audio->high_pass_split_data(i),
                           audio->low_pass_split_data(i),
                           audio->high_pass_split_data(i));
#elif defined(WEBRTC_NS_FIXED)
    err = WebRtcNsx_Process(static_cast<Handle*>(handle(i)),
                            audio->low_pass_split_data(i),
                            audio->high_pass_split_data(i),
                            audio->low_pass_split_data(i),
                            audio->high_pass_split_data(i));
#endif

    if (err != apm_->kNoError) {
      return GetHandleError(my_handle);
    }
  }

  return apm_->kNoError;
}

int NoiseSuppressionImpl::Enable(bool enable) {
  CriticalSectionScoped crit_scoped(apm_->crit());
  return EnableComponent(enable);
}

bool NoiseSuppressionImpl::is_enabled() const {
  return is_component_enabled();
}

int NoiseSuppressionImpl::set_level(Level level) {
  CriticalSectionScoped crit_scoped(apm_->crit());
  if (MapSetting(level) == -1) {
    return apm_->kBadParameterError;
  }

  level_ = level;
  return Configure();
}

NoiseSuppression::Level NoiseSuppressionImpl::level() const {
  return level_;
}

float NoiseSuppressionImpl::speech_probability() const {
#if defined(WEBRTC_NS_FLOAT)
  float probability_average = 0.0f;
  for (int i = 0; i < num_handles(); i++) {
    Handle* my_handle = static_cast<Handle*>(handle(i));
    probability_average += WebRtcNs_prior_speech_probability(my_handle);
  }
  return probability_average / num_handles();
#elif defined(WEBRTC_NS_FIXED)
  // Currently not available for the fixed point implementation.
  return apm_->kUnsupportedFunctionError;
#endif
}

void* NoiseSuppressionImpl::CreateHandle() const {
  Handle* handle = NULL;
#if defined(WEBRTC_NS_FLOAT)
  if (WebRtcNs_Create(&handle) != apm_->kNoError)
#elif defined(WEBRTC_NS_FIXED)
  if (WebRtcNsx_Create(&handle) != apm_->kNoError)
#endif
  {
    handle = NULL;
  } else {
    assert(handle != NULL);
  }

  return handle;
}

int NoiseSuppressionImpl::DestroyHandle(void* handle) const {
#if defined(WEBRTC_NS_FLOAT)
  return WebRtcNs_Free(static_cast<Handle*>(handle));
#elif defined(WEBRTC_NS_FIXED)
  return WebRtcNsx_Free(static_cast<Handle*>(handle));
#endif
}

int NoiseSuppressionImpl::InitializeHandle(void* handle) const {
#if defined(WEBRTC_NS_FLOAT)
  return WebRtcNs_Init(static_cast<Handle*>(handle), apm_->sample_rate_hz());
#elif defined(WEBRTC_NS_FIXED)
  return WebRtcNsx_Init(static_cast<Handle*>(handle), apm_->sample_rate_hz());
#endif
}

int NoiseSuppressionImpl::ConfigureHandle(void* handle) const {
#if defined(WEBRTC_NS_FLOAT)
  return WebRtcNs_set_policy(static_cast<Handle*>(handle),
                             MapSetting(level_));
#elif defined(WEBRTC_NS_FIXED)
  return WebRtcNsx_set_policy(static_cast<Handle*>(handle),
                              MapSetting(level_));
#endif
}

int NoiseSuppressionImpl::num_handles_required() const {
  return apm_->num_output_channels();
}

int NoiseSuppressionImpl::GetHandleError(void* handle) const {
  // The NS has no get_error() function.
  assert(handle != NULL);
  return apm_->kUnspecifiedError;
}
}  // namespace webrtc

