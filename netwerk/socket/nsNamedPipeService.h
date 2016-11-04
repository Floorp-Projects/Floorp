/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_netwerk_socket_nsNamedPipeService_h
#define mozilla_netwerk_socket_nsNamedPipeService_h

#include <windows.h>
#include "mozilla/Atomics.h"
#include "mozilla/Mutex.h"
#include "nsINamedPipeService.h"
#include "nsIObserver.h"
#include "nsIRunnable.h"
#include "nsTArray.h"

namespace mozilla {
namespace net {

class NamedPipeService final : public nsINamedPipeService
                             , public nsIObserver
                             , public nsIRunnable
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSINAMEDPIPESERVICE
  NS_DECL_NSIOBSERVER
  NS_DECL_NSIRUNNABLE

  explicit NamedPipeService();

  nsresult Init();

private:
  virtual ~NamedPipeService() = default;
  void Shutdown();
  void RemoveRetiredObjects();

  HANDLE mIocp; // native handle to the I/O completion port.
  Atomic<bool> mIsShutdown; // set to true to stop the event loop running by mThread.
  nsCOMPtr<nsIThread> mThread; // worker thread to get I/O events.

  /**
   * The observers is maintained in |mObservers| to ensure valid life-cycle.
   * We don't remove the handle and corresponding observer directly, instead
   * the handle and observer into a "retired" list and close/remove them in
   * the worker thread to avoid a race condition that might happen between
   * |CloseHandle()| and |GetQueuedCompletionStatus()|.
   */
  Mutex mLock;
  nsTArray<nsCOMPtr<nsINamedPipeDataObserver>> mObservers; // protected by mLock
  nsTArray<nsCOMPtr<nsINamedPipeDataObserver>> mRetiredObservers; // protected by mLock
  nsTArray<HANDLE> mRetiredHandles; // protected by mLock
};

} // namespace net
} // namespace mozilla

#endif // mozilla_netwerk_socket_nsNamedPipeService_h
