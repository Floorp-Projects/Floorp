/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rw_lock_win.h"

#include "critical_section_wrapper.h"
#include "condition_variable_wrapper.h"
#include "trace.h"

// TODO (hellner) why not just use the rw_lock_generic.cc solution if
//                           native is not supported? Unnecessary redundancy!

namespace webrtc {
bool RWLockWindows::_winSupportRWLockPrimitive = false;
static HMODULE library = NULL;

PInitializeSRWLock       _PInitializeSRWLock;
PAcquireSRWLockExclusive _PAcquireSRWLockExclusive;
PAcquireSRWLockShared    _PAcquireSRWLockShared;
PReleaseSRWLockShared    _PReleaseSRWLockShared;
PReleaseSRWLockExclusive _PReleaseSRWLockExclusive;

RWLockWindows::RWLockWindows()
    : _critSectPtr(NULL),
      _readCondPtr(NULL),
      _writeCondPtr(NULL),
      _readersActive(0),
      _writerActive(false),
      _readersWaiting(0),
      _writersWaiting(0)
{
}

RWLockWindows::~RWLockWindows()
{
    delete _writeCondPtr;
    delete _readCondPtr;
    delete _critSectPtr;
}

int RWLockWindows::Init()
{
    if(!library)
    {
        // Use native implementation if supported (i.e Vista+)
        library = LoadLibrary(TEXT("Kernel32.dll"));
        if(library)
        {
            WEBRTC_TRACE(kTraceStateInfo, kTraceUtility, -1,
                         "Loaded Kernel.dll");

            _PInitializeSRWLock =
                (PInitializeSRWLock)GetProcAddress(
                    library,
                    "InitializeSRWLock");

            _PAcquireSRWLockExclusive =
               (PAcquireSRWLockExclusive)GetProcAddress(
                   library,
                   "AcquireSRWLockExclusive");
            _PReleaseSRWLockExclusive =
                (PReleaseSRWLockExclusive)GetProcAddress(
                    library,
                    "ReleaseSRWLockExclusive");
            _PAcquireSRWLockShared =
                (PAcquireSRWLockShared)GetProcAddress(
                    library,
                    "AcquireSRWLockShared");
            _PReleaseSRWLockShared =
                (PReleaseSRWLockShared)GetProcAddress(
                    library,
                    "ReleaseSRWLockShared");

            if( _PInitializeSRWLock &&
                _PAcquireSRWLockExclusive &&
                _PReleaseSRWLockExclusive &&
                _PAcquireSRWLockShared &&
                _PReleaseSRWLockShared )
            {
                WEBRTC_TRACE(kTraceStateInfo, kTraceUtility, -1,
                            "Loaded Simple RW Lock");
                _winSupportRWLockPrimitive = true;
            }
        }
    }
    if(_winSupportRWLockPrimitive)
    {
        _PInitializeSRWLock(&_lock);
    } else {
        _critSectPtr  = CriticalSectionWrapper::CreateCriticalSection();
        _readCondPtr  = ConditionVariableWrapper::CreateConditionVariable();
        _writeCondPtr = ConditionVariableWrapper::CreateConditionVariable();
    }
    return 0;
}

void RWLockWindows::AcquireLockExclusive()
{
    if (_winSupportRWLockPrimitive)
    {
        _PAcquireSRWLockExclusive(&_lock);
    } else {
        _critSectPtr->Enter();

        if (_writerActive || _readersActive > 0)
        {
            ++_writersWaiting;
            while (_writerActive || _readersActive > 0)
            {
                _writeCondPtr->SleepCS(*_critSectPtr);
            }
            --_writersWaiting;
        }
        _writerActive = true;
        _critSectPtr->Leave();
    }
}

void RWLockWindows::ReleaseLockExclusive()
{
    if(_winSupportRWLockPrimitive)
    {
        _PReleaseSRWLockExclusive(&_lock);
    } else {
        _critSectPtr->Enter();
        _writerActive = false;
        if (_writersWaiting > 0)
        {
            _writeCondPtr->Wake();

        }else if (_readersWaiting > 0) {
            _readCondPtr->WakeAll();
        }
        _critSectPtr->Leave();
    }
}

void RWLockWindows::AcquireLockShared()
{
    if(_winSupportRWLockPrimitive)
    {
        _PAcquireSRWLockShared(&_lock);
    } else
    {
        _critSectPtr->Enter();
        if (_writerActive || _writersWaiting > 0)
        {
            ++_readersWaiting;

            while (_writerActive || _writersWaiting > 0)
            {
                _readCondPtr->SleepCS(*_critSectPtr);
            }
            --_readersWaiting;
        }
        ++_readersActive;
        _critSectPtr->Leave();
    }
}

void RWLockWindows::ReleaseLockShared()
{
    if(_winSupportRWLockPrimitive)
    {
        _PReleaseSRWLockShared(&_lock);
    } else
    {
        _critSectPtr->Enter();

        --_readersActive;

        if (_readersActive == 0 && _writersWaiting > 0)
        {
            _writeCondPtr->Wake();
        }
        _critSectPtr->Leave();
    }
}
} // namespace webrtc
