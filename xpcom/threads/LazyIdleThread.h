/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_lazyidlethread_h__
#define mozilla_lazyidlethread_h__

#ifndef MOZILLA_INTERNAL_API
#  error "This header is only usable from within libxul (MOZILLA_INTERNAL_API)."
#endif

#include "mozilla/TaskQueue.h"
#include "nsIObserver.h"
#include "nsThreadPool.h"

namespace mozilla {

/**
 * This class provides a basic event target that creates its thread lazily and
 * destroys its thread after a period of inactivity. It may be created and used
 * on any thread but it may only be shut down from the thread on which it is
 * created.
 */
class LazyIdleThread final : public nsISerialEventTarget, public nsIObserver {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIEVENTTARGET_FULL
  NS_DECL_NSIOBSERVER

  /**
   * If AutomaticShutdown is specified, and the LazyIdleThread is created on the
   * main thread, Shutdown() will automatically be called during
   * xpcom-shutdown-threads.
   */
  enum ShutdownMethod { AutomaticShutdown = 0, ManualShutdown };

  /**
   * Create a new LazyIdleThread that will destroy its thread after the given
   * number of milliseconds.
   */
  LazyIdleThread(uint32_t aIdleTimeoutMS, const char* aName,
                 ShutdownMethod aShutdownMethod = AutomaticShutdown);

  /**
   * Shuts down the LazyIdleThread, waiting for any pending work to complete.
   * Must be called from mOwningEventTarget.
   */
  void Shutdown();

  /**
   * Register a nsIThreadPoolListener on the underlying threadpool to track the
   * thread as it is created/destroyed.
   */
  nsresult SetListener(nsIThreadPoolListener* aListener);

 private:
  /**
   * Asynchronously shuts down the LazyIdleThread on mOwningEventTarget.
   */
  ~LazyIdleThread();

  /**
   * The thread which created this LazyIdleThread and is responsible for
   * shutting it down.
   */
  const nsCOMPtr<nsISerialEventTarget> mOwningEventTarget;

  /**
   * The single-thread backing threadpool which provides the actual threads used
   * by LazyIdleThread, and implements the timeout.
   */
  const RefPtr<nsThreadPool> mThreadPool;

  /**
   * The serial event target providing a `nsISerialEventTarget` implementation
   * when on the LazyIdleThread.
   */
  const RefPtr<TaskQueue> mTaskQueue;

  /**
   * Only safe to access on the owning thread or in the destructor (as no other
   * threads have access then). If `true`, means the LazyIdleThread has already
   * been shut down, so it does not need to be shut down asynchronously from the
   * destructor.
   */
  bool mShutdown = false;
};

}  // namespace mozilla

#endif  // mozilla_lazyidlethread_h__
