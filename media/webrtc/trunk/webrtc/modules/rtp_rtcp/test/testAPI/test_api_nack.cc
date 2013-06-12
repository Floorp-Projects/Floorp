/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
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
#include <gtest/gtest.h>

#include "test_api.h"

#include "common_types.h"
#include "rtp_rtcp.h"
#include "rtp_rtcp_defines.h"

using namespace webrtc;

const int kVideoNackListSize = 10;
const int kTestId = 123;
const uint32_t kTestSsrc = 3456;
const uint16_t kTestSequenceNumber = 2345;
const uint32_t kTestNumberOfPackets = 450;
const int kTestNumberOfRtxPackets = 49;

class VerifyingNackReceiver : public RtpData
{
 public:
  VerifyingNackReceiver() {}

  virtual int32_t OnReceivedPayloadData(
      const uint8_t* data,
      const uint16_t size,
      const webrtc::WebRtcRTPHeader* rtp_header) {

    EXPECT_EQ(kTestSsrc, rtp_header->header.ssrc);
    bool already_received = std::find(
        sequence_numbers_.begin(), sequence_numbers_.end(),
        rtp_header->header.sequenceNumber) != sequence_numbers_.end();
    EXPECT_FALSE(already_received);
    sequence_numbers_.push_back(rtp_header->header.sequenceNumber);
    return 0;
  }
  std::list<uint16_t> sequence_numbers_;
};

class NackLoopBackTransport : public webrtc::Transport {
 public:
  NackLoopBackTransport(uint32_t rtx_ssrc)
    : count_(0),
      packet_loss_(0),
      consecutive_drop_start_(0),
      consecutive_drop_end_(0),
      rtx_ssrc_(rtx_ssrc),
      count_rtx_ssrc_(0),
      module_(NULL) {
  }
  void SetSendModule(RtpRtcp* rtpRtcpModule) {
    module_ = rtpRtcpModule;
  }
  void DropEveryNthPacket(int n) {
    packet_loss_ = n;
    consecutive_drop_start_ = 0;
    consecutive_drop_end_ = 0;
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
    if (module_->IncomingPacket((const uint8_t*)data, len) == 0) {
      return len;
    }
    return -1;
  }
  virtual int SendRTCPPacket(int channel, const void *data, int len) {
    if (module_->IncomingPacket((const uint8_t*)data, len) == 0) {
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
  RtpRtcp* module_;
  std::set<uint16_t> expected_sequence_numbers_;
};

class RtpRtcpNackTest : public ::testing::Test {
 protected:
  RtpRtcpNackTest()
      : video_module_(NULL),
        transport_(NULL),
        nack_receiver_(NULL),
        payload_data_length(sizeof(payload_data)),
        fake_clock(123456) {}
  ~RtpRtcpNackTest() {}

  virtual void SetUp() {
    transport_ = new NackLoopBackTransport(kTestSsrc + 1);
    nack_receiver_ = new VerifyingNackReceiver();

    RtpRtcp::Configuration configuration;
    configuration.id = kTestId;
    configuration.audio = false;
    configuration.clock = &fake_clock;
    configuration.incoming_data = nack_receiver_;
    configuration.outgoing_transport = transport_;
    video_module_ = RtpRtcp::CreateRtpRtcp(configuration);

    EXPECT_EQ(0, video_module_->SetRTCPStatus(kRtcpCompound));
    EXPECT_EQ(0, video_module_->SetSSRC(kTestSsrc));
    EXPECT_EQ(0, video_module_->SetNACKStatus(kNackRtcp, 450));
    EXPECT_EQ(0, video_module_->SetStorePacketsStatus(true, 600));
    EXPECT_EQ(0, video_module_->SetSendingStatus(true));
    EXPECT_EQ(0, video_module_->SetSequenceNumber(kTestSequenceNumber));
    EXPECT_EQ(0, video_module_->SetStartTimestamp(111111));

    transport_->SetSendModule(video_module_);

    VideoCodec video_codec;
    memset(&video_codec, 0, sizeof(video_codec));
    video_codec.plType = 123;
    memcpy(video_codec.plName, "I420", 5);

    EXPECT_EQ(0, video_module_->RegisterSendPayload(video_codec));
    EXPECT_EQ(0, video_module_->RegisterReceivePayload(video_codec));

    for (int n = 0; n < payload_data_length; n++) {
      payload_data[n] = n % 10;
    }
  }

  virtual void TearDown() {
    delete video_module_;
    delete transport_;
    delete nack_receiver_;
  }

  int BuildNackList(uint16_t* nack_list) const {
    nack_receiver_->sequence_numbers_.sort();

    std::list<uint16_t> missing_sequence_numbers;
    std::list<uint16_t>::iterator it =
        nack_receiver_->sequence_numbers_.begin();

    while (it != nack_receiver_->sequence_numbers_.end()) {
      uint16_t sequence_number_1 = *it;
      ++it;
      if (it != nack_receiver_->sequence_numbers_.end()) {
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
    std::copy(nack_receiver_->sequence_numbers_.begin(),
              nack_receiver_->sequence_numbers_.end(),
              std::back_inserter(received_sorted));
    received_sorted.sort();
    return std::equal(received_sorted.begin(), received_sorted.end(),
                      transport_->expected_sequence_numbers_.begin());
  }

  RtpRtcp* video_module_;
  NackLoopBackTransport* transport_;
  VerifyingNackReceiver* nack_receiver_;
  uint8_t  payload_data[65000];
  int payload_data_length;
  SimulatedClock fake_clock;
};

TEST_F(RtpRtcpNackTest, RTCP) {
  uint32_t timestamp = 3000;
  uint16_t nack_list[kVideoNackListSize];
  transport_->DropEveryNthPacket(10);

  for (int frame = 0; frame < 10; ++frame) {
    EXPECT_EQ(0, video_module_->SendOutgoingData(webrtc::kVideoFrameDelta, 123,
                                                timestamp,
                                                timestamp / 90,
                                                payload_data,
                                                payload_data_length));

    int length = BuildNackList(nack_list);
    video_module_->SendNACK(nack_list, length);
    fake_clock.AdvanceTimeMilliseconds(33);
    video_module_->Process();

    // Prepare next frame.
    timestamp += 3000;
  }
  nack_receiver_->sequence_numbers_.sort();
  EXPECT_EQ(kTestSequenceNumber, *(nack_receiver_->sequence_numbers_.begin()));
  EXPECT_EQ(kTestSequenceNumber + kTestNumberOfPackets - 1,
            *(nack_receiver_->sequence_numbers_.rbegin()));
  EXPECT_EQ(kTestNumberOfPackets, nack_receiver_->sequence_numbers_.size());
  EXPECT_EQ(0, transport_->count_rtx_ssrc_);
}

TEST_F(RtpRtcpNackTest, LongNackList) {
  const int kNumPacketsToDrop = 900;
  const int kNumFrames = 30;
  const int kNumRequiredRtcp = 4;
  uint32_t timestamp = 3000;
  uint16_t nack_list[kNumPacketsToDrop];
  // Disable StorePackets to be able to set a larger packet history.
  EXPECT_EQ(0, video_module_->SetStorePacketsStatus(false, 0));
  // Enable StorePackets with a packet history of 2000 packets.
  EXPECT_EQ(0, video_module_->SetStorePacketsStatus(true, 2000));
  // Drop 900 packets from the second one so that we get a NACK list which is
  // big enough to require 4 RTCP packets to be fully transmitted to the sender.
  transport_->DropConsecutivePackets(2, kNumPacketsToDrop);
  // Send 30 frames which at the default size is roughly what we need to get
  // enough packets.
  for (int frame = 0; frame < kNumFrames; ++frame) {
    EXPECT_EQ(0, video_module_->SendOutgoingData(webrtc::kVideoFrameDelta, 123,
                                                 timestamp,
                                                 timestamp / 90,
                                                 payload_data,
                                                 payload_data_length));
    // Prepare next frame.
    timestamp += 3000;
    fake_clock.AdvanceTimeMilliseconds(33);
    video_module_->Process();
  }
  EXPECT_FALSE(transport_->expected_sequence_numbers_.empty());
  EXPECT_FALSE(nack_receiver_->sequence_numbers_.empty());
  size_t last_receive_count = nack_receiver_->sequence_numbers_.size();
  int length = BuildNackList(nack_list);
  for (int i = 0; i < kNumRequiredRtcp - 1; ++i) {
    video_module_->SendNACK(nack_list, length);
    EXPECT_GT(nack_receiver_->sequence_numbers_.size(), last_receive_count);
    last_receive_count = nack_receiver_->sequence_numbers_.size();
    EXPECT_FALSE(ExpectedPacketsReceived());
  }
  video_module_->SendNACK(nack_list, length);
  EXPECT_GT(nack_receiver_->sequence_numbers_.size(), last_receive_count);
  EXPECT_TRUE(ExpectedPacketsReceived());
}
