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

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "test_api.h"
#include "common_types.h"
#include "rtp_rtcp.h"
#include "rtp_rtcp_defines.h"

using namespace webrtc;

const uint64_t kTestPictureId = 12345678;

class RtcpCallback : public RtcpFeedback, public RtcpIntraFrameObserver {
 public:
  void SetModule(RtpRtcp* module) {
    _rtpRtcpModule = module;
  };
  virtual void OnRTCPPacketTimeout(const int32_t id) {
  }
  virtual void OnLipSyncUpdate(const int32_t id,
                               const int32_t audioVideoOffset) {
  };
  virtual void OnXRVoIPMetricReceived(
      const int32_t id,
      const RTCPVoIPMetric* metric) {
  };
  virtual void OnApplicationDataReceived(const int32_t id,
                                         const uint8_t subType,
                                         const uint32_t name,
                                         const uint16_t length,
                                         const uint8_t* data) {
    char print_name[5];
    print_name[0] = static_cast<char>(name >> 24);
    print_name[1] = static_cast<char>(name >> 16);
    print_name[2] = static_cast<char>(name >> 8);
    print_name[3] = static_cast<char>(name);
    print_name[4] = 0;

    EXPECT_STRCASEEQ("test", print_name);
  };
  virtual void OnSendReportReceived(const int32_t id,
                                    const uint32_t senderSSRC,
                                    uint32_t ntp_secs,
                                    uint32_t ntp_frac,
                                    uint32_t timestamp) {
    RTCPSenderInfo senderInfo;
    EXPECT_EQ(0, _rtpRtcpModule->RemoteRTCPStat(&senderInfo));
  };
  virtual void OnReceiveReportReceived(const int32_t id,
                                       const uint32_t senderSSRC) {
  };
  virtual void OnReceivedIntraFrameRequest(uint32_t ssrc) {
  };
  virtual void OnReceivedSLI(uint32_t ssrc,
                             uint8_t pictureId) {
    EXPECT_EQ(28, pictureId);
  };
  virtual void OnReceivedRPSI(uint32_t ssrc,
                              uint64_t pictureId) {
    EXPECT_EQ(kTestPictureId, pictureId);
  };
  virtual void OnLocalSsrcChanged(uint32_t old_ssrc, uint32_t new_ssrc) {};
 private:
  RtpRtcp* _rtpRtcpModule;
};

class RtpRtcpRtcpTest : public ::testing::Test {
 protected:
  RtpRtcpRtcpTest() : fake_clock(123456) {
    test_CSRC[0] = 1234;
    test_CSRC[1] = 2345;
    test_id = 123;
    test_ssrc = 3456;
    test_timestamp = 4567;
    test_sequence_number = 2345;
  }
  ~RtpRtcpRtcpTest() {}

  virtual void SetUp() {
    receiver = new RtpReceiver();
    transport1 = new LoopBackTransport();
    transport2 = new LoopBackTransport();
    myRTCPFeedback1 = new RtcpCallback();
    myRTCPFeedback2 = new RtcpCallback();

    RtpRtcp::Configuration configuration;
    configuration.id = test_id;
    configuration.audio = true;
    configuration.clock = &fake_clock;
    configuration.outgoing_transport = transport1;
    configuration.rtcp_feedback = myRTCPFeedback1;
    configuration.intra_frame_callback = myRTCPFeedback1;
    configuration.incoming_data = receiver;

    module1 = RtpRtcp::CreateRtpRtcp(configuration);

    configuration.id = test_id + 1;
    configuration.outgoing_transport = transport2;
    configuration.rtcp_feedback = myRTCPFeedback2;
    configuration.intra_frame_callback = myRTCPFeedback2;
    module2 = RtpRtcp::CreateRtpRtcp(configuration);

    transport1->SetSendModule(module2);
    transport2->SetSendModule(module1);
    myRTCPFeedback1->SetModule(module1);
    myRTCPFeedback2->SetModule(module2);

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
    const uint8_t test[9] = "testtest";
    EXPECT_EQ(0, module1->SendOutgoingData(webrtc::kAudioFrameSpeech, 96,
                                           0, -1, test, 8));
  }

  virtual void TearDown() {
    delete module1;
    delete module2;
    delete myRTCPFeedback1;
    delete myRTCPFeedback2;
    delete transport1;
    delete transport2;
    delete receiver;
  }

  int test_id;
  RtpRtcp* module1;
  RtpRtcp* module2;
  RtpReceiver* receiver;
  LoopBackTransport* transport1;
  LoopBackTransport* transport2;
  RtcpCallback* myRTCPFeedback1;
  RtcpCallback* myRTCPFeedback2;

  uint32_t test_ssrc;
  uint32_t test_timestamp;
  uint16_t test_sequence_number;
  uint32_t test_CSRC[webrtc::kRtpCsrcSize];
  SimulatedClock fake_clock;
};

TEST_F(RtpRtcpRtcpTest, RTCP_PLI_RPSI) {
  EXPECT_EQ(0, module1->SendRTCPReferencePictureSelection(kTestPictureId));
  EXPECT_EQ(0, module1->SendRTCPSliceLossIndication(156));
}

TEST_F(RtpRtcpRtcpTest, RTCP_CNAME) {
  uint32_t testOfCSRC[webrtc::kRtpCsrcSize];
  EXPECT_EQ(2, module2->RemoteCSRCs(testOfCSRC));
  EXPECT_EQ(test_CSRC[0], testOfCSRC[0]);
  EXPECT_EQ(test_CSRC[1], testOfCSRC[1]);

  // Set cname of mixed.
  EXPECT_EQ(0, module1->AddMixedCNAME(test_CSRC[0], "john@192.168.0.1"));
  EXPECT_EQ(0, module1->AddMixedCNAME(test_CSRC[1], "jane@192.168.0.2"));

  EXPECT_EQ(-1, module1->RemoveMixedCNAME(test_CSRC[0] + 1));
  EXPECT_EQ(0, module1->RemoveMixedCNAME(test_CSRC[1]));
  EXPECT_EQ(0, module1->AddMixedCNAME(test_CSRC[1], "jane@192.168.0.2"));

  // send RTCP packet, triggered by timer
  fake_clock.AdvanceTimeMilliseconds(7500);
  module1->Process();
  fake_clock.AdvanceTimeMilliseconds(100);
  module2->Process();

  char cName[RTCP_CNAME_SIZE];
  EXPECT_EQ(-1, module2->RemoteCNAME(module2->RemoteSSRC() + 1, cName));

  // Check multiple CNAME.
  EXPECT_EQ(0, module2->RemoteCNAME(module2->RemoteSSRC(), cName));
  EXPECT_EQ(0, strncmp(cName, "john.doe@test.test", RTCP_CNAME_SIZE));

  EXPECT_EQ(0, module2->RemoteCNAME(test_CSRC[0], cName));
  EXPECT_EQ(0, strncmp(cName, "john@192.168.0.1", RTCP_CNAME_SIZE));

  EXPECT_EQ(0, module2->RemoteCNAME(test_CSRC[1], cName));
  EXPECT_EQ(0, strncmp(cName, "jane@192.168.0.2", RTCP_CNAME_SIZE));

  EXPECT_EQ(0, module1->SetSendingStatus(false));

  // Test that BYE clears the CNAME
  EXPECT_EQ(-1, module2->RemoteCNAME(module2->RemoteSSRC(), cName));
}

TEST_F(RtpRtcpRtcpTest, RTCP) {
  RTCPReportBlock reportBlock;
  reportBlock.remoteSSRC = 1;
  reportBlock.sourceSSRC = 2;
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

  uint32_t name = 't' << 24;
  name += 'e' << 16;
  name += 's' << 8;
  name += 't';
  EXPECT_EQ(0, module1->SetRTCPApplicationSpecificData(
      3,
      name,
      (const uint8_t *)"test test test test test test test test test"\
          " test test test test test test test test test test test test test"\
          " test test test test test test test test test test test test test"\
          " test test test test test test test test test test test test test"\
          " test test test test test test test test test test test test ",
          300));

  // send RTCP packet, triggered by timer
  fake_clock.AdvanceTimeMilliseconds(7500);
  module1->Process();
  fake_clock.AdvanceTimeMilliseconds(100);
  module2->Process();

  uint32_t receivedNTPsecs = 0;
  uint32_t receivedNTPfrac = 0;
  uint32_t RTCPArrivalTimeSecs = 0;
  uint32_t RTCPArrivalTimeFrac = 0;
  EXPECT_EQ(0, module2->RemoteNTP(&receivedNTPsecs,
                                  &receivedNTPfrac,
                                  &RTCPArrivalTimeSecs,
                                  &RTCPArrivalTimeFrac,
                                  NULL));


  // get all report blocks
  std::vector<RTCPReportBlock> report_blocks;
  EXPECT_EQ(-1, module1->RemoteRTCPStat(NULL));
  EXPECT_EQ(0, module1->RemoteRTCPStat(&report_blocks));
  ASSERT_EQ(1u, report_blocks.size());
  const RTCPReportBlock& reportBlockReceived = report_blocks[0];

  float secSinceLastReport =
      static_cast<float>(reportBlockReceived.delaySinceLastSR) / 65536.0f;
  EXPECT_GE(0.101f, secSinceLastReport);
  EXPECT_LE(0.100f, secSinceLastReport);
  EXPECT_EQ(test_sequence_number, reportBlockReceived.extendedHighSeqNum);
  EXPECT_EQ(0, reportBlockReceived.fractionLost);

  EXPECT_EQ(static_cast<uint32_t>(0),
            reportBlockReceived.cumulativeLost);

  uint8_t  fraction_lost = 0;  // scale 0 to 255
  uint32_t cum_lost = 0;       // number of lost packets
  uint32_t ext_max = 0;        // highest sequence number received
  uint32_t jitter = 0;
  uint32_t max_jitter = 0;
  EXPECT_EQ(0, module2->StatisticsRTP(&fraction_lost,
                                      &cum_lost,
                                      &ext_max,
                                      &jitter,
                                      &max_jitter));
  EXPECT_EQ(0, fraction_lost);
  EXPECT_EQ((uint32_t)0, cum_lost);
  EXPECT_EQ(test_sequence_number, ext_max);
  EXPECT_EQ(reportBlockReceived.jitter, jitter);

  uint16_t RTT;
  uint16_t avgRTT;
  uint16_t minRTT;
  uint16_t maxRTT;

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

  // Send RTCP packet, triggered by timer.
  fake_clock.AdvanceTimeMilliseconds(5000);
  module1->Process();
  module2->Process();
}

TEST_F(RtpRtcpRtcpTest, RemoteRTCPStatRemote) {
  std::vector<RTCPReportBlock> report_blocks;

  EXPECT_EQ(0, module1->RemoteRTCPStat(&report_blocks));
  EXPECT_EQ(0u, report_blocks.size());

  // send RTCP packet, triggered by timer
  fake_clock.AdvanceTimeMilliseconds(7500);
  module1->Process();
  fake_clock.AdvanceTimeMilliseconds(100);
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
