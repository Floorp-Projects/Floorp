/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOCK_CALL_H_
#define MOCK_CALL_H_

#include <webrtc/api/call/audio_sink.h>
#include <webrtc/call/call.h>

using namespace webrtc;

namespace test {

class MockAudioSendStream : public webrtc::AudioSendStream {
public:
  void Start() override {}

  void Stop() override {}

  bool SendTelephoneEvent(int payload_type, int payload_frequency,
                          int event, int duration_ms) override
  {
    return true;
  }

  void SetMuted(bool muted) override {}

  Stats GetStats() const override
  {
    return mStats;
  }

  virtual ~MockAudioSendStream() {}

  AudioSendStream::Stats mStats;
};

class MockAudioReceiveStream : public webrtc::AudioReceiveStream {
public:
  void Start() override {}

  void Stop() override {}

  Stats GetStats() const override
  {
    return mStats;
  }

  void SetSink(std::unique_ptr<AudioSinkInterface> sink) override {}

  void SetGain(float gain) override {}

  virtual ~MockAudioReceiveStream() {}

  AudioReceiveStream::Stats mStats;
};

class MockVideoSendStream : public webrtc::VideoSendStream {
public:
  void Start() override {}

  void Stop() override {}

  void SetSource(rtc::VideoSourceInterface<webrtc::VideoFrame>* source,
                 const DegradationPreference& degradation_preference) override {}

  CPULoadStateObserver* LoadStateObserver() override
  {
    return nullptr;
  }

  void ReconfigureVideoEncoder(VideoEncoderConfig config) override
  {
    mEncoderConfig = config.Copy();
  }

  Stats GetStats() override
  {
    return mStats;
  }

  void EnableEncodedFrameRecording(const std::vector<rtc::PlatformFile>& files,
                                   size_t byte_limit) override {}

  virtual ~MockVideoSendStream() {}

  VideoEncoderConfig mEncoderConfig;
  VideoSendStream::Stats mStats;
};

class MockVideoReceiveStream : public webrtc::VideoReceiveStream {
public:
  void Start() override {}

  void Stop() override {}

  Stats GetStats() const override
  {
    return mStats;
  }

  bool
  GetRemoteRTCPSenderInfo(RTCPSenderInfo* sender_info) const override
  {
    return false;
  }

  void SetSyncChannel(VoiceEngine* voice_engine, int audio_channel_id) override {}

  void EnableEncodedFrameRecording(rtc::PlatformFile file,
                                   size_t byte_limit) override {}

  virtual ~MockVideoReceiveStream() {}

  VideoReceiveStream::Stats mStats;
};

class MockCall : public webrtc::Call {
public:
  MockCall()
    : mAudioSendConfig(nullptr)
    , mVideoReceiveConfig(nullptr)
    , mVideoSendConfig(nullptr)
    , mCurrentVideoSendStream(nullptr) {}

  AudioSendStream* CreateAudioSendStream(const AudioSendStream::Config& config) override
  {
    mAudioSendConfig = config;
    return new MockAudioSendStream;
  }

  void DestroyAudioSendStream(AudioSendStream* send_stream) override
  {
    delete static_cast<MockAudioSendStream*>(send_stream);
  }

  AudioReceiveStream* CreateAudioReceiveStream(const AudioReceiveStream::Config& config) override
  {
    mAudioReceiveConfig = config;
    return new MockAudioReceiveStream;
  }
  void DestroyAudioReceiveStream(AudioReceiveStream* receive_stream) override
  {
    delete static_cast<MockAudioReceiveStream*>(receive_stream);
  }

  VideoSendStream* CreateVideoSendStream(VideoSendStream::Config config,
                                         VideoEncoderConfig encoder_config) override {
    MOZ_RELEASE_ASSERT(!mCurrentVideoSendStream);
    mVideoSendConfig = config.Copy();
    mEncoderConfig = encoder_config.Copy();
    mCurrentVideoSendStream = new MockVideoSendStream;
    return mCurrentVideoSendStream;
  }

  void DestroyVideoSendStream(VideoSendStream* send_stream) override
  {
    MOZ_RELEASE_ASSERT(mCurrentVideoSendStream == send_stream);
    mCurrentVideoSendStream = nullptr;
    delete static_cast<MockVideoSendStream*>(send_stream);
  }

  VideoReceiveStream* CreateVideoReceiveStream(VideoReceiveStream::Config configuration) override
  {
    mVideoReceiveConfig = configuration.Copy();
    return new MockVideoReceiveStream;
  }

  void DestroyVideoReceiveStream(VideoReceiveStream* receive_stream) override
  {
    delete static_cast<MockVideoReceiveStream*>(receive_stream);
  }

  FlexfecReceiveStream* CreateFlexfecReceiveStream(const FlexfecReceiveStream::Config& config) override
  {
    return nullptr;
  }

  void DestroyFlexfecReceiveStream(FlexfecReceiveStream* receive_stream) override
  {
  }

  PacketReceiver* Receiver() override
  {
    return nullptr;
  }

  Stats GetStats() const override
  {
    return mStats;
  }

  void SetBitrateConfig(const Config::BitrateConfig& bitrate_config) override {}

  void SignalChannelNetworkState(MediaType media,
                                 NetworkState state) override {}

  void OnTransportOverheadChanged(MediaType media,
                                  int transport_overhead_per_packet) override {}

  void OnNetworkRouteChanged(const std::string& transport_name,
                             const rtc::NetworkRoute& network_route) override {}

  void OnSentPacket(const rtc::SentPacket& sent_packet) override {}

  VoiceEngine* voice_engine() override
  {
    return nullptr;
  }

  virtual ~MockCall() {};

  AudioReceiveStream::Config mAudioReceiveConfig;
  AudioSendStream::Config mAudioSendConfig;
  VideoReceiveStream::Config mVideoReceiveConfig;
  VideoSendStream::Config mVideoSendConfig;
  VideoEncoderConfig mEncoderConfig;
  Call::Stats mStats;
  MockVideoSendStream* mCurrentVideoSendStream;
};

}
#endif
