/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et lcs=trail\:.,tab\:>~ :
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozStorage.
 *
 * The Initial Developer of the Original Code is
 * the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Marco Bonardo <mak77@bonardo.net> (Original Author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "VacuumManager.h"

#include "mozilla/Services.h"
#include "mozilla/Preferences.h"
#include "nsIObserverService.h"
#include "nsPrintfCString.h"
#include "nsIFile.h"
#include "nsThreadUtils.h"
#include "prlog.h"

#include "mozStorageConnection.h"
#include "mozIStorageStatement.h"
#include "mozIStorageAsyncStatement.h"
#include "mozIStoragePendingStatement.h"
#include "mozIStorageError.h"

#define OBSERVER_TOPIC_IDLE_DAILY "idle-daily"
#define OBSERVER_TOPIC_XPCOM_SHUTDOWN "xpcom-shutdown"

// Used to notify begin and end of a heavy IO task.
#define OBSERVER_TOPIC_HEAVY_IO "heavy-io-task"
#define OBSERVER_DATA_VACUUM_BEGIN NS_LITERAL_STRING("vacuum-begin")
#define OBSERVER_DATA_VACUUM_END NS_LITERAL_STRING("vacuum-end")

// This preferences root will contain last vacuum timestamps (in seconds) for
// each database.  The database filename is used as a key.
#define PREF_VACUUM_BRANCH "storage.vacuum.last."

// Time between subsequent vacuum calls for a certain database.
#define VACUUM_INTERVAL_SECONDS 30 * 86400 // 30 days.

#ifdef PR_LOGGING
extern PRLogModuleInfo *gStorageLog;
#endif

namespace mozilla {
namespace storage {

namespace {

////////////////////////////////////////////////////////////////////////////////
//// BaseCallback

class BaseCallback : public mozIStorageStatementCallback
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_MOZISTORAGESTATEMENTCALLBACK
  BaseCallback() {}
protected:
  virtual ~BaseCallback() {}
};

NS_IMETHODIMP
BaseCallback::HandleError(mozIStorageError *aError)
{
#ifdef DEBUG
  PRInt32 result;
  nsresult rv = aError->GetResult(&result);
  NS_ENSURE_SUCCESS(rv, rv);
  nsCAutoString message;
  rv = aError->GetMessage(message);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCAutoString warnMsg;
  warnMsg.AppendLiteral("An error occured during async execution: ");
  warnMsg.AppendInt(result);
  warnMsg.AppendLiteral(" ");
  warnMsg.Append(message);
  NS_WARNING(warnMsg.get());
#endif
  return NS_OK;
}

NS_IMETHODIMP
BaseCallback::HandleResult(mozIStorageResultSet *aResultSet)
{
  // We could get results from PRAGMA statements, but we don't mind them.
  return NS_OK;
}

NS_IMETHODIMP
BaseCallback::HandleCompletion(PRUint16 aReason)
{
  // By default BaseCallback will just be silent on completion.
  return NS_OK;
}

NS_IMPL_ISUPPORTS1(
  BaseCallback
, mozIStorageStatementCallback
)

//////////////////////////////////////////////////////////////////////////////// 
//// Vacuumer declaration.

class Vacuumer : public BaseCallback
{
public:
  NS_DECL_MOZISTORAGESTATEMENTCALLBACK

  Vacuumer(mozIStorageVacuumParticipant *aParticipant);

  bool execute();
  nsresult notifyCompletion(bool aSucceeded);

private:
  nsCOMPtr<mozIStorageVacuumParticipant> mParticipant;
  nsCString mDBFilename;
  nsCOMPtr<mozIStorageConnection> mDBConn;
};

////////////////////////////////////////////////////////////////////////////////
// NotifyCallback
/**
 * This class handles an async statement execution and notifies completion
 * through a passed in callback.
 * Errors are handled through warnings in HandleError.
 */
class NotifyCallback : public BaseCallback
{
public:
  NS_IMETHOD HandleCompletion(PRUint16 aReason);

  NotifyCallback(Vacuumer *aVacuumer, bool aVacuumSucceeded);

private:
  nsCOMPtr<Vacuumer> mVacuumer;
  bool mVacuumSucceeded;
};

NotifyCallback::NotifyCallback(Vacuumer *aVacuumer,
                               bool aVacuumSucceeded)
  : mVacuumer(aVacuumer)
  , mVacuumSucceeded(aVacuumSucceeded)
{
}

NS_IMETHODIMP
NotifyCallback::HandleCompletion(PRUint16 aReason)
{
  // We succeeded if vacuum succeeded.
  nsresult rv = mVacuumer->notifyCompletion(mVacuumSucceeded);
  NS_ENSURE_SUCCESS(rv, rv);
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
//// Vacuumer implementation.

Vacuumer::Vacuumer(mozIStorageVacuumParticipant *aParticipant)
  : mParticipant(aParticipant)
{
}

bool
Vacuumer::execute()
{
  NS_PRECONDITION(NS_IsMainThread(), "Must be running on the main thread!");

  // Get the connection and check its validity.
  nsresult rv = mParticipant->GetDatabaseConnection(getter_AddRefs(mDBConn));
  NS_ENSURE_SUCCESS(rv, false);
  bool ready;
  if (!mDBConn || NS_FAILED(mDBConn->GetConnectionReady(&ready)) || !ready) {
    NS_WARNING(NS_LITERAL_CSTRING("Unable to get a connection to vacuum database").get());
    return false;
  }

  // Compare current page size with the expected one.  Vacuum can change the
  // page size value if needed.  Even if a vacuum happened recently, if the
  // page size can be optimized we should do it.
  PRInt32 expectedPageSize = 0;
  rv = mParticipant->GetExpectedDatabasePageSize(&expectedPageSize);
  if (NS_FAILED(rv) || expectedPageSize < 512 || expectedPageSize > 65536) {
    NS_WARNING("Invalid page size requested for database, will use default ");
    NS_WARNING(mDBFilename.get());
    expectedPageSize = mozIStorageConnection::DEFAULT_PAGE_SIZE;
  }

  bool canOptimizePageSize = false;
  {
    nsCOMPtr<mozIStorageStatement> stmt;
    rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
      "PRAGMA page_size"
    ), getter_AddRefs(stmt));
    NS_ENSURE_SUCCESS(rv, false);
    bool hasResult;
    rv = stmt->ExecuteStep(&hasResult);
    NS_ENSURE_SUCCESS(rv, false);
    NS_ENSURE_TRUE(hasResult, false);
    PRInt32 currentPageSize;
    rv = stmt->GetInt32(0, &currentPageSize);
    NS_ENSURE_SUCCESS(rv, false);
    NS_ASSERTION(currentPageSize > 0, "Got invalid page size value?");
    if (currentPageSize != expectedPageSize) {
      // Check journal mode.  WAL journaling does not allow vacuum to change
      // the page size.
      // TODO Bug 634374: figure out a strategy to fix page size with WAL.
      nsCAutoString journalMode;
      {
        nsCOMPtr<mozIStorageStatement> stmt;
        rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
          "PRAGMA journal_mode"
        ), getter_AddRefs(stmt));
        NS_ENSURE_SUCCESS(rv, false);
        bool hasResult;
        rv = stmt->ExecuteStep(&hasResult);
        NS_ENSURE_SUCCESS(rv, false);
        NS_ENSURE_TRUE(hasResult, false);
        rv = stmt->GetUTF8String(0, journalMode);
        NS_ENSURE_SUCCESS(rv, false);
      }
      canOptimizePageSize = !journalMode.EqualsLiteral("wal");
    }
  }

  // Get the database filename.  Last vacuum time is stored under this name
  // in PREF_VACUUM_BRANCH.
  nsCOMPtr<nsIFile> databaseFile;
  mDBConn->GetDatabaseFile(getter_AddRefs(databaseFile));
  if (!databaseFile) {
    NS_WARNING("Trying to vacuum a in-memory database!");
    return false;
  }
  nsAutoString databaseFilename;
  rv = databaseFile->GetLeafName(databaseFilename);
  NS_ENSURE_SUCCESS(rv, false);
  mDBFilename = NS_ConvertUTF16toUTF8(databaseFilename);
  NS_ASSERTION(!mDBFilename.IsEmpty(), "Database filename cannot be empty");

  // Check interval from last vacuum.
  PRInt32 now = static_cast<PRInt32>(PR_Now() / PR_USEC_PER_SEC);
  PRInt32 lastVacuum;
  nsCAutoString prefName(PREF_VACUUM_BRANCH);
  prefName += mDBFilename;
  rv = Preferences::GetInt(prefName.get(), &lastVacuum);
  if (NS_SUCCEEDED(rv) && (now - lastVacuum) < VACUUM_INTERVAL_SECONDS &&
      !canOptimizePageSize) {
    // This database was vacuumed recently and has optimal page size, skip it. 
    return false;
  }

  // Notify that we are about to start vacuuming.  The participant can opt-out
  // if it cannot handle a vacuum at this time, and then we'll move to the next
  // one.
  bool vacuumGranted = false;
  rv = mParticipant->OnBeginVacuum(&vacuumGranted);
  NS_ENSURE_SUCCESS(rv, false);
  if (!vacuumGranted) {
    return false;
  }

  // Notify a heavy IO task is about to start.
  nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
  if (os) {
    (void)os->NotifyObservers(nsnull, OBSERVER_TOPIC_HEAVY_IO,
                              OBSERVER_DATA_VACUUM_BEGIN.get());
  }

  if (canOptimizePageSize) {
    nsCOMPtr<mozIStorageAsyncStatement> pageSizeStmt;
    rv = mDBConn->CreateAsyncStatement(nsPrintfCString(
      "PRAGMA page_size = %ld", expectedPageSize
    ), getter_AddRefs(pageSizeStmt));
    NS_ENSURE_SUCCESS(rv, false);
    nsCOMPtr<BaseCallback> callback = new BaseCallback();
    NS_ENSURE_TRUE(callback, false);
    nsCOMPtr<mozIStoragePendingStatement> ps;
    rv = pageSizeStmt->ExecuteAsync(callback, getter_AddRefs(ps));
    NS_ENSURE_SUCCESS(rv, false);
  }

  nsCOMPtr<mozIStorageAsyncStatement> stmt;
  rv = mDBConn->CreateAsyncStatement(NS_LITERAL_CSTRING(
    "VACUUM"
  ), getter_AddRefs(stmt));
  NS_ENSURE_SUCCESS(rv, false);

  nsCOMPtr<mozIStoragePendingStatement> ps;
  rv = stmt->ExecuteAsync(this, getter_AddRefs(ps));
  NS_ENSURE_SUCCESS(rv, false);

  return true;
}

////////////////////////////////////////////////////////////////////////////////
//// mozIStorageStatementCallback

NS_IMETHODIMP
Vacuumer::HandleError(mozIStorageError *aError)
{
#ifdef DEBUG
  PRInt32 result;
  nsresult rv = aError->GetResult(&result);
  NS_ENSURE_SUCCESS(rv, rv);
  nsCAutoString message;
  rv = aError->GetMessage(message);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCAutoString warnMsg;
  warnMsg.AppendLiteral("Unable to vacuum database: ");
  warnMsg.Append(mDBFilename);
  warnMsg.AppendLiteral(" - ");
  warnMsg.AppendInt(result);
  warnMsg.AppendLiteral(" ");
  warnMsg.Append(message);
  NS_WARNING(warnMsg.get());
#endif

#ifdef PR_LOGGING
  {
    PRInt32 result;
    nsresult rv = aError->GetResult(&result);
    NS_ENSURE_SUCCESS(rv, rv);
    nsCAutoString message;
    rv = aError->GetMessage(message);
    NS_ENSURE_SUCCESS(rv, rv);
    PR_LOG(gStorageLog, PR_LOG_ERROR,
           ("Vacuum failed with error: %d '%s'. Database was: '%s'",
            result, message.get(), mDBFilename.get()));
  }
#endif
  return NS_OK;
}

NS_IMETHODIMP
Vacuumer::HandleResult(mozIStorageResultSet *aResultSet)
{
  NS_NOTREACHED("Got a resultset from a vacuum?");
  return NS_OK;
}

NS_IMETHODIMP
Vacuumer::HandleCompletion(PRUint16 aReason)
{
  if (aReason == REASON_FINISHED) {
    // Update last vacuum time.
    PRInt32 now = static_cast<PRInt32>(PR_Now() / PR_USEC_PER_SEC);
    NS_ASSERTION(!mDBFilename.IsEmpty(), "Database filename cannot be empty");
    nsCAutoString prefName(PREF_VACUUM_BRANCH);
    prefName += mDBFilename;
    (void)Preferences::SetInt(prefName.get(), now);
  }

  notifyCompletion(aReason == REASON_FINISHED);

  return NS_OK;
}

nsresult
Vacuumer::notifyCompletion(bool aSucceeded)
{
  nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
  if (os) {
    os->NotifyObservers(nsnull, OBSERVER_TOPIC_HEAVY_IO,
                        OBSERVER_DATA_VACUUM_END.get());
  }

  nsresult rv = mParticipant->OnEndVacuum(aSucceeded);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

} // Anonymous namespace.

////////////////////////////////////////////////////////////////////////////////
//// VacuumManager

NS_IMPL_ISUPPORTS1(
  VacuumManager
, nsIObserver
)

VacuumManager *
VacuumManager::gVacuumManager = nsnull;

VacuumManager *
VacuumManager::getSingleton()
{
  if (gVacuumManager) {
    NS_ADDREF(gVacuumManager);
    return gVacuumManager;
  }
  gVacuumManager = new VacuumManager();
  if (gVacuumManager) {
    NS_ADDREF(gVacuumManager);
  }
  return gVacuumManager;
}

VacuumManager::VacuumManager()
  : mParticipants("vacuum-participant")
{
  NS_ASSERTION(!gVacuumManager,
               "Attempting to create two instances of the service!");
  gVacuumManager = this;
}

VacuumManager::~VacuumManager()
{
  // Remove the static reference to the service.  Check to make sure its us
  // in case somebody creates an extra instance of the service.
  NS_ASSERTION(gVacuumManager == this,
               "Deleting a non-singleton instance of the service");
  if (gVacuumManager == this) {
    gVacuumManager = nsnull;
  }
}

////////////////////////////////////////////////////////////////////////////////
//// nsIObserver

NS_IMETHODIMP
VacuumManager::Observe(nsISupports *aSubject,
                       const char *aTopic,
                       const PRUnichar *aData)
{
  if (strcmp(aTopic, OBSERVER_TOPIC_IDLE_DAILY) == 0) {
    // Try to run vacuum on all registered entries.  Will stop at the first
    // successful one.
    const nsCOMArray<mozIStorageVacuumParticipant> &entries =
      mParticipants.GetEntries();
    // If there are more entries than what a month can contain, we could end up
    // skipping some, since we run daily.  So we use a starting index.
    static const char* kPrefName = PREF_VACUUM_BRANCH "index";
    PRInt32 startIndex = Preferences::GetInt(kPrefName, 0);
    if (startIndex >= entries.Count()) {
      startIndex = 0;
    }
    PRInt32 index;
    for (index = startIndex; index < entries.Count(); ++index) {
      nsCOMPtr<Vacuumer> vacuum = new Vacuumer(entries[index]);
      // Only vacuum one database per day.
      if (vacuum->execute()) {
        break;
      }
    }
    (void)Preferences::SetInt(kPrefName, index);
  }

  return NS_OK;
}

} // namespace storage
} // namespace mozilla
