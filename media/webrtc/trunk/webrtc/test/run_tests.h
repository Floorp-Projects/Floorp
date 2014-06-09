/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef WEBRTC_VIDEO_ENGINE_TEST_COMMON_TEST_RUNNER_H_
#define WEBRTC_VIDEO_ENGINE_TEST_COMMON_TEST_RUNNER_H_

namespace webrtc {
namespace test {

// Blocks until the user presses enter.
void PressEnterToContinue();

// Performs platform-dependent initializations and calls gtest's
// RUN_ALL_TESTS().
int RunAllTests();
}  // namespace test
}  // namespace webrtc

#endif  // WEBRTC_VIDEO_ENGINE_TEST_COMMON_TEST_RUNNER_H_
