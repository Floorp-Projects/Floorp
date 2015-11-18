/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/remote_bitrate_estimator/test/packet_receiver.h"

#include <vector>

#include "testing/gtest/include/gtest/gtest.h"
#include "webrtc/base/common.h"
#include "webrtc/modules/interface/module_common_types.h"
#include "webrtc/modules/remote_bitrate_estimator/test/bwe.h"
#include "webrtc/modules/remote_bitrate_estimator/test/bwe_test_framework.h"
#include "webrtc/modules/rtp_rtcp/interface/receive_statistics.h"
#include "webrtc/system_wrappers/interface/clock.h"

namespace webrtc {
namespace testing {
namespace bwe {

PacketReceiver::PacketReceiver(PacketProcessorListener* listener,
                               int flow_id,
                               BandwidthEstimatorType bwe_type,
                               bool plot_delay,
                               bool plot_bwe)
    : PacketProcessor(listener, flow_id, kReceiver),
      delay_log_prefix_(),
      last_delay_plot_ms_(0),
      plot_delay_(plot_delay),
      bwe_receiver_(CreateBweReceiver(bwe_type, flow_id, plot_bwe)) {
  // Setup the prefix ststd::rings used when logging.
  std::stringstream ss;
  ss << "Delay_" << flow_id << "#2";
  delay_log_prefix_ = ss.str();
}

PacketReceiver::~PacketReceiver() {
}

void PacketReceiver::RunFor(int64_t time_ms, Packets* in_out) {
  Packets feedback;
  for (auto it = in_out->begin(); it != in_out->end();) {
    // PacketReceivers are only associated with a single stream, and therefore
    // should only process a single flow id.
    // TODO(holmer): Break this out into a Demuxer which implements both
    // PacketProcessorListener and PacketProcessor.
    if ((*it)->GetPacketType() == Packet::kMedia &&
        (*it)->flow_id() == *flow_ids().begin()) {
      BWE_TEST_LOGGING_CONTEXT("Receiver");
      const MediaPacket* media_packet = static_cast<const MediaPacket*>(*it);
      // We're treating the send time (from previous filter) as the arrival
      // time once packet reaches the estimator.
      int64_t arrival_time_ms = (media_packet->send_time_us() + 500) / 1000;
      BWE_TEST_LOGGING_TIME(arrival_time_ms);
      PlotDelay(arrival_time_ms,
                (media_packet->creation_time_us() + 500) / 1000);

      bwe_receiver_->ReceivePacket(arrival_time_ms, *media_packet);
      FeedbackPacket* fb = bwe_receiver_->GetFeedback(arrival_time_ms);
      if (fb)
        feedback.push_back(fb);
      delete media_packet;
      it = in_out->erase(it);
    } else {
      ++it;
    }
  }
  // Insert feedback packets to be sent back to the sender.
  in_out->merge(feedback, DereferencingComparator<Packet>);
}

void PacketReceiver::PlotDelay(int64_t arrival_time_ms, int64_t send_time_ms) {
  static const int kDelayPlotIntervalMs = 100;
  if (!plot_delay_)
    return;
  if (arrival_time_ms - last_delay_plot_ms_ > kDelayPlotIntervalMs) {
    BWE_TEST_LOGGING_PLOT(delay_log_prefix_, arrival_time_ms,
                          arrival_time_ms - send_time_ms);
    last_delay_plot_ms_ = arrival_time_ms;
  }
}
}  // namespace bwe
}  // namespace testing
}  // namespace webrtc
