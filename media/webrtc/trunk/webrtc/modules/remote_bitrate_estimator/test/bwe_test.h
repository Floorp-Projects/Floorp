/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <string>
#include <vector>
#include "gtest/gtest.h"
#include "webrtc/modules/remote_bitrate_estimator/test/bwe_test_framework.h"
#include "webrtc/system_wrappers/interface/constructor_magic.h"

namespace webrtc {

struct RemoteBitrateEstimatorFactory;

namespace testing {
namespace bwe {

struct BweTestConfig {
  struct EstimatorConfig {
    EstimatorConfig()
        : debug_name(),
          estimator_factory(NULL),
          update_baseline(false) {
    }
    EstimatorConfig(std::string debug_name,
                    const RemoteBitrateEstimatorFactory* estimator_factory)
        : debug_name(debug_name),
          estimator_factory(estimator_factory),
          update_baseline(false) {
    }
    EstimatorConfig(std::string debug_name,
                    const RemoteBitrateEstimatorFactory* estimator_factory,
                    bool update_baseline)
        : debug_name(debug_name),
          estimator_factory(estimator_factory),
          update_baseline(update_baseline) {
    }
    std::string debug_name;
    const RemoteBitrateEstimatorFactory* estimator_factory;
    bool update_baseline;
  };

  std::vector<const PacketSenderFactory*> sender_factories;
  std::vector<EstimatorConfig> estimator_configs;
};

class BweTest : public ::testing::TestWithParam<BweTestConfig>,
    public PacketProcessorListener {
 public:
  BweTest();
  virtual ~BweTest();

  virtual void SetUp();
  virtual void TearDown();
  virtual void AddPacketProcessor(PacketProcessor* processor);
  virtual void RemovePacketProcessor(PacketProcessor* processor);

 protected:
  void VerboseLogging(bool enable);
  void RunFor(int64_t time_ms);

 private:
  class TestedEstimator;

  int64_t run_time_ms_;
  int64_t simulation_interval_ms_;
  Packets previous_packets_;
  std::vector<PacketSender*> packet_senders_;
  std::vector<TestedEstimator*> estimators_;
  std::vector<PacketProcessor*> processors_;

  DISALLOW_COPY_AND_ASSIGN(BweTest);
};
}  // namespace bwe
}  // namespace testing
}  // namespace webrtc
