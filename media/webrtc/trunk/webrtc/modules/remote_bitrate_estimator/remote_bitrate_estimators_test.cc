/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <sstream>

#include "webrtc/modules/remote_bitrate_estimator/include/remote_bitrate_estimator.h"
#include "webrtc/modules/remote_bitrate_estimator/test/bwe_test.h"
#include "webrtc/modules/remote_bitrate_estimator/test/packet_receiver.h"
#include "webrtc/modules/remote_bitrate_estimator/test/packet_sender.h"
#include "webrtc/test/testsupport/fileutils.h"
#include "webrtc/test/testsupport/perf_test.h"

using std::string;

namespace webrtc {
namespace testing {
namespace bwe {

class DefaultBweTest : public BweTest,
                       public ::testing::TestWithParam<BandwidthEstimatorType> {
 public:
  virtual ~DefaultBweTest() {}
};

INSTANTIATE_TEST_CASE_P(VideoSendersTest,
                        DefaultBweTest,
                        ::testing::Values(kRembEstimator,
                                          kFullSendSideEstimator));

TEST_P(DefaultBweTest, UnlimitedSpeed) {
  VideoSource source(0, 30, 300, 0, 0);
  PacketSender sender(&uplink_, &source, GetParam());
  PacketReceiver receiver(&uplink_, 0, GetParam(), false, false);
  RunFor(10 * 60 * 1000);
}

TEST_P(DefaultBweTest, SteadyLoss) {
  VideoSource source(0, 30, 300, 0, 0);
  PacketSender sender(&uplink_, &source, GetParam());
  LossFilter loss(&uplink_, 0);
  PacketReceiver receiver(&uplink_, 0, GetParam(), false, false);
  loss.SetLoss(20.0);
  RunFor(10 * 60 * 1000);
}

TEST_P(DefaultBweTest, IncreasingLoss1) {
  VideoSource source(0, 30, 300, 0, 0);
  PacketSender sender(&uplink_, &source, GetParam());
  LossFilter loss(&uplink_, 0);
  PacketReceiver receiver(&uplink_, 0, GetParam(), false, false);
  for (int i = 0; i < 76; ++i) {
    loss.SetLoss(i);
    RunFor(5000);
  }
}

TEST_P(DefaultBweTest, SteadyDelay) {
  VideoSource source(0, 30, 300, 0, 0);
  PacketSender sender(&uplink_, &source, GetParam());
  DelayFilter delay(&uplink_, 0);
  PacketReceiver receiver(&uplink_, 0, GetParam(), false, false);
  delay.SetDelay(1000);
  RunFor(10 * 60 * 1000);
}

TEST_P(DefaultBweTest, IncreasingDelay1) {
  VideoSource source(0, 30, 300, 0, 0);
  PacketSender sender(&uplink_, &source, GetParam());
  DelayFilter delay(&uplink_, 0);
  PacketReceiver receiver(&uplink_, 0, GetParam(), false, false);
  RunFor(10 * 60 * 1000);
  for (int i = 0; i < 30 * 2; ++i) {
    delay.SetDelay(i);
    RunFor(10 * 1000);
  }
  RunFor(10 * 60 * 1000);
}

TEST_P(DefaultBweTest, IncreasingDelay2) {
  VideoSource source(0, 30, 300, 0, 0);
  PacketSender sender(&uplink_, &source, GetParam());
  DelayFilter delay(&uplink_, 0);
  RateCounterFilter counter(&uplink_, 0, "");
  PacketReceiver receiver(&uplink_, 0, GetParam(), false, false);
  RunFor(1 * 60 * 1000);
  for (int i = 1; i < 51; ++i) {
    delay.SetDelay(10.0f * i);
    RunFor(10 * 1000);
  }
  delay.SetDelay(0.0f);
  RunFor(10 * 60 * 1000);
}

TEST_P(DefaultBweTest, JumpyDelay1) {
  VideoSource source(0, 30, 300, 0, 0);
  PacketSender sender(&uplink_, &source, GetParam());
  DelayFilter delay(&uplink_, 0);
  PacketReceiver receiver(&uplink_, 0, GetParam(), false, false);
  RunFor(10 * 60 * 1000);
  for (int i = 1; i < 200; ++i) {
    delay.SetDelay((10 * i) % 500);
    RunFor(1000);
    delay.SetDelay(1.0f);
    RunFor(1000);
  }
  delay.SetDelay(0.0f);
  RunFor(10 * 60 * 1000);
}

TEST_P(DefaultBweTest, SteadyJitter) {
  VideoSource source(0, 30, 300, 0, 0);
  PacketSender sender(&uplink_, &source, GetParam());
  JitterFilter jitter(&uplink_, 0);
  RateCounterFilter counter(&uplink_, 0, "");
  PacketReceiver receiver(&uplink_, 0, GetParam(), false, false);
  jitter.SetJitter(20);
  RunFor(2 * 60 * 1000);
}

TEST_P(DefaultBweTest, IncreasingJitter1) {
  VideoSource source(0, 30, 300, 0, 0);
  PacketSender sender(&uplink_, &source, GetParam());
  JitterFilter jitter(&uplink_, 0);
  PacketReceiver receiver(&uplink_, 0, GetParam(), false, false);
  for (int i = 0; i < 2 * 60 * 2; ++i) {
    jitter.SetJitter(i);
    RunFor(10 * 1000);
  }
  RunFor(10 * 60 * 1000);
}

TEST_P(DefaultBweTest, IncreasingJitter2) {
  VideoSource source(0, 30, 300, 0, 0);
  PacketSender sender(&uplink_, &source, GetParam());
  JitterFilter jitter(&uplink_, 0);
  PacketReceiver receiver(&uplink_, 0, GetParam(), false, false);
  RunFor(30 * 1000);
  for (int i = 1; i < 51; ++i) {
    jitter.SetJitter(10.0f * i);
    RunFor(10 * 1000);
  }
  jitter.SetJitter(0.0f);
  RunFor(10 * 60 * 1000);
}

TEST_P(DefaultBweTest, SteadyReorder) {
  VideoSource source(0, 30, 300, 0, 0);
  PacketSender sender(&uplink_, &source, GetParam());
  ReorderFilter reorder(&uplink_, 0);
  PacketReceiver receiver(&uplink_, 0, GetParam(), false, false);
  reorder.SetReorder(20.0);
  RunFor(10 * 60 * 1000);
}

TEST_P(DefaultBweTest, IncreasingReorder1) {
  VideoSource source(0, 30, 300, 0, 0);
  PacketSender sender(&uplink_, &source, GetParam());
  ReorderFilter reorder(&uplink_, 0);
  PacketReceiver receiver(&uplink_, 0, GetParam(), false, false);
  for (int i = 0; i < 76; ++i) {
    reorder.SetReorder(i);
    RunFor(5000);
  }
}

TEST_P(DefaultBweTest, SteadyChoke) {
  VideoSource source(0, 30, 300, 0, 0);
  PacketSender sender(&uplink_, &source, GetParam());
  ChokeFilter choke(&uplink_, 0);
  PacketReceiver receiver(&uplink_, 0, GetParam(), false, false);
  choke.SetCapacity(140);
  RunFor(10 * 60 * 1000);
}

TEST_P(DefaultBweTest, IncreasingChoke1) {
  VideoSource source(0, 30, 300, 0, 0);
  PacketSender sender(&uplink_, &source, GetParam());
  ChokeFilter choke(&uplink_, 0);
  PacketReceiver receiver(&uplink_, 0, GetParam(), false, false);
  for (int i = 1200; i >= 100; i -= 100) {
    choke.SetCapacity(i);
    RunFor(5000);
  }
}

TEST_P(DefaultBweTest, IncreasingChoke2) {
  VideoSource source(0, 30, 300, 0, 0);
  PacketSender sender(&uplink_, &source, GetParam());
  ChokeFilter choke(&uplink_, 0);
  PacketReceiver receiver(&uplink_, 0, GetParam(), false, false);
  RunFor(60 * 1000);
  for (int i = 1200; i >= 100; i -= 20) {
    choke.SetCapacity(i);
    RunFor(1000);
  }
}

TEST_P(DefaultBweTest, Multi1) {
  VideoSource source(0, 30, 300, 0, 0);
  PacketSender sender(&uplink_, &source, GetParam());
  DelayFilter delay(&uplink_, 0);
  ChokeFilter choke(&uplink_, 0);
  RateCounterFilter counter(&uplink_, 0, "");
  PacketReceiver receiver(&uplink_, 0, GetParam(), false, false);
  choke.SetCapacity(1000);
  RunFor(1 * 60 * 1000);
  for (int i = 1; i < 51; ++i) {
    delay.SetDelay(100.0f * i);
    RunFor(10 * 1000);
  }
  RunFor(500 * 1000);
  delay.SetDelay(0.0f);
  RunFor(5 * 60 * 1000);
}

TEST_P(DefaultBweTest, Multi2) {
  VideoSource source(0, 30, 300, 0, 0);
  PacketSender sender(&uplink_, &source, GetParam());
  ChokeFilter choke(&uplink_, 0);
  JitterFilter jitter(&uplink_, 0);
  RateCounterFilter counter(&uplink_, 0, "");
  PacketReceiver receiver(&uplink_, 0, GetParam(), false, false);
  choke.SetCapacity(2000);
  jitter.SetJitter(120);
  RunFor(5 * 60 * 1000);
}

// This test fixture is used to instantiate tests running with adaptive video
// senders.
class BweFeedbackTest
    : public BweTest,
      public ::testing::TestWithParam<BandwidthEstimatorType> {
 public:
  BweFeedbackTest() : BweTest() {}
  virtual ~BweFeedbackTest() {}

  void PrintResults(double max_throughput_kbps, Stats<double> throughput_kbps,
                    Stats<double> delay_ms) {
    double utilization = throughput_kbps.GetMean() / max_throughput_kbps;
    webrtc::test::PrintResult("BwePerformance",
                              GetTestName(),
                              "Utilization",
                              utilization * 100.0,
                              "%",
                              false);
    std::stringstream ss;
    ss << throughput_kbps.GetStdDev() / throughput_kbps.GetMean();
    webrtc::test::PrintResult("BwePerformance",
                              GetTestName(),
                              "Utilization var coeff",
                              ss.str(),
                              "",
                              false);
    webrtc::test::PrintResult("BwePerformance",
                              GetTestName(),
                              "Average delay",
                              delay_ms.AsString(),
                              "ms",
                              false);
  }

 protected:
  void SetUp() override { BweTest::SetUp(); }

 private:
  DISALLOW_COPY_AND_ASSIGN(BweFeedbackTest);
};

INSTANTIATE_TEST_CASE_P(VideoSendersTest,
                        BweFeedbackTest,
                        ::testing::Values(kRembEstimator,
                                          kFullSendSideEstimator));

TEST_P(BweFeedbackTest, Choke1000kbps500kbps1000kbps) {
  AdaptiveVideoSource source(0, 30, 300, 0, 0);
  PacketSender sender(&uplink_, &source, GetParam());
  ChokeFilter filter(&uplink_, 0);
  RateCounterFilter counter(&uplink_, 0, "receiver_input");
  PacketReceiver receiver(&uplink_, 0, GetParam(), false, false);
  const int kHighCapacityKbps = 1000;
  const int kLowCapacityKbps = 500;
  filter.SetCapacity(kHighCapacityKbps);
  filter.SetMaxDelay(500);
  RunFor(60 * 1000);
  filter.SetCapacity(kLowCapacityKbps);
  RunFor(60 * 1000);
  filter.SetCapacity(kHighCapacityKbps);
  RunFor(60 * 1000);
  PrintResults((2 * kHighCapacityKbps + kLowCapacityKbps) / 3.0,
               counter.GetBitrateStats(), filter.GetDelayStats());
}

TEST_P(BweFeedbackTest, Choke200kbps30kbps200kbps) {
  AdaptiveVideoSource source(0, 30, 300, 0, 0);
  PacketSender sender(&uplink_, &source, GetParam());
  ChokeFilter filter(&uplink_, 0);
  RateCounterFilter counter(&uplink_, 0, "receiver_input");
  PacketReceiver receiver(&uplink_, 0, GetParam(), false, false);
  const int kHighCapacityKbps = 200;
  const int kLowCapacityKbps = 30;
  filter.SetCapacity(kHighCapacityKbps);
  filter.SetMaxDelay(500);
  RunFor(60 * 1000);
  filter.SetCapacity(kLowCapacityKbps);
  RunFor(60 * 1000);
  filter.SetCapacity(kHighCapacityKbps);
  RunFor(60 * 1000);

  PrintResults((2 * kHighCapacityKbps + kLowCapacityKbps) / 3.0,
               counter.GetBitrateStats(), filter.GetDelayStats());
}

TEST_P(BweFeedbackTest, Verizon4gDownlinkTest) {
  AdaptiveVideoSource source(0, 30, 300, 0, 0);
  PacketSender sender(&uplink_, &source, GetParam());
  RateCounterFilter counter1(&uplink_, 0, "sender_output");
  TraceBasedDeliveryFilter filter(&uplink_, 0, "link_capacity");
  RateCounterFilter counter2(&uplink_, 0, "receiver_input");
  PacketReceiver receiver(&uplink_, 0, GetParam(), false, false);
  ASSERT_TRUE(filter.Init(test::ResourcePath("verizon4g-downlink", "rx")));
  RunFor(22 * 60 * 1000);
  PrintResults(filter.GetBitrateStats().GetMean(), counter2.GetBitrateStats(),
               filter.GetDelayStats());
}

// webrtc:3277
TEST_P(BweFeedbackTest, DISABLED_GoogleWifiTrace3Mbps) {
  AdaptiveVideoSource source(0, 30, 300, 0, 0);
  PacketSender sender(&uplink_, &source, GetParam());
  RateCounterFilter counter1(&uplink_, 0, "sender_output");
  TraceBasedDeliveryFilter filter(&uplink_, 0, "link_capacity");
  filter.SetMaxDelay(500);
  RateCounterFilter counter2(&uplink_, 0, "receiver_input");
  PacketReceiver receiver(&uplink_, 0, GetParam(), false, false);
  ASSERT_TRUE(filter.Init(test::ResourcePath("google-wifi-3mbps", "rx")));
  RunFor(300 * 1000);
  PrintResults(filter.GetBitrateStats().GetMean(), counter2.GetBitrateStats(),
               filter.GetDelayStats());
}
}  // namespace bwe
}  // namespace testing
}  // namespace webrtc
