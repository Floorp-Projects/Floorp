/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsThreadManager.h"
#include "nsThread.h"
#include "nsThreadUtils.h"
#include "nsIClassInfoImpl.h"
#include "nsTArray.h"
#include "nsAutoPtr.h"
#include "mozilla/ThreadLocal.h"
#include "mozilla/ReentrantMonitor.h"
#ifdef MOZ_CANARY
#include <fcntl.h>
#include <unistd.h>
#endif

using namespace mozilla;

static mozilla::ThreadLocal<bool> sTLSIsMainThread;

bool
NS_IsMainThread()
{
  return sTLSIsMainThread.get();
}

void
NS_SetMainThread()
{
  if (!sTLSIsMainThread.initialized()) {
    if (!sTLSIsMainThread.init()) {
      MOZ_CRASH();
    }
    sTLSIsMainThread.set(true);
  }
  MOZ_ASSERT(NS_IsMainThread());
}

typedef nsTArray<nsRefPtr<nsThread>> nsThreadArray;

#ifdef MOZ_NUWA_PROCESS
class NotifyAllThreadsWereIdle: public nsRunnable
{
public:

  NotifyAllThreadsWereIdle(
    nsTArray<nsRefPtr<nsThreadManager::AllThreadsWereIdleListener>>* aListeners)
    : mListeners(aListeners)
  {
  }

  virtual NS_IMETHODIMP
  Run() {
    // Copy listener array, which may be modified during call back.
    nsTArray<nsRefPtr<nsThreadManager::AllThreadsWereIdleListener>> arr(*mListeners);
    for (size_t i = 0; i < arr.Length(); i++) {
      arr[i]->OnAllThreadsWereIdle();
    }
    return NS_OK;
  }

private:
  // Raw pointer, since it's pointing to a  member of thread manager.
  nsTArray<nsRefPtr<nsThreadManager::AllThreadsWereIdleListener>>* mListeners;
};

struct nsThreadManager::ThreadStatusInfo {
  Atomic<bool> mWorking;
  Atomic<bool> mWillBeWorking;
  bool mIgnored;
  ThreadStatusInfo()
    : mWorking(false)
    , mWillBeWorking(false)
    , mIgnored(false)
  {
  }
};
#endif // MOZ_NUWA_PROCESS

//-----------------------------------------------------------------------------

static void
ReleaseObject(void* aData)
{
  static_cast<nsISupports*>(aData)->Release();
}

#ifdef MOZ_NUWA_PROCESS
void
nsThreadManager::DeleteThreadStatusInfo(void* aData)
{
  nsThreadManager* mgr = nsThreadManager::get();
  nsThreadManager::ThreadStatusInfo* thrInfo =
    static_cast<nsThreadManager::ThreadStatusInfo*>(aData);
  {
    ReentrantMonitorAutoEnter mon(*(mgr->mMonitor));
    mgr->mThreadStatusInfos.RemoveElement(thrInfo);
    if (NS_IsMainThread()) {
      mgr->mMainThreadStatusInfo = nullptr;
    }
  }
  delete thrInfo;
}
#endif

static PLDHashOperator
AppendAndRemoveThread(PRThread* aKey, nsRefPtr<nsThread>& aThread, void* aArg)
{
  nsThreadArray* threads = static_cast<nsThreadArray*>(aArg);
  threads->AppendElement(aThread);
  return PL_DHASH_REMOVE;
}

// statically allocated instance
NS_IMETHODIMP_(MozExternalRefCountType)
nsThreadManager::AddRef()
{
  return 2;
}
NS_IMETHODIMP_(MozExternalRefCountType)
nsThreadManager::Release()
{
  return 1;
}
NS_IMPL_CLASSINFO(nsThreadManager, nullptr,
                  nsIClassInfo::THREADSAFE | nsIClassInfo::SINGLETON,
                  NS_THREADMANAGER_CID)
NS_IMPL_QUERY_INTERFACE_CI(nsThreadManager, nsIThreadManager)
NS_IMPL_CI_INTERFACE_GETTER(nsThreadManager, nsIThreadManager)

//-----------------------------------------------------------------------------

nsresult
nsThreadManager::Init()
{
  // Child processes need to initialize the thread manager before they
  // initialize XPCOM in order to set up the crash reporter. This leads to
  // situations where we get initialized twice.
  if (mInitialized) {
    return NS_OK;
  }

  if (PR_NewThreadPrivateIndex(&mCurThreadIndex, ReleaseObject) == PR_FAILURE) {
    return NS_ERROR_FAILURE;
  }

#ifdef MOZ_NUWA_PROCESS
  if (PR_NewThreadPrivateIndex(
      &mThreadStatusInfoIndex,
      nsThreadManager::DeleteThreadStatusInfo) == PR_FAILURE) {
    return NS_ERROR_FAILURE;
  }
#endif // MOZ_NUWA_PROCESS

#ifdef MOZ_NUWA_PROCESS
  mMonitor = MakeUnique<ReentrantMonitor>("nsThreadManager.mMonitor");
#endif // MOZ_NUWA_PROCESS

#ifdef MOZ_CANARY
  const int flags = O_WRONLY | O_APPEND | O_CREAT | O_NONBLOCK;
  const mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
  char* env_var_flag = getenv("MOZ_KILL_CANARIES");
  sCanaryOutputFD =
    env_var_flag ? (env_var_flag[0] ? open(env_var_flag, flags, mode) :
                                      STDERR_FILENO) :
                   0;
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

  mInitialized = true;
  return NS_OK;
}

void
nsThreadManager::Shutdown()
{
  MOZ_ASSERT(NS_IsMainThread(), "shutdown not called from main thread");

  // Prevent further access to the thread manager (no more new threads!)
  //
  // What happens if shutdown happens before NewThread completes?
  // We Shutdown() the new thread, and return error if we've started Shutdown
  // between when NewThread started, and when the thread finished initializing
  // and registering with ThreadManager.
  //
  mInitialized = false;

  // Empty the main thread event queue before we begin shutting down threads.
  NS_ProcessPendingEvents(mMainThread);

  // We gather the threads from the hashtable into a list, so that we avoid
  // holding the hashtable lock while calling nsIThread::Shutdown.
  nsThreadArray threads;
  {
    OffTheBooksMutexAutoLock lock(mLock);
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
    nsThread* thread = threads[i];
    if (thread->ShutdownRequired()) {
      thread->Shutdown();
    }
  }

  // In case there are any more events somehow...
  NS_ProcessPendingEvents(mMainThread);

  // There are no more background threads at this point.

  // Clear the table of threads.
  {
    OffTheBooksMutexAutoLock lock(mLock);
    mThreadsByPRThread.Clear();
  }

  // Normally thread shutdown clears the observer for the thread, but since the
  // main thread is special we do it manually here after we're sure all events
  // have been processed.
  mMainThread->SetObserver(nullptr);
  mMainThread->ClearObservers();

  // Release main thread object.
  mMainThread = nullptr;

  // Remove the TLS entry for the main thread.
  PR_SetThreadPrivate(mCurThreadIndex, nullptr);
#ifdef MOZ_NUWA_PROCESS
  PR_SetThreadPrivate(mThreadStatusInfoIndex, nullptr);
#endif
}

void
nsThreadManager::RegisterCurrentThread(nsThread* aThread)
{
  MOZ_ASSERT(aThread->GetPRThread() == PR_GetCurrentThread(), "bad aThread");

  OffTheBooksMutexAutoLock lock(mLock);

  ++mCurrentNumberOfThreads;
  if (mCurrentNumberOfThreads > mHighestNumberOfThreads) {
    mHighestNumberOfThreads = mCurrentNumberOfThreads;
  }

  mThreadsByPRThread.Put(aThread->GetPRThread(), aThread);  // XXX check OOM?

  NS_ADDREF(aThread);  // for TLS entry
  PR_SetThreadPrivate(mCurThreadIndex, aThread);
}

void
nsThreadManager::UnregisterCurrentThread(nsThread* aThread)
{
  MOZ_ASSERT(aThread->GetPRThread() == PR_GetCurrentThread(), "bad aThread");

  OffTheBooksMutexAutoLock lock(mLock);

  --mCurrentNumberOfThreads;
  mThreadsByPRThread.Remove(aThread->GetPRThread());

  PR_SetThreadPrivate(mCurThreadIndex, nullptr);
  // Ref-count balanced via ReleaseObject
#ifdef MOZ_NUWA_PROCESS
  PR_SetThreadPrivate(mThreadStatusInfoIndex, nullptr);
#endif
}

nsThread*
nsThreadManager::GetCurrentThread()
{
  // read thread local storage
  void* data = PR_GetThreadPrivate(mCurThreadIndex);
  if (data) {
    return static_cast<nsThread*>(data);
  }

  if (!mInitialized) {
    return nullptr;
  }

  // OK, that's fine.  We'll dynamically create one :-)
  nsRefPtr<nsThread> thread = new nsThread(nsThread::NOT_MAIN_THREAD, 0);
  if (!thread || NS_FAILED(thread->InitCurrentThread())) {
    return nullptr;
  }

  return thread.get();  // reference held in TLS
}

#ifdef MOZ_NUWA_PROCESS
nsThreadManager::ThreadStatusInfo*
nsThreadManager::GetCurrentThreadStatusInfo()
{
  void* data = PR_GetThreadPrivate(mThreadStatusInfoIndex);
  if (!data) {
    ThreadStatusInfo *thrInfo = new ThreadStatusInfo();
    PR_SetThreadPrivate(mThreadStatusInfoIndex, thrInfo);
    data = thrInfo;

    ReentrantMonitorAutoEnter mon(*mMonitor);
    mThreadStatusInfos.AppendElement(thrInfo);
    if (NS_IsMainThread()) {
      mMainThreadStatusInfo = thrInfo;
    }
  }

  return static_cast<ThreadStatusInfo*>(data);
}
#endif

NS_IMETHODIMP
nsThreadManager::NewThread(uint32_t aCreationFlags,
                           uint32_t aStackSize,
                           nsIThread** aResult)
{
  // Note: can be called from arbitrary threads
  
  // No new threads during Shutdown
  if (NS_WARN_IF(!mInitialized)) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  nsRefPtr<nsThread> thr = new nsThread(nsThread::NOT_MAIN_THREAD, aStackSize);
  nsresult rv = thr->Init();  // Note: blocks until the new thread has been set up
  if (NS_FAILED(rv)) {
    return rv;
  }

  // At this point, we expect that the thread has been registered in mThreadByPRThread;
  // however, it is possible that it could have also been replaced by now, so
  // we cannot really assert that it was added.  Instead, kill it if we entered
  // Shutdown() during/before Init()

  if (NS_WARN_IF(!mInitialized)) {
    if (thr->ShutdownRequired()) {
      thr->Shutdown(); // ok if it happens multiple times
    }
    return NS_ERROR_NOT_INITIALIZED;
  }

  thr.forget(aResult);
  return NS_OK;
}

NS_IMETHODIMP
nsThreadManager::GetThreadFromPRThread(PRThread* aThread, nsIThread** aResult)
{
  // Keep this functioning during Shutdown
  if (NS_WARN_IF(!mMainThread)) {
    return NS_ERROR_NOT_INITIALIZED;
  }
  if (NS_WARN_IF(!aThread)) {
    return NS_ERROR_INVALID_ARG;
  }

  nsRefPtr<nsThread> temp;
  {
    OffTheBooksMutexAutoLock lock(mLock);
    mThreadsByPRThread.Get(aThread, getter_AddRefs(temp));
  }

  NS_IF_ADDREF(*aResult = temp);
  return NS_OK;
}

NS_IMETHODIMP
nsThreadManager::GetMainThread(nsIThread** aResult)
{
  // Keep this functioning during Shutdown
  if (NS_WARN_IF(!mMainThread)) {
    return NS_ERROR_NOT_INITIALIZED;
  }
  NS_ADDREF(*aResult = mMainThread);
  return NS_OK;
}

NS_IMETHODIMP
nsThreadManager::GetCurrentThread(nsIThread** aResult)
{
  // Keep this functioning during Shutdown
  if (NS_WARN_IF(!mMainThread)) {
    return NS_ERROR_NOT_INITIALIZED;
  }
  *aResult = GetCurrentThread();
  if (!*aResult) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  NS_ADDREF(*aResult);
  return NS_OK;
}

NS_IMETHODIMP
nsThreadManager::GetIsMainThread(bool* aResult)
{
  // This method may be called post-Shutdown

  *aResult = (PR_GetCurrentThread() == mMainPRThread);
  return NS_OK;
}

uint32_t
nsThreadManager::GetHighestNumberOfThreads()
{
  OffTheBooksMutexAutoLock lock(mLock);
  return mHighestNumberOfThreads;
}

NS_IMETHODIMP
nsThreadManager::SetIgnoreThreadStatus()
{
#ifdef MOZ_NUWA_PROCESS
  GetCurrentThreadStatusInfo()->mIgnored = true;
  return NS_OK;
#else
  return NS_ERROR_NOT_IMPLEMENTED;
#endif
}

#ifdef MOZ_NUWA_PROCESS
void
nsThreadManager::SetThreadIdle(nsIRunnable **aReturnRunnable)
{
  SetThreadIsWorking(GetCurrentThreadStatusInfo(), false, aReturnRunnable);
}

void
nsThreadManager::SetThreadWorking()
{
  SetThreadIsWorking(GetCurrentThreadStatusInfo(), true, nullptr);
}

void
nsThreadManager::SetThreadIsWorking(ThreadStatusInfo* aInfo,
                                    bool aIsWorking,
                                    nsIRunnable **aReturnRunnable)
{
  aInfo->mWillBeWorking = aIsWorking;
  if (mThreadsIdledListeners.Length() > 0) {

    // A race condition occurs since we don't want threads to try to enter the
    // monitor (nsThreadManager::mMonitor) when no one cares about their status.
    // And thus the race can happen when we put the first listener into
    // |mThreadsIdledListeners|:
    //
    // (1) Thread A wants to dispatch a task to Thread B.
    // (2) Thread A checks |mThreadsIdledListeners|, and nothing is in the
    //     list. So Thread A decides not to enter |mMonitor| when updating B's
    //     status.
    // (3) Thread A is suspended just before it changed status of B.
    // (4) A listener is added to |mThreadsIdledListeners|
    // (5) Now is Thread C's turn to run. Thread C finds there's something in
    //     |mThreadsIdledListeners|, so it enters |mMonitor| and check all
    //     thread info structs in |mThreadStatusInfos| while A is in the middle
    //     of changing B's status.
    //
    // Then C may find Thread B is an idle thread (which is not correct, because
    // A attempted to change B's status prior to C starting to walk throught
    // |mThreadStatusInfo|), but the fact that thread A is working (thread A
    // hasn't finished dispatching a task to thread B) can prevent thread C from
    // firing a bogus notification.
    //
    // If the state transition that happens outside the monitor is in the other
    // direction, the race condition could be:
    //
    // (1) Thread D has just finished its jobs and wants to set its status to idle.
    // (2) Thread D checks |mThreadsIdledListeners|, and nothing is in the list.
    //     So Thread D decides not to enter |mMonitor|.
    // (3) Thread D is is suspended before it updates its own status.
    // (4) A listener is put into |mThreadsIdledListeners|.
    // (5) Thread C wants to changes status of itself. It checks
    //     |mThreadsIdledListeners| and finds something inside the list. Thread C
    //     then enters |mMonitor|, updates its status and checks thread info in
    //     |mThreadStatusInfos| while D is changing status of itself out of monitor.
    //
    // Thread C will find that thread D is working (D actually wants to change its
    // status to idle before C starting to check), then C returns without firing
    // any notification. Finding that thread D is working can make our checking
    // mechanism miss a chance to fire a notification: because thread D thought
    // there's nothing in |mThreadsIdledListeners| and thus won't check the
    // |mThreadStatusInfos| after changing the status of itself.
    //
    // |mWillBeWorking| can be used to address this problem. We require each
    // thread to put the value that is going to be set to |mWorking| to
    // |mWillBeWorking| before the thread decide whether it should enter
    // |mMonitor| to change status or not. Thus C finds that D is working while
    // D's |mWillBeWorking| is false, and C realizes that D is just updating and
    // can treat D as an idle thread.
    //
    // It doesn't matter whether D will check thread status after changing its
    // own status or not. If D checks, which means D will enter the monitor
    // before updating status, thus D must be blocked until C has finished
    // dispatching the notification task to main thread, and D will find that main
    // thread is working and will not fire an additional event. On the other hand,
    // if D doesn't check |mThreadStatusInfos|, it's still ok, because C has
    // treated D as an idle thread already.

    bool hasWorkingThread = false;
    nsRefPtr<NotifyAllThreadsWereIdle> runnable;
    {
      ReentrantMonitorAutoEnter mon(*mMonitor);
      // Get data structure of thread info.
      aInfo->mWorking = aIsWorking;
      if (aIsWorking) {
        // We are working, so there's no need to check futher.
        return;
      }

      for (size_t i = 0; i < mThreadStatusInfos.Length(); i++) {
        ThreadStatusInfo *info = mThreadStatusInfos[i];
        if (!info->mIgnored) {
          if (info->mWorking) {
            if (info->mWillBeWorking) {
              hasWorkingThread = true;
              break;
            }
          }
        }
      }
      if (!hasWorkingThread && !mDispatchingToMainThread) {
        runnable = new NotifyAllThreadsWereIdle(&mThreadsIdledListeners);
        mDispatchingToMainThread = true;
      }
    }

    if (runnable) {
      if (NS_IsMainThread()) {
        // We are holding the main thread's |nsThread::mThreadStatusMonitor|.
        // If we dispatch a task to ourself, then we are in danger of causing
        // deadlock. Instead, return the task, and let the caller dispatch it
        // for us.
        MOZ_ASSERT(aReturnRunnable,
                   "aReturnRunnable must be provided on main thread");
        runnable.forget(aReturnRunnable);
      } else {
        NS_DispatchToMainThread(runnable);
        ResetIsDispatchingToMainThread();
      }
    }
  } else {
    // Update thread info without holding any lock.
    aInfo->mWorking = aIsWorking;
  }
}

void
nsThreadManager::ResetIsDispatchingToMainThread()
{
  ReentrantMonitorAutoEnter mon(*mMonitor);
  mDispatchingToMainThread = false;
}

void
nsThreadManager::AddAllThreadsWereIdleListener(AllThreadsWereIdleListener *listener)
{
  MOZ_ASSERT(GetCurrentThreadStatusInfo()->mWorking);
  mThreadsIdledListeners.AppendElement(listener);
}

void
nsThreadManager::RemoveAllThreadsWereIdleListener(AllThreadsWereIdleListener *listener)
{
  mThreadsIdledListeners.RemoveElement(listener);
}

#endif // MOZ_NUWA_PROCESS
