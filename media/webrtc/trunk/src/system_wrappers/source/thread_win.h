/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_SYSTEM_WRAPPERS_SOURCE_THREAD_WINDOWS_H_
#define WEBRTC_SYSTEM_WRAPPERS_SOURCE_THREAD_WINDOWS_H_

#include "thread_wrapper.h"
#include "event_wrapper.h"
#include "critical_section_wrapper.h"

#include <windows.h>

namespace webrtc {

class ThreadWindows : public ThreadWrapper
{
public:
    ThreadWindows(ThreadRunFunction func, ThreadObj obj, ThreadPriority prio,
                  const char* threadName);
    virtual ~ThreadWindows();

    virtual bool Start(unsigned int& id);
    bool SetAffinity(const int* processorNumbers,
                     const unsigned int amountOfProcessors);
    virtual bool Stop();
    virtual void SetNotAlive();

    static unsigned int WINAPI StartThread(LPVOID lpParameter);

    virtual bool Shutdown();

protected:
    virtual void Run();

private:
    ThreadRunFunction    _runFunction;
    ThreadObj            _obj;

    bool                    _alive;
    bool                    _dead;

    // TODO (hellner)
    // _doNotCloseHandle member seem pretty redundant. Should be able to remove
    // it. Basically it should be fine to reclaim the handle when calling stop
    // and in the destructor.
    bool                    _doNotCloseHandle;
    ThreadPriority          _prio;
    EventWrapper*           _event;
    CriticalSectionWrapper* _critsectStop;

    HANDLE                  _thread;
    unsigned int            _id;
    char                    _name[kThreadMaxNameLength];
    bool                    _setThreadName;

};
} // namespace webrtc

#endif // WEBRTC_SYSTEM_WRAPPERS_SOURCE_THREAD_WINDOWS_H_
