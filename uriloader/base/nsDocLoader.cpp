/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#include "nspr.h"
#include "prlog.h"

#include "nsDocLoader.h"
#include "nsCURILoader.h"
#include "nsNetUtil.h"
#include "nsIHttpChannel.h"

#include "nsIServiceManager.h"
#include "nsXPIDLString.h"

#include "nsIURL.h"
#include "nsCOMPtr.h"
#include "nsCom.h"
#include "nsWeakPtr.h"

#include "nsIDOMWindow.h"

// XXX ick ick ick
#include "nsIContentViewerContainer.h"
#include "nsIDocument.h"
#include "nsIPresShell.h"
#include "nsIPresContext.h"
#include "nsIStringBundle.h"
#include "nsIScriptSecurityManager.h"

static NS_DEFINE_CID(kStringBundleServiceCID, NS_STRINGBUNDLESERVICE_CID);

#if defined(PR_LOGGING)
//
// Log module for nsIDocumentLoader logging...
//
// To enable logging (see prlog.h for full details):
//
//    set NSPR_LOG_MODULES=DocLoader:5
//    set NSPR_LOG_FILE=nspr.log
//
// this enables PR_LOG_DEBUG level information and places all output in
// the file nspr.log
//
PRLogModuleInfo* gDocLoaderLog = nsnull;
#endif /* PR_LOGGING */


#if defined(DEBUG)
void GetURIStringFromRequest(nsIRequest* request, nsXPIDLCString &aStr)
{
    aStr.Adopt(nsCRT::strdup("???"));

    if (request) {
        nsXPIDLString name;
        request->GetName(getter_Copies(name));

        if (name)
            *getter_Copies(aStr) = ToNewUTF8String(nsDependentString(name));
    }
}
#endif /* DEBUG */

/* Define IIDs... */
static NS_DEFINE_IID(kIDocumentIID,                NS_IDOCUMENT_IID);
static NS_DEFINE_IID(kIContentViewerContainerIID,  NS_ICONTENTVIEWERCONTAINER_IID);


struct nsRequestInfo {
  nsRequestInfo(nsIRequest *key) : mKey(key),
                                   mCurrentProgress(0),
                                   mMaxProgress(0)
  {
  }

  void* mKey;
  PRInt32 mCurrentProgress;
  PRInt32 mMaxProgress;
};


nsDocLoaderImpl::nsDocLoaderImpl()
{
  NS_INIT_REFCNT();

#if defined(PR_LOGGING)
  if (nsnull == gDocLoaderLog) {
      gDocLoaderLog = PR_NewLogModule("DocLoader");
  }
#endif /* PR_LOGGING */

  mContainer = nsnull;
  mParent    = nsnull;

  mIsLoadingDocument = PR_FALSE;
  ClearInternalProgress();

  PR_LOG(gDocLoaderLog, PR_LOG_DEBUG, 
         ("DocLoader:%p: created.\n", this));
}

nsresult
nsDocLoaderImpl::SetDocLoaderParent(nsDocLoaderImpl *aParent)
{
  mParent = aParent;
  return NS_OK; 
}

nsresult
nsDocLoaderImpl::Init()
{
    nsresult rv;

    rv = NS_NewLoadGroup(getter_AddRefs(mLoadGroup), this);
    if (NS_FAILED(rv)) return rv;

    PR_LOG(gDocLoaderLog, PR_LOG_DEBUG, 
           ("DocLoader:%p: load group %x.\n", this, mLoadGroup.get()));

    rv = NS_NewISupportsArray(getter_AddRefs(mChildList));
    if (NS_FAILED(rv)) return rv;

    return NS_NewISupportsArray(getter_AddRefs(mListenerList));
}

NS_IMETHODIMP nsDocLoaderImpl::ClearParentDocLoader()
{
  SetDocLoaderParent(nsnull);
  return NS_OK;
}

nsDocLoaderImpl::~nsDocLoaderImpl()
{
		/*
			|ClearWeakReferences()| here is intended to prevent people holding weak references
			from re-entering this destructor since |QueryReferent()| will |AddRef()| me, and the
			subsequent |Release()| will try to destroy me.  At this point there should be only
			weak references remaining (otherwise, we wouldn't be getting destroyed).

			An alternative would be incrementing our refcount (consider it a compressed flag
			saying "Don't re-destroy.").  I haven't yet decided which is better. [scc]
		*/
  ClearWeakReferences();

  Destroy();

  PR_LOG(gDocLoaderLog, PR_LOG_DEBUG, 
         ("DocLoader:%p: deleted.\n", this));

  PRUint32 count=0;
  mChildList->Count(&count);
  // if the doc loader still has children...we need to enumerate the children and make
  // them null out their back ptr to the parent doc loader
  if (count > 0) 
  {
    for (PRUint32 i=0; i<count; i++) 
    {
      nsCOMPtr<nsIDocumentLoader> loader;
      loader = getter_AddRefs(NS_STATIC_CAST(nsIDocumentLoader*, mChildList->ElementAt(i)));

      if (loader) 
        loader->ClearParentDocLoader();
    }    
    mChildList->Clear();
  } 
}


/*
 * Implementation of ISupports methods...
 */
NS_IMPL_THREADSAFE_ADDREF(nsDocLoaderImpl)
NS_IMPL_THREADSAFE_RELEASE(nsDocLoaderImpl)

NS_INTERFACE_MAP_BEGIN(nsDocLoaderImpl)
   NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIRequestObserver)
   NS_INTERFACE_MAP_ENTRY(nsIRequestObserver)
   NS_INTERFACE_MAP_ENTRY(nsIDocumentLoader)
   NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
   NS_INTERFACE_MAP_ENTRY(nsIWebProgress)
   NS_INTERFACE_MAP_ENTRY(nsIProgressEventSink)   
   NS_INTERFACE_MAP_ENTRY(nsIInterfaceRequestor)
   NS_INTERFACE_MAP_ENTRY(nsIHttpEventSink)
   NS_INTERFACE_MAP_ENTRY(nsISecurityEventSink)
NS_INTERFACE_MAP_END


/*
 * Implementation of nsIInterfaceRequestor methods...
 */
NS_IMETHODIMP nsDocLoaderImpl::GetInterface(const nsIID& aIID, void** aSink)
{
  nsresult rv = NS_ERROR_NO_INTERFACE;

  NS_ENSURE_ARG_POINTER(aSink);

  if(aIID.Equals(NS_GET_IID(nsILoadGroup))) {
    *aSink = mLoadGroup;
    NS_IF_ADDREF((nsISupports*)*aSink);
    rv = NS_OK;
  } else {
    rv = QueryInterface(aIID, aSink);
  }

  return rv;
}




NS_IMETHODIMP
nsDocLoaderImpl::CreateDocumentLoader(nsIDocumentLoader** anInstance)
{

  nsresult rv = NS_OK;
  nsDocLoaderImpl * newLoader = new nsDocLoaderImpl();
  NS_ENSURE_TRUE(newLoader, NS_ERROR_OUT_OF_MEMORY);
  
  NS_ADDREF(newLoader);
  newLoader->Init();

  // Initialize now that we have a reference
  rv = newLoader->SetDocLoaderParent(this);
  if (NS_SUCCEEDED(rv)) {
    //
    // XXX this method incorrectly returns a bool    
    //
    rv = mChildList->AppendElement((nsIDocumentLoader*)newLoader) 
       ? NS_OK : NS_ERROR_FAILURE;
  }
  
  rv = newLoader->QueryInterface(NS_GET_IID(nsIDocumentLoader), (void **) anInstance);
  NS_RELEASE(newLoader);
  return rv;
}

NS_IMETHODIMP
nsDocLoaderImpl::Stop(void)
{
  nsresult rv = NS_OK;
  PRUint32 count, i;

  PR_LOG(gDocLoaderLog, PR_LOG_DEBUG, 
         ("DocLoader:%p: Stop() called\n", this));

  rv = mChildList->Count(&count);
  if (NS_FAILED(rv)) return rv;

  for (i=0; i<count; i++) {
    nsIDocumentLoader* loader;

    loader = NS_STATIC_CAST(nsIDocumentLoader*, mChildList->ElementAt(i));

    if (loader) {
      (void) loader->Stop();
      NS_RELEASE(loader);
    }
  }

  rv = mLoadGroup->Cancel(NS_BINDING_ABORTED);

  return rv;
}       


NS_IMETHODIMP
nsDocLoaderImpl::IsBusy(PRBool * aResult)
{
  nsresult rv;

  //
  // A document loader is busy if either:
  //
  //   1. It is currently loading a document (ie. one or more URIs)
  //   2. One of it's child document loaders is busy...
  //
  *aResult = PR_FALSE;

  /* Is this document loader busy? */
  if (mIsLoadingDocument) {
    rv = mLoadGroup->IsPending(aResult);
    if (NS_FAILED(rv)) return rv;
  }

  /* Otherwise, check its child document loaders... */
  if (!*aResult) {
    PRUint32 count, i;

    rv = mChildList->Count(&count);
    if (NS_FAILED(rv)) return rv;

    for (i=0; i<count; i++) {
      nsIDocumentLoader* loader;

      loader = NS_STATIC_CAST(nsIDocumentLoader*, mChildList->ElementAt(i));

      if (loader) {
        (void) loader->IsBusy(aResult);
        NS_RELEASE(loader);

        if (*aResult) break;
      }
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDocLoaderImpl::SetContainer(nsISupports* aContainer)
{
  // This is a weak reference...
  mContainer = aContainer;

  return NS_OK;
}

NS_IMETHODIMP
nsDocLoaderImpl::GetContainer(nsISupports** aResult)
{
   NS_ENSURE_ARG_POINTER(aResult);

   *aResult = mContainer;
   NS_IF_ADDREF(*aResult);

   return NS_OK;
}

NS_IMETHODIMP
nsDocLoaderImpl::GetLoadGroup(nsILoadGroup** aResult)
{
  nsresult rv = NS_OK;

  if (nsnull == aResult) {
    rv = NS_ERROR_NULL_POINTER;
  } else {
    *aResult = mLoadGroup;
    NS_IF_ADDREF(*aResult);
  }
  return rv;
}

NS_IMETHODIMP
nsDocLoaderImpl::GetContentViewerContainer(nsISupports* aDocumentID,
                                           nsIContentViewerContainer** aResult)
{
  nsISupports* base = aDocumentID;
  nsIDocument* doc;
  nsresult rv;

  rv = base->QueryInterface(kIDocumentIID, (void**)&doc);
  if (NS_SUCCEEDED(rv)) {
    nsCOMPtr<nsIPresShell> pres;
    doc->GetShellAt(0, getter_AddRefs(pres));
    if (nsnull != pres) {
      nsIPresContext* presContext;
      rv = pres->GetPresContext(&presContext);
      if (NS_SUCCEEDED(rv) && nsnull != presContext) {
        nsISupports* supp;
        rv = presContext->GetContainer(&supp);
        if (NS_SUCCEEDED(rv) && nsnull != supp) {          
          rv = supp->QueryInterface(kIContentViewerContainerIID, (void**)aResult);          
          NS_RELEASE(supp);
        }
        NS_RELEASE(presContext);
      }
    }
    NS_RELEASE(doc);
  }
  return rv;
}

NS_IMETHODIMP
nsDocLoaderImpl::Destroy()
{
  Stop();

// Remove the document loader from the parent list of loaders...
  if (mParent) 
  {
    mParent->RemoveChildGroup(this);
    mParent = nsnull;
  }

  ClearRequestInfoList();

  mDocumentRequest = 0;

  mLoadGroup->SetGroupObserver(nsnull);

  return NS_OK;
}

NS_IMETHODIMP
nsDocLoaderImpl::OnStartRequest(nsIRequest *request, nsISupports *aCtxt)
{
  // called each time a request is added to the group.

#ifdef PR_LOGGING
  if (PR_LOG_TEST(gDocLoaderLog, PR_LOG_DEBUG)) {
    nsXPIDLString name;
    request->GetName(getter_Copies(name));

    PRUint32 count = 0;
    if (mLoadGroup)
      mLoadGroup->GetActiveCount(&count);

    PR_LOG(gDocLoaderLog, PR_LOG_DEBUG,
           ("DocLoader:%p: OnStartRequest[%p](%s) mIsLoadingDocument=%s, %u active URLs",
            this, request, NS_ConvertUCS2toUTF8(name).get(),
            (mIsLoadingDocument ? "true" : "false"),
            count));
  }
#endif /* PR_LOGGING */
  PRBool bJustStartedLoading = PR_FALSE;

  PRUint32 loadFlags = 0;
  request->GetLoadFlags(&loadFlags);

  if (!mIsLoadingDocument && (loadFlags & nsIChannel::LOAD_DOCUMENT_URI)) {
      bJustStartedLoading = PR_TRUE;
      mIsLoadingDocument = PR_TRUE;
      ClearInternalProgress(); // only clear our progress if we are starting a new load....
  }

  //
  // Only fire an OnStartDocumentLoad(...) if the document loader
  // has initiated a load...  Otherwise, this notification has
  // resulted from a request being added to the load group.
  //
  if (mIsLoadingDocument) {
    //
    // Create a new nsRequestInfo for the request that is starting to
    // load...
    //
    AddRequestInfo(request);

    if (loadFlags & nsIChannel::LOAD_DOCUMENT_URI) {
      //
      // Make sure that hte document channel is null at this point...
      // (unless its been redirected)
      //
      NS_ASSERTION((loadFlags & nsIChannel::LOAD_REPLACE) ||
                   !(mDocumentRequest.get()),
                   "Overwriting an existing document channel!");

      // This request is associated with the entire document...
      mDocumentRequest = request;
      mLoadGroup->SetDefaultLoadRequest(request); 

      // Only fire the start document load notification for the first
      // document URI...  Do not fire it again for redirections
      //
      if (bJustStartedLoading) {
        // Update the progress status state
        mProgressStateFlags = nsIWebProgressListener::STATE_START;

        // Fire the start document load notification
        doStartDocumentLoad();
        return NS_OK;
      }
    } 
  }
  else {
    // The DocLoader is not busy, so clear out any cached information...
    ClearRequestInfoList();
  }

  NS_ASSERTION(!mIsLoadingDocument || mDocumentRequest,
               "mDocumentRequest MUST be set for the duration of a page load!");

  doStartURLLoad(request);

  return NS_OK;
}

NS_IMETHODIMP
nsDocLoaderImpl::OnStopRequest(nsIRequest *aRequest, 
                               nsISupports *aCtxt, 
                               nsresult aStatus)
{
  nsresult rv = NS_OK;

#ifdef PR_LOGGING
  if (PR_LOG_TEST(gDocLoaderLog, PR_LOG_DEBUG)) {
    nsXPIDLString name;
    aRequest->GetName(getter_Copies(name));

    PRUint32 count = 0;
    if (mLoadGroup)
      mLoadGroup->GetActiveCount(&count);

    PR_LOG(gDocLoaderLog, PR_LOG_DEBUG,
           ("DocLoader:%p: OnStopRequest[%p](%s) status=%x mIsLoadingDocument=%s, %u active URLs",
           this, aRequest, NS_ConvertUCS2toUTF8(name).get(),
           aStatus, (mIsLoadingDocument ? "true" : "false"),
           count));
  }
#endif

  //
  // Only fire the OnEndDocumentLoad(...) if the document loader 
  // has initiated a load...
  //
  if (mIsLoadingDocument) {
    PRUint32 count;

    //
    // Set the Maximum progress to the same value as the current progress.
    // Since the URI has finished loading, all the data is there.  Also,
    // this will allow a more accurate estimation of the max progress (in case
    // the old value was unknown ie. -1)
    //
    nsRequestInfo *info;
    info = GetRequestInfo(aRequest);
    if (info) {
      PRInt32 oldMax = info->mMaxProgress;

      info->mMaxProgress = info->mCurrentProgress;
      //
      // If a request whose content-length was previously unknown has just
      // finished loading, then use this new data to try to calculate a
      // mMaxSelfProgress...
      //
      if ((oldMax < 0) && (mMaxSelfProgress < 0)) {
        CalculateMaxProgress(&mMaxSelfProgress);
      }
    }

    //
    // Fire the OnStateChange(...) notification for stop request
    //
    doStopURLLoad(aRequest, aStatus);
    
    rv = mLoadGroup->GetActiveCount(&count);
    if (NS_FAILED(rv)) return rv;

    //
    // The load group for this DocumentLoader is idle...
    //
    if (0 == count) {
      DocLoaderIsEmpty();
    }
  }
  else {
    doStopURLLoad(aRequest, aStatus); 
  }
  
  return NS_OK;
}


nsresult nsDocLoaderImpl::RemoveChildGroup(nsDocLoaderImpl* aLoader)
{
  nsresult rv = NS_OK;

  if (NS_SUCCEEDED(rv)) {
    mChildList->RemoveElement((nsIDocumentLoader*)aLoader);
  }
  return rv;
}

NS_IMETHODIMP nsDocLoaderImpl::GetDocumentChannel(nsIChannel ** aChannel)
{
    nsCOMPtr<nsIChannel> ourChannel = do_QueryInterface(mDocumentRequest);   

  *aChannel = ourChannel.get();
  NS_IF_ADDREF(*aChannel);
  return NS_OK;
}


void nsDocLoaderImpl::DocLoaderIsEmpty()
{
  if (mIsLoadingDocument) {
    PRBool busy = PR_FALSE;
    /* In the unimagineably rude circumstance that onload event handlers
       triggered by this function actually kill the window ... ok, it's
       not unimagineable; it's happened ... this deathgrip keeps this object
       alive long enough to survive this function call. */
    nsCOMPtr<nsIDocumentLoader> kungFuDeathGrip(this);

    IsBusy(&busy);
    if (!busy) {
      PR_LOG(gDocLoaderLog, PR_LOG_DEBUG, 
             ("DocLoader:%p: Is now idle...\n", this));

      nsCOMPtr<nsIRequest> docRequest = mDocumentRequest;

      NS_ASSERTION(mDocumentRequest, "No Document Request!");
      mDocumentRequest = 0;
      mIsLoadingDocument = PR_FALSE;

      // Update the progress status state - the document is done
      mProgressStateFlags = nsIWebProgressListener::STATE_STOP;


      nsresult loadGroupStatus = NS_OK; 
      mLoadGroup->GetStatus(&loadGroupStatus);

      // 
      // New code to break the circular reference between 
      // the load group and the docloader... 
      // 
      mLoadGroup->SetDefaultLoadRequest(nsnull); 

      //
      // Do nothing after firing the OnEndDocumentLoad(...). The document
      // loader may be loading a *new* document - if LoadDocument()
      // was called from a handler!
      //
      doStopDocumentLoad(docRequest, loadGroupStatus);

      if (mParent) {
        mParent->DocLoaderIsEmpty();
      }
    }
  }
}

void nsDocLoaderImpl::doStartDocumentLoad(void)
{

#if defined(DEBUG)
  nsXPIDLCString buffer;

  GetURIStringFromRequest(mDocumentRequest, buffer);
  PR_LOG(gDocLoaderLog, PR_LOG_DEBUG, 
         ("DocLoader:%p: ++ Firing OnStateChange for start document load (...)."
          "\tURI: %s \n",
          this, (const char *) buffer));
#endif /* DEBUG */

  // Fire an OnStatus(...) notification STATE_START.  This indicates
  // that the document represented by mDocumentRequest has started to
  // load...
  FireOnStateChange(this,
                    mDocumentRequest,
                    nsIWebProgressListener::STATE_START |
                    nsIWebProgressListener::STATE_IS_DOCUMENT |
                    nsIWebProgressListener::STATE_IS_REQUEST |
                    nsIWebProgressListener::STATE_IS_NETWORK,
                    NS_OK);
}

void nsDocLoaderImpl::doStartURLLoad(nsIRequest *request)
{
#if defined(DEBUG)
  nsXPIDLCString buffer;

  GetURIStringFromRequest(request, buffer);
    PR_LOG(gDocLoaderLog, PR_LOG_DEBUG, 
          ("DocLoader:%p: ++ Firing OnStateChange start url load (...)."
           "\tURI: %s\n",
            this, (const char *) buffer));
#endif /* DEBUG */

  FireOnStateChange(this,
                    request,
                    nsIWebProgressListener::STATE_START |
                    nsIWebProgressListener::STATE_IS_REQUEST,
                    NS_OK);
}

void nsDocLoaderImpl::doStopURLLoad(nsIRequest *request, nsresult aStatus)
{
#if defined(DEBUG)
  nsXPIDLCString buffer;

  GetURIStringFromRequest(request, buffer);
    PR_LOG(gDocLoaderLog, PR_LOG_DEBUG, 
          ("DocLoader:%p: ++ Firing OnStateChange for end url load (...)."
           "\tURI: %s status=%x\n",
            this, (const char *) buffer, aStatus));
#endif /* DEBUG */

  FireOnStateChange(this,
                    request,
                    nsIWebProgressListener::STATE_STOP |
                    nsIWebProgressListener::STATE_IS_REQUEST,
                    aStatus);
}

void nsDocLoaderImpl::doStopDocumentLoad(nsIRequest *request,
                                         nsresult aStatus)
{
#if defined(DEBUG)
  nsXPIDLCString buffer;

  GetURIStringFromRequest(request, buffer);
  PR_LOG(gDocLoaderLog, PR_LOG_DEBUG, 
         ("DocLoader:%p: ++ Firing OnStateChange for end document load (...)."
         "\tURI: %s Status=%x\n",
          this, (const char *) buffer, aStatus));
#endif /* DEBUG */

  //
  // Fire an OnStatusChange(...) notification indicating the the
  // current document has finished loading...
  //
  FireOnStateChange(this,
                    request,
                    nsIWebProgressListener::STATE_STOP |
                    nsIWebProgressListener::STATE_IS_DOCUMENT,
                    aStatus);

  //
  // Fire a final OnStatusChange(...) notification indicating the the
  // current document has finished loading...
  //
  FireOnStateChange(this,
                    request,
                    nsIWebProgressListener::STATE_STOP |
                    nsIWebProgressListener::STATE_IS_WINDOW |
                    nsIWebProgressListener::STATE_IS_NETWORK,
                    aStatus);
}

////////////////////////////////////////////////////////////////////////////////////
// The following section contains support for nsIWebProgress and related stuff
////////////////////////////////////////////////////////////////////////////////////

// get web progress returns our web progress listener or if
// we don't have one, it will look up the doc loader hierarchy
// to see if one of our parent doc loaders has one.
nsresult nsDocLoaderImpl::GetParentWebProgressListener(nsDocLoaderImpl * aDocLoader, nsIWebProgressListener ** aWebProgress)
{
  // if we got here, there is no web progress to return
  *aWebProgress = nsnull;
  
  return NS_OK;
}

NS_IMETHODIMP
nsDocLoaderImpl::AddProgressListener(nsIWebProgressListener *aListener)
{
  nsresult rv;

  nsWeakPtr listener = getter_AddRefs(NS_GetWeakReference(aListener));
  if (!listener) return NS_ERROR_INVALID_ARG;

  if (mListenerList->IndexOf(listener) == -1) {

    // XXX this method incorrectly returns a bool    
    rv = mListenerList->AppendElement(listener) ? NS_OK : NS_ERROR_FAILURE;
  } else {
    // The listener is already in the list...
    rv = NS_ERROR_FAILURE;
  }
  return rv;
}

NS_IMETHODIMP
nsDocLoaderImpl::RemoveProgressListener(nsIWebProgressListener *aListener)
{
  nsWeakPtr listener = getter_AddRefs(NS_GetWeakReference(aListener));
  if (!listener) return NS_ERROR_INVALID_ARG;

  // XXX this method incorrectly returns a bool    
  return mListenerList->RemoveElement(listener) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsDocLoaderImpl::GetDOMWindow(nsIDOMWindow **aResult)
{
  nsresult rv = NS_OK;

  *aResult = nsnull;
  //
  // The DOM Window is available from the associated container (ie DocShell)
  // if one is available...
  //
  if (mContainer) {
    nsCOMPtr<nsIDOMWindow> window(do_GetInterface(mContainer, &rv));

    *aResult = window;
    NS_IF_ADDREF(*aResult);
  } else {
      rv = NS_ERROR_FAILURE;
  }

  return rv;
}

nsresult nsDocLoaderImpl::GetProgressStatusFlags(PRInt32 *aProgressStateFlags)
{
  *aProgressStateFlags = mProgressStateFlags;
  return NS_OK;
}

nsresult nsDocLoaderImpl::GetCurSelfProgress(PRInt32 *aCurSelfProgress)
{
  *aCurSelfProgress = mCurrentSelfProgress;
  return NS_OK;
}

nsresult nsDocLoaderImpl::GetMaxSelfProgress(PRInt32 *aMaxSelfProgress)
{
  *aMaxSelfProgress = mMaxSelfProgress;
  return NS_OK;
}

nsresult nsDocLoaderImpl::GetCurTotalProgress(PRInt32 *aCurTotalProgress)
{
  *aCurTotalProgress = mCurrentTotalProgress;
  return NS_OK;
}

nsresult nsDocLoaderImpl::GetMaxTotalProgress(PRInt32 *aMaxTotalProgress)
{
  PRUint32 count = 0;
  nsresult rv = NS_OK;
  PRInt32 invididualProgress, newMaxTotal;

  newMaxTotal = 0;

  rv = mChildList->Count(&count);
  if (NS_FAILED(rv)) return rv;
  nsCOMPtr<nsIWebProgress> webProgress;
  nsCOMPtr<nsISupports> docloader;
  for (PRUint32 i=0; i<count; i++) 
  {
    invididualProgress = 0;
    docloader = getter_AddRefs(mChildList->ElementAt(i));
    if (docloader)
    {
      // Cast is safe since all children are nsDocLoaderImpl too
      ((nsDocLoaderImpl *) docloader.get())->GetMaxTotalProgress(&invididualProgress);
    }
    if (invididualProgress < 0) // if one of the elements doesn't know it's size
                                // then none of them do
    {
       newMaxTotal = -1;
       break;
    }
    else
     newMaxTotal += invididualProgress;
  }
  if (mMaxSelfProgress >= 0 && newMaxTotal >= 0) {
    *aMaxTotalProgress = newMaxTotal + mMaxSelfProgress;
  } else {
    *aMaxTotalProgress = -1;
  }
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////////
// The following section contains support for nsIEventProgressSink which is used to 
// pass progress and status between the actual request and the doc loader. The doc loader
// then turns around and makes the right web progress calls based on this information.
////////////////////////////////////////////////////////////////////////////////////

NS_IMETHODIMP nsDocLoaderImpl::OnProgress(nsIRequest *aRequest, nsISupports* ctxt, 
                                          PRUint32 aProgress, PRUint32 aProgressMax)
{
  nsRequestInfo *info;
  PRInt32 progressDelta = 0;

  //
  // Update the RequsetInfo entry with the new progress data
  //
  info = GetRequestInfo(aRequest);
  if (info) {
    if ((0 == info->mCurrentProgress) && (0 == info->mMaxProgress)) {
      //
      // This is the first progress notification for the entry.  If
      // (aMaxProgress > 0) then the content-length of the data is known,
      // so update mMaxSelfProgress...  Otherwise, set it to -1 to indicate
      // that the content-length is no longer known.
      //
      if (aProgressMax != (PRUint32)-1) {
        mMaxSelfProgress  += aProgressMax;
        info->mMaxProgress = aProgressMax;
      } else {
        mMaxSelfProgress   = -1;
        info->mMaxProgress = -1;
      }

      // Send a STATE_TRANSFERRING notification for the request.
      PRInt32 flags;
    
      flags = nsIWebProgressListener::STATE_TRANSFERRING | 
              nsIWebProgressListener::STATE_IS_REQUEST;
      //
      // Move the WebProgress into the STATE_TRANSFERRING state if necessary...
      //
      if (mProgressStateFlags & nsIWebProgressListener::STATE_START) {
        mProgressStateFlags = nsIWebProgressListener::STATE_TRANSFERRING;

        // Send STATE_TRANSFERRING for the document too...
        flags |= nsIWebProgressListener::STATE_IS_DOCUMENT;
      }

      FireOnStateChange(this, aRequest, flags, NS_OK);
    }

    // Update the current progress count...
    progressDelta = aProgress - info->mCurrentProgress;
    mCurrentSelfProgress += progressDelta;

    info->mCurrentProgress = aProgress;
  } 
  //
  // The request is not part of the load group, so ignore its progress
  // information...
  //
  else {
#if defined(DEBUG)
    nsXPIDLCString buffer;

    GetURIStringFromRequest(aRequest, buffer);
    PR_LOG(gDocLoaderLog, PR_LOG_DEBUG, 
           ("DocLoader:%p OOPS - No Request Info for: %s\n",
            this, (const char *)buffer));
#endif /* DEBUG */

    return NS_OK;
  }

  //
  // Fire progress notifications out to any registered nsIWebProgressListeners
  //
  FireOnProgressChange(this, aRequest, aProgress, aProgressMax, progressDelta,
                       mCurrentTotalProgress, mMaxTotalProgress);

  return NS_OK;
}

NS_IMETHODIMP nsDocLoaderImpl::OnStatus(nsIRequest* aRequest, nsISupports* ctxt, 
                                        nsresult aStatus, const PRUnichar* aStatusArg)
{
  //
  // Fire progress notifications out to any registered nsIWebProgressListeners
  //
  if (aStatus) {
    nsresult rv;
    nsCOMPtr<nsIStringBundleService> sbs = do_GetService(kStringBundleServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;
    nsXPIDLString msg;
    rv = sbs->FormatStatusMessage(aStatus, aStatusArg, getter_Copies(msg));
    if (NS_FAILED(rv)) return rv;
    FireOnStatusChange(this, aRequest, aStatus, msg);
  }
  return NS_OK;
}

void nsDocLoaderImpl::ClearInternalProgress()
{
  ClearRequestInfoList();

  mCurrentSelfProgress  = mMaxSelfProgress  = 0;
  mCurrentTotalProgress = mMaxTotalProgress = 0;

  mProgressStateFlags = nsIWebProgressListener::STATE_STOP;
}


void nsDocLoaderImpl::FireOnProgressChange(nsDocLoaderImpl *aLoadInitiator,
                                           nsIRequest *request,
                                           PRInt32 aProgress,
                                           PRInt32 aProgressMax,
                                           PRInt32 aProgressDelta,
                                           PRInt32 aTotalProgress,
                                           PRInt32 aMaxTotalProgress)
{
  PRUint32 count;

  if (mIsLoadingDocument) {
    mCurrentTotalProgress += aProgressDelta;
    GetMaxTotalProgress(&mMaxTotalProgress);

    aTotalProgress    = mCurrentTotalProgress;
    aMaxTotalProgress = mMaxTotalProgress;
  }

#if defined(DEBUG)
  nsXPIDLCString buffer;

  GetURIStringFromRequest(request, buffer);
  PR_LOG(gDocLoaderLog, PR_LOG_DEBUG, 
         ("DocLoader:%p: Progress (%s): curSelf: %d maxSelf: %d curTotal: %d maxTotal %d\n",
          this, (const char *)buffer, aProgress, aProgressMax, aTotalProgress, aMaxTotalProgress));
#endif /* DEBUG */

  /*
   * First notify any listeners of the new progress info...
   *
   * Operate the elements from back to front so that if items get
   * get removed from the list it won't affect our iteration
   */
  (void)mListenerList->Count(&count);
  while (count > 0) {
    nsCOMPtr<nsISupports> supports;
    nsresult rv = mListenerList->GetElementAt(--count, getter_AddRefs(supports));
    if (NS_FAILED(rv)) return;
    nsWeakPtr weakPtr = do_QueryInterface(supports);

    nsCOMPtr<nsIWebProgressListener> listener;
    listener = do_QueryReferent(weakPtr);
    if (!listener) {
        // the listener went away. gracefully pull it out of the list.
        mListenerList->RemoveElementAt(count);
        continue;
    }

    listener->OnProgressChange(aLoadInitiator,request,
                               aProgress, aProgressMax,
                               aTotalProgress, aMaxTotalProgress);
  }

  mListenerList->Compact();

  // Pass the notification up to the parent...
  if (mParent) {
    mParent->FireOnProgressChange(aLoadInitiator, request,
                                  aProgress, aProgressMax,
                                  aProgressDelta,
                                  aTotalProgress, aMaxTotalProgress);
  }
}



void nsDocLoaderImpl::FireOnStateChange(nsIWebProgress *aProgress,
                                        nsIRequest *aRequest,
                                        PRInt32 aStateFlags,
                                        nsresult aStatus)
{
  PRUint32 count;

  //
  // Remove the STATE_IS_NETWORK bit if necessary.
  //
  // The rule is to remove this bit, if the notification has been passed
  // up from a child WebProgress, and the current WebProgress is already
  // active...
  //
  if (mIsLoadingDocument &&
      (aStateFlags & nsIWebProgressListener::STATE_IS_NETWORK) && 
      (this != aProgress)) {
    aStateFlags &= ~nsIWebProgressListener::STATE_IS_NETWORK;
  }

#if defined(DEBUG)
  nsXPIDLCString buffer;

  GetURIStringFromRequest(aRequest, buffer);
  PR_LOG(gDocLoaderLog, PR_LOG_DEBUG, 
         ("DocLoader:%p: Status (%s): code: %x\n",
         this, (const char *)buffer, aStateFlags));
#endif /* DEBUG */

  NS_ASSERTION(aRequest, "Firing OnStateChange(...) notification with a NULL request!");

  /*                                                                           
   * First notify any listeners of the new state info...
   *
   * Operate the elements from back to front so that if items get
   * get removed from the list it won't affect our iteration
   */
  (void)mListenerList->Count(&count);
  while (count > 0) {
    nsCOMPtr<nsISupports> supports;
    nsresult rv = mListenerList->GetElementAt(--count, getter_AddRefs(supports));
    if (NS_FAILED(rv)) return;
    nsWeakPtr weakPtr = do_QueryInterface(supports);

    nsCOMPtr<nsIWebProgressListener> listener;
    listener = do_QueryReferent(weakPtr);
    if (!listener) {
        // the listener went away, gracefully pull it out of the list.
        mListenerList->RemoveElementAt(count);
        continue;
    }

    listener->OnStateChange(aProgress, aRequest, aStateFlags, aStatus);
  }

  mListenerList->Compact();

  // Pass the notification up to the parent...
  if (mParent) {
    mParent->FireOnStateChange(aProgress, aRequest, aStateFlags, aStatus);
  }
}



NS_IMETHODIMP
nsDocLoaderImpl::FireOnLocationChange(nsIWebProgress* aWebProgress,
                                      nsIRequest* aRequest,
                                      nsIURI *aUri)
{
  PRUint32 count;

  (void)mListenerList->Count(&count);
  while (count > 0) {
    nsCOMPtr<nsISupports> supports;
    nsresult rv = mListenerList->GetElementAt(--count, getter_AddRefs(supports));
    if (NS_FAILED(rv)) return rv;
    nsWeakPtr weakPtr = do_QueryInterface(supports);

    nsCOMPtr<nsIWebProgressListener> listener;
    listener = do_QueryReferent(weakPtr);
    if (!listener) {
        // the listener went away. gracefully pull it out of the list.
        mListenerList->RemoveElementAt(count);
        continue;
    }

    listener->OnLocationChange(aWebProgress, aRequest, aUri);
  }

  mListenerList->Compact();
  // Pass the notification up to the parent...
  if (mParent) {
    mParent->FireOnLocationChange(aWebProgress, aRequest, aUri);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDocLoaderImpl::FireOnStatusChange(nsIWebProgress* aWebProgress,
                                    nsIRequest* aRequest,
                                    nsresult aStatus,
                                    const PRUnichar* aMessage)
{
  PRUint32 count;

  (void)mListenerList->Count(&count);
  while (count > 0) {
    nsCOMPtr<nsISupports> supports;
    nsresult rv = mListenerList->GetElementAt(--count, getter_AddRefs(supports));
    if (NS_FAILED(rv)) return rv;
    nsWeakPtr weakPtr = do_QueryInterface(supports);

    nsCOMPtr<nsIWebProgressListener> listener;
    listener = do_QueryReferent(weakPtr);
    if (!listener) {
        // the listener went away. gracefully pull it out of the list.
        mListenerList->RemoveElementAt(count);
        continue;
    }

    listener->OnStatusChange(aWebProgress, aRequest, aStatus, aMessage);
  }
  mListenerList->Compact();

  return NS_OK;
}

nsresult nsDocLoaderImpl::AddRequestInfo(nsIRequest *aRequest)
{
  nsresult rv;
  nsRequestInfo *info;

  info = new nsRequestInfo(aRequest);
  if (info) {
    // XXX this method incorrectly returns a bool    
    rv = mRequestInfoList.AppendElement(info) ? NS_OK : NS_ERROR_FAILURE;
  } else {
    rv = NS_ERROR_OUT_OF_MEMORY;
  }
  return rv;
}

nsRequestInfo * nsDocLoaderImpl::GetRequestInfo(nsIRequest *aRequest)
{
  nsRequestInfo *info;
  PRInt32 i, count;

  count = mRequestInfoList.Count();
  for(i=0; i<count; i++) {
    info = (nsRequestInfo *)mRequestInfoList.ElementAt(i);
    if (aRequest == info->mKey) {
      return info;
    }
  }

  return nsnull;
}

nsresult nsDocLoaderImpl::ClearRequestInfoList(void)
{
  nsRequestInfo *info;
  PRInt32 i, count;

  count = mRequestInfoList.Count();
  for(i=0; i<count; i++) {
    info = (nsRequestInfo *)mRequestInfoList.ElementAt(i);
    delete info;
  }

  mRequestInfoList.Clear();
  mRequestInfoList.Compact();

  return NS_OK;
}

void nsDocLoaderImpl::CalculateMaxProgress(PRInt32 *aMax)
{
  PRInt32 i, count;
  PRInt32 current=0, max=0;

  count = mRequestInfoList.Count();
  for(i=0; i<count; i++) {
    nsRequestInfo *info;
  
    info = (nsRequestInfo *)mRequestInfoList.ElementAt(i);

    current += info->mCurrentProgress;
    if (max >= 0) {
      if (info->mMaxProgress < info->mCurrentProgress) {
        max = -1;
      } else {
        max += info->mMaxProgress;
      }
    }
  }

  *aMax = max;
}

NS_IMETHODIMP nsDocLoaderImpl::OnRedirect(nsIHttpChannel *aOldChannel, nsIChannel *aNewChannel)
{
  if (aOldChannel)
  {
    nsresult rv;
    nsCOMPtr<nsIURI> oldURI, newURI;

    rv = aOldChannel->GetOriginalURI(getter_AddRefs(oldURI));
    if (NS_FAILED(rv)) return rv;

    rv = aNewChannel->GetURI(getter_AddRefs(newURI));
    if (NS_FAILED(rv)) return rv;

    // verify that this is a legal redirect
    nsCOMPtr<nsIScriptSecurityManager> securityManager = 
             do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID, &rv);
    if (NS_FAILED(rv)) return rv;
    rv = securityManager->CheckLoadURI(oldURI, newURI,
                                       nsIScriptSecurityManager::DISALLOW_FROM_MAIL);
    if (NS_FAILED(rv)) return rv;

    nsLoadFlags loadFlags = 0;
    PRInt32 stateFlags = nsIWebProgressListener::STATE_REDIRECTING |
                         nsIWebProgressListener::STATE_IS_REQUEST;

    aOldChannel->GetLoadFlags(&loadFlags);
    // If the document channel is being redirected, then indicate that the
    // document is being redirected in the notification...
    if (loadFlags & nsIChannel::LOAD_DOCUMENT_URI)
    {
      stateFlags |= nsIWebProgressListener::STATE_IS_DOCUMENT;

#if defined(DEBUG)
      nsCOMPtr<nsIRequest> request(do_QueryInterface(aOldChannel));
      NS_ASSERTION(request == mDocumentRequest, "Wrong Document Channel");
#endif /* DEBUG */
    }

    FireOnStateChange(this, aOldChannel, stateFlags, NS_OK);
  }

  return NS_OK;
}

NS_IMETHODIMP nsDocLoaderImpl::OnSecurityChange(nsISupports * aContext,
                                                PRInt32 state)
{
  //
  // Fire progress notifications out to any registered nsIWebProgressListeners.  
  //
  
  nsCOMPtr<nsIRequest> request = do_QueryInterface(aContext);
  nsIWebProgress* webProgress = NS_STATIC_CAST(nsIWebProgress*, this);

  PRUint32 count;
  (void)mListenerList->Count(&count);

  while (count > 0) {
    nsCOMPtr<nsISupports> supports;
    nsresult rv = mListenerList->GetElementAt(--count, getter_AddRefs(supports));
    if (NS_FAILED(rv)) return rv;
    nsWeakPtr weakPtr = do_QueryInterface(supports);

    nsCOMPtr<nsIWebProgressListener> listener;
    listener = do_QueryReferent(weakPtr);
    if (!listener) {
        // the listener went away. gracefully pull it out of the list.
        mListenerList->RemoveElementAt(count);
        continue;
    }
    listener->OnSecurityChange(webProgress, request, state);
  }

  mListenerList->Compact();

  // Pass the notification up to the parent...
  if (mParent) {
    mParent->OnSecurityChange(aContext, state);
  }
  return NS_OK;
}




#if 0
void nsDocLoaderImpl::DumpChannelInfo()
{
  nsChannelInfo *info;
  PRInt32 i, count;
  PRInt32 current=0, max=0;

  
  printf("==== DocLoader=%x\n", this);

  count = mChannelInfoList.Count();
  for(i=0; i<count; i++) {
    info = (nsChannelInfo *)mChannelInfoList.ElementAt(i);

#if defined(DEBUG)
    nsXPIDLCString buffer;
    nsresult rv = NS_OK;
    if (info->mURI) {
      rv = info->mURI->GetSpec(getter_Copies(buffer));
    }

    printf("  [%d] current=%d  max=%d [%s]\n", i,
           info->mCurrentProgress, 
           info->mMaxProgress, (const char *)buffer);
#endif /* DEBUG */

    current += info->mCurrentProgress;
    if (max >= 0) {
      if (info->mMaxProgress < info->mCurrentProgress) {
        max = -1;
      } else {
        max += info->mMaxProgress;
      }
    }
  }

  printf("\nCurrent=%d   Total=%d\n====\n", current, max);
}
#endif /* 0 */

