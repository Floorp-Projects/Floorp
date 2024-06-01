/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsThreadPool_h__
#define nsThreadPool_h__

#include "nsIThread.h"
#include "nsIThreadPool.h"
#include "nsIRunnable.h"
#include "nsCOMArray.h"
#include "nsCOMPtr.h"
#include "nsThreadUtils.h"
#include "mozilla/Atomics.h"
#include "mozilla/AlreadyAddRefed.h"
#include "mozilla/CondVar.h"
#include "mozilla/EventQueue.h"
#include "mozilla/LinkedList.h"
#include "mozilla/Mutex.h"

class nsIThread;

class nsThreadPool final : public mozilla::Runnable, public nsIThreadPool {
 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIEVENTTARGET_FULL
  NS_DECL_NSITHREADPOOL
  NS_DECL_NSIRUNNABLE

  nsThreadPool();

  static void InitTLS();
  static nsThreadPool* GetCurrentThreadPool();

 private:
  ~nsThreadPool();

  struct MRUIdleEntry;  // forward declaration only, see nsThreadPool.cpp

  void ShutdownThread(nsIThread* aThread);
  nsresult PutEvent(nsIRunnable* aEvent);
  nsresult PutEvent(already_AddRefed<nsIRunnable> aEvent, uint32_t aFlags);
  void NotifyChangeToAllIdleThreads() MOZ_REQUIRES(mMutex);

#ifdef DEBUG
  void DebugLogPoolStatus(mozilla::MutexAutoLock& aProofOfLock,
                          MRUIdleEntry* aWakingEntry = nullptr)
      MOZ_REQUIRES(mMutex);
#endif

  mozilla::Mutex mMutex;
  nsCOMArray<nsIThread> mThreads MOZ_GUARDED_BY(mMutex);
  mozilla::EventQueue mEvents MOZ_GUARDED_BY(mMutex);
  uint32_t mThreadLimit MOZ_GUARDED_BY(mMutex);
  uint32_t mIdleThreadLimit MOZ_GUARDED_BY(mMutex);
  mozilla::TimeDuration mIdleThreadGraceTimeout MOZ_GUARDED_BY(mMutex);
  mozilla::TimeDuration mIdleThreadMaxTimeout MOZ_GUARDED_BY(mMutex);
  mozilla::LinkedList<MRUIdleEntry> mMRUIdleThreads MOZ_GUARDED_BY(mMutex);
  nsIThread::QoSPriority mQoSPriority MOZ_GUARDED_BY(mMutex);
  uint32_t mStackSize MOZ_GUARDED_BY(mMutex);
  nsCOMPtr<nsIThreadPoolListener> mListener MOZ_GUARDED_BY(mMutex);
  mozilla::Atomic<bool, mozilla::Relaxed> mShutdown;
  mozilla::Atomic<bool, mozilla::Relaxed> mIsAPoolThreadFree;
  // set once before we start threads
  nsCString mName MOZ_GUARDED_BY(mMutex);
  nsThreadPoolNaming mThreadNaming;  // all data inside this is atomic
};

#define NS_THREADPOOL_CID                            \
  { /* 547ec2a8-315e-4ec4-888e-6e4264fe90eb */       \
    0x547ec2a8, 0x315e, 0x4ec4, {                    \
      0x88, 0x8e, 0x6e, 0x42, 0x64, 0xfe, 0x90, 0xeb \
    }                                                \
  }

#endif  // nsThreadPool_h__
