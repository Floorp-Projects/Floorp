/* -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsAppShell_h__
#define nsAppShell_h__

#include "nsBaseAppShell.h"
#include <windows.h>
#include "mozilla/TimeStamp.h"

/**
 * Native Win32 Application shell wrapper
 */
class nsAppShell : public nsBaseAppShell
{
public:
  nsAppShell() :
    mEventWnd(NULL),
    mNativeCallbackPending(false)
  {}
  typedef mozilla::TimeStamp TimeStamp;

  nsresult Init();
  void DoProcessMoreGeckoEvents();

  static UINT GetTaskbarButtonCreatedMessage();

protected:
#if defined(_MSC_VER) && defined(_M_IX86)
  NS_IMETHOD Run();
#endif
  virtual void ScheduleNativeEventCallback();
  virtual bool ProcessNextNativeEvent(bool mayWait);
  virtual ~nsAppShell();

  static LRESULT CALLBACK EventWindowProc(HWND, UINT, WPARAM, LPARAM);

protected:
  HWND mEventWnd;
  bool mNativeCallbackPending;
  TimeStamp mLastNativeEventScheduled;
};

#endif // nsAppShell_h__
