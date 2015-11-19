/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/remote_bitrate_estimator/test/bwe.h"

#include <limits>

#include "webrtc/base/common.h"
#include "webrtc/modules/remote_bitrate_estimator/test/estimators/nada.h"
#include "webrtc/modules/remote_bitrate_estimator/test/estimators/remb.h"
#include "webrtc/modules/remote_bitrate_estimator/test/estimators/send_side.h"

namespace webrtc {
namespace testing {
namespace bwe {

class NullBweSender : public BweSender {
 public:
  NullBweSender() {}
  virtual ~NullBweSender() {}

  int GetFeedbackIntervalMs() const override { return 1000; }
  void GiveFeedback(const FeedbackPacket& feedback) override {}
  void OnPacketsSent(const Packets& packets) override {}
  int64_t TimeUntilNextProcess() override {
    return std::numeric_limits<int64_t>::max();
  }
  int Process() override { return 0; }

 private:
  DISALLOW_COPY_AND_ASSIGN(NullBweSender);
};

int64_t GetAbsSendTimeInMs(uint32_t abs_send_time) {
  const int kInterArrivalShift = 26;
  const int kAbsSendTimeInterArrivalUpshift = 8;
  const double kTimestampToMs =
      1000.0 / static_cast<double>(1 << kInterArrivalShift);
  uint32_t timestamp = abs_send_time << kAbsSendTimeInterArrivalUpshift;
  return static_cast<int64_t>(timestamp) * kTimestampToMs;
}

BweSender* CreateBweSender(BandwidthEstimatorType estimator,
                           int kbps,
                           BitrateObserver* observer,
                           Clock* clock) {
  switch (estimator) {
    case kRembEstimator:
      return new RembBweSender(kbps, observer, clock);
    case kFullSendSideEstimator:
      return new FullBweSender(kbps, observer, clock);
    case kNadaEstimator:
      return new NadaBweSender(kbps, observer, clock);
    case kNullEstimator:
      return new NullBweSender();
  }
  assert(false);
  return NULL;
}

BweReceiver* CreateBweReceiver(BandwidthEstimatorType type,
                               int flow_id,
                               bool plot) {
  switch (type) {
    case kRembEstimator:
      return new RembReceiver(flow_id, plot);
    case kFullSendSideEstimator:
      return new SendSideBweReceiver(flow_id);
    case kNadaEstimator:
      return new NadaBweReceiver(flow_id);
    case kNullEstimator:
      return new BweReceiver(flow_id);
  }
  assert(false);
  return NULL;
}
}  // namespace bwe
}  // namespace testing
}  // namespace webrtc
