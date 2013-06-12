/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VIDEO_ENGINE_TEST_AUTO_TEST_AUTOMATED_VIE_LEGACY_FIXTURE_H_
#define WEBRTC_VIDEO_ENGINE_TEST_AUTO_TEST_AUTOMATED_VIE_LEGACY_FIXTURE_H_

#include "webrtc/video_engine/test/auto_test/automated/two_windows_fixture.h"

// Inherited by old-style standard integration tests based on ViEAutoTest.
class LegacyFixture : public TwoWindowsFixture {
 public:
  // Initializes ViEAutoTest in addition to the work done by ViEIntegrationTest.
  static void SetUpTestCase();

  // Releases anything allocated by SetupTestCase.
  static void TearDownTestCase();

 protected:
  static ViEAutoTest* tests_;
};

#endif  // WEBRTC_VIDEO_ENGINE_TEST_AUTO_TEST_AUTOMATED_VIE_LEGACY_FIXTURE_H_
