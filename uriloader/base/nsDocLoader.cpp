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
#include "nsIURL.h"
#include "nsCOMPtr.h"
#include "nsCom.h"

// XXX ick ick ick
#include "nsIContentViewerContainer.h"
#include "nsIDocument.h"
#include "nsIPresShell.h"
#include "nsIPresContext.h"

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


/* Define IIDs... */
static NS_DEFINE_IID(kISupportsIID,                NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIDocumentIID,                NS_IDOCUMENT_IID);
static NS_DEFINE_IID(kIDocumentLoaderIID,          NS_IDOCUMENTLOADER_IID);
static NS_DEFINE_IID(kIContentViewerContainerIID,  NS_ICONTENT_VIEWER_CONTAINER_IID);


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

    mProgressStatusFlags = 0;

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

///    rv = mLoadGroup->SetGroupListenerFactory(this);
///    if (NS_FAILED(rv)) return rv;

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
NS_IMPL_ADDREF(nsDocLoaderImpl);
NS_IMPL_RELEASE(nsDocLoaderImpl);

NS_INTERFACE_MAP_BEGIN(nsDocLoaderImpl)
   NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIStreamObserver)
   NS_INTERFACE_MAP_ENTRY(nsIStreamObserver)
   NS_INTERFACE_MAP_ENTRY(nsIDocumentLoader)
   NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
   NS_INTERFACE_MAP_ENTRY(nsIWebProgress)
NS_INTERFACE_MAP_END

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
    if (mParent) {
        mParent->RemoveChildGroup(this);
        mParent = nsnull;
    }

    mDocumentChannel = null_nsCOMPtr();

    // Clear factory pointer (which is the docloader)
    (void)mLoadGroup->SetGroupListenerFactory(nsnull);

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

        if (1 == count) {
            nsCOMPtr<nsIURI> uri;

            rv = aChannel->GetURI(getter_AddRefs(uri));
            if (NS_FAILED(rv)) return rv;

            // This channel is associated with the entire document...
            mDocumentChannel = aChannel;
            mLoadGroup->SetDefaultLoadChannel(mDocumentChannel); 
            FireOnStartDocumentLoad(this, uri);
        } 
        else {
          FireOnStartURLLoad(this, aChannel);
        }
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

    rv = mLoadGroup->GetActiveCount(&count);
    if (NS_FAILED(rv)) return rv;

    //
    // The load group for this DocumentLoader is idle...
    //
    if (0 == count) {
      DocLoaderIsEmpty(aStatus);
    } else {
      FireOnEndURLLoad(this, aChannel, aStatus);
    }
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
  if (mParent) { 
      mParent->DocLoaderIsEmpty(aStatus); 
      // 
      // New code to break the circular reference between 
      // the load group and the docloader... 
      // 
      mLoadGroup->SetDefaultLoadChannel(nsnull); 
  } 

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

      //
      // Do nothing after firing the OnEndDocumentLoad(...). The document
      // loader may be loading a *new* document - if LoadDocument()
      // was called from a handler!
      //
      FireOnEndDocumentLoad(this, docChannel, aStatus);

      if (mParent) {
        mParent->DocLoaderIsEmpty(aStatus);
      }
    }
  }
}


void nsDocLoaderImpl::FireOnStartDocumentLoad(nsDocLoaderImpl* aLoadInitiator,
                                              nsIURI* aURL)
{
  PRInt32 count;

#if defined(DEBUG)
  char *buffer;

  aURL->GetSpec(&buffer);
  if (aLoadInitiator == this) {
    PR_LOG(gDocLoaderLog, PR_LOG_DEBUG, 
           ("DocLoader:%p: ++ Firing OnStartDocumentLoad(...).\tURI: %s\n",
            this, buffer));
  } else {
    PR_LOG(gDocLoaderLog, PR_LOG_DEBUG, 
           ("DocLoader:%p: --   Propagating OnStartDocumentLoad(...)."
            "DocLoader:%p  URI:%s\n", 
            this, aLoadInitiator, buffer));
  }
  nsCRT::free(buffer);
#endif /* DEBUG */

  // if we have a web progress listener, propagate the fact that we are starting to load a url
  if (mProgressListener)
  {
    mProgressStatusFlags = nsIWebProgress::flag_net_start;

    if (aLoadInitiator == this)
      mProgressListener->OnStatusChange(mDocumentChannel, mProgressStatusFlags);
    else // the load must be initiated by a child...mscott: I'm passing the WRONG channel here! I need to add a get channel to the doc loader interface
      mProgressListener->OnChildStatusChange(mDocumentChannel, mProgressStatusFlags);
  }

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

    observer->OnStartDocumentLoad(aLoadInitiator, aURL, mCommand);
  }

  /*
   * Finally notify the parent...
   */
  if (mParent) {
    mParent->FireOnStartDocumentLoad(aLoadInitiator, aURL);
  }
}

void nsDocLoaderImpl::FireOnEndDocumentLoad(nsDocLoaderImpl* aLoadInitiator,
                                            nsIChannel *aDocChannel,
                                            nsresult aStatus)
									
{
#if defined(DEBUG)
  nsCOMPtr<nsIURI> uri;
  nsresult rv = NS_OK;
  if (aDocChannel)
    rv = aDocChannel->GetURI(getter_AddRefs(uri));
  if (NS_SUCCEEDED(rv)) {
    char* buffer = nsnull;
    if (uri)
      rv = uri->GetSpec(&buffer);
      if (NS_SUCCEEDED(rv) && buffer != nsnull) {
        if (aLoadInitiator == this) {
          PR_LOG(gDocLoaderLog, PR_LOG_DEBUG, 
                 ("DocLoader:%p: ++ Firing OnEndDocumentLoad(...)"
                  "\tURI: %s Status: %x\n", 
                  this, buffer, aStatus));
        } else {
          PR_LOG(gDocLoaderLog, PR_LOG_DEBUG, 
                 ("DocLoader:%p: --   Propagating OnEndDocumentLoad(...)."
                  "DocLoader:%p  URI:%s\n", 
                  this, aLoadInitiator, buffer));
        }
        nsCRT::free(buffer);
      }
    }
#endif /* DEBUG */

  // if we have a web progress listener, propagate the fact that we are starting to load a url
  if (mProgressListener)
  {
    mProgressStatusFlags = nsIWebProgress::flag_net_stop;

    if (aLoadInitiator == this)
      mProgressListener->OnStatusChange(mDocumentChannel, mProgressStatusFlags);
    else // the load must be initiated by a child...mscott: I'm passing the WRONG channel here! I need to add a get channel to the doc loader interface
      mProgressListener->OnChildStatusChange(mDocumentChannel, mProgressStatusFlags);
  }


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

  // now forget about our progress listener...
  mProgressListener = nsnull;
}


void nsDocLoaderImpl::FireOnStartURLLoad(nsDocLoaderImpl* aLoadInitiator,
                                         nsIChannel* aChannel)
{
#if defined(DEBUG)
  nsCOMPtr<nsIURI> uri;
  nsresult rv = NS_OK;
  if (aChannel)
    rv = aChannel->GetURI(getter_AddRefs(uri));
  if (NS_SUCCEEDED(rv)) {
    char* buffer = nsnull;
    if (uri)
      rv = uri->GetSpec(&buffer);
    if (NS_SUCCEEDED(rv) && buffer != nsnull) {
      if (aLoadInitiator == this) {
        PR_LOG(gDocLoaderLog, PR_LOG_DEBUG, 
               ("DocLoader:%p: ++ Firing OnStartURLLoad(...)"
                "\tURI: %s\n", 
                this, buffer));
      } else {
        PR_LOG(gDocLoaderLog, PR_LOG_DEBUG, 
               ("DocLoader:%p: --   Propagating OnStartURLLoad(...)."
                "DocLoader:%p  URI:%s\n", 
                this, aLoadInitiator, buffer));
      }
      nsCRT::free(buffer);
    }
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
  nsCOMPtr<nsIURI> uri;
  nsresult rv = NS_OK;
  if (aChannel)
    rv = aChannel->GetURI(getter_AddRefs(uri));
  if (NS_SUCCEEDED(rv)) {
    char* buffer = nsnull;
    if (uri)
      rv = uri->GetSpec(&buffer);
    if (NS_SUCCEEDED(rv) && buffer != nsnull) {
      if (aLoadInitiator == this) {
        PR_LOG(gDocLoaderLog, PR_LOG_DEBUG, 
               ("DocLoader:%p: ++ Firing OnEndURLLoad(...)"
                "\tURI: %s Status: %x\n", 
                this, buffer, aStatus));
      } else {
        PR_LOG(gDocLoaderLog, PR_LOG_DEBUG, 
               ("DocLoader:%p: --   Propagating OnEndURLLoad(...)."
                "DocLoader:%p  URI:%s\n", 
                this, aLoadInitiator, buffer));
      }
      nsCRT::free(buffer);
    }
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

NS_IMETHODIMP nsDocLoaderImpl::AddProgressListener(nsIWebProgressListener *listener)
{
  // the doc loader only needs to worry about the progress listener for the docshell.
  // it in turn actually manages the list of web progress listeners...
  mProgressListener = listener;
  return NS_OK;
}

NS_IMETHODIMP nsDocLoaderImpl::RemoveProgressListener(nsIWebProgressListener *listener)
{
  mProgressListener = nsnull; 
  return NS_OK;
}

NS_IMETHODIMP nsDocLoaderImpl::GetProgressStatusFlags(PRInt32 *aProgressStatusFlags)
{
  *aProgressStatusFlags = mProgressStatusFlags;
  return NS_OK;
}

NS_IMETHODIMP nsDocLoaderImpl::GetCurSelfProgress(PRInt32 *aCurSelfProgress)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP nsDocLoaderImpl::GetMaxSelfProgress(PRInt32 *aMaxSelfProgress)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP nsDocLoaderImpl::GetCurTotalProgress(PRInt32 *aCurTotalProgress)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsDocLoaderImpl::GetMaxTotalProgress(PRInt32 *aMaxTotalProgress)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}
