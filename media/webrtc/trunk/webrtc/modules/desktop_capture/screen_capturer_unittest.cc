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

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webrtc/modules/desktop_capture/desktop_capture_options.h"
#include "webrtc/modules/desktop_capture/desktop_frame.h"
#include "webrtc/modules/desktop_capture/desktop_region.h"
#include "webrtc/modules/desktop_capture/screen_capturer_mock_objects.h"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::Return;
using ::testing::SaveArg;

const int kTestSharedMemoryId = 123;

namespace webrtc {

class ScreenCapturerTest : public testing::Test {
 public:
  SharedMemory* CreateSharedMemory(size_t size);

  virtual void SetUp() OVERRIDE {
    capturer_.reset(
        ScreenCapturer::Create(DesktopCaptureOptions::CreateDefault()));
  }

 protected:
  scoped_ptr<ScreenCapturer> capturer_;
  MockScreenCapturerCallback callback_;
};

class FakeSharedMemory : public SharedMemory {
 public:
  FakeSharedMemory(char* buffer, size_t size)
    : SharedMemory(buffer, size, 0, kTestSharedMemoryId),
      buffer_(buffer) {
  }
  virtual ~FakeSharedMemory() {
    delete[] buffer_;
  }
 private:
  char* buffer_;
  DISALLOW_COPY_AND_ASSIGN(FakeSharedMemory);
};

SharedMemory* ScreenCapturerTest::CreateSharedMemory(size_t size) {
  return new FakeSharedMemory(new char[size], size);
}

TEST_F(ScreenCapturerTest, GetScreenListAndSelectScreen) {
  webrtc::ScreenCapturer::ScreenList screens;
  EXPECT_TRUE(capturer_->GetScreenList(&screens));
  for(webrtc::ScreenCapturer::ScreenList::iterator it = screens.begin();
      it != screens.end(); ++it) {
    EXPECT_TRUE(capturer_->SelectScreen(it->id));
  }
}

TEST_F(ScreenCapturerTest, StartCapturer) {
  capturer_->Start(&callback_);
}

TEST_F(ScreenCapturerTest, Capture) {
  // Assume that Start() treats the screen as invalid initially.
  DesktopFrame* frame = NULL;
  EXPECT_CALL(callback_, OnCaptureCompleted(_))
      .WillOnce(SaveArg<0>(&frame));

  EXPECT_CALL(callback_, CreateSharedMemory(_))
      .Times(AnyNumber())
      .WillRepeatedly(Return(static_cast<SharedMemory*>(NULL)));

  capturer_->Start(&callback_);
  capturer_->Capture(DesktopRegion());

  ASSERT_TRUE(frame);
  EXPECT_GT(frame->size().width(), 0);
  EXPECT_GT(frame->size().height(), 0);
  EXPECT_GE(frame->stride(),
            frame->size().width() * DesktopFrame::kBytesPerPixel);
  EXPECT_TRUE(frame->shared_memory() == NULL);

  // Verify that the region contains whole screen.
  EXPECT_FALSE(frame->updated_region().is_empty());
  DesktopRegion::Iterator it(frame->updated_region());
  ASSERT_TRUE(!it.IsAtEnd());
  EXPECT_TRUE(it.rect().equals(DesktopRect::MakeSize(frame->size())));
  it.Advance();
  EXPECT_TRUE(it.IsAtEnd());

  delete frame;
}

#if defined(WEBRTC_WIN)

TEST_F(ScreenCapturerTest, UseSharedBuffers) {
  DesktopFrame* frame = NULL;
  EXPECT_CALL(callback_, OnCaptureCompleted(_))
      .WillOnce(SaveArg<0>(&frame));

  EXPECT_CALL(callback_, CreateSharedMemory(_))
      .Times(AnyNumber())
      .WillRepeatedly(Invoke(this, &ScreenCapturerTest::CreateSharedMemory));

  capturer_->Start(&callback_);
  capturer_->Capture(DesktopRegion());

  ASSERT_TRUE(frame);
  ASSERT_TRUE(frame->shared_memory());
  EXPECT_EQ(frame->shared_memory()->id(), kTestSharedMemoryId);

  delete frame;
}

TEST_F(ScreenCapturerTest, UseMagnifier) {
  DesktopCaptureOptions options(DesktopCaptureOptions::CreateDefault());
  options.set_allow_use_magnification_api(true);
  capturer_.reset(ScreenCapturer::Create(options));

  DesktopFrame* frame = NULL;
  EXPECT_CALL(callback_, OnCaptureCompleted(_)).WillOnce(SaveArg<0>(&frame));

  capturer_->Start(&callback_);
  capturer_->Capture(DesktopRegion());
  ASSERT_TRUE(frame);
  delete frame;
}

#endif  // defined(WEBRTC_WIN)

}  // namespace webrtc
