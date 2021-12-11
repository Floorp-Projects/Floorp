/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/rtcp_transceiver_impl.h"

#include <vector>

#include "modules/rtp_rtcp/include/receive_statistics.h"
#include "modules/rtp_rtcp/source/time_util.h"
#include "rtc_base/event.h"
#include "rtc_base/fakeclock.h"
#include "rtc_base/ptr_util.h"
#include "rtc_base/task_queue.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/mock_transport.h"
#include "test/rtcp_packet_parser.h"

namespace {

using ::testing::_;
using ::testing::ElementsAre;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::SizeIs;
using ::webrtc::CompactNtp;
using ::webrtc::CompactNtpRttToMs;
using ::webrtc::MockTransport;
using ::webrtc::NtpTime;
using ::webrtc::RtcpTransceiverConfig;
using ::webrtc::RtcpTransceiverImpl;
using ::webrtc::rtcp::ReportBlock;
using ::webrtc::rtcp::SenderReport;
using ::webrtc::test::RtcpPacketParser;

class MockReceiveStatisticsProvider : public webrtc::ReceiveStatisticsProvider {
 public:
  MOCK_METHOD1(RtcpReportBlocks, std::vector<ReportBlock>(size_t));
};

// Since some tests will need to wait for this period, make it small to avoid
// slowing tests too much. As long as there are test bots with high scheduler
// granularity, small period should be ok.
constexpr int kReportPeriodMs = 10;
// On some systems task queue might be slow, instead of guessing right
// grace period, use very large timeout, 100x larger expected wait time.
// Use finite timeout to fail tests rather than hang them.
constexpr int kAlmostForeverMs = 1000;

// Helper to wait for an rtcp packet produced on a different thread/task queue.
class FakeRtcpTransport : public webrtc::Transport {
 public:
  FakeRtcpTransport() : sent_rtcp_(false, false) {}
  bool SendRtcp(const uint8_t* data, size_t size) override {
    sent_rtcp_.Set();
    return true;
  }
  bool SendRtp(const uint8_t*, size_t, const webrtc::PacketOptions&) override {
    ADD_FAILURE() << "RtcpTransciver shouldn't send rtp packets.";
    return true;
  }

  // Returns true when packet was received by the transport.
  bool WaitPacket() {
    // Normally packet should be sent fast, long before the timeout.
    bool packet_sent = sent_rtcp_.Wait(kAlmostForeverMs);
    // Disallow tests to wait almost forever for no packets.
    EXPECT_TRUE(packet_sent);
    // Return wait result even though it is expected to be true, so that
    // individual tests can EXPECT on it for better error message.
    return packet_sent;
  }

 private:
  rtc::Event sent_rtcp_;
};

class RtcpParserTransport : public webrtc::Transport {
 public:
  explicit RtcpParserTransport(RtcpPacketParser* parser) : parser_(parser) {}
  // Returns total number of rtcp packet received.
  int num_packets() const { return num_packets_; }

 private:
  bool SendRtcp(const uint8_t* data, size_t size) override {
    ++num_packets_;
    parser_->Parse(data, size);
    return true;
  }

  bool SendRtp(const uint8_t*, size_t, const webrtc::PacketOptions&) override {
    ADD_FAILURE() << "RtcpTransciver shouldn't send rtp packets.";
    return true;
  }

  RtcpPacketParser* const parser_;
  int num_packets_ = 0;
};

TEST(RtcpTransceiverImplTest, DelaysSendingFirstCompondPacket) {
  rtc::TaskQueue queue("rtcp");
  FakeRtcpTransport transport;
  RtcpTransceiverConfig config;
  config.outgoing_transport = &transport;
  config.initial_report_delay_ms = 10;
  config.task_queue = &queue;
  rtc::Optional<RtcpTransceiverImpl> rtcp_transceiver;

  int64_t started_ms = rtc::TimeMillis();
  queue.PostTask([&] { rtcp_transceiver.emplace(config); });
  EXPECT_TRUE(transport.WaitPacket());

  EXPECT_GE(rtc::TimeMillis() - started_ms, config.initial_report_delay_ms);

  // Cleanup.
  rtc::Event done(false, false);
  queue.PostTask([&] {
    rtcp_transceiver.reset();
    done.Set();
  });
  ASSERT_TRUE(done.Wait(kAlmostForeverMs));
}

TEST(RtcpTransceiverImplTest, PeriodicallySendsPackets) {
  rtc::TaskQueue queue("rtcp");
  FakeRtcpTransport transport;
  RtcpTransceiverConfig config;
  config.outgoing_transport = &transport;
  config.initial_report_delay_ms = 0;
  config.report_period_ms = kReportPeriodMs;
  config.task_queue = &queue;
  rtc::Optional<RtcpTransceiverImpl> rtcp_transceiver;
  int64_t time_just_before_1st_packet_ms = 0;
  queue.PostTask([&] {
    // Because initial_report_delay_ms is set to 0, time_just_before_the_packet
    // should be very close to the time_of_the_packet.
    time_just_before_1st_packet_ms = rtc::TimeMillis();
    rtcp_transceiver.emplace(config);
  });

  EXPECT_TRUE(transport.WaitPacket());
  EXPECT_TRUE(transport.WaitPacket());
  int64_t time_just_after_2nd_packet_ms = rtc::TimeMillis();

  EXPECT_GE(time_just_after_2nd_packet_ms - time_just_before_1st_packet_ms,
            config.report_period_ms);

  // Cleanup.
  rtc::Event done(false, false);
  queue.PostTask([&] {
    rtcp_transceiver.reset();
    done.Set();
  });
  ASSERT_TRUE(done.Wait(kAlmostForeverMs));
}

TEST(RtcpTransceiverImplTest, SendCompoundPacketDelaysPeriodicSendPackets) {
  rtc::TaskQueue queue("rtcp");
  FakeRtcpTransport transport;
  RtcpTransceiverConfig config;
  config.outgoing_transport = &transport;
  config.initial_report_delay_ms = 0;
  config.report_period_ms = kReportPeriodMs;
  config.task_queue = &queue;
  rtc::Optional<RtcpTransceiverImpl> rtcp_transceiver;
  queue.PostTask([&] { rtcp_transceiver.emplace(config); });

  // Wait for first packet.
  EXPECT_TRUE(transport.WaitPacket());
  // Send non periodic one after half period.
  rtc::Event non_periodic(false, false);
  int64_t time_of_non_periodic_packet_ms = 0;
  queue.PostDelayedTask(
      [&] {
        time_of_non_periodic_packet_ms = rtc::TimeMillis();
        rtcp_transceiver->SendCompoundPacket();
        non_periodic.Set();
      },
      config.report_period_ms / 2);
  // Though non-periodic packet is scheduled just in between periodic, due to
  // small period and task queue flakiness it migth end-up 1ms after next
  // periodic packet. To be sure duration after non-periodic packet is tested
  // wait for transport after ensuring non-periodic packet was sent.
  EXPECT_TRUE(non_periodic.Wait(kAlmostForeverMs));
  EXPECT_TRUE(transport.WaitPacket());
  // Wait for next periodic packet.
  EXPECT_TRUE(transport.WaitPacket());
  int64_t time_of_last_periodic_packet_ms = rtc::TimeMillis();

  EXPECT_GE(time_of_last_periodic_packet_ms - time_of_non_periodic_packet_ms,
            config.report_period_ms);

  // Cleanup.
  rtc::Event done(false, false);
  queue.PostTask([&] {
    rtcp_transceiver.reset();
    done.Set();
  });
  ASSERT_TRUE(done.Wait(kAlmostForeverMs));
}

TEST(RtcpTransceiverImplTest, SendsMinimalCompoundPacket) {
  const uint32_t kSenderSsrc = 12345;
  RtcpTransceiverConfig config;
  config.feedback_ssrc = kSenderSsrc;
  config.cname = "cname";
  RtcpPacketParser rtcp_parser;
  RtcpParserTransport transport(&rtcp_parser);
  config.outgoing_transport = &transport;
  config.schedule_periodic_compound_packets = false;
  RtcpTransceiverImpl rtcp_transceiver(config);

  rtcp_transceiver.SendCompoundPacket();

  // Minimal compound RTCP packet contains sender or receiver report and sdes
  // with cname.
  ASSERT_GT(rtcp_parser.receiver_report()->num_packets(), 0);
  EXPECT_EQ(rtcp_parser.receiver_report()->sender_ssrc(), kSenderSsrc);
  ASSERT_GT(rtcp_parser.sdes()->num_packets(), 0);
  ASSERT_EQ(rtcp_parser.sdes()->chunks().size(), 1u);
  EXPECT_EQ(rtcp_parser.sdes()->chunks()[0].ssrc, kSenderSsrc);
  EXPECT_EQ(rtcp_parser.sdes()->chunks()[0].cname, config.cname);
}

TEST(RtcpTransceiverImplTest, SendsNoRembInitially) {
  const uint32_t kSenderSsrc = 12345;
  RtcpTransceiverConfig config;
  config.feedback_ssrc = kSenderSsrc;
  RtcpPacketParser rtcp_parser;
  RtcpParserTransport transport(&rtcp_parser);
  config.outgoing_transport = &transport;
  config.schedule_periodic_compound_packets = false;
  RtcpTransceiverImpl rtcp_transceiver(config);

  rtcp_transceiver.SendCompoundPacket();

  EXPECT_EQ(transport.num_packets(), 1);
  EXPECT_EQ(rtcp_parser.remb()->num_packets(), 0);
}

TEST(RtcpTransceiverImplTest, SetRembIncludesRembInNextCompoundPacket) {
  const uint32_t kSenderSsrc = 12345;
  RtcpTransceiverConfig config;
  config.feedback_ssrc = kSenderSsrc;
  RtcpPacketParser rtcp_parser;
  RtcpParserTransport transport(&rtcp_parser);
  config.outgoing_transport = &transport;
  config.schedule_periodic_compound_packets = false;
  RtcpTransceiverImpl rtcp_transceiver(config);

  rtcp_transceiver.SetRemb(/*bitrate_bps=*/10000, /*ssrc=*/{54321, 64321});
  rtcp_transceiver.SendCompoundPacket();

  EXPECT_EQ(rtcp_parser.remb()->num_packets(), 1);
  EXPECT_EQ(rtcp_parser.remb()->sender_ssrc(), kSenderSsrc);
  EXPECT_EQ(rtcp_parser.remb()->bitrate_bps(), 10000u);
  EXPECT_THAT(rtcp_parser.remb()->ssrcs(), ElementsAre(54321, 64321));
}

TEST(RtcpTransceiverImplTest, SetRembUpdatesValuesToSend) {
  const uint32_t kSenderSsrc = 12345;
  RtcpTransceiverConfig config;
  config.feedback_ssrc = kSenderSsrc;
  RtcpPacketParser rtcp_parser;
  RtcpParserTransport transport(&rtcp_parser);
  config.outgoing_transport = &transport;
  config.schedule_periodic_compound_packets = false;
  RtcpTransceiverImpl rtcp_transceiver(config);

  rtcp_transceiver.SetRemb(/*bitrate_bps=*/10000, /*ssrc=*/{54321, 64321});
  rtcp_transceiver.SendCompoundPacket();

  EXPECT_EQ(rtcp_parser.remb()->num_packets(), 1);
  EXPECT_EQ(rtcp_parser.remb()->bitrate_bps(), 10000u);
  EXPECT_THAT(rtcp_parser.remb()->ssrcs(), ElementsAre(54321, 64321));

  rtcp_transceiver.SetRemb(/*bitrate_bps=*/70000, /*ssrc=*/{67321});
  rtcp_transceiver.SendCompoundPacket();

  EXPECT_EQ(rtcp_parser.remb()->num_packets(), 2);
  EXPECT_EQ(rtcp_parser.remb()->bitrate_bps(), 70000u);
  EXPECT_THAT(rtcp_parser.remb()->ssrcs(), ElementsAre(67321));
}

TEST(RtcpTransceiverImplTest, SetRembIncludesRembInAllCompoundPackets) {
  const uint32_t kSenderSsrc = 12345;
  RtcpTransceiverConfig config;
  config.feedback_ssrc = kSenderSsrc;
  RtcpPacketParser rtcp_parser;
  RtcpParserTransport transport(&rtcp_parser);
  config.outgoing_transport = &transport;
  config.schedule_periodic_compound_packets = false;
  RtcpTransceiverImpl rtcp_transceiver(config);

  rtcp_transceiver.SetRemb(/*bitrate_bps=*/10000, /*ssrc=*/{54321, 64321});
  rtcp_transceiver.SendCompoundPacket();
  rtcp_transceiver.SendCompoundPacket();

  EXPECT_EQ(transport.num_packets(), 2);
  EXPECT_EQ(rtcp_parser.remb()->num_packets(), 2);
}

TEST(RtcpTransceiverImplTest, SendsNoRembAfterUnset) {
  const uint32_t kSenderSsrc = 12345;
  RtcpTransceiverConfig config;
  config.feedback_ssrc = kSenderSsrc;
  RtcpPacketParser rtcp_parser;
  RtcpParserTransport transport(&rtcp_parser);
  config.outgoing_transport = &transport;
  config.schedule_periodic_compound_packets = false;
  RtcpTransceiverImpl rtcp_transceiver(config);

  rtcp_transceiver.SetRemb(/*bitrate_bps=*/10000, /*ssrc=*/{54321, 64321});
  rtcp_transceiver.SendCompoundPacket();
  EXPECT_EQ(transport.num_packets(), 1);
  ASSERT_EQ(rtcp_parser.remb()->num_packets(), 1);

  rtcp_transceiver.UnsetRemb();
  rtcp_transceiver.SendCompoundPacket();

  EXPECT_EQ(transport.num_packets(), 2);
  EXPECT_EQ(rtcp_parser.remb()->num_packets(), 1);
}

TEST(RtcpTransceiverImplTest, ReceiverReportUsesReceiveStatistics) {
  const uint32_t kSenderSsrc = 12345;
  const uint32_t kMediaSsrc = 54321;
  MockReceiveStatisticsProvider receive_statistics;
  std::vector<ReportBlock> report_blocks(1);
  report_blocks[0].SetMediaSsrc(kMediaSsrc);
  EXPECT_CALL(receive_statistics, RtcpReportBlocks(_))
      .WillRepeatedly(Return(report_blocks));

  RtcpTransceiverConfig config;
  config.feedback_ssrc = kSenderSsrc;
  RtcpPacketParser rtcp_parser;
  RtcpParserTransport transport(&rtcp_parser);
  config.outgoing_transport = &transport;
  config.receive_statistics = &receive_statistics;
  config.schedule_periodic_compound_packets = false;
  RtcpTransceiverImpl rtcp_transceiver(config);

  rtcp_transceiver.SendCompoundPacket();

  ASSERT_GT(rtcp_parser.receiver_report()->num_packets(), 0);
  EXPECT_EQ(rtcp_parser.receiver_report()->sender_ssrc(), kSenderSsrc);
  ASSERT_THAT(rtcp_parser.receiver_report()->report_blocks(),
              SizeIs(report_blocks.size()));
  EXPECT_EQ(rtcp_parser.receiver_report()->report_blocks()[0].source_ssrc(),
            kMediaSsrc);
}

// TODO(danilchap): Write test ReceivePacket handles several rtcp_packets
// stacked together when callbacks will be implemented that can be used for
// cleaner expectations.

TEST(RtcpTransceiverImplTest,
     WhenSendsReceiverReportSetsLastSenderReportTimestampPerRemoteSsrc) {
  const uint32_t kRemoteSsrc1 = 4321;
  const uint32_t kRemoteSsrc2 = 5321;
  std::vector<ReportBlock> statistics_report_blocks(2);
  statistics_report_blocks[0].SetMediaSsrc(kRemoteSsrc1);
  statistics_report_blocks[1].SetMediaSsrc(kRemoteSsrc2);
  MockReceiveStatisticsProvider receive_statistics;
  EXPECT_CALL(receive_statistics, RtcpReportBlocks(_))
      .WillOnce(Return(statistics_report_blocks));

  RtcpTransceiverConfig config;
  config.schedule_periodic_compound_packets = false;
  RtcpPacketParser rtcp_parser;
  RtcpParserTransport transport(&rtcp_parser);
  config.outgoing_transport = &transport;
  config.receive_statistics = &receive_statistics;
  RtcpTransceiverImpl rtcp_transceiver(config);

  const NtpTime kRemoteNtp(0x9876543211);
  // Receive SenderReport for RemoteSsrc2, but no report for RemoteSsrc1.
  SenderReport sr;
  sr.SetSenderSsrc(kRemoteSsrc2);
  sr.SetNtp(kRemoteNtp);
  auto raw_packet = sr.Build();
  rtcp_transceiver.ReceivePacket(raw_packet, /*now_us=*/0);

  // Trigger sending ReceiverReport.
  rtcp_transceiver.SendCompoundPacket();

  EXPECT_GT(rtcp_parser.receiver_report()->num_packets(), 0);
  const auto& report_blocks = rtcp_parser.receiver_report()->report_blocks();
  ASSERT_EQ(report_blocks.size(), 2u);
  // RtcpTransceiverImpl doesn't guarantee order of the report blocks
  // match result of ReceiveStatisticsProvider::RtcpReportBlocks callback,
  // but for simplicity of the test asume it is the same.
  ASSERT_EQ(report_blocks[0].source_ssrc(), kRemoteSsrc1);
  // No matching Sender Report for kRemoteSsrc1, LastSR fields has to be 0.
  EXPECT_EQ(report_blocks[0].last_sr(), 0u);

  ASSERT_EQ(report_blocks[1].source_ssrc(), kRemoteSsrc2);
  EXPECT_EQ(report_blocks[1].last_sr(), CompactNtp(kRemoteNtp));
}

TEST(RtcpTransceiverImplTest,
     WhenSendsReceiverReportCalculatesDelaySinceLastSenderReport) {
  const uint32_t kRemoteSsrc1 = 4321;
  const uint32_t kRemoteSsrc2 = 5321;
  rtc::ScopedFakeClock clock;
  std::vector<ReportBlock> statistics_report_blocks(2);
  statistics_report_blocks[0].SetMediaSsrc(kRemoteSsrc1);
  statistics_report_blocks[1].SetMediaSsrc(kRemoteSsrc2);
  MockReceiveStatisticsProvider receive_statistics;
  EXPECT_CALL(receive_statistics, RtcpReportBlocks(_))
      .WillOnce(Return(statistics_report_blocks));

  RtcpTransceiverConfig config;
  config.schedule_periodic_compound_packets = false;
  RtcpPacketParser rtcp_parser;
  RtcpParserTransport transport(&rtcp_parser);
  config.outgoing_transport = &transport;
  config.receive_statistics = &receive_statistics;
  RtcpTransceiverImpl rtcp_transceiver(config);

  auto receive_sender_report = [&rtcp_transceiver](uint32_t remote_ssrc) {
    SenderReport sr;
    sr.SetSenderSsrc(remote_ssrc);
    auto raw_packet = sr.Build();
    rtcp_transceiver.ReceivePacket(raw_packet, rtc::TimeMicros());
  };

  receive_sender_report(kRemoteSsrc1);
  clock.AdvanceTimeMicros(100 * rtc::kNumMicrosecsPerMillisec);

  receive_sender_report(kRemoteSsrc2);
  clock.AdvanceTimeMicros(100 * rtc::kNumMicrosecsPerMillisec);

  // Trigger ReceiverReport back.
  rtcp_transceiver.SendCompoundPacket();

  EXPECT_GT(rtcp_parser.receiver_report()->num_packets(), 0);
  const auto& report_blocks = rtcp_parser.receiver_report()->report_blocks();
  ASSERT_EQ(report_blocks.size(), 2u);
  // RtcpTransceiverImpl doesn't guarantee order of the report blocks
  // match result of ReceiveStatisticsProvider::RtcpReportBlocks callback,
  // but for simplicity of the test asume it is the same.
  ASSERT_EQ(report_blocks[0].source_ssrc(), kRemoteSsrc1);
  EXPECT_EQ(CompactNtpRttToMs(report_blocks[0].delay_since_last_sr()), 200);

  ASSERT_EQ(report_blocks[1].source_ssrc(), kRemoteSsrc2);
  EXPECT_EQ(CompactNtpRttToMs(report_blocks[1].delay_since_last_sr()), 100);
}

TEST(RtcpTransceiverImplTest, SendsNack) {
  const uint32_t kSenderSsrc = 1234;
  const uint32_t kRemoteSsrc = 4321;
  std::vector<uint16_t> kMissingSequenceNumbers = {34, 37, 38};
  RtcpTransceiverConfig config;
  config.feedback_ssrc = kSenderSsrc;
  config.schedule_periodic_compound_packets = false;
  RtcpPacketParser rtcp_parser;
  RtcpParserTransport transport(&rtcp_parser);
  config.outgoing_transport = &transport;
  RtcpTransceiverImpl rtcp_transceiver(config);

  rtcp_transceiver.SendNack(kRemoteSsrc, kMissingSequenceNumbers);

  EXPECT_EQ(rtcp_parser.nack()->num_packets(), 1);
  EXPECT_EQ(rtcp_parser.nack()->sender_ssrc(), kSenderSsrc);
  EXPECT_EQ(rtcp_parser.nack()->media_ssrc(), kRemoteSsrc);
  EXPECT_EQ(rtcp_parser.nack()->packet_ids(), kMissingSequenceNumbers);
}

TEST(RtcpTransceiverImplTest, RequestKeyFrameWithPictureLossIndication) {
  const uint32_t kSenderSsrc = 1234;
  const uint32_t kRemoteSsrcs[] = {4321, 5321};
  RtcpTransceiverConfig config;
  config.feedback_ssrc = kSenderSsrc;
  config.schedule_periodic_compound_packets = false;
  RtcpPacketParser rtcp_parser;
  RtcpParserTransport transport(&rtcp_parser);
  config.outgoing_transport = &transport;
  RtcpTransceiverImpl rtcp_transceiver(config);

  rtcp_transceiver.SendPictureLossIndication(kRemoteSsrcs);

  // Expect a pli packet per ssrc in the sent single compound packet.
  EXPECT_EQ(transport.num_packets(), 1);
  EXPECT_EQ(rtcp_parser.pli()->num_packets(), 2);
  EXPECT_EQ(rtcp_parser.pli()->sender_ssrc(), kSenderSsrc);
  // test::RtcpPacketParser overwrites first pli packet with second one.
  EXPECT_EQ(rtcp_parser.pli()->media_ssrc(), kRemoteSsrcs[1]);
}

TEST(RtcpTransceiverImplTest, RequestKeyFrameWithFullIntraRequest) {
  const uint32_t kSenderSsrc = 1234;
  const uint32_t kRemoteSsrcs[] = {4321, 5321};
  RtcpTransceiverConfig config;
  config.feedback_ssrc = kSenderSsrc;
  config.schedule_periodic_compound_packets = false;
  RtcpPacketParser rtcp_parser;
  RtcpParserTransport transport(&rtcp_parser);
  config.outgoing_transport = &transport;
  RtcpTransceiverImpl rtcp_transceiver(config);

  rtcp_transceiver.SendFullIntraRequest(kRemoteSsrcs);

  EXPECT_EQ(rtcp_parser.fir()->num_packets(), 1);
  EXPECT_EQ(rtcp_parser.fir()->sender_ssrc(), kSenderSsrc);
  EXPECT_EQ(rtcp_parser.fir()->requests()[0].ssrc, kRemoteSsrcs[0]);
  EXPECT_EQ(rtcp_parser.fir()->requests()[1].ssrc, kRemoteSsrcs[1]);
}

TEST(RtcpTransceiverImplTest, RequestKeyFrameWithFirIncreaseSeqNoPerSsrc) {
  RtcpTransceiverConfig config;
  config.schedule_periodic_compound_packets = false;
  RtcpPacketParser rtcp_parser;
  RtcpParserTransport transport(&rtcp_parser);
  config.outgoing_transport = &transport;
  RtcpTransceiverImpl rtcp_transceiver(config);

  const uint32_t kBothRemoteSsrcs[] = {4321, 5321};
  const uint32_t kOneRemoteSsrc[] = {4321};

  rtcp_transceiver.SendFullIntraRequest(kBothRemoteSsrcs);
  ASSERT_EQ(rtcp_parser.fir()->requests()[0].ssrc, kBothRemoteSsrcs[0]);
  uint8_t fir_sequence_number0 = rtcp_parser.fir()->requests()[0].seq_nr;
  ASSERT_EQ(rtcp_parser.fir()->requests()[1].ssrc, kBothRemoteSsrcs[1]);
  uint8_t fir_sequence_number1 = rtcp_parser.fir()->requests()[1].seq_nr;

  rtcp_transceiver.SendFullIntraRequest(kOneRemoteSsrc);
  ASSERT_EQ(rtcp_parser.fir()->requests().size(), 1u);
  ASSERT_EQ(rtcp_parser.fir()->requests()[0].ssrc, kBothRemoteSsrcs[0]);
  EXPECT_EQ(rtcp_parser.fir()->requests()[0].seq_nr, fir_sequence_number0 + 1);

  rtcp_transceiver.SendFullIntraRequest(kBothRemoteSsrcs);
  ASSERT_EQ(rtcp_parser.fir()->requests().size(), 2u);
  ASSERT_EQ(rtcp_parser.fir()->requests()[0].ssrc, kBothRemoteSsrcs[0]);
  EXPECT_EQ(rtcp_parser.fir()->requests()[0].seq_nr, fir_sequence_number0 + 2);
  ASSERT_EQ(rtcp_parser.fir()->requests()[1].ssrc, kBothRemoteSsrcs[1]);
  EXPECT_EQ(rtcp_parser.fir()->requests()[1].seq_nr, fir_sequence_number1 + 1);
}

TEST(RtcpTransceiverImplTest, KeyFrameRequestCreatesCompoundPacket) {
  const uint32_t kRemoteSsrcs[] = {4321};
  RtcpTransceiverConfig config;
  // Turn periodic off to ensure sent rtcp packet is explicitly requested.
  config.schedule_periodic_compound_packets = false;
  RtcpPacketParser rtcp_parser;
  RtcpParserTransport transport(&rtcp_parser);
  config.outgoing_transport = &transport;

  config.rtcp_mode = webrtc::RtcpMode::kCompound;

  RtcpTransceiverImpl rtcp_transceiver(config);
  rtcp_transceiver.SendFullIntraRequest(kRemoteSsrcs);

  // Test sent packet is compound by expecting presense of receiver report.
  EXPECT_EQ(transport.num_packets(), 1);
  EXPECT_EQ(rtcp_parser.receiver_report()->num_packets(), 1);
}

TEST(RtcpTransceiverImplTest, KeyFrameRequestCreatesReducedSizePacket) {
  const uint32_t kRemoteSsrcs[] = {4321};
  RtcpTransceiverConfig config;
  // Turn periodic off to ensure sent rtcp packet is explicitly requested.
  config.schedule_periodic_compound_packets = false;
  RtcpPacketParser rtcp_parser;
  RtcpParserTransport transport(&rtcp_parser);
  config.outgoing_transport = &transport;

  config.rtcp_mode = webrtc::RtcpMode::kReducedSize;

  RtcpTransceiverImpl rtcp_transceiver(config);
  rtcp_transceiver.SendFullIntraRequest(kRemoteSsrcs);

  // Test sent packet is reduced size by expecting absense of receiver report.
  EXPECT_EQ(transport.num_packets(), 1);
  EXPECT_EQ(rtcp_parser.receiver_report()->num_packets(), 0);
}

}  // namespace
