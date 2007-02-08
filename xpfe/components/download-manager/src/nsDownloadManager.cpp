/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cin: */
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
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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
#include "nsIRDFLiteral.h"
#include "rdf.h"
#include "nsNetUtil.h"
#include "nsIDOMWindow.h"
#include "nsIDOMWindowInternal.h"
#include "nsIDOMEvent.h"
#include "nsIDOMEventTarget.h"
#include "nsRDFCID.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsIObserver.h"
#include "nsIProgressDialog.h"
#include "nsIWebBrowserPersist.h"
#include "nsIWindowWatcher.h"
#include "nsIStringBundle.h"
#include "nsCRT.h"
#include "nsIWindowMediator.h"
#include "nsIPromptService.h"
#include "nsIObserverService.h"
#include "nsIProfileChangeStatus.h"
#include "nsIPrefService.h"
#include "nsIFileURL.h"
#include "nsIAlertsService.h"
#include "nsEmbedCID.h"
#include "nsInt64.h"
#ifdef MOZ_XUL_APP
#include "nsToolkitCompsCID.h" 
#endif

/* Outstanding issues/todo:
 * 1. Implement pause/resume.
 */
  
#define DOWNLOAD_MANAGER_FE_URL "chrome://communicator/content/downloadmanager/downloadmanager.xul"
#define DOWNLOAD_MANAGER_BUNDLE "chrome://communicator/locale/downloadmanager/downloadmanager.properties"

static const nsInt64 gInterval((PRUint32)(400 * PR_USEC_PER_MSEC));

static nsIRDFResource* gNC_DownloadsRoot = nsnull;
static nsIRDFResource* gNC_File = nsnull;
static nsIRDFResource* gNC_URL = nsnull;
static nsIRDFResource* gNC_Name = nsnull;
static nsIRDFResource* gNC_ProgressMode = nsnull;
static nsIRDFResource* gNC_ProgressPercent = nsnull;
static nsIRDFResource* gNC_Transferred = nsnull;
static nsIRDFResource* gNC_DownloadState = nsnull;
static nsIRDFResource* gNC_StatusText = nsnull;

static nsIRDFService* gRDFService = nsnull;

static PRInt32 gRefCnt = 0;

/**
 * This function extracts the local file path corresponding to the given URI.
 */
static nsresult
GetFilePathUTF8(nsIURI *aURI, nsACString &aResult)
{
  nsresult rv;

  nsCOMPtr<nsIFileURL> fileURL = do_QueryInterface(aURI, &rv);
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsIFile> file;
  rv = fileURL->GetFile(getter_AddRefs(file));
  if (NS_FAILED(rv)) return rv;

  nsAutoString path;
  rv = file->GetPath(path);
  if (NS_SUCCEEDED(rv))
    CopyUTF16toUTF8(path, aResult);
  return rv;
}

///////////////////////////////////////////////////////////////////////////////
// nsDownloadManager

NS_IMPL_ISUPPORTS3(nsDownloadManager, nsIDownloadManager, nsIDOMEventListener, nsIObserver)

nsDownloadManager::nsDownloadManager() : mBatches(0)
{
}

nsDownloadManager::~nsDownloadManager()
{
  if (--gRefCnt != 0 || !gRDFService)
    // Either somebody tried to use |CreateInstance| instead of
    // |GetService| or |Init| failed very early, so there's nothing to
    // do here.
    return;

  gRDFService->UnregisterDataSource(mDataSource);

  NS_IF_RELEASE(gNC_DownloadsRoot);                                             
  NS_IF_RELEASE(gNC_File);                                                      
  NS_IF_RELEASE(gNC_URL);                                                       
  NS_IF_RELEASE(gNC_Name);                                                      
  NS_IF_RELEASE(gNC_ProgressMode);
  NS_IF_RELEASE(gNC_ProgressPercent);
  NS_IF_RELEASE(gNC_Transferred);
  NS_IF_RELEASE(gNC_DownloadState);
  NS_IF_RELEASE(gNC_StatusText);

  NS_RELEASE(gRDFService);
}

nsresult
nsDownloadManager::Init()
{
  if (gRefCnt++ != 0) {
    NS_NOTREACHED("download manager should be used as a service");
    return NS_ERROR_UNEXPECTED; // This will make the |CreateInstance| fail.
  }

  if (!mCurrDownloads.Init())
    return NS_ERROR_FAILURE;

  nsresult rv;
  mRDFContainerUtils = do_GetService("@mozilla.org/rdf/container-utils;1", &rv);
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsIObserverService> obsService = do_GetService("@mozilla.org/observer-service;1", &rv);
  if (NS_FAILED(rv)) return rv;
  

  rv = CallGetService("@mozilla.org/rdf/rdf-service;1", &gRDFService);
  if (NS_FAILED(rv)) return rv;                                                 

  gRDFService->GetResource(NS_LITERAL_CSTRING("NC:DownloadsRoot"), &gNC_DownloadsRoot);
  gRDFService->GetResource(NS_LITERAL_CSTRING(NC_NAMESPACE_URI "File"), &gNC_File);
  gRDFService->GetResource(NS_LITERAL_CSTRING(NC_NAMESPACE_URI "URL"), &gNC_URL);
  gRDFService->GetResource(NS_LITERAL_CSTRING(NC_NAMESPACE_URI "Name"), &gNC_Name);
  gRDFService->GetResource(NS_LITERAL_CSTRING(NC_NAMESPACE_URI "ProgressMode"), &gNC_ProgressMode);
  gRDFService->GetResource(NS_LITERAL_CSTRING(NC_NAMESPACE_URI "ProgressPercent"), &gNC_ProgressPercent);
  gRDFService->GetResource(NS_LITERAL_CSTRING(NC_NAMESPACE_URI "Transferred"), &gNC_Transferred);
  gRDFService->GetResource(NS_LITERAL_CSTRING(NC_NAMESPACE_URI "DownloadState"), &gNC_DownloadState);
  gRDFService->GetResource(NS_LITERAL_CSTRING(NC_NAMESPACE_URI "StatusText"), &gNC_StatusText);

  nsCAutoString downloadsDB;
  rv = GetProfileDownloadsFileURL(downloadsDB);
  if (NS_FAILED(rv)) return rv;

  rv = gRDFService->GetDataSourceBlocking(downloadsDB.get(), getter_AddRefs(mDataSource));
  if (NS_FAILED(rv)) return rv;

  mListener = do_CreateInstance("@mozilla.org/download-manager/listener;1", &rv);
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsIStringBundleService> bundleService = do_GetService(NS_STRINGBUNDLE_CONTRACTID, &rv);
  if (NS_FAILED(rv)) return rv;
  
  rv =  bundleService->CreateBundle(DOWNLOAD_MANAGER_BUNDLE, getter_AddRefs(mBundle));
  if (NS_FAILED(rv))
    return rv;

  // The following two AddObserver calls must be the last lines in this function,
  // because otherwise, this function may fail (and thus, this object would be not
  // completely initialized), but the observerservice would still keep a reference
  // to us and notify us about shutdown, which may cause crashes.
  // failure to add an observer is not critical
  obsService->AddObserver(this, "profile-before-change", PR_FALSE);
  obsService->AddObserver(this, "profile-approve-change", PR_FALSE);

  return NS_OK;
}

nsresult
nsDownloadManager::DownloadStarted(const nsACString& aTargetPath)
{
  if (mCurrDownloads.GetWeak(aTargetPath))
    AssertProgressInfoFor(aTargetPath);

  return NS_OK;
}

nsresult
nsDownloadManager::DownloadEnded(const nsACString& aTargetPath, const PRUnichar* aMessage)
{
  nsDownload* dl = mCurrDownloads.GetWeak(aTargetPath);
  if (dl) {
    AssertProgressInfoFor(aTargetPath);
    mCurrDownloads.Remove(aTargetPath);
  }

  return NS_OK;
}

nsresult
nsDownloadManager::GetProfileDownloadsFileURL(nsCString& aDownloadsFileURL)
{
  nsCOMPtr<nsIFile> downloadsFile;
  nsresult rv = NS_GetSpecialDirectory(NS_APP_DOWNLOADS_50_FILE, getter_AddRefs(downloadsFile));
  if (NS_FAILED(rv))
    return rv;
    
  return NS_GetURLSpecFromFile(downloadsFile, aDownloadsFileURL);
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
nsDownloadManager::GetInternalListener(nsIDownloadProgressListener** aInternalListener)
{
  *aInternalListener = mListener;
  NS_IF_ADDREF(*aInternalListener);
  return NS_OK;
}

nsresult
nsDownloadManager::GetDataSource(nsIRDFDataSource** aDataSource)
{
  *aDataSource = mDataSource;
  NS_ADDREF(*aDataSource);
  return NS_OK;
}

nsresult
nsDownloadManager::AssertProgressInfo()
{
  nsCOMPtr<nsISupports> supports;
  nsCOMPtr<nsIRDFResource> res;
  nsCOMPtr<nsIRDFInt> intLiteral;

  gRDFService->GetIntLiteral(DOWNLOADING, getter_AddRefs(intLiteral));
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
    AssertProgressInfoFor(nsDependentCString(uri));
    downloads->HasMoreElements(&hasMoreElements);
  }
  return rv;
}

nsresult
nsDownloadManager::AssertProgressInfoFor(const nsACString& aTargetPath)
{
  nsDownload* internalDownload = mCurrDownloads.GetWeak(aTargetPath);
  if (!internalDownload)
    return NS_ERROR_FAILURE;
  
  nsresult rv;
  PRInt32 percentComplete;
  nsCOMPtr<nsIRDFNode> oldTarget;
  nsCOMPtr<nsIRDFInt> intLiteral;
  nsCOMPtr<nsIRDFResource> res;
  nsCOMPtr<nsIRDFLiteral> literal;

  gRDFService->GetResource(aTargetPath, getter_AddRefs(res));

  DownloadState state = internalDownload->GetDownloadState();
 
  // update progress mode
  nsAutoString progressMode;
  if (state == DOWNLOADING)
    progressMode.AssignLiteral("normal");
  else
    progressMode.AssignLiteral("none");

  gRDFService->GetLiteral(progressMode.get(), getter_AddRefs(literal));

  rv = mDataSource->GetTarget(res, gNC_ProgressMode, PR_TRUE, getter_AddRefs(oldTarget));
  
  if (oldTarget)
    rv = mDataSource->Change(res, gNC_ProgressMode, oldTarget, literal);
  else
    rv = mDataSource->Assert(res, gNC_ProgressMode, literal, PR_TRUE);
  if (NS_FAILED(rv)) return rv;

  // update download state (not started, downloading, queued, finished, etc...)
  gRDFService->GetIntLiteral(state, getter_AddRefs(intLiteral));

  mDataSource->GetTarget(res, gNC_DownloadState, PR_TRUE, getter_AddRefs(oldTarget));
  
  if (oldTarget) {
    rv = mDataSource->Change(res, gNC_DownloadState, oldTarget, intLiteral);
    if (NS_FAILED(rv)) return rv;
  }

  nsAutoString strKey;
  if (state == NOTSTARTED)
    strKey.AssignLiteral("notStarted");
  else if (state == DOWNLOADING)
    strKey.AssignLiteral("downloading");
  else if (state == FINISHED)
    strKey.AssignLiteral("finished");
  else if (state == FAILED)
    strKey.AssignLiteral("failed");
  else if (state == CANCELED)
    strKey.AssignLiteral("canceled");

  nsXPIDLString value;
  rv = mBundle->GetStringFromName(strKey.get(), getter_Copies(value));    
  if (NS_FAILED(rv)) return rv;

  gRDFService->GetLiteral(value, getter_AddRefs(literal));

  rv = mDataSource->GetTarget(res, gNC_StatusText, PR_TRUE, getter_AddRefs(oldTarget));
  
  if (oldTarget) {
    rv = mDataSource->Change(res, gNC_StatusText, oldTarget, literal);
    if (NS_FAILED(rv)) return rv;
  }
  else {
    rv = mDataSource->Assert(res, gNC_StatusText, literal, PR_TRUE);
    if (NS_FAILED(rv)) return rv;
  }
  
  // update percentage
  internalDownload->GetPercentComplete(&percentComplete);

  mDataSource->GetTarget(res, gNC_ProgressPercent, PR_TRUE, getter_AddRefs(oldTarget));
  gRDFService->GetIntLiteral(percentComplete, getter_AddRefs(intLiteral));

  if (oldTarget)
    rv = mDataSource->Change(res, gNC_ProgressPercent, oldTarget, intLiteral);
  else
    rv = mDataSource->Assert(res, gNC_ProgressPercent, intLiteral, PR_TRUE);
  if (NS_FAILED(rv)) return rv;

  // update transferred
  nsDownload::TransferInformation transferInfo =
                                 internalDownload->GetTransferInformation();

  // convert from bytes to kbytes for progress display
  PRInt64 current = (PRFloat64)transferInfo.mCurrBytes / 1024 + .5;
  PRInt64 max = (PRFloat64)transferInfo.mMaxBytes / 1024 + .5;

  nsAutoString currBytes; currBytes.AppendInt(current);
  nsAutoString maxBytes; maxBytes.AppendInt(max);
  const PRUnichar *strings[] = {
    currBytes.get(),
    maxBytes.get()
  };
  
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

  nsCOMPtr<nsIRDFRemoteDataSource> remote = do_QueryInterface(mDataSource);
  remote->Flush();

  // XXX should also store and update time elapsed
  return rv;
}  

///////////////////////////////////////////////////////////////////////////////
// nsIDownloadManager

NS_IMETHODIMP
nsDownloadManager::AddDownload(nsIURI* aSource,
                               nsIURI* aTarget,
                               const nsAString& aDisplayName,
                               nsIMIMEInfo *aMIMEInfo,
                               PRTime aStartTime,
                               nsILocalFile* aTempFile,
                               nsICancelable* aCancelable,
                               nsIDownload** aDownload)
{
  NS_ENSURE_ARG_POINTER(aSource);
  NS_ENSURE_ARG_POINTER(aTarget);
  NS_ENSURE_ARG_POINTER(aDownload);

  nsCOMPtr<nsIRDFContainer> downloads;
  nsresult rv = GetDownloadsContainer(getter_AddRefs(downloads));
  if (NS_FAILED(rv)) return rv;

  // this will create a cycle that will be broken in nsDownload::OnStateChange
  nsDownload* internalDownload = new nsDownload(this, aTarget, aSource, aCancelable);
  if (!internalDownload)
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*aDownload = internalDownload);

  // the path of the target is the unique identifier we use
  nsCOMPtr<nsILocalFile> targetFile;
  rv = internalDownload->GetTargetFile(getter_AddRefs(targetFile));
  if (NS_FAILED(rv)) return rv;

  nsAutoString path;
  rv = targetFile->GetPath(path);
  if (NS_FAILED(rv)) return rv;

  NS_ConvertUTF16toUTF8 utf8Path(path);

  nsCOMPtr<nsIRDFResource> downloadRes;
  gRDFService->GetResource(utf8Path, getter_AddRefs(downloadRes));

  nsCOMPtr<nsIRDFNode> node;

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
  if (NS_FAILED(rv)) return rv;

  // Set and assert the "pretty" (display) name of the download
  nsAutoString displayName; displayName.Assign(aDisplayName);
  if (displayName.IsEmpty()) {
    targetFile->GetLeafName(displayName);
  }
  internalDownload->SetDisplayName(displayName.get());
  internalDownload->SetTempFile(aTempFile);
 
  nsCOMPtr<nsIRDFLiteral> nameLiteral;
  gRDFService->GetLiteral(displayName.get(), getter_AddRefs(nameLiteral));
  mDataSource->GetTarget(downloadRes, gNC_Name, PR_TRUE, getter_AddRefs(node));
  if (node)
    rv = mDataSource->Change(downloadRes, gNC_Name, node, nameLiteral);
  else
    rv = mDataSource->Assert(downloadRes, gNC_Name, nameLiteral, PR_TRUE);
  if (NS_FAILED(rv)) return rv;
  
  internalDownload->SetMIMEInfo(aMIMEInfo);
  internalDownload->SetStartTime(aStartTime);

  // Assert file information
  nsCOMPtr<nsIRDFResource> fileResource;
  gRDFService->GetResource(utf8Path, getter_AddRefs(fileResource));
  rv = mDataSource->Assert(downloadRes, gNC_File, fileResource, PR_TRUE);
  if (NS_FAILED(rv)) return rv;
  
  // Assert download state information (NOTSTARTED, since it's just now being added)
  nsCOMPtr<nsIRDFInt> intLiteral;
  gRDFService->GetIntLiteral(NOTSTARTED, getter_AddRefs(intLiteral));
  mDataSource->GetTarget(downloadRes, gNC_DownloadState, PR_TRUE, getter_AddRefs(node));
  if (node)
    rv = mDataSource->Change(downloadRes, gNC_DownloadState, node, intLiteral);
  else
    rv = mDataSource->Assert(downloadRes, gNC_DownloadState, intLiteral, PR_TRUE);
  if (NS_FAILED(rv)) return rv;
  
  PRInt32 itemIndex;
  downloads->IndexOf(downloadRes, &itemIndex);
  if (itemIndex == -1) {
    rv = downloads->AppendElement(downloadRes);
    if (NS_FAILED(rv)) return rv;
  }

  // Now flush all this to disk
  nsCOMPtr<nsIRDFRemoteDataSource> remote(do_QueryInterface(mDataSource));
  rv = remote->Flush();
  if (NS_FAILED(rv)) return rv;

  mCurrDownloads.Put(utf8Path, internalDownload);

  return rv;
}

NS_IMETHODIMP
nsDownloadManager::GetDownload(const nsACString & aTargetPath, nsIDownload** aDownloadItem)
{
  NS_ENSURE_ARG_POINTER(aDownloadItem);

  // if it's currently downloading we can get it from the table
  // XXX otherwise we should look for it in the datasource and
  //     create a new nsIDownload with the resource's properties
  NS_IF_ADDREF(*aDownloadItem = mCurrDownloads.GetWeak(aTargetPath));
  return NS_OK;
}

NS_IMETHODIMP
nsDownloadManager::CancelDownload(const nsACString & aTargetPath)
{
  nsRefPtr<nsDownload> internalDownload = mCurrDownloads.GetWeak(aTargetPath);
  if (!internalDownload)
    return NS_ERROR_FAILURE;

  return internalDownload->Cancel();
}

NS_IMETHODIMP
nsDownloadManager::PauseDownload(nsIDownload* aDownload)
{
  NS_ENSURE_ARG_POINTER(aDownload);
  return NS_STATIC_CAST(nsDownload*, aDownload)->Suspend();
}

NS_IMETHODIMP
nsDownloadManager::ResumeDownload(const nsACString & aTargetPath)
{
  nsDownload* dl = mCurrDownloads.GetWeak(aTargetPath);
  if (!dl)
    return NS_ERROR_NOT_AVAILABLE;
  return dl->Resume();
}

NS_IMETHODIMP
nsDownloadManager::RemoveDownload(const nsACString & aTargetPath)
{
  // RemoveDownload is for downloads not currently in progress. Having it
  // cancel in-progress downloads would make things complicated, so just return.
  nsDownload* inProgress = mCurrDownloads.GetWeak(aTargetPath);
  NS_ASSERTION(!inProgress, "Can't call RemoveDownload on a download in progress!");
  if (inProgress)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIRDFContainer> downloads;
    nsresult rv = GetDownloadsContainer(getter_AddRefs(downloads));
  if (NS_FAILED(rv)) return rv;
  
  nsCOMPtr<nsIRDFResource> res;
  gRDFService->GetResource(aTargetPath, getter_AddRefs(res));

  // remove all the arcs for this resource, and then remove it from the Seq
  nsCOMPtr<nsISimpleEnumerator> arcs;
  rv = mDataSource->ArcLabelsOut(res, getter_AddRefs(arcs));
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
    rv = mDataSource->GetTargets(res, arc, PR_TRUE, getter_AddRefs(targets));
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
      rv = mDataSource->Unassert(res, arc, target);
      if (NS_FAILED(rv)) return rv;

      rv = targets->HasMoreElements(&moreTargets);
      if (NS_FAILED(rv)) return rv;
    }
    rv = arcs->HasMoreElements(&moreArcs);
    if (NS_FAILED(rv)) return rv;
  }

  PRInt32 itemIndex;
  downloads->IndexOf(res, &itemIndex);
  if (itemIndex <= 0)
    return NS_ERROR_FAILURE;
  
  nsCOMPtr<nsIRDFNode> node;
  rv = downloads->RemoveElementAt(itemIndex, PR_TRUE, getter_AddRefs(node));
  if (NS_FAILED(rv)) return rv;
  
  // if a mass removal is being done, we don't want to flush every time
  if (mBatches) return rv;

  nsCOMPtr<nsIRDFRemoteDataSource> remote = do_QueryInterface(mDataSource);
  return remote->Flush();
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
  nsresult rv = NS_OK;
  if (--mBatches == 0) {
    nsCOMPtr<nsIRDFRemoteDataSource> remote = do_QueryInterface(mDataSource);
    rv = remote->Flush();
  }
  return rv;
}

NS_IMETHODIMP
nsDownloadManager::Open(nsIDOMWindow* aParent, nsIDownload* aDownload)
{

  // first assert new progress info so the ui is correctly updated
  // if this fails, it fails -- continue.
  AssertProgressInfo();
  
  // check for an existing manager window and focus it
  nsresult rv;
  nsCOMPtr<nsIWindowMediator> wm = do_GetService(NS_WINDOWMEDIATOR_CONTRACTID, &rv);
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsISupports> dlSupports(do_QueryInterface(aDownload));

  // if the window's already open, do nothing (focusing it would be annoying)
  nsCOMPtr<nsIDOMWindowInternal> recentWindow;
  wm->GetMostRecentWindow(NS_LITERAL_STRING("Download:Manager").get(), getter_AddRefs(recentWindow));
  if (recentWindow) {
    nsCOMPtr<nsIObserverService> obsService = do_GetService("@mozilla.org/observer-service;1", &rv);
    if (NS_FAILED(rv)) return rv;
    return obsService->NotifyObservers(dlSupports, "download-starting", nsnull);
  }

  // if we ever have the capability to display the UI of third party dl managers,
  // we'll open their UI here instead.
  nsCOMPtr<nsIWindowWatcher> ww = do_GetService(NS_WINDOWWATCHER_CONTRACTID, &rv);
  if (NS_FAILED(rv)) return rv;

  // pass the datasource to the window
  nsCOMPtr<nsISupportsArray> params(do_CreateInstance(NS_SUPPORTSARRAY_CONTRACTID));
  nsCOMPtr<nsISupports> dsSupports(do_QueryInterface(mDataSource));
  params->AppendElement(dsSupports);
  params->AppendElement(dlSupports);

  nsCOMPtr<nsIDOMWindow> newWindow;
  rv = ww->OpenWindow(aParent,
                      DOWNLOAD_MANAGER_FE_URL,
                      "_blank",
                      "chrome,all,dialog=no,resizable",
                      params,
                      getter_AddRefs(newWindow));

  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsIDOMEventTarget> target = do_QueryInterface(newWindow);
  if (!target) return NS_ERROR_FAILURE;
  
  rv = target->AddEventListener(NS_LITERAL_STRING("load"), this, PR_FALSE);
  if (NS_FAILED(rv)) return rv;

  return target->AddEventListener(NS_LITERAL_STRING("unload"), this, PR_FALSE);
}

NS_IMETHODIMP
nsDownloadManager::OpenProgressDialogFor(nsIDownload* aDownload, nsIDOMWindow* aParent, PRBool aCancelDownloadOnClose)
{
  NS_ENSURE_ARG_POINTER(aDownload);
  nsresult rv;
  nsDownload* internalDownload = NS_STATIC_CAST(nsDownload*, aDownload);
  nsIProgressDialog* oldDialog = internalDownload->GetDialog();
  
  if (oldDialog) {
    nsCOMPtr<nsIDOMWindow> window;
    oldDialog->GetDialog(getter_AddRefs(window));
    if (window) {
      nsCOMPtr<nsIDOMWindowInternal> internalWin = do_QueryInterface(window);
      internalWin->Focus();
      return NS_OK;
    }
  }

  nsCOMPtr<nsIProgressDialog> dialog(do_CreateInstance("@mozilla.org/progressdialog;1", &rv));
  if (NS_FAILED(rv)) return rv;
  
  dialog->SetCancelDownloadOnClose(aCancelDownloadOnClose);
  
  // now give the dialog the necessary context
  
  // start time...
  PRInt64 startTime = 0;
  aDownload->GetStartTime(&startTime);
  
  // source...
  nsCOMPtr<nsIURI> source;
  aDownload->GetSource(getter_AddRefs(source));

  // target...
  nsCOMPtr<nsIURI> target;
  aDownload->GetTarget(getter_AddRefs(target));
  
  // helper app...
  nsCOMPtr<nsIMIMEInfo> mimeInfo;
  aDownload->GetMIMEInfo(getter_AddRefs(mimeInfo));

  dialog->Init(source, target, EmptyString(), mimeInfo, startTime, nsnull,
               nsnull); 
  dialog->SetObserver(internalDownload);

  // now set the listener so we forward notifications to the dialog
  nsCOMPtr<nsIDownloadProgressListener> listener = do_QueryInterface(dialog);
  internalDownload->SetDialogListener(listener);
  
  internalDownload->SetDialog(dialog);
  
  return dialog->Open(aParent);
}

NS_IMETHODIMP
nsDownloadManager::OnClose()
{
  mDocument = nsnull;
  mListener->SetDocument(nsnull);
  return NS_OK;
}

///////////////////////////////////////////////////////////////////////////////
// nsIDOMEventListener

NS_IMETHODIMP
nsDownloadManager::HandleEvent(nsIDOMEvent* aEvent)
{
  // the event is either load or unload
  nsAutoString eventType;
  aEvent->GetType(eventType);
  if (eventType.EqualsLiteral("unload"))
    return OnClose();

  nsCOMPtr<nsIDOMEventTarget> target;
  nsresult rv = aEvent->GetTarget(getter_AddRefs(target));
  if (NS_FAILED(rv)) return rv;

  mDocument = do_QueryInterface(target);
  mListener->SetDocument(mDocument);
  return NS_OK;
}

///////////////////////////////////////////////////////////////////////////////
// nsIObserver

NS_IMETHODIMP
nsDownloadManager::Observe(nsISupports* aSubject, const char* aTopic, const PRUnichar* aData)
{
  nsresult rv;
  if (nsCRT::strcmp(aTopic, "profile-approve-change") == 0) {
    // Only run this on profile switch
    if (!NS_LITERAL_STRING("switch").Equals(aData))
      return NS_OK;

    // If count == 0, nothing to do
    if (mCurrDownloads.Count() == 0)
      return NS_OK;

    nsCOMPtr<nsIProfileChangeStatus> changeStatus(do_QueryInterface(aSubject));
    if (!changeStatus)
      return NS_ERROR_UNEXPECTED;

    nsXPIDLString title, text, proceed, cancel;
    nsresult rv = mBundle->GetStringFromName(NS_LITERAL_STRING("profileSwitchTitle").get(),
                                             getter_Copies(title));
    NS_ENSURE_SUCCESS(rv, rv);
    rv = mBundle->GetStringFromName(NS_LITERAL_STRING("profileSwitchText").get(),
                                    getter_Copies(text));
    NS_ENSURE_SUCCESS(rv, rv);
    rv = mBundle->GetStringFromName(NS_LITERAL_STRING("profileSwitchContinue").get(),
                                    getter_Copies(proceed));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIPromptService> promptService(do_GetService(NS_PROMPTSERVICE_CONTRACTID, &rv));
    if (NS_FAILED(rv))
      return rv;

    PRInt32 button;
    rv = promptService->ConfirmEx(nsnull, title.get(), text.get(),
                                  nsIPromptService::BUTTON_TITLE_CANCEL * nsIPromptService::BUTTON_POS_0 |
                                  nsIPromptService::BUTTON_TITLE_IS_STRING * nsIPromptService::BUTTON_POS_1,
                                  nsnull,
                                  proceed.get(),
                                  nsnull,
                                  nsnull,
                                  nsnull,
                                  &button);
    if (NS_FAILED(rv))
      return rv;

    if (button == 0)
      changeStatus->VetoChange();
  }
  else if (nsCRT::strcmp(aTopic, "profile-before-change") == 0) {
    nsCOMPtr<nsISupports> supports;
    nsCOMPtr<nsIRDFResource> res;
    nsCOMPtr<nsIRDFInt> intLiteral;

    gRDFService->GetIntLiteral(DOWNLOADING, getter_AddRefs(intLiteral));
    nsCOMPtr<nsISimpleEnumerator> downloads;
    rv = mDataSource->GetSources(gNC_DownloadState, intLiteral, PR_TRUE, getter_AddRefs(downloads));
    if (NS_FAILED(rv)) return rv;
    
    PRBool hasMoreElements;
    downloads->HasMoreElements(&hasMoreElements);

    while (hasMoreElements) {
      const char* uri;

      downloads->GetNext(getter_AddRefs(supports));
      res = do_QueryInterface(supports);
      res->GetValueConst(&uri);
      CancelDownload(nsDependentCString(uri));
      downloads->HasMoreElements(&hasMoreElements);
    }    
  }
  return NS_OK;
}

///////////////////////////////////////////////////////////////////////////////
// nsDownload

NS_IMPL_ISUPPORTS5(nsDownload, nsIDownload, nsITransfer, nsIWebProgressListener,
                   nsIWebProgressListener2, nsIObserver)

nsDownload::nsDownload(nsDownloadManager* aManager,
                       nsIURI* aTarget,
                       nsIURI* aSource,
                       nsICancelable* aCancelable) :
                         mDownloadManager(aManager),
                         mTarget(aTarget),
                         mSource(aSource),
                         mCancelable(aCancelable),
                         mDownloadState(NOTSTARTED),
                         mPercentComplete(0),
                         mCurrBytes(LL_ZERO),
                         mMaxBytes(LL_ZERO),
                         mStartTime(LL_ZERO),
                         mSpeed(0),
                         mLastUpdate(PR_Now() - (PRUint32)gInterval)
{
}

nsDownload::~nsDownload()
{  
  nsCAutoString path;
  nsresult rv = GetFilePathUTF8(mTarget, path);
  if (NS_FAILED(rv)) return;

  mDownloadManager->AssertProgressInfoFor(path);
}

nsresult
nsDownload::Suspend()
{
  if (!mRequest)
    return NS_ERROR_UNEXPECTED;
  return mRequest->Suspend();
}

nsresult
nsDownload::Cancel()
{
  // Don't cancel if download is already finished or canceled
  if (GetDownloadState() == FINISHED || GetDownloadState() == CANCELED)
    return NS_OK;

  nsresult rv = mCancelable->Cancel(NS_BINDING_ABORTED);
  if (NS_FAILED(rv))
    return rv;
  
  SetDownloadState(CANCELED);

  nsCAutoString path;
  rv = GetFilePathUTF8(mTarget, path);
  if (NS_FAILED(rv))
    return rv;
  mDownloadManager->DownloadEnded(path, nsnull);
  
  // Dump the temp file.  This should really be done when the transfer
  // is cancelled, but there are other cancellation causes that shouldn't
  // remove this. We need to improve those bits.
  if (mTempFile) {
    PRBool exists;
    mTempFile->Exists(&exists);
    if (exists)
      mTempFile->Remove(PR_FALSE);
  }

  // if there's a progress dialog open for the item,
  // we have to notify it that we're cancelling
  nsCOMPtr<nsIObserver> observer = do_QueryInterface(GetDialog());
  if (observer) {
    rv = observer->Observe(NS_STATIC_CAST(nsIDownload*, this), "oncancel", nsnull);
  }
  
  return rv;
}

nsresult
nsDownload::SetDisplayName(const PRUnichar* aDisplayName)
{
  mDisplayName = aDisplayName;

  nsCOMPtr<nsIRDFDataSource> ds;
  mDownloadManager->GetDataSource(getter_AddRefs(ds));

  nsCOMPtr<nsIRDFLiteral> nameLiteral;
  nsCOMPtr<nsIRDFResource> res;
  nsCAutoString path;
  nsresult rv = GetFilePathUTF8(mTarget, path);
  if (NS_FAILED(rv)) return rv;

  gRDFService->GetResource(path, getter_AddRefs(res));
  
  gRDFService->GetLiteral(aDisplayName, getter_AddRefs(nameLiteral));
  ds->Assert(res, gNC_Name, nameLiteral, PR_TRUE);

  return NS_OK;
}

nsresult
nsDownload::Resume()
{
  if (!mRequest)
    return NS_ERROR_UNEXPECTED;
  return mRequest->Resume();
}

///////////////////////////////////////////////////////////////////////////////
// nsIObserver
NS_IMETHODIMP
nsDownload::Observe(nsISupports* aSubject, const char* aTopic, const PRUnichar* aData)
{
  if (strcmp(aTopic, "onpause") == 0) {
    return Suspend();
  }
  if (strcmp(aTopic, "onresume") == 0) {
    return Resume();
  }
  if (strcmp(aTopic, "oncancel") == 0) {
    SetDialog(nsnull);

    Cancel();
    // Ignoring return value; this function will get called twice,
    // and bad things happen if we return a failure code the second time.
    return NS_OK;
  }

  if (strcmp(aTopic, "alertclickcallback") == 0) {
    // show the download manager
    mDownloadManager->Open(nsnull, this);
    return NS_OK;
  }

  return NS_OK;
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

  // Filter notifications since they come in so frequently, but we want to
  // process the last notification.
  PRTime now = PR_Now();
  nsInt64 delta = now - mLastUpdate;
  if (delta < gInterval && aCurTotalProgress != aMaxTotalProgress)
    return NS_OK;

  mLastUpdate = now;

  if (mDownloadState == NOTSTARTED) {
    nsCAutoString path;
    nsresult rv = GetFilePathUTF8(mTarget, path);
    if (NS_FAILED(rv)) return rv;

    mDownloadState = DOWNLOADING;
    mDownloadManager->DownloadStarted(path);
  }

  // Calculate the speed using the elapsed delta time and bytes downloaded
  // during that time for more accuracy.
  double elapsedSecs = double(delta) / PR_USEC_PER_SEC;
  if (elapsedSecs > 0) {
    nsUint64 curTotalProgress = (PRUint64)aCurTotalProgress;
    nsUint64 diffBytes = curTotalProgress - nsUint64(mCurrBytes);
    double speed = double(diffBytes) / elapsedSecs;
    if (LL_IS_ZERO(mCurrBytes))
      mSpeed = speed;
    else {
      // Calculate 'smoothed average' of 10 readings.
      mSpeed = mSpeed * 0.9 + speed * 0.1;
    }
  }

  if (aMaxTotalProgress > 0)
    mPercentComplete = aCurTotalProgress * 100 / aMaxTotalProgress;
  else
    mPercentComplete = -1;

  mCurrBytes = aCurTotalProgress;
  mMaxBytes = aMaxTotalProgress;

  if (mDownloadManager->MustUpdateUI()) {
    nsCOMPtr<nsIDownloadProgressListener> internalListener;
    mDownloadManager->GetInternalListener(getter_AddRefs(internalListener));
    if (internalListener) {
      internalListener->OnProgressChange(aWebProgress, aRequest, aCurSelfProgress, aMaxSelfProgress,
                                         aCurTotalProgress, aMaxTotalProgress, this);
    }
  }

  if (mDialogListener) {
    mDialogListener->OnProgressChange(aWebProgress, aRequest, aCurSelfProgress, aMaxSelfProgress,
                                      aCurTotalProgress, aMaxTotalProgress, this);
  }

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
nsDownload::OnRefreshAttempted(nsIWebProgress *aWebProgress,
                               nsIURI *aUri,
                               PRInt32 aDelay,
                               PRBool aSameUri,
                               PRBool *allowRefresh)
{
  *allowRefresh = PR_TRUE;
  return NS_OK;
}

NS_IMETHODIMP
nsDownload::OnLocationChange(nsIWebProgress *aWebProgress,
                             nsIRequest *aRequest, nsIURI *aLocation)
{
  if (mDownloadManager->MustUpdateUI()) {
    nsCOMPtr<nsIDownloadProgressListener> internalListener;
    mDownloadManager->GetInternalListener(getter_AddRefs(internalListener));
    if (internalListener)
      internalListener->OnLocationChange(aWebProgress, aRequest, aLocation, this);
  }

  if (mDialogListener)
    mDialogListener->OnLocationChange(aWebProgress, aRequest, aLocation, this);

  return NS_OK;
}

NS_IMETHODIMP
nsDownload::OnStatusChange(nsIWebProgress *aWebProgress,
                           nsIRequest *aRequest, nsresult aStatus,
                           const PRUnichar *aMessage)
{   
  if (NS_FAILED(aStatus)) {
    mDownloadState = FAILED;
    nsCAutoString path;
    nsresult rv = GetFilePathUTF8(mTarget, path);
    if (NS_SUCCEEDED(rv))
      mDownloadManager->DownloadEnded(path, aMessage);
  }

  if (mDownloadManager->MustUpdateUI()) {
    nsCOMPtr<nsIDownloadProgressListener> internalListener;
    mDownloadManager->GetInternalListener(getter_AddRefs(internalListener));
    if (internalListener)
      internalListener->OnStatusChange(aWebProgress, aRequest, aStatus, aMessage, this);
  }

  if (mDialogListener)
    mDialogListener->OnStatusChange(aWebProgress, aRequest, aStatus, aMessage, this);
  else {
    // Need to display error alert ourselves, if an error occurred.
    if (NS_FAILED(aStatus)) {
      // Get title for alert.
      nsXPIDLString title;
      nsresult rv;
      nsCOMPtr<nsIStringBundleService> bundleService = do_GetService(NS_STRINGBUNDLE_CONTRACTID, &rv);
      nsCOMPtr<nsIStringBundle> bundle;
      if (bundleService)
        rv = bundleService->CreateBundle(DOWNLOAD_MANAGER_BUNDLE, getter_AddRefs(bundle));
      if (bundle)
        bundle->GetStringFromName(NS_LITERAL_STRING("alertTitle").get(), getter_Copies(title));    

      // Get Download Manager window, to be parent of alert.
      nsCOMPtr<nsIWindowMediator> wm = do_GetService(NS_WINDOWMEDIATOR_CONTRACTID, &rv);
      nsCOMPtr<nsIDOMWindowInternal> dmWindow;
      if (wm)
        wm->GetMostRecentWindow(NS_LITERAL_STRING("Download:Manager").get(), getter_AddRefs(dmWindow));

      // Show alert.
      nsCOMPtr<nsIPromptService> prompter(do_GetService(NS_PROMPTSERVICE_CONTRACTID));
      if (prompter)
        prompter->Alert(dmWindow, title, aMessage);
    }
  }

  return NS_OK;
}

void nsDownload::DisplayDownloadFinishedAlert()
{
  nsresult rv;
  nsCOMPtr<nsIAlertsService> alertsService(do_GetService(NS_ALERTSERVICE_CONTRACTID, &rv));
  if (NS_FAILED(rv))
    return;

  nsCOMPtr<nsIStringBundle> bundle;
  nsCOMPtr<nsIStringBundleService> bundleService = do_GetService(NS_STRINGBUNDLE_CONTRACTID, &rv);
  if (NS_FAILED(rv))
    return;

  rv = bundleService->CreateBundle(DOWNLOAD_MANAGER_BUNDLE, getter_AddRefs(bundle));
  if (NS_FAILED(rv))
    return;

  nsXPIDLString finishedTitle, finishedText;
  rv = bundle->GetStringFromName(NS_LITERAL_STRING("finishedTitle").get(),
                                 getter_Copies(finishedTitle));
  if (NS_FAILED(rv))
    return;

  const PRUnichar *strings[] = { mDisplayName.get() };
  rv = bundle->FormatStringFromName(NS_LITERAL_STRING("finishedText").get(),
                                    strings, 1, getter_Copies(finishedText));
  if (NS_FAILED(rv))
    return;
  
  nsCAutoString url;
  mTarget->GetSpec(url);
  alertsService->ShowAlertNotification(NS_LITERAL_STRING("moz-icon://") + NS_ConvertUTF8toUTF16(url),
                                       finishedTitle, finishedText, PR_TRUE,
                                       NS_LITERAL_STRING("download"), this);
}

NS_IMETHODIMP
nsDownload::OnStateChange(nsIWebProgress* aWebProgress,
                          nsIRequest* aRequest, PRUint32 aStateFlags,
                          nsresult aStatus)
{
  // Record the start time only if it hasn't been set.
  if (LL_IS_ZERO(mStartTime) && (aStateFlags & STATE_START))
    SetStartTime(PR_Now());

  // When we break the ref cycle with mPersist, we don't want to lose
  // access to out member vars!
  nsRefPtr<nsDownload> kungFuDeathGrip(this);
  
  // We need to update mDownloadState before updating the dialog, because
  // that will close and call CancelDownload if it was the last open window.
  nsresult rv = NS_OK;
  if (aStateFlags & STATE_STOP) {
    if (mDownloadState == DOWNLOADING || mDownloadState == NOTSTARTED) {
      mDownloadState = FINISHED;

      // Set file size at the end of a transfer (for unknown transfer amounts)
      if (mMaxBytes == -1)
        mMaxBytes = mCurrBytes;

      // Files less than 1Kb shouldn't show up as 0Kb.
      if (mMaxBytes < 1024) {
        mCurrBytes = 1024;
        mMaxBytes  = 1024;
      }

      mPercentComplete = 100;

      // Play a sound or show an alert when the download finishes
      PRBool playSound = PR_FALSE;
      PRBool showAlert = PR_FALSE;
      nsXPIDLCString soundStr;

      nsCOMPtr<nsIPrefService> prefs = do_GetService("@mozilla.org/preferences-service;1");

      if (prefs) {
        nsCOMPtr<nsIPrefBranch> prefBranch;
        prefs->GetBranch(nsnull, getter_AddRefs(prefBranch));
        if (prefBranch) {
          rv = prefBranch->GetBoolPref("browser.download.finished_download_sound", &playSound);
          if (NS_SUCCEEDED(rv) && playSound)
            prefBranch->GetCharPref("browser.download.finished_sound_url", getter_Copies(soundStr));
          rv = prefBranch->GetBoolPref("browser.download.finished_download_alert", &showAlert);
          if (NS_FAILED(rv))
            showAlert = PR_FALSE;
        }
      }

      if (!soundStr.IsEmpty()) {
        if (!mDownloadManager->mSoundInterface) {
          mDownloadManager->mSoundInterface = do_CreateInstance("@mozilla.org/sound;1");
        }
        if (mDownloadManager->mSoundInterface) {
          nsCOMPtr<nsIURI> soundURI;
          
          NS_NewURI(getter_AddRefs(soundURI), soundStr);
          nsCOMPtr<nsIURL> soundURL(do_QueryInterface(soundURI));
          if (soundURL)
            mDownloadManager->mSoundInterface->Play(soundURL);
          else
            mDownloadManager->mSoundInterface->Beep();
        }
      }
      if (showAlert)
        DisplayDownloadFinishedAlert();
      
      nsCAutoString path;
      rv = GetFilePathUTF8(mTarget, path);
      // can't do an early return; have to break reference cycle below
      if (NS_SUCCEEDED(rv)) {
        mDownloadManager->DownloadEnded(path, nsnull);
      }
    }

    // break the cycle we created in AddDownload
    mCancelable = nsnull;
    // and the one with the progress dialog
    if (mDialog) {
      mDialog->SetObserver(nsnull);
      mDialog = nsnull;
    }
  }

  if (mDownloadManager->MustUpdateUI()) {
    nsCOMPtr<nsIDownloadProgressListener> internalListener;
    mDownloadManager->GetInternalListener(getter_AddRefs(internalListener));
    if (internalListener)
      internalListener->OnStateChange(aWebProgress, aRequest, aStateFlags, aStatus, this);
  }

  if (mDialogListener) {
    mDialogListener->OnStateChange(aWebProgress, aRequest, aStateFlags, aStatus, this);
    if (aStateFlags & STATE_STOP) {
      // Break this cycle, too
      mDialogListener = nsnull;
    }
  }

  return rv;
}

NS_IMETHODIMP
nsDownload::OnSecurityChange(nsIWebProgress *aWebProgress,
                             nsIRequest *aRequest, PRUint32 aState)
{
  if (mDownloadManager->MustUpdateUI()) {
    nsCOMPtr<nsIDownloadProgressListener> internalListener;
    mDownloadManager->GetInternalListener(getter_AddRefs(internalListener));
    if (internalListener)
      internalListener->OnSecurityChange(aWebProgress, aRequest, aState, this);
  }

  if (mDialogListener)
    mDialogListener->OnSecurityChange(aWebProgress, aRequest, aState, this);

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
  NS_NOTREACHED("Huh...how did we get here?!");
  return NS_OK;
}

NS_IMETHODIMP
nsDownload::GetDisplayName(PRUnichar** aDisplayName)
{
  *aDisplayName = ToNewUnicode(mDisplayName);
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

nsresult
nsDownload::SetTempFile(nsILocalFile* aTempFile)
{
  mTempFile = aTempFile;
  return NS_OK;
}
