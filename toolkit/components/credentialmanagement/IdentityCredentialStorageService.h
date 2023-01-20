/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_IDENTITYCREDENTIALSTORAGESERVICE_H_
#define MOZILLA_IDENTITYCREDENTIALSTORAGESERVICE_H_

#include "ErrorList.h"
#include "mozilla/AlreadyAddRefed.h"
#include "mozilla/dom/FlippedOnce.h"
#include "mozilla/Monitor.h"
#include "mozIStorageConnection.h"
#include "nsIAsyncShutdown.h"
#include "nsIIdentityCredentialStorageService.h"
#include "nsIObserver.h"
#include "nsISupports.h"
#include "nsThreadUtils.h"

namespace mozilla {

class IdentityCredentialStorageService final
    : public nsIIdentityCredentialStorageService,
      public nsIObserver,
      public nsIAsyncShutdownBlocker {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIIDENTITYCREDENTIALSTORAGESERVICE
  NS_DECL_NSIOBSERVER
  NS_DECL_NSIASYNCSHUTDOWNBLOCKER

  // Returns the singleton instance. Used by the component manager
  static already_AddRefed<IdentityCredentialStorageService> GetSingleton();

  // Singletons shouldn't have copy constructors or assignment operators
  IdentityCredentialStorageService(const IdentityCredentialStorageService&) =
      delete;
  IdentityCredentialStorageService& operator=(
      const IdentityCredentialStorageService&) = delete;

 private:
  IdentityCredentialStorageService()
      : mMonitor("mozilla::IdentityCredentialStorageService::mMonitor"){};
  ~IdentityCredentialStorageService() = default;

  // Spins up the service. This includes firing off async work in a worker
  // thread. This should always be called before other use of the service to
  // prevent deadlock.
  nsresult Init();

  // Wait (non-blocking) until the service is fully initialized. We may be
  // waiting for that async work started by Init().
  nsresult WaitForInitialization();

  // Utility function to grab the correct barrier this service needs to shut
  // down by
  already_AddRefed<nsIAsyncShutdownClient> GetAsyncShutdownBarrier() const;

  // Called to indicate to the async shutdown service that we are all wrapped
  // up. This also spins down the worker thread, since it is called after all
  // disk database connections are closed.
  void Finalize();

  // Utility function to make sure a principal is an acceptable primary (RP)
  // principal
  static nsresult ValidatePrincipal(nsIPrincipal* aPrincipal);

  // Database connections. Guaranteed to be non-null and working once
  // initialized and not-yet finalized
  RefPtr<mozIStorageConnection> mDiskDatabaseConnection;  // Worker thread only
  RefPtr<mozIStorageConnection>
      mMemoryDatabaseConnection;  // Main thread only after initialization,
                                  // worker thread only before initialization.

  // Worker thread. This should be a valid thread after Init() returns and be
  // destroyed when we finalize
  nsCOMPtr<nsISerialEventTarget> mBackgroundThread;  // main thread only

  // The database file handle. We can only create this in the main thread and
  // need it in the worker to perform blocking disk IO. So we put it on this,
  // since we pass this to the worker anyway
  nsCOMPtr<nsIFile> mDatabaseFile;  // initialized in the main thread, read-only
                                    // in worker thread

  // Service state management. We protect these variables with a monitor. This
  // monitor is also used to signal the completion of initialization and
  // finalization performed in the worker thread.
  Monitor mMonitor;
  FlippedOnce<false> mInitialized MOZ_GUARDED_BY(mMonitor);
  FlippedOnce<false> mErrored MOZ_GUARDED_BY(mMonitor);
  FlippedOnce<false> mFinalized MOZ_GUARDED_BY(mMonitor);
};

}  // namespace mozilla

#endif /* MOZILLA_IDENTITYCREDENTIALSTORAGESERVICE_H_ */
