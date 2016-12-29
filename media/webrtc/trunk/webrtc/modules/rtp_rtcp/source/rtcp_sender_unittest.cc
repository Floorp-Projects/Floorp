/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


/*
 * This file includes unit tests for the RTCPSender.
 */

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#include "webrtc/common_types.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_sender.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_rtcp_impl.h"
#include "webrtc/test/rtcp_packet_parser.h"

using ::testing::ElementsAre;

namespace webrtc {

TEST(NACKStringBuilderTest, TestCase1) {
  NACKStringBuilder builder;
  builder.PushNACK(5);
  builder.PushNACK(7);
  builder.PushNACK(9);
  builder.PushNACK(10);
  builder.PushNACK(11);
  builder.PushNACK(12);
  builder.PushNACK(15);
  builder.PushNACK(18);
  builder.PushNACK(19);
  EXPECT_EQ(std::string("5,7,9-12,15,18-19"), builder.GetResult());
}

TEST(NACKStringBuilderTest, TestCase2) {
  NACKStringBuilder builder;
  builder.PushNACK(5);
  builder.PushNACK(6);
  builder.PushNACK(7);
  builder.PushNACK(9);
  builder.PushNACK(10);
  builder.PushNACK(11);
  builder.PushNACK(12);
  builder.PushNACK(15);
  builder.PushNACK(18);
  builder.PushNACK(19);
  EXPECT_EQ(std::string("5-7,9-12,15,18-19"), builder.GetResult());
}

TEST(NACKStringBuilderTest, TestCase3) {
  NACKStringBuilder builder;
  builder.PushNACK(5);
  builder.PushNACK(7);
  builder.PushNACK(9);
  builder.PushNACK(10);
  builder.PushNACK(11);
  builder.PushNACK(12);
  builder.PushNACK(15);
  builder.PushNACK(18);
  builder.PushNACK(19);
  builder.PushNACK(21);
  EXPECT_EQ(std::string("5,7,9-12,15,18-19,21"), builder.GetResult());
}

TEST(NACKStringBuilderTest, TestCase4) {
  NACKStringBuilder builder;
  builder.PushNACK(5);
  builder.PushNACK(7);
  builder.PushNACK(8);
  builder.PushNACK(9);
  builder.PushNACK(10);
  builder.PushNACK(11);
  builder.PushNACK(12);
  builder.PushNACK(15);
  builder.PushNACK(18);
  builder.PushNACK(19);
  EXPECT_EQ(std::string("5,7-12,15,18-19"), builder.GetResult());
}

TEST(NACKStringBuilderTest, TestCase5) {
  NACKStringBuilder builder;
  builder.PushNACK(5);
  builder.PushNACK(7);
  builder.PushNACK(9);
  builder.PushNACK(10);
  builder.PushNACK(11);
  builder.PushNACK(12);
  builder.PushNACK(15);
  builder.PushNACK(16);
  builder.PushNACK(18);
  builder.PushNACK(19);
  EXPECT_EQ(std::string("5,7,9-12,15-16,18-19"), builder.GetResult());
}

TEST(NACKStringBuilderTest, TestCase6) {
  NACKStringBuilder builder;
  builder.PushNACK(5);
  builder.PushNACK(7);
  builder.PushNACK(9);
  builder.PushNACK(10);
  builder.PushNACK(11);
  builder.PushNACK(12);
  builder.PushNACK(15);
  builder.PushNACK(16);
  builder.PushNACK(17);
  builder.PushNACK(18);
  builder.PushNACK(19);
  EXPECT_EQ(std::string("5,7,9-12,15-19"), builder.GetResult());
}

TEST(NACKStringBuilderTest, TestCase7) {
  NACKStringBuilder builder;
  builder.PushNACK(5);
  builder.PushNACK(6);
  builder.PushNACK(7);
  builder.PushNACK(8);
  builder.PushNACK(11);
  builder.PushNACK(12);
  builder.PushNACK(13);
  builder.PushNACK(14);
  builder.PushNACK(15);
  EXPECT_EQ(std::string("5-8,11-15"), builder.GetResult());
}

TEST(NACKStringBuilderTest, TestCase8) {
  NACKStringBuilder builder;
  builder.PushNACK(5);
  builder.PushNACK(7);
  builder.PushNACK(9);
  builder.PushNACK(11);
  builder.PushNACK(15);
  builder.PushNACK(17);
  builder.PushNACK(19);
  EXPECT_EQ(std::string("5,7,9,11,15,17,19"), builder.GetResult());
}

TEST(NACKStringBuilderTest, TestCase9) {
  NACKStringBuilder builder;
  builder.PushNACK(5);
  builder.PushNACK(6);
  builder.PushNACK(7);
  builder.PushNACK(8);
  builder.PushNACK(9);
  builder.PushNACK(10);
  builder.PushNACK(11);
  builder.PushNACK(12);
  EXPECT_EQ(std::string("5-12"), builder.GetResult());
}

TEST(NACKStringBuilderTest, TestCase10) {
  NACKStringBuilder builder;
  builder.PushNACK(5);
  EXPECT_EQ(std::string("5"), builder.GetResult());
}

TEST(NACKStringBuilderTest, TestCase11) {
  NACKStringBuilder builder;
  EXPECT_EQ(std::string(""), builder.GetResult());
}

TEST(NACKStringBuilderTest, TestCase12) {
  NACKStringBuilder builder;
  builder.PushNACK(5);
  builder.PushNACK(6);
  EXPECT_EQ(std::string("5-6"), builder.GetResult());
}

TEST(NACKStringBuilderTest, TestCase13) {
  NACKStringBuilder builder;
  builder.PushNACK(5);
  builder.PushNACK(6);
  builder.PushNACK(9);
  EXPECT_EQ(std::string("5-6,9"), builder.GetResult());
}

class RtcpPacketTypeCounterObserverImpl : public RtcpPacketTypeCounterObserver {
 public:
  RtcpPacketTypeCounterObserverImpl() : ssrc_(0) {}
  virtual ~RtcpPacketTypeCounterObserverImpl() {}
  void RtcpPacketTypesCounterUpdated(
      uint32_t ssrc,
      const RtcpPacketTypeCounter& packet_counter) override {
    ssrc_ = ssrc;
    counter_ = packet_counter;
  }
  uint32_t ssrc_;
  RtcpPacketTypeCounter counter_;
};

class TestTransport : public Transport,
                      public NullRtpData {
 public:
  TestTransport() {}

  bool SendRtp(const uint8_t* /*data*/,
               size_t /*len*/,
               const PacketOptions& options) override {
    return false;
  }
  bool SendRtcp(const uint8_t* data, size_t len) override {
    parser_.Parse(static_cast<const uint8_t*>(data), len);
    return true;
  }
  int OnReceivedPayloadData(const uint8_t* payload_data,
                            const size_t payload_size,
                            const WebRtcRTPHeader* rtp_header) override {
    return 0;
  }
  test::RtcpPacketParser parser_;
};

namespace {
static const uint32_t kSenderSsrc = 0x11111111;
static const uint32_t kRemoteSsrc = 0x22222222;
}

class RtcpSenderTest : public ::testing::Test {
 protected:
  RtcpSenderTest()
      : clock_(1335900000),
        receive_statistics_(ReceiveStatistics::Create(&clock_)) {
    RtpRtcp::Configuration configuration;
    configuration.audio = false;
    configuration.clock = &clock_;
    configuration.outgoing_transport = &test_transport_;

    rtp_rtcp_impl_.reset(new ModuleRtpRtcpImpl(configuration));
    rtcp_sender_.reset(new RTCPSender(false, &clock_, receive_statistics_.get(),
                                      nullptr, &test_transport_));
    rtcp_sender_->SetSSRC(kSenderSsrc);
    rtcp_sender_->SetRemoteSSRC(kRemoteSsrc);
  }

  void InsertIncomingPacket(uint32_t remote_ssrc, uint16_t seq_num) {
    RTPHeader header;
    header.ssrc = remote_ssrc;
    header.sequenceNumber = seq_num;
    header.timestamp = 12345;
    header.headerLength = 12;
    size_t kPacketLength = 100;
    receive_statistics_->IncomingPacket(header, kPacketLength, false);
  }

  test::RtcpPacketParser* parser() { return &test_transport_.parser_; }

  RTCPSender::FeedbackState feedback_state() {
    return rtp_rtcp_impl_->GetFeedbackState();
  }

  SimulatedClock clock_;
  TestTransport test_transport_;
  rtc::scoped_ptr<ReceiveStatistics> receive_statistics_;
  rtc::scoped_ptr<ModuleRtpRtcpImpl> rtp_rtcp_impl_;
  rtc::scoped_ptr<RTCPSender> rtcp_sender_;
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
  feedback_state.packets_sent = kPacketCount;
  feedback_state.media_bytes_sent = kOctetCount;
  uint32_t ntp_secs;
  uint32_t ntp_frac;
  clock_.CurrentNtp(ntp_secs, ntp_frac);
  EXPECT_EQ(0, rtcp_sender_->SendRTCP(feedback_state, kRtcpSr));
  EXPECT_EQ(1, parser()->sender_report()->num_packets());
  EXPECT_EQ(kSenderSsrc, parser()->sender_report()->Ssrc());
  EXPECT_EQ(ntp_secs, parser()->sender_report()->NtpSec());
  EXPECT_EQ(ntp_frac, parser()->sender_report()->NtpFrac());
  EXPECT_EQ(kPacketCount, parser()->sender_report()->PacketCount());
  EXPECT_EQ(kOctetCount, parser()->sender_report()->OctetCount());
  EXPECT_EQ(0, parser()->report_block()->num_packets());
}

TEST_F(RtcpSenderTest, SendRr) {
  rtcp_sender_->SetRTCPStatus(RtcpMode::kReducedSize);
  EXPECT_EQ(0, rtcp_sender_->SendRTCP(feedback_state(), kRtcpRr));
  EXPECT_EQ(1, parser()->receiver_report()->num_packets());
  EXPECT_EQ(kSenderSsrc, parser()->receiver_report()->Ssrc());
  EXPECT_EQ(0, parser()->report_block()->num_packets());
}

TEST_F(RtcpSenderTest, SendRrWithOneReportBlock) {
  const uint16_t kSeqNum = 11111;
  InsertIncomingPacket(kRemoteSsrc, kSeqNum);
  rtcp_sender_->SetRTCPStatus(RtcpMode::kCompound);
  EXPECT_EQ(0, rtcp_sender_->SendRTCP(feedback_state(), kRtcpRr));
  EXPECT_EQ(1, parser()->receiver_report()->num_packets());
  EXPECT_EQ(kSenderSsrc, parser()->receiver_report()->Ssrc());
  EXPECT_EQ(1, parser()->report_block()->num_packets());
  EXPECT_EQ(kRemoteSsrc, parser()->report_block()->Ssrc());
  EXPECT_EQ(0U, parser()->report_block()->FractionLost());
  EXPECT_EQ(0U, parser()->report_block()->CumPacketLost());
  EXPECT_EQ(kSeqNum, parser()->report_block()->ExtHighestSeqNum());
}

TEST_F(RtcpSenderTest, SendRrWithTwoReportBlocks) {
  const uint16_t kSeqNum = 11111;
  InsertIncomingPacket(kRemoteSsrc, kSeqNum);
  InsertIncomingPacket(kRemoteSsrc + 1, kSeqNum + 1);
  rtcp_sender_->SetRTCPStatus(RtcpMode::kCompound);
  EXPECT_EQ(0, rtcp_sender_->SendRTCP(feedback_state(), kRtcpRr));
  EXPECT_EQ(1, parser()->receiver_report()->num_packets());
  EXPECT_EQ(kSenderSsrc, parser()->receiver_report()->Ssrc());
  EXPECT_EQ(2, parser()->report_block()->num_packets());
  EXPECT_EQ(1, parser()->report_blocks_per_ssrc(kRemoteSsrc));
  EXPECT_EQ(1, parser()->report_blocks_per_ssrc(kRemoteSsrc + 1));
}

TEST_F(RtcpSenderTest, SendSdes) {
  rtcp_sender_->SetRTCPStatus(RtcpMode::kReducedSize);
  EXPECT_EQ(0, rtcp_sender_->SetCNAME("alice@host"));
  EXPECT_EQ(0, rtcp_sender_->SendRTCP(feedback_state(), kRtcpSdes));
  EXPECT_EQ(1, parser()->sdes()->num_packets());
  EXPECT_EQ(1, parser()->sdes_chunk()->num_packets());
  EXPECT_EQ(kSenderSsrc, parser()->sdes_chunk()->Ssrc());
  EXPECT_EQ("alice@host", parser()->sdes_chunk()->Cname());
}

TEST_F(RtcpSenderTest, SdesIncludedInCompoundPacket) {
  rtcp_sender_->SetRTCPStatus(RtcpMode::kCompound);
  EXPECT_EQ(0, rtcp_sender_->SetCNAME("alice@host"));
  EXPECT_EQ(0, rtcp_sender_->SendRTCP(feedback_state(), kRtcpReport));
  EXPECT_EQ(1, parser()->receiver_report()->num_packets());
  EXPECT_EQ(1, parser()->sdes()->num_packets());
  EXPECT_EQ(1, parser()->sdes_chunk()->num_packets());
}

TEST_F(RtcpSenderTest, SendBye) {
  rtcp_sender_->SetRTCPStatus(RtcpMode::kReducedSize);
  EXPECT_EQ(0, rtcp_sender_->SendRTCP(feedback_state(), kRtcpBye));
  EXPECT_EQ(1, parser()->bye()->num_packets());
  EXPECT_EQ(kSenderSsrc, parser()->bye()->Ssrc());
}

TEST_F(RtcpSenderTest, StopSendingTriggersBye) {
  rtcp_sender_->SetRTCPStatus(RtcpMode::kReducedSize);
  EXPECT_EQ(0, rtcp_sender_->SetSendingStatus(feedback_state(), true));
  EXPECT_EQ(0, rtcp_sender_->SetSendingStatus(feedback_state(), false));
  EXPECT_EQ(1, parser()->bye()->num_packets());
  EXPECT_EQ(kSenderSsrc, parser()->bye()->Ssrc());
}

TEST_F(RtcpSenderTest, SendApp) {
  const uint8_t kSubType = 30;
  uint32_t name = 'n' << 24;
  name += 'a' << 16;
  name += 'm' << 8;
  name += 'e';
  const uint8_t kData[] = {'t', 'e', 's', 't', 'd', 'a', 't', 'a'};
  const uint16_t kDataLength = sizeof(kData) / sizeof(kData[0]);
  EXPECT_EQ(0, rtcp_sender_->SetApplicationSpecificData(kSubType, name, kData,
                                                        kDataLength));
  rtcp_sender_->SetRTCPStatus(RtcpMode::kReducedSize);
  EXPECT_EQ(0, rtcp_sender_->SendRTCP(feedback_state(), kRtcpApp));
  EXPECT_EQ(1, parser()->app()->num_packets());
  EXPECT_EQ(kSubType, parser()->app()->SubType());
  EXPECT_EQ(name, parser()->app()->Name());
  EXPECT_EQ(1, parser()->app_item()->num_packets());
  EXPECT_EQ(kDataLength, parser()->app_item()->DataLength());
  EXPECT_EQ(0, strncmp(reinterpret_cast<const char*>(kData),
      reinterpret_cast<const char*>(parser()->app_item()->Data()),
      parser()->app_item()->DataLength()));
}

TEST_F(RtcpSenderTest, SendEmptyApp) {
  const uint8_t kSubType = 30;
  const uint32_t kName = 0x6E616D65;

  EXPECT_EQ(
      0, rtcp_sender_->SetApplicationSpecificData(kSubType, kName, nullptr, 0));

  rtcp_sender_->SetRTCPStatus(RtcpMode::kReducedSize);
  EXPECT_EQ(0, rtcp_sender_->SendRTCP(feedback_state(), kRtcpApp));
  EXPECT_EQ(1, parser()->app()->num_packets());
  EXPECT_EQ(kSubType, parser()->app()->SubType());
  EXPECT_EQ(kName, parser()->app()->Name());
  EXPECT_EQ(0, parser()->app_item()->num_packets());
}

TEST_F(RtcpSenderTest, SetInvalidApplicationSpecificData) {
  const uint8_t kData[] = {'t', 'e', 's', 't', 'd', 'a', 't'};
  const uint16_t kInvalidDataLength = sizeof(kData) / sizeof(kData[0]);
  EXPECT_EQ(-1, rtcp_sender_->SetApplicationSpecificData(
      0, 0, kData, kInvalidDataLength));  // Should by multiple of 4.
}

TEST_F(RtcpSenderTest, SendFirNonRepeat) {
  rtcp_sender_->SetRTCPStatus(RtcpMode::kReducedSize);
  EXPECT_EQ(0, rtcp_sender_->SendRTCP(feedback_state(), kRtcpFir));
  EXPECT_EQ(1, parser()->fir()->num_packets());
  EXPECT_EQ(kSenderSsrc, parser()->fir()->Ssrc());
  EXPECT_EQ(1, parser()->fir_item()->num_packets());
  EXPECT_EQ(kRemoteSsrc, parser()->fir_item()->Ssrc());
  uint8_t seq = parser()->fir_item()->SeqNum();
  // Sends non-repeat FIR as default.
  EXPECT_EQ(0, rtcp_sender_->SendRTCP(feedback_state(), kRtcpFir));
  EXPECT_EQ(2, parser()->fir()->num_packets());
  EXPECT_EQ(2, parser()->fir_item()->num_packets());
  EXPECT_EQ(seq + 1, parser()->fir_item()->SeqNum());
}

TEST_F(RtcpSenderTest, SendFirRepeat) {
  rtcp_sender_->SetRTCPStatus(RtcpMode::kReducedSize);
  EXPECT_EQ(0, rtcp_sender_->SendRTCP(feedback_state(), kRtcpFir));
  EXPECT_EQ(1, parser()->fir()->num_packets());
  EXPECT_EQ(1, parser()->fir_item()->num_packets());
  uint8_t seq = parser()->fir_item()->SeqNum();
  const bool kRepeat = true;
  EXPECT_EQ(0, rtcp_sender_->SendRTCP(feedback_state(), kRtcpFir, 0, nullptr,
                                      kRepeat));
  EXPECT_EQ(2, parser()->fir()->num_packets());
  EXPECT_EQ(2, parser()->fir_item()->num_packets());
  EXPECT_EQ(seq, parser()->fir_item()->SeqNum());
}

TEST_F(RtcpSenderTest, SendPli) {
  rtcp_sender_->SetRTCPStatus(RtcpMode::kReducedSize);
  EXPECT_EQ(0, rtcp_sender_->SendRTCP(feedback_state(), kRtcpPli));
  EXPECT_EQ(1, parser()->pli()->num_packets());
  EXPECT_EQ(kSenderSsrc, parser()->pli()->Ssrc());
  EXPECT_EQ(kRemoteSsrc, parser()->pli()->MediaSsrc());
}

TEST_F(RtcpSenderTest, SendRpsi) {
  const uint64_t kPictureId = 0x41;
  const int8_t kPayloadType = 100;
  rtcp_sender_->SetRTCPStatus(RtcpMode::kReducedSize);
  RTCPSender::FeedbackState feedback_state = rtp_rtcp_impl_->GetFeedbackState();
  feedback_state.send_payload_type = kPayloadType;
  EXPECT_EQ(0, rtcp_sender_->SendRTCP(feedback_state, kRtcpRpsi, 0, nullptr,
                                      false, kPictureId));
  EXPECT_EQ(kPayloadType, parser()->rpsi()->PayloadType());
  EXPECT_EQ(kPictureId, parser()->rpsi()->PictureId());
}

TEST_F(RtcpSenderTest, SendSli) {
  const uint16_t kFirstMb = 0;
  const uint16_t kNumberOfMb = 0x1FFF;
  const uint8_t kPictureId = 60;
  rtcp_sender_->SetRTCPStatus(RtcpMode::kReducedSize);
  EXPECT_EQ(0, rtcp_sender_->SendRTCP(feedback_state(), kRtcpSli, 0, nullptr,
                                      false, kPictureId));
  EXPECT_EQ(1, parser()->sli()->num_packets());
  EXPECT_EQ(kSenderSsrc, parser()->sli()->Ssrc());
  EXPECT_EQ(kRemoteSsrc, parser()->sli()->MediaSsrc());
  EXPECT_EQ(1, parser()->sli_item()->num_packets());
  EXPECT_EQ(kFirstMb, parser()->sli_item()->FirstMb());
  EXPECT_EQ(kNumberOfMb, parser()->sli_item()->NumberOfMb());
  EXPECT_EQ(kPictureId, parser()->sli_item()->PictureId());
}

TEST_F(RtcpSenderTest, SendNack) {
  rtcp_sender_->SetRTCPStatus(RtcpMode::kReducedSize);
  const uint16_t kList[] = {0, 1, 16};
  const int32_t kListLength = sizeof(kList) / sizeof(kList[0]);
  EXPECT_EQ(0, rtcp_sender_->SendRTCP(feedback_state(), kRtcpNack, kListLength,
                                      kList));
  EXPECT_EQ(1, parser()->nack()->num_packets());
  EXPECT_EQ(kSenderSsrc, parser()->nack()->Ssrc());
  EXPECT_EQ(kRemoteSsrc, parser()->nack()->MediaSsrc());
  EXPECT_EQ(1, parser()->nack_item()->num_packets());
  EXPECT_THAT(parser()->nack_item()->last_nack_list(), ElementsAre(0, 1, 16));
}

TEST_F(RtcpSenderTest, SendRemb) {
  const int kBitrate = 261011;
  std::vector<uint32_t> ssrcs;
  ssrcs.push_back(kRemoteSsrc);
  ssrcs.push_back(kRemoteSsrc + 1);
  rtcp_sender_->SetRTCPStatus(RtcpMode::kReducedSize);
  rtcp_sender_->SetREMBData(kBitrate, ssrcs);
  EXPECT_EQ(0, rtcp_sender_->SendRTCP(feedback_state(), kRtcpRemb));
  EXPECT_EQ(1, parser()->psfb_app()->num_packets());
  EXPECT_EQ(kSenderSsrc, parser()->psfb_app()->Ssrc());
  EXPECT_EQ(1, parser()->remb_item()->num_packets());
  EXPECT_EQ(kBitrate, parser()->remb_item()->last_bitrate_bps());
  EXPECT_THAT(parser()->remb_item()->last_ssrc_list(),
              ElementsAre(kRemoteSsrc, kRemoteSsrc + 1));
}

TEST_F(RtcpSenderTest, RembIncludedInCompoundPacketIfEnabled) {
  const int kBitrate = 261011;
  std::vector<uint32_t> ssrcs;
  ssrcs.push_back(kRemoteSsrc);
  rtcp_sender_->SetRTCPStatus(RtcpMode::kCompound);
  rtcp_sender_->SetREMBStatus(true);
  EXPECT_TRUE(rtcp_sender_->REMB());
  rtcp_sender_->SetREMBData(kBitrate, ssrcs);
  EXPECT_EQ(0, rtcp_sender_->SendRTCP(feedback_state(), kRtcpReport));
  EXPECT_EQ(1, parser()->psfb_app()->num_packets());
  EXPECT_EQ(1, parser()->remb_item()->num_packets());
  // REMB should be included in each compound packet.
  EXPECT_EQ(0, rtcp_sender_->SendRTCP(feedback_state(), kRtcpReport));
  EXPECT_EQ(2, parser()->psfb_app()->num_packets());
  EXPECT_EQ(2, parser()->remb_item()->num_packets());
}

TEST_F(RtcpSenderTest, RembNotIncludedInCompoundPacketIfNotEnabled) {
  const int kBitrate = 261011;
  std::vector<uint32_t> ssrcs;
  ssrcs.push_back(kRemoteSsrc);
  rtcp_sender_->SetRTCPStatus(RtcpMode::kCompound);
  rtcp_sender_->SetREMBData(kBitrate, ssrcs);
  EXPECT_FALSE(rtcp_sender_->REMB());
  EXPECT_EQ(0, rtcp_sender_->SendRTCP(feedback_state(), kRtcpReport));
  EXPECT_EQ(0, parser()->psfb_app()->num_packets());
}

TEST_F(RtcpSenderTest, SendXrWithVoipMetric) {
  rtcp_sender_->SetRTCPStatus(RtcpMode::kReducedSize);
  RTCPVoIPMetric metric;
  metric.lossRate = 1;
  metric.discardRate = 2;
  metric.burstDensity = 3;
  metric.gapDensity = 4;
  metric.burstDuration = 0x1111;
  metric.gapDuration = 0x2222;
  metric.roundTripDelay = 0x3333;
  metric.endSystemDelay = 0x4444;
  metric.signalLevel = 5;
  metric.noiseLevel = 6;
  metric.RERL = 7;
  metric.Gmin = 8;
  metric.Rfactor = 9;
  metric.extRfactor = 10;
  metric.MOSLQ = 11;
  metric.MOSCQ = 12;
  metric.RXconfig = 13;
  metric.JBnominal = 0x5555;
  metric.JBmax = 0x6666;
  metric.JBabsMax = 0x7777;
  EXPECT_EQ(0, rtcp_sender_->SetRTCPVoIPMetrics(&metric));
  EXPECT_EQ(0, rtcp_sender_->SendRTCP(feedback_state(), kRtcpXrVoipMetric));
  EXPECT_EQ(1, parser()->xr_header()->num_packets());
  EXPECT_EQ(kSenderSsrc, parser()->xr_header()->Ssrc());
  EXPECT_EQ(1, parser()->voip_metric()->num_packets());
  EXPECT_EQ(kRemoteSsrc, parser()->voip_metric()->Ssrc());
  EXPECT_EQ(metric.lossRate, parser()->voip_metric()->LossRate());
  EXPECT_EQ(metric.discardRate, parser()->voip_metric()->DiscardRate());
  EXPECT_EQ(metric.burstDensity, parser()->voip_metric()->BurstDensity());
  EXPECT_EQ(metric.gapDensity, parser()->voip_metric()->GapDensity());
  EXPECT_EQ(metric.burstDuration, parser()->voip_metric()->BurstDuration());
  EXPECT_EQ(metric.gapDuration, parser()->voip_metric()->GapDuration());
  EXPECT_EQ(metric.roundTripDelay, parser()->voip_metric()->RoundTripDelay());
  EXPECT_EQ(metric.endSystemDelay, parser()->voip_metric()->EndSystemDelay());
  EXPECT_EQ(metric.signalLevel, parser()->voip_metric()->SignalLevel());
  EXPECT_EQ(metric.noiseLevel, parser()->voip_metric()->NoiseLevel());
  EXPECT_EQ(metric.RERL, parser()->voip_metric()->Rerl());
  EXPECT_EQ(metric.Gmin, parser()->voip_metric()->Gmin());
  EXPECT_EQ(metric.Rfactor, parser()->voip_metric()->Rfactor());
  EXPECT_EQ(metric.extRfactor, parser()->voip_metric()->ExtRfactor());
  EXPECT_EQ(metric.MOSLQ, parser()->voip_metric()->MosLq());
  EXPECT_EQ(metric.MOSCQ, parser()->voip_metric()->MosCq());
  EXPECT_EQ(metric.RXconfig, parser()->voip_metric()->RxConfig());
  EXPECT_EQ(metric.JBnominal, parser()->voip_metric()->JbNominal());
  EXPECT_EQ(metric.JBmax, parser()->voip_metric()->JbMax());
  EXPECT_EQ(metric.JBabsMax, parser()->voip_metric()->JbAbsMax());
}

TEST_F(RtcpSenderTest, SendXrWithDlrr) {
  rtcp_sender_->SetRTCPStatus(RtcpMode::kCompound);
  RTCPSender::FeedbackState feedback_state = rtp_rtcp_impl_->GetFeedbackState();
  feedback_state.has_last_xr_rr = true;
  RtcpReceiveTimeInfo last_xr_rr;
  last_xr_rr.sourceSSRC = 0x11111111;
  last_xr_rr.lastRR = 0x22222222;
  last_xr_rr.delaySinceLastRR = 0x33333333;
  feedback_state.last_xr_rr = last_xr_rr;
  EXPECT_EQ(0, rtcp_sender_->SendRTCP(feedback_state, kRtcpReport));
  EXPECT_EQ(1, parser()->xr_header()->num_packets());
  EXPECT_EQ(kSenderSsrc, parser()->xr_header()->Ssrc());
  EXPECT_EQ(1, parser()->dlrr()->num_packets());
  EXPECT_EQ(1, parser()->dlrr_items()->num_packets());
  EXPECT_EQ(last_xr_rr.sourceSSRC, parser()->dlrr_items()->Ssrc(0));
  EXPECT_EQ(last_xr_rr.lastRR, parser()->dlrr_items()->LastRr(0));
  EXPECT_EQ(last_xr_rr.delaySinceLastRR,
            parser()->dlrr_items()->DelayLastRr(0));
}

TEST_F(RtcpSenderTest, SendXrWithRrtr) {
  rtcp_sender_->SetRTCPStatus(RtcpMode::kCompound);
  EXPECT_EQ(0, rtcp_sender_->SetSendingStatus(feedback_state(), false));
  rtcp_sender_->SendRtcpXrReceiverReferenceTime(true);
  uint32_t ntp_secs;
  uint32_t ntp_frac;
  clock_.CurrentNtp(ntp_secs, ntp_frac);
  EXPECT_EQ(0, rtcp_sender_->SendRTCP(feedback_state(), kRtcpReport));
  EXPECT_EQ(1, parser()->xr_header()->num_packets());
  EXPECT_EQ(kSenderSsrc, parser()->xr_header()->Ssrc());
  EXPECT_EQ(0, parser()->dlrr()->num_packets());
  EXPECT_EQ(1, parser()->rrtr()->num_packets());
  EXPECT_EQ(ntp_secs, parser()->rrtr()->NtpSec());
  EXPECT_EQ(ntp_frac, parser()->rrtr()->NtpFrac());
}

TEST_F(RtcpSenderTest, TestNoXrRrtrSentIfSending) {
  rtcp_sender_->SetRTCPStatus(RtcpMode::kCompound);
  EXPECT_EQ(0, rtcp_sender_->SetSendingStatus(feedback_state(), true));
  rtcp_sender_->SendRtcpXrReceiverReferenceTime(true);
  EXPECT_EQ(0, rtcp_sender_->SendRTCP(feedback_state(), kRtcpReport));
  EXPECT_EQ(0, parser()->xr_header()->num_packets());
  EXPECT_EQ(0, parser()->rrtr()->num_packets());
}

TEST_F(RtcpSenderTest, TestNoXrRrtrSentIfNotEnabled) {
  rtcp_sender_->SetRTCPStatus(RtcpMode::kCompound);
  EXPECT_EQ(0, rtcp_sender_->SetSendingStatus(feedback_state(), false));
  rtcp_sender_->SendRtcpXrReceiverReferenceTime(false);
  EXPECT_EQ(0, rtcp_sender_->SendRTCP(feedback_state(), kRtcpReport));
  EXPECT_EQ(0, parser()->xr_header()->num_packets());
  EXPECT_EQ(0, parser()->rrtr()->num_packets());
}

TEST_F(RtcpSenderTest, TestSendTimeOfXrRrtr) {
  rtcp_sender_->SetRTCPStatus(RtcpMode::kCompound);
  RTCPSender::FeedbackState feedback_state = rtp_rtcp_impl_->GetFeedbackState();
  EXPECT_EQ(0, rtcp_sender_->SetSendingStatus(feedback_state, false));
  rtcp_sender_->SendRtcpXrReceiverReferenceTime(true);
  uint32_t ntp_sec;
  uint32_t ntp_frac;
  clock_.CurrentNtp(ntp_sec, ntp_frac);
  uint32_t initial_mid_ntp = RTCPUtility::MidNtp(ntp_sec, ntp_frac);

  // No packet sent.
  int64_t time_ms;
  EXPECT_FALSE(rtcp_sender_->SendTimeOfXrRrReport(initial_mid_ntp, &time_ms));

  // Send XR RR packets.
  for (int i = 0; i <= RTCP_NUMBER_OF_SR; ++i) {
    EXPECT_EQ(0, rtcp_sender_->SendRTCP(feedback_state, kRtcpReport));
    EXPECT_EQ(i + 1, test_transport_.parser_.rrtr()->num_packets());
    clock_.CurrentNtp(ntp_sec, ntp_frac);
    uint32_t mid_ntp = RTCPUtility::MidNtp(ntp_sec, ntp_frac);
    EXPECT_TRUE(rtcp_sender_->SendTimeOfXrRrReport(mid_ntp, &time_ms));
    EXPECT_EQ(clock_.CurrentNtpInMilliseconds(), time_ms);
    clock_.AdvanceTimeMilliseconds(1000);
  }
  // The first report should no longer be stored.
  EXPECT_FALSE(rtcp_sender_->SendTimeOfXrRrReport(initial_mid_ntp, &time_ms));
}

TEST_F(RtcpSenderTest, TestRegisterRtcpPacketTypeObserver) {
  RtcpPacketTypeCounterObserverImpl observer;
  rtcp_sender_.reset(new RTCPSender(false, &clock_, receive_statistics_.get(),
                                    &observer, &test_transport_));
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
  EXPECT_EQ(kSenderSsrc, parser()->tmmbr()->Ssrc());
  EXPECT_EQ(1, parser()->tmmbr_item()->num_packets());
  EXPECT_EQ(kBitrateBps / 1000, parser()->tmmbr_item()->BitrateKbps());
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
  EXPECT_EQ(1, parser()->tmmbr_item()->num_packets());
  // TMMBR should be included in each compound packet.
  EXPECT_EQ(0, rtcp_sender_->SendRTCP(feedback_state(), kRtcpReport));
  EXPECT_EQ(2, parser()->tmmbr()->num_packets());
  EXPECT_EQ(2, parser()->tmmbr_item()->num_packets());

  rtcp_sender_->SetTMMBRStatus(false);
  EXPECT_FALSE(rtcp_sender_->TMMBR());
}

TEST_F(RtcpSenderTest, SendTmmbn) {
  rtcp_sender_->SetRTCPStatus(RtcpMode::kCompound);
  TMMBRSet bounding_set;
  bounding_set.VerifyAndAllocateSet(1);
  const uint32_t kBitrateKbps = 32768;
  const uint32_t kPacketOh = 40;
  const uint32_t kSourceSsrc = 12345;
  bounding_set.AddEntry(kBitrateKbps, kPacketOh, kSourceSsrc);
  EXPECT_EQ(0, rtcp_sender_->SetTMMBN(&bounding_set, 0));
  EXPECT_EQ(0, rtcp_sender_->SendRTCP(feedback_state(), kRtcpSr));
  EXPECT_EQ(1, parser()->sender_report()->num_packets());
  EXPECT_EQ(1, parser()->tmmbn()->num_packets());
  EXPECT_EQ(kSenderSsrc, parser()->tmmbn()->Ssrc());
  EXPECT_EQ(1, parser()->tmmbn_items()->num_packets());
  EXPECT_EQ(kBitrateKbps, parser()->tmmbn_items()->BitrateKbps(0));
  EXPECT_EQ(kPacketOh, parser()->tmmbn_items()->Overhead(0));
  EXPECT_EQ(kSourceSsrc, parser()->tmmbn_items()->Ssrc(0));
}

// This test is written to verify actual behaviour. It does not seem
// to make much sense to send an empty TMMBN, since there is no place
// to put an actual limit here. It's just information that no limit
// is set, which is kind of the starting assumption.
// See http://code.google.com/p/webrtc/issues/detail?id=468 for one
// situation where this caused confusion.
TEST_F(RtcpSenderTest, SendsTmmbnIfSetAndEmpty) {
  rtcp_sender_->SetRTCPStatus(RtcpMode::kCompound);
  TMMBRSet bounding_set;
  EXPECT_EQ(0, rtcp_sender_->SetTMMBN(&bounding_set, 3));
  EXPECT_EQ(0, rtcp_sender_->SendRTCP(feedback_state(), kRtcpSr));
  EXPECT_EQ(1, parser()->sender_report()->num_packets());
  EXPECT_EQ(1, parser()->tmmbn()->num_packets());
  EXPECT_EQ(kSenderSsrc, parser()->tmmbn()->Ssrc());
  EXPECT_EQ(0, parser()->tmmbn_items()->num_packets());
}

TEST_F(RtcpSenderTest, SendCompoundPliRemb) {
  const int kBitrate = 261011;
  std::vector<uint32_t> ssrcs;
  ssrcs.push_back(kRemoteSsrc);
  rtcp_sender_->SetRTCPStatus(RtcpMode::kCompound);
  rtcp_sender_->SetREMBData(kBitrate, ssrcs);
  std::set<RTCPPacketType> packet_types;
  packet_types.insert(kRtcpRemb);
  packet_types.insert(kRtcpPli);
  EXPECT_EQ(0, rtcp_sender_->SendCompoundRTCP(feedback_state(), packet_types));
  EXPECT_EQ(1, parser()->remb_item()->num_packets());
  EXPECT_EQ(1, parser()->pli()->num_packets());
}

}  // namespace webrtc
