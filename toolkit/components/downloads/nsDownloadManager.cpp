/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Blake Ross <blaker@netscape.com> (Original Author)
 *   Ben Goodger <ben@netscape.com> (Original Author)
 *   Shawn Wilsher <me@shawnwilsher.com>
 *   Srirang G Doddihal <brahmana@doddihal.com>
 *   Edward Lee <edward.lee@engineering.uiuc.edu>
 *   Graeme McCutcheon <graememcc_firefox@graeme-online.co.uk>
 *   Ehsan Akhgari <ehsan.akhgari@gmail.com>
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

#include "mozilla/Util.h"

#include "mozIStorageService.h"
#include "nsIAlertsService.h"
#include "nsIClassInfoImpl.h"
#include "nsIDOMWindow.h"
#include "nsIDownloadHistory.h"
#include "nsIDownloadManagerUI.h"
#include "nsIMIMEService.h"
#include "nsIParentalControlsService.h"
#include "nsIPrefService.h"
#include "nsIPrivateBrowsingService.h"
#include "nsIPromptService.h"
#include "nsIResumableChannel.h"
#include "nsIWebBrowserPersist.h"
#include "nsIWindowMediator.h"
#include "nsILocalFileWin.h"

#include "nsAppDirectoryServiceDefs.h"
#include "nsArrayEnumerator.h"
#include "nsCExternalHandlerService.h"
#include "nsDirectoryServiceDefs.h"
#include "nsDownloadManager.h"
#include "nsNetUtil.h"

#include "mozStorageCID.h"
#include "nsDocShellCID.h"
#include "nsEmbedCID.h"
#include "nsToolkitCompsCID.h"

#ifdef XP_WIN
#include <shlobj.h>
#ifdef DOWNLOAD_SCANNER
#include "nsDownloadScanner.h"
#endif
#endif

#ifdef XP_MACOSX
#include <CoreFoundation/CoreFoundation.h>
#endif

#ifdef ANDROID
#include "AndroidBridge.h"
#endif

using namespace mozilla;

#define DOWNLOAD_MANAGER_BUNDLE "chrome://mozapps/locale/downloads/downloads.properties"
#define DOWNLOAD_MANAGER_ALERT_ICON "chrome://mozapps/skin/downloads/downloadIcon.png"
#define PREF_BDM_SHOWALERTONCOMPLETE "browser.download.manager.showAlertOnComplete"
#define PREF_BDM_SHOWALERTINTERVAL "browser.download.manager.showAlertInterval"
#define PREF_BDM_RETENTION "browser.download.manager.retention"
#define PREF_BDM_QUITBEHAVIOR "browser.download.manager.quitBehavior"
#define PREF_BDM_ADDTORECENTDOCS "browser.download.manager.addToRecentDocs"
#define PREF_BDM_SCANWHENDONE "browser.download.manager.scanWhenDone"
#define PREF_BDM_RESUMEONWAKEDELAY "browser.download.manager.resumeOnWakeDelay"
#define PREF_BH_DELETETEMPFILEONEXIT "browser.helperApps.deleteTempFileOnExit"

static const PRInt64 gUpdateInterval = 400 * PR_USEC_PER_MSEC;

#define DM_SCHEMA_VERSION      8
#define DM_DB_NAME             NS_LITERAL_STRING("downloads.sqlite")
#define DM_DB_CORRUPT_FILENAME NS_LITERAL_STRING("downloads.sqlite.corrupt")

#define NS_SYSTEMINFO_CONTRACTID "@mozilla.org/system-info;1"

////////////////////////////////////////////////////////////////////////////////
//// nsDownloadManager

NS_IMPL_ISUPPORTS3(
  nsDownloadManager
, nsIDownloadManager
, nsINavHistoryObserver
, nsIObserver
)

nsDownloadManager *nsDownloadManager::gDownloadManagerService = nsnull;

nsDownloadManager *
nsDownloadManager::GetSingleton()
{
  if (gDownloadManagerService) {
    NS_ADDREF(gDownloadManagerService);
    return gDownloadManagerService;
  }

  gDownloadManagerService = new nsDownloadManager();
  if (gDownloadManagerService) {
    NS_ADDREF(gDownloadManagerService);
    if (NS_FAILED(gDownloadManagerService->Init()))
      NS_RELEASE(gDownloadManagerService);
  }

  return gDownloadManagerService;
}

nsDownloadManager::~nsDownloadManager()
{
#ifdef DOWNLOAD_SCANNER
  if (mScanner) {
    delete mScanner;
    mScanner = nsnull;
  }
#endif
  gDownloadManagerService = nsnull;
}

nsresult
nsDownloadManager::ResumeRetry(nsDownload *aDl)
{
  // Keep a reference in case we need to cancel the download
  nsRefPtr<nsDownload> dl = aDl;

  // Try to resume the active download
  nsresult rv = dl->Resume();

  // If not, try to retry the download
  if (NS_FAILED(rv)) {
    // First cancel the download so it's no longer active
    rv = CancelDownload(dl->mID);

    // Then retry it
    if (NS_SUCCEEDED(rv))
      rv = RetryDownload(dl->mID);
  }

  return rv;
}

nsresult
nsDownloadManager::PauseAllDownloads(bool aSetResume)
{
  nsresult retVal = NS_OK;
  for (PRInt32 i = mCurrentDownloads.Count() - 1; i >= 0; --i) {
    nsRefPtr<nsDownload> dl = mCurrentDownloads[i];

    // Only pause things that need to be paused
    if (!dl->IsPaused()) {
      // Set auto-resume before pausing so that it gets into the DB
      dl->mAutoResume = aSetResume ? nsDownload::AUTO_RESUME :
                                     nsDownload::DONT_RESUME;

      // Try to pause the download but don't bail now if we fail
      nsresult rv = dl->Pause();
      if (NS_FAILED(rv))
        retVal = rv;
    }
  }

  return retVal;
}

nsresult
nsDownloadManager::ResumeAllDownloads(bool aResumeAll)
{
  nsresult retVal = NS_OK;
  for (PRInt32 i = mCurrentDownloads.Count() - 1; i >= 0; --i) {
    nsRefPtr<nsDownload> dl = mCurrentDownloads[i];

    // If aResumeAll is true, then resume everything; otherwise, check if the
    // download should auto-resume
    if (aResumeAll || dl->ShouldAutoResume()) {
      // Reset auto-resume before retrying so that it gets into the DB through
      // ResumeRetry's eventual call to SetState. We clear the value now so we
      // don't accidentally query completed downloads that were previously
      // auto-resumed (and try to resume them).
      dl->mAutoResume = nsDownload::DONT_RESUME;

      // Try to resume/retry the download but don't bail now if we fail
      nsresult rv = ResumeRetry(dl);
      if (NS_FAILED(rv))
        retVal = rv;
    }
  }

  return retVal;
}

nsresult
nsDownloadManager::RemoveAllDownloads()
{
  nsresult rv = NS_OK;
  for (PRInt32 i = mCurrentDownloads.Count() - 1; i >= 0; --i) {
    nsRefPtr<nsDownload> dl = mCurrentDownloads[0];

    nsresult result;
    if (dl->IsPaused() && GetQuitBehavior() != QUIT_AND_CANCEL)
      result = mCurrentDownloads.RemoveObject(dl);
    else
      result = CancelDownload(dl->mID);

    // Track the failure, but don't miss out on other downloads
    if (NS_FAILED(result))
      rv = result;
  }

  return rv;
}

nsresult
nsDownloadManager::RemoveDownloadsForURI(nsIURI *aURI)
{
  mozStorageStatementScoper scope(mGetIdsForURIStatement);

  nsCAutoString source;
  nsresult rv = aURI->GetSpec(source);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mGetIdsForURIStatement->BindUTF8StringByName(
    NS_LITERAL_CSTRING("source"), source);
  NS_ENSURE_SUCCESS(rv, rv);

  bool hasMore = false;
  nsAutoTArray<PRInt64, 4> downloads;
  // Get all the downloads that match the provided URI
  while (NS_SUCCEEDED(mGetIdsForURIStatement->ExecuteStep(&hasMore)) &&
         hasMore) {
    PRInt64 downloadId;
    rv = mGetIdsForURIStatement->GetInt64(0, &downloadId);
    NS_ENSURE_SUCCESS(rv, rv);

    downloads.AppendElement(downloadId);
  }

  // Remove each download ignoring any failure so we reach other downloads
  for (PRInt32 i = downloads.Length(); --i >= 0; )
    (void)RemoveDownload(downloads[i]);

  return NS_OK;
}

void // static
nsDownloadManager::ResumeOnWakeCallback(nsITimer *aTimer, void *aClosure)
{
  // Resume the downloads that were set to autoResume
  nsDownloadManager *dlMgr = static_cast<nsDownloadManager *>(aClosure);
  (void)dlMgr->ResumeAllDownloads(false);
}

already_AddRefed<mozIStorageConnection>
nsDownloadManager::GetFileDBConnection(nsIFile *dbFile) const
{
  NS_ASSERTION(dbFile, "GetFileDBConnection called with an invalid nsIFile");

  nsCOMPtr<mozIStorageService> storage =
    do_GetService(MOZ_STORAGE_SERVICE_CONTRACTID);
  NS_ENSURE_TRUE(storage, nsnull);

  nsCOMPtr<mozIStorageConnection> conn;
  nsresult rv = storage->OpenDatabase(dbFile, getter_AddRefs(conn));
  if (rv == NS_ERROR_FILE_CORRUPTED) {
    // delete and try again, since we don't care so much about losing a user's
    // download history
    rv = dbFile->Remove(false);
    NS_ENSURE_SUCCESS(rv, nsnull);
    rv = storage->OpenDatabase(dbFile, getter_AddRefs(conn));
  }
  NS_ENSURE_SUCCESS(rv, nsnull);

  return conn.forget();
}

already_AddRefed<mozIStorageConnection>
nsDownloadManager::GetMemoryDBConnection() const
{
  nsCOMPtr<mozIStorageService> storage =
    do_GetService(MOZ_STORAGE_SERVICE_CONTRACTID);
  NS_ENSURE_TRUE(storage, nsnull);

  nsCOMPtr<mozIStorageConnection> conn;
  nsresult rv = storage->OpenSpecialDatabase("memory", getter_AddRefs(conn));
  NS_ENSURE_SUCCESS(rv, nsnull);

  return conn.forget();
}

nsresult
nsDownloadManager::InitMemoryDB()
{
  mDBConn = GetMemoryDBConnection();
  if (!mDBConn)
    return NS_ERROR_NOT_AVAILABLE;

  nsresult rv = CreateTable();
  NS_ENSURE_SUCCESS(rv, rv);

  mDBType = DATABASE_MEMORY;
  return NS_OK;
}

nsresult
nsDownloadManager::InitFileDB()
{
  nsresult rv;

  nsCOMPtr<nsIFile> dbFile;
  rv = NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR,
                              getter_AddRefs(dbFile));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = dbFile->Append(DM_DB_NAME);
  NS_ENSURE_SUCCESS(rv, rv);

  mDBConn = GetFileDBConnection(dbFile);
  NS_ENSURE_TRUE(mDBConn, NS_ERROR_NOT_AVAILABLE);

  bool tableExists;
  rv = mDBConn->TableExists(NS_LITERAL_CSTRING("moz_downloads"), &tableExists);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!tableExists) {
    rv = CreateTable();
    NS_ENSURE_SUCCESS(rv, rv);
    mDBType = DATABASE_DISK;
    return NS_OK;
  }

  mDBType = DATABASE_DISK;

  // Checking the database schema now
  PRInt32 schemaVersion;
  rv = mDBConn->GetSchemaVersion(&schemaVersion);
  NS_ENSURE_SUCCESS(rv, rv);

  // Changing the database?  Be sure to do these two things!
  // 1) Increment DM_SCHEMA_VERSION
  // 2) Implement the proper downgrade/upgrade code for the current version

  switch (schemaVersion) {
  // Upgrading
  // Every time you increment the database schema, you need to implement
  // the upgrading code from the previous version to the new one.
  // Also, don't forget to make a unit test to test your upgrading code!
  case 1: // Drop a column (iconURL) from the database (bug 385875)
    {
      // Safely wrap this in a transaction so we don't hose the whole DB
      mozStorageTransaction safeTransaction(mDBConn, true);

      // Create a temporary table that will store the existing records
      rv = mDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
        "CREATE TEMPORARY TABLE moz_downloads_backup ("
          "id INTEGER PRIMARY KEY, "
          "name TEXT, "
          "source TEXT, "
          "target TEXT, "
          "startTime INTEGER, "
          "endTime INTEGER, "
          "state INTEGER"
        ")"));
      NS_ENSURE_SUCCESS(rv, rv);

      // Insert into a temporary table
      rv = mDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
        "INSERT INTO moz_downloads_backup "
        "SELECT id, name, source, target, startTime, endTime, state "
        "FROM moz_downloads"));
      NS_ENSURE_SUCCESS(rv, rv);

      // Drop the old table
      rv = mDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
        "DROP TABLE moz_downloads"));
      NS_ENSURE_SUCCESS(rv, rv);

      // Now recreate it with this schema version
      rv = mDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
        "CREATE TABLE moz_downloads ("
          "id INTEGER PRIMARY KEY, "
          "name TEXT, "
          "source TEXT, "
          "target TEXT, "
          "startTime INTEGER, "
          "endTime INTEGER, "
          "state INTEGER"
        ")"));
      NS_ENSURE_SUCCESS(rv, rv);

      // Insert the data back into it
      rv = mDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
        "INSERT INTO moz_downloads "
        "SELECT id, name, source, target, startTime, endTime, state "
        "FROM moz_downloads_backup"));
      NS_ENSURE_SUCCESS(rv, rv);

      // And drop our temporary table
      rv = mDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
        "DROP TABLE moz_downloads_backup"));
      NS_ENSURE_SUCCESS(rv, rv);

      // Finally, update the schemaVersion variable and the database schema
      schemaVersion = 2;
      rv = mDBConn->SetSchemaVersion(schemaVersion);
      NS_ENSURE_SUCCESS(rv, rv);
    }
    // Fallthrough to the next upgrade

  case 2: // Add referrer column to the database
    {
      rv = mDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
        "ALTER TABLE moz_downloads "
        "ADD COLUMN referrer TEXT"));
      NS_ENSURE_SUCCESS(rv, rv);

      // Finally, update the schemaVersion variable and the database schema
      schemaVersion = 3;
      rv = mDBConn->SetSchemaVersion(schemaVersion);
      NS_ENSURE_SUCCESS(rv, rv);
    }
    // Fallthrough to the next upgrade

  case 3: // This version adds a column to the database (entityID)
    {
      rv = mDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
        "ALTER TABLE moz_downloads "
        "ADD COLUMN entityID TEXT"));
      NS_ENSURE_SUCCESS(rv, rv);

      // Finally, update the schemaVersion variable and the database schema
      schemaVersion = 4;
      rv = mDBConn->SetSchemaVersion(schemaVersion);
      NS_ENSURE_SUCCESS(rv, rv);
    }
    // Fallthrough to the next upgrade

  case 4: // This version adds a column to the database (tempPath)
    {
      rv = mDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
        "ALTER TABLE moz_downloads "
        "ADD COLUMN tempPath TEXT"));
      NS_ENSURE_SUCCESS(rv, rv);

      // Finally, update the schemaVersion variable and the database schema
      schemaVersion = 5;
      rv = mDBConn->SetSchemaVersion(schemaVersion);
      NS_ENSURE_SUCCESS(rv, rv);
    }
    // Fallthrough to the next upgrade

  case 5: // This version adds two columns for tracking transfer progress
    {
      rv = mDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
        "ALTER TABLE moz_downloads "
        "ADD COLUMN currBytes INTEGER NOT NULL DEFAULT 0"));
      NS_ENSURE_SUCCESS(rv, rv);

      rv = mDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
        "ALTER TABLE moz_downloads "
        "ADD COLUMN maxBytes INTEGER NOT NULL DEFAULT -1"));
      NS_ENSURE_SUCCESS(rv, rv);

      // Finally, update the schemaVersion variable and the database schema
      schemaVersion = 6;
      rv = mDBConn->SetSchemaVersion(schemaVersion);
      NS_ENSURE_SUCCESS(rv, rv);
    }
    // Fallthrough to the next upgrade

  case 6: // This version adds three columns to DB (MIME type related info)
    {
      rv = mDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
        "ALTER TABLE moz_downloads "
        "ADD COLUMN mimeType TEXT"));
      NS_ENSURE_SUCCESS(rv, rv);

      rv = mDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
        "ALTER TABLE moz_downloads "
        "ADD COLUMN preferredApplication TEXT"));
      NS_ENSURE_SUCCESS(rv, rv);

      rv = mDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
        "ALTER TABLE moz_downloads "
        "ADD COLUMN preferredAction INTEGER NOT NULL DEFAULT 0"));
      NS_ENSURE_SUCCESS(rv, rv);

      // Finally, update the schemaVersion variable and the database schema
      schemaVersion = 7;
      rv = mDBConn->SetSchemaVersion(schemaVersion);
      NS_ENSURE_SUCCESS(rv, rv);
    }
    // Fallthrough to next upgrade

  case 7: // This version adds a column to remember to auto-resume downloads
    {
      rv = mDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
        "ALTER TABLE moz_downloads "
        "ADD COLUMN autoResume INTEGER NOT NULL DEFAULT 0"));
      NS_ENSURE_SUCCESS(rv, rv);

      // Finally, update the schemaVersion variable and the database schema
      schemaVersion = 8;
      rv = mDBConn->SetSchemaVersion(schemaVersion);
      NS_ENSURE_SUCCESS(rv, rv);
    }
    // Fallthrough to the next upgrade

  // Extra sanity checking for developers
#ifndef DEBUG
  case DM_SCHEMA_VERSION:
#endif
    break;

  case 0:
    {
      NS_WARNING("Could not get download database's schema version!");

      // The table may still be usable - someone may have just messed with the
      // schema version, so let's just treat this like a downgrade and verify
      // that the needed columns are there.  If they aren't there, we'll drop
      // the table anyway.
      rv = mDBConn->SetSchemaVersion(DM_SCHEMA_VERSION);
      NS_ENSURE_SUCCESS(rv, rv);
    }
    // Fallthrough to downgrade check

  // Downgrading
  // If columns have been added to the table, we can still use the ones we
  // understand safely.  If columns have been deleted or alterd, we just
  // drop the table and start from scratch.  If you change how a column
  // should be interpreted, make sure you also change its name so this
  // check will catch it.
  default:
    {
      nsCOMPtr<mozIStorageStatement> stmt;
      rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
        "SELECT id, name, source, target, tempPath, startTime, endTime, state, "
               "referrer, entityID, currBytes, maxBytes, mimeType, "
               "preferredApplication, preferredAction, autoResume "
        "FROM moz_downloads"), getter_AddRefs(stmt));
      if (NS_SUCCEEDED(rv))
        break;

      // if the statement fails, that means all the columns were not there.
      // First we backup the database
      nsCOMPtr<mozIStorageService> storage =
        do_GetService(MOZ_STORAGE_SERVICE_CONTRACTID);
      NS_ENSURE_TRUE(storage, NS_ERROR_NOT_AVAILABLE);
      nsCOMPtr<nsIFile> backup;
      rv = storage->BackupDatabaseFile(dbFile, DM_DB_CORRUPT_FILENAME, nsnull,
                                       getter_AddRefs(backup));
      NS_ENSURE_SUCCESS(rv, rv);

      // Then we dump it
      rv = mDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
        "DROP TABLE moz_downloads"));
      NS_ENSURE_SUCCESS(rv, rv);

      rv = CreateTable();
      NS_ENSURE_SUCCESS(rv, rv);
    }
    break;
  }

  return NS_OK;
}

nsresult
nsDownloadManager::CreateTable()
{
  nsresult rv = mDBConn->SetSchemaVersion(DM_SCHEMA_VERSION);
  if (NS_FAILED(rv)) return rv;

  return mDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "CREATE TABLE moz_downloads ("
      "id INTEGER PRIMARY KEY, "
      "name TEXT, "
      "source TEXT, "
      "target TEXT, "
      "tempPath TEXT, "
      "startTime INTEGER, "
      "endTime INTEGER, "
      "state INTEGER, "
      "referrer TEXT, "
      "entityID TEXT, "
      "currBytes INTEGER NOT NULL DEFAULT 0, "
      "maxBytes INTEGER NOT NULL DEFAULT -1, "
      "mimeType TEXT, "
      "preferredApplication TEXT, "
      "preferredAction INTEGER NOT NULL DEFAULT 0, "
      "autoResume INTEGER NOT NULL DEFAULT 0"
    ")"));
}

nsresult
nsDownloadManager::RestoreDatabaseState()
{
  // Restore downloads that were in a scanning state.  We can assume that they
  // have been dealt with by the virus scanner
  nsCOMPtr<mozIStorageStatement> stmt;
  nsresult rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
    "UPDATE moz_downloads "
    "SET state = :state "
    "WHERE state = :state_cond"), getter_AddRefs(stmt));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = stmt->BindInt32ByName(NS_LITERAL_CSTRING("state"), nsIDownloadManager::DOWNLOAD_FINISHED);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = stmt->BindInt32ByName(NS_LITERAL_CSTRING("state_cond"), nsIDownloadManager::DOWNLOAD_SCANNING);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = stmt->Execute();
  NS_ENSURE_SUCCESS(rv, rv);

  // Convert supposedly-active downloads into downloads that should auto-resume
  rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
    "UPDATE moz_downloads "
    "SET autoResume = :autoResume "
    "WHERE state = :notStarted "
      "OR state = :queued "
      "OR state = :downloading"), getter_AddRefs(stmt));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = stmt->BindInt32ByName(NS_LITERAL_CSTRING("autoResume"), nsDownload::AUTO_RESUME);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = stmt->BindInt32ByName(NS_LITERAL_CSTRING("notStarted"), nsIDownloadManager::DOWNLOAD_NOTSTARTED);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = stmt->BindInt32ByName(NS_LITERAL_CSTRING("queued"), nsIDownloadManager::DOWNLOAD_QUEUED);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = stmt->BindInt32ByName(NS_LITERAL_CSTRING("downloading"), nsIDownloadManager::DOWNLOAD_DOWNLOADING);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = stmt->Execute();
  NS_ENSURE_SUCCESS(rv, rv);

  // Switch any download that is supposed to automatically resume and is in a
  // finished state to *not* automatically resume.  See Bug 409179 for details.
  rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
    "UPDATE moz_downloads "
    "SET autoResume = :autoResume "
    "WHERE state = :state "
      "AND autoResume = :autoResume_cond"),
    getter_AddRefs(stmt));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = stmt->BindInt32ByName(NS_LITERAL_CSTRING("autoResume"), nsDownload::DONT_RESUME);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = stmt->BindInt32ByName(NS_LITERAL_CSTRING("state"), nsIDownloadManager::DOWNLOAD_FINISHED);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = stmt->BindInt32ByName(NS_LITERAL_CSTRING("autoResume_cond"), nsDownload::AUTO_RESUME);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = stmt->Execute();
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
nsDownloadManager::RestoreActiveDownloads()
{
  nsCOMPtr<mozIStorageStatement> stmt;
  nsresult rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
    "SELECT id "
    "FROM moz_downloads "
    "WHERE (state = :state AND LENGTH(entityID) > 0) "
      "OR autoResume != :autoResume"), getter_AddRefs(stmt));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = stmt->BindInt32ByName(NS_LITERAL_CSTRING("state"), nsIDownloadManager::DOWNLOAD_PAUSED);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = stmt->BindInt32ByName(NS_LITERAL_CSTRING("autoResume"), nsDownload::DONT_RESUME);
  NS_ENSURE_SUCCESS(rv, rv);

  nsresult retVal = NS_OK;
  bool hasResults;
  while (NS_SUCCEEDED(stmt->ExecuteStep(&hasResults)) && hasResults) {
    nsRefPtr<nsDownload> dl;
    // Keep trying to add even if we fail one, but make sure to return failure.
    // Additionally, be careful to not call anything that tries to change the
    // database because we're iterating over a live statement.
    if (NS_FAILED(GetDownloadFromDB(stmt->AsInt32(0), getter_AddRefs(dl))) ||
        NS_FAILED(AddToCurrentDownloads(dl)))
      retVal = NS_ERROR_FAILURE;
  }

  // Try to resume only the downloads that should auto-resume
  rv = ResumeAllDownloads(false);
  NS_ENSURE_SUCCESS(rv, rv);

  return retVal;
}

PRInt64
nsDownloadManager::AddDownloadToDB(const nsAString &aName,
                                   const nsACString &aSource,
                                   const nsACString &aTarget,
                                   const nsAString &aTempPath,
                                   PRInt64 aStartTime,
                                   PRInt64 aEndTime,
                                   const nsACString &aMimeType,
                                   const nsACString &aPreferredApp,
                                   nsHandlerInfoAction aPreferredAction)
{
  nsCOMPtr<mozIStorageStatement> stmt;
  nsresult rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
    "INSERT INTO moz_downloads "
    "(name, source, target, tempPath, startTime, endTime, state, "
     "mimeType, preferredApplication, preferredAction) VALUES "
    "(:name, :source, :target, :tempPath, :startTime, :endTime, :state, "
     ":mimeType, :preferredApplication, :preferredAction)"),
    getter_AddRefs(stmt));
  NS_ENSURE_SUCCESS(rv, 0);

  rv = stmt->BindStringByName(NS_LITERAL_CSTRING("name"), aName);
  NS_ENSURE_SUCCESS(rv, 0);

  rv = stmt->BindUTF8StringByName(NS_LITERAL_CSTRING("source"), aSource);
  NS_ENSURE_SUCCESS(rv, 0);

  rv = stmt->BindUTF8StringByName(NS_LITERAL_CSTRING("target"), aTarget);
  NS_ENSURE_SUCCESS(rv, 0);

  rv = stmt->BindStringByName(NS_LITERAL_CSTRING("tempPath"), aTempPath);
  NS_ENSURE_SUCCESS(rv, 0);

  rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("startTime"), aStartTime);
  NS_ENSURE_SUCCESS(rv, 0);

  rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("endTime"), aEndTime);
  NS_ENSURE_SUCCESS(rv, 0);

  rv = stmt->BindInt32ByName(NS_LITERAL_CSTRING("state"), nsIDownloadManager::DOWNLOAD_NOTSTARTED);
  NS_ENSURE_SUCCESS(rv, 0);

  rv = stmt->BindUTF8StringByName(NS_LITERAL_CSTRING("mimeType"), aMimeType);
  NS_ENSURE_SUCCESS(rv, 0);

  rv = stmt->BindUTF8StringByName(NS_LITERAL_CSTRING("preferredApplication"), aPreferredApp);
  NS_ENSURE_SUCCESS(rv, 0);

  rv = stmt->BindInt32ByName(NS_LITERAL_CSTRING("preferredAction"), aPreferredAction);
  NS_ENSURE_SUCCESS(rv, 0);

  bool hasMore;
  rv = stmt->ExecuteStep(&hasMore); // we want to keep our lock
  NS_ENSURE_SUCCESS(rv, 0);

  PRInt64 id = 0;
  rv = mDBConn->GetLastInsertRowID(&id);
  NS_ENSURE_SUCCESS(rv, 0);

  // lock on DB from statement will be released once we return
  return id;
}

nsresult
nsDownloadManager::InitDB()
{
  nsresult rv = NS_OK;

  switch (mDBType) {
    case DATABASE_MEMORY:
      rv = InitMemoryDB();
      break;

    case DATABASE_DISK:
      rv = InitFileDB();
      break;

    default:
      NS_ERROR("Unexpected value encountered for nsDownloadManager::mDBType");
      break;
  }
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
    "UPDATE moz_downloads "
    "SET tempPath = :tempPath, startTime = :startTime, endTime = :endTime, "
      "state = :state, referrer = :referrer, entityID = :entityID, "
      "currBytes = :currBytes, maxBytes = :maxBytes, autoResume = :autoResume "
    "WHERE id = :id"), getter_AddRefs(mUpdateDownloadStatement));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
    "SELECT id "
    "FROM moz_downloads "
    "WHERE source = :source"), getter_AddRefs(mGetIdsForURIStatement));
  NS_ENSURE_SUCCESS(rv, rv);

  return rv;
}

nsresult
nsDownloadManager::Init()
{
  // Clean up any old downloads.rdf files from before Firefox 3
  {
    nsCOMPtr<nsIFile> oldDownloadsFile;
    bool fileExists;
    if (NS_SUCCEEDED(NS_GetSpecialDirectory(NS_APP_DOWNLOADS_50_FILE,
          getter_AddRefs(oldDownloadsFile))) &&
        NS_SUCCEEDED(oldDownloadsFile->Exists(&fileExists)) &&
        fileExists) {
      (void)oldDownloadsFile->Remove(false);
    }
  }

  mObserverService = mozilla::services::GetObserverService();
  if (!mObserverService)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIStringBundleService> bundleService =
    mozilla::services::GetStringBundleService();
  if (!bundleService)
    return NS_ERROR_FAILURE;

  nsresult rv = InitDB();
  NS_ENSURE_SUCCESS(rv, rv);

  rv = bundleService->CreateBundle(DOWNLOAD_MANAGER_BUNDLE,
                                   getter_AddRefs(mBundle));
  NS_ENSURE_SUCCESS(rv, rv);

#ifdef DOWNLOAD_SCANNER
  mScanner = new nsDownloadScanner();
  if (!mScanner)
    return NS_ERROR_OUT_OF_MEMORY;
  rv = mScanner->Init();
  if (NS_FAILED(rv)) {
    delete mScanner;
    mScanner = nsnull;
  }
#endif

  // Do things *after* initializing various download manager properties such as
  // restoring downloads to a consistent state
  rv = RestoreDatabaseState();
  NS_ENSURE_SUCCESS(rv, rv);

  rv = RestoreActiveDownloads();
  NS_WARN_IF_FALSE(NS_SUCCEEDED(rv), "Failed to restore all active downloads");

  nsCOMPtr<nsIPrivateBrowsingService> pbs =
    do_GetService(NS_PRIVATE_BROWSING_SERVICE_CONTRACTID);
  if (pbs) {
    (void)pbs->GetPrivateBrowsingEnabled(&mInPrivateBrowsing);
    if (mInPrivateBrowsing)
      OnEnterPrivateBrowsingMode();
  }

  nsCOMPtr<nsINavHistoryService> history =
    do_GetService(NS_NAVHISTORYSERVICE_CONTRACTID);

  // The following AddObserver calls must be the last lines in this function,
  // because otherwise, this function may fail (and thus, this object would be not
  // completely initialized), but the observerservice would still keep a reference
  // to us and notify us about shutdown, which may cause crashes.
  // failure to add an observer is not critical
  //
  // These observers will be cleaned up automatically at app shutdown.  We do
  // not bother explicitly breaking the observers because we are a singleton
  // that lives for the duration of the app.
  (void)mObserverService->AddObserver(this, "quit-application", false);
  (void)mObserverService->AddObserver(this, "quit-application-requested", false);
  (void)mObserverService->AddObserver(this, "offline-requested", false);
  (void)mObserverService->AddObserver(this, "sleep_notification", false);
  (void)mObserverService->AddObserver(this, "wake_notification", false);
  (void)mObserverService->AddObserver(this, NS_IOSERVICE_GOING_OFFLINE_TOPIC, false);
  (void)mObserverService->AddObserver(this, NS_IOSERVICE_OFFLINE_STATUS_TOPIC, false);
  (void)mObserverService->AddObserver(this, NS_PRIVATE_BROWSING_REQUEST_TOPIC, false);
  (void)mObserverService->AddObserver(this, NS_PRIVATE_BROWSING_SWITCH_TOPIC, false);

  if (history)
    (void)history->AddObserver(this, false);

  return NS_OK;
}

PRInt32
nsDownloadManager::GetRetentionBehavior()
{
  // We use 0 as the default, which is "remove when done"
  nsresult rv;
  nsCOMPtr<nsIPrefBranch> pref = do_GetService(NS_PREFSERVICE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, 0);

  PRInt32 val;
  rv = pref->GetIntPref(PREF_BDM_RETENTION, &val);
  NS_ENSURE_SUCCESS(rv, 0);

  return val;
}

enum nsDownloadManager::QuitBehavior
nsDownloadManager::GetQuitBehavior()
{
  // We use 0 as the default, which is "remember and resume the download"
  nsresult rv;
  nsCOMPtr<nsIPrefBranch> pref = do_GetService(NS_PREFSERVICE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, QUIT_AND_RESUME);

  PRInt32 val;
  rv = pref->GetIntPref(PREF_BDM_QUITBEHAVIOR, &val);
  NS_ENSURE_SUCCESS(rv, QUIT_AND_RESUME);

  switch (val) {
    case 1:
      return QUIT_AND_PAUSE;
    case 2:
      return QUIT_AND_CANCEL;
    default:
      return QUIT_AND_RESUME;
  }
}

nsresult
nsDownloadManager::GetDownloadFromDB(PRUint32 aID, nsDownload **retVal)
{
  NS_ASSERTION(!FindDownload(aID),
               "If it is a current download, you should not call this method!");

  // First, let's query the database and see if it even exists
  nsCOMPtr<mozIStorageStatement> stmt;
  nsresult rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
    "SELECT id, state, startTime, source, target, tempPath, name, referrer, "
           "entityID, currBytes, maxBytes, mimeType, preferredAction, "
           "preferredApplication, autoResume "
    "FROM moz_downloads "
    "WHERE id = :id"), getter_AddRefs(stmt));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("id"), aID);
  NS_ENSURE_SUCCESS(rv, rv);

  bool hasResults = false;
  rv = stmt->ExecuteStep(&hasResults);
  if (NS_FAILED(rv) || !hasResults)
    return NS_ERROR_NOT_AVAILABLE;

  // We have a download, so lets create it
  nsRefPtr<nsDownload> dl = new nsDownload();
  if (!dl)
    return NS_ERROR_OUT_OF_MEMORY;

  PRInt32 i = 0;
  // Setting all properties of the download now
  dl->mCancelable = nsnull;
  dl->mID = stmt->AsInt64(i++);
  dl->mDownloadState = stmt->AsInt32(i++);
  dl->mStartTime = stmt->AsInt64(i++);

  nsCString source;
  stmt->GetUTF8String(i++, source);
  rv = NS_NewURI(getter_AddRefs(dl->mSource), source);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCString target;
  stmt->GetUTF8String(i++, target);
  rv = NS_NewURI(getter_AddRefs(dl->mTarget), target);
  NS_ENSURE_SUCCESS(rv, rv);

  nsString tempPath;
  stmt->GetString(i++, tempPath);
  if (!tempPath.IsEmpty()) {
    rv = NS_NewLocalFile(tempPath, true, getter_AddRefs(dl->mTempFile));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  stmt->GetString(i++, dl->mDisplayName);

  nsCString referrer;
  rv = stmt->GetUTF8String(i++, referrer);
  if (NS_SUCCEEDED(rv) && !referrer.IsEmpty()) {
    rv = NS_NewURI(getter_AddRefs(dl->mReferrer), referrer);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  rv = stmt->GetUTF8String(i++, dl->mEntityID);
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt64 currBytes = stmt->AsInt64(i++);
  PRInt64 maxBytes = stmt->AsInt64(i++);
  dl->SetProgressBytes(currBytes, maxBytes);

  // Build mMIMEInfo only if the mimeType in DB is not empty
  nsCAutoString mimeType;
  rv = stmt->GetUTF8String(i++, mimeType);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!mimeType.IsEmpty()) {
    nsCOMPtr<nsIMIMEService> mimeService =
      do_GetService(NS_MIMESERVICE_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = mimeService->GetFromTypeAndExtension(mimeType, EmptyCString(),
                                              getter_AddRefs(dl->mMIMEInfo));
    NS_ENSURE_SUCCESS(rv, rv);

    nsHandlerInfoAction action = stmt->AsInt32(i++);
    rv = dl->mMIMEInfo->SetPreferredAction(action);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCAutoString persistentDescriptor;
    rv = stmt->GetUTF8String(i++, persistentDescriptor);
    NS_ENSURE_SUCCESS(rv, rv);

    if (!persistentDescriptor.IsEmpty()) {
      nsCOMPtr<nsILocalHandlerApp> handler =
        do_CreateInstance(NS_LOCALHANDLERAPP_CONTRACTID, &rv);
      NS_ENSURE_SUCCESS(rv, rv);

      nsCOMPtr<nsILocalFile> localExecutable;
      rv = NS_NewNativeLocalFile(EmptyCString(), false,
                                 getter_AddRefs(localExecutable));
      NS_ENSURE_SUCCESS(rv, rv);

      rv = localExecutable->SetPersistentDescriptor(persistentDescriptor);
      NS_ENSURE_SUCCESS(rv, rv);

      rv = handler->SetExecutable(localExecutable);
      NS_ENSURE_SUCCESS(rv, rv);

      rv = dl->mMIMEInfo->SetPreferredApplicationHandler(handler);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  } else {
    // Compensate for the i++s skipped in the true block
    i += 2;
  }

  dl->mAutoResume =
    static_cast<enum nsDownload::AutoResume>(stmt->AsInt32(i++));

  // Addrefing and returning
  NS_ADDREF(*retVal = dl);
  return NS_OK;
}

nsresult
nsDownloadManager::AddToCurrentDownloads(nsDownload *aDl)
{
  if (!mCurrentDownloads.AppendObject(aDl))
    return NS_ERROR_OUT_OF_MEMORY;

  aDl->mDownloadManager = this;
  return NS_OK;
}

void
nsDownloadManager::SendEvent(nsDownload *aDownload, const char *aTopic)
{
  (void)mObserverService->NotifyObservers(aDownload, aTopic, nsnull);
}

////////////////////////////////////////////////////////////////////////////////
//// nsIDownloadManager

NS_IMETHODIMP
nsDownloadManager::GetActiveDownloadCount(PRInt32 *aResult)
{
  *aResult = mCurrentDownloads.Count();

  return NS_OK;
}

NS_IMETHODIMP
nsDownloadManager::GetActiveDownloads(nsISimpleEnumerator **aResult)
{
  return NS_NewArrayEnumerator(aResult, mCurrentDownloads);
}

/**
 * For platforms where helper apps use the downloads directory (i.e. mobile),
 * this should be kept in sync with nsExternalHelperAppService.cpp
 */
NS_IMETHODIMP
nsDownloadManager::GetDefaultDownloadsDirectory(nsILocalFile **aResult)
{
  nsCOMPtr<nsILocalFile> downloadDir;

  nsresult rv;
  nsCOMPtr<nsIProperties> dirService =
     do_GetService(NS_DIRECTORY_SERVICE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  // OSX 10.4:
  // Desktop
  // OSX 10.5:
  // User download directory
  // Vista:
  // Downloads
  // XP/2K:
  // My Documents/Downloads
  // Linux:
  // XDG user dir spec, with a fallback to Home/Downloads

  nsXPIDLString folderName;
  mBundle->GetStringFromName(NS_LITERAL_STRING("downloadsFolder").get(),
                             getter_Copies(folderName));

#if defined (XP_MACOSX)
  rv = dirService->Get(NS_OSX_DEFAULT_DOWNLOAD_DIR,
                       NS_GET_IID(nsILocalFile),
                       getter_AddRefs(downloadDir));
  NS_ENSURE_SUCCESS(rv, rv);
#elif defined(XP_WIN)
  rv = dirService->Get(NS_WIN_DEFAULT_DOWNLOAD_DIR,
                       NS_GET_IID(nsILocalFile),
                       getter_AddRefs(downloadDir));
  NS_ENSURE_SUCCESS(rv, rv);

  // Check the os version
  nsCOMPtr<nsIPropertyBag2> infoService =
     do_GetService(NS_SYSTEMINFO_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt32 version;
  NS_NAMED_LITERAL_STRING(osVersion, "version");
  rv = infoService->GetPropertyAsInt32(osVersion, &version);
  NS_ENSURE_SUCCESS(rv, rv);
  if (version < 6) { // XP/2K
    // First get "My Documents"
    rv = dirService->Get(NS_WIN_PERSONAL_DIR,
                         NS_GET_IID(nsILocalFile),
                         getter_AddRefs(downloadDir));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = downloadDir->Append(folderName);
    NS_ENSURE_SUCCESS(rv, rv);

    // This could be the first time we are creating the downloads folder in My
    // Documents, so make sure it exists.
    bool exists;
    rv = downloadDir->Exists(&exists);
    NS_ENSURE_SUCCESS(rv, rv);
    if (!exists) {
      rv = downloadDir->Create(nsIFile::DIRECTORY_TYPE, 0755);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }
#elif defined(XP_UNIX)
#if defined(MOZ_PLATFORM_MAEMO)
    // As maemo does not follow the XDG "standard" (as usually desktop
    // Linux distros do) neither has a working $HOME/Desktop folder
    // for us to fallback into, "$HOME/MyDocs/.documents/" is the folder
    // we found most apropriate to be the default target folder for downloads
    // on the platform.
    rv = dirService->Get(NS_UNIX_XDG_DOCUMENTS_DIR,
                         NS_GET_IID(nsILocalFile),
                         getter_AddRefs(downloadDir));
#elif defined(ANDROID)
    // Android doesn't have a $HOME directory, and by default we only have
    // write access to /data/data/org.mozilla.{$APP} and /sdcard
    char* downloadDirPath = getenv("DOWNLOADS_DIRECTORY");
    if (downloadDirPath) {
      rv = NS_NewNativeLocalFile(nsDependentCString(downloadDirPath),
                                 true, getter_AddRefs(downloadDir));
      NS_ENSURE_SUCCESS(rv, rv);
    }
    else {
      rv = NS_ERROR_FAILURE;
    }
#else
  rv = dirService->Get(NS_UNIX_DEFAULT_DOWNLOAD_DIR,
                       NS_GET_IID(nsILocalFile),
                       getter_AddRefs(downloadDir));
  // fallback to Home/Downloads
  if (NS_FAILED(rv)) {
    rv = dirService->Get(NS_UNIX_HOME_DIR,
                         NS_GET_IID(nsILocalFile),
                         getter_AddRefs(downloadDir));
    NS_ENSURE_SUCCESS(rv, rv);
    rv = downloadDir->Append(folderName);
    NS_ENSURE_SUCCESS(rv, rv);
  }
#endif
#else
  rv = dirService->Get(NS_OS_HOME_DIR,
                       NS_GET_IID(nsILocalFile),
                       getter_AddRefs(downloadDir));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = downloadDir->Append(folderName);
  NS_ENSURE_SUCCESS(rv, rv);
#endif

  downloadDir.forget(aResult);

  return NS_OK;
}

#define NS_BRANCH_DOWNLOAD     "browser.download."
#define NS_PREF_FOLDERLIST     "folderList"
#define NS_PREF_DIR            "dir"

NS_IMETHODIMP
nsDownloadManager::GetUserDownloadsDirectory(nsILocalFile **aResult)
{
  nsresult rv;
  nsCOMPtr<nsIProperties> dirService =
     do_GetService(NS_DIRECTORY_SERVICE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIPrefService> prefService =
     do_GetService(NS_PREFSERVICE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIPrefBranch> prefBranch;
  rv = prefService->GetBranch(NS_BRANCH_DOWNLOAD,
                              getter_AddRefs(prefBranch));
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt32 val;
  rv = prefBranch->GetIntPref(NS_PREF_FOLDERLIST,
                              &val);
  NS_ENSURE_SUCCESS(rv, rv);

  switch(val) {
    case 0: // Desktop
      {
        nsCOMPtr<nsILocalFile> downloadDir;
        nsCOMPtr<nsIProperties> dirService =
           do_GetService(NS_DIRECTORY_SERVICE_CONTRACTID, &rv);
        NS_ENSURE_SUCCESS(rv, rv);
        rv = dirService->Get(NS_OS_DESKTOP_DIR,
                             NS_GET_IID(nsILocalFile),
                             getter_AddRefs(downloadDir));
        NS_ENSURE_SUCCESS(rv, rv);
        downloadDir.forget(aResult);
        return NS_OK;
      }
      break;
    case 1: // Downloads
      return GetDefaultDownloadsDirectory(aResult);
    case 2: // Custom
      {
        nsCOMPtr<nsILocalFile> customDirectory;
        prefBranch->GetComplexValue(NS_PREF_DIR,
                                    NS_GET_IID(nsILocalFile),
                                    getter_AddRefs(customDirectory));
        if (customDirectory) {
          bool exists = false;
          (void)customDirectory->Exists(&exists);

          if (!exists) {
            rv = customDirectory->Create(nsIFile::DIRECTORY_TYPE, 0755);
            if (NS_SUCCEEDED(rv)) {
              customDirectory.forget(aResult);
              return NS_OK;
            }

            // Create failed, so it still doesn't exist.  Fall out and get the
            // default downloads directory.
          }

          bool writable = false;
          bool directory = false;
          (void)customDirectory->IsWritable(&writable);
          (void)customDirectory->IsDirectory(&directory);

          if (exists && writable && directory) {
            customDirectory.forget(aResult);
            return NS_OK;
          }
        }
        rv = GetDefaultDownloadsDirectory(aResult);
        if (NS_SUCCEEDED(rv)) {
          (void)prefBranch->SetComplexValue(NS_PREF_DIR,
                                            NS_GET_IID(nsILocalFile),
                                            *aResult);
        }
        return rv;
      }
      break;
  }
  return NS_ERROR_INVALID_ARG;
}

NS_IMETHODIMP
nsDownloadManager::AddDownload(DownloadType aDownloadType,
                               nsIURI *aSource,
                               nsIURI *aTarget,
                               const nsAString& aDisplayName,
                               nsIMIMEInfo *aMIMEInfo,
                               PRTime aStartTime,
                               nsILocalFile *aTempFile,
                               nsICancelable *aCancelable,
                               nsIDownload **aDownload)
{
  NS_ENSURE_ARG_POINTER(aSource);
  NS_ENSURE_ARG_POINTER(aTarget);
  NS_ENSURE_ARG_POINTER(aDownload);

  nsresult rv;

  // target must be on the local filesystem
  nsCOMPtr<nsIFileURL> targetFileURL = do_QueryInterface(aTarget, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIFile> targetFile;
  rv = targetFileURL->GetFile(getter_AddRefs(targetFile));
  NS_ENSURE_SUCCESS(rv, rv);

  nsRefPtr<nsDownload> dl = new nsDownload();
  if (!dl)
    return NS_ERROR_OUT_OF_MEMORY;

  // give our new nsIDownload some info so it's ready to go off into the world
  dl->mTarget = aTarget;
  dl->mSource = aSource;
  dl->mTempFile = aTempFile;

  dl->mDisplayName = aDisplayName;
  if (dl->mDisplayName.IsEmpty())
    targetFile->GetLeafName(dl->mDisplayName);

  dl->mMIMEInfo = aMIMEInfo;
  dl->SetStartTime(aStartTime == 0 ? PR_Now() : aStartTime);

  // Creates a cycle that will be broken when the download finishes
  dl->mCancelable = aCancelable;

  // Adding to the DB
  nsCAutoString source, target;
  aSource->GetSpec(source);
  aTarget->GetSpec(target);

  // Track the temp file for exthandler downloads
  nsAutoString tempPath;
  if (aTempFile)
    aTempFile->GetPath(tempPath);

  // Break down MIMEInfo but don't panic if we can't get all the pieces - we
  // can still download the file
  nsCAutoString persistentDescriptor, mimeType;
  nsHandlerInfoAction action = nsIMIMEInfo::saveToDisk;
  if (aMIMEInfo) {
    (void)aMIMEInfo->GetType(mimeType);

    nsCOMPtr<nsIHandlerApp> handlerApp;
    (void)aMIMEInfo->GetPreferredApplicationHandler(getter_AddRefs(handlerApp));
    nsCOMPtr<nsILocalHandlerApp> locHandlerApp = do_QueryInterface(handlerApp);

    if (locHandlerApp) {
      nsCOMPtr<nsIFile> executable;
      (void)locHandlerApp->GetExecutable(getter_AddRefs(executable));
      nsCOMPtr<nsILocalFile> locExecutable = do_QueryInterface(executable);

      if (locExecutable)
        (void)locExecutable->GetPersistentDescriptor(persistentDescriptor);
    }

    (void)aMIMEInfo->GetPreferredAction(&action);
  }

  DownloadState startState = nsIDownloadManager::DOWNLOAD_QUEUED;

  PRInt64 id = AddDownloadToDB(dl->mDisplayName, source, target, tempPath,
                               dl->mStartTime, dl->mLastUpdate,
                               mimeType, persistentDescriptor, action);
  NS_ENSURE_TRUE(id, NS_ERROR_FAILURE);
  dl->mID = id;

  rv = AddToCurrentDownloads(dl);
  (void)dl->SetState(startState);
  NS_ENSURE_SUCCESS(rv, rv);

#ifdef DOWNLOAD_SCANNER
  if (mScanner) {
    bool scan = true;
    nsCOMPtr<nsIPrefBranch> prefs(do_GetService(NS_PREFSERVICE_CONTRACTID));
    if (prefs) {
      (void)prefs->GetBoolPref(PREF_BDM_SCANWHENDONE, &scan);
    }
    // We currently apply local security policy to downloads when we scan
    // via windows all-in-one download security api. The CheckPolicy call
    // below is a pre-emptive part of that process. So tie applying security
    // zone policy settings when downloads are intiated to the same pref
    // that triggers applying security zone policy settings after a download
    // completes. (bug 504804)
    if (scan) {
      AVCheckPolicyState res = mScanner->CheckPolicy(aSource, aTarget);
      if (res == AVPOLICY_BLOCKED) {
        // This download will get deleted during a call to IAE's Save,
        // so go ahead and mark it as blocked and avoid the download.
        (void)CancelDownload(id);
        startState = nsIDownloadManager::DOWNLOAD_BLOCKED_POLICY;
      }
    }
  }
#endif

  // Check with parental controls to see if file downloads
  // are allowed for this user. If not allowed, cancel the
  // download and mark its state as being blocked.
  nsCOMPtr<nsIParentalControlsService> pc =
    do_CreateInstance(NS_PARENTALCONTROLSSERVICE_CONTRACTID);
  if (pc) {
    bool enabled = false;
    (void)pc->GetBlockFileDownloadsEnabled(&enabled);
    if (enabled) {
      (void)CancelDownload(id);
      (void)dl->SetState(nsIDownloadManager::DOWNLOAD_BLOCKED_PARENTAL);
    }

    // Log the event if required by pc settings.
    bool logEnabled = false;
    (void)pc->GetLoggingEnabled(&logEnabled);
    if (logEnabled) {
      (void)pc->Log(nsIParentalControlsService::ePCLog_FileDownload,
                    enabled,
                    aSource,
                    nsnull);
    }
  }

  NS_ADDREF(*aDownload = dl);

  return NS_OK;
}

NS_IMETHODIMP
nsDownloadManager::GetDownload(PRUint32 aID, nsIDownload **aDownloadItem)
{
  nsDownload *itm = FindDownload(aID);

  nsRefPtr<nsDownload> dl;
  if (!itm) {
    nsresult rv = GetDownloadFromDB(aID, getter_AddRefs(dl));
    NS_ENSURE_SUCCESS(rv, rv);

    itm = dl.get();
  }

  NS_ADDREF(*aDownloadItem = itm);

  return NS_OK;
}

nsDownload *
nsDownloadManager::FindDownload(PRUint32 aID)
{
  // we shouldn't ever have many downloads, so we can loop over them
  for (PRInt32 i = mCurrentDownloads.Count() - 1; i >= 0; --i) {
    nsDownload *dl = mCurrentDownloads[i];

    if (dl->mID == aID)
      return dl;
  }

  return nsnull;
}

NS_IMETHODIMP
nsDownloadManager::CancelDownload(PRUint32 aID)
{
  // We AddRef here so we don't lose access to member variables when we remove
  nsRefPtr<nsDownload> dl = FindDownload(aID);

  // if it's null, someone passed us a bad id.
  if (!dl)
    return NS_ERROR_FAILURE;

  // Don't cancel if download is already finished
  if (dl->IsFinished())
    return NS_OK;

  // if the download is fake-paused, we have to resume it so we can cancel it
  if (dl->IsPaused() && !dl->IsResumable())
    (void)dl->Resume();

  // Have the download cancel its connection
  (void)dl->Cancel();

  // Dump the temp file because we know we don't need the file anymore. The
  // underlying transfer creating the file doesn't delete the file because it
  // can't distinguish between a pause that cancels the transfer or a real
  // cancel.
  if (dl->mTempFile) {
    bool exists;
    dl->mTempFile->Exists(&exists);
    if (exists)
      dl->mTempFile->Remove(false);
  }

  nsCOMPtr<nsILocalFile> file;
  if (NS_SUCCEEDED(dl->GetTargetFile(getter_AddRefs(file))))
  {
    bool exists;
    file->Exists(&exists);
    if (exists)
      file->Remove(false);
  }

  nsresult rv = dl->SetState(nsIDownloadManager::DOWNLOAD_CANCELED);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

NS_IMETHODIMP
nsDownloadManager::RetryDownload(PRUint32 aID)
{
  nsRefPtr<nsDownload> dl;
  nsresult rv = GetDownloadFromDB(aID, getter_AddRefs(dl));
  NS_ENSURE_SUCCESS(rv, rv);

  // if our download is not canceled or failed, we should fail
  if (dl->mDownloadState != nsIDownloadManager::DOWNLOAD_FAILED &&
      dl->mDownloadState != nsIDownloadManager::DOWNLOAD_BLOCKED_PARENTAL &&
      dl->mDownloadState != nsIDownloadManager::DOWNLOAD_BLOCKED_POLICY &&
      dl->mDownloadState != nsIDownloadManager::DOWNLOAD_DIRTY &&
      dl->mDownloadState != nsIDownloadManager::DOWNLOAD_CANCELED)
    return NS_ERROR_FAILURE;

  // If the download has failed and is resumable then we first try resuming it
  if (dl->mDownloadState == nsIDownloadManager::DOWNLOAD_FAILED && dl->IsResumable()) {
    rv = dl->Resume();
    if (NS_SUCCEEDED(rv))
      return rv;
  }

  // reset time and download progress
  dl->SetStartTime(PR_Now());
  dl->SetProgressBytes(0, -1);

  nsCOMPtr<nsIWebBrowserPersist> wbp =
    do_CreateInstance("@mozilla.org/embedding/browser/nsWebBrowserPersist;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = wbp->SetPersistFlags(nsIWebBrowserPersist::PERSIST_FLAGS_REPLACE_EXISTING_FILES |
                            nsIWebBrowserPersist::PERSIST_FLAGS_AUTODETECT_APPLY_CONVERSION);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = AddToCurrentDownloads(dl);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = dl->SetState(nsIDownloadManager::DOWNLOAD_QUEUED);
  NS_ENSURE_SUCCESS(rv, rv);

  // Creates a cycle that will be broken when the download finishes
  dl->mCancelable = wbp;
  (void)wbp->SetProgressListener(dl);

  rv = wbp->SaveURI(dl->mSource, nsnull, nsnull, nsnull, nsnull, dl->mTarget);
  if (NS_FAILED(rv)) {
    dl->mCancelable = nsnull;
    (void)wbp->SetProgressListener(nsnull);
    return rv;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDownloadManager::RemoveDownload(PRUint32 aID)
{
  nsDownload *dl = FindDownload(aID);
  NS_ASSERTION(!dl, "Can't call RemoveDownload on a download in progress!");
  if (dl)
    return NS_ERROR_FAILURE;

  nsCOMPtr<mozIStorageStatement> stmt;
  nsresult rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
    "DELETE FROM moz_downloads "
    "WHERE id = :id"), getter_AddRefs(stmt));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("id"), aID); // unsigned; 64-bit to prevent overflow
  NS_ENSURE_SUCCESS(rv, rv);

  rv = stmt->Execute();
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsISupportsPRUint32> id =
    do_CreateInstance(NS_SUPPORTS_PRUINT32_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = id->SetData(aID);
  NS_ENSURE_SUCCESS(rv, rv);

  // Notify the UI with the topic and download id
  return mObserverService->NotifyObservers(id,
                                           "download-manager-remove-download",
                                           nsnull);
}

NS_IMETHODIMP
nsDownloadManager::RemoveDownloadsByTimeframe(PRInt64 aStartTime,
                                              PRInt64 aEndTime)
{
  nsCOMPtr<mozIStorageStatement> stmt;
  nsresult rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
    "DELETE FROM moz_downloads "
    "WHERE startTime >= :startTime "
    "AND startTime <= :endTime "
    "AND state NOT IN (:downloading, :paused, :queued)"), getter_AddRefs(stmt));
  NS_ENSURE_SUCCESS(rv, rv);

  // Bind the times
  rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("startTime"), aStartTime);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("endTime"), aEndTime);
  NS_ENSURE_SUCCESS(rv, rv);

  // Bind the active states
  rv = stmt->BindInt32ByName(NS_LITERAL_CSTRING("downloading"), nsIDownloadManager::DOWNLOAD_DOWNLOADING);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = stmt->BindInt32ByName(NS_LITERAL_CSTRING("paused"), nsIDownloadManager::DOWNLOAD_PAUSED);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = stmt->BindInt32ByName(NS_LITERAL_CSTRING("queued"), nsIDownloadManager::DOWNLOAD_QUEUED);
  NS_ENSURE_SUCCESS(rv, rv);

  // Execute
  rv = stmt->Execute();
  NS_ENSURE_SUCCESS(rv, rv);

  // Notify the UI with the topic and null subject to indicate "remove multiple"
  return mObserverService->NotifyObservers(nsnull,
                                           "download-manager-remove-download",
                                           nsnull);
}

NS_IMETHODIMP
nsDownloadManager::CleanUp()
{
  DownloadState states[] = { nsIDownloadManager::DOWNLOAD_FINISHED,
                             nsIDownloadManager::DOWNLOAD_FAILED,
                             nsIDownloadManager::DOWNLOAD_CANCELED,
                             nsIDownloadManager::DOWNLOAD_BLOCKED_PARENTAL,
                             nsIDownloadManager::DOWNLOAD_BLOCKED_POLICY,
                             nsIDownloadManager::DOWNLOAD_DIRTY };

  nsCOMPtr<mozIStorageStatement> stmt;
  nsresult rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
    "DELETE FROM moz_downloads "
    "WHERE state = ? "
      "OR state = ? "
      "OR state = ? "
      "OR state = ? "
      "OR state = ? "
      "OR state = ?"), getter_AddRefs(stmt));
  NS_ENSURE_SUCCESS(rv, rv);
  for (PRUint32 i = 0; i < ArrayLength(states); ++i) {
    rv = stmt->BindInt32ByIndex(i, states[i]);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  rv = stmt->Execute();
  NS_ENSURE_SUCCESS(rv, rv);

  // Notify the UI with the topic and null subject to indicate "remove multiple"
  return mObserverService->NotifyObservers(nsnull,
                                           "download-manager-remove-download",
                                           nsnull);
}

NS_IMETHODIMP
nsDownloadManager::GetCanCleanUp(bool *aResult)
{
  *aResult = false;

  DownloadState states[] = { nsIDownloadManager::DOWNLOAD_FINISHED,
                             nsIDownloadManager::DOWNLOAD_FAILED,
                             nsIDownloadManager::DOWNLOAD_CANCELED,
                             nsIDownloadManager::DOWNLOAD_BLOCKED_PARENTAL,
                             nsIDownloadManager::DOWNLOAD_BLOCKED_POLICY,
                             nsIDownloadManager::DOWNLOAD_DIRTY };

  nsCOMPtr<mozIStorageStatement> stmt;
  nsresult rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
    "SELECT COUNT(*) "
    "FROM moz_downloads "
    "WHERE state = ? "
      "OR state = ? "
      "OR state = ? "
      "OR state = ? "
      "OR state = ? "
      "OR state = ?"), getter_AddRefs(stmt));
  NS_ENSURE_SUCCESS(rv, rv);
  for (PRUint32 i = 0; i < ArrayLength(states); ++i) {
    rv = stmt->BindInt32ByIndex(i, states[i]);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  bool moreResults; // We don't really care...
  rv = stmt->ExecuteStep(&moreResults);
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt32 count;
  rv = stmt->GetInt32(0, &count);

  if (count > 0)
    *aResult = true;

  return rv;
}

NS_IMETHODIMP
nsDownloadManager::PauseDownload(PRUint32 aID)
{
  nsDownload *dl = FindDownload(aID);
  if (!dl)
    return NS_ERROR_FAILURE;

  return dl->Pause();
}

NS_IMETHODIMP
nsDownloadManager::ResumeDownload(PRUint32 aID)
{
  nsDownload *dl = FindDownload(aID);
  if (!dl)
    return NS_ERROR_FAILURE;

  return dl->Resume();
}

NS_IMETHODIMP
nsDownloadManager::GetDBConnection(mozIStorageConnection **aDBConn)
{
  NS_ADDREF(*aDBConn = mDBConn);

  return NS_OK;
}

NS_IMETHODIMP
nsDownloadManager::AddListener(nsIDownloadProgressListener *aListener)
{
  mListeners.AppendObject(aListener);

  return NS_OK;
}

NS_IMETHODIMP
nsDownloadManager::RemoveListener(nsIDownloadProgressListener *aListener)
{
  mListeners.RemoveObject(aListener);

  return NS_OK;
}

void
nsDownloadManager::NotifyListenersOnDownloadStateChange(PRInt16 aOldState,
                                                        nsIDownload *aDownload)
{
  for (PRInt32 i = mListeners.Count() - 1; i >= 0; --i)
    mListeners[i]->OnDownloadStateChange(aOldState, aDownload);
}

void
nsDownloadManager::NotifyListenersOnProgressChange(nsIWebProgress *aProgress,
                                                   nsIRequest *aRequest,
                                                   PRInt64 aCurSelfProgress,
                                                   PRInt64 aMaxSelfProgress,
                                                   PRInt64 aCurTotalProgress,
                                                   PRInt64 aMaxTotalProgress,
                                                   nsIDownload *aDownload)
{
  for (PRInt32 i = mListeners.Count() - 1; i >= 0; --i)
    mListeners[i]->OnProgressChange(aProgress, aRequest, aCurSelfProgress,
                                    aMaxSelfProgress, aCurTotalProgress,
                                    aMaxTotalProgress, aDownload);
}

void
nsDownloadManager::NotifyListenersOnStateChange(nsIWebProgress *aProgress,
                                                nsIRequest *aRequest,
                                                PRUint32 aStateFlags,
                                                nsresult aStatus,
                                                nsIDownload *aDownload)
{
  for (PRInt32 i = mListeners.Count() - 1; i >= 0; --i)
    mListeners[i]->OnStateChange(aProgress, aRequest, aStateFlags, aStatus,
                                 aDownload);
}

nsresult
nsDownloadManager::SwitchDatabaseTypeTo(enum nsDownloadManager::DatabaseType aType)
{
  if (aType == mDBType)
    return NS_OK; // no-op

  mDBType = aType;

  (void)PauseAllDownloads(true);
  (void)RemoveAllDownloads();

  nsresult rv = InitDB();
  NS_ENSURE_SUCCESS(rv, rv);

  // Do things *after* initializing various download manager properties such as
  // restoring downloads to a consistent state
  rv = RestoreDatabaseState();
  NS_ENSURE_SUCCESS(rv, rv);

  rv = RestoreActiveDownloads();
  NS_WARN_IF_FALSE(NS_SUCCEEDED(rv), "Failed to restore all active downloads");

  return rv;
}

////////////////////////////////////////////////////////////////////////////////
//// nsINavHistoryObserver

NS_IMETHODIMP
nsDownloadManager::OnBeginUpdateBatch()
{
  // We already have a transaction, so don't make another
  if (mHistoryTransaction)
    return NS_OK;

  // Start a transaction that commits when deleted
  mHistoryTransaction = new mozStorageTransaction(mDBConn, true);

  return NS_OK;
}

NS_IMETHODIMP
nsDownloadManager::OnEndUpdateBatch()
{
  // Get rid of the transaction and cause it to commit
  mHistoryTransaction = nsnull;

  return NS_OK;
}

NS_IMETHODIMP
nsDownloadManager::OnVisit(nsIURI *aURI, PRInt64 aVisitID, PRTime aTime,
                           PRInt64 aSessionID, PRInt64 aReferringID,
                           PRUint32 aTransitionType, const nsACString& aGUID,
                           PRUint32 *aAdded)
{
  return NS_OK;
}

NS_IMETHODIMP
nsDownloadManager::OnTitleChanged(nsIURI *aURI,
                                  const nsAString &aPageTitle,
                                  const nsACString &aGUID)
{
  return NS_OK;
}

NS_IMETHODIMP
nsDownloadManager::OnBeforeDeleteURI(nsIURI *aURI,
                                     const nsACString& aGUID,
                                     PRUint16 aReason)
{
  return NS_OK;
}

NS_IMETHODIMP
nsDownloadManager::OnDeleteURI(nsIURI *aURI,
                               const nsACString& aGUID,
                               PRUint16 aReason)
{
  return RemoveDownloadsForURI(aURI);
}

NS_IMETHODIMP
nsDownloadManager::OnClearHistory()
{
  return CleanUp();
}

NS_IMETHODIMP
nsDownloadManager::OnPageChanged(nsIURI *aURI,
                                 PRUint32 aChangedAttribute,
                                 const nsAString& aNewValue,
                                 const nsACString &aGUID)
{
  return NS_OK;
}

NS_IMETHODIMP
nsDownloadManager::OnDeleteVisits(nsIURI *aURI, PRTime aVisitTime,
                                  const nsACString& aGUID,
                                  PRUint16 aReason)
{
  // Don't bother removing downloads until the page is removed.
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
//// nsIObserver

NS_IMETHODIMP
nsDownloadManager::Observe(nsISupports *aSubject,
                           const char *aTopic,
                           const PRUnichar *aData)
{
  PRInt32 currDownloadCount = mCurrentDownloads.Count();

  // If we don't need to cancel all the downloads on quit, only count the ones
  // that aren't resumable.
  if (GetQuitBehavior() != QUIT_AND_CANCEL)
    for (PRInt32 i = currDownloadCount - 1; i >= 0; --i)
      if (mCurrentDownloads[i]->IsResumable())
        currDownloadCount--;

  nsresult rv;
  if (strcmp(aTopic, "oncancel") == 0) {
    nsCOMPtr<nsIDownload> dl = do_QueryInterface(aSubject, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    PRUint32 id;
    dl->GetId(&id);
    nsDownload *dl2 = FindDownload(id);
    if (dl2)
      return CancelDownload(id);
  } else if (strcmp(aTopic, "quit-application") == 0) {
    // Try to pause all downloads and, if appropriate, mark them as auto-resume
    // unless user has specified that downloads should be canceled
    enum QuitBehavior behavior = GetQuitBehavior();
    if (behavior != QUIT_AND_CANCEL)
      (void)PauseAllDownloads(bool(behavior != QUIT_AND_PAUSE));

    // Remove downloads to break cycles and cancel downloads
    (void)RemoveAllDownloads();

   // Now that active downloads have been canceled, remove all completed or
   // aborted downloads if the user's retention policy specifies it.
    if (GetRetentionBehavior() == 1)
      CleanUp();
  } else if (strcmp(aTopic, "quit-application-requested") == 0 &&
             currDownloadCount) {
    nsCOMPtr<nsISupportsPRBool> cancelDownloads =
      do_QueryInterface(aSubject, &rv);
    NS_ENSURE_SUCCESS(rv, rv);
#ifndef XP_MACOSX
    ConfirmCancelDownloads(currDownloadCount, cancelDownloads,
                           NS_LITERAL_STRING("quitCancelDownloadsAlertTitle").get(),
                           NS_LITERAL_STRING("quitCancelDownloadsAlertMsgMultiple").get(),
                           NS_LITERAL_STRING("quitCancelDownloadsAlertMsg").get(),
                           NS_LITERAL_STRING("dontQuitButtonWin").get());
#else
    ConfirmCancelDownloads(currDownloadCount, cancelDownloads,
                           NS_LITERAL_STRING("quitCancelDownloadsAlertTitle").get(),
                           NS_LITERAL_STRING("quitCancelDownloadsAlertMsgMacMultiple").get(),
                           NS_LITERAL_STRING("quitCancelDownloadsAlertMsgMac").get(),
                           NS_LITERAL_STRING("dontQuitButtonMac").get());
#endif
  } else if (strcmp(aTopic, "offline-requested") == 0 && currDownloadCount) {
    nsCOMPtr<nsISupportsPRBool> cancelDownloads =
      do_QueryInterface(aSubject, &rv);
    NS_ENSURE_SUCCESS(rv, rv);
    ConfirmCancelDownloads(currDownloadCount, cancelDownloads,
                           NS_LITERAL_STRING("offlineCancelDownloadsAlertTitle").get(),
                           NS_LITERAL_STRING("offlineCancelDownloadsAlertMsgMultiple").get(),
                           NS_LITERAL_STRING("offlineCancelDownloadsAlertMsg").get(),
                           NS_LITERAL_STRING("dontGoOfflineButton").get());
  }
  else if (strcmp(aTopic, NS_IOSERVICE_GOING_OFFLINE_TOPIC) == 0) {
    // Pause all downloads, and mark them to auto-resume.
    (void)PauseAllDownloads(true);
  }
  else if (strcmp(aTopic, NS_IOSERVICE_OFFLINE_STATUS_TOPIC) == 0 &&
           nsDependentString(aData).EqualsLiteral(NS_IOSERVICE_ONLINE)) {
    // We can now resume all downloads that are supposed to auto-resume.
    (void)ResumeAllDownloads(false);
  }
  else if (strcmp(aTopic, "dlmgr-switchdb") == 0) {
    if (NS_LITERAL_STRING("memory").Equals(aData))
      return SwitchDatabaseTypeTo(DATABASE_MEMORY);
    else if (NS_LITERAL_STRING("disk").Equals(aData))
      return SwitchDatabaseTypeTo(DATABASE_DISK);
  }
  else if (strcmp(aTopic, "alertclickcallback") == 0) {
    nsCOMPtr<nsIDownloadManagerUI> dmui =
      do_GetService("@mozilla.org/download-manager-ui;1", &rv);
    NS_ENSURE_SUCCESS(rv, rv);
    return dmui->Show(nsnull, 0, nsIDownloadManagerUI::REASON_USER_INTERACTED);
  } else if (strcmp(aTopic, "sleep_notification") == 0) {
    // Pause downloads if we're sleeping, and mark the downloads as auto-resume
    (void)PauseAllDownloads(true);
  } else if (strcmp(aTopic, "wake_notification") == 0) {
    PRInt32 resumeOnWakeDelay = 10000;
    nsCOMPtr<nsIPrefBranch> pref = do_GetService(NS_PREFSERVICE_CONTRACTID);
    if (pref)
      (void)pref->GetIntPref(PREF_BDM_RESUMEONWAKEDELAY, &resumeOnWakeDelay);

    // Wait a little bit before trying to resume to avoid resuming when network
    // connections haven't restarted yet
    mResumeOnWakeTimer = do_CreateInstance("@mozilla.org/timer;1");
    if (resumeOnWakeDelay >= 0 && mResumeOnWakeTimer) {
      (void)mResumeOnWakeTimer->InitWithFuncCallback(ResumeOnWakeCallback,
        this, resumeOnWakeDelay, nsITimer::TYPE_ONE_SHOT);
    }
  }
  else if (strcmp(aTopic, NS_PRIVATE_BROWSING_REQUEST_TOPIC) == 0) {
    if (NS_LITERAL_STRING(NS_PRIVATE_BROWSING_ENTER).Equals(aData) &&
        currDownloadCount) {
      nsCOMPtr<nsISupportsPRBool> cancelDownloads =
        do_QueryInterface(aSubject, &rv);
      NS_ENSURE_SUCCESS(rv, rv);
      ConfirmCancelDownloads(currDownloadCount, cancelDownloads,
                             NS_LITERAL_STRING("enterPrivateBrowsingCancelDownloadsAlertTitle").get(),
                             NS_LITERAL_STRING("enterPrivateBrowsingCancelDownloadsAlertMsgMultiple").get(),
                             NS_LITERAL_STRING("enterPrivateBrowsingCancelDownloadsAlertMsg").get(),
                             NS_LITERAL_STRING("dontEnterPrivateBrowsingButton").get());
    }
    else if (NS_LITERAL_STRING(NS_PRIVATE_BROWSING_LEAVE).Equals(aData) &&
             mCurrentDownloads.Count()) {
      nsCOMPtr<nsISupportsPRBool> cancelDownloads =
        do_QueryInterface(aSubject, &rv);
      NS_ENSURE_SUCCESS(rv, rv);
      ConfirmCancelDownloads(mCurrentDownloads.Count(), cancelDownloads,
                             NS_LITERAL_STRING("leavePrivateBrowsingCancelDownloadsAlertTitle").get(),
                             NS_LITERAL_STRING("leavePrivateBrowsingCancelDownloadsAlertMsgMultiple").get(),
                             NS_LITERAL_STRING("leavePrivateBrowsingCancelDownloadsAlertMsg").get(),
                             NS_LITERAL_STRING("dontLeavePrivateBrowsingButton").get());
    }
  }
  else if (strcmp(aTopic, NS_PRIVATE_BROWSING_SWITCH_TOPIC) == 0) {
    if (NS_LITERAL_STRING(NS_PRIVATE_BROWSING_ENTER).Equals(aData))
      OnEnterPrivateBrowsingMode();
    else if (NS_LITERAL_STRING(NS_PRIVATE_BROWSING_LEAVE).Equals(aData))
      OnLeavePrivateBrowsingMode();
  }

  return NS_OK;
}

void
nsDownloadManager::OnEnterPrivateBrowsingMode()
{
  // Pause all downloads, and mark them to auto-resume.
  (void)PauseAllDownloads(true);

  // Switch to using an in-memory DB
  (void)SwitchDatabaseTypeTo(DATABASE_MEMORY);

  mInPrivateBrowsing = true;
}

void
nsDownloadManager::OnLeavePrivateBrowsingMode()
{
  // We can now resume all downloads that are supposed to auto-resume.
  (void)ResumeAllDownloads(false);

  // Switch back to the on-disk DB again
  (void)SwitchDatabaseTypeTo(DATABASE_DISK);

  mInPrivateBrowsing = false;
}

void
nsDownloadManager::ConfirmCancelDownloads(PRInt32 aCount,
                                          nsISupportsPRBool *aCancelDownloads,
                                          const PRUnichar *aTitle,
                                          const PRUnichar *aCancelMessageMultiple,
                                          const PRUnichar *aCancelMessageSingle,
                                          const PRUnichar *aDontCancelButton)
{
  // If user has already dismissed quit request, then do nothing
  bool quitRequestCancelled = false;
  aCancelDownloads->GetData(&quitRequestCancelled);
  if (quitRequestCancelled)
    return;

  nsXPIDLString title, message, quitButton, dontQuitButton;

  mBundle->GetStringFromName(aTitle, getter_Copies(title));

  nsAutoString countString;
  countString.AppendInt(aCount);
  const PRUnichar *strings[1] = { countString.get() };
  if (aCount > 1) {
    mBundle->FormatStringFromName(aCancelMessageMultiple, strings, 1,
                                  getter_Copies(message));
    mBundle->FormatStringFromName(NS_LITERAL_STRING("cancelDownloadsOKTextMultiple").get(),
                                  strings, 1, getter_Copies(quitButton));
  } else {
    mBundle->GetStringFromName(aCancelMessageSingle, getter_Copies(message));
    mBundle->GetStringFromName(NS_LITERAL_STRING("cancelDownloadsOKText").get(),
                               getter_Copies(quitButton));
  }

  mBundle->GetStringFromName(aDontCancelButton, getter_Copies(dontQuitButton));

  // Get Download Manager window, to be parent of alert.
  nsCOMPtr<nsIWindowMediator> wm = do_GetService(NS_WINDOWMEDIATOR_CONTRACTID);
  nsCOMPtr<nsIDOMWindow> dmWindow;
  if (wm) {
    wm->GetMostRecentWindow(NS_LITERAL_STRING("Download:Manager").get(),
                            getter_AddRefs(dmWindow));
  }

  // Show alert.
  nsCOMPtr<nsIPromptService> prompter(do_GetService(NS_PROMPTSERVICE_CONTRACTID));
  if (prompter) {
    PRInt32 flags = (nsIPromptService::BUTTON_TITLE_IS_STRING * nsIPromptService::BUTTON_POS_0) + (nsIPromptService::BUTTON_TITLE_IS_STRING * nsIPromptService::BUTTON_POS_1);
    bool nothing = false;
    PRInt32 button;
    prompter->ConfirmEx(dmWindow, title, message, flags, quitButton.get(), dontQuitButton.get(), nsnull, nsnull, &nothing, &button);

    aCancelDownloads->SetData(button == 1);
  }
}

////////////////////////////////////////////////////////////////////////////////
//// nsDownload

NS_IMPL_CLASSINFO(nsDownload, NULL, 0, NS_DOWNLOAD_CID)
NS_IMPL_ISUPPORTS4_CI(
    nsDownload
  , nsIDownload
  , nsITransfer
  , nsIWebProgressListener
  , nsIWebProgressListener2
)

nsDownload::nsDownload() : mDownloadState(nsIDownloadManager::DOWNLOAD_NOTSTARTED),
                           mID(0),
                           mPercentComplete(0),
                           mCurrBytes(0),
                           mMaxBytes(-1),
                           mStartTime(0),
                           mLastUpdate(PR_Now() - (PRUint32)gUpdateInterval),
                           mResumedAt(-1),
                           mSpeed(0),
                           mHasMultipleFiles(false),
                           mAutoResume(DONT_RESUME)
{
}

nsDownload::~nsDownload()
{
}

nsresult
nsDownload::SetState(DownloadState aState)
{
  NS_ASSERTION(mDownloadState != aState,
               "Trying to set the download state to what it already is set to!");

  PRInt16 oldState = mDownloadState;
  mDownloadState = aState;

  // We don't want to lose access to our member variables
  nsRefPtr<nsDownload> kungFuDeathGrip = this;

  // When the state changed listener is dispatched, queries to the database and
  // the download manager api should reflect what the nsIDownload object would
  // return. So, if a download is done (finished, canceled, etc.), it should
  // first be removed from the current downloads.  We will also have to update
  // the database *before* notifying listeners.  At this point, you can safely
  // dispatch to the observers as well.
  switch (aState) {
    case nsIDownloadManager::DOWNLOAD_BLOCKED_PARENTAL:
    case nsIDownloadManager::DOWNLOAD_BLOCKED_POLICY:
    case nsIDownloadManager::DOWNLOAD_DIRTY:
    case nsIDownloadManager::DOWNLOAD_CANCELED:
    case nsIDownloadManager::DOWNLOAD_FAILED:
      // Transfers are finished, so break the reference cycle
      Finalize();
      break;
#ifdef DOWNLOAD_SCANNER
    case nsIDownloadManager::DOWNLOAD_SCANNING:
    {
      nsresult rv = mDownloadManager->mScanner ? mDownloadManager->mScanner->ScanDownload(this) : NS_ERROR_NOT_INITIALIZED;
      // If we failed, then fall through to 'download finished'
      if (NS_SUCCEEDED(rv))
        break;
      mDownloadState = aState = nsIDownloadManager::DOWNLOAD_FINISHED;
    }
#endif
    case nsIDownloadManager::DOWNLOAD_FINISHED:
    {
      // Do what exthandler would have done if necessary
      nsresult rv = ExecuteDesiredAction();
      if (NS_FAILED(rv)) {
        // We've failed to execute the desired action.  As a result, we should
        // fail the download so the user can try again.
        (void)FailDownload(rv, nsnull);
        return rv;
      }

      // Now that we're done with handling the download, clean it up
      Finalize();

      nsCOMPtr<nsIPrefBranch> pref(do_GetService(NS_PREFSERVICE_CONTRACTID));

      // Master pref to control this function.
      bool showTaskbarAlert = true;
      if (pref)
        pref->GetBoolPref(PREF_BDM_SHOWALERTONCOMPLETE, &showTaskbarAlert);

      if (showTaskbarAlert) {
        PRInt32 alertInterval = 2000;
        if (pref)
          pref->GetIntPref(PREF_BDM_SHOWALERTINTERVAL, &alertInterval);

        PRInt64 alertIntervalUSec = alertInterval * PR_USEC_PER_MSEC;
        PRInt64 goat = PR_Now() - mStartTime;
        showTaskbarAlert = goat > alertIntervalUSec;

        PRInt32 size = mDownloadManager->mCurrentDownloads.Count();
        if (showTaskbarAlert && size == 0) {
          nsCOMPtr<nsIAlertsService> alerts =
            do_GetService("@mozilla.org/alerts-service;1");
          if (alerts) {
              nsXPIDLString title, message;

              mDownloadManager->mBundle->GetStringFromName(
                  NS_LITERAL_STRING("downloadsCompleteTitle").get(),
                  getter_Copies(title));
              mDownloadManager->mBundle->GetStringFromName(
                  NS_LITERAL_STRING("downloadsCompleteMsg").get(),
                  getter_Copies(message));

              bool removeWhenDone =
                mDownloadManager->GetRetentionBehavior() == 0;

              // If downloads are automatically removed per the user's
              // retention policy, there's no reason to make the text clickable
              // because if it is, they'll click open the download manager and
              // the items they downloaded will have been removed.
              alerts->ShowAlertNotification(
                  NS_LITERAL_STRING(DOWNLOAD_MANAGER_ALERT_ICON), title,
                  message, !removeWhenDone, EmptyString(), mDownloadManager,
                  EmptyString());
            }
        }
      }

#if defined(XP_WIN) || defined(XP_MACOSX) || defined(ANDROID)
      nsCOMPtr<nsIFileURL> fileURL = do_QueryInterface(mTarget);
      nsCOMPtr<nsIFile> file;
      nsAutoString path;

      if (fileURL &&
          NS_SUCCEEDED(fileURL->GetFile(getter_AddRefs(file))) &&
          file &&
          NS_SUCCEEDED(file->GetPath(path))) {

#ifdef XP_WIN
        // On windows, add the download to the system's "recent documents"
        // list, with a pref to disable.
        {
          bool addToRecentDocs = true;
          if (pref)
            pref->GetBoolPref(PREF_BDM_ADDTORECENTDOCS, &addToRecentDocs);

          if (addToRecentDocs &&
              !nsDownloadManager::gDownloadManagerService->mInPrivateBrowsing) {
            ::SHAddToRecentDocs(SHARD_PATHW, path.get());
          }
        }
#endif
#ifdef XP_MACOSX
        // On OS X, make the downloads stack bounce.
        CFStringRef observedObject = ::CFStringCreateWithCString(kCFAllocatorDefault,
                                                 NS_ConvertUTF16toUTF8(path).get(),
                                                 kCFStringEncodingUTF8);
        CFNotificationCenterRef center = ::CFNotificationCenterGetDistributedCenter();
        ::CFNotificationCenterPostNotification(center, CFSTR("com.apple.DownloadFileFinished"),
                                               observedObject, NULL, TRUE);
        ::CFRelease(observedObject);
#endif
#ifdef ANDROID
        nsCOMPtr<nsIMIMEInfo> mimeInfo;
        nsCAutoString contentType;
        GetMIMEInfo(getter_AddRefs(mimeInfo));

        if (mimeInfo)
          mimeInfo->GetMIMEType(contentType);

        mozilla::AndroidBridge::Bridge()->ScanMedia(path, contentType);
#endif
      }

#ifdef XP_WIN
      // Adjust file attributes so that by default, new files are indexed
      // by desktop search services. Skip off those that land in the temp
      // folder.
      nsCOMPtr<nsIFile> tempDir, fileDir;
      rv = NS_GetSpecialDirectory(NS_OS_TEMP_DIR, getter_AddRefs(tempDir));
      NS_ENSURE_SUCCESS(rv, rv);
      (void)file->GetParent(getter_AddRefs(fileDir));

      bool isTemp = false;
      if (fileDir)
        (void)fileDir->Equals(tempDir, &isTemp);

      nsCOMPtr<nsILocalFileWin> localFileWin(do_QueryInterface(file));
      if (!isTemp && localFileWin)
        (void)localFileWin->SetFileAttributesWin(nsILocalFileWin::WFA_SEARCH_INDEXED);
#endif

#endif
      // Now remove the download if the user's retention policy is "Remove when Done"
      if (mDownloadManager->GetRetentionBehavior() == 0)
        mDownloadManager->RemoveDownload(mID);
    }
    break;
  default:
    break;
  }

  // Before notifying the listener, we must update the database so that calls
  // to it work out properly.
  nsresult rv = UpdateDB();
  NS_ENSURE_SUCCESS(rv, rv);

  mDownloadManager->NotifyListenersOnDownloadStateChange(oldState, this);

  switch (mDownloadState) {
    case nsIDownloadManager::DOWNLOAD_DOWNLOADING:
      // Only send the dl-start event to downloads that are actually starting.
      if (oldState == nsIDownloadManager::DOWNLOAD_QUEUED)
        mDownloadManager->SendEvent(this, "dl-start");
      break;
    case nsIDownloadManager::DOWNLOAD_FAILED:
      mDownloadManager->SendEvent(this, "dl-failed");
      break;
    case nsIDownloadManager::DOWNLOAD_SCANNING:
      mDownloadManager->SendEvent(this, "dl-scanning");
      break;
    case nsIDownloadManager::DOWNLOAD_FINISHED:
      mDownloadManager->SendEvent(this, "dl-done");
      break;
    case nsIDownloadManager::DOWNLOAD_BLOCKED_PARENTAL:
    case nsIDownloadManager::DOWNLOAD_BLOCKED_POLICY:
      mDownloadManager->SendEvent(this, "dl-blocked");
      break;
    case nsIDownloadManager::DOWNLOAD_DIRTY:
      mDownloadManager->SendEvent(this, "dl-dirty");
      break;
    case nsIDownloadManager::DOWNLOAD_CANCELED:
      mDownloadManager->SendEvent(this, "dl-cancel");
      break;
    default:
      break;
  }
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
//// nsIWebProgressListener2

NS_IMETHODIMP
nsDownload::OnProgressChange64(nsIWebProgress *aWebProgress,
                               nsIRequest *aRequest,
                               PRInt64 aCurSelfProgress,
                               PRInt64 aMaxSelfProgress,
                               PRInt64 aCurTotalProgress,
                               PRInt64 aMaxTotalProgress)
{
  if (!mRequest)
    mRequest = aRequest; // used for pause/resume

  if (mDownloadState == nsIDownloadManager::DOWNLOAD_QUEUED) {
    // Obtain the referrer
    nsresult rv;
    nsCOMPtr<nsIChannel> channel(do_QueryInterface(aRequest));
    nsCOMPtr<nsIURI> referrer = mReferrer;
    if (channel)
      (void)NS_GetReferrerFromChannel(channel, getter_AddRefs(mReferrer));

    // Restore the original referrer if the new one isn't useful
    if (!mReferrer)
      mReferrer = referrer;

    // If we have a MIME info, we know that exthandler has already added this to
    // the history, but if we do not, we'll have to add it ourselves.
    if (!mMIMEInfo) {
      nsCOMPtr<nsIDownloadHistory> dh =
        do_GetService(NS_DOWNLOADHISTORY_CONTRACTID);
      if (dh)
        (void)dh->AddDownload(mSource, mReferrer, mStartTime, mTarget);
    }

    // Fetch the entityID, but if we can't get it, don't panic (non-resumable)
    nsCOMPtr<nsIResumableChannel> resumableChannel(do_QueryInterface(aRequest));
    if (resumableChannel)
      (void)resumableChannel->GetEntityID(mEntityID);

    // Before we update the state and dispatch state notifications, we want to
    // ensure that we have the correct state for this download with regards to
    // its percent completion and size.
    SetProgressBytes(0, aMaxTotalProgress);

    // Update the state and the database
    rv = SetState(nsIDownloadManager::DOWNLOAD_DOWNLOADING);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // filter notifications since they come in so frequently
  PRTime now = PR_Now();
  PRIntervalTime delta = now - mLastUpdate;
  if (delta < gUpdateInterval)
    return NS_OK;

  mLastUpdate = now;

  // Calculate the speed using the elapsed delta time and bytes downloaded
  // during that time for more accuracy.
  double elapsedSecs = double(delta) / PR_USEC_PER_SEC;
  if (elapsedSecs > 0) {
    double speed = double(aCurTotalProgress - mCurrBytes) / elapsedSecs;
    if (mCurrBytes == 0) {
      mSpeed = speed;
    } else {
      // Calculate 'smoothed average' of 10 readings.
      mSpeed = mSpeed * 0.9 + speed * 0.1;
    }
  }

  SetProgressBytes(aCurTotalProgress, aMaxTotalProgress);

  // Report to the listener our real sizes
  PRInt64 currBytes, maxBytes;
  (void)GetAmountTransferred(&currBytes);
  (void)GetSize(&maxBytes);
  mDownloadManager->NotifyListenersOnProgressChange(
    aWebProgress, aRequest, currBytes, maxBytes, currBytes, maxBytes, this);

  // If the maximums are different, then there must be more than one file
  if (aMaxSelfProgress != aMaxTotalProgress)
    mHasMultipleFiles = true;

  return NS_OK;
}

NS_IMETHODIMP
nsDownload::OnRefreshAttempted(nsIWebProgress *aWebProgress,
                               nsIURI *aUri,
                               PRInt32 aDelay,
                               bool aSameUri,
                               bool *allowRefresh)
{
  *allowRefresh = true;
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
//// nsIWebProgressListener

NS_IMETHODIMP
nsDownload::OnProgressChange(nsIWebProgress *aWebProgress,
                             nsIRequest *aRequest,
                             PRInt32 aCurSelfProgress,
                             PRInt32 aMaxSelfProgress,
                             PRInt32 aCurTotalProgress,
                             PRInt32 aMaxTotalProgress)
{
  return OnProgressChange64(aWebProgress, aRequest,
                            aCurSelfProgress, aMaxSelfProgress,
                            aCurTotalProgress, aMaxTotalProgress);
}

NS_IMETHODIMP
nsDownload::OnLocationChange(nsIWebProgress *aWebProgress,
                             nsIRequest *aRequest, nsIURI *aLocation,
                             PRUint32 aFlags)
{
  return NS_OK;
}

NS_IMETHODIMP
nsDownload::OnStatusChange(nsIWebProgress *aWebProgress,
                           nsIRequest *aRequest, nsresult aStatus,
                           const PRUnichar *aMessage)
{
  if (NS_FAILED(aStatus))
    return FailDownload(aStatus, aMessage);
  return NS_OK;
}

NS_IMETHODIMP
nsDownload::OnStateChange(nsIWebProgress *aWebProgress,
                          nsIRequest *aRequest, PRUint32 aStateFlags,
                          nsresult aStatus)
{
  // We don't want to lose access to our member variables
  nsRefPtr<nsDownload> kungFuDeathGrip = this;

  // Check if we're starting a request; the NETWORK flag is necessary to not
  // pick up the START of *each* file but only for the whole request
  if ((aStateFlags & STATE_START) && (aStateFlags & STATE_IS_NETWORK)) {
    nsresult rv;
    nsCOMPtr<nsIHttpChannel> channel = do_QueryInterface(aRequest, &rv);
    if (NS_SUCCEEDED(rv)) {
      PRUint32 status;
      rv = channel->GetResponseStatus(&status);
      // HTTP 450 - Blocked by parental control proxies
      if (NS_SUCCEEDED(rv) && status == 450) {
        // Cancel using the provided object
        (void)Cancel();

        // Fail the download
        (void)SetState(nsIDownloadManager::DOWNLOAD_BLOCKED_PARENTAL);
      }
    }
  } else if ((aStateFlags & STATE_STOP) && (aStateFlags & STATE_IS_NETWORK) &&
             IsFinishable()) {
    // We got both STOP and NETWORK so that means the whole request is done
    // (and not just a single file if there are multiple files)
    if (NS_SUCCEEDED(aStatus)) {
      // We can't completely trust the bytes we've added up because we might be
      // missing on some/all of the progress updates (especially from cache).
      // Our best bet is the file itself, but if for some reason it's gone or
      // if we have multiple files, the next best is what we've calculated.
      PRInt64 fileSize;
      nsCOMPtr<nsILocalFile> file;
      //  We need a nsIFile clone to deal with file size caching issues. :(
      nsCOMPtr<nsIFile> clone;
      if (!mHasMultipleFiles &&
          NS_SUCCEEDED(GetTargetFile(getter_AddRefs(file))) &&
          NS_SUCCEEDED(file->Clone(getter_AddRefs(clone))) &&
          NS_SUCCEEDED(clone->GetFileSize(&fileSize)) && fileSize > 0) {
        mCurrBytes = mMaxBytes = fileSize;

        // If we resumed, keep the fact that we did and fix size calculations
        if (WasResumed())
          mResumedAt = 0;
      } else if (mMaxBytes == -1) {
        mMaxBytes = mCurrBytes;
      } else {
        mCurrBytes = mMaxBytes;
      }

      mPercentComplete = 100;
      mLastUpdate = PR_Now();

#ifdef DOWNLOAD_SCANNER
      bool scan = true;
      nsCOMPtr<nsIPrefBranch> prefs(do_GetService(NS_PREFSERVICE_CONTRACTID));
      if (prefs)
        (void)prefs->GetBoolPref(PREF_BDM_SCANWHENDONE, &scan);

      if (scan)
        (void)SetState(nsIDownloadManager::DOWNLOAD_SCANNING);
      else
        (void)SetState(nsIDownloadManager::DOWNLOAD_FINISHED);
#else
      (void)SetState(nsIDownloadManager::DOWNLOAD_FINISHED);
#endif
    } else {
      // We failed for some unknown reason -- fail with a generic message
      (void)FailDownload(aStatus, nsnull);
    }
  }

  mDownloadManager->NotifyListenersOnStateChange(aWebProgress, aRequest,
                                                 aStateFlags, aStatus, this);
  return NS_OK;
}

NS_IMETHODIMP
nsDownload::OnSecurityChange(nsIWebProgress *aWebProgress,
                             nsIRequest *aRequest, PRUint32 aState)
{
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
//// nsIDownload

NS_IMETHODIMP
nsDownload::Init(nsIURI *aSource,
                 nsIURI *aTarget,
                 const nsAString& aDisplayName,
                 nsIMIMEInfo *aMIMEInfo,
                 PRTime aStartTime,
                 nsILocalFile *aTempFile,
                 nsICancelable *aCancelable)
{
  NS_WARNING("Huh...how did we get here?!");
  return NS_OK;
}

NS_IMETHODIMP
nsDownload::GetState(PRInt16 *aState)
{
  *aState = mDownloadState;
  return NS_OK;
}

NS_IMETHODIMP
nsDownload::GetDisplayName(nsAString &aDisplayName)
{
  aDisplayName = mDisplayName;
  return NS_OK;
}

NS_IMETHODIMP
nsDownload::GetCancelable(nsICancelable **aCancelable)
{
  *aCancelable = mCancelable;
  NS_IF_ADDREF(*aCancelable);
  return NS_OK;
}

NS_IMETHODIMP
nsDownload::GetTarget(nsIURI **aTarget)
{
  *aTarget = mTarget;
  NS_IF_ADDREF(*aTarget);
  return NS_OK;
}

NS_IMETHODIMP
nsDownload::GetSource(nsIURI **aSource)
{
  *aSource = mSource;
  NS_IF_ADDREF(*aSource);
  return NS_OK;
}

NS_IMETHODIMP
nsDownload::GetStartTime(PRInt64 *aStartTime)
{
  *aStartTime = mStartTime;
  return NS_OK;
}

NS_IMETHODIMP
nsDownload::GetPercentComplete(PRInt32 *aPercentComplete)
{
  *aPercentComplete = mPercentComplete;
  return NS_OK;
}

NS_IMETHODIMP
nsDownload::GetAmountTransferred(PRInt64 *aAmountTransferred)
{
  *aAmountTransferred = mCurrBytes + (WasResumed() ? mResumedAt : 0);
  return NS_OK;
}

NS_IMETHODIMP
nsDownload::GetSize(PRInt64 *aSize)
{
  *aSize = mMaxBytes + (WasResumed() && mMaxBytes != -1 ? mResumedAt : 0);
  return NS_OK;
}

NS_IMETHODIMP
nsDownload::GetMIMEInfo(nsIMIMEInfo **aMIMEInfo)
{
  *aMIMEInfo = mMIMEInfo;
  NS_IF_ADDREF(*aMIMEInfo);
  return NS_OK;
}

NS_IMETHODIMP
nsDownload::GetTargetFile(nsILocalFile **aTargetFile)
{
  nsresult rv;

  nsCOMPtr<nsIFileURL> fileURL = do_QueryInterface(mTarget, &rv);
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsIFile> file;
  rv = fileURL->GetFile(getter_AddRefs(file));
  if (NS_SUCCEEDED(rv))
    rv = CallQueryInterface(file, aTargetFile);
  return rv;
}

NS_IMETHODIMP
nsDownload::GetSpeed(double *aSpeed)
{
  *aSpeed = mSpeed;
  return NS_OK;
}

NS_IMETHODIMP
nsDownload::GetId(PRUint32 *aId)
{
  *aId = mID;
  return NS_OK;
}

NS_IMETHODIMP
nsDownload::GetReferrer(nsIURI **referrer)
{
  NS_IF_ADDREF(*referrer = mReferrer);
  return NS_OK;
}

NS_IMETHODIMP
nsDownload::GetResumable(bool *resumable)
{
  *resumable = IsResumable();
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
//// nsDownload Helper Functions

void
nsDownload::Finalize()
{
  // We're stopping, so break the cycle we created at download start
  mCancelable = nsnull;

  // Reset values that aren't needed anymore, so the DB can be updated as well
  mEntityID.Truncate();
  mTempFile = nsnull;

  // Remove ourself from the active downloads
  (void)mDownloadManager->mCurrentDownloads.RemoveObject(this);

  // Make sure we do not automatically resume
  mAutoResume = DONT_RESUME;
}

nsresult
nsDownload::ExecuteDesiredAction()
{
  // If we have a temp file and we have resumed, we have to do what the
  // external helper app service would have done.
  if (!mTempFile || !WasResumed())
    return NS_OK;

  // We need to bail if for some reason the temp file got removed
  bool fileExists;
  if (NS_FAILED(mTempFile->Exists(&fileExists)) || !fileExists)
    return NS_ERROR_FILE_NOT_FOUND;

  // Assume an unknown action is save to disk
  nsHandlerInfoAction action = nsIMIMEInfo::saveToDisk;
  if (mMIMEInfo) {
    nsresult rv = mMIMEInfo->GetPreferredAction(&action);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  nsresult retVal = NS_OK;
  switch (action) {
    case nsIMIMEInfo::saveToDisk:
      // Move the file to the proper location
      retVal = MoveTempToTarget();
      break;
    case nsIMIMEInfo::useHelperApp:
    case nsIMIMEInfo::useSystemDefault:
      // For these cases we have to move the file to the target location and
      // open with the appropriate application
      retVal = OpenWithApplication();
      break;
    default:
      break;
  }

  return retVal;
}

nsresult
nsDownload::MoveTempToTarget()
{
  nsCOMPtr<nsILocalFile> target;
  nsresult rv = GetTargetFile(getter_AddRefs(target));
  NS_ENSURE_SUCCESS(rv, rv);

  // MoveTo will fail if the file already exists, but we've already obtained
  // confirmation from the user that this is OK, so remove it if it exists.
  bool fileExists;
  if (NS_SUCCEEDED(target->Exists(&fileExists)) && fileExists) {
    rv = target->Remove(false);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // Extract the new leaf name from the file location
  nsAutoString fileName;
  rv = target->GetLeafName(fileName);
  NS_ENSURE_SUCCESS(rv, rv);
  nsCOMPtr<nsIFile> dir;
  rv = target->GetParent(getter_AddRefs(dir));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = mTempFile->MoveTo(dir, fileName);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
nsDownload::OpenWithApplication()
{
  // First move the temporary file to the target location
  nsCOMPtr<nsILocalFile> target;
  nsresult rv = GetTargetFile(getter_AddRefs(target));
  NS_ENSURE_SUCCESS(rv, rv);

  // Move the temporary file to the target location
  rv = MoveTempToTarget();
  NS_ENSURE_SUCCESS(rv, rv);

  // We do not verify the return value here because, irrespective of success
  // or failure of the method, the deletion of temp file has to take place, as
  // per the corresponding preference. But we store this separately as this is
  // what we ultimately return from this function.
  nsresult retVal = mMIMEInfo->LaunchWithFile(target);

  bool deleteTempFileOnExit;
  nsCOMPtr<nsIPrefBranch> prefs(do_GetService(NS_PREFSERVICE_CONTRACTID));
  if (!prefs || NS_FAILED(prefs->GetBoolPref(PREF_BH_DELETETEMPFILEONEXIT,
                                             &deleteTempFileOnExit))) {
    // No prefservice or no pref set; use default value
#if !defined(XP_MACOSX)
    // Mac users have been very verbal about temp files being deleted on
    // app exit - they don't like it - but we'll continue to do this on
    // other platforms for now.
    deleteTempFileOnExit = true;
#else
    deleteTempFileOnExit = false;
#endif
  }

  // Always schedule files to be deleted at the end of the private browsing
  // mode, regardless of the value of the pref.
  if (deleteTempFileOnExit ||
      nsDownloadManager::gDownloadManagerService->mInPrivateBrowsing) {
    // Use the ExternalHelperAppService to push the temporary file to the list
    // of files to be deleted on exit.
    nsCOMPtr<nsPIExternalAppLauncher> appLauncher(do_GetService
                    (NS_EXTERNALHELPERAPPSERVICE_CONTRACTID));

    // Even if we are unable to get this service we return the result
    // of LaunchWithFile() which makes more sense.
    if (appLauncher)
      (void)appLauncher->DeleteTemporaryFileOnExit(target);
  }

  return retVal;
}

void
nsDownload::SetStartTime(PRInt64 aStartTime)
{
  mStartTime = aStartTime;
  mLastUpdate = aStartTime;
}

void
nsDownload::SetProgressBytes(PRInt64 aCurrBytes, PRInt64 aMaxBytes)
{
  mCurrBytes = aCurrBytes;
  mMaxBytes = aMaxBytes;

  // Get the real bytes that include resume position
  PRInt64 currBytes, maxBytes;
  (void)GetAmountTransferred(&currBytes);
  (void)GetSize(&maxBytes);

  if (currBytes == maxBytes)
    mPercentComplete = 100;
  else if (maxBytes <= 0)
    mPercentComplete = -1;
  else
    mPercentComplete = (PRInt32)((PRFloat64)currBytes / maxBytes * 100 + .5);
}

nsresult
nsDownload::Pause()
{
  if (!IsResumable())
    return NS_ERROR_UNEXPECTED;

  nsresult rv = Cancel();
  NS_ENSURE_SUCCESS(rv, rv);

  return SetState(nsIDownloadManager::DOWNLOAD_PAUSED);
}

nsresult
nsDownload::Cancel()
{
  nsresult rv = NS_OK;
  if (mCancelable) {
    rv = mCancelable->Cancel(NS_BINDING_ABORTED);
    // we're done with this, so break the cycle
    mCancelable = nsnull;
  }

  return rv;
}

nsresult
nsDownload::Resume()
{
  if (!IsPaused() || !IsResumable())
    return NS_ERROR_UNEXPECTED;

  nsresult rv;
  nsCOMPtr<nsIWebBrowserPersist> wbp =
    do_CreateInstance("@mozilla.org/embedding/browser/nsWebBrowserPersist;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = wbp->SetPersistFlags(nsIWebBrowserPersist::PERSIST_FLAGS_APPEND_TO_FILE |
                            nsIWebBrowserPersist::PERSIST_FLAGS_AUTODETECT_APPLY_CONVERSION);
  NS_ENSURE_SUCCESS(rv, rv);

  // Create a new channel for the source URI
  nsCOMPtr<nsIChannel> channel;
  nsCOMPtr<nsIInterfaceRequestor> ir(do_QueryInterface(wbp));
  rv = NS_NewChannel(getter_AddRefs(channel), mSource, nsnull, nsnull, ir);
  NS_ENSURE_SUCCESS(rv, rv);

  // Make sure we can get a file, either the temporary or the real target, for
  // both purposes of file size and a target to write to
  nsCOMPtr<nsILocalFile> targetLocalFile(mTempFile);
  if (!targetLocalFile) {
    rv = GetTargetFile(getter_AddRefs(targetLocalFile));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // Get the file size to be used as an offset, but if anything goes wrong
  // along the way, we'll silently restart at 0.
  PRInt64 fileSize;
  //  We need a nsIFile clone to deal with file size caching issues. :(
  nsCOMPtr<nsIFile> clone;
  if (NS_FAILED(targetLocalFile->Clone(getter_AddRefs(clone))) ||
      NS_FAILED(clone->GetFileSize(&fileSize)))
    fileSize = 0;

  // Set the channel to resume at the right position along with the entityID
  nsCOMPtr<nsIResumableChannel> resumableChannel(do_QueryInterface(channel));
  if (!resumableChannel)
    return NS_ERROR_UNEXPECTED;
  rv = resumableChannel->ResumeAt(fileSize, mEntityID);
  NS_ENSURE_SUCCESS(rv, rv);

  // If we know the max size, we know what it should be when resuming
  PRInt64 maxBytes;
  GetSize(&maxBytes);
  SetProgressBytes(0, maxBytes != -1 ? maxBytes - fileSize : -1);
  // Track where we resumed because progress notifications restart at 0
  mResumedAt = fileSize;

  // Set the referrer
  if (mReferrer) {
    nsCOMPtr<nsIHttpChannel> httpChannel(do_QueryInterface(channel));
    if (httpChannel) {
      rv = httpChannel->SetReferrer(mReferrer);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  // Creates a cycle that will be broken when the download finishes
  mCancelable = wbp;
  (void)wbp->SetProgressListener(this);

  // Save the channel using nsIWBP
  rv = wbp->SaveChannel(channel, targetLocalFile);
  if (NS_FAILED(rv)) {
    mCancelable = nsnull;
    (void)wbp->SetProgressListener(nsnull);
    return rv;
  }

  return SetState(nsIDownloadManager::DOWNLOAD_DOWNLOADING);
}

bool
nsDownload::IsPaused()
{
  return mDownloadState == nsIDownloadManager::DOWNLOAD_PAUSED;
}

bool
nsDownload::IsResumable()
{
  return !mEntityID.IsEmpty();
}

bool
nsDownload::WasResumed()
{
  return mResumedAt != -1;
}

bool
nsDownload::ShouldAutoResume()
{
  return mAutoResume == AUTO_RESUME;
}

bool
nsDownload::IsFinishable()
{
  return mDownloadState == nsIDownloadManager::DOWNLOAD_NOTSTARTED ||
         mDownloadState == nsIDownloadManager::DOWNLOAD_QUEUED ||
         mDownloadState == nsIDownloadManager::DOWNLOAD_DOWNLOADING;
}

bool
nsDownload::IsFinished()
{
  return mDownloadState == nsIDownloadManager::DOWNLOAD_FINISHED;
}

nsresult
nsDownload::UpdateDB()
{
  NS_ASSERTION(mID, "Download ID is stored as zero.  This is bad!");
  NS_ASSERTION(mDownloadManager, "Egads!  We have no download manager!");

  mozIStorageStatement *stmt = mDownloadManager->mUpdateDownloadStatement;

  nsAutoString tempPath;
  if (mTempFile)
    (void)mTempFile->GetPath(tempPath);
  nsresult rv = stmt->BindStringByName(NS_LITERAL_CSTRING("tempPath"), tempPath);

  rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("startTime"), mStartTime);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("endTime"), mLastUpdate);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = stmt->BindInt32ByName(NS_LITERAL_CSTRING("state"), mDownloadState);
  NS_ENSURE_SUCCESS(rv, rv);

  if (mReferrer) {
    nsCAutoString referrer;
    rv = mReferrer->GetSpec(referrer);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = stmt->BindUTF8StringByName(NS_LITERAL_CSTRING("referrer"), referrer);
  } else {
    rv = stmt->BindNullByName(NS_LITERAL_CSTRING("referrer"));
  }
  NS_ENSURE_SUCCESS(rv, rv);

  rv = stmt->BindUTF8StringByName(NS_LITERAL_CSTRING("entityID"), mEntityID);
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt64 currBytes;
  (void)GetAmountTransferred(&currBytes);
  rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("currBytes"), currBytes);
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt64 maxBytes;
  (void)GetSize(&maxBytes);
  rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("maxBytes"), maxBytes);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = stmt->BindInt32ByName(NS_LITERAL_CSTRING("autoResume"), mAutoResume);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("id"), mID);
  NS_ENSURE_SUCCESS(rv, rv);

  return stmt->Execute();
}

nsresult
nsDownload::FailDownload(nsresult aStatus, const PRUnichar *aMessage)
{
  // Grab the bundle before potentially losing our member variables
  nsCOMPtr<nsIStringBundle> bundle = mDownloadManager->mBundle;

  (void)SetState(nsIDownloadManager::DOWNLOAD_FAILED);

  // Get title for alert.
  nsXPIDLString title;
  nsresult rv = bundle->GetStringFromName(
    NS_LITERAL_STRING("downloadErrorAlertTitle").get(), getter_Copies(title));
  NS_ENSURE_SUCCESS(rv, rv);

  // Get a generic message if we weren't supplied one
  nsXPIDLString message;
  message = aMessage;
  if (message.IsEmpty()) {
    rv = bundle->GetStringFromName(
      NS_LITERAL_STRING("downloadErrorGeneric").get(), getter_Copies(message));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // Get Download Manager window to be parent of alert
  nsCOMPtr<nsIWindowMediator> wm =
    do_GetService(NS_WINDOWMEDIATOR_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  nsCOMPtr<nsIDOMWindow> dmWindow;
  rv = wm->GetMostRecentWindow(NS_LITERAL_STRING("Download:Manager").get(),
                               getter_AddRefs(dmWindow));
  NS_ENSURE_SUCCESS(rv, rv);

  // Show alert
  nsCOMPtr<nsIPromptService> prompter =
    do_GetService("@mozilla.org/embedcomp/prompt-service;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  return prompter->Alert(dmWindow, title, message);
}
