/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "system_wrappers/interface/cpu_info.h"

#if defined(_WIN32)
#include <Windows.h>
#elif defined(WEBRTC_MAC)
#include <sys/sysctl.h>
#include <sys/types.h>
#elif defined(WEBRTC_ANDROID)
// Not implemented yet, might be possible to use Linux implementation
#else // defined(WEBRTC_LINUX)
#include <sys/sysinfo.h>
#endif

#include "trace.h"

namespace webrtc {

WebRtc_UWord32 CpuInfo::number_of_cores_ = 0;

WebRtc_UWord32 CpuInfo::DetectNumberOfCores() {
  if (!number_of_cores_) {
#if defined(_WIN32)
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    number_of_cores_ = static_cast<WebRtc_UWord32>(si.dwNumberOfProcessors);
    WEBRTC_TRACE(kTraceStateInfo, kTraceUtility, -1,
                 "Available number of cores:%d", number_of_cores_);

#elif defined(WEBRTC_LINUX) && !defined(WEBRTC_ANDROID)
    number_of_cores_ = get_nprocs();
    WEBRTC_TRACE(kTraceStateInfo, kTraceUtility, -1,
                 "Available number of cores:%d", number_of_cores_);

#elif defined(WEBRTC_MAC)
    int name[] = {CTL_HW, HW_AVAILCPU};
    int ncpu;
    size_t size = sizeof(ncpu);
    if (0 == sysctl(name, 2, &ncpu, &size, NULL, 0)) {
      number_of_cores_ = static_cast<WebRtc_UWord32>(ncpu);
      WEBRTC_TRACE(kTraceStateInfo, kTraceUtility, -1,
                   "Available number of cores:%d", number_of_cores_);
    } else {
      WEBRTC_TRACE(kTraceError, kTraceUtility, -1,
                   "Failed to get number of cores");
      number_of_cores_ = 1;
    }
#else
    WEBRTC_TRACE(kTraceWarning, kTraceUtility, -1,
                 "No function to get number of cores");
    number_of_cores_ = 1;
#endif
  }
  return number_of_cores_;
}

} // namespace webrtc
