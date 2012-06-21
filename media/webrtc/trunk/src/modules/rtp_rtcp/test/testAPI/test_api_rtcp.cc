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
 
const WebRtc_UWord64 kTestPictureId = 12345678;

class RtcpCallback : public RtcpFeedback {
 public:
  RtcpCallback(RtpRtcp* module) {
    _rtpRtcpModule = module;
  };
  virtual void OnRTCPPacketTimeout(const WebRtc_Word32 id) {
  }
  virtual void OnLipSyncUpdate(const WebRtc_Word32 id,
                               const WebRtc_Word32 audioVideoOffset) {
  };
  virtual void OnTMMBRReceived(const WebRtc_Word32 id,
                               const WebRtc_UWord16 bwEstimateKbit) {
  };
  virtual void OnXRVoIPMetricReceived(
      const WebRtc_Word32 id,
      const RTCPVoIPMetric* metric,
      const WebRtc_Word8 VoIPmetricBuffer[28]) {
  };
  virtual void OnSLIReceived(const WebRtc_Word32 id,
                             const WebRtc_UWord8 pictureId) {
    EXPECT_EQ(28, pictureId);
  };

  virtual void OnRPSIReceived(const WebRtc_Word32 id,
                              const WebRtc_UWord64 pictureId) {
    EXPECT_EQ(kTestPictureId, pictureId);
  };
  virtual void OnApplicationDataReceived(const WebRtc_Word32 id,
                                         const WebRtc_UWord8 subType,
                                         const WebRtc_UWord32 name,
                                         const WebRtc_UWord16 length,
                                         const WebRtc_UWord8* data) {
    char print_name[5];
    print_name[0] = static_cast<char>(name >> 24);
    print_name[1] = static_cast<char>(name >> 16);
    print_name[2] = static_cast<char>(name >> 8);
    print_name[3] = static_cast<char>(name);
    print_name[4] = 0;

    EXPECT_STRCASEEQ("test", print_name);
  };

  virtual void OnSendReportReceived(const WebRtc_Word32 id,
                                    const WebRtc_UWord32 senderSSRC) {
    RTCPSenderInfo senderInfo;
    EXPECT_EQ(0, _rtpRtcpModule->RemoteRTCPStat(&senderInfo));
  };

  virtual void OnReceiveReportReceived(const WebRtc_Word32 id,
                                       const WebRtc_UWord32 senderSSRC) {
  };
 private:
  RtpRtcp* _rtpRtcpModule;
};

class RtpRtcpRtcpTest : public ::testing::Test {
 protected:
  RtpRtcpRtcpTest() {
    test_CSRC[0] = 1234;
    test_CSRC[2] = 2345;
    test_id = 123;
    test_ssrc = 3456;
    test_timestamp = 4567;
    test_sequence_number = 2345;
  }
  ~RtpRtcpRtcpTest() {}

  virtual void SetUp() {
    module1 = RtpRtcp::CreateRtpRtcp(test_id, true, &fake_clock);
    module2 = RtpRtcp::CreateRtpRtcp(test_id+1, true, &fake_clock);

    EXPECT_EQ(0, module1->InitReceiver());
    EXPECT_EQ(0, module1->InitSender());
    EXPECT_EQ(0, module2->InitReceiver());
    EXPECT_EQ(0, module2->InitSender());
    receiver = new RtpReceiver();
    EXPECT_EQ(0, module2->RegisterIncomingDataCallback(receiver));
    transport1 = new LoopBackTransport(module2);
    EXPECT_EQ(0, module1->RegisterSendTransport(transport1));
    transport2 = new LoopBackTransport(module1);
    EXPECT_EQ(0, module2->RegisterSendTransport(transport2));
  }

  virtual void TearDown() {
    RtpRtcp::DestroyRtpRtcp(module1);
    RtpRtcp::DestroyRtpRtcp(module2);
    delete transport1;
    delete transport2;
    delete receiver;
  }

  void SetUpCallFromModule1(RtcpCallback* feedback1, RtcpCallback* feedback2 ) {
    EXPECT_EQ(0, module1->RegisterIncomingRTCPCallback(feedback1));
    EXPECT_EQ(0, module2->RegisterIncomingRTCPCallback(feedback2));

    EXPECT_EQ(0, module1->SetRTCPStatus(kRtcpCompound));
    EXPECT_EQ(0, module2->SetRTCPStatus(kRtcpCompound));

    EXPECT_EQ(0, module2->SetSSRC(test_ssrc + 1));
    EXPECT_EQ(0, module1->SetSSRC(test_ssrc));
    EXPECT_EQ(0, module1->SetSequenceNumber(test_sequence_number));
    EXPECT_EQ(0, module1->SetStartTimestamp(test_timestamp));
    EXPECT_EQ(0, module1->SetCSRCs(test_CSRC, 2));
    EXPECT_EQ(0, module1->SetCNAME("john.doe@test.test"));

    EXPECT_EQ(0, module1->SetSendingStatus(true));

    CodecInst voiceCodec;
    voiceCodec.pltype = 96;
    voiceCodec.plfreq = 8000;
    voiceCodec.rate = 64000;
    memcpy(voiceCodec.plname, "PCMU", 5);

    EXPECT_EQ(0, module1->RegisterSendPayload(voiceCodec));
    EXPECT_EQ(0, module1->RegisterReceivePayload(voiceCodec));
    EXPECT_EQ(0, module2->RegisterSendPayload(voiceCodec));
    EXPECT_EQ(0, module2->RegisterReceivePayload(voiceCodec));

    // We need to send one RTP packet to get the RTCP packet to be accepted by
    // the receiving module.
    // send RTP packet with the data "testtest"
    const WebRtc_UWord8 test[9] = "testtest";
    EXPECT_EQ(0, module1->SendOutgoingData(webrtc::kAudioFrameSpeech, 96,
                                           0, test, 8));
  }

  int test_id;
  RtpRtcp* module1;
  RtpRtcp* module2;
  RtpReceiver* receiver;
  LoopBackTransport* transport1;
  LoopBackTransport* transport2;
  WebRtc_UWord32 test_ssrc;
  WebRtc_UWord32 test_timestamp;
  WebRtc_UWord16 test_sequence_number;
  WebRtc_UWord32 test_CSRC[webrtc::kRtpCsrcSize];
  FakeRtpRtcpClock fake_clock;
};

TEST_F(RtpRtcpRtcpTest, RTCP) {
  RtcpCallback* myRTCPFeedback1 = new RtcpCallback(module1);
  RtcpCallback* myRTCPFeedback2 = new RtcpCallback(module2);

  SetUpCallFromModule1(myRTCPFeedback1, myRTCPFeedback2);

  EXPECT_EQ(0, module1->SendRTCPReferencePictureSelection(kTestPictureId));
  EXPECT_EQ(0, module1->SendRTCPSliceLossIndication(156));

  WebRtc_UWord32 testOfCSRC[webrtc::kRtpCsrcSize];
  EXPECT_EQ(2, module2->RemoteCSRCs(testOfCSRC));
  EXPECT_EQ(test_CSRC[0], testOfCSRC[0]);
  EXPECT_EQ(test_CSRC[1], testOfCSRC[1]);

  // Set cname of mixed.
  EXPECT_EQ(0, module1->AddMixedCNAME(test_CSRC[0], "john@192.168.0.1"));
  EXPECT_EQ(0, module1->AddMixedCNAME(test_CSRC[1], "jane@192.168.0.2"));
  EXPECT_EQ(-1, module1->AddMixedCNAME(test_CSRC[0], NULL));

  EXPECT_EQ(-1, module1->RemoveMixedCNAME(test_CSRC[0] + 1));
  EXPECT_EQ(0, module1->RemoveMixedCNAME(test_CSRC[1]));
  EXPECT_EQ(0, module1->AddMixedCNAME(test_CSRC[1], "jane@192.168.0.2"));

  RTCPReportBlock reportBlock;
  reportBlock.cumulativeLost = 1;
  reportBlock.delaySinceLastSR = 2;
  reportBlock.extendedHighSeqNum = 3;
  reportBlock.fractionLost= 4;
  reportBlock.jitter = 5;
  reportBlock.lastSR = 6;

  // Set report blocks.
  EXPECT_EQ(-1, module1->AddRTCPReportBlock(test_CSRC[0], NULL));
  EXPECT_EQ(0, module1->AddRTCPReportBlock(test_CSRC[0], &reportBlock));

  reportBlock.lastSR= 7;
  EXPECT_EQ(0, module1->AddRTCPReportBlock(test_CSRC[1], &reportBlock));

  WebRtc_UWord32 name = 't' << 24;
  name += 'e' << 16;
  name += 's' << 8;
  name += 't';
  EXPECT_EQ(0, module1->SetRTCPApplicationSpecificData(
      3,
      name,
      (const WebRtc_UWord8 *)"test test test test test test test test test"\
          " test test test test test test test test test test test test test"\
          " test test test test test test test test test test test test test"\
          " test test test test test test test test test test test test test"\
          " test test test test test test test test test test test test ",
          300));

  // send RTCP packet, triggered by timer
  fake_clock.IncrementTime(7500);
  module1->Process();
  fake_clock.IncrementTime(100);
  module2->Process();

  WebRtc_UWord32 receivedNTPsecs = 0;
  WebRtc_UWord32 receivedNTPfrac = 0;
  WebRtc_UWord32 RTCPArrivalTimeSecs = 0;
  WebRtc_UWord32 RTCPArrivalTimeFrac = 0;
  char cName[RTCP_CNAME_SIZE];

  EXPECT_EQ(0, module2->RemoteNTP(&receivedNTPsecs, &receivedNTPfrac,
                                  &RTCPArrivalTimeSecs, &RTCPArrivalTimeFrac));

  EXPECT_EQ(-1, module2->RemoteCNAME(module2->RemoteSSRC() + 1, cName));
  EXPECT_EQ(-1, module2->RemoteCNAME(module2->RemoteSSRC(), NULL));

  // Check multiple CNAME.
  EXPECT_EQ(0, module2->RemoteCNAME(module2->RemoteSSRC(), cName));
  EXPECT_EQ(0, strncmp(cName, "john.doe@test.test", RTCP_CNAME_SIZE));

  EXPECT_EQ(0, module2->RemoteCNAME(test_CSRC[0], cName));
  EXPECT_EQ(0, strncmp(cName, "john@192.168.0.1", RTCP_CNAME_SIZE));

  EXPECT_EQ(0, module2->RemoteCNAME(test_CSRC[1], cName));
  EXPECT_EQ(0, strncmp(cName, "jane@192.168.0.2", RTCP_CNAME_SIZE));

  // get all report blocks
  std::vector<RTCPReportBlock> report_blocks;
  EXPECT_EQ(-1, module1->RemoteRTCPStat(NULL));
  EXPECT_EQ(0, module1->RemoteRTCPStat(&report_blocks));
  EXPECT_EQ(1u, report_blocks.size());
  const RTCPReportBlock& reportBlockReceived = report_blocks[0];

  float secSinceLastReport =
      static_cast<float>(reportBlockReceived.delaySinceLastSR) / 65536.0f;
  EXPECT_GE(0.101f, secSinceLastReport);
  EXPECT_LE(0.100f, secSinceLastReport);
  EXPECT_EQ(test_sequence_number, reportBlockReceived.extendedHighSeqNum);
  EXPECT_EQ(0, reportBlockReceived.fractionLost);

  EXPECT_EQ(static_cast<WebRtc_UWord32>(0),
            reportBlockReceived.cumulativeLost);

  WebRtc_UWord8  fraction_lost = 0;  // scale 0 to 255
  WebRtc_UWord32 cum_lost = 0;       // number of lost packets
  WebRtc_UWord32 ext_max = 0;        // highest sequence number received
  WebRtc_UWord32 jitter = 0;
  WebRtc_UWord32 max_jitter = 0;
  EXPECT_EQ(0, module2->StatisticsRTP(&fraction_lost,
                                      &cum_lost,
                                      &ext_max,
                                      &jitter,
                                      &max_jitter));
  EXPECT_EQ(0, fraction_lost);
  EXPECT_EQ((WebRtc_UWord32)0, cum_lost);
  EXPECT_EQ(test_sequence_number, ext_max);
  EXPECT_EQ(reportBlockReceived.jitter, jitter);

  WebRtc_UWord16 RTT;
  WebRtc_UWord16 avgRTT;
  WebRtc_UWord16 minRTT;
  WebRtc_UWord16 maxRTT;

  // Get RoundTripTime.
  EXPECT_EQ(0, module1->RTT(test_ssrc + 1, &RTT, &avgRTT, &minRTT, &maxRTT));
  EXPECT_GE(10, RTT);
  EXPECT_GE(10, avgRTT);
  EXPECT_GE(10, minRTT);
  EXPECT_GE(10, maxRTT);

  // Set report blocks.
  EXPECT_EQ(0, module1->AddRTCPReportBlock(test_CSRC[0], &reportBlock));

  // Test receive report.
  EXPECT_EQ(0, module1->SetSendingStatus(false));

  // Test that BYE clears the CNAME
  EXPECT_EQ(-1, module2->RemoteCNAME(module2->RemoteSSRC(), cName));

  // Send RTCP packet, triggered by timer.
  fake_clock.IncrementTime(5000);
  module1->Process();
  module2->Process();

  delete myRTCPFeedback1;
  delete myRTCPFeedback2;
}

TEST_F(RtpRtcpRtcpTest, RemoteRTCPStatRemote) {
  std::vector<RTCPReportBlock> report_blocks;

  RtcpCallback feedback1(module1);
  RtcpCallback feedback2(module2);

  SetUpCallFromModule1(&feedback1, &feedback2);
  EXPECT_EQ(0, module1->RemoteRTCPStat(&report_blocks));
  EXPECT_EQ(0u, report_blocks.size());

  // send RTCP packet, triggered by timer
  fake_clock.IncrementTime(7500);
  module1->Process();
  fake_clock.IncrementTime(100);
  module2->Process();

  EXPECT_EQ(0, module1->RemoteRTCPStat(&report_blocks));
  ASSERT_EQ(1u, report_blocks.size());

  // |test_ssrc+1| is the SSRC of module2 that send the report.
  EXPECT_EQ(test_ssrc+1, report_blocks[0].remoteSSRC);
  EXPECT_EQ(test_ssrc, report_blocks[0].sourceSSRC);

  EXPECT_EQ(0u, report_blocks[0].cumulativeLost);
  EXPECT_LT(0u, report_blocks[0].delaySinceLastSR);
  EXPECT_EQ(test_sequence_number, report_blocks[0].extendedHighSeqNum);
  EXPECT_EQ(0u, report_blocks[0].fractionLost);
}
