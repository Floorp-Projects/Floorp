/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_REMOTE_BITRATE_ESTIMATOR_TEST_ESTIMATORS_NADA_H_
#define WEBRTC_MODULES_REMOTE_BITRATE_ESTIMATOR_TEST_ESTIMATORS_NADA_H_

#include "webrtc/modules/remote_bitrate_estimator/test/bwe.h"

namespace webrtc {

class ReceiveStatistics;

namespace testing {
namespace bwe {

class NadaBweReceiver : public BweReceiver {
 public:
  explicit NadaBweReceiver(int flow_id);
  virtual ~NadaBweReceiver();

  void ReceivePacket(int64_t arrival_time_ms,
                     const MediaPacket& media_packet) override;
  FeedbackPacket* GetFeedback(int64_t now_ms) override;

 private:
  SimulatedClock clock_;
  int64_t last_feedback_ms_;
  rtc::scoped_ptr<ReceiveStatistics> recv_stats_;
  int64_t baseline_delay_ms_;
  int64_t delay_signal_ms_;
  int64_t last_congestion_signal_ms_;
};

class NadaBweSender : public BweSender {
 public:
  NadaBweSender(int kbps, BitrateObserver* observer, Clock* clock);
  virtual ~NadaBweSender();

  int GetFeedbackIntervalMs() const override;
  void GiveFeedback(const FeedbackPacket& feedback) override;
  void OnPacketsSent(const Packets& packets) override {}
  int64_t TimeUntilNextProcess() override;
  int Process() override;

 private:
  Clock* const clock_;
  BitrateObserver* const observer_;
  int bitrate_kbps_;
  int64_t last_feedback_ms_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(NadaBweSender);
};

}  // namespace bwe
}  // namespace testing
}  // namespace webrtc

#endif  // WEBRTC_MODULES_REMOTE_BITRATE_ESTIMATOR_TEST_ESTIMATORS_NADA_H_
