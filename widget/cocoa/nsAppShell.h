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

#import <AppKit/NSApplication.h>

#include "nsBaseAppShell.h"
#include "nsTArray.h"

class ProfilingStack;

namespace mozilla {
class nsAvailableMemoryWatcherBase;
}

// GeckoNSApplication
//
// Subclass of NSApplication for filtering out certain events.
@interface GeckoNSApplication : NSApplication {
}
@property(readonly) BOOL didLaunch;
@end

@class AppShellDelegate;

class nsAppShell : public nsBaseAppShell {
 public:
  NS_IMETHOD ResumeNative(void) override;

  nsAppShell();

  nsresult Init();

  NS_IMETHOD Run(void) override;
  NS_IMETHOD Exit(void) override;
  NS_IMETHOD OnProcessNextEvent(nsIThreadInternal* aThread,
                                bool aMayWait) override;
  NS_IMETHOD AfterProcessNextEvent(nsIThreadInternal* aThread,
                                   bool aEventWasProcessed) override;

  void OnRunLoopActivityChanged(CFRunLoopActivity aActivity);

  // public only to be visible to Objective-C code that must call it
  void WillTerminate();

  static void OnMemoryPressureChanged(
      dispatch_source_memorypressure_flags_t aPressureLevel);

 protected:
  virtual ~nsAppShell();

  virtual void ScheduleNativeEventCallback() override;
  virtual bool ProcessNextNativeEvent(bool aMayWait) override;

  void InitMemoryPressureObserver();

  static void ProcessGeckoEvents(void* aInfo);

 protected:
  CFMutableArrayRef mAutoreleasePools;

  AppShellDelegate* mDelegate;
  CFRunLoopRef mCFRunLoop;
  CFRunLoopSourceRef mCFRunLoopSource;

  // An observer for the profiler that is notified when the event loop enters
  // and exits the waiting state.
  CFRunLoopObserverRef mCFRunLoopObserver;

  // Non-null while the native event loop is in the waiting state.
  ProfilingStack* mProfilingStackWhileWaiting = nullptr;

  // For getting notifications from the OS about memory pressure state changes.
  dispatch_source_t mMemoryPressureSource = nullptr;

  bool mRunningEventLoop;
  bool mStarted;
  bool mTerminated;
  bool mSkippedNativeCallback;
  bool mRunningCocoaEmbedded;

  int32_t mNativeEventCallbackDepth;
  // Can be set from different threads, so must be modified atomically
  int32_t mNativeEventScheduledDepth;
};

#endif  // nsAppShell_h_
