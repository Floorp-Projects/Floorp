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
#include "webrtc/modules/remote_bitrate_estimator/include/remote_bitrate_estimator.h"
#include "webrtc/modules/remote_bitrate_estimator/test/bwe_test.h"
#include "webrtc/test/testsupport/fileutils.h"

using std::string;

namespace webrtc {
namespace testing {
namespace bwe {
#if BWE_TEST_LOGGING_COMPILE_TIME_ENABLE
BweTestConfig::EstimatorConfig CreateEstimatorConfig(
    int flow_id, bool plot_delay, bool plot_estimate) {
  static const AbsoluteSendTimeRemoteBitrateEstimatorFactory factory =
      AbsoluteSendTimeRemoteBitrateEstimatorFactory();

  return BweTestConfig::EstimatorConfig("AST", flow_id, &factory, kAimdControl,
                                        plot_delay, plot_estimate);
}

BweTestConfig MakeAdaptiveBweTestConfig() {
  BweTestConfig result;
  result.estimator_configs.push_back(CreateEstimatorConfig(0, true, true));
  return result;
}

BweTestConfig MakeMultiFlowBweTestConfig(int flow_count) {
  BweTestConfig result;
  for (int i = 0; i < flow_count; ++i) {
    result.estimator_configs.push_back(CreateEstimatorConfig(i, false, true));
  }
  return result;
}

// This test fixture is used to instantiate tests running with adaptive video
// senders.
class BweSimulation : public BweTest,
                      public ::testing::TestWithParam<BweTestConfig> {
 public:
  BweSimulation() : BweTest() {}
  virtual ~BweSimulation() {}

  virtual void SetUp() {
    const BweTestConfig& config = GetParam();
    SetupTestFromConfig(config);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(BweSimulation);
};

INSTANTIATE_TEST_CASE_P(VideoSendersTest, BweSimulation,
    ::testing::Values(MakeAdaptiveBweTestConfig()));

TEST_P(BweSimulation, SprintUplinkTest) {
  VerboseLogging(true);
  AdaptiveVideoSender sender(0, this, 30, 300, 0, 0);
  RateCounterFilter counter1(this, "sender_output");
  TraceBasedDeliveryFilter filter(this, "link_capacity");
  RateCounterFilter counter2(this, "receiver_input");
  ASSERT_TRUE(filter.Init(test::ResourcePath("sprint-uplink", "rx")));
  RunFor(60 * 1000);
}

TEST_P(BweSimulation, Verizon4gDownlinkTest) {
  VerboseLogging(true);
  AdaptiveVideoSender sender(0, this, 30, 300, 0, 0);
  RateCounterFilter counter1(this, "sender_output");
  TraceBasedDeliveryFilter filter(this, "link_capacity");
  RateCounterFilter counter2(this, "receiver_input");
  ASSERT_TRUE(filter.Init(test::ResourcePath("verizon4g-downlink", "rx")));
  RunFor(22 * 60 * 1000);
}

TEST_P(BweSimulation, Choke1000kbps500kbps1000kbps) {
  VerboseLogging(true);
  AdaptiveVideoSender sender(0, this, 30, 300, 0, 0);
  ChokeFilter filter(this);
  RateCounterFilter counter(this, "receiver_input");
  filter.SetCapacity(1000);
  filter.SetMaxDelay(500);
  RunFor(60 * 1000);
  filter.SetCapacity(500);
  RunFor(60 * 1000);
  filter.SetCapacity(1000);
  RunFor(60 * 1000);
}

TEST_P(BweSimulation, PacerChoke1000kbps500kbps1000kbps) {
  VerboseLogging(true);
  PeriodicKeyFrameSender source(0, NULL, 30, 300, 0, 0, 1000);
  PacedVideoSender sender(this, 300, &source);
  ChokeFilter filter(this);
  RateCounterFilter counter(this, "receiver_input");
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
  PeriodicKeyFrameSender source(0, NULL, 30, 300, 0, 0, 0);
  PacedVideoSender sender(this, 300, &source);
  ChokeFilter filter(this);
  RateCounterFilter counter(this, "receiver_input");
  filter.SetCapacity(10000);
  filter.SetMaxDelay(500);
  RunFor(60 * 1000);
}

TEST_P(BweSimulation, PacerChoke200kbps30kbps200kbps) {
  VerboseLogging(true);
  PeriodicKeyFrameSender source(0, NULL, 30, 300, 0, 0, 1000);
  PacedVideoSender sender(this, 300, &source);
  ChokeFilter filter(this);
  RateCounterFilter counter(this, "receiver_input");
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
  AdaptiveVideoSender sender(0, this, 30, 300, 0, 0);
  ChokeFilter filter(this);
  RateCounterFilter counter(this, "receiver_input");
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
  AdaptiveVideoSender sender(0, this, 30, 300, 0, 0);
  RateCounterFilter counter1(this, "sender_output");
  TraceBasedDeliveryFilter filter(this, "link_capacity");
  filter.SetMaxDelay(500);
  RateCounterFilter counter2(this, "receiver_input");
  ASSERT_TRUE(filter.Init(test::ResourcePath("google-wifi-3mbps", "rx")));
  RunFor(300 * 1000);
}

TEST_P(BweSimulation, PacerGoogleWifiTrace3Mbps) {
  VerboseLogging(true);
  PeriodicKeyFrameSender source(0, NULL, 30, 300, 0, 0, 1000);
  PacedVideoSender sender(this, 300, &source);
  RateCounterFilter counter1(this, "sender_output");
  TraceBasedDeliveryFilter filter(this, "link_capacity");
  filter.SetMaxDelay(500);
  RateCounterFilter counter2(this, "receiver_input");
  ASSERT_TRUE(filter.Init(test::ResourcePath("google-wifi-3mbps", "rx")));
  RunFor(300 * 1000);
}

class MultiFlowBweSimulation : public BweSimulation {
 public:
  MultiFlowBweSimulation() : BweSimulation() {}
  virtual ~MultiFlowBweSimulation() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(MultiFlowBweSimulation);
};

INSTANTIATE_TEST_CASE_P(VideoSendersTest, MultiFlowBweSimulation,
    ::testing::Values(MakeMultiFlowBweTestConfig(3)));

TEST_P(MultiFlowBweSimulation, SelfFairnessTest) {
  VerboseLogging(true);
  const int kAllFlowIds[] = {0, 1, 2};
  const size_t kNumFlows = sizeof(kAllFlowIds) / sizeof(kAllFlowIds[0]);
  scoped_ptr<AdaptiveVideoSender> senders[kNumFlows];
  for (size_t i = 0; i < kNumFlows; ++i) {
    senders[i].reset(new AdaptiveVideoSender(kAllFlowIds[i], this, 30, 300, 0,
                                             0));
  }
  // Second and third flow.
  ChokeFilter choke(this, CreateFlowIds(&kAllFlowIds[1], 2));
  choke.SetCapacity(1500);
  // First flow.
  ChokeFilter choke2(this, CreateFlowIds(&kAllFlowIds[0], 1));
  choke2.SetCapacity(1000);

  scoped_ptr<RateCounterFilter> rate_counters[kNumFlows];
  for (size_t i = 0; i < kNumFlows; ++i) {
    rate_counters[i].reset(new RateCounterFilter(
        this, CreateFlowIds(&kAllFlowIds[i], 1), "receiver_input"));
  }
  RunFor(30 * 60 * 1000);
}
#endif  // BWE_TEST_LOGGING_COMPILE_TIME_ENABLE
}  // namespace bwe
}  // namespace testing
}  // namespace webrtc
