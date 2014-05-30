/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/remote_bitrate_estimator/test/bwe_test.h"

#include "webrtc/modules/remote_bitrate_estimator/test/bwe_test_baselinefile.h"
#include "webrtc/modules/remote_bitrate_estimator/test/bwe_test_framework.h"
#include "webrtc/modules/remote_bitrate_estimator/include/remote_bitrate_estimator.h"
#include "webrtc/system_wrappers/interface/clock.h"
#include "webrtc/system_wrappers/interface/scoped_ptr.h"

using std::string;
using std::vector;

namespace webrtc {
namespace testing {
namespace bwe {

namespace stl_helpers {
template<typename T> void DeleteElements(T* container) {
  if (!container) return;
  for (typename T::iterator it = container->begin(); it != container->end();
      ++it) {
    delete *it;
  }
  container->clear();
}
}  // namespace stl_helpers

class BweTest::TestedEstimator : public RemoteBitrateObserver {
 public:
  static const uint32_t kRemoteBitrateEstimatorMinBitrateBps = 30000;

  TestedEstimator(const string& test_name,
                  const BweTestConfig::EstimatorConfig& config)
      : debug_name_(config.debug_name),
        clock_(0),
        stats_(),
        relative_estimator_stats_(),
        latest_estimate_bps_(-1),
        estimator_(config.estimator_factory->Create(
            this, &clock_, kRemoteBitrateEstimatorMinBitrateBps)),
        relative_estimator_(NULL),
        baseline_(BaseLineFileInterface::Create(test_name + "_" + debug_name_,
                                                config.update_baseline)) {
    assert(estimator_.get());
    assert(baseline_.get());
    // Default RTT in RemoteRateControl is 200 ms ; 50 ms is more realistic.
    estimator_->OnRttUpdate(50);
  }

  void SetRelativeEstimator(TestedEstimator* relative_estimator) {
    relative_estimator_ = relative_estimator;
  }

  void EatPacket(const Packet& packet) {
    BWE_TEST_LOGGING_CONTEXT(debug_name_);

    latest_estimate_bps_ = -1;

    // We're treating the send time (from previous filter) as the arrival
    // time once packet reaches the estimator.
    int64_t packet_time_ms = (packet.send_time_us() + 500) / 1000;
    BWE_TEST_LOGGING_TIME(packet_time_ms);
    BWE_TEST_LOGGING_PLOT("Delay_#2", clock_.TimeInMilliseconds(),
                          packet_time_ms -
                          (packet.creation_time_us() + 500) / 1000);

    int64_t step_ms = estimator_->TimeUntilNextProcess();
    while ((clock_.TimeInMilliseconds() + step_ms) < packet_time_ms) {
      clock_.AdvanceTimeMilliseconds(step_ms);
      estimator_->Process();
      step_ms = estimator_->TimeUntilNextProcess();
    }
    estimator_->IncomingPacket(packet_time_ms, packet.payload_size(),
                               packet.header());
    clock_.AdvanceTimeMilliseconds(packet_time_ms -
                                   clock_.TimeInMilliseconds());
    ASSERT_TRUE(packet_time_ms == clock_.TimeInMilliseconds());
  }

  bool CheckEstimate(PacketSender::Feedback* feedback) {
    assert(feedback);
    BWE_TEST_LOGGING_CONTEXT(debug_name_);
    uint32_t estimated_bps = 0;
    if (LatestEstimate(&estimated_bps)) {
      feedback->estimated_bps = estimated_bps;
      baseline_->Estimate(clock_.TimeInMilliseconds(), estimated_bps);

      double estimated_kbps = static_cast<double>(estimated_bps) / 1000.0;
      stats_.Push(estimated_kbps);
      BWE_TEST_LOGGING_PLOT("Estimate_#1", clock_.TimeInMilliseconds(),
                            estimated_kbps);
      uint32_t relative_estimate_bps = 0;
      if (relative_estimator_ &&
          relative_estimator_->LatestEstimate(&relative_estimate_bps)) {
        double relative_estimate_kbps =
            static_cast<double>(relative_estimate_bps) / 1000.0;
        relative_estimator_stats_.Push(estimated_kbps - relative_estimate_kbps);
      }
      return true;
    }
    return false;
  }

  void LogStats() {
    BWE_TEST_LOGGING_CONTEXT(debug_name_);
    BWE_TEST_LOGGING_CONTEXT("Mean");
    stats_.Log("kbps");
    if (relative_estimator_) {
      BWE_TEST_LOGGING_CONTEXT("Diff");
      relative_estimator_stats_.Log("kbps");
    }
  }

  void VerifyOrWriteBaseline() {
    EXPECT_TRUE(baseline_->VerifyOrWrite());
  }

  virtual void OnReceiveBitrateChanged(const vector<unsigned int>& ssrcs,
                                       unsigned int bitrate) {
  }

 private:
  bool LatestEstimate(uint32_t* estimate_bps) {
    if (latest_estimate_bps_ < 0) {
      vector<unsigned int> ssrcs;
      unsigned int bps = 0;
      if (!estimator_->LatestEstimate(&ssrcs, &bps)) {
        return false;
      }
      latest_estimate_bps_ = bps;
    }
    *estimate_bps = latest_estimate_bps_;
    return true;
  }

  string debug_name_;
  SimulatedClock clock_;
  Stats<double> stats_;
  Stats<double> relative_estimator_stats_;
  int64_t latest_estimate_bps_;
  scoped_ptr<RemoteBitrateEstimator> estimator_;
  TestedEstimator* relative_estimator_;
  scoped_ptr<BaseLineFileInterface> baseline_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(TestedEstimator);
};

BweTest::BweTest()
    : run_time_ms_(0),
      simulation_interval_ms_(-1),
      previous_packets_(),
      packet_senders_(),
      estimators_(),
      processors_() {
}

BweTest::~BweTest() {
  stl_helpers::DeleteElements(&estimators_);
  stl_helpers::DeleteElements(&packet_senders_);
}

void BweTest::SetUp() {
  const ::testing::TestInfo* const test_info =
      ::testing::UnitTest::GetInstance()->current_test_info();
  string test_name =
      string(test_info->test_case_name()) + "_" + string(test_info->name());
  BWE_TEST_LOGGING_GLOBAL_CONTEXT(test_name);

  const BweTestConfig& config = GetParam();

  uint32_t total_capacity = 0;
  for (vector<const PacketSenderFactory*>::const_iterator it =
      config.sender_factories.begin(); it != config.sender_factories.end();
      ++it) {
    PacketSender* sender = (*it)->Create();
    assert(sender);
    total_capacity += sender->GetCapacityKbps();
    packet_senders_.push_back(sender);
    processors_.push_back(sender);
  }
  BWE_TEST_LOGGING_LOG1("RequiredLinkCapacity", "%d kbps", total_capacity)

  // Set simulation interval from first packet sender.
  if (packet_senders_.size() > 0) {
    simulation_interval_ms_ = packet_senders_[0]->GetFeedbackIntervalMs();
  }

  for (vector<BweTestConfig::EstimatorConfig>:: const_iterator it =
      config.estimator_configs.begin(); it != config.estimator_configs.end();
      ++it) {
    estimators_.push_back(new TestedEstimator(test_name, *it));
  }
  if (estimators_.size() > 1) {
    // Set all estimators as relative to the first one.
    for (uint32_t i = 1; i < estimators_.size(); ++i) {
      estimators_[i]->SetRelativeEstimator(estimators_[0]);
    }
  }

  BWE_TEST_LOGGING_GLOBAL_ENABLE(false);
}

void BweTest::TearDown() {
  BWE_TEST_LOGGING_GLOBAL_ENABLE(true);

  for (vector<TestedEstimator*>::iterator eit = estimators_.begin();
      eit != estimators_.end(); ++eit) {
    (*eit)->VerifyOrWriteBaseline();
    (*eit)->LogStats();
  }

  BWE_TEST_LOGGING_GLOBAL_CONTEXT("");
}

void BweTest::AddPacketProcessor(
    PacketProcessor* processor) {
  assert(processor);
  processors_.push_back(processor);
}

void BweTest::RemovePacketProcessor(
    PacketProcessor* processor) {
  vector<PacketProcessor*>::iterator it =
      std::find(processors_.begin(), processors_.end(), processor);
  assert(it != processors_.end());
  processors_.erase(it);
}

void BweTest::VerboseLogging(bool enable) {
  BWE_TEST_LOGGING_GLOBAL_ENABLE(enable);
}

void BweTest::RunFor(int64_t time_ms) {
  for (run_time_ms_ += time_ms; run_time_ms_ >= simulation_interval_ms_;
      run_time_ms_ -= simulation_interval_ms_) {
    Packets packets;
    for (vector<PacketProcessor*>::const_iterator it =
         processors_.begin(); it != processors_.end(); ++it) {
      (*it)->RunFor(simulation_interval_ms_, &packets);
      (*it)->Plot((packets.back().send_time_us() + 500) / 1000);
    }

    // Verify packets are in order between batches.
    if (!packets.empty() && !previous_packets_.empty()) {
      packets.splice(packets.begin(), previous_packets_,
                     --previous_packets_.end());
      ASSERT_TRUE(IsTimeSorted(packets));
      packets.erase(packets.begin());
    } else {
      ASSERT_TRUE(IsTimeSorted(packets));
    }

    for (PacketsConstIt pit = packets.begin(); pit != packets.end(); ++pit) {
      for (vector<TestedEstimator*>::iterator eit = estimators_.begin();
          eit != estimators_.end(); ++eit) {
        (*eit)->EatPacket(*pit);
      }
    }

    previous_packets_.swap(packets);

    for (vector<TestedEstimator*>::iterator eit = estimators_.begin();
        eit != estimators_.end(); ++eit) {
      PacketSender::Feedback feedback = {0};
      if ((*eit)->CheckEstimate(&feedback)) {
        for (vector<PacketSender*>::iterator psit = packet_senders_.begin();
            psit != packet_senders_.end(); ++psit) {
          (*psit)->GiveFeedback(feedback);
        }
      }
    }
  }
}
}  // namespace bwe
}  // namespace testing
}  // namespace webrtc
