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

#include "nsDownloadManager.h"
#include "nsIWebProgress.h"
#include "nsIRDFService.h"
#include "nsIRDFContainer.h"
#include "nsIRDFLiteral.h"
#include "rdf.h"
#include "nsNetUtil.h"
#include "nsIURL.h"
#include "nsIDOMChromeWindow.h"
#include "nsIDOMWindow.h"
#include "nsIDOMWindowInternal.h"
#include "nsIDOMEvent.h"
#include "nsIDOMEventTarget.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsDirectoryServiceDefs.h"
#include "nsIWindowWatcher.h"
#include "nsIWindowMediator.h"
#include "nsIPromptService.h"
#include "nsIPrefBranch.h"
#include "nsIPrefService.h"
#include "nsVoidArray.h"
#include "nsEnumeratorUtils.h"
#include "nsIFileURL.h"
#include "nsEmbedCID.h"
#include "mozStorageCID.h"
#include "mozIStorageService.h"
#include "mozStorageHelper.h"
#include "nsIMutableArray.h"
#include "nsIAlertsService.h"
#include "nsIPropertyBag2.h"
#include "nsIHttpChannel.h"
#include "nsIDownloadManagerUI.h"
#include "nsTArray.h"
#include "nsIResumableChannel.h"

#ifdef XP_WIN
#include <shlobj.h>
#include "nsDownloadScanner.h"
#endif

static PRBool gStoppingDownloads = PR_FALSE;

#define DOWNLOAD_MANAGER_BUNDLE "chrome://mozapps/locale/downloads/downloads.properties"
#define DOWNLOAD_MANAGER_ALERT_ICON "chrome://mozapps/skin/downloads/downloadIcon.png"
#define PREF_BDM_SHOWALERTONCOMPLETE "browser.download.manager.showAlertOnComplete"
#define PREF_BDM_SHOWALERTINTERVAL "browser.download.manager.showAlertInterval"
#define PREF_BDM_RETENTION "browser.download.manager.retention"
#define PREF_BDM_CLOSEWHENDONE "browser.download.manager.closeWhenDone"
#define PREF_BDM_ADDTORECENTDOCS "browser.download.manager.addToRecentDocs"

static const PRInt64 gUpdateInterval = 400 * PR_USEC_PER_MSEC;

#define DM_SCHEMA_VERSION      5
#define DM_DB_NAME             NS_LITERAL_STRING("downloads.sqlite")
#define DM_DB_CORRUPT_FILENAME NS_LITERAL_STRING("downloads.sqlite.corrupt")

///////////////////////////////////////////////////////////////////////////////
// nsDownloadManager

NS_IMPL_ISUPPORTS2(nsDownloadManager, nsIDownloadManager, nsIObserver)

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
#ifdef XP_WIN
  delete mScanner;
#endif
  gDownloadManagerService = nsnull;
}

nsresult
nsDownloadManager::RemoveAllDownloads()
{
  nsresult rv = NS_OK;
  for (PRInt32 i = mCurrentDownloads.Count() - 1; i >= 0; --i) {
    nsRefPtr<nsDownload> dl = mCurrentDownloads[0];

    nsresult result;
    if (dl->IsRealPaused())
      result = mCurrentDownloads.RemoveObject(dl);
    else
      result = CancelDownload(dl->mID);

    // Track the failure, but don't miss out on other downloads
    if (NS_FAILED(result))
      rv = result;
  }

  return rv;
}

void
nsDownloadManager::CompleteDownload(nsDownload *aDownload)
{
  // we've stopped, so break the cycle we created at download start
  aDownload->mCancelable = nsnull;
  aDownload->mEntityID.Truncate();

  // we need do what exthandler would have done for a finished download
  if (aDownload->mDownloadState == nsIDownloadManager::DOWNLOAD_FINISHED &&
      aDownload->WasResumed())
    (void)ExecuteDesiredAction(aDownload);

  (void)mCurrentDownloads.RemoveObject(aDownload);
}

nsresult
nsDownloadManager::ExecuteDesiredAction(nsDownload *aDownload)
{
  // If we have a temp file and we have resumed, we have to do what the external
  // helper app service would have done.
  if (!aDownload->mTempFile || !aDownload->WasResumed())
    return NS_OK;

  // We need to bail if for some reason the temp file got removed
  PRBool fileExists;
  if (NS_FAILED(aDownload->mTempFile->Exists(&fileExists)) || !fileExists)
    return NS_ERROR_FAILURE;

  // Find out if it was a SaveToDisk kind of a download
  nsHandlerInfoAction action = nsIMIMEInfo::saveToDisk;
  nsresult rv;
  if (aDownload->mMIMEInfo) {
    rv = aDownload->mMIMEInfo->GetPreferredAction(&action);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  switch (action) {
    case nsIMIMEInfo::saveToDisk:
      // For this instance, we need to move the file to the proper location
      {
        nsCOMPtr<nsILocalFile> target;
        rv = aDownload->GetTargetFile(getter_AddRefs(target));
        NS_ENSURE_SUCCESS(rv, rv);

        // MoveTo will fail if the file already exists, but we've already
        // obtained confirmation from the user that this is OK.  So, we have
        // to remove it if it exists.
        if (NS_SUCCEEDED(target->Exists(&fileExists)) && fileExists) {
          rv = target->Remove(PR_FALSE);
          NS_ENSURE_SUCCESS(rv, rv);
        }

        // extract the new leaf name from the file location
        nsAutoString fileName;
        rv = target->GetLeafName(fileName);
        NS_ENSURE_SUCCESS(rv, rv);
        nsCOMPtr<nsIFile> dir;
        rv = target->GetParent(getter_AddRefs(dir));
        NS_ENSURE_SUCCESS(rv, rv);
        if (dir) {
          rv = aDownload->mTempFile->MoveTo(dir, fileName);
          NS_ENSURE_SUCCESS(rv, rv);
        }
      }
      break;
    default:
      break;
  }

  return NS_OK;
}

nsresult
nsDownloadManager::InitDB(PRBool *aDoImport)
{
  nsresult rv;
  *aDoImport = PR_FALSE;

  nsCOMPtr<mozIStorageService> storage =
    do_GetService(MOZ_STORAGE_SERVICE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIFile> dbFile;
  rv = NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR,
                              getter_AddRefs(dbFile));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = dbFile->Append(DM_DB_NAME);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = storage->OpenDatabase(dbFile, getter_AddRefs(mDBConn));
  if (rv == NS_ERROR_FILE_CORRUPTED) {
    // delete and try again
    rv = dbFile->Remove(PR_TRUE);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = storage->OpenDatabase(dbFile, getter_AddRefs(mDBConn));
  }
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool tableExists;
  rv = mDBConn->TableExists(NS_LITERAL_CSTRING("moz_downloads"), &tableExists);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!tableExists) {
    *aDoImport = PR_TRUE;
    rv = CreateTable();
    NS_ENSURE_SUCCESS(rv, rv);
    return NS_OK;
  }

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
      mozStorageTransaction safeTransaction(mDBConn, PR_TRUE);

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
               "referrer, entityID "
        "FROM moz_downloads"), getter_AddRefs(stmt));
      if (NS_SUCCEEDED(rv))
        break;

      // if the statement fails, that means all the columns were not there.
      // First we backup the database
      nsCOMPtr<nsIFile> backup;
      rv = mDBConn->BackupDB(DM_DB_CORRUPT_FILENAME, nsnull,
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
      "entityID TEXT"
    ")"));
}

nsresult
nsDownloadManager::ImportDownloadHistory()
{
  nsCOMPtr<nsIFile> dlFile;
  nsresult rv = NS_GetSpecialDirectory(NS_APP_DOWNLOADS_50_FILE,
                                       getter_AddRefs(dlFile));
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool check;
  rv = dlFile->Exists(&check);
  if (NS_FAILED(rv) || !check)
    return rv;

  rv = dlFile->IsFile(&check);
  if (NS_FAILED(rv) || !check)
    return rv;

  nsCAutoString dlSrc;
  rv = NS_GetURLSpecFromFile(dlFile, dlSrc);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIRDFService> rdfs =
    do_GetService("@mozilla.org/rdf/rdf-service;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIRDFDataSource> ds;
  rv = rdfs->GetDataSourceBlocking(dlSrc.get(), getter_AddRefs(ds));
  NS_ENSURE_SUCCESS(rv, rv);

  // OK, we now have our datasouce, so lets get our resources
  nsCOMPtr<nsIRDFResource> NC_DownloadsRoot;
  rv = rdfs->GetResource(NS_LITERAL_CSTRING("NC:DownloadsRoot"),
                         getter_AddRefs(NC_DownloadsRoot));
  NS_ENSURE_SUCCESS(rv, rv);
  nsCOMPtr<nsIRDFResource> NC_Name;
  rv = rdfs->GetResource(NS_LITERAL_CSTRING(NC_NAMESPACE_URI "Name"),
                         getter_AddRefs(NC_Name));
  NS_ENSURE_SUCCESS(rv, rv);
  nsCOMPtr<nsIRDFResource> NC_URL;
  rv = rdfs->GetResource(NS_LITERAL_CSTRING(NC_NAMESPACE_URI "URL"),
                         getter_AddRefs(NC_URL));
  NS_ENSURE_SUCCESS(rv, rv);
  nsCOMPtr<nsIRDFResource> NC_File;
  rv = rdfs->GetResource(NS_LITERAL_CSTRING(NC_NAMESPACE_URI "File"),
                         getter_AddRefs(NC_File));
  NS_ENSURE_SUCCESS(rv, rv);
  nsCOMPtr<nsIRDFResource> NC_DateStarted;
  rv = rdfs->GetResource(NS_LITERAL_CSTRING(NC_NAMESPACE_URI "DateStarted"),
                         getter_AddRefs(NC_DateStarted));
  NS_ENSURE_SUCCESS(rv, rv);
  nsCOMPtr<nsIRDFResource> NC_DateEnded;
  rv = rdfs->GetResource(NS_LITERAL_CSTRING(NC_NAMESPACE_URI "DateEnded"),
                         getter_AddRefs(NC_DateEnded));
  NS_ENSURE_SUCCESS(rv, rv);
  nsCOMPtr<nsIRDFResource> NC_DownloadState;
  rv = rdfs->GetResource(NS_LITERAL_CSTRING(NC_NAMESPACE_URI "DownloadState"),
                         getter_AddRefs(NC_DownloadState));
  NS_ENSURE_SUCCESS(rv, rv);

  mozStorageTransaction transaction(mDBConn, PR_TRUE);

  // OK, now we can actually start to read and process our data
  nsCOMPtr<nsIRDFContainer> container =
    do_CreateInstance(NS_RDF_CONTRACTID "/container;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = container->Init(ds, NC_DownloadsRoot);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsISimpleEnumerator> dls;
  rv = container->GetElements(getter_AddRefs(dls));
  NS_ENSURE_SUCCESS(rv, rv);
  PRBool hasMore;
  while (NS_SUCCEEDED(dls->HasMoreElements(&hasMore)) && hasMore) {
    nsCOMPtr<nsISupports> itm;
    rv = dls->GetNext(getter_AddRefs(itm));
    if (NS_FAILED(rv)) continue;

    nsCOMPtr<nsIRDFResource> dl = do_QueryInterface(itm, &rv);
    if (NS_FAILED(rv)) continue;

    nsCOMPtr<nsIRDFNode> node;
    // Getting the data
    nsString name;
    nsCString source, target;
    PRInt64 startTime, endTime;
    PRInt32 state;

    rv = ds->GetTarget(dl, NC_Name, PR_TRUE, getter_AddRefs(node));
    if (NS_FAILED(rv)) continue;
    nsCOMPtr<nsIRDFLiteral> rdfLit = do_QueryInterface(node, &rv);
    if (NS_FAILED(rv)) continue;
    rv = rdfLit->GetValue(getter_Copies(name));
    if (NS_FAILED(rv)) continue;

    rv = ds->GetTarget(dl, NC_URL, PR_TRUE, getter_AddRefs(node));
    if (NS_FAILED(rv)) continue;
    nsCOMPtr<nsIRDFResource> rdfRes = do_QueryInterface(node, &rv);
    if (NS_FAILED(rv)) continue;
    rv = rdfRes->GetValueUTF8(source);
    if (NS_FAILED(rv)) continue;

    rv = ds->GetTarget(dl, NC_File, PR_TRUE, getter_AddRefs(node));
    if (NS_FAILED(rv)) continue;
    rdfRes = do_QueryInterface(node, &rv);
    if (NS_FAILED(rv)) continue;
    rv = rdfRes->GetValueUTF8(target);
    if (NS_FAILED(rv)) continue;

    rv = ds->GetTarget(dl, NC_DateStarted, PR_TRUE, getter_AddRefs(node));
    if (NS_FAILED(rv) || !node) {
      rv = ds->GetTarget(dl, NC_DateEnded, PR_TRUE, getter_AddRefs(node));
      if (NS_FAILED(rv)) continue;
    }
    nsCOMPtr<nsIRDFDate> rdfDate = do_QueryInterface(node, &rv);
    if (NS_FAILED(rv)) continue;
    rv = rdfDate->GetValue(&startTime);
    if (NS_FAILED(rv)) continue;

    rv = ds->GetTarget(dl, NC_DateEnded, PR_TRUE, getter_AddRefs(node));
    if (NS_FAILED(rv)) continue;
    rdfDate = do_QueryInterface(node, &rv);
    if (NS_FAILED(rv)) continue;
    rv = rdfDate->GetValue(&endTime);
    if (NS_FAILED(rv)) continue;

    rv = ds->GetTarget(dl, NC_DownloadState, PR_TRUE, getter_AddRefs(node));
    if (NS_FAILED(rv)) continue;
    nsCOMPtr<nsIRDFInt> rdfInt = do_QueryInterface(node, &rv);
    if (NS_FAILED(rv)) continue;
    rv = rdfInt->GetValue(&state);
    if (NS_FAILED(rv)) continue;

    (void)AddDownloadToDB(name, source, target, EmptyString(), startTime,
                          endTime, state);
  }

  return NS_OK;
}

nsresult
nsDownloadManager::RestoreDatabaseState()
{
  // First, we get all the downloads that are supposedly active, but are not
  // really because we crashed.
  nsCOMPtr<mozIStorageStatement> stmt;
  nsresult rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
    "SELECT id "
    "FROM moz_downloads "
    "WHERE state = ?1 "
      "OR state = ?2 "
      "OR state = ?3"), getter_AddRefs(stmt));
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt32 i = 0;
  rv = stmt->BindInt32Parameter(i++, nsIDownloadManager::DOWNLOAD_NOTSTARTED);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = stmt->BindInt32Parameter(i++, nsIDownloadManager::DOWNLOAD_QUEUED);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = stmt->BindInt32Parameter(i++, nsIDownloadManager::DOWNLOAD_DOWNLOADING);
  NS_ENSURE_SUCCESS(rv, rv);

  // Next, we iterate through them storing them in an array
  nsTArray<PRUint32> ids;
  PRBool hasResults;
  while (NS_SUCCEEDED(stmt->ExecuteStep(&hasResults)) && hasResults)
    (void)ids.AppendElement(stmt->AsInt32(0));

  rv = stmt->Reset();
  NS_ENSURE_SUCCESS(rv, rv);

  // Third, change all of those downloads to a failed state so we will be able
  // to retry them.
  rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
    "UPDATE moz_downloads "
    "SET state = ?1 "
    "WHERE state = ?2 "
      "OR state = ?3 "
      "OR state = ?4"), getter_AddRefs(stmt));
  NS_ENSURE_SUCCESS(rv, rv);

  i = 0;
  rv = stmt->BindInt32Parameter(i++, nsIDownloadManager::DOWNLOAD_FAILED);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = stmt->BindInt32Parameter(i++, nsIDownloadManager::DOWNLOAD_NOTSTARTED);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = stmt->BindInt32Parameter(i++, nsIDownloadManager::DOWNLOAD_QUEUED);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = stmt->BindInt32Parameter(i++, nsIDownloadManager::DOWNLOAD_DOWNLOADING);
  NS_ENSURE_SUCCESS(rv, rv);

  // Finally, let's retry all of those downloads
  for (nsTArray<PRUint32>::size_type i = 0; i < ids.Length(); i++)
    (void)RetryDownload(ids[i]);

  return NS_OK;
}

nsresult
nsDownloadManager::RestoreActiveDownloads()
{
  nsCOMPtr<mozIStorageStatement> stmt;
  nsresult rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
    "SELECT id "
    "FROM moz_downloads "
    "WHERE state = ?1 "
      "AND LENGTH(entityID) > 0"), getter_AddRefs(stmt));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = stmt->BindInt32Parameter(0, nsIDownloadManager::DOWNLOAD_PAUSED);
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool hasResults;
  while (NS_SUCCEEDED(stmt->ExecuteStep(&hasResults)) && hasResults) {
    nsRefPtr<nsDownload> dl;
    // Keep trying to add even if we fail one, but make sure to return failure.
    // Additionally, be careful to not call anything that tries to change the
    // database because we're iterating over a live statement.
    if (NS_FAILED(GetDownloadFromDB(stmt->AsInt32(0), getter_AddRefs(dl))) ||
        NS_FAILED(AddToCurrentDownloads(dl)))
      rv = NS_ERROR_FAILURE;
  }
  return rv;
}

PRInt64
nsDownloadManager::AddDownloadToDB(const nsAString &aName,
                                   const nsACString &aSource,
                                   const nsACString &aTarget,
                                   const nsAString &aTempPath,
                                   PRInt64 aStartTime,
                                   PRInt64 aEndTime,
                                   PRInt32 aState)
{
  nsCOMPtr<mozIStorageStatement> stmt;
  nsresult rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
    "INSERT INTO moz_downloads "
    "(name, source, target, tempPath, startTime, endTime, state) "
    "VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7)"), getter_AddRefs(stmt));
  NS_ENSURE_SUCCESS(rv, 0);

  PRInt32 i = 0;
  // name
  rv = stmt->BindStringParameter(i++, aName);
  NS_ENSURE_SUCCESS(rv, 0);

  // source
  rv = stmt->BindUTF8StringParameter(i++, aSource);
  NS_ENSURE_SUCCESS(rv, 0);

  // target
  rv = stmt->BindUTF8StringParameter(i++, aTarget);
  NS_ENSURE_SUCCESS(rv, 0);

  // tempPath
  rv = stmt->BindStringParameter(i++, aTempPath);
  NS_ENSURE_SUCCESS(rv, 0);

  // startTime
  rv = stmt->BindInt64Parameter(i++, aStartTime);
  NS_ENSURE_SUCCESS(rv, 0);

  // endTime
  rv = stmt->BindInt64Parameter(i++, aEndTime);
  NS_ENSURE_SUCCESS(rv, 0);

  // state
  rv = stmt->BindInt32Parameter(i++, aState);
  NS_ENSURE_SUCCESS(rv, 0);

  PRBool hasMore;
  rv = stmt->ExecuteStep(&hasMore); // we want to keep our lock
  NS_ENSURE_SUCCESS(rv, 0);

  PRInt64 id = 0;
  rv = mDBConn->GetLastInsertRowID(&id);
  NS_ENSURE_SUCCESS(rv, 0);

  // lock on DB from statement will be released once we return
  return id;
}

nsresult
nsDownloadManager::Init()
{
  nsresult rv;
  mObserverService = do_GetService("@mozilla.org/observer-service;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool doImport;
  rv = InitDB(&doImport);
  NS_ENSURE_SUCCESS(rv, rv);

  if (doImport)
    ImportDownloadHistory();

  rv = RestoreDatabaseState();
  NS_ENSURE_SUCCESS(rv, rv);

  rv = RestoreActiveDownloads();
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIStringBundleService> bundleService =
    do_GetService(NS_STRINGBUNDLE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = bundleService->CreateBundle(DOWNLOAD_MANAGER_BUNDLE,
                                   getter_AddRefs(mBundle));
  NS_ENSURE_SUCCESS(rv, rv);

#ifdef XP_WIN
  mScanner = new nsDownloadScanner();
  if (!mScanner)
    return NS_ERROR_OUT_OF_MEMORY;
  rv = mScanner->Init();
  if (NS_FAILED(rv)) {
    delete mScanner;
    mScanner = nsnull;
  }
#endif

  rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
    "UPDATE moz_downloads "
    "SET startTime = ?1, endTime = ?2, state = ?3, referrer = ?4, entityID = ?5 "
    "WHERE id = ?6"), getter_AddRefs(mUpdateDownloadStatement));
  NS_ENSURE_SUCCESS(rv, rv);

  // The following three AddObserver calls must be the last lines in this function,
  // because otherwise, this function may fail (and thus, this object would be not
  // completely initialized), but the observerservice would still keep a reference
  // to us and notify us about shutdown, which may cause crashes.
  // failure to add an observer is not critical
  //
  // These observers will be cleaned up automatically at app shutdown.  We do
  // not bother explicitly breaking the observers because we are a singleton
  // that lives for the duration of the app.
  //
  mObserverService->AddObserver(this, "quit-application", PR_FALSE);
  mObserverService->AddObserver(this, "quit-application-requested", PR_FALSE);
  mObserverService->AddObserver(this, "offline-requested", PR_FALSE);

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

nsresult
nsDownloadManager::GetDownloadFromDB(PRUint32 aID, nsDownload **retVal)
{
  NS_ASSERTION(!FindDownload(aID),
               "If it is a current download, you should not call this method!");

  // First, let's query the database and see if it even exists
  nsCOMPtr<mozIStorageStatement> stmt;
  nsresult rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
    "SELECT id, state, startTime, source, target, tempPath, name, referrer, "
           "entityID "
    "FROM moz_downloads "
    "WHERE id = ?1"), getter_AddRefs(stmt));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = stmt->BindInt64Parameter(0, aID);
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool hasResults = PR_FALSE;
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
    rv = NS_NewLocalFile(tempPath, PR_TRUE, getter_AddRefs(dl->mTempFile));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  stmt->GetString(i++, dl->mDisplayName);

  nsCString referrer;
  rv = stmt->GetUTF8String(i++, referrer);
  if (NS_SUCCEEDED(rv) && !referrer.IsEmpty()) {
    rv = NS_NewURI(getter_AddRefs(dl->mReferrer), referrer);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  nsCOMPtr<nsILocalFile> file;
  rv = dl->GetTargetFile(getter_AddRefs(file));
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool fileExists;
  if (NS_SUCCEEDED(file->Exists(&fileExists)) && fileExists) {
    if (dl->mDownloadState == nsIDownloadManager::DOWNLOAD_FINISHED) {
      dl->mPercentComplete = 100;

      PRInt64 size;
      rv = file->GetFileSize(&size);
      NS_ENSURE_SUCCESS(rv, rv);
      dl->mMaxBytes = dl->mCurrBytes = size;
    } else {
      dl->mPercentComplete = -1;
      dl->mMaxBytes = LL_MAXUINT;
    }
  } else {
    dl->mPercentComplete = 0;
    dl->mMaxBytes = LL_MAXUINT;
    dl->mCurrBytes = 0;
  }

  rv = stmt->GetUTF8String(i++, dl->mEntityID);
  NS_ENSURE_SUCCESS(rv, rv);

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

///////////////////////////////////////////////////////////////////////////////
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

NS_IMETHODIMP
nsDownloadManager::GetDefaultDownloadsDirectory(nsILocalFile **aResult)
{
  nsCOMPtr<nsILocalFile> downloadDir;

  nsresult rv;
  nsCOMPtr<nsIProperties> dirService =
     do_GetService(NS_DIRECTORY_SERVICE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  // OSX:
  // Safari download folder or Desktop/Downloads
  // Vista:
  // Downloads
  // XP/2K:
  // Desktop/Downloads
  // Linux:
  // Home/Downloads

  nsXPIDLString folderName;
  mBundle->GetStringFromName(NS_LITERAL_STRING("downloadsFolder").get(),
                             getter_Copies(folderName));

#if defined (XP_MACOSX)
  nsCOMPtr<nsILocalFile> desktopDir;
  rv = dirService->Get(NS_OSX_DEFAULT_DOWNLOAD_DIR,
                       NS_GET_IID(nsILocalFile),
                       getter_AddRefs(downloadDir));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = dirService->Get(NS_OSX_USER_DESKTOP_DIR,
                       NS_GET_IID(nsILocalFile),
                       getter_AddRefs(desktopDir));
  NS_ENSURE_SUCCESS(rv, rv);

  // Check to see if we have the desktop or the Safari downloads folder
  PRBool equals;
  rv = downloadDir->Equals(desktopDir, &equals);
  NS_ENSURE_SUCCESS(rv, rv);
  if (equals) {
    rv = downloadDir->Append(folderName);
    NS_ENSURE_SUCCESS(rv, rv);
  }
#elif defined (XP_WIN)
  rv = dirService->Get(NS_WIN_DEFAULT_DOWNLOAD_DIR,
                       NS_GET_IID(nsILocalFile),
                       getter_AddRefs(downloadDir));
  NS_ENSURE_SUCCESS(rv, rv);

  // Check the os version
  #define NS_SYSTEMINFO_CONTRACTID "@mozilla.org/system-info;1"
  nsCOMPtr<nsIPropertyBag2> infoService =
     do_GetService(NS_SYSTEMINFO_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt32 version;
  NS_NAMED_LITERAL_STRING(osVersion, "version");
  rv = infoService->GetPropertyAsInt32(osVersion, &version);
  if (version < 6) { // XP/2K
    rv = downloadDir->Append(folderName);
    NS_ENSURE_SUCCESS(rv, rv);
  }
#else
  rv = dirService->Get(NS_OS_HOME_DIR,
                       NS_GET_IID(nsILocalFile),
                       getter_AddRefs(downloadDir));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = downloadDir->Append(folderName);
  NS_ENSURE_SUCCESS(rv, rv);
#endif

  NS_ADDREF(*aResult = downloadDir);

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

  PRBool bRes = PR_FALSE;

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
        NS_ADDREF(*aResult = downloadDir);
        return NS_OK;
      }
      break;
    case 1: // Downloads
      {
        rv = GetDefaultDownloadsDirectory(aResult); // refup
        NS_ENSURE_SUCCESS(rv, rv);
        (*aResult)->Exists(&bRes);
        if (!bRes) {
          rv = (*aResult)->Create(nsIFile::DIRECTORY_TYPE, 755);
          NS_ENSURE_SUCCESS(rv, rv);
        }
        return NS_OK;
      }
      break;
    case 2: // Custom
      {
        nsCOMPtr<nsILocalFile> customDirectory;
        prefBranch->GetComplexValue(NS_PREF_DIR,
                                    NS_GET_IID(nsILocalFile),
                                    getter_AddRefs(customDirectory));
        if (customDirectory) {
          customDirectory->Exists(&bRes);
          if (bRes) {
            NS_ADDREF(*aResult = customDirectory);
            return NS_OK;
          }
          rv = customDirectory->Create(nsIFile::DIRECTORY_TYPE, 755);
          NS_ENSURE_SUCCESS(rv, rv);
          NS_ADDREF(*aResult = customDirectory);
          return NS_OK;
        }
        rv = GetDefaultDownloadsDirectory(aResult); // refup
        NS_ENSURE_SUCCESS(rv, rv);
        (*aResult)->Exists(&bRes);
        if (!bRes) {
          rv = (*aResult)->Create(nsIFile::DIRECTORY_TYPE, 755);
          NS_ENSURE_SUCCESS(rv, rv);
          // Update dir pref
          prefBranch->SetComplexValue(NS_PREF_DIR,
                                      NS_GET_IID(nsILocalFile),
                                      *aResult);
        }
        return NS_OK;
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
  dl->SetStartTime(aStartTime);

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

  PRInt64 id = AddDownloadToDB(dl->mDisplayName, source, target, tempPath,
                               aStartTime, 0,
                               nsIDownloadManager::DOWNLOAD_NOTSTARTED);
  NS_ENSURE_TRUE(id, NS_ERROR_FAILURE);
  dl->mID = id;

  rv = AddToCurrentDownloads(dl);
  (void)dl->SetState(nsIDownloadManager::DOWNLOAD_QUEUED);
  NS_ENSURE_SUCCESS(rv, rv);

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

  // Dump the temp file.  This should really be done when the transfer
  // is cancelled, but there are other cancellation causes that shouldn't
  // remove this. We need to improve those bits.
  if (dl->mTempFile) {
    PRBool exists;
    dl->mTempFile->Exists(&exists);
    if (exists)
      dl->mTempFile->Remove(PR_FALSE);
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
      dl->mDownloadState != nsIDownloadManager::DOWNLOAD_BLOCKED &&
      dl->mDownloadState != nsIDownloadManager::DOWNLOAD_CANCELED)
    return NS_ERROR_FAILURE;

  dl->SetStartTime(PR_Now());

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
    "WHERE id = ?1"), getter_AddRefs(stmt));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = stmt->BindInt64Parameter(0, aID); // unsigned; 64-bit to prevent overflow
  NS_ENSURE_SUCCESS(rv, rv);
  return stmt->Execute();
}

NS_IMETHODIMP
nsDownloadManager::CleanUp()
{
  DownloadState states[] = { nsIDownloadManager::DOWNLOAD_FINISHED,
                             nsIDownloadManager::DOWNLOAD_FAILED,
                             nsIDownloadManager::DOWNLOAD_CANCELED,
                             nsIDownloadManager::DOWNLOAD_BLOCKED };

  nsCOMPtr<mozIStorageStatement> stmt;
  nsresult rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
    "DELETE FROM moz_downloads "
    "WHERE state = ?1 "
      "OR state = ?2 "
      "OR state = ?3 "
      "OR state = ?4"), getter_AddRefs(stmt));
  NS_ENSURE_SUCCESS(rv, rv);
  for (PRUint32 i = 0; i < 4; ++i) {
    rv = stmt->BindInt32Parameter(i, states[i]);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return stmt->Execute();
}

NS_IMETHODIMP
nsDownloadManager::GetCanCleanUp(PRBool *aResult)
{
  *aResult = PR_FALSE;

  DownloadState states[] = { nsIDownloadManager::DOWNLOAD_FINISHED,
                             nsIDownloadManager::DOWNLOAD_FAILED,
                             nsIDownloadManager::DOWNLOAD_CANCELED,
                             nsIDownloadManager::DOWNLOAD_BLOCKED };

  nsCOMPtr<mozIStorageStatement> stmt;
  nsresult rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
    "SELECT COUNT(*) "
    "FROM moz_downloads "
    "WHERE state = ?1 "
      "OR state = ?2 "
      "OR state = ?3 "
      "OR state = ?4"), getter_AddRefs(stmt));
  NS_ENSURE_SUCCESS(rv, rv);
  for (PRUint32 i = 0; i < 4; ++i) {
    rv = stmt->BindInt32Parameter(i, states[i]);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  PRBool moreResults; // We don't really care...
  rv = stmt->ExecuteStep(&moreResults);
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt32 count;
  rv = stmt->GetInt32(0, &count);

  if (count > 0)
    *aResult = PR_TRUE;

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

///////////////////////////////////////////////////////////////////////////////
// nsIObserver

NS_IMETHODIMP
nsDownloadManager::Observe(nsISupports *aSubject,
                           const char *aTopic,
                           const PRUnichar *aData)
{
  // Count active downloads that aren't real-paused
  PRInt32 currDownloadCount = 0;
  for (PRInt32 i = mCurrentDownloads.Count() - 1; i >= 0; --i)
    if (!mCurrentDownloads[i]->IsRealPaused())
      currDownloadCount++;

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
    gStoppingDownloads = PR_TRUE;

    if (currDownloadCount)
      (void)RemoveAllDownloads();

    // Now that active downloads have been canceled, remove all downloads if
    // the user's retention policy specifies it.
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
  } else if (strcmp(aTopic, "alertclickcallback") == 0) {
    nsCOMPtr<nsIDownloadManagerUI> dmui =
      do_GetService("@mozilla.org/download-manager-ui;1", &rv);
    NS_ENSURE_SUCCESS(rv, rv);
    return dmui->Show(nsnull, 0);
  }

  return NS_OK;
}

void
nsDownloadManager::ConfirmCancelDownloads(PRInt32 aCount,
                                          nsISupportsPRBool *aCancelDownloads,
                                          const PRUnichar *aTitle,
                                          const PRUnichar *aCancelMessageMultiple,
                                          const PRUnichar *aCancelMessageSingle,
                                          const PRUnichar *aDontCancelButton)
{
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
  nsCOMPtr<nsIDOMWindowInternal> dmWindow;
  if (wm) {
    wm->GetMostRecentWindow(NS_LITERAL_STRING("Download:Manager").get(),
                            getter_AddRefs(dmWindow));
  }

  // Show alert.
  nsCOMPtr<nsIPromptService> prompter(do_GetService(NS_PROMPTSERVICE_CONTRACTID));
  if (prompter) {
    PRInt32 flags = (nsIPromptService::BUTTON_TITLE_IS_STRING * nsIPromptService::BUTTON_POS_0) + (nsIPromptService::BUTTON_TITLE_IS_STRING * nsIPromptService::BUTTON_POS_1);
    PRBool nothing = PR_FALSE;
    PRInt32 button;
    prompter->ConfirmEx(dmWindow, title, message, flags, quitButton.get(), dontQuitButton.get(), nsnull, nsnull, &nothing, &button);

    aCancelDownloads->SetData(button == 1);
  }
}

///////////////////////////////////////////////////////////////////////////////
// nsDownload

NS_IMPL_ISUPPORTS4(nsDownload, nsIDownload, nsITransfer, nsIWebProgressListener,
                   nsIWebProgressListener2)

nsDownload::nsDownload() : mDownloadState(nsIDownloadManager::DOWNLOAD_NOTSTARTED),
                           mID(0),
                           mPercentComplete(0),
                           mCurrBytes(0),
                           mMaxBytes(LL_MAXUINT),
                           mStartTime(0),
                           mLastUpdate(PR_Now() - (PRUint32)gUpdateInterval),
                           mResumedAt(0),
                           mSpeed(0)
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

  nsresult rv;

  nsCOMPtr<nsIPrefBranch> pref = do_GetService(NS_PREFSERVICE_CONTRACTID);

  // We don't want to lose access to our member variables
  nsRefPtr<nsDownload> kungFuDeathGrip = this;

  // When the state changed listener is dispatched, queries to the database and
  // the download manager api should reflect what the nsIDownload object would
  // return. So, if a download is done (finished, canceled, etc.), it should
  // first be removed from the current downloads.  We will also have to update
  // the database *before* notifying listeners.  At this point, you can safely
  // dispatch to the observers as well.
  switch (aState) {
    case nsIDownloadManager::DOWNLOAD_BLOCKED:
    case nsIDownloadManager::DOWNLOAD_CANCELED:
    case nsIDownloadManager::DOWNLOAD_FAILED:
      mDownloadManager->CompleteDownload(this);
      break;
#ifdef XP_WIN
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
      mDownloadManager->CompleteDownload(this);

      // Master pref to control this function.
      PRBool showTaskbarAlert = PR_TRUE;
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

              PRBool removeWhenDone =
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
#ifdef XP_WIN
      PRBool addToRecentDocs = PR_TRUE;
      if (pref)
        pref->GetBoolPref(PREF_BDM_ADDTORECENTDOCS, &addToRecentDocs);

      if (addToRecentDocs) {
        LPSHELLFOLDER lpShellFolder = NULL;

        if (SUCCEEDED(::SHGetDesktopFolder(&lpShellFolder))) {
          nsresult rv;
          nsCOMPtr<nsIFileURL> fileURL = do_QueryInterface(mTarget, &rv);
          NS_ENSURE_SUCCESS(rv, rv);

          nsCOMPtr<nsIFile> file;
          rv = fileURL->GetFile(getter_AddRefs(file));
          NS_ENSURE_SUCCESS(rv, rv);

          nsAutoString path;
          rv = file->GetPath(path);
          NS_ENSURE_SUCCESS(rv, rv);

          PRUnichar *filePath = ToNewUnicode(path);
          LPITEMIDLIST lpItemIDList = NULL;
          if (SUCCEEDED(lpShellFolder->ParseDisplayName(NULL, NULL, filePath,
                  NULL, &lpItemIDList, NULL))) {
            ::SHAddToRecentDocs(SHARD_PIDL, lpItemIDList);
            ::CoTaskMemFree(lpItemIDList);
          }
          nsMemory::Free(filePath);
          lpShellFolder->Release();
        }
      }
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
  rv = UpdateDB();
  NS_ENSURE_SUCCESS(rv, rv);

  mDownloadManager->NotifyListenersOnDownloadStateChange(oldState, this);

  switch (mDownloadState) {
    case nsIDownloadManager::DOWNLOAD_DOWNLOADING:
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
    case nsIDownloadManager::DOWNLOAD_BLOCKED:
      mDownloadManager->SendEvent(this, "dl-blocked");
      break;
    default:
      break;
  }
  return NS_OK;
}

DownloadType
nsDownload::GetDownloadType()
{
  return mDownloadType;
}

void
nsDownload::SetStartTime(PRInt64 aStartTime)
{
  mStartTime = aStartTime;
  mLastUpdate = aStartTime;
}

///////////////////////////////////////////////////////////////////////////////
// nsIWebProgressListener2

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
    if (channel) {
      // first by trying to get the property
      nsCOMPtr<nsIPropertyBag2> props(do_QueryInterface(channel));
      if (props) {
        // We have to check for a property on a property bag because the
        // referrer may be empty for security reasons (for example, when loading
        // an http page with an https referrer).
        rv = props->GetPropertyAsInterface(NS_LITERAL_STRING("docshell.internalReferrer"),
                                           NS_GET_IID(nsIURI),
                                           getter_AddRefs(mReferrer));
        if (NS_FAILED(rv))
          mReferrer = nsnull;
      }

      // if that didn't work, we can still try to get the referrer from the
      // nsIHttpChannel (if we can QI to it)
      if (!mReferrer) {
        nsCOMPtr<nsIHttpChannel> chan = do_QueryInterface(aRequest);
        if (chan) {
          rv = chan->GetReferrer(getter_AddRefs(mReferrer));
          if (NS_FAILED(rv))
            mReferrer = nsnull;
        }
      }
    }

    // Fetch the entityID
    nsCOMPtr<nsIResumableChannel> resumableChannel(do_QueryInterface(aRequest));
    if (resumableChannel) {
      rv = resumableChannel->GetEntityID(mEntityID);
      NS_ENSURE_SUCCESS(rv, rv);
    }

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

  mCurrBytes = aCurTotalProgress;
  mMaxBytes = aMaxTotalProgress;

  PRUint64 currBytes, maxBytes;
  (void)GetAmountTransferred(&currBytes);
  (void)GetSize(&maxBytes);
  if (aMaxTotalProgress > 0)
    mPercentComplete = (PRInt32)((PRFloat64)currBytes * 100 / maxBytes + .5);
  else
    mPercentComplete = -1;

  mDownloadManager->NotifyListenersOnProgressChange(
    aWebProgress, aRequest, currBytes, maxBytes, currBytes, maxBytes, this);

  return NS_OK;
}

NS_IMETHODIMP
nsDownload::OnRefreshAttempted(nsIWebProgress *aWebProgress,
                               nsIURI *aUri,
                               PRInt32 aDelay,
                               PRBool aSameUri,
                               PRBool *allowRefresh)
{
  *allowRefresh = PR_TRUE;
  return NS_OK;
}

///////////////////////////////////////////////////////////////////////////////
// nsIWebProgressListener

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
                             nsIRequest *aRequest, nsIURI *aLocation)
{
  return NS_OK;
}

NS_IMETHODIMP
nsDownload::OnStatusChange(nsIWebProgress *aWebProgress,
                           nsIRequest *aRequest, nsresult aStatus,
                           const PRUnichar *aMessage)
{
  if (NS_FAILED(aStatus)) {
    // We don't want to lose access to our member variables
    nsRefPtr<nsDownload> kungFuDeathGrip = this;

    (void)SetState(nsIDownloadManager::DOWNLOAD_FAILED);

    // Get title for alert.
    nsXPIDLString title;

    nsCOMPtr<nsIStringBundle> bundle = mDownloadManager->mBundle;
    bundle->GetStringFromName(NS_LITERAL_STRING("downloadErrorAlertTitle").get(),
                              getter_Copies(title));

    // Get Download Manager window, to be parent of alert.
    nsresult rv;
    nsCOMPtr<nsIWindowMediator> wm =
      do_GetService(NS_WINDOWMEDIATOR_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);
    nsCOMPtr<nsIDOMWindowInternal> dmWindow;
    wm->GetMostRecentWindow(NS_LITERAL_STRING("Download:Manager").get(),
                            getter_AddRefs(dmWindow));

    // Show alert.
    nsCOMPtr<nsIPromptService> prompter =
      do_GetService("@mozilla.org/embedcomp/prompt-service;1", &rv);
    NS_ENSURE_SUCCESS(rv, rv);
    prompter->Alert(dmWindow, title, aMessage);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDownload::OnStateChange(nsIWebProgress *aWebProgress,
                          nsIRequest *aRequest, PRUint32 aStateFlags,
                          nsresult aStatus)
{
  // Record the start time only if it hasn't been set.
  if (mStartTime == 0 && (aStateFlags & STATE_START))
    SetStartTime(PR_Now());

  // We don't want to lose access to our member variables
  nsRefPtr<nsDownload> kungFuDeathGrip = this;

  // We need to update mDownloadState before updating the dialog, because
  // that will close and call CancelDownload if it was the last open window.

  if (aStateFlags & STATE_START) {
    nsresult rv;
    nsCOMPtr<nsIHttpChannel> channel = do_QueryInterface(aRequest, &rv);
    if (NS_SUCCEEDED(rv)) {
      PRUint32 status;
      rv = channel->GetResponseStatus(&status);
      // HTTP 450 - Blocked by parental control proxies
      if (NS_SUCCEEDED(rv) && status == 450) {
        // Cancel using the provided object
        (void)Cancel();

        // Fail the download - DOWNLOAD_BLOCKED
        (void)SetState(nsIDownloadManager::DOWNLOAD_BLOCKED);
      }
    }
  } else if (aStateFlags & STATE_STOP) {
    if (IsFinishable()) {
      // Set file size at the end of a transfer (for unknown transfer amounts)
      if (mMaxBytes == LL_MAXUINT)
        mMaxBytes = mCurrBytes;

      mPercentComplete = 100;

#ifdef XP_WIN
      (void)SetState(nsIDownloadManager::DOWNLOAD_SCANNING);
#else
      (void)SetState(nsIDownloadManager::DOWNLOAD_FINISHED);
#endif
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

///////////////////////////////////////////////////////////////////////////////
// nsIDownload

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
nsDownload::GetAmountTransferred(PRUint64 *aAmountTransferred)
{
  *aAmountTransferred = mCurrBytes + mResumedAt;
  return NS_OK;
}

NS_IMETHODIMP
nsDownload::GetSize(PRUint64 *aSize)
{
  *aSize = mMaxBytes + (mMaxBytes != LL_MAXUINT ? mResumedAt : 0);
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

nsresult
nsDownload::Pause()
{
  nsresult rv = NS_ERROR_FAILURE;
  if (IsResumable())
    rv = Cancel();
  else if (mRequest)
    rv = mRequest->Suspend();
  else
    NS_NOTREACHED("We don't have a resumable download or a request to suspend??");
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
  nsresult rv = NS_ERROR_FAILURE;
  if (IsResumable())
    rv = RealResume();
  else if (mRequest)
    rv = mRequest->Resume();
  else
    NS_NOTREACHED("We don't have a resumable download or a request to resume??");
  NS_ENSURE_SUCCESS(rv, rv);

  return SetState(nsIDownloadManager::DOWNLOAD_DOWNLOADING);
}

nsresult
nsDownload::RealResume()
{
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

  // Get the size of the temporary or target file to be used as offset.
  PRInt64 fileSize;
  nsCOMPtr<nsILocalFile> targetLocalFile(mTempFile);
  if (!targetLocalFile) {
    rv = GetTargetFile(getter_AddRefs(targetLocalFile));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // We need to get a new nsIFile though because of caching issues with the
  // file size.  Cloning it takes care of this :(
  nsCOMPtr<nsIFile> clone;
  rv = targetLocalFile->Clone(getter_AddRefs(clone));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = clone->GetFileSize(&fileSize);
  NS_ENSURE_SUCCESS(rv, rv);

  // Set the channel to resume at the right position along with the entityID
  nsCOMPtr<nsIResumableChannel> resumableChannel(do_QueryInterface(channel));
  if (!resumableChannel)
    return NS_ERROR_UNEXPECTED;
  rv = resumableChannel->ResumeAt(fileSize, mEntityID);
  NS_ENSURE_SUCCESS(rv, rv);

  // Track where we resumed because progress notifications restart at 0
  mResumedAt = fileSize;
  mCurrBytes = 0;
  mMaxBytes = LL_MAXUINT;

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

  return NS_OK;
}

PRBool
nsDownload::IsPaused()
{
  return mDownloadState == nsIDownloadManager::DOWNLOAD_PAUSED;
}

PRBool
nsDownload::IsResumable()
{
  nsHandlerInfoAction action = nsIMIMEInfo::saveToDisk;
  if (mMIMEInfo)
    (void)mMIMEInfo->GetPreferredAction(&action);

  // For now we can only resume saveToDisk type actions (not open with)
  return action == nsIMIMEInfo::saveToDisk && !mEntityID.IsEmpty();
}

PRBool
nsDownload::WasResumed()
{
  return mResumedAt > 0;
}

PRBool
nsDownload::IsRealPaused()
{
  return IsPaused() && IsResumable();
}

PRBool
nsDownload::IsFinishable()
{
  return mDownloadState == nsIDownloadManager::DOWNLOAD_NOTSTARTED ||
         mDownloadState == nsIDownloadManager::DOWNLOAD_QUEUED ||
         mDownloadState == nsIDownloadManager::DOWNLOAD_DOWNLOADING;
}

PRBool
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

  PRInt32 i = 0;
  // startTime
  nsresult rv = stmt->BindInt64Parameter(i++, mStartTime);
  NS_ENSURE_SUCCESS(rv, rv);

  // endTime
  rv = stmt->BindInt64Parameter(i++, mLastUpdate);
  NS_ENSURE_SUCCESS(rv, rv);

  // state
  rv = stmt->BindInt32Parameter(i++, mDownloadState);
  NS_ENSURE_SUCCESS(rv, rv);

  // referrer
  if (mReferrer) {
    nsCAutoString referrer;
    rv = mReferrer->GetSpec(referrer);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = stmt->BindUTF8StringParameter(i++, referrer);
  } else {
    rv = stmt->BindNullParameter(i++);
  }
  NS_ENSURE_SUCCESS(rv, rv);

  // entityID
  rv = stmt->BindUTF8StringParameter(i++, mEntityID);
  NS_ENSURE_SUCCESS(rv, rv);

  // id
  rv = stmt->BindInt64Parameter(i++, mID);
  NS_ENSURE_SUCCESS(rv, rv);

  return stmt->Execute();
}
