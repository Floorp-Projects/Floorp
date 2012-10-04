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
#include <vector>
#include <gtest/gtest.h>

#include "test_api.h"

#include "common_types.h"
#include "rtp_rtcp.h"
#include "rtp_rtcp_defines.h"

using namespace webrtc;

const int kVideoNackListSize = 10;
const int kTestId = 123;
const WebRtc_UWord32 kTestSsrc = 3456;
const WebRtc_UWord16 kTestSequenceNumber = 2345;
const WebRtc_UWord32 kTestNumberOfPackets = 450;
const int kTestNumberOfRtxPackets = 49;

class VerifyingNackReceiver : public RtpData
{
 public:
  VerifyingNackReceiver() {}

  virtual WebRtc_Word32 OnReceivedPayloadData(
      const WebRtc_UWord8* data,
      const WebRtc_UWord16 size,
      const webrtc::WebRtcRTPHeader* rtp_header) {

    EXPECT_EQ(kTestSsrc, rtp_header->header.ssrc);
    EXPECT_EQ(std::find(sequence_numbers_.begin(),
                    sequence_numbers_.end(),
                    rtp_header->header.sequenceNumber),
              sequence_numbers_.end());
    sequence_numbers_.push_back(rtp_header->header.sequenceNumber);
    return 0;
  }
  std::vector<WebRtc_UWord16 > sequence_numbers_;
};

class NackLoopBackTransport : public webrtc::Transport {
 public:
  NackLoopBackTransport(uint32_t rtx_ssrc)
    : count_(0),
      packet_loss_(0),
      rtx_ssrc_(rtx_ssrc),
      count_rtx_ssrc_(0),
      module_(NULL) {
  }
  void SetSendModule(RtpRtcp* rtpRtcpModule) {
    module_ = rtpRtcpModule;
  }
  void DropEveryNthPacket(int n) {
    packet_loss_ = n;
  }
  virtual int SendPacket(int channel, const void *data, int len) {
    count_++;
    const unsigned char* ptr = static_cast<const unsigned  char*>(data);
    uint32_t ssrc = (ptr[8] << 24) + (ptr[9] << 16) + (ptr[10] << 8) + ptr[11];
    if (ssrc == rtx_ssrc_) count_rtx_ssrc_++;

    if (packet_loss_ > 0) {
      if ((count_ % packet_loss_) == 0) {
        return len;
      }
    }
    if (module_->IncomingPacket((const WebRtc_UWord8*)data, len) == 0) {
      return len;
    }
    return -1;
  }
  virtual int SendRTCPPacket(int channel, const void *data, int len) {
    if (module_->IncomingPacket((const WebRtc_UWord8*)data, len) == 0) {
      return len;
    }
    return -1;
  }
  int count_;
  int packet_loss_;
  uint32_t rtx_ssrc_;
  int count_rtx_ssrc_;
  RtpRtcp* module_;
};

class RtpRtcpNackTest : public ::testing::Test {
 protected:
  RtpRtcpNackTest() {}
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
    EXPECT_EQ(0, video_module_->SetNACKStatus(kNackRtcp));
    EXPECT_EQ(0, video_module_->SetStorePacketsStatus(true));
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

    payload_data_length = sizeof(payload_data);

    for (int n = 0; n < payload_data_length; n++) {
      payload_data[n] = n % 10;
    }
  }

  virtual void TearDown() {
    delete video_module_;
    delete transport_;
    delete nack_receiver_;
  }

  RtpRtcp* video_module_;
  NackLoopBackTransport* transport_;
  VerifyingNackReceiver* nack_receiver_;
  WebRtc_UWord8  payload_data[65000];
  int payload_data_length;
  FakeRtpRtcpClock fake_clock;
};

TEST_F(RtpRtcpNackTest, RTCP) {
  WebRtc_UWord32 timestamp = 3000;
  WebRtc_UWord16 nack_list[kVideoNackListSize];
  transport_->DropEveryNthPacket(10);

  for (int frame = 0; frame < 10; ++frame) {
    EXPECT_EQ(0, video_module_->SendOutgoingData(webrtc::kVideoFrameDelta, 123,
                                                timestamp,
                                                timestamp / 90,
                                                payload_data,
                                                payload_data_length));

    std::sort(nack_receiver_->sequence_numbers_.begin(),
              nack_receiver_->sequence_numbers_.end());

    std::vector<WebRtc_UWord16> missing_sequence_numbers;
    std::vector<WebRtc_UWord16>::iterator it =
        nack_receiver_->sequence_numbers_.begin();

    while (it != nack_receiver_->sequence_numbers_.end()) {
      WebRtc_UWord16 sequence_number_1 = *it;
      ++it;
      if (it != nack_receiver_->sequence_numbers_.end()) {
        WebRtc_UWord16 sequence_number_2 = *it;
        // Add all missing sequence numbers to list
        for (WebRtc_UWord16 i = sequence_number_1 + 1; i < sequence_number_2;
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
    video_module_->SendNACK(nack_list, n);
    fake_clock.IncrementTime(33);
    video_module_->Process();

    // Prepare next frame.
    timestamp += 3000;
  }
  std::sort(nack_receiver_->sequence_numbers_.begin(),
            nack_receiver_->sequence_numbers_.end());
  EXPECT_EQ(kTestSequenceNumber, *(nack_receiver_->sequence_numbers_.begin()));
  EXPECT_EQ(kTestSequenceNumber + kTestNumberOfPackets - 1,
            *(nack_receiver_->sequence_numbers_.rbegin()));
  EXPECT_EQ(kTestNumberOfPackets, nack_receiver_->sequence_numbers_.size());
  EXPECT_EQ(0, transport_->count_rtx_ssrc_);
}

TEST_F(RtpRtcpNackTest, RTX) {
  EXPECT_EQ(0, video_module_->SetRTXReceiveStatus(true, kTestSsrc + 1));
  EXPECT_EQ(0, video_module_->SetRTXSendStatus(true, true, kTestSsrc + 1));

  transport_->DropEveryNthPacket(10);

  WebRtc_UWord32 timestamp = 3000;
  WebRtc_UWord16 nack_list[kVideoNackListSize];

  for (int frame = 0; frame < 10; ++frame) {
    EXPECT_EQ(0, video_module_->SendOutgoingData(webrtc::kVideoFrameDelta,
                                                 123,
                                                 timestamp,
                                                 timestamp / 90,
                                                 payload_data,
                                                 payload_data_length));

    std::sort(nack_receiver_->sequence_numbers_.begin(),
              nack_receiver_->sequence_numbers_.end());

    std::vector<WebRtc_UWord16> missing_sequence_numbers;


    std::vector<WebRtc_UWord16>::iterator it =
        nack_receiver_->sequence_numbers_.begin();
    while (it != nack_receiver_->sequence_numbers_.end()) {
      int sequence_number_1 = *it;
      ++it;
      if (it != nack_receiver_->sequence_numbers_.end()) {
        int sequence_number_2 = *it;
        // Add all missing sequence numbers to list.
        for (int i = sequence_number_1 + 1; i < sequence_number_2; ++i) {
          missing_sequence_numbers.push_back(i);
        }
      }
    }
    int n = 0;
    for (it = missing_sequence_numbers.begin();
        it != missing_sequence_numbers.end(); ++it) {
      nack_list[n++] = (*it);
    }
    video_module_->SendNACK(nack_list, n);
    fake_clock.IncrementTime(33);
    video_module_->Process();

    // Prepare next frame.
    timestamp += 3000;
  }
  std::sort(nack_receiver_->sequence_numbers_.begin(),
            nack_receiver_->sequence_numbers_.end());
  EXPECT_EQ(kTestSequenceNumber, *(nack_receiver_->sequence_numbers_.begin()));
  EXPECT_EQ(kTestSequenceNumber + kTestNumberOfPackets - 1,
            *(nack_receiver_->sequence_numbers_.rbegin()));
  EXPECT_EQ(kTestNumberOfPackets, nack_receiver_->sequence_numbers_.size());
  EXPECT_EQ(kTestNumberOfRtxPackets, transport_->count_rtx_ssrc_);
}
