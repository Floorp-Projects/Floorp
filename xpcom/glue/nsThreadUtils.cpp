/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla code.
 *
 * The Initial Developer of the Original Code is Google Inc.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Darin Fisher <darin@meer.net>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsThreadUtils.h"

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
    rv = thread->ProcessNextEvent(PR_FALSE, &processedEvent);
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
    NS_ENSURE_TRUE(thread, PR_FALSE);
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
    NS_ENSURE_TRUE(thread, PR_FALSE);
  }
#else
  nsCOMPtr<nsIThread> current;
  if (!thread) {
    NS_GetCurrentThread(getter_AddRefs(current));
    NS_ENSURE_TRUE(current, PR_FALSE);
    thread = current.get();
  }
#endif
  bool val;
  return NS_SUCCEEDED(thread->ProcessNextEvent(mayWait, &val)) && val;
}

#ifdef MOZILLA_INTERNAL_API
nsIThread *
NS_GetCurrentThread() {
  return nsThreadManager::get()->GetCurrentThread();
}
#endif
