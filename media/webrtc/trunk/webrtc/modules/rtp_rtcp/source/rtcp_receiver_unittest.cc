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
 * This file includes unit tests for the RTCPReceiver.
 */
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#include "webrtc/common_types.h"
#include "webrtc/modules/remote_bitrate_estimator/include/mock/mock_remote_bitrate_observer.h"
#include "webrtc/modules/remote_bitrate_estimator/remote_bitrate_estimator_single_stream.h"
#include "webrtc/modules/rtp_rtcp/source/byte_io.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_packet.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/app.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/bye.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/extended_jitter_report.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/pli.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/receiver_report.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/sli.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/tmmbr.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/transport_feedback.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_receiver.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_sender.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_rtcp_impl.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_utility.h"

namespace webrtc {

namespace {  // Anonymous namespace; hide utility functions and classes.

// This test transport verifies that no functions get called.
class TestTransport : public Transport,
                      public NullRtpData {
 public:
  explicit TestTransport() : rtcp_receiver_(nullptr) {}
  void SetRTCPReceiver(RTCPReceiver* rtcp_receiver) {
    rtcp_receiver_ = rtcp_receiver;
  }
  bool SendRtp(const uint8_t* /*data*/,
               size_t /*len*/,
               const PacketOptions& options) override {
    ADD_FAILURE();  // FAIL() gives a compile error.
    return false;
  }

  bool SendRtcp(const uint8_t* packet, size_t packet_len) override {
    ADD_FAILURE();
    return true;
  }

  int OnReceivedPayloadData(const uint8_t* payloadData,
                            const size_t payloadSize,
                            const WebRtcRTPHeader* rtpHeader) override {
    ADD_FAILURE();
    return 0;
  }
  RTCPReceiver* rtcp_receiver_;
};

class RtcpReceiverTest : public ::testing::Test {
 protected:
  RtcpReceiverTest()
      : over_use_detector_options_(),
        system_clock_(1335900000),
        remote_bitrate_observer_(),
        remote_bitrate_estimator_(
            new RemoteBitrateEstimatorSingleStream(&remote_bitrate_observer_,
                                                   &system_clock_)) {
    test_transport_ = new TestTransport();

    RtpRtcp::Configuration configuration;
    configuration.audio = false;
    configuration.clock = &system_clock_;
    configuration.outgoing_transport = test_transport_;
    configuration.remote_bitrate_estimator = remote_bitrate_estimator_.get();
    rtp_rtcp_impl_ = new ModuleRtpRtcpImpl(configuration);
    rtcp_receiver_ = new RTCPReceiver(&system_clock_, false, nullptr, nullptr,
                                      nullptr, nullptr, rtp_rtcp_impl_);
    test_transport_->SetRTCPReceiver(rtcp_receiver_);
  }
  ~RtcpReceiverTest() {
    delete rtcp_receiver_;
    delete rtp_rtcp_impl_;
    delete test_transport_;
  }

  // Injects an RTCP packet into the receiver.
  // Returns 0 for OK, non-0 for failure.
  int InjectRtcpPacket(const uint8_t* packet,
                       uint16_t packet_len) {
    RTCPUtility::RTCPParserV2 rtcpParser(packet,
                                         packet_len,
                                         true);  // Allow non-compound RTCP

    RTCPHelp::RTCPPacketInformation rtcpPacketInformation;
    EXPECT_EQ(0, rtcp_receiver_->IncomingRTCPPacket(rtcpPacketInformation,
                                                    &rtcpParser));
    rtcp_receiver_->TriggerCallbacksFromRTCPPacket(rtcpPacketInformation);
    // The NACK list is on purpose not copied below as it isn't needed by the
    // test.
    rtcp_packet_info_.rtcpPacketTypeFlags =
        rtcpPacketInformation.rtcpPacketTypeFlags;
    rtcp_packet_info_.remoteSSRC = rtcpPacketInformation.remoteSSRC;
    rtcp_packet_info_.applicationSubType =
        rtcpPacketInformation.applicationSubType;
    rtcp_packet_info_.applicationName = rtcpPacketInformation.applicationName;
    rtcp_packet_info_.applicationLength =
        rtcpPacketInformation.applicationLength;
    rtcp_packet_info_.report_blocks = rtcpPacketInformation.report_blocks;
    rtcp_packet_info_.rtt = rtcpPacketInformation.rtt;
    rtcp_packet_info_.interArrivalJitter =
        rtcpPacketInformation.interArrivalJitter;
    rtcp_packet_info_.sliPictureId = rtcpPacketInformation.sliPictureId;
    rtcp_packet_info_.rpsiPictureId = rtcpPacketInformation.rpsiPictureId;
    rtcp_packet_info_.receiverEstimatedMaxBitrate =
        rtcpPacketInformation.receiverEstimatedMaxBitrate;
    rtcp_packet_info_.ntp_secs = rtcpPacketInformation.ntp_secs;
    rtcp_packet_info_.ntp_frac = rtcpPacketInformation.ntp_frac;
    rtcp_packet_info_.rtp_timestamp = rtcpPacketInformation.rtp_timestamp;
    rtcp_packet_info_.xr_dlrr_item = rtcpPacketInformation.xr_dlrr_item;
    if (rtcpPacketInformation.VoIPMetric)
      rtcp_packet_info_.AddVoIPMetric(rtcpPacketInformation.VoIPMetric);
    rtcp_packet_info_.transport_feedback_.reset(
        rtcpPacketInformation.transport_feedback_.release());
    return 0;
  }

  OverUseDetectorOptions over_use_detector_options_;
  SimulatedClock system_clock_;
  ModuleRtpRtcpImpl* rtp_rtcp_impl_;
  RTCPReceiver* rtcp_receiver_;
  TestTransport* test_transport_;
  RTCPHelp::RTCPPacketInformation rtcp_packet_info_;
  MockRemoteBitrateObserver remote_bitrate_observer_;
  rtc::scoped_ptr<RemoteBitrateEstimator> remote_bitrate_estimator_;
};


TEST_F(RtcpReceiverTest, BrokenPacketIsIgnored) {
  const uint8_t bad_packet[] = {0, 0, 0, 0};
  EXPECT_EQ(0, InjectRtcpPacket(bad_packet, sizeof(bad_packet)));
  EXPECT_EQ(0U, rtcp_packet_info_.rtcpPacketTypeFlags);
}

TEST_F(RtcpReceiverTest, InjectSrPacket) {
  const uint32_t kSenderSsrc = 0x10203;
  rtcp::SenderReport sr;
  sr.From(kSenderSsrc);
  rtc::scoped_ptr<rtcp::RawPacket> packet(sr.Build());
  EXPECT_EQ(0, InjectRtcpPacket(packet->Buffer(), packet->Length()));
  // The parser will note the remote SSRC on a SR from other than his
  // expected peer, but will not flag that he's gotten a packet.
  EXPECT_EQ(kSenderSsrc, rtcp_packet_info_.remoteSSRC);
  EXPECT_EQ(0U,
            kRtcpSr & rtcp_packet_info_.rtcpPacketTypeFlags);
}

TEST_F(RtcpReceiverTest, InjectSrPacketFromExpectedPeer) {
  const uint32_t kSenderSsrc = 0x10203;
  rtcp_receiver_->SetRemoteSSRC(kSenderSsrc);
  rtcp::SenderReport sr;
  sr.From(kSenderSsrc);
  rtc::scoped_ptr<rtcp::RawPacket> packet(sr.Build());
  EXPECT_EQ(0, InjectRtcpPacket(packet->Buffer(), packet->Length()));
  EXPECT_EQ(kSenderSsrc, rtcp_packet_info_.remoteSSRC);
  EXPECT_EQ(kRtcpSr, rtcp_packet_info_.rtcpPacketTypeFlags);
}

TEST_F(RtcpReceiverTest, InjectRrPacket) {
  const uint32_t kSenderSsrc = 0x10203;
  rtcp::ReceiverReport rr;
  rr.From(kSenderSsrc);
  rtc::scoped_ptr<rtcp::RawPacket> packet(rr.Build());
  EXPECT_EQ(0, InjectRtcpPacket(packet->Buffer(), packet->Length()));
  EXPECT_EQ(kSenderSsrc, rtcp_packet_info_.remoteSSRC);
  EXPECT_EQ(kRtcpRr, rtcp_packet_info_.rtcpPacketTypeFlags);
  ASSERT_EQ(0u, rtcp_packet_info_.report_blocks.size());
}

TEST_F(RtcpReceiverTest, InjectRrPacketWithReportBlockNotToUsIgnored) {
  const uint32_t kSenderSsrc = 0x10203;
  const uint32_t kSourceSsrc = 0x123456;
  std::set<uint32_t> ssrcs;
  ssrcs.insert(kSourceSsrc);
  rtcp_receiver_->SetSsrcs(kSourceSsrc, ssrcs);

  rtcp::ReportBlock rb;
  rb.To(kSourceSsrc + 1);
  rtcp::ReceiverReport rr;
  rr.From(kSenderSsrc);
  rr.WithReportBlock(rb);
  rtc::scoped_ptr<rtcp::RawPacket> packet(rr.Build());
  EXPECT_EQ(0, InjectRtcpPacket(packet->Buffer(), packet->Length()));
  EXPECT_EQ(kSenderSsrc, rtcp_packet_info_.remoteSSRC);
  EXPECT_EQ(kRtcpRr, rtcp_packet_info_.rtcpPacketTypeFlags);
  ASSERT_EQ(0u, rtcp_packet_info_.report_blocks.size());

  std::vector<RTCPReportBlock> received_blocks;
  rtcp_receiver_->StatisticsReceived(&received_blocks);
  EXPECT_TRUE(received_blocks.empty());
}

TEST_F(RtcpReceiverTest, InjectRrPacketWithOneReportBlock) {
  const uint32_t kSenderSsrc = 0x10203;
  const uint32_t kSourceSsrc = 0x123456;
  std::set<uint32_t> ssrcs;
  ssrcs.insert(kSourceSsrc);
  rtcp_receiver_->SetSsrcs(kSourceSsrc, ssrcs);

  rtcp::ReportBlock rb;
  rb.To(kSourceSsrc);
  rtcp::ReceiverReport rr;
  rr.From(kSenderSsrc);
  rr.WithReportBlock(rb);
  rtc::scoped_ptr<rtcp::RawPacket> packet(rr.Build());
  EXPECT_EQ(0, InjectRtcpPacket(packet->Buffer(), packet->Length()));
  EXPECT_EQ(kSenderSsrc, rtcp_packet_info_.remoteSSRC);
  EXPECT_EQ(kRtcpRr, rtcp_packet_info_.rtcpPacketTypeFlags);
  ASSERT_EQ(1u, rtcp_packet_info_.report_blocks.size());

  std::vector<RTCPReportBlock> received_blocks;
  rtcp_receiver_->StatisticsReceived(&received_blocks);
  EXPECT_EQ(1u, received_blocks.size());
}

TEST_F(RtcpReceiverTest, InjectRrPacketWithTwoReportBlocks) {
  const uint32_t kSenderSsrc = 0x10203;
  const uint32_t kSourceSsrcs[] = {0x40506, 0x50607};
  const uint16_t kSequenceNumbers[] = {10, 12423};
  const uint32_t kCumLost[] = {13, 555};
  const uint8_t kFracLost[] = {20, 11};
  const int kNumSsrcs = sizeof(kSourceSsrcs) / sizeof(kSourceSsrcs[0]);

  std::set<uint32_t> ssrcs(kSourceSsrcs, kSourceSsrcs + kNumSsrcs);
  rtcp_receiver_->SetSsrcs(kSourceSsrcs[0], ssrcs);

  rtcp::ReportBlock rb1;
  rb1.To(kSourceSsrcs[0]);
  rb1.WithExtHighestSeqNum(kSequenceNumbers[0]);
  rb1.WithFractionLost(10);
  rb1.WithCumulativeLost(5);

  rtcp::ReportBlock rb2;
  rb2.To(kSourceSsrcs[1]);
  rb2.WithExtHighestSeqNum(kSequenceNumbers[1]);

  rtcp::ReceiverReport rr1;
  rr1.From(kSenderSsrc);
  rr1.WithReportBlock(rb1);
  rr1.WithReportBlock(rb2);

  rtc::scoped_ptr<rtcp::RawPacket> p1(rr1.Build());
  EXPECT_EQ(0, InjectRtcpPacket(p1->Buffer(), p1->Length()));
  ASSERT_EQ(2u, rtcp_packet_info_.report_blocks.size());
  EXPECT_EQ(10, rtcp_packet_info_.report_blocks.front().fractionLost);
  EXPECT_EQ(0, rtcp_packet_info_.report_blocks.back().fractionLost);

  rtcp::ReportBlock rb3;
  rb3.To(kSourceSsrcs[0]);
  rb3.WithExtHighestSeqNum(kSequenceNumbers[0]);
  rb3.WithFractionLost(kFracLost[0]);
  rb3.WithCumulativeLost(kCumLost[0]);

  rtcp::ReportBlock rb4;
  rb4.To(kSourceSsrcs[1]);
  rb4.WithExtHighestSeqNum(kSequenceNumbers[1]);
  rb4.WithFractionLost(kFracLost[1]);
  rb4.WithCumulativeLost(kCumLost[1]);

  rtcp::ReceiverReport rr2;
  rr2.From(kSenderSsrc);
  rr2.WithReportBlock(rb3);
  rr2.WithReportBlock(rb4);

  rtc::scoped_ptr<rtcp::RawPacket> p2(rr2.Build());
  EXPECT_EQ(0, InjectRtcpPacket(p2->Buffer(), p2->Length()));
  ASSERT_EQ(2u, rtcp_packet_info_.report_blocks.size());
  EXPECT_EQ(kFracLost[0], rtcp_packet_info_.report_blocks.front().fractionLost);
  EXPECT_EQ(kFracLost[1], rtcp_packet_info_.report_blocks.back().fractionLost);

  std::vector<RTCPReportBlock> received_blocks;
  rtcp_receiver_->StatisticsReceived(&received_blocks);
  EXPECT_EQ(2u, received_blocks.size());
  for (size_t i = 0; i < received_blocks.size(); ++i) {
    EXPECT_EQ(kSenderSsrc, received_blocks[i].remoteSSRC);
    EXPECT_EQ(kSourceSsrcs[i], received_blocks[i].sourceSSRC);
    EXPECT_EQ(kFracLost[i], received_blocks[i].fractionLost);
    EXPECT_EQ(kCumLost[i], received_blocks[i].cumulativeLost);
    EXPECT_EQ(kSequenceNumbers[i], received_blocks[i].extendedHighSeqNum);
  }
}

TEST_F(RtcpReceiverTest, InjectRrPacketsFromTwoRemoteSsrcs) {
  const uint32_t kSenderSsrc1 = 0x10203;
  const uint32_t kSenderSsrc2 = 0x20304;
  const uint32_t kSourceSsrcs[] = {0x40506, 0x50607};
  const uint16_t kSequenceNumbers[] = {10, 12423};
  const uint32_t kCumLost[] = {13, 555};
  const uint8_t kFracLost[] = {20, 11};
  const int kNumSsrcs = sizeof(kSourceSsrcs) / sizeof(kSourceSsrcs[0]);

  std::set<uint32_t> ssrcs(kSourceSsrcs, kSourceSsrcs + kNumSsrcs);
  rtcp_receiver_->SetSsrcs(kSourceSsrcs[0], ssrcs);

  rtcp::ReportBlock rb1;
  rb1.To(kSourceSsrcs[0]);
  rb1.WithExtHighestSeqNum(kSequenceNumbers[0]);
  rb1.WithFractionLost(kFracLost[0]);
  rb1.WithCumulativeLost(kCumLost[0]);
  rtcp::ReceiverReport rr1;
  rr1.From(kSenderSsrc1);
  rr1.WithReportBlock(rb1);

  rtc::scoped_ptr<rtcp::RawPacket> p1(rr1.Build());
  EXPECT_EQ(0, InjectRtcpPacket(p1->Buffer(), p1->Length()));
  ASSERT_EQ(1u, rtcp_packet_info_.report_blocks.size());
  EXPECT_EQ(kFracLost[0], rtcp_packet_info_.report_blocks.front().fractionLost);

  std::vector<RTCPReportBlock> received_blocks;
  rtcp_receiver_->StatisticsReceived(&received_blocks);
  EXPECT_EQ(1u, received_blocks.size());
  EXPECT_EQ(kSenderSsrc1, received_blocks[0].remoteSSRC);
  EXPECT_EQ(kSourceSsrcs[0], received_blocks[0].sourceSSRC);
  EXPECT_EQ(kFracLost[0], received_blocks[0].fractionLost);
  EXPECT_EQ(kCumLost[0], received_blocks[0].cumulativeLost);
  EXPECT_EQ(kSequenceNumbers[0], received_blocks[0].extendedHighSeqNum);

  rtcp::ReportBlock rb2;
  rb2.To(kSourceSsrcs[0]);
  rb2.WithExtHighestSeqNum(kSequenceNumbers[1]);
  rb2.WithFractionLost(kFracLost[1]);
  rb2.WithCumulativeLost(kCumLost[1]);
  rtcp::ReceiverReport rr2;
  rr2.From(kSenderSsrc2);
  rr2.WithReportBlock(rb2);
  rtc::scoped_ptr<rtcp::RawPacket> p2(rr2.Build());
  EXPECT_EQ(0, InjectRtcpPacket(p2->Buffer(), p2->Length()));
  ASSERT_EQ(1u, rtcp_packet_info_.report_blocks.size());
  EXPECT_EQ(kFracLost[1], rtcp_packet_info_.report_blocks.front().fractionLost);

  received_blocks.clear();
  rtcp_receiver_->StatisticsReceived(&received_blocks);
  ASSERT_EQ(2u, received_blocks.size());
  EXPECT_EQ(kSenderSsrc1, received_blocks[0].remoteSSRC);
  EXPECT_EQ(kSenderSsrc2, received_blocks[1].remoteSSRC);
  for (size_t i = 0; i < received_blocks.size(); ++i) {
    EXPECT_EQ(kSourceSsrcs[0], received_blocks[i].sourceSSRC);
    EXPECT_EQ(kFracLost[i], received_blocks[i].fractionLost);
    EXPECT_EQ(kCumLost[i], received_blocks[i].cumulativeLost);
    EXPECT_EQ(kSequenceNumbers[i], received_blocks[i].extendedHighSeqNum);
  }
}

TEST_F(RtcpReceiverTest, GetRtt) {
  const uint32_t kSenderSsrc = 0x10203;
  const uint32_t kSourceSsrc = 0x123456;
  std::set<uint32_t> ssrcs;
  ssrcs.insert(kSourceSsrc);
  rtcp_receiver_->SetSsrcs(kSourceSsrc, ssrcs);

  // No report block received.
  EXPECT_EQ(
      -1, rtcp_receiver_->RTT(kSenderSsrc, nullptr, nullptr, nullptr, nullptr));

  rtcp::ReportBlock rb;
  rb.To(kSourceSsrc);
  rtcp::ReceiverReport rr;
  rr.From(kSenderSsrc);
  rr.WithReportBlock(rb);
  rtc::scoped_ptr<rtcp::RawPacket> packet(rr.Build());
  EXPECT_EQ(0, InjectRtcpPacket(packet->Buffer(), packet->Length()));
  EXPECT_EQ(kSenderSsrc, rtcp_packet_info_.remoteSSRC);
  EXPECT_EQ(kRtcpRr, rtcp_packet_info_.rtcpPacketTypeFlags);
  EXPECT_EQ(1u, rtcp_packet_info_.report_blocks.size());
  EXPECT_EQ(
      0, rtcp_receiver_->RTT(kSenderSsrc, nullptr, nullptr, nullptr, nullptr));

  // Report block not received.
  EXPECT_EQ(-1, rtcp_receiver_->RTT(kSenderSsrc + 1, nullptr, nullptr, nullptr,
                                    nullptr));
}

TEST_F(RtcpReceiverTest, InjectIjWithNoItem) {
  rtcp::ExtendedJitterReport ij;
  rtc::scoped_ptr<rtcp::RawPacket> packet(ij.Build());
  EXPECT_EQ(0, InjectRtcpPacket(packet->Buffer(), packet->Length()));
  EXPECT_EQ(0U, rtcp_packet_info_.rtcpPacketTypeFlags);
}

TEST_F(RtcpReceiverTest, InjectIjWithOneItem) {
  rtcp::ExtendedJitterReport ij;
  ij.WithJitter(0x11213141);

  rtc::scoped_ptr<rtcp::RawPacket> packet(ij.Build());
  EXPECT_EQ(0, InjectRtcpPacket(packet->Buffer(), packet->Length()));
  EXPECT_EQ(kRtcpTransmissionTimeOffset, rtcp_packet_info_.rtcpPacketTypeFlags);
  EXPECT_EQ(0x11213141U, rtcp_packet_info_.interArrivalJitter);
}

TEST_F(RtcpReceiverTest, InjectAppWithNoData) {
  rtcp::App app;
  app.WithSubType(30);
  uint32_t name = 'n' << 24;
  name += 'a' << 16;
  name += 'm' << 8;
  name += 'e';
  app.WithName(name);

  rtc::scoped_ptr<rtcp::RawPacket> packet(app.Build());
  EXPECT_EQ(0, InjectRtcpPacket(packet->Buffer(), packet->Length()));
  EXPECT_EQ(kRtcpApp, rtcp_packet_info_.rtcpPacketTypeFlags);
  EXPECT_EQ(30, rtcp_packet_info_.applicationSubType);
  EXPECT_EQ(name, rtcp_packet_info_.applicationName);
  EXPECT_EQ(0, rtcp_packet_info_.applicationLength);
}

TEST_F(RtcpReceiverTest, InjectAppWithData) {
  rtcp::App app;
  app.WithSubType(30);
  uint32_t name = 'n' << 24;
  name += 'a' << 16;
  name += 'm' << 8;
  name += 'e';
  app.WithName(name);
  const char kData[] = {'t', 'e', 's', 't', 'd', 'a', 't', 'a'};
  const size_t kDataLength = sizeof(kData) / sizeof(kData[0]);
  app.WithData((const uint8_t*)kData, kDataLength);

  rtc::scoped_ptr<rtcp::RawPacket> packet(app.Build());
  EXPECT_EQ(0, InjectRtcpPacket(packet->Buffer(), packet->Length()));
  EXPECT_EQ(kRtcpApp, rtcp_packet_info_.rtcpPacketTypeFlags);
  EXPECT_EQ(30, rtcp_packet_info_.applicationSubType);
  EXPECT_EQ(name, rtcp_packet_info_.applicationName);
  EXPECT_EQ(kDataLength, rtcp_packet_info_.applicationLength);
}

TEST_F(RtcpReceiverTest, InjectSdesWithOneChunk) {
  const uint32_t kSenderSsrc = 0x123456;
  rtcp::Sdes sdes;
  sdes.WithCName(kSenderSsrc, "alice@host");

  rtc::scoped_ptr<rtcp::RawPacket> packet(sdes.Build());
  EXPECT_EQ(0, InjectRtcpPacket(packet->Buffer(), packet->Length()));
  char cName[RTCP_CNAME_SIZE];
  EXPECT_EQ(0, rtcp_receiver_->CNAME(kSenderSsrc, cName));
  EXPECT_EQ(0, strncmp(cName, "alice@host", RTCP_CNAME_SIZE));
}

TEST_F(RtcpReceiverTest, InjectByePacket_RemovesCname) {
  const uint32_t kSenderSsrc = 0x123456;
  rtcp::Sdes sdes;
  sdes.WithCName(kSenderSsrc, "alice@host");

  rtc::scoped_ptr<rtcp::RawPacket> packet(sdes.Build());
  EXPECT_EQ(0, InjectRtcpPacket(packet->Buffer(), packet->Length()));
  char cName[RTCP_CNAME_SIZE];
  EXPECT_EQ(0, rtcp_receiver_->CNAME(kSenderSsrc, cName));

  // Verify that BYE removes the CNAME.
  rtcp::Bye bye;
  bye.From(kSenderSsrc);
  rtc::scoped_ptr<rtcp::RawPacket> p2(bye.Build());
  EXPECT_EQ(0, InjectRtcpPacket(p2->Buffer(), p2->Length()));
  EXPECT_EQ(-1, rtcp_receiver_->CNAME(kSenderSsrc, cName));
}

TEST_F(RtcpReceiverTest, InjectByePacket_RemovesReportBlocks) {
  const uint32_t kSenderSsrc = 0x10203;
  const uint32_t kSourceSsrcs[] = {0x40506, 0x50607};
  const int kNumSsrcs = sizeof(kSourceSsrcs) / sizeof(kSourceSsrcs[0]);

  std::set<uint32_t> ssrcs(kSourceSsrcs, kSourceSsrcs + kNumSsrcs);
  rtcp_receiver_->SetSsrcs(kSourceSsrcs[0], ssrcs);

  rtcp::ReportBlock rb1;
  rb1.To(kSourceSsrcs[0]);
  rtcp::ReportBlock rb2;
  rb2.To(kSourceSsrcs[1]);
  rtcp::ReceiverReport rr;
  rr.From(kSenderSsrc);
  rr.WithReportBlock(rb1);
  rr.WithReportBlock(rb2);

  rtc::scoped_ptr<rtcp::RawPacket> p1(rr.Build());
  EXPECT_EQ(0, InjectRtcpPacket(p1->Buffer(), p1->Length()));
  ASSERT_EQ(2u, rtcp_packet_info_.report_blocks.size());
  std::vector<RTCPReportBlock> received_blocks;
  rtcp_receiver_->StatisticsReceived(&received_blocks);
  EXPECT_EQ(2u, received_blocks.size());

  // Verify that BYE removes the report blocks.
  rtcp::Bye bye;
  bye.From(kSenderSsrc);
  rtc::scoped_ptr<rtcp::RawPacket> p2(bye.Build());
  EXPECT_EQ(0, InjectRtcpPacket(p2->Buffer(), p2->Length()));
  received_blocks.clear();
  rtcp_receiver_->StatisticsReceived(&received_blocks);
  EXPECT_TRUE(received_blocks.empty());

  // Inject packet.
  EXPECT_EQ(0, InjectRtcpPacket(p1->Buffer(), p1->Length()));
  ASSERT_EQ(2u, rtcp_packet_info_.report_blocks.size());
  received_blocks.clear();
  rtcp_receiver_->StatisticsReceived(&received_blocks);
  EXPECT_EQ(2u, received_blocks.size());
}

TEST_F(RtcpReceiverTest, InjectPliPacket) {
  const uint32_t kSourceSsrc = 0x123456;
  std::set<uint32_t> ssrcs;
  ssrcs.insert(kSourceSsrc);
  rtcp_receiver_->SetSsrcs(kSourceSsrc, ssrcs);

  rtcp::Pli pli;
  pli.To(kSourceSsrc);
  rtc::scoped_ptr<rtcp::RawPacket> packet(pli.Build());
  EXPECT_EQ(0, InjectRtcpPacket(packet->Buffer(), packet->Length()));
  EXPECT_EQ(kRtcpPli, rtcp_packet_info_.rtcpPacketTypeFlags);
}

TEST_F(RtcpReceiverTest, PliPacketNotToUsIgnored) {
  const uint32_t kSourceSsrc = 0x123456;
  std::set<uint32_t> ssrcs;
  ssrcs.insert(kSourceSsrc);
  rtcp_receiver_->SetSsrcs(kSourceSsrc, ssrcs);

  rtcp::Pli pli;
  pli.To(kSourceSsrc + 1);
  rtc::scoped_ptr<rtcp::RawPacket> packet(pli.Build());
  EXPECT_EQ(0, InjectRtcpPacket(packet->Buffer(), packet->Length()));
  EXPECT_EQ(0U, rtcp_packet_info_.rtcpPacketTypeFlags);
}

TEST_F(RtcpReceiverTest, InjectFirPacket) {
  const uint32_t kSourceSsrc = 0x123456;
  std::set<uint32_t> ssrcs;
  ssrcs.insert(kSourceSsrc);
  rtcp_receiver_->SetSsrcs(kSourceSsrc, ssrcs);

  rtcp::Fir fir;
  fir.To(kSourceSsrc);
  rtc::scoped_ptr<rtcp::RawPacket> packet(fir.Build());
  EXPECT_EQ(0, InjectRtcpPacket(packet->Buffer(), packet->Length()));
  EXPECT_EQ(kRtcpFir, rtcp_packet_info_.rtcpPacketTypeFlags);
}

TEST_F(RtcpReceiverTest, FirPacketNotToUsIgnored) {
  const uint32_t kSourceSsrc = 0x123456;
  std::set<uint32_t> ssrcs;
  ssrcs.insert(kSourceSsrc);
  rtcp_receiver_->SetSsrcs(kSourceSsrc, ssrcs);

  rtcp::Fir fir;
  fir.To(kSourceSsrc + 1);
  rtc::scoped_ptr<rtcp::RawPacket> packet(fir.Build());
  EXPECT_EQ(0, InjectRtcpPacket(packet->Buffer(), packet->Length()));
  EXPECT_EQ(0U, rtcp_packet_info_.rtcpPacketTypeFlags);
}

TEST_F(RtcpReceiverTest, InjectSliPacket) {
  rtcp::Sli sli;
  sli.WithPictureId(40);
  rtc::scoped_ptr<rtcp::RawPacket> packet(sli.Build());
  EXPECT_EQ(0, InjectRtcpPacket(packet->Buffer(), packet->Length()));
  EXPECT_EQ(kRtcpSli, rtcp_packet_info_.rtcpPacketTypeFlags);
  EXPECT_EQ(40, rtcp_packet_info_.sliPictureId);
}

TEST_F(RtcpReceiverTest, XrPacketWithZeroReportBlocksIgnored) {
  rtcp::Xr xr;
  xr.From(0x2345);
  rtc::scoped_ptr<rtcp::RawPacket> packet(xr.Build());
  EXPECT_EQ(0, InjectRtcpPacket(packet->Buffer(), packet->Length()));
  EXPECT_EQ(0U, rtcp_packet_info_.rtcpPacketTypeFlags);
}

TEST_F(RtcpReceiverTest, InjectXrVoipPacket) {
  const uint32_t kSourceSsrc = 0x123456;
  std::set<uint32_t> ssrcs;
  ssrcs.insert(kSourceSsrc);
  rtcp_receiver_->SetSsrcs(kSourceSsrc, ssrcs);

  const uint8_t kLossRate = 123;
  rtcp::VoipMetric voip_metric;
  voip_metric.To(kSourceSsrc);
  RTCPVoIPMetric metric;
  metric.lossRate = kLossRate;
  voip_metric.WithVoipMetric(metric);
  rtcp::Xr xr;
  xr.From(0x2345);
  xr.WithVoipMetric(&voip_metric);
  rtc::scoped_ptr<rtcp::RawPacket> packet(xr.Build());
  EXPECT_EQ(0, InjectRtcpPacket(packet->Buffer(), packet->Length()));
  ASSERT_TRUE(rtcp_packet_info_.VoIPMetric != nullptr);
  EXPECT_EQ(kLossRate, rtcp_packet_info_.VoIPMetric->lossRate);
  EXPECT_EQ(kRtcpXrVoipMetric, rtcp_packet_info_.rtcpPacketTypeFlags);
}

TEST_F(RtcpReceiverTest, XrVoipPacketNotToUsIgnored) {
  const uint32_t kSourceSsrc = 0x123456;
  std::set<uint32_t> ssrcs;
  ssrcs.insert(kSourceSsrc);
  rtcp_receiver_->SetSsrcs(kSourceSsrc, ssrcs);

  rtcp::VoipMetric voip_metric;
  voip_metric.To(kSourceSsrc + 1);
  rtcp::Xr xr;
  xr.From(0x2345);
  xr.WithVoipMetric(&voip_metric);
  rtc::scoped_ptr<rtcp::RawPacket> packet(xr.Build());
  EXPECT_EQ(0, InjectRtcpPacket(packet->Buffer(), packet->Length()));
  EXPECT_EQ(0U, rtcp_packet_info_.rtcpPacketTypeFlags);
}

TEST_F(RtcpReceiverTest, InjectXrReceiverReferenceTimePacket) {
  rtcp::Rrtr rrtr;
  rrtr.WithNtp(NtpTime(0x10203, 0x40506));
  rtcp::Xr xr;
  xr.From(0x2345);
  xr.WithRrtr(&rrtr);

  rtc::scoped_ptr<rtcp::RawPacket> packet(xr.Build());
  EXPECT_EQ(0, InjectRtcpPacket(packet->Buffer(), packet->Length()));
  EXPECT_EQ(kRtcpXrReceiverReferenceTime,
            rtcp_packet_info_.rtcpPacketTypeFlags);
}

TEST_F(RtcpReceiverTest, XrDlrrPacketNotToUsIgnored) {
  const uint32_t kSourceSsrc = 0x123456;
  std::set<uint32_t> ssrcs;
  ssrcs.insert(kSourceSsrc);
  rtcp_receiver_->SetSsrcs(kSourceSsrc, ssrcs);

  rtcp::Dlrr dlrr;
  dlrr.WithDlrrItem(kSourceSsrc + 1, 0x12345, 0x67890);
  rtcp::Xr xr;
  xr.From(0x2345);
  xr.WithDlrr(&dlrr);
  rtc::scoped_ptr<rtcp::RawPacket> packet(xr.Build());
  EXPECT_EQ(0, InjectRtcpPacket(packet->Buffer(), packet->Length()));
  EXPECT_EQ(0U, rtcp_packet_info_.rtcpPacketTypeFlags);
  EXPECT_FALSE(rtcp_packet_info_.xr_dlrr_item);
}

TEST_F(RtcpReceiverTest, InjectXrDlrrPacketWithSubBlock) {
  const uint32_t kSourceSsrc = 0x123456;
  std::set<uint32_t> ssrcs;
  ssrcs.insert(kSourceSsrc);
  rtcp_receiver_->SetSsrcs(kSourceSsrc, ssrcs);

  rtcp::Dlrr dlrr;
  dlrr.WithDlrrItem(kSourceSsrc, 0x12345, 0x67890);
  rtcp::Xr xr;
  xr.From(0x2345);
  xr.WithDlrr(&dlrr);
  rtc::scoped_ptr<rtcp::RawPacket> packet(xr.Build());
  EXPECT_EQ(0, InjectRtcpPacket(packet->Buffer(), packet->Length()));
  // The parser should note the DLRR report block item, but not flag the packet
  // since the RTT is not estimated.
  EXPECT_TRUE(rtcp_packet_info_.xr_dlrr_item);
}

TEST_F(RtcpReceiverTest, InjectXrDlrrPacketWithMultipleSubBlocks) {
  const uint32_t kSourceSsrc = 0x123456;
  std::set<uint32_t> ssrcs;
  ssrcs.insert(kSourceSsrc);
  rtcp_receiver_->SetSsrcs(kSourceSsrc, ssrcs);

  rtcp::Dlrr dlrr;
  dlrr.WithDlrrItem(kSourceSsrc + 1, 0x12345, 0x67890);
  dlrr.WithDlrrItem(kSourceSsrc + 2, 0x12345, 0x67890);
  dlrr.WithDlrrItem(kSourceSsrc,     0x12345, 0x67890);
  rtcp::Xr xr;
  xr.From(0x2345);
  xr.WithDlrr(&dlrr);
  rtc::scoped_ptr<rtcp::RawPacket> packet(xr.Build());
  EXPECT_EQ(0, InjectRtcpPacket(packet->Buffer(), packet->Length()));
  // The parser should note the DLRR report block item, but not flag the packet
  // since the RTT is not estimated.
  EXPECT_TRUE(rtcp_packet_info_.xr_dlrr_item);
}

TEST_F(RtcpReceiverTest, InjectXrPacketWithMultipleReportBlocks) {
  const uint32_t kSourceSsrc = 0x123456;
  std::set<uint32_t> ssrcs;
  ssrcs.insert(kSourceSsrc);
  rtcp_receiver_->SetSsrcs(kSourceSsrc, ssrcs);

  rtcp::Rrtr rrtr;
  rtcp::Dlrr dlrr;
  dlrr.WithDlrrItem(kSourceSsrc, 0x12345, 0x67890);
  rtcp::VoipMetric metric;
  metric.To(kSourceSsrc);
  rtcp::Xr xr;
  xr.From(0x2345);
  xr.WithRrtr(&rrtr);
  xr.WithDlrr(&dlrr);
  xr.WithVoipMetric(&metric);
  rtc::scoped_ptr<rtcp::RawPacket> packet(xr.Build());
  EXPECT_EQ(0, InjectRtcpPacket(packet->Buffer(), packet->Length()));
  EXPECT_EQ(static_cast<unsigned int>(kRtcpXrReceiverReferenceTime +
                                      kRtcpXrVoipMetric),
            rtcp_packet_info_.rtcpPacketTypeFlags);
  // The parser should note the DLRR report block item, but not flag the packet
  // since the RTT is not estimated.
  EXPECT_TRUE(rtcp_packet_info_.xr_dlrr_item);
}

TEST_F(RtcpReceiverTest, InjectXrPacketWithUnknownReportBlock) {
  const uint32_t kSourceSsrc = 0x123456;
  std::set<uint32_t> ssrcs;
  ssrcs.insert(kSourceSsrc);
  rtcp_receiver_->SetSsrcs(kSourceSsrc, ssrcs);
  std::vector<uint32_t> remote_ssrcs;
  remote_ssrcs.push_back(kSourceSsrc);

  rtcp::Rrtr rrtr;
  rtcp::Dlrr dlrr;
  dlrr.WithDlrrItem(kSourceSsrc, 0x12345, 0x67890);
  rtcp::VoipMetric metric;
  metric.To(kSourceSsrc);
  rtcp::Xr xr;
  xr.From(0x2345);
  xr.WithRrtr(&rrtr);
  xr.WithDlrr(&dlrr);
  xr.WithVoipMetric(&metric);
  rtc::scoped_ptr<rtcp::RawPacket> packet(xr.Build());
  // Modify the DLRR block to have an unsupported block type, from 5 to 6.
  uint8_t* buffer = const_cast<uint8_t*>(packet->Buffer());
  EXPECT_EQ(5, buffer[20]);
  buffer[20] = 6;

  EXPECT_EQ(0, InjectRtcpPacket(packet->Buffer(), packet->Length()));
  EXPECT_EQ(static_cast<unsigned int>(kRtcpXrReceiverReferenceTime +
                                      kRtcpXrVoipMetric),
            rtcp_packet_info_.rtcpPacketTypeFlags);
  EXPECT_FALSE(rtcp_packet_info_.xr_dlrr_item);
}

TEST_F(RtcpReceiverTest, TestXrRrRttInitiallyFalse) {
  int64_t rtt_ms;
  EXPECT_FALSE(rtcp_receiver_->GetAndResetXrRrRtt(&rtt_ms));
}

TEST_F(RtcpReceiverTest, LastReceivedXrReferenceTimeInfoInitiallyFalse) {
  RtcpReceiveTimeInfo info;
  EXPECT_FALSE(rtcp_receiver_->LastReceivedXrReferenceTimeInfo(&info));
}

TEST_F(RtcpReceiverTest, GetLastReceivedXrReferenceTimeInfo) {
  const uint32_t kSenderSsrc = 0x123456;
  const NtpTime kNtp(0x10203, 0x40506);
  const uint32_t kNtpMid =
      RTCPUtility::MidNtp(kNtp.seconds(), kNtp.fractions());

  rtcp::Rrtr rrtr;
  rrtr.WithNtp(kNtp);
  rtcp::Xr xr;
  xr.From(kSenderSsrc);
  xr.WithRrtr(&rrtr);
  rtc::scoped_ptr<rtcp::RawPacket> packet(xr.Build());
  EXPECT_EQ(0, InjectRtcpPacket(packet->Buffer(), packet->Length()));
  EXPECT_EQ(kRtcpXrReceiverReferenceTime,
      rtcp_packet_info_.rtcpPacketTypeFlags);

  RtcpReceiveTimeInfo info;
  EXPECT_TRUE(rtcp_receiver_->LastReceivedXrReferenceTimeInfo(&info));
  EXPECT_EQ(kSenderSsrc, info.sourceSSRC);
  EXPECT_EQ(kNtpMid, info.lastRR);
  EXPECT_EQ(0U, info.delaySinceLastRR);

  system_clock_.AdvanceTimeMilliseconds(1000);
  EXPECT_TRUE(rtcp_receiver_->LastReceivedXrReferenceTimeInfo(&info));
  EXPECT_EQ(65536U, info.delaySinceLastRR);
}

TEST_F(RtcpReceiverTest, ReceiveReportTimeout) {
  const uint32_t kSenderSsrc = 0x10203;
  const uint32_t kSourceSsrc = 0x40506;
  const int64_t kRtcpIntervalMs = 1000;

  std::set<uint32_t> ssrcs;
  ssrcs.insert(kSourceSsrc);
  rtcp_receiver_->SetSsrcs(kSourceSsrc, ssrcs);

  const uint16_t kSequenceNumber = 1234;
  system_clock_.AdvanceTimeMilliseconds(3 * kRtcpIntervalMs);

  // No RR received, shouldn't trigger a timeout.
  EXPECT_FALSE(rtcp_receiver_->RtcpRrTimeout(kRtcpIntervalMs));
  EXPECT_FALSE(rtcp_receiver_->RtcpRrSequenceNumberTimeout(kRtcpIntervalMs));

  // Add a RR and advance the clock just enough to not trigger a timeout.
  rtcp::ReportBlock rb1;
  rb1.To(kSourceSsrc);
  rb1.WithExtHighestSeqNum(kSequenceNumber);
  rtcp::ReceiverReport rr1;
  rr1.From(kSenderSsrc);
  rr1.WithReportBlock(rb1);
  rtc::scoped_ptr<rtcp::RawPacket> p1(rr1.Build());
  EXPECT_EQ(0, InjectRtcpPacket(p1->Buffer(), p1->Length()));
  system_clock_.AdvanceTimeMilliseconds(3 * kRtcpIntervalMs - 1);
  EXPECT_FALSE(rtcp_receiver_->RtcpRrTimeout(kRtcpIntervalMs));
  EXPECT_FALSE(rtcp_receiver_->RtcpRrSequenceNumberTimeout(kRtcpIntervalMs));

  // Add a RR with the same extended max as the previous RR to trigger a
  // sequence number timeout, but not a RR timeout.
  EXPECT_EQ(0, InjectRtcpPacket(p1->Buffer(), p1->Length()));
  system_clock_.AdvanceTimeMilliseconds(2);
  EXPECT_FALSE(rtcp_receiver_->RtcpRrTimeout(kRtcpIntervalMs));
  EXPECT_TRUE(rtcp_receiver_->RtcpRrSequenceNumberTimeout(kRtcpIntervalMs));

  // Advance clock enough to trigger an RR timeout too.
  system_clock_.AdvanceTimeMilliseconds(3 * kRtcpIntervalMs);
  EXPECT_TRUE(rtcp_receiver_->RtcpRrTimeout(kRtcpIntervalMs));

  // We should only get one timeout even though we still haven't received a new
  // RR.
  EXPECT_FALSE(rtcp_receiver_->RtcpRrTimeout(kRtcpIntervalMs));
  EXPECT_FALSE(rtcp_receiver_->RtcpRrSequenceNumberTimeout(kRtcpIntervalMs));

  // Add a new RR with increase sequence number to reset timers.
  rtcp::ReportBlock rb2;
  rb2.To(kSourceSsrc);
  rb2.WithExtHighestSeqNum(kSequenceNumber + 1);
  rtcp::ReceiverReport rr2;
  rr2.From(kSenderSsrc);
  rr2.WithReportBlock(rb2);
  rtc::scoped_ptr<rtcp::RawPacket> p2(rr2.Build());
  EXPECT_EQ(0, InjectRtcpPacket(p2->Buffer(), p2->Length()));
  EXPECT_FALSE(rtcp_receiver_->RtcpRrTimeout(kRtcpIntervalMs));
  EXPECT_FALSE(rtcp_receiver_->RtcpRrSequenceNumberTimeout(kRtcpIntervalMs));

  // Verify we can get a timeout again once we've received new RR.
  system_clock_.AdvanceTimeMilliseconds(2 * kRtcpIntervalMs);
  EXPECT_EQ(0, InjectRtcpPacket(p2->Buffer(), p2->Length()));
  system_clock_.AdvanceTimeMilliseconds(kRtcpIntervalMs + 1);
  EXPECT_FALSE(rtcp_receiver_->RtcpRrTimeout(kRtcpIntervalMs));
  EXPECT_TRUE(rtcp_receiver_->RtcpRrSequenceNumberTimeout(kRtcpIntervalMs));
  system_clock_.AdvanceTimeMilliseconds(2 * kRtcpIntervalMs);
  EXPECT_TRUE(rtcp_receiver_->RtcpRrTimeout(kRtcpIntervalMs));
}

TEST_F(RtcpReceiverTest, TmmbrReceivedWithNoIncomingPacket) {
  // This call is expected to fail because no data has arrived.
  EXPECT_EQ(-1, rtcp_receiver_->TMMBRReceived(0, 0, nullptr));
}

TEST_F(RtcpReceiverTest, TmmbrPacketAccepted) {
  const uint32_t kMediaFlowSsrc = 0x2040608;
  const uint32_t kSenderSsrc = 0x10203;
  std::set<uint32_t> ssrcs;
  ssrcs.insert(kMediaFlowSsrc);  // Matches "media source" above.
  rtcp_receiver_->SetSsrcs(kMediaFlowSsrc, ssrcs);

  rtcp::Tmmbr tmmbr;
  tmmbr.From(kSenderSsrc);
  tmmbr.To(kMediaFlowSsrc);
  tmmbr.WithBitrateKbps(30);

  rtcp::SenderReport sr;
  sr.From(kSenderSsrc);
  sr.Append(&tmmbr);
  rtc::scoped_ptr<rtcp::RawPacket> packet(sr.Build());
  EXPECT_EQ(0, InjectRtcpPacket(packet->Buffer(), packet->Length()));

  EXPECT_EQ(1, rtcp_receiver_->TMMBRReceived(0, 0, nullptr));
  TMMBRSet candidate_set;
  candidate_set.VerifyAndAllocateSet(1);
  EXPECT_EQ(1, rtcp_receiver_->TMMBRReceived(1, 0, &candidate_set));
  EXPECT_LT(0U, candidate_set.Tmmbr(0));
  EXPECT_EQ(kSenderSsrc, candidate_set.Ssrc(0));
}

TEST_F(RtcpReceiverTest, TmmbrPacketNotForUsIgnored) {
  const uint32_t kMediaFlowSsrc = 0x2040608;
  const uint32_t kSenderSsrc = 0x10203;

  rtcp::Tmmbr tmmbr;
  tmmbr.From(kSenderSsrc);
  tmmbr.To(kMediaFlowSsrc + 1);  // This SSRC is not what we are sending.
  tmmbr.WithBitrateKbps(30);

  rtcp::SenderReport sr;
  sr.From(kSenderSsrc);
  sr.Append(&tmmbr);
  rtc::scoped_ptr<rtcp::RawPacket> packet(sr.Build());

  std::set<uint32_t> ssrcs;
  ssrcs.insert(kMediaFlowSsrc);
  rtcp_receiver_->SetSsrcs(kMediaFlowSsrc, ssrcs);
  EXPECT_EQ(0, InjectRtcpPacket(packet->Buffer(), packet->Length()));
  EXPECT_EQ(0, rtcp_receiver_->TMMBRReceived(0, 0, nullptr));
}

TEST_F(RtcpReceiverTest, TmmbrPacketZeroRateIgnored) {
  const uint32_t kMediaFlowSsrc = 0x2040608;
  const uint32_t kSenderSsrc = 0x10203;
  std::set<uint32_t> ssrcs;
  ssrcs.insert(kMediaFlowSsrc);  // Matches "media source" above.
  rtcp_receiver_->SetSsrcs(kMediaFlowSsrc, ssrcs);

  rtcp::Tmmbr tmmbr;
  tmmbr.From(kSenderSsrc);
  tmmbr.To(kMediaFlowSsrc);
  tmmbr.WithBitrateKbps(0);

  rtcp::SenderReport sr;
  sr.From(kSenderSsrc);
  sr.Append(&tmmbr);
  rtc::scoped_ptr<rtcp::RawPacket> packet(sr.Build());

  EXPECT_EQ(0, InjectRtcpPacket(packet->Buffer(), packet->Length()));
  EXPECT_EQ(0, rtcp_receiver_->TMMBRReceived(0, 0, nullptr));
}

TEST_F(RtcpReceiverTest, TmmbrThreeConstraintsTimeOut) {
  const uint32_t kMediaFlowSsrc = 0x2040608;
  const uint32_t kSenderSsrc = 0x10203;
  std::set<uint32_t> ssrcs;
  ssrcs.insert(kMediaFlowSsrc);  // Matches "media source" above.
  rtcp_receiver_->SetSsrcs(kMediaFlowSsrc, ssrcs);

  // Inject 3 packets "from" kSenderSsrc, kSenderSsrc+1, kSenderSsrc+2.
  // The times of arrival are starttime + 0, starttime + 5 and starttime + 10.
  for (uint32_t ssrc = kSenderSsrc; ssrc < kSenderSsrc + 3; ++ssrc) {
    rtcp::Tmmbr tmmbr;
    tmmbr.From(ssrc);
    tmmbr.To(kMediaFlowSsrc);
    tmmbr.WithBitrateKbps(30);

    rtcp::SenderReport sr;
    sr.From(ssrc);
    sr.Append(&tmmbr);
    rtc::scoped_ptr<rtcp::RawPacket> packet(sr.Build());
    EXPECT_EQ(0, InjectRtcpPacket(packet->Buffer(), packet->Length()));
    // 5 seconds between each packet.
    system_clock_.AdvanceTimeMilliseconds(5000);
  }
  // It is now starttime + 15.
  EXPECT_EQ(3, rtcp_receiver_->TMMBRReceived(0, 0, nullptr));
  TMMBRSet candidate_set;
  candidate_set.VerifyAndAllocateSet(3);
  EXPECT_EQ(3, rtcp_receiver_->TMMBRReceived(3, 0, &candidate_set));
  EXPECT_LT(0U, candidate_set.Tmmbr(0));
  // We expect the timeout to be 25 seconds. Advance the clock by 12
  // seconds, timing out the first packet.
  system_clock_.AdvanceTimeMilliseconds(12000);
  // Odd behaviour: Just counting them does not trigger the timeout.
  EXPECT_EQ(3, rtcp_receiver_->TMMBRReceived(0, 0, nullptr));
  EXPECT_EQ(2, rtcp_receiver_->TMMBRReceived(3, 0, &candidate_set));
  EXPECT_EQ(kSenderSsrc + 1, candidate_set.Ssrc(0));
}

TEST_F(RtcpReceiverTest, Callbacks) {
  class RtcpCallbackImpl : public RtcpStatisticsCallback {
   public:
    RtcpCallbackImpl() : RtcpStatisticsCallback(), ssrc_(0) {}
    virtual ~RtcpCallbackImpl() {}

    void StatisticsUpdated(const RtcpStatistics& statistics,
                           uint32_t ssrc) override {
      stats_ = statistics;
      ssrc_ = ssrc;
    }

    void CNameChanged(const char* cname, uint32_t ssrc) override {}

    bool Matches(uint32_t ssrc, uint32_t extended_max, uint8_t fraction_loss,
                 uint32_t cumulative_loss, uint32_t jitter) {
      return ssrc_ == ssrc &&
          stats_.fraction_lost == fraction_loss &&
          stats_.cumulative_lost == cumulative_loss &&
          stats_.extended_max_sequence_number == extended_max &&
          stats_.jitter == jitter;
    }

    RtcpStatistics stats_;
    uint32_t ssrc_;
  } callback;

  rtcp_receiver_->RegisterRtcpStatisticsCallback(&callback);

  const uint32_t kSenderSsrc = 0x10203;
  const uint32_t kSourceSsrc = 0x123456;
  const uint8_t kFractionLoss = 3;
  const uint32_t kCumulativeLoss = 7;
  const uint32_t kJitter = 9;
  const uint16_t kSequenceNumber = 1234;

  std::set<uint32_t> ssrcs;
  ssrcs.insert(kSourceSsrc);
  rtcp_receiver_->SetSsrcs(kSourceSsrc, ssrcs);

  // First packet, all numbers should just propagate.
  rtcp::ReportBlock rb1;
  rb1.To(kSourceSsrc);
  rb1.WithExtHighestSeqNum(kSequenceNumber);
  rb1.WithFractionLost(kFractionLoss);
  rb1.WithCumulativeLost(kCumulativeLoss);
  rb1.WithJitter(kJitter);

  rtcp::ReceiverReport rr1;
  rr1.From(kSenderSsrc);
  rr1.WithReportBlock(rb1);
  rtc::scoped_ptr<rtcp::RawPacket> p1(rr1.Build());
  EXPECT_EQ(0, InjectRtcpPacket(p1->Buffer(), p1->Length()));
  EXPECT_TRUE(callback.Matches(kSourceSsrc, kSequenceNumber, kFractionLoss,
                               kCumulativeLoss, kJitter));

  rtcp_receiver_->RegisterRtcpStatisticsCallback(nullptr);

  // Add arbitrary numbers, callback should not be called (retain old values).
  rtcp::ReportBlock rb2;
  rb2.To(kSourceSsrc);
  rb2.WithExtHighestSeqNum(kSequenceNumber + 1);
  rb2.WithFractionLost(42);
  rb2.WithCumulativeLost(137);
  rb2.WithJitter(4711);

  rtcp::ReceiverReport rr2;
  rr2.From(kSenderSsrc);
  rr2.WithReportBlock(rb2);
  rtc::scoped_ptr<rtcp::RawPacket> p2(rr2.Build());
  EXPECT_EQ(0, InjectRtcpPacket(p2->Buffer(), p2->Length()));
  EXPECT_TRUE(callback.Matches(kSourceSsrc, kSequenceNumber, kFractionLoss,
                               kCumulativeLoss, kJitter));
}

TEST_F(RtcpReceiverTest, ReceivesTransportFeedback) {
  const uint32_t kSenderSsrc = 0x10203;
  const uint32_t kSourceSsrc = 0x123456;

  std::set<uint32_t> ssrcs;
  ssrcs.insert(kSourceSsrc);
  rtcp_receiver_->SetSsrcs(kSourceSsrc, ssrcs);

  rtcp::TransportFeedback packet;
  packet.WithMediaSourceSsrc(kSourceSsrc);
  packet.WithPacketSenderSsrc(kSenderSsrc);
  packet.WithBase(1, 1000);
  packet.WithReceivedPacket(1, 1000);

  rtc::scoped_ptr<rtcp::RawPacket> built_packet = packet.Build();
  ASSERT_TRUE(built_packet.get() != nullptr);

  EXPECT_EQ(0,
            InjectRtcpPacket(built_packet->Buffer(), built_packet->Length()));

  EXPECT_NE(0u, rtcp_packet_info_.rtcpPacketTypeFlags & kRtcpTransportFeedback);
  EXPECT_TRUE(rtcp_packet_info_.transport_feedback_.get() != nullptr);
}

TEST_F(RtcpReceiverTest, HandlesInvalidTransportFeedback) {
  const uint32_t kSenderSsrc = 0x10203;
  const uint32_t kSourceSsrc = 0x123456;

  std::set<uint32_t> ssrcs;
  ssrcs.insert(kSourceSsrc);
  rtcp_receiver_->SetSsrcs(kSourceSsrc, ssrcs);

  // Send a compound packet with a TransportFeedback followed by something else.
  rtcp::TransportFeedback packet;
  packet.WithMediaSourceSsrc(kSourceSsrc);
  packet.WithPacketSenderSsrc(kSenderSsrc);
  packet.WithBase(1, 1000);
  packet.WithReceivedPacket(1, 1000);

  static uint32_t kBitrateBps = 50000;
  rtcp::Remb remb;
  remb.From(kSourceSsrc);
  remb.WithBitrateBps(kBitrateBps);
  packet.Append(&remb);

  rtc::scoped_ptr<rtcp::RawPacket> built_packet = packet.Build();
  ASSERT_TRUE(built_packet.get() != nullptr);

  // Modify the TransportFeedback packet so that it is invalid.
  const size_t kStatusCountOffset = 14;
  ByteWriter<uint16_t>::WriteBigEndian(
      &built_packet->MutableBuffer()[kStatusCountOffset], 42);

  EXPECT_EQ(0,
            InjectRtcpPacket(built_packet->Buffer(), built_packet->Length()));

  // Transport feedback should be ignored, but next packet should work.
  EXPECT_EQ(0u, rtcp_packet_info_.rtcpPacketTypeFlags & kRtcpTransportFeedback);
  EXPECT_NE(0u, rtcp_packet_info_.rtcpPacketTypeFlags & kRtcpRemb);
  EXPECT_EQ(kBitrateBps, rtcp_packet_info_.receiverEstimatedMaxBitrate);
}

}  // Anonymous namespace

}  // namespace webrtc
