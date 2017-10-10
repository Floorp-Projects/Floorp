/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsCrashOnException.h"
#include "nsCOMPtr.h"
#include "nsICrashReporter.h"
#include "nsServiceManagerUtils.h"


namespace mozilla {

static int
ReportException(EXCEPTION_POINTERS* aExceptionInfo)
{
  nsCOMPtr<nsICrashReporter> cr =
    do_GetService("@mozilla.org/toolkit/crash-reporter;1");
  if (cr) {
    cr->WriteMinidumpForException(aExceptionInfo);
  }

  return EXCEPTION_EXECUTE_HANDLER;
}

XPCOM_API(LRESULT)
CallWindowProcCrashProtected(WNDPROC aWndProc, HWND aHWnd, UINT aMsg,
                             WPARAM aWParam, LPARAM aLParam)
{
  MOZ_SEH_TRY {
    return aWndProc(aHWnd, aMsg, aWParam, aLParam);
  }
  MOZ_SEH_EXCEPT(ReportException(GetExceptionInformation())) {
    ::TerminateProcess(::GetCurrentProcess(), 253);
  }
  return 0; // not reached
}

}

