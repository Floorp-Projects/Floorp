/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VOICE_ENGINE_VOE_BASE_IMPL_H_
#define VOICE_ENGINE_VOE_BASE_IMPL_H_

#include "voice_engine/include/voe_base.h"

#include "modules/include/module_common_types.h"
#include "rtc_base/criticalsection.h"
#include "voice_engine/shared_data.h"

namespace webrtc {

class ProcessThread;

class VoEBaseImpl : public VoEBase,
                    public AudioTransport {
 public:
  int Init(
      AudioDeviceModule* audio_device,
      AudioProcessing* audio_processing,
      const rtc::scoped_refptr<AudioDecoderFactory>& decoder_factory) override;
  voe::TransmitMixer* transmit_mixer() override {
    return shared_->transmit_mixer();
  }
  void Terminate() override;

  int CreateChannel() override;
  int CreateChannel(const ChannelConfig& config) override;
  int DeleteChannel(int channel) override;

  int StartPlayout(int channel) override;
  int StartSend(int channel) override;
  int StopPlayout(int channel) override;
  int StopSend(int channel) override;

  int SetPlayout(bool enabled) override;
  int SetRecording(bool enabled) override;

  AudioTransport* audio_transport() override { return this; }

  // AudioTransport
  int32_t RecordedDataIsAvailable(const void* audio_data,
                                  const size_t number_of_frames,
                                  const size_t bytes_per_sample,
                                  const size_t number_of_channels,
                                  const uint32_t sample_rate,
                                  const uint32_t audio_delay_milliseconds,
                                  const int32_t clock_drift,
                                  const uint32_t volume,
                                  const bool key_pressed,
                                  uint32_t& new_mic_volume) override;
  RTC_DEPRECATED int32_t NeedMorePlayData(const size_t nSamples,
                                          const size_t nBytesPerSample,
                                          const size_t nChannels,
                                          const uint32_t samplesPerSec,
                                          void* audioSamples,
                                          size_t& nSamplesOut,
                                          int64_t* elapsed_time_ms,
                                          int64_t* ntp_time_ms) override;
  void PushCaptureData(int voe_channel,
                       const void* audio_data,
                       int bits_per_sample,
                       int sample_rate,
                       size_t number_of_channels,
                       size_t number_of_frames) override;
  RTC_DEPRECATED void PullRenderData(int bits_per_sample,
                                     int sample_rate,
                                     size_t number_of_channels,
                                     size_t number_of_frames,
                                     void* audio_data,
                                     int64_t* elapsed_time_ms,
                                     int64_t* ntp_time_ms) override;

 protected:
  VoEBaseImpl(voe::SharedData* shared);
  ~VoEBaseImpl() override;

 private:
  int32_t StartPlayout();
  int32_t StopPlayout();
  int32_t StartSend();
  int32_t StopSend();
  void TerminateInternal();

  void GetPlayoutData(int sample_rate, size_t number_of_channels,
                      size_t number_of_frames, bool feed_data_to_apm,
                      void* audio_data, int64_t* elapsed_time_ms,
                      int64_t* ntp_time_ms);

  // Initialize channel by setting Engine Information then initializing
  // channel.
  int InitializeChannel(voe::ChannelOwner* channel_owner);
  rtc::scoped_refptr<AudioDecoderFactory> decoder_factory_;

  AudioFrame audioFrame_;
  voe::SharedData* shared_;
  bool playout_enabled_ = true;
  bool recording_enabled_ = true;
};

}  // namespace webrtc

#endif  // VOICE_ENGINE_VOE_BASE_IMPL_H_
