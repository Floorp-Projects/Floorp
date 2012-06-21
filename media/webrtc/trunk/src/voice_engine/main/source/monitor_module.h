/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VOICE_ENGINE_MONITOR_MODULE_H
#define WEBRTC_VOICE_ENGINE_MONITOR_MODULE_H

#include "module.h"
#include "typedefs.h"
#include "voice_engine_defines.h"

class MonitorObserver
{
public:
    virtual void OnPeriodicProcess() = 0;
protected:
    virtual ~MonitorObserver() {}
};


namespace webrtc {
class CriticalSectionWrapper;

namespace voe {

class MonitorModule : public Module
{
public:
    WebRtc_Word32 RegisterObserver(MonitorObserver& observer);

    WebRtc_Word32 DeRegisterObserver();

    MonitorModule();

    virtual ~MonitorModule();
public:	// module
    WebRtc_Word32 Version(char* version,
                          WebRtc_UWord32& remainingBufferInBytes,
                          WebRtc_UWord32& position) const;

    WebRtc_Word32 ChangeUniqueId(const WebRtc_Word32 id);

    WebRtc_Word32 TimeUntilNextProcess();

    WebRtc_Word32 Process();
private:
    enum { kAverageProcessUpdateTimeMs = 1000 };
    MonitorObserver* _observerPtr;
    CriticalSectionWrapper&	_callbackCritSect;
    WebRtc_Word32 _lastProcessTime;
};

}  //  namespace voe

}  //  namespace webrtc

#endif // VOICE_ENGINE_MONITOR_MODULE
