/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/call_test.h"

#include <algorithm>

#include "api/audio_codecs/builtin_audio_decoder_factory.h"
#include "api/audio_codecs/builtin_audio_encoder_factory.h"
#include "call/rtp_transport_controller_send.h"
#include "call/video_config.h"
#include "modules/audio_mixer/audio_mixer_impl.h"
#include "rtc_base/checks.h"
#include "rtc_base/event.h"
#include "rtc_base/ptr_util.h"
#include "test/testsupport/fileutils.h"
#include "voice_engine/include/voe_base.h"

namespace webrtc {
namespace test {

namespace {
const int kVideoRotationRtpExtensionId = 4;
}

CallTest::CallTest()
    : clock_(Clock::GetRealTimeClock()),
      event_log_(RtcEventLog::CreateNull()),
      sender_call_transport_controller_(nullptr),
      video_send_config_(nullptr),
      video_send_stream_(nullptr),
      audio_send_config_(nullptr),
      audio_send_stream_(nullptr),
      fake_encoder_(clock_),
      num_video_streams_(1),
      num_audio_streams_(0),
      num_flexfec_streams_(0),
      decoder_factory_(CreateBuiltinAudioDecoderFactory()),
      encoder_factory_(CreateBuiltinAudioEncoderFactory()),
      task_queue_("CallTestTaskQueue"),
      fake_send_audio_device_(nullptr),
      fake_recv_audio_device_(nullptr) {}

CallTest::~CallTest() {
  task_queue_.SendTask([this]() {
    fake_send_audio_device_.reset();
    fake_recv_audio_device_.reset();
    frame_generator_capturer_.reset();
  });
}

void CallTest::RunBaseTest(BaseTest* test) {
  task_queue_.SendTask([this, test]() {
    num_video_streams_ = test->GetNumVideoStreams();
    num_audio_streams_ = test->GetNumAudioStreams();
    num_flexfec_streams_ = test->GetNumFlexfecStreams();
    RTC_DCHECK(num_video_streams_ > 0 || num_audio_streams_ > 0);
    Call::Config send_config(test->GetSenderCallConfig());
    if (num_audio_streams_ > 0) {
      CreateFakeAudioDevices(test->CreateCapturer(), test->CreateRenderer());
      test->OnFakeAudioDevicesCreated(fake_send_audio_device_.get(),
                                      fake_recv_audio_device_.get());
      apm_send_ = AudioProcessing::Create();
      apm_recv_ = AudioProcessing::Create();
      CreateVoiceEngines();
      AudioState::Config audio_state_config;
      audio_state_config.voice_engine = voe_send_.voice_engine;
      audio_state_config.audio_mixer = AudioMixerImpl::Create();
      audio_state_config.audio_processing = apm_send_;
      send_config.audio_state = AudioState::Create(audio_state_config);
      fake_send_audio_device_->RegisterAudioCallback(
          send_config.audio_state->audio_transport());
    }
    CreateSenderCall(send_config);
    if (sender_call_transport_controller_ != nullptr) {
      test->OnRtpTransportControllerSendCreated(
          sender_call_transport_controller_);
    }
    if (test->ShouldCreateReceivers()) {
      Call::Config recv_config(test->GetReceiverCallConfig());
      if (num_audio_streams_ > 0) {
        AudioState::Config audio_state_config;
        audio_state_config.voice_engine = voe_recv_.voice_engine;
        audio_state_config.audio_mixer = AudioMixerImpl::Create();
        audio_state_config.audio_processing = apm_recv_;
        recv_config.audio_state = AudioState::Create(audio_state_config);
        fake_recv_audio_device_->RegisterAudioCallback(
            recv_config.audio_state->audio_transport());      }
      CreateReceiverCall(recv_config);
    }
    test->OnCallsCreated(sender_call_.get(), receiver_call_.get());
    receive_transport_.reset(test->CreateReceiveTransport(&task_queue_));
    send_transport_.reset(
        test->CreateSendTransport(&task_queue_, sender_call_.get()));

    if (test->ShouldCreateReceivers()) {
      send_transport_->SetReceiver(receiver_call_->Receiver());
      receive_transport_->SetReceiver(sender_call_->Receiver());
      if (num_video_streams_ > 0)
        receiver_call_->SignalChannelNetworkState(MediaType::VIDEO, kNetworkUp);
      if (num_audio_streams_ > 0)
        receiver_call_->SignalChannelNetworkState(MediaType::AUDIO, kNetworkUp);
    } else {
      // Sender-only call delivers to itself.
      send_transport_->SetReceiver(sender_call_->Receiver());
      receive_transport_->SetReceiver(nullptr);
    }

    CreateSendConfig(num_video_streams_, num_audio_streams_,
                     num_flexfec_streams_, send_transport_.get());
    if (test->ShouldCreateReceivers()) {
      CreateMatchingReceiveConfigs(receive_transport_.get());
    }
    if (num_video_streams_ > 0) {
      test->ModifyVideoConfigs(&video_send_config_, &video_receive_configs_,
                               &video_encoder_config_);
    }
    if (num_audio_streams_ > 0) {
      test->ModifyAudioConfigs(&audio_send_config_, &audio_receive_configs_);
    }
    if (num_flexfec_streams_ > 0) {
      test->ModifyFlexfecConfigs(&flexfec_receive_configs_);
    }

    if (num_flexfec_streams_ > 0) {
      CreateFlexfecStreams();
      test->OnFlexfecStreamsCreated(flexfec_receive_streams_);
    }
    if (num_video_streams_ > 0) {
      CreateVideoStreams();
      test->OnVideoStreamsCreated(video_send_stream_, video_receive_streams_);
    }
    if (num_audio_streams_ > 0) {
      CreateAudioStreams();
      test->OnAudioStreamsCreated(audio_send_stream_, audio_receive_streams_);
    }

    if (num_video_streams_ > 0) {
      int width = kDefaultWidth;
      int height = kDefaultHeight;
      int frame_rate = kDefaultFramerate;
      test->ModifyVideoCaptureStartResolution(&width, &height, &frame_rate);
      CreateFrameGeneratorCapturer(frame_rate, width, height);
      test->OnFrameGeneratorCapturerCreated(frame_generator_capturer_.get());
    }

    Start();
  });

  test->PerformTest();

  task_queue_.SendTask([this, test]() {
    Stop();
    test->OnStreamsStopped();
    DestroyStreams();
    send_transport_.reset();
    receive_transport_.reset();
    DestroyCalls();
    if (num_audio_streams_ > 0)
      DestroyVoiceEngines();
  });
}

void CallTest::CreateCalls(const Call::Config& sender_config,
                           const Call::Config& receiver_config) {
  CreateSenderCall(sender_config);
  CreateReceiverCall(receiver_config);
}

void CallTest::CreateSenderCall(const Call::Config& config) {
  sender_call_transport_controller_ = new RtpTransportControllerSend(
      Clock::GetRealTimeClock(), config.event_log);

  sender_call_.reset(
      Call::Create(config, std::unique_ptr<RtpTransportControllerSend>(
                               sender_call_transport_controller_)));
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
                                size_t num_flexfec_streams,
                                Transport* send_transport) {
  RTC_DCHECK(num_video_streams <= kNumSsrcs);
  RTC_DCHECK_LE(num_audio_streams, 1);
  RTC_DCHECK_LE(num_flexfec_streams, 1);
  RTC_DCHECK(num_audio_streams == 0 || voe_send_.channel_id >= 0);
  if (num_video_streams > 0) {
    video_send_config_ = VideoSendStream::Config(send_transport);
    video_send_config_.encoder_settings.encoder = &fake_encoder_;
    video_send_config_.encoder_settings.payload_name = "FAKE";
    video_send_config_.encoder_settings.payload_type =
        kFakeVideoSendPayloadType;
    video_send_config_.rtp.extensions.push_back(
        RtpExtension(RtpExtension::kTransportSequenceNumberUri,
                     kTransportSequenceNumberExtensionId));
    video_send_config_.rtp.extensions.push_back(RtpExtension(
        RtpExtension::kVideoContentTypeUri, kVideoContentTypeExtensionId));
    FillEncoderConfiguration(num_video_streams, &video_encoder_config_);

    for (size_t i = 0; i < num_video_streams; ++i)
      video_send_config_.rtp.ssrcs.push_back(kVideoSendSsrcs[i]);
    video_send_config_.rtp.extensions.push_back(RtpExtension(
        RtpExtension::kVideoRotationUri, kVideoRotationRtpExtensionId));
  }

  if (num_audio_streams > 0) {
    audio_send_config_ = AudioSendStream::Config(send_transport);
    audio_send_config_.voe_channel_id = voe_send_.channel_id;
    audio_send_config_.rtp.ssrc = kAudioSendSsrc;
    audio_send_config_.send_codec_spec =
        rtc::Optional<AudioSendStream::Config::SendCodecSpec>(
            {kAudioSendPayloadType, {"opus", 48000, 2, {{"stereo", "1"}}}});
    audio_send_config_.encoder_factory = encoder_factory_;
  }

  // TODO(brandtr): Update this when we support multistream protection.
  if (num_flexfec_streams > 0) {
    video_send_config_.rtp.flexfec.payload_type = kFlexfecPayloadType;
    video_send_config_.rtp.flexfec.ssrc = kFlexfecSendSsrc;
    video_send_config_.rtp.flexfec.protected_media_ssrcs = {kVideoSendSsrcs[0]};
  }
}

void CallTest::CreateMatchingReceiveConfigs(Transport* rtcp_send_transport) {
  video_receive_configs_.clear();
  allocated_decoders_.clear();
  if (num_video_streams_ > 0) {
    RTC_DCHECK(!video_send_config_.rtp.ssrcs.empty());
    VideoReceiveStream::Config video_config(rtcp_send_transport);
    video_config.rtp.remb = false;
    video_config.rtp.transport_cc = true;
    video_config.rtp.local_ssrc = kReceiverLocalVideoSsrc;
    for (const RtpExtension& extension : video_send_config_.rtp.extensions)
      video_config.rtp.extensions.push_back(extension);
    video_config.renderer = &fake_renderer_;
    for (size_t i = 0; i < video_send_config_.rtp.ssrcs.size(); ++i) {
      VideoReceiveStream::Decoder decoder =
          test::CreateMatchingDecoder(video_send_config_.encoder_settings);
      allocated_decoders_.push_back(
          std::unique_ptr<VideoDecoder>(decoder.decoder));
      video_config.decoders.clear();
      video_config.decoders.push_back(decoder);
      video_config.rtp.remote_ssrc = video_send_config_.rtp.ssrcs[i];
      video_receive_configs_.push_back(video_config.Copy());
    }
    video_receive_configs_[0].rtp.protected_by_flexfec =
        (num_flexfec_streams_ == 1);
  }

  RTC_DCHECK_GE(1, num_audio_streams_);
  if (num_audio_streams_ == 1) {
    RTC_DCHECK_LE(0, voe_send_.channel_id);
    AudioReceiveStream::Config audio_config;
    audio_config.rtp.local_ssrc = kReceiverLocalAudioSsrc;
    audio_config.rtcp_send_transport = rtcp_send_transport;
    audio_config.voe_channel_id = voe_recv_.channel_id;
    audio_config.rtp.remote_ssrc = audio_send_config_.rtp.ssrc;
    audio_config.decoder_factory = decoder_factory_;
    audio_config.decoder_map = {{kAudioSendPayloadType, {"opus", 48000, 2}}};
    audio_receive_configs_.push_back(audio_config);
  }

  // TODO(brandtr): Update this when we support multistream protection.
  RTC_DCHECK(num_flexfec_streams_ <= 1);
  if (num_flexfec_streams_ == 1) {
    FlexfecReceiveStream::Config config(rtcp_send_transport);
    config.payload_type = kFlexfecPayloadType;
    config.remote_ssrc = kFlexfecSendSsrc;
    config.protected_media_ssrcs = {kVideoSendSsrcs[0]};
    config.local_ssrc = kReceiverLocalVideoSsrc;
    for (const RtpExtension& extension : video_send_config_.rtp.extensions)
      config.rtp_header_extensions.push_back(extension);
    flexfec_receive_configs_.push_back(config);
  }
}

void CallTest::CreateFrameGeneratorCapturerWithDrift(Clock* clock,
                                                     float speed,
                                                     int framerate,
                                                     int width,
                                                     int height) {
  frame_generator_capturer_.reset(test::FrameGeneratorCapturer::Create(
      width, height, framerate * speed, clock));
  video_send_stream_->SetSource(
      frame_generator_capturer_.get(),
      VideoSendStream::DegradationPreference::kMaintainFramerate);
}

void CallTest::CreateFrameGeneratorCapturer(int framerate,
                                            int width,
                                            int height) {
  frame_generator_capturer_.reset(
      test::FrameGeneratorCapturer::Create(width, height, framerate, clock_));
  video_send_stream_->SetSource(
      frame_generator_capturer_.get(),
      VideoSendStream::DegradationPreference::kMaintainFramerate);
}

void CallTest::CreateFakeAudioDevices(
    std::unique_ptr<FakeAudioDevice::Capturer> capturer,
    std::unique_ptr<FakeAudioDevice::Renderer> renderer) {
  fake_send_audio_device_.reset(new FakeAudioDevice(
      std::move(capturer), nullptr, 1.f));
  fake_recv_audio_device_.reset(new FakeAudioDevice(
      nullptr, std::move(renderer), 1.f));
}

void CallTest::CreateVideoStreams() {
  RTC_DCHECK(video_send_stream_ == nullptr);
  RTC_DCHECK(video_receive_streams_.empty());
  RTC_DCHECK(audio_send_stream_ == nullptr);
  RTC_DCHECK(audio_receive_streams_.empty());

  video_send_stream_ = sender_call_->CreateVideoSendStream(
      video_send_config_.Copy(), video_encoder_config_.Copy());
  for (size_t i = 0; i < video_receive_configs_.size(); ++i) {
    video_receive_streams_.push_back(receiver_call_->CreateVideoReceiveStream(
        video_receive_configs_[i].Copy()));
  }

  AssociateFlexfecStreamsWithVideoStreams();
}

void CallTest::CreateAudioStreams() {
  audio_send_stream_ = sender_call_->CreateAudioSendStream(audio_send_config_);
  for (size_t i = 0; i < audio_receive_configs_.size(); ++i) {
    audio_receive_streams_.push_back(
        receiver_call_->CreateAudioReceiveStream(audio_receive_configs_[i]));
  }
}

void CallTest::CreateFlexfecStreams() {
  for (size_t i = 0; i < flexfec_receive_configs_.size(); ++i) {
    flexfec_receive_streams_.push_back(
        receiver_call_->CreateFlexfecReceiveStream(
            flexfec_receive_configs_[i]));
  }

  AssociateFlexfecStreamsWithVideoStreams();
}

void CallTest::AssociateFlexfecStreamsWithVideoStreams() {
  // All FlexFEC streams protect all of the video streams.
  for (FlexfecReceiveStream* flexfec_recv_stream : flexfec_receive_streams_) {
    for (VideoReceiveStream* video_recv_stream : video_receive_streams_) {
      video_recv_stream->AddSecondarySink(flexfec_recv_stream);
    }
  }
}

void CallTest::DissociateFlexfecStreamsFromVideoStreams() {
  for (FlexfecReceiveStream* flexfec_recv_stream : flexfec_receive_streams_) {
    for (VideoReceiveStream* video_recv_stream : video_receive_streams_) {
      video_recv_stream->RemoveSecondarySink(flexfec_recv_stream);
    }
  }
}

void CallTest::Start() {
  if (video_send_stream_)
    video_send_stream_->Start();
  for (VideoReceiveStream* video_recv_stream : video_receive_streams_)
    video_recv_stream->Start();
  if (audio_send_stream_) {
    audio_send_stream_->Start();
  }
  for (AudioReceiveStream* audio_recv_stream : audio_receive_streams_)
    audio_recv_stream->Start();
  if (frame_generator_capturer_.get() != NULL)
    frame_generator_capturer_->Start();
}

void CallTest::Stop() {
  if (frame_generator_capturer_.get() != NULL)
    frame_generator_capturer_->Stop();
  for (AudioReceiveStream* audio_recv_stream : audio_receive_streams_)
    audio_recv_stream->Stop();
  if (audio_send_stream_) {
    audio_send_stream_->Stop();
  }
  for (VideoReceiveStream* video_recv_stream : video_receive_streams_)
    video_recv_stream->Stop();
  if (video_send_stream_)
    video_send_stream_->Stop();
}

void CallTest::DestroyStreams() {
  DissociateFlexfecStreamsFromVideoStreams();

  if (audio_send_stream_)
    sender_call_->DestroyAudioSendStream(audio_send_stream_);
  audio_send_stream_ = nullptr;
  for (AudioReceiveStream* audio_recv_stream : audio_receive_streams_)
    receiver_call_->DestroyAudioReceiveStream(audio_recv_stream);

  if (video_send_stream_)
    sender_call_->DestroyVideoSendStream(video_send_stream_);
  video_send_stream_ = nullptr;

  for (VideoReceiveStream* video_recv_stream : video_receive_streams_)
    receiver_call_->DestroyVideoReceiveStream(video_recv_stream);

  for (FlexfecReceiveStream* flexfec_recv_stream : flexfec_receive_streams_)
    receiver_call_->DestroyFlexfecReceiveStream(flexfec_recv_stream);

  video_receive_streams_.clear();
  allocated_decoders_.clear();
}

void CallTest::SetFakeVideoCaptureRotation(VideoRotation rotation) {
  frame_generator_capturer_->SetFakeRotation(rotation);
}

void CallTest::CreateVoiceEngines() {
  voe_send_.voice_engine = VoiceEngine::Create();
  voe_send_.base = VoEBase::GetInterface(voe_send_.voice_engine);
  EXPECT_EQ(0, fake_send_audio_device_->Init());
  EXPECT_EQ(0, voe_send_.base->Init(fake_send_audio_device_.get(),
                                    apm_send_.get(), decoder_factory_));
  VoEBase::ChannelConfig config;
  config.enable_voice_pacing = true;
  voe_send_.channel_id = voe_send_.base->CreateChannel(config);
  EXPECT_GE(voe_send_.channel_id, 0);

  voe_recv_.voice_engine = VoiceEngine::Create();
  voe_recv_.base = VoEBase::GetInterface(voe_recv_.voice_engine);
  EXPECT_EQ(0, fake_recv_audio_device_->Init());
  EXPECT_EQ(0, voe_recv_.base->Init(fake_recv_audio_device_.get(),
                                    apm_recv_.get(), decoder_factory_));
  voe_recv_.channel_id = voe_recv_.base->CreateChannel();
  EXPECT_GE(voe_recv_.channel_id, 0);
}

void CallTest::DestroyVoiceEngines() {
  voe_recv_.base->DeleteChannel(voe_recv_.channel_id);
  voe_recv_.channel_id = -1;
  voe_recv_.base->Release();
  voe_recv_.base = nullptr;

  voe_send_.base->DeleteChannel(voe_send_.channel_id);
  voe_send_.channel_id = -1;
  voe_send_.base->Release();
  voe_send_.base = nullptr;

  VoiceEngine::Delete(voe_send_.voice_engine);
  voe_send_.voice_engine = nullptr;
  VoiceEngine::Delete(voe_recv_.voice_engine);
  voe_recv_.voice_engine = nullptr;
}

const int CallTest::kDefaultWidth;
const int CallTest::kDefaultHeight;
const int CallTest::kDefaultFramerate;
const int CallTest::kDefaultTimeoutMs = 30 * 1000;
const int CallTest::kLongTimeoutMs = 120 * 1000;
const uint8_t CallTest::kVideoSendPayloadType = 100;
const uint8_t CallTest::kFakeVideoSendPayloadType = 125;
const uint8_t CallTest::kSendRtxPayloadType = 98;
const uint8_t CallTest::kRedPayloadType = 118;
const uint8_t CallTest::kRtxRedPayloadType = 99;
const uint8_t CallTest::kUlpfecPayloadType = 119;
const uint8_t CallTest::kFlexfecPayloadType = 120;
const uint8_t CallTest::kAudioSendPayloadType = 103;
const uint8_t CallTest::kPayloadTypeH264 = 122;
const uint8_t CallTest::kPayloadTypeVP8 = 123;
const uint8_t CallTest::kPayloadTypeVP9 = 124;
const uint32_t CallTest::kSendRtxSsrcs[kNumSsrcs] = {0xBADCAFD, 0xBADCAFE,
                                                     0xBADCAFF};
const uint32_t CallTest::kVideoSendSsrcs[kNumSsrcs] = {0xC0FFED, 0xC0FFEE,
                                                       0xC0FFEF};
const uint32_t CallTest::kAudioSendSsrc = 0xDEADBEEF;
const uint32_t CallTest::kFlexfecSendSsrc = 0xBADBEEF;
const uint32_t CallTest::kReceiverLocalVideoSsrc = 0x123456;
const uint32_t CallTest::kReceiverLocalAudioSsrc = 0x1234567;
const int CallTest::kNackRtpHistoryMs = 1000;

const uint8_t CallTest::kDefaultKeepalivePayloadType =
    RtpKeepAliveConfig().payload_type;

const std::map<uint8_t, MediaType> CallTest::payload_type_map_ = {
    {CallTest::kVideoSendPayloadType, MediaType::VIDEO},
    {CallTest::kFakeVideoSendPayloadType, MediaType::VIDEO},
    {CallTest::kSendRtxPayloadType, MediaType::VIDEO},
    {CallTest::kRedPayloadType, MediaType::VIDEO},
    {CallTest::kRtxRedPayloadType, MediaType::VIDEO},
    {CallTest::kUlpfecPayloadType, MediaType::VIDEO},
    {CallTest::kFlexfecPayloadType, MediaType::VIDEO},
    {CallTest::kAudioSendPayloadType, MediaType::AUDIO},
    {CallTest::kDefaultKeepalivePayloadType, MediaType::ANY}};

BaseTest::BaseTest() : event_log_(RtcEventLog::CreateNull()) {}

BaseTest::BaseTest(unsigned int timeout_ms)
    : RtpRtcpObserver(timeout_ms), event_log_(RtcEventLog::CreateNull()) {}

BaseTest::~BaseTest() {
}

std::unique_ptr<FakeAudioDevice::Capturer> BaseTest::CreateCapturer() {
  return FakeAudioDevice::CreatePulsedNoiseCapturer(256, 48000);
}

std::unique_ptr<FakeAudioDevice::Renderer> BaseTest::CreateRenderer() {
  return FakeAudioDevice::CreateDiscardRenderer(48000);
}

void BaseTest::OnFakeAudioDevicesCreated(FakeAudioDevice* send_audio_device,
                                         FakeAudioDevice* recv_audio_device) {
}

Call::Config BaseTest::GetSenderCallConfig() {
  return Call::Config(event_log_.get());
}

Call::Config BaseTest::GetReceiverCallConfig() {
  return Call::Config(event_log_.get());
}

void BaseTest::OnRtpTransportControllerSendCreated(
    RtpTransportControllerSend* controller) {}

void BaseTest::OnCallsCreated(Call* sender_call, Call* receiver_call) {
}

test::PacketTransport* BaseTest::CreateSendTransport(
    SingleThreadedTaskQueueForTesting* task_queue,
    Call* sender_call) {
  return new PacketTransport(
      task_queue, sender_call, this, test::PacketTransport::kSender,
      CallTest::payload_type_map_, FakeNetworkPipe::Config());
}

test::PacketTransport* BaseTest::CreateReceiveTransport(
    SingleThreadedTaskQueueForTesting* task_queue) {
  return new PacketTransport(
      task_queue, nullptr, this, test::PacketTransport::kReceiver,
      CallTest::payload_type_map_, FakeNetworkPipe::Config());
}

size_t BaseTest::GetNumVideoStreams() const {
  return 1;
}

size_t BaseTest::GetNumAudioStreams() const {
  return 0;
}

size_t BaseTest::GetNumFlexfecStreams() const {
  return 0;
}

void BaseTest::ModifyVideoConfigs(
    VideoSendStream::Config* send_config,
    std::vector<VideoReceiveStream::Config>* receive_configs,
    VideoEncoderConfig* encoder_config) {}

void BaseTest::ModifyVideoCaptureStartResolution(int* width,
                                                 int* heigt,
                                                 int* frame_rate) {}

void BaseTest::OnVideoStreamsCreated(
    VideoSendStream* send_stream,
    const std::vector<VideoReceiveStream*>& receive_streams) {}

void BaseTest::ModifyAudioConfigs(
    AudioSendStream::Config* send_config,
    std::vector<AudioReceiveStream::Config>* receive_configs) {}

void BaseTest::OnAudioStreamsCreated(
    AudioSendStream* send_stream,
    const std::vector<AudioReceiveStream*>& receive_streams) {}

void BaseTest::ModifyFlexfecConfigs(
    std::vector<FlexfecReceiveStream::Config>* receive_configs) {}

void BaseTest::OnFlexfecStreamsCreated(
    const std::vector<FlexfecReceiveStream*>& receive_streams) {}

void BaseTest::OnFrameGeneratorCapturerCreated(
    FrameGeneratorCapturer* frame_generator_capturer) {
}

void BaseTest::OnStreamsStopped() {
}

SendTest::SendTest(unsigned int timeout_ms) : BaseTest(timeout_ms) {
}

bool SendTest::ShouldCreateReceivers() const {
  return false;
}

EndToEndTest::EndToEndTest() {}

EndToEndTest::EndToEndTest(unsigned int timeout_ms) : BaseTest(timeout_ms) {
}

bool EndToEndTest::ShouldCreateReceivers() const {
  return true;
}

}  // namespace test
}  // namespace webrtc
