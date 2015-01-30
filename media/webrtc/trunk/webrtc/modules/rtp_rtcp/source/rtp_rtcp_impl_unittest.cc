/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#include "webrtc/common_types.h"
#include "webrtc/modules/pacing/include/mock/mock_paced_sender.h"
#include "webrtc/modules/rtp_rtcp/interface/rtp_header_parser.h"
#include "webrtc/modules/rtp_rtcp/interface/rtp_rtcp_defines.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_packet.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_rtcp_impl.h"
#include "webrtc/system_wrappers/interface/scoped_vector.h"
#include "webrtc/test/rtcp_packet_parser.h"

using ::testing::_;
using ::testing::ElementsAre;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::SaveArg;

namespace webrtc {
namespace {
const uint32_t kSenderSsrc = 0x12345;
const uint32_t kReceiverSsrc = 0x23456;
const uint32_t kSenderRtxSsrc = 0x32345;
const uint32_t kOneWayNetworkDelayMs = 100;
const uint8_t kBaseLayerTid = 0;
const uint8_t kHigherLayerTid = 1;
const uint16_t kSequenceNumber = 100;

class RtcpRttStatsTestImpl : public RtcpRttStats {
 public:
  RtcpRttStatsTestImpl() : rtt_ms_(0) {}
  virtual ~RtcpRttStatsTestImpl() {}

  virtual void OnRttUpdate(uint32_t rtt_ms) OVERRIDE {
    rtt_ms_ = rtt_ms;
  }
  virtual uint32_t LastProcessedRtt() const OVERRIDE {
    return rtt_ms_;
  }
  uint32_t rtt_ms_;
};

class SendTransport : public Transport,
                      public NullRtpData {
 public:
  SendTransport()
      : receiver_(NULL),
        clock_(NULL),
        delay_ms_(0),
        rtp_packets_sent_(0) {
  }

  void SetRtpRtcpModule(ModuleRtpRtcpImpl* receiver) {
    receiver_ = receiver;
  }
  void SimulateNetworkDelay(uint32_t delay_ms, SimulatedClock* clock) {
    clock_ = clock;
    delay_ms_ = delay_ms;
  }
  virtual int SendPacket(int /*ch*/, const void* data, int len) OVERRIDE {
    RTPHeader header;
    scoped_ptr<RtpHeaderParser> parser(RtpHeaderParser::Create());
    EXPECT_TRUE(parser->Parse(static_cast<const uint8_t*>(data),
                              static_cast<size_t>(len),
                              &header));
    ++rtp_packets_sent_;
    last_rtp_header_ = header;
    return len;
  }
  virtual int SendRTCPPacket(int /*ch*/, const void *data, int len) OVERRIDE {
    test::RtcpPacketParser parser;
    parser.Parse(static_cast<const uint8_t*>(data), len);
    last_nack_list_ = parser.nack_item()->last_nack_list();

    if (clock_) {
      clock_->AdvanceTimeMilliseconds(delay_ms_);
    }
    EXPECT_TRUE(receiver_ != NULL);
    EXPECT_EQ(0, receiver_->IncomingRtcpPacket(
        static_cast<const uint8_t*>(data), len));
    return len;
  }
  ModuleRtpRtcpImpl* receiver_;
  SimulatedClock* clock_;
  uint32_t delay_ms_;
  int rtp_packets_sent_;
  RTPHeader last_rtp_header_;
  std::vector<uint16_t> last_nack_list_;
};

class RtpRtcpModule {
 public:
  RtpRtcpModule(SimulatedClock* clock)
      : receive_statistics_(ReceiveStatistics::Create(clock)) {
    RtpRtcp::Configuration config;
    config.audio = false;
    config.clock = clock;
    config.outgoing_transport = &transport_;
    config.receive_statistics = receive_statistics_.get();
    config.rtt_stats = &rtt_stats_;

    impl_.reset(new ModuleRtpRtcpImpl(config));
    EXPECT_EQ(0, impl_->SetRTCPStatus(kRtcpCompound));

    transport_.SimulateNetworkDelay(kOneWayNetworkDelayMs, clock);
  }

  RtcpPacketTypeCounter packets_sent_;
  RtcpPacketTypeCounter packets_received_;
  scoped_ptr<ReceiveStatistics> receive_statistics_;
  SendTransport transport_;
  RtcpRttStatsTestImpl rtt_stats_;
  scoped_ptr<ModuleRtpRtcpImpl> impl_;

  RtcpPacketTypeCounter RtcpSent() {
    impl_->GetRtcpPacketTypeCounters(&packets_sent_, &packets_received_);
    return packets_sent_;
  }
  RtcpPacketTypeCounter RtcpReceived() {
    impl_->GetRtcpPacketTypeCounters(&packets_sent_, &packets_received_);
    return packets_received_;
  }
  int RtpSent() {
    return transport_.rtp_packets_sent_;
  }
  uint16_t LastRtpSequenceNumber() {
    return transport_.last_rtp_header_.sequenceNumber;
  }
  std::vector<uint16_t> LastNackListSent() {
    return transport_.last_nack_list_;
  }
};
}  // namespace

class RtpRtcpImplTest : public ::testing::Test {
 protected:
  RtpRtcpImplTest()
      : clock_(1335900000),
        sender_(&clock_),
        receiver_(&clock_) {
    // Send module.
    EXPECT_EQ(0, sender_.impl_->SetSendingStatus(true));
    EXPECT_EQ(0, sender_.impl_->SetSendingMediaStatus(true));
    sender_.impl_->SetSSRC(kSenderSsrc);
    sender_.impl_->SetRemoteSSRC(kReceiverSsrc);
    sender_.impl_->SetSequenceNumber(kSequenceNumber);
    sender_.impl_->SetStorePacketsStatus(true, 100);

    memset(&codec_, 0, sizeof(VideoCodec));
    codec_.plType = 100;
    strncpy(codec_.plName, "VP8", 3);
    codec_.width = 320;
    codec_.height = 180;
    EXPECT_EQ(0, sender_.impl_->RegisterSendPayload(codec_));

    // Receive module.
    EXPECT_EQ(0, receiver_.impl_->SetSendingStatus(false));
    EXPECT_EQ(0, receiver_.impl_->SetSendingMediaStatus(false));
    receiver_.impl_->SetSSRC(kReceiverSsrc);
    receiver_.impl_->SetRemoteSSRC(kSenderSsrc);
    // Transport settings.
    sender_.transport_.SetRtpRtcpModule(receiver_.impl_.get());
    receiver_.transport_.SetRtpRtcpModule(sender_.impl_.get());
  }
  SimulatedClock clock_;
  RtpRtcpModule sender_;
  RtpRtcpModule receiver_;
  VideoCodec codec_;

  void SendFrame(const RtpRtcpModule* module, uint8_t tid) {
    RTPVideoHeaderVP8 vp8_header = {};
    vp8_header.temporalIdx = tid;
    RTPVideoHeader rtp_video_header = {
        codec_.width, codec_.height, true, 0, kRtpVideoVp8, {vp8_header}};

    const uint8_t payload[100] = {0};
    EXPECT_EQ(0, module->impl_->SendOutgoingData(kVideoFrameKey,
                                                 codec_.plType,
                                                 0,
                                                 0,
                                                 payload,
                                                 sizeof(payload),
                                                 NULL,
                                                 &rtp_video_header));
  }

  void IncomingRtcpNack(const RtpRtcpModule* module, uint16_t sequence_number) {
    rtcp::Nack nack;
    uint16_t list[1];
    list[0] = sequence_number;
    const uint16_t kListLength = sizeof(list) / sizeof(list[0]);
    nack.From(kReceiverSsrc);
    nack.To(kSenderSsrc);
    nack.WithList(list, kListLength);
    rtcp::RawPacket packet = nack.Build();
    EXPECT_EQ(0, module->impl_->IncomingRtcpPacket(packet.buffer(),
                                                   packet.buffer_length()));
  }
};

TEST_F(RtpRtcpImplTest, SetSelectiveRetransmissions_BaseLayer) {
  sender_.impl_->SetSelectiveRetransmissions(kRetransmitBaseLayer);
  EXPECT_EQ(kRetransmitBaseLayer, sender_.impl_->SelectiveRetransmissions());

  // Send frames.
  EXPECT_EQ(0, sender_.RtpSent());
  SendFrame(&sender_, kBaseLayerTid);    // kSequenceNumber
  SendFrame(&sender_, kHigherLayerTid);  // kSequenceNumber + 1
  SendFrame(&sender_, kNoTemporalIdx);   // kSequenceNumber + 2
  EXPECT_EQ(3, sender_.RtpSent());
  EXPECT_EQ(kSequenceNumber + 2, sender_.LastRtpSequenceNumber());

  // Frame with kBaseLayerTid re-sent.
  IncomingRtcpNack(&sender_, kSequenceNumber);
  EXPECT_EQ(4, sender_.RtpSent());
  EXPECT_EQ(kSequenceNumber, sender_.LastRtpSequenceNumber());
  // Frame with kHigherLayerTid not re-sent.
  IncomingRtcpNack(&sender_, kSequenceNumber + 1);
  EXPECT_EQ(4, sender_.RtpSent());
  // Frame with kNoTemporalIdx re-sent.
  IncomingRtcpNack(&sender_, kSequenceNumber + 2);
  EXPECT_EQ(5, sender_.RtpSent());
  EXPECT_EQ(kSequenceNumber + 2, sender_.LastRtpSequenceNumber());
}

TEST_F(RtpRtcpImplTest, SetSelectiveRetransmissions_HigherLayers) {
  const uint8_t kSetting = kRetransmitBaseLayer + kRetransmitHigherLayers;
  sender_.impl_->SetSelectiveRetransmissions(kSetting);
  EXPECT_EQ(kSetting, sender_.impl_->SelectiveRetransmissions());

  // Send frames.
  EXPECT_EQ(0, sender_.RtpSent());
  SendFrame(&sender_, kBaseLayerTid);    // kSequenceNumber
  SendFrame(&sender_, kHigherLayerTid);  // kSequenceNumber + 1
  SendFrame(&sender_, kNoTemporalIdx);   // kSequenceNumber + 2
  EXPECT_EQ(3, sender_.RtpSent());
  EXPECT_EQ(kSequenceNumber + 2, sender_.LastRtpSequenceNumber());

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

TEST_F(RtpRtcpImplTest, Rtt) {
  RTPHeader header;
  header.timestamp = 1;
  header.sequenceNumber = 123;
  header.ssrc = kSenderSsrc;
  header.headerLength = 12;
  receiver_.receive_statistics_->IncomingPacket(header, 100, false);

  // Sender module should send a SR.
  EXPECT_EQ(0, sender_.impl_->SendRTCP(kRtcpReport));

  // Receiver module should send a RR with a response to the last received SR.
  clock_.AdvanceTimeMilliseconds(1000);
  EXPECT_EQ(0, receiver_.impl_->SendRTCP(kRtcpReport));

  // Verify RTT.
  uint16_t rtt;
  uint16_t avg_rtt;
  uint16_t min_rtt;
  uint16_t max_rtt;
  EXPECT_EQ(0,
      sender_.impl_->RTT(kReceiverSsrc, &rtt, &avg_rtt, &min_rtt, &max_rtt));
  EXPECT_EQ(2 * kOneWayNetworkDelayMs, rtt);
  EXPECT_EQ(2 * kOneWayNetworkDelayMs, avg_rtt);
  EXPECT_EQ(2 * kOneWayNetworkDelayMs, min_rtt);
  EXPECT_EQ(2 * kOneWayNetworkDelayMs, max_rtt);

  // No RTT from other ssrc.
  EXPECT_EQ(-1,
      sender_.impl_->RTT(kReceiverSsrc+1, &rtt, &avg_rtt, &min_rtt, &max_rtt));

  // Verify RTT from rtt_stats config.
  EXPECT_EQ(0U, sender_.rtt_stats_.LastProcessedRtt());
  EXPECT_EQ(0U, sender_.impl_->rtt_ms());
  sender_.impl_->Process();
  EXPECT_EQ(2 * kOneWayNetworkDelayMs, sender_.rtt_stats_.LastProcessedRtt());
  EXPECT_EQ(2 * kOneWayNetworkDelayMs, sender_.impl_->rtt_ms());
}

TEST_F(RtpRtcpImplTest, SetRtcpXrRrtrStatus) {
  EXPECT_FALSE(receiver_.impl_->RtcpXrRrtrStatus());
  receiver_.impl_->SetRtcpXrRrtrStatus(true);
  EXPECT_TRUE(receiver_.impl_->RtcpXrRrtrStatus());
}

TEST_F(RtpRtcpImplTest, RttForReceiverOnly) {
  receiver_.impl_->SetRtcpXrRrtrStatus(true);

  // Receiver module should send a Receiver time reference report (RTRR).
  EXPECT_EQ(0, receiver_.impl_->SendRTCP(kRtcpReport));

  // Sender module should send a response to the last received RTRR (DLRR).
  clock_.AdvanceTimeMilliseconds(1000);
  EXPECT_EQ(0, sender_.impl_->SendRTCP(kRtcpReport));

  // Verify RTT.
  EXPECT_EQ(0U, receiver_.rtt_stats_.LastProcessedRtt());
  EXPECT_EQ(0U, receiver_.impl_->rtt_ms());
  receiver_.impl_->Process();
  EXPECT_EQ(2 * kOneWayNetworkDelayMs, receiver_.rtt_stats_.LastProcessedRtt());
  EXPECT_EQ(2 * kOneWayNetworkDelayMs, receiver_.impl_->rtt_ms());
}

TEST_F(RtpRtcpImplTest, RtcpPacketTypeCounter_Nack) {
  EXPECT_EQ(0U, sender_.RtcpReceived().nack_packets);
  EXPECT_EQ(0U, receiver_.RtcpSent().nack_packets);
  // Receive module sends a NACK.
  const uint16_t kNackLength = 1;
  uint16_t nack_list[kNackLength] = {123};
  EXPECT_EQ(0, receiver_.impl_->SendNACK(nack_list, kNackLength));
  EXPECT_EQ(1U, receiver_.RtcpSent().nack_packets);

  // Send module receives the NACK.
  EXPECT_EQ(1U, sender_.RtcpReceived().nack_packets);
}

TEST_F(RtpRtcpImplTest, RtcpPacketTypeCounter_FirAndPli) {
  EXPECT_EQ(0U, sender_.RtcpReceived().fir_packets);
  EXPECT_EQ(0U, receiver_.RtcpSent().fir_packets);
  // Receive module sends a FIR.
  EXPECT_EQ(0, receiver_.impl_->SendRTCP(kRtcpFir));
  EXPECT_EQ(1U, receiver_.RtcpSent().fir_packets);
  // Send module receives the FIR.
  EXPECT_EQ(1U, sender_.RtcpReceived().fir_packets);

  // Receive module sends a FIR and PLI.
  EXPECT_EQ(0, receiver_.impl_->SendRTCP(kRtcpFir | kRtcpPli));
  EXPECT_EQ(2U, receiver_.RtcpSent().fir_packets);
  EXPECT_EQ(1U, receiver_.RtcpSent().pli_packets);
  // Send module receives the FIR and PLI.
  EXPECT_EQ(2U, sender_.RtcpReceived().fir_packets);
  EXPECT_EQ(1U, sender_.RtcpReceived().pli_packets);
}

TEST_F(RtpRtcpImplTest, UniqueNackRequests) {
  receiver_.transport_.SimulateNetworkDelay(0, &clock_);
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
  clock_.AdvanceTimeMilliseconds(kStartupRttMs + 1);
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

class RtpSendingTestTransport : public Transport {
 public:
  void ResetCounters() { bytes_received_.clear(); }

  virtual int SendPacket(int channel, const void* data, int length) OVERRIDE {
    RTPHeader header;
    scoped_ptr<RtpHeaderParser> parser(RtpHeaderParser::Create());
    EXPECT_TRUE(parser->Parse(static_cast<const uint8_t*>(data),
                              static_cast<size_t>(length),
                              &header));
    bytes_received_[header.ssrc] += length;
    ++packets_received_[header.ssrc];
    return length;
  }

  virtual int SendRTCPPacket(int channel,
                             const void* data,
                             int length) OVERRIDE {
    return length;
  }

  int GetPacketsReceived(uint32_t ssrc) const {
    std::map<uint32_t, int>::const_iterator it = packets_received_.find(ssrc);
    if (it == packets_received_.end())
      return 0;
    return it->second;
  }

  int GetBytesReceived(uint32_t ssrc) const {
    std::map<uint32_t, int>::const_iterator it = bytes_received_.find(ssrc);
    if (it == bytes_received_.end())
      return 0;
    return it->second;
  }

  int GetTotalBytesReceived() const {
    int sum = 0;
    for (std::map<uint32_t, int>::const_iterator it = bytes_received_.begin();
         it != bytes_received_.end();
         ++it) {
      sum += it->second;
    }
    return sum;
  }

 private:
  std::map<uint32_t, int> bytes_received_;
  std::map<uint32_t, int> packets_received_;
};

class RtpSendingTest : public ::testing::Test {
 protected:
  // Map from SSRC to number of received packets and bytes.
  typedef std::map<uint32_t, std::pair<int, int> > PaddingMap;

  RtpSendingTest() {
    // Send module.
    RtpRtcp::Configuration config;
    config.audio = false;
    config.clock = Clock::GetRealTimeClock();
    config.outgoing_transport = &transport_;
    config.receive_statistics = receive_statistics_.get();
    config.rtt_stats = &rtt_stats_;
    config.paced_sender = &pacer_;
    memset(&codec_, 0, sizeof(VideoCodec));
    codec_.plType = 100;
    strncpy(codec_.plName, "VP8", 3);
    codec_.numberOfSimulcastStreams = 3;
    codec_.simulcastStream[0].width = 320;
    codec_.simulcastStream[0].height = 180;
    codec_.simulcastStream[0].maxBitrate = 300;
    codec_.simulcastStream[1].width = 640;
    codec_.simulcastStream[1].height = 360;
    codec_.simulcastStream[1].maxBitrate = 600;
    codec_.simulcastStream[2].width = 1280;
    codec_.simulcastStream[2].height = 720;
    codec_.simulcastStream[2].maxBitrate = 1200;
    // We need numberOfSimulcastStreams + 1 RTP modules since we need one
    // default module.
    for (int i = 0; i < codec_.numberOfSimulcastStreams + 1; ++i) {
      RtpRtcp* sender = RtpRtcp::CreateRtpRtcp(config);
      EXPECT_EQ(0, sender->RegisterSendPayload(codec_));
      EXPECT_EQ(0, sender->SetSendingStatus(true));
      EXPECT_EQ(0, sender->SetSendingMediaStatus(true));
      sender->SetSSRC(kSenderSsrc + i);
      sender->SetRemoteSSRC(kReceiverSsrc + i);
      senders_.push_back(sender);
      config.default_module = senders_[0];
    }
    std::vector<uint32_t> bitrates;
    bitrates.push_back(codec_.simulcastStream[0].maxBitrate);
    bitrates.push_back(codec_.simulcastStream[1].maxBitrate);
    bitrates.push_back(codec_.simulcastStream[2].maxBitrate);
    senders_[0]->SetTargetSendBitrate(bitrates);
  }

  ~RtpSendingTest() {
    for (int i = senders_.size() - 1; i >= 0; --i) {
      delete senders_[i];
    }
  }

  void SendFrameOnSender(int sender_index,
                         const uint8_t* payload,
                         size_t length) {
    RTPVideoHeader rtp_video_header = {
        codec_.simulcastStream[sender_index].width,
        codec_.simulcastStream[sender_index].height,
        true,
        0,
        kRtpVideoVp8,
        {}};
    uint32_t seq_num = 0;
    uint32_t ssrc = 0;
    int64_t capture_time_ms = 0;
    bool retransmission = false;
    EXPECT_CALL(pacer_, SendPacket(_, _, _, _, _, _))
        .WillRepeatedly(DoAll(SaveArg<1>(&ssrc),
                              SaveArg<2>(&seq_num),
                              SaveArg<3>(&capture_time_ms),
                              SaveArg<5>(&retransmission),
                              Return(true)));
    EXPECT_EQ(0,
              senders_[sender_index]->SendOutgoingData(kVideoFrameKey,
                                                       codec_.plType,
                                                       0,
                                                       0,
                                                       payload,
                                                       length,
                                                       NULL,
                                                       &rtp_video_header));
    EXPECT_TRUE(senders_[sender_index]->TimeToSendPacket(
        ssrc, seq_num, capture_time_ms, retransmission));
  }

  void ExpectPadding(const PaddingMap& expected_padding) {
    int expected_total_bytes = 0;
    for (PaddingMap::const_iterator it = expected_padding.begin();
         it != expected_padding.end();
         ++it) {
      int packets_received = transport_.GetBytesReceived(it->first);
      if (it->second.first > 0) {
        EXPECT_GE(packets_received, it->second.first)
            << "On SSRC: " << it->first;
      }
      int bytes_received = transport_.GetBytesReceived(it->first);
      expected_total_bytes += bytes_received;
      if (it->second.second > 0) {
        EXPECT_GE(bytes_received, it->second.second)
            << "On SSRC: " << it->first;
      } else {
        EXPECT_EQ(0, bytes_received) << "On SSRC: " << it->first;
      }
    }
    EXPECT_EQ(expected_total_bytes, transport_.GetTotalBytesReceived());
  }

  scoped_ptr<ReceiveStatistics> receive_statistics_;
  RtcpRttStatsTestImpl rtt_stats_;
  std::vector<RtpRtcp*> senders_;
  RtpSendingTestTransport transport_;
  NiceMock<MockPacedSender> pacer_;
  VideoCodec codec_;
};

TEST_F(RtpSendingTest, DISABLED_RoundRobinPadding) {
  // We have to send on an SSRC to be allowed to pad, since a marker bit must
  // be sent prior to padding packets.
  const uint8_t payload[200] = {0};
  for (int i = 0; i < codec_.numberOfSimulcastStreams; ++i) {
    SendFrameOnSender(i + 1, payload, sizeof(payload));
  }
  transport_.ResetCounters();
  senders_[0]->TimeToSendPadding(500);
  PaddingMap expected_padding;
  expected_padding[kSenderSsrc + 1] = std::make_pair(2, 500);
  expected_padding[kSenderSsrc + 2] = std::make_pair(0, 0);
  expected_padding[kSenderSsrc + 3] = std::make_pair(0, 0);
  ExpectPadding(expected_padding);
  senders_[0]->TimeToSendPadding(1000);
  expected_padding[kSenderSsrc + 2] = std::make_pair(4, 1000);
  ExpectPadding(expected_padding);
  senders_[0]->TimeToSendPadding(1500);
  expected_padding[kSenderSsrc + 3] = std::make_pair(6, 1500);
  ExpectPadding(expected_padding);
}

TEST_F(RtpSendingTest, DISABLED_RoundRobinPaddingRtx) {
  // Enable RTX to allow padding to be sent prior to media.
  for (int i = 1; i < codec_.numberOfSimulcastStreams + 1; ++i) {
    // Abs-send-time is needed to be allowed to send padding prior to media,
    // as otherwise the timestmap used for BWE will be broken.
    senders_[i]->RegisterSendRtpHeaderExtension(kRtpExtensionAbsoluteSendTime,
                                                1);
    senders_[i]->SetRtxSendPayloadType(96);
    senders_[i]->SetRtxSsrc(kSenderRtxSsrc + i);
    senders_[i]->SetRTXSendStatus(kRtxRetransmitted);
  }
  transport_.ResetCounters();
  senders_[0]->TimeToSendPadding(500);
  PaddingMap expected_padding;
  expected_padding[kSenderSsrc + 1] = std::make_pair(0, 0);
  expected_padding[kSenderSsrc + 2] = std::make_pair(0, 0);
  expected_padding[kSenderSsrc + 3] = std::make_pair(0, 0);
  expected_padding[kSenderRtxSsrc + 1] = std::make_pair(2, 500);
  expected_padding[kSenderRtxSsrc + 2] = std::make_pair(0, 0);
  expected_padding[kSenderRtxSsrc + 3] = std::make_pair(0, 0);
  ExpectPadding(expected_padding);
  senders_[0]->TimeToSendPadding(1000);
  expected_padding[kSenderRtxSsrc + 2] = std::make_pair(4, 500);
  ExpectPadding(expected_padding);
  senders_[0]->TimeToSendPadding(1500);

  expected_padding[kSenderRtxSsrc + 3] = std::make_pair(6, 500);
  ExpectPadding(expected_padding);
}

TEST_F(RtpSendingTest, DISABLED_RoundRobinPaddingRtxRedundantPayloads) {
  for (int i = 1; i < codec_.numberOfSimulcastStreams + 1; ++i) {
    senders_[i]->SetRtxSendPayloadType(96);
    senders_[i]->SetRtxSsrc(kSenderRtxSsrc + i);
    senders_[i]->SetRTXSendStatus(kRtxRetransmitted | kRtxRedundantPayloads);
    senders_[i]->SetStorePacketsStatus(true, 100);
  }
  // First send payloads so that we have something to retransmit.
  const size_t kPayloadSize = 500;
  const uint8_t payload[kPayloadSize] = {0};
  for (int i = 0; i < codec_.numberOfSimulcastStreams; ++i) {
    SendFrameOnSender(i + 1, payload, sizeof(payload));
  }
  transport_.ResetCounters();
  senders_[0]->TimeToSendPadding(500);
  PaddingMap expected_padding;
  expected_padding[kSenderSsrc + 1] = std::make_pair<int, int>(0, 0);
  expected_padding[kSenderSsrc + 2] = std::make_pair<int, int>(0, 0);
  expected_padding[kSenderSsrc + 3] = std::make_pair<int, int>(0, 0);
  expected_padding[kSenderRtxSsrc + 1] = std::make_pair<int, int>(1, 500);
  expected_padding[kSenderRtxSsrc + 2] = std::make_pair<int, int>(0, 0);
  expected_padding[kSenderRtxSsrc + 3] = std::make_pair<int, int>(0, 0);
  ExpectPadding(expected_padding);
  senders_[0]->TimeToSendPadding(1000);
  expected_padding[kSenderRtxSsrc + 2] = std::make_pair<int, int>(2, 1000);
  ExpectPadding(expected_padding);
  senders_[0]->TimeToSendPadding(1500);
  expected_padding[kSenderRtxSsrc + 3] = std::make_pair<int, int>(3, 1500);
  ExpectPadding(expected_padding);
}
}  // namespace webrtc
