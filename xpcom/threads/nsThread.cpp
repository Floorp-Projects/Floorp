/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsThread.h"

#if !defined(MOZILLA_XPCOMRT_API)
#include "base/message_loop.h"
#endif // !defined(MOZILLA_XPCOMRT_API)

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
#include "mozilla/CycleCollectedJSRuntime.h"
#include "mozilla/Logging.h"
#include "nsIObserverService.h"
#if !defined(MOZILLA_XPCOMRT_API)
#include "mozilla/HangMonitor.h"
#include "mozilla/IOInterposer.h"
#include "mozilla/ipc/MessageChannel.h"
#include "mozilla/ipc/BackgroundChild.h"
#endif // defined(MOZILLA_XPCOMRT_API)
#include "mozilla/Services.h"
#include "nsXPCOMPrivate.h"
#include "mozilla/ChaosMode.h"
#include "mozilla/TimeStamp.h"
#include "nsThreadSyncDispatch.h"
#include "LeakRefPtr.h"

#ifdef MOZ_CRASHREPORTER
#include "nsServiceManagerUtils.h"
#include "nsICrashReporter.h"
#endif

#ifdef MOZ_NUWA_PROCESS
#include "private/pprthred.h"
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

static PRLogModuleInfo*
GetThreadLog()
{
  static PRLogModuleInfo* sLog;
  if (!sLog) {
    sLog = PR_NewLogModule("nsThread");
  }
  return sLog;
}
#ifdef LOG
#undef LOG
#endif
#define LOG(args) MOZ_LOG(GetThreadLog(), mozilla::LogLevel::Debug, args)

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

class nsThreadStartupEvent : public nsRunnable
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
    if (mInitialized) {
      // Maybe avoid locking...
      return;
    }

    ReentrantMonitorAutoEnter mon(mMon);
    while (!mInitialized) {
      mon.Wait();
    }
  }

  // This method needs to be public to support older compilers (xlC_r on AIX).
  // It should be called directly as this class type is reference counted.
  virtual ~nsThreadStartupEvent() {}

private:
  NS_IMETHOD Run()
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

struct nsThreadShutdownContext
{
  // NB: This will be the last reference.
  nsRefPtr<nsThread> terminatingThread;
  nsThread* joiningThread;
  bool      awaitingShutdownAck;
};

// This event is responsible for notifying nsThread::Shutdown that it is time
// to call PR_JoinThread. It implements nsICancelableRunnable so that it can
// run on a DOM Worker thread (where all events must implement
// nsICancelableRunnable.)
class nsThreadShutdownAckEvent : public nsRunnable,
                                 public nsICancelableRunnable
{
public:
  explicit nsThreadShutdownAckEvent(nsThreadShutdownContext* aCtx)
    : mShutdownContext(aCtx)
  {
  }
  NS_DECL_ISUPPORTS_INHERITED
  NS_IMETHOD Run() override
  {
    mShutdownContext->terminatingThread->ShutdownComplete(mShutdownContext);
    return NS_OK;
  }
  NS_IMETHOD Cancel() override
  {
    return Run();
  }
private:
  virtual ~nsThreadShutdownAckEvent() { }

  nsThreadShutdownContext* mShutdownContext;
};

NS_IMPL_ISUPPORTS_INHERITED(nsThreadShutdownAckEvent, nsRunnable,
                            nsICancelableRunnable)

// This event is responsible for setting mShutdownContext
class nsThreadShutdownEvent : public nsRunnable
{
public:
  nsThreadShutdownEvent(nsThread* aThr, nsThreadShutdownContext* aCtx)
    : mThread(aThr)
    , mShutdownContext(aCtx)
  {
  }
  NS_IMETHOD Run()
  {
    mThread->mShutdownContext = mShutdownContext;
#if !defined(MOZILLA_XPCOMRT_API)
    MessageLoop::current()->Quit();
#endif // !defined(MOZILLA_XPCOMRT_API)
    return NS_OK;
  }
private:
  nsRefPtr<nsThread>       mThread;
  nsThreadShutdownContext* mShutdownContext;
};

//-----------------------------------------------------------------------------

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

#ifdef HAVE_SCHED_SETAFFINITY
  // Force half the threads to CPU 0 so they compete for CPU
  if (ChaosMode::randomUint32LessThan(2)) {
    cpu_set_t cpus;
    CPU_ZERO(&cpus);
    CPU_SET(0, &cpus);
    sched_setaffinity(0, sizeof(cpus), &cpus);
  }
#endif
}

/*static*/ void
nsThread::ThreadFunc(void* aArg)
{
#if !defined(MOZILLA_XPCOMRT_API)
  using mozilla::ipc::BackgroundChild;
#endif // !defined(MOZILLA_XPCOMRT_API)

  nsThread* self = static_cast<nsThread*>(aArg);  // strong reference
  self->mThread = PR_GetCurrentThread();
  SetupCurrentThreadForChaosMode();

  // Inform the ThreadManager
  nsThreadManager::get()->RegisterCurrentThread(self);

#if !defined(MOZILLA_XPCOMRT_API)
  mozilla::IOInterposer::RegisterCurrentThread();
#endif // !defined(MOZILLA_XPCOMRT_API)

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
#if defined(MOZILLA_XPCOMRT_API)
    while(!self->mShutdownContext) {
      NS_ProcessNextEvent();
    }
#else
    // Scope for MessageLoop.
    nsAutoPtr<MessageLoop> loop(
      new MessageLoop(MessageLoop::TYPE_MOZILLA_NONMAINTHREAD));

    // Now, process incoming events...
    loop->Run();

    BackgroundChild::CloseForCurrentThread();
#endif // defined(MOZILLA_XPCOMRT_API)

    // Do NS_ProcessPendingEvents but with special handling to set
    // mEventsAreDoomed atomically with the removal of the last event. The key
    // invariant here is that we will never permit PutEvent to succeed if the
    // event would be left in the queue after our final call to
    // NS_ProcessPendingEvents. We also have to keep processing events as long
    // as we have outstanding mRequestedShutdownContexts.
    while (true) {
      // Check and see if we're waiting on any threads.
      while (self->mRequestedShutdownContexts.Length()) {
        // We can't stop accepting events just yet.  Block and check again.
        NS_ProcessNextEvent(self, true);
      }

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

#if !defined(MOZILLA_XPCOMRT_API)
  mozilla::IOInterposer::UnregisterCurrentThread();
#endif // !defined(MOZILLA_XPCOMRT_API)

  // Inform the threadmanager that this thread is going away
  nsThreadManager::get()->UnregisterCurrentThread(self);

  // Dispatch shutdown ACK
  MOZ_ASSERT(self->mShutdownContext->terminatingThread == self);
  event = do_QueryObject(new nsThreadShutdownAckEvent(self->mShutdownContext));
  self->mShutdownContext->joiningThread->Dispatch(event, NS_DISPATCH_NORMAL);

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
static bool SaveMemoryReportNearOOM()
{
  bool needMemoryReport = false;

#ifdef XP_WIN // XXX implement on other platforms as needed
  const size_t LOWMEM_THRESHOLD_VIRTUAL = 200 * 1024 * 1024;
  MEMORYSTATUSEX statex;
  statex.dwLength = sizeof(statex);
  if (GlobalMemoryStatusEx(&statex)) {
    if (statex.ullAvailVirtual < LOWMEM_THRESHOLD_VIRTUAL) {
      needMemoryReport = true;
    }
  }
#endif

  if (needMemoryReport) {
    nsCOMPtr<nsICrashReporter> cr =
      do_GetService("@mozilla.org/toolkit/crash-reporter;1");
    cr->SaveMemoryReport();
  }

  return needMemoryReport;
}
#endif

#ifdef MOZ_CANARY
int sCanaryOutputFD = -1;
#endif

nsThread::nsThread(MainThreadFlag aMainThread, uint32_t aStackSize)
  : mLock("nsThread.mLock")
  , mScriptObserver(nullptr)
  , mEvents(&mEventsRoot)
  , mEventsRoot(mLock)
  , mPriority(PRIORITY_NORMAL)
  , mThread(nullptr)
  , mNestedEventLoopDepth(0)
  , mStackSize(aStackSize)
  , mShutdownContext(nullptr)
  , mShutdownRequired(false)
  , mEventsAreDoomed(false)
  , mIsMainThread(aMainThread)
{
}

nsThread::~nsThread()
{
}

nsresult
nsThread::Init()
{
  // spawn thread and wait until it is fully setup
  nsRefPtr<nsThreadStartupEvent> startup = new nsThreadStartupEvent();

  NS_ADDREF_THIS();

  mShutdownRequired = true;

  // ThreadFunc is responsible for setting mThread
  PRThread* thr = PR_CreateThread(PR_USER_THREAD, ThreadFunc, this,
                                  PR_PRIORITY_NORMAL, PR_GLOBAL_THREAD,
                                  PR_JOINABLE_THREAD, mStackSize);
  if (!thr) {
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

  nsThreadManager::get()->RegisterCurrentThread(this);
  return NS_OK;
}

nsresult
nsThread::PutEvent(nsIRunnable* aEvent, nsNestedEventTarget* aTarget)
{
  nsCOMPtr<nsIRunnable> event(aEvent);
  return PutEvent(event.forget(), aTarget);
}

nsresult
nsThread::PutEvent(already_AddRefed<nsIRunnable>&& aEvent, nsNestedEventTarget* aTarget)
{
  // We want to leak the reference when we fail to dispatch it, so that
  // we won't release the event in a wrong thread.
  LeakRefPtr<nsIRunnable> event(Move(aEvent));
  nsCOMPtr<nsIThreadObserver> obs;

#ifdef MOZ_NUWA_PROCESS
  // On debug build or when tests are enabled, assert that we are not about to
  // create a deadlock in the Nuwa process.
  NuwaAssertNotFrozen(PR_GetThreadID(mThread), PR_GetThreadName(mThread));
#endif

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
nsThread::DispatchInternal(already_AddRefed<nsIRunnable>&& aEvent, uint32_t aFlags,
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
    nsThread* thread = nsThreadManager::get()->GetCurrentThread();
    if (NS_WARN_IF(!thread)) {
      return NS_ERROR_NOT_AVAILABLE;
    }

    // XXX we should be able to do something better here... we should
    //     be able to monitor the slot occupied by this event and use
    //     that to tell us when the event has been processed.

    nsRefPtr<nsThreadSyncDispatch> wrapper =
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
    // NOTE that, unlike the behavior above, the event is not leaked by
    // this place, while it is possible that the result is an error.
    return wrapper->Result();
  }

  NS_ASSERTION(aFlags == NS_DISPATCH_NORMAL, "unexpected dispatch flags");
  return PutEvent(event.take(), aTarget);
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
nsThread::Dispatch(already_AddRefed<nsIRunnable>&& aEvent, uint32_t aFlags)
{
  LOG(("THRD(%p) Dispatch [%p %x]\n", this, /* XXX aEvent */nullptr, aFlags));

  return DispatchInternal(Move(aEvent), aFlags, nullptr);
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

  nsThread* currentThread = nsThreadManager::get()->GetCurrentThread();
  MOZ_ASSERT(currentThread);

  nsAutoPtr<nsThreadShutdownContext>& context =
    *currentThread->mRequestedShutdownContexts.AppendElement();
  context = new nsThreadShutdownContext();

  context->terminatingThread = this;
  context->joiningThread = currentThread;
  context->awaitingShutdownAck = aSync;

  // Set mShutdownContext and wake up the thread in case it is waiting for
  // events to process.
  nsCOMPtr<nsIRunnable> event = new nsThreadShutdownEvent(this, context);
  // XXXroc What if posting the event fails due to OOM?
  PutEvent(event.forget(), nullptr);

  // We could still end up with other events being added after the shutdown
  // task, but that's okay because we process pending events in ThreadFunc
  // after setting mShutdownContext just before exiting.
  return context;
}

void
nsThread::ShutdownComplete(nsThreadShutdownContext* aContext)
{
  MOZ_ASSERT(mThread);
  MOZ_ASSERT(aContext->terminatingThread == this);

  if (aContext->awaitingShutdownAck) {
    // We're in a synchronous shutdown, so tell whatever is up the stack that
    // we're done and unwind the stack so it can call us again.
    aContext->awaitingShutdownAck = false;
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
    aContext->joiningThread->mRequestedShutdownContexts.RemoveElement(aContext));
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

  nsThreadShutdownContext* context = ShutdownInternal(/* aSync = */ true);
  NS_ENSURE_TRUE(context, NS_ERROR_UNEXPECTED);

  // Process events on the current thread until we receive a shutdown ACK.
  // Allows waiting; ensure no locks are held that would deadlock us!
  while (context->awaitingShutdownAck) {
    NS_ProcessNextEvent(context->joiningThread, true);
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
      nsAutoTObserverArray<nsCOMPtr<nsIThreadObserver>, 2>::ForwardIterator    \
        iter_(mEventObservers);                                                \
      nsCOMPtr<nsIThreadObserver> obs_;                                        \
      while (iter_.HasMore()) {                                                \
        obs_ = iter_.GetNext();                                                \
        obs_ -> func_ params_ ;                                                \
      }                                                                        \
    }                                                                          \
  PR_END_MACRO

NS_IMETHODIMP
nsThread::ProcessNextEvent(bool aMayWait, bool* aResult)
{
  LOG(("THRD(%p) ProcessNextEvent [%u %u]\n", this, aMayWait,
       mNestedEventLoopDepth));

#if !defined(MOZILLA_XPCOMRT_API)
  // If we're on the main thread, we shouldn't be dispatching CPOWs.
  if (mIsMainThread == MAIN_THREAD) {
    ipc::CancelCPOWs();
  }
#endif // !defined(MOZILLA_XPCOMRT_API)

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

#if !defined(MOZILLA_XPCOMRT_API)
  if (MAIN_THREAD == mIsMainThread && reallyWait) {
    HangMonitor::Suspend();
  }
#endif // !defined(MOZILLA_XPCOMRT_API)

  // Fire a memory pressure notification, if we're the main thread and one is
  // pending.
  if (MAIN_THREAD == mIsMainThread && !ShuttingDown()) {
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
  if (MAIN_THREAD == mIsMainThread && !ShuttingDown()) {
    // Keep an eye on memory usage (cheap, ~7ms) somewhat frequently,
    // but save memory reports (expensive, ~75ms) less frequently.
    const size_t LOW_MEMORY_CHECK_SECONDS = 30;
    const size_t LOW_MEMORY_SAVE_SECONDS = 3 * 60;

    static TimeStamp nextCheck = TimeStamp::NowLoRes()
      + TimeDuration::FromSeconds(LOW_MEMORY_CHECK_SECONDS);

    TimeStamp now = TimeStamp::NowLoRes();
    if (now >= nextCheck) {
      if (SaveMemoryReportNearOOM()) {
        nextCheck = now + TimeDuration::FromSeconds(LOW_MEMORY_SAVE_SECONDS);
      } else {
        nextCheck = now + TimeDuration::FromSeconds(LOW_MEMORY_CHECK_SECONDS);
      }
    }
  }
#endif

  ++mNestedEventLoopDepth;

  bool callScriptObserver = !!mScriptObserver;
  if (callScriptObserver) {
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

    // If we are shutting down, then do not wait for new events.
    nsCOMPtr<nsIRunnable> event;
    {
      MutexAutoLock lock(mLock);
      mEvents->GetEvent(reallyWait, getter_AddRefs(event), lock);
    }

    *aResult = (event.get() != nullptr);

    if (event) {
      LOG(("THRD(%p) running [%p]\n", this, event.get()));
#if !defined(MOZILLA_XPCOMRT_API)
      if (MAIN_THREAD == mIsMainThread) {
        HangMonitor::NotifyActivity();
      }
#endif // !defined(MOZILLA_XPCOMRT_API)
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

  if (callScriptObserver && mScriptObserver) {
    mScriptObserver->AfterProcessTask(mNestedEventLoopDepth);
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

  NS_WARN_IF_FALSE(!mEventObservers.Contains(aObserver),
                   "Adding an observer twice!");

  if (!mEventObservers.AppendElement(aObserver)) {
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

  nsChainedEventQueue* queue = new nsChainedEventQueue(mLock);
  queue->mEventTarget = new nsNestedEventTarget(this, queue);

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
  nsRefPtr<nsNestedEventTarget> target;

  {
    MutexAutoLock lock(mLock);

    // Make sure we're popping the innermost event target.
    if (NS_WARN_IF(mEvents->mEventTarget != aInnermostTarget)) {
      return NS_ERROR_UNEXPECTED;
    }

    MOZ_ASSERT(mEvents != &mEventsRoot);

    queue = mEvents;
    mEvents = mEvents->mNext;

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
nsThread::SetScriptObserver(mozilla::CycleCollectedJSRuntime* aScriptObserver)
{
  if (!aScriptObserver) {
    mScriptObserver = nullptr;
    return;
  }

  MOZ_ASSERT(!mScriptObserver);
  mScriptObserver = aScriptObserver;
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
nsThread::nsNestedEventTarget::Dispatch(already_AddRefed<nsIRunnable>&& aEvent, uint32_t aFlags)
{
  LOG(("THRD(%p) Dispatch [%p %x] to nested loop %p\n", mThread.get(), /*XXX aEvent*/ nullptr,
       aFlags, this));

  return mThread->DispatchInternal(Move(aEvent), aFlags, this);
}

NS_IMETHODIMP
nsThread::nsNestedEventTarget::IsOnCurrentThread(bool* aResult)
{
  return mThread->IsOnCurrentThread(aResult);
}
