/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video_engine/test/auto_test/automated/two_windows_fixture.h"

#include "video_engine/test/auto_test/interface/vie_window_creator.h"
#include "video_engine/test/auto_test/interface/vie_autotest_window_manager_interface.h"

void TwoWindowsFixture::SetUpTestCase() {
  window_creator_ = new ViEWindowCreator();

  ViEAutoTestWindowManagerInterface* window_manager =
      window_creator_->CreateTwoWindows();

  window_1_ = window_manager->GetWindow1();
  window_2_ = window_manager->GetWindow2();
}

void TwoWindowsFixture::TearDownTestCase() {
  window_creator_->TerminateWindows();
  delete window_creator_;
}

ViEWindowCreator* TwoWindowsFixture::window_creator_ = NULL;
void* TwoWindowsFixture::window_1_ = NULL;
void* TwoWindowsFixture::window_2_ = NULL;
