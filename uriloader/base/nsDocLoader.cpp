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
#include "nsIWebProgressListener2.h"

#include "nsIServiceManager.h"
#include "nsXPIDLString.h"

#include "nsIURL.h"
#include "nsCOMPtr.h"
#include "nscore.h"
#include "nsWeakPtr.h"
#include "nsAutoPtr.h"

#include "nsIDOMWindow.h"

#include "nsIStringBundle.h"
#include "nsIScriptSecurityManager.h"

#include "nsITransport.h"
#include "nsISocketTransport.h"

#include "nsIDOMDocument.h"
#include "nsIDocument.h"
#include "nsPresContext.h"
#include "nsIAsyncVerifyRedirectCallback.h"

static NS_DEFINE_CID(kThisImplCID, NS_THIS_DOCLOADER_IMPL_CID);

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

struct nsStatusInfo : public PRCList
{
  nsString mStatusMessage;
  nsresult mStatusCode;
  // Weak mRequest is ok; we'll be told if it decides to go away.
  nsIRequest * const mRequest;

  nsStatusInfo(nsIRequest *aRequest) :
    mRequest(aRequest)
  {
    MOZ_COUNT_CTOR(nsStatusInfo);
    PR_INIT_CLIST(this);
  }
  ~nsStatusInfo()
  {
    MOZ_COUNT_DTOR(nsStatusInfo);
    PR_REMOVE_LINK(this);
  }
};

struct nsRequestInfo : public PLDHashEntryHdr
{
  nsRequestInfo(const void *key)
    : mKey(key), mCurrentProgress(0), mMaxProgress(0), mUploading(PR_FALSE)
    , mLastStatus(nsnull)
  {
    MOZ_COUNT_CTOR(nsRequestInfo);
  }

  ~nsRequestInfo()
  {
    MOZ_COUNT_DTOR(nsRequestInfo);
  }

  nsIRequest* Request() {
    return static_cast<nsIRequest*>(const_cast<void*>(mKey));
  }

  const void* mKey; // Must be first for the pldhash stubs to work
  PRInt64 mCurrentProgress;
  PRInt64 mMaxProgress;
  PRBool mUploading;

  nsAutoPtr<nsStatusInfo> mLastStatus;
};


static PRBool
RequestInfoHashInitEntry(PLDHashTable *table, PLDHashEntryHdr *entry,
                         const void *key)
{
  // Initialize the entry with placement new
  new (entry) nsRequestInfo(key);
  return PR_TRUE;
}

static void
RequestInfoHashClearEntry(PLDHashTable *table, PLDHashEntryHdr *entry)
{
  nsRequestInfo* info = static_cast<nsRequestInfo *>(entry);
  info->~nsRequestInfo();
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


nsDocLoader::nsDocLoader()
  : mParent(nsnull),
    mListenerInfoList(8),
    mCurrentSelfProgress(0),
    mMaxSelfProgress(0),
    mCurrentTotalProgress(0),
    mMaxTotalProgress(0),
    mCompletedTotalProgress(0),
    mIsLoadingDocument(PR_FALSE),
    mIsRestoringDocument(PR_FALSE),
    mDontFlushLayout(PR_FALSE),
    mIsFlushingLayout(PR_FALSE)
{
#if defined(PR_LOGGING)
  if (nsnull == gDocLoaderLog) {
      gDocLoaderLog = PR_NewLogModule("DocLoader");
  }
#endif /* PR_LOGGING */

  static PLDHashTableOps hash_table_ops =
  {
    PL_DHashAllocTable,
    PL_DHashFreeTable,
    PL_DHashVoidPtrKeyStub,
    PL_DHashMatchEntryStub,
    PL_DHashMoveEntryStub,
    RequestInfoHashClearEntry,
    PL_DHashFinalizeStub,
    RequestInfoHashInitEntry
  };

  if (!PL_DHashTableInit(&mRequestInfoHash, &hash_table_ops, nsnull,
                         sizeof(nsRequestInfo), 16)) {
    mRequestInfoHash.ops = nsnull;
  }

  ClearInternalProgress();

  PR_INIT_CLIST(&mStatusInfoList);

  PR_LOG(gDocLoaderLog, PR_LOG_DEBUG, 
         ("DocLoader:%p: created.\n", this));
}

nsresult
nsDocLoader::SetDocLoaderParent(nsDocLoader *aParent)
{
  mParent = aParent;
  return NS_OK; 
}

nsresult
nsDocLoader::Init()
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

nsDocLoader::~nsDocLoader()
{
		/*
			|ClearWeakReferences()| here is intended to prevent people holding weak references
			from re-entering this destructor since |QueryReferent()| will |AddRef()| me, and the
			subsequent |Release()| will try to destroy me.  At this point there should be only
			weak references remaining (otherwise, we wouldn't be getting destroyed).

			An alternative would be incrementing our refcount (consider it a compressed flag
			saying "Don't re-destroy.").  I haven't yet decided which is better. [scc]
		*/
  // XXXbz now that NS_IMPL_RELEASE stabilizes by setting refcount to 1, is
  // this needed?
  ClearWeakReferences();

  Destroy();

  PR_LOG(gDocLoaderLog, PR_LOG_DEBUG,
         ("DocLoader:%p: deleted.\n", this));

  if (mRequestInfoHash.ops) {
    PL_DHashTableFinish(&mRequestInfoHash);
  }
}


/*
 * Implementation of ISupports methods...
 */
NS_IMPL_THREADSAFE_ADDREF(nsDocLoader)
NS_IMPL_THREADSAFE_RELEASE(nsDocLoader)

NS_INTERFACE_MAP_BEGIN(nsDocLoader)
   NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIRequestObserver)
   NS_INTERFACE_MAP_ENTRY(nsIRequestObserver)
   NS_INTERFACE_MAP_ENTRY(nsIDocumentLoader)
   NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
   NS_INTERFACE_MAP_ENTRY(nsIWebProgress)
   NS_INTERFACE_MAP_ENTRY(nsIProgressEventSink)   
   NS_INTERFACE_MAP_ENTRY(nsIInterfaceRequestor)
   NS_INTERFACE_MAP_ENTRY(nsIChannelEventSink)
   NS_INTERFACE_MAP_ENTRY(nsISecurityEventSink)
   NS_INTERFACE_MAP_ENTRY(nsISupportsPriority)
   if (aIID.Equals(kThisImplCID))
     foundInterface = static_cast<nsIDocumentLoader *>(this);
   else
NS_INTERFACE_MAP_END


/*
 * Implementation of nsIInterfaceRequestor methods...
 */
NS_IMETHODIMP nsDocLoader::GetInterface(const nsIID& aIID, void** aSink)
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

/* static */
already_AddRefed<nsDocLoader>
nsDocLoader::GetAsDocLoader(nsISupports* aSupports)
{
  if (!aSupports) {
    return nsnull;
  }
  
  nsDocLoader* ptr;
  CallQueryInterface(aSupports, &ptr);
  return ptr;
}

/* static */
nsresult
nsDocLoader::AddDocLoaderAsChildOfRoot(nsDocLoader* aDocLoader)
{
  nsresult rv;
  nsCOMPtr<nsIDocumentLoader> docLoaderService =
    do_GetService(NS_DOCUMENTLOADER_SERVICE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsRefPtr<nsDocLoader> rootDocLoader = GetAsDocLoader(docLoaderService);
  NS_ENSURE_TRUE(rootDocLoader, NS_ERROR_UNEXPECTED);

  return rootDocLoader->AddChildLoader(aDocLoader);
}

NS_IMETHODIMP
nsDocLoader::Stop(void)
{
  nsresult rv = NS_OK;
  PRInt32 count, i;

  PR_LOG(gDocLoaderLog, PR_LOG_DEBUG, 
         ("DocLoader:%p: Stop() called\n", this));

  count = mChildList.Count();

  nsCOMPtr<nsIDocumentLoader> loader;
  for (i=0; i < count; i++) {
    loader = ChildAt(i);

    if (loader) {
      (void) loader->Stop();
    }
  }

  if (mLoadGroup)
    rv = mLoadGroup->Cancel(NS_BINDING_ABORTED);

  // Don't report that we're flushing layout so IsBusy returns false after a
  // Stop call.
  mIsFlushingLayout = PR_FALSE;

  // Clear out mChildrenInOnload.  We want to make sure to fire our
  // onload at this point, and there's no issue with mChildrenInOnload
  // after this, since mDocumentRequest will be null after the
  // DocLoaderIsEmpty() call.
  mChildrenInOnload.Clear();

  // Make sure to call DocLoaderIsEmpty now so that we reset mDocumentRequest,
  // etc, as needed.  We could be getting into here from a subframe onload, in
  // which case the call to DocLoaderIsEmpty() is coming but hasn't quite
  // happened yet, Canceling the loadgroup did nothing (because it was already
  // empty), and we're about to start a new load (which is what triggered this
  // Stop() call).

  // XXXbz If the child frame loadgroups were requests in mLoadgroup, I suspect
  // we wouldn't need the call here....

  NS_ASSERTION(!IsBusy(), "Shouldn't be busy here");
  DocLoaderIsEmpty(PR_FALSE);
  
  return rv;
}       


PRBool
nsDocLoader::IsBusy()
{
  nsresult rv;

  //
  // A document loader is busy if either:
  //
  //   1. One of its children is in the middle of an onload handler.  Note that
  //      the handler may have already removed this child from mChildList!
  //   2. It is currently loading a document and either has parts of it still
  //      loading, or has a busy child docloader.
  //   3. It's currently flushing layout in DocLoaderIsEmpty().
  //

  if (mChildrenInOnload.Count() || mIsFlushingLayout) {
    return PR_TRUE;
  }

  /* Is this document loader busy? */
  if (!mIsLoadingDocument) {
    return PR_FALSE;
  }
  
  PRBool busy;
  rv = mLoadGroup->IsPending(&busy);
  if (NS_FAILED(rv)) {
    return PR_FALSE;
  }
  if (busy) {
    return PR_TRUE;
  }

  /* check its child document loaders... */
  PRInt32 count, i;

  count = mChildList.Count();

  for (i=0; i < count; i++) {
    nsIDocumentLoader* loader = ChildAt(i);

    // This is a safe cast, because we only put nsDocLoader objects into the
    // array
    if (loader && static_cast<nsDocLoader*>(loader)->IsBusy())
      return PR_TRUE;
  }

  return PR_FALSE;
}

NS_IMETHODIMP
nsDocLoader::GetContainer(nsISupports** aResult)
{
   NS_ADDREF(*aResult = static_cast<nsIDocumentLoader*>(this));

   return NS_OK;
}

NS_IMETHODIMP
nsDocLoader::GetLoadGroup(nsILoadGroup** aResult)
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

void
nsDocLoader::Destroy()
{
  Stop();

  // Remove the document loader from the parent list of loaders...
  if (mParent) 
  {
    mParent->RemoveChildLoader(this);
  }

  // Release all the information about network requests...
  ClearRequestInfoHash();

  // Release all the information about registered listeners...
  PRInt32 count = mListenerInfoList.Count();
  for(PRInt32 i = 0; i < count; i++) {
    nsListenerInfo *info =
      static_cast<nsListenerInfo*>(mListenerInfoList.ElementAt(i));

    delete info;
  }

  mListenerInfoList.Clear();
  mListenerInfoList.Compact();

  mDocumentRequest = 0;

  if (mLoadGroup)
    mLoadGroup->SetGroupObserver(nsnull);

  DestroyChildren();
}

void
nsDocLoader::DestroyChildren()
{
  PRInt32 i, count;
  
  count = mChildList.Count();
  // if the doc loader still has children...we need to enumerate the
  // children and make them null out their back ptr to the parent doc
  // loader
  for (i=0; i < count; i++)
  {
    nsIDocumentLoader* loader = ChildAt(i);

    if (loader) {
      // This is a safe cast, as we only put nsDocLoader objects into the
      // array
      static_cast<nsDocLoader*>(loader)->SetDocLoaderParent(nsnull);
    }
  }
  mChildList.Clear();
}

NS_IMETHODIMP
nsDocLoader::OnStartRequest(nsIRequest *request, nsISupports *aCtxt)
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
  // Create a new nsRequestInfo for the request that is starting to
  // load...
  //
  AddRequestInfo(request);

  //
  // Only fire a doStartDocumentLoad(...) if the document loader
  // has initiated a load...  Otherwise, this notification has
  // resulted from a request being added to the load group.
  //
  if (mIsLoadingDocument) {
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

  NS_ASSERTION(!mIsLoadingDocument || mDocumentRequest,
               "mDocumentRequest MUST be set for the duration of a page load!");

  doStartURLLoad(request);

  return NS_OK;
}

NS_IMETHODIMP
nsDocLoader::OnStopRequest(nsIRequest *aRequest, 
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

  PRBool bFireTransferring = PR_FALSE;

  //
  // Set the Maximum progress to the same value as the current progress.
  // Since the URI has finished loading, all the data is there.  Also,
  // this will allow a more accurate estimation of the max progress (in case
  // the old value was unknown ie. -1)
  //
  nsRequestInfo *info = GetRequestInfo(aRequest);
  if (info) {
    if (info->mLastStatus) {
      // Null it out now so we don't find it when looking for status
      // from now on.  This destroys the nsStatusInfo and hence
      // removes it from our list.
      info->mLastStatus = nsnull;
    }

    PRInt64 oldMax = info->mMaxProgress;

    info->mMaxProgress = info->mCurrentProgress;
    
    //
    // If a request whose content-length was previously unknown has just
    // finished loading, then use this new data to try to calculate a
    // mMaxSelfProgress...
    //
    if ((oldMax < PRInt64(0)) && (mMaxSelfProgress < PRInt64(0))) {
      mMaxSelfProgress = CalculateMaxProgress();
    }

    // As we know the total progress of this request now, save it to be part
    // of CalculateMaxProgress() result. We need to remove the info from the
    // hash, see bug 480713.
    mCompletedTotalProgress += info->mMaxProgress;
    
    //
    // Determine whether a STATE_TRANSFERRING notification should be
    // 'synthesized'.
    //
    // If nsRequestInfo::mMaxProgress (as stored in oldMax) and
    // nsRequestInfo::mCurrentProgress are both 0, then the
    // STATE_TRANSFERRING notification has not been fired yet...
    //
    if ((oldMax == LL_ZERO) && (info->mCurrentProgress == LL_ZERO)) {
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
          //
          // Only if the load has been targeted (see bug 268483)...
          //
          PRUint32 lf;
          channel->GetLoadFlags(&lf);
          if (lf & nsIChannel::LOAD_TARGETED) {
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
  
  // Clear this request out of the hash to avoid bypass of FireOnStateChange
  // when address of the request is reused.
  RemoveRequestInfo(aRequest);
  
  //
  // Only fire the DocLoaderIsEmpty(...) if the document loader has initiated a
  // load.  This will handle removing the request from our hashtable as needed.
  //
  if (mIsLoadingDocument) {
    DocLoaderIsEmpty(PR_TRUE);
  }
  
  return NS_OK;
}


nsresult nsDocLoader::RemoveChildLoader(nsDocLoader* aChild)
{
  nsresult rv = mChildList.RemoveElement(aChild) ? NS_OK : NS_ERROR_FAILURE;
  if (NS_SUCCEEDED(rv)) {
    aChild->SetDocLoaderParent(nsnull);
  }
  return rv;
}

nsresult nsDocLoader::AddChildLoader(nsDocLoader* aChild)
{
  nsresult rv = mChildList.AppendElement(aChild) ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
  if (NS_SUCCEEDED(rv)) {
    aChild->SetDocLoaderParent(this);
  }
  return rv;
}

NS_IMETHODIMP nsDocLoader::GetDocumentChannel(nsIChannel ** aChannel)
{
  if (!mDocumentRequest) {
    *aChannel = nsnull;
    return NS_OK;
  }
  
  return CallQueryInterface(mDocumentRequest, aChannel);
}


void nsDocLoader::DocLoaderIsEmpty(PRBool aFlushLayout)
{
  if (mIsLoadingDocument) {
    /* In the unimagineably rude circumstance that onload event handlers
       triggered by this function actually kill the window ... ok, it's
       not unimagineable; it's happened ... this deathgrip keeps this object
       alive long enough to survive this function call. */
    nsCOMPtr<nsIDocumentLoader> kungFuDeathGrip(this);

    // Don't flush layout if we're still busy.
    if (IsBusy()) {
      return;
    }

    NS_ASSERTION(!mIsFlushingLayout, "Someone screwed up");

    // The load group for this DocumentLoader is idle.  Flush if we need to.
    if (aFlushLayout && !mDontFlushLayout) {
      nsCOMPtr<nsIDOMDocument> domDoc = do_GetInterface(GetAsSupports(this));
      nsCOMPtr<nsIDocument> doc = do_QueryInterface(domDoc);
      if (doc) {
        // We start loads from style resolution, so we need to flush out style
        // no matter what.  If we have user fonts, we also need to flush layout,
        // since the reflow is what starts font loads.
        mozFlushType flushType = Flush_Style;
        nsIPresShell* shell = doc->GetShell();
        if (shell) {
          // Be safe in case this presshell is in teardown now
          nsPresContext* presContext = shell->GetPresContext();
          if (presContext && presContext->GetUserFontSet()) {
            flushType = Flush_Layout;
          }
        }
        mDontFlushLayout = mIsFlushingLayout = PR_TRUE;
        doc->FlushPendingNotifications(flushType);
        mDontFlushLayout = mIsFlushingLayout = PR_FALSE;
      }
    }

    // And now check whether we're really busy; that might have changed with
    // the layout flush.
    if (!IsBusy()) {
      // Clear out our request info hash, now that our load really is done and
      // we don't need it anymore to CalculateMaxProgress().
      ClearInternalProgress();

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

      // Take a ref to our parent now so that we can call DocLoaderIsEmpty() on
      // it even if our onload handler removes us from the docloader tree.
      nsRefPtr<nsDocLoader> parent = mParent;

      // Note that if calling ChildEnteringOnload() on the parent returns false
      // then calling our onload handler is not safe.  That can only happen on
      // OOM, so that's ok.
      if (!parent || parent->ChildEnteringOnload(this)) {
        // Do nothing with our state after firing the
        // OnEndDocumentLoad(...). The document loader may be loading a *new*
        // document - if LoadDocument() was called from a handler!
        //
        doStopDocumentLoad(docRequest, loadGroupStatus);

        if (parent) {
          parent->ChildDoneWithOnload(this);
        }
      }
    }
  }
}

void nsDocLoader::doStartDocumentLoad(void)
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

void nsDocLoader::doStartURLLoad(nsIRequest *request)
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

void nsDocLoader::doStopURLLoad(nsIRequest *request, nsresult aStatus)
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

  // Fire a status change message for the most recent unfinished
  // request to make sure that the displayed status is not outdated.
  if (!PR_CLIST_IS_EMPTY(&mStatusInfoList)) {
    nsStatusInfo* statusInfo =
      static_cast<nsStatusInfo*>(PR_LIST_HEAD(&mStatusInfoList));
    FireOnStatusChange(this, statusInfo->mRequest,
                       statusInfo->mStatusCode,
                       statusInfo->mStatusMessage.get());
  }
}

void nsDocLoader::doStopDocumentLoad(nsIRequest *request,
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

NS_IMETHODIMP
nsDocLoader::AddProgressListener(nsIWebProgressListener *aListener,
                                     PRUint32 aNotifyMask)
{
  nsresult rv;

  nsListenerInfo* info = GetListenerInfo(aListener);
  if (info) {
    // The listener is already registered!
    return NS_ERROR_FAILURE;
  }

  nsWeakPtr listener = do_GetWeakReference(aListener);
  if (!listener) {
    return NS_ERROR_INVALID_ARG;
  }

  info = new nsListenerInfo(listener, aNotifyMask);
  if (!info) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  rv = mListenerInfoList.AppendElement(info) ? NS_OK : NS_ERROR_FAILURE;
  return rv;
}

NS_IMETHODIMP
nsDocLoader::RemoveProgressListener(nsIWebProgressListener *aListener)
{
  nsresult rv;

  nsListenerInfo* info = GetListenerInfo(aListener);
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
nsDocLoader::GetDOMWindow(nsIDOMWindow **aResult)
{
  return CallGetInterface(this, aResult);
}

NS_IMETHODIMP
nsDocLoader::GetIsLoadingDocument(PRBool *aIsLoadingDocument)
{
  *aIsLoadingDocument = mIsLoadingDocument;

  return NS_OK;
}

PRInt64 nsDocLoader::GetMaxTotalProgress()
{
  PRInt64 newMaxTotal = 0;

  PRInt32 count = mChildList.Count();
  nsCOMPtr<nsIWebProgress> webProgress;
  for (PRInt32 i=0; i < count; i++) 
  {
    PRInt64 individualProgress = 0;
    nsIDocumentLoader* docloader = ChildAt(i);
    if (docloader)
    {
      // Cast is safe since all children are nsDocLoader too
      individualProgress = ((nsDocLoader *) docloader)->GetMaxTotalProgress();
    }
    if (individualProgress < PRInt64(0)) // if one of the elements doesn't know it's size
                                         // then none of them do
    {
       newMaxTotal = PRInt64(-1);
       break;
    }
    else
     newMaxTotal += individualProgress;
  }

  PRInt64 progress = -1;
  if (mMaxSelfProgress >= PRInt64(0) && newMaxTotal >= PRInt64(0))
    progress = newMaxTotal + mMaxSelfProgress;
  
  return progress;
}

////////////////////////////////////////////////////////////////////////////////////
// The following section contains support for nsIProgressEventSink which is used to 
// pass progress and status between the actual request and the doc loader. The doc loader
// then turns around and makes the right web progress calls based on this information.
////////////////////////////////////////////////////////////////////////////////////

NS_IMETHODIMP nsDocLoader::OnProgress(nsIRequest *aRequest, nsISupports* ctxt, 
                                      PRUint64 aProgress, PRUint64 aProgressMax)
{
  nsRequestInfo *info;
  PRInt64 progressDelta = 0;

  //
  // Update the RequestInfo entry with the new progress data
  //
  info = GetRequestInfo(aRequest);
  if (info) {
    // suppress sending STATE_TRANSFERRING if this is upload progress (see bug 240053)
    if (!info->mUploading && (PRInt64(0) == info->mCurrentProgress) && (PRInt64(0) == info->mMaxProgress)) {
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
      if (PRUint64(aProgressMax) != LL_MAXUINT) {
        mMaxSelfProgress  += PRInt64(aProgressMax);
        info->mMaxProgress = PRInt64(aProgressMax);
      } else {
        mMaxSelfProgress   =  PRInt64(-1);
        info->mMaxProgress =  PRInt64(-1);
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
    progressDelta = PRInt64(aProgress) - info->mCurrentProgress;
    mCurrentSelfProgress += progressDelta;

    info->mCurrentProgress = PRInt64(aProgress);
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

NS_IMETHODIMP nsDocLoader::OnStatus(nsIRequest* aRequest, nsISupports* ctxt, 
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
        mCurrentSelfProgress  = mMaxSelfProgress  = LL_ZERO;
        mCurrentTotalProgress = mMaxTotalProgress = LL_ZERO;
        mCompletedTotalProgress = LL_ZERO;
        info->mUploading = uploading;
        info->mCurrentProgress = LL_ZERO;
        info->mMaxProgress = LL_ZERO;
      }
    }

    nsCOMPtr<nsIStringBundleService> sbs =
      mozilla::services::GetStringBundleService();
    if (!sbs)
      return NS_ERROR_FAILURE;
    nsXPIDLString msg;
    nsresult rv = sbs->FormatStatusMessage(aStatus, aStatusArg,
                                           getter_Copies(msg));
    if (NS_FAILED(rv))
      return rv;

    // Keep around the message. In case a request finishes, we need to make sure
    // to send the status message of another request to our user to that we
    // don't display, for example, "Transferring" messages for requests that are
    // already done.
    if (info) {
      if (!info->mLastStatus) {
        info->mLastStatus = new nsStatusInfo(aRequest);
      } else {
        // We're going to move it to the front of the list, so remove
        // it from wherever it is now.
        PR_REMOVE_LINK(info->mLastStatus);
      }
      info->mLastStatus->mStatusMessage = msg;
      info->mLastStatus->mStatusCode = aStatus;
      // Put the info at the front of the list
      PR_INSERT_LINK(info->mLastStatus, &mStatusInfoList);
    }
    FireOnStatusChange(this, aRequest, aStatus, msg);
  }
  return NS_OK;
}

void nsDocLoader::ClearInternalProgress()
{
  ClearRequestInfoHash();

  mCurrentSelfProgress  = mMaxSelfProgress  = LL_ZERO;
  mCurrentTotalProgress = mMaxTotalProgress = LL_ZERO;
  mCompletedTotalProgress = LL_ZERO;

  mProgressStateFlags = nsIWebProgressListener::STATE_STOP;
}


void nsDocLoader::FireOnProgressChange(nsDocLoader *aLoadInitiator,
                                       nsIRequest *request,
                                       PRInt64 aProgress,
                                       PRInt64 aProgressMax,
                                       PRInt64 aProgressDelta,
                                       PRInt64 aTotalProgress,
                                       PRInt64 aMaxTotalProgress)
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

    info = static_cast<nsListenerInfo*>(mListenerInfoList.SafeElementAt(count));
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

    // XXX truncates 64-bit to 32-bit
    listener->OnProgressChange(aLoadInitiator,request,
                               PRInt32(aProgress), PRInt32(aProgressMax),
                               PRInt32(aTotalProgress), PRInt32(aMaxTotalProgress));
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


void nsDocLoader::FireOnStateChange(nsIWebProgress *aProgress,
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

  // Add the STATE_RESTORING bit if necessary.
  if (mIsRestoringDocument)
    aStateFlags |= nsIWebProgressListener::STATE_RESTORING;

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
  PRInt32 notifyMask = (aStateFlags >> 16) & nsIWebProgress::NOTIFY_STATE_ALL;

  while (--count >= 0) {
    nsListenerInfo *info;

    info = static_cast<nsListenerInfo*>(mListenerInfoList.SafeElementAt(count));
    if (!info || !(info->mNotifyMask & notifyMask)) {
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



void
nsDocLoader::FireOnLocationChange(nsIWebProgress* aWebProgress,
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

    info = static_cast<nsListenerInfo*>(mListenerInfoList.SafeElementAt(count));
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
}

void
nsDocLoader::FireOnStatusChange(nsIWebProgress* aWebProgress,
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

    info = static_cast<nsListenerInfo*>(mListenerInfoList.SafeElementAt(count));
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

PRBool
nsDocLoader::RefreshAttempted(nsIWebProgress* aWebProgress,
                              nsIURI *aURI,
                              PRInt32 aDelay,
                              PRBool aSameURI)
{
  /*
   * Returns true if the refresh may proceed,
   * false if the refresh should be blocked.
   *
   * First notify any listeners of the refresh attempt...
   *
   * Iterate the elements from back to front so that if items
   * get removed from the list it won't affect our iteration
   */
  PRBool allowRefresh = PR_TRUE;
  PRInt32 count = mListenerInfoList.Count();

  while (--count >= 0) {
    nsListenerInfo *info;

    info = static_cast<nsListenerInfo*>(mListenerInfoList.SafeElementAt(count));
    if (!info || !(info->mNotifyMask & nsIWebProgress::NOTIFY_REFRESH)) {
      continue;
    }

    nsCOMPtr<nsIWebProgressListener> listener =
      do_QueryReferent(info->mWeakListener);
    if (!listener) {
      // the listener went away. gracefully pull it out of the list.
      mListenerInfoList.RemoveElementAt(count);
      delete info;
      continue;
    }

    nsCOMPtr<nsIWebProgressListener2> listener2 =
      do_QueryReferent(info->mWeakListener);
    if (!listener2)
      continue;

    PRBool listenerAllowedRefresh;
    nsresult listenerRV = listener2->OnRefreshAttempted(
        aWebProgress, aURI, aDelay, aSameURI, &listenerAllowedRefresh);
    if (NS_FAILED(listenerRV))
      continue;

    allowRefresh = allowRefresh && listenerAllowedRefresh;
  }

  mListenerInfoList.Compact();

  // Pass the notification up to the parent...
  if (mParent) {
    allowRefresh = allowRefresh &&
      mParent->RefreshAttempted(aWebProgress, aURI, aDelay, aSameURI);
  }

  return allowRefresh;
}

nsListenerInfo * 
nsDocLoader::GetListenerInfo(nsIWebProgressListener *aListener)
{
  PRInt32 i, count;
  nsListenerInfo *info;

  nsCOMPtr<nsISupports> listener1 = do_QueryInterface(aListener);
  count = mListenerInfoList.Count();
  for (i=0; i<count; i++) {
    info = static_cast<nsListenerInfo*>(mListenerInfoList.SafeElementAt(i));

    NS_ASSERTION(info, "There should NEVER be a null listener in the list");
    if (info) {
      nsCOMPtr<nsISupports> listener2 = do_QueryReferent(info->mWeakListener);
      if (listener1 == listener2)
        return info;
    }
  }
  return nsnull;
}

nsresult nsDocLoader::AddRequestInfo(nsIRequest *aRequest)
{
  if (!PL_DHashTableOperate(&mRequestInfoHash, aRequest, PL_DHASH_ADD)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return NS_OK;
}

void nsDocLoader::RemoveRequestInfo(nsIRequest *aRequest)
{
  PL_DHashTableOperate(&mRequestInfoHash, aRequest, PL_DHASH_REMOVE);
}

nsRequestInfo * nsDocLoader::GetRequestInfo(nsIRequest *aRequest)
{
  nsRequestInfo *info =
    static_cast<nsRequestInfo *>
               (PL_DHashTableOperate(&mRequestInfoHash, aRequest,
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
static PLDHashOperator
RemoveInfoCallback(PLDHashTable *table, PLDHashEntryHdr *hdr, PRUint32 number,
                   void *arg)
{
  return PL_DHASH_REMOVE;
}

void nsDocLoader::ClearRequestInfoHash(void)
{
  if (!mRequestInfoHash.ops || !mRequestInfoHash.entryCount) {
    // No hash, or the hash is empty, nothing to do here then...

    return;
  }

  PL_DHashTableEnumerate(&mRequestInfoHash, RemoveInfoCallback, nsnull);
}

// PLDHashTable enumeration callback that calculates the max progress.
static PLDHashOperator
CalcMaxProgressCallback(PLDHashTable *table, PLDHashEntryHdr *hdr,
                        PRUint32 number, void *arg)
{
  const nsRequestInfo *info = static_cast<const nsRequestInfo *>(hdr);
  PRInt64 *max = static_cast<PRInt64 *>(arg);

  if (info->mMaxProgress < info->mCurrentProgress) {
    *max = PRInt64(-1);

    return PL_DHASH_STOP;
  }

  *max += info->mMaxProgress;

  return PL_DHASH_NEXT;
}

PRInt64 nsDocLoader::CalculateMaxProgress()
{
  PRInt64 max = mCompletedTotalProgress;
  PL_DHashTableEnumerate(&mRequestInfoHash, CalcMaxProgressCallback, &max);
  return max;
}

NS_IMETHODIMP nsDocLoader::AsyncOnChannelRedirect(nsIChannel *aOldChannel,
                                                  nsIChannel *aNewChannel,
                                                  PRUint32 aFlags,
                                                  nsIAsyncVerifyRedirectCallback *cb)
{
  if (aOldChannel)
  {
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

    OnRedirectStateChange(aOldChannel, aNewChannel, aFlags, stateFlags);
    FireOnStateChange(this, aOldChannel, stateFlags, NS_OK);
  }

  cb->OnRedirectVerifyCallback(NS_OK);
  return NS_OK;
}

/*
 * Implementation of nsISecurityEventSink method...
 */

NS_IMETHODIMP nsDocLoader::OnSecurityChange(nsISupports * aContext,
                                            PRUint32 aState)
{
  //
  // Fire progress notifications out to any registered nsIWebProgressListeners.  
  //
  
  nsCOMPtr<nsIRequest> request = do_QueryInterface(aContext);
  nsIWebProgress* webProgress = static_cast<nsIWebProgress*>(this);

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

    info = static_cast<nsListenerInfo*>(mListenerInfoList.SafeElementAt(count));
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

/*
 * Implementation of nsISupportsPriority methods...
 *
 * The priority of the DocLoader _is_ the priority of its LoadGroup.
 *
 * XXX(darin): Once we start storing loadgroups in loadgroups, this code will
 * go away. 
 */

NS_IMETHODIMP nsDocLoader::GetPriority(PRInt32 *aPriority)
{
  nsCOMPtr<nsISupportsPriority> p = do_QueryInterface(mLoadGroup);
  if (p)
    return p->GetPriority(aPriority);

  *aPriority = 0;
  return NS_OK;
}

NS_IMETHODIMP nsDocLoader::SetPriority(PRInt32 aPriority)
{
  PR_LOG(gDocLoaderLog, PR_LOG_DEBUG, 
         ("DocLoader:%p: SetPriority(%d) called\n", this, aPriority));

  nsCOMPtr<nsISupportsPriority> p = do_QueryInterface(mLoadGroup);
  if (p)
    p->SetPriority(aPriority);

  PRInt32 count = mChildList.Count();

  nsDocLoader *loader;
  for (PRInt32 i=0; i < count; i++) {
    loader = static_cast<nsDocLoader*>(ChildAt(i));
    if (loader) {
      loader->SetPriority(aPriority);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP nsDocLoader::AdjustPriority(PRInt32 aDelta)
{
  PR_LOG(gDocLoaderLog, PR_LOG_DEBUG, 
         ("DocLoader:%p: AdjustPriority(%d) called\n", this, aDelta));

  nsCOMPtr<nsISupportsPriority> p = do_QueryInterface(mLoadGroup);
  if (p)
    p->AdjustPriority(aDelta);

  PRInt32 count = mChildList.Count();

  nsDocLoader *loader;
  for (PRInt32 i=0; i < count; i++) {
    loader = static_cast<nsDocLoader*>(ChildAt(i));
    if (loader) {
      loader->AdjustPriority(aDelta);
    }
  }

  return NS_OK;
}




#if 0
void nsDocLoader::DumpChannelInfo()
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
