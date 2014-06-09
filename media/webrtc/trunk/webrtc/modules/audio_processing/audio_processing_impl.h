/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_PROCESSING_MAIN_SOURCE_AUDIO_PROCESSING_IMPL_H_
#define WEBRTC_MODULES_AUDIO_PROCESSING_MAIN_SOURCE_AUDIO_PROCESSING_IMPL_H_

#include "webrtc/modules/audio_processing/include/audio_processing.h"

#include <list>
#include <string>

#include "webrtc/system_wrappers/interface/scoped_ptr.h"

namespace webrtc {
class AudioBuffer;
class CriticalSectionWrapper;
class EchoCancellationImplWrapper;
class EchoControlMobileImpl;
class FileWrapper;
class GainControlImpl;
class HighPassFilterImpl;
class LevelEstimatorImpl;
class NoiseSuppressionImpl;
class ProcessingComponent;
class VoiceDetectionImpl;

#ifdef WEBRTC_AUDIOPROC_DEBUG_DUMP
namespace audioproc {

class Event;

}  // namespace audioproc
#endif

class AudioProcessingImpl : public AudioProcessing {
 public:
  enum {
    kSampleRate8kHz = 8000,
    kSampleRate16kHz = 16000,
    kSampleRate32kHz = 32000
  };

  explicit AudioProcessingImpl(const Config& config);
  virtual ~AudioProcessingImpl();

  CriticalSectionWrapper* crit() const;

  int split_sample_rate_hz() const;
  bool was_stream_delay_set() const;

  // AudioProcessing methods.
  virtual int Initialize() OVERRIDE;
  virtual void SetExtraOptions(const Config& config) OVERRIDE;
  virtual int EnableExperimentalNs(bool enable) OVERRIDE;
  virtual bool experimental_ns_enabled() const OVERRIDE {
    return false;
  }
  virtual int set_sample_rate_hz(int rate) OVERRIDE;
  virtual int sample_rate_hz() const OVERRIDE;
  virtual int set_num_channels(int input_channels,
                               int output_channels) OVERRIDE;
  virtual int num_input_channels() const OVERRIDE;
  virtual int num_output_channels() const OVERRIDE;
  virtual int set_num_reverse_channels(int channels) OVERRIDE;
  virtual int num_reverse_channels() const OVERRIDE;
  virtual void set_output_will_be_muted(bool muted) OVERRIDE;
  virtual bool output_will_be_muted() const OVERRIDE;
  virtual int ProcessStream(AudioFrame* frame) OVERRIDE;
  virtual int AnalyzeReverseStream(AudioFrame* frame) OVERRIDE;
  virtual int set_stream_delay_ms(int delay) OVERRIDE;
  virtual int stream_delay_ms() const OVERRIDE;
  virtual void set_delay_offset_ms(int offset) OVERRIDE;
  virtual int delay_offset_ms() const OVERRIDE;
  virtual void set_stream_key_pressed(bool key_pressed) OVERRIDE;
  virtual bool stream_key_pressed() const OVERRIDE;
  virtual int StartDebugRecording(
      const char filename[kMaxFilenameSize]) OVERRIDE;
  virtual int StartDebugRecording(FILE* handle) OVERRIDE;
  virtual int StopDebugRecording() OVERRIDE;
  virtual EchoCancellation* echo_cancellation() const OVERRIDE;
  virtual EchoControlMobile* echo_control_mobile() const OVERRIDE;
  virtual GainControl* gain_control() const OVERRIDE;
  virtual HighPassFilter* high_pass_filter() const OVERRIDE;
  virtual LevelEstimator* level_estimator() const OVERRIDE;
  virtual NoiseSuppression* noise_suppression() const OVERRIDE;
  virtual VoiceDetection* voice_detection() const OVERRIDE;

 protected:
  virtual int InitializeLocked();

 private:
  int MaybeInitializeLocked(int sample_rate_hz, int num_input_channels,
                            int num_output_channels, int num_reverse_channels);
  bool is_data_processed() const;
  bool interleave_needed(bool is_data_processed) const;
  bool synthesis_needed(bool is_data_processed) const;
  bool analysis_needed(bool is_data_processed) const;

  EchoCancellationImplWrapper* echo_cancellation_;
  EchoControlMobileImpl* echo_control_mobile_;
  GainControlImpl* gain_control_;
  HighPassFilterImpl* high_pass_filter_;
  LevelEstimatorImpl* level_estimator_;
  NoiseSuppressionImpl* noise_suppression_;
  VoiceDetectionImpl* voice_detection_;

  std::list<ProcessingComponent*> component_list_;
  CriticalSectionWrapper* crit_;
  AudioBuffer* render_audio_;
  AudioBuffer* capture_audio_;
#ifdef WEBRTC_AUDIOPROC_DEBUG_DUMP
  // TODO(andrew): make this more graceful. Ideally we would split this stuff
  // out into a separate class with an "enabled" and "disabled" implementation.
  int WriteMessageToDebugFile();
  int WriteInitMessage();
  scoped_ptr<FileWrapper> debug_file_;
  scoped_ptr<audioproc::Event> event_msg_;  // Protobuf message.
  std::string event_str_;  // Memory for protobuf serialization.
#endif

  int sample_rate_hz_;
  int split_sample_rate_hz_;
  int samples_per_channel_;
  int stream_delay_ms_;
  int delay_offset_ms_;
  bool was_stream_delay_set_;

  int num_reverse_channels_;
  int num_input_channels_;
  int num_output_channels_;
  bool output_will_be_muted_;

  bool key_pressed_;
};
}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_PROCESSING_MAIN_SOURCE_AUDIO_PROCESSING_IMPL_H_
