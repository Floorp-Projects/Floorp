/*
 *  Copyright 2023 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "audio/channel_receive.h"

#include "api/crypto/frame_decryptor_interface.h"
#include "api/task_queue/default_task_queue_factory.h"
#include "logging/rtc_event_log/mock/mock_rtc_event_log.h"
#include "modules/audio_device/include/audio_device.h"
#include "modules/audio_device/include/mock_audio_device.h"
#include "modules/rtp_rtcp/source/byte_io.h"
#include "modules/rtp_rtcp/source/rtcp_packet/receiver_report.h"
#include "rtc_base/logging.h"
#include "rtc_base/thread.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/mock_transport.h"
#include "test/time_controller/simulated_time_controller.h"

namespace webrtc {
namespace voe {
namespace {

using ::testing::NiceMock;
using ::testing::NotNull;
using ::testing::Test;

constexpr uint32_t kLocalSsrc = 1111;
constexpr uint32_t kRemoteSsrc = 2222;

class ChannelReceiveTest : public Test {
 public:
  ChannelReceiveTest()
      : time_controller_(Timestamp::Seconds(5555)),
        audio_device_module_(test::MockAudioDeviceModule::CreateStrict()) {}

  std::unique_ptr<ChannelReceiveInterface> CreateTestChannelReceive() {
    CryptoOptions crypto_options;
    return CreateChannelReceive(
        time_controller_.GetClock(),
        /* neteq_factory= */ nullptr, audio_device_module_.get(), &transport_,
        &event_log_, kLocalSsrc, kRemoteSsrc,
        /* jitter_buffer_max_packets= */ 0,
        /* jitter_buffer_fast_playout= */ false,
        /* jitter_buffer_min_delay_ms= */ 0,
        /* enable_non_sender_rtt= */ false,
        /* decoder_factory= */ nullptr,
        /* codec_pair_id= */ absl::nullopt,
        /* frame_decryptor_interface= */ nullptr, crypto_options,
        /* frame_transformer= */ nullptr);
  }

  NtpTime NtpNow() { return time_controller_.GetClock()->CurrentNtpTime(); }

 protected:
  GlobalSimulatedTimeController time_controller_;
  rtc::scoped_refptr<test::MockAudioDeviceModule> audio_device_module_;
  MockTransport transport_;
  NiceMock<MockRtcEventLog> event_log_;
};

TEST_F(ChannelReceiveTest, CreateAndDestroy) {
  auto channel = CreateTestChannelReceive();
  EXPECT_THAT(channel, NotNull());
}

TEST_F(ChannelReceiveTest, ReceiveReportGeneratedOnTime) {
  auto channel = CreateTestChannelReceive();
  channel->SetReceiveCodecs({{10, {"L16", 44100, 1}}});

  bool receiver_report_sent = false;
  EXPECT_CALL(transport_, SendRtcp)
      .WillRepeatedly([&](const uint8_t* packet, size_t length) {
        if (length >= 2 && packet[1] == rtcp::ReceiverReport::kPacketType) {
          receiver_report_sent = true;
        }
        return true;
      });
  // RFC 3550 section 6.2 mentions 5 seconds as a reasonable expectation
  // for the interval between RTCP packets.
  time_controller_.AdvanceTime(TimeDelta::Seconds(5));

  EXPECT_TRUE(receiver_report_sent);
}

}  // namespace
}  // namespace voe
}  // namespace webrtc
