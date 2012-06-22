/*
 *  Use of this source code is governed by the ACE copyright license which
 *  can be found in the LICENSE file in the third_party_mods/ace directory of
 *  the source tree or at http://www1.cse.wustl.edu/~schmidt/ACE-copying.html.
 */
/*
 *  This source code contain modifications to the original source code
 *  which can be found here:
 *  http://www.cs.wustl.edu/~schmidt/win32-cv-1.html (section 3.2).
 *  Modifications:
 *  1) Dynamic detection of native support for condition variables.
 *  2) Use of WebRTC defined types and classes. Renaming of some functions.
 *  3) Introduction of a second event for wake all functionality. This prevents
 *     a thread from spinning on the same condition variable, preventing other
 *     threads from waking up.
 */

// TODO (hellner): probably nicer to split up native and generic
// implementation into two different files

#include "condition_variable_win.h"

#include "critical_section_win.h"
#include "trace.h"

namespace webrtc {
bool ConditionVariableWindows::_winSupportConditionVariablesPrimitive = false;
static HMODULE library = NULL;

PInitializeConditionVariable  _PInitializeConditionVariable;
PSleepConditionVariableCS     _PSleepConditionVariableCS;
PWakeConditionVariable        _PWakeConditionVariable;
PWakeAllConditionVariable     _PWakeAllConditionVariable;

typedef void (WINAPI *PInitializeConditionVariable)(PCONDITION_VARIABLE);
typedef BOOL (WINAPI *PSleepConditionVariableCS)(PCONDITION_VARIABLE,
                                                 PCRITICAL_SECTION, DWORD);
typedef void (WINAPI *PWakeConditionVariable)(PCONDITION_VARIABLE);
typedef void (WINAPI *PWakeAllConditionVariable)(PCONDITION_VARIABLE);

ConditionVariableWindows::ConditionVariableWindows()
    : _eventID(WAKEALL_0)
{
    if (!library)
    {
        // Use native implementation if supported (i.e Vista+)
        library = LoadLibrary(TEXT("Kernel32.dll"));
        if (library)
        {
            WEBRTC_TRACE(kTraceStateInfo, kTraceUtility, -1,
                         "Loaded Kernel.dll");

            _PInitializeConditionVariable =
                (PInitializeConditionVariable) GetProcAddress(
                    library,
                    "InitializeConditionVariable");
            _PSleepConditionVariableCS =
                (PSleepConditionVariableCS)GetProcAddress(
                    library,
                    "SleepConditionVariableCS");
            _PWakeConditionVariable =
                (PWakeConditionVariable)GetProcAddress(
                    library,
                     "WakeConditionVariable");
            _PWakeAllConditionVariable =
                (PWakeAllConditionVariable)GetProcAddress(
                    library,
                    "WakeAllConditionVariable");

            if(_PInitializeConditionVariable &&
               _PSleepConditionVariableCS &&
               _PWakeConditionVariable &&
               _PWakeAllConditionVariable)
            {
                WEBRTC_TRACE(kTraceStateInfo, kTraceUtility, -1,
                             "Loaded native condition variables");
                _winSupportConditionVariablesPrimitive = true;
            }
        }
    }

    if (_winSupportConditionVariablesPrimitive)
    {
        _PInitializeConditionVariable(&_conditionVariable);

        _events[WAKEALL_0] = NULL;
        _events[WAKEALL_1] = NULL;
        _events[WAKE] = NULL;

    } else {
        memset(&_numWaiters[0],0,sizeof(_numWaiters));

        InitializeCriticalSection(&_numWaitersCritSect);

        _events[WAKEALL_0] = CreateEvent(NULL,  // no security attributes
                                         TRUE,  // manual-reset, sticky event
                                         FALSE, // initial state non-signaled
                                         NULL); // no name for event

        _events[WAKEALL_1] = CreateEvent(NULL,  // no security attributes
                                         TRUE,  // manual-reset, sticky event
                                         FALSE, // initial state non-signaled
                                         NULL); // no name for event

        _events[WAKE] = CreateEvent(NULL,  // no security attributes
                                    FALSE, // auto-reset, sticky event
                                    FALSE, // initial state non-signaled
                                    NULL); // no name for event
    }
}

ConditionVariableWindows::~ConditionVariableWindows()
{
    if(!_winSupportConditionVariablesPrimitive)
    {
        CloseHandle(_events[WAKE]);
        CloseHandle(_events[WAKEALL_1]);
        CloseHandle(_events[WAKEALL_0]);

        DeleteCriticalSection(&_numWaitersCritSect);
    }
}

void ConditionVariableWindows::SleepCS(CriticalSectionWrapper& critSect)
{
    SleepCS(critSect, INFINITE);
}

bool ConditionVariableWindows::SleepCS(CriticalSectionWrapper& critSect,
                                       unsigned long maxTimeInMS)
{
    CriticalSectionWindows* cs = reinterpret_cast<CriticalSectionWindows*>(
                                     &critSect);

    if(_winSupportConditionVariablesPrimitive)
    {
        BOOL retVal = _PSleepConditionVariableCS(&_conditionVariable,
                                                 &(cs->crit),maxTimeInMS);
        return (retVal == 0) ? false : true;

    }else
    {
        EnterCriticalSection(&_numWaitersCritSect);
        // Get the eventID for the event that will be triggered by next
        // WakeAll() call and start waiting for it.
        const EventWakeUpType eventID = (WAKEALL_0 == _eventID) ?
                                            WAKEALL_1 : WAKEALL_0;
        ++(_numWaiters[eventID]);
        LeaveCriticalSection(&_numWaitersCritSect);

        LeaveCriticalSection(&cs->crit);
        HANDLE events[2];
        events[0] = _events[WAKE];
        events[1] = _events[eventID];
        const DWORD result = WaitForMultipleObjects(2, // Wait on 2 events.
                                                    events,
                                                    FALSE, // Wait for either.
                                                    maxTimeInMS);

        const bool retVal = (result != WAIT_TIMEOUT);

        EnterCriticalSection(&_numWaitersCritSect);
        --(_numWaiters[eventID]);
        // Last waiter should only be true for WakeAll(). WakeAll() correspond
        // to position 1 in events[] -> (result == WAIT_OBJECT_0 + 1)
        const bool lastWaiter = (result == WAIT_OBJECT_0 + 1) &&
                                (_numWaiters[eventID] == 0);
        LeaveCriticalSection(&_numWaitersCritSect);

        if (lastWaiter)
        {
            // Reset/unset the WakeAll() event since all threads have been
            // released.
            ResetEvent(_events[eventID]);
        }

        EnterCriticalSection(&cs->crit);
        return retVal;
    }
}

void
ConditionVariableWindows::Wake()
{
    if(_winSupportConditionVariablesPrimitive)
    {
        _PWakeConditionVariable(&_conditionVariable);
    }else
    {
        EnterCriticalSection(&_numWaitersCritSect);
        const bool haveWaiters = (_numWaiters[WAKEALL_0] > 0) ||
                                 (_numWaiters[WAKEALL_1] > 0);
        LeaveCriticalSection(&_numWaitersCritSect);

        if (haveWaiters)
        {
            SetEvent(_events[WAKE]);
        }
    }
}

void
ConditionVariableWindows::WakeAll()
{
    if(_winSupportConditionVariablesPrimitive)
    {
        _PWakeAllConditionVariable(&_conditionVariable);
    }else
    {
        EnterCriticalSection(&_numWaitersCritSect);
        // Update current WakeAll() event
        _eventID = (WAKEALL_0 == _eventID) ? WAKEALL_1 : WAKEALL_0;
        // Trigger current event
        const EventWakeUpType eventID = _eventID;
        const bool haveWaiters = _numWaiters[eventID] > 0;
        LeaveCriticalSection(&_numWaitersCritSect);

        if (haveWaiters)
        {
            SetEvent(_events[eventID]);
        }
    }
}
} // namespace webrtc
