/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/remote_bitrate_estimator/include/remote_bitrate_estimator.h"
#include "webrtc/modules/remote_bitrate_estimator/test/bwe_test.h"
#include "webrtc/test/testsupport/fileutils.h"

namespace webrtc {
namespace testing {
namespace bwe {
#if BWE_TEST_LOGGING_COMPILE_TIME_ENABLE
std::vector<BweTestConfig::EstimatorConfig> SingleEstimatorConfig() {
  static const RemoteBitrateEstimatorFactory factory =
      AbsoluteSendTimeRemoteBitrateEstimatorFactory();

  std::vector<BweTestConfig::EstimatorConfig> result;
  result.push_back(BweTestConfig::EstimatorConfig("AST", &factory));
  return result;
}

std::vector<const PacketSenderFactory*> AdaptiveVideoSenderFactories(
    uint32_t count) {
  static const AdaptiveVideoPacketSenderFactory factories[] = {
    AdaptiveVideoPacketSenderFactory(30.00f, 150, 0x1234, 0.13f),
    AdaptiveVideoPacketSenderFactory(30.00f, 300, 0x3456, 0.26f),
    AdaptiveVideoPacketSenderFactory(15.00f, 600, 0x4567, 0.39f),
  };

  assert(count <= sizeof(factories) / sizeof(factories[0]));

  std::vector<const PacketSenderFactory*> result;
  for (uint32_t i = 0; i < count; ++i) {
    result.push_back(&factories[i]);
  }
  return result;
}

BweTestConfig MakeAdaptiveBweTestConfig(uint32_t sender_count) {
  BweTestConfig result = {
    AdaptiveVideoSenderFactories(sender_count), SingleEstimatorConfig()
  };
  return result;
}

// This test fixture is used to instantiate tests running with adaptive video
// senders.
class BweSimulation : public BweTest {
 public:
  BweSimulation() : BweTest() {}
  virtual ~BweSimulation() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(BweSimulation);
};

INSTANTIATE_TEST_CASE_P(VideoSendersTest, BweSimulation,
    ::testing::Values(MakeAdaptiveBweTestConfig(1),
                      MakeAdaptiveBweTestConfig(3)));

TEST_P(BweSimulation, SprintUplinkTest) {
  VerboseLogging(true);
  RateCounterFilter counter1(this, "sender_output");
  TraceBasedDeliveryFilter filter(this, "link_capacity");
  RateCounterFilter counter2(this, "receiver_input");
  ASSERT_TRUE(filter.Init(test::ResourcePath("sprint-uplink", "rx")));
  RunFor(60 * 1000);
}

TEST_P(BweSimulation, Verizon4gDownlinkTest) {
  VerboseLogging(true);
  RateCounterFilter counter1(this, "sender_output");
  TraceBasedDeliveryFilter filter(this, "link_capacity");
  RateCounterFilter counter2(this, "receiver_input");
  ASSERT_TRUE(filter.Init(test::ResourcePath("verizon4g-downlink", "rx")));
  RunFor(22 * 60 * 1000);
}
#endif  // BWE_TEST_LOGGING_COMPILE_TIME_ENABLE
}  // namespace bwe
}  // namespace testing
}  // namespace webrtc
