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

#include "webrtc/modules/audio_processing/aecm/echo_control_mobile.h"
#include "webrtc/modules/audio_processing/audio_buffer.h"
#include "webrtc/system_wrappers/include/logging.h"

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

AudioProcessing::Error MapError(int err) {
  switch (err) {
    case AECM_UNSUPPORTED_FUNCTION_ERROR:
      return AudioProcessing::kUnsupportedFunctionError;
    case AECM_NULL_POINTER_ERROR:
      return AudioProcessing::kNullPointerError;
    case AECM_BAD_PARAMETER_ERROR:
      return AudioProcessing::kBadParameterError;
    case AECM_BAD_PARAMETER_WARNING:
      return AudioProcessing::kBadStreamParameterWarning;
    default:
      // AECM_UNSPECIFIED_ERROR
      // AECM_UNINITIALIZED_ERROR
      return AudioProcessing::kUnspecifiedError;
  }
}
// Maximum length that a frame of samples can have.
static const size_t kMaxAllowedValuesOfSamplesPerFrame = 160;
// Maximum number of frames to buffer in the render queue.
// TODO(peah): Decrease this once we properly handle hugely unbalanced
// reverse and forward call numbers.
static const size_t kMaxNumFramesToBuffer = 100;
}  // namespace

size_t EchoControlMobile::echo_path_size_bytes() {
    return WebRtcAecm_echo_path_size_bytes();
}

EchoControlMobileImpl::EchoControlMobileImpl(const AudioProcessing* apm,
                                             rtc::CriticalSection* crit_render,
                                             rtc::CriticalSection* crit_capture)
    : ProcessingComponent(),
      apm_(apm),
      crit_render_(crit_render),
      crit_capture_(crit_capture),
      routing_mode_(kSpeakerphone),
      comfort_noise_enabled_(true),
      external_echo_path_(NULL),
      render_queue_element_max_size_(0) {
  RTC_DCHECK(apm);
  RTC_DCHECK(crit_render);
  RTC_DCHECK(crit_capture);
}

EchoControlMobileImpl::~EchoControlMobileImpl() {
    if (external_echo_path_ != NULL) {
      delete [] external_echo_path_;
      external_echo_path_ = NULL;
    }
}

int EchoControlMobileImpl::ProcessRenderAudio(const AudioBuffer* audio) {
  rtc::CritScope cs_render(crit_render_);

  if (!is_component_enabled()) {
    return AudioProcessing::kNoError;
  }

  assert(audio->num_frames_per_band() <= 160);
  assert(audio->num_channels() == apm_->num_reverse_channels());

  int err = AudioProcessing::kNoError;
  // The ordering convention must be followed to pass to the correct AECM.
  size_t handle_index = 0;
  render_queue_buffer_.clear();
  for (size_t i = 0; i < apm_->num_output_channels(); i++) {
    for (size_t j = 0; j < audio->num_channels(); j++) {
      Handle* my_handle = static_cast<Handle*>(handle(handle_index));
      err = WebRtcAecm_GetBufferFarendError(
          my_handle, audio->split_bands_const(j)[kBand0To8kHz],
          audio->num_frames_per_band());

      if (err != AudioProcessing::kNoError)
        return MapError(err);  // TODO(ajm): warning possible?);

      // Buffer the samples in the render queue.
      render_queue_buffer_.insert(render_queue_buffer_.end(),
                                  audio->split_bands_const(j)[kBand0To8kHz],
                                  (audio->split_bands_const(j)[kBand0To8kHz] +
                                   audio->num_frames_per_band()));

      handle_index++;
    }
  }

  // Insert the samples into the queue.
  if (!render_signal_queue_->Insert(&render_queue_buffer_)) {
    // The data queue is full and needs to be emptied.
    ReadQueuedRenderData();

    // Retry the insert (should always work).
    RTC_DCHECK_EQ(render_signal_queue_->Insert(&render_queue_buffer_), true);
  }

  return AudioProcessing::kNoError;
}

// Read chunks of data that were received and queued on the render side from
// a queue. All the data chunks are buffered into the farend signal of the AEC.
void EchoControlMobileImpl::ReadQueuedRenderData() {
  rtc::CritScope cs_capture(crit_capture_);

  if (!is_component_enabled()) {
    return;
  }

  while (render_signal_queue_->Remove(&capture_queue_buffer_)) {
    size_t handle_index = 0;
    size_t buffer_index = 0;
    const size_t num_frames_per_band =
        capture_queue_buffer_.size() /
        (apm_->num_output_channels() * apm_->num_reverse_channels());
    for (size_t i = 0; i < apm_->num_output_channels(); i++) {
      for (size_t j = 0; j < apm_->num_reverse_channels(); j++) {
        Handle* my_handle = static_cast<Handle*>(handle(handle_index));
        WebRtcAecm_BufferFarend(my_handle, &capture_queue_buffer_[buffer_index],
                                num_frames_per_band);

        buffer_index += num_frames_per_band;
        handle_index++;
      }
    }
  }
}

int EchoControlMobileImpl::ProcessCaptureAudio(AudioBuffer* audio) {
  rtc::CritScope cs_capture(crit_capture_);

  if (!is_component_enabled()) {
    return AudioProcessing::kNoError;
  }

  if (!apm_->was_stream_delay_set()) {
    return AudioProcessing::kStreamParameterNotSetError;
  }

  assert(audio->num_frames_per_band() <= 160);
  assert(audio->num_channels() == apm_->num_output_channels());

  int err = AudioProcessing::kNoError;

  // The ordering convention must be followed to pass to the correct AECM.
  size_t handle_index = 0;
  for (size_t i = 0; i < audio->num_channels(); i++) {
    // TODO(ajm): improve how this works, possibly inside AECM.
    //            This is kind of hacked up.
    const int16_t* noisy = audio->low_pass_reference(i);
    const int16_t* clean = audio->split_bands_const(i)[kBand0To8kHz];
    if (noisy == NULL) {
      noisy = clean;
      clean = NULL;
    }
    for (size_t j = 0; j < apm_->num_reverse_channels(); j++) {
      Handle* my_handle = static_cast<Handle*>(handle(handle_index));
      err = WebRtcAecm_Process(
          my_handle,
          noisy,
          clean,
          audio->split_bands(i)[kBand0To8kHz],
          audio->num_frames_per_band(),
          apm_->stream_delay_ms());

      if (err != AudioProcessing::kNoError)
        return MapError(err);

      handle_index++;
    }
  }

  return AudioProcessing::kNoError;
}

int EchoControlMobileImpl::Enable(bool enable) {
  // Ensure AEC and AECM are not both enabled.
  rtc::CritScope cs_render(crit_render_);
  rtc::CritScope cs_capture(crit_capture_);
  // The is_enabled call is safe from a deadlock perspective
  // as both locks are allready held in the correct order.
  if (enable && apm_->echo_cancellation()->is_enabled()) {
    return AudioProcessing::kBadParameterError;
  }

  return EnableComponent(enable);
}

bool EchoControlMobileImpl::is_enabled() const {
  rtc::CritScope cs(crit_capture_);
  return is_component_enabled();
}

int EchoControlMobileImpl::set_routing_mode(RoutingMode mode) {
  if (MapSetting(mode) == -1) {
    return AudioProcessing::kBadParameterError;
  }

  {
    rtc::CritScope cs(crit_capture_);
    routing_mode_ = mode;
  }
  return Configure();
}

EchoControlMobile::RoutingMode EchoControlMobileImpl::routing_mode()
    const {
  rtc::CritScope cs(crit_capture_);
  return routing_mode_;
}

int EchoControlMobileImpl::enable_comfort_noise(bool enable) {
  {
    rtc::CritScope cs(crit_capture_);
    comfort_noise_enabled_ = enable;
  }
  return Configure();
}

bool EchoControlMobileImpl::is_comfort_noise_enabled() const {
  rtc::CritScope cs(crit_capture_);
  return comfort_noise_enabled_;
}

int EchoControlMobileImpl::SetEchoPath(const void* echo_path,
                                       size_t size_bytes) {
  {
    rtc::CritScope cs_render(crit_render_);
    rtc::CritScope cs_capture(crit_capture_);
    if (echo_path == NULL) {
      return AudioProcessing::kNullPointerError;
    }
    if (size_bytes != echo_path_size_bytes()) {
      // Size mismatch
      return AudioProcessing::kBadParameterError;
    }

    if (external_echo_path_ == NULL) {
      external_echo_path_ = new unsigned char[size_bytes];
    }
    memcpy(external_echo_path_, echo_path, size_bytes);
  }

  return Initialize();
}

int EchoControlMobileImpl::GetEchoPath(void* echo_path,
                                       size_t size_bytes) const {
  rtc::CritScope cs(crit_capture_);
  if (echo_path == NULL) {
    return AudioProcessing::kNullPointerError;
  }
  if (size_bytes != echo_path_size_bytes()) {
    // Size mismatch
    return AudioProcessing::kBadParameterError;
  }
  if (!is_component_enabled()) {
    return AudioProcessing::kNotEnabledError;
  }

  // Get the echo path from the first channel
  Handle* my_handle = static_cast<Handle*>(handle(0));
  int32_t err = WebRtcAecm_GetEchoPath(my_handle, echo_path, size_bytes);
  if (err != 0)
    return MapError(err);

  return AudioProcessing::kNoError;
}

int EchoControlMobileImpl::Initialize() {
  {
    rtc::CritScope cs_capture(crit_capture_);
    if (!is_component_enabled()) {
      return AudioProcessing::kNoError;
    }
  }

  if (apm_->proc_sample_rate_hz() > AudioProcessing::kSampleRate16kHz) {
    LOG(LS_ERROR) << "AECM only supports 16 kHz or lower sample rates";
    return AudioProcessing::kBadSampleRateError;
  }

  int err = ProcessingComponent::Initialize();
  if (err != AudioProcessing::kNoError) {
    return err;
  }

  AllocateRenderQueue();

  return AudioProcessing::kNoError;
}

void EchoControlMobileImpl::AllocateRenderQueue() {
  const size_t new_render_queue_element_max_size = std::max<size_t>(
      static_cast<size_t>(1),
      kMaxAllowedValuesOfSamplesPerFrame * num_handles_required());

  rtc::CritScope cs_render(crit_render_);
  rtc::CritScope cs_capture(crit_capture_);

  // Reallocate the queue if the queue item size is too small to fit the
  // data to put in the queue.
  if (render_queue_element_max_size_ < new_render_queue_element_max_size) {
    render_queue_element_max_size_ = new_render_queue_element_max_size;

    std::vector<int16_t> template_queue_element(render_queue_element_max_size_);

    render_signal_queue_.reset(
        new SwapQueue<std::vector<int16_t>, RenderQueueItemVerifier<int16_t>>(
            kMaxNumFramesToBuffer, template_queue_element,
            RenderQueueItemVerifier<int16_t>(render_queue_element_max_size_)));

    render_queue_buffer_.resize(render_queue_element_max_size_);
    capture_queue_buffer_.resize(render_queue_element_max_size_);
  } else {
    render_signal_queue_->Clear();
  }
}

void* EchoControlMobileImpl::CreateHandle() const {
  return WebRtcAecm_Create();
}

void EchoControlMobileImpl::DestroyHandle(void* handle) const {
  // This method is only called in a non-concurrent manner during APM
  // destruction.
  WebRtcAecm_Free(static_cast<Handle*>(handle));
}

int EchoControlMobileImpl::InitializeHandle(void* handle) const {
  rtc::CritScope cs_render(crit_render_);
  rtc::CritScope cs_capture(crit_capture_);
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

  return AudioProcessing::kNoError;
}

int EchoControlMobileImpl::ConfigureHandle(void* handle) const {
  rtc::CritScope cs_render(crit_render_);
  rtc::CritScope cs_capture(crit_capture_);
  AecmConfig config;
  config.cngMode = comfort_noise_enabled_;
  config.echoMode = MapSetting(routing_mode_);

  return WebRtcAecm_set_config(static_cast<Handle*>(handle), config);
}

size_t EchoControlMobileImpl::num_handles_required() const {
  // Not locked as it only relies on APM public API which is threadsafe.
  return apm_->num_output_channels() * apm_->num_reverse_channels();
}

int EchoControlMobileImpl::GetHandleError(void* handle) const {
  // Not locked as it does not rely on anything in the state.
  assert(handle != NULL);
  return AudioProcessing::kUnspecifiedError;
}
}  // namespace webrtc
