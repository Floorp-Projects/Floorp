/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_REMOTE_BITRATE_ESTIMATOR_REMOTE_RATE_CONTROL_H_
#define WEBRTC_MODULES_REMOTE_BITRATE_ESTIMATOR_REMOTE_RATE_CONTROL_H_

#include "webrtc/modules/remote_bitrate_estimator/include/bwe_defines.h"
#include "webrtc/modules/remote_bitrate_estimator/include/remote_bitrate_estimator.h"

namespace webrtc {

class RemoteRateControl {
 public:
  static RemoteRateControl* Create(RateControlType control_type,
                                   uint32_t min_bitrate_bps);

  virtual ~RemoteRateControl() {}

  // Returns true if there is a valid estimate of the incoming bitrate, false
  // otherwise.
  virtual bool ValidEstimate() const = 0;
  virtual RateControlType GetControlType() const = 0;
  virtual uint32_t GetMinBitrate() const = 0;
  virtual int64_t GetFeedbackInterval() const = 0;

  // Returns true if the bitrate estimate hasn't been changed for more than
  // an RTT, or if the incoming_bitrate is more than 5% above the current
  // estimate. Should be used to decide if we should reduce the rate further
  // when over-using.
  virtual bool TimeToReduceFurther(int64_t time_now,
                                   uint32_t incoming_bitrate_bps) const = 0;
  virtual uint32_t LatestEstimate() const = 0;
  virtual uint32_t UpdateBandwidthEstimate(int64_t now_ms) = 0;
  virtual void SetRtt(int64_t rtt) = 0;
  virtual RateControlRegion Update(const RateControlInput* input,
                                   int64_t now_ms) = 0;
  virtual void SetEstimate(int bitrate_bps, int64_t time_now_ms) = 0;

 protected:
  static const int64_t kMaxFeedbackIntervalMs;
};
}  // namespace webrtc

#endif // WEBRTC_MODULES_REMOTE_BITRATE_ESTIMATOR_REMOTE_RATE_CONTROL_H_
