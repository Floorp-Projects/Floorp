/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_processing/audio_processing_impl.h"

#include <assert.h>

#include "webrtc/common_audio/signal_processing/include/signal_processing_library.h"
#include "webrtc/modules/audio_processing/audio_buffer.h"
#include "webrtc/modules/audio_processing/echo_cancellation_impl_wrapper.h"
#include "webrtc/modules/audio_processing/echo_control_mobile_impl.h"
#include "webrtc/modules/audio_processing/gain_control_impl.h"
#include "webrtc/modules/audio_processing/high_pass_filter_impl.h"
#include "webrtc/modules/audio_processing/level_estimator_impl.h"
#include "webrtc/modules/audio_processing/noise_suppression_impl.h"
#include "webrtc/modules/audio_processing/processing_component.h"
#include "webrtc/modules/audio_processing/voice_detection_impl.h"
#include "webrtc/modules/interface/module_common_types.h"
#include "webrtc/system_wrappers/interface/compile_assert.h"
#include "webrtc/system_wrappers/interface/critical_section_wrapper.h"
#include "webrtc/system_wrappers/interface/file_wrapper.h"
#include "webrtc/system_wrappers/interface/logging.h"

#ifdef WEBRTC_AUDIOPROC_DEBUG_DUMP
// Files generated at build-time by the protobuf compiler.
#ifdef WEBRTC_ANDROID_PLATFORM_BUILD
#include "external/webrtc/webrtc/modules/audio_processing/debug.pb.h"
#else
#include "webrtc/audio_processing/debug.pb.h"
#endif
#endif  // WEBRTC_AUDIOPROC_DEBUG_DUMP

static const int kChunkSizeMs = 10;

#define RETURN_ON_ERR(expr)  \
  do {                       \
    int err = expr;          \
    if (err != kNoError) {   \
      return err;            \
    }                        \
  } while (0)

namespace webrtc {

// Throughout webrtc, it's assumed that success is represented by zero.
COMPILE_ASSERT(AudioProcessing::kNoError == 0, no_error_must_be_zero);

AudioProcessing* AudioProcessing::Create(int id) {
  return Create();
}

AudioProcessing* AudioProcessing::Create() {
  Config config;
  return Create(config);
}

AudioProcessing* AudioProcessing::Create(const Config& config) {
  AudioProcessingImpl* apm = new AudioProcessingImpl(config);
  if (apm->Initialize() != kNoError) {
    delete apm;
    apm = NULL;
  }

  return apm;
}

AudioProcessingImpl::AudioProcessingImpl(const Config& config)
    : echo_cancellation_(NULL),
      echo_control_mobile_(NULL),
      gain_control_(NULL),
      high_pass_filter_(NULL),
      level_estimator_(NULL),
      noise_suppression_(NULL),
      voice_detection_(NULL),
      crit_(CriticalSectionWrapper::CreateCriticalSection()),
      render_audio_(NULL),
      capture_audio_(NULL),
#ifdef WEBRTC_AUDIOPROC_DEBUG_DUMP
      debug_file_(FileWrapper::Create()),
      event_msg_(new audioproc::Event()),
#endif
      sample_rate_hz_(kSampleRate16kHz),
      split_sample_rate_hz_(kSampleRate16kHz),
      samples_per_channel_(kChunkSizeMs * sample_rate_hz_ / 1000),
      stream_delay_ms_(0),
      delay_offset_ms_(0),
      was_stream_delay_set_(false),
      num_reverse_channels_(1),
      num_input_channels_(1),
      num_output_channels_(1),
      output_will_be_muted_(false),
      key_pressed_(false) {
  echo_cancellation_ = EchoCancellationImplWrapper::Create(this);
  component_list_.push_back(echo_cancellation_);

  echo_control_mobile_ = new EchoControlMobileImpl(this);
  component_list_.push_back(echo_control_mobile_);

  gain_control_ = new GainControlImpl(this);
  component_list_.push_back(gain_control_);

  high_pass_filter_ = new HighPassFilterImpl(this);
  component_list_.push_back(high_pass_filter_);

  level_estimator_ = new LevelEstimatorImpl(this);
  component_list_.push_back(level_estimator_);

  noise_suppression_ = new NoiseSuppressionImpl(this);
  component_list_.push_back(noise_suppression_);

  voice_detection_ = new VoiceDetectionImpl(this);
  component_list_.push_back(voice_detection_);

  SetExtraOptions(config);
}

AudioProcessingImpl::~AudioProcessingImpl() {
  {
    CriticalSectionScoped crit_scoped(crit_);
    while (!component_list_.empty()) {
      ProcessingComponent* component = component_list_.front();
      component->Destroy();
      delete component;
      component_list_.pop_front();
    }

#ifdef WEBRTC_AUDIOPROC_DEBUG_DUMP
    if (debug_file_->Open()) {
      debug_file_->CloseFile();
    }
#endif

    if (render_audio_) {
      delete render_audio_;
      render_audio_ = NULL;
    }

    if (capture_audio_) {
      delete capture_audio_;
      capture_audio_ = NULL;
    }
  }

  delete crit_;
  crit_ = NULL;
}

CriticalSectionWrapper* AudioProcessingImpl::crit() const {
  return crit_;
}

int AudioProcessingImpl::split_sample_rate_hz() const {
  return split_sample_rate_hz_;
}

int AudioProcessingImpl::Initialize() {
  CriticalSectionScoped crit_scoped(crit_);
  return InitializeLocked();
}

int AudioProcessingImpl::InitializeLocked() {
  if (render_audio_ != NULL) {
    delete render_audio_;
    render_audio_ = NULL;
  }

  if (capture_audio_ != NULL) {
    delete capture_audio_;
    capture_audio_ = NULL;
  }

  render_audio_ = new AudioBuffer(num_reverse_channels_,
                                  samples_per_channel_);
  capture_audio_ = new AudioBuffer(num_input_channels_,
                                   samples_per_channel_);

  // Initialize all components.
  std::list<ProcessingComponent*>::iterator it;
  for (it = component_list_.begin(); it != component_list_.end(); ++it) {
    int err = (*it)->Initialize();
    if (err != kNoError) {
      return err;
    }
  }

#ifdef WEBRTC_AUDIOPROC_DEBUG_DUMP
  if (debug_file_->Open()) {
    int err = WriteInitMessage();
    if (err != kNoError) {
      return err;
    }
  }
#endif

  return kNoError;
}

void AudioProcessingImpl::SetExtraOptions(const Config& config) {
  CriticalSectionScoped crit_scoped(crit_);
  std::list<ProcessingComponent*>::iterator it;
  for (it = component_list_.begin(); it != component_list_.end(); ++it)
    (*it)->SetExtraOptions(config);
}

int AudioProcessingImpl::EnableExperimentalNs(bool enable) {
  return kNoError;
}

int AudioProcessingImpl::set_sample_rate_hz(int rate) {
  CriticalSectionScoped crit_scoped(crit_);
  if (rate == sample_rate_hz_) {
    return kNoError;
  }
  if (rate != kSampleRate8kHz &&
      rate != kSampleRate16kHz &&
      rate != kSampleRate32kHz) {
    return kBadParameterError;
  }
  if (echo_control_mobile_->is_enabled() && rate > kSampleRate16kHz) {
    LOG(LS_ERROR) << "AECM only supports 16 kHz or lower sample rates";
    return kUnsupportedComponentError;
  }

  sample_rate_hz_ = rate;
  samples_per_channel_ = rate / 100;

  if (sample_rate_hz_ == kSampleRate32kHz) {
    split_sample_rate_hz_ = kSampleRate16kHz;
  } else {
    split_sample_rate_hz_ = sample_rate_hz_;
  }

  return InitializeLocked();
}

int AudioProcessingImpl::sample_rate_hz() const {
  CriticalSectionScoped crit_scoped(crit_);
  return sample_rate_hz_;
}

int AudioProcessingImpl::set_num_reverse_channels(int channels) {
  CriticalSectionScoped crit_scoped(crit_);
  if (channels == num_reverse_channels_) {
    return kNoError;
  }
  // Only stereo supported currently.
  if (channels > 2 || channels < 1) {
    return kBadParameterError;
  }

  num_reverse_channels_ = channels;

  return InitializeLocked();
}

int AudioProcessingImpl::num_reverse_channels() const {
  return num_reverse_channels_;
}

int AudioProcessingImpl::set_num_channels(
    int input_channels,
    int output_channels) {
  CriticalSectionScoped crit_scoped(crit_);
  if (input_channels == num_input_channels_ &&
      output_channels == num_output_channels_) {
    return kNoError;
  }
  if (output_channels > input_channels) {
    return kBadParameterError;
  }
  // Only stereo supported currently.
  if (input_channels > 2 || input_channels < 1 ||
      output_channels > 2 || output_channels < 1) {
    return kBadParameterError;
  }

  num_input_channels_ = input_channels;
  num_output_channels_ = output_channels;

  return InitializeLocked();
}

int AudioProcessingImpl::num_input_channels() const {
  return num_input_channels_;
}

int AudioProcessingImpl::num_output_channels() const {
  return num_output_channels_;
}

void AudioProcessingImpl::set_output_will_be_muted(bool muted) {
  output_will_be_muted_ = muted;
}

bool AudioProcessingImpl::output_will_be_muted() const {
  return output_will_be_muted_;
}

int AudioProcessingImpl::MaybeInitializeLocked(int sample_rate_hz,
    int num_input_channels, int num_output_channels, int num_reverse_channels) {
  if (sample_rate_hz == sample_rate_hz_ &&
      num_input_channels == num_input_channels_ &&
      num_output_channels == num_output_channels_ &&
      num_reverse_channels == num_reverse_channels_) {
    return kNoError;
  }

  if (sample_rate_hz != kSampleRate8kHz &&
      sample_rate_hz != kSampleRate16kHz &&
      sample_rate_hz != kSampleRate32kHz) {
    return kBadSampleRateError;
  }
  if (num_output_channels > num_input_channels) {
    return kBadNumberChannelsError;
  }
  // Only mono and stereo supported currently.
  if (num_input_channels > 2 || num_input_channels < 1 ||
      num_output_channels > 2 || num_output_channels < 1 ||
      num_reverse_channels > 2 || num_reverse_channels < 1) {
    return kBadNumberChannelsError;
  }
  if (echo_control_mobile_->is_enabled() && sample_rate_hz > kSampleRate16kHz) {
    LOG(LS_ERROR) << "AECM only supports 16 or 8 kHz sample rates";
    return kUnsupportedComponentError;
  }

  sample_rate_hz_ = sample_rate_hz;
  samples_per_channel_ = kChunkSizeMs * sample_rate_hz / 1000;
  num_input_channels_ = num_input_channels;
  num_output_channels_ = num_output_channels;
  num_reverse_channels_ = num_reverse_channels;

  if (sample_rate_hz_ == kSampleRate32kHz) {
    split_sample_rate_hz_ = kSampleRate16kHz;
  } else {
    split_sample_rate_hz_ = sample_rate_hz_;
  }

  return InitializeLocked();
}

int AudioProcessingImpl::ProcessStream(AudioFrame* frame) {
  CriticalSectionScoped crit_scoped(crit_);
  int err = kNoError;

  if (frame == NULL) {
    return kNullPointerError;
  }
  // TODO(ajm): We now always set the output channels equal to the input
  // channels here. Remove the ability to downmix entirely.
  RETURN_ON_ERR(MaybeInitializeLocked(frame->sample_rate_hz_,
      frame->num_channels_, frame->num_channels_, num_reverse_channels_));
  if (frame->samples_per_channel_ != samples_per_channel_) {
    return kBadDataLengthError;
  }

#ifdef WEBRTC_AUDIOPROC_DEBUG_DUMP
  if (debug_file_->Open()) {
    event_msg_->set_type(audioproc::Event::STREAM);
    audioproc::Stream* msg = event_msg_->mutable_stream();
    const size_t data_size = sizeof(int16_t) *
                             frame->samples_per_channel_ *
                             frame->num_channels_;
    msg->set_input_data(frame->data_, data_size);
    msg->set_delay(stream_delay_ms_);
    msg->set_drift(echo_cancellation_->stream_drift_samples());
    msg->set_level(gain_control_->stream_analog_level());
    msg->set_keypress(key_pressed_);
  }
#endif

  capture_audio_->DeinterleaveFrom(frame);

  // TODO(ajm): experiment with mixing and AEC placement.
  if (num_output_channels_ < num_input_channels_) {
    capture_audio_->Mix(num_output_channels_);
    frame->num_channels_ = num_output_channels_;
  }

  bool data_processed = is_data_processed();
  if (analysis_needed(data_processed)) {
    for (int i = 0; i < num_output_channels_; i++) {
      // Split into a low and high band.
      WebRtcSpl_AnalysisQMF(capture_audio_->data(i),
                            capture_audio_->samples_per_channel(),
                            capture_audio_->low_pass_split_data(i),
                            capture_audio_->high_pass_split_data(i),
                            capture_audio_->analysis_filter_state1(i),
                            capture_audio_->analysis_filter_state2(i));
    }
  }

  err = high_pass_filter_->ProcessCaptureAudio(capture_audio_);
  if (err != kNoError) {
    return err;
  }

  err = gain_control_->AnalyzeCaptureAudio(capture_audio_);
  if (err != kNoError) {
    return err;
  }

  err = echo_cancellation_->ProcessCaptureAudio(capture_audio_);
  if (err != kNoError) {
    return err;
  }

  if (echo_control_mobile_->is_enabled() &&
      noise_suppression_->is_enabled()) {
    capture_audio_->CopyLowPassToReference();
  }

  err = noise_suppression_->ProcessCaptureAudio(capture_audio_);
  if (err != kNoError) {
    return err;
  }

  err = echo_control_mobile_->ProcessCaptureAudio(capture_audio_);
  if (err != kNoError) {
    return err;
  }

  err = voice_detection_->ProcessCaptureAudio(capture_audio_);
  if (err != kNoError) {
    return err;
  }

  err = gain_control_->ProcessCaptureAudio(capture_audio_);
  if (err != kNoError) {
    return err;
  }

  if (synthesis_needed(data_processed)) {
    for (int i = 0; i < num_output_channels_; i++) {
      // Recombine low and high bands.
      WebRtcSpl_SynthesisQMF(capture_audio_->low_pass_split_data(i),
                             capture_audio_->high_pass_split_data(i),
                             capture_audio_->samples_per_split_channel(),
                             capture_audio_->data(i),
                             capture_audio_->synthesis_filter_state1(i),
                             capture_audio_->synthesis_filter_state2(i));
    }
  }

  // The level estimator operates on the recombined data.
  err = level_estimator_->ProcessStream(capture_audio_);
  if (err != kNoError) {
    return err;
  }

  capture_audio_->InterleaveTo(frame, interleave_needed(data_processed));

#ifdef WEBRTC_AUDIOPROC_DEBUG_DUMP
  if (debug_file_->Open()) {
    audioproc::Stream* msg = event_msg_->mutable_stream();
    const size_t data_size = sizeof(int16_t) *
                             frame->samples_per_channel_ *
                             frame->num_channels_;
    msg->set_output_data(frame->data_, data_size);
    err = WriteMessageToDebugFile();
    if (err != kNoError) {
      return err;
    }
  }
#endif

  was_stream_delay_set_ = false;
  return kNoError;
}

// TODO(ajm): Have AnalyzeReverseStream accept sample rates not matching the
// primary stream and convert ourselves rather than having the user manage it.
// We can be smarter and use the splitting filter when appropriate. Similarly,
// perform downmixing here.
int AudioProcessingImpl::AnalyzeReverseStream(AudioFrame* frame) {
  CriticalSectionScoped crit_scoped(crit_);
  int err = kNoError;
  if (frame == NULL) {
    return kNullPointerError;
  }
  if (frame->sample_rate_hz_ != sample_rate_hz_) {
    return kBadSampleRateError;
  }
  RETURN_ON_ERR(MaybeInitializeLocked(sample_rate_hz_, num_input_channels_,
      num_output_channels_, frame->num_channels_));

#ifdef WEBRTC_AUDIOPROC_DEBUG_DUMP
  if (debug_file_->Open()) {
    event_msg_->set_type(audioproc::Event::REVERSE_STREAM);
    audioproc::ReverseStream* msg = event_msg_->mutable_reverse_stream();
    const size_t data_size = sizeof(int16_t) *
                             frame->samples_per_channel_ *
                             frame->num_channels_;
    msg->set_data(frame->data_, data_size);
    err = WriteMessageToDebugFile();
    if (err != kNoError) {
      return err;
    }
  }
#endif

  render_audio_->DeinterleaveFrom(frame);

  if (sample_rate_hz_ == kSampleRate32kHz) {
    for (int i = 0; i < num_reverse_channels_; i++) {
      // Split into low and high band.
      WebRtcSpl_AnalysisQMF(render_audio_->data(i),
                            render_audio_->samples_per_channel(),
                            render_audio_->low_pass_split_data(i),
                            render_audio_->high_pass_split_data(i),
                            render_audio_->analysis_filter_state1(i),
                            render_audio_->analysis_filter_state2(i));
    }
  }

  // TODO(ajm): warnings possible from components?
  err = echo_cancellation_->ProcessRenderAudio(render_audio_);
  if (err != kNoError) {
    return err;
  }

  err = echo_control_mobile_->ProcessRenderAudio(render_audio_);
  if (err != kNoError) {
    return err;
  }

  err = gain_control_->ProcessRenderAudio(render_audio_);
  if (err != kNoError) {
    return err;
  }

  return err;  // TODO(ajm): this is for returning warnings; necessary?
}

int AudioProcessingImpl::set_stream_delay_ms(int delay) {
  Error retval = kNoError;
  was_stream_delay_set_ = true;
  delay += delay_offset_ms_;

  if (delay < 0) {
    delay = 0;
    retval = kBadStreamParameterWarning;
  }

  // TODO(ajm): the max is rather arbitrarily chosen; investigate.
  if (delay > 500) {
    delay = 500;
    retval = kBadStreamParameterWarning;
  }

  stream_delay_ms_ = delay;
  return retval;
}

int AudioProcessingImpl::stream_delay_ms() const {
  return stream_delay_ms_;
}

bool AudioProcessingImpl::was_stream_delay_set() const {
  return was_stream_delay_set_;
}

void AudioProcessingImpl::set_delay_offset_ms(int offset) {
  CriticalSectionScoped crit_scoped(crit_);
  delay_offset_ms_ = offset;
}

int AudioProcessingImpl::delay_offset_ms() const {
  return delay_offset_ms_;
}

void AudioProcessingImpl::set_stream_key_pressed(bool key_pressed) {
  key_pressed_ = key_pressed;
}

bool AudioProcessingImpl::stream_key_pressed() const {
  return key_pressed_;
}

int AudioProcessingImpl::StartDebugRecording(
    const char filename[AudioProcessing::kMaxFilenameSize]) {
  CriticalSectionScoped crit_scoped(crit_);
  assert(kMaxFilenameSize == FileWrapper::kMaxFileNameSize);

  if (filename == NULL) {
    return kNullPointerError;
  }

#ifdef WEBRTC_AUDIOPROC_DEBUG_DUMP
  // Stop any ongoing recording.
  if (debug_file_->Open()) {
    if (debug_file_->CloseFile() == -1) {
      return kFileError;
    }
  }

  if (debug_file_->OpenFile(filename, false) == -1) {
    debug_file_->CloseFile();
    return kFileError;
  }

  int err = WriteInitMessage();
  if (err != kNoError) {
    return err;
  }
  return kNoError;
#else
  return kUnsupportedFunctionError;
#endif  // WEBRTC_AUDIOPROC_DEBUG_DUMP
}

int AudioProcessingImpl::StartDebugRecording(FILE* handle) {
  CriticalSectionScoped crit_scoped(crit_);

  if (handle == NULL) {
    return kNullPointerError;
  }

#ifdef WEBRTC_AUDIOPROC_DEBUG_DUMP
  // Stop any ongoing recording.
  if (debug_file_->Open()) {
    if (debug_file_->CloseFile() == -1) {
      return kFileError;
    }
  }

  if (debug_file_->OpenFromFileHandle(handle, true, false) == -1) {
    return kFileError;
  }

  int err = WriteInitMessage();
  if (err != kNoError) {
    return err;
  }
  return kNoError;
#else
  return kUnsupportedFunctionError;
#endif  // WEBRTC_AUDIOPROC_DEBUG_DUMP
}

int AudioProcessingImpl::StopDebugRecording() {
  CriticalSectionScoped crit_scoped(crit_);

#ifdef WEBRTC_AUDIOPROC_DEBUG_DUMP
  // We just return if recording hasn't started.
  if (debug_file_->Open()) {
    if (debug_file_->CloseFile() == -1) {
      return kFileError;
    }
  }
  return kNoError;
#else
  return kUnsupportedFunctionError;
#endif  // WEBRTC_AUDIOPROC_DEBUG_DUMP
}

EchoCancellation* AudioProcessingImpl::echo_cancellation() const {
  return echo_cancellation_;
}

EchoControlMobile* AudioProcessingImpl::echo_control_mobile() const {
  return echo_control_mobile_;
}

GainControl* AudioProcessingImpl::gain_control() const {
  return gain_control_;
}

HighPassFilter* AudioProcessingImpl::high_pass_filter() const {
  return high_pass_filter_;
}

LevelEstimator* AudioProcessingImpl::level_estimator() const {
  return level_estimator_;
}

NoiseSuppression* AudioProcessingImpl::noise_suppression() const {
  return noise_suppression_;
}

VoiceDetection* AudioProcessingImpl::voice_detection() const {
  return voice_detection_;
}

bool AudioProcessingImpl::is_data_processed() const {
  int enabled_count = 0;
  std::list<ProcessingComponent*>::const_iterator it;
  for (it = component_list_.begin(); it != component_list_.end(); it++) {
    if ((*it)->is_component_enabled()) {
      enabled_count++;
    }
  }

  // Data is unchanged if no components are enabled, or if only level_estimator_
  // or voice_detection_ is enabled.
  if (enabled_count == 0) {
    return false;
  } else if (enabled_count == 1) {
    if (level_estimator_->is_enabled() || voice_detection_->is_enabled()) {
      return false;
    }
  } else if (enabled_count == 2) {
    if (level_estimator_->is_enabled() && voice_detection_->is_enabled()) {
      return false;
    }
  }
  return true;
}

bool AudioProcessingImpl::interleave_needed(bool is_data_processed) const {
  // Check if we've upmixed or downmixed the audio.
  return (num_output_channels_ != num_input_channels_ || is_data_processed);
}

bool AudioProcessingImpl::synthesis_needed(bool is_data_processed) const {
  return (is_data_processed && sample_rate_hz_ == kSampleRate32kHz);
}

bool AudioProcessingImpl::analysis_needed(bool is_data_processed) const {
  if (!is_data_processed && !voice_detection_->is_enabled()) {
    // Only level_estimator_ is enabled.
    return false;
  } else if (sample_rate_hz_ == kSampleRate32kHz) {
    // Something besides level_estimator_ is enabled, and we have super-wb.
    return true;
  }
  return false;
}

#ifdef WEBRTC_AUDIOPROC_DEBUG_DUMP
int AudioProcessingImpl::WriteMessageToDebugFile() {
  int32_t size = event_msg_->ByteSize();
  if (size <= 0) {
    return kUnspecifiedError;
  }
#if defined(WEBRTC_ARCH_BIG_ENDIAN)
  // TODO(ajm): Use little-endian "on the wire". For the moment, we can be
  //            pretty safe in assuming little-endian.
#endif

  if (!event_msg_->SerializeToString(&event_str_)) {
    return kUnspecifiedError;
  }

  // Write message preceded by its size.
  if (!debug_file_->Write(&size, sizeof(int32_t))) {
    return kFileError;
  }
  if (!debug_file_->Write(event_str_.data(), event_str_.length())) {
    return kFileError;
  }

  event_msg_->Clear();

  return 0;
}

int AudioProcessingImpl::WriteInitMessage() {
  event_msg_->set_type(audioproc::Event::INIT);
  audioproc::Init* msg = event_msg_->mutable_init();
  msg->set_sample_rate(sample_rate_hz_);
  msg->set_device_sample_rate(echo_cancellation_->device_sample_rate_hz());
  msg->set_num_input_channels(num_input_channels_);
  msg->set_num_output_channels(num_output_channels_);
  msg->set_num_reverse_channels(num_reverse_channels_);

  int err = WriteMessageToDebugFile();
  if (err != kNoError) {
    return err;
  }

  return kNoError;
}
#endif  // WEBRTC_AUDIOPROC_DEBUG_DUMP
}  // namespace webrtc
