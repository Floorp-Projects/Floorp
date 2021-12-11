/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/echo_control_mobile_impl.h"

#include <string.h>

#include "modules/audio_processing/aecm/echo_control_mobile.h"
#include "modules/audio_processing/audio_buffer.h"
#include "rtc_base/constructormagic.h"
#include "rtc_base/logging.h"

namespace webrtc {

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
  RTC_NOTREACHED();
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
}  // namespace

size_t EchoControlMobile::echo_path_size_bytes() {
  return WebRtcAecm_echo_path_size_bytes();
}

struct EchoControlMobileImpl::StreamProperties {
  StreamProperties() = delete;
  StreamProperties(int sample_rate_hz,
                   size_t num_reverse_channels,
                   size_t num_output_channels)
      : sample_rate_hz(sample_rate_hz),
        num_reverse_channels(num_reverse_channels),
        num_output_channels(num_output_channels) {}

  int sample_rate_hz;
  size_t num_reverse_channels;
  size_t num_output_channels;
};

class EchoControlMobileImpl::Canceller {
 public:
  Canceller() {
    state_ = WebRtcAecm_Create();
    RTC_CHECK(state_);
  }

  ~Canceller() {
    RTC_DCHECK(state_);
    WebRtcAecm_Free(state_);
  }

  void* state() {
    RTC_DCHECK(state_);
    return state_;
  }

  void Initialize(int sample_rate_hz,
                  unsigned char* external_echo_path,
                  size_t echo_path_size_bytes) {
    RTC_DCHECK(state_);
    int error = WebRtcAecm_Init(state_, sample_rate_hz);
    RTC_DCHECK_EQ(AudioProcessing::kNoError, error);
    if (external_echo_path != NULL) {
      error = WebRtcAecm_InitEchoPath(state_, external_echo_path,
                                      echo_path_size_bytes);
      RTC_DCHECK_EQ(AudioProcessing::kNoError, error);
    }
  }

 private:
  void* state_;
  RTC_DISALLOW_COPY_AND_ASSIGN(Canceller);
};

EchoControlMobileImpl::EchoControlMobileImpl(rtc::CriticalSection* crit_render,
                                             rtc::CriticalSection* crit_capture)
    : crit_render_(crit_render),
      crit_capture_(crit_capture),
      routing_mode_(kSpeakerphone),
      comfort_noise_enabled_(true),
      external_echo_path_(NULL) {
  RTC_DCHECK(crit_render);
  RTC_DCHECK(crit_capture);
}

EchoControlMobileImpl::~EchoControlMobileImpl() {
    if (external_echo_path_ != NULL) {
      delete [] external_echo_path_;
      external_echo_path_ = NULL;
    }
}

void EchoControlMobileImpl::ProcessRenderAudio(
    rtc::ArrayView<const int16_t> packed_render_audio) {
  rtc::CritScope cs_capture(crit_capture_);
  if (!enabled_) {
    return;
  }

  RTC_DCHECK(stream_properties_);

  size_t buffer_index = 0;
  size_t num_frames_per_band =
      packed_render_audio.size() / (stream_properties_->num_output_channels *
                                    stream_properties_->num_reverse_channels);

  for (auto& canceller : cancellers_) {
    WebRtcAecm_BufferFarend(canceller->state(),
                            &packed_render_audio[buffer_index],
                            num_frames_per_band);

    buffer_index += num_frames_per_band;
  }
}

void EchoControlMobileImpl::PackRenderAudioBuffer(
    const AudioBuffer* audio,
    size_t num_output_channels,
    size_t num_channels,
    std::vector<int16_t>* packed_buffer) {
  RTC_DCHECK_GE(160, audio->num_frames_per_band());
  RTC_DCHECK_EQ(num_channels, audio->num_channels());

  // The ordering convention must be followed to pass to the correct AECM.
  packed_buffer->clear();
  int render_channel = 0;
  for (size_t i = 0; i < num_output_channels; i++) {
    for (size_t j = 0; j < audio->num_channels(); j++) {
      // Buffer the samples in the render queue.
      packed_buffer->insert(
          packed_buffer->end(),
          audio->split_bands_const(render_channel)[kBand0To8kHz],
          (audio->split_bands_const(render_channel)[kBand0To8kHz] +
           audio->num_frames_per_band()));
      render_channel = (render_channel + 1) % audio->num_channels();
    }
  }
}

size_t EchoControlMobileImpl::NumCancellersRequired(
    size_t num_output_channels,
    size_t num_reverse_channels) {
  return num_output_channels * num_reverse_channels;
}

int EchoControlMobileImpl::ProcessCaptureAudio(AudioBuffer* audio,
                                               int stream_delay_ms) {
  rtc::CritScope cs_capture(crit_capture_);
  if (!enabled_) {
    return AudioProcessing::kNoError;
  }

  RTC_DCHECK(stream_properties_);
  RTC_DCHECK_GE(160, audio->num_frames_per_band());
  RTC_DCHECK_EQ(audio->num_channels(), stream_properties_->num_output_channels);
  RTC_DCHECK_GE(cancellers_.size(), stream_properties_->num_reverse_channels *
                                        audio->num_channels());

  int err = AudioProcessing::kNoError;

  // The ordering convention must be followed to pass to the correct AECM.
  size_t handle_index = 0;
  for (size_t capture = 0; capture < audio->num_channels(); ++capture) {
    // TODO(ajm): improve how this works, possibly inside AECM.
    //            This is kind of hacked up.
    const int16_t* noisy = audio->low_pass_reference(capture);
    const int16_t* clean = audio->split_bands_const(capture)[kBand0To8kHz];
    if (noisy == NULL) {
      noisy = clean;
      clean = NULL;
    }
    for (size_t render = 0; render < stream_properties_->num_reverse_channels;
         ++render) {
      err = WebRtcAecm_Process(cancellers_[handle_index]->state(), noisy, clean,
                               audio->split_bands(capture)[kBand0To8kHz],
                               audio->num_frames_per_band(), stream_delay_ms);

      if (err != AudioProcessing::kNoError) {
        return MapError(err);
      }

      ++handle_index;
    }
    for (size_t band = 1u; band < audio->num_bands(); ++band) {
      memset(audio->split_bands(capture)[band],
             0,
             audio->num_frames_per_band() *
                 sizeof(audio->split_bands(capture)[band][0]));
    }
  }
  return AudioProcessing::kNoError;
}

int EchoControlMobileImpl::Enable(bool enable) {
  // Ensure AEC and AECM are not both enabled.
  rtc::CritScope cs_render(crit_render_);
  rtc::CritScope cs_capture(crit_capture_);
  RTC_DCHECK(stream_properties_);

  if (enable &&
      stream_properties_->sample_rate_hz > AudioProcessing::kSampleRate16kHz) {
    return AudioProcessing::kBadSampleRateError;
  }

  if (enable && !enabled_) {
    enabled_ = enable;  // Must be set before Initialize() is called.

    // TODO(peah): Simplify once the Enable function has been removed from
    // the public APM API.
    Initialize(stream_properties_->sample_rate_hz,
               stream_properties_->num_reverse_channels,
               stream_properties_->num_output_channels);
  } else {
    enabled_ = enable;
  }
  return AudioProcessing::kNoError;
}

bool EchoControlMobileImpl::is_enabled() const {
  rtc::CritScope cs(crit_capture_);
  return enabled_;
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

  // TODO(peah): Simplify once the Enable function has been removed from
  // the public APM API.
  RTC_DCHECK(stream_properties_);
  Initialize(stream_properties_->sample_rate_hz,
             stream_properties_->num_reverse_channels,
             stream_properties_->num_output_channels);
  return AudioProcessing::kNoError;
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
  if (!enabled_) {
    return AudioProcessing::kNotEnabledError;
  }

  // Get the echo path from the first channel
  int32_t err =
      WebRtcAecm_GetEchoPath(cancellers_[0]->state(), echo_path, size_bytes);
  if (err != 0) {
    return MapError(err);
  }

  return AudioProcessing::kNoError;
}

void EchoControlMobileImpl::Initialize(int sample_rate_hz,
                                       size_t num_reverse_channels,
                                       size_t num_output_channels) {
  rtc::CritScope cs_render(crit_render_);
  rtc::CritScope cs_capture(crit_capture_);

  stream_properties_.reset(new StreamProperties(
      sample_rate_hz, num_reverse_channels, num_output_channels));

  if (!enabled_) {
    return;
  }

  if (stream_properties_->sample_rate_hz > AudioProcessing::kSampleRate16kHz) {
    RTC_LOG(LS_ERROR) << "AECM only supports 16 kHz or lower sample rates";
  }

  cancellers_.resize(
      NumCancellersRequired(stream_properties_->num_output_channels,
                            stream_properties_->num_reverse_channels));

  for (auto& canceller : cancellers_) {
    if (!canceller) {
      canceller.reset(new Canceller());
    }
    canceller->Initialize(sample_rate_hz, external_echo_path_,
                          echo_path_size_bytes());
  }

  Configure();
}

int EchoControlMobileImpl::Configure() {
  rtc::CritScope cs_render(crit_render_);
  rtc::CritScope cs_capture(crit_capture_);
  AecmConfig config;
  config.cngMode = comfort_noise_enabled_;
  config.echoMode = MapSetting(routing_mode_);
  int error = AudioProcessing::kNoError;
  for (auto& canceller : cancellers_) {
    int handle_error = WebRtcAecm_set_config(canceller->state(), config);
    if (handle_error != AudioProcessing::kNoError) {
      error = handle_error;
    }
  }
  return error;
}

}  // namespace webrtc
