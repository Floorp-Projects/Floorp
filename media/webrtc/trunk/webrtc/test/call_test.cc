/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "webrtc/test/call_test.h"
#include "webrtc/test/encoder_settings.h"

namespace webrtc {
namespace test {

namespace {
const int kVideoRotationRtpExtensionId = 4;
}

CallTest::CallTest()
    : clock_(Clock::GetRealTimeClock()),
      send_stream_(NULL),
      fake_encoder_(clock_) {
}

CallTest::~CallTest() {
}

void CallTest::RunBaseTest(BaseTest* test) {
  CreateSenderCall(test->GetSenderCallConfig());
  if (test->ShouldCreateReceivers())
    CreateReceiverCall(test->GetReceiverCallConfig());
  test->OnCallsCreated(sender_call_.get(), receiver_call_.get());

  if (test->ShouldCreateReceivers()) {
    test->SetReceivers(receiver_call_->Receiver(), sender_call_->Receiver());
  } else {
    // Sender-only call delivers to itself.
    test->SetReceivers(sender_call_->Receiver(), NULL);
  }

  CreateSendConfig(test->GetNumStreams());
  if (test->ShouldCreateReceivers()) {
    CreateMatchingReceiveConfigs();
  }
  test->ModifyConfigs(&send_config_, &receive_configs_, &encoder_config_);
  CreateStreams();
  test->OnStreamsCreated(send_stream_, receive_streams_);

  CreateFrameGeneratorCapturer();
  test->OnFrameGeneratorCapturerCreated(frame_generator_capturer_.get());

  Start();
  test->PerformTest();
  test->StopSending();
  Stop();

  DestroyStreams();
}

void CallTest::Start() {
  send_stream_->Start();
  for (size_t i = 0; i < receive_streams_.size(); ++i)
    receive_streams_[i]->Start();
  if (frame_generator_capturer_.get() != NULL)
    frame_generator_capturer_->Start();
}

void CallTest::Stop() {
  if (frame_generator_capturer_.get() != NULL)
    frame_generator_capturer_->Stop();
  for (size_t i = 0; i < receive_streams_.size(); ++i)
    receive_streams_[i]->Stop();
  send_stream_->Stop();
}

void CallTest::CreateCalls(const Call::Config& sender_config,
                           const Call::Config& receiver_config) {
  CreateSenderCall(sender_config);
  CreateReceiverCall(receiver_config);
}

void CallTest::CreateSenderCall(const Call::Config& config) {
  sender_call_.reset(Call::Create(config));
}

void CallTest::CreateReceiverCall(const Call::Config& config) {
  receiver_call_.reset(Call::Create(config));
}

void CallTest::CreateSendConfig(size_t num_streams) {
  assert(num_streams <= kNumSsrcs);
  send_config_ = VideoSendStream::Config();
  send_config_.encoder_settings.encoder = &fake_encoder_;
  send_config_.encoder_settings.payload_name = "FAKE";
  send_config_.encoder_settings.payload_type = kFakeSendPayloadType;
  encoder_config_.streams = test::CreateVideoStreams(num_streams);
  for (size_t i = 0; i < num_streams; ++i)
    send_config_.rtp.ssrcs.push_back(kSendSsrcs[i]);
  send_config_.rtp.extensions.push_back(
      RtpExtension(RtpExtension::kVideoRotation, kVideoRotationRtpExtensionId));
}

void CallTest::CreateMatchingReceiveConfigs() {
  assert(!send_config_.rtp.ssrcs.empty());
  assert(receive_configs_.empty());
  assert(allocated_decoders_.empty());
  VideoReceiveStream::Config config;
  config.rtp.local_ssrc = kReceiverLocalSsrc;
  for (size_t i = 0; i < send_config_.rtp.ssrcs.size(); ++i) {
    VideoReceiveStream::Decoder decoder =
        test::CreateMatchingDecoder(send_config_.encoder_settings);
    allocated_decoders_.push_back(decoder.decoder);
    config.decoders.clear();
    config.decoders.push_back(decoder);
    config.rtp.remote_ssrc = send_config_.rtp.ssrcs[i];
    receive_configs_.push_back(config);
  }
}

void CallTest::CreateFrameGeneratorCapturer() {
  VideoStream stream = encoder_config_.streams.back();
  frame_generator_capturer_.reset(
      test::FrameGeneratorCapturer::Create(send_stream_->Input(),
                                           stream.width,
                                           stream.height,
                                           stream.max_framerate,
                                           clock_));
}
void CallTest::CreateStreams() {
  assert(send_stream_ == NULL);
  assert(receive_streams_.empty());

  send_stream_ =
      sender_call_->CreateVideoSendStream(send_config_, encoder_config_);

  for (size_t i = 0; i < receive_configs_.size(); ++i) {
    receive_streams_.push_back(
        receiver_call_->CreateVideoReceiveStream(receive_configs_[i]));
  }
}

void CallTest::DestroyStreams() {
  if (send_stream_ != NULL)
    sender_call_->DestroyVideoSendStream(send_stream_);
  send_stream_ = NULL;
  for (size_t i = 0; i < receive_streams_.size(); ++i)
    receiver_call_->DestroyVideoReceiveStream(receive_streams_[i]);
  receive_streams_.clear();
  allocated_decoders_.clear();
}

const unsigned int CallTest::kDefaultTimeoutMs = 30 * 1000;
const unsigned int CallTest::kLongTimeoutMs = 120 * 1000;
const uint8_t CallTest::kSendPayloadType = 100;
const uint8_t CallTest::kFakeSendPayloadType = 125;
const uint8_t CallTest::kSendRtxPayloadType = 98;
const uint8_t CallTest::kRedPayloadType = 118;
const uint8_t CallTest::kUlpfecPayloadType = 119;
const uint32_t CallTest::kSendRtxSsrcs[kNumSsrcs] = {0xBADCAFD, 0xBADCAFE,
                                                     0xBADCAFF};
const uint32_t CallTest::kSendSsrcs[kNumSsrcs] = {0xC0FFED, 0xC0FFEE, 0xC0FFEF};
const uint32_t CallTest::kReceiverLocalSsrc = 0x123456;
const int CallTest::kNackRtpHistoryMs = 1000;

BaseTest::BaseTest(unsigned int timeout_ms) : RtpRtcpObserver(timeout_ms) {
}

BaseTest::BaseTest(unsigned int timeout_ms,
                   const FakeNetworkPipe::Config& config)
    : RtpRtcpObserver(timeout_ms, config) {
}

BaseTest::~BaseTest() {
}

Call::Config BaseTest::GetSenderCallConfig() {
  return Call::Config(SendTransport());
}

Call::Config BaseTest::GetReceiverCallConfig() {
  return Call::Config(ReceiveTransport());
}

void BaseTest::OnCallsCreated(Call* sender_call, Call* receiver_call) {
}

size_t BaseTest::GetNumStreams() const {
  return 1;
}

void BaseTest::ModifyConfigs(
    VideoSendStream::Config* send_config,
    std::vector<VideoReceiveStream::Config>* receive_configs,
    VideoEncoderConfig* encoder_config) {
}

void BaseTest::OnStreamsCreated(
    VideoSendStream* send_stream,
    const std::vector<VideoReceiveStream*>& receive_streams) {
}

void BaseTest::OnFrameGeneratorCapturerCreated(
    FrameGeneratorCapturer* frame_generator_capturer) {
}

SendTest::SendTest(unsigned int timeout_ms) : BaseTest(timeout_ms) {
}

SendTest::SendTest(unsigned int timeout_ms,
                   const FakeNetworkPipe::Config& config)
    : BaseTest(timeout_ms, config) {
}

bool SendTest::ShouldCreateReceivers() const {
  return false;
}

EndToEndTest::EndToEndTest(unsigned int timeout_ms) : BaseTest(timeout_ms) {
}

EndToEndTest::EndToEndTest(unsigned int timeout_ms,
                           const FakeNetworkPipe::Config& config)
    : BaseTest(timeout_ms, config) {
}

bool EndToEndTest::ShouldCreateReceivers() const {
  return true;
}

}  // namespace test
}  // namespace webrtc
