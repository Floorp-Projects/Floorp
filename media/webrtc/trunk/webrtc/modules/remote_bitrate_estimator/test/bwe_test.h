/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <map>
#include <string>
#include <vector>
#include "testing/gtest/include/gtest/gtest.h"
#include "webrtc/base/constructormagic.h"
#include "webrtc/modules/remote_bitrate_estimator/include/remote_bitrate_estimator.h"
#include "webrtc/modules/remote_bitrate_estimator/test/bwe_test_framework.h"

namespace webrtc {

struct RemoteBitrateEstimatorFactory;

namespace testing {
namespace bwe {

struct BweTestConfig {
  struct EstimatorConfig {
    EstimatorConfig()
        : debug_name(),
          flow_id(0),
          estimator_factory(NULL),
          control_type(kMimdControl),
          update_baseline(false),
          plot_delay(true),
          plot_estimate(true) {
    }
    EstimatorConfig(std::string debug_name,
                    int flow_id,
                    const RemoteBitrateEstimatorFactory* estimator_factory,
                    bool plot_delay,
                    bool plot_estimate)
        : debug_name(debug_name),
          flow_id(flow_id),
          estimator_factory(estimator_factory),
          control_type(kMimdControl),
          update_baseline(false),
          plot_delay(plot_delay),
          plot_estimate(plot_estimate) {
    }
    EstimatorConfig(std::string debug_name,
                    int flow_id,
                    const RemoteBitrateEstimatorFactory* estimator_factory,
                    RateControlType control_type,
                    bool plot_delay,
                    bool plot_estimate)
        : debug_name(debug_name),
          flow_id(flow_id),
          estimator_factory(estimator_factory),
          control_type(control_type),
          update_baseline(false),
          plot_delay(plot_delay),
          plot_estimate(plot_estimate) {
    }
    EstimatorConfig(std::string debug_name,
                    int flow_id,
                    const RemoteBitrateEstimatorFactory* estimator_factory,
                    RateControlType control_type,
                    bool update_baseline)
        : debug_name(debug_name),
          flow_id(flow_id),
          estimator_factory(estimator_factory),
          control_type(control_type),
          update_baseline(update_baseline),
          plot_delay(false),
          plot_estimate(false) {
    }
    std::string debug_name;
    int flow_id;
    const RemoteBitrateEstimatorFactory* estimator_factory;
    RateControlType control_type;
    bool update_baseline;
    bool plot_delay;
    bool plot_estimate;
  };

  std::vector<EstimatorConfig> estimator_configs;
};

class TestedEstimator;
class PacketProcessorRunner;

class BweTest : public PacketProcessorListener {
 public:
  BweTest();
  virtual ~BweTest();

  virtual void AddPacketProcessor(PacketProcessor* processor, bool is_sender);
  virtual void RemovePacketProcessor(PacketProcessor* processor);

 protected:
  void SetupTestFromConfig(const BweTestConfig& config);
  void VerboseLogging(bool enable);
  void RunFor(int64_t time_ms);
  std::string GetTestName() const;

 private:
  typedef std::map<int, TestedEstimator*> EstimatorMap;

  void FindPacketsToProcess(const FlowIds& flow_ids, Packets* in,
                            Packets* out);
  void GiveFeedbackToAffectedSenders(int flow_id, TestedEstimator* estimator);

  int64_t run_time_ms_;
  int64_t time_now_ms_;
  int64_t simulation_interval_ms_;
  EstimatorMap estimators_;
  Packets previous_packets_;
  std::vector<PacketSender*> senders_;
  std::vector<PacketProcessorRunner> processors_;

  DISALLOW_COPY_AND_ASSIGN(BweTest);
};
}  // namespace bwe
}  // namespace testing
}  // namespace webrtc
