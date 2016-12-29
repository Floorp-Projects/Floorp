/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "webrtc/base/checks.h"
#include "webrtc/common.h"
#include "webrtc/config.h"
#include "webrtc/test/call_test.h"
#include "webrtc/test/encoder_settings.h"
#include "webrtc/test/testsupport/fileutils.h"
#include "webrtc/voice_engine/include/voe_base.h"
#include "webrtc/voice_engine/include/voe_codec.h"
#include "webrtc/voice_engine/include/voe_network.h"

namespace webrtc {
namespace test {

namespace {
const int kVideoRotationRtpExtensionId = 4;
}

CallTest::CallTest()
    : clock_(Clock::GetRealTimeClock()),
      video_send_config_(nullptr),
      video_send_stream_(nullptr),
      audio_send_config_(nullptr),
      audio_send_stream_(nullptr),
      fake_encoder_(clock_),
      num_video_streams_(0),
      num_audio_streams_(0),
      fake_send_audio_device_(nullptr),
      fake_recv_audio_device_(nullptr) {}

CallTest::~CallTest() {
}

void CallTest::RunBaseTest(BaseTest* test) {
  num_video_streams_ = test->GetNumVideoStreams();
  num_audio_streams_ = test->GetNumAudioStreams();
  RTC_DCHECK(num_video_streams_ > 0 || num_audio_streams_ > 0);
  Call::Config send_config(test->GetSenderCallConfig());
  if (num_audio_streams_ > 0) {
    CreateVoiceEngines();
    AudioState::Config audio_state_config;
    audio_state_config.voice_engine = voe_send_.voice_engine;
    send_config.audio_state = AudioState::Create(audio_state_config);
  }
  CreateSenderCall(send_config);
  if (test->ShouldCreateReceivers()) {
    Call::Config recv_config(test->GetReceiverCallConfig());
    if (num_audio_streams_ > 0) {
      AudioState::Config audio_state_config;
      audio_state_config.voice_engine = voe_recv_.voice_engine;
      recv_config.audio_state = AudioState::Create(audio_state_config);
    }
    CreateReceiverCall(recv_config);
  }
  send_transport_.reset(test->CreateSendTransport(sender_call_.get()));
  receive_transport_.reset(test->CreateReceiveTransport());
  test->OnCallsCreated(sender_call_.get(), receiver_call_.get());

  if (test->ShouldCreateReceivers()) {
    send_transport_->SetReceiver(receiver_call_->Receiver());
    receive_transport_->SetReceiver(sender_call_->Receiver());
  } else {
    // Sender-only call delivers to itself.
    send_transport_->SetReceiver(sender_call_->Receiver());
    receive_transport_->SetReceiver(nullptr);
  }

  CreateSendConfig(num_video_streams_, num_audio_streams_,
                   send_transport_.get());
  if (test->ShouldCreateReceivers()) {
    CreateMatchingReceiveConfigs(receive_transport_.get());
  }
  if (num_audio_streams_ > 0)
    SetupVoiceEngineTransports(send_transport_.get(), receive_transport_.get());

  if (num_video_streams_ > 0) {
    test->ModifyVideoConfigs(&video_send_config_, &video_receive_configs_,
                             &video_encoder_config_);
  }
  if (num_audio_streams_ > 0)
    test->ModifyAudioConfigs(&audio_send_config_, &audio_receive_configs_);

  if (num_video_streams_ > 0) {
    CreateVideoStreams();
    test->OnVideoStreamsCreated(video_send_stream_, video_receive_streams_);
  }
  if (num_audio_streams_ > 0) {
    CreateAudioStreams();
    test->OnAudioStreamsCreated(audio_send_stream_, audio_receive_streams_);
  }

  CreateFrameGeneratorCapturer();
  test->OnFrameGeneratorCapturerCreated(frame_generator_capturer_.get());

  Start();
  test->PerformTest();
  send_transport_->StopSending();
  receive_transport_->StopSending();
  Stop();

  DestroyStreams();
  DestroyCalls();
  if (num_audio_streams_ > 0)
    DestroyVoiceEngines();
}

void CallTest::Start() {
  if (video_send_stream_)
    video_send_stream_->Start();
  for (VideoReceiveStream* video_recv_stream : video_receive_streams_)
    video_recv_stream->Start();
  if (audio_send_stream_) {
    fake_send_audio_device_->Start();
    audio_send_stream_->Start();
    EXPECT_EQ(0, voe_send_.base->StartSend(voe_send_.channel_id));
  }
  for (AudioReceiveStream* audio_recv_stream : audio_receive_streams_)
    audio_recv_stream->Start();
  if (!audio_receive_streams_.empty()) {
    fake_recv_audio_device_->Start();
    EXPECT_EQ(0, voe_recv_.base->StartPlayout(voe_recv_.channel_id));
    EXPECT_EQ(0, voe_recv_.base->StartReceive(voe_recv_.channel_id));
  }
  if (frame_generator_capturer_.get() != NULL)
    frame_generator_capturer_->Start();
}

void CallTest::Stop() {
  if (frame_generator_capturer_.get() != NULL)
    frame_generator_capturer_->Stop();
  if (!audio_receive_streams_.empty()) {
    fake_recv_audio_device_->Stop();
    EXPECT_EQ(0, voe_recv_.base->StopReceive(voe_recv_.channel_id));
    EXPECT_EQ(0, voe_recv_.base->StopPlayout(voe_recv_.channel_id));
  }
  for (AudioReceiveStream* audio_recv_stream : audio_receive_streams_)
    audio_recv_stream->Stop();
  if (audio_send_stream_) {
    fake_send_audio_device_->Stop();
    EXPECT_EQ(0, voe_send_.base->StopSend(voe_send_.channel_id));
    audio_send_stream_->Stop();
  }
  for (VideoReceiveStream* video_recv_stream : video_receive_streams_)
    video_recv_stream->Stop();
  if (video_send_stream_)
    video_send_stream_->Stop();
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

void CallTest::DestroyCalls() {
  sender_call_.reset();
  receiver_call_.reset();
}

void CallTest::CreateSendConfig(size_t num_video_streams,
                                size_t num_audio_streams,
                                Transport* send_transport) {
  RTC_DCHECK(num_video_streams <= kNumSsrcs);
  RTC_DCHECK_LE(num_audio_streams, 1u);
  RTC_DCHECK(num_audio_streams == 0 || voe_send_.channel_id >= 0);
  video_send_config_ = VideoSendStream::Config(send_transport);
  video_send_config_.encoder_settings.encoder = &fake_encoder_;
  video_send_config_.encoder_settings.payload_name = "FAKE";
  video_send_config_.encoder_settings.payload_type = kFakeVideoSendPayloadType;
  video_send_config_.rtp.extensions.push_back(
      RtpExtension(RtpExtension::kAbsSendTime, kAbsSendTimeExtensionId));
  video_encoder_config_.streams = test::CreateVideoStreams(num_video_streams);
  for (size_t i = 0; i < num_video_streams; ++i)
    video_send_config_.rtp.ssrcs.push_back(kVideoSendSsrcs[i]);
  video_send_config_.rtp.extensions.push_back(
      RtpExtension(RtpExtension::kVideoRotation, kVideoRotationRtpExtensionId));

  if (num_audio_streams > 0) {
    audio_send_config_ = AudioSendStream::Config(send_transport);
    audio_send_config_.voe_channel_id = voe_send_.channel_id;
    audio_send_config_.rtp.ssrc = kAudioSendSsrc;
  }
}

void CallTest::CreateMatchingReceiveConfigs(Transport* rtcp_send_transport) {
  RTC_DCHECK(!video_send_config_.rtp.ssrcs.empty());
  RTC_DCHECK(video_receive_configs_.empty());
  RTC_DCHECK(allocated_decoders_.empty());
  RTC_DCHECK(num_audio_streams_ == 0 || voe_send_.channel_id >= 0);
  VideoReceiveStream::Config video_config(rtcp_send_transport);
  video_config.rtp.remb = true;
  video_config.rtp.local_ssrc = kReceiverLocalVideoSsrc;
  for (const RtpExtension& extension : video_send_config_.rtp.extensions)
    video_config.rtp.extensions.push_back(extension);
  for (size_t i = 0; i < video_send_config_.rtp.ssrcs.size(); ++i) {
    VideoReceiveStream::Decoder decoder =
        test::CreateMatchingDecoder(video_send_config_.encoder_settings);
    allocated_decoders_.push_back(decoder.decoder);
    video_config.decoders.clear();
    video_config.decoders.push_back(decoder);
    video_config.rtp.remote_ssrc = video_send_config_.rtp.ssrcs[i];
    video_receive_configs_.push_back(video_config);
  }

  RTC_DCHECK(num_audio_streams_ <= 1);
  if (num_audio_streams_ == 1) {
    AudioReceiveStream::Config audio_config;
    audio_config.rtp.local_ssrc = kReceiverLocalAudioSsrc;
    audio_config.rtcp_send_transport = rtcp_send_transport;
    audio_config.voe_channel_id = voe_recv_.channel_id;
    audio_config.rtp.remote_ssrc = audio_send_config_.rtp.ssrc;
    audio_receive_configs_.push_back(audio_config);
  }
}

void CallTest::CreateFrameGeneratorCapturer() {
  VideoStream stream = video_encoder_config_.streams.back();
  frame_generator_capturer_.reset(test::FrameGeneratorCapturer::Create(
      video_send_stream_->Input(), stream.width, stream.height,
      stream.max_framerate, clock_));
}

void CallTest::CreateFakeAudioDevices() {
  fake_send_audio_device_.reset(new FakeAudioDevice(
      clock_, test::ResourcePath("voice_engine/audio_long16", "pcm")));
  fake_recv_audio_device_.reset(new FakeAudioDevice(
      clock_, test::ResourcePath("voice_engine/audio_long16", "pcm")));
}

void CallTest::CreateVideoStreams() {
  RTC_DCHECK(video_send_stream_ == nullptr);
  RTC_DCHECK(video_receive_streams_.empty());
  RTC_DCHECK(audio_send_stream_ == nullptr);
  RTC_DCHECK(audio_receive_streams_.empty());

  video_send_stream_ = sender_call_->CreateVideoSendStream(
      video_send_config_, video_encoder_config_);
  for (size_t i = 0; i < video_receive_configs_.size(); ++i) {
    video_receive_streams_.push_back(
        receiver_call_->CreateVideoReceiveStream(video_receive_configs_[i]));
  }
}

void CallTest::CreateAudioStreams() {
  audio_send_stream_ = sender_call_->CreateAudioSendStream(audio_send_config_);
  for (size_t i = 0; i < audio_receive_configs_.size(); ++i) {
    audio_receive_streams_.push_back(
        receiver_call_->CreateAudioReceiveStream(audio_receive_configs_[i]));
  }
  CodecInst isac = {kAudioSendPayloadType, "ISAC", 16000, 480, 1, 32000};
  EXPECT_EQ(0, voe_send_.codec->SetSendCodec(voe_send_.channel_id, isac));
}

void CallTest::DestroyStreams() {
  if (video_send_stream_)
    sender_call_->DestroyVideoSendStream(video_send_stream_);
  video_send_stream_ = nullptr;
  for (VideoReceiveStream* video_recv_stream : video_receive_streams_)
    receiver_call_->DestroyVideoReceiveStream(video_recv_stream);

  if (audio_send_stream_)
    sender_call_->DestroyAudioSendStream(audio_send_stream_);
  audio_send_stream_ = nullptr;
  for (AudioReceiveStream* audio_recv_stream : audio_receive_streams_)
    receiver_call_->DestroyAudioReceiveStream(audio_recv_stream);
  video_receive_streams_.clear();

  allocated_decoders_.clear();
}

void CallTest::CreateVoiceEngines() {
  CreateFakeAudioDevices();
  voe_send_.voice_engine = VoiceEngine::Create();
  voe_send_.base = VoEBase::GetInterface(voe_send_.voice_engine);
  voe_send_.network = VoENetwork::GetInterface(voe_send_.voice_engine);
  voe_send_.codec = VoECodec::GetInterface(voe_send_.voice_engine);
  EXPECT_EQ(0, voe_send_.base->Init(fake_send_audio_device_.get(), nullptr));
  Config voe_config;
  voe_config.Set<VoicePacing>(new VoicePacing(true));
  voe_send_.channel_id = voe_send_.base->CreateChannel(voe_config);
  EXPECT_GE(voe_send_.channel_id, 0);

  voe_recv_.voice_engine = VoiceEngine::Create();
  voe_recv_.base = VoEBase::GetInterface(voe_recv_.voice_engine);
  voe_recv_.network = VoENetwork::GetInterface(voe_recv_.voice_engine);
  voe_recv_.codec = VoECodec::GetInterface(voe_recv_.voice_engine);
  EXPECT_EQ(0, voe_recv_.base->Init(fake_recv_audio_device_.get(), nullptr));
  voe_recv_.channel_id = voe_recv_.base->CreateChannel();
  EXPECT_GE(voe_recv_.channel_id, 0);
}

void CallTest::SetupVoiceEngineTransports(PacketTransport* send_transport,
                                          PacketTransport* recv_transport) {
  voe_send_.transport_adapter.reset(
      new internal::TransportAdapter(send_transport));
  voe_send_.transport_adapter->Enable();
  EXPECT_EQ(0, voe_send_.network->RegisterExternalTransport(
                   voe_send_.channel_id, *voe_send_.transport_adapter.get()));

  voe_recv_.transport_adapter.reset(
      new internal::TransportAdapter(recv_transport));
  voe_recv_.transport_adapter->Enable();
  EXPECT_EQ(0, voe_recv_.network->RegisterExternalTransport(
                   voe_recv_.channel_id, *voe_recv_.transport_adapter.get()));
}

void CallTest::DestroyVoiceEngines() {
  voe_recv_.base->DeleteChannel(voe_recv_.channel_id);
  voe_recv_.channel_id = -1;
  voe_recv_.base->Release();
  voe_recv_.base = nullptr;
  voe_recv_.network->Release();
  voe_recv_.network = nullptr;
  voe_recv_.codec->Release();
  voe_recv_.codec = nullptr;

  voe_send_.base->DeleteChannel(voe_send_.channel_id);
  voe_send_.channel_id = -1;
  voe_send_.base->Release();
  voe_send_.base = nullptr;
  voe_send_.network->Release();
  voe_send_.network = nullptr;
  voe_send_.codec->Release();
  voe_send_.codec = nullptr;

  VoiceEngine::Delete(voe_send_.voice_engine);
  voe_send_.voice_engine = nullptr;
  VoiceEngine::Delete(voe_recv_.voice_engine);
  voe_recv_.voice_engine = nullptr;
}

const int CallTest::kDefaultTimeoutMs = 30 * 1000;
const int CallTest::kLongTimeoutMs = 120 * 1000;
const uint8_t CallTest::kVideoSendPayloadType = 100;
const uint8_t CallTest::kFakeVideoSendPayloadType = 125;
const uint8_t CallTest::kSendRtxPayloadType = 98;
const uint8_t CallTest::kRedPayloadType = 118;
const uint8_t CallTest::kRtxRedPayloadType = 99;
const uint8_t CallTest::kUlpfecPayloadType = 119;
const uint8_t CallTest::kAudioSendPayloadType = 103;
const uint32_t CallTest::kSendRtxSsrcs[kNumSsrcs] = {0xBADCAFD, 0xBADCAFE,
                                                     0xBADCAFF};
const uint32_t CallTest::kVideoSendSsrcs[kNumSsrcs] = {0xC0FFED, 0xC0FFEE,
                                                       0xC0FFEF};
const uint32_t CallTest::kAudioSendSsrc = 0xDEADBEEF;
const uint32_t CallTest::kReceiverLocalVideoSsrc = 0x123456;
const uint32_t CallTest::kReceiverLocalAudioSsrc = 0x1234567;
const int CallTest::kNackRtpHistoryMs = 1000;

BaseTest::BaseTest(unsigned int timeout_ms) : RtpRtcpObserver(timeout_ms) {
}

BaseTest::~BaseTest() {
}

Call::Config BaseTest::GetSenderCallConfig() {
  return Call::Config();
}

Call::Config BaseTest::GetReceiverCallConfig() {
  return Call::Config();
}

void BaseTest::OnCallsCreated(Call* sender_call, Call* receiver_call) {
}

test::PacketTransport* BaseTest::CreateSendTransport(Call* sender_call) {
  return new PacketTransport(sender_call, this, test::PacketTransport::kSender,
                             FakeNetworkPipe::Config());
}

test::PacketTransport* BaseTest::CreateReceiveTransport() {
  return new PacketTransport(nullptr, this, test::PacketTransport::kReceiver,
                             FakeNetworkPipe::Config());
}

size_t BaseTest::GetNumVideoStreams() const {
  return 1;
}

size_t BaseTest::GetNumAudioStreams() const {
  return 0;
}

void BaseTest::ModifyVideoConfigs(
    VideoSendStream::Config* send_config,
    std::vector<VideoReceiveStream::Config>* receive_configs,
    VideoEncoderConfig* encoder_config) {}

void BaseTest::OnVideoStreamsCreated(
    VideoSendStream* send_stream,
    const std::vector<VideoReceiveStream*>& receive_streams) {}

void BaseTest::ModifyAudioConfigs(
    AudioSendStream::Config* send_config,
    std::vector<AudioReceiveStream::Config>* receive_configs) {}

void BaseTest::OnAudioStreamsCreated(
    AudioSendStream* send_stream,
    const std::vector<AudioReceiveStream*>& receive_streams) {}

void BaseTest::OnFrameGeneratorCapturerCreated(
    FrameGeneratorCapturer* frame_generator_capturer) {
}

SendTest::SendTest(unsigned int timeout_ms) : BaseTest(timeout_ms) {
}

bool SendTest::ShouldCreateReceivers() const {
  return false;
}

EndToEndTest::EndToEndTest(unsigned int timeout_ms) : BaseTest(timeout_ms) {
}

bool EndToEndTest::ShouldCreateReceivers() const {
  return true;
}

}  // namespace test
}  // namespace webrtc
