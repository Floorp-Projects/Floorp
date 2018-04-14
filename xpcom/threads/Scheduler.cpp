/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Scheduler.h"

#include "jsfriendapi.h"
#include "LabeledEventQueue.h"
#include "LeakRefPtr.h"
#include "MainThreadQueue.h"
#include "mozilla/CooperativeThreadPool.h"
#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/ipc/BackgroundChild.h"
#include "mozilla/SchedulerGroup.h"
#include "nsCycleCollector.h"
#include "nsIThread.h"
#include "nsPrintfCString.h"
#include "nsThread.h"
#include "nsThreadManager.h"
#include "PrioritizedEventQueue.h"
#include "xpcpublic.h"
#include "xpccomponents.h"

// Windows silliness. winbase.h defines an empty no-argument Yield macro.
#undef Yield

using namespace mozilla;

// Using the anonymous namespace here causes GCC to generate:
// error: 'mozilla::SchedulerImpl' has a field 'mozilla::SchedulerImpl::mQueue' whose type uses the anonymous namespace
namespace mozilla {
namespace detail {

class SchedulerEventQueue final : public SynchronizedEventQueue
{
public:
  explicit SchedulerEventQueue(UniquePtr<AbstractEventQueue> aQueue)
    : mLock("Scheduler")
    , mNonCooperativeCondVar(mLock, "SchedulerNonCoop")
    , mQueue(Move(aQueue))
    , mScheduler(nullptr)
  {}

  bool PutEvent(already_AddRefed<nsIRunnable>&& aEvent,
                EventPriority aPriority) final;

  void Disconnect(const MutexAutoLock& aProofOfLock) final {}

  already_AddRefed<nsIRunnable> GetEvent(bool aMayWait,
                                         EventPriority* aPriority) final;
  bool HasPendingEvent() final;
  bool HasPendingEvent(const MutexAutoLock& aProofOfLock);

  bool ShutdownIfNoPendingEvents() final;

  already_AddRefed<nsIThreadObserver> GetObserver() final;
  already_AddRefed<nsIThreadObserver> GetObserverOnThread() final;
  void SetObserver(nsIThreadObserver* aObserver) final;

  void EnableInputEventPrioritization() final;
  void FlushInputEventPrioritization() final;
  void SuspendInputEventPrioritization() final;
  void ResumeInputEventPrioritization() final;

  bool UseCooperativeScheduling() const;
  void SetScheduler(SchedulerImpl* aScheduler);

  Mutex& MutexRef() { return mLock; }

private:
  Mutex mLock;
  CondVar mNonCooperativeCondVar;

  // Using the actual type here would avoid a virtual dispatch. However, that
  // would prevent us from switching between EventQueue and LabeledEventQueue at
  // runtime.
  UniquePtr<AbstractEventQueue> mQueue;

  bool mEventsAreDoomed = false;
  SchedulerImpl* mScheduler;
  nsCOMPtr<nsIThreadObserver> mObserver;
};

} // namespace detail
} // namespace mozilla

using mozilla::detail::SchedulerEventQueue;

class mozilla::SchedulerImpl
{
public:
  explicit SchedulerImpl(SchedulerEventQueue* aQueue);

  void Start();
  void Stop(already_AddRefed<nsIRunnable> aStoppedCallback);
  void Shutdown();

  void Dispatch(already_AddRefed<nsIRunnable> aEvent);

  void Yield();

  static void EnterNestedEventLoop(Scheduler::EventLoopActivation& aOuterActivation);
  static void ExitNestedEventLoop(Scheduler::EventLoopActivation& aOuterActivation);

  static void StartEvent(Scheduler::EventLoopActivation& aActivation);
  static void FinishEvent(Scheduler::EventLoopActivation& aActivation);

  void SetJSContext(size_t aIndex, JSContext* aCx)
  {
    mContexts[aIndex] = aCx;
  }

  static void YieldCallback(JSContext* aCx);
  static bool InterruptCallback(JSContext* aCx);

  CooperativeThreadPool* GetThreadPool() { return mThreadPool.get(); }

  static bool UnlabeledEventRunning() { return sUnlabeledEventRunning; }
  static bool AnyEventRunning() { return sNumThreadsRunning > 0; }

  void BlockThreadedExecution(nsIBlockThreadedExecutionCallback* aCallback);
  void UnblockThreadedExecution();

  CooperativeThreadPool::Resource* GetQueueResource() { return &mQueueResource; }
  bool UseCooperativeScheduling() const { return mQueue->UseCooperativeScheduling(); }

  // Preferences.
  static bool sPrefScheduler;
  static bool sPrefChaoticScheduling;
  static bool sPrefPreemption;
  static size_t sPrefThreadCount;
  static bool sPrefUseMultipleQueues;

private:
  void Interrupt(JSContext* aCx);
  void YieldFromJS(JSContext* aCx);

  static void SwitcherThread(void* aData);
  void Switcher();

  size_t mNumThreads;

  // Protects mQueue as well as mThreadPool. The lock comes from the SchedulerEventQueue.
  Mutex& mLock;
  CondVar mShutdownCondVar;

  bool mShuttingDown;

  // Runnable to call when the scheduler has finished shutting down.
  nsTArray<nsCOMPtr<nsIRunnable>> mShutdownCallbacks;

  UniquePtr<CooperativeThreadPool> mThreadPool;

  RefPtr<SchedulerEventQueue> mQueue;

  class QueueResource : public CooperativeThreadPool::Resource
  {
  public:
    explicit QueueResource(SchedulerImpl* aScheduler)
      : mScheduler(aScheduler)
    {}

    bool IsAvailable(const MutexAutoLock& aProofOfLock) override;

  private:
    SchedulerImpl* mScheduler;
  };
  QueueResource mQueueResource;

  class SystemZoneResource : public CooperativeThreadPool::Resource
  {
  public:
    explicit SystemZoneResource(SchedulerImpl* aScheduler)
      : mScheduler(aScheduler) {}

    bool IsAvailable(const MutexAutoLock& aProofOfLock) override;

  private:
    SchedulerImpl* mScheduler;
  };
  SystemZoneResource mSystemZoneResource;

  class ThreadController : public CooperativeThreadPool::Controller
  {
  public:
    ThreadController(SchedulerImpl* aScheduler, SchedulerEventQueue* aQueue)
      : mScheduler(aScheduler)
      , mMainVirtual(GetCurrentVirtualThread())
      , mMainLoop(MessageLoop::current())
      , mMainQueue(aQueue)
    {}

    void OnStartThread(size_t aIndex, const nsACString& aName, void* aStackTop) override;
    void OnStopThread(size_t aIndex) override;

    void OnSuspendThread(size_t aIndex) override;
    void OnResumeThread(size_t aIndex) override;

  private:
    SchedulerImpl* mScheduler;
    PRThread* mMainVirtual;
    MessageLoop* mMainLoop;
    MessageLoop* mOldMainLoop;
    RefPtr<SynchronizedEventQueue> mMainQueue;
  };
  ThreadController mController;

  static size_t sNumThreadsRunning;
  static bool sUnlabeledEventRunning;

  // Number of times that BlockThreadedExecution has been called without
  // corresponding calls to UnblockThreadedExecution. If this is non-zero,
  // scheduling is disabled.
  size_t mNumSchedulerBlocks = 0;

  JSContext* mContexts[CooperativeThreadPool::kMaxThreads];
};

bool SchedulerImpl::sPrefScheduler = false;
bool SchedulerImpl::sPrefChaoticScheduling = false;
bool SchedulerImpl::sPrefPreemption = false;
bool SchedulerImpl::sPrefUseMultipleQueues = false;
size_t SchedulerImpl::sPrefThreadCount = 2;

size_t SchedulerImpl::sNumThreadsRunning;
bool SchedulerImpl::sUnlabeledEventRunning;

bool
SchedulerEventQueue::PutEvent(already_AddRefed<nsIRunnable>&& aEvent,
                              EventPriority aPriority)
{
  // We want to leak the reference when we fail to dispatch it, so that
  // we won't release the event in a wrong thread.
  LeakRefPtr<nsIRunnable> event(Move(aEvent));
  nsCOMPtr<nsIThreadObserver> obs;

  {
    MutexAutoLock lock(mLock);

    if (mEventsAreDoomed) {
      return false;
    }

    mQueue->PutEvent(event.take(), aPriority, lock);

    if (mScheduler) {
      CooperativeThreadPool* pool = mScheduler->GetThreadPool();
      MOZ_ASSERT(pool);
      pool->RecheckBlockers(lock);
    } else {
      mNonCooperativeCondVar.Notify();
    }

    // Make sure to grab the observer before dropping the lock, otherwise the
    // event that we just placed into the queue could run and eventually delete
    // this nsThread before the calling thread is scheduled again. We would then
    // crash while trying to access a dead nsThread.
    obs = mObserver;
  }

  if (obs) {
    obs->OnDispatchedEvent();
  }

  return true;
}

already_AddRefed<nsIRunnable>
SchedulerEventQueue::GetEvent(bool aMayWait,
                              EventPriority* aPriority)
{
  MutexAutoLock lock(mLock);

  if (SchedulerImpl::sPrefChaoticScheduling) {
    CooperativeThreadPool::Yield(nullptr, lock);
  }

  nsCOMPtr<nsIRunnable> event;
  for (;;) {
    event = mQueue->GetEvent(aPriority, lock);

    if (event || !aMayWait) {
      break;
    }

    if (mScheduler) {
      CooperativeThreadPool::Yield(mScheduler->GetQueueResource(), lock);
    } else {
      mNonCooperativeCondVar.Wait();
    }
  }

  return event.forget();
}

bool
SchedulerEventQueue::HasPendingEvent()
{
  MutexAutoLock lock(mLock);
  return HasPendingEvent(lock);
}

bool
SchedulerEventQueue::HasPendingEvent(const MutexAutoLock& aProofOfLock)
{
  return mQueue->HasReadyEvent(aProofOfLock);
}

bool
SchedulerEventQueue::ShutdownIfNoPendingEvents()
{
  MutexAutoLock lock(mLock);

  MOZ_ASSERT(!mScheduler);

  if (mQueue->IsEmpty(lock)) {
    mEventsAreDoomed = true;
    return true;
  }
  return false;
}

bool
SchedulerEventQueue::UseCooperativeScheduling() const
{
  MOZ_ASSERT(NS_IsMainThread());
  return !!mScheduler;
}

void
SchedulerEventQueue::SetScheduler(SchedulerImpl* aScheduler)
{
  MutexAutoLock lock(mLock);
  mScheduler = aScheduler;
}

already_AddRefed<nsIThreadObserver>
SchedulerEventQueue::GetObserver()
{
  MutexAutoLock lock(mLock);
  return do_AddRef(mObserver);
}

already_AddRefed<nsIThreadObserver>
SchedulerEventQueue::GetObserverOnThread()
{
  MOZ_ASSERT(NS_IsMainThread());
  return do_AddRef(mObserver);
}

void
SchedulerEventQueue::SetObserver(nsIThreadObserver* aObserver)
{
  MutexAutoLock lock(mLock);
  mObserver = aObserver;
}

void
SchedulerEventQueue::EnableInputEventPrioritization()
{
  MutexAutoLock lock(mLock);
  mQueue->EnableInputEventPrioritization(lock);
}

void
SchedulerEventQueue::FlushInputEventPrioritization()
{
  MutexAutoLock lock(mLock);
  mQueue->FlushInputEventPrioritization(lock);
}

void
SchedulerEventQueue::SuspendInputEventPrioritization()
{
  MutexAutoLock lock(mLock);
  mQueue->SuspendInputEventPrioritization(lock);
}

void
SchedulerEventQueue::ResumeInputEventPrioritization()
{
  MutexAutoLock lock(mLock);
  mQueue->ResumeInputEventPrioritization(lock);
}

UniquePtr<SchedulerImpl> Scheduler::sScheduler;

SchedulerImpl::SchedulerImpl(SchedulerEventQueue* aQueue)
  : mNumThreads(sPrefThreadCount)
  , mLock(aQueue->MutexRef())
  , mShutdownCondVar(aQueue->MutexRef(), "SchedulerImpl")
  , mShuttingDown(false)
  , mQueue(aQueue)
  , mQueueResource(this)
  , mSystemZoneResource(this)
  , mController(this, aQueue)
  , mContexts()
{
}

void
SchedulerImpl::Interrupt(JSContext* aCx)
{
  MutexAutoLock lock(mLock);
  CooperativeThreadPool::Yield(nullptr, lock);
}

/* static */ bool
SchedulerImpl::InterruptCallback(JSContext* aCx)
{
  Scheduler::sScheduler->Interrupt(aCx);
  return true;
}

void
SchedulerImpl::YieldFromJS(JSContext* aCx)
{
  MutexAutoLock lock(mLock);
  CooperativeThreadPool::Yield(&mSystemZoneResource, lock);
}

/* static */ void
SchedulerImpl::YieldCallback(JSContext* aCx)
{
  Scheduler::sScheduler->YieldFromJS(aCx);
}

void
SchedulerImpl::Switcher()
{
  // This thread switcher is extremely basic and only meant for testing. The
  // goal is to switch as much as possible without regard for performance.

  MutexAutoLock lock(mLock);
  while (!mShuttingDown) {
    CooperativeThreadPool::SelectedThread threadIndex = mThreadPool->CurrentThreadIndex(lock);
    if (threadIndex.is<size_t>()) {
      JSContext* cx = mContexts[threadIndex.as<size_t>()];
      if (cx) {
        JS_RequestInterruptCallbackCanWait(cx);
      }
    }

    mShutdownCondVar.Wait(TimeDuration::FromMicroseconds(50));
  }
}

/* static */ void
SchedulerImpl::SwitcherThread(void* aData)
{
  static_cast<SchedulerImpl*>(aData)->Switcher();
}

void
SchedulerImpl::Start()
{
  MOZ_ASSERT(mNumSchedulerBlocks == 0);

  NS_DispatchToMainThread(NS_NewRunnableFunction("Scheduler::Start", [this]() -> void {
    // Let's pretend the runnable here isn't actually running.
    MOZ_ASSERT(sUnlabeledEventRunning);
    sUnlabeledEventRunning = false;
    MOZ_ASSERT(sNumThreadsRunning == 1);
    sNumThreadsRunning = 0;

    mQueue->SetScheduler(this);

    xpc::YieldCooperativeContext();

    mThreadPool = MakeUnique<CooperativeThreadPool>(mNumThreads, mLock,
                                                    mController);

    PRThread* switcher = nullptr;
    if (sPrefPreemption) {
      switcher = PR_CreateThread(PR_USER_THREAD,
                                 SwitcherThread,
                                 this,
                                 PR_PRIORITY_HIGH,
                                 PR_GLOBAL_THREAD,
                                 PR_JOINABLE_THREAD,
                                 0);
    }

    {
      MutexAutoLock mutex(mLock);
      while (!mShuttingDown) {
        mShutdownCondVar.Wait();
      }
    }

    if (switcher) {
      PR_JoinThread(switcher);
    }

    mThreadPool->Shutdown();
    mThreadPool = nullptr;

    mQueue->SetScheduler(nullptr);

    xpc::ResumeCooperativeContext();

    // Put things back to the way they were before we started scheduling.
    MOZ_ASSERT(!sUnlabeledEventRunning);
    sUnlabeledEventRunning = true;
    MOZ_ASSERT(sNumThreadsRunning == 0);
    sNumThreadsRunning = 1;

    mShuttingDown = false;
    nsTArray<nsCOMPtr<nsIRunnable>> callbacks = Move(mShutdownCallbacks);
    for (nsIRunnable* runnable : callbacks) {
      runnable->Run();
    }
  }));
}

void
SchedulerImpl::Stop(already_AddRefed<nsIRunnable> aStoppedCallback)
{
  MOZ_ASSERT(mNumSchedulerBlocks > 0);

  // Note that this may be called when mShuttingDown is already true. We still
  // want to invoke the callback in that case.

  MutexAutoLock lock(mLock);
  mShuttingDown = true;
  mShutdownCallbacks.AppendElement(aStoppedCallback);
  mShutdownCondVar.Notify();
}

void
SchedulerImpl::Shutdown()
{
  MOZ_ASSERT(mNumSchedulerBlocks == 0);

  MutexAutoLock lock(mLock);
  mShuttingDown = true;

  // Delete the SchedulerImpl once shutdown is complete.
  mShutdownCallbacks.AppendElement(NS_NewRunnableFunction("SchedulerImpl::Shutdown",
                                                          [] { Scheduler::sScheduler = nullptr; }));

  mShutdownCondVar.Notify();
}

bool
SchedulerImpl::QueueResource::IsAvailable(const MutexAutoLock& aProofOfLock)
{
  mScheduler->mLock.AssertCurrentThreadOwns();

  RefPtr<SchedulerEventQueue> queue = mScheduler->mQueue;
  return queue->HasPendingEvent(aProofOfLock);
}

bool
SchedulerImpl::SystemZoneResource::IsAvailable(const MutexAutoLock& aProofOfLock)
{
  mScheduler->mLock.AssertCurrentThreadOwns();

  // It doesn't matter which context we pick; we really just some main-thread
  // JSContext.
  JSContext* cx = mScheduler->mContexts[0];
  return js::SystemZoneAvailable(cx);
}

MOZ_THREAD_LOCAL(Scheduler::EventLoopActivation*) Scheduler::EventLoopActivation::sTopActivation;

/* static */ void
Scheduler::EventLoopActivation::Init()
{
  sTopActivation.infallibleInit();
}

Scheduler::EventLoopActivation::EventLoopActivation()
  : mPrev(sTopActivation.get())
  , mProcessingEvent(false)
  , mIsLabeled(false)
{
  sTopActivation.set(this);

  if (mPrev && mPrev->mProcessingEvent) {
    SchedulerImpl::EnterNestedEventLoop(*mPrev);
  }
}

Scheduler::EventLoopActivation::~EventLoopActivation()
{
  if (mProcessingEvent) {
    SchedulerImpl::FinishEvent(*this);
  }

  MOZ_ASSERT(sTopActivation.get() == this);
  sTopActivation.set(mPrev);

  if (mPrev && mPrev->mProcessingEvent) {
    SchedulerImpl::ExitNestedEventLoop(*mPrev);
  }
}

/* static */ void
SchedulerImpl::StartEvent(Scheduler::EventLoopActivation& aActivation)
{
  MOZ_ASSERT(!sUnlabeledEventRunning);
  if (aActivation.IsLabeled()) {
    SchedulerGroup::SetValidatingAccess(SchedulerGroup::StartValidation);
    aActivation.EventGroupsAffected().SetIsRunning(true);
  } else {
    sUnlabeledEventRunning = true;
  }
  sNumThreadsRunning++;
}

/* static */ void
SchedulerImpl::FinishEvent(Scheduler::EventLoopActivation& aActivation)
{
  if (aActivation.IsLabeled()) {
    aActivation.EventGroupsAffected().SetIsRunning(false);
    SchedulerGroup::SetValidatingAccess(SchedulerGroup::EndValidation);
  } else {
    MOZ_ASSERT(sUnlabeledEventRunning);
    sUnlabeledEventRunning = false;
  }

  MOZ_ASSERT(sNumThreadsRunning > 0);
  sNumThreadsRunning--;
}

// When we enter a nested event loop, we act as if the outer event loop's event
// finished. When we exit the nested event loop, we "resume" the outer event
// loop's event.
/* static */ void
SchedulerImpl::EnterNestedEventLoop(Scheduler::EventLoopActivation& aOuterActivation)
{
  FinishEvent(aOuterActivation);
}

/* static */ void
SchedulerImpl::ExitNestedEventLoop(Scheduler::EventLoopActivation& aOuterActivation)
{
  StartEvent(aOuterActivation);
}

void
Scheduler::EventLoopActivation::SetEvent(nsIRunnable* aEvent,
                                         EventPriority aPriority)
{
  if (nsCOMPtr<nsILabelableRunnable> labelable = do_QueryInterface(aEvent)) {
    if (labelable->GetAffectedSchedulerGroups(mEventGroups)) {
      mIsLabeled = true;
    }
  }

  mPriority = aPriority;
  mProcessingEvent = aEvent != nullptr;

  if (aEvent) {
    SchedulerImpl::StartEvent(*this);
  }
}

void
SchedulerImpl::ThreadController::OnStartThread(size_t aIndex, const nsACString& aName, void* aStackTop)
{
  using mozilla::ipc::BackgroundChild;

  // Causes GetCurrentVirtualThread() to return mMainVirtual and NS_IsMainThread()
  // to return true.
  NS_SetMainThread(mMainVirtual);

  // This will initialize the thread's mVirtualThread to mMainVirtual since
  // GetCurrentVirtualThread() now returns mMainVirtual.
  nsThreadManager::get().CreateCurrentThread(mMainQueue, nsThread::MAIN_THREAD);

  PROFILER_REGISTER_THREAD(aName.BeginReading());

  mOldMainLoop = MessageLoop::current();

  MessageLoop::set_current(mMainLoop);

  xpc::CreateCooperativeContext();

  JSContext* cx = dom::danger::GetJSContext();
  mScheduler->SetJSContext(aIndex, cx);
  if (sPrefPreemption) {
    JS_AddInterruptCallback(cx, SchedulerImpl::InterruptCallback);
  }
  Servo_InitializeCooperativeThread();
}

void
SchedulerImpl::ThreadController::OnStopThread(size_t aIndex)
{
  xpc::DestroyCooperativeContext();

  NS_UnsetMainThread();
  MessageLoop::set_current(mOldMainLoop);

  RefPtr<nsThread> self = static_cast<nsThread*>(NS_GetCurrentThread());
  nsThreadManager::get().UnregisterCurrentThread(*self);

  PROFILER_UNREGISTER_THREAD();
}

void
SchedulerImpl::ThreadController::OnSuspendThread(size_t aIndex)
{
  xpc::YieldCooperativeContext();
}

void
SchedulerImpl::ThreadController::OnResumeThread(size_t aIndex)
{
  xpc::ResumeCooperativeContext();
}

void
SchedulerImpl::Yield()
{
  MutexAutoLock lock(mLock);
  CooperativeThreadPool::Yield(nullptr, lock);
}

void
SchedulerImpl::BlockThreadedExecution(nsIBlockThreadedExecutionCallback* aCallback)
{
  if (mNumSchedulerBlocks++ == 0 || mShuttingDown) {
    Stop(NewRunnableMethod("BlockThreadedExecution", aCallback,
                           &nsIBlockThreadedExecutionCallback::Callback));
  } else {
    // The scheduler is already blocked.
    nsCOMPtr<nsIBlockThreadedExecutionCallback> kungFuDeathGrip(aCallback);
    aCallback->Callback();
  }
}

void
SchedulerImpl::UnblockThreadedExecution()
{
  if (--mNumSchedulerBlocks == 0) {
    Start();
  }
}

/* static */ already_AddRefed<nsThread>
Scheduler::Init(nsIIdlePeriod* aIdlePeriod)
{
  MOZ_ASSERT(!sScheduler);

  RefPtr<SchedulerEventQueue> queue;
  RefPtr<nsThread> mainThread;
  if (Scheduler::UseMultipleQueues()) {
    mainThread = CreateMainThread<SchedulerEventQueue, LabeledEventQueue>(aIdlePeriod, getter_AddRefs(queue));
  } else {
    mainThread = CreateMainThread<SchedulerEventQueue, EventQueue>(aIdlePeriod, getter_AddRefs(queue));
  }

  sScheduler = MakeUnique<SchedulerImpl>(queue);
  return mainThread.forget();
}

/* static */ void
Scheduler::Start()
{
  sScheduler->Start();
}

/* static */ void
Scheduler::Shutdown()
{
  if (sScheduler) {
    sScheduler->Shutdown();
  }
}

/* static */ nsCString
Scheduler::GetPrefs()
{
  MOZ_ASSERT(XRE_IsParentProcess());
  nsPrintfCString result("%d%d%d%d,%d",
                         Preferences::GetBool("dom.ipc.scheduler",
                                              SchedulerImpl::sPrefScheduler),
                         Preferences::GetBool("dom.ipc.scheduler.chaoticScheduling",
                                              SchedulerImpl::sPrefChaoticScheduling),
                         Preferences::GetBool("dom.ipc.scheduler.preemption",
                                              SchedulerImpl::sPrefPreemption),
                         Preferences::GetBool("dom.ipc.scheduler.useMultipleQueues",
                                              SchedulerImpl::sPrefUseMultipleQueues),
                         Preferences::GetInt("dom.ipc.scheduler.threadCount",
                                             SchedulerImpl::sPrefThreadCount));

  return result;
}

/* static */ void
Scheduler::SetPrefs(const char* aPrefs)
{
  MOZ_ASSERT(XRE_IsContentProcess());

  // If the prefs weren't sent to this process, use the default values.
  if (!aPrefs) {
    return;
  }

  // If the pref string appears truncated, use the default values.
  if (strlen(aPrefs) < 6) {
    return;
  }

  SchedulerImpl::sPrefScheduler = aPrefs[0] == '1';
  SchedulerImpl::sPrefChaoticScheduling = aPrefs[1] == '1';
  SchedulerImpl::sPrefPreemption = aPrefs[2] == '1';
  SchedulerImpl::sPrefUseMultipleQueues = aPrefs[3] == '1';
  MOZ_ASSERT(aPrefs[4] == ',');
  SchedulerImpl::sPrefThreadCount = atoi(aPrefs + 5);
}

/* static */ bool
Scheduler::IsSchedulerEnabled()
{
  return SchedulerImpl::sPrefScheduler;
}

/* static */ bool
Scheduler::UseMultipleQueues()
{
  return SchedulerImpl::sPrefUseMultipleQueues;
}

/* static */ bool
Scheduler::IsCooperativeThread()
{
  return CooperativeThreadPool::IsCooperativeThread();
}

/* static */ void
Scheduler::Yield()
{
  sScheduler->Yield();
}

/* static */ bool
Scheduler::UnlabeledEventRunning()
{
  return SchedulerImpl::UnlabeledEventRunning();
}

/* static */ bool
Scheduler::AnyEventRunning()
{
  return SchedulerImpl::AnyEventRunning();
}

/* static */ void
Scheduler::BlockThreadedExecution(nsIBlockThreadedExecutionCallback* aCallback)
{
  if (!sScheduler) {
    nsCOMPtr<nsIBlockThreadedExecutionCallback> kungFuDeathGrip(aCallback);
    aCallback->Callback();
    return;
  }

  sScheduler->BlockThreadedExecution(aCallback);
}

/* static */ void
Scheduler::UnblockThreadedExecution()
{
  if (!sScheduler) {
    return;
  }

  sScheduler->UnblockThreadedExecution();
}
