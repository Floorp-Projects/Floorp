/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_DESKTOP_CAPTURE_LINUX_X11_X_ERROR_TRAP_H_
#define MODULES_DESKTOP_CAPTURE_LINUX_X11_X_ERROR_TRAP_H_

#include <X11/Xlibint.h>
#undef max // Xlibint.h defines this and it breaks std::max
#undef min // Xlibint.h defines this and it breaks std::min

namespace webrtc {

// Helper class that registers X Window error handler. Caller can use
// GetLastErrorAndDisable() to get the last error that was caught, if any.
// An XErrorTrap may be constructed on any thread, but errors are collected
// from all threads and so |display| should be used only on one thread.
// Other Displays are unaffected.
class XErrorTrap {
 public:
  explicit XErrorTrap(Display* display);
  ~XErrorTrap();

  XErrorTrap(const XErrorTrap&) = delete;
  XErrorTrap& operator=(const XErrorTrap&) = delete;

  // Returns last error and removes unregisters the error handler.
  // Must not be called more than once.
  int GetLastErrorAndDisable();

 private:
  static Bool XServerErrorHandler(Display* display, xReply* rep,
                                  char* /* buf */, int /* len */,
                                  XPointer data);

  _XAsyncHandler async_handler_;
  Display* display_;
  unsigned long last_ignored_request_;
  int last_xserver_error_code_;
  bool enabled_;
};

}  // namespace webrtc

#endif  // MODULES_DESKTOP_CAPTURE_LINUX_X11_X_ERROR_TRAP_H_
