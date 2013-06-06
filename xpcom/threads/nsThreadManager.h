/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
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

class nsThreadManager : public nsIThreadManager
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSITHREADMANAGER

  static nsThreadManager *get() {
    static nsThreadManager sInstance;
    return &sInstance;
  }

  nsresult Init();

  // Shutdown all threads.  This function should only be called on the main
  // thread of the application process.
  void Shutdown();

  // Called by nsThread to inform the ThreadManager it exists.  This method
  // must be called when the given thread is the current thread.
  void RegisterCurrentThread(nsThread *thread);

  // Called by nsThread to inform the ThreadManager it is going away.  This
  // method must be called when the given thread is the current thread.
  void UnregisterCurrentThread(nsThread *thread);

  // Returns the current thread.  Returns null if OOM or if ThreadManager isn't
  // initialized.
  nsThread *GetCurrentThread();

  // Returns the maximal number of threads that have been in existence
  // simultaneously during the execution of the thread manager.
  uint32_t GetHighestNumberOfThreads();

  // This needs to be public in order to support static instantiation of this
  // class with older compilers (e.g., egcs-2.91.66).
  ~nsThreadManager() {}

private:
  nsThreadManager()
    : mCurThreadIndex(0)
    , mMainPRThread(nullptr)
    , mLock(nullptr)
    , mInitialized(false)
    , mCurrentNumberOfThreads(1)
    , mHighestNumberOfThreads(1) {
  }

  nsRefPtrHashtable<nsPtrHashKey<PRThread>, nsThread> mThreadsByPRThread;
  unsigned             mCurThreadIndex;  // thread-local-storage index
  nsRefPtr<nsThread>  mMainThread;
  PRThread           *mMainPRThread;
  // This is a pointer in order to allow creating nsThreadManager from
  // the static context in debug builds.
  nsAutoPtr<mozilla::Mutex> mLock;  // protects tables
  bool                mInitialized;

   // The current number of threads
   uint32_t           mCurrentNumberOfThreads;
   // The highest number of threads encountered so far during the session
   uint32_t           mHighestNumberOfThreads;
};

#define NS_THREADMANAGER_CID                       \
{ /* 7a4204c6-e45a-4c37-8ebb-6709a22c917c */       \
  0x7a4204c6,                                      \
  0xe45a,                                          \
  0x4c37,                                          \
  {0x8e, 0xbb, 0x67, 0x09, 0xa2, 0x2c, 0x91, 0x7c} \
}

#endif  // nsThreadManager_h__
