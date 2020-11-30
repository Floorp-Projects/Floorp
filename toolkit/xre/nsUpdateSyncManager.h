/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsUpdateSyncManager_h__
#define nsUpdateSyncManager_h__

#include "mozilla/AlreadyAddRefed.h"
#include "nsIObserver.h"
#include "nsIUpdateService.h"
#include "GlobalSemaphore.h"

// The update sync manager is responsible for making sure that only one
// instance of the application is running at the time we want to start updating
// it. It does this by taking a semaphore very early during the application's
// startup process. Then, when app update tasks are ready to run, the update
// service asks us whether anything else has also taken the semaphore, which,
// if true, would mean another instance of the application is currently running
// and performing update tasks should be avoided (the update service also runs
// a timeout and eventually goes ahead with the update in order to prevent an
// external program from effectively disabling updates).
// The immediately obvious tool for this job would be a mutex and not a
// semaphore, since only one instance can be applying updates at a time, but at
// it turns out that wouldn't quite meet the requirements. Consider this
// scenario: an application instance we'll call instance A runs and takes the
// mutex. It doesn't check for updates right away. A second instance called B
// then starts and cannot get the mutex during its startup because instance A
// still holds it. A third instance C is started and has the same problem. Now,
// what if instance A exits? It returns the mutex, so if either B or C decide to
// check for updates they'll be able to take it. But neither is aware of the
// other's existence, so whichever one wins that race will be able to apply an
// update behind the other one's back, which is the exact thing this component
// is intended to prevent. By using a semaphore instead, every instance is
// always aware of how many other instances are running by checking the
// semaphore's count, and this problem is avoided.
class nsUpdateSyncManager final : public nsIUpdateSyncManager,
                                  public nsIObserver {
 public:
  nsUpdateSyncManager();

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIUPDATESYNCMANAGER
  NS_DECL_NSIOBSERVER

  static already_AddRefed<nsUpdateSyncManager> GetSingleton();

 private:
  ~nsUpdateSyncManager();

  nsUpdateSyncManager(nsUpdateSyncManager&) = delete;
  nsUpdateSyncManager(nsUpdateSyncManager&&) = delete;
  nsUpdateSyncManager& operator=(nsUpdateSyncManager&) = delete;
  nsUpdateSyncManager& operator=(nsUpdateSyncManager&&) = delete;

  nsresult OpenSemaphore();
  void ReleaseSemaphore();

  mozilla::GlobalSemHandle mSemaphore = nullptr;
};

#endif  // nsUpdateSyncManager_h__
