/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_processing/echo_control_mobile_impl.h"

#include <assert.h>
#include <string.h>

#include "webrtc/modules/audio_processing/aecm/include/echo_control_mobile.h"
#include "webrtc/modules/audio_processing/audio_buffer.h"
#include "webrtc/system_wrappers/interface/critical_section_wrapper.h"
#include "webrtc/system_wrappers/interface/logging.h"

namespace webrtc {

typedef void Handle;

namespace {
int16_t MapSetting(EchoControlMobile::RoutingMode mode) {
  switch (mode) {
    case EchoControlMobile::kQuietEarpieceOrHeadset:
      return 0;
    case EchoControlMobile::kEarpiece:
      return 1;
    case EchoControlMobile::kLoudEarpiece:
      return 2;
    case EchoControlMobile::kSpeakerphone:
      return 3;
    case EchoControlMobile::kLoudSpeakerphone:
      return 4;
  }
  assert(false);
  return -1;
}

}  // namespace

size_t EchoControlMobile::echo_path_size_bytes() {
    return WebRtcAecm_echo_path_size_bytes();
}

EchoControlMobileImpl::EchoControlMobileImpl(const AudioProcessing* apm,
                                             CriticalSectionWrapper* crit)
  : ProcessingComponent(),
    apm_(apm),
    crit_(crit),
    routing_mode_(kSpeakerphone),
    comfort_noise_enabled_(true),
    external_echo_path_(NULL) {}

EchoControlMobileImpl::~EchoControlMobileImpl() {
    if (external_echo_path_ != NULL) {
      delete [] external_echo_path_;
      external_echo_path_ = NULL;
    }
}

int EchoControlMobileImpl::ProcessRenderAudio(const AudioBuffer* audio) {
  if (!is_component_enabled()) {
    return apm_->kNoError;
  }

  assert(audio->num_frames_per_band() <= 160);
  assert(audio->num_channels() == apm_->num_reverse_channels());

  int err = apm_->kNoError;

  // The ordering convention must be followed to pass to the correct AECM.
  size_t handle_index = 0;
  for (int i = 0; i < apm_->num_output_channels(); i++) {
    for (int j = 0; j < audio->num_channels(); j++) {
      Handle* my_handle = static_cast<Handle*>(handle(handle_index));
      err = WebRtcAecm_BufferFarend(
          my_handle,
          audio->split_bands_const(j)[kBand0To8kHz],
          audio->num_frames_per_band());

      if (err != apm_->kNoError) {
        return GetHandleError(my_handle);  // TODO(ajm): warning possible?
      }

      handle_index++;
    }
  }

  return apm_->kNoError;
}

int EchoControlMobileImpl::ProcessCaptureAudio(AudioBuffer* audio) {
  if (!is_component_enabled()) {
    return apm_->kNoError;
  }

  if (!apm_->was_stream_delay_set()) {
    return apm_->kStreamParameterNotSetError;
  }

  assert(audio->num_frames_per_band() <= 160);
  assert(audio->num_channels() == apm_->num_output_channels());

  int err = apm_->kNoError;

  // The ordering convention must be followed to pass to the correct AECM.
  size_t handle_index = 0;
  for (int i = 0; i < audio->num_channels(); i++) {
    // TODO(ajm): improve how this works, possibly inside AECM.
    //            This is kind of hacked up.
    const int16_t* noisy = audio->low_pass_reference(i);
    const int16_t* clean = audio->split_bands_const(i)[kBand0To8kHz];
    if (noisy == NULL) {
      noisy = clean;
      clean = NULL;
    }
    for (int j = 0; j < apm_->num_reverse_channels(); j++) {
      Handle* my_handle = static_cast<Handle*>(handle(handle_index));
      err = WebRtcAecm_Process(
          my_handle,
          noisy,
          clean,
          audio->split_bands(i)[kBand0To8kHz],
          audio->num_frames_per_band(),
          apm_->stream_delay_ms());

      if (err != apm_->kNoError) {
        return GetHandleError(my_handle);  // TODO(ajm): warning possible?
      }

      handle_index++;
    }
  }

  return apm_->kNoError;
}

int EchoControlMobileImpl::Enable(bool enable) {
  CriticalSectionScoped crit_scoped(crit_);
  // Ensure AEC and AECM are not both enabled.
  if (enable && apm_->echo_cancellation()->is_enabled()) {
    return apm_->kBadParameterError;
  }

  return EnableComponent(enable);
}

bool EchoControlMobileImpl::is_enabled() const {
  return is_component_enabled();
}

int EchoControlMobileImpl::set_routing_mode(RoutingMode mode) {
  CriticalSectionScoped crit_scoped(crit_);
  if (MapSetting(mode) == -1) {
    return apm_->kBadParameterError;
  }

  routing_mode_ = mode;
  return Configure();
}

EchoControlMobile::RoutingMode EchoControlMobileImpl::routing_mode()
    const {
  return routing_mode_;
}

int EchoControlMobileImpl::enable_comfort_noise(bool enable) {
  CriticalSectionScoped crit_scoped(crit_);
  comfort_noise_enabled_ = enable;
  return Configure();
}

bool EchoControlMobileImpl::is_comfort_noise_enabled() const {
  return comfort_noise_enabled_;
}

int EchoControlMobileImpl::SetEchoPath(const void* echo_path,
                                       size_t size_bytes) {
  CriticalSectionScoped crit_scoped(crit_);
  if (echo_path == NULL) {
    return apm_->kNullPointerError;
  }
  if (size_bytes != echo_path_size_bytes()) {
    // Size mismatch
    return apm_->kBadParameterError;
  }

  if (external_echo_path_ == NULL) {
    external_echo_path_ = new unsigned char[size_bytes];
  }
  memcpy(external_echo_path_, echo_path, size_bytes);

  return Initialize();
}

int EchoControlMobileImpl::GetEchoPath(void* echo_path,
                                       size_t size_bytes) const {
  CriticalSectionScoped crit_scoped(crit_);
  if (echo_path == NULL) {
    return apm_->kNullPointerError;
  }
  if (size_bytes != echo_path_size_bytes()) {
    // Size mismatch
    return apm_->kBadParameterError;
  }
  if (!is_component_enabled()) {
    return apm_->kNotEnabledError;
  }

  // Get the echo path from the first channel
  Handle* my_handle = static_cast<Handle*>(handle(0));
  if (WebRtcAecm_GetEchoPath(my_handle, echo_path, size_bytes) != 0) {
      return GetHandleError(my_handle);
  }

  return apm_->kNoError;
}

int EchoControlMobileImpl::Initialize() {
  if (!is_component_enabled()) {
    return apm_->kNoError;
  }

  if (apm_->proc_sample_rate_hz() > apm_->kSampleRate16kHz) {
    LOG(LS_ERROR) << "AECM only supports 16 kHz or lower sample rates";
    return apm_->kBadSampleRateError;
  }

  return ProcessingComponent::Initialize();
}

void* EchoControlMobileImpl::CreateHandle() const {
  return WebRtcAecm_Create();
}

void EchoControlMobileImpl::DestroyHandle(void* handle) const {
  WebRtcAecm_Free(static_cast<Handle*>(handle));
}

int EchoControlMobileImpl::InitializeHandle(void* handle) const {
  assert(handle != NULL);
  Handle* my_handle = static_cast<Handle*>(handle);
  if (WebRtcAecm_Init(my_handle, apm_->proc_sample_rate_hz()) != 0) {
    return GetHandleError(my_handle);
  }
  if (external_echo_path_ != NULL) {
    if (WebRtcAecm_InitEchoPath(my_handle,
                                external_echo_path_,
                                echo_path_size_bytes()) != 0) {
      return GetHandleError(my_handle);
    }
  }

  return apm_->kNoError;
}

int EchoControlMobileImpl::ConfigureHandle(void* handle) const {
  AecmConfig config;
  config.cngMode = comfort_noise_enabled_;
  config.echoMode = MapSetting(routing_mode_);

  return WebRtcAecm_set_config(static_cast<Handle*>(handle), config);
}

int EchoControlMobileImpl::num_handles_required() const {
  return apm_->num_output_channels() *
         apm_->num_reverse_channels();
}

int EchoControlMobileImpl::GetHandleError(void* handle) const {
  assert(handle != NULL);
  return AudioProcessing::kUnspecifiedError;
}
}  // namespace webrtc
