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

#include <assert.h>

#include "webrtc/modules/desktop_capture/desktop_frame.h"

namespace webrtc {

namespace {

class WindowCapturerNull : public WindowCapturer {
 public:
  WindowCapturerNull();
  virtual ~WindowCapturerNull();

  // WindowCapturer interface.
  virtual bool GetWindowList(WindowList* windows) OVERRIDE;
  virtual bool SelectWindow(WindowId id) OVERRIDE;
  virtual bool BringSelectedWindowToFront() OVERRIDE;

  // DesktopCapturer interface.
  virtual void Start(Callback* callback) OVERRIDE;
  virtual void Capture(const DesktopRegion& region) OVERRIDE;

 private:
  Callback* callback_;

  DISALLOW_COPY_AND_ASSIGN(WindowCapturerNull);
};

WindowCapturerNull::WindowCapturerNull()
    : callback_(NULL) {
}

WindowCapturerNull::~WindowCapturerNull() {
}

bool WindowCapturerNull::GetWindowList(WindowList* windows) {
  // Not implemented yet.
  return false;
}

bool WindowCapturerNull::SelectWindow(WindowId id) {
  // Not implemented yet.
  return false;
}

bool WindowCapturerNull::BringSelectedWindowToFront() {
  // Not implemented yet.
  return false;
}

void WindowCapturerNull::Start(Callback* callback) {
  assert(!callback_);
  assert(callback);

  callback_ = callback;
}

void WindowCapturerNull::Capture(const DesktopRegion& region) {
  // Not implemented yet.
  callback_->OnCaptureCompleted(NULL);
}

}  // namespace

// static
WindowCapturer* WindowCapturer::Create(const DesktopCaptureOptions& options) {
  return new WindowCapturerNull();
}

}  // namespace webrtc
