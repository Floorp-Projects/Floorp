/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "testing/gtest/include/gtest/gtest.h"
#include "webrtc/base/scoped_ptr.h"
#include "webrtc/modules/remote_bitrate_estimator/include/remote_bitrate_estimator.h"
#include "webrtc/modules/remote_bitrate_estimator/test/bwe_test.h"
#include "webrtc/modules/remote_bitrate_estimator/test/packet_receiver.h"
#include "webrtc/modules/remote_bitrate_estimator/test/packet_sender.h"
#include "webrtc/test/testsupport/fileutils.h"

using std::string;

namespace webrtc {
namespace testing {
namespace bwe {
#if BWE_TEST_LOGGING_COMPILE_TIME_ENABLE
// This test fixture is used to instantiate tests running with adaptive video
// senders.
class BweSimulation : public BweTest,
                      public ::testing::TestWithParam<BandwidthEstimatorType> {
 public:
  BweSimulation() : BweTest() {}
  virtual ~BweSimulation() {}

 protected:
  void SetUp() override { BweTest::SetUp(); }

 private:
  DISALLOW_COPY_AND_ASSIGN(BweSimulation);
};

INSTANTIATE_TEST_CASE_P(VideoSendersTest,
                        BweSimulation,
                        ::testing::Values(kRembEstimator,
                                          kFullSendSideEstimator,
                                          kNadaEstimator));

TEST_P(BweSimulation, SprintUplinkTest) {
  VerboseLogging(true);
  AdaptiveVideoSource source(0, 30, 300, 0, 0);
  PacketSender sender(&uplink_, &source, GetParam());
  RateCounterFilter counter1(&uplink_, 0, "sender_output");
  TraceBasedDeliveryFilter filter(&uplink_, 0, "link_capacity");
  RateCounterFilter counter2(&uplink_, 0, "receiver_input");
  PacketReceiver receiver(&uplink_, 0, GetParam(), true, true);
  ASSERT_TRUE(filter.Init(test::ResourcePath("sprint-uplink", "rx")));
  RunFor(60 * 1000);
}

TEST_P(BweSimulation, Verizon4gDownlinkTest) {
  VerboseLogging(true);
  AdaptiveVideoSource source(0, 30, 300, 0, 0);
  PacketSender sender(&downlink_, &source, GetParam());
  RateCounterFilter counter1(&downlink_, 0, "sender_output");
  TraceBasedDeliveryFilter filter(&downlink_, 0, "link_capacity");
  RateCounterFilter counter2(&downlink_, 0, "receiver_input");
  PacketReceiver receiver(&downlink_, 0, GetParam(), true, true);
  ASSERT_TRUE(filter.Init(test::ResourcePath("verizon4g-downlink", "rx")));
  RunFor(22 * 60 * 1000);
}

TEST_P(BweSimulation, Choke1000kbps500kbps1000kbpsBiDirectional) {
  VerboseLogging(true);

  const int kFlowIds[] = {0, 1};
  const size_t kNumFlows = sizeof(kFlowIds) / sizeof(kFlowIds[0]);

  AdaptiveVideoSource source(kFlowIds[0], 30, 300, 0, 0);
  PacketSender sender(&uplink_, &source, GetParam());
  ChokeFilter choke(&uplink_, kFlowIds[0]);
  RateCounterFilter counter(&uplink_, kFlowIds[0], "receiver_input_0");
  PacketReceiver receiver(&uplink_, kFlowIds[0], GetParam(), true, false);

  AdaptiveVideoSource source2(kFlowIds[1], 30, 300, 0, 0);
  PacketSender sender2(&downlink_, &source2, GetParam());
  ChokeFilter choke2(&downlink_, kFlowIds[1]);
  DelayFilter delay(&downlink_, CreateFlowIds(kFlowIds, kNumFlows));
  RateCounterFilter counter2(&downlink_, kFlowIds[1], "receiver_input_1");
  PacketReceiver receiver2(&downlink_, kFlowIds[1], GetParam(), true, false);

  choke2.SetCapacity(500);
  delay.SetDelay(0);

  choke.SetCapacity(1000);
  choke.SetMaxDelay(500);
  RunFor(60 * 1000);
  choke.SetCapacity(500);
  RunFor(60 * 1000);
  choke.SetCapacity(1000);
  RunFor(60 * 1000);
}

TEST_P(BweSimulation, Choke1000kbps500kbps1000kbps) {
  VerboseLogging(true);

  AdaptiveVideoSource source(0, 30, 300, 0, 0);
  PacketSender sender(&uplink_, &source, GetParam());
  ChokeFilter choke(&uplink_, 0);
  RateCounterFilter counter(&uplink_, 0, "receiver_input");
  PacketReceiver receiver(&uplink_, 0, GetParam(), true, false);

  choke.SetCapacity(1000);
  choke.SetMaxDelay(500);
  RunFor(60 * 1000);
  choke.SetCapacity(500);
  RunFor(60 * 1000);
  choke.SetCapacity(1000);
  RunFor(60 * 1000);
}

TEST_P(BweSimulation, PacerChoke1000kbps500kbps1000kbps) {
  VerboseLogging(true);
  PeriodicKeyFrameSource source(0, 30, 300, 0, 0, 1000);
  PacedVideoSender sender(&uplink_, &source, GetParam());
  ChokeFilter filter(&uplink_, 0);
  RateCounterFilter counter(&uplink_, 0, "receiver_input");
  PacketReceiver receiver(&uplink_, 0, GetParam(), true, true);
  filter.SetCapacity(1000);
  filter.SetMaxDelay(500);
  RunFor(60 * 1000);
  filter.SetCapacity(500);
  RunFor(60 * 1000);
  filter.SetCapacity(1000);
  RunFor(60 * 1000);
}

TEST_P(BweSimulation, PacerChoke10000kbps) {
  VerboseLogging(true);
  PeriodicKeyFrameSource source(0, 30, 300, 0, 0, 1000);
  PacedVideoSender sender(&uplink_, &source, GetParam());
  ChokeFilter filter(&uplink_, 0);
  RateCounterFilter counter(&uplink_, 0, "receiver_input");
  PacketReceiver receiver(&uplink_, 0, GetParam(), true, true);
  filter.SetCapacity(10000);
  filter.SetMaxDelay(500);
  RunFor(60 * 1000);
}

TEST_P(BweSimulation, PacerChoke200kbps30kbps200kbps) {
  VerboseLogging(true);
  PeriodicKeyFrameSource source(0, 30, 300, 0, 0, 1000);
  PacedVideoSender sender(&uplink_, &source, GetParam());
  ChokeFilter filter(&uplink_, 0);
  RateCounterFilter counter(&uplink_, 0, "receiver_input");
  PacketReceiver receiver(&uplink_, 0, GetParam(), true, true);
  filter.SetCapacity(200);
  filter.SetMaxDelay(500);
  RunFor(60 * 1000);
  filter.SetCapacity(30);
  RunFor(60 * 1000);
  filter.SetCapacity(200);
  RunFor(60 * 1000);
}

TEST_P(BweSimulation, Choke200kbps30kbps200kbps) {
  VerboseLogging(true);
  AdaptiveVideoSource source(0, 30, 300, 0, 0);
  PacketSender sender(&uplink_, &source, GetParam());
  ChokeFilter filter(&uplink_, 0);
  RateCounterFilter counter(&uplink_, 0, "receiver_input");
  PacketReceiver receiver(&uplink_, 0, GetParam(), true, true);
  filter.SetCapacity(200);
  filter.SetMaxDelay(500);
  RunFor(60 * 1000);
  filter.SetCapacity(30);
  RunFor(60 * 1000);
  filter.SetCapacity(200);
  RunFor(60 * 1000);
}

TEST_P(BweSimulation, GoogleWifiTrace3Mbps) {
  VerboseLogging(true);
  AdaptiveVideoSource source(0, 30, 300, 0, 0);
  PacketSender sender(&uplink_, &source, kRembEstimator);
  RateCounterFilter counter1(&uplink_, 0, "sender_output");
  TraceBasedDeliveryFilter filter(&uplink_, 0, "link_capacity");
  filter.SetMaxDelay(500);
  RateCounterFilter counter2(&uplink_, 0, "receiver_input");
  PacketReceiver receiver(&uplink_, 0, GetParam(), true, true);
  ASSERT_TRUE(filter.Init(test::ResourcePath("google-wifi-3mbps", "rx")));
  RunFor(300 * 1000);
}

TEST_P(BweSimulation, PacerGoogleWifiTrace3Mbps) {
  VerboseLogging(true);
  PeriodicKeyFrameSource source(0, 30, 300, 0, 0, 1000);
  PacedVideoSender sender(&uplink_, &source, kRembEstimator);
  RateCounterFilter counter1(&uplink_, 0, "sender_output");
  TraceBasedDeliveryFilter filter(&uplink_, 0, "link_capacity");
  filter.SetMaxDelay(500);
  RateCounterFilter counter2(&uplink_, 0, "receiver_input");
  PacketReceiver receiver(&uplink_, 0, GetParam(), true, true);
  ASSERT_TRUE(filter.Init(test::ResourcePath("google-wifi-3mbps", "rx")));
  RunFor(300 * 1000);
}

TEST_P(BweSimulation, SelfFairnessTest) {
  VerboseLogging(true);
  const int kAllFlowIds[] = {0, 1, 2};
  const size_t kNumFlows = sizeof(kAllFlowIds) / sizeof(kAllFlowIds[0]);
  rtc::scoped_ptr<AdaptiveVideoSource> sources[kNumFlows];
  rtc::scoped_ptr<PacketSender> senders[kNumFlows];
  for (size_t i = 0; i < kNumFlows; ++i) {
    // Streams started 20 seconds apart to give them different advantage when
    // competing for the bandwidth.
    sources[i].reset(
        new AdaptiveVideoSource(kAllFlowIds[i], 30, 300, 0, i * 20000));
    senders[i].reset(new PacketSender(&uplink_, sources[i].get(), GetParam()));
  }

  ChokeFilter choke(&uplink_, CreateFlowIds(kAllFlowIds, kNumFlows));
  choke.SetCapacity(1000);

  rtc::scoped_ptr<RateCounterFilter> rate_counters[kNumFlows];
  for (size_t i = 0; i < kNumFlows; ++i) {
    rate_counters[i].reset(new RateCounterFilter(
        &uplink_, CreateFlowIds(&kAllFlowIds[i], 1), "receiver_input"));
  }

  RateCounterFilter total_utilization(
      &uplink_, CreateFlowIds(kAllFlowIds, kNumFlows), "total_utilization");

  rtc::scoped_ptr<PacketReceiver> receivers[kNumFlows];
  for (size_t i = 0; i < kNumFlows; ++i) {
    receivers[i].reset(new PacketReceiver(&uplink_, kAllFlowIds[i], GetParam(),
                                          i == 0, false));
  }

  RunFor(30 * 60 * 1000);
}

TEST_P(BweSimulation, PacedSelfFairnessTest) {
  VerboseLogging(true);
  const int kAllFlowIds[] = {0, 1, 2};
  const size_t kNumFlows = sizeof(kAllFlowIds) / sizeof(kAllFlowIds[0]);
  rtc::scoped_ptr<PeriodicKeyFrameSource> sources[kNumFlows];
  rtc::scoped_ptr<PacedVideoSender> senders[kNumFlows];

  for (size_t i = 0; i < kNumFlows; ++i) {
    // Streams started 20 seconds apart to give them different advantage when
    // competing for the bandwidth.
    sources[i].reset(new PeriodicKeyFrameSource(kAllFlowIds[i], 30, 300, 0,
                                                i * 20000, 1000));
    senders[i].reset(
        new PacedVideoSender(&uplink_, sources[i].get(), GetParam()));
  }

  ChokeFilter choke(&uplink_, CreateFlowIds(kAllFlowIds, kNumFlows));
  choke.SetCapacity(1000);

  rtc::scoped_ptr<RateCounterFilter> rate_counters[kNumFlows];
  for (size_t i = 0; i < kNumFlows; ++i) {
    rate_counters[i].reset(new RateCounterFilter(
        &uplink_, CreateFlowIds(&kAllFlowIds[i], 1), "receiver_input"));
  }

  RateCounterFilter total_utilization(
      &uplink_, CreateFlowIds(kAllFlowIds, kNumFlows), "total_utilization");

  rtc::scoped_ptr<PacketReceiver> receivers[kNumFlows];
  for (size_t i = 0; i < kNumFlows; ++i) {
    receivers[i].reset(new PacketReceiver(&uplink_, kAllFlowIds[i], GetParam(),
                                          i == 0, false));
  }

  RunFor(30 * 60 * 1000);
}
#endif  // BWE_TEST_LOGGING_COMPILE_TIME_ENABLE
}  // namespace bwe
}  // namespace testing
}  // namespace webrtc
