/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/system_wrappers/interface/thread_wrapper.h"

#if defined(_WIN32)
#include "webrtc/system_wrappers/source/thread_win.h"
#else
#include "webrtc/system_wrappers/source/thread_posix.h"
#endif

namespace webrtc {

#if defined(_WIN32)
typedef ThreadWindows ThreadType;
#else
typedef ThreadPosix ThreadType;
#endif

rtc::scoped_ptr<ThreadWrapper> ThreadWrapper::CreateThread(
    ThreadRunFunction func, void* obj, const char* thread_name) {
  return rtc::scoped_ptr<ThreadWrapper>(
      new ThreadType(func, obj, thread_name)).Pass();
}

}  // namespace webrtc
