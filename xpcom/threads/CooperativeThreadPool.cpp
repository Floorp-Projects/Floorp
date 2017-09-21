/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CooperativeThreadPool.h"

#include "base/message_loop.h"
#include "mozilla/IOInterposer.h"
#include "mozilla/ServoBindings.h"
#include "nsError.h"
#include "nsThreadUtils.h"

using namespace mozilla;

static bool gCooperativeSchedulingEnabled;
MOZ_THREAD_LOCAL(CooperativeThreadPool::CooperativeThread*) CooperativeThreadPool::sTlsCurrentThread;

// Windows silliness. winbase.h defines an empty no-argument Yield macro.
#undef Yield

CooperativeThreadPool::CooperativeThreadPool(size_t aNumThreads,
                                             Mutex& aMutex,
                                             Controller& aController)
  : mMutex(aMutex)
  , mShutdownCondition(mMutex, "CoopShutdown")
  , mRunning(false)
  , mNumThreads(std::min(aNumThreads, kMaxThreads))
  , mRunningThreads(0)
  , mController(aController)
  , mSelectedThread(size_t(0))
{
  MOZ_ASSERT(aNumThreads < kMaxThreads);

  gCooperativeSchedulingEnabled = true;
  sTlsCurrentThread.infallibleInit();

  MutexAutoLock lock(mMutex);

  mRunning = true;
  mRunningThreads = mNumThreads;

  for (size_t i = 0; i < mNumThreads; i++) {
    mThreads[i] = MakeUnique<CooperativeThread>(this, i);
  }
}

CooperativeThreadPool::~CooperativeThreadPool()
{
  MOZ_ASSERT(!mRunning);
}

const size_t CooperativeThreadPool::kMaxThreads;

void
CooperativeThreadPool::Shutdown()
{
  // This will not be called on any of the cooperative threads.
  {
    MutexAutoLock lock(mMutex);
    MOZ_ASSERT(mRunning);
    mRunning = false;
  }

  for (size_t i = 0; i < mNumThreads; i++) {
    mThreads[i]->BeginShutdown();
  }

  {
    MutexAutoLock lock(mMutex);
    while (mRunningThreads) {
      mShutdownCondition.Wait();
    }
  }

  for (size_t i = 0; i < mNumThreads; i++) {
    mThreads[i]->EndShutdown();
  }
}

void
CooperativeThreadPool::RecheckBlockers(const MutexAutoLock& aProofOfLock)
{
  aProofOfLock.AssertOwns(mMutex);

  if (!mSelectedThread.is<AllThreadsBlocked>()) {
    return;
  }

  for (size_t i = 0; i < mNumThreads; i++) {
    if (mThreads[i]->mRunning && !mThreads[i]->IsBlocked(aProofOfLock)) {
      mSelectedThread = AsVariant(i);
      mThreads[i]->mCondVar.Notify();
      return;
    }
  }

  // It may be valid to reach this point. For example, if we are waiting for an
  // event to be posted from a non-main thread. Even if the queue is non-empty,
  // it may have only idle events that we do not want to run (because we are
  // expecting a vsync soon).
}

/* static */ void
CooperativeThreadPool::Yield(Resource* aBlocker, const MutexAutoLock& aProofOfLock)
{
  if (!gCooperativeSchedulingEnabled) {
    return;
  }

  CooperativeThread* thread = sTlsCurrentThread.get();
  MOZ_RELEASE_ASSERT(thread);
  thread->SetBlocker(aBlocker);
  thread->Yield(aProofOfLock);
}

/* static */ bool
CooperativeThreadPool::IsCooperativeThread()
{
  if (!gCooperativeSchedulingEnabled) {
    return false;
  }
  return !!sTlsCurrentThread.get();
}

CooperativeThreadPool::SelectedThread
CooperativeThreadPool::CurrentThreadIndex(const MutexAutoLock& aProofOfLock) const
{
  aProofOfLock.AssertOwns(mMutex);
  return mSelectedThread;
}

CooperativeThreadPool::CooperativeThread::CooperativeThread(CooperativeThreadPool* aPool,
                                                            size_t aIndex)
  : mPool(aPool)
  , mCondVar(aPool->mMutex, "CooperativeThreadPool")
  , mBlocker(nullptr)
  , mIndex(aIndex)
  , mRunning(true)
{
  mThread = PR_CreateThread(PR_USER_THREAD, ThreadFunc, this,
                            PR_PRIORITY_NORMAL, PR_GLOBAL_THREAD,
                            PR_JOINABLE_THREAD, 0);
  MOZ_RELEASE_ASSERT(mThread);
}

void
CooperativeThreadPool::CooperativeThread::ThreadMethod()
{
  char stackTop;

  MOZ_ASSERT(gCooperativeSchedulingEnabled);
  sTlsCurrentThread.set(this);

  nsCString name = mPool->mThreadNaming.GetNextThreadName("Main");
  PR_SetCurrentThreadName(name.get());

  mozilla::IOInterposer::RegisterCurrentThread();

  {
    // Make sure only one thread at a time can proceed. This only happens during
    // thread startup.
    MutexAutoLock lock(mPool->mMutex);
    while (mPool->mSelectedThread != AsVariant(mIndex)) {
      mCondVar.Wait();
    }
  }

  mPool->mController.OnStartThread(mIndex, name, &stackTop);

  nsCOMPtr<nsIThread> thread = do_GetCurrentThread();
  mEventTarget = thread;

  // The main event loop for this thread.
  for (;;) {
    {
      MutexAutoLock lock(mPool->mMutex);
      if (!mPool->mRunning) {
        break;
      }
    }

    bool processedEvent;
    thread->ProcessNextEvent(true, &processedEvent);
  }

  mPool->mController.OnStopThread(mIndex);
  mozilla::IOInterposer::UnregisterCurrentThread();

  MutexAutoLock lock(mPool->mMutex);
  mPool->mRunningThreads--;
  mRunning = false;
  mPool->mSelectedThread = AsVariant(AllThreadsBlocked::Blocked);
  mPool->RecheckBlockers(lock);
  mPool->mShutdownCondition.Notify();
}

/* static */ void
CooperativeThreadPool::CooperativeThread::ThreadFunc(void* aArg)
{
  auto thread = static_cast<CooperativeThreadPool::CooperativeThread*>(aArg);
  thread->ThreadMethod();
}

void
CooperativeThreadPool::CooperativeThread::BeginShutdown()
{
  mEventTarget->Dispatch(new mozilla::Runnable("CooperativeShutdownEvent"),
                         nsIEventTarget::DISPATCH_NORMAL);
}

void
CooperativeThreadPool::CooperativeThread::EndShutdown()
{
  PR_JoinThread(mThread);
}

bool
CooperativeThreadPool::CooperativeThread::IsBlocked(const MutexAutoLock& aProofOfLock)
{
  if (!mBlocker) {
    return false;
  }

  return !mBlocker->IsAvailable(aProofOfLock);
}

void
CooperativeThreadPool::CooperativeThread::Yield(const MutexAutoLock& aProofOfLock)
{
  aProofOfLock.AssertOwns(mPool->mMutex);

  // First select the next thread to run.
  size_t selected = mIndex + 1;
  bool found = false;
  do {
    if (selected >= mPool->mNumThreads) {
      selected = 0;
    }

    if (mPool->mThreads[selected]->mRunning
        && !mPool->mThreads[selected]->IsBlocked(aProofOfLock)) {
      found = true;
      break;
    }
    selected++;
  } while (selected != mIndex + 1);

  if (found) {
    mPool->mSelectedThread = AsVariant(selected);
    mPool->mThreads[selected]->mCondVar.Notify();
  } else {
    // We need to block all threads. Some thread will be unblocked when
    // RecheckBlockers is called (if a new event is posted for an outside
    // thread, for example).
    mPool->mSelectedThread = AsVariant(AllThreadsBlocked::Blocked);
  }

  mPool->mController.OnSuspendThread(mIndex);

  while (mPool->mSelectedThread != AsVariant(mIndex)) {
    mCondVar.Wait();
  }

  mPool->mController.OnResumeThread(mIndex);
}
