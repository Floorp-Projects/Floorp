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
 *   Blake Ross <blaker@netscape.com>
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
#include "nsIDOMWindow.h"
#include "nsIDOMWindowInternal.h"
#include "nsIDOMEvent.h"
#include "nsIDOMEventTarget.h"
#include "nsRDFCID.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsIWebBrowserPersist.h"
#include "nsIObserver.h"
#include "nsIProgressDialog.h"
#include "nsIWindowWatcher.h"

/* Outstanding issues/todo:
 * 1. Using the target path as an identifier is not sufficient because it's not unique on mac.
 * 2. Implement pause/resume.
 */
  
static NS_DEFINE_CID(kRDFServiceCID, NS_RDFSERVICE_CID);
static NS_DEFINE_CID(kRDFXMLDataSourceCID, NS_RDFXMLDATASOURCE_CID);

#define PROFILE_DOWNLOAD_FILE "downloads.rdf"
#define DOWNLOAD_MANAGER_URL "chrome://communicator/content/downloadmanager/downloadmanager.xul"

nsIRDFResource* gNC_DownloadsRoot;
nsIRDFResource* gNC_File;
nsIRDFResource* gNC_URL;
nsIRDFResource* gNC_Name;
nsIRDFResource* gNC_ProgressPercent;

nsIRDFService* gRDFService;

///////////////////////////////////////////////////////////////////////////////
// nsDownloadManager

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

  rv = remote->Refresh(PR_FALSE);
  if (NS_FAILED(rv)) return rv;

  mListener = do_CreateInstance("@mozilla.org/download-manager/listener;1", &rv);
  if (NS_FAILED(rv)) return rv;

  return gRDFService->RegisterDataSource(this, PR_FALSE);
}

nsresult
nsDownloadManager::DownloadFinished(const char* aKey)
{
  nsCStringKey key(aKey);
  if (mCurrDownloadItems->Exists(&key)) {
    DownloadItem* item = NS_STATIC_CAST(DownloadItem*, mCurrDownloadItems->Get(&key));
    PRInt32 percentComplete;
    item->GetPercentComplete(&percentComplete);

    nsCOMPtr<nsIRDFResource> res;
    nsCOMPtr<nsIRDFNode> oldTarget;
    gRDFService->GetResource(aKey, getter_AddRefs(res));
    nsresult rv = GetTarget(res, gNC_ProgressPercent, PR_TRUE, getter_AddRefs(oldTarget));
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIRDFInt> percent;
    rv = gRDFService->GetIntLiteral(percentComplete, getter_AddRefs(percent));
    if (NS_FAILED(rv)) return rv;

    if (oldTarget)
      rv = Change(res, gNC_ProgressPercent, oldTarget, percent);
    else
      rv = Assert(res, gNC_ProgressPercent, percent, PR_TRUE);

    mCurrDownloadItems->Remove(&key);
  }

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

nsresult
nsDownloadManager::GetInternalListener(nsIDownloadProgressListener** aInternalListener)
{
  *aInternalListener = mListener;
  NS_IF_ADDREF(*aInternalListener);
  return NS_OK;
}

nsresult
nsDownloadManager::AssertProgressInfo()
{
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
  nsCOMPtr<nsIRDFNode> oldTarget;
  char* id;
  PRInt32 percentComplete;

  // enumerate the resources, use their ids to retrieve the corresponding
  // nsIDownloadItems from the hashtable (if they don't exist, the download isn't
  // a current transfer), get the items' progress information,
  // and assert it into the graph

  PRBool moreElements;
  items->HasMoreElements(&moreElements);
  for( ; moreElements; items->HasMoreElements(&moreElements)) {
    items->GetNext(getter_AddRefs(supports));
    res = do_QueryInterface(supports);
    res->GetValue(&id);
    nsCStringKey key(id);
    if (mCurrDownloadItems->Exists(&key)) { // if not, must be a finished download; don't need to update info
      nsIDownloadItem* item = NS_STATIC_CAST(nsIDownloadItem*, mCurrDownloadItems->Get(&key));

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
    }
  }
  return rv;
}

///////////////////////////////////////////////////////////////////////////////
// nsIDownloadManager

NS_IMETHODIMP
nsDownloadManager::AddDownload(nsIDownloadItem* aDownloadItem)
{
  NS_ENSURE_ARG_POINTER(aDownloadItem);

  nsCOMPtr<nsIRDFContainer> downloads;
  nsresult rv = GetDownloadsContainer(getter_AddRefs(downloads));
  if (NS_FAILED(rv)) return rv;

  DownloadItem* item = NS_STATIC_CAST(DownloadItem*, aDownloadItem);

  item->SetDownloadManager(this);

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
  
  if (!mCurrDownloadItems)
    mCurrDownloadItems = new nsHashtable();
  
  nsCStringKey key(filePath);
  if (mCurrDownloadItems->Exists(&key)) {
    DownloadItem* download = NS_STATIC_CAST(DownloadItem*, mCurrDownloadItems->Get(&key));
    if (download)
      delete download;
  }
  mCurrDownloadItems->Put(&key, aDownloadItem);

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
  
  // if a persist object was specified, set the download item as the progress listener
  nsCOMPtr<nsIWebBrowserPersist> persist;
  aDownloadItem->GetPersist(getter_AddRefs(persist));
  if (persist) {
    nsCOMPtr<nsIWebProgressListener> progressListener = do_QueryInterface(aDownloadItem);
    persist->SetProgressListener(progressListener);
  }
  return rv;
}

NS_IMETHODIMP
nsDownloadManager::GetDownload(const char* aKey, nsIDownloadItem** aDownloadItem)
{
  // if it's currently downloading we can get it from the table
  // XXX otherwise we should look for it in the datasource and
  //     create a new nsIDownloadItem with the resource's properties
  nsCStringKey key(aKey);
  if (mCurrDownloadItems->Exists(&key)) {
    *aDownloadItem = NS_STATIC_CAST(nsIDownloadItem*, mCurrDownloadItems->Get(&key));
    NS_ADDREF(*aDownloadItem);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDownloadManager::CancelDownload(const char* aKey)
{
  nsresult rv = NS_OK;
  nsCStringKey key(aKey);
  if (mCurrDownloadItems->Exists(&key)) {
    nsIDownloadItem* item = NS_STATIC_CAST(nsIDownloadItem*, mCurrDownloadItems->Get(&key));

    // if a persist was provided, we can do the cancel ourselves.
    nsCOMPtr<nsIWebBrowserPersist> persist;
    item->GetPersist(getter_AddRefs(persist));
    if (persist)
      rv = persist->CancelSave();

    // if an observer was provided, notify that the download was cancelled.
    // if no persist was provided, this is necessary so that whatever transfer
    // component being used can cancel the download itself.
    nsCOMPtr<nsIObserver> observer;
    item->GetObserver(getter_AddRefs(observer));
    if (observer)
      rv = observer->Observe(item, "oncancel", nsnull);

    mCurrDownloadItems->Remove(&key);
  }
  return rv;
}

NS_IMETHODIMP
nsDownloadManager::RemoveDownload(const char* aKey)
{
  nsresult rv;
  nsCStringKey key(aKey);
  
  // RemoveDownload is for downloads not currently in progress. Having it
  // cancel in-progress downloads would make things complicated, so just return.
  PRBool inProgress = mCurrDownloadItems->Exists(&key);
  NS_ASSERTION(!inProgress, "Can't call RemoveDownload on a download in progress!");
  if (inProgress)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIRDFContainer> downloads;
  rv = GetDownloadsContainer(getter_AddRefs(downloads));
  if (NS_FAILED(rv)) return rv;
  
  nsCOMPtr<nsIRDFResource> res;
  gRDFService->GetResource(aKey, getter_AddRefs(res));

  PRInt32 itemIndex;
  downloads->IndexOf(res, &itemIndex);
  if (itemIndex <= 0)
    return NS_ERROR_FAILURE;
  
  nsCOMPtr<nsIRDFNode> node;
  rv = downloads->RemoveElementAt(itemIndex, PR_TRUE, getter_AddRefs(node));
  if (NS_FAILED(rv)) return rv;

  return Flush(); // necessary?
}  

NS_IMETHODIMP
nsDownloadManager::Open(nsIDOMWindow* aParent)
{

  // first assert new progress info so the ui is correctly updated
  AssertProgressInfo();
  
  // if we ever have the capability to display the UI of third party dl managers,
  // we'll open their UI here instead.
  nsresult rv;
  nsCOMPtr<nsIWindowWatcher> ww = do_GetService("@mozilla.org/embedcomp/window-watcher;1", &rv);
  if (NS_FAILED(rv)) return rv;
  
  nsCOMPtr<nsIDOMWindow> newWindow;
  rv = ww->OpenWindow(aParent,
                      DOWNLOAD_MANAGER_URL,
                      "_blank",
                      "chrome,titlebar,dependent,centerscreen",
                      nsnull,
                      getter_AddRefs(newWindow));

  if (NS_FAILED(rv)) return rv;

  // XXX whether or not mDocument is null is not a sufficient flag,
  // because in the future we may support using a third party download manager that
  // doesn't use our architecture

  nsCOMPtr<nsIDOMEventTarget> target = do_QueryInterface(newWindow);
  if (!target) return NS_ERROR_FAILURE;
  
  rv = target->AddEventListener(NS_LITERAL_STRING("load"), this, PR_FALSE);
  if (NS_FAILED(rv)) return rv;
 
  return target->AddEventListener(NS_LITERAL_STRING("unload"), this, PR_FALSE);
}

NS_IMETHODIMP
nsDownloadManager::OpenProgressDialogFor(const char* aKey, nsIDOMWindow* aParent)
{
  nsresult rv;
  nsCStringKey key(aKey);
  if (!mCurrDownloadItems->Exists(&key))
    return NS_ERROR_FAILURE;

  nsIDownloadItem* item = NS_STATIC_CAST(nsIDownloadItem*, mCurrDownloadItems->Get(&key));
  nsCOMPtr<nsIProgressDialog> dialog(do_CreateInstance("@mozilla.org/progressdialog;1", &rv));
  if (NS_FAILED(rv)) return rv;
  
  // now give the dialog the necessary context
  
  // start time...
  PRInt64 startTime = 0;
  item->GetStartTime(&startTime);
  if (startTime) // possible not to have a start time yet if the dialog was requested immediately
    dialog->SetStartTime(startTime);
  
  // source...
  nsCOMPtr<nsIURI> uri;
  item->GetSource(getter_AddRefs(uri));
  dialog->SetSource(uri);
  
  // target...
  nsCOMPtr<nsILocalFile> target;
  item->GetTarget(getter_AddRefs(target));
  dialog->SetTarget(target);

  // now set the listener so we forward notifications to the dialog
  nsCOMPtr<nsIWebProgressListener> listener = do_QueryInterface(dialog);
  (NS_STATIC_CAST(DownloadItem*, item))->SetDialogListener(listener);
  
  return dialog->Open(aParent, nsnull);
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
  NS_ENSURE_ARG_POINTER(aEvent);

  // the event is either load or unload
  nsAutoString eventType;
  aEvent->GetType(eventType);
  if (eventType.Equals(NS_LITERAL_STRING("unload")))
    return OnClose();

  nsCOMPtr<nsIDOMEventTarget> target;
  nsresult rv = aEvent->GetTarget(getter_AddRefs(target));
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsIDOMNode> targetNode(do_QueryInterface(target));
  mDocument = do_QueryInterface(targetNode);
  mListener->SetDocument(mDocument);

  return NS_OK;
}

///////////////////////////////////////////////////////////////////////////////
// nsIRDFDataSource

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

NS_IMPL_ISUPPORTS2(DownloadItem, nsIWebProgressListener, nsIDownloadItem)

DownloadItem::DownloadItem():mStartTime(0)
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

nsresult
DownloadItem::SetDownloadManager(nsDownloadManager* aDownloadManager)
{
  mDownloadManager = aDownloadManager;
  return NS_OK;
}

nsresult
DownloadItem::SetDialogListener(nsIWebProgressListener* aDialogListener)
{
  mDialogListener = aDialogListener;
  return NS_OK;
}

nsresult
DownloadItem::GetDialogListener(nsIWebProgressListener** aDialogListener)
{
  *aDialogListener = mDialogListener;
  NS_IF_ADDREF(*aDialogListener);
  return NS_OK;
}

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

  if (mDownloadManager->MustUpdateUI()) {
    nsCOMPtr<nsIDownloadProgressListener> internalListener;
    mDownloadManager->GetInternalListener(getter_AddRefs(internalListener));
    if (internalListener)
      internalListener->OnLocationChange(aWebProgress, aRequest, aLocation, this);
  }

  if (mDialogListener)
    mDialogListener->OnLocationChange(aWebProgress, aRequest, aLocation);

  return NS_OK;
}

NS_IMETHODIMP
DownloadItem::OnStatusChange(nsIWebProgress *aWebProgress,
                             nsIRequest *aRequest, nsresult aStatus,
                             const PRUnichar *aMessage)
{
  if (mListener)
    mListener->OnStatusChange(aWebProgress, aRequest, aStatus, aMessage);

  if (mDownloadManager->MustUpdateUI()) {
    nsCOMPtr<nsIDownloadProgressListener> internalListener;
    mDownloadManager->GetInternalListener(getter_AddRefs(internalListener));
    if (internalListener)
      internalListener->OnStatusChange(aWebProgress, aRequest, aStatus, aMessage, this);
  }

  if (mDialogListener)
    mDialogListener->OnStatusChange(aWebProgress, aRequest, aStatus, aMessage);

  return NS_OK;
}

NS_IMETHODIMP
DownloadItem::OnStateChange(nsIWebProgress* aWebProgress,
                            nsIRequest* aRequest, PRInt32 aStateFlags,
                            PRUint32 aStatus)
{
  if (aStateFlags & STATE_START) {
    mStartTime = PR_Now();
    mRequest = aRequest; // used for pause/resume
  }
  else if (aStateFlags & STATE_STOP) {
    char* path;
    mTarget->GetPath(&path);
    mDownloadManager->DownloadFinished(path);
  }

  if (mListener)
    mListener->OnStateChange(aWebProgress, aRequest, aStateFlags, aStatus);

  if (mDownloadManager->MustUpdateUI()) {
    nsCOMPtr<nsIDownloadProgressListener> internalListener;
    mDownloadManager->GetInternalListener(getter_AddRefs(internalListener));
    if (internalListener)
      internalListener->OnStateChange(aWebProgress, aRequest, aStateFlags, aStatus, this);
  }

  if (mDialogListener)
    mDialogListener->OnStateChange(aWebProgress, aRequest, aStateFlags, aStatus);

  return NS_OK;
}

NS_IMETHODIMP
DownloadItem::OnSecurityChange(nsIWebProgress *aWebProgress,
                               nsIRequest *aRequest, PRInt32 aState)
{
  if (mListener)
    mListener->OnSecurityChange(aWebProgress, aRequest, aState);

  if (mDownloadManager->MustUpdateUI()) {
    nsCOMPtr<nsIDownloadProgressListener> internalListener;
    mDownloadManager->GetInternalListener(getter_AddRefs(internalListener));
    if (internalListener)
      internalListener->OnSecurityChange(aWebProgress, aRequest, aState, this);
  }

  if (mDialogListener)
    mDialogListener->OnSecurityChange(aWebProgress, aRequest, aState);

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
  NS_IF_ADDREF(*aTarget);
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
  NS_IF_ADDREF(*aSource);
  return NS_OK;
}

NS_IMETHODIMP
DownloadItem::SetSource(nsIURI* aSource)
{
  mSource = aSource;
  return NS_OK;
}

NS_IMETHODIMP
DownloadItem::GetStartTime(PRInt64* aStartTime)
{
  *aStartTime = mStartTime;
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
  NS_IF_ADDREF(*aListener);
  return NS_OK;
}

NS_IMETHODIMP
DownloadItem::SetPersist(nsIWebBrowserPersist* aPersist)
{
  mPersist = aPersist;
  return NS_OK;
}

NS_IMETHODIMP
DownloadItem::GetPersist(nsIWebBrowserPersist** aPersist)
{
  *aPersist = mPersist;
  NS_IF_ADDREF(*aPersist);
  return NS_OK;
}

NS_IMETHODIMP
DownloadItem::SetObserver(nsIObserver* aObserver)
{
  mObserver = aObserver;
  return NS_OK;
}

NS_IMETHODIMP
DownloadItem::GetObserver(nsIObserver** aObserver)
{
  *aObserver = mObserver;
  NS_IF_ADDREF(*aObserver);
  return NS_OK;
}