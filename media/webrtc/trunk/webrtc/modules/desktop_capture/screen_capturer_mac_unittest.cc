/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/desktop_capture/screen_capturer.h"

#include <ApplicationServices/ApplicationServices.h>

#include <ostream>

#include "testing/gtest/include/gtest/gtest.h"
#include "webrtc/modules/desktop_capture/desktop_frame.h"
#include "webrtc/modules/desktop_capture/desktop_geometry.h"
#include "webrtc/modules/desktop_capture/desktop_region.h"
#include "webrtc/modules/desktop_capture/mac/desktop_configuration.h"
#include "webrtc/modules/desktop_capture/screen_capturer_mock_objects.h"
#include "webrtc/system_wrappers/interface/scoped_ptr.h"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::Return;

namespace webrtc {

class ScreenCapturerMacTest : public testing::Test {
 public:
  // Verifies that the whole screen is initially dirty.
  void CaptureDoneCallback1(DesktopFrame* frame);

  // Verifies that a rectangle explicitly marked as dirty is propagated
  // correctly.
  void CaptureDoneCallback2(DesktopFrame* frame);

 protected:
  virtual void SetUp() OVERRIDE {
    capturer_.reset(ScreenCapturer::Create());
  }

  scoped_ptr<ScreenCapturer> capturer_;
  MockScreenCapturerCallback callback_;
};

void ScreenCapturerMacTest::CaptureDoneCallback1(
    DesktopFrame* frame) {
  scoped_ptr<DesktopFrame> owned_frame(frame);

  MacDesktopConfiguration config = MacDesktopConfiguration::GetCurrent(
      MacDesktopConfiguration::BottomLeftOrigin);

  // Verify that the region contains full frame.
  DesktopRegion::Iterator it(frame->updated_region());
  EXPECT_TRUE(!it.IsAtEnd() && it.rect().equals(config.pixel_bounds));
}

void ScreenCapturerMacTest::CaptureDoneCallback2(
    DesktopFrame* frame) {
  scoped_ptr<DesktopFrame> owned_frame(frame);

  MacDesktopConfiguration config = MacDesktopConfiguration::GetCurrent(
      MacDesktopConfiguration::BottomLeftOrigin);
  int width = config.pixel_bounds.width();
  int height = config.pixel_bounds.height();

  EXPECT_EQ(width, frame->size().width());
  EXPECT_EQ(height, frame->size().height());
  EXPECT_TRUE(frame->data() != NULL);
  // Depending on the capture method, the screen may be flipped or not, so
  // the stride may be positive or negative.
  EXPECT_EQ(static_cast<int>(sizeof(uint32_t) * width),
            abs(frame->stride()));
}

TEST_F(ScreenCapturerMacTest, Capture) {
  EXPECT_CALL(callback_, OnCaptureCompleted(_))
      .Times(2)
      .WillOnce(Invoke(this, &ScreenCapturerMacTest::CaptureDoneCallback1))
      .WillOnce(Invoke(this, &ScreenCapturerMacTest::CaptureDoneCallback2));

  EXPECT_CALL(callback_, CreateSharedMemory(_))
      .Times(AnyNumber())
      .WillRepeatedly(Return(static_cast<SharedMemory*>(NULL)));

  SCOPED_TRACE("");
  capturer_->Start(&callback_);

  // Check that we get an initial full-screen updated.
  capturer_->Capture(DesktopRegion());

  // Check that subsequent dirty rects are propagated correctly.
  capturer_->Capture(DesktopRegion());
}

}  // namespace webrtc
