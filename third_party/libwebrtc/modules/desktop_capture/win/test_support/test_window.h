/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef MODULES_DESKTOP_CAPTURE_WIN_TEST_SUPPORT_TEST_WINDOW_H_
#define MODULES_DESKTOP_CAPTURE_WIN_TEST_SUPPORT_TEST_WINDOW_H_

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace webrtc {

struct WindowInfo {
  HWND hwnd;
  HINSTANCE window_instance;
  ATOM window_class;
};

WindowInfo CreateTestWindow(const WCHAR* window_title,
                            const int height = 0,
                            const int width = 0);

void ResizeTestWindow(const HWND hwnd, const int width, const int height);

void MinimizeTestWindow(const HWND hwnd);

void UnminimizeTestWindow(const HWND hwnd);

void DestroyTestWindow(WindowInfo info);

}  // namespace webrtc

#endif  // MODULES_DESKTOP_CAPTURE_WIN_TEST_SUPPORT_TEST_WINDOW_H_
