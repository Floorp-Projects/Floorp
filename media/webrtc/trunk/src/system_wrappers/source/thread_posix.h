/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_SYSTEM_WRAPPERS_SOURCE_THREAD_POSIX_H_
#define WEBRTC_SYSTEM_WRAPPERS_SOURCE_THREAD_POSIX_H_

#include "thread_wrapper.h"
#include <pthread.h>

namespace webrtc {

class CriticalSectionWrapper;
class EventWrapper;

class ThreadPosix : public ThreadWrapper
{
public:
    static ThreadWrapper* Create(ThreadRunFunction func, ThreadObj obj,
                                 ThreadPriority prio, const char* threadName);

    ThreadPosix(ThreadRunFunction func, ThreadObj obj, ThreadPriority prio,
                const char* threadName);
    ~ThreadPosix();

    // From ThreadWrapper
    virtual void SetNotAlive();
    virtual bool Start(unsigned int& id);
    // Not implemented on Mac
    virtual bool SetAffinity(const int* processorNumbers,
                             unsigned int amountOfProcessors);
    virtual bool Stop();
    virtual bool Shutdown();

    void Run();

private:
    int Construct();

private:
    // processing function
    ThreadRunFunction   _runFunction;
    ThreadObj           _obj;

    // internal state
    CriticalSectionWrapper* _crit_state;  // Protects _alive and _dead
    bool                    _alive;
    bool                    _dead;
    ThreadPriority          _prio;
    EventWrapper*           _event;

    // zero-terminated thread name string
    char                    _name[kThreadMaxNameLength];
    bool                    _setThreadName;

    // handle to thread
#if (defined(WEBRTC_LINUX) || defined(WEBRTC_ANDROID))
    pid_t                   _pid;
#endif
    pthread_attr_t          _attr;
    pthread_t               _thread;
};
} // namespace webrtc

#endif // WEBRTC_SYSTEM_WRAPPERS_SOURCE_THREAD_POSIX_H_
