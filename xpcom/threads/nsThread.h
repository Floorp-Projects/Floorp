/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsThread_h__
#define nsThread_h__

#include "mozilla/Mutex.h"
#include "nsIThreadInternal.h"
#include "nsISupportsPriority.h"
#include "nsEventQueue.h"
#include "nsThreadUtils.h"
#include "nsString.h"
#include "nsTObserverArray.h"
#include "mozilla/Attributes.h"

// A native thread
class nsThread MOZ_FINAL : public nsIThreadInternal,
                           public nsISupportsPriority
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIEVENTTARGET
  NS_DECL_NSITHREAD
  NS_DECL_NSITHREADINTERNAL
  NS_DECL_NSISUPPORTSPRIORITY

  enum MainThreadFlag {
    MAIN_THREAD,
    NOT_MAIN_THREAD
  };

  nsThread(MainThreadFlag aMainThread, PRUint32 aStackSize);

  // Initialize this as a wrapper for a new PRThread.
  nsresult Init();

  // Initialize this as a wrapper for the current PRThread.
  nsresult InitCurrentThread();

  // The PRThread corresponding to this thread.
  PRThread *GetPRThread() { return mThread; }

  // If this flag is true, then the nsThread was created using
  // nsIThreadManager::NewThread.
  bool ShutdownRequired() { return mShutdownRequired; }

  // Clear the observer list.
  void ClearObservers() { mEventObservers.Clear(); }

private:
  friend class nsThreadShutdownEvent;

  ~nsThread();

  bool ShuttingDown() { return mShutdownContext != nsnull; }

  static void ThreadFunc(void *arg);

  // Helper
  already_AddRefed<nsIThreadObserver> GetObserver() {
    nsIThreadObserver *obs;
    nsThread::GetObserver(&obs);
    return already_AddRefed<nsIThreadObserver>(obs);
  }

  // Wrappers for event queue methods:
  bool GetEvent(bool mayWait, nsIRunnable **event) {
    return mEvents.GetEvent(mayWait, event);
  }
  nsresult PutEvent(nsIRunnable *event);

  // This lock protects access to mObserver, mEvents and mEventsAreDoomed.
  // All of those fields are only modified on the thread itself (never from
  // another thread).  This means that we can avoid holding the lock while
  // using mObserver and mEvents on the thread itself.  When calling PutEvent
  // on mEvents, we have to hold the lock to synchronize with PopEventQueue.
  mozilla::Mutex mLock;

  nsCOMPtr<nsIThreadObserver> mObserver;

  // Only accessed on the target thread.
  nsAutoTObserverArray<nsCOMPtr<nsIThreadObserver>, 2> mEventObservers;

  nsEventQueue  mEvents;

  PRInt32   mPriority;
  PRThread *mThread;
  PRUint32  mRunningEvent;  // counter
  PRUint32  mStackSize;

  struct nsThreadShutdownContext *mShutdownContext;

  bool mShutdownRequired;
  // Set to true when events posted to this thread will never run.
  bool mEventsAreDoomed;
  MainThreadFlag mIsMainThread;
};

//-----------------------------------------------------------------------------

class nsThreadSyncDispatch : public nsRunnable {
public:
  nsThreadSyncDispatch(nsIThread *origin, nsIRunnable *task)
    : mOrigin(origin), mSyncTask(task), mResult(NS_ERROR_NOT_INITIALIZED) {
  }

  bool IsPending() {
    return mSyncTask != nsnull;
  }

  nsresult Result() {
    return mResult;
  }

private:
  NS_DECL_NSIRUNNABLE

  nsCOMPtr<nsIThread> mOrigin;
  nsCOMPtr<nsIRunnable> mSyncTask;
  nsresult mResult;
};

namespace mozilla {

/**
 * This function causes the main thread to fire a memory pressure event at its
 * next available opportunity.
 *
 * You may call this function from any thread.
 */
void ScheduleMemoryPressureEvent();

} // namespace mozilla

#endif  // nsThread_h__
