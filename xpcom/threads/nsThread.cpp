/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsThread.h"

#include "base/message_loop.h"
#include "base/platform_thread.h"

// Chromium's logging can sometimes leak through...
#ifdef LOG
#  undef LOG
#endif

#include "mozilla/ReentrantMonitor.h"
#include "nsMemoryPressure.h"
#include "nsThreadManager.h"
#include "nsIClassInfoImpl.h"
#include "nsCOMPtr.h"
#include "nsQueryObject.h"
#include "pratom.h"
#include "mozilla/BackgroundHangMonitor.h"
#include "mozilla/CycleCollectedJSContext.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/Logging.h"
#include "nsIObserverService.h"
#include "mozilla/IOInterposer.h"
#include "mozilla/ipc/MessageChannel.h"
#include "mozilla/ipc/BackgroundChild.h"
#include "mozilla/Preferences.h"
#include "mozilla/ProfilerRunnable.h"
#include "mozilla/SchedulerGroup.h"
#include "mozilla/Services.h"
#include "mozilla/SpinEventLoopUntil.h"
#include "mozilla/StaticLocalPtr.h"
#include "mozilla/StaticPrefs_threads.h"
#include "mozilla/TaskController.h"
#include "nsXPCOMPrivate.h"
#include "mozilla/ChaosMode.h"
#include "mozilla/Telemetry.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/Unused.h"
#include "mozilla/dom/DocGroup.h"
#include "mozilla/dom/ScriptSettings.h"
#include "nsThreadSyncDispatch.h"
#include "nsServiceManagerUtils.h"
#include "GeckoProfiler.h"
#include "InputEventStatistics.h"
#include "ThreadEventQueue.h"
#include "ThreadEventTarget.h"
#include "ThreadDelay.h"

#include <limits>

#ifdef XP_LINUX
#  ifdef __GLIBC__
#    include <gnu/libc-version.h>
#  endif
#  include <sys/mman.h>
#  include <sys/time.h>
#  include <sys/resource.h>
#  include <sched.h>
#  include <stdio.h>
#endif

#ifdef XP_WIN
#  include "mozilla/DynamicallyLinkedFunctionPtr.h"

#  include <winbase.h>

using GetCurrentThreadStackLimitsFn = void(WINAPI*)(PULONG_PTR LowLimit,
                                                    PULONG_PTR HighLimit);
#endif

#define HAVE_UALARM                                                        \
  _BSD_SOURCE ||                                                           \
      (_XOPEN_SOURCE >= 500 || _XOPEN_SOURCE && _XOPEN_SOURCE_EXTENDED) && \
          !(_POSIX_C_SOURCE >= 200809L || _XOPEN_SOURCE >= 700)

#if defined(XP_LINUX) && !defined(ANDROID) && defined(_GNU_SOURCE)
#  define HAVE_SCHED_SETAFFINITY
#endif

#ifdef XP_MACOSX
#  include <mach/mach.h>
#  include <mach/thread_policy.h>
#endif

#ifdef MOZ_CANARY
#  include <unistd.h>
#  include <execinfo.h>
#  include <signal.h>
#  include <fcntl.h>
#  include "nsXULAppAPI.h"
#endif

using namespace mozilla;

extern void InitThreadLocalVariables();

static LazyLogModule sThreadLog("nsThread");
#ifdef LOG
#  undef LOG
#endif
#define LOG(args) MOZ_LOG(sThreadLog, mozilla::LogLevel::Debug, args)

NS_DECL_CI_INTERFACE_GETTER(nsThread)

Array<char, nsThread::kRunnableNameBufSize> nsThread::sMainThreadRunnableName;

#ifdef EARLY_BETA_OR_EARLIER
const uint32_t kTelemetryWakeupCountLimit = 100;
#endif

//-----------------------------------------------------------------------------
// Because we do not have our own nsIFactory, we have to implement nsIClassInfo
// somewhat manually.

class nsThreadClassInfo : public nsIClassInfo {
 public:
  NS_DECL_ISUPPORTS_INHERITED  // no mRefCnt
      NS_DECL_NSICLASSINFO

      nsThreadClassInfo() = default;
};

NS_IMETHODIMP_(MozExternalRefCountType)
nsThreadClassInfo::AddRef() { return 2; }
NS_IMETHODIMP_(MozExternalRefCountType)
nsThreadClassInfo::Release() { return 1; }
NS_IMPL_QUERY_INTERFACE(nsThreadClassInfo, nsIClassInfo)

NS_IMETHODIMP
nsThreadClassInfo::GetInterfaces(nsTArray<nsIID>& aArray) {
  return NS_CI_INTERFACE_GETTER_NAME(nsThread)(aArray);
}

NS_IMETHODIMP
nsThreadClassInfo::GetScriptableHelper(nsIXPCScriptable** aResult) {
  *aResult = nullptr;
  return NS_OK;
}

NS_IMETHODIMP
nsThreadClassInfo::GetContractID(nsACString& aResult) {
  aResult.SetIsVoid(true);
  return NS_OK;
}

NS_IMETHODIMP
nsThreadClassInfo::GetClassDescription(nsACString& aResult) {
  aResult.SetIsVoid(true);
  return NS_OK;
}

NS_IMETHODIMP
nsThreadClassInfo::GetClassID(nsCID** aResult) {
  *aResult = nullptr;
  return NS_OK;
}

NS_IMETHODIMP
nsThreadClassInfo::GetFlags(uint32_t* aResult) {
  *aResult = THREADSAFE;
  return NS_OK;
}

NS_IMETHODIMP
nsThreadClassInfo::GetClassIDNoAlloc(nsCID* aResult) {
  return NS_ERROR_NOT_AVAILABLE;
}

//-----------------------------------------------------------------------------

NS_IMPL_ADDREF(nsThread)
NS_IMPL_RELEASE(nsThread)
NS_INTERFACE_MAP_BEGIN(nsThread)
  NS_INTERFACE_MAP_ENTRY(nsIThread)
  NS_INTERFACE_MAP_ENTRY(nsIThreadInternal)
  NS_INTERFACE_MAP_ENTRY(nsIEventTarget)
  NS_INTERFACE_MAP_ENTRY(nsISerialEventTarget)
  NS_INTERFACE_MAP_ENTRY(nsISupportsPriority)
  NS_INTERFACE_MAP_ENTRY_CONDITIONAL(nsIDelayedRunnableObserver, mEventTarget)
  NS_INTERFACE_MAP_ENTRY(nsIDirectTaskDispatcher)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIThread)
  if (aIID.Equals(NS_GET_IID(nsIClassInfo))) {
    static nsThreadClassInfo sThreadClassInfo;
    foundInterface = static_cast<nsIClassInfo*>(&sThreadClassInfo);
  } else
NS_INTERFACE_MAP_END
NS_IMPL_CI_INTERFACE_GETTER(nsThread, nsIThread, nsIThreadInternal,
                            nsIEventTarget, nsISerialEventTarget,
                            nsISupportsPriority)

//-----------------------------------------------------------------------------

bool nsThread::ShutdownContextsComp::Equals(
    const ShutdownContexts::elem_type& a,
    const ShutdownContexts::elem_type::Pointer b) const {
  return a.get() == b;
}

// This event is responsible for notifying nsThread::Shutdown that it is time
// to call PR_JoinThread. It implements nsICancelableRunnable so that it can
// run on a DOM Worker thread (where all events must implement
// nsICancelableRunnable.)
class nsThreadShutdownAckEvent : public CancelableRunnable {
 public:
  explicit nsThreadShutdownAckEvent(NotNull<nsThreadShutdownContext*> aCtx)
      : CancelableRunnable("nsThreadShutdownAckEvent"),
        mShutdownContext(aCtx) {}
  NS_IMETHOD Run() override {
    mShutdownContext->mTerminatingThread->ShutdownComplete(mShutdownContext);
    return NS_OK;
  }
  nsresult Cancel() override { return Run(); }

 private:
  virtual ~nsThreadShutdownAckEvent() = default;

  NotNull<nsThreadShutdownContext*> mShutdownContext;
};

// This event is responsible for setting mShutdownContext
class nsThreadShutdownEvent : public Runnable {
 public:
  nsThreadShutdownEvent(NotNull<nsThread*> aThr,
                        NotNull<nsThreadShutdownContext*> aCtx)
      : Runnable("nsThreadShutdownEvent"),
        mThread(aThr),
        mShutdownContext(aCtx) {}
  NS_IMETHOD Run() override {
    mThread->mShutdownContext = mShutdownContext;
    if (mThread->mEventTarget) {
      mThread->mEventTarget->NotifyShutdown();
    }
    MessageLoop::current()->Quit();
    return NS_OK;
  }

 private:
  NotNull<RefPtr<nsThread>> mThread;
  NotNull<nsThreadShutdownContext*> mShutdownContext;
};

//-----------------------------------------------------------------------------

static void SetThreadAffinity(unsigned int cpu) {
#ifdef HAVE_SCHED_SETAFFINITY
  cpu_set_t cpus;
  CPU_ZERO(&cpus);
  CPU_SET(cpu, &cpus);
  sched_setaffinity(0, sizeof(cpus), &cpus);
  // Don't assert sched_setaffinity's return value because it intermittently (?)
  // fails with EINVAL on Linux x64 try runs.
#elif defined(XP_MACOSX)
  // OS X does not provide APIs to pin threads to specific processors, but you
  // can tag threads as belonging to the same "affinity set" and the OS will try
  // to run them on the same processor. To run threads on different processors,
  // tag them as belonging to different affinity sets. Tag 0, the default, means
  // "no affinity" so let's pretend each CPU has its own tag `cpu+1`.
  thread_affinity_policy_data_t policy;
  policy.affinity_tag = cpu + 1;
  MOZ_ALWAYS_TRUE(thread_policy_set(mach_thread_self(), THREAD_AFFINITY_POLICY,
                                    &policy.affinity_tag, 1) == KERN_SUCCESS);
#elif defined(XP_WIN)
  MOZ_ALWAYS_TRUE(SetThreadIdealProcessor(GetCurrentThread(), cpu) !=
                  (DWORD)-1);
#endif
}

static void SetupCurrentThreadForChaosMode() {
  if (!ChaosMode::isActive(ChaosFeature::ThreadScheduling)) {
    return;
  }

#ifdef XP_LINUX
  // PR_SetThreadPriority doesn't really work since priorities >
  // PR_PRIORITY_NORMAL can't be set by non-root users. Instead we'll just use
  // setpriority(2) to set random 'nice values'. In regular Linux this is only
  // a dynamic adjustment so it still doesn't really do what we want, but tools
  // like 'rr' can be more aggressive about honoring these values.
  // Some of these calls may fail due to trying to lower the priority
  // (e.g. something may have already called setpriority() for this thread).
  // This makes it hard to have non-main threads with higher priority than the
  // main thread, but that's hard to fix. Tools like rr can choose to honor the
  // requested values anyway.
  // Use just 4 priorities so there's a reasonable chance of any two threads
  // having equal priority.
  setpriority(PRIO_PROCESS, 0, ChaosMode::randomUint32LessThan(4));
#else
  // We should set the affinity here but NSPR doesn't provide a way to expose
  // it.
  uint32_t priority = ChaosMode::randomUint32LessThan(PR_PRIORITY_LAST + 1);
  PR_SetThreadPriority(PR_GetCurrentThread(), PRThreadPriority(priority));
#endif

  // Force half the threads to CPU 0 so they compete for CPU
  if (ChaosMode::randomUint32LessThan(2)) {
    SetThreadAffinity(0);
  }
}

namespace {

struct ThreadInitData {
  nsThread* thread;
  nsCString name;
};

}  // namespace

/* static */ mozilla::OffTheBooksMutex& nsThread::ThreadListMutex() {
  static StaticLocalAutoPtr<OffTheBooksMutex> sMutex(
      new OffTheBooksMutex("nsThread::ThreadListMutex"));
  return *sMutex;
}

/* static */ LinkedList<nsThread>& nsThread::ThreadList() {
  static StaticLocalAutoPtr<LinkedList<nsThread>> sList(
      new LinkedList<nsThread>());
  return *sList;
}

/* static */
void nsThread::ClearThreadList() {
  OffTheBooksMutexAutoLock mal(ThreadListMutex());
  while (ThreadList().popFirst()) {
  }
}

/* static */
nsThreadEnumerator nsThread::Enumerate() { return {}; }

void nsThread::AddToThreadList() {
  OffTheBooksMutexAutoLock mal(ThreadListMutex());
  MOZ_ASSERT(!isInList());

  ThreadList().insertBack(this);
}

void nsThread::MaybeRemoveFromThreadList() {
  OffTheBooksMutexAutoLock mal(ThreadListMutex());
  if (isInList()) {
    removeFrom(ThreadList());
  }
}

/*static*/
void nsThread::ThreadFunc(void* aArg) {
  using mozilla::ipc::BackgroundChild;

  UniquePtr<ThreadInitData> initData(static_cast<ThreadInitData*>(aArg));
  nsThread* self = initData->thread;  // strong reference

  MOZ_ASSERT(self->mEventTarget);
  MOZ_ASSERT(self->mEvents);

  // Note: see the comment in nsThread::Init, where we set these same values.
  DebugOnly<PRThread*> prev = self->mThread.exchange(PR_GetCurrentThread());
  MOZ_ASSERT(!prev || prev == PR_GetCurrentThread());
  self->mEventTarget->SetCurrentThread(self->mThread);
  SetupCurrentThreadForChaosMode();

  if (!initData->name.IsEmpty()) {
    NS_SetCurrentThreadName(initData->name.BeginReading());
  }

  self->InitCommon();

  // Inform the ThreadManager
  nsThreadManager::get().RegisterCurrentThread(*self);

  mozilla::IOInterposer::RegisterCurrentThread();

  // This must come after the call to nsThreadManager::RegisterCurrentThread(),
  // because that call is needed to properly set up this thread as an nsThread,
  // which profiler_register_thread() requires. See bug 1347007.
  const bool registerWithProfiler = !initData->name.IsEmpty();
  if (registerWithProfiler) {
    PROFILER_REGISTER_THREAD(initData->name.BeginReading());
  }

  {
    // Scope for MessageLoop.
    MessageLoop loop(MessageLoop::TYPE_MOZILLA_NONMAINTHREAD, self);

    // Now, process incoming events...
    loop.Run();

    BackgroundChild::CloseForCurrentThread();

    // NB: The main thread does not shut down here!  It shuts down via
    // nsThreadManager::Shutdown.

    // Do NS_ProcessPendingEvents but with special handling to set
    // mEventsAreDoomed atomically with the removal of the last event. The key
    // invariant here is that we will never permit PutEvent to succeed if the
    // event would be left in the queue after our final call to
    // NS_ProcessPendingEvents. We also have to keep processing events as long
    // as we have outstanding mRequestedShutdownContexts.
    while (true) {
      // Check and see if we're waiting on any threads.
      self->WaitForAllAsynchronousShutdowns();

      if (self->mEvents->ShutdownIfNoPendingEvents()) {
        break;
      }
      NS_ProcessPendingEvents(self);
    }
  }

  mozilla::IOInterposer::UnregisterCurrentThread();

  // Inform the threadmanager that this thread is going away
  nsThreadManager::get().UnregisterCurrentThread(*self);

  // The thread should only unregister itself if it was registered above.
  if (registerWithProfiler) {
    PROFILER_UNREGISTER_THREAD();
  }

  // Dispatch shutdown ACK
  NotNull<nsThreadShutdownContext*> context =
      WrapNotNull(self->mShutdownContext);
  MOZ_ASSERT(context->mTerminatingThread == self);
  nsCOMPtr<nsIRunnable> event =
      do_QueryObject(new nsThreadShutdownAckEvent(context));
  nsresult dispatch_ack_rv;
  if (context->mIsMainThreadJoining) {
    dispatch_ack_rv =
        SchedulerGroup::Dispatch(TaskCategory::Other, event.forget());
  } else {
    dispatch_ack_rv =
        context->mJoiningThread->Dispatch(event, NS_DISPATCH_NORMAL);
  }
  // We do not expect this to ever happen, but If we cannot dispatch
  // the ack event, someone probably blocks waiting on us and will
  // crash with a hang later anyways. The best we can do is to tell
  // the world what happened right here.
  MOZ_RELEASE_ASSERT(NS_SUCCEEDED(dispatch_ack_rv));

  // Release any observer of the thread here.
  self->SetObserver(nullptr);

  // The PRThread will be deleted in PR_JoinThread(), so clear references.
  self->mThread = nullptr;
  self->mEventTarget->ClearCurrentThread();
  NS_RELEASE(self);
}

void nsThread::InitCommon() {
  mThreadId = uint32_t(PlatformThread::CurrentId());

  {
#if defined(XP_LINUX)
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_getattr_np(pthread_self(), &attr);

    size_t stackSize;
    pthread_attr_getstack(&attr, &mStackBase, &stackSize);

    // Glibc prior to 2.27 reports the stack size and base including the guard
    // region, so we need to compensate for it to get accurate accounting.
    // Also, this behavior difference isn't guarded by a versioned symbol, so we
    // actually need to check the runtime glibc version, not the version we were
    // compiled against.
    static bool sAdjustForGuardSize = ({
#  ifdef __GLIBC__
      unsigned major, minor;
      sscanf(gnu_get_libc_version(), "%u.%u", &major, &minor) < 2 ||
          major < 2 || (major == 2 && minor < 27);
#  else
      false;
#  endif
    });
    if (sAdjustForGuardSize) {
      size_t guardSize;
      pthread_attr_getguardsize(&attr, &guardSize);

      // Note: This assumes that the stack grows down, as is the case on all of
      // our tier 1 platforms. On platforms where the stack grows up, the
      // mStackBase adjustment is unnecessary, but doesn't cause any harm other
      // than under-counting stack memory usage by one page.
      mStackBase = reinterpret_cast<char*>(mStackBase) + guardSize;
      stackSize -= guardSize;
    }

    mStackSize = stackSize;

    // This is a bit of a hack.
    //
    // We really do want the NOHUGEPAGE flag on our thread stacks, since we
    // don't expect any of them to need anywhere near 2MB of space. But setting
    // it here is too late to have an effect, since the first stack page has
    // already been faulted in existence, and NSPR doesn't give us a way to set
    // it beforehand.
    //
    // What this does get us, however, is a different set of VM flags on our
    // thread stacks compared to normal heap memory. Which makes the Linux
    // kernel report them as separate regions, even when they are adjacent to
    // heap memory. This allows us to accurately track the actual memory
    // consumption of our allocated stacks.
    madvise(mStackBase, stackSize, MADV_NOHUGEPAGE);

    pthread_attr_destroy(&attr);
#elif defined(XP_WIN)
    static const StaticDynamicallyLinkedFunctionPtr<
        GetCurrentThreadStackLimitsFn>
        sGetStackLimits(L"kernel32.dll", "GetCurrentThreadStackLimits");

    if (sGetStackLimits) {
      ULONG_PTR stackBottom, stackTop;
      sGetStackLimits(&stackBottom, &stackTop);
      mStackBase = reinterpret_cast<void*>(stackBottom);
      mStackSize = stackTop - stackBottom;
    }
#endif
  }

  InitThreadLocalVariables();
  AddToThreadList();
}

//-----------------------------------------------------------------------------

#ifdef MOZ_CANARY
int sCanaryOutputFD = -1;
#endif

nsThread::nsThread(NotNull<SynchronizedEventQueue*> aQueue,
                   MainThreadFlag aMainThread, uint32_t aStackSize)
    : mEvents(aQueue.get()),
      mEventTarget(
          new ThreadEventTarget(mEvents.get(), aMainThread == MAIN_THREAD)),
      mShutdownContext(nullptr),
      mScriptObserver(nullptr),
      mThreadName("<uninitialized>"),
      mStackSize(aStackSize),
      mNestedEventLoopDepth(0),
      mShutdownRequired(false),
      mPriority(PRIORITY_NORMAL),
      mIsMainThread(aMainThread == MAIN_THREAD),
      mUseHangMonitor(aMainThread == MAIN_THREAD),
      mIsAPoolThreadFree(nullptr),
      mCanInvokeJS(false),
#ifdef EARLY_BETA_OR_EARLIER
      mLastWakeupCheckTime(TimeStamp::Now()),
#endif
      mPerformanceCounterState(mNestedEventLoopDepth, mIsMainThread) {
  if (mIsMainThread) {
    mozilla::TaskController::Get()->SetPerformanceCounterState(
        &mPerformanceCounterState);
  }
}

nsThread::nsThread()
    : mEvents(nullptr),
      mEventTarget(nullptr),
      mShutdownContext(nullptr),
      mScriptObserver(nullptr),
      mThreadName("<uninitialized>"),
      mStackSize(0),
      mNestedEventLoopDepth(0),
      mShutdownRequired(false),
      mPriority(PRIORITY_NORMAL),
      mIsMainThread(false),
      mUseHangMonitor(false),
      mCanInvokeJS(false),
#ifdef EARLY_BETA_OR_EARLIER
      mLastWakeupCheckTime(TimeStamp::Now()),
#endif
      mPerformanceCounterState(mNestedEventLoopDepth, mIsMainThread) {
  MOZ_ASSERT(!NS_IsMainThread());
}

nsThread::~nsThread() {
  NS_ASSERTION(mRequestedShutdownContexts.IsEmpty(),
               "shouldn't be waiting on other threads to shutdown");

  MaybeRemoveFromThreadList();

#ifdef DEBUG
  // We deliberately leak these so they can be tracked by the leak checker.
  // If you're having nsThreadShutdownContext leaks, you can set:
  //   XPCOM_MEM_LOG_CLASSES=nsThreadShutdownContext
  // during a test run and that will at least tell you what thread is
  // requesting shutdown on another, which can be helpful for diagnosing
  // the leak.
  for (size_t i = 0; i < mRequestedShutdownContexts.Length(); ++i) {
    Unused << mRequestedShutdownContexts[i].release();
  }
#endif
}

nsresult nsThread::Init(const nsACString& aName) {
  MOZ_ASSERT(mEvents);
  MOZ_ASSERT(mEventTarget);
  MOZ_ASSERT(!mThread);

  NS_ADDREF_THIS();

  SetThreadNameInternal(aName);

  mShutdownRequired = true;

  UniquePtr<ThreadInitData> initData(
      new ThreadInitData{this, nsCString(aName)});

  PRThread* thread = nullptr;
  // ThreadFunc is responsible for setting mThread
  if (!(thread = PR_CreateThread(PR_USER_THREAD, ThreadFunc, initData.get(),
                                 PR_PRIORITY_NORMAL, PR_GLOBAL_THREAD,
                                 PR_JOINABLE_THREAD, mStackSize))) {
    NS_RELEASE_THIS();
    return NS_ERROR_OUT_OF_MEMORY;
  }

  // The created thread now owns initData, so release our ownership of it.
  Unused << initData.release();

  // Note: we set these both here and inside ThreadFunc, to what should be
  // the same value. This is because calls within ThreadFunc need these values
  // to be set, and our callers need these values to be set.
  DebugOnly<PRThread*> prev = mThread.exchange(thread);
  MOZ_ASSERT(!prev || prev == thread);

  mEventTarget->SetCurrentThread(thread);
  return NS_OK;
}

nsresult nsThread::InitCurrentThread() {
  mThread = PR_GetCurrentThread();
  SetupCurrentThreadForChaosMode();
  InitCommon();

  nsThreadManager::get().RegisterCurrentThread(*this);
  return NS_OK;
}

void nsThread::GetThreadName(nsACString& aNameBuffer) {
  auto lock = mThreadName.Lock();
  aNameBuffer = lock.ref();
}

void nsThread::SetThreadNameInternal(const nsACString& aName) {
  auto lock = mThreadName.Lock();
  lock->Assign(aName);
}

//-----------------------------------------------------------------------------
// nsIEventTarget

NS_IMETHODIMP
nsThread::DispatchFromScript(nsIRunnable* aEvent, uint32_t aFlags) {
  MOZ_ASSERT(mEventTarget);
  NS_ENSURE_TRUE(mEventTarget, NS_ERROR_NOT_IMPLEMENTED);

  nsCOMPtr<nsIRunnable> event(aEvent);
  return mEventTarget->Dispatch(event.forget(), aFlags);
}

NS_IMETHODIMP
nsThread::Dispatch(already_AddRefed<nsIRunnable> aEvent, uint32_t aFlags) {
  MOZ_ASSERT(mEventTarget);
  NS_ENSURE_TRUE(mEventTarget, NS_ERROR_NOT_IMPLEMENTED);

  LOG(("THRD(%p) Dispatch [%p %x]\n", this, /* XXX aEvent */ nullptr, aFlags));

  return mEventTarget->Dispatch(std::move(aEvent), aFlags);
}

NS_IMETHODIMP
nsThread::DelayedDispatch(already_AddRefed<nsIRunnable> aEvent,
                          uint32_t aDelayMs) {
  MOZ_ASSERT(mEventTarget);
  NS_ENSURE_TRUE(mEventTarget, NS_ERROR_NOT_IMPLEMENTED);

  return mEventTarget->DelayedDispatch(std::move(aEvent), aDelayMs);
}

NS_IMETHODIMP
nsThread::GetRunningEventDelay(TimeDuration* aDelay, TimeStamp* aStart) {
  if (mIsAPoolThreadFree && *mIsAPoolThreadFree) {
    // if there are unstarted threads in the pool, a new event to the
    // pool would not be delayed at all (beyond thread start time)
    *aDelay = TimeDuration();
    *aStart = TimeStamp();
  } else {
    *aDelay = mLastEventDelay;
    *aStart = mLastEventStart;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsThread::SetRunningEventDelay(TimeDuration aDelay, TimeStamp aStart) {
  mLastEventDelay = aDelay;
  mLastEventStart = aStart;
  return NS_OK;
}

NS_IMETHODIMP
nsThread::IsOnCurrentThread(bool* aResult) {
  if (mEventTarget) {
    return mEventTarget->IsOnCurrentThread(aResult);
  }
  *aResult = PR_GetCurrentThread() == mThread;
  return NS_OK;
}

NS_IMETHODIMP_(bool)
nsThread::IsOnCurrentThreadInfallible() {
  // This method is only going to be called if `mThread` is null, which
  // only happens when the thread has exited the event loop.  Therefore, when
  // we are called, we can never be on this thread.
  return false;
}

//-----------------------------------------------------------------------------
// nsIThread

NS_IMETHODIMP
nsThread::GetPRThread(PRThread** aResult) {
  PRThread* thread = mThread;  // atomic load
  *aResult = thread;
  return thread ? NS_OK : NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP
nsThread::GetCanInvokeJS(bool* aResult) {
  *aResult = mCanInvokeJS;
  return NS_OK;
}

NS_IMETHODIMP
nsThread::SetCanInvokeJS(bool aCanInvokeJS) {
  mCanInvokeJS = aCanInvokeJS;
  return NS_OK;
}

NS_IMETHODIMP
nsThread::GetLastLongTaskEnd(TimeStamp* _retval) {
  *_retval = mPerformanceCounterState.LastLongTaskEnd();
  return NS_OK;
}

NS_IMETHODIMP
nsThread::GetLastLongNonIdleTaskEnd(TimeStamp* _retval) {
  *_retval = mPerformanceCounterState.LastLongNonIdleTaskEnd();
  return NS_OK;
}

NS_IMETHODIMP
nsThread::SetNameForWakeupTelemetry(const nsACString& aName) {
#ifdef EARLY_BETA_OR_EARLIER
  mNameForWakeupTelemetry = aName;
#endif
  return NS_OK;
}

NS_IMETHODIMP
nsThread::AsyncShutdown() {
  LOG(("THRD(%p) async shutdown\n", this));

  ShutdownInternal(/* aSync = */ false);
  return NS_OK;
}

nsThreadShutdownContext* nsThread::ShutdownInternal(bool aSync) {
  MOZ_ASSERT(mEvents);
  MOZ_ASSERT(mEventTarget);
  MOZ_ASSERT(mThread != PR_GetCurrentThread());
  if (NS_WARN_IF(mThread == PR_GetCurrentThread())) {
    return nullptr;
  }

  // Prevent multiple calls to this method.
  if (!mShutdownRequired.compareExchange(true, false)) {
    return nullptr;
  }
  MOZ_ASSERT(mThread);

  MaybeRemoveFromThreadList();

  NotNull<nsThread*> currentThread =
      WrapNotNull(nsThreadManager::get().GetCurrentThread());

  MOZ_DIAGNOSTIC_ASSERT(currentThread->EventQueue(),
                        "Shutdown() may only be called from an XPCOM thread");

  // Allocate a shutdown context and store a strong ref.
  auto context =
      new nsThreadShutdownContext(WrapNotNull(this), currentThread, aSync);
  Unused << *currentThread->mRequestedShutdownContexts.EmplaceBack(context);

  // Set mShutdownContext and wake up the thread in case it is waiting for
  // events to process.
  nsCOMPtr<nsIRunnable> event =
      new nsThreadShutdownEvent(WrapNotNull(this), WrapNotNull(context));
  if (!mEvents->PutEvent(event.forget(), EventQueuePriority::Normal)) {
    // We do not expect this to happen. Let's collect some diagnostics.
    nsAutoCString threadName;
    currentThread->GetThreadName(threadName);
    MOZ_CRASH_UNSAFE_PRINTF("Attempt to shutdown an already dead thread: %s",
                            threadName.get());
  }

  // We could still end up with other events being added after the shutdown
  // task, but that's okay because we process pending events in ThreadFunc
  // after setting mShutdownContext just before exiting.
  return context;
}

void nsThread::ShutdownComplete(NotNull<nsThreadShutdownContext*> aContext) {
  MOZ_ASSERT(mEvents);
  MOZ_ASSERT(mEventTarget);
  MOZ_ASSERT(aContext->mTerminatingThread == this);

  MaybeRemoveFromThreadList();

  if (aContext->mAwaitingShutdownAck) {
    // We're in a synchronous shutdown, so tell whatever is up the stack that
    // we're done and unwind the stack so it can call us again.
    aContext->mAwaitingShutdownAck = false;
    return;
  }

  // Now, it should be safe to join without fear of dead-locking.
  PR_JoinThread(aContext->mTerminatingPRThread);
  MOZ_ASSERT(!mThread);

#ifdef DEBUG
  nsCOMPtr<nsIThreadObserver> obs = mEvents->GetObserver();
  MOZ_ASSERT(!obs, "Should have been cleared at shutdown!");
#endif

  // Delete aContext.
  // aContext might not be in mRequestedShutdownContexts if it belongs to a
  // thread that was leaked by calling nsIThreadPool::ShutdownWithTimeout.
  aContext->mJoiningThread->mRequestedShutdownContexts.RemoveElement(
      aContext, ShutdownContextsComp{});
}

void nsThread::WaitForAllAsynchronousShutdowns() {
  // This is the motivating example for why SpinEventLoopUntil
  // has the template parameter we are providing here.
  SpinEventLoopUntil<ProcessFailureBehavior::IgnoreAndContinue>(
      "nsThread::WaitForAllAsynchronousShutdowns"_ns,
      [&]() { return mRequestedShutdownContexts.IsEmpty(); }, this);
}

NS_IMETHODIMP
nsThread::Shutdown() {
  LOG(("THRD(%p) sync shutdown\n", this));

  nsThreadShutdownContext* maybeContext = ShutdownInternal(/* aSync = */ true);
  if (!maybeContext) {
    return NS_OK;  // The thread has already shut down.
  }

  NotNull<nsThreadShutdownContext*> context = WrapNotNull(maybeContext);

  // If we are going to hang here we want to see the thread's name
  nsAutoCString threadName;
  GetThreadName(threadName);

  // Process events on the current thread until we receive a shutdown ACK.
  // Allows waiting; ensure no locks are held that would deadlock us!
  SpinEventLoopUntil(
      "nsThread::Shutdown: "_ns + threadName,
      [&, context]() { return !context->mAwaitingShutdownAck; },
      context->mJoiningThread);

  ShutdownComplete(context);

  return NS_OK;
}

NS_IMETHODIMP
nsThread::HasPendingEvents(bool* aResult) {
  if (NS_WARN_IF(PR_GetCurrentThread() != mThread)) {
    return NS_ERROR_NOT_SAME_THREAD;
  }

  if (mIsMainThread && !mIsInLocalExecutionMode) {
    *aResult = TaskController::Get()->HasMainThreadPendingTasks();
  } else {
    *aResult = mEvents->HasPendingEvent();
  }
  return NS_OK;
}

NS_IMETHODIMP
nsThread::HasPendingHighPriorityEvents(bool* aResult) {
  if (NS_WARN_IF(PR_GetCurrentThread() != mThread)) {
    return NS_ERROR_NOT_SAME_THREAD;
  }

  // This function appears to never be called anymore.
  *aResult = false;
  return NS_OK;
}

NS_IMETHODIMP
nsThread::DispatchToQueue(already_AddRefed<nsIRunnable> aEvent,
                          EventQueuePriority aQueue) {
  nsCOMPtr<nsIRunnable> event = aEvent;

  if (NS_WARN_IF(!event)) {
    return NS_ERROR_INVALID_ARG;
  }

  if (!mEvents->PutEvent(event.forget(), aQueue)) {
    NS_WARNING(
        "An idle event was posted to a thread that will never run it "
        "(rejected)");
    return NS_ERROR_UNEXPECTED;
  }

  return NS_OK;
}

#ifdef MOZ_CANARY
void canary_alarm_handler(int signum);

class Canary {
  // XXX ToDo: support nested loops
 public:
  Canary() {
    if (sCanaryOutputFD > 0 && EventLatencyIsImportant()) {
      signal(SIGALRM, canary_alarm_handler);
      ualarm(15000, 0);
    }
  }

  ~Canary() {
    if (sCanaryOutputFD != 0 && EventLatencyIsImportant()) {
      ualarm(0, 0);
    }
  }

  static bool EventLatencyIsImportant() {
    return NS_IsMainThread() && XRE_IsParentProcess();
  }
};

void canary_alarm_handler(int signum) {
  void* array[30];
  const char msg[29] = "event took too long to run:\n";
  // use write to be safe in the signal handler
  write(sCanaryOutputFD, msg, sizeof(msg));
  backtrace_symbols_fd(array, backtrace(array, 30), sCanaryOutputFD);
}

#endif

#define NOTIFY_EVENT_OBSERVERS(observers_, func_, params_)                 \
  do {                                                                     \
    if (!observers_.IsEmpty()) {                                           \
      for (nsCOMPtr<nsIThreadObserver> obs_ : observers_.ForwardRange()) { \
        obs_->func_ params_;                                               \
      }                                                                    \
    }                                                                      \
  } while (0)

#ifdef MOZ_COLLECTING_RUNNABLE_TELEMETRY
// static
bool nsThread::GetLabeledRunnableName(nsIRunnable* aEvent, nsACString& aName,
                                      EventQueuePriority aPriority) {
  bool labeled = false;
  if (RefPtr<SchedulerGroup::Runnable> groupRunnable = do_QueryObject(aEvent)) {
    labeled = true;
    MOZ_ALWAYS_TRUE(NS_SUCCEEDED(groupRunnable->GetName(aName)));
  } else if (nsCOMPtr<nsINamed> named = do_QueryInterface(aEvent)) {
    MOZ_ALWAYS_TRUE(NS_SUCCEEDED(named->GetName(aName)));
  } else {
    aName.AssignLiteral("non-nsINamed runnable");
  }
  if (aName.IsEmpty()) {
    aName.AssignLiteral("anonymous runnable");
  }

  if (!labeled && aPriority > EventQueuePriority::InputHigh) {
    aName.AppendLiteral("(unlabeled)");
  }

  return labeled;
}
#endif

mozilla::PerformanceCounter* nsThread::GetPerformanceCounter(
    nsIRunnable* aEvent) const {
  return GetPerformanceCounterBase(aEvent);
}

// static
mozilla::PerformanceCounter* nsThread::GetPerformanceCounterBase(
    nsIRunnable* aEvent) {
  RefPtr<SchedulerGroup::Runnable> docRunnable = do_QueryObject(aEvent);
  if (docRunnable) {
    return docRunnable->GetPerformanceCounter();
  }
  return nullptr;
}

size_t nsThread::ShallowSizeOfIncludingThis(
    mozilla::MallocSizeOf aMallocSizeOf) const {
  size_t n = 0;
  if (mShutdownContext) {
    n += aMallocSizeOf(mShutdownContext);
  }
  n += mRequestedShutdownContexts.ShallowSizeOfExcludingThis(aMallocSizeOf);
  return aMallocSizeOf(this) + aMallocSizeOf(mThread) + n;
}

size_t nsThread::SizeOfEventQueues(mozilla::MallocSizeOf aMallocSizeOf) const {
  size_t n = 0;
  if (mEventTarget) {
    // The size of mEvents is reported by mEventTarget.
    n += mEventTarget->SizeOfIncludingThis(aMallocSizeOf);
  }
  return n;
}

size_t nsThread::SizeOfIncludingThis(
    mozilla::MallocSizeOf aMallocSizeOf) const {
  return ShallowSizeOfIncludingThis(aMallocSizeOf) +
         SizeOfEventQueues(aMallocSizeOf);
}

NS_IMETHODIMP
nsThread::ProcessNextEvent(bool aMayWait, bool* aResult) {
  MOZ_ASSERT(mEvents);
  NS_ENSURE_TRUE(mEvents, NS_ERROR_NOT_IMPLEMENTED);

  LOG(("THRD(%p) ProcessNextEvent [%u %u]\n", this, aMayWait,
       mNestedEventLoopDepth));

  if (NS_WARN_IF(PR_GetCurrentThread() != mThread)) {
    return NS_ERROR_NOT_SAME_THREAD;
  }

  // The toplevel event loop normally blocks waiting for the next event, but
  // if we're trying to shut this thread down, we must exit the event loop
  // when the event queue is empty. This only applys to the toplevel event
  // loop! Nested event loops (e.g. during sync dispatch) are waiting for
  // some state change and must be able to block even if something has
  // requested shutdown of the thread. Otherwise we'll just busywait as we
  // endlessly look for an event, fail to find one, and repeat the nested
  // event loop since its state change hasn't happened yet.
  bool reallyWait = aMayWait && (mNestedEventLoopDepth > 0 || !ShuttingDown());

  if (mIsInLocalExecutionMode) {
    if (nsCOMPtr<nsIRunnable> event = mEvents->GetEvent(reallyWait)) {
      *aResult = true;
      LogRunnable::Run log(event);
      event->Run();
      event = nullptr;
    } else {
      *aResult = false;
    }
    return NS_OK;
  }

  Maybe<dom::AutoNoJSAPI> noJSAPI;

  if (mUseHangMonitor && reallyWait) {
    BackgroundHangMonitor().NotifyWait();
  }

  if (mIsMainThread) {
    DoMainThreadSpecificProcessing();
  }

  ++mNestedEventLoopDepth;

  // We only want to create an AutoNoJSAPI on threads that actually do DOM
  // stuff (including workers).  Those are exactly the threads that have an
  // mScriptObserver.
  bool callScriptObserver = !!mScriptObserver;
  if (callScriptObserver) {
    noJSAPI.emplace();
    mScriptObserver->BeforeProcessTask(reallyWait);
  }

#ifdef EARLY_BETA_OR_EARLIER
  // Need to capture mayWaitForWakeup state before OnProcessNextEvent,
  // since on the main thread OnProcessNextEvent ends up waiting for the new
  // events.
  bool mayWaitForWakeup = reallyWait && !mEvents->HasPendingEvent();
#endif

  nsCOMPtr<nsIThreadObserver> obs = mEvents->GetObserverOnThread();
  if (obs) {
    obs->OnProcessNextEvent(this, reallyWait);
  }

  NOTIFY_EVENT_OBSERVERS(EventQueue()->EventObservers(), OnProcessNextEvent,
                         (this, reallyWait));

#ifdef MOZ_CANARY
  Canary canary;
#endif
  nsresult rv = NS_OK;

  {
    // Scope for |event| to make sure that its destructor fires while
    // mNestedEventLoopDepth has been incremented, since that destructor can
    // also do work.
    nsCOMPtr<nsIRunnable> event;
    bool usingTaskController = mIsMainThread;
    if (usingTaskController) {
      event = TaskController::Get()->GetRunnableForMTTask(reallyWait);
    } else {
      event = mEvents->GetEvent(reallyWait, &mLastEventDelay);
    }

    *aResult = (event.get() != nullptr);

    if (event) {
#ifdef EARLY_BETA_OR_EARLIER
      if (mayWaitForWakeup && mThread) {
        ++mWakeupCount;
        if (mWakeupCount == kTelemetryWakeupCountLimit) {
          TimeStamp now = TimeStamp::Now();
          double ms = (now - mLastWakeupCheckTime).ToMilliseconds();
          if (ms < 0) {
            ms = 0;
          }
          const char* name = !mNameForWakeupTelemetry.IsEmpty()
                                 ? mNameForWakeupTelemetry.get()
                                 : PR_GetThreadName(mThread);
          if (!name) {
            name = mIsMainThread ? "MainThread" : "(nameless thread)";
          }
          nsDependentCString key(name);
          Telemetry::Accumulate(Telemetry::THREAD_WAKEUP, key,
                                static_cast<uint32_t>(ms));
          mLastWakeupCheckTime = now;
          mWakeupCount = 0;
        }
      }
#endif

      LOG(("THRD(%p) running [%p]\n", this, event.get()));

      Maybe<LogRunnable::Run> log;

      if (!usingTaskController) {
        log.emplace(event);
      }

      // Delay event processing to encourage whoever dispatched this event
      // to run.
      DelayForChaosMode(ChaosFeature::TaskRunning, 1000);

      mozilla::TimeStamp now = mozilla::TimeStamp::Now();

      if (mUseHangMonitor) {
        BackgroundHangMonitor().NotifyActivity();
      }

      Maybe<PerformanceCounterState::Snapshot> snapshot;
      if (!usingTaskController) {
        snapshot.emplace(mPerformanceCounterState.RunnableWillRun(
            GetPerformanceCounter(event), now, false));
      }

      mLastEventStart = now;

      if (!usingTaskController) {
        AUTO_PROFILE_FOLLOWING_RUNNABLE(event);
        event->Run();
      } else {
        // Avoid generating "Runnable" profiler markers for the
        // "TaskController::ExecutePendingMTTasks" runnables created
        // by TaskController, which already adds "Runnable" markers
        // when executing tasks.
        event->Run();
      }

      if (usingTaskController) {
        *aResult = TaskController::Get()->MTTaskRunnableProcessedTask();
      } else {
        mPerformanceCounterState.RunnableDidRun(std::move(snapshot.ref()));
      }

      // To cover the event's destructor code inside the LogRunnable span.
      event = nullptr;
    } else {
      mLastEventDelay = TimeDuration();
      mLastEventStart = TimeStamp();
      if (aMayWait) {
        MOZ_ASSERT(ShuttingDown(),
                   "This should only happen when shutting down");
        rv = NS_ERROR_UNEXPECTED;
      }
    }
  }

  DrainDirectTasks();

  NOTIFY_EVENT_OBSERVERS(EventQueue()->EventObservers(), AfterProcessNextEvent,
                         (this, *aResult));

  if (obs) {
    obs->AfterProcessNextEvent(this, *aResult);
  }

  // In case some EventObserver dispatched some direct tasks; process them
  // now.
  DrainDirectTasks();

  if (callScriptObserver) {
    if (mScriptObserver) {
      mScriptObserver->AfterProcessTask(mNestedEventLoopDepth);
    }
    noJSAPI.reset();
  }

  --mNestedEventLoopDepth;

  return rv;
}

//-----------------------------------------------------------------------------
// nsISupportsPriority

NS_IMETHODIMP
nsThread::GetPriority(int32_t* aPriority) {
  *aPriority = mPriority;
  return NS_OK;
}

NS_IMETHODIMP
nsThread::SetPriority(int32_t aPriority) {
  if (NS_WARN_IF(!mThread)) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  // NSPR defines the following four thread priorities:
  //   PR_PRIORITY_LOW
  //   PR_PRIORITY_NORMAL
  //   PR_PRIORITY_HIGH
  //   PR_PRIORITY_URGENT
  // We map the priority values defined on nsISupportsPriority to these
  // values.

  mPriority = aPriority;

  PRThreadPriority pri;
  if (mPriority <= PRIORITY_HIGHEST) {
    pri = PR_PRIORITY_URGENT;
  } else if (mPriority < PRIORITY_NORMAL) {
    pri = PR_PRIORITY_HIGH;
  } else if (mPriority > PRIORITY_NORMAL) {
    pri = PR_PRIORITY_LOW;
  } else {
    pri = PR_PRIORITY_NORMAL;
  }
  // If chaos mode is active, retain the randomly chosen priority
  if (!ChaosMode::isActive(ChaosFeature::ThreadScheduling)) {
    PR_SetThreadPriority(mThread, pri);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsThread::AdjustPriority(int32_t aDelta) {
  return SetPriority(mPriority + aDelta);
}

//-----------------------------------------------------------------------------
// nsIThreadInternal

NS_IMETHODIMP
nsThread::GetObserver(nsIThreadObserver** aObs) {
  MOZ_ASSERT(mEvents);
  NS_ENSURE_TRUE(mEvents, NS_ERROR_NOT_IMPLEMENTED);

  nsCOMPtr<nsIThreadObserver> obs = mEvents->GetObserver();
  obs.forget(aObs);
  return NS_OK;
}

NS_IMETHODIMP
nsThread::SetObserver(nsIThreadObserver* aObs) {
  MOZ_ASSERT(mEvents);
  NS_ENSURE_TRUE(mEvents, NS_ERROR_NOT_IMPLEMENTED);

  if (NS_WARN_IF(PR_GetCurrentThread() != mThread)) {
    return NS_ERROR_NOT_SAME_THREAD;
  }

  mEvents->SetObserver(aObs);
  return NS_OK;
}

uint32_t nsThread::RecursionDepth() const {
  MOZ_ASSERT(PR_GetCurrentThread() == mThread);
  return mNestedEventLoopDepth;
}

NS_IMETHODIMP
nsThread::AddObserver(nsIThreadObserver* aObserver) {
  MOZ_ASSERT(mEvents);
  NS_ENSURE_TRUE(mEvents, NS_ERROR_NOT_IMPLEMENTED);

  if (NS_WARN_IF(!aObserver)) {
    return NS_ERROR_INVALID_ARG;
  }
  if (NS_WARN_IF(PR_GetCurrentThread() != mThread)) {
    return NS_ERROR_NOT_SAME_THREAD;
  }

  EventQueue()->AddObserver(aObserver);

  return NS_OK;
}

NS_IMETHODIMP
nsThread::RemoveObserver(nsIThreadObserver* aObserver) {
  MOZ_ASSERT(mEvents);
  NS_ENSURE_TRUE(mEvents, NS_ERROR_NOT_IMPLEMENTED);

  if (NS_WARN_IF(PR_GetCurrentThread() != mThread)) {
    return NS_ERROR_NOT_SAME_THREAD;
  }

  EventQueue()->RemoveObserver(aObserver);

  return NS_OK;
}

void nsThread::SetScriptObserver(
    mozilla::CycleCollectedJSContext* aScriptObserver) {
  if (!aScriptObserver) {
    mScriptObserver = nullptr;
    return;
  }

  MOZ_ASSERT(!mScriptObserver);
  mScriptObserver = aScriptObserver;
}

void NS_DispatchMemoryPressure();

void nsThread::DoMainThreadSpecificProcessing() const {
  MOZ_ASSERT(mIsMainThread);

  ipc::CancelCPOWs();

  // Fire a memory pressure notification, if one is pending.
  if (!ShuttingDown()) {
    NS_DispatchMemoryPressure();
  }
}

NS_IMETHODIMP
nsThread::GetEventTarget(nsIEventTarget** aEventTarget) {
  nsCOMPtr<nsIEventTarget> target = this;
  target.forget(aEventTarget);
  return NS_OK;
}

//-----------------------------------------------------------------------------
// nsIDirectTaskDispatcher

NS_IMETHODIMP
nsThread::DispatchDirectTask(already_AddRefed<nsIRunnable> aEvent) {
  if (!IsOnCurrentThread()) {
    return NS_ERROR_FAILURE;
  }
  mDirectTasks.AddTask(std::move(aEvent));
  return NS_OK;
}

NS_IMETHODIMP nsThread::DrainDirectTasks() {
  if (!IsOnCurrentThread()) {
    return NS_ERROR_FAILURE;
  }
  mDirectTasks.DrainTasks();
  return NS_OK;
}

NS_IMETHODIMP nsThread::HaveDirectTasks(bool* aValue) {
  if (!IsOnCurrentThread()) {
    return NS_ERROR_FAILURE;
  }

  *aValue = mDirectTasks.HaveTasks();
  return NS_OK;
}

nsIEventTarget* nsThread::EventTarget() { return this; }

nsISerialEventTarget* nsThread::SerialEventTarget() { return this; }

void nsThread::OnDelayedRunnableCreated(mozilla::DelayedRunnable* aRunnable) {
  mEventTarget->OnDelayedRunnableCreated(aRunnable);
}

void nsThread::OnDelayedRunnableScheduled(mozilla::DelayedRunnable* aRunnable) {
  mEventTarget->OnDelayedRunnableScheduled(aRunnable);
}

void nsThread::OnDelayedRunnableRan(mozilla::DelayedRunnable* aRunnable) {
  mEventTarget->OnDelayedRunnableRan(aRunnable);
}

nsLocalExecutionRecord nsThread::EnterLocalExecution() {
  MOZ_RELEASE_ASSERT(!mIsInLocalExecutionMode);
  MOZ_ASSERT(IsOnCurrentThread());
  MOZ_ASSERT(EventQueue());
  return nsLocalExecutionRecord(*EventQueue(), mIsInLocalExecutionMode);
}

nsLocalExecutionGuard::nsLocalExecutionGuard(
    nsLocalExecutionRecord&& aLocalExecutionRecord)
    : mEventQueueStack(aLocalExecutionRecord.mEventQueueStack),
      mLocalEventTarget(mEventQueueStack.PushEventQueue()),
      mLocalExecutionFlag(aLocalExecutionRecord.mLocalExecutionFlag) {
  MOZ_ASSERT(mLocalEventTarget);
  MOZ_ASSERT(!mLocalExecutionFlag);
  mLocalExecutionFlag = true;
}

nsLocalExecutionGuard::~nsLocalExecutionGuard() {
  MOZ_ASSERT(mLocalExecutionFlag);
  mLocalExecutionFlag = false;
  mEventQueueStack.PopEventQueue(mLocalEventTarget);
}

namespace mozilla {
PerformanceCounterState::Snapshot PerformanceCounterState::RunnableWillRun(
    PerformanceCounter* aCounter, TimeStamp aNow, bool aIsIdleRunnable) {
  if (IsNestedRunnable()) {
    // Flush out any accumulated time that should be accounted to the
    // current runnable before we start running a nested runnable.
    MaybeReportAccumulatedTime(aNow);
  }

  Snapshot snapshot(mCurrentEventLoopDepth, mCurrentPerformanceCounter,
                    mCurrentRunnableIsIdleRunnable);

  mCurrentEventLoopDepth = mNestedEventLoopDepth;
  mCurrentPerformanceCounter = aCounter;
  mCurrentRunnableIsIdleRunnable = aIsIdleRunnable;
  mCurrentTimeSliceStart = aNow;

  return snapshot;
}

void PerformanceCounterState::RunnableDidRun(Snapshot&& aSnapshot) {
  // First thing: Restore our mCurrentEventLoopDepth so we can use
  // IsNestedRunnable().
  mCurrentEventLoopDepth = aSnapshot.mOldEventLoopDepth;

  // We may not need the current timestamp; don't bother computing it if we
  // don't.
  TimeStamp now;
  if (mCurrentPerformanceCounter || mIsMainThread || IsNestedRunnable()) {
    now = TimeStamp::Now();
  }
  if (mCurrentPerformanceCounter || mIsMainThread) {
    MaybeReportAccumulatedTime(now);
  }

  // And now restore the rest of our state.
  mCurrentPerformanceCounter = std::move(aSnapshot.mOldPerformanceCounter);
  mCurrentRunnableIsIdleRunnable = aSnapshot.mOldIsIdleRunnable;
  if (IsNestedRunnable()) {
    // Reset mCurrentTimeSliceStart to right now, so our parent runnable's
    // next slice can be properly accounted for.
    mCurrentTimeSliceStart = now;
  } else {
    // We are done at the outermost level; we are no longer in a timeslice.
    mCurrentTimeSliceStart = TimeStamp();
  }
}

void PerformanceCounterState::MaybeReportAccumulatedTime(TimeStamp aNow) {
  MOZ_ASSERT(mCurrentTimeSliceStart,
             "How did we get here if we're not in a timeslice?");

  if (!mCurrentPerformanceCounter && !mIsMainThread) {
    // No one cares about this timeslice.
    return;
  }

  TimeDuration duration = aNow - mCurrentTimeSliceStart;
  if (mCurrentPerformanceCounter) {
    mCurrentPerformanceCounter->IncrementExecutionDuration(
        duration.ToMicroseconds());
  }

  // Long tasks only matter on the main thread.
  if (mIsMainThread && duration.ToMilliseconds() > LONGTASK_BUSY_WINDOW_MS) {
    // Idle events (gc...) don't *really* count here
    if (!mCurrentRunnableIsIdleRunnable) {
      mLastLongNonIdleTaskEnd = aNow;
    }
    mLastLongTaskEnd = aNow;

    if (profiler_thread_is_being_profiled_for_markers()) {
      struct LongTaskMarker {
        static constexpr Span<const char> MarkerTypeName() {
          return MakeStringSpan("MainThreadLongTask");
        }
        static void StreamJSONMarkerData(
            baseprofiler::SpliceableJSONWriter& aWriter) {
          aWriter.StringProperty("category", "LongTask");
        }
        static MarkerSchema MarkerTypeDisplay() {
          using MS = MarkerSchema;
          MS schema{MS::Location::MarkerChart, MS::Location::MarkerTable};
          schema.AddKeyLabelFormat("category", "Type", MS::Format::String);
          return schema;
        }
      };

      profiler_add_marker(mCurrentRunnableIsIdleRunnable
                              ? ProfilerString8View("LongIdleTask")
                              : ProfilerString8View("LongTask"),
                          geckoprofiler::category::OTHER,
                          MarkerTiming::Interval(mCurrentTimeSliceStart, aNow),
                          LongTaskMarker{});
    }
  }
}

}  // namespace mozilla
