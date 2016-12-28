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

#include "testing/gtest/include/gtest/gtest.h"

#include "webrtc/audio/audio_receive_stream.h"
#include "webrtc/audio/conversion.h"
#include "webrtc/call/mock/mock_congestion_controller.h"
#include "webrtc/modules/bitrate_controller/include/mock/mock_bitrate_controller.h"
#include "webrtc/modules/pacing/packet_router.h"
#include "webrtc/modules/remote_bitrate_estimator/include/mock/mock_remote_bitrate_estimator.h"
#include "webrtc/modules/rtp_rtcp/source/byte_io.h"
#include "webrtc/modules/utility/include/mock/mock_process_thread.h"
#include "webrtc/system_wrappers/include/clock.h"
#include "webrtc/test/mock_voe_channel_proxy.h"
#include "webrtc/test/mock_voice_engine.h"
#include "webrtc/video/call_stats.h"

namespace webrtc {
namespace test {
namespace {

using testing::_;
using testing::Return;

AudioDecodingCallStats MakeAudioDecodeStatsForTest() {
  AudioDecodingCallStats audio_decode_stats;
  audio_decode_stats.calls_to_silence_generator = 234;
  audio_decode_stats.calls_to_neteq = 567;
  audio_decode_stats.decoded_normal = 890;
  audio_decode_stats.decoded_plc = 123;
  audio_decode_stats.decoded_cng = 456;
  audio_decode_stats.decoded_plc_cng = 789;
  return audio_decode_stats;
}

const int kChannelId = 2;
const uint32_t kRemoteSsrc = 1234;
const uint32_t kLocalSsrc = 5678;
const size_t kOneByteExtensionHeaderLength = 4;
const size_t kOneByteExtensionLength = 4;
const int kAbsSendTimeId = 2;
const int kAudioLevelId = 3;
const int kTransportSequenceNumberId = 4;
const int kJitterBufferDelay = -7;
const int kPlayoutBufferDelay = 302;
const unsigned int kSpeechOutputLevel = 99;
const CallStatistics kCallStats = {
    345,  678,  901, 234, -12, 3456, 7890, 567, 890, 123};
const CodecInst kCodecInst = {
    123, "codec_name_recv", 96000, -187, 0, -103};
const NetworkStatistics kNetworkStats = {
    123, 456, false, 0, 0, 789, 12, 345, 678, 901, -1, -1, -1, -1, -1, 0};
const AudioDecodingCallStats kAudioDecodeStats = MakeAudioDecodeStatsForTest();

struct ConfigHelper {
  ConfigHelper()
      : simulated_clock_(123456),
        call_stats_(&simulated_clock_),
        congestion_controller_(&process_thread_,
                               &call_stats_,
                               &bitrate_observer_) {
    using testing::Invoke;

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
          EXPECT_CALL(*channel_proxy_, SetLocalSSRC(kLocalSsrc)).Times(1);
          EXPECT_CALL(*channel_proxy_,
              SetReceiveAbsoluteSenderTimeStatus(true, kAbsSendTimeId))
                  .Times(1);
          EXPECT_CALL(*channel_proxy_,
              SetReceiveAudioLevelIndicationStatus(true, kAudioLevelId))
                  .Times(1);
          EXPECT_CALL(*channel_proxy_, SetCongestionControlObjects(
                                           nullptr, nullptr, &packet_router_))
              .Times(1);
          EXPECT_CALL(congestion_controller_, packet_router())
              .WillOnce(Return(&packet_router_));
          EXPECT_CALL(*channel_proxy_,
                      SetCongestionControlObjects(nullptr, nullptr, nullptr))
              .Times(1);
          return channel_proxy_;
        }));
    stream_config_.voe_channel_id = kChannelId;
    stream_config_.rtp.local_ssrc = kLocalSsrc;
    stream_config_.rtp.remote_ssrc = kRemoteSsrc;
    stream_config_.rtp.extensions.push_back(
        RtpExtension(RtpExtension::kAbsSendTime, kAbsSendTimeId));
    stream_config_.rtp.extensions.push_back(
        RtpExtension(RtpExtension::kAudioLevel, kAudioLevelId));
  }

  MockCongestionController* congestion_controller() {
    return &congestion_controller_;
  }
  MockRemoteBitrateEstimator* remote_bitrate_estimator() {
    return &remote_bitrate_estimator_;
  }
  AudioReceiveStream::Config& config() { return stream_config_; }
  rtc::scoped_refptr<AudioState> audio_state() { return audio_state_; }
  MockVoiceEngine& voice_engine() { return voice_engine_; }

  void SetupMockForBweFeedback(bool send_side_bwe) {
    EXPECT_CALL(congestion_controller_,
                GetRemoteBitrateEstimator(send_side_bwe))
        .WillOnce(Return(&remote_bitrate_estimator_));
    EXPECT_CALL(remote_bitrate_estimator_,
                RemoveStream(stream_config_.rtp.remote_ssrc));
  }

  void SetupMockForGetStats() {
    using testing::DoAll;
    using testing::SetArgReferee;

    ASSERT_TRUE(channel_proxy_);
    EXPECT_CALL(*channel_proxy_, GetRTCPStatistics())
        .WillOnce(Return(kCallStats));
    EXPECT_CALL(*channel_proxy_, GetDelayEstimate())
        .WillOnce(Return(kJitterBufferDelay + kPlayoutBufferDelay));
    EXPECT_CALL(*channel_proxy_, GetSpeechOutputLevelFullRange())
        .WillOnce(Return(kSpeechOutputLevel));
    EXPECT_CALL(*channel_proxy_, GetNetworkStatistics())
        .WillOnce(Return(kNetworkStats));
    EXPECT_CALL(*channel_proxy_, GetDecodingCallStatistics())
        .WillOnce(Return(kAudioDecodeStats));

    EXPECT_CALL(voice_engine_, GetRecCodec(kChannelId, _))
        .WillOnce(DoAll(SetArgReferee<1>(kCodecInst), Return(0)));
  }

 private:
  SimulatedClock simulated_clock_;
  CallStats call_stats_;
  PacketRouter packet_router_;
  testing::NiceMock<MockBitrateObserver> bitrate_observer_;
  testing::NiceMock<MockProcessThread> process_thread_;
  MockCongestionController congestion_controller_;
  MockRemoteBitrateEstimator remote_bitrate_estimator_;
  testing::StrictMock<MockVoiceEngine> voice_engine_;
  rtc::scoped_refptr<AudioState> audio_state_;
  AudioReceiveStream::Config stream_config_;
  testing::StrictMock<MockVoEChannelProxy>* channel_proxy_ = nullptr;
};

void BuildOneByteExtension(std::vector<uint8_t>::iterator it,
                           int id,
                           uint32_t extension_value,
                           size_t value_length) {
  const uint16_t kRtpOneByteHeaderExtensionId = 0xBEDE;
  ByteWriter<uint16_t>::WriteBigEndian(&(*it), kRtpOneByteHeaderExtensionId);
  it += 2;

  ByteWriter<uint16_t>::WriteBigEndian(&(*it), kOneByteExtensionLength / 4);
  it += 2;
  const size_t kExtensionDataLength = kOneByteExtensionLength - 1;
  uint32_t shifted_value = extension_value
                           << (8 * (kExtensionDataLength - value_length));
  *it = (id << 4) + (value_length - 1);
  ++it;
  ByteWriter<uint32_t, kExtensionDataLength>::WriteBigEndian(&(*it),
                                                             shifted_value);
}

std::vector<uint8_t> CreateRtpHeaderWithOneByteExtension(
    int extension_id,
    uint32_t extension_value,
    size_t value_length) {
  std::vector<uint8_t> header;
  header.resize(webrtc::kRtpHeaderSize + kOneByteExtensionHeaderLength +
                kOneByteExtensionLength);
  header[0] = 0x80;   // Version 2.
  header[0] |= 0x10;  // Set extension bit.
  header[1] = 100;    // Payload type.
  header[1] |= 0x80;  // Marker bit is set.
  ByteWriter<uint16_t>::WriteBigEndian(&header[2], 0x1234);  // Sequence number.
  ByteWriter<uint32_t>::WriteBigEndian(&header[4], 0x5678);  // Timestamp.
  ByteWriter<uint32_t>::WriteBigEndian(&header[8], 0x4321);  // SSRC.

  BuildOneByteExtension(header.begin() + webrtc::kRtpHeaderSize, extension_id,
                        extension_value, value_length);
  return header;
}
}  // namespace

TEST(AudioReceiveStreamTest, ConfigToString) {
  AudioReceiveStream::Config config;
  config.rtp.remote_ssrc = kRemoteSsrc;
  config.rtp.local_ssrc = kLocalSsrc;
  config.rtp.extensions.push_back(
      RtpExtension(RtpExtension::kAbsSendTime, kAbsSendTimeId));
  config.voe_channel_id = kChannelId;
  config.combined_audio_video_bwe = true;
  EXPECT_EQ(
      "{rtp: {remote_ssrc: 1234, local_ssrc: 5678, extensions: [{name: "
      "http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time, id: 2}]}, "
      "receive_transport: nullptr, rtcp_send_transport: nullptr, "
      "voe_channel_id: 2, combined_audio_video_bwe: true}",
      config.ToString());
}

TEST(AudioReceiveStreamTest, ConstructDestruct) {
  ConfigHelper helper;
  internal::AudioReceiveStream recv_stream(
      helper.congestion_controller(), helper.config(), helper.audio_state());
}

MATCHER_P(VerifyHeaderExtension, expected_extension, "") {
  return arg.extension.hasAbsoluteSendTime ==
             expected_extension.hasAbsoluteSendTime &&
         arg.extension.absoluteSendTime ==
             expected_extension.absoluteSendTime &&
         arg.extension.hasTransportSequenceNumber ==
             expected_extension.hasTransportSequenceNumber &&
         arg.extension.transportSequenceNumber ==
             expected_extension.transportSequenceNumber;
}

TEST(AudioReceiveStreamTest, AudioPacketUpdatesBweWithTimestamp) {
  ConfigHelper helper;
  helper.config().combined_audio_video_bwe = true;
  helper.SetupMockForBweFeedback(false);
  internal::AudioReceiveStream recv_stream(
      helper.congestion_controller(), helper.config(), helper.audio_state());
  const int kAbsSendTimeValue = 1234;
  std::vector<uint8_t> rtp_packet =
      CreateRtpHeaderWithOneByteExtension(kAbsSendTimeId, kAbsSendTimeValue, 3);
  PacketTime packet_time(5678000, 0);
  const size_t kExpectedHeaderLength = 20;
  RTPHeaderExtension expected_extension;
  expected_extension.hasAbsoluteSendTime = true;
  expected_extension.absoluteSendTime = kAbsSendTimeValue;
  EXPECT_CALL(*helper.remote_bitrate_estimator(),
              IncomingPacket(packet_time.timestamp / 1000,
                             rtp_packet.size() - kExpectedHeaderLength,
                             VerifyHeaderExtension(expected_extension), false))
      .Times(1);
  EXPECT_TRUE(
      recv_stream.DeliverRtp(&rtp_packet[0], rtp_packet.size(), packet_time));
}

TEST(AudioReceiveStreamTest, AudioPacketUpdatesBweFeedback) {
  ConfigHelper helper;
  helper.config().combined_audio_video_bwe = true;
  helper.config().rtp.transport_cc = true;
  helper.config().rtp.extensions.push_back(RtpExtension(
      RtpExtension::kTransportSequenceNumber, kTransportSequenceNumberId));
  helper.SetupMockForBweFeedback(true);
  internal::AudioReceiveStream recv_stream(
      helper.congestion_controller(), helper.config(), helper.audio_state());
  const int kTransportSequenceNumberValue = 1234;
  std::vector<uint8_t> rtp_packet = CreateRtpHeaderWithOneByteExtension(
      kTransportSequenceNumberId, kTransportSequenceNumberValue, 2);
  PacketTime packet_time(5678000, 0);
  const size_t kExpectedHeaderLength = 20;
  RTPHeaderExtension expected_extension;
  expected_extension.hasTransportSequenceNumber = true;
  expected_extension.transportSequenceNumber = kTransportSequenceNumberValue;
  EXPECT_CALL(*helper.remote_bitrate_estimator(),
              IncomingPacket(packet_time.timestamp / 1000,
                             rtp_packet.size() - kExpectedHeaderLength,
                             VerifyHeaderExtension(expected_extension), false))
      .Times(1);
  EXPECT_TRUE(
      recv_stream.DeliverRtp(&rtp_packet[0], rtp_packet.size(), packet_time));
}

TEST(AudioReceiveStreamTest, GetStats) {
  ConfigHelper helper;
  internal::AudioReceiveStream recv_stream(
      helper.congestion_controller(), helper.config(), helper.audio_state());
  helper.SetupMockForGetStats();
  AudioReceiveStream::Stats stats = recv_stream.GetStats();
  EXPECT_EQ(kRemoteSsrc, stats.remote_ssrc);
  EXPECT_EQ(static_cast<int64_t>(kCallStats.bytesReceived), stats.bytes_rcvd);
  EXPECT_EQ(static_cast<uint32_t>(kCallStats.packetsReceived),
            stats.packets_rcvd);
  EXPECT_EQ(kCallStats.cumulativeLost, stats.packets_lost);
  EXPECT_EQ(Q8ToFloat(kCallStats.fractionLost), stats.fraction_lost);
  EXPECT_EQ(std::string(kCodecInst.plname), stats.codec_name);
  EXPECT_EQ(kCallStats.extendedMax, stats.ext_seqnum);
  EXPECT_EQ(kCallStats.jitterSamples / (kCodecInst.plfreq / 1000),
            stats.jitter_ms);
  EXPECT_EQ(kNetworkStats.currentBufferSize, stats.jitter_buffer_ms);
  EXPECT_EQ(kNetworkStats.preferredBufferSize,
            stats.jitter_buffer_preferred_ms);
  EXPECT_EQ(static_cast<uint32_t>(kJitterBufferDelay + kPlayoutBufferDelay),
            stats.delay_estimate_ms);
  EXPECT_EQ(static_cast<int32_t>(kSpeechOutputLevel), stats.audio_level);
  EXPECT_EQ(Q14ToFloat(kNetworkStats.currentExpandRate), stats.expand_rate);
  EXPECT_EQ(Q14ToFloat(kNetworkStats.currentSpeechExpandRate),
            stats.speech_expand_rate);
  EXPECT_EQ(Q14ToFloat(kNetworkStats.currentSecondaryDecodedRate),
            stats.secondary_decoded_rate);
  EXPECT_EQ(Q14ToFloat(kNetworkStats.currentAccelerateRate),
            stats.accelerate_rate);
  EXPECT_EQ(Q14ToFloat(kNetworkStats.currentPreemptiveRate),
            stats.preemptive_expand_rate);
  EXPECT_EQ(kAudioDecodeStats.calls_to_silence_generator,
            stats.decoding_calls_to_silence_generator);
  EXPECT_EQ(kAudioDecodeStats.calls_to_neteq, stats.decoding_calls_to_neteq);
  EXPECT_EQ(kAudioDecodeStats.decoded_normal, stats.decoding_normal);
  EXPECT_EQ(kAudioDecodeStats.decoded_plc, stats.decoding_plc);
  EXPECT_EQ(kAudioDecodeStats.decoded_cng, stats.decoding_cng);
  EXPECT_EQ(kAudioDecodeStats.decoded_plc_cng, stats.decoding_plc_cng);
  EXPECT_EQ(kCallStats.capture_start_ntp_time_ms_,
            stats.capture_start_ntp_time_ms);
}
}  // namespace test
}  // namespace webrtc
