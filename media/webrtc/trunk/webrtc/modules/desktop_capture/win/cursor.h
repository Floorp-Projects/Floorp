/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_DESKTOP_CAPTURE_WIN_CURSOR_H_
#define WEBRTC_MODULES_DESKTOP_CAPTURE_WIN_CURSOR_H_

#include <windows.h>

#include "webrtc/modules/desktop_capture/mouse_cursor_shape.h"

namespace webrtc {

// Converts a cursor into a |MouseCursorShape| instance.
MouseCursorShape* CreateMouseCursorShapeFromCursor(
    HDC dc, HCURSOR cursor);

}  // namespace webrtc

#endif  // WEBRTC_MODULES_DESKTOP_CAPTURE_WIN_CURSOR_H_
