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
#include "nsAutoPtr.h"

// A native thread
class nsThread MOZ_FINAL : public nsIThreadInternal,
                           public nsISupportsPriority
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIEVENTTARGET
  NS_DECL_NSITHREAD
  NS_DECL_NSITHREADINTERNAL
  NS_DECL_NSISUPPORTSPRIORITY

  enum MainThreadFlag {
    MAIN_THREAD,
    NOT_MAIN_THREAD
  };

  nsThread(MainThreadFlag aMainThread, uint32_t aStackSize);

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

  static nsresult
  SetMainThreadObserver(nsIThreadObserver* aObserver);

private:
  static nsIThreadObserver* sMainThreadObserver;

  class nsChainedEventQueue;

  class nsNestedEventTarget;
  friend class nsNestedEventTarget;

  friend class nsThreadShutdownEvent;

  ~nsThread();

  bool ShuttingDown() { return mShutdownContext != nullptr; }

  static void ThreadFunc(void *arg);

  // Helper
  already_AddRefed<nsIThreadObserver> GetObserver() {
    nsIThreadObserver *obs;
    nsThread::GetObserver(&obs);
    return already_AddRefed<nsIThreadObserver>(obs);
  }

  // Wrappers for event queue methods:
  bool GetEvent(bool mayWait, nsIRunnable **event) {
    return mEvents->GetEvent(mayWait, event);
  }
  nsresult PutEvent(nsIRunnable *event, nsNestedEventTarget *target);

  nsresult DispatchInternal(nsIRunnable *event, uint32_t flags,
                            nsNestedEventTarget *target);

  // Wrapper for nsEventQueue that supports chaining.
  class nsChainedEventQueue {
  public:
    nsChainedEventQueue()
      : mNext(nullptr) {
    }

    bool GetEvent(bool mayWait, nsIRunnable **event) {
      return mQueue.GetEvent(mayWait, event);
    }

    bool PutEvent(nsIRunnable *event) {
      return mQueue.PutEvent(event);
    }

    bool HasPendingEvent() {
      return mQueue.HasPendingEvent();
    }

    nsChainedEventQueue *mNext;
    nsRefPtr<nsNestedEventTarget> mEventTarget;

  private:
    nsEventQueue mQueue;
  };

  class nsNestedEventTarget MOZ_FINAL : public nsIEventTarget {
  public:
    NS_DECL_THREADSAFE_ISUPPORTS
    NS_DECL_NSIEVENTTARGET

    nsNestedEventTarget(nsThread *thread, nsChainedEventQueue *queue)
      : mThread(thread), mQueue(queue) {
    }

    nsRefPtr<nsThread> mThread;

    // This is protected by mThread->mLock.
    nsChainedEventQueue* mQueue;

  private:
    ~nsNestedEventTarget() {}
  };

  // This lock protects access to mObserver, mEvents and mEventsAreDoomed.
  // All of those fields are only modified on the thread itself (never from
  // another thread).  This means that we can avoid holding the lock while
  // using mObserver and mEvents on the thread itself.  When calling PutEvent
  // on mEvents, we have to hold the lock to synchronize with PopEventQueue.
  mozilla::Mutex mLock;

  nsCOMPtr<nsIThreadObserver> mObserver;

  // Only accessed on the target thread.
  nsAutoTObserverArray<nsCOMPtr<nsIThreadObserver>, 2> mEventObservers;

  nsChainedEventQueue *mEvents;   // never null
  nsChainedEventQueue  mEventsRoot;

  int32_t   mPriority;
  PRThread *mThread;
  uint32_t  mRunningEvent;  // counter
  uint32_t  mStackSize;

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
    return mSyncTask != nullptr;
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

#if defined(XP_UNIX) && !defined(ANDROID) && !defined(DEBUG) && HAVE_UALARM \
  && defined(_GNU_SOURCE)
# define MOZ_CANARY

extern int sCanaryOutputFD;
#endif

#endif  // nsThread_h__
