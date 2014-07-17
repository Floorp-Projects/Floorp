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
#include <ApplicationServices/ApplicationServices.h>
#include <Cocoa/Cocoa.h>
#include <CoreFoundation/CoreFoundation.h>

#include "webrtc/modules/desktop_capture/desktop_frame.h"
#include "webrtc/system_wrappers/interface/logging.h"

namespace webrtc {

namespace {

class AppCapturerMac : public AppCapturer {
 public:
  AppCapturerMac();
  virtual ~AppCapturerMac();

  // AppCapturer interface.
  virtual bool GetAppList(AppList* apps) OVERRIDE;
  virtual bool SelectApp(ProcessId id) OVERRIDE;
  virtual bool BringAppToFront() OVERRIDE;

  // DesktopCapturer interface.
  virtual void Start(Callback* callback) OVERRIDE;
  virtual void Capture(const DesktopRegion& region) OVERRIDE;

 private:
  Callback* callback_;
  ProcessId process_id_;

  DISALLOW_COPY_AND_ASSIGN(AppCapturerMac);
};

AppCapturerMac::AppCapturerMac()
  : callback_(NULL),
    process_id_(0) {
}

AppCapturerMac::~AppCapturerMac() {
}

// AppCapturer interface.
bool AppCapturerMac::GetAppList(AppList* apps){
  // Not implemented yet: See Bug 1036653
  return true;
}

bool AppCapturerMac::SelectApp(ProcessId id){
  // Not implemented yet: See Bug 1036653
  return true;
}

bool AppCapturerMac::BringAppToFront() {
  // Not implemented yet: See Bug 1036653
   return true;
}

// DesktopCapturer interface.
void AppCapturerMac::Start(Callback* callback) {
  assert(!callback_);
  assert(callback);

  callback_ = callback;
}

void AppCapturerMac::Capture(const DesktopRegion& region) {
  // Not implemented yet: See Bug 1036653
}

}  // namespace

// static
AppCapturer* AppCapturer::Create(const DesktopCaptureOptions& options) {
  return new AppCapturerMac();
}

}  // namespace webrtc
