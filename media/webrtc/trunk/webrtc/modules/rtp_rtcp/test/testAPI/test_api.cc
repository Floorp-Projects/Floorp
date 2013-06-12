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

class RtpRtcpAPITest : public ::testing::Test {
 protected:
  RtpRtcpAPITest() : module(NULL), fake_clock(123456) {
    test_CSRC[0] = 1234;
    test_CSRC[1] = 2345;
    test_id = 123;
    test_ssrc = 3456;
    test_timestamp = 4567;
    test_sequence_number = 2345;
  }
  ~RtpRtcpAPITest() {}

  virtual void SetUp() {
    RtpRtcp::Configuration configuration;
    configuration.id = test_id;
    configuration.audio = true;
    configuration.clock = &fake_clock;
    module = RtpRtcp::CreateRtpRtcp(configuration);
  }

  virtual void TearDown() {
    delete module;
  }

  int test_id;
  RtpRtcp* module;
  uint32_t test_ssrc;
  uint32_t test_timestamp;
  uint16_t test_sequence_number;
  uint32_t test_CSRC[webrtc::kRtpCsrcSize];
  SimulatedClock fake_clock;
};

TEST_F(RtpRtcpAPITest, Basic) {
  EXPECT_EQ(0, module->SetSequenceNumber(test_sequence_number));
  EXPECT_EQ(test_sequence_number, module->SequenceNumber());

  EXPECT_EQ(0, module->SetStartTimestamp(test_timestamp));
  EXPECT_EQ(test_timestamp, module->StartTimestamp());

  EXPECT_FALSE(module->Sending());
  EXPECT_EQ(0, module->SetSendingStatus(true));
  EXPECT_TRUE(module->Sending());
}

TEST_F(RtpRtcpAPITest, MTU) {
  EXPECT_EQ(-1, module->SetMaxTransferUnit(10));
  EXPECT_EQ(-1, module->SetMaxTransferUnit(IP_PACKET_SIZE + 1));
  EXPECT_EQ(0, module->SetMaxTransferUnit(1234));
  EXPECT_EQ(1234-20-8, module->MaxPayloadLength());

  EXPECT_EQ(0, module->SetTransportOverhead(true, true, 12));
  EXPECT_EQ(1234 - 20- 20 -20 - 12, module->MaxPayloadLength());

  EXPECT_EQ(0, module->SetTransportOverhead(false, false, 0));
  EXPECT_EQ(1234 - 20 - 8, module->MaxPayloadLength());
}

TEST_F(RtpRtcpAPITest, SSRC) {
  EXPECT_EQ(0, module->SetSSRC(test_ssrc));
  EXPECT_EQ(test_ssrc, module->SSRC());
}

TEST_F(RtpRtcpAPITest, CSRC) {
  EXPECT_EQ(0, module->SetCSRCs(test_CSRC, 2));
  uint32_t testOfCSRC[webrtc::kRtpCsrcSize];
  EXPECT_EQ(2, module->CSRCs(testOfCSRC));
  EXPECT_EQ(test_CSRC[0], testOfCSRC[0]);
  EXPECT_EQ(test_CSRC[1], testOfCSRC[1]);
}

TEST_F(RtpRtcpAPITest, RTCP) {
  EXPECT_EQ(kRtcpOff, module->RTCP());
  EXPECT_EQ(0, module->SetRTCPStatus(kRtcpCompound));
  EXPECT_EQ(kRtcpCompound, module->RTCP());

  EXPECT_EQ(0, module->SetCNAME("john.doe@test.test"));

  char cName[RTCP_CNAME_SIZE];
  EXPECT_EQ(0, module->CNAME(cName));
  EXPECT_STRCASEEQ(cName, "john.doe@test.test");

  EXPECT_FALSE(module->TMMBR());
  EXPECT_EQ(0, module->SetTMMBRStatus(true));
  EXPECT_TRUE(module->TMMBR());
  EXPECT_EQ(0, module->SetTMMBRStatus(false));
  EXPECT_FALSE(module->TMMBR());

  EXPECT_EQ(kNackOff, module->NACK());
  EXPECT_EQ(0, module->SetNACKStatus(kNackRtcp, 450));
  EXPECT_EQ(kNackRtcp, module->NACK());
}

TEST_F(RtpRtcpAPITest, RTXSender) {
  unsigned int ssrc = 0;
  RtxMode rtx_mode = kRtxOff;
  const int kRtxPayloadType = 119;
  int payload_type = -1;
  EXPECT_EQ(0, module->SetRTXSendStatus(kRtxRetransmitted, true, 1));
  module->SetRtxSendPayloadType(kRtxPayloadType);
  EXPECT_EQ(0, module->RTXSendStatus(&rtx_mode, &ssrc, &payload_type));
  EXPECT_EQ(kRtxRetransmitted, rtx_mode);
  EXPECT_EQ(1u, ssrc);
  EXPECT_EQ(kRtxPayloadType, payload_type);
  rtx_mode = kRtxOff;
  EXPECT_EQ(0, module->SetRTXSendStatus(kRtxOff, true, 0));
  payload_type = -1;
  module->SetRtxSendPayloadType(kRtxPayloadType);
  EXPECT_EQ(0, module->RTXSendStatus(&rtx_mode, &ssrc, &payload_type));
  EXPECT_EQ(kRtxOff, rtx_mode);
  EXPECT_EQ(kRtxPayloadType ,payload_type);
  EXPECT_EQ(0, module->SetRTXSendStatus(kRtxRetransmitted, false, 1));
  EXPECT_EQ(0, module->RTXSendStatus(&rtx_mode, &ssrc, &payload_type));
  EXPECT_EQ(kRtxRetransmitted, rtx_mode);
  EXPECT_EQ(kRtxPayloadType ,payload_type);
}

TEST_F(RtpRtcpAPITest, RTXReceiver) {
  bool enable = false;
  unsigned int ssrc = 0;
  const int kRtxPayloadType = 119;
  int payload_type = -1;
  EXPECT_EQ(0, module->SetRTXReceiveStatus(true, 1));
  module->SetRtxReceivePayloadType(kRtxPayloadType);
  EXPECT_EQ(0, module->RTXReceiveStatus(&enable, &ssrc, &payload_type));
  EXPECT_TRUE(enable);
  EXPECT_EQ(1u, ssrc);
  EXPECT_EQ(kRtxPayloadType ,payload_type);
  EXPECT_EQ(0, module->SetRTXReceiveStatus(false, 0));
  EXPECT_EQ(0, module->RTXReceiveStatus(&enable, &ssrc, &payload_type));
  EXPECT_FALSE(enable);
  EXPECT_EQ(kRtxPayloadType ,payload_type);
}
