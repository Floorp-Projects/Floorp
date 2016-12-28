/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#include "webrtc/modules/pacing/packet_router.h"
#include "webrtc/modules/remote_bitrate_estimator/remote_estimator_proxy.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/transport_feedback.h"
#include "webrtc/system_wrappers/include/clock.h"

using ::testing::_;
using ::testing::InSequence;
using ::testing::Invoke;

namespace webrtc {

class MockPacketRouter : public PacketRouter {
 public:
  MOCK_METHOD1(SendFeedback, bool(rtcp::TransportFeedback* packet));
};

class RemoteEstimatorProxyTest : public ::testing::Test {
 public:
  RemoteEstimatorProxyTest() : clock_(0), proxy_(&clock_, &router_) {}

 protected:
  void IncomingPacket(uint16_t seq, int64_t time_ms) {
    RTPHeader header;
    header.extension.hasTransportSequenceNumber = true;
    header.extension.transportSequenceNumber = seq;
    header.ssrc = kMediaSsrc;
    proxy_.IncomingPacket(time_ms, kDefaultPacketSize, header, true);
  }

  void Process() {
    clock_.AdvanceTimeMilliseconds(
        RemoteEstimatorProxy::kDefaultProcessIntervalMs);
    proxy_.Process();
  }

  SimulatedClock clock_;
  MockPacketRouter router_;
  RemoteEstimatorProxy proxy_;

  const size_t kDefaultPacketSize = 100;
  const uint32_t kMediaSsrc = 456;
  const uint16_t kBaseSeq = 10;
  const int64_t kBaseTimeMs = 123;
  const int64_t kMaxSmallDeltaMs =
      (rtcp::TransportFeedback::kDeltaScaleFactor * 0xFF) / 1000;
};

TEST_F(RemoteEstimatorProxyTest, SendsSinglePacketFeedback) {
  IncomingPacket(kBaseSeq, kBaseTimeMs);

  EXPECT_CALL(router_, SendFeedback(_))
      .Times(1)
      .WillOnce(Invoke([this](rtcp::TransportFeedback* packet) {
        packet->Build();
        EXPECT_EQ(kBaseSeq, packet->GetBaseSequence());
        EXPECT_EQ(kMediaSsrc, packet->GetMediaSourceSsrc());

        std::vector<rtcp::TransportFeedback::StatusSymbol> status_vec =
            packet->GetStatusVector();
        EXPECT_EQ(1u, status_vec.size());
        EXPECT_EQ(rtcp::TransportFeedback::StatusSymbol::kReceivedSmallDelta,
                  status_vec[0]);
        std::vector<int64_t> delta_vec = packet->GetReceiveDeltasUs();
        EXPECT_EQ(1u, delta_vec.size());
        EXPECT_EQ(kBaseTimeMs, (packet->GetBaseTimeUs() + delta_vec[0]) / 1000);
        return true;
      }));

  Process();
}

TEST_F(RemoteEstimatorProxyTest, SendsFeedbackWithVaryingDeltas) {
  IncomingPacket(kBaseSeq, kBaseTimeMs);
  IncomingPacket(kBaseSeq + 1, kBaseTimeMs + kMaxSmallDeltaMs);
  IncomingPacket(kBaseSeq + 2, kBaseTimeMs + (2 * kMaxSmallDeltaMs) + 1);

  EXPECT_CALL(router_, SendFeedback(_))
      .Times(1)
      .WillOnce(Invoke([this](rtcp::TransportFeedback* packet) {
        packet->Build();
        EXPECT_EQ(kBaseSeq, packet->GetBaseSequence());
        EXPECT_EQ(kMediaSsrc, packet->GetMediaSourceSsrc());

        std::vector<rtcp::TransportFeedback::StatusSymbol> status_vec =
            packet->GetStatusVector();
        EXPECT_EQ(3u, status_vec.size());
        EXPECT_EQ(rtcp::TransportFeedback::StatusSymbol::kReceivedSmallDelta,
                  status_vec[0]);
        EXPECT_EQ(rtcp::TransportFeedback::StatusSymbol::kReceivedSmallDelta,
                  status_vec[1]);
        EXPECT_EQ(rtcp::TransportFeedback::StatusSymbol::kReceivedLargeDelta,
                  status_vec[2]);

        std::vector<int64_t> delta_vec = packet->GetReceiveDeltasUs();
        EXPECT_EQ(3u, delta_vec.size());
        EXPECT_EQ(kBaseTimeMs, (packet->GetBaseTimeUs() + delta_vec[0]) / 1000);
        EXPECT_EQ(kMaxSmallDeltaMs, delta_vec[1] / 1000);
        EXPECT_EQ(kMaxSmallDeltaMs + 1, delta_vec[2] / 1000);
        return true;
      }));

  Process();
}

TEST_F(RemoteEstimatorProxyTest, SendsFragmentedFeedback) {
  const int64_t kTooLargeDelta =
      rtcp::TransportFeedback::kDeltaScaleFactor * (1 << 16);

  IncomingPacket(kBaseSeq, kBaseTimeMs);
  IncomingPacket(kBaseSeq + 1, kBaseTimeMs + kTooLargeDelta);

  InSequence s;
  EXPECT_CALL(router_, SendFeedback(_))
      .Times(1)
      .WillOnce(Invoke([kTooLargeDelta, this](rtcp::TransportFeedback* packet) {
        packet->Build();
        EXPECT_EQ(kBaseSeq, packet->GetBaseSequence());
        EXPECT_EQ(kMediaSsrc, packet->GetMediaSourceSsrc());

        std::vector<rtcp::TransportFeedback::StatusSymbol> status_vec =
            packet->GetStatusVector();
        EXPECT_EQ(1u, status_vec.size());
        EXPECT_EQ(rtcp::TransportFeedback::StatusSymbol::kReceivedSmallDelta,
                  status_vec[0]);
        std::vector<int64_t> delta_vec = packet->GetReceiveDeltasUs();
        EXPECT_EQ(1u, delta_vec.size());
        EXPECT_EQ(kBaseTimeMs, (packet->GetBaseTimeUs() + delta_vec[0]) / 1000);
        return true;
      }))
      .RetiresOnSaturation();

  EXPECT_CALL(router_, SendFeedback(_))
      .Times(1)
      .WillOnce(Invoke([kTooLargeDelta, this](rtcp::TransportFeedback* packet) {
        packet->Build();
        EXPECT_EQ(kBaseSeq + 1, packet->GetBaseSequence());
        EXPECT_EQ(kMediaSsrc, packet->GetMediaSourceSsrc());

        std::vector<rtcp::TransportFeedback::StatusSymbol> status_vec =
            packet->GetStatusVector();
        EXPECT_EQ(1u, status_vec.size());
        EXPECT_EQ(rtcp::TransportFeedback::StatusSymbol::kReceivedSmallDelta,
                  status_vec[0]);
        std::vector<int64_t> delta_vec = packet->GetReceiveDeltasUs();
        EXPECT_EQ(1u, delta_vec.size());
        EXPECT_EQ(kBaseTimeMs + kTooLargeDelta,
                  (packet->GetBaseTimeUs() + delta_vec[0]) / 1000);
        return true;
      }))
      .RetiresOnSaturation();

  Process();
}

TEST_F(RemoteEstimatorProxyTest, ResendsTimestampsOnReordering) {
  IncomingPacket(kBaseSeq, kBaseTimeMs);
  IncomingPacket(kBaseSeq + 2, kBaseTimeMs + 2);

  EXPECT_CALL(router_, SendFeedback(_))
      .Times(1)
      .WillOnce(Invoke([this](rtcp::TransportFeedback* packet) {
        packet->Build();
        EXPECT_EQ(kBaseSeq, packet->GetBaseSequence());
        EXPECT_EQ(kMediaSsrc, packet->GetMediaSourceSsrc());

        std::vector<int64_t> delta_vec = packet->GetReceiveDeltasUs();
        EXPECT_EQ(2u, delta_vec.size());
        EXPECT_EQ(kBaseTimeMs, (packet->GetBaseTimeUs() + delta_vec[0]) / 1000);
        EXPECT_EQ(2, delta_vec[1] / 1000);
        return true;
      }));

  Process();

  IncomingPacket(kBaseSeq + 1, kBaseTimeMs + 1);

  EXPECT_CALL(router_, SendFeedback(_))
      .Times(1)
      .WillOnce(Invoke([this](rtcp::TransportFeedback* packet) {
        packet->Build();
        EXPECT_EQ(kBaseSeq + 1, packet->GetBaseSequence());
        EXPECT_EQ(kMediaSsrc, packet->GetMediaSourceSsrc());

        std::vector<int64_t> delta_vec = packet->GetReceiveDeltasUs();
        EXPECT_EQ(2u, delta_vec.size());
        EXPECT_EQ(kBaseTimeMs + 1,
                  (packet->GetBaseTimeUs() + delta_vec[0]) / 1000);
        EXPECT_EQ(1, delta_vec[1] / 1000);
        return true;
      }));

  Process();
}

TEST_F(RemoteEstimatorProxyTest, RemovesTimestampsOutOfScope) {
  const int64_t kTimeoutTimeMs =
      kBaseTimeMs + RemoteEstimatorProxy::kBackWindowMs;

  IncomingPacket(kBaseSeq + 2, kBaseTimeMs);

  EXPECT_CALL(router_, SendFeedback(_))
      .Times(1)
      .WillOnce(Invoke([kTimeoutTimeMs, this](rtcp::TransportFeedback* packet) {
        packet->Build();
        EXPECT_EQ(kBaseSeq + 2, packet->GetBaseSequence());

        std::vector<int64_t> delta_vec = packet->GetReceiveDeltasUs();
        EXPECT_EQ(1u, delta_vec.size());
        EXPECT_EQ(kBaseTimeMs, (packet->GetBaseTimeUs() + delta_vec[0]) / 1000);
        return true;
      }));

  Process();

  IncomingPacket(kBaseSeq + 3, kTimeoutTimeMs);  // kBaseSeq + 2 times out here.

  EXPECT_CALL(router_, SendFeedback(_))
      .Times(1)
      .WillOnce(Invoke([kTimeoutTimeMs, this](rtcp::TransportFeedback* packet) {
        packet->Build();
        EXPECT_EQ(kBaseSeq + 3, packet->GetBaseSequence());

        std::vector<int64_t> delta_vec = packet->GetReceiveDeltasUs();
        EXPECT_EQ(1u, delta_vec.size());
        EXPECT_EQ(kTimeoutTimeMs,
                  (packet->GetBaseTimeUs() + delta_vec[0]) / 1000);
        return true;
      }));

  Process();

  // New group, with sequence starting below the first so that they may be
  // retransmitted.
  IncomingPacket(kBaseSeq, kBaseTimeMs - 1);
  IncomingPacket(kBaseSeq + 1, kTimeoutTimeMs - 1);

  EXPECT_CALL(router_, SendFeedback(_))
      .Times(1)
      .WillOnce(Invoke([kTimeoutTimeMs, this](rtcp::TransportFeedback* packet) {
        packet->Build();
        EXPECT_EQ(kBaseSeq, packet->GetBaseSequence());

        // Four status entries (kBaseSeq + 3 missing).
        EXPECT_EQ(4u, packet->GetStatusVector().size());

        // Only three actual timestamps.
        std::vector<int64_t> delta_vec = packet->GetReceiveDeltasUs();
        EXPECT_EQ(3u, delta_vec.size());
        EXPECT_EQ(kBaseTimeMs - 1,
                  (packet->GetBaseTimeUs() + delta_vec[0]) / 1000);
        EXPECT_EQ(kTimeoutTimeMs - kBaseTimeMs, delta_vec[1] / 1000);
        EXPECT_EQ(1, delta_vec[2] / 1000);
        return true;
      }));

  Process();
}

}  // namespace webrtc
