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

#include "webrtc/base/platform_file.h"
#include "webrtc/common_audio/include/audio_util.h"
#include "webrtc/common_audio/signal_processing/include/signal_processing_library.h"
#include "webrtc/modules/audio_processing/audio_buffer.h"
#include "webrtc/modules/audio_processing/common.h"
#include "webrtc/modules/audio_processing/echo_cancellation_impl.h"
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
#ifdef WEBRTC_AUDIOPROC_DEBUG_DUMP
      debug_file_(FileWrapper::Create()),
      event_msg_(new audioproc::Event()),
#endif
      fwd_in_format_(kSampleRate16kHz, 1),
      fwd_proc_format_(kSampleRate16kHz, 1),
      fwd_out_format_(kSampleRate16kHz),
      rev_in_format_(kSampleRate16kHz, 1),
      rev_proc_format_(kSampleRate16kHz, 1),
      split_rate_(kSampleRate16kHz),
      stream_delay_ms_(0),
      delay_offset_ms_(0),
      was_stream_delay_set_(false),
      output_will_be_muted_(false),
      key_pressed_(false) {
  echo_cancellation_ = new EchoCancellationImpl(this, crit_);
  component_list_.push_back(echo_cancellation_);

  echo_control_mobile_ = new EchoControlMobileImpl(this, crit_);
  component_list_.push_back(echo_control_mobile_);

  gain_control_ = new GainControlImpl(this, crit_);
  component_list_.push_back(gain_control_);

  high_pass_filter_ = new HighPassFilterImpl(this, crit_);
  component_list_.push_back(high_pass_filter_);

  level_estimator_ = new LevelEstimatorImpl(this, crit_);
  component_list_.push_back(level_estimator_);

  noise_suppression_ = new NoiseSuppressionImpl(this, crit_);
  component_list_.push_back(noise_suppression_);

  voice_detection_ = new VoiceDetectionImpl(this, crit_);
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
  }
  delete crit_;
  crit_ = NULL;
}

int AudioProcessingImpl::Initialize() {
  CriticalSectionScoped crit_scoped(crit_);
  return InitializeLocked();
}

int AudioProcessingImpl::set_sample_rate_hz(int rate) {
  CriticalSectionScoped crit_scoped(crit_);
  return InitializeLocked(rate,
                          rate,
                          rev_in_format_.rate(),
                          fwd_in_format_.num_channels(),
                          fwd_proc_format_.num_channels(),
                          rev_in_format_.num_channels());
}

int AudioProcessingImpl::Initialize(int input_sample_rate_hz,
                                    int output_sample_rate_hz,
                                    int reverse_sample_rate_hz,
                                    ChannelLayout input_layout,
                                    ChannelLayout output_layout,
                                    ChannelLayout reverse_layout) {
  CriticalSectionScoped crit_scoped(crit_);
  return InitializeLocked(input_sample_rate_hz,
                          output_sample_rate_hz,
                          reverse_sample_rate_hz,
                          ChannelsFromLayout(input_layout),
                          ChannelsFromLayout(output_layout),
                          ChannelsFromLayout(reverse_layout));
}

int AudioProcessingImpl::InitializeLocked() {
  render_audio_.reset(new AudioBuffer(rev_in_format_.samples_per_channel(),
                                      rev_in_format_.num_channels(),
                                      rev_proc_format_.samples_per_channel(),
                                      rev_proc_format_.num_channels(),
                                      rev_proc_format_.samples_per_channel()));
  capture_audio_.reset(new AudioBuffer(fwd_in_format_.samples_per_channel(),
                                       fwd_in_format_.num_channels(),
                                       fwd_proc_format_.samples_per_channel(),
                                       fwd_proc_format_.num_channels(),
                                       fwd_out_format_.samples_per_channel()));

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

int AudioProcessingImpl::InitializeLocked(int input_sample_rate_hz,
                                          int output_sample_rate_hz,
                                          int reverse_sample_rate_hz,
                                          int num_input_channels,
                                          int num_output_channels,
                                          int num_reverse_channels) {
  if (input_sample_rate_hz <= 0 ||
      output_sample_rate_hz <= 0 ||
      reverse_sample_rate_hz <= 0) {
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

  fwd_in_format_.set(input_sample_rate_hz, num_input_channels);
  fwd_out_format_.set(output_sample_rate_hz);
  rev_in_format_.set(reverse_sample_rate_hz, num_reverse_channels);

  // We process at the closest native rate >= min(input rate, output rate)...
  int min_proc_rate = std::min(fwd_in_format_.rate(), fwd_out_format_.rate());
  int fwd_proc_rate;
  if (min_proc_rate > kSampleRate16kHz) {
    fwd_proc_rate = kSampleRate32kHz;
  } else if (min_proc_rate > kSampleRate8kHz) {
    fwd_proc_rate = kSampleRate16kHz;
  } else {
    fwd_proc_rate = kSampleRate8kHz;
  }
  // ...with one exception.
  if (echo_control_mobile_->is_enabled() && min_proc_rate > kSampleRate16kHz) {
    fwd_proc_rate = kSampleRate16kHz;
  }

  fwd_proc_format_.set(fwd_proc_rate, num_output_channels);

  // We normally process the reverse stream at 16 kHz. Unless...
  int rev_proc_rate = kSampleRate16kHz;
  if (fwd_proc_format_.rate() == kSampleRate8kHz) {
    // ...the forward stream is at 8 kHz.
    rev_proc_rate = kSampleRate8kHz;
  } else {
    if (rev_in_format_.rate() == kSampleRate32kHz) {
      // ...or the input is at 32 kHz, in which case we use the splitting
      // filter rather than the resampler.
      rev_proc_rate = kSampleRate32kHz;
    }
  }

  // Always downmix the reverse stream to mono for analysis. This has been
  // demonstrated to work well for AEC in most practical scenarios.
  rev_proc_format_.set(rev_proc_rate, 1);

  if (fwd_proc_format_.rate() == kSampleRate32kHz) {
    split_rate_ = kSampleRate16kHz;
  } else {
    split_rate_ = fwd_proc_format_.rate();
  }

  return InitializeLocked();
}

// Calls InitializeLocked() if any of the audio parameters have changed from
// their current values.
int AudioProcessingImpl::MaybeInitializeLocked(int input_sample_rate_hz,
                                               int output_sample_rate_hz,
                                               int reverse_sample_rate_hz,
                                               int num_input_channels,
                                               int num_output_channels,
                                               int num_reverse_channels) {
  if (input_sample_rate_hz == fwd_in_format_.rate() &&
      output_sample_rate_hz == fwd_out_format_.rate() &&
      reverse_sample_rate_hz == rev_in_format_.rate() &&
      num_input_channels == fwd_in_format_.num_channels() &&
      num_output_channels == fwd_proc_format_.num_channels() &&
      num_reverse_channels == rev_in_format_.num_channels()) {
    return kNoError;
  }

  return InitializeLocked(input_sample_rate_hz,
                          output_sample_rate_hz,
                          reverse_sample_rate_hz,
                          num_input_channels,
                          num_output_channels,
                          num_reverse_channels);
}

void AudioProcessingImpl::SetExtraOptions(const Config& config) {
  CriticalSectionScoped crit_scoped(crit_);
  std::list<ProcessingComponent*>::iterator it;
  for (it = component_list_.begin(); it != component_list_.end(); ++it)
    (*it)->SetExtraOptions(config);
}

int AudioProcessingImpl::input_sample_rate_hz() const {
  CriticalSectionScoped crit_scoped(crit_);
  return fwd_in_format_.rate();
}

int AudioProcessingImpl::sample_rate_hz() const {
  CriticalSectionScoped crit_scoped(crit_);
  return fwd_in_format_.rate();
}

int AudioProcessingImpl::proc_sample_rate_hz() const {
  return fwd_proc_format_.rate();
}

int AudioProcessingImpl::proc_split_sample_rate_hz() const {
  return split_rate_;
}

int AudioProcessingImpl::num_reverse_channels() const {
  return rev_proc_format_.num_channels();
}

int AudioProcessingImpl::num_input_channels() const {
  return fwd_in_format_.num_channels();
}

int AudioProcessingImpl::num_output_channels() const {
  return fwd_proc_format_.num_channels();
}

void AudioProcessingImpl::set_output_will_be_muted(bool muted) {
  output_will_be_muted_ = muted;
}

bool AudioProcessingImpl::output_will_be_muted() const {
  return output_will_be_muted_;
}

int AudioProcessingImpl::ProcessStream(const float* const* src,
                                       int samples_per_channel,
                                       int input_sample_rate_hz,
                                       ChannelLayout input_layout,
                                       int output_sample_rate_hz,
                                       ChannelLayout output_layout,
                                       float* const* dest) {
  CriticalSectionScoped crit_scoped(crit_);
  if (!src || !dest) {
    return kNullPointerError;
  }

  RETURN_ON_ERR(MaybeInitializeLocked(input_sample_rate_hz,
                                      output_sample_rate_hz,
                                      rev_in_format_.rate(),
                                      ChannelsFromLayout(input_layout),
                                      ChannelsFromLayout(output_layout),
                                      rev_in_format_.num_channels()));
  if (samples_per_channel != fwd_in_format_.samples_per_channel()) {
    return kBadDataLengthError;
  }

#ifdef WEBRTC_AUDIOPROC_DEBUG_DUMP
  if (debug_file_->Open()) {
    event_msg_->set_type(audioproc::Event::STREAM);
    audioproc::Stream* msg = event_msg_->mutable_stream();
    const size_t channel_size =
        sizeof(float) * fwd_in_format_.samples_per_channel();
    for (int i = 0; i < fwd_in_format_.num_channels(); ++i)
      msg->add_input_channel(src[i], channel_size);
  }
#endif

  capture_audio_->CopyFrom(src, samples_per_channel, input_layout);
  RETURN_ON_ERR(ProcessStreamLocked());
  if (output_copy_needed(is_data_processed())) {
    capture_audio_->CopyTo(fwd_out_format_.samples_per_channel(),
                           output_layout,
                           dest);
  }

#ifdef WEBRTC_AUDIOPROC_DEBUG_DUMP
  if (debug_file_->Open()) {
    audioproc::Stream* msg = event_msg_->mutable_stream();
    const size_t channel_size =
        sizeof(float) * fwd_out_format_.samples_per_channel();
    for (int i = 0; i < fwd_proc_format_.num_channels(); ++i)
      msg->add_output_channel(dest[i], channel_size);
    RETURN_ON_ERR(WriteMessageToDebugFile());
  }
#endif

  return kNoError;
}

int AudioProcessingImpl::ProcessStream(AudioFrame* frame) {
  CriticalSectionScoped crit_scoped(crit_);
  if (!frame) {
    return kNullPointerError;
  }
  // Must be a native rate.
  if (frame->sample_rate_hz_ != kSampleRate8kHz &&
      frame->sample_rate_hz_ != kSampleRate16kHz &&
      frame->sample_rate_hz_ != kSampleRate32kHz) {
    return kBadSampleRateError;
  }
  if (echo_control_mobile_->is_enabled() &&
      frame->sample_rate_hz_ > kSampleRate16kHz) {
    LOG(LS_ERROR) << "AECM only supports 16 or 8 kHz sample rates";
    return kUnsupportedComponentError;
  }

  // TODO(ajm): The input and output rates and channels are currently
  // constrained to be identical in the int16 interface.
  RETURN_ON_ERR(MaybeInitializeLocked(frame->sample_rate_hz_,
                                      frame->sample_rate_hz_,
                                      rev_in_format_.rate(),
                                      frame->num_channels_,
                                      frame->num_channels_,
                                      rev_in_format_.num_channels()));
  if (frame->samples_per_channel_ != fwd_in_format_.samples_per_channel()) {
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
  }
#endif

  capture_audio_->DeinterleaveFrom(frame);
  RETURN_ON_ERR(ProcessStreamLocked());
  capture_audio_->InterleaveTo(frame, output_copy_needed(is_data_processed()));

#ifdef WEBRTC_AUDIOPROC_DEBUG_DUMP
  if (debug_file_->Open()) {
    audioproc::Stream* msg = event_msg_->mutable_stream();
    const size_t data_size = sizeof(int16_t) *
                             frame->samples_per_channel_ *
                             frame->num_channels_;
    msg->set_output_data(frame->data_, data_size);
    RETURN_ON_ERR(WriteMessageToDebugFile());
  }
#endif

  return kNoError;
}


int AudioProcessingImpl::ProcessStreamLocked() {
#ifdef WEBRTC_AUDIOPROC_DEBUG_DUMP
  if (debug_file_->Open()) {
    audioproc::Stream* msg = event_msg_->mutable_stream();
    msg->set_delay(stream_delay_ms_);
    msg->set_drift(echo_cancellation_->stream_drift_samples());
    msg->set_level(gain_control_->stream_analog_level());
    msg->set_keypress(key_pressed_);
  }
#endif

  AudioBuffer* ca = capture_audio_.get();  // For brevity.
  bool data_processed = is_data_processed();
  if (analysis_needed(data_processed)) {
    for (int i = 0; i < fwd_proc_format_.num_channels(); i++) {
      // Split into a low and high band.
      WebRtcSpl_AnalysisQMF(ca->data(i),
                            ca->samples_per_channel(),
                            ca->low_pass_split_data(i),
                            ca->high_pass_split_data(i),
                            ca->filter_states(i)->analysis_filter_state1,
                            ca->filter_states(i)->analysis_filter_state2);
    }
  }

  RETURN_ON_ERR(high_pass_filter_->ProcessCaptureAudio(ca));
  RETURN_ON_ERR(gain_control_->AnalyzeCaptureAudio(ca));
  RETURN_ON_ERR(noise_suppression_->AnalyzeCaptureAudio(ca));
  RETURN_ON_ERR(echo_cancellation_->ProcessCaptureAudio(ca));

  if (echo_control_mobile_->is_enabled() && noise_suppression_->is_enabled()) {
    ca->CopyLowPassToReference();
  }
  RETURN_ON_ERR(noise_suppression_->ProcessCaptureAudio(ca));
  RETURN_ON_ERR(echo_control_mobile_->ProcessCaptureAudio(ca));
  RETURN_ON_ERR(voice_detection_->ProcessCaptureAudio(ca));
  RETURN_ON_ERR(gain_control_->ProcessCaptureAudio(ca));

  if (synthesis_needed(data_processed)) {
    for (int i = 0; i < fwd_proc_format_.num_channels(); i++) {
      // Recombine low and high bands.
      WebRtcSpl_SynthesisQMF(ca->low_pass_split_data(i),
                             ca->high_pass_split_data(i),
                             ca->samples_per_split_channel(),
                             ca->data(i),
                             ca->filter_states(i)->synthesis_filter_state1,
                             ca->filter_states(i)->synthesis_filter_state2);
    }
  }

  // The level estimator operates on the recombined data.
  RETURN_ON_ERR(level_estimator_->ProcessStream(ca));

  was_stream_delay_set_ = false;
  return kNoError;
}

int AudioProcessingImpl::AnalyzeReverseStream(const float* const* data,
                                              int samples_per_channel,
                                              int sample_rate_hz,
                                              ChannelLayout layout) {
  CriticalSectionScoped crit_scoped(crit_);
  if (data == NULL) {
    return kNullPointerError;
  }

  const int num_channels = ChannelsFromLayout(layout);
  RETURN_ON_ERR(MaybeInitializeLocked(fwd_in_format_.rate(),
                                      fwd_out_format_.rate(),
                                      sample_rate_hz,
                                      fwd_in_format_.num_channels(),
                                      fwd_proc_format_.num_channels(),
                                      num_channels));
  if (samples_per_channel != rev_in_format_.samples_per_channel()) {
    return kBadDataLengthError;
  }

#ifdef WEBRTC_AUDIOPROC_DEBUG_DUMP
  if (debug_file_->Open()) {
    event_msg_->set_type(audioproc::Event::REVERSE_STREAM);
    audioproc::ReverseStream* msg = event_msg_->mutable_reverse_stream();
    const size_t channel_size =
        sizeof(float) * rev_in_format_.samples_per_channel();
    for (int i = 0; i < num_channels; ++i)
      msg->add_channel(data[i], channel_size);
    RETURN_ON_ERR(WriteMessageToDebugFile());
  }
#endif

  render_audio_->CopyFrom(data, samples_per_channel, layout);
  return AnalyzeReverseStreamLocked();
}

int AudioProcessingImpl::AnalyzeReverseStream(AudioFrame* frame) {
  CriticalSectionScoped crit_scoped(crit_);
  if (frame == NULL) {
    return kNullPointerError;
  }
  // Must be a native rate.
  if (frame->sample_rate_hz_ != kSampleRate8kHz &&
      frame->sample_rate_hz_ != kSampleRate16kHz &&
      frame->sample_rate_hz_ != kSampleRate32kHz) {
    return kBadSampleRateError;
  }
  // This interface does not tolerate different forward and reverse rates.
  if (frame->sample_rate_hz_ != fwd_in_format_.rate()) {
    return kBadSampleRateError;
  }

  RETURN_ON_ERR(MaybeInitializeLocked(fwd_in_format_.rate(),
                                      fwd_out_format_.rate(),
                                      frame->sample_rate_hz_,
                                      fwd_in_format_.num_channels(),
                                      fwd_in_format_.num_channels(),
                                      frame->num_channels_));
  if (frame->samples_per_channel_ != rev_in_format_.samples_per_channel()) {
    return kBadDataLengthError;
  }

#ifdef WEBRTC_AUDIOPROC_DEBUG_DUMP
  if (debug_file_->Open()) {
    event_msg_->set_type(audioproc::Event::REVERSE_STREAM);
    audioproc::ReverseStream* msg = event_msg_->mutable_reverse_stream();
    const size_t data_size = sizeof(int16_t) *
                             frame->samples_per_channel_ *
                             frame->num_channels_;
    msg->set_data(frame->data_, data_size);
    RETURN_ON_ERR(WriteMessageToDebugFile());
  }
#endif

  render_audio_->DeinterleaveFrom(frame);
  return AnalyzeReverseStreamLocked();
}

int AudioProcessingImpl::AnalyzeReverseStreamLocked() {
  AudioBuffer* ra = render_audio_.get();  // For brevity.
  if (rev_proc_format_.rate() == kSampleRate32kHz) {
    for (int i = 0; i < rev_proc_format_.num_channels(); i++) {
      // Split into low and high band.
      WebRtcSpl_AnalysisQMF(ra->data(i),
                            ra->samples_per_channel(),
                            ra->low_pass_split_data(i),
                            ra->high_pass_split_data(i),
                            ra->filter_states(i)->analysis_filter_state1,
                            ra->filter_states(i)->analysis_filter_state2);
    }
  }

  RETURN_ON_ERR(echo_cancellation_->ProcessRenderAudio(ra));
  RETURN_ON_ERR(echo_control_mobile_->ProcessRenderAudio(ra));
  RETURN_ON_ERR(gain_control_->ProcessRenderAudio(ra));

  return kNoError;
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

void AudioProcessingImpl::set_stream_key_pressed(bool key_pressed) {
  key_pressed_ = key_pressed;
}

bool AudioProcessingImpl::stream_key_pressed() const {
  return key_pressed_;
}

void AudioProcessingImpl::set_delay_offset_ms(int offset) {
  CriticalSectionScoped crit_scoped(crit_);
  delay_offset_ms_ = offset;
}

int AudioProcessingImpl::delay_offset_ms() const {
  return delay_offset_ms_;
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

int AudioProcessingImpl::StartDebugRecordingForPlatformFile(
    rtc::PlatformFile handle) {
  FILE* stream = rtc::FdopenPlatformFileForWriting(handle);
  return StartDebugRecording(stream);
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

bool AudioProcessingImpl::output_copy_needed(bool is_data_processed) const {
  // Check if we've upmixed or downmixed the audio.
  return ((fwd_proc_format_.num_channels() != fwd_in_format_.num_channels()) ||
          is_data_processed);
}

bool AudioProcessingImpl::synthesis_needed(bool is_data_processed) const {
  return (is_data_processed && fwd_proc_format_.rate() == kSampleRate32kHz);
}

bool AudioProcessingImpl::analysis_needed(bool is_data_processed) const {
  if (!is_data_processed && !voice_detection_->is_enabled()) {
    // Only level_estimator_ is enabled.
    return false;
  } else if (fwd_proc_format_.rate() == kSampleRate32kHz) {
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

  return kNoError;
}

int AudioProcessingImpl::WriteInitMessage() {
  event_msg_->set_type(audioproc::Event::INIT);
  audioproc::Init* msg = event_msg_->mutable_init();
  msg->set_sample_rate(fwd_in_format_.rate());
  msg->set_num_input_channels(fwd_in_format_.num_channels());
  msg->set_num_output_channels(fwd_proc_format_.num_channels());
  msg->set_num_reverse_channels(rev_in_format_.num_channels());
  msg->set_reverse_sample_rate(rev_in_format_.rate());
  msg->set_output_sample_rate(fwd_out_format_.rate());

  int err = WriteMessageToDebugFile();
  if (err != kNoError) {
    return err;
  }

  return kNoError;
}
#endif  // WEBRTC_AUDIOPROC_DEBUG_DUMP

}  // namespace webrtc
