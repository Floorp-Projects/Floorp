/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "cpu_mac.h"

#include <iostream>
#include <mach/mach.h>
#include <mach/mach_error.h>

#include "tick_util.h"

namespace webrtc {
CpuWrapperMac::CpuWrapperMac()
    : _cpuCount(0),
      _cpuUsage(NULL), 
      _totalCpuUsage(0),
      _lastTickCount(NULL),
      _lastTime(0)
{
    natural_t cpuCount;
    processor_info_array_t infoArray;
    mach_msg_type_number_t infoCount;

    kern_return_t error = host_processor_info(mach_host_self(),
                                              PROCESSOR_CPU_LOAD_INFO,
                                              &cpuCount,
                                              &infoArray,
                                              &infoCount);
    if (error)
    {
        return;
    }

    _cpuCount = cpuCount;
    _cpuUsage = new WebRtc_UWord32[cpuCount];
    _lastTickCount = new WebRtc_Word64[cpuCount];
    _lastTime = TickTime::MillisecondTimestamp();

    processor_cpu_load_info_data_t* cpuLoadInfo =
        (processor_cpu_load_info_data_t*) infoArray;
    for (unsigned int cpu= 0; cpu < cpuCount; cpu++)
    {
        WebRtc_Word64 ticks = 0;
        for (int state = 0; state < 2; state++)
        {
            ticks += cpuLoadInfo[cpu].cpu_ticks[state];
        }
        _lastTickCount[cpu] = ticks;
        _cpuUsage[cpu] = 0;
    }
    vm_deallocate(mach_task_self(), (vm_address_t)infoArray, infoCount);
}

CpuWrapperMac::~CpuWrapperMac()
{
    delete[] _cpuUsage;
    delete[] _lastTickCount;
}

WebRtc_Word32 CpuWrapperMac::CpuUsage()
{
    WebRtc_UWord32 numCores;
    WebRtc_UWord32* array = NULL;
    return CpuUsageMultiCore(numCores, array);
}

WebRtc_Word32
CpuWrapperMac::CpuUsageMultiCore(WebRtc_UWord32& numCores,
                                 WebRtc_UWord32*& array)
{
    // sanity check
    if(_cpuUsage == NULL)
    {
        return -1;
    }
    
    WebRtc_Word64 now = TickTime::MillisecondTimestamp();
    WebRtc_Word64 timeDiffMS = now - _lastTime;
    if(timeDiffMS >= 500) 
    {
        if(Update(timeDiffMS) != 0) 
        {
           return -1;
        }
        _lastTime = now;
    }
    
    numCores = _cpuCount;
    array = _cpuUsage;
    return _totalCpuUsage / _cpuCount;
}

WebRtc_Word32 CpuWrapperMac::Update(WebRtc_Word64 timeDiffMS)
{    
    natural_t cpuCount;
    processor_info_array_t infoArray;
    mach_msg_type_number_t infoCount;
    
    kern_return_t error = host_processor_info(mach_host_self(),
                                              PROCESSOR_CPU_LOAD_INFO,
                                              &cpuCount,
                                              &infoArray,
                                              &infoCount);
    if (error)
    {
        return -1;
    }

    processor_cpu_load_info_data_t* cpuLoadInfo =
        (processor_cpu_load_info_data_t*) infoArray;

    _totalCpuUsage = 0;
    for (unsigned int cpu = 0; cpu < cpuCount; cpu++)
    {
        WebRtc_Word64 ticks = 0;
        for (int state = 0; state < 2; state++)
        {
            ticks += cpuLoadInfo[cpu].cpu_ticks[state];
        }
        if(timeDiffMS <= 0)
        {
            _cpuUsage[cpu] = 0;
        }else {
            _cpuUsage[cpu] = (WebRtc_UWord32)((1000 *
                                              (ticks - _lastTickCount[cpu])) /
                                              timeDiffMS);
        }
        _lastTickCount[cpu] = ticks;
        _totalCpuUsage += _cpuUsage[cpu];
    }

    vm_deallocate(mach_task_self(), (vm_address_t)infoArray, infoCount);

    return 0;
}
} // namespace webrtc
