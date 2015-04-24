/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsThreadManager_h__
#define nsThreadManager_h__

#include "mozilla/Mutex.h"
#include "nsIThreadManager.h"
#include "nsRefPtrHashtable.h"
#include "nsThread.h"

class nsIRunnable;

namespace mozilla {
class ReentrantMonitor;
}

class nsThreadManager : public nsIThreadManager
{
public:
#ifdef MOZ_NUWA_PROCESS
  struct ThreadStatusInfo;
  class AllThreadsWereIdleListener {
  public:
    NS_INLINE_DECL_REFCOUNTING(AllThreadsWereIdleListener);
    virtual void OnAllThreadsWereIdle() = 0;
  protected:
    virtual ~AllThreadsWereIdleListener()
    {
    }
  };
#endif // MOZ_NUWA_PROCESS

  NS_DECL_ISUPPORTS
  NS_DECL_NSITHREADMANAGER

  static nsThreadManager* get()
  {
    static nsThreadManager sInstance;
    return &sInstance;
  }

  nsresult Init();

  // Shutdown all threads.  This function should only be called on the main
  // thread of the application process.
  void Shutdown();

  // Called by nsThread to inform the ThreadManager it exists.  This method
  // must be called when the given thread is the current thread.
  void RegisterCurrentThread(nsThread* aThread);

  // Called by nsThread to inform the ThreadManager it is going away.  This
  // method must be called when the given thread is the current thread.
  void UnregisterCurrentThread(nsThread* aThread);

  // Returns the current thread.  Returns null if OOM or if ThreadManager isn't
  // initialized.
  nsThread* GetCurrentThread();

  // Returns the maximal number of threads that have been in existence
  // simultaneously during the execution of the thread manager.
  uint32_t GetHighestNumberOfThreads();

  // This needs to be public in order to support static instantiation of this
  // class with older compilers (e.g., egcs-2.91.66).
  ~nsThreadManager()
  {
  }

#ifdef MOZ_NUWA_PROCESS
  // |SetThreadWorking| and |SetThreadIdle| set status of thread that is
  // currently running. They get thread status information from TLS and pass
  // the information to |SetThreadIsWorking|.
  void SetThreadIdle(nsIRunnable** aReturnRunnable);
  void SetThreadWorking();

  // |SetThreadIsWorking| is where is status actually changed. Thread status
  // information is passed as a argument so caller must obtain the structure
  // by itself. If this method is invoked on main thread, |aReturnRunnable|
  // should be provided to receive the runnable of notifying listeners.
  // |ResetIsDispatchingToMainThread| should be invoked after caller on main
  // thread dispatched the task to main thread's queue.
  void SetThreadIsWorking(ThreadStatusInfo* aInfo,
                          bool aIsWorking,
                          nsIRunnable** aReturnRunnable);
  void ResetIsDispatchingToMainThread();

  void AddAllThreadsWereIdleListener(AllThreadsWereIdleListener *listener);
  void RemoveAllThreadsWereIdleListener(AllThreadsWereIdleListener *listener);
  ThreadStatusInfo* GetCurrentThreadStatusInfo();
#endif // MOZ_NUWA_PROCESS

private:
  nsThreadManager()
    : mCurThreadIndex(0)
    , mMainPRThread(nullptr)
    , mLock("nsThreadManager.mLock")
    , mInitialized(false)
    , mCurrentNumberOfThreads(1)
    , mHighestNumberOfThreads(1)
#ifdef MOZ_NUWA_PROCESS
    , mMonitor(nullptr)
    , mMainThreadStatusInfo(nullptr)
    , mDispatchingToMainThread(nullptr)
#endif
  {
  }

  nsRefPtrHashtable<nsPtrHashKey<PRThread>, nsThread> mThreadsByPRThread;
  unsigned            mCurThreadIndex;  // thread-local-storage index
  nsRefPtr<nsThread>  mMainThread;
  PRThread*           mMainPRThread;
  mozilla::OffTheBooksMutex mLock;  // protects tables
  mozilla::Atomic<bool> mInitialized;

  // The current number of threads
  uint32_t            mCurrentNumberOfThreads;
  // The highest number of threads encountered so far during the session
  uint32_t            mHighestNumberOfThreads;

#ifdef MOZ_NUWA_PROCESS
  static void DeleteThreadStatusInfo(void *aData);
  unsigned mThreadStatusInfoIndex;
  nsTArray<nsRefPtr<AllThreadsWereIdleListener>> mThreadsIdledListeners;
  nsTArray<ThreadStatusInfo*> mThreadStatusInfos;
  mozilla::UniquePtr<mozilla::ReentrantMonitor> mMonitor;
  ThreadStatusInfo* mMainThreadStatusInfo;
  // |mDispatchingToMainThread| is set when all thread are found to be idle
  // before task of notifying all listeners are dispatched to main thread.
  // The flag is protected by |mMonitor|.
  bool mDispatchingToMainThread;
#endif // MOZ_NUWA_PROCESS
};

#define NS_THREADMANAGER_CID                       \
{ /* 7a4204c6-e45a-4c37-8ebb-6709a22c917c */       \
  0x7a4204c6,                                      \
  0xe45a,                                          \
  0x4c37,                                          \
  {0x8e, 0xbb, 0x67, 0x09, 0xa2, 0x2c, 0x91, 0x7c} \
}

#endif  // nsThreadManager_h__
