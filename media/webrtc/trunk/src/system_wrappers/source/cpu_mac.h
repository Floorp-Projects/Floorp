/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_SYSTEM_WRAPPERS_SOURCE_CPU_MAC_H_
#define WEBRTC_SYSTEM_WRAPPERS_SOURCE_CPU_MAC_H_

#include "cpu_wrapper.h"

namespace webrtc {
class CpuWrapperMac : public CpuWrapper
{
public:
    CpuWrapperMac();
    virtual ~CpuWrapperMac();

    virtual WebRtc_Word32 CpuUsage();
    virtual WebRtc_Word32 CpuUsage(WebRtc_Word8* /*pProcessName*/,
                                   WebRtc_UWord32 /*length*/) {return -1;}
    virtual WebRtc_Word32 CpuUsage(WebRtc_UWord32  /*dwProcessID*/) {return -1;}

    // Note: this class will block the call and sleep if called too fast
    // This function blocks the calling thread if the thread is calling it more
    // often than every 500 ms.
    virtual WebRtc_Word32 CpuUsageMultiCore(WebRtc_UWord32& numCores,
                                            WebRtc_UWord32*& array);

    virtual void Reset() {}
    virtual void Stop() {}

private:
    WebRtc_Word32 Update(WebRtc_Word64 timeDiffMS);
    
    WebRtc_UWord32  _cpuCount;
    WebRtc_UWord32* _cpuUsage;
    WebRtc_Word32   _totalCpuUsage;
    WebRtc_Word64*  _lastTickCount;
    WebRtc_Word64   _lastTime;
};
} // namespace webrtc

#endif // WEBRTC_SYSTEM_WRAPPERS_SOURCE_CPU_MAC_H_
