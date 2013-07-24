/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ReentrantMonitor.h"
#include "nsMemoryPressure.h"
#include "nsThread.h"
#include "nsThreadManager.h"
#include "nsIClassInfoImpl.h"
#include "nsIProgrammingLanguage.h"
#include "nsAutoPtr.h"
#include "nsCOMPtr.h"
#include "pratom.h"
#include "prlog.h"
#include "nsIObserverService.h"
#include "mozilla/HangMonitor.h"
#include "mozilla/Services.h"
#include "nsXPCOMPrivate.h"

#define HAVE_UALARM _BSD_SOURCE || (_XOPEN_SOURCE >= 500 ||                 \
                      _XOPEN_SOURCE && _XOPEN_SOURCE_EXTENDED) &&           \
                      !(_POSIX_C_SOURCE >= 200809L || _XOPEN_SOURCE >= 700)

#if defined(XP_UNIX) && !defined(ANDROID) && !defined(DEBUG) && HAVE_UALARM \
  && defined(_GNU_SOURCE)
# define MOZ_CANARY
# include <unistd.h>
# include <execinfo.h>
# include <signal.h>
# include <fcntl.h>
# include "nsXULAppAPI.h"
#endif

#if defined(NS_FUNCTION_TIMER) && defined(_MSC_VER)
#include "nsTimerImpl.h"
#include "nsStackWalk.h"
#endif
#ifdef NS_FUNCTION_TIMER
#include "nsCRT.h"
#endif

using namespace mozilla;

#ifdef PR_LOGGING
static PRLogModuleInfo *
GetThreadLog()
{
  static PRLogModuleInfo *sLog;
  if (!sLog)
    sLog = PR_NewLogModule("nsThread");
  return sLog;
}
#endif
#define LOG(args) PR_LOG(GetThreadLog(), PR_LOG_DEBUG, args)

NS_DECL_CI_INTERFACE_GETTER(nsThread)

nsIThreadObserver* nsThread::sMainThreadObserver = nullptr;

//-----------------------------------------------------------------------------
// Because we do not have our own nsIFactory, we have to implement nsIClassInfo
// somewhat manually.

class nsThreadClassInfo : public nsIClassInfo {
public:
  NS_DECL_ISUPPORTS_INHERITED  // no mRefCnt
  NS_DECL_NSICLASSINFO

  nsThreadClassInfo() {}
};

NS_IMETHODIMP_(nsrefcnt) nsThreadClassInfo::AddRef() { return 2; }
NS_IMETHODIMP_(nsrefcnt) nsThreadClassInfo::Release() { return 1; }
NS_IMPL_QUERY_INTERFACE1(nsThreadClassInfo, nsIClassInfo)

NS_IMETHODIMP
nsThreadClassInfo::GetInterfaces(uint32_t *count, nsIID ***array)
{
  return NS_CI_INTERFACE_GETTER_NAME(nsThread)(count, array);
}

NS_IMETHODIMP
nsThreadClassInfo::GetHelperForLanguage(uint32_t lang, nsISupports **result)
{
  *result = nullptr;
  return NS_OK;
}

NS_IMETHODIMP
nsThreadClassInfo::GetContractID(char **result)
{
  *result = nullptr;
  return NS_OK;
}

NS_IMETHODIMP
nsThreadClassInfo::GetClassDescription(char **result)
{
  *result = nullptr;
  return NS_OK;
}

NS_IMETHODIMP
nsThreadClassInfo::GetClassID(nsCID **result)
{
  *result = nullptr;
  return NS_OK;
}

NS_IMETHODIMP
nsThreadClassInfo::GetImplementationLanguage(uint32_t *result)
{
  *result = nsIProgrammingLanguage::CPLUSPLUS;
  return NS_OK;
}

NS_IMETHODIMP
nsThreadClassInfo::GetFlags(uint32_t *result)
{
  *result = THREADSAFE;
  return NS_OK;
}

NS_IMETHODIMP
nsThreadClassInfo::GetClassIDNoAlloc(nsCID *result)
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
NS_IMPL_CI_INTERFACE_GETTER4(nsThread, nsIThread, nsIThreadInternal,
                             nsIEventTarget, nsISupportsPriority)

//-----------------------------------------------------------------------------

class nsThreadStartupEvent : public nsRunnable {
public:
  // Create a new thread startup object.
  static nsThreadStartupEvent *Create() {
    return new nsThreadStartupEvent();
  }

  // This method does not return until the thread startup object is in the
  // completion state.
  void Wait() {
    if (mInitialized)  // Maybe avoid locking...
      return;
    ReentrantMonitorAutoEnter mon(mMon);
    while (!mInitialized)
      mon.Wait();
  }

  // This method needs to be public to support older compilers (xlC_r on AIX).
  // It should be called directly as this class type is reference counted.
  virtual ~nsThreadStartupEvent() {
  }

private:
  NS_IMETHOD Run() {
    ReentrantMonitorAutoEnter mon(mMon);
    mInitialized = true;
    mon.Notify();
    return NS_OK;
  }

  nsThreadStartupEvent()
    : mMon("nsThreadStartupEvent.mMon")
    , mInitialized(false) {
  }

  ReentrantMonitor mMon;
  bool       mInitialized;
};

//-----------------------------------------------------------------------------

struct nsThreadShutdownContext {
  nsThread *joiningThread;
  bool      shutdownAck;
};

// This event is responsible for notifying nsThread::Shutdown that it is time
// to call PR_JoinThread.
class nsThreadShutdownAckEvent : public nsRunnable {
public:
  nsThreadShutdownAckEvent(nsThreadShutdownContext *ctx)
    : mShutdownContext(ctx) {
  }
  NS_IMETHOD Run() {
    mShutdownContext->shutdownAck = true;
    return NS_OK;
  }
private:
  nsThreadShutdownContext *mShutdownContext;
};

// This event is responsible for setting mShutdownContext
class nsThreadShutdownEvent : public nsRunnable {
public:
  nsThreadShutdownEvent(nsThread *thr, nsThreadShutdownContext *ctx)
    : mThread(thr), mShutdownContext(ctx) {
  } 
  NS_IMETHOD Run() {
    mThread->mShutdownContext = mShutdownContext;
    return NS_OK;
  }
private:
  nsRefPtr<nsThread>       mThread;
  nsThreadShutdownContext *mShutdownContext;
};

//-----------------------------------------------------------------------------

/*static*/ void
nsThread::ThreadFunc(void *arg)
{
  nsThread *self = static_cast<nsThread *>(arg);  // strong reference
  self->mThread = PR_GetCurrentThread();

  // Inform the ThreadManager
  nsThreadManager::get()->RegisterCurrentThread(self);

  // Wait for and process startup event
  nsCOMPtr<nsIRunnable> event;
  if (!self->GetEvent(true, getter_AddRefs(event))) {
    NS_WARNING("failed waiting for thread startup event");
    return;
  }
  event->Run();  // unblocks nsThread::Init
  event = nullptr;

  // Now, process incoming events...
  while (!self->ShuttingDown())
    NS_ProcessNextEvent(self);

  // Do NS_ProcessPendingEvents but with special handling to set
  // mEventsAreDoomed atomically with the removal of the last event. The key
  // invariant here is that we will never permit PutEvent to succeed if the
  // event would be left in the queue after our final call to
  // NS_ProcessPendingEvents.
  while (true) {
    {
      MutexAutoLock lock(self->mLock);
      if (!self->mEvents.HasPendingEvent()) {
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

  // Inform the threadmanager that this thread is going away
  nsThreadManager::get()->UnregisterCurrentThread(self);

  // Dispatch shutdown ACK
  event = new nsThreadShutdownAckEvent(self->mShutdownContext);
  self->mShutdownContext->joiningThread->Dispatch(event, NS_DISPATCH_NORMAL);

  // Release any observer of the thread here.
  self->SetObserver(nullptr);

  NS_RELEASE(self);
}

//-----------------------------------------------------------------------------

nsThread::nsThread(MainThreadFlag aMainThread, uint32_t aStackSize)
  : mLock("nsThread.mLock")
  , mPriority(PRIORITY_NORMAL)
  , mThread(nullptr)
  , mRunningEvent(0)
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
  nsRefPtr<nsThreadStartupEvent> startup = nsThreadStartupEvent::Create();
  NS_ENSURE_TRUE(startup, NS_ERROR_OUT_OF_MEMORY);
 
  NS_ADDREF_THIS();
 
  mShutdownRequired = true;

  // ThreadFunc is responsible for setting mThread
  PRThread *thr = PR_CreateThread(PR_USER_THREAD, ThreadFunc, this,
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
    mEvents.PutEvent(startup);
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

  nsThreadManager::get()->RegisterCurrentThread(this);
  return NS_OK;
}

nsresult
nsThread::PutEvent(nsIRunnable *event)
{
  {
    MutexAutoLock lock(mLock);
    if (mEventsAreDoomed) {
      NS_WARNING("An event was posted to a thread that will never run it (rejected)");
      return NS_ERROR_UNEXPECTED;
    }
    if (!mEvents.PutEvent(event))
      return NS_ERROR_OUT_OF_MEMORY;
  }

  nsCOMPtr<nsIThreadObserver> obs = GetObserver();
  if (obs)
    obs->OnDispatchedEvent(this);

  return NS_OK;
}

//-----------------------------------------------------------------------------
// nsIEventTarget

NS_IMETHODIMP
nsThread::Dispatch(nsIRunnable *event, uint32_t flags)
{
  LOG(("THRD(%p) Dispatch [%p %x]\n", this, event, flags));

  NS_ENSURE_ARG_POINTER(event);

  if (gXPCOMThreadsShutDown && MAIN_THREAD != mIsMainThread) {
    return NS_ERROR_ILLEGAL_DURING_SHUTDOWN;
  }

  if (flags & DISPATCH_SYNC) {
    nsThread *thread = nsThreadManager::get()->GetCurrentThread();
    NS_ENSURE_STATE(thread);

    // XXX we should be able to do something better here... we should
    //     be able to monitor the slot occupied by this event and use
    //     that to tell us when the event has been processed.
 
    nsRefPtr<nsThreadSyncDispatch> wrapper =
        new nsThreadSyncDispatch(thread, event);
    if (!wrapper)
      return NS_ERROR_OUT_OF_MEMORY;
    nsresult rv = PutEvent(wrapper);
    // Don't wait for the event to finish if we didn't dispatch it...
    if (NS_FAILED(rv))
      return rv;

    while (wrapper->IsPending())
      NS_ProcessNextEvent(thread);
    return wrapper->Result();
  }

  NS_ASSERTION(flags == NS_DISPATCH_NORMAL, "unexpected dispatch flags");
  return PutEvent(event);
}

NS_IMETHODIMP
nsThread::IsOnCurrentThread(bool *result)
{
  *result = (PR_GetCurrentThread() == mThread);
  return NS_OK;
}

//-----------------------------------------------------------------------------
// nsIThread

NS_IMETHODIMP
nsThread::GetPRThread(PRThread **result)
{
  *result = mThread;
  return NS_OK;
}

NS_IMETHODIMP
nsThread::Shutdown()
{
  LOG(("THRD(%p) shutdown\n", this));

  // XXX If we make this warn, then we hit that warning at xpcom shutdown while
  //     shutting down a thread in a thread pool.  That happens b/c the thread
  //     in the thread pool is already shutdown by the thread manager.
  if (!mThread)
    return NS_OK;

  NS_ENSURE_STATE(mThread != PR_GetCurrentThread());

  // Prevent multiple calls to this method
  {
    MutexAutoLock lock(mLock);
    if (!mShutdownRequired)
      return NS_ERROR_UNEXPECTED;
    mShutdownRequired = false;
  }

  nsThreadShutdownContext context;
  context.joiningThread = nsThreadManager::get()->GetCurrentThread();
  context.shutdownAck = false;

  // Set mShutdownContext and wake up the thread in case it is waiting for
  // events to process.
  nsCOMPtr<nsIRunnable> event = new nsThreadShutdownEvent(this, &context);
  if (!event)
    return NS_ERROR_OUT_OF_MEMORY;
  // XXXroc What if posting the event fails due to OOM?
  PutEvent(event);

  // We could still end up with other events being added after the shutdown
  // task, but that's okay because we process pending events in ThreadFunc
  // after setting mShutdownContext just before exiting.
  
  // Process events on the current thread until we receive a shutdown ACK.
  while (!context.shutdownAck)
    NS_ProcessNextEvent(context.joiningThread);

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

  return NS_OK;
}

NS_IMETHODIMP
nsThread::HasPendingEvents(bool *result)
{
  NS_ENSURE_STATE(PR_GetCurrentThread() == mThread);

  *result = mEvents.GetEvent(false, nullptr);
  return NS_OK;
}

#ifdef MOZ_CANARY
void canary_alarm_handler (int signum);

class Canary {
//XXX ToDo: support nested loops
public:
  Canary() {
    if (sOutputFD != 0 && EventLatencyIsImportant()) {
      if (sOutputFD == -1) {
        const int flags = O_WRONLY | O_APPEND | O_CREAT | O_NONBLOCK;
        const mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
        char* env_var_flag = getenv("MOZ_KILL_CANARIES");
        sOutputFD = env_var_flag ? (env_var_flag[0] ?
                                    open(env_var_flag, flags, mode) :
                                    STDERR_FILENO) : 0;
        if (sOutputFD == 0)
          return;
      }
      signal(SIGALRM, canary_alarm_handler);
      ualarm(15000, 0);      
    }
  }

  ~Canary() {
    if (sOutputFD != 0 && EventLatencyIsImportant())
      ualarm(0, 0);
  }

  static bool EventLatencyIsImportant() {
    return NS_IsMainThread() && XRE_GetProcessType() == GeckoProcessType_Default;
  }

  static int sOutputFD;
};

int Canary::sOutputFD = -1;

void canary_alarm_handler (int signum)
{
  void *array[30];
  const char msg[29] = "event took too long to run:\n";
  // use write to be safe in the signal handler
  write(Canary::sOutputFD, msg, sizeof(msg)); 
  backtrace_symbols_fd(array, backtrace(array, 30), Canary::sOutputFD);
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
nsThread::ProcessNextEvent(bool mayWait, bool *result)
{
  LOG(("THRD(%p) ProcessNextEvent [%u %u]\n", this, mayWait, mRunningEvent));

  NS_ENSURE_STATE(PR_GetCurrentThread() == mThread);

  if (MAIN_THREAD == mIsMainThread && mayWait && !ShuttingDown())
    HangMonitor::Suspend();

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

  bool notifyMainThreadObserver =
    (MAIN_THREAD == mIsMainThread) && sMainThreadObserver;
  if (notifyMainThreadObserver) 
   sMainThreadObserver->OnProcessNextEvent(this, mayWait && !ShuttingDown(),
                                           mRunningEvent);

  nsCOMPtr<nsIThreadObserver> obs = mObserver;
  if (obs)
    obs->OnProcessNextEvent(this, mayWait && !ShuttingDown(), mRunningEvent);

  NOTIFY_EVENT_OBSERVERS(OnProcessNextEvent,
                         (this, mayWait && !ShuttingDown(), mRunningEvent));

  ++mRunningEvent;

#ifdef MOZ_CANARY
  Canary canary;
#endif
  nsresult rv = NS_OK;

  {
    // Scope for |event| to make sure that its destructor fires while
    // mRunningEvent has been incremented, since that destructor can
    // also do work.

    // If we are shutting down, then do not wait for new events.
    nsCOMPtr<nsIRunnable> event;
    mEvents.GetEvent(mayWait && !ShuttingDown(), getter_AddRefs(event));

    *result = (event.get() != nullptr);

    if (event) {
      LOG(("THRD(%p) running [%p]\n", this, event.get()));
      if (MAIN_THREAD == mIsMainThread)
        HangMonitor::NotifyActivity();
      event->Run();
    } else if (mayWait) {
      MOZ_ASSERT(ShuttingDown(),
                 "This should only happen when shutting down");
      rv = NS_ERROR_UNEXPECTED;
    }
  }

  --mRunningEvent;

  NOTIFY_EVENT_OBSERVERS(AfterProcessNextEvent, (this, mRunningEvent));

  if (obs)
    obs->AfterProcessNextEvent(this, mRunningEvent);

  if (notifyMainThreadObserver && sMainThreadObserver)
    sMainThreadObserver->AfterProcessNextEvent(this, mRunningEvent);

  return rv;
}

//-----------------------------------------------------------------------------
// nsISupportsPriority

NS_IMETHODIMP
nsThread::GetPriority(int32_t *priority)
{
  *priority = mPriority;
  return NS_OK;
}

NS_IMETHODIMP
nsThread::SetPriority(int32_t priority)
{
  NS_ENSURE_STATE(mThread);

  // NSPR defines the following four thread priorities:
  //   PR_PRIORITY_LOW
  //   PR_PRIORITY_NORMAL
  //   PR_PRIORITY_HIGH
  //   PR_PRIORITY_URGENT
  // We map the priority values defined on nsISupportsPriority to these values.

  mPriority = priority;

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
  PR_SetThreadPriority(mThread, pri);

  return NS_OK;
}

NS_IMETHODIMP
nsThread::AdjustPriority(int32_t delta)
{
  return SetPriority(mPriority + delta);
}

//-----------------------------------------------------------------------------
// nsIThreadInternal

NS_IMETHODIMP
nsThread::GetObserver(nsIThreadObserver **obs)
{
  MutexAutoLock lock(mLock);
  NS_IF_ADDREF(*obs = mObserver);
  return NS_OK;
}

NS_IMETHODIMP
nsThread::SetObserver(nsIThreadObserver *obs)
{
  NS_ENSURE_STATE(PR_GetCurrentThread() == mThread);

  MutexAutoLock lock(mLock);
  mObserver = obs;
  return NS_OK;
}

NS_IMETHODIMP
nsThread::GetRecursionDepth(uint32_t *depth)
{
  NS_ENSURE_ARG_POINTER(depth);
  NS_ENSURE_STATE(PR_GetCurrentThread() == mThread);

  *depth = mRunningEvent;
  return NS_OK;
}

NS_IMETHODIMP
nsThread::AddObserver(nsIThreadObserver *observer)
{
  NS_ENSURE_ARG_POINTER(observer);
  NS_ENSURE_STATE(PR_GetCurrentThread() == mThread);

  NS_WARN_IF_FALSE(!mEventObservers.Contains(observer),
                   "Adding an observer twice!");

  if (!mEventObservers.AppendElement(observer)) {
    NS_WARNING("Out of memory!");
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsThread::RemoveObserver(nsIThreadObserver *observer)
{
  NS_ENSURE_STATE(PR_GetCurrentThread() == mThread);

  if (observer && !mEventObservers.RemoveElement(observer)) {
    NS_WARNING("Removing an observer that was never added!");
  }

  return NS_OK;
}

nsresult
nsThread::SetMainThreadObserver(nsIThreadObserver* aObserver)
{
  if (aObserver && nsThread::sMainThreadObserver) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  if (!NS_IsMainThread()) {
    return NS_ERROR_UNEXPECTED;
  }

  nsThread::sMainThreadObserver = aObserver;
  return NS_OK;
}

//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsThreadSyncDispatch::Run()
{
  if (mSyncTask) {
    mResult = mSyncTask->Run();
    mSyncTask = nullptr;
    // unblock the origin thread
    mOrigin->Dispatch(this, NS_DISPATCH_NORMAL);
  }
  return NS_OK;
}
