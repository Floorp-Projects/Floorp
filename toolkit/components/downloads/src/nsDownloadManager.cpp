/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
 
#include "nsDownloadManager.h"
#include "nsIWebProgress.h"
#include "nsIRDFLiteral.h"
#include "rdf.h"
#include "nsNetUtil.h"
#include "nsIDOMChromeWindow.h"
#include "nsIDOMWindow.h"
#include "nsIDOMWindowInternal.h"
#include "nsIDOMEvent.h"
#include "nsIDOMEventTarget.h"
#include "nsRDFCID.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsIWebBrowserPersist.h"
#include "nsIObserver.h"
#include "nsIProgressDialog.h"
#include "nsIWebBrowserPersist.h"
#include "nsIWindowWatcher.h"
#include "nsIStringBundle.h"
#include "nsCRT.h"
#include "nsIWindowMediator.h"
#include "nsIPromptService.h"
#include "nsIObserverService.h"
#include "nsIPrefBranch.h"
#include "nsIPrefService.h"
#include "nsVoidArray.h"
#include "nsEnumeratorUtils.h"
#include "nsIFileURL.h"

/* Outstanding issues/todo:
 * 1. Implement pause/resume.
 */
  
static NS_DEFINE_CID(kRDFServiceCID, NS_RDFSERVICE_CID);
static NS_DEFINE_CID(kStringBundleServiceCID, NS_STRINGBUNDLESERVICE_CID);
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
#define PREF_BDM_FLASHCOUNT "browser.download.manager.flashCount"
#define INTERVAL 500

static nsIRDFResource* gNC_DownloadsRoot = nsnull;
static nsIRDFResource* gNC_File = nsnull;
static nsIRDFResource* gNC_URL = nsnull;
static nsIRDFResource* gNC_IconURL = nsnull;
static nsIRDFResource* gNC_Name = nsnull;
static nsIRDFResource* gNC_ProgressPercent = nsnull;
static nsIRDFResource* gNC_Transferred = nsnull;
static nsIRDFResource* gNC_DownloadState = nsnull;
static nsIRDFResource* gNC_StatusText = nsnull;
static nsIRDFResource* gNC_DateStarted = nsnull;
static nsIRDFResource* gNC_DateEnded = nsnull;

static nsIRDFService* gRDFService = nsnull;
static nsIObserverService* gObserverService = nsnull;
static PRInt32 gRefCnt = 0;

/**
 * Extract the file path associated with a URI.  We try to convert to a
 * nsIFile instead of extracting the path from the URI directly since this
 * ensures that we get a string in the right charset and that all %-encoded
 * characters have been expanded.
 */
static nsresult
GetFilePathFromURI(nsIURI *aURI, nsAString &aPath)
{
  nsresult rv;

  nsCOMPtr<nsIFileURL> fileURL = do_QueryInterface(aURI, &rv);
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsIFile> file;
  rv = fileURL->GetFile(getter_AddRefs(file));
  if (NS_SUCCEEDED(rv))
    rv = file->GetPath(aPath);

  return rv;
}

///////////////////////////////////////////////////////////////////////////////
// nsDownloadManager

#ifdef XP_WIN
NS_IMPL_ISUPPORTS4(nsDownloadManager, nsIDownloadManager, nsIXPInstallManagerUI, nsIObserver, nsIAlertListener)
#else
NS_IMPL_ISUPPORTS3(nsDownloadManager, nsIDownloadManager, nsIXPInstallManagerUI, nsIObserver)
#endif

nsDownloadManager::nsDownloadManager() : mBatches(0)
{
}

nsDownloadManager::~nsDownloadManager()
{
  if (--gRefCnt != 0 || !gRDFService  || !gObserverService)
    // Either somebody tried to use |CreateInstance| instead of
    // |GetService| or |Init| failed very early, so there's nothing to
    // do here.
    return;

  gRDFService->UnregisterDataSource(mDataSource);

  gObserverService->RemoveObserver(this, "quit-application");
  gObserverService->RemoveObserver(this, "quit-application-requested");
  gObserverService->RemoveObserver(this, "offline-requested");

  NS_IF_RELEASE(gNC_DownloadsRoot);                                             
  NS_IF_RELEASE(gNC_File);                                                      
  NS_IF_RELEASE(gNC_URL);
  NS_IF_RELEASE(gNC_IconURL);
  NS_IF_RELEASE(gNC_Name);                                                      
  NS_IF_RELEASE(gNC_ProgressPercent);
  NS_IF_RELEASE(gNC_Transferred);
  NS_IF_RELEASE(gNC_DownloadState);
  NS_IF_RELEASE(gNC_StatusText);
  NS_IF_RELEASE(gNC_DateStarted);
  NS_IF_RELEASE(gNC_DateEnded);

  NS_RELEASE(gRDFService);
  NS_RELEASE(gObserverService);
}

PRInt32 PR_CALLBACK nsDownloadManager::CancelAllDownloads(nsHashKey* aKey, void* aData, void* aClosure)
{
  nsStringKey* key = (nsStringKey*)aKey;
  nsresult rv;

  nsCOMPtr<nsIDownloadManager> manager = do_QueryInterface((nsISupports*)aClosure, &rv);
  if (NS_FAILED(rv)) return kHashEnumerateRemove;  
  
  if (IsInProgress(NS_STATIC_CAST(nsDownload*, aData)->GetDownloadState()))  
    manager->CancelDownload(key->GetString());
  else
    NS_STATIC_CAST(nsDownloadManager*, aClosure)->DownloadEnded(key->GetString(), nsnull);
 
  return kHashEnumerateRemove;
}

nsresult
nsDownloadManager::Init()
{
  if (gRefCnt++ != 0) {
    NS_NOTREACHED("download manager should be used as a service");
    return NS_ERROR_UNEXPECTED; // This will make the |CreateInstance| fail.
  }

  nsresult rv;
  mRDFContainerUtils = do_GetService("@mozilla.org/rdf/container-utils;1", &rv);
  if (NS_FAILED(rv)) return rv;

  rv = CallGetService("@mozilla.org/observer-service;1", &gObserverService);
  if (NS_FAILED(rv)) return rv;
  
  rv = CallGetService(kRDFServiceCID, &gRDFService);
  if (NS_FAILED(rv)) return rv;                                                 

  gRDFService->GetResource(NS_LITERAL_CSTRING("NC:DownloadsRoot"), &gNC_DownloadsRoot);
  gRDFService->GetResource(NS_LITERAL_CSTRING(NC_NAMESPACE_URI "File"), &gNC_File);
  gRDFService->GetResource(NS_LITERAL_CSTRING(NC_NAMESPACE_URI "URL"), &gNC_URL);
  gRDFService->GetResource(NS_LITERAL_CSTRING(NC_NAMESPACE_URI "IconURL"), &gNC_IconURL);
  gRDFService->GetResource(NS_LITERAL_CSTRING(NC_NAMESPACE_URI "Name"), &gNC_Name);
  gRDFService->GetResource(NS_LITERAL_CSTRING(NC_NAMESPACE_URI "ProgressPercent"), &gNC_ProgressPercent);
  gRDFService->GetResource(NS_LITERAL_CSTRING(NC_NAMESPACE_URI "Transferred"), &gNC_Transferred);
  gRDFService->GetResource(NS_LITERAL_CSTRING(NC_NAMESPACE_URI "DownloadState"), &gNC_DownloadState);
  gRDFService->GetResource(NS_LITERAL_CSTRING(NC_NAMESPACE_URI "StatusText"), &gNC_StatusText);
  gRDFService->GetResource(NS_LITERAL_CSTRING(NC_NAMESPACE_URI "DateStarted"), &gNC_DateStarted);
  gRDFService->GetResource(NS_LITERAL_CSTRING(NC_NAMESPACE_URI "DateEnded"), &gNC_DateEnded);

  mDataSource = new nsDownloadsDataSource();
  if (!mDataSource)
    return NS_ERROR_OUT_OF_MEMORY;

  rv = NS_STATIC_CAST(nsDownloadsDataSource*, (nsIRDFDataSource*)mDataSource.get())->LoadDataSource();
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsIStringBundleService> bundleService = do_GetService(kStringBundleServiceCID, &rv);
  if (NS_FAILED(rv)) return rv;
  
  rv = bundleService->CreateBundle(DOWNLOAD_MANAGER_BUNDLE, getter_AddRefs(mBundle));
  if (NS_FAILED(rv))
    return rv;

  // The following three AddObserver calls must be the last lines in this function,
  // because otherwise, this function may fail (and thus, this object would be not
  // completely initialized), but the observerservice would still keep a reference
  // to us and notify us about shutdown, which may cause crashes.
  // failure to add an observer is not critical
  gObserverService->AddObserver(this, "quit-application", PR_FALSE);
  gObserverService->AddObserver(this, "quit-application-requested", PR_FALSE);
  gObserverService->AddObserver(this, "offline-requested", PR_FALSE);

  return NS_OK;
}

PRInt32 
nsDownloadManager::GetRetentionBehavior()
{
  PRInt32 val = 0;
  nsCOMPtr<nsIPrefBranch> pref(do_GetService(NS_PREFSERVICE_CONTRACTID));
  if (pref) {
    nsresult rv = pref->GetIntPref(PREF_BDM_RETENTION, &val);
    if (NS_FAILED(rv))
      val = 0; // Use 0 as the default ("remove when done")
  }

  return val;
}

nsresult
nsDownloadManager::DownloadStarted(const PRUnichar* aPath)
{
  nsStringKey key(aPath);
  if (mCurrDownloads.Exists(&key)) {
  
    // Assert the date and time that the download ended.    
    nsCOMPtr<nsIRDFDate> dateLiteral;
    if (NS_SUCCEEDED(gRDFService->GetDateLiteral(PR_Now(), getter_AddRefs(dateLiteral)))) {    
      nsCOMPtr<nsIRDFResource> res;
      nsCOMPtr<nsIRDFNode> node;
      
      gRDFService->GetUnicodeResource(nsDependentString(aPath), getter_AddRefs(res));
      
      mDataSource->GetTarget(res, gNC_DateStarted, PR_TRUE, getter_AddRefs(node));
      if (node)
        mDataSource->Change(res, gNC_DateStarted, node, dateLiteral);
      else
        mDataSource->Assert(res, gNC_DateStarted, dateLiteral, PR_TRUE);
    }
  
    AssertProgressInfoFor(aPath);
  }

  return NS_OK;
}

nsresult
nsDownloadManager::DownloadEnded(const PRUnichar* aPath, const PRUnichar* aMessage)
{
  nsStringKey key(aPath);
  if (mCurrDownloads.Exists(&key)) {

    // Assert the date and time that the download ended.    
    nsCOMPtr<nsIRDFDate> dateLiteral;
    if (NS_SUCCEEDED(gRDFService->GetDateLiteral(PR_Now(), getter_AddRefs(dateLiteral)))) {    
      nsCOMPtr<nsIRDFResource> res;
      nsCOMPtr<nsIRDFNode> node;
      
      gRDFService->GetUnicodeResource(nsDependentString(aPath), getter_AddRefs(res));
      
      mDataSource->GetTarget(res, gNC_DateEnded, PR_TRUE, getter_AddRefs(node));
      if (node)
        mDataSource->Change(res, gNC_DateEnded, node, dateLiteral);
      else
        mDataSource->Assert(res, gNC_DateEnded, dateLiteral, PR_TRUE);
    }

    AssertProgressInfoFor(aPath);
    
    nsDownload* download = NS_STATIC_CAST(nsDownload*, mCurrDownloads.Get(&key));
    NS_RELEASE(download);

    if (!gStoppingDownloads)
      mCurrDownloads.Remove(&key);
  }

  return NS_OK;
}

nsresult
nsDownloadManager::GetDownloadsContainer(nsIRDFContainer** aResult)
{
  if (mDownloadsContainer) {
    *aResult = mDownloadsContainer;
    NS_ADDREF(*aResult);
    return NS_OK;
  }

  PRBool isContainer;
  nsresult rv = mRDFContainerUtils->IsContainer(mDataSource, gNC_DownloadsRoot, &isContainer);
  if (NS_FAILED(rv)) return rv;

  if (!isContainer) {
    rv = mRDFContainerUtils->MakeSeq(mDataSource, gNC_DownloadsRoot, getter_AddRefs(mDownloadsContainer));
    if (NS_FAILED(rv)) return rv;
  }
  else {
    mDownloadsContainer = do_CreateInstance(NS_RDF_CONTRACTID "/container;1", &rv);
    if (NS_FAILED(rv)) return rv;
    rv = mDownloadsContainer->Init(mDataSource, gNC_DownloadsRoot);
    if (NS_FAILED(rv)) return rv;
  }

  *aResult = mDownloadsContainer;
  NS_IF_ADDREF(*aResult);

  return rv;
}

nsresult
nsDownloadManager::GetInternalListener(nsIDownloadProgressListener** aListener)
{
  *aListener = mListener;
  NS_IF_ADDREF(*aListener);
  return NS_OK;
}

NS_IMETHODIMP
nsDownloadManager::GetActiveDownloadCount(PRInt32* aResult)
{
  *aResult = mCurrDownloads.Count();
  
  return NS_OK;
}

NS_IMETHODIMP
nsDownloadManager::GetActiveDownloads(nsISupportsArray** aResult)
{
  nsCOMPtr<nsISupportsArray> ary;
  NS_NewISupportsArray(getter_AddRefs(ary));
  mCurrDownloads.Enumerate(BuildActiveDownloadsList, (void*)ary);

  NS_ADDREF(*aResult = ary);

  return NS_OK;
}

PRInt32 PR_CALLBACK 
nsDownloadManager::BuildActiveDownloadsList(nsHashKey* aKey, void* aData, void* aClosure)
{
  nsCOMPtr<nsISupportsArray> ary(do_QueryInterface((nsISupports*)aClosure));
  nsCOMPtr<nsIDownload> dl(do_QueryInterface((nsISupports*)aData));  

  ary->AppendElement(dl);
 
  return kHashEnumerateNext;
}

NS_IMETHODIMP
nsDownloadManager::SaveState()
{
  nsCOMPtr<nsISupports> supports;
  nsCOMPtr<nsIRDFResource> res;
  nsCOMPtr<nsIRDFInt> intLiteral;

  DownloadState states[] = { nsIDownloadManager::DOWNLOAD_DOWNLOADING, 
                             nsIDownloadManager::DOWNLOAD_PAUSED,
                             nsIXPInstallManagerUI::INSTALL_DOWNLOADING,
                             nsIXPInstallManagerUI::INSTALL_INSTALLING };

  for (int i = 0; i < 4; ++i) {
    gRDFService->GetIntLiteral(states[i], getter_AddRefs(intLiteral));
    nsCOMPtr<nsISimpleEnumerator> downloads;
    nsresult rv = mDataSource->GetSources(gNC_DownloadState, intLiteral, PR_TRUE, getter_AddRefs(downloads));
    if (NS_FAILED(rv)) return rv;
  
    PRBool hasMoreElements;
    downloads->HasMoreElements(&hasMoreElements);

    while (hasMoreElements) {
      const char* uri;
      downloads->GetNext(getter_AddRefs(supports));
      res = do_QueryInterface(supports);
      res->GetValueConst(&uri);
      AssertProgressInfoFor(NS_ConvertASCIItoUCS2(uri).get());
      downloads->HasMoreElements(&hasMoreElements);
    }
  }

  return NS_OK;
}

nsresult
nsDownloadManager::AssertProgressInfoFor(const PRUnichar* aPath)
{
  nsStringKey key(aPath);
  if (!mCurrDownloads.Exists(&key))
    return NS_ERROR_FAILURE;
 
  nsDownload* internalDownload = NS_STATIC_CAST(nsDownload*, mCurrDownloads.Get(&key));
  nsCOMPtr<nsIDownload> download;
  internalDownload->QueryInterface(NS_GET_IID(nsIDownload), (void**) getter_AddRefs(download));
  if (!download)
    return NS_ERROR_FAILURE;
  
  nsresult rv;
  PRInt32 percentComplete;
  nsCOMPtr<nsIRDFNode> oldTarget;
  nsCOMPtr<nsIRDFInt> intLiteral;
  nsCOMPtr<nsIRDFResource> res;
  nsCOMPtr<nsIRDFLiteral> literal;

  gRDFService->GetUnicodeResource(nsDependentString(aPath), getter_AddRefs(res));

  DownloadState state = internalDownload->GetDownloadState();
 
  // update download state (not started, downloading, queued, finished, etc...)
  gRDFService->GetIntLiteral(state, getter_AddRefs(intLiteral));

  mDataSource->GetTarget(res, gNC_DownloadState, PR_TRUE, getter_AddRefs(oldTarget));
  
  if (oldTarget)
    rv = mDataSource->Change(res, gNC_DownloadState, oldTarget, intLiteral);
  else
    rv = mDataSource->Assert(res, gNC_DownloadState, intLiteral, PR_TRUE);
  if (NS_FAILED(rv)) return rv;

  // update percentage
  download->GetPercentComplete(&percentComplete);

  mDataSource->GetTarget(res, gNC_ProgressPercent, PR_TRUE, getter_AddRefs(oldTarget));
  gRDFService->GetIntLiteral(percentComplete, getter_AddRefs(intLiteral));

  if (oldTarget)
    rv = mDataSource->Change(res, gNC_ProgressPercent, oldTarget, intLiteral);
  else
    rv = mDataSource->Assert(res, gNC_ProgressPercent, intLiteral, PR_TRUE);
  if (NS_FAILED(rv)) return rv;

  // update transferred
  PRInt32 current = 0;
  PRInt32 max = 0;
  internalDownload->GetTransferInformation(&current, &max);
 
  nsAutoString currBytes; currBytes.AppendInt(current);
  nsAutoString maxBytes; maxBytes.AppendInt(max);
  const PRUnichar *strings[] = {
    currBytes.get(),
    maxBytes.get()
  };

  nsXPIDLString value; 
  rv = mBundle->FormatStringFromName(NS_LITERAL_STRING("transferred").get(),
                                     strings, 2, getter_Copies(value));
  if (NS_FAILED(rv)) return rv;

  gRDFService->GetLiteral(value, getter_AddRefs(literal));
 
  mDataSource->GetTarget(res, gNC_Transferred, PR_TRUE, getter_AddRefs(oldTarget));
 
  if (oldTarget)
    rv = mDataSource->Change(res, gNC_Transferred, oldTarget, literal);
  else
    rv = mDataSource->Assert(res, gNC_Transferred, literal, PR_TRUE);
  if (NS_FAILED(rv)) return rv;

  // XXX should also store and update time elapsed
  return Flush();
}  

///////////////////////////////////////////////////////////////////////////////
// nsIDownloadManager

NS_IMETHODIMP
nsDownloadManager::AddDownload(DownloadType aDownloadType, 
                               nsIURI* aSource,
                               nsIURI* aTarget,
                               const PRUnichar* aDisplayName,
                               const PRUnichar* aIconURL, 
                               nsIMIMEInfo *aMIMEInfo,
                               PRInt64 aStartTime,
                               nsIWebBrowserPersist* aPersist,
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

  nsCOMPtr<nsIRDFContainer> downloads;
  rv = GetDownloadsContainer(getter_AddRefs(downloads));
  if (NS_FAILED(rv)) return rv;

  nsDownload* internalDownload = new nsDownload();
  if (!internalDownload)
    return NS_ERROR_OUT_OF_MEMORY;

  internalDownload->QueryInterface(NS_GET_IID(nsIDownload), (void**) aDownload);
  if (!aDownload)
    return NS_ERROR_FAILURE;

  NS_ADDREF(*aDownload);

  // give our new nsIDownload some info so it's ready to go off into the world
  internalDownload->SetDownloadManager(this);
  internalDownload->SetTarget(aTarget);
  internalDownload->SetSource(aSource);

  // The path is the uniquifier of the download resource. 
  // XXXben - this is a little risky - really we should be using anonymous
  // resources because of duplicate file paths on MacOSX. We can't use persistent
  // descriptor here because in most cases the file doesn't exist on the local disk
  // yet (it's being downloaded) and persistentDescriptor fails on MacOSX for 
  // non-existent files. 

  // XXXben - This is not really ideal. If the download is to be handled by a 
  //          helper application, then we want to see if there is a duplicate file
  //          in place in the temp folder and remove it _NOW_ before the External
  //          Helper App Service gets a chance to make a unique clone. If we don't,
  //          the EHAS will create a unique version of the name which will muck
  //          with our RDF datasource. We can't create a unique name here either,
  //          because the EHAS isn't smart enough to know that we're fooling with
  //          it... 
  nsMIMEInfoHandleAction action = nsIMIMEInfo::saveToDisk;
  if (aMIMEInfo) {
    aMIMEInfo->GetPreferredAction(&action);
    if (action == nsIMIMEInfo::useHelperApp || 
      action == nsIMIMEInfo::useSystemDefault) {
      PRBool fileExists;
      targetFile->Exists(&fileExists);
      if (fileExists)
        targetFile->Remove(PR_TRUE);
    }
  }

  nsAutoString path;
  rv = targetFile->GetPath(path);
  if (NS_FAILED(rv)) return rv;

  nsStringKey key(path);
  if (mCurrDownloads.Exists(&key))
    CancelDownload(path.get());

  nsCOMPtr<nsIRDFResource> downloadRes;
  gRDFService->GetUnicodeResource(path, getter_AddRefs(downloadRes));

  // Save state of existing downloads NOW... because inserting the new 
  // download resource into the container will cause the FE to rebuild and all 
  // the active downloads will have their progress reset (since progress is held
  // mostly by the FE and not the datasource) until they get another progress
  // notification (which could be a while for slow downloads).
  SaveState();

  // if the resource is in the container already (the user has already
  // downloaded this file), remove it
  PRInt32 itemIndex;
  nsCOMPtr<nsIRDFNode> node;
  downloads->IndexOf(downloadRes, &itemIndex);
  if (itemIndex > 0) {
    rv = downloads->RemoveElementAt(itemIndex, PR_TRUE, getter_AddRefs(node));
    if (NS_FAILED(rv)) return rv;
  }
  rv = downloads->InsertElementAt(downloadRes, 1, PR_TRUE);
  if (NS_FAILED(rv)) return rv;
  
  // Assert source url information
  nsCAutoString spec;
  aSource->GetSpec(spec);

  nsCOMPtr<nsIRDFResource> urlResource;
  gRDFService->GetResource(spec, getter_AddRefs(urlResource));
  mDataSource->GetTarget(downloadRes, gNC_URL, PR_TRUE, getter_AddRefs(node));
  if (node)
    rv = mDataSource->Change(downloadRes, gNC_URL, node, urlResource);
  else
    rv = mDataSource->Assert(downloadRes, gNC_URL, urlResource, PR_TRUE);
  if (NS_FAILED(rv)) {
    downloads->IndexOf(downloadRes, &itemIndex);
    downloads->RemoveElementAt(itemIndex, PR_TRUE, getter_AddRefs(node));
    return rv;
  }

  // Set and assert the "pretty" (display) name of the download
  nsAutoString displayName; displayName.Assign(aDisplayName);
  if (displayName.IsEmpty()) {
    targetFile->GetLeafName(displayName);
  }
  (*aDownload)->SetDisplayName(displayName.get());
 
  nsCOMPtr<nsIRDFLiteral> nameLiteral;
  gRDFService->GetLiteral(displayName.get(), getter_AddRefs(nameLiteral));
  mDataSource->GetTarget(downloadRes, gNC_Name, PR_TRUE, getter_AddRefs(node));
  if (node)
    rv = mDataSource->Change(downloadRes, gNC_Name, node, nameLiteral);
  else
    rv = mDataSource->Assert(downloadRes, gNC_Name, nameLiteral, PR_TRUE);
  if (NS_FAILED(rv)) {
    downloads->IndexOf(downloadRes, &itemIndex);
    downloads->RemoveElementAt(itemIndex, PR_TRUE, getter_AddRefs(node));
    return rv;
  }
  
  // Assert icon information
  if (aIconURL) {
    nsCOMPtr<nsIRDFResource> iconURIRes;
    nsDependentString iconURL(aIconURL);
    gRDFService->GetUnicodeResource(iconURL, getter_AddRefs(iconURIRes));
    mDataSource->GetTarget(downloadRes, gNC_IconURL, PR_TRUE, getter_AddRefs(node));
    if (node)
      rv = mDataSource->Change(downloadRes, gNC_IconURL, node, iconURIRes);
    else
      rv = mDataSource->Assert(downloadRes, gNC_IconURL, iconURIRes, PR_TRUE);
  }

  internalDownload->SetMIMEInfo(aMIMEInfo);
  internalDownload->SetStartTime(aStartTime);

  // Assert file information
  nsCOMPtr<nsIRDFResource> fileResource;
  gRDFService->GetUnicodeResource(path, getter_AddRefs(fileResource));
  rv = mDataSource->Assert(downloadRes, gNC_File, fileResource, PR_TRUE);
  if (NS_FAILED(rv)) {
    downloads->IndexOf(downloadRes, &itemIndex);
    downloads->RemoveElementAt(itemIndex, PR_TRUE, getter_AddRefs(node));
    return rv;
  }
  
  // Assert download state information (NOTSTARTED, since it's just now being added)
  nsCOMPtr<nsIRDFInt> intLiteral;
  gRDFService->GetIntLiteral(nsIDownloadManager::DOWNLOAD_NOTSTARTED, getter_AddRefs(intLiteral));
  mDataSource->GetTarget(downloadRes, gNC_DownloadState, PR_TRUE, getter_AddRefs(node));
  if (node)
    rv = mDataSource->Change(downloadRes, gNC_DownloadState, node, intLiteral);
  else
    rv = mDataSource->Assert(downloadRes, gNC_DownloadState, intLiteral, PR_TRUE);
  if (NS_FAILED(rv)) {
    downloads->IndexOf(downloadRes, &itemIndex);
    downloads->RemoveElementAt(itemIndex, PR_TRUE, getter_AddRefs(node));
    return rv;
  }

  // Now flush all this to disk
  if (NS_FAILED(Flush())) {
    downloads->IndexOf(downloadRes, &itemIndex);
    downloads->RemoveElementAt(itemIndex, PR_TRUE, getter_AddRefs(node));
    return rv;
  }

  // if a persist object was specified, set the download item as the progress listener
  // this will create a cycle that will be broken in nsDownload::OnStateChange
  if (aPersist) {
    internalDownload->SetPersist(aPersist);
    nsCOMPtr<nsIWebProgressListener> listener = do_QueryInterface(*aDownload);
    aPersist->SetProgressListener(listener);
  }

  // If this is an install operation, ensure we have a progress listener for the
  // install and track this download separately. 
  if (aDownloadType == nsIXPInstallManagerUI::DOWNLOAD_TYPE_INSTALL) {
    if (!mXPIProgress)
      mXPIProgress = new nsXPIProgressListener(this);

    nsIXPIProgressDialog* dialog = mXPIProgress.get();
    nsXPIProgressListener* listener = NS_STATIC_CAST(nsXPIProgressListener*, dialog);
    listener->AddDownload(*aDownload);
  }

  mCurrDownloads.Put(&key, *aDownload);
  gObserverService->NotifyObservers(*aDownload, "dl-start", nsnull);

  return rv;
}

NS_IMETHODIMP
nsDownloadManager::GetDownload(const PRUnichar* aPath, nsIDownload** aDownloadItem)
{
  NS_ENSURE_ARG_POINTER(aDownloadItem);

  // if it's currently downloading we can get it from the table
  // XXX otherwise we should look for it in the datasource and
  //     create a new nsIDownload with the resource's properties
  nsStringKey key(aPath);
  if (mCurrDownloads.Exists(&key)) {
    *aDownloadItem = NS_STATIC_CAST(nsIDownload*, mCurrDownloads.Get(&key));
    NS_ADDREF(*aDownloadItem);
    return NS_OK;
  }

  *aDownloadItem = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
nsDownloadManager::CancelDownload(const PRUnichar* aPath)
{
  nsresult rv = NS_OK;
  nsStringKey key(aPath);
  if (!mCurrDownloads.Exists(&key))
    return RemoveDownload(aPath); // XXXBlake for now, to provide a workaround for stuck downloads
  
  nsDownload* internalDownload = NS_STATIC_CAST(nsDownload*, mCurrDownloads.Get(&key));
  nsCOMPtr<nsIDownload> download;
  CallQueryInterface(internalDownload, NS_STATIC_CAST(nsIDownload**, 
                                                      getter_AddRefs(download)));
  if (!download)
    return NS_ERROR_FAILURE;

  // Don't cancel if download is already finished
  if (CompletedSuccessfully(internalDownload->mDownloadState))
    return NS_OK;

  internalDownload->SetDownloadState(nsIDownloadManager::DOWNLOAD_CANCELED);

  // if a persist was provided, we can do the cancel ourselves.
  nsCOMPtr<nsIWebBrowserPersist> persist;
  download->GetPersist(getter_AddRefs(persist));
  if (persist) {
    rv = persist->CancelSave();
    if (NS_FAILED(rv)) return rv;
  }

  // if an observer was provided, notify that the download was cancelled.
  // if no persist was provided, this is necessary so that whatever transfer
  // component being used can cancel the download itself.
  nsCOMPtr<nsIObserver> observer;
  download->GetObserver(getter_AddRefs(observer));
  if (observer) {
    rv = observer->Observe(download, "oncancel", nsnull);
    if (NS_FAILED(rv)) return rv;
  }
 
  DownloadEnded(aPath, nsnull);

  gObserverService->NotifyObservers(download, "dl-cancel", nsnull);

  // if there's a progress dialog open for the item,
  // we have to notify it that we're cancelling
  nsCOMPtr<nsIProgressDialog> dialog;
  internalDownload->GetDialog(getter_AddRefs(dialog));
  if (dialog) {
    observer = do_QueryInterface(dialog);
    rv = observer->Observe(download, "oncancel", nsnull);
    if (NS_FAILED(rv)) return rv;
  }
  
  return rv;
}

NS_IMETHODIMP
nsDownloadManager::RemoveDownload(const PRUnichar* aPath)
{
  nsStringKey key(aPath);
  
  // RemoveDownload is for downloads not currently in progress. Having it
  // cancel in-progress downloads would make things complicated, so just return.
  PRBool inProgress = mCurrDownloads.Exists(&key);
  NS_ASSERTION(!inProgress, "Can't call RemoveDownload on a download in progress!");
  if (inProgress)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIRDFResource> res;
  gRDFService->GetUnicodeResource(nsDependentString(aPath), getter_AddRefs(res));

  return RemoveDownload(res);
}

// This is an internal method only because it does NOT check to see whether
// or not a download is in progress or not before removing it. The FE should use
// RemoveDownload(const PRUnichar* aPath) as supplied by nsIDownloadManager which
// does the appropriate checking. 
nsresult
nsDownloadManager::RemoveDownload(nsIRDFResource* aDownload)
{
  nsCOMPtr<nsIRDFContainer> downloads;
  nsresult rv = GetDownloadsContainer(getter_AddRefs(downloads));
  if (NS_FAILED(rv)) return rv;
  
  // remove all the arcs for this resource, and then remove it from the Seq
  nsCOMPtr<nsISimpleEnumerator> arcs;
  rv = mDataSource->ArcLabelsOut(aDownload, getter_AddRefs(arcs));
  if (NS_FAILED(rv)) return rv;

  PRBool moreArcs;
  rv = arcs->HasMoreElements(&moreArcs);
  if (NS_FAILED(rv)) return rv;

  while (moreArcs) {
    nsCOMPtr<nsISupports> supports;
    rv = arcs->GetNext(getter_AddRefs(supports));
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIRDFResource> arc(do_QueryInterface(supports, &rv));
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsISimpleEnumerator> targets;
    rv = mDataSource->GetTargets(aDownload, arc, PR_TRUE, getter_AddRefs(targets));
    if (NS_FAILED(rv)) return rv;

    PRBool moreTargets;
    rv = targets->HasMoreElements(&moreTargets);
    if (NS_FAILED(rv)) return rv;

    while (moreTargets) {
      rv = targets->GetNext(getter_AddRefs(supports));
      if (NS_FAILED(rv)) return rv;

      nsCOMPtr<nsIRDFNode> target(do_QueryInterface(supports, &rv));
      if (NS_FAILED(rv)) return rv;

      // and now drop this assertion from the graph
      rv = mDataSource->Unassert(aDownload, arc, target);
      if (NS_FAILED(rv)) return rv;

      rv = targets->HasMoreElements(&moreTargets);
      if (NS_FAILED(rv)) return rv;
    }
    rv = arcs->HasMoreElements(&moreArcs);
    if (NS_FAILED(rv)) return rv;
  }

  PRInt32 itemIndex;
  downloads->IndexOf(aDownload, &itemIndex);
  if (itemIndex <= 0)
    return NS_ERROR_FAILURE;
  
  nsCOMPtr<nsIRDFNode> node;
  rv = downloads->RemoveElementAt(itemIndex, PR_TRUE, getter_AddRefs(node));
  if (NS_FAILED(rv)) return rv;
  
  // if a mass removal is being done, we don't want to flush every time
  if (mBatches) return rv;

  return Flush();
}  

// We implement this here in the download manager service as inspecting 
// datasource directly is a more reliable way of clearing things up, rather
// than relying on the template builder's fuzzy facsimile of the contents
// of the datasource. The template builder filters out bad entries, which would
// remain and accumulate if we did not happen to remove them here. 
NS_IMETHODIMP
nsDownloadManager::CleanUp()
{
  nsCOMPtr<nsIRDFResource> downloadRes;
  nsCOMPtr<nsIRDFInt> intLiteral;
  nsCOMPtr<nsISimpleEnumerator> downloads;

  // Coalesce operations so that we don't write to disk for every removal, or
  // attempt to update the UI too much. 
  StartBatchUpdate();
  mDataSource->BeginUpdateBatch();

  // 1). First, clean out the usual suspects - downloads that are
  //     finished, failed or canceled. 
  DownloadState states[] = { nsIDownloadManager::DOWNLOAD_FINISHED,
                             nsIDownloadManager::DOWNLOAD_FAILED,
                             nsIDownloadManager::DOWNLOAD_CANCELED,
                             nsIXPInstallManagerUI::INSTALL_FINISHED };

  for (int i = 0; i < 4; ++i) {
    gRDFService->GetIntLiteral(states[i], getter_AddRefs(intLiteral));
    nsresult rv = mDataSource->GetSources(gNC_DownloadState, intLiteral, PR_TRUE, getter_AddRefs(downloads));
    if (NS_FAILED(rv)) return rv;
  
    PRBool hasMoreElements;
    downloads->HasMoreElements(&hasMoreElements);

    while (hasMoreElements) {
      downloads->GetNext(getter_AddRefs(downloadRes));
      
      // Use the internal method because we know what we're doing! (We hope!)
      RemoveDownload(downloadRes);
      
      downloads->HasMoreElements(&hasMoreElements);
    }
  }

  mDataSource->EndUpdateBatch();
  EndBatchUpdate();

  return NS_OK;
}

NS_IMETHODIMP
nsDownloadManager::GetCanCleanUp(PRBool* aResult)
{
  nsCOMPtr<nsIRDFResource> downloadRes;
  nsCOMPtr<nsIRDFInt> intLiteral;

  *aResult = PR_FALSE;

  // 1). Downloads that can be cleaned up include those that are finished, 
  //     failed or canceled.
  DownloadState states[] = { nsIDownloadManager::DOWNLOAD_FINISHED,
                             nsIDownloadManager::DOWNLOAD_FAILED,
                             nsIDownloadManager::DOWNLOAD_CANCELED,
                             nsIXPInstallManagerUI::INSTALL_FINISHED };

  for (int i = 0; i < 4; ++i) {
    gRDFService->GetIntLiteral(states[i], getter_AddRefs(intLiteral));
    
    mDataSource->GetSource(gNC_DownloadState, intLiteral, PR_TRUE, getter_AddRefs(downloadRes));
    if (downloadRes) {
      *aResult = PR_TRUE;
      break;
    }
  }
  return NS_OK;
}

nsresult
nsDownloadManager::ValidateDownloadsContainer()
{
  // None of the function calls here should need error checking or their results
  // null checked because they should always succeed. If they fail, and there's 
  // a crash, it's a sign that this function is being called after the download 
  // manager or services that it rely on have been shut down, and there's a 
  // problem in some other code, somewhere.
  nsCOMPtr<nsIRDFContainer> downloads;
  GetDownloadsContainer(getter_AddRefs(downloads));

  nsCOMPtr<nsISimpleEnumerator> e;
  downloads->GetElements(getter_AddRefs(e));

  // This is our list of bad download entries. 
  nsCOMPtr<nsISupportsArray> ary;
  NS_NewISupportsArray(getter_AddRefs(ary));

  PRBool hasMore;
  e->HasMoreElements(&hasMore);
  nsCOMPtr<nsIRDFResource> downloadRes;
  while (hasMore) {
    e->GetNext(getter_AddRefs(downloadRes));

    PRBool hasProperty;

    // A valid download entry in the datasource will have at least the following
    // properties:
    // - NC:DownloadState -- the current state of the download, defined as
    //                       an integer value, see enumeration in 
    //                       nsDownloadManager.h, e.g. |0| (|DOWNLOADING|)
    // - NC:File          -- where the file is to be saved, e.g. 
    //                       "C:\\FirebirdSetup.exe"
    // - NC:Name          -- the visual display name of the download, e.g.
    //                       "FirebirdSetup.exe"

    nsIRDFResource* properties[] = { gNC_DownloadState, gNC_File, gNC_Name };
    for (PRInt32 i = 0; i < 3; ++i) {
      mDataSource->HasArcOut(downloadRes, properties[i], &hasProperty);
      if (!hasProperty) {
        ary->AppendElement(downloadRes);
        break;
      }
    }

    e->HasMoreElements(&hasMore);
  }

  // Coalesce notifications
  mDataSource->BeginUpdateBatch();

  // Now Remove all the bad downloads. 
  PRUint32 cnt;
  ary->Count(&cnt);
  for (PRInt32 i = 0; i < cnt; ++i) {
    nsCOMPtr<nsIRDFResource> download(do_QueryElementAt(ary, i));

    // Use the internal method because we know what we're doing! (We hope!)
    RemoveDownload(download);
  }

  mDataSource->EndUpdateBatch();

  return NS_OK;
}

NS_IMETHODIMP
nsDownloadManager::SetListener(nsIDownloadProgressListener* aListener)
{
  mListener = aListener;
  return NS_OK;
}

NS_IMETHODIMP
nsDownloadManager::GetListener(nsIDownloadProgressListener** aListener)
{
  *aListener = mListener;
  NS_IF_ADDREF(*aListener);
  return NS_OK;
}

NS_IMETHODIMP
nsDownloadManager::StartBatchUpdate()
{
  ++mBatches;
  return NS_OK;
}

NS_IMETHODIMP
nsDownloadManager::EndBatchUpdate()
{
  return --mBatches == 0 ? Flush() : NS_OK;
}

NS_IMETHODIMP
nsDownloadManager::PauseDownload(const PRUnichar* aPath)
{
  return PauseResumeDownload(aPath, PR_TRUE);
}

NS_IMETHODIMP
nsDownloadManager::ResumeDownload(const PRUnichar* aPath)
{
  return PauseResumeDownload(aPath, PR_FALSE);
}

nsresult
nsDownloadManager::PauseResumeDownload(const PRUnichar* aPath, PRBool aPause)
{
  nsresult rv;

  nsStringKey key(aPath);
  if (!mCurrDownloads.Exists(&key))
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIDownload> download;
  nsDownload* internalDownload = NS_STATIC_CAST(nsDownload*, mCurrDownloads.Get(&key));
  internalDownload->QueryInterface(NS_GET_IID(nsIDownload), (void**) getter_AddRefs(download));
  if (!download)
    return NS_ERROR_FAILURE;

  // Update download state in the DataSource
  nsCOMPtr<nsIRDFInt> intLiteral;

  gRDFService->GetIntLiteral(aPause ? nsIDownloadManager::DOWNLOAD_PAUSED : nsIDownloadManager::DOWNLOAD_DOWNLOADING, getter_AddRefs(intLiteral));

  nsCOMPtr<nsIRDFResource> res;
  gRDFService->GetUnicodeResource(nsDependentString(aPath), getter_AddRefs(res));

  nsCOMPtr<nsIRDFNode> oldTarget;
  mDataSource->GetTarget(res, gNC_DownloadState, PR_TRUE, getter_AddRefs(oldTarget));

  if (oldTarget) {
    rv = mDataSource->Change(res, gNC_DownloadState, oldTarget, intLiteral);
    if (NS_FAILED(rv)) return rv;
  }
  else {
    rv = mDataSource->Assert(res, gNC_DownloadState, intLiteral, PR_TRUE);
    if (NS_FAILED(rv)) return rv;
  }

  // Pause the actual download
  internalDownload->Pause(aPause);

  return NS_OK;
}

NS_IMETHODIMP
nsDownloadManager::Flush()
{
  // Before writing, always be sure what we're about to write is good data.
  ValidateDownloadsContainer();

  nsCOMPtr<nsIRDFRemoteDataSource> remote(do_QueryInterface(mDataSource));
  return remote->Flush();
}

NS_IMETHODIMP
nsDownloadManager::GetDatasource(nsIRDFDataSource** aDatasource)
{
  *aDatasource = mDataSource;
  NS_IF_ADDREF(*aDatasource);
  return NS_OK;
}
  
NS_IMETHODIMP
nsDownloadManager::Open(nsIDOMWindow* aParent, const PRUnichar* aPath)
{
  // 1). Retrieve the download object for the supplied path. 
  nsStringKey key(aPath);
  if (!mCurrDownloads.Exists(&key))
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIDownload> download;
  nsDownload* internalDownload = NS_STATIC_CAST(nsDownload*, mCurrDownloads.Get(&key));
  internalDownload->QueryInterface(NS_GET_IID(nsIDownload), (void**) getter_AddRefs(download));
  if (!download)
    return NS_ERROR_FAILURE;

  // 2). Update the DataSource. 
  AssertProgressInfoFor(aPath);

  nsVoidArray* params = new nsVoidArray();
  if (!params)
    return NS_ERROR_OUT_OF_MEMORY;

  NS_IF_ADDREF(aParent);
  NS_ADDREF(internalDownload);

  params->AppendElement((void*)aParent);
  params->AppendElement((void*)internalDownload);

  PRInt32 delay = 0;
  nsCOMPtr<nsIPrefBranch> pref(do_GetService(NS_PREFSERVICE_CONTRACTID));
  if (pref)
    pref->GetIntPref(PREF_BDM_OPENDELAY, &delay);

  params->AppendElement((void*)&delay);

  // 3). Look for an existing Download Manager window, if we find one we just 
  //     tell it that a new download has begun (we don't focus, that's 
  //     annoying), otherwise we need to open the window. We do this on a timer 
  //     so that we can see if the download has already completed, if so, don't 
  //     bother opening the window. 
  mDMOpenTimer = do_CreateInstance("@mozilla.org/timer;1");
  return mDMOpenTimer->InitWithFuncCallback(OpenTimerCallback, 
                                       (void*)params, delay, 
                                       nsITimer::TYPE_ONE_SHOT);
}

void
nsDownloadManager::OpenTimerCallback(nsITimer* aTimer, void* aClosure)
{
  nsVoidArray* params = (nsVoidArray*)aClosure;
  nsIDOMWindow* parent = (nsIDOMWindow*)params->ElementAt(0);
  nsDownload* download = (nsDownload*)params->ElementAt(1);
  PRInt32 openDelay = *(PRInt32*)params->ElementAt(2);
  
  PRInt32 complete;
  download->GetPercentComplete(&complete);
  
  // We only show the download window if the download is taking more than a non-tiny
  // amount of time to complete. 
  if (!openDelay || complete < 100) {
    PRBool focusDM = PR_FALSE;
    PRBool showDM = PR_TRUE;
    PRInt32 flashCount = -1;
    nsCOMPtr<nsIPrefBranch> pref(do_GetService(NS_PREFSERVICE_CONTRACTID));
    if (pref) {
      pref->GetBoolPref(PREF_BDM_FOCUSWHENSTARTING, &focusDM);

      // We only flash the download manager if the user has the download manager show
      pref->GetBoolPref(PREF_BDM_SHOWWHENSTARTING, &showDM);
      if (showDM) 
        pref->GetIntPref(PREF_BDM_FLASHCOUNT, &flashCount);
      else
        flashCount = 0;
    }

    nsDownloadManager::OpenDownloadManager(focusDM, flashCount, download, parent);
  }

  NS_RELEASE(download);
  NS_IF_RELEASE(parent);

  delete params;
}

nsresult
nsDownloadManager::OpenDownloadManager(PRBool aShouldFocus, PRInt32 aFlashCount, nsIDownload* aDownload, nsIDOMWindow* aParent)
{
  nsresult rv = NS_OK;

  nsCOMPtr<nsIWindowMediator> wm = do_GetService(NS_WINDOWMEDIATOR_CONTRACTID, &rv);
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsIDOMWindowInternal> recentWindow;
  wm->GetMostRecentWindow(NS_LITERAL_STRING("Download:Manager").get(), getter_AddRefs(recentWindow));
  if (recentWindow) {
    nsCOMPtr<nsIObserverService> obsService = do_GetService("@mozilla.org/observer-service;1", &rv);
    if (NS_FAILED(rv)) return rv;
    
    if (aShouldFocus)
      recentWindow->Focus();
    else {
      nsCOMPtr<nsIDOMChromeWindow> chromeWindow(do_QueryInterface(recentWindow));
      chromeWindow->GetAttentionWithCycleCount(aFlashCount);
    }
  }
  else {
    // If we ever have the capability to display the UI of third party dl managers,
    // we'll open their UI here instead.
    nsCOMPtr<nsIWindowWatcher> ww = do_GetService(NS_WINDOWWATCHER_CONTRACTID, &rv);
    if (NS_FAILED(rv)) return rv;

    // pass the datasource to the window
    nsCOMPtr<nsISupportsArray> params;
    NS_NewISupportsArray(getter_AddRefs(params));

    // I love static members. 
    nsCOMPtr<nsIDownloadManager> dlMgr(do_GetService("@mozilla.org/download-manager;1"));
    nsCOMPtr<nsIRDFDataSource> ds;
    dlMgr->GetDatasource(getter_AddRefs(ds));

    params->AppendElement(ds);
    params->AppendElement(aDownload);
    
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

///////////////////////////////////////////////////////////////////////////////
// nsIObserver

NS_IMETHODIMP
nsDownloadManager::Observe(nsISupports* aSubject, const char* aTopic, const PRUnichar* aData)
{
  nsresult rv;
  PRInt32 currDownloadCount = 0;

  if (nsCRT::strcmp(aTopic, "oncancel") == 0) {
    nsCOMPtr<nsIProgressDialog> dialog = do_QueryInterface(aSubject);
    nsCOMPtr<nsIURI> target;
    dialog->GetTarget(getter_AddRefs(target));

    nsAutoString path;
    rv = GetFilePathFromURI(target, path);
    if (NS_FAILED(rv)) return rv;
    
    nsStringKey key(path);
    if (mCurrDownloads.Exists(&key)) {
      // unset dialog since it's closing
      nsDownload* download = NS_STATIC_CAST(nsDownload*, mCurrDownloads.Get(&key));
      download->SetDialog(nsnull);
      
      return CancelDownload(path.get());  
    }
  }
  else if (nsCRT::strcmp(aTopic, "quit-application") == 0) {
    gStoppingDownloads = PR_TRUE;
    if (mCurrDownloads.Count()) {
      mCurrDownloads.Enumerate(CancelAllDownloads, this);

      // Download Manager is shutting down! Tell the XPInstallManager to stop
      // transferring any files that may have been being downloaded. 
      gObserverService->NotifyObservers(mXPIProgress, "xpinstall-progress", NS_LITERAL_STRING("cancel").get());

      // Now go and update the datasource so that we "cancel" all paused downloads. 
      SaveState();
    }

    // Now that active downloads have been canceled, remove all downloads if 
    // the user's retention policy specifies it. 
    if (GetRetentionBehavior() == 1) {
      nsCOMPtr<nsIRDFContainer> ctr;
      GetDownloadsContainer(getter_AddRefs(ctr));

      StartBatchUpdate();

      nsCOMPtr<nsISupportsArray> ary;
      NS_NewISupportsArray(getter_AddRefs(ary));

      if (ary) {
        nsCOMPtr<nsISimpleEnumerator> e;
        ctr->GetElements(getter_AddRefs(e));

        PRBool hasMore;
        e->HasMoreElements(&hasMore);
        while(hasMore) {
          nsCOMPtr<nsIRDFResource> curr;
          e->GetNext(getter_AddRefs(curr));
          
          ary->AppendElement(curr);
          
          e->HasMoreElements(&hasMore);
        }

        // Now Remove all the downloads. 
        PRUint32 cnt;
        ary->Count(&cnt);
        for (PRInt32 i = 0; i < cnt; ++i) {
          nsCOMPtr<nsIRDFResource> download(do_QueryElementAt(ary, i));
          // Here we use the internal RemoveDownload method, and only here
          // because this is _after_ the download table |mCurrDownloads| has been 
          // cleared of active downloads, so we know we're not accidentally
          // clearing active downloads. 
          RemoveDownload(download);
        }
      }

      EndBatchUpdate();
    }
  }
  else if (nsCRT::strcmp(aTopic, "quit-application-requested") == 0 && 
           (currDownloadCount = mCurrDownloads.Count())) {
    nsCOMPtr<nsISupportsPRBool> cancelDownloads(do_QueryInterface(aSubject));
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
  }
  else if (nsCRT::strcmp(aTopic, "offline-requested") == 0 && 
           (currDownloadCount = mCurrDownloads.Count())) {
    nsCOMPtr<nsISupportsPRBool> cancelDownloads(do_QueryInterface(aSubject));
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
      gObserverService->NotifyObservers(mXPIProgress, "xpinstall-progress", NS_LITERAL_STRING("cancel").get());
    
      mCurrDownloads.Enumerate(CancelAllDownloads, this);
      gStoppingDownloads = PR_FALSE;
    }
  }
  return NS_OK;
}

void
nsDownloadManager::ConfirmCancelDownloads(PRInt32 aCount, nsISupportsPRBool* aCancelDownloads,
                                          const PRUnichar* aTitle, 
                                          const PRUnichar* aCancelMessageMultiple, 
                                          const PRUnichar* aCancelMessageSingle,
                                          const PRUnichar* aDontCancelButton)
{
  nsXPIDLString title, message, quitButton, dontQuitButton;
  
  nsCOMPtr<nsIStringBundleService> bundleService = do_GetService(kStringBundleServiceCID);
  nsCOMPtr<nsIStringBundle> bundle;
  if (bundleService)
    bundleService->CreateBundle(DOWNLOAD_MANAGER_BUNDLE, getter_AddRefs(bundle));
  if (bundle) {
    bundle->GetStringFromName(aTitle, getter_Copies(title));    

    nsAutoString countString;
    countString.AppendInt(aCount);
    const PRUnichar* strings[1] = { countString.get() };
    if (aCount > 1) {
      bundle->FormatStringFromName(aCancelMessageMultiple, strings, 1, getter_Copies(message));
      bundle->FormatStringFromName(NS_LITERAL_STRING("cancelDownloadsOKTextMultiple").get(), strings, 1, getter_Copies(quitButton));
    }
    else {
      bundle->GetStringFromName(aCancelMessageSingle, getter_Copies(message));
      bundle->GetStringFromName(NS_LITERAL_STRING("cancelDownloadsOKText").get(), getter_Copies(quitButton));
    }

    bundle->GetStringFromName(aDontCancelButton, getter_Copies(dontQuitButton));
  }

  // Get Download Manager window, to be parent of alert.
  nsCOMPtr<nsIWindowMediator> wm = do_GetService(NS_WINDOWMEDIATOR_CONTRACTID);
  nsCOMPtr<nsIDOMWindowInternal> dmWindow;
  if (wm)
    wm->GetMostRecentWindow(NS_LITERAL_STRING("Download:Manager").get(), getter_AddRefs(dmWindow));

  // Show alert.
  nsCOMPtr<nsIPromptService> prompter(do_GetService("@mozilla.org/embedcomp/prompt-service;1"));
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

#ifdef XP_WIN 
///////////////////////////////////////////////////////////////////////////////
// nsIAlertListener
NS_IMETHODIMP
nsDownloadManager::OnAlertFinished(const PRUnichar* aAlertCookie)
{
  // Nothing to do here. 
  return NS_OK;
}

NS_IMETHODIMP
nsDownloadManager::OnAlertClickCallback(const PRUnichar* aAlertCookie)
{
  nsresult rv;

  // Attempt to locate a browser window to parent the download manager to
  nsCOMPtr<nsIWindowMediator> wm = do_GetService(NS_WINDOWMEDIATOR_CONTRACTID, &rv);
  nsCOMPtr<nsIDOMWindowInternal> browserWindow;
  if (wm)
    wm->GetMostRecentWindow(NS_LITERAL_STRING("navigator:browser").get(), getter_AddRefs(browserWindow));

  return OpenDownloadManager(PR_TRUE, -1, nsnull, browserWindow);
}
#endif

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

  mDownloadManager = nsnull;
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

  switch (aState) {
  case nsIXPIProgressDialog::DOWNLOAD_START:
    wpl->OnStateChange(nsnull, nsnull, nsIWebProgressListener::STATE_START, 0);

    dl->SetDownloadState(nsIXPInstallManagerUI::INSTALL_DOWNLOADING);
    AssertProgressInfoForDownload(dl);
    break;
  case nsIXPIProgressDialog::DOWNLOAD_DONE:
    break;
  case nsIXPIProgressDialog::INSTALL_START:
    dl->SetDownloadState(nsIXPInstallManagerUI::INSTALL_INSTALLING);
    AssertProgressInfoForDownload(dl);
    break;
  case nsIXPIProgressDialog::INSTALL_DONE:
    wpl->OnStateChange(nsnull, nsnull, nsIWebProgressListener::STATE_STOP, 0);
    
    dl->SetDownloadState(nsIXPInstallManagerUI::INSTALL_FINISHED);
    AssertProgressInfoForDownload(dl);
    
    // Now, remove it from our internal bookkeeping list. 
    RemoveDownloadAtIndex(aIndex);
    break;
  case nsIXPIProgressDialog::DIALOG_CLOSE:
    // Close now, if we're allowed to. 
    gObserverService->NotifyObservers(nsnull, "xpinstall-dialog-close", nsnull);

    if (!gStoppingDownloads) {
      nsCOMPtr<nsIStringBundleService> sbs(do_GetService("@mozilla.org/intl/stringbundle;1"));
      nsCOMPtr<nsIStringBundle> brandBundle, xpinstallBundle;
      sbs->CreateBundle("chrome://global/locale/brand.properties", getter_AddRefs(brandBundle));
      sbs->CreateBundle("chrome://mozapps/locale/xpinstall/xpinstallConfirm.properties", getter_AddRefs(xpinstallBundle));

      nsXPIDLString brandShortName, message, title;
      brandBundle->GetStringFromName(NS_LITERAL_STRING("brandShortName").get(), getter_Copies(brandShortName));
      const PRUnichar* strings[1] = { brandShortName.get() };
      xpinstallBundle->FormatStringFromName(NS_LITERAL_STRING("installComplete").get(), strings, 1, getter_Copies(message));
      xpinstallBundle->GetStringFromName(NS_LITERAL_STRING("installCompleteTitle").get(), getter_Copies(title));

      nsCOMPtr<nsIPromptService> ps(do_GetService("@mozilla.org/embedcomp/prompt-service;1"));
      ps->Alert(nsnull, title, message);
    }

    break;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsXPIProgressListener::OnProgress(PRUint32 aIndex, PRUint32 aValue, PRUint32 aMaxValue)
{
  nsCOMPtr<nsIWebProgressListener> wpl(do_QueryElementAt(mDownloads, aIndex));
  if (wpl) 
    return wpl->OnProgressChange(nsnull, nsnull, 0, 0, aValue, aMaxValue);
  return NS_OK;
}

void 
nsXPIProgressListener::AssertProgressInfoForDownload(nsDownload* aDownload)
{
  nsCOMPtr<nsIURI> target;
  aDownload->GetTarget(getter_AddRefs(target));

  nsAutoString path;
  GetFilePathFromURI(target, path);

  mDownloadManager->AssertProgressInfoFor(path.get());
}

///////////////////////////////////////////////////////////////////////////////
// nsDownloadsDataSource
//   XXXben I'm making this datasource proxy its own object now and passing 
//          IconURL requests through it so that the framework is in place by
//          0.9 when the time comes to upgrade to the Magical World of Mork.
//          
//          Eventually I want to abstract away almost all direct dealings with 
//          this datasource into functions on this object, to simplify the 
//          code in the download manager service and front end.

NS_IMPL_ISUPPORTS2(nsDownloadsDataSource, nsIRDFDataSource, nsIRDFRemoteDataSource)

nsresult
nsDownloadsDataSource::LoadDataSource()
{
  nsCOMPtr<nsIFile> downloadsFile;
  NS_GetSpecialDirectory(NS_APP_DOWNLOADS_50_FILE, getter_AddRefs(downloadsFile));
    
  nsCAutoString downloadsDB;
  NS_GetURLSpecFromFile(downloadsFile, downloadsDB);

  return gRDFService->GetDataSourceBlocking(downloadsDB.get(), getter_AddRefs(mInner));
}

///////////////////////////////////////////////////////////////////////////////
// nsIRDFDataSource
/* readonly attribute string URI; */
NS_IMETHODIMP 
nsDownloadsDataSource::GetURI(char** aURI)
{
  return mInner->GetURI(aURI);
}

NS_IMETHODIMP 
nsDownloadsDataSource::GetSource(nsIRDFResource* aProperty, nsIRDFNode* aTarget, PRBool aTruthValue, nsIRDFResource** aResult)
{
  return mInner->GetSource(aProperty, aTarget, aTruthValue, aResult);
}

NS_IMETHODIMP 
nsDownloadsDataSource::GetSources(nsIRDFResource* aProperty, nsIRDFNode* aTarget, PRBool aTruthValue, nsISimpleEnumerator** aResult)
{
  return mInner->GetSources(aProperty, aTarget, aTruthValue, aResult);
}

NS_IMETHODIMP 
nsDownloadsDataSource::GetTarget(nsIRDFResource* aSource, nsIRDFResource* aProperty, PRBool aTruthValue, nsIRDFNode** aResult)
{
  if (aProperty == gNC_IconURL) {
    PRBool hasIconURLArc;
    nsresult rv = mInner->HasArcOut(aSource, aProperty, &hasIconURLArc);
    if (NS_FAILED(rv)) return rv;

    // If the download entry doesn't have a IconURL property, use the 
    // File property instead, so that moz-icon works. 
    if (!hasIconURLArc) {
      nsCOMPtr<nsIRDFNode> target;
      rv = mInner->GetTarget(aSource, gNC_File, aTruthValue, getter_AddRefs(target));
      if (NS_SUCCEEDED(rv) && target) {
        nsXPIDLCString path;
        nsCOMPtr<nsIRDFResource> res(do_QueryInterface(target));
        res->GetValue(getter_Copies(path));

        nsCOMPtr<nsILocalFile> lf(do_CreateInstance("@mozilla.org/file/local;1"));
        lf->InitWithNativePath(path);
        nsCOMPtr<nsIIOService> ios(do_GetService("@mozilla.org/network/io-service;1"));
        nsCOMPtr<nsIProtocolHandler> ph;
        ios->GetProtocolHandler("file", getter_AddRefs(ph));
        nsCOMPtr<nsIFileProtocolHandler> fph(do_QueryInterface(ph));

        nsCAutoString fileURL;
        fph->GetURLSpecFromFile(lf, fileURL);
        
        nsAutoString iconURL(NS_LITERAL_STRING("moz-icon://"));
        AppendUTF8toUTF16(fileURL, iconURL);
        iconURL.AppendLiteral("?size=32");

        nsCOMPtr<nsIRDFResource> result;
        gRDFService->GetUnicodeResource(iconURL, getter_AddRefs(result));

        *aResult = result;
        NS_IF_ADDREF(*aResult);

        return NS_OK;
      }
    }
  }
  // Either it's some other property, or we DO have an IconURL property
  // and we just need to get the value from the real datasource. 
  return mInner->GetTarget(aSource, aProperty, aTruthValue, aResult);
}

NS_IMETHODIMP 
nsDownloadsDataSource::GetTargets(nsIRDFResource* aSource, nsIRDFResource* aProperty, PRBool aTruthValue, nsISimpleEnumerator** aResult)
{
  if (aProperty == gNC_IconURL) {
    nsCOMPtr<nsIRDFNode> target;
    nsresult rv = GetTarget(aSource, aProperty, aTruthValue, getter_AddRefs(target));
    if (NS_FAILED(rv)) return rv;

    if (NS_SUCCEEDED(rv)) 
      return NS_NewSingletonEnumerator(aResult, target);
  }
  return mInner->GetTargets(aSource, aProperty, aTruthValue, aResult);
}

NS_IMETHODIMP 
nsDownloadsDataSource::Assert(nsIRDFResource* aSource, nsIRDFResource* aProperty, nsIRDFNode* aTarget, PRBool aTruthValue)
{
  return mInner->Assert(aSource, aProperty, aTarget, aTruthValue);
}

NS_IMETHODIMP 
nsDownloadsDataSource::Unassert(nsIRDFResource* aSource, nsIRDFResource* aProperty, nsIRDFNode* aTarget)
{
  return mInner->Unassert(aSource, aProperty, aTarget);
}

NS_IMETHODIMP 
nsDownloadsDataSource::Change(nsIRDFResource* aSource, nsIRDFResource* aProperty, nsIRDFNode* aOldTarget, nsIRDFNode* aNewTarget)
{
  return mInner->Change(aSource, aProperty, aOldTarget, aNewTarget);
}

NS_IMETHODIMP 
nsDownloadsDataSource::Move(nsIRDFResource* aOldSource, nsIRDFResource* aNewSource, nsIRDFResource* aProperty, nsIRDFNode* aTarget)
{
  return mInner->Move(aOldSource, aNewSource, aProperty, aTarget);
}

NS_IMETHODIMP 
nsDownloadsDataSource::HasAssertion(nsIRDFResource* aSource, nsIRDFResource* aProperty, nsIRDFNode* aTarget, PRBool aTruthValue, PRBool* aResult)
{
  return mInner->HasAssertion(aSource, aProperty, aTarget, aTruthValue, aResult);
}

NS_IMETHODIMP 
nsDownloadsDataSource::AddObserver(nsIRDFObserver* aObserver)
{
  return mInner->AddObserver(aObserver);
}

NS_IMETHODIMP 
nsDownloadsDataSource::RemoveObserver(nsIRDFObserver* aObserver)
{
  return mInner->RemoveObserver(aObserver);
}

NS_IMETHODIMP 
nsDownloadsDataSource::ArcLabelsIn(nsIRDFNode* aNode, nsISimpleEnumerator** aResult)
{
  return mInner->ArcLabelsIn(aNode, aResult);
}

NS_IMETHODIMP 
nsDownloadsDataSource::ArcLabelsOut(nsIRDFResource* aSource, nsISimpleEnumerator** aResult)
{
  return mInner->ArcLabelsOut(aSource, aResult);
}

NS_IMETHODIMP 
nsDownloadsDataSource::GetAllResources(nsISimpleEnumerator** aResult)
{
  return mInner->GetAllResources(aResult);
}

NS_IMETHODIMP 
nsDownloadsDataSource::IsCommandEnabled(nsISupportsArray* aSources, nsIRDFResource* aCommand, nsISupportsArray* aArguments, PRBool* aResult)
{
  return mInner->IsCommandEnabled(aSources, aCommand, aArguments, aResult);
}

NS_IMETHODIMP 
nsDownloadsDataSource::DoCommand(nsISupportsArray* aSources, nsIRDFResource* aCommand, nsISupportsArray* aArguments)
{
  return mInner->DoCommand(aSources, aCommand, aArguments);
}

NS_IMETHODIMP 
nsDownloadsDataSource::GetAllCmds(nsIRDFResource* aSource, nsISimpleEnumerator** aResult)
{
  return mInner->GetAllCmds(aSource, aResult);
}

NS_IMETHODIMP 
nsDownloadsDataSource::HasArcIn(nsIRDFNode* aNode, nsIRDFResource* aArc, PRBool* aResult)
{
  return mInner->HasArcIn(aNode, aArc, aResult);
}

NS_IMETHODIMP 
nsDownloadsDataSource::HasArcOut(nsIRDFResource* aSource, nsIRDFResource* aArc, PRBool* aResult)
{
  return mInner->HasArcOut(aSource, aArc, aResult);
}

NS_IMETHODIMP 
nsDownloadsDataSource::BeginUpdateBatch()
{
  return mInner->BeginUpdateBatch();
}

NS_IMETHODIMP 
nsDownloadsDataSource::EndUpdateBatch()
{
  return mInner->EndUpdateBatch();
}

///////////////////////////////////////////////////////////////////////////////
// nsIRDFRemoteDataSource

NS_IMETHODIMP
nsDownloadsDataSource::GetLoaded(PRBool* aResult)
{
  nsCOMPtr<nsIRDFRemoteDataSource> rds(do_QueryInterface(mInner));
  return rds->GetLoaded(aResult);
}

NS_IMETHODIMP
nsDownloadsDataSource::Init(const char* aURI)
{
  nsCOMPtr<nsIRDFRemoteDataSource> rds(do_QueryInterface(mInner));
  return rds->Init(aURI);
}

NS_IMETHODIMP
nsDownloadsDataSource::Refresh(PRBool aBlocking)
{
  nsCOMPtr<nsIRDFRemoteDataSource> rds(do_QueryInterface(mInner));
  return rds->Refresh(aBlocking);
}

NS_IMETHODIMP
nsDownloadsDataSource::Flush()
{
  nsCOMPtr<nsIRDFRemoteDataSource> rds(do_QueryInterface(mInner));
  return rds->Flush();
}

NS_IMETHODIMP
nsDownloadsDataSource::FlushTo(const char* aURI)
{
  nsCOMPtr<nsIRDFRemoteDataSource> rds(do_QueryInterface(mInner));
  return rds->FlushTo(aURI);
}

///////////////////////////////////////////////////////////////////////////////
// nsDownload

NS_IMPL_ISUPPORTS3(nsDownload, nsIDownload, nsITransfer, nsIWebProgressListener)

nsDownload::nsDownload():mDownloadState(nsIDownloadManager::DOWNLOAD_NOTSTARTED),
                         mPercentComplete(0),
                         mCurrBytes(0),
                         mMaxBytes(0),
                         mStartTime(0),
                         mLastUpdate(-500)
{
}

nsDownload::~nsDownload()
{  
}

nsresult
nsDownload::SetDownloadManager(nsDownloadManager* aDownloadManager)
{
  mDownloadManager = aDownloadManager;
  return NS_OK;
}

nsresult
nsDownload::SetDialogListener(nsIWebProgressListener* aDialogListener)
{
  mDialogListener = aDialogListener;
  return NS_OK;
}

nsresult
nsDownload::GetDialogListener(nsIWebProgressListener** aDialogListener)
{
  *aDialogListener = mDialogListener;
  NS_IF_ADDREF(*aDialogListener);
  return NS_OK;
}

nsresult
nsDownload::SetDialog(nsIProgressDialog* aDialog)
{
  mDialog = aDialog;
  return NS_OK;
}

nsresult
nsDownload::GetDialog(nsIProgressDialog** aDialog)
{
  *aDialog = mDialog;
  NS_IF_ADDREF(*aDialog);
  return NS_OK;
}

DownloadState
nsDownload::GetDownloadState()
{
  return mDownloadState;
}

void
nsDownload::SetDownloadState(DownloadState aState)
{
  mDownloadState = aState;
}

DownloadType
nsDownload::GetDownloadType()
{
  return mDownloadType;
}

void
nsDownload::SetDownloadType(DownloadType aType)
{
  mDownloadType = aType;
}

nsresult
nsDownload::SetPersist(nsIWebBrowserPersist* aPersist)
{
  mPersist = aPersist;
  return NS_OK;
}

nsresult
nsDownload::SetSource(nsIURI* aSource)
{
  mSource = aSource;
  return NS_OK;
}

nsresult
nsDownload::SetTarget(nsIURI* aTarget)
{
  mTarget = aTarget;
  return NS_OK;
}

nsresult
nsDownload::GetTransferInformation(PRInt32* aCurr, PRInt32* aMax)
{
  *aCurr = mCurrBytes;
  *aMax = mMaxBytes;
  return NS_OK;
}

nsresult
nsDownload::SetStartTime(PRInt64 aStartTime)
{
  mStartTime = aStartTime;
  return NS_OK;
}

nsresult
nsDownload::SetMIMEInfo(nsIMIMEInfo *aMIMEInfo)
{
  mMIMEInfo = aMIMEInfo;
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

  if (!mRequest)
    mRequest = aRequest; // used for pause/resume

  // filter notifications since they come in so frequently
  PRTime delta;
  PRTime now = PR_Now();
  LL_SUB(delta, now, mLastUpdate);
  if (LL_CMP(delta, <, INTERVAL) &&  aMaxTotalProgress != -1 && aCurTotalProgress < aMaxTotalProgress)
    return NS_OK;

  mLastUpdate = now;

  if (mDownloadState == nsIDownloadManager::DOWNLOAD_NOTSTARTED) {
    nsAutoString path;
    nsresult rv = GetFilePathFromURI(mTarget, path);
    if (NS_FAILED(rv)) return rv;

    mDownloadState = nsIDownloadManager::DOWNLOAD_DOWNLOADING;
    mDownloadManager->DownloadStarted(path.get());
  }

  if (aMaxTotalProgress > 0)
    mPercentComplete = (PRInt32)((PRFloat64)aCurTotalProgress * 100 / aMaxTotalProgress + .5);
  else
    mPercentComplete = -1;

  mCurrBytes = (PRInt32)((PRFloat64)aCurTotalProgress / 1024.0 + .5);
  mMaxBytes = (PRInt32)((PRFloat64)aMaxTotalProgress / 1024 + .5);

  if (mDownloadManager->NeedsUIUpdate()) {
    nsCOMPtr<nsIDownloadProgressListener> dpl;
    mDownloadManager->GetInternalListener(getter_AddRefs(dpl));
    if (dpl) {
      dpl->OnProgressChange(aWebProgress, aRequest, aCurSelfProgress, aMaxSelfProgress,
                            aCurTotalProgress, aMaxTotalProgress, this);
    }
  }

  return NS_OK;
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
    mDownloadState = nsIDownloadManager::DOWNLOAD_FAILED;
    nsAutoString path;
    nsresult rv = GetFilePathFromURI(mTarget, path);
    if (NS_SUCCEEDED(rv)) {
      mDownloadManager->DownloadEnded(path.get(), nsnull);
      gObserverService->NotifyObservers(NS_STATIC_CAST(nsIDownload *, this), "dl-failed", nsnull);                     
    }

    // Get title for alert.
    nsXPIDLString title;
    
    nsCOMPtr<nsIStringBundleService> bundleService = do_GetService(kStringBundleServiceCID, &rv);
    nsCOMPtr<nsIStringBundle> bundle;
    if (bundleService)
      rv = bundleService->CreateBundle(DOWNLOAD_MANAGER_BUNDLE, getter_AddRefs(bundle));
    if (bundle)
      bundle->GetStringFromName(NS_LITERAL_STRING("downloadErrorAlertTitle").get(), getter_Copies(title));    

    // Get Download Manager window, to be parent of alert.
    nsCOMPtr<nsIWindowMediator> wm = do_GetService(NS_WINDOWMEDIATOR_CONTRACTID, &rv);
    nsCOMPtr<nsIDOMWindowInternal> dmWindow;
    if (wm)
      wm->GetMostRecentWindow(NS_LITERAL_STRING("Download:Manager").get(), getter_AddRefs(dmWindow));

    // Show alert.
    nsCOMPtr<nsIPromptService> prompter(do_GetService("@mozilla.org/embedcomp/prompt-service;1"));
    if (prompter)
      prompter->Alert(dmWindow, title, aMessage);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDownload::OnStateChange(nsIWebProgress* aWebProgress,
                          nsIRequest* aRequest, PRUint32 aStateFlags,
                          nsresult aStatus)
{
  if (aStateFlags & STATE_START)    
    mStartTime = PR_Now();  

  // When we break the ref cycle with mPersist, we don't want to lose
  // access to out member vars!
  nsCOMPtr<nsIDownload> kungFuDeathGrip;
  CallQueryInterface(this, NS_STATIC_CAST(nsIDownload**,
                                          getter_AddRefs(kungFuDeathGrip)));

  // We need to update mDownloadState before updating the dialog, because
  // that will close and call CancelDownload if it was the last open window.
  nsresult rv = NS_OK;
  if (aStateFlags & STATE_STOP) {
    if (nsDownloadManager::IsInFinalStage(mDownloadState)) {
      if (mDownloadState != nsIXPInstallManagerUI::INSTALL_INSTALLING)
        mDownloadState = nsIDownloadManager::DOWNLOAD_FINISHED;
      else
        mDownloadState = nsIXPInstallManagerUI::INSTALL_FINISHED;

      // Files less than 1Kb shouldn't show up as 0Kb.
      if (mMaxBytes==0)
        mMaxBytes = 1;
      mCurrBytes = mMaxBytes;
      mPercentComplete = 100;

      nsAutoString path;
      rv = GetFilePathFromURI(mTarget, path);
      // can't do an early return; have to break reference cycle below
      if (NS_SUCCEEDED(rv)) {
        mDownloadManager->DownloadEnded(path.get(), nsnull);
      }

#ifdef XP_WIN
      // Master pref to control this function. 
      PRBool showTaskbarAlert = PR_FALSE;
      nsCOMPtr<nsIPrefBranch> pref(do_GetService(NS_PREFSERVICE_CONTRACTID));
      if (pref)
        pref->GetBoolPref(PREF_BDM_SHOWALERTONCOMPLETE, &showTaskbarAlert);

      if (showTaskbarAlert) {
        PRInt32 alertInterval = -1;
        if (pref)
          pref->GetIntPref(PREF_BDM_SHOWALERTINTERVAL, &alertInterval);

        PRInt64 temp, uSecPerMSec, alertIntervalUSec;
        LL_I2L(temp, alertInterval);
        LL_I2L(uSecPerMSec, PR_USEC_PER_MSEC);
        LL_MUL(alertIntervalUSec, temp, uSecPerMSec);
        
        PRInt64 goat = PR_Now() - mStartTime;
        showTaskbarAlert = goat > alertIntervalUSec;
        
        if (showTaskbarAlert && (mDownloadManager->mCurrDownloads).Count() == 0) {
          nsCOMPtr<nsIAlertsService> alerts(do_GetService("@mozilla.org/alerts-service;1"));
          if (alerts) {
            nsXPIDLString title, message;

            mDownloadManager->mBundle->GetStringFromName(NS_LITERAL_STRING("downloadsCompleteTitle").get(), getter_Copies(title));
            mDownloadManager->mBundle->GetStringFromName(NS_LITERAL_STRING("downloadsCompleteMsg").get(), getter_Copies(message));

            PRBool removeWhenDone = mDownloadManager->GetRetentionBehavior() == 0;


            // If downloads are automatically removed per the user's retention policy, 
            // there's no reason to make the text clickable because if it is, they'll
            // click open the download manager and the items they downloaded will have
            // been removed. 
            alerts->ShowAlertNotification(DOWNLOAD_MANAGER_ALERT_ICON, title, message, !removeWhenDone, 
                                          EmptyString().get(), mDownloadManager);
          }
        }
      }
#endif
    }

    gObserverService->NotifyObservers(NS_STATIC_CAST(nsIDownload *, this), "dl-done", nsnull);

    // break the cycle we created in AddDownload
    if (mPersist)
      mPersist->SetProgressListener(nsnull);

    // Now remove the download if the user's retention policy is "Remove when Done"
    if (mDownloadManager->GetRetentionBehavior() == 0) {
      nsAutoString path;
      GetFilePathFromURI(mTarget, path);

      mDownloadManager->RemoveDownload(path.get());
    }
  }

  if (mDownloadManager->NeedsUIUpdate()) {
    nsCOMPtr<nsIDownloadProgressListener> dpl;
    mDownloadManager->GetInternalListener(getter_AddRefs(dpl));
    if (dpl) {
      dpl->OnStateChange(aWebProgress, aRequest, aStateFlags, aStatus, this);
    }
  }

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
                 const PRUnichar* aDisplayName,
                 nsIMIMEInfo *aMIMEInfo,
                 PRInt64 aStartTime,
                 nsIWebBrowserPersist* aPersist)
{
  NS_WARNING("Huh...how did we get here?!");
  return NS_OK;
}

NS_IMETHODIMP
nsDownload::SetDisplayName(const PRUnichar* aDisplayName)
{
  mDisplayName = aDisplayName;

  nsCOMPtr<nsIRDFDataSource> ds;
  mDownloadManager->GetDatasource(getter_AddRefs(ds));

  nsCOMPtr<nsIRDFLiteral> nameLiteral;
  nsCOMPtr<nsIRDFResource> res;
  nsAutoString path;
  nsresult rv = GetFilePathFromURI(mTarget, path);
  if (NS_FAILED(rv)) return rv;

  gRDFService->GetUnicodeResource(path, getter_AddRefs(res));
  
  gRDFService->GetLiteral(aDisplayName, getter_AddRefs(nameLiteral));
  ds->Assert(res, gNC_Name, nameLiteral, PR_TRUE);

  return NS_OK;
}

NS_IMETHODIMP
nsDownload::GetDisplayName(PRUnichar** aDisplayName)
{
  *aDisplayName = ToNewUnicode(mDisplayName);
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
nsDownload::GetPersist(nsIWebBrowserPersist** aPersist)
{
  *aPersist = mPersist;
  NS_IF_ADDREF(*aPersist);
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
nsDownload::SetListener(nsIWebProgressListener* aListener)
{
  return NS_OK;
}

NS_IMETHODIMP
nsDownload::GetListener(nsIWebProgressListener** aListener)
{
  *aListener = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
nsDownload::SetObserver(nsIObserver* aObserver)
{
  mObserver = aObserver;
  return NS_OK;
}

NS_IMETHODIMP
nsDownload::GetObserver(nsIObserver** aObserver)
{
  *aObserver = mObserver;
  NS_IF_ADDREF(*aObserver);
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

void
nsDownload::Pause(PRBool aPaused)
{
  if (mPaused != aPaused) {
    if (mRequest) {
      if (aPaused) {
        mRequest->Suspend();
        mDownloadState = nsIDownloadManager::DOWNLOAD_PAUSED;
      }
      else {
        mRequest->Resume();
        mDownloadState = nsIDownloadManager::DOWNLOAD_DOWNLOADING;
      }
    }
  }
}

PRBool
nsDownload::IsPaused()
{
  return mPaused;
}

