/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef WEBRTC_TEST_COMMON_CALL_TEST_H_
#define WEBRTC_TEST_COMMON_CALL_TEST_H_

#include <vector>

#include "webrtc/call.h"
#include "webrtc/system_wrappers/interface/scoped_vector.h"
#include "webrtc/test/fake_decoder.h"
#include "webrtc/test/fake_encoder.h"
#include "webrtc/test/frame_generator_capturer.h"
#include "webrtc/test/rtp_rtcp_observer.h"

namespace webrtc {
namespace test {

class BaseTest;

class CallTest : public ::testing::Test {
 public:
  CallTest();
  ~CallTest();

  static const size_t kNumSsrcs = 3;

  static const unsigned int kDefaultTimeoutMs;
  static const unsigned int kLongTimeoutMs;
  static const uint8_t kSendPayloadType;
  static const uint8_t kSendRtxPayloadType;
  static const uint8_t kFakeSendPayloadType;
  static const uint8_t kRedPayloadType;
  static const uint8_t kUlpfecPayloadType;
  static const uint32_t kSendRtxSsrcs[kNumSsrcs];
  static const uint32_t kSendSsrcs[kNumSsrcs];
  static const uint32_t kReceiverLocalSsrc;
  static const int kNackRtpHistoryMs;

 protected:
  void RunBaseTest(BaseTest* test);

  void CreateCalls(const Call::Config& sender_config,
                   const Call::Config& receiver_config);
  void CreateSenderCall(const Call::Config& config);
  void CreateReceiverCall(const Call::Config& config);

  void CreateSendConfig(size_t num_streams);
  void CreateMatchingReceiveConfigs();

  void CreateFrameGeneratorCapturer();

  void CreateStreams();
  void Start();
  void Stop();
  void DestroyStreams();

  Clock* const clock_;

  rtc::scoped_ptr<Call> sender_call_;
  VideoSendStream::Config send_config_;
  VideoEncoderConfig encoder_config_;
  VideoSendStream* send_stream_;

  rtc::scoped_ptr<Call> receiver_call_;
  std::vector<VideoReceiveStream::Config> receive_configs_;
  std::vector<VideoReceiveStream*> receive_streams_;

  rtc::scoped_ptr<test::FrameGeneratorCapturer> frame_generator_capturer_;
  test::FakeEncoder fake_encoder_;
  ScopedVector<VideoDecoder> allocated_decoders_;
};

class BaseTest : public RtpRtcpObserver {
 public:
  explicit BaseTest(unsigned int timeout_ms);
  BaseTest(unsigned int timeout_ms, const FakeNetworkPipe::Config& config);
  virtual ~BaseTest();

  virtual void PerformTest() = 0;
  virtual bool ShouldCreateReceivers() const = 0;

  virtual size_t GetNumStreams() const;

  virtual Call::Config GetSenderCallConfig();
  virtual Call::Config GetReceiverCallConfig();
  virtual void OnCallsCreated(Call* sender_call, Call* receiver_call);

  virtual void ModifyConfigs(
      VideoSendStream::Config* send_config,
      std::vector<VideoReceiveStream::Config>* receive_configs,
      VideoEncoderConfig* encoder_config);
  virtual void OnStreamsCreated(
      VideoSendStream* send_stream,
      const std::vector<VideoReceiveStream*>& receive_streams);

  virtual void OnFrameGeneratorCapturerCreated(
      FrameGeneratorCapturer* frame_generator_capturer);
};

class SendTest : public BaseTest {
 public:
  explicit SendTest(unsigned int timeout_ms);
  SendTest(unsigned int timeout_ms, const FakeNetworkPipe::Config& config);

  bool ShouldCreateReceivers() const override;
};

class EndToEndTest : public BaseTest {
 public:
  explicit EndToEndTest(unsigned int timeout_ms);
  EndToEndTest(unsigned int timeout_ms, const FakeNetworkPipe::Config& config);

  bool ShouldCreateReceivers() const override;
};

}  // namespace test
}  // namespace webrtc

#endif  // WEBRTC_TEST_COMMON_CALL_TEST_H_
