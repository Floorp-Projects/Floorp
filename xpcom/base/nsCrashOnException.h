/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsCrashOnException_h
#define nsCrashOnException_h

#include <nscore.h>
#include <windows.h>

namespace mozilla {

// Call a given window procedure, and catch any Win32 exceptions raised from it,
// and report them as crashes.
XPCOM_API(LRESULT) CallWindowProcCrashProtected(WNDPROC aWndProc, HWND aHWnd,
                                                UINT aMsg, WPARAM aWParam,
                                                LPARAM aLParam);

}

#endif
