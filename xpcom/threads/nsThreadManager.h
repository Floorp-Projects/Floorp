/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsThreadManager_h__
#define nsThreadManager_h__

#include "nsIThreadManager.h"
#include "nsThread.h"
#include "mozilla/ShutdownPhase.h"

class nsIRunnable;
class nsIEventTarget;
class nsISerialEventTarget;
class nsIThread;

namespace mozilla {
class IdleTaskManager;
class SynchronizedEventQueue;
}  // namespace mozilla

class BackgroundEventTarget;

class nsThreadManager : public nsIThreadManager {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSITHREADMANAGER

  static nsThreadManager& get();

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

  // For each background TaskQueue cancel pending DelayedRunnables, and prohibit
  // creating future DelayedRunnables for them, since we'll soon be shutting
  // them down.
  // Pending DelayedRunnables are canceled on their respective TaskQueue.
  // We block main thread until they are all done, but spin the eventloop in the
  // meantime.
  void CancelBackgroundDelayedRunnables();

  ~nsThreadManager();

  void EnableMainThreadEventPrioritization();
  void FlushInputEventPrioritization();
  void SuspendInputEventPrioritization();
  void ResumeInputEventPrioritization();

  static bool MainThreadHasPendingHighPriorityEvents();

  nsIThread* GetMainThreadWeak() { return mMainThread; }

 private:
  nsThreadManager();

  nsresult SpinEventLoopUntilInternal(
      const nsACString& aVeryGoodReasonToDoThis,
      nsINestedEventLoopCondition* aCondition,
      mozilla::ShutdownPhase aCheckingShutdownPhase);

  static void ReleaseThread(void* aData);

  unsigned mCurThreadIndex;  // thread-local-storage index
  RefPtr<mozilla::IdleTaskManager> mIdleTaskManager;
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
