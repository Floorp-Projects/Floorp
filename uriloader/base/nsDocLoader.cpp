/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include "nspr.h"
#include "prlog.h"

#include "nsIDocumentLoaderObserver.h"
#include "nsDocLoader.h"
#include "nsCURILoader.h"
#include "nsNetUtil.h"

#include "nsIServiceManager.h"
#include "nsXPIDLString.h"

#include "nsIURL.h"
#include "nsCOMPtr.h"
#include "nsCom.h"

// XXX ick ick ick
#include "nsIContentViewerContainer.h"
#include "nsIDocument.h"
#include "nsIPresShell.h"
#include "nsIPresContext.h"
#include "nsIStringBundle.h"

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
void GetURIStringFromChannel(nsIRequest *aRequest, nsXPIDLCString &aStr)
{
  nsCOMPtr<nsIChannel> channel;
  nsCOMPtr<nsIURI> uri;
  nsresult rv = NS_OK;

  channel = do_QueryInterface(aRequest, &rv);
  if (NS_SUCCEEDED(rv))
    rv = channel->GetURI(getter_AddRefs(uri));

  if (NS_SUCCEEDED(rv) && uri)
    rv = uri->GetSpec(getter_Copies(aStr));
  else 
    aStr = "???";
}
#endif /* DEBUG */

/* Define IIDs... */
static NS_DEFINE_IID(kIDocumentIID,                NS_IDOCUMENT_IID);
static NS_DEFINE_IID(kIContentViewerContainerIID,  NS_ICONTENTVIEWERCONTAINER_IID);


struct nsChannelInfo {
  nsChannelInfo(nsIChannel *key) : mKey(key),
                                   mCurrentProgress(0),
                                   mMaxProgress(0)
  {
    key->GetURI(getter_AddRefs(mURI));
  }

  void* mKey;
  nsCOMPtr<nsIURI> mURI;
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
    
    return rv;
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
   NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIStreamObserver)
   NS_INTERFACE_MAP_ENTRY(nsIStreamObserver)
   NS_INTERFACE_MAP_ENTRY(nsIDocumentLoader)
   NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
   NS_INTERFACE_MAP_ENTRY(nsIWebProgress)
   NS_INTERFACE_MAP_ENTRY(nsIProgressEventSink)   
   NS_INTERFACE_MAP_ENTRY(nsIInterfaceRequestor)
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


/*
 * Do not hold refs to the objects in the observer lists.  Observers
 * are expected to remove themselves upon their destruction if they
 * have not removed themselves previously
 */
NS_IMETHODIMP
nsDocLoaderImpl::AddObserver(nsIDocumentLoaderObserver* aObserver)
{
  nsresult rv;

  if (mDocObservers.IndexOf(aObserver) == -1) {
    //
    // XXX this method incorrectly returns a bool    
    //
    rv = mDocObservers.AppendElement(aObserver) ? NS_OK : NS_ERROR_FAILURE;
  } else {
    // The observer is already in the list...
    rv = NS_ERROR_FAILURE;
  }
  return rv;
}

NS_IMETHODIMP
nsDocLoaderImpl::RemoveObserver(nsIDocumentLoaderObserver* aObserver)
{
  if (PR_TRUE == mDocObservers.RemoveElement(aObserver)) {
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
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
    nsIPresShell* pres;
    pres = doc->GetShellAt(0);
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
      NS_RELEASE(pres);
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

  ClearChannelInfoList();

  mDocumentChannel = null_nsCOMPtr();

  mLoadGroup->SetGroupObserver(nsnull);

  return NS_OK;
}

NS_IMETHODIMP
nsDocLoaderImpl::OnStartRequest(nsIChannel *aChannel, nsISupports *aCtxt)
{
    // called each time a channel is added to the group.
    nsresult rv;

    if (!mIsLoadingDocument) {
        PRUint32 loadAttribs = 0;

        aChannel->GetLoadAttributes(&loadAttribs);
        if (loadAttribs & nsIChannel::LOAD_DOCUMENT_URI) {
            mIsLoadingDocument = PR_TRUE;
            ClearInternalProgress(); // only clear our progress if we are starting a new load....
        }
    }

    //
    // Only fire an OnStartDocumentLoad(...) if the document loader
    // has initiated a load...  Otherwise, this notification has
    // resulted from a channel being added to the load group.
    //
    if (mIsLoadingDocument) {
        PRUint32 count;

        rv = mLoadGroup->GetActiveCount(&count);
        if (NS_FAILED(rv)) return rv;
        //
        // Create a new ChannelInfo for the channel that is starting to
        // load...
        //
        AddChannelInfo(aChannel);

        if (1 == count) {
            // This channel is associated with the entire document...
            mDocumentChannel = aChannel;
            mLoadGroup->SetDefaultLoadChannel(mDocumentChannel); 
        
            // Update the progress status state
            mProgressStateFlags = nsIWebProgressListener::flag_start;

            doStartDocumentLoad();
            FireOnStartDocumentLoad(this, aChannel);
        } 
        else {
          doStartURLLoad(aChannel);
          FireOnStartURLLoad(this, aChannel);
        }
    }
    else {
      ClearChannelInfoList();
      doStartURLLoad(aChannel);
      FireOnStartURLLoad(this, aChannel);
    }
    
  return NS_OK;
}

NS_IMETHODIMP
nsDocLoaderImpl::OnStopRequest(nsIChannel *aChannel, 
                               nsISupports *aCtxt, 
                               nsresult aStatus, 
                               const PRUnichar *aMsg)
{
  nsresult rv = NS_OK;

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
    nsChannelInfo *info;
    info = GetChannelInfo(aChannel);
    if (info) {
      PRInt32 oldMax = info->mMaxProgress;

      info->mMaxProgress = info->mCurrentProgress;
      //
      // If a channel whose content-length was previously unknown has just
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
    doStopURLLoad(aChannel, aStatus);
    FireOnEndURLLoad(this, aChannel, aStatus);
    
    rv = mLoadGroup->GetActiveCount(&count);
    if (NS_FAILED(rv)) return rv;

    //
    // The load group for this DocumentLoader is idle...
    //
    if (0 == count) {
      DocLoaderIsEmpty(aStatus);
    }
  }
  else {
    doStopURLLoad(aChannel, aStatus); 
    FireOnEndURLLoad(this, aChannel, aStatus);
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


void nsDocLoaderImpl::DocLoaderIsEmpty(nsresult aStatus)
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

      nsCOMPtr<nsIChannel> docChannel(mDocumentChannel);

      mDocumentChannel = null_nsCOMPtr();
      mIsLoadingDocument = PR_FALSE;

      // Update the progress status state - the document is done
      mProgressStateFlags = nsIWebProgressListener::flag_stop;

      // 
      // New code to break the circular reference between 
      // the load group and the docloader... 
      // 
      mLoadGroup->SetDefaultLoadChannel(nsnull); 

      //
      // Do nothing after firing the OnEndDocumentLoad(...). The document
      // loader may be loading a *new* document - if LoadDocument()
      // was called from a handler!
      //
      doStopDocumentLoad(docChannel, aStatus);
      FireOnEndDocumentLoad(this, docChannel, aStatus);

      if (mParent) {
        mParent->DocLoaderIsEmpty(aStatus);
      }
    }
  }
}

void nsDocLoaderImpl::doStartDocumentLoad(void)
{

#if defined(DEBUG)
  nsXPIDLCString buffer;

  GetURIStringFromChannel(mDocumentChannel, buffer);
  PR_LOG(gDocLoaderLog, PR_LOG_DEBUG, 
         ("DocLoader:%p: ++ Firing OnStateChange for start document load (...)."
          "\tURI: %s \n",
          this, (const char *) buffer));
#endif /* DEBUG */

  // Fire an OnStatus(...) notification flag_net_start.  This indicates
  // that the document represented by mDocumentChannel has started to
  // load...
  FireOnStateChange(this,
                    mDocumentChannel,
                    nsIWebProgressListener::flag_start |
                    nsIWebProgressListener::flag_is_document |
                    nsIWebProgressListener::flag_is_request |
                    nsIWebProgressListener::flag_is_network,
                    NS_OK);
}

void nsDocLoaderImpl::doStartURLLoad(nsIChannel *aChannel)
{
#if defined(DEBUG)
  nsXPIDLCString buffer;

  GetURIStringFromChannel(aChannel, buffer);
    PR_LOG(gDocLoaderLog, PR_LOG_DEBUG, 
          ("DocLoader:%p: ++ Firing OnStateChange start url load (...)."
           "\tURI: %s\n",
            this, (const char *) buffer));
#endif /* DEBUG */

  FireOnStateChange(this,
                    aChannel,
                    nsIWebProgressListener::flag_start |
                    nsIWebProgressListener::flag_is_request,
                    NS_OK);
}

void nsDocLoaderImpl::doStopURLLoad(nsIChannel *aChannel, nsresult aStatus)
{
#if defined(DEBUG)
  nsXPIDLCString buffer;

  GetURIStringFromChannel(aChannel, buffer);
    PR_LOG(gDocLoaderLog, PR_LOG_DEBUG, 
          ("DocLoader:%p: ++ Firing OnStateChange for end url load (...)."
           "\tURI: %s status=%x\n",
            this, (const char *) buffer, aStatus));
#endif /* DEBUG */

  FireOnStateChange(this,
                    aChannel,
                    nsIWebProgressListener::flag_stop |
                    nsIWebProgressListener::flag_is_request,
                    aStatus);
}

void nsDocLoaderImpl::doStopDocumentLoad(nsIChannel* aChannel,
                                         nsresult aStatus)
{
#if defined(DEBUG)
  nsXPIDLCString buffer;

  GetURIStringFromChannel(aChannel, buffer);
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
                    aChannel,
                    nsIWebProgressListener::flag_stop |
                    nsIWebProgressListener::flag_is_document |
                    nsIWebProgressListener::flag_is_network,
                    aStatus);
}




void nsDocLoaderImpl::FireOnStartDocumentLoad(nsDocLoaderImpl* aLoadInitiator,
                                              nsIChannel *aDocChannel)
{
  PRInt32 count;

  nsCOMPtr<nsIURI> uri;

  aDocChannel->GetURI(getter_AddRefs(uri));

#if defined(DEBUG)
  nsXPIDLCString buffer;

  GetURIStringFromChannel(aDocChannel, buffer);
  if (aLoadInitiator == this) {
    PR_LOG(gDocLoaderLog, PR_LOG_DEBUG, 
           ("DocLoader:%p: ++ Firing OnStartDocumentLoad(...).\tURI: %s\n",
            this, (const char *) buffer));
  } else {
    PR_LOG(gDocLoaderLog, PR_LOG_DEBUG, 
           ("DocLoader:%p: --   Propagating OnStartDocumentLoad(...)."
            "DocLoader:%p  URI:%s\n", 
            this, aLoadInitiator, (const char *) buffer));
  }
#endif /* DEBUG */

  /*
   * First notify any observers that the document load has begun...
   *
   * Operate the elements from back to front so that if items get
   * get removed from the list it won't affect our iteration
   */
  count = mDocObservers.Count();
  while (count > 0) {
    nsIDocumentLoaderObserver *observer;

    observer = NS_STATIC_CAST(nsIDocumentLoaderObserver*, mDocObservers.ElementAt(--count));

    NS_ASSERTION(observer, "NULL observer found in list.");
    if (! observer) {
      continue;
    }

    observer->OnStartDocumentLoad(aLoadInitiator, uri, mCommand);
  }

  /*
   * Finally notify the parent...
   */
  if (mParent) {
    mParent->FireOnStartDocumentLoad(aLoadInitiator, aDocChannel);
  }
}

void nsDocLoaderImpl::FireOnEndDocumentLoad(nsDocLoaderImpl* aLoadInitiator,
                                            nsIChannel *aDocChannel,
                                            nsresult aStatus)
									
{
#if defined(DEBUG)
  nsXPIDLCString buffer;

  GetURIStringFromChannel(aDocChannel, buffer);
  if (aLoadInitiator == this) {
    PR_LOG(gDocLoaderLog, PR_LOG_DEBUG, 
           ("DocLoader:%p: ++ Firing OnEndDocumentLoad(...)"
            "\tURI: %s Status: %x\n", 
            this, (const char *) buffer, aStatus));
  } else {
    PR_LOG(gDocLoaderLog, PR_LOG_DEBUG, 
           ("DocLoader:%p: --   Propagating OnEndDocumentLoad(...)."
            "DocLoader:%p  URI:%s\n", 
            this, aLoadInitiator, (const char *)buffer));
  }
#endif /* DEBUG */

  /*
   * First notify any observers that the document load has finished...
   *
   * Operate the elements from back to front so that if items get
   * get removed from the list it won't affect our iteration
   */
  PRInt32 count;

  count = mDocObservers.Count();
  while (count > 0) {
    nsIDocumentLoaderObserver *observer;

    observer = NS_STATIC_CAST(nsIDocumentLoaderObserver*, mDocObservers.ElementAt(--count));

    NS_ASSERTION(observer, "NULL observer found in list.");
    if (! observer) {
      continue;
    }

    observer->OnEndDocumentLoad(aLoadInitiator, aDocChannel, aStatus);
  }

  /*
   * Next notify the parent...
   */
  if (mParent) {
    mParent->FireOnEndDocumentLoad(aLoadInitiator, aDocChannel, aStatus);
  }
}


void nsDocLoaderImpl::FireOnStartURLLoad(nsDocLoaderImpl* aLoadInitiator,
                                         nsIChannel* aChannel)
{
#if defined(DEBUG)
  nsXPIDLCString buffer;

  GetURIStringFromChannel(aChannel, buffer);
  if (aLoadInitiator == this) {
    PR_LOG(gDocLoaderLog, PR_LOG_DEBUG, 
           ("DocLoader:%p: ++ Firing OnStartURLLoad(...)"
            "\tURI: %s\n", 
            this, (const char *) buffer));
  } else {
    PR_LOG(gDocLoaderLog, PR_LOG_DEBUG, 
           ("DocLoader:%p: --   Propagating OnStartURLLoad(...)."
            "DocLoader:%p  URI:%s\n", 
            this, aLoadInitiator, (const char *) buffer));
  }
#endif /* DEBUG */


  PRInt32 count;

  /*
   * First notify any observers that the URL load has begun...
   *
   * Operate the elements from back to front so that if items get
   * get removed from the list it won't affect our iteration
   */
  count = mDocObservers.Count();
  while (count > 0) {
    nsIDocumentLoaderObserver *observer;

    observer = NS_STATIC_CAST(nsIDocumentLoaderObserver*, mDocObservers.ElementAt(--count));

    NS_ASSERTION(observer, "NULL observer found in list.");
    if (! observer) {
      continue;
    }

    observer->OnStartURLLoad(aLoadInitiator, aChannel);
  }

  /*
   * Finally notify the parent...
   */
  if (mParent) {
    mParent->FireOnStartURLLoad(aLoadInitiator, aChannel);
  }
}


void nsDocLoaderImpl::FireOnEndURLLoad(nsDocLoaderImpl* aLoadInitiator,
                                       nsIChannel* aChannel, nsresult aStatus)
{
#if defined(DEBUG)
  nsXPIDLCString buffer;

  GetURIStringFromChannel(aChannel, buffer);
  if (aLoadInitiator == this) {
    PR_LOG(gDocLoaderLog, PR_LOG_DEBUG, 
           ("DocLoader:%p: ++ Firing OnEndURLLoad(...)"
            "\tURI: %s Status: %x\n", 
            this, (const char *) buffer, aStatus));
  } else {
    PR_LOG(gDocLoaderLog, PR_LOG_DEBUG, 
           ("DocLoader:%p: --   Propagating OnEndURLLoad(...)."
            "DocLoader:%p  URI:%s\n", 
            this, aLoadInitiator, (const char *) buffer));
  }
#endif /* DEBUG */

  PRInt32 count;

  /*
   * First notify any observers that the URL load has completed...
   *
   * Operate the elements from back to front so that if items get
   * get removed from the list it won't affect our iteration
   */
  count = mDocObservers.Count();
  while (count > 0) {
    nsIDocumentLoaderObserver *observer;

    observer = NS_STATIC_CAST(nsIDocumentLoaderObserver*, mDocObservers.ElementAt(--count));

    NS_ASSERTION(observer, "NULL observer found in list.");
    if (! observer) {
      continue;
    }

    observer->OnEndURLLoad(aLoadInitiator, aChannel, aStatus);
  }

  /*
   * Finally notify the parent...
   */
  if (mParent) {
    mParent->FireOnEndURLLoad(aLoadInitiator, aChannel, aStatus);
  }
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

  if (mListenerList.IndexOf(aListener) == -1) {

    // XXX this method incorrectly returns a bool    
    rv = mListenerList.AppendElement(aListener) ? NS_OK : NS_ERROR_FAILURE;
  } else {
    // The listener is already in the list...
    rv = NS_ERROR_FAILURE;
  }
  return rv;
}

NS_IMETHODIMP
nsDocLoaderImpl::RemoveProgressListener(nsIWebProgressListener *aListener)
{
  nsresult rv;

  // XXX this method incorrectly returns a bool    
  rv = mListenerList.RemoveElement(aListener) ? NS_OK : NS_ERROR_FAILURE;

  return rv;
}

NS_IMETHODIMP nsDocLoaderImpl::GetProgressStatusFlags(PRInt32 *aProgressStateFlags)
{
  *aProgressStateFlags = mProgressStateFlags;
  return NS_OK;
}

NS_IMETHODIMP nsDocLoaderImpl::GetCurSelfProgress(PRInt32 *aCurSelfProgress)
{
  *aCurSelfProgress = mCurrentSelfProgress;
  return NS_OK;
}


NS_IMETHODIMP nsDocLoaderImpl::GetMaxSelfProgress(PRInt32 *aMaxSelfProgress)
{
  *aMaxSelfProgress = mMaxSelfProgress;
  return NS_OK;
}


NS_IMETHODIMP nsDocLoaderImpl::GetCurTotalProgress(PRInt32 *aCurTotalProgress)
{
  *aCurTotalProgress = mCurrentTotalProgress;
  return NS_OK;
}

NS_IMETHODIMP nsDocLoaderImpl::GetMaxTotalProgress(PRInt32 *aMaxTotalProgress)
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
      webProgress = do_QueryInterface(docloader);
      webProgress->GetMaxTotalProgress(&invididualProgress);
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
// pass progress and status between the actual channel and the doc loader. The doc loader
// then turns around and makes the right web progress calls based on this information.
////////////////////////////////////////////////////////////////////////////////////

NS_IMETHODIMP nsDocLoaderImpl::OnProgress(nsIChannel* aChannel, nsISupports* ctxt, 
                                          PRUint32 aProgress, PRUint32 aProgressMax)
{
  nsChannelInfo *info;
  PRInt32 progressDelta = 0;

  //
  // Update the ChannelInfo entry with the new progress data
  //
  info = GetChannelInfo(aChannel);
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

      // Send a flag_transferring notification for the request.
      PRInt32 flags;
    
      flags = nsIWebProgressListener::flag_transferring | 
              nsIWebProgressListener::flag_is_request;
      //
      // Move the WebProgress into the flag_transferring state if necessary...
      //
      if (mProgressStateFlags & nsIWebProgressListener::flag_start) {
        mProgressStateFlags = nsIWebProgressListener::flag_transferring;

        // Send flag_transferring for the document too...
        flags |= nsIWebProgressListener::flag_is_document;
      }

      FireOnStateChange(this, aChannel, flags, NS_OK);
    }

    // Update the current progress count...
    progressDelta = aProgress - info->mCurrentProgress;
    mCurrentSelfProgress += progressDelta;

    info->mCurrentProgress = aProgress;
  } 
  //
  // The channel is not part of the load group, so ignore its progress
  // information...
  //
  else {
#if defined(DEBUG)
    nsXPIDLCString buffer;

    GetURIStringFromChannel(aChannel, buffer);
    PR_LOG(gDocLoaderLog, PR_LOG_DEBUG, 
           ("DocLoader:%p OOPS - No Channel Info for: %s\n",
            this, (const char *)buffer));
#endif /* DEBUG */

    return NS_OK;
  }

  //
  // Fire progress notifications out to any registered nsIWebProgressListeners
  //
  FireOnProgressChange(this, aChannel, aProgress, aProgressMax, progressDelta,
                       mCurrentTotalProgress, mMaxTotalProgress);

  return NS_OK;
}

NS_IMETHODIMP nsDocLoaderImpl::OnStatus(nsIChannel* aChannel, nsISupports* ctxt, 
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
    FireOnStatusChange(this, aChannel, aStatus, msg);
  }
  return NS_OK;
}

void nsDocLoaderImpl::ClearInternalProgress()
{
  ClearChannelInfoList();

  mCurrentSelfProgress  = mMaxSelfProgress  = 0;
  mCurrentTotalProgress = mMaxTotalProgress = 0;

  mProgressStateFlags = nsIWebProgressListener::flag_stop;
}


void nsDocLoaderImpl::FireOnProgressChange(nsDocLoaderImpl *aLoadInitiator,
                                           nsIChannel *aChannel,
                                           PRInt32 aProgress,
                                           PRInt32 aProgressMax,
                                           PRInt32 aProgressDelta,
                                           PRInt32 aTotalProgress,
                                           PRInt32 aMaxTotalProgress)
{
  PRInt32 count;

  if (mIsLoadingDocument) {
    mCurrentTotalProgress += aProgressDelta;
    GetMaxTotalProgress(&mMaxTotalProgress);

    aTotalProgress    = mCurrentTotalProgress;
    aMaxTotalProgress = mMaxTotalProgress;
  }

#if defined(DEBUG)
  nsXPIDLCString buffer;

  GetURIStringFromChannel(aChannel, buffer);
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
  count = mListenerList.Count();
  while (count > 0) {
    nsIWebProgressListener *listener;

    listener = NS_STATIC_CAST(nsIWebProgressListener*,
                              mListenerList.ElementAt(--count));

    NS_ASSERTION(listener, "NULL listener found in list.");
    if (! listener) {
      continue;
    }

    listener->OnProgressChange(aLoadInitiator,aChannel,
                               aProgress, aProgressMax,
                               aTotalProgress, aMaxTotalProgress);
  }

  // Pass the notification up to the parent...
  if (mParent) {
    mParent->FireOnProgressChange(aLoadInitiator, aChannel,
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
  PRInt32 count;

  //
  // Remove the flag_is_network bit if necessary.
  //
  // The rule is to remove this bit, if the notification has been passed
  // up from a child WebProgress, and the current WebProgress is already
  // active...
  //
  if (mIsLoadingDocument &&
      (aStateFlags & nsIWebProgressListener::flag_is_network) && 
      (this != aProgress)) {
    aStateFlags &= ~nsIWebProgressListener::flag_is_network;
  }

#if defined(DEBUG)
  nsXPIDLCString buffer;

  GetURIStringFromChannel(aRequest, buffer);
  PR_LOG(gDocLoaderLog, PR_LOG_DEBUG, 
         ("DocLoader:%p: Status (%s): code: %x\n",
         this, (const char *)buffer, aStateFlags));
#endif /* DEBUG */


  /*                                                                           
   * First notify any listeners of the new state info...
   *
   * Operate the elements from back to front so that if items get
   * get removed from the list it won't affect our iteration
   */
  count = mListenerList.Count();
  while (count > 0) {
    nsIWebProgressListener *listener;

    listener = NS_STATIC_CAST(nsIWebProgressListener*,
                              mListenerList.ElementAt(--count));

    NS_ASSERTION(listener, "NULL listener found in list.");
    if (! listener) {
      continue;
    }

    listener->OnStateChange(aProgress, aRequest, aStateFlags, aStatus);
  }

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
  PRInt32 count;

  count = mListenerList.Count();
  while (count > 0) {
    nsIWebProgressListener *listener;

    listener = NS_STATIC_CAST(nsIWebProgressListener*,
                              mListenerList.ElementAt(--count));

    NS_ASSERTION(listener, "NULL listener found in list.");
    if (! listener) {
      continue;
    }

    listener->OnLocationChange(aWebProgress, aRequest, aUri);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDocLoaderImpl::FireOnStatusChange(nsIWebProgress* aWebProgress,
                                    nsIRequest* aRequest,
                                    nsresult aStatus,
                                    const PRUnichar* aMessage)
{
  PRInt32 count;

  count = mListenerList.Count();
  while (count > 0) {
    nsIWebProgressListener *listener;

    listener = NS_STATIC_CAST(nsIWebProgressListener*,
                              mListenerList.ElementAt(--count));

    NS_ASSERTION(listener, "NULL listener found in list.");
    if (! listener) {
      continue;
    }

    listener->OnStatusChange(aWebProgress, aRequest, aStatus, aMessage);
  }

  return NS_OK;
}

nsresult nsDocLoaderImpl::AddChannelInfo(nsIChannel *aChannel)
{
  nsresult rv;
  PRUint32 loadAttribs=nsIChannel::LOAD_NORMAL;

  //
  // Only create a ChannelInfo entry if the channel is *not* being loaded
  // in the background...
  //
  rv = aChannel->GetLoadAttributes(&loadAttribs);
  if (!(nsIChannel::LOAD_BACKGROUND & loadAttribs)) {
    nsChannelInfo *info;

    info = new nsChannelInfo(aChannel);
    if (info) {
      // XXX this method incorrectly returns a bool    
      rv = mChannelInfoList.AppendElement(info) ? NS_OK : NS_ERROR_FAILURE;
    } else {
      rv = NS_ERROR_OUT_OF_MEMORY;
    }
  }
  else {
    rv = NS_OK;
  }
  return rv;
}

nsChannelInfo * nsDocLoaderImpl::GetChannelInfo(nsIChannel *aChannel)
{
  nsChannelInfo *info;
  PRInt32 i, count;

  count = mChannelInfoList.Count();
  for(i=0; i<count; i++) {
    info = (nsChannelInfo *)mChannelInfoList.ElementAt(i);
    if (aChannel == info->mKey) {
      return info;
    }
  }

  return nsnull;
}

nsresult nsDocLoaderImpl::ClearChannelInfoList(void)
{
  nsChannelInfo *info;
  PRInt32 i, count;

  count = mChannelInfoList.Count();
  for(i=0; i<count; i++) {
    info = (nsChannelInfo *)mChannelInfoList.ElementAt(i);
    delete info;
  }

  mChannelInfoList.Clear();
  mChannelInfoList.Compact();

  return NS_OK;
}

void nsDocLoaderImpl::CalculateMaxProgress(PRInt32 *aMax)
{
  PRInt32 i, count;
  PRInt32 current=0, max=0;

  count = mChannelInfoList.Count();
  for(i=0; i<count; i++) {
    nsChannelInfo *info;
  
    info = (nsChannelInfo *)mChannelInfoList.ElementAt(i);

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

