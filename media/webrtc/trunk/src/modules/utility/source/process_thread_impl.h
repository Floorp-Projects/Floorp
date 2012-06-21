/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_UTILITY_SOURCE_PROCESS_THREAD_IMPL_H_
#define WEBRTC_MODULES_UTILITY_SOURCE_PROCESS_THREAD_IMPL_H_

#include "critical_section_wrapper.h"
#include "event_wrapper.h"
#include "list_wrapper.h"
#include "process_thread.h"
#include "thread_wrapper.h"
#include "typedefs.h"

namespace webrtc {
class ProcessThreadImpl : public ProcessThread
{
public:
    ProcessThreadImpl();
    virtual ~ProcessThreadImpl();

    virtual WebRtc_Word32 Start();
    virtual WebRtc_Word32 Stop();

    virtual WebRtc_Word32 RegisterModule(const Module* module);
    virtual WebRtc_Word32 DeRegisterModule(const Module* module);

protected:
    static bool Run(void* obj);

    bool Process();

private:
    EventWrapper&           _timeEvent;
    CriticalSectionWrapper* _critSectModules;
    ListWrapper             _modules;
    ThreadWrapper*          _thread;
};
} // namespace webrtc

#endif // WEBRTC_MODULES_UTILITY_SOURCE_PROCESS_THREAD_IMPL_H_
