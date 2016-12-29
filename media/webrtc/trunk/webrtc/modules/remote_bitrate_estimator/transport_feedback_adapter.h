/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_REMOTE_BITRATE_ESTIMATOR_TRANSPORT_FEEDBACK_ADAPTER_H_
#define WEBRTC_MODULES_REMOTE_BITRATE_ESTIMATOR_TRANSPORT_FEEDBACK_ADAPTER_H_

#include <vector>

#include "webrtc/base/criticalsection.h"
#include "webrtc/base/thread_annotations.h"
#include "webrtc/modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "webrtc/modules/include/module_common_types.h"
#include "webrtc/modules/remote_bitrate_estimator/include/remote_bitrate_estimator.h"
#include "webrtc/modules/remote_bitrate_estimator/include/send_time_history.h"

namespace webrtc {

class ProcessThread;

class TransportFeedbackAdapter : public TransportFeedbackObserver,
                                 public CallStatsObserver,
                                 public RemoteBitrateObserver {
 public:
  TransportFeedbackAdapter(RtcpBandwidthObserver* bandwidth_observer,
                           Clock* clock,
                           ProcessThread* process_thread);
  virtual ~TransportFeedbackAdapter();

  void AddPacket(uint16_t sequence_number,
                 size_t length,
                 bool was_paced) override;

  void OnSentPacket(uint16_t sequence_number, int64_t send_time_ms);

  void OnTransportFeedback(const rtcp::TransportFeedback& feedback) override;

  void SetBitrateEstimator(RemoteBitrateEstimator* rbe);

  RemoteBitrateEstimator* GetBitrateEstimator() const {
    return bitrate_estimator_.get();
  }

 private:
  void OnReceiveBitrateChanged(const std::vector<unsigned int>& ssrcs,
                               unsigned int bitrate) override;
  void OnRttUpdate(int64_t avg_rtt_ms, int64_t max_rtt_ms) override;

  rtc::CriticalSection lock_;
  SendTimeHistory send_time_history_ GUARDED_BY(&lock_);
  rtc::scoped_ptr<RtcpBandwidthObserver> rtcp_bandwidth_observer_;
  rtc::scoped_ptr<RemoteBitrateEstimator> bitrate_estimator_;
  ProcessThread* const process_thread_;
  Clock* const clock_;
  int64_t current_offset_ms_;
  int64_t last_timestamp_us_;
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_REMOTE_BITRATE_ESTIMATOR_TRANSPORT_FEEDBACK_ADAPTER_H_
