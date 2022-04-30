/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/desktop_capture/win/screen_capture_utils.h"

#include <string>
#include <vector>

#include "modules/desktop_capture/desktop_capturer.h"
#include "rtc_base/logging.h"
#include "test/gtest.h"

namespace webrtc {

TEST(ScreenCaptureUtilsTest, GetScreenList) {
  DesktopCapturer::SourceList screens;
  std::vector<std::string> device_names;

  ASSERT_TRUE(GetScreenList(&screens));
  screens.clear();
  ASSERT_TRUE(GetScreenList(&screens, &device_names));

  ASSERT_EQ(screens.size(), device_names.size());
}

TEST(ScreenCaptureUtilsTest, GetMonitorList) {
  DesktopCapturer::SourceList monitors;

  ASSERT_TRUE(GetMonitorList(&monitors));
}

TEST(ScreenCaptureUtilsTest, IsMonitorValid) {
  DesktopCapturer::SourceList monitors;

  ASSERT_TRUE(GetMonitorList(&monitors));
  if (monitors.size() == 0) {
    RTC_LOG(LS_INFO) << "Skip screen capture test on systems with no monitors.";
    GTEST_SKIP();
  }

  ASSERT_TRUE(IsMonitorValid(monitors[0].id));
}

TEST(ScreenCaptureUtilsTest, InvalidMonitor) {
  ASSERT_FALSE(IsMonitorValid(NULL));
}

}  // namespace webrtc
