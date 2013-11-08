/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "testing/gtest/include/gtest/gtest.h"

#include "webrtc/test/testsupport/fileutils.h"
#include "webrtc/video_engine/test/common/flags.h"
#include "webrtc/video_engine/test/common/run_tests.h"

int main(int argc, char* argv[]) {
  ::testing::InitGoogleTest(&argc, argv);
  webrtc::test::flags::Init(&argc, &argv);
  webrtc::test::SetExecutablePath(argv[0]);

  return webrtc::test::RunAllTests();
}
