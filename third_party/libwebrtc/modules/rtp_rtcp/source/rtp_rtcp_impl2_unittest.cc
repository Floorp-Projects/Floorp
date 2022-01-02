/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/rtp_rtcp_impl2.h"

#include <map>
#include <memory>
#include <set>

#include "api/transport/field_trial_based_config.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "modules/rtp_rtcp/source/rtcp_packet.h"
#include "modules/rtp_rtcp/source/rtcp_packet/nack.h"
#include "modules/rtp_rtcp/source/rtp_packet_received.h"
#include "modules/rtp_rtcp/source/rtp_sender_video.h"
#include "rtc_base/rate_limiter.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/rtcp_packet_parser.h"
#include "test/rtp_header_parser.h"
#include "test/run_loop.h"
#include "test/time_controller/simulated_time_controller.h"

using ::testing::ElementsAre;

namespace webrtc {
namespace {
const uint32_t kSenderSsrc = 0x12345;
const uint32_t kReceiverSsrc = 0x23456;
const int64_t kOneWayNetworkDelayMs = 100;
const uint8_t kBaseLayerTid = 0;
const uint8_t kHigherLayerTid = 1;
const uint16_t kSequenceNumber = 100;
const uint8_t kPayloadType = 100;
const int kWidth = 320;
const int kHeight = 100;

class RtcpRttStatsTestImpl : public RtcpRttStats {
 public:
  RtcpRttStatsTestImpl() : rtt_ms_(0) {}
  ~RtcpRttStatsTestImpl() override = default;

  void OnRttUpdate(int64_t rtt_ms) override { rtt_ms_ = rtt_ms; }
  int64_t LastProcessedRtt() const override { return rtt_ms_; }
  int64_t rtt_ms_;
};

class SendTransport : public Transport {
 public:
  SendTransport()
      : receiver_(nullptr),
        time_controller_(nullptr),
        delay_ms_(0),
        rtp_packets_sent_(0),
        rtcp_packets_sent_(0) {}

  void SetRtpRtcpModule(ModuleRtpRtcpImpl2* receiver) { receiver_ = receiver; }
  void SimulateNetworkDelay(int64_t delay_ms, TimeController* time_controller) {
    time_controller_ = time_controller;
    delay_ms_ = delay_ms;
  }
  bool SendRtp(const uint8_t* data,
               size_t len,
               const PacketOptions& options) override {
    RTPHeader header;
    std::unique_ptr<RtpHeaderParser> parser(RtpHeaderParser::CreateForTest());
    EXPECT_TRUE(parser->Parse(static_cast<const uint8_t*>(data), len, &header));
    ++rtp_packets_sent_;
    last_rtp_header_ = header;
    return true;
  }
  bool SendRtcp(const uint8_t* data, size_t len) override {
    test::RtcpPacketParser parser;
    parser.Parse(data, len);
    last_nack_list_ = parser.nack()->packet_ids();

    if (time_controller_) {
      time_controller_->AdvanceTime(TimeDelta::Millis(delay_ms_));
    }
    EXPECT_TRUE(receiver_);
    receiver_->IncomingRtcpPacket(data, len);
    ++rtcp_packets_sent_;
    return true;
  }

  size_t NumRtcpSent() { return rtcp_packets_sent_; }

  ModuleRtpRtcpImpl2* receiver_;
  TimeController* time_controller_;
  int64_t delay_ms_;
  int rtp_packets_sent_;
  size_t rtcp_packets_sent_;
  RTPHeader last_rtp_header_;
  std::vector<uint16_t> last_nack_list_;
};

class RtpRtcpModule : public RtcpPacketTypeCounterObserver {
 public:
  RtpRtcpModule(TimeController* time_controller, bool is_sender)
      : is_sender_(is_sender),
        receive_statistics_(
            ReceiveStatistics::Create(time_controller->GetClock())),
        time_controller_(time_controller) {
    CreateModuleImpl();
    transport_.SimulateNetworkDelay(kOneWayNetworkDelayMs, time_controller);
  }

  const bool is_sender_;
  RtcpPacketTypeCounter packets_sent_;
  RtcpPacketTypeCounter packets_received_;
  std::unique_ptr<ReceiveStatistics> receive_statistics_;
  SendTransport transport_;
  RtcpRttStatsTestImpl rtt_stats_;
  std::unique_ptr<ModuleRtpRtcpImpl2> impl_;
  int rtcp_report_interval_ms_ = 0;

  void RtcpPacketTypesCounterUpdated(
      uint32_t ssrc,
      const RtcpPacketTypeCounter& packet_counter) override {
    counter_map_[ssrc] = packet_counter;
  }

  RtcpPacketTypeCounter RtcpSent() {
    // RTCP counters for remote SSRC.
    return counter_map_[is_sender_ ? kReceiverSsrc : kSenderSsrc];
  }

  RtcpPacketTypeCounter RtcpReceived() {
    // Received RTCP stats for (own) local SSRC.
    return counter_map_[impl_->SSRC()];
  }
  int RtpSent() { return transport_.rtp_packets_sent_; }
  uint16_t LastRtpSequenceNumber() {
    return transport_.last_rtp_header_.sequenceNumber;
  }
  std::vector<uint16_t> LastNackListSent() {
    return transport_.last_nack_list_;
  }
  void SetRtcpReportIntervalAndReset(int rtcp_report_interval_ms) {
    rtcp_report_interval_ms_ = rtcp_report_interval_ms;
    CreateModuleImpl();
  }

 private:
  void CreateModuleImpl() {
    RtpRtcpInterface::Configuration config;
    config.audio = false;
    config.clock = time_controller_->GetClock();
    config.outgoing_transport = &transport_;
    config.receive_statistics = receive_statistics_.get();
    config.rtcp_packet_type_counter_observer = this;
    config.rtt_stats = &rtt_stats_;
    config.rtcp_report_interval_ms = rtcp_report_interval_ms_;
    config.local_media_ssrc = is_sender_ ? kSenderSsrc : kReceiverSsrc;
    config.need_rtp_packet_infos = true;

    impl_.reset(new ModuleRtpRtcpImpl2(config));
    impl_->SetRemoteSSRC(is_sender_ ? kReceiverSsrc : kSenderSsrc);
    impl_->SetRTCPStatus(RtcpMode::kCompound);
  }

  TimeController* const time_controller_;
  std::map<uint32_t, RtcpPacketTypeCounter> counter_map_;
};
}  // namespace

class RtpRtcpImpl2Test : public ::testing::Test {
 protected:
  RtpRtcpImpl2Test()
      : time_controller_(Timestamp::Micros(133590000000000)),
        sender_(&time_controller_, /*is_sender=*/true),
        receiver_(&time_controller_, /*is_sender=*/false) {}

  void SetUp() override {
    // Send module.
    EXPECT_EQ(0, sender_.impl_->SetSendingStatus(true));
    sender_.impl_->SetSendingMediaStatus(true);
    sender_.impl_->SetSequenceNumber(kSequenceNumber);
    sender_.impl_->SetStorePacketsStatus(true, 100);

    FieldTrialBasedConfig field_trials;
    RTPSenderVideo::Config video_config;
    video_config.clock = time_controller_.GetClock();
    video_config.rtp_sender = sender_.impl_->RtpSender();
    video_config.field_trials = &field_trials;
    sender_video_ = std::make_unique<RTPSenderVideo>(video_config);

    // Receive module.
    EXPECT_EQ(0, receiver_.impl_->SetSendingStatus(false));
    receiver_.impl_->SetSendingMediaStatus(false);
    // Transport settings.
    sender_.transport_.SetRtpRtcpModule(receiver_.impl_.get());
    receiver_.transport_.SetRtpRtcpModule(sender_.impl_.get());
  }

  void AdvanceTimeMs(int64_t milliseconds) {
    time_controller_.AdvanceTime(TimeDelta::Millis(milliseconds));
  }

  GlobalSimulatedTimeController time_controller_;
  // test::RunLoop loop_;
  // SimulatedClock clock_;
  RtpRtcpModule sender_;
  std::unique_ptr<RTPSenderVideo> sender_video_;
  RtpRtcpModule receiver_;

  void SendFrame(const RtpRtcpModule* module,
                 RTPSenderVideo* sender,
                 uint8_t tid) {
    RTPVideoHeaderVP8 vp8_header = {};
    vp8_header.temporalIdx = tid;
    RTPVideoHeader rtp_video_header;
    rtp_video_header.frame_type = VideoFrameType::kVideoFrameKey;
    rtp_video_header.width = kWidth;
    rtp_video_header.height = kHeight;
    rtp_video_header.rotation = kVideoRotation_0;
    rtp_video_header.content_type = VideoContentType::UNSPECIFIED;
    rtp_video_header.playout_delay = {-1, -1};
    rtp_video_header.is_first_packet_in_frame = true;
    rtp_video_header.simulcastIdx = 0;
    rtp_video_header.codec = kVideoCodecVP8;
    rtp_video_header.video_type_header = vp8_header;
    rtp_video_header.video_timing = {0u, 0u, 0u, 0u, 0u, 0u, false};

    const uint8_t payload[100] = {0};
    EXPECT_TRUE(module->impl_->OnSendingRtpFrame(0, 0, kPayloadType, true));
    EXPECT_TRUE(sender->SendVideo(kPayloadType, VideoCodecType::kVideoCodecVP8,
                                  0, 0, payload, rtp_video_header, 0));
  }

  void IncomingRtcpNack(const RtpRtcpModule* module, uint16_t sequence_number) {
    bool sender = module->impl_->SSRC() == kSenderSsrc;
    rtcp::Nack nack;
    uint16_t list[1];
    list[0] = sequence_number;
    const uint16_t kListLength = sizeof(list) / sizeof(list[0]);
    nack.SetSenderSsrc(sender ? kReceiverSsrc : kSenderSsrc);
    nack.SetMediaSsrc(sender ? kSenderSsrc : kReceiverSsrc);
    nack.SetPacketIds(list, kListLength);
    rtc::Buffer packet = nack.Build();
    module->impl_->IncomingRtcpPacket(packet.data(), packet.size());
  }
};

TEST_F(RtpRtcpImpl2Test, RetransmitsAllLayers) {
  // Send frames.
  EXPECT_EQ(0, sender_.RtpSent());
  SendFrame(&sender_, sender_video_.get(), kBaseLayerTid);  // kSequenceNumber
  SendFrame(&sender_, sender_video_.get(),
            kHigherLayerTid);  // kSequenceNumber + 1
  SendFrame(&sender_, sender_video_.get(),
            kNoTemporalIdx);  // kSequenceNumber + 2
  EXPECT_EQ(3, sender_.RtpSent());
  EXPECT_EQ(kSequenceNumber + 2, sender_.LastRtpSequenceNumber());

  // Min required delay until retransmit = 5 + RTT ms (RTT = 0).
  AdvanceTimeMs(5);

  // Frame with kBaseLayerTid re-sent.
  IncomingRtcpNack(&sender_, kSequenceNumber);
  EXPECT_EQ(4, sender_.RtpSent());
  EXPECT_EQ(kSequenceNumber, sender_.LastRtpSequenceNumber());
  // Frame with kHigherLayerTid re-sent.
  IncomingRtcpNack(&sender_, kSequenceNumber + 1);
  EXPECT_EQ(5, sender_.RtpSent());
  EXPECT_EQ(kSequenceNumber + 1, sender_.LastRtpSequenceNumber());
  // Frame with kNoTemporalIdx re-sent.
  IncomingRtcpNack(&sender_, kSequenceNumber + 2);
  EXPECT_EQ(6, sender_.RtpSent());
  EXPECT_EQ(kSequenceNumber + 2, sender_.LastRtpSequenceNumber());
}

TEST_F(RtpRtcpImpl2Test, Rtt) {
  RtpPacketReceived packet;
  packet.SetTimestamp(1);
  packet.SetSequenceNumber(123);
  packet.SetSsrc(kSenderSsrc);
  packet.AllocatePayload(100 - 12);
  receiver_.receive_statistics_->OnRtpPacket(packet);

  // Send Frame before sending an SR.
  SendFrame(&sender_, sender_video_.get(), kBaseLayerTid);
  // Sender module should send an SR.
  EXPECT_EQ(0, sender_.impl_->SendRTCP(kRtcpReport));

  // Receiver module should send a RR with a response to the last received SR.
  AdvanceTimeMs(1000);
  EXPECT_EQ(0, receiver_.impl_->SendRTCP(kRtcpReport));

  // Verify RTT.
  int64_t rtt;
  int64_t avg_rtt;
  int64_t min_rtt;
  int64_t max_rtt;
  EXPECT_EQ(
      0, sender_.impl_->RTT(kReceiverSsrc, &rtt, &avg_rtt, &min_rtt, &max_rtt));
  EXPECT_NEAR(2 * kOneWayNetworkDelayMs, rtt, 1);
  EXPECT_NEAR(2 * kOneWayNetworkDelayMs, avg_rtt, 1);
  EXPECT_NEAR(2 * kOneWayNetworkDelayMs, min_rtt, 1);
  EXPECT_NEAR(2 * kOneWayNetworkDelayMs, max_rtt, 1);

  // No RTT from other ssrc.
  EXPECT_EQ(-1, sender_.impl_->RTT(kReceiverSsrc + 1, &rtt, &avg_rtt, &min_rtt,
                                   &max_rtt));

  // Verify RTT from rtt_stats config.
  EXPECT_EQ(0, sender_.rtt_stats_.LastProcessedRtt());
  EXPECT_EQ(0, sender_.impl_->rtt_ms());
  AdvanceTimeMs(1000);

  EXPECT_NEAR(2 * kOneWayNetworkDelayMs, sender_.rtt_stats_.LastProcessedRtt(),
              1);
  EXPECT_NEAR(2 * kOneWayNetworkDelayMs, sender_.impl_->rtt_ms(), 1);
}

TEST_F(RtpRtcpImpl2Test, SetRtcpXrRrtrStatus) {
  EXPECT_FALSE(receiver_.impl_->RtcpXrRrtrStatus());
  receiver_.impl_->SetRtcpXrRrtrStatus(true);
  EXPECT_TRUE(receiver_.impl_->RtcpXrRrtrStatus());
}

TEST_F(RtpRtcpImpl2Test, RttForReceiverOnly) {
  receiver_.impl_->SetRtcpXrRrtrStatus(true);

  // Receiver module should send a Receiver time reference report (RTRR).
  EXPECT_EQ(0, receiver_.impl_->SendRTCP(kRtcpReport));

  // Sender module should send a response to the last received RTRR (DLRR).
  AdvanceTimeMs(1000);
  // Send Frame before sending a SR.
  SendFrame(&sender_, sender_video_.get(), kBaseLayerTid);
  EXPECT_EQ(0, sender_.impl_->SendRTCP(kRtcpReport));

  // Verify RTT.
  EXPECT_EQ(0, receiver_.rtt_stats_.LastProcessedRtt());
  EXPECT_EQ(0, receiver_.impl_->rtt_ms());
  AdvanceTimeMs(1000);
  EXPECT_NEAR(2 * kOneWayNetworkDelayMs,
              receiver_.rtt_stats_.LastProcessedRtt(), 1);
  EXPECT_NEAR(2 * kOneWayNetworkDelayMs, receiver_.impl_->rtt_ms(), 1);
}

TEST_F(RtpRtcpImpl2Test, NoSrBeforeMedia) {
  // Ignore fake transport delays in this test.
  sender_.transport_.SimulateNetworkDelay(0, &time_controller_);
  receiver_.transport_.SimulateNetworkDelay(0, &time_controller_);

  sender_.impl_->Process();
  EXPECT_EQ(-1, sender_.RtcpSent().first_packet_time_ms);

  // Verify no SR is sent before media has been sent, RR should still be sent
  // from the receiving module though.
  AdvanceTimeMs(2000);
  int64_t current_time = time_controller_.GetClock()->TimeInMilliseconds();
  sender_.impl_->Process();
  receiver_.impl_->Process();
  EXPECT_EQ(-1, sender_.RtcpSent().first_packet_time_ms);
  EXPECT_EQ(receiver_.RtcpSent().first_packet_time_ms, current_time);

  SendFrame(&sender_, sender_video_.get(), kBaseLayerTid);
  EXPECT_EQ(sender_.RtcpSent().first_packet_time_ms, current_time);
}

TEST_F(RtpRtcpImpl2Test, RtcpPacketTypeCounter_Nack) {
  EXPECT_EQ(-1, receiver_.RtcpSent().first_packet_time_ms);
  EXPECT_EQ(-1, sender_.RtcpReceived().first_packet_time_ms);
  EXPECT_EQ(0U, sender_.RtcpReceived().nack_packets);
  EXPECT_EQ(0U, receiver_.RtcpSent().nack_packets);

  // Receive module sends a NACK.
  const uint16_t kNackLength = 1;
  uint16_t nack_list[kNackLength] = {123};
  EXPECT_EQ(0, receiver_.impl_->SendNACK(nack_list, kNackLength));
  EXPECT_EQ(1U, receiver_.RtcpSent().nack_packets);
  EXPECT_GT(receiver_.RtcpSent().first_packet_time_ms, -1);

  // Send module receives the NACK.
  EXPECT_EQ(1U, sender_.RtcpReceived().nack_packets);
  EXPECT_GT(sender_.RtcpReceived().first_packet_time_ms, -1);
}

TEST_F(RtpRtcpImpl2Test, AddStreamDataCounters) {
  StreamDataCounters rtp;
  const int64_t kStartTimeMs = 1;
  rtp.first_packet_time_ms = kStartTimeMs;
  rtp.transmitted.packets = 1;
  rtp.transmitted.payload_bytes = 1;
  rtp.transmitted.header_bytes = 2;
  rtp.transmitted.padding_bytes = 3;
  EXPECT_EQ(rtp.transmitted.TotalBytes(), rtp.transmitted.payload_bytes +
                                              rtp.transmitted.header_bytes +
                                              rtp.transmitted.padding_bytes);

  StreamDataCounters rtp2;
  rtp2.first_packet_time_ms = -1;
  rtp2.transmitted.packets = 10;
  rtp2.transmitted.payload_bytes = 10;
  rtp2.retransmitted.header_bytes = 4;
  rtp2.retransmitted.payload_bytes = 5;
  rtp2.retransmitted.padding_bytes = 6;
  rtp2.retransmitted.packets = 7;
  rtp2.fec.packets = 8;

  StreamDataCounters sum = rtp;
  sum.Add(rtp2);
  EXPECT_EQ(kStartTimeMs, sum.first_packet_time_ms);
  EXPECT_EQ(11U, sum.transmitted.packets);
  EXPECT_EQ(11U, sum.transmitted.payload_bytes);
  EXPECT_EQ(2U, sum.transmitted.header_bytes);
  EXPECT_EQ(3U, sum.transmitted.padding_bytes);
  EXPECT_EQ(4U, sum.retransmitted.header_bytes);
  EXPECT_EQ(5U, sum.retransmitted.payload_bytes);
  EXPECT_EQ(6U, sum.retransmitted.padding_bytes);
  EXPECT_EQ(7U, sum.retransmitted.packets);
  EXPECT_EQ(8U, sum.fec.packets);
  EXPECT_EQ(sum.transmitted.TotalBytes(),
            rtp.transmitted.TotalBytes() + rtp2.transmitted.TotalBytes());

  StreamDataCounters rtp3;
  rtp3.first_packet_time_ms = kStartTimeMs + 10;
  sum.Add(rtp3);
  EXPECT_EQ(kStartTimeMs, sum.first_packet_time_ms);  // Holds oldest time.
}

TEST_F(RtpRtcpImpl2Test, SendsInitialNackList) {
  // Send module sends a NACK.
  const uint16_t kNackLength = 1;
  uint16_t nack_list[kNackLength] = {123};
  EXPECT_EQ(0U, sender_.RtcpSent().nack_packets);
  // Send Frame before sending a compound RTCP that starts with SR.
  SendFrame(&sender_, sender_video_.get(), kBaseLayerTid);
  EXPECT_EQ(0, sender_.impl_->SendNACK(nack_list, kNackLength));
  EXPECT_EQ(1U, sender_.RtcpSent().nack_packets);
  EXPECT_THAT(sender_.LastNackListSent(), ElementsAre(123));
}

TEST_F(RtpRtcpImpl2Test, SendsExtendedNackList) {
  // Send module sends a NACK.
  const uint16_t kNackLength = 1;
  uint16_t nack_list[kNackLength] = {123};
  EXPECT_EQ(0U, sender_.RtcpSent().nack_packets);
  // Send Frame before sending a compound RTCP that starts with SR.
  SendFrame(&sender_, sender_video_.get(), kBaseLayerTid);
  EXPECT_EQ(0, sender_.impl_->SendNACK(nack_list, kNackLength));
  EXPECT_EQ(1U, sender_.RtcpSent().nack_packets);
  EXPECT_THAT(sender_.LastNackListSent(), ElementsAre(123));

  // Same list not re-send.
  EXPECT_EQ(0, sender_.impl_->SendNACK(nack_list, kNackLength));
  EXPECT_EQ(1U, sender_.RtcpSent().nack_packets);
  EXPECT_THAT(sender_.LastNackListSent(), ElementsAre(123));

  // Only extended list sent.
  const uint16_t kNackExtLength = 2;
  uint16_t nack_list_ext[kNackExtLength] = {123, 124};
  EXPECT_EQ(0, sender_.impl_->SendNACK(nack_list_ext, kNackExtLength));
  EXPECT_EQ(2U, sender_.RtcpSent().nack_packets);
  EXPECT_THAT(sender_.LastNackListSent(), ElementsAre(124));
}

TEST_F(RtpRtcpImpl2Test, ReSendsNackListAfterRttMs) {
  sender_.transport_.SimulateNetworkDelay(0, &time_controller_);
  // Send module sends a NACK.
  const uint16_t kNackLength = 2;
  uint16_t nack_list[kNackLength] = {123, 125};
  EXPECT_EQ(0U, sender_.RtcpSent().nack_packets);
  // Send Frame before sending a compound RTCP that starts with SR.
  SendFrame(&sender_, sender_video_.get(), kBaseLayerTid);
  EXPECT_EQ(0, sender_.impl_->SendNACK(nack_list, kNackLength));
  EXPECT_EQ(1U, sender_.RtcpSent().nack_packets);
  EXPECT_THAT(sender_.LastNackListSent(), ElementsAre(123, 125));

  // Same list not re-send, rtt interval has not passed.
  const int kStartupRttMs = 100;
  AdvanceTimeMs(kStartupRttMs);
  EXPECT_EQ(0, sender_.impl_->SendNACK(nack_list, kNackLength));
  EXPECT_EQ(1U, sender_.RtcpSent().nack_packets);

  // Rtt interval passed, full list sent.
  AdvanceTimeMs(1);
  EXPECT_EQ(0, sender_.impl_->SendNACK(nack_list, kNackLength));
  EXPECT_EQ(2U, sender_.RtcpSent().nack_packets);
  EXPECT_THAT(sender_.LastNackListSent(), ElementsAre(123, 125));
}

TEST_F(RtpRtcpImpl2Test, UniqueNackRequests) {
  receiver_.transport_.SimulateNetworkDelay(0, &time_controller_);
  EXPECT_EQ(0U, receiver_.RtcpSent().nack_packets);
  EXPECT_EQ(0U, receiver_.RtcpSent().nack_requests);
  EXPECT_EQ(0U, receiver_.RtcpSent().unique_nack_requests);
  EXPECT_EQ(0, receiver_.RtcpSent().UniqueNackRequestsInPercent());

  // Receive module sends NACK request.
  const uint16_t kNackLength = 4;
  uint16_t nack_list[kNackLength] = {10, 11, 13, 18};
  EXPECT_EQ(0, receiver_.impl_->SendNACK(nack_list, kNackLength));
  EXPECT_EQ(1U, receiver_.RtcpSent().nack_packets);
  EXPECT_EQ(4U, receiver_.RtcpSent().nack_requests);
  EXPECT_EQ(4U, receiver_.RtcpSent().unique_nack_requests);
  EXPECT_THAT(receiver_.LastNackListSent(), ElementsAre(10, 11, 13, 18));

  // Send module receives the request.
  EXPECT_EQ(1U, sender_.RtcpReceived().nack_packets);
  EXPECT_EQ(4U, sender_.RtcpReceived().nack_requests);
  EXPECT_EQ(4U, sender_.RtcpReceived().unique_nack_requests);
  EXPECT_EQ(100, sender_.RtcpReceived().UniqueNackRequestsInPercent());

  // Receive module sends new request with duplicated packets.
  const int kStartupRttMs = 100;
  AdvanceTimeMs(kStartupRttMs + 1);
  const uint16_t kNackLength2 = 4;
  uint16_t nack_list2[kNackLength2] = {11, 18, 20, 21};
  EXPECT_EQ(0, receiver_.impl_->SendNACK(nack_list2, kNackLength2));
  EXPECT_EQ(2U, receiver_.RtcpSent().nack_packets);
  EXPECT_EQ(8U, receiver_.RtcpSent().nack_requests);
  EXPECT_EQ(6U, receiver_.RtcpSent().unique_nack_requests);
  EXPECT_THAT(receiver_.LastNackListSent(), ElementsAre(11, 18, 20, 21));

  // Send module receives the request.
  EXPECT_EQ(2U, sender_.RtcpReceived().nack_packets);
  EXPECT_EQ(8U, sender_.RtcpReceived().nack_requests);
  EXPECT_EQ(6U, sender_.RtcpReceived().unique_nack_requests);
  EXPECT_EQ(75, sender_.RtcpReceived().UniqueNackRequestsInPercent());
}

TEST_F(RtpRtcpImpl2Test, ConfigurableRtcpReportInterval) {
  const int kVideoReportInterval = 3000;

  // Recreate sender impl with new configuration, and redo setup.
  sender_.SetRtcpReportIntervalAndReset(kVideoReportInterval);
  SetUp();

  SendFrame(&sender_, sender_video_.get(), kBaseLayerTid);

  // Initial state
  sender_.impl_->Process();
  EXPECT_EQ(sender_.RtcpSent().first_packet_time_ms, -1);
  EXPECT_EQ(0u, sender_.transport_.NumRtcpSent());

  // Move ahead to the last ms before a rtcp is expected, no action.
  AdvanceTimeMs(kVideoReportInterval / 2 - 1);
  sender_.impl_->Process();
  EXPECT_EQ(sender_.RtcpSent().first_packet_time_ms, -1);
  EXPECT_EQ(sender_.transport_.NumRtcpSent(), 0u);

  // Move ahead to the first rtcp. Send RTCP.
  AdvanceTimeMs(1);
  sender_.impl_->Process();
  EXPECT_GT(sender_.RtcpSent().first_packet_time_ms, -1);
  EXPECT_EQ(sender_.transport_.NumRtcpSent(), 1u);

  SendFrame(&sender_, sender_video_.get(), kBaseLayerTid);

  // Move ahead to the last possible second before second rtcp is expected.
  AdvanceTimeMs(kVideoReportInterval * 1 / 2 - 1);
  sender_.impl_->Process();
  EXPECT_EQ(sender_.transport_.NumRtcpSent(), 1u);

  // Move ahead into the range of second rtcp, the second rtcp may be sent.
  AdvanceTimeMs(1);
  sender_.impl_->Process();
  EXPECT_GE(sender_.transport_.NumRtcpSent(), 1u);

  AdvanceTimeMs(kVideoReportInterval / 2);
  sender_.impl_->Process();
  EXPECT_GE(sender_.transport_.NumRtcpSent(), 1u);

  // Move out the range of second rtcp, the second rtcp must have been sent.
  AdvanceTimeMs(kVideoReportInterval / 2);
  sender_.impl_->Process();
  EXPECT_EQ(sender_.transport_.NumRtcpSent(), 2u);
}

TEST_F(RtpRtcpImpl2Test, StoresPacketInfoForSentPackets) {
  const uint32_t kStartTimestamp = 1u;
  SetUp();
  sender_.impl_->SetStartTimestamp(kStartTimestamp);

  PacedPacketInfo pacing_info;
  RtpPacketToSend packet(nullptr);
  packet.set_packet_type(RtpPacketToSend::Type::kVideo);
  packet.SetSsrc(kSenderSsrc);

  // Single-packet frame.
  packet.SetTimestamp(1);
  packet.SetSequenceNumber(1);
  packet.set_first_packet_of_frame(true);
  packet.SetMarker(true);
  sender_.impl_->TrySendPacket(&packet, pacing_info);
  AdvanceTimeMs(1);

  std::vector<RtpSequenceNumberMap::Info> seqno_info =
      sender_.impl_->GetSentRtpPacketInfos(std::vector<uint16_t>{1});

  EXPECT_THAT(seqno_info, ElementsAre(RtpSequenceNumberMap::Info(
                              /*timestamp=*/1 - kStartTimestamp,
                              /*is_first=*/1,
                              /*is_last=*/1)));

  // Three-packet frame.
  packet.SetTimestamp(2);
  packet.SetSequenceNumber(2);
  packet.set_first_packet_of_frame(true);
  packet.SetMarker(false);
  sender_.impl_->TrySendPacket(&packet, pacing_info);

  packet.SetSequenceNumber(3);
  packet.set_first_packet_of_frame(false);
  sender_.impl_->TrySendPacket(&packet, pacing_info);

  packet.SetSequenceNumber(4);
  packet.SetMarker(true);
  sender_.impl_->TrySendPacket(&packet, pacing_info);

  AdvanceTimeMs(1);

  seqno_info =
      sender_.impl_->GetSentRtpPacketInfos(std::vector<uint16_t>{2, 3, 4});

  EXPECT_THAT(seqno_info, ElementsAre(RtpSequenceNumberMap::Info(
                                          /*timestamp=*/2 - kStartTimestamp,
                                          /*is_first=*/1,
                                          /*is_last=*/0),
                                      RtpSequenceNumberMap::Info(
                                          /*timestamp=*/2 - kStartTimestamp,
                                          /*is_first=*/0,
                                          /*is_last=*/0),
                                      RtpSequenceNumberMap::Info(
                                          /*timestamp=*/2 - kStartTimestamp,
                                          /*is_first=*/0,
                                          /*is_last=*/1)));
}

}  // namespace webrtc
