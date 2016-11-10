/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsThread.h"

#include "base/message_loop.h"

// Chromium's logging can sometimes leak through...
#ifdef LOG
#undef LOG
#endif

#include "mozilla/ReentrantMonitor.h"
#include "nsMemoryPressure.h"
#include "nsThreadManager.h"
#include "nsIClassInfoImpl.h"
#include "nsAutoPtr.h"
#include "nsCOMPtr.h"
#include "nsQueryObject.h"
#include "pratom.h"
#include "mozilla/CycleCollectedJSContext.h"
#include "mozilla/Logging.h"
#include "nsIObserverService.h"
#include "mozilla/HangMonitor.h"
#include "mozilla/IOInterposer.h"
#include "mozilla/ipc/MessageChannel.h"
#include "mozilla/ipc/BackgroundChild.h"
#include "mozilla/Services.h"
#include "nsXPCOMPrivate.h"
#include "mozilla/ChaosMode.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/Unused.h"
#include "mozilla/dom/ScriptSettings.h"
#include "nsIIdlePeriod.h"
#include "nsIIncrementalRunnable.h"
#include "nsThreadSyncDispatch.h"
#include "LeakRefPtr.h"

#ifdef MOZ_CRASHREPORTER
#include "nsServiceManagerUtils.h"
#include "nsICrashReporter.h"
#include "mozilla/dom/ContentChild.h"
#endif

#ifdef XP_LINUX
#include <sys/time.h>
#include <sys/resource.h>
#include <sched.h>
#endif

#define HAVE_UALARM _BSD_SOURCE || (_XOPEN_SOURCE >= 500 ||                 \
                      _XOPEN_SOURCE && _XOPEN_SOURCE_EXTENDED) &&           \
                      !(_POSIX_C_SOURCE >= 200809L || _XOPEN_SOURCE >= 700)

#if defined(XP_LINUX) && !defined(ANDROID) && defined(_GNU_SOURCE)
#define HAVE_SCHED_SETAFFINITY
#endif

#ifdef XP_MACOSX
#include <mach/mach.h>
#include <mach/thread_policy.h>
#endif

#ifdef MOZ_CANARY
# include <unistd.h>
# include <execinfo.h>
# include <signal.h>
# include <fcntl.h>
# include "nsXULAppAPI.h"
#endif

#if defined(NS_FUNCTION_TIMER) && defined(_MSC_VER)
#include "nsTimerImpl.h"
#include "mozilla/StackWalk.h"
#endif
#ifdef NS_FUNCTION_TIMER
#include "nsCRT.h"
#endif

#ifdef MOZ_TASK_TRACER
#include "GeckoTaskTracer.h"
#include "TracedTaskCommon.h"
using namespace mozilla::tasktracer;
#endif

using namespace mozilla;

static LazyLogModule sThreadLog("nsThread");
#ifdef LOG
#undef LOG
#endif
#define LOG(args) MOZ_LOG(sThreadLog, mozilla::LogLevel::Debug, args)

NS_DECL_CI_INTERFACE_GETTER(nsThread)

//-----------------------------------------------------------------------------
// Because we do not have our own nsIFactory, we have to implement nsIClassInfo
// somewhat manually.

class nsThreadClassInfo : public nsIClassInfo
{
public:
  NS_DECL_ISUPPORTS_INHERITED  // no mRefCnt
  NS_DECL_NSICLASSINFO

  nsThreadClassInfo()
  {
  }
};

NS_IMETHODIMP_(MozExternalRefCountType)
nsThreadClassInfo::AddRef()
{
  return 2;
}
NS_IMETHODIMP_(MozExternalRefCountType)
nsThreadClassInfo::Release()
{
  return 1;
}
NS_IMPL_QUERY_INTERFACE(nsThreadClassInfo, nsIClassInfo)

NS_IMETHODIMP
nsThreadClassInfo::GetInterfaces(uint32_t* aCount, nsIID*** aArray)
{
  return NS_CI_INTERFACE_GETTER_NAME(nsThread)(aCount, aArray);
}

NS_IMETHODIMP
nsThreadClassInfo::GetScriptableHelper(nsIXPCScriptable** aResult)
{
  *aResult = nullptr;
  return NS_OK;
}

NS_IMETHODIMP
nsThreadClassInfo::GetContractID(char** aResult)
{
  *aResult = nullptr;
  return NS_OK;
}

NS_IMETHODIMP
nsThreadClassInfo::GetClassDescription(char** aResult)
{
  *aResult = nullptr;
  return NS_OK;
}

NS_IMETHODIMP
nsThreadClassInfo::GetClassID(nsCID** aResult)
{
  *aResult = nullptr;
  return NS_OK;
}

NS_IMETHODIMP
nsThreadClassInfo::GetFlags(uint32_t* aResult)
{
  *aResult = THREADSAFE;
  return NS_OK;
}

NS_IMETHODIMP
nsThreadClassInfo::GetClassIDNoAlloc(nsCID* aResult)
{
  return NS_ERROR_NOT_AVAILABLE;
}

//-----------------------------------------------------------------------------

NS_IMPL_ADDREF(nsThread)
NS_IMPL_RELEASE(nsThread)
NS_INTERFACE_MAP_BEGIN(nsThread)
  NS_INTERFACE_MAP_ENTRY(nsIThread)
  NS_INTERFACE_MAP_ENTRY(nsIThreadInternal)
  NS_INTERFACE_MAP_ENTRY(nsIEventTarget)
  NS_INTERFACE_MAP_ENTRY(nsISupportsPriority)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIThread)
  if (aIID.Equals(NS_GET_IID(nsIClassInfo))) {
    static nsThreadClassInfo sThreadClassInfo;
    foundInterface = static_cast<nsIClassInfo*>(&sThreadClassInfo);
  } else
NS_INTERFACE_MAP_END
NS_IMPL_CI_INTERFACE_GETTER(nsThread, nsIThread, nsIThreadInternal,
                            nsIEventTarget, nsISupportsPriority)

//-----------------------------------------------------------------------------

class nsThreadStartupEvent : public Runnable
{
public:
  nsThreadStartupEvent()
    : mMon("nsThreadStartupEvent.mMon")
    , mInitialized(false)
  {
  }

  // This method does not return until the thread startup object is in the
  // completion state.
  void Wait()
  {
    ReentrantMonitorAutoEnter mon(mMon);
    while (!mInitialized) {
      mon.Wait();
    }
  }

  // This method needs to be public to support older compilers (xlC_r on AIX).
  // It should be called directly as this class type is reference counted.
  virtual ~nsThreadStartupEvent() {}

private:
  NS_IMETHOD Run() override
  {
    ReentrantMonitorAutoEnter mon(mMon);
    mInitialized = true;
    mon.Notify();
    return NS_OK;
  }

  ReentrantMonitor mMon;
  bool mInitialized;
};
//-----------------------------------------------------------------------------

namespace {
class DelayedRunnable : public Runnable,
                        public nsITimerCallback
{
public:
  DelayedRunnable(already_AddRefed<nsIThread> aTargetThread,
                  already_AddRefed<nsIRunnable> aRunnable,
                  uint32_t aDelay)
    : mTargetThread(aTargetThread),
      mWrappedRunnable(aRunnable),
      mDelayedFrom(TimeStamp::NowLoRes()),
      mDelay(aDelay)
  { }

  NS_DECL_ISUPPORTS_INHERITED

  nsresult Init()
  {
    nsresult rv;
    mTimer = do_CreateInstance(NS_TIMER_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    MOZ_ASSERT(mTimer);
    rv = mTimer->SetTarget(mTargetThread);

    NS_ENSURE_SUCCESS(rv, rv);
    return mTimer->InitWithCallback(this, mDelay, nsITimer::TYPE_ONE_SHOT);
  }

  nsresult DoRun()
  {
    nsCOMPtr<nsIRunnable> r = mWrappedRunnable.forget();
    return r->Run();
  }

  NS_IMETHOD Run() override
  {
    // Already ran?
    if (!mWrappedRunnable) {
      return NS_OK;
    }

    // Are we too early?
    if ((TimeStamp::NowLoRes() - mDelayedFrom).ToMilliseconds() < mDelay) {
      return NS_OK; // Let the nsITimer run us.
    }

    mTimer->Cancel();
    return DoRun();
  }

  NS_IMETHOD Notify(nsITimer* aTimer) override
  {
    // If we already ran, the timer should have been canceled.
    MOZ_ASSERT(mWrappedRunnable);
    MOZ_ASSERT(aTimer == mTimer);

    return DoRun();
  }

private:
  ~DelayedRunnable() {}

  nsCOMPtr<nsIThread> mTargetThread;
  nsCOMPtr<nsIRunnable> mWrappedRunnable;
  nsCOMPtr<nsITimer> mTimer;
  TimeStamp mDelayedFrom;
  uint32_t mDelay;
};

NS_IMPL_ISUPPORTS_INHERITED(DelayedRunnable, Runnable, nsITimerCallback)

} // anonymous namespace

//-----------------------------------------------------------------------------

struct nsThreadShutdownContext
{
  nsThreadShutdownContext(NotNull<nsThread*> aTerminatingThread,
                          NotNull<nsThread*> aJoiningThread,
                          bool      aAwaitingShutdownAck)
    : mTerminatingThread(aTerminatingThread)
    , mJoiningThread(aJoiningThread)
    , mAwaitingShutdownAck(aAwaitingShutdownAck)
  {
    MOZ_COUNT_CTOR(nsThreadShutdownContext);
  }
  ~nsThreadShutdownContext()
  {
    MOZ_COUNT_DTOR(nsThreadShutdownContext);
  }

  // NB: This will be the last reference.
  NotNull<RefPtr<nsThread>> mTerminatingThread;
  NotNull<nsThread*> mJoiningThread;
  bool mAwaitingShutdownAck;
};

// This event is responsible for notifying nsThread::Shutdown that it is time
// to call PR_JoinThread. It implements nsICancelableRunnable so that it can
// run on a DOM Worker thread (where all events must implement
// nsICancelableRunnable.)
class nsThreadShutdownAckEvent : public CancelableRunnable
{
public:
  explicit nsThreadShutdownAckEvent(NotNull<nsThreadShutdownContext*> aCtx)
    : mShutdownContext(aCtx)
  {
  }
  NS_IMETHOD Run() override
  {
    mShutdownContext->mTerminatingThread->ShutdownComplete(mShutdownContext);
    return NS_OK;
  }
  nsresult Cancel() override
  {
    return Run();
  }
private:
  virtual ~nsThreadShutdownAckEvent() { }

  NotNull<nsThreadShutdownContext*> mShutdownContext;
};

// This event is responsible for setting mShutdownContext
class nsThreadShutdownEvent : public Runnable
{
public:
  nsThreadShutdownEvent(NotNull<nsThread*> aThr,
                        NotNull<nsThreadShutdownContext*> aCtx)
    : mThread(aThr)
    , mShutdownContext(aCtx)
  {
  }
  NS_IMETHOD Run() override
  {
    mThread->mShutdownContext = mShutdownContext;
    MessageLoop::current()->Quit();
    return NS_OK;
  }
private:
  NotNull<RefPtr<nsThread>> mThread;
  NotNull<nsThreadShutdownContext*> mShutdownContext;
};

//-----------------------------------------------------------------------------

static void
SetThreadAffinity(unsigned int cpu)
{
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
  MOZ_ALWAYS_TRUE(SetThreadIdealProcessor(GetCurrentThread(), cpu) != -1);
#endif
}

static void
SetupCurrentThreadForChaosMode()
{
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
  // We should set the affinity here but NSPR doesn't provide a way to expose it.
  uint32_t priority = ChaosMode::randomUint32LessThan(PR_PRIORITY_LAST + 1);
  PR_SetThreadPriority(PR_GetCurrentThread(), PRThreadPriority(priority));
#endif

  // Force half the threads to CPU 0 so they compete for CPU
  if (ChaosMode::randomUint32LessThan(2)) {
    SetThreadAffinity(0);
  }
}

/*static*/ void
nsThread::ThreadFunc(void* aArg)
{
  using mozilla::ipc::BackgroundChild;

  nsThread* self = static_cast<nsThread*>(aArg);  // strong reference
  self->mThread = PR_GetCurrentThread();
  SetupCurrentThreadForChaosMode();

  // Inform the ThreadManager
  nsThreadManager::get().RegisterCurrentThread(*self);

  mozilla::IOInterposer::RegisterCurrentThread();

  // Wait for and process startup event
  nsCOMPtr<nsIRunnable> event;
  {
    MutexAutoLock lock(self->mLock);
    if (!self->mEvents->GetEvent(true, getter_AddRefs(event), lock)) {
      NS_WARNING("failed waiting for thread startup event");
      return;
    }
  }
  event->Run();  // unblocks nsThread::Init
  event = nullptr;

  {
    // Scope for MessageLoop.
    nsAutoPtr<MessageLoop> loop(
      new MessageLoop(MessageLoop::TYPE_MOZILLA_NONMAINTHREAD, self));

    // Now, process incoming events...
    loop->Run();

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

      {
        MutexAutoLock lock(self->mLock);
        if (!self->mEvents->HasPendingEvent(lock)) {
          // No events in the queue, so we will stop now. Don't let any more
          // events be added, since they won't be processed. It is critical
          // that no PutEvent can occur between testing that the event queue is
          // empty and setting mEventsAreDoomed!
          self->mEventsAreDoomed = true;
          break;
        }
      }
      NS_ProcessPendingEvents(self);
    }
  }

  mozilla::IOInterposer::UnregisterCurrentThread();

  // Inform the threadmanager that this thread is going away
  nsThreadManager::get().UnregisterCurrentThread(*self);

  // Dispatch shutdown ACK
  NotNull<nsThreadShutdownContext*> context =
    WrapNotNull(self->mShutdownContext);
  MOZ_ASSERT(context->mTerminatingThread == self);
  event = do_QueryObject(new nsThreadShutdownAckEvent(context));
  context->mJoiningThread->Dispatch(event, NS_DISPATCH_NORMAL);

  // Release any observer of the thread here.
  self->SetObserver(nullptr);

#ifdef MOZ_TASK_TRACER
  FreeTraceInfo();
#endif

  NS_RELEASE(self);
}

//-----------------------------------------------------------------------------

#ifdef MOZ_CRASHREPORTER
// Tell the crash reporter to save a memory report if our heuristics determine
// that an OOM failure is likely to occur soon.
// Memory usage will not be checked more than every 30 seconds or saved more
// than every 3 minutes
// If |aShouldSave == kForceReport|, a report will be saved regardless of
// whether the process is low on memory or not. However, it will still not be
// saved if a report was saved less than 3 minutes ago.
bool
nsThread::SaveMemoryReportNearOOM(ShouldSaveMemoryReport aShouldSave)
{
  // Keep an eye on memory usage (cheap, ~7ms) somewhat frequently,
  // but save memory reports (expensive, ~75ms) less frequently.
  const size_t kLowMemoryCheckSeconds = 30;
  const size_t kLowMemorySaveSeconds = 3 * 60;

  static TimeStamp nextCheck = TimeStamp::NowLoRes()
    + TimeDuration::FromSeconds(kLowMemoryCheckSeconds);
  static bool recentlySavedReport = false; // Keeps track of whether a report
                                           // was saved last time we checked

  // Are we checking again too soon?
  TimeStamp now = TimeStamp::NowLoRes();
  if ((aShouldSave == ShouldSaveMemoryReport::kMaybeReport ||
      recentlySavedReport) && now < nextCheck) {
    return false;
  }

  bool needMemoryReport = (aShouldSave == ShouldSaveMemoryReport::kForceReport);
#ifdef XP_WIN // XXX implement on other platforms as needed
  // If the report is forced there is no need to check whether it is necessary
  if (aShouldSave != ShouldSaveMemoryReport::kForceReport) {
    const size_t LOWMEM_THRESHOLD_VIRTUAL = 200 * 1024 * 1024;
    MEMORYSTATUSEX statex;
    statex.dwLength = sizeof(statex);
    if (GlobalMemoryStatusEx(&statex)) {
      if (statex.ullAvailVirtual < LOWMEM_THRESHOLD_VIRTUAL) {
        needMemoryReport = true;
      }
    }
  }
#endif

  if (needMemoryReport) {
    if (XRE_IsContentProcess()) {
      dom::ContentChild* cc = dom::ContentChild::GetSingleton();
      if (cc) {
        cc->SendNotifyLowMemory();
      }
    } else {
      nsCOMPtr<nsICrashReporter> cr =
        do_GetService("@mozilla.org/toolkit/crash-reporter;1");
      if (cr) {
        cr->SaveMemoryReport();
      }
    }
    recentlySavedReport = true;
    nextCheck = now + TimeDuration::FromSeconds(kLowMemorySaveSeconds);
  } else {
    recentlySavedReport = false;
    nextCheck = now + TimeDuration::FromSeconds(kLowMemoryCheckSeconds);
  }

  return recentlySavedReport;
}
#endif

#ifdef MOZ_CANARY
int sCanaryOutputFD = -1;
#endif

nsThread::nsThread(MainThreadFlag aMainThread, uint32_t aStackSize)
  : mLock("nsThread.mLock")
  , mScriptObserver(nullptr)
  , mEvents(WrapNotNull(&mEventsRoot))
  , mEventsRoot(mLock)
  , mIdleEventsAvailable(mLock, "[nsThread.mEventsAvailable]")
  , mIdleEvents(mIdleEventsAvailable, nsEventQueue::eNormalQueue)
  , mPriority(PRIORITY_NORMAL)
  , mThread(nullptr)
  , mNestedEventLoopDepth(0)
  , mStackSize(aStackSize)
  , mShutdownContext(nullptr)
  , mShutdownRequired(false)
  , mEventsAreDoomed(false)
  , mIsMainThread(aMainThread)
  , mCanInvokeJS(false)
{
}

nsThread::~nsThread()
{
  NS_ASSERTION(mRequestedShutdownContexts.IsEmpty(),
               "shouldn't be waiting on other threads to shutdown");
#ifdef DEBUG
  // We deliberately leak these so they can be tracked by the leak checker.
  // If you're having nsThreadShutdownContext leaks, you can set:
  //   XPCOM_MEM_LOG_CLASSES=nsThreadShutdownContext
  // during a test run and that will at least tell you what thread is
  // requesting shutdown on another, which can be helpful for diagnosing
  // the leak.
  for (size_t i = 0; i < mRequestedShutdownContexts.Length(); ++i) {
    Unused << mRequestedShutdownContexts[i].forget();
  }
#endif
}

nsresult
nsThread::Init()
{
  // spawn thread and wait until it is fully setup
  RefPtr<nsThreadStartupEvent> startup = new nsThreadStartupEvent();

  NS_ADDREF_THIS();

  mIdlePeriod = new IdlePeriod();

  mShutdownRequired = true;

  // ThreadFunc is responsible for setting mThread
  if (!PR_CreateThread(PR_USER_THREAD, ThreadFunc, this,
                       PR_PRIORITY_NORMAL, PR_GLOBAL_THREAD,
                       PR_JOINABLE_THREAD, mStackSize)) {
    NS_RELEASE_THIS();
    return NS_ERROR_OUT_OF_MEMORY;
  }

  // ThreadFunc will wait for this event to be run before it tries to access
  // mThread.  By delaying insertion of this event into the queue, we ensure
  // that mThread is set properly.
  {
    MutexAutoLock lock(mLock);
    mEventsRoot.PutEvent(startup, lock); // retain a reference
  }

  // Wait for thread to call ThreadManager::SetupCurrentThread, which completes
  // initialization of ThreadFunc.
  startup->Wait();
  return NS_OK;
}

nsresult
nsThread::InitCurrentThread()
{
  mThread = PR_GetCurrentThread();
  SetupCurrentThreadForChaosMode();

  mIdlePeriod = new IdlePeriod();

  nsThreadManager::get().RegisterCurrentThread(*this);
  return NS_OK;
}

nsresult
nsThread::PutEvent(nsIRunnable* aEvent, nsNestedEventTarget* aTarget)
{
  nsCOMPtr<nsIRunnable> event(aEvent);
  return PutEvent(event.forget(), aTarget);
}

nsresult
nsThread::PutEvent(already_AddRefed<nsIRunnable> aEvent, nsNestedEventTarget* aTarget)
{
  // We want to leak the reference when we fail to dispatch it, so that
  // we won't release the event in a wrong thread.
  LeakRefPtr<nsIRunnable> event(Move(aEvent));
  nsCOMPtr<nsIThreadObserver> obs;

  {
    MutexAutoLock lock(mLock);
    nsChainedEventQueue* queue = aTarget ? aTarget->mQueue : &mEventsRoot;
    if (!queue || (queue == &mEventsRoot && mEventsAreDoomed)) {
      NS_WARNING("An event was posted to a thread that will never run it (rejected)");
      return NS_ERROR_UNEXPECTED;
    }
    queue->PutEvent(event.take(), lock);

    // Make sure to grab the observer before dropping the lock, otherwise the
    // event that we just placed into the queue could run and eventually delete
    // this nsThread before the calling thread is scheduled again. We would then
    // crash while trying to access a dead nsThread.
    obs = mObserver;
  }

  if (obs) {
    obs->OnDispatchedEvent(this);
  }

  return NS_OK;
}

nsresult
nsThread::DispatchInternal(already_AddRefed<nsIRunnable> aEvent, uint32_t aFlags,
                           nsNestedEventTarget* aTarget)
{
  // We want to leak the reference when we fail to dispatch it, so that
  // we won't release the event in a wrong thread.
  LeakRefPtr<nsIRunnable> event(Move(aEvent));
  if (NS_WARN_IF(!event)) {
    return NS_ERROR_INVALID_ARG;
  }

  if (gXPCOMThreadsShutDown && MAIN_THREAD != mIsMainThread && !aTarget) {
    NS_ASSERTION(false, "Failed Dispatch after xpcom-shutdown-threads");
    return NS_ERROR_ILLEGAL_DURING_SHUTDOWN;
  }

#ifdef MOZ_TASK_TRACER
  nsCOMPtr<nsIRunnable> tracedRunnable = CreateTracedRunnable(event.take());
  (static_cast<TracedRunnable*>(tracedRunnable.get()))->DispatchTask();
  // XXX tracedRunnable will always leaked when we fail to disptch.
  event = tracedRunnable.forget();
#endif

  if (aFlags & DISPATCH_SYNC) {
    nsThread* thread = nsThreadManager::get().GetCurrentThread();
    if (NS_WARN_IF(!thread)) {
      return NS_ERROR_NOT_AVAILABLE;
    }

    // XXX we should be able to do something better here... we should
    //     be able to monitor the slot occupied by this event and use
    //     that to tell us when the event has been processed.

    RefPtr<nsThreadSyncDispatch> wrapper =
      new nsThreadSyncDispatch(thread, event.take());
    nsresult rv = PutEvent(wrapper, aTarget); // hold a ref
    // Don't wait for the event to finish if we didn't dispatch it...
    if (NS_FAILED(rv)) {
      // PutEvent leaked the wrapper runnable object on failure, so we
      // explicitly release this object once for that. Note that this
      // object will be released again soon because it exits the scope.
      wrapper.get()->Release();
      return rv;
    }

    // Allows waiting; ensure no locks are held that would deadlock us!
    while (wrapper->IsPending()) {
      NS_ProcessNextEvent(thread, true);
    }
    return NS_OK;
  }

  NS_ASSERTION(aFlags == NS_DISPATCH_NORMAL ||
               aFlags == NS_DISPATCH_AT_END, "unexpected dispatch flags");
  return PutEvent(event.take(), aTarget);
}

bool
nsThread::nsChainedEventQueue::GetEvent(bool aMayWait, nsIRunnable** aEvent,
                                        mozilla::MutexAutoLock& aProofOfLock)
{
  bool retVal = false;
  do {
    if (mProcessSecondaryQueueRunnable) {
      MOZ_ASSERT(mSecondaryQueue->HasPendingEvent(aProofOfLock));
      retVal = mSecondaryQueue->GetEvent(aMayWait, aEvent, aProofOfLock);
      MOZ_ASSERT(*aEvent);
      mProcessSecondaryQueueRunnable = false;
      return retVal;
    }

    // We don't want to wait if mSecondaryQueue has some events.
    bool reallyMayWait =
      aMayWait && !mSecondaryQueue->HasPendingEvent(aProofOfLock);
    retVal =
      mNormalQueue->GetEvent(reallyMayWait, aEvent, aProofOfLock);

    // Let's see if we should next time process an event from the secondary
    // queue.
    mProcessSecondaryQueueRunnable =
      mSecondaryQueue->HasPendingEvent(aProofOfLock);

    if (*aEvent) {
      // We got an event, return early.
      return retVal;
    }
  } while(aMayWait || mProcessSecondaryQueueRunnable);

  return retVal;
}

//-----------------------------------------------------------------------------
// nsIEventTarget

NS_IMETHODIMP
nsThread::DispatchFromScript(nsIRunnable* aEvent, uint32_t aFlags)
{
  nsCOMPtr<nsIRunnable> event(aEvent);
  return Dispatch(event.forget(), aFlags);
}

NS_IMETHODIMP
nsThread::Dispatch(already_AddRefed<nsIRunnable> aEvent, uint32_t aFlags)
{
  LOG(("THRD(%p) Dispatch [%p %x]\n", this, /* XXX aEvent */nullptr, aFlags));

  return DispatchInternal(Move(aEvent), aFlags, nullptr);
}

NS_IMETHODIMP
nsThread::DelayedDispatch(already_AddRefed<nsIRunnable> aEvent, uint32_t aDelayMs)
{
  NS_ENSURE_TRUE(!!aDelayMs, NS_ERROR_UNEXPECTED);

  RefPtr<DelayedRunnable> r = new DelayedRunnable(Move(do_AddRef(this)),
                                                  Move(aEvent),
                                                  aDelayMs);
  nsresult rv = r->Init();
  NS_ENSURE_SUCCESS(rv, rv);

  return DispatchInternal(r.forget(), 0, nullptr);
}

NS_IMETHODIMP
nsThread::IsOnCurrentThread(bool* aResult)
{
  *aResult = (PR_GetCurrentThread() == mThread);
  return NS_OK;
}

//-----------------------------------------------------------------------------
// nsIThread

NS_IMETHODIMP
nsThread::GetPRThread(PRThread** aResult)
{
  *aResult = mThread;
  return NS_OK;
}

NS_IMETHODIMP
nsThread::GetCanInvokeJS(bool* aResult)
{
  *aResult = mCanInvokeJS;
  return NS_OK;
}

NS_IMETHODIMP
nsThread::SetCanInvokeJS(bool aCanInvokeJS)
{
  mCanInvokeJS = aCanInvokeJS;
  return NS_OK;
}

NS_IMETHODIMP
nsThread::AsyncShutdown()
{
  LOG(("THRD(%p) async shutdown\n", this));

  // XXX If we make this warn, then we hit that warning at xpcom shutdown while
  //     shutting down a thread in a thread pool.  That happens b/c the thread
  //     in the thread pool is already shutdown by the thread manager.
  if (!mThread) {
    return NS_OK;
  }

  return !!ShutdownInternal(/* aSync = */ false) ? NS_OK : NS_ERROR_UNEXPECTED;
}

nsThreadShutdownContext*
nsThread::ShutdownInternal(bool aSync)
{
  MOZ_ASSERT(mThread);
  MOZ_ASSERT(mThread != PR_GetCurrentThread());
  if (NS_WARN_IF(mThread == PR_GetCurrentThread())) {
    return nullptr;
  }

  // Prevent multiple calls to this method
  {
    MutexAutoLock lock(mLock);
    if (!mShutdownRequired) {
      return nullptr;
    }
    mShutdownRequired = false;
  }

  NotNull<nsThread*> currentThread =
    WrapNotNull(nsThreadManager::get().GetCurrentThread());

  nsAutoPtr<nsThreadShutdownContext>& context =
    *currentThread->mRequestedShutdownContexts.AppendElement();
  context = new nsThreadShutdownContext(WrapNotNull(this), currentThread, aSync);

  // Set mShutdownContext and wake up the thread in case it is waiting for
  // events to process.
  nsCOMPtr<nsIRunnable> event =
    new nsThreadShutdownEvent(WrapNotNull(this), WrapNotNull(context.get()));
  // XXXroc What if posting the event fails due to OOM?
  PutEvent(event.forget(), nullptr);

  // We could still end up with other events being added after the shutdown
  // task, but that's okay because we process pending events in ThreadFunc
  // after setting mShutdownContext just before exiting.
  return context;
}

void
nsThread::ShutdownComplete(NotNull<nsThreadShutdownContext*> aContext)
{
  MOZ_ASSERT(mThread);
  MOZ_ASSERT(aContext->mTerminatingThread == this);

  if (aContext->mAwaitingShutdownAck) {
    // We're in a synchronous shutdown, so tell whatever is up the stack that
    // we're done and unwind the stack so it can call us again.
    aContext->mAwaitingShutdownAck = false;
    return;
  }

  // Now, it should be safe to join without fear of dead-locking.

  PR_JoinThread(mThread);
  mThread = nullptr;

  // We hold strong references to our event observers, and once the thread is
  // shut down the observers can't easily unregister themselves. Do it here
  // to avoid leaking.
  ClearObservers();

#ifdef DEBUG
  {
    MutexAutoLock lock(mLock);
    MOZ_ASSERT(!mObserver, "Should have been cleared at shutdown!");
  }
#endif

  // Delete aContext.
  MOZ_ALWAYS_TRUE(
    aContext->mJoiningThread->mRequestedShutdownContexts.RemoveElement(aContext));
}

void
nsThread::WaitForAllAsynchronousShutdowns()
{
  while (mRequestedShutdownContexts.Length()) {
    NS_ProcessNextEvent(this, true);
  }
}

NS_IMETHODIMP
nsThread::Shutdown()
{
  LOG(("THRD(%p) sync shutdown\n", this));

  // XXX If we make this warn, then we hit that warning at xpcom shutdown while
  //     shutting down a thread in a thread pool.  That happens b/c the thread
  //     in the thread pool is already shutdown by the thread manager.
  if (!mThread) {
    return NS_OK;
  }

  nsThreadShutdownContext* maybeContext = ShutdownInternal(/* aSync = */ true);
  NS_ENSURE_TRUE(maybeContext, NS_ERROR_UNEXPECTED);
  NotNull<nsThreadShutdownContext*> context = WrapNotNull(maybeContext);

  // Process events on the current thread until we receive a shutdown ACK.
  // Allows waiting; ensure no locks are held that would deadlock us!
  while (context->mAwaitingShutdownAck) {
    NS_ProcessNextEvent(context->mJoiningThread, true);
  }

  ShutdownComplete(context);

  return NS_OK;
}

NS_IMETHODIMP
nsThread::HasPendingEvents(bool* aResult)
{
  if (NS_WARN_IF(PR_GetCurrentThread() != mThread)) {
    return NS_ERROR_NOT_SAME_THREAD;
  }

  {
    MutexAutoLock lock(mLock);
    *aResult = mEvents->HasPendingEvent(lock);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsThread::RegisterIdlePeriod(already_AddRefed<nsIIdlePeriod> aIdlePeriod)
{
  if (NS_WARN_IF(PR_GetCurrentThread() != mThread)) {
    return NS_ERROR_NOT_SAME_THREAD;
  }

  MutexAutoLock lock(mLock);
  mIdlePeriod = aIdlePeriod;
  return NS_OK;
}

NS_IMETHODIMP
nsThread::IdleDispatch(already_AddRefed<nsIRunnable> aEvent)
{
  // Currently the only supported idle dispatch is from the same
  // thread. To support idle dispatch from another thread we need to
  // support waking threads that are waiting for an event queue that
  // isn't mIdleEvents.
  MOZ_ASSERT(PR_GetCurrentThread() == mThread);

  MutexAutoLock lock(mLock);
  LeakRefPtr<nsIRunnable> event(Move(aEvent));

  if (NS_WARN_IF(!event)) {
    return NS_ERROR_INVALID_ARG;
  }

  if (mEventsAreDoomed) {
    NS_WARNING("An idle event was posted to a thread that will never run it (rejected)");
    return NS_ERROR_UNEXPECTED;
  }

  mIdleEvents.PutEvent(event.take(), lock);
  return NS_OK;
}

#ifdef MOZ_CANARY
void canary_alarm_handler(int signum);

class Canary
{
  //XXX ToDo: support nested loops
public:
  Canary()
  {
    if (sCanaryOutputFD > 0 && EventLatencyIsImportant()) {
      signal(SIGALRM, canary_alarm_handler);
      ualarm(15000, 0);
    }
  }

  ~Canary()
  {
    if (sCanaryOutputFD != 0 && EventLatencyIsImportant()) {
      ualarm(0, 0);
    }
  }

  static bool EventLatencyIsImportant()
  {
    return NS_IsMainThread() && XRE_IsParentProcess();
  }
};

void canary_alarm_handler(int signum)
{
  void* array[30];
  const char msg[29] = "event took too long to run:\n";
  // use write to be safe in the signal handler
  write(sCanaryOutputFD, msg, sizeof(msg));
  backtrace_symbols_fd(array, backtrace(array, 30), sCanaryOutputFD);
}

#endif

#define NOTIFY_EVENT_OBSERVERS(func_, params_)                                 \
  PR_BEGIN_MACRO                                                               \
    if (!mEventObservers.IsEmpty()) {                                          \
      nsAutoTObserverArray<NotNull<nsCOMPtr<nsIThreadObserver>>, 2>::ForwardIterator \
        iter_(mEventObservers);                                                \
      nsCOMPtr<nsIThreadObserver> obs_;                                        \
      while (iter_.HasMore()) {                                                \
        obs_ = iter_.GetNext();                                                \
        obs_ -> func_ params_ ;                                                \
      }                                                                        \
    }                                                                          \
  PR_END_MACRO

void
nsThread::GetIdleEvent(nsIRunnable** aEvent, MutexAutoLock& aProofOfLock)
{
  MOZ_ASSERT(PR_GetCurrentThread() == mThread);
  MOZ_ASSERT(aEvent);

  TimeStamp idleDeadline;
  mIdlePeriod->GetIdlePeriodHint(&idleDeadline);

  if (!idleDeadline || idleDeadline < TimeStamp::Now()) {
    aEvent = nullptr;
    return;
  }

  mIdleEvents.GetEvent(false, aEvent, aProofOfLock);

  if (*aEvent) {
    nsCOMPtr<nsIIncrementalRunnable> incrementalEvent(do_QueryInterface(*aEvent));
    if (incrementalEvent) {
      incrementalEvent->SetDeadline(idleDeadline);
    }
  }
}

void
nsThread::GetEvent(bool aWait, nsIRunnable** aEvent, MutexAutoLock& aProofOfLock)
{
  MOZ_ASSERT(PR_GetCurrentThread() == mThread);
  MOZ_ASSERT(aEvent);

  // We'll try to get an event to execute in three stages.
  // [1] First we just try to get it from the regular queue without waiting.
  mEvents->GetEvent(false, aEvent, aProofOfLock);

  // [2] If we didn't get an event from the regular queue, try to
  // get one from the idle queue
  if (!*aEvent) {
    // Since events in mEvents have higher priority than idle
    // events, we will only consider idle events when there are no
    // pending events in mEvents. We will for the same reason never
    // wait for an idle event, since a higher priority event might
    // appear at any time.
    GetIdleEvent(aEvent, aProofOfLock);
  }

  // [3] If we neither got an event from the regular queue nor the
  // idle queue, then if we should wait for events we block on the
  // main queue until an event is available.
  // If we are shutting down, then do not wait for new events.
  if (!*aEvent && aWait) {
    mEvents->GetEvent(aWait, aEvent, aProofOfLock);
  }
}

NS_IMETHODIMP
nsThread::ProcessNextEvent(bool aMayWait, bool* aResult)
{
  LOG(("THRD(%p) ProcessNextEvent [%u %u]\n", this, aMayWait,
       mNestedEventLoopDepth));

  if (NS_WARN_IF(PR_GetCurrentThread() != mThread)) {
    return NS_ERROR_NOT_SAME_THREAD;
  }

  // The toplevel event loop normally blocks waiting for the next event, but
  // if we're trying to shut this thread down, we must exit the event loop when
  // the event queue is empty.
  // This only applys to the toplevel event loop! Nested event loops (e.g.
  // during sync dispatch) are waiting for some state change and must be able
  // to block even if something has requested shutdown of the thread. Otherwise
  // we'll just busywait as we endlessly look for an event, fail to find one,
  // and repeat the nested event loop since its state change hasn't happened yet.
  bool reallyWait = aMayWait && (mNestedEventLoopDepth > 0 || !ShuttingDown());

  if (mIsMainThread == MAIN_THREAD) {
    DoMainThreadSpecificProcessing(reallyWait);
  }

  ++mNestedEventLoopDepth;

  // We only want to create an AutoNoJSAPI on threads that actually do DOM stuff
  // (including workers).  Those are exactly the threads that have an
  // mScriptObserver.
  Maybe<dom::AutoNoJSAPI> noJSAPI;
  bool callScriptObserver = !!mScriptObserver;
  if (callScriptObserver) {
    noJSAPI.emplace();
    mScriptObserver->BeforeProcessTask(reallyWait);
  }

  nsCOMPtr<nsIThreadObserver> obs = mObserver;
  if (obs) {
    obs->OnProcessNextEvent(this, reallyWait);
  }

  NOTIFY_EVENT_OBSERVERS(OnProcessNextEvent, (this, reallyWait));

#ifdef MOZ_CANARY
  Canary canary;
#endif
  nsresult rv = NS_OK;

  {
    // Scope for |event| to make sure that its destructor fires while
    // mNestedEventLoopDepth has been incremented, since that destructor can
    // also do work.
    nsCOMPtr<nsIRunnable> event;
    {
      MutexAutoLock lock(mLock);
      GetEvent(reallyWait, getter_AddRefs(event), lock);
    }

    *aResult = (event.get() != nullptr);

    if (event) {
      LOG(("THRD(%p) running [%p]\n", this, event.get()));
      if (MAIN_THREAD == mIsMainThread) {
        HangMonitor::NotifyActivity();
      }
      event->Run();
    } else if (aMayWait) {
      MOZ_ASSERT(ShuttingDown(),
                 "This should only happen when shutting down");
      rv = NS_ERROR_UNEXPECTED;
    }
  }

  NOTIFY_EVENT_OBSERVERS(AfterProcessNextEvent, (this, *aResult));

  if (obs) {
    obs->AfterProcessNextEvent(this, *aResult);
  }

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
nsThread::GetPriority(int32_t* aPriority)
{
  *aPriority = mPriority;
  return NS_OK;
}

NS_IMETHODIMP
nsThread::SetPriority(int32_t aPriority)
{
  if (NS_WARN_IF(!mThread)) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  // NSPR defines the following four thread priorities:
  //   PR_PRIORITY_LOW
  //   PR_PRIORITY_NORMAL
  //   PR_PRIORITY_HIGH
  //   PR_PRIORITY_URGENT
  // We map the priority values defined on nsISupportsPriority to these values.

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
nsThread::AdjustPriority(int32_t aDelta)
{
  return SetPriority(mPriority + aDelta);
}

//-----------------------------------------------------------------------------
// nsIThreadInternal

NS_IMETHODIMP
nsThread::GetObserver(nsIThreadObserver** aObs)
{
  MutexAutoLock lock(mLock);
  NS_IF_ADDREF(*aObs = mObserver);
  return NS_OK;
}

NS_IMETHODIMP
nsThread::SetObserver(nsIThreadObserver* aObs)
{
  if (NS_WARN_IF(PR_GetCurrentThread() != mThread)) {
    return NS_ERROR_NOT_SAME_THREAD;
  }

  MutexAutoLock lock(mLock);
  mObserver = aObs;
  return NS_OK;
}

uint32_t
nsThread::RecursionDepth() const
{
  MOZ_ASSERT(PR_GetCurrentThread() == mThread);
  return mNestedEventLoopDepth;
}

NS_IMETHODIMP
nsThread::AddObserver(nsIThreadObserver* aObserver)
{
  if (NS_WARN_IF(!aObserver)) {
    return NS_ERROR_INVALID_ARG;
  }
  if (NS_WARN_IF(PR_GetCurrentThread() != mThread)) {
    return NS_ERROR_NOT_SAME_THREAD;
  }

  NS_WARNING_ASSERTION(!mEventObservers.Contains(aObserver),
                       "Adding an observer twice!");

  if (!mEventObservers.AppendElement(WrapNotNull(aObserver))) {
    NS_WARNING("Out of memory!");
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsThread::RemoveObserver(nsIThreadObserver* aObserver)
{
  if (NS_WARN_IF(PR_GetCurrentThread() != mThread)) {
    return NS_ERROR_NOT_SAME_THREAD;
  }

  if (aObserver && !mEventObservers.RemoveElement(aObserver)) {
    NS_WARNING("Removing an observer that was never added!");
  }

  return NS_OK;
}

NS_IMETHODIMP
nsThread::PushEventQueue(nsIEventTarget** aResult)
{
  if (NS_WARN_IF(PR_GetCurrentThread() != mThread)) {
    return NS_ERROR_NOT_SAME_THREAD;
  }

  NotNull<nsChainedEventQueue*> queue =
    WrapNotNull(new nsChainedEventQueue(mLock));
  queue->mEventTarget = new nsNestedEventTarget(WrapNotNull(this), queue);

  {
    MutexAutoLock lock(mLock);
    queue->mNext = mEvents;
    mEvents = queue;
  }

  NS_ADDREF(*aResult = queue->mEventTarget);
  return NS_OK;
}

NS_IMETHODIMP
nsThread::PopEventQueue(nsIEventTarget* aInnermostTarget)
{
  if (NS_WARN_IF(PR_GetCurrentThread() != mThread)) {
    return NS_ERROR_NOT_SAME_THREAD;
  }

  if (NS_WARN_IF(!aInnermostTarget)) {
    return NS_ERROR_NULL_POINTER;
  }

  // Don't delete or release anything while holding the lock.
  nsAutoPtr<nsChainedEventQueue> queue;
  RefPtr<nsNestedEventTarget> target;

  {
    MutexAutoLock lock(mLock);

    // Make sure we're popping the innermost event target.
    if (NS_WARN_IF(mEvents->mEventTarget != aInnermostTarget)) {
      return NS_ERROR_UNEXPECTED;
    }

    MOZ_ASSERT(mEvents != &mEventsRoot);

    queue = mEvents;
    mEvents = WrapNotNull(mEvents->mNext);

    nsCOMPtr<nsIRunnable> event;
    while (queue->GetEvent(false, getter_AddRefs(event), lock)) {
      mEvents->PutEvent(event.forget(), lock);
    }

    // Don't let the event target post any more events.
    queue->mEventTarget.swap(target);
    target->mQueue = nullptr;
  }

  return NS_OK;
}

void
nsThread::SetScriptObserver(mozilla::CycleCollectedJSContext* aScriptObserver)
{
  if (!aScriptObserver) {
    mScriptObserver = nullptr;
    return;
  }

  MOZ_ASSERT(!mScriptObserver);
  mScriptObserver = aScriptObserver;
}

void
nsThread::DoMainThreadSpecificProcessing(bool aReallyWait)
{
  MOZ_ASSERT(mIsMainThread == MAIN_THREAD);

  ipc::CancelCPOWs();

  if (aReallyWait) {
    HangMonitor::Suspend();
  }

  // Fire a memory pressure notification, if one is pending.
  if (!ShuttingDown()) {
    MemoryPressureState mpPending = NS_GetPendingMemoryPressure();
    if (mpPending != MemPressure_None) {
      nsCOMPtr<nsIObserverService> os = services::GetObserverService();

      // Use no-forward to prevent the notifications from being transferred to
      // the children of this process.
      NS_NAMED_LITERAL_STRING(lowMem, "low-memory-no-forward");
      NS_NAMED_LITERAL_STRING(lowMemOngoing, "low-memory-ongoing-no-forward");

      if (os) {
        os->NotifyObservers(nullptr, "memory-pressure",
                            mpPending == MemPressure_New ? lowMem.get() :
                            lowMemOngoing.get());
      } else {
        NS_WARNING("Can't get observer service!");
      }
    }
  }

#ifdef MOZ_CRASHREPORTER
  if (!ShuttingDown()) {
    SaveMemoryReportNearOOM(ShouldSaveMemoryReport::kMaybeReport);
  }
#endif
}

//-----------------------------------------------------------------------------

NS_IMPL_ISUPPORTS(nsThread::nsNestedEventTarget, nsIEventTarget)

NS_IMETHODIMP
nsThread::nsNestedEventTarget::DispatchFromScript(nsIRunnable* aEvent, uint32_t aFlags)
{
  nsCOMPtr<nsIRunnable> event(aEvent);
  return Dispatch(event.forget(), aFlags);
}

NS_IMETHODIMP
nsThread::nsNestedEventTarget::Dispatch(already_AddRefed<nsIRunnable> aEvent, uint32_t aFlags)
{
  LOG(("THRD(%p) Dispatch [%p %x] to nested loop %p\n", mThread.get().get(),
       /*XXX aEvent*/ nullptr, aFlags, this));

  return mThread->DispatchInternal(Move(aEvent), aFlags, this);
}

NS_IMETHODIMP
nsThread::nsNestedEventTarget::DelayedDispatch(already_AddRefed<nsIRunnable>, uint32_t)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsThread::nsNestedEventTarget::IsOnCurrentThread(bool* aResult)
{
  return mThread->IsOnCurrentThread(aResult);
}
