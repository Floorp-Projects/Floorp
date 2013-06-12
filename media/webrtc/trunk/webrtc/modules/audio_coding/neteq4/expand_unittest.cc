/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// Unit tests for Expand class.

#include "webrtc/modules/audio_coding/neteq4/expand.h"

#include "gtest/gtest.h"
#include "webrtc/modules/audio_coding/neteq4/background_noise.h"
#include "webrtc/modules/audio_coding/neteq4/random_vector.h"
#include "webrtc/modules/audio_coding/neteq4/sync_buffer.h"

namespace webrtc {

TEST(Expand, CreateAndDestroy) {
  int fs = 8000;
  size_t channels = 1;
  BackgroundNoise bgn(channels);
  SyncBuffer sync_buffer(1, 1000);
  RandomVector random_vector;
  Expand expand(&bgn, &sync_buffer, &random_vector, fs, channels);
}

// TODO(hlundin): Write more tests.

}  // namespace webrtc
