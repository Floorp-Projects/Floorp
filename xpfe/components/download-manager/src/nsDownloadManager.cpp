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
#include "nsIStringBundle.h"

/* Outstanding issues/todo:
 * 1. Implement pause/resume.
 */
  
static NS_DEFINE_CID(kRDFServiceCID, NS_RDFSERVICE_CID);
static NS_DEFINE_CID(kStringBundleServiceCID, NS_STRINGBUNDLESERVICE_CID);

#define PROFILE_DOWNLOAD_FILE "downloads.rdf"
#define DOWNLOAD_MANAGER_FE_URL "chrome://communicator/content/downloadmanager/downloadmanager.xul"
#define DOWNLOAD_MANAGER_BUNDLE "chrome://communicator/locale/downloadmanager/downloadmanager.properties"

nsCOMPtr<nsIRDFResource> gNC_DownloadsRoot;
nsCOMPtr<nsIRDFResource> gNC_File;
nsCOMPtr<nsIRDFResource> gNC_URL;
nsCOMPtr<nsIRDFResource> gNC_Name;
nsCOMPtr<nsIRDFResource> gNC_ProgressPercent;
nsCOMPtr<nsIRDFResource> gNC_Status;

nsCOMPtr<nsIRDFService> gRDFService;

///////////////////////////////////////////////////////////////////////////////
// nsDownloadManager

NS_IMPL_ISUPPORTS3(nsDownloadManager, nsIDownloadManager, nsIDOMEventListener, nsIObserver)

nsDownloadManager::nsDownloadManager() : mCurrDownloadItems(nsnull)
{
  NS_INIT_ISUPPORTS();
  NS_INIT_REFCNT();
}

nsDownloadManager::~nsDownloadManager()
{
  // write datasource to disk on shutdown
  nsCOMPtr<nsIRDFRemoteDataSource> remote = do_QueryInterface(mDataSource);
  remote->Flush();

  gRDFService->UnregisterDataSource(mDataSource);

  delete mCurrDownloadItems;
  mCurrDownloadItems = nsnull;
}

nsresult
nsDownloadManager::Init()
{
  nsresult rv;
  mRDFContainerUtils = do_GetService("@mozilla.org/rdf/container-utils;1", &rv);
  if (NS_FAILED(rv)) return rv;

  gRDFService = do_GetService("@mozilla.org/rdf/rdf-service;1", &rv);
  if (NS_FAILED(rv)) return rv;

  gRDFService->GetResource("NC:DownloadsRoot", getter_AddRefs(gNC_DownloadsRoot));
  gRDFService->GetResource(NC_NAMESPACE_URI "File", getter_AddRefs(gNC_File));
  gRDFService->GetResource(NC_NAMESPACE_URI "URL", getter_AddRefs(gNC_URL));
  gRDFService->GetResource(NC_NAMESPACE_URI "Name", getter_AddRefs(gNC_Name));
  gRDFService->GetResource(NC_NAMESPACE_URI "ProgressPercent", getter_AddRefs(gNC_ProgressPercent));
  gRDFService->GetResource(NC_NAMESPACE_URI "Status", getter_AddRefs(gNC_Status));

  nsXPIDLCString downloadsDB;
  rv = GetProfileDownloadsFileURL(getter_Copies(downloadsDB));
  if (NS_FAILED(rv)) return rv;

  rv = gRDFService->GetDataSourceBlocking(downloadsDB, getter_AddRefs(mDataSource));
  if (NS_FAILED(rv)) return rv;

  mListener = do_CreateInstance("@mozilla.org/download-manager/listener;1", &rv);
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsIStringBundleService> bundleService = do_GetService(kStringBundleServiceCID, &rv);
  if (NS_FAILED(rv)) return rv;
  
  return bundleService->CreateBundle(DOWNLOAD_MANAGER_BUNDLE, getter_AddRefs(mBundle));
}

nsresult
nsDownloadManager::DownloadEnded(const char* aPersistentDescriptor, const PRUnichar* aMessage)
{
  nsCStringKey key(aPersistentDescriptor);
  if (mCurrDownloadItems->Exists(&key)) {
    AssertProgressInfoFor(aPersistentDescriptor);
    mCurrDownloadItems->Remove(&key);
  }

  return NS_OK;
}

nsresult
nsDownloadManager::GetProfileDownloadsFileURL(char** aDownloadsFileURL)
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
  nsresult rv;

  nsCOMPtr<nsIRDFContainerUtils> rdfC(do_GetService(NS_RDF_CONTRACTID "/container-utils;1", &rv));
  if (NS_FAILED(rv)) return rv;

  PRBool isContainer;
  rv = mRDFContainerUtils->IsContainer(mDataSource, gNC_DownloadsRoot, &isContainer);
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsIRDFContainer> ctr;

  if (!isContainer) {
    rv = mRDFContainerUtils->MakeSeq(mDataSource, gNC_DownloadsRoot, getter_AddRefs(ctr));
    if (NS_FAILED(rv)) return rv;
  }
  else {
    ctr = do_CreateInstance(NS_RDF_CONTRACTID "/container;1", &rv);
    if (NS_FAILED(rv)) return rv;
    rv = ctr->Init(mDataSource, gNC_DownloadsRoot);
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

  // enumerate the resources, use their ids to retrieve the corresponding
  // nsIDownloadItems from the hashtable (if they don't exist, the download isn't
  // a current transfer), get the items' progress information,
  // and assert it into the graph

  PRBool moreElements;
  items->HasMoreElements(&moreElements);
  for( ; moreElements; items->HasMoreElements(&moreElements)) {
    nsCOMPtr<nsISupports> supports;
    items->GetNext(getter_AddRefs(supports));
    nsCOMPtr<nsIRDFResource> res = do_QueryInterface(supports);
    char* id;
    res->GetValue(&id);
    rv = AssertProgressInfoFor(id);
  }
  return rv;
}

nsresult
nsDownloadManager::AssertProgressInfoFor(const char* aPersistentDescriptor)
{
  nsresult rv = NS_ERROR_FAILURE;
  nsCStringKey key(aPersistentDescriptor);
  if (mCurrDownloadItems->Exists(&key)) {
    nsIDownloadItem* item = NS_STATIC_CAST(nsIDownloadItem*, mCurrDownloadItems->Get(&key));

    PRInt32 percentComplete;
    nsCOMPtr<nsIRDFNode> oldTarget;
    nsCOMPtr<nsIRDFInt> percent;
    nsCOMPtr<nsIRDFResource> res;
    gRDFService->GetResource(aPersistentDescriptor, getter_AddRefs(res));

    // update percentage
    item->GetPercentComplete(&percentComplete);
    rv = mDataSource->GetTarget(res, gNC_ProgressPercent, PR_TRUE, getter_AddRefs(oldTarget));
    if (NS_FAILED(rv)) return rv;

    rv = gRDFService->GetIntLiteral(percentComplete, getter_AddRefs(percent));
    if (NS_FAILED(rv)) return rv;

    if (oldTarget)
      rv = mDataSource->Change(res, gNC_ProgressPercent, oldTarget, percent);
    else
      rv = mDataSource->Assert(res, gNC_ProgressPercent, percent, PR_TRUE);

    // XXX should also store time elapsed
  }
  return rv;
}  

///////////////////////////////////////////////////////////////////////////////
// nsIDownloadManager

NS_IMETHODIMP
nsDownloadManager::AddDownload(nsIURI* aSource,
                               nsILocalFile* aTarget,
                               const PRUnichar* aDisplayName,
                               nsIWebBrowserPersist* aPersist,
                               nsIDownloadItem** aDownloadItem)
{
  NS_ENSURE_ARG_POINTER(aSource);
  NS_ENSURE_ARG_POINTER(aTarget);
  NS_ENSURE_ARG_POINTER(aDownloadItem);

  nsCOMPtr<nsIRDFContainer> downloads;
  nsresult rv = GetDownloadsContainer(getter_AddRefs(downloads));
  if (NS_FAILED(rv)) return rv;

  DownloadItem* item = new DownloadItem();
  if (!item)
    return NS_ERROR_OUT_OF_MEMORY;

  item->QueryInterface(NS_GET_IID(nsIDownloadItem), (void**) aDownloadItem);
  if (!aDownloadItem)
    return NS_ERROR_FAILURE;

  item->SetDownloadManager(this);

  (*aDownloadItem)->SetTarget(aTarget);

  nsXPIDLCString persistentDescriptor;
  aTarget->GetPersistentDescriptor(getter_Copies(persistentDescriptor));

  nsCOMPtr<nsIRDFResource> downloadItem;
  gRDFService->GetResource(persistentDescriptor, getter_AddRefs(downloadItem));

  // if the resource is in the container already (the user has already
  // downloaded this file), remove it
  PRInt32 itemIndex;
  nsCOMPtr<nsIRDFNode> node;
  downloads->IndexOf(downloadItem, &itemIndex);
  if (itemIndex > 0) {
    rv = downloads->RemoveElementAt(itemIndex, PR_TRUE, getter_AddRefs(node));
    if (NS_FAILED(rv)) return rv;
  }
  rv = downloads->AppendElement(downloadItem);
  if (NS_FAILED(rv)) return rv;
  
  nsXPIDLString prettyName;
  nsAutoString displayName; displayName.Assign(aDisplayName);
  // NC:Name
  if (displayName.IsEmpty()) {
    aTarget->GetUnicodeLeafName(getter_Copies(prettyName));
    displayName.Assign(prettyName);
  }
  (*aDownloadItem)->SetPrettyName(displayName.get());
 
  nsCOMPtr<nsIRDFLiteral> nameLiteral;
  gRDFService->GetLiteral(displayName.get(), getter_AddRefs(nameLiteral));
  rv = mDataSource->Assert(downloadItem, gNC_Name, nameLiteral, PR_TRUE);
  if (NS_FAILED(rv)) {
    downloads->IndexOf(downloadItem, &itemIndex);
    downloads->RemoveElementAt(itemIndex, PR_TRUE, getter_AddRefs(node));
    return rv;
  }

  (*aDownloadItem)->SetSource(aSource);
  // NC:URL
  nsXPIDLCString spec;
  aSource->GetSpec(getter_Copies(spec));

  nsCOMPtr<nsIRDFResource> urlResource;
  gRDFService->GetResource(spec, getter_AddRefs(urlResource));
  rv = mDataSource->Assert(downloadItem, gNC_URL, urlResource, PR_TRUE);
  if (NS_FAILED(rv)) {
    downloads->IndexOf(downloadItem, &itemIndex);
    downloads->RemoveElementAt(itemIndex, PR_TRUE, getter_AddRefs(node));
    return rv;
  }

  // NC:File
  nsCOMPtr<nsIRDFResource> fileResource;
  gRDFService->GetResource(persistentDescriptor, getter_AddRefs(fileResource));
  rv = mDataSource->Assert(downloadItem, gNC_File, fileResource, PR_TRUE);
  if (NS_FAILED(rv)) {
    downloads->IndexOf(downloadItem, &itemIndex);
    downloads->RemoveElementAt(itemIndex, PR_TRUE, getter_AddRefs(node));
    return rv;
  }

  nsCOMPtr<nsIRDFRemoteDataSource> remote(do_QueryInterface(mDataSource));
  rv = remote->Flush();
  if (NS_FAILED(rv)) {
    downloads->IndexOf(downloadItem, &itemIndex);
    downloads->RemoveElementAt(itemIndex, PR_TRUE, getter_AddRefs(node));
    return rv;
  }

  // if a persist object was specified, set the download item as the progress listener
  // this will create a cycle that will be broken in DownloadItem::OnStateChange
  if (aPersist) {
    (*aDownloadItem)->SetPersist(aPersist);
    nsCOMPtr<nsIWebProgressListener> listener = do_QueryInterface(*aDownloadItem);
    aPersist->SetProgressListener(listener);
  }

  if (!mCurrDownloadItems)
    mCurrDownloadItems = new nsHashtable();
  
  nsCStringKey key(persistentDescriptor);
  if (mCurrDownloadItems->Exists(&key))
    mCurrDownloadItems->Remove(&key);

  mCurrDownloadItems->Put(&key, (*aDownloadItem));

  return rv;
}

NS_IMETHODIMP
nsDownloadManager::GetDownload(const char* aPersistentDescriptor, nsIDownloadItem** aDownloadItem)
{
  NS_ENSURE_ARG_POINTER(aDownloadItem);

  // if it's currently downloading we can get it from the table
  // XXX otherwise we should look for it in the datasource and
  //     create a new nsIDownloadItem with the resource's properties
  nsCStringKey key(aPersistentDescriptor);
  if (mCurrDownloadItems->Exists(&key)) {
    *aDownloadItem = NS_STATIC_CAST(nsIDownloadItem*, mCurrDownloadItems->Get(&key));
    NS_ADDREF(*aDownloadItem);
    return NS_OK;
  }

  *aDownloadItem = nsnull;
  return NS_ERROR_NULL_POINTER;
}

NS_IMETHODIMP
nsDownloadManager::CancelDownload(const char* aPersistentDescriptor)
{
  nsresult rv = NS_OK;
  nsCStringKey key(aPersistentDescriptor);
  if (mCurrDownloadItems->Exists(&key)) {
    nsIDownloadItem* item = NS_STATIC_CAST(nsIDownloadItem*, mCurrDownloadItems->Get(&key));

    // if a persist was provided, we can do the cancel ourselves.
    nsCOMPtr<nsIWebBrowserPersist> persist;
    item->GetPersist(getter_AddRefs(persist));
    if (persist) {
      rv = persist->CancelSave();
      if (NS_FAILED(rv)) return rv;
    }

    // if an observer was provided, notify that the download was cancelled.
    // if no persist was provided, this is necessary so that whatever transfer
    // component being used can cancel the download itself.
    nsCOMPtr<nsIObserver> observer;
    item->GetObserver(getter_AddRefs(observer));
    if (observer) {
      rv = observer->Observe(item, "oncancel", nsnull);
      if (NS_FAILED(rv)) return rv;
    }

    mCurrDownloadItems->Remove(&key);
  }
  return rv;
}

NS_IMETHODIMP
nsDownloadManager::RemoveDownload(const char* aPersistentDescriptor)
{
  nsCStringKey key(aPersistentDescriptor);
  
  // RemoveDownload is for downloads not currently in progress. Having it
  // cancel in-progress downloads would make things complicated, so just return.
  PRBool inProgress = mCurrDownloadItems->Exists(&key);
  NS_ASSERTION(!inProgress, "Can't call RemoveDownload on a download in progress!");
  if (inProgress)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIRDFContainer> downloads;
    nsresult rv = GetDownloadsContainer(getter_AddRefs(downloads));
  if (NS_FAILED(rv)) return rv;
  
  nsCOMPtr<nsIRDFResource> res;
  gRDFService->GetResource(aPersistentDescriptor, getter_AddRefs(res));

  PRInt32 itemIndex;
  downloads->IndexOf(res, &itemIndex);
  if (itemIndex <= 0)
    return NS_ERROR_FAILURE;
  
  nsCOMPtr<nsIRDFNode> node;
  rv = downloads->RemoveElementAt(itemIndex, PR_TRUE, getter_AddRefs(node));
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsIRDFRemoteDataSource> remote = do_QueryInterface(mDataSource);
  return remote->Flush(); // necessary?
}  

NS_IMETHODIMP
nsDownloadManager::Open(nsIDOMWindow* aParent)
{

  // first assert new progress info so the ui is correctly updated
  // if this fails, it fails -- continue.
  AssertProgressInfo();
  
  // if we ever have the capability to display the UI of third party dl managers,
  // we'll open their UI here instead.
  nsresult rv;
  nsCOMPtr<nsIWindowWatcher> ww = do_GetService("@mozilla.org/embedcomp/window-watcher;1", &rv);
  if (NS_FAILED(rv)) return rv;

  // pass the datasource to the window
  nsCOMPtr<nsISupportsArray> params(do_CreateInstance(NS_SUPPORTSARRAY_CONTRACTID));
  nsCOMPtr<nsISupports> dsSupports(do_QueryInterface(mDataSource));
  params->AppendElement(dsSupports);

  nsCOMPtr<nsIDOMWindow> newWindow;
  rv = ww->OpenWindow(aParent,
                      DOWNLOAD_MANAGER_FE_URL,
                      "_blank",
                      "chrome,titlebar,dialog=no,resizable,centerscreen",
                      params,
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
nsDownloadManager::OpenProgressDialogFor(const char* aPersistentDescriptor, nsIDOMWindow* aParent)
{
  nsresult rv;
  nsCStringKey key(aPersistentDescriptor);
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

  dialog->SetObserver(this);

  // now set the listener so we forward notifications to the dialog
  nsCOMPtr<nsIWebProgressListener> listener = do_QueryInterface(dialog);
  nsISupports* supports = (nsISupports*)item;
  (NS_STATIC_CAST(DownloadItem*, supports))->SetDialogListener(listener);
  
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
// nsIObserver

NS_IMETHODIMP
nsDownloadManager::Observe(nsISupports* aSubject, const char* aTopic, const PRUnichar* aData)
{
  nsCOMPtr<nsIProgressDialog> dialog = do_QueryInterface(aSubject);
  if (nsCRT::strcmp(aTopic, "oncancel") == 0) {
    nsCOMPtr<nsILocalFile> target;
    dialog->GetTarget(getter_AddRefs(target));
    
    char* persistentDescriptor;
    target->GetPersistentDescriptor(&persistentDescriptor);
    
    nsCStringKey key(persistentDescriptor);
    if (mCurrDownloadItems->Exists(&key))
      return CancelDownload(persistentDescriptor);  
  }
  return NS_OK;
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
  char* persistentDescriptor;
  mTarget->GetPersistentDescriptor(&persistentDescriptor);
  mDownloadManager->AssertProgressInfoFor(persistentDescriptor);
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
  if (aMaxTotalProgress)
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

  if (aStateFlags & STATE_STOP) {
    char* persistentDescriptor;
    mTarget->GetPersistentDescriptor(&persistentDescriptor);
    mDownloadManager->DownloadEnded(persistentDescriptor, nsnull);

    // break the cycle we created in AddDownload
    if (mPersist)
      mPersist->SetProgressListener(nsnull);
  }

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