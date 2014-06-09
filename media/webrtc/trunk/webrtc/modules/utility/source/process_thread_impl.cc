/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/interface/module.h"
#include "webrtc/modules/utility/source/process_thread_impl.h"
#include "webrtc/system_wrappers/interface/trace.h"

namespace webrtc {
ProcessThread::~ProcessThread()
{
}

ProcessThread* ProcessThread::CreateProcessThread()
{
    return new ProcessThreadImpl();
}

void ProcessThread::DestroyProcessThread(ProcessThread* module)
{
    delete module;
}

ProcessThreadImpl::ProcessThreadImpl()
    : _timeEvent(*EventWrapper::Create()),
      _critSectModules(CriticalSectionWrapper::CreateCriticalSection()),
      _thread(NULL)
{
    WEBRTC_TRACE(kTraceMemory, kTraceUtility, -1, "%s created", __FUNCTION__);
}

ProcessThreadImpl::~ProcessThreadImpl()
{
    delete _critSectModules;
    delete &_timeEvent;
    WEBRTC_TRACE(kTraceMemory, kTraceUtility, -1, "%s deleted", __FUNCTION__);
}

int32_t ProcessThreadImpl::Start()
{
    CriticalSectionScoped lock(_critSectModules);
    if(_thread)
    {
        return -1;
    }
    _thread = ThreadWrapper::CreateThread(Run, this, kNormalPriority,
                                          "ProcessThread");
    unsigned int id;
    int32_t retVal = _thread->Start(id);
    if(retVal >= 0)
    {
        return 0;
    }
    delete _thread;
    _thread = NULL;
    return -1;
}

int32_t ProcessThreadImpl::Stop()
{
    _critSectModules->Enter();
    if(_thread)
    {
        _thread->SetNotAlive();

        ThreadWrapper* thread = _thread;
        _thread = NULL;

        _timeEvent.Set();
        _critSectModules->Leave();

        if(thread->Stop())
        {
            delete thread;
        } else {
            return -1;
        }
    } else {
        _critSectModules->Leave();
    }
    return 0;
}

int32_t ProcessThreadImpl::RegisterModule(Module* module)
{
    CriticalSectionScoped lock(_critSectModules);

    // Only allow module to be registered once.
    for (ModuleList::iterator iter = _modules.begin();
         iter != _modules.end(); ++iter) {
        if(module == *iter)
        {
            return -1;
        }
    }

    _modules.push_front(module);
    WEBRTC_TRACE(kTraceInfo, kTraceUtility, -1,
                 "number of registered modules has increased to %d",
                 _modules.size());
    // Wake the thread calling ProcessThreadImpl::Process() to update the
    // waiting time. The waiting time for the just registered module may be
    // shorter than all other registered modules.
    _timeEvent.Set();
    return 0;
}

int32_t ProcessThreadImpl::DeRegisterModule(const Module* module)
{
    CriticalSectionScoped lock(_critSectModules);
    for (ModuleList::iterator iter = _modules.begin();
         iter != _modules.end(); ++iter) {
        if(module == *iter)
        {
            _modules.erase(iter);
            WEBRTC_TRACE(kTraceInfo, kTraceUtility, -1,
                         "number of registered modules has decreased to %d",
                         _modules.size());
            return 0;
        }
    }
    return -1;
}

bool ProcessThreadImpl::Run(void* obj)
{
    return static_cast<ProcessThreadImpl*>(obj)->Process();
}

bool ProcessThreadImpl::Process()
{
    // Wait for the module that should be called next, but don't block thread
    // longer than 100 ms.
    int32_t minTimeToNext = 100;
    {
        CriticalSectionScoped lock(_critSectModules);
        for (ModuleList::iterator iter = _modules.begin();
             iter != _modules.end(); ++iter) {
          int32_t timeToNext = (*iter)->TimeUntilNextProcess();
            if(minTimeToNext > timeToNext)
            {
                minTimeToNext = timeToNext;
            }
        }
    }

    if(minTimeToNext > 0)
    {
        if(kEventError == _timeEvent.Wait(minTimeToNext))
        {
            return true;
        }
        CriticalSectionScoped lock(_critSectModules);
        if(!_thread)
        {
            return false;
        }
    }
    {
        CriticalSectionScoped lock(_critSectModules);
        for (ModuleList::iterator iter = _modules.begin();
             iter != _modules.end(); ++iter) {
          int32_t timeToNext = (*iter)->TimeUntilNextProcess();
            if(timeToNext < 1)
            {
                (*iter)->Process();
            }
        }
    }
    return true;
}
}  // namespace webrtc
