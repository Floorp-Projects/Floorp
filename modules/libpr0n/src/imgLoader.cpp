/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
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
 * Copyright (C) 2001 Netscape Communications Corporation.
 * All Rights Reserved.
 * 
 * Contributor(s):
 *   Stuart Parmenter <pavlov@netscape.com>
 */


#include "imgLoader.h"

#include "nsCOMPtr.h"

#include "nsIChannel.h"
#include "nsIHttpChannel.h"
#include "nsIIOService.h"
#include "nsILoadGroup.h"
#include "nsIProxyObjectManager.h"
#include "nsIServiceManager.h"
#include "nsIStreamListener.h"
#include "nsIURI.h"
#include "nsXPIDLString.h"

#include "imgCache.h"
#include "imgRequest.h"
#include "imgRequestProxy.h"

#include "ImageLogging.h"

#include "nsMimeTypes.h"

#ifdef DEBUG_pavlov
#include "nsIEnumerator.h"
#include "nsISupportsPrimitives.h"
#include "nsXPIDLString.h"
#include "nsComponentManagerUtils.h"

static void PrintImageDecoders()
{
  nsCOMPtr<nsIEnumerator> enumer;
  nsComponentManager::EnumerateContractIDs(getter_AddRefs(enumer));

  nsCString str;
  nsCOMPtr<nsISupports> s;
  do {
    enumer->CurrentItem(getter_AddRefs(s));
    if (s) {
      nsCOMPtr<nsISupportsString> ss(do_QueryInterface(s));

      nsXPIDLCString xcs;
      ss->GetData(getter_Copies(xcs));

      NS_NAMED_LITERAL_CSTRING(decoderContract, "@mozilla.org/image/decoder;2?type=");

      if (Substring(xcs, 0, decoderContract.Length()).Equals(decoderContract)) {
        printf("Have decoder for mime type: %s\n", xcs.get()+decoderContract.Length());
      }
    }
  } while(NS_SUCCEEDED(enumer->Next()));
}
#endif

NS_IMPL_ISUPPORTS1(imgLoader, imgILoader)

imgLoader::imgLoader()
{
  NS_INIT_ISUPPORTS();
  /* member initializers and constructor code */
#ifdef DEBUG_pavlov
  PrintImageDecoders();
#endif
}

imgLoader::~imgLoader()
{
  /* destructor code */
}

#define SHOULD_RELOAD(flags) (flags & nsIRequest::LOAD_BYPASS_CACHE || flags & nsIRequest::VALIDATE_ALWAYS)

/* imgIRequest loadImage (in nsIURI aURI, in nsILoadGroup aLoadGroup, in imgIDecoderObserver aObserver, in nsISupports aCX, in nsLoadFlags aLoadFlags); */
NS_IMETHODIMP imgLoader::LoadImage(nsIURI *aURI, nsILoadGroup *aLoadGroup, imgIDecoderObserver *aObserver, nsISupports *aCX, nsLoadFlags aLoadFlags, imgIRequest **_retval)
{
  NS_ASSERTION(aURI, "imgLoader::LoadImage -- NULL URI pointer");

  if (!aURI)
    return NS_ERROR_NULL_POINTER;

#if defined(PR_LOGGING)
  nsXPIDLCString spec;
  aURI->GetSpec(getter_Copies(spec));
  LOG_SCOPE_WITH_PARAM(gImgLog, "imgLoader::LoadImage", "aURI", spec.get());
#endif

  imgRequest *request = nsnull;

  nsCOMPtr<nsICacheEntryDescriptor> entry;
  imgCache::Get(aURI, &request, getter_AddRefs(entry)); // addrefs request

  if (request && entry) {
    /* this isn't exactly what I want here.  This code will re-doom every
       cache hit in a document while it is force reloading.  So for multiple
       copies of an image on a page, when you force reload, this will cause
       you to get seperate loads for each copy of the image... this sucks.
    */
    PRBool doomRequest = PR_FALSE;

    if (SHOULD_RELOAD(aLoadFlags)) {
      doomRequest = PR_TRUE;
    } else if (aLoadGroup) {
      nsLoadFlags flags = 0;
      aLoadGroup->GetLoadFlags(&flags);
      if (SHOULD_RELOAD(flags)) {
        doomRequest = PR_TRUE;
      } else {
        nsCOMPtr<nsIRequest> r;
        aLoadGroup->GetDefaultLoadRequest(getter_AddRefs(r));
        if (r) {
          flags = 0;
          r->GetLoadFlags(&flags);
          if (SHOULD_RELOAD(flags))
            doomRequest = PR_TRUE;
        }
      }
    }

    if (doomRequest) {
      entry->Doom(); // doom this thing.
      entry = nsnull;
      NS_RELEASE(request);
      request = nsnull;
    }
  }

  if (!request) {
    /* no request from the cache.  do a new load */
    LOG_SCOPE(gImgLog, "imgLoader::LoadImage |cache miss|");

    nsCOMPtr<nsIIOService> ioserv(do_GetService("@mozilla.org/network/io-service;1"));
    if (!ioserv) return NS_ERROR_FAILURE;

    nsCOMPtr<nsIChannel> newChannel;
    ioserv->NewChannelFromURI(aURI, getter_AddRefs(newChannel));
    if (!newChannel) return NS_ERROR_FAILURE;

    if (aLoadGroup) {
      PRUint32 flags;
      aLoadGroup->GetLoadFlags(&flags);
      newChannel->SetLoadFlags(flags);
    }

    NS_NEWXPCOM(request, imgRequest);
    if (!request) return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(request);

    PR_LOG(gImgLog, PR_LOG_DEBUG,
           ("[this=%p] imgLoader::LoadImage -- Created new imgRequest [request=%p]\n", this, request));

    imgCache::Put(aURI, request, getter_AddRefs(entry));

    request->Init(newChannel, entry);

    PR_LOG(gImgLog, PR_LOG_DEBUG,
           ("[this=%p] imgLoader::LoadImage -- Calling channel->AsyncOpen()\n", this));

    // create the proxy listener
    ProxyListener *pl = new ProxyListener(NS_STATIC_CAST(nsIStreamListener *, request));
    if (!pl) {
      NS_RELEASE(request);
      return NS_ERROR_OUT_OF_MEMORY;
    }

    NS_ADDREF(pl);

    // set the referrer if this is an HTTP request
    nsCOMPtr<nsIHttpChannel> httpChannel(do_QueryInterface(newChannel));
    if (aLoadGroup && httpChannel) {
      nsresult rv;
      // Get the defloadRequest from the loadgroup
      nsCOMPtr<nsIRequest> defLoadRequest;
      rv = aLoadGroup->GetDefaultLoadRequest(getter_AddRefs(defLoadRequest));

      if (NS_SUCCEEDED(rv) && defLoadRequest) {
        nsCOMPtr<nsIChannel> reqChannel(do_QueryInterface(defLoadRequest));

        if (reqChannel) {
          // Get the referrer from the loadchannel
          nsCOMPtr<nsIURI> referrer;
          rv = reqChannel->GetURI(getter_AddRefs(referrer));
          if (NS_SUCCEEDED(rv)) {
            // Set the referrer
            httpChannel->SetReferrer(referrer, nsIHttpChannel::REFERRER_INLINES);
          }   
        }
      }
    }

    /* XXX Are we calling AsyncOpen() too early?  Is it possible for
       AsyncOpen to result in an OnStartRequest to the channel before
       we call CreateNewProxyForRequest() ?
     */
    nsresult asyncOpenResult = newChannel->AsyncOpen(NS_STATIC_CAST(nsIStreamListener *, pl), nsnull);

    NS_RELEASE(pl);

    if (NS_FAILED(asyncOpenResult)) {
      /* If AsyncOpen fails, then we want to hand back a request proxy
         object that has a canceled load.
       */
      LOG_MSG(gImgLog, "imgLoader::LoadImage", "async open failed.");

      nsresult rv = CreateNewProxyForRequest(request, aLoadGroup, aObserver,
                                             aCX, aLoadFlags, _retval);
      if (NS_SUCCEEDED(rv)) {
        request->OnStartRequest(newChannel, nsnull);
        request->OnStopRequest(newChannel, nsnull, NS_BINDING_ABORTED);
      }

      NS_RELEASE(request);
      
      return asyncOpenResult;
    }

  } else {
    /* request found in cache.  use it */
    LOG_MSG_WITH_PARAM(gImgLog, "imgLoader::LoadImage |cache hit|", "request", request);
  }

  LOG_MSG(gImgLog, "imgLoader::LoadImage", "creating proxy request.");

  nsresult rv = CreateNewProxyForRequest(request, aLoadGroup, aObserver, aCX, aLoadFlags, _retval);

  NS_RELEASE(request);

  return rv;
}

#undef SHOULD_RELOAD

/* imgIRequest loadImageWithChannel(in nsIChannel, in imgIDecoderObserver aObserver, in nsISupports cx, out nsIStreamListener); */
NS_IMETHODIMP imgLoader::LoadImageWithChannel(nsIChannel *channel, imgIDecoderObserver *aObserver, nsISupports *cx, nsIStreamListener **listener, imgIRequest **_retval)
{
  NS_ASSERTION(channel, "imgLoader::LoadImageWithChannel -- NULL channel pointer");

  imgRequest *request = nsnull;

  nsCOMPtr<nsIURI> uri;
  channel->GetOriginalURI(getter_AddRefs(uri));

  nsCOMPtr<nsICacheEntryDescriptor> entry;
  imgCache::Get(uri, &request, getter_AddRefs(entry)); // addrefs request

  if (request) {
    // we have this in our cache already.. cancel the current (document) load

    /* XXX If |*listener| is null when we return here, the caller should 
       probably cancel the channel instead of us doing it here.
    */
    channel->Cancel(NS_BINDING_ABORTED); // this should fire an OnStopRequest

    *listener = nsnull; // give them back a null nsIStreamListener
  } else {
    NS_NEWXPCOM(request, imgRequest);
    if (!request) return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(request);

    imgCache::Put(uri, request, getter_AddRefs(entry));

    request->Init(channel, entry);

    ProxyListener *pl = new ProxyListener(NS_STATIC_CAST(nsIStreamListener *, request));
    if (!pl) {
      NS_RELEASE(request);
      return NS_ERROR_OUT_OF_MEMORY;
    }

    NS_ADDREF(pl);

    *listener = NS_STATIC_CAST(nsIStreamListener*, pl);
    NS_ADDREF(*listener);

    NS_RELEASE(pl);
  }

  nsCOMPtr<nsILoadGroup> loadGroup;
  channel->GetLoadGroup(getter_AddRefs(loadGroup));

  nsresult rv = CreateNewProxyForRequest(request, loadGroup, aObserver, cx, nsIRequest::LOAD_NORMAL, _retval);

  NS_RELEASE(request);

  return rv;
}



nsresult
imgLoader::CreateNewProxyForRequest(imgRequest *aRequest, nsILoadGroup *aLoadGroup,
                                    imgIDecoderObserver *aObserver, nsISupports *cx,
                                    nsLoadFlags aLoadFlags, imgIRequest **_retval)
{
  LOG_SCOPE_WITH_PARAM(gImgLog, "imgLoader::CreateNewProxyForRequest", "imgRequest", aRequest);

  /* XXX If we move decoding onto seperate threads, we should save off the
     calling thread here and pass it off to |proxyRequest| so that it call
     proxy calls to |aObserver|.
   */

  imgRequestProxy *proxyRequest;
  NS_NEWXPCOM(proxyRequest, imgRequestProxy);
  if (!proxyRequest) return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(proxyRequest);

  /* It is important to call |SetLoadFlags()| before calling |Init()| because
     |Init()| adds the request to the loadgroup.
   */
  proxyRequest->SetLoadFlags(aLoadFlags);

  // init adds itself to imgRequest's list of observers
  nsresult rv = proxyRequest->Init(aRequest, aLoadGroup, aObserver, cx);
  if (NS_FAILED(rv)) {
    NS_RELEASE(proxyRequest);
    return rv;
  }

  *_retval = NS_STATIC_CAST(imgIRequest*, proxyRequest);
  NS_ADDREF(*_retval);

  NS_RELEASE(proxyRequest);

  return NS_OK;
}



/**
 * proxy stream listener class used to handle multipart/x-mixed-replace
 */

#include "nsIRequest.h"
#include "nsIStreamConverterService.h"
#include "nsXPIDLString.h"

NS_IMPL_ISUPPORTS2(ProxyListener, nsIStreamListener, nsIRequestObserver)

ProxyListener::ProxyListener(nsIStreamListener *dest) :
  mDestListener(dest)
{
  NS_INIT_ISUPPORTS();
  /* member initializers and constructor code */
}

ProxyListener::~ProxyListener()
{
  /* destructor code */
}


/** nsIRequestObserver methods **/

/* void onStartRequest (in nsIRequest request, in nsISupports ctxt); */
NS_IMETHODIMP ProxyListener::OnStartRequest(nsIRequest *aRequest, nsISupports *ctxt)
{
  if (!mDestListener)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIChannel> channel(do_QueryInterface(aRequest));
  if (channel) {
    nsXPIDLCString contentType;
    nsresult rv = channel->GetContentType(getter_Copies(contentType));

    if (contentType.get()) {
     /* If multipart/x-mixed-replace content, we'll insert a MIME decoder
        in the pipeline to handle the content and pass it along to our
        original listener.
      */
      if (NS_LITERAL_CSTRING(MULTIPART_MIXED_REPLACE).Equals(contentType)) {

        nsCOMPtr<nsIStreamConverterService> convServ(do_GetService("@mozilla.org/streamConverters;1", &rv));
        if (NS_SUCCEEDED(rv)) {
          nsCOMPtr<nsIStreamListener> toListener(mDestListener);
          nsCOMPtr<nsIStreamListener> fromListener;

          rv = convServ->AsyncConvertData(NS_LITERAL_STRING(MULTIPART_MIXED_REPLACE).get(),
                                          NS_LITERAL_STRING("*/*").get(),
                                          toListener,
                                          nsnull,
                                          getter_AddRefs(fromListener));
          if (NS_SUCCEEDED(rv))
            mDestListener = fromListener;
        }
      }
    }
  }

  return mDestListener->OnStartRequest(aRequest, ctxt);
}

/* void onStopRequest (in nsIRequest request, in nsISupports ctxt, in nsresult status); */
NS_IMETHODIMP ProxyListener::OnStopRequest(nsIRequest *aRequest, nsISupports *ctxt, nsresult status)
{
  if (!mDestListener)
    return NS_ERROR_FAILURE;

  return mDestListener->OnStopRequest(aRequest, ctxt, status);
}

/** nsIStreamListener methods **/

/* void onDataAvailable (in nsIRequest request, in nsISupports ctxt, in nsIInputStream inStr, in unsigned long sourceOffset, in unsigned long count); */
NS_IMETHODIMP ProxyListener::OnDataAvailable(nsIRequest *aRequest, nsISupports *ctxt, nsIInputStream *inStr, PRUint32 sourceOffset, PRUint32 count)
{
  if (!mDestListener)
    return NS_ERROR_FAILURE;

  return mDestListener->OnDataAvailable(aRequest, ctxt, inStr, sourceOffset, count);
}
