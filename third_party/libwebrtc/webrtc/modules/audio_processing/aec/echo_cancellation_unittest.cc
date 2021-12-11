/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// TODO(bjornv): Make this a comprehensive test.

#include "modules/audio_processing/aec/echo_cancellation.h"

#include <stdlib.h>
#include <time.h>

#include "modules/audio_processing/aec/aec_core.h"
#include "rtc_base/checks.h"
#include "test/gtest.h"

namespace webrtc {

TEST(EchoCancellationTest, CreateAndFreeHasExpectedBehavior) {
  void* handle = WebRtcAec_Create();
  ASSERT_TRUE(handle);
  WebRtcAec_Free(nullptr);
  WebRtcAec_Free(handle);
}

TEST(EchoCancellationTest, ApplyAecCoreHandle) {
  void* handle = WebRtcAec_Create();
  ASSERT_TRUE(handle);
  EXPECT_TRUE(WebRtcAec_aec_core(NULL) == NULL);
  AecCore* aec_core = WebRtcAec_aec_core(handle);
  EXPECT_TRUE(aec_core != NULL);
  // A simple test to verify that we can set and get a value from the lower
  // level |aec_core| handle.
  int delay = 111;
  WebRtcAec_SetSystemDelay(aec_core, delay);
  EXPECT_EQ(delay, WebRtcAec_system_delay(aec_core));
  WebRtcAec_Free(handle);
}

}  // namespace webrtc
