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

#include "nsNetUtil.h"
#include "nsIHttpChannel.h"
#include "nsICachingChannel.h"
#include "nsIProxyObjectManager.h"
#include "nsIServiceManager.h"
#include "nsXPIDLString.h"
#include "nsCRT.h"

#include "netCore.h"

#include "imgCache.h"
#include "imgRequest.h"
#include "imgRequestProxy.h"

#include "ImageLogging.h"

#include "nsIComponentRegistrar.h"

// we want to explore making the document own the load group
// so we can associate the document URI with the load group.
// until this point, we have an evil hack:
#include "nsIHttpChannelInternal.h"  

#if defined(DEBUG_pavlov) || defined(DEBUG_timeless)
#include "nsISimpleEnumerator.h"
#include "nsXPCOM.h"
#include "nsISupportsPrimitives.h"
#include "nsXPIDLString.h"
#include "nsComponentManagerUtils.h"

static void PrintImageDecoders()
{
  nsCOMPtr<nsIComponentRegistrar> compMgr;
  if (NS_FAILED(NS_GetComponentRegistrar(getter_AddRefs(compMgr))) || !compMgr)
    return;
  nsCOMPtr<nsISimpleEnumerator> enumer;
  if (NS_FAILED(compMgr->EnumerateContractIDs(getter_AddRefs(enumer))) || !enumer)
    return;
  
  nsCString str;
  nsCOMPtr<nsISupports> s;
  PRBool more = PR_FALSE;
  while (NS_SUCCEEDED(enumer->HasMoreElements(&more)) && more) {
    enumer->GetNext(getter_AddRefs(s));
    if (s) {
      nsCOMPtr<nsISupportsCString> ss(do_QueryInterface(s));

      nsCAutoString xcs;
      ss->GetData(xcs);

      NS_NAMED_LITERAL_CSTRING(decoderContract, "@mozilla.org/image/decoder;2?type=");

      if (Substring(xcs, 0, decoderContract.Length()).Equals(decoderContract)) {
        printf("Have decoder for mime type: %s\n", xcs.get()+decoderContract.Length());
      }
    }
  }
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

#define LOAD_FLAGS_CACHE_MASK    (nsIRequest::LOAD_BYPASS_CACHE | \
                                  nsIRequest::LOAD_FROM_CACHE)

#define LOAD_FLAGS_VALIDATE_MASK (nsIRequest::VALIDATE_ALWAYS |   \
                                  nsIRequest::VALIDATE_NEVER |    \
                                  nsIRequest::VALIDATE_ONCE_PER_SESSION)


static PRBool RevalidateEntry(nsICacheEntryDescriptor *aEntry,
                              nsLoadFlags aFlags,
                              PRBool aHasExpired)
{
  PRBool bValidateEntry = PR_FALSE;

  NS_ASSERTION(!(aFlags & nsIRequest::LOAD_BYPASS_CACHE),
               "MUST not revalidate when BYPASS_CACHE is specified.");

  if (aFlags & nsIRequest::VALIDATE_ALWAYS) {
    bValidateEntry = PR_TRUE;
  }
  //
  // The cache entry has expired...  Determine whether the stale cache
  // entry can be used without validation...
  //
  else if (aHasExpired) {
    //
    // VALIDATE_NEVER and VALIDATE_ONCE_PER_SESSION allow stale cache
    // entries to be used unless they have been explicitly marked to
    // indicate that revalidation is necessary.
    //
    if (aFlags & (nsIRequest::VALIDATE_NEVER | 
                  nsIRequest::VALIDATE_ONCE_PER_SESSION)) 
    {
      nsXPIDLCString value;

      aEntry->GetMetaDataElement("MustValidateIfExpired",
                                 getter_Copies(value));
      if (PL_strcmp(value, "true")) {
        bValidateEntry = PR_TRUE;
      }
    }
    //
    // LOAD_FROM_CACHE allows a stale cache entry to be used... Otherwise,
    // the entry must be revalidated.
    //
    else if (!(aFlags & nsIRequest::LOAD_FROM_CACHE)) {
      bValidateEntry = PR_TRUE;
    }
  }

  return bValidateEntry;
}


static nsresult NewImageChannel(nsIChannel **aResult,
                                nsIURI *aURI,
                                nsIURI *aInitialDocumentURI,
                                nsIURI *aReferringURI,
                                nsILoadGroup *aLoadGroup, nsLoadFlags aLoadFlags)
{
  nsresult rv;
  nsCOMPtr<nsIChannel> newChannel;
  nsCOMPtr<nsIHttpChannel> newHttpChannel;
 
  nsCOMPtr<nsIInterfaceRequestor> callbacks;

  if (aLoadGroup) {
    // Get the notification callbacks from the load group for the new channel.
    //
    // XXX: This is not exactly correct, because the network request could be
    //      referenced by multiple windows...  However, the new channel needs
    //      something.  So, using the 'first' notification callbacks is better
    //      than nothing...
    //
    aLoadGroup->GetNotificationCallbacks(getter_AddRefs(callbacks));
  }

  // Pass in a NULL loadgroup because this is the underlying network request.
  // This request may be referenced by several proxy image requests (psossibly
  // in different documents).
  // If all of the proxy requests are canceled then this request should be
  // canceled too.
  //
  rv = NS_NewChannel(aResult,
                     aURI,        // URI 
                     nsnull,      // Cached IOService
                     nsnull,      // LoadGroup
                     callbacks,   // Notification Callbacks
                     aLoadFlags);
  if (NS_FAILED(rv))
    return rv;

  // Initialize HTTP-specific attributes
  newHttpChannel = do_QueryInterface(*aResult);
  if (newHttpChannel) {
    newHttpChannel->SetRequestHeader(NS_LITERAL_CSTRING("Accept"),
                                     NS_LITERAL_CSTRING("video/x-mng,image/png,image/jpeg,image/gif;q=0.2,*/*;q=0.1"),
                                     PR_FALSE);

    nsCOMPtr<nsIHttpChannelInternal> httpChannelInternal = do_QueryInterface(newHttpChannel);
    NS_ENSURE_TRUE(httpChannelInternal, NS_ERROR_UNEXPECTED);
    httpChannelInternal->SetDocumentURI(aInitialDocumentURI);
    newHttpChannel->SetReferrer(aReferringURI);
  }

  return NS_OK;
}

/* imgIRequest loadImage (in nsIURI aURI, in nsIURI initialDocumentURI, in nsILoadGroup aLoadGroup, in imgIDecoderObserver aObserver, in nsISupports aCX, in nsLoadFlags aLoadFlags, in nsISupports cacheKey, in imgIRequest aRequest); */

NS_IMETHODIMP imgLoader::LoadImage(nsIURI *aURI, 
                                   nsIURI *aInitialDocumentURI,
                                   nsIURI *aReferrerURI,
                                   nsILoadGroup *aLoadGroup,
                                   imgIDecoderObserver *aObserver,
                                   nsISupports *aCX,
                                   nsLoadFlags aLoadFlags,
                                   nsISupports *cacheKey,
                                   imgIRequest *aRequest,
                                   imgIRequest **_retval)
{
  NS_ASSERTION(aURI, "imgLoader::LoadImage -- NULL URI pointer");

  if (!aURI)
    return NS_ERROR_NULL_POINTER;

#if defined(PR_LOGGING)
  nsCAutoString spec;
  aURI->GetAsciiSpec(spec);
  LOG_SCOPE_WITH_PARAM(gImgLog, "imgLoader::LoadImage", "aURI", spec.get());
#endif

  // This is an owning reference that must be released.
  imgRequest *request = nsnull;

  nsresult rv;
  nsLoadFlags requestFlags = nsIRequest::LOAD_NORMAL;

  // Get the default load flags from the loadgroup (if possible)...
  if (aLoadGroup) {
    aLoadGroup->GetLoadFlags(&requestFlags);
  }
  //
  // Merge the default load flags with those passed in via aLoadFlags.
  // Currently, *only* the caching, validation and background load flags
  // are merged...
  //
  // The flags in aLoadFlags take precidence over the default flags!
  //
  if (aLoadFlags & LOAD_FLAGS_CACHE_MASK) {
    // Override the default caching flags...
    requestFlags = (requestFlags & ~LOAD_FLAGS_CACHE_MASK) |
                   (aLoadFlags & LOAD_FLAGS_CACHE_MASK);
  }
  if (aLoadFlags & LOAD_FLAGS_VALIDATE_MASK) {
    // Override the default validation flags...
    requestFlags = (requestFlags & ~LOAD_FLAGS_VALIDATE_MASK) |
                   (aLoadFlags & LOAD_FLAGS_VALIDATE_MASK);
  }
  if (aLoadFlags & nsIRequest::LOAD_BACKGROUND) {
    // Propagate background loading...
    requestFlags |= nsIRequest::LOAD_BACKGROUND;
  }

  nsCOMPtr<nsICacheEntryDescriptor> entry;
  PRBool bCanCacheRequest = PR_TRUE;
  PRBool bHasExpired      = PR_FALSE;
  PRBool bValidateRequest = PR_FALSE;

  // XXX For now ignore the cache key. We will need it in the future
  // for correctly dealing with image load requests that are a result
  // of post data.
  imgCache::Get(aURI, &bHasExpired,
                &request, getter_AddRefs(entry)); // addrefs request

  if (request && entry) {

    // request's null out their mCacheEntry when all proxy's are removed.
    // If we are about to add a new one back, go ahead and re-set the cache
    // entry so it can be used.
    if (!request->mCacheEntry) {
      request->mCacheEntry = entry;
    }

    // If the request's loadId is the same as the aCX, then it is ok to use
    // this one because it has already been validated for this context.
    //
    // XXX: nsnull seems to be a 'special' key value that indicates that NO
    //      validation is required.
    //
    void *key = (void*)aCX;
    if (request->mLoadId != key) {

      // LOAD_BYPASS_CACHE - Always re-fetch
      if (requestFlags & nsIRequest::LOAD_BYPASS_CACHE) {
        entry->Doom(); // doom this thing.
        entry = nsnull;
        NS_RELEASE(request);
        request = nsnull;
      } else {
        // Determine whether the cache entry must be revalidated...
        bValidateRequest = RevalidateEntry(entry, requestFlags, bHasExpired);

        PR_LOG(gImgLog, PR_LOG_DEBUG,
               ("imgLoader::LoadImage validating cache entry. " 
                "bValidateRequest = %d", bValidateRequest));
      }

    }
#if defined(PR_LOGGING)
    else if (!key) {
      PR_LOG(gImgLog, PR_LOG_DEBUG,
             ("imgLoader::LoadImage BYPASSING cache validation for %s " 
              "because of NULL LoadID", spec.get()));
    }
#endif
  }

  //
  // Get the current EventQueue...  This is used as a cacheId to prevent
  // sharing requests which are being loaded across multiple event queues...
  //
  nsCOMPtr<nsIEventQueueService> eventQService;
  nsCOMPtr<nsIEventQueue> activeQ;

  eventQService = do_GetService(NS_EVENTQUEUESERVICE_CONTRACTID, &rv);
  if (NS_FAILED(rv)) {
    NS_IF_RELEASE(request);
    return rv;
  }

  rv = eventQService->ResolveEventQueue(NS_CURRENT_EVENTQ,
                                        getter_AddRefs(activeQ));
  if (NS_FAILED(rv)) {
    NS_IF_RELEASE(request);
    return rv;
  }

  void *cacheId = activeQ.get();
  if (request && !request->IsReusable(cacheId)) {
    //
    // The current request is still being loaded and lives on a different
    // event queue.
    //
    // Since its event queue is NOT active, do not reuse this imgRequest !!
    // Instead, force a new request to be created but DO NOT allow it to be
    // cached!
    //
    PR_LOG(gImgLog, PR_LOG_DEBUG,
           ("[this=%p] imgLoader::LoadImage -- DANGER!! Unable to use cached "
            "imgRequest [request=%p]\n", this, request));

    entry = nsnull;
    NS_RELEASE(request);

    bCanCacheRequest = PR_FALSE;
  }

  //
  // Time to load the request... There are 3 possible cases:
  // =======================================================
  //   1. There is no cached request (ie. nothing was found in the cache).
  //
  //   2. There is a cached request that must be validated.
  //
  //   3. There is a valid cached request.
  //
  if (request && bValidateRequest) {
    /* Case #2: the cache request cache must be revalidated. */
    LOG_SCOPE(gImgLog, "imgLoader::LoadImage |cache hit| must validate");

    // now we need to insert a new channel request object inbetween the real
    // request and the proxy that basically delays loading the image until it
    // gets a 304 or figures out that this needs to be a new request

    if (request->mValidator) {
      rv = CreateNewProxyForRequest(request, aLoadGroup, aObserver, aCX,
                                    requestFlags, aRequest, _retval);

      if (*_retval)
        request->mValidator->AddProxy(NS_STATIC_CAST(imgRequestProxy*, *_retval));

      NS_RELEASE(request);
      return rv;

    } else {
      nsCOMPtr<nsIChannel> newChannel;
      rv = NewImageChannel(getter_AddRefs(newChannel),
                           aURI,
                           aInitialDocumentURI,
                           aReferrerURI,
                           aLoadGroup,
                           requestFlags);
      if (NS_FAILED(rv)) {
        NS_RELEASE(request);
        return NS_ERROR_FAILURE;
      }

      nsCOMPtr<nsICachingChannel> cacheChan(do_QueryInterface(newChannel));

      if (cacheChan) {
        // since this channel supports nsICachingChannel, we can ask it
        // to only stream us data if the data comes off the net.
        PRUint32 loadFlags;
        if (NS_SUCCEEDED(newChannel->GetLoadFlags(&loadFlags)))
            newChannel->SetLoadFlags(loadFlags | nsICachingChannel::LOAD_ONLY_IF_MODIFIED);

        rv = CreateNewProxyForRequest(request, aLoadGroup, aObserver, aCX,
                                      requestFlags, aRequest, _retval);

        httpValidateChecker *hvc = new httpValidateChecker(request, aCX);
        if (!hvc) {
          NS_RELEASE(request);
          return NS_ERROR_OUT_OF_MEMORY;
        }

        NS_ADDREF(hvc);
        request->mValidator = hvc;

        hvc->AddProxy(NS_STATIC_CAST(imgRequestProxy*, *_retval));

        nsresult openRes;
        openRes = newChannel->AsyncOpen(NS_STATIC_CAST(nsIStreamListener *, hvc), nsnull);

        NS_RELEASE(hvc);

        NS_RELEASE(request);

        return openRes;

      }
      // If it isn't caching channel, use the cached version.
      // XXX we should probably do something more intelligent for local files.
      bValidateRequest = PR_FALSE;
    }
  } else if (!request) {
    /* Case #1: no request from the cache.  do a new load */
    LOG_SCOPE(gImgLog, "imgLoader::LoadImage |cache miss|");

    nsCOMPtr<nsIChannel> newChannel;
    rv = NewImageChannel(getter_AddRefs(newChannel),
                         aURI,
                         aInitialDocumentURI,
                         aReferrerURI,
                         aLoadGroup,
                         requestFlags);
    if (NS_FAILED(rv))
      return NS_ERROR_FAILURE;

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

    PR_LOG(gImgLog, PR_LOG_DEBUG,
           ("[this=%p] imgLoader::LoadImage -- Calling channel->AsyncOpen()\n", this));

    nsresult openRes;
    openRes = newChannel->AsyncOpen(NS_STATIC_CAST(nsIStreamListener *, pl), nsnull);

    NS_RELEASE(pl);

    if (NS_FAILED(openRes)) {
      /* If AsyncOpen fails, then we want to hand back a request proxy
         object that has a canceled load.
       */
      LOG_MSG(gImgLog, "imgLoader::LoadImage", "async open failed.");

      rv = CreateNewProxyForRequest(request, aLoadGroup, aObserver,
                                    aCX, requestFlags, aRequest, _retval);
      request->NotifyProxyListener(NS_STATIC_CAST(imgRequestProxy*, *_retval));

      if (NS_SUCCEEDED(rv)) {
        request->OnStartRequest(newChannel, nsnull);
        request->OnStopRequest(newChannel, nsnull, NS_BINDING_ABORTED);
      }

      NS_RELEASE(request);
      
      return openRes;
    }

  } else {
    /* Case #3: request found in cache.  use it */
    // XXX: Should this be executed if an expired cache entry does not have a caching channel??
    LOG_MSG_WITH_PARAM(gImgLog, 
                       "imgLoader::LoadImage |cache hit|", "request", request);

    // Update the request's LoadId
    request->SetLoadId(aCX);
  }

  LOG_MSG(gImgLog, "imgLoader::LoadImage", "creating proxy request.");

  rv = CreateNewProxyForRequest(request, aLoadGroup, aObserver, aCX,
                                requestFlags, aRequest, _retval);

  if (!bValidateRequest) // if we have to validate the request, then we will send the notifications later.
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
  PRBool bHasExpired;

  imgCache::Get(uri, &bHasExpired, &request, getter_AddRefs(entry)); // addrefs request

  nsLoadFlags requestFlags = nsIRequest::LOAD_NORMAL;

  channel->GetLoadFlags(&requestFlags);

  if (request) {
    PRBool bUseCacheCopy = PR_TRUE;

    // LOAD_BYPASS_CACHE - Always re-fetch
    if (requestFlags & nsIRequest::LOAD_BYPASS_CACHE) {
      bUseCacheCopy = PR_FALSE;
    }
    else if (RevalidateEntry(entry, requestFlags, bHasExpired)) {
      nsCOMPtr<nsICachingChannel> cacheChan(do_QueryInterface(channel));

      NS_ASSERTION(cacheChan, "Cache entry without a caching channel!");
      if (cacheChan) {
        cacheChan->IsFromCache(&bUseCacheCopy);
      }
    }

    if (!bUseCacheCopy) {
      entry->Doom(); // doom this thing.
      entry = nsnull;
      NS_RELEASE(request);
    }
  }

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

  // XXX: It looks like the wrong load flags are being passed in...
  requestFlags &= 0xFFFF;

  rv = CreateNewProxyForRequest(request, loadGroup, aObserver,
                                aCX, requestFlags, nsnull, _retval);
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
    PRBool isFromCache;
    if (NS_SUCCEEDED(cacheChan->IsFromCache(&isFromCache)) && isFromCache) {

      PRUint32 count;
      mProxies.Count(&count);
      for (PRInt32 i = count-1; i>=0; i--) {
        imgRequestProxy *proxy;
        mProxies.GetElementAt(i, (nsISupports**)&proxy);
        mRequest->NotifyProxyListener(proxy);
        NS_RELEASE(proxy);
      }

      mRequest->SetLoadId(mContext);
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
#ifdef DEBUG
  nsCOMPtr<nsICachingChannel> cacheChan(do_QueryInterface(aRequest));
  if (cacheChan) {
    PRBool isFromCache;
    if (NS_SUCCEEDED(cacheChan->IsFromCache(&isFromCache)) && isFromCache)
      NS_ERROR("OnDataAvailable not suppressed by LOAD_ONLY_IF_MODIFIED load flag");
  }
#endif

  if (!mDestListener) {
    // XXX see bug 113959
    PRUint32 _retval;
    inStr->ReadSegments(dispose_of_data, nsnull, count, &_retval);
    return NS_OK;
  }

  return mDestListener->OnDataAvailable(aRequest, ctxt, inStr, sourceOffset, count);
}
