/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsThreadUtils.h"
#include "mozilla/Attributes.h"
#include "mozilla/Likely.h"

#ifdef MOZILLA_INTERNAL_API
# include "nsThreadManager.h"
#else
# include "nsXPCOMCIDInternal.h"
# include "nsIThreadManager.h"
# include "nsServiceManagerUtils.h"
#endif

#ifdef XP_WIN
#include <windows.h>
#include "mozilla/WindowsVersion.h"
using mozilla::IsVistaOrLater;
#elif defined(XP_MACOSX)
#include <sys/resource.h>
#endif

#include <pratom.h>
#include <prthread.h>

#ifndef XPCOM_GLUE_AVOID_NSPR

NS_IMPL_ISUPPORTS1(nsRunnable, nsIRunnable)

NS_IMETHODIMP
nsRunnable::Run()
{
  // Do nothing
  return NS_OK;
}

NS_IMPL_ISUPPORTS2(nsCancelableRunnable, nsICancelableRunnable,
                              nsIRunnable)

NS_IMETHODIMP
nsCancelableRunnable::Run()
{
  // Do nothing
  return NS_OK;
}

NS_IMETHODIMP
nsCancelableRunnable::Cancel()
{
  // Do nothing
  return NS_OK;
}

#endif  // XPCOM_GLUE_AVOID_NSPR

//-----------------------------------------------------------------------------

NS_METHOD
NS_NewThread(nsIThread **result, nsIRunnable *event, uint32_t stackSize)
{
  nsCOMPtr<nsIThread> thread;
#ifdef MOZILLA_INTERNAL_API
  nsresult rv = nsThreadManager::get()->
      nsThreadManager::NewThread(0, stackSize, getter_AddRefs(thread));
#else
  nsresult rv;
  nsCOMPtr<nsIThreadManager> mgr =
      do_GetService(NS_THREADMANAGER_CONTRACTID, &rv);
  if (NS_WARN_IF(NS_FAILED(rv)))
    return rv;

  rv = mgr->NewThread(0, stackSize, getter_AddRefs(thread));
#endif
  if (NS_WARN_IF(NS_FAILED(rv)))
    return rv;

  if (event) {
    rv = thread->Dispatch(event, NS_DISPATCH_NORMAL);
    if (NS_WARN_IF(NS_FAILED(rv)))
      return rv;
  }

  *result = nullptr;
  thread.swap(*result);
  return NS_OK;
}

NS_METHOD
NS_GetCurrentThread(nsIThread **result)
{
#ifdef MOZILLA_INTERNAL_API
  return nsThreadManager::get()->nsThreadManager::GetCurrentThread(result);
#else
  nsresult rv;
  nsCOMPtr<nsIThreadManager> mgr =
      do_GetService(NS_THREADMANAGER_CONTRACTID, &rv);
  if (NS_WARN_IF(NS_FAILED(rv)))
    return rv;
  return mgr->GetCurrentThread(result);
#endif
}

NS_METHOD
NS_GetMainThread(nsIThread **result)
{
#ifdef MOZILLA_INTERNAL_API
  return nsThreadManager::get()->nsThreadManager::GetMainThread(result);
#else
  nsresult rv;
  nsCOMPtr<nsIThreadManager> mgr =
      do_GetService(NS_THREADMANAGER_CONTRACTID, &rv);
  if (NS_WARN_IF(NS_FAILED(rv)))
    return rv;
  return mgr->GetMainThread(result);
#endif
}

#if defined(MOZILLA_INTERNAL_API) && defined(XP_WIN)
extern DWORD gTLSThreadIDIndex;
bool
NS_IsMainThread()
{
  return TlsGetValue(gTLSThreadIDIndex) == (void*) mozilla::threads::Main;
}
#elif defined(MOZILLA_INTERNAL_API) && defined(NS_TLS)
#ifdef MOZ_ASAN
// Temporary workaround, see bug 895845
bool NS_IsMainThread()
{
  return gTLSThreadID == mozilla::threads::Main;
}
#else
// NS_IsMainThread() is defined inline in MainThreadUtils.h
#endif
#else
#ifdef MOZILLA_INTERNAL_API
bool NS_IsMainThread()
{
  bool result = false;
  nsThreadManager::get()->nsThreadManager::GetIsMainThread(&result);
  return bool(result);
}
#else
bool NS_IsMainThread()
{
  bool result = false;
  nsCOMPtr<nsIThreadManager> mgr =
    do_GetService(NS_THREADMANAGER_CONTRACTID);
  if (mgr)
    mgr->GetIsMainThread(&result);
  return bool(result);
}
#endif
#endif

NS_METHOD
NS_DispatchToCurrentThread(nsIRunnable *event)
{
#ifdef MOZILLA_INTERNAL_API
  nsIThread *thread = NS_GetCurrentThread();
  if (!thread) { return NS_ERROR_UNEXPECTED; }
#else
  nsCOMPtr<nsIThread> thread;
  nsresult rv = NS_GetCurrentThread(getter_AddRefs(thread));
  if (NS_WARN_IF(NS_FAILED(rv)))
    return rv;
#endif
  return thread->Dispatch(event, NS_DISPATCH_NORMAL);
}

NS_METHOD
NS_DispatchToMainThread(nsIRunnable *event, uint32_t dispatchFlags)
{
  nsCOMPtr<nsIThread> thread;
  nsresult rv = NS_GetMainThread(getter_AddRefs(thread));
  if (NS_WARN_IF(NS_FAILED(rv)))
    return rv;
  return thread->Dispatch(event, dispatchFlags);
}

#ifndef XPCOM_GLUE_AVOID_NSPR
NS_METHOD
NS_ProcessPendingEvents(nsIThread *thread, PRIntervalTime timeout)
{
  nsresult rv = NS_OK;

#ifdef MOZILLA_INTERNAL_API
  if (!thread) {
    thread = NS_GetCurrentThread();
    if (NS_WARN_IF(!thread))
      return NS_ERROR_UNEXPECTED;
  }
#else
  nsCOMPtr<nsIThread> current;
  if (!thread) {
    rv = NS_GetCurrentThread(getter_AddRefs(current));
    if (NS_WARN_IF(NS_FAILED(rv)))
      return rv;
    thread = current.get();
  }
#endif

  PRIntervalTime start = PR_IntervalNow();
  for (;;) {
    bool processedEvent;
    rv = thread->ProcessNextEvent(false, &processedEvent);
    if (NS_FAILED(rv) || !processedEvent)
      break;
    if (PR_IntervalNow() - start > timeout)
      break;
  }
  return rv;
}
#endif // XPCOM_GLUE_AVOID_NSPR

inline bool
hasPendingEvents(nsIThread *thread)
{
  bool val;
  return NS_SUCCEEDED(thread->HasPendingEvents(&val)) && val;
}

bool
NS_HasPendingEvents(nsIThread *thread)
{
  if (!thread) {
#ifndef MOZILLA_INTERNAL_API
    nsCOMPtr<nsIThread> current;
    NS_GetCurrentThread(getter_AddRefs(current));
    return hasPendingEvents(current);
#else
    thread = NS_GetCurrentThread();
    if (NS_WARN_IF(!thread))
      return false;
#endif
  }
  return hasPendingEvents(thread);
}

bool
NS_ProcessNextEvent(nsIThread *thread, bool mayWait)
{
#ifdef MOZILLA_INTERNAL_API
  if (!thread) {
    thread = NS_GetCurrentThread();
    if (NS_WARN_IF(!thread))
      return false;
  }
#else
  nsCOMPtr<nsIThread> current;
  if (!thread) {
    NS_GetCurrentThread(getter_AddRefs(current));
    if (NS_WARN_IF(!current))
      return false;
    thread = current.get();
  }
#endif
  bool val;
  return NS_SUCCEEDED(thread->ProcessNextEvent(mayWait, &val)) && val;
}

#ifndef XPCOM_GLUE_AVOID_NSPR

namespace {

class nsNameThreadRunnable MOZ_FINAL : public nsIRunnable
{
public:
  nsNameThreadRunnable(const nsACString &name) : mName(name) { }

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIRUNNABLE

protected:
  const nsCString mName;
};

NS_IMPL_ISUPPORTS1(nsNameThreadRunnable, nsIRunnable)

NS_IMETHODIMP
nsNameThreadRunnable::Run()
{
  PR_SetCurrentThreadName(mName.BeginReading());
  return NS_OK;
}

} // anonymous namespace

void
NS_SetThreadName(nsIThread *thread, const nsACString &name)
{
  if (!thread)
    return;

  thread->Dispatch(new nsNameThreadRunnable(name),
                   nsIEventTarget::DISPATCH_NORMAL);
}

#else // !XPCOM_GLUE_AVOID_NSPR

void
NS_SetThreadName(nsIThread *thread, const nsACString &name)
{
  // No NSPR, no love.
}

#endif

#ifdef MOZILLA_INTERNAL_API
nsIThread *
NS_GetCurrentThread() {
  return nsThreadManager::get()->GetCurrentThread();
}
#endif

// nsThreadPoolNaming
void
nsThreadPoolNaming::SetThreadPoolName(const nsACString & aPoolName,
                                      nsIThread * aThread)
{
  nsCString name(aPoolName);
  name.Append(NS_LITERAL_CSTRING(" #"));
  name.AppendInt(++mCounter, 10); // The counter is declared as volatile

  if (aThread) {
    // Set on the target thread
    NS_SetThreadName(aThread, name);
  }
  else {
    // Set on the current thread
    PR_SetCurrentThreadName(name.BeginReading());
  }
}

// nsAutoLowPriorityIO
nsAutoLowPriorityIO::nsAutoLowPriorityIO()
{
#if defined(XP_WIN)
  lowIOPrioritySet = IsVistaOrLater() &&
                     SetThreadPriority(GetCurrentThread(),
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

