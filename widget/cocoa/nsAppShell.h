/* -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Runs the main native Cocoa run loop, interrupting it as needed to process
 * Gecko events.
 */

#ifndef nsAppShell_h_
#define nsAppShell_h_

#include "nsBaseAppShell.h"
#include "nsTArray.h"

// GeckoNSApplication
//
// Subclass of NSApplication for filtering out certain events.
@interface GeckoNSApplication : NSApplication
{
}
@end

@class AppShellDelegate;

class nsAppShell : public nsBaseAppShell
{
public:
  NS_IMETHOD ResumeNative(void) override;

  nsAppShell();

  nsresult Init();

  NS_IMETHOD Run(void) override;
  NS_IMETHOD Exit(void) override;
  NS_IMETHOD OnProcessNextEvent(nsIThreadInternal *aThread, bool aMayWait) override;
  NS_IMETHOD AfterProcessNextEvent(nsIThreadInternal *aThread,
                                   bool aEventWasProcessed) override;

  // public only to be visible to Objective-C code that must call it
  void WillTerminate();

protected:
  virtual ~nsAppShell();

  virtual void ScheduleNativeEventCallback() override;
  virtual bool ProcessNextNativeEvent(bool aMayWait) override;

  static void ProcessGeckoEvents(void* aInfo);

protected:
  CFMutableArrayRef  mAutoreleasePools;

  AppShellDelegate*  mDelegate;
  CFRunLoopRef       mCFRunLoop;
  CFRunLoopSourceRef mCFRunLoopSource;

  bool               mRunningEventLoop;
  bool               mStarted;
  bool               mTerminated;
  bool               mSkippedNativeCallback;
  bool               mRunningCocoaEmbedded;

  int32_t            mNativeEventCallbackDepth;
  // Can be set from different threads, so must be modified atomically
  int32_t            mNativeEventScheduledDepth;
};

#endif // nsAppShell_h_
