/*
*  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
*
*  Use of this source code is governed by a BSD-style license
*  that can be found in the LICENSE file in the root of the source
*  tree. An additional intellectual property rights grant can be found
*  in the file PATENTS.  All contributing project authors may
*  be found in the AUTHORS file in the root of the source tree.
*/

#include <algorithm>
#include <iterator>
#include <list>
#include <set>

#include "testing/gtest/include/gtest/gtest.h"
#include "webrtc/common_types.h"
#include "webrtc/modules/rtp_rtcp/interface/receive_statistics.h"
#include "webrtc/modules/rtp_rtcp/interface/rtp_header_parser.h"
#include "webrtc/modules/rtp_rtcp/interface/rtp_payload_registry.h"
#include "webrtc/modules/rtp_rtcp/interface/rtp_receiver.h"
#include "webrtc/modules/rtp_rtcp/interface/rtp_rtcp.h"
#include "webrtc/modules/rtp_rtcp/interface/rtp_rtcp_defines.h"
#include "webrtc/system_wrappers/interface/scoped_ptr.h"

using namespace webrtc;

const int kVideoNackListSize = 30;
const int kTestId = 123;
const uint32_t kTestSsrc = 3456;
const uint16_t kTestSequenceNumber = 2345;
const uint32_t kTestNumberOfPackets = 1350;
const int kTestNumberOfRtxPackets = 149;
const int kNumFrames = 30;

class VerifyingRtxReceiver : public NullRtpData
{
 public:
  VerifyingRtxReceiver() {}

  virtual int32_t OnReceivedPayloadData(
      const uint8_t* data,
      const uint16_t size,
      const webrtc::WebRtcRTPHeader* rtp_header) {
    if (!sequence_numbers_.empty())
      EXPECT_EQ(kTestSsrc, rtp_header->header.ssrc);
    sequence_numbers_.push_back(rtp_header->header.sequenceNumber);
    return 0;
  }
  std::list<uint16_t> sequence_numbers_;
};

class TestRtpFeedback : public NullRtpFeedback {
 public:
  TestRtpFeedback(RtpRtcp* rtp_rtcp) : rtp_rtcp_(rtp_rtcp) {}
  virtual ~TestRtpFeedback() {}

  virtual void OnIncomingSSRCChanged(const int32_t id,
                                     const uint32_t ssrc) {
    rtp_rtcp_->SetRemoteSSRC(ssrc);
  }

 private:
  RtpRtcp* rtp_rtcp_;
};

class RtxLoopBackTransport : public webrtc::Transport {
 public:
  explicit RtxLoopBackTransport(uint32_t rtx_ssrc)
      : count_(0),
        packet_loss_(0),
        consecutive_drop_start_(0),
        consecutive_drop_end_(0),
        rtx_ssrc_(rtx_ssrc),
        count_rtx_ssrc_(0),
        rtp_payload_registry_(NULL),
        rtp_receiver_(NULL),
        module_(NULL) {}

  void SetSendModule(RtpRtcp* rtpRtcpModule,
                     RTPPayloadRegistry* rtp_payload_registry,
                     RtpReceiver* receiver) {
    module_ = rtpRtcpModule;
    rtp_payload_registry_ = rtp_payload_registry;
    rtp_receiver_ = receiver;
  }

  void DropEveryNthPacket(int n) {
    packet_loss_ = n;
  }

  void DropConsecutivePackets(int start, int total) {
    consecutive_drop_start_ = start;
    consecutive_drop_end_ = start + total;
    packet_loss_ = 0;
  }

  virtual int SendPacket(int channel, const void *data, int len) {
    count_++;
    const unsigned char* ptr = static_cast<const unsigned  char*>(data);
    uint32_t ssrc = (ptr[8] << 24) + (ptr[9] << 16) + (ptr[10] << 8) + ptr[11];
    if (ssrc == rtx_ssrc_) count_rtx_ssrc_++;
    uint16_t sequence_number = (ptr[2] << 8) + ptr[3];
    expected_sequence_numbers_.insert(expected_sequence_numbers_.end(),
        sequence_number);
    if (packet_loss_ > 0) {
      if ((count_ % packet_loss_) == 0) {
        return len;
      }
    } else if (count_ >= consecutive_drop_start_ &&
        count_ < consecutive_drop_end_) {
      return len;
    }
    int packet_length = len;
    uint8_t restored_packet[1500];
    uint8_t* restored_packet_ptr = restored_packet;
    RTPHeader header;
    scoped_ptr<RtpHeaderParser> parser(RtpHeaderParser::Create());
    if (!parser->Parse(ptr, len, &header)) {
      return -1;
    }
    if (rtp_payload_registry_->IsRtx(header)) {
      // Remove the RTX header and parse the original RTP header.
      EXPECT_TRUE(rtp_payload_registry_->RestoreOriginalPacket(
          &restored_packet_ptr, ptr, &packet_length, rtp_receiver_->SSRC(),
          header));
      if (!parser->Parse(restored_packet_ptr, packet_length, &header)) {
        return -1;
      }
    }
    restored_packet_ptr += header.headerLength;
    packet_length -= header.headerLength;
    PayloadUnion payload_specific;
    if (!rtp_payload_registry_->GetPayloadSpecifics(header.payloadType,
                                                    &payload_specific)) {
      return -1;
    }
    if (!rtp_receiver_->IncomingRtpPacket(header, restored_packet_ptr,
                                          packet_length, payload_specific,
                                          true)) {
      return -1;
    }
    return len;
  }

  virtual int SendRTCPPacket(int channel, const void *data, int len) {
    if (module_->IncomingRtcpPacket((const uint8_t*)data, len) == 0) {
      return len;
    }
    return -1;
  }
  int count_;
  int packet_loss_;
  int consecutive_drop_start_;
  int consecutive_drop_end_;
  uint32_t rtx_ssrc_;
  int count_rtx_ssrc_;
  RTPPayloadRegistry* rtp_payload_registry_;
  RtpReceiver* rtp_receiver_;
  RtpRtcp* module_;
  std::set<uint16_t> expected_sequence_numbers_;
};

class RtpRtcpRtxNackTest : public ::testing::Test {
 protected:
  RtpRtcpRtxNackTest()
      : rtp_payload_registry_(0, RTPPayloadStrategy::CreateStrategy(false)),
        rtp_rtcp_module_(NULL),
        transport_(kTestSsrc + 1),
        receiver_(),
        payload_data_length(sizeof(payload_data)),
        fake_clock(123456) {}
  ~RtpRtcpRtxNackTest() {}

  virtual void SetUp() {
    RtpRtcp::Configuration configuration;
    configuration.id = kTestId;
    configuration.audio = false;
    configuration.clock = &fake_clock;
    receive_statistics_.reset(ReceiveStatistics::Create(&fake_clock));
    configuration.receive_statistics = receive_statistics_.get();
    configuration.outgoing_transport = &transport_;
    rtp_rtcp_module_ = RtpRtcp::CreateRtpRtcp(configuration);

    rtp_feedback_.reset(new TestRtpFeedback(rtp_rtcp_module_));

    rtp_receiver_.reset(RtpReceiver::CreateVideoReceiver(
        kTestId, &fake_clock, &receiver_, rtp_feedback_.get(),
        &rtp_payload_registry_));

    EXPECT_EQ(0, rtp_rtcp_module_->SetSSRC(kTestSsrc));
    EXPECT_EQ(0, rtp_rtcp_module_->SetRTCPStatus(kRtcpCompound));
    rtp_receiver_->SetNACKStatus(kNackRtcp);
    EXPECT_EQ(0, rtp_rtcp_module_->SetStorePacketsStatus(true, 600));
    EXPECT_EQ(0, rtp_rtcp_module_->SetSendingStatus(true));
    EXPECT_EQ(0, rtp_rtcp_module_->SetSequenceNumber(kTestSequenceNumber));
    EXPECT_EQ(0, rtp_rtcp_module_->SetStartTimestamp(111111));

    transport_.SetSendModule(rtp_rtcp_module_, &rtp_payload_registry_,
                             rtp_receiver_.get());

    VideoCodec video_codec;
    memset(&video_codec, 0, sizeof(video_codec));
    video_codec.plType = 123;
    memcpy(video_codec.plName, "I420", 5);

    EXPECT_EQ(0, rtp_rtcp_module_->RegisterSendPayload(video_codec));
    EXPECT_EQ(0, rtp_receiver_->RegisterReceivePayload(video_codec.plName,
                                                       video_codec.plType,
                                                       90000,
                                                       0,
                                                       video_codec.maxBitrate));

    for (int n = 0; n < payload_data_length; n++) {
      payload_data[n] = n % 10;
    }
  }

  int BuildNackList(uint16_t* nack_list) {
    receiver_.sequence_numbers_.sort();
    std::list<uint16_t> missing_sequence_numbers;
    std::list<uint16_t>::iterator it =
        receiver_.sequence_numbers_.begin();

    while (it != receiver_.sequence_numbers_.end()) {
      uint16_t sequence_number_1 = *it;
      ++it;
      if (it != receiver_.sequence_numbers_.end()) {
        uint16_t sequence_number_2 = *it;
        // Add all missing sequence numbers to list
        for (uint16_t i = sequence_number_1 + 1; i < sequence_number_2;
            ++i) {
          missing_sequence_numbers.push_back(i);
        }
      }
    }
    int n = 0;
    for (it = missing_sequence_numbers.begin();
        it != missing_sequence_numbers.end(); ++it) {
      nack_list[n++] = (*it);
    }
    return n;
  }

  bool ExpectedPacketsReceived() {
    std::list<uint16_t> received_sorted;
    std::copy(receiver_.sequence_numbers_.begin(),
              receiver_.sequence_numbers_.end(),
              std::back_inserter(received_sorted));
    received_sorted.sort();
    return std::equal(received_sorted.begin(), received_sorted.end(),
                      transport_.expected_sequence_numbers_.begin());
  }

  void RunRtxTest(RtxMode rtx_method, int loss) {
    rtp_payload_registry_.SetRtxStatus(true, kTestSsrc + 1);
    EXPECT_EQ(0, rtp_rtcp_module_->SetRTXSendStatus(rtx_method, true,
        kTestSsrc + 1));
    transport_.DropEveryNthPacket(loss);
    uint32_t timestamp = 3000;
    uint16_t nack_list[kVideoNackListSize];
    for (int frame = 0; frame < kNumFrames; ++frame) {
      EXPECT_EQ(0, rtp_rtcp_module_->SendOutgoingData(webrtc::kVideoFrameDelta,
                                                      123,
                                                      timestamp,
                                                      timestamp / 90,
                                                      payload_data,
                                                      payload_data_length));
      int length = BuildNackList(nack_list);
      if (length > 0)
        rtp_rtcp_module_->SendNACK(nack_list, length);
      fake_clock.AdvanceTimeMilliseconds(33);
      rtp_rtcp_module_->Process();
      // Prepare next frame.
      timestamp += 3000;
    }
    receiver_.sequence_numbers_.sort();
  }

  virtual void TearDown() {
    delete rtp_rtcp_module_;
  }

  scoped_ptr<ReceiveStatistics> receive_statistics_;
  RTPPayloadRegistry rtp_payload_registry_;
  scoped_ptr<RtpReceiver> rtp_receiver_;
  RtpRtcp* rtp_rtcp_module_;
  scoped_ptr<TestRtpFeedback> rtp_feedback_;
  RtxLoopBackTransport transport_;
  VerifyingRtxReceiver receiver_;
  uint8_t  payload_data[65000];
  int payload_data_length;
  SimulatedClock fake_clock;
};

TEST_F(RtpRtcpRtxNackTest, LongNackList) {
  const int kNumPacketsToDrop = 900;
  const int kNumRequiredRtcp = 4;
  uint32_t timestamp = 3000;
  uint16_t nack_list[kNumPacketsToDrop];
  // Disable StorePackets to be able to set a larger packet history.
  EXPECT_EQ(0, rtp_rtcp_module_->SetStorePacketsStatus(false, 0));
  // Enable StorePackets with a packet history of 2000 packets.
  EXPECT_EQ(0, rtp_rtcp_module_->SetStorePacketsStatus(true, 2000));
  // Drop 900 packets from the second one so that we get a NACK list which is
  // big enough to require 4 RTCP packets to be fully transmitted to the sender.
  transport_.DropConsecutivePackets(2, kNumPacketsToDrop);
  // Send 30 frames which at the default size is roughly what we need to get
  // enough packets.
  for (int frame = 0; frame < kNumFrames; ++frame) {
    EXPECT_EQ(0, rtp_rtcp_module_->SendOutgoingData(webrtc::kVideoFrameDelta,
                                                    123,
                                                    timestamp,
                                                    timestamp / 90,
                                                    payload_data,
                                                    payload_data_length));
    // Prepare next frame.
    timestamp += 3000;
    fake_clock.AdvanceTimeMilliseconds(33);
    rtp_rtcp_module_->Process();
  }
  EXPECT_FALSE(transport_.expected_sequence_numbers_.empty());
  EXPECT_FALSE(receiver_.sequence_numbers_.empty());
  size_t last_receive_count = receiver_.sequence_numbers_.size();
  int length = BuildNackList(nack_list);
  for (int i = 0; i < kNumRequiredRtcp - 1; ++i) {
    rtp_rtcp_module_->SendNACK(nack_list, length);
    EXPECT_GT(receiver_.sequence_numbers_.size(), last_receive_count);
    last_receive_count = receiver_.sequence_numbers_.size();
    EXPECT_FALSE(ExpectedPacketsReceived());
  }
  rtp_rtcp_module_->SendNACK(nack_list, length);
  EXPECT_GT(receiver_.sequence_numbers_.size(), last_receive_count);
  EXPECT_TRUE(ExpectedPacketsReceived());
}

TEST_F(RtpRtcpRtxNackTest, RtxNack) {
  RunRtxTest(kRtxRetransmitted, 10);
  EXPECT_EQ(kTestSequenceNumber, *(receiver_.sequence_numbers_.begin()));
  EXPECT_EQ(kTestSequenceNumber + kTestNumberOfPackets - 1,
      *(receiver_.sequence_numbers_.rbegin()));
  EXPECT_EQ(kTestNumberOfPackets, receiver_.sequence_numbers_.size());
  EXPECT_EQ(kTestNumberOfRtxPackets, transport_.count_rtx_ssrc_);
  EXPECT_TRUE(ExpectedPacketsReceived());
}
