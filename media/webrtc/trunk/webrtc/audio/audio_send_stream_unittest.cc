/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <string>
#include <vector>

#include "testing/gtest/include/gtest/gtest.h"

#include "webrtc/audio/audio_send_stream.h"
#include "webrtc/audio/audio_state.h"
#include "webrtc/audio/conversion.h"
#include "webrtc/call/congestion_controller.h"
#include "webrtc/modules/bitrate_controller/include/mock/mock_bitrate_controller.h"
#include "webrtc/modules/pacing/paced_sender.h"
#include "webrtc/test/mock_voe_channel_proxy.h"
#include "webrtc/test/mock_voice_engine.h"
#include "webrtc/video/call_stats.h"

namespace webrtc {
namespace test {
namespace {

using testing::_;
using testing::Return;

const int kChannelId = 1;
const uint32_t kSsrc = 1234;
const char* kCName = "foo_name";
const int kAudioLevelId = 2;
const int kAbsSendTimeId = 3;
const int kTransportSequenceNumberId = 4;
const int kEchoDelayMedian = 254;
const int kEchoDelayStdDev = -3;
const int kEchoReturnLoss = -65;
const int kEchoReturnLossEnhancement = 101;
const unsigned int kSpeechInputLevel = 96;
const CallStatistics kCallStats = {
    1345,  1678,  1901, 1234,  112, 13456, 17890, 1567, -1890, -1123};
const CodecInst kCodecInst = {-121, "codec_name_send", 48000, -231, 0, -671};
const ReportBlock kReportBlock = {456, 780, 123, 567, 890, 132, 143, 13354};
const int kTelephoneEventPayloadType = 123;
const uint8_t kTelephoneEventCode = 45;
const uint32_t kTelephoneEventDuration = 6789;

struct ConfigHelper {
  ConfigHelper()
      : stream_config_(nullptr),
        call_stats_(Clock::GetRealTimeClock()),
        process_thread_(ProcessThread::Create("AudioTestThread")),
        congestion_controller_(process_thread_.get(),
                               &call_stats_,
                               &bitrate_observer_) {
    using testing::Invoke;
    using testing::StrEq;

    EXPECT_CALL(voice_engine_,
        RegisterVoiceEngineObserver(_)).WillOnce(Return(0));
    EXPECT_CALL(voice_engine_,
        DeRegisterVoiceEngineObserver()).WillOnce(Return(0));
    AudioState::Config config;
    config.voice_engine = &voice_engine_;
    audio_state_ = AudioState::Create(config);

    EXPECT_CALL(voice_engine_, ChannelProxyFactory(kChannelId))
        .WillOnce(Invoke([this](int channel_id) {
          EXPECT_FALSE(channel_proxy_);
          channel_proxy_ = new testing::StrictMock<MockVoEChannelProxy>();
          EXPECT_CALL(*channel_proxy_, SetRTCPStatus(true)).Times(1);
          EXPECT_CALL(*channel_proxy_, SetLocalSSRC(kSsrc)).Times(1);
          EXPECT_CALL(*channel_proxy_, SetRTCP_CNAME(StrEq(kCName))).Times(1);
          EXPECT_CALL(*channel_proxy_,
              SetSendAbsoluteSenderTimeStatus(true, kAbsSendTimeId)).Times(1);
          EXPECT_CALL(*channel_proxy_,
              SetSendAudioLevelIndicationStatus(true, kAudioLevelId)).Times(1);
          EXPECT_CALL(*channel_proxy_, EnableSendTransportSequenceNumber(
                                           kTransportSequenceNumberId))
              .Times(1);
          EXPECT_CALL(*channel_proxy_,
                      SetCongestionControlObjects(
                          congestion_controller_.pacer(),
                          congestion_controller_.GetTransportFeedbackObserver(),
                          congestion_controller_.packet_router()))
              .Times(1);
          EXPECT_CALL(*channel_proxy_,
                      SetCongestionControlObjects(nullptr, nullptr, nullptr))
              .Times(1);
          return channel_proxy_;
        }));
    stream_config_.voe_channel_id = kChannelId;
    stream_config_.rtp.ssrc = kSsrc;
    stream_config_.rtp.c_name = kCName;
    stream_config_.rtp.extensions.push_back(
        RtpExtension(RtpExtension::kAudioLevel, kAudioLevelId));
    stream_config_.rtp.extensions.push_back(
        RtpExtension(RtpExtension::kAbsSendTime, kAbsSendTimeId));
    stream_config_.rtp.extensions.push_back(RtpExtension(
        RtpExtension::kTransportSequenceNumber, kTransportSequenceNumberId));
  }

  AudioSendStream::Config& config() { return stream_config_; }
  rtc::scoped_refptr<AudioState> audio_state() { return audio_state_; }
  CongestionController* congestion_controller() {
    return &congestion_controller_;
  }

  void SetupMockForSendTelephoneEvent() {
    EXPECT_TRUE(channel_proxy_);
    EXPECT_CALL(*channel_proxy_,
        SetSendTelephoneEventPayloadType(kTelephoneEventPayloadType))
            .WillOnce(Return(true));
    EXPECT_CALL(*channel_proxy_,
        SendTelephoneEventOutband(kTelephoneEventCode, kTelephoneEventDuration))
            .WillOnce(Return(true));
  }

  void SetupMockForGetStats() {
    using testing::DoAll;
    using testing::SetArgReferee;

    std::vector<ReportBlock> report_blocks;
    webrtc::ReportBlock block = kReportBlock;
    report_blocks.push_back(block);  // Has wrong SSRC.
    block.source_SSRC = kSsrc;
    report_blocks.push_back(block);  // Correct block.
    block.fraction_lost = 0;
    report_blocks.push_back(block);  // Duplicate SSRC, bad fraction_lost.

    EXPECT_TRUE(channel_proxy_);
    EXPECT_CALL(*channel_proxy_, GetRTCPStatistics())
        .WillRepeatedly(Return(kCallStats));
    EXPECT_CALL(*channel_proxy_, GetRemoteRTCPReportBlocks())
        .WillRepeatedly(Return(report_blocks));

    EXPECT_CALL(voice_engine_, GetSendCodec(kChannelId, _))
        .WillRepeatedly(DoAll(SetArgReferee<1>(kCodecInst), Return(0)));
    EXPECT_CALL(voice_engine_, GetSpeechInputLevelFullRange(_))
        .WillRepeatedly(DoAll(SetArgReferee<0>(kSpeechInputLevel), Return(0)));
    EXPECT_CALL(voice_engine_, GetEcMetricsStatus(_))
        .WillRepeatedly(DoAll(SetArgReferee<0>(true), Return(0)));
    EXPECT_CALL(voice_engine_, GetEchoMetrics(_, _, _, _))
        .WillRepeatedly(DoAll(SetArgReferee<0>(kEchoReturnLoss),
                        SetArgReferee<1>(kEchoReturnLossEnhancement),
                        Return(0)));
    EXPECT_CALL(voice_engine_, GetEcDelayMetrics(_, _, _))
        .WillRepeatedly(DoAll(SetArgReferee<0>(kEchoDelayMedian),
                        SetArgReferee<1>(kEchoDelayStdDev), Return(0)));
  }

 private:
  testing::StrictMock<MockVoiceEngine> voice_engine_;
  rtc::scoped_refptr<AudioState> audio_state_;
  AudioSendStream::Config stream_config_;
  testing::StrictMock<MockVoEChannelProxy>* channel_proxy_ = nullptr;
  CallStats call_stats_;
  testing::NiceMock<MockBitrateObserver> bitrate_observer_;
  rtc::scoped_ptr<ProcessThread> process_thread_;
  CongestionController congestion_controller_;
};
}  // namespace

TEST(AudioSendStreamTest, ConfigToString) {
  AudioSendStream::Config config(nullptr);
  config.rtp.ssrc = kSsrc;
  config.rtp.extensions.push_back(
      RtpExtension(RtpExtension::kAbsSendTime, kAbsSendTimeId));
  config.rtp.c_name = kCName;
  config.voe_channel_id = kChannelId;
  config.cng_payload_type = 42;
  config.red_payload_type = 17;
  EXPECT_EQ(
      "{rtp: {ssrc: 1234, extensions: [{name: "
      "http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time, id: 3}], "
      "c_name: foo_name}, voe_channel_id: 1, cng_payload_type: 42, "
      "red_payload_type: 17}",
      config.ToString());
}

TEST(AudioSendStreamTest, ConstructDestruct) {
  ConfigHelper helper;
  internal::AudioSendStream send_stream(helper.config(), helper.audio_state(),
                                        helper.congestion_controller());
}

TEST(AudioSendStreamTest, SendTelephoneEvent) {
  ConfigHelper helper;
  internal::AudioSendStream send_stream(helper.config(), helper.audio_state(),
                                        helper.congestion_controller());
  helper.SetupMockForSendTelephoneEvent();
  EXPECT_TRUE(send_stream.SendTelephoneEvent(kTelephoneEventPayloadType,
      kTelephoneEventCode, kTelephoneEventDuration));
}

TEST(AudioSendStreamTest, GetStats) {
  ConfigHelper helper;
  internal::AudioSendStream send_stream(helper.config(), helper.audio_state(),
                                        helper.congestion_controller());
  helper.SetupMockForGetStats();
  AudioSendStream::Stats stats = send_stream.GetStats();
  EXPECT_EQ(kSsrc, stats.local_ssrc);
  EXPECT_EQ(static_cast<int64_t>(kCallStats.bytesSent), stats.bytes_sent);
  EXPECT_EQ(kCallStats.packetsSent, stats.packets_sent);
  EXPECT_EQ(static_cast<int32_t>(kReportBlock.cumulative_num_packets_lost),
            stats.packets_lost);
  EXPECT_EQ(Q8ToFloat(kReportBlock.fraction_lost), stats.fraction_lost);
  EXPECT_EQ(std::string(kCodecInst.plname), stats.codec_name);
  EXPECT_EQ(static_cast<int32_t>(kReportBlock.extended_highest_sequence_number),
            stats.ext_seqnum);
  EXPECT_EQ(static_cast<int32_t>(kReportBlock.interarrival_jitter /
                                 (kCodecInst.plfreq / 1000)),
            stats.jitter_ms);
  EXPECT_EQ(kCallStats.rttMs, stats.rtt_ms);
  EXPECT_EQ(static_cast<int32_t>(kSpeechInputLevel), stats.audio_level);
  EXPECT_EQ(-1, stats.aec_quality_min);
  EXPECT_EQ(kEchoDelayMedian, stats.echo_delay_median_ms);
  EXPECT_EQ(kEchoDelayStdDev, stats.echo_delay_std_ms);
  EXPECT_EQ(kEchoReturnLoss, stats.echo_return_loss);
  EXPECT_EQ(kEchoReturnLossEnhancement, stats.echo_return_loss_enhancement);
  EXPECT_FALSE(stats.typing_noise_detected);
}

TEST(AudioSendStreamTest, GetStatsTypingNoiseDetected) {
  ConfigHelper helper;
  internal::AudioSendStream send_stream(helper.config(), helper.audio_state(),
                                        helper.congestion_controller());
  helper.SetupMockForGetStats();
  EXPECT_FALSE(send_stream.GetStats().typing_noise_detected);

  internal::AudioState* internal_audio_state =
      static_cast<internal::AudioState*>(helper.audio_state().get());
  VoiceEngineObserver* voe_observer =
      static_cast<VoiceEngineObserver*>(internal_audio_state);
  voe_observer->CallbackOnError(-1, VE_TYPING_NOISE_WARNING);
  EXPECT_TRUE(send_stream.GetStats().typing_noise_detected);
  voe_observer->CallbackOnError(-1, VE_TYPING_NOISE_OFF_WARNING);
  EXPECT_FALSE(send_stream.GetStats().typing_noise_detected);
}
}  // namespace test
}  // namespace webrtc
