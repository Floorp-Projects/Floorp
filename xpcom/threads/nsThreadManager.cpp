/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsThreadManager.h"
#include "nsThread.h"
#include "nsThreadUtils.h"
#include "nsIClassInfoImpl.h"
#include "nsTArray.h"
#include "nsAutoPtr.h"
#ifdef MOZ_CANARY
#include <fcntl.h>
#include <unistd.h>
#endif

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
AppendAndRemoveThread(PRThread *key, nsRefPtr<nsThread> &thread, void *arg)
{
  nsThreadArray *threads = static_cast<nsThreadArray *>(arg);
  threads->AppendElement(thread);
  return PL_DHASH_REMOVE;
}

// statically allocated instance
NS_IMETHODIMP_(nsrefcnt) nsThreadManager::AddRef() { return 2; }
NS_IMETHODIMP_(nsrefcnt) nsThreadManager::Release() { return 1; }
NS_IMPL_CLASSINFO(nsThreadManager, nullptr,
                  nsIClassInfo::THREADSAFE | nsIClassInfo::SINGLETON,
                  NS_THREADMANAGER_CID)
NS_IMPL_QUERY_INTERFACE1_CI(nsThreadManager, nsIThreadManager)
NS_IMPL_CI_INTERFACE_GETTER1(nsThreadManager, nsIThreadManager)

//-----------------------------------------------------------------------------

nsresult
nsThreadManager::Init()
{
  // Child processes need to initialize the thread manager before they
  // initialize XPCOM in order to set up the crash reporter. This leads to
  // situations where we get initialized twice.
  if (mInitialized)
    return NS_OK;

  if (PR_NewThreadPrivateIndex(&mCurThreadIndex, ReleaseObject) == PR_FAILURE)
    return NS_ERROR_FAILURE;

  mLock = new Mutex("nsThreadManager.mLock");

#ifdef MOZ_CANARY
  const int flags = O_WRONLY | O_APPEND | O_CREAT | O_NONBLOCK;
  const mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
  char* env_var_flag = getenv("MOZ_KILL_CANARIES");
  sCanaryOutputFD = env_var_flag ? (env_var_flag[0] ?
      open(env_var_flag, flags, mode) :
      STDERR_FILENO) : 0;
#endif

  // Setup "main" thread
  mMainThread = new nsThread(nsThread::MAIN_THREAD, 0);

  nsresult rv = mMainThread->InitCurrentThread();
  if (NS_FAILED(rv)) {
    mMainThread = nullptr;
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

  mInitialized = true;
  return NS_OK;
}

void
nsThreadManager::Shutdown()
{
  MOZ_ASSERT(NS_IsMainThread(), "shutdown not called from main thread");

  // Prevent further access to the thread manager (no more new threads!)
  //
  // XXX What happens if shutdown happens before NewThread completes?
  //     Fortunately, NewThread is only called on the main thread for now.
  //
  mInitialized = false;

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
  for (uint32_t i = 0; i < threads.Length(); ++i) {
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
  mMainThread->SetObserver(nullptr);
  mMainThread->ClearObservers();

  // Release main thread object.
  mMainThread = nullptr;
  mLock = nullptr;

  // Remove the TLS entry for the main thread.
  PR_SetThreadPrivate(mCurThreadIndex, nullptr);
}

void
nsThreadManager::RegisterCurrentThread(nsThread *thread)
{
  MOZ_ASSERT(thread->GetPRThread() == PR_GetCurrentThread(), "bad thread");

  MutexAutoLock lock(*mLock);

  ++mCurrentNumberOfThreads;
  if (mCurrentNumberOfThreads > mHighestNumberOfThreads) {
    mHighestNumberOfThreads = mCurrentNumberOfThreads;
  }

  mThreadsByPRThread.Put(thread->GetPRThread(), thread);  // XXX check OOM?

  NS_ADDREF(thread);  // for TLS entry
  PR_SetThreadPrivate(mCurThreadIndex, thread);
}

void
nsThreadManager::UnregisterCurrentThread(nsThread *thread)
{
  MOZ_ASSERT(thread->GetPRThread() == PR_GetCurrentThread(), "bad thread");

  MutexAutoLock lock(*mLock);

  --mCurrentNumberOfThreads;
  mThreadsByPRThread.Remove(thread->GetPRThread());

  PR_SetThreadPrivate(mCurThreadIndex, nullptr);
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
    return nullptr;
  }

  // OK, that's fine.  We'll dynamically create one :-)
  nsRefPtr<nsThread> thread = new nsThread(nsThread::NOT_MAIN_THREAD, 0);
  if (!thread || NS_FAILED(thread->InitCurrentThread()))
    return nullptr;

  return thread.get();  // reference held in TLS
}

NS_IMETHODIMP
nsThreadManager::NewThread(uint32_t creationFlags,
                           uint32_t stackSize,
                           nsIThread **result)
{
  // No new threads during Shutdown
  if (NS_WARN_IF(!mInitialized))
    return NS_ERROR_NOT_INITIALIZED;

  nsThread *thr = new nsThread(nsThread::NOT_MAIN_THREAD, stackSize);
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
  if (NS_WARN_IF(!mMainThread))
    return NS_ERROR_NOT_INITIALIZED;
  if (NS_WARN_IF(!thread))
    return NS_ERROR_INVALID_ARG;

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
  if (NS_WARN_IF(!mMainThread))
    return NS_ERROR_NOT_INITIALIZED;
  NS_ADDREF(*result = mMainThread);
  return NS_OK;
}

NS_IMETHODIMP
nsThreadManager::GetCurrentThread(nsIThread **result)
{
  // Keep this functioning during Shutdown
  if (NS_WARN_IF(!mMainThread))
    return NS_ERROR_NOT_INITIALIZED;
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

uint32_t
nsThreadManager::GetHighestNumberOfThreads()
{
  MutexAutoLock lock(*mLock);
  return mHighestNumberOfThreads;
}
