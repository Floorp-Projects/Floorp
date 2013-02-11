/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/system_wrappers/interface/event_wrapper.h"

#if defined(_WIN32)
#include <windows.h>
#include "webrtc/system_wrappers/source/event_win.h"
#elif defined(WEBRTC_MAC) && !defined(WEBRTC_IOS)
#include <ApplicationServices/ApplicationServices.h>
#include <pthread.h>
#include "webrtc/system_wrappers/source/event_posix.h"
#else
#include <pthread.h>
#include "webrtc/system_wrappers/source/event_posix.h"
#endif

namespace webrtc {
EventWrapper* EventWrapper::Create() {
#if defined(_WIN32)
  return new EventWindows();
#else
  return EventPosix::Create();
#endif
}

int EventWrapper::KeyPressed() {
#if defined(_WIN32)
  int key_down = 0;
  for (int key = 0x20; key < 0x90; key++) {
    short res = GetAsyncKeyState(key);
    key_down |= res % 2; // Get the LSB
  }
  if (key_down) {
    return 1;
  } else {
    return 0;
  }
#elif defined(WEBRTC_MAC) && !defined(WEBRTC_IOS)
  bool key_down = false;
  // loop through all Mac virtual key constant values
  for (int key_index = 0; key_index <= 0x5C; key_index++) {
    key_down |= CGEventSourceKeyState(kCGEventSourceStateHIDSystemState,
                                      key_index);
  }
  if (key_down) {
    return 1;
  } else {
    return 0;
  }
#else
  return -1;
#endif
}
} // namespace webrtc
