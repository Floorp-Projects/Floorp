/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_SYSTEM_WRAPPERS_SOURCE_CONDITION_VARIABLE_WIN_H_
#define WEBRTC_SYSTEM_WRAPPERS_SOURCE_CONDITION_VARIABLE_WIN_H_

#include <windows.h>

#include "condition_variable_wrapper.h"

namespace webrtc {

#if !defined CONDITION_VARIABLE_INIT
typedef struct RTL_CONDITION_VARIABLE_ {
  void* Ptr;
} RTL_CONDITION_VARIABLE, *PRTL_CONDITION_VARIABLE;

typedef RTL_CONDITION_VARIABLE CONDITION_VARIABLE, *PCONDITION_VARIABLE;
#endif

typedef void (WINAPI* PInitializeConditionVariable)(PCONDITION_VARIABLE);
typedef BOOL (WINAPI* PSleepConditionVariableCS)(PCONDITION_VARIABLE,
                                                 PCRITICAL_SECTION, DWORD);
typedef void (WINAPI* PWakeConditionVariable)(PCONDITION_VARIABLE);
typedef void (WINAPI* PWakeAllConditionVariable)(PCONDITION_VARIABLE);

class ConditionVariableWindows : public ConditionVariableWrapper {
 public:
  ConditionVariableWindows();
  ~ConditionVariableWindows();

  void SleepCS(CriticalSectionWrapper& crit_sect);
  bool SleepCS(CriticalSectionWrapper& crit_sect, unsigned long max_time_inMS);
  void Wake();
  void WakeAll();

 private:
  enum EventWakeUpType {
    WAKEALL_0   = 0,
    WAKEALL_1   = 1,
    WAKE        = 2,
    EVENT_COUNT = 3
  };

 private:
  // Native support for Windows Vista+
  static bool              win_support_condition_variables_primitive_;
  CONDITION_VARIABLE       condition_variable_;

  unsigned int     num_waiters_[2];
  EventWakeUpType  eventID_;
  CRITICAL_SECTION num_waiters_crit_sect_;
  HANDLE           events_[EVENT_COUNT];
};

} // namespace webrtc

#endif  // WEBRTC_SYSTEM_WRAPPERS_SOURCE_CONDITION_VARIABLE_WIN_H_
