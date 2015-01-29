/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_EXPERIMENTS_H_
#define WEBRTC_EXPERIMENTS_H_

#include "webrtc/typedefs.h"

namespace webrtc {
struct RemoteBitrateEstimatorMinRate {
  RemoteBitrateEstimatorMinRate() : min_rate(30000) {}
  RemoteBitrateEstimatorMinRate(uint32_t min_rate) : min_rate(min_rate) {}

  uint32_t min_rate;
};

struct AimdRemoteRateControl {
  AimdRemoteRateControl() : enabled(false) {}
  explicit AimdRemoteRateControl(bool set_enabled)
    : enabled(set_enabled) {}
  virtual ~AimdRemoteRateControl() {}

  const bool enabled;
};
}  // namespace webrtc
#endif  // WEBRTC_EXPERIMENTS_H_
