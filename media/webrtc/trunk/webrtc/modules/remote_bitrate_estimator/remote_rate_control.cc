/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/remote_bitrate_estimator/remote_rate_control.h"

#include "webrtc/modules/remote_bitrate_estimator/aimd_rate_control.h"
#include "webrtc/modules/remote_bitrate_estimator/mimd_rate_control.h"

namespace webrtc {

// static
const int64_t RemoteRateControl::kMaxFeedbackIntervalMs = 1000;

RemoteRateControl* RemoteRateControl::Create(RateControlType control_type,
                                             uint32_t min_bitrate_bps) {
  if (control_type == kAimdControl) {
    return new AimdRateControl(min_bitrate_bps);
  } else {
    return new MimdRateControl(min_bitrate_bps);
  }
}

}  // namespace webrtc
