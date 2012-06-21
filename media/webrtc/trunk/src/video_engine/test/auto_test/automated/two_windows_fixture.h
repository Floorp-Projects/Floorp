/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef SRC_VIDEO_ENGINE_MAIN_TEST_AUTOTEST_AUTOMATED_TWO_WINDOWS_FIXTURE_H_
#define SRC_VIDEO_ENGINE_MAIN_TEST_AUTOTEST_AUTOMATED_TWO_WINDOWS_FIXTURE_H_

#include "gtest/gtest.h"

class ViEWindowCreator;
class ViEAutoTest;

// Meant to be inherited by all standard test who require two windows.
class TwoWindowsFixture : public testing::Test {
 public:
  // Launches two windows in a platform-dependent manner and stores the handles
  // in the window_1_ and window_2_ fields.
  static void SetUpTestCase();

  // Releases anything allocated by SetupTestCase.
  static void TearDownTestCase();

 protected:
  static void* window_1_;
  static void* window_2_;
  static ViEWindowCreator* window_creator_;
};

#endif  // SRC_VIDEO_ENGINE_MAIN_TEST_AUTOTEST_AUTOMATED_TWO_WINDOWS_FIXTURE_H_
