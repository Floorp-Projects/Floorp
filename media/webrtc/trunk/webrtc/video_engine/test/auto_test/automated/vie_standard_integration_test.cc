/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// This file contains the "standard" suite of integration tests, implemented
// as a GUnit test. This file is a part of the effort to try to automate all
// tests in this section of the code. Currently, this code makes no attempt
// to verify any video output - it only checks for direct errors.

#include <stdio.h>

#include "gflags/gflags.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webrtc/test/testsupport/metrics/video_metrics.h"
#include "webrtc/test/testsupport/metrics/video_metrics.h"
#include "webrtc/video_engine/test/auto_test/automated/legacy_fixture.h"
#include "webrtc/video_engine/test/auto_test/interface/vie_autotest.h"
#include "webrtc/video_engine/test/auto_test/interface/vie_autotest_window_manager_interface.h"
#include "webrtc/video_engine/test/auto_test/interface/vie_window_creator.h"
#include "webrtc/video_engine/test/libvietest/include/vie_to_file_renderer.h"

namespace {

class ViEStandardIntegrationTest : public LegacyFixture {
};

TEST_F(ViEStandardIntegrationTest, RunsBaseTestWithoutErrors)  {
  tests_->ViEBaseStandardTest();
}

// Flaky: https://code.google.com/p/webrtc/issues/detail?id=1734
TEST_F(ViEStandardIntegrationTest, DISABLED_RunsCodecTestWithoutErrors)  {
  tests_->ViECodecStandardTest();
}

TEST_F(ViEStandardIntegrationTest, RunsCaptureTestWithoutErrors)  {
  tests_->ViECaptureStandardTest();
}

TEST_F(ViEStandardIntegrationTest, RunsEncryptionTestWithoutErrors)  {
  tests_->ViEEncryptionStandardTest();
}

TEST_F(ViEStandardIntegrationTest, RunsImageProcessTestWithoutErrors)  {
  tests_->ViEImageProcessStandardTest();
}

TEST_F(ViEStandardIntegrationTest, RunsRenderTestWithoutErrors)  {
  tests_->ViERenderStandardTest();
}

// Fails on Mac, see https://code.google.com/p/webrtc/issues/detail?id=1790
#if defined(WEBRTC_MAC)
#define MAYBE_RunsRtpRtcpTestWithoutErrors DISABLED_RunsRtpRtcpTestWithoutErrors
#else
#define MAYBE_RunsRtpRtcpTestWithoutErrors RunsRtpRtcpTestWithoutErrors
#endif
TEST_F(ViEStandardIntegrationTest, MAYBE_RunsRtpRtcpTestWithoutErrors)  {
  tests_->ViERtpRtcpStandardTest();
}

}  // namespace
