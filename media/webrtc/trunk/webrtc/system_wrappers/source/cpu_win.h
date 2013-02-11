// This file contains a Windows implementation of CpuWrapper.
// Note: Windows XP, Windows Server 2003 are the minimum requirements.
//       The requirements are due to the implementation being based on
//       WMI.
/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_SYSTEM_WRAPPERS_SOURCE_CPU_WINDOWS_NO_CPOL_H_
#define WEBRTC_SYSTEM_WRAPPERS_SOURCE_CPU_WINDOWS_NO_CPOL_H_

#include "cpu_wrapper.h"

#include <Wbemidl.h>

namespace webrtc {
class ConditionVariableWrapper;
class CriticalSectionWrapper;
class EventWrapper;
class ThreadWrapper;

class CpuWindows : public CpuWrapper
{
public:
    virtual WebRtc_Word32 CpuUsage();
    virtual WebRtc_Word32 CpuUsage(WebRtc_Word8* /*pProcessName*/,
                                   WebRtc_UWord32 /*length*/) {return -1;}
    virtual WebRtc_Word32 CpuUsage(WebRtc_UWord32  /*dwProcessID*/) {return -1;}

    virtual WebRtc_Word32 CpuUsageMultiCore(WebRtc_UWord32& num_cores,
                                            WebRtc_UWord32*& cpu_usage);

    virtual void Reset() {}
    virtual void Stop() {}

    CpuWindows();
    virtual ~CpuWindows();
private:
    bool AllocateComplexDataTypes();
    void DeAllocateComplexDataTypes();

    void StartPollingCpu();
    bool StopPollingCpu();

    static bool Process(void* thread_object);
    bool ProcessImpl();

    bool CreateWmiConnection();
    bool CreatePerfOsRefresher();
    bool CreatePerfOsCpuHandles();
    bool Initialize();
    bool Terminate();

    bool UpdateCpuUsage();

    ThreadWrapper* cpu_polling_thread;

    bool initialize_;
    bool has_initialized_;
    CriticalSectionWrapper* init_crit_;
    ConditionVariableWrapper* init_cond_;

    bool terminate_;
    bool has_terminated_;
    CriticalSectionWrapper* terminate_crit_;
    ConditionVariableWrapper* terminate_cond_;

    // For sleep with wake-up functionality.
    EventWrapper* sleep_event;

    // Will be an array. Just care about CPU 0 for now.
    WebRtc_UWord32* cpu_usage_;

    // One IWbemObjectAccess for each processor and one for the total.
    // 0-n-1 is the individual processors.
    // n is the total.
    IWbemObjectAccess** wbem_enum_access_;
    DWORD number_of_objects_;

    // Cpu timestamp
    long cpu_usage_handle_;
    unsigned __int64* previous_processor_timestamp_;

    // Timestamp
    long timestamp_sys_100_ns_handle_;
    unsigned __int64* previous_100ns_timestamp_;

    IWbemServices* wbem_service_;
    IWbemServices* wbem_service_proxy_;

    IWbemRefresher* wbem_refresher_;

    IWbemHiPerfEnum* wbem_enum_;

};
} // namespace webrtc
#endif // WEBRTC_SYSTEM_WRAPPERS_SOURCE_CPU_WINDOWS_NO_CPOL_H_
