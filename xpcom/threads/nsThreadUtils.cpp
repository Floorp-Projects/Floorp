/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsThreadUtils.h"
#include "mozilla/Attributes.h"
#include "mozilla/Likely.h"
#include "mozilla/TimeStamp.h"
#include "LeakRefPtr.h"

#ifdef MOZILLA_INTERNAL_API
# include "nsThreadManager.h"
#else
# include "nsXPCOMCIDInternal.h"
# include "nsIThreadManager.h"
# include "nsServiceManagerUtils.h"
#endif

#ifdef XP_WIN
#include <windows.h>
#elif defined(XP_MACOSX)
#include <sys/resource.h>
#endif

#ifdef MOZ_CRASHREPORTER
#include "nsExceptionHandler.h"
#endif

using namespace mozilla;

#ifndef XPCOM_GLUE_AVOID_NSPR

NS_IMPL_ISUPPORTS(IdlePeriod, nsIIdlePeriod)

NS_IMETHODIMP
IdlePeriod::GetIdlePeriodHint(TimeStamp* aIdleDeadline)
{
  *aIdleDeadline = TimeStamp();
  return NS_OK;
}

NS_IMPL_ISUPPORTS(Runnable, nsIRunnable, nsINamed)

NS_IMETHODIMP
Runnable::Run()
{
  // Do nothing
  return NS_OK;
}

NS_IMETHODIMP
Runnable::GetName(nsACString& aName)
{
#ifdef RELEASE_OR_BETA
  aName.Truncate();
#else
  if (mName) {
    aName.AssignASCII(mName);
  } else {
    aName.Truncate();
  }
#endif
  return NS_OK;
}

nsresult
Runnable::SetName(const char* aName)
{
#ifndef RELEASE_OR_BETA
  mName = aName;
#endif
  return NS_OK;
}

NS_IMPL_ISUPPORTS_INHERITED(CancelableRunnable, Runnable,
                            nsICancelableRunnable)

nsresult
CancelableRunnable::Cancel()
{
  // Do nothing
  return NS_OK;
}

NS_IMPL_ISUPPORTS_INHERITED(IdleRunnable, CancelableRunnable,
                            nsIIdleRunnable)

namespace mozilla {
namespace detail {
already_AddRefed<nsITimer> CreateTimer()
{
  nsCOMPtr<nsITimer> timer = do_CreateInstance(NS_TIMER_CONTRACTID);
  return timer.forget();
}
} // namespace detail
} // namespace mozilla

#endif  // XPCOM_GLUE_AVOID_NSPR

//-----------------------------------------------------------------------------

nsresult
NS_NewNamedThread(const nsACString& aName,
                  nsIThread** aResult,
                  nsIRunnable* aEvent,
                  uint32_t aStackSize)
{
  nsCOMPtr<nsIThread> thread;
#ifdef MOZILLA_INTERNAL_API
  nsresult rv =
    nsThreadManager::get().nsThreadManager::NewNamedThread(aName, aStackSize,
                                                           getter_AddRefs(thread));
#else
  nsresult rv;
  nsCOMPtr<nsIThreadManager> mgr =
    do_GetService(NS_THREADMANAGER_CONTRACTID, &rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = mgr->NewNamedThread(aName, aStackSize, getter_AddRefs(thread));
#endif
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (aEvent) {
    rv = thread->Dispatch(aEvent, NS_DISPATCH_NORMAL);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  *aResult = nullptr;
  thread.swap(*aResult);
  return NS_OK;
}

nsresult
NS_NewThread(nsIThread** aResult, nsIRunnable* aEvent, uint32_t aStackSize)
{
  return NS_NewNamedThread(NS_LITERAL_CSTRING(""), aResult, aEvent, aStackSize);
}

nsresult
NS_GetCurrentThread(nsIThread** aResult)
{
#ifdef MOZILLA_INTERNAL_API
  return nsThreadManager::get().nsThreadManager::GetCurrentThread(aResult);
#else
  nsresult rv;
  nsCOMPtr<nsIThreadManager> mgr =
    do_GetService(NS_THREADMANAGER_CONTRACTID, &rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  return mgr->GetCurrentThread(aResult);
#endif
}

nsresult
NS_GetMainThread(nsIThread** aResult)
{
#ifdef MOZILLA_INTERNAL_API
  return nsThreadManager::get().nsThreadManager::GetMainThread(aResult);
#else
  nsresult rv;
  nsCOMPtr<nsIThreadManager> mgr =
    do_GetService(NS_THREADMANAGER_CONTRACTID, &rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  return mgr->GetMainThread(aResult);
#endif
}

#ifndef MOZILLA_INTERNAL_API
bool
NS_IsMainThread()
{
  bool result = false;
  nsCOMPtr<nsIThreadManager> mgr =
    do_GetService(NS_THREADMANAGER_CONTRACTID);
  if (mgr) {
    mgr->GetIsMainThread(&result);
  }
  return bool(result);
}
#endif

nsresult
NS_DispatchToCurrentThread(already_AddRefed<nsIRunnable>&& aEvent)
{
  nsresult rv;
  nsCOMPtr<nsIRunnable> event(aEvent);
#ifdef MOZILLA_INTERNAL_API
  nsIEventTarget* thread = GetCurrentThreadEventTarget();
  if (!thread) {
    return NS_ERROR_UNEXPECTED;
  }
#else
  nsCOMPtr<nsIThread> thread;
  rv = NS_GetCurrentThread(getter_AddRefs(thread));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
#endif
  // To keep us from leaking the runnable if dispatch method fails,
  // we grab the reference on failures and release it.
  nsIRunnable* temp = event.get();
  rv = thread->Dispatch(event.forget(), NS_DISPATCH_NORMAL);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    // Dispatch() leaked the reference to the event, but due to caller's
    // assumptions, we shouldn't leak here. And given we are on the same
    // thread as the dispatch target, it's mostly safe to do it here.
    NS_RELEASE(temp);
  }
  return rv;
}

// It is common to call NS_DispatchToCurrentThread with a newly
// allocated runnable with a refcount of zero. To keep us from leaking
// the runnable if the dispatch method fails, we take a death grip.
nsresult
NS_DispatchToCurrentThread(nsIRunnable* aEvent)
{
  nsCOMPtr<nsIRunnable> event(aEvent);
  return NS_DispatchToCurrentThread(event.forget());
}

nsresult
NS_DispatchToMainThread(already_AddRefed<nsIRunnable>&& aEvent, uint32_t aDispatchFlags)
{
  LeakRefPtr<nsIRunnable> event(Move(aEvent));
  nsCOMPtr<nsIThread> thread;
  nsresult rv = NS_GetMainThread(getter_AddRefs(thread));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    NS_ASSERTION(false, "Failed NS_DispatchToMainThread() in shutdown; leaking");
    // NOTE: if you stop leaking here, adjust Promise::MaybeReportRejected(),
    // which assumes a leak here, or split into leaks and no-leaks versions
    return rv;
  }
  return thread->Dispatch(event.take(), aDispatchFlags);
}

// In the case of failure with a newly allocated runnable with a
// refcount of zero, we intentionally leak the runnable, because it is
// likely that the runnable is being dispatched to the main thread
// because it owns main thread only objects, so it is not safe to
// release them here.
nsresult
NS_DispatchToMainThread(nsIRunnable* aEvent, uint32_t aDispatchFlags)
{
  nsCOMPtr<nsIRunnable> event(aEvent);
  return NS_DispatchToMainThread(event.forget(), aDispatchFlags);
}

nsresult
NS_DelayedDispatchToCurrentThread(already_AddRefed<nsIRunnable>&& aEvent, uint32_t aDelayMs)
{
  nsCOMPtr<nsIRunnable> event(aEvent);
#ifdef MOZILLA_INTERNAL_API
  nsIEventTarget* thread = GetCurrentThreadEventTarget();
  if (!thread) {
    return NS_ERROR_UNEXPECTED;
  }
#else
  nsresult rv;
  nsCOMPtr<nsIThread> thread;
  rv = NS_GetCurrentThread(getter_AddRefs(thread));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
#endif

  return thread->DelayedDispatch(event.forget(), aDelayMs);
}

nsresult
NS_IdleDispatchToCurrentThread(already_AddRefed<nsIRunnable>&& aEvent)
{
  nsresult rv;
  nsCOMPtr<nsIRunnable> event(aEvent);
  NS_ENSURE_TRUE(event, NS_ERROR_INVALID_ARG);
#ifdef MOZILLA_INTERNAL_API
  nsIThread* thread = NS_GetCurrentThread();
  if (!thread) {
    return NS_ERROR_UNEXPECTED;
  }
#else
  nsCOMPtr<nsIThread> thread;
  rv = NS_GetCurrentThread(getter_AddRefs(thread));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
#endif
  // To keep us from leaking the runnable if dispatch method fails,
  // we grab the reference on failures and release it.
  nsIRunnable* temp = event.get();
  rv = thread->IdleDispatch(event.forget());
  if (NS_WARN_IF(NS_FAILED(rv))) {
    // Dispatch() leaked the reference to the event, but due to caller's
    // assumptions, we shouldn't leak here. And given we are on the same
    // thread as the dispatch target, it's mostly safe to do it here.
    NS_RELEASE(temp);
  }

  return rv;
}

class IdleRunnableWrapper : public IdleRunnable
{
public:
  explicit IdleRunnableWrapper(already_AddRefed<nsIRunnable>&& aEvent)
    : mRunnable(Move(aEvent))
  {
  }

  NS_IMETHOD Run() override
  {
    if (!mRunnable) {
      return NS_OK;
    }
    CancelTimer();
    nsCOMPtr<nsIRunnable> runnable = mRunnable.forget();
    return runnable->Run();
  }

  static void
  TimedOut(nsITimer* aTimer, void* aClosure)
  {
    RefPtr<IdleRunnableWrapper> runnable =
      static_cast<IdleRunnableWrapper*>(aClosure);
    runnable->Run();
  }

  void SetTimer(uint32_t aDelay, nsIEventTarget* aTarget) override
  {
    MOZ_ASSERT(aTarget);
    MOZ_ASSERT(!mTimer);
    mTimer = do_CreateInstance(NS_TIMER_CONTRACTID);
    if (mTimer) {
      mTimer->SetTarget(aTarget);
      mTimer->InitWithFuncCallback(TimedOut, this, aDelay,
                                   nsITimer::TYPE_ONE_SHOT);
    }
  }
private:
  ~IdleRunnableWrapper()
  {
    CancelTimer();
  }

  void CancelTimer()
  {
    if (mTimer) {
      mTimer->Cancel();
    }
  }

  nsCOMPtr<nsITimer> mTimer;
  nsCOMPtr<nsIRunnable> mRunnable;
};

extern nsresult
NS_IdleDispatchToCurrentThread(already_AddRefed<nsIRunnable>&& aEvent,
                               uint32_t aTimeout)
{
  nsCOMPtr<nsIRunnable> event(Move(aEvent));
  NS_ENSURE_TRUE(event, NS_ERROR_INVALID_ARG);
  nsCOMPtr<nsIIdleRunnable> idleEvent = do_QueryInterface(event);

  if (!idleEvent) {
    idleEvent = new IdleRunnableWrapper(event.forget());
    event = do_QueryInterface(idleEvent);
    MOZ_DIAGNOSTIC_ASSERT(event);
  }

  //XXX Using current thread for now as the nsIEventTarget.
  idleEvent->SetTimer(aTimeout, NS_GetCurrentThread());

  return NS_IdleDispatchToCurrentThread(event.forget());
}

#ifndef XPCOM_GLUE_AVOID_NSPR
nsresult
NS_ProcessPendingEvents(nsIThread* aThread, PRIntervalTime aTimeout)
{
  nsresult rv = NS_OK;

#ifdef MOZILLA_INTERNAL_API
  if (!aThread) {
    aThread = NS_GetCurrentThread();
    if (NS_WARN_IF(!aThread)) {
      return NS_ERROR_UNEXPECTED;
    }
  }
#else
  nsCOMPtr<nsIThread> current;
  if (!aThread) {
    rv = NS_GetCurrentThread(getter_AddRefs(current));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    aThread = current.get();
  }
#endif

  PRIntervalTime start = PR_IntervalNow();
  for (;;) {
    bool processedEvent;
    rv = aThread->ProcessNextEvent(false, &processedEvent);
    if (NS_FAILED(rv) || !processedEvent) {
      break;
    }
    if (PR_IntervalNow() - start > aTimeout) {
      break;
    }
  }
  return rv;
}
#endif // XPCOM_GLUE_AVOID_NSPR

inline bool
hasPendingEvents(nsIThread* aThread)
{
  bool val;
  return NS_SUCCEEDED(aThread->HasPendingEvents(&val)) && val;
}

bool
NS_HasPendingEvents(nsIThread* aThread)
{
  if (!aThread) {
#ifndef MOZILLA_INTERNAL_API
    nsCOMPtr<nsIThread> current;
    NS_GetCurrentThread(getter_AddRefs(current));
    return hasPendingEvents(current);
#else
    aThread = NS_GetCurrentThread();
    if (NS_WARN_IF(!aThread)) {
      return false;
    }
#endif
  }
  return hasPendingEvents(aThread);
}

bool
NS_ProcessNextEvent(nsIThread* aThread, bool aMayWait)
{
#ifdef MOZILLA_INTERNAL_API
  if (!aThread) {
    aThread = NS_GetCurrentThread();
    if (NS_WARN_IF(!aThread)) {
      return false;
    }
  }
#else
  nsCOMPtr<nsIThread> current;
  if (!aThread) {
    NS_GetCurrentThread(getter_AddRefs(current));
    if (NS_WARN_IF(!current)) {
      return false;
    }
    aThread = current.get();
  }
#endif
  bool val;
  return NS_SUCCEEDED(aThread->ProcessNextEvent(aMayWait, &val)) && val;
}

void
NS_SetCurrentThreadName(const char* aName)
{
  PR_SetCurrentThreadName(aName);
#ifdef MOZ_CRASHREPORTER
  CrashReporter::SetCurrentThreadName(aName);
#endif
}

#ifdef MOZILLA_INTERNAL_API
nsIThread*
NS_GetCurrentThread()
{
  return nsThreadManager::get().GetCurrentThread();
}
#endif

// nsThreadPoolNaming
nsCString
nsThreadPoolNaming::GetNextThreadName(const nsACString& aPoolName)
{
  nsCString name(aPoolName);
  name.AppendLiteral(" #");
  name.AppendInt(++mCounter, 10); // The counter is declared as atomic
  return name;
}

// nsAutoLowPriorityIO
nsAutoLowPriorityIO::nsAutoLowPriorityIO()
{
#if defined(XP_WIN)
  lowIOPrioritySet = SetThreadPriority(GetCurrentThread(),
                                       THREAD_MODE_BACKGROUND_BEGIN);
#elif defined(XP_MACOSX)
  oldPriority = getiopolicy_np(IOPOL_TYPE_DISK, IOPOL_SCOPE_THREAD);
  lowIOPrioritySet = oldPriority != -1 &&
                     setiopolicy_np(IOPOL_TYPE_DISK,
                                    IOPOL_SCOPE_THREAD,
                                    IOPOL_THROTTLE) != -1;
#else
  lowIOPrioritySet = false;
#endif
}

nsAutoLowPriorityIO::~nsAutoLowPriorityIO()
{
#if defined(XP_WIN)
  if (MOZ_LIKELY(lowIOPrioritySet)) {
    // On Windows the old thread priority is automatically restored
    SetThreadPriority(GetCurrentThread(), THREAD_MODE_BACKGROUND_END);
  }
#elif defined(XP_MACOSX)
  if (MOZ_LIKELY(lowIOPrioritySet)) {
    setiopolicy_np(IOPOL_TYPE_DISK, IOPOL_SCOPE_THREAD, oldPriority);
  }
#endif
}

namespace mozilla {

PRThread*
GetCurrentVirtualThread()
{
  return PR_GetCurrentThread();
}

PRThread*
GetCurrentPhysicalThread()
{
  return PR_GetCurrentThread();
}

nsIEventTarget*
GetCurrentThreadEventTarget()
{
  nsCOMPtr<nsIThread> thread;
  nsresult rv = NS_GetCurrentThread(getter_AddRefs(thread));
  if (NS_FAILED(rv)) {
    return nullptr;
  }

  return thread->EventTarget();
}

nsIEventTarget*
GetMainThreadEventTarget()
{
  nsCOMPtr<nsIThread> thread;
  nsresult rv = NS_GetMainThread(getter_AddRefs(thread));
  if (NS_FAILED(rv)) {
    return nullptr;
  }

  return thread->EventTarget();
}

nsISerialEventTarget*
GetCurrentThreadSerialEventTarget()
{
  nsCOMPtr<nsIThread> thread;
  nsresult rv = NS_GetCurrentThread(getter_AddRefs(thread));
  if (NS_FAILED(rv)) {
    return nullptr;
  }

  return thread->SerialEventTarget();
}

nsISerialEventTarget*
GetMainThreadSerialEventTarget()
{
  nsCOMPtr<nsIThread> thread;
  nsresult rv = NS_GetMainThread(getter_AddRefs(thread));
  if (NS_FAILED(rv)) {
    return nullptr;
  }

  return thread->SerialEventTarget();
}

} // namespace mozilla

bool
nsIEventTarget::IsOnCurrentThread()
{
  if (mVirtualThread) {
    return mVirtualThread == GetCurrentVirtualThread();
  }
  return IsOnCurrentThreadInfallible();
}
