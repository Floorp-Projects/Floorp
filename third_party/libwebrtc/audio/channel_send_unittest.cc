/*
 *  Copyright 2023 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "audio/channel_send.h"

#include <utility>

#include "api/audio/audio_frame.h"
#include "api/audio_codecs/builtin_audio_encoder_factory.h"
#include "api/rtc_event_log/rtc_event_log.h"
#include "api/scoped_refptr.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "call/rtp_transport_controller_send.h"
#include "test/gtest.h"
#include "test/mock_transport.h"
#include "test/scoped_key_value_config.h"
#include "test/time_controller/simulated_time_controller.h"

namespace webrtc {
namespace voe {
namespace {

constexpr int kRtcpIntervalMs = 1000;
constexpr int kSsrc = 333;
constexpr int kPayloadType = 1;

BitrateConstraints GetBitrateConfig() {
  BitrateConstraints bitrate_config;
  bitrate_config.min_bitrate_bps = 10000;
  bitrate_config.start_bitrate_bps = 100000;
  bitrate_config.max_bitrate_bps = 1000000;
  return bitrate_config;
}

std::unique_ptr<AudioFrame> CreateAudioFrame() {
  auto frame = std::make_unique<AudioFrame>();
  frame->samples_per_channel_ = 480;
  frame->sample_rate_hz_ = 48000;
  frame->num_channels_ = 1;
  return frame;
}

class ChannelSendTest : public ::testing::Test {
 protected:
  ChannelSendTest()
      : time_controller_(Timestamp::Seconds(1)),
        transport_controller_(
            time_controller_.GetClock(),
            RtpTransportConfig{
                .bitrate_config = GetBitrateConfig(),
                .event_log = &event_log_,
                .task_queue_factory = time_controller_.GetTaskQueueFactory(),
                .trials = &field_trials_,
            }) {
    transport_controller_.EnsureStarted();
  }

  std::unique_ptr<ChannelSendInterface> CreateChannelSend() {
    return voe::CreateChannelSend(
        time_controller_.GetClock(), time_controller_.GetTaskQueueFactory(),
        &transport_, nullptr, &event_log_, nullptr, crypto_options_, false,
        kRtcpIntervalMs, kSsrc, nullptr, nullptr, field_trials_);
  }

  GlobalSimulatedTimeController time_controller_;
  webrtc::test::ScopedKeyValueConfig field_trials_;
  RtcEventLogNull event_log_;
  MockTransport transport_;
  RtpTransportControllerSend transport_controller_;
  CryptoOptions crypto_options_;
};

TEST_F(ChannelSendTest, StopSendShouldResetEncoder) {
  std::unique_ptr<ChannelSendInterface> channel = CreateChannelSend();
  rtc::scoped_refptr<AudioEncoderFactory> encoder_factory =
      CreateBuiltinAudioEncoderFactory();
  std::unique_ptr<AudioEncoder> encoder = encoder_factory->MakeAudioEncoder(
      kPayloadType, SdpAudioFormat("opus", 48000, 2), {});
  channel->SetEncoder(kPayloadType, std::move(encoder));
  channel->RegisterSenderCongestionControlObjects(&transport_controller_,
                                                  nullptr);
  channel->StartSend();

  // Insert two frames which should trigger a new packet.
  EXPECT_CALL(transport_, SendRtp).Times(1);
  channel->ProcessAndEncodeAudio(CreateAudioFrame());
  time_controller_.AdvanceTime(webrtc::TimeDelta::Zero());
  channel->ProcessAndEncodeAudio(CreateAudioFrame());
  time_controller_.AdvanceTime(webrtc::TimeDelta::Zero());

  EXPECT_CALL(transport_, SendRtp).Times(0);
  channel->ProcessAndEncodeAudio(CreateAudioFrame());
  time_controller_.AdvanceTime(webrtc::TimeDelta::Zero());
  // StopSend should clear the previous audio frame stored in the encoder.
  channel->StopSend();
  channel->StartSend();
  // The following frame should not trigger a new packet since the encoder
  // needs 20 ms audio.
  channel->ProcessAndEncodeAudio(CreateAudioFrame());
  time_controller_.AdvanceTime(webrtc::TimeDelta::Zero());
}

}  // namespace
}  // namespace voe
}  // namespace webrtc
