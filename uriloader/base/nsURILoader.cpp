/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include "nsURILoader.h"
#include "nsIURIContentListener.h"
#include "nsIContentHandler.h"
#include "nsILoadGroup.h"
#include "nsIDocumentLoader.h"
#include "nsIWebProgress.h"
#include "nsIWebProgressListener.h"
#include "nsIIOService.h"
#include "nsIServiceManager.h"
#include "nsIStreamListener.h"
#include "nsIURI.h"
#include "nsIChannel.h"
#include "nsIInterfaceRequestor.h"
#include "nsIProgressEventSink.h"
#include "nsIInputStream.h"
#include "nsIStreamConverterService.h"
#include "nsWeakReference.h"

#include "nsIDocShellTreeItem.h"
#include "nsIDocShellTreeOwner.h"

#include "nsVoidArray.h"
#include "nsXPIDLString.h"
#include "nsString.h"

#include "nsIDOMWindow.h"
#include "nsIUnkContentTypeHandler.h"

static NS_DEFINE_CID(kURILoaderCID, NS_URI_LOADER_CID);
static NS_DEFINE_CID(kStreamConverterServiceCID, NS_STREAMCONVERTERSERVICE_CID);

///////////////////////////////////////////////////////////////////
// The nsLoadCookie is an nsISupports class that is opaque to users
// of the URI Loader service. The intent is to use the load cookie 
// as a "load context" so we can keep a load group and a doc loader
// associated with multiple openURI requests on the uri loader from
// the same caller. i.e. if the caller has a load cookie, they pass
// it into the uri loader so we can group this request with other
// requests from the same caller. If you don't have a load cookie yet,
// we will provide one for you.
///////////////////////////////////////////////////////////////////
class nsLoadCookie : public nsIInterfaceRequestor,
                     public nsSupportsWeakReference
{
  NS_DECL_ISUPPORTS
  NS_DECL_NSIINTERFACEREQUESTOR

  nsLoadCookie();
  nsresult Init(nsISupports * aParentCookie);
protected: 
  virtual ~nsLoadCookie();
  
  nsCOMPtr<nsILoadGroup> mLoadGroup;
  nsCOMPtr<nsIDocumentLoader> mDocLoader;
};

NS_IMPL_ADDREF(nsLoadCookie);
NS_IMPL_RELEASE(nsLoadCookie);

NS_INTERFACE_MAP_BEGIN(nsLoadCookie)
   NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIInterfaceRequestor)
   NS_INTERFACE_MAP_ENTRY(nsIInterfaceRequestor)
   NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
NS_INTERFACE_MAP_END

nsLoadCookie::nsLoadCookie()
{
  NS_INIT_ISUPPORTS();
}

nsLoadCookie::~nsLoadCookie()
{
  // if we are going away, then the doc loader we're associated with needs to be
  // destroyed....
  if (mDocLoader)
    mDocLoader->Destroy();
}

nsresult
nsLoadCookie::Init(nsISupports * aParentCookie)
{
  nsCOMPtr<nsIDocumentLoader> parentDocLoader;
  nsresult rv = NS_OK;
  if (aParentCookie) // if we had a parent cookie use it to help with the creation process
    parentDocLoader = do_GetInterface(aParentCookie);

  if (!parentDocLoader)
    parentDocLoader = do_GetService(NS_DOCUMENTLOADER_SERVICE_PROGID, &rv);

  NS_ENSURE_TRUE(parentDocLoader, NS_ERROR_FAILURE);

  parentDocLoader->CreateDocumentLoader(getter_AddRefs(mDocLoader));

  // now turn around and get the load group that the doc loader created...
  NS_ENSURE_SUCCESS(mDocLoader->GetLoadGroup(getter_AddRefs(mLoadGroup)), NS_ERROR_FAILURE);

  return rv;
}

NS_IMETHODIMP
nsLoadCookie::GetInterface(const nsIID &aIID, void** aInstancePtr)
{
   NS_ENSURE_ARG_POINTER(aInstancePtr);
   if(aIID.Equals(NS_GET_IID(nsILoadGroup)))
   {
      *aInstancePtr = mLoadGroup;
      NS_ADDREF((nsISupports*)*aInstancePtr);
      return NS_OK;
   }
   else if(aIID.Equals(NS_GET_IID(nsIDocumentLoader)))
   {
      *aInstancePtr = mDocLoader;
      NS_ADDREF((nsISupports*)*aInstancePtr);
      return NS_OK;
   }
   else
     return NS_ERROR_NO_INTERFACE;
}

/* 
 * The nsDocumentOpenInfo contains the state required when a single document
 * is being opened in order to discover the content type...  Each instance remains alive until its target URL has 
 * been loaded (or aborted).
 *
 */
class nsDocumentOpenInfo : public nsIStreamListener
{
public:
    nsDocumentOpenInfo();

    nsresult Init(nsISupports * aRetargetedWindowContext, 
                  nsISupports * aOriginalWindowContext);

    NS_DECL_ISUPPORTS

    nsresult Open(nsIChannel * aChannel, 
                  nsURILoadCommand aCommand,
                  const char * aWindowTarget,
                  nsISupports * aWindowContext);

    nsresult DispatchContent(nsIChannel * aChannel, nsISupports * aCtxt);
    nsresult RetargetOutput(nsIChannel * aChannel, const char * aSrcContentType, 
                            const char * aOutContentType, nsIStreamListener * aStreamListener);

    // nsIStreamObserver methods:
    NS_DECL_NSISTREAMOBSERVER

	  // nsIStreamListener methods:
    NS_DECL_NSISTREAMLISTENER

protected:
    virtual ~nsDocumentOpenInfo();
    nsDocumentOpenInfo* Clone();

    nsresult InvokeUnknownContentHandler(nsIChannel * aChannel, const char * aContentType, nsIDOMWindow * aDomWindow);

protected:
    nsCOMPtr<nsIURIContentListener> m_contentListener;
    nsCOMPtr<nsIStreamListener> m_targetStreamListener;
    nsCOMPtr<nsISupports> m_originalContext;
    nsURILoadCommand mCommand;
    nsCString m_windowTarget;
};

NS_IMPL_THREADSAFE_ADDREF(nsDocumentOpenInfo);
NS_IMPL_THREADSAFE_RELEASE(nsDocumentOpenInfo);

NS_INTERFACE_MAP_BEGIN(nsDocumentOpenInfo)
   NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIStreamObserver)
   NS_INTERFACE_MAP_ENTRY(nsIStreamObserver)
   NS_INTERFACE_MAP_ENTRY(nsIStreamListener)
NS_INTERFACE_MAP_END_THREADSAFE

nsDocumentOpenInfo::nsDocumentOpenInfo()
{
  NS_INIT_ISUPPORTS();
}

nsDocumentOpenInfo::~nsDocumentOpenInfo()
{
}

nsresult nsDocumentOpenInfo::Init(nsISupports * aCurrentWindowContext, 
                                  nsISupports * aOriginalWindowContext)
{
  // ask the window context if it has a uri content listener...
  nsresult rv = NS_OK;
  m_contentListener = do_GetInterface(aCurrentWindowContext, &rv);
  m_originalContext = aOriginalWindowContext;
  return rv;
}

nsDocumentOpenInfo* nsDocumentOpenInfo::Clone()
{
  nsDocumentOpenInfo* newObject;

  newObject = new nsDocumentOpenInfo();
  if (newObject) {
    newObject->m_contentListener = m_contentListener;
    newObject->mCommand          = mCommand;
    newObject->m_windowTarget    = m_windowTarget;
  }

  return newObject;
}

nsresult nsDocumentOpenInfo::Open(nsIChannel * aChannel,  
                                  nsURILoadCommand aCommand,
                                  const char * aWindowTarget,
                                  nsISupports * aWindowContext)
{
   // this method is not complete!!! Eventually, we should first go
  // to the content listener and ask them for a protocol handler...
  // if they don't give us one, we need to go to the registry and get
  // the preferred protocol handler. 

  // But for now, I'm going to let necko do the work for us....

  nsresult rv = NS_OK;
  // store any local state
  m_windowTarget = aWindowTarget;
  mCommand = aCommand;

  // now just open the channel!
  if (aChannel)
    rv =  aChannel->AsyncRead(this, nsnull);
  return rv;
}

NS_IMETHODIMP nsDocumentOpenInfo::OnStartRequest(nsIChannel * aChannel, nsISupports * aCtxt)
{
  nsresult rv = NS_OK;
  rv = DispatchContent(aChannel, aCtxt);
  if (m_targetStreamListener)
    rv = m_targetStreamListener->OnStartRequest(aChannel, aCtxt);
  return rv;
}

NS_IMETHODIMP nsDocumentOpenInfo::OnDataAvailable(nsIChannel * aChannel, nsISupports * aCtxt,
                                                  nsIInputStream * inStr, PRUint32 sourceOffset, PRUint32 count)
{
  // if we have retarged to the end stream listener, then forward the call....
  // otherwise, don't do anything

  nsresult rv = NS_OK;
  if (m_targetStreamListener)
    rv = m_targetStreamListener->OnDataAvailable(aChannel, aCtxt, inStr, sourceOffset, count);
  return rv;
}

NS_IMETHODIMP nsDocumentOpenInfo::OnStopRequest(nsIChannel * aChannel, nsISupports *aCtxt, 
                                                nsresult aStatus, const PRUnichar * errorMsg)
{
  nsresult rv = NS_OK;
  if (m_targetStreamListener)
    m_targetStreamListener->OnStopRequest(aChannel, aCtxt, aStatus, errorMsg);

  m_targetStreamListener = 0;

  return rv;
}

nsresult nsDocumentOpenInfo::DispatchContent(nsIChannel * aChannel, nsISupports * aCtxt)
{
  nsresult rv;
  nsXPIDLCString contentType;
  nsCOMPtr<nsISupports> originalWindowContext = m_originalContext; // local variable to keep track of this.

  rv = aChannel->GetContentType(getter_Copies(contentType));
  if (NS_FAILED(rv)) return rv;

   // go to the uri dispatcher and give them our stuff...
  NS_WITH_SERVICE(nsIURILoader, pURILoader, kURILoaderCID, &rv);
  if (NS_SUCCEEDED(rv))
  {
    nsCOMPtr<nsIURIContentListener> contentListener;
    nsXPIDLCString desiredContentType;

    //
    // First step:  See if any nsIURIContentListener prefers to handle this
    //              content type.
    //
    PRBool abortDispatch = PR_FALSE;
    rv = pURILoader->DispatchContent(contentType, mCommand, m_windowTarget, 
                                     aChannel, aCtxt, 
                                     m_contentListener, 
                                     m_originalContext,
                                     getter_Copies(desiredContentType), 
                                     getter_AddRefs(contentListener),
                                     &abortDispatch);
    m_originalContext = nsnull; // we don't need this anymore....

    // if the uri loader says to abort the dispatch then someone
    // else must have stepped in and taken over for us...so stop..

    if (abortDispatch) return NS_OK;
    //
    // Second step:  If no listener prefers this type, see if any stream
    //               decoders exist to transform this content type into
    //               some other.
    //
    if (!contentListener) {
      nsDocumentOpenInfo* nextLink;

      // When applying stream decoders, it is necessary to "insert" an 
      // intermediate nsDocumentOpenInfo instance to handle the targeting of
      // the "final" stream or streams.
      //
      // For certain content types (ie. multi-part/x-mixed-replace) the input
      // stream is split up into multiple destination streams.  This
      // intermediate instance is used to target these "decoded" streams...
      //
      nextLink = Clone();
      if (!nextLink) return NS_ERROR_OUT_OF_MEMORY;
      NS_ADDREF(nextLink);

      // Set up the final destination listener.
      nextLink->m_targetStreamListener = nsnull;

      // The following call binds this channelListener's mNextListener (typically
      // the nsDocumentBindInfo) to the underlying stream converter, and returns
      // the underlying stream converter which we then set to be this channelListener's
      // mNextListener. This effectively nestles the stream converter down right
      // in between the raw stream and the final listener.

      // catch the case when some joker server sends back a content type of "*/*"
      // because we said we could handle "*/*" in our accept headers
      if (nsCRT::strcmp(contentType, "*/*")) {
          rv = RetargetOutput(aChannel, contentType, "*/*", nextLink);
          NS_RELEASE(nextLink);

          if (m_targetStreamListener) {
            return NS_OK;
          }
      } else {
        NS_RELEASE(nextLink);
      }
    }
    
    //
    // Step 3:
    //
    // BIG TIME HACK ALERT!!!!! WE NEED THIS HACK IN PLACE UNTIL OUR NEW UNKNOWN CONTENT
    // HANDLER COMES ONLINE!!! 
    // Until that day, if we couldn't find a handler for the content type, then go back to the listener who
    // originated the url request and force them to handle the content....this forces us through the old code
    // path for unknown content types which brings up the file save as dialog...
    if (!contentListener) {
      contentListener = m_contentListener;
    }

    //
    // Good news!  Some content listener can handle this content type.
    //
    if (contentListener)
    {
      nsCOMPtr<nsIStreamListener> contentStreamListener;
      PRBool bAbortProcess = PR_FALSE;     
      nsCAutoString contentTypeToUse;
      if (desiredContentType)
        contentTypeToUse.Assign(desiredContentType);
      else
        contentTypeToUse.Assign(contentType);

      // We need to first figure out if we are retargeting the load to a content listener
      // that is different from the one that originated the request....if so, set
      // LOAD_RETARGETED_DOCUMENT_URI on the channel. 

      if (contentListener.get() != m_contentListener.get())
      {
         // we must be retargeting...so set an appropriate flag on the channel
        nsLoadFlags loadAttribs = 0;
        aChannel->GetLoadAttributes(&loadAttribs);
        loadAttribs |= nsIChannel::LOAD_RETARGETED_DOCUMENT_URI;
        aChannel->SetLoadAttributes(loadAttribs);
      }

      rv = contentListener->DoContent(contentTypeToUse, mCommand, m_windowTarget, 
                                    aChannel, getter_AddRefs(contentStreamListener),
                                    &bAbortProcess);

      // the listener is doing all the work from here...we are done!!!
      if (bAbortProcess) return rv;

      if (NS_FAILED(rv))
      {
        nsCOMPtr<nsIDOMWindow> domWindow (do_GetInterface(originalWindowContext));
        return InvokeUnknownContentHandler(aChannel, contentType, domWindow);
      }

      // okay, all registered listeners have had a chance to handle this content...
      // did one of them give us a stream listener back? if so, let's start reading data
      // into it...
      rv = RetargetOutput(aChannel, contentType, desiredContentType, contentStreamListener);
    } 
  }
  return rv;
}

nsresult nsDocumentOpenInfo::InvokeUnknownContentHandler(nsIChannel * aChannel, const char * aContentType, nsIDOMWindow * aDomWindow)
{
  NS_ENSURE_ARG(aChannel);
  NS_ENSURE_ARG(aDomWindow);

  nsCOMPtr<nsIUnknownContentTypeHandler> handler (do_GetService(NS_IUNKNOWNCONTENTTYPEHANDLER_PROGID));
  NS_ENSURE_TRUE(handler, NS_ERROR_FAILURE);

  return handler->HandleUnknownContentType( aChannel, aContentType, aDomWindow );
}

nsresult nsDocumentOpenInfo::RetargetOutput(nsIChannel * aChannel, const char * aSrcContentType, const char * aOutContentType,
                                            nsIStreamListener * aStreamListener)
{
  nsresult rv = NS_OK;
  // do we need to invoke the stream converter service?
  if (aOutContentType && *aOutContentType && nsCRT::strcasecmp(aSrcContentType, aOutContentType))
  {
      NS_WITH_SERVICE(nsIStreamConverterService, StreamConvService, kStreamConverterServiceCID, &rv);
      if (NS_FAILED(rv)) return rv;
      nsAutoString from_w; from_w.AssignWithConversion (aSrcContentType);
      nsAutoString to_w; to_w.AssignWithConversion (aOutContentType);

      // The following call binds this channelListener's mNextListener (typically
      // the nsDocumentBindInfo) to the underlying stream converter, and returns
      // the underlying stream converter which we then set to be this channelListener's
      // mNextListener. This effectively nestles the stream converter down right
      // in between the raw stream and the final listener.
      rv = StreamConvService->AsyncConvertData(from_w.GetUnicode(), 
                                               to_w.GetUnicode(), 
                                               aStreamListener, 
                                               aChannel,
                                               getter_AddRefs(m_targetStreamListener));
  }
  else
    m_targetStreamListener = aStreamListener; // no converter necessary so use a direct pipe

  return rv;
}

///////////////////////////////////////////////////////////////////////////////////////////////
// Implementation of nsURILoader
///////////////////////////////////////////////////////////////////////////////////////////////

nsURILoader::nsURILoader()
{
  NS_INIT_ISUPPORTS();
  m_listeners = new nsVoidArray();
}

nsURILoader::~nsURILoader()
{
  if (m_listeners)
    delete m_listeners;
}

NS_IMPL_ADDREF(nsURILoader);
NS_IMPL_RELEASE(nsURILoader);

NS_INTERFACE_MAP_BEGIN(nsURILoader)
   NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIURILoader)
   NS_INTERFACE_MAP_ENTRY(nsIURILoader)
NS_INTERFACE_MAP_END

NS_IMETHODIMP nsURILoader::RegisterContentListener(nsIURIContentListener * aContentListener)
{
  nsresult rv = NS_OK;
  if (m_listeners)
    m_listeners->AppendElement(aContentListener);
  else
    rv = NS_ERROR_FAILURE;

  return rv;
} 

NS_IMETHODIMP nsURILoader::UnRegisterContentListener(nsIURIContentListener * aContentListener)
{
  if (m_listeners)
    m_listeners->RemoveElement(aContentListener);
  return NS_OK;
  
}

NS_IMETHODIMP nsURILoader::OpenURI(nsIChannel * aChannel, 
                                   nsURILoadCommand aCommand,
                                   const char * aWindowTarget,
                                   nsISupports * aWindowContext)
{
  return OpenURIVia(aChannel, aCommand, aWindowTarget, aWindowContext, 0 /* ip address */); 
}

NS_IMETHODIMP nsURILoader::GetTarget(const char * aWindowTarget, 
                                     nsISupports * aWindowContext,
                                     nsISupports ** aRetargetedWindowContext)
{
  nsAutoString name; name.AssignWithConversion(aWindowTarget);
  nsCOMPtr<nsIDocShellTreeItem> windowCtxtAsTreeItem (do_GetInterface(aWindowContext));
  nsCOMPtr<nsIDocShellTreeItem>  treeItem;
  *aRetargetedWindowContext = nsnull;

  if(!name.Length() || name.EqualsIgnoreCase("_self") || name.EqualsIgnoreCase("_blank"))
  {
     *aRetargetedWindowContext = aWindowContext;
  }
  else if(name.EqualsIgnoreCase("_parent"))
  {
      windowCtxtAsTreeItem->GetSameTypeParent(getter_AddRefs(treeItem));
      if(!treeItem)
        *aRetargetedWindowContext = aWindowContext;
  }
  else if(name.EqualsIgnoreCase("_top"))
  {
      windowCtxtAsTreeItem->GetSameTypeRootTreeItem(getter_AddRefs(treeItem));
      if(!treeItem)
        *aRetargetedWindowContext = aWindowContext;
  }
  else if(name.EqualsIgnoreCase("_content"))
  {
    nsCOMPtr<nsIDocShellTreeOwner> treeOwner;
    windowCtxtAsTreeItem->GetTreeOwner(getter_AddRefs(treeOwner));

    if (treeOwner)
       treeOwner->FindItemWithName(name.GetUnicode(), nsnull, getter_AddRefs(treeItem));
    else
    {
      NS_ERROR("Someone isn't setting up the tree owner.  You might like to try that."
          "Things will.....you know, work.");
    }
  }
  else
  {
    windowCtxtAsTreeItem->FindItemWithName(name.GetUnicode(), nsnull, getter_AddRefs(treeItem));
  }

  nsCOMPtr<nsISupports> treeItemCtxt (do_QueryInterface(treeItem));
  if (!*aRetargetedWindowContext)  
    *aRetargetedWindowContext = treeItemCtxt;

  NS_IF_ADDREF(*aRetargetedWindowContext);
  return NS_OK;
}

NS_IMETHODIMP nsURILoader::OpenURIVia(nsIChannel * aChannel, 
                                      nsURILoadCommand aCommand,
                                      const char * aWindowTarget,
                                      nsISupports * aOriginalWindowContext,
                                      PRUint32 aLocalIP)
{
  // we need to create a DocumentOpenInfo object which will go ahead and open the url
  // and discover the content type....

  nsresult rv = NS_OK;
  nsDocumentOpenInfo* loader = nsnull;

  if (!aChannel) return NS_ERROR_NULL_POINTER;

  // Let the window context's uriListener know that the open is starting.  This
  // gives that window a chance to abort the load process.
  nsCOMPtr<nsIURIContentListener> winContextListener(do_GetInterface(aOriginalWindowContext));
  if(winContextListener)
    {
    nsCOMPtr<nsIURI> uri;
    aChannel->GetURI(getter_AddRefs(uri));
    if(uri)
      {
      PRBool doAbort = PR_FALSE;
      winContextListener->OnStartURIOpen(uri, aWindowTarget, &doAbort);
         
      if(doAbort)
         return NS_OK;
      }   
    }

  nsCOMPtr<nsISupports> retargetedWindowContext;
  NS_ENSURE_SUCCESS(GetTarget(aWindowTarget, aOriginalWindowContext, getter_AddRefs(retargetedWindowContext)), NS_ERROR_FAILURE);

  NS_NEWXPCOM(loader, nsDocumentOpenInfo);
  if (!loader) return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(loader);

  nsCOMPtr<nsISupports> loadCookie;
  SetupLoadCookie(retargetedWindowContext, getter_AddRefs(loadCookie));

  loader->Init(retargetedWindowContext, aOriginalWindowContext);    // Extra Info

  // now instruct the loader to go ahead and open the url
  rv = loader->Open(aChannel, aCommand, aWindowTarget, retargetedWindowContext);
  NS_RELEASE(loader);

  return NS_OK;
}

NS_IMETHODIMP nsURILoader::Stop(nsISupports* aLoadCookie)
{
   NS_ENSURE_TRUE(aLoadCookie, NS_ERROR_INVALID_ARG);

   nsCOMPtr<nsIDocumentLoader> docLoader(do_GetInterface(aLoadCookie));
   NS_ENSURE_TRUE(docLoader, NS_ERROR_FAILURE);

   NS_ENSURE_SUCCESS(docLoader->Stop(), NS_ERROR_FAILURE);
   return NS_OK;
}

NS_IMETHODIMP
nsURILoader::GetLoadGroupForContext(nsISupports * aWindowContext, nsILoadGroup ** aLoadGroup)
{
  NS_ENSURE_ARG(aWindowContext);
  nsCOMPtr<nsISupports> loadCookieForWindow;
  NS_ENSURE_SUCCESS(SetupLoadCookie(aWindowContext, getter_AddRefs(loadCookieForWindow)), NS_ERROR_FAILURE);
  
  nsCOMPtr<nsIInterfaceRequestor> requestor (do_QueryInterface(loadCookieForWindow));
  NS_ENSURE_TRUE(requestor, NS_ERROR_FAILURE);
  
  return requestor->GetInterface(NS_GET_IID(nsILoadGroup), (void **) aLoadGroup);
}

NS_IMETHODIMP
nsURILoader::GetDocumentLoaderForContext(nsISupports * aWindowContext, nsIDocumentLoader ** aLoadGroup)
{
  NS_ENSURE_ARG(aWindowContext);
  nsCOMPtr<nsISupports> loadCookieForWindow;
  NS_ENSURE_SUCCESS(SetupLoadCookie(aWindowContext, getter_AddRefs(loadCookieForWindow)), NS_ERROR_FAILURE);
  
  nsCOMPtr<nsIInterfaceRequestor> requestor (do_QueryInterface(loadCookieForWindow));
  NS_ENSURE_TRUE(requestor, NS_ERROR_FAILURE);
  
  return requestor->GetInterface(NS_GET_IID(nsIDocumentLoader), (void **) aLoadGroup);
}

nsresult nsURILoader::SetupLoadCookie(nsISupports * aWindowContext, nsISupports ** aLoadCookie)
{
  // first, see if we have already set a load cookie on the cnt listener..
  // i.e. if this isn't the first time we've tried to run a url through this window
  // context then we don't need to create another load cookie, we can reuse the first one.
  nsresult rv = NS_OK;
  nsCOMPtr<nsISupports> loadCookie;

  nsCOMPtr<nsIURIContentListener> cntListener (do_GetInterface(aWindowContext));
  if (cntListener)
  {
    rv = cntListener->GetLoadCookie(getter_AddRefs(loadCookie));
    if (NS_FAILED(rv) || !loadCookie)
    {
      // if we don't have a load cookie for this window context yet, then 
      // go create one! In order to create a load cookie, we need to get
      // the parent's load cookie if there is one...
      nsCOMPtr<nsIURIContentListener> parentListener;
      cntListener->GetParentContentListener(getter_AddRefs(parentListener));
      if (parentListener)
        rv = parentListener->GetLoadCookie(getter_AddRefs(loadCookie));
      nsLoadCookie * newLoadCookie = new nsLoadCookie();
      if (newLoadCookie)
      {
        newLoadCookie->Init(loadCookie);        
        rv = cntListener->SetLoadCookie (NS_STATIC_CAST(nsISupports *, (nsIInterfaceRequestor *) newLoadCookie));
        newLoadCookie->QueryInterface(NS_GET_IID(nsISupports), getter_AddRefs(loadCookie)); 
      } // if we created a new cookie
    } // if we don't have  a load cookie already
  } // if we have a cntListener

  // every time we do a load, we should reset the progress listener on it...
  if (loadCookie)
  {
    // bind the web progress listener (if there is one) to the web progress
    // instance of the doc loader..
    nsCOMPtr<nsIDocumentLoader> docLoader (do_GetInterface(loadCookie));
    nsCOMPtr<nsIWebProgress> webProgress (do_QueryInterface(docLoader));
    nsCOMPtr<nsIWebProgressListener> webProgressListener (do_GetInterface(aWindowContext));
    if (webProgress && webProgressListener)
      webProgress->AddProgressListener(webProgressListener);
  }
  
  *aLoadCookie = loadCookie;
  NS_IF_ADDREF(*aLoadCookie);

  return rv;
}

PRBool nsURILoader::ShouldHandleContent(nsIURIContentListener * aCntListener, 
                                        const char * aContentType,
                                        nsURILoadCommand aCommand,
                                        const char * aWindowTarget,
                                        char ** aContentTypeToUse)
{
  PRBool foundContentHandler = PR_FALSE;
  if (aCommand == nsIURILoader::viewUserClick)
    aCntListener->IsPreferred(aContentType, aCommand, aWindowTarget, 
                                aContentTypeToUse, 
                                &foundContentHandler);
  else
    aCntListener->CanHandleContent(aContentType, aCommand, aWindowTarget, 
                                   aContentTypeToUse, 
                                   &foundContentHandler);
  return foundContentHandler;
} 

NS_IMETHODIMP nsURILoader::DispatchContent(const char * aContentType,
                                           nsURILoadCommand aCommand,
                                           const char * aWindowTarget,
                                           nsIChannel * aChannel, 
                                           nsISupports * aCtxt, 
                                           nsIURIContentListener * aContentListener,
                                           nsISupports * aSrcWindowContext,
                                           char ** aContentTypeToUse,
                                           nsIURIContentListener ** aContentListenerToUse,
                                           PRBool * aAbortProcess)
{
  NS_ENSURE_ARG(aContentType);
  NS_ENSURE_ARG(aChannel);

  // okay, now we've discovered the content type. We need to do the following:
  // (1) We always start with the original content listener (if any) that originated the request
  // and then ask if it can handle the content.
  // (2) if it can't, we'll move on to the registered content listeners and give 
  // them a crack at handling the content.

  // (3) if we cannot find a registered content lister to handle the type, then we move on to
  // phase II which is to try to find a content handler in the registry for the content type.
  // hitting this phase usually means we'll be creating a new window or handing off to an 
  // external application.

  nsresult rv = NS_OK;

  nsCOMPtr<nsIURIContentListener> listenerToUse = aContentListener;
  PRBool skipRetargetingSearch = PR_FALSE;
  // How do we determine whether we need to ask any registered content listeners if they
  // want a crack at the content? 
  // (1) if the window target is blank or new, then we don't want to ask...
  if (!nsCRT::strcasecmp(aWindowTarget, "_blank") || !nsCRT::strcasecmp(aWindowTarget, "_new"))
    skipRetargetingSearch = PR_TRUE;
  else
  {
    // (2) if the original content listener is NULL and we have a target name then we
    // must not be a window open with that target name so skip the content listener search
    // and skip to the part that brings up the new window.
    if (aWindowTarget && *aWindowTarget && !aContentListener)
      skipRetargetingSearch = PR_TRUE;
  }
  
  // find a content handler that can and will handle the content
  if (!skipRetargetingSearch)
  {
    PRBool foundContentHandler = PR_FALSE;
    if (listenerToUse)
      foundContentHandler = ShouldHandleContent(listenerToUse, aContentType, 
                                                aCommand, aWindowTarget, aContentTypeToUse);
                                            

    if (!foundContentHandler) // if it can't handle the content, scan through the list of registered listeners
    {
       PRInt32 i = 0;
       // keep looping until we get a content listener back
       for(i = 0; i < m_listeners->Count() && !foundContentHandler; i++)
	     {
	        //nsIURIContentListener's aren't refcounted.
		      nsIURIContentListener * listener =(nsIURIContentListener*)m_listeners->ElementAt(i);
          if (listener)
          {
              foundContentHandler = ShouldHandleContent(listener, aContentType, 
                                                        aCommand, aWindowTarget, aContentTypeToUse);
              if (foundContentHandler)
                listenerToUse = listener;
          }
      } // for loop
    } // if we can't handle the content


    if (foundContentHandler && listenerToUse)
    {
      *aContentListenerToUse = listenerToUse;
      NS_IF_ADDREF(*aContentListenerToUse);
      return rv;
    }
  }

  // no registered content listeners to handle this type!!! so go to the register 
  // and get a registered nsIContentHandler for our content type. Hand it off 
  // to them...
  // eventually we want to hit up the category manager so we can allow people to
  // over ride the default content type handlers....for now...i'm skipping that part.

  nsCAutoString handlerProgID (NS_CONTENT_HANDLER_PROGID_PREFIX);
  handlerProgID += aContentType;
  
  nsCOMPtr<nsIContentHandler> aContentHandler;
  rv = nsComponentManager::CreateInstance(handlerProgID, nsnull, NS_GET_IID(nsIContentHandler), getter_AddRefs(aContentHandler));
  if (NS_SUCCEEDED(rv)) // we did indeed have a content handler for this type!! yippee...
  {
      rv = aContentHandler->HandleContent(aContentType, "view", aWindowTarget, aSrcWindowContext, aChannel);
      *aAbortProcess = PR_TRUE;
  }
  
  return rv;
}

