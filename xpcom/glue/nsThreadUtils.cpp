/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsThreadUtils.h"
#include "mozilla/Attributes.h"

#ifdef MOZILLA_INTERNAL_API
# include "nsThreadManager.h"
#else
# include "nsXPCOMCIDInternal.h"
# include "nsIThreadManager.h"
# include "nsServiceManagerUtils.h"
#endif

#ifdef XP_WIN
#include <windows.h>
#endif

#include <pratom.h>
#include <prthread.h>

#ifndef XPCOM_GLUE_AVOID_NSPR

NS_IMPL_THREADSAFE_ISUPPORTS1(nsRunnable, nsIRunnable)
  
NS_IMETHODIMP
nsRunnable::Run()
{
  // Do nothing
  return NS_OK;
}

#endif  // XPCOM_GLUE_AVOID_NSPR

//-----------------------------------------------------------------------------

NS_METHOD
NS_NewThread(nsIThread **result, nsIRunnable *event, PRUint32 stackSize)
{
  nsCOMPtr<nsIThread> thread;
#ifdef MOZILLA_INTERNAL_API
  nsresult rv = nsThreadManager::get()->
      nsThreadManager::NewThread(0, stackSize, getter_AddRefs(thread));
#else
  nsresult rv;
  nsCOMPtr<nsIThreadManager> mgr =
      do_GetService(NS_THREADMANAGER_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mgr->NewThread(0, stackSize, getter_AddRefs(thread));
#endif
  NS_ENSURE_SUCCESS(rv, rv);

  if (event) {
    rv = thread->Dispatch(event, NS_DISPATCH_NORMAL);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  *result = nsnull;
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
  NS_ENSURE_SUCCESS(rv, rv);
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
  NS_ENSURE_SUCCESS(rv, rv);
  return mgr->GetMainThread(result);
#endif
}

#ifndef MOZILLA_INTERNAL_API
bool NS_IsMainThread()
{
  bool result = false;
  nsCOMPtr<nsIThreadManager> mgr =
    do_GetService(NS_THREADMANAGER_CONTRACTID);
  if (mgr)
    mgr->GetIsMainThread(&result);
  return bool(result);
}
#elif defined(XP_WIN)
extern DWORD gTLSThreadIDIndex;
bool
NS_IsMainThread()
{
  return TlsGetValue(gTLSThreadIDIndex) == (void*) mozilla::threads::Main;
}
#elif !defined(NS_TLS)
bool NS_IsMainThread()
{
  bool result = false;
  nsThreadManager::get()->nsThreadManager::GetIsMainThread(&result);
  return bool(result);
}
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
  NS_ENSURE_SUCCESS(rv, rv);
#endif
  return thread->Dispatch(event, NS_DISPATCH_NORMAL);
}

NS_METHOD
NS_DispatchToMainThread(nsIRunnable *event, PRUint32 dispatchFlags)
{
  nsCOMPtr<nsIThread> thread;
  nsresult rv = NS_GetMainThread(getter_AddRefs(thread));
  NS_ENSURE_SUCCESS(rv, rv);
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
    NS_ENSURE_STATE(thread);
  }
#else
  nsCOMPtr<nsIThread> current;
  if (!thread) {
    rv = NS_GetCurrentThread(getter_AddRefs(current));
    NS_ENSURE_SUCCESS(rv, rv);
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
    NS_ENSURE_TRUE(thread, false);
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
    NS_ENSURE_TRUE(thread, false);
  }
#else
  nsCOMPtr<nsIThread> current;
  if (!thread) {
    NS_GetCurrentThread(getter_AddRefs(current));
    NS_ENSURE_TRUE(current, false);
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

  NS_DECL_ISUPPORTS
  NS_DECL_NSIRUNNABLE

protected:
  const nsCString mName;
};

NS_IMPL_THREADSAFE_ISUPPORTS1(nsNameThreadRunnable, nsIRunnable)

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
