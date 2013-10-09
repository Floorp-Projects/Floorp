/* -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsAppShell_h__
#define nsAppShell_h__

#include "nsBaseAppShell.h"
#define INCL_DEV
#define INCL_WIN
#define INCL_DOS
#include <os2.h>

/**
 * Native OS/2 Application shell wrapper
 */

class nsAppShell : public nsBaseAppShell
{
public:
  nsAppShell() : mEventWnd(nullptr) {}

  nsresult Init();

protected:
  virtual void ScheduleNativeEventCallback();
  virtual bool ProcessNextNativeEvent(bool mayWait);
  virtual ~nsAppShell();

protected:
  HWND mEventWnd;

friend MRESULT EXPENTRY EventWindowProc(HWND, ULONG, MPARAM, MPARAM);
};


#endif // nsAppShell_h__

