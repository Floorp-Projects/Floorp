/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOCK_CALL_H_
#define MOCK_CALL_H_

#include "gmock/gmock.h"
#include "mozilla/Assertions.h"
#include "WebrtcCallWrapper.h"

// libwebrtc
#include "api/call/audio_sink.h"
#include "call/call.h"

using namespace webrtc;

namespace test {

class MockAudioSendStream : public webrtc::AudioSendStream {
 public:
  MockAudioSendStream() : mConfig(nullptr) {}

  const webrtc::AudioSendStream::Config& GetConfig() const override {
    return mConfig;
  }

  void Reconfigure(const Config& config) override { mConfig = config; }

  void Start() override {}

  void Stop() override {}

  void SendAudioData(std::unique_ptr<AudioFrame> audio_frame) override {}

  bool SendTelephoneEvent(int payload_type, int payload_frequency, int event,
                          int duration_ms) override {
    return true;
  }

  void SetMuted(bool muted) override {}

  Stats GetStats() const override { return mStats; }

  Stats GetStats(bool has_remote_tracks) const override { return mStats; }

  virtual ~MockAudioSendStream() {}

  AudioSendStream::Config mConfig;
  AudioSendStream::Stats mStats;
};

class MockAudioReceiveStream : public webrtc::AudioReceiveStream {
 public:
  void Start() override {}

  void Stop() override {}

  Stats GetStats(bool get_and_clear_legacy_stats) const override {
    return mStats;
  }

  void SetSink(AudioSinkInterface* sink) override {}

  void SetGain(float gain) override {}

  std::vector<RtpSource> GetSources() const override { return mRtpSources; }

  void Reconfigure(const Config& config) override {}
  bool SetBaseMinimumPlayoutDelayMs(int delay_ms) override { return false; }
  int GetBaseMinimumPlayoutDelayMs() const override { return 0; }

  virtual ~MockAudioReceiveStream() {}

  AudioReceiveStream::Stats mStats;
  std::vector<RtpSource> mRtpSources;
};

class MockVideoSendStream : public webrtc::VideoSendStream {
 public:
  explicit MockVideoSendStream(VideoEncoderConfig&& config)
      : mEncoderConfig(std::move(config)) {}

  void Start() override {}

  void Stop() override {}

  void SetSource(rtc::VideoSourceInterface<webrtc::VideoFrame>* source,
                 const DegradationPreference& degradation_preference) override {
  }

  void ReconfigureVideoEncoder(VideoEncoderConfig config) override {
    mEncoderConfig = config.Copy();
  }

  Stats GetStats() override { return mStats; }

  void UpdateActiveSimulcastLayers(
      const std::vector<bool> active_layers) override {}

  void AddAdaptationResource(rtc::scoped_refptr<Resource> resource) override {}

  std::vector<rtc::scoped_refptr<Resource>> GetAdaptationResources() override {
    return std::vector<rtc::scoped_refptr<Resource>>();
  }

  virtual ~MockVideoSendStream() {}

  VideoEncoderConfig mEncoderConfig;
  VideoSendStream::Stats mStats;
};

class MockVideoReceiveStream : public webrtc::VideoReceiveStream {
 public:
  void Start() override {}

  void Stop() override {}

  Stats GetStats() const override { return mStats; }

  void AddSecondarySink(RtpPacketSinkInterface* sink) override {}
  void RemoveSecondarySink(const RtpPacketSinkInterface* sink) override {}

  std::vector<RtpSource> GetSources() const override {
    return std::vector<RtpSource>();
  }

  bool SetBaseMinimumPlayoutDelayMs(int delay_ms) override { return false; }

  int GetBaseMinimumPlayoutDelayMs() const override { return 0; }

  void SetFrameDecryptor(
      rtc::scoped_refptr<FrameDecryptorInterface> frame_decryptor) override {}

  void SetDepacketizerToDecoderFrameTransformer(
      rtc::scoped_refptr<FrameTransformerInterface> frame_transformer)
      override {}

  RecordingState SetAndGetRecordingState(RecordingState state,
                                         bool generate_key_frame) override {
    return {};
  }

  void GenerateKeyFrame() override {}

  virtual ~MockVideoReceiveStream() {}

  VideoReceiveStream::Stats mStats;
};

class MockCall : public webrtc::Call {
 public:
  MockCall()
      : mAudioSendConfig(nullptr),
        mVideoReceiveConfig(nullptr),
        mVideoSendConfig(nullptr),
        mCurrentVideoSendStream(nullptr) {}

  AudioSendStream* CreateAudioSendStream(
      const AudioSendStream::Config& config) override {
    mAudioSendConfig = config;
    return new MockAudioSendStream;
  }

  void DestroyAudioSendStream(AudioSendStream* send_stream) override {
    delete static_cast<MockAudioSendStream*>(send_stream);
  }

  AudioReceiveStream* CreateAudioReceiveStream(
      const AudioReceiveStream::Config& config) override {
    mAudioReceiveConfig = config;
    return new MockAudioReceiveStream;
  }
  void DestroyAudioReceiveStream(AudioReceiveStream* receive_stream) override {
    delete static_cast<MockAudioReceiveStream*>(receive_stream);
  }

  VideoSendStream* CreateVideoSendStream(
      VideoSendStream::Config config,
      VideoEncoderConfig encoder_config) override {
    MOZ_RELEASE_ASSERT(!mCurrentVideoSendStream);
    mVideoSendConfig = config.Copy();
    mCurrentVideoSendStream = new MockVideoSendStream(encoder_config.Copy());
    return mCurrentVideoSendStream;
  }

  void DestroyVideoSendStream(VideoSendStream* send_stream) override {
    MOZ_RELEASE_ASSERT(mCurrentVideoSendStream == send_stream);
    mCurrentVideoSendStream = nullptr;
    delete static_cast<MockVideoSendStream*>(send_stream);
  }

  VideoReceiveStream* CreateVideoReceiveStream(
      VideoReceiveStream::Config configuration) override {
    mVideoReceiveConfig = configuration.Copy();
    return new MockVideoReceiveStream;
  }

  void DestroyVideoReceiveStream(VideoReceiveStream* receive_stream) override {
    delete static_cast<MockVideoReceiveStream*>(receive_stream);
  }

  FlexfecReceiveStream* CreateFlexfecReceiveStream(
      const FlexfecReceiveStream::Config& config) override {
    return nullptr;
  }

  void DestroyFlexfecReceiveStream(
      FlexfecReceiveStream* receive_stream) override {}

  void AddAdaptationResource(rtc::scoped_refptr<Resource> resource) override {}

  PacketReceiver* Receiver() override { return nullptr; }

  RtpTransportControllerSendInterface* GetTransportControllerSend() override {
    return nullptr;
  }

  Stats GetStats() const override { return mStats; }

  void SignalChannelNetworkState(MediaType media, NetworkState state) override {
  }

  void OnAudioTransportOverheadChanged(
      int transport_overhead_per_packet) override {}

  void OnSentPacket(const rtc::SentPacket& sent_packet) override {}

  void SetClientBitratePreferences(
      const BitrateSettings& preferences) override {}

  std::vector<webrtc::VideoStream> CreateEncoderStreams(int width, int height) {
    const VideoEncoderConfig& config = mCurrentVideoSendStream->mEncoderConfig;
    return config.video_stream_factory->CreateEncoderStreams(width, height,
                                                             config);
  }

  virtual ~MockCall(){};

  AudioReceiveStream::Config mAudioReceiveConfig;
  AudioSendStream::Config mAudioSendConfig;
  VideoReceiveStream::Config mVideoReceiveConfig;
  VideoSendStream::Config mVideoSendConfig;
  Call::Stats mStats;
  MockVideoSendStream* mCurrentVideoSendStream;
};

class MockCallWrapper : public mozilla::WebrtcCallWrapper {
 public:
  MockCallWrapper()
      : mozilla::WebrtcCallWrapper(
            mozilla::AbstractThread::MainThread(), nullptr, nullptr, nullptr,
            nullptr, mozilla::dom::RTCStatsTimestampMaker(), nullptr) {}

  static RefPtr<testing::NiceMock<MockCallWrapper>> Create() {
    auto wrapper = mozilla::MakeRefPtr<testing::NiceMock<MockCallWrapper>>();
    wrapper->SetCall(mozilla::WrapUnique(new MockCall));
    return wrapper;
  }

  MockCall* GetMockCall() { return static_cast<MockCall*>(Call()); }
};

}  // namespace test
#endif
