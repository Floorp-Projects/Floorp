/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video_engine/test/auto_test/interface/vie_window_creator.h"

#include "video_engine/include/vie_codec.h"
#include "video_engine/test/auto_test/interface/vie_autotest_main.h"
#include "video_engine/test/auto_test/interface/vie_autotest_window_manager_interface.h"
#include "video_engine/test/auto_test/interface/vie_window_manager_factory.h"
#include "voice_engine/main/interface/voe_codec.h"

#if defined(WIN32)
#include <tchar.h>
#endif

ViEWindowCreator::ViEWindowCreator() {
#ifndef WEBRTC_ANDROID
  window_manager_ =
      ViEWindowManagerFactory::CreateWindowManagerForCurrentPlatform();
#endif
}

ViEWindowCreator::~ViEWindowCreator() {
  delete window_manager_;
}

ViEAutoTestWindowManagerInterface*
  ViEWindowCreator::CreateTwoWindows() {
#if defined(WIN32)
  TCHAR window1Title[1024] = _T("ViE Autotest Window 1");
  TCHAR window2Title[1024] = _T("ViE Autotest Window 2");
#else
  char window1Title[1024] = "ViE Autotest Window 1";
  char window2Title[1024] = "ViE Autotest Window 2";
#endif

  AutoTestRect window1Size(352, 288, 600, 100);
  AutoTestRect window2Size(352, 288, 1000, 100);
  window_manager_->CreateWindows(window1Size, window2Size, window1Title,
                                 window2Title);
  window_manager_->SetTopmostWindow();

  return window_manager_;
}

void ViEWindowCreator::TerminateWindows() {
  window_manager_->TerminateWindows();
}
