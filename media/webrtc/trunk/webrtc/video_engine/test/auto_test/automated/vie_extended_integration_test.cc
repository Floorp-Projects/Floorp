/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/**
 * Runs "extended" integration tests.
 */

#include "gtest/gtest.h"
#include "legacy_fixture.h"
#include "vie_autotest.h"

namespace {

class ViEExtendedIntegrationTest : public LegacyFixture {
};

TEST_F(ViEExtendedIntegrationTest, RunsBaseTestWithoutErrors) {
  tests_->ViEBaseExtendedTest();
}

// TODO(phoglund): Crashes on the v4l2loopback camera.
TEST_F(ViEExtendedIntegrationTest, DISABLED_RunsCaptureTestWithoutErrors) {
  tests_->ViECaptureExtendedTest();
}

TEST_F(ViEExtendedIntegrationTest, RunsCodecTestWithoutErrors) {
  tests_->ViECodecExtendedTest();
}

TEST_F(ViEExtendedIntegrationTest, RunsEncryptionTestWithoutErrors) {
  tests_->ViEEncryptionExtendedTest();
}

TEST_F(ViEExtendedIntegrationTest, RunsFileTestWithoutErrors) {
  tests_->ViEFileExtendedTest();
}

TEST_F(ViEExtendedIntegrationTest, RunsImageProcessTestWithoutErrors) {
  tests_->ViEImageProcessExtendedTest();
}

TEST_F(ViEExtendedIntegrationTest, RunsNetworkTestWithoutErrors) {
  tests_->ViENetworkExtendedTest();
}

TEST_F(ViEExtendedIntegrationTest, RunsRenderTestWithoutErrors) {
  tests_->ViERenderExtendedTest();
}

TEST_F(ViEExtendedIntegrationTest, RunsRtpRtcpTestWithoutErrors) {
  tests_->ViERtpRtcpExtendedTest();
}

} // namespace
