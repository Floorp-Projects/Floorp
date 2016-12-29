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
#include "webrtc/modules/remote_bitrate_estimator/remote_bitrate_estimator_single_stream.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_receiver.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_sender.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_rtcp_impl.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_utility.h"
#include "webrtc/test/null_transport.h"
#include "webrtc/typedefs.h"

namespace webrtc {
namespace {

class TestTransport : public Transport {
 public:
  explicit TestTransport(RTCPReceiver* rtcp_receiver)
      : rtcp_receiver_(rtcp_receiver) {}

  bool SendRtp(const uint8_t* /*data*/,
               size_t /*len*/,
               const PacketOptions& options) override {
    return false;
  }
  bool SendRtcp(const uint8_t* packet, size_t packetLength) override {
    RTCPUtility::RTCPParserV2 rtcpParser(packet, packetLength,
                                         true);  // Allow non-compound RTCP

    EXPECT_TRUE(rtcpParser.IsValid());
    RTCPHelp::RTCPPacketInformation rtcpPacketInformation;
    EXPECT_EQ(0, rtcp_receiver_->IncomingRTCPPacket(rtcpPacketInformation,
                                                    &rtcpParser));

    EXPECT_EQ((uint32_t)kRtcpRemb,
              rtcpPacketInformation.rtcpPacketTypeFlags & kRtcpRemb);
    EXPECT_EQ((uint32_t)1234,
              rtcpPacketInformation.receiverEstimatedMaxBitrate);
    return true;
  }

 private:
  RTCPReceiver* rtcp_receiver_;
};

class RtcpFormatRembTest : public ::testing::Test {
 protected:
  RtcpFormatRembTest()
      : over_use_detector_options_(),
        system_clock_(Clock::GetRealTimeClock()),
        dummy_rtp_rtcp_impl_(nullptr),
        receive_statistics_(ReceiveStatistics::Create(system_clock_)),
        rtcp_sender_(nullptr),
        rtcp_receiver_(nullptr),
        test_transport_(nullptr),
        remote_bitrate_observer_(),
        remote_bitrate_estimator_(
            new RemoteBitrateEstimatorSingleStream(&remote_bitrate_observer_,
                                                   system_clock_)) {}
  void SetUp() override;
  void TearDown() override;

  OverUseDetectorOptions over_use_detector_options_;
  Clock* system_clock_;
  ModuleRtpRtcpImpl* dummy_rtp_rtcp_impl_;
  rtc::scoped_ptr<ReceiveStatistics> receive_statistics_;
  RTCPSender* rtcp_sender_;
  RTCPReceiver* rtcp_receiver_;
  TestTransport* test_transport_;
  test::NullTransport null_transport_;
  MockRemoteBitrateObserver remote_bitrate_observer_;
  rtc::scoped_ptr<RemoteBitrateEstimator> remote_bitrate_estimator_;
};

void RtcpFormatRembTest::SetUp() {
  RtpRtcp::Configuration configuration;
  configuration.audio = false;
  configuration.clock = system_clock_;
  configuration.remote_bitrate_estimator = remote_bitrate_estimator_.get();
  configuration.outgoing_transport = &null_transport_;
  dummy_rtp_rtcp_impl_ = new ModuleRtpRtcpImpl(configuration);
  rtcp_receiver_ = new RTCPReceiver(system_clock_, false, nullptr, nullptr,
                                    nullptr, nullptr, dummy_rtp_rtcp_impl_);
  test_transport_ = new TestTransport(rtcp_receiver_);
  rtcp_sender_ = new RTCPSender(false, system_clock_, receive_statistics_.get(),
                                nullptr, test_transport_);
}

void RtcpFormatRembTest::TearDown() {
  delete rtcp_sender_;
  delete rtcp_receiver_;
  delete dummy_rtp_rtcp_impl_;
  delete test_transport_;
}

TEST_F(RtcpFormatRembTest, TestRembStatus) {
  EXPECT_FALSE(rtcp_sender_->REMB());
  rtcp_sender_->SetREMBStatus(true);
  EXPECT_TRUE(rtcp_sender_->REMB());
  rtcp_sender_->SetREMBStatus(false);
  EXPECT_FALSE(rtcp_sender_->REMB());
}

TEST_F(RtcpFormatRembTest, TestNonCompund) {
  uint32_t SSRC = 456789;
  rtcp_sender_->SetRTCPStatus(RtcpMode::kReducedSize);
  rtcp_sender_->SetREMBData(1234, std::vector<uint32_t>(1, SSRC));
  RTCPSender::FeedbackState feedback_state =
      dummy_rtp_rtcp_impl_->GetFeedbackState();
  EXPECT_EQ(0, rtcp_sender_->SendRTCP(feedback_state, kRtcpRemb));
}

TEST_F(RtcpFormatRembTest, TestCompund) {
  uint32_t SSRCs[2] = {456789, 98765};
  rtcp_sender_->SetRTCPStatus(RtcpMode::kCompound);
  rtcp_sender_->SetREMBData(1234, std::vector<uint32_t>(SSRCs, SSRCs + 2));
  RTCPSender::FeedbackState feedback_state =
      dummy_rtp_rtcp_impl_->GetFeedbackState();
  EXPECT_EQ(0, rtcp_sender_->SendRTCP(feedback_state, kRtcpRemb));
}
}  // namespace
}  // namespace webrtc
