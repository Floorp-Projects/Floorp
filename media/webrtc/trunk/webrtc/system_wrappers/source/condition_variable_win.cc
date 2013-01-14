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

bool ConditionVariableWindows::win_support_condition_variables_primitive_ =
    false;
static HMODULE library = NULL;

PInitializeConditionVariable  PInitializeConditionVariable_;
PSleepConditionVariableCS     PSleepConditionVariableCS_;
PWakeConditionVariable        PWakeConditionVariable_;
PWakeAllConditionVariable     PWakeAllConditionVariable_;

typedef void (WINAPI *PInitializeConditionVariable)(PCONDITION_VARIABLE);
typedef BOOL (WINAPI *PSleepConditionVariableCS)(PCONDITION_VARIABLE,
                                                 PCRITICAL_SECTION, DWORD);
typedef void (WINAPI *PWakeConditionVariable)(PCONDITION_VARIABLE);
typedef void (WINAPI *PWakeAllConditionVariable)(PCONDITION_VARIABLE);

ConditionVariableWindows::ConditionVariableWindows()
    : eventID_(WAKEALL_0) {
  if (!library) {
    // Use native implementation if supported (i.e Vista+)
    library = LoadLibrary(TEXT("Kernel32.dll"));
    if (library) {
      WEBRTC_TRACE(kTraceStateInfo, kTraceUtility, -1, "Loaded Kernel.dll");

      PInitializeConditionVariable_ =
          (PInitializeConditionVariable) GetProcAddress(
              library, "InitializeConditionVariable");
      PSleepConditionVariableCS_ = (PSleepConditionVariableCS) GetProcAddress(
          library, "SleepConditionVariableCS");
      PWakeConditionVariable_ = (PWakeConditionVariable) GetProcAddress(
          library, "WakeConditionVariable");
      PWakeAllConditionVariable_ = (PWakeAllConditionVariable) GetProcAddress(
          library, "WakeAllConditionVariable");

      if (PInitializeConditionVariable_ && PSleepConditionVariableCS_
          && PWakeConditionVariable_ && PWakeAllConditionVariable_) {
        WEBRTC_TRACE(
            kTraceStateInfo, kTraceUtility, -1,
            "Loaded native condition variables");
        win_support_condition_variables_primitive_ = true;
      }
    }
  }

  if (win_support_condition_variables_primitive_) {
    PInitializeConditionVariable_(&condition_variable_);

    events_[WAKEALL_0] = NULL;
    events_[WAKEALL_1] = NULL;
    events_[WAKE] = NULL;

  } else {
    memset(&num_waiters_[0], 0, sizeof(num_waiters_));

    InitializeCriticalSection(&num_waiters_crit_sect_);

    events_[WAKEALL_0] = CreateEvent(NULL,  // no security attributes
                                     TRUE,  // manual-reset, sticky event
                                     FALSE,  // initial state non-signaled
                                     NULL);  // no name for event

    events_[WAKEALL_1] = CreateEvent(NULL,  // no security attributes
                                     TRUE,  // manual-reset, sticky event
                                     FALSE,  // initial state non-signaled
                                     NULL);  // no name for event

    events_[WAKE] = CreateEvent(NULL,  // no security attributes
                                FALSE,  // auto-reset, sticky event
                                FALSE,  // initial state non-signaled
                                NULL);  // no name for event
  }
}

ConditionVariableWindows::~ConditionVariableWindows() {
  if (!win_support_condition_variables_primitive_) {
    CloseHandle(events_[WAKE]);
    CloseHandle(events_[WAKEALL_1]);
    CloseHandle(events_[WAKEALL_0]);

    DeleteCriticalSection(&num_waiters_crit_sect_);
  }
}

void ConditionVariableWindows::SleepCS(CriticalSectionWrapper& crit_sect) {
  SleepCS(crit_sect, INFINITE);
}

bool ConditionVariableWindows::SleepCS(CriticalSectionWrapper& crit_sect,
                                       unsigned long max_time_in_ms) {
  CriticalSectionWindows* cs =
      reinterpret_cast<CriticalSectionWindows*>(&crit_sect);

  if (win_support_condition_variables_primitive_) {
    BOOL ret_val = PSleepConditionVariableCS_(
        &condition_variable_, &(cs->crit), max_time_in_ms);
    return (ret_val == 0) ? false : true;
  } else {
    EnterCriticalSection(&num_waiters_crit_sect_);

    // Get the eventID for the event that will be triggered by next
    // WakeAll() call and start waiting for it.
    const EventWakeUpType eventID =
        (WAKEALL_0 == eventID_) ? WAKEALL_1 : WAKEALL_0;

    ++(num_waiters_[eventID]);
    LeaveCriticalSection(&num_waiters_crit_sect_);

    LeaveCriticalSection(&cs->crit);
    HANDLE events[2];
    events[0] = events_[WAKE];
    events[1] = events_[eventID];
    const DWORD result = WaitForMultipleObjects(2,  // Wait on 2 events.
        events, FALSE,  // Wait for either.
        max_time_in_ms);

    const bool ret_val = (result != WAIT_TIMEOUT);

    EnterCriticalSection(&num_waiters_crit_sect_);
    --(num_waiters_[eventID]);

    // Last waiter should only be true for WakeAll(). WakeAll() correspond
    // to position 1 in events[] -> (result == WAIT_OBJECT_0 + 1)
    const bool last_waiter = (result == WAIT_OBJECT_0 + 1)
        && (num_waiters_[eventID] == 0);
    LeaveCriticalSection(&num_waiters_crit_sect_);

    if (last_waiter) {
      // Reset/unset the WakeAll() event since all threads have been
      // released.
      ResetEvent(events_[eventID]);
    }

    EnterCriticalSection(&cs->crit);
    return ret_val;
  }
}

void ConditionVariableWindows::Wake() {
  if (win_support_condition_variables_primitive_) {
    PWakeConditionVariable_(&condition_variable_);
  } else {
    EnterCriticalSection(&num_waiters_crit_sect_);
    const bool have_waiters = (num_waiters_[WAKEALL_0] > 0)
        || (num_waiters_[WAKEALL_1] > 0);
    LeaveCriticalSection(&num_waiters_crit_sect_);

    if (have_waiters) {
      SetEvent(events_[WAKE]);
    }
  }
}

void ConditionVariableWindows::WakeAll() {
  if (win_support_condition_variables_primitive_) {
    PWakeAllConditionVariable_(&condition_variable_);
  } else {
    EnterCriticalSection(&num_waiters_crit_sect_);

    // Update current WakeAll() event
    eventID_ = (WAKEALL_0 == eventID_) ? WAKEALL_1 : WAKEALL_0;

    // Trigger current event
    const EventWakeUpType eventID = eventID_;
    const bool have_waiters = num_waiters_[eventID] > 0;
    LeaveCriticalSection(&num_waiters_crit_sect_);

    if (have_waiters) {
      SetEvent(events_[eventID]);
    }
  }
}

} // namespace webrtc
