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
#include "MultiInstanceLock.h"

// The update sync manager is responsible for making sure that only one
// instance of the application is running at the time we want to start updating
// it. It does this by taking a multi-instance lock very early during the
// application's startup process. Then, when app update tasks are ready to run,
// the update service asks us whether anything else has also taken that lock,
// which, if true, would mean another instance of the application is currently
// running and performing update tasks should be avoided (the update service
// also runs a timeout and eventually goes ahead with the update in order to
// prevent an external program from effectively disabling updates).
class nsUpdateSyncManager final : public nsIUpdateSyncManager,
                                  public nsIObserver {
 public:
  explicit nsUpdateSyncManager(nsIFile* anAppFile = nullptr);

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

  nsresult OpenLock(nsIFile* anAppFile = nullptr);
  void ReleaseLock();

  mozilla::MultiInstLockHandle mLock = MULTI_INSTANCE_LOCK_HANDLE_ERROR;
};

#endif  // nsUpdateSyncManager_h__
