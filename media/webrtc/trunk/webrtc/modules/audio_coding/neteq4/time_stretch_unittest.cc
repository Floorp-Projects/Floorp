/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// Unit tests for Accelerate and PreemptiveExpand classes.

#include "webrtc/modules/audio_coding/neteq4/accelerate.h"
#include "webrtc/modules/audio_coding/neteq4/preemptive_expand.h"

#include "gtest/gtest.h"
#include "webrtc/modules/audio_coding/neteq4/background_noise.h"

namespace webrtc {

TEST(TimeStretch, CreateAndDestroy) {
  int sample_rate = 8000;
  size_t num_channels = 1;
  BackgroundNoise bgn(num_channels);
  Accelerate accelerate(sample_rate, num_channels, bgn);
  PreemptiveExpand preemptive_expand(sample_rate, num_channels, bgn);
}

// TODO(hlundin): Write more tests.

}  // namespace webrtc
