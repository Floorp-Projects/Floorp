// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0.If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <windows.h>
#include "exdll.h"

static void APIENTRY TimerCallback(LPVOID NSISFunctionAddr, DWORD, DWORD) {
  g_executeCodeSegment((int)NSISFunctionAddr, nullptr);
}

PLUGINFUNCTION(CreateTimer) {
  EXDLL_INIT();
  extra->RegisterPluginCallback(gHInst, NSISPluginCallback);

  TCHAR* funcAddrStr =
      (TCHAR*)HeapAlloc(GetProcessHeap(), 0, g_stringsize * sizeof(TCHAR));
  popstring(funcAddrStr);
  // Apparently GetFunctionAddress returnes a 1-indexed offset, but
  // ExecuteCodeSegment expects a 0-indexed one. Or something.
  uintptr_t funcAddr = static_cast<uintptr_t>(_ttoi64(funcAddrStr)) - 1;
  HeapFree(GetProcessHeap(), 0, funcAddrStr);

  TCHAR* intervalStr =
      (TCHAR*)HeapAlloc(GetProcessHeap(), 0, g_stringsize * sizeof(TCHAR));
  popstring(intervalStr);
  long interval = _ttol(intervalStr);
  HeapFree(GetProcessHeap(), 0, intervalStr);

  HANDLE timer = CreateWaitableTimer(nullptr, FALSE, nullptr);
  if (!timer) {
    return;
  }

  // The interval we were passed is in milliseconds, so we need to convert it
  // to the 100-nanosecond units that SetWaitableTimer expects. We also need to
  // make it negative, because that's what signifies a relative offset from now
  // instead of an epoch-based time stamp.
  LARGE_INTEGER dueTime;
  dueTime.QuadPart = -(interval * 10000);

  SetWaitableTimer(timer, &dueTime, interval, TimerCallback, (void*)funcAddr,
                   FALSE);

  TCHAR timerStr[32] = _T("");
  _ltot_s((long)timer, timerStr, 10);
  pushstring(timerStr);
}

PLUGINFUNCTION(CancelTimer) {
  TCHAR* timerStr =
      (TCHAR*)HeapAlloc(GetProcessHeap(), 0, g_stringsize * sizeof(TCHAR));
  popstring(timerStr);
  HANDLE timer = reinterpret_cast<HANDLE>(_ttoi(timerStr));
  HeapFree(GetProcessHeap(), 0, timerStr);

  CancelWaitableTimer(timer);
  CloseHandle(timer);
}
