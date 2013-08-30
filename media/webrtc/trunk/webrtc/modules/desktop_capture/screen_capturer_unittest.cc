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

 protected:
  scoped_ptr<ScreenCapturer> capturer_;
  MockMouseShapeObserver mouse_observer_;
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

TEST_F(ScreenCapturerTest, StartCapturer) {
  capturer_.reset(ScreenCapturer::Create());
  capturer_->SetMouseShapeObserver(&mouse_observer_);
  capturer_->Start(&callback_);
}

TEST_F(ScreenCapturerTest, Capture) {
  // Assume that Start() treats the screen as invalid initially.
  DesktopFrame* frame = NULL;
  EXPECT_CALL(callback_, OnCaptureCompleted(_))
      .WillOnce(SaveArg<0>(&frame));
  EXPECT_CALL(mouse_observer_, OnCursorShapeChangedPtr(_))
      .Times(AnyNumber());

  EXPECT_CALL(callback_, CreateSharedMemory(_))
      .Times(AnyNumber())
      .WillRepeatedly(Return(static_cast<SharedMemory*>(NULL)));

  capturer_.reset(ScreenCapturer::Create());
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

#if defined(OS_WIN)

TEST_F(ScreenCapturerTest, UseSharedBuffers) {
  DesktopFrame* frame = NULL;
  EXPECT_CALL(callback_, OnCaptureCompleted(_))
      .WillOnce(SaveArg<0>(&frame));
  EXPECT_CALL(mouse_observer_, OnCursorShapeChangedPtr(_))
      .Times(AnyNumber());

  EXPECT_CALL(callback_, CreateSharedMemory(_))
      .Times(AnyNumber())
      .WillRepeatedly(Invoke(this, &ScreenCapturerTest::CreateSharedMemory));

  capturer_.reset(ScreenCapturer::Create());
  capturer_->Start(&callback_);
  capturer_->Capture(DesktopRegion());

  ASSERT_TRUE(frame);
  ASSERT_TRUE(frame->shared_memory());
  EXPECT_EQ(frame->shared_memory()->id(), kTestSharedMemoryId);

  delete frame;
}

#endif  // defined(OS_WIN)

}  // namespace webrtc
