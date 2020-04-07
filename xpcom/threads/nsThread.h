/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsThread_h__
#define nsThread_h__

#include "mozilla/Mutex.h"
#include "nsIThreadInternal.h"
#include "nsISupportsPriority.h"
#include "nsThreadUtils.h"
#include "nsString.h"
#include "nsTObserverArray.h"
#include "mozilla/Atomics.h"
#include "mozilla/Attributes.h"
#include "mozilla/IntegerTypeTraits.h"
#include "mozilla/LinkedList.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/SynchronizedEventQueue.h"
#include "mozilla/NotNull.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/AlreadyAddRefed.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/Array.h"
#include "mozilla/dom/DocGroup.h"

namespace mozilla {
class CycleCollectedJSContext;
class EventQueue;
template <typename>
class ThreadEventQueue;
class ThreadEventTarget;
}  // namespace mozilla

using mozilla::NotNull;

class nsLocalExecutionRecord;
class nsThreadEnumerator;

// See https://www.w3.org/TR/longtasks
#define LONGTASK_BUSY_WINDOW_MS 50

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
    Snapshot(uint32_t aOldEventLoopDepth, PerformanceCounter* aCounter,
             bool aOldIsIdleRunnable)
        : mOldEventLoopDepth(aOldEventLoopDepth),
          mOldPerformanceCounter(aCounter),
          mOldIsIdleRunnable(aOldIsIdleRunnable) {}

    Snapshot(const Snapshot&) = default;
    Snapshot(Snapshot&&) = default;

   private:
    friend class PerformanceCounterState;

    const uint32_t mOldEventLoopDepth;
    // Non-const so we can move out of it and avoid the extra refcounting.
    RefPtr<PerformanceCounter> mOldPerformanceCounter;
    const bool mOldIsIdleRunnable;
  };

  // Notification that a runnable is about to run.  This captures a snapshot of
  // our current state before we reset to prepare for the new runnable.  This
  // muast be called after mNestedEventLoopDepth has been incremented for the
  // runnable execution.  The performance counter passed in should be the one
  // for the relevant runnable and may be null.  aIsIdleRunnable should be true
  // if and only if the runnable has idle priority.
  Snapshot RunnableWillRun(PerformanceCounter* Counter, TimeStamp aNow,
                           bool aIsIdleRunnable);

  // Notification that a runnable finished executing.  This must be passed the
  // snapshot that RunnableWillRun returned for the same runnable.  This must be
  // called before mNestedEventLoopDepth is decremented after the runnable's
  // execution.
  void RunnableDidRun(Snapshot&& aSnapshot);

  const TimeStamp& LastLongTaskEnd() const { return mLastLongTaskEnd; }
  const TimeStamp& LastLongNonIdleTaskEnd() const {
    return mLastLongNonIdleTaskEnd;
  }

 private:
  // Called to report accumulated time, as needed, when we're about to run a
  // runnable or just finished running one.
  void MaybeReportAccumulatedTime(TimeStamp aNow);

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
  bool mIsMainThread;

  // The timestamp from which time to be accounted for should be measured.  This
  // can be the start of a runnable running or the end of a nested runnable
  // running.
  TimeStamp mCurrentTimeSliceStart;

  // Information about when long tasks last ended.
  TimeStamp mLastLongTaskEnd;
  TimeStamp mLastLongNonIdleTaskEnd;

  // The performance counter to use for accumulating the runtime of
  // the currently running event.  May be null, in which case the
  // event's running time should not be accounted to any performance
  // counters.
  RefPtr<PerformanceCounter> mCurrentPerformanceCounter;
};
}  // namespace mozilla

// A native thread
class nsThread : public nsIThreadInternal,
                 public nsISupportsPriority,
                 private mozilla::LinkedListElement<nsThread> {
  friend mozilla::LinkedList<nsThread>;
  friend mozilla::LinkedListElement<nsThread>;

 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIEVENTTARGET_FULL
  NS_DECL_NSITHREAD
  NS_DECL_NSITHREADINTERNAL
  NS_DECL_NSISUPPORTSPRIORITY

  enum MainThreadFlag { MAIN_THREAD, NOT_MAIN_THREAD };

  nsThread(NotNull<mozilla::SynchronizedEventQueue*> aQueue,
           MainThreadFlag aMainThread, uint32_t aStackSize);

 private:
  nsThread();

 public:
  // Initialize this as a named wrapper for a new PRThread.
  nsresult Init(const nsACString& aName);

  // Initialize this as a wrapper for the current PRThread.
  nsresult InitCurrentThread();

 private:
  // Initializes the mThreadId and stack base/size members, and adds the thread
  // to the ThreadList().
  void InitCommon();

 public:
  // The PRThread corresponding to this thread.
  PRThread* GetPRThread() { return mThread; }

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

  void ShutdownComplete(NotNull<struct nsThreadShutdownContext*> aContext);

  void WaitForAllAsynchronousShutdowns();

  static const uint32_t kRunnableNameBufSize = 1000;
  static mozilla::Array<char, kRunnableNameBufSize> sMainThreadRunnableName;

  void EnableInputEventPrioritization() {
    EventQueue()->EnableInputEventPrioritization();
  }

  void FlushInputEventPrioritization() {
    EventQueue()->FlushInputEventPrioritization();
  }

  void SuspendInputEventPrioritization() {
    EventQueue()->SuspendInputEventPrioritization();
  }

  void ResumeInputEventPrioritization() {
    EventQueue()->ResumeInputEventPrioritization();
  }

  mozilla::SynchronizedEventQueue* EventQueue() { return mEvents.get(); }

  bool ShuttingDown() { return mShutdownContext != nullptr; }

  virtual mozilla::PerformanceCounter* GetPerformanceCounter(
      nsIRunnable* aEvent);

  size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const;

  // Returns the size of this object, its PRThread, and its shutdown contexts,
  // but excluding its event queues.
  size_t ShallowSizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const;

  size_t SizeOfEventQueues(mozilla::MallocSizeOf aMallocSizeOf) const;

  static nsThreadEnumerator Enumerate();

  static uint32_t MaxActiveThreads();

  // When entering local execution mode a new event queue is created and used as
  // an event source. This queue is only accessible through an
  // nsLocalExecutionGuard constructed from the nsLocalExecutionRecord returned
  // by this function, effectively restricting the events that get run while in
  // local execution mode to those dispatched by the owner of the guard object.
  //
  // Local execution is not nestable. When the nsLocalExecutionGuard is
  // destructed, the thread exits the local execution mode.
  //
  // Note that code run in local execution mode is not considered a task in the
  // spec sense. Events from the local queue are considered part of the
  // enclosing task and as such do not trigger profiling hooks, observer
  // notifications, etc.
  nsLocalExecutionRecord EnterLocalExecution();

 private:
  void DoMainThreadSpecificProcessing(bool aReallyWait);

 protected:
  friend class nsThreadShutdownEvent;

  friend class nsThreadEnumerator;

  virtual ~nsThread();

  static void ThreadFunc(void* aArg);

  // Helper
  already_AddRefed<nsIThreadObserver> GetObserver() {
    nsIThreadObserver* obs;
    nsThread::GetObserver(&obs);
    return already_AddRefed<nsIThreadObserver>(obs);
  }

  struct nsThreadShutdownContext* ShutdownInternal(bool aSync);

  friend class nsThreadManager;
  friend class nsThreadPool;

  static mozilla::OffTheBooksMutex& ThreadListMutex();
  static mozilla::LinkedList<nsThread>& ThreadList();
  static void ClearThreadList();

  // The current number of active threads.
  static uint32_t sActiveThreads;
  // The maximum current number of active threads we've had in this session.
  static uint32_t sMaxActiveThreads;

  void AddToThreadList();
  void MaybeRemoveFromThreadList();

  // Whether or not these members have a value determines whether the nsThread
  // is treated as a full XPCOM thread or as a thin wrapper.
  //
  // For full nsThreads, they will always contain valid pointers. For thin
  // wrappers around non-XPCOM threads, they will be null, and event dispatch
  // methods which rely on them will fail (and assert) if called.
  RefPtr<mozilla::SynchronizedEventQueue> mEvents;
  RefPtr<mozilla::ThreadEventTarget> mEventTarget;

  // The shutdown contexts for any other threads we've asked to shut down.
  using ShutdownContexts =
      nsTArray<mozilla::UniquePtr<struct nsThreadShutdownContext>>;

  // Helper for finding a ShutdownContext in the contexts array.
  struct ShutdownContextsComp {
    bool Equals(const ShutdownContexts::elem_type& a,
                const ShutdownContexts::elem_type::Pointer b) const;
  };

  ShutdownContexts mRequestedShutdownContexts;
  // The shutdown context for ourselves.
  struct nsThreadShutdownContext* mShutdownContext;

  mozilla::CycleCollectedJSContext* mScriptObserver;

  void* mStackBase = nullptr;
  uint32_t mStackSize;
  uint32_t mThreadId;

  uint32_t mNestedEventLoopDepth;

  mozilla::Atomic<bool> mShutdownRequired;

  int8_t mPriority;

  bool mIsMainThread;
  mozilla::Atomic<bool, mozilla::Relaxed>* mIsAPoolThreadFree;

  // Set to true if this thread creates a JSRuntime.
  bool mCanInvokeJS;

  bool mHasTLSEntry = false;

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

  bool mIsInLocalExecutionMode = false;
};

struct nsThreadShutdownContext {
  nsThreadShutdownContext(NotNull<nsThread*> aTerminatingThread,
                          NotNull<nsThread*> aJoiningThread,
                          bool aAwaitingShutdownAck)
      : mTerminatingThread(aTerminatingThread),
        mTerminatingPRThread(aTerminatingThread->GetPRThread()),
        mJoiningThread(aJoiningThread),
        mAwaitingShutdownAck(aAwaitingShutdownAck),
        mIsMainThreadJoining(NS_IsMainThread()) {
    MOZ_COUNT_CTOR(nsThreadShutdownContext);
  }
  MOZ_COUNTED_DTOR(nsThreadShutdownContext)

  // NB: This will be the last reference.
  NotNull<RefPtr<nsThread>> mTerminatingThread;
  PRThread* const mTerminatingPRThread;
  NotNull<nsThread*> MOZ_UNSAFE_REF(
      "Thread manager is holding reference to joining thread") mJoiningThread;
  bool mAwaitingShutdownAck;
  bool mIsMainThreadJoining;
};

class nsLocalExecutionRecord;

// This RAII class controls the duration of the associated nsThread's local
// execution mode and provides access to the local event target. (See
// nsThread::EnterLocalExecution() for details.) It is constructed from an
// nsLocalExecutionRecord, which can only be constructed by nsThread.
class MOZ_RAII nsLocalExecutionGuard final {
 public:
  MOZ_IMPLICIT nsLocalExecutionGuard(
      nsLocalExecutionRecord&& aLocalExecutionRecord);
  nsLocalExecutionGuard(const nsLocalExecutionGuard&) = delete;
  nsLocalExecutionGuard(nsLocalExecutionGuard&&) = delete;
  ~nsLocalExecutionGuard();

  nsCOMPtr<nsISerialEventTarget> GetEventTarget() const {
    return mLocalEventTarget;
  }

 private:
  mozilla::SynchronizedEventQueue& mEventQueueStack;
  nsCOMPtr<nsISerialEventTarget> mLocalEventTarget;
  bool& mLocalExecutionFlag;
};

class MOZ_TEMPORARY_CLASS nsLocalExecutionRecord final {
 private:
  friend class nsThread;
  friend class nsLocalExecutionGuard;

  nsLocalExecutionRecord(mozilla::SynchronizedEventQueue& aEventQueueStack,
                         bool& aLocalExecutionFlag)
      : mEventQueueStack(aEventQueueStack),
        mLocalExecutionFlag(aLocalExecutionFlag) {}

  nsLocalExecutionRecord(nsLocalExecutionRecord&&) = default;

 public:
  nsLocalExecutionRecord(const nsLocalExecutionRecord&) = delete;

 private:
  mozilla::SynchronizedEventQueue& mEventQueueStack;
  bool& mLocalExecutionFlag;
};

class MOZ_STACK_CLASS nsThreadEnumerator final {
 public:
  nsThreadEnumerator() = default;

  auto begin() { return nsThread::ThreadList().begin(); }
  auto end() { return nsThread::ThreadList().end(); }

 private:
  mozilla::OffTheBooksMutexAutoLock mMal{nsThread::ThreadListMutex()};
};

#if defined(XP_UNIX) && !defined(ANDROID) && !defined(DEBUG) && HAVE_UALARM && \
    defined(_GNU_SOURCE)
#  define MOZ_CANARY

extern int sCanaryOutputFD;
#endif

#endif  // nsThread_h__
