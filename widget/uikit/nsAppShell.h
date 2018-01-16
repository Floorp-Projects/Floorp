/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Runs the main native UIKit run loop, interrupting it as needed to process
 * Gecko events.
 */

#ifndef nsAppShell_h_
#define nsAppShell_h_

#include "nsBaseAppShell.h"
#include "nsTArray.h"

#include <Foundation/NSAutoreleasePool.h>
#include <CoreFoundation/CFRunLoop.h>
#include <UIKit/UIWindow.h>

@class AppShellDelegate;

class nsAppShell : public nsBaseAppShell
{
public:
  NS_IMETHOD ResumeNative(void) override;

  nsAppShell();

  nsresult Init();

  NS_IMETHOD Run(void) override;
  NS_IMETHOD Exit(void) override;
  // Called by the application delegate
  void WillTerminate(void);

  static nsAppShell* gAppShell;
  static UIWindow* gWindow;
  static NSMutableArray* gTopLevelViews;

protected:
  virtual ~nsAppShell();

  static void ProcessGeckoEvents(void* aInfo);
  virtual void ScheduleNativeEventCallback();
  virtual bool ProcessNextNativeEvent(bool aMayWait);

  NSAutoreleasePool* mAutoreleasePool;
  AppShellDelegate*  mDelegate;
  CFRunLoopRef       mCFRunLoop;
  CFRunLoopSourceRef mCFRunLoopSource;

  bool               mTerminated;
  bool               mNotifiedWillTerminate;
};

#endif // nsAppShell_h_
