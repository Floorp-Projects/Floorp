/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/system_wrappers/source/unittest_utilities.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "webrtc/system_wrappers/interface/trace.h"

namespace webrtc {

// These tests merely check that the code compiles and that no
// fatal accidents happen when logging.
TEST(UnittestUtilities, TraceOn) {
  ScopedTracing trace(true);
  WEBRTC_TRACE(kTraceInfo, kTraceUtility, 0, "Log line that should appear");
  // TODO(hta): Verify that output appears.
  // Note - output is written on another thread, so can take time to appear.
}

TEST(UnittestUtilities, TraceOff) {
  ScopedTracing trace(false);
  WEBRTC_TRACE(kTraceInfo, kTraceUtility, 0,
               "Log line that should not appear");
  // TODO(hta): Verify that no output appears.
}

}  // namespace webrtc
