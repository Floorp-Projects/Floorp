/*
*  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
*
*  Use of this source code is governed by a BSD-style license
*  that can be found in the LICENSE file in the root of the source
*  tree. An additional intellectual property rights grant can be found
*  in the file PATENTS.  All contributing project authors may
*  be found in the AUTHORS file in the root of the source tree.
*/
#include "webrtc/modules/desktop_capture/app_capturer.h"

#include "gtest/gtest.h"
#include "webrtc/modules/desktop_capture/desktop_capture_options.h"
#include "webrtc/modules/desktop_capture/desktop_frame.h"
#include "webrtc/modules/desktop_capture/desktop_region.h"
#include "webrtc/system_wrappers/interface/logging.h"
#include "webrtc/base/scoped_ptr.h"

namespace webrtc {

class AppCapturerTest : public testing::Test,
                        public DesktopCapturer::Callback {
public:
  void SetUp() override {
    capturer_.reset(
      AppCapturer::Create(DesktopCaptureOptions::CreateDefault())
    );
  }

  void TearDown() override {
  }

  // DesktopCapturer::Callback interface
  virtual SharedMemory* CreateSharedMemory(size_t size) override {
    return NULL;
  }

  virtual void OnCaptureCompleted(DesktopFrame* frame) override {
    frame_.reset(frame);
  }

protected:
  rtc::scoped_ptr<AppCapturer> capturer_;
  rtc::scoped_ptr<DesktopFrame> frame_;
};

// Verify that we can enumerate applications.
TEST_F(AppCapturerTest, Enumerate) {
  AppCapturer::AppList apps;
  EXPECT_TRUE(capturer_->GetAppList(&apps));

  // Verify that window titles are set.
  for (AppCapturer::AppList::iterator it = apps.begin();
       it != windows.end(); ++it) {
    EXPECT_FALSE(it->title.empty());
  }
}

// Verify we can capture a app.
TEST_F(AppCapturerTest, Capture) {
  AppCapturer::AppList apps;
  capturer_->Start(this);
  EXPECT_TRUE(capturer_->GetAppList(&apps));

  // Verify that we can select and capture each app.
  for (AppCapturer::AppList::iterator it = apps.begin();
       it != apps.end(); ++it) {
    frame_.reset();
    if (capturer_->SelectApp(it->id)) {
      capturer_->Capture(DesktopRegion());
    }

    // If we failed to capture a window make sure it no longer exists.
    if (!frame_.get()) {
      AppCapturer::AppList new_list;
      EXPECT_TRUE(capturer_->GetAppList(&new_list));
      for (AppCapturer::AppList::iterator new_list_it = apps.begin();
           new_list_it != apps.end(); ++new_list_it) {
        EXPECT_FALSE(it->id == new_list_it->id);
      }
      continue;
    }

    EXPECT_GT(frame_->size().width(), 0);
    EXPECT_GT(frame_->size().height(), 0);
  }
}

}  // namespace webrtc
