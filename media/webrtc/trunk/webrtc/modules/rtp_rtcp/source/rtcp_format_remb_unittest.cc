/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "testing/gtest/include/gtest/gtest.h"

#include "webrtc/common_types.h"
#include "webrtc/modules/remote_bitrate_estimator/include/bwe_defines.h"
#include "webrtc/modules/remote_bitrate_estimator/include/mock/mock_remote_bitrate_observer.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_receiver.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_sender.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_rtcp_impl.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_utility.h"
#include "webrtc/typedefs.h"

namespace {

using namespace webrtc;


class TestTransport : public Transport {
 public:
  TestTransport(RTCPReceiver* rtcp_receiver) :
    rtcp_receiver_(rtcp_receiver) {
  }

  virtual int SendPacket(int /*channel*/,
                         const void* /*data*/,
                         int /*len*/) OVERRIDE {
    return -1;
  }
  virtual int SendRTCPPacket(int /*channel*/,
                             const void *packet,
                             int packetLength) OVERRIDE {
    RTCPUtility::RTCPParserV2 rtcpParser((uint8_t*)packet,
                                         (int32_t)packetLength,
                                         true); // Allow non-compound RTCP

    EXPECT_TRUE(rtcpParser.IsValid());
    RTCPHelp::RTCPPacketInformation rtcpPacketInformation;
    EXPECT_EQ(0, rtcp_receiver_->IncomingRTCPPacket(rtcpPacketInformation,
                                                    &rtcpParser));

    EXPECT_EQ((uint32_t)kRtcpRemb,
              rtcpPacketInformation.rtcpPacketTypeFlags & kRtcpRemb);
    EXPECT_EQ((uint32_t)1234,
              rtcpPacketInformation.receiverEstimatedMaxBitrate);
    return packetLength;
  }
 private:
  RTCPReceiver* rtcp_receiver_;
};


class RtcpFormatRembTest : public ::testing::Test {
 protected:
  static const uint32_t kRemoteBitrateEstimatorMinBitrateBps = 30000;

  RtcpFormatRembTest()
      : over_use_detector_options_(),
        system_clock_(Clock::GetRealTimeClock()),
        receive_statistics_(ReceiveStatistics::Create(system_clock_)),
        remote_bitrate_observer_(),
        remote_bitrate_estimator_(
            RemoteBitrateEstimatorFactory().Create(
                &remote_bitrate_observer_,
                system_clock_,
                kMimdControl,
                kRemoteBitrateEstimatorMinBitrateBps)) {}
  virtual void SetUp() OVERRIDE;
  virtual void TearDown() OVERRIDE;

  OverUseDetectorOptions over_use_detector_options_;
  Clock* system_clock_;
  ModuleRtpRtcpImpl* dummy_rtp_rtcp_impl_;
  scoped_ptr<ReceiveStatistics> receive_statistics_;
  RTCPSender* rtcp_sender_;
  RTCPReceiver* rtcp_receiver_;
  TestTransport* test_transport_;
  MockRemoteBitrateObserver remote_bitrate_observer_;
  scoped_ptr<RemoteBitrateEstimator> remote_bitrate_estimator_;
};

void RtcpFormatRembTest::SetUp() {
  RtpRtcp::Configuration configuration;
  configuration.id = 0;
  configuration.audio = false;
  configuration.clock = system_clock_;
  configuration.remote_bitrate_estimator = remote_bitrate_estimator_.get();
  dummy_rtp_rtcp_impl_ = new ModuleRtpRtcpImpl(configuration);
  rtcp_sender_ =
      new RTCPSender(0, false, system_clock_, receive_statistics_.get());
  rtcp_receiver_ = new RTCPReceiver(0, system_clock_, dummy_rtp_rtcp_impl_);
  test_transport_ = new TestTransport(rtcp_receiver_);

  EXPECT_EQ(0, rtcp_sender_->RegisterSendTransport(test_transport_));
}

void RtcpFormatRembTest::TearDown() {
  delete rtcp_sender_;
  delete rtcp_receiver_;
  delete dummy_rtp_rtcp_impl_;
  delete test_transport_;
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
  uint32_t SSRC = 456789;
  EXPECT_EQ(0, rtcp_sender_->SetRTCPStatus(kRtcpNonCompound));
  EXPECT_EQ(0, rtcp_sender_->SetREMBData(1234, 1, &SSRC));
  RTCPSender::FeedbackState feedback_state =
      dummy_rtp_rtcp_impl_->GetFeedbackState();
  EXPECT_EQ(0, rtcp_sender_->SendRTCP(feedback_state, kRtcpRemb));
}

TEST_F(RtcpFormatRembTest, TestCompund) {
  uint32_t SSRCs[2] = {456789, 98765};
  EXPECT_EQ(0, rtcp_sender_->SetRTCPStatus(kRtcpCompound));
  EXPECT_EQ(0, rtcp_sender_->SetREMBData(1234, 2, SSRCs));
  RTCPSender::FeedbackState feedback_state =
      dummy_rtp_rtcp_impl_->GetFeedbackState();
  EXPECT_EQ(0, rtcp_sender_->SendRTCP(feedback_state, kRtcpRemb));
}
}  // namespace
