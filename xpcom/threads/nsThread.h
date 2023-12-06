/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsThread_h__
#define nsThread_h__

#include "MainThreadUtils.h"
#include "mozilla/AlreadyAddRefed.h"
#include "mozilla/Atomics.h"
#include "mozilla/Attributes.h"
#include "mozilla/DataMutex.h"
#include "mozilla/EventQueue.h"
#include "mozilla/LinkedList.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/Mutex.h"
#include "mozilla/NotNull.h"
#include "mozilla/RefPtr.h"
#include "mozilla/TaskDispatcher.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/UniquePtr.h"
#include "nsIDirectTaskDispatcher.h"
#include "nsIEventTarget.h"
#include "nsISerialEventTarget.h"
#include "nsISupportsPriority.h"
#include "nsIThread.h"
#include "nsIThreadInternal.h"
#include "nsTArray.h"

namespace mozilla {
class CycleCollectedJSContext;
class DelayedRunnable;
class SynchronizedEventQueue;
class ThreadEventQueue;
class ThreadEventTarget;

template <typename T, size_t Length>
class Array;
}  // namespace mozilla

using mozilla::NotNull;

class nsIRunnable;
class nsThreadShutdownContext;

// See https://www.w3.org/TR/longtasks
#define LONGTASK_BUSY_WINDOW_MS 50

// Time a Runnable executes before we accumulate telemetry on it
#define LONGTASK_TELEMETRY_MS 30

// A class for managing performance counter state.
namespace mozilla {
class PerformanceCounterState {
 public:
  explicit PerformanceCounterState(const uint32_t& aNestedEventLoopDepthRef,
                                   bool aIsMainThread)
      : mNestedEventLoopDepth(aNestedEventLoopDepthRef),
        mIsMainThread(aIsMainThread),
        // Does it really make sense to initialize these to "now" when we
        // haven't run any tasks?
        mLastLongTaskEnd(TimeStamp::Now()),
        mLastLongNonIdleTaskEnd(mLastLongTaskEnd) {}

  class Snapshot {
   public:
    Snapshot(uint32_t aOldEventLoopDepth, bool aOldIsIdleRunnable)
        : mOldEventLoopDepth(aOldEventLoopDepth),
          mOldIsIdleRunnable(aOldIsIdleRunnable) {}

    Snapshot(const Snapshot&) = default;
    Snapshot(Snapshot&&) = default;

   private:
    friend class PerformanceCounterState;

    const uint32_t mOldEventLoopDepth;
    const bool mOldIsIdleRunnable;
  };

  // Notification that a runnable is about to run.  This captures a snapshot of
  // our current state before we reset to prepare for the new runnable.  This
  // muast be called after mNestedEventLoopDepth has been incremented for the
  // runnable execution.  The performance counter passed in should be the one
  // for the relevant runnable and may be null.  aIsIdleRunnable should be true
  // if and only if the runnable has idle priority.
  Snapshot RunnableWillRun(TimeStamp aNow, bool aIsIdleRunnable);

  // Notification that a runnable finished executing.  This must be passed the
  // snapshot that RunnableWillRun returned for the same runnable.  This must be
  // called before mNestedEventLoopDepth is decremented after the runnable's
  // execution.
  void RunnableDidRun(const nsCString& aName, Snapshot&& aSnapshot);

  const TimeStamp& LastLongTaskEnd() const { return mLastLongTaskEnd; }
  const TimeStamp& LastLongNonIdleTaskEnd() const {
    return mLastLongNonIdleTaskEnd;
  }

 private:
  // Called to report accumulated time, as needed, when we're about to run a
  // runnable or just finished running one.
  void MaybeReportAccumulatedTime(const nsCString& aName, TimeStamp aNow);

  // Whether the runnable we are about to run, or just ran, is a nested
  // runnable, in the sense that there is some other runnable up the stack
  // spinning the event loop.  This must be called before we change our
  // mCurrentEventLoopDepth (when about to run a new event) or after we restore
  // it (after we ran one).
  bool IsNestedRunnable() const {
    return mNestedEventLoopDepth > mCurrentEventLoopDepth;
  }

  // The event loop depth of the currently running runnable.  Set to the max
  // value of a uint32_t when there is no runnable running, so when starting to
  // run a toplevel (not nested) runnable IsNestedRunnable() will test false.
  uint32_t mCurrentEventLoopDepth = std::numeric_limits<uint32_t>::max();

  // A reference to the nsThread's mNestedEventLoopDepth, so we can
  // see what it is right now.
  const uint32_t& mNestedEventLoopDepth;

  // A boolean that indicates whether the currently running runnable is an idle
  // runnable.  Only has a useful value between RunnableWillRun() being called
  // and RunnableDidRun() returning.
  bool mCurrentRunnableIsIdleRunnable = false;

  // Whether we're attached to the mainthread nsThread.
  const bool mIsMainThread;

  // The timestamp from which time to be accounted for should be measured.  This
  // can be the start of a runnable running or the end of a nested runnable
  // running.
  TimeStamp mCurrentTimeSliceStart;

  // Information about when long tasks last ended.
  TimeStamp mLastLongTaskEnd;
  TimeStamp mLastLongNonIdleTaskEnd;
};
}  // namespace mozilla

// A native thread
class nsThread : public nsIThreadInternal,
                 public nsISupportsPriority,
                 public nsIDirectTaskDispatcher,
                 private mozilla::LinkedListElement<nsThread> {
  friend mozilla::LinkedList<nsThread>;
  friend mozilla::LinkedListElement<nsThread>;

 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIEVENTTARGET_FULL
  NS_DECL_NSITHREAD
  NS_DECL_NSITHREADINTERNAL
  NS_DECL_NSISUPPORTSPRIORITY
  NS_DECL_NSIDIRECTTASKDISPATCHER

  enum MainThreadFlag { MAIN_THREAD, NOT_MAIN_THREAD };

  nsThread(NotNull<mozilla::SynchronizedEventQueue*> aQueue,
           MainThreadFlag aMainThread,
           nsIThreadManager::ThreadCreationOptions aOptions);

 private:
  nsThread();

 public:
  // Initialize this as a named wrapper for a new PRThread.
  nsresult Init(const nsACString& aName);

  // Initialize this as a wrapper for the current PRThread.
  nsresult InitCurrentThread();

  // Get this thread's name, thread-safe.
  void GetThreadName(nsACString& aNameBuffer);

  // Set this thread's name. Consider using
  // NS_SetCurrentThreadName if you are not sure.
  void SetThreadNameInternal(const nsACString& aName);

 private:
  // Initializes the mThreadId and stack base/size members, and adds the thread
  // to the ThreadList().
  void InitCommon();

 public:
  // The PRThread corresponding to this thread.
  PRThread* GetPRThread() const { return mThread; }

  const void* StackBase() const { return mStackBase; }
  size_t StackSize() const { return mStackSize; }

  uint32_t ThreadId() const { return mThreadId; }

  // If this flag is true, then the nsThread was created using
  // nsIThreadManager::NewThread.
  bool ShutdownRequired() { return mShutdownRequired; }

  // Lets GetRunningEventDelay() determine if the pool this is part
  // of has an unstarted thread
  void SetPoolThreadFreePtr(mozilla::Atomic<bool, mozilla::Relaxed>* aPtr) {
    mIsAPoolThreadFree = aPtr;
  }

  void SetScriptObserver(mozilla::CycleCollectedJSContext* aScriptObserver);

  uint32_t RecursionDepth() const;

  void ShutdownComplete(NotNull<nsThreadShutdownContext*> aContext);

  void WaitForAllAsynchronousShutdowns();

  static const uint32_t kRunnableNameBufSize = 1000;
  static mozilla::Array<char, kRunnableNameBufSize> sMainThreadRunnableName;

  mozilla::SynchronizedEventQueue* EventQueue() { return mEvents.get(); }

  bool ShuttingDown() const { return mShutdownContext != nullptr; }

  size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const;

  // Returns the size of this object, its PRThread, and its shutdown contexts,
  // but excluding its event queues.
  size_t ShallowSizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const;

  size_t SizeOfEventQueues(mozilla::MallocSizeOf aMallocSizeOf) const;

  void SetUseHangMonitor(bool aValue) {
    MOZ_ASSERT(IsOnCurrentThread());
    mUseHangMonitor = aValue;
  }

 private:
  void DoMainThreadSpecificProcessing() const;

 protected:
  friend class nsThreadShutdownEvent;

  virtual ~nsThread();

  static void ThreadFunc(void* aArg);

  // Helper
  already_AddRefed<nsIThreadObserver> GetObserver() {
    nsIThreadObserver* obs;
    nsThread::GetObserver(&obs);
    return already_AddRefed<nsIThreadObserver>(obs);
  }

  already_AddRefed<nsThreadShutdownContext> ShutdownInternal(bool aSync);

  friend class nsThreadManager;
  friend class nsThreadPool;

  void MaybeRemoveFromThreadList();

  // Whether or not these members have a value determines whether the nsThread
  // is treated as a full XPCOM thread or as a thin wrapper.
  //
  // For full nsThreads, they will always contain valid pointers. For thin
  // wrappers around non-XPCOM threads, they will be null, and event dispatch
  // methods which rely on them will fail (and assert) if called.
  RefPtr<mozilla::SynchronizedEventQueue> mEvents;
  RefPtr<mozilla::ThreadEventTarget> mEventTarget;

  // The number of outstanding nsThreadShutdownContext started by this thread.
  // The thread will not be allowed to exit until this number reaches 0.
  uint32_t mOutstandingShutdownContexts;
  // The shutdown context for ourselves.
  RefPtr<nsThreadShutdownContext> mShutdownContext;

  mozilla::CycleCollectedJSContext* mScriptObserver;

  // Our name.
  mozilla::DataMutex<nsCString> mThreadName;

  void* mStackBase = nullptr;
  uint32_t mStackSize;
  uint32_t mThreadId;

  uint32_t mNestedEventLoopDepth;

  mozilla::Atomic<bool> mShutdownRequired;

  int8_t mPriority;

  const bool mIsMainThread;
  bool mUseHangMonitor;
  const bool mIsUiThread;
  mozilla::Atomic<bool, mozilla::Relaxed>* mIsAPoolThreadFree;

  // Set to true if this thread creates a JSRuntime.
  bool mCanInvokeJS;

  // The time the currently running event spent in event queues, and
  // when it started running.  If no event is running, they are
  // TimeDuration() & TimeStamp().
  mozilla::TimeDuration mLastEventDelay;
  mozilla::TimeStamp mLastEventStart;

#ifdef EARLY_BETA_OR_EARLIER
  nsCString mNameForWakeupTelemetry;
  mozilla::TimeStamp mLastWakeupCheckTime;
  uint32_t mWakeupCount = 0;
#endif

  mozilla::PerformanceCounterState mPerformanceCounterState;

  mozilla::SimpleTaskQueue mDirectTasks;
};

class nsThreadShutdownContext final : public nsIThreadShutdown {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSITHREADSHUTDOWN

 private:
  friend class nsThread;
  friend class nsThreadShutdownEvent;
  friend class nsThreadShutdownAckEvent;

  nsThreadShutdownContext(NotNull<nsThread*> aTerminatingThread,
                          nsThread* aJoiningThread)
      : mTerminatingThread(aTerminatingThread),
        mTerminatingPRThread(aTerminatingThread->GetPRThread()),
        mJoiningThreadMutex("nsThreadShutdownContext::mJoiningThreadMutex"),
        mJoiningThread(aJoiningThread) {}

  ~nsThreadShutdownContext() = default;

  // Must be called on the joining thread.
  void MarkCompleted();

  // NB: This may be the last reference.
  NotNull<RefPtr<nsThread>> const mTerminatingThread;
  PRThread* const mTerminatingPRThread;

  // May only be accessed on the joining thread.
  bool mCompleted = false;
  nsTArray<nsCOMPtr<nsIRunnable>> mCompletionCallbacks;

  // The thread waiting for this thread to shut down. Will either be cleared by
  // the joining thread if `StopWaitingAndLeakThread` is called or by the
  // terminating thread upon exiting and notifying the joining thread.
  mozilla::Mutex mJoiningThreadMutex;
  RefPtr<nsThread> mJoiningThread MOZ_GUARDED_BY(mJoiningThreadMutex);
  bool mThreadLeaked MOZ_GUARDED_BY(mJoiningThreadMutex) = false;
};

#if defined(XP_UNIX) && !defined(ANDROID) && !defined(DEBUG) && HAVE_UALARM && \
    defined(_GNU_SOURCE)
#  define MOZ_CANARY

extern int sCanaryOutputFD;
#endif

#endif  // nsThread_h__
