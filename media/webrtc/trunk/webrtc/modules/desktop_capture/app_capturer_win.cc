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
#include <windows.h>

#include "webrtc/modules/desktop_capture/desktop_frame_win.h"
#include "webrtc/system_wrappers/interface/logging.h"
#include "webrtc/system_wrappers/interface/scoped_ptr.h"

namespace webrtc {

namespace {

std::string Utf16ToUtf8(const WCHAR* str) {
  int len_utf8 = WideCharToMultiByte(CP_UTF8, 0, str, -1,
                                     NULL, 0, NULL, NULL);
  if (len_utf8 <= 0)
    return std::string();
  std::string result(len_utf8, '\0');
  int rv = WideCharToMultiByte(CP_UTF8, 0, str, -1,
                               &*(result.begin()), len_utf8, NULL, NULL);
  if (rv != len_utf8)
    assert(false);

  return result;
}


class AppCapturerWin : public AppCapturer {
public:
  AppCapturerWin();
  virtual ~AppCapturerWin();

  // AppCapturer interface.
  virtual bool GetAppList(AppList* apps) OVERRIDE;
  virtual bool SelectApp(ProcessId id) OVERRIDE;
  virtual bool BringAppToFront() OVERRIDE;

  // DesktopCapturer interface.
  virtual void Start(Callback* callback) OVERRIDE;
  virtual void Capture(const DesktopRegion& region) OVERRIDE;

private:
  Callback* callback_;

  ProcessId processId_;

  DISALLOW_COPY_AND_ASSIGN(AppCapturerWin);
};

AppCapturerWin::AppCapturerWin()
  : callback_(NULL),
    processId_(NULL) {
}

AppCapturerWin::~AppCapturerWin() {
}

// AppCapturer interface.
bool AppCapturerWin::GetAppList(AppList* apps){
  // Not implemented yet: See Bug 1036653
  return true;
}

bool AppCapturerWin::SelectApp(ProcessId id) {
  // Not implemented yet: See Bug 1036653
  return true;
}

bool AppCapturerWin::BringAppToFront() {
  // Not implemented yet: See Bug 1036653
  return true;
}

// DesktopCapturer interface.
void AppCapturerWin::Start(Callback* callback) {
  assert(!callback_);
  assert(callback);

  callback_ = callback;
}
void AppCapturerWin::Capture(const DesktopRegion& region) {
  // Not implemented yet: See Bug 1036653
}

}  // namespace

// static
AppCapturer* AppCapturer::Create(const DesktopCaptureOptions& options) {
  return new AppCapturerWin();
}

}  // namespace webrtc
