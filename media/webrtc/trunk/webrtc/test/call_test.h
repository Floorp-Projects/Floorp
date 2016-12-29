/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef WEBRTC_TEST_CALL_TEST_H_
#define WEBRTC_TEST_CALL_TEST_H_

#include <vector>

#include "webrtc/call.h"
#include "webrtc/call/transport_adapter.h"
#include "webrtc/system_wrappers/include/scoped_vector.h"
#include "webrtc/test/fake_audio_device.h"
#include "webrtc/test/fake_decoder.h"
#include "webrtc/test/fake_encoder.h"
#include "webrtc/test/frame_generator_capturer.h"
#include "webrtc/test/rtp_rtcp_observer.h"

namespace webrtc {

class VoEBase;
class VoECodec;
class VoENetwork;

namespace test {

class BaseTest;

class CallTest : public ::testing::Test {
 public:
  CallTest();
  virtual ~CallTest();

  static const size_t kNumSsrcs = 3;

  static const int kDefaultTimeoutMs;
  static const int kLongTimeoutMs;
  static const uint8_t kVideoSendPayloadType;
  static const uint8_t kSendRtxPayloadType;
  static const uint8_t kFakeVideoSendPayloadType;
  static const uint8_t kRedPayloadType;
  static const uint8_t kRtxRedPayloadType;
  static const uint8_t kUlpfecPayloadType;
  static const uint8_t kAudioSendPayloadType;
  static const uint32_t kSendRtxSsrcs[kNumSsrcs];
  static const uint32_t kVideoSendSsrcs[kNumSsrcs];
  static const uint32_t kAudioSendSsrc;
  static const uint32_t kReceiverLocalVideoSsrc;
  static const uint32_t kReceiverLocalAudioSsrc;
  static const int kNackRtpHistoryMs;

 protected:
  // RunBaseTest overwrites the audio_state and the voice_engine of the send and
  // receive Call configs to simplify test code and avoid having old VoiceEngine
  // APIs in the tests.
  void RunBaseTest(BaseTest* test);

  void CreateCalls(const Call::Config& sender_config,
                   const Call::Config& receiver_config);
  void CreateSenderCall(const Call::Config& config);
  void CreateReceiverCall(const Call::Config& config);
  void DestroyCalls();

  void CreateSendConfig(size_t num_video_streams,
                        size_t num_audio_streams,
                        Transport* send_transport);
  void CreateMatchingReceiveConfigs(Transport* rtcp_send_transport);

  void CreateFrameGeneratorCapturer();
  void CreateFakeAudioDevices();

  void CreateVideoStreams();
  void CreateAudioStreams();
  void Start();
  void Stop();
  void DestroyStreams();

  Clock* const clock_;

  rtc::scoped_ptr<Call> sender_call_;
  rtc::scoped_ptr<PacketTransport> send_transport_;
  VideoSendStream::Config video_send_config_;
  VideoEncoderConfig video_encoder_config_;
  VideoSendStream* video_send_stream_;
  AudioSendStream::Config audio_send_config_;
  AudioSendStream* audio_send_stream_;

  rtc::scoped_ptr<Call> receiver_call_;
  rtc::scoped_ptr<PacketTransport> receive_transport_;
  std::vector<VideoReceiveStream::Config> video_receive_configs_;
  std::vector<VideoReceiveStream*> video_receive_streams_;
  std::vector<AudioReceiveStream::Config> audio_receive_configs_;
  std::vector<AudioReceiveStream*> audio_receive_streams_;

  rtc::scoped_ptr<test::FrameGeneratorCapturer> frame_generator_capturer_;
  test::FakeEncoder fake_encoder_;
  ScopedVector<VideoDecoder> allocated_decoders_;
  size_t num_video_streams_;
  size_t num_audio_streams_;

 private:
  // TODO(holmer): Remove once VoiceEngine is fully refactored to the new API.
  // These methods are used to set up legacy voice engines and channels which is
  // necessary while voice engine is being refactored to the new stream API.
  struct VoiceEngineState {
    VoiceEngineState()
        : voice_engine(nullptr),
          base(nullptr),
          network(nullptr),
          codec(nullptr),
          channel_id(-1),
          transport_adapter(nullptr) {}

    VoiceEngine* voice_engine;
    VoEBase* base;
    VoENetwork* network;
    VoECodec* codec;
    int channel_id;
    rtc::scoped_ptr<internal::TransportAdapter> transport_adapter;
  };

  void CreateVoiceEngines();
  void SetupVoiceEngineTransports(PacketTransport* send_transport,
                                  PacketTransport* recv_transport);
  void DestroyVoiceEngines();

  VoiceEngineState voe_send_;
  VoiceEngineState voe_recv_;

  // The audio devices must outlive the voice engines.
  rtc::scoped_ptr<test::FakeAudioDevice> fake_send_audio_device_;
  rtc::scoped_ptr<test::FakeAudioDevice> fake_recv_audio_device_;
};

class BaseTest : public RtpRtcpObserver {
 public:
  explicit BaseTest(unsigned int timeout_ms);
  virtual ~BaseTest();

  virtual void PerformTest() = 0;
  virtual bool ShouldCreateReceivers() const = 0;

  virtual size_t GetNumVideoStreams() const;
  virtual size_t GetNumAudioStreams() const;

  virtual Call::Config GetSenderCallConfig();
  virtual Call::Config GetReceiverCallConfig();
  virtual void OnCallsCreated(Call* sender_call, Call* receiver_call);

  virtual test::PacketTransport* CreateSendTransport(Call* sender_call);
  virtual test::PacketTransport* CreateReceiveTransport();

  virtual void ModifyVideoConfigs(
      VideoSendStream::Config* send_config,
      std::vector<VideoReceiveStream::Config>* receive_configs,
      VideoEncoderConfig* encoder_config);
  virtual void OnVideoStreamsCreated(
      VideoSendStream* send_stream,
      const std::vector<VideoReceiveStream*>& receive_streams);

  virtual void ModifyAudioConfigs(
      AudioSendStream::Config* send_config,
      std::vector<AudioReceiveStream::Config>* receive_configs);
  virtual void OnAudioStreamsCreated(
      AudioSendStream* send_stream,
      const std::vector<AudioReceiveStream*>& receive_streams);

  virtual void OnFrameGeneratorCapturerCreated(
      FrameGeneratorCapturer* frame_generator_capturer);
};

class SendTest : public BaseTest {
 public:
  explicit SendTest(unsigned int timeout_ms);

  bool ShouldCreateReceivers() const override;
};

class EndToEndTest : public BaseTest {
 public:
  explicit EndToEndTest(unsigned int timeout_ms);

  bool ShouldCreateReceivers() const override;
};

}  // namespace test
}  // namespace webrtc

#endif  // WEBRTC_TEST_CALL_TEST_H_
