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
#include "nsThreadUtils.h"
#include "nsString.h"
#include "nsTObserverArray.h"
#include "mozilla/Attributes.h"
#include "mozilla/LinkedList.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/SynchronizedEventQueue.h"
#include "mozilla/NotNull.h"
#include "mozilla/TimeStamp.h"
#include "nsAutoPtr.h"
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

#ifndef RELEASE_OR_BETA
  mozilla::TimeStamp& NextIdleDeadlineRef() { return mNextIdleDeadline; }
#endif

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

  const mozilla::TimeStamp& LastLongTaskEnd() { return mLastLongTaskEnd; }
  const mozilla::TimeStamp& LastLongNonIdleTaskEnd() {
    return mLastLongNonIdleTaskEnd;
  }

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
  using ShutdownContexts = nsTArray<nsAutoPtr<struct nsThreadShutdownContext>>;
  ShutdownContexts mRequestedShutdownContexts;
  // The shutdown context for ourselves.
  struct nsThreadShutdownContext* mShutdownContext;

  mozilla::CycleCollectedJSContext* mScriptObserver;

  void* mStackBase = nullptr;
  uint32_t mStackSize;
  uint32_t mThreadId;

  uint32_t mNestedEventLoopDepth;
  uint32_t mCurrentEventLoopDepth;

  mozilla::TimeStamp mLastLongTaskEnd;
  mozilla::TimeStamp mLastLongNonIdleTaskEnd;

  mozilla::Atomic<bool> mShutdownRequired;

  int8_t mPriority;

  bool mIsMainThread;

  // Set to true if this thread creates a JSRuntime.
  bool mCanInvokeJS;

  bool mHasTLSEntry = false;

  // Used to track which event is being executed by ProcessNextEvent
  nsCOMPtr<nsIRunnable> mCurrentEvent;

  mozilla::TimeStamp mCurrentEventStart;
  mozilla::TimeStamp mNextIdleDeadline;

#ifdef EARLY_BETA_OR_EARLIER
  nsCString mNameForWakeupTelemetry;
  mozilla::TimeStamp mLastWakeupCheckTime;
  uint32_t mWakeupCount = 0;
#endif

  RefPtr<mozilla::PerformanceCounter> mCurrentPerformanceCounter;

  bool mIsInLocalExecutionMode = false;
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
  nsThreadEnumerator() : mMal(nsThread::ThreadListMutex()) {}

  auto begin() { return nsThread::ThreadList().begin(); }
  auto end() { return nsThread::ThreadList().end(); }

 private:
  mozilla::OffTheBooksMutexAutoLock mMal;
};

#if defined(XP_UNIX) && !defined(ANDROID) && !defined(DEBUG) && HAVE_UALARM && \
    defined(_GNU_SOURCE)
#  define MOZ_CANARY

extern int sCanaryOutputFD;
#endif

#endif  // nsThread_h__
