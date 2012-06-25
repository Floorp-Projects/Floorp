/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/test_suite.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace webrtc {
namespace test {
TestSuite::TestSuite(int argc, char** argv) {
  testing::InitGoogleMock(&argc, argv);  // Runs InitGoogleTest() internally.
}

TestSuite::~TestSuite() {
}

int TestSuite::Run() {
  Initialize();
  int result = RUN_ALL_TESTS();
  Shutdown();
  return result;
}

void TestSuite::Initialize() {
  // TODO(andrew): initialize singletons here (e.g. Trace).
}

void TestSuite::Shutdown() {
}
}  // namespace test
}  // namespace webrtc
