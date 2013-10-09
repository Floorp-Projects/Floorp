/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#pragma once

#include "nsBaseAppShell.h"
#include <windows.h>
#include "nsWindowsHelpers.h"
#include "nsIObserver.h"

class MetroAppShell : public nsBaseAppShell
{
public:
  NS_DECL_NSIOBSERVER

  MetroAppShell() :
    mEventWnd(nullptr),
    mPowerRequestCount(0)
  {
  }

  nsresult Init();
  void DoProcessMoreGeckoEvents();
  void NativeCallback();

  static LRESULT CALLBACK EventWindowProc(HWND, UINT, WPARAM, LPARAM);
  static bool ProcessOneNativeEventIfPresent();
  static void MarkEventQueueForPurge();

protected:
  NS_IMETHOD Run();

  virtual void ScheduleNativeEventCallback();
  virtual bool ProcessNextNativeEvent(bool mayWait);
  static void DispatchAllGeckoEvents();
  virtual ~MetroAppShell();

  HWND mEventWnd;
  nsAutoHandle mPowerRequest;
  ULONG mPowerRequestCount;
};
