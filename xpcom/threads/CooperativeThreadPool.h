/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_CooperativeThreadPool_h
#define mozilla_CooperativeThreadPool_h

#include "mozilla/Array.h"
#include "mozilla/CondVar.h"
#include "mozilla/Mutex.h"
#include "mozilla/ThreadLocal.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/Variant.h"
#include "nsCOMPtr.h"
#include "nsThreadUtils.h"
#include "prthread.h"

// Windows silliness. winbase.h defines an empty no-argument Yield macro.
#undef Yield

class nsIEventTarget;

namespace mozilla {

// A CooperativeThreadPool is a pool of threads that process jobs, with at most
// one thread running at a given time. While processing a job, a thread can
// yield to another thread in the pool. Threads can be blocked on abstract
// Resources. When a thread yields, we iterate over all threads and look for
// one whose Resource has become available. It's possible that no thread is
// available to run, in which case all threads in the pool sleep. An outside
// thread can get things started again by calling RecheckBlockers (presumably
// after it has mutated some state that it expects would cause a Resource to
// become available).
class CooperativeThreadPool
{
public:
  // Every pool must have a controller object, on which callbacks are invoked.
  class Controller
  {
  public:
    // Called when a new thread in the pool is started. aIndex is the index of
    // the thread within the pool. aName is the thread name (e.g., Main#4), and
    // aStackTop is the guess for the address of the top of the stack. The
    // thread will begin processing events immediately after OnStartThread is called.
    virtual void OnStartThread(size_t aIndex, const nsACString& aName, void* aStackTop) = 0;

    // Called when a thread in the pool is about to be shut down.
    virtual void OnStopThread(size_t aIndex) = 0;

    // Threads in the pool are either suspended or running. At most one thread
    // will be running at any time. All other threads will be suspended.

    // Called when a thread is resumed (un-suspended). Note that OnResumeThread
    // will not be called if the thread is starting up. OnStartThread will be
    // called instead.
    virtual void OnResumeThread(size_t aIndex) = 0;

    // Called when a thread in the pool is about to be suspended. Note that
    // OnSuspendThread will not be called if the thread is shutting
    // down. OnStopThread is called instead.
    virtual void OnSuspendThread(size_t aIndex) = 0;
  };

  CooperativeThreadPool(size_t aNumThreads,
                        Mutex& aMutex,
                        Controller& aController);
  ~CooperativeThreadPool();

  void Shutdown();

  // An abstract class representing something that can be blocked on. Examples
  // are an event queue (where IsAvailable would check if the queue is
  // non-empty) or an object that can be owned by only one thread at a time
  // (where IsAvailable would check if the object is unowned).
  class Resource {
  public:
    virtual bool IsAvailable(const MutexAutoLock& aProofOfLock) = 0;
  };

  // Typically called by a thread outside the pool, this will check if any
  // thread is blocked on a resource that has become available. In this case,
  // the thread will resume. Nothing is done if some thread in the pool was
  // already running.
  void RecheckBlockers(const MutexAutoLock& aProofOfLock);

  // Must be called from within the thread pool. Causes the current thread to be
  // blocked on aBlocker, which can be null if the thread is ready to run
  // unconditionally.
  static void Yield(Resource* aBlocker, const MutexAutoLock& aProofOfLock);

  static bool IsCooperativeThread();

  enum class AllThreadsBlocked { Blocked };
  using SelectedThread = Variant<size_t, AllThreadsBlocked>;

  SelectedThread CurrentThreadIndex(const MutexAutoLock& aProofOfLock) const;

  static const size_t kMaxThreads = 16;

private:
  class CooperativeThread
  {
    friend class CooperativeThreadPool;

  public:
    CooperativeThread(CooperativeThreadPool* aPool,
                      size_t aIndex);

    void BeginShutdown();
    void EndShutdown();

    void Started();

    bool IsBlocked(const MutexAutoLock& aProofOfLock);
    void SetBlocker(Resource* aResource) { mBlocker = aResource; }

    void Yield(const MutexAutoLock& aProofOfLock);

  private:
    static void ThreadFunc(void* aArg);
    void ThreadMethod();

    CooperativeThreadPool* mPool;
    CondVar mCondVar;
    Resource* mBlocker;
    PRThread* mThread;
    nsCOMPtr<nsIEventTarget> mEventTarget;
    const size_t mIndex;
    bool mRunning;
  };

  class StartRunnable;

  Mutex& mMutex;
  CondVar mShutdownCondition;
  nsThreadPoolNaming mThreadNaming;

  bool mRunning;
  const size_t mNumThreads;
  size_t mRunningThreads;
  Controller& mController;
  Array<UniquePtr<CooperativeThread>, kMaxThreads> mThreads;

  SelectedThread mSelectedThread;

  static MOZ_THREAD_LOCAL(CooperativeThread*) sTlsCurrentThread;
};

} // namespace mozilla

#endif // mozilla_CooperativeThreadPool_h
