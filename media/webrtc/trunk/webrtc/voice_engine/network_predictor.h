/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VOICE_ENGINE_NETWORK_PREDICTOR_H_
#define WEBRTC_VOICE_ENGINE_NETWORK_PREDICTOR_H_

#include "webrtc/base/exp_filter.h"
#include "webrtc/system_wrappers/interface/clock.h"

namespace webrtc {

namespace voe {

// NetworkPredictor is to predict network conditions e.g., packet loss rate, for
// sender and/or receiver to cope with changes in the network condition.
class NetworkPredictor {
 public:
  explicit NetworkPredictor(Clock* clock);
  ~NetworkPredictor() {}

  // Gets the predicted packet loss rate.
  uint8_t GetLossRate();

  // Updates the packet loss rate predictor, on receiving a new observation of
  // packet loss rate from past. Input packet loss rate should be in the
  // interval [0, 255].
  void UpdatePacketLossRate(uint8_t loss_rate);

 private:
  Clock* clock_;
  int64_t last_loss_rate_update_time_ms_;

  // An exponential filter is used to predict packet loss rate.
  scoped_ptr<rtc::ExpFilter> loss_rate_filter_;
};

}  // namespace voe
}  // namespace webrtc
#endif  // WEBRTC_VOICE_ENGINE_NETWORK_PREDICTOR_H_
