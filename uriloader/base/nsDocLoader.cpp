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
#include "nsIDocumentLoader.h"
#include "nsIWebShell.h"
#include "prmem.h"
#include "plstr.h"
#include "nsString.h"
#include "nsISupportsArray.h"
#include "nsIURL.h"
#include "nsIStreamListener.h"
#include "nsIFactory.h"
#include "nsIContentViewerContainer.h"
#include "nsIContentViewer.h"
#include "nsITimer.h"
#include "nsIDocumentLoaderObserver.h"
#include "nsVoidArray.h"
#include "nsIServiceManager.h"
#include "nsXPIDLString.h"

#include "nsILoadGroup.h"

#include "nsNetUtil.h"
#include "nsIURL.h"
#include "nsIChannel.h"
#include "nsIHTTPChannel.h"
#include "nsHTTPEnums.h"
#include "nsIDNSService.h"
#include "nsIProgressEventSink.h"

#include "nsIGenericFactory.h"
#include "nsCOMPtr.h"
#include "nsCom.h"
#include "prlog.h"
#include "prprf.h"

#include "nsWeakReference.h"

#include "nsIStreamConverterService.h"
#include "nsIStreamConverter.h"
static NS_DEFINE_CID(kStreamConverterServiceCID, NS_STREAMCONVERTERSERVICE_CID);

#include <iostream.h>

#include "nsIPref.h"
#include "nsIURIContentListener.h"
#include "nsIURILoader.h"
#include "nsCURILoader.h"

// XXX ick ick ick
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
static NS_DEFINE_IID(kIStreamObserverIID,          NS_ISTREAMOBSERVER_IID);
static NS_DEFINE_IID(kIDocumentLoaderIID,          NS_IDOCUMENTLOADER_IID);
static NS_DEFINE_IID(kIDocumentLoaderFactoryIID,   NS_IDOCUMENTLOADERFACTORY_IID);
static NS_DEFINE_IID(kISupportsIID,                NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIDocumentIID,                NS_IDOCUMENT_IID);
static NS_DEFINE_IID(kIStreamListenerIID,          NS_ISTREAMLISTENER_IID);

static NS_DEFINE_IID(kIContentViewerContainerIID,  NS_ICONTENT_VIEWER_CONTAINER_IID);
static NS_DEFINE_CID(kGenericFactoryCID,           NS_GENERICFACTORY_CID);

/* Forward declarations.... */
class nsDocLoaderImpl;


/* 
 * The nsDocumentBindInfo contains the state required when a single document
 * is being loaded...  Each instance remains alive until its target URL has 
 * been loaded (or aborted).
 *
 * The Document Loader maintains a list of nsDocumentBindInfo instances which 
 * represents the set of documents actively being loaded...
 */
class nsDocumentBindInfo : public nsIStreamListener
{
public:
    nsDocumentBindInfo();

    nsresult Init(nsDocLoaderImpl* aDocLoader,
                  const char *aCommand, 
                  nsISupports* aContainer,
                  nsISupports* aExtraInfo);

    NS_DECL_ISUPPORTS

    nsresult Bind(nsIURI *aURL, 
                  nsILoadGroup* aLoadGroup, 
                  nsIInputStream *postDataStream,
                  const PRUnichar* aReferrer=nsnull);

    // nsIStreamObserver methods:
    NS_DECL_NSISTREAMOBSERVER

	// nsIStreamListener methods:
    NS_DECL_NSISTREAMLISTENER

protected:
    virtual ~nsDocumentBindInfo();

protected:
    char*               m_Command;
    nsCOMPtr<nsISupports>  m_Container;
    nsCOMPtr<nsISupports>  m_ExtraInfo;
    nsCOMPtr<nsIStreamListener> m_NextStream;
    nsDocLoaderImpl*    m_DocLoader;
};




/****************************************************************************
 * nsDocLoaderImpl implementation...
 ****************************************************************************/

class nsDocLoaderImpl : public nsIDocumentLoader, 
                        public nsIStreamObserver,
                        public nsSupportsWeakReference
{
public:

    nsDocLoaderImpl();

    nsresult Init(nsDocLoaderImpl* aParent);

    // for nsIGenericFactory:
    static NS_IMETHODIMP Create(nsISupports *aOuter, const nsIID &aIID, void **aResult);

    NS_DECL_ISUPPORTS

    NS_IMETHOD LoadOpenedDocument(nsIChannel * aOpenedChannel, 
                                  const char * aCommand,
                                  nsIContentViewerContainer *aContainer,
                                  nsIInputStream * aPostDataStream,
                                  nsIURI * aReffererUrl,
                                  nsIStreamListener ** aContentHandler);

    // nsIDocumentLoader interface
    NS_IMETHOD LoadDocument(nsIURI * aUri, 
                            const char *aCommand,
                            nsISupports* aContainer,
                            nsIInputStream* aPostDataStream = nsnull,
                            nsISupports* aExtraInfo = nsnull,
                            nsLoadFlags aType = nsIChannel::LOAD_NORMAL,
                            const PRUint32 aLocalIP = 0,
                            const PRUnichar* aReferrer = nsnull);

    NS_IMETHOD Stop(void);

    NS_IMETHOD IsBusy(PRBool& aResult);

    NS_IMETHOD CreateDocumentLoader(nsIDocumentLoader** anInstance);

    NS_IMETHOD AddObserver(nsIDocumentLoaderObserver *aObserver);
    NS_IMETHOD RemoveObserver(nsIDocumentLoaderObserver *aObserver);

    NS_IMETHOD SetContainer(nsISupports* aContainer);
    NS_IMETHOD GetContainer(nsISupports** aResult);

    // XXX: this method is evil and should be removed.
    NS_IMETHOD GetContentViewerContainer(PRUint32 aDocumentID, 
                                         nsIContentViewerContainer** aResult);

	NS_IMETHOD GetLoadGroup(nsILoadGroup** aResult);

    NS_IMETHOD Destroy();

    // nsIStreamObserver methods: (for observing the load group)
    NS_DECL_NSISTREAMOBSERVER

    // Implementation specific methods...
    nsresult CreateContentViewer(const char *aCommand,
                                 nsIChannel* channel,
                                 const char* aContentType, 
                                 nsISupports* aContainer,
                                 nsISupports* aExtraInfo,
                                 nsIStreamListener** aDocListener,
                                 nsIContentViewer** aDocViewer);

protected:
    virtual ~nsDocLoaderImpl();

    nsresult RemoveChildGroup(nsDocLoaderImpl *aLoader);
    void DocLoaderIsEmpty(nsresult aStatus);

    void FireOnStartDocumentLoad(nsDocLoaderImpl* aLoadInitiator,
                                 nsIURI* aURL);

    void FireOnEndDocumentLoad(nsDocLoaderImpl* aLoadInitiator,
                               nsIChannel *aDocChannel,
                               nsresult aStatus);
							   
    void FireOnStartURLLoad(nsDocLoaderImpl* aLoadInitiator,
                            nsIChannel* channel, 
                            nsIContentViewer* aViewer);

    void FireOnEndURLLoad(nsDocLoaderImpl* aLoadInitiator,
                          nsIChannel* channel, nsresult aStatus);

protected:

    // IMPORTANT: The ownership implicit in the following member
    // variables has been explicitly checked and set using nsCOMPtr
    // for owning pointers and raw COM interface pointers for weak
    // (ie, non owning) references. If you add any members to this
    // class, please make the ownership explicit (pinkerton, scc).
  
    nsCOMPtr<nsIChannel>       mDocumentChannel;       // [OWNER] ???compare with document
    nsVoidArray                mDocObservers;
    nsISupports*               mContainer;             // [WEAK] it owns me!

    nsDocLoaderImpl*           mParent;                // [WEAK]

    nsCString                  mCommand;

    /*
     * This flag indicates that the loader is loading a document.  It is set
     * from the call to LoadDocument(...) until the OnConnectionsComplete(...)
     * notification is fired...
     */
    PRBool mIsLoadingDocument;

    nsCOMPtr<nsILoadGroup>      mLoadGroup;
    nsCOMPtr<nsISupportsArray>  mChildList;
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

    PR_LOG(gDocLoaderLog, PR_LOG_DEBUG, 
           ("DocLoader:%p: created.\n", this));
}

nsresult
nsDocLoaderImpl::Init(nsDocLoaderImpl *aParent)
{
    nsresult rv;

    rv = NS_NewLoadGroup(this, getter_AddRefs(mLoadGroup));
    if (NS_FAILED(rv)) return rv;

    PR_LOG(gDocLoaderLog, PR_LOG_DEBUG, 
           ("DocLoader:%p: load group %x.\n", this, mLoadGroup.get()));

///    rv = mLoadGroup->SetGroupListenerFactory(this);
///    if (NS_FAILED(rv)) return rv;

    rv = NS_NewISupportsArray(getter_AddRefs(mChildList));
    if (NS_FAILED(rv)) return rv;

    mParent = aParent;
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

#ifdef DEBUG
  PRUint32 count=0;
  mChildList->Count(&count);
  NS_PRECONDITION((0 == count), "Document loader has children...");
#endif
}


/*
 * Implementation of ISupports methods...
 */
NS_IMPL_ADDREF(nsDocLoaderImpl);
NS_IMPL_RELEASE(nsDocLoaderImpl);


NS_IMETHODIMP
nsDocLoaderImpl::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
    if (NULL == aInstancePtr) {
        return NS_ERROR_NULL_POINTER;
    }
    if (aIID.Equals(kIDocumentLoaderIID)) {
        *aInstancePtr = (void*)(nsIDocumentLoader*)this;
        NS_ADDREF_THIS();
        return NS_OK;
    }
    if (aIID.Equals(kIStreamObserverIID)) {
        *aInstancePtr = (void*)(nsIStreamObserver*)this;
        NS_ADDREF_THIS();
        return NS_OK;
    }
    if (aIID.Equals(nsCOMTypeInfo<nsISupportsWeakReference>::GetIID())) {
        *aInstancePtr = NS_STATIC_CAST(nsISupportsWeakReference*,this);
        NS_ADDREF_THIS();
        return NS_OK;        
    }
    return NS_NOINTERFACE;
}

NS_IMETHODIMP
nsDocLoaderImpl::CreateDocumentLoader(nsIDocumentLoader** anInstance)
{
  nsDocLoaderImpl* newLoader = nsnull;
  nsresult rv = NS_OK;

  /* Check for initial error conditions... */
  if (nsnull == anInstance) {
    return NS_ERROR_NULL_POINTER;
  }

  *anInstance = nsnull;
  NS_NEWXPCOM(newLoader, nsDocLoaderImpl);
  if (nsnull == newLoader) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  //
  // The QI causes the new document laoder to get AddRefed...  So now
  // its refcount is 1.
  //
  rv = newLoader->QueryInterface(kIDocumentLoaderIID, (void**)anInstance);
  if (NS_FAILED(rv)) {
    delete newLoader;
    return rv;
  }

  // Initialize now that we have a reference
  rv = newLoader->Init(this);
  if (NS_SUCCEEDED(rv)) {
    //
    // XXX this method incorrectly returns a bool    
    //
    rv = mChildList->AppendElement((nsIDocumentLoader*)newLoader) 
       ? NS_OK : NS_ERROR_FAILURE;
  }

  // Delete the new document loader if any error occurs during initialization
  if (NS_FAILED(rv)) {
    NS_RELEASE(*anInstance);
  }

  return rv;
}

nsresult
nsDocLoaderImpl::CreateContentViewer(const char *aCommand,
                                     nsIChannel* channel,
                                     const char* aContentType, 
                                     nsISupports* aContainer,
                                     nsISupports* aExtraInfo,
                                     nsIStreamListener** aDocListenerResult,
                                     nsIContentViewer** aDocViewerResult)
{
    // Lookup class-id for the command plus content-type combination
    nsCID cid;
    char id[500];
    PR_snprintf(id, sizeof(id),
                NS_DOCUMENT_LOADER_FACTORY_PROGID_PREFIX "%s/%s",
                aCommand ? aCommand : "view",/* XXX bug! shouldn't b needed!*/
                aContentType);
    nsresult rv = nsComponentManager::ProgIDToCLSID(id, &cid);
    if (NS_FAILED(rv)) {
        return rv;
    }

    // Create an instance of the document-loader-factory object
    nsIDocumentLoaderFactory* factory;
    rv = nsComponentManager::CreateInstance(cid, (nsISupports *)nsnull,
                                            kIDocumentLoaderFactoryIID, 
                                            (void **)&factory);
    if (NS_FAILED(rv)) {
        return rv;
    }

    // Now create an instance of the content viewer
    rv = factory->CreateInstance(aCommand, 
                                 channel, mLoadGroup,
                                 aContentType,
                                 aContainer,
                                 aExtraInfo, aDocListenerResult,
                                 aDocViewerResult);
    NS_RELEASE(factory);
    return rv;
}

NS_IMETHODIMP
nsDocLoaderImpl::LoadOpenedDocument(nsIChannel * aOpenedChannel, 
                                  const char * aCommand,
                                  nsIContentViewerContainer *aContainer,
                                  nsIInputStream * aPostDataStream,
                                  nsIURI * aReffererUrl,
                                  nsIStreamListener ** aContentHandler)
{
  // mscott - there's a big flaw here...we'll proably need to reset the 
  // loadgroupon the channel to theloadgroup associated with the content
  // viewer container since that's the new guy whose actually going to be
  // receiving the content
  nsresult rv = NS_OK;

  nsDocumentBindInfo* loader = nsnull;
  if (!aOpenedChannel)
    return NS_ERROR_NULL_POINTER;

  NS_NEWXPCOM(loader, nsDocumentBindInfo);
  if (nsnull == loader)
      return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(loader);
  loader->Init(this,           // DocLoader
               aCommand,       // Command
               aContainer,     // Viewer Container
               /* aExtraInfo */ nsnull);    // Extra Info

  loader->QueryInterface(NS_GET_IID(nsIStreamListener), (void **) aContentHandler);

  /*
   * Set the flag indicating that the document loader is in the process of
   * loading a document.  This flag will remain set until the 
   * OnConnectionsComplete(...) notification is fired for the loader...
   */
  mIsLoadingDocument = PR_TRUE;

  // let's try resetting the load group if we need to...
  nsCOMPtr<nsILoadGroup> aCurrentLoadGroup;
  rv = aOpenedChannel->GetLoadGroup(getter_AddRefs(aCurrentLoadGroup));
  if (NS_SUCCEEDED(rv))
  {
    if (aCurrentLoadGroup.get() != mLoadGroup.get())
      aOpenedChannel->SetLoadGroup(mLoadGroup);
  }

  NS_RELEASE(loader);
  return rv;
}


static NS_DEFINE_CID(kURILoaderCID, NS_URI_LOADER_CID);
static NS_DEFINE_CID(kPrefServiceCID, NS_PREF_CID);

  // start up prefs
  

NS_IMETHODIMP
nsDocLoaderImpl::LoadDocument(nsIURI * aUri, 
                              const char* aCommand,
                              nsISupports* aContainer,
                              nsIInputStream* aPostDataStream,
                              nsISupports* aExtraInfo,
                              nsLoadFlags aType,
                              const PRUint32 aLocalIP,
                              const PRUnichar* aReferrer)
{
  nsresult rv = NS_OK;

  NS_WITH_SERVICE(nsIPref, prefs, kPrefServiceCID, &rv); 
  PRBool useURILoader = PR_FALSE;
  if (NS_SUCCEEDED(rv))
    prefs->GetBoolPref("browser.uriloader", &useURILoader);

  nsXPIDLCString aUrlScheme;
  if (aUri)
    aUri->GetScheme(getter_Copies(aUrlScheme));

  // temporary hack alert! whether uri loading is turned on or off,
  // I want mailto urls to properly get sent to the uri loader because
  // it works so well...=)
  // so for now, if the protocol scheme is mailto, for useURILoader to true
  if (nsCRT::strcasecmp(aUrlScheme, "mailto") == 0)
    useURILoader = PR_TRUE;

  // right now, uri dispatching is only hooked up to work with imap, mailbox and
  //  news urls...so as a hack....check the protocol scheme and only call the 
  // dispatching code for those schemes which work with uri dispatching...
  if (useURILoader && aUri)
  { 
     nsCOMPtr<nsISupports> aOpenContext = do_QueryInterface(mLoadGroup);

      // let's try uri dispatching...
      NS_WITH_SERVICE(nsIURILoader, pURILoader, kURILoaderCID, &rv);
      if (NS_SUCCEEDED(rv)) 
      {

          /*
           * Set the flag indicating that the document loader is in the process of
           * loading a document.  This flag will remain set until the 
           * OnConnectionsComplete(...) notification is fired for the loader...
           */
           mIsLoadingDocument = PR_TRUE;

           nsURILoadCommand loadCmd = nsIURILoader::viewNormal;
           if (nsCRT::strcasecmp(aCommand, "view-link-click") == 0)
             loadCmd = nsIURILoader::viewUserClick;
           else if (nsCRT::strcasecmp(aCommand, "view-source") == 0)
             loadCmd = nsIURILoader::viewSource;

        // temporary hack for post data...eventually this snippet of code
        // should be moved into the layout call when callers go through the
        // uri loader directly!

        if (aPostDataStream)
        {
          // query for private post data stream interface
          nsCOMPtr<nsPIURILoaderWithPostData>  postLoader = do_QueryInterface(pURILoader, &rv);
          if (NS_SUCCEEDED(rv))
            rv = postLoader->OpenURIWithPostData(aUri, 
                                                 loadCmd,
                                                 nsnull /* window target */,
                                                 aContainer,
                                                 nsnull /* referring uri */,
                                                 aPostDataStream,
                                                 mLoadGroup,
                                                 getter_AddRefs(aOpenContext));
                                             
        }
        else
          rv = pURILoader->OpenURI(aUri, loadCmd, nsnull /* window target */, 
                                   aContainer,
                                   nsnull /* refferring URI */, 
                                   mLoadGroup, 
                                   getter_AddRefs(aOpenContext));
        if (aOpenContext)
          mLoadGroup = do_QueryInterface(aOpenContext);
      }
      
      return rv;
  } // end try uri loader code

  nsDocumentBindInfo* loader = nsnull;
  if (!aUri)
    return NS_ERROR_NULL_POINTER;

#if defined(DEBUG)
  nsXPIDLCString urlSpec;
  aUri->GetSpec(getter_Copies(urlSpec));
  PR_LOG(gDocLoaderLog, PR_LOG_DEBUG, 
         ("DocLoader:%p: LoadDocument(...) called for %s.", 
          this, (const char *) urlSpec));
#endif /* DEBUG */

  /* Check for initial error conditions... */
  if (nsnull == aContainer) {
      rv = NS_ERROR_NULL_POINTER;
      goto done;
  }

  // Save the command associated with this load...
  mCommand = aCommand;

  NS_NEWXPCOM(loader, nsDocumentBindInfo);
  if (nsnull == loader) {
      rv = NS_ERROR_OUT_OF_MEMORY;
      goto done;
  }
  NS_ADDREF(loader);
  loader->Init(this,           // DocLoader
               aCommand,       // Command
               aContainer,     // Viewer Container
               aExtraInfo);    // Extra Info

  /*
   * Set the flag indicating that the document loader is in the process of
   * loading a document.  This flag will remain set until the 
   * OnConnectionsComplete(...) notification is fired for the loader...
   */
  mIsLoadingDocument = PR_TRUE;

  rv = loader->Bind(aUri, mLoadGroup, aPostDataStream, aReferrer);

done:
  NS_RELEASE(loader);
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

  rv = mLoadGroup->Cancel();

  return rv;
}       


NS_IMETHODIMP
nsDocLoaderImpl::IsBusy(PRBool& aResult)
{
  nsresult rv;

  //
  // A document loader is busy if either:
  //
  //   1. It is currently loading a document (ie. one or more URIs)
  //   2. One of it's child document loaders is busy...
  //
  aResult = PR_FALSE;

  /* Is this document loader busy? */
  if (mIsLoadingDocument) {
    rv = mLoadGroup->IsPending(&aResult);
    if (NS_FAILED(rv)) return rv;
  }

  /* Otherwise, check its child document loaders... */
  if (!aResult) {
    PRUint32 count, i;

    rv = mChildList->Count(&count);
    if (NS_FAILED(rv)) return rv;

    for (i=0; i<count; i++) {
      nsIDocumentLoader* loader;

      loader = NS_STATIC_CAST(nsIDocumentLoader*, mChildList->ElementAt(i));

      if (loader) {
        (void) loader->IsBusy(aResult);
        NS_RELEASE(loader);

        if (aResult) break;
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
nsDocLoaderImpl::GetContentViewerContainer(PRUint32 aDocumentID,
                                           nsIContentViewerContainer** aResult)
{
  nsISupports* base = (nsISupports*) aDocumentID;
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
          nsCOMPtr<nsIContentViewer> viewer;
          nsCOMPtr<nsIWebShell> webShell(do_QueryInterface(mContainer));
          NS_ENSURE_TRUE(webShell, NS_ERROR_FAILURE);
  
          webShell->GetContentViewer(getter_AddRefs(viewer));

          FireOnStartURLLoad(this, aChannel, viewer);
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

    IsBusy(busy);
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

    observer->OnEndDocumentLoad(aLoadInitiator, aDocChannel, aStatus, observer);
  }

  /*
   * Next notify the parent...
   */
  if (mParent) {
    mParent->FireOnEndDocumentLoad(aLoadInitiator, aDocChannel, aStatus);
  }
}


void nsDocLoaderImpl::FireOnStartURLLoad(nsDocLoaderImpl* aLoadInitiator,
                                         nsIChannel* aChannel,
                                         nsIContentViewer* aViewer)
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
                "\tURI: %s Viewer: %x\n", 
                this, buffer, aViewer));
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

    observer->OnStartURLLoad(aLoadInitiator, aChannel, aViewer);
  }

  /*
   * Finally notify the parent...
   */
  if (mParent) {
    mParent->FireOnStartURLLoad(aLoadInitiator, aChannel, aViewer);
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


/****************************************************************************
 * nsDocumentBindInfo implementation...
 ****************************************************************************/

nsDocumentBindInfo::nsDocumentBindInfo()
{
    NS_INIT_REFCNT();

    m_Command = nsnull;
    m_DocLoader = nsnull;
}

nsresult
nsDocumentBindInfo::Init(nsDocLoaderImpl* aDocLoader,
                         const char *aCommand, 
                         nsISupports* aContainer,
                         nsISupports* aExtraInfo)
{
    m_Command    = (nsnull != aCommand) ? PL_strdup(aCommand) : nsnull;

    m_DocLoader = aDocLoader;
    NS_ADDREF(m_DocLoader);

    m_Container = aContainer;  // This does an addref

    m_ExtraInfo = aExtraInfo;  // This does an addref

    return NS_OK;
}

nsDocumentBindInfo::~nsDocumentBindInfo()
{
    if (m_Command) {
        PR_Free(m_Command);
    }
    m_Command = nsnull;

    NS_RELEASE   (m_DocLoader);
}

/*
 * Implementation of ISupports methods...
 */
NS_IMPL_ADDREF(nsDocumentBindInfo);
NS_IMPL_RELEASE(nsDocumentBindInfo);

nsresult
nsDocumentBindInfo::QueryInterface(const nsIID& aIID,
                                   void** aInstancePtrResult)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null pointer");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }

  *aInstancePtrResult = NULL;

  if (aIID.Equals(kIStreamObserverIID)) {
    *aInstancePtrResult = (void*) ((nsIStreamObserver*)this);
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(kIStreamListenerIID)) {
    *aInstancePtrResult = (void*) ((nsIStreamListener*)this);
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(kISupportsIID)) {
    *aInstancePtrResult = (void*) ((nsISupports*) this);
    NS_ADDREF_THIS();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}

nsresult nsDocumentBindInfo::Bind(nsIURI* aURL, 
                                  nsILoadGroup *aLoadGroup,
                                  nsIInputStream *postDataStream,
                                  const PRUnichar* aReferrer)
{
  nsresult rv = NS_OK;

  // XXXbe this knows that m_Container implements nsIWebShell
  nsCOMPtr<nsIInterfaceRequestor> capabilities = do_QueryInterface(m_Container);
  nsCOMPtr<nsIChannel> channel;
  rv = NS_OpenURI(getter_AddRefs(channel), aURL, aLoadGroup, capabilities);
  if (NS_FAILED(rv)) return rv;

  if (postDataStream || aReferrer)
  {
      nsCOMPtr<nsIHTTPChannel> httpChannel(do_QueryInterface(channel));
      if (httpChannel)
      {
          if (postDataStream) {
              httpChannel->SetRequestMethod(HM_POST);
              httpChannel->SetPostDataStream(postDataStream);
          }
          if (aReferrer) {
              // Referer - misspelled, but per the HTTP spec
              nsCAutoString str = aReferrer;
              nsCOMPtr<nsIAtom> key = NS_NewAtom("referer");
              httpChannel->SetRequestHeader(key, str.GetBuffer());
          }
      }
  }

  rv = channel->AsyncRead(0, -1, nsnull, this);

  return rv;
}


NS_IMETHODIMP
nsDocumentBindInfo::OnStartRequest(nsIChannel* channel, nsISupports *ctxt)
{
    nsresult rv = NS_OK;
    nsIContentViewer* viewer = nsnull;

    nsCOMPtr<nsIURI> aURL;
    rv = channel->GetURI(getter_AddRefs(aURL));
    if (NS_FAILED(rv)) return rv;
    char* aContentType = nsnull;
    rv = channel->GetContentType(&aContentType);
    if (NS_FAILED(rv)) return rv;

#if defined(DEBUG)
    char* spec;
    (void)aURL->GetSpec(&spec);

    PR_LOG(gDocLoaderLog, PR_LOG_DEBUG, 
           ("DocumentBindInfo:%p OnStartRequest(...) called for %s.  Content-type is %s\n",
            this, spec, aContentType));
    nsCRT::free(spec);
#endif /* DEBUG */

    if (nsnull == m_NextStream) {
        /*
         * Now that the content type is available, create a document
         * (and viewer) of the appropriate type...
         */
        if (m_DocLoader) {

            rv = m_DocLoader->CreateContentViewer(m_Command, 
                                                  channel,
                                                  aContentType, 
                                                  m_Container,
                                                  m_ExtraInfo,
                                                  getter_AddRefs(m_NextStream), 
                                                  &viewer);
        } else {
            rv = NS_ERROR_NULL_POINTER;
        }

        if (NS_FAILED(rv)) {
            printf("DocLoaderFactory: Unable to create ContentViewer for command=%s, content-type=%s\n", m_Command ? m_Command : "(null)", aContentType);
            nsCOMPtr<nsIContentViewerContainer> 
               cvContainer(do_QueryInterface(m_Container));
            if ( cvContainer ) {
                // Give content container a chance to do something with this URL.
                rv = cvContainer->HandleUnknownContentType( (nsIDocumentLoader*) m_DocLoader, channel, aContentType, m_Command );
            }
            // Stop the binding.
            // This crashes on Unix/Mac... Stop();
            goto done;
        }

        /*
         * Give the document container the new viewer...
         */
        nsCOMPtr<nsIContentViewerContainer> 
                                 cvContainer(do_QueryInterface(m_Container));
        if (cvContainer) {
            viewer->SetContainer(m_Container);

            rv = cvContainer->Embed(viewer, m_Command, m_ExtraInfo);
            if (NS_FAILED(rv)) {
                goto done;
            }
        }
    }

    /*
     * Pass the OnStartRequest(...) notification out to the document 
     * IStreamListener.
     */
    if (nsnull != m_NextStream) {
        rv = m_NextStream->OnStartRequest(channel, ctxt);
    }

  done:
    NS_IF_RELEASE(viewer);
    nsCRT::free(aContentType);
    return rv;
}


NS_METHOD nsDocumentBindInfo::OnDataAvailable(nsIChannel* channel, nsISupports *ctxt,
                                              nsIInputStream *aStream, 
                                              PRUint32 sourceOffset, 
                                              PRUint32 aLength)
{
    nsresult rv;

    nsCOMPtr<nsIURI> aURL;
    rv = channel->GetURI(getter_AddRefs(aURL));
    if (NS_FAILED(rv)) return rv;

#if defined(DEBUG)
    char* spec;
    (void)aURL->GetSpec(&spec);

    PR_LOG(gDocLoaderLog, PR_LOG_DEBUG, 
           ("DocumentBindInfo:%p: OnDataAvailable(...) called for %s.  Bytes available: %d.\n", 
            this, spec, aLength));
    nsCRT::free(spec);
#endif /* DEBUG */

    if (nsnull != m_NextStream) {
       /*
        * Bump the refcount in case the stream gets destroyed while the data
        * is being processed...  If Stop(...) is called the stream could be
        * freed prematurely :-(
        *
        * Currently this can happen if javascript loads a new URL 
        * (via nsIWebShell::LoadURL) during the parse phase... 
        */
        nsCOMPtr<nsIStreamListener> listener(m_NextStream);

        rv = listener->OnDataAvailable(channel, ctxt, aStream, sourceOffset, aLength);
    } else {
      rv = NS_BINDING_FAILED;
    }

    return rv;
}


NS_METHOD nsDocumentBindInfo::OnStopRequest(nsIChannel* channel, nsISupports *ctxt,
                                            nsresult aStatus, const PRUnichar *aMsg)
{
    nsresult rv;

    nsCOMPtr<nsIURI> aURL;
    rv = channel->GetURI(getter_AddRefs(aURL));
    if (NS_FAILED(rv)) return rv;

#if defined(DEBUG)
    char* spec;
    (void)aURL->GetSpec(&spec);
    PR_LOG(gDocLoaderLog, PR_LOG_DEBUG, 
           ("DocumentBindInfo:%p: OnStopRequest(...) called for %s.  Status: %d.\n", 
            this, spec, aStatus));
    nsCRT::free(spec);
#endif // DEBUG

    if (NS_FAILED(aStatus)) {
        char *url;
        aURL->GetSpec(&url);

        if (aStatus == NS_ERROR_UNKNOWN_HOST)
            printf("Error: Unknown host: %s\n", url);
        else if (aStatus == NS_ERROR_MALFORMED_URI)
            printf("Error: Malformed URI: %s\n", url);
        else if (NS_FAILED(aStatus))
            printf("Error: Can't load: %s (%x)\n", url, aStatus);
        nsCRT::free(url);
    }

    if (nsnull != m_NextStream) {
        rv = m_NextStream->OnStopRequest(channel, ctxt, aStatus, aMsg);
    }

    m_NextStream = nsnull;  // Release our stream we are holding

    return rv;
}



/*******************************************
 *  nsDocLoaderServiceFactory
 *******************************************/
static nsDocLoaderImpl* gServiceInstance = nsnull;

NS_IMETHODIMP
nsDocLoaderImpl::Create(nsISupports *aOuter, const nsIID &aIID, void **aResult)
{
  nsresult rv;
  nsDocLoaderImpl* inst;

  // Parameter validation...
  if (NULL == aResult) {
    rv = NS_ERROR_NULL_POINTER;
    goto done;
  }
  // Do not support aggregatable components...
  *aResult = NULL;
  if (NULL != aOuter) {
    rv = NS_ERROR_NO_AGGREGATION;
    goto done;
  }

  if (NULL == gServiceInstance) {
    // Create a new instance of the component...
    NS_NEWXPCOM(gServiceInstance, nsDocLoaderImpl);
    if (NULL == gServiceInstance) {
      rv = NS_ERROR_OUT_OF_MEMORY;
      goto done;
    }
  }

  // If the QI fails, the component will be destroyed...
  //
  // Use a local copy so the NS_RELEASE() will not null the global
  // pointer...
  inst = gServiceInstance;

  NS_ADDREF(inst);
  rv = inst->QueryInterface(aIID, aResult);
  if (NS_SUCCEEDED(rv)) {
    rv = inst->Init(nsnull);
  }
  NS_RELEASE(inst);

done:
  return rv;
}

// Entry point to create nsDocLoaderService factory instances...

nsresult NS_NewDocLoaderServiceFactory(nsIFactory** aResult)
{
  nsresult rv = NS_OK;
  nsIGenericFactory* factory;
  rv = nsComponentManager::CreateInstance(kGenericFactoryCID, nsnull, 
                                          nsIGenericFactory::GetIID(),
                                          (void**)&factory);
  if (NS_FAILED(rv)) return rv;

  rv = factory->SetConstructor(nsDocLoaderImpl::Create);
  if (NS_FAILED(rv)) {
      NS_RELEASE(factory);
      return rv;
  }

  *aResult = factory;
  return rv;
}

