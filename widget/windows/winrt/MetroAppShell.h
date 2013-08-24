/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#pragma once

#include "nsBaseAppShell.h"
#include <windows.h>
#include "mozilla/TimeStamp.h"
#include "nsWindowsHelpers.h"
#include "nsIObserver.h"

class MetroAppShell : public nsBaseAppShell
{
public:
  NS_DECL_NSIOBSERVER

  MetroAppShell() : mEventWnd(NULL), mExiting(false), mPowerRequestCount(0)
  {}

  nsresult Init();
  void DoProcessMoreGeckoEvents();
  void NativeCallback();

  static LRESULT CALLBACK EventWindowProc(HWND, UINT, WPARAM, LPARAM);
  static void ProcessAllNativeEventsPresent();
  static void ProcessOneNativeEventIfPresent();

protected:
  HWND mEventWnd;
  bool mExiting;
  nsAutoHandle mPowerRequest;
  ULONG mPowerRequestCount;

  NS_IMETHOD Run();
  NS_IMETHOD Exit();
  virtual void ScheduleNativeEventCallback();
  virtual bool ProcessNextNativeEvent(bool mayWait);
  virtual ~MetroAppShell();
};
