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
#include "nsICachingChannel.h"
#include "nsIInterfaceRequestor.h"
#include "nsIIOService.h"
#include "nsILoadGroup.h"
#include "nsIProxyObjectManager.h"
#include "nsIServiceManager.h"
#include "nsIStreamListener.h"
#include "nsIURI.h"
#include "nsXPIDLString.h"
#include "nsCRT.h"

#include "netCore.h"

#include "imgCache.h"
#include "imgRequest.h"
#include "imgRequestProxy.h"

#include "ImageLogging.h"

#include "nsIComponentRegistrar.h"

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
      nsCOMPtr<nsISupportsCString> ss(do_QueryInterface(s));

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

#define SHOULD_RELOAD(flags) (flags & nsIRequest::LOAD_BYPASS_CACHE)
#define SHOULD_VALIDATE(flags) (flags & nsIRequest::VALIDATE_ALWAYS)
#define SHOULD_USE_CACHE(flags) (flags & nsIRequest::LOAD_FROM_CACHE)

inline int merge_flags(const nsLoadFlags& inFlags, nsLoadFlags& outFlags)
{
  if (SHOULD_RELOAD(inFlags))
    outFlags |= nsIRequest::LOAD_BYPASS_CACHE;
  else if (SHOULD_VALIDATE(inFlags))
     outFlags |= nsIRequest::VALIDATE_ALWAYS;
  else
    return 0;

  return 1;
}

/* imgIRequest loadImage (in nsIURI aURI, in nsIURI parentURI, in nsILoadGroup aLoadGroup, in imgIDecoderObserver aObserver, in nsISupports aCX, in nsLoadFlags aLoadFlags, in nsISupports cacheKey, in imgIRequest aRequest); */

NS_IMETHODIMP imgLoader::LoadImage(nsIURI *aURI, nsIURI *parentURI, nsILoadGroup *aLoadGroup,
                                   imgIDecoderObserver *aObserver, nsISupports *aCX, nsLoadFlags aLoadFlags,
                                   nsISupports *cacheKey, imgIRequest *aRequest, imgIRequest **_retval)
{
  NS_ASSERTION(aURI, "imgLoader::LoadImage -- NULL URI pointer");

  if (!aURI)
    return NS_ERROR_NULL_POINTER;

#if defined(PR_LOGGING)
  nsCAutoString spec;
  aURI->GetAsciiSpec(spec);
  LOG_SCOPE_WITH_PARAM(gImgLog, "imgLoader::LoadImage", "aURI", spec.get());
#endif

  imgRequest *request = nsnull;

  nsresult rv;

  // XXX For now ignore the cache key. We will need it in the future
  // for correctly dealing with image load requests that are a result
  // of post data.
  nsCOMPtr<nsICacheEntryDescriptor> entry;
  imgCache::Get(aURI, !SHOULD_USE_CACHE(aLoadFlags), 
                &request, getter_AddRefs(entry)); // addrefs request

  PRBool validateRequest = PR_FALSE;

  if (request && entry) {

    // request's null out their mCacheEntry when all proxy's are removed from them.
    // If we are about to add a new one back, go ahead and re-set the cache entry so
    // it can be used.
    if (!request->mCacheEntry)
      request->mCacheEntry = entry;

    nsLoadFlags ourFlags = 0;

    if (merge_flags(aLoadFlags, ourFlags)) {
      ourFlags |= nsIRequest::LOAD_BYPASS_CACHE;
    } else if (SHOULD_VALIDATE(aLoadFlags)) {
      ourFlags |= nsIRequest::VALIDATE_ALWAYS;
    } else if (SHOULD_USE_CACHE(aLoadFlags)) {
      ourFlags |= nsIRequest::LOAD_FROM_CACHE;
    } else if (aLoadGroup) {
      nsLoadFlags flags = 0;
      aLoadGroup->GetLoadFlags(&flags);
      if (!merge_flags(flags, ourFlags)) {

        nsCOMPtr<nsIRequest> r;
        aLoadGroup->GetDefaultLoadRequest(getter_AddRefs(r));
        if (r) {
          flags = 0;
          r->GetLoadFlags(&flags);
          merge_flags(flags, ourFlags);
        }
      }
    }

    // If the request's loadId is the same as the aCX, then it is ok to use this
    // one because it has already been validated.
    void *key = (void*)aCX;
    if (request->mLoadId != key) {
      if (SHOULD_RELOAD(ourFlags)) {
        entry->Doom(); // doom this thing.
        entry = nsnull;
        NS_RELEASE(request);
        request = nsnull;
      } else if (SHOULD_VALIDATE(ourFlags)) {
        validateRequest = PR_TRUE;
      }
    }
  }

  //
  // Get the current EventQueue...  This is used as a cacheId to prevent
  // sharing requests which are being loaded across multiple event queues...
  //
  nsCOMPtr<nsIEventQueueService> eventQService;
  nsCOMPtr<nsIEventQueue> activeQ;

  eventQService = do_GetService(NS_EVENTQUEUESERVICE_CONTRACTID, &rv);
  if (NS_FAILED(rv)) 
    return rv;

  rv = eventQService->ResolveEventQueue(NS_CURRENT_EVENTQ, getter_AddRefs(activeQ));
  if (NS_FAILED(rv))
    return rv;

  void *cacheId = activeQ.get();
  PRBool bCanCacheRequest = PR_TRUE;
  if (request) {
    if (!request->IsReusable(cacheId)) {
      //
      // The current request is still being loaded and lives on a different
      // event queue.
      //
      // Since its event queue is NOT active, do not reuse this imgRequest !!
      // Instead, force a new request to be created but DO NOT allow it to be
      // cached!
      //
      PR_LOG(gImgLog, PR_LOG_DEBUG,
             ("[this=%p] imgLoader::LoadImage -- DANGER!! Unable to use cached imgRequest [request=%p]\n", this, request));

      entry = nsnull;
      NS_RELEASE(request);

      bCanCacheRequest = PR_FALSE;
    }
  }

  if (request && validateRequest) {
    /* no request from the cache.  do a new load */
    LOG_SCOPE(gImgLog, "imgLoader::LoadImage |cache hit| must validate");

    // now we need to insert a new channel request object inbetween the real request and the proxy
    // that basically delays loading the image until it gets a 304 or figures out that this needs to
    // be a new request

    if (request->mValidator) {
      rv = CreateNewProxyForRequest(request, aLoadGroup, aObserver, aCX, aLoadFlags, aRequest, _retval);

      if (*_retval)
        request->mValidator->AddProxy(NS_STATIC_CAST(imgRequestProxy*, *_retval));

      return rv;

    } else {
      nsCOMPtr<nsIIOService> ioserv(do_GetService("@mozilla.org/network/io-service;1"));
      if (!ioserv) return NS_ERROR_FAILURE;

      nsCOMPtr<nsIChannel> newChannel;
      ioserv->NewChannelFromURI(aURI, getter_AddRefs(newChannel));
      if (!newChannel) return NS_ERROR_FAILURE;

      nsCOMPtr<nsICachingChannel> cacheChan(do_QueryInterface(newChannel));

      if (cacheChan) {

        if (aLoadGroup) {
          PRUint32 flags;
          aLoadGroup->GetLoadFlags(&flags);
          if (aLoadFlags & nsIRequest::LOAD_BACKGROUND)
            flags |= nsIRequest::LOAD_BACKGROUND;
          newChannel->SetLoadFlags(flags | nsIRequest::VALIDATE_ALWAYS);
        }

        rv = CreateNewProxyForRequest(request, aLoadGroup, aObserver, aCX, aLoadFlags, aRequest, _retval);

        httpValidateChecker *hvc = new httpValidateChecker(request, aCX);
        if (!hvc) {
          NS_RELEASE(request);
          return NS_ERROR_OUT_OF_MEMORY;
        }

        NS_ADDREF(hvc);
        request->mValidator = hvc;

        hvc->AddProxy(NS_STATIC_CAST(imgRequestProxy*, *_retval));

        nsresult asyncOpenResult = newChannel->AsyncOpen(NS_STATIC_CAST(nsIStreamListener *, hvc), nsnull);

        NS_RELEASE(hvc);

        NS_RELEASE(request);

        return asyncOpenResult;

      } else {
        // If it isn't caching channel, use the cached version.
        // XXX we should probably do something more intelligent for local files.
        validateRequest = PR_FALSE;
      }
    }
  } else if (!request) {
    /* no request from the cache.  do a new load */
    LOG_SCOPE(gImgLog, "imgLoader::LoadImage |cache miss|");

    nsCOMPtr<nsIIOService> ioserv(do_GetService("@mozilla.org/network/io-service;1"));
    if (!ioserv) return NS_ERROR_FAILURE;

    nsCOMPtr<nsIChannel> newChannel;
    ioserv->NewChannelFromURI(aURI, getter_AddRefs(newChannel));
    if (!newChannel) return NS_ERROR_FAILURE;

    nsCOMPtr<nsIHttpChannel> newHttpChannel = do_QueryInterface(newChannel);
    if (newHttpChannel) {
      newHttpChannel->SetDocumentURI(parentURI);
    }

    if (aLoadGroup) {
      PRUint32 flags;
      aLoadGroup->GetLoadFlags(&flags);
      if (aLoadFlags & nsIRequest::LOAD_BACKGROUND)
        flags |= nsIRequest::LOAD_BACKGROUND;
      newChannel->SetLoadFlags(flags);
    }

    NS_NEWXPCOM(request, imgRequest);
    if (!request) return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(request);

    PR_LOG(gImgLog, PR_LOG_DEBUG,
           ("[this=%p] imgLoader::LoadImage -- Created new imgRequest [request=%p]\n", this, request));

    // Add the new request into the imgCache if its cachable...
    if (bCanCacheRequest) {
      imgCache::Put(aURI, request, getter_AddRefs(entry));
    }

    request->Init(newChannel, entry, cacheId, aCX);


    // create the proxy listener
    ProxyListener *pl = new ProxyListener(NS_STATIC_CAST(nsIStreamListener *, request));
    if (!pl) {
      NS_RELEASE(request);
      return NS_ERROR_OUT_OF_MEMORY;
    }

    NS_ADDREF(pl);

    if (aLoadGroup) {
      // Get the notification callbacks from the load group and set them on the channel
      nsCOMPtr<nsIInterfaceRequestor> interfaceRequestor;
      aLoadGroup->GetNotificationCallbacks(getter_AddRefs(interfaceRequestor));
      if (interfaceRequestor)
        newChannel->SetNotificationCallbacks(interfaceRequestor);

      // set the referrer if this is an HTTP request
      nsCOMPtr<nsIHttpChannel> httpChannel(do_QueryInterface(newChannel));

      if (httpChannel) {
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
    }

    PR_LOG(gImgLog, PR_LOG_DEBUG,
           ("[this=%p] imgLoader::LoadImage -- Calling channel->AsyncOpen()\n", this));

    nsresult asyncOpenResult = newChannel->AsyncOpen(NS_STATIC_CAST(nsIStreamListener *, pl), nsnull);

    NS_RELEASE(pl);

    if (NS_FAILED(asyncOpenResult)) {
      /* If AsyncOpen fails, then we want to hand back a request proxy
         object that has a canceled load.
       */
      LOG_MSG(gImgLog, "imgLoader::LoadImage", "async open failed.");

      nsresult rv = CreateNewProxyForRequest(request, aLoadGroup, aObserver,
                                             aCX, aLoadFlags, aRequest, _retval);
      request->NotifyProxyListener(NS_STATIC_CAST(imgRequestProxy*, *_retval));

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

    // Update the request's LoadId
    request->SetLoadId(aCX);
  }

  LOG_MSG(gImgLog, "imgLoader::LoadImage", "creating proxy request.");

  rv = CreateNewProxyForRequest(request, aLoadGroup, aObserver, aCX, aLoadFlags, aRequest, _retval);

  if (!validateRequest) // if we have to validate the request, then we will send the notifications later.
    request->NotifyProxyListener(NS_STATIC_CAST(imgRequestProxy*, *_retval));

  NS_RELEASE(request);

  return rv;
}

/* imgIRequest loadImageWithChannel(in nsIChannel channel, in imgIDecoderObserver aObserver, in nsISupports cx, out nsIStreamListener); */
NS_IMETHODIMP imgLoader::LoadImageWithChannel(nsIChannel *channel, imgIDecoderObserver *aObserver, nsISupports *aCX, nsIStreamListener **listener, imgIRequest **_retval)
{
  NS_ASSERTION(channel, "imgLoader::LoadImageWithChannel -- NULL channel pointer");

  nsresult rv;
  imgRequest *request = nsnull;

  nsCOMPtr<nsIURI> uri;
  channel->GetOriginalURI(getter_AddRefs(uri));

  nsCOMPtr<nsICacheEntryDescriptor> entry;
  imgCache::Get(uri, PR_TRUE, &request, getter_AddRefs(entry)); // addrefs request

  nsCOMPtr<nsILoadGroup> loadGroup;
  channel->GetLoadGroup(getter_AddRefs(loadGroup));

  if (request) {
    // we have this in our cache already.. cancel the current (document) load

    /* XXX If |*listener| is null when we return here, the caller should 
       probably cancel the channel instead of us doing it here.
    */
    channel->Cancel(NS_BINDING_ABORTED); // this should fire an OnStopRequest

    *listener = nsnull; // give them back a null nsIStreamListener
  } else {
    //
    // Get the current EventQueue...  This is used as a cacheId to prevent
    // sharing requests which are being loaded across multiple event queues...
    //
    nsCOMPtr<nsIEventQueueService> eventQService;
    nsCOMPtr<nsIEventQueue> activeQ;

    eventQService = do_GetService(NS_EVENTQUEUESERVICE_CONTRACTID, &rv);
    if (NS_FAILED(rv)) 
      return rv;
        
    rv = eventQService->ResolveEventQueue(NS_CURRENT_EVENTQ, getter_AddRefs(activeQ));
    if (NS_FAILED(rv))
      return rv;

    NS_NEWXPCOM(request, imgRequest);
    if (!request) return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(request);

    imgCache::Put(uri, request, getter_AddRefs(entry));

    request->Init(channel, entry, activeQ.get(), aCX);

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

  rv = CreateNewProxyForRequest(request, loadGroup, aObserver, aCX, nsIRequest::LOAD_NORMAL, nsnull, _retval);
  request->NotifyProxyListener(NS_STATIC_CAST(imgRequestProxy*, *_retval));

  NS_RELEASE(request);

  return rv;
}


nsresult
imgLoader::CreateNewProxyForRequest(imgRequest *aRequest, nsILoadGroup *aLoadGroup,
                                    imgIDecoderObserver *aObserver, nsISupports *cx,
                                    nsLoadFlags aLoadFlags, imgIRequest *aProxyRequest,
                                    imgIRequest **_retval)
{
  LOG_SCOPE_WITH_PARAM(gImgLog, "imgLoader::CreateNewProxyForRequest", "imgRequest", aRequest);

  /* XXX If we move decoding onto seperate threads, we should save off the
     calling thread here and pass it off to |proxyRequest| so that it call
     proxy calls to |aObserver|.
   */

  imgRequestProxy *proxyRequest;
  if (aProxyRequest) {
    proxyRequest = NS_STATIC_CAST(imgRequestProxy *, aProxyRequest);
  } else {
    NS_NEWXPCOM(proxyRequest, imgRequestProxy);
    if (!proxyRequest) return NS_ERROR_OUT_OF_MEMORY;
  }
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

NS_IMETHODIMP imgLoader::SupportImageWithMimeType(const char* aMimeType, PRBool *_retval)
{
  *_retval = PR_FALSE;
  nsCOMPtr<nsIComponentRegistrar> reg;
  nsresult rv = NS_GetComponentRegistrar(getter_AddRefs(reg));
  if (NS_FAILED(rv))
    return rv;
  nsCAutoString mimeType(aMimeType);
  ToLowerCase(mimeType);
  nsCAutoString decoderId(NS_LITERAL_CSTRING("@mozilla.org/image/decoder;2?type=") + mimeType);
  return reg->IsContractIDRegistered(decoderId.get(),  _retval);
}

NS_IMETHODIMP imgLoader::SupportImageWithContents(const char* aContents, PRUint32 aLength, char** aContentType)
{
  return GetMimeTypeFromContent(aContents, aLength, aContentType);
}

/* static */
nsresult imgLoader::GetMimeTypeFromContent(const char* aContents, PRUint32 aLength, char** aContentType)
{
  *aContentType = nsnull;
  /* Is it a GIF? */
  if (aLength >= 4 && !nsCRT::strncmp(aContents, "GIF8", 4))  {
    *aContentType = nsCRT::strndup("image/gif", 9);
  }

  /* or a PNG? */
  else if (aLength >= 4 && ((unsigned char)aContents[0]==0x89 &&
                   (unsigned char)aContents[1]==0x50 &&
                   (unsigned char)aContents[2]==0x4E &&
                   (unsigned char)aContents[3]==0x47))
  { 
    *aContentType = nsCRT::strndup("image/png", 9);
  }

  /* maybe a JPEG (JFIF)? */
  /* JFIF files start with SOI APP0 but older files can start with SOI DQT
   * so we test for SOI followed by any marker, i.e. FF D8 FF
   * this will also work for SPIFF JPEG files if they appear in the future.
   *
   * (JFIF is 0XFF 0XD8 0XFF 0XE0 <skip 2> 0X4A 0X46 0X49 0X46 0X00)
   */
  else if (aLength >= 3 &&
     ((unsigned char)aContents[0])==0xFF &&
     ((unsigned char)aContents[1])==0xD8 &&
     ((unsigned char)aContents[2])==0xFF)
  {
    *aContentType = nsCRT::strndup("image/jpeg", 10);
  }

  /* or how about ART? */
  /* ART begins with JG (4A 47). Major version offset 2.
   * Minor version offset 3. Offset 4 must be NULL.
   */
  else if (aLength >= 5 &&
   ((unsigned char) aContents[0])==0x4a &&
   ((unsigned char) aContents[1])==0x47 &&
   ((unsigned char) aContents[4])==0x00 )
  {
    *aContentType = nsCRT::strndup("image/x-jg", 10);
  }

  else if (aLength >= 2 && !nsCRT::strncmp(aContents, "BM", 2)) {
    *aContentType = nsCRT::strndup("image/bmp", 9);
  }

  // ICOs always begin with a 2-byte 0 followed by a 2-byte 1.
  else if (aLength >= 4 && !memcmp(aContents, "\000\000\001\000", 4)) {
    *aContentType = nsCRT::strndup("image/x-icon", 12);
  }

  else if (aLength >= 4 && ((unsigned char)aContents[0]==0x8A &&
                   (unsigned char)aContents[1]==0x4D &&
                   (unsigned char)aContents[2]==0x4E &&
                   (unsigned char)aContents[3]==0x47))
  { 
    *aContentType = nsCRT::strndup("video/x-mng", 11);
  }

  else if (aLength >= 4 && ((unsigned char)aContents[0]==0x8B &&
                   (unsigned char)aContents[1]==0x4A &&
                   (unsigned char)aContents[2]==0x4E &&
                   (unsigned char)aContents[3]==0x47))
  { 
    *aContentType = nsCRT::strndup("image/x-jng", 11);
  }

  else if (aLength >= 8 && !nsCRT::strncmp(aContents, "#define ", 8)) {
    *aContentType = nsCRT::strndup("image/x-xbitmap", 15);
  }
  /* PBM, PGM or PPM? */
  /* These start with the letter 'P' followed by a digit from 1 through 6
   * followed by a whitespace character.
   */
  else if (aLength >= 3 && ((unsigned char)aContents[0]==0x50 &&
                            ((unsigned char)aContents[2]==0x9 || (unsigned char)aContents[2]==0xa ||
                             (unsigned char)aContents[2]==0xd || (unsigned char)aContents[2]==0x20)))
  {
    unsigned char c = (unsigned char)aContents[1];
    if (c == '1' || c == '4') {
      *aContentType = nsCRT::strndup("image/x-portable-bitmap", 23);
    } else if (c == '2' || c == '5') {
      *aContentType = nsCRT::strndup("image/x-portable-graymap", 24);
    } else if (c == '3' || c == '6') {
      *aContentType = nsCRT::strndup("image/x-portable-pixmap", 23);
    }
  }
  else {
    /* none of the above?  I give up */
    /* don't raise an exception, simply return null */
    return NS_OK;
  }

  if (*aContentType)
    return NS_OK;

  return NS_ERROR_OUT_OF_MEMORY;
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
    nsCAutoString contentType;
    nsresult rv = channel->GetContentType(contentType);

    if (!contentType.IsEmpty()) {
     /* If multipart/x-mixed-replace content, we'll insert a MIME decoder
        in the pipeline to handle the content and pass it along to our
        original listener.
      */
      if (NS_LITERAL_CSTRING("multipart/x-mixed-replace").Equals(contentType)) {

        nsCOMPtr<nsIStreamConverterService> convServ(do_GetService("@mozilla.org/streamConverters;1", &rv));
        if (NS_SUCCEEDED(rv)) {
          nsCOMPtr<nsIStreamListener> toListener(mDestListener);
          nsCOMPtr<nsIStreamListener> fromListener;

          rv = convServ->AsyncConvertData(NS_LITERAL_STRING("multipart/x-mixed-replace").get(),
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





/**
 * http validate class.  check a channel for a 304
 */

NS_IMPL_ISUPPORTS2(httpValidateChecker, nsIStreamListener, nsIRequestObserver)

httpValidateChecker::httpValidateChecker(imgRequest *request, void *aContext) :
  mContext(aContext)
{
  NS_INIT_ISUPPORTS();
  /* member initializers and constructor code */

  mRequest = request;
  NS_ADDREF(mRequest);
}

httpValidateChecker::~httpValidateChecker()
{
  /* destructor code */
  NS_IF_RELEASE(mRequest);
}

void httpValidateChecker::AddProxy(imgRequestProxy *aProxy)
{
  mProxies.AppendElement(aProxy);
}

/** nsIRequestObserver methods **/

/* void onStartRequest (in nsIRequest request, in nsISupports ctxt); */
NS_IMETHODIMP httpValidateChecker::OnStartRequest(nsIRequest *aRequest, nsISupports *ctxt)
{
  nsCOMPtr<nsICachingChannel> cacheChan(do_QueryInterface(aRequest));
  if (cacheChan) {
    PRBool isFromCache = PR_FALSE;
    cacheChan->IsFromCache(&isFromCache);

    if (isFromCache) {

      PRUint32 count;
      mProxies.Count(&count);
      for (PRInt32 i = count-1; i>=0; i--) {
        imgRequestProxy *proxy;
        mProxies.GetElementAt(i, (nsISupports**)&proxy);
        mRequest->NotifyProxyListener(proxy);
        NS_RELEASE(proxy);
      }

      mRequest->SetLoadId(mContext);

      // XXX see bug 113959
      // Don't call cancel here because it makes the HTTP cache entry go away.
      //      aRequest->Cancel(NS_BINDING_ABORTED);

      mRequest->mValidator = nsnull;

      NS_RELEASE(mRequest);
      mRequest = nsnull;

      return NS_OK;
      // we're set.  do nothing.
    } else {
      // fun stuff.
      nsCOMPtr<nsIChannel> channel(do_QueryInterface(aRequest));
      nsCOMPtr<nsICacheEntryDescriptor> entry;
      nsCOMPtr<nsIURI> uri;

      // Doom the old request's cache entry
      if (mRequest->mCacheEntry)
        mRequest->mCacheEntry->Doom();

      mRequest->GetURI(getter_AddRefs(uri));

      mRequest->mValidator = nsnull;
      NS_RELEASE(mRequest);
      mRequest = nsnull;

      nsresult rv;
      nsCOMPtr<nsIEventQueueService> eventQService = do_GetService(NS_EVENTQUEUESERVICE_CONTRACTID, &rv);
      if (NS_FAILED(rv)) return rv;

      nsCOMPtr<nsIEventQueue> activeQ;
      rv = eventQService->ResolveEventQueue(NS_CURRENT_EVENTQ, getter_AddRefs(activeQ));
      if (NS_FAILED(rv)) return rv;

      imgRequest *request;
      NS_NEWXPCOM(request, imgRequest);
      if (!request) return NS_ERROR_OUT_OF_MEMORY;
      NS_ADDREF(request);

      imgCache::Put(uri, request, getter_AddRefs(entry));

      request->Init(channel, entry, activeQ.get(), mContext);

      ProxyListener *pl = new ProxyListener(NS_STATIC_CAST(nsIStreamListener *, request));
      if (!pl) {
        NS_RELEASE(request);
        return NS_ERROR_OUT_OF_MEMORY;
      }

      mDestListener = NS_STATIC_CAST(nsIStreamListener*, pl);

      PRUint32 count;
      mProxies.Count(&count);
      for (PRInt32 i = count-1; i>=0; i--) {
        imgRequestProxy *proxy;
        mProxies.GetElementAt(i, (nsISupports**)&proxy);
        proxy->ChangeOwner(request);
        request->NotifyProxyListener(proxy);
        NS_RELEASE(proxy);
      }

      NS_RELEASE(request);
    }
  }

  if (!mDestListener)
    return NS_OK;

  return mDestListener->OnStartRequest(aRequest, ctxt);
}

/* void onStopRequest (in nsIRequest request, in nsISupports ctxt, in nsresult status); */
NS_IMETHODIMP httpValidateChecker::OnStopRequest(nsIRequest *aRequest, nsISupports *ctxt, nsresult status)
{
  if (!mDestListener)
    return NS_OK;

  return mDestListener->OnStopRequest(aRequest, ctxt, status);
}

/** nsIStreamListener methods **/


// XXX see bug 113959
static NS_METHOD dispose_of_data(nsIInputStream* in, void* closure,
                                 const char* fromRawSegment, PRUint32 toOffset,
                                 PRUint32 count, PRUint32 *writeCount)
{
  *writeCount = count;
  return NS_OK;
}

/* void onDataAvailable (in nsIRequest request, in nsISupports ctxt, in nsIInputStream inStr, in unsigned long sourceOffset, in unsigned long count); */
NS_IMETHODIMP httpValidateChecker::OnDataAvailable(nsIRequest *aRequest, nsISupports *ctxt, nsIInputStream *inStr, PRUint32 sourceOffset, PRUint32 count)
{
  if (!mDestListener) {
    // XXX see bug 113959
    PRUint32 _retval;
    inStr->ReadSegments(dispose_of_data, nsnull, count, &_retval);
    return NS_OK;
  }

  return mDestListener->OnDataAvailable(aRequest, ctxt, inStr, sourceOffset, count);
}
