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
#include "nsIIOService.h"
#include "nsIServiceManager.h"
#include "nsIStreamListener.h"
#include "nsIURI.h"
#include "nsIChannel.h"
#include "nsIInterfaceRequestor.h"
#include "nsIProgressEventSink.h"
#include "nsIInputStream.h"
#include "nsIStreamConverterService.h"
#include "nsIHttpChannel.h"
#include "nsHTTPEnums.h"

#include "nsVoidArray.h"
#include "nsXPIDLString.h"
#include "nsString.h"

static NS_DEFINE_CID(kIOServiceCID, NS_IOSERVICE_CID);
static NS_DEFINE_CID(kURILoaderCID, NS_URI_LOADER_CID);
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

    nsresult Init(nsISupports * aWindowContext);

    NS_DECL_ISUPPORTS

    nsresult Open(nsIURI *aURL, 
                  const char * aWindowTarget,
                  nsISupports * aWindowContext,
                  nsIURI * aReferringURI,
                  nsIInputStream * aInputStream,
                  nsISupports * aOpenContext,
                  nsISupports ** aCurrentOpenContext);

    nsresult DispatchContent(nsIChannel * aChannel, nsISupports * aCtxt);
    nsresult RetargetOutput(nsIChannel * aChannel, const char * aSrcContentType, 
                            const char * aOutContentType, nsIStreamListener * aStreamListener);

    // nsIStreamObserver methods:
    NS_DECL_NSISTREAMOBSERVER

	  // nsIStreamListener methods:
    NS_DECL_NSISTREAMLISTENER

protected:
    virtual ~nsDocumentOpenInfo();

protected:
    nsCOMPtr<nsIURIContentListener> m_contentListener;
    nsCOMPtr<nsIStreamListener> m_targetStreamListener;
    nsCString m_windowTarget;
};

NS_IMPL_ADDREF(nsDocumentOpenInfo);
NS_IMPL_RELEASE(nsDocumentOpenInfo);

NS_INTERFACE_MAP_BEGIN(nsDocumentOpenInfo)
   NS_INTERFACE_MAP_ENTRY(nsIStreamObserver)
   NS_INTERFACE_MAP_ENTRY(nsIStreamListener)
NS_INTERFACE_MAP_END

nsDocumentOpenInfo::nsDocumentOpenInfo()
{
  NS_INIT_ISUPPORTS();
}

nsDocumentOpenInfo::~nsDocumentOpenInfo()
{
}

nsresult nsDocumentOpenInfo::Init(nsISupports * aWindowContext)
{
  // ask the window context if it has a uri content listener...
  nsresult rv = NS_OK;
  m_contentListener = do_GetInterface(aWindowContext, &rv);
  return rv;
}

nsresult nsDocumentOpenInfo::Open(nsIURI *aURI, 
                                  const char * aWindowTarget,
                                  nsISupports * aWindowContext,
                                  nsIURI * aReferringURI,
                                  nsIInputStream * aPostData,
                                  nsISupports * aOpenContext,
                                  nsISupports ** aCurrentOpenContext)
{
   // this method is not complete!!! Eventually, we should first go
  // to the content listener and ask them for a protocol handler...
  // if they don't give us one, we need to go to the registry and get
  // the preferred protocol handler. 

  // But for now, I'm going to let necko do the work for us....

  // store any local state
  m_windowTarget = aWindowTarget;

  // get the requestor for the window context...
  nsCOMPtr<nsIInterfaceRequestor> requestor = do_QueryInterface(aWindowContext);


  // and get the load group out of the open context
  nsCOMPtr<nsILoadGroup> aLoadGroup = do_QueryInterface(aOpenContext);
  if (!aLoadGroup)
  { 
    // i haven't implemented this yet...it's going to be hard
    // because it requires a persistant stream observer (the uri loader maybe???)
    // that we don't have in this architecture in order to create a new load group
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  if (aCurrentOpenContext)
    aLoadGroup->QueryInterface(NS_GET_IID(nsISupports), (void **) aCurrentOpenContext);

  // now we have all we need, so go get the necko channel service so we can 
  // open the url. 

  nsresult rv = NS_OK;
  NS_WITH_SERVICE(nsIIOService, pNetService, kIOServiceCID, &rv);
  if (NS_SUCCEEDED(rv))
  {
    nsCOMPtr<nsIChannel> aChannel;
    rv = pNetService->NewChannelFromURI("", aURI, aLoadGroup, requestor,
                                        nsIChannel::LOAD_NORMAL, aReferringURI, getter_AddRefs(aChannel));
    if (NS_FAILED(rv)) return rv; // uhoh we were unable to get a channel to handle the url!!!
    // figure out if we need to set the post data stream on the channel...right now, 
    // this is only done for http channels.....

    if (aPostData || aReferringURI)
    {
      nsCOMPtr<nsIHTTPChannel> httpChannel(do_QueryInterface(aChannel));
      if (httpChannel)
      {
          if (aPostData)
          {
              httpChannel->SetRequestMethod(HM_POST);
              httpChannel->SetPostDataStream(aPostData);
          }
          if (aReferringURI) 
          {
              // Referer - misspelled, but per the HTTP spec
              nsCOMPtr<nsIAtom> key = NS_NewAtom("referer");
              nsXPIDLCString aSpec;
              aReferringURI->GetSpec(getter_Copies(aSpec));
              httpChannel->SetRequestHeader(key, aSpec);
          }
      }
    } // if post data || a refferring uri
    rv =  aChannel->AsyncRead(0, -1, nsnull, this);
  }

  return rv;
}

NS_IMETHODIMP nsDocumentOpenInfo::OnStartRequest(nsIChannel * aChannel, nsISupports * aCtxt)
{
  nsresult rv = NS_OK;
  rv = DispatchContent(aChannel, aCtxt);
  if (m_targetStreamListener)
    m_targetStreamListener->OnStartRequest(aChannel, aCtxt);
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
  if (m_targetStreamListener)
    m_targetStreamListener->OnStopRequest(aChannel, aCtxt, aStatus, errorMsg);

  return NS_OK;
}

nsresult nsDocumentOpenInfo::DispatchContent(nsIChannel * aChannel, nsISupports * aCtxt)
{
  nsXPIDLCString aContentType;
  nsresult rv = aChannel->GetContentType(getter_Copies(aContentType));
  if (NS_FAILED(rv)) return rv;

  // go to the uri dispatcher and give them our stuff...
  NS_WITH_SERVICE(nsIURILoader, pURILoader, kURILoaderCID, &rv);
  if (NS_SUCCEEDED(rv))
  {
    nsCOMPtr<nsIURIContentListener> aContentListener;
    nsXPIDLCString aDesiredContentType;
    rv = pURILoader->DispatchContent(aContentType, "view", m_windowTarget, 
                                     aChannel, aCtxt, 
                                     m_contentListener, 
                                     getter_Copies(aDesiredContentType), 
                                     getter_AddRefs(aContentListener));
    if (NS_SUCCEEDED(rv) && aContentListener)
    {
      nsCOMPtr<nsIStreamListener> aContentStreamListener;
      PRBool aAbortProcess = PR_FALSE;
      nsCAutoString contentTypeToUse;
      if (aDesiredContentType)
        contentTypeToUse = aDesiredContentType;
      else
        contentTypeToUse = aContentType;

      rv = aContentListener->DoContent(contentTypeToUse, "view", m_windowTarget, 
                                    aChannel, getter_AddRefs(aContentStreamListener),
                                    &aAbortProcess);

      // the listener is doing all the work from here...we are done!!!
      if (aAbortProcess) return rv;

      // okay, all registered listeners have had a chance to handle this content...
      // did one of them give us a stream listener back? if so, let's start reading data
      // into it...
      rv = RetargetOutput(aChannel, aContentType, aDesiredContentType, aContentStreamListener);
    }
  }
  return rv;
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
      nsAutoString aUnicSrc (aSrcContentType);
      nsAutoString aUniTo (aOutContentType);

      // The following call binds this channelListener's mNextListener (typically
      // the nsDocumentBindInfo) to the underlying stream converter, and returns
      // the underlying stream converter which we then set to be this channelListener's
      // mNextListener. This effectively nestles the stream converter down right
      // in between the raw stream and the final listener.
      rv = StreamConvService->AsyncConvertData(aUnicSrc.GetUnicode(), aUniTo.GetUnicode(), aStreamListener, aChannel,
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
   NS_INTERFACE_MAP_ENTRY(nsIURILoader)
   NS_INTERFACE_MAP_ENTRY(nsPIURILoaderWithPostData)
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

NS_IMETHODIMP nsURILoader::OpenURI(nsIURI *aURI, 
                                   const char * aWindowTarget,
                                   nsISupports * aWindowContext,
                                   nsIURI *aReferringURI,
                                   nsISupports *aOpenContext, 
                                   nsISupports **aCurrentOpenContext)
{
  return OpenURIVia(aURI, aWindowTarget, aWindowContext, aReferringURI, aOpenContext, 
                    aCurrentOpenContext, 0 /* ip address */); 
}

NS_IMETHODIMP nsURILoader::OpenURIVia(nsIURI *aURI, 
                                      const char * aWindowTarget,
                                      nsISupports * aWindowContext,
                                      nsIURI *aReferringURI,
                                      nsISupports *aOpenContext, 
                                      nsISupports **aCurrentOpenContext,
                                      PRUint32 aLocalIP)
{
  // forward our call
  return OpenURIWithPostDataVia(aURI, aWindowTarget, aWindowContext, aReferringURI, nsnull  /* post stream */,
                                aOpenContext,aCurrentOpenContext, aLocalIP);
}

NS_IMETHODIMP nsURILoader::OpenURIWithPostData(nsIURI *aURI, 
                                  const char *aWindowTarget, 
                                  nsISupports * aWindowContext,
                                  nsIURI *aReferringURI, 
                                  nsIInputStream *aPostDataStream, 
                                  nsISupports *aOpenContext, 
                                  nsISupports **aCurrentOpenContext)
{
  return OpenURIWithPostDataVia(aURI, aWindowTarget, aWindowContext, aReferringURI, 
                                aPostDataStream, aOpenContext, aCurrentOpenContext, 0);

}

NS_IMETHODIMP nsURILoader::OpenURIWithPostDataVia(nsIURI *aURI, 
                                     const char *aWindowTarget, 
                                     nsISupports * aWindowContext,
                                     nsIURI *aReferringURI, 
                                     nsIInputStream *aPostDataStream,                                     
                                     nsISupports *aOpenContext, 
                                     nsISupports **aCurrentOpenContext, 
                                     PRUint32 adapterBinding)
{
  // we need to create a DocumentOpenInfo object which will go ahead and open the url
  // and discover the content type....

  nsresult rv = NS_OK;
  nsDocumentOpenInfo* loader = nsnull;

  if (!aURI) return NS_ERROR_NULL_POINTER;

  NS_NEWXPCOM(loader, nsDocumentOpenInfo);
  if (!loader) return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(loader);
  loader->Init(aWindowContext);    // Extra Info

  // now instruct the loader to go ahead and open the url
  rv = loader->Open(aURI, aWindowTarget, aWindowContext,  
                    aReferringURI, aPostDataStream, aOpenContext, aCurrentOpenContext);
  NS_RELEASE(loader);

  return NS_OK;
}


nsresult nsURILoader::DispatchContent(const char * aContentType,
                                      const char * aCommand,
                                      const char * aWindowTarget,
                                      nsIChannel * aChannel, 
                                      nsISupports * aCtxt, 
                                      nsIURIContentListener * aContentListener,
                                      char ** aContentTypeToUse,
                                      nsIURIContentListener ** aContentListenerToUse)
{
  // okay, now we've discovered the content type. We need to do the following:
  // (1) Give our uri content listener first crack at handling this content type.  
  nsresult rv = NS_OK;

  nsCOMPtr<nsIURIContentListener> listenerToUse = aContentListener;
  // find a content handler that can and will handle the content
  PRBool canHandleContent = PR_FALSE;
  if (listenerToUse)
    listenerToUse->CanHandleContent(aContentType, aCommand, aWindowTarget, 
                                    aContentTypeToUse, 
                                    &canHandleContent);
  if (!canHandleContent) // if it can't handle the content, scan through the list of registered listeners
  {
     PRInt32 i = 0;
     // keep looping until we are told to abort or we get a content listener back
     for(i = 0; i < m_listeners->Count() && !canHandleContent; i++)
	   {
	      //nsIURIContentListener's aren't refcounted.
		    nsIURIContentListener * listener =(nsIURIContentListener*)m_listeners->ElementAt(i);
        if (listener)
         {
            rv = listener->CanHandleContent(aContentType, aCommand, aWindowTarget, 
                                            aContentTypeToUse,
                                            &canHandleContent);
            if (canHandleContent)
              listenerToUse = listener;
         }
    } // for loop
  } // if we can't handle the content


  if (canHandleContent && listenerToUse)
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

  nsCAutoString handlerProgID (NS_CONTENT_HANDLER_PROGID_PREFIX);
  handlerProgID += aContentType;
  
  nsCOMPtr<nsIContentHandler> aContentHandler;
  rv = nsComponentManager::CreateInstance(handlerProgID, nsnull, NS_GET_IID(nsIContentHandler), getter_AddRefs(aContentHandler));
  if (NS_SUCCEEDED(rv)) // we did indeed have a content handler for this type!! yippee...
    rv = aContentHandler->HandleContent(aContentType, aCommand, aWindowTarget, aChannel);
  return rv;
}

