/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_nativefilewatcher_h__
#define mozilla_nativefilewatcher_h__

#include "nsINativeFileWatcher.h"
#include "nsIObserver.h"
#include "nsCOMPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsThreadUtils.h"

// We need to include this header here for HANDLE definition.
#include <windows.h>

namespace mozilla {

class NativeFileWatcherService final : public nsINativeFileWatcherService,
                                       public nsIObserver {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSINATIVEFILEWATCHERSERVICE
  NS_DECL_NSIOBSERVER

  NativeFileWatcherService();

  nsresult Init();

 private:
  // The |HANDLE| to the I/O Completion Port, owned by the main thread.
  HANDLE mIOCompletionPort;
  nsCOMPtr<nsIThread> mIOThread;

  // The instance of the runnable dealing with the I/O.
  nsCOMPtr<nsIRunnable> mWorkerIORunnable;

  nsresult Uninit();
  void WakeUpWorkerThread();

  // Make the dtor private to make this object only deleted via its ::Release()
  // method.
  ~NativeFileWatcherService();
  NativeFileWatcherService(const NativeFileWatcherService& other) = delete;
  void operator=(const NativeFileWatcherService& other) = delete;
};

}  // namespace mozilla

#endif  // mozilla_nativefilewatcher_h__
