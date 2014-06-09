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

#include "webrtc/test/flags.h"
#include "webrtc/test/run_tests.h"
#include "webrtc/test/testsupport/fileutils.h"

int main(int argc, char* argv[]) {
  ::testing::InitGoogleTest(&argc, argv);
  webrtc::test::flags::Init(&argc, &argv);
  webrtc::test::SetExecutablePath(argv[0]);

  return webrtc::test::RunAllTests();
}
