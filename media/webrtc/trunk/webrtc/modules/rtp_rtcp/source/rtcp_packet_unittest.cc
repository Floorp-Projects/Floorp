/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 *
 * This file includes unit tests for the RtcpPacket.
 */

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#include "webrtc/modules/rtp_rtcp/source/rtcp_packet.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/app.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/bye.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/receiver_report.h"
#include "webrtc/test/rtcp_packet_parser.h"

using ::testing::ElementsAre;

using webrtc::rtcp::App;
using webrtc::rtcp::Bye;
using webrtc::rtcp::Dlrr;
using webrtc::rtcp::Fir;
using webrtc::rtcp::RawPacket;
using webrtc::rtcp::ReceiverReport;
using webrtc::rtcp::Remb;
using webrtc::rtcp::ReportBlock;
using webrtc::rtcp::Rpsi;
using webrtc::rtcp::Rrtr;
using webrtc::rtcp::Sdes;
using webrtc::rtcp::SenderReport;
using webrtc::rtcp::VoipMetric;
using webrtc::rtcp::Xr;
using webrtc::test::RtcpPacketParser;

namespace webrtc {

const uint32_t kSenderSsrc = 0x12345678;
const uint32_t kRemoteSsrc = 0x23456789;

TEST(RtcpPacketTest, Sr) {
  SenderReport sr;
  sr.From(kSenderSsrc);
  sr.WithNtpSec(0x11111111);
  sr.WithNtpFrac(0x22222222);
  sr.WithRtpTimestamp(0x33333333);
  sr.WithPacketCount(0x44444444);
  sr.WithOctetCount(0x55555555);

  rtc::scoped_ptr<RawPacket> packet(sr.Build());
  RtcpPacketParser parser;
  parser.Parse(packet->Buffer(), packet->Length());

  EXPECT_EQ(1, parser.sender_report()->num_packets());
  EXPECT_EQ(kSenderSsrc, parser.sender_report()->Ssrc());
  EXPECT_EQ(0x11111111U, parser.sender_report()->NtpSec());
  EXPECT_EQ(0x22222222U, parser.sender_report()->NtpFrac());
  EXPECT_EQ(0x33333333U, parser.sender_report()->RtpTimestamp());
  EXPECT_EQ(0x44444444U, parser.sender_report()->PacketCount());
  EXPECT_EQ(0x55555555U, parser.sender_report()->OctetCount());
  EXPECT_EQ(0, parser.report_block()->num_packets());
}

TEST(RtcpPacketTest, SrWithOneReportBlock) {
  ReportBlock rb;
  rb.To(kRemoteSsrc);

  SenderReport sr;
  sr.From(kSenderSsrc);
  EXPECT_TRUE(sr.WithReportBlock(rb));

  rtc::scoped_ptr<RawPacket> packet(sr.Build());
  RtcpPacketParser parser;
  parser.Parse(packet->Buffer(), packet->Length());
  EXPECT_EQ(1, parser.sender_report()->num_packets());
  EXPECT_EQ(kSenderSsrc, parser.sender_report()->Ssrc());
  EXPECT_EQ(1, parser.report_block()->num_packets());
  EXPECT_EQ(kRemoteSsrc, parser.report_block()->Ssrc());
}

TEST(RtcpPacketTest, SrWithTwoReportBlocks) {
  ReportBlock rb1;
  rb1.To(kRemoteSsrc);
  ReportBlock rb2;
  rb2.To(kRemoteSsrc + 1);

  SenderReport sr;
  sr.From(kSenderSsrc);
  EXPECT_TRUE(sr.WithReportBlock(rb1));
  EXPECT_TRUE(sr.WithReportBlock(rb2));

  rtc::scoped_ptr<RawPacket> packet(sr.Build());
  RtcpPacketParser parser;
  parser.Parse(packet->Buffer(), packet->Length());
  EXPECT_EQ(1, parser.sender_report()->num_packets());
  EXPECT_EQ(kSenderSsrc, parser.sender_report()->Ssrc());
  EXPECT_EQ(2, parser.report_block()->num_packets());
  EXPECT_EQ(1, parser.report_blocks_per_ssrc(kRemoteSsrc));
  EXPECT_EQ(1, parser.report_blocks_per_ssrc(kRemoteSsrc + 1));
}

TEST(RtcpPacketTest, SrWithTooManyReportBlocks) {
  SenderReport sr;
  sr.From(kSenderSsrc);
  const int kMaxReportBlocks = (1 << 5) - 1;
  ReportBlock rb;
  for (int i = 0; i < kMaxReportBlocks; ++i) {
    rb.To(kRemoteSsrc + i);
    EXPECT_TRUE(sr.WithReportBlock(rb));
  }
  rb.To(kRemoteSsrc + kMaxReportBlocks);
  EXPECT_FALSE(sr.WithReportBlock(rb));
}

TEST(RtcpPacketTest, AppWithNoData) {
  App app;
  app.WithSubType(30);
  uint32_t name = 'n' << 24;
  name += 'a' << 16;
  name += 'm' << 8;
  name += 'e';
  app.WithName(name);

  rtc::scoped_ptr<RawPacket> packet(app.Build());
  RtcpPacketParser parser;
  parser.Parse(packet->Buffer(), packet->Length());
  EXPECT_EQ(1, parser.app()->num_packets());
  EXPECT_EQ(30U, parser.app()->SubType());
  EXPECT_EQ(name, parser.app()->Name());
  EXPECT_EQ(0, parser.app_item()->num_packets());
}

TEST(RtcpPacketTest, App) {
  App app;
  app.From(kSenderSsrc);
  app.WithSubType(30);
  uint32_t name = 'n' << 24;
  name += 'a' << 16;
  name += 'm' << 8;
  name += 'e';
  app.WithName(name);
  const char kData[] = {'t', 'e', 's', 't', 'd', 'a', 't', 'a'};
  const size_t kDataLength = sizeof(kData) / sizeof(kData[0]);
  app.WithData((const uint8_t*)kData, kDataLength);

  rtc::scoped_ptr<RawPacket> packet(app.Build());
  RtcpPacketParser parser;
  parser.Parse(packet->Buffer(), packet->Length());
  EXPECT_EQ(1, parser.app()->num_packets());
  EXPECT_EQ(30U, parser.app()->SubType());
  EXPECT_EQ(name, parser.app()->Name());
  EXPECT_EQ(1, parser.app_item()->num_packets());
  EXPECT_EQ(kDataLength, parser.app_item()->DataLength());
  EXPECT_EQ(0, strncmp(kData, (const char*)parser.app_item()->Data(),
      parser.app_item()->DataLength()));
}

TEST(RtcpPacketTest, SdesWithOneChunk) {
  Sdes sdes;
  EXPECT_TRUE(sdes.WithCName(kSenderSsrc, "alice@host"));

  rtc::scoped_ptr<RawPacket> packet(sdes.Build());
  RtcpPacketParser parser;
  parser.Parse(packet->Buffer(), packet->Length());
  EXPECT_EQ(1, parser.sdes()->num_packets());
  EXPECT_EQ(1, parser.sdes_chunk()->num_packets());
  EXPECT_EQ(kSenderSsrc, parser.sdes_chunk()->Ssrc());
  EXPECT_EQ("alice@host", parser.sdes_chunk()->Cname());
}

TEST(RtcpPacketTest, SdesWithMultipleChunks) {
  Sdes sdes;
  EXPECT_TRUE(sdes.WithCName(kSenderSsrc, "a"));
  EXPECT_TRUE(sdes.WithCName(kSenderSsrc + 1, "ab"));
  EXPECT_TRUE(sdes.WithCName(kSenderSsrc + 2, "abc"));
  EXPECT_TRUE(sdes.WithCName(kSenderSsrc + 3, "abcd"));
  EXPECT_TRUE(sdes.WithCName(kSenderSsrc + 4, "abcde"));
  EXPECT_TRUE(sdes.WithCName(kSenderSsrc + 5, "abcdef"));

  rtc::scoped_ptr<RawPacket> packet(sdes.Build());
  RtcpPacketParser parser;
  parser.Parse(packet->Buffer(), packet->Length());
  EXPECT_EQ(1, parser.sdes()->num_packets());
  EXPECT_EQ(6, parser.sdes_chunk()->num_packets());
  EXPECT_EQ(kSenderSsrc + 5, parser.sdes_chunk()->Ssrc());
  EXPECT_EQ("abcdef", parser.sdes_chunk()->Cname());
}

TEST(RtcpPacketTest, SdesWithTooManyChunks) {
  Sdes sdes;
  const int kMaxChunks = (1 << 5) - 1;
  for (int i = 0; i < kMaxChunks; ++i) {
    uint32_t ssrc = kSenderSsrc + i;
    std::ostringstream oss;
    oss << "cname" << i;
    EXPECT_TRUE(sdes.WithCName(ssrc, oss.str()));
  }
  EXPECT_FALSE(sdes.WithCName(kSenderSsrc + kMaxChunks, "foo"));
}

TEST(RtcpPacketTest, CnameItemWithEmptyString) {
  Sdes sdes;
  EXPECT_TRUE(sdes.WithCName(kSenderSsrc, ""));

  rtc::scoped_ptr<RawPacket> packet(sdes.Build());
  RtcpPacketParser parser;
  parser.Parse(packet->Buffer(), packet->Length());
  EXPECT_EQ(1, parser.sdes()->num_packets());
  EXPECT_EQ(1, parser.sdes_chunk()->num_packets());
  EXPECT_EQ(kSenderSsrc, parser.sdes_chunk()->Ssrc());
  EXPECT_EQ("", parser.sdes_chunk()->Cname());
}

TEST(RtcpPacketTest, Rpsi) {
  Rpsi rpsi;
  // 1000001 (7 bits = 1 byte in native string).
  const uint64_t kPictureId = 0x41;
  const uint16_t kNumberOfValidBytes = 1;
  rpsi.WithPayloadType(100);
  rpsi.WithPictureId(kPictureId);

  rtc::scoped_ptr<RawPacket> packet(rpsi.Build());
  RtcpPacketParser parser;
  parser.Parse(packet->Buffer(), packet->Length());
  EXPECT_EQ(100, parser.rpsi()->PayloadType());
  EXPECT_EQ(kNumberOfValidBytes * 8, parser.rpsi()->NumberOfValidBits());
  EXPECT_EQ(kPictureId, parser.rpsi()->PictureId());
}

TEST(RtcpPacketTest, RpsiWithTwoByteNativeString) {
  Rpsi rpsi;
  // |1 0000001 (7 bits = 1 byte in native string).
  const uint64_t kPictureId = 0x81;
  const uint16_t kNumberOfValidBytes = 2;
  rpsi.WithPictureId(kPictureId);

  rtc::scoped_ptr<RawPacket> packet(rpsi.Build());
  RtcpPacketParser parser;
  parser.Parse(packet->Buffer(), packet->Length());
  EXPECT_EQ(kNumberOfValidBytes * 8, parser.rpsi()->NumberOfValidBits());
  EXPECT_EQ(kPictureId, parser.rpsi()->PictureId());
}

TEST(RtcpPacketTest, RpsiWithThreeByteNativeString) {
  Rpsi rpsi;
  // 10000|00 100000|0 1000000 (7 bits = 1 byte in native string).
  const uint64_t kPictureId = 0x102040;
  const uint16_t kNumberOfValidBytes = 3;
  rpsi.WithPictureId(kPictureId);

  rtc::scoped_ptr<RawPacket> packet(rpsi.Build());
  RtcpPacketParser parser;
  parser.Parse(packet->Buffer(), packet->Length());
  EXPECT_EQ(kNumberOfValidBytes * 8, parser.rpsi()->NumberOfValidBits());
  EXPECT_EQ(kPictureId, parser.rpsi()->PictureId());
}

TEST(RtcpPacketTest, RpsiWithFourByteNativeString) {
  Rpsi rpsi;
  // 1000|001 00001|01 100001|1 1000010 (7 bits = 1 byte in native string).
  const uint64_t kPictureId = 0x84161C2;
  const uint16_t kNumberOfValidBytes = 4;
  rpsi.WithPictureId(kPictureId);

  rtc::scoped_ptr<RawPacket> packet(rpsi.Build());
  RtcpPacketParser parser;
  parser.Parse(packet->Buffer(), packet->Length());
  EXPECT_EQ(kNumberOfValidBytes * 8, parser.rpsi()->NumberOfValidBits());
  EXPECT_EQ(kPictureId, parser.rpsi()->PictureId());
}

TEST(RtcpPacketTest, RpsiWithMaxPictureId) {
  Rpsi rpsi;
  // 1 1111111| 1111111 1|111111 11|11111 111|1111 1111|111 11111|
  // 11 111111|1 1111111 (7 bits = 1 byte in native string).
  const uint64_t kPictureId = 0xffffffffffffffff;
  const uint16_t kNumberOfValidBytes = 10;
  rpsi.WithPictureId(kPictureId);

  rtc::scoped_ptr<RawPacket> packet(rpsi.Build());
  RtcpPacketParser parser;
  parser.Parse(packet->Buffer(), packet->Length());
  EXPECT_EQ(kNumberOfValidBytes * 8, parser.rpsi()->NumberOfValidBits());
  EXPECT_EQ(kPictureId, parser.rpsi()->PictureId());
}

TEST(RtcpPacketTest, Fir) {
  Fir fir;
  fir.From(kSenderSsrc);
  fir.To(kRemoteSsrc);
  fir.WithCommandSeqNum(123);

  rtc::scoped_ptr<RawPacket> packet(fir.Build());
  RtcpPacketParser parser;
  parser.Parse(packet->Buffer(), packet->Length());
  EXPECT_EQ(1, parser.fir()->num_packets());
  EXPECT_EQ(kSenderSsrc, parser.fir()->Ssrc());
  EXPECT_EQ(1, parser.fir_item()->num_packets());
  EXPECT_EQ(kRemoteSsrc, parser.fir_item()->Ssrc());
  EXPECT_EQ(123U, parser.fir_item()->SeqNum());
}

TEST(RtcpPacketTest, BuildWithTooSmallBuffer) {
  ReportBlock rb;
  ReceiverReport rr;
  rr.From(kSenderSsrc);
  EXPECT_TRUE(rr.WithReportBlock(rb));

  const size_t kRrLength = 8;
  const size_t kReportBlockLength = 24;

  // No packet.
  class Verifier : public rtcp::RtcpPacket::PacketReadyCallback {
    void OnPacketReady(uint8_t* data, size_t length) override {
      ADD_FAILURE() << "Packet should not fit within max size.";
    }
  } verifier;
  const size_t kBufferSize = kRrLength + kReportBlockLength - 1;
  uint8_t buffer[kBufferSize];
  EXPECT_FALSE(rr.BuildExternalBuffer(buffer, kBufferSize, &verifier));
}

TEST(RtcpPacketTest, Remb) {
  Remb remb;
  remb.From(kSenderSsrc);
  remb.AppliesTo(kRemoteSsrc);
  remb.AppliesTo(kRemoteSsrc + 1);
  remb.AppliesTo(kRemoteSsrc + 2);
  remb.WithBitrateBps(261011);

  rtc::scoped_ptr<RawPacket> packet(remb.Build());
  RtcpPacketParser parser;
  parser.Parse(packet->Buffer(), packet->Length());
  EXPECT_EQ(1, parser.psfb_app()->num_packets());
  EXPECT_EQ(kSenderSsrc, parser.psfb_app()->Ssrc());
  EXPECT_EQ(1, parser.remb_item()->num_packets());
  EXPECT_EQ(261011, parser.remb_item()->last_bitrate_bps());
  std::vector<uint32_t> ssrcs = parser.remb_item()->last_ssrc_list();
  EXPECT_EQ(kRemoteSsrc, ssrcs[0]);
  EXPECT_EQ(kRemoteSsrc + 1, ssrcs[1]);
  EXPECT_EQ(kRemoteSsrc + 2, ssrcs[2]);
}

TEST(RtcpPacketTest, XrWithNoReportBlocks) {
  Xr xr;
  xr.From(kSenderSsrc);

  rtc::scoped_ptr<RawPacket> packet(xr.Build());
  RtcpPacketParser parser;
  parser.Parse(packet->Buffer(), packet->Length());
  EXPECT_EQ(1, parser.xr_header()->num_packets());
  EXPECT_EQ(kSenderSsrc, parser.xr_header()->Ssrc());
}

TEST(RtcpPacketTest, XrWithRrtr) {
  Rrtr rrtr;
  rrtr.WithNtp(NtpTime(0x11111111, 0x22222222));
  Xr xr;
  xr.From(kSenderSsrc);
  EXPECT_TRUE(xr.WithRrtr(&rrtr));

  rtc::scoped_ptr<RawPacket> packet(xr.Build());
  RtcpPacketParser parser;
  parser.Parse(packet->Buffer(), packet->Length());
  EXPECT_EQ(1, parser.xr_header()->num_packets());
  EXPECT_EQ(kSenderSsrc, parser.xr_header()->Ssrc());
  EXPECT_EQ(1, parser.rrtr()->num_packets());
  EXPECT_EQ(0x11111111U, parser.rrtr()->NtpSec());
  EXPECT_EQ(0x22222222U, parser.rrtr()->NtpFrac());
}

TEST(RtcpPacketTest, XrWithTwoRrtrBlocks) {
  Rrtr rrtr1;
  rrtr1.WithNtp(NtpTime(0x11111111, 0x22222222));
  Rrtr rrtr2;
  rrtr2.WithNtp(NtpTime(0x33333333, 0x44444444));
  Xr xr;
  xr.From(kSenderSsrc);
  EXPECT_TRUE(xr.WithRrtr(&rrtr1));
  EXPECT_TRUE(xr.WithRrtr(&rrtr2));

  rtc::scoped_ptr<RawPacket> packet(xr.Build());
  RtcpPacketParser parser;
  parser.Parse(packet->Buffer(), packet->Length());
  EXPECT_EQ(1, parser.xr_header()->num_packets());
  EXPECT_EQ(kSenderSsrc, parser.xr_header()->Ssrc());
  EXPECT_EQ(2, parser.rrtr()->num_packets());
  EXPECT_EQ(0x33333333U, parser.rrtr()->NtpSec());
  EXPECT_EQ(0x44444444U, parser.rrtr()->NtpFrac());
}

TEST(RtcpPacketTest, XrWithDlrrWithOneSubBlock) {
  Dlrr dlrr;
  EXPECT_TRUE(dlrr.WithDlrrItem(0x11111111, 0x22222222, 0x33333333));
  Xr xr;
  xr.From(kSenderSsrc);
  EXPECT_TRUE(xr.WithDlrr(&dlrr));

  rtc::scoped_ptr<RawPacket> packet(xr.Build());
  RtcpPacketParser parser;
  parser.Parse(packet->Buffer(), packet->Length());
  EXPECT_EQ(1, parser.xr_header()->num_packets());
  EXPECT_EQ(kSenderSsrc, parser.xr_header()->Ssrc());
  EXPECT_EQ(1, parser.dlrr()->num_packets());
  EXPECT_EQ(1, parser.dlrr_items()->num_packets());
  EXPECT_EQ(0x11111111U, parser.dlrr_items()->Ssrc(0));
  EXPECT_EQ(0x22222222U, parser.dlrr_items()->LastRr(0));
  EXPECT_EQ(0x33333333U, parser.dlrr_items()->DelayLastRr(0));
}

TEST(RtcpPacketTest, XrWithDlrrWithTwoSubBlocks) {
  Dlrr dlrr;
  EXPECT_TRUE(dlrr.WithDlrrItem(0x11111111, 0x22222222, 0x33333333));
  EXPECT_TRUE(dlrr.WithDlrrItem(0x44444444, 0x55555555, 0x66666666));
  Xr xr;
  xr.From(kSenderSsrc);
  EXPECT_TRUE(xr.WithDlrr(&dlrr));

  rtc::scoped_ptr<RawPacket> packet(xr.Build());
  RtcpPacketParser parser;
  parser.Parse(packet->Buffer(), packet->Length());
  EXPECT_EQ(1, parser.xr_header()->num_packets());
  EXPECT_EQ(kSenderSsrc, parser.xr_header()->Ssrc());
  EXPECT_EQ(1, parser.dlrr()->num_packets());
  EXPECT_EQ(2, parser.dlrr_items()->num_packets());
  EXPECT_EQ(0x11111111U, parser.dlrr_items()->Ssrc(0));
  EXPECT_EQ(0x22222222U, parser.dlrr_items()->LastRr(0));
  EXPECT_EQ(0x33333333U, parser.dlrr_items()->DelayLastRr(0));
  EXPECT_EQ(0x44444444U, parser.dlrr_items()->Ssrc(1));
  EXPECT_EQ(0x55555555U, parser.dlrr_items()->LastRr(1));
  EXPECT_EQ(0x66666666U, parser.dlrr_items()->DelayLastRr(1));
}

TEST(RtcpPacketTest, DlrrWithTooManySubBlocks) {
  const int kMaxItems = 100;
  Dlrr dlrr;
  for (int i = 0; i < kMaxItems; ++i)
    EXPECT_TRUE(dlrr.WithDlrrItem(i, i, i));
  EXPECT_FALSE(dlrr.WithDlrrItem(kMaxItems, kMaxItems, kMaxItems));
}

TEST(RtcpPacketTest, XrWithTwoDlrrBlocks) {
  Dlrr dlrr1;
  EXPECT_TRUE(dlrr1.WithDlrrItem(0x11111111, 0x22222222, 0x33333333));
  Dlrr dlrr2;
  EXPECT_TRUE(dlrr2.WithDlrrItem(0x44444444, 0x55555555, 0x66666666));
  Xr xr;
  xr.From(kSenderSsrc);
  EXPECT_TRUE(xr.WithDlrr(&dlrr1));
  EXPECT_TRUE(xr.WithDlrr(&dlrr2));

  rtc::scoped_ptr<RawPacket> packet(xr.Build());
  RtcpPacketParser parser;
  parser.Parse(packet->Buffer(), packet->Length());
  EXPECT_EQ(1, parser.xr_header()->num_packets());
  EXPECT_EQ(kSenderSsrc, parser.xr_header()->Ssrc());
  EXPECT_EQ(2, parser.dlrr()->num_packets());
  EXPECT_EQ(2, parser.dlrr_items()->num_packets());
  EXPECT_EQ(0x11111111U, parser.dlrr_items()->Ssrc(0));
  EXPECT_EQ(0x22222222U, parser.dlrr_items()->LastRr(0));
  EXPECT_EQ(0x33333333U, parser.dlrr_items()->DelayLastRr(0));
  EXPECT_EQ(0x44444444U, parser.dlrr_items()->Ssrc(1));
  EXPECT_EQ(0x55555555U, parser.dlrr_items()->LastRr(1));
  EXPECT_EQ(0x66666666U, parser.dlrr_items()->DelayLastRr(1));
}

TEST(RtcpPacketTest, XrWithVoipMetric) {
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
  VoipMetric metric_block;
  metric_block.To(kRemoteSsrc);
  metric_block.WithVoipMetric(metric);
  Xr xr;
  xr.From(kSenderSsrc);
  EXPECT_TRUE(xr.WithVoipMetric(&metric_block));

  rtc::scoped_ptr<RawPacket> packet(xr.Build());
  RtcpPacketParser parser;
  parser.Parse(packet->Buffer(), packet->Length());
  EXPECT_EQ(1, parser.xr_header()->num_packets());
  EXPECT_EQ(kSenderSsrc, parser.xr_header()->Ssrc());
  EXPECT_EQ(1, parser.voip_metric()->num_packets());
  EXPECT_EQ(kRemoteSsrc, parser.voip_metric()->Ssrc());
  EXPECT_EQ(1, parser.voip_metric()->LossRate());
  EXPECT_EQ(2, parser.voip_metric()->DiscardRate());
  EXPECT_EQ(3, parser.voip_metric()->BurstDensity());
  EXPECT_EQ(4, parser.voip_metric()->GapDensity());
  EXPECT_EQ(0x1111, parser.voip_metric()->BurstDuration());
  EXPECT_EQ(0x2222, parser.voip_metric()->GapDuration());
  EXPECT_EQ(0x3333, parser.voip_metric()->RoundTripDelay());
  EXPECT_EQ(0x4444, parser.voip_metric()->EndSystemDelay());
  EXPECT_EQ(5, parser.voip_metric()->SignalLevel());
  EXPECT_EQ(6, parser.voip_metric()->NoiseLevel());
  EXPECT_EQ(7, parser.voip_metric()->Rerl());
  EXPECT_EQ(8, parser.voip_metric()->Gmin());
  EXPECT_EQ(9, parser.voip_metric()->Rfactor());
  EXPECT_EQ(10, parser.voip_metric()->ExtRfactor());
  EXPECT_EQ(11, parser.voip_metric()->MosLq());
  EXPECT_EQ(12, parser.voip_metric()->MosCq());
  EXPECT_EQ(13, parser.voip_metric()->RxConfig());
  EXPECT_EQ(0x5555, parser.voip_metric()->JbNominal());
  EXPECT_EQ(0x6666, parser.voip_metric()->JbMax());
  EXPECT_EQ(0x7777, parser.voip_metric()->JbAbsMax());
}

TEST(RtcpPacketTest, XrWithMultipleReportBlocks) {
  Rrtr rrtr;
  Dlrr dlrr;
  EXPECT_TRUE(dlrr.WithDlrrItem(1, 2, 3));
  VoipMetric metric;
  Xr xr;
  xr.From(kSenderSsrc);
  EXPECT_TRUE(xr.WithRrtr(&rrtr));
  EXPECT_TRUE(xr.WithDlrr(&dlrr));
  EXPECT_TRUE(xr.WithVoipMetric(&metric));

  rtc::scoped_ptr<RawPacket> packet(xr.Build());
  RtcpPacketParser parser;
  parser.Parse(packet->Buffer(), packet->Length());
  EXPECT_EQ(1, parser.xr_header()->num_packets());
  EXPECT_EQ(kSenderSsrc, parser.xr_header()->Ssrc());
  EXPECT_EQ(1, parser.rrtr()->num_packets());
  EXPECT_EQ(1, parser.dlrr()->num_packets());
  EXPECT_EQ(1, parser.dlrr_items()->num_packets());
  EXPECT_EQ(1, parser.voip_metric()->num_packets());
}

TEST(RtcpPacketTest, DlrrWithoutItemNotIncludedInPacket) {
  Rrtr rrtr;
  Dlrr dlrr;
  VoipMetric metric;
  Xr xr;
  xr.From(kSenderSsrc);
  EXPECT_TRUE(xr.WithRrtr(&rrtr));
  EXPECT_TRUE(xr.WithDlrr(&dlrr));
  EXPECT_TRUE(xr.WithVoipMetric(&metric));

  rtc::scoped_ptr<RawPacket> packet(xr.Build());
  RtcpPacketParser parser;
  parser.Parse(packet->Buffer(), packet->Length());
  EXPECT_EQ(1, parser.xr_header()->num_packets());
  EXPECT_EQ(kSenderSsrc, parser.xr_header()->Ssrc());
  EXPECT_EQ(1, parser.rrtr()->num_packets());
  EXPECT_EQ(0, parser.dlrr()->num_packets());
  EXPECT_EQ(1, parser.voip_metric()->num_packets());
}

TEST(RtcpPacketTest, XrWithTooManyBlocks) {
  const int kMaxBlocks = 50;
  Xr xr;

  Rrtr rrtr;
  for (int i = 0; i < kMaxBlocks; ++i)
    EXPECT_TRUE(xr.WithRrtr(&rrtr));
  EXPECT_FALSE(xr.WithRrtr(&rrtr));

  Dlrr dlrr;
  for (int i = 0; i < kMaxBlocks; ++i)
    EXPECT_TRUE(xr.WithDlrr(&dlrr));
  EXPECT_FALSE(xr.WithDlrr(&dlrr));

  VoipMetric voip_metric;
  for (int i = 0; i < kMaxBlocks; ++i)
    EXPECT_TRUE(xr.WithVoipMetric(&voip_metric));
  EXPECT_FALSE(xr.WithVoipMetric(&voip_metric));
}

}  // namespace webrtc
