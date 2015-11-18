/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_PROCESSING_AUDIO_PROCESSING_IMPL_H_
#define WEBRTC_MODULES_AUDIO_PROCESSING_AUDIO_PROCESSING_IMPL_H_

#include <list>
#include <string>

#include "webrtc/base/scoped_ptr.h"
#include "webrtc/base/thread_annotations.h"
#include "webrtc/modules/audio_processing/include/audio_processing.h"

namespace webrtc {

class AgcManagerDirect;
class AudioBuffer;

template<typename T>
class Beamformer;

class CriticalSectionWrapper;
class EchoCancellationImpl;
class EchoControlMobileImpl;
class FileWrapper;
class GainControlImpl;
class GainControlForNewAgc;
class HighPassFilterImpl;
class LevelEstimatorImpl;
class NoiseSuppressionImpl;
class ProcessingComponent;
class TransientSuppressor;
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

  // AudioProcessingImpl takes ownership of beamformer.
  AudioProcessingImpl(const Config& config, Beamformer<float>* beamformer);
  virtual ~AudioProcessingImpl();

  // AudioProcessing methods.
  int Initialize() override;
  int Initialize(int input_sample_rate_hz,
                 int output_sample_rate_hz,
                 int reverse_sample_rate_hz,
                 ChannelLayout input_layout,
                 ChannelLayout output_layout,
                 ChannelLayout reverse_layout) override;
  void SetExtraOptions(const Config& config) override;
  int set_sample_rate_hz(int rate) override;
  int input_sample_rate_hz() const override;
  int sample_rate_hz() const override;
  int proc_sample_rate_hz() const override;
  int proc_split_sample_rate_hz() const override;
  int num_input_channels() const override;
  int num_output_channels() const override;
  int num_reverse_channels() const override;
  void set_output_will_be_muted(bool muted) override;
  bool output_will_be_muted() const override;
  int ProcessStream(AudioFrame* frame) override;
  int ProcessStream(const float* const* src,
                    int samples_per_channel,
                    int input_sample_rate_hz,
                    ChannelLayout input_layout,
                    int output_sample_rate_hz,
                    ChannelLayout output_layout,
                    float* const* dest) override;
  int AnalyzeReverseStream(AudioFrame* frame) override;
  int AnalyzeReverseStream(const float* const* data,
                           int samples_per_channel,
                           int sample_rate_hz,
                           ChannelLayout layout) override;
  int set_stream_delay_ms(int delay) override;
  int stream_delay_ms() const override;
  bool was_stream_delay_set() const override;
  void set_delay_offset_ms(int offset) override;
  int delay_offset_ms() const override;
  void set_stream_key_pressed(bool key_pressed) override;
  bool stream_key_pressed() const override;
  int StartDebugRecording(const char filename[kMaxFilenameSize]) override;
  int StartDebugRecording(FILE* handle) override;
  int StartDebugRecordingForPlatformFile(rtc::PlatformFile handle) override;
  int StopDebugRecording() override;
  EchoCancellation* echo_cancellation() const override;
  EchoControlMobile* echo_control_mobile() const override;
  GainControl* gain_control() const override;
  HighPassFilter* high_pass_filter() const override;
  LevelEstimator* level_estimator() const override;
  NoiseSuppression* noise_suppression() const override;
  VoiceDetection* voice_detection() const override;

 protected:
  // Overridden in a mock.
  virtual int InitializeLocked() EXCLUSIVE_LOCKS_REQUIRED(crit_);

 private:
  int InitializeLocked(int input_sample_rate_hz,
                       int output_sample_rate_hz,
                       int reverse_sample_rate_hz,
                       int num_input_channels,
                       int num_output_channels,
                       int num_reverse_channels)
      EXCLUSIVE_LOCKS_REQUIRED(crit_);
  int MaybeInitializeLocked(int input_sample_rate_hz,
                            int output_sample_rate_hz,
                            int reverse_sample_rate_hz,
                            int num_input_channels,
                            int num_output_channels,
                            int num_reverse_channels)
      EXCLUSIVE_LOCKS_REQUIRED(crit_);
  int ProcessStreamLocked() EXCLUSIVE_LOCKS_REQUIRED(crit_);
  int AnalyzeReverseStreamLocked() EXCLUSIVE_LOCKS_REQUIRED(crit_);

  bool is_data_processed() const;
  bool output_copy_needed(bool is_data_processed) const;
  bool synthesis_needed(bool is_data_processed) const;
  bool analysis_needed(bool is_data_processed) const;
  int InitializeExperimentalAgc() EXCLUSIVE_LOCKS_REQUIRED(crit_);
  int InitializeTransient() EXCLUSIVE_LOCKS_REQUIRED(crit_);
  void InitializeBeamformer() EXCLUSIVE_LOCKS_REQUIRED(crit_);

  EchoCancellationImpl* echo_cancellation_;
  EchoControlMobileImpl* echo_control_mobile_;
  GainControlImpl* gain_control_;
  HighPassFilterImpl* high_pass_filter_;
  LevelEstimatorImpl* level_estimator_;
  NoiseSuppressionImpl* noise_suppression_;
  VoiceDetectionImpl* voice_detection_;
  rtc::scoped_ptr<GainControlForNewAgc> gain_control_for_new_agc_;

  std::list<ProcessingComponent*> component_list_;
  CriticalSectionWrapper* crit_;
  rtc::scoped_ptr<AudioBuffer> render_audio_;
  rtc::scoped_ptr<AudioBuffer> capture_audio_;
#ifdef WEBRTC_AUDIOPROC_DEBUG_DUMP
  // TODO(andrew): make this more graceful. Ideally we would split this stuff
  // out into a separate class with an "enabled" and "disabled" implementation.
  int WriteMessageToDebugFile();
  int WriteInitMessage();
  rtc::scoped_ptr<FileWrapper> debug_file_;
  rtc::scoped_ptr<audioproc::Event> event_msg_;  // Protobuf message.
  std::string event_str_;  // Memory for protobuf serialization.
#endif

  AudioFormat fwd_in_format_;
  // This one is an AudioRate, because the forward processing number of channels
  // is mutable and is tracked by the capture_audio_.
  AudioRate fwd_proc_format_;
  AudioFormat fwd_out_format_;
  AudioFormat rev_in_format_;
  AudioFormat rev_proc_format_;
  int split_rate_;

  int stream_delay_ms_;
  int delay_offset_ms_;
  bool was_stream_delay_set_;

  bool output_will_be_muted_ GUARDED_BY(crit_);

  bool key_pressed_;

  // Only set through the constructor's Config parameter.
  const bool use_new_agc_;
  rtc::scoped_ptr<AgcManagerDirect> agc_manager_ GUARDED_BY(crit_);

  bool transient_suppressor_enabled_;
  rtc::scoped_ptr<TransientSuppressor> transient_suppressor_;
  const bool beamformer_enabled_;
  rtc::scoped_ptr<Beamformer<float>> beamformer_;
  const std::vector<Point> array_geometry_;

  const bool supports_48kHz_;
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_PROCESSING_AUDIO_PROCESSING_IMPL_H_
