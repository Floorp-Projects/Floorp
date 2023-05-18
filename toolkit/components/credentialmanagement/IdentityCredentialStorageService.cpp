/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ErrorList.h"
#include "IdentityCredentialStorageService.h"
#include "MainThreadUtils.h"
#include "mozilla/AppShutdown.h"
#include "mozilla/Base64.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/Components.h"
#include "mozilla/OriginAttributes.h"
#include "mozilla/Services.h"
#include "mozilla/StaticPrefs_dom.h"
#include "mozilla/StaticPtr.h"
#include "mozIStorageService.h"
#include "mozIStorageConnection.h"
#include "mozIStorageStatement.h"
#include "mozStorageCID.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsComponentManagerUtils.h"
#include "nsCRT.h"
#include "nsDebug.h"
#include "nsDirectoryServiceUtils.h"
#include "nsIObserverService.h"
#include "nsIWritablePropertyBag2.h"
#include "nsServiceManagerUtils.h"
#include "nsThreadUtils.h"
#include "nsVariant.h"
#include "prtime.h"

#define ACCOUNT_STATE_FILENAME "credentialstate.sqlite"_ns
#define SCHEMA_VERSION 1
#define MODIFIED_NOW PR_Now()

namespace mozilla {

StaticRefPtr<IdentityCredentialStorageService>
    gIdentityCredentialStorageService;

NS_IMPL_ISUPPORTS(IdentityCredentialStorageService,
                  nsIIdentityCredentialStorageService, nsIObserver,
                  nsIAsyncShutdownBlocker)

already_AddRefed<IdentityCredentialStorageService>
IdentityCredentialStorageService::GetSingleton() {
  AssertIsOnMainThread();
  MOZ_ASSERT(XRE_IsParentProcess());
  if (!gIdentityCredentialStorageService) {
    gIdentityCredentialStorageService = new IdentityCredentialStorageService();
    ClearOnShutdown(&gIdentityCredentialStorageService);
    nsresult rv = gIdentityCredentialStorageService->Init();
    NS_ENSURE_SUCCESS(rv, nullptr);
  }
  RefPtr<IdentityCredentialStorageService> service =
      gIdentityCredentialStorageService;
  return service.forget();
}

NS_IMETHODIMP IdentityCredentialStorageService::GetName(nsAString& aName) {
  aName = u"IdentityCredentialStorageService: Flushing data"_ns;
  return NS_OK;
}

NS_IMETHODIMP IdentityCredentialStorageService::BlockShutdown(
    nsIAsyncShutdownClient* aClient) {
  MOZ_ASSERT(NS_IsMainThread());
  nsresult rv = WaitForInitialization();
  NS_ENSURE_SUCCESS(rv, rv);

  MonitorAutoLock lock(mMonitor);
  mShuttingDown.Flip();

  if (mMemoryDatabaseConnection) {
    Unused << mMemoryDatabaseConnection->Close();
    mMemoryDatabaseConnection = nullptr;
  }

  RefPtr<IdentityCredentialStorageService> self = this;
  mBackgroundThread->Dispatch(
      NS_NewRunnableFunction(
          "IdentityCredentialStorageService::BlockShutdown",
          [self]() {
            MonitorAutoLock lock(self->mMonitor);

            MOZ_ASSERT(self->mPendingWrites == 0);

            if (self->mDiskDatabaseConnection) {
              Unused << self->mDiskDatabaseConnection->Close();
              self->mDiskDatabaseConnection = nullptr;
            }

            self->mFinalized.Flip();
            self->mMonitor.NotifyAll();
            NS_DispatchToMainThread(NS_NewRunnableFunction(
                "IdentityCredentialStorageService::BlockShutdown "
                "- mainthread callback",
                [self]() { self->Finalize(); }));
          }),
      NS_DISPATCH_EVENT_MAY_BLOCK);

  return NS_OK;
}

NS_IMETHODIMP
IdentityCredentialStorageService::GetState(nsIPropertyBag** aBagOut) {
  return NS_OK;
}

already_AddRefed<nsIAsyncShutdownClient>
IdentityCredentialStorageService::GetAsyncShutdownBarrier() const {
  nsresult rv;
  nsCOMPtr<nsIAsyncShutdownService> svc = components::AsyncShutdown::Service();
  MOZ_RELEASE_ASSERT(svc);

  nsCOMPtr<nsIAsyncShutdownClient> client;
  rv = svc->GetProfileBeforeChange(getter_AddRefs(client));
  MOZ_RELEASE_ASSERT(NS_SUCCEEDED(rv));
  MOZ_RELEASE_ASSERT(client);
  return client.forget();
}

nsresult IdentityCredentialStorageService::Init() {
  AssertIsOnMainThread();

  if (AppShutdown::IsInOrBeyond(ShutdownPhase::AppShutdownConfirmed)) {
    MonitorAutoLock lock(mMonitor);
    mShuttingDown.Flip();
    return NS_ERROR_ILLEGAL_DURING_SHUTDOWN;
  }

  nsCOMPtr<nsIAsyncShutdownClient> asc = GetAsyncShutdownBarrier();
  if (!asc) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  nsresult rv = asc->AddBlocker(this, NS_LITERAL_STRING_FROM_CSTRING(__FILE__),
                                __LINE__, u""_ns);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR,
                              getter_AddRefs(mDatabaseFile));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = mDatabaseFile->AppendNative(ACCOUNT_STATE_FILENAME);
  NS_ENSURE_SUCCESS(rv, rv);

  // Register the PBMode cleaner (IdentityCredentialStorageService::Observe) as
  // an observer.
  nsCOMPtr<nsIObserverService> observerService =
      mozilla::services::GetObserverService();
  NS_ENSURE_TRUE(observerService, NS_ERROR_FAILURE);
  observerService->AddObserver(this, "last-pb-context-exited", false);

  rv = GetMemoryDatabaseConnection();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    MonitorAutoLock lock(mMonitor);
    mErrored.Flip();
    return rv;
  }

  NS_ENSURE_SUCCESS(
      NS_CreateBackgroundTaskQueue("IdentityCredentialStorage",
                                   getter_AddRefs(mBackgroundThread)),
      NS_ERROR_FAILURE);

  RefPtr<IdentityCredentialStorageService> self = this;

  mBackgroundThread->Dispatch(
      NS_NewRunnableFunction("IdentityCredentialStorageService::Init",
                             [self]() {
                               MonitorAutoLock lock(self->mMonitor);
                               nsresult rv = self->GetDiskDatabaseConnection();
                               if (NS_WARN_IF(NS_FAILED(rv))) {
                                 self->mErrored.Flip();
                                 self->mMonitor.Notify();
                                 return;
                               }

                               rv = self->LoadMemoryTableFromDisk();
                               if (NS_WARN_IF(NS_FAILED(rv))) {
                                 self->mErrored.Flip();
                                 self->mMonitor.Notify();
                                 return;
                               }

                               self->mInitialized.Flip();
                               self->mMonitor.Notify();
                             }),
      NS_DISPATCH_EVENT_MAY_BLOCK);

  return NS_OK;
}

nsresult IdentityCredentialStorageService::WaitForInitialization() {
  MOZ_ASSERT(NS_IsMainThread(),
             "Must only wait for initialization in the main thread.");
  MonitorAutoLock lock(mMonitor);
  while (!mInitialized && !mErrored && !mShuttingDown) {
    mMonitor.Wait();
  }
  if (mErrored) {
    return NS_ERROR_FAILURE;
  }
  if (mShuttingDown) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  return NS_OK;
}

void IdentityCredentialStorageService::Finalize() {
  nsCOMPtr<nsIAsyncShutdownClient> asc = GetAsyncShutdownBarrier();
  MOZ_ASSERT(asc);
  DebugOnly<nsresult> rv = asc->RemoveBlocker(this);
  MOZ_ASSERT(NS_SUCCEEDED(rv));
}

// static
nsresult IdentityCredentialStorageService::ValidatePrincipal(
    nsIPrincipal* aPrincipal) {
  // We add some constraints on the RP principal where it is provided to reduce
  // edge cases in implementation. These are reasonable constraints with the
  // semantics of the store: it must be a http or https content principal.
  NS_ENSURE_ARG_POINTER(aPrincipal);
  NS_ENSURE_TRUE(aPrincipal->GetIsContentPrincipal(), NS_ERROR_FAILURE);
  nsCString scheme;
  nsresult rv = aPrincipal->GetScheme(scheme);
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(scheme.Equals("http"_ns) || scheme.Equals("https"_ns),
                 NS_ERROR_FAILURE);
  return NS_OK;
}

nsresult IdentityCredentialStorageService::GetMemoryDatabaseConnection() {
  return IdentityCredentialStorageService::GetDatabaseConnectionInternal(
      getter_AddRefs(mMemoryDatabaseConnection), nullptr);
}

nsresult IdentityCredentialStorageService::GetDiskDatabaseConnection() {
  NS_ENSURE_TRUE(mDatabaseFile, NS_ERROR_NULL_POINTER);
  return IdentityCredentialStorageService::GetDatabaseConnectionInternal(
      getter_AddRefs(mDiskDatabaseConnection), mDatabaseFile);
}

// static
nsresult IdentityCredentialStorageService::GetDatabaseConnectionInternal(
    mozIStorageConnection** aDatabase, nsIFile* aFile) {
  NS_ENSURE_TRUE(aDatabase, NS_ERROR_UNEXPECTED);
  NS_ENSURE_STATE(!(*aDatabase));
  nsCOMPtr<mozIStorageService> storage =
      do_GetService(MOZ_STORAGE_SERVICE_CONTRACTID);
  NS_ENSURE_TRUE(storage, NS_ERROR_UNEXPECTED);
  nsresult rv;

  if (aFile) {
    rv = storage->OpenDatabase(aFile, mozIStorageService::CONNECTION_DEFAULT,
                               aDatabase);
    if (rv == NS_ERROR_FILE_CORRUPTED) {
      rv = aFile->Remove(false);
      NS_ENSURE_SUCCESS(rv, rv);
      rv = storage->OpenDatabase(aFile, mozIStorageService::CONNECTION_DEFAULT,
                                 aDatabase);
    }
    NS_ENSURE_SUCCESS(rv, rv);
  } else {
    rv = storage->OpenSpecialDatabase(
        kMozStorageMemoryStorageKey, "icsprivatedb"_ns,
        mozIStorageService::CONNECTION_DEFAULT, aDatabase);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  NS_ENSURE_TRUE(*aDatabase, NS_ERROR_UNEXPECTED);
  bool ready = false;
  (*aDatabase)->GetConnectionReady(&ready);
  NS_ENSURE_TRUE(ready, NS_ERROR_UNEXPECTED);
  rv = EnsureTable(*aDatabase);
  NS_ENSURE_SUCCESS(rv, rv);
  return NS_OK;
}

// static
nsresult IdentityCredentialStorageService::EnsureTable(
    mozIStorageConnection* aDatabase) {
  NS_ENSURE_ARG_POINTER(aDatabase);
  bool tableExists = false;
  aDatabase->TableExists("identity"_ns, &tableExists);
  if (!tableExists) {
    // Currently there is only one schema version, so we just need to create the
    // table. The definition uses no explicit rowid column, instead primary
    // keying on the tuple defined in the spec. We store two bits and some
    // additional data to make integration with the ClearDataService
    // easier/possible.
    nsresult rv = aDatabase->SetSchemaVersion(SCHEMA_VERSION);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = aDatabase->ExecuteSimpleSQL(
        "CREATE TABLE identity ("
        "rpOrigin TEXT NOT NULL"
        ",idpOrigin TEXT NOT NULL"
        ",credentialId TEXT NOT NULL"
        ",registered INTEGER"
        ",allowLogout INTEGER"
        ",modificationTime INTEGER"
        ",rpBaseDomain TEXT"
        ",PRIMARY KEY (rpOrigin, idpOrigin, credentialId)"
        ")"_ns);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  return NS_OK;
}

nsresult IdentityCredentialStorageService::LoadMemoryTableFromDisk() {
  MOZ_ASSERT(!NS_IsMainThread(),
             "Must not load the table from disk in the main thread.");
  auto constexpr selectAllQuery =
      "SELECT rpOrigin, idpOrigin, credentialId, registered, allowLogout, "
      "modificationTime, rpBaseDomain FROM identity;"_ns;
  auto constexpr insertQuery =
      "INSERT INTO identity(rpOrigin, idpOrigin, credentialId, registered, "
      "allowLogout, modificationTime, rpBaseDomain) VALUES (?1, ?2, ?3, ?4, "
      "?5, ?6, ?7);"_ns;

  nsCOMPtr<mozIStorageStatement> writeStmt;
  nsresult rv = mMemoryDatabaseConnection->CreateStatement(
      insertQuery, getter_AddRefs(writeStmt));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<mozIStorageStatement> readStmt;
  rv = mDiskDatabaseConnection->CreateStatement(selectAllQuery,
                                                getter_AddRefs(readStmt));
  NS_ENSURE_SUCCESS(rv, rv);

  bool hasResult;
  while (NS_SUCCEEDED(readStmt->ExecuteStep(&hasResult)) && hasResult) {
    int64_t registered, allowLogout, modificationTime;
    nsCString rpOrigin, idpOrigin, credentialID, rpBaseDomain;

    // Read values from disk query
    rv = readStmt->GetUTF8String(0, rpOrigin);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = readStmt->GetUTF8String(1, idpOrigin);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = readStmt->GetUTF8String(2, credentialID);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = readStmt->GetInt64(3, &registered);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = readStmt->GetInt64(4, &allowLogout);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = readStmt->GetInt64(5, &modificationTime);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = readStmt->GetUTF8String(6, rpBaseDomain);
    NS_ENSURE_SUCCESS(rv, rv);

    // Write values to memory database
    rv = writeStmt->BindUTF8StringByIndex(0, rpOrigin);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = writeStmt->BindUTF8StringByIndex(1, idpOrigin);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = writeStmt->BindUTF8StringByIndex(2, credentialID);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = writeStmt->BindInt64ByIndex(3, registered);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = writeStmt->BindInt64ByIndex(4, allowLogout);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = writeStmt->BindInt64ByIndex(5, modificationTime);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = writeStmt->BindUTF8StringByIndex(6, rpBaseDomain);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = writeStmt->Execute();
    NS_ENSURE_SUCCESS(rv, rv);
  }
  return NS_OK;
}

void IdentityCredentialStorageService::IncrementPendingWrites() {
  MonitorAutoLock lock(mMonitor);
  MOZ_ASSERT(mPendingWrites < std::numeric_limits<uint32_t>::max());
  mPendingWrites++;
}

void IdentityCredentialStorageService::DecrementPendingWrites() {
  MonitorAutoLock lock(mMonitor);
  MOZ_ASSERT(mPendingWrites > 0);
  mPendingWrites--;
}

// static
nsresult IdentityCredentialStorageService::UpsertData(
    mozIStorageConnection* aDatabaseConnection, nsIPrincipal* aRPPrincipal,
    nsIPrincipal* aIDPPrincipal, nsACString const& aCredentialID,
    bool aRegistered, bool aAllowLogout) {
  NS_ENSURE_ARG_POINTER(aDatabaseConnection);
  NS_ENSURE_ARG_POINTER(aRPPrincipal);
  NS_ENSURE_ARG_POINTER(aIDPPrincipal);
  nsresult rv;
  constexpr auto upsert_query =
      "INSERT INTO identity(rpOrigin, idpOrigin, credentialId, "
      "registered, allowLogout, modificationTime, rpBaseDomain)"
      "VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7)"
      "ON CONFLICT(rpOrigin, idpOrigin, credentialId)"
      "DO UPDATE SET registered=excluded.registered, "
      "allowLogout=excluded.allowLogout, "
      "modificationTime=excluded.modificationTime"_ns;

  nsCOMPtr<mozIStorageStatement> stmt;
  rv = aDatabaseConnection->CreateStatement(upsert_query, getter_AddRefs(stmt));
  NS_ENSURE_SUCCESS(rv, rv);
  nsCString rpOrigin;
  rv = aRPPrincipal->GetOrigin(rpOrigin);
  NS_ENSURE_SUCCESS(rv, rv);
  nsCString idpOrigin;
  rv = aIDPPrincipal->GetOrigin(idpOrigin);
  NS_ENSURE_SUCCESS(rv, rv);
  nsCString rpBaseDomain;
  rv = aRPPrincipal->GetBaseDomain(rpBaseDomain);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = stmt->BindUTF8StringByIndex(0, rpOrigin);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = stmt->BindUTF8StringByIndex(1, idpOrigin);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = stmt->BindUTF8StringByIndex(2, aCredentialID);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = stmt->BindInt64ByIndex(3, aRegistered ? 1 : 0);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = stmt->BindInt64ByIndex(4, aAllowLogout ? 1 : 0);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = stmt->BindInt64ByIndex(5, MODIFIED_NOW);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = stmt->BindUTF8StringByIndex(6, rpBaseDomain);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = stmt->Execute();
  NS_ENSURE_SUCCESS(rv, rv);
  return NS_OK;
}

// static
nsresult IdentityCredentialStorageService::DeleteData(
    mozIStorageConnection* aDatabaseConnection, nsIPrincipal* aRPPrincipal,
    nsIPrincipal* aIDPPrincipal, nsACString const& aCredentialID) {
  NS_ENSURE_ARG_POINTER(aDatabaseConnection);
  NS_ENSURE_ARG_POINTER(aRPPrincipal);
  NS_ENSURE_ARG_POINTER(aIDPPrincipal);
  auto constexpr deleteQuery =
      "DELETE FROM identity WHERE rpOrigin=?1 AND idpOrigin=?2 AND "
      "credentialId=?3"_ns;
  nsCOMPtr<mozIStorageStatement> stmt;
  nsresult rv =
      aDatabaseConnection->CreateStatement(deleteQuery, getter_AddRefs(stmt));
  NS_ENSURE_SUCCESS(rv, rv);
  nsCString rpOrigin;
  rv = aRPPrincipal->GetOrigin(rpOrigin);
  NS_ENSURE_SUCCESS(rv, rv);
  nsCString idpOrigin;
  rv = aIDPPrincipal->GetOrigin(idpOrigin);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = stmt->BindUTF8StringByIndex(0, rpOrigin);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = stmt->BindUTF8StringByIndex(1, idpOrigin);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = stmt->BindUTF8StringByIndex(2, aCredentialID);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = stmt->Execute();
  NS_ENSURE_SUCCESS(rv, rv);
  return NS_OK;
}

// static
nsresult IdentityCredentialStorageService::ClearData(
    mozIStorageConnection* aDatabaseConnection) {
  NS_ENSURE_ARG_POINTER(aDatabaseConnection);
  nsresult rv =
      aDatabaseConnection->ExecuteSimpleSQL("DELETE FROM identity;"_ns);
  NS_ENSURE_SUCCESS(rv, rv);
  return NS_OK;
}

// static
nsresult
IdentityCredentialStorageService::DeleteDataFromOriginAttributesPattern(
    mozIStorageConnection* aDatabaseConnection,
    OriginAttributesPattern const& aOriginAttributesPattern) {
  NS_ENSURE_ARG_POINTER(aDatabaseConnection);
  nsCOMPtr<mozIStorageFunction> patternMatchFunction(
      new OriginAttrsPatternMatchOriginSQLFunction(aOriginAttributesPattern));

  nsresult rv = aDatabaseConnection->CreateFunction(
      "ORIGIN_ATTRS_PATTERN_MATCH_ORIGIN"_ns, 1, patternMatchFunction);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aDatabaseConnection->ExecuteSimpleSQL(
      "DELETE FROM identity WHERE "
      "ORIGIN_ATTRS_PATTERN_MATCH_ORIGIN(rpOrigin);"_ns);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aDatabaseConnection->RemoveFunction(
      "ORIGIN_ATTRS_PATTERN_MATCH_ORIGIN"_ns);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

// static
nsresult IdentityCredentialStorageService::DeleteDataFromTimeRange(
    mozIStorageConnection* aDatabaseConnection, int64_t aStart, int64_t aEnd) {
  NS_ENSURE_ARG_POINTER(aDatabaseConnection);
  auto constexpr deleteTimeQuery =
      "DELETE FROM identity WHERE modificationTime > ?1 and modificationTime "
      "< ?2"_ns;
  nsCOMPtr<mozIStorageStatement> stmt;
  nsresult rv = aDatabaseConnection->CreateStatement(deleteTimeQuery,
                                                     getter_AddRefs(stmt));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = stmt->BindInt64ByIndex(0, aStart);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = stmt->BindInt64ByIndex(1, aEnd);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = stmt->Execute();
  NS_ENSURE_SUCCESS(rv, rv);
  return NS_OK;
}

// static
nsresult IdentityCredentialStorageService::DeleteDataFromPrincipal(
    mozIStorageConnection* aDatabaseConnection, nsIPrincipal* aPrincipal) {
  NS_ENSURE_ARG_POINTER(aDatabaseConnection);
  NS_ENSURE_ARG_POINTER(aPrincipal);

  nsCString rpOrigin;
  nsresult rv = aPrincipal->GetOrigin(rpOrigin);
  NS_ENSURE_SUCCESS(rv, rv);
  auto constexpr deletePrincipalQuery =
      "DELETE FROM identity WHERE rpOrigin=?1"_ns;
  nsCOMPtr<mozIStorageStatement> stmt;
  rv = aDatabaseConnection->CreateStatement(deletePrincipalQuery,
                                            getter_AddRefs(stmt));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = stmt->BindUTF8StringByIndex(0, rpOrigin);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = stmt->Execute();
  NS_ENSURE_SUCCESS(rv, rv);
  return NS_OK;
}

// static
nsresult IdentityCredentialStorageService::DeleteDataFromBaseDomain(
    mozIStorageConnection* aDatabaseConnection, nsACString const& aBaseDomain) {
  NS_ENSURE_ARG_POINTER(aDatabaseConnection);

  auto constexpr deleteBaseDomainQuery =
      "DELETE FROM identity WHERE rpBaseDomain=?1"_ns;
  nsCOMPtr<mozIStorageStatement> stmt;
  nsresult rv = aDatabaseConnection->CreateStatement(deleteBaseDomainQuery,
                                                     getter_AddRefs(stmt));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = stmt->BindUTF8StringByIndex(0, aBaseDomain);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = stmt->Execute();
  NS_ENSURE_SUCCESS(rv, rv);
  return NS_OK;
}

NS_IMETHODIMP IdentityCredentialStorageService::SetState(
    nsIPrincipal* aRPPrincipal, nsIPrincipal* aIDPPrincipal,
    nsACString const& aCredentialID, bool aRegistered, bool aAllowLogout) {
  AssertIsOnMainThread();
  NS_ENSURE_ARG_POINTER(aRPPrincipal);
  NS_ENSURE_ARG_POINTER(aIDPPrincipal);

  nsresult rv;
  rv = WaitForInitialization();
  NS_ENSURE_SUCCESS(rv, rv);
  rv = IdentityCredentialStorageService::ValidatePrincipal(aRPPrincipal);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = UpsertData(mMemoryDatabaseConnection, aRPPrincipal, aIDPPrincipal,
                  aCredentialID, aRegistered, aAllowLogout);
  NS_ENSURE_SUCCESS(rv, rv);

  IncrementPendingWrites();
  RefPtr<IdentityCredentialStorageService> self = this;
  RefPtr<nsIPrincipal> rpPrincipal = aRPPrincipal;
  RefPtr<nsIPrincipal> idpPrincipal = aIDPPrincipal;
  nsCString credentialID(aCredentialID);
  mBackgroundThread->Dispatch(
      NS_NewRunnableFunction("IdentityCredentialStorageService::Init",
                             [self, rpPrincipal, idpPrincipal, credentialID,
                              aRegistered, aAllowLogout]() {
                               nsresult rv = UpsertData(
                                   self->mDiskDatabaseConnection, rpPrincipal,
                                   idpPrincipal, credentialID, aRegistered,
                                   aAllowLogout);
                               NS_ENSURE_SUCCESS_VOID(rv);
                               self->DecrementPendingWrites();
                             }),
      NS_DISPATCH_EVENT_MAY_BLOCK);
  return NS_OK;
}

NS_IMETHODIMP IdentityCredentialStorageService::GetState(
    nsIPrincipal* aRPPrincipal, nsIPrincipal* aIDPPrincipal,
    nsACString const& aCredentialID, bool* aRegistered, bool* aAllowLogout) {
  AssertIsOnMainThread();
  NS_ENSURE_ARG_POINTER(aRPPrincipal);
  NS_ENSURE_ARG_POINTER(aIDPPrincipal);

  nsresult rv;
  rv = WaitForInitialization();
  NS_ENSURE_SUCCESS(rv, rv);
  rv = IdentityCredentialStorageService::ValidatePrincipal(aRPPrincipal);
  NS_ENSURE_SUCCESS(rv, rv);

  auto constexpr selectQuery =
      "SELECT registered, allowLogout FROM identity WHERE rpOrigin=?1 AND "
      "idpOrigin=?2 AND credentialId=?3"_ns;
  nsCOMPtr<mozIStorageStatement> stmt;
  rv = mMemoryDatabaseConnection->CreateStatement(selectQuery,
                                                  getter_AddRefs(stmt));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCString rpOrigin;
  nsCString idpOrigin;
  rv = aRPPrincipal->GetOrigin(rpOrigin);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = aIDPPrincipal->GetOrigin(idpOrigin);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = stmt->BindUTF8StringByIndex(0, rpOrigin);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = stmt->BindUTF8StringByIndex(1, idpOrigin);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = stmt->BindUTF8StringByIndex(2, aCredentialID);
  NS_ENSURE_SUCCESS(rv, rv);

  bool hasResult;
  // If we find a result, return it
  if (NS_SUCCEEDED(stmt->ExecuteStep(&hasResult)) && hasResult) {
    int64_t registeredInt, allowLogoutInt;
    rv = stmt->GetInt64(0, &registeredInt);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = stmt->GetInt64(1, &allowLogoutInt);
    NS_ENSURE_SUCCESS(rv, rv);
    *aRegistered = registeredInt != 0;
    *aAllowLogout = allowLogoutInt != 0;
    return NS_OK;
  }

  // The tuple was not found on disk or in memory, use the defaults.
  *aRegistered = false;
  *aAllowLogout = false;
  return NS_OK;
}

NS_IMETHODIMP IdentityCredentialStorageService::Delete(
    nsIPrincipal* aRPPrincipal, nsIPrincipal* aIDPPrincipal,
    nsACString const& aCredentialID) {
  AssertIsOnMainThread();
  NS_ENSURE_ARG_POINTER(aRPPrincipal);
  NS_ENSURE_ARG_POINTER(aIDPPrincipal);

  nsresult rv;
  rv = WaitForInitialization();
  NS_ENSURE_SUCCESS(rv, rv);
  rv = IdentityCredentialStorageService::ValidatePrincipal(aRPPrincipal);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = DeleteData(mMemoryDatabaseConnection, aRPPrincipal, aIDPPrincipal,
                  aCredentialID);
  NS_ENSURE_SUCCESS(rv, rv);

  IncrementPendingWrites();
  RefPtr<IdentityCredentialStorageService> self = this;
  RefPtr<nsIPrincipal> rpPrincipal = aRPPrincipal;
  RefPtr<nsIPrincipal> idpPrincipal = aIDPPrincipal;
  nsCString credentialID(aCredentialID);
  mBackgroundThread->Dispatch(
      NS_NewRunnableFunction("IdentityCredentialStorageService::Init",
                             [self, rpPrincipal, idpPrincipal, credentialID]() {
                               nsresult rv = DeleteData(
                                   self->mDiskDatabaseConnection, rpPrincipal,
                                   idpPrincipal, credentialID);
                               NS_ENSURE_SUCCESS_VOID(rv);
                               self->DecrementPendingWrites();
                             }),
      NS_DISPATCH_EVENT_MAY_BLOCK);

  return NS_OK;
}

NS_IMETHODIMP IdentityCredentialStorageService::Clear() {
  AssertIsOnMainThread();
  nsresult rv;
  rv = WaitForInitialization();
  NS_ENSURE_SUCCESS(rv, rv);
  rv = ClearData(mMemoryDatabaseConnection);
  NS_ENSURE_SUCCESS(rv, rv);
  IncrementPendingWrites();
  RefPtr<IdentityCredentialStorageService> self = this;
  mBackgroundThread->Dispatch(
      NS_NewRunnableFunction("IdentityCredentialStorageService::Init",
                             [self]() {
                               nsresult rv =
                                   ClearData(self->mDiskDatabaseConnection);
                               NS_ENSURE_SUCCESS_VOID(rv);
                               self->DecrementPendingWrites();
                             }),
      NS_DISPATCH_EVENT_MAY_BLOCK);

  return NS_OK;
}

NS_IMETHODIMP
IdentityCredentialStorageService::DeleteFromOriginAttributesPattern(
    nsAString const& aOriginAttributesPattern) {
  AssertIsOnMainThread();
  NS_ENSURE_FALSE(aOriginAttributesPattern.IsEmpty(), NS_ERROR_FAILURE);
  OriginAttributesPattern oaPattern;
  if (!oaPattern.Init(aOriginAttributesPattern)) {
    NS_ERROR("Could not parse the argument for OriginAttributes");
    return NS_ERROR_FAILURE;
  }
  nsresult rv;
  rv = WaitForInitialization();
  NS_ENSURE_SUCCESS(rv, rv);
  rv = DeleteDataFromOriginAttributesPattern(mMemoryDatabaseConnection,
                                             oaPattern);
  NS_ENSURE_SUCCESS(rv, rv);
  IncrementPendingWrites();
  RefPtr<IdentityCredentialStorageService> self = this;
  mBackgroundThread->Dispatch(
      NS_NewRunnableFunction(
          "IdentityCredentialStorageService::Init",
          [self, oaPattern]() {
            nsresult rv = DeleteDataFromOriginAttributesPattern(
                self->mDiskDatabaseConnection, oaPattern);
            NS_ENSURE_SUCCESS_VOID(rv);
            self->DecrementPendingWrites();
          }),
      NS_DISPATCH_EVENT_MAY_BLOCK);
  return NS_OK;
}

NS_IMETHODIMP IdentityCredentialStorageService::DeleteFromTimeRange(
    int64_t aStart, int64_t aEnd) {
  AssertIsOnMainThread();
  nsresult rv;
  rv = WaitForInitialization();
  NS_ENSURE_SUCCESS(rv, rv);
  rv = DeleteDataFromTimeRange(mMemoryDatabaseConnection, aStart, aEnd);
  NS_ENSURE_SUCCESS(rv, rv);
  IncrementPendingWrites();
  RefPtr<IdentityCredentialStorageService> self = this;
  mBackgroundThread->Dispatch(
      NS_NewRunnableFunction("IdentityCredentialStorageService::Init",
                             [self, aStart, aEnd]() {
                               nsresult rv = DeleteDataFromTimeRange(
                                   self->mDiskDatabaseConnection, aStart, aEnd);
                               NS_ENSURE_SUCCESS_VOID(rv);
                               self->DecrementPendingWrites();
                             }),
      NS_DISPATCH_EVENT_MAY_BLOCK);
  return NS_OK;
}

NS_IMETHODIMP IdentityCredentialStorageService::
    IdentityCredentialStorageService::DeleteFromPrincipal(
        nsIPrincipal* aRPPrincipal) {
  AssertIsOnMainThread();
  NS_ENSURE_ARG_POINTER(aRPPrincipal);
  nsresult rv =
      IdentityCredentialStorageService::ValidatePrincipal(aRPPrincipal);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = DeleteDataFromPrincipal(mMemoryDatabaseConnection, aRPPrincipal);
  NS_ENSURE_SUCCESS(rv, rv);
  IncrementPendingWrites();
  RefPtr<IdentityCredentialStorageService> self = this;
  RefPtr<nsIPrincipal> principal = aRPPrincipal;
  mBackgroundThread->Dispatch(
      NS_NewRunnableFunction("IdentityCredentialStorageService::Init",
                             [self, principal]() {
                               nsresult rv = DeleteDataFromPrincipal(
                                   self->mDiskDatabaseConnection, principal);
                               NS_ENSURE_SUCCESS_VOID(rv);
                               self->DecrementPendingWrites();
                             }),
      NS_DISPATCH_EVENT_MAY_BLOCK);

  return NS_OK;
}

NS_IMETHODIMP IdentityCredentialStorageService::DeleteFromBaseDomain(
    nsACString const& aBaseDomain) {
  AssertIsOnMainThread();
  nsresult rv;
  rv = WaitForInitialization();
  NS_ENSURE_SUCCESS(rv, rv);
  rv = DeleteDataFromBaseDomain(mMemoryDatabaseConnection, aBaseDomain);
  NS_ENSURE_SUCCESS(rv, rv);
  IncrementPendingWrites();
  RefPtr<IdentityCredentialStorageService> self = this;
  nsCString baseDomain(aBaseDomain);
  mBackgroundThread->Dispatch(
      NS_NewRunnableFunction("IdentityCredentialStorageService::Init",
                             [self, baseDomain]() {
                               nsresult rv = DeleteDataFromBaseDomain(
                                   self->mDiskDatabaseConnection, baseDomain);
                               NS_ENSURE_SUCCESS_VOID(rv);
                               self->DecrementPendingWrites();
                             }),
      NS_DISPATCH_EVENT_MAY_BLOCK);
  return NS_OK;
}

NS_IMETHODIMP
IdentityCredentialStorageService::Observe(nsISupports* aSubject,
                                          const char* aTopic,
                                          const char16_t* aData) {
  AssertIsOnMainThread();
  // Double check that we have the right topic.
  if (!nsCRT::strcmp(aTopic, "last-pb-context-exited")) {
    MonitorAutoLock lock(mMonitor);
    if (mInitialized && mMemoryDatabaseConnection) {
      nsCOMPtr<mozIStorageFunction> patternMatchFunction(
          new PrivateBrowsingOriginSQLFunction());
      nsresult rv = mMemoryDatabaseConnection->CreateFunction(
          "PRIVATE_BROWSING_PATTERN_MATCH_ORIGIN"_ns, 1, patternMatchFunction);
      NS_ENSURE_SUCCESS(rv, rv);

      rv = mMemoryDatabaseConnection->ExecuteSimpleSQL(
          "DELETE FROM identity WHERE "
          "PRIVATE_BROWSING_PATTERN_MATCH_ORIGIN(rpOrigin);"_ns);
      NS_ENSURE_SUCCESS(rv, rv);

      rv = mMemoryDatabaseConnection->RemoveFunction(
          "PRIVATE_BROWSING_PATTERN_MATCH_ORIGIN"_ns);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }
  return NS_OK;
}

NS_IMPL_ISUPPORTS(OriginAttrsPatternMatchOriginSQLFunction, mozIStorageFunction)

NS_IMETHODIMP
OriginAttrsPatternMatchOriginSQLFunction::OnFunctionCall(
    mozIStorageValueArray* aFunctionArguments, nsIVariant** aResult) {
  nsresult rv;

  nsAutoCString origin;
  rv = aFunctionArguments->GetUTF8String(0, origin);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCString originNoSuffix;
  OriginAttributes oa;
  bool parsedSuccessfully = oa.PopulateFromOrigin(origin, originNoSuffix);
  NS_ENSURE_TRUE(parsedSuccessfully, NS_ERROR_FAILURE);
  bool result = mPattern.Matches(oa);

  RefPtr<nsVariant> outVar(new nsVariant());
  rv = outVar->SetAsBool(result);
  NS_ENSURE_SUCCESS(rv, rv);

  outVar.forget(aResult);
  return NS_OK;
}

NS_IMPL_ISUPPORTS(PrivateBrowsingOriginSQLFunction, mozIStorageFunction)

NS_IMETHODIMP
PrivateBrowsingOriginSQLFunction::OnFunctionCall(
    mozIStorageValueArray* aFunctionArguments, nsIVariant** aResult) {
  nsresult rv;

  nsAutoCString origin;
  rv = aFunctionArguments->GetUTF8String(0, origin);
  NS_ENSURE_SUCCESS(rv, rv);

  bool result = OriginAttributes::IsPrivateBrowsing(origin);

  RefPtr<nsVariant> outVar(new nsVariant());
  rv = outVar->SetAsBool(result);
  NS_ENSURE_SUCCESS(rv, rv);

  outVar.forget(aResult);
  return NS_OK;
}

}  // namespace mozilla
