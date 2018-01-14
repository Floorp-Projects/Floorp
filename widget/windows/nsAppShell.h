/* -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsAppShell_h__
#define nsAppShell_h__

#include "nsBaseAppShell.h"
#include <windows.h>
#include <vector>
#include "mozilla/TimeStamp.h"
#include "mozilla/Mutex.h"

// The maximum time we allow before forcing a native event callback.
// In seconds.
#define NATIVE_EVENT_STARVATION_LIMIT 1

/**
 * Native Win32 Application shell wrapper
 */
class nsAppShell : public nsBaseAppShell
{
public:
  nsAppShell() :
    mEventWnd(nullptr),
    mNativeCallbackPending(false),
    mLastNativeEventScheduledMutex("nsAppShell::mLastNativeEventScheduledMutex")
  {}
  typedef mozilla::TimeStamp TimeStamp;
  typedef mozilla::Mutex Mutex;

  nsresult Init();
  void DoProcessMoreGeckoEvents();

  static UINT GetTaskbarButtonCreatedMessage();

  NS_IMETHOD AfterProcessNextEvent(nsIThreadInternal* thread,
                                   bool eventWasProcessed) final override;

protected:
  NS_IMETHOD Run() override;
  NS_IMETHOD Exit() override;

#if defined(ACCESSIBILITY)
  NS_IMETHOD Observe(nsISupports* aSubject, const char* aTopic,
                     const char16_t* aData) override;
#endif // defined(ACCESSIBILITY)

  virtual void ScheduleNativeEventCallback();
  virtual bool ProcessNextNativeEvent(bool mayWait);
  virtual ~nsAppShell();

  static LRESULT CALLBACK EventWindowProc(HWND, UINT, WPARAM, LPARAM);

protected:
  HWND mEventWnd;
  bool mNativeCallbackPending;

  Mutex mLastNativeEventScheduledMutex;
  TimeStamp mLastNativeEventScheduled;
  std::vector<MSG> mMsgsToRepost;
};

#endif // nsAppShell_h__
