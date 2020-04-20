/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsThreadManager_h__
#define nsThreadManager_h__

#include "mozilla/Mutex.h"
#include "nsIThreadManager.h"
#include "nsThread.h"

class nsIRunnable;

class BackgroundEventTarget;

class nsThreadManager : public nsIThreadManager {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSITHREADMANAGER

  static nsThreadManager& get();

  static void InitializeShutdownObserver();

  nsresult Init();

  // Shutdown all threads.  This function should only be called on the main
  // thread of the application process.
  void Shutdown();

  // Called by nsThread to inform the ThreadManager it exists.  This method
  // must be called when the given thread is the current thread.
  void RegisterCurrentThread(nsThread& aThread);

  // Called by nsThread to inform the ThreadManager it is going away.  This
  // method must be called when the given thread is the current thread.
  void UnregisterCurrentThread(nsThread& aThread);

  // Returns the current thread.  Returns null if OOM or if ThreadManager isn't
  // initialized.  Creates the nsThread if one does not exist yet.
  nsThread* GetCurrentThread();

  // Returns true iff the currently running thread has an nsThread associated
  // with it (ie; whether this is a thread that we can dispatch runnables to).
  bool IsNSThread() const;

  // CreateCurrentThread sets up an nsThread for the current thread. It uses the
  // event queue and main thread flags passed in. It should only be called once
  // for the current thread. After it returns, GetCurrentThread() will return
  // the thread that was created. GetCurrentThread() will also create a thread
  // (lazily), but it doesn't allow the queue or main-thread attributes to be
  // specified.
  nsThread* CreateCurrentThread(mozilla::SynchronizedEventQueue* aQueue,
                                nsThread::MainThreadFlag aMainThread);

  nsresult DispatchToBackgroundThread(nsIRunnable* aEvent,
                                      uint32_t aDispatchFlags);

  already_AddRefed<nsISerialEventTarget> CreateBackgroundTaskQueue(
      const char* aName);

  // Returns the maximal number of threads that have been in existence
  // simultaneously during the execution of the thread manager.
  uint32_t GetHighestNumberOfThreads();

  ~nsThreadManager();

  void EnableMainThreadEventPrioritization();
  void FlushInputEventPrioritization();
  void SuspendInputEventPrioritization();
  void ResumeInputEventPrioritization();

  static bool MainThreadHasPendingHighPriorityEvents();

  nsIThread* GetMainThreadWeak() { return mMainThread; }

 private:
  nsThreadManager();

  nsresult SpinEventLoopUntilInternal(nsINestedEventLoopCondition* aCondition,
                                      bool aCheckingShutdown);

  static void ReleaseThread(void* aData);

  unsigned mCurThreadIndex;  // thread-local-storage index
  RefPtr<nsThread> mMainThread;
  PRThread* mMainPRThread;
  mozilla::Atomic<bool, mozilla::SequentiallyConsistent> mInitialized;

  // Shared event target used for background runnables.
  RefPtr<BackgroundEventTarget> mBackgroundEventTarget;
};

#define NS_THREADMANAGER_CID                         \
  { /* 7a4204c6-e45a-4c37-8ebb-6709a22c917c */       \
    0x7a4204c6, 0xe45a, 0x4c37, {                    \
      0x8e, 0xbb, 0x67, 0x09, 0xa2, 0x2c, 0x91, 0x7c \
    }                                                \
  }

#endif  // nsThreadManager_h__
