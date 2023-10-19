/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/WindowsMsctfInitialization.h"

#include <windows.h>

#include "mozilla/NativeNt.h"
#include "mozilla/WindowsVersion.h"
#include "nsWindowsDllInterceptor.h"

namespace mozilla {

#if defined(_M_IX86) || defined(_M_X64)

// Starting with Windows 11 22H2 (10.0.22621.*), msctf.dll uses a new
// convention for the lParam argument of TF_Notify when uMsg is 0x20000. It now
// expects a pointer to a structure similar to LPARAM20000 described below,
// where a scalar value was used before. We convert the value forwarded by
// ZoneAlarm Anti-Keylogger to the new convention, if we detect that it is
// still using the old convention (bug 1777960).

struct LPARAM20000 {
  uintptr_t Reserved1;  // Not used
  LPARAM LegacyValue;   // This value used to be sent as lParam directly
  uintptr_t Reserved2;  // Used as a boolean (though never saw it set to true)
};

static WindowsDllInterceptor MsctfIntercept;

typedef uintptr_t(WINAPI* TF_Notify_func)(UINT uMsg, WPARAM wParam,
                                          LPARAM lParam);
static WindowsDllInterceptor::FuncHookType<TF_Notify_func> stub_TF_Notify;

uintptr_t WINAPI patched_TF_Notify(UINT uMsg, WPARAM wParam, LPARAM lParam) {
  // Only convert to the new convention if we detect a problem with the lParam
  if (uMsg == 0x20000 &&
      IsBadReadPtr(reinterpret_cast<void*>(lParam), sizeof(LPARAM20000))) {
    // Using a pointer to stack as lParam is fine here: when observing calls
    // that originate from Microsoft code, pointers to stack are used as well
    LPARAM20000 lParamWithNewConvention{
        .Reserved1 = 0,
        .LegacyValue = lParam,
        .Reserved2 = 0,
    };
    return stub_TF_Notify(uMsg, wParam,
                          reinterpret_cast<LPARAM>(&lParamWithNewConvention));
  }
  return stub_TF_Notify(uMsg, wParam, lParam);
}

bool WindowsMsctfInitialization() {
  // Only proceed if we detect ZoneAlarm Anti-Keylogger (bug 1777960)
  HMODULE icsak = ::GetModuleHandleW(L"icsak.dll");
  if (!icsak) {
    return true;
  }

  // Only proceed if msctf.dll uses the new lParam convention
  if (!IsWin1122H2OrLater()) {
    return true;
  }

  // Only proceed if icsak.dll is in version 1.5.393.2181 or older
  nt::PEHeaders icsakHeaders{icsak};
  uint64_t icsakVersion{};
  if (!icsakHeaders || !icsakHeaders.GetVersionInfo(icsakVersion) ||
      icsakVersion > 0x0001000501890885) {
    return true;
  }

  // Apply our hook to fix messages using the old lParam convention
  MsctfIntercept.Init(L"msctf.dll");
  return stub_TF_Notify.Set(MsctfIntercept, "TF_Notify", &patched_TF_Notify);
}

#endif  // _M_IX86 || _M_X64

}  // namespace mozilla
