/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_DESKTOP_CAPTURE_WINDOW_LIST_UTILS_H_
#define WEBRTC_MODULES_DESKTOP_CAPTURE_WINDOW_LIST_UTILS_H_

#include "webrtc/modules/desktop_capture/window_capturer.h"

namespace webrtc {

// A helper function to get the on-screen windows.
bool GetWindowList(WindowCapturer::WindowList* windows);

}  // namespace webrtc

#endif  // WEBRTC_MODULES_DESKTOP_CAPTURE_WINDOW_LIST_UTILS_H_

