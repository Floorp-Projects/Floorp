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

#include <cassert>

#include "webrtc/modules/desktop_capture/desktop_frame.h"

namespace webrtc {

namespace {

class WindowCapturerMac : public WindowCapturer {
 public:
  WindowCapturerMac();
  virtual ~WindowCapturerMac();

  // WindowCapturer interface.
  virtual bool GetWindowList(WindowList* windows) OVERRIDE;
  virtual bool SelectWindow(WindowId id) OVERRIDE;

  // DesktopCapturer interface.
  virtual void Start(Callback* callback) OVERRIDE;
  virtual void Capture(const DesktopRegion& region) OVERRIDE;

 private:
  Callback* callback_;

  DISALLOW_COPY_AND_ASSIGN(WindowCapturerMac);
};

WindowCapturerMac::WindowCapturerMac()
    : callback_(NULL) {
}

WindowCapturerMac::~WindowCapturerMac() {
}

bool WindowCapturerMac::GetWindowList(WindowList* windows) {
  // Not implemented yet.
  return false;
}

bool WindowCapturerMac::SelectWindow(WindowId id) {
  // Not implemented yet.
  return false;
}

void WindowCapturerMac::Start(Callback* callback) {
  assert(!callback_);
  assert(callback);

  callback_ = callback;
}

void WindowCapturerMac::Capture(const DesktopRegion& region) {
  // Not implemented yet.
  callback_->OnCaptureCompleted(NULL);
}

}  // namespace

// static
WindowCapturer* WindowCapturer::Create() {
  return new WindowCapturerMac();
}

}  // namespace webrtc
