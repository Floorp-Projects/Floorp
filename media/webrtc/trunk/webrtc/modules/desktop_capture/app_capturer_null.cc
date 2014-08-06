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
#include "webrtc/modules/desktop_capture/app_capturer.h"

#include <assert.h>

#include "webrtc/modules/desktop_capture/desktop_frame.h"

namespace webrtc {

namespace {

class AppCapturerNull : public AppCapturer {
public:
  AppCapturerNull();
  virtual ~AppCapturerNull();

  // AppCapturer interface.
  virtual bool GetAppList(AppList* apps) OVERRIDE;
  virtual bool SelectApp(ProcessId id) OVERRIDE;
  virtual bool BringAppToFront()	OVERRIDE;

  // DesktopCapturer interface.
  virtual void Start(Callback* callback) OVERRIDE;
  virtual void Capture(const DesktopRegion& region) OVERRIDE;

private:
  Callback* callback_;

  DISALLOW_COPY_AND_ASSIGN(AppCapturerNull);
};

AppCapturerNull::AppCapturerNull()
  : callback_(NULL) {
}

AppCapturerNull::~AppCapturerNull() {
}

bool AppCapturerNull::GetAppList(AppList* apps) {
  // Not implemented yet: See Bug 1036653
  return false;
}

bool AppCapturerNull::SelectApp(ProcessId id) {
  // Not implemented yet: See Bug 1036653
  return false;
}

bool AppCapturerNull::BringAppToFront() {
  // Not implemented yet: See Bug 1036653
  return false;
}

// DesktopCapturer interface.
void AppCapturerNull::Start(Callback* callback) {
  assert(!callback_);
  assert(callback);

  callback_ = callback;
}

void AppCapturerNull::Capture(const DesktopRegion& region) {
  // Not implemented yet: See Bug 1036653
  callback_->OnCaptureCompleted(NULL);
}

}  // namespace

// static
AppCapturer* AppCapturer::Create(const DesktopCaptureOptions& options) {
  return new AppCapturerNull();
}

}  // namespace webrtc
