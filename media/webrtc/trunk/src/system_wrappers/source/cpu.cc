/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "cpu_wrapper.h"

#if defined(_WIN32)
    #include "cpu_win.h"
#elif defined(WEBRTC_MAC)
    #include "cpu_mac.h"
#elif defined(WEBRTC_MAC_INTEL)
    #include "cpu_mac.h"
#elif defined(WEBRTC_ANDROID)
    // Not implemented yet, might be possible to use Linux implementation
#else // defined(WEBRTC_LINUX)
    #include "cpu_linux.h"
#endif

namespace webrtc {
CpuWrapper* CpuWrapper::CreateCpu()
{
#if defined(_WIN32)
   return new CpuWindows();
#elif (defined(WEBRTC_MAC) || defined(WEBRTC_MAC_INTEL))
    return new CpuWrapperMac();
#elif defined(WEBRTC_ANDROID)
    return 0;
#else
    return new CpuLinux();
#endif
}
} // namespace webrtc
