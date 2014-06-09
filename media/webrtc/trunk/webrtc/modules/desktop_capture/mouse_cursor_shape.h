/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_DESKTOP_CAPTURE_MOUSE_CURSOR_SHAPE_H_
#define WEBRTC_MODULES_DESKTOP_CAPTURE_MOUSE_CURSOR_SHAPE_H_

#include <string>

#include "webrtc/modules/desktop_capture/desktop_geometry.h"

namespace webrtc {

// Type used to return mouse cursor shape from video capturers.
//
// TODO(sergeyu): Remove this type and use MouseCursor instead.
struct MouseCursorShape {
  // Size of the cursor in screen pixels.
  DesktopSize size;

  // Coordinates of the cursor hotspot relative to upper-left corner.
  DesktopVector hotspot;

  // Cursor pixmap data in 32-bit BGRA format.
  std::string data;
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_DESKTOP_CAPTURE_MOUSE_CURSOR_SHAPE_H_
