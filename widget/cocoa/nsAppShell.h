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

class nsCocoaWindow;

#include "nsBaseAppShell.h"
#include "nsTArray.h"

typedef struct _nsCocoaAppModalWindowListItem {
  _nsCocoaAppModalWindowListItem(NSWindow *aWindow, NSModalSession aSession) :
    mWindow(aWindow), mSession(aSession), mWidget(nsnull) {}
  _nsCocoaAppModalWindowListItem(NSWindow *aWindow, nsCocoaWindow *aWidget) :
    mWindow(aWindow), mSession(nil), mWidget(aWidget) {}
  NSWindow *mWindow;       // Weak
  NSModalSession mSession; // Weak (not retainable)
  nsCocoaWindow *mWidget;  // Weak
} nsCocoaAppModalWindowListItem;

class nsCocoaAppModalWindowList {
public:
  nsCocoaAppModalWindowList() {}
  ~nsCocoaAppModalWindowList() {}
  // Push a Cocoa app-modal window onto the top of our list.
  nsresult PushCocoa(NSWindow *aWindow, NSModalSession aSession);
  // Pop the topmost Cocoa app-modal window off our list.
  nsresult PopCocoa(NSWindow *aWindow, NSModalSession aSession);
  // Push a Gecko-modal window onto the top of our list.
  nsresult PushGecko(NSWindow *aWindow, nsCocoaWindow *aWidget);
  // Pop the topmost Gecko-modal window off our list.
  nsresult PopGecko(NSWindow *aWindow, nsCocoaWindow *aWidget);
  // Return the "session" of the top-most visible Cocoa app-modal window.
  NSModalSession CurrentSession();
  // Has a Gecko modal dialog popped up over a Cocoa app-modal dialog?
  bool GeckoModalAboveCocoaModal();
private:
  nsTArray<nsCocoaAppModalWindowListItem> mList;
};

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
  NS_IMETHOD ResumeNative(void);
	
  nsAppShell();

  nsresult Init();

  NS_IMETHOD Run(void);
  NS_IMETHOD Exit(void);
  NS_IMETHOD OnProcessNextEvent(nsIThreadInternal *aThread, bool aMayWait,
                                PRUint32 aRecursionDepth);
  NS_IMETHOD AfterProcessNextEvent(nsIThreadInternal *aThread,
                                   PRUint32 aRecursionDepth);

  // public only to be visible to Objective-C code that must call it
  void WillTerminate();

protected:
  virtual ~nsAppShell();

  virtual void ScheduleNativeEventCallback();
  virtual bool ProcessNextNativeEvent(bool aMayWait);

  bool InGeckoMainEventLoop();

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

  // mHadMoreEventsCount and kHadMoreEventsCountMax are used in
  // ProcessNextNativeEvent().
  PRUint32               mHadMoreEventsCount;
  // Setting kHadMoreEventsCountMax to '10' contributed to a fairly large
  // (about 10%) increase in the number of calls to malloc (though without
  // effecting the total amount of memory used).  Cutting it to '3'
  // reduced the number of calls by 6%-7% (reducing the original regression
  // to 3%-4%).  See bmo bug 395397.
  static const PRUint32  kHadMoreEventsCountMax = 3;

  PRInt32            mRecursionDepth;
  PRInt32            mNativeEventCallbackDepth;
  // Can be set from different threads, so must be modified atomically
  PRInt32            mNativeEventScheduledDepth;
};

#endif // nsAppShell_h_
