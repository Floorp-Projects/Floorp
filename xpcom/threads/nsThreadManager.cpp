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

#include "nsThreadManager.h"
#include "nsThread.h"
#include "nsThreadUtils.h"
#include "nsIClassInfoImpl.h"
#include "nsTArray.h"
#include "nsAutoPtr.h"
#include "nsCycleCollectorUtils.h"

using namespace mozilla;

#ifdef XP_WIN
#include <windows.h>
DWORD gTLSThreadIDIndex = TlsAlloc();
#elif defined(NS_TLS)
NS_TLS mozilla::threads::ID gTLSThreadID = mozilla::threads::Generic;
#endif

typedef nsTArray< nsRefPtr<nsThread> > nsThreadArray;

//-----------------------------------------------------------------------------

static void
ReleaseObject(void *data)
{
  static_cast<nsISupports *>(data)->Release();
}

static PLDHashOperator
AppendAndRemoveThread(const void *key, nsRefPtr<nsThread> &thread, void *arg)
{
  nsThreadArray *threads = static_cast<nsThreadArray *>(arg);
  threads->AppendElement(thread);
  return PL_DHASH_REMOVE;
}

//-----------------------------------------------------------------------------

nsThreadManager nsThreadManager::sInstance;

// statically allocated instance
NS_IMETHODIMP_(nsrefcnt) nsThreadManager::AddRef() { return 2; }
NS_IMETHODIMP_(nsrefcnt) nsThreadManager::Release() { return 1; }
NS_IMPL_CLASSINFO(nsThreadManager, NULL,
                  nsIClassInfo::THREADSAFE | nsIClassInfo::SINGLETON,
                  NS_THREADMANAGER_CID)
NS_IMPL_QUERY_INTERFACE1_CI(nsThreadManager, nsIThreadManager)
NS_IMPL_CI_INTERFACE_GETTER1(nsThreadManager, nsIThreadManager)

//-----------------------------------------------------------------------------

nsresult
nsThreadManager::Init()
{
  if (!mThreadsByPRThread.Init())
    return NS_ERROR_OUT_OF_MEMORY;

  if (PR_NewThreadPrivateIndex(&mCurThreadIndex, ReleaseObject) == PR_FAILURE)
    return NS_ERROR_FAILURE;

  mLock = new Mutex("nsThreadManager.mLock");

  // Setup "main" thread
  mMainThread = new nsThread();
  if (!mMainThread)
    return NS_ERROR_OUT_OF_MEMORY;

  nsresult rv = mMainThread->InitCurrentThread();
  if (NS_FAILED(rv)) {
    mMainThread = nsnull;
    return rv;
  }

  // We need to keep a pointer to the current thread, so we can satisfy
  // GetIsMainThread calls that occur post-Shutdown.
  mMainThread->GetPRThread(&mMainPRThread);

#ifdef XP_WIN
  TlsSetValue(gTLSThreadIDIndex, (void*) mozilla::threads::Main);
#elif defined(NS_TLS)
  gTLSThreadID = mozilla::threads::Main;
#endif

  mInitialized = PR_TRUE;
  return NS_OK;
}

void
nsThreadManager::Shutdown()
{
  NS_ASSERTION(NS_IsMainThread(), "shutdown not called from main thread");

  // Prevent further access to the thread manager (no more new threads!)
  //
  // XXX What happens if shutdown happens before NewThread completes?
  //     Fortunately, NewThread is only called on the main thread for now.
  //
  mInitialized = PR_FALSE;

  // Empty the main thread event queue before we begin shutting down threads.
  NS_ProcessPendingEvents(mMainThread);

  // We gather the threads from the hashtable into a list, so that we avoid
  // holding the hashtable lock while calling nsIThread::Shutdown.
  nsThreadArray threads;
  {
    MutexAutoLock lock(*mLock);
    mThreadsByPRThread.Enumerate(AppendAndRemoveThread, &threads);
  }

  // It's tempting to walk the list of threads here and tell them each to stop
  // accepting new events, but that could lead to badness if one of those
  // threads is stuck waiting for a response from another thread.  To do it
  // right, we'd need some way to interrupt the threads.
  // 
  // Instead, we process events on the current thread while waiting for threads
  // to shutdown.  This means that we have to preserve a mostly functioning
  // world until such time as the threads exit.

  // Shutdown all threads that require it (join with threads that we created).
  for (PRUint32 i = 0; i < threads.Length(); ++i) {
    nsThread *thread = threads[i];
    if (thread->ShutdownRequired())
      thread->Shutdown();
  }

  // In case there are any more events somehow...
  NS_ProcessPendingEvents(mMainThread);

  // There are no more background threads at this point.

  // Clear the table of threads.
  {
    MutexAutoLock lock(*mLock);
    mThreadsByPRThread.Clear();
  }

  // Normally thread shutdown clears the observer for the thread, but since the
  // main thread is special we do it manually here after we're sure all events
  // have been processed.
  mMainThread->SetObserver(nsnull);

  // Release main thread object.
  mMainThread = nsnull;
  mLock = nsnull;

  // Remove the TLS entry for the main thread.
  PR_SetThreadPrivate(mCurThreadIndex, nsnull);
}

void
nsThreadManager::RegisterCurrentThread(nsThread *thread)
{
  NS_ASSERTION(thread->GetPRThread() == PR_GetCurrentThread(), "bad thread");

  MutexAutoLock lock(*mLock);

  mThreadsByPRThread.Put(thread->GetPRThread(), thread);  // XXX check OOM?

  NS_ADDREF(thread);  // for TLS entry
  PR_SetThreadPrivate(mCurThreadIndex, thread);
}

void
nsThreadManager::UnregisterCurrentThread(nsThread *thread)
{
  NS_ASSERTION(thread->GetPRThread() == PR_GetCurrentThread(), "bad thread");

  MutexAutoLock lock(*mLock);

  mThreadsByPRThread.Remove(thread->GetPRThread());

  PR_SetThreadPrivate(mCurThreadIndex, nsnull);
  // Ref-count balanced via ReleaseObject
}

nsThread *
nsThreadManager::GetCurrentThread()
{
  // read thread local storage
  void *data = PR_GetThreadPrivate(mCurThreadIndex);
  if (data)
    return static_cast<nsThread *>(data);

  if (!mInitialized) {
    return nsnull;
  }

  // OK, that's fine.  We'll dynamically create one :-)
  nsRefPtr<nsThread> thread = new nsThread();
  if (!thread || NS_FAILED(thread->InitCurrentThread()))
    return nsnull;

  return thread.get();  // reference held in TLS
}

NS_IMETHODIMP
nsThreadManager::NewThread(PRUint32 creationFlags,
                           PRUint32 stackSize,
                           nsIThread **result)
{
  // No new threads during Shutdown
  NS_ENSURE_TRUE(mInitialized, NS_ERROR_NOT_INITIALIZED);

  nsThread *thr = new nsThread(stackSize);
  if (!thr)
    return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(thr);

  nsresult rv = thr->Init();
  if (NS_FAILED(rv)) {
    NS_RELEASE(thr);
    return rv;
  }

  // At this point, we expect that the thread has been registered in mThread;
  // however, it is possible that it could have also been replaced by now, so
  // we cannot really assert that it was added.

  *result = thr;
  return NS_OK;
}

NS_IMETHODIMP
nsThreadManager::GetThreadFromPRThread(PRThread *thread, nsIThread **result)
{
  // Keep this functioning during Shutdown
  NS_ENSURE_TRUE(mMainThread, NS_ERROR_NOT_INITIALIZED);
  NS_ENSURE_ARG_POINTER(thread);

  nsRefPtr<nsThread> temp;
  {
    MutexAutoLock lock(*mLock);
    mThreadsByPRThread.Get(thread, getter_AddRefs(temp));
  }

  NS_IF_ADDREF(*result = temp);
  return NS_OK;
}

NS_IMETHODIMP
nsThreadManager::GetMainThread(nsIThread **result)
{
  // Keep this functioning during Shutdown
  NS_ENSURE_TRUE(mMainThread, NS_ERROR_NOT_INITIALIZED);
  NS_ADDREF(*result = mMainThread);
  return NS_OK;
}

NS_IMETHODIMP
nsThreadManager::GetCurrentThread(nsIThread **result)
{
  // Keep this functioning during Shutdown
  NS_ENSURE_TRUE(mMainThread, NS_ERROR_NOT_INITIALIZED);
  *result = GetCurrentThread();
  if (!*result)
    return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(*result);
  return NS_OK;
}

NS_IMETHODIMP
nsThreadManager::GetIsMainThread(bool *result)
{
  // This method may be called post-Shutdown

  *result = (PR_GetCurrentThread() == mMainPRThread);
  return NS_OK;
}

NS_IMETHODIMP
nsThreadManager::GetIsCycleCollectorThread(bool *result)
{
  *result = bool(NS_IsCycleCollectorThread());
  return NS_OK;
}
