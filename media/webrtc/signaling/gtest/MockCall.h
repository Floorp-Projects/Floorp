/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOCK_CALL_H_
#define MOCK_CALL_H_

#include "gmock/gmock.h"
#include "mozilla/Assertions.h"
#include "WebrtcCallWrapper.h"
#include "PeerConnectionCtx.h"

// libwebrtc
#include "api/call/audio_sink.h"
#include "call/call.h"

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

  void SendAudioData(std::unique_ptr<webrtc::AudioFrame> audio_frame) override {
  }

  bool SendTelephoneEvent(int payload_type, int payload_frequency, int event,
                          int duration_ms) override {
    return true;
  }

  void SetMuted(bool muted) override {}

  Stats GetStats() const override { return mStats; }

  Stats GetStats(bool has_remote_tracks) const override { return mStats; }

  virtual ~MockAudioSendStream() {}

  webrtc::AudioSendStream::Config mConfig;
  webrtc::AudioSendStream::Stats mStats;
};

class MockAudioReceiveStream : public webrtc::AudioReceiveStream {
 public:
  void Start() override {}

  void Stop() override {}

  Stats GetStats(bool get_and_clear_legacy_stats) const override {
    return mStats;
  }

  void SetSink(webrtc::AudioSinkInterface* sink) override {}

  void SetGain(float gain) override {}

  std::vector<webrtc::RtpSource> GetSources() const override {
    return mRtpSources;
  }

  void Reconfigure(const Config& config) override {}
  bool SetBaseMinimumPlayoutDelayMs(int delay_ms) override { return false; }
  int GetBaseMinimumPlayoutDelayMs() const override { return 0; }

  virtual ~MockAudioReceiveStream() {}

  webrtc::AudioReceiveStream::Stats mStats;
  std::vector<webrtc::RtpSource> mRtpSources;
};

class MockVideoSendStream : public webrtc::VideoSendStream {
 public:
  explicit MockVideoSendStream(webrtc::VideoEncoderConfig&& config)
      : mEncoderConfig(std::move(config)) {}

  void Start() override {}

  void Stop() override {}

  void SetSource(
      rtc::VideoSourceInterface<webrtc::VideoFrame>* source,
      const webrtc::DegradationPreference& degradation_preference) override {}

  void ReconfigureVideoEncoder(webrtc::VideoEncoderConfig config) override {
    mEncoderConfig = config.Copy();
  }

  Stats GetStats() override { return mStats; }

  void UpdateActiveSimulcastLayers(
      const std::vector<bool> active_layers) override {}

  void AddAdaptationResource(
      rtc::scoped_refptr<webrtc::Resource> resource) override {}

  std::vector<rtc::scoped_refptr<webrtc::Resource>> GetAdaptationResources()
      override {
    return std::vector<rtc::scoped_refptr<webrtc::Resource>>();
  }

  virtual ~MockVideoSendStream() {}

  webrtc::VideoEncoderConfig mEncoderConfig;
  webrtc::VideoSendStream::Stats mStats;
};

class MockVideoReceiveStream : public webrtc::VideoReceiveStream {
 public:
  void Start() override {}

  void Stop() override {}

  Stats GetStats() const override { return mStats; }

  void AddSecondarySink(webrtc::RtpPacketSinkInterface* sink) override {}
  void RemoveSecondarySink(
      const webrtc::RtpPacketSinkInterface* sink) override {}

  std::vector<webrtc::RtpSource> GetSources() const override {
    return std::vector<webrtc::RtpSource>();
  }

  bool SetBaseMinimumPlayoutDelayMs(int delay_ms) override { return false; }

  int GetBaseMinimumPlayoutDelayMs() const override { return 0; }

  void SetFrameDecryptor(rtc::scoped_refptr<webrtc::FrameDecryptorInterface>
                             frame_decryptor) override {}

  void SetDepacketizerToDecoderFrameTransformer(
      rtc::scoped_refptr<webrtc::FrameTransformerInterface> frame_transformer)
      override {}

  RecordingState SetAndGetRecordingState(RecordingState state,
                                         bool generate_key_frame) override {
    return {};
  }

  void GenerateKeyFrame() override {}

  virtual ~MockVideoReceiveStream() {}

  webrtc::VideoReceiveStream::Stats mStats;
};

class MockCall : public webrtc::Call {
 public:
  MockCall()
      : mAudioSendConfig(nullptr),
        mVideoReceiveConfig(nullptr),
        mVideoSendConfig(nullptr),
        mCurrentVideoSendStream(nullptr) {}

  webrtc::AudioSendStream* CreateAudioSendStream(
      const webrtc::AudioSendStream::Config& config) override {
    mAudioSendConfig = config;
    return new MockAudioSendStream;
  }

  void DestroyAudioSendStream(webrtc::AudioSendStream* send_stream) override {
    delete static_cast<MockAudioSendStream*>(send_stream);
  }

  webrtc::AudioReceiveStream* CreateAudioReceiveStream(
      const webrtc::AudioReceiveStream::Config& config) override {
    mAudioReceiveConfig = config;
    return new MockAudioReceiveStream;
  }
  void DestroyAudioReceiveStream(
      webrtc::AudioReceiveStream* receive_stream) override {
    delete static_cast<MockAudioReceiveStream*>(receive_stream);
  }

  webrtc::VideoSendStream* CreateVideoSendStream(
      webrtc::VideoSendStream::Config config,
      webrtc::VideoEncoderConfig encoder_config) override {
    MOZ_RELEASE_ASSERT(!mCurrentVideoSendStream);
    mVideoSendConfig = config.Copy();
    mCurrentVideoSendStream = new MockVideoSendStream(encoder_config.Copy());
    return mCurrentVideoSendStream;
  }

  void DestroyVideoSendStream(webrtc::VideoSendStream* send_stream) override {
    MOZ_RELEASE_ASSERT(mCurrentVideoSendStream == send_stream);
    mCurrentVideoSendStream = nullptr;
    delete static_cast<MockVideoSendStream*>(send_stream);
  }

  webrtc::VideoReceiveStream* CreateVideoReceiveStream(
      webrtc::VideoReceiveStream::Config configuration) override {
    mVideoReceiveConfig = configuration.Copy();
    return new MockVideoReceiveStream;
  }

  void DestroyVideoReceiveStream(
      webrtc::VideoReceiveStream* receive_stream) override {
    delete static_cast<MockVideoReceiveStream*>(receive_stream);
  }

  webrtc::FlexfecReceiveStream* CreateFlexfecReceiveStream(
      const webrtc::FlexfecReceiveStream::Config& config) override {
    return nullptr;
  }

  void DestroyFlexfecReceiveStream(
      webrtc::FlexfecReceiveStream* receive_stream) override {}

  void AddAdaptationResource(
      rtc::scoped_refptr<webrtc::Resource> resource) override {}

  webrtc::PacketReceiver* Receiver() override { return nullptr; }

  webrtc::RtpTransportControllerSendInterface* GetTransportControllerSend()
      override {
    return nullptr;
  }

  Stats GetStats() const override { return mStats; }

  void SignalChannelNetworkState(webrtc::MediaType media,
                                 webrtc::NetworkState state) override {}

  void OnAudioTransportOverheadChanged(
      int transport_overhead_per_packet) override {}

  void OnSentPacket(const rtc::SentPacket& sent_packet) override {}

  void SetClientBitratePreferences(
      const webrtc::BitrateSettings& preferences) override {}

  std::vector<webrtc::VideoStream> CreateEncoderStreams(int width, int height) {
    const webrtc::VideoEncoderConfig& config =
        mCurrentVideoSendStream->mEncoderConfig;
    return config.video_stream_factory->CreateEncoderStreams(width, height,
                                                             config);
  }

  virtual ~MockCall(){};

  webrtc::AudioReceiveStream::Config mAudioReceiveConfig;
  webrtc::AudioSendStream::Config mAudioSendConfig;
  webrtc::VideoReceiveStream::Config mVideoReceiveConfig;
  webrtc::VideoSendStream::Config mVideoSendConfig;
  webrtc::Call::Stats mStats;
  MockVideoSendStream* mCurrentVideoSendStream;
};

class MockCallWrapper : public mozilla::WebrtcCallWrapper {
 public:
  MockCallWrapper(
      RefPtr<mozilla::SharedWebrtcState> aSharedState,
      mozilla::UniquePtr<webrtc::VideoBitrateAllocatorFactory>
          aVideoBitrateAllocatorFactory,
      mozilla::UniquePtr<webrtc::RtcEventLog> aEventLog,
      mozilla::UniquePtr<webrtc::TaskQueueFactory> aTaskQueueFactory,
      const mozilla::dom::RTCStatsTimestampMaker& aTimestampMaker,
      mozilla::UniquePtr<mozilla::media::ShutdownBlockingTicket>
          aShutdownTicket)
      : mozilla::WebrtcCallWrapper(
            std::move(aSharedState), std::move(aVideoBitrateAllocatorFactory),
            std::move(aEventLog), std::move(aTaskQueueFactory), aTimestampMaker,
            std::move(aShutdownTicket)) {}

  static RefPtr<MockCallWrapper> Create() {
    auto state = mozilla::MakeRefPtr<mozilla::SharedWebrtcState>(
        mozilla::AbstractThread::GetCurrent(), webrtc::AudioState::Config(),
        nullptr, nullptr);
    auto wrapper = mozilla::MakeRefPtr<MockCallWrapper>(
        state, nullptr, nullptr, nullptr,
        mozilla::dom::RTCStatsTimestampMaker(), nullptr);
    wrapper->SetCall(mozilla::WrapUnique(new MockCall));
    return wrapper;
  }

  MockCall* GetMockCall() { return static_cast<MockCall*>(Call()); }
};

}  // namespace test
#endif
