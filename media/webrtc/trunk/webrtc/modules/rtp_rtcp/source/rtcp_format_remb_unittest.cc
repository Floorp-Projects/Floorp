/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <gtest/gtest.h>

#include "typedefs.h"
#include "common_types.h"
#include "rtp_utility.h"
#include "rtcp_sender.h"
#include "rtcp_receiver.h"
#include "rtp_rtcp_impl.h"
#include "modules/remote_bitrate_estimator/include/bwe_defines.h"
#include "modules/remote_bitrate_estimator/include/mock/mock_remote_bitrate_observer.h"

namespace {

using namespace webrtc;


class TestTransport : public Transport {
 public:
  TestTransport(RTCPReceiver* rtcp_receiver) :
    rtcp_receiver_(rtcp_receiver) {
  }

  virtual int SendPacket(int /*channel*/, const void* /*data*/, int /*len*/) {
    return -1;
  }
  virtual int SendRTCPPacket(int /*channel*/,
                             const void *packet,
                             int packetLength) {
    RTCPUtility::RTCPParserV2 rtcpParser((WebRtc_UWord8*)packet,
                                         (WebRtc_Word32)packetLength,
                                         true); // Allow non-compound RTCP

    EXPECT_TRUE(rtcpParser.IsValid());
    RTCPHelp::RTCPPacketInformation rtcpPacketInformation;
    EXPECT_EQ(0, rtcp_receiver_->IncomingRTCPPacket(rtcpPacketInformation,
                                                    &rtcpParser));

    EXPECT_EQ((WebRtc_UWord32)kRtcpRemb,
              rtcpPacketInformation.rtcpPacketTypeFlags & kRtcpRemb);
    EXPECT_EQ((WebRtc_UWord32)1234,
              rtcpPacketInformation.receiverEstimatedMaxBitrate);
    return packetLength;
  }
 private:
  RTCPReceiver* rtcp_receiver_;
};


class RtcpFormatRembTest : public ::testing::Test {
 protected:
  RtcpFormatRembTest()
      : over_use_detector_options_(),
        remote_bitrate_observer_(),
        remote_bitrate_estimator_(RemoteBitrateEstimator::Create(
            &remote_bitrate_observer_,
            over_use_detector_options_,
            RemoteBitrateEstimator::kMultiStreamEstimation)) {}
  virtual void SetUp();
  virtual void TearDown();

  OverUseDetectorOptions over_use_detector_options_;
  RtpRtcpClock* system_clock_;
  ModuleRtpRtcpImpl* dummy_rtp_rtcp_impl_;
  RTCPSender* rtcp_sender_;
  RTCPReceiver* rtcp_receiver_;
  TestTransport* test_transport_;
  MockRemoteBitrateObserver remote_bitrate_observer_;
  scoped_ptr<RemoteBitrateEstimator> remote_bitrate_estimator_;
};

void RtcpFormatRembTest::SetUp() {
  system_clock_ = ModuleRTPUtility::GetSystemClock();
  RtpRtcp::Configuration configuration;
  configuration.id = 0;
  configuration.audio = false;
  configuration.clock = system_clock_;
  configuration.remote_bitrate_estimator = remote_bitrate_estimator_.get();
  dummy_rtp_rtcp_impl_ = new ModuleRtpRtcpImpl(configuration);
  rtcp_sender_ = new RTCPSender(0, false, system_clock_, dummy_rtp_rtcp_impl_);
  rtcp_receiver_ = new RTCPReceiver(0, system_clock_, dummy_rtp_rtcp_impl_);
  test_transport_ = new TestTransport(rtcp_receiver_);

  EXPECT_EQ(0, rtcp_sender_->Init());
  EXPECT_EQ(0, rtcp_sender_->RegisterSendTransport(test_transport_));
}

void RtcpFormatRembTest::TearDown() {
  delete rtcp_sender_;
  delete rtcp_receiver_;
  delete dummy_rtp_rtcp_impl_;
  delete test_transport_;
  delete system_clock_;
}

TEST_F(RtcpFormatRembTest, TestBasicAPI) {
  EXPECT_FALSE(rtcp_sender_->REMB());
  EXPECT_EQ(0, rtcp_sender_->SetREMBStatus(true));
  EXPECT_TRUE(rtcp_sender_->REMB());
  EXPECT_EQ(0, rtcp_sender_->SetREMBStatus(false));
  EXPECT_FALSE(rtcp_sender_->REMB());

  EXPECT_EQ(0, rtcp_sender_->SetREMBData(1234, 0, NULL));
}

TEST_F(RtcpFormatRembTest, TestNonCompund) {
  WebRtc_UWord32 SSRC = 456789;
  EXPECT_EQ(0, rtcp_sender_->SetRTCPStatus(kRtcpNonCompound));
  EXPECT_EQ(0, rtcp_sender_->SetREMBData(1234, 1, &SSRC));
  EXPECT_EQ(0, rtcp_sender_->SendRTCP(kRtcpRemb));
}

TEST_F(RtcpFormatRembTest, TestCompund) {
  WebRtc_UWord32 SSRCs[2] = {456789, 98765};
  EXPECT_EQ(0, rtcp_sender_->SetRTCPStatus(kRtcpCompound));
  EXPECT_EQ(0, rtcp_sender_->SetREMBData(1234, 2, SSRCs));
  EXPECT_EQ(0, rtcp_sender_->SendRTCP(kRtcpRemb));
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);

  return RUN_ALL_TESTS();
}

} // namespace
