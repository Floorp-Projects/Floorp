/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
#include "nscore.h"
#include "nsWeakPtr.h"

#include "nsIDOMWindow.h"

#include "nsIPresShell.h"
#include "nsPresContext.h"
#include "nsIStringBundle.h"
#include "nsIScriptSecurityManager.h"

#include "nsITransport.h"
#include "nsISocketTransport.h"

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
void GetURIStringFromRequest(nsIRequest* request, nsACString &name)
{
    if (request)
        request->GetName(name);
    else
        name.AssignLiteral("???");
}
#endif /* DEBUG */

struct nsRequestInfo : public PLDHashEntryHdr
{
  nsRequestInfo(const void *key)
    : mKey(key), mCurrentProgress(0), mMaxProgress(0), mUploading(PR_FALSE)
  {
  }

  const void* mKey; // Must be first for the pldhash stubs to work
  PRInt32 mCurrentProgress;
  PRInt32 mMaxProgress;
  PRBool mUploading;
};


PR_STATIC_CALLBACK(PRBool)
RequestInfoHashInitEntry(PLDHashTable *table, PLDHashEntryHdr *entry,
                         const void *key)
{
  // Initialize the entry with placement new
  new (entry) nsRequestInfo(key);
  return PR_TRUE;
}


struct nsListenerInfo {
  nsListenerInfo(nsIWeakReference *aListener, unsigned long aNotifyMask) 
    : mWeakListener(aListener),
      mNotifyMask(aNotifyMask)
  {
  }

  // Weak pointer for the nsIWebProgressListener...
  nsWeakPtr mWeakListener;

  // Mask indicating which notifications the listener wants to receive.
  unsigned long mNotifyMask;
};


nsDocLoaderImpl::nsDocLoaderImpl()
  : mListenerInfoList(8)
{
#if defined(PR_LOGGING)
  if (nsnull == gDocLoaderLog) {
      gDocLoaderLog = PR_NewLogModule("DocLoader");
  }
#endif /* PR_LOGGING */

  mContainer = nsnull;
  mParent    = nsnull;

  mIsLoadingDocument = PR_FALSE;

  static PLDHashTableOps hash_table_ops =
  {
    PL_DHashAllocTable,
    PL_DHashFreeTable,
    PL_DHashGetKeyStub,
    PL_DHashVoidPtrKeyStub,
    PL_DHashMatchEntryStub,
    PL_DHashMoveEntryStub,
    PL_DHashClearEntryStub,
    PL_DHashFinalizeStub,
    RequestInfoHashInitEntry
  };

  if (!PL_DHashTableInit(&mRequestInfoHash, &hash_table_ops, nsnull,
                         sizeof(nsRequestInfo), 16)) {
    mRequestInfoHash.ops = nsnull;
  }

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
  if (!mRequestInfoHash.ops) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  nsresult rv = NS_NewLoadGroup(getter_AddRefs(mLoadGroup), this);
  if (NS_FAILED(rv)) return rv;

  PR_LOG(gDocLoaderLog, PR_LOG_DEBUG, 
         ("DocLoader:%p: load group %x.\n", this, mLoadGroup.get()));

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

  PRInt32 count = mChildList.Count();
  // if the doc loader still has children...we need to enumerate the
  // children and make them null out their back ptr to the parent doc
  // loader
  if (count > 0)
  {
    for (PRInt32 i=0; i < count; i++)
    {
      nsIDocumentLoader* loader = mChildList.ObjectAt(i);

      if (loader) {
        // This is a safe cast, as we only put nsDocLoaderImpl objects into the
        // array
        NS_STATIC_CAST(nsDocLoaderImpl*, loader)->SetDocLoaderParent(nsnull);
      }
    }
    mChildList.Clear();
  }

  if (mRequestInfoHash.ops) {
    PL_DHashTableFinish(&mRequestInfoHash);
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
  *anInstance = nsnull;
  
  nsDocLoaderImpl * newLoader = new nsDocLoaderImpl();
  NS_ENSURE_TRUE(newLoader, NS_ERROR_OUT_OF_MEMORY);
  
  NS_ADDREF(newLoader);

  // Initialize now that we have a reference
  rv = newLoader->Init();

  if (NS_SUCCEEDED(rv)) {
    rv = newLoader->SetDocLoaderParent(this);
  }
  
  if (NS_SUCCEEDED(rv)) {
    rv = mChildList.AppendObject(newLoader) 
       ? NS_OK : NS_ERROR_FAILURE;
  }
  
  if (NS_SUCCEEDED(rv)) {
    rv = CallQueryInterface(newLoader, anInstance);
  }
  
  NS_RELEASE(newLoader);
  return rv;
}

NS_IMETHODIMP
nsDocLoaderImpl::Stop(void)
{
  nsresult rv = NS_OK;
  PRInt32 count, i;

  PR_LOG(gDocLoaderLog, PR_LOG_DEBUG, 
         ("DocLoader:%p: Stop() called\n", this));

  count = mChildList.Count();

  nsCOMPtr<nsIDocumentLoader> loader;
  for (i=0; i < count; i++) {
    loader = mChildList.ObjectAt(i);

    if (loader) {
      (void) loader->Stop();
    }
  }

  if (mLoadGroup)
    rv = mLoadGroup->Cancel(NS_BINDING_ABORTED);

  return rv;
}       


PRBool
nsDocLoaderImpl::IsBusy()
{
  nsresult rv;

  //
  // A document loader is busy if either:
  //
  //   1. It is currently loading a document (ie. one or more URIs)
  //   2. One of it's child document loaders is busy...
  //

  /* Is this document loader busy? */
  if (mIsLoadingDocument) {
    PRBool busy;
    rv = mLoadGroup->IsPending(&busy);
    if (NS_FAILED(rv))
      return PR_FALSE;
    if (busy)
      return PR_TRUE;
  }

  /* Otherwise, check its child document loaders... */
  PRInt32 count, i;

  count = mChildList.Count();

  for (i=0; i < count; i++) {
    nsIDocumentLoader* loader = mChildList.ObjectAt(i);

    // This is a safe cast, because we only put nsDocLoaderImpl objects into the
    // array
    if (loader && NS_STATIC_CAST(nsDocLoaderImpl*, loader)->IsBusy())
        return PR_TRUE;
  }

  return PR_FALSE;
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
nsDocLoaderImpl::Destroy()
{
  Stop();

// Remove the document loader from the parent list of loaders...
  if (mParent) 
  {
    mParent->RemoveChildGroup(this);
    mParent = nsnull;
  }

  // Release all the information about network requests...
  ClearRequestInfoHash();

  // Release all the information about registered listeners...
  PRInt32 i, count;

  count = mListenerInfoList.Count();
  for(i=0; i<count; i++) {
    nsListenerInfo *info =
      NS_STATIC_CAST(nsListenerInfo*, mListenerInfoList.ElementAt(i));

    delete info;
  }

  mListenerInfoList.Clear();
  mListenerInfoList.Compact();

  mDocumentRequest = 0;

  if (mLoadGroup)
    mLoadGroup->SetGroupObserver(nsnull);

  return NS_OK;
}

NS_IMETHODIMP
nsDocLoaderImpl::OnStartRequest(nsIRequest *request, nsISupports *aCtxt)
{
  // called each time a request is added to the group.

#ifdef PR_LOGGING
  if (PR_LOG_TEST(gDocLoaderLog, PR_LOG_DEBUG)) {
    nsCAutoString name;
    request->GetName(name);

    PRUint32 count = 0;
    if (mLoadGroup)
      mLoadGroup->GetActiveCount(&count);

    PR_LOG(gDocLoaderLog, PR_LOG_DEBUG,
           ("DocLoader:%p: OnStartRequest[%p](%s) mIsLoadingDocument=%s, %u active URLs",
            this, request, name.get(),
            (mIsLoadingDocument ? "true" : "false"),
            count));
  }
#endif /* PR_LOGGING */
  PRBool bJustStartedLoading = PR_FALSE;

  nsLoadFlags loadFlags = 0;
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
      // Make sure that the document channel is null at this point...
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
    ClearRequestInfoHash();
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
    nsCAutoString name;
    aRequest->GetName(name);

    PRUint32 count = 0;
    if (mLoadGroup)
      mLoadGroup->GetActiveCount(&count);

    PR_LOG(gDocLoaderLog, PR_LOG_DEBUG,
           ("DocLoader:%p: OnStopRequest[%p](%s) status=%x mIsLoadingDocument=%s, %u active URLs",
           this, aRequest, name.get(),
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
    PRBool bFireTransferring = PR_FALSE;

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
        mMaxSelfProgress = CalculateMaxProgress();
      }

      //
      // Determine whether a STATE_TRANSFERRING notification should be 
      // 'synthesized'.
      //
      // If nsRequestInfo::mMaxProgress (as stored in oldMax) and
      // nsRequestInfo::mCurrentProgress are both 0, then the
      // STATE_TRANSFERRING notification has not been fired yet...
      //
      if ((oldMax == 0) && (info->mCurrentProgress == 0)) {
        nsCOMPtr<nsIChannel> channel(do_QueryInterface(aRequest));

        // Only fire a TRANSFERRING notification if the request is also a
        // channel -- data transfer requires a nsIChannel!
        //
        if (channel) {
          if (NS_SUCCEEDED(aStatus)) {
            bFireTransferring = PR_TRUE;
          } 
          //
          // If the request failed (for any reason other than being
          // redirected or retargeted), the TRANSFERRING notification can
          // still be fired if a HTTP connection was established to a server.
          //
          else if (aStatus != NS_BINDING_REDIRECTED &&
                   aStatus != NS_BINDING_RETARGETED) {
            nsCOMPtr<nsIHttpChannel> httpChannel(do_QueryInterface(aRequest));

            if (httpChannel) {
              PRUint32 responseCode;

              rv = httpChannel->GetResponseStatus(&responseCode);
              if (NS_SUCCEEDED(rv)) {
                //
                // A valid server status indicates that a connection was
                // established to the server... So, fire the notification
                // even though a failure occurred later...
                //
                bFireTransferring = PR_TRUE;
              }
            }
          }
        }
      }
    }

    if (bFireTransferring) {
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
    mChildList.RemoveObject((nsIDocumentLoader*)aLoader);
  }
  return rv;
}

NS_IMETHODIMP nsDocLoaderImpl::GetDocumentChannel(nsIChannel ** aChannel)
{
  if (!mDocumentRequest) {
    *aChannel = nsnull;
    return NS_OK;
  }
  
  return CallQueryInterface(mDocumentRequest, aChannel);
}


void nsDocLoaderImpl::DocLoaderIsEmpty()
{
  if (mIsLoadingDocument) {
    /* In the unimagineably rude circumstance that onload event handlers
       triggered by this function actually kill the window ... ok, it's
       not unimagineable; it's happened ... this deathgrip keeps this object
       alive long enough to survive this function call. */
    nsCOMPtr<nsIDocumentLoader> kungFuDeathGrip(this);

    if (!IsBusy()) {
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
  nsCAutoString buffer;

  GetURIStringFromRequest(mDocumentRequest, buffer);
  PR_LOG(gDocLoaderLog, PR_LOG_DEBUG, 
         ("DocLoader:%p: ++ Firing OnStateChange for start document load (...)."
          "\tURI: %s \n",
          this, buffer.get()));
#endif /* DEBUG */

  // Fire an OnStatus(...) notification STATE_START.  This indicates
  // that the document represented by mDocumentRequest has started to
  // load...
  FireOnStateChange(this,
                    mDocumentRequest,
                    nsIWebProgressListener::STATE_START |
                    nsIWebProgressListener::STATE_IS_DOCUMENT |
                    nsIWebProgressListener::STATE_IS_REQUEST |
                    nsIWebProgressListener::STATE_IS_WINDOW |
                    nsIWebProgressListener::STATE_IS_NETWORK,
                    NS_OK);
}

void nsDocLoaderImpl::doStartURLLoad(nsIRequest *request)
{
#if defined(DEBUG)
  nsCAutoString buffer;

  GetURIStringFromRequest(request, buffer);
    PR_LOG(gDocLoaderLog, PR_LOG_DEBUG, 
          ("DocLoader:%p: ++ Firing OnStateChange start url load (...)."
           "\tURI: %s\n",
            this, buffer.get()));
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
  nsCAutoString buffer;

  GetURIStringFromRequest(request, buffer);
    PR_LOG(gDocLoaderLog, PR_LOG_DEBUG, 
          ("DocLoader:%p: ++ Firing OnStateChange for end url load (...)."
           "\tURI: %s status=%x\n",
            this, buffer.get(), aStatus));
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
  nsCAutoString buffer;

  GetURIStringFromRequest(request, buffer);
  PR_LOG(gDocLoaderLog, PR_LOG_DEBUG, 
         ("DocLoader:%p: ++ Firing OnStateChange for end document load (...)."
         "\tURI: %s Status=%x\n",
          this, buffer.get(), aStatus));
#endif /* DEBUG */

  //
  // Fire an OnStateChange(...) notification indicating the the
  // current document has finished loading...
  //
  FireOnStateChange(this,
                    request,
                    nsIWebProgressListener::STATE_STOP |
                    nsIWebProgressListener::STATE_IS_DOCUMENT,
                    aStatus);

  //
  // Fire a final OnStateChange(...) notification indicating the the
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
nsDocLoaderImpl::AddProgressListener(nsIWebProgressListener *aListener,
                                     PRUint32 aNotifyMask)
{
  nsresult rv;
  nsWeakPtr listener = do_GetWeakReference(aListener);
  if (!listener) {
    return NS_ERROR_INVALID_ARG;
  }

  nsListenerInfo* info = GetListenerInfo(listener);
  if (info) {
    // The listener is already registered!
    return NS_ERROR_FAILURE;
  }

  info = new nsListenerInfo(listener, aNotifyMask);
  if (!info) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  rv = mListenerInfoList.AppendElement(info) ? NS_OK : NS_ERROR_FAILURE;
  return rv;
}

NS_IMETHODIMP
nsDocLoaderImpl::RemoveProgressListener(nsIWebProgressListener *aListener)
{
  nsresult rv;
  nsWeakPtr listener = do_GetWeakReference(aListener);
  if (!listener) {
    return NS_ERROR_INVALID_ARG;
  }

  nsListenerInfo* info = GetListenerInfo(listener);
  if (info) {
    rv = mListenerInfoList.RemoveElement(info) ? NS_OK : NS_ERROR_FAILURE;
    delete info;
  } else {
    // The listener is not registered!
    rv = NS_ERROR_FAILURE;
  }
  return rv;
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

NS_IMETHODIMP
nsDocLoaderImpl::GetIsLoadingDocument(PRBool *aIsLoadingDocument)
{
  *aIsLoadingDocument = mIsLoadingDocument;

  return NS_OK;
}

PRInt32 nsDocLoaderImpl::GetMaxTotalProgress()
{
  PRInt32 count = 0;
  PRInt32 individualProgress, newMaxTotal;

  newMaxTotal = 0;

  count = mChildList.Count();
  nsCOMPtr<nsIWebProgress> webProgress;
  nsCOMPtr<nsIDocumentLoader> docloader;
  for (PRInt32 i=0; i < count; i++) 
  {
    individualProgress = 0;
    docloader = mChildList.ObjectAt(i);
    if (docloader)
    {
      // Cast is safe since all children are nsDocLoaderImpl too
      individualProgress = ((nsDocLoaderImpl *) docloader.get())->GetMaxTotalProgress();
    }
    if (individualProgress < 0) // if one of the elements doesn't know it's size
                                // then none of them do
    {
       newMaxTotal = -1;
       break;
    }
    else
     newMaxTotal += individualProgress;
  }

  PRInt32 progress = -1;
  if (mMaxSelfProgress >= 0 && newMaxTotal >= 0)
    progress = newMaxTotal + mMaxSelfProgress;
  
  return progress;
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
  // Update the RequestInfo entry with the new progress data
  //
  info = GetRequestInfo(aRequest);
  if (info) {
    // suppress sending STATE_TRANSFERRING if this is upload progress (see bug 240053)
    if (!info->mUploading && (0 == info->mCurrentProgress) && (0 == info->mMaxProgress)) {
      //
      // If we receive an OnProgress event from a toplevel channel that the URI Loader
      // has not yet targeted, then we must suppress the event.  This is necessary to
      // ensure that webprogresslisteners do not get confused when the channel is
      // finally targeted.  See bug 257308.
      //
      nsLoadFlags lf = 0;
      aRequest->GetLoadFlags(&lf);
      if ((lf & nsIChannel::LOAD_DOCUMENT_URI) && !(lf & nsIChannel::LOAD_TARGETED)) {
        PR_LOG(gDocLoaderLog, PR_LOG_DEBUG, 
            ("DocLoader:%p Ignoring OnProgress while load is not targeted\n", this));
        return NS_OK;
      }

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
    nsCAutoString buffer;

    GetURIStringFromRequest(aRequest, buffer);
    PR_LOG(gDocLoaderLog, PR_LOG_DEBUG, 
           ("DocLoader:%p OOPS - No Request Info for: %s\n",
            this, buffer.get()));
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
    // Remember the current status for this request
    nsRequestInfo *info;
    info = GetRequestInfo(aRequest);
    if (info) {
      PRBool uploading = (aStatus == nsITransport::STATUS_WRITING ||
                          aStatus == nsISocketTransport::STATUS_SENDING_TO);
      // If switching from uploading to downloading (or vice versa), then we
      // need to reset our progress counts.  This is designed with HTTP form
      // submission in mind, where an upload is performed followed by download
      // of possibly several documents.
      if (info->mUploading != uploading) {
        mCurrentSelfProgress  = mMaxSelfProgress  = 0;
        mCurrentTotalProgress = mMaxTotalProgress = 0;
        info->mUploading = uploading;
        info->mCurrentProgress = 0;
        info->mMaxProgress = 0;
      }
    }
    
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
  ClearRequestInfoHash();

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
  if (mIsLoadingDocument) {
    mCurrentTotalProgress += aProgressDelta;
    mMaxTotalProgress = GetMaxTotalProgress();

    aTotalProgress    = mCurrentTotalProgress;
    aMaxTotalProgress = mMaxTotalProgress;
  }

#if defined(DEBUG)
  nsCAutoString buffer;

  GetURIStringFromRequest(request, buffer);
  PR_LOG(gDocLoaderLog, PR_LOG_DEBUG, 
         ("DocLoader:%p: Progress (%s): curSelf: %d maxSelf: %d curTotal: %d maxTotal %d\n",
          this, buffer.get(), aProgress, aProgressMax, aTotalProgress, aMaxTotalProgress));
#endif /* DEBUG */

  /*
   * First notify any listeners of the new progress info...
   *
   * Operate the elements from back to front so that if items get
   * get removed from the list it won't affect our iteration
   */
  nsCOMPtr<nsIWebProgressListener> listener;
  PRInt32 count = mListenerInfoList.Count();

  while (--count >= 0) {
    nsListenerInfo *info;

    info = NS_STATIC_CAST(nsListenerInfo*,mListenerInfoList.SafeElementAt(count));
    if (!info || !(info->mNotifyMask & nsIWebProgress::NOTIFY_PROGRESS)) {
      continue;
    }

    listener = do_QueryReferent(info->mWeakListener);
    if (!listener) {
      // the listener went away. gracefully pull it out of the list.
      mListenerInfoList.RemoveElementAt(count);
      delete info;
      continue;
    }

    listener->OnProgressChange(aLoadInitiator,request,
                               aProgress, aProgressMax,
                               aTotalProgress, aMaxTotalProgress);
  }

  mListenerInfoList.Compact();

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
  nsCAutoString buffer;

  GetURIStringFromRequest(aRequest, buffer);
  PR_LOG(gDocLoaderLog, PR_LOG_DEBUG, 
         ("DocLoader:%p: Status (%s): code: %x\n",
         this, buffer.get(), aStateFlags));
#endif /* DEBUG */

  NS_ASSERTION(aRequest, "Firing OnStateChange(...) notification with a NULL request!");

  /*                                                                           
   * First notify any listeners of the new state info...
   *
   * Operate the elements from back to front so that if items get
   * get removed from the list it won't affect our iteration
   */
  nsCOMPtr<nsIWebProgressListener> listener;
  PRInt32 count = mListenerInfoList.Count();

  while (--count >= 0) {
    nsListenerInfo *info;

    info = NS_STATIC_CAST(nsListenerInfo*,mListenerInfoList.SafeElementAt(count));
    if (!info || !(info->mNotifyMask & (aStateFlags >>16))) {
      continue;
    }

    listener = do_QueryReferent(info->mWeakListener);
    if (!listener) {
      // the listener went away. gracefully pull it out of the list.
      mListenerInfoList.RemoveElementAt(count);
      delete info;
      continue;
    }

    listener->OnStateChange(aProgress, aRequest, aStateFlags, aStatus);
  }

  mListenerInfoList.Compact();

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
  /*                                                                           
   * First notify any listeners of the new state info...
   *
   * Operate the elements from back to front so that if items get
   * get removed from the list it won't affect our iteration
   */
  nsCOMPtr<nsIWebProgressListener> listener;
  PRInt32 count = mListenerInfoList.Count();

  while (--count >= 0) {
    nsListenerInfo *info;

    info = NS_STATIC_CAST(nsListenerInfo*,mListenerInfoList.SafeElementAt(count));
    if (!info || !(info->mNotifyMask & nsIWebProgress::NOTIFY_LOCATION)) {
      continue;
    }

    listener = do_QueryReferent(info->mWeakListener);
    if (!listener) {
      // the listener went away. gracefully pull it out of the list.
      mListenerInfoList.RemoveElementAt(count);
      delete info;
      continue;
    }

    listener->OnLocationChange(aWebProgress, aRequest, aUri);
  }

  mListenerInfoList.Compact();

  // Pass the notification up to the parent...
  if (mParent) {
    mParent->FireOnLocationChange(aWebProgress, aRequest, aUri);
  }

  return NS_OK;
}

void
nsDocLoaderImpl::FireOnStatusChange(nsIWebProgress* aWebProgress,
                                    nsIRequest* aRequest,
                                    nsresult aStatus,
                                    const PRUnichar* aMessage)
{
  /*                                                                           
   * First notify any listeners of the new state info...
   *
   * Operate the elements from back to front so that if items get
   * get removed from the list it won't affect our iteration
   */
  nsCOMPtr<nsIWebProgressListener> listener;
  PRInt32 count = mListenerInfoList.Count();

  while (--count >= 0) {
    nsListenerInfo *info;

    info = NS_STATIC_CAST(nsListenerInfo*,mListenerInfoList.SafeElementAt(count));
    if (!info || !(info->mNotifyMask & nsIWebProgress::NOTIFY_STATUS)) {
      continue;
    }

    listener = do_QueryReferent(info->mWeakListener);
    if (!listener) {
      // the listener went away. gracefully pull it out of the list.
      mListenerInfoList.RemoveElementAt(count);
      delete info;
      continue;
    }

    listener->OnStatusChange(aWebProgress, aRequest, aStatus, aMessage);
  }
  mListenerInfoList.Compact();
  
  // Pass the notification up to the parent...
  if (mParent) {
    mParent->FireOnStatusChange(aWebProgress, aRequest, aStatus, aMessage);
  }
}

nsListenerInfo * 
nsDocLoaderImpl::GetListenerInfo(nsIWeakReference *aListener)
{
  PRInt32 i, count;
  nsListenerInfo *info;

  count = mListenerInfoList.Count();
  for (i=0; i<count; i++) {
    info = NS_STATIC_CAST(nsListenerInfo* ,mListenerInfoList.SafeElementAt(i));

    NS_ASSERTION(info, "There should NEVER be a null listener in the list");
    if (info && (aListener == info->mWeakListener)) {
      return info;
    }
  }
  return nsnull;
}

nsresult nsDocLoaderImpl::AddRequestInfo(nsIRequest *aRequest)
{
  if (!PL_DHashTableOperate(&mRequestInfoHash, aRequest, PL_DHASH_ADD)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return NS_OK;
}

nsRequestInfo * nsDocLoaderImpl::GetRequestInfo(nsIRequest *aRequest)
{
  nsRequestInfo *info =
    NS_STATIC_CAST(nsRequestInfo *,
                   PL_DHashTableOperate(&mRequestInfoHash, aRequest,
                                        PL_DHASH_LOOKUP));

  if (PL_DHASH_ENTRY_IS_FREE(info)) {
    // Nothing found in the hash, return null.

    return nsnull;
  }

  // Return what we found in the hash...

  return info;
}

// PLDHashTable enumeration callback that just removes every entry
// from the hash.
PR_STATIC_CALLBACK(PLDHashOperator)
RemoveInfoCallback(PLDHashTable *table, PLDHashEntryHdr *hdr, PRUint32 number,
                   void *arg)
{
  return PL_DHASH_REMOVE;
}

void nsDocLoaderImpl::ClearRequestInfoHash(void)
{
  if (!mRequestInfoHash.ops || !mRequestInfoHash.entryCount) {
    // No hash, or the hash is empty, nothing to do here then...

    return;
  }

  PL_DHashTableEnumerate(&mRequestInfoHash, RemoveInfoCallback, nsnull);
}

// PLDHashTable enumeration callback that calculates the max progress.
PR_STATIC_CALLBACK(PLDHashOperator)
CalcMaxProgressCallback(PLDHashTable *table, PLDHashEntryHdr *hdr,
                        PRUint32 number, void *arg)
{
  const nsRequestInfo *info = NS_STATIC_CAST(const nsRequestInfo *, hdr);
  PRInt32 *max = NS_STATIC_CAST(PRInt32 *, arg);

  if (info->mMaxProgress < info->mCurrentProgress) {
    *max = -1;

    return PL_DHASH_STOP;
  }

  *max += info->mMaxProgress;

  return PL_DHASH_NEXT;
}

PRInt32 nsDocLoaderImpl::CalculateMaxProgress()
{
  PRInt32 max = 0;
  PL_DHashTableEnumerate(&mRequestInfoHash, CalcMaxProgressCallback, &max);
  return max;
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

/*
 * Implementation of nsISecurityEventSink method...
 */

NS_IMETHODIMP nsDocLoaderImpl::OnSecurityChange(nsISupports * aContext,
                                                PRUint32 aState)
{
  //
  // Fire progress notifications out to any registered nsIWebProgressListeners.  
  //
  
  nsCOMPtr<nsIRequest> request = do_QueryInterface(aContext);
  nsIWebProgress* webProgress = NS_STATIC_CAST(nsIWebProgress*, this);

  /*                                                                           
   * First notify any listeners of the new state info...
   *
   * Operate the elements from back to front so that if items get
   * get removed from the list it won't affect our iteration
   */
  nsCOMPtr<nsIWebProgressListener> listener;
  PRInt32 count = mListenerInfoList.Count();

  while (--count >= 0) {
    nsListenerInfo *info;

    info = NS_STATIC_CAST(nsListenerInfo*,mListenerInfoList.SafeElementAt(count));
    if (!info || !(info->mNotifyMask & nsIWebProgress::NOTIFY_SECURITY)) {
      continue;
    }

    listener = do_QueryReferent(info->mWeakListener);
    if (!listener) {
      // the listener went away. gracefully pull it out of the list.
      mListenerInfoList.RemoveElementAt(count);
      delete info;
      continue;
    }

    listener->OnSecurityChange(webProgress, request, aState);
  }

  mListenerInfoList.Compact();

  // Pass the notification up to the parent...
  if (mParent) {
    mParent->OnSecurityChange(aContext, aState);
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
    nsCAutoString buffer;
    nsresult rv = NS_OK;
    if (info->mURI) {
      rv = info->mURI->GetSpec(buffer);
    }

    printf("  [%d] current=%d  max=%d [%s]\n", i,
           info->mCurrentProgress, 
           info->mMaxProgress, buffer.get());
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

