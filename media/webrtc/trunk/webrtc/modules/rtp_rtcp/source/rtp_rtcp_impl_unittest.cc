/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#include "webrtc/common_types.h"
#include "webrtc/modules/rtp_rtcp/interface/rtp_rtcp_defines.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_rtcp_impl.h"

namespace webrtc {
namespace {
const uint32_t kSenderSsrc = 0x12345;
const uint32_t kReceiverSsrc = 0x23456;
const uint32_t kOneWayNetworkDelayMs = 100;

class RtcpRttStatsTestImpl : public RtcpRttStats {
 public:
  RtcpRttStatsTestImpl() : rtt_ms_(0) {}
  virtual ~RtcpRttStatsTestImpl() {}

  virtual void OnRttUpdate(uint32_t rtt_ms) {
    rtt_ms_ = rtt_ms;
  }
  virtual uint32_t LastProcessedRtt() const {
    return rtt_ms_;
  }
  uint32_t rtt_ms_;
};

class SendTransport : public Transport,
                      public NullRtpData {
 public:
  SendTransport() : receiver_(NULL), clock_(NULL), delay_ms_(0) {}

  void SetRtpRtcpModule(ModuleRtpRtcpImpl* receiver) {
    receiver_ = receiver;
  }
  void SimulateNetworkDelay(uint32_t delay_ms, SimulatedClock* clock) {
    clock_ = clock;
    delay_ms_ = delay_ms;
  }
  virtual int SendPacket(int /*ch*/, const void* /*data*/, int /*len*/) {
    return -1;
  }
  virtual int SendRTCPPacket(int /*ch*/, const void *data, int len) {
    if (clock_) {
      clock_->AdvanceTimeMilliseconds(delay_ms_);
    }
    EXPECT_TRUE(receiver_ != NULL);
    EXPECT_EQ(0, receiver_->IncomingRtcpPacket(
        static_cast<const uint8_t*>(data), len));
    return len;
  }
  ModuleRtpRtcpImpl* receiver_;
  SimulatedClock* clock_;
  uint32_t delay_ms_;
};

class RtpRtcpModule {
 public:
  RtpRtcpModule(SimulatedClock* clock)
      : receive_statistics_(ReceiveStatistics::Create(clock)) {
    RtpRtcp::Configuration config;
    config.audio = false;
    config.clock = clock;
    config.outgoing_transport = &transport_;
    config.receive_statistics = receive_statistics_.get();
    config.rtt_stats = &rtt_stats_;

    impl_.reset(new ModuleRtpRtcpImpl(config));
    EXPECT_EQ(0, impl_->SetRTCPStatus(kRtcpCompound));

    transport_.SimulateNetworkDelay(kOneWayNetworkDelayMs, clock);
  }
  scoped_ptr<ReceiveStatistics> receive_statistics_;
  SendTransport transport_;
  RtcpRttStatsTestImpl rtt_stats_;
  scoped_ptr<ModuleRtpRtcpImpl> impl_;
};
}  // namespace

class RtpRtcpImplTest : public ::testing::Test {
 protected:
  RtpRtcpImplTest()
      : clock_(1335900000),
        sender_(&clock_),
        receiver_(&clock_) {
    // Send module.
    EXPECT_EQ(0, sender_.impl_->SetSendingStatus(true));
    EXPECT_EQ(0, sender_.impl_->SetSSRC(kSenderSsrc));
    sender_.impl_->SetRemoteSSRC(kReceiverSsrc);
    // Receive module.
    EXPECT_EQ(0, receiver_.impl_->SetSendingStatus(false));
    EXPECT_EQ(0, receiver_.impl_->SetSSRC(kReceiverSsrc));
    receiver_.impl_->SetRemoteSSRC(kSenderSsrc);
    // Transport settings.
    sender_.transport_.SetRtpRtcpModule(receiver_.impl_.get());
    receiver_.transport_.SetRtpRtcpModule(sender_.impl_.get());
  }
  SimulatedClock clock_;
  RtpRtcpModule sender_;
  RtpRtcpModule receiver_;
};

TEST_F(RtpRtcpImplTest, Rtt) {
  RTPHeader header = {};
  header.timestamp = 1;
  header.sequenceNumber = 123;
  header.ssrc = kSenderSsrc;
  header.headerLength = 12;
  receiver_.receive_statistics_->IncomingPacket(header, 100, false);

  // Sender module should send a SR.
  EXPECT_EQ(0, sender_.impl_->SendRTCP(kRtcpReport));

  // Receiver module should send a RR with a response to the last received SR.
  clock_.AdvanceTimeMilliseconds(1000);
  EXPECT_EQ(0, receiver_.impl_->SendRTCP(kRtcpReport));

  // Verify RTT.
  uint16_t rtt;
  uint16_t avg_rtt;
  uint16_t min_rtt;
  uint16_t max_rtt;
  EXPECT_EQ(0,
      sender_.impl_->RTT(kReceiverSsrc, &rtt, &avg_rtt, &min_rtt, &max_rtt));
  EXPECT_EQ(2 * kOneWayNetworkDelayMs, rtt);
  EXPECT_EQ(2 * kOneWayNetworkDelayMs, avg_rtt);
  EXPECT_EQ(2 * kOneWayNetworkDelayMs, min_rtt);
  EXPECT_EQ(2 * kOneWayNetworkDelayMs, max_rtt);

  // No RTT from other ssrc.
  EXPECT_EQ(-1,
      sender_.impl_->RTT(kReceiverSsrc+1, &rtt, &avg_rtt, &min_rtt, &max_rtt));

  // Verify RTT from rtt_stats config.
  EXPECT_EQ(0U, sender_.rtt_stats_.LastProcessedRtt());
  EXPECT_EQ(0U, sender_.impl_->rtt_ms());
  sender_.impl_->Process();
  EXPECT_EQ(2 * kOneWayNetworkDelayMs, sender_.rtt_stats_.LastProcessedRtt());
  EXPECT_EQ(2 * kOneWayNetworkDelayMs, sender_.impl_->rtt_ms());
}

TEST_F(RtpRtcpImplTest, SetRtcpXrRrtrStatus) {
  EXPECT_FALSE(receiver_.impl_->RtcpXrRrtrStatus());
  receiver_.impl_->SetRtcpXrRrtrStatus(true);
  EXPECT_TRUE(receiver_.impl_->RtcpXrRrtrStatus());
}

TEST_F(RtpRtcpImplTest, RttForReceiverOnly) {
  receiver_.impl_->SetRtcpXrRrtrStatus(true);

  // Receiver module should send a Receiver time reference report (RTRR).
  EXPECT_EQ(0, receiver_.impl_->SendRTCP(kRtcpReport));

  // Sender module should send a response to the last received RTRR (DLRR).
  clock_.AdvanceTimeMilliseconds(1000);
  EXPECT_EQ(0, sender_.impl_->SendRTCP(kRtcpReport));

  // Verify RTT.
  EXPECT_EQ(0U, receiver_.rtt_stats_.LastProcessedRtt());
  EXPECT_EQ(0U, receiver_.impl_->rtt_ms());
  receiver_.impl_->Process();
  EXPECT_EQ(2 * kOneWayNetworkDelayMs, receiver_.rtt_stats_.LastProcessedRtt());
  EXPECT_EQ(2 * kOneWayNetworkDelayMs, receiver_.impl_->rtt_ms());
}
}  // namespace webrtc
