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
#include <string.h>
#include <X11/Xatom.h>
#include <X11/extensions/Xcomposite.h>
#include <X11/extensions/Xrender.h>
#include <X11/Xutil.h>

#include <algorithm>

#include "webrtc/modules/desktop_capture/desktop_capture_options.h"
#include "webrtc/modules/desktop_capture/desktop_frame.h"
#include "webrtc/modules/desktop_capture/x11/shared_x_display.h"
#include "webrtc/modules/desktop_capture/x11/x_error_trap.h"
#include "webrtc/modules/desktop_capture/x11/x_server_pixel_buffer.h"
#include "webrtc/system_wrappers/interface/logging.h"
#include "webrtc/system_wrappers/interface/scoped_ptr.h"
#include "webrtc/system_wrappers/interface/scoped_refptr.h"

namespace webrtc {

namespace {


class AppCapturerLinux : public AppCapturer{
public:
  AppCapturerLinux(const DesktopCaptureOptions& options);
  virtual ~AppCapturerLinux();

  // AppCapturer interface.
  virtual bool GetAppList(AppList* apps) OVERRIDE;
  virtual bool SelectApp(ProcessId id) OVERRIDE;
  virtual bool BringAppToFront() OVERRIDE;

  // DesktopCapturer interface.
  virtual void Start(Callback* callback) OVERRIDE;
  virtual void Capture(const DesktopRegion& region) OVERRIDE;


private:
  Callback* callback_;
  ProcessId selected_process_;

  DISALLOW_COPY_AND_ASSIGN(AppCapturerLinux);
};

AppCapturerLinux::AppCapturerLinux(const DesktopCaptureOptions& options)
  : callback_(NULL),
    selected_process_(0) {
}

AppCapturerLinux::~AppCapturerLinux() {
}

// AppCapturer interface.
bool AppCapturerLinux::GetAppList(AppList* apps){
  // Not implemented yet: See Bug 1036653
  return true;
}
bool AppCapturerLinux::SelectApp(ProcessId id){
  // Not implemented yet: See Bug 1036653
  return true;
}
bool AppCapturerLinux::BringAppToFront(){
  // Not implemented yet: See Bug 1036653
  return true;
}

// DesktopCapturer interface.
void AppCapturerLinux::Start(Callback* callback) {
  assert(!callback_);
  assert(callback);

  callback_ = callback;
}

void AppCapturerLinux::Capture(const DesktopRegion& region) {
  // Not implemented yet: See Bug 1036653
}

}  // namespace

// static
AppCapturer* AppCapturer::Create(const DesktopCaptureOptions& options) {
  return new AppCapturerLinux(options);
}

}  // namespace webrtc
