/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "thread_posix.h"

#include <errno.h>
#include <string.h> // strncpy
#include <time.h>   // nanosleep
#include <unistd.h>
#ifdef WEBRTC_LINUX
#include <sys/types.h>
#include <sched.h>
#include <sys/syscall.h>
#include <linux/unistd.h>
#include <sys/prctl.h>
#endif

#if defined(WEBRTC_MAC)
#include <mach/mach.h>
#endif

#include "event_wrapper.h"
#include "trace.h"

namespace webrtc {
extern "C"
{
    static void* StartThread(void* lpParameter)
    {
        static_cast<ThreadPosix*>(lpParameter)->Run();
        return 0;
    }
}

ThreadWrapper* ThreadPosix::Create(ThreadRunFunction func, ThreadObj obj,
                                   ThreadPriority prio, const char* threadName)
{
    ThreadPosix* ptr = new ThreadPosix(func, obj, prio, threadName);
    if (!ptr)
    {
        return NULL;
    }
    const int error = ptr->Construct();
    if (error)
    {
        delete ptr;
        return NULL;
    }
    return ptr;
}

ThreadPosix::ThreadPosix(ThreadRunFunction func, ThreadObj obj,
                         ThreadPriority prio, const char* threadName)
    : _runFunction(func),
      _obj(obj),
      _alive(false),
      _dead(true),
      _prio(prio),
      _event(EventWrapper::Create()),
      _name(),
      _setThreadName(false),
#if (defined(WEBRTC_LINUX) || defined(WEBRTC_ANDROID))
      _pid(-1),
#endif
      _attr(),
      _thread(0)
{
    if (threadName != NULL)
    {
        _setThreadName = true;
        strncpy(_name, threadName, kThreadMaxNameLength);
        _name[kThreadMaxNameLength - 1] = '\0';
    }
}

uint32_t ThreadWrapper::GetThreadId() {
#if defined(WEBRTC_ANDROID) || defined(WEBRTC_LINUX)
  return static_cast<uint32_t>(syscall(__NR_gettid));
#elif defined(WEBRTC_MAC)
  return static_cast<uint32_t>(mach_thread_self());
#else
  return reinterpret_cast<uint32_t>(pthread_self());
#endif
}

int ThreadPosix::Construct()
{
    int result = 0;
#if !defined(WEBRTC_ANDROID)
    // Enable immediate cancellation if requested, see Shutdown()
    result = pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    if (result != 0)
    {
        return -1;
    }
    result = pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
    if (result != 0)
    {
        return -1;
    }
#endif
    result = pthread_attr_init(&_attr);
    if (result != 0)
    {
        return -1;
    }
    return 0;
}

ThreadPosix::~ThreadPosix()
{
    pthread_attr_destroy(&_attr);
    delete _event;
}

#define HAS_THREAD_ID !defined(MAC_IPHONE) && !defined(MAC_IPHONE_SIM)  &&  \
                      !defined(WEBRTC_MAC) && !defined(WEBRTC_MAC_INTEL) && \
                      !defined(MAC_DYLIB)  && !defined(MAC_INTEL_DYLIB)
#if HAS_THREAD_ID
bool ThreadPosix::Start(unsigned int& threadID)
#else
bool ThreadPosix::Start(unsigned int& /*threadID*/)
#endif
{
    if (!_runFunction)
    {
        return false;
    }
    int result = pthread_attr_setdetachstate(&_attr, PTHREAD_CREATE_DETACHED);
    // Set the stack stack size to 1M.
    result |= pthread_attr_setstacksize(&_attr, 1024*1024);
#ifdef WEBRTC_THREAD_RR
    const int policy = SCHED_RR;
#else
    const int policy = SCHED_FIFO;
#endif
    _event->Reset();
    result |= pthread_create(&_thread, &_attr, &StartThread, this);
    if (result != 0)
    {
        return false;
    }

    // Wait up to 10 seconds for the OS to call the callback function. Prevents
    // race condition if Stop() is called too quickly after start.
    if (kEventSignaled != _event->Wait(WEBRTC_EVENT_10_SEC))
    {
        // Timed out. Something went wrong.
        _runFunction = NULL;
        return false;
    }

#if HAS_THREAD_ID
    threadID = static_cast<unsigned int>(_thread);
#endif
    sched_param param;

    const int minPrio = sched_get_priority_min(policy);
    const int maxPrio = sched_get_priority_max(policy);
    if ((minPrio == EINVAL) || (maxPrio == EINVAL))
    {
        return false;
    }

    switch (_prio)
    {
    case kLowPriority:
        param.sched_priority = minPrio + 1;
        break;
    case kNormalPriority:
        param.sched_priority = (minPrio + maxPrio) / 2;
        break;
    case kHighPriority:
        param.sched_priority = maxPrio - 3;
        break;
    case kHighestPriority:
        param.sched_priority = maxPrio - 2;
        break;
    case kRealtimePriority:
        param.sched_priority = maxPrio - 1;
        break;
    }
    result = pthread_setschedparam(_thread, policy, &param);
    if (result == EINVAL)
    {
        return false;
    }
    return true;
}

// CPU_ZERO and CPU_SET are not available in NDK r7, so disable
// SetAffinity on Android for now.
#if (defined(WEBRTC_LINUX) && (!defined(WEBRTC_ANDROID)))
bool ThreadPosix::SetAffinity(const int* processorNumbers,
                              const unsigned int amountOfProcessors) {
  if (!processorNumbers || (amountOfProcessors == 0)) {
    return false;
  }
  cpu_set_t mask;
  CPU_ZERO(&mask);

  for (unsigned int processor = 0;
      processor < amountOfProcessors;
      processor++) {
    CPU_SET(processorNumbers[processor], &mask);
  }
#if defined(WEBRTC_ANDROID)
  // Android.
  const int result = syscall(__NR_sched_setaffinity,
                             _pid,
                             sizeof(mask),
                             &mask);
#else
  // "Normal" Linux.
  const int result = sched_setaffinity(_pid,
                                       sizeof(mask),
                                       &mask);
#endif
  if (result != 0) {
    return false;
  }
  return true;
}

#else
// NOTE: On Mac OS X, use the Thread affinity API in
// /usr/include/mach/thread_policy.h: thread_policy_set and mach_thread_self()
// instead of Linux gettid() syscall.
bool ThreadPosix::SetAffinity(const int* , const unsigned int)
{
    return false;
}
#endif

void ThreadPosix::SetNotAlive()
{
    _alive = false;
}

bool ThreadPosix::Shutdown()
{
#if !defined(WEBRTC_ANDROID)
    if (_thread && (0 != pthread_cancel(_thread)))
    {
        return false;
    }

    return true;
#else
    return false;
#endif
}

bool ThreadPosix::Stop()
{
    _alive = false;

    // TODO (hellner) why not use an event here?
    // Wait up to 10 seconds for the thread to terminate
    for (int i = 0; i < 1000 && !_dead; i++)
    {
        timespec t;
        t.tv_sec = 0;
        t.tv_nsec = 10*1000*1000;
        nanosleep(&t, NULL);
    }
    if (_dead)
    {
        return true;
    }
    else
    {
        return false;
    }
}

void ThreadPosix::Run()
{
    _alive = true;
    _dead  = false;
#if (defined(WEBRTC_LINUX) || defined(WEBRTC_ANDROID))
    _pid = GetThreadId();
#endif
    // The event the Start() is waiting for.
    _event->Set();

    if (_setThreadName)
    {
#ifdef WEBRTC_LINUX
        prctl(PR_SET_NAME, (unsigned long)_name, 0, 0, 0);
#endif
        WEBRTC_TRACE(kTraceStateInfo, kTraceUtility,-1,
                     "Thread with name:%s started ", _name);
    } else
    {
        WEBRTC_TRACE(kTraceStateInfo, kTraceUtility, -1,
                     "Thread without name started");
    }
    do
    {
        if (_runFunction)
        {
            if (!_runFunction(_obj))
            {
                _alive = false;
            }
        }
        else
        {
            _alive = false;
        }
    }
    while (_alive);

    if (_setThreadName)
    {
        // Don't set the name for the trace thread because it may cause a
        // deadlock. TODO (hellner) there should be a better solution than
        // coupling the thread and the trace class like this.
        if (strcmp(_name, "Trace"))
        {
            WEBRTC_TRACE(kTraceStateInfo, kTraceUtility,-1,
                         "Thread with name:%s stopped", _name);
        }
    }
    else
    {
        WEBRTC_TRACE(kTraceStateInfo, kTraceUtility,-1,
                     "Thread without name stopped");
    }
    _dead = true;
}
} // namespace webrtc
