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

#include "webrtc/base/common.h"
#include "webrtc/base/scoped_ptr.h"
#include "webrtc/modules/interface/module_common_types.h"
#include "webrtc/modules/remote_bitrate_estimator/test/bwe_test_framework.h"
#include "webrtc/modules/remote_bitrate_estimator/test/packet_receiver.h"
#include "webrtc/modules/remote_bitrate_estimator/test/packet_sender.h"
#include "webrtc/system_wrappers/interface/clock.h"

using std::string;
using std::vector;

namespace webrtc {
namespace testing {
namespace bwe {

PacketProcessorRunner::PacketProcessorRunner(PacketProcessor* processor)
    : processor_(processor) {
}

PacketProcessorRunner::~PacketProcessorRunner() {
  for (Packet* packet : queue_)
    delete packet;
}

bool PacketProcessorRunner::RunsProcessor(
    const PacketProcessor* processor) const {
  return processor == processor_;
}

void PacketProcessorRunner::RunFor(int64_t time_ms,
                                   int64_t time_now_ms,
                                   Packets* in_out) {
  Packets to_process;
  FindPacketsToProcess(processor_->flow_ids(), in_out, &to_process);
  processor_->RunFor(time_ms, &to_process);
  QueuePackets(&to_process, time_now_ms * 1000);
  if (!to_process.empty()) {
    processor_->Plot((to_process.back()->send_time_us() + 500) / 1000);
  }
  in_out->merge(to_process, DereferencingComparator<Packet>);
}

void PacketProcessorRunner::FindPacketsToProcess(const FlowIds& flow_ids,
                                                 Packets* in,
                                                 Packets* out) {
  assert(out->empty());
  for (Packets::iterator it = in->begin(); it != in->end();) {
    // TODO(holmer): Further optimize this by looking for consecutive flow ids
    // in the packet list and only doing the binary search + splice once for a
    // sequence.
    if (flow_ids.find((*it)->flow_id()) != flow_ids.end()) {
      Packets::iterator next = it;
      ++next;
      out->splice(out->end(), *in, it);
      it = next;
    } else {
      ++it;
    }
  }
}

void PacketProcessorRunner::QueuePackets(Packets* batch,
                                         int64_t end_of_batch_time_us) {
  queue_.merge(*batch, DereferencingComparator<Packet>);
  if (queue_.empty()) {
    return;
  }
  Packets::iterator it = queue_.begin();
  for (; it != queue_.end(); ++it) {
    if ((*it)->send_time_us() > end_of_batch_time_us) {
      break;
    }
  }
  Packets to_transfer;
  to_transfer.splice(to_transfer.begin(), queue_, queue_.begin(), it);
  batch->merge(to_transfer, DereferencingComparator<Packet>);
}

BweTest::BweTest()
    : run_time_ms_(0), time_now_ms_(-1), simulation_interval_ms_(-1) {
  links_.push_back(&uplink_);
  links_.push_back(&downlink_);
}

BweTest::~BweTest() {
  for (Packet* packet : packets_)
    delete packet;
}

void BweTest::SetUp() {
  const ::testing::TestInfo* const test_info =
      ::testing::UnitTest::GetInstance()->current_test_info();
  string test_name =
      string(test_info->test_case_name()) + "_" + string(test_info->name());
  BWE_TEST_LOGGING_GLOBAL_CONTEXT(test_name);
  BWE_TEST_LOGGING_GLOBAL_ENABLE(false);
}

void Link::AddPacketProcessor(PacketProcessor* processor,
                              ProcessorType processor_type) {
  assert(processor);
  switch (processor_type) {
    case kSender:
      senders_.push_back(static_cast<PacketSender*>(processor));
      break;
    case kReceiver:
      receivers_.push_back(static_cast<PacketReceiver*>(processor));
      break;
    case kRegular:
      break;
  }
  processors_.push_back(PacketProcessorRunner(processor));
}

void Link::RemovePacketProcessor(PacketProcessor* processor) {
  for (vector<PacketProcessorRunner>::iterator it = processors_.begin();
       it != processors_.end(); ++it) {
    if (it->RunsProcessor(processor)) {
      processors_.erase(it);
      return;
    }
  }
  assert(false);
}

// Ownership of the created packets is handed over to the caller.
void Link::Run(int64_t run_for_ms, int64_t now_ms, Packets* packets) {
  for (auto& processor : processors_) {
    processor.RunFor(run_for_ms, now_ms, packets);
  }
}

void BweTest::VerboseLogging(bool enable) {
  BWE_TEST_LOGGING_GLOBAL_ENABLE(enable);
}

void BweTest::RunFor(int64_t time_ms) {
  // Set simulation interval from first packet sender.
  // TODO(holmer): Support different feedback intervals for different flows.
  if (!uplink_.senders().empty()) {
    simulation_interval_ms_ = uplink_.senders()[0]->GetFeedbackIntervalMs();
  } else if (!downlink_.senders().empty()) {
    simulation_interval_ms_ = downlink_.senders()[0]->GetFeedbackIntervalMs();
  }
  assert(simulation_interval_ms_ > 0);
  if (time_now_ms_ == -1) {
    time_now_ms_ = simulation_interval_ms_;
  }
  for (run_time_ms_ += time_ms;
       time_now_ms_ <= run_time_ms_ - simulation_interval_ms_;
       time_now_ms_ += simulation_interval_ms_) {
    // Packets are first generated on the first link, passed through all the
    // PacketProcessors and PacketReceivers. The PacketReceivers produces
    // FeedbackPackets which are then processed by the next link, where they
    // at some point will be consumed by a PacketSender.
    for (Link* link : links_)
      link->Run(simulation_interval_ms_, time_now_ms_, &packets_);
  }
}

string BweTest::GetTestName() const {
  const ::testing::TestInfo* const test_info =
      ::testing::UnitTest::GetInstance()->current_test_info();
  return string(test_info->name());
}
}  // namespace bwe
}  // namespace testing
}  // namespace webrtc
