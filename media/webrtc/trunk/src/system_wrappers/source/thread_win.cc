/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "thread_win.h"

#include <assert.h>
#include <process.h>
#include <stdio.h>
#include <windows.h>

#include "set_thread_name_win.h"
#include "trace.h"

namespace webrtc {
ThreadWindows::ThreadWindows(ThreadRunFunction func, ThreadObj obj,
                             ThreadPriority prio, const char* threadName)
    : ThreadWrapper(),
      _runFunction(func),
      _obj(obj),
      _alive(false),
      _dead(true),
      _doNotCloseHandle(false),
      _prio(prio),
      _event(NULL),
      _thread(NULL),
      _id(0),
      _name(),
      _setThreadName(false)
{
    _event = EventWrapper::Create();
    _critsectStop = CriticalSectionWrapper::CreateCriticalSection();
    if (threadName != NULL)
    {
        // Set the thread name to appear in the VS debugger.
        _setThreadName = true;
        strncpy(_name, threadName, kThreadMaxNameLength);
    }
}

ThreadWindows::~ThreadWindows()
{
#ifdef _DEBUG
    assert(!_alive);
#endif
    if (_thread)
    {
        CloseHandle(_thread);
    }
    if(_event)
    {
        delete _event;
    }
    if(_critsectStop)
    {
        delete _critsectStop;
    }
}

uint32_t ThreadWrapper::GetThreadId() {
  return GetCurrentThreadId();
}

unsigned int WINAPI ThreadWindows::StartThread(LPVOID lpParameter)
{
    static_cast<ThreadWindows*>(lpParameter)->Run();
    return 0;
}

bool ThreadWindows::Start(unsigned int& threadID)
{
    _doNotCloseHandle = false;

    // Set stack size to 1M
    _thread=(HANDLE)_beginthreadex(NULL, 1024*1024, StartThread, (void*)this, 0,
                                   &threadID);
    if(_thread == NULL)
    {
        return false;
    }
    _id = threadID;
    _event->Wait(INFINITE);

    switch(_prio)
    {
    case kLowPriority:
        SetThreadPriority(_thread, THREAD_PRIORITY_BELOW_NORMAL);
        break;
    case kNormalPriority:
        SetThreadPriority(_thread, THREAD_PRIORITY_NORMAL);
        break;
    case kHighPriority:
        SetThreadPriority(_thread, THREAD_PRIORITY_ABOVE_NORMAL);
        break;
    case kHighestPriority:
        SetThreadPriority(_thread, THREAD_PRIORITY_HIGHEST);
        break;
    case kRealtimePriority:
        SetThreadPriority(_thread, THREAD_PRIORITY_TIME_CRITICAL);
        break;
    };
    return true;
}

bool ThreadWindows::SetAffinity(const int* processorNumbers,
                                const unsigned int amountOfProcessors)
{
    DWORD_PTR processorBitMask = 0;
    for(unsigned int processorIndex = 0;
        processorIndex < amountOfProcessors;
        processorIndex++)
    {
        // Convert from an array with processor numbers to a bitmask
        // Processor numbers start at zero.
        // TODO (hellner): this looks like a bug. Shouldn't the '=' be a '+='?
        // Or even better |=
        processorBitMask = 1 << processorNumbers[processorIndex];
    }
    return SetThreadAffinityMask(_thread,processorBitMask) != 0;
}

void ThreadWindows::SetNotAlive()
{
    _alive = false;
}

bool ThreadWindows::Shutdown()
{
    DWORD exitCode = 0;
    BOOL ret = TRUE;
    if (_thread)
    {
        ret = TerminateThread(_thread, exitCode);
        _alive = false;
        _dead = true;
        _thread = NULL;
    }
    return ret == TRUE;
}

bool ThreadWindows::Stop()
{
    _critsectStop->Enter();
    // Prevents the handle from being closed in ThreadWindows::Run()
    _doNotCloseHandle = true;
    _alive = false;
    bool signaled = false;
    if (_thread && !_dead)
    {
        _critsectStop->Leave();
        // Wait up to 2 seconds for the thread to complete.
        if( WAIT_OBJECT_0 == WaitForSingleObject(_thread, 2000))
        {
            signaled = true;
        }
        _critsectStop->Enter();
    }
    if (_thread)
    {
        CloseHandle(_thread);
        _thread = NULL;
    }
    _critsectStop->Leave();

    if (_dead || signaled)
    {
        return true;
    }
    else
    {
        return false;
    }
}

void ThreadWindows::Run()
{
    _alive = true;
    _dead = false;
    _event->Set();

    // All tracing must be after _event->Set to avoid deadlock in Trace.
    if (_setThreadName)
    {
        WEBRTC_TRACE(kTraceStateInfo, kTraceUtility, _id,
                     "Thread with name:%s started ", _name);
        SetThreadName(-1, _name); // -1, set thread name for the calling thread.
    }else
    {
        WEBRTC_TRACE(kTraceStateInfo, kTraceUtility, _id,
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
        } else {
            _alive = false;
        }
    } while(_alive);

    if (_setThreadName)
    {
        WEBRTC_TRACE(kTraceStateInfo, kTraceUtility, _id,
                     "Thread with name:%s stopped", _name);
    } else {
        WEBRTC_TRACE(kTraceStateInfo, kTraceUtility,_id,
                     "Thread without name stopped");
    }

    _critsectStop->Enter();

    if (_thread && !_doNotCloseHandle)
    {
        HANDLE thread = _thread;
        _thread = NULL;
        CloseHandle(thread);
    }
    _dead = true;

    _critsectStop->Leave();
};
} // namespace webrtc
