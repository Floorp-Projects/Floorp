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
 *   Ben Goodger <ben@netscape.com>
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
#include "prprf.h"
#include "nsIServiceManager.h"
#include "nsIWebProgress.h"
#include "nsIStringBundle.h"
#include "nsIRDFLiteral.h"
#include "nsIRDFXMLSerializer.h"
#include "nsIContent.h"
#include "nsIRDFXMLSource.h"
#include "rdf.h"
#include "nsNetUtil.h"
#include "nsMemory.h"
#include "nsIDOMXULDocument.h"
#include "nsIDOMXULElement.h"
#include "prtime.h"
#include "nsRDFCID.h"
#include "nsCRT.h"
#include "nsString.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsFileSpec.h"
#include "nsFileStream.h"

/* Outstanding issues/todo:
 * 1. Client should be able to set cancel callback so users can cancel downloads from manager FE.
 * 2. InitializeUI/UnitializeUI is lame...would like to keep manager separate from FE as
     much as possible. The document arg is also lame but the listener needs it to update the UI.
 * 3. Error handling? Holding off on this until law is done with his changes.
 * 4. NotifyDownloadEnded and internalListener should not be on nsIDownloadManager
 */
  
static NS_DEFINE_CID(kRDFServiceCID, NS_RDFSERVICE_CID);
static NS_DEFINE_CID(kRDFXMLDataSourceCID, NS_RDFXMLDATASOURCE_CID);

#define PROFILE_DOWNLOAD_FILE "downloads.rdf"

nsIRDFResource* gNC_DownloadsRoot;
nsIRDFResource* gNC_File;
nsIRDFResource* gNC_URL;
nsIRDFResource* gNC_Name;
nsIRDFResource* gNC_ProgressPercent;

nsIRDFService* gRDFService;
static PRBool gMustUpdateUI = PR_FALSE;

NS_IMPL_ISUPPORTS3(nsDownloadManager, nsIDownloadManager, nsIRDFDataSource, nsIRDFRemoteDataSource)

nsDownloadManager::nsDownloadManager() : mCurrDownloadItems(nsnull)
{
  NS_INIT_ISUPPORTS();
  NS_INIT_REFCNT();
}

nsDownloadManager::~nsDownloadManager()
{
  // write datasource to disk on shutdown
  Flush();

  gRDFService->UnregisterDataSource(this);

  NS_IF_RELEASE(gNC_DownloadsRoot);
  NS_IF_RELEASE(gNC_File);
  NS_IF_RELEASE(gNC_URL);
  NS_IF_RELEASE(gNC_Name);
  NS_IF_RELEASE(gNC_ProgressPercent);

  nsServiceManager::ReleaseService(kRDFServiceCID, gRDFService);
  gRDFService = nsnull;

  delete mCurrDownloadItems;
  mCurrDownloadItems = nsnull;
}

nsresult
nsDownloadManager::Init()
{
  nsresult rv = nsServiceManager::GetService(kRDFServiceCID, NS_GET_IID(nsIRDFService),
                                             (nsISupports**) &gRDFService);
  if (NS_FAILED(rv)) return rv;

  mRDFContainerUtils = do_GetService("@mozilla.org/rdf/container-utils;1", &rv);
  if (NS_FAILED(rv)) return rv;

  gRDFService->GetResource("NC:DownloadsRoot", &gNC_DownloadsRoot);
  gRDFService->GetResource(NC_NAMESPACE_URI "File", &gNC_File);
  gRDFService->GetResource(NC_NAMESPACE_URI "URL", &gNC_URL);
  gRDFService->GetResource(NC_NAMESPACE_URI "Name", &gNC_Name);
  gRDFService->GetResource(NC_NAMESPACE_URI "ProgressPercent", &gNC_ProgressPercent);

  mInner = do_GetService(kRDFXMLDataSourceCID, &rv);
  if (NS_FAILED(rv)) return rv;

  nsXPIDLCString downloadsDB;
  rv = GetProfileDownloadsFileURL(getter_Copies(downloadsDB));
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsIRDFRemoteDataSource> remote(do_QueryInterface(mInner));

  rv = remote->Init(downloadsDB);
  if (NS_FAILED(rv)) return rv;

  rv = remote->Refresh(PR_TRUE);
  if (NS_FAILED(rv)) return rv;

  return gRDFService->RegisterDataSource(this, PR_FALSE);
}

NS_IMETHODIMP
nsDownloadManager::GetURI(char** aURI)
{
  if (!aURI)
    return NS_ERROR_NULL_POINTER;

  *aURI = nsCRT::strdup("rdf:downloads");
  if (!(*aURI))
    return NS_ERROR_OUT_OF_MEMORY;

  return NS_OK;
}

NS_IMETHODIMP
nsDownloadManager::AddItem(nsIDownloadItem* aDownloadItem)
{
  NS_ENSURE_ARG_POINTER(aDownloadItem);

  nsCOMPtr<nsIRDFContainer> downloads;
  nsresult rv = GetDownloadsContainer(getter_AddRefs(downloads));
  if (NS_FAILED(rv)) return rv;

  nsXPIDLCString filePath;
  nsCOMPtr<nsILocalFile> target;
  aDownloadItem->GetTarget(getter_AddRefs(target));
  if (!target) return NS_ERROR_FAILURE;

  target->GetPath(getter_Copies(filePath));

  nsCOMPtr<nsIRDFResource> downloadItem;
  gRDFService->GetResource(filePath, getter_AddRefs(downloadItem));

  // if the resource is in the container already (the user has already
  // downloaded this file), remove it
  PRInt32 itemIndex;
  downloads->IndexOf(downloadItem, &itemIndex);
  if (itemIndex > 0) {
    nsCOMPtr<nsIRDFNode> node;
    downloads->RemoveElementAt(itemIndex, PR_TRUE, getter_AddRefs(node));
  }

  downloads->AppendElement(downloadItem);

  nsXPIDLString prettyName;
  aDownloadItem->GetPrettyName(getter_Copies(prettyName));
  nsAutoString displayName; displayName.Assign(prettyName);
  // NC:Name
  if (displayName.IsEmpty()) {
    target->GetUnicodeLeafName(getter_Copies(prettyName));
    displayName.Assign(prettyName);
  }

  nsCOMPtr<nsIRDFLiteral> nameLiteral;
  gRDFService->GetLiteral(displayName.get(), getter_AddRefs(nameLiteral));
  rv = Assert(downloadItem, gNC_Name, nameLiteral, PR_TRUE);
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsIURI> source;
  aDownloadItem->GetSource(getter_AddRefs(source));
  // NC:URL
  nsXPIDLCString spec;
  source->GetSpec(getter_Copies(spec));

  nsCOMPtr<nsIDownloadProgressListener> listener(do_CreateInstance("@mozilla.org/download-manager/listener;1", &rv));
  if (NS_FAILED(rv)) return rv;
  aDownloadItem->SetInternalListener(listener);
  if (gMustUpdateUI) {
    listener->SetDocument(mDocument);
    listener->SetDownloadItem(aDownloadItem);
  }

  nsCOMPtr<nsIRDFResource> urlResource;
  gRDFService->GetResource(spec, getter_AddRefs(urlResource));
  rv = Assert(downloadItem, gNC_URL, urlResource, PR_TRUE);
  if (NS_FAILED(rv)) return rv;

  // NC:File
  nsCOMPtr<nsIRDFResource> fileResource;
  gRDFService->GetResource(filePath, getter_AddRefs(fileResource));
  rv = Assert(downloadItem, gNC_File, fileResource, PR_TRUE);
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsIRDFRemoteDataSource> remote(do_QueryInterface(mInner));
  rv = remote->Flush();
  if (NS_FAILED(rv)) return rv;

  if (!mCurrDownloadItems)
    mCurrDownloadItems = new nsHashtable();

  nsCStringKey key(filePath);
  if (mCurrDownloadItems->Exists(&key)) {
    DownloadItem* download = NS_STATIC_CAST(DownloadItem*, mCurrDownloadItems->Get(&key));
    if (download)
      delete download;
  }
  mCurrDownloadItems->Put(&key, aDownloadItem);
  return rv;
}

NS_IMETHODIMP
nsDownloadManager::GetItem(const char* aID, nsIDownloadItem** aDownloadItem)
{
  // if it's currently downloading we can get it from the table
  // XXX otherwise we need to look for it in the datasource and
  //     create a new nsIDownloadItem with the resource's properties
  nsCStringKey key(aID);
  if (mCurrDownloadItems->Exists(&key)) {
    *aDownloadItem = NS_STATIC_CAST(nsIDownloadItem*, mCurrDownloadItems->Get(&key));
    NS_ADDREF(*aDownloadItem);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDownloadManager::InitializeUI(nsIDOMXULDocument* aDocument)
{
  mDocument = aDocument;
  gMustUpdateUI = PR_TRUE;

  // get the downloads container
  nsCOMPtr<nsIRDFContainer> downloads;
  nsresult rv = GetDownloadsContainer(getter_AddRefs(downloads));
  if (NS_FAILED(rv)) return rv;

  // get the container's elements (nsIRDFResource's)
  nsCOMPtr<nsISimpleEnumerator> items;
  rv = downloads->GetElements(getter_AddRefs(items));
  if (NS_FAILED(rv)) return rv;

  if (!mCurrDownloadItems)
    mCurrDownloadItems = new nsHashtable();

  nsCOMPtr<nsISupports> supports;
  nsCOMPtr<nsIRDFResource> res;
  nsCOMPtr<nsIRDFInt> percent;
  nsCOMPtr<nsIDownloadProgressListener> listener;
  nsCOMPtr<nsIRDFNode> oldTarget;
  char* id;
  PRInt32 percentComplete;

  // enumerate the resources, use their ids to retrieve the corresponding
  // nsIDownloadItems from the hashtable (if they don't exist, the download isn't
  // a current transfer), get the items' progress information,
  // and assert it into the graph so we can show it in the UI

  PRBool moreElements;
  items->HasMoreElements(&moreElements);
  for( ; moreElements; items->HasMoreElements(&moreElements)) {
    items->GetNext(getter_AddRefs(supports));
    res = do_QueryInterface(supports);
    res->GetValue(&id);
    nsCStringKey key(id);
    if (mCurrDownloadItems->Exists(&key)) {
      nsIDownloadItem* item = NS_STATIC_CAST(nsIDownloadItem*, mCurrDownloadItems->Get(&key));
      if (!item) continue; // must be a finished download; don't need to update ui

      // update percentage
      item->GetPercentComplete(&percentComplete);
      rv = GetTarget(res, gNC_ProgressPercent, PR_TRUE, getter_AddRefs(oldTarget));
      if (NS_FAILED(rv)) continue;

      rv = gRDFService->GetIntLiteral(percentComplete, getter_AddRefs(percent));
      if (NS_FAILED(rv)) continue;

      if (oldTarget)
        rv = Change(res, gNC_ProgressPercent, oldTarget, percent);
      else
        rv = Assert(res, gNC_ProgressPercent, percent, PR_TRUE);

      if (NS_FAILED(rv)) continue;
 
      item->GetInternalListener(getter_AddRefs(listener));
      if (listener) {
        listener->SetDocument(aDocument);
        listener->SetDownloadItem(item);
      }
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDownloadManager::UninitializeUI()
{
  gMustUpdateUI = PR_FALSE;
  mDocument = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
nsDownloadManager::NotifyDownloadEnded(const char* aTargetPath)
{
  nsCStringKey key(aTargetPath);
  if (mCurrDownloadItems->Exists(&key))
    mCurrDownloadItems->Remove(&key);

  return NS_OK;
}

nsresult
nsDownloadManager::GetProfileDownloadsFileURL(char** aDownloadsFileURL)
{
  nsCOMPtr<nsIProperties> fileLocator(do_GetService("@mozilla.org/file/directory_service;1"));
  nsCOMPtr<nsIFile> profileDir;

  // get the profile directory
  nsresult rv = fileLocator->Get(NS_APP_USER_PROFILE_50_DIR, NS_GET_IID(nsIFile), getter_AddRefs(profileDir));
  if (NS_FAILED(rv)) return rv;

  // add downloads.rdf to the path
  rv = profileDir->Append(PROFILE_DOWNLOAD_FILE);
  if (NS_FAILED(rv)) return rv;

  return NS_GetURLSpecFromFile(profileDir, aDownloadsFileURL);
}

nsresult
nsDownloadManager::GetDownloadsContainer(nsIRDFContainer** aResult)
{
  nsresult rv;

  nsCOMPtr<nsIRDFContainerUtils> rdfC(do_GetService(NS_RDF_CONTRACTID "/container-utils;1", &rv));
  if (NS_FAILED(rv)) return rv;

  PRBool isContainer;
  rv = mRDFContainerUtils->IsContainer(mInner, gNC_DownloadsRoot, &isContainer);
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsIRDFContainer> ctr;

  if (!isContainer) {
    rv = mRDFContainerUtils->MakeSeq(mInner, gNC_DownloadsRoot, getter_AddRefs(ctr));
    if (NS_FAILED(rv)) return rv;
  }
  else {
    ctr = do_CreateInstance(NS_RDF_CONTRACTID "/container;1", &rv);
    if (NS_FAILED(rv)) return rv;
    rv = ctr->Init(mInner, gNC_DownloadsRoot);
    if (NS_FAILED(rv)) return rv;
  }

  *aResult = ctr;
  NS_IF_ADDREF(*aResult);

  return rv;
}

///////////////////////////////////////////////////////////////////////////////
// nsIRDFDataSource

NS_IMETHODIMP
nsDownloadManager::GetSource(nsIRDFResource* aProperty,
                             nsIRDFNode* aTarget,
                             PRBool aTruthValue,
                             nsIRDFResource** aSource)
{
  return mInner->GetSource(aProperty, aTarget, aTruthValue, aSource);
}

NS_IMETHODIMP nsDownloadManager::GetSources(nsIRDFResource* aProperty,
                                            nsIRDFNode* aTarget,
                                            PRBool aTruthValue,
                                            nsISimpleEnumerator** aSources) {
  return mInner->GetSources(aProperty, aTarget, aTruthValue, aSources);
}

NS_IMETHODIMP nsDownloadManager::GetTarget(nsIRDFResource* aSource,
                                           nsIRDFResource* aProperty,
                                           PRBool aTruthValue,
                                           nsIRDFNode** aTarget) {
  return mInner->GetTarget(aSource, aProperty, aTruthValue, aTarget);
}

NS_IMETHODIMP nsDownloadManager::GetTargets(nsIRDFResource* aSource,
                                            nsIRDFResource* aProperty,
                                            PRBool aTruthValue,
                                            nsISimpleEnumerator** aTargets) {
  return mInner->GetTargets(aSource, aProperty, aTruthValue, aTargets);
}

NS_IMETHODIMP
nsDownloadManager::Assert(nsIRDFResource* aSource,
                          nsIRDFResource* aProperty,
                          nsIRDFNode* aTarget,
                          PRBool aTruthValue)
{
  return mInner->Assert(aSource, aProperty, aTarget, aTruthValue);
}

NS_IMETHODIMP
nsDownloadManager::Unassert(nsIRDFResource* aSource,
                            nsIRDFResource* aProperty,
                            nsIRDFNode* aTarget)
{
  return mInner->Unassert(aSource, aProperty, aTarget);
}

NS_IMETHODIMP
nsDownloadManager::Change(nsIRDFResource* aSource,
                          nsIRDFResource* aProperty,
                          nsIRDFNode* aOldTarget,
                          nsIRDFNode* aNewTarget)
{
  return mInner->Change(aSource, aProperty, aOldTarget, aNewTarget);
}

NS_IMETHODIMP
nsDownloadManager::Move(nsIRDFResource* aOldSource,
                        nsIRDFResource* aNewSource,
                        nsIRDFResource* aProperty,
                        nsIRDFNode* aTarget)
{
  return mInner->Move(aOldSource, aNewSource, aProperty, aTarget);
}

NS_IMETHODIMP
nsDownloadManager::HasAssertion(nsIRDFResource* aSource,
                                nsIRDFResource* aProperty,
                                nsIRDFNode* aTarget,
                                PRBool aTruthValue,
                                PRBool* hasAssertion)
{
  return mInner->HasAssertion(aSource, aProperty, aTarget, aTruthValue, hasAssertion);
}

NS_IMETHODIMP
nsDownloadManager::AddObserver(nsIRDFObserver* aObserver)
{
  return mInner->AddObserver(aObserver);
}

NS_IMETHODIMP
nsDownloadManager::RemoveObserver(nsIRDFObserver* aObserver)
{
  return mInner->RemoveObserver(aObserver);
}

NS_IMETHODIMP
nsDownloadManager::HasArcIn(nsIRDFNode* aNode,
                            nsIRDFResource* aArc, PRBool* _retval)
{
  return mInner->HasArcIn(aNode, aArc, _retval);
}

NS_IMETHODIMP
nsDownloadManager::HasArcOut(nsIRDFResource* aSource,
                             nsIRDFResource* aArc, PRBool* _retval)
{
  return mInner->HasArcOut(aSource, aArc, _retval);
}

NS_IMETHODIMP
nsDownloadManager::ArcLabelsIn(nsIRDFNode* aNode,
                               nsISimpleEnumerator** aLabels)
{
  return mInner->ArcLabelsIn(aNode, aLabels);
}

NS_IMETHODIMP
nsDownloadManager::ArcLabelsOut(nsIRDFResource* aSource,
                                nsISimpleEnumerator** aLabels)
{
  return mInner->ArcLabelsIn(aSource, aLabels);
}

NS_IMETHODIMP
nsDownloadManager::GetAllResources(nsISimpleEnumerator** aResult)
{
  return mInner->GetAllResources(aResult);
}

NS_IMETHODIMP
nsDownloadManager::GetAllCommands(nsIRDFResource* aSource,
                                  nsIEnumerator** aCommands)
{
  return mInner->GetAllCommands(aSource, aCommands);
}

NS_IMETHODIMP
nsDownloadManager::GetAllCmds(nsIRDFResource* aSource,
                              nsISimpleEnumerator** aCommands)
{
  return mInner->GetAllCmds(aSource, aCommands);
}

NS_IMETHODIMP
nsDownloadManager::IsCommandEnabled(nsISupportsArray* aSources,
                                    nsIRDFResource* aCommand,
                                    nsISupportsArray* aArguments,
                                    PRBool* aResult)
{
  return mInner->IsCommandEnabled(aSources, aCommand, aArguments, aResult);
}

NS_IMETHODIMP
nsDownloadManager::DoCommand(nsISupportsArray* aSources,
                             nsIRDFResource* aCommand,
                             nsISupportsArray* aArguments)
{
  return mInner->DoCommand(aSources, aCommand, aArguments);
}

///////////////////////////////////////////////////////////////////////////////
// nsIRDFRemoteDataSource

NS_IMETHODIMP
nsDownloadManager::GetLoaded(PRBool* aResult)
{
  nsCOMPtr<nsIRDFRemoteDataSource> remote(do_QueryInterface(mInner));
  return remote->GetLoaded(aResult);
}

NS_IMETHODIMP
nsDownloadManager::Init(const char* aURI)
{
  return NS_OK;
}

NS_IMETHODIMP
nsDownloadManager::Refresh(PRBool aBlocking)
{
  nsCOMPtr<nsIRDFRemoteDataSource> remote(do_QueryInterface(mInner));
  return remote->Refresh(aBlocking);
}

NS_IMETHODIMP
nsDownloadManager::Flush()
{
  nsCOMPtr<nsIRDFRemoteDataSource> remote(do_QueryInterface(mInner));
  return remote->Flush();
}

///////////////////////////////////////////////////////////////////////////////
// DownloadItem

DownloadItem::DownloadItem()
{
  NS_INIT_ISUPPORTS();
  NS_INIT_REFCNT();
}

// for convenience
DownloadItem::DownloadItem(const PRUnichar* aPrettyName, nsILocalFile* aTarget, nsIURI* aSource)
:mPrettyName(aPrettyName), mTarget(aTarget), mSource(aSource)
{
  NS_INIT_ISUPPORTS();
  NS_INIT_REFCNT();

}

DownloadItem::~DownloadItem()
{
  UpdateProgressInfo();
}

nsresult
DownloadItem::UpdateProgressInfo()
{
  nsCOMPtr<nsIRDFDataSource> ds;

  nsresult rv = gRDFService->GetDataSource("rdf:downloads", getter_AddRefs(ds));
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsIRDFNode> oldTarget;
  rv = ds->GetTarget(mDownloadItem, gNC_ProgressPercent, PR_TRUE, getter_AddRefs(oldTarget));
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsIRDFInt> percent;
  rv = gRDFService->GetIntLiteral(mPercentComplete, getter_AddRefs(percent));

  if (NS_FAILED(rv)) return rv;

  if (oldTarget)
    rv = ds->Change(mDownloadItem, gNC_ProgressPercent, oldTarget, percent);
  else
    rv = ds->Assert(mDownloadItem, gNC_ProgressPercent, percent, PR_TRUE);

  // XXX Store elapsed time here eventually

  return rv;
}
NS_IMPL_ISUPPORTS2(DownloadItem, nsIWebProgressListener, nsIDownloadItem)

///////////////////////////////////////////////////////////////////////////////
// nsIWebProgressListener

NS_IMETHODIMP
DownloadItem::OnProgressChange(nsIWebProgress *aWebProgress,
                               nsIRequest *aRequest,
                               PRInt32 aCurSelfProgress,
                               PRInt32 aMaxSelfProgress,
                               PRInt32 aCurTotalProgress,
                               PRInt32 aMaxTotalProgress)
{
  mPercentComplete = aCurTotalProgress * 100 / aMaxTotalProgress;

  if (mListener) {
    mListener->OnProgressChange(aWebProgress, aRequest, aCurSelfProgress, aMaxSelfProgress,
                                aCurTotalProgress, aMaxTotalProgress);
  }

  if (gMustUpdateUI && mInternalListener) {
    mInternalListener->OnProgressChange(aWebProgress, aRequest, aCurSelfProgress, aMaxSelfProgress,
                                        aCurTotalProgress, aMaxTotalProgress);
  }

  if (mPropertiesListener) {
    mPropertiesListener->OnProgressChange(aWebProgress, aRequest, aCurSelfProgress, aMaxSelfProgress,
                                          aCurTotalProgress, aMaxTotalProgress);
  }

  return NS_OK;
}

NS_IMETHODIMP
DownloadItem::OnLocationChange(nsIWebProgress *aWebProgress,
                               nsIRequest *aRequest, nsIURI *aLocation)
{
  if (mListener)
    mListener->OnLocationChange(aWebProgress, aRequest, aLocation);

  if (gMustUpdateUI && mInternalListener)
    mInternalListener->OnLocationChange(aWebProgress, aRequest, aLocation);

  if (mPropertiesListener)
    mPropertiesListener->OnLocationChange(aWebProgress, aRequest, aLocation);

  return NS_OK;
}

NS_IMETHODIMP
DownloadItem::OnStatusChange(nsIWebProgress *aWebProgress,
                             nsIRequest *aRequest, nsresult aStatus,
                             const PRUnichar *aMessage)
{
  if (mListener)
    mListener->OnStatusChange(aWebProgress, aRequest, aStatus, aMessage);

  if (gMustUpdateUI && mInternalListener)
    mInternalListener->OnStatusChange(aWebProgress, aRequest, aStatus, aMessage);

  if (mPropertiesListener)
    mPropertiesListener->OnStatusChange(aWebProgress, aRequest, aStatus, aMessage);

  return NS_OK;
}

NS_IMETHODIMP
DownloadItem::OnStateChange(nsIWebProgress* aWebProgress,
                            nsIRequest* aRequest, PRInt32 aStateFlags,
                            PRUint32 aStatus)
{
  if (aStateFlags & STATE_START)
    mTimeStarted = PR_Now();
  else if (aStateFlags & STATE_STOP) {
    nsresult rv;
    nsCOMPtr<nsIDownloadManager> downloadManager = do_GetService("@mozilla.org/download-manager;1", &rv);
    if (NS_FAILED(rv)) return rv;
    
    char* path;
    mTarget->GetPath(&path);
    downloadManager->NotifyDownloadEnded(path);
  }


  if (mListener)
    mListener->OnStateChange(aWebProgress, aRequest, aStateFlags, aStatus);

  if (gMustUpdateUI && mInternalListener)
    mInternalListener->OnStateChange(aWebProgress, aRequest, aStateFlags, aStatus);

  if (mPropertiesListener)
    mPropertiesListener->OnStateChange(aWebProgress, aRequest, aStateFlags, aStatus);

  return NS_OK;
}

NS_IMETHODIMP
DownloadItem::OnSecurityChange(nsIWebProgress *aWebProgress,
                               nsIRequest *aRequest, PRInt32 aState)
{
  if (mListener)
    mListener->OnSecurityChange(aWebProgress, aRequest, aState);

  if (gMustUpdateUI && mInternalListener)
    mInternalListener->OnSecurityChange(aWebProgress, aRequest, aState);

  if (mPropertiesListener)
    mPropertiesListener->OnSecurityChange(aWebProgress, aRequest, aState);

  return NS_OK;
}

///////////////////////////////////////////////////////////////////////////////
// nsIDownloadItem

NS_IMETHODIMP
DownloadItem::GetPrettyName(PRUnichar** aPrettyName)
{
  *aPrettyName = ToNewUnicode(mPrettyName);
  return NS_OK;
}

NS_IMETHODIMP
DownloadItem::SetPrettyName(const PRUnichar* aPrettyName)
{
  mPrettyName = aPrettyName;
  return NS_OK;
}

NS_IMETHODIMP
DownloadItem::GetTarget(nsILocalFile** aTarget)
{
  *aTarget = mTarget;
  NS_ADDREF(*aTarget);
  return NS_OK;
}

NS_IMETHODIMP
DownloadItem::SetTarget(nsILocalFile* aTarget)
{
  mTarget = aTarget;
  return NS_OK;
}

NS_IMETHODIMP
DownloadItem::GetSource(nsIURI** aSource)
{
  *aSource = mSource;
  NS_ADDREF(*aSource);
  return NS_OK;
}

NS_IMETHODIMP
DownloadItem::SetSource(nsIURI* aSource)
{
  mSource = aSource;
  return NS_OK;
}

NS_IMETHODIMP
DownloadItem::GetTimeStarted(PRInt64* aTimeStarted)
{
  *aTimeStarted = mTimeStarted;
  return NS_OK;
}

NS_IMETHODIMP
DownloadItem::GetPercentComplete(PRInt32* aPercentComplete)
{
  *aPercentComplete = mPercentComplete;
  return NS_OK;
}

NS_IMETHODIMP
DownloadItem::SetListener(nsIWebProgressListener* aListener)
{
  mListener = aListener;
  return NS_OK;
}

NS_IMETHODIMP
DownloadItem::GetListener(nsIWebProgressListener** aListener)
{
  *aListener = mListener;
  NS_ADDREF(*aListener);
  return NS_OK;
}

NS_IMETHODIMP
DownloadItem::SetInternalListener(nsIDownloadProgressListener* aInternalListener)
{
  mInternalListener = aInternalListener;
  return NS_OK;
}

NS_IMETHODIMP
DownloadItem::GetInternalListener(nsIDownloadProgressListener** aInternalListener)
{
  *aInternalListener = mInternalListener;
  NS_ADDREF(*aInternalListener);
  return NS_OK;
}

NS_IMETHODIMP
DownloadItem::SetPropertiesListener(nsIWebProgressListener* aPropertiesListener)
{
  mPropertiesListener = aPropertiesListener;
  return NS_OK;
}

NS_IMETHODIMP
DownloadItem::GetPropertiesListener(nsIWebProgressListener** aPropertiesListener)
{
  *aPropertiesListener = mPropertiesListener;
  NS_ADDREF(*aPropertiesListener);
  return NS_OK;
}
