/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/rtcp_sender.h"

#include <memory>
#include <utility>

#include "absl/base/macros.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "modules/rtp_rtcp/source/rtcp_packet/bye.h"
#include "modules/rtp_rtcp/source/rtcp_packet/common_header.h"
#include "modules/rtp_rtcp/source/rtp_packet_received.h"
#include "modules/rtp_rtcp/source/rtp_rtcp_impl2.h"
#include "rtc_base/rate_limiter.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/mock_transport.h"
#include "test/rtcp_packet_parser.h"

using ::testing::_;
using ::testing::ElementsAre;
using ::testing::Invoke;
using ::testing::SizeIs;

namespace webrtc {

class RtcpPacketTypeCounterObserverImpl : public RtcpPacketTypeCounterObserver {
 public:
  RtcpPacketTypeCounterObserverImpl() : ssrc_(0) {}
  ~RtcpPacketTypeCounterObserverImpl() override = default;
  void RtcpPacketTypesCounterUpdated(
      uint32_t ssrc,
      const RtcpPacketTypeCounter& packet_counter) override {
    ssrc_ = ssrc;
    counter_ = packet_counter;
  }
  uint32_t ssrc_;
  RtcpPacketTypeCounter counter_;
};

class TestTransport : public Transport {
 public:
  TestTransport() {}

  bool SendRtp(const uint8_t* /*data*/,
               size_t /*len*/,
               const PacketOptions& options) override {
    return false;
  }
  bool SendRtcp(const uint8_t* data, size_t len) override {
    parser_.Parse(data, len);
    return true;
  }
  test::RtcpPacketParser parser_;
};

namespace {
static const uint32_t kSenderSsrc = 0x11111111;
static const uint32_t kRemoteSsrc = 0x22222222;
static const uint32_t kStartRtpTimestamp = 0x34567;
static const uint32_t kRtpTimestamp = 0x45678;
}  // namespace

class RtcpSenderTest : public ::testing::Test {
 protected:
  RtcpSenderTest()
      : clock_(1335900000),
        receive_statistics_(ReceiveStatistics::Create(&clock_)),
        retransmission_rate_limiter_(&clock_, 1000) {
    RtpRtcpInterface::Configuration configuration = GetDefaultConfig();
    rtp_rtcp_impl_.reset(new ModuleRtpRtcpImpl2(configuration));
    rtcp_sender_.reset(new RTCPSender(configuration));
    rtcp_sender_->SetRemoteSSRC(kRemoteSsrc);
    rtcp_sender_->SetTimestampOffset(kStartRtpTimestamp);
    rtcp_sender_->SetLastRtpTime(kRtpTimestamp, clock_.TimeInMilliseconds(),
                                 /*payload_type=*/0);
  }

  RtpRtcpInterface::Configuration GetDefaultConfig() {
    RtpRtcpInterface::Configuration configuration;
    configuration.audio = false;
    configuration.clock = &clock_;
    configuration.outgoing_transport = &test_transport_;
    configuration.retransmission_rate_limiter = &retransmission_rate_limiter_;
    configuration.rtcp_report_interval_ms = 1000;
    configuration.receive_statistics = receive_statistics_.get();
    configuration.local_media_ssrc = kSenderSsrc;
    return configuration;
  }

  void InsertIncomingPacket(uint32_t remote_ssrc, uint16_t seq_num) {
    RtpPacketReceived packet;
    packet.SetSsrc(remote_ssrc);
    packet.SetSequenceNumber(seq_num);
    packet.SetTimestamp(12345);
    packet.SetPayloadSize(100 - 12);
    receive_statistics_->OnRtpPacket(packet);
  }

  test::RtcpPacketParser* parser() { return &test_transport_.parser_; }

  RTCPSender::FeedbackState feedback_state() {
    return rtp_rtcp_impl_->GetFeedbackState();
  }

  SimulatedClock clock_;
  TestTransport test_transport_;
  std::unique_ptr<ReceiveStatistics> receive_statistics_;
  std::unique_ptr<ModuleRtpRtcpImpl2> rtp_rtcp_impl_;
  std::unique_ptr<RTCPSender> rtcp_sender_;
  RateLimiter retransmission_rate_limiter_;
};

TEST_F(RtcpSenderTest, SetRtcpStatus) {
  EXPECT_EQ(RtcpMode::kOff, rtcp_sender_->Status());
  rtcp_sender_->SetRTCPStatus(RtcpMode::kReducedSize);
  EXPECT_EQ(RtcpMode::kReducedSize, rtcp_sender_->Status());
}

TEST_F(RtcpSenderTest, SetSendingStatus) {
  EXPECT_FALSE(rtcp_sender_->Sending());
  EXPECT_EQ(0, rtcp_sender_->SetSendingStatus(feedback_state(), true));
  EXPECT_TRUE(rtcp_sender_->Sending());
}

TEST_F(RtcpSenderTest, NoPacketSentIfOff) {
  rtcp_sender_->SetRTCPStatus(RtcpMode::kOff);
  EXPECT_EQ(-1, rtcp_sender_->SendRTCP(feedback_state(), kRtcpSr));
}

TEST_F(RtcpSenderTest, SendSr) {
  const uint32_t kPacketCount = 0x12345;
  const uint32_t kOctetCount = 0x23456;
  rtcp_sender_->SetRTCPStatus(RtcpMode::kReducedSize);
  RTCPSender::FeedbackState feedback_state = rtp_rtcp_impl_->GetFeedbackState();
  rtcp_sender_->SetSendingStatus(feedback_state, true);
  feedback_state.packets_sent = kPacketCount;
  feedback_state.media_bytes_sent = kOctetCount;
  NtpTime ntp = clock_.CurrentNtpTime();
  EXPECT_EQ(0, rtcp_sender_->SendRTCP(feedback_state, kRtcpSr));
  EXPECT_EQ(1, parser()->sender_report()->num_packets());
  EXPECT_EQ(kSenderSsrc, parser()->sender_report()->sender_ssrc());
  EXPECT_EQ(ntp, parser()->sender_report()->ntp());
  EXPECT_EQ(kPacketCount, parser()->sender_report()->sender_packet_count());
  EXPECT_EQ(kOctetCount, parser()->sender_report()->sender_octet_count());
  EXPECT_EQ(kStartRtpTimestamp + kRtpTimestamp,
            parser()->sender_report()->rtp_timestamp());
  EXPECT_EQ(0U, parser()->sender_report()->report_blocks().size());
}

TEST_F(RtcpSenderTest, SendConsecutiveSrWithExactSlope) {
  const uint32_t kPacketCount = 0x12345;
  const uint32_t kOctetCount = 0x23456;
  const int kTimeBetweenSRsUs = 10043;  // Not exact value in milliseconds.
  const int kExtraPackets = 30;
  // Make sure clock is not exactly at some milliseconds point.
  clock_.AdvanceTimeMicroseconds(kTimeBetweenSRsUs);
  rtcp_sender_->SetRTCPStatus(RtcpMode::kReducedSize);
  RTCPSender::FeedbackState feedback_state = rtp_rtcp_impl_->GetFeedbackState();
  rtcp_sender_->SetSendingStatus(feedback_state, true);
  feedback_state.packets_sent = kPacketCount;
  feedback_state.media_bytes_sent = kOctetCount;

  EXPECT_EQ(0, rtcp_sender_->SendRTCP(feedback_state, kRtcpSr));
  EXPECT_EQ(1, parser()->sender_report()->num_packets());
  NtpTime ntp1 = parser()->sender_report()->ntp();
  uint32_t rtp1 = parser()->sender_report()->rtp_timestamp();

  // Send more SRs to ensure slope is always exact for different offsets
  for (int packets = 1; packets <= kExtraPackets; ++packets) {
    clock_.AdvanceTimeMicroseconds(kTimeBetweenSRsUs);
    EXPECT_EQ(0, rtcp_sender_->SendRTCP(feedback_state, kRtcpSr));
    EXPECT_EQ(packets + 1, parser()->sender_report()->num_packets());

    NtpTime ntp2 = parser()->sender_report()->ntp();
    uint32_t rtp2 = parser()->sender_report()->rtp_timestamp();

    uint32_t ntp_diff_in_rtp_units =
        (ntp2.ToMs() - ntp1.ToMs()) * (kVideoPayloadTypeFrequency / 1000);
    EXPECT_EQ(rtp2 - rtp1, ntp_diff_in_rtp_units);
  }
}

TEST_F(RtcpSenderTest, DoNotSendSrBeforeRtp) {
  RtpRtcpInterface::Configuration config;
  config.clock = &clock_;
  config.receive_statistics = receive_statistics_.get();
  config.outgoing_transport = &test_transport_;
  config.rtcp_report_interval_ms = 1000;
  config.local_media_ssrc = kSenderSsrc;
  rtcp_sender_.reset(new RTCPSender(config));
  rtcp_sender_->SetRemoteSSRC(kRemoteSsrc);
  rtcp_sender_->SetRTCPStatus(RtcpMode::kReducedSize);
  rtcp_sender_->SetSendingStatus(feedback_state(), true);

  // Sender Report shouldn't be send as an SR nor as a Report.
  rtcp_sender_->SendRTCP(feedback_state(), kRtcpSr);
  EXPECT_EQ(0, parser()->sender_report()->num_packets());
  rtcp_sender_->SendRTCP(feedback_state(), kRtcpReport);
  EXPECT_EQ(0, parser()->sender_report()->num_packets());
  // Other packets (e.g. Pli) are allowed, even if useless.
  EXPECT_EQ(0, rtcp_sender_->SendRTCP(feedback_state(), kRtcpPli));
  EXPECT_EQ(1, parser()->pli()->num_packets());
}

TEST_F(RtcpSenderTest, DoNotSendCompundBeforeRtp) {
  RtpRtcpInterface::Configuration config;
  config.clock = &clock_;
  config.receive_statistics = receive_statistics_.get();
  config.outgoing_transport = &test_transport_;
  config.rtcp_report_interval_ms = 1000;
  config.local_media_ssrc = kSenderSsrc;
  rtcp_sender_.reset(new RTCPSender(config));
  rtcp_sender_->SetRemoteSSRC(kRemoteSsrc);
  rtcp_sender_->SetRTCPStatus(RtcpMode::kCompound);
  rtcp_sender_->SetSendingStatus(feedback_state(), true);

  // In compound mode no packets are allowed (e.g. Pli) because compound mode
  // should start with Sender Report.
  EXPECT_EQ(-1, rtcp_sender_->SendRTCP(feedback_state(), kRtcpPli));
  EXPECT_EQ(0, parser()->pli()->num_packets());
}

TEST_F(RtcpSenderTest, SendRr) {
  rtcp_sender_->SetRTCPStatus(RtcpMode::kReducedSize);
  EXPECT_EQ(0, rtcp_sender_->SendRTCP(feedback_state(), kRtcpRr));
  EXPECT_EQ(1, parser()->receiver_report()->num_packets());
  EXPECT_EQ(kSenderSsrc, parser()->receiver_report()->sender_ssrc());
  EXPECT_EQ(0U, parser()->receiver_report()->report_blocks().size());
}

TEST_F(RtcpSenderTest, SendRrWithOneReportBlock) {
  const uint16_t kSeqNum = 11111;
  InsertIncomingPacket(kRemoteSsrc, kSeqNum);
  rtcp_sender_->SetRTCPStatus(RtcpMode::kCompound);
  EXPECT_EQ(0, rtcp_sender_->SendRTCP(feedback_state(), kRtcpRr));
  EXPECT_EQ(1, parser()->receiver_report()->num_packets());
  EXPECT_EQ(kSenderSsrc, parser()->receiver_report()->sender_ssrc());
  ASSERT_EQ(1U, parser()->receiver_report()->report_blocks().size());
  const rtcp::ReportBlock& rb = parser()->receiver_report()->report_blocks()[0];
  EXPECT_EQ(kRemoteSsrc, rb.source_ssrc());
  EXPECT_EQ(0U, rb.fraction_lost());
  EXPECT_EQ(0, rb.cumulative_lost_signed());
  EXPECT_EQ(kSeqNum, rb.extended_high_seq_num());
}

TEST_F(RtcpSenderTest, SendRrWithTwoReportBlocks) {
  const uint16_t kSeqNum = 11111;
  InsertIncomingPacket(kRemoteSsrc, kSeqNum);
  InsertIncomingPacket(kRemoteSsrc + 1, kSeqNum + 1);
  rtcp_sender_->SetRTCPStatus(RtcpMode::kCompound);
  EXPECT_EQ(0, rtcp_sender_->SendRTCP(feedback_state(), kRtcpRr));
  EXPECT_EQ(1, parser()->receiver_report()->num_packets());
  EXPECT_EQ(kSenderSsrc, parser()->receiver_report()->sender_ssrc());
  EXPECT_EQ(2U, parser()->receiver_report()->report_blocks().size());
  EXPECT_EQ(kRemoteSsrc,
            parser()->receiver_report()->report_blocks()[0].source_ssrc());
  EXPECT_EQ(kRemoteSsrc + 1,
            parser()->receiver_report()->report_blocks()[1].source_ssrc());
}

TEST_F(RtcpSenderTest, SendSdes) {
  rtcp_sender_->SetRTCPStatus(RtcpMode::kReducedSize);
  EXPECT_EQ(0, rtcp_sender_->SetCNAME("alice@host"));
  EXPECT_EQ(0, rtcp_sender_->SendRTCP(feedback_state(), kRtcpSdes));
  EXPECT_EQ(1, parser()->sdes()->num_packets());
  EXPECT_EQ(1U, parser()->sdes()->chunks().size());
  EXPECT_EQ(kSenderSsrc, parser()->sdes()->chunks()[0].ssrc);
  EXPECT_EQ("alice@host", parser()->sdes()->chunks()[0].cname);
}

TEST_F(RtcpSenderTest, SendSdesWithMaxChunks) {
  rtcp_sender_->SetRTCPStatus(RtcpMode::kReducedSize);
  EXPECT_EQ(0, rtcp_sender_->SetCNAME("alice@host"));
  const char cname[] = "smith@host";
  for (size_t i = 0; i < 30; ++i) {
    const uint32_t csrc = 0x1234 + i;
    EXPECT_EQ(0, rtcp_sender_->AddMixedCNAME(csrc, cname));
  }
  EXPECT_EQ(0, rtcp_sender_->SendRTCP(feedback_state(), kRtcpSdes));
  EXPECT_EQ(1, parser()->sdes()->num_packets());
  EXPECT_EQ(31U, parser()->sdes()->chunks().size());
}

TEST_F(RtcpSenderTest, SdesIncludedInCompoundPacket) {
  rtcp_sender_->SetRTCPStatus(RtcpMode::kCompound);
  EXPECT_EQ(0, rtcp_sender_->SetCNAME("alice@host"));
  EXPECT_EQ(0, rtcp_sender_->SendRTCP(feedback_state(), kRtcpReport));
  EXPECT_EQ(1, parser()->receiver_report()->num_packets());
  EXPECT_EQ(1, parser()->sdes()->num_packets());
  EXPECT_EQ(1U, parser()->sdes()->chunks().size());
}

TEST_F(RtcpSenderTest, SendBye) {
  rtcp_sender_->SetRTCPStatus(RtcpMode::kReducedSize);
  EXPECT_EQ(0, rtcp_sender_->SendRTCP(feedback_state(), kRtcpBye));
  EXPECT_EQ(1, parser()->bye()->num_packets());
  EXPECT_EQ(kSenderSsrc, parser()->bye()->sender_ssrc());
}

TEST_F(RtcpSenderTest, StopSendingTriggersBye) {
  rtcp_sender_->SetRTCPStatus(RtcpMode::kReducedSize);
  EXPECT_EQ(0, rtcp_sender_->SetSendingStatus(feedback_state(), true));
  EXPECT_EQ(0, rtcp_sender_->SetSendingStatus(feedback_state(), false));
  EXPECT_EQ(1, parser()->bye()->num_packets());
  EXPECT_EQ(kSenderSsrc, parser()->bye()->sender_ssrc());
}

TEST_F(RtcpSenderTest, SendFir) {
  rtcp_sender_->SetRTCPStatus(RtcpMode::kReducedSize);
  EXPECT_EQ(0, rtcp_sender_->SendRTCP(feedback_state(), kRtcpFir));
  EXPECT_EQ(1, parser()->fir()->num_packets());
  EXPECT_EQ(kSenderSsrc, parser()->fir()->sender_ssrc());
  EXPECT_EQ(1U, parser()->fir()->requests().size());
  EXPECT_EQ(kRemoteSsrc, parser()->fir()->requests()[0].ssrc);
  uint8_t seq = parser()->fir()->requests()[0].seq_nr;
  EXPECT_EQ(0, rtcp_sender_->SendRTCP(feedback_state(), kRtcpFir));
  EXPECT_EQ(2, parser()->fir()->num_packets());
  EXPECT_EQ(seq + 1, parser()->fir()->requests()[0].seq_nr);
}

TEST_F(RtcpSenderTest, SendPli) {
  rtcp_sender_->SetRTCPStatus(RtcpMode::kReducedSize);
  EXPECT_EQ(0, rtcp_sender_->SendRTCP(feedback_state(), kRtcpPli));
  EXPECT_EQ(1, parser()->pli()->num_packets());
  EXPECT_EQ(kSenderSsrc, parser()->pli()->sender_ssrc());
  EXPECT_EQ(kRemoteSsrc, parser()->pli()->media_ssrc());
}

TEST_F(RtcpSenderTest, SendNack) {
  rtcp_sender_->SetRTCPStatus(RtcpMode::kReducedSize);
  const uint16_t kList[] = {0, 1, 16};
  EXPECT_EQ(0, rtcp_sender_->SendRTCP(feedback_state(), kRtcpNack,
                                      ABSL_ARRAYSIZE(kList), kList));
  EXPECT_EQ(1, parser()->nack()->num_packets());
  EXPECT_EQ(kSenderSsrc, parser()->nack()->sender_ssrc());
  EXPECT_EQ(kRemoteSsrc, parser()->nack()->media_ssrc());
  EXPECT_THAT(parser()->nack()->packet_ids(), ElementsAre(0, 1, 16));
}

TEST_F(RtcpSenderTest, SendLossNotificationBufferingNotAllowed) {
  rtcp_sender_->SetRTCPStatus(RtcpMode::kReducedSize);
  constexpr uint16_t kLastDecoded = 0x1234;
  constexpr uint16_t kLastReceived = 0x4321;
  constexpr bool kDecodabilityFlag = true;
  constexpr bool kBufferingAllowed = false;
  EXPECT_EQ(rtcp_sender_->SendLossNotification(feedback_state(), kLastDecoded,
                                               kLastReceived, kDecodabilityFlag,
                                               kBufferingAllowed),
            0);
  EXPECT_EQ(parser()->processed_rtcp_packets(), 1u);
  EXPECT_EQ(parser()->loss_notification()->num_packets(), 1);
  EXPECT_EQ(kSenderSsrc, parser()->loss_notification()->sender_ssrc());
  EXPECT_EQ(kRemoteSsrc, parser()->loss_notification()->media_ssrc());
}

TEST_F(RtcpSenderTest, SendLossNotificationBufferingAllowed) {
  rtcp_sender_->SetRTCPStatus(RtcpMode::kReducedSize);
  constexpr uint16_t kLastDecoded = 0x1234;
  constexpr uint16_t kLastReceived = 0x4321;
  constexpr bool kDecodabilityFlag = true;
  constexpr bool kBufferingAllowed = true;
  EXPECT_EQ(rtcp_sender_->SendLossNotification(feedback_state(), kLastDecoded,
                                               kLastReceived, kDecodabilityFlag,
                                               kBufferingAllowed),
            0);

  // No RTCP messages sent yet.
  ASSERT_EQ(parser()->processed_rtcp_packets(), 0u);

  // Sending another messages triggers sending the LNTF messages as well.
  const uint16_t kList[] = {0, 1, 16};
  EXPECT_EQ(rtcp_sender_->SendRTCP(feedback_state(), kRtcpNack,
                                   ABSL_ARRAYSIZE(kList), kList),
            0);

  // Exactly one packet was produced, and it contained both the buffered LNTF
  // as well as the message that had triggered the packet.
  EXPECT_EQ(parser()->processed_rtcp_packets(), 1u);
  EXPECT_EQ(parser()->loss_notification()->num_packets(), 1);
  EXPECT_EQ(parser()->loss_notification()->sender_ssrc(), kSenderSsrc);
  EXPECT_EQ(parser()->loss_notification()->media_ssrc(), kRemoteSsrc);
  EXPECT_EQ(parser()->nack()->num_packets(), 1);
  EXPECT_EQ(parser()->nack()->sender_ssrc(), kSenderSsrc);
  EXPECT_EQ(parser()->nack()->media_ssrc(), kRemoteSsrc);
}

TEST_F(RtcpSenderTest, RembNotIncludedBeforeSet) {
  rtcp_sender_->SetRTCPStatus(RtcpMode::kReducedSize);

  rtcp_sender_->SendRTCP(feedback_state(), kRtcpRr);

  ASSERT_EQ(1, parser()->receiver_report()->num_packets());
  EXPECT_EQ(0, parser()->remb()->num_packets());
}

TEST_F(RtcpSenderTest, RembNotIncludedAfterUnset) {
  const int64_t kBitrate = 261011;
  const std::vector<uint32_t> kSsrcs = {kRemoteSsrc, kRemoteSsrc + 1};
  rtcp_sender_->SetRTCPStatus(RtcpMode::kReducedSize);
  rtcp_sender_->SetRemb(kBitrate, kSsrcs);
  rtcp_sender_->SendRTCP(feedback_state(), kRtcpRr);
  ASSERT_EQ(1, parser()->receiver_report()->num_packets());
  EXPECT_EQ(1, parser()->remb()->num_packets());

  // Turn off REMB. rtcp_sender no longer should send it.
  rtcp_sender_->UnsetRemb();
  rtcp_sender_->SendRTCP(feedback_state(), kRtcpRr);
  ASSERT_EQ(2, parser()->receiver_report()->num_packets());
  EXPECT_EQ(1, parser()->remb()->num_packets());
}

TEST_F(RtcpSenderTest, SendRemb) {
  const int64_t kBitrate = 261011;
  const std::vector<uint32_t> kSsrcs = {kRemoteSsrc, kRemoteSsrc + 1};
  rtcp_sender_->SetRTCPStatus(RtcpMode::kReducedSize);
  rtcp_sender_->SetRemb(kBitrate, kSsrcs);

  rtcp_sender_->SendRTCP(feedback_state(), kRtcpRemb);

  EXPECT_EQ(1, parser()->remb()->num_packets());
  EXPECT_EQ(kSenderSsrc, parser()->remb()->sender_ssrc());
  EXPECT_EQ(kBitrate, parser()->remb()->bitrate_bps());
  EXPECT_THAT(parser()->remb()->ssrcs(),
              ElementsAre(kRemoteSsrc, kRemoteSsrc + 1));
}

TEST_F(RtcpSenderTest, RembIncludedInEachCompoundPacketAfterSet) {
  const int kBitrate = 261011;
  const std::vector<uint32_t> kSsrcs = {kRemoteSsrc, kRemoteSsrc + 1};
  rtcp_sender_->SetRTCPStatus(RtcpMode::kCompound);
  rtcp_sender_->SetRemb(kBitrate, kSsrcs);

  rtcp_sender_->SendRTCP(feedback_state(), kRtcpReport);
  EXPECT_EQ(1, parser()->remb()->num_packets());
  // REMB should be included in each compound packet.
  rtcp_sender_->SendRTCP(feedback_state(), kRtcpReport);
  EXPECT_EQ(2, parser()->remb()->num_packets());
}

TEST_F(RtcpSenderTest, SendXrWithDlrr) {
  rtcp_sender_->SetRTCPStatus(RtcpMode::kCompound);
  RTCPSender::FeedbackState feedback_state = rtp_rtcp_impl_->GetFeedbackState();
  rtcp::ReceiveTimeInfo last_xr_rr;
  last_xr_rr.ssrc = 0x11111111;
  last_xr_rr.last_rr = 0x22222222;
  last_xr_rr.delay_since_last_rr = 0x33333333;
  feedback_state.last_xr_rtis.push_back(last_xr_rr);
  EXPECT_EQ(0, rtcp_sender_->SendRTCP(feedback_state, kRtcpReport));
  EXPECT_EQ(1, parser()->xr()->num_packets());
  EXPECT_EQ(kSenderSsrc, parser()->xr()->sender_ssrc());
  ASSERT_THAT(parser()->xr()->dlrr().sub_blocks(), SizeIs(1));
  EXPECT_EQ(last_xr_rr.ssrc, parser()->xr()->dlrr().sub_blocks()[0].ssrc);
  EXPECT_EQ(last_xr_rr.last_rr, parser()->xr()->dlrr().sub_blocks()[0].last_rr);
  EXPECT_EQ(last_xr_rr.delay_since_last_rr,
            parser()->xr()->dlrr().sub_blocks()[0].delay_since_last_rr);
}

TEST_F(RtcpSenderTest, SendXrWithMultipleDlrrSubBlocks) {
  const size_t kNumReceivers = 2;
  rtcp_sender_->SetRTCPStatus(RtcpMode::kCompound);
  RTCPSender::FeedbackState feedback_state = rtp_rtcp_impl_->GetFeedbackState();
  for (size_t i = 0; i < kNumReceivers; ++i) {
    rtcp::ReceiveTimeInfo last_xr_rr;
    last_xr_rr.ssrc = i;
    last_xr_rr.last_rr = (i + 1) * 100;
    last_xr_rr.delay_since_last_rr = (i + 2) * 200;
    feedback_state.last_xr_rtis.push_back(last_xr_rr);
  }

  EXPECT_EQ(0, rtcp_sender_->SendRTCP(feedback_state, kRtcpReport));
  EXPECT_EQ(1, parser()->xr()->num_packets());
  EXPECT_EQ(kSenderSsrc, parser()->xr()->sender_ssrc());
  ASSERT_THAT(parser()->xr()->dlrr().sub_blocks(), SizeIs(kNumReceivers));
  for (size_t i = 0; i < kNumReceivers; ++i) {
    EXPECT_EQ(feedback_state.last_xr_rtis[i].ssrc,
              parser()->xr()->dlrr().sub_blocks()[i].ssrc);
    EXPECT_EQ(feedback_state.last_xr_rtis[i].last_rr,
              parser()->xr()->dlrr().sub_blocks()[i].last_rr);
    EXPECT_EQ(feedback_state.last_xr_rtis[i].delay_since_last_rr,
              parser()->xr()->dlrr().sub_blocks()[i].delay_since_last_rr);
  }
}

TEST_F(RtcpSenderTest, SendXrWithRrtr) {
  rtcp_sender_->SetRTCPStatus(RtcpMode::kCompound);
  EXPECT_EQ(0, rtcp_sender_->SetSendingStatus(feedback_state(), false));
  rtcp_sender_->SendRtcpXrReceiverReferenceTime(true);
  NtpTime ntp = clock_.CurrentNtpTime();
  EXPECT_EQ(0, rtcp_sender_->SendRTCP(feedback_state(), kRtcpReport));
  EXPECT_EQ(1, parser()->xr()->num_packets());
  EXPECT_EQ(kSenderSsrc, parser()->xr()->sender_ssrc());
  EXPECT_FALSE(parser()->xr()->dlrr());
  ASSERT_TRUE(parser()->xr()->rrtr());
  EXPECT_EQ(ntp, parser()->xr()->rrtr()->ntp());
}

TEST_F(RtcpSenderTest, TestNoXrRrtrSentIfSending) {
  rtcp_sender_->SetRTCPStatus(RtcpMode::kCompound);
  EXPECT_EQ(0, rtcp_sender_->SetSendingStatus(feedback_state(), true));
  rtcp_sender_->SendRtcpXrReceiverReferenceTime(true);
  EXPECT_EQ(0, rtcp_sender_->SendRTCP(feedback_state(), kRtcpReport));
  EXPECT_EQ(0, parser()->xr()->num_packets());
}

TEST_F(RtcpSenderTest, TestNoXrRrtrSentIfNotEnabled) {
  rtcp_sender_->SetRTCPStatus(RtcpMode::kCompound);
  EXPECT_EQ(0, rtcp_sender_->SetSendingStatus(feedback_state(), false));
  rtcp_sender_->SendRtcpXrReceiverReferenceTime(false);
  EXPECT_EQ(0, rtcp_sender_->SendRTCP(feedback_state(), kRtcpReport));
  EXPECT_EQ(0, parser()->xr()->num_packets());
}

TEST_F(RtcpSenderTest, TestRegisterRtcpPacketTypeObserver) {
  RtcpPacketTypeCounterObserverImpl observer;
  RtpRtcpInterface::Configuration config;
  config.clock = &clock_;
  config.receive_statistics = receive_statistics_.get();
  config.outgoing_transport = &test_transport_;
  config.rtcp_packet_type_counter_observer = &observer;
  config.rtcp_report_interval_ms = 1000;
  rtcp_sender_.reset(new RTCPSender(config));

  rtcp_sender_->SetRemoteSSRC(kRemoteSsrc);
  rtcp_sender_->SetRTCPStatus(RtcpMode::kReducedSize);
  EXPECT_EQ(0, rtcp_sender_->SendRTCP(feedback_state(), kRtcpPli));
  EXPECT_EQ(1, parser()->pli()->num_packets());
  EXPECT_EQ(kRemoteSsrc, observer.ssrc_);
  EXPECT_EQ(1U, observer.counter_.pli_packets);
  EXPECT_EQ(clock_.TimeInMilliseconds(),
            observer.counter_.first_packet_time_ms);
}

TEST_F(RtcpSenderTest, SendTmmbr) {
  const unsigned int kBitrateBps = 312000;
  rtcp_sender_->SetRTCPStatus(RtcpMode::kReducedSize);
  rtcp_sender_->SetTargetBitrate(kBitrateBps);
  EXPECT_EQ(0, rtcp_sender_->SendRTCP(feedback_state(), kRtcpTmmbr));
  EXPECT_EQ(1, parser()->tmmbr()->num_packets());
  EXPECT_EQ(kSenderSsrc, parser()->tmmbr()->sender_ssrc());
  EXPECT_EQ(1U, parser()->tmmbr()->requests().size());
  EXPECT_EQ(kBitrateBps, parser()->tmmbr()->requests()[0].bitrate_bps());
  // TODO(asapersson): tmmbr_item()->Overhead() looks broken, always zero.
}

TEST_F(RtcpSenderTest, TmmbrIncludedInCompoundPacketIfEnabled) {
  const unsigned int kBitrateBps = 312000;
  rtcp_sender_->SetRTCPStatus(RtcpMode::kCompound);
  EXPECT_FALSE(rtcp_sender_->TMMBR());
  rtcp_sender_->SetTMMBRStatus(true);
  EXPECT_TRUE(rtcp_sender_->TMMBR());
  rtcp_sender_->SetTargetBitrate(kBitrateBps);
  EXPECT_EQ(0, rtcp_sender_->SendRTCP(feedback_state(), kRtcpReport));
  EXPECT_EQ(1, parser()->tmmbr()->num_packets());
  EXPECT_EQ(1U, parser()->tmmbr()->requests().size());
  // TMMBR should be included in each compound packet.
  EXPECT_EQ(0, rtcp_sender_->SendRTCP(feedback_state(), kRtcpReport));
  EXPECT_EQ(2, parser()->tmmbr()->num_packets());

  rtcp_sender_->SetTMMBRStatus(false);
  EXPECT_FALSE(rtcp_sender_->TMMBR());
}

TEST_F(RtcpSenderTest, SendTmmbn) {
  rtcp_sender_->SetRTCPStatus(RtcpMode::kCompound);
  rtcp_sender_->SetSendingStatus(feedback_state(), true);
  std::vector<rtcp::TmmbItem> bounding_set;
  const uint32_t kBitrateBps = 32768000;
  const uint32_t kPacketOh = 40;
  const uint32_t kSourceSsrc = 12345;
  const rtcp::TmmbItem tmmbn(kSourceSsrc, kBitrateBps, kPacketOh);
  bounding_set.push_back(tmmbn);
  rtcp_sender_->SetTmmbn(bounding_set);

  EXPECT_EQ(0, rtcp_sender_->SendRTCP(feedback_state(), kRtcpSr));
  EXPECT_EQ(1, parser()->sender_report()->num_packets());
  EXPECT_EQ(1, parser()->tmmbn()->num_packets());
  EXPECT_EQ(kSenderSsrc, parser()->tmmbn()->sender_ssrc());
  EXPECT_EQ(1U, parser()->tmmbn()->items().size());
  EXPECT_EQ(kBitrateBps, parser()->tmmbn()->items()[0].bitrate_bps());
  EXPECT_EQ(kPacketOh, parser()->tmmbn()->items()[0].packet_overhead());
  EXPECT_EQ(kSourceSsrc, parser()->tmmbn()->items()[0].ssrc());
}

// This test is written to verify actual behaviour. It does not seem
// to make much sense to send an empty TMMBN, since there is no place
// to put an actual limit here. It's just information that no limit
// is set, which is kind of the starting assumption.
// See http://code.google.com/p/webrtc/issues/detail?id=468 for one
// situation where this caused confusion.
TEST_F(RtcpSenderTest, SendsTmmbnIfSetAndEmpty) {
  rtcp_sender_->SetRTCPStatus(RtcpMode::kCompound);
  rtcp_sender_->SetSendingStatus(feedback_state(), true);
  std::vector<rtcp::TmmbItem> bounding_set;
  rtcp_sender_->SetTmmbn(bounding_set);
  EXPECT_EQ(0, rtcp_sender_->SendRTCP(feedback_state(), kRtcpSr));
  EXPECT_EQ(1, parser()->sender_report()->num_packets());
  EXPECT_EQ(1, parser()->tmmbn()->num_packets());
  EXPECT_EQ(kSenderSsrc, parser()->tmmbn()->sender_ssrc());
  EXPECT_EQ(0U, parser()->tmmbn()->items().size());
}

TEST_F(RtcpSenderTest, SendCompoundPliRemb) {
  const int kBitrate = 261011;
  std::vector<uint32_t> ssrcs;
  ssrcs.push_back(kRemoteSsrc);
  rtcp_sender_->SetRTCPStatus(RtcpMode::kCompound);
  rtcp_sender_->SetRemb(kBitrate, ssrcs);
  std::set<RTCPPacketType> packet_types;
  packet_types.insert(kRtcpRemb);
  packet_types.insert(kRtcpPli);
  EXPECT_EQ(0, rtcp_sender_->SendCompoundRTCP(feedback_state(), packet_types));
  EXPECT_EQ(1, parser()->remb()->num_packets());
  EXPECT_EQ(1, parser()->pli()->num_packets());
}

// This test is written to verify that BYE is always the last packet
// type in a RTCP compoud packet.  The rtcp_sender_ is recreated with
// mock_transport, which is used to check for whether BYE at the end
// of a RTCP compound packet.
TEST_F(RtcpSenderTest, ByeMustBeLast) {
  MockTransport mock_transport;
  EXPECT_CALL(mock_transport, SendRtcp(_, _))
      .WillOnce(Invoke([](const uint8_t* data, size_t len) {
        const uint8_t* next_packet = data;
        const uint8_t* const packet_end = data + len;
        rtcp::CommonHeader packet;
        while (next_packet < packet_end) {
          EXPECT_TRUE(packet.Parse(next_packet, packet_end - next_packet));
          next_packet = packet.NextPacket();
          if (packet.type() ==
              rtcp::Bye::kPacketType)  // Main test expectation.
            EXPECT_EQ(0, packet_end - next_packet)
                << "Bye packet should be last in a compound RTCP packet.";
          if (next_packet == packet_end)  // Validate test was set correctly.
            EXPECT_EQ(packet.type(), rtcp::Bye::kPacketType)
                << "Last packet in this test expected to be Bye.";
        }

        return true;
      }));

  // Re-configure rtcp_sender_ with mock_transport_
  RtpRtcpInterface::Configuration config;
  config.clock = &clock_;
  config.receive_statistics = receive_statistics_.get();
  config.outgoing_transport = &mock_transport;
  config.rtcp_report_interval_ms = 1000;
  config.local_media_ssrc = kSenderSsrc;
  rtcp_sender_.reset(new RTCPSender(config));

  rtcp_sender_->SetRemoteSSRC(kRemoteSsrc);
  rtcp_sender_->SetTimestampOffset(kStartRtpTimestamp);
  rtcp_sender_->SetLastRtpTime(kRtpTimestamp, clock_.TimeInMilliseconds(),
                               /*payload_type=*/0);

  // Set up REMB info to be included with BYE.
  rtcp_sender_->SetRTCPStatus(RtcpMode::kCompound);
  rtcp_sender_->SetRemb(1234, {});
  EXPECT_EQ(0, rtcp_sender_->SendRTCP(feedback_state(), kRtcpBye));
}

TEST_F(RtcpSenderTest, SendXrWithTargetBitrate) {
  rtcp_sender_->SetRTCPStatus(RtcpMode::kCompound);
  const size_t kNumSpatialLayers = 2;
  const size_t kNumTemporalLayers = 2;
  VideoBitrateAllocation allocation;
  for (size_t sl = 0; sl < kNumSpatialLayers; ++sl) {
    uint32_t start_bitrate_bps = (sl + 1) * 100000;
    for (size_t tl = 0; tl < kNumTemporalLayers; ++tl)
      allocation.SetBitrate(sl, tl, start_bitrate_bps + (tl * 20000));
  }
  rtcp_sender_->SetVideoBitrateAllocation(allocation);

  EXPECT_EQ(0, rtcp_sender_->SendRTCP(feedback_state(), kRtcpReport));
  EXPECT_EQ(1, parser()->xr()->num_packets());
  EXPECT_EQ(kSenderSsrc, parser()->xr()->sender_ssrc());
  const absl::optional<rtcp::TargetBitrate>& target_bitrate =
      parser()->xr()->target_bitrate();
  ASSERT_TRUE(target_bitrate);
  const std::vector<rtcp::TargetBitrate::BitrateItem>& bitrates =
      target_bitrate->GetTargetBitrates();
  EXPECT_EQ(kNumSpatialLayers * kNumTemporalLayers, bitrates.size());

  for (size_t sl = 0; sl < kNumSpatialLayers; ++sl) {
    uint32_t start_bitrate_bps = (sl + 1) * 100000;
    for (size_t tl = 0; tl < kNumTemporalLayers; ++tl) {
      size_t index = (sl * kNumSpatialLayers) + tl;
      const rtcp::TargetBitrate::BitrateItem& item = bitrates[index];
      EXPECT_EQ(sl, item.spatial_layer);
      EXPECT_EQ(tl, item.temporal_layer);
      EXPECT_EQ(start_bitrate_bps + (tl * 20000),
                item.target_bitrate_kbps * 1000);
    }
  }
}

TEST_F(RtcpSenderTest, SendImmediateXrWithTargetBitrate) {
  // Initialize. Send a first report right away.
  rtcp_sender_->SetRTCPStatus(RtcpMode::kCompound);
  EXPECT_EQ(0, rtcp_sender_->SendRTCP(feedback_state(), kRtcpReport));
  clock_.AdvanceTimeMilliseconds(5);

  // Video bitrate allocation generated, save until next time we send a report.
  VideoBitrateAllocation allocation;
  allocation.SetBitrate(0, 0, 100000);
  rtcp_sender_->SetVideoBitrateAllocation(allocation);
  // First seen instance will be sent immediately.
  EXPECT_TRUE(rtcp_sender_->TimeToSendRTCPReport(false));
  EXPECT_EQ(0, rtcp_sender_->SendRTCP(feedback_state(), kRtcpReport));
  clock_.AdvanceTimeMilliseconds(5);

  // Update bitrate of existing layer, does not quality for immediate sending.
  allocation.SetBitrate(0, 0, 150000);
  rtcp_sender_->SetVideoBitrateAllocation(allocation);
  EXPECT_FALSE(rtcp_sender_->TimeToSendRTCPReport(false));

  // A new spatial layer enabled, signal this as soon as possible.
  allocation.SetBitrate(1, 0, 200000);
  rtcp_sender_->SetVideoBitrateAllocation(allocation);
  EXPECT_TRUE(rtcp_sender_->TimeToSendRTCPReport(false));
  EXPECT_EQ(0, rtcp_sender_->SendRTCP(feedback_state(), kRtcpReport));
  clock_.AdvanceTimeMilliseconds(5);

  // Explicitly disable top layer. The same set of layers now has a bitrate
  // defined, but the explicit 0 indicates shutdown. Signal immediately.
  allocation.SetBitrate(1, 0, 0);
  EXPECT_FALSE(rtcp_sender_->TimeToSendRTCPReport(false));
  rtcp_sender_->SetVideoBitrateAllocation(allocation);
  EXPECT_TRUE(rtcp_sender_->TimeToSendRTCPReport(false));
}

TEST_F(RtcpSenderTest, SendTargetBitrateExplicitZeroOnStreamRemoval) {
  // Set up and send a bitrate allocation with two layers.

  rtcp_sender_->SetRTCPStatus(RtcpMode::kCompound);
  VideoBitrateAllocation allocation;
  allocation.SetBitrate(0, 0, 100000);
  allocation.SetBitrate(1, 0, 200000);
  rtcp_sender_->SetVideoBitrateAllocation(allocation);
  EXPECT_EQ(0, rtcp_sender_->SendRTCP(feedback_state(), kRtcpReport));
  absl::optional<rtcp::TargetBitrate> target_bitrate =
      parser()->xr()->target_bitrate();
  ASSERT_TRUE(target_bitrate);
  std::vector<rtcp::TargetBitrate::BitrateItem> bitrates =
      target_bitrate->GetTargetBitrates();
  ASSERT_EQ(2u, bitrates.size());
  EXPECT_EQ(bitrates[0].target_bitrate_kbps,
            allocation.GetBitrate(0, 0) / 1000);
  EXPECT_EQ(bitrates[1].target_bitrate_kbps,
            allocation.GetBitrate(1, 0) / 1000);

  // Create a new allocation, where the second stream is no longer available.
  VideoBitrateAllocation new_allocation;
  new_allocation.SetBitrate(0, 0, 150000);
  rtcp_sender_->SetVideoBitrateAllocation(new_allocation);
  EXPECT_EQ(0, rtcp_sender_->SendRTCP(feedback_state(), kRtcpReport));
  target_bitrate = parser()->xr()->target_bitrate();
  ASSERT_TRUE(target_bitrate);
  bitrates = target_bitrate->GetTargetBitrates();

  // Two bitrates should still be set, with an explicit entry indicating the
  // removed stream is gone.
  ASSERT_EQ(2u, bitrates.size());
  EXPECT_EQ(bitrates[0].target_bitrate_kbps,
            new_allocation.GetBitrate(0, 0) / 1000);
  EXPECT_EQ(bitrates[1].target_bitrate_kbps, 0u);
}

TEST_F(RtcpSenderTest, DoesntSchedulesInitialReportWhenSsrcSetOnConstruction) {
  rtcp_sender_->SetRTCPStatus(RtcpMode::kReducedSize);
  rtcp_sender_->SetRemoteSSRC(kRemoteSsrc);
  // New report should not have been scheduled yet.
  clock_.AdvanceTimeMilliseconds(100);
  EXPECT_FALSE(rtcp_sender_->TimeToSendRTCPReport(false));
}

TEST_F(RtcpSenderTest, SendsCombinedRtcpPacket) {
  rtcp_sender_->SetRTCPStatus(RtcpMode::kReducedSize);

  std::vector<std::unique_ptr<rtcp::RtcpPacket>> packets;
  auto transport_feedback = std::make_unique<rtcp::TransportFeedback>();
  transport_feedback->AddReceivedPacket(321, 10000);
  packets.push_back(std::move(transport_feedback));
  auto remote_estimate = std::make_unique<rtcp::RemoteEstimate>();
  packets.push_back(std::move(remote_estimate));
  rtcp_sender_->SendCombinedRtcpPacket(std::move(packets));

  EXPECT_EQ(parser()->transport_feedback()->num_packets(), 1);
  EXPECT_EQ(parser()->transport_feedback()->sender_ssrc(), kSenderSsrc);
  EXPECT_EQ(parser()->app()->num_packets(), 1);
  EXPECT_EQ(parser()->app()->sender_ssrc(), kSenderSsrc);
}

}  // namespace webrtc
