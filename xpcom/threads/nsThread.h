/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsThread_h__
#define nsThread_h__

#include "mozilla/Mutex.h"
#include "nsIIdlePeriod.h"
#include "nsIThreadInternal.h"
#include "nsISupportsPriority.h"
#include "nsEventQueue.h"
#include "nsThreadUtils.h"
#include "nsString.h"
#include "nsTObserverArray.h"
#include "mozilla/Attributes.h"
#include "mozilla/NotNull.h"
#include "mozilla/TimeStamp.h"
#include "nsAutoPtr.h"
#include "mozilla/AlreadyAddRefed.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/Array.h"

namespace mozilla {
class CycleCollectedJSContext;
}

using mozilla::NotNull;

// A native thread
class nsThread
  : public nsIThreadInternal
  , public nsISupportsPriority
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIEVENTTARGET_FULL
  NS_DECL_NSITHREAD
  NS_DECL_NSITHREADINTERNAL
  NS_DECL_NSISUPPORTSPRIORITY

  enum MainThreadFlag
  {
    MAIN_THREAD,
    NOT_MAIN_THREAD
  };

  nsThread(MainThreadFlag aMainThread, uint32_t aStackSize);

  // Initialize this as a wrapper for a new PRThread, and optionally give it a name.
  nsresult Init(const nsACString& aName = NS_LITERAL_CSTRING(""));

  // Initialize this as a wrapper for the current PRThread.
  nsresult InitCurrentThread();

  // The PRThread corresponding to this thread.
  PRThread* GetPRThread()
  {
    return mThread;
  }

  // If this flag is true, then the nsThread was created using
  // nsIThreadManager::NewThread.
  bool ShutdownRequired()
  {
    return mShutdownRequired;
  }

  // Clear the observer list.
  void ClearObservers()
  {
    mEventObservers.Clear();
  }

  void
  SetScriptObserver(mozilla::CycleCollectedJSContext* aScriptObserver);

  uint32_t
  RecursionDepth() const;

  void ShutdownComplete(NotNull<struct nsThreadShutdownContext*> aContext);

  void WaitForAllAsynchronousShutdowns();

#ifdef MOZ_CRASHREPORTER
  enum class ShouldSaveMemoryReport
  {
    kMaybeReport,
    kForceReport
  };

  static bool SaveMemoryReportNearOOM(ShouldSaveMemoryReport aShouldSave);
#endif

  static const uint32_t kRunnableNameBufSize = 1000;
  static mozilla::Array<char, kRunnableNameBufSize> sMainThreadRunnableName;

  // Query whether there are some pending input events in the queue. This method
  // is supposed to be called on main thread with input event prioritization
  // enabled.
  bool HasPendingInputEvents()
  {
    MOZ_ASSERT(NS_IsMainThread());
    mozilla::MutexAutoLock lock(mLock);
    return mEventsRoot.HasPendingEventsInInputQueue(lock);
  }

private:
  void DoMainThreadSpecificProcessing(bool aReallyWait);

  // Returns a null TimeStamp if we're not in the idle period.
  mozilla::TimeStamp GetIdleDeadline();
  void GetIdleEvent(nsIRunnable** aEvent, mozilla::MutexAutoLock& aProofOfLock);
  void GetEvent(bool aWait, nsIRunnable** aEvent,
                unsigned short* aPriority,
                mozilla::MutexAutoLock& aProofOfLock);

protected:
  class nsChainedEventQueue;

  class nsNestedEventTarget;
  friend class nsNestedEventTarget;

  friend class nsThreadShutdownEvent;

  virtual ~nsThread();

  bool ShuttingDown()
  {
    return mShutdownContext != nullptr;
  }

  static void ThreadFunc(void* aArg);

  // Helper
  already_AddRefed<nsIThreadObserver> GetObserver()
  {
    nsIThreadObserver* obs;
    nsThread::GetObserver(&obs);
    return already_AddRefed<nsIThreadObserver>(obs);
  }

  // Wrappers for event queue methods:
  nsresult PutEvent(nsIRunnable* aEvent, nsNestedEventTarget* aTarget);
  nsresult PutEvent(already_AddRefed<nsIRunnable> aEvent,
                    nsNestedEventTarget* aTarget);

  nsresult DispatchInternal(already_AddRefed<nsIRunnable> aEvent,
                            uint32_t aFlags, nsNestedEventTarget* aTarget);

  struct nsThreadShutdownContext* ShutdownInternal(bool aSync);

  // Wrapper for nsEventQueue that supports chaining and prioritization.
  class nsChainedEventQueue
  {
  public:
    explicit nsChainedEventQueue(mozilla::Mutex& aLock)
      : mNext(nullptr)
      , mEventsAvailable(aLock, "[nsChainedEventQueue.mEventsAvailable]")
      , mIsInputPrioritizationEnabled(false)
      , mIsReadyToPrioritizeEvents(false)
      , mProcessHighPriorityQueueRunnable(false)
    {
      mNormalQueue =
        mozilla::MakeUnique<nsEventQueue>(mEventsAvailable,
                                          nsEventQueue::eSharedCondVarQueue);
      // All queues need to use the same CondVar!
      mInputQueue =
        mozilla::MakeUnique<nsEventQueue>(mEventsAvailable,
                                          nsEventQueue::eSharedCondVarQueue);
      mHighQueue =
        mozilla::MakeUnique<nsEventQueue>(mEventsAvailable,
                                          nsEventQueue::eSharedCondVarQueue);
    }

    void EnablePrioritization(mozilla::MutexAutoLock& aProofOfLock);

    bool IsPrioritizationEnabled()
    {
      return mIsInputPrioritizationEnabled;
    }

    bool GetEvent(bool aMayWait, nsIRunnable** aEvent,
                  unsigned short* aPriority,
                  mozilla::MutexAutoLock& aProofOfLock) {
      return mIsReadyToPrioritizeEvents
        ? GetNormalOrInputOrHighPriorityEvent(aMayWait, aEvent, aPriority, aProofOfLock)
        : GetNormalOrHighPriorityEvent(aMayWait, aEvent, aPriority, aProofOfLock);
    }

    void PutEvent(nsIRunnable* aEvent, mozilla::MutexAutoLock& aProofOfLock)
    {
      RefPtr<nsIRunnable> event(aEvent);
      PutEvent(event.forget(), aProofOfLock);
    }

    void PutEvent(already_AddRefed<nsIRunnable> aEvent,
                  mozilla::MutexAutoLock& aProofOfLock);

    bool HasPendingEvent(mozilla::MutexAutoLock& aProofOfLock)
    {
      return mNormalQueue->HasPendingEvent(aProofOfLock) ||
             mInputQueue->HasPendingEvent(aProofOfLock) ||
             mHighQueue->HasPendingEvent(aProofOfLock);
    }

    bool HasPendingEventsInInputQueue(mozilla::MutexAutoLock& aProofOfLock)
    {
      MOZ_ASSERT(mIsInputPrioritizationEnabled);
      return mInputQueue->HasPendingEvent(aProofOfLock);
    }

    nsChainedEventQueue* mNext;
    RefPtr<nsNestedEventTarget> mEventTarget;

  private:
    bool GetNormalOrInputOrHighPriorityEvent(bool aMayWait,
                                             nsIRunnable** aEvent,
                                             unsigned short* aPriority,
                                             mozilla::MutexAutoLock& aProofOfLock);

    bool GetNormalOrHighPriorityEvent(bool aMayWait, nsIRunnable** aEvent,
                                      unsigned short* aPriority,
                                      mozilla::MutexAutoLock& aProofOfLock);

    // This is used to flush pending events in nsChainedEventQueue::mNormalQueue
    // before starting event prioritization.
    class EnablePrioritizationRunnable final : public nsIRunnable
    {
      nsChainedEventQueue* mEventQueue;

    public:
      NS_DECL_ISUPPORTS

      explicit EnablePrioritizationRunnable(nsChainedEventQueue* aQueue)
        : mEventQueue(aQueue)
      {
      }

      NS_IMETHOD Run() override
      {
        mEventQueue->mIsReadyToPrioritizeEvents = true;
        return NS_OK;
      }
    private:
      ~EnablePrioritizationRunnable()
      {
      }
    };

    static void SetPriorityIfNotNull(unsigned short* aPriority, short aValue)
    {
      if (aPriority) {
        *aPriority = aValue;
      }
    }
    mozilla::CondVar mEventsAvailable;
    mozilla::TimeStamp mInputHandlingStartTime;
    mozilla::UniquePtr<nsEventQueue> mNormalQueue;
    mozilla::UniquePtr<nsEventQueue> mInputQueue;
    mozilla::UniquePtr<nsEventQueue> mHighQueue;
    bool mIsInputPrioritizationEnabled;

    // When enabling input event prioritization, there may be some events in the
    // queue. We have to process all of them before the new coming events to
    // prevent the queued events are preempted by the newly ones with the same
    // priority.
    bool mIsReadyToPrioritizeEvents;
    // Try to process one high priority runnable after each normal
    // priority runnable. This gives the processing model HTML spec has for
    // 'Update the rendering' in the case only vsync messages are in the
    // secondary queue and prevents starving the normal queue.
    bool mProcessHighPriorityQueueRunnable;
  };

  class nsNestedEventTarget final : public nsIEventTarget
  {
  public:
    NS_DECL_THREADSAFE_ISUPPORTS
    NS_DECL_NSIEVENTTARGET_FULL

    nsNestedEventTarget(NotNull<nsThread*> aThread,
                        NotNull<nsChainedEventQueue*> aQueue)
      : mThread(aThread)
      , mQueue(aQueue)



    {
    }

    NotNull<RefPtr<nsThread>> mThread;

    // This is protected by mThread->mLock.
    nsChainedEventQueue* mQueue;

  private:
    ~nsNestedEventTarget()
    {
    }
  };

  // This lock protects access to mObserver, mEvents, mIdleEvents,
  // mIdlePeriod and mEventsAreDoomed.  All of those fields are only
  // modified on the thread itself (never from another thread).  This
  // means that we can avoid holding the lock while using mObserver
  // and mEvents on the thread itself.  When calling PutEvent on
  // mEvents, we have to hold the lock to synchronize with
  // PopEventQueue.
  mozilla::Mutex mLock;

  nsCOMPtr<nsIThreadObserver> mObserver;
  mozilla::CycleCollectedJSContext* mScriptObserver;

  // Only accessed on the target thread.
  nsAutoTObserverArray<NotNull<nsCOMPtr<nsIThreadObserver>>, 2> mEventObservers;

  NotNull<nsChainedEventQueue*> mEvents;  // never null
  nsChainedEventQueue mEventsRoot;

  // mIdlePeriod keeps track of the current idle period. If at any
  // time the main event queue is empty, calling
  // mIdlePeriod->GetIdlePeriodHint() will give an estimate of when
  // the current idle period will end.
  nsCOMPtr<nsIIdlePeriod> mIdlePeriod;
  mozilla::CondVar mIdleEventsAvailable;
  nsEventQueue mIdleEvents;

  int32_t   mPriority;
  PRThread* mThread;
  uint32_t  mNestedEventLoopDepth;
  uint32_t  mStackSize;

  // The shutdown context for ourselves.
  struct nsThreadShutdownContext* mShutdownContext;
  // The shutdown contexts for any other threads we've asked to shut down.
  nsTArray<nsAutoPtr<struct nsThreadShutdownContext>> mRequestedShutdownContexts;

  bool mShutdownRequired;
  // Set to true when events posted to this thread will never run.
  bool mEventsAreDoomed;
  MainThreadFlag mIsMainThread;

  // The time when we last ran an unlabeled runnable (one not associated with a
  // SchedulerGroup).
  mozilla::TimeStamp mLastUnlabeledRunnable;

  // Set to true if this thread creates a JSRuntime.
  bool mCanInvokeJS;
  // Set to true if HasPendingEvents() has been called and returned true because
  // of a pending idle event.  This is used to remember to return that idle
  // event from GetIdleEvent() to ensure that HasPendingEvents() never lies.
  bool mHasPendingEventsPromisedIdleEvent;

#ifndef RELEASE_OR_BETA
  mozilla::TimeStamp mNextIdleDeadline;
#endif
};

#if defined(XP_UNIX) && !defined(ANDROID) && !defined(DEBUG) && HAVE_UALARM \
  && defined(_GNU_SOURCE)
# define MOZ_CANARY

extern int sCanaryOutputFD;
#endif

#endif  // nsThread_h__
