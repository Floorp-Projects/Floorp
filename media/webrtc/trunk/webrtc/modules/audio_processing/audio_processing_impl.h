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
class EchoCancellationImpl;
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

class AudioRate {
 public:
  explicit AudioRate(int sample_rate_hz)
      : rate_(sample_rate_hz),
        samples_per_channel_(AudioProcessing::kChunkSizeMs * rate_ / 1000) {}
  virtual ~AudioRate() {}

  void set(int rate) {
    rate_ = rate;
    samples_per_channel_ = AudioProcessing::kChunkSizeMs * rate_ / 1000;
  }

  int rate() const { return rate_; }
  int samples_per_channel() const { return samples_per_channel_; }

 private:
  int rate_;
  int samples_per_channel_;
};

class AudioFormat : public AudioRate {
 public:
  AudioFormat(int sample_rate_hz, int num_channels)
      : AudioRate(sample_rate_hz),
        num_channels_(num_channels) {}
  virtual ~AudioFormat() {}

  void set(int rate, int num_channels) {
    AudioRate::set(rate);
    num_channels_ = num_channels;
  }

  int num_channels() const { return num_channels_; }

 private:
  int num_channels_;
};

class AudioProcessingImpl : public AudioProcessing {
 public:
  explicit AudioProcessingImpl(const Config& config);
  virtual ~AudioProcessingImpl();

  // AudioProcessing methods.
  virtual int Initialize() OVERRIDE;
  virtual int Initialize(int input_sample_rate_hz,
                         int output_sample_rate_hz,
                         int reverse_sample_rate_hz,
                         ChannelLayout input_layout,
                         ChannelLayout output_layout,
                         ChannelLayout reverse_layout) OVERRIDE;
  virtual void SetExtraOptions(const Config& config) OVERRIDE;
  virtual int set_sample_rate_hz(int rate) OVERRIDE;
  virtual int input_sample_rate_hz() const OVERRIDE;
  virtual int sample_rate_hz() const OVERRIDE;
  virtual int proc_sample_rate_hz() const OVERRIDE;
  virtual int proc_split_sample_rate_hz() const OVERRIDE;
  virtual int num_input_channels() const OVERRIDE;
  virtual int num_output_channels() const OVERRIDE;
  virtual int num_reverse_channels() const OVERRIDE;
  virtual void set_output_will_be_muted(bool muted) OVERRIDE;
  virtual bool output_will_be_muted() const OVERRIDE;
  virtual int ProcessStream(AudioFrame* frame) OVERRIDE;
  virtual int ProcessStream(const float* const* src,
                            int samples_per_channel,
                            int input_sample_rate_hz,
                            ChannelLayout input_layout,
                            int output_sample_rate_hz,
                            ChannelLayout output_layout,
                            float* const* dest) OVERRIDE;
  virtual int AnalyzeReverseStream(AudioFrame* frame) OVERRIDE;
  virtual int AnalyzeReverseStream(const float* const* data,
                                   int samples_per_channel,
                                   int sample_rate_hz,
                                   ChannelLayout layout) OVERRIDE;
  virtual int set_stream_delay_ms(int delay) OVERRIDE;
  virtual int stream_delay_ms() const OVERRIDE;
  virtual bool was_stream_delay_set() const OVERRIDE;
  virtual void set_delay_offset_ms(int offset) OVERRIDE;
  virtual int delay_offset_ms() const OVERRIDE;
  virtual void set_stream_key_pressed(bool key_pressed) OVERRIDE;
  virtual bool stream_key_pressed() const OVERRIDE;
  virtual int StartDebugRecording(
      const char filename[kMaxFilenameSize]) OVERRIDE;
  virtual int StartDebugRecording(FILE* handle) OVERRIDE;
  virtual int StartDebugRecordingForPlatformFile(
      rtc::PlatformFile handle) OVERRIDE;
  virtual int StopDebugRecording() OVERRIDE;
  virtual EchoCancellation* echo_cancellation() const OVERRIDE;
  virtual EchoControlMobile* echo_control_mobile() const OVERRIDE;
  virtual GainControl* gain_control() const OVERRIDE;
  virtual HighPassFilter* high_pass_filter() const OVERRIDE;
  virtual LevelEstimator* level_estimator() const OVERRIDE;
  virtual NoiseSuppression* noise_suppression() const OVERRIDE;
  virtual VoiceDetection* voice_detection() const OVERRIDE;

 protected:
  // Overridden in a mock.
  virtual int InitializeLocked();

 private:
  int InitializeLocked(int input_sample_rate_hz,
                       int output_sample_rate_hz,
                       int reverse_sample_rate_hz,
                       int num_input_channels,
                       int num_output_channels,
                       int num_reverse_channels);
  int MaybeInitializeLocked(int input_sample_rate_hz,
                            int output_sample_rate_hz,
                            int reverse_sample_rate_hz,
                            int num_input_channels,
                            int num_output_channels,
                            int num_reverse_channels);
  int ProcessStreamLocked();
  int AnalyzeReverseStreamLocked();

  bool is_data_processed() const;
  bool output_copy_needed(bool is_data_processed) const;
  bool synthesis_needed(bool is_data_processed) const;
  bool analysis_needed(bool is_data_processed) const;

  EchoCancellationImpl* echo_cancellation_;
  EchoControlMobileImpl* echo_control_mobile_;
  GainControlImpl* gain_control_;
  HighPassFilterImpl* high_pass_filter_;
  LevelEstimatorImpl* level_estimator_;
  NoiseSuppressionImpl* noise_suppression_;
  VoiceDetectionImpl* voice_detection_;

  std::list<ProcessingComponent*> component_list_;
  CriticalSectionWrapper* crit_;
  scoped_ptr<AudioBuffer> render_audio_;
  scoped_ptr<AudioBuffer> capture_audio_;
#ifdef WEBRTC_AUDIOPROC_DEBUG_DUMP
  // TODO(andrew): make this more graceful. Ideally we would split this stuff
  // out into a separate class with an "enabled" and "disabled" implementation.
  int WriteMessageToDebugFile();
  int WriteInitMessage();
  scoped_ptr<FileWrapper> debug_file_;
  scoped_ptr<audioproc::Event> event_msg_;  // Protobuf message.
  std::string event_str_;  // Memory for protobuf serialization.
#endif

  AudioFormat fwd_in_format_;
  AudioFormat fwd_proc_format_;
  AudioRate fwd_out_format_;
  AudioFormat rev_in_format_;
  AudioFormat rev_proc_format_;
  int split_rate_;

  int stream_delay_ms_;
  int delay_offset_ms_;
  bool was_stream_delay_set_;

  bool output_will_be_muted_;

  bool key_pressed_;
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_PROCESSING_MAIN_SOURCE_AUDIO_PROCESSING_IMPL_H_
