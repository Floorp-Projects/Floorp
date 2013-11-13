/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "testing/gtest/include/gtest/gtest.h"
#include "webrtc/test/testsupport/gtest_disable.h"
#include "webrtc/video_engine/test/auto_test/automated/legacy_fixture.h"
#include "webrtc/video_engine/test/auto_test/interface/vie_autotest.h"

namespace {

// TODO(phoglund): These tests are generally broken on mac.
// http://code.google.com/p/webrtc/issues/detail?id=1268
class DISABLED_ON_MAC(ViEApiIntegrationTest) : public LegacyFixture {
};

TEST_F(DISABLED_ON_MAC(ViEApiIntegrationTest), RunsBaseTestWithoutErrors) {
  tests_->ViEBaseAPITest();
}

// TODO(phoglund): Crashes on the v4l2loopback camera.
TEST_F(DISABLED_ON_MAC(ViEApiIntegrationTest),
       DISABLED_RunsCaptureTestWithoutErrors) {
  tests_->ViECaptureAPITest();
}

TEST_F(DISABLED_ON_MAC(ViEApiIntegrationTest), RunsCodecTestWithoutErrors) {
  tests_->ViECodecAPITest();
}

TEST_F(DISABLED_ON_MAC(ViEApiIntegrationTest),
       RunsEncryptionTestWithoutErrors) {
  tests_->ViEEncryptionAPITest();
}

TEST_F(DISABLED_ON_MAC(ViEApiIntegrationTest),
       RunsImageProcessTestWithoutErrors) {
  tests_->ViEImageProcessAPITest();
}

TEST_F(DISABLED_ON_MAC(ViEApiIntegrationTest), RunsRenderTestWithoutErrors) {
  tests_->ViERenderAPITest();
}

// See: https://code.google.com/p/webrtc/issues/detail?id=2415
TEST_F(DISABLED_ON_MAC(ViEApiIntegrationTest),
       DISABLED_RunsRtpRtcpTestWithoutErrors) {
  tests_->ViERtpRtcpAPITest();
}

}  // namespace
