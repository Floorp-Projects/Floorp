/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/system_wrappers/interface/critical_section_wrapper.h"
#include "webrtc/system_wrappers/interface/tick_util.h"
#include "webrtc/voice_engine/monitor_module.h"

namespace webrtc  {

namespace voe  {

MonitorModule::MonitorModule() :
    _observerPtr(NULL),
    _callbackCritSect(*CriticalSectionWrapper::CreateCriticalSection()),
    _lastProcessTime(TickTime::MillisecondTimestamp())
{
}

MonitorModule::~MonitorModule()
{
    delete &_callbackCritSect;
}

int32_t 
MonitorModule::RegisterObserver(MonitorObserver& observer)
{
    CriticalSectionScoped lock(&_callbackCritSect);
    if (_observerPtr)
    {
        return -1;
    }
    _observerPtr = &observer;
    return 0;
}

int32_t 
MonitorModule::DeRegisterObserver()
{
    CriticalSectionScoped lock(&_callbackCritSect);
    if (!_observerPtr)
    {
        return 0;
    }
    _observerPtr = NULL;
    return 0;
}

int32_t 
MonitorModule::Version(char* version,
                       uint32_t& remainingBufferInBytes,
                       uint32_t& position) const
{
    return 0;
}
   
int32_t 
MonitorModule::ChangeUniqueId(const int32_t id)
{
    return 0;
}

int32_t 
MonitorModule::TimeUntilNextProcess()
{
    uint32_t now = TickTime::MillisecondTimestamp();
    int32_t timeToNext =
        kAverageProcessUpdateTimeMs - (now - _lastProcessTime);
    return (timeToNext); 
}

int32_t 
MonitorModule::Process()
{
    _lastProcessTime = TickTime::MillisecondTimestamp();
    if (_observerPtr)
    {
        CriticalSectionScoped lock(&_callbackCritSect);
        _observerPtr->OnPeriodicProcess();
    }
    return 0;
}

}  //  namespace voe

}  //  namespace webrtc
