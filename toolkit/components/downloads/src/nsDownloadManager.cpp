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
#include "nsIWindowWatcher.h"
#include "nsIWindowMediator.h"
#include "nsIPromptService.h"
#include "nsIPrefBranch.h"
#include "nsIPrefService.h"
#include "nsVoidArray.h"
#include "nsEnumeratorUtils.h"
#include "nsIFileURL.h"
#include "nsEmbedCID.h"
#include "nsInt64.h"
#include "mozStorageCID.h"
#include "mozIStorageService.h"
#include "mozIStorageStatement.h"
#include "nsIMutableArray.h"
#include "nsIAlertsService.h"

#ifdef XP_WIN
#include <shlobj.h>
#endif

static PRBool gStoppingDownloads = PR_FALSE;

#define DOWNLOAD_MANAGER_FE_URL "chrome://mozapps/content/downloads/downloads.xul"
#define DOWNLOAD_MANAGER_BUNDLE "chrome://mozapps/locale/downloads/downloads.properties"
#define DOWNLOAD_MANAGER_ALERT_ICON "chrome://mozapps/skin/downloads/downloadIcon.png"
#define PREF_BDM_SHOWALERTONCOMPLETE "browser.download.manager.showAlertOnComplete"
#define PREF_BDM_SHOWALERTINTERVAL "browser.download.manager.showAlertInterval"
#define PREF_BDM_RETENTION "browser.download.manager.retention"
#define PREF_BDM_OPENDELAY "browser.download.manager.openDelay"
#define PREF_BDM_SHOWWHENSTARTING "browser.download.manager.showWhenStarting"
#define PREF_BDM_FOCUSWHENSTARTING "browser.download.manager.focusWhenStarting"
#define PREF_BDM_CLOSEWHENDONE "browser.download.manager.closeWhenDone"
#define PREF_BDM_FLASHCOUNT "browser.download.manager.flashCount"
#define PREF_BDM_ADDTORECENTDOCS "browser.download.manager.addToRecentDocs"

static const PRInt64 gUpdateInterval = 400 * PR_USEC_PER_MSEC;

static PRInt32 gRefCnt = 0;

///////////////////////////////////////////////////////////////////////////////
// nsDownloadManager

NS_IMPL_ISUPPORTS3(nsDownloadManager, nsIDownloadManager, nsIXPInstallManagerUI, nsIObserver)

nsDownloadManager::~nsDownloadManager()
{
  if (--gRefCnt != 0)
    // Either somebody tried to use |CreateInstance| instead of
    // |GetService| or |Init| failed very early, so there's nothing to
    // do here.
    return;

#if 0
  // Temporary fix for orange regression from bug 328159 until I
  // understand new protocol following bug 326491.  See bug 315421.
  mObserverService->RemoveObserver(this, "quit-application");
  mObserverService->RemoveObserver(this, "quit-application-requested");
  mObserverService->RemoveObserver(this, "offline-requested");
#endif
}

nsresult
nsDownloadManager::CancelAllDownloads()
{
  nsresult rv = NS_OK;
  for (PRInt32 i = mCurrentDownloads.Count() - 1; i >= 0; --i) {
    nsRefPtr<nsDownload> dl = mCurrentDownloads[0];

    nsresult result = CancelDownload(dl->mID);
    // We want to try the rest of them because they should be canceled if they
    // can be canceled.
    if (NS_FAILED(result)) rv = result;
  }
  
  return rv;
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
  rv = dbFile->Append(NS_LITERAL_STRING("downloads.sqlite"));
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
  }

  return NS_OK;
}

nsresult
nsDownloadManager::CreateTable()
{
  return mDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "CREATE TABLE moz_downloads ("
    "id INTEGER PRIMARY KEY, name TEXT, source TEXT, target TEXT,"
    "iconURL TEXT, startTime INTEGER, endTime INTEGER, state INTEGER)"));
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
    if (NS_FAILED(rv)) continue;
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
 
    AddDownloadToDB(name, source, target, EmptyString(), startTime,
                    endTime, state);
  }

  return NS_OK;
}

PRInt64
nsDownloadManager::AddDownloadToDB(const nsAString &aName,
                                   const nsACString &aSource,
                                   const nsACString &aTarget,
                                   const nsAString &aIconURL,
                                   PRInt64 aStartTime,
                                   PRInt64 aEndTime,
                                   PRInt32 aState)
{
  nsCOMPtr<mozIStorageStatement> stmt;
  nsresult rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
    "INSERT INTO moz_downloads "
    "(name, source, target, iconURL, startTime, endTime, state) "
    "VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7)"), getter_AddRefs(stmt));
  NS_ENSURE_SUCCESS(rv, 0);

  // name
  rv = stmt->BindStringParameter(0, aName);
  NS_ENSURE_SUCCESS(rv, 0);

  // source
  rv = stmt->BindUTF8StringParameter(1, aSource);
  NS_ENSURE_SUCCESS(rv, 0);

  // target
  rv = stmt->BindUTF8StringParameter(2, aTarget);
  NS_ENSURE_SUCCESS(rv, 0);

  // iconURL
  rv = stmt->BindStringParameter(3, aIconURL);
  NS_ENSURE_SUCCESS(rv, 0);

  // startTime
  rv = stmt->BindInt64Parameter(4, aStartTime);
  NS_ENSURE_SUCCESS(rv, 0);

  // endTime
  rv = stmt->BindInt64Parameter(5, aEndTime);
  NS_ENSURE_SUCCESS(rv, 0);

  // state
  rv = stmt->BindInt32Parameter(6, aState);
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
  if (gRefCnt++ != 0) {
    NS_NOTREACHED("download manager should be used as a service");
    return NS_ERROR_UNEXPECTED; // This will make the |CreateInstance| fail.
  }

  nsresult rv;
  mObserverService = do_GetService("@mozilla.org/observer-service;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool doImport;
  rv = InitDB(&doImport);
  NS_ENSURE_SUCCESS(rv, rv);

  if (doImport)
    ImportDownloadHistory();

  nsCOMPtr<nsIStringBundleService> bundleService =
    do_GetService(NS_STRINGBUNDLE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  
  rv = bundleService->CreateBundle(DOWNLOAD_MANAGER_BUNDLE,
                                   getter_AddRefs(mBundle));
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

///////////////////////////////////////////////////////////////////////////////
// nsIDownloadManager

NS_IMETHODIMP
nsDownloadManager::AddDownload(DownloadType aDownloadType, 
                               nsIURI* aSource,
                               nsIURI* aTarget,
                               const nsAString& aDisplayName,
                               const nsAString& aIconURL, 
                               nsIMIMEInfo *aMIMEInfo,
                               PRTime aStartTime,
                               nsILocalFile* aTempFile,
                               nsICancelable* aCancelable,
                               nsIDownload** aDownload)
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
  dl->mDownloadManager = this;
  dl->mTarget = aTarget;
  dl->mSource = aSource;
  dl->mTempFile = aTempFile;

  dl->mDisplayName = aDisplayName;
  if (dl->mDisplayName.IsEmpty())
    targetFile->GetLeafName(dl->mDisplayName);
 
  dl->mMIMEInfo = aMIMEInfo;
  dl->SetStartTime(aStartTime);

  // this will create a cycle that will be broken in nsDownload::OnStateChange
  dl->mCancelable = aCancelable;

  // Adding to the DB
  nsCAutoString source, target;
  aSource->GetSpec(source);
  aTarget->GetSpec(target);
  
  PRInt64 id = AddDownloadToDB(dl->mDisplayName, source, target, aIconURL,
                               aStartTime, 0,
                               nsIDownloadManager::DOWNLOAD_NOTSTARTED);
  NS_ENSURE_TRUE(id, NS_ERROR_FAILURE);
  dl->mID = id;

  // If this is an install operation, ensure we have a progress listener for the
  // install and track this download separately. 
  if (aDownloadType == nsIXPInstallManagerUI::DOWNLOAD_TYPE_INSTALL) {
    if (!mXPIProgress) {
      mXPIProgress = new nsXPIProgressListener(this);
      if (!mXPIProgress)
        return NS_ERROR_OUT_OF_MEMORY;
    }

    nsIXPIProgressDialog* dialog = mXPIProgress.get();
    nsXPIProgressListener* listener = NS_STATIC_CAST(nsXPIProgressListener*, dialog);
    listener->AddDownload(*aDownload);
  }

  mCurrentDownloads.AppendObject(dl);

  NS_ADDREF(*aDownload = dl);
  
  return NS_OK;
}

NS_IMETHODIMP
nsDownloadManager::GetDownload(PRUint32 aID, nsIDownload **aDownloadItem)
{
  nsDownload *itm = FindDownload(aID);

  if (!itm) {
    *aDownloadItem = nsnull;

    return NS_ERROR_FAILURE;
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
  if (CompletedSuccessfully(dl->mDownloadState))
    return NS_OK;

  // Cancel using the provided object
  if (dl->mCancelable)
    dl->mCancelable->Cancel(NS_BINDING_ABORTED);

  // Dump the temp file.  This should really be done when the transfer
  // is cancelled, but there are other cancellation causes that shouldn't
  // remove this. We need to improve those bits.
  if (dl->mTempFile) {
    PRBool exists;
    dl->mTempFile->Exists(&exists);
    if (exists)
      dl->mTempFile->Remove(PR_FALSE);
  }

  // This have to be done in this exact order to not mess up our invariants
  // 1) when the state changed listener is dispatched, it must no longer be
  //    an active download.
  // 2) when the dl-cancel observer is dispatched, the same conditions for 1
  //    must be true as well as the state being up to date.
  RemoveDownloadFromCurrent(dl);
  dl->SetState(nsIDownloadManager::DOWNLOAD_CANCELED);
  mObserverService->NotifyObservers(dl, "dl-cancel", nsnull);

  // if there's a progress dialog open for the item,
  // we have to notify it that we're cancelling
  if (dl->mDialog) {
    nsCOMPtr<nsIObserver> observer = do_QueryInterface(dl->mDialog);
    observer->Observe(dl, "oncancel", nsnull);
  }

  return dl->UpdateDB();
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
                             nsIXPInstallManagerUI::INSTALL_FINISHED };

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
                             nsIXPInstallManagerUI::INSTALL_FINISHED };

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
nsDownloadManager::SetListener(nsIDownloadProgressListener *aListener)
{
  mListener = aListener;

  return NS_OK;
}

NS_IMETHODIMP
nsDownloadManager::GetListener(nsIDownloadProgressListener** aListener)
{
  NS_IF_ADDREF(*aListener = mListener);

  return NS_OK;
}

NS_IMETHODIMP
nsDownloadManager::PauseDownload(PRUint32 aID)
{
  return PauseResumeDownload(aID, PR_TRUE);
}

NS_IMETHODIMP
nsDownloadManager::ResumeDownload(PRUint32 aID)
{
  return PauseResumeDownload(aID, PR_FALSE);
}

nsresult
nsDownloadManager::PauseResumeDownload(PRUint32 aID, PRBool aPause)
{
  nsDownload *dl = FindDownload(aID);
  
  if (!dl)
    return NS_ERROR_FAILURE;

  return dl->PauseResume(aPause);
}

NS_IMETHODIMP
nsDownloadManager::Open(nsIDOMWindow* aParent, PRUint32 aID)
{
  nsDownload *dl = FindDownload(aID);
  
  if (!dl)
    return NS_ERROR_FAILURE;

  TimerParams* params = new TimerParams();
  if (!params)
    return NS_ERROR_OUT_OF_MEMORY;

  params->parent = aParent;
  params->download = dl;

  PRInt32 delay = 0;
  nsCOMPtr<nsIPrefBranch> pref(do_GetService(NS_PREFSERVICE_CONTRACTID));
  if (pref)
    pref->GetIntPref(PREF_BDM_OPENDELAY, &delay);

  // Look for an existing Download Manager window, if we find one we just 
  // tell it that a new download has begun (we don't focus, that's 
  // annoying), otherwise we need to open the window. We do this on a timer 
  // so that we can see if the download has already completed, if so, don't 
  // bother opening the window. 
  mDMOpenTimer = do_CreateInstance("@mozilla.org/timer;1");
  return mDMOpenTimer->InitWithFuncCallback(OpenTimerCallback, 
                                       (void*)params, delay, 
                                       nsITimer::TYPE_ONE_SHOT);
}

void
nsDownloadManager::OpenTimerCallback(nsITimer* aTimer, void* aClosure)
{
  TimerParams* params = NS_STATIC_CAST(TimerParams*, aClosure);
  
  PRInt32 complete;
  params->download->GetPercentComplete(&complete);
  
  PRBool closeDM = PR_FALSE;
  nsCOMPtr<nsIPrefBranch> pref(do_GetService(NS_PREFSERVICE_CONTRACTID));
  if (pref)
    pref->GetBoolPref(PREF_BDM_CLOSEWHENDONE, &closeDM);

  // Check closeWhenDone pref before opening download manager
  if (!closeDM || complete < 100) {
    PRBool focusDM = PR_FALSE;
    PRBool showDM = PR_TRUE;
    PRInt32 flashCount = -1;

    if (pref) {
      pref->GetBoolPref(PREF_BDM_FOCUSWHENSTARTING, &focusDM);

      // We only flash the download manager if the user has the download manager show
      pref->GetBoolPref(PREF_BDM_SHOWWHENSTARTING, &showDM);
      if (showDM) 
        pref->GetIntPref(PREF_BDM_FLASHCOUNT, &flashCount);
      else
        flashCount = 0;
    }

    nsDownloadManager::OpenDownloadManager(focusDM, flashCount,
                                           params->download, params->parent);
  }

  delete params;
}

nsresult
nsDownloadManager::OpenDownloadManager(PRBool aShouldFocus,
                                       PRInt32 aFlashCount,
                                       nsIDownload *aDownload,
                                       nsIDOMWindow *aParent)
{
  nsresult rv;
  nsCOMPtr<nsIWindowMediator> wm =
    do_GetService(NS_WINDOWMEDIATOR_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIDOMWindowInternal> recentWindow;
  wm->GetMostRecentWindow(NS_LITERAL_STRING("Download:Manager").get(),
                          getter_AddRefs(recentWindow));
  if (recentWindow) {
    if (aShouldFocus) {
      recentWindow->Focus();
    } else {
      nsCOMPtr<nsIDOMChromeWindow> chromeWindow(do_QueryInterface(recentWindow));
      chromeWindow->GetAttentionWithCycleCount(aFlashCount);
    }
  } else {
    // If we ever have the capability to display the UI of third party dl
    // managers, we'll open their UI here instead.
    nsCOMPtr<nsIWindowWatcher> ww =
      do_GetService(NS_WINDOWWATCHER_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    // pass the datasource to the window
    nsCOMPtr<nsIMutableArray> params =
      do_CreateInstance(NS_ARRAY_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIDownloadManager> dlMgr =
      do_GetService("@mozilla.org/download-manager;1", &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<mozIStorageConnection> DBConn;
    rv = dlMgr->GetDBConnection(getter_AddRefs(DBConn));
    NS_ENSURE_SUCCESS(rv, rv);
    
    params->AppendElement(DBConn, PR_FALSE);
    params->AppendElement(aDownload, PR_FALSE);
    
    nsCOMPtr<nsIDOMWindow> newWindow;
    rv = ww->OpenWindow(aParent,
                        DOWNLOAD_MANAGER_FE_URL,
                        "_blank",
                        "chrome,dialog=no,resizable",
                        params,
                        getter_AddRefs(newWindow));
  }
  return rv;
}

NS_IMETHODIMP
nsDownloadManager::GetDBConnection(mozIStorageConnection **aDBConn)
{
  NS_ADDREF(*aDBConn = mDBConn);

  return NS_OK;
}

///////////////////////////////////////////////////////////////////////////////
// nsIObserver

NS_IMETHODIMP
nsDownloadManager::Observe(nsISupports *aSubject,
                           const char *aTopic,
                           const PRUnichar* aData)
{
  PRInt32 currDownloadCount = mCurrentDownloads.Count();

  nsresult rv;
  if (strcmp(aTopic, "oncancel") == 0) {
    nsCOMPtr<nsIDownload> dl = do_QueryInterface(aSubject, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    PRUint32 id;
    dl->GetId(&id);
    nsDownload *dl2 = FindDownload(id);
    if (dl2) {
      // unset dialog since it's closing
      dl2->mDialog = nsnull;
      
      return CancelDownload(id);  
    }
  } else if (strcmp(aTopic, "quit-application") == 0) {
    gStoppingDownloads = PR_TRUE;
    
    if (currDownloadCount) {
      CancelAllDownloads();

      // Download Manager is shutting down! Tell the XPInstallManager to stop
      // transferring any files that may have been being downloaded. 
      mObserverService->NotifyObservers(mXPIProgress, "xpinstall-progress",
                                        NS_LITERAL_STRING("cancel").get());
    }

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
    PRBool data;
    cancelDownloads->GetData(&data);
    if (!data) {
      gStoppingDownloads = PR_TRUE;

      // Network is going down! Tell the XPInstallManager to stop
      // transferring any files that may have been being downloaded. 
      mObserverService->NotifyObservers(mXPIProgress, "xpinstall-progress",
                                        NS_LITERAL_STRING("cancel").get());
    
      CancelAllDownloads();
      gStoppingDownloads = PR_FALSE;
    }
  } else if (strcmp(aTopic, "alertclickcallback") == 0) {
    // Attempt to locate a browser window to parent the download manager to
    nsCOMPtr<nsIWindowMediator> wm = do_GetService(NS_WINDOWMEDIATOR_CONTRACTID);
    nsCOMPtr<nsIDOMWindowInternal> browserWindow;
    if (wm) {
      wm->GetMostRecentWindow(NS_LITERAL_STRING("navigator:browser").get(),
                              getter_AddRefs(browserWindow));
    }

    return OpenDownloadManager(PR_TRUE, -1, nsnull, browserWindow);
  }

  return NS_OK;
}

void
nsDownloadManager::ConfirmCancelDownloads(PRInt32 aCount,
                                          nsISupportsPRBool* aCancelDownloads,
                                          const PRUnichar* aTitle, 
                                          const PRUnichar* aCancelMessageMultiple, 
                                          const PRUnichar* aCancelMessageSingle,
                                          const PRUnichar* aDontCancelButton)
{
  nsXPIDLString title, message, quitButton, dontQuitButton;
  
  mBundle->GetStringFromName(aTitle, getter_Copies(title));    

  nsAutoString countString;
  countString.AppendInt(aCount);
  const PRUnichar* strings[1] = { countString.get() };
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
// nsIXPInstallManagerUI
NS_IMETHODIMP
nsDownloadManager::GetXpiProgress(nsIXPIProgressDialog** aProgress)
{
  *aProgress = mXPIProgress;
  NS_IF_ADDREF(*aProgress);
  return NS_OK;
}

NS_IMETHODIMP
nsDownloadManager::GetHasActiveXPIOperations(PRBool* aHasOps)
{
  nsIXPIProgressDialog* dialog = mXPIProgress.get();
  nsXPIProgressListener* listener = NS_STATIC_CAST(nsXPIProgressListener*, dialog);
  *aHasOps = !mXPIProgress ? PR_FALSE : listener->HasActiveXPIOperations();
  return NS_OK;
}

///////////////////////////////////////////////////////////////////////////////
// nsXPIProgressListener

NS_IMPL_ISUPPORTS1(nsXPIProgressListener, nsIXPIProgressDialog)

nsXPIProgressListener::nsXPIProgressListener(nsDownloadManager* aDownloadManager)
{
  NS_NewISupportsArray(getter_AddRefs(mDownloads));

  mDownloadManager = aDownloadManager;
}

nsXPIProgressListener::~nsXPIProgressListener()
{
  // Release any remaining references to objects held by the downloads array
  mDownloads->Clear();
}

void
nsXPIProgressListener::AddDownload(nsIDownload* aDownload)
{
  PRUint32 cnt;
  mDownloads->Count(&cnt);
  PRBool foundMatch = PR_FALSE;

  nsCOMPtr<nsIURI> uri1, uri2;
  for (PRUint32 i = 0; i < cnt; ++i) {
    nsCOMPtr<nsIDownload> download(do_QueryElementAt(mDownloads, i));
    download->GetSource(getter_AddRefs(uri1));
    aDownload->GetSource(getter_AddRefs(uri2));

    uri1->Equals(uri2, &foundMatch);
    if (foundMatch)
      break;
  }
  if (!foundMatch)
    mDownloads->AppendElement(aDownload);
}

void 
nsXPIProgressListener::RemoveDownloadAtIndex(PRUint32 aIndex)
{
  mDownloads->RemoveElementAt(aIndex);
}

PRBool
nsXPIProgressListener::HasActiveXPIOperations()
{
  PRUint32 count;
  mDownloads->Count(&count);
  return count != 0;
}

///////////////////////////////////////////////////////////////////////////////
// nsIXPIProgressDialog
NS_IMETHODIMP
nsXPIProgressListener::OnStateChange(PRUint32 aIndex, PRInt16 aState, PRInt32 aValue)
{
  nsCOMPtr<nsIWebProgressListener> wpl(do_QueryElementAt(mDownloads, aIndex));
  nsIWebProgressListener* temp = wpl.get();
  nsDownload* dl = NS_STATIC_CAST(nsDownload*, temp);
  // Sometimes we get XPInstall progress notifications after everything is done, and there's
  // no more active downloads... this null check is to prevent a crash in this case.
  if (!dl) 
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIObserverService> os;
  
  switch (aState) {
  case nsIXPIProgressDialog::DOWNLOAD_START:
    wpl->OnStateChange(nsnull, nsnull, nsIWebProgressListener::STATE_START, 0);

    dl->SetState(nsIXPInstallManagerUI::INSTALL_DOWNLOADING);
    os = do_GetService("@mozilla.org/observer-service;1");
    if (os)
      os->NotifyObservers(dl, "dl-start", nsnull);
    break;
  case nsIXPIProgressDialog::DOWNLOAD_DONE:
    break;
  case nsIXPIProgressDialog::INSTALL_START:
    dl->SetState(nsIXPInstallManagerUI::INSTALL_INSTALLING);
    break;
  case nsIXPIProgressDialog::INSTALL_DONE:
    wpl->OnStateChange(nsnull, nsnull, nsIWebProgressListener::STATE_STOP, 0);
    
    dl->SetState(nsIXPInstallManagerUI::INSTALL_FINISHED);
    
    // Now, remove it from our internal bookkeeping list. 
    RemoveDownloadAtIndex(aIndex);
    break;
  case nsIXPIProgressDialog::DIALOG_CLOSE:
    // Close now, if we're allowed to. 
    os = do_GetService("@mozilla.org/observer-service;1");
    if (os)
      os->NotifyObservers(nsnull, "xpinstall-dialog-close", nsnull);

    if (!gStoppingDownloads) {
      nsCOMPtr<nsIStringBundleService> sbs(do_GetService("@mozilla.org/intl/stringbundle;1"));
      nsCOMPtr<nsIStringBundle> brandBundle, xpinstallBundle;
      sbs->CreateBundle("chrome://branding/locale/brand.properties", getter_AddRefs(brandBundle));
      sbs->CreateBundle("chrome://mozapps/locale/xpinstall/xpinstallConfirm.properties", getter_AddRefs(xpinstallBundle));

      nsXPIDLString brandShortName, message, title;
      brandBundle->GetStringFromName(NS_LITERAL_STRING("brandShortName").get(), getter_Copies(brandShortName));
      const PRUnichar* strings[1] = { brandShortName.get() };
      xpinstallBundle->FormatStringFromName(NS_LITERAL_STRING("installComplete").get(), strings, 1, getter_Copies(message));
      xpinstallBundle->GetStringFromName(NS_LITERAL_STRING("installCompleteTitle").get(), getter_Copies(title));

      nsCOMPtr<nsIPromptService> ps(do_GetService(NS_PROMPTSERVICE_CONTRACTID));
      ps->Alert(nsnull, title, message);
    }

    break;
  }

  return dl->UpdateDB();
}

NS_IMETHODIMP
nsXPIProgressListener::OnProgress(PRUint32 aIndex, PRUint64 aValue, PRUint64 aMaxValue)
{
  nsCOMPtr<nsIWebProgressListener2> wpl(do_QueryElementAt(mDownloads, aIndex));
  if (wpl) 
    return wpl->OnProgressChange64(nsnull, nsnull, 0, 0, aValue, aMaxValue);
  return NS_OK;
}

///////////////////////////////////////////////////////////////////////////////
// nsDownload

NS_IMPL_ISUPPORTS4(nsDownload, nsIDownload, nsITransfer, nsIWebProgressListener,
                   nsIWebProgressListener2)

nsDownload::nsDownload() : mDownloadState(nsIDownloadManager::DOWNLOAD_NOTSTARTED),
                           mID(0),
                           mPercentComplete(0),
                           mCurrBytes(LL_ZERO),
                           mMaxBytes(LL_ZERO),
                           mStartTime(LL_ZERO),
                           mLastUpdate(PR_Now() - (PRUint32)gUpdateInterval),
                           mPaused(PR_FALSE),
                           mSpeed(0)
{
}

nsDownload::~nsDownload()
{  
}

void
nsDownload::SetState(DownloadState aState)
{
  if (mDownloadState == aState)
    return;

  PRInt16 oldState = mDownloadState;
  mDownloadState = aState;

  if (mDownloadManager->mListener)
    mDownloadManager->mListener->OnDownloadStateChange(oldState, this);
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

  // filter notifications since they come in so frequently
  PRTime now = PR_Now();
  PRIntervalTime delta = now - mLastUpdate;
  if (delta < gUpdateInterval)
    return NS_OK;

  mLastUpdate = now;

  if (mDownloadState == nsIDownloadManager::DOWNLOAD_NOTSTARTED) {
    SetState(nsIDownloadManager::DOWNLOAD_DOWNLOADING);
    mDownloadManager->mObserverService->NotifyObservers(this, "dl-start", nsnull);

    nsresult rv = UpdateDB();
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // Calculate the speed using the elapsed delta time and bytes downloaded
  // during that time for more accuracy.
  double elapsedSecs = double(delta) / PR_USEC_PER_SEC;
  if (elapsedSecs > 0) {
    nsUint64 curTotalProgress = (PRUint64)aCurTotalProgress;
    nsUint64 diffBytes = curTotalProgress - nsUint64(mCurrBytes);
    double speed = double(diffBytes) / elapsedSecs;
    if (mCurrBytes == 0) {
      mSpeed = speed;
    } else {
      // Calculate 'smoothed average' of 10 readings.
      mSpeed = mSpeed * 0.9 + speed * 0.1;
    }
  }

  if (aMaxTotalProgress > 0)
    mPercentComplete = (PRInt32)((PRFloat64)aCurTotalProgress * 100 / aMaxTotalProgress + .5);
  else
    mPercentComplete = -1;

  mCurrBytes = aCurTotalProgress;
  mMaxBytes = aMaxTotalProgress;

  if (mDownloadManager->NeedsUIUpdate()) {
    nsIDownloadProgressListener* dpl = mDownloadManager->mListener;
    if (dpl) {
      dpl->OnProgressChange(aWebProgress, aRequest, aCurSelfProgress,
                            aMaxSelfProgress, aCurTotalProgress,
                            aMaxTotalProgress, this);
    }
  }

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
    SetState(nsIDownloadManager::DOWNLOAD_FAILED);
    mDownloadManager->mObserverService->NotifyObservers(this, "dl-failed", nsnull);

    UpdateDB();

    mDownloadManager->RemoveDownloadFromCurrent(this);

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
nsDownload::OnStateChange(nsIWebProgress* aWebProgress,
                          nsIRequest* aRequest, PRUint32 aStateFlags,
                          nsresult aStatus)
{
  // Record the start time only if it hasn't been set.
  if (LL_IS_ZERO(mStartTime) && (aStateFlags & STATE_START))
    SetStartTime(PR_Now());

  // We don't want to lose access to our member variables
  nsRefPtr<nsDownload> kungFuDeathGrip = this;
  
  // We need to update mDownloadState before updating the dialog, because
  // that will close and call CancelDownload if it was the last open window.
  nsCOMPtr<nsIPrefBranch> pref = do_GetService(NS_PREFSERVICE_CONTRACTID);

  if (aStateFlags & STATE_STOP) {
    if (nsDownloadManager::IsInFinalStage(mDownloadState)) {
      // Set file size at the end of a tranfer (for unknown transfer amounts)
      if (mMaxBytes == LL_MAXUINT)
        mMaxBytes = mCurrBytes;

      // Files less than 1Kb shouldn't show up as 0Kb.
      if (mMaxBytes < 1024) {
        mCurrBytes = 1024;
        mMaxBytes  = 1024;
      }

      mPercentComplete = 100;

      // Master pref to control this function. 
      PRBool showTaskbarAlert = PR_FALSE;
      if (pref)
        pref->GetBoolPref(PREF_BDM_SHOWALERTONCOMPLETE, &showTaskbarAlert);

      if (showTaskbarAlert) {
        PRInt32 alertInterval = -1;
        pref->GetIntPref(PREF_BDM_SHOWALERTINTERVAL, &alertInterval);

        PRInt64 temp, uSecPerMSec, alertIntervalUSec;
        LL_I2L(temp, alertInterval);
        LL_I2L(uSecPerMSec, PR_USEC_PER_MSEC);
        LL_MUL(alertIntervalUSec, temp, uSecPerMSec);
        
        PRInt64 goat = PR_Now() - mStartTime;
        showTaskbarAlert = goat > alertIntervalUSec;
       
        PRInt32 size = mDownloadManager->mCurrentDownloads.Count();
        if (showTaskbarAlert && size == 0) {
          nsCOMPtr<nsIAlertsService> alerts =
            do_GetService("@mozilla.org/alerts-service;1");
        if (alerts) {
            nsXPIDLString title, message;

            mDownloadManager->mBundle->GetStringFromName(NS_LITERAL_STRING("downloadsCompleteTitle").get(), getter_Copies(title));
            mDownloadManager->mBundle->GetStringFromName(NS_LITERAL_STRING("downloadsCompleteMsg").get(), getter_Copies(message));

            PRBool removeWhenDone = mDownloadManager->GetRetentionBehavior() == 0;


            // If downloads are automatically removed per the user's retention policy, 
            // there's no reason to make the text clickable because if it is, they'll
            // click open the download manager and the items they downloaded will have
            // been removed. 
            alerts->ShowAlertNotification(NS_LITERAL_STRING(DOWNLOAD_MANAGER_ALERT_ICON), title, message, !removeWhenDone, 
                                          EmptyString(), mDownloadManager);
          }
        }
      }

      // We can safely remove it from the current downloads
      mDownloadManager->RemoveDownloadFromCurrent(this);

      if (mDownloadState != nsIXPInstallManagerUI::INSTALL_INSTALLING)
        SetState(nsIDownloadManager::DOWNLOAD_FINISHED);
      else
        SetState(nsIXPInstallManagerUI::INSTALL_FINISHED);

      mDownloadManager->mObserverService->NotifyObservers(this, "dl-done",
                                                          nsnull);
    }

#ifdef XP_WIN
    PRBool addToRecentDocs = PR_TRUE;
    if (pref)
      pref->GetBoolPref(PREF_BDM_ADDTORECENTDOCS, &addToRecentDocs);

    if (addToRecentDocs) {
      LPSHELLFOLDER lpShellFolder = NULL;

      if (SUCCEEDED(::SHGetDesktopFolder(&lpShellFolder))) {
        nsCOMPtr<nsIFileURL> fileURL = do_QueryInterface(mTarget, &rv);
        NS_ENSURE_SUCCESS(rv, rv);

        nsCOMPtr<nsIFile> file;
        rv = fileURL->GetFile(getter_Addrefs(file));
        NS_ENSURE_SUCCESS(rv, rv);
        
        nsAutoString path;
        rv = file->GetPath(&path);
        NS_ENSURE_SUCCESS(rv, rv);

        PRUnichar *filePath = ToNewUnicode(path);
        LPITEMIDLIST lpItemIDList = NULL;
        if (SUCCEEDED(lpShellFolder->ParseDisplayName(NULL, NULL, filePath, NULL, &lpItemIDList, NULL))) {
          ::SHAddToRecentDocs(SHARD_PIDL, lpItemIDList);
          ::CoTaskMemFree(lpItemIDList);
        }
        nsMemory::Free(filePath);
        lpShellFolder->Release();
      }
    }
#endif

    // break the cycle we created in AddDownload
    mCancelable = nsnull;

    // Now remove the download if the user's retention policy is "Remove when Done"
    if (mDownloadManager->GetRetentionBehavior() == 0)
      mDownloadManager->RemoveDownload(mID);
  }

  if (mDownloadManager->NeedsUIUpdate()) {
    nsIDownloadProgressListener* dpl = mDownloadManager->mListener;
    if (dpl)
      dpl->OnStateChange(aWebProgress, aRequest, aStateFlags, aStatus, this);
  }

  nsresult rv = UpdateDB();

  return rv;
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
nsDownload::Init(nsIURI* aSource,
                 nsIURI* aTarget,
                 const nsAString& aDisplayName,
                 nsIMIMEInfo *aMIMEInfo,
                 PRTime aStartTime,
                 nsILocalFile* aTempFile,
                 nsICancelable* aCancelable)
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
nsDownload::GetCancelable(nsICancelable** aCancelable)
{
  *aCancelable = mCancelable;
  NS_IF_ADDREF(*aCancelable);
  return NS_OK;
}

NS_IMETHODIMP
nsDownload::GetTarget(nsIURI** aTarget)
{
  *aTarget = mTarget;
  NS_IF_ADDREF(*aTarget);
  return NS_OK;
}

NS_IMETHODIMP
nsDownload::GetSource(nsIURI** aSource)
{
  *aSource = mSource;
  NS_IF_ADDREF(*aSource);
  return NS_OK;
}

NS_IMETHODIMP
nsDownload::GetStartTime(PRInt64* aStartTime)
{
  *aStartTime = mStartTime;
  return NS_OK;
}

NS_IMETHODIMP
nsDownload::GetPercentComplete(PRInt32* aPercentComplete)
{
  *aPercentComplete = mPercentComplete;
  return NS_OK;
}

NS_IMETHODIMP
nsDownload::GetAmountTransferred(PRUint64* aAmountTransferred)
{
  *aAmountTransferred = mCurrBytes;
  return NS_OK;
}

NS_IMETHODIMP
nsDownload::GetSize(PRUint64* aSize)
{
  *aSize = mMaxBytes;
  return NS_OK;
}

NS_IMETHODIMP
nsDownload::GetMIMEInfo(nsIMIMEInfo** aMIMEInfo)
{
  *aMIMEInfo = mMIMEInfo;
  NS_IF_ADDREF(*aMIMEInfo);
  return NS_OK;
}

NS_IMETHODIMP
nsDownload::GetTargetFile(nsILocalFile** aTargetFile)
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
nsDownload::GetSpeed(double* aSpeed)
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

nsresult
nsDownload::PauseResume(PRBool aPause)
{
  if (mPaused != aPause && mRequest) {
    if (aPause) {
      nsresult rv = mRequest->Suspend();
      NS_ENSURE_SUCCESS(rv, rv);
      mPaused = PR_TRUE;
      SetState(nsIDownloadManager::DOWNLOAD_PAUSED);
    } else {
      nsresult rv = mRequest->Resume();
      NS_ENSURE_SUCCESS(rv, rv);
      mPaused = PR_FALSE;
      SetState(nsIDownloadManager::DOWNLOAD_DOWNLOADING);
    }
  
    return UpdateDB();
  }

  return NS_OK;
}

nsresult
nsDownload::UpdateDB()
{
  NS_ASSERTION(mID, "Download ID is stored as zero.  This is bad!");

  nsCOMPtr<mozIStorageStatement> stmt;
  nsresult rv = mDownloadManager->mDBConn->CreateStatement(NS_LITERAL_CSTRING(
    "UPDATE moz_downloads "
    "SET name = ?1, source = ?2, target = ?3, startTime = ?4, endTime = ?5,"
    "state = ?6 "
    "WHERE id = ?7"), getter_AddRefs(stmt));
  NS_ENSURE_SUCCESS(rv, rv);
  
  // name
  rv = stmt->BindStringParameter(0, mDisplayName);
  NS_ENSURE_SUCCESS(rv, rv);

  // source
  nsCAutoString src;
  rv = mSource->GetSpec(src);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = stmt->BindUTF8StringParameter(1, src);
  NS_ENSURE_SUCCESS(rv, rv);

  // target
  nsCAutoString target;
  rv = mTarget->GetSpec(target);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = stmt->BindUTF8StringParameter(2, target);
  NS_ENSURE_SUCCESS(rv, rv);

  // startTime
  rv = stmt->BindInt64Parameter(3, mStartTime);
  NS_ENSURE_SUCCESS(rv, rv);

  // endTime
  rv = stmt->BindInt64Parameter(4, mLastUpdate);
  NS_ENSURE_SUCCESS(rv, rv);

  // state
  rv = stmt->BindInt32Parameter(5, mDownloadState);
  NS_ENSURE_SUCCESS(rv, rv);

  // id
  rv = stmt->BindInt64Parameter(6, mID);
  NS_ENSURE_SUCCESS(rv, rv);

  return stmt->Execute();
}
