/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsThreadManager.h"
#include "nsThread.h"
#include "nsThreadPool.h"
#include "nsThreadUtils.h"
#include "nsIClassInfoImpl.h"
#include "nsExceptionHandler.h"
#include "nsTArray.h"
#include "nsXULAppAPI.h"
#include "nsExceptionHandler.h"
#include "mozilla/AbstractThread.h"
#include "mozilla/AppShutdown.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/CycleCollectedJSContext.h"  // nsAutoMicroTask
#include "mozilla/EventQueue.h"
#include "mozilla/InputTaskManager.h"
#include "mozilla/Mutex.h"
#include "mozilla/NeverDestroyed.h"
#include "mozilla/Preferences.h"
#include "mozilla/ProfilerMarkers.h"
#include "mozilla/SpinEventLoopUntil.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/TaskQueue.h"
#include "mozilla/ThreadEventQueue.h"
#include "mozilla/ThreadLocal.h"
#include "TaskController.h"
#include "ThreadEventTarget.h"
#ifdef MOZ_CANARY
#  include <fcntl.h>
#  include <unistd.h>
#endif

#include "MainThreadIdlePeriod.h"

using namespace mozilla;

static MOZ_THREAD_LOCAL(bool) sTLSIsMainThread;

bool NS_IsMainThreadTLSInitialized() { return sTLSIsMainThread.initialized(); }

class BackgroundEventTarget final : public nsIEventTarget,
                                    public TaskQueueTracker {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIEVENTTARGET_FULL

  BackgroundEventTarget() = default;

  nsresult Init();

  already_AddRefed<TaskQueue> CreateBackgroundTaskQueue(const char* aName);

  void BeginShutdown(nsTArray<RefPtr<ShutdownPromise>>&);
  void FinishShutdown();

 private:
  ~BackgroundEventTarget() = default;

  nsCOMPtr<nsIThreadPool> mPool;
  nsCOMPtr<nsIThreadPool> mIOPool;
};

NS_IMPL_ISUPPORTS(BackgroundEventTarget, nsIEventTarget, TaskQueueTracker)

nsresult BackgroundEventTarget::Init() {
  nsCOMPtr<nsIThreadPool> pool(new nsThreadPool());
  NS_ENSURE_TRUE(pool, NS_ERROR_FAILURE);

  nsresult rv = pool->SetName("BackgroundThreadPool"_ns);
  NS_ENSURE_SUCCESS(rv, rv);

  // Use potentially more conservative stack size.
  rv = pool->SetThreadStackSize(nsIThreadManager::kThreadPoolStackSize);
  NS_ENSURE_SUCCESS(rv, rv);

  // Thread limit of 2 makes deadlock during synchronous dispatch less likely.
  rv = pool->SetThreadLimit(2);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = pool->SetIdleThreadLimit(1);
  NS_ENSURE_SUCCESS(rv, rv);

  // Leave threads alive for up to 5 minutes
  rv = pool->SetIdleThreadTimeout(300000);
  NS_ENSURE_SUCCESS(rv, rv);

  // Initialize the background I/O event target.
  nsCOMPtr<nsIThreadPool> ioPool(new nsThreadPool());
  NS_ENSURE_TRUE(pool, NS_ERROR_FAILURE);

  // The io pool spends a lot of its time blocking on io, so we want to offload
  // these jobs on a lower priority if available.
  rv = ioPool->SetQoSForThreads(nsIThread::QOS_PRIORITY_LOW);
  NS_ENSURE_SUCCESS(
      rv, rv);  // note: currently infallible, keeping this for brevity.

  rv = ioPool->SetName("BgIOThreadPool"_ns);
  NS_ENSURE_SUCCESS(rv, rv);

  // Use potentially more conservative stack size.
  rv = ioPool->SetThreadStackSize(nsIThreadManager::kThreadPoolStackSize);
  NS_ENSURE_SUCCESS(rv, rv);

  // Thread limit of 4 makes deadlock during synchronous dispatch less likely.
  rv = ioPool->SetThreadLimit(4);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = ioPool->SetIdleThreadLimit(1);
  NS_ENSURE_SUCCESS(rv, rv);

  // Leave threads alive for up to 5 minutes
  rv = ioPool->SetIdleThreadTimeout(300000);
  NS_ENSURE_SUCCESS(rv, rv);

  pool.swap(mPool);
  ioPool.swap(mIOPool);

  return NS_OK;
}

NS_IMETHODIMP_(bool)
BackgroundEventTarget::IsOnCurrentThreadInfallible() {
  return mPool->IsOnCurrentThread() || mIOPool->IsOnCurrentThread();
}

NS_IMETHODIMP
BackgroundEventTarget::IsOnCurrentThread(bool* aValue) {
  bool value = false;
  if (NS_SUCCEEDED(mPool->IsOnCurrentThread(&value)) && value) {
    *aValue = value;
    return NS_OK;
  }
  return mIOPool->IsOnCurrentThread(aValue);
}

NS_IMETHODIMP
BackgroundEventTarget::Dispatch(already_AddRefed<nsIRunnable> aRunnable,
                                uint32_t aFlags) {
  // We need to be careful here, because if an event is getting dispatched here
  // from within TaskQueue::Runner::Run, it will be dispatched with
  // NS_DISPATCH_AT_END, but we might not be running the event on the same
  // pool, depending on which pool we were on and the dispatch flags.  If we
  // dispatch an event with NS_DISPATCH_AT_END to the wrong pool, the pool
  // may not process the event in a timely fashion, which can lead to deadlock.
  uint32_t flags = aFlags & ~NS_DISPATCH_EVENT_MAY_BLOCK;
  bool mayBlock = bool(aFlags & NS_DISPATCH_EVENT_MAY_BLOCK);
  nsCOMPtr<nsIThreadPool>& pool = mayBlock ? mIOPool : mPool;

  // If we're already running on the pool we want to dispatch to, we can
  // unconditionally add NS_DISPATCH_AT_END to indicate that we shouldn't spin
  // up a new thread.
  //
  // Otherwise, we should remove NS_DISPATCH_AT_END so we don't run into issues
  // like those in the above comment.
  if (pool->IsOnCurrentThread()) {
    flags |= NS_DISPATCH_AT_END;
  } else {
    flags &= ~NS_DISPATCH_AT_END;
  }

  return pool->Dispatch(std::move(aRunnable), flags);
}

NS_IMETHODIMP
BackgroundEventTarget::DispatchFromScript(nsIRunnable* aRunnable,
                                          uint32_t aFlags) {
  nsCOMPtr<nsIRunnable> runnable(aRunnable);
  return Dispatch(runnable.forget(), aFlags);
}

NS_IMETHODIMP
BackgroundEventTarget::DelayedDispatch(already_AddRefed<nsIRunnable> aRunnable,
                                       uint32_t) {
  nsCOMPtr<nsIRunnable> dropRunnable(aRunnable);
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
BackgroundEventTarget::RegisterShutdownTask(nsITargetShutdownTask* aTask) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
BackgroundEventTarget::UnregisterShutdownTask(nsITargetShutdownTask* aTask) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

void BackgroundEventTarget::BeginShutdown(
    nsTArray<RefPtr<ShutdownPromise>>& promises) {
  auto queues = GetAllTrackedTaskQueues();
  for (auto& queue : queues) {
    promises.AppendElement(queue->BeginShutdown());
  }
}

void BackgroundEventTarget::FinishShutdown() {
  mPool->Shutdown();
  mIOPool->Shutdown();
}

already_AddRefed<TaskQueue> BackgroundEventTarget::CreateBackgroundTaskQueue(
    const char* aName) {
  return TaskQueue::Create(do_AddRef(this), aName).forget();
}

extern "C" {
// This uses the C language linkage because it's exposed to Rust
// via the xpcom/rust/moz_task crate.
bool NS_IsMainThread() { return sTLSIsMainThread.get(); }
}

void NS_SetMainThread() {
  if (!sTLSIsMainThread.init()) {
    MOZ_CRASH();
  }
  sTLSIsMainThread.set(true);
  MOZ_ASSERT(NS_IsMainThread());
  // We initialize the SerialEventTargetGuard's TLS here for simplicity as it
  // needs to be initialized around the same time you would initialize
  // sTLSIsMainThread.
  SerialEventTargetGuard::InitTLS();
  nsThreadPool::InitTLS();
}

#ifdef DEBUG

namespace mozilla {

void AssertIsOnMainThread() { MOZ_ASSERT(NS_IsMainThread(), "Wrong thread!"); }

}  // namespace mozilla

#endif

//-----------------------------------------------------------------------------

/* static */
void nsThreadManager::ReleaseThread(void* aData) {
  static_cast<nsThread*>(aData)->Release();
}

// statically allocated instance
NS_IMETHODIMP_(MozExternalRefCountType)
nsThreadManager::AddRef() { return 2; }
NS_IMETHODIMP_(MozExternalRefCountType)
nsThreadManager::Release() { return 1; }
NS_IMPL_CLASSINFO(nsThreadManager, nullptr,
                  nsIClassInfo::THREADSAFE | nsIClassInfo::SINGLETON,
                  NS_THREADMANAGER_CID)
NS_IMPL_QUERY_INTERFACE_CI(nsThreadManager, nsIThreadManager)
NS_IMPL_CI_INTERFACE_GETTER(nsThreadManager, nsIThreadManager)

//-----------------------------------------------------------------------------

/*static*/ nsThreadManager& nsThreadManager::get() {
  static NeverDestroyed<nsThreadManager> sInstance;
  return *sInstance;
}

nsThreadManager::nsThreadManager()
    : mCurThreadIndex(0),
      mMutex("nsThreadManager::mMutex"),
      mState(State::eUninit) {}

nsThreadManager::~nsThreadManager() = default;

nsresult nsThreadManager::Init() {
  // Child processes need to initialize the thread manager before they
  // initialize XPCOM in order to set up the crash reporter. This leads to
  // situations where we get initialized twice.
  {
    OffTheBooksMutexAutoLock lock(mMutex);
    if (mState > State::eUninit) {
      return NS_OK;
    }
  }

  if (PR_NewThreadPrivateIndex(&mCurThreadIndex, ReleaseThread) == PR_FAILURE) {
    return NS_ERROR_FAILURE;
  }

#ifdef MOZ_CANARY
  const int flags = O_WRONLY | O_APPEND | O_CREAT | O_NONBLOCK;
  const mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
  char* env_var_flag = getenv("MOZ_KILL_CANARIES");
  sCanaryOutputFD =
      env_var_flag
          ? (env_var_flag[0] ? open(env_var_flag, flags, mode) : STDERR_FILENO)
          : 0;
#endif

  TaskController::Initialize();

  // Initialize idle handling.
  nsCOMPtr<nsIIdlePeriod> idlePeriod = new MainThreadIdlePeriod();
  TaskController::Get()->SetIdleTaskManager(
      new IdleTaskManager(idlePeriod.forget()));

  // Create main thread queue that forwards events to TaskController and
  // construct main thread.
  UniquePtr<EventQueue> queue = MakeUnique<EventQueue>(true);

  RefPtr<ThreadEventQueue> synchronizedQueue =
      new ThreadEventQueue(std::move(queue), true);

  mMainThread =
      new nsThread(WrapNotNull(synchronizedQueue), nsThread::MAIN_THREAD,
                   {0, false, false, Some(W3_LONGTASK_BUSY_WINDOW_MS)});

  nsresult rv = mMainThread->InitCurrentThread();
  if (NS_FAILED(rv)) {
    mMainThread = nullptr;
    return rv;
  }
#ifdef MOZ_MEMORY
  jemalloc_set_main_thread();
#endif

  // Init AbstractThread.
  AbstractThread::InitTLS();
  AbstractThread::InitMainThread();

  // Initialize the background event target.
  RefPtr<BackgroundEventTarget> target(new BackgroundEventTarget());

  rv = target->Init();
  NS_ENSURE_SUCCESS(rv, rv);

  {
    OffTheBooksMutexAutoLock lock(mMutex);

    mBackgroundEventTarget = std::move(target);

    mState = State::eActive;
  }

  return NS_OK;
}

void nsThreadManager::ShutdownNonMainThreads() {
  MOZ_ASSERT(NS_IsMainThread(), "shutdown not called from main thread");

  // Empty the main thread event queue before we begin shutting down threads.
  NS_ProcessPendingEvents(mMainThread);

  mMainThread->mEvents->RunShutdownTasks();

  RefPtr<BackgroundEventTarget> backgroundEventTarget;
  {
    OffTheBooksMutexAutoLock lock(mMutex);
    MOZ_ASSERT(mState == State::eActive, "shutdown called multiple times");
    backgroundEventTarget = mBackgroundEventTarget;
  }

  nsTArray<RefPtr<ShutdownPromise>> promises;
  backgroundEventTarget->BeginShutdown(promises);

  bool taskQueuesShutdown = false;
  // It's fine to capture everything by reference in the Then handler since it
  // runs before we exit the nested event loop, thanks to the SpinEventLoopUntil
  // below.
  ShutdownPromise::All(mMainThread, promises)->Then(mMainThread, __func__, [&] {
    backgroundEventTarget->FinishShutdown();
    taskQueuesShutdown = true;
  });

  // Wait for task queues to shutdown, so we don't shut down the underlying
  // threads of the background event target in the block below, thereby
  // preventing the task queues from emptying, preventing the shutdown promises
  // from resolving, and prevent anything checking `taskQueuesShutdown` from
  // working.
  mozilla::SpinEventLoopUntil(
      "nsThreadManager::Shutdown"_ns, [&]() { return taskQueuesShutdown; },
      mMainThread);

  {
    // Prevent new nsThreads from being created, and collect a list of threads
    // which need to be shut down.
    //
    // We don't prevent new thread creation until we've shut down background
    // task queues, to ensure that they are able to start thread pool threads
    // for shutdown tasks.
    nsTArray<RefPtr<nsThread>> threadsToShutdown;
    {
      OffTheBooksMutexAutoLock lock(mMutex);
      mState = State::eShutdown;

      for (auto* thread : mThreadList) {
        if (thread->ShutdownRequired()) {
          threadsToShutdown.AppendElement(thread);
        }
      }
    }

    // It's tempting to walk the list of threads here and tell them each to stop
    // accepting new events, but that could lead to badness if one of those
    // threads is stuck waiting for a response from another thread.  To do it
    // right, we'd need some way to interrupt the threads.
    //
    // Instead, we process events on the current thread while waiting for
    // threads to shutdown.  This means that we have to preserve a mostly
    // functioning world until such time as the threads exit.

    // As we're going to be waiting for all asynchronous shutdowns below, we
    // can begin asynchronously shutting down all XPCOM threads here, rather
    // than shutting each thread down one-at-a-time.
    for (const auto& thread : threadsToShutdown) {
      thread->AsyncShutdown();
    }
  }

  // NB: It's possible that there are events in the queue that want to *start*
  // an asynchronous shutdown. But we have already started async shutdown of
  // the threads above, so there's no need to worry about them. We only have to
  // wait for all in-flight asynchronous thread shutdowns to complete.
  mMainThread->WaitForAllAsynchronousShutdowns();

  // There are no more background threads at this point.
}

void nsThreadManager::ShutdownMainThread() {
#ifdef DEBUG
  {
    OffTheBooksMutexAutoLock lock(mMutex);
    MOZ_ASSERT(mState == State::eShutdown, "Must have called BeginShutdown");
  }
#endif

  // Do NS_ProcessPendingEvents but with special handling to set
  // mEventsAreDoomed atomically with the removal of the last event. This means
  // that PutEvent cannot succeed if the event would be left in the main thread
  // queue after our final call to NS_ProcessPendingEvents.
  // See comments in `nsThread::ThreadFunc` for a more detailed explanation.
  while (true) {
    if (mMainThread->mEvents->ShutdownIfNoPendingEvents()) {
      break;
    }
    NS_ProcessPendingEvents(mMainThread);
  }

  // Normally thread shutdown clears the observer for the thread, but since the
  // main thread is special we do it manually here after we're sure all events
  // have been processed.
  mMainThread->SetObserver(nullptr);

  OffTheBooksMutexAutoLock lock(mMutex);
  mBackgroundEventTarget = nullptr;
}

void nsThreadManager::ReleaseMainThread() {
#ifdef DEBUG
  {
    OffTheBooksMutexAutoLock lock(mMutex);
    MOZ_ASSERT(mState == State::eShutdown, "Must have called BeginShutdown");
    MOZ_ASSERT(!mBackgroundEventTarget, "Must have called ShutdownMainThread");
  }
#endif
  MOZ_ASSERT(mMainThread);

  // Release main thread object.
  mMainThread = nullptr;

  // Remove the TLS entry for the main thread.
  PR_SetThreadPrivate(mCurThreadIndex, nullptr);
}

void nsThreadManager::RegisterCurrentThread(nsThread& aThread) {
  MOZ_ASSERT(aThread.GetPRThread() == PR_GetCurrentThread(), "bad aThread");

  aThread.AddRef();  // for TLS entry
  PR_SetThreadPrivate(mCurThreadIndex, &aThread);

#ifdef DEBUG
  {
    OffTheBooksMutexAutoLock lock(mMutex);
    MOZ_ASSERT(aThread.isInList(),
               "Thread was not added to the thread list before registering!");
  }
#endif
}

void nsThreadManager::UnregisterCurrentThread(nsThread& aThread) {
  MOZ_ASSERT(aThread.GetPRThread() == PR_GetCurrentThread(), "bad aThread");

  PR_SetThreadPrivate(mCurThreadIndex, nullptr);
  // Ref-count balanced via ReleaseThread
}

// Not to be used for MainThread!
nsThread* nsThreadManager::CreateCurrentThread(SynchronizedEventQueue* aQueue) {
  // Make sure we don't have an nsThread yet.
  MOZ_ASSERT(!PR_GetThreadPrivate(mCurThreadIndex));

  if (!AllowNewXPCOMThreads()) {
    return nullptr;
  }

  RefPtr<nsThread> thread = new nsThread(
      WrapNotNull(aQueue), nsThread::NOT_MAIN_THREAD, {.stackSize = 0});
  if (NS_FAILED(thread->InitCurrentThread())) {
    return nullptr;
  }

  return thread.get();  // reference held in TLS
}

nsresult nsThreadManager::DispatchToBackgroundThread(nsIRunnable* aEvent,
                                                     uint32_t aDispatchFlags) {
  RefPtr<BackgroundEventTarget> backgroundTarget;
  {
    OffTheBooksMutexAutoLock lock(mMutex);
    if (!AllowNewXPCOMThreadsLocked() || !mBackgroundEventTarget) {
      return NS_ERROR_FAILURE;
    }
    backgroundTarget = mBackgroundEventTarget;
  }

  return backgroundTarget->Dispatch(aEvent, aDispatchFlags);
}

already_AddRefed<TaskQueue> nsThreadManager::CreateBackgroundTaskQueue(
    const char* aName) {
  RefPtr<BackgroundEventTarget> backgroundTarget;
  {
    OffTheBooksMutexAutoLock lock(mMutex);
    if (!AllowNewXPCOMThreadsLocked() || !mBackgroundEventTarget) {
      return nullptr;
    }
    backgroundTarget = mBackgroundEventTarget;
  }

  return backgroundTarget->CreateBackgroundTaskQueue(aName);
}

nsThread* nsThreadManager::GetCurrentThread() {
  // read thread local storage
  void* data = PR_GetThreadPrivate(mCurThreadIndex);
  if (data) {
    return static_cast<nsThread*>(data);
  }

  // Keep this function working early during startup or late during shutdown on
  // the main thread.
  if (!AllowNewXPCOMThreads() || NS_IsMainThread()) {
    return nullptr;
  }

  // OK, that's fine.  We'll dynamically create one :-)
  //
  // We assume that if we're implicitly creating a thread here that it doesn't
  // want an event queue. Any thread which wants an event queue should
  // explicitly create its nsThread wrapper.
  //
  // nsThread::InitCurrentThread() will check AllowNewXPCOMThreads, and return
  // an error if we're too late in shutdown to create new XPCOM threads.
  RefPtr<nsThread> thread = new nsThread();
  if (NS_FAILED(thread->InitCurrentThread())) {
    return nullptr;
  }

  return thread.get();  // reference held in TLS
}

bool nsThreadManager::IsNSThread() const {
  {
    OffTheBooksMutexAutoLock lock(mMutex);
    if (mState == State::eUninit) {
      return false;
    }
  }
  if (auto* thread = (nsThread*)PR_GetThreadPrivate(mCurThreadIndex)) {
    return thread->EventQueue();
  }
  return false;
}

NS_IMETHODIMP
nsThreadManager::NewNamedThread(
    const nsACString& aName, nsIThreadManager::ThreadCreationOptions aOptions,
    nsIThread** aResult) {
  // Note: can be called from arbitrary threads

  [[maybe_unused]] TimeStamp startTime = TimeStamp::Now();

  RefPtr<ThreadEventQueue> queue =
      new ThreadEventQueue(MakeUnique<EventQueue>());
  RefPtr<nsThread> thr =
      new nsThread(WrapNotNull(queue), nsThread::NOT_MAIN_THREAD, aOptions);

  // Note: nsThread::Init() will check AllowNewXPCOMThreads, and return an
  // error if we're too late in shutdown to create new XPCOM threads. If we
  // aren't, the thread will be synchronously added to mThreadList.
  nsresult rv = thr->Init(aName);
  if (NS_FAILED(rv)) {
    return rv;
  }

  PROFILER_MARKER_TEXT(
      "NewThread", OTHER,
      MarkerOptions(MarkerStack::Capture(),
                    MarkerTiming::IntervalUntilNowFrom(startTime)),
      aName);
  if (!NS_IsMainThread()) {
    PROFILER_MARKER_TEXT(
        "NewThread (non-main thread)", OTHER,
        MarkerOptions(MarkerStack::Capture(), MarkerThreadId::MainThread(),
                      MarkerTiming::IntervalUntilNowFrom(startTime)),
        aName);
  }

  thr.forget(aResult);
  return NS_OK;
}

NS_IMETHODIMP
nsThreadManager::GetMainThread(nsIThread** aResult) {
  // Keep this functioning during Shutdown
  if (!mMainThread) {
    if (!NS_IsMainThread()) {
      NS_WARNING(
          "Called GetMainThread but there isn't a main thread and "
          "we're not the main thread.");
    }
    return NS_ERROR_NOT_INITIALIZED;
  }
  NS_ADDREF(*aResult = mMainThread);
  return NS_OK;
}

NS_IMETHODIMP
nsThreadManager::GetCurrentThread(nsIThread** aResult) {
  // Keep this functioning during Shutdown
  if (!mMainThread) {
    return NS_ERROR_NOT_INITIALIZED;
  }
  *aResult = GetCurrentThread();
  if (!*aResult) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  NS_ADDREF(*aResult);
  return NS_OK;
}

NS_IMETHODIMP
nsThreadManager::SpinEventLoopUntil(const nsACString& aVeryGoodReasonToDoThis,
                                    nsINestedEventLoopCondition* aCondition) {
  return SpinEventLoopUntilInternal(aVeryGoodReasonToDoThis, aCondition,
                                    ShutdownPhase::NotInShutdown);
}

NS_IMETHODIMP
nsThreadManager::SpinEventLoopUntilOrQuit(
    const nsACString& aVeryGoodReasonToDoThis,
    nsINestedEventLoopCondition* aCondition) {
  return SpinEventLoopUntilInternal(aVeryGoodReasonToDoThis, aCondition,
                                    ShutdownPhase::AppShutdownConfirmed);
}

// statics from SpinEventLoopUntil.h
AutoNestedEventLoopAnnotation* AutoNestedEventLoopAnnotation::sCurrent =
    nullptr;
StaticMutex AutoNestedEventLoopAnnotation::sStackMutex;

// static from SpinEventLoopUntil.h
void AutoNestedEventLoopAnnotation::AnnotateXPCOMSpinEventLoopStack(
    const nsACString& aStack) {
  if (aStack.Length() > 0) {
    nsCString prefixedStack(XRE_GetProcessTypeString());
    prefixedStack += ": "_ns + aStack;
    CrashReporter::AnnotateCrashReport(
        CrashReporter::Annotation::XPCOMSpinEventLoopStack, prefixedStack);
  } else {
    CrashReporter::AnnotateCrashReport(
        CrashReporter::Annotation::XPCOMSpinEventLoopStack, ""_ns);
  }
}

nsresult nsThreadManager::SpinEventLoopUntilInternal(
    const nsACString& aVeryGoodReasonToDoThis,
    nsINestedEventLoopCondition* aCondition,
    ShutdownPhase aShutdownPhaseToCheck) {
  // XXX: We would want to AssertIsOnMainThread(); but that breaks some GTest.
  nsCOMPtr<nsINestedEventLoopCondition> condition(aCondition);
  nsresult rv = NS_OK;

  if (!mozilla::SpinEventLoopUntil(aVeryGoodReasonToDoThis, [&]() -> bool {
        // Check if an ongoing shutdown reached our limits.
        if (aShutdownPhaseToCheck > ShutdownPhase::NotInShutdown &&
            AppShutdown::GetCurrentShutdownPhase() >= aShutdownPhaseToCheck) {
          return true;
        }

        bool isDone = false;
        rv = condition->IsDone(&isDone);
        // JS failure should be unusual, but we need to stop and propagate
        // the error back to the caller.
        if (NS_FAILED(rv)) {
          return true;
        }

        return isDone;
      })) {
    // We stopped early for some reason, which is unexpected.
    return NS_ERROR_UNEXPECTED;
  }

  // If we exited when the condition told us to, we need to return whether
  // the condition encountered failure when executing.
  return rv;
}

NS_IMETHODIMP
nsThreadManager::SpinEventLoopUntilEmpty() {
  nsIThread* thread = NS_GetCurrentThread();

  while (NS_HasPendingEvents(thread)) {
    (void)NS_ProcessNextEvent(thread, false);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsThreadManager::GetMainThreadEventTarget(nsIEventTarget** aTarget) {
  nsCOMPtr<nsIEventTarget> target = GetMainThreadSerialEventTarget();
  target.forget(aTarget);
  return NS_OK;
}

NS_IMETHODIMP
nsThreadManager::DispatchToMainThread(nsIRunnable* aEvent, uint32_t aPriority,
                                      uint8_t aArgc) {
  // Note: C++ callers should instead use NS_DispatchToMainThread.
  MOZ_ASSERT(NS_IsMainThread());

  // Keep this functioning during Shutdown
  if (NS_WARN_IF(!mMainThread)) {
    return NS_ERROR_NOT_INITIALIZED;
  }
  // If aPriority wasn't explicitly passed, that means it should be treated as
  // PRIORITY_NORMAL.
  if (aArgc > 0 && aPriority != nsIRunnablePriority::PRIORITY_NORMAL) {
    nsCOMPtr<nsIRunnable> event(aEvent);
    return mMainThread->DispatchFromScript(
        new PrioritizableRunnable(event.forget(), aPriority), 0);
  }
  return mMainThread->DispatchFromScript(aEvent, 0);
}

class AutoMicroTaskWrapperRunnable final : public Runnable {
 public:
  explicit AutoMicroTaskWrapperRunnable(nsIRunnable* aEvent)
      : Runnable("AutoMicroTaskWrapperRunnable"), mEvent(aEvent) {
    MOZ_ASSERT(aEvent);
  }

 private:
  ~AutoMicroTaskWrapperRunnable() = default;

  NS_IMETHOD Run() override {
    nsAutoMicroTask mt;

    return mEvent->Run();
  }

  RefPtr<nsIRunnable> mEvent;
};

NS_IMETHODIMP
nsThreadManager::DispatchToMainThreadWithMicroTask(nsIRunnable* aEvent,
                                                   uint32_t aPriority,
                                                   uint8_t aArgc) {
  RefPtr<AutoMicroTaskWrapperRunnable> runnable =
      new AutoMicroTaskWrapperRunnable(aEvent);

  return DispatchToMainThread(runnable, aPriority, aArgc);
}

void nsThreadManager::EnableMainThreadEventPrioritization() {
  MOZ_ASSERT(NS_IsMainThread());
  InputTaskManager::Get()->EnableInputEventPrioritization();
}

void nsThreadManager::FlushInputEventPrioritization() {
  MOZ_ASSERT(NS_IsMainThread());
  InputTaskManager::Get()->FlushInputEventPrioritization();
}

void nsThreadManager::SuspendInputEventPrioritization() {
  MOZ_ASSERT(NS_IsMainThread());
  InputTaskManager::Get()->SuspendInputEventPrioritization();
}

void nsThreadManager::ResumeInputEventPrioritization() {
  MOZ_ASSERT(NS_IsMainThread());
  InputTaskManager::Get()->ResumeInputEventPrioritization();
}

// static
bool nsThreadManager::MainThreadHasPendingHighPriorityEvents() {
  MOZ_ASSERT(NS_IsMainThread());
  bool retVal = false;
  if (get().mMainThread) {
    get().mMainThread->HasPendingHighPriorityEvents(&retVal);
  }
  return retVal;
}

NS_IMETHODIMP
nsThreadManager::IdleDispatchToMainThread(nsIRunnable* aEvent,
                                          uint32_t aTimeout) {
  // Note: C++ callers should instead use NS_DispatchToThreadQueue or
  // NS_DispatchToCurrentThreadQueue.
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsIRunnable> event(aEvent);
  if (aTimeout) {
    return NS_DispatchToThreadQueue(event.forget(), aTimeout, mMainThread,
                                    EventQueuePriority::Idle);
  }

  return NS_DispatchToThreadQueue(event.forget(), mMainThread,
                                  EventQueuePriority::Idle);
}

NS_IMETHODIMP
nsThreadManager::DispatchDirectTaskToCurrentThread(nsIRunnable* aEvent) {
  NS_ENSURE_STATE(aEvent);
  nsCOMPtr<nsIRunnable> runnable = aEvent;
  return GetCurrentThread()->DispatchDirectTask(runnable.forget());
}

bool nsThreadManager::AllowNewXPCOMThreads() {
  mozilla::OffTheBooksMutexAutoLock lock(mMutex);
  return AllowNewXPCOMThreadsLocked();
}
