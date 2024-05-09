/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et lcs=trail\:.,tab\:>~ :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/DebugOnly.h"

#include "VacuumManager.h"

#include "mozilla/ErrorNames.h"
#include "mozilla/Services.h"
#include "mozilla/Preferences.h"
#include "nsIObserverService.h"
#include "nsIFile.h"
#include "nsThreadUtils.h"
#include "mozilla/Logging.h"
#include "mozilla/IntegerPrintfMacros.h"
#include "prtime.h"

#include "mozStorageConnection.h"
#include "mozStoragePrivateHelpers.h"
#include "mozIStorageCompletionCallback.h"
#include "nsXULAppAPI.h"
#include "xpcpublic.h"

#define OBSERVER_TOPIC_IDLE_DAILY "idle-daily"

// Used to notify the begin and end of a vacuum operation.
#define OBSERVER_TOPIC_VACUUM_BEGIN "vacuum-begin"
#define OBSERVER_TOPIC_VACUUM_END "vacuum-end"
// This notification is sent only in automation when vacuum for a database is
// skipped, and can thus be used to verify that.
#define OBSERVER_TOPIC_VACUUM_SKIP "vacuum-skip"

// This preferences root will contain last vacuum timestamps (in seconds) for
// each database.  The database filename is used as a key.
#define PREF_VACUUM_BRANCH "storage.vacuum.last."

// Time between subsequent vacuum calls for a certain database.
#define VACUUM_INTERVAL_SECONDS (30 * 86400)  // 30 days.

extern mozilla::LazyLogModule gStorageLog;

namespace mozilla::storage {

namespace {

////////////////////////////////////////////////////////////////////////////////
//// Vacuumer declaration.

class Vacuumer final : public mozIStorageCompletionCallback {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_MOZISTORAGECOMPLETIONCALLBACK

  explicit Vacuumer(mozIStorageVacuumParticipant* aParticipant);
  bool execute();

 private:
  nsresult notifyCompletion(bool aSucceeded);
  ~Vacuumer() = default;

  nsCOMPtr<mozIStorageVacuumParticipant> mParticipant;
  nsCString mDBFilename;
  nsCOMPtr<mozIStorageAsyncConnection> mDBConn;
};

////////////////////////////////////////////////////////////////////////////////
//// Vacuumer implementation.

NS_IMPL_ISUPPORTS(Vacuumer, mozIStorageCompletionCallback)

Vacuumer::Vacuumer(mozIStorageVacuumParticipant* aParticipant)
    : mParticipant(aParticipant) {}

bool Vacuumer::execute() {
  MOZ_ASSERT(NS_IsMainThread(), "Must be running on the main thread!");

  // Get the connection and check its validity.
  nsresult rv = mParticipant->GetDatabaseConnection(getter_AddRefs(mDBConn));
  if (NS_FAILED(rv) || !mDBConn) return false;

  nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();

  bool inAutomation = xpc::IsInAutomation();
  // Get the database filename.  Last vacuum time is stored under this name
  // in PREF_VACUUM_BRANCH.
  nsCOMPtr<nsIFile> databaseFile;
  mDBConn->GetDatabaseFile(getter_AddRefs(databaseFile));
  if (!databaseFile) {
    NS_WARNING("Trying to vacuum a in-memory database!");
    if (inAutomation && os) {
      mozilla::Unused << os->NotifyObservers(
          nullptr, OBSERVER_TOPIC_VACUUM_SKIP, u":memory:");
    }
    return false;
  }
  nsAutoString databaseFilename;
  rv = databaseFile->GetLeafName(databaseFilename);
  NS_ENSURE_SUCCESS(rv, false);
  CopyUTF16toUTF8(databaseFilename, mDBFilename);
  MOZ_ASSERT(!mDBFilename.IsEmpty(), "Database filename cannot be empty");

  // Check interval from last vacuum.
  int32_t now = static_cast<int32_t>(PR_Now() / PR_USEC_PER_SEC);
  int32_t lastVacuum;
  nsAutoCString prefName(PREF_VACUUM_BRANCH);
  prefName += mDBFilename;
  rv = Preferences::GetInt(prefName.get(), &lastVacuum);
  if (NS_SUCCEEDED(rv) && (now - lastVacuum) < VACUUM_INTERVAL_SECONDS) {
    // This database was vacuumed recently, skip it.
    if (inAutomation && os) {
      mozilla::Unused << os->NotifyObservers(
          nullptr, OBSERVER_TOPIC_VACUUM_SKIP,
          NS_ConvertUTF8toUTF16(mDBFilename).get());
    }
    return false;
  }

  // Notify that we are about to start vacuuming.  The participant can opt-out
  // if it cannot handle a vacuum at this time, and then we'll move to the next
  // one.
  bool vacuumGranted = false;
  rv = mParticipant->OnBeginVacuum(&vacuumGranted);
  NS_ENSURE_SUCCESS(rv, false);
  if (!vacuumGranted) {
    if (inAutomation && os) {
      mozilla::Unused << os->NotifyObservers(
          nullptr, OBSERVER_TOPIC_VACUUM_SKIP,
          NS_ConvertUTF8toUTF16(mDBFilename).get());
    }
    return false;
  }

  // Ask for the expected page size.  Vacuum can change the page size, unless
  // the database is using WAL journaling.
  // TODO Bug 634374: figure out a strategy to fix page size with WAL.
  int32_t expectedPageSize = 0;
  rv = mParticipant->GetExpectedDatabasePageSize(&expectedPageSize);
  if (NS_FAILED(rv) || !Service::pageSizeIsValid(expectedPageSize)) {
    NS_WARNING("Invalid page size requested for database, won't set it. ");
    NS_WARNING(mDBFilename.get());
    expectedPageSize = 0;
  }

  bool incremental = false;
  mozilla::Unused << mParticipant->GetUseIncrementalVacuum(&incremental);

  // Notify vacuum is about to start.
  if (os) {
    mozilla::Unused << os->NotifyObservers(
        nullptr, OBSERVER_TOPIC_VACUUM_BEGIN,
        NS_ConvertUTF8toUTF16(mDBFilename).get());
  }

  rv = mDBConn->AsyncVacuum(this, incremental, expectedPageSize);
  if (NS_FAILED(rv)) {
    // The connection is not ready.
    mozilla::Unused << Complete(rv, nullptr);
    return false;
  }

  return true;
}

NS_IMETHODIMP
Vacuumer::Complete(nsresult aStatus, nsISupports* aValue) {
  if (NS_SUCCEEDED(aStatus)) {
    // Update last vacuum time.
    int32_t now = static_cast<int32_t>(PR_Now() / PR_USEC_PER_SEC);
    MOZ_ASSERT(!mDBFilename.IsEmpty(), "Database filename cannot be empty");
    nsAutoCString prefName(PREF_VACUUM_BRANCH);
    prefName += mDBFilename;
    DebugOnly<nsresult> rv = Preferences::SetInt(prefName.get(), now);
    MOZ_ASSERT(NS_SUCCEEDED(rv), "Should be able to set a preference");
    notifyCompletion(true);
    return NS_OK;
  }

  nsAutoCString errName;
  GetErrorName(aStatus, errName);
  nsCString errMsg = nsPrintfCString(
      "Vacuum failed on '%s' with error %s - code %" PRIX32, mDBFilename.get(),
      errName.get(), static_cast<uint32_t>(aStatus));
  NS_WARNING(errMsg.get());
  if (MOZ_LOG_TEST(gStorageLog, LogLevel::Error)) {
    MOZ_LOG(gStorageLog, LogLevel::Error, ("%s", errMsg.get()));
  }

  notifyCompletion(false);
  return NS_OK;
}

nsresult Vacuumer::notifyCompletion(bool aSucceeded) {
  nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
  if (os) {
    mozilla::Unused << os->NotifyObservers(
        nullptr, OBSERVER_TOPIC_VACUUM_END,
        NS_ConvertUTF8toUTF16(mDBFilename).get());
  }

  nsresult rv = mParticipant->OnEndVacuum(aSucceeded);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
//// VacuumManager

NS_IMPL_ISUPPORTS(VacuumManager, nsIObserver)

VacuumManager* VacuumManager::gVacuumManager = nullptr;

already_AddRefed<VacuumManager> VacuumManager::getSingleton() {
  // Don't allocate it in the child Process.
  if (!XRE_IsParentProcess()) {
    return nullptr;
  }

  if (!gVacuumManager) {
    auto manager = MakeRefPtr<VacuumManager>();
    MOZ_ASSERT(gVacuumManager == manager.get());
    return manager.forget();
  }
  return do_AddRef(gVacuumManager);
}

VacuumManager::VacuumManager() : mParticipants("vacuum-participant") {
  MOZ_ASSERT(!gVacuumManager,
             "Attempting to create two instances of the service!");
  gVacuumManager = this;
}

VacuumManager::~VacuumManager() {
  // Remove the static reference to the service.  Check to make sure its us
  // in case somebody creates an extra instance of the service.
  MOZ_ASSERT(gVacuumManager == this,
             "Deleting a non-singleton instance of the service");
  if (gVacuumManager == this) {
    gVacuumManager = nullptr;
  }
}

////////////////////////////////////////////////////////////////////////////////
//// nsIObserver

NS_IMETHODIMP
VacuumManager::Observe(nsISupports* aSubject, const char* aTopic,
                       const char16_t* aData) {
  if (strcmp(aTopic, OBSERVER_TOPIC_IDLE_DAILY) == 0) {
    // Try to run vacuum on all registered entries.  Will stop at the first
    // successful one.
    nsCOMArray<mozIStorageVacuumParticipant> entries;
    mParticipants.GetEntries(entries);
    // If there are more entries than what a month can contain, we could end up
    // skipping some, since we run daily.  So we use a starting index.
    static const char* kPrefName = PREF_VACUUM_BRANCH "index";
    int32_t startIndex = Preferences::GetInt(kPrefName, 0);
    if (startIndex >= entries.Count()) {
      startIndex = 0;
    }
    int32_t index;
    for (index = startIndex; index < entries.Count(); ++index) {
      RefPtr<Vacuumer> vacuum = new Vacuumer(entries[index]);
      // Only vacuum one database per day.
      if (vacuum->execute()) {
        break;
      }
    }
    DebugOnly<nsresult> rv = Preferences::SetInt(kPrefName, index);
    MOZ_ASSERT(NS_SUCCEEDED(rv), "Should be able to set a preference");
  }

  return NS_OK;
}

}  // namespace mozilla::storage
