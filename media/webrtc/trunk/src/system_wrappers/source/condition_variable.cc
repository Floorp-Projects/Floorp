/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#if defined(_WIN32)
   #include <windows.h>
   #include "condition_variable_wrapper.h"
   #include "condition_variable_win.h"
#elif defined(WEBRTC_LINUX)
   #include <pthread.h>
   #include "condition_variable_wrapper.h"
   #include "condition_variable_posix.h"
#elif defined(WEBRTC_MAC) || defined(WEBRTC_MAC_INTEL)
   #include <pthread.h>
   #include "condition_variable_wrapper.h"
   #include "condition_variable_posix.h"
#endif

namespace webrtc {
ConditionVariableWrapper*
ConditionVariableWrapper::CreateConditionVariable()
{
#if defined(_WIN32)
    return new ConditionVariableWindows;
#elif defined(WEBRTC_LINUX) || defined(WEBRTC_MAC) || defined(WEBRTC_MAC_INTEL)
    return ConditionVariablePosix::Create();
#else
    return NULL;
#endif
}
} // namespace webrtc
