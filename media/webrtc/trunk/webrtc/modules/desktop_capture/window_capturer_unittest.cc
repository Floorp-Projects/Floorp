/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/desktop_capture/window_capturer.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "webrtc/base/scoped_ptr.h"
#include "webrtc/modules/desktop_capture/desktop_capture_options.h"
#include "webrtc/modules/desktop_capture/desktop_frame.h"
#include "webrtc/modules/desktop_capture/desktop_region.h"
#include "webrtc/system_wrappers/include/logging.h"

namespace webrtc {

class WindowCapturerTest : public testing::Test,
                           public DesktopCapturer::Callback {
 public:
  void SetUp() override {
    capturer_.reset(
        WindowCapturer::Create(DesktopCaptureOptions::CreateDefault()));
  }

  void TearDown() override {}

  // DesktopCapturer::Callback interface
  SharedMemory* CreateSharedMemory(size_t size) override { return NULL; }

  void OnCaptureCompleted(DesktopFrame* frame) override { frame_.reset(frame); }

 protected:
  rtc::scoped_ptr<WindowCapturer> capturer_;
  rtc::scoped_ptr<DesktopFrame> frame_;
};

// Verify that we can enumerate windows.
TEST_F(WindowCapturerTest, Enumerate) {
  WindowCapturer::WindowList windows;
  EXPECT_TRUE(capturer_->GetWindowList(&windows));

  // Verify that window titles are set.
  for (WindowCapturer::WindowList::iterator it = windows.begin();
       it != windows.end(); ++it) {
    EXPECT_FALSE(it->title.empty());
  }
}

// Verify we can capture a window.
//
// TODO(sergeyu): Currently this test just looks at the windows that already
// exist. Ideally it should create a test window and capture from it, but there
// is no easy cross-platform way to create new windows (potentially we could
// have a python script showing Tk dialog, but launching code will differ
// between platforms).
TEST_F(WindowCapturerTest, Capture) {
  WindowCapturer::WindowList windows;
  capturer_->Start(this);
  EXPECT_TRUE(capturer_->GetWindowList(&windows));

  // Verify that we can select and capture each window.
  for (WindowCapturer::WindowList::iterator it = windows.begin();
       it != windows.end(); ++it) {
    frame_.reset();
    if (capturer_->SelectWindow(it->id)) {
      capturer_->Capture(DesktopRegion());
    }

    // If we failed to capture a window make sure it no longer exists.
    if (!frame_.get()) {
      WindowCapturer::WindowList new_list;
      EXPECT_TRUE(capturer_->GetWindowList(&new_list));
      for (WindowCapturer::WindowList::iterator new_list_it = new_list.begin();
           new_list_it != new_list.end(); ++new_list_it) {
        EXPECT_FALSE(it->id == new_list_it->id);
      }
      continue;
    }

    EXPECT_GT(frame_->size().width(), 0);
    EXPECT_GT(frame_->size().height(), 0);
  }
}

}  // namespace webrtc
