/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_REMOTE_BITRATE_ESTIMATOR_TEST_ESTIMATORS_SEND_SIDE_H_
#define WEBRTC_MODULES_REMOTE_BITRATE_ESTIMATOR_TEST_ESTIMATORS_SEND_SIDE_H_

#include <vector>

#include "webrtc/modules/remote_bitrate_estimator/include/send_time_history.h"
#include "webrtc/modules/remote_bitrate_estimator/test/bwe.h"

namespace webrtc {
namespace testing {
namespace bwe {

class FullBweSender : public BweSender, public RemoteBitrateObserver {
 public:
  FullBweSender(int kbps, BitrateObserver* observer, Clock* clock);
  virtual ~FullBweSender();

  int GetFeedbackIntervalMs() const override;
  void GiveFeedback(const FeedbackPacket& feedback) override;
  void OnPacketsSent(const Packets& packets) override;
  void OnReceiveBitrateChanged(const std::vector<unsigned int>& ssrcs,
                               unsigned int bitrate) override;
  int64_t TimeUntilNextProcess() override;
  int Process() override;

 protected:
  rtc::scoped_ptr<BitrateController> bitrate_controller_;
  rtc::scoped_ptr<RemoteBitrateEstimator> rbe_;
  rtc::scoped_ptr<RtcpBandwidthObserver> feedback_observer_;

 private:
  Clock* const clock_;
  RTCPReportBlock report_block_;
  SendTimeHistory send_time_history_;
  bool has_received_ack_;
  uint16_t last_acked_seq_num_;

  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(FullBweSender);
};

class SendSideBweReceiver : public BweReceiver {
 public:
  explicit SendSideBweReceiver(int flow_id);
  virtual ~SendSideBweReceiver();

  void ReceivePacket(int64_t arrival_time_ms,
                     const MediaPacket& media_packet) override;
  FeedbackPacket* GetFeedback(int64_t now_ms) override;

 private:
  int64_t last_feedback_ms_;
  std::vector<PacketInfo> packet_feedback_vector_;
};

}  // namespace bwe
}  // namespace testing
}  // namespace webrtc

#endif  // WEBRTC_MODULES_REMOTE_BITRATE_ESTIMATOR_TEST_ESTIMATORS_SEND_SIDE_H_
