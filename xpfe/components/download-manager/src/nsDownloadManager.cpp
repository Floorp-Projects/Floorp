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
 *   Blake Ross <blakeross@telocity.com>
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

#include "nsIServiceManager.h"
#include "nsIWebProgress.h"

#include "nsIStringBundle.h"
#include "nsIRDFLiteral.h"
#include "nsIRDFXMLSerializer.h"
#include "nsIRDFXMLSource.h"
#include "rdf.h"
#include "nsRDFCID.h"
#include "nsCRT.h"
#include "nsString.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsFileSpec.h"
#include "nsFileStream.h"

static NS_DEFINE_CID(kRDFServiceCID, NS_RDFSERVICE_CID);
static NS_DEFINE_CID(kRDFXMLDataSourceCID, NS_RDFXMLDATASOURCE_CID);

#define PROFILE_DOWNLOAD_FILE "downloads.rdf"
#define NSDOWNLOADMANAGER_PROPERTIES_URI "chrome://communicator/locale/downloadmanager/downloadmanager.properties"

nsIRDFResource* gNC_DownloadsRoot;
nsIRDFResource* gNC_File;
nsIRDFResource* gNC_URL;
nsIRDFResource* gNC_Name;
nsIRDFResource* gNC_Progress;
nsIRDFResource* gNC_ProgressPercent;

nsIRDFService* gRDFService;

NS_IMPL_ISUPPORTS3(nsDownloadManager, nsIDownloadManager, nsIRDFDataSource, nsIRDFRemoteDataSource)

nsDownloadManager::nsDownloadManager() : mDownloadItems(nsnull)
{
  NS_INIT_ISUPPORTS();
}

nsDownloadManager::~nsDownloadManager()
{
  gRDFService->UnregisterDataSource(this);

  NS_IF_RELEASE(gNC_DownloadsRoot);
  NS_IF_RELEASE(gNC_File);
  NS_IF_RELEASE(gNC_URL);
  NS_IF_RELEASE(gNC_Name);
  NS_IF_RELEASE(gNC_Progress);
  NS_IF_RELEASE(gNC_ProgressPercent);

  nsServiceManager::ReleaseService(kRDFServiceCID, gRDFService);
  gRDFService = nsnull;

  delete mDownloadItems;
  mDownloadItems = nsnull;
}

nsresult
nsDownloadManager::Init()
{
  nsresult rv;

  rv = nsServiceManager::GetService(kRDFServiceCID, NS_GET_IID(nsIRDFService), 
                                    (nsISupports**) &gRDFService);
  if (NS_FAILED(rv)) return rv;

  mRDFContainerUtils = do_GetService("@mozilla.org/rdf/container-utils;1", &rv);
  if (NS_FAILED(rv)) return rv;

  gRDFService->GetResource("NC:DownloadsRoot", &gNC_DownloadsRoot);
  gRDFService->GetResource(NC_NAMESPACE_URI "File", &gNC_File);
  gRDFService->GetResource(NC_NAMESPACE_URI "URL", &gNC_URL);
  gRDFService->GetResource(NC_NAMESPACE_URI "Name", &gNC_Name);
  gRDFService->GetResource(NC_NAMESPACE_URI "Progress", &gNC_Progress);
  gRDFService->GetResource(NC_NAMESPACE_URI "ProgressPercent", &gNC_ProgressPercent);

#if 0
  mInner = do_GetService(NS_RDF_DATASOURCE_CONTRACTID_PREFIX "in-memory-datasource", &rv);
  if (NS_FAILED(rv)) return rv;
#else
  mInner = do_GetService(kRDFXMLDataSourceCID, &rv);
  if (NS_FAILED(rv)) return rv;

  // This should move elsewhere
  nsXPIDLCString downloadsDB;
  GetProfileDownloadsFileURL(getter_Copies(downloadsDB));

  nsCOMPtr<nsIRDFRemoteDataSource> remote(do_QueryInterface(mInner));
  rv = remote->Init(downloadsDB);
  if (NS_FAILED(rv)) return rv;

  rv = remote->Refresh(PR_TRUE);
  if (NS_FAILED(rv)) return rv;
#endif

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
nsDownloadManager::AddItem(const PRUnichar* aDisplayName, nsIURI* aSourceURI,
                           nsILocalFile* aLocalFile, const char* aParentID, nsIWebProgress* aProgress)
{
  nsresult rv;

  nsCOMPtr<nsIRDFContainer> downloads;
  GetDownloadsContainer(getter_AddRefs(downloads));

  nsXPIDLCString filePath;
  aLocalFile->GetPath(getter_Copies(filePath));

  nsCOMPtr<nsIRDFResource> downloadItem;
  gRDFService->GetResource(filePath, getter_AddRefs(downloadItem));

  PRInt32 itemIndex;
  downloads->IndexOf(downloadItem, &itemIndex);
  if (itemIndex > 0) {
    nsCOMPtr<nsIRDFNode> node;
    downloads->RemoveElementAt(itemIndex, PR_TRUE, getter_AddRefs(node));
  }
   
  downloads->AppendElement(downloadItem);

  // NC:Name 
  nsAutoString displayName; displayName.Assign(aDisplayName);
  if (displayName.IsEmpty()) {
    nsXPIDLString unicodeDisplayName;
    aLocalFile->GetUnicodeLeafName(getter_Copies(unicodeDisplayName));
    displayName.Assign(unicodeDisplayName);
  }

  nsCOMPtr<nsIRDFLiteral> nameLiteral;
  gRDFService->GetLiteral(displayName.get(), getter_AddRefs(nameLiteral));
  Assert(downloadItem, gNC_Name, nameLiteral, PR_TRUE);

  // NC:URL
  nsXPIDLCString spec;
  aSourceURI->GetSpec(getter_Copies(spec));

  nsCOMPtr<nsIRDFResource> urlResource;
  gRDFService->GetResource(spec, getter_AddRefs(urlResource));
  Assert(downloadItem, gNC_URL, urlResource, PR_TRUE);

  // NC:File
  nsCOMPtr<nsIRDFResource> fileResource;
  gRDFService->GetResource(filePath, getter_AddRefs(fileResource));
  Assert(downloadItem, gNC_File, fileResource, PR_TRUE);

  nsCOMPtr<nsIRDFRemoteDataSource> remote(do_QueryInterface(mInner));
  rv = remote->Flush();
  if (NS_FAILED(rv)) return rv;

  DownloadItem* item = new DownloadItem();
  if (!item) return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(item);

  if (!mDownloadItems)
    mDownloadItems = new nsHashtable();
  
  nsCStringKey key(filePath);
  if (mDownloadItems->Exists(&key)) {
    DownloadItem* download = (DownloadItem*)mDownloadItems->Get(&key);
    if (download)
      delete download;
  }
  mDownloadItems->Put(&key, item);

  rv = item->Init(downloadItem, this, aSourceURI, nsnull, aLocalFile);
  if (NS_FAILED(rv)) return rv;

  return rv;
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

nsresult
nsDownloadManager::GetProfileDownloadsFileURL(char** aDownloadsFileURL)
{
  nsresult rv;

  nsCOMPtr<nsIProperties> fileLocator(do_GetService("@mozilla.org/file/directory_service;1"));
  nsCOMPtr<nsIFile> profileDir;
  rv = fileLocator->Get(NS_APP_USER_PROFILE_50_DIR, NS_GET_IID(nsIFile), getter_AddRefs(profileDir));
  if (NS_FAILED(rv)) return rv;

  rv = profileDir->Append(PROFILE_DOWNLOAD_FILE);
  if (NS_FAILED(rv)) return rv;

  return profileDir->GetURL(aDownloadsFileURL);
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
//
DownloadItem::DownloadItem() 
{
}

DownloadItem::~DownloadItem()
{
  UpdateProgressInfo();
}

nsresult
DownloadItem::UpdateProgressInfo()
{
  nsresult rv;

  nsCOMPtr<nsIStringBundleService> sbs(do_GetService(NS_STRINGBUNDLE_CONTRACTID, &rv));
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsIStringBundle> bundle;
  rv = sbs->CreateBundle(NSDOWNLOADMANAGER_PROPERTIES_URI, getter_AddRefs(bundle));
  if (NS_FAILED(rv)) return rv;

  nsAutoString key(NS_LITERAL_STRING("progressFormat"));
  nsAutoString curTotalProgressStr; curTotalProgressStr.AppendInt(mCurTotalProgress);
  nsAutoString maxTotalProgressStr; maxTotalProgressStr.AppendInt(mMaxTotalProgress);
  const PRUnichar* formatStrings[2] = { curTotalProgressStr.get(), maxTotalProgressStr.get() };
  
  nsXPIDLString progressString;
  rv = bundle->FormatStringFromName(key.get(), formatStrings, 2, getter_Copies(progressString));
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsIRDFLiteral> progressLiteral;
  rv = gRDFService->GetLiteral(progressString, getter_AddRefs(progressLiteral));
  if (NS_FAILED(rv)) return rv;

  rv = mDataSource->Assert(mDownloadItem, gNC_Progress, progressLiteral, PR_TRUE);
  if (NS_FAILED(rv)) return rv;

  // Store Unformatted Elapsed Time


  return rv;
}

nsresult
DownloadItem::Init(nsIRDFResource* aDownloadItem, 
                   nsIRDFDataSource* aDataSource,
                   nsIURI* aURI, nsIInputStream* aPostData, nsILocalFile* aFile)
{
  mDownloadItem = aDownloadItem;
  mDataSource = aDataSource;

  nsresult rv;

  mWebBrowserPersist = do_CreateInstance("@mozilla.org/embedding/browser/nsWebBrowserPersist;1", &rv);
  if (NS_FAILED(rv)) return rv;

  // XXX using RDF here could really kill perf. Need to investigate timing. 
  //     RDF is probably not worthwhile if we're only looking at a single
  //     view
  nsCOMPtr<nsIWebProgressListener> listener(do_QueryInterface(this));
  rv = mWebBrowserPersist->SetProgressListener(listener);
  if (NS_FAILED(rv)) return rv;

  rv = mWebBrowserPersist->SaveURI(aURI, aPostData, aFile);
  if (NS_FAILED(rv)) return rv;



  return rv;
}

///////////////////////////////////////////////////////////////////////////////
// nsIWebProgressListener
NS_IMPL_ISUPPORTS1(DownloadItem, nsIWebProgressListener)

NS_IMETHODIMP 
DownloadItem::OnProgressChange(nsIWebProgress *aWebProgress, 
                               nsIRequest *aRequest, 
                               PRInt32 aCurSelfProgress, 
                               PRInt32 aMaxSelfProgress, 
                               PRInt32 aCurTotalProgress, 
                               PRInt32 aMaxTotalProgress)
{
  if (mCurTotalProgress != aCurTotalProgress) {
    mCurTotalProgress = aCurTotalProgress;
    mMaxTotalProgress = aMaxTotalProgress;

    PRInt32 percent = (PRInt32)(mCurTotalProgress / mMaxTotalProgress) * 100;
    nsCOMPtr<nsIRDFInt> percentInt;
    gRDFService->GetIntLiteral(percent, getter_AddRefs(percentInt));
    mDataSource->Assert(mDownloadItem, gNC_ProgressPercent, percentInt, PR_TRUE);
  }  
  return NS_OK;  
}

NS_IMETHODIMP 
DownloadItem::OnLocationChange(nsIWebProgress *aWebProgress, 
                                    nsIRequest *aRequest, nsIURI *location)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP 
DownloadItem::OnStatusChange(nsIWebProgress *aWebProgress, 
                                  nsIRequest *aRequest, nsresult aStatus, 
                                  const PRUnichar *aMessage)
{
  if (aStatus & nsIWebProgressListener::STATE_START) 
    mRequestObserver->OnStartRequest(aRequest, nsnull);
  else if (aStatus & nsIWebProgressListener::STATE_STOP)
    mRequestObserver->OnStopRequest(aRequest, nsnull, aStatus);

  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
DownloadItem::OnStateChange(nsIWebProgress* aWebProgress,
                            nsIRequest* aRequest, PRInt32 aStateFlags,
                            PRUint32 aStatus)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP 
DownloadItem::OnSecurityChange(nsIWebProgress *aWebProgress, 
                                    nsIRequest *aRequest, PRInt32 state)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

