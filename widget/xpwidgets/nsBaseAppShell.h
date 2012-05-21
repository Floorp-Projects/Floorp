/* -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsBaseAppShell_h__
#define nsBaseAppShell_h__

#include "nsIAppShell.h"
#include "nsIThreadInternal.h"
#include "nsIObserver.h"
#include "nsIRunnable.h"
#include "nsCOMPtr.h"
#include "nsTArray.h"
#include "prinrval.h"

/**
 * A singleton that manages the UI thread's event queue.  Subclass this class
 * to enable platform-specific event queue support.
 */
class nsBaseAppShell : public nsIAppShell, public nsIThreadObserver,
                       public nsIObserver
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIAPPSHELL
  NS_DECL_NSITHREADOBSERVER
  NS_DECL_NSIOBSERVER

  nsBaseAppShell();

protected:
  virtual ~nsBaseAppShell();

  /**
   * This method is called by subclasses when the app shell singleton is
   * instantiated.
   */
  nsresult Init();

  /**
   * Called by subclasses from a native event. See ScheduleNativeEventCallback.
   */
  void NativeEventCallback();

  /**
   * Make a decision as to whether or not NativeEventCallback will
   * trigger gecko event processing when there are pending gecko
   * events.
   */
  virtual void DoProcessMoreGeckoEvents();

  /**
   * Implemented by subclasses.  Invoke NativeEventCallback from a native
   * event.  This method may be called on any thread.
   */
  virtual void ScheduleNativeEventCallback() = 0;

  /**
   * Implemented by subclasses.  Process the next native event.  Only wait for
   * the next native event if mayWait is true.  This method is only called on
   * the main application thread.
   *
   * @param mayWait
   *   If "true", then this method may wait if necessary for the next available
   *   native event.  DispatchNativeEvent may be called to unblock a call to
   *   ProcessNextNativeEvent that is waiting.
   * @return
   *   This method returns "true" if a native event was processed.
   */
  virtual bool ProcessNextNativeEvent(bool mayWait) = 0;

  PRInt32 mSuspendNativeCount;
  PRUint32 mEventloopNestingLevel;

private:
  bool DoProcessNextNativeEvent(bool mayWait, PRUint32 recursionDepth);

  bool DispatchDummyEvent(nsIThread* target);

  /**
   * Runs all synchronous sections which are queued up in mSyncSections.
   */
  void RunSyncSectionsInternal(bool stable, PRUint32 threadRecursionLevel);

  void RunSyncSections(bool stable, PRUint32 threadRecursionLevel)
  {
    if (!mSyncSections.IsEmpty()) {
      RunSyncSectionsInternal(stable, threadRecursionLevel);
    }
  }

  void ScheduleSyncSection(nsIRunnable* runnable, bool stable);

  struct SyncSection {
    SyncSection()
    : mStable(false), mEventloopNestingLevel(0), mThreadRecursionLevel(0)
    { }

    void Forget(SyncSection* other) {
      other->mStable = mStable;
      other->mEventloopNestingLevel = mEventloopNestingLevel;
      other->mThreadRecursionLevel = mThreadRecursionLevel;
      other->mRunnable = mRunnable.forget();
    }

    bool mStable;
    PRUint32 mEventloopNestingLevel;
    PRUint32 mThreadRecursionLevel;
    nsCOMPtr<nsIRunnable> mRunnable;
  };

  nsCOMPtr<nsIRunnable> mDummyEvent;
  /**
   * mBlockedWait points back to a slot that controls the wait loop in
   * an outer OnProcessNextEvent invocation.  Nested calls always set
   * it to false to unblock an outer loop, since all events may
   * have been consumed by the inner event loop(s).
   */
  bool *mBlockedWait;
  PRInt32 mFavorPerf;
  PRInt32 mNativeEventPending;
  PRIntervalTime mStarvationDelay;
  PRIntervalTime mSwitchTime;
  PRIntervalTime mLastNativeEventTime;
  enum EventloopNestingState {
    eEventloopNone,  // top level thread execution
    eEventloopXPCOM, // innermost native event loop is ProcessNextNativeEvent
    eEventloopOther  // innermost native event loop is a native library/plugin etc
  };
  EventloopNestingState mEventloopNestingState;
  nsTArray<SyncSection> mSyncSections;
  bool mRunning;
  bool mExiting;
  /**
   * mBlockNativeEvent blocks the appshell from processing native events.
   * It is set to true while a nested native event loop (eEventloopOther)
   * is processing gecko events in NativeEventCallback(), thus queuing up
   * native events until we return to that loop (bug 420148).
   * We force mBlockNativeEvent to false in case handling one of the gecko
   * events spins up a nested XPCOM event loop (eg. modal window) which would
   * otherwise lead to a "deadlock" where native events aren't processed at all.
   */
  bool mBlockNativeEvent;
  /**
   * Tracks whether we have processed any gecko events in NativeEventCallback so
   * that we can avoid erroneously entering a blocking loop waiting for gecko
   * events to show up during OnProcessNextEvent.  This is required because on
   * OS X ProcessGeckoEvents may be invoked inside the context of 
   * ProcessNextNativeEvent and may result in NativeEventCallback being invoked
   * and in turn invoking NS_ProcessPendingEvents.  Because
   * ProcessNextNativeEvent may be invoked prior to the NS_HasPendingEvents
   * waiting loop, this is the only way to make the loop aware that events may
   * have been processed.
   *
   * This variable is set to false in OnProcessNextEvent prior to the first
   * call to DoProcessNextNativeEvent.  It is set to true by
   * NativeEventCallback after calling NS_ProcessPendingEvents.
   */
  bool mProcessedGeckoEvents;
};

#endif // nsBaseAppShell_h__
