/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_DESKTOP_CAPTURE_SCREEN_CAPTURER_MOCK_OBJECTS_H_
#define WEBRTC_MODULES_DESKTOP_CAPTURE_SCREEN_CAPTURER_MOCK_OBJECTS_H_

#include "testing/gmock/include/gmock/gmock.h"
#include "webrtc/modules/desktop_capture/mouse_cursor_shape.h"
#include "webrtc/modules/desktop_capture/screen_capturer.h"

namespace webrtc {

class MockScreenCapturer : public ScreenCapturer {
 public:
  MockScreenCapturer() {}
  virtual ~MockScreenCapturer() {}

  MOCK_METHOD1(Start, void(Callback* callback));
  MOCK_METHOD1(Capture, void(const DesktopRegion& region));
  MOCK_METHOD1(SetMouseShapeObserver, void(
      MouseShapeObserver* mouse_shape_observer));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockScreenCapturer);
};

class MockScreenCapturerCallback : public ScreenCapturer::Callback {
 public:
  MockScreenCapturerCallback() {}
  virtual ~MockScreenCapturerCallback() {}

  MOCK_METHOD1(CreateSharedMemory, SharedMemory*(size_t));
  MOCK_METHOD1(OnCaptureCompleted, void(DesktopFrame*));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockScreenCapturerCallback);
};

class MockMouseShapeObserver : public ScreenCapturer::MouseShapeObserver {
 public:
  MockMouseShapeObserver() {}
  virtual ~MockMouseShapeObserver() {}

  void OnCursorShapeChanged(MouseCursorShape* cursor_shape) OVERRIDE {
    OnCursorShapeChangedPtr(cursor_shape);
    delete cursor_shape;
  }

  MOCK_METHOD1(OnCursorShapeChangedPtr,
               void(MouseCursorShape* cursor_shape));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockMouseShapeObserver);
};


}  // namespace webrtc

#endif  // WEBRTC_MODULES_DESKTOP_CAPTURE_SCREEN_CAPTURER_MOCK_OBJECTS_H_
