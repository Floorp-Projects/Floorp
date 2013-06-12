/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/video_engine/test/auto_test/automated/legacy_fixture.h"

#include "webrtc/video_engine/test/auto_test/interface/vie_autotest.h"

void LegacyFixture::SetUpTestCase() {
  TwoWindowsFixture::SetUpTestCase();

  // Create the test cases
  tests_ = new ViEAutoTest(window_1_, window_2_);
}

void LegacyFixture::TearDownTestCase() {
  delete tests_;

  TwoWindowsFixture::TearDownTestCase();
}

ViEAutoTest* LegacyFixture::tests_ = NULL;
