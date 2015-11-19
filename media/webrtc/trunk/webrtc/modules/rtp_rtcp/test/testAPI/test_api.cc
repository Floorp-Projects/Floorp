/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/rtp_rtcp/test/testAPI/test_api.h"

#include <algorithm>
#include <vector>

using namespace webrtc;

namespace webrtc {
void LoopBackTransport::SetSendModule(RtpRtcp* rtp_rtcp_module,
                                      RTPPayloadRegistry* payload_registry,
                                      RtpReceiver* receiver,
                                      ReceiveStatistics* receive_statistics) {
  rtp_rtcp_module_ = rtp_rtcp_module;
  rtp_payload_registry_ = payload_registry;
  rtp_receiver_ = receiver;
  receive_statistics_ = receive_statistics;
}

void LoopBackTransport::DropEveryNthPacket(int n) {
  packet_loss_ = n;
}

int LoopBackTransport::SendPacket(int channel, const void* data, size_t len) {
  count_++;
  if (packet_loss_ > 0) {
    if ((count_ % packet_loss_) == 0) {
      return len;
    }
  }
  RTPHeader header;
  rtc::scoped_ptr<RtpHeaderParser> parser(RtpHeaderParser::Create());
  if (!parser->Parse(static_cast<const uint8_t*>(data), len, &header)) {
    return -1;
  }
  PayloadUnion payload_specific;
  if (!rtp_payload_registry_->GetPayloadSpecifics(header.payloadType,
                                                  &payload_specific)) {
    return -1;
  }
  receive_statistics_->IncomingPacket(header, len, false);
  if (!rtp_receiver_->IncomingRtpPacket(header,
                                        static_cast<const uint8_t*>(data), len,
                                        payload_specific, true)) {
    return -1;
  }
  return len;
}

int LoopBackTransport::SendRTCPPacket(int channel,
                                      const void* data,
                                      size_t len) {
  if (rtp_rtcp_module_->IncomingRtcpPacket((const uint8_t*)data, len) < 0) {
    return -1;
  }
  return static_cast<int>(len);
}

int32_t TestRtpReceiver::OnReceivedPayloadData(
    const uint8_t* payload_data,
    const size_t payload_size,
    const webrtc::WebRtcRTPHeader* rtp_header) {
  EXPECT_LE(payload_size, sizeof(payload_data_));
  memcpy(payload_data_, payload_data, payload_size);
  memcpy(&rtp_header_, rtp_header, sizeof(rtp_header_));
  payload_size_ = payload_size;
  return 0;
}
}  // namespace webrtc

class RtpRtcpAPITest : public ::testing::Test {
 protected:
  RtpRtcpAPITest() : fake_clock_(123456) {
    test_csrcs_.push_back(1234);
    test_csrcs_.push_back(2345);
    test_id = 123;
    test_ssrc_ = 3456;
    test_timestamp_ = 4567;
    test_sequence_number_ = 2345;
  }
  ~RtpRtcpAPITest() {}

  void SetUp() override {
    RtpRtcp::Configuration configuration;
    configuration.id = test_id;
    configuration.audio = true;
    configuration.clock = &fake_clock_;
    module_.reset(RtpRtcp::CreateRtpRtcp(configuration));
    rtp_payload_registry_.reset(new RTPPayloadRegistry(
            RTPPayloadStrategy::CreateStrategy(true)));
    rtp_receiver_.reset(RtpReceiver::CreateAudioReceiver(
        test_id, &fake_clock_, NULL, NULL, NULL, rtp_payload_registry_.get()));
  }

  int test_id;
  rtc::scoped_ptr<RTPPayloadRegistry> rtp_payload_registry_;
  rtc::scoped_ptr<RtpReceiver> rtp_receiver_;
  rtc::scoped_ptr<RtpRtcp> module_;
  uint32_t test_ssrc_;
  uint32_t test_timestamp_;
  uint16_t test_sequence_number_;
  std::vector<uint32_t> test_csrcs_;
  SimulatedClock fake_clock_;
};

TEST_F(RtpRtcpAPITest, Basic) {
  module_->SetSequenceNumber(test_sequence_number_);
  EXPECT_EQ(test_sequence_number_, module_->SequenceNumber());

  module_->SetStartTimestamp(test_timestamp_);
  EXPECT_EQ(test_timestamp_, module_->StartTimestamp());

  EXPECT_FALSE(module_->Sending());
  EXPECT_EQ(0, module_->SetSendingStatus(true));
  EXPECT_TRUE(module_->Sending());
}

TEST_F(RtpRtcpAPITest, MTU) {
  EXPECT_EQ(-1, module_->SetMaxTransferUnit(10));
  EXPECT_EQ(-1, module_->SetMaxTransferUnit(IP_PACKET_SIZE + 1));
  EXPECT_EQ(0, module_->SetMaxTransferUnit(1234));
  EXPECT_EQ(1234 - 20 - 8, module_->MaxPayloadLength());

  EXPECT_EQ(0, module_->SetTransportOverhead(true, true, 12));
  EXPECT_EQ(1234 - 20 - 20 - 20 - 12, module_->MaxPayloadLength());

  EXPECT_EQ(0, module_->SetTransportOverhead(false, false, 0));
  EXPECT_EQ(1234 - 20 - 8, module_->MaxPayloadLength());
}

TEST_F(RtpRtcpAPITest, SSRC) {
  module_->SetSSRC(test_ssrc_);
  EXPECT_EQ(test_ssrc_, module_->SSRC());
}

TEST_F(RtpRtcpAPITest, RTCP) {
  EXPECT_EQ(kRtcpOff, module_->RTCP());
  module_->SetRTCPStatus(kRtcpCompound);
  EXPECT_EQ(kRtcpCompound, module_->RTCP());

  EXPECT_EQ(0, module_->SetCNAME("john.doe@test.test"));

  EXPECT_FALSE(module_->TMMBR());
  module_->SetTMMBRStatus(true);
  EXPECT_TRUE(module_->TMMBR());
  module_->SetTMMBRStatus(false);
  EXPECT_FALSE(module_->TMMBR());

  EXPECT_EQ(kNackOff, rtp_receiver_->NACK());
  rtp_receiver_->SetNACKStatus(kNackRtcp);
  EXPECT_EQ(kNackRtcp, rtp_receiver_->NACK());
}

TEST_F(RtpRtcpAPITest, RtxSender) {
  module_->SetRtxSendStatus(kRtxRetransmitted);
  EXPECT_EQ(kRtxRetransmitted, module_->RtxSendStatus());

  module_->SetRtxSendStatus(kRtxOff);
  EXPECT_EQ(kRtxOff, module_->RtxSendStatus());

  module_->SetRtxSendStatus(kRtxRetransmitted);
  EXPECT_EQ(kRtxRetransmitted, module_->RtxSendStatus());
}

TEST_F(RtpRtcpAPITest, RtxReceiver) {
  const uint32_t kRtxSsrc = 1;
  const int kRtxPayloadType = 119;
  EXPECT_FALSE(rtp_payload_registry_->RtxEnabled());
  rtp_payload_registry_->SetRtxSsrc(kRtxSsrc);
  rtp_payload_registry_->SetRtxPayloadType(kRtxPayloadType);
  EXPECT_TRUE(rtp_payload_registry_->RtxEnabled());
  RTPHeader rtx_header;
  rtx_header.ssrc = kRtxSsrc;
  rtx_header.payloadType = kRtxPayloadType;
  EXPECT_TRUE(rtp_payload_registry_->IsRtx(rtx_header));
  rtx_header.ssrc = 0;
  EXPECT_FALSE(rtp_payload_registry_->IsRtx(rtx_header));
  rtx_header.ssrc = kRtxSsrc;
  rtx_header.payloadType = 0;
  EXPECT_TRUE(rtp_payload_registry_->IsRtx(rtx_header));
}
