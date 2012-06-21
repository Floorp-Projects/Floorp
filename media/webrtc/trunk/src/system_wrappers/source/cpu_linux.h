/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_SYSTEM_WRAPPERS_SOURCE_CPU_LINUX_H_
#define WEBRTC_SYSTEM_WRAPPERS_SOURCE_CPU_LINUX_H_

#include "cpu_wrapper.h"

namespace webrtc {
class CpuLinux : public CpuWrapper
{
public:
    CpuLinux();
    virtual ~CpuLinux();

    virtual WebRtc_Word32 CpuUsage();
    virtual WebRtc_Word32 CpuUsage(WebRtc_Word8* /*pProcessName*/,
                                   WebRtc_UWord32 /*length*/) {return 0;}
    virtual WebRtc_Word32 CpuUsage(WebRtc_UWord32 /*dwProcessID*/) {return 0;}

    virtual WebRtc_Word32 CpuUsageMultiCore(WebRtc_UWord32& numCores,
                                            WebRtc_UWord32*& array);

    virtual void Reset() {return;}
    virtual void Stop() {return;}
private:
    int GetData(long long& busy, long long& idle, long long*& busyArray,
                long long*& idleArray);
    int GetNumCores();

    long long m_oldBusyTime;
    long long m_oldIdleTime;

    long long* m_oldBusyTimeMulti;
    long long* m_oldIdleTimeMulti;

    long long* m_idleArray;
    long long* m_busyArray;
    WebRtc_UWord32* m_resultArray;
    WebRtc_UWord32  m_numCores;
};
} // namespace webrtc

#endif // WEBRTC_SYSTEM_WRAPPERS_SOURCE_CPU_LINUX_H_
