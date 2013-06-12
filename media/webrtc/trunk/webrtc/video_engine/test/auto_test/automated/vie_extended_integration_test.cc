/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "gtest/gtest.h"
#include "webrtc/test/testsupport/gtest_disable.h"
#include "webrtc/video_engine/test/auto_test/automated/legacy_fixture.h"
#include "webrtc/video_engine/test/auto_test/interface/vie_autotest.h"

namespace {

// TODO(phoglund): These tests are generally broken on mac.
// http://code.google.com/p/webrtc/issues/detail?id=1268
class DISABLED_ON_MAC(ViEExtendedIntegrationTest) : public LegacyFixture {
};

TEST_F(DISABLED_ON_MAC(ViEExtendedIntegrationTest), RunsBaseTestWithoutErrors) {
  tests_->ViEBaseExtendedTest();
}

// TODO(phoglund): Crashes on the v4l2loopback camera.
TEST_F(DISABLED_ON_MAC(ViEExtendedIntegrationTest),
       DISABLED_RunsCaptureTestWithoutErrors) {
  tests_->ViECaptureExtendedTest();
}

TEST_F(DISABLED_ON_MAC(ViEExtendedIntegrationTest),
       RunsCodecTestWithoutErrors) {
  tests_->ViECodecExtendedTest();
}

TEST_F(DISABLED_ON_MAC(ViEExtendedIntegrationTest),
       RunsEncryptionTestWithoutErrors) {
  tests_->ViEEncryptionExtendedTest();
}

TEST_F(DISABLED_ON_MAC(ViEExtendedIntegrationTest),
       RunsFileTestWithoutErrors) {
  tests_->ViEFileExtendedTest();
}

TEST_F(DISABLED_ON_MAC(ViEExtendedIntegrationTest),
       RunsImageProcessTestWithoutErrors) {
  tests_->ViEImageProcessExtendedTest();
}

TEST_F(DISABLED_ON_MAC(ViEExtendedIntegrationTest),
       RunsRenderTestWithoutErrors) {
  tests_->ViERenderExtendedTest();
}

TEST_F(DISABLED_ON_MAC(ViEExtendedIntegrationTest),
       RunsRtpRtcpTestWithoutErrors) {
  tests_->ViERtpRtcpExtendedTest();
}

}  // namespace
