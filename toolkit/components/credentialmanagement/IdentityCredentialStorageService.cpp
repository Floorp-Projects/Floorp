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
  MonitorAutoLock lock(mMonitor);

  if (AppShutdown::IsInOrBeyond(ShutdownPhase::AppShutdown)) {
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
  rv = mDatabaseFile->AppendNative(nsLiteralCString(ACCOUNT_STATE_FILENAME));
  NS_ENSURE_SUCCESS(rv, rv);

  // Register the PBMode cleaner (IdentityCredentialStorageService::Observe) as
  // an observer.
  nsCOMPtr<nsIObserverService> observerService =
      mozilla::services::GetObserverService();
  NS_ENSURE_TRUE(observerService, NS_ERROR_FAILURE);
  observerService->AddObserver(this, "last-pb-context-exited", false);

  // rv = GetMemoryDatabaseConnection();
  // if (NS_WARN_IF(NS_FAILED(rv))) {
  //   mErrored.Flip();
  //   return rv;
  // }

  NS_ENSURE_SUCCESS(
      NS_CreateBackgroundTaskQueue("IdentityCredentialStorage",
                                   getter_AddRefs(mBackgroundThread)),
      NS_ERROR_FAILURE);

  RefPtr<IdentityCredentialStorageService> self = this;

  mBackgroundThread->Dispatch(
      NS_NewRunnableFunction("IdentityCredentialStorageService::Init",
                             [self]() {
                               MonitorAutoLock lock(self->mMonitor);
                               //  nsresult rv =
                               //  self->GetDiskDatabaseConnection(); if
                               //  (NS_WARN_IF(NS_FAILED(rv))) {
                               //    self->mErrored.Flip();
                               //    self->mMonitor.Notify();
                               //    return;
                               //  }

                               //  rv = self->LoadMemoryTableFromDisk();
                               //  if (NS_WARN_IF(NS_FAILED(rv))) {
                               //    self->mErrored.Flip();
                               //    self->mMonitor.NotifyAll();
                               //    return;
                               //  }

                               self->mInitialized.Flip();
                               self->mMonitor.Notify();
                             }),
      NS_DISPATCH_EVENT_MAY_BLOCK);

  return NS_OK;
}

nsresult IdentityCredentialStorageService::WaitForInitialization() {
  MonitorAutoLock lock(mMonitor);
  while (!mInitialized && !mErrored) {
    mMonitor.Wait();
  }
  if (mErrored) {
    return NS_ERROR_FAILURE;
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

nsresult createTable(mozIStorageConnection* aDatabase) {
  // Currently there is only one schema version, so we just need to create the
  // table. The definition uses no explicit rowid column, instead primary keying
  // on the tuple defined in the spec. We store two bits and some additional
  // data to make integration with the ClearDataService easier/possible.
  NS_ENSURE_ARG_POINTER(aDatabase);
  nsresult rv = aDatabase->SetSchemaVersion(SCHEMA_VERSION);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = aDatabase->ExecuteSimpleSQL(
      nsLiteralCString("CREATE TABLE identity ("
                       "rpOrigin TEXT NOT NULL"
                       ",idpOrigin TEXT NOT NULL"
                       ",credentialId TEXT NOT NULL"
                       ",registered INTEGER"
                       ",allowLogout INTEGER"
                       ",modificationTime INTEGER"
                       ",rpBaseDomain TEXT"
                       ",PRIMARY KEY (rpOrigin, idpOrigin, credentialId)"
                       ")"));
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult getMemoryDatabaseConnection(mozIStorageConnection** aDatabase) {
  nsCOMPtr<mozIStorageService> storage =
      do_GetService(MOZ_STORAGE_SERVICE_CONTRACTID);
  NS_ENSURE_TRUE(storage, NS_ERROR_UNEXPECTED);
  nsresult rv = storage->OpenSpecialDatabase(
      kMozStorageMemoryStorageKey, "icsprivatedb"_ns,
      mozIStorageService::CONNECTION_DEFAULT, aDatabase);
  NS_ENSURE_SUCCESS(rv, rv);
  bool ready = false;
  (*aDatabase)->GetConnectionReady(&ready);
  NS_ENSURE_TRUE(ready, NS_ERROR_UNEXPECTED);
  bool tableExists = false;
  (*aDatabase)->TableExists("identity"_ns, &tableExists);
  if (!tableExists) {
    rv = createTable(*aDatabase);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  return NS_OK;
}

nsresult getDiskDatabaseConnection(mozIStorageConnection** aDatabase) {
  // Create the file we store the database in.
  nsCOMPtr<nsIFile> profileDir;
  nsresult rv = NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR,
                                       getter_AddRefs(profileDir));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = profileDir->AppendNative(nsLiteralCString(ACCOUNT_STATE_FILENAME));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<mozIStorageService> storage =
      do_GetService(MOZ_STORAGE_SERVICE_CONTRACTID);
  NS_ENSURE_TRUE(storage, NS_ERROR_UNEXPECTED);
  rv = storage->OpenDatabase(profileDir, mozIStorageService::CONNECTION_DEFAULT,
                             aDatabase);
  if (rv == NS_ERROR_FILE_CORRUPTED) {
    rv = profileDir->Remove(false);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = storage->OpenDatabase(
        profileDir, mozIStorageService::CONNECTION_DEFAULT, aDatabase);
  }
  NS_ENSURE_SUCCESS(rv, rv);
  bool ready = false;
  (*aDatabase)->GetConnectionReady(&ready);
  NS_ENSURE_TRUE(ready, NS_ERROR_UNEXPECTED);
  return NS_OK;
}

NS_IMETHODIMP IdentityCredentialStorageService::SetState(
    nsIPrincipal* aRPPrincipal, nsIPrincipal* aIDPPrincipal,
    nsACString const& aCredentialID, bool aRegistered, bool aAllowLogout) {
  AssertIsOnMainThread();
  if (!StaticPrefs::dom_security_credentialmanagement_identity_enabled()) {
    return NS_OK;
  }
  MOZ_ASSERT(XRE_IsParentProcess());
  NS_ENSURE_ARG_POINTER(aRPPrincipal);
  NS_ENSURE_ARG_POINTER(aIDPPrincipal);

  nsresult rv;
  rv = IdentityCredentialStorageService::ValidatePrincipal(aRPPrincipal);
  NS_ENSURE_SUCCESS(rv, rv);

  RefPtr<mozIStorageConnection> database;
  rv = getDiskDatabaseConnection(getter_AddRefs(database));
  NS_ENSURE_SUCCESS(rv, rv);
  RefPtr<mozIStorageConnection> privateBrowsingDatabase;
  rv = getMemoryDatabaseConnection(getter_AddRefs(privateBrowsingDatabase));
  NS_ENSURE_SUCCESS(rv, rv);

  // Build the statement on one of our databases. This is in memory if the RP
  // principal is private, disk otherwise. The queries are the same, using the
  // SQLite3 UPSERT syntax.
  nsCOMPtr<mozIStorageStatement> stmt;
  nsCString rpOrigin;
  rv = aRPPrincipal->GetOrigin(rpOrigin);
  NS_ENSURE_SUCCESS(rv, rv);
  constexpr auto upsert_query = nsLiteralCString(
      "INSERT INTO identity(rpOrigin, idpOrigin, credentialId, "
      "registered, allowLogout, modificationTime, rpBaseDomain)"
      "VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7)"
      "ON CONFLICT(rpOrigin, idpOrigin, credentialId)"
      "DO UPDATE SET registered=excluded.registered, "
      "allowLogout=excluded.allowLogout, "
      "modificationTime=excluded.modificationTime");
  if (OriginAttributes::IsPrivateBrowsing(rpOrigin)) {
    rv = privateBrowsingDatabase->CreateStatement(upsert_query,
                                                  getter_AddRefs(stmt));
    NS_ENSURE_SUCCESS(rv, rv);
  } else {
    rv = database->CreateStatement(upsert_query, getter_AddRefs(stmt));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // Bind the arguments to the query and execute it.
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
  rv = stmt->BindInt32ByIndex(3, aRegistered ? 1 : 0);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = stmt->BindInt32ByIndex(4, aAllowLogout ? 1 : 0);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = stmt->BindInt64ByIndex(5, MODIFIED_NOW);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = stmt->BindUTF8StringByIndex(6, rpBaseDomain);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = stmt->Execute();
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

NS_IMETHODIMP IdentityCredentialStorageService::GetState(
    nsIPrincipal* aRPPrincipal, nsIPrincipal* aIDPPrincipal,
    nsACString const& aCredentialID, bool* aRegistered, bool* aAllowLogout) {
  AssertIsOnMainThread();
  if (!StaticPrefs::dom_security_credentialmanagement_identity_enabled()) {
    *aRegistered = false;
    *aAllowLogout = false;
    return NS_OK;
  }
  nsresult rv;
  rv = IdentityCredentialStorageService::ValidatePrincipal(aRPPrincipal);
  NS_ENSURE_SUCCESS(rv, rv);

  RefPtr<mozIStorageConnection> database;
  rv = getDiskDatabaseConnection(getter_AddRefs(database));
  NS_ENSURE_SUCCESS(rv, rv);
  RefPtr<mozIStorageConnection> privateBrowsingDatabase;
  rv = getMemoryDatabaseConnection(getter_AddRefs(privateBrowsingDatabase));
  NS_ENSURE_SUCCESS(rv, rv);

  auto constexpr selectQuery = nsLiteralCString(
      "SELECT registered, allowLogout FROM identity WHERE rpOrigin=?1 AND "
      "idpOrigin=?2 AND credentialId=?3");

  nsCString rpOrigin;
  nsCString idpOrigin;
  rv = aRPPrincipal->GetOrigin(rpOrigin);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = aIDPPrincipal->GetOrigin(idpOrigin);
  NS_ENSURE_SUCCESS(rv, rv);

  // If the RP principal is private, query the provided tuple in memory
  nsCOMPtr<mozIStorageStatement> stmt;
  if (OriginAttributes::IsPrivateBrowsing(rpOrigin)) {
    rv = privateBrowsingDatabase->CreateStatement(selectQuery,
                                                  getter_AddRefs(stmt));
    NS_ENSURE_SUCCESS(rv, rv);
  } else {
    rv = database->CreateStatement(selectQuery, getter_AddRefs(stmt));
    NS_ENSURE_SUCCESS(rv, rv);
  }

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
  if (!StaticPrefs::dom_security_credentialmanagement_identity_enabled()) {
    return NS_OK;
  }
  nsresult rv;
  rv = IdentityCredentialStorageService::ValidatePrincipal(aRPPrincipal);
  NS_ENSURE_SUCCESS(rv, rv);

  RefPtr<mozIStorageConnection> database;
  rv = getDiskDatabaseConnection(getter_AddRefs(database));
  NS_ENSURE_SUCCESS(rv, rv);
  RefPtr<mozIStorageConnection> privateBrowsingDatabase;
  rv = getMemoryDatabaseConnection(getter_AddRefs(privateBrowsingDatabase));
  NS_ENSURE_SUCCESS(rv, rv);

  auto constexpr deleteQuery = nsLiteralCString(
      "DELETE FROM identity WHERE rpOrigin=?1 AND idpOrigin=?2 AND "
      "credentialId=?3");

  // Delete all entries matching this tuple.
  // We only have to execute one statement because we don't want to delete
  // entries on disk from PB mode
  nsCOMPtr<mozIStorageStatement> stmt;
  nsCString rpOrigin;
  rv = aRPPrincipal->GetOrigin(rpOrigin);
  NS_ENSURE_SUCCESS(rv, rv);
  if (OriginAttributes::IsPrivateBrowsing(rpOrigin)) {
    rv = privateBrowsingDatabase->CreateStatement(deleteQuery,
                                                  getter_AddRefs(stmt));
    NS_ENSURE_SUCCESS(rv, rv);
  } else {
    rv = database->CreateStatement(deleteQuery, getter_AddRefs(stmt));
    NS_ENSURE_SUCCESS(rv, rv);
  }
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

NS_IMETHODIMP IdentityCredentialStorageService::Clear() {
  AssertIsOnMainThread();
  if (!StaticPrefs::dom_security_credentialmanagement_identity_enabled()) {
    return NS_OK;
  }
  RefPtr<mozIStorageConnection> database;
  nsresult rv = getDiskDatabaseConnection(getter_AddRefs(database));
  NS_ENSURE_SUCCESS(rv, rv);
  RefPtr<mozIStorageConnection> privateBrowsingDatabase;
  rv = getMemoryDatabaseConnection(getter_AddRefs(privateBrowsingDatabase));
  NS_ENSURE_SUCCESS(rv, rv);

  // We just clear all rows from both databases.
  rv = privateBrowsingDatabase->ExecuteSimpleSQL(
      nsLiteralCString("DELETE FROM identity;"));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = database->ExecuteSimpleSQL(nsLiteralCString("DELETE FROM identity;"));
  NS_ENSURE_SUCCESS(rv, rv);
  return NS_OK;
}

NS_IMETHODIMP
IdentityCredentialStorageService::DeleteFromOriginAttributesPattern(
    nsAString const& aOriginAttributesPattern) {
  if (!StaticPrefs::dom_security_credentialmanagement_identity_enabled()) {
    return NS_OK;
  }
  nsresult rv;
  NS_ENSURE_FALSE(aOriginAttributesPattern.IsEmpty(), NS_ERROR_FAILURE);

  // parse the JSON origin attribute argument
  OriginAttributesPattern oaPattern;
  if (!oaPattern.Init(aOriginAttributesPattern)) {
    NS_ERROR("Could not parse the argument for OriginAttributes");
    return NS_ERROR_FAILURE;
  }

  RefPtr<mozIStorageConnection> database;
  rv = getDiskDatabaseConnection(getter_AddRefs(database));
  NS_ENSURE_SUCCESS(rv, rv);
  RefPtr<mozIStorageConnection> privateBrowsingDatabase;
  rv = getMemoryDatabaseConnection(getter_AddRefs(privateBrowsingDatabase));
  NS_ENSURE_SUCCESS(rv, rv);

  RefPtr<mozIStorageConnection> chosenDatabase;
  if (!oaPattern.mPrivateBrowsingId.WasPassed() ||
      oaPattern.mPrivateBrowsingId.Value() ==
          nsIScriptSecurityManager::DEFAULT_PRIVATE_BROWSING_ID) {
    chosenDatabase = database;
  } else {
    chosenDatabase = privateBrowsingDatabase;
  }

  chosenDatabase->BeginTransaction();
  Vector<int64_t> rowIdsToDelete;
  nsCOMPtr<mozIStorageStatement> stmt;
  rv = chosenDatabase->CreateStatement(
      nsLiteralCString("SELECT rowid, rpOrigin FROM identity;"),
      getter_AddRefs(stmt));
  NS_ENSURE_SUCCESS(rv, rv);

  bool hasResult;
  while (NS_SUCCEEDED(stmt->ExecuteStep(&hasResult)) && hasResult) {
    int64_t rowId;
    nsCString rpOrigin;
    nsCString rpOriginNoSuffix;
    rv = stmt->GetInt64(0, &rowId);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      continue;
    }
    rv = stmt->GetUTF8String(1, rpOrigin);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      continue;
    }
    OriginAttributes oa;
    bool parsedSuccessfully = oa.PopulateFromOrigin(rpOrigin, rpOriginNoSuffix);
    NS_ENSURE_TRUE(parsedSuccessfully, NS_ERROR_FAILURE);
    if (oaPattern.Matches(oa)) {
      bool appendedSuccessfully = rowIdsToDelete.append(rowId);
      NS_ENSURE_TRUE(appendedSuccessfully, NS_ERROR_FAILURE);
    }
  }

  for (auto rowId : rowIdsToDelete) {
    rv = chosenDatabase->CreateStatement(
        nsLiteralCString("DELETE FROM identity WHERE rowid = ?1"),
        getter_AddRefs(stmt));
    NS_ENSURE_SUCCESS(rv, rv);
    rv = stmt->BindInt64ByIndex(0, rowId);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = stmt->Execute();
    NS_ENSURE_SUCCESS(rv, rv);
  }

  rv = chosenDatabase->CommitTransaction();
  NS_ENSURE_SUCCESS(rv, rv);
  return NS_OK;
}

NS_IMETHODIMP IdentityCredentialStorageService::DeleteFromTimeRange(
    int64_t aStart, int64_t aEnd) {
  if (!StaticPrefs::dom_security_credentialmanagement_identity_enabled()) {
    return NS_OK;
  }
  nsresult rv;

  RefPtr<mozIStorageConnection> database;
  rv = getDiskDatabaseConnection(getter_AddRefs(database));
  NS_ENSURE_SUCCESS(rv, rv);
  RefPtr<mozIStorageConnection> privateBrowsingDatabase;
  rv = getMemoryDatabaseConnection(getter_AddRefs(privateBrowsingDatabase));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<mozIStorageStatement> stmt;

  auto constexpr deleteTimeQuery = nsLiteralCString(
      "DELETE FROM identity WHERE modificationTime > ?1 and modificationTime < "
      "?2");

  // We just clear all matching rows from both databases.
  rv = privateBrowsingDatabase->CreateStatement(deleteTimeQuery,
                                                getter_AddRefs(stmt));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = stmt->BindInt64ByIndex(0, aStart);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = stmt->BindInt64ByIndex(1, aEnd);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = stmt->Execute();
  NS_ENSURE_SUCCESS(rv, rv);

  rv = database->CreateStatement(deleteTimeQuery, getter_AddRefs(stmt));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = stmt->BindInt64ByIndex(0, aStart);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = stmt->BindInt64ByIndex(1, aEnd);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = stmt->Execute();
  NS_ENSURE_SUCCESS(rv, rv);
  return NS_OK;
}

NS_IMETHODIMP IdentityCredentialStorageService::
    IdentityCredentialStorageService::DeleteFromPrincipal(
        nsIPrincipal* aRPPrincipal) {
  if (!StaticPrefs::dom_security_credentialmanagement_identity_enabled()) {
    return NS_OK;
  }
  nsresult rv =
      IdentityCredentialStorageService::ValidatePrincipal(aRPPrincipal);
  NS_ENSURE_SUCCESS(rv, rv);

  RefPtr<mozIStorageConnection> database;
  rv = getDiskDatabaseConnection(getter_AddRefs(database));
  NS_ENSURE_SUCCESS(rv, rv);
  RefPtr<mozIStorageConnection> privateBrowsingDatabase;
  rv = getMemoryDatabaseConnection(getter_AddRefs(privateBrowsingDatabase));
  NS_ENSURE_SUCCESS(rv, rv);

  auto constexpr deletePrincipalQuery =
      nsLiteralCString("DELETE FROM identity WHERE rpOrigin=?1");

  // create the (identical) statement on the database we need to clear from.
  // Like delete, a given argument is either private or not, so there is only
  // one argument to execute.
  nsCOMPtr<mozIStorageStatement> stmt;
  nsCString rpOrigin;
  rv = aRPPrincipal->GetOrigin(rpOrigin);
  NS_ENSURE_SUCCESS(rv, rv);
  if (OriginAttributes::IsPrivateBrowsing(rpOrigin)) {
    rv = privateBrowsingDatabase->CreateStatement(deletePrincipalQuery,
                                                  getter_AddRefs(stmt));
    NS_ENSURE_SUCCESS(rv, rv);
  } else {
    rv = database->CreateStatement(deletePrincipalQuery, getter_AddRefs(stmt));
    NS_ENSURE_SUCCESS(rv, rv);
  }
  rv = stmt->BindUTF8StringByIndex(0, rpOrigin);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = stmt->Execute();
  NS_ENSURE_SUCCESS(rv, rv);
  return NS_OK;
}

NS_IMETHODIMP IdentityCredentialStorageService::DeleteFromBaseDomain(
    nsACString const& aBaseDomain) {
  if (!StaticPrefs::dom_security_credentialmanagement_identity_enabled()) {
    return NS_OK;
  }
  nsresult rv;

  RefPtr<mozIStorageConnection> database;
  rv = getDiskDatabaseConnection(getter_AddRefs(database));
  NS_ENSURE_SUCCESS(rv, rv);
  RefPtr<mozIStorageConnection> privateBrowsingDatabase;
  rv = getMemoryDatabaseConnection(getter_AddRefs(privateBrowsingDatabase));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<mozIStorageStatement> stmt;

  auto constexpr deleteBaseDomainQuery =
      nsLiteralCString("DELETE FROM identity WHERE rpBaseDomain=?1");

  // We just clear all matching rows from both databases.
  // This is very easy because we store the relevant base domain.
  rv = database->CreateStatement(deleteBaseDomainQuery, getter_AddRefs(stmt));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = stmt->BindUTF8StringByIndex(0, aBaseDomain);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = stmt->Execute();
  NS_ENSURE_SUCCESS(rv, rv);

  rv = privateBrowsingDatabase->CreateStatement(deleteBaseDomainQuery,
                                                getter_AddRefs(stmt));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = stmt->BindUTF8StringByIndex(0, aBaseDomain);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = stmt->Execute();
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

NS_IMETHODIMP
IdentityCredentialStorageService::Observe(nsISupports* aSubject,
                                          const char* aTopic,
                                          const char16_t* aData) {
  AssertIsOnMainThread();

  // Double check that we have the right topic.
  if (!nsCRT::strcmp(aTopic, "last-pb-context-exited")) {
    RefPtr<mozIStorageConnection> privateBrowsingDatabase;
    nsresult rv =
        getMemoryDatabaseConnection(getter_AddRefs(privateBrowsingDatabase));
    NS_ENSURE_SUCCESS(rv, rv);
    // Delete exactly all of the private browsing data
    rv = privateBrowsingDatabase->ExecuteSimpleSQL(
        nsLiteralCString("DELETE FROM identity;"));
    NS_ENSURE_SUCCESS(rv, rv);
  }
  return NS_OK;
}

}  // namespace mozilla
