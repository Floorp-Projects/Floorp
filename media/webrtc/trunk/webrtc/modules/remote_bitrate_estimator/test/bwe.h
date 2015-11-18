/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_REMOTE_BITRATE_ESTIMATOR_TEST_BWE_H_
#define WEBRTC_MODULES_REMOTE_BITRATE_ESTIMATOR_TEST_BWE_H_

#include <sstream>

#include "webrtc/modules/remote_bitrate_estimator/test/packet.h"
#include "webrtc/modules/bitrate_controller/include/bitrate_controller.h"

namespace webrtc {
namespace testing {
namespace bwe {

const int kMinBitrateKbps = 150;
const int kMaxBitrateKbps = 2000;

class BweSender : public Module {
 public:
  BweSender() {}
  virtual ~BweSender() {}

  virtual int GetFeedbackIntervalMs() const = 0;
  virtual void GiveFeedback(const FeedbackPacket& feedback) = 0;
  virtual void OnPacketsSent(const Packets& packets) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(BweSender);
};

class BweReceiver {
 public:
  explicit BweReceiver(int flow_id) : flow_id_(flow_id) {}
  virtual ~BweReceiver() {}

  virtual void ReceivePacket(int64_t arrival_time_ms,
                             const MediaPacket& media_packet) {}
  virtual FeedbackPacket* GetFeedback(int64_t now_ms) { return NULL; }

 protected:
  int flow_id_;
};

enum BandwidthEstimatorType {
  kNullEstimator,
  kNadaEstimator,
  kRembEstimator,
  kFullSendSideEstimator
};

int64_t GetAbsSendTimeInMs(uint32_t abs_send_time);

BweSender* CreateBweSender(BandwidthEstimatorType estimator,
                           int kbps,
                           BitrateObserver* observer,
                           Clock* clock);

BweReceiver* CreateBweReceiver(BandwidthEstimatorType type,
                               int flow_id,
                               bool plot);
}  // namespace bwe
}  // namespace testing
}  // namespace webrtc
#endif  // WEBRTC_MODULES_REMOTE_BITRATE_ESTIMATOR_TEST_BWE_H_
