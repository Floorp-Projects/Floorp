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
#include "nsTArray.h"
#include "nsXULAppAPI.h"
#include "MainThreadQueue.h"
#include "mozilla/AbstractThread.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/EventQueue.h"
#include "mozilla/Mutex.h"
#include "mozilla/Preferences.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/TaskQueue.h"
#include "mozilla/ThreadEventQueue.h"
#include "mozilla/ThreadLocal.h"
#include "PrioritizedEventQueue.h"
#ifdef MOZ_CANARY
#  include <fcntl.h>
#  include <unistd.h>
#endif

#include "MainThreadIdlePeriod.h"
#include "InputEventStatistics.h"

using namespace mozilla;

static MOZ_THREAD_LOCAL(bool) sTLSIsMainThread;

bool NS_IsMainThreadTLSInitialized() { return sTLSIsMainThread.initialized(); }

class BackgroundEventTarget final : public nsIEventTarget {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIEVENTTARGET_FULL

  BackgroundEventTarget();

  nsresult Init();

  already_AddRefed<nsISerialEventTarget> CreateBackgroundTaskQueue(
      const char* aName);

  void BeginShutdown(nsTArray<RefPtr<ShutdownPromise>>&);
  void FinishShutdown();

 private:
  ~BackgroundEventTarget() = default;

  nsCOMPtr<nsIThreadPool> mPool;
  nsCOMPtr<nsIThreadPool> mIOPool;

  Mutex mMutex;
  nsTArray<RefPtr<TaskQueue>> mTaskQueues;
};

NS_IMPL_ISUPPORTS(BackgroundEventTarget, nsIEventTarget)

BackgroundEventTarget::BackgroundEventTarget()
    : mMutex("BackgroundEventTarget::mMutex") {}

nsresult BackgroundEventTarget::Init() {
  nsCOMPtr<nsIThreadPool> pool(new nsThreadPool());
  NS_ENSURE_TRUE(pool, NS_ERROR_FAILURE);

  nsresult rv = pool->SetName(NS_LITERAL_CSTRING("BackgroundThreadPool"));
  NS_ENSURE_SUCCESS(rv, rv);

  // Use potentially more conservative stack size.
  rv = pool->SetThreadStackSize(nsIThreadManager::kThreadPoolStackSize);
  NS_ENSURE_SUCCESS(rv, rv);

  // For now just one thread. Can increase easily later if we want.
  rv = pool->SetThreadLimit(1);
  NS_ENSURE_SUCCESS(rv, rv);

  // Leave threads alive for up to 5 minutes
  rv = pool->SetIdleThreadTimeout(300000);
  NS_ENSURE_SUCCESS(rv, rv);

  // Initialize the background I/O event target.
  nsCOMPtr<nsIThreadPool> ioPool(new nsThreadPool());
  NS_ENSURE_TRUE(pool, NS_ERROR_FAILURE);

  rv = ioPool->SetName(NS_LITERAL_CSTRING("BgIOThreadPool"));
  NS_ENSURE_SUCCESS(rv, rv);

  // Use potentially more conservative stack size.
  rv = ioPool->SetThreadStackSize(nsIThreadManager::kThreadPoolStackSize);
  NS_ENSURE_SUCCESS(rv, rv);

  // For now just one thread. Can increase easily later if we want.
  rv = ioPool->SetThreadLimit(1);
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

void BackgroundEventTarget::BeginShutdown(
    nsTArray<RefPtr<ShutdownPromise>>& promises) {
  for (auto& queue : mTaskQueues) {
    promises.AppendElement(queue->BeginShutdown());
  }
}

void BackgroundEventTarget::FinishShutdown() {
  mPool->Shutdown();
  mIOPool->Shutdown();
}

already_AddRefed<nsISerialEventTarget>
BackgroundEventTarget::CreateBackgroundTaskQueue(const char* aName) {
  MutexAutoLock lock(mMutex);

  RefPtr<TaskQueue> queue = new TaskQueue(do_AddRef(this), aName,
                                          /*aSupportsTailDispatch=*/false,
                                          /*aRetainFlags=*/true);
  nsCOMPtr<nsISerialEventTarget> target(queue->WrapAsEventTarget());

  mTaskQueues.AppendElement(queue.forget());

  return target.forget();
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
}

#ifdef DEBUG

namespace mozilla {

void AssertIsOnMainThread() { MOZ_ASSERT(NS_IsMainThread(), "Wrong thread!"); }

}  // namespace mozilla

#endif

typedef nsTArray<NotNull<RefPtr<nsThread>>> nsThreadArray;

static bool sShutdownComplete;

//-----------------------------------------------------------------------------

/* static */
void nsThreadManager::ReleaseThread(void* aData) {
  if (sShutdownComplete) {
    // We've already completed shutdown and released the references to all or
    // our TLS wrappers. Don't try to release them again.
    return;
  }

  auto* thread = static_cast<nsThread*>(aData);

  if (thread->mHasTLSEntry) {
    thread->mHasTLSEntry = false;
    thread->Release();
  }
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

namespace {

// Simple observer to monitor the beginning of the shutdown.
class ShutdownObserveHelper final : public nsIObserver,
                                    public nsSupportsWeakReference {
 public:
  NS_DECL_ISUPPORTS

  static nsresult Create(ShutdownObserveHelper** aObserver) {
    MOZ_ASSERT(aObserver);

    RefPtr<ShutdownObserveHelper> observer = new ShutdownObserveHelper();

    nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
    if (NS_WARN_IF(!obs)) {
      return NS_ERROR_FAILURE;
    }

    nsresult rv =
        obs->AddObserver(observer, NS_XPCOM_SHUTDOWN_OBSERVER_ID, true);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = obs->AddObserver(observer, "content-child-will-shutdown", true);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    observer.forget(aObserver);
    return NS_OK;
  }

  NS_IMETHOD
  Observe(nsISupports* aSubject, const char* aTopic,
          const char16_t* aData) override {
    if (!strcmp(aTopic, NS_XPCOM_SHUTDOWN_OBSERVER_ID) ||
        !strcmp(aTopic, "content-child-will-shutdown")) {
      mShuttingDown = true;
      return NS_OK;
    }

    return NS_OK;
  }

  bool ShuttingDown() const { return mShuttingDown; }

 private:
  explicit ShutdownObserveHelper() : mShuttingDown(false) {}

  ~ShutdownObserveHelper() = default;

  bool mShuttingDown;
};

NS_INTERFACE_MAP_BEGIN(ShutdownObserveHelper)
  NS_INTERFACE_MAP_ENTRY(nsIObserver)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIObserver)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(ShutdownObserveHelper)
NS_IMPL_RELEASE(ShutdownObserveHelper)

StaticRefPtr<ShutdownObserveHelper> gShutdownObserveHelper;

}  // namespace

//-----------------------------------------------------------------------------

/*static*/ nsThreadManager& nsThreadManager::get() {
  static nsThreadManager sInstance;
  return sInstance;
}

/* static */
void nsThreadManager::InitializeShutdownObserver() {
  MOZ_ASSERT(!gShutdownObserveHelper);

  RefPtr<ShutdownObserveHelper> observer;
  nsresult rv = ShutdownObserveHelper::Create(getter_AddRefs(observer));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  gShutdownObserveHelper = observer;
  ClearOnShutdown(&gShutdownObserveHelper);
}

nsThreadManager::nsThreadManager()
    : mCurThreadIndex(0), mMainPRThread(nullptr), mInitialized(false) {}

nsThreadManager::~nsThreadManager() = default;

nsresult nsThreadManager::Init() {
  // Child processes need to initialize the thread manager before they
  // initialize XPCOM in order to set up the crash reporter. This leads to
  // situations where we get initialized twice.
  if (mInitialized) {
    return NS_OK;
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

  nsCOMPtr<nsIIdlePeriod> idlePeriod = new MainThreadIdlePeriod();

  mMainThread =
      CreateMainThread<ThreadEventQueue<PrioritizedEventQueue>>(idlePeriod);

  nsresult rv = mMainThread->InitCurrentThread();
  if (NS_FAILED(rv)) {
    mMainThread = nullptr;
    return rv;
  }

  // We need to keep a pointer to the current thread, so we can satisfy
  // GetIsMainThread calls that occur post-Shutdown.
  mMainThread->GetPRThread(&mMainPRThread);

  // Init AbstractThread.
  AbstractThread::InitTLS();
  AbstractThread::InitMainThread();

  // Initialize the background event target.
  RefPtr<BackgroundEventTarget> target(new BackgroundEventTarget());

  rv = target->Init();
  NS_ENSURE_SUCCESS(rv, rv);

  mBackgroundEventTarget = std::move(target);

  mInitialized = true;

  return NS_OK;
}

void nsThreadManager::Shutdown() {
  MOZ_ASSERT(NS_IsMainThread(), "shutdown not called from main thread");

  // Prevent further access to the thread manager (no more new threads!)
  //
  // What happens if shutdown happens before NewThread completes?
  // We Shutdown() the new thread, and return error if we've started Shutdown
  // between when NewThread started, and when the thread finished initializing
  // and registering with ThreadManager.
  //
  mInitialized = false;

  // Empty the main thread event queue before we begin shutting down threads.
  NS_ProcessPendingEvents(mMainThread);

  typedef typename ShutdownPromise::AllPromiseType AllPromise;
  typename AllPromise::ResolveOrRejectValue val;
  using ResolveValueT = typename AllPromise::ResolveValueType;
  using RejectValueT = typename AllPromise::RejectValueType;

  nsTArray<RefPtr<ShutdownPromise>> promises;
  mBackgroundEventTarget->BeginShutdown(promises);

  RefPtr<AllPromise> complete = ShutdownPromise::All(mMainThread, promises);

  bool taskQueuesShutdown = false;

  complete->Then(
      mMainThread, __func__,
      [&](const ResolveValueT& aResolveValue) {
        mBackgroundEventTarget->FinishShutdown();
        taskQueuesShutdown = true;
      },
      [&](RejectValueT aRejectValue) {
        mBackgroundEventTarget->FinishShutdown();
        taskQueuesShutdown = true;
      });

  // Wait for task queues to shutdown, so we don't shut down the underlying
  // threads of the background event target in the block below, thereby
  // preventing the task queues from emptying, preventing the shutdown promises
  // from resolving, and prevent anything checking `taskQueuesShutdown` from
  // working.
  ::SpinEventLoopUntil([&]() { return taskQueuesShutdown; }, mMainThread);

  {
    // We gather the threads from the hashtable into a list, so that we avoid
    // holding the enumerator lock while calling nsIThread::Shutdown.
    nsTArray<RefPtr<nsThread>> threadsToShutdown;
    for (auto* thread : nsThread::Enumerate()) {
      if (thread->ShutdownRequired()) {
        threadsToShutdown.AppendElement(thread);
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

    // Shutdown all threads that require it (join with threads that we created).
    for (auto& thread : threadsToShutdown) {
      thread->Shutdown();
    }
  }

  // NB: It's possible that there are events in the queue that want to *start*
  // an asynchronous shutdown. But we have already shutdown the threads above,
  // so there's no need to worry about them. We only have to wait for all
  // in-flight asynchronous thread shutdowns to complete.
  mMainThread->WaitForAllAsynchronousShutdowns();

  // In case there are any more events somehow...
  NS_ProcessPendingEvents(mMainThread);

  // There are no more background threads at this point.

  // Normally thread shutdown clears the observer for the thread, but since the
  // main thread is special we do it manually here after we're sure all events
  // have been processed.
  mMainThread->SetObserver(nullptr);

  mBackgroundEventTarget = nullptr;

  // Release main thread object.
  mMainThread = nullptr;

  // Remove the TLS entry for the main thread.
  PR_SetThreadPrivate(mCurThreadIndex, nullptr);

  {
    // Cleanup the last references to any threads which haven't shut down yet.
    nsTArray<RefPtr<nsThread>> threads;
    for (auto* thread : nsThread::Enumerate()) {
      if (thread->mHasTLSEntry) {
        threads.AppendElement(dont_AddRef(thread));
        thread->mHasTLSEntry = false;
      }
    }
  }

  // xpcshell tests sometimes leak the main thread. They don't enable leak
  // checking, so that doesn't cause the test to fail, but leaving the entry in
  // the thread list triggers an assertion, which does.
  nsThread::ClearThreadList();

  sShutdownComplete = true;
}

void nsThreadManager::RegisterCurrentThread(nsThread& aThread) {
  MOZ_ASSERT(aThread.GetPRThread() == PR_GetCurrentThread(), "bad aThread");

  aThread.AddRef();  // for TLS entry
  aThread.mHasTLSEntry = true;
  PR_SetThreadPrivate(mCurThreadIndex, &aThread);
}

void nsThreadManager::UnregisterCurrentThread(nsThread& aThread) {
  MOZ_ASSERT(aThread.GetPRThread() == PR_GetCurrentThread(), "bad aThread");

  PR_SetThreadPrivate(mCurThreadIndex, nullptr);
  // Ref-count balanced via ReleaseThread
}

nsThread* nsThreadManager::CreateCurrentThread(
    SynchronizedEventQueue* aQueue, nsThread::MainThreadFlag aMainThread) {
  // Make sure we don't have an nsThread yet.
  MOZ_ASSERT(!PR_GetThreadPrivate(mCurThreadIndex));

  if (!mInitialized) {
    return nullptr;
  }

  RefPtr<nsThread> thread = new nsThread(WrapNotNull(aQueue), aMainThread, 0);
  if (!thread || NS_FAILED(thread->InitCurrentThread())) {
    return nullptr;
  }

  return thread.get();  // reference held in TLS
}

nsresult nsThreadManager::DispatchToBackgroundThread(nsIRunnable* aEvent,
                                                     uint32_t aDispatchFlags) {
  if (!mInitialized) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIEventTarget> backgroundTarget(mBackgroundEventTarget);
  return backgroundTarget->Dispatch(aEvent, aDispatchFlags);
}

already_AddRefed<nsISerialEventTarget>
nsThreadManager::CreateBackgroundTaskQueue(const char* aName) {
  if (!mInitialized) {
    return nullptr;
  }

  return mBackgroundEventTarget->CreateBackgroundTaskQueue(aName);
}

nsThread* nsThreadManager::GetCurrentThread() {
  // read thread local storage
  void* data = PR_GetThreadPrivate(mCurThreadIndex);
  if (data) {
    return static_cast<nsThread*>(data);
  }

  if (!mInitialized) {
    return nullptr;
  }

  // OK, that's fine.  We'll dynamically create one :-)
  //
  // We assume that if we're implicitly creating a thread here that it doesn't
  // want an event queue. Any thread which wants an event queue should
  // explicitly create its nsThread wrapper.
  RefPtr<nsThread> thread = new nsThread();
  if (!thread || NS_FAILED(thread->InitCurrentThread())) {
    return nullptr;
  }

  return thread.get();  // reference held in TLS
}

bool nsThreadManager::IsNSThread() const {
  if (!mInitialized) {
    return false;
  }
  if (auto* thread = (nsThread*)PR_GetThreadPrivate(mCurThreadIndex)) {
    return thread->EventQueue();
  }
  return false;
}

NS_IMETHODIMP
nsThreadManager::NewThread(uint32_t aCreationFlags, uint32_t aStackSize,
                           nsIThread** aResult) {
  return NewNamedThread(NS_LITERAL_CSTRING(""), aStackSize, aResult);
}

NS_IMETHODIMP
nsThreadManager::NewNamedThread(const nsACString& aName, uint32_t aStackSize,
                                nsIThread** aResult) {
  // Note: can be called from arbitrary threads

  // No new threads during Shutdown
  if (NS_WARN_IF(!mInitialized)) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  RefPtr<ThreadEventQueue<EventQueue>> queue =
      new ThreadEventQueue<EventQueue>(MakeUnique<EventQueue>());
  RefPtr<nsThread> thr =
      new nsThread(WrapNotNull(queue), nsThread::NOT_MAIN_THREAD, aStackSize);
  nsresult rv =
      thr->Init(aName);  // Note: blocks until the new thread has been set up
  if (NS_FAILED(rv)) {
    return rv;
  }

  // At this point, we expect that the thread has been registered in
  // mThreadByPRThread; however, it is possible that it could have also been
  // replaced by now, so we cannot really assert that it was added.  Instead,
  // kill it if we entered Shutdown() during/before Init()

  if (NS_WARN_IF(!mInitialized)) {
    if (thr->ShutdownRequired()) {
      thr->Shutdown();  // ok if it happens multiple times
    }
    return NS_ERROR_NOT_INITIALIZED;
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
nsThreadManager::SpinEventLoopUntil(nsINestedEventLoopCondition* aCondition) {
  return SpinEventLoopUntilInternal(aCondition, false);
}

NS_IMETHODIMP
nsThreadManager::SpinEventLoopUntilOrShutdown(
    nsINestedEventLoopCondition* aCondition) {
  return SpinEventLoopUntilInternal(aCondition, true);
}

nsresult nsThreadManager::SpinEventLoopUntilInternal(
    nsINestedEventLoopCondition* aCondition, bool aCheckingShutdown) {
  nsCOMPtr<nsINestedEventLoopCondition> condition(aCondition);
  nsresult rv = NS_OK;

  // Nothing to do if already shutting down. Note that gShutdownObserveHelper is
  // nullified on shutdown.
  if (aCheckingShutdown &&
      (!gShutdownObserveHelper || gShutdownObserveHelper->ShuttingDown())) {
    return NS_OK;
  }

  if (!mozilla::SpinEventLoopUntil([&]() -> bool {
        // Shutting down is started.
        if (aCheckingShutdown && (!gShutdownObserveHelper ||
                                  gShutdownObserveHelper->ShuttingDown())) {
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

uint32_t nsThreadManager::GetHighestNumberOfThreads() {
  return nsThread::MaxActiveThreads();
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

void nsThreadManager::EnableMainThreadEventPrioritization() {
  MOZ_ASSERT(NS_IsMainThread());
  InputEventStatistics::Get().SetEnable(true);
  mMainThread->EnableInputEventPrioritization();
}

void nsThreadManager::FlushInputEventPrioritization() {
  MOZ_ASSERT(NS_IsMainThread());
  mMainThread->FlushInputEventPrioritization();
}

void nsThreadManager::SuspendInputEventPrioritization() {
  MOZ_ASSERT(NS_IsMainThread());
  mMainThread->SuspendInputEventPrioritization();
}

void nsThreadManager::ResumeInputEventPrioritization() {
  MOZ_ASSERT(NS_IsMainThread());
  mMainThread->ResumeInputEventPrioritization();
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
