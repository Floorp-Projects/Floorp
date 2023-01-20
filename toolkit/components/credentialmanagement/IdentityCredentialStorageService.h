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
#include "mozilla/OriginAttributes.h"
#include "mozIStorageConnection.h"
#include "mozIStorageFunction.h"
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
      : mMonitor("mozilla::IdentityCredentialStorageService::mMonitor"),
        mPendingWrites(0){};
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

  // Helper functions to initialize the database connections. Also makes sure
  // the tables are present and have up to date schemas.
  nsresult GetMemoryDatabaseConnection();
  nsresult GetDiskDatabaseConnection();
  static nsresult GetDatabaseConnectionInternal(
      mozIStorageConnection** aDatabase, nsIFile* aFile);

  // Helper function for the Get*DatabaseConnection functions to ensure the
  // tables are present and have up to date schemas.
  static nsresult EnsureTable(mozIStorageConnection* aDatabase);

  // Grab all data from the disk database and insert it into the memory
  // database/ This is used at start up
  nsresult LoadMemoryTableFromDisk();

  // Used to (thread-safely) track how many operations have been launched to the
  // worker thread so that we can wait for it to hit zero before close the disk
  // database connection
  void IncrementPendingWrites();
  void DecrementPendingWrites();

  // Helper functions for database writes.
  //   We have helper functions here for all operations we perform on the
  //   databases that invlolve writes. These are split out and take a connection
  //   as an argument because we have to write to both the memory database and
  //   the disk database, where we only read from the memory database after
  //   initialization. See nsIIdentityCredentialStorageService.idl for more info
  //   on these functions' semantics

  // Upsert == Update or Insert! Uses some nice SQLite syntax to make this easy.
  static nsresult UpsertData(mozIStorageConnection* aDatabaseConnection,
                             nsIPrincipal* aRPPrincipal,
                             nsIPrincipal* aIDPPrincipal,
                             nsACString const& aCredentialID, bool aRegistered,
                             bool aAllowLogout);

  static nsresult DeleteData(mozIStorageConnection* aDatabaseConnection,
                             nsIPrincipal* aRPPrincipal,
                             nsIPrincipal* aIDPPrincipal,
                             nsACString const& aCredentialID);

  static nsresult ClearData(mozIStorageConnection* aDatabaseConnection);

  static nsresult DeleteDataFromOriginAttributesPattern(
      mozIStorageConnection* aDatabaseConnection,
      OriginAttributesPattern const& aOriginAttributesPattern);

  static nsresult DeleteDataFromTimeRange(
      mozIStorageConnection* aDatabaseConnection, int64_t aStart, int64_t aEnd);

  static nsresult DeleteDataFromPrincipal(
      mozIStorageConnection* aDatabaseConnection, nsIPrincipal* aPrincipal);

  static nsresult DeleteDataFromBaseDomain(
      mozIStorageConnection* aDatabaseConnection,
      nsACString const& aBaseDomain);

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
  FlippedOnce<false> mShuttingDown MOZ_GUARDED_BY(mMonitor);
  FlippedOnce<false> mFinalized MOZ_GUARDED_BY(mMonitor);
  uint32_t mPendingWrites MOZ_GUARDED_BY(mMonitor);
};

class OriginAttrsPatternMatchOriginSQLFunction final
    : public mozIStorageFunction {
  NS_DECL_ISUPPORTS
  NS_DECL_MOZISTORAGEFUNCTION

  explicit OriginAttrsPatternMatchOriginSQLFunction(
      OriginAttributesPattern const& aPattern)
      : mPattern(aPattern) {}
  OriginAttrsPatternMatchOriginSQLFunction() = delete;

 private:
  ~OriginAttrsPatternMatchOriginSQLFunction() = default;

  OriginAttributesPattern mPattern;
};

class PrivateBrowsingOriginSQLFunction final : public mozIStorageFunction {
  NS_DECL_ISUPPORTS
  NS_DECL_MOZISTORAGEFUNCTION

  PrivateBrowsingOriginSQLFunction() = default;

 private:
  ~PrivateBrowsingOriginSQLFunction() = default;
};

}  // namespace mozilla

#endif /* MOZILLA_IDENTITYCREDENTIALSTORAGESERVICE_H_ */
