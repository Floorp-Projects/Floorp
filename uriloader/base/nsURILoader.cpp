/* -*- Mode: C++; tab-width: 2; indent-tabs-mode:nil; c-basic-offset: 2 -*- */
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
 * Portions created by the Initial Developer are Copyright (C) 1999
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
#include "nsIInterfaceRequestorUtils.h"
#include "nsIProgressEventSink.h"
#include "nsIInputStream.h"
#include "nsIStreamConverterService.h"
#include "nsWeakReference.h"
#include "nsIHttpChannel.h"

#include "nsIDocShell.h"
#include "nsIDocShellTreeItem.h"
#include "nsIDocShellTreeOwner.h"

#include "nsXPIDLString.h"
#include "nsString.h"

#include "nsIDOMWindowInternal.h"
#include "nsDOMError.h"

#include "nsCExternalHandlerService.h" // contains contractids for the helper app service

static NS_DEFINE_CID(kStreamConverterServiceCID, NS_STREAMCONVERTERSERVICE_CID);


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

    NS_DECL_ISUPPORTS

    nsresult Open(nsIChannel* channel, 
                  PRBool aIsContentPreferred,
                  nsISupports * aWindowContext);

    nsresult DispatchContent(nsIRequest *request, nsISupports * aCtxt);
    nsresult RetargetOutput(nsIRequest *request, const char * aSrcContentType, 
                            const char * aOutContentType, nsIStreamListener * aStreamListener);

    // nsIRequestObserver methods:
    NS_DECL_NSIREQUESTOBSERVER

    // nsIStreamListener methods:
    NS_DECL_NSISTREAMLISTENER

protected:
    virtual ~nsDocumentOpenInfo();
    nsDocumentOpenInfo* Clone();

protected:
    nsCOMPtr<nsIURIContentListener> m_contentListener;
    nsCOMPtr<nsIStreamListener> m_targetStreamListener;
    nsCOMPtr<nsISupports> m_originalContext;
    PRBool mIsContentPreferred;
};

NS_IMPL_THREADSAFE_ADDREF(nsDocumentOpenInfo);
NS_IMPL_THREADSAFE_RELEASE(nsDocumentOpenInfo);

NS_INTERFACE_MAP_BEGIN(nsDocumentOpenInfo)
   NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIRequestObserver)
   NS_INTERFACE_MAP_ENTRY(nsIRequestObserver)
   NS_INTERFACE_MAP_ENTRY(nsIStreamListener)
NS_INTERFACE_MAP_END_THREADSAFE

nsDocumentOpenInfo::nsDocumentOpenInfo()
{
  NS_INIT_ISUPPORTS();
}

nsDocumentOpenInfo::~nsDocumentOpenInfo()
{
}

nsDocumentOpenInfo* nsDocumentOpenInfo::Clone()
{
  nsDocumentOpenInfo* newObject;

  newObject = new nsDocumentOpenInfo();
  if (newObject) {
    newObject->m_contentListener   = m_contentListener;
    newObject->mIsContentPreferred = mIsContentPreferred;
    newObject->m_originalContext   = m_originalContext;
  }

  return newObject;
}

nsresult nsDocumentOpenInfo::Open(nsIChannel *aChannel,  
                                  PRBool aIsContentPreferred,
                                  nsISupports * aWindowContext)
{
   // this method is not complete!!! Eventually, we should first go
  // to the content listener and ask them for a protocol handler...
  // if they don't give us one, we need to go to the registry and get
  // the preferred protocol handler. 

  // But for now, I'm going to let necko do the work for us....

  nsresult rv;

  // store any local state
  mIsContentPreferred = aIsContentPreferred;

  m_originalContext = aWindowContext;

  // ask the window context if it has a uri content listener...
  m_contentListener = do_GetInterface(aWindowContext, &rv);
  if (NS_FAILED(rv)) return rv;

  // now just open the channel!
  if (aChannel){
    rv =  aChannel->AsyncOpen(this, nsnull);
  }

  // no content from this load - that's OK.
  if (rv == NS_ERROR_DOM_RETVAL_UNDEFINED ||
      rv == NS_ERROR_NO_CONTENT) {
      rv = NS_OK;
  }

  return rv;
}

NS_IMETHODIMP nsDocumentOpenInfo::OnStartRequest(nsIRequest *request, nsISupports * aCtxt)
{
  nsresult rv = NS_OK;

  //
  // Deal with "special" HTTP responses:
  //
  // - In the case of a 204 (No Content) response, do not try to find a
  //   content handler.  Just return.  This causes the request to be
  //   ignored.
  //
  nsCOMPtr<nsIHttpChannel> httpChannel(do_QueryInterface(request, &rv));

  if (NS_SUCCEEDED(rv)) {
    PRUint32 responseCode = 0;

    rv = httpChannel->GetResponseStatus(&responseCode);

    if (NS_FAILED(rv)) {
      // behave as in the canceled case
      return NS_OK;
    }

    if (204 == responseCode) {
      return NS_OK;
    }
  }

  //
  // Make sure that the transaction has succeeded, so far...
  //
  nsresult status;

  rv = request->GetStatus(&status);
  
  NS_ASSERTION(NS_SUCCEEDED(rv), "Unable to get request status!");
  if (NS_FAILED(rv)) return rv;

  if (NS_FAILED(status)) {
    //
    // The transaction has already reported an error - so it will be torn
    // down. Therefore, it is not necessary to return an error code...
    //
    return NS_OK;
  }

  rv = DispatchContent(request, aCtxt);
  if (m_targetStreamListener)
    rv = m_targetStreamListener->OnStartRequest(request, aCtxt);
  return rv;
}

NS_IMETHODIMP nsDocumentOpenInfo::OnDataAvailable(nsIRequest *request, nsISupports * aCtxt,
                                                  nsIInputStream * inStr, PRUint32 sourceOffset, PRUint32 count)
{
  // if we have retarged to the end stream listener, then forward the call....
  // otherwise, don't do anything

  nsresult rv = NS_OK;
  
  if (m_targetStreamListener)
    rv = m_targetStreamListener->OnDataAvailable(request, aCtxt, inStr, sourceOffset, count);
  return rv;
}

NS_IMETHODIMP nsDocumentOpenInfo::OnStopRequest(nsIRequest *request, nsISupports *aCtxt, 
                                                nsresult aStatus)
{
  if ( m_targetStreamListener)
  {
    nsCOMPtr<nsIStreamListener> listener(m_targetStreamListener);

    m_targetStreamListener = 0;
    listener->OnStopRequest(request, aCtxt, aStatus);
  }

  // Remember...
  // In the case of multiplexed streams (such as multipart/x-mixed-replace)
  // these stream listener methods could be called again :-)
  //
  return NS_OK;
}

nsresult nsDocumentOpenInfo::DispatchContent(nsIRequest *request, nsISupports * aCtxt)
{
  nsresult rv;
  nsXPIDLCString contentType;
  nsCOMPtr<nsISupports> originalWindowContext = m_originalContext; // local variable to keep track of this.
  nsCOMPtr<nsIStreamListener> contentStreamListener;
  nsCOMPtr<nsIChannel> aChannel = do_QueryInterface(request);
  if (!aChannel) {
      return NS_ERROR_FAILURE;
  }

  rv = aChannel->GetContentType(getter_Copies(contentType));
  if (NS_FAILED(rv)) return rv;

   // go to the uri dispatcher and give them our stuff...
  nsCOMPtr<nsIURILoader> uriLoader;
  uriLoader = do_GetService(NS_URI_LOADER_CONTRACTID, &rv);
  if (NS_SUCCEEDED(rv))
  {
    nsCOMPtr<nsIURIContentListener> contentListener;
    nsXPIDLCString desiredContentType;

    //
    // First step:  See if any nsIURIContentListener prefers to handle this
    //              content type.
    //
    PRBool abortDispatch = PR_FALSE;
    rv = uriLoader->DispatchContent(contentType, mIsContentPreferred, 
                                    request, aCtxt, 
                                    m_contentListener, 
                                    m_originalContext,
                                    getter_Copies(desiredContentType), 
                                    getter_AddRefs(contentListener),
                                    &abortDispatch);  

    // if the uri loader says to abort the dispatch then someone
    // else must have stepped in and taken over for us...so stop..

    if (abortDispatch) return NS_OK;
    //
    // Second step:  If no listener prefers this type, see if any stream
    //               decoders exist to transform this content type into
    //               some other.
    //
    if (!contentListener) 
    {
      rv = RetargetOutput(request, contentType, "*/*", nsnull);
      if (m_targetStreamListener)
        return NS_OK;
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
        nsLoadFlags loadFlags = 0;
        aChannel->GetLoadFlags(&loadFlags);
        loadFlags |= nsIChannel::LOAD_RETARGETED_DOCUMENT_URI;
        aChannel->SetLoadFlags(loadFlags);
      }

      rv = contentListener->DoContent(contentTypeToUse.get(),
                                      mIsContentPreferred,
                                      request,
                                      getter_AddRefs(contentStreamListener),
                                      &bAbortProcess);

      // Do not continue loading if nsIURIContentListener::DoContent(...)
      // fails - It means that an unexpected error occurred...
      //
      // If bAbortProcess is TRUE then the listener is doing all the work from
      // here...we are done!!!
      if (NS_FAILED(rv) || bAbortProcess) {
        return rv;
      }

      // try to detect if there is a helper application we an use...
      if (!contentStreamListener)
      {
        nsCOMPtr<nsIURI> uri;
        PRBool abortProcess = PR_FALSE;
        aChannel->GetURI(getter_AddRefs(uri));
        nsCOMPtr<nsIExternalHelperAppService> helperAppService (do_GetService(NS_EXTERNALHELPERAPPSERVICE_CONTRACTID));
        if (helperAppService)
        {
            rv = helperAppService->DoContent(contentType, uri, m_originalContext, &abortProcess, getter_AddRefs(contentStreamListener));
            if (NS_SUCCEEDED(rv) && contentStreamListener)
              return RetargetOutput(request, contentType, contentType, contentStreamListener);
        }
        rv = NS_ERROR_FAILURE; // this will cause us to bring up the unknown content handler dialog.
      }

      // okay, all registered listeners have had a chance to handle this content...
      // did one of them give us a stream listener back? if so, let's start reading data
      // into it...
      rv = RetargetOutput(request, contentType, desiredContentType, contentStreamListener);
      m_originalContext = nsnull; // we don't need this anymore....
    } 
  }
  return rv;
}

nsresult nsDocumentOpenInfo::RetargetOutput(nsIRequest *request, const char * aSrcContentType, const char * aOutContentType,
                                            nsIStreamListener * aStreamListener)
{
  nsresult rv = NS_OK;

  // catch the case when some joker server sends back a content type of "*/*"
  // because we said we could handle "*/*" in our accept headers
  if (aOutContentType && *aOutContentType && nsCRT::strcasecmp(aSrcContentType, aOutContentType) && nsCRT::strcmp(aSrcContentType, "*/*"))
  {
      nsCOMPtr<nsIStreamConverterService> StreamConvService = 
               do_GetService(kStreamConverterServiceCID, &rv);
      if (NS_FAILED(rv)) return rv;
      nsAutoString from_w; from_w.AssignWithConversion (aSrcContentType);
      nsAutoString to_w; to_w.AssignWithConversion (aOutContentType);
      
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
      rv = StreamConvService->AsyncConvertData(from_w.get(), 
                                               to_w.get(), 
                                               nextLink, 
                                               request,
                                               getter_AddRefs(m_targetStreamListener));
      NS_RELEASE(nextLink);
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
  NS_NewISupportsArray(getter_AddRefs(m_listeners));
}

nsURILoader::~nsURILoader()
{
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
  if (!m_listeners) return NS_ERROR_FAILURE;

  nsWeakPtr weakListener = do_GetWeakReference(aContentListener);
  NS_ASSERTION(weakListener, "your URIContentListener must support weak refs!\n");
  
  if (weakListener)
    m_listeners->AppendElement(weakListener);

  return rv;
} 

NS_IMETHODIMP nsURILoader::UnRegisterContentListener(nsIURIContentListener * aContentListener)
{
  if (!m_listeners) return NS_OK;
    
  nsWeakPtr weakListener = do_GetWeakReference(aContentListener);
  if (weakListener)
    m_listeners->RemoveElement(weakListener);

  return NS_OK;
  
}

NS_IMETHODIMP nsURILoader::OpenURI(nsIChannel *channel, 
                                   PRBool aIsContentPreferred,
                                   nsISupports * aWindowContext)
{
  return OpenURIVia(channel, aIsContentPreferred, aWindowContext,
                    0 /* ip address */); 
}

NS_IMETHODIMP nsURILoader::OpenURIVia(nsIChannel *channel, 
                                      PRBool aIsContentPreferred,
                                      nsISupports * aWindowContext,
                                      PRUint32 aLocalIP)
{
  // we need to create a DocumentOpenInfo object which will go ahead and open the url
  // and discover the content type....
  nsresult rv = NS_OK;
  nsDocumentOpenInfo* loader = nsnull;

  if (!channel) return NS_ERROR_NULL_POINTER;

  // Let the window context's uriListener know that the open is starting.  This
  // gives that window a chance to abort the load process.
  nsCOMPtr<nsIURIContentListener> winContextListener(do_GetInterface(aWindowContext));
  if(winContextListener) {
      // get channel from request
    nsCOMPtr<nsIURI> uri;
    channel->GetURI(getter_AddRefs(uri));
    if(uri) {
      PRBool doAbort = PR_FALSE;
      winContextListener->OnStartURIOpen(uri, &doAbort);
         
      if(doAbort)
         return NS_OK;
    }   
  }

  NS_NEWXPCOM(loader, nsDocumentOpenInfo);
  if (!loader) return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(loader);

  nsCOMPtr<nsIInterfaceRequestor> loadCookie;
  SetupLoadCookie(aWindowContext, getter_AddRefs(loadCookie));

  // now instruct the loader to go ahead and open the url
  rv = loader->Open(channel, aIsContentPreferred, aWindowContext);
  NS_RELEASE(loader);

  return rv;
}

NS_IMETHODIMP nsURILoader::Stop(nsISupports* aLoadCookie)
{
  nsresult rv;
  nsCOMPtr<nsIDocumentLoader> docLoader;

  NS_ENSURE_ARG_POINTER(aLoadCookie);

  docLoader = do_GetInterface(aLoadCookie, &rv);
  if (docLoader) {
    rv = docLoader->Stop();
  }
  return rv;
}

NS_IMETHODIMP
nsURILoader::GetLoadGroupForContext(nsISupports * aWindowContext,
                                    nsILoadGroup ** aLoadGroup)
{
  nsresult rv;
  nsCOMPtr<nsIInterfaceRequestor> loadCookieForWindow;

  // Initialize the [out] parameter...
  *aLoadGroup= nsnull;

  NS_ENSURE_ARG(aWindowContext);

  rv = SetupLoadCookie(aWindowContext, getter_AddRefs(loadCookieForWindow));
  if (NS_FAILED(rv)) return rv;
  
  rv = loadCookieForWindow->GetInterface(NS_GET_IID(nsILoadGroup),
                                         (void **) aLoadGroup);
  return rv;
}

NS_IMETHODIMP
nsURILoader::GetDocumentLoaderForContext(nsISupports * aWindowContext,
                                         nsIDocumentLoader ** aDocLoader)
{
  nsresult rv;
  nsCOMPtr<nsIInterfaceRequestor> loadCookieForWindow;

  // Initialize the [out] parameter...
  *aDocLoader = nsnull;

  NS_ENSURE_ARG(aWindowContext);

  rv = SetupLoadCookie(aWindowContext, getter_AddRefs(loadCookieForWindow));
  if (NS_FAILED(rv)) return rv;
  
  rv = loadCookieForWindow->GetInterface(NS_GET_IID(nsIDocumentLoader), 
                                         (void **) aDocLoader);
  return rv;
}

nsresult nsURILoader::SetupLoadCookie(nsISupports * aWindowContext, 
                                      nsIInterfaceRequestor ** aLoadCookie)
{
  // first, see if we have already set a load cookie on the cnt listener..
  // i.e. if this isn't the first time we've tried to run a url through this window
  // context then we don't need to create another load cookie, we can reuse the first one.
  nsresult rv = NS_OK;
  nsCOMPtr<nsISupports> loadCookie;

  // Initialize the [out] parameter...
  *aLoadCookie = nsnull;

  nsCOMPtr<nsIURIContentListener> cntListener (do_GetInterface(aWindowContext));
  if (cntListener) {
    // Get the load cookie for the requested window context...
    rv = cntListener->GetLoadCookie(getter_AddRefs(loadCookie));

    //
    // If we don't have a load cookie for this window context yet, then 
    // go create one! In order to create a load cookie, we need to get
    // the parent's load cookie if there is one...
    //
    if (!loadCookie) {
      nsCOMPtr<nsIURIContentListener> parentListener;
      nsCOMPtr<nsIDocumentLoader>     parentDocLoader;
      nsCOMPtr<nsIDocumentLoader>     newDocLoader;

      // Try to get the parent's load cookie...
      cntListener->GetParentContentListener(getter_AddRefs(parentListener));
      if (parentListener) {
        rv = parentListener->GetLoadCookie(getter_AddRefs(loadCookie));

        // if we had a parent cookie use it to help with the creation process      
        if (loadCookie) {
          parentDocLoader = do_GetInterface(loadCookie);
        }
      }
      // If there is no parent DocLoader, then use the global DocLoader
      // service as the parent...
      if (!parentDocLoader) {
        parentDocLoader = do_GetService(NS_DOCUMENTLOADER_SERVICE_CONTRACTID, &rv);
      }
      if (NS_FAILED(rv)) return rv;

      //
      // Create a new document loader.  The document loader represents
      // the load cookie which the uriloader hands out...
      //
      rv = parentDocLoader->CreateDocumentLoader(getter_AddRefs(newDocLoader));
      if (NS_FAILED(rv)) return rv;

      newDocLoader->QueryInterface(NS_GET_IID(nsIInterfaceRequestor), 
                                   getter_AddRefs(loadCookie)); 
      rv = cntListener->SetLoadCookie(loadCookie);
    } // if we don't have  a load cookie already
  } // if we have a cntListener

  // loadCookie may be null - for example, <a target="popupWin"> if popupWin is
  // not a defined window.  The following prevents a crash (Bug 32898)
  if (loadCookie) {
    rv = loadCookie->QueryInterface(NS_GET_IID(nsIInterfaceRequestor),
                                  (void**)aLoadCookie);
  } else {
    rv = NS_ERROR_UNEXPECTED;
  }

  return rv;
}

PRBool nsURILoader::ShouldHandleContent(nsIURIContentListener * aCntListener, 
                                        const char * aContentType,
                                        PRBool aIsContentPreferred,
                                        char ** aContentTypeToUse)
{
  PRBool foundContentHandler = PR_FALSE;
  if (aIsContentPreferred) {
    aCntListener->IsPreferred(aContentType,
                              aContentTypeToUse, 
                              &foundContentHandler);
  } else {
    aCntListener->CanHandleContent(aContentType, PR_FALSE,
                                   aContentTypeToUse, 
                                   &foundContentHandler);
  }
  return foundContentHandler;
} 

NS_IMETHODIMP nsURILoader::DispatchContent(const char * aContentType,
                                           PRBool aIsContentPreferred,
                                           nsIRequest *request, 
                                           nsISupports * aCtxt, 
                                           nsIURIContentListener * aContentListener,
                                           nsISupports * aSrcWindowContext,
                                           char ** aContentTypeToUse,
                                           nsIURIContentListener ** aContentListenerToUse,
                                           PRBool * aAbortProcess)
{
  NS_ENSURE_ARG(aContentType);
  NS_ENSURE_ARG(request);

  // okay, now we've discovered the content type. We need to do the
  // following:
  // (1) We always start with the original content listener (if any)
  //     that originated the request and then ask if it can handle the
  //     content.
  // (2) if it can't, we'll move on to the registered content
  //     listeners and give them a crack at handling the content.
  // (3) if we cannot find a registered content lister to handle the
  //     type, then we move on to phase II which is to try to find a
  //     content handler in the registry for the content type.
  //     hitting this phase usually means we'll be creating a new
  //     window or handing off to an external application.

  nsresult rv = NS_OK;

  nsCOMPtr<nsIURIContentListener> listenerToUse = aContentListener;

  PRBool foundContentHandler = PR_FALSE;
  if (listenerToUse)
    foundContentHandler = ShouldHandleContent(listenerToUse,
                                              aContentType, 
                                              aIsContentPreferred,
                                              aContentTypeToUse);
                                            

  // if it can't handle the content, scan through the list of
  // registered listeners
  if (!foundContentHandler)
  {
    PRUint32 count = 0;
    PRInt32 i;
    
    // keep looping until we get a content listener back
    m_listeners->Count(&count);
    for(i = 0; i < (PRInt32)count && !foundContentHandler; i++)
    {
      //nsIURIContentListener's aren't refcounted.
      nsWeakPtr weakListener;
      nsCOMPtr<nsIURIContentListener> listener;

      m_listeners->QueryElementAt(i, NS_GET_IID(nsIWeakReference),
                                  getter_AddRefs(weakListener));
         
      listener = do_QueryReferent(weakListener);
      if (listener)
      {
        foundContentHandler = ShouldHandleContent(listener,
                                                  aContentType, 
                                                  aIsContentPreferred,
                                                  aContentTypeToUse);
        if (foundContentHandler) {
          listenerToUse = listener;
        }
      } else {
        // remove from the listener list, and reset i
        m_listeners->RemoveElementAt(i);
        i--;
      }
    } // for loop
  } // if we can't handle the content


  if (foundContentHandler && listenerToUse)
  {
    *aContentListenerToUse = listenerToUse;
    NS_IF_ADDREF(*aContentListenerToUse);
    return rv;
  }

  // no registered content listeners to handle this type!!! so go to the register 
  // and get a registered nsIContentHandler for our content type. Hand it off 
  // to them...
  // eventually we want to hit up the category manager so we can allow people to
  // over ride the default content type handlers....for now...i'm skipping that part.

  nsCAutoString handlerContractID (NS_CONTENT_HANDLER_CONTRACTID_PREFIX);
  handlerContractID += aContentType;
  
  nsCOMPtr<nsIContentHandler> aContentHandler;
  aContentHandler = do_CreateInstance(handlerContractID.get(), &rv);
  if (NS_SUCCEEDED(rv)) // we did indeed have a content handler for this type!! yippee...
  {
      rv = aContentHandler->HandleContent(aContentType, "view", aSrcWindowContext, request);
      *aAbortProcess = PR_TRUE;
  }
  
  return rv;
}

